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

// WeaponSet.h

#pragma once

#include "Lib/BaseType.h"
#include "Common/GameType.h"
#include "Common/KindOf.h"
#include "Common/ModelState.h"
#include "Common/SparseMatchFinder.h"
#include "Common/Snapshot.h"
#include "GameLogic/Damage.h"

//-------------------------------------------------------------------------------------------------
class INI;
class Object;
class Weapon;
class WeaponTemplate;

enum CommandSourceType CPP_11(: Int);
enum DamageType CPP_11(: Int);

// for WeaponSetType. Part of detangling.
#include "GameLogic/WeaponSetType.h"

//-------------------------------------------------------------------------------------------------
// for WeaponSetFlags. Part of detangling.
#include "GameLogic/WeaponSetFlags.h"

#ifdef DEFINE_WEAPONSLOTTYPE_NAMES
static const char *const TheWeaponSlotTypeNames[] =
{
	"PRIMARY",
	"SECONDARY",
	"TERTIARY",
	"QUATERNARY",
	"QUINARY",

	nullptr
};
static_assert(ARRAY_SIZE(TheWeaponSlotTypeNames) == WEAPONSLOT_COUNT + 1, "Incorrect array size");

static const LookupListRec TheWeaponSlotTypeNamesLookupList[] =
{
	{ "PRIMARY",		PRIMARY_WEAPON },
	{ "SECONDARY",	SECONDARY_WEAPON },
	{ "TERTIARY",		TERTIARY_WEAPON },
	{ "QUATERNARY",	QUATERNARY_WEAPON },
	{ "QUINARY",		QUINARY_WEAPON },

	{ nullptr, 0	}
};
static_assert(ARRAY_SIZE(TheWeaponSlotTypeNamesLookupList) == WEAPONSLOT_COUNT + 1, "Incorrect array size");

#endif

//-------------------------------------------------------------------------------------------------
#ifdef DEFINE_WEAPONCONDITIONMAP

//Kris: I did not write this code, but I am adding comments to clarify it.
//When I ran into this, I discovered that it was grossly out of date. It wasn't
//clearly identified as a "lookup table", but hopefully now it makes sense. I've
//updated it as of May 2003 when I added RIDER1-8 conditions.

