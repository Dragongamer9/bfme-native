// bfme-native: RotWK weapon nuggets, see WeaponNugget.h
#include "PreRTS.h"

#define DEFINE_DEATH_NAMES

#include "GameLogic/WeaponNugget.h"
#include "GameLogic/AttributeModifier.h"

#include "Common/GameUtility.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

// "unset" value RotWK uses for DamageTaperOff / DamageMaxHeight / DamageMaxHeightAboveTerrain
static const Real NUGGET_UNSET = -1.0f;

//=================================================================================================
// helpers
//=================================================================================================
static Object* damageCreditFor(Object* source)
{
	// a projectile's damage is credited to whoever launched it (same as Zero Hour)
	if (source && source->isKindOf(KINDOF_PROJECTILE))
	{
		for (BehaviorModule** u = source->getBehaviorModules(); *u; ++u)
		{
			ProjectileUpdateInterface* pui = (*u)->getProjectileUpdateInterface();
			if (pui)
			{
				Object* launcher = TheGameLogic->findObjectByID(pui->projectileGetLauncherID());
				return launcher ? launcher : source;
			}
		}
	}
	return source;
}

/// RotWK arc test: the arc is centred on the attack direction (firer -> impact point)
static Bool insideArc(const Object* source, const Coord3D* impact, const Object* victim, Real arc, Bool inverted)
{
	if (arc <= 0.0f || arc >= PI)
		return TRUE;
	if (source == nullptr || victim == nullptr)
		return TRUE;

	Coord3D attackDir;
	attackDir.set(*impact);
	attackDir.sub(*source->getPosition());
	if (attackDir.length() == 0.0f)
	{
		const Matrix3D* m = source->getTransformMatrix();
		attackDir.set(m->Get_X_Vector().X, m->Get_X_Vector().Y, m->Get_X_Vector().Z);
	}
	Coord3D toVictim;
	toVictim.set(*victim->getPosition());
	toVictim.sub(*impact);
	if (toVictim.length() == 0.0f)
		return !inverted;
	attackDir.normalize();
	toVictim.normalize();
	const Real dot = attackDir.x * toVictim.x + attackDir.y * toVictim.y + attackDir.z * toVictim.z;
	const Bool inside = dot >= Cos(arc);
	return inverted ? !inside : inside;
}

/// common affect test for area hits (weapon RadiusDamageAffects, as in Zero Hour)
static Bool areaAffects(const WeaponTemplate* wt, const Object* source, const Object* victim, const Object* primaryVictim)
{
	if (victim == primaryVictim || source == nullptr)
		return TRUE;
	const Int affects = wt->getAffectsMask();
	if ((affects & WEAPON_AFFECTS_SELF) == 0)
	{
		if (source == victim || source->getProducerID() == victim->getID())
			return FALSE;
	}
	if ((affects & WEAPON_DOESNT_AFFECT_SIMILAR) && source->getTemplate()->isEquivalentTo(victim->getTemplate())
		&& source->getRelationship(victim) == ALLIES)
		return FALSE;
	if ((affects & WEAPON_DOESNT_AFFECT_AIRBORNE) && victim->isSignificantlyAboveTerrain())
		return FALSE;
	Relationship r = victim->getRelationship(source);
	Int required = (r == ALLIES) ? WEAPON_AFFECTS_ALLIES : (r == ENEMIES ? WEAPON_AFFECTS_ENEMIES : WEAPON_AFFECTS_NEUTRALS);
	return (affects & required) != 0;
}

/// gather victims: the direct victim when radius is 0, else everything in range
template <class Fn>
static void forEachVictim(const Coord3D* pos, Object* primaryVictim, Real radius, Bool cylinder, Fn fn)
{
	if (radius <= 0.0f)
	{
		if (primaryVictim)
			fn(primaryVictim, 0.0f, FALSE);
		return;
	}
	if (pos == nullptr)
		return;
	Real distSqr = 0.0f;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(pos, radius,
		cylinder ? FROM_BOUNDINGSPHERE_2D : FROM_BOUNDINGSPHERE_3D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* o = iter->firstWithNumeric(&distSqr); o; o = iter->nextWithNumeric(&distSqr))
		fn(o, sqrtf(distSqr), TRUE);
}

