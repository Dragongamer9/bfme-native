// bfme-native: RotWK experience levels.
//
// RotWK replaces Zero Hour's four veterancy levels with up to ten ranks per unit type, defined in
// ExperienceLevels.ini blocks:
//   ExperienceLevel StrongWarriorHeroGoodLevel2
//     TargetNames = <template names>   RequiredExperience = N   ExperienceAward = N   Rank = 2
//     Upgrades = Upgrade_Level_2       AttributeModifiers = ...  LevelUpFx = FX:...
//   End
// Reaching a rank grants its Upgrades and (permanent) AttributeModifiers; rank 1 is given at creation.
#pragma once

#include "Common/AsciiString.h"
#include "Common/SubsystemInterface.h"
#include <map>
#include <vector>

class INI;
class Object;
class ThingTemplate;
class FXList;

struct ExperienceLevel
{
	ExperienceLevel() : m_requiredExperience(0), m_experienceAward(0), m_rank(0), m_levelUpFX(nullptr) {}
	AsciiString m_name;
	std::vector<AsciiString> m_targetNames;
	Int m_requiredExperience;
	Int m_experienceAward;
	Int m_rank;
	std::vector<AsciiString> m_upgrades;
	std::vector<AsciiString> m_attributeModifiers;
	const FXList* m_levelUpFX;
	AsciiString m_unusedString;
};

class ExperienceLevelStore : public SubsystemInterface
{
public:
	ExperienceLevelStore() {}
	virtual ~ExperienceLevelStore();
	virtual void init() {}
	virtual void reset() {}
	virtual void update() {}
	virtual void postProcessLoad() override;

	static void parseExperienceLevelDefinition(INI* ini);

	/// the ranks of a template, sorted by Rank (empty when it has none)
	const std::vector<const ExperienceLevel*>& getLevels(const ThingTemplate* t) const;
	const ExperienceLevel* getLevel(const ThingTemplate* t, Int rank) const;
	Int getMaxRank(const ThingTemplate* t) const;

	/// the rank that 'experience' points buy for this template
	Int rankForExperience(const ThingTemplate* t, Int experience) const;

	/// grant everything ranks (oldRank, newRank] give
	void onRankGained(Object* obj, Int oldRank, Int newRank, Bool provideFeedback) const;

private:
	std::vector<ExperienceLevel*> m_levels;
	std::map<AsciiString, std::vector<const ExperienceLevel*> > m_byTemplate;	///< lower-case template name
};

extern ExperienceLevelStore* TheExperienceLevelStore;
