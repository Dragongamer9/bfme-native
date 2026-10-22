/*
**	bfme-native: APT movie player -- display list, timeline, script objects and builtins.
*/
#include "W3DDevice/GameClient/Apt/AptPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Apt
{

static bool aptDebug() { static int v = -1; if (v < 0) v = getenv("BFME_DEBUG_APT") ? 1 : 0; return v != 0; }

// ------------------------------------------------------------------------------------------------
// Instance helpers
// ------------------------------------------------------------------------------------------------
Instance *Instance::findChild(const std::string &n)
{
	const std::string key = lower(n);
	for (auto &kv : children)
		if (kv.second && !kv.second->name.empty() && lower(kv.second->name) == key)
			return kv.second.get();
	return nullptr;
}

std::string Instance::targetPath() const
{
	if (parent == nullptr) return "_level0";
	return parent->targetPath() + "." + name;
}

Matrix Instance::worldMatrix() const
{
	return parent ? matrix.then(parent->worldMatrix()) : matrix;
}

ColorTransform Instance::worldColor() const
{
	return parent ? color.then(parent->worldColor()) : color;
}

bool Instance::bounds(float &x0, float &y0, float &x1, float &y1, const Matrix &m) const
{
	bool any = false;
	auto addPt = [&](float x, float y) {
		float ox, oy; m.apply(x, y, ox, oy);
		if (!any) { x0 = x1 = ox; y0 = y1 = oy; any = true; return; }
		x0 = std::min(x0, ox); x1 = std::max(x1, ox); y0 = std::min(y0, oy); y1 = std::max(y1, oy);
	};
	if (!character) return false;
	switch (character->type)
	{
	case CHAR_SHAPE:
	{
		const Geometry *g = character->file->getGeometry(character->geometry);
		if (g && !g->entries.empty())
		{
			addPt(g->minX, g->minY); addPt(g->maxX, g->minY); addPt(g->minX, g->maxY); addPt(g->maxX, g->maxY);
		}
		break;
	}
	case CHAR_TEXT:
		addPt(character->bounds[0], character->bounds[1]); addPt(character->bounds[2], character->bounds[3]);
		break;
	case CHAR_BUTTON:
	{
		for (size_t i = 0; i + 1 < character->hitVerts.size(); i += 2) addPt(character->hitVerts[i], character->hitVerts[i + 1]);
		break;
	}
	default:
		break;
	}
	for (auto &kv : children)
	{
		if (!kv.second || !kv.second->visible) continue;
		float a, b, c, d;
		if (kv.second->bounds(a, b, c, d, kv.second->matrix.then(m)))
		{
			if (!any) { x0 = a; y0 = b; x1 = c; y1 = d; any = true; }
			else { x0 = std::min(x0, a); y0 = std::min(y0, b); x1 = std::max(x1, c); y1 = std::max(y1, d); }
		}
	}
	return any;
}

// ------------------------------------------------------------------------------------------------
// script objects with live properties
// ------------------------------------------------------------------------------------------------
static float matScaleX(const Matrix &m) { return sqrtf(m.m11 * m.m11 + m.m12 * m.m12); }
static float matScaleY(const Matrix &m) { return sqrtf(m.m21 * m.m21 + m.m22 * m.m22); }
static float matRotation(const Matrix &m) { return atan2f(m.m12, m.m11) * 180.0f / 3.14159265f; }
static void matCompose(Matrix &m, float sx, float sy, float rotDeg)
{
	const float r = rotDeg * 3.14159265f / 180.0f;
	const float c = cosf(r), s = sinf(r);
	m.m11 = c * sx; m.m12 = s * sx;
	m.m21 = -s * sy; m.m22 = c * sy;
}

struct ClipObject : public Object
{
	Player *player = nullptr;

	bool getOwn(const std::string &key, const std::string &name, Value &out) override
	{
		std::shared_ptr<Instance> inst = clip.lock();
		if (inst && !key.empty() && key[0] == '_')
		{
			Matrix &m = inst->matrix;
			if (key == "_x") { out = Value::number(m.tx); return true; }
			if (key == "_y") { out = Value::number(m.ty); return true; }
			if (key == "_xscale") { out = Value::number(matScaleX(m) * 100.0); return true; }
			if (key == "_yscale") { out = Value::number(matScaleY(m) * 100.0); return true; }
			if (key == "_rotation") { out = Value::number(matRotation(m)); return true; }
			if (key == "_alpha") { out = Value::number(inst->color.mul.a * 100.0); return true; }
			if (key == "_visible") { out = Value::boolean(inst->visible); return true; }
			if (key == "_currentframe") { out = Value::number(inst->currentFrame + 1); return true; }
			if (key == "_totalframes" || key == "_framesloaded") { out = Value::number(inst->totalFrames()); return true; }
			if (key == "_name") { out = Value::string(inst->name); return true; }
			if (key == "_target")
			{
				std::string p;
				for (const Instance *i = inst.get(); i && i->parent; i = i->parent) p = "/" + i->name + p;
				out = Value::string(p.empty() ? "/" : p);
				return true;
			}
			if (key == "_parent") { out = inst->parent ? Value::object(player->clipObject(inst->parent->shared_from_this())) : Value(); return true; }
			if (key == "_width" || key == "_height")
			{
				float x0, y0, x1, y1;
				const Matrix pm = inst->matrix;
				if (!inst->bounds(x0, y0, x1, y1, pm)) { out = Value::number(0); return true; }
				out = Value::number(key == "_width" ? x1 - x0 : y1 - y0);
				return true;
			}
			if (key == "_xmouse" || key == "_ymouse")
			{
				Matrix inv;
				float lx = player->mouseX, ly = player->mouseY;
				if (inst->worldMatrix().invert(inv)) inv.apply(player->mouseX, player->mouseY, lx, ly);
				out = Value::number(key == "_xmouse" ? lx : ly);
				return true;
			}
			if (key == "_url") { out = Value::string(inst->file ? inst->file->name : ""); return true; }
			if (key == "_droptarget" || key == "_quality" || key == "_highquality" || key == "_focusrect" || key == "_soundbuftime") { out = Value(); return true; }
		}
		if (inst && inst->character && inst->character->type == CHAR_TEXT)
		{
			if (key == "text" || key == "htmltext")
			{
				out = Value::string(inst->textSet ? inst->text : inst->character->content);
				return true;
			}
			if (key == "textcolor")
			{
				const Color &c = inst->character->textColor;
				const ColorTransform &x = inst->color;
				const int rgb = ((int)(c.r * x.mul.r * 255 + x.add.r * 255) << 16) | ((int)(c.g * x.mul.g * 255 + x.add.g * 255) << 8) | (int)(c.b * x.mul.b * 255 + x.add.b * 255);
				out = Value::number(rgb);
				return true;
			}
		}
		if (Object::getOwn(key, name, out)) return true;
		if (inst)
		{
			Instance *c = inst->findChild(name);
			if (c) { out = Value::object(player->clipObject(c->shared_from_this())); return true; }
		}
		return false;
	}

	bool setOwn(const std::string &key, const std::string &name, const Value &v) override
	{
		std::shared_ptr<Instance> inst = clip.lock();
		if (inst && !key.empty() && key[0] == '_')
		{
			Matrix &m = inst->matrix;
			const double d = v.toNumber();
			const bool ok = !std::isnan(d);
			if (key == "_x") { if (ok) m.tx = (float)d; inst->scriptTouched = true; return true; }
			if (key == "_y") { if (ok) m.ty = (float)d; inst->scriptTouched = true; return true; }
			if (key == "_xscale") { if (ok) matCompose(m, (float)d / 100.0f, matScaleY(m), matRotation(m)); inst->scriptTouched = true; return true; }
			if (key == "_yscale") { if (ok) matCompose(m, matScaleX(m), (float)d / 100.0f, matRotation(m)); inst->scriptTouched = true; return true; }
			if (key == "_rotation") { if (ok) matCompose(m, matScaleX(m), matScaleY(m), (float)d); inst->scriptTouched = true; return true; }
			if (key == "_alpha") { if (ok) inst->color.mul.a = (float)(d / 100.0); inst->scriptTouched = true; return true; }
			if (key == "_visible") { inst->visible = v.toBool(); return true; }
			if (key == "_name") { inst->name = v.toString(); return true; }
			if (key == "_width" || key == "_height")
			{
				float x0, y0, x1, y1;
				if (ok && inst->bounds(x0, y0, x1, y1, inst->matrix))
				{
					const float cur = key == "_width" ? x1 - x0 : y1 - y0;
					if (cur > 1e-6f)
					{
						const float f = (float)d / cur;
						if (key == "_width") { m.m11 *= f; m.m12 *= f; } else { m.m21 *= f; m.m22 *= f; }
					}
				}
				inst->scriptTouched = true;
				return true;
			}
			if (key == "_currentframe" || key == "_totalframes" || key == "_target" || key == "_parent" || key == "_xmouse" || key == "_ymouse")
				return true;	// read-only
		}
		if (inst && inst->character && inst->character->type == CHAR_TEXT)
		{
			if (key == "text" || key == "htmltext")
			{
				inst->text = v.toString();
				inst->textSet = true;
				return true;
			}
			if (key == "textcolor")
			{
				const int rgb = v.toInt();
				const Color &c = inst->character->textColor;
				inst->color.mul = { 0, 0, 0, inst->color.mul.a };
				inst->color.add = { ((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, 0 };
				(void)c;
				return true;
			}
		}
		return Object::setOwn(key, name, v);
	}

	void keys(std::vector<std::string> &out) override
	{
		Object::keys(out);
		std::shared_ptr<Instance> inst = clip.lock();
		if (inst)
			for (auto &kv : inst->children)
				if (kv.second && !kv.second->name.empty()) out.push_back(kv.second->name);
	}
};

// ------------------------------------------------------------------------------------------------
// Player basics
// ------------------------------------------------------------------------------------------------
Player::Player()
{
	installBuiltins();
}

Player::~Player()
{
	// break cycles (objects hold functions that capture scopes holding objects ...)
	intervals.clear();
	mouseListeners.clear();
	m_pendingActions.clear();
	if (m_global) { m_global->props.clear(); m_global->order.clear(); }
	std::vector<Instance *> stack;
	if (m_root) stack.push_back(m_root.get());
	while (!stack.empty())
	{
		Instance *i = stack.back(); stack.pop_back();
		if (i->object) { i->object->props.clear(); i->object->order.clear(); }
		for (auto &kv : i->children) if (kv.second) stack.push_back(kv.second.get());
	}
	for (ObjRef *p : { &objectProto, &functionProto, &arrayProto, &clipProto, &stringProto, &colorProto, &textProto, &externObj })
		if (*p) { (*p)->props.clear(); (*p)->order.clear(); }
}

ObjRef Player::makeObject(const ObjRef &proto)
{
	ObjRef o = std::make_shared<Object>();
	o->proto = proto;
	return o;
}

ObjRef Player::makeArray(const std::vector<Value> &items)
{
	ObjRef o = makeObject(arrayProto);
	o->kind = Object::ARRAY;
	o->items = items;
	return o;
}

ObjRef Player::makeNative(const NativeFn &fn)
{
	ObjRef o = makeObject(functionProto);
	o->kind = Object::FUNCTION;
	o->native = fn;
	return o;
}

ObjRef Player::clipObject(const std::shared_ptr<Instance> &inst)
{
	if (!inst) return nullptr;
	if (!inst->object)
	{
		std::shared_ptr<ClipObject> o = std::make_shared<ClipObject>();
		o->kind = Object::CLIP;
		o->player = this;
		o->clip = inst;
		o->proto = (inst->character && inst->character->type == CHAR_TEXT) ? textProto : clipProto;
		inst->object = o;
	}
	return inst->object;
}

std::shared_ptr<Instance> Player::instanceOf(const Value &v)
{
	if (v.isObject())
	{
		if (v.o->kind == Object::CLIP) return v.o->clip.lock();
		return nullptr;
	}
	if (v.type == Value::T_STRING && !v.s.empty())
		return find(v.s);
	return nullptr;
}

std::shared_ptr<Instance> Player::find(const std::string &path)
{
	if (!m_root) return nullptr;
	Context ctx(*this);
	ctx.target = m_root;
	ctx.self = clipObject(m_root);
	bool found = false;
	Value v = ctx.resolvePath(path, found);
	if (v.isObject() && v.o->kind == Object::CLIP) return v.o->clip.lock();
	return nullptr;
}

Value Player::get(const std::string &path)
{
	if (!m_root) return Value();
	Context ctx(*this);
	ctx.target = m_root;
	ctx.self = clipObject(m_root);
	bool found = false;
	return ctx.resolvePath(path, found);
}

void Player::set(const std::string &path, const Value &v)
{
	if (!m_root) return;
	Context ctx(*this);
	ctx.target = m_root;
	ctx.self = clipObject(m_root);
	ctx.setVariable(path, v);
}

Value Player::call(const std::string &path, std::vector<Value> args)
{
	if (!m_root) return Value();
	const size_t dot = path.find_last_of(".:");
	Value holder = dot == std::string::npos ? Value::object(clipObject(m_root)) : get(path.substr(0, dot));
	const std::string method = dot == std::string::npos ? path : path.substr(dot + 1);
	if (!holder.isObject())
	{
		if (aptDebug()) fprintf(stderr, "[apt] call %s: no object\n", path.c_str());
		return Value();
	}
	Value fn = holder.o->get(method);
	if (!fn.isObject() || fn.o->kind != Object::FUNCTION)
	{
		if (aptDebug()) fprintf(stderr, "[apt] call %s: not a function\n", path.c_str());
		return Value();
	}
	Value r = callFunction(fn.o, holder.o, args);
	runPending();
	return r;
}

// ------------------------------------------------------------------------------------------------
// execution
// ------------------------------------------------------------------------------------------------
void Player::execute(const Code &code, const std::shared_ptr<Instance> &target, const std::vector<Value> *pool)
{
	if (!code.valid() || !target) return;
	Context ctx(*this);
	ctx.target = target;
	ctx.self = clipObject(target);
	ctx.registers.resize(4);
	if (pool) ctx.pool = *pool;
	runCode(ctx, code);
}

Value Player::callFunction(const ObjRef &fn, const ObjRef &self, std::vector<Value> &args, int depth)
{
	if (!fn || fn->kind != Object::FUNCTION) return Value();
	if (depth > 96)
	{
		fprintf(stderr, "[apt] call depth exceeded\n");
		return Value();
	}
	Context ctx(*this);
	ctx.depth = depth;
	std::shared_ptr<Instance> tgt = fn->target.lock();
	if (!tgt && self && self->kind == Object::CLIP) tgt = self->clip.lock();
	if (!tgt) tgt = m_root;
	ctx.target = tgt;
	ctx.self = self ? self : clipObject(tgt);
	if (fn->native)
		return fn->native(ctx, ctx.self, args);
	if (!fn->body.valid()) return Value();
	ctx.scope = fn->scope;
	ctx.pool = fn->pool;
	ctx.locals = makeObject(nullptr);
	ObjRef argsArr = makeArray(args);
	if (fn->isFunction2)
	{
		enum { PRELOAD_THIS = 0x100, SUPPRESS_THIS = 0x200, PRELOAD_ARGS = 0x400, SUPPRESS_ARGS = 0x800,
			PRELOAD_SUPER = 0x1000, SUPPRESS_SUPER = 0x2000, PRELOAD_ROOT = 0x4000, PRELOAD_PARENT = 0x8000,
			PRELOAD_GLOBAL = 0x1, PRELOAD_EXTERN = 0x10000 };
		ctx.registers.resize(std::max<size_t>(fn->numRegisters + 1, 8));
		int reg = 1;
		const unsigned f = fn->flags2;
		if (f & PRELOAD_THIS) ctx.registers[reg++] = Value::object(ctx.self);
		if (f & PRELOAD_ARGS) ctx.registers[reg++] = Value::object(argsArr);
		if (f & PRELOAD_SUPER)
		{
			Value sup;
			if (ctx.self && ctx.self->proto) sup = ctx.self->proto->get("__constructor__");
			ctx.registers[reg++] = sup;
		}
		if (f & PRELOAD_ROOT) ctx.registers[reg++] = Value::object(clipObject(m_root));
		if (f & PRELOAD_PARENT) ctx.registers[reg++] = (tgt && tgt->parent) ? Value::object(clipObject(tgt->parent->shared_from_this())) : Value();
		if (f & PRELOAD_GLOBAL) ctx.registers[reg++] = Value::object(m_global);
		if (f & PRELOAD_EXTERN) ctx.registers[reg++] = Value::object(externObj);
		if (!(f & SUPPRESS_THIS)) ctx.locals->set("this", Value::object(ctx.self));
		if (!(f & SUPPRESS_ARGS)) ctx.locals->set("arguments", Value::object(argsArr));
		for (size_t i = 0; i < fn->params.size(); ++i)
		{
			const Value a = i < args.size() ? args[i] : Value();
			const int r = fn->params[i].first;
			if (r > 0 && (size_t)r < ctx.registers.size()) ctx.registers[r] = a;
			else ctx.locals->set(fn->params[i].second, a);
		}
	}
	else
	{
		ctx.registers.resize(4);
		ctx.locals->set("this", Value::object(ctx.self));
		ctx.locals->set("arguments", Value::object(argsArr));
		for (size_t i = 0; i < fn->params.size(); ++i)
			ctx.locals->set(fn->params[i].second, i < args.size() ? args[i] : Value());
	}
	runCode(ctx, fn->body, fn->body.offset + fn->bodySize);
	return ctx.returned ? ctx.returnValue : Value();
}

void Player::queueCode(const std::shared_ptr<Instance> &clip, const Code &code)
{
	if (!code.valid() || !clip) return;
	Pending p; p.clip = clip; p.code = code;
	m_pendingActions.push_back(p);
}

void Player::queueHandler(const std::shared_ptr<Instance> &clip, const std::string &handler)
{
	if (!clip || !clip->object) return;
	Value fn = clip->object->get(handler);
	if (!fn.isObject() || fn.o->kind != Object::FUNCTION) return;
	Pending p; p.clip = clip; p.handler = handler;
	m_pendingActions.push_back(p);
}

void Player::queueActions(Instance &clip, int frame)
{
	if (!clip.isSprite() || frame < 0 || frame >= clip.totalFrames()) return;
	std::shared_ptr<Instance> self = clip.shared_from_this();
	for (const FrameItem &it : clip.character->frames[frame].items)
		if (it.kind == FrameItem::ACTION) queueCode(self, it.code);
}

void Player::runPendingFrom(size_t first)
{
	// run (and remove) the actions queued at index >= first, in order
	while (m_pendingActions.size() > first)
	{
		Pending p = m_pendingActions[first];
		m_pendingActions.erase(m_pendingActions.begin() + first);
		std::shared_ptr<Instance> clip = p.clip.lock();
		if (!clip) continue;
		if (!p.handler.empty())
		{
			Value fn = clipObject(clip)->get(p.handler);
			std::vector<Value> none;
			if (fn.isObject()) callFunction(fn.o, clipObject(clip), none);
		}
		else
			execute(p.code, clip);
	}
}

void Player::runPending()
{
	size_t guard = 0;
	while (!m_pendingActions.empty() && guard++ < 100000)
	{
		Pending p = m_pendingActions.front();
		m_pendingActions.erase(m_pendingActions.begin());
		std::shared_ptr<Instance> clip = p.clip.lock();
		if (!clip) continue;
		// skip clips that were removed from the display list meanwhile
		Instance *top = clip.get();
		while (top->parent) top = top->parent;
		if (top != m_root.get()) continue;
		if (!p.handler.empty())
		{
			Value fn = clipObject(clip)->get(p.handler);
			std::vector<Value> none;
			if (fn.isObject()) callFunction(fn.o, clipObject(clip), none);
		}
		else
			execute(p.code, clip);
	}
}

void Player::fireClipEvent(Instance &inst, unsigned flag)
{
	std::shared_ptr<Instance> self = inst.shared_from_this();
	for (const ClipEvent &e : inst.clipEvents)
		if (e.flags & flag)
		{
			// onClipEvent handlers run in the clip's own timeline
			queueCode(self, e.code);
		}
	const char *handler = nullptr;
	switch (flag)
	{
	case CLIPEVENT_LOAD: handler = "onLoad"; break;
	case CLIPEVENT_ENTERFRAME: handler = "onEnterFrame"; break;
	case CLIPEVENT_UNLOAD: handler = "onUnload"; break;
	case CLIPEVENT_PRESS: handler = "onPress"; break;
	case CLIPEVENT_RELEASE: handler = "onRelease"; break;
	case CLIPEVENT_RELEASEOUTSIDE: handler = "onReleaseOutside"; break;
	case CLIPEVENT_ROLLOVER: handler = "onRollOver"; break;
	case CLIPEVENT_ROLLOUT: handler = "onRollOut"; break;
	case CLIPEVENT_MOUSEDOWN: handler = "onMouseDown"; break;
	case CLIPEVENT_MOUSEUP: handler = "onMouseUp"; break;
	case CLIPEVENT_MOUSEMOVE: handler = "onMouseMove"; break;
	default: break;
	}
	if (handler) queueHandler(self, handler);
}

// ------------------------------------------------------------------------------------------------
// display list / timeline
// ------------------------------------------------------------------------------------------------
std::shared_ptr<Instance> Player::createInstance(Character *c, File *file, Instance *parent, int depth)
{
	std::shared_ptr<Instance> inst = std::make_shared<Instance>();
	inst->character = c;
	inst->file = (c && c->file) ? c->file : file;
	inst->player = this;
	inst->parent = parent;
	inst->depth = depth;
	inst->bornTick = m_tick;
	return inst;
}

void Player::initInstance(const std::shared_ptr<Instance> &inst)
{
	Character *c = inst->character;
	if (!c) return;
	switch (c->type)
	{
	case CHAR_SPRITE:
	case CHAR_MOVIE:
		clipObject(inst);
		fireClipEvent(*inst, CLIPEVENT_INITIALIZE);
		fireClipEvent(*inst, CLIPEVENT_CONSTRUCT);
		fireClipEvent(*inst, CLIPEVENT_LOAD);
		inst->currentFrame = -1;
		applyFrame(*inst, 0, true);
		break;
	case CHAR_BUTTON:
	{
		clipObject(inst);
		inst->recordStates.clear();
		int idx = 0;
		for (const ButtonRecord &r : c->records)
		{
			Character *rc = (r.character >= 0 && (size_t)r.character < c->file->characters.size()) ? c->file->characters[r.character] : nullptr;
			if (!rc) { ++idx; continue; }
			std::shared_ptr<Instance> child = createInstance(rc, c->file, inst.get(), idx);
			child->matrix = r.matrix;
			child->color = r.color;
			inst->children[idx] = child;
			if ((size_t)idx >= inst->recordStates.size()) inst->recordStates.resize(idx + 1, 0);
			inst->recordStates[idx] = r.states;
			initInstance(child);
			++idx;
		}
		break;
	}
	case CHAR_TEXT:
		clipObject(inst);
		break;
	default:
		break;
	}
}

void Player::removeInstance(Instance &parent, int depth)
{
	auto it = parent.children.find(depth);
	if (it == parent.children.end()) return;
	std::shared_ptr<Instance> child = it->second;
	parent.children.erase(it);
	if (!child) return;
	// onUnload runs right away (the clip is going)
	if (child->object)
	{
		Value fn = child->object->get("onUnload");
		std::vector<Value> none;
		if (fn.isObject() && fn.o->kind == Object::FUNCTION) callFunction(fn.o, child->object, none);
	}
	for (const ClipEvent &e : child->clipEvents)
		if (e.flags & CLIPEVENT_UNLOAD) execute(e.code, child);
	if (m_hover.lock() == child) m_hover.reset();
	if (m_pressed.lock() == child) m_pressed.reset();
	child->parent = nullptr;	// detached (descendants stay valid while referenced)
}

void Player::placeItem(Instance &clip, const FrameItem &it)
{
	auto existing = clip.children.find(it.depth);
	std::shared_ptr<Instance> inst = existing != clip.children.end() ? existing->second : nullptr;
	const bool move = (it.flags & PLACE_MOVE) != 0;
	Character *c = nullptr;
	if (it.flags & PLACE_HAS_CHARACTER)
	{
		File *f = clip.file;
		if (f && it.character >= 0 && (size_t)it.character < f->characters.size())
			c = f->characters[it.character];
		if (!c && aptDebug()) fprintf(stderr, "[apt] %s: missing character %d\n", f ? f->name.c_str() : "?", it.character);
	}
	if (inst && move && (!c || c == inst->character))
	{
		// modify the existing instance
		if (!inst->scriptTouched)
		{
			if (it.flags & PLACE_HAS_MATRIX) inst->matrix = it.matrix;
			if (it.flags & PLACE_HAS_COLOR) inst->color = it.color;
		}
		if (it.flags & PLACE_HAS_RATIO) inst->ratio = it.ratio;
		if (it.flags & PLACE_HAS_NAME) inst->name = it.name;
		if (it.flags & PLACE_HAS_CLIPDEPTH) inst->clipDepth = it.clipDepth;
		return;
	}
	if (!c) return;
	if (inst) removeInstance(clip, it.depth);
	std::shared_ptr<Instance> child = createInstance(c, clip.file, &clip, it.depth);
	if (it.flags & PLACE_HAS_MATRIX) child->matrix = it.matrix;
	else if (inst) child->matrix = inst->matrix;
	if (it.flags & PLACE_HAS_COLOR) child->color = it.color;
	if (it.flags & PLACE_HAS_RATIO) child->ratio = it.ratio;
	if (it.flags & PLACE_HAS_NAME) child->name = it.name;
	if (it.flags & PLACE_HAS_CLIPDEPTH) child->clipDepth = it.clipDepth;
	child->clipEvents = it.clipEvents;
	clip.children[it.depth] = child;
	initInstance(child);
}

void Player::applyFrame(Instance &clip, int frame, bool runActions)
{
	if (!clip.isSprite() || frame < 0 || frame >= clip.totalFrames()) return;
	clip.currentFrame = frame;
	const Frame &fr = clip.character->frames[frame];
	// the parent's frame script runs before the first-frame scripts of clips placed in this frame
	// (RotWK buttons define functions their children call from their own first frame)
	if (runActions) queueActions(clip, frame);
	for (const FrameItem &it : fr.items)
	{
		switch (it.kind)
		{
		case FrameItem::PLACE: placeItem(clip, it); break;
		case FrameItem::REMOVE: removeInstance(clip, it.depth); break;
		case FrameItem::INITACTION:
		{
			const unsigned char *key = it.code.file ? it.code.file->data.data() + it.code.offset : nullptr;
			if (key && std::find(m_initActionsDone.begin(), m_initActionsDone.end(), key) == m_initActionsDone.end())
			{
				m_initActionsDone.push_back(key);
				execute(it.code, clip.shared_from_this());
			}
			break;
		}
		default: break;
		}
	}
}

void Player::rebuildTo(Instance &clip, int frame)
{
	// replay the display list from frame 0, keeping instances that survive unchanged
	struct Slot { Character *c = nullptr; FrameItem item; bool fresh = false; };
	std::map<int, Slot> state;
	File *f = clip.file;
	for (int i = 0; i <= frame; ++i)
		for (const FrameItem &it : clip.character->frames[i].items)
		{
			if (it.kind == FrameItem::REMOVE) { state.erase(it.depth); continue; }
			if (it.kind != FrameItem::PLACE) continue;
			Character *c = nullptr;
			if ((it.flags & PLACE_HAS_CHARACTER) && f && it.character >= 0 && (size_t)it.character < f->characters.size())
				c = f->characters[it.character];
			auto s = state.find(it.depth);
			if (s != state.end() && (it.flags & PLACE_MOVE) && (!c || c == s->second.c))
			{
				FrameItem &m = s->second.item;
				if (it.flags & PLACE_HAS_MATRIX) { m.matrix = it.matrix; m.flags |= PLACE_HAS_MATRIX; }
				if (it.flags & PLACE_HAS_COLOR) { m.color = it.color; m.flags |= PLACE_HAS_COLOR; }
				if (it.flags & PLACE_HAS_NAME) { m.name = it.name; m.flags |= PLACE_HAS_NAME; }
				if (it.flags & PLACE_HAS_RATIO) { m.ratio = it.ratio; m.flags |= PLACE_HAS_RATIO; }
				continue;
			}
			if (!c) continue;
			Slot sl; sl.c = c; sl.item = it; sl.item.flags &= ~PLACE_MOVE;
			state[it.depth] = sl;
		}
	// remove timeline children that are gone or changed
	std::vector<int> drop;
	for (auto &kv : clip.children)
	{
		if (!kv.second || !kv.second->placedByTimeline) continue;
		auto s = state.find(kv.first);
		if (s == state.end() || s->second.c != kv.second->character) drop.push_back(kv.first);
	}
	for (int d : drop) removeInstance(clip, d);
	clip.currentFrame = frame;
	for (auto &kv : state)
	{
		auto ex = clip.children.find(kv.first);
		const FrameItem &it = kv.second.item;
		if (ex != clip.children.end() && ex->second)
		{
			Instance &inst = *ex->second;
			if (!inst.placedByTimeline) continue;
			if (!inst.scriptTouched)
			{
				inst.matrix = (it.flags & PLACE_HAS_MATRIX) ? it.matrix : Matrix();
				inst.color = (it.flags & PLACE_HAS_COLOR) ? it.color : ColorTransform();
			}
			if (it.flags & PLACE_HAS_NAME) inst.name = it.name;
			continue;
		}
		placeItem(clip, it);
	}
	queueActions(clip, frame);
}

void Player::gotoFrame(Instance &clip, int frame, bool play)
{
	if (!clip.isSprite()) return;
	const int total = clip.totalFrames();
	if (total <= 0) return;
	if (frame < 0) frame = 0;
	if (frame >= total) frame = total - 1;
	clip.playing = play;
	if (frame == clip.currentFrame)
	{
		// APT re-runs the target frame's script on a goto to the current frame (RotWK: Hide() =
		// gotoAndPlay("_hide") on a clip already sitting at "_hide", whose script is stop())
		queueActions(clip, frame);
		return;
	}
	if (frame < clip.currentFrame)
		rebuildTo(clip, frame);
	else
		for (int f = clip.currentFrame + 1; f <= frame; ++f)
			applyFrame(clip, f, f == frame);
}

bool Player::gotoLabel(Instance &clip, const std::string &label, bool play)
{
	if (!clip.isSprite()) return false;
	auto it = clip.character->labels.find(lower(label));
	if (it == clip.character->labels.end())
	{
		// "frame number as string"
		char *end = nullptr;
		const long n = strtol(label.c_str(), &end, 10);
		if (!label.empty() && end && *end == 0) { gotoFrame(clip, (int)n - 1, play); return true; }
		if (aptDebug()) fprintf(stderr, "[apt] %s: no label '%s'\n", clip.targetPath().c_str(), label.c_str());
		clip.playing = play;
		return false;
	}
	gotoFrame(clip, it->second, play);
	return true;
}

std::shared_ptr<Instance> Player::loadMovieInto(Instance &target, const std::string &movieIn)
{
	std::string movie = movieIn;
	if (m_host && !m_host->allowLoadMovie(*this, movie)) return nullptr;
	File *f = library().load(movie);
	if (!f || !f->movie)
	{
		fprintf(stderr, "[apt] loadMovie: cannot load %s\n", movie.c_str());
		return nullptr;
	}
	std::vector<int> depths;
	for (auto &kv : target.children) depths.push_back(kv.first);
	for (int d : depths) removeInstance(target, d);
	target.character = f->movie;
	target.file = f;
	target.currentFrame = -1;
	target.playing = true;
	target.clipEvents.clear();
	std::shared_ptr<Instance> self = target.shared_from_this();
	if (target.object)
	{
		// a loaded movie starts with fresh variables (keep the object identity for references)
		target.object->props.clear();
		target.object->order.clear();
	}
	clipObject(self);
	applyFrame(target, 0, true);
	return self;
}

void Player::advance(Instance &inst)
{
	if (!inst.isSprite())
	{
		if (inst.character && inst.character->type == CHAR_BUTTON)
			for (auto &kv : inst.children) if (kv.second) advance(*kv.second);
		return;
	}
	if (inst.bornTick == m_tick) return;	// placed this tick: already showing frame 1
	fireClipEvent(inst, CLIPEVENT_ENTERFRAME);
	if (inst.playing)
	{
		const int total = inst.totalFrames();
		if (total > 1)
		{
			const int next = inst.currentFrame + 1;
			if (next >= total) rebuildTo(inst, 0);
			else applyFrame(inst, next, true);
		}
	}
	std::vector<std::shared_ptr<Instance>> kids;
	kids.reserve(inst.children.size());
	for (auto &kv : inst.children) if (kv.second) kids.push_back(kv.second);
	for (auto &k : kids)
		if (k->parent == &inst) advance(*k);
}

bool Player::load(const std::string &movieName)
{
	m_file = library().load(movieName);
	if (!m_file || !m_file->movie)
	{
		fprintf(stderr, "[apt] cannot load movie %s: %s\n", movieName.c_str(), library().lastError.c_str());
		return false;
	}
	m_root = createInstance(m_file->movie, m_file, nullptr, 0);
	m_root->bornTick = 0;
	clipObject(m_root);
	// game entry points on _root
	static const char *gameCalls[] = { "GameCode", "GameCodeNoPrefix", "PlaySound", "GetExtern", "SetExtern", "ToggleExtern" };
	for (const char *name : gameCalls)
	{
		const std::string method = name;
		m_root->object->set(method, Value::object(makeNative([method](Context &ctx, const ObjRef &, std::vector<Value> &args) -> Value {
			if (aptDebug())
			{
				std::string a;
				for (const Value &v : args) a += (a.empty() ? "" : ", ") + v.toString();
				fprintf(stderr, "[apt] _root.%s(%s)\n", method.c_str(), a.c_str());
			}
			if (ctx.player.host()) return ctx.player.host()->onGameCall(ctx.player, "_root", method, args);
			return Value();
		})), true);
	}
	applyFrame(*m_root, 0, true);
	runPending();
	return true;
}

void Player::update(unsigned elapsedMs)
{
	if (!m_root || !m_file) return;
	timeMs += elapsedMs;
	m_accum += elapsedMs;
	const unsigned period = m_file->msPerFrame ? m_file->msPerFrame : 33;
	int steps = 0;
	while (m_accum >= period && steps < 4)
	{
		m_accum -= period;
		++steps;
		++m_tick;
		advance(*m_root);
		runPending();
	}
	if (m_accum > period * 4) m_accum = 0;
	// intervals
	for (size_t i = 0; i < intervals.size(); ++i)
	{
		if (timeMs < intervals[i].nextMs) continue;
		intervals[i].nextMs = timeMs + std::max(1u, intervals[i].periodMs);
		Interval iv = intervals[i];
		std::vector<Value> args = iv.args;
		if (iv.fn) callFunction(iv.fn, iv.self, args);
		else if (iv.self)
		{
			Value fn = iv.self->get(iv.method);
			if (fn.isObject()) callFunction(fn.o, iv.self, args);
		}
	}
	runPending();
}

// ------------------------------------------------------------------------------------------------
// input
// ------------------------------------------------------------------------------------------------
static bool pointInTris(const std::vector<float> &v, float x, float y)
{
	for (size_t i = 0; i + 5 < v.size(); i += 6)
	{
		const float ax = v[i], ay = v[i + 1], bx = v[i + 2], by = v[i + 3], cx = v[i + 4], cy = v[i + 5];
		const float d1 = (x - bx) * (ay - by) - (ax - bx) * (y - by);
		const float d2 = (x - cx) * (by - cy) - (bx - cx) * (y - cy);
		const float d3 = (x - ax) * (cy - ay) - (cx - ax) * (y - ay);
		const bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
		if (!(neg && pos)) return true;
	}
	return false;
}

static bool clipHasMouseHandlers(Instance &inst)
{
	if (!inst.object) return false;
	for (const char *h : { "onPress", "onRelease", "onRollOver", "onRollOut", "onReleaseOutside" })
	{
		Value v = inst.object->get(h);
		if (v.isObject() && v.o->kind == Object::FUNCTION) return true;
	}
	for (const ClipEvent &e : inst.clipEvents)
		if (e.flags & (CLIPEVENT_PRESS | CLIPEVENT_RELEASE | CLIPEVENT_ROLLOVER | CLIPEVENT_ROLLOUT)) return true;
	return false;
}

/// topmost hit; 'button' gets the innermost interactive instance (button or clip with handlers)
bool Player::hitTest(Instance &inst, const Matrix &parent, float x, float y, Instance *&button)
{
	if (!inst.visible || !inst.character) return false;
	if (inst.clipDepth > 0) return false;	// masks are not hit
	const Matrix world = inst.matrix.then(parent);
	switch (inst.character->type)
	{
	case CHAR_SHAPE:
	{
		Matrix inv; if (!world.invert(inv)) return false;
		float lx, ly; inv.apply(x, y, lx, ly);
		const Geometry *g = inst.character->file->getGeometry(inst.character->geometry);
		if (!g || lx < g->minX || lx > g->maxX || ly < g->minY || ly > g->maxY) return false;
		for (const GeometryEntry &e : g->entries)
			if (e.style != GeometryEntry::LINES && pointInTris(e.verts, lx, ly)) return true;
		return false;
	}
	case CHAR_TEXT:
		return false;
	case CHAR_BUTTON:
	{
		Matrix inv; if (!world.invert(inv)) return false;
		float lx, ly; inv.apply(x, y, lx, ly);
		std::vector<float> tris;
		const Character &c = *inst.character;
		for (size_t i = 0; i + 2 < c.hitTris.size(); i += 3)
			for (int k = 0; k < 3; ++k)
			{
				const unsigned vi = c.hitTris[i + k];
				tris.push_back(2 * vi + 1 < c.hitVerts.size() ? c.hitVerts[2 * vi] : 0);
				tris.push_back(2 * vi + 1 < c.hitVerts.size() ? c.hitVerts[2 * vi + 1] : 0);
			}
		if (pointInTris(tris, lx, ly)) { if (!button) button = &inst; return true; }
		return false;
	}
	default:
		break;
	}
	// sprites: topmost child first. Plain shapes do not take the mouse away from buttons below them
	// (Flash picking), they only make the interface "under the cursor".
	bool any = false;
	for (auto it = inst.children.rbegin(); it != inst.children.rend(); ++it)
	{
		if (!it->second) continue;
		if (hitTest(*it->second, world, x, y, button)) any = true;
	}
	if (any && !button && clipHasMouseHandlers(inst)) button = &inst;
	return any;
}

bool Player::mouse(float sx, float sy, bool down, const Matrix &view)
{
	if (!m_root) return false;
	Matrix inv;
	if (!view.invert(inv)) return false;
	float x, y; inv.apply(sx, sy, x, y);
	const bool moved = x != mouseX || y != mouseY;
	mouseX = x; mouseY = y;
	Instance *btn = nullptr;
	const bool hit = hitTest(*m_root, Matrix(), x, y, btn);
	std::shared_ptr<Instance> over = btn ? btn->shared_from_this() : nullptr;
	std::shared_ptr<Instance> hover = m_hover.lock(), pressed = m_pressed.lock();
	static bool s_wasDown = false;
	const bool press = down && !s_wasDown, release = !down && s_wasDown;
	s_wasDown = down;

	auto runButton = [&](const std::shared_ptr<Instance> &b, unsigned char cond, unsigned clipEvent) {
		if (!b) return;
		if (b->character && b->character->type == CHAR_BUTTON)
		{
			std::shared_ptr<Instance> parent = b->parent ? b->parent->shared_from_this() : m_root;
			for (const ButtonAction &a : b->character->actions)
				if (a.flags & cond) queueCode(parent, a.code);
		}
		fireClipEvent(*b, clipEvent);
	};
	enum { IDLE_TO_OVERUP = 1, OVERUP_TO_IDLE = 2, OVERUP_TO_OVERDOWN = 4, OVERDOWN_TO_OVERUP = 8,
		OVERDOWN_TO_OUTDOWN = 16, OUTDOWN_TO_OVERDOWN = 32, IDLE_TO_OVERDOWN = 64, OVERDOWN_TO_IDLE = 128 };

	if (over != hover)
	{
		if (hover && !pressed) { hover->buttonState = 0; runButton(hover, OVERUP_TO_IDLE, CLIPEVENT_ROLLOUT); }
		if (over && !pressed) { over->buttonState = 1; runButton(over, IDLE_TO_OVERUP, CLIPEVENT_ROLLOVER); }
		m_hover = over;
	}
	if (press && over)
	{
		over->buttonState = 2;
		m_pressed = over;
		runButton(over, OVERUP_TO_OVERDOWN, CLIPEVENT_PRESS);
	}
	if (release && pressed)
	{
		if (pressed == over) { pressed->buttonState = 1; runButton(pressed, OVERDOWN_TO_OVERUP, CLIPEVENT_RELEASE); }
		else { pressed->buttonState = 0; runButton(pressed, OVERDOWN_TO_IDLE, CLIPEVENT_RELEASEOUTSIDE); }
		m_pressed.reset();
	}
	// listeners (Mouse.addListener)
	if (press || release || moved)
	{
		const char *h = press ? "onMouseDown" : release ? "onMouseUp" : "onMouseMove";
		std::vector<ObjRef> ls = mouseListeners;
		for (ObjRef &l : ls)
		{
			Value fn = l->get(h);
			std::vector<Value> none;
			if (fn.isObject() && fn.o->kind == Object::FUNCTION) callFunction(fn.o, l, none);
		}
	}
	runPending();
	return hit || pressed;
}

// ------------------------------------------------------------------------------------------------
// rendering
// ------------------------------------------------------------------------------------------------
void Player::render(const Matrix &view)
{
	if (!m_root) return;
	renderBegin();
	renderInstance(*m_root, view, ColorTransform());
	renderEnd();
}

void Player::renderInstance(Instance &inst, const Matrix &parent, const ColorTransform &pcx)
{
	if (!inst.visible || !inst.character) return;
	const Matrix world = inst.matrix.then(parent);
	const ColorTransform cx = inst.color.then(pcx);
	if (cx.mul.a <= 0.001f && cx.add.a <= 0.001f) return;
	if (inst.customDraw) { inst.customDraw(inst, world, cx); return; }
	switch (inst.character->type)
	{
	case CHAR_SHAPE: drawShape(inst, world, cx); return;
	case CHAR_TEXT: drawText(inst, world, cx); return;
	case CHAR_BUTTON:
	{
		const unsigned char bit = inst.buttonState == 2 ? 4 : inst.buttonState == 1 ? 2 : 1;
		for (auto &kv : inst.children)
			if (kv.second && (size_t)kv.first < inst.recordStates.size() && (inst.recordStates[kv.first] & bit))
				renderInstance(*kv.second, world, cx);
		return;
	}
	default: break;
	}
	int maskUntil = -1;
	for (auto &kv : inst.children)
	{
		if (!kv.second) continue;
		if (kv.second->clipDepth > 0)
		{
			// mask layer: not drawn. TODO: stencil clip; content stays unclipped for now
			maskUntil = kv.second->clipDepth;
			continue;
		}
		(void)maskUntil;
		renderInstance(*kv.second, world, cx);
	}
}

static Color applyCx(const Color &c, const ColorTransform &cx)
{
	Color r;
	r.r = std::min(1.0f, std::max(0.0f, c.r * cx.mul.r + cx.add.r));
	r.g = std::min(1.0f, std::max(0.0f, c.g * cx.mul.g + cx.add.g));
	r.b = std::min(1.0f, std::max(0.0f, c.b * cx.mul.b + cx.add.b));
	r.a = std::min(1.0f, std::max(0.0f, c.a * cx.mul.a + cx.add.a));
	return r;
}

void Player::drawShape(Instance &inst, const Matrix &m, const ColorTransform &cx)
{
	File *f = inst.character->file;
	const Geometry *g = f->getGeometry(inst.character->geometry);
	if (!g) return;
	std::vector<float> xy, uv;
	for (const GeometryEntry &e : g->entries)
	{
		const Color col = applyCx(e.color, cx);
		if (col.a <= 0.002f) continue;
		if (e.style == GeometryEntry::LINES)
		{
			// lines as thin quads
			xy.clear();
			for (size_t i = 0; i + 3 < e.verts.size(); i += 4)
			{
				float ax, ay, bx, by;
				m.apply(e.verts[i], e.verts[i + 1], ax, ay);
				m.apply(e.verts[i + 2], e.verts[i + 3], bx, by);
				float dx = bx - ax, dy = by - ay;
				const float len = sqrtf(dx * dx + dy * dy);
				if (len < 1e-4f) continue;
				const float w = std::max(1.0f, e.lineWidth * matScaleX(m)) * 0.5f;
				const float nx = -dy / len * w, ny = dx / len * w;
				const float q[12] = { ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, ax + nx, ay + ny, bx - nx, by - ny, ax - nx, ay - ny };
				xy.insert(xy.end(), q, q + 12);
			}
			if (!xy.empty()) renderTriangles(f, -1, xy.data(), nullptr, (int)xy.size() / 6, col, false);
			continue;
		}
		const size_t n = e.verts.size() / 2;
		xy.resize(n * 2);
		for (size_t i = 0; i < n; ++i) m.apply(e.verts[2 * i], e.verts[2 * i + 1], xy[2 * i], xy[2 * i + 1]);
		if (e.style == GeometryEntry::TEXTURED)
		{
			uv.resize(n * 2);
			for (size_t i = 0; i < n; ++i) e.texMatrix.apply(e.verts[2 * i], e.verts[2 * i + 1], uv[2 * i], uv[2 * i + 1]);	// texture pixels
			renderTriangles(f, e.image, xy.data(), uv.data(), (int)(n / 3), col, true);
		}
		else
			renderTriangles(f, -1, xy.data(), nullptr, (int)(n / 3), col, false);
	}
}

void Player::drawText(Instance &inst, const Matrix &m, const ColorTransform &cx)
{
	const Character &c = *inst.character;
	std::string text;
	if (inst.textSet) text = inst.text;
	else if (!c.variable.empty() && inst.parent)
	{
		Context ctx(*this);
		ctx.target = inst.parent->shared_from_this();
		ctx.self = clipObject(ctx.target);
		Value v = ctx.getVariable(c.variable);
		text = (v.isUndef() || v.isObject()) ? c.content : v.toString();
	}
	else text = c.content;
	if (getenv("BFME_DEBUG_APT_TEXT")) fprintf(stderr, "[apt] text %s var='%s' content='%s' set=%d -> '%s'\n", inst.targetPath().c_str(), c.variable.c_str(), c.content.c_str(), (int)inst.textSet, text.c_str());
	if (text.empty()) return;
	unsigned flags = 0;
	if (text[0] == '$')
	{
		// "$<string id>[&flag...]": localised or game-supplied text; flags: dropshadow
		std::string key = text.substr(1);
		const size_t amp = key.find('&');
		if (amp != std::string::npos)
		{
			if (lower(key.substr(amp)).find("dropshadow") != std::string::npos) flags |= 0x200;
			key = key.substr(0, amp);
		}
		text = m_host ? m_host->translate(key) : key;
		if (text.empty()) return;
	}
	// strip simple html tags (htmlText)
	if (text.find('<') != std::string::npos)
	{
		std::string out; bool tag = false;
		for (char ch : text) { if (ch == '<') tag = true; else if (ch == '>') tag = false; else if (!tag) out += ch; }
		text = out;
	}
	const Color col = applyCx(c.textColor, cx);
	if (col.a <= 0.002f) return;
	float x0, y0, x1, y1;
	m.apply(c.bounds[0], c.bounds[1], x0, y0);
	m.apply(c.bounds[2], c.bounds[3], x1, y1);
	const float size = c.fontHeight * matScaleY(m);
	std::string font;
	if (c.font < c.file->characters.size() && c.file->characters[c.font]) font = c.file->characters[c.font]->fontName;
	renderText(text, font.c_str(), size, col, std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1), c.alignment | (c.multiline || c.wordWrap ? 0x100 : 0) | flags);
}

} // namespace Apt