//=================================================================================================
// WeaponNugget
//=================================================================================================
const FieldParse WeaponNugget::s_baseFieldParse[] =
{
	{ "RequiredUpgradeNames",		INI::parseAsciiStringVectorAppend,	nullptr, offsetof(WeaponNugget, m_requiredUpgradeNames) },
	{ "ForbiddenUpgradeNames",	INI::parseAsciiStringVectorAppend,	nullptr, offsetof(WeaponNugget, m_forbiddenUpgradeNames) },
	{ "SpecialObjectFilter",		ObjectFilter::parseFromINI,					nullptr, offsetof(WeaponNugget, m_specialObjectFilter) },
	{ nullptr, nullptr, nullptr, 0 }
};

WeaponNugget::WeaponNugget() {}
WeaponNugget::~WeaponNugget() {}

void WeaponNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	p.add(s_baseFieldParse);
}

static Bool sourceHasUpgrade(const Object* source, const AsciiString& name)
{
	const UpgradeTemplate* ut = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
	if (ut == nullptr)
		return FALSE;
	if (source->hasUpgrade(ut))
		return TRUE;
	const Player* p = source->getControllingPlayer();
	return p && p->hasUpgradeComplete(ut);
}

Bool WeaponNugget::isAllowed(const Object* source) const
{
	if (m_requiredUpgradeNames.empty() && m_forbiddenUpgradeNames.empty())
		return TRUE;
	if (source == nullptr)
		return m_requiredUpgradeNames.empty();
	for (size_t i = 0; i < m_requiredUpgradeNames.size(); ++i)
		if (!sourceHasUpgrade(source, m_requiredUpgradeNames[i]))
			return FALSE;
	for (size_t i = 0; i < m_forbiddenUpgradeNames.size(); ++i)
		if (sourceHasUpgrade(source, m_forbiddenUpgradeNames[i]))
			return FALSE;
	return TRUE;
}

Bool WeaponNugget::passesSpecialFilter(const Object* victim, const Object* source) const
{
	if (m_specialObjectFilter.isEmpty())
		return TRUE;
	return m_specialObjectFilter.matches(victim, source);
}

