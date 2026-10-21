// bfme-native: see BfmeGenericSpecialPower.h
#include "PreRTS.h"

#include "Common/Xfer.h"
#include "GameLogic/Module/BfmeGenericSpecialPower.h"

BfmeGenericSpecialPower::BfmeGenericSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData )
{
}

BfmeGenericSpecialPower::~BfmeGenericSpecialPower()
{
}

void BfmeGenericSpecialPower::crc( Xfer *xfer )
{
	SpecialPowerModule::crc( xfer );
}

void BfmeGenericSpecialPower::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	SpecialPowerModule::xfer( xfer );
}

void BfmeGenericSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
