// =====================================================================
//  MODULE: SNDINST2 (.CPP) -- the client sound module.
//
//  Split from the 0x0047c000 cluster's file: its first half is
//  server-engine code compiled into BOTH images (src/engine/server/
//  objmover.cpp), this half is client-only. Literal pooling and block
//  merging are per-TU, so they must not share one.
// =====================================================================
// *********************************************************************** //
// *********************************************************************** //
//
//   PART TWO -- THE CLIENT'S SOUND MODULE.  A DIFFERENT FILE.
//
//   Everything above belongs in src/engine/server/.  Everything below is
//   lithtech.exe ONLY (no server.dll twin exists for any of it) and belongs
//   in src/engine/lithtech/clientsoundinst.cpp, beside the CSoundDataView
//   and CSoundInstanceView it extends.  The two halves are in one file;
//   SPLIT THEM BEFORE COMPILING, or at
//   the very least check that servermgr.h and the views below do not fight.
//
//   The boundary is the one clientsoundinst.cpp already documents:
//   0x0047bc50-0x0047c62x is the server engine compiled into the client,
//   0x0047c63x-0x0047f9b0 is the client's sound engine.  0x0047c020..
//   0x0047c590 is on one side of it and 0x0047cd90.. is on the other.
//
//   THE ONE NEW STRUCTURAL FACT.  CSoundData+0x64 is Miles' AILSOUNDINFO,
//   and 0x0047cd90 is what proves it: nine consecutive dwords written in
//   AILSOUNDINFO's field order out of the WaveInfo the RIFF parser just
//   filled, and CSoundData::_v08 (0x0047c710) hands `pSrc + 0x64` to
//   AIL_decompress_ADPCM as its first argument, which is that function's
//   AILSOUNDINFO parameter.  The struct is 0x24 bytes and ends exactly
//   where clientsoundinst.cpp's m_nInstances begins at 0x88.
//
// *********************************************************************** //

#include "ltbasedefs.h"
#include "ltcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iltstream.h"


// Win32, declared rather than included so this file stands alone.
extern "C" {
__declspec(dllimport) int   __stdcall CloseHandle(void* hObject);
__declspec(dllimport) void* __stdcall CreateFileA(char const* pName,
                                                  uint32 dwAccess,
                                                  uint32 dwShare,
                                                  void* pSecurity,
                                                  uint32 dwCreate,
                                                  uint32 dwFlags,
                                                  void* hTemplate);
__declspec(dllimport) uint32 __stdcall SetFilePointer(void* hFile,
                                                      long lDistance,
                                                      long* pDistanceHigh,
                                                      uint32 dwMethod);
}

// The sample's file record, CSoundData+0x04.  clientsoundinst.cpp already
// establishes that +0x1c is the file's NAME (CSoundData::Load pushes it for
// "Missing sound file %s" at 0x0047c8f6); +0x10 is the other half of what
// 0x00421750 is handed and nothing here says what it is (names ours).
class CSoundFileRefQ47C
{
public:
	// 0x00 — the sample loaded from this file, or null.  CSoundData::Init
	// (0x0047c702) points it back at the sample and Free (0x0047c7da)
	// clears it (name ours).
	class CSoundDataQ47C* m_pSoundData;      // 0x00
	uint8  m_pad_04[0x10 - 0x04];
	void*  m_pUnknown10;                     // 0x10
	uint8  m_pad_14[0x1c - 0x14];
	char*  m_pName1c;                        // 0x1c
};


// ---- what the two client headers do not model yet ---------------------- //

// mmreg.h's WAVEFORMATEX and the RIFF parser's output, both already decoded
// in src/engine/lithtech/lttexmod.cpp (see its WaveFormatEx / WaveInfo and
// the cross-checks there).  Re-declared rather than shared because that file
// keeps them private; on integration they should move to a header and these
// copies should go.
#pragma pack(push, 1)
struct WaveFormatExQ47C
{
	uint16 wFormatTag;                       // 0x00
	uint16 nChannels;                        // 0x02
	uint32 nSamplesPerSec;                   // 0x04
	uint32 nAvgBytesPerSec;                  // 0x08
	uint16 nBlockAlign;                      // 0x0c
	uint16 wBitsPerSample;                   // 0x0e
	uint16 cbSize;                           // 0x10
};
#pragma pack(pop)

struct WaveInfoQ47C
{
	WaveFormatExQ47C m_Format;               // 0x00  eighteen bytes + two pad
	uint32           m_nDataOffset;          // 0x14
	uint32           m_nDataSize;            // 0x18
	uint32           m_nSampleCount;         // 0x1c
	uint32           m_nLithFlags;           // 0x20
	uint32           m_nLithUnknown24;       // 0x24
	uint32           m_bHasLoop;             // 0x28
	uint32           m_nLoopName;            // 0x2c
	uint32           m_nLoopStart;           // 0x30
	uint32           m_nLoopLength;          // 0x34
};                                           // 0x38

// Miles' AILSOUNDINFO, mss.h's field order.  Names are Miles'.
struct AILSOUNDINFOQ47C
{
	int32  format;                           // 0x00
	void*  data_ptr;                         // 0x04
	uint32 data_len;                         // 0x08
	uint32 rate;                             // 0x0c
	int32  bits;                             // 0x10
	int32  channels;                         // 0x14
	uint32 samples;                          // 0x18
	uint32 block_size;                       // 0x1c
	void*  initial_ptr;                      // 0x20
};                                           // 0x24

// Miles' digital sample formats.  The four values 0..3 the switch below
// produces are DIG_F_MONO_8, DIG_F_MONO_16, DIG_F_STEREO_8, DIG_F_STEREO_16
// in that numeric order, which is what makes the mapping readable: three
// bits per sample-frame byte count, and anything else is mono 8.
#define DIG_F_MONO_8_Q47C     0
#define DIG_F_MONO_16_Q47C    1
#define DIG_F_STEREO_8_Q47C   2
#define DIG_F_STEREO_16_Q47C  3

// stdlith's StructBank / ObjectBank, modelled by hand for the reason
// src/engine/lithtech/clientsoundmgr.cpp gives: object_bank.h is not on this
// target's include path.  Field order and offsets are that file's.
struct StructBankQ47C
{
	uint32 m_StructSize;                     // +0x00
	uint32 m_AlignedStructSize;              // +0x04
	uint32 m_CacheSize;                      // +0x08
	uint32 m_nPages;                         // +0x0c
	uint32 m_nTotalObjects;                  // +0x10
	void*  m_pPageHead;                      // +0x14
	void*  m_FreeListHead;                   // +0x18
};                                           // 0x1c

struct ObjectBankQ47C
{
	void*          m_pVTable;                // +0x00  BaseObjectBank's vptr
	StructBankQ47C m_Bank;                   // +0x04
	uint32         m_CS;                     // +0x20  the empty CS member
};                                           // 0x24

// 0x00490c20 — struct_bank.h's page grower, __cdecl.
int sb_AllocateNewStructPageQ47C(StructBankQ47C* pBank, unsigned long nAllocations);

// struct_bank.h's own body.  `inline` and defined above its caller for the
// reason cltnew.cpp states: /Ob1 expands only what is marked inline, and the
// shipped 0x0047cec0 has it expanded — head, grow, RE-READ of the head,
// unlink.
inline void* sb_AllocateQ47C(StructBankQ47C* pBank)
{
	void* pRet;

	pRet = pBank->m_FreeListHead;
	if (!pRet)
	{
		sb_AllocateNewStructPageQ47C(pBank, pBank->m_CacheSize);
		pRet = pBank->m_FreeListHead;
		if (!pRet)
			return 0;
	}

	pBank->m_FreeListHead = *(void**)pRet;
	return pRet;
}

// object_bank.h's placement-new hook, verbatim.  It is what turns
// ObjectBank<T>::Allocate's `::new((DummyThingForConstructor*)0, pRet) T`
// into the bare `call <ctor>` the shipped code has at 0x0047cf06.
class DummyThingForConstructorQ47C
{
public:
};

inline void* operator new(size_t size, DummyThingForConstructorQ47C* pThing,
                          void* ptr)
{
	return ptr;
}

// 0x0040f7f0 — `*(char**)0x004b17e0 + 0x774`, the client's sound manager
// (binkvideo.cpp, cchandler.cpp, soundinst2.cpp all name this address).
class CSoundMgrQ47C
{
public:
	uint8          m_pad_000[0x130];
	// 0x130 — AIL_open_stream's first argument (0x0047de5d).  Miles calls it
	// the DIG_DRIVER; clientsound.h stops at 0x124 and 0x13c and does not
	// reach this one, so the offset is recorded here rather than guessed.
	void*          m_pDigDriver;             // 0x130
	uint8          m_pad_134[0x284 - 0x134];
	ObjectBankQ47C m_SoundDataBank;          // 0x284  clientsoundmgr.cpp
	uint8          m_pad_2a8[0x7e4 - 0x2a8];
	// 0x7e4 — clientsoundmgr.cpp's m_bConvert16To8 (INITSOUNDINFOFLAG_
	// CONVERT16TO8); CSoundData::Load halves 16-bit PCM when it is set
	// (0x0047cbed).
	uint32         m_bConvert16To8;          // 0x7e4
	uint32         m_bReverbA;               // 0x7e8  clientsound.h

	LTRESULT SetSampleUserData(void* hSample, uint32 nValue);   // 0x00481910
	LTRESULT SetStreamUserData(void* hStream, uint32 nValue);   // 0x004819f0
	void*    GetFreeSWVoice();                                  // 0x00481a30
	void     FreeSWVoice(void* hSample);                        // 0x00481bc0
};

CSoundMgrQ47C* ci_GetSoundMgrQ47C();         // 0x0040f7f0

// 0x00484580 — clientstreams.cpp's memory-stream factory; that file already
// notes "the caller at 0x0047cdb0 takes the result as an ILTStream*".
ILTStream* memstream_OpenQ47C(void* pData, uint32 nLen);

// 0x0048c1a0 — lttexmod.cpp's RIFF/WAVE reader.
LTBOOL ParseWaveFileQ47C(ILTStream* pStream, WaveInfoQ47C* pInfo);


