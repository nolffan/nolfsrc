// *********************************************************************** //
//
//  MODULE:   INPUT (.CPP)   -- lithtech.exe only
//
//  PURPOSE:  The client's DirectInput device layer, 0x00436860..0x00439fbc.
//
//  WHAT IDENTIFIES THE SUBSYSTEM
//
//  Every function below is reached from one 24-slot table of plain function
//  pointers at 0x004ab008 -- not a vtable: the call sites are
//  `call *0x34(%eax)` with no vtable load and the CALLER cleans the stack,
//  so the entries are cdecl.  `src/engine/lithtech/clientmgr.h` already
//  declares that table as `CClientInput` (it hangs off CClientMgr+0x1508)
//  and names ten of its slots from their call sites; those names are used
//  here unchanged, and the ones clientmgr.h leaves blank are named from
//  what the bytes do.  The whole slot map:
//
//     +0x00 0x00436b40  Init                    <- below
//     +0x04 0x00436940  Term                    <- below
//     +0x08 0x00436c70  IsInitialized           <- below
//     +0x0c 0x00436c80  ListDevices             <- below
//     +0x10 0x0048fab0  PlayJoystickEffect      <- below (a folded return 0)
//     +0x14 0x00437160  ReadActions             <- below (near)
//     +0x18 0x00437b70  FlushDeviceData         <- below
//     +0x1c 0x00437bc0  ReadInput               <- below
//     +0x20 0x00437f50  AddAction               <- below
//     +0x24 0x00436e80  EnableDevice            <- below
//     +0x28 0x00438000  ClearBindings           <- below
//     +0x2c 0x004381a0  AddTrigger              <- below
//     +0x30 0x00438bb0  ScaleTrigger            <- below
//     +0x34 0x00439090  GetDeviceBindings       <- below
//     +0x38 0x00439050  FreeDeviceBindings      <- below
//     +0x3c 0x00439270  StartDeviceTrack        <- below
//     +0x40 0x004395a0  TrackDevice             <- below
//     +0x44 0x00439a30  EndDeviceTrack          <- below
//     +0x48 0x00439ab0  GetDeviceObjects        <- below (near)
//     +0x4c 0x00439e20  FreeDeviceObjects       <- below
//     +0x50 0x00439e40  GetDeviceName           <- below
//     +0x54 0x00439ea0  IsDeviceEnabled         <- below
//     +0x58 0x00439ec0  ListDeviceObjects       <- below
//     +0x5c 0x00439f80  ListDeviceNames         <- below
//
//  That it is DirectInput and not something engine-grown is settled by the
//  constants, all checked against toolchains/msvc-6.00-8447/INCLUDE/dinput.h:
//  0x004a3a4c/0x004a3a5c are GUID_SysKeyboard/GUID_SysMouse, 0x004a3b1c is
//  IID_IDirectInputDevice2A, 0x004a3914 is c_dfDIJoystick (dwSize 24,
//  dwObjSize 16, dwFlags 1, dwDataSize 0x50, dwNumObjs 0x2c), and the ten
//  GUIDs the type-name mapper compares against are GUID_XAxis .. GUID_POV
//  in that order.  Init calls DirectInputCreateA with 0x700 and falls back
//  to 0x300.
//
//  The vtable slot offsets used through the interfaces all agree with
//  dinput.h's declaration order: IDirectInput 0x0c CreateDevice / 0x10
//  EnumDevices; IDirectInputDevice2 0x08 Release / 0x10 EnumObjects /
//  0x1c Acquire / 0x20 Unacquire / 0x28 GetDeviceData / 0x2c SetDataFormat
//  / 0x34 SetCooperativeLevel.  `this` is PUSHED, never in ecx, which is
//  what MSVC does for `virtual __stdcall` -- so the interface views below
//  are declared STDMETHOD-style and not thiscall.
//
//  NOT WRITTEN, ALREADY SOURCED ELSEWHERE:
//    0x00436170 is ltmatrix.h's SetBasisVectors, out of line; 0x004361e0/
//    0x004362c0 are NOT CPacket::WriteType/ReadType of their own but the
//    one-byte yaw writer/reader that inline them -- written at the foot of
//    this file, see there for their real home; 0x00436370/0x004363f0/
//    0x00436490 push "GetNextModelNode" and belong to
//    src/engine/lithtech/clientinterface.cpp; 0x00436760/0x00436790/
//    0x004367b0/0x00436800 are _CVector<float>::Mag/Dot/Norm/Cross from
//    src/LT2/sdk/inc/ltvector.h.
//
// *********************************************************************** //

#include <windows.h>
#include <dinput.h>
#include <string.h>
#include <stdio.h>

#include "ltbasedefs.h"
#include "de_error.h"
#include "packet.h"

#ifdef NOLFSRC_MODERN
#include "..\..\modern\engine\p7_gamepad.h"   // M-P7-01: ##gamepad
#include "..\..\modern\engine\p1_mousebutton.h" // M-P1-33: ##button<N>
#endif

#include <math.h>

// windows.h renames engine methods out from under the SDK headers.
#undef PostMessage
#undef GetMessage
#undef PeekMessage
#undef CopyFile
#undef PlaySound
#undef GetClassName


// ======================================================================= //
//  WHAT THIS FILE REACHES OUTSIDE ITSELF
// ======================================================================= //

// 0x0042e6f0 -- the client's console printf (clientconsole.cpp names it).
void con_ConsolePrint(char* pFormat, ...);

// 0x004b0b04 -- the main window, read through the one-instruction getter at
// 0x0042e740.  clientmgr.h/clientdsi.cpp already declare the global; the
// getter is what the code below actually calls, because Init's store is
// `call 0x0042e740; mov %eax,0x004b367c`.
HWND dsi_GetMainWindow();                     // 0x0042e740
HINSTANCE dsi_GetInstanceHandle();            // 0x0042e730

// 0x00490cd0 -- case-insensitive string EQUALITY: it returns 1 when the two
// strings match and 0 when they do not (not a three-way compare), folding
// case through a 256-byte table at 0x004b4d88.  It is not _stricmp, which
// this file also calls at 0x00492f30, so it keeps its address for a name.
int sub_490cd0(const char* pStr1, const char* pStr2);

// 0x00437bf0 (864 bytes) -- rebuild a device's data format and read its
// first state; defined below.  Called from ReadInput for every created
// device, from EndDeviceTrack when the device has no track buffer, and by
// ClearBindings, which returns its result.  It RETURNS a flag (eax is 0 or 1
// on every exit): input_AddTrigger (0x004381a0) tests it at 0x004383aa.
LTBOOL di_SetupDeviceFormat(struct InputDevice* pDevice);

// 0x00439bf0 (558 bytes, defined below GetDeviceObjects) -- the
// DIEnumDeviceObjects callback that appends a
// DeviceObject to the list ListDeviceObjects/GetDeviceObjects build.  Its
// pvRef is a `DeviceObject**` cursor, one level of indirection deeper than
// the list head (see ListDeviceObjects below), and it reads the device it is
// working on out of the global at 0x004b368c.
BOOL PASCAL di_BuildDeviceObjectCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi,
                                         LPVOID pvRef);


// ======================================================================= //
//  THE DEVICE RECORD
//
//  0x304 bytes, proven by the `operator new` argument at 0x00436bb7 and by
//  the `rep stos` of 0xc1 dwords that follows it.  Every offset below is a
//  store or a load in one of the functions in this file:
//
//    +0x000  the created device            EnableDevice's QueryInterface out
//    +0x004  the alias name               di_FindDevice's three stores
//    +0x00c  the DirectInput name         strncpy target, 100 bytes
//    +0x070  guidInstance                 16-byte copy out of DIDEVICEINSTANCE
//    +0x080  dwDevType                    LOBYTE compared, HID bit tested
//    +0x084  embedded object list         vptr + count + head
//    +0x2e8  tracking flag                cleared by EndDeviceTrack
//    +0x2ec  track buffer                 EndDeviceTrack's arm selector
//    +0x2f0  track data                   `delete`d by EndDeviceTrack
//    +0x2f8  GetTickCount at construction
//    +0x2fc  "is the mouse"               set by EnableDevice on "##mouse"
//    +0x300  next
//
//  The name field's length is not directly proven -- 100 is the strncpy
//  count at 0x00436c16, and the field cannot run past the GUID at 0x070, so
//  0x64 is both the count and the largest it can be.
// ======================================================================= //

// The object list embedded at InputDevice+0x84.  It has ONE virtual (slot 0
// of the vtable at 0x004a23e4 is 0x00439fc0) and Term's teardown restores
// that vptr before freeing, which is how the vptr is known to be at +0x00 of
// the sub-object and not a data pointer.  Its head is walked by
// di_FindDeviceObject below: nodes carry `next` at +0x04 and two names, at
// +0x08 and +0x6c.
//
// The virtual is declared and deliberately not defined here: the class only
// has to be layout- and vptr-store-identical, and 0x00439fc0 hands back a
// two-word iterator whose second word comes out of an uninitialised local,
// which is not something to reconstruct on the strength of one call site.
class CDeviceObject;

class CDeviceObjectList
{
public:
	CDeviceObjectList();

	virtual void GetHead(void* pIterOut);    // 0x00439fc0

	// goodlinklist.h's ~CGLinkedList() { RemoveAll(); } -- the vptr store
	// and the second head/count clear at 0x00436a18..0x00436a28 in Term.
	~CDeviceObjectList()
	{
		RemoveAll();
	}

	// goodlinklist.h's GetNext: hand back the node, step the position, and
	// drop it to null when it comes back round to the head.
	CDeviceObject* GetNext(CDeviceObject*& pos) const;

	// goodlinklist.h's RemoveAt: unlink, step the head off the node, and
	// drop the head when the count reaches zero (0x0043804b..0x0043807c).
	void RemoveAt(CDeviceObject* pos);

	// goodlinklist.h's RemoveAll: head first, count second.
	void RemoveAll()
	{
		m_pHead  = LTNULL;
		m_nCount = 0;
	}

	uint32         m_nCount;                 // +0x04
	CDeviceObject* m_pHead;                  // +0x08
};

// A constructor called from one place is inlined and emits no symbol: the
// shipped image has no out-of-line body for this, only the three stores
// inlined at the top of di_CreateDeviceCallback, in this order.
inline CDeviceObjectList::CDeviceObjectList()
{
	m_nCount = 0;
	m_pHead  = LTNULL;
}


struct InputDevice
{
	InputDevice();
	~InputDevice();

	LPDIRECTINPUTDEVICE2A m_pDevice;         // 0x000
	char*                 m_pAliasName;      // 0x004
	uint32                m_dwDataSize;      // 0x008  di_SetupDeviceFormat
	char                  m_Name[0x64];      // 0x00c
	GUID                  m_Guid;            // 0x070
	uint32                m_dwDevType;       // 0x080
	CDeviceObjectList     m_Objects;         // 0x084
	// Each state dword's object, by dword offset (di_SetupDeviceFormat's
	// 0x96-dword clear at 0x00437c09 and the `shr $2` store at 0x00437ce2).
	CDeviceObject*        m_ObjectMap[150];  // 0x090
	uint32                m_bTracking;       // 0x2e8
	// Set by StartDeviceTrack (0x0043931c) when IT had to enable the device,
	// so that EndDeviceTrack releases it again.
	uint32                m_bTrackEnabled;   // 0x2ec
	// `delete`d with the ONE-argument operator delete at 0x004290c0, not the
	// array form, so it is a single object and not a block; nothing here says
	// what kind, hence uint8*.
	uint8*                m_pTrackData;      // 0x2f0
	uint32                m_nTrackObjects;   // 0x2f4  (0x0043948e)
	uint32                m_dwCreateTime;    // 0x2f8
	uint32                m_bIsMouse;        // 0x2fc
	InputDevice*          m_pNext;           // 0x300
};

// The member's constructor runs before the body, which is exactly the store
// order the shipped bytes have: 0x084, 0x088, 0x08c, then 0x000, 0x2e8,
// 0x2ec, 0x2f8, 0x2fc, 0x2f0, 0x2f4.  Note that 0x2f0/0x2f4 come AFTER
// 0x2fc: that is the shipped order and not a slip.
inline InputDevice::InputDevice()
{
	m_pDevice      = LTNULL;
	m_bTracking    = 0;
	m_bTrackEnabled = 0;
	m_dwCreateTime = ::GetTickCount();
	m_bIsMouse     = 0;
	m_pTrackData   = LTNULL;
	m_nTrackObjects = 0;
}


// One entry of a device's object list.  Only what di_FindDeviceObject and
// Term reach is named: `next` at +0x04 and the two names it matches against,
// 0x64 bytes apart, which is INPUTNAME_LEN.  The list is CIRCULAR -- the
// walk stops when the next pointer comes back to the head, which is what the
// `neg / sbb / and` triple at 0x0043816a does -- so the head is a real node.
// A binding record: twelve bytes, no vptr, on a singly linked list whose
// head is 0x004b3678 (warpblit.cpp PART E, which carries the same record as
// CBindRecQ438, has the evidence).  Its destructor unlinks it; `delete` of
// one is the scalar-deleting destructor 0x00436ae0, which Term CALLS
// (`push $1 ; call` at 0x004369dd) where the shallower 0x004368c0 and
// 0x00438000 expand it.
struct CInputBindRec;

extern CInputBindRec* g_pBindRecHead;       // 0x004b3678

struct CInputBindRec
{
	// The two zero stores after the null-guarded `new` in input_AddTrigger
	// (0x00438281 / 0x00438284).
	CInputBindRec()
	{
		m_n00 = 0;
		m_n04 = 0;
	}

	~CInputBindRec()
	{
		CInputBindRec* pPrev;

		m_n00 = 0;
		m_n04 = 0;

		if (g_pBindRecHead == this)
		{
			g_pBindRecHead = m_pNext;
		}
		else
		{
			pPrev = g_pBindRecHead;

			// The found arm ENDS THE LOOP BY NULLING pPrev, not by
			// `break`: with `break` (or return, goto, for(;;), an else
			// arm, a do/while) VC6 rotates the test to `jne top ; jmp
			// exit`; the null store is folded away and leaves shipped's
			// `je exit ; jmp top` (0x00436b12).  warpblit.cpp's
			// CBindRecQ438 carries the same fix.
			while (pPrev)
			{
				if (pPrev->m_pNext == this)
				{
					pPrev->m_pNext = m_pNext;
					pPrev = LTNULL;
				}
				else
				{
					pPrev = pPrev->m_pNext;
				}
			}
		}
	}

	uint32         m_n00;                    // 0x00
	uint32         m_n04;                    // 0x04
	CInputBindRec* m_pNext;                  // 0x08
};

