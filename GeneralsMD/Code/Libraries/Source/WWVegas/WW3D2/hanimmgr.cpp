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

/* $Header: /Commando/Code/ww3d2/hanimmgr.cpp 3     1/16/02 9:51a Jani_p $ */
/***********************************************************************************************
 ***                            Confidential - Westwood Studios                              ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Commando / G 3D Library                                      *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/hanimmgr.cpp                           $*
 *                                                                                             *
 *                       Author:: Greg_h                                                       *
 *                                                                                             *
 *                     $Modtime:: 1/16/02 9:49a                                               $*
 *                                                                                             *
 *                    $Revision:: 3                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   HAnimManagerClass::HAnimManagerClass -- constructor                                       *
 *   HAnimManagerClass::~HAnimManagerClass -- destructor                                       *
 *   HAnimManagerClass::Load_Anim -- loads a set of motion data from a file                    *
 *   HAnimManagerClass::Get_Anim_ID -- looks up the ID of a named Hierarchy Animation          *
 *   HAnimManagerClass::Get_Anim -- returns a pointer to the specified animation data          *
 *   HAnimManagerClass::Get_Anim -- returns a pointer to the specified Hierarchy Animation     *
 *   HAnimManagerClass::Free -- de-allocate all memory in use                                  *
 *   HAnimManagerClass::Free_All_Anims -- de-allocate all currently loaded animations          *
 *   HAnimManagerClass::Load_Raw_Anim -- Load a raw anim                                       *
 *   HAnimManagerClass::Load_Compressed_Anim -- load a compressed animation                    *
 *	  HAnimManagerClass::Add_Anim -- Adds an externally created animation to the manager		  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "hanimmgr.h"
#include <vector>
#include <cstring>
#include <cmath>
#include "WWLib/RAMFILE.h"
#include "WWMath/wwmath.h"
#include "hanim.h"
#include "hrawanim.h"
#include "hcanim.h"
#include "hmorphanim.h"
#include "WWLib/chunkio.h"
#include "WWDebug/wwmemlog.h"
#include "w3dexclusionlist.h"
#include "animatedsoundmgr.h"


/***********************************************************************************************
 * HAnimManagerClass::HAnimManagerClass -- constructor                                         *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/11/1997 GH  : Created.                                                                 *
 *=============================================================================================*/
HAnimManagerClass::HAnimManagerClass()
{
	// Create the hash tables
	AnimPtrTable = W3DNEW HashTableClass( 2048 );
	MissingAnimTable = W3DNEW HashTableClass( 2048 );
}


/***********************************************************************************************
 * HAnimManagerClass::~HAnimManagerClass -- destructor                                         *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/11/1997 GH  : Created.                                                                 *
 *=============================================================================================*/
HAnimManagerClass::~HAnimManagerClass()
{
	Free_All_Anims();
	Reset_Missing();	// Jani: Deleting missing animations as well

	delete AnimPtrTable;
	AnimPtrTable = nullptr;

	Reset_Missing();
	delete MissingAnimTable;
	MissingAnimTable = nullptr;
}


/***********************************************************************************************
 * HAnimManagerClass::Load_Anim -- loads a set of motion data from a file                      *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/11/1997 GH  : Created.                                                                 *
 *=============================================================================================*/
int HAnimManagerClass::Load_Anim(ChunkLoadClass & cload)
{
	WWMEMLOG(MEM_ANIMATION);

	switch (cload.Cur_Chunk_ID())
	{
	case W3D_CHUNK_ANIMATION:
		return Load_Raw_Anim(cload);
		break;

	case W3D_CHUNK_COMPRESSED_ANIMATION:
		return Load_Compressed_Anim(cload);
		break;

	case W3D_CHUNK_MORPH_ANIMATION:
		return Load_Morph_Anim(cload);
		break;
	}

	return 0;
}


/***********************************************************************************************
 * HAnimManagerClass::Load_Morph_Anim -- Load a HMorphAnimClass										  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   5/23/2000  pds : Created.                                                                 *
 *=============================================================================================*/
