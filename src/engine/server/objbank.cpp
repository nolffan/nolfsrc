// server.dll — the per-object-type banks (reconstruction). Build /O1.
//
// Eleven banks sit side by side in .rdata at 0x1004de9c and up, each a
// four-slot vtable of the same shape: destructor, Allocate, Free, Init. Free
// is one function shared by all eleven — a pointer is a pointer, so the
// instantiations fold — while Allocate, Init and the destructor differ only
// in the size they bake in and the constructor they call. That is a template,
// instantiated once per object type.
//
// Which type each one belongs to is settled by the sizes: eleven banks, and
// ltbasedefs.h declares exactly eleven OT_ constants. The order matches, and
// the container's 0x274 is the confirmation — ContainerInstance's own code
// reads its container code at 0x270, so its object cannot be smaller and the
// next size up is the bank's.
//
// The class names below are ours, taken from those OT_ constants; the
// engine's own names for them are not recoverable. What is not a guess is the
// size of each and the constructor each Allocate runs.
//
// A freed object is destructed but not released: the vtable's slot zero is
// called with the "do not free" flag and the memory then goes onto the struct
// bank's free list, which is why an object type's storage never leaves its
// bank once claimed.
#include <new.h>
#include <math.h>
#include "servermgr.h"
#include "objbank.h"
#include "objtypes.h"
#include "commonmodel.h"
#include "de_error.h"
#include <string.h>

// This file's two memsets want OPPOSITE expansions in the server's /O1
// build and the same one in the client's /O2 build, and that is what the
// pragma placement below is for.
//
//   * The model constructor clears two eight-byte members with inline
//     `stosd` runs (`xor eax,eax; lea &m, %edi; stosd; stosd`, the zero
//     re-materialised for each) in BOTH images, which is what `memset`
//     compiles to only when it is intrinsic — a probe of every other
//     eight-byte zeroing form in this file (a value-initialised member, an
//     assignment from a default-constructed temporary, element-by-element
//     stores, a call) reproduces none of it. So it needs
//     `#pragma intrinsic(memset)`.
//   * The sprite constructor's twenty-byte clear is a CALL in server.dll
//     (0x100250cc pushes three arguments to 0x1003e280) and five inline
//     `mov`s through a base register in lithtech.exe (0x45b470). That is
//     exactly what plain `memset` gives with NO pragma at all: /O1 is
//     /Og /Os /Oy /Ob1 /Gs /GF /Gy and carries no /Oi, so the server's
//     build calls the library; /O2 carries /Oi, so the client's inlines.
//
// A pragma reaches forward from where it stands and there is no "restore
// the default", so the sprite's constructor is placed BEFORE the pragma and
// the model's after it — which is why the two constructors are not in
// declaration order below. `#pragma function(memset)` around the sprite
// instead costs lithtech.exe that constructor (85 bytes against 88), and
// dropping the pragma outright costs the model its size-exact 151.

// 0x1003e280 — and this is NOT that memset, which is why the sprite's
// twenty-byte clear is a call while the model's are inline. It returns its
// *fill value* (`mov 0x8(%esp),%eax` on the way out) where the CRT's returns
// its destination, and it early-outs on a zero length; it is the engine's own
// fill (name ours).
void* lt_MemSet(void* pDest, int nFill, uint32 nBytes);

// 0x1003d1f0 — hand a struct bank's chunks back (declared in filemgr.cpp's
// neighbourhood; the second argument is accepted and ignored).
void sb_FreeChunks(StructBank* pBank, int bAll);

// 0x1003d240 — add another chunk's worth of records to the free list (name
// ours; it lives outside the reconstructed region). structbank.cpp declares
// the same function.
void sb_Grow(StructBank* pBank, uint32 nCount);

// sb_Allocate now lives in servermgr.h as `inline`, which is where the
// original had it; the local copy this file carried was a workaround for
// its absence.



// The four types with recovered structure come from objtypes.h; the rest
// are still only their size and their constructor.
#define DECLARE_OBJTYPE(name, size)                    \
	class name                                         \
	{                                                  \
	public:                                            \
		name();                                        \
		virtual ~name();                               \
		uint8 m_pad[(size) - 4];                       \
	}


typedef CObjBase CObjNormal;                  // OT_NORMAL, 0x1a8

// The ILTSpriteControl implementation a sprite object carries at its +0x1c0:
// a vptr — the vtable at 0x1004e06c, whose six slots are spritecontrol.cpp's
// six methods (0x10035929 .. 0x10035a7b, in iltspritecontrol.h's order) —
// and a pointer back to the sprite it belongs to. An embedded object of its
// own, not a base: a second base would have to start at 0x1a8.
//
// This is spritecontrol.cpp's SpriteControl, and it carries that class's
// name and method list so that the vtable this file emits for it names the
// real bodies. That file types the back-pointer as its own SpriteInstance
// view; here it is the sprite object this file builds.
class SpriteControl
{
public:
	// Slot zero is NOT a destructor: the sprite's own destructor is eleven
	// bytes (vptr store, tail jump to the base) and destroys nothing, so
	// this object has none to run.
	virtual LTRESULT GetNumAnims(uint32& nAnims);                   // 0x10035929
	virtual LTRESULT GetNumFrames(uint32 iAnim, uint32& nFrames);   // 0x10035940
	virtual LTRESULT GetCurPos(uint32& iAnim, uint32& iFrame);      // 0x1003599a
	virtual LTRESULT SetCurPos(uint32 iAnim, uint32 iFrame);        // 0x100359d8
	virtual LTRESULT GetUnknown1b8(uint32& val);                    // 0x10035a67
	virtual LTRESULT SetUnknown1b8(uint32 val);                     // 0x10035a7b

public:
	class CObjSprite* m_pInstance;           // +0x04
};

// OT_SPRITE, 0x1c8 bytes. The play state spritecontrol.cpp reads out — the
// resource at +0x1a8, the current anim, the current frame and the
// milliseconds into it — is cleared wholesale by one twenty-byte memset
// rather than field by field, which is why the region is one block here and
// not five named pointers; the dword immediately past it starts at -1.
// The AnimTimeRef inside a model's own animation tracker. servercommon.h
// models the same structure as CAnimTimeRef with its constructor declared and
// defined out of line, because that is what the *save* code's copy compiles
// to; here the constructor is expanded inline (`orl $-1,0x1f4` for the
// weight set, then a call to the out-of-line Clear at 0x100175fd), which is
// the SDK's in-class definition. Both shapes ship, so both are written.
// ltanimtracker.h's LTAnimTracker and AnimTimeRef, and commonmodel.h's
// ModelWalker, are the real types for everything below and this file used to
// carry local views of all three. They were folded once objbank.cpp could
// include commonmodel.h: the SDK's LTAnimTracker agrees field for field with
// the view that was here (LTLink at 0x00, the callback, m_pUser1, m_CurKey,
// m_Flags, m_InterpolationMS, AnimTimeRef at 0x1c), its GetNext() is exactly
// the cast the walks below were doing by hand, and its SetupTimeRef is the
// declared name for 0x10001000.
//
void at_Init(LTAnimTracker* pTracker, class Model* pModel,
             uint32 iAnim);

// OT_MODEL, 0x248 bytes. commonmodel.h models the same object from the
// animation side as ModelInstance and agrees offset for offset: the hook at
// +0x1b4, the tracker at +0x1c0, the extra-tracker list at +0x1f8 and the
// hidden-piece mask at +0x204.
class CObjModel : public CObjBase
{
public:
	CObjModel();                             // 0x10024ea7

	virtual ~CObjModel();

	// 0x10024f3e — the base's slot 0x2c, overridden: the model's own radius,
	// unscaled.
	virtual float _v2c();

	// CObjModel's own four virtuals, at slots 0x38 .. 0x44. CObjBase stops at
	// 0x34 -- the vftables of CObjCamera and CObjCanvas are exactly 0x38
	// long, which is what fixes the base's count at fourteen.
	virtual void   SetupModelWalker(class ModelWalker* pOut);   // 0x10024fda
	virtual void   SetupModelWalkerRefsOnly(class ModelWalker* pOut); // 0x10025035
	virtual LTBOOL FindTracker(LTAnimTracker* pTracker,
	                               LTAnimTracker*** pppLink); // 0x10025074
	virtual float  GetScaledRadius();                             // 0x10024f4b

	// Not in the vftable: called directly.
	uint32 GetTrackerCount();                // 0x100250a4
	char*  GetFilename();                    // 0x100250b7

public:
	void*             m_pUnknown1a8;         // 0x1a8  not touched here
	void*             m_pUnknown1ac;         // 0x1ac
	void*             m_pUnknown1b0;         // 0x1b0
	void*             m_pHookFn;             // 0x1b4
	void*             m_pUnknown1b8;         // 0x1b8
	void*             m_pUnknown1bc;         // 0x1bc
	LTAnimTracker m_AnimTracker;         // 0x1c0
	LTAnimTracker* m_pTrackerList;       // 0x1f8
	uint32            m_Unknown1fc;          // 0x1fc
	uint32            m_Unknown200;          // 0x200
	uint32            m_HiddenPieces;        // 0x204
	void*             m_pUnknown208;         // 0x208
	void*             m_pUnknown20c;         // 0x20c
	uint8             m_pad_210[0x244 - 0x210];
	float             m_f244;                // 0x244  starts at minus one
};

// OT_PARTICLESYSTEM, 0x25c bytes. Two of its pointers start at the address
// of its own storage at +0x1a8 — one `lea`, stored twice — and the two float
// constants that are not zero or one are stored as integer immediates, which
// is the cheaper encoding once `fldz`/`fld1` are unavailable.
class CObjParticleSystem : public CObjBase
{
public:
	CObjParticleSystem();                    // 0x10025229

	// 0x100253d0 — slot 0x20: the base's parameter copy, then a pointer into
	// the manager at its +0x14. A different offset from the line system's
	// +0x30, so the manager hands each type its own sub-object.
	virtual void _v20(class CAttachMgrView* pAttachMgr,
	                  struct ObjectCreateStruct* pStruct);