// One binding of a device object: 0x18 bytes (`push $0x18` at 0x004382a0),
// on a list hanging off the object at +0x138.  Only the three fields the
// teardown reaches are named.  The destructor is what Term's inner loop
// runs after its null test at 0x004369c2: the block first, then the record.
struct SInputBinding
{
	// The four zero stores after the null-guarded `new` in input_AddTrigger
	// (0x004382ae..0x004382b7): everything but the range.
	SInputBinding()
	{
		m_pData   = LTNULL;
		m_pAction = LTNULL;
		m_pNext   = LTNULL;
		m_pBind   = LTNULL;
	}

	~SInputBinding()
	{
		// Explicit guard: `delete` of a uint8* emits no null test.
		if (m_pData)
			delete m_pData;                  // 0x004369cc

		if (m_pBind)
		{
			delete m_pBind;                  // 0x004369dd, ??_G
			m_pBind = LTNULL;
		}
	}

	// The range and the action GetDeviceBindings (0x00439090) copies into
	// each GameAction: nRangeLow/nRangeHigh from +0x00/+0x04 (0x004391ec /
	// 0x004391f5), nActionCode and the name from the action at +0x0c.
	float          m_fRangeLow;              // 0x00
	float          m_fRangeHigh;             // 0x04
	uint8*         m_pData;                  // 0x08
	class CInputAction* m_pAction;           // 0x0c
	SInputBinding* m_pNext;                  // 0x10
	CInputBindRec* m_pBind;                  // 0x14
};

class CDeviceObject
{
public:
	// The binding list, freed node by node (0x004369b3..0x004369f2).
	~CDeviceObject()
	{
		SInputBinding* pBinding;
		SInputBinding* pNext;

		pBinding = m_pBindings;

		while (pBinding)
		{
			pNext = pBinding->m_pNext;

			delete pBinding;

			pBinding = pNext;
		}
	}

	CDeviceObject* m_pPrev;                  // 0x000  (RemoveAt, 0x0043804b)
	CDeviceObject* m_pNext;                  // 0x004
	char           m_Name[0x64];             // 0x008
	char           m_TriggerName[0x64];      // 0x06c
	// The data-format entry di_SetupDeviceFormat (0x00437bf0) builds from
	// the object: dwType and the GUID pointer (0x00437cb4 / 0x00437cc3).
	uint32         m_dwType;                 // 0x0d0
	GUID           m_guidType;               // 0x0d4
	// 3 and 2 are the axis kinds: only they are range-centred and scaled
	// (0x00437df9, 0x00437ee2).
	uint32         m_Type;                   // 0x0e4
	// The object's slot in the device state -- a dword index; objects
	// that share a name share the slot (0x00437d69).
	uint32         m_nIndex;                 // 0x0e8
	uint32         m_dwValue;                // 0x0ec
	uint32         m_Unknown0f0;             // 0x0f0
	uint32         m_Unknown0f4;             // 0x0f4
	int32          m_nValue;                 // 0x0f8
	uint32         m_Unknown0fc;             // 0x0fc
	uint32         m_Unknown100;             // 0x100
	// The per-millisecond rate slot 5 (0x00437160) spreads an axis's
	// buffered movement over, in value units (x10000) per tick.
	float          m_fRate;                  // 0x104
	// The scale and range-scale state input_ScaleTrigger (0x00438bb0) sets
	// and GetDeviceBindings copies out (0x00439160..0x0043918a).  The
	// range maps the object's own range [m_fRangeMin, m_fRangeMax] onto
	// [m_fRangeScaleMin, m_fRangeScaleMax]; with a pre-centre offset it is
	// two linear pieces meeting at m_fCenter.
	float          m_fScale;                 // 0x108
	float          m_fRangeScaleMin;         // 0x10c
	float          m_fRangeScaleMax;         // 0x110
	float          m_fRangeScaleMult;        // 0x114
	float          m_fRangeScaleOffset;      // 0x118
	float          m_fLowMult;               // 0x11c
	float          m_fLowOffset;             // 0x120
	float          m_fHighMult;              // 0x124
	float          m_fHighOffset;            // 0x128
	float          m_fRangeScalePreCenterOffset;  // 0x12c
	float          m_fCenter;                // 0x130
	uint32         m_Unknown134;             // 0x134
	SInputBinding* m_pBindings;              // 0x138
	float          m_fRangeMin;              // 0x13c
	float          m_fRangeMax;              // 0x140
};


// goodlinklist.h's GDeleteAndRemoveElements over the object list, and then
// the member's own destructor.  The walk is GetNext's: the successor is
// dropped to null when it comes back round to the head, re-read from the
// list on every pass (`mov 0x8c(%ebx),%esi` at 0x00436997) -- the same
// `neg / sbb / and` di_FindDeviceObject has.
inline CDeviceObject* CDeviceObjectList::GetNext(CDeviceObject*& pos) const
{
	CDeviceObject* ret = pos;

	if (pos->m_pNext == m_pHead)
		pos = LTNULL;
	else
		pos = pos->m_pNext;

	return ret;
}

inline void CDeviceObjectList::RemoveAt(CDeviceObject* pos)
{
	pos->m_pPrev->m_pNext = pos->m_pNext;
	pos->m_pNext->m_pPrev = pos->m_pPrev;

	if (pos == m_pHead)
		m_pHead = m_pHead->m_pNext;

	--m_nCount;
	if (m_nCount == 0)
		m_pHead = LTNULL;
}

inline InputDevice::~InputDevice()
{
	CDeviceObject* pos;

	for (pos = m_Objects.m_pHead; pos != LTNULL; )
		delete m_Objects.GetNext(pos);

	m_Objects.RemoveAll();
}


// ======================================================================= //
//  THE GLOBALS, all in .bss (0x004b3674..0x004b368f, zero in the shipped
//  image), defined here in address order: this file owns the run.
// ======================================================================= //

struct SInputEffect;

// 0x004b3674 -- whatever Init's second argument is; Term clears it and
// slot 5 (0x00437160) hands it to the console calls as their ConsoleState.
void*          g_pInputClient;               // 0x004b3674

// 0x004b3678 -- the binding list CInputBindRec's destructor unlinks from.
CInputBindRec* g_pBindRecHead;               // 0x004b3678

// 0x004b367c -- the window SetCooperativeLevel is given.  Init stores
// dsi_GetMainWindow()'s result into it.
HWND           g_hInputWnd;                  // 0x004b367c

LPDIRECTINPUTA g_pDirectInput;               // 0x004b3680
InputDevice*   g_pDeviceHead;                // 0x004b3684

// 0x004b3688 -- the force-feedback effect list Term releases.
SInputEffect*  g_pEffectHead;                // 0x004b3688

// 0x004b368c -- the device di_BuildDeviceObjectCallback is enumerating; set
// around the EnumObjects call and cleared after it.
InputDevice*   g_pEnumDevice;                // 0x004b368c


// The interface table itself, 0x004ab008 -- the run of 24 pointers into
// .text, with the caller cleaning the stack at
// every call site (so cdecl, not a vtable).  Declared here for
// input_GetInputMgr and DEFINED, with its 24 shipped words, after it at the
// foot of the input code.  The slots are spelled out because the offsets are
// how clientmgr.h's CClientInput names line up with the addresses; see the
// map in this file's header comment.
struct InputMgr
{
	void* Init;                              // 0x00  0x00436b40
	void* Term;                              // 0x04  0x00436940
	void* IsInitialized;                     // 0x08  0x00436c70
	void* ListDevices;                       // 0x0c  0x00436c80
	void* PlayJoystickEffect;                // 0x10  0x0048fab0
	void* ReadActions;                       // 0x14  0x00437160
	void* FlushDeviceData;                   // 0x18  0x00437b70
	void* ReadInput;                         // 0x1c  0x00437bc0
	void* AddAction;                         // 0x20  0x00437f50
	void* EnableDevice;                      // 0x24  0x00436e80
	void* ClearBindings;                     // 0x28  0x00438000
	void* AddTrigger;                        // 0x2c  0x004381a0
	void* ScaleTrigger;                      // 0x30  0x00438bb0
	void* GetDeviceBindings;                 // 0x34  0x00439090
	void* FreeDeviceBindings;                // 0x38  0x00439050
	void* StartDeviceTrack;                  // 0x3c  0x00439270
	void* TrackDevice;                       // 0x40  0x004395a0
	void* EndDeviceTrack;                    // 0x44  0x00439a30
	void* GetDeviceObjects;                  // 0x48  0x00439ab0
	void* FreeDeviceObjects;                 // 0x4c  0x00439e20
	void* GetDeviceName;                     // 0x50  0x00439e40
	void* IsDeviceEnabled;                   // 0x54  0x00439ea0
	void* ListDeviceObjects;                 // 0x58  0x00439ec0
	void* ListDeviceNames;                   // 0x5c  0x00439f80
};

extern InputMgr g_InputMgr;                  // 0x004ab008


// ======================================================================= //
//  DEVICE LOOKUP
// ======================================================================= //

// 0x00437100 -- the first device of a given DirectInput type, preferring a
// non-HID one.  TWO passes over the same list: the first accepts a device
// only when DIDEVTYPE_HID is clear, the second only when it is set.  Two
// things say it is written as two loops and not one: the list head is loaded
// ONCE (`mov 0x004b3684,%ecx`, then both loops start from ecx), and it is a
// NAMED local: both guards test ecx itself (`test %ecx,%ecx ; mov %ecx,%eax`
// at 0x0043710c and 0x00437135), where `pDevice = g_pDeviceHead` twice
// copies first and tests eax (5 blocks).  And 0x10000 is hoisted into edi instead of
// being an immediate, which is what VC6 does with a constant used twice.
//
// GET_DIDEVICE_TYPE against a uint32 needs a zero-extend, and that is the
// `xor %edx,%edx; mov 0x80(%eax),%dl` -- compare with EnableDevice below,
// where the same field against the literal 4 narrows to a byte compare.
InputDevice* di_FindDeviceByType(uint32 nDevType)
{
	InputDevice* pHead;
	InputDevice* pDevice;

	pHead = g_pDeviceHead;

	for (pDevice = pHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == nDevType &&
		    !(pDevice->m_dwDevType & DIDEVTYPE_HID))
		{
			return pDevice;
		}
	}

	for (pDevice = pHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == nDevType &&
		    (pDevice->m_dwDevType & DIDEVTYPE_HID))
		{
			return pDevice;
		}
	}

	return LTNULL;
}


// 0x00439bb0 -- the nIndex'th device of a given type, this time without the
// HID split.  The count is spent DOWN and the test is `<= 0`, not `== 0`
// (`test %ecx,%ecx; jle`), so a negative index returns the first match.
InputDevice* di_FindDeviceByIndex(uint32 nDevType, int nIndex)
{
	InputDevice* pDevice;

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == nDevType)
		{
			if (nIndex <= 0)
				return pDevice;

			nIndex--;
		}
	}

	return LTNULL;
}


// 0x00437040 -- a device by name, with three aliases.  The list is searched
// on the DirectInput name first; failing that, the three well-known names
// resolve by TYPE and the record is tagged with the alias that found it.
// "Joystick 1" is matched on its first eight characters only -- strnicmp
// with a count of 8 -- so "Joystick 2" finds the same device.
//
// The shared exit is the tell for each alias arm ending in an unconditional
// `return pDevice` (the tag guarded by `if (pDevice)`) and ONE trailing
// `return LTNULL`: the three null arms jump to the bare `pop/pop/ret` at
// 0x004370f2 because eax is already the null pDevice, and only the
// strnicmp-failed path pays for the `xor %eax,%eax` at 0x004370f0.  With
// `return pDevice` inside the guard the null arms fell through to the
// `return LTNULL` and paid the xor too (2 blocks).
InputDevice* di_FindDevice(char* pName)
{
	InputDevice* pDevice;

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (sub_490cd0(pName, pDevice->m_Name))
			return pDevice;
	}

	if (stricmp(pName, "##mouse") == 0)
	{
		pDevice = di_FindDeviceByType(DIDEVTYPE_MOUSE);

		if (pDevice)
			pDevice->m_pAliasName = "##mouse";

		return pDevice;
	}
	else if (stricmp(pName, "##keyboard") == 0)
	{
		pDevice = di_FindDeviceByType(DIDEVTYPE_KEYBOARD);

		if (pDevice)
			pDevice->m_pAliasName = "##keyboard";

		return pDevice;
	}
	else if (strnicmp(pName, "Joystick 1", 8) == 0)
	{
		pDevice = di_FindDeviceByType(DIDEVTYPE_JOYSTICK);

		if (pDevice)
			pDevice->m_pAliasName = "Joystick 1";

		return pDevice;
	}

	return LTNULL;
}


// 0x00438140 -- one object on one device, by either of its two names.  The
// walk is the list's own GetNext (the `neg / sbb / and` at 0x0043816a, the
// same triple Term's teardown has), and the head is RE-READ from the device
// on every iteration (`mov 0x8c(%ebp),%edx` at 0x00438156) because the two
// stricmp calls could in principle write it.  An open-coded
// `if (pNext == head) pNext = 0` branches instead (91 bytes, 6 blocks).
CDeviceObject* di_FindDeviceObject(InputDevice* pDevice, char* pName)
{
	CDeviceObject* pos;
	CDeviceObject* pCur;

	pos = pDevice->m_Objects.m_pHead;

	while (pos)
	{
		pCur = pDevice->m_Objects.GetNext(pos);

		if (stricmp(pCur->m_Name, pName) == 0)
			return pCur;

		if (stricmp(pCur->m_TriggerName, pName) == 0)
			return pCur;
	}

#ifdef NOLFSRC_MODERN
	// M-P1-33: a mouse button by any of its names -- the fixed one
	// ("##button<N>"), DirectInput's, or retail's "Button <N>" -- is the
	// object of that button, which has the fixed name (input_AddTrigger).
	if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_MOUSE)
	{
		char szFixed[16];
		int  nButton;

		nButton = P1Mouse_ButtonNumber(pDevice->m_pDevice, pName, LTNULL, 0);

		if (nButton >= 0)
		{
			P1Mouse_FixedName(nButton, szFixed);

			pos = pDevice->m_Objects.m_pHead;

			while (pos)
			{
				pCur = pDevice->m_Objects.GetNext(pos);

				if (stricmp(pCur->m_TriggerName, szFixed) == 0)
					return pCur;
			}
		}
	}
#endif

	return LTNULL;
}


// ======================================================================= //
//  THE ACTION LIST
//
//  A circular doubly-linked list whose head node is the static record at
//  0x004b34f8; 0x00436870 self-links it (`m_pPrev = m_pNext = this`, next
//  stored first) and the walk below stops when it comes back to it.  The
//  name is an inline array at +0x04 -- its address is what gets pushed --
//  and the two links are at +0x24 and +0x28, which fixes the array at 0x20
//  bytes.  What sits at +0x00 is never read here.
// ======================================================================= //

class CInputAction
{
public:
	// Self-linking, next first: the null-guarded inline construction at
	// 0x00437f83 in input_AddAction stores +0x28 then +0x24, the same pair
	// 0x00436870 stores into the static head.
	CInputAction()
	{
		m_pNext = this;
		m_pPrev = this;
	}

