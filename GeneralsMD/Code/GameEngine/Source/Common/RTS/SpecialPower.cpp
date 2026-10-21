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

// FILE: SpecialPower.cpp /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:   Special power templates and the system that holds them
// Edited: Kris Morness -- July 2002 (added BitFlag system)
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Object.h"
#include "Common/BitFlagsIO.h"


// GLOBAL /////////////////////////////////////////////////////////////////////////////////////////
SpecialPowerStore *TheSpecialPowerStore = nullptr;

#define DEFAULT_DEFECTION_DETECTION_PROTECTION_TIME_LIMIT (LOGICFRAMES_PER_SECOND * 10)

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// Externs ////////////////////////////////////////////////////////////////////////////////////////
template<>
const char* const SpecialPowerMaskType::s_bitNameList[] =
{
	"SPECIAL_INVALID",

	//Superweapons
	"SPECIAL_DAISY_CUTTER",
	"SPECIAL_PARADROP_AMERICA",
	"SPECIAL_CARPET_BOMB",
	"SPECIAL_CLUSTER_MINES",
	"SPECIAL_EMP_PULSE",
	"SPECIAL_NAPALM_STRIKE",
	"SPECIAL_CASH_HACK",
	"SPECIAL_NEUTRON_MISSILE",
	"SPECIAL_SPY_SATELLITE",
	"SPECIAL_DEFECTOR",
	"SPECIAL_TERROR_CELL",
	"SPECIAL_AMBUSH",
	"SPECIAL_BLACK_MARKET_NUKE",
	"SPECIAL_ANTHRAX_BOMB",
	"SPECIAL_SCUD_STORM",
#ifdef ALLOW_DEMORALIZE
	"SPECIAL_DEMORALIZE",
#else
	"SPECIAL_DEMORALIZE_OBSOLETE",
#endif
	"SPECIAL_CRATE_DROP",
	"SPECIAL_A10_THUNDERBOLT_STRIKE",
	"SPECIAL_DETONATE_DIRTY_NUKE",
	"SPECIAL_ARTILLERY_BARRAGE",

	//Special abilities
	"SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES",
	"SPECIAL_REMOTE_CHARGES",
	"SPECIAL_TIMED_CHARGES",
	"SPECIAL_HELIX_NAPALM_BOMB",
	"SPECIAL_HACKER_DISABLE_BUILDING",
	"SPECIAL_TANKHUNTER_TNT_ATTACK",
	"SPECIAL_BLACKLOTUS_CAPTURE_BUILDING",
	"SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK",
	"SPECIAL_BLACKLOTUS_STEAL_CASH_HACK",
	"SPECIAL_INFANTRY_CAPTURE_BUILDING",
	"SPECIAL_RADAR_VAN_SCAN",
	"SPECIAL_SPY_DRONE",
	"SPECIAL_DISGUISE_AS_VEHICLE",
	"SPECIAL_BOOBY_TRAP",
	"SPECIAL_REPAIR_VEHICLES",
	"SPECIAL_PARTICLE_UPLINK_CANNON",
	"SPECIAL_CASH_BOUNTY",
	"SPECIAL_CHANGE_BATTLE_PLANS",
	"SPECIAL_CIA_INTELLIGENCE",
	"SPECIAL_CLEANUP_AREA",
	"SPECIAL_LAUNCH_BAIKONUR_ROCKET",
  "SPECIAL_SPECTRE_GUNSHIP",
  "SPECIAL_GPS_SCRAMBLER",
	"SPECIAL_FRENZY",
	"SPECIAL_SNEAK_ATTACK",

	"SPECIAL_CHINA_CARPET_BOMB",
	"EARLY_SPECIAL_CHINA_CARPET_BOMB",
	"SPECIAL_LEAFLET_DROP",
	"EARLY_SPECIAL_LEAFLET_DROP",
	"EARLY_SPECIAL_FRENZY",
	"SPECIAL_COMMUNICATIONS_DOWNLOAD",
	"EARLY_SPECIAL_REPAIR_VEHICLES",
	"SPECIAL_TANK_PARADROP",
	"SUPW_SPECIAL_PARTICLE_UPLINK_CANNON",
	"AIRF_SPECIAL_DAISY_CUTTER",
	"NUKE_SPECIAL_CLUSTER_MINES",
	"NUKE_SPECIAL_NEUTRON_MISSILE",
	"AIRF_SPECIAL_A10_THUNDERBOLT_STRIKE",
	"AIRF_SPECIAL_SPECTRE_GUNSHIP",
	"INFA_SPECIAL_PARADROP_AMERICA",
	"SLTH_SPECIAL_GPS_SCRAMBLER",
	"AIRF_SPECIAL_CARPET_BOMB",
	"SUPR_SPECIAL_CRUISE_MISSILE",
	"LAZR_SPECIAL_PARTICLE_UPLINK_CANNON",
	"SUPW_SPECIAL_NEUTRON_MISSILE",

	"SPECIAL_BATTLESHIP_BOMBARDMENT",

	// bfme-native: RotWK
	"SPECIAL_UP_FOR_GRABS_2",
	"SPECIAL_UP_FOR_GRABS_3",
	"SPECIAL_UP_FOR_GRABS_4",
	"SPECIAL_UP_FOR_GRABS",
	"SPECIAL_PRINCE_OF_DOL_ARMOTH",
	"SPECIAL_MAN_THE_WALLS",
	"SPECIAL_OSGILIATH_VETERANS",
	"SPECIAL_RANGER_AMBUSH",
	"SPECIAL_GRAB_PASSENGER",
	"SPECIAL_GRAB_CHUNK",
	"SPECIAL_SPAWN_ORCS",
	"SPECIAL_CHARGE_ATTACK",
	"SPECIAL_PART_THE_HEAVENS",
	"SPECIAL_DEFLECT_PROJECTILES",
	"SPECIAL_SIEGEDEPLOY",
	"SPECIAL_STOP",
	"SPECIAL_ARROW_STORM",
	"SPECIAL_SWOOP_ATTACK",
	"SPECIAL_LEVEL_ATTACK",
	"SPECIAL_LEVEL_POSITION",
	"SPECIAL_GIVE_UPGRADE",
	"SPECIAL_ROUSING_SPEECH",
	"SPECIAL_GENERAL_TARGETLESS",
	"SPECIAL_GENERAL_TARGETLESS_TWO",
	"SPECIAL_SHIELD_BUBBLE",
	"SPECIAL_TOGGLE_MOUNTED",
	"SPECIAL_WIZARD_BLAST",
	"SPECIAL_GLORIOUS_CHARGE",
	"SPECIAL_WOUND_ARROW",
	"SPECIAL_HERO_MODE",
	"SPECIAL_FLAMING_SWORD",
	"SPECIAL_FIRE_WHIP",
	"SPECIAL_BALROG_BREATH",
	"SPECIAL_MTTROLL_BORED",
	"SPECIAL_BALROG_WINGS",
	"SPECIAL_BALROG_SCREAM",
	"SPECIAL_TRAINING",
	"SPECIAL_TELEKENETIC_PUSH",
	"SPECIAL_SONIC_SONG",
	"SPECIAL_REVEAL_MAP_AREA",
	"SPECIAL_KNIFE_ATTACK",
	"SPECIAL_SPAWN_OATHBREAKERS",
	"SPECIAL_CALL_OF_THE_DEEP",
	"SPECIAL_SKULL_TOTEM",
	"SPECIAL_EXTINGUISH_FIRE",
	"SPECIAL_TRIGGER_ATTRIBUTE_MODIFIER",
	"SPECIAL_DOMINATE_ENEMY",
	"SPECIAL_WORD_OF_POWER",
	"SPECIAL_KNIFE_FIGHTER",
	"SPECIAL_SPELL_BOOK_HEAL",
	"SPECIAL_SPELL_BOOK_ELVEN_GIFTS",
	"SPECIAL_SPELL_BOOK_SPAWN_LONE_TOWER",
	"SPECIAL_SPELL_BOOK_ENSHROUDING_MIST",
	"SPECIAL_SPELL_BOOK_RALLYING_CALL",
	"SPECIAL_SPELL_BOOK_TOM_BOMBADIL",
	"SPECIAL_SPELL_BOOK_HOBBIT_ALLIES",
	"SPECIAL_SPELL_BOOK_REBUILD",
	"SPECIAL_SPELL_BOOK_ARROW_VOLLEY_GOOD",
	"SPECIAL_SPELL_BOOK_ELVEN_WOOD",
	"SPECIAL_SPELL_BOOK_DWARVEN_RICHES",
	"SPECIAL_SPELL_BOOK_MEN_OF_DALE_ALLIES",
	"SPECIAL_SPELL_BOOK_CLOUD_BREAK",
	"SPECIAL_SPELL_BOOK_ROHAN_ALLIES",
	"SPECIAL_SPELL_BOOK_DUNEDAIN_ALLIES",
	"SPECIAL_SPELL_BOOK_ENT_ALLIES",
	"SPECIAL_SPELL_BOOK_EAGLE_ALLIES",
	"SPECIAL_SPELL_BOOK_UNDERMINE",
	"SPECIAL_SPELL_BOOK_BOMBARD",
	"SPECIAL_SPELL_BOOK_ARMY_OF_THE_DEAD",
	"SPECIAL_SPELL_BOOK_EARTHQUAKE",
	"SPECIAL_SPELL_BOOK_FLOOD",
	"SPECIAL_SPELL_BOOK_SUNFLARE",
	"SPECIAL_SPELL_BOOK_CITADEL",
	"SPECIAL_SPELL_BOOK_TAINT",
	"SPECIAL_SPELL_BOOK_EYE_OF_SAURON",
	"SPECIAL_SPELL_BOOK_BARRICADE",
	"SPECIAL_SPELL_BOOK_WAR_CHANT",
	"SPECIAL_SPELL_BOOK_PALANTIR_VISION",
	"SPECIAL_SPELL_BOOK_CREBAIN",
	"SPECIAL_SPELL_BOOK_CAVE_BATS",
	"SPECIAL_SPELL_BOOK_INDUSTRY",
	"SPECIAL_SPELL_BOOK_DEVASTATION",
	"SPECIAL_SPELL_BOOK_UNTAMED_ALLEGIANCE",
	"SPECIAL_SPELL_BOOK_ARROW_VOLLEY_EVIL",
	"SPECIAL_SPELL_BOOK_WILD_MEN_ALLIES",
	"SPECIAL_SPELL_BOOK_SCAVENGER",
	"SPECIAL_SPELL_BOOK_CALL_THE_HORDE",
	"SPECIAL_SPELL_BOOK_SPIDERLING_ALLIES",
	"SPECIAL_SPELL_BOOK_DARKNESS",
	"SPECIAL_SPELL_BOOK_AWAKEN_WYRM",
	"SPECIAL_SPELL_BOOK_FREEZING_RAIN",
	"SPECIAL_SPELL_BOOK_FUEL_THE_FIRES",
	"SPECIAL_SPELL_BOOK_WATCHER_ALLY",
	"SPECIAL_SPELL_BOOK_BALROG_ALLY",
	"SPECIAL_SPELL_BOOK_RAIN_OF_FIRE",
	"SPECIAL_SPELL_BOOK_DRAGON_ALLY",
	"SPECIAL_SPELL_BOOK_DRAGON_STRIKE",
	"SPECIAL_ATHELAS",
	"SPECIAL_AT_VISIBLE_OBJECT",
	"SPECIAL_SARUMAN_FIRE_BALL",
	"SPECIAL_DISGUISE",
	"SPECIAL_SMITE_CANCELDISGUISE",
	"SPECIAL_ATTRIBUTEMOD_CANCELDISGUISE",
	"SPECIAL_KINGS_FAVOR",
	"SPECIAL_FAKE_LEADERSHIP_BUTTON",
	"SPECIAL_GIVE_UPGRADE_NEAREST",
	"SPECIAL_SCREECH",
	"SPECIAL_REPAIR_STRUCTURE",
	"SPECIAL_EAT",
	"SPECIAL_GIMLI_LEAP",
	"SPECIAL_HARVEST",
	"SPECIAL_PERSONAL_FLOOD",
	"SPECIAL_ELVEN_GRACE",
	"SPECIAL_AT_VISIBLE_GROUNDED_OBJECT",
	"SPECIAL_TELEPORT_TEAM_TO_CASTER",
	"SPECIAL_SPAWN_TORNADO",
	"SPECIAL_CURSE_ENEMY",
	"SPECIAL_STORE_LIST_1",
	"SPECIAL_STORE_LIST_2",
	"SPECIAL_TELEPORT_LIST_TO_POSITION",
	"SPECIAL_EVACUATE_GARRISON",
	"SPECIAL_GENERAL_TARGETLESS_THREE",
	"SPECIAL_SUMMON_ALLIES",
	"SPECIAL_SPELL_BOOK_GENERAL_SUMMON",
	"SPECIAL_SPELL_BOOK_BLIGHT",
	"SPECIAL_SPELL_BOOK_SNOWBIND",
	"SPECIAL_SPELL_BOOK_CHILL_WIND",
	"SPECIAL_AOE_ATTACK_HEAL",
	nullptr
};
static_assert(ARRAY_SIZE(SpecialPowerMaskType::s_bitNameList) == SpecialPowerMaskType::NumBits + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerStore::parseSpecialPowerDefinition( INI *ini )
{
	// read the name
	AsciiString name = ini->getNextToken();

	SpecialPowerTemplate* specialPower = TheSpecialPowerStore->findSpecialPowerTemplatePrivate( name );

	if ( ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES )
	{
		if (specialPower)
		{
			SpecialPowerTemplate* child = (SpecialPowerTemplate*)specialPower->friend_getFinalOverride();
			specialPower = newInstance(SpecialPowerTemplate);
			*specialPower = *child;
			child->setNextOverride(specialPower);
			specialPower->markAsOverride();
			//TheSpecialPowerStore->m_specialPowerTemplates.push_back(specialPower); // nope, do NOT do this
		}
		else
		{
			specialPower = newInstance(SpecialPowerTemplate);
			const SpecialPowerTemplate *defaultTemplate = TheSpecialPowerStore->findSpecialPowerTemplate( "DefaultSpecialPower" );
			if( defaultTemplate )
				*specialPower = *defaultTemplate;
			specialPower->friend_setNameAndID(name, ++TheSpecialPowerStore->m_nextSpecialPowerID);
			specialPower->markAsOverride();
			TheSpecialPowerStore->m_specialPowerTemplates.push_back(specialPower);
		}
	}
	else
	{
		if (specialPower)
		{
			throw INI_INVALID_DATA;
		}
		else
		{
			specialPower = newInstance(SpecialPowerTemplate);
			const SpecialPowerTemplate *defaultTemplate = TheSpecialPowerStore->findSpecialPowerTemplate( "DefaultSpecialPower" );
			if( defaultTemplate )
				*specialPower = *defaultTemplate;
			specialPower->friend_setNameAndID(name, ++TheSpecialPowerStore->m_nextSpecialPowerID);
			TheSpecialPowerStore->m_specialPowerTemplates.push_back(specialPower);
		}
	}

	// parse the ini definition
	if (specialPower)
		ini->initFromINI( specialPower, specialPower->getFieldParse() );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ const FieldParse SpecialPowerTemplate::m_specialPowerFieldParse[] =
{

	{ "ReloadTime",								INI::parseDurationUnsignedInt,		nullptr,	offsetof( SpecialPowerTemplate, m_reloadTime ) },
	{ "RequiredScience",					INI::parseScience,								nullptr, offsetof( SpecialPowerTemplate, m_requiredScience ) },
	{ "InitiateSound",						INI::parseAudioEventRTS,					nullptr,	offsetof( SpecialPowerTemplate, m_initiateSound ) },
	{ "InitiateAtLocationSound",	INI::parseAudioEventRTS,					nullptr,	offsetof( SpecialPowerTemplate, m_initiateAtLocationSound ) },
	{ "PublicTimer",							INI::parseBool,										nullptr, offsetof( SpecialPowerTemplate, m_publicTimer ) },
	{ "Enum",											INI::parseIndexList,							SpecialPowerMaskType::getBitNames(), offsetof( SpecialPowerTemplate, m_type ) },
	{ "DetectionTime",						INI::parseDurationUnsignedInt,		nullptr,	offsetof( SpecialPowerTemplate, m_detectionTime ) },
	{ "SharedSyncedTimer",				INI::parseBool,										nullptr, offsetof( SpecialPowerTemplate, m_sharedNSync ) },
	{ "ViewObjectDuration",				INI::parseDurationUnsignedInt,		nullptr,	offsetof( SpecialPowerTemplate, m_viewObjectDuration ) },
	{ "ViewObjectRange",					INI::parseReal,										nullptr,	offsetof( SpecialPowerTemplate, m_viewObjectRange ) },
	{ "RadiusCursorRadius",				INI::parseReal,										nullptr,	offsetof( SpecialPowerTemplate, m_radiusCursorRadius ) },
	{ "ShortcutPower",						INI::parseBool,										nullptr, offsetof( SpecialPowerTemplate, m_shortcutPower ) },
	{ "AcademyClassify",					INI::parseIndexList,			TheAcademyClassificationTypeNames, offsetof( SpecialPowerTemplate, m_academyClassificationType ) },
	{ nullptr,	nullptr, nullptr,	0 }

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerTemplate::SpecialPowerTemplate()
{
	m_id = 0;
	m_type = SPECIAL_INVALID;
	m_reloadTime = 0;
	m_requiredScience = SCIENCE_INVALID;
	m_publicTimer = FALSE;
	m_detectionTime = DEFAULT_DEFECTION_DETECTION_PROTECTION_TIME_LIMIT;
	m_sharedNSync = FALSE;
	m_viewObjectDuration = 0;
	m_viewObjectRange = 0;
	m_radiusCursorRadius = 0;
	m_shortcutPower = FALSE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerTemplate::~SpecialPowerTemplate()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerStore::SpecialPowerStore()
{

	m_nextSpecialPowerID = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerStore::~SpecialPowerStore()
{

	// delete all templates
	for( size_t i = 0; i < m_specialPowerTemplates.size(); ++i )
		deleteInstance(m_specialPowerTemplates[ i ]);

	// erase the list
	m_specialPowerTemplates.clear();

	// set our count to zero
	m_nextSpecialPowerID = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerTemplate* SpecialPowerStore::findSpecialPowerTemplatePrivate( AsciiString name )
{

	// search the template list for matching name
	for( size_t i = 0; i < m_specialPowerTemplates.size(); ++i )
		if( m_specialPowerTemplates[ i ]->getName() == name )
			return m_specialPowerTemplates[ i ];

	return nullptr;  // not found

}

//-------------------------------------------------------------------------------------------------
/** Find a special power template given unique ID */
//-------------------------------------------------------------------------------------------------
const SpecialPowerTemplate *SpecialPowerStore::findSpecialPowerTemplateByID( UnsignedInt id )
{

	// search the template list for matching name
	for( size_t i = 0; i < m_specialPowerTemplates.size(); ++i )
		if( m_specialPowerTemplates[ i ]->getID() == id )
			return m_specialPowerTemplates[ i ];

	return nullptr;  // not found

}

//-------------------------------------------------------------------------------------------------
/** Find a special power template given index (WB) */
//-------------------------------------------------------------------------------------------------
const SpecialPowerTemplate *SpecialPowerStore::getSpecialPowerTemplateByIndex( UnsignedInt index )
{

	if (index >= 0 && index < m_specialPowerTemplates.size())
		return m_specialPowerTemplates[ index ];

	return nullptr;  // not found

}

//-------------------------------------------------------------------------------------------------
/** Return the size of the store (WB) */
//-------------------------------------------------------------------------------------------------
Int SpecialPowerStore::getNumSpecialPowers()
{

	return m_specialPowerTemplates.size();

}

//-------------------------------------------------------------------------------------------------
/** does the object (and therefore the player) meet all the requirements to use this power */
//-------------------------------------------------------------------------------------------------
Bool SpecialPowerStore::canUseSpecialPower( Object *obj, const SpecialPowerTemplate *specialPowerTemplate )
{

	// sanity
	if( obj == nullptr || specialPowerTemplate == nullptr )
		return FALSE;

	// as a first sanity check, the object must have a module capable of executing the power
	if( obj->getSpecialPowerModule( specialPowerTemplate ) == nullptr )
		return FALSE;

	//
	// in order to execute the special powers we have attached special power modules to the objects
	// that can use them.  However, just because an object has a module that is capable of
	// doing the power, does not mean the object and the player can actually execute the
	// power because some powers require a specialized science that the player must select and
	// they cannot have all of them.
	//

	// check for required science
	ScienceType requiredScience = specialPowerTemplate->getRequiredScience();
	if( requiredScience != SCIENCE_INVALID )
	{
		Player *player = obj->getControllingPlayer();

		if( player->hasScience( requiredScience ) == FALSE )
			return FALSE;

	}


	// I THINK THIS IS WHERE WE BAIL OUT IF A DIFFERENT CONYARD IS ALREADY CHARGING THIS SPECIAL RIGHT NOW //LORENZEN


	// all is well
	return TRUE;

}

//-------------------------------------------------------------------------------------------------
/** Reset */
//-------------------------------------------------------------------------------------------------
void SpecialPowerStore::reset()
{
	for (SpecialPowerTemplatePtrVector::iterator it = m_specialPowerTemplates.begin(); it != m_specialPowerTemplates.end(); /*++it*/)
	{
		SpecialPowerTemplate* si = *it;
		Overridable* temp = si->deleteOverrides();
		if (temp == nullptr)
		{
			it = m_specialPowerTemplates.erase(it);
		}
		else
		{
			++it;
		}
	}
}
