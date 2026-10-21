// bfme-native: see Stances.h
#include "PreRTS.h"

#include "Common/INI.h"
#include "Common/Xfer.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Stances.h"

StanceStore* TheStanceStore = nullptr;

static AsciiString lowerKey(const AsciiString& s)
{
	AsciiString r = s;
	r.toLower();
	return r;
}

const StanceEntry* StanceTemplate::find(const AsciiString& stance) const
{
	for (size_t i = 0; i < m_stances.size(); ++i)
		if (m_stances[i].m_name.compareNoCase(stance) == 0)
			return &m_stances[i];
	return nullptr;
}

const StanceTemplate* StanceStore::find(const AsciiString& name) const
{
	std::map<AsciiString, StanceTemplate>::const_iterator it = m_templates.find(lowerKey(name));
	return it == m_templates.end() ? nullptr : &it->second;
}

static void parseSkipBlock(INI* ini, void*, void*, const void*)
{
	static const FieldParse none[] = { { nullptr, nullptr, nullptr, 0 } };
	char dummy[16];
	ini->initFromINI(dummy, none);
}

static void parseStance(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fp[] =
	{
		{ "AttributeModifier",	INI::parseAsciiString,	nullptr, offsetof( StanceEntry, m_attributeModifier ) },
		{ "MeleeBehavior",			parseSkipBlock,					nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	StanceTemplate* t = (StanceTemplate*)instance;
	StanceEntry e;
	e.m_name = ini->getNextToken();
	ini->initFromINI(&e, fp);
	t->m_stances.push_back(e);
}

void StanceStore::parseStanceTemplateDefinition(INI* ini)
{
	static const FieldParse fp[] =
	{
		{ "Stance",	parseStance,	nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	StanceTemplate t;
	t.m_name = ini->getNextToken();
	ini->initFromINI(&t, fp);
	TheStanceStore->m_templates[lowerKey(t.m_name)] = t;
}

void INI::parseStanceTemplateDefinition( INI *ini )
{
	StanceStore::parseStanceTemplateDefinition(ini);
}

//-------------------------------------------------------------------------------------------------
void StancesBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "StanceTemplate",	INI::parseAsciiString,	nullptr, offsetof( StancesBehaviorModuleData, m_stanceTemplate ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

StancesBehavior::StancesBehavior( Thing *thing, const ModuleData* moduleData ) : BehaviorModule( thing, moduleData ) {}
StancesBehavior::~StancesBehavior() {}
void StancesBehavior::crc( Xfer *xfer ) { BehaviorModule::crc( xfer ); }
void StancesBehavior::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	BehaviorModule::xfer( xfer );
	xfer->xferAsciiString(&m_stance);
}
void StancesBehavior::loadPostProcess() { BehaviorModule::loadPostProcess(); }

StancesBehavior* StancesBehavior::findOn(const Object* obj)
{
	for (BehaviorModule** m = const_cast<Object*>(obj)->getBehaviorModules(); *m; ++m)
	{
		StancesBehavior* s = dynamic_cast<StancesBehavior*>(*m);
		if (s)
			return s;
	}
	return nullptr;
}

void StancesBehavior::applyModifier(const AsciiString& name, Bool add)
{
	const AttributeModifierList* list = name.isNotEmpty() ? TheAttributeModifierStore->findList(name) : nullptr;
	if (list == nullptr)
		return;
	Object* me = getObject();
	std::vector<Object*> targets;
	HordeContain* horde = HordeContain::findOn(me);
	if (horde)
	{
		for (size_t i = 0; i < horde->getMembers().size(); ++i)
		{
			Object* m = TheGameLogic->findObjectByID(horde->getMembers()[i]);
			if (m)
				targets.push_back(m);
		}
	}
	else
	{
		targets.push_back(me);
	}
	for (size_t i = 0; i < targets.size(); ++i)
	{
		if (add)
			TheAttributeModifierStore->apply(targets[i], list, me->getID(), 0, TRUE);
		else
			TheAttributeModifierStore->remove(targets[i], list);
	}
}

Bool StancesBehavior::setStance(const AsciiString& stance)
{
	const StanceTemplate* t = TheStanceStore ? TheStanceStore->find(getStancesBehaviorModuleData()->m_stanceTemplate) : nullptr;
	const StanceEntry* next = (t && stance.isNotEmpty()) ? t->find(stance) : nullptr;
	if (stance.isNotEmpty() && next == nullptr)
		return FALSE;
	const StanceEntry* prev = (t && m_stance.isNotEmpty()) ? t->find(m_stance) : nullptr;
	if (prev)
		applyModifier(prev->m_attributeModifier, FALSE);
	if (next)
		applyModifier(next->m_attributeModifier, TRUE);
	m_stance = stance;
	return TRUE;
}