	virtual ~CObjParticleSystem();          // 0x10025349

public:
	uint8  m_Data1a8[0x1c8 - 0x1a8];         // 0x1a8
	void*  m_p1c8;                           // 0x1c8  -> m_Data1a8
	uint8  m_pad_1cc[0x1e0 - 0x1cc];
	void*  m_p1e0;                           // 0x1e0  -> m_Data1a8
	void*  m_p1e4;                           // 0x1e4  -> pManager + 0x14
	uint32 m_u1e8;                           // 0x1e8
	uint8  m_u1ec;                           // 0x1ec
	uint8  m_u1ed;                           // 0x1ed
	uint8  m_u1ee;                           // 0x1ee
	uint8  m_pad_1ef[0x1f0 - 0x1ef];
	uint32 m_u1f0;                           // 0x1f0
	uint8  m_pad_1f4[0x208 - 0x1f4];
	float  m_f208;                           // 0x208
	float  m_f20c;                           // 0x20c
	float  m_f210;                           // 0x210
	float  m_f214;                           // 0x214  starts at one
	float  m_f218;                           // 0x218
	float  m_f21c;                           // 0x21c
	float  m_f220;                           // 0x220
	float  m_f224;                           // 0x224  starts at one
	uint32 m_u228;                           // 0x228
	uint32 m_u22c;                           // 0x22c
	// 0x230 and 0x23c are two LTVectors, not six floats: the destructor
	// resets them through LTVector::Init, whose inlined `this` is the `lea`
	// the shipped code forms before the three stores. They are a bounding
	// box -- reset to +1e37 and -1e37, an empty one.
	LTVector m_vBoxMin;                      // 0x230
	LTVector m_vBoxMax;                      // 0x23c
	float  m_f248;                           // 0x248  starts at -500
	float  m_f24c;                           // 0x24c  starts at 300
	int    m_n250;                           // 0x250  starts at -1
	int    m_n254;                           // 0x254  starts at -1
	uint32 m_u258;                           // 0x258
};

// OT_POLYGRID, 0x11f8 bytes — nearly all of which is the grid itself and
// none of which the constructor touches. The link at +0x1e4 is tied off and
// given no data, so it is a list *head* and not the object's own link
// (`mov %eax,0x4(%eax)` then `mov %eax,(%eax)` is CheapLTLink::TieOff, whose
// chained assignment writes m_pNext before m_pPrev).
class CObjPolyGrid : public CObjBase
{
public:
	CObjPolyGrid();                          // 0x100253f0

	virtual ~CObjPolyGrid();

	// CObjPolyGrid's one own virtual, at slot 0x38: its vftable at 0x1004e134
	// is fifteen slots and the two dwords after it are the float constants
	// this uses.
	virtual float GetBoundingRadius();       // 0x10025461

public:
	void*  m_pUnknown1a8;                    // 0x1a8
	void*  m_pUnknown1ac;                    // 0x1ac
	void*  m_pUnknown1b0;                    // 0x1b0
	uint8  m_pad_1b4[0x1c8 - 0x1b4];
	uint32 m_Unknown1c8;                     // 0x1c8
	float  m_f1cc;                           // 0x1cc
	float  m_f1d0;                           // 0x1d0
	float  m_f1d4;                           // 0x1d4  starts at one
	float  m_f1d8;                           // 0x1d8  starts at one
	void*  m_pUnknown1dc;                    // 0x1dc
	void*  m_pUnknown1e0;                    // 0x1e0
	LTLink m_Link1e4;                        // 0x1e4  a tied-off head
	uint32 m_Unknown1f0;                     // 0x1f0
	uint32 m_Unknown1f4;                     // 0x1f4
	uint8  m_pad_1f8[0x11f8 - 0x1f8];
};

class CObjSprite : public CObjBase
{
public:
	CObjSprite();                            // 0x100250cc

	// CObjSprite's one own virtual, at slot 0x38 of its vftable at
	// 0x1004e030; the 363.0f it multiplies by sits just past the sprite
	// control's vftable that follows.
	virtual float GetScaledRadius();         // 0x1002511f

	virtual ~CObjSprite();

public:
	uint8              m_Data1a8[0x1bc - 0x1a8];   // 0x1a8
	int                m_n1bc;                     // 0x1bc  starts at -1
	SpriteControl      m_Control;                  // 0x1c0
};

// Two more banks over things that are not object types at all — the same
// template, reached through vtables of its own at 0x1004e2f0 and 0x1004e358.
// The larger is 0x54 bytes, which is sizeof(ServerMessage); what the smaller
// holds is not recoverable from the bank alone (names ours).
// Neither is polymorphic — which the release says, not the bank: the eleven
// object types are destructed through their vtables, the 0x54 element by a
// direct call, and the 0x14 element not at all.
class CBankElem54
{
public:
	virtual ~CBankElem54();
	uint8 m_pad[0x54 - 4];
};

class CBankElem14
{
public:
	uint8 m_pad[0x14];
};

// Five more banks, found by walking `.rdata` for the four-slot vtable shape
// (deleting destructor, Allocate, Free, Init) rather than from any call site.
// Each is a distinct instantiation in the shipped image with a vtable of its
// own, and the element size is baked into its Init's `sb_Init` — which is
// what makes them separable from one another at all, since everything else
// in a bank is either shared or relocation:
//
//   0x1004e8b4  0x0c  Init 0x10034e7b  dtor 0x10034e96
//   0x1004d638  0x18  Init 0x1000dea0  dtor 0x1000debb
//   0x1004d648  0x1c  Init 0x1000ed7a  dtor 0x1000ed95
//   0x1004d8dc  0x20  Init 0x10015b4a  dtor 0x10015b9d
//   0x1004dd8c  0x70  Init 0x10024242  dtor 0x10024291
//
// The first four share the type-independent Free at 0x10015ac1 and the same
// Allocate (0x1000de9b, a five-byte tail jump onto 0x1000ed6f), so their
// elements have neither constructor nor destructor and are modelled as raw
// bytes. The 0x70 bank has an Allocate and a Free of its own (0x10023fd6 /
// 0x10023fdb), which says that element *is* non-trivial — so only its Init
// is reconstructed by the instantiation below, and its Allocate and Free
// remain open work.
//
// What the elements are is not recoverable from the banks; the names, as
// with CBankElem14 and CBankElem54 above, are the sizes.
class CBankElem0c
{
public:
	uint8 m_pad[0x0c];
};

class CBankElem18
{
public:
	uint8 m_pad[0x18];
};

class CBankElem1c
{
public:
	uint8 m_pad[0x1c];
};

class CBankElem20
{
public:
	uint8 m_pad[0x20];
};

class CBankElem70
{
public:
	uint8 m_pad[0x70];
};

// A second bank template, found the same way: 0x1004d7d8 (0x0c) and
// 0x1004d250 (0x24) have the same four slots, but their Init brackets the
// two bank calls with LCriticalSection::Enter/Leave (0x100155aa /
// 0x100155b2) on an object at the bank's own +0x20 — exactly where
// CObjectBank leaves m_Unknown20 — and their destructors run
// ~LCriticalSection (0x100155a2) there as well. So the plain bank's spare
// word is this one's lock, and the two templates are otherwise the same
// shape.
//
// The 0x24 one is already written, by hand and under its real name:
// fsstream.cpp's CStreamBank, whose element is a CTreeStream. That is the
// independent check on this shape — the template instantiated at 0x24 comes
// out byte-for-byte as that file's hand-written class — so only the 0x0c
// instantiation is added here, for 0x1001412d.
//
// The lock is lthread.h's LCriticalSection (servermgr.h already brings it
// in): Enter, Leave and the destructor are the three calls named above.


// Each object type has a destructor of its own; the deleting form the bank
// reaches through slot zero is what these emit, and it is identical for all
// eleven but for which body it calls.
CObjModel::~CObjModel() {}

// 0x10001000 — the generated copy assignment, reached through a member.
void LTAnimTracker::SetupTimeRef(AnimTimeRef* pOut)
{
	*pOut = m_TimeRef;
}

// 0x10024f3e — slot 0x2c: the model's own radius, straight off the model the
// tracker's time reference points at.
float CObjModel::_v2c()
{
	return m_AnimTracker.m_TimeRef.m_pModel->m_f0a8;
}

// 0x10024f4b — slot 0x44: the same radius scaled by the object's largest
// scale component.
//
// The max is LTMAX nested, not a three-way compare, and the shipped bytes
// prove it: `LTMAX(m_Scale.y, m_Scale.z)` is computed TWICE, once for the
// comparison against x and once again in the arm where x loses. That is what
// the macro expansion does, since the outer LTMAX names its second argument
// in both arms of its own ternary. A helper function or a temporary would
// each have computed it once.
float CObjModel::GetScaledRadius()
{
	return LTMAX(m_Scale.x, LTMAX(m_Scale.y, m_Scale.z))
	       * m_AnimTracker.m_TimeRef.m_pModel->m_f0a8;
}

// 0x10024fda — slot 0x38: copy every extra tracker's time reference into the
// gather buffer, then the two fields at 0xec and 0xf0.
//
// Overflowing the buffer returns EARLY, so the eight-reference case skips the
// two trailing fields entirely -- the shipped `jae` goes to the epilogue and
// the exhausted-list path falls into the copies instead.
void CObjModel::SetupModelWalker(ModelWalker* pOut)
{
	LTAnimTracker* pTracker;

	pOut->m_Unknown_e0 = 0;

	for (pTracker = m_pTrackerList; pTracker;
	     pTracker = pTracker->GetNext())
	{
		if (pOut->m_Unknown_e0 >= 8)
			return;

		pTracker->SetupTimeRef(&pOut->m_TimeRefs[pOut->m_Unknown_e0]);
		pOut->m_Unknown_e0++;
	}

	pOut->m_Unknown_ec = m_Unknown1fc;
	pOut->m_Unknown_f0 = m_Unknown200;
}

// 0x10025035 — slot 0x3c: the same gather without the two trailing fields,
// so here the overflow and the exhausted list share one exit.
void CObjModel::SetupModelWalkerRefsOnly(ModelWalker* pOut)
{
	LTAnimTracker* pTracker;

	pOut->m_Unknown_e0 = 0;

	for (pTracker = m_pTrackerList; pTracker;
	     pTracker = pTracker->GetNext())
	{
		if (pOut->m_Unknown_e0 >= 8)
			return;

		pTracker->SetupTimeRef(&pOut->m_TimeRefs[pOut->m_Unknown_e0]);
		pOut->m_Unknown_e0++;
	}
}

// 0x10025074 — slot 0x40: find a tracker in the extra-tracker list and hand
// back the address of the POINTER that reaches it, which is what an unlink
// needs. The walk therefore starts at &m_pTrackerList rather than at the
// first tracker, and the out parameter is written before each hop, so a
// caller that gets LTFALSE still holds the address of the last link.
LTBOOL CObjModel::FindTracker(LTAnimTracker* pTracker,
                                  LTAnimTracker*** pppLink)
{
	// The guard tests POSITIVELY and the failure return is the last thing in
	// the function, because the empty-list guard and the walked-off-the-end
	// case share one `xor %eax,%eax`. Written as an early `if (!list) return
	// LTFALSE;` VC6 hosts that block at its first mention instead and the
	// initial branch inverts -- same size, same instruction count, different
	// bytes.
	if (m_pTrackerList)
	{
		LTAnimTracker** ppCur = &m_pTrackerList;

		for (;;)
		{
			LTAnimTracker* pCur;

			*pppLink = ppCur;

			pCur = *ppCur;

			if (!pCur)
				break;
			if (pCur == pTracker)
				return LTTRUE;

			ppCur = (LTAnimTracker**)&pCur->m_Link.m_pNext;
		}
	}

	return LTFALSE;
}

