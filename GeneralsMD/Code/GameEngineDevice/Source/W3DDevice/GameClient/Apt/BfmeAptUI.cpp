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

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace BfmeApt
{

class GameHost : public Apt::Host
{
public:
	void onFSCommand(Apt::Player &, const std::string &cmd, const std::string &args) override
	{
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] fscommand %s %s\n", cmd.c_str(), args.c_str());
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
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] extern get %s\n", name.c_str());
		return Apt::Value();
	}
	void setExtern(Apt::Player &, const std::string &name, const Apt::Value &v) override
	{
		if (getenv("BFME_DEBUG_APT")) fprintf(stderr, "[aptui] extern set %s = %s\n", name.c_str(), v.toString().c_str());
	}
	std::string translate(const std::string &key) override
	{
		if (!TheGameText) return key;
		for (const char *prefix : { "APT:", "" })
		{
			Bool exists = FALSE;
			const std::string label = std::string(prefix) + key;
			UnicodeString u = TheGameText->fetch(label.c_str(), &exists);
			if (exists)
			{
				AsciiString a; a.translate(u);
				return a.str();
			}
		}
		return key;
	}
};

static GameHost s_host;
static Apt::Player *s_player = nullptr;
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
		fprintf(stderr, "[aptui] call %s -> %s\n", call.c_str(), r.toString().c_str());
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

void draw()
{
	if (getenv("BFME_NO_APT")) return;
	if (!s_tried)
	{
		const char *movie = getenv("BFME_APT");
		if (!movie) return;	// opt-in until the game bridge is complete
		s_tried = true;
		s_player = new Apt::Player();
		s_player->setHost(&s_host);
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
	s_player->update(ms);
	s_player->render(viewMatrix());
}

bool mouse(int x, int y, bool down)
{
	if (!s_player) return false;
	return s_player->mouse((float)x, (float)y, down, viewMatrix());
}

void shutdown()
{
	delete s_player;
	s_player = nullptr;
	s_tried = false;
}

} // namespace BfmeApt
