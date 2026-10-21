// bfme-native: RotWK weapon nuggets.
//
// A RotWK Weapon carries no damage numbers of its own; what it does on impact is a list of
// "nuggets" (DamageNugget, ProjectileNugget, MetaImpactNugget, ...). Layouts and behaviour are
// reimplemented from game.dat: field tables at 0xC7AE00 (base) / 0xC7AFB0 (DamageNugget) /
// 0xC7B4C8 (ProjectileNugget) / 0xC7B6A8 (MetaImpactNugget) / ..., DamageNugget vtable 0xC7AE78
// (trigger 0x0090DEF0, damage calc 0x0090E28C, apply 0x0090E683).
#pragma once

#include "Common/AsciiString.h"
#include "Common/GameType.h"
#include "GameLogic/Damage.h"
#include "GameLogic/ObjectFilter.h"
#include <vector>

class INI;
class Object;
class WeaponTemplate;
class WeaponBonus;
class FXList;
class ObjectCreationList;
class UpgradeTemplate;
struct FieldParse;
class MultiIniFieldParse;

//-------------------------------------------------------------------------------------------------
class WeaponNugget
{
public:
	WeaponNugget();
	virtual ~WeaponNugget();

	/// run the nugget. 'victim' may be null (ground attack), 'pos' is the impact point.
	virtual void trigger(const WeaponTemplate* wt, Object* source, Object* victim, const Coord3D* pos,
		const WeaponBonus& bonus) const = 0;

	virtual void buildFieldParse(MultiIniFieldParse& p) const;
	virtual void onParsed(WeaponTemplate* /*owner*/) {}
	virtual void postProcessLoad() {}

	virtual Bool isProjectileNugget() const { return FALSE; }
	virtual Real getAggregateDamage() const { return 0.0f; }
	virtual Real getAggregateRadius() const { return 0.0f; }
	virtual Bool getAggregateDamageType(DamageType& /*out*/) const { return FALSE; }

	/// RequiredUpgradeNames / ForbiddenUpgradeNames against the firer
	Bool isAllowed(const Object* source) const;
	/// SpecialObjectFilter against a candidate victim
	Bool passesSpecialFilter(const Object* victim, const Object* source) const;

protected:
	std::vector<AsciiString> m_requiredUpgradeNames;
	std::vector<AsciiString> m_forbiddenUpgradeNames;
	ObjectFilter m_specialObjectFilter;

	// sinks for fields that are parsed but not used yet
	AsciiString m_unusedString;
	Real m_unusedReal;
	Bool m_unusedBool;
	Coord3D m_unusedCoord;

	static const FieldParse s_baseFieldParse[];
};

//-------------------------------------------------------------------------------------------------
struct BfmeDamageScalar
{
	Real m_scalar;
	ObjectFilter m_filter;
};

class DamageNugget : public WeaponNugget
{
public:
	DamageNugget();

	virtual void trigger(const WeaponTemplate* wt, Object* source, Object* victim, const Coord3D* pos,
		const WeaponBonus& bonus) const;
	virtual void buildFieldParse(MultiIniFieldParse& p) const;

	virtual Real getAggregateDamage() const { return m_damage; }
	virtual Real getAggregateRadius() const { return m_radius; }
	virtual Bool getAggregateDamageType(DamageType& out) const { out = m_damageType; return TRUE; }

protected:
	/// damage dealt to one victim at distance 'dist' from the impact point (0 if it should be skipped)
	Bool computeDamage(const Object* source, const Object* victim, Real dist, Bool isAreaHit,
		const WeaponBonus& bonus, DamageInfo& out) const;
	virtual void applyToVictim(const WeaponTemplate* wt, Object* source, Object* victim, Real dist, Bool isAreaHit,
		const WeaponBonus& bonus) const;

	static void parseDamageScalar(INI* ini, void* instance, void* store, const void* userData);

	Real m_damage;
	Real m_damageTaperOff;			///< damage at the outer radius (sentinel = no taper)
	Real m_radius;
	Real m_minRadius;
	Real m_damageArc;
	Bool m_damageArcInverted;
	Real m_damageMaxHeight;
	Real m_damageMaxHeightAboveTerrain;
	Bool m_acceptDamageAdd;
	Real m_flankingBonus;
	Real m_flankedScalar;
	UnsignedInt m_delayTime;
	DamageType m_damageType;
	DeathType m_deathType;
	DamageFXType m_damageFXType;
	Int m_damageSubType;
	std::vector<BfmeDamageScalar> m_damageScalars;
	Real m_damageSpeed;
	KindOfMaskType m_lostLeadershipUselessAgainst;
	Bool m_drainLife;
	Real m_drainLifeMultiplier;
	Bool m_cylinderAOE;
	ObjectFilter m_forceKillObjectFilter;

