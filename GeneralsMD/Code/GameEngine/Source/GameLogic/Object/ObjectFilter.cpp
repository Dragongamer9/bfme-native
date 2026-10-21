// bfme-native: RotWK ObjectFilter, see ObjectFilter.h
#include "PreRTS.h"

#include "GameLogic/ObjectFilter.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"

void ObjectFilter::clear()
{
	m_includeNames.clear();
	m_excludeNames.clear();
	m_includeKinds.clear();
	m_excludeKinds.clear();
	m_mode = MODE_ALL;
	m_relationships = 0;
	m_alignment = ALIGN_ANY;
	m_empty = TRUE;
}

static Bool nameInList(const std::vector<AsciiString>& v, const AsciiString& n)
{
	for (size_t i = 0; i < v.size(); ++i)
		if (v[i].compareNoCase(n) == 0)
			return TRUE;
	return FALSE;
}

Bool ObjectFilter::matchCommon(const ThingTemplate* tmpl, const KindOfMaskType& kinds) const
{
	if (tmpl)
	{
		if (nameInList(m_includeNames, tmpl->getName()))
			return TRUE;
		if (nameInList(m_excludeNames, tmpl->getName()))
			return FALSE;
	}
	if (m_excludeKinds.any() && kinds.anyIntersectionWith(m_excludeKinds))
		return FALSE;
	switch (m_mode)
	{
		case MODE_NONE:	// "NONE +A +B": must have every listed kind
			return m_includeKinds.any() && kinds.testForAll(m_includeKinds);
		case MODE_ANY:	// "ANY +A +B": at least one listed kind
			return m_includeKinds.any() && kinds.anyIntersectionWith(m_includeKinds);
		default:
			return TRUE;
	}
}

Bool ObjectFilter::matchesTemplate(const ThingTemplate* tmpl) const
{
	if (tmpl == nullptr)
		return FALSE;
	return matchCommon(tmpl, tmpl->getKindOfMask());
}

Bool ObjectFilter::matches(const Object* obj, const Object* source) const
{
	if (obj == nullptr)
		return FALSE;

	if (m_alignment != ALIGN_ANY)
	{
		const Player* p = obj->getControllingPlayer();
		const PlayerTemplate* pt = p ? p->getPlayerTemplate() : nullptr;
		const Bool evil = pt && pt->isEvil();
		if ((m_alignment == ALIGN_EVIL) != evil)
			return FALSE;
	}

	if (m_relationships != 0)
	{
		if (source == nullptr)
			return FALSE;
		Relationship r = source->getRelationship(obj);
		if (r == ENEMIES)
		{
			if ((m_relationships & REL_ENEMIES) == 0)
				return FALSE;
		}
		else if (r == NEUTRAL)
		{
			if ((m_relationships & REL_NEUTRAL) == 0)
				return FALSE;
		}
		else
		{
			if ((m_relationships & REL_ALLIES) == 0)
			{
				if ((m_relationships & REL_SAME_PLAYER) == 0 || obj->getControllingPlayer() != source->getControllingPlayer())
					return FALSE;
			}
		}
	}

	return matchCommon(obj->getTemplate(), obj->getTemplate()->getKindOfMask());
}

void ObjectFilter::parseFromINI(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	ObjectFilter* f = (ObjectFilter*)store;
	f->clear();
	Bool first = TRUE;
	Bool haveMode = FALSE;
	for (const char* tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull(), first = FALSE)
	{
		if (stricmp(tok, "ALL") == 0 || stricmp(tok, "ANY") == 0 || stricmp(tok, "NONE") == 0)
		{
			if (!first)
				throw INI_INVALID_DATA;
			f->m_mode = stricmp(tok, "ALL") == 0 ? MODE_ALL : (stricmp(tok, "ANY") == 0 ? MODE_ANY : MODE_NONE);
			haveMode = TRUE;
			continue;
		}
		if (!haveMode)
			throw INI_INVALID_DATA;	// "You must specify ALL, ANY or NONE first"
		if (stricmp(tok, "ALLIES") == 0) f->m_relationships |= REL_ALLIES;
		else if (stricmp(tok, "ENEMIES") == 0) f->m_relationships |= REL_ENEMIES;
		else if (stricmp(tok, "NEUTRAL") == 0) f->m_relationships |= REL_NEUTRAL;
		else if (stricmp(tok, "SAME_PLAYER") == 0) f->m_relationships |= REL_SAME_PLAYER;
		else if (stricmp(tok, "EVIL") == 0) f->m_alignment = ALIGN_EVIL;
		else if (stricmp(tok, "GOOD") == 0) f->m_alignment = ALIGN_GOOD;
		else if (tok[0] == '+' || tok[0] == '-')
		{
			const Bool plus = tok[0] == '+';
			Int k = -1;
			const char* const* names = KindOfMaskType::getBitNames();
			for (Int i = 0; names[i]; ++i)
				if (stricmp(names[i], tok + 1) == 0) { k = i; break; }
			if (k >= 0)
				(plus ? f->m_includeKinds : f->m_excludeKinds).set((KindOfType)k);
			else
				(plus ? f->m_includeNames : f->m_excludeNames).push_back(AsciiString(tok + 1));
		}
		else
			throw INI_INVALID_DATA;
	}
	f->m_empty = FALSE;
}