// ---- the sample -------------------------------------------------------- //
//
// clientsoundinst.cpp's CSoundDataView with the fields these two bodies add.
// Offsets 0x08, 0x44, 0x4c, 0x54, 0x88 and 0x8c are that file's; 0x48, 0x50,
// 0x60 and 0x64 are new here and each is justified beside the body that
// reaches it.
class CSoundDataQ47C
{
public:
	// NOT pure, and there is a reason: 0x0047cec0 placement-news one of
	// these, so the class has to be concrete.  The constructor is declared
	// and not defined because the shipped code CALLS it out of line
	// (`call 0x0047c630` at 0x0047cf06); an implicit constructor would be
	// expanded to a bare vptr store and the call would vanish.  On
	// integration this whole view is replaced by clientsoundinst.cpp's
	// CSoundDataView, which already has the right shape.
	CSoundDataQ47C();                                      // 0x0047c630
	virtual ~CSoundDataQ47C();                             // +0x00 0x0047c6b0
	// +0x04 0x0047c6c0 — bind this sample to its file record and load it
	// (`ret $4`: one argument, stored at +0x04).
	virtual LTRESULT Init(CSoundFileRefQ47C* pFile);
	// +0x08 0x0047c710 — decompress pSrc into this sample.  The argument is
	// what carries the AILSOUNDINFO: 0x0047c736 hands `pSrc + 0x64` to
	// AIL_decompress_ADPCM.
	virtual LTRESULT InitFromCompressed(CSoundDataQ47C* pSrc);
	virtual void Free();                                   // +0x0c 0x0047c7d0
	// +0x10 0x0047cf60 — Free without letting go of the file record or
	// the wave description; 0x0047d020 calls it before re-Loading.
	virtual LTRESULT Unload();
	virtual LTBOOL sub_47d020();                           // +0x14 0x0047d020
	// +0x18 0x0047c8c0.  An LTRESULT: Init (0x0047c6dd) refuses on NONZERO.
	virtual LTRESULT Load();

public:
	class CSoundFileRefQ47C* m_pUnknown04;   // 0x04
	WaveInfoQ47C     m_WaveInfo;             // 0x08  (0x38 bytes)
	// 0x40 — set by Init and InitFromCompressed before they load
	// (0x0047c6d3, 0x0047c71e), cleared by Free (0x0047c853) beside, but
	// NOT inside, its 0x38-byte `rep stos` over m_WaveInfo.  Without it
	// every field below sat four bytes low (name ours).
	uint32           m_bInitialised;         // 0x40
	// 0x44/0x48 — the AIL_mem_alloc_lock'ed copy of the file and its length.
	// CSoundData::Load allocates them together at 0x0047ca48 and 0x0047ca3f;
	// 0x0047cdab hands exactly this pair to the memory-stream factory, which
	// is what says 0x48 is the length.
	void*            m_pSampleData;          // 0x44
	uint32           m_nSampleDataSize;      // 0x48
	// 0x4c — where the samples start inside that copy.  Load computes the
	// same sum at 0x0047ca6d.
	void*            m_pDataStart;           // 0x4c
	// 0x50 — the DECOMPRESSED shadow of a compressed sample, allocated by
	// 0x0047cec0 below.  0x00409c67 only ever consults it after checking
	// wFormatTag against 0x11 (IMA ADPCM) and 0x55 (MP3).
	CSoundDataQ47C*  m_pDecompressed;        // 0x50
	uint32           m_nDurationMS;          // 0x54  clientsound.h
	uint32           m_nLoopStart58;         // 0x58  Load, 0x0047c9ce
	uint32           m_nLoopEnd5c;           // 0x5c  Load, 0x0047c9e4
	// 0x60 — the Miles sample type, DIG_F_*; see the switch below.
	uint32           m_nSampleType;          // 0x60
	AILSOUNDINFOQ47C m_SoundInfo;            // 0x64  (0x24 bytes)
	// 0x88 — ltlink.h's LTList {m_nElements, m_Head}: clientsoundinst.cpp's
	// m_nInstances at 0x88 and m_Instances at 0x8c.  It is ONE member and
	// not two: the constructor (0x0047c630) ties 0x8c off and zeroes 0x88
	// twice over — LTList's own constructor, then dl_InitList — and Free
	// and Unload end their walks on dl_RemoveAt's remove-then-decrement.
	LTList           m_InstanceList;         // 0x88

	LTRESULT sub_47cd90();                                 // 0x0047cd90
	LTRESULT sub_47cec0();                                 // 0x0047cec0
	// 0x0047d240 — clientsoundinst.cpp's CSoundDataView::sub_47d240, the
	// sixteen-to-eight-bit sample converter; Load calls it at 0x0047ccb0
	// with `mov %ebp,%ecx`.
	LTRESULT sub_47d240(uint8* pDst, uint16* pSrc, uint32 nCount);
	LTBOOL   sub_47d1a0(class CSoundInstanceQ47C* pInst);  // 0x0047d1a0

	// The decompressed shadow's field if there is one, otherwise this
	// sample's -- one in-class accessor PER FIELD (names ours).  0x0047d9a0
	// selects at field level every time (`mov 0x60(%ecx)` / `mov 0x60(%eax)`
	// per arm, 0x0047d9de), re-tests m_pDecompressed between DataStart and
	// DataSize (0x0047daa6), and still tests it at 0x0047da5c where it is
	// provably null.  A pointer-level `m_pDecompressed ? m_pDecompressed :
	// this` accessor was 365 bytes / 20 blocks; the ternaries written out in
	// the body let VC6 fold the dead 0x0047da5c test and CSE the re-test.
	void* GetSampleDataQ47C()
	{
		return m_pDecompressed ? m_pDecompressed->m_pSampleData : m_pSampleData;
	}
	void* GetDataStartQ47C()
	{
		return m_pDecompressed ? m_pDecompressed->m_pDataStart : m_pDataStart;
	}
	uint32 GetDataSizeQ47C()
	{
		return m_pDecompressed ? m_pDecompressed->m_WaveInfo.m_nDataSize : m_WaveInfo.m_nDataSize;
	}
	// POINTER-level, unlike the three above: Load's sixteen-to-eight-bit
	// path re-tests m_pDecompressed at every access and each time selects
	// `lea 0x8(%eax)` or the sample's own +0x08 (0x0047cbfb..0x0047cc62).
	WaveInfoQ47C* GetWaveInfoQ47C()
	{
		return m_pDecompressed ? &m_pDecompressed->m_WaveInfo : &m_WaveInfo;
	}
};

// clientsoundinst.cpp's CSoundInstanceView, cut down to what the four
// bodies below reach.  Every offset and every name is that file's, with two
// additions called out where they are used: 0x44 is the Miles 2D sample
// handle (0x0047d9a0 hands it to AIL_init_sample) and 0x2c is a second
// millisecond mark beside m_nPriority.
//
// The virtuals are declared in full because 0x0047d9a0 dispatches through
// slot +0x58; only the offsets matter, so they are all pure and no vtable
// of ours is emitted.
class CSoundInstanceQ47C
{
public:
	virtual ~CSoundInstanceQ47C();                         // +0x00
	virtual void _v04()=0;
	// +0x08 0x0047d5c0 — clientsoundinst.cpp's Term; CSoundData::Free calls
	// it on every instance still listed (0x0047c802).
	virtual void Term()=0;
	virtual void _v0c()=0;      virtual void _v10()=0;
	virtual void _v14()=0;      virtual void _v18()=0;
	virtual void _v1c()=0;      virtual void _v20()=0;
	virtual void _v24()=0;      virtual void _v28()=0;
	virtual void _v2c()=0;      virtual void _v30()=0;
	virtual void _v34()=0;      virtual void _v38()=0;
	virtual void _v3c()=0;      virtual void _v40()=0;
	virtual void _v44()=0;      virtual void _v48()=0;
	virtual void _v4c()=0;      virtual void _v50()=0;
	virtual void _v54()=0;
	// +0x58 0x0047e610 — the shipped 0x0047d9a0 ends on `call *0x58(%edx)`.
	virtual LTRESULT ApplyFilter();

public:
	uint32   m_Type;                         // 0x04
	CSoundDataQ47C* m_pData;                 // 0x08
	void*    m_pUnknown0c;                   // 0x0c
	uint32   m_Flags10;                      // 0x10
	uint32   m_nTableSlot;                   // 0x14
	uint8    m_Unknown18;                    // 0x18
	uint8    m_pad_19[0x1c - 0x19];
	uint32   m_Unknown1c;                    // 0x1c
	uint8    m_nUnknown20;                   // 0x20  constructed to 100
	uint8    m_pad_21[0x24 - 0x21];
	uint32   m_nPriority;                    // 0x24
	uint32   m_Unknown28;                    // 0x28
	uint32   m_Unknown2c;                    // 0x2c
	uint32   m_Unknown30;                    // 0x30
	float    m_f34;                          // 0x34  constructed to 1.0f
	uint32   m_nHoldCount;                   // 0x38
	uint32   m_Saved3c;                      // 0x3c
	uint32   m_StateFlags;                   // 0x40
	void*    m_hSample44;                    // 0x44  clientsoundinst.cpp's
	                                         //       m_Unknown44
	void*    m_h3DSample;                    // 0x48
	void*    m_hStream4c;                    // 0x4c
	void*    m_hFile50;                      // 0x50
	uint16   m_nUnknown54;                   // 0x54
	uint8    m_pad_56[0x58 - 0x56];
	uint32   m_Unknown58;                    // 0x58
	LTLink   m_DataLink;                     // 0x5c  clientsoundinst.cpp
	LTVector m_vPos;                         // 0x68  clientsoundinst.cpp
	uint32   m_Unknown74;                    // 0x74
	uint32   m_Unknown78;                    // 0x78
	// 0x7c/0x80/0x9c — clientsoundinst.cpp's m_nUnknown7c, m_Unknown80[7]
	// and m_Unknown9c[7].  ApplyFilter (0x0047e610) says what they are:
	// the index of a Miles filter provider in AIL_enumerate_filters order
	// (-1 = none), up to seven attribute indices in
	// AIL_enumerate_filter_sample_attributes order (-1 ends the list), and
	// the value for each, whose ADDRESS goes to
	// AIL_set_filter_sample_preference (names ours).
	int      m_nFilter;                      // 0x7c
	int      m_FilterParams[7];              // 0x80
	float    m_FilterValues[7];              // 0x9c

	LTRESULT sub_47d860(uint32 nElapsedMS);                // 0x0047d860
	LTRESULT sub_47d9a0();                                 // 0x0047d9a0
	LTRESULT sub_47dd50();                                 // 0x0047dd50
};


// ---- the sample's own virtuals (0x0047c6c0..0x0047c7d0, 0x0047cf60) ----- //
//
// HOME: these are CSoundData's, in vtable 0x004a342c beside the destructor
// clientsoundinst.cpp already writes (0x0047c6b0), so on integration they
// move there with the rest of this view.  Written here because this is the
// file that names them.

