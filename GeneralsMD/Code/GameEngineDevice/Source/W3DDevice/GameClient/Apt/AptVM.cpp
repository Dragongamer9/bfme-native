/*
**	bfme-native: APT ActionScript interpreter (AVM1 + EA extensions).
**
**	Operand layouts: tools/aptdis.py decodes all 2396 action blocks of RotWK's 55 movies with this
**	table without a single error. EA additions over AVM1:
**	  - operands of branch/push/string/function opcodes are 4-byte aligned (relative to the file)
**	  - strings are pointers into the .apt; PushData / ConstantPool hold indices into the .const table
**	  - shorthands: PushThis/PushGlobal, PushZero/One/True/False/Null/Undefined, PushByte/Short/Long/Float,
**	    PushConstantByte/Word (pool), PushValueOfVar / GetNamedMember / CallNamed* (name from the pool),
**	    Get/SetString{Var,Member} (inline name), CallFuncPop / CallMethodPop (discard result),
**	    BranchIfFalse, PushRegister.
*/
#include "W3DDevice/GameClient/Apt/AptPlayer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace Apt
{

// ------------------------------------------------------------------------------------------------
// values
// ------------------------------------------------------------------------------------------------
double Value::toNumber() const
{
	switch (type)
	{
	case T_NUMBER: return n;
	case T_BOOLEAN: return b ? 1.0 : 0.0;
	case T_STRING:
	{
		if (s.empty()) return 0.0;
		char *end = nullptr;
		const double d = strtod(s.c_str(), &end);
		while (end && *end == ' ') ++end;
		if (end && *end == 0) return d;
		return NAN;
	}
	case T_OBJECT:
		if (o && o->kind == Object::STRING_OBJ) return Value::string(o->str).toNumber();
		return NAN;
	default: return NAN;	// undefined / null (Flash 7+ semantics; Flash 5 gave 0 -- scripts guard)
	}
}

int Value::toInt() const
{
	const double d = toNumber();
	if (std::isnan(d) || std::isinf(d)) return 0;
	return (int)(long long)d;
}

bool Value::toBool() const
{
	switch (type)
	{
	case T_BOOLEAN: return b;
	case T_NUMBER: return n != 0 && !std::isnan(n);
	case T_STRING: return !s.empty();	// Flash 7 rule; Flash 5 converted to number first
	case T_OBJECT: return o != nullptr;
	default: return false;
	}
}

static std::string numberToString(double d)
{
	if (std::isnan(d)) return "NaN";
	if (std::isinf(d)) return d > 0 ? "Infinity" : "-Infinity";
	if (d == (double)(long long)d && fabs(d) < 1e15)
	{
		char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)d); return buf;
	}
	char buf[64]; snprintf(buf, sizeof(buf), "%.15g", d);
	return buf;
}

std::string Value::toString() const
{
	switch (type)
	{
	case T_UNDEFINED: return "undefined";
	case T_NULL: return "null";
	case T_BOOLEAN: return b ? "true" : "false";
	case T_NUMBER: return numberToString(n);
	case T_STRING: return s;
	case T_OBJECT:
		if (!o) return "null";
		if (o->kind == Object::STRING_OBJ) return o->str;
		if (o->kind == Object::CLIP) { auto c = o->clip.lock(); return c ? c->targetPath() : std::string(); }
		if (o->kind == Object::ARRAY)
		{
			std::string r;
			for (size_t i = 0; i < o->items.size(); ++i) { if (i) r += ","; if (!o->items[i].isUndef()) r += o->items[i].toString(); }
			return r;
		}
		if (o->kind == Object::FUNCTION) return "[type Function]";
		return "[object Object]";
	}
	return std::string();
}

static bool looseEquals(const Value &a, const Value &b)
{
	if (a.type == b.type)
	{
		switch (a.type)
		{
		case Value::T_UNDEFINED: case Value::T_NULL: return true;
		case Value::T_BOOLEAN: return a.b == b.b;
		case Value::T_NUMBER: return a.n == b.n;
		case Value::T_STRING: return a.s == b.s;
		case Value::T_OBJECT: return a.o == b.o;
		}
	}
	const bool an = a.type == Value::T_UNDEFINED || a.type == Value::T_NULL;
	const bool bn = b.type == Value::T_UNDEFINED || b.type == Value::T_NULL;
	if (an || bn) return an && bn;
	if (a.type == Value::T_OBJECT || b.type == Value::T_OBJECT)
	{
		// clips compare by path, string objects by content
		return a.toString() == b.toString();
	}
	return a.toNumber() == b.toNumber();
}

static bool strictEquals(const Value &a, const Value &b)
{
	if (a.type != b.type) return false;
	return looseEquals(a, b);
}

// ------------------------------------------------------------------------------------------------
// objects
// ------------------------------------------------------------------------------------------------
bool Object::getOwn(const std::string &key, const std::string &, Value &out)
{
	if (kind == ARRAY)
	{
		if (key == "length") { out = Value::number((double)items.size()); return true; }
		char *end = nullptr;
		const long idx = strtol(key.c_str(), &end, 10);
		if (!key.empty() && end && *end == 0 && idx >= 0)
		{
			out = (size_t)idx < items.size() ? items[idx] : Value();
			return true;
		}
	}
	if (kind == STRING_OBJ && key == "length") { out = Value::number((double)str.size()); return true; }
	auto it = props.find(key);
	if (it == props.end()) return false;
	out = it->second.value;
	return true;
}

