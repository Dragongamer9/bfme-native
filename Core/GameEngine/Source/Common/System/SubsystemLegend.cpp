// bfme-native: data-driven subsystem INI lists. See SubsystemLegend.h.
#include "PreRTS.h"

#include "Common/SubsystemLegend.h"
#include "Common/INI.h"
#include "Common/INIPreprocessor.h"
#include "Common/FileSystem.h"
#include "Common/file.h"

#include <algorithm>
#include <cstring>

namespace {

std::vector<SubsystemLegend::Entry>& entries()
{
	static std::vector<SubsystemLegend::Entry> e;
	return e;
}

Bool s_loaded = FALSE;

AsciiString trimmed(const char* b, const char* e)
{
	while (b < e && (*b == ' ' || *b == '\t' || *b == '\r'))
		++b;
	while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
		--e;
	AsciiString s;
	s.set(b, (Int)(e - b));
	return s;
}

Bool startsWithPath(const AsciiString& path, const AsciiString& prefix)
{
	if (prefix.isEmpty())
		return FALSE;
	const char* p = path.str();
	const char* q = prefix.str();
	while (*q) {
		char a = (char)tolower((unsigned char)(*p == '/' ? '\\' : *p));
		char b = (char)tolower((unsigned char)(*q == '/' ? '\\' : *q));
		if (a != b)
			return FALSE;
		++p;
		++q;
	}
	return *p == 0 || *p == '\\' || *p == '/' || q[-1] == '\\' || q[-1] == '/';
}

} // namespace

Bool SubsystemLegend::loadDefault()
{
	static const char* candidates[] = {
		"Data\\INI\\Default\\SubsystemLegendExpansion1.ini", // RotWK
		"Data\\INI\\Default\\SubsystemLegend.ini",           // BFME2
	};
	for (const char* c : candidates)
		if (TheFileSystem && TheFileSystem->doesFileExist(c) && load(c))
			return TRUE;
	return FALSE;
}

Bool SubsystemLegend::load(const char* filename)
{
	File* f = TheFileSystem->openFile(filename, File::READ);
	if (!f)
		return FALSE;
	unsigned rawLen = f->size();
	char* raw = f->readEntireAndClose();
	unsigned len = 0;
	char* text = INIPreprocessor::expand(filename, raw, rawLen, len);
	delete[] raw;

	entries().clear();
	Entry cur;
	Bool inBlock = FALSE;
	const char* p = text;
	const char* end = text + len;
	while (p < end) {
		const char* le = (const char*)memchr(p, '\n', end - p);
		if (!le)
			le = end;
		AsciiString line = trimmed(p, le);
		p = le + 1;
		if (line.isEmpty())
			continue;
		const char* s = line.str();
		if (!inBlock) {
			if (strncmp(s, "LoadSubsystem", 13) == 0) {
				cur = Entry();
				cur.name = trimmed(s + 13, s + strlen(s));
				inBlock = TRUE;
			}
			continue;
		}
		if (strcasecmp(s, "End") == 0) {
			entries().push_back(cur);
			inBlock = FALSE;
			continue;
		}
		const char* eq = strchr(s, '=');
		if (!eq)
			continue;
		AsciiString key = trimmed(s, eq);
		AsciiString val = trimmed(eq + 1, s + strlen(s));
		if (key.compareNoCase("Loader") == 0)
			cur.loader = val;
		else if (key.compareNoCase("InitFile") == 0)
			cur.initFiles.push_back(val);
		else if (key.compareNoCase("InitPath") == 0)
			cur.initPaths.push_back(val);
		else if (key.compareNoCase("ExcludePath") == 0)
			cur.excludePaths.push_back(val);
		else if (key.compareNoCase("InitFileDebug") == 0)
			cur.initFilesDebug.push_back(val);
		// Extension / IncludePathCinematics: unused by the retail game
	}
	delete[] text;
	s_loaded = !entries().empty();
	DEBUG_LOG(("SubsystemLegend: %d subsystems from %s", (Int)entries().size(), filename));
	return s_loaded;
}

const SubsystemLegend::Entry* SubsystemLegend::find(const AsciiString& name)
{
	for (const Entry& e : entries())
		if (e.name.compareNoCase(name) == 0)
			return &e;
	return nullptr;
}

Bool SubsystemLegend::isLoaded() { return s_loaded; }
UnsignedInt SubsystemLegend::getEntryCount() { return (UnsignedInt)entries().size(); }

UnsignedInt SubsystemLegend::loadEntry(const Entry& e, Xfer* pXfer)
{
	if (!e.loader.isEmpty() && e.loader.compareNoCase("INI") != 0)
		return 0; // other loaders (none in retail data) are not handled here

	UnsignedInt read = 0;
	INI ini;
	for (const AsciiString& f : e.initFiles) {
		if (TheFileSystem->doesFileExist(f.str()))
			read += ini.load(f, INI_LOAD_OVERWRITE, pXfer);
		else
			DEBUG_LOG(("SubsystemLegend: %s: missing InitFile %s", e.name.str(), f.str()));
	}
#if defined(RTS_DEBUG)
	for (const AsciiString& f : e.initFilesDebug)
		if (TheFileSystem->doesFileExist(f.str()))
			read += ini.load(f, INI_LOAD_OVERWRITE, pXfer);
#endif
	for (const AsciiString& dir : e.initPaths) {
		AsciiString d = dir;
		if (!d.endsWith("\\") && !d.endsWith("/"))
			d.concat('\\');
		FilenameList files;
		TheFileSystem->getFileListInDirectory(d, "*.ini", files, TRUE);
		// sorted, top directory first then subdirectories - same order as INI::loadDirectory
		std::vector<AsciiString> top, sub;
		for (FilenameList::const_iterator it = files.begin(); it != files.end(); ++it) {
			Bool excluded = FALSE;
			for (const AsciiString& x : e.excludePaths)
				if (startsWithPath(*it, x)) {
					excluded = TRUE;
					break;
				}
			if (excluded)
				continue;
			const char* rest = it->str() + d.getLength();
			(strchr(rest, '\\') || strchr(rest, '/') ? sub : top).push_back(*it);
		}
		for (const AsciiString& f : top)
			read += ini.load(f, INI_LOAD_OVERWRITE, pXfer);
		for (const AsciiString& f : sub)
			read += ini.load(f, INI_LOAD_OVERWRITE, pXfer);
	}
	return read;
}
