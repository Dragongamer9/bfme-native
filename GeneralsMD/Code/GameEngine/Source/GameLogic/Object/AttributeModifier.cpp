// bfme-native: RotWK attribute modifiers, see AttributeModifier.h
#include "PreRTS.h"

#include "GameLogic/AttributeModifier.h"

#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

AttributeModifierStore* TheAttributeModifierStore = nullptr;

const char *const TheAttributeModifierTypeNames[] =
{
	"ATTRIBUTE_NONE", "ARMOR", "DAMAGE_ADD", "DAMAGE_MULT", "RESIST_FEAR", "RESIST_TERROR", "EXPERIENCE",
	"RANGE", "SPEED", "CRUSH_DECELERATE", "RESIST_KNOCKBACK", "SPELL_DAMAGE", "RECHARGE_TIME", "PRODUCTION",
	"HEALTH", "HEALTH_MULT", "VISION", "BOUNTY_PERCENTAGE", "MINIMUM_CRUSH_VELOCITY", "AUTO_HEAL",
	"SHROUD_CLEARING", "RATE_OF_FIRE", "DAMAGE_STRUCTURE_BOUNTY_ADD", "CRUSHER_LEVEL", "COMMAND_POINT_BONUS",
	"CRUSHABLE_LEVEL", "CRUSHED_DECELERATE", "INVULNERABLE",
	nullptr
};
static_assert(ARRAY_SIZE(TheAttributeModifierTypeNames) == ATTRIBUTE_COUNT + 1, "Incorrect array size");

const char *const TheAttributeModifierCategoryNames[] =
{
	"NONE", "LEADERSHIP", "FORMATION", "SPELL", "WEAPON", "STRUCTURE", "LEVEL", "BUFF", "DEBUFF", "STUN",
	"INNATE_ARMOR", "INNATE_DAMAGEMULT", "INNATE_VISION", "INNATE_AUTOHEAL", "INNATE_HEALTH",
	nullptr
};
static_assert(ARRAY_SIZE(TheAttributeModifierCategoryNames) == AMCAT_COUNT + 1, "Incorrect array size");

//=================================================================================================
// AttributeModifierList
//=================================================================================================
AttributeModifierList::AttributeModifierList() :
	m_category(AMCAT_NONE),
	m_duration(0),
	m_upgradeDelay(0),
	m_multiLevelFX(FALSE),
	m_replaceInCategoryIfLongest(FALSE),
	m_ignoreIfAnticategoryActive(FALSE)
{
	for (Int i = 0; i < 3; ++i)
	{
		m_fx[i] = nullptr;
		m_endFX[i] = nullptr;
	}
}

void AttributeModifierList::parseModifier(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	// "Modifier = TYPE VALUE [DAMAGE_TYPE ...]"; VALUE is a percentage or a plain number
	std::vector<Entry>* v = (std::vector<Entry>*)store;
	Entry e;
	e.m_type = (AttributeModifierType)INI::scanIndexList(ini->getNextToken(), TheAttributeModifierTypeNames);
	const char* val = ini->getNextToken();
	e.m_value = strchr(val, '%') ? INI::scanPercentToReal(val) : INI::scanReal(val);
	for (const char* tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull())
	{
		const char* const* names = DamageTypeFlags::getBitNames();
		for (Int i = 0; names[i]; ++i)
			if (stricmp(names[i], tok) == 0)
				e.m_damageTypes.set((DamageType)i);
	}
	v->push_back(e);
}

void AttributeModifierList::parseUpgrade(INI* ini, void* instance, void* /*store*/, const void* /*userData*/)
{
	// "Upgrade = Upgrade_Name [Delay ms]"
	AttributeModifierList* self = (AttributeModifierList*)instance;
	self->m_upgradeName = ini->getNextToken();
	const char* tok = ini->getNextTokenOrNull();
	if (tok && stricmp(tok, "Delay") == 0)
	{
		const char* ms = ini->getNextTokenOrNull();
		if (ms)
			self->m_upgradeDelay = (UnsignedInt)ceilf(ConvertDurationFromMsecsToFrames((Real)INI::scanInt(ms)));
	}
}

static void parseCategory(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	Int c = INI::scanIndexList(ini->getNextToken(), TheAttributeModifierCategoryNames);
	if (c <= 0 || c >= AMCAT_COUNT)
		throw INI_INVALID_DATA;	// "Invalid Category encountered by ModifierList"
	*(AttributeModifierCategory*)store = (AttributeModifierCategory)c;
}