// 0x100250a4 — how many extra trackers the model carries.
uint32 CObjModel::GetTrackerCount()
{
	uint32 nCount = 0;
	LTAnimTracker* pTracker = m_pTrackerList;

	while (pTracker)
	{
		pTracker = pTracker->GetNext();
		nCount++;
	}

	return nCount;
}

// 0x100250b7 — the model file's name, or the shared empty string when there
// is no model. A tail call into Model::GetFilename.
char* CObjModel::GetFilename()
{
	Model* pModel = m_AnimTracker.m_TimeRef.m_pModel;

	// Positive test again: the tail jump into Model::GetFilename is the
	// FALLTHROUGH and the empty string is hosted after it.
	if (pModel)
		return pModel->GetFilename();

	return "";
}


CObjSprite::~CObjSprite() {}

// 0x100250cc — three of the base's four colour stores again (not the alpha,
// exactly as CObjWorldModel::InitFields repeats them), the play state
// cleared, and the control object pointed back at its owner.
//
// The twenty-byte clear is a CALL here and five inline stores in
// lithtech.exe's copy, which is plain `memset` with no pragma in force —
// see the note at the head of this file for why that puts this constructor
// ahead of the model's.
CObjSprite::CObjSprite() : CObjBase(OT_SPRITE)
{
	m_ColorB = 255;
	m_ColorG = 255;
	m_ColorR = 255;

	memset(m_Data1a8, 0, sizeof(m_Data1a8));

	m_n1bc = -1;

	m_Control.m_pInstance = this;
}


#pragma intrinsic(memset)

// 0x10024ea7.
//
// NEAR MATCH (151 of 151 bytes, 44 of 44 instructions): the whole function
// agrees instruction for instruction, and the only difference is where the
// first block fill's address materialises -- the shipped build emits
// `lea 0x208(%esi),%edi` / `xor %eax,%eax` immediately before the last
// tracker store, ours hoists the pair to the moment %edi falls dead and
// emits it in the other order.
//
// The store order is lithtech.exe's (0x0045b1f2: 1fc 200 204 1b4, THEN the
// tracker list at 1f8, then the link); server.dll's /O1 schedules the 1f8
// store up to the call (0x10024ef0), so it is no witness. lithtech.exe also
// says what is still missing: its 208 fill stores through `0x208(%esi)`
// directly (0x0045b21e) where ours goes through a `lea` into %ecx, and VC6
// only folds an intrinsic memset's address when the member was referenced
// earlier in the function -- delete the early `m_pUnknown1b8 = LTNULL` and
// the 1b8 fill grows a `lea` too; add an early `m_pUnknown208 = LTNULL` and
// lithtech's tail becomes byte-identical plus that one extra 6-byte store.
// So the original touched m_pUnknown208 before this point in some way that
// emits no code. Not found: a discarded read, an empty `if`, a redundant
// store beside the fill (kept, not eliminated), an array or struct type for
// the pair, a trivially-constructed member type, and moving the fill
// earlier, all leave it as it is or worse. Also unchanged on both images:
// `&m_pUnknown208` taken into a dead local before at_Init, the fill through
// a `void**` local, `m_pUnknown208 = m_pUnknown208`, `(char*)this + 0x208`,
// `&m_HiddenPieces + 1`, `&this->m_...`, and `sizeof(void*) * 2`; the 208
// fill at each of the six other positions after at_Init is worse on
// server.dll (13-22 differing instructions or 152/153 bytes) and still
// `lea`s on lithtech.exe; the tracker and link stores through an
// `LTAnimTracker*` local are 145 / 196 bytes.
CObjModel::CObjModel() : CObjBase(OT_MODEL)
{
	m_pUnknown1ac = LTNULL;
	m_pUnknown1b0 = LTNULL;
	m_pUnknown1b8 = LTNULL;

	at_Init(&m_AnimTracker, LTNULL, LTFALSE);

	m_Unknown1fc   = 0;
	m_Unknown200   = 0;
	m_HiddenPieces = 0;

	m_pHookFn = LTNULL;

	m_pTrackerList = &m_AnimTracker;

	m_AnimTracker.m_Link.m_pNext = LTNULL;
	m_AnimTracker.m_Link.m_pData = this;

	memset(&m_pUnknown208, 0, 8);
	memset(&m_pUnknown1b8, 0, 8);

	m_f244 = -1.0f;
}

// 0x10025229.
//
// Byte-identical on server.dll. What closed it was m_u1e8's position: it is
// the FIRST store in the source, ahead of the two -1 stores. That is what
// makes the shipped build materialise the zero into %ecx at the top (next to
// `mov $0xff,%al`) and keep it there for all six integer zero stores; with
// m_u1e8 in its declaration-order place, down among the other zeros, VC6
// deferred the constant past the `lea`, reused %eax for it, and scheduled
// the second `orl $-1` and both large-immediate float stores late (21
// differing instructions). lithtech.exe's /O2 copy (0x0045b610) agrees on
// this too -- its zero stores open with 0x1e8 -- but still groups its
// constant stores in an order no source order tried here reproduces
// (1.0, 0, 255, -1, 300/-500, then the `lea`), so it stays a size-exact
// near there; moving the -1 pair or the 258/1e4/1f0 trio leaves both
// images exactly where they were.
CObjParticleSystem::CObjParticleSystem() : CObjBase(OT_PARTICLESYSTEM)
{
	m_u1e8 = 0;

	m_n254 = -1;
	m_n250 = -1;

	m_ColorB = 255;
	m_ColorG = 255;
	m_ColorR = 255;

	m_u1ee = 255;
	m_u1ed = 255;
	m_u1ec = 255;

	m_vBoxMin.z = 0.0f;
	m_vBoxMin.y = 0.0f;
	m_vBoxMin.x = 0.0f;

	m_vBoxMax.z = 0.0f;
	m_vBoxMax.y = 0.0f;
	m_vBoxMax.x = 0.0f;

	m_p1e0 = m_Data1a8;
	m_p1c8 = m_Data1a8;

	m_f210 = 0.0f;
	m_f20c = 0.0f;
	m_f208 = 0.0f;
	m_f214 = 1.0f;

	m_f24c = 300.0f;
	m_f248 = -500.0f;

	m_u258 = 0;
	m_p1e4 = LTNULL;
	m_u1f0 = 0;

	m_f220 = 0.0f;
	m_f21c = 0.0f;
	m_f218 = 0.0f;
	m_f224 = 1.0f;

	m_u228 = 0;
	m_u22c = 0;
}

// 0x100253f0.
CObjPolyGrid::CObjPolyGrid() : CObjBase(OT_POLYGRID)
{
	m_Link1e4.TieOff();

	// The source order is the one server.dll's x87 stream keeps (0x10025406:
	// 1d0, 1cc, 1d8, 1d4 -- /O1 does not reorder FP stores among
	// themselves). lithtech.exe's /O2 copy (0x0045b806) turns the float
	// constants into integer stores and schedules m_f1d8 up to the top, next
	// to the `mov $0x3f800000,%ecx` it hoists, so it is not a source-order
	// witness for that one store.
	m_pUnknown1a8 = LTNULL;
	m_pUnknown1ac = LTNULL;
	m_pUnknown1b0 = LTNULL;

	m_f1d0 = 0.0f;
	m_f1cc = 0.0f;
	m_f1d8 = 1.0f;
	m_Unknown1c8  = 0;
	m_pUnknown1dc = LTNULL;
	m_pUnknown1e0 = LTNULL;
	m_Unknown1f4  = 0;
	m_Unknown1f0  = 0;

	m_f1d4 = 1.0f;
}
// One particle as the destructor reads it: the bank's intrusive free link at
// +0x00 and the next-in-list at +0x20 (name ours).
struct CParticle
{
	void*            m_pNextFree;            // +0x00  sb_Free's link
	uint8            m_pad_04[0x20 - 0x04];
	struct CParticle* m_pNext;               // +0x20
};

// 0x10025349 — the line system's destructor again, on a different type.
//
// Every particle goes back to a bank, the list terminator is the ADDRESS of
// the object's own storage rather than a null, and both cursors are reset to
// it afterwards. What differs is where the bank comes from: the line system
// keeps its own pointer at 0x1a8, and this one recomputes m_pAttachMgr + 0x14
// each time round -- the same +0x14 its _v20 stored into m_p1e4, which it does
// not use here.
//
// The whole walk is guarded on m_pAttachMgr, so a particle system whose
// creation never ran leaks nothing and crashes on nothing.
CObjParticleSystem::~CObjParticleSystem()
{
	m_vBoxMin.Init(1.0e+37f, 1.0e+37f, 1.0e+37f);
	m_vBoxMax.Init(-1.0e+37f, -1.0e+37f, -1.0e+37f);

	m_u1e8 = 0;

	if (m_pAttachMgr)
	{
		CParticle* pSentinel = (CParticle*)m_Data1a8;
		CParticle* pCur = (CParticle*)m_p1c8;

		while (pCur != pSentinel)
		{
			CParticle* pNext = pCur->m_pNext;

			sb_Free((StructBank*)((uint8*)m_pAttachMgr + 0x14), pCur);

			pCur = pNext;
		}
	}

	m_u228 = 0;
	m_p1e0 = m_Data1a8;
	m_p1c8 = m_Data1a8;
}

// 0x100253d0 — slot 0x20.
void CObjParticleSystem::_v20(CAttachMgrView* pAttachMgr,
                              ObjectCreateStruct* pStruct)
{
	CObjBase::_v20(pAttachMgr, pStruct);

	m_p1e4 = (uint8*)pAttachMgr + 0x14;
}

// 0x1002511f — slot 0x38: a sprite's radius. TWO axes, not three -- a sprite
// is flat, so only x and y are consulted, and there is a single LTMAX rather
// than the nested pair CObjModel::GetScaledRadius needs. That also means no
// term is computed twice here.
float CObjSprite::GetScaledRadius()
{
	return LTMAX(m_Scale.x, m_Scale.y) * 363.0f;
}