// The Miles entries these five reach.  Names and stack sizes are the import
// table's own (_AIL_decompress_ADPCM@12, _AIL_decompress_ASI@24,
// _AIL_mem_free_lock@4, _AIL_enumerate_filters@12, ...).
extern "C" {
__declspec(dllimport) void   __stdcall AIL_lock(void);
__declspec(dllimport) void   __stdcall AIL_unlock(void);
__declspec(dllimport) int32  __stdcall AIL_decompress_ADPCM(AILSOUNDINFOQ47C const* pInfo,
                                                            void** ppOutData,
                                                            uint32* pOutSize);
__declspec(dllimport) int32  __stdcall AIL_decompress_ASI(void const* pInData,
                                                          uint32 nInSize,
                                                          char const* pExt,
                                                          void** ppOutData,
                                                          uint32* pOutSize,
                                                          void* pCallback);
__declspec(dllimport) void   __stdcall AIL_mem_free_lock(void* pMem);
__declspec(dllimport) int32  __stdcall AIL_enumerate_filters(uint32* pNext,
                                                             uint32* pDest,
                                                             char** ppName);
__declspec(dllimport) void*  __stdcall AIL_set_sample_processor(void* hSample,
                                                                uint32 nStage,
                                                                uint32 hProvider);
__declspec(dllimport) int32  __stdcall AIL_enumerate_filter_sample_attributes(
                                                             uint32 hProvider,
                                                             uint32* pNext,
                                                             void* pDest);
__declspec(dllimport) void   __stdcall AIL_set_filter_sample_preference(void* hSample,
                                                                        char const* pName,
                                                                        void const* pValue);
}

// 0x004b4064 — the running total of sample memory: Load adds each
// AIL_mem_alloc_lock'ed copy's length (0x0047ca3f), InitFromCompressed adds
// the decompressed length (0x0047c7b3) and Free/Unload take it back
// (0x0047c8af, 0x0047d005).  Uninitialised data (name ours), defined here:
// every reader but commonphysics.cpp's phys_ResetMoveCounter (0x004451a0,
// which externs it as g_nMoveCounter4B4064) is in this file.
uint32 g_nSoundMemoryQ47C;

// object_bank.h's ObjectBank<T>::Free, expanded: the explicit destructor
// (`push $0; call *(%edx)` — the deleting thunk with the flag clear) and
// struct_bank.h's two-store sb_Free.
inline void ob_FreeQ47C(ObjectBankQ47C* pBank, CSoundDataQ47C* pObj)
{
	pObj->~CSoundDataQ47C();
	*(void**)pObj = pBank->m_Bank.m_FreeListHead;
	pBank->m_Bank.m_FreeListHead = pObj;
}


// 0x0047c6c0, 74 bytes.  Free, bind to the file record, Load; on success
// start an empty instance list and point the record back at this sample.
LTRESULT CSoundDataQ47C::Init(CSoundFileRefQ47C* pFile)
{
	Free();

	m_pUnknown04   = pFile;
	m_bInitialised = 1;

	if (Load() != LT_OK)
		return LT_ERROR;

	dl_InitList(&m_InstanceList);
	m_pUnknown04->m_pSoundData = this;

	return LT_OK;
}


// 0x0047c710, 178 bytes.  Become the decompressed copy of pSrc.  IMA ADPCM
// (0x11) goes through AIL_decompress_ADPCM on pSrc's AILSOUNDINFO, MP3
// (0x55) through AIL_decompress_ASI on its raw data with the ".mp3"
// extension (0x004ae6c0); both write the new buffer and its length straight
// into +0x44/+0x48.  The resulting RIFF image is then re-parsed by
// sub_47cd90; if that fails the buffer goes back to Miles.
LTRESULT CSoundDataQ47C::InitFromCompressed(CSoundDataQ47C* pSrc)
{
	Free();

	m_bInitialised = 1;

	if (pSrc->m_WaveInfo.m_Format.wFormatTag == 0x11)
	{
		if (!AIL_decompress_ADPCM(&pSrc->m_SoundInfo, &m_pSampleData,
		                          &m_nSampleDataSize))
		{
			AIL_unlock();
			return LT_ERROR;
		}
	}
	else if (pSrc->m_WaveInfo.m_Format.wFormatTag == 0x55)
	{
		if (!AIL_decompress_ASI(pSrc->m_pDataStart,
		                        pSrc->m_WaveInfo.m_nDataSize, ".mp3",
		                        &m_pSampleData, &m_nSampleDataSize, 0))
		{
			AIL_unlock();
			return LT_ERROR;
		}
	}
	else
	{
		return LT_ERROR;
	}

	if (sub_47cd90() != LT_OK)
	{
		AIL_lock();
		AIL_mem_free_lock(m_pSampleData);
		AIL_unlock();
		m_pSampleData = LTNULL;
		return LT_ERROR;
	}

	g_nSoundMemoryQ47C += m_nSampleDataSize;

	return LT_OK;
}


// 0x0047c7d0, 236 bytes.  Let go of everything: the file record's back
// pointer, every instance still playing this sample (each is Term'd and
// unlinked), the wave description, the decompressed shadow (back into the
// manager's object bank at +0x284), and the sample memory.
void CSoundDataQ47C::Free()
{
	LTLink*         pCur;
	LTLink*         pNext;
	ObjectBankQ47C* pBank;

	if (m_pUnknown04)
		m_pUnknown04->m_pSoundData = LTNULL;

	m_pUnknown04 = LTNULL;

	pCur = m_InstanceList.m_Head.m_pNext;
	while (pCur != &m_InstanceList.m_Head)
	{
		pNext = pCur->m_pNext;

		((CSoundInstanceQ47C*)pCur->m_pData)->Term();

		dl_RemoveAt(&m_InstanceList, pCur);
		dl_TieOff(pCur);

		pCur = pNext;
	}

	dl_InitList(&m_InstanceList);

	memset(&m_WaveInfo, 0, sizeof(m_WaveInfo));
	m_bInitialised = 0;

	if (m_pDecompressed)
	{
		pBank = &ci_GetSoundMgrQ47C()->m_SoundDataBank;
		ob_FreeQ47C(pBank, m_pDecompressed);
		m_pDecompressed = LTNULL;
	}

	if (m_pSampleData)
	{
		AIL_lock();
		AIL_mem_free_lock(m_pSampleData);
		AIL_unlock();

		m_pSampleData = LTNULL;
		g_nSoundMemoryQ47C -= m_nSampleDataSize;
		m_pDataStart = LTNULL;
	}
}



// struct_bank.h's sb_Free (Load's shadow release and sub_47cec0's).
inline void sb_FreeQ47C(StructBankQ47C* pBank, void* pObj)
{
	*(void**)pObj = pBank->m_FreeListHead;
	pBank->m_FreeListHead = pObj;
}

// The rest of what Load reaches.
extern "C" {
__declspec(dllimport) void*  __stdcall AIL_mem_alloc_lock(uint32 nBytes);
}

// 0x004b17e0 — the client manager; Load reads only its file manager, the
// pointer clientconsole.cpp and cchandler.cpp also name at +0x1534 (view
// names ours).
class CFileMgrQ47C;
class CClientMgrQ47C
{
public:
	uint8         m_pad_0000[0x1534];
	CFileMgrQ47C* m_pFileMgr;                // 0x1534
};
extern CClientMgrQ47C* g_pClientMgrQ47C;     // 0x004b17e0

// 0x00404890 — lt2new2.cpp's sub_00404890 (cmodelload.cpp's cm_OpenStream):
// a stream over one file record, null when the manager is.
ILTStream* cm_OpenStreamQ47C(CFileMgrQ47C* pFileMgr, CSoundFileRefQ47C* pRec);

extern int   g_DebugLevel;                   // 0x004b0ab0
void dsi_ServerError(char* pFormat, ...);    // 0x0042e6f0