//=================================================================================================
// DamageNugget
//=================================================================================================
const FieldParse DamageNugget::s_fieldParse[] =
{
	{ "Damage",												INI::parseReal,						nullptr,							offsetof(DamageNugget, m_damage) },
	{ "DamageTaperOff",								INI::parseReal,						nullptr,							offsetof(DamageNugget, m_damageTaperOff) },
	{ "AcceptDamageAdd",							INI::parseBool,						nullptr,							offsetof(DamageNugget, m_acceptDamageAdd) },
	{ "Radius",												INI::parseReal,						nullptr,							offsetof(DamageNugget, m_radius) },
	{ "MinRadius",										INI::parseReal,						nullptr,							offsetof(DamageNugget, m_minRadius) },
	{ "DamageArc",										INI::parseAngleReal,			nullptr,							offsetof(DamageNugget, m_damageArc) },
	{ "DamageArcInverted",						INI::parseBool,						nullptr,							offsetof(DamageNugget, m_damageArcInverted) },
	{ "DamageMaxHeight",							INI::parseReal,						nullptr,							offsetof(DamageNugget, m_damageMaxHeight) },
	{ "DamageMaxHeightAboveTerrain",	INI::parseReal,						nullptr,							offsetof(DamageNugget, m_damageMaxHeightAboveTerrain) },
	{ "DelayTime",										INI::parseDurationUnsignedInt, nullptr,					offsetof(DamageNugget, m_delayTime) },
	{ "DamageType",										DamageTypeFlags::parseSingleBitFromINI, nullptr,	offsetof(DamageNugget, m_damageType) },
	{ "DeathType",										INI::parseIndexList,			TheDeathNames,				offsetof(DamageNugget, m_deathType) },
	{ "DamageFXType",									INI::parseIndexList,			TheDamageFXTypeNames,	offsetof(DamageNugget, m_damageFXType) },
	{ "DamageSubType",								INI::parseAsciiString,		nullptr,							offsetof(DamageNugget, m_unusedString) },	// TODO BECOME_UNDEAD
	{ "DamageScalar",									DamageNugget::parseDamageScalar, nullptr,				offsetof(DamageNugget, m_damageScalars) },
	{ "DamageSpeed",									INI::parseVelocityReal,		nullptr,							offsetof(DamageNugget, m_damageSpeed) },
	{ "LostLeadershipUselessAgainst",	KindOfMaskType::parseFromINI, nullptr,					offsetof(DamageNugget, m_lostLeadershipUselessAgainst) },
	{ "FlankingBonus",								INI::parsePercentToReal,	nullptr,							offsetof(DamageNugget, m_flankingBonus) },
	{ "FlankedScalar",								INI::parsePercentToReal,	nullptr,							offsetof(DamageNugget, m_flankedScalar) },
	{ "DrainLife",										INI::parseBool,						nullptr,							offsetof(DamageNugget, m_drainLife) },
	{ "DrainLifeMultiplier",					INI::parseReal,						nullptr,							offsetof(DamageNugget, m_drainLifeMultiplier) },
	{ "CylinderAOE",									INI::parseBool,						nullptr,							offsetof(DamageNugget, m_cylinderAOE) },
	{ "ForceKillObjectFilter",				ObjectFilter::parseFromINI, nullptr,						offsetof(DamageNugget, m_forceKillObjectFilter) },
	{ nullptr, nullptr, nullptr, 0 }
};

DamageNugget::DamageNugget() :
	m_damage(0.0f),
	m_damageTaperOff(NUGGET_UNSET),
	m_radius(0.0f),
	m_minRadius(0.0f),
	m_damageArc(0.0f),
	m_damageArcInverted(FALSE),
	m_damageMaxHeight(NUGGET_UNSET),
	m_damageMaxHeightAboveTerrain(NUGGET_UNSET),
	m_acceptDamageAdd(TRUE),
	m_flankingBonus(0.0f),
	m_flankedScalar(1.0f),
	m_delayTime(0),
	m_damageType((DamageType)0x16),	// RotWK default (index 0x16)
	m_deathType(DEATH_NORMAL),
	m_damageFXType(DAMAGEFX_UNDEFINED),
	m_damageSubType(0),
	m_damageSpeed(0.0f),
	m_drainLife(FALSE),
	m_drainLifeMultiplier(0.0f),
	m_cylinderAOE(FALSE)
{
}

void DamageNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	WeaponNugget::buildFieldParse(p);
	p.add(s_fieldParse);
}

void DamageNugget::parseDamageScalar(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	// "DamageScalar = 50% ANY ALLIES +HERO"
	std::vector<BfmeDamageScalar>* v = (std::vector<BfmeDamageScalar>*)store;
	BfmeDamageScalar ds;
	ds.m_scalar = INI::scanPercentToReal(ini->getNextToken());
	ObjectFilter::parseFromINI(ini, nullptr, &ds.m_filter, nullptr);
	v->push_back(ds);
}

Bool DamageNugget::computeDamage(const Object* source, const Object* victim, Real dist, Bool isAreaHit,
	const WeaponBonus& bonus, DamageInfo& out) const
{
	Real amount = m_damage;
	// taper: linear from Damage at MinRadius down to DamageTaperOff at Radius (0x0090E28C)
	if (m_damageTaperOff != NUGGET_UNSET && isAreaHit && m_radius > m_minRadius)
	{
		const Real d = min(max(dist, m_minRadius), m_radius);
		amount = m_damage - (m_damage - m_damageTaperOff) * ((d - m_minRadius) / (m_radius - m_minRadius));
	}
	for (size_t i = 0; i < m_damageScalars.size(); ++i)
	{
		if (m_damageScalars[i].m_filter.matches(victim, source))
		{
			amount *= m_damageScalars[i].m_scalar;
			break;
		}
	}
	amount *= bonus.getField(WeaponBonus::DAMAGE);
	amount = bfmeApplyDamageAttributes(damageCreditFor(const_cast<Object*>(source)), m_damageType, amount, m_acceptDamageAdd);

	out.in.m_damageType = m_damageType;
	out.in.m_damageFXType = m_damageFXType;
	out.in.m_deathType = m_deathType;
	out.in.m_amount = amount;
	if (!m_forceKillObjectFilter.isEmpty() && m_forceKillObjectFilter.matches(victim, source))
		out.in.m_kill = TRUE;
	return TRUE;
}

