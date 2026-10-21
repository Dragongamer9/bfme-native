// bfme-native: RotWK AttributeModifierUpgrade -- puts a ModifierList on the object while the
// triggering upgrade is present.
#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class AttributeModifierUpgradeModuleData : public UpgradeModuleData
{
public:
	AttributeModifierUpgradeModuleData() {}
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_attributeModifier;
	AsciiString m_customAnimAndDuration;	///< parsed, not used yet
};

class AttributeModifierUpgrade : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttributeModifierUpgrade, "AttributeModifierUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttributeModifierUpgrade, AttributeModifierUpgradeModuleData );

public:
	AttributeModifierUpgrade( Thing *thing, const ModuleData* moduleData );

	virtual Bool resetUpgrade( const UpgradeMaskType& keyMask ) override;

protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }
};
