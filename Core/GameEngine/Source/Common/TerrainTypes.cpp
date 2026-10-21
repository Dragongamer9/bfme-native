/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: TerrainTypes.cpp /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Terrain type descriptions and collection
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_TERRAIN_TYPE_NAMES

#include "Common/INI.h"
#include "Common/TerrainTypes.h"

// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
TerrainTypeCollection *TheTerrainTypes = nullptr;

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
const FieldParse TerrainType::m_terrainTypeFieldParseTable[] =
{

	{ "Texture",		INI::parseAsciiString,			nullptr,		offsetof( TerrainType, m_texture ) },
	{ "BlendEdges", INI::parseBool,							nullptr,		offsetof( TerrainType, m_blendEdgeTexture ) },
	{ "Class",			TerrainType::parseBfmeClass,	nullptr, 0 },
	{ "RestrictConstruction", INI::parseBool,		nullptr,		offsetof( TerrainType, m_restrictConstruction ) },
	{ "TerrainObject",	TerrainType::parseTerrainObject, nullptr, 0 },

	{ nullptr,					nullptr,												nullptr,		0 },

};

//-------------------------------------------------------------------------------------------------
// bfme-native: RotWK terrain class vocabulary (game.dat 0x00da3448), in table order.
static const char* const bfmeTerrainClassNames[] =
{
	"NONE", "NEXT", "Region", "Minas_Tirith", "Osgiliath", "Dead_Marshes", "Edoras", "Helm's_Deep",
	"Paths_of_Dead", "Fangorn_Forest", "Misty_Mountains", "Rivendell", "Moria", "The_Shire", "Isengard",
	"Mordor", "Misc_Village", "Misc_Grassland", "Misc_Town", "Misc_Woodland", "Misc_Water", "Type",
	"Grass", "Rock", "Dirt", "Snow", "Cliff", "Misc", "DESERT_1", "DESERT_2", "DESERT_3",
	"EASTERN_EUROPE_1", "EASTERN_EUROPE_2", "EASTERN_EUROPE_3", "SWISS_1", "SWISS_2", "SWISS_3",
	"SNOW_1", "SNOW_2", "SNOW_3", "ASPHALT", "CONCRETE", "TRANSITION", "SAND", "WOOD", "BLEND_EDGE",
	"LOTR_NEW", nullptr
};

/** RotWK 0x0073b7cb: every token is looked up in the class vocabulary; "None" clears the list.
	Generals-style single class names still map onto m_class for the existing editor code. */
/*static*/ void TerrainType::parseBfmeClass(INI* ini, void* instance, void* /*store*/, const void* /*userData*/)
{
	TerrainType* self = (TerrainType*)instance;
	self->m_bfmeClassList.clear();
	self->m_class = TERRAIN_NONE;
	for (const char* tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull())
	{
		if (stricmp(tok, "None") == 0)
		{
			self->m_bfmeClassList.clear();
			return;
		}
		Int idx = -1;
		for (Int i = 0; bfmeTerrainClassNames[i]; ++i)
			if (stricmp(bfmeTerrainClassNames[i], tok) == 0) { idx = i; break; }
		if (idx < 0)
		{
			// a Generals class name (data shared with ZH tools) - keep the old behaviour
			for (Int i = 0; terrainTypeNames[i]; ++i)
				if (stricmp(terrainTypeNames[i], tok) == 0) { self->m_class = (TerrainClass)i; break; }
			continue;
		}
		self->m_bfmeClassList.push_back(idx);
	}
}

/** RotWK 0x00719b59: "TerrainObject = <name> <real>" */
/*static*/ void TerrainType::parseTerrainObject(INI* ini, void* instance, void* /*store*/, const void* /*userData*/)
{
	TerrainType* self = (TerrainType*)instance;
	TerrainObject o;
	o.name = ini->getNextToken();
	o.value = INI::scanReal(ini->getNextToken());
	self->m_terrainObjects.push_back(o);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TerrainType::TerrainType()
{

	m_name.clear();
	m_texture.clear();
	m_blendEdgeTexture = FALSE;
	m_class = TERRAIN_NONE;
	m_restrictConstruction = FALSE;
	m_next = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TerrainType::~TerrainType()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TerrainTypeCollection::TerrainTypeCollection()
{

	m_terrainList = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TerrainTypeCollection::~TerrainTypeCollection()
{
	TerrainType *temp;

	// delete all the type instances
	while( m_terrainList )
	{

		// get the next element
		temp = m_terrainList->friend_getNext();

		// delete the head of the type list
		deleteInstance(m_terrainList);

		// set the new head of the type list
		m_terrainList = temp;

	}

}

//-------------------------------------------------------------------------------------------------
/** Find a terrain type given the name */
//-------------------------------------------------------------------------------------------------
TerrainType *TerrainTypeCollection::findTerrain( AsciiString name )
{
	TerrainType *terrain;

	for( terrain = m_terrainList; terrain; terrain = terrain->friend_getNext() )
	{

		if( terrain->getName() == name )
			return terrain;

	}

	// not found
	return nullptr;

}

//-------------------------------------------------------------------------------------------------
/** Allocate a new type, assign the name, and tie to type list */
//-------------------------------------------------------------------------------------------------
TerrainType *TerrainTypeCollection::newTerrain( AsciiString name )
{
	TerrainType *terrain = nullptr;

	// allocate a new type
	terrain = newInstance(TerrainType);

	// copy default values from the default terrain entry
	TerrainType *defaultTerrain = findTerrain( "DefaultTerrain" );
	if( defaultTerrain )
		*terrain = *defaultTerrain;
/*
	{

		terrain->friend_setTexture( defaultTerrain->getTexture() );
		terrain->friend_setClass( defaultTerrain->getClass() );
		terrain->friend_setBlendEdge( defaultTerrain->isBlendEdge() );

	}
*/

	// assign a name
	terrain->friend_setName( name );

	// tie to list
	terrain->friend_setNext( m_terrainList );
	m_terrainList = terrain;

	// return the new terrain
	return terrain;

}