int HAnimManagerClass::Load_Morph_Anim(ChunkLoadClass & cload)
{
	HMorphAnimClass * newanim = W3DNEW HMorphAnimClass;

	if (newanim == nullptr) {
		goto Error;
	}

	SET_REF_OWNER( newanim );

	if (newanim->Load_W3D(cload) != HMorphAnimClass::OK) {
		// load failed!
		newanim->Release_Ref();
		goto Error;
	} else if (Peek_Anim(newanim->Get_Name()) != nullptr) {
		// duplicate exists!
		newanim->Release_Ref();	// Release the one we just loaded
		goto Error;
	} else {
		Add_Anim( newanim );
		newanim->Release_Ref();
	}

	return 0;

Error:

	return 1;
}


/***********************************************************************************************
 * HAnimManagerClass::Load_Raw_Anim -- Load a raw anim                                         *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   5/23/2000  gth : Created.                                                                 *
 *=============================================================================================*/
int HAnimManagerClass::Load_Raw_Anim(ChunkLoadClass & cload)
{
	HRawAnimClass * newanim = W3DNEW HRawAnimClass;

	if (newanim == nullptr) {
		goto Error;
	}

	SET_REF_OWNER( newanim );

	if (newanim->Load_W3D(cload) != HRawAnimClass::OK) {
		// load failed!
		newanim->Release_Ref();
		goto Error;
	} else if (Peek_Anim(newanim->Get_Name()) != nullptr) {
		// duplicate exists!
		newanim->Release_Ref();	// Release the one we just loaded
		goto Error;
	} else {
		Add_Anim( newanim );
		newanim->Release_Ref();
	}

	return 0;

Error:

	return 1;
}


/***********************************************************************************************
 * HAnimManagerClass::Load_Compressed_Anim -- load a compressed animation                      *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   5/23/2000  gth : Created.                                                                 *
 *=============================================================================================*/