const FieldParse* AttributeModifierList::getFieldParse()
{
	static const FieldParse fp[] =
	{
		{ "Modifier",										AttributeModifierList::parseModifier,	nullptr, offsetof(AttributeModifierList, m_entries) },
		{ "Duration",										INI::parseDurationUnsignedInt,				nullptr, offsetof(AttributeModifierList, m_duration) },
		{ "ModelCondition",							ModelConditionFlags::parseFromINI,		nullptr, offsetof(AttributeModifierList, m_modelConditionsSet) },
		{ "ClearModelCondition",				ModelConditionFlags::parseFromINI,		nullptr, offsetof(AttributeModifierList, m_modelConditionsClear) },
		{ "Category",										parseCategory,												nullptr, offsetof(AttributeModifierList, m_category) },
		{ "FX",													INI::parseFXList,											nullptr, offsetof(AttributeModifierList, m_fx[0]) },
		{ "FX2",												INI::parseFXList,											nullptr, offsetof(AttributeModifierList, m_fx[1]) },
		{ "FX3",												INI::parseFXList,											nullptr, offsetof(AttributeModifierList, m_fx[2]) },
		{ "EndFX",											INI::parseFXList,											nullptr, offsetof(AttributeModifierList, m_endFX[0]) },
		{ "EndFX2",											INI::parseFXList,											nullptr, offsetof(AttributeModifierList, m_endFX[1]) },
		{ "EndFX3",											INI::parseFXList,											nullptr, offsetof(AttributeModifierList, m_endFX[2]) },
		{ "Upgrade",										AttributeModifierList::parseUpgrade,	nullptr, 0 },
		{ "MultiLevelFX",								INI::parseBool,												nullptr, offsetof(AttributeModifierList, m_multiLevelFX) },
		{ "ReplaceInCategoryIfLongest",	INI::parseBool,												nullptr, offsetof(AttributeModifierList, m_replaceInCategoryIfLongest) },
		{ "IgnoreIfAnticategoryActive",	INI::parseBool,												nullptr, offsetof(AttributeModifierList, m_ignoreIfAnticategoryActive) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return fp;
}

//=================================================================================================
// AttributeModifierStore
//=================================================================================================
AttributeModifierStore::AttributeModifierStore() {}

AttributeModifierStore::~AttributeModifierStore()
{
	for (std::map<AsciiString, AttributeModifierList*>::iterator it = m_lists.begin(); it != m_lists.end(); ++it)
		delete it->second;
	m_lists.clear();
}

void AttributeModifierStore::reset()
{
	// keep the INI definitions, drop per-game state
	m_active.clear();
	m_timedConditions.clear();
}

static AsciiString lowerName(const AsciiString& n)
{
	AsciiString l = n;
	l.toLower();
	return l;
}

const AttributeModifierList* AttributeModifierStore::findList(const AsciiString& name) const
{
	std::map<AsciiString, AttributeModifierList*>::const_iterator it = m_lists.find(lowerName(name));
	return it == m_lists.end() ? nullptr : it->second;
}

void AttributeModifierStore::parseModifierListDefinition(INI* ini)
{
	AsciiString name = ini->getNextToken();
	AttributeModifierList* list = new AttributeModifierList;
	list->m_name = name;
	ini->initFromINI(list, AttributeModifierList::getFieldParse());

	AsciiString key = lowerName(name);
	std::map<AsciiString, AttributeModifierList*>::iterator it = TheAttributeModifierStore->m_lists.find(key);
	if (it != TheAttributeModifierStore->m_lists.end())
	{
		delete it->second;	// a later definition replaces the earlier one
		it->second = list;
	}
	else
	{
		TheAttributeModifierStore->m_lists[key] = list;
	}
}

static Real valueOf(const AttributeModifierList* list, AttributeModifierType t, Bool additive)
{
	Real r = additive ? 0.0f : 1.0f;
	for (size_t i = 0; i < list->m_entries.size(); ++i)
	{
		if (list->m_entries[i].m_type != t)
			continue;
		if (additive)
			r += list->m_entries[i].m_value;
		else
			r *= list->m_entries[i].m_value;
	}
	return r;
}

void AttributeModifierStore::onGain(Object* obj, const AttributeModifierList* list)
{
	BodyModuleInterface* body = obj->getBodyModule();
	if (body)
	{
		const Real addHealth = valueOf(list, ATTRIBUTE_HEALTH, TRUE);
		if (addHealth != 0.0f)
			body->setMaxHealth(body->getMaxHealth() + addHealth, ADD_CURRENT_HEALTH_TOO);
		const Real multHealth = valueOf(list, ATTRIBUTE_HEALTH_MULT, FALSE);
		if (multHealth != 1.0f && multHealth > 0.0f)
			body->setMaxHealth(body->getMaxHealth() * multHealth, PRESERVE_RATIO);
	}
	if (list->m_modelConditionsClear.any() || list->m_modelConditionsSet.any())
		obj->clearAndSetModelConditionFlags(list->m_modelConditionsClear, list->m_modelConditionsSet);
	for (Int i = 0; i < 3; ++i)
		FXList::doFXObj(list->m_fx[i], obj);
	if (list->m_upgradeName.isNotEmpty() && TheUpgradeCenter)
	{
		const UpgradeTemplate* ut = TheUpgradeCenter->findUpgrade(list->m_upgradeName);
		if (ut && !obj->hasUpgrade(ut))
			obj->giveUpgrade(ut);	// TODO(bfme-native): honour "Delay"
	}
}

void AttributeModifierStore::onLose(Object* obj, const AttributeModifierList* list)
{
	BodyModuleInterface* body = obj->getBodyModule();
	if (body && !obj->isEffectivelyDead())
	{
		const Real multHealth = valueOf(list, ATTRIBUTE_HEALTH_MULT, FALSE);
		if (multHealth != 1.0f && multHealth > 0.0f)
			body->setMaxHealth(body->getMaxHealth() / multHealth, PRESERVE_RATIO);
		const Real addHealth = valueOf(list, ATTRIBUTE_HEALTH, TRUE);
		if (addHealth != 0.0f)
			body->setMaxHealth(max(1.0f, body->getMaxHealth() - addHealth), SAME_CURRENTHEALTH);
	}
	if (list->m_modelConditionsSet.any())
		obj->clearModelConditionFlags(list->m_modelConditionsSet);
	for (Int i = 0; i < 3; ++i)
		FXList::doFXObj(list->m_endFX[i], obj);
}

void AttributeModifierStore::apply(Object* obj, const AttributeModifierList* list, ObjectID source, UnsignedInt duration, Bool permanent)
{
	if (obj == nullptr || list == nullptr || obj->isEffectivelyDead())
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	const UnsignedInt dur = permanent ? 0 : (duration ? duration : list->m_duration);
	const UnsignedInt expire = dur ? now + dur : 0;

	ActiveVec& v = m_active[obj->getID()];
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (v[i].m_list == list)
		{
			// same list again: keep the longer one ("can't be given the same list twice")
			if (v[i].m_expireFrame != 0 && (expire == 0 || expire > v[i].m_expireFrame))
				v[i].m_expireFrame = expire;
			return;
		}
	}
	if (list->m_replaceInCategoryIfLongest && list->m_category != AMCAT_NONE)
	{
		for (size_t i = 0; i < v.size(); ++i)
		{
			if (v[i].m_list->m_category != list->m_category)
				continue;
			const Bool newIsLonger = expire == 0 || (v[i].m_expireFrame != 0 && expire > v[i].m_expireFrame);
			if (!newIsLonger)
				return;
			const AttributeModifierList* old = v[i].m_list;
			v.erase(v.begin() + i);
			onLose(obj, old);
			break;
		}
	}
	Active a;
	a.m_list = list;
	a.m_expireFrame = expire;
	a.m_source = source;
	m_active[obj->getID()].push_back(a);
	onGain(obj, list);
}

void AttributeModifierStore::remove(Object* obj, const AttributeModifierList* list)
{
	if (obj == nullptr || list == nullptr)
		return;
	std::map<ObjectID, ActiveVec>::iterator it = m_active.find(obj->getID());
	if (it == m_active.end())
		return;
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		if (it->second[i].m_list == list)
		{
			it->second.erase(it->second.begin() + i);
			onLose(obj, list);
			break;
		}
	}
	if (it->second.empty())
		m_active.erase(it);
}

