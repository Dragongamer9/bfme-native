// bfme-native: RotWK "ObjectFilter" (e.g. "SpecialObjectFilter = ANY +INFANTRY -HERO ENEMIES").
// Reimplemented from game.dat (parser 0x0076392f, match 0x00763543 / 0x0076379c).
#pragma once

#include "Common/AsciiString.h"
#include "Common/KindOf.h"
#include <vector>

class INI;
class Object;
class ThingTemplate;

class ObjectFilter
{
public:
	enum Mode { MODE_UNSET = 0, MODE_NONE = 1, MODE_ANY = 2, MODE_ALL = 3 };
	enum { REL_ALLIES = 1, REL_ENEMIES = 2, REL_NEUTRAL = 4, REL_SAME_PLAYER = 8 };
	enum { ALIGN_ANY = 0, ALIGN_EVIL = 1, ALIGN_GOOD = 2 };

	ObjectFilter() { clear(); }
	void clear();
	Bool isEmpty() const { return m_empty; }

	/// does obj pass the filter? 'source' is the object the relationship words are relative to (may be null)
	Bool matches(const Object* obj, const Object* source) const;
	/// template-only test (relationship words are ignored when no objects are given)
	Bool matchesTemplate(const ThingTemplate* tmpl) const;

	static void parseFromINI(INI* ini, void* instance, void* store, const void* userData);

private:
	Bool matchCommon(const ThingTemplate* tmpl, const KindOfMaskType& kinds) const;

	std::vector<AsciiString> m_includeNames;	///< "+TemplateName"
	std::vector<AsciiString> m_excludeNames;	///< "-TemplateName"
	KindOfMaskType m_includeKinds;					///< "+KINDOF"
	KindOfMaskType m_excludeKinds;					///< "-KINDOF"
	Int m_mode;
	UnsignedInt m_relationships;
	Int m_alignment;
	Bool m_empty;
};