// ======================================================================= //
//  0x0047c8c0, 1228 bytes (to the `ret` at 0x0047cd8b).  CSoundData::Load,
//  vtable 0x004a342c slot +0x18 (name ours: its three messages are
//  "Missing sound file %s", "Corrupt sound file %s", "Loaded sound file
//  %s").  One function: the `ret` at 0x0047c913 is the missing-file early
//  return, and 0x0047c914 (which the enumerator took for a second start) is
//  the target of the `jne` at 0x0047c8e8.
//
//  Open the record, parse the RIFF headers into m_WaveInfo, work out the
//  duration and the loop marks in milliseconds; then, unless the file is
//  flagged to stream (m_nLithFlags bit 0), read the whole file into Miles
//  memory, fill the AILSOUNDINFO, decompress an ADPCM/MP3 file that asks
//  for it (bit 1) and adopt its shadow's buffer, or halve 16-bit PCM when
//  the manager was set up to convert.  Finally the Miles sample type, as
//  sub_47cd90 computes it.
// ======================================================================= //
LTRESULT CSoundDataQ47C::Load()
{
	ILTStream*      pStream;
	LTBOOL          bParsed;
	uint32          nHeader;
	uint8*          pNewData;
	CSoundDataQ47C* pObj;
	ObjectBankQ47C* pBank;
	int             nFrameBytes;

	pStream = cm_OpenStreamQ47C(g_pClientMgrQ47C->m_pFileMgr, m_pUnknown04);
	if (!pStream)
	{
		if (g_DebugLevel >= 2)
			dsi_ServerError("Missing sound file %s", m_pUnknown04->m_pName1c);

		return LT_ERROR;
	}

	pStream->SeekTo(0);

	bParsed = ParseWaveFileQ47C(pStream, &m_WaveInfo);

	// IMA ADPCM is only accepted at four bits per sample.
	if ((m_WaveInfo.m_Format.wFormatTag == 0x11 &&
	     m_WaveInfo.m_Format.wBitsPerSample != 4) || !bParsed)
	{
		pStream->Release();

		if (g_DebugLevel >= 2)
			dsi_ServerError("Corrupt sound file %s", m_pUnknown04->m_pName1c);

		return LT_ERROR;
	}

	// sub_47cd90's duration, verbatim.  The value is kept in a frame slot
	// (0x0047c99a) and read back for the loop clamps: VC6's own CSE of the
	// member, sharing that slot with GetLen()'s result and pNewData below.
	// A named `nDuration` or `nLen` local takes a slot of its own and the
	// frame grows from 0xc to 0x10, so neither is written.
	m_nDurationMS = (uint32)((float)m_WaveInfo.m_nDataSize * 1000.0f /
	                         (float)m_WaveInfo.m_Format.nAvgBytesPerSec + 0.5f);

	// The loop region, samples to milliseconds, clamped to the sample; an
	// empty or inverted one is no loop at all.
	if (m_WaveInfo.m_bHasLoop)
	{
		if (m_WaveInfo.m_nLoopLength > 0 && m_WaveInfo.m_Format.nSamplesPerSec)
		{
			m_nLoopStart58 = m_WaveInfo.m_nLoopStart * 1000 /
			                 m_WaveInfo.m_Format.nSamplesPerSec;
			m_nLoopEnd5c   = m_nLoopStart58 + m_WaveInfo.m_nLoopLength * 1000 /
			                 m_WaveInfo.m_Format.nSamplesPerSec;

			// The two clamps are spelled differently, and the bytes say so:
			// the start is selected in %ecx and stored back UNCONDITIONALLY
			// (0x0047c9e2..0x0047c9ed), the end is stored only when it is
			// over (0x0047c9f2) and then re-read from the member for the
			// last test.  An `if` for the start gives `jb` and a reload.
			m_nLoopStart58 = (m_nLoopStart58 > m_nDurationMS) ? m_nDurationMS : m_nLoopStart58;

			if (m_nLoopEnd5c > m_nDurationMS)
				m_nLoopEnd5c = m_nDurationMS;

			if (m_nLoopEnd5c <= m_nLoopStart58)
				m_WaveInfo.m_bHasLoop = 0;
		}
		else
		{
			m_WaveInfo.m_bHasLoop = 0;
		}
	}

	if (!m_WaveInfo.m_bHasLoop)
	{
		m_nLoopStart58 = 0;
		m_nLoopEnd5c   = m_nDurationMS;
	}

	if (!(m_WaveInfo.m_nLithFlags & 1))
	{
		// The whole file into Miles memory.
		pStream->SeekTo(0);
		m_nSampleDataSize = pStream->GetLen();

		AIL_lock();
		m_pSampleData = AIL_mem_alloc_lock(m_nSampleDataSize);
		AIL_unlock();

		pStream->Read(m_pSampleData, m_nSampleDataSize);
		pStream->Release();

		m_pDataStart = (uint8*)m_pSampleData + m_WaveInfo.m_nDataOffset;

		if ((m_WaveInfo.m_Format.wFormatTag != 0x11 &&
		     m_WaveInfo.m_Format.wFormatTag != 0x55) ||
		    (m_WaveInfo.m_nLithFlags & 6))
		{
			m_SoundInfo.format      = m_WaveInfo.m_Format.wFormatTag;
			m_SoundInfo.data_ptr    = m_pDataStart;
			m_SoundInfo.data_len    = m_WaveInfo.m_nDataSize;
			m_SoundInfo.rate        = m_WaveInfo.m_Format.nSamplesPerSec;
			m_SoundInfo.bits        = m_WaveInfo.m_Format.wBitsPerSample;
			m_SoundInfo.channels    = m_WaveInfo.m_Format.nChannels;
			m_SoundInfo.samples     = m_WaveInfo.m_nSampleCount;
			m_SoundInfo.block_size  = m_WaveInfo.m_Format.nBlockAlign;
			m_SoundInfo.initial_ptr = m_pDataStart;

			// Decompress up front and keep only the result: the shadow's
			// buffer and description become this sample's, and the shadow
			// goes back to the bank.
			if ((m_WaveInfo.m_Format.wFormatTag == 0x11 ||
			     m_WaveInfo.m_Format.wFormatTag == 0x55) &&
			    (m_WaveInfo.m_nLithFlags & 2))
			{
				if (sub_47cec0() != LT_OK)
				{
					AIL_lock();
					AIL_mem_free_lock(m_pSampleData);
					AIL_unlock();
					m_pSampleData = LTNULL;

					return LT_ERROR;
				}

				g_nSoundMemoryQ47C -= m_pDecompressed->m_nSampleDataSize;

				AIL_lock();
				AIL_mem_free_lock(m_pSampleData);
				AIL_unlock();

				m_WaveInfo.m_nDataOffset  = m_pDecompressed->m_WaveInfo.m_nDataOffset;
				m_WaveInfo.m_nDataSize    = m_pDecompressed->m_WaveInfo.m_nDataSize;
				m_WaveInfo.m_nSampleCount = m_pDecompressed->m_WaveInfo.m_nSampleCount;
				m_WaveInfo.m_Format       = m_pDecompressed->m_WaveInfo.m_Format;
				m_pSampleData     = m_pDecompressed->m_pSampleData;
				m_nSampleDataSize = m_pDecompressed->m_nSampleDataSize;
				m_pDataStart      = m_pDecompressed->m_pDataStart;
				m_pDecompressed->m_pSampleData = LTNULL;
				m_SoundInfo = m_pDecompressed->m_SoundInfo;

				pBank = &ci_GetSoundMgrQ47C()->m_SoundDataBank;
				pObj = m_pDecompressed;
				pObj->~CSoundDataQ47C();
				sb_FreeQ47C(&pBank->m_Bank, pObj);
				m_pDecompressed = LTNULL;
			}

			// 16-bit PCM down to 8-bit when the manager asks for it: the
			// header is copied, the samples converted, the tail copied.
			if (m_WaveInfo.m_Format.wFormatTag != 0x11 &&
			    m_WaveInfo.m_Format.wFormatTag != 0x55 &&
			    ci_GetSoundMgrQ47C()->m_bConvert16To8 &&
			    GetWaveInfoQ47C()->m_Format.wBitsPerSample == 16 &&
			    GetWaveInfoQ47C()->m_Format.wFormatTag == 1)
			{
				m_WaveInfo.m_nDataSize >>= 1;
				GetWaveInfoQ47C()->m_Format.wBitsPerSample = 8;
				GetWaveInfoQ47C()->m_Format.nAvgBytesPerSec >>= 1;
				GetWaveInfoQ47C()->m_Format.nBlockAlign >>= 1;

				AIL_lock();
				pNewData = (uint8*)AIL_mem_alloc_lock(m_nSampleDataSize -
				                                      m_WaveInfo.m_nDataSize);
				AIL_unlock();

				nHeader = (uint8*)m_pDataStart - (uint8*)m_pSampleData;

				memcpy(pNewData, m_pSampleData, nHeader);
				sub_47d240(pNewData + nHeader, (uint16*)m_pDataStart,
				           m_WaveInfo.m_nDataSize);
				memcpy(pNewData + (m_WaveInfo.m_nDataSize + nHeader),
				       (uint8*)m_pSampleData + m_WaveInfo.m_nDataSize * 2 + nHeader,
				       m_nSampleDataSize - m_WaveInfo.m_nDataSize * 2 - nHeader);

				AIL_lock();
				AIL_mem_free_lock(m_pSampleData);
				AIL_unlock();

				m_pSampleData      = pNewData;
				m_pDataStart       = pNewData + nHeader;
				m_nSampleDataSize -= m_WaveInfo.m_nDataSize;
			}
		}

		g_nSoundMemoryQ47C += m_nSampleDataSize;
	}
	else
	{
		pStream->Release();
	}

	nFrameBytes = ((m_WaveInfo.m_Format.nChannels + 1) *
	               m_WaveInfo.m_Format.wBitsPerSample) >> 3;

	switch (nFrameBytes)
	{
		case 3:  m_nSampleType = DIG_F_STEREO_8_Q47C;  break;
		case 4:  m_nSampleType = DIG_F_MONO_16_Q47C;   break;
		case 6:  m_nSampleType = DIG_F_STEREO_16_Q47C; break;
		default: m_nSampleType = DIG_F_MONO_8_Q47C;    break;
	}

	if (g_DebugLevel >= 2)
		dsi_ServerError("Loaded sound file %s", m_pUnknown04->m_pName1c);

	return LT_OK;
}


// ======================================================================= //
//  0x0047cd90, 303 bytes (0x0047cd90..0x0047cebf, `ret` at 0x0047cebe, then
//  one alignment nop).  No server twin: client only.
//
//  Re-read the RIFF headers out of the buffer already in memory and rebuild
//  the Miles description of it.  CSoundData::Load does the same work from a
//  FILE stream; this does it from the sample's own copy, which is why it is
//  what CSoundData::_v08 calls (0x0047c77d) after Miles has decompressed
//  ADPCM or MP3 into a fresh buffer.
//
//  WHAT PINS IT.
//   * 0x00484580 is clientstreams.cpp's memstream_Open, whose own comment
//     already names 0x0047cdb0 as its caller; the two arguments are +0x44
//     and +0x48, so those are the buffer and its length.
//   * 0x0048c1a0 is lttexmod.cpp's ParseWaveFile, handed `this + 8` — the
//     same argument CSoundData::Load passes at 0x0047c921, which is what
//     put WaveInfo at CSoundData+0x08 in the first place.
//   * the nine stores from 0x0047cdfe to 0x0047ce36 are AILSOUNDINFO in
//     field order (format, data_ptr, data_len, rate, bits, channels,
//     samples, block_size, initial_ptr) out of WaveInfo, at +0x64..+0x84.
//   * the duration is the identical five-instruction sequence Load has at
//     0x0047c96b — `fild` the qword {m_nDataSize,0}, `fmul` 1000.0f
//     (0x004a1afc), `fidiv` nAvgBytesPerSec, `fadd` 0.5f (0x004a1a74),
//     `__ftol`.  Both constants are already named by other files
//     (clientsoundinst.cpp for the 1000.0f, soundinst2.cpp for the 0.5f).
//   * the `sar $0x3` has NO sign correction, so the source shifted rather
//     than divided; the three `sub/je` steps over 3, 4 and 6 are VC6's
//     linear lowering of a sparse switch, not an if-chain (an if-chain
//     would re-compare the value each time).
//
//  COIN-FLIP, and the one place this is likeliest to miss: the exact
//  spelling of the duration expression.  `fild` of a qword whose high dword
//  is zero is how VC6 converts a uint32, and `fidiv` on a dword is how it
//  divides by an int — so the two operands are spelled with different
//  signedness in the source.  The alternative is
//  `(uint32)((double)m_WaveInfo.m_nDataSize * 1000.0f /
//   (int)m_WaveInfo.m_Format.nAvgBytesPerSec + 0.5f)`.  Whatever
//  CSoundData::Load turns out to need at 0x0047c96b, this needs the same.
// ======================================================================= //
LTRESULT CSoundDataQ47C::sub_47cd90()
{
	ILTStream*    pStream;
	int           nFrameBytes;
	WaveInfoQ47C* pWave;
	uint32* pType;

	if (!m_pSampleData)
		return LT_ERROR;

	pStream = memstream_OpenQ47C(m_pSampleData, m_nSampleDataSize);

	if (!pStream)
		return LT_ERROR;

	// The pointer handed to the parser (`lea 0x8(%esi),%edi`, 0x0047cdca)
	// is what the frame-size switch reads through: it RE-LOADS
	// nChannels/wBitsPerSample off %edi (0x0047ce3c) after the
	// AILSOUNDINFO stores instead of reusing the values those stores
	// loaded, which is what a different access path does to VC6's CSE.
	pWave = &m_WaveInfo;
	if (!ParseWaveFileQ47C(pStream, pWave))
		return LT_ERROR;

	pStream->Release();

	m_pDataStart = (uint8*)m_pSampleData + m_WaveInfo.m_nDataOffset;

	m_SoundInfo.format      = m_WaveInfo.m_Format.wFormatTag;
	m_SoundInfo.data_ptr    = m_pDataStart;
	m_SoundInfo.data_len    = m_WaveInfo.m_nDataSize;
	m_SoundInfo.rate        = m_WaveInfo.m_Format.nSamplesPerSec;
	m_SoundInfo.bits        = m_WaveInfo.m_Format.wBitsPerSample;
	m_SoundInfo.channels    = m_WaveInfo.m_Format.nChannels;
	m_SoundInfo.samples     = m_WaveInfo.m_nSampleCount;
	m_SoundInfo.block_size  = m_WaveInfo.m_Format.nBlockAlign;
	m_SoundInfo.initial_ptr = m_pDataStart;

	nFrameBytes = ((pWave->m_Format.nChannels + 1) *
	               pWave->m_Format.wBitsPerSample) >> 3;

	// Stored THROUGH A POINTER (name ours): an indirect uint32 store is
	// what VC6 must assume may alias m_WaveInfo, so the duration below
	// reloads m_nDataSize (0x0047ce7c) instead of reusing the value the
	// data_len store loaded.  Stored as m_nSampleType directly, the load
	// is CSE'd (300 bytes, 10 blocks).
	pType = &m_nSampleType;
	switch (nFrameBytes)
	{
		case 3:  *pType = DIG_F_STEREO_8_Q47C;  break;
		case 4:  *pType = DIG_F_MONO_16_Q47C;   break;
		case 6:  *pType = DIG_F_STEREO_16_Q47C; break;
		default: *pType = DIG_F_MONO_8_Q47C;    break;
	}

	// Float arithmetic (fmuls/fadds on 4-byte constants, 0x0047ce9e), both
	// uint32s converted, read off the member (0x20/0x10(%esi)); through
	// pWave they come out as 0x18/0x8(%edi).
	m_nDurationMS = (uint32)((float)m_WaveInfo.m_nDataSize * 1000.0f /
	                         (float)m_WaveInfo.m_Format.nAvgBytesPerSec + 0.5f);

	return LT_OK;
}