	// The action code AddAction stores; below 256 (`cmp $0x100` at
	// 0x00437f55, signed `jge`).
	int32         m_nCode;                   // 0x00
	char          m_Name[0x20];              // 0x04
	CInputAction* m_pPrev;                   // 0x24
	CInputAction* m_pNext;                   // 0x28
};

// 0x004b34f8.  Defined here, not extern: its constructor is the whole of
// the compiler-generated initialiser at 0x00436870 (the self-link, the
// address materialised once and stored to +0x28 then +0x24), reached by
// the tail-jump at 0x00436860 -- the first two functions of this module.
CInputAction g_ActionList;


// 0x00437fc0 -- an action by name.  The head sentinel's address is an
// immediate in the loop's exit test, which is what says the list head is a
// static object and not a pointer.
CInputAction* di_FindAction(char* pName)
{
	CInputAction* pAction;

	for (pAction = g_ActionList.m_pNext;
	     pAction != &g_ActionList;
	     pAction = pAction->m_pNext)
	{
		if (sub_490cd0(pName, pAction->m_Name))
			return pAction;
	}

	return LTNULL;
}


// 0x00437f50 -- slot 8.  Bind an action name to a code: an existing action
// just takes the new code, an unknown one is allocated (0x2c bytes, `push
// $0x2c` at 0x00437f75), linked in at the TAIL of the circular list -- the
// head's m_pPrev is read at 0x00437f8f and the new node goes between it and
// the head -- and named with a 30-character strncpy (`push $0x1e`).  Codes
// of 256 and up are refused without a word.  The first argument is the
// table's receiver and is never read.
void input_AddAction(InputMgr* pMgr, char* pActionName, int nActionCode)
{
	CInputAction* pAction;

	if (nActionCode >= 256)
		return;

	pAction = di_FindAction(pActionName);

	if (pAction)
	{
		pAction->m_nCode = nActionCode;

		return;
	}

	pAction = new CInputAction;

	pAction->m_pPrev = g_ActionList.m_pPrev;
	pAction->m_pNext = &g_ActionList;
	pAction->m_pPrev->m_pNext = pAction->m_pNext->m_pPrev = pAction;

	strncpy(pAction->m_Name, pActionName, 30);

	pAction->m_nCode = nActionCode;
}


// 0x004b0ab0 -- the engine's debug level (binkvideo.cpp / clientrender.cpp).
extern int g_DebugLevel;                     // 0x004b0ab0

// 0x00438470 -- diblit.cpp's di_AddDeviceObjectQ438: the create half of
// "find or create" for a device object.
CDeviceObject* di_AddDeviceObject(InputDevice* pDevice, char* pName);

#ifdef NOLFSRC_MODERN
// M-P1-33: the create half for the modern build.  A mouse button is created
// by its DirectInput name, whatever name the binding gave, and takes its
// fixed name ("##button<N>") as the name the config save writes.  A fixed
// name of a button that the mouse does not have creates nothing: the
// retail "##" parse would read "##button2" as instance 0 (atoi of
// "button2"), which is the X axis.
CDeviceObject* di_AddDeviceObjectP1(InputDevice* pDevice, char* pName)
{
	char           szReal[MAX_PATH];
	CDeviceObject* pObj;
	int            nButton;

	if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) != DIDEVTYPE_MOUSE)
		return di_AddDeviceObject(pDevice, pName);

	nButton = P1Mouse_ButtonNumber(pDevice->m_pDevice, pName, szReal, sizeof(szReal));

	if (nButton < 0)
	{
		if (P1Mouse_IsFixedName(pName))
			return LTNULL;

		return di_AddDeviceObject(pDevice, pName);
	}

	pObj = di_AddDeviceObject(pDevice, szReal);

	if (pObj)
		P1Mouse_FixedName(nButton, pObj->m_TriggerName);

	return pObj;
}
#endif

// 0x004381a0 -- slot 11 (+0x2c), the trigger binder: hang a binding of
// pActionName, over [rangeLow, rangeHigh], off the named object of the named
// device, and rebuild the device's state; "Bound %s (on %s) to action %s"
// at debug level 11 and up.  ONE function (the enumerator's "second start"
// at 0x00438206 is the target of the device's null-m_pDevice refusal).
//
// Read off the bytes:
//   * the range is put in order first (`fcomps; test $0x41,%ah; jne` and a
//     swap through eax at 0x004381bc..0x004381c8);
//   * a _stricmp against "Joystick 1" whose result nothing reads
//     (0x004381d6: its argument pushes are cleaned with di_FindDevice's);
//   * the object is found (0x00438140), else created (0x00438470);
//   * an action name of more than one character starting '*' binds no
//     action: the binding carries a copy of the name after the '*' in a
//     strlen-byte block (strlen, not strlen + 1: the '*' pays for the NUL);
//     otherwise the action must exist, and one whose code is -10000 or less
//     also gets a CInputBindRec pushed on 0x004b3678, back-pointed at the
//     binding;
//   * the binding goes on the FRONT of the object's list; if the state
//     rebuild (0x00437bf0) fails it is taken off again (only if it is still
//     the head), deleted, and the rebuild run once more.
LTBOOL input_AddTrigger(InputMgr* pMgr, char* pDeviceName, char* pTriggerName,
                        char* pActionName, float rangeLow, float rangeHigh)
{
	CInputBindRec* pRec = LTNULL;
	InputDevice*   pDevice;
	CDeviceObject* pObj;
	CInputAction*  pAction;
	SInputBinding* pBinding;
	float          fTemp;

	if (rangeLow > rangeHigh)
	{
		fTemp     = rangeLow;
		rangeLow  = rangeHigh;
		rangeHigh = fTemp;
	}

	stricmp(pDeviceName, "Joystick 1");

	pDevice = di_FindDevice(pDeviceName);

	if (!pDevice)
		return LTFALSE;

	if (!pDevice->m_pDevice)
		return LTFALSE;

	pObj = di_FindDeviceObject(pDevice, pTriggerName);

	if (!pObj)
	{
#ifdef NOLFSRC_MODERN
		pObj = di_AddDeviceObjectP1(pDevice, pTriggerName);
#else
		pObj = di_AddDeviceObject(pDevice, pTriggerName);
#endif

		if (!pObj)
			return LTFALSE;
	}

	if (strlen(pActionName) > 1 && pActionName[0] == '*')
	{
		pAction = LTNULL;
	}
	else
	{
		pAction = di_FindAction(pActionName);

		if (!pAction)
			return LTFALSE;

		if (pAction->m_nCode <= -10000)
		{
			pRec = new CInputBindRec;

			pRec->m_pNext  = g_pBindRecHead;
			g_pBindRecHead = pRec;
		}
	}

	pBinding = new SInputBinding;

	pBinding->m_pNext      = pObj->m_pBindings;
	pBinding->m_fRangeLow  = rangeLow;
	pBinding->m_fRangeHigh = rangeHigh;
	pBinding->m_pBind      = pRec;

	if (pRec)
		pRec->m_n00 = (uint32)pBinding;

	pBinding->m_pAction = pAction;

	if (!pAction)
	{
		pBinding->m_pData = new uint8[strlen(pActionName)];

		if (!pBinding->m_pData)
		{
			delete pBinding;

			return LTFALSE;
		}

		strcpy((char*)pBinding->m_pData, pActionName + 1);
	}

	pObj->m_pBindings = pBinding;

	if (!di_SetupDeviceFormat(pDevice))
	{
		if (pObj->m_pBindings == pBinding)
		{
			pObj->m_pBindings = pBinding->m_pNext;

			delete pBinding;
		}

		di_SetupDeviceFormat(pDevice);

		return LTFALSE;
	}

	if (g_DebugLevel > 10)
		con_ConsolePrint("Bound %s (on %s) to action %s", pTriggerName,
		                 pDeviceName, pActionName);

	return LTTRUE;
}


// 0x004b0ab0 -- the console debug level (binkvideo.cpp names it).
extern int g_DebugLevel;                   // 0x004b0ab0

// 0x00438000 -- slot 10, ClearBindings.  Find the object, unlink it from
// the device's list (goodlinklist.h's RemoveAt, inlined), delete it -- its
// destructor frees the binding list, each binding's destructor its block
// and bind record, all expanded -- say so above debug level 10, and rebuild
// the device's data format, whose result is what comes back (eax is not
// touched after the call at 0x00438128).
LTBOOL input_ClearBindings(InputMgr* pMgr, char* pDeviceName, char* pTriggerName)
{
	InputDevice*   pDevice;
	CDeviceObject* pObject;

	pDevice = di_FindDevice(pDeviceName);
	if (!pDevice)
		return LTFALSE;

	if (!pDevice->m_pDevice)
		return LTFALSE;

	pObject = di_FindDeviceObject(pDevice, pTriggerName);
	if (!pObject)
		return LTFALSE;

	pDevice->m_Objects.RemoveAt(pObject);
	delete pObject;

	if (g_DebugLevel > 10)
		con_ConsolePrint("Cleared bindings for %s (on %s)", pTriggerName, pDeviceName);

	return di_SetupDeviceFormat(pDevice);
}


// ======================================================================= //
//  START-UP
// ======================================================================= //

// 0x00436bb0 -- the DIEnumDevices callback Init installs.  One record per
// attached device, pushed on the front of the list.
//
// Two things read off the bytes rather than assumed.  The allocation is
// `new`, not dalloc: 0x004290b0 is `operator new` and the
// `test %eax,%eax; je; <inlined ctor>; jmp; xor %esi,%esi` shape around it
// is VC6's null-guarded constructor call, with the redundant `xor` on the
// path where eax is provably zero.  And the memset AFTER it is real: 0xc1
// dwords of zero over the whole 0x304-byte record, which wipes the vptr the
// constructor just stored at +0x84 and the GetTickCount at +0x2f8.  That is
// a shipped bug, not a misreading -- the stores are at 0x00436bc7..0x00436c09
// and the `rep stos` is at 0x00436c18.
BOOL PASCAL di_CreateDeviceCallback(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
{
	InputDevice* pDevice;

#ifdef NOLFSRC_MODERN
	// M-P7-01: the joystick is ##gamepad, read through XInput; DirectInput's
	// joysticks are left out (one of them may be the same pad).
	if (GET_DIDEVICE_TYPE(lpddi->dwDevType) == DIDEVTYPE_JOYSTICK && !P7Pad_IsPad(lpddi->guidInstance))
		return DIENUM_CONTINUE;
#endif

	pDevice = new InputDevice;

	memset(pDevice, 0, sizeof(InputDevice));

	strncpy(pDevice->m_Name, lpddi->tszInstanceName, sizeof(pDevice->m_Name));

	pDevice->m_Guid      = lpddi->guidInstance;
	pDevice->m_dwDevType = lpddi->dwDevType;

	pDevice->m_pNext = g_pDeviceHead;
	g_pDeviceHead    = pDevice;

	return DIENUM_CONTINUE;
}


// 0x00436b40 -- slot 0.  Create the DirectInput object at version 7 and
// fall back to version 3, then enumerate the attached devices.
//
// The two DirectInputCreateA calls are separate call sites (the pushes are
// duplicated, only the version differs), and the FIRST argument is the
// getter's return value, not the global: `call 0x0042e730` is emitted
// between the push of 0x300/0x700 and the push of eax, which only happens
// when the instance handle is a call and not a variable.
LTBOOL input_Init(InputMgr* pMgr, void* pClient)
{
	g_hInputWnd     = dsi_GetMainWindow();
	g_pInputClient  = pClient;

	if (DirectInputCreateA(dsi_GetInstanceHandle(), 0x0700,
	                       &g_pDirectInput, LTNULL) != DI_OK)
	{
		if (DirectInputCreateA(dsi_GetInstanceHandle(), 0x0300,
		                       &g_pDirectInput, LTNULL) != DI_OK)
		{
			return LTFALSE;
		}
	}

#ifdef NOLFSRC_MODERN
	// M-P7-01: ##gamepad, the only joystick (DirectInput's are left out).
	if (P7Pad_Available())
	{
		DIDEVICEINSTANCEA instance;

		P7Pad_GetInstance(&instance);
		di_CreateDeviceCallback(&instance, LTNULL);
	}

	// M-P7-09: the keyboard and the mouse are the only other devices, each
	// described by its own GetDeviceInfo, not EnumDevices. That walks every
	// HID device on the machine looking for joysticks, which are left out
	// above, and on some Windows 11 PCs it crashes doing so (an access
	// violation in RtlAllocateHeap, inside dinput.dll, before the window
	// opens).
	{
		static const GUID* s_pGuids[] = { &GUID_SysKeyboard, &GUID_SysMouse };
		int i;

		for (i = 0; i < 2; i++)
		{
			LPDIRECTINPUTDEVICEA pDev = LTNULL;
			DIDEVICEINSTANCEA    instance;

			if (g_pDirectInput->CreateDevice(*s_pGuids[i], &pDev, LTNULL) != DI_OK || !pDev)
				continue;

			memset(&instance, 0, sizeof(instance));
			instance.dwSize = sizeof(instance);
			if (pDev->GetDeviceInfo(&instance) == DI_OK)
				di_CreateDeviceCallback(&instance, LTNULL);
			pDev->Release();
		}
	}
#else
	g_pDirectInput->EnumDevices(0, di_CreateDeviceCallback, LTNULL,
	                            DIEDFL_ATTACHEDONLY);
#endif

	return LTTRUE;
}


// One force-feedback effect: the list 0x004b3688 heads, which the
// joystick-effect entry (slot 4, 0x0048fab0) fills.  Only the two fields
// Term reaches are known: the effect interface at +0x100 (Unload is vtable
// 0x2c, Release 0x08) and the link at +0x104.  No destructor -- `delete`
// here is a bare `operator delete` with no null test.
struct SInputEffect
{
	uint8                m_pad_000[0x100];
	LPDIRECTINPUTEFFECT  m_pEffect;          // 0x100
	SInputEffect*        m_pNext;            // 0x104
};

extern SInputEffect* g_pEffectHead;         // 0x004b3688


// 0x00436940 -- slot 1.  Tear everything down: each device is unacquired
// and released (the interface pointer is NOT cleared -- the record is
// about to go) and deleted, which runs the object-list and binding
// destructors above; then the effects, the DirectInput object, and the
// action list, whose head is tied off again at the end (next first, as its
// constructor does).  The first thing it does is forget Init's client.
void input_Term()
{
	InputDevice*  pDevice;
	InputDevice*  pNextDevice;
	SInputEffect* pEffect;
	SInputEffect* pDelete;
	CInputAction* pAction;
	CInputAction* pNextAction;

	g_pInputClient = LTNULL;

	pDevice = g_pDeviceHead;

	if (g_pDeviceHead)
	{
		do
		{
			pNextDevice = pDevice->m_pNext;

			if (pDevice->m_pDevice)
			{
				pDevice->m_pDevice->Unacquire();
				pDevice->m_pDevice->Release();
			}

			delete pDevice;

			pDevice = pNextDevice;
		}
		while (pDevice);
	}

	g_pDeviceHead = LTNULL;

	pEffect = g_pEffectHead;

	while (pEffect)
	{
		if (pEffect->m_pEffect)
		{
			pEffect->m_pEffect->Unload();
			pEffect->m_pEffect->Release();
		}

		pEffect->m_pEffect = LTNULL;

		pDelete = pEffect;
		pEffect = pEffect->m_pNext;

		delete pDelete;
	}

	g_pEffectHead = LTNULL;

	if (g_pDirectInput)
	{
		g_pDirectInput->Release();
		g_pDirectInput = LTNULL;
	}

	pAction = g_ActionList.m_pNext;

	while (pAction != &g_ActionList)
	{
		pNextAction = pAction->m_pNext;

		delete pAction;

		pAction = pNextAction;
	}

	g_ActionList.m_pNext = &g_ActionList;
	g_ActionList.m_pPrev = &g_ActionList;
}


// 0x00436c70 -- slot 2.  `xor %eax,%eax; test %ecx,%ecx; setne %al` off the
// global, with the argument never read: the parameter is proven only by the
// call site in EnableDevice, which pushes one.
LTBOOL input_IsInitialized(InputMgr* pMgr)
{
	return g_pDirectInput != LTNULL;
}


// ======================================================================= //
//  THE "listdevices" CONSOLE DUMP
// ======================================================================= //

// 0x00436d60 -- the name of an object's control type, by GUID.  Ten guarded
// returns and a default; the last two share their exit because
// `return "POV hat"` and `return "unknown"` become one `mov`, a `je`, and a
// second `mov`.
//
// The comparison is IsEqualGUID, which objbase.h expands (non-_OLE32_, C++)
// to `!memcmp(&a, &b, sizeof(GUID))`; VC6's memcmp intrinsic on a constant
// 16 is the `mov $0x4,%ecx; repz cmpsl` pair, with the zeroed result
// register left dead because only the flags are used.
char* di_GetObjectTypeName(LPCDIDEVICEOBJECTINSTANCEA lpddoi)
{
	if (IsEqualGUID(lpddoi->guidType, GUID_XAxis))
		return "X axis";

	if (IsEqualGUID(lpddoi->guidType, GUID_YAxis))
		return "Y axis";

	if (IsEqualGUID(lpddoi->guidType, GUID_ZAxis))
		return "Z axis";

	if (IsEqualGUID(lpddoi->guidType, GUID_RxAxis))
		return "X axis rotation";

	if (IsEqualGUID(lpddoi->guidType, GUID_RyAxis))
		return "Y axis rotation";

	if (IsEqualGUID(lpddoi->guidType, GUID_RzAxis))
		return "Z axis rotation";

	if (IsEqualGUID(lpddoi->guidType, GUID_Slider))
		return "slider";

	if (IsEqualGUID(lpddoi->guidType, GUID_Button))
		return "button";

	if (IsEqualGUID(lpddoi->guidType, GUID_Key))
		return "key";

	if (IsEqualGUID(lpddoi->guidType, GUID_POV))
		return "POV hat";

	return "unknown";
}


// 0x00436d30 -- the DIEnumDeviceObjects callback of the console dump.  The
// type-name call is emitted BEFORE the other two pushes, which is cdecl's
// right-to-left argument evaluation and proves it is the last argument.
BOOL PASCAL di_ListObjectCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi,
                                  LPVOID pvRef)
{
	con_ConsolePrint("-    Object %s (type: %s)", lpddoi->tszName,
	                 di_GetObjectTypeName(lpddoi));

	return DIENUM_CONTINUE;
}


