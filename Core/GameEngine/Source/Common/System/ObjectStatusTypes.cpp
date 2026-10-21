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

// FILE: ObjectStatusTypes.cpp ////////////////////////////////////////////////////////////////////
// Author: Kris, May 2003
// Desc:   Object status types that are stackable using the BitSet system. Used to be ObjectStatusBits
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "Common/ObjectStatusTypes.h"
#include "Common/BitFlagsIO.h"

template<>
const char* const ObjectStatusMaskType::s_bitNameList[] =
{
	"NONE",
	"DESTROYED",
	"CAN_ATTACK",
	"UNDER_CONSTRUCTION",
	"UNSELECTABLE",
	"NO_COLLISIONS",
	"NO_ATTACK",
	"AIRBORNE_TARGET",
	"PARACHUTING",
	"REPULSOR",
	"HIJACKED",
	"AFLAME",
	"BURNED",
	"WET",
	"IS_FIRING_WEAPON",
	"IS_BRAKING",
	"STEALTHED",
	"DETECTED",
	"CAN_STEALTH",
	"SOLD",
	"UNDERGOING_REPAIR",
	"RECONSTRUCTING",
	"MASKED",
	"IS_ATTACKING",
	"USING_ABILITY",
	"IS_AIMING_WEAPON",
	"NO_ATTACK_FROM_AI",
	"IGNORING_STEALTH",
	"IS_CARBOMB",
	// TheSuperHackers @info New statuses added in Zero Hour
	"DECK_HEIGHT_OFFSET",
	"STATUS_RIDER1",
	"STATUS_RIDER2",
	"STATUS_RIDER3",
	"STATUS_RIDER4",
	"STATUS_RIDER5",
	"STATUS_RIDER6",
	"STATUS_RIDER7",
	"STATUS_RIDER8",
	"FAERIE_FIRE",
  "KILLING_SELF",
	"REASSIGN_PARKING",
	"BOOBY_TRAPPED",
	"IMMOBILE",
	"DISGUISED",
	"DEPLOYED",
	// bfme-native: RotWK
	"HIDDEN",
	"NO_AUTO_ACQUIRE",
	"IS_MELEE_ATTACKING",
	"GUARD_SELECTION",
	"LEASHED_RETURNING",
	"DEATH_1",
	"DEATH_2",
	"DEATH_3",
	"DEATH_4",
	"DEATH_5",
	"CONTESTED",
	"CONTESTING_BUILDING",
	"HORDE_MEMBER",
	"RIDERLESS",
	"RIDER_IS_PILOT",
	"RIDER1",
	"RIDER2",
	"RIDER3",
	"RIDER4",
	"RIDER5",
	"RIDER6",
	"RIDER7",
	"RIDER8",
	"FLEE_OFF_MAP",
	"NOT_IN_WORLD",
	"INAUDIBLE",
	"CHANTING",
	"ENRAGED",
	"CREATE_DRAWABLE_WITH_LOW_DETAIL",
	"SINKING",
	"RAMPAGING",
	"INSIDE_GARRISON",
	"UNATTACKABLE",
	"ENCLOSED",
	"TEMPORARILY_DEFECTED",
	"TAGGED",
	"DEPLOYING",
	"BLOODTHIRSTY",
	"PORTER_TAGGED",
	"GRAB_AND_DROP",
	"STAND_GROUND",
	"UNCONTROLLABLY_SCARED",
	"SPECIAL_ABILITY_PACKING_UNPACKING_OR_USING",
	"PLEASE_EAT_ME",
	"UPDATING_AI",
	"HUNT_WHEN_IDLE",
	"IGNORE_AI_COMMAND",
	"RUNNING_DOWN_FROM_BEHIND",
	"DO_NOT_SCORE",
	"CAN_NOT_WALK_ON",
	"MARCH_OF_DEATH",
	"DO_NOT_PICK_ME",
	"INHERITED_FROM_ALLY_TEAM",
	"SWITCHED_WEAPONS",
	"END_FIRE_STATE",
	"BOOKENDING",
	"ELVISH_EXPRESSLY",
	"INSIDE_CASTLE",
	"BUILD_BEING_CANCELED",
	"PENDING_CONSTRUCTION",
	"PHANTOM_STRUCTURE",
	"IN_FORMATION_TEMPLATE",
	"IS_LEAVING_FACTORY",
	"MOVING_TO_DISMOUNT",
	"NO_HERO_PROPERTIES",
	"CAN_ENTER_ANYTHING",
	"HOLDING_THE_RING",
	"INVISIBLE_DETECTED_BY_FRIEND",
	"INVISIBLE_DETECTED",
	"WORKER_REPAIRING",
	"ATTACHED",
	"WONT_RIDE_WITH_YOU",
	"COMMAND_BUTTON_TOGGLED",
	"IGNORE_PARALYZE_NUGGET",
	"SUMMONING_REPLACEMENT",
	"HOLDING_THE_SHARD",
	"USER_DEFINED_1",
	"USER_DEFINED_2",
	nullptr
};
static_assert(ARRAY_SIZE(ObjectStatusMaskType::s_bitNameList) == ObjectStatusMaskType::NumBits + 1, "Incorrect array size");

ObjectStatusMaskType OBJECT_STATUS_MASK_NONE;	// inits to all zeroes