// 0x10025461 — slot 0x38: the radius of the grid's scaled box, plus one.
//
// The half-extents are the grid's width and height SHIFTED right by one, not
// divided, and both shifts are unsigned -- which is also what proves the two
// fields are uint32: VC6 widens each to a 64-bit temporary with the high dword
// forced to zero and converts with `fildll`, which is exactly its unsigned
// int-to-float sequence. A signed field would have been one `fild`.
//
// The middle axis has no field at all: it is a flat 128.0f times the scale,
// so the grid is a fixed height and only its footprint varies.
//
// The three terms are evaluated x, y, z but SQUARED and summed z, y, x --
// three values live on the x87 stack at once and the first square taken is of
// the last one pushed. Writing the sum in source order costs the match.
//
// The root is a NAMED float local, added to afterwards: that is what lets
// the first of the two `pop ecx` releasing sqrt's double argument land BEFORE
// the `fadds` (0x100254c2). With the add nested in the return expression
// both pops sink after it; the cast's position and a reversed add
// (`1.0f + ...`) do not move it.
float CObjPolyGrid::GetBoundingRadius()
{
	float fx = (float)(m_Unknown1f0 >> 1) * m_Scale.x;
	float fy = m_Scale.y * 128.0f;
	float fz = (float)(m_Unknown1f4 >> 1) * m_Scale.z;

	// The cast is INSIDE the add, not around it: `(float)(sqrt(..) + 1.0f)`
	// promotes the literal to double and gets `faddl`, where the shipped code
	// adds a 32-bit constant.
	float fRadius = (float)sqrt(fz * fz + fy * fy + fx * fx);
	return fRadius + 1.0f;
}

// 0x100254e8 — the grid's two owned buffers go back to the engine allocator
// and the fields are cleared, which is why each release is followed by a
// reload through a `lea` of the field's own address.
CObjPolyGrid::~CObjPolyGrid()
{
	if (m_pUnknown1a8)
	{
		dfree(m_pUnknown1a8);
		m_pUnknown1a8 = LTNULL;
	}

	if (m_pUnknown1ac)
	{
		dfree(m_pUnknown1ac);
		m_pUnknown1ac = LTNULL;
	}
}

CBankElem54::~CBankElem54() {}


// The locking bank. Only Init and the destructor are written: its Allocate
// and Free are not the shared ones (0x1001409c / 0x100140a1 and 0x100075b5 /
// 0x100075ba), and the Free forwards to a *cdecl* function of one argument
// that never sees the bank — so whatever these banks hand out is returned to
// something global, and reconstructing that is separate work. Leaving the
// two slots pure keeps the class abstract and emits nothing for them.
template<class T>
class CLockedObjectBank : public CObjectBankBase
{
public:
	virtual ~CLockedObjectBank()
	{
		sb_FreeChunks(&m_Bank, 1);
	}

	virtual void Init()
	{
		m_Lock.Enter();

		sb_FreeChunks(&m_Bank, 1);
		sb_Init(&m_Bank, sizeof(T), 0x20);

		m_Lock.Leave();
	}

public:
	StructBank m_Bank;                       // +0x04
	LCriticalSection m_Lock;                 // +0x20
};


// The eleven object banks need no instantiation of their own: CObjMgrView's
// constructor below builds all eleven, and the vtables it stores bring every
// slot with them. (Instantiated here as well, each also emitted a
// non-deleting destructor the shipped image does not have.)
//
// The banks of raw records are built nowhere in this file; what matters for
// them is their slot bodies, each the one shipped body its record size
// gives.
template class CObjectBank<CBankElem54>;
template class CObjectBank<CBankElem14>;
template class CObjectBank<CBankElem0c>;
template class CObjectBank<CBankElem18>;
template class CObjectBank<CBankElem1c>;
template class CObjectBank<CBankElem20>;
template class CObjectBank<CBankElem70>;
template class CLockedObjectBank<CBankElem0c>;





// *********************************************************************** //
//  Three functions from the *neighbouring* translation unit.
//
//  0x1002592e, 0x10025984 and 0x10025a1b sit between un390_Term (0x100257f2,
//  objtypes.cpp) and the bank Free/Init run above (0x10025ae7 and up), so by
//  address they belong to objtypes.cpp — the file that already models the
//  0x390 manager. They are reconstructed here because that file is not ours
//  to edit; the views below duplicate objtypes.cpp's on purpose and should
//  be folded into it when the two files meet.
// *********************************************************************** //

// The four LTObject fields these reach, at their own offsets. The link is
// the one the per-type object lists hold the object on (ltobject_internal.h
// leaves 0x7c inside m_pad_07c), and the type is a *signed* char — both uses
// sign-extend it, and the bound test against eleven is a signed `jge`.
class CObjLinked
{
public:
	uint8             m_pad_00[0x7c];
	LTLink            m_Link7c;              // 0x07c
	uint8             m_pad_88[0x098 - 0x088];
	class Attachment* m_pAttachments;        // 0x098  singly linked
	uint8             m_pad_9c[0x0b8 - 0x09c];
	uint16            m_ObjectID;            // 0x0b8
	uint8             m_pad_ba[0x0bc - 0x0ba];
	char              m_ObjectType;          // 0x0bc  OT_*
};

// The 0x390 manager, as far as these three need it: the eleven bank
// pointers and the eleven per-type lists. Named apart from servermgr.h's
// CUnknown390 (which is a four-byte stand-in) and objtypes.cpp's full view.
// THE TWO TABLES AT OFFSET ZERO, and why neither is a C++ vptr here.
//
// The constructor at 0x1002449b writes offset zero TWICE: 0x1004df68 before
// the eleven banks (0x100244ac) and 0x1004deb0 after them (0x1002460c).
// 0x1004df68 is four slots, the first two of them _purecall (0x1003ea24);
// 0x1004deb0 is two, Lock (0x1002461c) and a getter folded onto
// Model::GetFilename. No derived table is shorter than the one it replaces,
// so these are not a base/derived pair, and the destructor at 0x1002ca7c
// settles it from the other side: a polymorphic class restores its own
// vftable at the top of its destructor, and that function has no store to
// (%esi) at all.
//
// So the pointer is modelled as plain data, the way servermgr.h's
// CUnknown8FC already models its own. The FIRST store still cannot live in
// the constructor's body -- it has to precede eleven member constructors --
// so it goes in a one-member base whose inline constructor is nothing else;
// /O1 implies /Ob1 and expands it in place. Written instead as a real
// virtual on CObjMgrView, VC6 drops the constructor's vptr store as dead
// (the body overwrites it) and keeps the destructor's, which is exactly
// backwards: 379 of 385 and 291 of 285, six bytes each way.
//
// CORRECTION to the slot count above: 0x1004df68 is only TWO slots. The two
// after them (0x10024690, 0x10024afc) start the next table, CObjBase's at
// 0x1004df70, whose constructors store exactly that address. So both tables
// are two slots, pure then {Lock, lock-depth getter}, which does read like
// an abstract base and its implementation; the destructor argument above is
// what still stands against a real vptr. Both tables are defined in
// objtypes.cpp.
extern void* const g_ObjMgrVtblA[];          // 0x1004df68  objtypes.cpp

class CObjMgrVtblBase
{
public:
	CObjMgrVtblBase() { m_pVtbl = (void*)g_ObjMgrVtblA; }

	void*                      m_pVtbl;        // +0x000
};

class CObjMgrView : public CObjMgrVtblBase
{
public:
	CObjMgrView();                             // 0x1002449b

	int32                      m_LockCount;    // +0x004
	LTLink                     m_Link;         // +0x008
	uint8                      m_pad_14[0x4c - 0x14];
	// The 0x28-byte records the attachments come out of; its own intrusive
	// free list at +0x18 (manager +0x64) is where a removed one goes back.
	StructBank                 m_Bank4c;       // +0x04c
	// The eleven per-object-type banks, which used to be a pad here.
	CObjectBank<CObjNormal>         m_Normal;          // 0x068
	CObjectBank<CObjModel>          m_Model;           // 0x08c
	CObjectBank<CObjWorldModel>     m_WorldModel;      // 0x0b0
	CObjectBank<CObjSprite>         m_Sprite;          // 0x0d4
	CObjectBank<CObjLight>          m_Light;           // 0x0f8
	CObjectBank<CObjCamera>         m_Camera;          // 0x11c
	CObjectBank<CObjParticleSystem> m_ParticleSystem;  // 0x140
	CObjectBank<CObjPolyGrid>       m_PolyGrid;        // 0x164
	CObjectBank<CObjLineSystem>     m_LineSystem;      // 0x188
	CObjectBank<CObjContainer>      m_Container;       // 0x1ac
	CObjectBank<CObjCanvas>         m_Canvas;          // 0x1d0
	CObjectBankBase* m_pBanks[11];             // +0x1f4
	LTList                     m_Lists[11];    // +0x220

	~CObjMgrView();                            // 0x1002ca7c
};

// The eleven banks in place, so their destructor can be written.
//
// They are members of the sub-object at CServerMgr+0x390, at +0x68 and every
// 0x24 after it, in OT_ order. objtypes.cpp ALREADY had that -- its own view
// of the same object carries `CObjBankSlot m_Banks[11]; // 0x068` -- so the
// location is confirmation rather than discovery; what the destructor at
// 0x1002ca7c adds is the ORDER (it walks them backwards from 0x1d0 to 0x68,
// which is reverse declaration order) and the fact that they are eleven
// DISTINCT types: each restores a different four-slot vftable, 0x10 apart,
// which an array of one type could not produce and which is why
// objtypes.cpp's opaque CObjBankSlot stand-in is right to be opaque.
//
// A third view rather than an extension of servermgr.h's CUnknown390 stub,
// for two reasons: the bank template lives in this file and servermgr.h
// cannot see it, and this way CServerMgr's own layout is untouched. The
// wide view with the lock, the three struct banks and the eleven per-type
// object lists is objtypes.cpp's (name ours).
// ======================================================================== //
// 0x1002ca7c — the eleven banks handed back. The body is empty; every
// instruction is the compiler expanding CObjectBank's inline destructor
// eleven times, each expansion being its own vftable restore, the
// sb_FreeChunks call, and then CObjectBankBase's vftable restore.
//
// This is what proved CObjectBankBase is not a template. Written against a
// templated base it came out 322 bytes against 285 with 88 instructions
// against 91 -- longer with FEWER instructions, which localised the residual
// completely: the shipped build holds the base vftable's address in a
// register across all eleven expansions and restores it in two bytes, and
// ours spent a six-byte immediate each time because our eleven restores
// named eleven different symbols. It could not be linker folding either,
// since /OPT:ICF is off in server.dll.
// Defined in objtypes.cpp, beside the two functions it points at.
struct CObjMgrVtbl;
extern const CObjMgrVtbl g_ObjMgrVtbl;       // 0x1004deb0