/*
** bfme-native: BFME / RotWK store animation data in "motion channels" (chunk 0x284) inside a
** compressed animation. ZH's loader skips them, which left every unit in its bind pose.
** We decode them (time-coded keys, or 4/8-bit adaptive deltas - layout as documented by the
** OpenSAGE project) to per-frame values and rebuild a classic raw animation (0x200) in memory.
*/
namespace {

static const uint32 BFME_CHUNK_MOTION_CHANNEL = 0x284;

struct BfmeChunkWriter
{
	std::vector<unsigned char> buf;
	void u8(unsigned v) { buf.push_back((unsigned char)v); }
	void u16(unsigned v) { u8(v & 0xff); u8((v >> 8) & 0xff); }
	void u32(uint32 v) { u16(v & 0xffff); u16(v >> 16); }
	void f32(float f) { uint32 v; memcpy(&v, &f, 4); u32(v); }
	void bytes(const void *p, size_t n) { const unsigned char *b = (const unsigned char *)p; buf.insert(buf.end(), b, b + n); }
	size_t begin(uint32 id) { u32(id); u32(0); return buf.size(); }
	void end(size_t start, bool hasSubChunks) {
		uint32 len = (uint32)(buf.size() - start);
		if (hasSubChunks) len |= 0x80000000u;
		memcpy(&buf[start - 4], &len, 4);
	}
};

static float bfmeFilter(int index)
{
	static float table[256];
	static bool valid = false;
	if (!valid) {
		for (int i = 0; i < 16; ++i) table[i] = powf(10.0f, (float)(i - 8));
		for (int i = 0; i < 240; ++i) table[16 + i] = 1.0f - WWMath::Sin(DEG_TO_RAD(90.0f * (i / 240.0f)));
		valid = true;
	}
	return table[index & 0xff];
}

// decodes one 0x284 channel into numFrames*vlen floats; returns false on malformed data
static bool bfmeDecodeMotionChannel(const unsigned char *d, uint32 len, uint32 numFrames,
	int &type, int &pivot, int &vlen, std::vector<float> &out)
{
	if (len < 8 || numFrames == 0) return false;
	const int deltaType = d[1];
	vlen = d[2];
	type = d[3];
	uint16 numTimeCodes; memcpy(&numTimeCodes, d + 4, 2);
	uint16 piv; memcpy(&piv, d + 6, 2);
	pivot = piv;
	if (vlen < 1 || vlen > 4) return false;
	out.assign((size_t)numFrames * vlen, 0.0f);
	const unsigned char *p = d + 8;
	const unsigned char *e = d + len;

	if (deltaType == 0) {
		// time coded: keys, (pad to 4), values
		if (numTimeCodes == 0) return false;
		std::vector<uint16> keys(numTimeCodes);
		if (p + 2 * numTimeCodes > e) return false;
		memcpy(&keys[0], p, 2 * numTimeCodes);
		p += 2 * numTimeCodes;
		if (numTimeCodes & 1) p += 2;
		if (p + 4 * vlen * numTimeCodes > e) return false;
		std::vector<float> vals((size_t)numTimeCodes * vlen);
		memcpy(&vals[0], p, 4 * vals.size());
		for (uint32 f = 0; f < numFrames; ++f) {
			int k = 0;
			while (k + 1 < numTimeCodes && keys[k + 1] <= f) ++k;
			float t = 0.0f;
			if (k + 1 < numTimeCodes && keys[k + 1] > keys[k] && f > keys[k])
				t = (float)(f - keys[k]) / (float)(keys[k + 1] - keys[k]);
			const int k2 = (k + 1 < numTimeCodes) ? k + 1 : k;
			float dot = 0.0f;
			for (int v = 0; v < vlen; ++v) dot += vals[k * vlen + v] * vals[k2 * vlen + v];
			const float sign = (vlen == 4 && dot < 0.0f) ? -1.0f : 1.0f;	// shortest quaternion path
			float n = 0.0f;
			for (int v = 0; v < vlen; ++v) {
				float a = vals[k * vlen + v], b = vals[k2 * vlen + v] * sign;
				float r = a + (b - a) * t;
				out[f * vlen + v] = r;
				n += r * r;
			}
			if (vlen == 4 && n > 0.0f) {
				n = 1.0f / sqrtf(n);
				for (int v = 0; v < 4; ++v) out[f * vlen + v] *= n;
			}
		}
		return true;
	}

	if (deltaType != 1 && deltaType != 2) return false;
	// adaptive delta: scale, initial value, then per 16 frames one block per vector component
	const int bits = deltaType == 1 ? 4 : 8;
	const int blockBytes = 1 + bits * 2;
	if (p + 4 + 4 * vlen > e) return false;
	float scale; memcpy(&scale, p, 4); p += 4;
	for (int v = 0; v < vlen; ++v) memcpy(&out[v], p + 4 * v, 4);
	p += 4 * vlen;
	// the channel has its own frame count (a static bone stores 1); later frames hold the last value
	const uint32 chanFrames = numTimeCodes > 0 ? numTimeCodes : 1;
	const uint32 blocks = (chanFrames + 15) >> 4;
	const float scaleFactor = bits == 8 ? 1.0f / 16.0f : 1.0f;
	for (uint32 b = 0; b < blocks; ++b) {
		for (int v = 0; v < vlen; ++v) {
			if (p + blockBytes > e) return false;
			const float deltaScale = bfmeFilter(p[0]) * scale * scaleFactor;
			const unsigned char *db = p + 1;
			for (int k = 0; k < 16; ++k) {
				const uint32 frame = (b << 4) + k + 1;
				if (frame >= chanFrames || frame >= numFrames) break;
				int delta;
				if (bits == 4) {
					int nib = (k & 1) ? (db[k >> 1] >> 4) : (db[k >> 1] & 0x0f);
					delta = (nib & 8) ? nib - 16 : nib;
				} else {
					delta = (signed char)db[k];
				}
				out[frame * vlen + v] = out[(frame - 1) * vlen + v] + deltaScale * delta;
			}
			p += blockBytes;
		}
	}
	for (uint32 f = chanFrames; f < numFrames; ++f)
		for (int v = 0; v < vlen; ++v)
			out[f * vlen + v] = out[(chanFrames - 1) * vlen + v];
	if (vlen == 4) {
		for (uint32 f = 0; f < numFrames; ++f) {
			float *q = &out[f * 4];
			float n = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
			if (n > 0.0f) { n = 1.0f / sqrtf(n); q[0] *= n; q[1] *= n; q[2] *= n; q[3] *= n; }
		}
	}
	return true;
}

} // namespace

