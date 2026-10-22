/*
**	bfme-native: the in-game APT interface (palantir, minimap, command buttons ...).
**
**	Debug / test switches:
**	  BFME_APT=<movie>          movie to show (default: Palantir in game)
**	  BFME_NO_APT               disable
**	  BFME_APT_CALLS=a.b(x,y);c()  script calls after loading (string args; numbers when numeric)
**	  BFME_APT_FIXEDSTEP=<ms>   advance by a fixed step per drawn frame (render tests)
**	  BFME_DEBUG_APT            log script calls / fscommands / missing members
*/
#include "W3DDevice/GameClient/Apt/AptPlayer.h"

#include "GameClient/Display.h"
#include "GameClient/GameText.h"
#include "GameLogic/GameLogic.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Money.h"
#include "Common/Radar.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/View.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Drawable.h"
#include "GameClient/Image.h"
#include "GameLogic/Object.h"
#include "Common/ThingTemplate.h"

#include <map>
#include <vector>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern Bool (*g_bfmeAptMouseHook)(Int kind, Int x, Int y);

namespace BfmeApt
{

void shutdown();

static Apt::Player *s_player = nullptr;

class GameHost : public Apt::Host
{
public:
	std::map<std::string, std::string> dynamicText;	///< lower-case id -> text (game-supplied strings)
	std::vector<std::string> deferred;			///< script calls to make after the current update

	/// palantir command ring: CommandSet slots 1..6
	struct Slot
	{
		std::string frame;		///< MovieClipFrame path (from OnButtonFrameLoaded)
		std::string content;	///< CommandButton content path
		bool ready = false;		///< content reported _OnInitialized
		const CommandButton *button = nullptr;
		std::string state;
	};
	enum { NUM_SLOTS = 6 };
	Slot slots[NUM_SLOTS];
	bool commandShown = false;
	DrawableID source = INVALID_DRAWABLE_ID;

	static std::string arg(const std::string &args, const char *key)
	{
		const std::string k = std::string(key) + "=";
		size_t p = args.find(k);
		if (p == std::string::npos) return std::string();
		p += k.size();
		const size_t e = args.find('&', p);
		return args.substr(p, e == std::string::npos ? std::string::npos : e - p);
	}

