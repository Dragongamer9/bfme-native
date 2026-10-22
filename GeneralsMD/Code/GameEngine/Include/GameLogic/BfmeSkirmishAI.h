// bfme-native: a computer opponent for RotWK skirmish games.
//
// RotWK's own skirmish AI builds from "AIBase" maps and army definitions in SkirmishAIData.ini. The
// AI base maps Edain points at do not ship with it, so this player is driven by the data that does:
//   - AIDozerAssignment: the side's builder (Edain's free-build porters)
//   - ArmyDefinition: the side's army make-up per game phase, its hero order and farm count
// and by the builder's own command set (what it can construct). It builds farms and the buildings
// that train its army around its start, trains units by the army percentages, and sends the army
// at the nearest enemy building in waves.
#pragma once

#include "Common/AsciiString.h"
#include "Common/GameType.h"
#include "Lib/BaseType.h"
#include <vector>

class INI;
class Player;
class Object;
class ThingTemplate;

struct BfmeArmyMember
{
	AsciiString m_unit;
	Real m_percent[3];	///< per phase (rush, mid game, end game)
	BfmeArmyMember() { m_percent[0] = m_percent[1] = m_percent[2] = 0.0f; }
};

struct BfmeArmyDefinition
{
	AsciiString m_name;
	AsciiString m_side;
	std::vector<BfmeArmyMember> m_members;
	std::vector<AsciiString> m_heroes;
	Real m_phaseRush;			///< seconds
	Real m_phaseMidGame;	///< seconds after the rush phase
	Int m_minFarms;
	BfmeArmyDefinition() : m_phaseRush(300.0f), m_phaseMidGame(400.0f), m_minFarms(6) {}
};

class BfmeSkirmishAIStore
{
public:
	const BfmeArmyDefinition* findArmy(const AsciiString& side) const;
	AsciiString findDozer(const AsciiString& side) const;

	static void parseArmyDefinition(INI* ini);
	static void parseDozerAssignment(INI* ini);

	std::vector<BfmeArmyDefinition> m_armies;
	std::vector< std::pair<AsciiString, AsciiString> > m_dozers;	///< side, unit
};
extern BfmeSkirmishAIStore TheBfmeSkirmishAIStore;

/// the per-player brain; owned by AISkirmishPlayer
class BfmeSkirmishAI
{
public:
	explicit BfmeSkirmishAI(Player* player);
	void update();

private:
	Int phase() const;
	void think();
	void buildBase(std::vector<Object*>& dozers);
	void trainArmy();
	void attack();
	Bool placeBuilding(Object* dozer, const ThingTemplate* what);
	const ThingTemplate* producerFor(Object* dozer, const ThingTemplate* unit) const;
	Bool canTrain(const ThingTemplate* building, const ThingTemplate* unit) const;

	Player* m_player;
	Bool m_started;
	Coord3D m_base;
	UnsignedInt m_nextThink;
	UnsignedInt m_nextAttack;
	UnsignedInt m_startFrame;
	Int m_placementTry;
};
