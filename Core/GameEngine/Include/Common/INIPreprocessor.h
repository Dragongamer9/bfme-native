// bfme-native: INI preprocessor for BFME2 / RotWK data.
//
// Reimplements the behaviour of the RotWK game.dat INI front end
// (decompiled from 0x00a159aa include expansion, 0x0042cfc9 macro table,
// 0x0042ddd1 / 0x0042dfa1 / 0x0042e172 macro arithmetic):
//
//   #include "path"        inline another file; path is relative to the including
//                          file's directory ("\includes\x.inc" -> <dir>/includes/x.inc),
//                          '/' and '\' both accepted, circular includes are an error.
//   #define NAME value...  global text macro. Names must be UPPERCASE. The value is the
//                          remaining tokens joined by single spaces. Macros persist across
//                          files for the whole INI load, exactly like the original
//                          (one global table); redefining with a different value is an error
//                          except during a reload/override pass. Not allowed in map.ini.
//   NAME                   any whole token equal to a macro name is replaced by its value.
//   #ADD( a b ... )        n-ary sum, #MULTIPLY( a b ... ) n-ary product,
//   #SUBTRACT( a b )       binary, #DIVIDE( a b ) binary. Operands may be numbers,
//                          macros or nested operations. Evaluated as Int, UnsignedInt or
//                          Real depending on the field type being parsed.
//   // comment             BFME accepts C++-style comments in addition to ';'.
#pragma once

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"

class INIPreprocessor
{
public:
	/// Expand includes, consume #define lines and substitute macros in an INI file buffer.
	/// Returns a newly allocated NUL-terminated buffer (delete[]) and its length in outLen.
	static char* expand(const AsciiString& filename, const char* data, unsigned len, unsigned& outLen);

	/// True when the token starts a macro arithmetic operation (#ADD( etc).
	static Bool isMathToken(const char* token);

	/// Evaluate a math operation whose opening token has already been read; operands are
	/// pulled with INI::getNextToken(). Throws INI_INVALID_DATA on malformed input.
	static Int evalInt(const char* opToken);
	static UnsignedInt evalUnsignedInt(const char* opToken);
	static Real evalReal(const char* opToken);

	/// Forget all macros (new game data set, e.g. switching mods).
	static void reset();

	/// When TRUE, a #define may replace an existing macro (override/patch INI pass).
	static void setAllowRedefine(Bool allow);

	static UnsignedInt getMacroCount();
};