void AttributeModifierStore::removeCategory(Object* obj, AttributeModifierCategory cat)
{
	if (obj == nullptr)
		return;
	std::map<ObjectID, ActiveVec>::iterator it = m_active.find(obj->getID());
	if (it == m_active.end())
		return;
	ActiveVec keep;
	ActiveVec lost;
	for (size_t i = 0; i < it->second.size(); ++i)
		(it->second[i].m_list->m_category == cat ? lost : keep).push_back(it->second[i]);
	it->second = keep;
	for (size_t i = 0; i < lost.size(); ++i)
		onLose(obj, lost[i].m_list);
	if (it->second.empty())
		m_active.erase(it);
}

Bool AttributeModifierStore::has(const Object* obj, const AttributeModifierList* list) const
{
	if (obj == nullptr)
		return FALSE;
	std::map<ObjectID, ActiveVec>::const_iterator it = m_active.find(obj->getID());
	if (it == m_active.end())
		return FALSE;
	for (size_t i = 0; i < it->second.size(); ++i)
		if (it->second[i].m_list == list)
			return TRUE;
	return FALSE;
}

Real AttributeModifierStore::getSum(const Object* obj, AttributeModifierType t) const
{
	Real r = 0.0f;
	if (obj == nullptr)
		return r;
	std::map<ObjectID, ActiveVec>::const_iterator it = m_active.find(obj->getID());
	if (it == m_active.end())
		return r;
	for (size_t i = 0; i < it->second.size(); ++i)
		r += valueOf(it->second[i].m_list, t, TRUE);
	return r;
}

