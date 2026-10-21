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

// FILE: INIAudioEventInfo.cpp ////////////////////////////////////////////////////////////////////////
// Author: Colin Day, July 2002
// Desc:   Parsing AudioEvent, MusicTrack and DialogEvent INI entries
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/INI.h"
#include "Common/GameAudio.h"
#include "Common/AudioEventInfo.h"

AudioEventInfo::~AudioEventInfo()
{

}
// @todo: Get these functions unified.

//-------------------------------------------------------------------------------------------------
void INI::parseMusicTrackDefinition( INI* ini )
{
	AsciiString name;
	AudioEventInfo *track;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	track = TheAudio->newAudioEventInfo( name );
	if (!track) {
		return;
	}

	AudioEventInfo *defaultInfo = TheAudio->findAudioEventInfo("DefaultMusicTrack");
	if (defaultInfo) {
		(*track) = (*defaultInfo);
		TheAudio->addTrackName( name );
	}

	track->m_audioName = name;
	track->m_soundType = AT_Music;

	// parse the ini definition
	ini->initFromINI( track, track->getFieldParse() );
}

//-------------------------------------------------------------------------------------------------
void INI::parseAudioEventDefinition( INI* ini )
{
	AsciiString name;
	AudioEventInfo *track;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	track = TheAudio->newAudioEventInfo( name );
	if (!track) {
		return;
	}

	AudioEventInfo *defaultInfo = TheAudio->findAudioEventInfo("DefaultSoundEffect");
	if (defaultInfo) {
		(*track) = (*defaultInfo);
	}

	track->m_audioName = name;
	track->m_soundType = AT_SoundEffect;

	// parse the ini definition
	ini->initFromINI( track, track->getFieldParse() );
}

//-------------------------------------------------------------------------------------------------
void INI::parseDialogDefinition( INI* ini )
{
	AsciiString name;
	AudioEventInfo *track;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	track = TheAudio->newAudioEventInfo( name );
	if (!track) {
		return;
	}

	AudioEventInfo *defaultInfo = TheAudio->findAudioEventInfo("DefaultDialog");
	if (defaultInfo) {
		(*track) = (*defaultInfo);
	}

	track->m_audioName = name;
	track->m_soundType = AT_Streaming;

	// parse the ini definition
	ini->initFromINI( track, track->getFieldParse() );
}


//-------------------------------------------------------------------------------------------------
static void parseDelay( INI* ini, void *instance, void *store, const void* /*userData*/ );
static void parsePitchShift( INI* ini, void *instance, void *store, const void* /*userData*/ );

//-------------------------------------------------------------------------------------------------
// bfme-native: RotWK audio vocabularies (game.dat AudioEventInfo field table)
static const char *const theBfmeAudioControlNames[] =
{
	"LOOP", "SEQUENTIAL", "RANDOMSTART", "INTERRUPT", "FADE_ON_KILL", "FADE_ON_START",
	"\\not used by INI files", "PLAY_ONE", nullptr
};
static const char *const theBfmeSubmixSliderNames[] =
{
	"SOUNDFX", "VOICE", "MUSIC", "AMBIENT", "MOVIE", "NONE", nullptr
};

/** bfme-native: RotWK Control flags. Stored verbatim in m_bfmeControl and translated onto the
	Generals flags the audio code uses: RotWK sounds pick randomly unless SEQUENTIAL is given. */
static void parseBfmeAudioControl( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	AudioEventInfo *info = (AudioEventInfo*) instance;
	UnsignedInt bits = 0;
	for (const char* tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull())
	{
		if (stricmp(tok, "NONE") == 0) { bits = 0; break; }
		Bool found = FALSE;
		for (Int i = 0; theBfmeAudioControlNames[i]; ++i)
			if (stricmp(theBfmeAudioControlNames[i], tok) == 0) { bits |= (1u << i); found = TRUE; break; }
		if (!found)
		{
			// Generals spelling (shared ZH data)
			for (Int i = 0; theAudioControlNames[i]; ++i)
				if (stricmp(theAudioControlNames[i], tok) == 0) { info->m_control |= (1u << i); break; }
		}
	}
	info->m_bfmeControl = bits;
	if (bits)
	{
		UnsignedInt c = 0;
		if (bits & (1u << 0)) c |= AC_LOOP;
		if (!(bits & (1u << 1))) c |= AC_RANDOM;
		if (bits & (1u << 3)) c |= AC_INTERRUPT;
		info->m_control = c;
	}
}

