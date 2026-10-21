// bfme-native: see ExperienceLevel.h
#include "PreRTS.h"

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/ExperienceLevel.h"
#include "GameLogic/Object.h"
#include <algorithm>

ExperienceLevelStore* TheExperienceLevelStore = nullptr;

static AsciiString lowerKey(const AsciiString& s)
{
	AsciiString r = s;
	r.toLower();
	return r;
}

ExperienceLevelStore::~ExperienceLevelStore()
{
	for (size_t i = 0; i < m_levels.size(); ++i)
		delete m_levels[i];
}

static void parseFXName(INI* ini, void*, void* store, const void*)
{
	// "FX:GandalfLevelUp1FX" or "GandalfLevelUp1FX"
	const char* tok = ini->getNextTokenOrNull();
	if (tok == nullptr)
		return;
	if (strnicmp(tok, "FX:", 3) == 0)
		tok += 3;
	if (*tok == 0)
		tok = ini->getNextTokenOrNull();
	*(const FXList**)store = (tok && TheFXListStore) ? TheFXListStore->findFXList(tok) : nullptr;
}

static void parseSkipBlock(INI* ini, void*, void*, const void*)
{
	// SelectionDecal ... End
	static const FieldParse none[] = { { nullptr, nullptr, nullptr, 0 } };
	char dummy[16];
	ini->initFromINI(dummy, none);
}

static void parseLine(INI* ini, void*, void*, const void*)
{
	while (ini->getNextTokenOrNull()) {}
}

