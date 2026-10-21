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

// FILE: ObjectStatusTypes.h /////////////////////////////////////////////////////////////////////////
// Author: Kris, May 2003
// Desc:   Object status types that are stackable using the BitSet system. Used to be ObjectStatusBits
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/BitFlags.h"
#include "Common/BitFlagsIO.h"

//-------------------------------------------------------------------------------------------------
/** Object status types */
//-------------------------------------------------------------------------------------------------
enum ObjectStatusTypes CPP_11(: Int)
{
	//These are saved. Do not insert or remove any!

	OBJECT_STATUS_NONE,									///< no status bit
	OBJECT_STATUS_DESTROYED,						///< has been destroyed, pending delete
	OBJECT_STATUS_CAN_ATTACK,						///< used by garrissoned buildings, is OR'ed with KINDOF_CAN_ATTACK in isAbleToAttack()
	OBJECT_STATUS_UNDER_CONSTRUCTION,		///< object is being constructed and is not yet complete
	OBJECT_STATUS_UNSELECTABLE,					///< This is a negative condition since these statuses are overrides.  ie their presence forces the condition, but their absence means nothing
	OBJECT_STATUS_NO_COLLISIONS,				///< object should be ignored for object-object collisions (but not object-ground); used for thing like collapsing parachutes that are intangible
	OBJECT_STATUS_NO_ATTACK,						///< Absolute override to being able to attack
	OBJECT_STATUS_AIRBORNE_TARGET,			///< InTheAir as far as AntiAir weapons are concerned only.
	OBJECT_STATUS_PARACHUTING,					///< object is on a parachute
	OBJECT_STATUS_REPULSOR,							///< object repulses "KINDOF_CAN_BE_REPULSED" objects.
	OBJECT_STATUS_HIJACKED,  						///< unit is in the possesion of an enemy criminal, call the authorities
	OBJECT_STATUS_AFLAME,								///< This object is on fire.
	OBJECT_STATUS_BURNED,								///< This object has already burned as much as it can.
	OBJECT_STATUS_WET,									///< object has been soaked with water
	OBJECT_STATUS_IS_FIRING_WEAPON,			///< Object is firing a weapon, now. Not true for special attacks. --Lorenzen
	OBJECT_STATUS_BRAKING,							///< Object is braking, and subverts the physics.
	OBJECT_STATUS_STEALTHED,						///< Object is currently "stealthed"
	OBJECT_STATUS_DETECTED,							///< Object is in range of a stealth-detector unit (meaningless if STEALTHED not set)
	OBJECT_STATUS_CAN_STEALTH,					///< Object has ability to stealth allowing the stealth update module to run.
	OBJECT_STATUS_SOLD,									///< Object is being sold
	OBJECT_STATUS_UNDERGOING_REPAIR,		///< Object is awaiting/undergoing a repair order that has been issued
	OBJECT_STATUS_RECONSTRUCTING,				///< Reconstructing
	OBJECT_STATUS_MASKED,								///< Masked objects are not selectable and targetable by players or AI
	OBJECT_STATUS_IS_ATTACKING,					///< Object is in the general Attack state (incl. aim, approach, etc.). Note that IS_FIRING_WEAPON and IS_AIMING_WEAPON is a subset of this!
	OBJECT_STATUS_IS_USING_ABILITY,			///< Object is in the process of preparing or firing a special ability.
	OBJECT_STATUS_IS_AIMING_WEAPON,			///< Object is aiming a weapon, now. Not true for special attacks.
	OBJECT_STATUS_NO_ATTACK_FROM_AI,		///< attacking this object may not be done from commandSource == CMD_FROM_AI
	OBJECT_STATUS_IGNORING_STEALTH,			///< temporarily ignoring all stealth bits. (used only for some special-case mine clearing stuff.)
	OBJECT_STATUS_IS_CARBOMB,						///< Object is now a carbomb.