int HAnimManagerClass::bfme_Load_Motion_Channel_Anim(const unsigned char *data, uint32 len)
{
	// gather the header and channels
	const unsigned char *hdr = nullptr;
	uint32 hdrLen = 0;
	bool hasMotion = false;
	for (uint32 o = 0; o + 8 <= len;) {
		uint32 id, sz; memcpy(&id, data + o, 4); memcpy(&sz, data + o + 4, 4); sz &= 0x7fffffff;
		if (o + 8 + sz > len) break;
		if (id == W3D_CHUNK_COMPRESSED_ANIMATION_HEADER) { hdr = data + o + 8; hdrLen = sz; }
		if (id == BFME_CHUNK_MOTION_CHANNEL) hasMotion = true;
		o += 8 + sz;
	}
	if (!hasMotion || hdr == nullptr || hdrLen < 40)
		return -1;	// not ours

	char name[W3D_NAME_LEN + 1] = { 0 }, hname[W3D_NAME_LEN + 1] = { 0 };
	memcpy(name, hdr + 4, W3D_NAME_LEN);
	memcpy(hname, hdr + 4 + W3D_NAME_LEN, W3D_NAME_LEN);
	uint32 numFrames; memcpy(&numFrames, hdr + 36, 4);
	uint16 frameRate = 30; if (hdrLen >= 42) memcpy(&frameRate, hdr + 40, 2);
	if (numFrames == 0) return 1;

	BfmeChunkWriter w;
	size_t anim = w.begin(W3D_CHUNK_ANIMATION);
	size_t h = w.begin(W3D_CHUNK_ANIMATION_HEADER);
	w.u32(W3D_MAKE_VERSION(4,1));
	w.bytes(name, W3D_NAME_LEN);
	w.bytes(hname, W3D_NAME_LEN);
	w.u32(numFrames);
	w.u32(frameRate);
	w.end(h, false);

	std::vector<float> values;
	for (uint32 o = 0; o + 8 <= len;) {
		uint32 id, sz; memcpy(&id, data + o, 4); memcpy(&sz, data + o + 4, 4); sz &= 0x7fffffff;
		if (o + 8 + sz > len) break;
		const unsigned char *c = data + o + 8;
		if (id == BFME_CHUNK_MOTION_CHANNEL) {
			int type, pivot, vlen;
			if (bfmeDecodeMotionChannel(c, sz, numFrames, type, pivot, vlen, values)) {
				size_t ch = w.begin(W3D_CHUNK_ANIMATION_CHANNEL);
				w.u16(0); w.u16(numFrames - 1); w.u16(vlen); w.u16(type); w.u16(pivot); w.u16(0);
				w.bytes(&values[0], values.size() * sizeof(float));
				w.end(ch, false);
			}
		} else if (id == W3D_CHUNK_COMPRESSED_BIT_CHANNEL && sz >= 8) {
			// time coded visibility: entries are (frame | value<<31), the value holds until the next entry
			uint32 count; memcpy(&count, c, 4);
			uint16 pivot; memcpy(&pivot, c + 4, 2);
			const unsigned char flags = c[6], def = c[7];
			if (8 + 4 * count <= sz) {
				std::vector<unsigned char> bitsOut((numFrames + 7) / 8, 0);
				bool val = def != 0;
				uint32 k = 0;
				for (uint32 f = 0; f < numFrames; ++f) {
					while (k < count) {
						uint32 e; memcpy(&e, c + 8 + 4 * k, 4);
						if ((e & 0x7fffffff) > f) break;
						val = (e >> 31) != 0;
						++k;
					}
					if (val) bitsOut[f >> 3] |= (unsigned char)(1 << (f & 7));
				}
				size_t ch = w.begin(W3D_CHUNK_BIT_CHANNEL);
				w.u16(0); w.u16(numFrames - 1); w.u16(flags); w.u16(pivot); w.u8(def);
				w.bytes(&bitsOut[0], bitsOut.size());
				w.end(ch, false);
			}
		}
		o += 8 + sz;
	}
	w.end(anim, true);

	RAMFileClass file(&w.buf[0], (int)w.buf.size());
	file.Open();
	ChunkLoadClass raw(&file);
	if (!raw.Open_Chunk())
		return 1;
	const int result = Load_Raw_Anim(raw);
	raw.Close_Chunk();
	file.Close();
	return result;
}