// 0x1002449b -- build the object manager at CServerMgr+0x390.
// BYTE-IDENTICAL (385 of 385). netmgr.cpp used to carry this address as
// NOT RECONSTRUCTED; the block that stood in the way was the pair of tables
// at offset zero, answered above.
//
// It is the mirror of the destructor below and everything that function
// proved holds here: eleven DISTINCT bank types at +0x68 and every 0x24
// after it, constructed in declaration order (the destructor walks the same
// eleven backwards). Only two of the eleven grow by anything but 0x20 --
// models at 0x100 and sprites at 0x80.
//
// The eleven LTLists at +0x220 are a member ARRAY, which is why the shipped
// code has a real down-counting loop (`push $0xb ; pop %edx`, 0x10 stride,
// 0x100245f4) rather than eleven expansions: src/LT2/sdk/inc/ltlink.h already
// gives LTList a default constructor that ties the head off and zeroes the
// count, and an array of eleven of those is exactly one loop. At /O2 the
// same array becomes a call to the vector constructor iterator, which is
// lithtech.exe's 0x00401000 and now matches there too.
//
// The body is three statements, at +0x00, +0x04 and +0x08 in that order.
// The table store is FIRST: with the count first instead, server.dll is
// unchanged (its /O1 schedule is the same either way) but lithtech.exe's /O2
// copy stores the count ahead of the table (0x0045a5fc) and is a near.
CObjMgrView::CObjMgrView()
	: m_Normal(0x20), m_Model(0x100), m_WorldModel(0x20), m_Sprite(0x80),
	  m_Light(0x20), m_Camera(0x20), m_ParticleSystem(0x20), m_PolyGrid(0x20),
	  m_LineSystem(0x20), m_Container(0x20), m_Canvas(0x20)
{
	m_pVtbl = (void*)&g_ObjMgrVtbl;

	m_LockCount = 0;

	m_Link.TieOff();
}

CObjMgrView::~CObjMgrView()
{
}

// 0x1002592e — drop an object out of the manager. Two statements: unlink it
// from its type's list, and give the storage back to that type's bank.
//
// The list base never appears: `(nType + 0x22) * 16` is VC6 folding the
// 0x220 byte offset into the scaled index, which is also what says an
// LTList is sixteen bytes and that the array starts there.
//
// The bound test guards only the Free, so an object whose type is eleven or
// more is unlinked and then leaked. That is what shipped.
LTRESULT un390_RemoveObject(CObjMgrView* p, CObjLinked* pObject)
{
	dl_RemoveAt(&p->m_Lists[pObject->m_ObjectType], &pObject->m_Link7c);

	if (pObject->m_ObjectType < 11)
		p->m_pBanks[pObject->m_ObjectType]->Free((CObjBase*)pObject);

	return LT_OK;
}


// 0x10025a1b — unlink one attachment record from its parent and push it onto
// the manager's spare list. Named by the literal it reports through:
// "ILTServer::RemoveAttachment".
//
// smgr.cpp also carries this function, as a 108-of-103 near written with two
// separate RETURN_ERROR sites — so one of the two definitions has to go when
// the files meet, and this is the one that matches. The shipped code has ONE
// error block, at the
// very end, reached from the "ran off the list" test at both the entry and
// the back edge — which is the `while (pCur)` form below with the found arm
// sunk past it (a shared exit block at the very end means the
// failures were merged, not written out twice).
//
// The record's own free link is its first dword, not m_pNext: the spare list
// threads through offset zero while a live record threads through 0x24.
LTRESULT am_RemoveAttachment(CObjMgrView* pAttachMgr, CObjLinked* pParent,
                             Attachment* pAttachment)
{
	Attachment** ppLink = &pParent->m_pAttachments;
	Attachment*  pCur   = *ppLink;

	while (pCur)
	{
		if (pCur == pAttachment)
		{
			*ppLink = pCur->m_pNext;

			sb_Free(&pAttachMgr->m_Bank4c, pAttachment);

			return LT_OK;
		}

		ppLink = &pCur->m_pNext;
		pCur   = *ppLink;
	}

	RETURN_ERROR(1, "ILTServer::RemoveAttachment", LT_ERROR);
}


// 0x10025984 — hang a child object off a parent. The record comes out of the
// manager's 0x28-byte bank (its m_Bank4c, which is what says sizeof
// (Attachment) is 0x28), goes on the front of the parent's singly linked
// list, and is filled in from the arguments.
//
// A missing record is a bare `return LT_ERROR` with no report — the only
// failure path in the function, and `push $1; pop eax` is all of it.
//
// Both offsets are optional and are handled differently, which is in the
// bytes: a null position leaves the record's twelve bytes as the bank left
// them, while a null rotation is written out as the identity quaternion
// through LTRotation::Init's defaults (fldz/fldz/fldz/fld1, interleaved with
// their stores, which is four assignments and not one Init(x,y,z,w) call).
LTRESULT am_CreateAttachment(CObjMgrView* pAttachMgr, CObjLinked* pParent,
                             uint16 nChildID, int iNode, LTVector* pOffset,
                             LTRotation* pRotOffset, Attachment** ppAttachment)
{
	Attachment* pAttachment = (Attachment*)sb_Allocate(&pAttachMgr->m_Bank4c);

	if (!pAttachment)
		return LT_ERROR;

	pAttachment->m_pNext    = pParent->m_pAttachments;
	pParent->m_pAttachments = pAttachment;

	pAttachment->m_nParentID = pParent->m_ObjectID;
	pAttachment->m_nChildID  = nChildID;
	pAttachment->m_iNode     = iNode;

	if (pOffset)
		pAttachment->m_Offset.m_Pos = *pOffset;

	if (pRotOffset)
		pAttachment->m_Offset.m_Rot = *pRotOffset;
	else
		pAttachment->m_Offset.m_Rot.Init();

	if (ppAttachment)
		*ppAttachment = pAttachment;

	return LT_OK;
}


// *********************************************************************** //
//  0x1000edd7 — the world's segment query (servermgr.h names it
//  w_IntersectSegment). It is here rather than in a world file because this
//  file already has servermgr.h; nothing about it belongs to the banks.
//
//  Every intermediate lives in a run of file-scope globals at 0x10058540 and
//  up rather than in the frame — the same style as worldsim.cpp's box
//  machinery — so the walk's two per-poly tests and its per-object callback
//  can read them without being handed anything. All the names below are
//  ours, from what each is used for; only their sizes and types are read off
//  the bytes.
// *********************************************************************** //

// They are DEFINED here, this file being the one that uses them, and with no
// initialiser: the whole run lies in .data's uninitialised tail (the section's
// raw data stops at 0x10057000), so every one of them starts at zero.
//
// 0x10058540 — the segment's length, and 0x10058544 the projection of the
// start onto the (normalised) direction divided by its squared length.
float g_fIsectLength;
float g_fIsectStartFrac;
// 0x10058548 — the direction scaled by the reciprocal of its squared length,
// and 0x10058558 the normalised direction itself.
LTVector g_vIsectScaledDir;
LTVector g_vIsectDir;
// 0x10058564 — the polygon handle the walk found, seeded to -1 (no poly).
int g_hIsectPoly;
// 0x10058568 and 0x10058578 — where and on what the segment stopped.
LTVector g_vIsectPoint;
LTPlane  g_IsectPlane;
// 0x10058588 — the squared length plus one, which is what the per-poly tests
// reject against.
float g_fIsectRangeSqr;
// 0x1005858c and 0x10058590 — what was hit: the object always, and the
// world-polygon record only when it was world geometry.
class LTObject* g_pIsectObject;
class CIsectPolyRef* g_pIsectPolyRef;
// 0x10058594 and 0x10058595 — the two INTERSECT_ bits, unpacked to bytes so
// the walk can test them without the mask.
uint8 g_bIsectFlag0;
uint8 g_bIsectNotFlag1;
// 0x10058598 — the query itself, for everything the walk needs that is not
// unpacked above.
IntersectQuery* g_pIsectQuery;

// 0x1000f00c and 0x1000f0a3 — the two per-polygon tests, chosen by
// INTERSECT_ bit 2; 0x1005859c is the slot the walk calls through, and
// 0x1000f0e8 is the per-object callback the container walk is handed
// (names ours).
//
// Both take the same eight arguments, and the eighth is read by neither.
// Both are defined below.
void w_IsectPolyTestFull(class IWorldBsp* pWorldBsp,
                         class CIsectPolyRef** ppPolyRef, LTVector* pPoint,
                         float* pDist, int* pHPoly, LTVector* pFrom,
                         LTVector* pTo, int nUnused);   // 0x1000f00c
void w_IsectPolyTestCheap(class IWorldBsp* pWorldBsp,
                          class CIsectPolyRef** ppPolyRef, LTVector* pPoint,
                          float* pDist, int* pHPoly, LTVector* pFrom,
                          LTVector* pTo, int nUnused);  // 0x1000f0a3

// The three halves of the per-object test that are not reconstructed. All
// three are cdecl and all three answer in `al`, so their return type is a
// byte rather than LTBOOL (names ours, from the call sites):
//
// 0x1000f650 — the cheap reject: does the segment come near this object's
// bounding sphere at all (name ours). Defined below, and `inline` — see the
// note on the definition.
inline uint8 w_IsectObjectBounds(LTVector* pFrom, LTVector* pTo,
                                 class LTObject* pObj);
// 0x1000f74e — the whole test for an object that carries its own geometry,
// which is why it takes nothing but the object: everything else it needs is
// in the query globals above, and it records its own hit. Defined below
// (name ours).
uint8 w_IsectObjectGeometry(class LTObject* pObj);
// 0x1000f1f7 — the test for everything else, which reports where and on what
// rather than recording it. Defined below (name ours).
uint8 w_IsectObjectShape(LTVector* pFrom, LTVector* pTo, class LTObject* pObj,
                         LTVector* pPoint, LTPlane* pPlane);

// The LTObject virtual at slot 0x34 answers whether the object takes part in
// the test at all, and ltobject_internal.h declares that slot `void` (see the
// same view in servercommon.cpp). Redeclared here on a layout-identical view
// whose only purpose is to give the slot a return value; the view's vptr is
// the object's own, at offset 0, which is where the shipped `mov (%esi),%eax`
// reads it (names ours).
class IObjIsectTestView
{
public:
	virtual void _p00()=0; virtual void _p04()=0; virtual void _p08()=0;
	virtual void _p0c()=0; virtual void _p10()=0; virtual void _p14()=0;
	virtual void _p18()=0; virtual void _p1c()=0; virtual void _p20()=0;
	virtual void _p24()=0; virtual void _p28()=0; virtual void _p2c()=0;
	virtual void _p30()=0;
	virtual LTBOOL TakesPartInIsect()=0;     // 0x34
};