	void onFSCommand(Apt::Player &, const std::string &cmd, const std::string &args) override
	{
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] fscommand %s %s\n", cmd.c_str(), args.c_str());
		const std::string c = Apt::lower(cmd);
		if (c == "palantircommandui::onbuttonframeloaded")
		{
			const int i = atoi(arg(args, "index").c_str());
			if (i >= 0 && i < NUM_SLOTS)
			{
				Slot &sl = slots[i];
				sl = Slot();
				sl.frame = arg(args, "name");
				sl.content = sl.frame + ".Button";
				deferred.push_back(sl.frame + ".CreateContent(CommandButton,Button)");
			}
			return;
		}
		for (int i = 0; i < NUM_SLOTS; ++i)
		{
			Slot &sl = slots[i];
			if (sl.content.empty() || c.compare(0, sl.content.size(), Apt::lower(sl.content)) != 0) continue;
			const std::string what = c.substr(sl.content.size());
			if (what == "_oninitialized") { sl.ready = true; sl.state.clear(); }
			else if (what == "_onpress") pressSlot(i);
			return;
		}
		// sub-movies report in; the game decides what is visible
		if (c == "aptpalantir::onhelpboxloaded") deferred.push_back("helpBox.Hide()");
		else if (c == "aptpalantir::onplanningmodeuiloaded") deferred.push_back("planningModeUI.Close()");
		else if (c == "aptpalantir::onheroselectloaded") deferred.push_back("FactionFrameUI.Hide()");
		else if (c == "onaptingamesidecommandbarloaded") deferred.push_back("SideCommandBar.FadeOut()");
		else if (c == "aptpalantir::oninitialized")
		{
			deferred.push_back(std::string("SetPalantirFrameState(") + (localIsGood() ? "_good" : "_evil") + ")");
			deferred.push_back("HideCommandInterface()");
			commandShown = false;
		}
	}
	Apt::Value onGameCall(Apt::Player &, const std::string &object, const std::string &method, std::vector<Apt::Value> &args) override
	{
		if (getenv("BFME_DEBUG_APT"))
		{
			std::string a;
			for (const Apt::Value &v : args) a += (a.empty() ? "" : ", ") + v.toString();
			fprintf(stderr, "[aptui] %s.%s(%s)\n", object.c_str(), method.c_str(), a.c_str());
		}
		return Apt::Value();
	}
	Apt::Value getExtern(Apt::Player &, const std::string &name) override
	{
		const std::string n = Apt::lower(name);
		if (n == "ingame") return Apt::Value::boolean(true);
		if (n == "minlod" || n == "palantirminlod") return Apt::Value::boolean(false);
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] extern get %s\n", name.c_str());
		return Apt::Value();
	}
	void setExtern(Apt::Player &, const std::string &name, const Apt::Value &v) override
	{
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] extern set %s = %s\n", name.c_str(), v.toString().c_str());
	}
	std::string translate(const std::string &key) override
	{
		auto d = dynamicText.find(Apt::lower(key));
		if (d != dynamicText.end()) return d->second;
		if (TheGameText)
		{
			Bool exists = FALSE;
			const std::string label = "APT:" + key;
			UnicodeString u = TheGameText->fetch(label.c_str(), &exists);
			if (exists)
			{
				AsciiString a; a.translate(u);
				return a.str();
			}
		}
		// "_level0.Movie_Thing" ids are filled by the game; nothing yet
		if (key.find('.') != std::string::npos || (!key.empty() && key[0] == '_')) return std::string();
		return key;
	}

	static bool localIsGood()
	{
		Player *p = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (!p) return true;
		const std::string side = Apt::lower(p->getSide().str());
		return !(side.find("mordor") != std::string::npos || side.find("isengard") != std::string::npos ||
			side.find("wild") != std::string::npos || side.find("angmar") != std::string::npos);
	}

	void pressSlot(int i)
	{
		const CommandButton *b = slots[i].button;
		Drawable *d = TheGameClient ? TheGameClient->findDrawableByID(source) : nullptr;
		if (!b || !TheControlBar) return;
		const Bool multi = TheInGameUI && TheInGameUI->getSelectCount() > 1;
		const CBCommandStatus st = TheControlBar->bfmeExecuteCommand(b, d, multi);
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] command %s -> %d\n", b->getName().str(), (int)st);
	}

	/// the first selected object of ours drives the command ring
	void refreshCommands(Apt::Player &player)
	{
		Player *local = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		Drawable *d = nullptr;
		if (TheInGameUI && local)
		{
			const DrawableList *sel = TheInGameUI->getAllSelectedDrawables();
			for (Drawable *x : *sel)
				if (x && x->getObject() && x->getObject()->getControllingPlayer() == local) { d = x; break; }
		}
		Object *obj = d ? d->getObject() : nullptr;
		source = d ? d->getID() : INVALID_DRAWABLE_ID;
		const CommandSet *cs = (obj && TheControlBar) ? TheControlBar->findCommandSet(obj->getCommandSetString()) : nullptr;
		static DrawableID s_lastSource = INVALID_DRAWABLE_ID;
		if (source != s_lastSource && getenv("BFME_DEBUG_APT"))
		{
			s_lastSource = source;
			fprintf(stderr, "[aptui] selection %s commandset '%s' -> %s\n", obj ? obj->getTemplate()->getName().str() : "-",
				obj ? obj->getCommandSetString().str() : "", cs ? "found" : "none");
		}
		bool any = false;
		for (int i = 0; i < NUM_SLOTS; ++i)
		{
			Slot &sl = slots[i];
			sl.button = cs ? cs->getCommandButton(i) : nullptr;
			std::string state = "_unused";
			if (sl.button)
			{
				const CommandAvailability av = TheControlBar->getCommandAvailability(sl.button, obj, nullptr);
				if (av == COMMAND_HIDDEN) sl.button = nullptr;
				else
				{
					any = true;
					state = (av == COMMAND_AVAILABLE || av == COMMAND_ACTIVE) ? "_up" : (av == COMMAND_CANT_AFFORD ? "_cantAfford" : "_disabled");
				}
			}
			if (getenv("BFME_DEBUG_APT") && state != sl.state)
				fprintf(stderr, "[aptui] slot %d %s ready=%d state %s\n", i, sl.button ? sl.button->getName().str() : "-", (int)sl.ready, state.c_str());
			if (sl.ready && state != sl.state)
			{
				// keep _over/_down animations: only push the base state when it changes
				sl.state = state;
				std::vector<Apt::Value> a = { Apt::Value::string(state) };
				player.call(sl.content + ".SetState", a);
			}
			if (sl.ready)
				attachImage(player, i);
		}
		if (any != commandShown)
		{
			commandShown = any;
			player.call(any ? "ShowCommandInterface" : "HideCommandInterface", {});
		}
	}

	/// the game draws the button picture into the content's RenderImage clip
	void attachImage(Apt::Player &player, int i)
	{
		std::shared_ptr<Apt::Instance> content = player.find(slots[i].content);
		if (!content) return;
		static Apt::Character *s_renderImage = nullptr;
		if (!s_renderImage)
		{
			Apt::File *lib = Apt::library().load("libInGameUI");
			s_renderImage = lib ? lib->exported("RenderImage") : nullptr;
			if (!s_renderImage) return;
		}
		std::vector<Apt::Instance *> stack = { content.get() };
		while (!stack.empty())
		{
			Apt::Instance *n = stack.back(); stack.pop_back();
			if (n->character == s_renderImage)
			{
				if (!n->customDraw)
				{
					GameHost *self = this;
					n->customDraw = [self, i](Apt::Instance &inst, const Apt::Matrix &world, const Apt::ColorTransform &cx) {
						const CommandButton *b = self->slots[i].button;
						const Image *img = b ? b->getButtonImage() : nullptr;
						float x0, y0, x1, y1;
						const bool hasBounds = inst.bounds(x0, y0, x1, y1, world);
						if (getenv("BFME_DEBUG_APT_IMG"))
							fprintf(stderr, "[aptui] image slot %d %s img=%s rect=(%.0f,%.0f)-(%.0f,%.0f) a=%.2f\n", i, b ? b->getName().str() : "-",
								img ? img->getName().str() : "null", hasBounds ? x0 : -1, hasBounds ? y0 : -1, x1, y1, cx.mul.a);
						if (!img || !TheDisplay || !hasBounds) return;
						Apt::renderFlush();
						TheDisplay->drawImage(img, (Int)x0, (Int)y0, (Int)x1, (Int)y1, GameMakeColor(255, 255, 255, (UnsignedByte)(255 * (cx.mul.a < 1 ? cx.mul.a : 1))));
					};
				}
				return;
			}
			for (auto &kv : n->children) if (kv.second) stack.push_back(kv.second.get());
		}
	}

	/// per-frame game values shown by the palantir
	void refresh()
	{
		Player *p = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
		if (!p) return;
		char buf[64];
		snprintf(buf, sizeof(buf), "%u", (unsigned)p->getMoney()->countMoney());
		dynamicText["palantirresources"] = buf;
		const Int used = p->bfmeCommandPointsUsed(), limit = p->bfmeCommandPointLimit();
		if (limit > 0) snprintf(buf, sizeof(buf), "%d/%d", used, limit);
		else snprintf(buf, sizeof(buf), "%d", used);
		dynamicText["palantircommandpoints"] = buf;
		dynamicText["palantirresourcemultiplier"] = "";
		snprintf(buf, sizeof(buf), "%d", 1);
		dynamicText["playerrank"] = buf;
	}
};

