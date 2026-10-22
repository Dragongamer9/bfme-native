/*
**	bfme-native: APT script builtins -- the parts of the Flash 6 runtime the RotWK movies use
**	(MovieClip, Array, String, Math, Color, Mouse, setInterval, ...), plus 'extern' for game values.
*/
#include "W3DDevice/GameClient/Apt/AptPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Apt
{

typedef std::vector<Value> Args;

static Value arg(const Args &a, size_t i) { return i < a.size() ? a[i] : Value(); }

static std::shared_ptr<Instance> selfClip(const ObjRef &self)
{
	return (self && self->kind == Object::CLIP) ? self->clip.lock() : nullptr;
}

static void method(Player &p, const ObjRef &proto, const char *name, const NativeFn &fn)
{
	proto->set(name, Value::object(p.makeNative(fn)), true);
}

static Character *emptySprite()
{
	static Character s_empty;
	static bool init = false;
	if (!init)
	{
		init = true;
		s_empty.type = CHAR_SPRITE;
		s_empty.frames.resize(1);
	}
	return &s_empty;
}

static Character *findExport(Instance &inst, const std::string &id, Player &p)
{
	for (File *f : { inst.file, p.file() })
	{
		if (!f) continue;
		Character *c = f->exported(id);
		if (c) return c;
		// imported under that name?
		for (const Import &im : f->imports)
			if (im.name == id && im.character < f->characters.size() && f->characters[im.character])
				return f->characters[im.character];
	}
	return nullptr;
}

/// create a scripted child (attachMovie / createEmptyMovieClip / duplicateMovieClip)
static std::shared_ptr<Instance> addChild(Player &p, Instance &parent, Character *c, File *file, const std::string &name, int depth,
	const ObjRef &init, const Instance *copyFrom)
{
	auto old = parent.children.find(depth);
	if (old != parent.children.end())
	{
		// Flash replaces whatever lives at that depth
		std::vector<Value> none;
		parent.children.erase(old);
	}
	std::shared_ptr<Instance> child = p.createInstance(c, file, &parent, depth);
	child->name = name;
	child->placedByTimeline = false;
	if (copyFrom)
	{
		child->matrix = copyFrom->matrix;
		child->color = copyFrom->color;
		child->clipEvents = copyFrom->clipEvents;
		child->visible = copyFrom->visible;
	}
	parent.children[depth] = child;
	ObjRef o = p.clipObject(child);
	if (init)
		for (const std::string &k : init->order)
		{
			auto it = init->props.find(k);
			if (it != init->props.end() && !it->second.dontEnum) o->set(it->second.name, it->second.value);
		}
	// same as a timeline placement: load events, then frame 1
	if (child->isSprite())
	{
		p.fireClipEvent(*child, CLIPEVENT_INITIALIZE);
		p.fireClipEvent(*child, CLIPEVENT_LOAD);
		child->currentFrame = -1;
		const size_t mark = p.pendingCount();
		p.gotoFrame(*child, 0, true);
		p.runPendingFrom(mark);	// the new clip's first-frame scripts run before attachMovie returns
	}
	return child;
}

static int highestDepth(Instance &inst)
{
	int d = -1;
	for (auto &kv : inst.children) d = std::max(d, kv.first);
	return d;
}

static std::string numToStr(double d) { return Value::number(d).toString(); }

void Player::installBuiltins()
{
	Player &P = *this;
	objectProto = makeObject(nullptr);
	functionProto = makeObject(objectProto);
	arrayProto = makeObject(objectProto);
	stringProto = makeObject(objectProto);
	clipProto = makeObject(objectProto);
	colorProto = makeObject(objectProto);
	textProto = makeObject(objectProto);
	m_global = makeObject(objectProto);

	// ---------------- Object ----------------
	method(P, objectProto, "toString", [](Context &, const ObjRef &self, Args &) { return Value::string(Value::object(self).toString()); });
	method(P, objectProto, "valueOf", [](Context &, const ObjRef &self, Args &) { return Value::object(self); });
	method(P, objectProto, "hasOwnProperty", [](Context &, const ObjRef &self, Args &a) {
		Value v; return Value::boolean(self && self->getOwn(lower(arg(a, 0).toString()), arg(a, 0).toString(), v)); });
	method(P, objectProto, "addProperty", [](Context &, const ObjRef &, Args &) { return Value::boolean(false); });
	method(P, objectProto, "watch", [](Context &, const ObjRef &, Args &) { return Value::boolean(false); });
	method(P, objectProto, "unwatch", [](Context &, const ObjRef &, Args &) { return Value::boolean(false); });

	// ---------------- Function ----------------
	method(P, functionProto, "call", [](Context &ctx, const ObjRef &self, Args &a) {
		ObjRef thisObj = arg(a, 0).isObject() ? arg(a, 0).o : nullptr;
		Args rest(a.size() > 1 ? a.begin() + 1 : a.end(), a.end());
		return ctx.player.callFunction(self, thisObj, rest, ctx.depth + 1);
	});
	method(P, functionProto, "apply", [](Context &ctx, const ObjRef &self, Args &a) {
		ObjRef thisObj = arg(a, 0).isObject() ? arg(a, 0).o : nullptr;
		Args rest;
		if (arg(a, 1).isObject() && arg(a, 1).o->kind == Object::ARRAY) rest = arg(a, 1).o->items;
		return ctx.player.callFunction(self, thisObj, rest, ctx.depth + 1);
	});

	auto ctor = [&](const char *name, const ObjRef &proto, const NativeFn &fn) {
		ObjRef c = makeNative(fn);
		c->set("__isNativeCtor", Value::boolean(true), true);
		c->set("prototype", Value::object(proto), true);
		proto->set("constructor", Value::object(c), true);
		m_global->set(name, Value::object(c), true);
		return c;
	};

	ObjRef objectCtor = ctor("Object", objectProto, [](Context &ctx, const ObjRef &, Args &a) {
		if (arg(a, 0).isObject()) return arg(a, 0);
		return Value::object(ctx.player.makeObject(ctx.player.objectProto));
	});
	objectCtor->set("registerClass", Value::object(makeNative([](Context &, const ObjRef &, Args &) { return Value::boolean(false); })), true);
	ctor("Function", functionProto, [](Context &, const ObjRef &, Args &) { return Value(); });

	// ---------------- Array ----------------
	ctor("Array", arrayProto, [](Context &ctx, const ObjRef &, Args &a) {
		if (a.size() == 1 && a[0].type == Value::T_NUMBER)
			return Value::object(ctx.player.makeArray(std::vector<Value>((size_t)std::max(0, a[0].toInt()))));
		return Value::object(ctx.player.makeArray(a));
	});
	method(P, arrayProto, "push", [](Context &, const ObjRef &self, Args &a) {
		for (const Value &v : a) self->items.push_back(v);
		return Value::number((double)self->items.size()); });
	method(P, arrayProto, "pop", [](Context &, const ObjRef &self, Args &) {
		if (self->items.empty()) return Value();
		Value v = self->items.back(); self->items.pop_back(); return v; });
	method(P, arrayProto, "shift", [](Context &, const ObjRef &self, Args &) {
		if (self->items.empty()) return Value();
		Value v = self->items.front(); self->items.erase(self->items.begin()); return v; });
	method(P, arrayProto, "unshift", [](Context &, const ObjRef &self, Args &a) {
		self->items.insert(self->items.begin(), a.begin(), a.end());
		return Value::number((double)self->items.size()); });
	method(P, arrayProto, "join", [](Context &, const ObjRef &self, Args &a) {
		const std::string sep = arg(a, 0).isUndef() ? "," : arg(a, 0).toString();
		std::string r;
		for (size_t i = 0; i < self->items.size(); ++i) { if (i) r += sep; if (!self->items[i].isUndef()) r += self->items[i].toString(); }
		return Value::string(r); });
	method(P, arrayProto, "toString", [](Context &, const ObjRef &self, Args &) { return Value::string(Value::object(self).toString()); });
	method(P, arrayProto, "reverse", [](Context &, const ObjRef &self, Args &) {
		std::reverse(self->items.begin(), self->items.end()); return Value::object(self); });
	method(P, arrayProto, "slice", [](Context &ctx, const ObjRef &self, Args &a) {
		const int n = (int)self->items.size();
		int s = arg(a, 0).isUndef() ? 0 : arg(a, 0).toInt(), e = arg(a, 1).isUndef() ? n : arg(a, 1).toInt();
		if (s < 0) s += n; if (e < 0) e += n;
		s = std::max(0, std::min(n, s)); e = std::max(s, std::min(n, e));
		return Value::object(ctx.player.makeArray(std::vector<Value>(self->items.begin() + s, self->items.begin() + e))); });
	method(P, arrayProto, "splice", [](Context &ctx, const ObjRef &self, Args &a) {
		const int n = (int)self->items.size();
		int s = arg(a, 0).toInt(); if (s < 0) s += n; s = std::max(0, std::min(n, s));
		int cnt = arg(a, 1).isUndef() ? n - s : arg(a, 1).toInt(); cnt = std::max(0, std::min(n - s, cnt));
		std::vector<Value> removed(self->items.begin() + s, self->items.begin() + s + cnt);
		self->items.erase(self->items.begin() + s, self->items.begin() + s + cnt);
		if (a.size() > 2) self->items.insert(self->items.begin() + s, a.begin() + 2, a.end());
		return Value::object(ctx.player.makeArray(removed)); });
	method(P, arrayProto, "concat", [](Context &ctx, const ObjRef &self, Args &a) {
		std::vector<Value> r = self->items;
		for (const Value &v : a)
			if (v.isObject() && v.o->kind == Object::ARRAY) r.insert(r.end(), v.o->items.begin(), v.o->items.end());
			else r.push_back(v);
		return Value::object(ctx.player.makeArray(r)); });
	method(P, arrayProto, "sort", [](Context &ctx, const ObjRef &self, Args &a) {
		Value cmp = arg(a, 0);
		int opts = 0;
		if (cmp.type == Value::T_NUMBER) { opts = cmp.toInt(); cmp = Value(); }
		else opts = arg(a, 1).toInt();
		const bool numeric = (opts & 16) != 0, desc = (opts & 2) != 0, ci = (opts & 1) != 0;
		Context *c = &ctx;
		std::stable_sort(self->items.begin(), self->items.end(), [&](const Value &x, const Value &y) {
			bool less;
			if (cmp.isObject())
			{
				Args pa = { x, y };
				less = c->player.callFunction(cmp.o, nullptr, pa, c->depth + 1).toNumber() < 0;
			}
			else if (numeric) less = x.toNumber() < y.toNumber();
			else if (ci) less = lower(x.toString()) < lower(y.toString());
			else less = x.toString() < y.toString();
			return less;
		});
		if (desc) std::reverse(self->items.begin(), self->items.end());
		return Value::object(self); });

	// ---------------- String ----------------
	ctor("String", stringProto, [](Context &ctx, const ObjRef &, Args &a) {
		ObjRef s = ctx.player.makeObject(ctx.player.stringProto);
		s->kind = Object::STRING_OBJ;
		s->str = a.empty() ? std::string() : a[0].toString();
		return Value::object(s); });
	m_global->get("String").o->set("fromCharCode", Value::object(makeNative([](Context &, const ObjRef &, Args &a) {
		std::string r; for (const Value &v : a) r += (char)v.toInt(); return Value::string(r); })), true);
	auto strOf = [](const ObjRef &self) { return self ? (self->kind == Object::STRING_OBJ ? self->str : Value::object(self).toString()) : std::string(); };
	method(P, stringProto, "toString", [strOf](Context &, const ObjRef &self, Args &) { return Value::string(strOf(self)); });
	method(P, stringProto, "valueOf", [strOf](Context &, const ObjRef &self, Args &) { return Value::string(strOf(self)); });
	method(P, stringProto, "toUpperCase", [strOf](Context &, const ObjRef &self, Args &) {
		std::string s = strOf(self); for (char &c : s) c = (char)toupper((unsigned char)c); return Value::string(s); });
	method(P, stringProto, "toLowerCase", [strOf](Context &, const ObjRef &self, Args &) { return Value::string(lower(strOf(self))); });
	method(P, stringProto, "charAt", [strOf](Context &, const ObjRef &self, Args &a) {
		std::string s = strOf(self); const int i = arg(a, 0).toInt();
		return Value::string(i >= 0 && (size_t)i < s.size() ? s.substr(i, 1) : std::string()); });
	method(P, stringProto, "charCodeAt", [strOf](Context &, const ObjRef &self, Args &a) {
		std::string s = strOf(self); const int i = arg(a, 0).toInt();
		return i >= 0 && (size_t)i < s.size() ? Value::number((unsigned char)s[i]) : Value::number(NAN); });
	method(P, stringProto, "indexOf", [strOf](Context &, const ObjRef &self, Args &a) {
		std::string s = strOf(self); size_t from = (size_t)std::max(0, arg(a, 1).toInt());
		size_t r = s.find(arg(a, 0).toString(), from);
		return Value::number(r == std::string::npos ? -1.0 : (double)r); });
	method(P, stringProto, "lastIndexOf", [strOf](Context &, const ObjRef &self, Args &a) {
		std::string s = strOf(self);
		size_t r = s.rfind(arg(a, 0).toString(), arg(a, 1).isUndef() ? std::string::npos : (size_t)std::max(0, arg(a, 1).toInt()));
		return Value::number(r == std::string::npos ? -1.0 : (double)r); });
	method(P, stringProto, "substr", [strOf](Context &, const ObjRef &self, Args &a) {
		std::string s = strOf(self); const int n = (int)s.size();
		int st = arg(a, 0).toInt(); if (st < 0) st = std::max(0, n + st); st = std::min(st, n);
		int len = arg(a, 1).isUndef() ? n - st : arg(a, 1).toInt(); len = std::max(0, std::min(len, n - st));
		return Value::string(s.substr(st, len)); });
	method(P, stringProto, "substring", [strOf](Context &, const ObjRef &self, Args &a) {
		std::string s = strOf(self); const int n = (int)s.size();
		int st = std::max(0, std::min(n, arg(a, 0).toInt()));
		int en = arg(a, 1).isUndef() ? n : std::max(0, std::min(n, arg(a, 1).toInt()));
		if (st > en) std::swap(st, en);
		return Value::string(s.substr(st, en - st)); });
	method(P, stringProto, "split", [strOf](Context &ctx, const ObjRef &self, Args &a) {
		std::string s = strOf(self);
		std::vector<Value> out;
		if (arg(a, 0).isUndef()) out.push_back(Value::string(s));
		else
		{
			const std::string sep = arg(a, 0).toString();
			if (sep.empty()) for (char c : s) out.push_back(Value::string(std::string(1, c)));
			else
			{
				size_t pos = 0;
				while (true)
				{
					size_t n = s.find(sep, pos);
					out.push_back(Value::string(s.substr(pos, n == std::string::npos ? std::string::npos : n - pos)));
					if (n == std::string::npos) break;
					pos = n + sep.size();
				}
			}
		}
		return Value::object(ctx.player.makeArray(out)); });

	// ---------------- Number / Boolean ----------------
	{
		ObjRef numProto = makeObject(objectProto);
		ObjRef numCtor = ctor("Number", numProto, [](Context &, const ObjRef &, Args &a) { return Value::number(a.empty() ? 0 : a[0].toNumber()); });
		numCtor->set("MAX_VALUE", Value::number(1.79769313486231e308), true);
		numCtor->set("MIN_VALUE", Value::number(4.9406564584124654e-324), true);
		numCtor->set("NaN", Value::number(NAN), true);
		ObjRef boolProto = makeObject(objectProto);
		ctor("Boolean", boolProto, [](Context &, const ObjRef &, Args &a) { return Value::boolean(!a.empty() && a[0].toBool()); });
		// called as functions (not constructors): conversions
		m_global->get("Number").o->native = [](Context &, const ObjRef &, Args &a) { return Value::number(a.empty() ? 0 : a[0].toNumber()); };
	}

	// ---------------- global functions ----------------
	auto global = [&](const char *name, const NativeFn &fn) { m_global->set(name, Value::object(makeNative(fn)), true); };
	global("parseInt", [](Context &, const ObjRef &, Args &a) {
		const std::string s = arg(a, 0).toString(); const int radix = arg(a, 1).isUndef() ? 10 : arg(a, 1).toInt();
		char *end = nullptr; const long v = strtol(s.c_str(), &end, radix);
		return end == s.c_str() ? Value::number(NAN) : Value::number((double)v); });
	global("parseFloat", [](Context &, const ObjRef &, Args &a) {
		const std::string s = arg(a, 0).toString(); char *end = nullptr; const double v = strtod(s.c_str(), &end);
		return end == s.c_str() ? Value::number(NAN) : Value::number(v); });
	global("isNaN", [](Context &, const ObjRef &, Args &a) { return Value::boolean(std::isnan(arg(a, 0).toNumber())); });
	global("isFinite", [](Context &, const ObjRef &, Args &a) { const double d = arg(a, 0).toNumber(); return Value::boolean(!std::isnan(d) && !std::isinf(d)); });
	global("escape", [](Context &, const ObjRef &, Args &a) {
		std::string s = arg(a, 0).toString(), r; char buf[4];
		for (unsigned char c : s) { if (isalnum(c)) r += (char)c; else { snprintf(buf, sizeof(buf), "%%%02X", c); r += buf; } }
		return Value::string(r); });
	global("unescape", [](Context &, const ObjRef &, Args &a) {
		std::string s = arg(a, 0).toString(), r;
		for (size_t i = 0; i < s.size(); ++i)
			if (s[i] == '%' && i + 2 < s.size()) { r += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
			else r += s[i];
		return Value::string(r); });
	global("trace", [](Context &, const ObjRef &, Args &a) {
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[apt] trace: %s\n", arg(a, 0).toString().c_str());
		return Value(); });
	global("ASSetPropFlags", [](Context &, const ObjRef &, Args &) { return Value(); });
	global("setInterval", [](Context &ctx, const ObjRef &, Args &a) {
		Player &p = ctx.player;
		Player::Interval iv;
		iv.id = p.nextIntervalId++;
		size_t next;
		if (arg(a, 0).isObject() && arg(a, 0).o->kind == Object::FUNCTION) { iv.fn = arg(a, 0).o; iv.self = ctx.self; next = 1; }
		else if (arg(a, 0).isObject()) { iv.self = arg(a, 0).o; iv.method = arg(a, 1).toString(); next = 2; }
		else return Value();
		iv.periodMs = (unsigned)std::max(1, arg(a, next).toInt());
		iv.nextMs = p.timeMs + iv.periodMs;
		for (size_t i = next + 1; i < a.size(); ++i) iv.args.push_back(a[i]);
		p.intervals.push_back(iv);
		return Value::number(iv.id); });
	global("clearInterval", [](Context &ctx, const ObjRef &, Args &a) {
		const int id = arg(a, 0).toInt();
		auto &v = ctx.player.intervals;
		v.erase(std::remove_if(v.begin(), v.end(), [id](const Player::Interval &i) { return i.id == id; }), v.end());
		return Value(); });
	global("getTimer", [](Context &ctx, const ObjRef &, Args &) { return Value::number(ctx.player.timeMs); });
	global("fscommand", [](Context &ctx, const ObjRef &, Args &a) {
		if (ctx.player.host()) ctx.player.host()->onFSCommand(ctx.player, arg(a, 0).toString(), arg(a, 1).toString());
		return Value(); });

	// ---------------- Math ----------------
	{
		ObjRef math = makeObject(objectProto);
		m_global->set("Math", Value::object(math), true);
		auto m1 = [&](const char *n, double (*f)(double)) {
			math->set(n, Value::object(makeNative([f](Context &, const ObjRef &, Args &a) { return Value::number(f(arg(a, 0).toNumber())); })), true); };
		m1("abs", fabs); m1("ceil", ceil); m1("floor", floor); m1("sqrt", sqrt); m1("sin", sin); m1("cos", cos); m1("tan", tan);
		m1("asin", asin); m1("acos", acos); m1("atan", atan); m1("exp", exp); m1("log", log);
		m1("round", [](double d) { return floor(d + 0.5); });
		math->set("atan2", Value::object(makeNative([](Context &, const ObjRef &, Args &a) { return Value::number(atan2(arg(a, 0).toNumber(), arg(a, 1).toNumber())); })), true);
		math->set("pow", Value::object(makeNative([](Context &, const ObjRef &, Args &a) { return Value::number(pow(arg(a, 0).toNumber(), arg(a, 1).toNumber())); })), true);
		math->set("min", Value::object(makeNative([](Context &, const ObjRef &, Args &a) {
			double r = INFINITY; for (const Value &v : a) r = std::min(r, v.toNumber()); return Value::number(r); })), true);
		math->set("max", Value::object(makeNative([](Context &, const ObjRef &, Args &a) {
			double r = -INFINITY; for (const Value &v : a) r = std::max(r, v.toNumber()); return Value::number(r); })), true);
		math->set("random", Value::object(makeNative([](Context &, const ObjRef &, Args &) { return Value::number(rand() / (RAND_MAX + 1.0)); })), true);
		math->set("PI", Value::number(3.14159265358979323846), true);
		math->set("E", Value::number(2.71828182845904523536), true);
		math->set("SQRT2", Value::number(1.41421356237309504880), true);
		math->set("LN2", Value::number(0.69314718055994530942), true);
		math->set("LN10", Value::number(2.30258509299404568402), true);
	}

	// ---------------- Mouse / Key / Stage ----------------
	{
		ObjRef mouseObj = makeObject(objectProto);
		m_global->set("Mouse", Value::object(mouseObj), true);
		mouseObj->set("addListener", Value::object(makeNative([](Context &ctx, const ObjRef &, Args &a) {
			if (arg(a, 0).isObject()) ctx.player.mouseListeners.push_back(arg(a, 0).o);
			return Value(); })), true);
		mouseObj->set("removeListener", Value::object(makeNative([](Context &ctx, const ObjRef &, Args &a) {
			auto &v = ctx.player.mouseListeners;
			if (arg(a, 0).isObject()) v.erase(std::remove(v.begin(), v.end(), arg(a, 0).o), v.end());
			return Value::boolean(true); })), true);
		mouseObj->set("show", Value::object(makeNative([](Context &, const ObjRef &, Args &) { return Value(); })), true);
		mouseObj->set("hide", Value::object(makeNative([](Context &, const ObjRef &, Args &) { return Value(); })), true);
		ObjRef keyObj = makeObject(objectProto);
		m_global->set("Key", Value::object(keyObj), true);
		keyObj->set("isDown", Value::object(makeNative([](Context &, const ObjRef &, Args &) { return Value::boolean(false); })), true);
		keyObj->set("addListener", Value::object(makeNative([](Context &, const ObjRef &, Args &) { return Value(); })), true);
		keyObj->set("removeListener", Value::object(makeNative([](Context &, const ObjRef &, Args &) { return Value(); })), true);
		ObjRef stage = makeObject(objectProto);
		m_global->set("Stage", Value::object(stage), true);
	}

	// ---------------- extern (game values) ----------------
	{
		struct Ext : public Object
		{
			Player *player = nullptr;
			bool getOwn(const std::string &key, const std::string &name, Value &out) override
			{
				if (Object::getOwn(key, name, out)) return true;
				if (player && player->host()) { out = player->host()->getExtern(*player, name); return true; }
				return false;
			}
			bool setOwn(const std::string &key, const std::string &name, const Value &v) override
			{
				if (player && player->host()) player->host()->setExtern(*player, name, v);
				return Object::setOwn(key, name, v);
			}
		};
		std::shared_ptr<Ext> e = std::make_shared<Ext>();
		e->player = this;
		e->proto = objectProto;
		externObj = e;
		m_global->set("extern", Value::object(externObj), true);
	}

	// ---------------- Color ----------------
	ctor("Color", colorProto, [](Context &ctx, const ObjRef &, Args &a) {
		ObjRef c = ctx.player.makeObject(ctx.player.colorProto);
		c->kind = Object::COLOR;
		std::shared_ptr<Instance> t = ctx.player.instanceOf(arg(a, 0));
		if (!t && arg(a, 0).type == Value::T_STRING)
		{
			bool found; t = ctx.player.instanceOf(ctx.resolvePath(arg(a, 0).s, found));
		}
		c->colorTarget = t;
		return Value::object(c); });
	method(P, colorProto, "setRGB", [](Context &, const ObjRef &self, Args &a) {
		std::shared_ptr<Instance> t = self->colorTarget.lock();
		if (!t) return Value();
		const int rgb = arg(a, 0).toInt();
		t->color.mul = { 0, 0, 0, t->color.mul.a };
		t->color.add = { ((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, t->color.add.a };
		t->scriptTouched = true;
		return Value(); });
	method(P, colorProto, "getRGB", [](Context &, const ObjRef &self, Args &) {
		std::shared_ptr<Instance> t = self->colorTarget.lock();
		if (!t) return Value::number(0);
		const ColorTransform &c = t->color;
		return Value::number(((int)(c.add.r * 255) << 16) | ((int)(c.add.g * 255) << 8) | (int)(c.add.b * 255)); });
	method(P, colorProto, "setTransform", [](Context &, const ObjRef &self, Args &a) {
		std::shared_ptr<Instance> t = self->colorTarget.lock();
		if (!t || !arg(a, 0).isObject()) return Value();
		ObjRef o = arg(a, 0).o;
		ColorTransform &c = t->color;
		auto rd = [&](const char *k, float &dst, float scale) { Value v = o->get(k); if (!v.isUndef()) dst = (float)(v.toNumber() / scale); };
		rd("ra", c.mul.r, 100); rd("ga", c.mul.g, 100); rd("ba", c.mul.b, 100); rd("aa", c.mul.a, 100);
		rd("rb", c.add.r, 255); rd("gb", c.add.g, 255); rd("bb", c.add.b, 255); rd("ab", c.add.a, 255);
		t->scriptTouched = true;
		return Value(); });
	method(P, colorProto, "getTransform", [](Context &ctx, const ObjRef &self, Args &) {
		std::shared_ptr<Instance> t = self->colorTarget.lock();
		ObjRef o = ctx.player.makeObject(ctx.player.objectProto);
		ColorTransform c = t ? t->color : ColorTransform();
		o->set("ra", Value::number(c.mul.r * 100)); o->set("ga", Value::number(c.mul.g * 100));
		o->set("ba", Value::number(c.mul.b * 100)); o->set("aa", Value::number(c.mul.a * 100));
		o->set("rb", Value::number(c.add.r * 255)); o->set("gb", Value::number(c.add.g * 255));
		o->set("bb", Value::number(c.add.b * 255)); o->set("ab", Value::number(c.add.a * 255));
		return Value::object(o); });

	// ---------------- MovieClip ----------------
	{
		ObjRef mcCtor = makeNative([](Context &, const ObjRef &, Args &) { return Value(); });
		mcCtor->set("prototype", Value::object(clipProto), true);
		clipProto->set("constructor", Value::object(mcCtor), true);
		m_global->set("MovieClip", Value::object(mcCtor), true);
		ObjRef tfCtor = makeNative([](Context &, const ObjRef &, Args &) { return Value(); });
		tfCtor->set("prototype", Value::object(textProto), true);
		m_global->set("TextField", Value::object(tfCtor), true);
	}
	method(P, clipProto, "play", [](Context &, const ObjRef &self, Args &) { auto c = selfClip(self); if (c) c->playing = true; return Value(); });
	method(P, clipProto, "stop", [](Context &, const ObjRef &self, Args &) { auto c = selfClip(self); if (c) c->playing = false; return Value(); });
	auto gotoImpl = [](Context &ctx, const ObjRef &self, Args &a, bool play) {
		auto c = selfClip(self);
		if (!c) return Value();
		const Value f = arg(a, 0);
		if (f.type == Value::T_STRING) ctx.player.gotoLabel(*c, f.s, play);
		else ctx.player.gotoFrame(*c, f.toInt() - 1, play);
		return Value();
	};
	method(P, clipProto, "gotoAndPlay", [gotoImpl](Context &ctx, const ObjRef &self, Args &a) { return gotoImpl(ctx, self, a, true); });
	method(P, clipProto, "gotoAndStop", [gotoImpl](Context &ctx, const ObjRef &self, Args &a) { return gotoImpl(ctx, self, a, false); });
	method(P, clipProto, "nextFrame", [](Context &ctx, const ObjRef &self, Args &) {
		auto c = selfClip(self); if (c) ctx.player.gotoFrame(*c, c->currentFrame + 1, false); return Value(); });
	method(P, clipProto, "prevFrame", [](Context &ctx, const ObjRef &self, Args &) {
		auto c = selfClip(self); if (c) ctx.player.gotoFrame(*c, c->currentFrame - 1, false); return Value(); });
	method(P, clipProto, "getDepth", [](Context &, const ObjRef &self, Args &) { auto c = selfClip(self); return Value::number(c ? c->depth : 0); });
	method(P, clipProto, "getNextHighestDepth", [](Context &, const ObjRef &self, Args &) {
		auto c = selfClip(self); return Value::number(c ? std::max(0, highestDepth(*c) + 1) : 0); });
	method(P, clipProto, "attachMovie", [](Context &ctx, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (!c) return Value();
		const std::string id = arg(a, 0).toString();
		Character *ch = findExport(*c, id, ctx.player);
		if (!ch)
		{
			fprintf(stderr, "[apt] attachMovie: no export '%s' in %s\n", id.c_str(), c->file ? c->file->name.c_str() : "?");
			return Value();
		}
		std::shared_ptr<Instance> child = addChild(ctx.player, *c, ch, c->file, arg(a, 1).toString(), arg(a, 2).toInt(),
			arg(a, 3).isObject() ? arg(a, 3).o : nullptr, nullptr);
		return Value::object(ctx.player.clipObject(child)); });
	method(P, clipProto, "createEmptyMovieClip", [](Context &ctx, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (!c) return Value();
		std::shared_ptr<Instance> child = addChild(ctx.player, *c, emptySprite(), c->file, arg(a, 0).toString(), arg(a, 1).toInt(), nullptr, nullptr);
		child->file = c->file;
		return Value::object(ctx.player.clipObject(child)); });
	method(P, clipProto, "duplicateMovieClip", [](Context &ctx, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (!c || !c->parent) return Value();
		std::shared_ptr<Instance> child = addChild(ctx.player, *c->parent, c->character, c->file, arg(a, 0).toString(), arg(a, 1).toInt(),
			arg(a, 2).isObject() ? arg(a, 2).o : nullptr, c.get());
		return Value::object(ctx.player.clipObject(child)); });
	method(P, clipProto, "removeMovieClip", [](Context &, const ObjRef &self, Args &) {
		auto c = selfClip(self);
		if (!c || !c->parent) return Value();
		Instance *parent = c->parent;
		auto it = parent->children.find(c->depth);
		if (it != parent->children.end() && it->second == c)
		{
			parent->children.erase(it);
			c->parent = nullptr;
		}
		return Value(); });
	method(P, clipProto, "swapDepths", [](Context &ctx, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (!c || !c->parent) return Value();
		Instance *parent = c->parent;
		int target;
		std::shared_ptr<Instance> other = ctx.player.instanceOf(arg(a, 0));
		if (other && other->parent == parent) target = other->depth;
		else target = arg(a, 0).toInt();
		auto o = parent->children.find(target);
		std::shared_ptr<Instance> swap = o != parent->children.end() ? o->second : nullptr;
		parent->children.erase(c->depth);
		if (swap) { swap->depth = c->depth; parent->children[swap->depth] = swap; swap->placedByTimeline = false; }
		c->depth = target;
		c->placedByTimeline = false;
		parent->children[target] = c;
		return Value(); });
	method(P, clipProto, "loadMovie", [](Context &ctx, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (c) ctx.player.loadMovieInto(*c, arg(a, 0).toString());
		return Value(); });
	method(P, clipProto, "unloadMovie", [](Context &, const ObjRef &self, Args &) {
		auto c = selfClip(self);
		if (c) { c->children.clear(); c->character = emptySprite(); c->currentFrame = 0; }
		return Value(); });
	method(P, clipProto, "getBounds", [](Context &ctx, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		ObjRef o = ctx.player.makeObject(ctx.player.objectProto);
		if (!c) return Value::object(o);
		Matrix m;	// in own coordinates unless a target space is given
		std::shared_ptr<Instance> space = ctx.player.instanceOf(arg(a, 0));
		if (space)
		{
			Matrix inv;
			if (space->worldMatrix().invert(inv)) m = c->worldMatrix().then(inv);
		}
		float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		c->bounds(x0, y0, x1, y1, m);
		o->set("xMin", Value::number(x0)); o->set("xMax", Value::number(x1));
		o->set("yMin", Value::number(y0)); o->set("yMax", Value::number(y1));
		return Value::object(o); });
	method(P, clipProto, "localToGlobal", [](Context &, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (!c || !arg(a, 0).isObject()) return Value();
		ObjRef pt = arg(a, 0).o;
		float x, y; c->worldMatrix().apply((float)pt->get("x").toNumber(), (float)pt->get("y").toNumber(), x, y);
		pt->set("x", Value::number(x)); pt->set("y", Value::number(y));
		return Value(); });
	method(P, clipProto, "globalToLocal", [](Context &, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (!c || !arg(a, 0).isObject()) return Value();
		ObjRef pt = arg(a, 0).o;
		Matrix inv;
		if (!c->worldMatrix().invert(inv)) return Value();
		float x, y; inv.apply((float)pt->get("x").toNumber(), (float)pt->get("y").toNumber(), x, y);
		pt->set("x", Value::number(x)); pt->set("y", Value::number(y));
		return Value(); });
	method(P, clipProto, "hitTest", [](Context &ctx, const ObjRef &self, Args &a) {
		auto c = selfClip(self);
		if (!c) return Value::boolean(false);
		float x0, y0, x1, y1;
		if (!c->bounds(x0, y0, x1, y1, c->worldMatrix())) return Value::boolean(false);
		if (a.size() >= 2)
		{
			const float x = (float)arg(a, 0).toNumber(), y = (float)arg(a, 1).toNumber();
			return Value::boolean(x >= x0 && x <= x1 && y >= y0 && y <= y1);
		}
		std::shared_ptr<Instance> o = ctx.player.instanceOf(arg(a, 0));
		float a0, b0, a1, b1;
		if (!o || !o->bounds(a0, b0, a1, b1, o->worldMatrix())) return Value::boolean(false);
		return Value::boolean(!(a1 < x0 || a0 > x1 || b1 < y0 || b0 > y1)); });
	for (const char *noop : { "setMask", "startDrag", "stopDrag", "clear", "lineStyle", "beginFill", "endFill", "moveTo", "lineTo", "createTextField" })
		method(P, clipProto, noop, [](Context &, const ObjRef &, Args &) { return Value(); });
	method(P, clipProto, "getURL", [](Context &ctx, const ObjRef &, Args &a) {
		const std::string url = arg(a, 0).toString();
		if (lower(url.substr(0, 10)) == "fscommand:" && ctx.player.host())
			ctx.player.host()->onFSCommand(ctx.player, url.substr(10), arg(a, 1).toString());
		return Value(); });

	// TextField
	method(P, textProto, "setTextFormat", [](Context &, const ObjRef &, Args &) { return Value(); });
	method(P, textProto, "setNewTextFormat", [](Context &, const ObjRef &, Args &) { return Value(); });
	(void)numToStr;
}

} // namespace Apt