bool Object::setOwn(const std::string &key, const std::string &name, const Value &v)
{
	if (kind == ARRAY)
	{
		if (key == "length") { const int n = v.toInt(); items.resize(n < 0 ? 0 : (size_t)n); return true; }
		char *end = nullptr;
		const long idx = strtol(key.c_str(), &end, 10);
		if (!key.empty() && end && *end == 0 && idx >= 0 && idx < 1000000)
		{
			if ((size_t)idx >= items.size()) items.resize(idx + 1);
			items[idx] = v;
			return true;
		}
	}
	auto it = props.find(key);
	if (it == props.end())
	{
		props[key] = Prop{name, v, false};
		order.push_back(key);
	}
	else
		it->second.value = v;
	return true;
}

Value Object::get(const std::string &name)
{
	const std::string key = lower(name);
	Object *o = this;
	for (int guard = 0; o && guard < 64; ++guard)
	{
		Value v;
		if (o->getOwn(key, name, v)) return v;
		if (key == "__proto__") return Value::object(o->proto);
		o = o->proto.get();
	}
	return Value();
}

void Object::set(const std::string &name, const Value &v, bool dontEnum)
{
	const std::string key = lower(name);
	if (key == "__proto__") { proto = v.isObject() ? v.o : nullptr; return; }
	setOwn(key, name, v);
	if (dontEnum) { auto it = props.find(key); if (it != props.end()) it->second.dontEnum = true; }
}

bool Object::has(const std::string &name)
{
	const std::string key = lower(name);
	for (Object *o = this; o; o = o->proto.get())
	{
		Value v;
		if (o->getOwn(key, name, v)) return true;
	}
	return false;
}

bool Object::remove(const std::string &name)
{
	const std::string key = lower(name);
	auto it = props.find(key);
	if (it == props.end()) return false;
	props.erase(it);
	order.erase(std::remove(order.begin(), order.end(), key), order.end());
	return true;
}

void Object::keys(std::vector<std::string> &out)
{
	if (kind == ARRAY)
		for (size_t i = 0; i < items.size(); ++i) out.push_back(numberToString((double)i));
	for (const std::string &k : order)
	{
		auto it = props.find(k);
		if (it != props.end() && !it->second.dontEnum) out.push_back(it->second.name);
	}
}

// ------------------------------------------------------------------------------------------------
// variable resolution
// ------------------------------------------------------------------------------------------------
static ObjRef toObject(Context &ctx, const Value &v)
{
	if (v.isObject()) return v.o;
	if (v.type == Value::T_STRING)
	{
		ObjRef s = ctx.player.makeObject(ctx.player.stringProto);
		s->kind = Object::STRING_OBJ;
		s->str = v.s;
		return s;
	}
	return nullptr;
}

static Value getMember(Context &ctx, const Value &objv, const std::string &name)
{
	ObjRef o = toObject(ctx, objv);
	if (!o) return Value();
	return o->get(name);
}

Value Context::resolvePath(const std::string &path, bool &found)
{
	found = false;
	// slash syntax "/a/b:var" or "../a"; dot syntax "_root.a.b"
	std::vector<std::string> parts;
	std::string cur;
	for (char ch : path)
	{
		if (ch == '.' || ch == '/' || ch == ':')
		{
			if (!cur.empty()) parts.push_back(cur);
			else if (ch == '/' && parts.empty()) parts.push_back("_root");
			cur.clear();
		}
		else cur += ch;
	}
	if (!cur.empty()) parts.push_back(cur);
	if (parts.empty()) return Value();
	Value v;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (i == 0)
		{
			v = getVariable(parts[0]);
			if (parts.size() == 1) { found = !v.isUndef(); return v; }
		}
		else
		{
			if (parts[i] == "..") v = getMember(*this, v, "_parent");
			else v = getMember(*this, v, parts[i]);
		}
		if (v.isUndef()) return v;
	}
	found = true;
	return v;
}

Value Context::getVariable(const std::string &name)
{
	if (name.find_first_of("./:") != std::string::npos && name != "." )
	{
		bool found;
		return resolvePath(name, found);
	}
	const std::string key = lower(name);
	if (key == "this") return Value::object(self);
	if (key == "_global") return Value::object(player.global());
	if (key == "_root" || key == "_level0") return Value::object(player.clipObject(player.root()));
	if (key == "_parent")
	{
		if (target && target->parent) return Value::object(player.clipObject(target->parent->shared_from_this()));
		return Value();
	}
	// with-scopes and the activation object (innermost first)
	for (size_t i = scope.size(); i-- > 0;)
	{
		if (scope[i] && scope[i]->has(name)) return scope[i]->get(name);
	}
	if (locals && locals->has(name)) return locals->get(name);
	if (target && target->object && target->object->has(name)) return target->object->get(name);
	ObjRef g = player.global();
	if (g && g->has(name)) return g->get(name);
	return Value();
}

void Context::setVariable(const std::string &name, const Value &v)
{
	const size_t sep = name.find_last_of(".:");
	if (sep != std::string::npos)
	{
		bool found;
		Value holder = resolvePath(name.substr(0, sep), found);
		ObjRef o = toObject(*this, holder);
		if (o) o->set(name.substr(sep + 1), v);
		return;
	}
	if (locals && locals->has(name)) { locals->set(name, v); return; }
	for (size_t i = scope.size(); i-- > 0;)
		if (scope[i] && scope[i]->has(name)) { scope[i]->set(name, v); return; }
	if (target && target->object) { target->object->set(name, v); return; }
	if (self) self->set(name, v);
}