	// TheSuperHackers @info New statuses added in Zero Hour
	// Note: Loading old save games that do not track these flags in objects will not recover them. Expect logic bugs.
	OBJECT_STATUS_DECK_HEIGHT_OFFSET,		///< Object factors deck height on top of ground altitude.
	OBJECT_STATUS_RIDER1,
	OBJECT_STATUS_RIDER2,
	OBJECT_STATUS_RIDER3,
	OBJECT_STATUS_RIDER4,
	OBJECT_STATUS_RIDER5,
	OBJECT_STATUS_RIDER6,
	OBJECT_STATUS_RIDER7,
	OBJECT_STATUS_RIDER8,
	OBJECT_STATUS_FAERIE_FIRE,			///< Anyone shooting at you shoots faster than normal
  OBJECT_STATUS_MISSILE_KILLING_SELF, ///< Object (likely a missile or bomb) is *BUSTING* its way through the *BUNKER*, building or ground, awaiting death at the bottom.
	OBJECT_STATUS_REASSIGN_PARKING,			///< Jet is trying to get a better parking assignment.
	OBJECT_STATUS_BOOBY_TRAPPED,				///< We need to know we have a booby trap on us so we can detonate it from many different code segments
	OBJECT_STATUS_IMMOBILE,							///< Do not move!
	OBJECT_STATUS_DISGUISED,						///< Object is disguised (a type of stealth)
	OBJECT_STATUS_DEPLOYED,							///< Object is deployed.
	// add more status types here and don't forget to add to the string table ObjectStatusMaskType::s_bitNameList[]

	// bfme-native: RotWK status bits
	OBJECT_STATUS_BFME_HIDDEN,
	OBJECT_STATUS_BFME_NO_AUTO_ACQUIRE,
	OBJECT_STATUS_BFME_IS_MELEE_ATTACKING,
	OBJECT_STATUS_BFME_GUARD_SELECTION,
	OBJECT_STATUS_BFME_LEASHED_RETURNING,
	OBJECT_STATUS_BFME_DEATH_1,
	OBJECT_STATUS_BFME_DEATH_2,
	OBJECT_STATUS_BFME_DEATH_3,
	OBJECT_STATUS_BFME_DEATH_4,
	OBJECT_STATUS_BFME_DEATH_5,
	OBJECT_STATUS_BFME_CONTESTED,
	OBJECT_STATUS_BFME_CONTESTING_BUILDING,
	OBJECT_STATUS_BFME_HORDE_MEMBER,
	OBJECT_STATUS_BFME_RIDERLESS,
	OBJECT_STATUS_BFME_RIDER_IS_PILOT,
	OBJECT_STATUS_BFME_RIDER1,
	OBJECT_STATUS_BFME_RIDER2,
	OBJECT_STATUS_BFME_RIDER3,
	OBJECT_STATUS_BFME_RIDER4,
	OBJECT_STATUS_BFME_RIDER5,
	OBJECT_STATUS_BFME_RIDER6,
	OBJECT_STATUS_BFME_RIDER7,
	OBJECT_STATUS_BFME_RIDER8,
	OBJECT_STATUS_BFME_FLEE_OFF_MAP,
	OBJECT_STATUS_BFME_NOT_IN_WORLD,
	OBJECT_STATUS_BFME_INAUDIBLE,
	OBJECT_STATUS_BFME_CHANTING,
	OBJECT_STATUS_BFME_ENRAGED,
	OBJECT_STATUS_BFME_CREATE_DRAWABLE_WITH_LOW_DETAIL,
	OBJECT_STATUS_BFME_SINKING,
	OBJECT_STATUS_BFME_RAMPAGING,
	OBJECT_STATUS_BFME_INSIDE_GARRISON,
	OBJECT_STATUS_BFME_UNATTACKABLE,
	OBJECT_STATUS_BFME_ENCLOSED,
	OBJECT_STATUS_BFME_TEMPORARILY_DEFECTED,
	OBJECT_STATUS_BFME_TAGGED,
	OBJECT_STATUS_BFME_DEPLOYING,
	OBJECT_STATUS_BFME_BLOODTHIRSTY,
	OBJECT_STATUS_BFME_PORTER_TAGGED,
	OBJECT_STATUS_BFME_GRAB_AND_DROP,
	OBJECT_STATUS_BFME_STAND_GROUND,
	OBJECT_STATUS_BFME_UNCONTROLLABLY_SCARED,
	OBJECT_STATUS_BFME_SPECIAL_ABILITY_PACKING_UNPACKING_OR_USING,
	OBJECT_STATUS_BFME_PLEASE_EAT_ME,
	OBJECT_STATUS_BFME_UPDATING_AI,
	OBJECT_STATUS_BFME_HUNT_WHEN_IDLE,
	OBJECT_STATUS_BFME_IGNORE_AI_COMMAND,
	OBJECT_STATUS_BFME_RUNNING_DOWN_FROM_BEHIND,
	OBJECT_STATUS_BFME_DO_NOT_SCORE,
	OBJECT_STATUS_BFME_CAN_NOT_WALK_ON,
	OBJECT_STATUS_BFME_MARCH_OF_DEATH,
	OBJECT_STATUS_BFME_DO_NOT_PICK_ME,
	OBJECT_STATUS_BFME_INHERITED_FROM_ALLY_TEAM,
	OBJECT_STATUS_BFME_SWITCHED_WEAPONS,
	OBJECT_STATUS_BFME_END_FIRE_STATE,
	OBJECT_STATUS_BFME_BOOKENDING,
	OBJECT_STATUS_BFME_ELVISH_EXPRESSLY,
	OBJECT_STATUS_BFME_INSIDE_CASTLE,
	OBJECT_STATUS_BFME_BUILD_BEING_CANCELED,
	OBJECT_STATUS_BFME_PENDING_CONSTRUCTION,
	OBJECT_STATUS_BFME_PHANTOM_STRUCTURE,
	OBJECT_STATUS_BFME_IN_FORMATION_TEMPLATE,
	OBJECT_STATUS_BFME_IS_LEAVING_FACTORY,
	OBJECT_STATUS_BFME_MOVING_TO_DISMOUNT,
	OBJECT_STATUS_BFME_NO_HERO_PROPERTIES,
	OBJECT_STATUS_BFME_CAN_ENTER_ANYTHING,
	OBJECT_STATUS_BFME_HOLDING_THE_RING,
	OBJECT_STATUS_BFME_INVISIBLE_DETECTED_BY_FRIEND,
	OBJECT_STATUS_BFME_INVISIBLE_DETECTED,
	OBJECT_STATUS_BFME_WORKER_REPAIRING,
	OBJECT_STATUS_BFME_ATTACHED,
	OBJECT_STATUS_BFME_WONT_RIDE_WITH_YOU,
	OBJECT_STATUS_BFME_COMMAND_BUTTON_TOGGLED,
	OBJECT_STATUS_BFME_IGNORE_PARALYZE_NUGGET,
	OBJECT_STATUS_BFME_SUMMONING_REPLACEMENT,
	OBJECT_STATUS_BFME_HOLDING_THE_SHARD,
	OBJECT_STATUS_BFME_USER_DEFINED_1,
	OBJECT_STATUS_BFME_USER_DEFINED_2,

