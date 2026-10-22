/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: ObjectCreationUpgrade.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, April 2002
// Desc:	 upgrades that spawn OCLs
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ModelState.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ObjectCreationUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgradeModuleData::ObjectCreationUpgradeModuleData()
{

	m_ocl = nullptr;
	m_offset.zero();
	m_angle = 0.0f;
	m_delay = 0;
	m_fadeInTime = 0;
	m_destroyWhenSold = FALSE;
	m_useBuildingProduction = FALSE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ void ObjectCreationUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "UpgradeObject", INI::parseObjectCreationList, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_ocl ) },
		{ "ThingToSpawn", INI::parseAsciiString, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_thingToSpawn ) },
		{ "Offset", INI::parseCoord3D, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_offset ) },
		{ "Angle", INI::parseAngleReal, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_angle ) },
		{ "Delay", INI::parseDurationUnsignedInt, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_delay ) },
		{ "FadeInTime", INI::parseDurationUnsignedInt, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_fadeInTime ) },
		{ "GrantUpgrade", INI::parseAsciiStringVectorAppend, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_grantUpgrades ) },
		{ "RemoveUpgrade", INI::parseAsciiStringVectorAppend, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_removeUpgrades ) },
		{ "DestroyWhenSold", INI::parseBool, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_destroyWhenSold ) },
		{ "UseBuildingProduction", INI::parseBool, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_useBuildingProduction ) },
		{ "DeathAnimAndDuration", INI::parseAsciiString, nullptr, offsetof( ObjectCreationUpgradeModuleData, m_deathAnimAndDuration ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
	p.add(UpgradeMuxData::getFieldParse(), offsetof( ObjectCreationUpgradeModuleData, m_upgradeMuxData ));

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgrade::ObjectCreationUpgrade( Thing *thing, const ModuleData* moduleData ) :
							UpdateModule( thing, moduleData )
{
	m_pendingFrame = 0;
	m_spawnedID = INVALID_ID;
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgrade::~ObjectCreationUpgrade()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::onDelete()
{
	// the spawned piece belongs to its owner (a tower's archer, a wall's gate): it goes when the owner is sold
	const ObjectCreationUpgradeModuleData *d = getObjectCreationUpgradeModuleData();
	if (d->m_destroyWhenSold && m_spawnedID != INVALID_ID && getObject()->getStatusBits().test( OBJECT_STATUS_SOLD ))
	{
		Object *spawned = TheGameLogic->findObjectByID( m_spawnedID );
		if (spawned)
			TheGameLogic->destroyObject( spawned );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::upgradeImplementation()
{
	const ObjectCreationUpgradeModuleData *d = getObjectCreationUpgradeModuleData();
	if (d->m_delay > 0)
	{
		m_pendingFrame = TheGameLogic->getFrame() + d->m_delay;
		setWakeFrame( getObject(), UPDATE_SLEEP( d->m_delay ) );
		return;
	}
	doCreation();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime ObjectCreationUpgrade::update()
{
	if (m_pendingFrame == 0 || TheGameLogic->getFrame() < m_pendingFrame)
		return m_pendingFrame ? UPDATE_SLEEP( m_pendingFrame - TheGameLogic->getFrame() ) : UPDATE_SLEEP_FOREVER;
	m_pendingFrame = 0;
	if (!getObject()->isEffectivelyDead())
		doCreation();
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::doCreation()
{
	const ObjectCreationUpgradeModuleData *d = getObjectCreationUpgradeModuleData();
	Object *me = getObject();

	// spawn everything in the OCL
	if (d->m_ocl)
		ObjectCreationList::create( d->m_ocl, me, nullptr );

	if (d->m_thingToSpawn.isNotEmpty())
	{
		const ThingTemplate *tmpl = TheThingFactory->findTemplate( d->m_thingToSpawn );
		if (tmpl && me->getTeam())
		{
			Object *obj = TheThingFactory->newObject( tmpl, me->getTeam() );
			if (obj)
			{
				const Real angle = me->getOrientation();
				const Real c = Cos( angle ), s = Sin( angle );
				Coord3D pos = *me->getPosition();
				pos.x += d->m_offset.x * c - d->m_offset.y * s;
				pos.y += d->m_offset.x * s + d->m_offset.y * c;
				pos.z += d->m_offset.z;
				obj->setPosition( &pos );
				obj->setOrientation( angle + d->m_angle );
				obj->setProducer( me );
				m_spawnedID = obj->getID();
			}
		}
	}

	Player *player = me->getControllingPlayer();
	for (size_t i = 0; i < d->m_grantUpgrades.size(); ++i)
	{
		const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade( d->m_grantUpgrades[i] );
		if (!u)
			continue;
		if (u->getUpgradeType() == UPGRADE_TYPE_PLAYER)
		{
			if (player)
				player->addUpgrade( u, UPGRADE_STATUS_COMPLETE );
		}
		else
			me->giveUpgrade( u );
	}
	for (size_t i = 0; i < d->m_removeUpgrades.size(); ++i)
	{
		const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade( d->m_removeUpgrades[i] );
		if (!u)
			continue;
		if (u->getUpgradeType() == UPGRADE_TYPE_PLAYER)
		{
			if (player)
				player->removeUpgrade( u );
		}
		else
			me->removeUpgrade( u );
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );
	UpgradeMux::upgradeMuxCRC( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: bfme-native: update module with a pending frame and the spawned object */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );
	UpgradeMux::upgradeMuxXfer( xfer );
	xfer->xferUnsignedInt( &m_pendingFrame );
	xfer->xferObjectID( &m_spawnedID );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();
	UpgradeMux::upgradeMuxLoadPostProcess();

}
