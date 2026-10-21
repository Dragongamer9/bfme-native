// bfme-native: RotWK AttributeModifierAuraUpdate -- every RefreshDelay, puts a ModifierList on
// everything in Range that passes ObjectFilter (leadership, banners, wells, ...).
#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/ObjectFilter.h"

class AttributeModifierAuraUpdateModuleData : public UpdateModuleData
{
public:
	AttributeModifierAuraUpdateModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_bonusName;
	Bool m_startsActive;
	UnsignedInt m_refreshDelay;
	Real m_range;
	ObjectFilter m_objectFilter;
	std::vector<AsciiString> m_triggeredBy;
	std::vector<AsciiString> m_conflictsWith;
	Bool m_requiresAllTriggers;
	Bool m_targetEnemy;
	Bool m_allowSelf;
	Bool m_runWhileDead;
	Bool m_affectContainedOnly;
	Int m_maxActiveRank;
	ModelConditionFlags m_requiredConditions;
	AsciiString m_antiCategory;	///< parsed, not used yet
};

class AttributeModifierAuraUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttributeModifierAuraUpdate, "AttributeModifierAuraUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttributeModifierAuraUpdate, AttributeModifierAuraUpdateModuleData )

public:
	AttributeModifierAuraUpdate( Thing *thing, const ModuleData* moduleData );

	virtual UpdateSleepTime update() override;

private:
	Bool isActive() const;
};
