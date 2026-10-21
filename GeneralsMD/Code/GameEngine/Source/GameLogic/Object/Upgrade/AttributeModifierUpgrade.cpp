// bfme-native: RotWK AttributeModifierUpgrade
#include "PreRTS.h"

#include "Common/Xfer.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Module/AttributeModifierUpgrade.h"
#include "GameLogic/Object.h"

void AttributeModifierUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse dataFieldParse[] =
	{
		{ "AttributeModifier",			INI::parseAsciiString,	nullptr, offsetof( AttributeModifierUpgradeModuleData, m_attributeModifier ) },
		{ "CustomAnimAndDuration",	INI::parseAsciiString,	nullptr, offsetof( AttributeModifierUpgradeModuleData, m_customAnimAndDuration ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

AttributeModifierUpgrade::AttributeModifierUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

AttributeModifierUpgrade::~AttributeModifierUpgrade()
{
}

void AttributeModifierUpgrade::upgradeImplementation()
{
	const AttributeModifierList* list = TheAttributeModifierStore->findList(getAttributeModifierUpgradeModuleData()->m_attributeModifier);
	TheAttributeModifierStore->apply(getObject(), list, getObject()->getID(), 0, TRUE);
}

Bool AttributeModifierUpgrade::resetUpgrade( const UpgradeMaskType& keyMask )
{
	if (!UpgradeModule::resetUpgrade(keyMask))
		return false;
	const AttributeModifierList* list = TheAttributeModifierStore->findList(getAttributeModifierUpgradeModuleData()->m_attributeModifier);
	TheAttributeModifierStore->remove(getObject(), list);
	return true;
}

void AttributeModifierUpgrade::crc( Xfer *xfer )
{
	UpgradeModule::crc( xfer );
}

void AttributeModifierUpgrade::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpgradeModule::xfer( xfer );
}

void AttributeModifierUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