//Purpose: Whenever you change a weaponset, the model condition state associated with it
//will be properly set exclusively.
static const ModelConditionFlagType TheWeaponSetTypeToModelConditionTypeMap[WEAPONSET_COUNT] =
{
	/*WEAPONSET_VETERAN*/								MODELCONDITION_WEAPONSET_VETERAN,
	/*WEAPONSET_ELITE*/									MODELCONDITION_WEAPONSET_ELITE,
	/*WEAPONSET_HERO*/									MODELCONDITION_WEAPONSET_HERO,
	/*WEAPONSET_PLAYER_UPGRADE*/				MODELCONDITION_WEAPONSET_PLAYER_UPGRADE,
	/*WEAPONSET_CRATEUPGRADE_ONE*/			MODELCONDITION_WEAPONSET_CRATEUPGRADE_ONE,
	/*WEAPONSET_CRATEUPGRADE_TWO*/			MODELCONDITION_WEAPONSET_CRATEUPGRADE_TWO,
	/*WEAPONSET_VEHICLE_HIJACK*/				MODELCONDITION_INVALID,
	/*WEAPONSET_CARBOMB*/								MODELCONDITION_INVALID,
	/*WEAPONSET_MINE_CLEARING_DETAIL*/	MODELCONDITION_INVALID,
	/*WEAPONSET_RIDER1*/								MODELCONDITION_RIDER1,	//Added these for different riders, but feel free to use these for anything.
	/*WEAPONSET_RIDER2*/								MODELCONDITION_RIDER2,
	/*WEAPONSET_RIDER3*/								MODELCONDITION_RIDER3,
	/*WEAPONSET_RIDER4*/								MODELCONDITION_RIDER4,
	/*WEAPONSET_RIDER5*/								MODELCONDITION_RIDER5,
	/*WEAPONSET_RIDER6*/								MODELCONDITION_RIDER6,
	/*WEAPONSET_RIDER7*/								MODELCONDITION_RIDER7,
	/*WEAPONSET_RIDER8*/								MODELCONDITION_RIDER8,
	// bfme-native: RotWK
	/*PASSENGER_TYPE_ONE*/ MODELCONDITION_WEAPONSET_PASSENGER_TYPE_ONE,
	/*PASSENGER_TYPE_TWO*/ MODELCONDITION_WEAPONSET_PASSENGER_TYPE_TWO,
	/*GARRISONED*/ MODELCONDITION_WEAPONSET_GARRISONED,
	/*CLOSE_RANGE*/ MODELCONDITION_WEAPONSET_CLOSE_RANGE,
	/*RAMPAGE*/ MODELCONDITION_WEAPONSET_RAMPAGE,
	/*CONTESTING_BUILDING*/ MODELCONDITION_WEAPONSET_CONTESTING_BUILDING,
	/*SPECIAL_ONE*/ MODELCONDITION_WEAPONSET_SPECIAL_ONE,
	/*SPECIAL_TWO*/ MODELCONDITION_WEAPONSET_SPECIAL_TWO,
	/*CONTAINED*/ MODELCONDITION_WEAPONSET_CONTAINED,
	/*MOUNTED*/ MODELCONDITION_WEAPONSET_MOUNTED,
	/*ENRAGED*/ MODELCONDITION_WEAPONSET_ENRAGED,
	/*SPECIAL_UPGRADE*/ MODELCONDITION_WEAPONSET_SPECIAL_UPGRADE,
	/*WEAPONSET_TOGGLE_1*/ MODELCONDITION_WEAPONSET_TOGGLE_1,
	/*WEAPONSET_TOGGLE_2*/ MODELCONDITION_WEAPONSET_TOGGLE_2,
	/*WEAPONSET_TOGGLE_3*/ MODELCONDITION_WEAPONSET_TOGGLE_3,
	/*WEAPONSET_HERO_MODE*/ MODELCONDITION_WEAPONSET_HERO_MODE,
	/*WEAPONSET_ONE_RING_MODE*/ MODELCONDITION_INVALID,
	/*WEAPONSET_CREATE_A_HERO_WS_01*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_01,
	/*WEAPONSET_CREATE_A_HERO_WS_02*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_02,
	/*WEAPONSET_CREATE_A_HERO_WS_03*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_03,
	/*WEAPONSET_CREATE_A_HERO_WS_04*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_04,
	/*WEAPONSET_CREATE_A_HERO_WS_05*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_05,
	/*WEAPONSET_CREATE_A_HERO_WS_06*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_06,
	/*WEAPONSET_CREATE_A_HERO_WS_07*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_07,
	/*WEAPONSET_CREATE_A_HERO_WS_08*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_08,
	/*WEAPONSET_CREATE_A_HERO_WS_09*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_09,
	/*WEAPONSET_CREATE_A_HERO_WS_10*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_10,
	/*WEAPONSET_CREATE_A_HERO_WS_11*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_11,
	/*WEAPONSET_CREATE_A_HERO_WS_12*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_12,
	/*WEAPONSET_CREATE_A_HERO_WS_13*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_13,
	/*WEAPONSET_CREATE_A_HERO_WS_14*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_14,
	/*WEAPONSET_CREATE_A_HERO_WS_15*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_15,
	/*WEAPONSET_CREATE_A_HERO_WS_16*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_16,
	/*WEAPONSET_CREATE_A_HERO_WS_17*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_17,
	/*WEAPONSET_CREATE_A_HERO_WS_18*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_18,
	/*WEAPONSET_CREATE_A_HERO_WS_19*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_19,
	/*WEAPONSET_CREATE_A_HERO_WS_20*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_20,
	/*WEAPONSET_CREATE_A_HERO_WS_21*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_21,
	/*WEAPONSET_CREATE_A_HERO_WS_22*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_22,
	/*WEAPONSET_CREATE_A_HERO_WS_23*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_23,
	/*WEAPONSET_CREATE_A_HERO_WS_24*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_24,
	/*WEAPONSET_CREATE_A_HERO_WS_25*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_25,
	/*WEAPONSET_CREATE_A_HERO_WS_26*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_26,
	/*WEAPONSET_CREATE_A_HERO_WS_27*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_27,
	/*WEAPONSET_CREATE_A_HERO_WS_28*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_28,
	/*WEAPONSET_CREATE_A_HERO_WS_29*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_29,
	/*WEAPONSET_CREATE_A_HERO_WS_30*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_30,
	/*WEAPONSET_CREATE_A_HERO_WS_31*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_31,
	/*WEAPONSET_CREATE_A_HERO_WS_32*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_32,
	/*HIDDEN*/ MODELCONDITION_HIDDEN,
	/*ARMORSET_CREATE_A_HERO_01*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_01,
	/*ARMORSET_CREATE_A_HERO_02*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_02,
	/*ARMORSET_CREATE_A_HERO_03*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_03,
	/*ARMORSET_CREATE_A_HERO_04*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_04,
	/*ARMORSET_CREATE_A_HERO_05*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_05,
	/*ARMORSET_CREATE_A_HERO_06*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_06,
	/*ARMORSET_CREATE_A_HERO_07*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_07,
	/*ARMORSET_CREATE_A_HERO_08*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_08,
	/*ARMORSET_CREATE_A_HERO_09*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_09,
	/*ARMORSET_CREATE_A_HERO_10*/ MODELCONDITION_ARMORSET_CREATE_A_HERO_10,
	/*WEAPONSET_CREATE_A_HERO_WS_33*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_33,
	/*WEAPONSET_CREATE_A_HERO_WS_34*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_34,
	/*WEAPONSET_CREATE_A_HERO_WS_35*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_35,
	/*WEAPONSET_CREATE_A_HERO_WS_36*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_36,
	/*WEAPONSET_CREATE_A_HERO_WS_37*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_37,
	/*WEAPONSET_CREATE_A_HERO_WS_38*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_38,
	/*WEAPONSET_CREATE_A_HERO_WS_39*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_39,
	/*WEAPONSET_CREATE_A_HERO_WS_40*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_40,
	/*WEAPONSET_CREATE_A_HERO_WS_41*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_41,
	/*WEAPONSET_CREATE_A_HERO_WS_42*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_42,
	/*WEAPONSET_CREATE_A_HERO_WS_43*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_43,
	/*WEAPONSET_CREATE_A_HERO_WS_44*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_44,
	/*WEAPONSET_CREATE_A_HERO_WS_45*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_45,
	/*WEAPONSET_CREATE_A_HERO_WS_46*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_46,
	/*WEAPONSET_CREATE_A_HERO_WS_47*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_47,
	/*WEAPONSET_CREATE_A_HERO_WS_48*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_48,
	/*WEAPONSET_CREATE_A_HERO_WS_49*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_49,
	/*WEAPONSET_CREATE_A_HERO_WS_50*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_50,
	/*WEAPONSET_CREATE_A_HERO_WS_51*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_51,
	/*WEAPONSET_CREATE_A_HERO_WS_52*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_52,
	/*WEAPONSET_CREATE_A_HERO_WS_53*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_53,
	/*WEAPONSET_CREATE_A_HERO_WS_54*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_54,
	/*WEAPONSET_CREATE_A_HERO_WS_55*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_55,
	/*WEAPONSET_CREATE_A_HERO_WS_56*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_56,
	/*WEAPONSET_CREATE_A_HERO_WS_57*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_57,
	/*WEAPONSET_CREATE_A_HERO_WS_58*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_58,
	/*WEAPONSET_CREATE_A_HERO_WS_59*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_59,
	/*WEAPONSET_CREATE_A_HERO_WS_60*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_60,
	/*WEAPONSET_CREATE_A_HERO_WS_61*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_61,
	/*WEAPONSET_CREATE_A_HERO_WS_62*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_62,
	/*WEAPONSET_CREATE_A_HERO_WS_63*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_63,
	/*WEAPONSET_CREATE_A_HERO_WS_64*/ MODELCONDITION_WEAPONSET_CREATE_A_HERO_WS_64,
};
#endif

