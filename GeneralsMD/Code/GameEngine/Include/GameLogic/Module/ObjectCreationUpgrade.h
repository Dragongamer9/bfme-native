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

// FILE: ObjectCreationUpgrade.h ///////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, April 2002
// Desc:	 upgrades that spawn OCLs
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;
class Player;
class ObjectCreationList;

//-------------------------------------------------------------------------------------------------
/** bfme-native: RotWK's ObjectCreationUpgrade also spawns a ThingToSpawn at an Offset/Angle,
	after an optional Delay, and grants or removes upgrades (object or player ones). */
//-------------------------------------------------------------------------------------------------
class ObjectCreationUpgradeModuleData : public UpdateModuleData
{

public:

	ObjectCreationUpgradeModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

	UpgradeMuxData m_upgradeMuxData;
	const ObjectCreationList *m_ocl;			///< the object creation list to make
	AsciiString m_thingToSpawn;
	Coord3D m_offset;
	Real m_angle;													///< radians, added to the owner's orientation
	UnsignedInt m_delay;									///< frames
	UnsignedInt m_fadeInTime;
	std::vector<AsciiString> m_grantUpgrades;
	std::vector<AsciiString> m_removeUpgrades;
	Bool m_destroyWhenSold;
	Bool m_useBuildingProduction;
	AsciiString m_deathAnimAndDuration;

};

//-------------------------------------------------------------------------------------------------
/** The OCL upgrade module */
//-------------------------------------------------------------------------------------------------
class ObjectCreationUpgrade : public UpdateModule, public UpgradeMux
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ObjectCreationUpgrade, "ObjectCreationUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ObjectCreationUpgrade, ObjectCreationUpgradeModuleData );

public:

	ObjectCreationUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }
	virtual UpgradeModuleInterface* getUpgrade() override { return this; }
	virtual void onDelete() override;																///< we have some work to do when this module goes away
	virtual UpdateSleepTime update() override;

protected:

	virtual void upgradeImplementation() override; ///< Here's the actual work of Upgrading
	virtual Bool isSubObjectsUpgrade() override { return false; }
	virtual void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const override
	{
		getObjectCreationUpgradeModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
	}
	virtual void performUpgradeFX() override
	{
		getObjectCreationUpgradeModuleData()->m_upgradeMuxData.performUpgradeFX(getObject());
	}
	virtual void processUpgradeRemoval() override
	{
		getObjectCreationUpgradeModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval(getObject());
	}
	virtual Bool requiresAllActivationUpgrades() const override
	{
		return getObjectCreationUpgradeModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
	}

private:

	void doCreation();

	UnsignedInt m_pendingFrame;		///< 0 = nothing pending
	ObjectID m_spawnedID;

};
