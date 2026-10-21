// bfme-native: RotWK hordes.
//
// A horde is an invisible, immortal "leader" object (HordeAIUpdate + HordeContain) that the player
// selects and orders; its members are ordinary units spawned from InitialPayload that keep a rank
// formation (RankInfo) around it and fight on their own when the horde attacks. Field layout from
// game.dat table 0xC5BB50; this is a first implementation of the behaviour (formation following,
// attack hand-off, horde dies with its last member), not yet a decompiled port of every detail.
#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/ObjectFilter.h"
#include <vector>

class ThingTemplate;

struct HordeRankInfo
{
	Int m_rankNumber;
	AsciiString m_unitType;
	std::vector<Coord2D> m_positions;	///< X forward, Y left, relative to the horde
};

struct HordePayload
{
	AsciiString m_templateName;
	Int m_count;
};

class HordeContainModuleData : public UpdateModuleData
{
public:
	HordeContainModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);

	std::vector<HordePayload> m_initialPayload;
	Int m_slots;
	ObjectFilter m_passengerFilter;
	Bool m_showPips;
	std::vector<HordeRankInfo> m_rankInfo;
	Coord2D m_randomOffset;
	Int m_ranksToReleaseWhenAttacking;
	Int m_ranksToJustFreeWhenAttacking;
	Real m_meleeAttackLeashDistance;
	Bool m_thisFormationIsTheMainFormation;
	std::vector<AsciiString> m_attributeModifiers;
	std::vector<AsciiString> m_bannerCarriersAllowed;
	Int m_minimumHordeSize;

	// parsed for completeness, not used yet
	AsciiString m_unusedString;
	Real m_unusedReal;
	Bool m_unusedBool;
	Int m_unusedInt;
	Coord3D m_unusedCoord;
	std::vector<AsciiString> m_unusedStrings;

	static void parseInitialPayload(INI* ini, void* instance, void* store, const void* userData);
	static void parseRankInfo(INI* ini, void* instance, void* store, const void* userData);
	static void parseRandomOffset(INI* ini, void* instance, void* store, const void* userData);
	static void parseLine(INI* ini, void* instance, void* store, const void* userData);
	static void parseSubBlock(INI* ini, void* instance, void* store, const void* userData);
};

class HordeContain : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HordeContain, "HordeContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HordeContain, HordeContainModuleData )

public:
	HordeContain( Thing *thing, const ModuleData* moduleData );

	virtual UpdateSleepTime update() override;

	/// members still alive
	Int getMemberCount() const;
	/// the member nearest to 'pos' (to redirect hits on the horde), or null
	Object* getMemberClosestTo(const Coord3D* pos) const;

	static HordeContain* findOn(const Object* obj);

private:
	void spawnMembers();
	void slotPosition(Int index, Coord3D* out) const;

	std::vector<ObjectID> m_members;
	std::vector<Coord3D> m_lastOrder;
	Bool m_spawned;
};

/// the unit type a horde template is made of (first InitialPayload), or null
const ThingTemplate* bfmeHordeMemberTemplate(const ThingTemplate* hordeTemplate);