// ======================================================================= //
//  0x0047cec0, 156 bytes (0x0047cec0..0x0047cf5c, `ret` at 0x0047cf5b, then
//  four alignment nops).  No server twin.
//
//  Give a compressed sample its decompressed shadow: take a fresh CSoundData
//  out of the manager's object bank and let it decompress this one.  The
//  callers are 0x00409c70 and 0x00409df5, which both reach it only after
//  testing wFormatTag against 0x11 and 0x55, and 0x0047caf7 inside
//  CSoundData::Load.
//
//  WHAT PINS IT.
//   * the bank is at ci_GetSoundMgr() + 0x284, which clientsoundmgr.cpp
//     already names m_SoundDataBank, and the body walks it at +0x288 (the
//     StructBank inside the ObjectBank), +0x290 (m_CacheSize) and +0x2a0
//     (m_FreeListHead).
//   * the allocate is struct_bank.h's sb_Allocate expanded: head, grow
//     through 0x00490c20, RE-READ of the head, unlink — followed by
//     object_bank.h's own redundant null test and a bare `call 0x0047c630`,
//     which is CSoundData's constructor.  That pair is
//     `ObjectBank<T>::Allocate()` exactly.
//   * the free is object_bank.h's `Free`: an explicit destructor call
//     (`push $0; call *(%edx)` — the deleting thunk with the flag clear)
//     and struct_bank.h's two-store sb_Free.  clientsoundmgr.cpp's
//     ReleaseSound already carries that shape verbatim.
//   * GUARD SHAPE.  The `jne 0x0047cf57` at 0x0047cec9 and the `je
//     0x0047cf57` at 0x0047cf24 both land on an `xor %eax,%eax; ret` that
//     sits PAST every other return, so it is a trailing `return LT_OK` and
//     the entry test is positive.
//   * the manager is fetched TWICE, once for the allocate and once for the
//     free (0x0047cecf and 0x0047cf27) — written out at each use.
// ======================================================================= //

// object_bank.h's ObjectBank<T>::Allocate, as an inline helper (the
// redundant null test before the constructor, 0x0047cf02, is the
// template's own).
inline CSoundDataQ47C* ob_AllocateQ47C(ObjectBankQ47C* pBank)
{
	CSoundDataQ47C* pRet;

	pRet = (CSoundDataQ47C*)sb_AllocateQ47C(&pBank->m_Bank);
	if (pRet)
	{
		::new((DummyThingForConstructorQ47C*)0, pRet) CSoundDataQ47C;
	}

	return pRet;
}

LTRESULT CSoundDataQ47C::sub_47cec0()
{
	CSoundDataQ47C* pObj;

	if (!m_pDecompressed)
	{
		m_pDecompressed = ob_AllocateQ47C(&ci_GetSoundMgrQ47C()->m_SoundDataBank);

		if (!m_pDecompressed)
			return LT_ERROR;

		if (m_pDecompressed->InitFromCompressed(this) != LT_OK)
		{
			// ObjectBank<T>::Free(pObj), expanded.  0x0047cf27 fetches the
			// manager FIRST and only then loads m_pDecompressed (once, into
			// %edi) and adds 0x284: the bank and the object are both named
			// before the destructor call.  ob_FreeQ47C (manager argument
			// evaluated last) loads the object first and swaps esi/edi.
			ObjectBankQ47C* pBank = &ci_GetSoundMgrQ47C()->m_SoundDataBank;
			pObj = m_pDecompressed;
			pObj->~CSoundDataQ47C();
			sb_FreeQ47C(&pBank->m_Bank, pObj);
			m_pDecompressed = LTNULL;

			return LT_ERROR;
		}
	}

	return LT_OK;
}


// 0x0047cf60, 183 bytes.  Free's middle: unlink every instance (without
// Term'ing it), drop the shadow and the memory, keep the file record and
// the wave description.  0x0047d020 calls it and then Load.
LTRESULT CSoundDataQ47C::Unload()
{
	LTLink*         pCur;
	LTLink*         pNext;
	ObjectBankQ47C* pBank;

	pCur = m_InstanceList.m_Head.m_pNext;
	while (pCur != &m_InstanceList.m_Head)
	{
		pNext = pCur->m_pNext;

		dl_RemoveAt(&m_InstanceList, pCur);
		dl_TieOff(pCur);

		pCur = pNext;
	}

	dl_InitList(&m_InstanceList);

	if (m_pDecompressed)
	{
		pBank = &ci_GetSoundMgrQ47C()->m_SoundDataBank;
		ob_FreeQ47C(pBank, m_pDecompressed);
		m_pDecompressed = LTNULL;
	}

	if (m_pSampleData)
	{
		AIL_lock();
		AIL_mem_free_lock(m_pSampleData);
		AIL_unlock();

		m_pSampleData = LTNULL;
		g_nSoundMemoryQ47C -= m_nSampleDataSize;
		m_pDataStart = LTNULL;
	}

	return LT_OK;
}



// ======================================================================= //
//  0x0047d1a0, 145 bytes (0x0047d1a0..0x0047d231, `ret $4` at 0x0047d22e,
//  then fifteen alignment nops).  No server twin.
//
//  May this instance take a voice?  Only one instance of a sample gets to
//  play when several want the same one at nearly the same priority; the
//  loser has its retry counter bumped and is told to wait.  Called once, by
//  the manager's update sweep at 0x004811c7, with `%ecx` already holding
//  `pInst->m_pData` from 0x004811ad — which is what makes it a CSoundData
//  member with the instance as its argument.
//
//  WHAT PINS IT.
//   * `0x88(%ecx)` and `0x8c(%ecx)` are clientsoundinst.cpp's m_nInstances
//     and m_Instances, and `0x90(%ecx)` is that LTLink's m_pNext (ltlink.h:
//     m_pPrev 0x00, m_pNext 0x04, m_pData 0x08).  The walk reads +8 for the
//     instance and +4 for the next link, so it is the standard head-tied
//     list walk.
//   * every field of the argument is clientsoundinst.cpp's: m_StateFlags
//     0x40 (bit 5 = a live hold), m_Unknown44 / m_h3DSample 0x44 and 0x48,
//     m_nPriority 0x24, m_nUnknown54 0x54 (a uint16 — `incw`), m_Unknown58
//     0x58.
//   * `sub; cdq; xor; sub` at 0x0047d1f9 is VC6's intrinsic `abs`, and the
//     `jge` after `cmp $5` is signed, so the difference is taken as an int.
//   * the two counter tests are spelled DIFFERENTLY and that is source, not
//     allocation: `cmp $0x1,%ebx; jae` at the top of the body is `>= 1`,
//     `test %ebx,%ebx; jbe` at the bottom is `> 0`.  An unsigned `> 0`
//     compiles to the `test`, an unsigned `>= 1` to the `cmp`.
//   * the early `if (nFound >= 1) break;` is inside the loop and not part of
//     its condition: the bottom of the loop re-tests ONLY `pLink != head`.
// ======================================================================= //
LTBOOL CSoundDataQ47C::sub_47d1a0(CSoundInstanceQ47C* pInst)
{
	LTLink*             pLink;
	CSoundInstanceQ47C* pOther;
	uint32              nFound;

	if (pInst->m_StateFlags & 0x20)
		return LTFALSE;

	if (m_InstanceList.m_nElements > 1)
	{
		nFound = 0;

		pLink = m_InstanceList.m_Head.m_pNext;

		while (pLink != &m_InstanceList.m_Head)
		{
			if (nFound >= 1)
				break;

			pOther = (CSoundInstanceQ47C*)pLink->m_pData;
			pLink  = pLink->m_pNext;

			if (pOther && pOther != pInst &&
			    (pOther->m_hSample44 || pOther->m_h3DSample) &&
			    abs((int)(pOther->m_nPriority - pInst->m_nPriority)) < 5 &&
			    pOther->m_Unknown58 > pInst->m_Unknown58)
			{
				nFound++;
			}
		}

		if (nFound > 0)
		{
			pInst->m_nUnknown54++;
			return LTFALSE;
		}
	}

	return LTTRUE;
}