void Context::defineLocal(const std::string &name, const Value &v)
{
	if (locals) locals->set(name, v);
	else if (target && target->object) target->object->set(name, v);
}

// ------------------------------------------------------------------------------------------------
// the interpreter
// ------------------------------------------------------------------------------------------------
static Value constantValue(Context &ctx, File *f, unsigned idx)
{
	if (idx >= f->constants.size()) return Value();
	const ConstEntry &e = f->constants[idx];
	switch (e.type)
	{
	case ConstEntry::C_STRING: return Value::string(e.str);
	case ConstEntry::C_REGISTER: return e.u < ctx.registers.size() ? ctx.registers[e.u] : Value();
	case ConstEntry::C_BOOLEAN: return Value::boolean(e.u != 0);
	case ConstEntry::C_FLOAT: return Value::number(e.f);
	case ConstEntry::C_INTEGER: return Value::number((double)(int)e.u);
	case ConstEntry::C_NONE: return Value::null();
	default: return Value::number((double)e.u);
	}
}

static std::vector<Value> popArgs(Context &ctx)
{
	int n = ctx.pop().toInt();
	if (n < 0) n = 0;
	if (n > 256) n = 256;
	std::vector<Value> args(n);
	for (int i = 0; i < n; ++i) args[i] = ctx.pop();
	return args;
}

static std::string typeOf(const Value &v)
{
	switch (v.type)
	{
	case Value::T_UNDEFINED: return "undefined";
	case Value::T_NULL: return "null";
	case Value::T_BOOLEAN: return "boolean";
	case Value::T_NUMBER: return "number";
	case Value::T_STRING: return "string";
	case Value::T_OBJECT:
		if (v.o->kind == Object::FUNCTION) return "function";
		if (v.o->kind == Object::CLIP) return "movieclip";
		return "object";
	}
	return "undefined";
}

static Value add2(const Value &a, const Value &b)
{
	if (a.type == Value::T_STRING || b.type == Value::T_STRING ||
		(a.isObject() && a.o->kind == Object::STRING_OBJ) || (b.isObject() && b.o->kind == Object::STRING_OBJ))
		return Value::string(a.toString() + b.toString());
	return Value::number(a.toNumber() + b.toNumber());
}

static Value less2(const Value &a, const Value &b)
{
	if (a.type == Value::T_STRING && b.type == Value::T_STRING) return Value::boolean(a.s < b.s);
	const double x = a.toNumber(), y = b.toNumber();
	if (std::isnan(x) || std::isnan(y)) return Value();
	return Value::boolean(x < y);
}

static ObjRef makeFunction(Context &ctx, File *f, unsigned bodyOffset, unsigned size)
{
	ObjRef fn = ctx.player.makeObject(ctx.player.functionProto);
	fn->kind = Object::FUNCTION;
	fn->body.file = f;
	fn->body.offset = bodyOffset;
	fn->bodySize = size;
	fn->pool = ctx.pool;
	fn->scope = ctx.scope;
	if (ctx.locals) fn->scope.push_back(ctx.locals);
	fn->target = ctx.target;
	ObjRef proto = ctx.player.makeObject(ctx.player.objectProto);
	proto->set("constructor", Value::object(fn), true);
	fn->set("prototype", Value::object(proto), true);
	return fn;
}

static Value callMethod(Context &ctx, const Value &objv, const std::string &name, std::vector<Value> &args)
{
	if (name.empty() || objv.isUndef() && false)
		return Value();
	ObjRef o = toObject(ctx, objv);
	if (!o)
	{
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[apt] call %s on %s\n", name.c_str(), objv.toString().c_str());
		return Value();
	}
	Value fn = o->get(name);
	if (!fn.isObject() || fn.o->kind != Object::FUNCTION)
	{
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[apt] no method %s on %s\n", name.c_str(), objv.toString().c_str());
		return Value();
	}
	if (getenv("BFME_DEBUG_APT_CALLS")) fprintf(stderr, "[apt] -> %s.%s\n", objv.toString().c_str(), name.c_str());
	return ctx.player.callFunction(fn.o, o, args, ctx.depth + 1);
}

static Value callNamedFunction(Context &ctx, const std::string &name, std::vector<Value> &args)
{
	Value fn = ctx.getVariable(name);
	if (!fn.isObject() || fn.o->kind != Object::FUNCTION)
	{
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[apt] no function %s\n", name.c_str());
		return Value();
	}
	if (getenv("BFME_DEBUG_APT_CALLS")) fprintf(stderr, "[apt] -> %s()\n", name.c_str());
	return ctx.player.callFunction(fn.o, ctx.self, args, ctx.depth + 1);
}

static Value construct(Context &ctx, const Value &ctor, std::vector<Value> &args)
{
	if (!ctor.isObject() || ctor.o->kind != Object::FUNCTION) return Value();
	Value protoV = ctor.o->get("prototype");
	ObjRef obj;
	if (ctor.o->native && ctor.o->has("__isNativeCtor"))
	{
		// native constructors build their own object (Array, String, Color, Object ...)
		Value r = ctor.o->native(ctx, nullptr, args);
		if (r.isObject()) return r;
		obj = ctx.player.makeObject(protoV.isObject() ? protoV.o : ctx.player.objectProto);
		return Value::object(obj);
	}
	obj = ctx.player.makeObject(protoV.isObject() ? protoV.o : ctx.player.objectProto);
	obj->set("__constructor__", ctor, true);
	Value r = ctx.player.callFunction(ctor.o, obj, args, ctx.depth + 1);
	if (r.isObject()) return r;
	return Value::object(obj);
}