// 0x00436cc0 -- the DIEnumDevices callback of the console dump.  It creates
// each device just to list its objects and releases it again.
//
// The frame is the interesting part: there is no `sub $N,%esp` at all, and
// the LPDIRECTINPUTDEVICEA out-parameter lives at `0x8(%esp)` -- lpddi's own
// parameter home slot.  lpddi itself is in esi from the first instruction,
// so the slot is dead and VC6 homes the local there instead of growing the
// frame.  The pointer is re-read from that slot for the EnumObjects and for
// the Release, so it never gets a register.
BOOL PASCAL di_ListDeviceCallback(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
{
	LPDIRECTINPUTDEVICEA pDevice;

	if (g_pDirectInput->CreateDevice(lpddi->guidInstance, &pDevice,
	                                 LTNULL) == DI_OK)
	{
		con_ConsolePrint("- Device %s", lpddi->tszInstanceName);

		pDevice->EnumObjects(di_ListObjectCallback, LTNULL, 0);

		pDevice->Release();
	}
	else
	{
		con_ConsolePrint("- Device %s (unable to create)",
		                 lpddi->tszInstanceName);
	}

	return DIENUM_CONTINUE;
}


// 0x00436c80 -- slot 3.  Note there is NO return after the "not
// initialized" complaint: the shipped bytes fall straight through into the
// header print and then dereference the null g_pDirectInput.  That is what
// shipped, and it is why the test is a bare `if` and not a guard.
void input_ListDevices()
{
	if (!g_pDirectInput)
		con_ConsolePrint("Input not initialized");

	con_ConsolePrint("------------ Input devices ------------");

	g_pDirectInput->EnumDevices(0, di_ListDeviceCallback, LTNULL, 0);
}


// 0x00439f80 -- slot 23.  One line per device, out of the record's own name
// field.
LTBOOL input_ListDeviceNames()
{
	InputDevice* pDevice;

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
		con_ConsolePrint("Device = %s", pDevice->m_Name);

	return LTTRUE;
}


// 0x00439ec0 -- slot 22 (0x00439f1c in the enumeration is the middle of it,
// not a function start: the whole body runs 0x00439ec0..0x00439f7f).
// List one named device's objects, through the same list builder
// GetDeviceObjects uses, and free the list again.
//
// The cursor is two levels deep and that is read off the bytes, not guessed:
// the list head is the 4-byte local `push %ecx` buys at 0x00439ec0, a
// SECOND pointer holding its address is homed in pDeviceName's dead
// parameter slot (`lea 0x4(%esp),%eax; mov %eax,0xc(%esp)`), and what
// EnumObjects is handed is the address of THAT (`lea 0xc(%esp),%edx`).  So
// the callback appends through a tail pointer.
//
// The freeing loop walks its own cursor: shipped never stores back into the
// list head's slot (0x00439f62..0x00439f75); `while (pList) { ... pList =
// pNext; }` adds a `mov %eax,0x8(%esp)` per pass because pList's address
// was taken (194 bytes).
LTBOOL input_ListDeviceObjects(char* pDeviceName)
{
	DeviceObject*  pList = LTNULL;
	DeviceObject** ppTail;
	DeviceObject*  pCur;
	DeviceObject*  pNext;
	InputDevice*   pDevice;

	if (!pDeviceName)
		return LTFALSE;

	pDevice = di_FindDevice(pDeviceName);

	if (!pDevice)
		return LTFALSE;

	if (!pDevice->m_pDevice)
		return LTFALSE;

	g_pEnumDevice = pDevice;
	ppTail        = &pList;

	if (pDevice->m_pDevice->EnumObjects(di_BuildDeviceObjectCallback,
	                                    &ppTail, 0) != DI_OK)
	{
		return LTFALSE;
	}

	g_pEnumDevice = LTNULL;

	for (pCur = pList; pCur; pCur = pCur->m_pNext)
	{
		con_ConsolePrint("Device = %s  Control = %s Type = %i",
		                 pDevice->m_Name, pCur->m_ObjectName,
		                 pCur->m_ObjectType);
	}

	pCur = pList;
	while (pCur)
	{
		pNext = pCur->m_pNext;

		delete pCur;

		pCur = pNext;
	}

	return LTTRUE;
}


// 0x00439ab0 -- slot 18 (clientmgr.h's GetDeviceObjects).  The object list
// of every device the SDK DEVICETYPE_ flags ask for, through the same
// tail-cursor builder ListDeviceObjects uses.  Each pass clears the flag it
// served (joysticks excepted: those are walked by index, 0x00439af6 calling
// di_FindDeviceByIndex with a counter kept at 0x14(%esp), and the flag is
// cleared only when the index runs off the end), and then stamps the SDK
// type and the device's name on every object that has no type yet -- which
// is all the ones this pass appended.  A pass that finds no device ends the
// whole call, not just that flag (0x00439b2e).
//
// NEAR, 246 of 247 bytes: shipped keeps the joystick arm's flag clear
// (`and $~4,%ebx ; jmp 0x00439b2c` at 0x00439b0a) and re-tests the device
// at the join; VC6 here sees the device is null on that path, threads the
// `je` straight to the exit and drops the clear as dead.  Tried and no
// different: if/else order of the arm, `if (!pDevice)` / `if (pDevice)`
// guards, break/return/continue exits, do-while(pDevice), while(flags),
// and the order of the g_pEnumDevice / ppTail stores (the one other,
// scheduling-only block).  Also no better: `if (pDevice == LTNULL) return;
// else if (pDevice->m_pDevice)`; ppTail set before the m_pDevice test (4
// blocks) or the DirectInput device in a local (5); the flag selection as
// an inline helper returning the device (12 blocks).
DeviceObject* input_GetDeviceObjects(uint32 nDeviceFlags)
{
	DeviceObject*  pList = LTNULL;
	int            nJoystick = 0;
	DeviceObject** ppTail;
	DeviceObject*  pCur;
	InputDevice*   pDevice;
	uint32         nType;

	for (;;)
	{
		if (nDeviceFlags & DEVICETYPE_KEYBOARD)
		{
			nDeviceFlags &= ~DEVICETYPE_KEYBOARD;
			nType   = DEVICETYPE_KEYBOARD;
			pDevice = di_FindDeviceByType(DIDEVTYPE_KEYBOARD);
		}
		else if (nDeviceFlags & DEVICETYPE_MOUSE)
		{
			nType = DEVICETYPE_MOUSE;
			nDeviceFlags &= ~DEVICETYPE_MOUSE;
			pDevice = di_FindDeviceByType(DIDEVTYPE_MOUSE);
		}
		else if (nDeviceFlags & DEVICETYPE_JOYSTICK)
		{
			nType   = DEVICETYPE_JOYSTICK;
			pDevice = di_FindDeviceByIndex(DIDEVTYPE_JOYSTICK, nJoystick);

			if (pDevice)
				nJoystick++;
			else
				nDeviceFlags &= ~DEVICETYPE_JOYSTICK;
		}
		else if (nDeviceFlags & DEVICETYPE_UNKNOWN)
		{
			nDeviceFlags &= ~DEVICETYPE_UNKNOWN;
			nType   = DEVICETYPE_UNKNOWN;
			pDevice = di_FindDeviceByType(DIDEVTYPE_DEVICE);
		}
		else
		{
			return pList;
		}

		if (!pDevice)
			return pList;

		if (pDevice->m_pDevice)
		{
			g_pEnumDevice = pDevice;
			ppTail        = &pList;

			pDevice->m_pDevice->EnumObjects(di_BuildDeviceObjectCallback,
			                                &ppTail, 0);

			g_pEnumDevice = LTNULL;
		}

		for (pCur = pList; pCur; pCur = pCur->m_pNext)
		{
			if (!pCur->m_DeviceType)
			{
				pCur->m_DeviceType = nType;

				strncpy(pCur->m_DeviceName, pDevice->m_Name, 127);
			}
		}
	}
}


// 0x00439bf0 -- the DIEnumDeviceObjects callback GetDeviceObjects and
// ListDeviceObjects build their lists with.  pvRef is the address of the
// tail pointer (see ListDeviceObjects), so every access is `**pppTail`,
// re-read after each store because the stores could alias it: a 0xd4-byte
// SDK DeviceObject from `operator new` (0x004290b0), memset clear (the
// 0x35-dword `rep stos`), its CONTROLTYPE_ from the same ten GUIDs, in the
// same order, as di_GetObjectTypeName (0x004a3b0c .. 0x004a3a7c), the name
// strncpy'd at 63, and -- when there is a device being enumerated -- the
// object's DIPROP_RANGE (GetProperty is slot 0x14; the 0x18-byte frame is
// the DIPROPRANGE) as the float range.  Then the tail steps to the new
// node's m_pNext (+0xd0).
BOOL PASCAL di_BuildDeviceObjectCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi,
                                         LPVOID pvRef)
{
	DeviceObject*** pppTail = (DeviceObject***)pvRef;
	DIPROPRANGE     range;

	**pppTail = new DeviceObject;
	memset(**pppTail, 0, sizeof(DeviceObject));

	if (IsEqualGUID(lpddoi->guidType, GUID_XAxis))
		(**pppTail)->m_ObjectType = CONTROLTYPE_XAXIS;
	else if (IsEqualGUID(lpddoi->guidType, GUID_YAxis))
		(**pppTail)->m_ObjectType = CONTROLTYPE_YAXIS;
	else if (IsEqualGUID(lpddoi->guidType, GUID_ZAxis))
		(**pppTail)->m_ObjectType = CONTROLTYPE_ZAXIS;
	else if (IsEqualGUID(lpddoi->guidType, GUID_RxAxis))
		(**pppTail)->m_ObjectType = CONTROLTYPE_RXAXIS;
	else if (IsEqualGUID(lpddoi->guidType, GUID_RyAxis))
		(**pppTail)->m_ObjectType = CONTROLTYPE_RYAXIS;
	else if (IsEqualGUID(lpddoi->guidType, GUID_RzAxis))
		(**pppTail)->m_ObjectType = CONTROLTYPE_RZAXIS;
	else if (IsEqualGUID(lpddoi->guidType, GUID_Slider))
		(**pppTail)->m_ObjectType = CONTROLTYPE_SLIDER;
	else if (IsEqualGUID(lpddoi->guidType, GUID_Button))
		(**pppTail)->m_ObjectType = CONTROLTYPE_BUTTON;
	else if (IsEqualGUID(lpddoi->guidType, GUID_Key))
		(**pppTail)->m_ObjectType = CONTROLTYPE_KEY;
	else if (IsEqualGUID(lpddoi->guidType, GUID_POV))
		(**pppTail)->m_ObjectType = CONTROLTYPE_POV;
	else
		(**pppTail)->m_ObjectType = CONTROLTYPE_UNKNOWN;

	strncpy((**pppTail)->m_ObjectName, lpddoi->tszName, 63);

	if (g_pEnumDevice)
	{
		range.diph.dwSize       = sizeof(DIPROPRANGE);
		range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		range.diph.dwObj        = lpddoi->dwType;
		range.diph.dwHow        = DIPH_BYID;

		if (g_pEnumDevice->m_pDevice->GetProperty(DIPROP_RANGE,
		                                          &range.diph) == DI_OK)
		{
			(**pppTail)->m_RangeLow  = (float)range.lMin;
			(**pppTail)->m_RangeHigh = (float)range.lMax;
		}
	}

	*pppTail = &(**pppTail)->m_pNext;

	return DIENUM_CONTINUE;
}