// ---- Miles, the client's sound manager and the file layer -------------- //
//
// Names and stack sizes are the import table's own; clientsound.h and
// clientsoundmgr.cpp already declare AIL_lock/AIL_unlock, so on integration
// the duplicates here go.
extern "C" {
__declspec(dllimport) void   __stdcall AIL_lock(void);
__declspec(dllimport) void   __stdcall AIL_unlock(void);
__declspec(dllimport) uint32 __stdcall AIL_ms_count(void);
__declspec(dllimport) void   __stdcall AIL_init_sample(void* hSample);
__declspec(dllimport) void   __stdcall AIL_set_sample_type(void* hSample,
                                                           int32 nFormat,
                                                           uint32 nFlags);
__declspec(dllimport) void   __stdcall AIL_set_sample_reverb(void* hSample,
                                                             float fLevel,
                                                             float fReflectTime,
                                                             float fDecayTime);
__declspec(dllimport) void   __stdcall AIL_set_sample_volume(void* hSample,
                                                             int32 nVolume);
__declspec(dllimport) int32  __stdcall AIL_set_sample_file(void* hSample,
                                                           void const* pImage,
                                                           int32 nBlock);
__declspec(dllimport) void   __stdcall AIL_set_sample_address(void* hSample,
                                                              void const* pStart,
                                                              uint32 nLen);
__declspec(dllimport) void   __stdcall AIL_set_sample_playback_rate(void* hSample,
                                                                    int32 nRate);
__declspec(dllimport) void   __stdcall AIL_set_sample_pan(void* hSample,
                                                          int32 nPan);
__declspec(dllimport) void*  __stdcall AIL_open_stream(void* hDig,
                                                       char const* pName,
                                                       int32 nMemSize);
__declspec(dllimport) void   __stdcall AIL_set_stream_playback_rate(void* hStream,
                                                                    int32 nRate);
__declspec(dllimport) void   __stdcall AIL_set_stream_pan(void* hStream,
                                                          int32 nPan);
__declspec(dllimport) void   __stdcall AIL_set_stream_volume(void* hStream,
                                                             int32 nVolume);
}

// The four CSoundMgr members these bodies reach are declared on
// CSoundMgrQ47C above, at the addresses src/engine/lithtech/clientsoundmgr.cpp
// names.  They are thiscall MEMBERS: 0x0047df0b moves the manager into ecx
// and 0x004819f0 ends `ret $8` with no caller-side `add %esp`.

// 0x00421750 — one of the four filemgr symbols src/engine/lithtech/
// clienttexmgr.cpp lists as still unmatched (fs_OpenFileEx, fs_AddResources,
// ar_FindFile, fs_BuildFileList).  Which of them it is is NOT settled here,
// so it is named by address.  Its six arguments are read off 0x0047ddbc:
// two fields of the sample's file record, a path buffer and its size, and
// two out-parameters of which only the first (a byte offset into the file)
// is used by the caller below.
int sub_421750Q47C(void* pTree, char* pName, char* pPathOut, uint32 nPathMax,
                   uint32* pOffset, uint32* pLength);


// ======================================================================= //
//  0x0047d9a0, 381 bytes (0x0047d9a0..0x0047db1d, `ret` at 0x0047db1c, then
//  three alignment nops).  No server twin.
//
//  Put the sample into a Miles software voice and configure the voice.  Two
//  call sites, both in the manager's update sweep (0x00481229 and
//  0x004812d7), both thiscall on the instance with no argument.
//
//  WHAT PINS IT.
//   * 0x00481a30 and 0x00481bc0 are clientsoundmgr.cpp's GetFreeSWVoice and
//     FreeSWVoice, and 0x00481910 is its SetSampleUserData; 0x0040f7f0 is
//     ci_GetSoundMgr.  The handle they trade in is stored at +0x44, and
//     AIL_init_sample / AIL_set_sample_* take it — which is what identifies
//     clientsoundinst.cpp's m_Unknown44 as the Miles 2D sample handle.
//   * the shadow-or-self selection is expanded FIVE times, at FIELD level
//     (0x0047d9d7, 0x0047da55, 0x0047da97, 0x0047daa6, 0x0047dac4).  The
//     expansion at 0x0047da5c is the proof it is an inline accessor: control only reaches it when
//     m_pDecompressed is known zero (the `jne` two instructions earlier),
//     and the test is emitted anyway.
//   * the volume is `(m_nUnknown20 * 127) / 100`: `shl $7 / sub` is the
//     ×127 and the 0x51eb851f magic with `sar $5` plus the sign fix-up is
//     VC6's signed divide by 100.  m_nUnknown20 is the byte
//     clientsoundinst.cpp records as "constructed to 100", so it is a
//     percentage and 127 is Miles' full scale.
//   * the reverb gate is an `||` of two negatives: `test %ecx,%ecx; je
//     <call>` then `test $0x1,%ah; jne <skip>` is exactly
//     `if (!A || !B) call;`.  0x7e8 is clientsound.h's m_bReverbA.
//   * the compressed branch tests wFormatTag against 0x11 (IMA ADPCM) and
//     0x55 (MP3), the same pair CSoundData::Load tests at 0x0047ca7c, and
//     only takes the AIL_set_sample_file path when there is NO decompressed
//     shadow.
//   * the playback rate is `fild` of a DWORD — a SIGNED load — so
//     nSamplesPerSec is cast to int in the source; the loop marks in
//     0x0047d860 below use the qword form for the same reason in reverse.
//   * the three zeros AIL_set_sample_reverb gets are `push $0` each: VC6's
//     encoding of a float literal zero.
//
//  0x0047da5c's `test %ecx,%ecx` is inside the compressed arm, where
//  m_pDecompressed is provably null: the ternary written out in the body is
//  folded there (369 bytes), GetSampleDataQ47C() keeps the test (identical).
// ======================================================================= //
LTRESULT CSoundInstanceQ47C::sub_47d9a0()
{
	if (!m_hSample44)
	{
		m_hSample44 = ci_GetSoundMgrQ47C()->GetFreeSWVoice();

		if (!m_hSample44)
			return LT_ERROR;
	}

	AIL_lock();

	AIL_init_sample(m_hSample44);

	AIL_set_sample_type(m_hSample44, m_pData->m_pDecompressed ? m_pData->m_pDecompressed->m_nSampleType : m_pData->m_nSampleType, 0);

	if (!ci_GetSoundMgrQ47C()->m_bReverbA || !(m_Flags10 & 0x100))
		AIL_set_sample_reverb(m_hSample44, 0.0f, 0.0f, 0.0f);

	AIL_set_sample_volume(m_hSample44, m_nUnknown20 * 127 / 100);

	if ((m_pData->m_WaveInfo.m_Format.wFormatTag == 0x11 ||
	     m_pData->m_WaveInfo.m_Format.wFormatTag == 0x55) &&
	    !m_pData->m_pDecompressed)
	{
		if (!AIL_set_sample_file(m_hSample44,
		                         m_pData->GetSampleDataQ47C(), 0))
		{
			ci_GetSoundMgrQ47C()->FreeSWVoice(m_hSample44);

			m_hSample44 = LTNULL;

			return LT_ERROR;
		}
	}
	else
	{
		AIL_set_sample_address(m_hSample44,
		                       m_pData->GetDataStartQ47C(),
		                       m_pData->GetDataSizeQ47C());
	}

	AIL_set_sample_playback_rate(m_hSample44,
	    (int32)((int)(m_pData->m_pDecompressed ? &m_pData->m_pDecompressed->m_WaveInfo
	                                           : &m_pData->m_WaveInfo)->m_Format.nSamplesPerSec *
	            m_f34 + 0.5f));

	AIL_set_sample_pan(m_hSample44, 0x40);

	ci_GetSoundMgrQ47C()->SetSampleUserData(m_hSample44, (uint32)this);

	AIL_unlock();

	ApplyFilter();

	return LT_OK;
}


// ======================================================================= //
//  0x0047dd50, 468 bytes (0x0047dd50..0x0047df24, `ret` at 0x0047df23, then
//  twelve alignment nops).  No server twin.
//
//  The streaming counterpart of the above: open the sound's own file as a
//  Miles stream and configure the stream.  One call site, 0x004811df, on the
//  arm where the sample's WaveInfo says bit 0 of m_nLithFlags is set.
//
//  WHAT PINS IT.
//   * the tail from 0x0047de9d is 0x0047d9a0's tail with the stream
//     entry points substituted, volume expression and all -- same
//     field-level shadow selection, same `* m_f34 + 0.5f` rate, same 0x40 pan,
//     same SetUserData (0x004819f0 is clientsoundmgr.cpp's
//     SetStreamUserData), same AIL_unlock.
//   * the format string at 0x004ae710 is four backslashes and "%d" (the C
//     literal "\\\\\\\\%d"), and the handle is
//     CreateFileA's, which is Miles' documented way of streaming from an
//     already-open Win32 file.
//   * the CreateFileA arguments read off 0x0047ddd7..0x0047ddee are
//     (szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
//     FILE_ATTRIBUTE_NORMAL, NULL).
//   * 0x0040f7f0's +0x130 is the argument AIL_open_stream wants first, so
//     it is the manager's DIG_DRIVER (clientsound.h stops at 0x124/0x13c and
//     does not name it; the offset is recorded here, not guessed at).
//   * FRAME.  `sub $0x130` with szPath (0x104) at the very top of the block
//     and the two out-parameters at its bottom -- the same shape
//     se_InitWorldModel (0x0046b330) has, where the first-declared aggregate
//     takes the highest address and the scalars follow underneath.  The
//     second out-parameter is written by the callee and never read here.
//   * `mov ds:0x4a140c,%ebp` at 0x0047dd74 caches the AIL_lock import
//     BEFORE the first branch, so both paths call it -- which is why the
//     lock is outside the `if` below rather than inside it.
// ======================================================================= //
LTRESULT CSoundInstanceQ47C::sub_47dd50()
{
	// 0x105 (MAX_PATH + 1): the frame is 0x130, four bytes above the
	// 0x104 buffer, while the size passed to 0x00421750 stays 0x104.
	char   szPath[0x105];
	char   szName[0x20];
	uint32 nLength;
	uint32 nOffset;
	void*  hDriver;

	if (!m_pData)
		return LT_ERROR;

	if (!m_hStream4c)
	{
		if (m_hFile50 != (void*)0xffffffff)
		{
			CloseHandle(m_hFile50);
			m_hFile50 = (void*)0xffffffff;
		}

		if (!sub_421750Q47C(m_pData->m_pUnknown04->m_pUnknown10,
		                    m_pData->m_pUnknown04->m_pName1c,
		                    szPath, 0x104, &nOffset, &nLength))
		{
			return LT_ERROR;
		}

		m_hFile50 = CreateFileA(szPath, 0x80000000, 1, LTNULL, 3, 0x80, LTNULL);

		if (m_hFile50 == (void*)0xffffffff)
			return LT_ERROR;

		if (nOffset)
		{
			if (SetFilePointer(m_hFile50, nOffset, LTNULL, 0) == 0xffffffff)
			{
				CloseHandle(m_hFile50);
				m_hFile50 = (void*)0xffffffff;

				return LT_ERROR;
			}
		}

		// The shipped format string (0x004ae710) is four backslashes and
		// the handle. OpenMiles, which the modern build links, reads only
		// two backslashes and the handle (ext/openmiles stream.zig,
		// handleFromStreamName), so the modern build keeps that form.
#ifdef NOLFSRC_MODERN
		sprintf(szName, "\\\\%d", m_hFile50);
#else
		sprintf(szName, "\\\\\\\\%d", m_hFile50);
#endif

		AIL_lock();

		// Its own statement: 0x0047de58 fetches the driver before the
		// other two arguments are pushed.
		hDriver = ci_GetSoundMgrQ47C()->m_pDigDriver;
		m_hStream4c = AIL_open_stream(hDriver, szName, 0);

		AIL_unlock();

		if (!m_hStream4c)
		{
			CloseHandle(m_hFile50);
			m_hFile50 = (void*)0xffffffff;

			return LT_ERROR;
		}
	}

	AIL_lock();

	// A FIELD-level ternary: 0x0047dea9 selects `lea 0x8(%ecx)` or
	// `add $0x8` per arm and then reads +4 off the result;
	// GetActualQ47C()->m_WaveInfo folds the offsets and is 4 bytes short.
	AIL_set_stream_playback_rate(m_hStream4c,
	    (int32)((int)(m_pData->m_pDecompressed ? &m_pData->m_pDecompressed->m_WaveInfo
	                                           : &m_pData->m_WaveInfo)->m_Format.nSamplesPerSec *
	            m_f34 + 0.5f));

	AIL_set_stream_pan(m_hStream4c, 0x40);

	AIL_set_stream_volume(m_hStream4c, m_nUnknown20 * 127 / 100);

	ci_GetSoundMgrQ47C()->SetStreamUserData(m_hStream4c, (uint32)this);

	AIL_unlock();

	return LT_OK;
}