void DamageNugget::applyToVictim(const WeaponTemplate* /*wt*/, Object* source, Object* victim, Real dist, Bool isAreaHit,
	const WeaponBonus& bonus) const
{
	DamageInfo damageInfo;
	if (!computeDamage(source, victim, dist, isAreaHit, bonus, damageInfo))
		return;
	Object* credit = damageCreditFor(source);
	damageInfo.in.m_sourceID = credit ? credit->getID() : INVALID_ID;
	if (credit && credit->getControllingPlayer())
		damageInfo.in.m_sourcePlayerMask = credit->getControllingPlayer()->getPlayerMask();
	victim->attemptDamage(&damageInfo);

	if (m_drainLife && credit && !credit->isEffectivelyDead() && damageInfo.out.m_actualDamageDealt > 0.0f)
		credit->attemptHealing(damageInfo.out.m_actualDamageDealt * m_drainLifeMultiplier, credit);
}

void DamageNugget::trigger(const WeaponTemplate* wt, Object* source, Object* victim, const Coord3D* pos,
	const WeaponBonus& bonus) const
{
	const Real radius = max(m_radius, 0.0f);
	const Coord3D* impact = pos ? pos : (victim ? victim->getPosition() : nullptr);
	const Real minRadiusSqr = m_minRadius * m_minRadius;

	forEachVictim(impact, victim, radius, m_cylinderAOE, [&](Object* o, Real dist, Bool isArea)
	{
		if (o->isEffectivelyDead())
			return;
		if (!passesSpecialFilter(o, source))
			return;
		if (isArea)
		{
			if (!areaAffects(wt, source, o, victim))
				return;
			if (!insideArc(source, impact, o, m_damageArc, m_damageArcInverted))
				return;
			if (m_damageMaxHeight != NUGGET_UNSET && source && o->getPosition()->z - source->getPosition()->z > m_damageMaxHeight)
				return;
			if (m_damageMaxHeightAboveTerrain != NUGGET_UNSET && o->getHeightAboveTerrain() > m_damageMaxHeightAboveTerrain)
				return;
			if (m_minRadius > 0.0f && dist * dist < minRadiusSqr)
				return;
		}
		applyToVictim(wt, source, o, dist, isArea, bonus);
	});
}

//=================================================================================================
// DOTNugget
//=================================================================================================
const FieldParse DOTNugget::s_dotFieldParse[] =
{
	{ "DamageInterval",	INI::parseDurationUnsignedInt, nullptr, offsetof(DOTNugget, m_damageInterval) },
	{ "DamageDuration",	INI::parseDurationUnsignedInt, nullptr, offsetof(DOTNugget, m_damageDuration) },
	{ nullptr, nullptr, nullptr, 0 }
};

DOTNugget::DOTNugget() : m_damageInterval(1), m_damageDuration(0) {}

void DOTNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	DamageNugget::buildFieldParse(p);
	p.add(s_dotFieldParse);
}

Real DOTNugget::getAggregateDamage() const
{
	const UnsignedInt ticks = m_damageInterval ? max(1u, m_damageDuration / m_damageInterval) : 1u;
	return m_damage * ticks;
}

