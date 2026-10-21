// bfme-native: RotWK attribute modifiers ("ModifierList" blocks in Data\INI\AttributeModifier.ini).
//
// A ModifierList is a package of bonuses (armor, damage, speed, ...) that can be put on an object by
// upgrades, auras, weapons, special powers, leadership and so on. Layout from game.dat field table
// 0xC4EAF0 (Modifier 0x00806264, Category 0x00804DEA, Upgrade 0x008050D3); type and category name
// tables at 0xD8AF70 / 0xD9FA68.
#pragma once

#include "Common/AsciiString.h"
#include "Common/GameType.h"
#include "Common/ModelState.h"
#include "Common/SubsystemInterface.h"
#include "GameLogic/Damage.h"
#include <map>
#include <vector>

class FXList;
class INI;
class Object;
class UpgradeTemplate;

enum AttributeModifierType CPP_11(: Int)
{
	ATTRIBUTE_NONE = 0,
	ATTRIBUTE_ARMOR,							///< additive, capped by GameData AttributeModifierArmorMaxBonus
	ATTRIBUTE_DAMAGE_ADD,					///< additive, added to base damage
	ATTRIBUTE_DAMAGE_MULT,				///< multiplicative
	ATTRIBUTE_RESIST_FEAR,
	ATTRIBUTE_RESIST_TERROR,
	ATTRIBUTE_EXPERIENCE,
	ATTRIBUTE_RANGE,
	ATTRIBUTE_SPEED,
	ATTRIBUTE_CRUSH_DECELERATE,
	ATTRIBUTE_RESIST_KNOCKBACK,
	ATTRIBUTE_SPELL_DAMAGE,
	ATTRIBUTE_RECHARGE_TIME,
	ATTRIBUTE_PRODUCTION,
	ATTRIBUTE_HEALTH,
	ATTRIBUTE_HEALTH_MULT,
	ATTRIBUTE_VISION,
	ATTRIBUTE_BOUNTY_PERCENTAGE,
	ATTRIBUTE_MINIMUM_CRUSH_VELOCITY,
	ATTRIBUTE_AUTO_HEAL,
	ATTRIBUTE_SHROUD_CLEARING,
	ATTRIBUTE_RATE_OF_FIRE,
	ATTRIBUTE_DAMAGE_STRUCTURE_BOUNTY_ADD,
	ATTRIBUTE_CRUSHER_LEVEL,
	ATTRIBUTE_COMMAND_POINT_BONUS,
	ATTRIBUTE_CRUSHABLE_LEVEL,
	ATTRIBUTE_CRUSHED_DECELERATE,
	ATTRIBUTE_INVULNERABLE,

	ATTRIBUTE_COUNT
};

enum AttributeModifierCategory CPP_11(: Int)
{
	AMCAT_NONE = 0,
	AMCAT_LEADERSHIP,
	AMCAT_FORMATION,
	AMCAT_SPELL,
	AMCAT_WEAPON,
	AMCAT_STRUCTURE,
	AMCAT_LEVEL,
	AMCAT_BUFF,
	AMCAT_DEBUFF,
	AMCAT_STUN,
	AMCAT_INNATE_ARMOR,
	AMCAT_INNATE_DAMAGEMULT,
	AMCAT_INNATE_VISION,
	AMCAT_INNATE_AUTOHEAL,
	AMCAT_INNATE_HEALTH,

	AMCAT_COUNT
};

extern const char *const TheAttributeModifierTypeNames[];
extern const char *const TheAttributeModifierCategoryNames[];

//-------------------------------------------------------------------------------------------------
class AttributeModifierList
{
public:
	struct Entry
	{
		AttributeModifierType m_type;
		Real m_value;
		DamageTypeFlags m_damageTypes;	///< INVULNERABLE: which damage types
	};

	AttributeModifierList();

	AsciiString m_name;
	AttributeModifierCategory m_category;
	UnsignedInt m_duration;					///< frames; 0 = until removed
	std::vector<Entry> m_entries;
	ModelConditionFlags m_modelConditionsSet;
	ModelConditionFlags m_modelConditionsClear;
	const FXList* m_fx[3];
	const FXList* m_endFX[3];
	AsciiString m_upgradeName;
	UnsignedInt m_upgradeDelay;
	Bool m_multiLevelFX;
	Bool m_replaceInCategoryIfLongest;
	Bool m_ignoreIfAnticategoryActive;

	static const FieldParse* getFieldParse();
	static void parseModifier(INI* ini, void* instance, void* store, const void* userData);
	static void parseUpgrade(INI* ini, void* instance, void* store, const void* userData);
};

//-------------------------------------------------------------------------------------------------
class AttributeModifierStore : public SubsystemInterface
{
public:
	AttributeModifierStore();
	virtual ~AttributeModifierStore();

	virtual void init() {}
	virtual void reset();
	virtual void update();

	const AttributeModifierList* findList(const AsciiString& name) const;

	static void parseModifierListDefinition(INI* ini);

	// ---- per-object state ----
	/// put 'list' on 'obj'. duration 0 uses the list's Duration; permanent ignores durations.
	void apply(Object* obj, const AttributeModifierList* list, ObjectID source, UnsignedInt duration = 0, Bool permanent = FALSE);
	void remove(Object* obj, const AttributeModifierList* list);
	void removeCategory(Object* obj, AttributeModifierCategory cat);
	Bool has(const Object* obj, const AttributeModifierList* list) const;

	Real getSum(const Object* obj, AttributeModifierType t) const;			///< additive types (0 when none)
	Real getProduct(const Object* obj, AttributeModifierType t) const;		///< multiplicative types (1 when none)
	Bool isInvulnerableTo(const Object* obj, DamageType dt) const;

	void onObjectDestroyed(ObjectID id);

	/// set model conditions on obj for 'frames' frames, then clear them again
	void applyTimedModelConditions(Object* obj, const ModelConditionFlags& flags, UnsignedInt frames);

private:
	struct Active
	{
		const AttributeModifierList* m_list;
		UnsignedInt m_expireFrame;	///< 0 = never
		ObjectID m_source;
	};
	typedef std::vector<Active> ActiveVec;

	void onGain(Object* obj, const AttributeModifierList* list);
	void onLose(Object* obj, const AttributeModifierList* list);

	std::map<AsciiString, AttributeModifierList*> m_lists;
	std::map<ObjectID, ActiveVec> m_active;

	struct TimedConditions
	{
		ObjectID m_id;
		ModelConditionFlags m_flags;
		UnsignedInt m_expireFrame;
	};
	std::vector<TimedConditions> m_timedConditions;
};

extern AttributeModifierStore* TheAttributeModifierStore;

//-------------------------------------------------------------------------------------------------
/// armor attributes applied after Armor.ini: amount * (1 - min(sum(ARMOR), cap)); 0 when invulnerable
Real bfmeApplyArmorAttributes(const Object* victim, DamageType dt, Real amount);
/// damage attributes of the firer: (amount + DAMAGE_ADD) * DAMAGE_MULT (SPELL_DAMAGE for magic)
Real bfmeApplyDamageAttributes(const Object* source, DamageType dt, Real amount, Bool acceptDamageAdd);
