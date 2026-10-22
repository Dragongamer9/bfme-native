/*
**	bfme-native: SAGE "APT" user interface movies (BFME / BFME2 / RotWK).
**
**	An APT movie is EA's binary rendition of a Flash movie:
**	  <name>.apt            characters, frames, frame items and ActionScript bytecode
**	  <name>.const          constant table (strings / registers) and the movie's entry offset
**	  <name>.dat            image id -> texture id (or rectangle) map
**	  <name>_geometry/N.ru  shape geometry as text (triangles / lines, solid or textured)
**	  art/Textures/apt_<name>_N.tga
**	Pointers inside the .apt are file offsets; lists are (count, offset).
**	Format knowledge: OpenSAGE (GPLv3) Data/Apt, verified against RotWK 2.01 data.
*/
#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Apt
{

enum CharacterType
{
	CHAR_SHAPE = 1, CHAR_TEXT = 2, CHAR_FONT = 3, CHAR_BUTTON = 4, CHAR_SPRITE = 5, CHAR_SOUND = 6,
	CHAR_IMAGE = 7, CHAR_MORPH = 8, CHAR_MOVIE = 9, CHAR_STATICTEXT = 10, CHAR_NONE = 11, CHAR_VIDEO = 12
};

enum PlaceFlags
{
	PLACE_MOVE = 1, PLACE_HAS_CHARACTER = 2, PLACE_HAS_MATRIX = 4, PLACE_HAS_COLOR = 8,
	PLACE_HAS_RATIO = 16, PLACE_HAS_NAME = 32, PLACE_HAS_CLIPDEPTH = 64, PLACE_HAS_CLIPACTION = 128
};

enum ClipEventFlags
{
	CLIPEVENT_KEYUP = 0x800000, CLIPEVENT_KEYDOWN = 0x400000, CLIPEVENT_MOUSEUP = 0x200000,
	CLIPEVENT_MOUSEDOWN = 0x100000, CLIPEVENT_MOUSEMOVE = 0x080000, CLIPEVENT_UNLOAD = 0x040000,
	CLIPEVENT_ENTERFRAME = 0x020000, CLIPEVENT_LOAD = 0x010000, CLIPEVENT_DRAGOVER = 0x008000,
	CLIPEVENT_ROLLOUT = 0x004000, CLIPEVENT_ROLLOVER = 0x002000, CLIPEVENT_RELEASEOUTSIDE = 0x001000,
	CLIPEVENT_RELEASE = 0x000800, CLIPEVENT_PRESS = 0x000400, CLIPEVENT_DRAGOUT = 0x000200,
	CLIPEVENT_DATA = 0x000100, CLIPEVENT_CONSTRUCT = 0x000004, CLIPEVENT_KEYPRESS = 0x000002,
	CLIPEVENT_INITIALIZE = 0x000001
};

/// 2x3 affine transform, row-vector convention: x' = x*m11 + y*m21 + tx, y' = x*m12 + y*m22 + ty
struct Matrix
{
	float m11 = 1, m12 = 0, m21 = 0, m22 = 1, tx = 0, ty = 0;
	/// this applied first, then b
	Matrix then(const Matrix &b) const
	{
		Matrix r;
		r.m11 = m11 * b.m11 + m12 * b.m21; r.m12 = m11 * b.m12 + m12 * b.m22;
		r.m21 = m21 * b.m11 + m22 * b.m21; r.m22 = m21 * b.m12 + m22 * b.m22;
		r.tx = tx * b.m11 + ty * b.m21 + b.tx; r.ty = tx * b.m12 + ty * b.m22 + b.ty;
		return r;
	}
	void apply(float x, float y, float &ox, float &oy) const { ox = x * m11 + y * m21 + tx; oy = x * m12 + y * m22 + ty; }
	bool invert(Matrix &out) const;
};

struct Color { float r = 1, g = 1, b = 1, a = 1; };

/// multiply + add colour transform (Flash CXFORM)
struct ColorTransform
{
	Color mul;
	Color add{0, 0, 0, 0};
	ColorTransform then(const ColorTransform &outer) const
	{
		ColorTransform r;
		r.mul = { mul.r * outer.mul.r, mul.g * outer.mul.g, mul.b * outer.mul.b, mul.a * outer.mul.a };
		r.add = { add.r * outer.mul.r + outer.add.r, add.g * outer.mul.g + outer.add.g,
			add.b * outer.mul.b + outer.add.b, add.a * outer.mul.a + outer.add.a };
		return r;
	}
};

class File;

/// a reference to bytecode: the movie it lives in plus its offset there
struct Code
{
	File *file = nullptr;
	unsigned offset = 0;
	bool valid() const { return file != nullptr && offset != 0; }
};

struct ClipEvent
{
	unsigned flags = 0;
	unsigned char keyCode = 0;
	Code code;
};

struct FrameItem
{
	enum Kind { ACTION = 1, LABEL = 2, PLACE = 3, REMOVE = 4, BACKGROUND = 5, INITACTION = 8 } kind = ACTION;
	// place / remove
	unsigned flags = 0;
	int depth = 0;
	int character = -1;
	Matrix matrix;
	ColorTransform color;
	float ratio = 0;
	std::string name;	///< place: instance name; label: frame label
	int clipDepth = 0;
	std::vector<ClipEvent> clipEvents;
	// actions
	Code code;
	unsigned initSprite = 0;
	Color background;
};

struct Frame
{
	std::vector<FrameItem> items;
};

struct GeometryEntry
{
	enum Style { SOLID, TEXTURED, LINES } style = SOLID;
	Color color;
	int image = 0;			///< textured: image id (see .dat)
	Matrix texMatrix;		///< textured: shape space -> texture pixels
	float lineWidth = 1;
	std::vector<float> verts;	///< triangles: x,y * 3 per triangle; lines: x,y * 2 per line
};

struct Geometry
{
	std::vector<GeometryEntry> entries;
	float minX = 0, minY = 0, maxX = 0, maxY = 0;
};

struct ButtonRecord
{
	unsigned char states = 0;	///< 1 up, 2 over, 4 down, 8 hit
	int character = 0;
	int depth = 0;
	Matrix matrix;
	ColorTransform color;
};

struct ButtonAction
{
	unsigned char flags = 0;	///< transition flags (idle->overUp = 1, ...)
	unsigned short key = 0;
	Code code;
};

struct Character
{
	CharacterType type = CHAR_NONE;
	File *file = nullptr;		///< the movie this character comes from
	int id = 0;
	// sprite / movie
	std::vector<Frame> frames;
	std::map<std::string, int> labels;	///< label -> frame index (case-insensitive key: lower-case)
	// shape
	float bounds[4] = {0, 0, 0, 0};
	unsigned geometry = 0;
	// image
	unsigned texture = 0;
	// text
	unsigned font = 0, alignment = 0;
	Color textColor;
	float fontHeight = 0;
	bool readOnly = false, multiline = false, wordWrap = false;
	std::string content, variable;
	// font
	std::string fontName;
	// button
	bool isMenu = false;
	std::vector<float> hitVerts;		///< button hit area: x,y pairs
	std::vector<unsigned short> hitTris;
	std::vector<ButtonRecord> records;
	std::vector<ButtonAction> actions;
};

struct Import { std::string movie, name; unsigned character = 0; };
struct Export { std::string name; unsigned character = 0; };

struct ConstEntry
{
	enum Type { C_UNDEF = 0, C_STRING = 1, C_PROPERTY = 2, C_NONE = 3, C_REGISTER = 4, C_BOOLEAN = 5, C_FLOAT = 6, C_INTEGER = 7, C_LOOKUP = 8 } type = C_UNDEF;
	std::string str;
	unsigned u = 0;
	float f = 0;
};

class File
{
public:
	std::string name;
	std::vector<unsigned char> data;		///< the .apt image (bytecode is read from here)
	std::vector<ConstEntry> constants;
	std::vector<Character *> characters;	///< owned by the File of origin (imports alias other files)
	std::vector<std::unique_ptr<Character>> owned;
	Character *movie = nullptr;
	unsigned width = 0, height = 0, msPerFrame = 33;
	std::vector<Import> imports;
	std::vector<Export> exports;
	std::unordered_map<int, std::pair<int, bool>> imageMap;	///< image -> (texture id, has rectangle)
	std::unordered_map<unsigned, Geometry> geometry;

	const Geometry *getGeometry(unsigned id);
	Character *exported(const std::string &name) const;
	int textureFor(int image) const;

	unsigned u32(unsigned o) const;
	int i32(unsigned o) const { return (int)u32(o); }
	float f32(unsigned o) const;
	std::string cstr(unsigned o) const;
};

/// loads movies by name (case-insensitive), resolving imports; owns all loaded files
class Library
{
public:
	File *load(const std::string &name);
	void clear();
	std::string lastError;
private:
	std::map<std::string, std::unique_ptr<File>> m_files;
	bool parse(File *f);
	Character *parseCharacter(File *f, unsigned offset, int id, bool &seenMovie);
	void parseFrames(File *f, unsigned listOffset, Character *c);
	void parseFrameItem(File *f, unsigned o, FrameItem &it);
	bool readFile(const std::string &path, std::vector<unsigned char> &out);
	bool parseGeometryText(const std::string &text, Geometry &g);
	friend class File;
};

Library &library();

std::string lower(const std::string &s);

} // namespace Apt