void DOTNugget::applyToVictim(const WeaponTemplate* wt, Object* source, Object* victim, Real dist, Bool isAreaHit,
	const WeaponBonus& bonus) const
{
	// TODO(bfme-native): RotWK spreads this over DamageDuration in DamageInterval steps (a DOT
	// status on the victim). Until that status exists, the whole amount is dealt on impact.
	const UnsignedInt ticks = m_damageInterval ? max(1u, m_damageDuration / m_damageInterval) : 1u;
	DamageInfo damageInfo;
	if (!computeDamage(source, victim, dist, isAreaHit, bonus, damageInfo))
		return;
	damageInfo.in.m_amount *= ticks;
	Object* credit = damageCreditFor(source);
	damageInfo.in.m_sourceID = credit ? credit->getID() : INVALID_ID;
	if (credit && credit->getControllingPlayer())
		damageInfo.in.m_sourcePlayerMask = credit->getControllingPlayer()->getPlayerMask();
	victim->attemptDamage(&damageInfo);
	(void)wt;
}

//=================================================================================================
// ProjectileNugget
//=================================================================================================
const FieldParse ProjectileNugget::s_fieldParse[] =
{
	{ "WarheadTemplateName",					INI::parseAsciiString,	nullptr, offsetof(ProjectileNugget, m_warheadTemplateName) },
	{ "ProjectileTemplateName",				INI::parseAsciiString,	nullptr, offsetof(ProjectileNugget, m_projectileTemplateName) },
	{ "ProjectileStreamName",					INI::parseAsciiString,	nullptr, offsetof(ProjectileNugget, m_projectileStreamName) },
	{ "WeaponLaunchBoneSlotOverride",	INI::parseAsciiString,	nullptr, offsetof(ProjectileNugget, m_unusedString) },	// TODO
	{ "AlwaysAttackHereOffset",				INI::parseCoord3D,			nullptr, offsetof(ProjectileNugget, m_unusedCoord) },	// TODO
	{ "UseAlwaysAttackOffset",				INI::parseBool,					nullptr, offsetof(ProjectileNugget, m_unusedBool) },	// TODO
	{ nullptr, nullptr, nullptr, 0 }
};

ProjectileNugget::ProjectileNugget() {}

void ProjectileNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	WeaponNugget::buildFieldParse(p);
	p.add(s_fieldParse);
}

void ProjectileNugget::onParsed(WeaponTemplate* owner)
{
	owner->bfmeSetProjectile(m_projectileTemplateName, m_warheadTemplateName);
}

//=================================================================================================
// MetaImpactNugget
//=================================================================================================
const FieldParse MetaImpactNugget::s_fieldParse[] =
{
	{ "ShockWaveAmount",						INI::parseVelocityReal,	nullptr, offsetof(MetaImpactNugget, m_shockWaveAmount) },
	{ "ShockWaveRadius",						INI::parseReal,					nullptr, offsetof(MetaImpactNugget, m_shockWaveRadius) },
	{ "ShockWaveArc",								INI::parseAngleReal,		nullptr, offsetof(MetaImpactNugget, m_shockWaveArc) },
	{ "ShockWaveArcInverted",				INI::parseBool,					nullptr, offsetof(MetaImpactNugget, m_shockWaveArcInverted) },
	{ "ShockWaveTaperOff",					INI::parseReal,					nullptr, offsetof(MetaImpactNugget, m_shockWaveTaperOff) },
	{ "ShockWaveSpeed",							INI::parseVelocityReal,	nullptr, offsetof(MetaImpactNugget, m_shockWaveSpeed) },
	{ "ShockWaveZMult",							INI::parseReal,					nullptr, offsetof(MetaImpactNugget, m_shockWaveZMult) },
	{ "DelayTime",									INI::parseDurationUnsignedInt, nullptr, offsetof(MetaImpactNugget, m_delayTime) },
	{ "InvertShockWave",						INI::parseBool,					nullptr, offsetof(MetaImpactNugget, m_invertShockWave) },
	{ "FlipDirection",							INI::parseBool,					nullptr, offsetof(MetaImpactNugget, m_flipDirection) },
	{ "HeroResist",									INI::parseReal,					nullptr, offsetof(MetaImpactNugget, m_heroResist) },
	{ "OnlyWhenJustDied",						INI::parseBool,					nullptr, offsetof(MetaImpactNugget, m_onlyWhenJustDied) },
	{ "CyclonicFactor",							INI::parseReal,					nullptr, offsetof(MetaImpactNugget, m_cyclonicFactor) },
	{ "ShockWaveClearRadius",				INI::parseBool,					nullptr, offsetof(MetaImpactNugget, m_unusedBool) },	// TODO
	{ "ShockWaveClearMult",					INI::parseReal,					nullptr, offsetof(MetaImpactNugget, m_unusedReal) },	// TODO
	{ "ShockWaveClearFlingHeight",	INI::parseReal,					nullptr, offsetof(MetaImpactNugget, m_unusedReal) },	// TODO
	{ "KillObjectFilter",						ObjectFilter::parseFromINI, nullptr, offsetof(MetaImpactNugget, m_killObjectFilter) },
	{ "AffectHordes",								INI::parseBool,					nullptr, offsetof(MetaImpactNugget, m_affectHordes) },
	{ nullptr, nullptr, nullptr, 0 }
};

