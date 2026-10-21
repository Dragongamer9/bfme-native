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