//-------------------------------------------------------------------------------------------------
enum WeaponSetConditionType CPP_11(: Int)
{
	WSF_INVALID = -1,

	WSF_NONE = 0,
	WSF_FIRING,
	WSF_BETWEEN,
	WSF_RELOADING,
	WSF_PREATTACK,

	WSF_COUNT
};

//-------------------------------------------------------------------------------------------------
class WeaponTemplateSet
{
private:
	const ThingTemplate*		m_thingTemplate;	// needed for save/load
	WeaponSetFlags					m_types;
	const WeaponTemplate*		m_template[WEAPONSLOT_COUNT];
	UnsignedInt							m_autoChooseMask[WEAPONSLOT_COUNT];
	KindOfMaskType					m_preferredAgainst[WEAPONSLOT_COUNT];
	KindOfMaskType					m_onlyAgainst[WEAPONSLOT_COUNT];	///< bfme-native: slot only used against these kinds
	Bool										m_isReloadTimeShared;
	Bool										m_isWeaponLockSharedAcrossSets; ///< A weapon set so similar that it is safe to hold locks across

	static void parseWeapon(INI* ini, void *instance, void *store, const void* userData);
	static void parseAutoChoose(INI* ini, void *instance, void *store, const void* userData);
	static void parsePreferredAgainst(INI* ini, void *instance, void *store, const void* userData);
	static void parseOnlyAgainst(INI* ini, void *instance, void *store, const void* userData);

public:
	inline WeaponTemplateSet()
	{
		clear();
	}