	static const FieldParse s_fieldParse[];
};

//-------------------------------------------------------------------------------------------------
/** DOTNugget: a DamageNugget repeated every DamageInterval for DamageDuration. */
class DOTNugget : public DamageNugget
{
public:
	DOTNugget();
	virtual void buildFieldParse(MultiIniFieldParse& p) const;
	virtual Real getAggregateDamage() const;
protected:
	virtual void applyToVictim(const WeaponTemplate* wt, Object* source, Object* victim, Real dist, Bool isAreaHit,
		const WeaponBonus& bonus) const;
	UnsignedInt m_damageInterval;
	UnsignedInt m_damageDuration;
	static const FieldParse s_dotFieldParse[];
};

//-------------------------------------------------------------------------------------------------
class ProjectileNugget : public WeaponNugget
{
public:
	ProjectileNugget();
	virtual void trigger(const WeaponTemplate*, Object*, Object*, const Coord3D*, const WeaponBonus&) const {}
	virtual void buildFieldParse(MultiIniFieldParse& p) const;
	virtual void onParsed(WeaponTemplate* owner);
	virtual Bool isProjectileNugget() const { return TRUE; }
protected:
	AsciiString m_warheadTemplateName;
	AsciiString m_projectileTemplateName;
	AsciiString m_projectileStreamName;
	static const FieldParse s_fieldParse[];
};

//-------------------------------------------------------------------------------------------------
class MetaImpactNugget : public WeaponNugget
{
public:
	MetaImpactNugget();
	virtual void trigger(const WeaponTemplate* wt, Object* source, Object* victim, const Coord3D* pos,
		const WeaponBonus& bonus) const;
	virtual void buildFieldParse(MultiIniFieldParse& p) const;
protected:
	Real m_shockWaveAmount;
	Real m_shockWaveRadius;
	Real m_shockWaveArc;
	Bool m_shockWaveArcInverted;
	Real m_shockWaveTaperOff;
	Real m_shockWaveSpeed;
	Real m_shockWaveZMult;
	Real m_cyclonicFactor;
	UnsignedInt m_delayTime;
	Bool m_invertShockWave;
	Bool m_flipDirection;
	Real m_heroResist;
	Bool m_onlyWhenJustDied;
	Bool m_affectHordes;
	ObjectFilter m_killObjectFilter;
	static const FieldParse s_fieldParse[];
};

//-------------------------------------------------------------------------------------------------
class WeaponOCLNugget : public WeaponNugget
{
public:
	WeaponOCLNugget();
	virtual void trigger(const WeaponTemplate* wt, Object* source, Object* victim, const Coord3D* pos,
		const WeaponBonus& bonus) const;
	virtual void buildFieldParse(MultiIniFieldParse& p) const;
	virtual void postProcessLoad();
protected:
	AsciiString m_oclNames[4];	// regular / veteran / elite / heroic
	const ObjectCreationList* m_ocls[4];
	static const FieldParse s_fieldParse[];
	static void parseVetOCL(INI* ini, void* instance, void* store, const void* userData);
};

//-------------------------------------------------------------------------------------------------
class ParalyzeNugget : public WeaponNugget
{
public:
	ParalyzeNugget();
	virtual void trigger(const WeaponTemplate* wt, Object* source, Object* victim, const Coord3D* pos,
		const WeaponBonus& bonus) const;
	virtual void buildFieldParse(MultiIniFieldParse& p) const;
protected:
	Real m_radius;
	UnsignedInt m_duration;
	Real m_damageArc;
	const FXList* m_paralyzeFX;
	Bool m_freezeAnimation;
	Bool m_affectHordeMembers;
	static const FieldParse s_fieldParse[];
};

//-------------------------------------------------------------------------------------------------
/// Factory: returns a new nugget for an INI token such as "DamageNugget", or null if unsupported.
WeaponNugget* newWeaponNugget(const char* token);
