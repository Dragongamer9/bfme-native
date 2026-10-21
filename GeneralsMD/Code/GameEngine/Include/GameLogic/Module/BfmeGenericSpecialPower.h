// bfme-native: RotWK uses "SpecialPowerModule" directly as a concrete module (buffs, leadership
// shouts, ...); everything it does lives in the base class (SpecialPowerModule::bfmeOnTrigger).
#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"
#include "Common/KindOf.h"

class FXList;
class ObjectCreationList;

class BfmeGenericSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( BfmeGenericSpecialPower, "BfmeGenericSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( BfmeGenericSpecialPower, SpecialPowerModuleData )

public:
	BfmeGenericSpecialPower( Thing *thing, const ModuleData* moduleData );
};

//-------------------------------------------------------------------------------------------------
/** WeaponModeSpecialPowerUpdate: a timed stance -- for Duration the unit gets AttributeModifier,
	its weapon is locked to LockWeaponSlot and WeaponSetFlags are set (knife fighters, watch towers). */
class WeaponModeSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	WeaponModeSpecialPowerModuleData() : m_duration(0), m_lockWeaponSlot(-1) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseWeaponSetFlags(INI* ini, void* instance, void* store, const void* userData);
	UnsignedInt m_duration;
	AsciiString m_attributeModifier;
	Int m_lockWeaponSlot;
	std::vector<Int> m_weaponSetFlags;
};

class WeaponModeSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( WeaponModeSpecialPower, "WeaponModeSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( WeaponModeSpecialPower, WeaponModeSpecialPowerModuleData )

public:
	WeaponModeSpecialPower( Thing *thing, const ModuleData* moduleData );
	virtual void bfmeOnTrigger( const Coord3D *location ) override;
};

//-------------------------------------------------------------------------------------------------
/** PlayerHealSpecialPower: spellbook heal -- heals the player's units of HealAffects kinds within
	HealRadius of the target point (HealAmount: fraction of max health with HealAsPercent). */
class PlayerHealSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	PlayerHealSpecialPowerModuleData() : m_healAmount(0.0f), m_healRadius(0.0f), m_healAsPercent(FALSE), m_healFX(nullptr), m_healOCL(nullptr) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	KindOfMaskType m_healAffects;
	Real m_healAmount;
	Real m_healRadius;
	Bool m_healAsPercent;
	const FXList* m_healFX;
	const ObjectCreationList* m_healOCL;
};

class PlayerHealSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PlayerHealSpecialPower, "PlayerHealSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( PlayerHealSpecialPower, PlayerHealSpecialPowerModuleData )
public:
	PlayerHealSpecialPower( Thing *thing, const ModuleData* moduleData );
	virtual void bfmeOnTrigger( const Coord3D *location ) override;
};

//-------------------------------------------------------------------------------------------------
/** PlayerUpgradeSpecialPower: spellbook power that grants the player an upgrade. */
class PlayerUpgradeSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	PlayerUpgradeSpecialPowerModuleData() : m_affectAllies(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	AsciiString m_upgradeName;
	Bool m_affectAllies;
};

class PlayerUpgradeSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PlayerUpgradeSpecialPower, "PlayerUpgradeSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( PlayerUpgradeSpecialPower, PlayerUpgradeSpecialPowerModuleData )
public:
	PlayerUpgradeSpecialPower( Thing *thing, const ModuleData* moduleData );
	virtual void bfmeOnTrigger( const Coord3D *location ) override;
};