MetaImpactNugget::MetaImpactNugget() :
	m_shockWaveAmount(0.0f), m_shockWaveRadius(0.0f), m_shockWaveArc(0.0f), m_shockWaveArcInverted(FALSE),
	m_shockWaveTaperOff(0.0f), m_shockWaveSpeed(0.0f), m_shockWaveZMult(1.0f), m_cyclonicFactor(0.0f),
	m_delayTime(0), m_invertShockWave(FALSE), m_flipDirection(FALSE), m_heroResist(0.0f),
	m_onlyWhenJustDied(FALSE), m_affectHordes(TRUE)
{
}

void MetaImpactNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	WeaponNugget::buildFieldParse(p);
	p.add(s_fieldParse);
}

void MetaImpactNugget::trigger(const WeaponTemplate* wt, Object* source, Object* victim, const Coord3D* pos,
	const WeaponBonus& /*bonus*/) const
{
	if (m_shockWaveAmount <= 0.0f || m_shockWaveRadius <= 0.0f)
		return;
	const Coord3D* impact = pos ? pos : (victim ? victim->getPosition() : nullptr);
	if (impact == nullptr)
		return;
	forEachVictim(impact, victim, m_shockWaveRadius, FALSE, [&](Object* o, Real dist, Bool isArea)
	{
		if (o->isKindOf(KINDOF_PROJECTILE) || o->isAirborneTarget())
			return;
		if (!passesSpecialFilter(o, source))
			return;
		if (isArea && !areaAffects(wt, source, o, victim))
			return;
		if (!insideArc(source, impact, o, m_shockWaveArc, m_shockWaveArcInverted))
			return;
		PhysicsBehavior* phys = o->getPhysics();
		if (phys == nullptr)
			return;

		Coord3D dir;
		dir.set(*o->getPosition());
		dir.sub(*impact);
		if (fabs(dir.x) < WWMATH_EPSILON && fabs(dir.y) < WWMATH_EPSILON)
			dir.z = 1.0f;
		if (m_invertShockWave)
			dir.scale(-1.0f);

		const Real distFrac = min(1.0f, dist / m_shockWaveRadius);
		Real mult = 1.0f - distFrac * (1.0f - m_shockWaveTaperOff);
		if (o->isKindOf(KINDOF_HERO))
			mult *= (1.0f - m_heroResist);
		if (mult <= 0.0f)
			return;

		Coord3D force;
		force.set(dir);
		force.normalize();
		force.scale(m_shockWaveAmount * mult);
		force.z = force.length() * m_shockWaveZMult;
		phys->applyShock(&force);
		phys->applyRandomRotation();
		phys->setStunned(true);
		o->setModelConditionState(MODELCONDITION_STUNNED_FLAILING);

		if (!m_killObjectFilter.isEmpty() && m_killObjectFilter.matches(o, source))
			o->kill();
	});
}

