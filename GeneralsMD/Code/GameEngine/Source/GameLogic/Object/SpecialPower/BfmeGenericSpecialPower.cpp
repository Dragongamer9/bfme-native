// bfme-native: see BfmeGenericSpecialPower.h
#include "PreRTS.h"

#define DEFINE_WEAPONSLOTTYPE_NAMES

#include "Common/Xfer.h"
#include "GameLogic/Module/BfmeGenericSpecialPower.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/WeaponSet.h"

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

//=================================================================================================
// WeaponModeSpecialPower
//=================================================================================================
void WeaponModeSpecialPowerModuleData::parseWeaponSetFlags(INI* ini, void*, void* store, const void*)
{
	std::vector<Int>* v = (std::vector<Int>*)store;
	for (const char* tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull())
	{
		const Int i = INI::scanIndexList(tok, WeaponSetFlags::getBitNames());
		v->push_back(i);
	}
}

void WeaponModeSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialPowerModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "Duration",						INI::parseDurationUnsignedInt,	nullptr, offsetof( WeaponModeSpecialPowerModuleData, m_duration ) },
		{ "AttributeModifier",	INI::parseAsciiString,					nullptr, offsetof( WeaponModeSpecialPowerModuleData, m_attributeModifier ) },
		{ "LockWeaponSlot",			INI::parseIndexList,						TheWeaponSlotTypeNames, offsetof( WeaponModeSpecialPowerModuleData, m_lockWeaponSlot ) },
		{ "WeaponSetFlags",			WeaponModeSpecialPowerModuleData::parseWeaponSetFlags,	nullptr, offsetof( WeaponModeSpecialPowerModuleData, m_weaponSetFlags ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

WeaponModeSpecialPower::WeaponModeSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData )
{
}

WeaponModeSpecialPower::~WeaponModeSpecialPower()
{
}

void WeaponModeSpecialPower::bfmeOnTrigger( const Coord3D *location )
{
	SpecialPowerModule::bfmeOnTrigger(location);
	const WeaponModeSpecialPowerModuleData* d = getWeaponModeSpecialPowerModuleData();
	Object* me = getObject();
	const UnsignedInt dur = d->m_duration;

	const AttributeModifierList* list = d->m_attributeModifier.isNotEmpty() ? TheAttributeModifierStore->findList(d->m_attributeModifier) : nullptr;
	if (list)
		TheAttributeModifierStore->apply(me, list, me->getID(), dur);
	if (d->m_lockWeaponSlot >= 0)
		me->setWeaponLock((WeaponSlotType)d->m_lockWeaponSlot, LOCKED_PERMANENTLY);
	for (size_t i = 0; i < d->m_weaponSetFlags.size(); ++i)
		me->setWeaponSetFlag((WeaponSetType)d->m_weaponSetFlags[i]);

	if (dur == 0)
		return;
	const Int lock = d->m_lockWeaponSlot;
	const std::vector<Int> flags = d->m_weaponSetFlags;
	TheAttributeModifierStore->schedule(me, dur, [lock, flags](Object* o)
	{
		if (lock >= 0)
		{
			// back to the primary weapon (choosing again on the next target)
			o->releaseWeaponLock(LOCKED_PERMANENTLY);
			o->setWeaponLock(PRIMARY_WEAPON, LOCKED_TEMPORARILY);
			o->releaseWeaponLock(LOCKED_TEMPORARILY);
		}
		for (size_t i = 0; i < flags.size(); ++i)
			o->clearWeaponSetFlag((WeaponSetType)flags[i]);
	});
}

void WeaponModeSpecialPower::crc( Xfer *xfer )
{
	SpecialPowerModule::crc( xfer );
}

void WeaponModeSpecialPower::xfer( Xfer *xfer )
{
	XferVersion cv = 1;
	XferVersion v = cv;
	xfer->xferVersion( &v, cv );
	SpecialPowerModule::xfer( xfer );
}

void WeaponModeSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