// 0x1000f650 — the object's bounding sphere against the segment, and the
// first thing every object gets. Neither of the two vector arguments is read:
// the segment is entirely in the query globals by the time this runs, and the
// pair survives because the test below it takes them.
//
// The parameter slot the object arrived in is where the squared radius goes.
// It has to be computed before MagSqr is called, because that call is a real
// one -- VC6 declines to expand the SDK's inline -- and it would take the FPU
// stack with it.
//
// Byte-identical. The difference is taken as an unnamed temporary straight
// into MagSqr -- `(vOn - pObj->m_Pos).MagSqr()` -- not assigned back into vOn
// first. That is what gives the shipped frame colouring: vOn alone at -0x18,
// the by-value copy of m_Pos on v's old group at -0xc, operator-'s inner temp
// at -0x24 and the outer (return-value) temp sharing -0x18 once vOn is dead,
// and vOn.z is then never stored, carried in st from 0x1000f6f9 to the
// subtraction at 0x1000f715. Written as `vOn = vOn - pObj->m_Pos; ...
// vOn.MagSqr()` the frame groups swap and vOn.z is stored and reloaded (260
// bytes); an `LTVector v[2]`, vOn.Init(...) and a constructed `LTVector
// vOn(...)` were all worse (275, 252, 252).
//
// The upper bound's radius is read through an inline accessor
// (obj_GetRadius, just below), not as a bare member. VC6 canonicalises the
// commutative sum, so neither operand order nor a local changes it; what
// does is the accessor's value arriving as a temporary, which puts it first
// -- `flds 0x1a4(%ecx); fadds g_fIsectLength` at 0x1000f6a3, where the bare
// member gave the global first. Only there: the accessor on the lower bound
// is neutral, and on the final squared radius it costs 8 bytes (262). How
// the global is declared (defined, static, a struct member, an array
// element) makes no difference.
// INLINE, and the client image is what says so: lithtech.exe has no
// standalone copy of this function at all and no call to one. Its
// w_IsectObjectCB is 839 bytes at 0x00430a60 and carries this whole body
// written out — the two gates, then `flds g_vIsectScaledDir.z; fmuls
// pObj->m_Pos.z ... fsubs g_fIsectStartFrac` at 0x00430ab4 — with only
// w_IsectObjectShape (0x00430db0) and w_IsectObjectGeometry (0x004312c0)
// left as calls. server.dll's w_IsectObjectCB is 71 bytes at 0x1000f0e8 and
// calls out to everything instead, which is the /Os-vs-/Ot inline budget and
// not a second source: at 260 bytes this is far past what /Os will take and
// well inside what `inline` admits at /Ot.
// The object's bounding radius (name ours); see the note above on why the
// upper bound reads it through this rather than as the member.
inline float obj_GetRadius(LTObject* pObj) { return pObj->m_Radius; }

inline uint8 w_IsectObjectBounds(LTVector* pFrom, LTVector* pTo, LTObject* pObj)
{
	LTVector vOn;
	float    t;

	t = g_vIsectScaledDir.Dot(pObj->m_Pos) - g_fIsectStartFrac;

	if (t < -pObj->m_Radius || t > obj_GetRadius(pObj) + g_fIsectLength)
		return 0;

	// One expression, the start point on the LEFT: lithtech.exe's copy calls
	// operator* (0x0041ab70, this = g_vIsectDir) and hands its result
	// straight to operator+ (0x004316d0, this = g_pIsectQuery, i.e. m_From)
	// as the by-value argument, with no named intermediate (0x00430b33 ..
	// 0x00430b5d). server.dll's copy stays byte-identical in this spelling
	// (/O1 expands both operators and folds the argument copy away); the
	// old component-by-component form was an equivalent /O1 spelling only.
	// Lithtech 0x00430a60: 25 -> 13 mnemonic blocks, pushes back at entry.
	vOn = g_pIsectQuery->m_From + g_vIsectDir * t;

	// The square on the left, so it is computed before the call rather than
	// after it: the shipped code parks it in the dead parameter slot and then
	// compares MagSqr's answer straight against it.
	return pObj->m_Radius * pObj->m_Radius > (vOn - pObj->m_Pos).MagSqr();
}


// 0x1000f12f — one object against the segment, and the closest hit so far
// kept in the query globals (name ours).
//
// The object type chooses the path: a WorldModel or a Container carries its
// own geometry and records its own hit, so the answer is whatever that test
// says. Everything else reports a point and a plane back here, and this is
// what decides whether they are nearer than what is already recorded.
//
// The distance is squared and never lands in a stack slot -- the frame is
// exactly the point and the plane -- so it lives in the FPU stack, which is
// what the `fstp %st(0)` on the reject path is throwing away.
//
// Byte-identical at 200 bytes. Three shapes got it there:
//
//   * the success path is wrapped in a positive guard and `return 0` is the
//     LAST statement, which is what puts the shared reject block after the
//     copy block instead of in front of it. Written as
//     `if (...) return 0;` followed by the copy block, our reject lands in
//     the middle, every forward branch to it fits in a byte, and the two
//     `je` that shipped spells `0f 84` shrink to `74` -- eight bytes.
//   * the two object types are TWO separate `if`s with the same body, not
//     one `||`. VC6 keeps the second copy inline and jumps the first
//     forward into it, which is the shipped `je arm / cmp / jne shape /
//     arm:` layout; written as one `||` (with or without an explicit
//     `else`) the arm sinks past the copy block to just before the epilogue
//     and the function comes out four bytes short at 196.
//     The two tests now live in w_IsGeometryObjectQ (just below), which
//     /O1 expands into the same two-if layout and /O2 gives one call.
//   * the record-the-hit block is written TWICE, once under
//     `if (!g_pIsectObject)` and once under `if (fDistSqr <
//     g_fIsectRangeSqr)`, not once under their `||`. lithtech.exe's copy
//     (0x00430cb0 and 0x00430d20) has the two blocks side by side with
//     different store schedules, which /O2 does not produce from one `||`
//     block; and at /O1 VC6 merges the two back into one, but with the edi
//     save pushed up front and popped in the shared epilogue as shipped,
//     where the single `||` block shrink-wrapped it around the copy (six
//     mnemonic blocks, every spelling of the `||` form measured).
// INLINE for the same reason as w_IsectObjectBounds above: lithtech.exe's
// w_IsectObjectCB carries this body too — the filter hook called through
// `call *%eax` at 0x00430bf4, the OT_WORLDMODEL/OT_CONTAINER pair at
// 0x00430c09 falling into `call w_IsectObjectGeometry`, and the shared
// reject at 0x00430d91 — and there is no standalone copy in the image.
// Does the object carry its own geometry (a stand-in helper, name ours).
// lithtech.exe's inlined copy of w_IsectObject has ONE call to
// w_IsectObjectGeometry behind `cmp $2 ; je arm ; cmp $9 ; jne shape`
// (0x00430c07..0x00430c10); two separate `if (...) return Geometry(pObj);`
// statements give /O2 two calls, and a `||` or a switch costs server.dll's
// w_IsectObject (0x1000f12f) its match. Asked through this inline helper
// with its two early returns, /O1 still produces the shipped two-if layout
// and /O2 has one call.
inline LTBOOL w_IsGeometryObjectQ(LTObject* pObj)
{
	if (pObj->m_ObjectType == OT_WORLDMODEL)
		return LTTRUE;
	if (pObj->m_ObjectType == OT_CONTAINER)
		return LTTRUE;
	return LTFALSE;
}

static inline uint8 w_IsectObject(LTVector* pFrom, LTVector* pTo,
                                  LTObject* pObj)
{
	LTVector vPoint;
	LTPlane  Plane;
	float    fDistSqr;

	if (!w_IsectObjectBounds(pFrom, pTo, pObj))
		return 0;

	if (g_pIsectQuery->m_FilterFn)
	{
		if (!g_pIsectQuery->m_FilterFn((HOBJECT)pObj,
		                               g_pIsectQuery->m_pUserData))
			return 0;
	}

	// Two tests, not one `||` -- see the note above and the helper's.
	if (w_IsGeometryObjectQ(pObj))
		return w_IsectObjectGeometry(pObj);

	if (w_IsectObjectShape(pFrom, pTo, pObj, &vPoint, &Plane))
	{
		fDistSqr = vPoint.DistSqr(g_pIsectQuery->m_From);

		if (!g_pIsectObject)
		{
			g_pIsectObject   = pObj;
			g_hIsectPoly     = -1;
			g_IsectPlane     = Plane;
			g_fIsectRangeSqr = fDistSqr;
			g_vIsectPoint    = vPoint;

			return 1;
		}

		if (fDistSqr < g_fIsectRangeSqr)
		{
			g_pIsectObject   = pObj;
			g_hIsectPoly     = -1;
			g_IsectPlane     = Plane;
			g_fIsectRangeSqr = fDistSqr;
			g_vIsectPoint    = vPoint;

			return 1;
		}
	}

	return 0;
}


// 0x1000f0e8 — the per-object callback the container walk is handed, and the
// only thing in front of the test above (name ours).
//
// Two gates. An object that is neither ray-hittable nor solid is skipped
// unless the query's second bit says to consider it anyway; and with
// INTERSECT_OBJECTS clear the object is asked through its own slot 0x34
// whether it wants to be tested at all.
LTBOOL w_IsectObjectCB(LTObject* pObj)
{
	if (!(pObj->m_Flags & (FLAG_RAYHIT | FLAG_SOLID)) && !g_bIsectNotFlag1)
	{
		return LTFALSE;
	}
	else
	{
		if (!g_bIsectFlag0)
		{
			if (!((IObjIsectTestView*)pObj)->TakesPartInIsect())
				return LTFALSE;
		}

		return w_IsectObject(&g_pIsectQuery->m_From,
		                     &g_pIsectQuery->m_To, pObj);
	}
}

void (*g_pIsectPolyTest)();                  // 0x1005859c

// 0x100585a0 — a float in this run that only smgr.cpp's Update touches: it
// zeroes it each frame beside the two counters below (0x10032ebb, the one
// reference in the image). servermgr.h names it (name ours).
float g_Unknown585a0;

// 0x100585a4 and 0x100585a8 — how many segment queries have run and how long
// they have taken.
// smgr.cpp's Update zeroes the same two each frame (servermgr.h externs).
uint32 g_nIsectQueries;
uint32 g_nIsectTime;

// worldbsp.h's CWorldPolyRef / CWorldPoly pair plus the surface behind it;
// the flags the query reports come from the surface's own word at 0x3c.
class CIsectSurface
{
public:
	uint8    m_pad_00[0x34];
	// 0x34 — the polygon's plane, in the world model's own space. The
	// per-object test transforms it out (name ours).
	LTPlane* m_pPlane;                       // 0x34
	uint8    m_pad_38[0x3c - 0x38];
	uint16   m_nTextureFlags;                // 0x3c
};

class CIsectPoly
{
public:
	uint8          m_pad_00[0x28];
	CIsectSurface* m_pSurface;               // 0x28
};

class CIsectPolyRef
{
public:
	CIsectPoly* m_pPoly;                     // 0x00
};

// The container at CServerMgr+0x210 once more (servercommon.h declares its
// slot 3, the point query); this is slot 4, the segment walk (name ours).
class ISegmentQueryWorld
{
public:
	virtual void _v00() = 0;
	virtual void _v04() = 0;
	virtual void _v08() = 0;
	virtual void _v0c() = 0;
	virtual void FindObjectsOnSegment(LTVector* pFrom, LTVector* pTo,
	                                  void (*pfn)(), int a, int b) = 0;   // 0x10
};