//=================================================================================================
// WeaponOCLNugget
//=================================================================================================
const FieldParse WeaponOCLNugget::s_fieldParse[] =
{
	{ "WeaponOCLName",	WeaponOCLNugget::parseVetOCL, nullptr, offsetof(WeaponOCLNugget, m_oclNames) },
	{ nullptr, nullptr, nullptr, 0 }
};

WeaponOCLNugget::WeaponOCLNugget()
{
	for (Int i = 0; i < 4; ++i)
		m_ocls[i] = nullptr;
}

void WeaponOCLNugget::parseVetOCL(INI* ini, void* /*instance*/, void* store, const void* /*userData*/)
{
	// "WeaponOCLName = OCL_Foo" applies to all veterancy levels
	AsciiString* names = (AsciiString*)store;
	AsciiString n = ini->getNextToken();
	for (Int i = 0; i < 4; ++i)
		names[i] = n;
}

void WeaponOCLNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	WeaponNugget::buildFieldParse(p);
	p.add(s_fieldParse);
}

void WeaponOCLNugget::postProcessLoad()
{
	for (Int i = 0; i < 4; ++i)
		m_ocls[i] = m_oclNames[i].isEmpty() ? nullptr : TheObjectCreationListStore->findObjectCreationList(m_oclNames[i].str());
}

void WeaponOCLNugget::trigger(const WeaponTemplate* /*wt*/, Object* source, Object* victim, const Coord3D* pos,
	const WeaponBonus& /*bonus*/) const
{
	const Coord3D* impact = pos ? pos : (victim ? victim->getPosition() : nullptr);
	if (impact == nullptr || source == nullptr)
		return;
	Int vet = (Int)source->getVeterancyLevel();
	if (vet < 0 || vet > 3)
		vet = 0;
	if (m_ocls[vet])
		ObjectCreationList::create(m_ocls[vet], source, impact, (const Coord3D*)nullptr, (Bool)FALSE);
}

//=================================================================================================
// ParalyzeNugget
//=================================================================================================
const FieldParse ParalyzeNugget::s_fieldParse[] =
{
	{ "Radius",							INI::parseReal,									nullptr, offsetof(ParalyzeNugget, m_radius) },
	{ "Duration",						INI::parseDurationUnsignedInt,	nullptr, offsetof(ParalyzeNugget, m_duration) },
	{ "DamageArc",					INI::parseAngleReal,						nullptr, offsetof(ParalyzeNugget, m_damageArc) },
	{ "ParalyzeFX",					INI::parseFXList,								nullptr, offsetof(ParalyzeNugget, m_paralyzeFX) },
	{ "FreezeAnimation",		INI::parseBool,									nullptr, offsetof(ParalyzeNugget, m_freezeAnimation) },
	{ "AffectHordeMembers",	INI::parseBool,									nullptr, offsetof(ParalyzeNugget, m_affectHordeMembers) },
	{ nullptr, nullptr, nullptr, 0 }
};

ParalyzeNugget::ParalyzeNugget() :
	m_radius(0.0f), m_duration(0), m_damageArc(0.0f), m_paralyzeFX(nullptr), m_freezeAnimation(FALSE), m_affectHordeMembers(FALSE)
{
}

void ParalyzeNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	WeaponNugget::buildFieldParse(p);
	p.add(s_fieldParse);
}

void ParalyzeNugget::trigger(const WeaponTemplate* /*wt*/, Object* source, Object* victim, const Coord3D* pos,
	const WeaponBonus& /*bonus*/) const
{
	const Coord3D* impact = pos ? pos : (victim ? victim->getPosition() : nullptr);
	const UnsignedInt until = TheGameLogic->getFrame() + m_duration;
	forEachVictim(impact, victim, m_radius, FALSE, [&](Object* o, Real /*dist*/, Bool /*isArea*/)
	{
		if (o->isEffectivelyDead() || !passesSpecialFilter(o, source))
			return;
		if (!insideArc(source, impact, o, m_damageArc, FALSE))
			return;
		o->setDisabledUntil(DISABLED_PARALYZED, until);
		FXList::doFXObj(m_paralyzeFX, o);
	});
}