static void getUrl(Context &ctx, const std::string &url, const std::string &target)
{
	static const char prefix[] = "FSCommand:";
	if (strncmp(url.c_str(), prefix, sizeof(prefix) - 1) == 0 || strncmp(url.c_str(), "fscommand:", 10) == 0)
	{
		const std::string cmd = url.substr(10);
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[apt] fscommand %s (%s)\n", cmd.c_str(), target.c_str());
		if (ctx.player.host()) ctx.player.host()->onFSCommand(ctx.player, cmd, target);
		return;
	}
	// loadMovie(url, target): an .apt (or empty url = unload)
	std::shared_ptr<Instance> t;
	bool found = false;
	Value tv = ctx.resolvePath(target, found);
	t = ctx.player.instanceOf(tv);
	if (!t && target.compare(0, 6, "_level") == 0) t = ctx.player.root();
	if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[apt] loadMovie '%s' -> '%s'%s\n", url.c_str(), target.c_str(), t ? "" : " (no target)");
	if (t && !url.empty())
		ctx.player.loadMovieInto(*t, url);
}

/// property index of Get/SetProperty
static const char *s_propNames[] = {
	"_x", "_y", "_xscale", "_yscale", "_currentframe", "_totalframes", "_alpha", "_visible", "_width", "_height",
	"_rotation", "_target", "_framesloaded", "_name", "_droptarget", "_url", "_highquality", "_focusrect",
	"_soundbuftime", "_quality", "_xmouse", "_ymouse"
};

