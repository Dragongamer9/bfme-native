// bfme-native: data-driven subsystem INI lists (BFME2 / RotWK "SubsystemLegend").
//
// RotWK replaced the hard-coded INI paths in GameEngine::init with
// Data\INI\Default\SubsystemLegendExpansion1.ini (BFME2: SubsystemLegend.ini):
//
//   LoadSubsystem TheThingFactory
//     Loader      = INI
//     InitFile    = Data\INI\Default\Object.ini
//     InitPath    = Data\INI\Object          ; every *.ini below, recursively
//     ExcludePath = Data\INI\Object\Junk     ; pruned while recursing
//     InitFileDebug = ...                    ; debug/internal builds only
//   End
//
// When a legend is loaded, initSubsystem() uses its entry for the subsystem name and
// ignores the compiled-in paths; subsystems without an entry keep the old behaviour.
#pragma once

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"

#include <vector>

class Xfer;

class SubsystemLegend
{
public:
	struct Entry
	{
		AsciiString name;
		AsciiString loader;
		std::vector<AsciiString> initFiles;
		std::vector<AsciiString> initPaths;
		std::vector<AsciiString> excludePaths;
		std::vector<AsciiString> initFilesDebug;
	};

	/// Try the RotWK legend, then the BFME2 one. Returns TRUE if one was found.
	static Bool loadDefault();
	static Bool load(const char* filename);

	static const Entry* find(const AsciiString& subsystemName);
	static Bool isLoaded();

	/// Load every file the entry names, in legend order. Returns files read.
	static UnsignedInt loadEntry(const Entry& e, Xfer* pXfer);

	static UnsignedInt getEntryCount();
};
