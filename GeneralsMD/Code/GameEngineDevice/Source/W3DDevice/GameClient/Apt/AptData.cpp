/*
**	bfme-native: APT movie loader. See AptData.h for the format overview.
*/
#include "W3DDevice/GameClient/Apt/AptData.h"

#include "Common/FileSystem.h"
#include "Common/file.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>

namespace Apt
{

static const unsigned SIGNATURE = 0x09876543;

std::string lower(const std::string &s)
{
	std::string r(s);
	for (char &c : r) c = (char)tolower((unsigned char)c);
	return r;
}

bool Matrix::invert(Matrix &out) const
{
	const float det = m11 * m22 - m12 * m21;
	if (det > -1e-12f && det < 1e-12f)
		return false;
	const float id = 1.0f / det;
	out.m11 = m22 * id; out.m12 = -m12 * id;
	out.m21 = -m21 * id; out.m22 = m11 * id;
	out.tx = -(tx * out.m11 + ty * out.m21);
	out.ty = -(tx * out.m12 + ty * out.m22);
	return true;
}

unsigned File::u32(unsigned o) const
{
	if (o + 4 > data.size()) return 0;
	unsigned v; memcpy(&v, &data[o], 4); return v;
}

float File::f32(unsigned o) const
{
	if (o + 4 > data.size()) return 0;
	float v; memcpy(&v, &data[o], 4); return v;
}

std::string File::cstr(unsigned o) const
{
	if (o == 0 || o >= data.size()) return std::string();
	const char *s = (const char *)&data[o];
	size_t n = strnlen(s, data.size() - o);
	return std::string(s, n);
}

Character *File::exported(const std::string &n) const
{
	for (const Export &e : exports)
		if (e.name == n && e.character < characters.size())
			return characters[e.character];
	return nullptr;
}

int File::textureFor(int image) const
{
	auto it = imageMap.find(image);
	return it == imageMap.end() ? image : it->second.first;
}

const Geometry *File::getGeometry(unsigned id)
{
	auto it = geometry.find(id);
	if (it != geometry.end())
		return &it->second;
	Geometry &g = geometry[id];
	char path[512];
	snprintf(path, sizeof(path), "%s_geometry/%u.ru", name.c_str(), id);
	std::vector<unsigned char> buf;
	if (!library().readFile(path, buf))
	{
		snprintf(path, sizeof(path), "%s_geometry\\%u.ru", name.c_str(), id);
		library().readFile(path, buf);
	}
	library().parseGeometryText(std::string(buf.begin(), buf.end()), g);
	return &g;
}

// ------------------------------------------------------------------------------------------------
static Library s_library;
Library &library() { return s_library; }

void Library::clear()
{
	m_files.clear();
}

bool Library::readFile(const std::string &path, std::vector<unsigned char> &out)
{
	out.clear();
	if (TheFileSystem == nullptr)
		return false;
	::File *f = TheFileSystem->openFile(path.c_str(), ::File::READ | ::File::BINARY);
	if (f == nullptr)
		return false;
	const Int size = f->size();
	if (size > 0)
	{
		out.resize(size);
		f->read(&out[0], size);
	}
	f->close();
	return true;
}

static void parseColorBytes(const unsigned char *p, Color &c)
{
	c.r = p[0] / 255.0f; c.g = p[1] / 255.0f; c.b = p[2] / 255.0f; c.a = p[3] / 255.0f;
}

bool Library::parseGeometryText(const std::string &text, Geometry &g)
{
	GeometryEntry *cur = nullptr;
	size_t pos = 0;
	bool first = true;
	while (pos < text.size())
	{
		size_t end = text.find('\n', pos);
		if (end == std::string::npos) end = text.size();
		std::string line = text.substr(pos, end - pos);
		pos = end + 1;
		// trim
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
		size_t s = 0;
		while (s < line.size() && (line[s] == ' ' || line[s] == '\t')) ++s;
		line = line.substr(s);
		if (line.empty())
			continue;
		const char mode = line[0];
		std::string rest = line.substr(1);
		// split on ':'
		std::vector<std::string> ps;
		{
			size_t a = 0;
			while (true)
			{
				size_t b = rest.find(':', a);
				std::string tok = rest.substr(a, b == std::string::npos ? std::string::npos : b - a);
				size_t t0 = tok.find_first_not_of(" \t");
				size_t t1 = tok.find_last_not_of(" \t");
				ps.push_back(t0 == std::string::npos ? std::string() : tok.substr(t0, t1 - t0 + 1));
				if (b == std::string::npos) break;
				a = b + 1;
			}
		}
		auto F = [&](size_t i) { return i < ps.size() ? (float)atof(ps[i].c_str()) : 0.0f; };
		auto C = [&](size_t i) { return i < ps.size() ? atoi(ps[i].c_str()) / 255.0f : 0.0f; };
		switch (mode)
		{
		case 'c':
			cur = nullptr;
			break;
		case 's':
		{
			GeometryEntry e;
			if (ps.empty()) break;
			if (ps[0] == "s" && ps.size() >= 5)
			{
				e.style = GeometryEntry::SOLID;
				e.color = { C(1), C(2), C(3), C(4) };
			}
			else if (ps[0] == "l" && ps.size() >= 6)
			{
				e.style = GeometryEntry::LINES;
				e.lineWidth = F(1);
				e.color = { C(2), C(3), C(4), C(5) };
			}
			else if (ps[0] == "tc" && ps.size() >= 12)
			{
				e.style = GeometryEntry::TEXTURED;
				e.color = { C(1), C(2), C(3), C(4) };
				e.image = atoi(ps[5].c_str());
				e.texMatrix.m11 = F(6); e.texMatrix.m12 = F(7);
				e.texMatrix.m21 = F(8); e.texMatrix.m22 = F(9);
				e.texMatrix.tx = F(10); e.texMatrix.ty = F(11);
			}
			else
				break;
			g.entries.push_back(e);
			cur = &g.entries.back();
			break;
		}
		case 't':
			if (cur && cur->style != GeometryEntry::LINES && ps.size() >= 6)
				for (int i = 0; i < 6; ++i) cur->verts.push_back(F(i));
			break;
		case 'l':
			if (cur && cur->style == GeometryEntry::LINES && ps.size() >= 4)
				for (int i = 0; i < 4; ++i) cur->verts.push_back(F(i));
			break;
		}
	}
	for (const GeometryEntry &e : g.entries)
		for (size_t i = 0; i + 1 < e.verts.size(); i += 2)
		{
			const float x = e.verts[i], y = e.verts[i + 1];
			if (first) { g.minX = g.maxX = x; g.minY = g.maxY = y; first = false; }
			if (x < g.minX) g.minX = x;
			if (x > g.maxX) g.maxX = x;
			if (y < g.minY) g.minY = y;
			if (y > g.maxY) g.maxY = y;
		}
	return true;
}

void Library::parseFrameItem(File *f, unsigned o, FrameItem &it)
{
	const unsigned type = f->u32(o);
	const unsigned p = o + 4;
	it.kind = (FrameItem::Kind)type;
	switch (type)
	{
	case FrameItem::ACTION:
		it.code.file = f; it.code.offset = f->u32(p);
		break;
	case FrameItem::LABEL:
		it.name = f->cstr(f->u32(p));
		break;
	case FrameItem::PLACE:
	{
		it.flags = f->u32(p);
		it.depth = f->i32(p + 4);
		it.character = (it.flags & PLACE_HAS_CHARACTER) ? f->i32(p + 8) : -1;
		if (it.flags & PLACE_HAS_MATRIX)
		{
			it.matrix.m11 = f->f32(p + 12); it.matrix.m12 = f->f32(p + 16);
			it.matrix.m21 = f->f32(p + 20); it.matrix.m22 = f->f32(p + 24);
			it.matrix.tx = f->f32(p + 28); it.matrix.ty = f->f32(p + 32);
		}
		if (it.flags & PLACE_HAS_COLOR)
		{
			if (p + 44 <= f->data.size())
			{
				parseColorBytes(&f->data[p + 36], it.color.mul);
				parseColorBytes(&f->data[p + 40], it.color.add);
			}
		}
		if (it.flags & PLACE_HAS_RATIO) it.ratio = f->f32(p + 44);
		if (it.flags & PLACE_HAS_NAME) it.name = f->cstr(f->u32(p + 48));
		it.clipDepth = f->i32(p + 52);
		if (it.flags & PLACE_HAS_CLIPACTION)
		{
			const unsigned poa = f->u32(p + 56);
			if (poa != 0)
			{
				const unsigned n = f->u32(poa), list = f->u32(poa + 4);
				for (unsigned i = 0; i < n && i < 64; ++i)
				{
					const unsigned e = list + 12 * i;
					ClipEvent ce;
					ce.flags = f->u32(e) & 0xffffff;
					ce.keyCode = (e + 3 < f->data.size()) ? f->data[e + 3] : 0;
					ce.code.file = f; ce.code.offset = f->u32(e + 8);
					it.clipEvents.push_back(ce);
				}
			}
		}
		break;
	}
	case FrameItem::REMOVE:
		it.depth = f->i32(p);
		break;
	case FrameItem::BACKGROUND:
		if (p + 4 <= f->data.size()) parseColorBytes(&f->data[p], it.background);
		break;
	case FrameItem::INITACTION:
		it.initSprite = f->u32(p);
		it.code.file = f; it.code.offset = f->u32(p + 4);
		break;
	default:
		break;
	}
}

void Library::parseFrames(File *f, unsigned listOffset, Character *c)
{
	const unsigned n = f->u32(listOffset), off = f->u32(listOffset + 4);
	c->frames.resize(n);
	for (unsigned i = 0; i < n; ++i)
	{
		const unsigned fo = off + 8 * i;
		const unsigned items = f->u32(fo), list = f->u32(fo + 4);
		for (unsigned k = 0; k < items; ++k)
		{
			const unsigned io = f->u32(list + 4 * k);
			if (io == 0) continue;
			FrameItem it;
			parseFrameItem(f, io, it);
			if (it.kind == FrameItem::LABEL)
				c->labels[lower(it.name)] = (int)i;
			c->frames[i].items.push_back(std::move(it));
		}
	}
}

Character *Library::parseCharacter(File *f, unsigned o, int id, bool &seenMovie)
{
	const unsigned type = f->u32(o);
	if (f->u32(o + 4) != SIGNATURE)
		return nullptr;
	if (type == CHAR_MOVIE && seenMovie)
		return nullptr;	// character 0 points back at the movie itself
	const unsigned p = o + 8;
	std::unique_ptr<Character> c(new Character);
	c->type = (CharacterType)type;
	c->file = f;
	c->id = id;
	switch (type)
	{
	case CHAR_MOVIE:
		seenMovie = true;
		f->movie = c.get();
		parseFrames(f, p, c.get());
		{
			const unsigned n = f->u32(p + 12), list = f->u32(p + 16);
			f->characters.assign(n, nullptr);
			Character *self = c.get();
			f->owned.push_back(std::move(c));
			for (unsigned i = 0; i < n; ++i)
			{
				const unsigned co = f->u32(list + 4 * i);
				if (co) f->characters[i] = parseCharacter(f, co, (int)i, seenMovie);
			}
			if (n > 0) f->characters[0] = self;
			f->width = f->u32(p + 20);
			f->height = f->u32(p + 24);
			f->msPerFrame = f->u32(p + 28);
			if (f->msPerFrame == 0) f->msPerFrame = 33;
			const unsigned ni = f->u32(p + 32), li = f->u32(p + 36);
			for (unsigned i = 0; i < ni; ++i)
			{
				const unsigned io = li + 16 * i;
				Import im; im.movie = f->cstr(f->u32(io)); im.name = f->cstr(f->u32(io + 4)); im.character = f->u32(io + 8);
				f->imports.push_back(im);
			}
			const unsigned ne = f->u32(p + 40), le = f->u32(p + 44);
			for (unsigned i = 0; i < ne; ++i)
			{
				const unsigned eo = le + 8 * i;
				Export ex; ex.name = f->cstr(f->u32(eo)); ex.character = f->u32(eo + 4);
				f->exports.push_back(ex);
			}
			return self;
		}
	case CHAR_SPRITE:
		parseFrames(f, p, c.get());
		break;
	case CHAR_SHAPE:
		for (int i = 0; i < 4; ++i) c->bounds[i] = f->f32(p + 4 * i);
		c->geometry = f->u32(p + 16);
		break;
	case CHAR_IMAGE:
		c->texture = f->u32(p);
		break;
	case CHAR_TEXT:
		for (int i = 0; i < 4; ++i) c->bounds[i] = f->f32(p + 4 * i);
		c->font = f->u32(p + 16);
		c->alignment = f->u32(p + 20);
		if (p + 28 <= f->data.size()) parseColorBytes(&f->data[p + 24], c->textColor);
		c->fontHeight = f->f32(p + 28);
		c->readOnly = f->u32(p + 32) != 0;
		c->multiline = f->u32(p + 36) != 0;
		c->wordWrap = f->u32(p + 40) != 0;
		c->content = f->cstr(f->u32(p + 44));
		c->variable = f->cstr(f->u32(p + 48));
		break;
	case CHAR_FONT:
		c->fontName = f->cstr(f->u32(p));
		break;
	case CHAR_BUTTON:
	{
		c->isMenu = f->u32(p) != 0;
		for (int i = 0; i < 4; ++i) c->bounds[i] = f->f32(p + 4 + 4 * i);
		const unsigned tc = f->u32(p + 20), vc = f->u32(p + 24);
		const unsigned vo = f->u32(p + 28), to = f->u32(p + 32);
		for (unsigned i = 0; i < vc; ++i) { c->hitVerts.push_back(f->f32(vo + 8 * i)); c->hitVerts.push_back(f->f32(vo + 8 * i + 4)); }
		for (unsigned i = 0; i < tc * 3; ++i)
		{
			unsigned short v = 0;
			if (to + 2 * i + 2 <= f->data.size()) memcpy(&v, &f->data[to + 2 * i], 2);
			c->hitTris.push_back(v);
		}
		const unsigned nr = f->u32(p + 36), lr = f->u32(p + 40);
		for (unsigned i = 0; i < nr; ++i)
		{
			const unsigned ro = lr + 68 * i;
			ButtonRecord r;
			r.states = ro < f->data.size() ? f->data[ro] : 0;
			r.character = f->i32(ro + 4);
			r.depth = f->i32(ro + 8);
			r.matrix.m11 = f->f32(ro + 12); r.matrix.m12 = f->f32(ro + 16);
			r.matrix.m21 = f->f32(ro + 20); r.matrix.m22 = f->f32(ro + 24);
			r.matrix.tx = f->f32(ro + 28); r.matrix.ty = f->f32(ro + 32);
			r.color.mul = { f->f32(ro + 36), f->f32(ro + 40), f->f32(ro + 44), f->f32(ro + 48) };
			c->records.push_back(r);
		}
		const unsigned na = f->u32(p + 44), la = f->u32(p + 48);
		for (unsigned i = 0; i < na; ++i)
		{
			const unsigned ao = la + 8 * i;
			ButtonAction a;
			a.flags = ao < f->data.size() ? f->data[ao] : 0;
			if (ao + 3 <= f->data.size()) memcpy(&a.key, &f->data[ao + 1], 2);
			a.code.file = f; a.code.offset = f->u32(ao + 4);
			c->actions.push_back(a);
		}
		break;
	}
	default:
		break;
	}
	Character *raw = c.get();
	f->owned.push_back(std::move(c));
	return raw;
}

bool Library::parse(File *f)
{
	// constants
	std::vector<unsigned char> cdata;
	if (!readFile(f->name + ".const", cdata) || cdata.size() < 32 || memcmp(&cdata[0], "Apt constant file", 17) != 0)
	{
		lastError = "missing or bad " + f->name + ".const";
		return false;
	}
	unsigned entry, count, hdr;
	memcpy(&entry, &cdata[20], 4);
	memcpy(&count, &cdata[24], 4);
	memcpy(&hdr, &cdata[28], 4);
	for (unsigned i = 0; i < count && 32 + 8 * i + 8 <= cdata.size(); ++i)
	{
		ConstEntry e;
		unsigned t, v;
		memcpy(&t, &cdata[32 + 8 * i], 4);
		memcpy(&v, &cdata[32 + 8 * i + 4], 4);
		e.type = (ConstEntry::Type)t;
		e.u = v;
		if (t == ConstEntry::C_STRING && v < cdata.size())
			e.str = std::string((const char *)&cdata[v], strnlen((const char *)&cdata[v], cdata.size() - v));
		else if (t == ConstEntry::C_FLOAT)
			memcpy(&e.f, &v, 4);
		f->constants.push_back(e);
	}
	if (f->data.size() < 8 || memcmp(&f->data[0], "Apt Data", 8) != 0)
	{
		lastError = "bad " + f->name + ".apt";
		return false;
	}
	bool seen = false;
	if (parseCharacter(f, entry, 0, seen) == nullptr || f->movie == nullptr)
	{
		lastError = "no movie in " + f->name + ".apt";
		return false;
	}
	// image map
	std::vector<unsigned char> dat;
	if (readFile(f->name + ".dat", dat))
	{
		std::string text(dat.begin(), dat.end());
		size_t pos = 0;
		while (pos < text.size())
		{
			size_t end = text.find('\n', pos);
			if (end == std::string::npos) end = text.size();
			std::string line = text.substr(pos, end - pos);
			pos = end + 1;
			const size_t sc = line.find(';');
			if (sc != std::string::npos) line = line.substr(0, sc);
			size_t sep = line.find("->");
			size_t seplen = 2;
			if (sep == std::string::npos) { sep = line.find('='); seplen = 1; }
			if (sep == std::string::npos) continue;
			const int image = atoi(line.substr(0, sep).c_str());
			std::string rest = line.substr(sep + seplen);
			int vals[4] = {0, 0, 0, 0};
			const int n = sscanf(rest.c_str(), " %d %d %d %d", &vals[0], &vals[1], &vals[2], &vals[3]);
			if (n == 1) f->imageMap[image] = std::make_pair(vals[0], false);
			else if (n >= 4) f->imageMap[image] = std::make_pair(image, true);
		}
	}
	// imports
	for (const Import &im : f->imports)
	{
		std::string mov = im.movie;
		const size_t dot = mov.rfind('.');
		if (dot != std::string::npos) mov = mov.substr(0, dot);
		File *other = load(mov);
		if (other == nullptr || other == f) continue;
		Character *c = other->exported(im.name);
		if (c && im.character < f->characters.size())
			f->characters[im.character] = c;
	}
	return true;
}

File *Library::load(const std::string &nameIn)
{
	std::string name = nameIn;
	const size_t dot = name.rfind('.');
	if (dot != std::string::npos && (lower(name.substr(dot)) == ".apt" || lower(name.substr(dot)) == ".swf"))
		name = name.substr(0, dot);	// scripts loadMovie("X.swf"): the shipped movie is X.apt
	const std::string key = lower(name);
	auto it = m_files.find(key);
	if (it != m_files.end())
		return it->second.get();	// may be null for a file that failed to load
	m_files[key] = nullptr;
	std::unique_ptr<File> f(new File);
	f->name = name;
	if (!readFile(name + ".apt", f->data))
	{
		lastError = "missing " + name + ".apt";
		return nullptr;
	}
	File *raw = f.get();
	m_files[key] = std::move(f);
	if (!parse(raw))
	{
		fprintf(stderr, "[apt] failed to load %s: %s\n", name.c_str(), lastError.c_str());
		m_files[key].reset();
		return nullptr;
	}
	return raw;
}

} // namespace Apt