	OBJECT_STATUS_COUNT

};

typedef BitFlags<OBJECT_STATUS_COUNT, struct ObjectStatusMaskTypeTag>	ObjectStatusMaskType;

#define MAKE_OBJECT_STATUS_MASK(k) ObjectStatusMaskType(ObjectStatusMaskType::kInit, (k))
#define MAKE_OBJECT_STATUS_MASK2(k,a) ObjectStatusMaskType(ObjectStatusMaskType::kInit, (k), (a))
#define MAKE_OBJECT_STATUS_MASK3(k,a,b) ObjectStatusMaskType(ObjectStatusMaskType::kInit, (k), (a), (b))
#define MAKE_OBJECT_STATUS_MASK4(k,a,b,c) ObjectStatusMaskType(ObjectStatusMaskType::kInit, (k), (a), (b), (c))
#define MAKE_OBJECT_STATUS_MASK5(k,a,b,c,d) ObjectStatusMaskType(ObjectStatusMaskType::kInit, (k), (a), (b), (c), (d))

inline Bool TEST_OBJECT_STATUS_MASK( const ObjectStatusMaskType& m, ObjectStatusTypes t )
{
	return m.test( t );
}

inline Bool TEST_OBJECT_STATUS_MASK_ANY( const ObjectStatusMaskType& m, const ObjectStatusMaskType& mask )
{
	return m.anyIntersectionWith( mask );
}

inline Bool TEST_OBJECT_STATUS_MASK_MULTI( const ObjectStatusMaskType& m, const ObjectStatusMaskType& mustBeSet, const ObjectStatusMaskType& mustBeClear )
{
	return m.testSetAndClear( mustBeSet, mustBeClear );
}

inline Bool OBJECT_STATUS_MASK_ANY_SET( const ObjectStatusMaskType& m)
{
	return m.any();
}

inline void CLEAR_OBJECT_STATUS_MASK( ObjectStatusMaskType& m )
{
	m.clear();
}

inline void SET_ALL_OBJECT_STATUS_MASK_BITS( ObjectStatusMaskType& m )
{
	m.clear( );
	m.flip( );
}

inline void FLIP_OBJECT_STATUS_MASK( ObjectStatusMaskType& m )
{
	m.flip();
}

// defined in Common/System/ObjectStatusTypes.cpp
extern ObjectStatusMaskType OBJECT_STATUS_MASK_NONE;	// inits to all zeroes
