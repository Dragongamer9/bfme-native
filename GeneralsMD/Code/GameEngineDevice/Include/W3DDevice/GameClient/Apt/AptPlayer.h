/*
**	bfme-native: APT runtime -- movie clips, timeline, ActionScript (AVM1 + EA extensions), rendering.
**
**	Semantics follow Flash 5/6 (AVM1): case-insensitive identifiers, prototype objects, the
**	display list rebuilt on backward gotos, frame actions run after the frame's display changes.
**	EA's APT bytecode is AVM1 with 4-byte aligned operands, pointer-sized constant references and a
**	few shorthand opcodes (see AptVM.cpp).
*/
#pragma once

#include "W3DDevice/GameClient/Apt/AptData.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Apt
{

struct Object;
struct Instance;
class Player;
typedef std::shared_ptr<Object> ObjRef;

struct Value
{
	enum Type { T_UNDEFINED, T_NULL, T_BOOLEAN, T_NUMBER, T_STRING, T_OBJECT } type = T_UNDEFINED;
	bool b = false;
	double n = 0;
	std::string s;
	ObjRef o;

	Value() {}
	static Value undef() { return Value(); }
	static Value null() { Value v; v.type = T_NULL; return v; }
	static Value boolean(bool x) { Value v; v.type = T_BOOLEAN; v.b = x; return v; }
	static Value number(double x) { Value v; v.type = T_NUMBER; v.n = x; return v; }
	static Value string(const std::string &x) { Value v; v.type = T_STRING; v.s = x; return v; }
	static Value object(const ObjRef &x) { Value v; if (x) { v.type = T_OBJECT; v.o = x; } return v; }

	bool isUndef() const { return type == T_UNDEFINED; }
	bool isObject() const { return type == T_OBJECT && o != nullptr; }
	double toNumber() const;
	int toInt() const;
	bool toBool() const;
	std::string toString() const;
};

class Context;
typedef std::function<Value(Context &ctx, const ObjRef &self, std::vector<Value> &args)> NativeFn;

struct Object : public std::enable_shared_from_this<Object>
{
	enum Kind { PLAIN, FUNCTION, ARRAY, CLIP, STRING_OBJ, COLOR } kind = PLAIN;
	struct Prop { std::string name; Value value; bool dontEnum = false; };
	std::unordered_map<std::string, Prop> props;	///< key: lower-case name
	std::vector<std::string> order;					///< enumeration order (lower-case keys)
	ObjRef proto;

	virtual ~Object() {}
	virtual bool getOwn(const std::string &key, const std::string &name, Value &out);
	virtual bool setOwn(const std::string &key, const std::string &name, const Value &v);
	Value get(const std::string &name);
	void set(const std::string &name, const Value &v, bool dontEnum = false);
	bool has(const std::string &name);
	bool remove(const std::string &name);
	virtual void keys(std::vector<std::string> &out);

	// function
	NativeFn native;
	Code body;
	unsigned bodySize = 0;
	std::vector<std::pair<int, std::string>> params;	///< (register or 0, name)
	unsigned char numRegisters = 0;
	unsigned flags2 = 0;
	bool isFunction2 = false;
	std::vector<Value> pool;							///< constant pool when the function was defined
	std::vector<ObjRef> scope;						///< scope chain captured at definition
	std::weak_ptr<Instance> target;					///< movie clip the function was defined in

	// array
	std::vector<Value> items;
	// string object
	std::string str;
	// clip
	std::weak_ptr<Instance> clip;
	// Color object target
	std::weak_ptr<Instance> colorTarget;
};

/// display list entry: shape / sprite (movie clip) / button / text / movie root
struct Instance : public std::enable_shared_from_this<Instance>
{
	Character *character = nullptr;
	File *file = nullptr;				///< the movie whose characters this instance's timeline uses
	Player *player = nullptr;
	Instance *parent = nullptr;
	int depth = 0;
	std::string name;
	Matrix matrix;
	ColorTransform color;
	bool visible = true;
	bool placedByTimeline = true;
	bool scriptTouched = false;		///< script changed matrix/colour: the timeline stops moving it
	unsigned bornTick = 0;
	int clipDepth = 0;
	float ratio = 0;

	// sprite
	std::map<int, std::shared_ptr<Instance>> children;
	int currentFrame = -1;				///< 0-based; -1 before the first frame ran
	bool playing = true;
	ObjRef object;						///< the MovieClip script object (sprites, movies, buttons, texts)
	std::vector<ClipEvent> clipEvents;
	bool loaded = false;

	// button
	int buttonState = 0;				///< 0 up, 1 over, 2 down
	std::vector<unsigned char> recordStates;	///< per child (key = record index): state bits

	// text
	std::string text;
	bool textSet = false;

	// game hook: draw something else in place of this shape
	std::function<void(Instance &self, const Matrix &world, const ColorTransform &cx)> customDraw;
	/// game hook: draw something on top of this instance after it rendered normally
	std::function<void(Instance &self, const Matrix &world, const ColorTransform &cx)> afterDraw;

	bool isSprite() const { return character && (character->type == CHAR_SPRITE || character->type == CHAR_MOVIE); }
	int totalFrames() const { return isSprite() ? (int)character->frames.size() : 1; }
	Instance *findChild(const std::string &name);
	std::string targetPath() const;
	Matrix worldMatrix() const;
	ColorTransform worldColor() const;
	bool bounds(float &x0, float &y0, float &x1, float &y1, const Matrix &m) const;
};

/// host interface: how movies talk to the game
class Host
{
public:
	virtual ~Host() {}
	/// getURL("FSCommand:<cmd>", args) / fscommand(cmd, args)
	virtual void onFSCommand(Player &player, const std::string &cmd, const std::string &args) {}
	/// calls on the game object (e.g. _root.GameCode.OnInitialized(...)); return value goes back to script
	virtual Value onGameCall(Player &player, const std::string &object, const std::string &method, std::vector<Value> &args) { return Value(); }
	/// loadMovie("<file>.apt", target): the host may veto or redirect
	virtual bool allowLoadMovie(Player &player, const std::string &movie) { return true; }
	/// the global 'extern' object: "Class::Variable" values owned by the game
	virtual Value getExtern(Player &player, const std::string &name) { return Value(); }
	virtual void setExtern(Player &player, const std::string &name, const Value &v) {}
	/// "$KEY" strings (localised text)
	virtual std::string translate(const std::string &key) { return key; }
};

/// execution state for one action block or function call
class Context
{
public:
	Player &player;
	ObjRef self;					///< 'this'
	std::shared_ptr<Instance> target;	///< current target clip (SetTarget changes it)
	std::shared_ptr<Instance> origTarget;	///< SetTarget("") goes back here
	ObjRef origSelf;
	std::vector<ObjRef> scope;		///< innermost last
	ObjRef locals;					///< activation object (functions), else null
	std::vector<Value> registers;
	std::vector<Value> pool;
	std::vector<Value> stack;
	File *file = nullptr;
	int depth = 0;
	Value returnValue;
	bool returned = false;

	Context(Player &p) : player(p) {}
	Value pop() { if (stack.empty()) return Value(); Value v = stack.back(); stack.pop_back(); return v; }
	void push(const Value &v) { stack.push_back(v); }
	Value getVariable(const std::string &name);
	void setVariable(const std::string &name, const Value &v);
	void defineLocal(const std::string &name, const Value &v);
	Value resolvePath(const std::string &path, bool &found);
};

class Player
{
public:
	Player();
	~Player();

	bool load(const std::string &movieName);
	void setHost(Host *host) { m_host = host; }
	Host *host() { return m_host; }

	/// advance by elapsed milliseconds (frames run at the movie's own rate)
	void update(unsigned elapsedMs);
	/// render with movie units mapped by 'view' (movie -> screen pixels)
	void render(const Matrix &view);
	/// mouse in screen pixels; returns true when the UI consumed the event
	bool mouse(float x, float y, bool down, const Matrix &view);

	std::shared_ptr<Instance> root() { return m_root; }
	/// the button (or clip with mouse handlers) under the cursor
	std::shared_ptr<Instance> hovered() { return m_hover.lock(); }
	File *file() { return m_file; }
	ObjRef global() { return m_global; }

	/// script helpers for the game
	Value call(const std::string &path, std::vector<Value> args);
	Value get(const std::string &path);
	void set(const std::string &path, const Value &v);
	std::shared_ptr<Instance> find(const std::string &path);

	// runtime services used by the VM / builtins
	void execute(const Code &code, const std::shared_ptr<Instance> &target, const std::vector<Value> *pool = nullptr);
	Value callFunction(const ObjRef &fn, const ObjRef &self, std::vector<Value> &args, int depth = 0);
	ObjRef makeObject(const ObjRef &proto = nullptr);
	ObjRef makeArray(const std::vector<Value> &items);
	ObjRef makeNative(const NativeFn &fn);
	ObjRef clipObject(const std::shared_ptr<Instance> &inst);
	std::shared_ptr<Instance> instanceOf(const Value &v);
	std::shared_ptr<Instance> createInstance(Character *c, File *file, Instance *parent, int depth);
	void gotoFrame(Instance &clip, int frame, bool play);
	bool gotoLabel(Instance &clip, const std::string &label, bool play);
	std::shared_ptr<Instance> loadMovieInto(Instance &target, const std::string &movie);
	void queueActions(Instance &clip, int frame);
	size_t pendingCount() const { return m_pendingActions.size(); }
	void runPendingFrom(size_t first);
	void queueCode(const std::shared_ptr<Instance> &clip, const Code &code);
	void queueHandler(const std::shared_ptr<Instance> &clip, const std::string &handler);
	void fireClipEvent(Instance &inst, unsigned flag);
	std::string trace;

	ObjRef objectProto, functionProto, arrayProto, clipProto, stringProto, colorProto, textProto, externObj;
	std::vector<ObjRef> mouseListeners;
	float mouseX = 0, mouseY = 0;		///< movie coordinates
	unsigned timeMs = 0;
	struct Pending { std::weak_ptr<Instance> clip; Code code; std::string handler; };
	std::vector<Pending> m_pendingActions;
	struct Interval { int id; ObjRef fn; ObjRef self; std::string method; std::vector<Value> args; unsigned periodMs, nextMs; };
	std::vector<Interval> intervals;
	int nextIntervalId = 1;

private:
	void installBuiltins();
	void advance(Instance &inst);
	void applyFrame(Instance &clip, int frame, bool runActions);
	void rebuildTo(Instance &clip, int frame);
	void placeItem(Instance &clip, const FrameItem &it);
	void renderInstance(Instance &inst, const Matrix &parent, const ColorTransform &pcx);
	void drawShape(Instance &inst, const Matrix &m, const ColorTransform &cx);
	void drawText(Instance &inst, const Matrix &m, const ColorTransform &cx);
	bool hitTest(Instance &inst, const Matrix &parent, float x, float y, Instance *&button);
	void runPending();

	Host *m_host = nullptr;
	File *m_file = nullptr;
	std::shared_ptr<Instance> m_root;
	ObjRef m_global;
	unsigned m_accum = 0;
	unsigned m_tick = 1;
	std::weak_ptr<Instance> m_pressed, m_hover;
	std::vector<const unsigned char *> m_initActionsDone;
	void initInstance(const std::shared_ptr<Instance> &inst);
	void removeInstance(Instance &parent, int depth);
	friend class Context;
};

/// the VM entry point (AptVM.cpp)
void runCode(Context &ctx, const Code &code, unsigned endOffset = 0);

/// drawing backend (AptRender.cpp)
struct DrawBatch;
void renderBegin();
void renderTriangles(File *file, int image, const float *xy, const float *uv, int count, const Color &color, bool textured);
void renderText(const std::string &text, const char *font, float size, const Color &color, float x0, float y0, float x1, float y1, unsigned align);
void renderEnd();
/// draw everything batched so far (before the game draws into the UI directly)
void renderFlush();

} // namespace Apt
