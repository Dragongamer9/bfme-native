// bfme-native: see BfmeGenericSpecialPower.h
#include "PreRTS.h"

#define DEFINE_WEAPONSLOTTYPE_NAMES

#include "Common/Xfer.h"
#include "GameLogic/Module/BfmeGenericSpecialPower.h"
#include "GameLogic/AttributeModifier.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/PlayerList.h"
#include "Common/Upgrade.h"
#include "GameClient/FXList.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/BodyModule.h"
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

#define BFME_SP_SNAPSHOT(Cls) \
	Cls::~Cls() {} \
	void Cls::crc( Xfer *xfer ) { SpecialPowerModule::crc( xfer ); } \
	void Cls::xfer( Xfer *xfer ) { XferVersion cv = 1; XferVersion v = cv; xfer->xferVersion( &v, cv ); SpecialPowerModule::xfer( xfer ); } \
	void Cls::loadPostProcess() { SpecialPowerModule::loadPostProcess(); }

//=================================================================================================
// PlayerHealSpecialPower
//=================================================================================================
void PlayerHealSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialPowerModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "HealAffects",		KindOfMaskType::parseFromINI,			nullptr, offsetof( PlayerHealSpecialPowerModuleData, m_healAffects ) },
		{ "HealAmount",			INI::parseReal,										nullptr, offsetof( PlayerHealSpecialPowerModuleData, m_healAmount ) },
		{ "HealRadius",			INI::parseReal,										nullptr, offsetof( PlayerHealSpecialPowerModuleData, m_healRadius ) },
		{ "HealAsPercent",	INI::parseBool,										nullptr, offsetof( PlayerHealSpecialPowerModuleData, m_healAsPercent ) },
		{ "HealFX",					INI::parseFXList,									nullptr, offsetof( PlayerHealSpecialPowerModuleData, m_healFX ) },
		{ "HealOCL",				INI::parseObjectCreationList,			nullptr, offsetof( PlayerHealSpecialPowerModuleData, m_healOCL ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

PlayerHealSpecialPower::PlayerHealSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData ) {}
BFME_SP_SNAPSHOT(PlayerHealSpecialPower)

void PlayerHealSpecialPower::bfmeOnTrigger( const Coord3D *location )
{
	SpecialPowerModule::bfmeOnTrigger(location);
	const PlayerHealSpecialPowerModuleData* d = getPlayerHealSpecialPowerModuleData();
	Object* me = getObject();
	Player* p = me->getControllingPlayer();
	const Coord3D* at = location ? location : me->getPosition();
	if (p == nullptr)
		return;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(at, d->m_healRadius, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->first(); o; o = iter->next())
	{
		if (o->isEffectivelyDead() || o->getControllingPlayer() != p)
			continue;
		if (d->m_healAffects.any() && !o->getTemplate()->getKindOfMask().anyIntersectionWith(d->m_healAffects))
			continue;
		BodyModuleInterface* body = o->getBodyModule();
		if (body == nullptr)
			continue;
		const Real amount = d->m_healAsPercent || d->m_healAmount <= 1.0f ? body->getMaxHealth() * d->m_healAmount : d->m_healAmount;
		body->internalChangeHealth(min(amount, body->getMaxHealth() - body->getHealth()));
		if (d->m_healFX)
			FXList::doFXObj(d->m_healFX, o);
	}
	if (d->m_healOCL)
		ObjectCreationList::create(d->m_healOCL, me, at, at, (Bool)FALSE);
}

//=================================================================================================
// PlayerUpgradeSpecialPower
//=================================================================================================
void PlayerUpgradeSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialPowerModuleData::buildFieldParse(p);
	static const FieldParse fp[] =
	{
		{ "UpgradeName",	INI::parseAsciiString,	nullptr, offsetof( PlayerUpgradeSpecialPowerModuleData, m_upgradeName ) },
		{ "AffectAllies",	INI::parseBool,					nullptr, offsetof( PlayerUpgradeSpecialPowerModuleData, m_affectAllies ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fp);
}

PlayerUpgradeSpecialPower::PlayerUpgradeSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData ) {}
BFME_SP_SNAPSHOT(PlayerUpgradeSpecialPower)

void PlayerUpgradeSpecialPower::bfmeOnTrigger( const Coord3D *location )
{
	SpecialPowerModule::bfmeOnTrigger(location);
	const PlayerUpgradeSpecialPowerModuleData* d = getPlayerUpgradeSpecialPowerModuleData();
	const UpgradeTemplate* ut = d->m_upgradeName.isNotEmpty() ? TheUpgradeCenter->findUpgrade(d->m_upgradeName) : nullptr;
	Player* p = getObject()->getControllingPlayer();
	if (ut == nullptr || p == nullptr)
		return;
	if (!p->hasUpgradeComplete(ut))
		p->addUpgrade(ut, UPGRADE_STATUS_COMPLETE);
	if (d->m_affectAllies)
	{
		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player* other = ThePlayerList->getNthPlayer(i);
			if (other && other != p && p->getRelationship(other->getDefaultTeam()) == ALLIES && !other->hasUpgradeComplete(ut))
				other->addUpgrade(ut, UPGRADE_STATUS_COMPLETE);
		}
	}
}