Real AttributeModifierStore::getProduct(const Object* obj, AttributeModifierType t) const
{
	Real r = 1.0f;
	if (obj == nullptr)
		return r;
	std::map<ObjectID, ActiveVec>::const_iterator it = m_active.find(obj->getID());
	if (it == m_active.end())
		return r;
	for (size_t i = 0; i < it->second.size(); ++i)
		r *= valueOf(it->second[i].m_list, t, FALSE);
	return r;
}

Bool AttributeModifierStore::isInvulnerableTo(const Object* obj, DamageType dt) const
{
	if (obj == nullptr)
		return FALSE;
	std::map<ObjectID, ActiveVec>::const_iterator it = m_active.find(obj->getID());
	if (it == m_active.end())
		return FALSE;
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		const AttributeModifierList* l = it->second[i].m_list;
		for (size_t e = 0; e < l->m_entries.size(); ++e)
			if (l->m_entries[e].m_type == ATTRIBUTE_INVULNERABLE && l->m_entries[e].m_damageTypes.test(dt))
				return TRUE;
	}
	return FALSE;
}

void AttributeModifierStore::onObjectDestroyed(ObjectID id)
{
	m_active.erase(id);
}

void AttributeModifierStore::applyTimedModelConditions(Object* obj, const ModelConditionFlags& flags, UnsignedInt frames)
{
	if (obj == nullptr || !flags.any())
		return;
	obj->setModelConditionFlags(flags);
	if (frames == 0)
		return;
	TimedConditions t;
	t.m_id = obj->getID();
	t.m_flags = flags;
	t.m_expireFrame = TheGameLogic->getFrame() + frames;
	m_timedConditions.push_back(t);
}

void AttributeModifierStore::update()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	for (size_t i = 0; i < m_timedConditions.size(); )
	{
		if (m_timedConditions[i].m_expireFrame <= now)
		{
			Object* o = TheGameLogic->findObjectByID(m_timedConditions[i].m_id);
			if (o)
				o->clearModelConditionFlags(m_timedConditions[i].m_flags);
			m_timedConditions.erase(m_timedConditions.begin() + i);
		}
		else
		{
			++i;
		}
	}
	if (m_active.empty())
		return;
	std::vector<ObjectID> empty;
	for (std::map<ObjectID, ActiveVec>::iterator it = m_active.begin(); it != m_active.end(); ++it)
	{
		Object* obj = TheGameLogic->findObjectByID(it->first);
		if (obj == nullptr)
		{
			empty.push_back(it->first);
			continue;
		}
		ActiveVec& v = it->second;
		for (size_t i = 0; i < v.size(); )
		{
			if (v[i].m_expireFrame != 0 && v[i].m_expireFrame <= now)
			{
				const AttributeModifierList* l = v[i].m_list;
				v.erase(v.begin() + i);
				onLose(obj, l);
			}
			else
			{
				++i;
			}
		}
		if (v.empty())
			empty.push_back(it->first);
	}
	for (size_t i = 0; i < empty.size(); ++i)
		m_active.erase(empty[i]);
}

//=================================================================================================
Real bfmeApplyArmorAttributes(const Object* victim, DamageType dt, Real amount)
{
	if (TheAttributeModifierStore == nullptr || victim == nullptr)
		return amount;
	if (TheAttributeModifierStore->isInvulnerableTo(victim, dt))
		return 0.0f;
	Real armor = TheAttributeModifierStore->getSum(victim, ATTRIBUTE_ARMOR);
	if (armor != 0.0f)
	{
		const Real cap = TheGlobalData->m_attributeModifierArmorMaxBonus;
		if (armor > cap)
			armor = cap;
		amount *= (1.0f - armor);
	}
	return amount;
}

Real bfmeApplyDamageAttributes(const Object* source, DamageType dt, Real amount, Bool acceptDamageAdd)
{
	if (TheAttributeModifierStore == nullptr || source == nullptr)
		return amount;
	if (acceptDamageAdd && amount != 0.0f)
		amount += TheAttributeModifierStore->getSum(source, ATTRIBUTE_DAMAGE_ADD);
	// magic damage uses SPELL_DAMAGE instead of DAMAGE_MULT (0x0090E28C)
	amount *= TheAttributeModifierStore->getProduct(source, dt == DAMAGE_MAGIC ? ATTRIBUTE_SPELL_DAMAGE : ATTRIBUTE_DAMAGE_MULT);
	return amount;
}

//-------------------------------------------------------------------------------------------------
void INI::parseModifierListDefinition( INI *ini )
{
	AttributeModifierStore::parseModifierListDefinition(ini);
}