const FieldParse AudioEventInfo::m_audioEventInfo[] =
{
	{ "Filename",							INI::parseAsciiString,		nullptr,								offsetof( AudioEventInfo, m_filename) },
	{ "Volume",								INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_volume ) },
	{ "VolumeShift",					INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_volumeShift ) },
	{ "MinVolume",						INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_minVolume ) },
	{ "PitchShift",						parsePitchShift,					nullptr,								0 },
	{ "Delay",								parseDelay,								nullptr,								0 },
	{ "Limit",								INI::parseInt,						nullptr,								offsetof( AudioEventInfo, m_limit ) },
	{ "LoopCount",						INI::parseInt,						nullptr,								offsetof( AudioEventInfo, m_loopCount ) },
	{ "Priority",							INI::parseIndexList,			theAudioPriorityNames,	offsetof( AudioEventInfo, m_priority ) },
	{ "Type",									INI::parseBitString32,		theSoundTypeNames,			offsetof( AudioEventInfo, m_type ) },
	{ "Control",							parseBfmeAudioControl,		nullptr,								0 },
	{ "SubmixSlider",					INI::parseIndexList,			theBfmeSubmixSliderNames, offsetof( AudioEventInfo, m_submixSlider ) },
	{ "Sounds",								INI::parseSoundsList,			nullptr,								offsetof( AudioEventInfo, m_sounds ) },
	{ "SoundsNight",					INI::parseSoundsList,			nullptr,								offsetof( AudioEventInfo, m_soundsNight ) },
	{ "SoundsEvening",				INI::parseSoundsList,			nullptr,								offsetof( AudioEventInfo, m_soundsEvening ) },
	{ "SoundsMorning",				INI::parseSoundsList,			nullptr,								offsetof( AudioEventInfo, m_soundsMorning ) },
	{ "Attack",								INI::parseSoundsList,			nullptr,								offsetof( AudioEventInfo, m_attackSounds ) },
	{ "Decay",								INI::parseSoundsList,			nullptr,								offsetof( AudioEventInfo, m_decaySounds ) },
	{ "MinRange",							INI::parseReal,						nullptr,								offsetof( AudioEventInfo, m_minDistance) },
	{ "MaxRange",							INI::parseReal,						nullptr,								offsetof( AudioEventInfo, m_maxDistance) },
	{ "LowPassCutoff",				INI::parsePercentToReal,	nullptr,								offsetof( AudioEventInfo, m_lowPassFreq) },
	{ nullptr, nullptr, nullptr, 0 }
};

//-------------------------------------------------------------------------------------------------
static void parseDelay( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	AudioEventInfo *attribs = (AudioEventInfo*) store;

	Int min = ini->scanInt(ini->getNextToken());
	Int max = ini->scanInt(ini->getNextToken());

	DEBUG_ASSERTCRASH( min >= 0 && max >= min, ("Bad delay values for audio event %s", attribs->m_audioName.str()));
	attribs->m_delayMax = max;
	attribs->m_delayMin = min;
}

//-------------------------------------------------------------------------------------------------
static void parsePitchShift( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	AudioEventInfo *attribs = (AudioEventInfo*) store;

	Real min = ini->scanReal(ini->getNextToken());
	Real max = ini->scanReal(ini->getNextToken());

	DEBUG_ASSERTCRASH( min > -100 && max >= min, ("Bad pitch shift values for audio event %s", attribs->m_audioName.str()));
	attribs->m_pitchShiftMin = 1.0f + min/100;
	attribs->m_pitchShiftMax = 1.0f + max/100;
}

// STATIC DEFINIITIONS ////////////////////////////////////////////////////////////////////////////

const char *const theAudioPriorityNames[] =
{
	"LOWEST",
	"LOW",
	"NORMAL",
	"HIGH",
	"CRITICAL",
	nullptr
};
static_assert(ARRAY_SIZE(theAudioPriorityNames) == AP_COUNT + 1, "Incorrect array size");

const char *const theSoundTypeNames[] =
{
	"UI",
	"WORLD",
	"SHROUDED",
	"GLOBAL",
	"VOICE",
	"PLAYER",
	"ALLIES",
	"ENEMIES",
	"EVERYONE",
	"FAKE",		// bfme-native: RotWK
	"DEFAULT",	// bfme-native: RotWK
	nullptr
};

const char *const theAudioControlNames[] =
{
	"LOOP",
	"RANDOM",
	"ALL",
	"POSTDELAY",
	"INTERRUPT",
	nullptr
};