// 0x1000f1f7 — the segment against the object's world-space bounding box,
// reporting where it crossed and on which face. This is what every object
// that does not carry its own geometry gets (name ours).
//
// Six faces written out, not a loop over three axes: each one tests the near
// side, solves for the crossing parameter, and rejects unless BOTH other
// components land inside their slab. An inner rejection falls through to the
// next axis rather than answering, which is why the six blocks are three
// if/else pairs in a row and not a chain.
//
// The three POSITIVE faces report the wrong plane distance -- the box's
// minimum on that axis where the point they just reported is the maximum.
// Written here as the bytes have it.
uint8 w_IsectObjectShape(LTVector* pFrom, LTVector* pTo, LTObject* pObj,
                         LTVector* pPoint, LTPlane* pPlane)
{
	// The two components that are not the face's own axis, in the order the
	// tests take them. An array rather than a pair of named floats, and that
	// is the frame talking: the shipped build reserves eight bytes of locals
	// and writes only the first, while a pair of floats lets VC6 park the one
	// that needs memory in the dead pObj parameter slot and reserve nothing.
	float fCross[2];
	float t;

	if (pFrom->x < pObj->m_BBoxMin.x)
	{
		if (pTo->x < pObj->m_BBoxMin.x)
			return 0;

		t = (pObj->m_BBoxMin.x - pFrom->x) / (pTo->x - pFrom->x);

		fCross[0] = (pTo->y - pFrom->y) * t + pFrom->y;

		if (fCross[0] > pObj->m_BBoxMin.y && fCross[0] < pObj->m_BBoxMax.y)
		{
			fCross[1] = (pTo->z - pFrom->z) * t + pFrom->z;

			if (fCross[1] > pObj->m_BBoxMin.z && fCross[1] < pObj->m_BBoxMax.z)
			{
				pPoint->x = pObj->m_BBoxMin.x;
				pPoint->y = fCross[0];
				pPoint->z = fCross[1];

				pPlane->m_Normal.Init(-1.0f, 0.0f, 0.0f);
				pPlane->m_Dist = pObj->m_BBoxMin.x * -1.0f;

				return 1;
			}
		}
	}
	else if (pFrom->x > pObj->m_BBoxMax.x)
	{
		if (pTo->x > pObj->m_BBoxMax.x)
			return 0;

		t = (pObj->m_BBoxMax.x - pFrom->x) / (pTo->x - pFrom->x);

		fCross[0] = (pTo->y - pFrom->y) * t + pFrom->y;

		if (fCross[0] > pObj->m_BBoxMin.y && fCross[0] < pObj->m_BBoxMax.y)
		{
			fCross[1] = (pTo->z - pFrom->z) * t + pFrom->z;

			if (fCross[1] > pObj->m_BBoxMin.z && fCross[1] < pObj->m_BBoxMax.z)
			{
				pPoint->x = pObj->m_BBoxMax.x;
				pPoint->y = fCross[0];
				pPoint->z = fCross[1];

				pPlane->m_Normal.Init(1.0f, 0.0f, 0.0f);
#ifdef NOLFSRC_MODERN
				// fixed: the box-intersect test reports the wrong plane on three
				// of six faces (RETAIL_BUGS.md). The positive face is at the maximum.
				pPlane->m_Dist = pObj->m_BBoxMax.x;
#else
				pPlane->m_Dist = pObj->m_BBoxMin.x;
#endif

				return 1;
			}
		}
	}

	if (pFrom->y < pObj->m_BBoxMin.y)
	{
		if (pTo->y < pObj->m_BBoxMin.y)
			return 0;

		t = (pObj->m_BBoxMin.y - pFrom->y) / (pTo->y - pFrom->y);

		fCross[0] = (pTo->x - pFrom->x) * t + pFrom->x;

		if (fCross[0] > pObj->m_BBoxMin.x && fCross[0] < pObj->m_BBoxMax.x)
		{
			fCross[1] = (pTo->z - pFrom->z) * t + pFrom->z;

			if (fCross[1] > pObj->m_BBoxMin.z && fCross[1] < pObj->m_BBoxMax.z)
			{
				pPoint->y = pObj->m_BBoxMin.y;
				pPoint->x = fCross[0];
				pPoint->z = fCross[1];

				pPlane->m_Normal.Init(0.0f, -1.0f, 0.0f);
				pPlane->m_Dist = pObj->m_BBoxMin.y * -1.0f;

				return 1;
			}
		}
	}
	else if (pFrom->y > pObj->m_BBoxMax.y)
	{
		if (pTo->y > pObj->m_BBoxMax.y)
			return 0;

		t = (pObj->m_BBoxMax.y - pFrom->y) / (pTo->y - pFrom->y);

		fCross[0] = (pTo->x - pFrom->x) * t + pFrom->x;

		if (fCross[0] > pObj->m_BBoxMin.x && fCross[0] < pObj->m_BBoxMax.x)
		{
			fCross[1] = (pTo->z - pFrom->z) * t + pFrom->z;

			if (fCross[1] > pObj->m_BBoxMin.z && fCross[1] < pObj->m_BBoxMax.z)
			{
				pPoint->y = pObj->m_BBoxMax.y;
				pPoint->x = fCross[0];
				pPoint->z = fCross[1];

				pPlane->m_Normal.Init(0.0f, 1.0f, 0.0f);
#ifdef NOLFSRC_MODERN
				pPlane->m_Dist = pObj->m_BBoxMax.y;
#else
				pPlane->m_Dist = pObj->m_BBoxMin.y;
#endif

				return 1;
			}
		}
	}

	if (pFrom->z < pObj->m_BBoxMin.z)
	{
		if (pTo->z < pObj->m_BBoxMin.z)
			return 0;

		t = (pObj->m_BBoxMin.z - pFrom->z) / (pTo->z - pFrom->z);

		fCross[0] = (pTo->x - pFrom->x) * t + pFrom->x;

		if (fCross[0] > pObj->m_BBoxMin.x && fCross[0] < pObj->m_BBoxMax.x)
		{
			fCross[1] = (pTo->y - pFrom->y) * t + pFrom->y;

			if (fCross[1] > pObj->m_BBoxMin.y && fCross[1] < pObj->m_BBoxMax.y)
			{
				pPoint->z = pObj->m_BBoxMin.z;
				pPoint->x = fCross[0];
				pPoint->y = fCross[1];

				pPlane->m_Normal.Init(0.0f, 0.0f, -1.0f);
				pPlane->m_Dist = pObj->m_BBoxMin.z * -1.0f;

				return 1;
			}
		}
	}
	else if (pFrom->z > pObj->m_BBoxMax.z)
	{
		if (pTo->z > pObj->m_BBoxMax.z)
			return 0;

		t = (pObj->m_BBoxMax.z - pFrom->z) / (pTo->z - pFrom->z);

		fCross[0] = (pTo->x - pFrom->x) * t + pFrom->x;

		if (fCross[0] > pObj->m_BBoxMin.x && fCross[0] < pObj->m_BBoxMax.x)
		{
			fCross[1] = (pTo->y - pFrom->y) * t + pFrom->y;

			if (fCross[1] > pObj->m_BBoxMin.y && fCross[1] < pObj->m_BBoxMax.y)
			{
				pPoint->z = pObj->m_BBoxMax.z;
				pPoint->x = fCross[0];
				pPoint->y = fCross[1];

				pPlane->m_Normal.Init(0.0f, 0.0f, 1.0f);
#ifdef NOLFSRC_MODERN
				pPlane->m_Dist = pObj->m_BBoxMax.z;
#else
				pPlane->m_Dist = pObj->m_BBoxMin.z;
#endif

				return 1;
			}
		}
	}

	return 0;
}


// 0x1000f74e — a WorldModel or a Container against the segment. These carry
// their own BSP, so the segment is taken into the object's space, walked
// there, and the hit taken back out (name ours).
//
// The polygon reference the walk finds goes into the slot the object arrived
// in: that parameter is dead from the first instruction (the object lives in
// a callee-saved register), and VC6 hands the slot to the local, which is why
// the frame is exactly the seven vectors and floats below with nothing spare.
// w_IntersectSegment does the same thing with its own query parameter.
//
// The plane transform is ltmatrix.h's LTMatrix::TransformPlane -- but as the
// shipped build's copy of that header had it, which is three lines, not four.
// The SDK copy in src/LT2/sdk/inc/ltmatrix.h transforms the scaled start point
// (`MatVMul_InPlace_H(this, &startPt)`) before dotting it; the shipped bytes
// have no such call, so the start point is scaled and dotted in the object's
// own space while the normal has already been taken out of it. Spelled
// out here rather than calling the header, which would emit
// the extra transform.
//
// The two transformed endpoints are ONE local, a two-element array: `from`
// at -0x48 and `to` at -0x3c (0x1000f759, 0x1000f778). VC6 lays aggregates
// out below the scalars sorted by size, larger lower, so with two separate
// twelve-byte vectors the sixteen-byte plane sank beneath both and ten
// instructions named the wrong slot; as a 24-byte array the pair sinks
// below the plane instead, and the frame comes out exactly as shipped. The
// same change takes lithtech.exe's copy (0x004312c0) from 915 bytes and 25
// mnemonic blocks to 895 and 18.
//
// The plane is reached through the whole pPolyRef->m_pPoly->m_pSurface->
// m_pPlane chain at each use, not a pPlane local: lithtech.exe walks the
// chain afresh after the vector constructor's call (0x0043158c..0x0043159d,
// for MatVMul_3x3's argument) where a local kept it in %ebx across the call.
// /O1 gives the same bytes either way (server.dll still identical).
uint8 w_IsectObjectGeometry(LTObject* pObj)
{
	// The frame, read off the shipped slots.
	float    fDist;             // -0x04
	int      hPoly;             // -0x08
	LTVector vStartPt;          // -0x14
	LTVector vPoint;            // -0x20
	LTPlane  Plane;             // -0x30
	LTVector vLocal[2];         // -0x48 from, -0x3c to

	CIsectPolyRef* pPolyRef;

	MatVMul_H(&vLocal[0], &((WorldModelObject*)pObj)->m_Transform230, &g_pIsectQuery->m_From);
	MatVMul_H(&vLocal[1], &((WorldModelObject*)pObj)->m_Transform230, &g_pIsectQuery->m_To);

	((void (*)(IWorldBsp*, CIsectPolyRef**, LTVector*, float*, int*,
	           LTVector*, LTVector*, int))g_pIsectPolyTest)
		(pObj->m_pWorldBsp, &pPolyRef, &vPoint, &fDist, &hPoly,
		 &vLocal[0], &vLocal[1], 1);

	if (!pPolyRef)
		return 0;

	MatVMul_InPlace_H(&((WorldModelObject*)pObj)->m_Transform1f0, &vPoint);

	fDist = g_pIsectQuery->m_From.DistSqr(vPoint);

	// Tested the way it continues, not the way it rejects: the shipped code
	// jumps AWAY on the reject and puts the shared answer after the taking
	// block, which is what an if around the body gives.
	if (fDist < g_fIsectRangeSqr)
	{
		vStartPt = pPolyRef->m_pPoly->m_pSurface->m_pPlane->m_Normal *
		           pPolyRef->m_pPoly->m_pSurface->m_pPlane->m_Dist;

		MatVMul_3x3(&Plane.m_Normal,
		            &((WorldModelObject*)pObj)->m_Transform1f0,
		            &pPolyRef->m_pPoly->m_pSurface->m_pPlane->m_Normal);

#ifdef NOLFSRC_MODERN
		// fixed: LTMatrix::TransformPlane never transforms the point it dots
		// (RETAIL_BUGS.md). The point goes through the same transform as the
		// normal, as the SDK's ltmatrix.h has it.
		MatVMul_InPlace_H(&((WorldModelObject*)pObj)->m_Transform1f0, &vStartPt);

#endif
		Plane.m_Dist = Plane.m_Normal.Dot(vStartPt);

		g_pIsectObject   = pObj;
		g_pIsectPolyRef  = pPolyRef;
		g_fIsectRangeSqr = fDist;
		g_hIsectPoly     = hPoly;
		g_IsectPlane     = Plane;
		g_vIsectPoint    = vPoint;

		return 1;
	}

	return 0;
}