// ======================================================================= //
//  ENABLING A DEVICE
// ======================================================================= //

// 0x00436e80 -- slot 9 (clientmgr.h's EnableDevice).  Create the named
// device, upgrade it to IDirectInputDevice2A, set its cooperative level and
// data format, and acquire it.
//
// Four things pinned by the bytes:
//
//   * the two locals are laid out DIDATAFORMAT first (frame -0x18..-0x01)
//     and GUID second (-0x28..-0x19), which is first-declared-highest, so
//     the data format has to be DECLARED first even though the GUID is used
//     first.  That is the whole 0x28-byte frame; the HRESULT never gets a
//     slot.
//   * the created IDirectInputDeviceA* lives in pMgr's dead parameter home
//     slot (`lea 0x34(%esp),%edx` with esp 0x30 below entry), the same
//     trick di_ListDeviceCallback plays.
//   * `GET_DIDEVICE_TYPE(...) == DIDEVTYPE_JOYSTICK` narrows to a bare byte
//     compare (`mov 0x80(%esi),%al; cmp $0x4,%al`) because the literal fits
//     in a byte -- contrast di_FindDeviceByType, where the other operand is
//     a uint32 and the load has to zero-extend.
//   * the cooperative level is TWO calls in an if/else that VC6 tail-merges
//     into `push $5; jmp; push $6; <shared call>` (0x00436fb0).  A ternary
//     argument, or a flags local set in an if/else, both fold to a
//     branchless `setne / add $5`.
//
// The empty data format the non-joystick path builds really is empty:
// dwNumObjs 0 and rgodf null, with only dwSize, dwObjSize and dwFlags
// filled in over the memset.  dwFlags is 2, DIDF_RELAXIS (`movl $0x2,
// 0x20(%esp)` at 0x00436fe2) -- the old DIDF_ABSAXIS was a wrong value
// operand masking hid.
//
// The early refusals are nested positive guards so that they share the
// LAST `return LTFALSE` (0x00437020); the already-enabled test wraps the
// whole creation so its `return LTTRUE` is shared with the Acquire path
// (0x00437030).
//
// NEAR, size-exact (443/443, 141 instructions, 5 blocks): the final
// SetDataFormat test is inverted (ours falls into Acquire, shipped `je`s to
// it at 0x00437010 and falls into the Release), and ecx/edx swap in the two
// SetDataFormat arms and the Release/Acquire vtable loads.  Writing the
// Release block as the `then` arm (`!= DI_OK`, with or without an else, a
// trailing return, or goto) fixes that order but moves the shared FALSE exit
// onto the QueryInterface failure's epilogue (439 bytes); a positive
// QueryInterface block gives 447; a ternary between the two SetDataFormat
// calls changes nothing.
LTBOOL input_EnableDevice(InputMgr* pMgr, char* strDeviceName)
{
	DIDATAFORMAT         dataFormat;
	GUID                 guid;
	InputDevice*         pDevice;
	LPDIRECTINPUTDEVICEA pCreated;
	HRESULT              hResult;

	if (input_IsInitialized(pMgr))
	{
		pDevice = di_FindDevice(strDeviceName);

		if (pDevice)
		{
			if (!pDevice->m_pDevice)
			{
				guid = pDevice->m_Guid;

				pDevice->m_bIsMouse = 0;

				if (stricmp(strDeviceName, "##mouse") == 0)
				{
					pDevice->m_bIsMouse = 1;

					guid = GUID_SysMouse;
				}
				else if (stricmp(strDeviceName, "##keyboard") == 0)
				{
					guid = GUID_SysKeyboard;
				}

#ifdef NOLFSRC_MODERN
				// M-P7-01: ##gamepad is ours, not DirectInput's.
				if (P7Pad_CreateDevice(g_pDirectInput, guid, &pCreated) != DI_OK)
					return LTFALSE;
#ifdef NOLFSRC_DEBUG
				// M-P1-33's test: other names for the mouse's buttons.
				pCreated = P1Mouse_TestWrap(guid, pCreated);
#endif
#else
				if (g_pDirectInput->CreateDevice(guid, &pCreated, LTNULL) != DI_OK)
					return LTFALSE;
#endif

				if (pCreated->QueryInterface(IID_IDirectInputDevice2A,
				                             (void**)&pDevice->m_pDevice) != S_OK)
				{
					pCreated->Release();

					return LTFALSE;
				}

				if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK)
					hResult = pDevice->m_pDevice->SetCooperativeLevel(g_hInputWnd,
					              DISCL_FOREGROUND | DISCL_EXCLUSIVE);
				else
					hResult = pDevice->m_pDevice->SetCooperativeLevel(g_hInputWnd,
					              DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

				if (hResult == DI_OK)
				{
					memset(&dataFormat, 0, sizeof(dataFormat));

					dataFormat.dwSize    = sizeof(DIDATAFORMAT);
					dataFormat.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
					dataFormat.dwFlags   = DIDF_RELAXIS;

					if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK)
						hResult = pDevice->m_pDevice->SetDataFormat(&c_dfDIJoystick);
					else
						hResult = pDevice->m_pDevice->SetDataFormat(&dataFormat);
				}

				if (hResult == DI_OK)
				{
					pDevice->m_pDevice->Acquire();

					return LTTRUE;
				}

				pDevice->m_pDevice->Release();

				pDevice->m_pDevice = LTNULL;

				return LTFALSE;
			}

			return LTTRUE;
		}
	}

	return LTFALSE;
}


// 0x00439ea0 -- slot 21 (clientmgr.h's IsDeviceEnabled).  No register is
// saved and no frame allocated: the `return LTFALSE` costs nothing because
// eax is already the null di_FindDevice returned.
LTBOOL input_IsDeviceEnabled(char* strDeviceName)
{
	InputDevice* pDevice;

	pDevice = di_FindDevice(strDeviceName);

	if (!pDevice)
		return LTFALSE;

	return pDevice->m_pDevice != LTNULL;
}


// 0x00439e40 -- slot 20 (clientmgr.h's GetDeviceName).  SDK DEVICETYPE_
// flags in, a DirectInput DIDEVTYPE_ out.  Each arm pushes its own constant
// (`push $3 ; jmp 0x00439e70`) and jumps to one shared `call
// di_FindDeviceByType`: that is FOUR call sites in the source, cross-jumped
// by VC6 -- computing the type into a local gives `mov $3,%eax` + `push
// %eax` instead (99 bytes).
//
// The keyboard arm tests for EQUALITY and the other three test BITS, which
// is why DEVICETYPE_KEYBOARD (1) cannot be reached by a device asking for
// KEYBOARD|MOUSE.  DEVICETYPE_UNKNOWN maps to DIDEVTYPE_DEVICE.
//
// The "no flag matched" arm jumps (0x00439e6c) into the "no such device"
// refusal right after the call, so it is `pDevice = LTNULL`, threaded through
// the null test -- an `else return LTFALSE` merged every refusal into one
// tail at the end (86 bytes).
LTBOOL input_GetDeviceName(uint32 nDeviceType, char* pStrBuffer,
                           uint32 nBufferSize)
{
	InputDevice* pDevice;

	if (!pStrBuffer)
		return LTFALSE;

	if (nDeviceType == DEVICETYPE_KEYBOARD)
		pDevice = di_FindDeviceByType(DIDEVTYPE_KEYBOARD);
	else if (nDeviceType & DEVICETYPE_MOUSE)
		pDevice = di_FindDeviceByType(DIDEVTYPE_MOUSE);
	else if (nDeviceType & DEVICETYPE_JOYSTICK)
		pDevice = di_FindDeviceByType(DIDEVTYPE_JOYSTICK);
	else if (nDeviceType & DEVICETYPE_UNKNOWN)
		pDevice = di_FindDeviceByType(DIDEVTYPE_DEVICE);
	else
		pDevice = LTNULL;

	if (!pDevice)
		return LTFALSE;

	strncpy(pStrBuffer, pDevice->m_Name, nBufferSize - 1);

	return LTTRUE;
}


// ======================================================================= //
//  PER-FRAME INPUT
// ======================================================================= //

// ======================================================================= //
//  SLOT 5 -- READ THE BOUND ACTIONS
// ======================================================================= //

// src/engine/server/console.cpp, compiled into this image: 0x0041b0f0
// con_SetVar, 0x0041b3e0 con_FindVar, 0x0041abe0 con_RunString.  The state
// they take is g_pInputClient, the pointer Init was handed.
void        con_SetVar(class ConsoleState* pState, char* pName, char* pVal);
HCONSOLEVAR con_FindVar(class ConsoleState* pState, char* pName);
void        con_RunString(class ConsoleState* pState, char* pString);

// The three console variables slot 5 reads (server/dsi.cpp defines them;
// lithtech addresses are the server ones moved into the zeroed run).
extern int g_CV_InputDebug;                  // 0x004b3274  "InputDebug"
extern int g_CV_JoystickDisable;             // 0x004b3278  "JoystickDisable"
extern int g_CV_InputRate;                   // 0x004b3298  "InputRate"

// What a console-variable binding record reaches: its m_n00 is the
// binding that owns it (input_AddTrigger's back-pointer) and its m_n04 the
// variable con_FindVar found for the action's name, whose float is at +0x18
// (servermgr.h's LTCommandVar::m_FloatVal).
struct SInputConVar
{
	uint8 m_pad_00[0x18];
	float m_FloatVal;                        // 0x18
};

inline char* di_BindRecVarName(CInputBindRec* pRec)
{
	SInputBinding* pBinding = (SInputBinding*)pRec->m_n00;

	if (!pBinding)
		return LTNULL;

	return (char*)pBinding->m_pAction + 4;
}

inline float di_BindRecValue(CInputBindRec* pRec)
{
	SInputConVar* pVar = (SInputConVar*)pRec->m_n04;

	if (!pVar)
		return 0.0f;

	return pVar->m_FloatVal;
}

// 0x00437160 -- slot 5, the per-frame read (demodtx.cpp's m_ReadInput: the
// manager, a 256-byte action array, three axis offsets).  2564 bytes:
//
//   * the axis offsets are cleared, and every console-variable binding
//     record resets its variable to "%f" of 0 and re-finds it;
//   * per created device (a joystick is skipped when JoystickDisable is on,
//     and polled otherwise): the buffered events are read -- 64 at a time,
//     or ONE at a time for the mouse, looping while events keep coming --
//     with the same overflow / re-acquire handling as TrackDevice;
//   * each event updates its object (the object map by dwOfs / 4): a button
//     latches its value and a "pressed" flag at +0xf0, a POV stores
//     value * 10000, an axis accumulates into a rate at +0x104 over the
//     ticks since its last event (+0xfc), InputRate being the least time an
//     event is spread over when the object is behind;
//   * every object then turns its value into a float -- a button's scaled,
//     an axis's drained at its rate for the elapsed time, clamped to its
//     range and range-scaled -- and walks its bindings: a '*' command runs
//     through the console on a press, actions of code -1/-2/-3 add to the
//     axis offsets, other negative codes set their console variable
//     ("rel"/"abs" Trigger ... with InputDebug), a POV steps the offsets by
//     30 per 45-degree sector, and anything else ORs the action bit;
//   * relative axes and released buttons are cleared again, and the device's
//     time stamp (+0x2f8) becomes this frame's GetTickCount.
void input_ReadActions(InputMgr* pMgr, uint8* pActions, float* pAxisOffsets)
{
	DIDEVICEOBJECTDATA data[64];
	InputDevice*       pDevice;
	CDeviceObject*     pObj;
	CDeviceObject*     pos;
	SInputBinding*     pBinding;
	CInputBindRec*     pRec;
	uint32             dwNow;
	uint32             dwElapsed;
	uint32             dwTime;
	uint32             dwLeft;
	DWORD              dwItems;
	HRESULT            hResult;
	uint32             i;
	uint32             iObject;
	int32              nOld;
	int32              nNew;
	int32              nCode;
	uint32             nType;
	float              fValue;
	float              fVar;

	pAxisOffsets[2] = 0.0f;
	pAxisOffsets[1] = 0.0f;
	pAxisOffsets[0] = 0.0f;

	for (pRec = g_pBindRecHead; pRec; pRec = pRec->m_pNext)
	{
		if (pRec->m_n00)
		{
			char szValue[100];

			_snprintf(szValue, 100, "%f", 0.0);
			szValue[99] = '\0';

			con_SetVar((ConsoleState*)g_pInputClient, di_BindRecVarName(pRec), szValue);
		}

		pRec->m_n04 = (uint32)con_FindVar((ConsoleState*)g_pInputClient,
		                                  di_BindRecVarName(pRec));
	}

	dwNow = GetTickCount();

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		while (pDevice->m_pDevice)
		{
#ifdef NOLFSRC_MODERN
			// M-P7-01: JoystickDisable (the launcher's "Disable joystick")
			// was about DirectInput's joysticks, which are left out; not
			// ##gamepad.
			if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK &&
			    g_CV_JoystickDisable && !P7Pad_IsPad(pDevice->m_Guid))
				break;
#else
			if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK &&
			    g_CV_JoystickDisable)
				break;