// ======================================================================= //
//  0x0047d860, 311 bytes (0x0047d860..0x0047d997, `ret $4` at 0x0047d994,
//  then nine alignment nops).  No server twin.
//
//  Advance the sound's play cursor by however many milliseconds have gone
//  by, wrapping it round the sample's loop region when there is one, and
//  answer whether the caller should stop.  One call site, 0x00481359, in the
//  manager's update sweep, which treats a nonzero answer as "this one is
//  finished" (`jne 0x004814aa`).
//
//  THE LOWEST-CONFIDENCE ROW OF THIS SET.  The control flow, the field
//  offsets and the arithmetic are all read straight off the bytes, but two
//  things are judgement calls and are flagged where they occur.
//
//  WHAT PINS IT.
//   * the parameter's own home slot is written at 0x0047d8a6
//     (`mov %eax,0x1c(%esp)` with the frame at E-0x18, i.e. E+4) -- the
//     source assigns to its own parameter, which is why the clock branch
//     below overwrites nElapsedMS instead of using a local.
//   * m_StateFlags' bits are clientsoundinst.cpp's: 0x20 is a live hold,
//     0x01 is consumed-and-cleared here, 0x02 selects the real clock, 0x40
//     is set by Stop.
//   * the loop marks are CSoundData +0x58 and +0x5c, which CSoundData::Load
//     computes at 0x0047c9ce and 0x0047c9e4; both are converted with the
//     qword `fild` (unsigned) and divided by m_f34.
//   * `div %ecx` at 0x0047d945 is UNSIGNED and the remainder (%edx) is what
//     is used, so the wrap is a modulo of the loop span.
//
//  THE TWO JUDGEMENT CALLS.
//   1. `m_nPriority = nCur - nElapsed;` appears ONCE in the shipped code, at
//      0x0047d96d, reached from both arms (0x0047d959 jumps into it).  It is
//      written twice below, once per arm, on the assumption that VC6
//      cross-jumped two identical three-instruction tails.  If the build
//      comes out two instructions long with a duplicated store, the source
//      factored it instead -- put the statement after the two arms and give
//      each of them a `goto`-free early exit.
//   2. the sense of m_nUnknown54 == 1.  It gates ONLY the plain decrement,
//      never the wrap and never the clamp to zero, which reads as
//      "paused" -- but nothing else in the image names it, so it stays
//      m_nUnknown54.
//
//  NEAR, size-exact 311/311, 0 mnemonic blocks.  `nSpan = nEnd - nCur`
//  comes BEFORE the two loop-mark conversions (shipped computes it in %ebp
//  between the first fild and its fdiv, 0x0047d8f9/0x0047d8ff); after them
//  it was 309 bytes and 15 blocks.  Residual: frame slots only -- shipped
//  shares one qword conversion temp at 0x10 and spills nLoopStart to 0x18;
//  ours gives the second conversion its own temp at 0x18 and spills
//  nLoopStart into the first one's dead slot.  Unchanged by: locals in the
//  block, declaration order, no (double) cast, an inline ms-conversion
//  member.
// ======================================================================= //
LTRESULT CSoundInstanceQ47C::sub_47d860(uint32 nElapsedMS)
{
	uint32 nNow;
	uint32 nCur;
	uint32 nEnd;
	uint32 nSpan;
	uint32 nLoopStart;
	uint32 nLoopEnd;

	if (!(m_StateFlags & 0x20))
	{
		if (m_StateFlags & 1)
		{
			m_StateFlags &= ~1;
		}
		else
		{
			if (m_StateFlags & 2)
			{
				AIL_lock();
				nNow = AIL_ms_count();
				AIL_unlock();

				nElapsedMS = nNow - m_Unknown30;

				m_Unknown30 = nNow;
			}

			if (!m_pData)
			{
				m_nPriority = 0;

				return LT_OK;
			}

			nEnd = m_Unknown2c;
			nCur = m_nPriority;

			if (nCur > nEnd && nCur - nEnd < nElapsedMS)
			{
				m_nPriority = nEnd;
			}
			else if ((m_Flags10 & 4) && !(m_StateFlags & 0x40))
			{
				nSpan = nEnd - nCur;

				nLoopStart = (uint32)((double)m_pData->m_nLoopStart58 / m_f34);
				nLoopEnd   = (uint32)((double)m_pData->m_nLoopEnd5c / m_f34);

				if (nLoopEnd - nSpan <= nElapsedMS)
				{
					m_nPriority = nEnd -
					    (nSpan - nLoopStart + nElapsedMS) %
					    (nLoopEnd - nLoopStart) - nLoopStart;
				}
				else if (m_nUnknown54 != 1)
				{
					m_nPriority = nCur - nElapsedMS;
				}
			}
			else if (nCur <= nElapsedMS)
			{
				m_nPriority = 0;
			}
			else if (m_nUnknown54 != 1)
			{
				m_nPriority = nCur - nElapsedMS;
			}

			if (m_nPriority == 0)
			{
				if (!(m_Flags10 & 4) || (m_StateFlags & 0x40))
					return LT_OK;
			}
		}
	}

	return LT_ERROR;
}


// ======================================================================= //
//  0x0047e610, 241 bytes.  CSoundInstance vtable slot +0x58, the one
//  0x0047d9a0 ends on.  HOME: clientsoundinst.cpp, after SetFilter /
//  SetFilterParam / GetFilterParam (0x0047e3c0..0x0047e540), which fill
//  the three members this reads.
//
//  Put this instance's filter on its 2D sample: walk AIL_enumerate_filters
//  to the m_nFilter'th provider (LT_ERROR if the list runs out), install it
//  as the sample's DP_FILTER (1) processor, then walk that provider's
//  sample attributes and, for each whose index is in m_FilterParams, hand
//  Miles the stored value.  Frame 0x20: provider, two HPROENUM/HINTENUM
//  cursors, the provider name, and a 16-byte RIB_INTERFACE_ENTRY whose
//  +0x04 entry_name is what the preference is set by.
// ======================================================================= //
struct RIBInterfaceEntryQ47C
{
	uint32      type;                        // 0x00
	char const* entry_name;                  // 0x04
	uint32      token;                       // 0x08
	uint32      subtype;                     // 0x0c
};

LTRESULT CSoundInstanceQ47C::ApplyFilter()
{
	uint32                hProvider;
	uint32                nNext;
	uint32                nAttrNext;
	char*                 pName;
	RIBInterfaceEntryQ47C attr;
	int                   nIndex;
	int                   nAttr;
	uint32                i;

	if (m_hSample44 && m_nFilter != -1)
	{
		nIndex = m_nFilter;
		nNext  = 0;

		while (AIL_enumerate_filters(&nNext, &hProvider, &pName))
		{
			if (nIndex == 0)
			{
				AIL_set_sample_processor(m_hSample44, 1, hProvider);

				nAttrNext = 0;
				nAttr     = 0;

				while (AIL_enumerate_filter_sample_attributes(hProvider,
				                                              &nAttrNext,
				                                              &attr))
				{
					for (i = 0; i < 7; i++)
					{
						if (m_FilterParams[i] == -1)
							break;

						if (m_FilterParams[i] == nAttr)
						{
							AIL_set_filter_sample_preference(m_hSample44,
							                                 attr.entry_name,
							                                 &m_FilterValues[i]);
							break;
						}
					}

					nAttr++;
				}

				return LT_OK;
			}

			nIndex--;
		}

		return LT_ERROR;
	}

	return LT_OK;
}


// *********************************************************************** //
//
//   THE CLIENT INTERFACE'S SOUND ACCESSORS (0x00409ac0..0x0040a14f).
//
//   HOME: src/engine/lithtech/clientinterface.cpp.  ci_SetupInterface
//   stores all five into the CLTClient table (slots 0x110, 0x114, 0x118,
//   0x128, 0x12c), and the image places them between ci_GetSoundDuration
//   (0x00409aa0) and ci_IsDone (0x00409ec0), and ci_KillSoundLoop
//   (0x00409f00) and the rest — i.e. in clientinterface.cpp's own source
//   order.  Written here because they reach into this file's views of the
//   sample and the instance (0x50 shadow, 0x64 AILSOUNDINFO, 0x0047cec0).
//
// *********************************************************************** //

#include "ltmatrix.h"
#include "ltrotation.h"

class CClientMgr;
extern CClientMgr* g_pClientMgr;              // 0x004b17e0

extern "C" void dsi_OnReturnError(int code); // 0x0042d760
extern int   g_DebugLevel;                   // 0x004b0ab0
extern char* g_pErrorFormat;                 // 0x004a7110
void dsi_ServerError(char* pFormat, ...);    // 0x0042e6f0

