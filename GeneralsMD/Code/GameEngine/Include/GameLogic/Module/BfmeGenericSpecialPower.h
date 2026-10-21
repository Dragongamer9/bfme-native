// bfme-native: RotWK uses "SpecialPowerModule" directly as a concrete module (buffs, leadership
// shouts, ...); everything it does lives in the base class (SpecialPowerModule::bfmeOnTrigger).
#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

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