#endif

			dwElapsed = dwNow - pDevice->m_dwCreateTime;

			if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK)
				pDevice->m_pDevice->Poll();

			memset(data, 0, sizeof(data));
			dwItems = pDevice->m_bIsMouse ? 1 : 64;

			hResult = pDevice->m_pDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),
			                                            data, &dwItems, 0);
			if (hResult != DI_OK)
			{
				if (hResult == DI_BUFFEROVERFLOW)
				{
					con_ConsolePrint("Input buffer overflow on %s", pDevice->m_Name);
				}
				else
				{
					pDevice->m_pDevice->Acquire();

					hResult = pDevice->m_pDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),
					                                            data, &dwItems, 0);
					if (hResult != DI_OK && hResult != DI_BUFFEROVERFLOW)
						break;
				}
			}

			// The events, into their objects.
			for (i = 0; i < dwItems; i++)
			{
				iObject = data[i].dwOfs >> 2;

				if (iObject >= pDevice->m_Objects.m_nCount)
					continue;

				pObj = pDevice->m_ObjectMap[iObject];

				if (g_CV_InputDebug)
					con_ConsolePrint("%s (%s) generated %d", pObj->m_Name,
					                 pObj->m_TriggerName, data[i].dwData);

				nType = pObj->m_Type;

				if (nType == 0)
				{
					if (data[i].dwData)
					{
						if (!pObj->m_dwValue)
							pObj->m_Unknown0f0 = 1;

						pObj->m_dwValue = data[i].dwData;
					}
				}
				else if (nType == 4)
				{
					pObj->m_dwValue = data[i].dwData * 10000;
				}
				else
				{
					if (!pObj->m_Unknown0f4)
					{
						pObj->m_Unknown0f4 = 1;
						pObj->m_fRate      = 0.0f;
						pObj->m_Unknown100 = 0;
						pObj->m_Unknown0fc = pDevice->m_dwCreateTime;
					}
					else
					{
						pObj->m_fRate *= pObj->m_Unknown100;
					}

					if (pObj->m_Unknown100 < dwElapsed * 2)
					{
						dwTime = data[i].dwTimeStamp - pObj->m_Unknown0fc;

						if (dwTime <= (uint32)g_CV_InputRate)
							dwTime = g_CV_InputRate;
					}
					else
					{
						dwTime = data[i].dwTimeStamp - pObj->m_Unknown0fc;
					}

					pObj->m_Unknown100 += dwTime;
					pObj->m_Unknown0fc  = data[i].dwTimeStamp;

					if (nType == 3)
					{
						pObj->m_fRate = ((int32)(data[i].dwData * 10000) + pObj->m_fRate) /
						                pObj->m_Unknown100;
					}
					else
					{
						nNew = data[i].dwData;
						nOld = pObj->m_nValue;
						pObj->m_nValue = nNew;

						pObj->m_fRate = ((nNew - nOld) * 10000 + pObj->m_fRate) /
						                pObj->m_Unknown100;
					}
				}
			}

			// Every object: its value, then its bindings.
			for (pos = pDevice->m_Objects.m_pHead; pos; )
			{
				pObj = pDevice->m_Objects.GetNext(pos);

				if (pObj->m_Type == 0)
				{
					fValue = (int32)pObj->m_dwValue * pObj->m_fScale;
				}
				else
				{
					if (pObj->m_Type != 4 && pObj->m_Unknown0f4)
					{
						dwLeft = pObj->m_Unknown100;
						dwTime = (dwLeft < dwElapsed) ? dwLeft : dwElapsed;

						pObj->m_dwValue += (long)(dwTime * pObj->m_fRate);

						if (dwLeft > dwTime)
						{
							dwLeft -= dwTime;
						}
						else
						{
							dwLeft = 0;
							pObj->m_Unknown0f4 = 0;
							pObj->m_nValue = (long)(pObj->m_dwValue * 0.0001f);
							pObj->m_fRate  = 0;
						}

						pObj->m_Unknown100 = dwLeft;
					}

					fValue = (int32)pObj->m_dwValue * 0.0001f;

					fValue = LTCLAMP(fValue, pObj->m_fRangeMin, pObj->m_fRangeMax);

					if (pObj->m_fRangeScalePreCenterOffset != 0.0)
					{
						if (fValue <= pObj->m_fCenter)
							fValue = fValue * pObj->m_fLowMult + pObj->m_fLowOffset;
						else
							fValue = fValue * pObj->m_fHighMult + pObj->m_fHighOffset;
					}
					else
					{
						fValue = fValue * pObj->m_fRangeScaleMult + pObj->m_fRangeScaleOffset;
					}

					fValue *= pObj->m_fScale;
				}

				for (pBinding = pObj->m_pBindings; pBinding; pBinding = pBinding->m_pNext)
				{
					if (pBinding->m_pData && pObj->m_Unknown0f0 && g_pInputClient)
					{
						con_RunString((ConsoleState*)g_pInputClient, (char*)pBinding->m_pData);
						continue;
					}

					if (!pBinding->m_pAction)
						continue;

					nCode = pBinding->m_pAction->m_nCode;

					if (pObj->m_Type == 3 && nCode < 0)
					{
						if (nCode == -1)
							pAxisOffsets[0] += fValue;
						else if (nCode == -2)
							pAxisOffsets[1] += fValue;
						else if (nCode == -3)
							pAxisOffsets[2] += fValue;
						else if (pRec = pBinding->m_pBind)
						{
							fVar = di_BindRecValue(pRec);

							if (pRec->m_n00)
							{
								char szValue[100];

								_snprintf(szValue, 100, "%f", fVar + fValue);
								szValue[99] = '\0';

								con_SetVar((ConsoleState*)g_pInputClient,
								           di_BindRecVarName(pRec), szValue);
							}

							if (g_CV_InputDebug)
							{
								pRec = pBinding->m_pBind;

								con_ConsolePrint("rel Trigger %s set console var %s to %22.12f",
								                 pObj->m_Name, di_BindRecVarName(pRec),
								                 di_BindRecValue(pRec));
							}
						}
					}
					else if (pObj->m_Type == 2)
					{
						if (nCode < 0)
						{
							if (nCode == -1)
								pAxisOffsets[0] += fValue;
							else if (nCode == -2)
								pAxisOffsets[1] += fValue;
							else if (nCode == -3)
								pAxisOffsets[2] += fValue;
							else if (pRec = pBinding->m_pBind)
							{
								fVar = di_BindRecValue(pRec);

								if (pRec->m_n00)
								{
									char szValue[100];

									_snprintf(szValue, 100, "%f", fVar + fValue);
									szValue[99] = '\0';

									con_SetVar((ConsoleState*)g_pInputClient,
									           di_BindRecVarName(pRec), szValue);
								}

								if (g_CV_InputDebug)
								{
									pRec = pBinding->m_pBind;

									con_ConsolePrint("abs Trigger %s set console var %s to %22.12f",
									                 pObj->m_Name, di_BindRecVarName(pRec),
									                 di_BindRecValue(pRec));
								}
							}
						}
						else if (fValue >= pBinding->m_fRangeLow &&
						         fValue <= pBinding->m_fRangeHigh)
						{
							pActions[nCode] |= 1;
						}
					}
					else if (pObj->m_Type == 4 && (pRec = pBinding->m_pBind))
					{
						fVar = di_BindRecValue(pRec);

						if (pRec->m_n00)
						{
							char szValue[100];

							_snprintf(szValue, 100, "%f", fVar + fValue);
							szValue[99] = '\0';

							con_SetVar((ConsoleState*)g_pInputClient,
							           di_BindRecVarName(pRec), szValue);
						}
					}
					else if (pObj->m_Type == 4 && nCode < 0 && fValue != 65536.0f)
					{
						if (fValue >= 4500.0f && fValue <= 13500.0f)
							pAxisOffsets[0] += 30.0f;
						else if (fValue >= 22500.0f && fValue <= 31500.0f)
							pAxisOffsets[0] -= 30.0f;

						if ((fValue >= 31500.0f && fValue <= 36000.0f) ||
						    (fValue <= 4500.0f && fValue >= 0.0f))
							pAxisOffsets[1] -= 30.0f;

						if (fValue >= 13500.0f && fValue <= 22500.0f)
							pAxisOffsets[1] += 30.0f;
					}
					else if (pBinding->m_fRangeLow == 0.0f && pBinding->m_fRangeHigh == 0.0f)
					{
						if (nCode >= 0)
							pActions[nCode] |= (pObj->m_dwValue != 0);
					}
					else if (fValue >= pBinding->m_fRangeLow &&
					         fValue <= pBinding->m_fRangeHigh)
					{
						pActions[nCode] |= 1;
					}
				}

				if (pObj->m_Type == 3)
					pObj->m_dwValue = 0;

				pObj->m_Unknown0f0 = 0;
			}

			// Relative axes and released buttons, cleared for the next read.
			for (i = 0; i < dwItems; i++)
			{
				iObject = data[i].dwOfs >> 2;

				if (iObject >= pDevice->m_Objects.m_nCount)
					continue;

				pObj = pDevice->m_ObjectMap[iObject];

				if (pObj->m_Type == 0 && !data[i].dwData)
					pObj->m_dwValue = data[i].dwData;

				if (pObj->m_Type == 3)
					pObj->m_dwValue = 0;
			}

			if (!dwItems || !pDevice->m_bIsMouse)
				break;
		}

		pDevice->m_dwCreateTime = dwNow;
	}
}


// 0x00437b70 -- slot 6.  Throw away everything in every device's buffer:
// GetDeviceData with a null array and a count of INFINITE is DirectInput's
// flush.  DI_BUFFEROVERFLOW (1) is accepted alongside DI_OK, so a device
// that overflowed is not reported as a failure.
//
// The `push %ecx` prologue is the 4-byte local dwItems, re-set to INFINITE
// on every iteration.  The DirectInput device is a named local: shipped
// loads it once into eax and tests that (`mov (%esi),%eax ; test` at
// 0x00437b82); testing pDevice->m_pDevice directly gives `cmpl $0,(%esi)`
// and a reload (77 bytes, 4 blocks).
LTBOOL input_FlushDeviceData()
{
	InputDevice* pDevice;
	HRESULT      hResult;
	DWORD        dwItems;
	LTBOOL       bOK;
	LPDIRECTINPUTDEVICE2A pDIDevice;

	bOK = LTTRUE;

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		pDIDevice = pDevice->m_pDevice;

		if (pDIDevice)
		{
			dwItems = INFINITE;

			hResult = pDIDevice->GetDeviceData(
			              sizeof(DIDEVICEOBJECTDATA), LTNULL, &dwItems, 0);

			if (hResult != DI_OK && hResult != DI_BUFFEROVERFLOW)
				bOK = LTFALSE;
		}
	}

	return bOK;
}


// 0x00437bf0 -- (re)build a device's data format from its object list, one
// dword per distinctly-named object and at most 150, set the buffer size,
// centre the axes, acquire, and read the first state.  A refused Acquire
// reports whether ANOTHER APP has it (E_ACCESSDENIED), not failure.
//
// NEAR, size-exact (864/262, 10 mnemonic blocks), register allocation only:
// the inner walk's head load lands in edx and is copied to edi (shipped
// loads edi directly), and the DIDATAFORMAT fill is scheduled differently.
// The allocation sits on a cliff set by the constant 4: with the (wrong)
// DIDEVTYPE_KEYBOARD, or `nOffset += 8`, the frame and every register match
// shipped; with the right DIDEVTYPE_JOYSTICK, 4 is also the offset step and
// nOffset gets a register after the loop (32 blocks) unless m_dwDataSize is
// read back from df, as below.  Measured without moving it: the six df
// stores in 32 orders, the flag test as ?:, if/else, `+ 1`, LOBYTE, `& 0xff`,
// (BYTE)/4u constants; nOffset as uint32/unsigned/derived from nObjects;
// declaration order; the dup search with goto, a flag or `if (!bFound)`.
LTBOOL di_SetupDeviceFormat(InputDevice* pDevice)
{
	DIOBJECTDATAFORMAT formats[150];
	DWORD              state[150];
	DIDATAFORMAT       df;
	DIPROPDWORD        dipdw;
	CDeviceObject*     pos;
	CDeviceObject*     pos2;
	CDeviceObject*     pObject;
	CDeviceObject*     pOther;
	int                nObjects;
	DWORD              nOffset;
	HRESULT            hr;
	BOOL               bFound;

	pDevice->m_pDevice->Unacquire();

	memset(pDevice->m_ObjectMap, 0, sizeof(pDevice->m_ObjectMap));
	memset(formats, 0, sizeof(formats));

	nObjects = 0;
	nOffset  = 0;

	for (pos = pDevice->m_Objects.m_pHead; pos; )
	{
		pObject = pDevice->m_Objects.GetNext(pos);

		if (nObjects >= 150)
			break;

		bFound = FALSE;

		for (pos2 = pDevice->m_Objects.m_pHead; pos2; )
		{
			pOther = pDevice->m_Objects.GetNext(pos2);

			if (pOther == pObject)
				break;

			if (stricmp(pOther->m_Name, pObject->m_Name) == 0)
			{
				pObject->m_nIndex = pOther->m_nIndex;
				bFound = TRUE;
				break;
			}
		}

		if (bFound)
			continue;

		formats[nObjects].pguid  = &pObject->m_guidType;
		formats[nObjects].dwOfs  = nOffset;
		formats[nObjects].dwType = pObject->m_dwType;

		pObject->m_nIndex = nObjects;
		pDevice->m_ObjectMap[nOffset / sizeof(DWORD)] = pObject;

		nObjects++;
		nOffset += sizeof(DWORD);
	}

	memset(&df, 0, sizeof(df));
	df.dwSize     = sizeof(DIDATAFORMAT);
	df.dwObjSize  = sizeof(DIOBJECTDATAFORMAT);
	df.dwFlags    = (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK) ?
	                DIDF_ABSAXIS : DIDF_RELAXIS;
	df.dwDataSize = nOffset;
	df.dwNumObjs  = nObjects;
	df.rgodf      = formats;

	if (pDevice->m_pDevice->SetDataFormat(&df) != DI_OK)
		return LTFALSE;

	pDevice->m_dwDataSize = df.dwDataSize;

	dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj        = 0;
	dipdw.diph.dwHow        = DIPH_DEVICE;
	dipdw.dwData            = 64;

	pDevice->m_pDevice->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);
	pDevice->m_pDevice->GetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);

	if (dipdw.dwData == 0)
		return LTFALSE;

	for (pos = pDevice->m_Objects.m_pHead; pos; )
	{
		pObject = pDevice->m_Objects.GetNext(pos);

		if (pObject->m_Type == 3 || pObject->m_Type == 2)
		{
			pObject->m_dwValue = (long)((pObject->m_fRangeMax - pObject->m_fRangeMin) * 0.5f +
			                            (long)pObject->m_fRangeScalePreCenterOffset) * 10000;
			pObject->m_nValue  = (long)(pObject->m_dwValue * 0.0001f);
		}
		else
		{
			pObject->m_nValue  = 0;
			pObject->m_dwValue = 0;
		}

		pObject->m_Unknown0f4 = 0;
		pObject->m_Unknown100 = 0;
	}

	hr = pDevice->m_pDevice->Acquire();
	if (hr != DI_OK)
		return hr == DIERR_OTHERAPPHASPRIO;

	if (pDevice->m_pDevice->GetDeviceState(pDevice->m_dwDataSize, state) == DI_OK)
	{
		for (pos = pDevice->m_Objects.m_pHead; pos; )
		{
			pObject = pDevice->m_Objects.GetNext(pos);

			if (pObject->m_Type == 3 || pObject->m_Type == 2)
			{
				pObject->m_dwValue = state[pObject->m_nIndex] * 10000;
				pObject->m_nValue  = (long)(pObject->m_dwValue * 0.0001f);
			}
			else
			{
				pObject->m_nValue = pObject->m_dwValue = state[pObject->m_nIndex];
			}
		}
	}

	return LTTRUE;
}


// 0x00437bc0 -- slot 7.  Read every created device's state.  It always
// returns zero (`xor %eax,%eax` on the only exit), which is what shipped.
int input_ReadInput()
{
	InputDevice* pDevice;

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (pDevice->m_pDevice)
			di_SetupDeviceFormat(pDevice);
	}

	return 0;
}


