// bfme-native: INI preprocessor for BFME2 / RotWK data. See INIPreprocessor.h.
#include "PreRTS.h"

#include "Common/INIPreprocessor.h"
#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/FileSystem.h"
#include "Common/file.h"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <cstdio>

// errors are reported on stderr too: DEBUG_CRASH is compiled out of release builds
#define BFME_INI_ERROR(args) do { DEBUG_CRASH(args); bfmeIniErrorf args; fputc('\n', stderr); } while (0)
static void bfmeIniErrorf(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fputs("[INI-PP] ", stderr);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

namespace {

// RotWK compares macro names case-insensitively (Edain writes Isengard_TAVERN_BUILDTIME for
// ISENGARD_TAVERN_BUILDTIME), so names are stored upper-cased.
typedef std::map<std::string, std::string> MacroMap;

std::string upper(const std::string& s)
{
	std::string u = s;
	for (char& c : u)
		c = (char)toupper((unsigned char)c);
	return u;
}

MacroMap& macros()
{
	static MacroMap m;
	return m;
}

Bool s_allowRedefine = FALSE;

const int MAX_INCLUDE_DEPTH = 32;
const int MAX_MACRO_DEPTH = 16;

// --- path helpers ---------------------------------------------------------------------------

std::string normalizeSlashes(std::string p)
{
	for (char& c : p)
		if (c == '/')
			c = '\\';
	return p;
}

std::string dirOf(const std::string& path)
{
	size_t s = path.find_last_of('\\');
	return s == std::string::npos ? std::string() : path.substr(0, s);
}

/// "Data\INI\Object" + "..\..\Includes\x.inc" -> "Data\Includes\x.inc"
/// A leading '\' is relative to dir, as with Win32 PathAppend.
std::string joinCanonical(const std::string& dir, std::string rel)
{
	rel = normalizeSlashes(rel);
	while (!rel.empty() && rel[0] == '\\')
		rel.erase(0, 1);
	std::vector<std::string> parts;
	auto push = [&parts](const std::string& s) {
		size_t b = 0;
		while (b <= s.size()) {
			size_t e = s.find('\\', b);
			if (e == std::string::npos)
				e = s.size();
			std::string seg = s.substr(b, e - b);
			if (seg == "..") {
				if (!parts.empty())
					parts.pop_back();
			} else if (!seg.empty() && seg != ".") {
				parts.push_back(seg);
			}
			b = e + 1;
		}
	};
	push(normalizeSlashes(dir));
	push(rel);
	std::string out;
	for (size_t i = 0; i < parts.size(); ++i) {
		if (i)
			out += '\\';
		out += parts[i];
	}
	return out;
}

std::string lower(std::string s)
{
	for (char& c : s)
		c = (char)tolower((unsigned char)c);
	return s;
}

// --- line helpers ---------------------------------------------------------------------------

const char* skipWs(const char* p)
{
	while (*p == ' ' || *p == '\t')
		++p;
	return p;
}

/// Strip ';' and '//' comments (outside quotes) in place.
void stripComments(std::string& line)
{
	bool q = false;
	for (size_t i = 0; i < line.size(); ++i) {
		char c = line[i];
		if (c == '"')
			q = !q;
		else if (!q && (c == ';' || (c == '/' && i + 1 < line.size() && line[i + 1] == '/'))) {
			line.erase(i);
			return;
		}
	}
}

bool startsWithDirective(const char* p, const char* dir)
{
	size_t n = strlen(dir);
	return strncmp(p, dir, n) == 0 && (p[n] == 0 || p[n] == ' ' || p[n] == '\t' || p[n] == '"');
}

bool isTokenSep(char c)
{
	return c == ' ' || c == '\t' || c == '=' || c == ',' || c == ':' || c == '\r' || c == '\0';
}

/// Replace every whole token equal to a macro name. Text inside quotes is left alone.
std::string substitute(const std::string& line, int depth = 0)
{
	const MacroMap& m = macros();
	if (m.empty() || depth > MAX_MACRO_DEPTH)
		return line;
	std::string out;
	out.reserve(line.size());
	size_t i = 0;
	bool changed = false;
	while (i < line.size()) {
		char c = line[i];
		if (c == '"') {
			size_t e = line.find('"', i + 1);
			e = (e == std::string::npos) ? line.size() : e + 1;
			out.append(line, i, e - i);
			i = e;
			continue;
		}
		if (isTokenSep(c)) {
			out += c;
			++i;
			continue;
		}
		size_t e = i;
		while (e < line.size() && !isTokenSep(line[e]) && line[e] != '"')
			++e;
		std::string tok = line.substr(i, e - i);
		MacroMap::const_iterator it = m.find(upper(tok));
		if (it != m.end()) {
			out += it->second;
			changed = true;
		} else {
			out += tok;
		}
		i = e;
	}
	return changed ? substitute(out, depth + 1) : out;
}

void defineMacro(const AsciiString& file, unsigned lineNum, const char* rest)
{
	// rest: "NAME value tokens..."
	const char* p = skipWs(rest);
	const char* ne = p;
	while (*ne && *ne != ' ' && *ne != '\t')
		++ne;
	std::string name(p, ne - p);
	if (name.empty()) {
		BFME_INI_ERROR(("[%s:%u] #define without a name", file.str(), lineNum));
		throw INI_INVALID_DATA;
	}
	if (lower(file.str()).find("map.ini") != std::string::npos && lower(file.str()).rfind("map.ini") + 7 == strlen(file.str())) {
		BFME_INI_ERROR(("[%s] MACROs not allowed in map.ini (%s)", file.str(), name.c_str()));
		throw INI_INVALID_DATA;
	}
	for (char c : name) {
		if (c >= 'a' && c <= 'z') {
			// the original rejects these; Edain ships one in an unused include, so only warn
			DEBUG_LOG(("[%s:%u] MACRO names must use UPPERCASE: %s", file.str(), lineNum, name.c_str()));
			break;
		}
	}
	// value: remaining whitespace-separated tokens joined by single spaces
	std::string value;
	const char* v = skipWs(ne);
	while (*v) {
		const char* te = v;
		while (*te && *te != ' ' && *te != '\t')
			++te;
		if (!value.empty())
			value += ' ';
		value.append(v, te - v);
		v = skipWs(te);
	}
	if (value.empty()) {
		BFME_INI_ERROR(("[%s:%u] Error parsing MACRO '%s' has no value", file.str(), lineNum, name.c_str()));
		throw INI_INVALID_DATA;
	}
	name = upper(name);
	MacroMap& m = macros();
	MacroMap::iterator it = m.find(name);
	if (it != m.end()) {
		if (it->second != value && !s_allowRedefine) {
			BFME_INI_ERROR(("[%s:%u] Duplicate MACRO name '%s'", file.str(), lineNum, name.c_str()));
			throw INI_INVALID_DATA;
		}
		it->second = value;
	} else {
		m.insert(MacroMap::value_type(name, value));
	}
}

// --- include expansion ----------------------------------------------------------------------

void expandInto(const std::string& path, const char* data, unsigned len, std::string& out,
	std::vector<std::string>& stack, const AsciiString& rootFile)
{
	std::string key = lower(path);
	for (const std::string& s : stack) {
		if (s == key) {
			BFME_INI_ERROR(("ERROR: Circular include in %s (%s)", rootFile.str(), path.c_str()));
			throw INI_INVALID_DATA;
		}
	}
	if ((int)stack.size() >= MAX_INCLUDE_DEPTH) {
		BFME_INI_ERROR(("ERROR: include depth exceeded in %s", rootFile.str()));
		throw INI_INVALID_DATA;
	}
	stack.push_back(key);

	unsigned lineNum = 0;
	unsigned i = 0;
	std::string line;
	while (i < len) {
		unsigned e = i;
		while (e < len && data[e] != '\n')
			++e;
		line.assign(data + i, e - i);
		i = e + 1;
		++lineNum;
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		stripComments(line);
		const char* p = skipWs(line.c_str());

		if (*p == '#') {
			if (startsWithDirective(p, "#include")) {
				const char* q = skipWs(p + 8);
				std::string rel;
				if (*q == '"') {
					const char* qe = strchr(q + 1, '"');
					rel.assign(q + 1, qe ? (size_t)(qe - q - 1) : strlen(q + 1));
				} else {
					rel = q;
				}
				std::string inc = joinCanonical(dirOf(path), rel);
				File* f = TheFileSystem ? TheFileSystem->openFile(inc.c_str(), File::READ) : nullptr;
				if (!f) {
					BFME_INI_ERROR(("ERROR: Could not open include file %s (from %s line %u)", inc.c_str(), path.c_str(), lineNum));
					throw INI_CANT_OPEN_FILE;
				}
				unsigned sz = f->size();
				char* buf = f->readEntireAndClose();
				expandInto(inc, buf, sz, out, stack, rootFile);
				delete[] buf;
				continue;
			}
			if (startsWithDirective(p, "#define")) {
				defineMacro(rootFile, lineNum, p + 7);
				out += '\n'; // keep line count close to the original file
				continue;
			}
		}
		out += substitute(line);
		out += '\n';
	}
	stack.pop_back();
}

// --- arithmetic -----------------------------------------------------------------------------

enum MathOp { OP_NONE, OP_ADD, OP_SUBTRACT, OP_MULTIPLY, OP_DIVIDE };

MathOp opOf(const char* t)
{
	if (!t || t[0] != '#')
		return OP_NONE;
	if (strcmp(t, "#ADD(") == 0)
		return OP_ADD;
	if (strcmp(t, "#SUBTRACT(") == 0)
		return OP_SUBTRACT;
	if (strcmp(t, "#MULTIPLY(") == 0)
		return OP_MULTIPLY;
	if (strcmp(t, "#DIVIDE(") == 0)
		return OP_DIVIDE;
	return OP_NONE;
}

template <typename T>
T scanOperand(const char* tok);

template <>
Int scanOperand<Int>(const char* tok) { return INI::scanInt(tok); }
template <>
UnsignedInt scanOperand<UnsignedInt>(const char* tok) { return INI::scanUnsignedInt(tok); }
template <>
Real scanOperand<Real>(const char* tok) { return INI::scanReal(tok); }

/// Mirrors 0x0042ddd1: first operand, then (n-ary) more operands until ")".
template <typename T>
T evalOp(const char* opToken)
{
	MathOp op = opOf(opToken);
	if (op == OP_NONE) {
		BFME_INI_ERROR(("Expected known math operation after #, got '%s'", opToken));
		throw INI_INVALID_DATA;
	}
	// operands are themselves scanned, so nested #OP( ... ) and macros work
	T acc = scanOperand<T>(INI::getNextToken());
	switch (op) {
	case OP_ADD:
	case OP_MULTIPLY:
		for (;;) {
			const char* t = INI::getNextToken();
			if (strcmp(t, ")") == 0)
				break;
			T v = scanOperand<T>(t);
			acc = (op == OP_ADD) ? (T)(acc + v) : (T)(acc * v);
		}
		break;
	case OP_SUBTRACT:
	case OP_DIVIDE: {
		T v = scanOperand<T>(INI::getNextToken());
		if (op == OP_SUBTRACT)
			acc = (T)(acc - v);
		else {
			if (v == (T)0) {
				BFME_INI_ERROR(("#DIVIDE by zero"));
				throw INI_INVALID_DATA;
			}
			acc = (T)(acc / v);
		}
		if (strcmp(INI::getNextToken(), ")") != 0) {
			BFME_INI_ERROR((op == OP_SUBTRACT ? "#SUBTRACT takes only 2 operands" : "#DIVIDE takes only 2 operands"));
			throw INI_INVALID_DATA;
		}
		break;
	}
	default:
		break;
	}
	return acc;
}

} // namespace

char* INIPreprocessor::expand(const AsciiString& filename, const char* data, unsigned len, unsigned& outLen)
{
	std::string out;
	out.reserve(len + len / 8);
	std::vector<std::string> stack;
	expandInto(normalizeSlashes(filename.str()), data, len, out, stack, filename);
	char* buf = new char[out.size() + 1];
	memcpy(buf, out.data(), out.size());
	buf[out.size()] = 0;
	outLen = (unsigned)out.size();
	return buf;
}

Bool INIPreprocessor::isMathToken(const char* token)
{
	return opOf(token) != OP_NONE;
}

Int INIPreprocessor::evalInt(const char* opToken) { return evalOp<Int>(opToken); }
UnsignedInt INIPreprocessor::evalUnsignedInt(const char* opToken) { return evalOp<UnsignedInt>(opToken); }
Real INIPreprocessor::evalReal(const char* opToken) { return evalOp<Real>(opToken); }

void INIPreprocessor::reset() { macros().clear(); }
void INIPreprocessor::setAllowRedefine(Bool allow) { s_allowRedefine = allow; }
UnsignedInt INIPreprocessor::getMacroCount() { return (UnsignedInt)macros().size(); }