static GameHost s_host;
static Int s_radarRect[4] = { 0, 0, 0, 0 };
static bool s_draggingRadar = false;
static bool s_tried = false;
static std::chrono::steady_clock::time_point s_last;

static void runCalls(const char *spec)
{
	std::string s = spec;
	size_t pos = 0;
	while (pos < s.size())
	{
		size_t end = s.find(';', pos);
		if (end == std::string::npos) end = s.size();
		std::string call = s.substr(pos, end - pos);
		pos = end + 1;
		const size_t lp = call.find('('), rp = call.rfind(')');
		std::string path = call.substr(0, lp);
		std::vector<Apt::Value> args;
		if (lp != std::string::npos && rp != std::string::npos && rp > lp + 1)
		{
			std::string list = call.substr(lp + 1, rp - lp - 1);
			size_t a = 0;
			while (true)
			{
				size_t b = list.find(',', a);
				std::string tok = list.substr(a, b == std::string::npos ? std::string::npos : b - a);
				char *e = nullptr;
				const double d = strtod(tok.c_str(), &e);
				if (!tok.empty() && e && *e == 0) args.push_back(Apt::Value::number(d));
				else if (tok == "true" || tok == "false") args.push_back(Apt::Value::boolean(tok == "true"));
				else args.push_back(Apt::Value::string(tok));
				if (b == std::string::npos) break;
				a = b + 1;
			}
		}
		Apt::Value r = s_player->call(path, args);
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] call %s -> %s\n", call.c_str(), r.toString().c_str());
	}
}