// worldsim.h's CLineQuery, declared here rather than included so this file
// stays free of the world headers (packet.cpp keeps its own view of the same
// class for the same reason). The two dwords worldsim.h leaves unnamed are
// what the walk's polygon filter needs: +0x0c is the SDK's IntersectQuery,
// whose m_PolyFilterFn and m_pUserData it reaches, and +0x10 is the world
// whose MakeHPoly turns the node it stopped on into the handle that filter
// takes. Both are filled in here and nowhere else, which is what makes this
// the FULL test — the cheap one at 0x1000f0a3 goes through
// bsp_IntersectLine, which constructs its query without them and so accepts
// every polygon.
//
// The constructor (0x1000f08d) zeroes all six fields, in the order below.
// It is `inline`: /O1 declines to expand it and emits the out-of-line
// COMDAT server.dll calls, while /O2 expands it in lithtech.exe's copy
// (0x004308f0), where the only store that survives is m_pPoly's
// `movl $0x0` -- the call there was a 214-vs-213-byte near.
class CLineQuery
{
public:
	CLineQuery();                            // 0x1000f08d

	LTVector*       m_pStart;                // 0x00
	LTVector*       m_pEnd;                  // 0x04
	LTVector*       m_pHit;                  // 0x08
	IntersectQuery* m_pQuery;                // 0x0c
	IWorldBsp*      m_pWorldBsp;             // 0x10
	WorldPoly*      m_pPoly;                 // 0x14  what the walk found
};

inline CLineQuery::CLineQuery()
{
	m_pStart    = LTNULL;
	m_pEnd      = LTNULL;
	m_pHit      = LTNULL;
	m_pPoly     = LTNULL;
	m_pQuery    = LTNULL;
	m_pWorldBsp = LTNULL;
}

// 0x10013f28 — the walk's entry point (worldsim.h's bspq_IntersectLine).
int bspq_IntersectLine(class BSPNode* pRoot, CLineQuery* pQuery);

// 0x1000f00c — the per-polygon test the segment walk calls through when
// INTERSECT_ bit 2 is set. Everything the caller wants comes back through
// pointers, so the function itself answers nothing.
//
// The distance is read back out of the query's own m_pHit rather than out of
// pPoint: the two hold the same pointer, but VC6 does not CSE across the
// virtual call above it, and the shipped code pushes the frame slot.
void w_IsectPolyTestFull(IWorldBsp* pWorldBsp, CIsectPolyRef** ppPolyRef,
                         LTVector* pPoint, float* pDist, int* pHPoly,
                         LTVector* pFrom, LTVector* pTo, int nUnused)
{
	CLineQuery Query;

	Query.m_pStart    = pFrom;
	Query.m_pEnd      = pTo;
	Query.m_pHit      = pPoint;
	Query.m_pQuery    = g_pIsectQuery;
	Query.m_pWorldBsp = pWorldBsp;

	if (bspq_IntersectLine(pWorldBsp->GetRoot(), &Query))
	{
		*pHPoly    = pWorldBsp->MakeHPoly(Query.m_pPoly);
		*ppPolyRef = (CIsectPolyRef*)Query.m_pPoly;
		*pDist     = g_pIsectQuery->m_From.DistSqr(*Query.m_pHit);
	}
	else
	{
		*pHPoly    = -1;
		*ppPolyRef = LTNULL;
	}
}


// 0x10013f53 — worldsim.h's line-versus-BSP query: the first polygon the
// segment crosses, where it crossed it and the plane it crossed.
WorldPoly* bsp_IntersectLine(class BSPNode* pRoot, LTVector* pStart,
                             LTVector* pEnd, LTVector* pHit,
                             LTPlane* pPlane);

// 0x1000f0a3 — the per-polygon test the segment walk calls through when
// INTERSECT_ bit 2 is clear: the plain world query, which has no polygon
// filter to consult and hands back no handle, so the handle is always -1.
// The plane it reports is not wanted (the frame is exactly one LTPlane), and
// the distance is measured from the segment's own start rather than from the
// query's.
void w_IsectPolyTestCheap(IWorldBsp* pWorldBsp, CIsectPolyRef** ppPolyRef,
                          LTVector* pPoint, float* pDist, int* pHPoly,
                          LTVector* pFrom, LTVector* pTo, int nUnused)
{
	LTPlane Plane;

	*pHPoly = -1;

	*ppPolyRef = (CIsectPolyRef*)bsp_IntersectLine(pWorldBsp->GetRoot(),
	                                               pFrom, pTo, pPoint, &Plane);
	if (*ppPolyRef)
		*pDist = pFrom->DistSqr(*pPoint);
}

// The last argument is accepted and never read.
LTBOOL w_IntersectSegment(IntersectQuery* pQuery, IntersectInfo* pInfo,
                          CUnknown210* pObjects, int nFlags)
{
	// The scoped timer is the SERVER's only. lithtech.exe's copy of this
	// function (0x004305f0, 738 bytes) opens `sub $0x18,%esp` and goes
	// straight from the query counter's increment to the four global stores —
	// no CBaseCounter constructor, no cnt_StartCounter, no cnt_EndCounter,
	// and a frame with no room for the object. server.dll's (0x1000edd7) has
	// all three. cnt_StartCounter reaches QueryPerformanceCounter and cannot
	// be optimised away, so this is the preprocessor, not the optimiser —
	// the same split wmrotate.cpp documents for sm_MoveObject's CountAdder
	// and worldsim.cpp already spells LT_SERVER_TIMERS. The counter global
	// itself is incremented in BOTH, so only the timer is guarded.
#ifdef LT_SERVER_TIMERS
	CBaseCounter counter(0);
#endif

	g_nIsectQueries++;

#ifdef LT_SERVER_TIMERS
	cnt_StartCounter(&counter);
#endif

	// The -1 store is LAST. server.dll's /O1 emits these in source order
	// whenever the query store leads (any of the eight such orders match
	// there), but lithtech.exe's /O2 hoists the two zero stores above the
	// counter increment and keeps -1 where the source puts it: with -1
	// first, 0x004305f0 opened with it instead of the shipped
	// `movl $0,g_pIsectObject; inc; movl $0,g_pIsectPolyRef`. The one
	// difference left there is fUnit's inlined MagSqr: shipped squares the
	// fsts'd z against memory and reloads y and x (z, y, x, 753 bytes); ours
	// keeps y and z on the FPU stack and spills the sum (757). No revision
	// (8168/8447/8804/8966/9782), no /Ob2 or /Ox, and no rewrite of the
	// test or the divides that keeps server.dll's MagSqr call moves it.
	g_pIsectQuery    = pQuery;
	g_pIsectObject   = LTNULL;
	g_pIsectPolyRef  = LTNULL;
	g_hIsectPoly     = -1;

	g_bIsectNotFlag1 = !((pQuery->m_Flags >> 1) & 1);
	g_bIsectFlag0    = (uint8)(pQuery->m_Flags & 1);

	g_fIsectRangeSqr = (pQuery->m_From - pQuery->m_To).MagSqr() + 1.0f;

	g_vIsectDir = pQuery->m_To - pQuery->m_From;

	g_fIsectLength = g_vIsectDir.Mag();

	g_vIsectDir.x /= g_fIsectLength;
	g_vIsectDir.y /= g_fIsectLength;
	g_vIsectDir.z /= g_fIsectLength;

	float fUnit = g_vIsectDir.MagSqr();

	if (fUnit < 0.5f || fUnit > 2.0f)
		return LTFALSE;

	// The dot product goes into the pQuery parameter's slot, which is dead:
	// the query is in a callee-saved register from the top of the function.
	float fStartDot = g_vIsectDir.Dot(pQuery->m_From);

	float fInvSqr = 1.0f / g_vIsectDir.MagSqr();

	g_vIsectScaledDir = g_vIsectDir * fInvSqr;
	g_fIsectStartFrac = fInvSqr * fStartDot;

	// An if/else, not a store plus a conditional overwrite: VC6 predicates
	// the pair into exactly that (the then-value stored unconditionally and
	// the else-value stored under the inverted branch), and writing it the
	// second way instead lets the scheduler hoist the first store thirty
	// bytes up into the vector multiply above.
	if (pQuery->m_Flags & 4)
		g_pIsectPolyTest = (void (*)())w_IsectPolyTestFull;
	else
		g_pIsectPolyTest = (void (*)())w_IsectPolyTestCheap;

	((ISegmentQueryWorld*)pObjects)->FindObjectsOnSegment(&pQuery->m_From,
	                                                      &pQuery->m_To,
	                                                      (void (*)())
	                                                      w_IsectObjectCB,
	                                                      0, 0);

#ifdef LT_SERVER_TIMERS
	g_nIsectTime += cnt_EndCounter(&counter);
#endif

	if (!g_pIsectObject)
		return LTFALSE;

	pInfo->m_Point   = g_vIsectPoint;
	pInfo->m_Plane   = g_IsectPlane;
	pInfo->m_hObject = (HOBJECT)g_pIsectObject;
	pInfo->m_hPoly   = g_hIsectPoly;

	if (g_pIsectPolyRef)
		pInfo->m_SurfaceFlags = g_pIsectPolyRef->m_pPoly->m_pSurface->m_nTextureFlags;
	else
		pInfo->m_SurfaceFlags = 0;

	return LTTRUE;
}