void runCode(Context &ctx, const Code &code, unsigned endOffset)
{
	File *f = code.file;
	if (f == nullptr || code.offset == 0) return;
	ctx.file = f;
	if (!ctx.origTarget) { ctx.origTarget = ctx.target; ctx.origSelf = ctx.self; }
	const std::vector<unsigned char> &d = f->data;
	unsigned pc = code.offset;
	unsigned furthest = pc;
	int budget = 200000;	// runaway guard
	const bool trace = getenv("BFME_DEBUG_APT_TRACE") != nullptr;
	// 'with' blocks: (end offset, scope depth)
	std::vector<std::pair<unsigned, size_t>> withs;

	while (pc < d.size() && --budget > 0 && !ctx.returned)
	{
		if (endOffset && pc >= endOffset) break;
		while (!withs.empty() && pc >= withs.back().first) { ctx.scope.resize(withs.back().second); withs.pop_back(); }
		const unsigned at = pc;
		const unsigned char op = d[pc++];
		auto align = [&]() { pc = (pc + 3) & ~3u; };
		auto rd32 = [&]() { unsigned v = f->u32(pc); pc += 4; return v; };
		auto rd16 = [&]() { unsigned short v = 0; if (pc + 2 <= d.size()) memcpy(&v, &d[pc], 2); pc += 2; return (unsigned)v; };
		auto rd8 = [&]() { return pc < d.size() ? (unsigned)d[pc++] : 0u; };
		auto poolStr = [&](unsigned i) { return i < ctx.pool.size() ? ctx.pool[i].toString() : std::string(); };
		if (trace) fprintf(stderr, "[apt-trace] %06x op %02x stack %zu\n", at, op, ctx.stack.size());

		switch (op)
		{
		case 0x00:	// End
			if (endOffset == 0 && pc > furthest) return;
			break;
		case 0x04: if (ctx.target) ctx.player.gotoFrame(*ctx.target, ctx.target->currentFrame + 1, false); break;
		case 0x05: if (ctx.target) ctx.player.gotoFrame(*ctx.target, ctx.target->currentFrame - 1, false); break;
		case 0x06: if (ctx.target) ctx.target->playing = true; break;
		case 0x07: if (ctx.target) ctx.target->playing = false; break;
		case 0x08: case 0x09: break;
		case 0x0A: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::number(a.toNumber() + b.toNumber())); break; }
		case 0x0B: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::number(a.toNumber() - b.toNumber())); break; }
		case 0x0C: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::number(a.toNumber() * b.toNumber())); break; }
		case 0x0D: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::number(a.toNumber() / b.toNumber())); break; }
		case 0x0E: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(a.toNumber() == b.toNumber())); break; }
		case 0x0F: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(a.toNumber() < b.toNumber())); break; }
		case 0x10: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(a.toBool() && b.toBool())); break; }
		case 0x11: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(a.toBool() || b.toBool())); break; }
		case 0x12: ctx.push(Value::boolean(!ctx.pop().toBool())); break;
		case 0x13: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(a.toString() == b.toString())); break; }
		case 0x14: case 0x31: ctx.push(Value::number((double)ctx.pop().toString().size())); break;
		case 0x15: case 0x35:
		{
			int count = ctx.pop().toInt(); int index = ctx.pop().toInt() - 1; std::string s = ctx.pop().toString();
			if (index < 0) index = 0;
			if ((size_t)index > s.size()) index = (int)s.size();
			if (count < 0) count = (int)s.size();
			ctx.push(Value::string(s.substr(index, count)));
			break;
		}
		case 0x17: ctx.pop(); break;
		case 0x18: ctx.push(Value::number((double)ctx.pop().toInt())); break;
		case 0x1C: { std::string n = ctx.pop().toString(); ctx.push(ctx.getVariable(n)); break; }
		case 0x1D: { Value v = ctx.pop(); std::string n = ctx.pop().toString(); ctx.setVariable(n, v); break; }
		case 0x20:	// SetTarget2
		{
			Value t = ctx.pop();
			std::shared_ptr<Instance> inst = ctx.player.instanceOf(t);
			if (!inst && t.type == Value::T_STRING)
			{
				bool found; inst = ctx.player.instanceOf(ctx.resolvePath(t.s, found));
			}
			if (inst) { ctx.target = inst; ctx.self = ctx.player.clipObject(inst); }
			break;
		}
		case 0x21: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::string(a.toString() + b.toString())); break; }
		case 0x22:	// GetProperty
		{
			const int idx = ctx.pop().toInt(); Value t = ctx.pop();
			std::shared_ptr<Instance> inst = t.type == Value::T_STRING && t.s.empty() ? ctx.target : ctx.player.instanceOf(t);
			if (!inst && t.type == Value::T_STRING) { bool found; inst = ctx.player.instanceOf(ctx.resolvePath(t.s, found)); }
			if (inst && idx >= 0 && idx < 22) ctx.push(ctx.player.clipObject(inst)->get(s_propNames[idx]));
			else ctx.push(Value());
			break;
		}
		case 0x23:	// SetProperty
		{
			Value v = ctx.pop(); const int idx = ctx.pop().toInt(); Value t = ctx.pop();
			std::shared_ptr<Instance> inst = t.type == Value::T_STRING && t.s.empty() ? ctx.target : ctx.player.instanceOf(t);
			if (!inst && t.type == Value::T_STRING) { bool found; inst = ctx.player.instanceOf(ctx.resolvePath(t.s, found)); }
			if (inst && idx >= 0 && idx < 22) ctx.player.clipObject(inst)->set(s_propNames[idx], v);
			break;
		}
		case 0x24:	// CloneSprite
		{
			const int depth = ctx.pop().toInt(); std::string newName = ctx.pop().toString(); Value src = ctx.pop();
			std::shared_ptr<Instance> s = ctx.player.instanceOf(src);
			if (s && s->parent)
			{
				std::shared_ptr<Instance> c = ctx.player.createInstance(s->character, s->file, s->parent, depth);
				c->name = newName; c->matrix = s->matrix; c->color = s->color; c->placedByTimeline = false;
				s->parent->children[depth] = c;
				ctx.player.gotoFrame(*c, 0, true);
			}
			break;
		}
		case 0x25:	// RemoveSprite
		{
			std::shared_ptr<Instance> s = ctx.player.instanceOf(ctx.pop());
			if (s && s->parent) s->parent->children.erase(s->depth);
			break;
		}
		case 0x26:	// Trace
		{
			const std::string s = ctx.pop().toString();
			if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[apt] trace: %s\n", s.c_str());
			break;
		}
		case 0x27: ctx.pop(); ctx.pop(); if (ctx.pop().toBool()) { ctx.pop(); ctx.pop(); ctx.pop(); ctx.pop(); } break;	// StartDrag (unsupported)
		case 0x28: break;
		case 0x29: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(a.toString() < b.toString())); break; }
		case 0x2A: ctx.pop(); ctx.returned = true; break;	// Throw: abort the block
		case 0x2B: { Value o = ctx.pop(); ctx.pop(); ctx.push(o); break; }	// CastOp (approximate)
		case 0x2C: { const int n = ctx.pop().toInt(); for (int i = 0; i < n; ++i) ctx.pop(); ctx.pop(); break; }
		case 0x30: { const int m = ctx.pop().toInt(); ctx.push(Value::number(m > 0 ? rand() % m : 0)); break; }
		case 0x32: case 0x36: { std::string s = ctx.pop().toString(); ctx.push(Value::number(s.empty() ? 0 : (unsigned char)s[0])); break; }
		case 0x33: case 0x37: { const int c = ctx.pop().toInt(); ctx.push(Value::string(std::string(1, (char)c))); break; }
		case 0x34: ctx.push(Value::number((double)ctx.player.timeMs)); break;
		case 0x3A:	// Delete
		{
			std::string n = ctx.pop().toString(); ObjRef o = toObject(ctx, ctx.pop());
			ctx.push(Value::boolean(o ? o->remove(n) : false));
			break;
		}
		case 0x3B:	// Delete2
		{
			std::string n = ctx.pop().toString();
			bool ok = false;
			if (ctx.locals) ok = ctx.locals->remove(n);
			if (!ok && ctx.target && ctx.target->object) ok = ctx.target->object->remove(n);
			ctx.push(Value::boolean(ok));
			break;
		}
		case 0x3C: { Value v = ctx.pop(); std::string n = ctx.pop().toString(); ctx.defineLocal(n, v); break; }
		case 0x41: { std::string n = ctx.pop().toString(); if (ctx.locals && !ctx.locals->has(n)) ctx.locals->set(n, Value()); break; }
		case 0x3D: case 0x5C: case 0x5B:	// CallFunction (0x5B: discard result)
		{
			std::string n = ctx.pop().toString();
			std::vector<Value> args = popArgs(ctx);
			Value r = callNamedFunction(ctx, n, args);
			if (op != 0x5B) ctx.push(r);
			break;
		}
		case 0x3E:	// Return
			ctx.returnValue = ctx.pop();
			ctx.returned = true;
			break;
		case 0x3F: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::number(fmod(a.toNumber(), b.toNumber()))); break; }
		case 0x40:	// NewObject
		{
			std::string n = ctx.pop().toString();
			std::vector<Value> args = popArgs(ctx);
			ctx.push(construct(ctx, ctx.getVariable(n), args));
			break;
		}
		case 0x42:	// InitArray
		{
			std::vector<Value> items = popArgs(ctx);
			ctx.push(Value::object(ctx.player.makeArray(items)));
			break;
		}
		case 0x43:	// InitObject
		{
			int n = ctx.pop().toInt();
			ObjRef o = ctx.player.makeObject(ctx.player.objectProto);
			for (int i = 0; i < n && i < 1024; ++i) { Value v = ctx.pop(); std::string k = ctx.pop().toString(); o->set(k, v); }
			ctx.push(Value::object(o));
			break;
		}
		case 0x44: ctx.push(Value::string(typeOf(ctx.pop()))); break;
		case 0x45:	// TargetPath
		{
			std::shared_ptr<Instance> inst = ctx.player.instanceOf(ctx.pop());
			ctx.push(inst ? Value::string(inst->targetPath()) : Value());
			break;
		}
		case 0x46: case 0x55:	// Enumerate / Enumerate2
		{
			Value v = ctx.pop();
			ObjRef o = op == 0x55 ? toObject(ctx, v) : toObject(ctx, ctx.getVariable(v.toString()));
			ctx.push(Value::null());
			if (o)
			{
				std::vector<std::string> ks;
				o->keys(ks);
				for (size_t i = ks.size(); i-- > 0;) ctx.push(Value::string(ks[i]));
			}
			break;
		}
		case 0x47: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(add2(a, b)); break; }
		case 0x48: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(less2(a, b)); break; }
		case 0x67: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(less2(b, a)); break; }
		case 0x49: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(looseEquals(a, b))); break; }
		case 0x66: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(strictEquals(a, b))); break; }
		case 0x68: { Value b = ctx.pop(), a = ctx.pop(); ctx.push(Value::boolean(a.toString() > b.toString())); break; }
		case 0x4A: ctx.push(Value::number(ctx.pop().toNumber())); break;
		case 0x4B: ctx.push(Value::string(ctx.pop().toString())); break;
		case 0x4C: { Value v = ctx.stack.empty() ? Value() : ctx.stack.back(); ctx.push(v); break; }
		case 0x4D: { Value a = ctx.pop(), b = ctx.pop(); ctx.push(a); ctx.push(b); break; }
		case 0x4E:	// GetMember
		{
			std::string n = ctx.pop().toString(); Value o = ctx.pop();
			ctx.push(getMember(ctx, o, n));
			break;
		}
		case 0x4F:	// SetMember
		{
			Value v = ctx.pop(); std::string n = ctx.pop().toString(); ObjRef o = toObject(ctx, ctx.pop());
			if (o) o->set(n, v);
			break;
		}
		case 0x50: ctx.push(Value::number(ctx.pop().toNumber() + 1)); break;
		case 0x51: ctx.push(Value::number(ctx.pop().toNumber() - 1)); break;
		case 0x52: case 0x5E: case 0x5D:	// CallMethod (0x5D: discard result)
		{
			Value nameV = ctx.pop(); Value objV = ctx.pop();
			std::vector<Value> args = popArgs(ctx);
			Value r;
			if (nameV.isUndef() || (nameV.type == Value::T_STRING && nameV.s.empty()))
			{
				if (objV.isObject() && objV.o->kind == Object::FUNCTION) r = ctx.player.callFunction(objV.o, ctx.self, args, ctx.depth + 1);
			}
			else
				r = callMethod(ctx, objV, nameV.toString(), args);
			if (op != 0x5D) ctx.push(r);
			break;
		}
		case 0x53:	// NewMethod
		{
			Value nameV = ctx.pop(); Value objV = ctx.pop();
			std::vector<Value> args = popArgs(ctx);
			Value ctor = (nameV.isUndef() || nameV.toString().empty()) ? objV : getMember(ctx, objV, nameV.toString());
			ctx.push(construct(ctx, ctor, args));
			break;
		}
		case 0x54:	// InstanceOf
		{
			Value ctor = ctx.pop(); Value v = ctx.pop();
			bool r = false;
			if (v.isObject() && ctor.isObject())
			{
				Value p = ctor.o->get("prototype");
				for (Object *o = v.o->proto.get(); o && p.isObject(); o = o->proto.get()) if (o == p.o.get()) { r = true; break; }
			}
			ctx.push(Value::boolean(r));
			break;
		}
		case 0x56: case 0x70: ctx.push(Value::object(ctx.self)); break;
		case 0x58: case 0x71: ctx.push(Value::object(ctx.player.global())); break;
		case 0x59: ctx.push(Value::number(0)); break;
		case 0x5A: ctx.push(Value::number(1)); break;
		case 0x60: { const int b = ctx.pop().toInt(), a = ctx.pop().toInt(); ctx.push(Value::number(a & b)); break; }
		case 0x61: { const int b = ctx.pop().toInt(), a = ctx.pop().toInt(); ctx.push(Value::number(a | b)); break; }
		case 0x62: { const int b = ctx.pop().toInt(), a = ctx.pop().toInt(); ctx.push(Value::number(a ^ b)); break; }
		case 0x63: { const int b = ctx.pop().toInt(), a = ctx.pop().toInt(); ctx.push(Value::number(a << (b & 31))); break; }
		case 0x64: { const int b = ctx.pop().toInt(), a = ctx.pop().toInt(); ctx.push(Value::number(a >> (b & 31))); break; }
		case 0x65: { const int b = ctx.pop().toInt(); const unsigned a = (unsigned)ctx.pop().toInt(); ctx.push(Value::number((double)(a >> (b & 31)))); break; }
		case 0x69:	// Extends
		{
			Value super_ = ctx.pop(); Value sub = ctx.pop();
			if (sub.isObject() && super_.isObject())
			{
				ObjRef proto = ctx.player.makeObject(nullptr);
				Value sp = super_.o->get("prototype");
				proto->proto = sp.isObject() ? sp.o : nullptr;
				proto->set("__constructor__", super_, true);
				sub.o->set("prototype", Value::object(proto), true);
			}
			break;
		}
		case 0x72: { std::string n = ctx.pop().toString(); ctx.setVariable(n, Value::number(0)); break; }	// EA_ZeroVar
		case 0x73: ctx.push(Value::boolean(true)); break;
		case 0x74: ctx.push(Value::boolean(false)); break;
		case 0x75: ctx.push(Value::null()); break;
		case 0x76: ctx.push(Value()); break;
		case 0x77: break;
		// --- operand opcodes ---
		case 0x81:	// GotoFrame
		{
			align(); const int frame = (int)rd32();
			if (ctx.target) ctx.player.gotoFrame(*ctx.target, frame, false);
			break;
		}
		case 0x83:	// GetURL
		{
			align(); std::string url = f->cstr(rd32()); std::string tgt = f->cstr(rd32());
			getUrl(ctx, url, tgt);
			break;
		}
		case 0x87:	// SetRegister
		{
			align(); const unsigned r = rd32();
			if (r < 256) { if (r >= ctx.registers.size()) ctx.registers.resize(r + 1); ctx.registers[r] = ctx.stack.empty() ? Value() : ctx.stack.back(); }
			break;
		}
		case 0x88:	// ConstantPool
		{
			align(); const unsigned n = rd32(), lo = rd32();
			ctx.pool.clear();
			for (unsigned i = 0; i < n; ++i) ctx.pool.push_back(constantValue(ctx, f, f->u32(lo + 4 * i)));
			break;
		}
		case 0x8A: align(); rd32(); rd32(); break;	// WaitForFrame (unused)
		case 0x8D: rd8(); ctx.pop(); break;			// WaitForFrame2 (unused)
		case 0x8B:	// SetTarget
		{
			align(); std::string t = f->cstr(rd32());
			if (t.empty())
			{
				if (ctx.origTarget) { ctx.target = ctx.origTarget; ctx.self = ctx.origSelf; }
			}
			else
			{
				bool found; std::shared_ptr<Instance> inst = ctx.player.instanceOf(ctx.resolvePath(t, found));
				if (inst) { ctx.target = inst; ctx.self = ctx.player.clipObject(inst); }
			}
			break;
		}
		case 0x8C:	// GotoLabel
		{
			align(); std::string label = f->cstr(rd32());
			if (ctx.target) ctx.player.gotoLabel(*ctx.target, label, false);
			break;
		}
		case 0x8E:	// DefineFunction2
		{
			align();
			std::string name = f->cstr(f->u32(pc));
			const unsigned np = f->u32(pc + 4);
			const unsigned char nregs = pc + 8 < d.size() ? d[pc + 8] : 0;
			const unsigned flags = (pc + 11 < d.size()) ? (d[pc + 9] | (d[pc + 10] << 8) | (d[pc + 11] << 16)) : 0;	// OpenSAGE FunctionPreloadFlags layout
			const unsigned plist = f->u32(pc + 12);
			const unsigned size = f->u32(pc + 16);
			pc += 28;
			ObjRef fn = makeFunction(ctx, f, pc, size);
			fn->isFunction2 = true;
			fn->numRegisters = nregs;
			fn->flags2 = flags;
			for (unsigned i = 0; i < np && i < 64; ++i)
				fn->params.push_back(std::make_pair(f->i32(plist + 8 * i), f->cstr(f->u32(plist + 8 * i + 4))));
			pc += size;
			if (name.empty()) ctx.push(Value::object(fn));
			else ctx.setVariable(name, Value::object(fn));
			break;
		}
		case 0x9B:	// DefineFunction
		{
			align();
			std::string name = f->cstr(f->u32(pc));
			const unsigned np = f->u32(pc + 4), plist = f->u32(pc + 8);
			const unsigned size = f->u32(pc + 12);
			pc += 24;
			ObjRef fn = makeFunction(ctx, f, pc, size);
			for (unsigned i = 0; i < np && i < 64; ++i) fn->params.push_back(std::make_pair(0, f->cstr(f->u32(plist + 4 * i))));
			pc += size;
			if (name.empty()) ctx.push(Value::object(fn));
			else ctx.setVariable(name, Value::object(fn));
			break;
		}
		case 0x8F: align(); rd32(); break;	// Try (unused in RotWK)
		case 0x94:	// With
		{
			align(); const unsigned size = rd32();
			ObjRef o = toObject(ctx, ctx.pop());
			withs.push_back(std::make_pair(pc + size, ctx.scope.size()));
			ctx.scope.push_back(o);
			break;
		}
		case 0x96:	// PushData
		{
			align(); const unsigned n = rd32(), lo = rd32();
			for (unsigned i = 0; i < n; ++i) ctx.push(constantValue(ctx, f, f->u32(lo + 4 * i)));
			break;
		}
		case 0x99:	// BranchAlways
		{
			align(); const int off = (int)rd32();
			pc = (unsigned)((int)pc + off);
			if (pc > furthest) furthest = pc;
			break;
		}
		case 0x9A:	// GetURL2
		{
			std::string tgt = ctx.pop().toString(); std::string url = ctx.pop().toString();
			getUrl(ctx, url, tgt);
			break;
		}
		case 0x9D: case 0xB8:	// BranchIfTrue / EA_BranchIfFalse
		{
			align(); const int off = (int)rd32();
			const bool c = ctx.pop().toBool();
			if (pc + off > furthest) furthest = pc + off;
			if (c == (op == 0x9D)) pc = (unsigned)((int)pc + off);
			break;
		}
		case 0x9E:	// CallFrame
		{
			Value fr = ctx.pop();
			if (ctx.target && ctx.target->isSprite())
			{
				int frame = -1;
				if (fr.type == Value::T_STRING) { auto it = ctx.target->character->labels.find(lower(fr.s)); if (it != ctx.target->character->labels.end()) frame = it->second; }
				else frame = fr.toInt() - 1;
				if (frame >= 0 && frame < ctx.target->totalFrames())
					for (const FrameItem &it : ctx.target->character->frames[frame].items)
						if (it.kind == FrameItem::ACTION) ctx.player.execute(it.code, ctx.target);
			}
			break;
		}
		case 0x9F:	// GotoFrame2
		{
			align(); const unsigned flags = rd32();
			Value fr = ctx.pop();
			if (ctx.target)
			{
				const bool play = (flags & 1) != 0;
				if (fr.type == Value::T_STRING && !fr.s.empty() && !(fr.s[0] >= '0' && fr.s[0] <= '9'))
					ctx.player.gotoLabel(*ctx.target, fr.s, play);
				else
					ctx.player.gotoFrame(*ctx.target, fr.toInt() - 1, play);
			}
			break;
		}
		case 0xA1: align(); ctx.push(Value::string(f->cstr(rd32()))); break;
		case 0xA2: { const unsigned i = rd8(); ctx.push(i < ctx.pool.size() ? ctx.pool[i] : Value()); break; }
		case 0xA3: { const unsigned i = rd16(); ctx.push(i < ctx.pool.size() ? ctx.pool[i] : Value()); break; }
		case 0xA4: { align(); std::string n = f->cstr(rd32()); ctx.push(ctx.getVariable(n)); break; }
		case 0xA5: { align(); std::string n = f->cstr(rd32()); Value o = ctx.pop(); ctx.push(getMember(ctx, o, n)); break; }
		// EA_SetStringVar / EA_SetStringMember: the inline string is the VALUE; names come off the stack
		case 0xA6: { align(); Value v = Value::string(f->cstr(rd32())); std::string n = ctx.pop().toString(); ctx.setVariable(n, v); break; }
		case 0xA7: { align(); Value v = Value::string(f->cstr(rd32())); std::string n = ctx.pop().toString(); ObjRef o = toObject(ctx, ctx.pop()); if (o) o->set(n, v); break; }
		case 0xAE: { const unsigned i = rd8(); ctx.push(ctx.getVariable(poolStr(i))); break; }
		case 0xAF: { const unsigned i = rd8(); Value o = ctx.pop(); ctx.push(getMember(ctx, o, poolStr(i))); break; }
		case 0xB0: case 0xB1:	// CallNamedFuncPop / CallNamedFunc
		{
			const unsigned i = rd8();
			std::vector<Value> args = popArgs(ctx);
			Value r = callNamedFunction(ctx, poolStr(i), args);
			if (op == 0xB1) ctx.push(r);
			break;
		}
		case 0xB2: case 0xB3:	// CallNamedMethodPop / CallNamedMethod
		{
			const unsigned i = rd8();
			Value objV = ctx.pop();
			std::vector<Value> args = popArgs(ctx);
			Value r = callMethod(ctx, objV, poolStr(i), args);
			if (op == 0xB3) ctx.push(r);
			break;
		}
		case 0xB4: { float v = f->f32(pc); pc += 4; ctx.push(Value::number(v)); break; }
		case 0xB5: ctx.push(Value::number((double)(signed char)rd8())); break;
		case 0xB6: ctx.push(Value::number((double)(short)rd16())); break;
		case 0xB7: ctx.push(Value::number((double)(int)rd32())); break;
		case 0xB9: { const unsigned r = rd8(); ctx.push(r < ctx.registers.size() ? ctx.registers[r] : Value()); break; }
		default:
			fprintf(stderr, "[apt] unknown opcode %02x at %06x in %s\n", op, at, f->name.c_str());
			return;
		}
	}
	if (budget <= 0) fprintf(stderr, "[apt] script budget exhausted in %s at %06x\n", f->name.c_str(), pc);
}

} // namespace Apt
