/*
**	bfme-native: APT drawing through WW3D's Render2DClass (textured / solid triangles) and the
**	engine's DisplayString text renderer. Textures: art/Textures/apt_<movie>_<texture>.tga
*/
#include "W3DDevice/GameClient/Apt/AptPlayer.h"

#include "Common/FileSystem.h"
#include "Common/file.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/Color.h"
#include "WW3D2/render2d.h"
#include "WW3D2/texture.h"
#include "WW3D2/assetmgr.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace Apt
{

namespace
{
struct TexInfo { TextureClass *tex = nullptr; int w = 0, h = 0; bool tried = false; };
std::map<std::string, TexInfo> s_textures;
Render2DClass *s_render = nullptr;
TextureClass *s_current = nullptr;
bool s_currentTextured = false;
int s_pending = 0;

struct TextEntry { DisplayString *ds = nullptr; unsigned lastUsed = 0; };
std::map<std::string, TextEntry> s_texts;
unsigned s_frame = 0;

bool readHeaderSize(const std::string &path, int &w, int &h)
{
	if (TheFileSystem == nullptr) return false;
	::File *f = TheFileSystem->openFile(path.c_str(), ::File::READ | ::File::BINARY);
	if (getenv("BFME_DEBUG_APT_TEX")) fprintf(stderr, "[apt] header %s: %s\n", path.c_str(), f ? "open" : "no");
	if (!f) return false;
	unsigned char hdr[32];
	const Int n = f->read(hdr, sizeof(hdr));
	f->close();
	if (n < 20) return false;
	if (memcmp(hdr, "DDS ", 4) == 0)
	{
		unsigned hh, ww; memcpy(&hh, hdr + 12, 4); memcpy(&ww, hdr + 16, 4);
		w = (int)ww; h = (int)hh;
		return w > 0 && h > 0;
	}
	w = hdr[12] | (hdr[13] << 8);
	h = hdr[14] | (hdr[15] << 8);
	return w > 0 && h > 0;
}

TexInfo &texture(File *file, int image)
{
	char name[256];
	snprintf(name, sizeof(name), "apt_%s_%d.tga", file->name.c_str(), file->textureFor(image));
	std::string key = lower(name);
	TexInfo &t = s_textures[key];
	if (t.tried) return t;
	t.tried = true;
	const char *dirs[] = { "Art/Textures/", "art/textures/", "Data/Art/Textures/", "" };
	for (const char *d : dirs)
	{
		if (readHeaderSize(std::string(d) + name, t.w, t.h)) break;
		std::string dds = std::string(d) + name;
		dds.replace(dds.size() - 4, 4, ".dds");
		if (readHeaderSize(dds, t.w, t.h)) break;
	}
	WW3DAssetManager *am = WW3DAssetManager::Get_Instance();
	if (am && t.w > 0) t.tex = am->Get_Texture(name);	// not shipped: draw nothing (not the magenta placeholder)
	if (!t.tex || t.w <= 0)
		fprintf(stderr, "[apt] texture %s: %s (%dx%d)\n", name, t.tex ? "loaded" : "missing", t.w, t.h);
	return t;
}

void flush()
{
	if (s_render && s_pending)
	{
		s_render->Render();
		s_render->Reset();
	}
	s_pending = 0;
}

unsigned long argb(const Color &c)
{
	auto b = [](float v) { int i = (int)(v * 255.0f + 0.5f); return (unsigned long)(i < 0 ? 0 : i > 255 ? 255 : i); };
	return (b(c.a) << 24) | (b(c.r) << 16) | (b(c.g) << 8) | b(c.b);
}

void setState(TextureClass *tex, bool textured)
{
	if (s_pending && tex == s_current && textured == s_currentTextured) return;
	flush();
	s_current = tex;
	s_currentTextured = textured;
	s_render->Reset();
	if (textured && tex)
	{
		s_render->Enable_Texturing(true);
		s_render->Set_Texture(tex);
	}
	else
		s_render->Enable_Texturing(false);
	s_render->Enable_Additive(false);
	s_render->Enable_Alpha(true);
}
} // namespace

void renderBegin()
{
	if (!s_render)
		s_render = new Render2DClass();
	const float w = TheDisplay ? (float)TheDisplay->getWidth() : 800.0f;
	const float h = TheDisplay ? (float)TheDisplay->getHeight() : 600.0f;
	s_render->Set_Coordinate_Range(RectClass(0, 0, w, h));
	s_render->Reset();
	s_pending = 0;
	s_current = nullptr;
	++s_frame;
}

void renderTriangles(File *file, int image, const float *xy, const float *uv, int count, const Color &color, bool textured)
{
	if (!s_render || count <= 0) return;
	TextureClass *tex = nullptr;
	float iw = 1, ih = 1;
	if (textured)
	{
		TexInfo &t = texture(file, image);
		tex = t.tex;
		if (!tex) return;
		if (t.w > 0) iw = 1.0f / t.w;
		if (t.h > 0) ih = 1.0f / t.h;
	}
	setState(tex, textured);
	const unsigned long c = argb(color);
	for (int i = 0; i < count; ++i)
	{
		const float *p = xy + 6 * i;
		// Render2DClass culls: screen-space (y down) triangles must wind with a negative signed area
		const bool flip = (p[2] - p[0]) * (p[5] - p[1]) - (p[3] - p[1]) * (p[4] - p[0]) > 0;
		const int i1 = flip ? 2 : 1, i2 = flip ? 1 : 2;
		Vector2 a(p[0], p[1]), b(p[2 * i1], p[2 * i1 + 1]), d(p[2 * i2], p[2 * i2 + 1]);
		if (textured && uv)
		{
			const float *q = uv + 6 * i;
			s_render->Add_Tri(a, b, d, Vector2(q[0] * iw, q[1] * ih), Vector2(q[2 * i1] * iw, q[2 * i1 + 1] * ih), Vector2(q[2 * i2] * iw, q[2 * i2 + 1] * ih), c);
		}
		else
			s_render->Add_Tri(a, b, d, Vector2(0, 0), Vector2(0, 0), Vector2(0, 0), c);
	}
	s_pending += count;
	if (s_pending > 4000) flush();
}

void renderText(const std::string &text, const char *font, float size, const Color &color, float x0, float y0, float x1, float y1, unsigned align)
{
	if (!TheDisplayStringManager || !TheFontLibrary || text.empty()) return;
	flush();
	int pt = (int)(size * 0.75f + 0.5f);	// APT font heights are pixel heights
	if (pt < 6) pt = 6;
	if (pt > 48) pt = 48;
	const std::string fontName = (font && *font) ? font : "Arial";
	char keybuf[64];
	snprintf(keybuf, sizeof(keybuf), "|%s|%d|%d|", fontName.c_str(), pt, (int)(x1 - x0));
	const std::string key = text + keybuf;
	TextEntry &e = s_texts[key];
	if (!e.ds)
	{
		e.ds = TheDisplayStringManager->newDisplayString();
		e.ds->setFont(TheFontLibrary->getFont(AsciiString(fontName.c_str()), pt, FALSE));
		UnicodeString u;
		u.translate(AsciiString(text.c_str()));
		e.ds->setText(u);
		if (align & 0x100) e.ds->setWordWrap((Int)(x1 - x0));
	}
	e.lastUsed = s_frame;
	Int w = 0, h = 0;
	e.ds->getSize(&w, &h);
	float x = x0;
	switch (align & 3)
	{
	case 1: x = x1 - w; break;			// right
	case 2: x = (x0 + x1 - w) * 0.5f; break;	// centre
	default: break;
	}
	const float y = y0 + ((y1 - y0) - h) * 0.5f;
	auto b = [](float v) { int i = (int)(v * 255.0f + 0.5f); return (UnsignedByte)(i < 0 ? 0 : i > 255 ? 255 : i); };
	e.ds->draw((Int)x, (Int)(y > y0 ? y : y0), GameMakeColor(b(color.r), b(color.g), b(color.b), b(color.a)), GameMakeColor(0, 0, 0, b(color.a * 0.6f)));
	// resume triangles with whatever state comes next
	s_current = nullptr;
}

void renderEnd()
{
	flush();
	// drop text that has not been drawn for a while
	for (auto it = s_texts.begin(); it != s_texts.end();)
	{
		if (s_frame - it->second.lastUsed > 120)
		{
			if (it->second.ds && TheDisplayStringManager) TheDisplayStringManager->freeDisplayString(it->second.ds);
			it = s_texts.erase(it);
		}
		else ++it;
	}
}

} // namespace Apt
