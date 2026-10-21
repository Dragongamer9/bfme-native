// bfme-native: RotWK stances (stances.ini) and StancesBehavior.
//
//   StanceTemplate FighterHorde
//     Stance Aggressive
//       AttributeModifier FighterHordeStanceAggressive
//       MeleeBehavior = Amoeba ... End
//     End
//   End
// A unit with StancesBehavior uses its template's stances; the chosen stance's attribute modifier is
// applied to the unit (to the soldiers, for a horde). No stance is set by default.
#pragma once

#include "Common/AsciiString.h"
#include "Common/SubsystemInterface.h"
#include "GameLogic/Module/BehaviorModule.h"
#include <map>
#include <vector>

class INI;

struct StanceEntry
{
	AsciiString m_name;
	AsciiString m_attributeModifier;
};

struct StanceTemplate
{
	AsciiString m_name;
	std::vector<StanceEntry> m_stances;
	const StanceEntry* find(const AsciiString& stance) const;
};

class StanceStore : public SubsystemInterface
{
public:
	virtual ~StanceStore() {}
	virtual void init() {}
	virtual void reset() {}
	virtual void update() {}
	static void parseStanceTemplateDefinition(INI* ini);
	const StanceTemplate* find(const AsciiString& name) const;
private:
	std::map<AsciiString, StanceTemplate> m_templates;	///< lower-case name
};
extern StanceStore* TheStanceStore;

class StancesBehaviorModuleData : public BehaviorModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
	AsciiString m_stanceTemplate;
};

class StancesBehavior : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StancesBehavior, "StancesBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( StancesBehavior, StancesBehaviorModuleData )
public:
	StancesBehavior( Thing *thing, const ModuleData* moduleData );
	/// switch stance; an empty name clears it. FALSE if the template has no such stance.
	Bool setStance(const AsciiString& stance);
	const AsciiString& getStance() const { return m_stance; }
	static StancesBehavior* findOn(const Object* obj);
private:
	void applyModifier(const AsciiString& list, Bool add);
	AsciiString m_stance;
};