// 0x00439a30 -- slot 18 (clientmgr.h's EndDeviceTrack).  Stop tracking on
// every device: a device that has a track buffer is torn down completely, a
// device that does not just gets one more state read.
//
// The nested `if (pDevice->m_pDevice)` really is a second test of the field
// the outer `if` already tested -- `mov (%esi),%eax; cmp %edi,%eax; je` at
// 0x00439a52 -- so the release is written as its own guarded block and not
// hoisted.  The zero lives in edi for the whole function, which is VC6 with
// one constant used by five comparisons and three stores.
LTBOOL input_EndDeviceTrack()
{
	InputDevice* pDevice;

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (pDevice->m_pDevice)
		{
			pDevice->m_bTracking = 0;

			if (pDevice->m_bTrackEnabled)
			{
				if (pDevice->m_pDevice)
				{
					pDevice->m_pDevice->Unacquire();
					pDevice->m_pDevice->Release();

					pDevice->m_pDevice = LTNULL;
				}

				pDevice->m_bTrackEnabled = 0;

				if (pDevice->m_pTrackData)
				{
					delete pDevice->m_pTrackData;

					pDevice->m_pTrackData = LTNULL;
				}
			}
			else
			{
				di_SetupDeviceFormat(pDevice);
			}
		}
	}

	return LTTRUE;
}


// ======================================================================= //
//  OBJECT ENUMERATION HELPERS
// ======================================================================= //

// 0x00439520 -- a DIEnumDeviceObjects callback that only counts.  `ret $8`
// with the first argument never read.
BOOL PASCAL di_CountObjectsCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi,
                                    LPVOID pvRef)
{
	(*(uint32*)pvRef)++;

	return DIENUM_CONTINUE;
}


// One row of the flat object table the counting pass sizes and the copying
// pass fills.  0x11c bytes -- the stride the cursor advances by at
// 0x0043958d -- and the layout is read straight off the three stores: the
// 16-byte guidType at 0x000, dwType at 0x014 (NOT 0x010: `mov %ecx,0x14(%edx)`,
// so there is a dword nothing writes in between), and a MAX_PATH name at
// 0x018 truncated at 259 and terminated by hand at [0x103] off that base.
// 16 + 4 + 4 + 260 = 0x11c, which is the stride, so nothing else fits.
struct DIObjectRow
{
	GUID   m_guidType;                       // 0x000
	uint32 m_Unknown010;                     // 0x010
	DWORD  m_dwType;                         // 0x014
	char   m_Name[MAX_PATH];                 // 0x018
};


// 0x00439540 -- the copying pass.  pvRef is a `DIObjectRow**` cursor and
// every field is written through a fresh `*ppRow` load, which is why the
// cursor is re-read four times instead of being cached.
BOOL PASCAL di_CopyObjectCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi,
                                  LPVOID pvRef)
{
	DIObjectRow** ppRow = (DIObjectRow**)pvRef;

	(*ppRow)->m_guidType = lpddoi->guidType;
	(*ppRow)->m_dwType   = lpddoi->dwType;

	char* pName = (*ppRow)->m_Name;

	strncpy(pName, lpddoi->tszName, MAX_PATH - 1);
	pName[MAX_PATH - 1] = '\0';

	(*ppRow)++;

	return DIENUM_CONTINUE;
}


// 0x00439270 -- slot 15 (clientmgr.h's StartDeviceTrack).  For each of the
// four SDK device types asked for (in the fixed order keyboard, mouse,
// joystick, unknown -- an index loop with an if/else chain on it, 0x00439292
// .. 0x004392e9), switch the device to BUFFERED reads of every axis, button
// and POV (0x1f): enable it if nobody has (and remember that we did, +0x2ec,
// so EndDeviceTrack releases it), unacquire, count the objects, copy them
// into a DIObjectRow table kept on the device (+0x2f0, 0x11c a row), build a
// DIOBJECTDATAFORMAT per row pointing into that table (dwOfs 4*j), set the
// format -- ABSAXIS for a joystick, RELAXIS otherwise, `setne; inc` -- set an
// 8-entry buffer, and acquire.  A buffer size over 32 is refused up front
// (`jbe`) and the size is otherwise NOT used; its dead home slot is where
// VC6 keeps the object count.  The count and the copy cursor are zeroed at
// the TOP of every pass (the stores sit after the loop label, 0x00439294).  Any failure drops out of the loop, and the
// test after it (`cmp $4,%edi; jge` at 0x00439502) turns an early exit into
// LTFALSE.
LTBOOL input_StartDeviceTrack(InputMgr* pMgr, uint32 nDevices, uint32 nBufferSize)
{
	DIDATAFORMAT         dataFormat;
	DIPROPDWORD          dipdw;
	DIObjectRow*         pCursor;
	int                  i;
	uint32               nObjects;
	InputDevice*         pDevice;
	DIOBJECTDATAFORMAT*  pFormats;
	uint32               j;
	HRESULT              hResult;

	if (nBufferSize > 32)
		return LTFALSE;

	for (i = 0; i < 4; i++)
	{
		nObjects = 0;
		pCursor  = LTNULL;

		if (i == 0)
		{
			if (!(nDevices & DEVICETYPE_KEYBOARD))
				continue;

			pDevice = di_FindDeviceByType(DIDEVTYPE_KEYBOARD);
		}
		else if (i == 1)
		{
			if (!(nDevices & DEVICETYPE_MOUSE))
				continue;

			pDevice = di_FindDeviceByType(DIDEVTYPE_MOUSE);
		}
		else if (i == 2)
		{
			if (!(nDevices & DEVICETYPE_JOYSTICK))
				continue;

			pDevice = di_FindDeviceByType(DIDEVTYPE_JOYSTICK);
		}
		else if (i == 3)
		{
			if (!(nDevices & DEVICETYPE_UNKNOWN))
				continue;

			pDevice = di_FindDeviceByType(DIDEVTYPE_DEVICE);
		}
		else
		{
			continue;
		}

		if (!pDevice)
			continue;

		if (!pDevice->m_pDevice)
		{
			input_EnableDevice(pMgr, pDevice->m_Name);

			if (!pDevice->m_pDevice)
				break;

			pDevice->m_bTrackEnabled = 1;
		}

		pDevice->m_pDevice->Unacquire();

		if (pDevice->m_pDevice->EnumObjects(di_CountObjectsCallback, &nObjects,
		                                    DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV) != DI_OK)
			break;

		if (!nObjects)
			break;

		pDevice->m_pTrackData = (uint8*)new DIObjectRow[nObjects];
		if (!pDevice->m_pTrackData)
			break;

		pCursor = (DIObjectRow*)pDevice->m_pTrackData;

		if (pDevice->m_pDevice->EnumObjects(di_CopyObjectCallback, &pCursor,
		                                    DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV) != DI_OK)
		{
			delete pDevice->m_pTrackData;
			pDevice->m_pTrackData = LTNULL;
			break;
		}

		pFormats = new DIOBJECTDATAFORMAT[nObjects];
		if (!pFormats)
		{
			delete pDevice->m_pTrackData;
			pDevice->m_pTrackData = LTNULL;
			break;
		}

		for (j = 0; j < nObjects; j++)
		{
			pFormats[j].pguid   = &((DIObjectRow*)pDevice->m_pTrackData)[j].m_guidType;
			pFormats[j].dwOfs   = j * 4;
			pFormats[j].dwType  = ((DIObjectRow*)pDevice->m_pTrackData)[j].m_dwType;
			pFormats[j].dwFlags = 0;
		}

		dataFormat.dwSize     = sizeof(DIDATAFORMAT);
		dataFormat.dwObjSize  = sizeof(DIOBJECTDATAFORMAT);
		dataFormat.dwFlags    = (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK)
		                        ? DIDF_ABSAXIS : DIDF_RELAXIS;
		dataFormat.dwDataSize = nObjects * 4;
		dataFormat.dwNumObjs  = nObjects;
		dataFormat.rgodf      = pFormats;

		hResult = pDevice->m_pDevice->SetDataFormat(&dataFormat);

		delete pFormats;

		if (hResult != DI_OK)
		{
			delete pDevice->m_pTrackData;
			pDevice->m_pTrackData = LTNULL;
			break;
		}

		dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipdw.diph.dwObj        = 0;
		dipdw.diph.dwHow        = DIPH_DEVICE;
		dipdw.dwData            = 8;

		pDevice->m_pDevice->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);

		pDevice->m_bTracking     = 1;
		pDevice->m_nTrackObjects = nObjects;

		pDevice->m_pDevice->Acquire();
	}

	if (i < 4)
		return LTFALSE;

	return LTTRUE;
}


// 0x004395a0 -- slot 16 (clientmgr.h's TrackDevice).  Drain every tracking
// device's DirectInput buffer (32 events of 16 bytes, the 0x80-dword clear
// at 0x0043960b) into the caller's DeviceInput array: *pnInOut is the room
// on entry and the count filled on exit.  A joystick is polled first
// (IDirectInputDevice2 slot 0x64); an overflow is reported and the data
// kept, any other failure re-acquires and reads once more.  The device type
// is a four-case switch (jump table 0x00439a20) on the DIDEVTYPE, in the
// keyboard, mouse, joystick, device order the case bodies are laid out in.
// The control is found through the DIObjectRow StartDeviceTrack stored for
// it (dwOfs / 4 is the row), its type from eleven IsEqualGUID tests
// (GUID_Unknown's 0 is stored out of memcmp's zeroed edx), and the last byte
// of the control name carries DIDFT_GETINSTANCE, the DIK code
// FolderCustomControls.cpp reads back from m_ControlName[63].
LTBOOL input_TrackDevice(DeviceInput* pInputArray, uint32* pnInOut)
{
	DIDEVICEOBJECTDATA data[32];
	uint32             nMax;
	uint32             i;
	uint32             iObject;
	InputDevice*       pDevice;
	DWORD              dwItems;
	HRESULT            hResult;

	nMax     = *pnInOut;
	*pnInOut = 0;

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (!pDevice->m_bTracking || !pDevice->m_pTrackData)
			continue;

		if (GET_DIDEVICE_TYPE(pDevice->m_dwDevType) == DIDEVTYPE_JOYSTICK)
			pDevice->m_pDevice->Poll();

		memset(data, 0, sizeof(data));
		dwItems = 32;

		hResult = pDevice->m_pDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),
		                                            data, &dwItems, 0);
		if (hResult != DI_OK)
		{
			if (hResult == DI_BUFFEROVERFLOW)
			{
				con_ConsolePrint("Input buffer overflow on %s", pDevice->m_Name);
			}
			else
			{
				pDevice->m_pDevice->Acquire();

				hResult = pDevice->m_pDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),
				                                            data, &dwItems, 0);
				if (hResult != DI_OK && hResult != DI_BUFFEROVERFLOW)
					continue;
			}
		}

		for (i = 0; i < dwItems; i++)
		{
			if (*pnInOut == nMax)
				return LTTRUE;

			switch (GET_DIDEVICE_TYPE(pDevice->m_dwDevType))
			{
				case DIDEVTYPE_KEYBOARD:
					pInputArray[*pnInOut].m_DeviceType = DEVICETYPE_KEYBOARD;
					break;

				case DIDEVTYPE_MOUSE:
					pInputArray[*pnInOut].m_DeviceType = DEVICETYPE_MOUSE;
					break;

				case DIDEVTYPE_JOYSTICK:
					pInputArray[*pnInOut].m_DeviceType = DEVICETYPE_JOYSTICK;
					break;

				case DIDEVTYPE_DEVICE:
					pInputArray[*pnInOut].m_DeviceType = DEVICETYPE_UNKNOWN;
					break;
			}

			strncpy(pInputArray[*pnInOut].m_DeviceName, pDevice->m_Name, 127);

			iObject = data[i].dwOfs >> 2;

			if (iObject >= pDevice->m_nTrackObjects)
				continue;

			if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_XAxis))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_XAXIS;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_YAxis))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_YAXIS;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_ZAxis))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_ZAXIS;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_RxAxis))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_RXAXIS;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_RyAxis))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_RYAXIS;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_RzAxis))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_RZAXIS;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_Slider))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_SLIDER;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_Button))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_BUTTON;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_Key))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_KEY;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_POV))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_POV;
			else if (IsEqualGUID(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_guidType, GUID_Unknown))
				pInputArray[*pnInOut].m_ControlType = CONTROLTYPE_UNKNOWN;

			strncpy(pInputArray[*pnInOut].m_ControlName, ((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_Name, 62);
			pInputArray[*pnInOut].m_ControlName[62] = '\0';
			pInputArray[*pnInOut].m_ControlName[63] = (char)DIDFT_GETINSTANCE(((DIObjectRow*)pDevice->m_pTrackData)[iObject].m_dwType);
			pInputArray[*pnInOut].m_InputValue = data[i].dwData;

			(*pnInOut)++;
		}
	}

	return LTTRUE;
}


// 0x00438bb0 -- slot 12, the console's trigger scale (LithTech's
// input_ScaleTrigger).  The scale is clamped to +-500 in its own argument
// slot (0x00438bc1 / 0x00438bdc store straight back into it), the device and
// then the object are looked up by name, and the scale is stored.  A range
// scale of 0..0 means "none" and stops there; otherwise the object's range
// [m_fRangeMin, m_fRangeMax] is mapped linearly onto [fMin, fMax] (a
// degenerate range gets a multiplier of 1, the constant hoisted into edx at
// 0x00438c61), and a non-zero pre-centre offset splits it into two pieces
// that meet at the shifted centre, stored last at +0x130.  The receiver is
// never read.
LTBOOL input_ScaleTrigger(InputMgr* pMgr, char* pDeviceName, char* pTriggerName,
                          float fScale, float fRangeScaleMin, float fRangeScaleMax,
                          float fRangeScalePreCenterOffset)
{
	InputDevice*   pDevice;
	CDeviceObject* pObject;
	float          fRange;
	float          fCenter;
	float          fOutCenter;

	if (fScale < -500.0f)
		fScale = -500.0f;
	else if (fScale > 500.0f)
		fScale = 500.0f;

	pDevice = di_FindDevice(pDeviceName);
	if (!pDevice)
		return LTFALSE;

	pObject = di_FindDeviceObject(pDevice, pTriggerName);
	if (!pObject)
		return LTFALSE;

	pObject->m_fScale = fScale;

	if (fRangeScaleMin != 0.0f || fRangeScaleMax != 0.0f)
	{
		pObject->m_fRangeScaleMin = fRangeScaleMin;
		pObject->m_fRangeScaleMax = fRangeScaleMax;

		fRange = pObject->m_fRangeMin - pObject->m_fRangeMax;
		if (fRange != 0.0f)
			pObject->m_fRangeScaleMult = (fRangeScaleMin - fRangeScaleMax) / fRange;
		else
			pObject->m_fRangeScaleMult = 1.0f;

		pObject->m_fRangeScaleOffset = fRangeScaleMin - pObject->m_fRangeScaleMult * pObject->m_fRangeMin;

		pObject->m_fRangeScalePreCenterOffset = fRangeScalePreCenterOffset;

		if (fRangeScalePreCenterOffset != 0.0f)
		{
			fCenter    = (pObject->m_fRangeMax - pObject->m_fRangeMin) * 0.5f +
			             fRangeScalePreCenterOffset + pObject->m_fRangeMin;
			fOutCenter = (fRangeScaleMax - fRangeScaleMin) * 0.5f + fRangeScaleMin;

			fRange = fCenter - pObject->m_fRangeMax;
			if (fRange != 0.0f)
				pObject->m_fHighMult = (fOutCenter - fRangeScaleMax) / fRange;
			else
				pObject->m_fHighMult = 1.0f;

			pObject->m_fHighOffset = fOutCenter - fCenter * pObject->m_fHighMult;

			fRange = pObject->m_fRangeMin - fCenter;
			if (fRange != 0.0f)
				pObject->m_fLowMult = (fRangeScaleMin - fOutCenter) / fRange;
			else
				pObject->m_fLowMult = 1.0f;

			pObject->m_fLowOffset = fRangeScaleMin - pObject->m_fLowMult * pObject->m_fRangeMin;

			pObject->m_fCenter = fCenter;
		}
	}

	return LTTRUE;
}