void ExperienceLevelStore::parseExperienceLevelDefinition(INI* ini)
{
	static const FieldParse fp[] =
	{
		{ "TargetNames",								INI::parseAsciiStringVectorAppend,	nullptr, offsetof( ExperienceLevel, m_targetNames ) },
		{ "RequiredExperience",					INI::parseInt,											nullptr, offsetof( ExperienceLevel, m_requiredExperience ) },
		{ "ExperienceAward",						INI::parseInt,											nullptr, offsetof( ExperienceLevel, m_experienceAward ) },
		{ "Rank",												INI::parseInt,											nullptr, offsetof( ExperienceLevel, m_rank ) },
		{ "Upgrades",										INI::parseAsciiStringVectorAppend,	nullptr, offsetof( ExperienceLevel, m_upgrades ) },
		{ "AttributeModifiers",					INI::parseAsciiStringVectorAppend,	nullptr, offsetof( ExperienceLevel, m_attributeModifiers ) },
		{ "LevelUpFx",									parseFXName,												nullptr, offsetof( ExperienceLevel, m_levelUpFX ) },
		{ "SelectionDecal",							parseSkipBlock,											nullptr, 0 },
		{ "InformUpdateModule",					parseLine,													nullptr, 0 },
		{ "LevelUpTintColor",						parseLine,													nullptr, 0 },
		{ "LevelUpTintPreColorTime",		parseLine,													nullptr, 0 },
		{ "LevelUpTintPostColorTime",		parseLine,													nullptr, 0 },
		{ "LevelUpTintSustainColorTime",	parseLine,												nullptr, 0 },
		{ "ShowLevelUpTint",						parseLine,													nullptr, 0 },
		{ "LevelUpOCL",									parseLine,													nullptr, 0 },
		{ "SkillPointsAward",						parseLine,													nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ExperienceLevel* l = new ExperienceLevel;
	l->m_name = ini->getNextToken();
	ini->initFromINI(l, fp);
	TheExperienceLevelStore->m_levels.push_back(l);
}

void INI::parseExperienceLevelDefinition( INI *ini )
{
	ExperienceLevelStore::parseExperienceLevelDefinition(ini);
}

void ExperienceLevelStore::postProcessLoad()
{
	m_byTemplate.clear();
	for (size_t i = 0; i < m_levels.size(); ++i)
		for (size_t j = 0; j < m_levels[i]->m_targetNames.size(); ++j)
			m_byTemplate[lowerKey(m_levels[i]->m_targetNames[j])].push_back(m_levels[i]);
	for (std::map<AsciiString, std::vector<const ExperienceLevel*> >::iterator it = m_byTemplate.begin(); it != m_byTemplate.end(); ++it)
	{
		std::vector<const ExperienceLevel*>& v = it->second;
		std::stable_sort(v.begin(), v.end(), [](const ExperienceLevel* a, const ExperienceLevel* b) { return a->m_rank < b->m_rank; });
		// a later definition of the same rank replaces the earlier one
		std::vector<const ExperienceLevel*> u;
		for (size_t i = 0; i < v.size(); ++i)
		{
			if (!u.empty() && u.back()->m_rank == v[i]->m_rank)
				u.back() = v[i];
			else
				u.push_back(v[i]);
		}
		v.swap(u);
	}
	fprintf(stderr, "[bfme] experience levels: %d definitions for %d unit types\n", (int)m_levels.size(), (int)m_byTemplate.size());
}

const std::vector<const ExperienceLevel*>& ExperienceLevelStore::getLevels(const ThingTemplate* t) const
{
	static const std::vector<const ExperienceLevel*> none;
	if (t == nullptr)
		return none;
	std::map<AsciiString, std::vector<const ExperienceLevel*> >::const_iterator it = m_byTemplate.find(lowerKey(t->getName()));
	if (it == m_byTemplate.end())
		return none;
	return it->second;
}

const ExperienceLevel* ExperienceLevelStore::getLevel(const ThingTemplate* t, Int rank) const
{
	const std::vector<const ExperienceLevel*>& v = getLevels(t);
	for (size_t i = 0; i < v.size(); ++i)
		if (v[i]->m_rank == rank)
			return v[i];
	return nullptr;
}

Int ExperienceLevelStore::getMaxRank(const ThingTemplate* t) const
{
	const std::vector<const ExperienceLevel*>& v = getLevels(t);
	return v.empty() ? 0 : v.back()->m_rank;
}

Int ExperienceLevelStore::rankForExperience(const ThingTemplate* t, Int experience) const
{
	const std::vector<const ExperienceLevel*>& v = getLevels(t);
	Int rank = v.empty() ? 0 : v.front()->m_rank;
	for (size_t i = 0; i < v.size(); ++i)
		if (experience >= v[i]->m_requiredExperience)
			rank = v[i]->m_rank;
	return rank;
}

void ExperienceLevelStore::onRankGained(Object* obj, Int oldRank, Int newRank, Bool provideFeedback) const
{
	const std::vector<const ExperienceLevel*>& v = getLevels(obj->getTemplate());
	for (size_t i = 0; i < v.size(); ++i)
	{
		const ExperienceLevel* l = v[i];
		if (l->m_rank <= oldRank || l->m_rank > newRank)
			continue;
		for (size_t u = 0; u < l->m_upgrades.size(); ++u)
		{
			const UpgradeTemplate* ut = TheUpgradeCenter->findUpgrade(l->m_upgrades[u]);
			if (ut == nullptr)
				continue;
			if (ut->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			{
				Player* p = obj->getControllingPlayer();
				if (p && !p->hasUpgradeComplete(ut))
					p->addUpgrade(ut, UPGRADE_STATUS_COMPLETE);
			}
			else if (!obj->hasUpgrade(ut))
			{
				obj->giveUpgrade(ut);
			}
		}
		for (size_t a = 0; a < l->m_attributeModifiers.size(); ++a)
		{
			const AttributeModifierList* list = TheAttributeModifierStore->findList(l->m_attributeModifiers[a]);
			if (list)
				TheAttributeModifierStore->apply(obj, list, obj->getID(), 0, TRUE);
		}
		if (provideFeedback && l->m_levelUpFX)
			FXList::doFXObj(l->m_levelUpFX, obj);
	}
}