	const ThingTemplate* friend_getThingTemplate() const { return m_thingTemplate; }	// only for WeaponSet::xfer
	const WeaponSetFlags& friend_getWeaponSetFlags() const { return m_types; }	// only for WeaponSet::xfer

	void clear();
	void parseWeaponTemplateSet( INI* ini, const ThingTemplate* tt );
	Bool testWeaponSetFlag( WeaponSetType wst ) const;
	Bool isSharedReloadTime() const { return m_isReloadTimeShared; }
	Bool isWeaponLockSharedAcrossSets() const {return m_isWeaponLockSharedAcrossSets; }

	Bool hasAnyWeapons() const;
	inline const WeaponTemplate* getNth(WeaponSlotType n) const { return m_template[n]; }
	inline UnsignedInt getNthCommandSourceMask(WeaponSlotType n) const { return m_autoChooseMask[n]; }
	inline const KindOfMaskType& getNthPreferredAgainstMask(WeaponSlotType n) const { return m_preferredAgainst[n]; }
	inline const KindOfMaskType& getNthOnlyAgainstMask(WeaponSlotType n) const { return m_onlyAgainst[n]; }

	inline Int getConditionsYesCount() const { return 1; }
	inline const WeaponSetFlags& getNthConditionsYes(Int i) const { return m_types; }
#if defined(RTS_DEBUG)
	inline AsciiString getDescription() const { return "ArmorTemplateSet"; }
#endif
};

//-------------------------------------------------------------------------------------------------
typedef std::vector<WeaponTemplateSet> WeaponTemplateSetVector;

//-------------------------------------------------------------------------------------------------
enum WeaponChoiceCriteria CPP_11(: Int)
{
	PREFER_MOST_DAMAGE,		///< choose the weapon that will do the most damage
	PREFER_LONGEST_RANGE	///< choose the weapon with the longest range (that will do nonzero damage)
};

//-------------------------------------------------------------------------------------------------
enum WeaponLockType CPP_11(: Int)
{
	NOT_LOCKED,							///< Weapon is not locked
	LOCKED_TEMPORARILY,			///< Weapon is locked until clip is empty, or current "attack" state exits
	LOCKED_PERMANENTLY			///< Weapon is locked until explicitly unlocked or lock is changed to another weapon
};

//-------------------------------------------------------------------------------------------------
enum CanAttackResult CPP_11(: Int)
{
	//Worst scenario to best scenario -- These must be done this way now!
	ATTACKRESULT_NOT_POSSIBLE,					//Can't possibly attack target.
	ATTACKRESULT_INVALID_SHOT,					//Not a clear shot
	ATTACKRESULT_POSSIBLE_AFTER_MOVING, //I can attack, but after moving closer.
	ATTACKRESULT_POSSIBLE,							//I can attack now.
};