// 0x00438d80 -- the input half of the console's config save (the first of
// ccclient.cpp's two g_ClientConsoleSavers, which declares it under this
// name).  Every action as "AddAction", a blank line, then per enabled device
// with objects an "enabledevice" line under its alias (or its DirectInput
// name), and per object one "rangebind" line built up in a 1 KB buffer from
// each binding's "%f %f" range and action ("*name" for a '*' binding, which
// keeps the name in m_pData; nothing for one with neither), followed by the
// object's "scale" (only when not 1) or, when any range-scale value is
// non-zero, "rangescale".  The object walk is CDeviceObjectList::GetNext
// (the `neg / sbb / and` at 0x00438e57).
void cc_SaveInputBindings(FILE* pFile)
{
	char           str[1024];
	char           temp[1024];
	CInputAction*  pAction;
	InputDevice*   pDevice;
	char*          pDeviceName;
	CDeviceObject* pos;
	CDeviceObject* pObj;
	SInputBinding* pBinding;

	for (pAction = g_ActionList.m_pNext;
	     pAction != &g_ActionList;
	     pAction = pAction->m_pNext)
	{
		fprintf(pFile, "AddAction %s %d\n", pAction->m_Name, pAction->m_nCode);
	}

	fprintf(pFile, "\n");

	for (pDevice = g_pDeviceHead; pDevice; pDevice = pDevice->m_pNext)
	{
		if (!pDevice->m_pDevice || pDevice->m_Objects.m_nCount <= 0)
			continue;

		if (pDevice->m_pAliasName)
			pDeviceName = pDevice->m_pAliasName;
		else
			pDeviceName = pDevice->m_Name;

		fprintf(pFile, "enabledevice \"%s\"\n", pDeviceName);

		for (pos = pDevice->m_Objects.m_pHead; pos; )
		{
			pObj = pDevice->m_Objects.GetNext(pos);

			sprintf(str, "rangebind \"%s\" \"%s\" ", pDeviceName,
			        pObj->m_TriggerName);

			for (pBinding = pObj->m_pBindings; pBinding;
			     pBinding = pBinding->m_pNext)
			{
				if (pBinding->m_pAction)
				{
					sprintf(temp, "%f %f \"%s\" ", pBinding->m_fRangeLow,
					        pBinding->m_fRangeHigh,
					        pBinding->m_pAction->m_Name);
				}
				else if (pBinding->m_pData)
				{
					sprintf(temp, "%f %f \"*%s\" ", pBinding->m_fRangeLow,
					        pBinding->m_fRangeHigh,
					        (char*)pBinding->m_pData);
				}
				else
				{
					temp[0] = '\0';
				}

				strcat(str, temp);
			}

			strcat(str, "\n");
			fprintf(pFile, str);

			if (pObj->m_fRangeScaleMin == 0.0f &&
			    pObj->m_fRangeScaleMax == 0.0f &&
			    pObj->m_fRangeScalePreCenterOffset == 0.0f)
			{
				if (pObj->m_fScale != 1.0f)
				{
					fprintf(pFile, "scale \"%s\" \"%s\" %f\n", pDeviceName,
					        pObj->m_TriggerName, pObj->m_fScale);
				}
			}
			else
			{
				fprintf(pFile, "rangescale \"%s\" \"%s\" %f %f %f %f\n",
				        pDeviceName, pObj->m_TriggerName, pObj->m_fScale,
				        pObj->m_fRangeScaleMin, pObj->m_fRangeScaleMax,
				        pObj->m_fRangeScalePreCenterOffset);
			}
		}
	}
}


// ======================================================================= //
//  THE TWO LIST FREES
//
//  Both are pure list walks, and both are the SDK structures from
//  src/LT2/sdk/inc/ltbasedefs.h with nothing local about them -- which is what
//  identifies them.  DeviceObject's next lands at 0xd0 (uint32 + char[128] +
//  uint32 + char[64] + 2 floats) and DeviceBinding's at 0xdc with its
//  GameAction list at 0xd8 (2 * char[100] + 4 floats), and GameAction's own
//  next at 0x2c (int + char[30] padded + 2 floats).  Every one of those
//  offsets is in the code.
// ======================================================================= //

// 0x00439e20 -- slot 19 (clientmgr.h's FreeDeviceObjects).
void input_FreeDeviceObjects(DeviceObject* pList)
{
	DeviceObject* pNext;

	while (pList)
	{
		pNext = pList->m_pNext;

		delete pList;

		pList = pNext;
	}
}


// 0x00439050 -- slot 14 (clientmgr.h's FreeDeviceBindings).  The inner loop
// frees each binding's action list first; both loops are plain `while`s with
// the pre-test VC6 puts in front of a rotated loop.
void input_FreeDeviceBindings(DeviceBinding* pBindings)
{
	DeviceBinding* pNextBinding;
	GameAction*    pAction;
	GameAction*    pNextAction;

	while (pBindings)
	{
		pAction = pBindings->pActionHead;

		while (pAction)
		{
			pNextAction = pAction->pNext;

			delete pAction;

			pAction = pNextAction;
		}

		pNextBinding = pBindings->pNext;

		delete pBindings;

		pBindings = pNextBinding;
	}
}


// 0x00439090 -- slot 13 (clientmgr.h's GetDeviceBindings).  The bindings
// of ONE device, as the SDK DeviceBinding/GameAction lists (0xe0 and 0x30
// bytes, each memset over after `new`), built by pushing on the FRONT, so
// they come out in reverse.  Keyboard is tested by equality and the rest by
// bit (0x0043909a vs 0x004390ba); nothing matched is a plain NULL.  Every
// object of the device gets a binding (its names copied 99 + terminator,
// its scale state copied), and every binding of the object that has an
// action gets a GameAction (name 29 + terminator).  An allocation failure
// frees what was built and returns NULL.
DeviceBinding* input_GetDeviceBindings(uint32 nDevice)
{
	DeviceBinding* pBindings = LTNULL;
	InputDevice*   pDevice = LTNULL;
	CDeviceObject* pos;
	CDeviceObject* pObject;
	DeviceBinding* pBinding;
	GameAction*    pActions;
	GameAction*    pAction;
	SInputBinding* pInput;

	if (nDevice == DEVICETYPE_KEYBOARD)
		pDevice = di_FindDeviceByType(DIDEVTYPE_KEYBOARD);
	else if (nDevice & DEVICETYPE_MOUSE)
		pDevice = di_FindDeviceByType(DIDEVTYPE_MOUSE);
	else if (nDevice & DEVICETYPE_JOYSTICK)
		pDevice = di_FindDeviceByType(DIDEVTYPE_JOYSTICK);
	else if (nDevice & DEVICETYPE_UNKNOWN)
		pDevice = di_FindDeviceByType(DIDEVTYPE_DEVICE);

	if (!pDevice)
		return LTNULL;

	for (pos = pDevice->m_Objects.m_pHead; pos != LTNULL; )
	{
		pObject = pDevice->m_Objects.GetNext(pos);

		pBinding = new DeviceBinding;
		if (!pBinding)
		{
			input_FreeDeviceBindings(pBindings);
			return LTNULL;
		}

		memset(pBinding, 0, sizeof(DeviceBinding));

		SAFE_STRCPY(pBinding->strDeviceName, pDevice->m_Name);
		SAFE_STRCPY(pBinding->strTriggerName, pObject->m_Name);

		pBinding->nScale                     = pObject->m_fScale;
		pBinding->nRangeScaleMin             = pObject->m_fRangeScaleMin;
		pBinding->nRangeScaleMax             = pObject->m_fRangeScaleMax;
		pBinding->nRangeScalePreCenterOffset = pObject->m_fRangeScalePreCenterOffset;

		pActions = LTNULL;

		for (pInput = pObject->m_pBindings; pInput; pInput = pInput->m_pNext)
		{
			if (pInput->m_pAction)
			{
				pAction = new GameAction;
				if (!pAction)
				{
					input_FreeDeviceBindings(pBindings);
					return LTNULL;
				}

				memset(pAction, 0, sizeof(GameAction));

				pAction->nActionCode = pInput->m_pAction->m_nCode;
				SAFE_STRCPY(pAction->strActionName, pInput->m_pAction->m_Name);

				pAction->nRangeLow  = pInput->m_fRangeLow;
				pAction->nRangeHigh = pInput->m_fRangeHigh;

				pAction->pNext = pActions;
				pActions = pAction;
			}
		}

		pBinding->pActionHead = pActions;
		pBinding->pNext       = pBindings;
		pBindings = pBinding;
	}

	return pBindings;
}


// ======================================================================= //
//  THE DRIVER'S ENTRY POINT
// ======================================================================= //

// 0x00439fb0 -- hand the caller the interface table and report success.
// Only nine of its thirteen bytes survive relocation masking, which is just
// over the eight-byte matching floor, so this one is verifiable -- but the
// unmasked part is `mov 0x4(%esp),%eax` plus `xor %eax,%eax; ret`, which is
// not distinctive.  Confirm its relocations before trusting it.
int input_GetInputMgr(InputMgr** ppMgr)
{
	*ppMgr = &g_InputMgr;

	return 0;
}


// 0x0048fab0 -- slot 4 (clientmgr.h's PlayJoystickEffect).  The shipped
// slot points at a shared `xor %eax,%eax; ret` well outside this module:
// this build's joystick-effect entry does nothing and reports failure, and
// the linker folded its body into the first identical one (the same address
// sits in eight .rdata vtables).  So this is the real function, not a stub.
LTBOOL input_PlayJoystickEffect(InputMgr* pMgr, char* pEffectName,
                                float x, float y)
{
	return LTFALSE;
}


// 0x004ab008 -- the interface table, in .data (this module's first data:
// the "----- Input devices -----" literal follows it at 0x004ab068).  The
// 24 shipped words, in slot order.
InputMgr g_InputMgr =
{
	(void*)input_Init,                       // 0x00  0x00436b40
	(void*)input_Term,                       // 0x04  0x00436940
	(void*)input_IsInitialized,              // 0x08  0x00436c70
	(void*)input_ListDevices,                // 0x0c  0x00436c80
	(void*)input_PlayJoystickEffect,         // 0x10  0x0048fab0
	(void*)input_ReadActions,                // 0x14  0x00437160
	(void*)input_FlushDeviceData,            // 0x18  0x00437b70
	(void*)input_ReadInput,                  // 0x1c  0x00437bc0
	(void*)input_AddAction,                  // 0x20  0x00437f50
	(void*)input_EnableDevice,               // 0x24  0x00436e80
	(void*)input_ClearBindings,              // 0x28  0x00438000
	(void*)input_AddTrigger,                 // 0x2c  0x004381a0
	(void*)input_ScaleTrigger,               // 0x30  0x00438bb0
	(void*)input_GetDeviceBindings,          // 0x34  0x00439090
	(void*)input_FreeDeviceBindings,         // 0x38  0x00439050
	(void*)input_StartDeviceTrack,           // 0x3c  0x00439270
	(void*)input_TrackDevice,                // 0x40  0x004395a0
	(void*)input_EndDeviceTrack,             // 0x44  0x00439a30
	(void*)input_GetDeviceObjects,           // 0x48  0x00439ab0
	(void*)input_FreeDeviceObjects,          // 0x4c  0x00439e20
	(void*)input_GetDeviceName,              // 0x50  0x00439e40
	(void*)input_IsDeviceEnabled,            // 0x54  0x00439ea0
	(void*)input_ListDeviceObjects,          // 0x58  0x00439ec0
	(void*)input_ListDeviceNames,            // 0x5c  0x00439f80
};


// ======================================================================= //
//  NOT THIS SUBSYSTEM -- the one-byte yaw, both directions
//
//  These two sit at 0x004361e0 and 0x004362c0, in servermsg.cpp's /O2
//  block (0x00435ad0 sub_1001307f and 0x00435d90 lmsg_ReadCompRotation are
//  their neighbours), and they are here only because this file names their
//  addresses in its header.  THE REAL HOME of the writer is
//  src/engine/server/smgr.cpp's sub_10013585 (server.dll 0x10013585, called
//  from lithtech.exe's copy of the object-update writer at 0x00464e87): the
//  same statements, but there the byte goes out through
//  ServerMessage::WriteByte, a stand-in for CPacket::WriteType<uint8> that
//  server.dll's /O1 calls out of line (0x1001393e) and this /O2 image
//  EXPANDS -- that is the whole 0x0043620b..0x004362ba tail.  The reader
//  has no server twin; its one caller is the client's object-update reader
//  at 0x0047a340.  When smgr.cpp's copy is respelled through the template,
//  the writer below is the one to drop.
// ======================================================================= //

// 0x00444880 -- sharedcommon.h's quat_ConvertToMatrix (server 0x10015bcd).
void quat_ConvertToMatrix(LTRotation* pRot, LTMatrix* pMat);

// 0x00434020 -- servermath.h's rot_SetupEuler, (pitch, yaw, roll, out).
void rot_SetupEuler(float x, float y, float z, LTRotation* pRot);


// 0x004361e0 -- the writer: servermsg.cpp's sub_10013585 (moved; its
// spelling there gives server.dll exactly and this image size-exact).


// 0x004362c0 (163 bytes).  The other end: one SIGNED byte (`movsbl` at
// 0x00436332) through the inlined CPacket::ReadType<int8>, scaled by
// 1/127 (0x004a1a80) and then by PI (0x004a2360) -- two `fmuls`, in that
// order -- into a pure-yaw rotation.  The parentheses are load-bearing:
// without them VC6 folds 1/127 * PI into one constant and one `fmuls`.
void cin_ReadYawByte(CPacket* pPacket, LTRotation* pRot)
{
	int8 nYaw;

	nYaw = pPacket->ReadType((int8*)0);

	rot_SetupEuler(0.0f, ((float)nYaw * (1.0f / 127.0f)) * MATH_PI, 0.0f, pRot);
}
