// bfme-native: RotWK ToggleMountedSpecialAbilityUpdate -- mount / dismount a hero.
//
// Without MountedTemplate the same object toggles its MOUNTED state (model condition, weapon set,
// armor set and the MOUNTED locomotor set). With MountedTemplate the object is replaced by that
// template, keeping health, experience, upgrades and the recharge of the listed special powers
// (also used for ring-hero and transformation toggles).
#pragma once

#include "GameLogic/Module/SpecialAbilityUpdate.h"

class ToggleMountedSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	ToggleMountedSpecialAbilityUpdateModuleData() : m_opacityTarget(1.0f), m_ignoreFacingCheck(FALSE), m_triggerInstantlyOnCreate(FALSE),
		m_cancelDisguiseWhenDismounting(FALSE) { m_bfmeNoPersistence = TRUE; }
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_mountedTemplate;
	Real m_opacityTarget;
	Bool m_ignoreFacingCheck;
	std::vector<AsciiString> m_synchronizeTimers;
	Bool m_triggerInstantlyOnCreate;
	Bool m_cancelDisguiseWhenDismounting;
};

class ToggleMountedSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ToggleMountedSpecialAbilityUpdate, "ToggleMountedSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ToggleMountedSpecialAbilityUpdate, ToggleMountedSpecialAbilityUpdateModuleData )

public:
	ToggleMountedSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData );
	virtual void onObjectCreated() override;

	static void setMounted(Object* obj, Bool mounted);
	static Bool isMounted(const Object* obj);

protected:
	virtual Bool bfmeTriggerOverride() override;
};

/// swap 'obj' for a new object of 'tmpl' in place (health fraction, experience, upgrades carried over;
/// the recharge of 'syncPowers' too). Returns the new object (the old one is destroyed), or null.
Object* bfmeReplaceObject(Object* obj, const ThingTemplate* tmpl, const std::vector<AsciiString>* syncPowers = nullptr);