static Apt::Matrix viewMatrix()
{
	Apt::Matrix m;
	Apt::File *f = s_player ? s_player->file() : nullptr;
	const float mw = f && f->width ? (float)f->width : 1024.0f, mh = f && f->height ? (float)f->height : 768.0f;
	const float sw = TheDisplay ? (float)TheDisplay->getWidth() : mw, sh = TheDisplay ? (float)TheDisplay->getHeight() : mh;
	m.m11 = sw / mw;
	m.m22 = sh / mh;
	return m;
}

static void dumpTree(Apt::Instance &inst, int depth, const Apt::Matrix &parent)
{
	if (depth > 5) return;
	const Apt::Matrix w = inst.matrix.then(parent);
	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	inst.bounds(x0, y0, x1, y1, w);
	fprintf(stderr, "[apttree] %*s%s [%d] type=%d vis=%d frame=%d/%d bounds=(%.0f,%.0f)-(%.0f,%.0f)\n", depth * 2, "", inst.name.empty() ? "-" : inst.name.c_str(), inst.depth,
		inst.character ? (int)inst.character->type : -1, (int)inst.visible, inst.currentFrame, inst.totalFrames(), x0, y0, x1, y1);
	for (auto &kv : inst.children) if (kv.second) dumpTree(*kv.second, depth + 1, w);
}

static void dumpFunctions(Apt::Instance &inst, int depth)
{
	if (depth > 6) return;
	if (inst.object)
	{
		std::string fns;
		for (const std::string &k : inst.object->order)
		{
			auto it = inst.object->props.find(k);
			if (it != inst.object->props.end() && it->second.value.isObject() && it->second.value.o->kind == Apt::Object::FUNCTION && !it->second.value.o->native)
				fns += " " + it->second.name;
		}
		if (!fns.empty()) fprintf(stderr, "[aptui] %s:%s\n", inst.targetPath().c_str(), fns.c_str());
	}
	for (auto &kv : inst.children) if (kv.second) dumpFunctions(*kv.second, depth + 1);
}

