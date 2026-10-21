// bfme-native: small RotWK modules (one file to keep the boilerplate down).
//   LevelUpUpgrade, RemoveUpgradeUpgrade, ReplaceSelfUpgrade   (upgrade modules)
//   PassiveAreaEffectBehavior, LargeGroupBonusUpdate            (update modules)
//   HordeMemberCollide                                          (collide; behaviour lives in HordeContain)
#pragma once

#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/ObjectFilter.h"

class FXList;

//-------------------------------------------------------------------------------------------------
class LevelUpUpgradeModuleData : public UpgradeModuleData
{
public:
	LevelUpUpgradeModuleData() : m_levelsToGain(1), m_levelCap(0) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Int m_levelsToGain;
	Int m_levelCap;
};

class LevelUpUpgrade : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LevelUpUpgrade, "LevelUpUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( LevelUpUpgrade, LevelUpUpgradeModuleData );
public:
	LevelUpUpgrade( Thing *thing, const ModuleData* moduleData );
protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }
};

//-------------------------------------------------------------------------------------------------
class RemoveUpgradeUpgradeModuleData : public UpgradeModuleData
{
public:
	RemoveUpgradeUpgradeModuleData() : m_removeFromAllPlayerObjects(FALSE), m_suppressEva(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	std::vector<AsciiString> m_upgradesToRemove;
	Bool m_removeFromAllPlayerObjects;
	Bool m_suppressEva;
};

class RemoveUpgradeUpgrade : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RemoveUpgradeUpgrade, "RemoveUpgradeUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RemoveUpgradeUpgrade, RemoveUpgradeUpgradeModuleData );
public:
	RemoveUpgradeUpgrade( Thing *thing, const ModuleData* moduleData );
protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }
};

//-------------------------------------------------------------------------------------------------
/** DoCommandUpgrade: when triggered, carry out a command button on this object (free upgrades,
	special abilities such as ring-hero switches). */
class DoCommandUpgradeModuleData : public UpgradeModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
	AsciiString m_getUpgradeCommandButtonName;
	AsciiString m_removeUpgradeCommandButtonName;
};

class DoCommandUpgrade : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DoCommandUpgrade, "DoCommandUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( DoCommandUpgrade, DoCommandUpgradeModuleData );
public:
	DoCommandUpgrade( Thing *thing, const ModuleData* moduleData );
protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }
};

//-------------------------------------------------------------------------------------------------
class ReplaceSelfUpgradeModuleData : public UpgradeModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
	AsciiString m_replaceWith;
	AsciiString m_andThenAddA;
};

class ReplaceSelfUpgrade : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ReplaceSelfUpgrade, "ReplaceSelfUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ReplaceSelfUpgrade, ReplaceSelfUpgradeModuleData );
public:
	ReplaceSelfUpgrade( Thing *thing, const ModuleData* moduleData );
protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }
};

//-------------------------------------------------------------------------------------------------
class PassiveAreaEffectBehaviorModuleData : public UpdateModuleData
{
public:
	PassiveAreaEffectBehaviorModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);
	Real m_effectRadius;
	UnsignedInt m_pingDelay;
	ObjectFilter m_allowFilter;
	std::vector<AsciiString> m_modifierNames;
	std::vector<AsciiString> m_upgradeRequired;
	Real m_healPercentPerSecond;
	const FXList* m_healFX;
	Bool m_nonStackable;
	AsciiString m_antiCategories;
};

class PassiveAreaEffectBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PassiveAreaEffectBehavior, "PassiveAreaEffectBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( PassiveAreaEffectBehavior, PassiveAreaEffectBehaviorModuleData )
public:
	PassiveAreaEffectBehavior( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;
};

//-------------------------------------------------------------------------------------------------
class LargeGroupBonusUpdateModuleData : public UpdateModuleData
{
public:
	LargeGroupBonusUpdateModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);
	UnsignedInt m_updateRate;
	ObjectFilter m_hordeMemberFilter;
	Int m_count;
	Real m_radius;
	Real m_rubOffRadius;
	Bool m_alliesOnly;
	AsciiString m_attributeModifier;
	std::vector<AsciiString> m_flagSubObjectNames;
};

class LargeGroupBonusUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LargeGroupBonusUpdate, "LargeGroupBonusUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( LargeGroupBonusUpdate, LargeGroupBonusUpdateModuleData )
public:
	LargeGroupBonusUpdate( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;
};

//-------------------------------------------------------------------------------------------------
/** GettingBuiltBehavior: RotWK structures put themselves up -- no dozer drives the construction.
	(First version: the structure builds over its BuildTime; the worker walking over is not
	simulated yet.) */
class GettingBuiltBehaviorModuleData : public UpdateModuleData
{
public:
	GettingBuiltBehaviorModuleData() : m_spawnTimer(0), m_rebuildTimeSeconds(0.0f), m_useSpawnTimerWithoutWorker(FALSE), m_rebuildWhenDead(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	AsciiString m_workerName;
	AsciiString m_evilWorkerName;
	UnsignedInt m_spawnTimer;
	Real m_rebuildTimeSeconds;
	Bool m_useSpawnTimerWithoutWorker;
	Bool m_rebuildWhenDead;
	AsciiString m_unusedString;
	Real m_unusedReal;
};

class GettingBuiltBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( GettingBuiltBehavior, "GettingBuiltBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( GettingBuiltBehavior, GettingBuiltBehaviorModuleData )
public:
	GettingBuiltBehavior( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;
};

//-------------------------------------------------------------------------------------------------
/** TerrainResourceBehavior: RotWK economy. A resource building earns up to MaxIncome every
	IncomeInterval; the land within Radius is shared with other resource buildings covering it, so
	crowded farms earn less. */
class TerrainResourceBehaviorModuleData : public UpdateModuleData
{
public:
	TerrainResourceBehaviorModuleData() : m_maxIncome(0), m_incomeInterval(LOGICFRAMES_PER_SECOND * 10), m_radius(0.0f),
		m_visible(TRUE), m_highPriority(FALSE), m_upgradeBonusPercent(0.0f) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Int m_maxIncome;
	UnsignedInt m_incomeInterval;
	Real m_radius;
	Bool m_visible;
	Bool m_highPriority;
	std::vector<AsciiString> m_upgrades;
	Real m_upgradeBonusPercent;
	ObjectFilter m_upgradeMustBePresent;	///< the bonus needs one of the player's objects to match this
};

class TerrainResourceBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TerrainResourceBehavior, "TerrainResourceBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TerrainResourceBehavior, TerrainResourceBehaviorModuleData )
public:
	TerrainResourceBehavior( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;
	/// share of this building's land (0..1) -- cells it covers, split with the other resource buildings on them
	Real computeLandShare() const;
	Real getRadius() const { return getTerrainResourceBehaviorModuleData()->m_radius; }
};

//-------------------------------------------------------------------------------------------------
/** AutoAbilityBehavior: fires a special ability by itself when enough matching objects are in range
	("Query = 3 ANY ENEMIES +INFANTRY"). Always on for computer players; for humans when StartsActive. */
struct AutoAbilityQuery
{
	Int m_count;
	ObjectFilter m_filter;
};

class AutoAbilityBehaviorModuleData : public UpdateModuleData
{
public:
	AutoAbilityBehaviorModuleData() : m_maxScanRange(0.0f), m_minScanRange(0.0f), m_allowSelf(FALSE), m_startsActive(FALSE),
		m_adjustAttackMeleePosition(FALSE), m_idleTime(0), m_baseMaxRangeFromStartPos(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseQuery(INI* ini, void* instance, void* store, const void* userData);
	AsciiString m_specialAbility;
	Real m_maxScanRange;
	Real m_minScanRange;
	Bool m_allowSelf;
	Bool m_startsActive;
	Bool m_adjustAttackMeleePosition;
	UnsignedInt m_idleTime;
	Bool m_baseMaxRangeFromStartPos;
	ObjectStatusMaskType m_forbiddenStatus;
	std::vector<AutoAbilityQuery> m_queries;
};

class AutoAbilityBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AutoAbilityBehavior, "AutoAbilityBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AutoAbilityBehavior, AutoAbilityBehaviorModuleData )
public:
	AutoAbilityBehavior( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;
	void setActive(Bool a) { m_active = a; }
	Bool isActive() const { return m_active; }
private:
	Bool m_active;
	UnsignedInt m_idleSince;
};

//-------------------------------------------------------------------------------------------------
/** RespawnUpdate: RotWK heroes do not die -- they fall (DeathAnim), vanish and wait to be revived at a
	fortress (Cost/Time per level), or come back by themselves (AutoSpawn) at AutoRespawnAtObjectFilter. */
struct RespawnRule
{
	RespawnRule() : m_level(0), m_autoSpawn(FALSE), m_cost(0), m_time(0), m_health(1.0f) {}
	Int m_level;
	Bool m_autoSpawn;
	Int m_cost;
	UnsignedInt m_time;
	Real m_health;
};

class RespawnUpdateModuleData : public UpdateModuleData
{
public:
	RespawnUpdateModuleData() : m_deathAnimationTime(0), m_respawnAnimationTime(0), m_deathFX(nullptr), m_respawnFX(nullptr), m_initialSpawnFX(nullptr) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseRule(INI* ini, void* instance, void* store, const void* userData);
	static void parseEntry(INI* ini, void* instance, void* store, const void* userData);

	ModelConditionFlags m_deathAnim;
	UnsignedInt m_deathAnimationTime;
	ModelConditionFlags m_respawnAnim;
	UnsignedInt m_respawnAnimationTime;
	const FXList* m_deathFX;
	const FXList* m_respawnFX;
	const FXList* m_initialSpawnFX;
	ObjectFilter m_autoRespawnAt;
	AsciiString m_buttonImage;
	AsciiString m_respawnAsTemplate;
	RespawnRule m_rules;
	std::vector<RespawnRule> m_entries;	///< per-level overrides of Cost/Time
};

class RespawnUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RespawnUpdate, "RespawnUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RespawnUpdate, RespawnUpdateModuleData )
public:
	RespawnUpdate( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;

	/// the hero just took lethal damage: fall instead of dying
	void onFallen();
	Bool isFallen() const { return m_state != STATE_ALIVE; }
	/// revive cost and time at the hero's current level
	RespawnRule currentRule() const;
	/// bring the hero back at 'pos' (its own position if null). Returns the revived object.
	Object* revive(const Coord3D* pos);

	static RespawnUpdate* findOn(const Object* obj);

private:
	enum State { STATE_ALIVE, STATE_DYING, STATE_DEAD };
	State m_state;
	UnsignedInt m_stateFrame;
};

/// called by ActiveBody when health reaches 0; TRUE when a RespawnUpdate took over
Bool bfmeRespawnInterceptDeath(Object* obj, DamageInfo* damageInfo);

//-------------------------------------------------------------------------------------------------
/** ExperienceLevelCreate: the object starts at rank LevelToGrant. */
class ExperienceLevelCreateModuleData : public BehaviorModuleData
{
public:
	ExperienceLevelCreateModuleData() : m_levelToGrant(1), m_mpOnly(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Int m_levelToGrant;
	Bool m_mpOnly;
};

class ExperienceLevelCreate : public BehaviorModule, public CreateModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ExperienceLevelCreate, "ExperienceLevelCreate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ExperienceLevelCreate, ExperienceLevelCreateModuleData )
public:
	ExperienceLevelCreate( Thing *thing, const ModuleData* moduleData );
	static Int getInterfaceMask() { return MODULEINTERFACE_CREATE; }
	virtual CreateModuleInterface* getCreate() override { return this; }
	virtual void onCreate() override;
	virtual void onBuildComplete() override {}
	virtual Bool shouldDoOnBuildComplete() const override { return FALSE; }
};

//-------------------------------------------------------------------------------------------------
/** InheritUpgradeCreate: a new object gets Upgrade if one of its player's objects matching ObjectFilter
	within Radius already has it (fortress upgrades reaching new buildings). */
class InheritUpgradeCreateModuleData : public BehaviorModuleData
{
public:
	InheritUpgradeCreateModuleData() : m_radius(0.0f) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Real m_radius;
	std::vector<AsciiString> m_upgrades;
	ObjectFilter m_filter;
};

class InheritUpgradeCreate : public BehaviorModule, public CreateModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( InheritUpgradeCreate, "InheritUpgradeCreate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( InheritUpgradeCreate, InheritUpgradeCreateModuleData )
public:
	InheritUpgradeCreate( Thing *thing, const ModuleData* moduleData );
	static Int getInterfaceMask() { return MODULEINTERFACE_CREATE; }
	virtual CreateModuleInterface* getCreate() override { return this; }
	virtual void onCreate() override {}
	virtual void onBuildComplete() override;
	virtual Bool shouldDoOnBuildComplete() const override { return TRUE; }
};

//-------------------------------------------------------------------------------------------------
/** RefundDie: when the object dies, its owner gets RefundPercent of its cost back, if the player has
	UpgradeRequired and a building matching BuildingRequired. */
class RefundDieModuleData : public DieModuleData
{
public:
	RefundDieModuleData() : m_refundPercent(0.0f) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Real m_refundPercent;
	AsciiString m_upgradeRequired;
	ObjectFilter m_buildingRequired;
};

class RefundDie : public DieModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RefundDie, "RefundDie" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RefundDie, RefundDieModuleData )
public:
	RefundDie( Thing *thing, const ModuleData* moduleData );
	virtual void onDie( const DamageInfo *damageInfo ) override;
};

//-------------------------------------------------------------------------------------------------
/** AttachUpdate: the object attaches itself to the nearest object matching ObjectFilter within
	ScanRange and rides along on it (on top when AnchorToTopOfGeometry); it goes when its parent does. */
class AttachUpdateModuleData : public UpdateModuleData
{
public:
	AttachUpdateModuleData() : m_scanRange(50.0f), m_alwaysTeleport(FALSE), m_anchorToTop(FALSE) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	ObjectFilter m_filter;
	Real m_scanRange;
	Bool m_alwaysTeleport;
	Bool m_anchorToTop;
	AsciiString m_unusedString;
};

class AttachUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttachUpdate, "AttachUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttachUpdate, AttachUpdateModuleData )
public:
	AttachUpdate( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;
	ObjectID getParentID() const { return m_parent; }
private:
	ObjectID m_parent;
	Bool m_searched;
};

//-------------------------------------------------------------------------------------------------
/** DualWeaponBehavior: archers switch to their CLOSE_RANGE weapon set (which holds the melee weapon)
	while the target is within SwitchWeaponOnCloseRangeDistance. */
class DualWeaponBehaviorModuleData : public UpdateModuleData
{
public:
	DualWeaponBehaviorModuleData() : m_closeRange(0.0f) {}
	static void buildFieldParse(MultiIniFieldParse& p);
	Real m_closeRange;
};

class DualWeaponBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DualWeaponBehavior, "DualWeaponBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( DualWeaponBehavior, DualWeaponBehaviorModuleData )
public:
	DualWeaponBehavior( Thing *thing, const ModuleData* moduleData );
	virtual UpdateSleepTime update() override;
private:
	Bool m_close;
};

//-------------------------------------------------------------------------------------------------
/** A RotWK module that is parsed but has no behaviour yet (audio/animation polish, or logic still to
	be ported -- see docs/SCOPE.md "inert modules"). Registered under several names. */
class BfmeInertBehaviorModuleData : public BehaviorModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p) { BehaviorModuleData::buildFieldParse(p); }
};

class BfmeInertBehavior : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( BfmeInertBehavior, "BfmeInertBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( BfmeInertBehavior, BfmeInertBehaviorModuleData )
public:
	BfmeInertBehavior( Thing *thing, const ModuleData* moduleData ) : BehaviorModule( thing, moduleData ) {}
};

//-------------------------------------------------------------------------------------------------
class HordeMemberCollide : public CollideModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HordeMemberCollide, "HordeMemberCollide" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HordeMemberCollide, CollideModuleData )
public:
	HordeMemberCollide( Thing *thing, const ModuleData* moduleData );
	virtual void onCollide( Object * /*other*/, const Coord3D * /*loc*/, const Coord3D * /*normal*/ ) override {}
};