#define LT_ERRSTR_Q8_(x) #x
#define LT_ERRSTR_Q8(x) LT_ERRSTR_Q8_(x)
#define RETURN_ERROR_Q8(level, fnName, code) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, LT_ERRSTR_Q8(code), " "); \
		return code; \
	}

// The client's sound manager as the interface layer sees it: the
// ILTClientSoundMgr vtable (SetListener is slot +0x64) and the listener
// state after it.  Offsets 0x124, 0x72c and 0x750 are clientsound.h's and
// soundinst2.cpp's; 0x738, 0x75c and 0x774 are read off the bodies below.
class CClientSoundMgrQ8
{
public:
	virtual void _v00()=0; virtual void _v04()=0; virtual void _v08()=0;
	virtual void _v0c()=0; virtual void _v10()=0; virtual void _v14()=0;
	virtual void _v18()=0; virtual void _v1c()=0; virtual void _v20()=0;
	virtual void _v24()=0; virtual void _v28()=0; virtual void _v2c()=0;
	virtual void _v30()=0; virtual void _v34()=0; virtual void _v38()=0;
	virtual void _v3c()=0; virtual void _v40()=0; virtual void _v44()=0;
	virtual void _v48()=0; virtual void _v4c()=0; virtual void _v50()=0;
	virtual void _v54()=0; virtual void _v58()=0;
	virtual LTRESULT PauseSounds()=0;                      // +0x5c
	virtual LTRESULT ResumeSounds()=0;                     // +0x60
	virtual LTRESULT SetListener(LTBOOL bListenerInClient, LTVector* pPos,
	                             LTRotation* pRot, LTBOOL bTeleport)=0; // +0x64

	// 0x00480ce0 — clientsound.h's GetSoundInstance.
	CSoundInstanceQ47C* GetSoundInstance(HLTSOUND hSound, LTBOOL bUnknown);

	LTVector* GetLastListenerPos() { return &m_vLastListenerPos; }

public:
	uint8    m_pad_004[0x124 - 0x04];
	void*    m_pSoundSys;                    // 0x124
	uint8    m_pad_128[0x72c - 0x128];
	LTVector m_vListenerPos;                 // 0x72c
	// 0x738 — what SetListener measures the new position against to decide
	// whether the move is a teleport (name ours).
	LTVector m_vLastListenerPos;             // 0x738
	uint8    m_pad_744[0x750 - 0x744];
	LTVector m_vListenerFront;               // 0x750
	LTVector m_vListenerRight;               // 0x75c
	uint8    m_pad_768[0x774 - 0x768];
	LTBOOL   m_bListenerInClient;            // 0x774
};

CClientSoundMgrQ8* ci_GetSoundMgrQ8();       // 0x0040f7f0


// 0x00409ac0, 259 bytes.  The null-argument refusal names
// "GetSoundDuration": the shipped string is that one (0x004a7df8).
LTRESULT ci_GetSoundTimer(HLTSOUND hSound, LTFLOAT* fTimer)
{
	CSoundInstanceQ47C* pInst;

	if (!g_pClientMgr)
		return LT_OK;

	if (!ci_GetSoundMgrQ8()->m_pSoundSys)
		return LT_OK;

	if (!hSound || !fTimer)
		RETURN_ERROR_Q8(1, "GetSoundDuration", LT_INVALIDPARAMS);

	pInst = ci_GetSoundMgrQ8()->GetSoundInstance(hSound, LTTRUE);
	if (pInst)
	{
		if (pInst->m_StateFlags & 2)
		{
			*fTimer = (float)(pInst->m_Unknown2c - pInst->m_nPriority) * 0.001f;
			return LT_OK;
		}

		return LT_FINISHED;
	}

	RETURN_ERROR_Q8(1, "GetSoundTimer", LT_INVALIDPARAMS);
}


// 0x00409bd0, 389 bytes.
LTRESULT ci_GetSoundData(HLTSOUND hSound, int16*& pSixteenBitData,
                         int8*& pEightBitData, uint32* dwSamplesPerSecond,
                         uint32* dwChannels)
{
	CSoundInstanceQ47C* pInst;
	CSoundDataQ47C*     pData;
	AILSOUNDINFOQ47C*   pInfo;

	if (!g_pClientMgr)
		return LT_OK;

	if (!ci_GetSoundMgrQ8()->m_pSoundSys)
		return LT_OK;

	if (!hSound)
		RETURN_ERROR_Q8(1, "GetSoundData", LT_INVALIDPARAMS);

	pInst = ci_GetSoundMgrQ8()->GetSoundInstance(hSound, LTTRUE);
	if (pInst && (pData = pInst->m_pData))
	{
		if ((pData->m_WaveInfo.m_Format.wFormatTag == 0x11 ||
		     pData->m_WaveInfo.m_Format.wFormatTag == 0x55) &&
		    !pData->m_pDecompressed)
		{
			pData->sub_47cec0();
		}

		pInfo = pData->m_pDecompressed ? &pData->m_pDecompressed->m_SoundInfo
		                               : &pData->m_SoundInfo;
		if (pInfo)
		{
			if (dwSamplesPerSecond)
				*dwSamplesPerSecond = pInfo->rate;

			if (dwChannels)
				*dwChannels = pInfo->channels;

			if (pInfo->bits == 8)
			{
				pEightBitData    = (int8*)(pData->m_pDecompressed
				                            ? pData->m_pDecompressed->m_pDataStart
				                            : pData->m_pDataStart);
				pSixteenBitData  = LTNULL;
				return LT_OK;
			}
			else if (pInfo->bits == 16)
			{
				pEightBitData    = LTNULL;
				pSixteenBitData  = (int16*)(pData->m_pDecompressed
				                            ? pData->m_pDecompressed->m_pDataStart
				                            : pData->m_pDataStart);
				return LT_OK;
			}

			pEightBitData   = LTNULL;
			pSixteenBitData = LTNULL;
			return LT_ERROR;
		}
	}

	RETURN_ERROR_Q8(1, "GetSoundData", LT_INVALIDPARAMS);
}


// 0x00409d60, 348 bytes.  The refusal after the instance lookup names
// "GetSoundData" (0x004a7e1c), not this function.
LTRESULT ci_GetSoundOffset(HLTSOUND hSound, uint32* dwOffset, uint32* dwSize)
{
	CSoundInstanceQ47C* pInst;
	CSoundDataQ47C*     pData;
	AILSOUNDINFOQ47C*   pInfo;

	if (!g_pClientMgr)
		return LT_OK;

	if (!ci_GetSoundMgrQ8()->m_pSoundSys)
		return LT_OK;

	if (!hSound)
		RETURN_ERROR_Q8(1, "GetSoundOffset", LT_INVALIDPARAMS);

	pInst = ci_GetSoundMgrQ8()->GetSoundInstance(hSound, LTTRUE);
	if (pInst && (pData = pInst->m_pData))
	{
		if ((pData->m_WaveInfo.m_Format.wFormatTag == 0x11 ||
		     pData->m_WaveInfo.m_Format.wFormatTag == 0x55) &&
		    !pData->m_pDecompressed)
		{
			pData->sub_47cec0();
		}

		if (pInst->m_StateFlags & 2)
		{
			pInfo = pData->m_pDecompressed
			        ? &pData->m_pDecompressed->m_SoundInfo
			        : &pData->m_SoundInfo;
			if (pInfo)
			{
				if (dwOffset)
					*dwOffset = (pInst->m_Unknown2c - pInst->m_nPriority) *
					            pInfo->channels * pInfo->rate / 1000;

				if (dwSize)
				{
					if (pInfo->bits)
					{
						*dwSize = (pData->m_pDecompressed
						           ? pData->m_pDecompressed->m_WaveInfo.m_nDataSize
						           : pData->m_WaveInfo.m_nDataSize) /
						          pInfo->bits * 8;
					}
					else
					{
						return LT_ERROR;
					}
				}

				return LT_OK;
			}
		}

		return LT_FINISHED;
	}

	RETURN_ERROR_Q8(1, "GetSoundData", LT_INVALIDPARAMS);
}


// 0x00409f20, 218 bytes.  A move of more than 1024 units (the square,
// 1048576.0f at 0x004a1a60) is a teleport.  VEC_DISTSQR is the macro:
// ci_GetSoundMgr is called once per operand, six times, and the pointer
// form of the operand (`call; add $0x738`, kept in ebx/ebp/edi) is what
// the in-class GetLastListenerPos gives.  The distance is NAMED in a float
// local before the compare: that is what makes VC6 read pPos->x into
// a spare slot BEFORE the sixth call (0x00409f7e, frame 0xc).  With the
// macro inside the `if` it was 210 bytes, 6 blocks; so were a
// `C < dist` compare and a folded `pPos && ...` guard.  Earlier negatives:
// swapped operands, a reference accessor, a `LTVector& vPos = *pPos`
// operand (8 blocks), the macro written out with y+z grouped (8).
void ci_SetListener(LTBOOL bListenerInClient, LTVector* pPos, LTRotation* pRot)
{
	LTBOOL bTeleport = LTFALSE;

	if (pPos)
	{
		float fDistSqr = VEC_DISTSQR(*pPos,
		                             *ci_GetSoundMgrQ8()->GetLastListenerPos());

		if (fDistSqr > 1024.0f * 1024.0f)                  // 0x00409fc4
		{
			bTeleport = LTTRUE;
		}
	}

	ci_GetSoundMgrQ8()->SetListener(bListenerInClient, pPos, pRot, bTeleport);
}


// 0x0040a000, 336 bytes.  The rotation is rebuilt from the front and right
// vectors: up = vRight.Cross(vForward) (the SDK's by-value _CVector::Cross;
// VEC_CROSS gives the same numbers but schedules the fsubrp two stores
// early), then the basis matrix, then the quaternion (0x004449b0).
void ci_GetListener(LTBOOL* bListenerInClient, LTVector* pPos, LTRotation* pRot)
{
	if (bListenerInClient)
		*bListenerInClient = ci_GetSoundMgrQ8()->m_bListenerInClient;

	if (pPos)
		*pPos = ci_GetSoundMgrQ8()->m_vListenerPos;

	if (pRot)
	{
		LTVector vUp, vForward, vRight;
		LTMatrix mat;

		vForward = ci_GetSoundMgrQ8()->m_vListenerFront;
		vRight   = ci_GetSoundMgrQ8()->m_vListenerRight;
		vUp = vRight.Cross(vForward);
		mat.SetBasisVectors(&vRight, &vUp, &vForward);
		pRot->ConvertFromMatrix(mat);
	}
}