int HAnimManagerClass::Load_Compressed_Anim(ChunkLoadClass & cload)
{
	{
		// bfme-native: take a copy of the chunk; BFME motion-channel anims are rebuilt as raw anims
		const uint32 len = cload.Cur_Chunk_Length();
		std::vector<unsigned char> data(len);
		if (len && cload.Read(&data[0], len) == len) {
			const int r = bfme_Load_Motion_Channel_Anim(&data[0], len);
			if (r >= 0)
				return r;
			// a plain ZH compressed animation: rebuild the chunk and use the original loader
			BfmeChunkWriter w;
			size_t c = w.begin(W3D_CHUNK_COMPRESSED_ANIMATION);
			w.bytes(&data[0], len);
			w.end(c, true);
			RAMFileClass file(&w.buf[0], (int)w.buf.size());
			file.Open();
			ChunkLoadClass again(&file);
			if (!again.Open_Chunk())
				return 1;
			const int result = bfme_Load_Compressed_Anim_Original(again);
			again.Close_Chunk();
			file.Close();
			return result;
		}
		return 1;
	}
}

int HAnimManagerClass::bfme_Load_Compressed_Anim_Original(ChunkLoadClass & cload)
{
	HCompressedAnimClass * newanim = W3DNEW HCompressedAnimClass;

	if (newanim == nullptr) {
		goto Error;
	}

	SET_REF_OWNER( newanim );

	if (newanim->Load_W3D(cload) != HCompressedAnimClass::OK) {
		// load failed!
		newanim->Release_Ref();
		goto Error;
	} else if (Peek_Anim(newanim->Get_Name()) != nullptr) {
		// duplicate exists!
		newanim->Release_Ref();	// Release the one we just loaded
		goto Error;
	} else {
		Add_Anim( newanim );
		newanim->Release_Ref();
	}

	return 0;

Error:

	return 1;
}

/***********************************************************************************************
 * HAnimManagerClass::Peek_Anim -- returns a pointer to the specified animation data            *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/11/1997 GH  : Created.                                                                 *
 *=============================================================================================*/
HAnimClass * HAnimManagerClass::Peek_Anim(const char * name)
{
	return (HAnimClass*)AnimPtrTable->Find( name );
}


/***********************************************************************************************
 * HAnimManagerClass::Get_Anim -- returns a pointer to the specified animation data            *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/11/1997 GH  : Created.                                                                 *
 *=============================================================================================*/
HAnimClass * HAnimManagerClass::Get_Anim(const char * name)
{
	HAnimClass * anim = Peek_Anim( name );
	if ( anim != nullptr ) {
		anim->Add_Ref();
	}
	return anim;
}