void draw()
{
	if (getenv("BFME_NO_APT")) return;
	const bool inGame = TheGameLogic && TheGameLogic->isInGame() && !TheGameLogic->isInShellGame();
	if (!inGame)
	{
		if (s_player) shutdown();
		return;
	}
	if (!s_tried)
	{
		const char *movie = getenv("BFME_APT");
		if (!movie) movie = "Palantir";
		s_tried = true;
		s_player = new Apt::Player();
		s_player->setHost(&s_host);
		// movies run test code (fake heroes, factions ...) unless they know they are in the game
		s_player->global()->set("InGame", Apt::Value::boolean(true));
		s_player->global()->set("MinLOD", Apt::Value::boolean(false));
		if (!s_player->load(movie))
		{
			delete s_player;
			s_player = nullptr;
			return;
		}
		fprintf(stderr, "[aptui] loaded %s (%ux%u, %u ms/frame)\n", movie, s_player->file()->width, s_player->file()->height, s_player->file()->msPerFrame);
		if (getenv("BFME_APT_CALLS")) runCalls(getenv("BFME_APT_CALLS"));
		s_last = std::chrono::steady_clock::now();
	}
	if (!s_player) return;
	unsigned ms;
	if (getenv("BFME_APT_FIXEDSTEP")) ms = (unsigned)atoi(getenv("BFME_APT_FIXEDSTEP"));
	else
	{
		auto now = std::chrono::steady_clock::now();
		ms = (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last).count();
		s_last = now;
		if (ms > 250) ms = 250;
	}
	s_host.refresh();
	s_host.refreshCommands(*s_player);
	// the in-palantir video panel is only shown while a movie plays (none yet)
	if (std::shared_ptr<Apt::Instance> mp = s_player->find("MoviePlayback"))
		if (!mp->customDraw)
			mp->customDraw = [](Apt::Instance &, const Apt::Matrix &, const Apt::ColorTransform &) {};
	// the minimap draws in place of the palantir's radar placeholder
	if (std::shared_ptr<Apt::Instance> radar = s_player->find("Radar"))
		if (!radar->customDraw)
			radar->customDraw = [](Apt::Instance &self, const Apt::Matrix &world, const Apt::ColorTransform &) {
				float x0, y0, x1, y1;
				if (!self.bounds(x0, y0, x1, y1, world) || !TheRadar) return;
				Apt::renderFlush();
				s_radarRect[0] = (Int)x0; s_radarRect[1] = (Int)y0; s_radarRect[2] = (Int)(x1 - x0); s_radarRect[3] = (Int)(y1 - y0);
				TheRadar->draw(s_radarRect[0], s_radarRect[1], s_radarRect[2], s_radarRect[3]);
			};
	s_player->update(ms);
	for (size_t i = 0; i < s_host.deferred.size() && i < 64; ++i)
	{
		const std::string call = s_host.deferred[i];
		runCalls(call.c_str());
	}
	s_host.deferred.clear();
	static int s_frames = 0;
	if (++s_frames == 30 && getenv("BFME_APT_DUMP")) dumpFunctions(*s_player->root(), 0);
	if (getenv("BFME_APT_TREE") && s_frames == std::max(1, atoi(getenv("BFME_APT_TREE")))) dumpTree(*s_player->root(), 0, Apt::Matrix());
	s_player->render(viewMatrix());
}

/// minimap click: centre the tactical view on that spot
static bool radarClick(Int x, Int y)
{
	const Int rx = s_radarRect[0], ry = s_radarRect[1], rw = s_radarRect[2], rh = s_radarRect[3];
	if (rw <= 0 || rh <= 0 || x < rx || y < ry || x >= rx + rw || y >= ry + rh || !TheTerrainLogic || !TheTacticalView) return false;
	Region3D ext;
	TheTerrainLogic->getExtent(&ext);
	const Real mw = ext.width(), mh = ext.height();
	if (mw <= 0 || mh <= 0) return false;
	// same aspect fit as Radar::findDrawPositions
	Real dw = (Real)rw, dh = (Real)rh;
	if (mw / rw >= mh / rh) dh = rw * mh / mw; else dw = rh * mw / mh;
	const Real ulx = rx + (rw - dw) / 2, uly = ry + (rh - dh) / 2;
	const Real u = (x - ulx) / dw, v = (y - uly) / dh;
	if (u < 0 || u > 1 || v < 0 || v > 1) return true;	// letterbox: still the minimap
	Coord3D world;
	world.x = ext.lo.x + u * mw;
	world.y = ext.lo.y + (1.0f - v) * mh;	// radar rows run north to south
	world.z = 0;
	TheTacticalView->lookAt(&world);
	return true;
}

static Bool mouseHook(Int kind, Int x, Int y)
{
	if (!s_player) return FALSE;
	static bool s_left = false;
	if (kind == 1) s_left = true;
	else if (kind == 2) s_left = false;
	bool used = s_player->mouse((float)x, (float)y, s_left, viewMatrix());
	if (getenv("BFME_DEBUG_APT_MOUSE")) fprintf(stderr, "[aptui] mouse kind=%d %d,%d -> %d\n", (int)kind, (int)x, (int)y, (int)used);
	if ((kind == 1 || (kind == 0 && s_left && s_draggingRadar)) && radarClick(x, y))
	{
		s_draggingRadar = true;
		used = true;
	}
	if (kind == 2) s_draggingRadar = false;
	return used ? TRUE : FALSE;
}

bool mouse(int x, int y, bool down)
{
	if (!s_player) return false;
	return s_player->mouse((float)x, (float)y, down, viewMatrix());
}

void install()
{
	g_bfmeAptMouseHook = &mouseHook;
}

void shutdown()
{
	delete s_player;
	s_player = nullptr;
	s_tried = false;
}

} // namespace BfmeApt