//-------------------------------------------------------------------------------------------------
class WeaponSet : public Snapshot
{
private:
	const WeaponTemplateSet*	m_curWeaponTemplateSet;
	Weapon*										m_weapons[WEAPONSLOT_COUNT];
	WeaponSlotType						m_curWeapon;
	WeaponLockType						m_curWeaponLockedStatus;
	UnsignedInt								m_filledWeaponSlotMask;
	Int												m_totalAntiMask;						///< anti mask of all current weapons
	DamageTypeFlags						m_totalDamageTypeMask;			///< damagetype mask of all current weapons
	Bool											m_hasPitchLimit;
	Bool											m_hasDamageWeapon;

	Bool isAnyWithinTargetPitch(const Object* obj, const Object* victim) const;

protected:
	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

public:

	WeaponSet();
	~WeaponSet();

	void updateWeaponSet(const Object* obj);
	void reloadAllAmmo(const Object *obj, Bool now);
	Bool isOutOfAmmo() const;
	Bool hasAnyWeapon() const { return m_filledWeaponSlotMask != 0; }
	Bool hasAnyDamageWeapon() const { return m_hasDamageWeapon; }
	Bool hasWeaponToDealDamageType(DamageType typeToDeal) const { return m_totalDamageTypeMask.test(typeToDeal); }
	Bool hasSingleDamageType(DamageType typeToDeal) const { return (m_totalDamageTypeMask.test(typeToDeal) && (m_totalDamageTypeMask.count() == 1) ); }
	Bool isCurWeaponLocked() const { return m_curWeaponLockedStatus != NOT_LOCKED; }
	Weapon* getCurWeapon() { return m_weapons[m_curWeapon]; }
	const Weapon* getCurWeapon() const { return m_weapons[m_curWeapon]; }
	WeaponSlotType getCurWeaponSlot() const { return m_curWeapon; }
	Weapon* findWaypointFollowingCapableWeapon();
	const Weapon* findAmmoPipShowingWeapon() const;
	void weaponSetOnWeaponBonusChange(const Object *source);
	UnsignedInt getMostPercentReadyToFireAnyWeapon() const;
	inline UnsignedInt getNthCommandSourceMask( WeaponSlotType n ) const { return m_curWeaponTemplateSet ? m_curWeaponTemplateSet->getNthCommandSourceMask( n ) : 0; }

	Bool setWeaponLock( WeaponSlotType weaponSlot, WeaponLockType lockType );
	void releaseWeaponLock(WeaponLockType lockType);
	Bool isSharedReloadTime() const;

	//When an AIAttackState is over, it needs to clean up any weapons that might be in leech range mode
	//or else those weapons will have unlimited range!
	void clearLeechRangeModeForAllWeapons();

	/**
		Determines if the unit has any weapon that could conceivably
		harm the victim. this does not take range, ammo, etc. into
		account, but immutable weapon properties, such as "can you
		target airborne victims".
	*/
	CanAttackResult getAbleToAttackSpecificObject( AbleToAttackType t, const Object* obj, const Object* victim, CommandSourceType commandSource, WeaponSlotType specificSlot = (WeaponSlotType)-1 ) const;

	//When calling this function, all conditions must be validated to the point where we have decided that we wish to attack the object (faction checks, etc).
	//Now, we are determining if the attack itself is able to be performed!
	CanAttackResult getAbleToUseWeaponAgainstTarget( AbleToAttackType attackType, const Object *source, const Object *victim, const Coord3D *pos, CommandSourceType commandSource, WeaponSlotType specificSlot = (WeaponSlotType)-1 ) const;

	/**
		Selects the best weapon for the given target, and sets it as the current weapon.
		If there is no weapon that can damage the target, false is returned (and the current-weapon is unchanged).
		Note that this DOES take weapon attack range into account.
	*/
	Bool chooseBestWeaponForTarget(const Object* obj, const Object* victim, WeaponChoiceCriteria criteria, CommandSourceType cmdSource);

	Weapon* getWeaponInWeaponSlot(WeaponSlotType wslot) const;

	static ModelConditionFlags getModelConditionForWeaponSlot(WeaponSlotType wslot, WeaponSetConditionType a);
};