/***********************************************************************************************
 * HAnimManagerClass::Free_All_Anims -- de-allocate all currently loaded animations            *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/11/1997 GH  : Created.                                                                 *
 *=============================================================================================*/
void HAnimManagerClass::Free_All_Anims()
{
	// Make an iterator, and release all ptrs
	HAnimManagerIterator it( *this );
	for( it.First(); !it.Is_Done(); it.Next() ) {
		HAnimClass *anim = it.Get_Current_Anim();
		anim->Release_Ref();
	}

	// Then clear the table
	AnimPtrTable->Reset();
}

/***********************************************************************************************
 * HAnimManagerClass::Free_All_Anims_With_Exclusion_List -- release animations not in the list *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/12/2002 GH  : Created.                                                                 *
 *=============================================================================================*/
void HAnimManagerClass::Free_All_Anims_With_Exclusion_List(const W3DExclusionListClass & exclusion_list)
{
	// Remove and Release_Ref any animation not in the exclusion list.
	HAnimManagerIterator it( *this );
	for( it.First(); !it.Is_Done(); it.Next() ) {
		HAnimClass *anim = it.Get_Current_Anim();

		if ((anim->Num_Refs() == 1) && (exclusion_list.Is_Excluded(anim) == false)) {
			//WWDEBUG_SAY(("deleting HAnim %s",anim->Get_Name()));
			AnimPtrTable->Remove(anim);
			anim->Release_Ref();
		}
		//else
		//{
		//	WWDEBUG_SAY(("keeping HAnim %s (ref %d)",anim->Get_Name(),anim->Num_Refs()));
		//}
	}
}


/***********************************************************************************************
 * HAnimManagerClass::Create_Asset_List -- Create a list of the W3D files that are loaded      *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/12/2002 GH  : Created.                                                                 *
 *=============================================================================================*/
void HAnimManagerClass::Create_Asset_List(DynamicVectorClass<StringClass> & exclusion_list)
{
	HAnimManagerIterator it( *this );
	for( it.First(); !it.Is_Done(); it.Next() ) {
		HAnimClass *anim = it.Get_Current_Anim();

		// File that this anim came from should be the name after the '.'
		// Anims are named in the format: <skeleton>.<animname>
		const char * anim_name = anim->Get_Name();
		const char * filename = strchr(anim_name,'.');
		if (filename != nullptr) {
			exclusion_list.Add(StringClass(filename+1));
		}
	}
}


/***********************************************************************************************
 * HAnimManagerClass::Add_Anim -- Adds an externally created animation to the manager			  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/31/2000 PDS  : Created.                                                                *
 *=============================================================================================*/
bool HAnimManagerClass::Add_Anim(HAnimClass *new_anim)
{
	WWASSERT (new_anim != nullptr);

	// Increment the refcount on the W3DNEW animation and add it to our table.
	new_anim->Add_Ref ();
	AnimPtrTable->Add( new_anim );

	return true;
}


/*
** Missing Anims
**
** The idea here, allow the system to register which anims are determined to be missing
** so that if they are asked for again, we can quickly return nullptr, without searching the
** disk again.
*/
void	HAnimManagerClass::Register_Missing( const char * name )
{
	MissingAnimTable->Add( W3DNEW MissingAnimClass( name ) );
}

bool	HAnimManagerClass::Is_Missing( const char * name )
{
	return ( MissingAnimTable->Find( name ) != nullptr );
}

void	HAnimManagerClass::Reset_Missing()
{
	// Make an iterator, and release all ptrs
	HashTableIteratorClass it( *MissingAnimTable );
	for( it.First(); !it.Is_Done(); it.Next() ) {
		MissingAnimClass *missing = (MissingAnimClass *)it.Get_Current();
		delete missing;
	}

	// Then clear the table
	MissingAnimTable->Reset();
}


/*
** Iterator converter from HashableClass to HAnimClass
*/
HAnimClass * HAnimManagerIterator::Get_Current_Anim()
{
	return (HAnimClass *)Get_Current();
}