//=================================================================================================
// AttributeModifierNugget
//=================================================================================================
const FieldParse AttributeModifierNugget::s_fieldParse[] =
{
	{ "AttributeModifier",	INI::parseAsciiString,	nullptr,							offsetof(AttributeModifierNugget, m_attributeModifierName) },
	{ "DamageFXType",				INI::parseIndexList,		TheDamageFXTypeNames,	offsetof(AttributeModifierNugget, m_damageFXType) },
	{ "Radius",							INI::parseReal,					nullptr,							offsetof(AttributeModifierNugget, m_radius) },
	{ "DamageArc",					INI::parseAngleReal,		nullptr,							offsetof(AttributeModifierNugget, m_damageArc) },
	{ "AntiCategories",			INI::parseAsciiString,	nullptr,							offsetof(AttributeModifierNugget, m_unusedString) },	// TODO
	{ "AntiFX",							INI::parseFXList,				nullptr,							offsetof(AttributeModifierNugget, m_antiFX) },
	{ "AffectHordeMembers",	INI::parseBool,					nullptr,							offsetof(AttributeModifierNugget, m_affectHordeMembers) },
	{ nullptr, nullptr, nullptr, 0 }
};

AttributeModifierNugget::AttributeModifierNugget() :
	m_list(nullptr), m_damageFXType(DAMAGEFX_UNDEFINED), m_radius(0.0f), m_damageArc(0.0f), m_antiFX(nullptr), m_affectHordeMembers(FALSE)
{
}

void AttributeModifierNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	WeaponNugget::buildFieldParse(p);
	p.add(s_fieldParse);
}

void AttributeModifierNugget::postProcessLoad()
{
	m_list = TheAttributeModifierStore ? TheAttributeModifierStore->findList(m_attributeModifierName) : nullptr;
}

void AttributeModifierNugget::trigger(const WeaponTemplate* /*wt*/, Object* source, Object* victim, const Coord3D* pos,
	const WeaponBonus& /*bonus*/) const
{
	if (m_list == nullptr)
		return;
	const Coord3D* impact = pos ? pos : (victim ? victim->getPosition() : nullptr);
	Object* credit = damageCreditFor(source);
	forEachVictim(impact, victim, m_radius, FALSE, [&](Object* o, Real /*dist*/, Bool /*isArea*/)
	{
		if (o->isEffectivelyDead() || !passesSpecialFilter(o, credit))
			return;
		if (!insideArc(source, impact, o, m_damageArc, FALSE))
			return;
		TheAttributeModifierStore->apply(o, m_list, credit ? credit->getID() : INVALID_ID);
	});
}

//=================================================================================================
const FieldParse HordeAttackNugget::s_fieldParse[] =
{
	{ "LockWeaponSlot",	INI::parseAsciiString,	nullptr, offsetof(HordeAttackNugget, m_unusedString) },
	{ nullptr, nullptr, nullptr, 0 }
};

void HordeAttackNugget::buildFieldParse(MultiIniFieldParse& p) const
{
	WeaponNugget::buildFieldParse(p);
	p.add(s_fieldParse);
}

//=================================================================================================
WeaponNugget* newWeaponNugget(const char* token)
{
	if (stricmp(token, "DamageNugget") == 0) return new DamageNugget;
	if (stricmp(token, "DOTNugget") == 0) return new DOTNugget;
	if (stricmp(token, "ProjectileNugget") == 0) return new ProjectileNugget;
	if (stricmp(token, "MetaImpactNugget") == 0) return new MetaImpactNugget;
	if (stricmp(token, "WeaponOCLNugget") == 0) return new WeaponOCLNugget;
	if (stricmp(token, "ParalyzeNugget") == 0) return new ParalyzeNugget;
	if (stricmp(token, "AttributeModifierNugget") == 0) return new AttributeModifierNugget;
	if (stricmp(token, "HordeAttackNugget") == 0) return new HordeAttackNugget;
	return nullptr;
}
