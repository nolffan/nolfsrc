// server.dll — the shared ILTPhysics implementation (reconstruction). Build /O1.
#include <math.h>
#include "commonphysics.h"
#include "servermgr.h"
#include "worldsim.h"
#include "de_error.h"
#include "iltserver.h"
#include "ilttransform.h"
#include "servermath.h"
#include "servercommon.h"
#include "servermodel.h"
#include "serverlightanim.h"

// The name an argument error reports: server.dll's copy of these functions
// says "ILTCommon::...", lithtech.exe's "CommonLT::..." (the retail images'
// strings: the two were built from different revisions of this file).
#ifdef LT_SERVER_BUILD
#define COMMON_ERROR_NAME(fn)	"ILTCommon::" fn
#else
#define COMMON_ERROR_NAME(fn)	"CommonLT::" fn
#endif

LTRESULT CommonPhysicsLT::GetFrictionCoefficient(HOBJECT hObj, float& coeff)
{
	if (!hObj)
		RETURN_ERROR(2, "ILTPhysics::GetFrictionCoefficient", LT_INVALIDPARAMS);

	coeff = hObj->m_FrictionCoefficient;
	return LT_OK;
}

LTRESULT CommonPhysicsLT::SetFrictionCoefficient(HOBJECT hObj, float coeff)
{
	if (!hObj)
		RETURN_ERROR(2, "ILTPhysics::SetFrictionCoefficient", LT_INVALIDPARAMS);

	hObj->m_FrictionCoefficient = coeff;
	return LT_OK;
}

// The limit is kept squared so the engine's own comparisons need no sqrt;
// the accessors take the root on the way out and square on the way in.
LTRESULT CommonPhysicsLT::GetForceIgnoreLimit(HOBJECT hObj, float& limit)
{
	if (!hObj)
		RETURN_ERROR(2, "ILTPhysics::GetForceIgnoreLimit", LT_INVALIDPARAMS);

	limit = (float)sqrt(hObj->m_ForceIgnoreLimitSqr);
	return LT_OK;
}

LTRESULT CommonPhysicsLT::SetForceIgnoreLimit(HOBJECT hObj, float limit)
{
	if (!hObj)
		RETURN_ERROR(2, "ILTPhysics::SetForceIgnoreLimit", LT_INVALIDPARAMS);

	hObj->m_ForceIgnoreLimitSqr = limit * limit;
	return LT_OK;
}

LTRESULT CommonPhysicsLT::GetVelocity(HOBJECT hObj, LTVector* pVel)
{
	if (!hObj || !pVel)
		RETURN_ERROR(1, "CommonLT::GetVelocity", LT_INVALIDPARAMS);

	*pVel = hObj->m_Velocity;
	return LT_OK;
}

LTRESULT CommonPhysicsLT::GetAcceleration(HOBJECT hObj, LTVector* pAccel)
{
	if (!hObj || !pAccel)
		RETURN_ERROR(2, COMMON_ERROR_NAME("GetAcceleration"), LT_INVALIDPARAMS);

	*pAccel = hObj->m_Acceleration;
	return LT_OK;
}

LTRESULT CommonPhysicsLT::GetObjectMass(HOBJECT hObj, float& mass)
{
	if (!hObj)
		RETURN_ERROR(2, COMMON_ERROR_NAME("GetObjectMass"), LT_INVALIDPARAMS);

	mass = hObj->m_Mass;
	return LT_OK;
}

LTRESULT CommonPhysicsLT::SetObjectMass(HOBJECT hObj, float mass)
{
	if (!hObj)
		RETURN_ERROR(2, COMMON_ERROR_NAME("GetObjectMass"), LT_INVALIDPARAMS);

	hObj->m_Mass = mass;
	return LT_OK;
}

// The engine keeps the standing-on contact as an object plus, for world
// geometry, the polygon underfoot. For anything else the contact plane is
// synthesized as the flat top of the object's bounding box.
LTRESULT CommonPhysicsLT::GetStandingOn(HOBJECT hObj, CollisionInfo* pInfo)
{
	if (!hObj || !pInfo)
		RETURN_ERROR(2, "ILTPhysics::GetStandingOn", LT_INVALIDPARAMS);

	pInfo->m_vStopVel.x = 0.0f;
	pInfo->m_vStopVel.y = 0.0f;
	pInfo->m_vStopVel.z = 0.0f;

	LTObject* pStanding = hObj->m_pStandingOn;
	if (pStanding)
	{
		if (hObj->m_pStandingOnPoly)
		{
			pInfo->m_Plane = *hObj->m_pStandingOnPoly->GetPlane();
			pInfo->m_hObject = pStanding;

			if (pStanding->m_ObjectType == OT_WORLDMODEL ||
			    pStanding->m_ObjectType == OT_CONTAINER)
			{
				pInfo->m_hPoly = pStanding->MakeHPoly(hObj->m_pStandingOnPoly);
				return LT_OK;
			}
		}
		else
		{
			pInfo->m_Plane.m_Normal.x = 0.0f;
			pInfo->m_Plane.m_Normal.y = 1.0f;
			pInfo->m_Plane.m_Normal.z = 0.0f;
			pInfo->m_Plane.m_Dist = pStanding->m_Dims.y + pStanding->m_Pos.y;
			pInfo->m_hObject = pStanding;
		}

		pInfo->m_hPoly = INVALID_HPOLY;
	}
	else
	{
		// The null case is the `else` of a positive test, not an early
		// return: reached by a taken branch, VC6 no longer knows a zero
		// register is live and clears the field with `and [mem],0`.
		pInfo->m_hObject = LTNULL;
	}

	return LT_OK;
}

LTRESULT CommonPhysicsLT::GetObjectDims(HOBJECT hObj, LTVector* pDims)
{
	if (!hObj || !pDims)
		RETURN_ERROR(2, "ILTPhysics::GetObjectDims", LT_INVALIDPARAMS);

	*pDims = hObj->m_Dims;
	return LT_OK;
}

LTRESULT CommonPhysicsLT::IsWorldObject(HOBJECT hObj)
{
	if (hObj && hObj->m_ObjectType == OT_WORLDMODEL &&
	    (hObj->m_pWorldBsp->GetWorldFlags() & WORLDFLAG_MAINWORLD))
	{
		return LT_YES;
	}

	return LT_NO;
}

// ---- the server interface object ---------------------------------------- //

// The ILTServer the engine hands to the game (CServerMgr+0x928; the object
// CUnknown928Obj and servermsg.h's CLTServer view describe from outside).
// iltcommon.h names it: `friend class CLTServer`, which is how the
// constructor below reaches ILTCommon's protected m_pTransformLT and
// m_pModelLT on its m_CommonLT member.
//
// Layout, from 0x1002d385's stores:
//   +0x000  ILTServer (ILTCSBase: vptr, m_MathLT +0x04, six interface
//           pointers +0x08..+0x1c, then ILTServer's function pointers to
//           +0x223)
//   +0x224  m_pServerMgr
//   +0x228  ILTTransform (vtable 0x1004e660 is ilttransform.h's own: the
//           interface's virtuals are implemented as ILTTransform:: itself)
//   +0x22c  ServerModelLT   (vtable 0x1004e5f4)
//   +0x234  ServerCommonLT  (vtable 0x1004e5ac, after the SharedCommonLT
//           constructor CALL 0x1001214c)
//   +0x248  SPhysicsLT      (vtable 0x1004e560)
//   +0x258  ServerLightAnimLT (vtable 0x1004e550)
//   0x260 total, which is what 0x1002d355's `new` asks for.
//
// The ??_G 0x1002f83a (slot 71 of 0x1004e368, after ILTCSBase's seventy-one
// virtuals) is the compiler's: ILTCommon's inline destructor on +0x234 and
// ILTServer's on +0x00 are the only stores left in it.
class CLTServer : public ILTServer
{
public:
	CLTServer(CServerMgr* pServerMgr);       // 0x1002d385

	CServerMgr*       m_pServerMgr;          // 0x224
	ILTTransform      m_TransformLT;         // 0x228
	ServerModelLT     m_ModelLT;             // 0x22c
	ServerCommonLT    m_CommonLT;            // 0x234
	SPhysicsLT        m_PhysicsLT;           // 0x248
	ServerLightAnimLT m_LightAnimLT;         // 0x258
};

// Expanded into CLTServer's constructor below. The world simulation the
// server physics forwards to is the server manager itself. m_StairHeight is
// a MEMBER INITIALISER: shipped stores the -1.0f (0x1002d3d7) before the
// vptr (0x1002d3de); assigned in the body it comes after.
inline SPhysicsLT::SPhysicsLT(CServerMgr* pServerMgr)
	: m_StairHeight(-1.0f)
{
	m_pWorld           = (IWorldSim*)pServerMgr;
	m_ClientServerType = ServerType;
}

// 0x1002d385 -- 208 bytes.  `inline` (the in-class spelling): server.dll's
// /O1 still CALLS it from 0x1002d355 below and keeps the standalone copy,
// while lithtech.exe's /O2 EXPANDS it into that caller (0x0046bbf0; there is
// no standalone copy in lithtech.exe).
inline CLTServer::CLTServer(CServerMgr* pServerMgr)
	: m_ModelLT(pServerMgr), m_PhysicsLT(pServerMgr), m_LightAnimLT(pServerMgr)
{
	m_pServerMgr   = pServerMgr;

	m_pCommonLT    = &m_CommonLT;
	m_pTransformLT = &m_TransformLT;
	m_pModelLT     = (ILTModel*)&m_ModelLT;
	m_pPhysicsLT   = &m_PhysicsLT;
	m_pLightAnimLT = &m_LightAnimLT;

	// CServerMgr's sound interface is its second base (+0x04). Shipped
	// null-tests with a BRANCH and one store at the join (0x1002d406..
	// 0x1002d423): an if/else into a local. The conversion written as an
	// expression -- plain or as `?:` -- is `neg/sbb/and`; an if/else storing
	// the member on each arm stores twice.
	ILTSoundMgr* pSoundMgr;
	if (pServerMgr)
		pSoundMgr = (ILTSoundMgr*)(CServerSoundMgr*)pServerMgr;
	else
		pSoundMgr = LTNULL;
	m_pSoundMgr = pSoundMgr;

	m_CommonLT.SetMath((ICommonMath*)&m_MathLT);
	m_CommonLT.m_pServerMgr   = pServerMgr;
	m_CommonLT.m_pModelLT     = (ILTModel*)&m_ModelLT;
	m_CommonLT.m_pTransformLT = &m_TransformLT;
}

// smgr.cpp:3846, 0x1002fd9b.
void si_SetupInterface(CLTServer* pServer);

// 0x1002d355 (48 bytes) / lithtech 0x0046bbf0 -- builds the server interface
// object and fills its function table.  Reached only through
// sm_CreateUnknown928 (smgr.cpp, 0x1002d34a), which is why it answers the
// CUnknown928Obj* smgr.cpp declares.
CUnknown928Obj* sub_1002d355(CServerMgr* pMgr)
{
	// `new CLTServer(pMgr)` spelled out: the class is abstract here (its
	// ILTCSBase overrides are written as views in servermessage.cpp and
	// elsewhere), so the allocation and the constructor call are separate.
	void*      pMem = operator new(sizeof(CLTServer));
	CLTServer* pServer;

	if (pMem)
		pServer = &((CLTServer*)pMem)->CLTServer::CLTServer(pMgr);
	else
		pServer = LTNULL;

	if (pServer)
		si_SetupInterface(pServer);

	return (CUnknown928Obj*)pServer;
}

// ---- the server's half -------------------------------------------------- //

// Velocity and acceleration are only written when they actually moved: a
// change below 0.001 squared is dropped rather than sent to every client.
//
// Both byte-identical on both images. The shape that matters is the EARLY
// RETURN on the small-change test (`< 0.001f` -> return LT_OK) rather than an
// `if (>=) { ... }` body: with the guarded body, cl shrink-wraps the edi save
// past the parameter tests and pops it before the success return; with the
// early return, the two LT_OK returns share one epilogue and edi is saved in
// the entry block (`push esi`, load hObj, `test`, `push edi`, `je`), as
// shipped. The member is also named directly, not through a pointer local —
// the local made cl compute the lea before pushing DistSqr's argument, and
// kept the /O2 copy off.
LTRESULT SPhysicsLT::SetVelocity(HOBJECT hObj, LTVector* pVel)
{
	if (!hObj || !pVel)
		RETURN_ERROR(1, "ILTPhysics::SetVelocity", LT_INVALIDPARAMS);

	if (hObj->m_Velocity.DistSqr(*pVel) < 0.001f)
		return LT_OK;

	hObj->m_ChangeFlags |= CHANGEFLAG_MOTION;
	hObj->m_Velocity = *pVel;

	return LT_OK;
}

LTRESULT SPhysicsLT::SetAcceleration(HOBJECT hObj, LTVector* pAccel)
{
	if (!hObj)
		RETURN_ERROR(1, "ILTPhysics::SetAcceleration", LT_INVALIDPARAMS);

	if (hObj->m_Acceleration.DistSqr(*pAccel) < 0.001f)
		return LT_OK;

	hObj->m_ChangeFlags |= CHANGEFLAG_MOTION;
	hObj->m_Acceleration = *pAccel;

	return LT_OK;
}

// The flags argument is accepted and ignored; the simulation is always asked
// for the same 7.
LTRESULT SPhysicsLT::MoveObject(HOBJECT hObj, LTVector* pPos, uint32 flags)
{
	if (!hObj)
		RETURN_ERROR(1, "ILTPhysics::MoveObject", LT_INVALIDPARAMS);

	sm_MoveObject(m_pWorld, hObj, pPos, 7);
	return LT_OK;
}

// iltphysics.h declares the force accessors taking LTVector& — no null check
// is possible, and none is compiled. (The null-checking pair that forwards
// through the world's vtable is ILTServer's, whose header takes LTVector* —
// see servermessage.cpp. Same names, two interfaces, and the parameter type
// alone dictates both shapes.)

LTRESULT SPhysicsLT::GetGlobalForce(LTVector& vec)
{
	vec = m_pWorld->m_ForceControl.m_Force;
	return LT_OK;
}

// HOISTED ABOVE ITS CALLER AND MARKED `inline`.  VC6's inliner is
// forward-only: with the definition below SetGlobalForce it expands in
// none of it, and the shipped /O2 build expands it.  Same cause as
// sm_SetObjectChangeFlag in servermodel.cpp.
// The direction is kept alongside the force so the simulation never has to
// normalize per object. A force too small to have a direction leaves the
// direction at nothing rather than dividing by it.
//
// The magnitude is stored in the member, compared from the member and the
// direction divided by the member (lithtech 0x46c0f0: `fcomps` then a fresh
// `flds` of m_fMag for the divides) -- the shape the client twin at
// 0x405190 closed with (src/engine/lithtech/lt2new1.cpp). A local fMag kept
// server.dll identical but left lithtech.exe a near.
inline void GlobalForceControl::Set(LTVector* pVec)
{
	m_Force = *pVec;

	m_fMag = m_Force.Mag();

	// 1e-5f: the shipped operand at 0x1004d92c is 0x3727c5ac (fpconst.py).
	if (m_fMag > 1e-5f)
	{
		m_Dir = m_Force;
		m_Dir /= m_fMag;
	}
	else
	{
		m_Dir.x = 0.0f;
		m_Dir.y = 0.0f;
		m_Dir.z = 0.0f;
	}
}

LTRESULT SPhysicsLT::SetGlobalForce(LTVector& vec)
{
	m_pWorld->m_ForceControl.Set(&vec);
	return LT_OK;
}

LTRESULT SPhysicsLT::GetStairHeight(float& fHeight)
{
	fHeight = m_StairHeight;
	return LT_OK;
}

LTRESULT SPhysicsLT::SetStairHeight(float fHeight)
{
	m_StairHeight = fHeight;
	return LT_OK;
}

// The six-field fill: four values in and the two supplied-list fields
// cleared. SPhysicsLT::SetObjectDims just below, and the two out-of-line
// fillers CWMStateInit::Init / InitFrom further down, all use it (defined
// here, ahead of its first caller, so /Ob1 can expand it). It has to be an
// inline function rather than a body written out, because that is the only
// shape that
// puts InitFrom's three source loads ahead of all six stores — as arguments
// they are evaluated right to left (0x0c, then the object, then 0x04, then
// 0x00) before the body runs, and written out inline they would interleave
// load with store the way Init's stack arguments do.
static inline void wm_InitMoveState(WMMoveState* pState, void* pContainer,
                                    void* pUnknown04, LTObject* pObj,
                                    uint32 nPriority)
{
	pState->m_pContainer = (CUnknown210*)pContainer;
	pState->m_pUnknown04 = (CUnknown6A8Obj*)pUnknown04;
	pState->m_pObj       = pObj;
	pState->m_nUnknown0C = nPriority;
	pState->m_pPushList = 0;
	pState->m_nPushCount = 0;
}

#ifndef LT_SERVER_BUILD
// lithtech.exe has NO out-of-line copy of CWMMoveSetup::Setup (server
// 0x1001efd8): all three client call sites expand it -- 0x0045456d..
// 0x00454577 in wm_RotateWorldModel (0x00454530) is `mov 4(%ebx),%ecx;
// mov (%ecx),%eax; call *0x24(%eax); mov %eax,0x18(%ebx)` with no call.
// So the client's copy is inline, defined here ahead of its first caller
// (VC6's inliner is forward-only); server.dll keeps the out-of-line body
// below.
inline void CWMMoveSetup::Setup()
{
	m_Unknown18 = m_pUnknown04->_v24();
}
#endif

// 0x1002d5a9 / lithtech 0x0046bf20 — byte-identical on both images. The
// second fill is wm_InitMoveState, not six stores written out: shipped loads
// both the container address and the 0x6a8 object (0x1002d662/0x1002d668)
// before the first store, and the container goes in first — an inline
// helper's arguments evaluated ahead of its body. Written out, the fill put
// `this` in a different spill position and the function-long zero in ecx
// rather than edx (and cost 22 blocks on /O2). The vDims copy comes after all
// four zero stores (lithtech 0x46bf3f..0x46bf60: zeros, then the copy).
LTRESULT SPhysicsLT::SetObjectDims(HOBJECT hObj, LTVector* pNewDims, uint32 flags)
{
	WMMoveState state;
	LTVector vDims;

	state.m_pContainer = LTNULL;
	state.m_pUnknown04 = LTNULL;
	state.m_pObj = LTNULL;
	state.m_nUnknown64 = 0;
	vDims = *pNewDims;

	if (!hObj || !pNewDims)
		RETURN_ERROR(2, "SPhysicsLT::SetObjectDims", LT_INVALIDPARAMS);

	if (pNewDims->x > g_DebugMaxDims || pNewDims->y > g_DebugMaxDims ||
	    pNewDims->z > g_DebugMaxDims)
	{
		dsi_ServerError("ILTPhysics::SetObjectDims: dims larger than 'DebugMaxDims'");
		dsi_ServerError("Object class: %s, dims (%.2f, %.2f, %.2f)",
			hObj->m_pSData->m_pClass->m_pClassName,
			pNewDims->x, pNewDims->y, pNewDims->z);
	}

	if (hObj->m_ObjectType == OT_CONTAINER || hObj->m_ObjectType == OT_WORLDMODEL)
		return LT_INVALIDPARAMS;

	CServerMgr* pMgr = (CServerMgr*)m_pWorld;
	wm_InitMoveState(&state, &pMgr->m_Unknown210, pMgr->m_pUnknown6a8, hObj,
	                 hObj->m_BlockingPriority);

	if (wm_SetObjectDims(&state, &vDims, flags & 1, 1))
		return LT_OK;

	// Refused: hand back the dims the simulation would have accepted.
	*pNewDims = vDims;
	return LT_ERROR;
}


// ---- the world's global force -------------------------------------------- //

// Gravity, and the slope beyond which a surface stops holding anything up.
GlobalForceControl::GlobalForceControl()
{
	LTVector vForce(0.0f, -2000.0f, 0.0f);

	Set(&vForce);

	m_Unknown30 = -0.70709997f;
}



// ---- the simulation entry points ----------------------------------------- //

// 0x10058728 / 0x1005872c — the counted-move pair below: how many moves, and
// the ticks their CountAdders charged. Zeroed .bss, owned here: they sit
// directly ahead of this file's g_AxisDirs (0x10058730), and the only
// writers besides the two moves are smgr.cpp's reset (0x10032ec1/0x10032ec7).
// servermgr.h declares both (names ours).
int g_Unknown58728;
int g_Unknown5872c;

// 0x100355fe — move an object through the world simulation.
//
// A counted move: one global counts them and a scoped CountAdder charges the
// time to the next one along (0x10058728 and 0x1005872c). The world pointer is
// the server manager — the request record is filled from its container and its
// 0x6a8 object, the same six fields SPhysicsLT::SetObjectDims fills — and the
// position goes to the simulation BY VALUE, twelve bytes onto the stack ahead
// of the flags.
//
// Two orderings in the block below are load-bearing and were found by probe.
// The container has to be taken before the 0x6a8 pointer: assigning it later
// lets the load of 0x6a8 have eax to itself and the address comes out as
// `add $0x210,%eax` where the shipped code has `lea 0x210(%eax),%ecx` (one
// byte short). And m_pObj has to precede m_nUnknown0C even though the shipped
// code stores them in that order anyway — with the priority written first,
// the object pointer and the byte read off it end up in the wrong registers.
void sm_MoveObject(IWorldSim* pWorld, HOBJECT hObj, LTVector* pPos, uint32 flags)
{
	g_Unknown58728++;

	// PROBE: the shipped client's copy of this function (lithtech 0x0047c5b0)
	// has no CountAdder at all -- no ctor, no cnt_StartCounter, no dtor, and a
	// frame exactly 12 bytes smaller (the counter's 8 plus m_pTotal). The
	// server's (0x1001fb22) has all three. Same shared TU, so a build-level
	// selector, not a sibling file.
#ifdef LT_SERVER_TIMERS
	CountAdder  cntAdder((uint32*)&g_Unknown5872c);
#endif
	WMMoveState state;
	CServerMgr* pMgr = (CServerMgr*)pWorld;

	state.m_nUnknown64 = 0;
	state.m_pContainer = &pMgr->m_Unknown210;
	state.m_pUnknown04 = pMgr->m_pUnknown6a8;
	state.m_pObj = hObj;
	state.m_nUnknown0C = hObj->m_BlockingPriority;
	state.m_pPushList  = LTNULL;
	state.m_nPushCount = 0;

	wm_MoveObject(&state, *pPos, flags);
}


// STAND-IN: an inline twin of obj_MaybeRetransformWorldModel (0x1001eb0c),
// for the sites lithtech.exe expands (wm_SetObjectDims, wm_MoveObject,
// wm_MoveThroughSim, wm_RotateWorldModel, wm_PushWorldModel; defined here,
// above the first of them). lithtech.exe expands that call
// (0x4524e0 is 526 bytes with the `cmp $2 / cmp $9` + obj_RetransformWorldModel
// written out), server.dll calls it (0x1001f9ac). Marking the real function
// `inline` makes /O1 expand it at six other server sites too, so the twin is
// guarded to the /O2 image: one spelling cannot satisfy both builds.
#ifndef LT_SERVER_BUILD
static inline void obj_MaybeRetransformWorldModelInline(LTObject* pObj, int bFlag)
{
	if (bFlag)
	{
		uint8 nType = pObj->m_ObjectType;

		if (nType == OT_WORLDMODEL || nType == OT_CONTAINER)
			obj_RetransformWorldModel(pObj);
	}
}
#endif

// The real function cannot simply be `inline` in the client: its out-of-line
// copy (0x00451c80) is called only from wm_PushIntersectingObjects
// (0x00451b02), where the shipped build's inline budget is spent and ours is
// not, so that site would expand too and the copy would vanish.
#ifndef LT_SERVER_BUILD
#define WM_RETRANSFORM obj_MaybeRetransformWorldModelInline
#else
#define WM_RETRANSFORM obj_MaybeRetransformWorldModel
#endif


// 0x10020611 — resize a world object through the simulation.
//
// Three separate one-axis passes, and which way each goes is decided by a
// float compare against the object's current dims: an axis that is shrinking
// (or unchanged) is written straight onto the object, an axis that is growing
// goes through wm_ResizeAxis, which is the one that can be refused. Afterwards
// the caller's vector is overwritten with whatever the object ended up with,
// and the answer is whether the miss was under a unit.
//
// Read off the bytes rather than guessed:
//
//   * the flag word tested is 0x20012000 = FLAG_SOLID | FLAG_TOUCH_NOTIFY |
//     FLAG_CONTAINER. An object with none of them (and the caller's flag clear)
//     skips the whole per-axis dance and just takes the new dims.
//   * the early-out compares the *squared* miss against 0.001, not 0.001
//     squared, and it is only consulted when the fourth argument is set —
//     MagSqr is called either way and its result discarded (`fstp st(0)`) when
//     it is not.
//   * the result is its own local, bResult, written three times and
//     returned. server.dll /O1 homes it in the dead fourth parameter's slot
//     (the stores at 0x14(%ebp)); lithtech.exe /O2 homes it in pState's
//     (`movl $0x1,0x2c(%esp)`, 0x00454180). Reusing bUnknown itself gave
//     the server bytes but pinned lithtech's stores to 0x38(%esp).
//   * the three-float construction at the top is a CALL (0x1001e6d7, which
//     stores its arguments at this+0/4/8 and returns `this`), and the vector
//     operator at the bottom is not. So the engine's own ltvector.h declared
//     the three-float constructor out of line — /Ob1 expands the SDK copy's
//     in-class one, and the call disappears — while its operator- built its
//     result member-wise rather than through that constructor. Both halves are
//     reproduced: CVectorInit for the call, the SDK's operator- for the rest.
//     The unnamed-temporary form (`*(LTVector*)&CVectorInit(...)`) is what
//     makes the assignment copy out of the constructor's returned pointer;
//     a named temporary re-addresses the local instead.
//
// Byte-identical on both images. The facts that closed it, each read off
// the frame:
//
//   * the two thresholds are retail's .rdata floats, which the masked
//     byte match cannot see: the early-out compares against 0.0001f
//     (0x1004dcf8, 0x004a2a38) and the final miss against 0.001f
//     (0x1004d1f0, 0x004a1a54); tools/fpconst.py checks them.
//   * the wanted dims the miss is measured against are a BLOCK-SCOPED copy
//     (vNew), not vDims: lithtech.exe's 0x18 frame then folds the
//     MagSqr temporary onto it (0x10(%esp), 0x00453ff7); with vDims at
//     function scope the temporary took a slot of its own (0x24 frame).
//     server.dll lays the two out the same either way.
//   * the retransform at the end is WM_RETRANSFORM: lithtech.exe expands
//     it here (the `cmp $2 / cmp $9` at 0x00454217 around the call to
//     0x00450c30), server.dll calls 0x1001eb0c.
//
//   * the object's dims are read through a POINTER LOCAL for the three
//     subtractions below. The shipped code folds the first indexed use back
//     to absolute addressing (`flds 0x1a0(%ebx)`, the z term, evaluated
//     first because arguments go right to left) and only then materialises
//     the `lea 0x198(%ebx),%eax` the other two use.
//   * the first miss is not a variable: the CVectorInit result is
//     copied into an unnamed LTVector temporary (0x1002064e: -0x18 -> -0x24)
//     and MagSqr'd there. A named vDiff kept a slot of its own all function.
//   * vMiss IS a variable, at function scope (-0x30); vWanted is block-scoped
//     and shares -0x18 with the CVectorInit temp. Block-scoping vMiss, or
//     leaving it an unnamed temporary, lets VC6 hand -0x30 to that temp.
//   * the per-axis sets are `Init()`, not VEC_SET: the x set at 0x10020708
//     goes `flds (%esi); fstps -0xc(%ebp)` because Init's float parameters
//     pass through the FPU; VEC_SET's plain copy became `mov (%esi),%eax`.
int wm_SetObjectDims(WMMoveState* pState, LTVector* pDims, int bFlag, int bUnknown)
{
	LTVector vDims;
	LTVector vMiss;
	float    fDiffSqr;
	int      bResult;

	((CWMMoveSetup*)pState)->Setup();

	LTObject* pObj = pState->m_pObj;

	{
		LTVector vNew;

		vNew = *pDims;

		LTVector* pObjDims = &pObj->m_Dims;

		fDiffSqr = LTVector(*(LTVector*)&CVectorInit(pObjDims->x - vNew.x,
		                                 pObjDims->y - vNew.y,
		                                 pObjDims->z - vNew.z)).MagSqr();
	}

	if (bUnknown && fDiffSqr < 0.0001f)
		return 1;

	if (bFlag && (pObj->m_Flags & (FLAG_SOLID | FLAG_TOUCH_NOTIFY | FLAG_CONTAINER)))
	{
		LTVector vWanted = *pDims;

		if (pObj->m_Dims.y >= pDims->y)
		{
			vDims.Init(pObj->m_Dims.x, pDims->y, pObj->m_Dims.z);
			((CObjDimsSetter*)pObj)->SetDims(vDims);
		}
		else
		{
			wm_ResizeAxis(pState, 1, &pDims->y);
		}

		if (pObj->m_Dims.x >= pDims->x)
		{
			vDims.Init(pDims->x, pObj->m_Dims.y, pObj->m_Dims.z);
			((CObjDimsSetter*)pObj)->SetDims(vDims);
		}
		else
		{
			wm_ResizeAxis(pState, 0, &pDims->x);
		}

		if (pObj->m_Dims.z >= pDims->z)
		{
			vDims.Init(pObj->m_Dims.x, pObj->m_Dims.y, pDims->z);
			((CObjDimsSetter*)pObj)->SetDims(vDims);
		}
		else
		{
			wm_ResizeAxis(pState, 2, &pDims->z);
		}

		vMiss = vWanted - pObj->m_Dims;

		if (vMiss.MagSqr() < 0.001f)
			bResult = 1;
		else
			bResult = 0;

		*pDims = pObj->m_Dims;
	}
	else
	{
		((CObjDimsSetter*)pObj)->SetDims(*pDims);
		bResult = 1;
	}

	obj_UnlinkTrackersA(pObj);
	WM_RETRANSFORM(pObj, 1);
	pState->m_pContainer->BumpObject(pObj, 0);

	return bResult;
}


// ---- the two casts and the world-model retransform ----------------------- //

// 0x1001efd8 — the request record's one piece of setup: ask the manager's
// 0x6a8 object for something through slot 9 of its vtable and cache the answer
// at record +0x18. `push esi; mov ecx,esi` because ecx is spent on the callee's
// `this`; nothing else in the function.
#ifdef LT_SERVER_BUILD
void CWMMoveSetup::Setup()
{
	m_Unknown18 = m_pUnknown04->_v24();
}
#endif

// 0x10024ae3 — LTObject::SetDims, the dims counterpart of SetPos: the vector
// arrives by value (`ret $0xc`), goes into m_Dims as a whole-vector copy —
// `lea 0x198(%ecx),%edi` plus three `movsl`, the base-adjusting form — and the
// bounds are rebuilt afterwards. Reached here through a cast because
// ltobject_internal.h is not a file this reconstruction owns.
#ifdef LT_SERVER_BUILD
void CObjDimsSetter::SetDims(LTVector vDims)
{
	m_Dims = vDims;

	UpdateBounds();
}
#else
// lithtech.exe 0x0045aba0 (219 bytes) -- the same SetDims at /O2, which
// EXPANDS UpdateBounds (ltobject.cpp, server.dll 0x10024a4c): the two box
// stores at +0xf0/+0xfc through by-value temps (the 0xc frame) and
// Mag() + 0.1f into +0x1a4 (0x0045ac61 fsqrt). commonphysics.h's
// CObjDimsSetter only declares UpdateBounds, so this view restates it
// in-class, as predobj.cpp's LTObjectQ460 does for SetPos (0x0045aaa0).
// STAND-IN; the layout is ltobject_internal.h's LTObject. (names ours)
class CObjDimsSetterQG9
{
public:
	void SetDims(LTVector vDims);                           // 0x0045aba0

	void UpdateBounds()
	{
		m_BBoxMin = m_Pos - m_Dims;
		m_BBoxMax = m_Pos + m_Dims;

		m_Radius = m_Dims.Mag() + 0.1f;
	}

	uint8    m_pad_000[0xf0];
	LTVector m_BBoxMin;                                     // 0x0f0
	LTVector m_BBoxMax;                                     // 0x0fc
	uint8    m_pad_108[0x18c - 0x108];
	LTVector m_Pos;                                         // 0x18c
	LTVector m_Dims;                                        // 0x198
	float    m_Radius;                                      // 0x1a4
};

void CObjDimsSetterQG9::SetDims(LTVector vDims)
{
	m_Dims = vDims;

	UpdateBounds();
}
#endif

// WM_CINLINE: `inline` in lithtech.exe's copy of this file only. server.dll
// has an out-of-line copy of each of these functions and calls it at every
// site; lithtech.exe expands them, and keeps an out-of-line copy only where
// some site in this file was left a call once the inline budget ran out. The
// evidence for each is at its definition.
#ifndef LT_SERVER_BUILD
#define WM_CINLINE inline
#else
#define WM_CINLINE
#endif

// 0x1001eb0c — the flag is tested first (`cmpl $0x0,0x8(%esp)` before the
// object is even loaded), so it is the outer guard. The type byte is read once
// into %al and compared twice, which is a named local rather than two compares
// against memory; the two values are 2 and 9, i.e. OT_WORLDMODEL and
// OT_CONTAINER.
void obj_MaybeRetransformWorldModel(LTObject* pObj, int bFlag)
{
	if (bFlag)
	{
		uint8 nType = pObj->m_ObjectType;

		if (nType == OT_WORLDMODEL || nType == OT_CONTAINER)
			obj_RetransformWorldModel(pObj);
	}
}


// 0x1002082b — grow the object along one axis, by bisection.
//
// The wanted component arrives by address and the function writes back what it
// managed. The method is: work out the growth this axis is asking for, shove
// the object that far one way and then twice as far the other way through the
// simulation, and see how far the world actually let it go each time. If it
// moved freely both ways the axis can have the whole amount; otherwise it gets
// half of whatever slack the two probes found. The final move is the only one
// with flags 7 (the same 7 sm_MoveObject always asks for); the probes go in
// with 1.
//
// Read off the bytes:
//
//   * the request record is a fresh WMMoveState of its own, zeroed in the same
//     four fields as SPhysicsLT::SetObjectDims and then filled by the
//     out-of-line 0x10020a2d rather than field by field — the only caller in
//     the reconstructed set that uses that helper.
//   * the object's position is re-read from the object after every probe (five
//     whole-vector copies in all): the simulation is what moved it, so nothing
//     the function computed can be trusted across a move.
//   * the halving constant is 0.5f at 0x1004d1fc, applied to the *difference*
//     between the two probes' resting places.
//   * three of the six-byte slots below the frame are dead parameters: nAxis's
//     slot holds fDelta, pfWanted's holds the byte offset nAxis*4 (VC6
//     strength-reduced the subscript once and kept it), and pState's holds
//     fPosB — which is then reused as the result, which is why the multiply
//     stores back to the same slot.
//   * the two `(fPosB - fPosA) * 0.5f` blocks are one block in the image: the
//     inner else and the outer else are identical, and VC6 keeps the second
//     copy and jumps the first into it.
//   * the last twelve bytes of the wm_MoveObject frame are reused for the
//     by-value dims argument — `add $0x8,%esp` instead of `add $0x14,%esp` —
//     which is just VC6's deferred cdecl cleanup meeting a thiscall that
//     wants a twelve-byte argument.
//
// Three source facts were found by probe and each is load-bearing:
//
//   * every `pObj->m_Pos` snapshot that a float component is then read out of
//     is its OWN block-scoped LTVector. One function-scope vector instead lets
//     VC6 common `&vPos[nAxis]` into a spilled pointer local, which takes the
//     slot the shipped build gives fPosA and shifts every local below it;
//     three disjoint scopes overlay onto one slot (-0x20) and defeat the
//     address CSE, which is exactly the shipped layout.
//   * the `vMove = pObj->m_Pos` before the second test belongs to BOTH inner
//     arms, not to the outer arm. With it written once in the outer arm VC6
//     sees the outer if and else starting with the same store and hoists it
//     above the outer branch, losing the second copy the shipped code has —
//     and the inner-else's copy is the one VC6 hoists away, which is why the
//     shipped `jb` lands *past* the outer else's copy.
//   * the subscripts are `_CVector::operator[]` (ltvector.h:96), not
//     VEC_INDEX, at every site but two: `fDelta = *pfWanted - m_Dims[nAxis]`
//     is then one statement that folds as `flds (%edx); fsubs
//     0x198(%eax,%edx,4)` (0x1002085c) with fDelta in nAxis's slot and
//     nAxis*4 in pfWanted's, as shipped. The in-inner-if `fPosB = (fPosB -
//     VEC_INDEX(vPos, nAxis)) * 0.5f` and the final `VEC_INDEX(vMove, nAxis)
//     = fPosB` stay macros: operator[] there costs 4 and 1 blocks (the final
//     one flips the store to an integer `mov %eax,(%ebx)`). Every other site
//     measured worse as VEC_INDEX.
//   * fDelta2 is computed AFTER `vMove = pObj->m_Pos`, directly above the
//     `+=` that uses it: VC6 then stores it with a non-popping `fsts
//     -0x24(%ebp)` and adds straight from st0 (0x100208da), where the other
//     order popped and reloaded.
//   * the final dims are read through a pointer local: the first add folds
//     `fadds 0x198(%ecx)` and only then materialises `lea 0x198(%ecx),%eax`
//     for the other two (0x100209ff), the same fingerprint as wm_SetObjectDims.
//
// The two float snapshots fPosA and fPosB are INTEGER copies in both images
// (server 0x100208f2 `mov -0x20(%ebp,%ecx,1),%ecx ; ... mov %ecx,-0x14(%ebp)`
// and 0x10020927), and VC6 only gives a float copy integer moves here when
// the copy is spelled through an integer type: every float spelling
// (operator[], VEC_INDEX, `(&vPos.x)[nAxis]`, at every position) goes
// through the FPU. fPosA's copy also sits directly under its vPos copy --
// that is what loads nAxis*4 into %ecx ahead of the vMove copy
// (0x100208d0). With both, server.dll 0x1002082b is byte-identical.
//
// lithtech.exe 0x00454250: 731/731 and ZERO mnemonic blocks; what is left
// is the esp-frame slots of fDelta and one other float swapped (0x18 against
// 0x28) and one 0x30/0x20 pair. Declaration order of the five floats does
// not move it (four swaps measured).
//
// The inner test is written `<` with the else-arm body first: lithtech.exe
// (0x454250) falls through into the inner-else's own vMove copy and `jmp`s
// into the (fPosB - fPosA) * 0.5f tail it shares with the outer else, where
// the `>=` spelling let /O2 merge the two else arms whole (710 bytes against
// 731). Now 731/731 there; server.dll is unchanged by it (7 blocks). The
// lithtech residual is the same two integer-vs-FPU float copies.
void wm_ResizeAxis(WMMoveState* pState, int nAxis, float* pfWanted)
{
	WMMoveState state;
	LTVector    vMove;
	float       fDelta;
	float       fDelta2;
	float       fOrigPos;
	float       fPosA;
	float       fPosB;

	state.m_pContainer = LTNULL;
	state.m_pUnknown04 = LTNULL;
	state.m_pObj       = LTNULL;
	state.m_nUnknown64 = 0;

	LTObject* pObj = pState->m_pObj;

	fDelta = *pfWanted - pObj->m_Dims[nAxis];

	vMove = pObj->m_Pos;

	fOrigPos = vMove[nAxis];

	vMove[nAxis] -= fDelta;

	((CWMStateInit*)&state)->Init(pState->m_pContainer, pState->m_pUnknown04,
	                              pObj, pObj->m_BlockingPriority);

	wm_MoveObject(&state, vMove, 1);

	{
		LTVector vPos;

		vPos = pObj->m_Pos;

		*(uint32*)&fPosA = *(uint32*)&vPos[nAxis];

		vMove = pObj->m_Pos;

		fDelta2 = fDelta + fDelta;

		vMove[nAxis] += fDelta2;
	}

	wm_MoveObject(&state, vMove, 1);

	{
		LTVector vPos;

		vPos  = pObj->m_Pos;
		*(uint32*)&fPosB = *(uint32*)&vPos[nAxis];
	}

	if (fOrigPos + fDelta > fPosB)
	{
		if (fOrigPos - fDelta < fPosA)
		{
			vMove = pObj->m_Pos;

			fPosB = (fPosB - fPosA) * 0.5f;

			vMove[nAxis] -= fPosB;
		}
		else
		{
			vMove = pObj->m_Pos;

			vMove[nAxis] -= fDelta2;

			wm_MoveObject(&state, vMove, 1);

			{
				LTVector vPos;

				vPos  = pObj->m_Pos;
				vMove = pObj->m_Pos;

				fPosB = (fPosB - VEC_INDEX(vPos, nAxis)) * 0.5f;

				vMove[nAxis] += fPosB;
			}
		}
	}
	else
	{
		vMove = pObj->m_Pos;

		fPosB = (fPosB - fPosA) * 0.5f;

		vMove[nAxis] -= fPosB;
	}

	wm_MoveObject(&state, vMove, 7);

	VEC_SET(vMove, 0.0f, 0.0f, 0.0f);

	VEC_INDEX(vMove, nAxis) = fPosB;

	{
		LTVector vNewDims;

		LTVector* pDims = &pObj->m_Dims;

		vNewDims.x = vMove.x + pDims->x;
		vNewDims.y = vMove.y + pDims->y;
		vNewDims.z = vMove.z + pDims->z;

		((CObjDimsSetter*)pObj)->SetDims(vNewDims);
	}
}


// 0x1001f010 — do two boxes overlap by more than the tolerance on every
// axis? (name ours) One of wm_MoveObject's two intersection primitives; the
// resize walk and the standing-on-me gather use it too.
//
// The tolerance is negated once and kept on the FPU stack across the whole
// chain — `fcomp %st(1)` on the min side against `fcomps 0x20(%ebp)` on the
// max side — which is what says `-fTolerance` is one common subexpression of
// three statements while `fTolerance` itself is read straight out of its
// parameter slot each time. The pairing is min1-against-max2 and
// max1-against-min2, so the first and last arguments belong to opposite
// boxes; the operand order is `fsubs` of the second box, not the first.
int wm_BoxesOverlap(LTVector* pMin1, LTVector* pMax1,
                    LTVector* pMin2, LTVector* pMax2, float fTolerance)
{
	if (pMin1->x - pMax2->x < -fTolerance && pMax1->x - pMin2->x > fTolerance &&
	    pMin1->y - pMax2->y < -fTolerance && pMax1->y - pMin2->y > fTolerance &&
	    pMin1->z - pMax2->z < -fTolerance && pMax1->z - pMin2->z > fTolerance)
	{
		return 1;
	}

	return 0;
}


// 0x1001f082 — the box test with the world-model geometry behind it (name
// ours). Two objects whose boxes overlap have really intersected unless one
// of them is a world model that does its own collision, in which case the
// other one's box is put to that model's transformed BSP. The flag the
// caller gets back says which of the two answers it is holding.
//
// Three things read off the bytes:
//
//   * the three values the arms pick — the world model and the *other*
//     object's min and max — are three locals assigned in each arm, not an
//     expression: the else arm zeroes all three and the test that follows is
//     on the pointer, so a null model is how "neither of them is one" is
//     carried out of the chain.
//   * the mask is materialised in a register (`test %edx,0x88(%eax)` twice)
//     because both arms want it; that is VC6's, not the source's.
//   * the BSP call's value is normalised to 0/1 (`neg`/`sbb`/`neg`), and by
//     an `if` returning 1 or 0, not `!= 0` in the return. Both spellings give
//     the same out-of-line bytes (0x00451cb0, 0x1001f082), but lithtech.exe
//     also EXPANDS this function -- in wm_MoveObject (0x004515a6..) and in
//     both arms of wm_PushWithGeometry (0x00452101.., 0x004523a2..) -- and
//     there the shipped code tests the call result and branches with no
//     neg/sbb/neg. Expanded into an `if`, the `if` form folds into the test
//     and the `!= 0` form is materialised first (measured, cl 8447 /O2).
//     Its out-of-line copy is called from the pusher bodies expanded into
//     wm_PushIntersectingObjects and wm_PushWorldModel.
WM_CINLINE int wm_ObjectsIntersect(LTObject* pObj1, LTObject* pObj2,
                        LTVector* pMin1, LTVector* pMax1,
                        LTVector* pMin2, LTVector* pMax2,
                        float fTolerance, int* pbUsedGeometry)
{
	LTObject* pWorldModel;
	LTVector* pMin;
	LTVector* pMax;

	if (!wm_BoxesOverlap(pMin1, pMax1, pMin2, pMax2, fTolerance))
		return 0;

	if ((pObj1->m_ObjectType == OT_WORLDMODEL || pObj1->m_ObjectType == OT_CONTAINER) &&
	    !(pObj1->m_Flags & FLAG_BOXPHYSICS))
	{
		pWorldModel = pObj1;
		pMin = pMin2;
		pMax = pMax2;
	}
	else if ((pObj2->m_ObjectType == OT_WORLDMODEL || pObj2->m_ObjectType == OT_CONTAINER) &&
	         !(pObj2->m_Flags & FLAG_BOXPHYSICS))
	{
		pWorldModel = pObj2;
		pMin = pMin1;
		pMax = pMax1;
	}
	else
	{
		pWorldModel = LTNULL;
		pMin = LTNULL;
		pMax = LTNULL;
	}

	if (pWorldModel)
	{
		*pbUsedGeometry = 1;

		if (bsp_IntersectBox(pWorldModel->m_pWMIface->GetTransformedBsp(),
		                     pMin, pMax))
			return 1;

		return 0;
	}

	*pbUsedGeometry = 0;

	return 1;
}


// ---- wm_MoveObject's push-everything-else-out-of-the-way pass ------------ //

// The record the container's box query fills in, and the callback it reports
// each hit through. The callback names itself in its own overflow message —
// "FindObjectsCB: Overflowed (more than %d intersecting objects...)" — and
// that message's argument is 0x80, which is also the size of the buffer the
// caller reserves (0x200 bytes of frame). Field names ours.
#define WM_MAX_FOUND_OBJECTS 0x80

// The buffer is part of the record and not a local beside it: the frame is
// 0xc + 0x200 + 0x24 with the header at -0x230 and the array immediately
// above it at -0x224, and `query.m_ppObjects = query.m_Objects` is what puts
// the header BELOW the array. Two separate locals put the 512-byte array at
// the bottom of the frame instead, whatever their declaration order.
struct CFindObjectsQuery
{
	struct WMMoveState* m_pState;            // 0x00
	class LTObject**    m_ppObjects;         // 0x04
	int                 m_nObjects;          // 0x08
	class LTObject*     m_Objects[WM_MAX_FOUND_OBJECTS];   // 0x0c
};

void FindObjectsCB(class LTObject* pObj, CFindObjectsQuery* pQuery);  // 0x100205af

typedef void (*FindObjectsFn)(class LTObject* pObj, CFindObjectsQuery* pQuery);

// The container at WMMoveState+0x00, reached here through slot 1: everything
// whose box overlaps the one it is handed, reported through the callback.
// servercommon.h declares the same object for slot 3 (the point query) and
// servermgr.h's CUnknown210 models neither, so the interface is declared
// locally and the pointer cast (names ours).
class IBoxQueryWorld
{
public:
	virtual void _v00()=0;
	virtual void FindObjectsInBox(LTVector* pMin, LTVector* pMax,
	                              FindObjectsFn pfn, CFindObjectsQuery* pQuery,
	                              int nFlag)=0;                        // 0x04
};

// 0x1001f357 — the swept box: the object's own box copied into the two
// destinations, then each component pushed out along whichever side the move
// vector points at (name ours). It is a separate out-of-line function even
// though worldsim.cpp has its own two-argument ws_SetupSweptBox.
void wm_SweepBox(LTVector* pMinOut, LTVector* pMaxOut, LTVector* pMove,
                 LTVector* pBoxMin, LTVector* pBoxMax);

// 0x1001f3e7 — is this object one this move has to shift, and did shifting it
// work? Nonzero means it moved (name ours).
int  wm_PushObject(struct WMMoveState* pState, class LTObject* pObj);

// 0x10058730 — the six axis directions, in +X, -X, +Y, -Y, +Z, -Z order, and
// an array rather than six globals: 0x1001fd56 indexes it at runtime with
// `lea (%eax,%eax,2)` then `lea 0x10058730(,%eax,4)`, which is a twelve-byte
// stride in the clear (name ours).
//
// Their constructor run is 0x1001e747, and the -1.0f is loaded ONCE and kept
// on the FPU stack across the three entries that need it -- `fsts` without a
// pop for -X and -Y, and the survivor lands in -Z -- which is what says the
// three are one common subexpression of the same literal rather than three
// reads.
LTVector g_AxisDirs[6] =
{
	LTVector( 1.0f,  0.0f,  0.0f),
	LTVector(-1.0f,  0.0f,  0.0f),
	LTVector( 0.0f,  1.0f,  0.0f),
	LTVector( 0.0f, -1.0f,  0.0f),
	LTVector( 0.0f,  0.0f,  1.0f),
	LTVector( 0.0f,  0.0f, -1.0f),
};


// The rest of the push cluster, in address order (names ours):
//
//   0x1001f4ee  is this object one a push may not simply walk past?
//   0x1001f51e  the touch/container notifications one contact produces
//   0x1001f5cd  push it with the world-model geometry behind the move
//   0x1001fb22  push it with the box alone
//   0x10020045  the world-model-against-world-model case
//   0x1002056f  does this pair need real geometry rather than box physics?
LTBOOL wm_IsPushBlocker(class LTObject* pObj);
void   wm_NotifyContact(class IWMSetupSource* pSource, class LTObject* pObj,
                        class LTObject* pOther);
int    wm_PushWithGeometry(struct WMMoveState* pState, class LTObject* pObj,
                           LTVector* pOrigPos, LTVector* pNewPos, int bFlag,
                           int* pnOut);
int    wm_PushWithBox(struct WMMoveState* pState, class LTObject* pObj,
                      LTVector* pOrigPos, LTVector* pNewPos);
int    wm_PushWorldModel(struct WMMoveState* pState, class LTObject* pObj);
LTBOOL wm_PairNeedsGeometry(class LTObject* pObj1, class LTObject* pObj2);

// 0x1001ff57 — of the six ways one box can be pushed clear of another (two
// per axis), the one that moves it least. The answer is a vector that is
// zero everywhere but the winning axis, plus which of the six candidates
// won, numbered 2*axis for the +axis push and 2*axis+1 for the -axis one
// (names ours).
void wm_SmallestSeparation(LTVector* pMin1, LTVector* pMax1,
                           LTVector* pMin2, LTVector* pMax2,
                           LTVector* pOut, int* pnSide);

// 0x1001f9ac — run one box move through the world simulation on a world
// model's behalf, and deal with what it hit: what the object is now standing
// on, the collision notification when the stopping momentum is over the
// object's own ignore limit, and the velocity the bounce added. Nonzero means
// something was hit (name ours).
int wm_MoveThroughSim(struct WMMoveState* pState, class IWorldBsp* pWorldBsp,
                      HOBJECT hObject, class LTObject* pObj,
                      LTVector* pStart, LTVector* pEnd,
                      int bPushOut, int bStandOn);

// 0x1001e8c8 — give the object the dims of its world-model geometry as that
// geometry actually sits: the geometry's own box is taken through the
// transform corner by corner and the object's dims become half the extent of
// what comes back (name ours).
void wm_UpdateDimsFromGeometry(class LTObject* pObj, LTMatrix* pTransform);

// 0x1000d73b is ltmatrix.h's own MatVMul_InPlace_H, emitted out of line the
// way MatMul and the vector helpers are. It returns the reciprocal of w, and
// every caller discards it with a bare `fstp %st(0)` — which is what makes
// the return type readable off the call site rather than only off the body.

// 0x1001f4ee reaches LTObject's vtable slot 0x34, which ltobject_internal.h
// (not a file this reconstruction owns) declares as void. The cast to this
// same-layout stand-in is how the call is written (name ours).
class IWMMovableObject
{
public:
	virtual void _v00()=0; virtual void _v04()=0; virtual void _v08()=0;
	virtual void _v0c()=0; virtual void _v10()=0; virtual void _v14()=0;
	virtual int _v18(struct WMMoveState*, class LTObject*, class LTObject*)=0; virtual void _v1c()=0; virtual void _v20()=0;
	virtual void _v24()=0; virtual void _v28()=0; virtual void _v2c()=0;
	virtual void _v30()=0;
	// 0x34 — answers whether the object is one a push has to take seriously.
	virtual LTBOOL _v34()=0;
};

// The record the 0x6a8 object is currently serving — the 0x24 bytes
// wm_MoveObject parks there — is the SDK's CollisionInfo: the notifier below
// clears its plane (the normal and the distance, four `fldz`) and its polygon
// handle, and wm_MoveThroughSim reads the object out of its +0x10. That is
// every field ltbasedefs.h declares, at the offsets it declares them, which
// is what identifies the record rather than any name in the image.

// The WMMoveState fields servermgr.h leaves inside m_pad_18: the position the
// move started from, the move itself, its midpoint and half-length-plus-dims
// radius, and the swept box the query runs on. Every one of them is written
// here and read by the helpers above (names ours).
class CWMMoveGeom
{
public:
	uint8      m_pad_00[0x1c];
	LTVector*  m_pOrigPos;                   // 0x1c
	LTVector   m_vPos;                       // 0x20
	LTVector   m_vMove;                      // 0x2c
	LTVector   m_vMid;                       // 0x38
	float      m_fRadius;                    // 0x44
	LTVector   m_vBoxMin;                    // 0x48
	LTVector   m_vBoxMax;                    // 0x54
};

// 0x1001f131 — shift everything the move would pass through, then check the
// move again, until nothing else is in the way or the attempt is given up.
//
// Read off the bytes:
//
//   * the two-entry history is an indexed array (`mov %esi,-0x8(%ebp,%eax,4)`
//     with the index `nHandled & 1`), so it alternates slots rather than
//     keeping a "previous" and a "one before that".
//   * `i = query.m_nObjects` at the end of a successful push restarts the
//     scan; the value goes through the dead third parameter's slot because
//     the two vector assignments after it need edi for their `movsl` runs.
//   * the give-up block is written twice — once for the retry limit and once
//     for the object that came round again — and the copy the retry limit
//     jumps to is the one after the loop, reached without re-testing the
//     count because VC6 propagates the range it just branched on.
//   * the container is only asked when the caller did not supply a list
//     (WMMoveState+0x10); the shipped `then` arm is the supplied one, so the
//     query is the `else`.
//
// NEAR MATCH: 550 of 550 bytes, 208 instructions against 208, and every byte
// equal but two — the shipped build emits `add $0xf0,%eax` (the object box's
// min) before `push %ecx` (its max) and ours emits it after. Two adjacent
// independent instructions transposed, with the same registers and the same
// frame. Ruled out: naming either box address in a local, naming the object,
// and all four compiler revisions. Getting here needed three real source
// facts, all recorded above: the buffer is a member of the query record
// rather than a local beside it (worth 27 bytes of frame displacement), the
// half-move magnitude is its own statement (the one-expression form makes
// VC6 evaluate the dims term first), and `i = query.m_nObjects` is written
// before the two vector assignments that follow it, not after.
//
// That is server.dll. lithtech.exe's 0x00451720 (1306 bytes) expands
// wm_SweepBox and wm_PushObject here (both WM_CINLINE); ours comes out 1303
// bytes with the same pushers called in the same order. The rest is inline
// budget: the shipped build CALLS obj_MaybeRetransformWorldModel (0x00451b02)
// and the loop-tail operator- (0x00451b54); ours calls the first and expands
// the second (calling its constructor), which costs a 0x244 frame against
// 0x238. With obj_MaybeRetransformWorldModel `inline` ours expands it and
// calls operator- (frame 0x234) -- and loses the out-of-line 0x00451c80.
void wm_PushIntersectingObjects(WMMoveState* pState, LTVector* pOrigPos,
                                LTVector* pNewPos)
{
	CFindObjectsQuery query;
	CWMMoveGeom*      pGeom = (CWMMoveGeom*)pState;

	query.m_ppObjects = query.m_Objects;
	query.m_nObjects = 0;

	pGeom->m_pOrigPos = pOrigPos;

	pGeom->m_vPos = *pNewPos;
	pGeom->m_vMove = *pNewPos - *pOrigPos;
	pGeom->m_vMid = *pOrigPos + pGeom->m_vMove * 0.5f;

	LTVector* pDims = &pState->m_pObj->m_Dims;
	float     fHalfMove = pGeom->m_vMove.Mag() * 0.5f;

	pGeom->m_fRadius = pDims->Mag() + fHalfMove;

	wm_SweepBox(&pGeom->m_vBoxMin, &pGeom->m_vBoxMax, &pGeom->m_vMove,
	            &pState->m_pObj->m_BBoxMin, &pState->m_pObj->m_BBoxMax);

	query.m_pState = pState;

	if (pState->m_pPushList)
	{
		query.m_ppObjects = (LTObject**)pState->m_pPushList;
		query.m_nObjects = pState->m_nPushCount;
	}
	else
	{
		((IBoxQueryWorld*)pState->m_pContainer)->FindObjectsInBox(
			&pGeom->m_vBoxMin, &pGeom->m_vBoxMax, FindObjectsCB, &query, 0);
	}

	LTObject* pLast[2];
	int       nHandled;
	int       i;

	pLast[1] = LTNULL;
	pLast[0] = LTNULL;
	nHandled = 0;

	for (i = query.m_nObjects - 1; i >= 0; i--)
	{
		if (nHandled >= 10)
			break;

		LTObject* pObj = query.m_ppObjects[i];

		pState->m_nUnknown64 = (pObj == pLast[0] || pObj == pLast[1]) ? nHandled : 0;

		if (!wm_PushObject(pState, pObj))
			continue;

		if (nHandled >= 2 && (pObj == pLast[0] || pObj == pLast[1]))
		{
			pGeom->m_vPos = *pGeom->m_pOrigPos;
			pState->m_pObj->SetPos(*pGeom->m_pOrigPos);
			break;
		}

		pLast[nHandled & 1] = pObj;

		obj_MaybeRetransformWorldModel(pState->m_pObj, 1);

		nHandled++;

		// Before the two vector writes, not after: the count goes into the
		// dead third parameter's slot because the `movsl` runs below need
		// edi. Written after them it stays a plain read of the record.
		i = query.m_nObjects;

		pGeom->m_vPos = pState->m_pObj->m_Pos;
		pGeom->m_vMove = pGeom->m_vPos - *pOrigPos;
	}

	if (nHandled >= 10)
	{
		pGeom->m_vPos = *pGeom->m_pOrigPos;
		pState->m_pObj->SetPos(*pGeom->m_pOrigPos);
	}
}


// 0x1001eb2d — the world simulation's move. It sets the object down at the
// requested place, carries whatever was standing on it, hands the objects it
// runs into to the push pass, and re-establishes what everything is standing
// on afterwards. It recurses once per carried object.
//
// Read off the bytes:
//
//   * the position arrives BY VALUE and the body writes it back
//     (`vPos = vStart + vDelta`), so the caller's copy is scratch.
//   * `pState->m_pObj` is re-read at every use; nothing holds it.
//   * the two flag tests that are kept in locals — `flags & 8` and
//     `flags & 4` — are each a store followed by an `and` against the slot,
//     and the first lives in the dead first parameter's slot.
//   * the carry list's records are reached through a pointer VC6 strength-
//     reduces out of `found[nFound]`; the walk starts it one record BELOW
//     the array because nFound starts at -1.
//   * the recursion's argument list and the box test after it share stack:
//     the `add esp,0x10` after the recursive call leaves the float argument's
//     four bytes in place for `fstps (%esp)`.
//
// NEAR MATCH: 1196 bytes against 1195, 411 instructions against 410, 14
// mnemonic blocks (17 before the carry loop's vDest was named; that took the
// size from 1192 to 1196). Two faults are left. The loop's record pointer is
// biased onto the record (ours) rather than onto m_vOffset (shipped
// `mov -0x4(%esi),%ecx` at 0x1001ee81); unchanged by a pointer to the
// offset, a copied offset, reading the object first, and for/while/do
// loops over nFound itself. The other is the one fault behind the rest — which of the two memory-only locals gets the dead
// first parameter's slot. The shipped build puts `bUseArg` there and gives
// nFound a frame slot of its own; ours does the reverse. Everything that
// differs follows from that:
//
//   * a local homed in a parameter slot is never enregistered, so the shipped
//     build masks it in place (`mov %edx,0x8(%ebp)` / `andl $0x8,0x8(%ebp)`)
//     while ours masks in a register and stores once. The two-statement form
//     below is what gets our build as close as it gets — written `flags & 8`
//     it is three bytes worse again.
//   * with `bUseArg` in memory the shipped standing-on test has three
//     memory-immediate comparisons, so VC6 materialises a zero in esi and
//     spends it on all three plus the null store; ours needs it once, keeps
//     the value in a register, and pays an extra `jmp` for the else arm's
//     store instead of hoisting the then arm's above the branch.
//   * every frame displacement below vDelta shifts by four, because the
//     walk's record-cursor spill lands under vDelta in ours and shares
//     nSubFlags' slot in the shipped build.
//
// It is not about declaration or statement order: all 40 orders of the
// bUseArg / vStart / pStanding / nFound / bWalk statements keep nFound in
// the slot. Moving the bUseArg pair above `vDelta = ...` does give bUseArg
// the slot, but grows the function (1199). Declaring bUseArg and bWalk
// `volatile` reproduces the shipped shape exactly — `flags` held in edx,
// `mov %edx,0x8(%ebp)` / `andl` in memory — at the price of reloads (1204,
// 8 blocks): shipped treats both as memory-only variables, which is what
// stops VC6 folding `x = flags; x &= 8` into `flags & 8` and lets it keep
// `flags` in edx for both. An unused `&bUseArg`, `*&bUseArg`, and an
// inline `uint32&`/`uint32*` mask helper do not do it.
//
// Probed and negative: declaring bUseArg first, last, and in the top block;
// copying `flags` into a named local first so both masks read one register;
// moving the `m_ChangeFlags |=` statement to either side of the pair; and all
// four compiler revisions (8168, 8447, 8804, 8966), which emit the same 1189
// bytes. The one thing that did move a lot — worth eleven bytes and four
// instructions — was writing the carry loop as a down-counting SUBSCRIPT
// (`for (i = nFound; i >= 0; i--)` over `found[i]`) rather than a pointer
// walk; the pointer form needs the trip count as a second variable and comes
// out with two induction pointers where the shipped build has one, biased
// onto the record's vector.
//
// The stack array also leaves an unreferenced `??_H` vector-constructor
// iterator COMDAT behind, because LTVector declares a default constructor.
// Nothing calls it (wm_MoveObject has no relocation for it) and it matches no
// shipped address; it is an artifact of the declaration, not missing work.
//
// lithtech.exe (0x00451000, 1676 bytes) expands InitFrom, wm_ObjectsIntersect
// and both obj_MaybeRetransformWorldModel calls; ours matches that call for
// call. NEAR: 1651 bytes, 4 mnemonic blocks. The 25 bytes are the
// `bUseArg ? vPos : m_Pos` copy -- the shipped build copies vPos straight
// into vStart in its own arm, ours selects an address and shares one copy
// (written as if/else both arms copy, but registers reshuffle to 19 blocks)
// -- plus the carry loop's record pointer biased by a different constant.
void wm_MoveObject(WMMoveState* pState, LTVector vPos, uint32 flags)
{
	CWMStandRec   found[WM_MAX_STANDING];
	CWMRequestRec rec;
	WMMoveState   state;
	uint32        bUseArg;
	int           bUsedGeometry;
	void*         pSavedRequest;
	LTVector      vStart;
	LTObject*     pStanding;
	LTVector      vDelta;
	uint32        bWalk;
	int           nFound;

	state.m_pContainer = LTNULL;
	state.m_pUnknown04 = LTNULL;
	state.m_pObj       = LTNULL;
	state.m_nUnknown64 = 0;

	((CWMMoveSetup*)pState)->Setup();

	if (pState->m_pObj->m_ChangeFlags & 2)
		return;

	if (!pState->m_pObj->_p30())
		return;

	vDelta = vPos - pState->m_pObj->m_Pos;

	if (vDelta.MagSqr() < WM_MOVE_EPSILON_SQR)
		return;

	if (((IWMSetupSource*)pState->m_pUnknown04)->_v28(pState->m_pObj))
	{
		if (flags & 2)
			((IWMSetupSource*)pState->m_pUnknown04)->_v00(pState->m_pObj, 2);

		pState->m_pObj->SetPos(vPos);
		return;
	}

	((IWMSetupSource*)pState->m_pUnknown04)->_v20(pState, &vPos);

	pSavedRequest = *((IWMSetupSource*)pState->m_pUnknown04)->_v04();
	*((IWMSetupSource*)pState->m_pUnknown04)->_v04() = &rec;

	memset(&rec, 0, sizeof(rec));

	((IWMSetupSource*)pState->m_pUnknown04)->_v10(pState->m_pObj);

	// Two statements, not `flags & 8`, and both taken off one copy of the
	// flag word: the shipped code loads `flags` into a register that stays
	// live all the way down to the walk test below, stores it into each of
	// the two variables and masks each in place. Written as `flags & 8` the
	// mask happens in the register and the flag word is reloaded for the
	// second variable — and the variable then gets an ordinary frame slot
	// instead of the dead first parameter's. (Re-measured: `bUseArg = flags
	// & 8` now emits the same bytes as the two-statement form, and neither
	// holds `flags` in a register the way the shipped build does.)
	pState->m_pObj->m_ChangeFlags |= CHANGEFLAG_MOTION | 2;

	bUseArg = flags;
	bUseArg &= 8;

	vStart = bUseArg ? vPos : pState->m_pObj->m_Pos;

	// The thing it was standing on is only remembered when the move came out
	// of the simulation and the object was resting on another object rather
	// than on world geometry.
	if (pState->m_pObj->m_pStandingOn && !pState->m_pObj->m_pStandingOnPoly && !bUseArg)
		pStanding = pState->m_pObj->m_pStandingOn;
	else
		pStanding = LTNULL;

	nFound = -1;

	bWalk = flags;
	bWalk &= 4;

	if (bWalk)
	{
		LTLink* pCur;

		for (pCur = pState->m_pObj->m_StandingOnMe.m_pNext;
		     pCur != &pState->m_pObj->m_StandingOnMe; )
		{
			LTObject* pOn = (LTObject*)pCur->m_pData;

			pCur = pCur->m_pNext;

			// SHIPPED BUG, reproduced as shipped: the cap is
			// `cmpl $0x20,-0x4(%ebp); jge`, so an index of 0x20 is admitted
			// into an array of 0x20 records. The frame is the proof that the
			// array really is that size — 0x200 for it plus 0x24 for the
			// request record, 0x68 for the nested move state and 0x4c for
			// the rest is exactly the `sub $0x2d8,%esp` the prologue does —
			// so a thirty-third object standing on this one writes its
			// record over the first sixteen bytes of the request record at
			// -0xd8, which the notification above it has already been
			// handed the address of.
			if (!(pOn->m_Flags & FLAG_DONTFOLLOWSTANDING) && nFound < WM_MAX_STANDING)
			{
				nFound++;

				found[nFound].m_pObj = pOn;
				found[nFound].m_vOffset = pOn->m_Pos - pState->m_pObj->m_Pos;
			}
			else
			{
				obj_UnlinkTrackersA(pOn);
			}
		}
	}
	else
	{
		obj_UnlinkTrackersB(pState->m_pObj);
	}

	if (pState->m_pObj->m_Flags & (FLAG_SOLID | FLAG_TOUCH_NOTIFY | FLAG_CONTAINER))
	{
		if (!bUseArg)
		{
			if (fabs(vDelta.x) < 0.001) vDelta.x = 0.0f;
			if (fabs(vDelta.y) < 0.001) vDelta.y = 0.0f;
			if (fabs(vDelta.z) < 0.001) vDelta.z = 0.0f;

			vPos = vStart + vDelta;
		}

		if (flags & 1)
			obj_UnlinkTrackersA(pState->m_pObj);

		pState->m_pObj->SetPos(vPos);

		WM_RETRANSFORM(pState->m_pObj, 1);

		wm_PushIntersectingObjects(pState, &vStart, &pState->m_pObj->m_Pos);
	}
	else
	{
		pState->m_pObj->SetPos(vPos);
	}

	WM_RETRANSFORM(pState->m_pObj, 1);

	pState->m_pContainer->BumpObject(pState->m_pObj, 0);

	if (bWalk && nFound >= 0)
	{
		uint32 nSubFlags = bUseArg | 7;
		int    i;

		// The array is INDEXED, not walked: the shipped loop's trip count
		// (nFound + 1) and its record pointer (base + nFound * 0x10) are
		// both computed from nFound before the first iteration, which is
		// what VC6's strength reduction of a down-counting subscript gives
		// — a hand-written pointer walk needs the count as a second
		// variable and comes out with two induction pointers.
		for (i = nFound; i >= 0; i--)
		{
			// Named, and computed BEFORE the fill: the shipped loop copies
			// the offset and does the three fadds ahead of the InitFrom call
			// (0x1001ee84..0x1001eeb5) and hands the recursion a copy of
			// -0x28. Written as the argument it cannot cross the call.
			LTVector vDest = pState->m_pObj->m_Pos + found[i].m_vOffset;

			((CWMStateInit*)&state)->InitFrom(pState, found[i].m_pObj);

			wm_MoveObject(&state, vDest, nSubFlags);

			// A local for the second read: the shipped code reads the
			// record's object twice — once for the fill above and once
			// here, where the box test and the setter share it in a
			// callee-saved register.
			LTObject* pOn = found[i].m_pObj;

			if (wm_BoxesOverlap(&pOn->m_BBoxMin, &pOn->m_BBoxMax,
			                    &pState->m_pObj->m_BBoxMin, &pState->m_pObj->m_BBoxMax,
			                    -0.01f))
				obj_SetStandingOn(pOn, pState->m_pObj, LTNULL);
		}
	}

	// A world model or a container is not something to stand on; anything
	// else the object is still overlapping it goes back to standing on.
	if (pStanding && pStanding->m_ObjectType != OT_WORLDMODEL &&
	    pStanding->m_ObjectType != OT_CONTAINER)
	{
		if (wm_ObjectsIntersect(pStanding, pState->m_pObj,
		                        &pStanding->m_BBoxMin, &pStanding->m_BBoxMax,
		                        &pState->m_pObj->m_BBoxMin, &pState->m_pObj->m_BBoxMax,
		                        -0.01f, &bUsedGeometry))
			obj_SetStandingOn(pState->m_pObj, pStanding, LTNULL);
	}

	pState->m_pObj->m_ChangeFlags &= ~2;

	if (flags & 2)
	{
		((IWMSetupSource*)pState->m_pUnknown04)->_v00(pState->m_pObj, 2);

		if (bUseArg)
			((IWMSetupSource*)pState->m_pUnknown04)->_v00(pState->m_pObj, 0x200);
	}

	((IWMSetupSource*)pState->m_pUnknown04)->_v14(pState);

	*((IWMSetupSource*)pState->m_pUnknown04)->_v04() = pSavedRequest;
}


// 0x10020a2d — the fill from four loose arguments. The zero stores are
// interleaved between the first two argument stores, which is the scheduler
// filling the gaps between the four `mov 0xN(%esp),%eax` loads.
void CWMStateInit::Init(void* pContainer, void* pUnknown04, LTObject* pObj,
                        uint32 nPriority)
{
	wm_InitMoveState((WMMoveState*)this, pContainer, pUnknown04, pObj,
	                 nPriority);
}

// 0x1001efe8 — the same fill for the recursive move: three of the four values
// come off the record that is being recursed from and the object is the one
// being carried. The blocking priority is copied as the dword it already is
// in the source record, not re-read off the object — which is what makes this
// a separate entry point rather than a call to Init above.
//
// Inline in lithtech.exe only: the client expands it in wm_MoveObject and in
// the out-of-line wm_PushWithGeometry (0x00451e10), and its one out-of-line
// copy (0x00453ef0) is called only from the pusher bodies expanded into
// wm_PushWorldModel (0x00453357, 0x00453589, 0x004539dc), where the inline
// budget has run out. server.dll calls it everywhere.
WM_CINLINE void CWMStateInit::InitFrom(WMMoveState* pSrc, LTObject* pObj)
{
	wm_InitMoveState((WMMoveState*)this, pSrc->m_pContainer,
	                 pSrc->m_pUnknown04, pObj, pSrc->m_nUnknown0C);
}


// 0x1001f357 — the swept box. The object's own world box is copied into both
// destinations and then stretched *backwards* along the move: the box the
// caller hands in is the one at the position the object has already been put
// down at, so extending it against the move vector is what makes it cover
// where the object came from.
//
// The zero the six comparisons are against is loaded ONCE out of the literal
// pool (`flds`, not `fldz`) and kept in st(0) across the whole body. That is
// what puts the literal on the RIGHT of every comparison: written
// `pMove->x > 0.0f` VC6 reverses the compare and hoists the constant, written
// `0.0f < pMove->x` it emits `fldz` + `fcomps` at each of the six sites and
// the function is four bytes longer.
//
// Inline in lithtech.exe only, like wm_PushObject below: their one caller
// there, wm_PushIntersectingObjects (0x00451720), expands both, and the image
// has no out-of-line copy of either.
WM_CINLINE void wm_SweepBox(LTVector* pMinOut, LTVector* pMaxOut, LTVector* pMove,
                 LTVector* pBoxMin, LTVector* pBoxMax)
{
	*pMinOut = *pBoxMin;
	*pMaxOut = *pBoxMax;

	if (pMove->x > 0.0f)
		pMinOut->x -= pMove->x;
	else if (pMove->x < 0.0f)
		pMaxOut->x -= pMove->x;

	if (pMove->y > 0.0f)
		pMinOut->y -= pMove->y;
	else if (pMove->y < 0.0f)
		pMaxOut->y -= pMove->y;

	if (pMove->z > 0.0f)
		pMinOut->z -= pMove->z;
	else if (pMove->z < 0.0f)
		pMaxOut->z -= pMove->z;
}


// 0x1001f4ee — is this object one the push pass has to take seriously? The
// object's own vtable answers first (slot 0x34); failing that, a solid world
// model or container qualifies on its type alone.
LTBOOL wm_IsPushBlocker(LTObject* pObj)
{
	// Written with the negative exit first: that is what places the shared
	// `return 1` block last, where the shipped build has it, and makes both
	// type comparisons jump forward into it. ONE refusal (an `||`), not two
	// early `return LTFALSE`s: lithtech 0x451c40 shares a single `xor/pop/ret`
	// for the not-solid and wrong-type cases; two refusals gave /O2 two
	// copies (server.dll was identical either way).
	if (!((IWMMovableObject*)pObj)->_v34())
	{
		if (!(pObj->m_Flags & FLAG_SOLID) ||
		    (pObj->m_ObjectType != OT_WORLDMODEL &&
		     pObj->m_ObjectType != OT_CONTAINER))
			return LTFALSE;
	}

	return LTTRUE;
}


// 0x1001f51e — the notifications one contact between two objects produces.
// The contact record the move parked in the 0x6a8 object is cleared first
// (no point, no force), then each end that asked for touch notification is
// told about the other, and each end that is a container is told what is
// inside it. The two ends are handled by two separate written-out pairs, not
// a loop, and the second of each pair re-reads its object out of the
// parameter slot.
void wm_NotifyContact(IWMSetupSource* pSource, LTObject* pObj, LTObject* pOther)
{
	CollisionInfo* pRec = (CollisionInfo*)*pSource->_v04();
	LTVector       vZero;

	// MEMBER-STORE ORDER: shipped /O2 emits +0x00,+0x04,+0x08,+0x0c then
	// +0x14, so m_hPoly is written LAST. server.dll's /O1 copy emits it
	// first and is currently byte-identical, so that match survives only if
	// /O1 hoists the store eight slots -- which it demonstrably does here,
	// since the very next shipped instruction is itself hoisted from the
	// bottom of the function. Revert this if server drops.
	pRec->m_Plane.m_Normal.x = 0.0f;
	pRec->m_Plane.m_Normal.y = 0.0f;
	pRec->m_Plane.m_Normal.z = 0.0f;
	pRec->m_Plane.m_Dist = 0.0f;
	pRec->m_hPoly = 0;

	vZero.x = 0.0f;
	vZero.y = 0.0f;
	vZero.z = 0.0f;

	if (pObj->m_Flags & FLAG_TOUCH_NOTIFY)
		pSource->_v08(pObj, pOther, &vZero, 0.0f);

	if (pOther->m_Flags & FLAG_TOUCH_NOTIFY)
		pSource->_v08(pOther, pObj, &vZero, 0.0f);

	if (pObj->m_Flags & FLAG_CONTAINER)
		pSource->_v0c(pOther, pObj);

	if (pOther->m_Flags & FLAG_CONTAINER)
		pSource->_v0c(pObj, pOther);
}


// 0x1002056f — does this pair of objects need the real world-model geometry,
// or will the boxes do? Either end being a world model or container that did
// NOT ask for box physics is what forces the geometry (name ours).
// Inline in lithtech.exe only: expanded into wm_PushWorldModel, its one
// caller, and absent from the image as a function.
WM_CINLINE LTBOOL wm_PairNeedsGeometry(LTObject* pObj1, LTObject* pObj2)
{
	if (pObj1->m_ObjectType == OT_WORLDMODEL || pObj1->m_ObjectType == OT_CONTAINER)
	{
		if (!(pObj1->m_Flags & FLAG_BOXPHYSICS))
			return LTTRUE;
	}

	if (pObj2->m_ObjectType == OT_WORLDMODEL || pObj2->m_ObjectType == OT_CONTAINER)
	{
		if (!(pObj2->m_Flags & FLAG_BOXPHYSICS))
			return LTTRUE;
	}

	return LTFALSE;
}


// 0x100205af — the container query's callback: everything whose box overlaps
// the swept box is offered here and the ones worth pushing are kept.
//
// The cap and the overflow message's argument are the same 0x80, which is
// what puts it in a register rather than as two immediates.
void FindObjectsCB(LTObject* pObj, CFindObjectsQuery* pQuery)
{
	if (pObj->m_Unknown54)
		return;

	// The overflow report is the ELSE of the whole body, not an early return:
	// that is what puts its block after the append, where the shipped build
	// has it, with the count test jumping forward into it.
	if (pQuery->m_nObjects < WM_MAX_FOUND_OBJECTS)
	{
		if (pObj == pQuery->m_pState->m_pObj)
			return;

		uint32 nFlags = pObj->m_Flags;

		if (!(nFlags & (FLAG_SOLID | FLAG_TOUCH_NOTIFY | FLAG_CONTAINER)))
			return;

		if (!((CWMMoveSetup*)pQuery->m_pState)->m_Unknown18)
		{
			if (nFlags & FLAG_CLIENTNONSOLID)
				return;

			if (!(nFlags & FLAG_SOLID))
				return;
		}

		pQuery->m_ppObjects[pQuery->m_nObjects] = pObj;
		pQuery->m_nObjects++;
	}
	else
	{
		dsi_ServerError("FindObjectsCB: Overflowed (more than %d intersecting objects)!",
		                WM_MAX_FOUND_OBJECTS);
	}
}


// 0x1001f3e7 — one object out of the swept box: does the move have to shift
// it, and did shifting it work?
WM_CINLINE int wm_PushObject(WMMoveState* pState, LTObject* pObj)
{
	CWMMoveGeom* pGeom = (CWMMoveGeom*)pState;
	int          bBlocker;
	int          bUsedGeometry;
	int          nMoved;
	int          nUnused;

	if (pObj->m_ChangeFlags & 2)
		return 0;

	// A mover that may pass through the world simply ignores anything the
	// blocker test claims. Note the flag: FLAG_GOTHRUWORLD (bit 21, so
	// `testb $0x20,0x8a`), NOT the FLAG_SOLID one byte lower — reading it as
	// FLAG_SOLID makes the constant shared with the pair below and VC6 then
	// hoists 0x2000 into a callee-saved register for all three tests where
	// the shipped build narrows this one to a byte immediate.
	if (pState->m_pObj->m_Flags & FLAG_GOTHRUWORLD)
	{
		if (wm_IsPushBlocker(pObj))
			return 0;
	}

	if ((pObj->m_Flags & FLAG_SOLID) && (pState->m_pObj->m_Flags & FLAG_SOLID))
	{
		// Before the inner test, not after: that hoists the store above the
		// branch (the cylinder arm overwrites nothing, so the store is free
		// there) and saves the `jmp` over the else arm.
		bBlocker = 1;

		if (pState->m_pObj->m_Flags2 & FLAG2_PLAYERCOLLIDE)
			return wm_PushWorldModel(pState, pObj);
	}
	else
	{
		bBlocker = 0;
	}

	if (!wm_ObjectsIntersect(pState->m_pObj, pObj,
	                         &pGeom->m_vBoxMin, &pGeom->m_vBoxMax,
	                         &pObj->m_BBoxMin, &pObj->m_BBoxMax,
	                         0.0f, &bUsedGeometry))
		return 0;

	// The blocker test folds into bBlocker itself (0x1001f478: `test %esi;
	// jne push; call wm_IsPushBlocker; test %eax; je notify`). Written as
	// `bBlocker || wm_IsPushBlocker(pObj)` (either polarity, with or without
	// a result local) VC6 lays the notification out BEFORE the push arm;
	// through the variable the push arm is the fall-through, as shipped.
	if (!bBlocker)
		bBlocker = wm_IsPushBlocker(pObj);

	if (bBlocker)
	{
		if (bUsedGeometry)
			nMoved = wm_PushWithGeometry(pState, pObj, pGeom->m_pOrigPos,
			                             &pGeom->m_vPos, 1, &nUnused);
		else
			nMoved = wm_PushWithBox(pState, pObj, pGeom->m_pOrigPos, &pGeom->m_vPos);

		if (nMoved)
			pState->m_pObj->SetPos(pGeom->m_vPos);

		return nMoved;
	}
	else
	{
		wm_NotifyContact((IWMSetupSource*)pState->m_pUnknown04, pObj,
		                 pState->m_pObj);
	}

	return 0;
}


// 0x1001e088 — the frame-move worker (declared in commonphysics.h; the
// header's "not reconstructed" note predates this body). Integrates the
// velocity and acceleration the record points at over m_fFrameTime into
// m_vFrameMove: small values are snapped to zero, an object standing on
// something has its acceleration projected onto the ground plane (the
// poly's plane, or -m_Dir with no poly) and, on shallow ground, friction
// applied analytically through exp(); a non-gravity object at rest is
// stopped outright (velocity/acceleration zeroed, CHANGEFLAG_MOTION cleared,
// return 0). Every constant is .rdata: 0.1 (rest), 0.01, 1.0, 0.5.
//
// NEAR: 1616 vs 1615 bytes, 629/629 instructions, and the 23 calls (which
// MagSqr/Norm/Mag/ctor/operator sites are calls vs inline) are the shipped
// sequence exactly. What reached that, measured:
//   * the A1 projection's dot is written out (no Dot() call) and the
//     friction decay is `vNew *= fDecay; vNewVel = vTerm + vNew;` -- both
//     move VC6's /O1 inline budget; every other spelling tried either
//     inlined the fDot-branch constructor or left one Stop Init() a call.
// What is left (14 mnemonic blocks, raw diff ~166 lines):
//   * frame slots: shipped keeps vAccel, vTerm AND the second friction
//     vector all at -0x1c (one variable reused). Merging them here
//     (vTerm -> vAccel) fixes most slots but costs one inline unit (Stop's
//     second Init becomes a call); no compensating spelling was found
//     (~400 combinations of VEC_SET/VEC_DOT/scope/goto variants searched).
//   * the standing-at-rest test: shipped `jae; jmp Stop` (a cross-jumped
//     duplicate of the Stop block) vs our `jb Stop` from the goto; the
//     duplicated block needs one more inline unit too.
//   * operand order in exp(-(t*fric)) and a few copy schedulings.
// lithtech.exe (0x004500d0) is a further way off: our SDK Norm() is inline
// and /O2 expands it, which the engine's declaration-only Norm never was.
int phys_ComputeFrameMove(GlobalForceControl* pControl)
{
	LTObject* pObj;
	LTVector* pOffset;
	LTVector* pNormal;
	float     fHalfTimeSqr;
	float     fVelMag;
	float     fAccelMag;
	float     fMag;
	float     fDecay;
	LTVector  vAccel;
	LTVector  vDir;
	LTVector  vUp;
	LTVector  vNew;
	LTVector  vTerm;
	LTVector  vNewVel;
	LTVector  vDelta;

	fHalfTimeSqr = pControl->m_fFrameTime * pControl->m_fFrameTime * 0.5f;
	pObj = pControl->m_pObj;

	fVelMag   = pControl->m_pVelocity->MagSqr();
	fAccelMag = pControl->m_pAcceleration->MagSqr();

	if (fVelMag < 0.1f)
	{
		pControl->m_pVelocity->Init();
		fVelMag = 0.0f;
	}

	if (fAccelMag < 0.1f)
	{
		pControl->m_pAcceleration->Init();
		fAccelMag = 0.0f;
	}

	pOffset = &pControl->m_vFrameMove;
	pOffset->Init();

	if (pControl->m_nObjFlags & FLAG_GRAVITY)
	{
		if (pObj->m_pStandingOn)
		{
			if (fVelMag < 0.1f && fAccelMag < 0.1f)
				goto Stop;

			if (pObj->m_pStandingOnPoly)
			{
				pNormal = &pObj->m_pStandingOnPoly->GetPlane()->m_Normal;
			}
			else
			{
				pNormal = &vUp;
				vUp = -pControl->m_Dir;
			}

			vAccel = *pControl->m_pAcceleration;
			vAccel += pControl->m_Force;

			vDir = pControl->m_Force;
			vDir.Norm();

			if (vDir.Dot(*pNormal) > pControl->m_Unknown30)
			{
				fMag = vAccel.Mag();

				*pControl->m_pAcceleration = vAccel - *pNormal * (vAccel.z * pNormal->z + (vAccel.y * pNormal->y + vAccel.x * pNormal->x));

				float fDot = pControl->m_pAcceleration->Dot(vDir);
				if (fDot < 0.0f)
					*pControl->m_pAcceleration -= vDir * fDot;

				pControl->m_pAcceleration->Norm(fMag);
			}
			else
			{
				vNew = *pControl->m_pVelocity + pControl->m_Force * pControl->m_fFrameTime;

				if (vNew.Dot(*pNormal) > 0.01f)
				{
					*pControl->m_pAcceleration = vAccel;
				}
				else
				{
					if (pControl->m_pAcceleration->Dot(*pNormal) > 0.01f)
					{
						fMag = pControl->m_pAcceleration->Mag();

						*pControl->m_pAcceleration -= *pNormal *
							(pNormal->Dot(*pControl->m_pAcceleration) + 1.0f);

						pControl->m_pAcceleration->Norm(fMag);
					}

					if (pObj->m_FrictionCoefficient > 0.0f)
					{
						fDecay = (float)exp(-(pObj->m_FrictionCoefficient * pControl->m_fFrameTime));

						vTerm = *pControl->m_pAcceleration / pObj->m_FrictionCoefficient;
						vNew = *pControl->m_pVelocity - vTerm;
						vNew *= fDecay;
						vNewVel = vTerm + vNew;

						*pOffset = vTerm * pControl->m_fFrameTime;

						vTerm = *pControl->m_pVelocity * pObj->m_FrictionCoefficient;
						vTerm -= *pControl->m_pAcceleration;
						vTerm *= (1.0f - fDecay) / pObj->m_FrictionCoefficient / pObj->m_FrictionCoefficient;
						*pOffset += vTerm;

						pObj->m_Velocity = vNewVel;
						return 1;
					}
				}
			}
		}
		else
		{
			*pControl->m_pAcceleration += pControl->m_Force;
		}
	}
	else if (fVelMag < 0.1f && fAccelMag < 0.1f)
	{
Stop:
		pObj->m_Velocity.Init();
		pObj->m_Acceleration.Init();
		pObj->m_ChangeFlags &= ~CHANGEFLAG_MOTION;
		return 0;
	}

	vDelta = *pControl->m_pAcceleration * pControl->m_fFrameTime;
	*pOffset = *pControl->m_pVelocity * pControl->m_fFrameTime;
	*pOffset += *pControl->m_pAcceleration * (fHalfTimeSqr * 0.5f);

	pObj->m_Velocity += vDelta;
	*pControl->m_pVelocity = pObj->m_Velocity;

	return 1;
}


// 0x1001e6d7 — LTVector's three-float constructor, out of line. The stores
// are integer `mov`s: a float parameter copied straight into a float member
// never touches the FPU.
CVectorInit::CVectorInit(float _x, float _y, float _z)
{
	x = _x;
	y = _y;
	z = _z;
}


// 0x1001ff57 — the smallest of the six box separations.
//
// Read off the bytes:
//
//   * all four boxes are indexed by ONE induction variable: the shipped
//     prologue subtracts the first pointer from the other three and then
//     addresses them `(%reg,%esi,1)` with esi walking four bytes a step. So
//     the source subscripts all four with the same `[i]`, and the component
//     names are not what it used.
//   * the running best is a plain float local initialised to 1.0e37f with an
//     integer immediate store (`movl $0x7cf0bdc2`), not an `flds`.
//   * `fabs` is the CRT's double one, reached through the two-push
//     `fstpl (%esp)` form, and the two calls come out LEFT operand first —
//     the candidate's `fabs` is computed before the running best's, so the
//     comparison is written `fabs(f) < fabs(fBest)`. Written the other way
//     round the two calls swap, the candidate has to be reloaded off its
//     slot instead of staying in `st`, and the function is six bytes longer.
//   * the candidate difference lives in the dead first parameter's slot: it
//     is stored there as a float (`fsts 0x8(%ebp)`) off the same `st` value
//     the `fabs` call then widens.
void wm_SmallestSeparation(LTVector* pMin1, LTVector* pMax1,
                           LTVector* pMin2, LTVector* pMax2,
                           LTVector* pOut, int* pnSide)
{
	float fBest = 1.0e37f;
	int   nAxis = 0;
	int   nCount = 0;
	int   nSide = 0;
	int   i;

	for (i = 0; i < 3; i++)
	{
		float f;

		f = ((float*)pMax2)[i] - ((float*)pMin1)[i];

		if (fabs(f) < fabs(fBest))
		{
			fBest = f;
			nAxis = i;
			nSide = nCount;
		}

		nCount++;

		f = ((float*)pMin2)[i] - ((float*)pMax1)[i];

		if (fabs(f) < fabs(fBest))
		{
			fBest = f;
			nAxis = i;
			nSide = nCount;
		}

		nCount++;
	}

	pOut->x = 0.0f;
	pOut->y = 0.0f;
	pOut->z = 0.0f;

	((float*)pOut)[nAxis] = fBest;

	*pnSide = nSide;
}


// 0x1001f9ac — the bridge into worldsim's box move.
//
// Read off the bytes:
//
//   * the request record the move state points at is the CollisionInfo the
//     0x6a8 object is currently serving — the same 0x24 bytes wm_MoveObject
//     parks there and the contact notifier clears.
//   * the standing flag lives in the dead `pEnd` parameter slot: the vector
//     is copied out of the result before the flag is first written.
//   * `bStanding = 1` is hoisted above the standing-polygon test, so it is
//     written before that `if` and not inside it.
//   * the stopping momentum's squared length stays in `st` across the limit
//     comparison and the `sqrt`, so it is one float local, not two reads.
int wm_MoveThroughSim(WMMoveState* pState, IWorldBsp* pWorldBsp,
                      HOBJECT hObject, LTObject* pObj,
                      LTVector* pStart, LTVector* pEnd,
                      int bPushOut, int bStandOn)
{
	CMoveState   move;
	CBounceState result;
	int          bStanding;

	move.m_n44 = 0;

	if (pObj->m_Flags & FLAG_GOTHRUWORLD)
		return 0;

	IWMSetupSource* pSource = (IWMSetupSource*)pState->m_pUnknown04;

	move.m_pHost      = (IPhysicsHost*)pSource;
	move.m_pUnknown08 = ((CWMMoveSetup*)pState)->m_Unknown18;
	move.m_pInfo      = (CollisionInfo*)*pSource->_v04();
	move.m_pWorldBsp  = pWorldBsp;
	move.m_hObject    = hObject;
	move.m_vStart     = *pStart;
	move.m_vEnd       = *pEnd;
	move.m_vDims      = pObj->m_Dims;
	move.m_n38        = (pObj->m_Flags >> 18) & 1;
	move.m_pObject    = pObj;
	move.m_bPushOut   = bPushOut;
	move.m_n44        = pState->m_nUnknown64;

	ws_MoveObject(&move, &result);

	*pEnd = result.m_vPoint;

	if (bStandOn)
	{
		if (wm_IsPushBlocker(hObject) || (pObj->m_Flags & FLAG_SOLID))
		{
			bStanding = 1;

			if (result.m_pStandingPoly)
				obj_SetStandingOn(pObj, hObject, result.m_pStandingPoly);
		}
		else
		{
			bStanding = 0;
		}

		if (result.m_nHits > 0)
		{
#ifdef LT_SERVER_BUILD
			obj_MaybeRetransformWorldModel(pObj, 1);
#else
			obj_MaybeRetransformWorldModelInline(pObj, 1);
#endif

			float fMagSqr = result.m_vMomentum.MagSqr();

			if (fMagSqr >= pObj->m_ForceIgnoreLimitSqr)
				((IWMSetupSource*)pState->m_pUnknown04)->_v08(
					pObj,
					((CollisionInfo*)*((IWMSetupSource*)pState->m_pUnknown04)->_v04())->m_hObject,
					&result.m_vVelocityAdd,
					(float)sqrt(fMagSqr));

			if (bStanding)
			{
				LTVector* pVel = &pObj->m_Velocity;

				pVel->x += result.m_vVelocityAdd.x;
				pVel->y += result.m_vVelocityAdd.y;
				pVel->z += result.m_vVelocityAdd.z;
			}
		}
	}

	return (result.m_nHits > 0) ? 1 : 0;
}


// 0x1001eae7 — everything a world model needs after its position or rotation
// changed: rebuild its own transform, re-derive the object's dims from the
// geometry as transformed, then push the geometry itself through. The three
// calls share one stack cleanup (`add $0x14,%esp`), which is what says all
// three were compiled against fixed prototypes (name ours).
void obj_RetransformWorldModelDims(LTObject* pObj)
{
	obj_UpdateTransform(pObj);
	wm_UpdateDimsFromGeometry(pObj, &((WorldModelObject*)pObj)->m_Transform1f0);
#ifdef LT_SERVER_BUILD
	obj_MaybeRetransformWorldModel(pObj, 1);
#else
	// 0x00450fc0 (52 bytes): the client expands the call -- the type byte
	// is read and tested against 2 / 9 inline before 0x00450c30 -- as at
	// wm_MoveThroughSim's site.
	obj_MaybeRetransformWorldModelInline(pObj, 1);
#endif
}


// A non-strict min, candidate first. The max side turned out to be the
// SDK's own strict LTMAX with the running value first -- the same predicate
// as a non-strict max with the candidate first, but a different operand
// order, and lithtech.exe's /O2 build (0x450c60) can tell the two apart (see
// wm_UpdateDimsFromGeometry). The min side has not been shown to be
// LTMIN(vMin, pt): written that way the lithtech copy reschedules the
// inlined MatVMul_InPlace_H (server unchanged), so WM_MIN stays.
#define WM_MIN(a, b) ((a) <= (b) ? (a) : (b))

// 0x1001e8c8 — the world model's dims, from its geometry as transformed.
//
// The geometry's own axis-aligned box is halved, the eight corners of that box
// around the object's position are built, each is taken through the
// transform, and the object's dims become half the extent of the transformed
// set. A world whose BSP says it is busy is left alone.
//
// Read off the bytes:
//
//   * the eight corners are written out one component at a time, not built in
//     a loop: VC6 shares the six sums (pos ± half, per axis) between them and
//     keeps two of the six in `st` for the whole run, which is why the
//     corner order is +++ / +-+ / ++- / +-- / -++ / --+ / -+- / ---.
//   * both min/max searches ARE the VEC_MIN/VEC_MAX shape — one assignment
//     whose value is a conditional expression, so the kept value goes through
//     `st` (`flds`/`fstps`) where three separate `if`+assign statements would
//     copy it with an integer `mov`. The shipped `ja`/`jb` (candidate
//     loaded, running value as the memory operand) are the predicates
//     "candidate <= min" and "candidate >= max". The max side is the SDK's
//     strict LTMAX with the operands the other way round (LTMAX(vMax, pt));
//     the min side is WM_MIN below (see the note there).
//   * the frame is exactly `sub $0x90` — 8*12 for the corners and 48 for
//     everything else. At /O1 the block-scoped size/half pair shares its two
//     slots with the function-scoped min/max pair (min over size, max over
//     half); lithtech.exe's /O2 twin (0x450c60) lays them out the other way
//     round, which is what shows they are four variables and not two reused.
//   * the box size and the final extent are _CVector operator expressions.
//     operator- takes its right operand BY VALUE: that copy is the `movsl`
//     of m_vBoxMin into the frame, and `this` = &m_vBoxMax is why the world
//     pointer is advanced by `add $0xd0`. The final `(vMax - vMin) * 0.5f`
//     reloads the half at each `fmuls`, where three scalar statements keep
//     it in `st`.
void wm_UpdateDimsFromGeometry(LTObject* pObj, LTMatrix* pTransform)
{
	LTVector vPts[8];
	LTVector vMin;
	LTVector vMax;
	int      i;

	if (pObj->m_pWorldBsp->IsBusy())
		return;

	{
	LTVector vSize;
	LTVector vTemp;

	vSize = pObj->m_pWorldBsp->m_vBoxMax - pObj->m_pWorldBsp->m_vBoxMin;

	vTemp = vSize;
	vTemp.x *= 0.5f;
	vTemp.y *= 0.5f;
	vTemp.z *= 0.5f;

	vPts[0].x = pObj->m_Pos.x + vTemp.x;
	vPts[0].y = pObj->m_Pos.y + vTemp.y;
	vPts[0].z = pObj->m_Pos.z + vTemp.z;

	vPts[1].x = pObj->m_Pos.x + vTemp.x;
	vPts[1].y = pObj->m_Pos.y - vTemp.y;
	vPts[1].z = pObj->m_Pos.z + vTemp.z;

	vPts[2].x = pObj->m_Pos.x + vTemp.x;
	vPts[2].y = pObj->m_Pos.y + vTemp.y;
	vPts[2].z = pObj->m_Pos.z - vTemp.z;

	vPts[3].x = pObj->m_Pos.x + vTemp.x;
	vPts[3].y = pObj->m_Pos.y - vTemp.y;
	vPts[3].z = pObj->m_Pos.z - vTemp.z;

	vPts[4].x = pObj->m_Pos.x - vTemp.x;
	vPts[4].y = pObj->m_Pos.y + vTemp.y;
	vPts[4].z = pObj->m_Pos.z + vTemp.z;

	vPts[5].x = pObj->m_Pos.x - vTemp.x;
	vPts[5].y = pObj->m_Pos.y - vTemp.y;
	vPts[5].z = pObj->m_Pos.z + vTemp.z;

	vPts[6].x = pObj->m_Pos.x - vTemp.x;
	vPts[6].y = pObj->m_Pos.y + vTemp.y;
	vPts[6].z = pObj->m_Pos.z - vTemp.z;

	vPts[7].x = pObj->m_Pos.x - vTemp.x;
	vPts[7].y = pObj->m_Pos.y - vTemp.y;
	vPts[7].z = pObj->m_Pos.z - vTemp.z;

	}

	vMin.x = 1.0e37f;
	vMin.y = 1.0e37f;
	vMin.z = 1.0e37f;

	vMax.x = -1.0e37f;
	vMax.y = -1.0e37f;
	vMax.z = -1.0e37f;

	for (i = 0; i < 8; i++)
	{
		MatVMul_InPlace_H(pTransform, &vPts[i]);

		vMin.x = WM_MIN(vPts[i].x, vMin.x);
		vMin.y = WM_MIN(vPts[i].y, vMin.y);
		vMin.z = WM_MIN(vPts[i].z, vMin.z);

		vMax.x = LTMAX(vMax.x, vPts[i].x);
		vMax.y = LTMAX(vMax.y, vPts[i].y);
		vMax.z = LTMAX(vMax.z, vPts[i].z);
	}

	// Byte-identical on BOTH images. The max side is the SDK's LTMAX with
	// the running value FIRST: lithtech.exe keeps vMax.z in `st` across the
	// loop and compares it as `flds pt ; fld %st(1) ; fcompp` (0x450c60),
	// which only `(vMax.z) > (pt)` -- parenthesised left operand, a register
	// value re-pushed rather than folded -- produces. WM_MAX(pt, vMax.z)
	// gave `fcomp %st(1)` there; server.dll is identical either way.
	vMin = (vMax - vMin) * 0.5f;

	((CObjDimsSetter*)pObj)->SetDims(vMin);
}


// 0x1002115c -- the per-object callback wm_RotateWorldModel hands the
// container's box query: push this object unless it is excluded (LTObject's
// +0x54) or is the world model doing the pushing (names ours).
void wm_RotatePushFilter(LTObject* pObj, WMMoveState* pState);
void wm_RotatePushObject(WMMoveState* pState, LTObject* pObj);   // 0x10020e31

// 0x10020a53 -- turn a world model to a new rotation and take everything it
// was carrying, and everything it now overlaps, round with it (name ours).
//
// The object is re-rotated first (dims re-derived from the transformed
// geometry, box re-seated in the container); only then do the third argument
// and FLAG_BOXPHYSICS decide whether anything is carried. The rotation is
// expressed as the 4x4 T(+pos) * R(new) * R(old)^T * T(-pos), parked at
// pState->m_pRotation for wm_RotatePushObject, and every object standing on
// the world model is moved to where that 4x4 puts it, half a unit low.
//
// lithtech.exe's twin is 0x00454530 (src/engine/lithtech/wmrotate.cpp, read
// against the client bytes), which has the counter and not the CountAdder --
// the same LT_SERVER_TIMERS split sm_MoveObject has. /O1 calls out of line
// what the client inlines: CWMMoveSetup::Setup, LTMatrix::Init (0x10011dc5),
// MatTranspose3x3 (0x10020e08), obj_MaybeRetransformWorldModel, memset and
// MatVMul_H (0x1000f878).
//
// lithtech.exe (0x00454530, 1285 bytes) is NEAR at 1297 after the two
// client-only expansions (CWMMoveSetup::Setup and the retransform guard).
// Left: the /O2 image copies vPos into matFromOrigin's translation column as
// INTEGER moves (0x004545b4 `mov %edx,0x70(%esp)`, 0x00454693, 0x004546bb)
// where Init's float parameters go through the FPU here, and MatVMul_H's
// sums come out z,x,y in the carry loop (0x00454913). Written as direct
// element stores, VC6 constant-folds the whole matrix into the MatMul
// (1394 bytes); Init over pObj->m_Pos is 1301. Neither is the shape.
// matFromOrigin.Init(identity) + SetTranslation(vPos) (or three member
// stores after it) gives the integer copies and is SIZE-EXACT on lithtech
// (1285/310, 47 insns differ: the m[1][3]/m[2][3] stores land after the
// constants, shipped has them in Init's element order) -- but server.dll
// PUSHES vPos.x/y/z as Init's arguments (0x10020a53, 973 vs 949 with the
// SetTranslation form), so the source is this Init and the integer copy is
// /O2's lowering of the inlined float argument, not a spelling. Identity()
// instead of the explicit Init leaves Init a call (1121/1177).
//
// The carry list keeps wm_MoveObject's shipped off-by-one: the cap is
// `cmpl $0x20,0xc(%ebp); jge` (0x10020cd6), admitting an index of 0x20 into
// an array of 0x20 -- found[] ends exactly at the saved %ebp.
void wm_RotateWorldModel(WMMoveState* pState, LTRotation* pRotation, int bUnknown)
{
	CWMStandRec   found[WM_MAX_STANDING];
	CWMRequestRec rec;
	WMMoveState   state;
	LTMatrix      matToOrigin;
	LTMatrix      matFromOrigin;
	LTMatrix      matObjRot;
	LTMatrix      matNewRot;
	LTMatrix      matTemp;
	LTMatrix      matRotation;
	LTVector      vPos;
	LTVector      vDest;
	void*         pSavedRequest;
	LTObject*     pObj;
	int           nFound;

	state.m_pContainer = LTNULL;
	state.m_pUnknown04 = LTNULL;
	state.m_pObj       = LTNULL;
	state.m_nUnknown64 = 0;

	g_Unknown58728++;

#ifdef LT_SERVER_TIMERS
	CountAdder cntAdder((uint32*)&g_Unknown5872c);
#endif

	((CWMMoveSetup*)pState)->Setup();

	pObj = pState->m_pObj;
	vPos = pObj->m_Pos;

	matToOrigin.Init(1.0f, 0.0f, 0.0f, -vPos.x,
	                 0.0f, 1.0f, 0.0f, -vPos.y,
	                 0.0f, 0.0f, 1.0f, -vPos.z,
	                 0.0f, 0.0f, 0.0f, 1.0f);

	matFromOrigin.Init(1.0f, 0.0f, 0.0f, vPos.x,
	                   0.0f, 1.0f, 0.0f, vPos.y,
	                   0.0f, 0.0f, 1.0f, vPos.z,
	                   0.0f, 0.0f, 0.0f, 1.0f);

	// Where it points now, inverted (a rotation's transpose is its inverse),
	// and where it is being asked to point.
	quat_ConvertToMatrix(pObj->m_Rotation.m_Quat, matObjRot.m);
	MatTranspose3x3(&matObjRot);

	quat_ConvertToMatrix(pRotation->m_Quat, matNewRot.m);

	pObj->m_Rotation = *pRotation;

	obj_UpdateTransform(pObj);
	wm_UpdateDimsFromGeometry(pObj, &((WorldModelObject*)pObj)->m_Transform1f0);
#ifdef LT_SERVER_BUILD
	obj_MaybeRetransformWorldModel(pObj, 1);
#else
	// 0x00454786..0x0045479d: the client expands it (type 2/9 test, then
	// 0x00450c30), exactly as at wm_MoveThroughSim's site.
	obj_MaybeRetransformWorldModelInline(pObj, 1);
#endif

	pState->m_pContainer->BumpObject(pState->m_pObj, 0);

	if (!bUnknown)
		return;

	if (pObj->m_Flags & FLAG_BOXPHYSICS)
		return;

	MatMul(&matRotation, &matFromOrigin, &matNewRot);
	MatMul(&matTemp, &matRotation, &matObjRot);
	MatMul(&matRotation, &matTemp, &matToOrigin);

	pSavedRequest = *((IWMSetupSource*)pState->m_pUnknown04)->_v04();
	*((IWMSetupSource*)pState->m_pUnknown04)->_v04() = &rec;

	memset(&rec, 0, sizeof(rec));

	nFound = -1;

	{
		LTLink* pCur;

		for (pCur = pObj->m_StandingOnMe.m_pNext;
		     pCur != &pObj->m_StandingOnMe; )
		{
			LTObject* pOn = (LTObject*)pCur->m_pData;

			pCur = pCur->m_pNext;

			if (!(pOn->m_Flags & FLAG_DONTFOLLOWSTANDING) && nFound < WM_MAX_STANDING)
			{
				nFound++;

				found[nFound].m_pObj    = pOn;
				found[nFound].m_vOffset = pOn->m_Pos;
			}
			else
			{
				obj_UnlinkTrackersA(pOn);
			}
		}
	}

	pState->m_pRotation = &matRotation;

	// Either the caller named the objects to push, or the container is asked
	// which ones the new box touches.
	if (pState->m_pPushList)
	{
		uint32 i;

		for (i = 0; i < pState->m_nPushCount; i++)
			wm_RotatePushObject(pState, ((LTObject**)pState->m_pPushList)[i]);
	}
	else
	{
		((IBoxQueryWorld*)pState->m_pContainer)->FindObjectsInBox(
			&pState->m_pObj->m_BBoxMin, &pState->m_pObj->m_BBoxMax,
			(FindObjectsFn)wm_RotatePushFilter, (CFindObjectsQuery*)pState, 0);
	}

	if (nFound >= 0)
	{
		int i;

		for (i = nFound; i >= 0; i--)
		{
			LTObject* pOn = found[i].m_pObj;

			MatVMul_H(&vDest, &matRotation, &found[i].m_vOffset);

			wm_InitMoveState(&state, pState->m_pContainer, pState->m_pUnknown04,
			                 pOn, pOn->m_BlockingPriority);

			vDest.y -= 0.5f;

			wm_MoveObject(&state, vDest, 7);
		}
	}

	*((IWMSetupSource*)pState->m_pUnknown04)->_v04() = pSavedRequest;
}


// LTObject's virtual at slot 0x34 again, and the third view of it in this
// tree (objbank.cpp and servercommon.cpp have their own). It answers whether
// the object takes part in the test the caller is running;
// ltobject_internal.h declares the slot `void` and localdriver.cpp overrides
// it that way, so the return value has to be reached through a
// layout-identical view. The vptr is the object's own, at offset 0, which is
// where the shipped `mov (%ebx),%eax` reads it (names ours).
class IObjPushTestView
{
public:
	virtual void _p00()=0; virtual void _p04()=0; virtual void _p08()=0;
	virtual void _p0c()=0; virtual void _p10()=0; virtual void _p14()=0;
	virtual void _p18()=0; virtual void _p1c()=0; virtual void _p20()=0;
	virtual void _p24()=0; virtual void _p28()=0; virtual void _p2c()=0;
	virtual void _p30()=0;
	virtual int  TakesPartInPush()=0;        // 0x34
};


// 0x10020e31 — one object out of the way of a ROTATING world model, and the
// per-object worker wm_RotateWorldModel (0x10020a53) hands every overlapping
// object to (name ours).
//
// The rotation is the 4x4 at the move state's +0x60: the object's position
// goes through it to find where the rotation would carry that object, and the
// difference between there and where it is now is the direction to walk it
// out along. The walk is at most 199 unit steps, and it stops as soon as the
// object's box clears the world model's transformed BSP. Then the object is
// really moved, and if it is STILL inside afterwards the pair is reported
// through the setup source's slot 0x1c -- which is the crush case.
//
// Read off the bytes rather than guessed:
//
//   * the object's VELOCITY is how the push vector reaches wm_MoveThroughSim.
//     m_Velocity is saved into a local, overwritten with `vObjPos - vPush`
//     for the duration of the call and restored straight after, so that
//     argument travels through the object rather than the argument list.
//   * the pObj parameter's slot is the step counter. pObj is in %ebx from the
//     first instruction, and the loop reads the slot, decrements it and
//     branches on the OLD value -- `while (nSteps-- > 0)`, so 199 gives 199
//     steps and the last one is taken with the counter already at zero.
//   * `bPushOut` is `!(pObj->m_Flags & FLAG_NOSLIDING)`, unpacked with
//     `not`/`shr $0x19`/`and $1` rather than a test and a branch.
//   * the two box corners are built with the operands in OPPOSITE orders --
//     `vPush - m_Dims` for the low one and `m_Dims + vPush` for the high --
//     and each gets its own by-value copy of m_Dims, which is why the frame
//     carries two twelve-byte scratches for one field.
//   * nothing is returned. Two of the exits leave the FPU status word in
//     %ax and one leaves a void call's %eax, so the shipped function is
//     void and the `ret` is bare.
//
// STATUS: size-exact near, 811/811 and 276/276 with ZERO mnemonic blocks.
// What is left is frame-slot assignment only: pMoving is homed in the dead
// pObj parameter slot 0xc(%ebp) and nSteps gets a stack slot, where shipped
// has nSteps at 0xc(%ebp) (0x10020f82 `movl $0xc7,0xc(%ebp)`) and pMoving at
// -0x1c (0x10020e7f), and the twelve-byte slots below shift to fit. Not
// moved by declaration order of either, by a block-scoped nSteps, or by
// setting nSteps at the top of the function.
//
// Two source facts that got it here, both read off the bytes:
//   * the bsp test and the FLAG_SOLID test read pState->m_pObj afresh
//     (0x10020e98 `mov 0x8(%esi),%eax`, 0x10020eb5 `mov 0x8(%esi),%ecx`),
//     and wm_NotifyContact is handed that same %ecx (0x1002114b); pMoving
//     is only stored at 0x10020e7f and read back from -0x1c later.
//   * the velocity restore is ONE statement after the call, before the
//     test, with the result in a local: shipped recomputes
//     `lea 0xc4(%ebx),%edi` after the call (0x10021049). Two restores in an
//     if/else made VC6 cache &m_Velocity in a frame slot, and together with
//     the fresh pState->m_pObj reads flipped %ebx from pObj to pState.
void wm_RotatePushObject(WMMoveState* pState, LTObject* pObj)
{
	LTVector    vPush;         // -0x0c
	LTVector    vTmp;          // -0x18
	LTObject*   pMoving;       // -0x1c
	LTVector    vObjPos;       // -0x28
	LTVector    vScratch;      // -0x34
	LTVector    vScratch2;     // -0x40
	LTVector    vDir;          // -0x4c
	LTVector    vSaveVel;      // -0x58
	LTVector    vTestMax;      // -0x64
	LTVector    vTestMin;      // -0x70
	WMMoveState move;          // -0xd8
	// No frame slot of its own: every one of the eleven above is accounted
	// for and the frame is exactly 0xd8, so this lands in the dead pObj
	// parameter slot -- pObj lives in a callee-saved register from the
	// first instruction.
	int         nSteps;
	int         bMoved;

	move.m_pContainer = LTNULL;
	move.m_pUnknown04 = LTNULL;
	move.m_pObj       = LTNULL;
	move.m_nUnknown64 = 0;

	if (((IObjPushTestView*)pObj)->TakesPartInPush())
		return;

	pMoving = pState->m_pObj;

	if (!wm_BoxesOverlap(&pMoving->m_BBoxMin, &pMoving->m_BBoxMax,
	                     &pObj->m_BBoxMin, &pObj->m_BBoxMax, 0.01f))
		return;

	if (!bsp_IntersectBox(pState->m_pObj->m_pWMIface->GetTransformedBsp(),
	                      &pObj->m_BBoxMin, &pObj->m_BBoxMax))
		return;

	// Either end not solid and there is nothing to push: the two are told
	// they touched and that is all.
	if (!(pState->m_pObj->m_Flags & FLAG_SOLID) || !(pObj->m_Flags & FLAG_SOLID))
	{
		wm_NotifyContact((IWMSetupSource*)pState->m_pUnknown04,
		                 pState->m_pObj, pObj);
		return;
	}

	vObjPos = pObj->m_Pos;

	MatVMul_H(&vPush, pState->m_pRotation, &vObjPos);

	// Where the rotation would carry it, against where it is: no distance
	// means the rotation does not move this object and there is no direction
	// to push it along.
	vDir = vPush - vObjPos;

	if (vDir.MagSqr() < 0.001f)
		return;

	// pState->m_pObj, NOT the cached pMoving: 0x10020f56 is
	// `mov esi,[eax+0x8]`, a fresh load of the member, followed by
	// `add esi,0x18c` to reach m_Pos. VC6 does not re-read a local it
	// already has in a register, so the source names the member here.
	vDir = pObj->m_Pos - pState->m_pObj->m_Pos;
	vDir.Norm(1.0f);

	nSteps = 199;

	do
	{
		vTestMin = vPush - pObj->m_Dims;
		// OPERAND ORDER IS A TWELVE-BYTE FACT HERE, not allocation.
		// _CVector<T>::operator+ takes its right operand BY VALUE
		// (src/LT2/sdk/inc/ltvector.h), so the right-hand side gets a 12-byte
		// stack copy while the left is read straight out of the object as
		// `this`. Both shipped images copy m_Dims twice (server 0x10020fa9
		// and 0x10020fd5, client 0x00454cc3 and 0x00454d05), so m_Dims is
		// the RIGHT operand at both sites -- which is the line above's
		// order, not this one's.
		vTestMax = vPush + pObj->m_Dims;

		if (!bsp_IntersectBox(pMoving->m_pWMIface->GetTransformedBsp(),
		                      &vTestMin, &vTestMax))
			break;

		vPush += vDir;
	}
	while (nSteps-- > 0);

	vSaveVel = pObj->m_Velocity;

	pObj->m_Velocity = vObjPos - vPush;

	bMoved = wm_MoveThroughSim(pState, (IWorldBsp*)pMoving->m_pWMIface,
	                           (HOBJECT)pState->m_pObj, pObj, &vPush, &vObjPos,
	                           !(pObj->m_Flags & FLAG_NOSLIDING), 1);

	pObj->m_Velocity = vSaveVel;

	if (!bMoved)
		return;

	move.m_pContainer = pState->m_pContainer;
	move.m_pUnknown04 = pState->m_pUnknown04;
	move.m_pObj       = pObj;
	move.m_nUnknown0C = pObj->m_BlockingPriority;
	move.m_pPushList  = LTNULL;
	move.m_nPushCount = 0;

	wm_MoveObject(&move, vObjPos, 7);

	if (!bsp_IntersectBox(pMoving->m_pWMIface->GetTransformedBsp(),
	                      &pObj->m_BBoxMin, &pObj->m_BBoxMax))
		return;

	((IWMSetupSource*)pState->m_pUnknown04)->_v1c(pObj, pState->m_pObj);
}


// 0x1002115c -- see the declaration above wm_RotateWorldModel. The +0x54 is
// read off ARGUMENT ONE, the object the box query hands in; the +0x08 it
// compares against is the move state's m_pObj. lithtech.exe's twin is
// 0x00454f00 (sub_454f00 in src/engine/lithtech/lt2new1.cpp).
void wm_RotatePushFilter(LTObject* pObj, WMMoveState* pState)
{
	if (!pObj->m_Unknown54 && pObj != pState->m_pObj)
		wm_RotatePushObject(pState, pObj);
}


// ======================================================================= //
//  wm_PushWithGeometry (0x00451e10) and wm_PushWithBox (0x004526f0).
//
//  This file already DECLARED both (above) and CALLED both, and never
//  defined them.  Reconstructed from the retail lithtech.exe as part of a
//  twelve-function cluster covering 0x00451000..0x00452800, of which
//  these two were the only members not already present here -- the other
//  ten are the bodies above, and four of those are already byte-exact
//  out of this file's own object.
// ======================================================================= //
// ======================================================================= //
//  0x00451e10  wm_PushWithGeometry            1691 bytes, 17 calls
//
//  NEW DECODE.  commonphysics.cpp only DECLARES this function (from
//  wm_PushObject's call site); nothing had ever read its body.  Cross-checked
//  instruction by instruction against the server's /O1 copy at 0x1001f5cd,
//  which keeps an %ebp frame and is therefore much easier to read -- every
//  statement below is visible in both.
//
//  WHAT IT IS: push one object clear of a moving world model using the
//  model's real geometry.  Two arms, and which one runs depends on which END
//  of the pair is the world model:
//
//    * the MOVER is the world model (0x00451e54..0x004521d3): the object is
//      swept BACK from where the model's move would leave it (its own
//      position plus the model's move, plus half a unit along the move
//      direction) to where it is now, so the sim stops it at the last
//      non-penetrating point.
//    * the mover is NOT a world model (0x004521d4..0x00452478): then the
//      thing it hit is, so the MOVER is swept from pOrigPos to pNewPos and
//      the world model is moved out of the way by whatever the mover could
//      not travel.
//
//  Either way the answer is "did I have to pull the mover's target back",
//  and the correction is written through pNewPos -- which is what
//  wm_PushObject then hands to SetPos.
//
//  SIGNATURE, and how it was read: entry `mov 0xd4(%esp),%ecx; xor %eax,%eax;
//  mov %eax,(%ecx)` at 0x00451e16 makes argument 6 an `int*` cleared on the
//  way in and set to 1 at 0x00451fea / 0x0045223e, so it is an out-parameter.
//  Arguments 1..5 come straight off the two wm_MoveThroughSim call sites,
//  whose eight pushes are all visible (0x00451f7e..0x00451fd1 and
//  0x004521fb..0x00452225): arg1 = pState (%esi), arg2 = pObj (%edi), arg3 =
//  pOrigPos, arg4 = pNewPos, arg5 = bFlag (forwarded to bStandOn).  That is
//  exactly the prototype commonphysics.h already carried, derived
//  independently from wm_PushObject.
//
//  FRAME ARITHMETIC, `sub $0xbc,%esp` = 188 bytes, and it decomposes with
//  NOTHING left over:
//
//     F-0x68 .. F-0x04   104 = 0x68   WMMoveState state
//     F-0x80 .. F-0x78    12          three pointer slots (see below)
//     F-0xbc .. F-0x6c    72 = 6x12   six twelve-byte vector slots
//     -----------------------------
//                        188 = 0xbc
//
//  The 0x68 is the clincher: the four zero stores at 0x00451e2b..0x00451e3a
//  land at F-0x68, F-0x64, F-0x60 and F-0x04, and 0x68 - 0x04 = 0x64, so the
//  fourth is state.m_nUnknown64 -- the same four-field clear wm_MoveObject
//  and SPhysicsLT::SetObjectDims open with, at the same offsets, in a
//  DIFFERENT function.  servermgr.h's WMMoveState is confirmed twice over.
//  (The server's frame is also 0xbc, with `state` at the bottom instead of
//  the top: -0xbc..-0x54 with the m_nUnknown64 clear at -0x58.)
//
//  The seven twelve-byte slots are fewer than seven source locals: at /O2
//  VC6 coalesces dead ones, and the proof is in the bytes -- F-0xbc..F-0xb4
//  is `vMove` up to 0x00451f8f and `vMax` from 0x0045234c, and F-0xb0..F-0xa8
//  is `vDir` and then `vMin`.  So the frame pins the SIZE of every local and
//  the count of live ones, but not declaration order.  The three pointer
//  slots are the inlined wm_ObjectsIntersect's own pMin1/pMax1 (spilled
//  across wm_BoxesOverlap at 0x0045239a/0x0045239e) plus &pObj->m_Pos
//  (0x00451e6a / 0x0045226d, which the server parks in its dead first
//  parameter slot instead -- `mov %esi,0x8(%ebp)` at 0x1001f622).
//
//  CONSTANTS, all read out of .rdata rather than guessed:
//     0x004a14d8 = 0.0f   (Norm's `if (mag == 0.0f) return;`)
//     0x004a1a08 = 1.0f   (Norm's nVal default; `fdivrs` = nVal/mag)
//     0x004a1a74 = 0.5f
//     0x004a1e24 = 0.1f   and the same value as the immediate 0x3dcccccd
//                         pushed for wm_BoxesOverlap's tolerance
//     0x02000000 = FLAG_NOSLIDING (1<<25), via `not %eax; shr $0x19; and $1`
//     0x00004000 = FLAG_BOXPHYSICS (1<<14), in the inlined wm_ObjectsIntersect
//     0x1b0      = LTObject::m_pWMIface, passed RAW as wm_MoveThroughSim's
//                  second argument (0x00451fca, 0x0045221d) and dereferenced
//                  through slot 1 (GetTransformedBsp) inside the inlined
//                  wm_ObjectsIntersect (0x004523fb..0x00452405).  Both builds
//                  agree; the parameter's declared IWorldBsp* type in
//                  commonphysics.h is therefore loose, and the cast below is
//                  how the shipped call is written rather than a claim.
//
//  `vDir.Norm()` is not a guess either: the SERVER emits it as a CALL to
//  0x1000557c with `this` = a copy of vMove and ONE stack argument built by
//  `fld1; fstps (%esp)` -- i.e. `Norm(1.0f)`, the ltvector.h default -- and
//  the client's inline expansion is that function's body line for line:
//  Mag() as fsqrt, `fcoms 0.0f` + early exit, `fdivrs 1.0f`, three
//  multiplies.
//
//  PREDICTED CALLSEQ: 13 or 14 of 17.  Ours emits, in order:
//    wm_MoveThroughSim, _v18(ind), InitFrom(*), wm_MoveObject,
//    wm_BoxesOverlap, wm_ObjectsIntersect(*), _v1c(ind),
//    wm_MoveThroughSim, _v18(ind), Init, wm_MoveObject, wm_BoxesOverlap,
//    wm_ObjectsIntersect(*), _v1c(ind)
//  against the shipped
//    wm_MoveThroughSim, _v18(ind), wm_MoveObject, wm_BoxesOverlap,
//    GetTransformedBsp(ind), bsp_IntersectBox, _v1c(ind),
//    wm_MoveThroughSim, _v18(ind), Init, wm_MoveObject, wm_BoxesOverlap,
//    GetTransformedBsp(ind), bsp_IntersectBox, _v1c(ind),
//    operator-, operator+=
//  so the residue is: the two INLINED wm_ObjectsIntersect expansions (ours
//  calls 0x00451cb0, shipped has 0x00451690 + indirect + 0x00417840 in its
//  place -- 4 shipped-only, 2 ours-only), the InitFrom the shipped build
//  expanded (1 ours-only), and the second arm's out-of-line
//  _CVector::operator- / operator+= pair at 0x00452448 / 0x00452464 which our
//  SDK header inlines (2 shipped-only).  Every one of those is the
//  auto-inline residual described at the top of this file, in both
//  directions -- the shipped build expands three things we call AND calls two
//  things we expand, in the same function, which is the strongest evidence
//  in this cluster that the difference is an inlining SETTING and not source.
//  Witness: 17 calls over 1691 bytes = 99 B/call, comfortably in the strong
//  band; and the two 400-byte call-free stretches (0x00451e5b..0x00451fc9,
//  the vector arithmetic, and 0x004522ee..0x004523a2, the box bias) are
//  witnessed instead by the frame decomposition and the four .rdata
//  constants above.
//
//  UNDECODED: none.  Every instruction from 0x00451e10 to 0x004524aa is
//  accounted for; the esp trace balances to zero at all three `ret`s
//  (0x004521d3, 0x00452478, 0x0045249d, 0x004524aa) once the four callee-pop
//  sites are charged (0x00451ffa/0x0045224e pop 12 = _v18's three arguments,
//  0x00452180/0x00452423 pop 8 = _v1c's two, 0x004522c4 pops 16 = Init's
//  `ret $0x10`, 0x00452448 pops 16 = operator-'s buffer+vector, 0x00452464
//  pops 12 = operator+='s vector).
//
//  COIN-FLIPS, both stated so one build settles them:
//   1. `*pnOut = 0` before or after the four state clears.  The client emits
//      it FIRST (0x00451e29 before 0x00451e2b), the server LAST (0x1001f5f4
//      after 0x1001f5f1).  Written as below (clears first) our /O2 build
//      should follow the client; written the other way it should not.  No
//      call either way.
//   2. `vDest + vMove` versus `vMove + vDest`.  Both builds load vDest first
//      (`flds vDest.x; fadds vMove.x`) and NEITHER makes the by-value copy
//      the usual pattern predicts, so the operand order is not readable off the
//      copy; the load order is the only evidence and it says vDest is
//      `this`.  No call either way.
//
//  THE SECOND ARM'S MOVE-STATE FILL.  The client pushes four loose values
//  and calls 0x004524b0 (`ret $0x10`, CWMStateInit::Init); the server
//  pushes two and calls 0x1001efe8 (`ret $0x8`, InitFrom) at 0x1001f8ae.
//  The server's bytes are the ground truth here, so both arms name
//  InitFrom; the client's four pushes are exactly InitFrom's own reads.
//
//  SERVER 0x1001f5cd IS BYTE-IDENTICAL from the source below.  Three source
//  facts, each found by probe:
//   * the type test is an inline helper.  Written in place (either sense,
//     either arm first, or as a switch) VC6 lays the non-world-model arm
//     first with two `je`s to the other; through the helper it emits the
//     shipped `je then ; cmp $9 ; jne else` with the world-model arm on
//     the fall-through.
//   * nested positive guards with ONE `return 0` at the end (the
//     *pNewPos = vDest refusal as an `else`), which is what puts the
//     `xor %eax,%eax` next to the epilogue (0x1001f9a5).
//   * the locals are function scope and the two arms SHARE vTo: the
//     client-side mover's target is the same variable as the world-model
//     arm's sweep start.  Because vTo's address goes to wm_MoveThroughSim,
//     VC6 builds `m_Pos + (*pNewPos - vDest)` in a temporary and copies it
//     in (0x1001f8a5..0x1001f8ad), which is also what forces InitFrom's
//     pObj push to come from memory.  A separate vTarget is stored in
//     place and the function is eleven bytes short.  Block-scoped locals
//     leave the frame 0x18 short of the shipped 0xbc.
// ======================================================================= //
// Is this object one of the two world-model types? (name ours)
static inline LTBOOL wm_IsWorldModelType(LTObject* pObj)
{
	return pObj->m_ObjectType == OT_WORLDMODEL ||
	       pObj->m_ObjectType == OT_CONTAINER;
}

// lithtech.exe status (cl 8447 /O2): the 17 calls now agree in target and
// order with the shipped 0x00451e10 (the two wm_ObjectsIntersect and first
// InitFrom expanded, the second arm's operator- and operator+= called). What
// is left is allocation: 1688 bytes against 1691, a 0xb8 frame against 0xbc
// -- pNewPos and &pObj->m_Pos swap %ebx/%ebp from 0x00451e54 on, and the
// shipped build spills the second arm's &pObj->m_Pos to the slot the first
// arm uses (0x50, 0x0045226d) -- plus the second arm's filler call, noted
// there.
WM_CINLINE int wm_PushWithGeometry(WMMoveState* pState, LTObject* pObj, LTVector* pOrigPos,
                        LTVector* pNewPos, int bFlag, int* pnOut)
{
	WMMoveState state;
	LTVector    vDest;
	LTVector    vMove, vTo, vDir, vMin, vMax;
	int         bUsedGeometry;

	state.m_pContainer = LTNULL;
	state.m_pUnknown04 = LTNULL;
	state.m_pObj       = LTNULL;
	state.m_nUnknown64 = 0;

	*pnOut = 0;

	// The mover is the world model: sweep the object BACKWARDS, from where
	// the model's move would leave it (plus half a unit along the move, so
	// the sweep starts outside the model) to where it is now.
	if (wm_IsWorldModelType(pState->m_pObj))
	{
		vDest = pObj->m_Pos;
		vMove = *pNewPos - *pOrigPos;
		vTo   = vDest + vMove;

		vDir = vMove;
		vDir.Norm();

		vTo += vDir * 0.5f;

		if (wm_MoveThroughSim(pState, (IWorldBsp*)pState->m_pObj->m_pWMIface,
		                      pState->m_pObj, pObj, &vTo, &vDest,
		                      !(pState->m_pObj->m_Flags & FLAG_NOSLIDING),
		                      bFlag))
		{
			*pnOut = 1;

			if (((IWMSetupSource*)pState->m_pUnknown04)->_v18(pState,
			                                                  pState->m_pObj, pObj))
			{
				((CWMStateInit*)&state)->InitFrom(pState, pObj);

				wm_MoveObject(&state, vDest, 7);

				vMin = pObj->m_BBoxMin;
				vMax = pObj->m_BBoxMax;

				vMin.x += 0.1f;
				vMin.y += 0.1f;
				vMin.z += 0.1f;

				vMax.x -= 0.1f;
				vMax.y -= 0.1f;
				vMax.z -= 0.1f;

				if (!wm_ObjectsIntersect(pState->m_pObj, pObj,
				                         &pState->m_pObj->m_BBoxMin,
				                         &pState->m_pObj->m_BBoxMax,
				                         &vMin, &vMax, 0.1f, &bUsedGeometry))
					return 0;

				((IWMSetupSource*)pState->m_pUnknown04)->_v1c(pObj,
				                                              pState->m_pObj);
			}

			// However far short of vDest the object ended up, the model's
			// own target gives way by the same amount.
			*pNewPos += pObj->m_Pos - vDest;

			return 1;
		}
	}
	else
	{
		vDest = *pNewPos;

		if (wm_MoveThroughSim(pState, (IWorldBsp*)pObj->m_pWMIface, pObj,
		                      pState->m_pObj, pOrigPos, &vDest,
		                      !(pState->m_pObj->m_Flags & FLAG_NOSLIDING),
		                      bFlag))
		{
			*pnOut = 1;

			if (((IWMSetupSource*)pState->m_pUnknown04)->_v18(pState,
			                                                  pState->m_pObj,
			                                                  pObj))
			{
				// Move the world model out of the way by what the mover
				// could not travel.
				vTo = pObj->m_Pos + (*pNewPos - vDest);

				// lithtech.exe expands InitFrom here but, its inline budget
				// spent, CALLS the four-argument filler inside it (0x004522c4
				// -> 0x004524b0, CWMStateInit::Init, with InitFrom's four
				// loads). Ours does the same with the filler InitFrom is
				// written through here -- the static wm_InitMoveState, whose
				// COMDAT is then emitted -- so the shipped InitFrom was
				// written through Init. That cannot be reproduced without
				// making Init inline, and wm_ResizeAxis (0x00454250) CALLS
				// Init where ours would then expand it. Written as an
				// explicit Init call instead, the client's second arm stops
				// spending that budget and expands its final operator-, and
				// the copy expanded into wm_PushWorldModel calls Init where
				// the shipped copy calls InitFrom (0x00453589). server.dll
				// calls InitFrom (0x1001f8ae).
				((CWMStateInit*)&state)->InitFrom(pState, pObj);

				wm_MoveObject(&state, vTo, 7);

				vMin = pObj->m_BBoxMin;
				vMax = pObj->m_BBoxMax;

				vMin.x += 0.1f;
				vMin.y += 0.1f;
				vMin.z += 0.1f;

				vMax.x -= 0.1f;
				vMax.y -= 0.1f;
				vMax.z -= 0.1f;

				if (!wm_ObjectsIntersect(pState->m_pObj, pObj,
				                         &pState->m_pObj->m_BBoxMin,
				                         &pState->m_pObj->m_BBoxMax,
				                         &vMin, &vMax, 0.1f, &bUsedGeometry))
					return 0;

				((IWMSetupSource*)pState->m_pUnknown04)->_v1c(pObj,
				                                              pState->m_pObj);

				*pNewPos += pObj->m_Pos - vTo;
			}
			else
			{
				// Nothing else to try: hand back where the sim stopped.
				*pNewPos = vDest;
			}

			return 1;
		}
	}

	return 0;
}

// ======================================================================= //
//  0x004526f0  wm_PushWithBox                 1315 bytes, 10 calls
//
//  NEW DECODE.  Like wm_PushWithGeometry, commonphysics.cpp only declared
//  it.  Cross-checked against the server's 0x1001fb22 (/O1, %ebp frame),
//  which is instruction-for-instruction the same source.
//
//  WHAT IT IS: separate two boxes with no geometry involved.  It is two
//  symmetrical halves and it tries them in order:
//
//    1. 0x004527b3..0x00452a2c -- push the OTHER object (pObj) out of the
//       mover.  If that leaves the boxes clear it returns 0 and the mover's
//       target is untouched.
//    2. 0x00452a3b..0x00452c12 -- otherwise push the MOVER's own target
//       back out of pObj, and return 1 so the caller re-seats it.
//
//  The two halves differ ONLY in which box is argument 1 of
//  wm_SmallestSeparation, whose object is scaled and where the answer is
//  written; everything else -- the axis pick, the standing-on decision, the
//  sliding branch -- is the same code twice.  Half 1 is skipped entirely
//  when slot 0x18 refuses (`je 0x452a3b` at 0x004527ad).
//
//  SIGNATURE: four arguments, exactly as commonphysics.h already had from
//  wm_PushObject's call site.  Read off the frame: F+0x04 -> %edi is used as
//  `pState` (`0x4(%edi)` = m_pUnknown04, `0x8(%edi)` = m_pObj, `0xc(%edi)` =
//  m_nUnknown0C), F+0x08 -> %esi is the object (its 0x88/0xf0/0xfc/0x18c are
//  read), F+0x0c and F+0x10 are the two positions and only ever appear as
//  `*pOrigPos` / `*pNewPos`.  The two `ret`s hand back 0 (0x00452a84) and 1
//  (0x00452c03).
//
//  FRAME ARITHMETIC, `sub $0xac,%esp` = 172 bytes, again with no residue:
//
//     F-0x68 .. F-0x04   104 = 0x68   WMMoveState state
//     F-0xa0 .. F-0x6c    60 = 5x12   five twelve-byte vector slots
//     F-0x94               4          int  nSide     (from SmallestSeparation)
//     F-0x84               4          float fDist    (the collision magnitude)
//     -----------------------------
//                        172 = 0xac
//
//  and the same four-store WMMoveState clear as everywhere else in this
//  module: F-0x68/F-0x64/F-0x60 and F-0x04, i.e. +0x00/+0x04/+0x08 and
//  +0x64 of a 0x68-byte record (0x00452709..0x0045271b).  The server's own
//  frame is also 0xac and parks `nSide` in the DEAD FIRST PARAMETER SLOT
//  (`mov 0x8(%ebp),%eax` at 0x1001fbd5 reads it back while %ebx holds
//  pState) -- a known VC6 idiom, and independent confirmation
//  that nSide is one int and not two.
//
//  INSTRUCTION CENSUS, decomposed, because at 131 B/call the call check is
//  in the weak band and this is the load-bearing evidence.  412
//  instructions; every floating-point row is fully accounted for by the
//  structure below and NOTHING is left over:
//
//     fsqrt    2  = 2 x Mag()                     (one per half, sliding arm)
//     faddp    8  = 2 x 2 (Mag's two adds) + 2 x 2 (Dot's two adds)
//     fmuls    9  } 18 multiplies = 2 x 3 (Mag's squares)
//     fmul     9  }              + 2 x 3 (Dot's products)
//                 }              + 2 x 3 (vSlide * fT)
//     fdiv     4  = 2 x 2   vDir.x, vDir.y   (fdivrs 2 = vDir.z, kept in st)
//     fdivr    2  = 2 x 1   the fT ratio
//     fchs     8  = 2 (negating fDist) + 2 x 3 (the +mag / -mag pair and the
//                    final sign of the ratio)
//     fsubs    3  = the ONE subtraction in the function,
//                   vSlide = *pNewPos - *pOrigPos
//     fadds   11  = 3 (vSep + pObj->m_Pos) + 3 (vNew += pObj->m_Pos)
//                 + 2 (*pNewPos += vSep, x and y)
//                 + 2 (*pNewPos += *pOrigPos, x and y)
//                 + 1 (their SHARED z, cross-jumped: `jmp 0x452bdc` at
//                      0x00452b0f lands on `fadds 0x8(%ebp)`)
//     fcomps   5  = 3 (half 1: m_Pos.y against m_Pos.y, then pNewPos.y
//                      against pOrigPos.y in each arm)
//                 + 2 (half 2: VC6 hoisted the second compare above the
//                      branch, so one fcomps serves both arms -- which is
//                      also why fnstsw is 5 and not 6)
//     sar      4  = 2 halves x 2 arms, the `nSide >> 1` axis pick
//
//  A third half, a missing Mag, a fourth normalise, one more standing-on
//  test or a componentwise rather than whole-vector `*pNewPos += vSep`
//  would move at least three of those rows.
//
//  CONSTANTS: 0x004a1a08 = 1.0f (half 1's `1.0f -`), 0x3dcccccd = 0.1f
//  (wm_BoxesOverlap's tolerance, as an immediate), 0x02000000 =
//  FLAG_NOSLIDING.  No other literal is loaded.
//
//  PREDICTED CALLSEQ: 10 of 10 in agreement, with 1 ours-only.  The shipped
//  order is _v18(ind), wm_SmallestSeparation, obj_SetStandingOn,
//  wm_MoveObject, ws_ObjectsCollided, wm_BoxesOverlap, _v1c(ind),
//  wm_SmallestSeparation, obj_SetStandingOn, ws_ObjectsCollided; ours adds
//  CWMStateInit::InitFrom between obj_SetStandingOn and wm_MoveObject,
//  because the shipped build expanded it (six stores at 0x00452999..
//  0x004529be, no call) -- the same auto-inline residual as everywhere
//  else.  The server, which does NOT expand it, calls 0x1001efe8 there.
//  TWO RISKS worth stating so the build is read correctly:
//    * the shipped build CROSS-JUMPS the two obj_SetStandingOn arms in BOTH
//      halves (`jmp 0x45285c` at 0x00452845 and `jmp 0x452adc` at
//      0x00452ad1), so four source call sites become two calls.  The server
//      cross-jumped only half 1 and emits THREE.  If our build declines to
//      cross-jump, expect 2 more ours-only rows and the same 10 agreements.
//    * Mag() and Dot() are expanded here (fsqrt, and nine multiplies) where
//      the server CALLS them (0x10005502 twice).  If our /O2 build calls
//      them, that is 2 more ours-only.
//
//  UNDECODED: none.  Every instruction from 0x004526f0 to 0x00452c12 is
//  accounted for and the esp trace balances at both `ret`s once
//  0x004527a8 is charged 12 bytes (_v18's three arguments) and 0x00452a2a 8
//  (_v1c's two).
//
//  THE ASYMMETRY IN fDist, recorded rather than tidied: the axis pick reads
//  a face coordinate off a DIFFERENT object in the two halves' odd arms.
//  Half 1 (0x004527eb): `-pState->m_pObj->m_BBoxMin[axis]`.
//  Half 2 (0x00452a74): `-pObj->m_BBoxMin[axis]`.
//  Both even arms read `pObj->m_BBoxMax[axis]`.  So half 2 takes both
//  values off pObj while half 1 takes the odd one off the mover.  This is
//  NOT a decode slip: server.dll has exactly the same pair of base
//  registers at 0x1001fbe7 (`0xf0(%edx,...)`, %edx = pState->m_pObj) and
//  0x1001fdf3 (`0xf0(%esi,...)`, %esi = pObj), against `0xfc(%esi,...)` in
//  both even arms.  Two independent builds of the same source agreeing on
//  the asymmetry makes it the original's, and it looks like a copy-paste
//  slip in the second half -- but it is what shipped and it is what is
//  written below.
//
//  COIN-FLIPS, no call either way:
//   1. `vDir = vSep / fMag` versus three component divides.  The divides
//      write straight into vDir's slots with no operator temporary, so the
//      copy the usual pattern predicts is elided and the two are indistinguishable
//      here.
//   2. half 1's `fT`.  The bytes compute `1.0f - (fDot - fMag) / fDot`
//      through a -fMag that is reconstructed into -fDot (0x00452903..
//      0x0045290d), which algebraically is fMag/fDot.  Written directly as
//      `fMag / fDot` there would be one fdiv and no `fsubrs 1.0f`, and the
//      `fsubrs 0x4a1a08` in the shipped bytes says the `1.0f -` is real.
//      Half 2's is `(fDot + fMag) / fDot` and has NO 1.0f load, which is
//      the cross-check: the two halves really do differ by one, so this is
//      not a spelling artefact.
//   3. the sliding branch's sense.  `testl $0x2000000,...; jne <sliding>`
//      puts the simple arm on the fall-through, so it is written below as
//      `if (!(flags & FLAG_NOSLIDING)) { simple } else { sliding }`.
//      Written the other way round VC6 should swap the two blocks.
// ======================================================================= //
WM_CINLINE int wm_PushWithBox(WMMoveState* pState, LTObject* pObj, LTVector* pOrigPos,
                   LTVector* pNewPos)
{
	WMMoveState state;
	LTVector    vSep;
	LTVector    vSlide;
	int         nSide;
	float       fDist;

	state.m_pContainer = LTNULL;
	state.m_pUnknown04 = LTNULL;
	state.m_pObj       = LTNULL;
	state.m_nUnknown64 = 0;

	// Only the sliding paths below need it, and they are the only paths
	// either NOSLIDING flag can reach -- so the whole vector is computed
	// under one `||` guard (`jne` then `je`, 0x00452731/0x0045273c).
	if ((pObj->m_Flags & FLAG_NOSLIDING) ||
	    (pState->m_pObj->m_Flags & FLAG_NOSLIDING))
	{
		vSlide = *pNewPos - *pOrigPos;
	}

	// ---- half 1: push the other object out of the mover ---------------- //
	if (((IWMSetupSource*)pState->m_pUnknown04)->_v18(pState, pState->m_pObj,
	                                                  pObj))
	{
		LTVector vNew;

		wm_SmallestSeparation(&pObj->m_BBoxMin, &pObj->m_BBoxMax,
		                      &pState->m_pObj->m_BBoxMin,
		                      &pState->m_pObj->m_BBoxMax, &vSep, &nSide);

		// The winning side is 2*axis for a +axis push and 2*axis+1 for a
		// -axis one, so the low bit picks the face and the shift the axis.
		// `sar $1` with no round-to-zero fixup: the source wrote `>> 1`.
		if (nSide & 1)
			fDist = -(&pState->m_pObj->m_BBoxMin.x)[(nSide - 1) >> 1];
		else
			fDist = (&pObj->m_BBoxMax.x)[nSide >> 1];

		// Sides 2 and 3 are the Y axis, and a vertical separation is what
		// decides what is standing on what: whichever of the two is above
		// the other ends up on top, but only if the move was going that way.
		if (nSide == 2 || nSide == 3)
		{
			if (pState->m_pObj->m_Pos.y > pObj->m_Pos.y)
			{
				if (pNewPos->y < pOrigPos->y)
					obj_SetStandingOn(pState->m_pObj, pObj, LTNULL);
			}
			else
			{
				if (pNewPos->y > pOrigPos->y)
					obj_SetStandingOn(pObj, pState->m_pObj, LTNULL);
			}
		}

		if (!(pObj->m_Flags & FLAG_NOSLIDING))
		{
			// m_Pos on the LEFT, same by-value operand rule as above. The
			// function is byte-identical on server.dll with this order; the
			// old x87 residual was the fX spelling in the sliding arms.
			vNew = pObj->m_Pos + vSep;
		}
		else
		{
			// It may not slide, so it has to go along the mover's own move
			// far enough that the projection onto the separation direction
			// covers the whole separation.
			// fMag IS THE NEGATIVE MAGNITUDE, and both images say so in
			// the same three instructions. After the Mag call the shipped
			// code is `fchs ; fld %st(0) ; fchs` (S:0x1001fc87, and
			// lithtech 0x004528cd after its inlined fsqrt): it negates the
			// magnitude, keeps that copy live to the end of the arm, and
			// negates a duplicate BACK to divide by. That is exactly
			// `fMag = -Mag()` followed by `vSep / -fMag`, and nothing else
			// produces a double fchs around an fld. The two arms then read
			// `fDot + fMag` and `fDot - fMag`, which is what S:0x1001fcc2
			// `fadd %st(2),%st` and S:0x1001fedc `fsubr %st(2),%st` are --
			// the sign of the operator follows fMag's sign, so the pair is
			// self-checking.
			//
			// The extra live value is also what makes VC6 spill vDir: with
			// both +mag and -mag on the x87 stack it stores dir.x and dir.y
			// to memory and keeps only dir.z (`fstps ; fstps ; fstp %st(1)`)
			// and the dot product then reads them back with `fmuls`. Our
			// build kept all three in registers before this change. Worth 8
			// instructions and 24 bytes on server.dll, and it takes the two
			// division-and-dot blocks to byte-exact on both arms.
			float    fMag = -vSep.Mag();
			LTVector vDir = vSep / -fMag;
			float    fDot = vSlide.Dot(vDir);
			// fX, and the divisor rebuilt from it, as in half 2 below
			// (S:0x1001fcc2..0x1001fcce).
			float    fX   = fDot + fMag;
			float    fT   = 1.0f - -fX / (fMag - fX);

			vNew = vSlide * fT;
			vNew += pObj->m_Pos;
		}

		((CWMStateInit*)&state)->InitFrom(pState, pObj);

		wm_MoveObject(&state, vNew, 7);

		ws_ObjectsCollided((IPhysicsSim*)pState->m_pUnknown04, pObj,
		                   pState->m_pObj, &g_AxisDirs[nSide], fDist);

		// Still overlapping: report it and fall through to half 2.
		// Clear: nothing to tell the caller, and its target stands.
		// if/else, not an early `if (!...) return 0;`: shipped lays the
		// `return 0` block out right after the _v1c call with a jump over
		// it and an explicit `xor %eax,%eax` (S:0x1001fdac..0x1001fdb0).
		// The early return instead jumped straight to the epilogue reusing
		// the test's zero %eax, three bytes short.
		if (wm_BoxesOverlap(&pState->m_pObj->m_BBoxMin,
		                    &pState->m_pObj->m_BBoxMax,
		                    &pObj->m_BBoxMin, &pObj->m_BBoxMax, 0.1f))
		{
			((IWMSetupSource*)pState->m_pUnknown04)->_v1c(pObj,
			                                              pState->m_pObj);
		}
		else
		{
			return 0;
		}
	}

	// ---- half 2: the same again, with the mover as the one that gives -- //
	wm_SmallestSeparation(&pState->m_pObj->m_BBoxMin,
	                      &pState->m_pObj->m_BBoxMax,
	                      &pObj->m_BBoxMin, &pObj->m_BBoxMax, &vSep, &nSide);

	if (nSide & 1)
		fDist = -(&pObj->m_BBoxMin.x)[(nSide - 1) >> 1];
	else
		fDist = (&pObj->m_BBoxMax.x)[nSide >> 1];

	if (nSide == 2 || nSide == 3)
	{
		if (pState->m_pObj->m_Pos.y > pObj->m_Pos.y)
		{
			if (pNewPos->y < pOrigPos->y)
				obj_SetStandingOn(pState->m_pObj, pObj, LTNULL);
		}
		else
		{
			if (pNewPos->y > pOrigPos->y)
				obj_SetStandingOn(pObj, pState->m_pObj, LTNULL);
		}
	}

	if (!(pState->m_pObj->m_Flags & FLAG_NOSLIDING))
	{
		*pNewPos += vSep;
	}
	else
	{
		// Negative magnitude again; see the note on the first arm.
		//
		// THE NUMERATOR IS A NAMED VALUE AND THE DENOMINATOR IS REBUILT
		// FROM IT. S:0x1001fedc `fsubr %st(2),%st` overwrites the dot
		// product with fMag - fDot, and the divisor is then made out of
		// fMag and that value, not out of fDot: `fld %st(1) ; fsub
		// %st(1),%st ; fdivr %st(1),%st ; fchs` is -(fX / (fMag - fX)).
		// `(fDot - fMag) / fDot`, the same number, keeps fDot alive with an
		// `fld %st(0)` and divides straight, two instructions short.
		// `fX / (fX - fMag)` is not the same tree either (fld %st(0) ;
		// fsub %st(2) and no fchs). Half 1 is the same shape plus the
		// `1.0f -`.
		//
		// And the ratio goes straight into the multiply with no fT local:
		// with one, the first component is `flds ; fmul %st(1)` where
		// S:0x1001feec has `fld %st(0) ; fmuls -0x34(%ebp)`.
		float    fMag = -vSep.Mag();
		LTVector vDir = vSep / -fMag;
		float    fDot = vSlide.Dot(vDir);
		float    fX   = fMag - fDot;

		*pNewPos = vSlide * (-fX / (fMag - fX));
		*pNewPos += *pOrigPos;
	}

	ws_ObjectsCollided((IPhysicsSim*)pState->m_pUnknown04, pState->m_pObj,
	                   pObj, &g_AxisDirs[nSide], fDist);

	return 1;
}

// ======================================================================= //
//  EXTENT VERIFICATION FOR THE WHOLE CLUSTER, and the index
// ======================================================================= //
//
//  Three mechanical checks over 0x00451000..0x00452c13, all passing:
//
//   1. NO BRANCH CROSSES A FUNCTION BOUNDARY.  Every `j*` target in the
//      range resolves to an instruction boundary inside its OWN function,
//      taking the eleven starts below as the partition.  A row that spanned
//      two functions would show a branch from one half into the other's
//      interior, or a target landing mid-instruction; there are zero of
//      each.  This is the check that replaces the `push %ebp` prologue
//      count, which is useless on this target -- /O2 implies /Oy and all
//      eleven functions are frameless, so a prologue count reports "0 prologues"
//      for every row.
//
//   2. THE CALL COUNTS SUM.  The call sequence over the eleven functions separately
//      gives 86 calls; the call sequence over 0x00451000..0x00452c13 as one range
//      gives 86.  Nothing is double-counted and nothing falls in a gap.
//
//   3. THE BYTES SUM.  7,124, unchanged by the 0x004524e0/0x00452506
//      correction, because the two rows were adjacent -- 38+488 becomes one
//      526.
//
//  The multiple `ret`s the extent check flags on six of the rows (2, 2, 2, 3, 4
//  and 2) are all duplicated /O2 epilogues, and for four of the six there is
//  direct proof: a SINGLE reconstructed function compiled from the source in
//  this file already produces an in-order call match over the whole stated
//  extent (wm_MoveObject 23 of 29 across all 1,676 bytes, wm_MoveThroughSim
//  6 of 7 across all 526, wm_ObjectsIntersect 3 of 3, wm_NotifyContact 5 of
//  5).  One function cannot match a sequence spread over two.
//
//    client addr  size  function                        calls
//    -----------  ----  ------------------------------  ----------------------
//    0x00451000   1676  wm_MoveObject                   23/29 measured
//    0x00451690    142  wm_BoxesOverlap                 vacuous; BYTE-EXACT
//    0x00451720   1306  wm_PushIntersectingObjects       5/13 measured
//    0x00451c40     52  wm_IsPushBlocker                 1/1  measured
//    0x00451c80     34  obj_MaybeRetransformWorldModel   1/1  measured, BYTE-EXACT
//    0x00451cb0    184  wm_ObjectsIntersect              3/3  measured, BYTE-EXACT
//    0x00451d70    160  wm_NotifyContact                 5/5  measured
//    0x00451e10   1691  wm_PushWithGeometry             13-14/17 predicted (NEW)
//    0x004524b0     38  CWMStateInit::Init              vacuous; BYTE-EXACT
//    0x004524e0    526  wm_MoveThroughSim                6/7  measured
//    (0x00452506)   --  NOT A FUNCTION: inside the above
//    0x004526f0   1315  wm_PushWithBox                  10/10 predicted (NEW)
//
//  Beyond the twelve, and defined here because the cluster's bodies were
//  expanded out of them: wm_SweepBox and wm_PushObject (both folded into
//  0x00451720) and CWMStateInit::InitFrom (0x00453ef0).
// ======================================================================= //


// ======================================================================= //


// ----------------------------------------------------------------------- //
//  0x00452cd0 / 0x10020045 -- two world models that are both solid and that
//  overlap, pushed apart along one axis at a time.
//
//  wm_PushObject hands the pair here when both ends are FLAG_SOLID and the
//  mover carries FLAG2_PLAYERCOLLIDE (0x00451a0a..0x00451a1f).  The answer is
//  the usual one: nonzero means the other object was moved.
//
//  THE SHAPE, read off the two builds:
//
//  Three nested loops of decreasing scope.
//
//   1. The retry loop (client head 0x00452d54, back edge `jl` at 0x00453ec2;
//      server head 0x10020090, back edge 0x1002046f).  A do/while whose
//      condition is four terms in this order -- bBothSolid, bMoved, !nResult,
//      nTries++ < 10 -- read off 0x00453e9a/0x00453ea2/0x00453eaa/0x00453eae
//      and 0x10020457/0x1002045c/0x10020461/0x10020466.  The 10 is
//      `cmp $0xa` against the value BEFORE the increment on both builds
//      (client `mov %eax,%ecx ; inc %eax ; cmp $0xa,%ecx`), so it is a post-
//      increment inside the condition.
//
//   2. The axis loop, `nAxis < 3 && !bMoved`.  The two tests are in that
//      order: the back edge at 0x00453150 / 0x10020382 re-tests `nAxis < 3`
//      and jumps to 0x00452d80 / 0x100200b7, which is the `bMoved` test.
//      Its taken arm (0x00453168 / 0x1002038d) lands one instruction PAST the
//      `if (bMoved)` that follows the loop -- a layout rule in its
//      purest form, and what says the loop's own second term and that `if`
//      are the same predicate written twice.
//
//   3. Inside one axis, two near-identical halves (client 0x00452d8c and
//      0x00452eaf; server 0x100200c0 and 0x10020244), the first driven by
//      fT1 and the second by fT2, with the direction test reversed between
//      them.  The second half plus the undo block are wrapped in `if
//      (!bMoved)`: the server emits that test at 0x10020235, the client folds
//      it (0x00452fb3 `je 0x00452eaf` falls straight into the second half,
//      0x00452fbb `jmp 0x00453136` straight to the loop tail).
//
//  THE TWO CROSSING TIMES.  Per axis the move is parameterised 0..1 and the
//  object's two faces give two parameters.  Both are clamped to [0,1] the
//  same way -- `if (f < 0) f = 0; else if (f > 1) f = 1;` -- and the `else if`
//  is what makes the second test RELOAD the value (client 0x00452df3
//  `flds 0x1c(%esp)` after the store at 0x00452de9).  They are then sorted
//  into fT1 <= fT2 with a flag saying which face won:
//
//    client 0x00452e3f  flds 0x1c(%esp) ; fcomp %st(1) ; test $0x1,%ah
//    server 0x10020143  flds -0x8(%ebp) ; fcomp %st(1) ; sahf ; jae
//
//  When the axis has no move at all the whole computation is skipped
//  (`jbe` at 0x00452daa..0x00452db8 / 0x100200d4..0x100200df) and control
//  lands ON the else arm of that sort, with 0.0f already on the FPU and 0.0f
//  in the temp -- both builds constant-fold `0.0f < 0.0f` rather than test
//  it, which is why the skip target is 0x00452e62 / 0x1002015f and not the
//  comparison above it.
//
//  THE DIRECTION TEST.  `fcomps` is evaluated ONCE and the status word is
//  then read with two different masks either side of the bMinFirst branch
//  (client 0x00452e97..0x00452f00: one `fnstsw`, then `test $0x41,%ah` on
//  one arm and `test $0x1,%ah` on the other; server 0x10020183..0x1002019b:
//  one `fnstsw`, two `sahf`).  That is one comparison in the source with two
//  senses taken off it -- written here as a conditional expression.  The
//  first half wants the sign to AGREE with bMinFirst, the second half to
//  disagree.
//
//  `!bBothSolid ||` short-circuits ahead of it: 0x00452e8f / 0x00452ecc test
//  bBothSolid and jump straight into the push, without touching the FPU.
//
//  WHAT ONE PUSH DOES.  Move the mover to the interpolated point, retransform
//  it if it is a world model, and ask whether the boxes still overlap with a
//  hundredth of tolerance (0x3c23d70a at 0x00452f91 / 0x1004d224).  If they
//  do the axis is done and bMoved says so.  If neither half separated them
//  but at least one was tried, the mover is put back at *pOrigPos + vMove
//  (client 0x0045309b, server 0x10020324) -- that undo is the only reader of
//  bAny inside the loop, and it clears bAny first, before the SetPos.
//
//  AFTER THE LOOP the pair is handed to whichever pusher fits, and the arms
//  are nested `if (bBothSolid) { if (bNeedsGeom) geom else box } else sim`:
//  0x00453176 `je 0x00453c4d` is the bBothSolid else, 0x0045317e
//  `je 0x00453735` the bNeedsGeom else, and the geometry arm is the fall-
//  through.  Same three at 0x1002038d / 0x10020396 / 0x1002039f.
//
//  The not-both-solid arm runs the move through the simulation on behalf of
//  whichever end is a world model that did NOT ask for box physics -- the
//  same `type == 2 || type == 9` plus `!(flags & FLAG_BOXPHYSICS)` pair
//  wm_PairNeedsGeometry uses, tried on pObj first (0x00453c62 / 0x100204b2)
//  and on the mover second (0x00453caf / 0x100204e3), with the two calls
//  sharing one tail (`jmp 0x00453ce2` / `jmp 0x1002050f`) because only the
//  first three pushed arguments differ.  Whatever comes back, the arm's
//  result is 0 -- both `bRet = 0` stores land on the same block
//  (0x00453e25 / 0x10020554).
//
//  UNDECODED: none.  Every instruction from 0x00452cd0 to 0x00453ed4 is
//  accounted for, either by a statement here or by one of the two inlined
//  bodies already in this file.
//
//  UNPINNED, and weighed:
//
//    * the four zeroing stores at the head of the retry loop.  The server
//      order is nResult, bMoved, bAny, nAxis (0x100200a5..0x100200ae); the
//      client order is bAny, nResult, bMoved, nAxis (0x00452d6c..0x00452d78,
//      with bAny's REGISTER copy `xor %ebx,%ebx` hoisted to 0x00452d57 so
//      %ebx can serve as the zero source for the other three).  bAny's home
//      slot 0x28(%esp) is the same slot nAxis uses inside the loop -- %ebx
//      is spilled back into it at 0x0045315a/0x00453168 once nAxis is dead --
//      which is why that slot is written twice.  The server's order is taken
//      here on the grounds that /O1 does not reschedule stores; if the client
//      comes back one instruction out of place at 0x00452d6c, moving
//      `bAny = 0;` to the front of the four is the fix.
//
//    * `0.0f < fT1` / `0.0f < fT2`, not `fT1 > 0.0f`.  Reversed deliberately:
//      the shipped guards load the CONSTANT first (`flds 0x004a14d8 ;
//      fcomps 0x5c(%esp)` at 0x00452e76, `fldz ; fcomps -0x18(%ebp)` at
//      0x1002016b) while every clamp two lines above loads the VALUE first
//      (0x00452ddc, 0x00452e13), which is what says VC6 is being literal
//      about operand order here and the source really is the constant on the
//      left.  If the client comes back with those two compares swapped, the
//      natural `fT1 > 0.0f` is the alternative.
//
//    * `*pGeom->m_pOrigPos + pGeom->m_vMove` appears twice and the client
//      inlines operator+ at the first site (0x004530af) but CALLS 0x004316d0
//      at the second (0x00453e5e).  The server inlines both (0x10020333,
//      0x10020413).  Same expression, same operand roles -- `this` is
//      m_pOrigPos at all four sites, the by-value argument is m_vMove -- so
//      this is the inliner's per-function budget running out, not a source
//      difference.  Written identically at both.
//
//  lithtech.exe EXPANDS wm_PairNeedsGeometry, wm_PushWithGeometry and
//  wm_PushWithBox here (all WM_CINLINE): 0x00452cd0 is 4613 bytes and calls
//  neither 0x00451e10 nor 0x004526f0. Inside the two expanded pushers every
//  nested inline is a CALL (InitFrom 0x00453ef0, wm_ObjectsIntersect,
//  operator-/+/+=, Norm, Mag, Dot, the WMMoveState constructor 0x00453ee0),
//  where their own out-of-line copies expand them -- the budget is spent.
//  NEAR: ours is 4534 bytes, same frame (0x288) and prologue; 57 of the calls
//  agree. Where ours differs it expands a nested helper the shipped build
//  calls (in the geometry pusher's first arm, operator-/operator+ and Dot)
//  or calls one the shipped build expands (wm_NotifyContact, 0x00453d86..,
//  and the final obj_MaybeRetransformWorldModel) -- same compiler, a
//  different spend order.
// ----------------------------------------------------------------------- //
int wm_PushWorldModel(WMMoveState* pState, LTObject* pObj)
{
	CWMMoveGeom* pGeom = (CWMMoveGeom*)pState;
	int          bBothSolid;
	int          bNeedsGeom;
	int          nTries;
	int          nAxis;
	int          bMoved;
	int          bAny;
	int          nResult;
	// ONE function-scope vector, not two CVectorInit temporaries. Both
	// images build three floats into a single 12-byte local in x,y,z order
	// and copy it into the argument, with NO constructor call at all
	// (server 0x100201a4..0x100201e4, client 0x00452f02..0x00452f69).
	// CVectorInit is out of line (commonphysics.h), so a call to it is
	// visible in our object and absent from both shipped images.
	LTVector     vNew;

	// Both ends solid, tested pObj first: the mover is the second read on
	// both builds (0x00452cef then 0x00452d01; 0x1002005a then 0x10020067).
	if ((pObj->m_Flags & FLAG_SOLID) && (pState->m_pObj->m_Flags & FLAG_SOLID))
		bBothSolid = 1;
	else
		bBothSolid = 0;

	nTries = 0;

	bNeedsGeom = wm_PairNeedsGeometry(pState->m_pObj, pObj);

	do
	{
		// ORDER MEASURED, do not re-try the client's. The client
		// (0x00452d6c) writes bAny first and hoists `xor %ebx,%ebx` as the
		// zero source; the server writes nResult, bMoved, bAny. Taking the
		// client's order made the server diff WORSE, 15 blocks -> 17, and
		// moved lithtech not at all. The hoist is allocation, not order.
		nResult = 0;
		bMoved  = 0;
		bAny    = 0;

		for (nAxis = 0; nAxis < 3 && !bMoved; nAxis++)
		{
			float fMinT;
			float fMaxT;
			float fT1;
			float fT2;
			int   bMinFirst;

			// fMaxT FIRST.  Both images materialise two zeroes and store
			// one, keeping the other in st(0); the shipped /O1 writes
			// `fldz ; fldz ; fstps -0x8(%ebp)` (0x100200c0) and fMinT is
			// the one that goes to memory.  Written fMinT-first we get
			// `fldz ; fstps ; fldz` instead -- same bytes, wrong order,
			// and the last mnemonic block in the function.
			fMaxT = 0.0f;
			fMinT = 0.0f;

			// Not `!= 0.0f`: two separate compares against the same 0.0f,
			// `jb` then `jbe` (0x100200d2 / 0x100200df), and the client's
			// `test $0x1,%ah` then `test $0x41,%ah`.
			if ((&pGeom->m_vMove.x)[nAxis] < 0.0f ||
			    (&pGeom->m_vMove.x)[nAxis] > 0.0f)
			{
				// THE SPELLING OF THE SUBTRAHEND AND THE DIVISOR IS WHAT
				// FOLDS THEM INTO THE ARITHMETIC, and it is measured.
				// VEC_INDEX is `(((float*)&(v))[i])`, so
				// VEC_INDEX(*pGeom->m_pOrigPos, nAxis) expands to
				// `((float*)&(*p))[i]` -- an ADDR-of-IND pair -- and
				// VEC_INDEX(pGeom->m_vMove, nAxis) to `(float*)&` of a
				// class object.  With either of those as the right operand
				// /Os will not fold the memory operand: it emits
				// `flds (%edx) ; fsubp` and `flds (%ecx) ; fdivp` where the
				// shipped build has `fsubs (%edx)` and `fdivs (%ecx)`
				// (0x100200f2 / 0x100200f4 and 0x1002011f / 0x10020121),
				// four bytes lost at each of the two sites.  Dropping the
				// `&*` on the first and naming the FLOAT member on the
				// second folds both.  /O2 folds either spelling, which is
				// why this only shows in server.dll.
				fMinT = (VEC_INDEX(pObj->m_BBoxMin, nAxis) -
				         ((float*)pGeom->m_pOrigPos)[nAxis]) /
				        (&pGeom->m_vMove.x)[nAxis];

				if (fMinT < 0.0f)
					fMinT = 0.0f;
				else if (fMinT > 1.0f)
					fMinT = 1.0f;

				fMaxT = (VEC_INDEX(pObj->m_BBoxMax, nAxis) -
				         ((float*)pGeom->m_pOrigPos)[nAxis]) /
				        (&pGeom->m_vMove.x)[nAxis];

				if (fMaxT < 0.0f)
					fMaxT = 0.0f;
				else if (fMaxT > 1.0f)
					fMaxT = 1.0f;
			}

			// bMinFirst FIRST in each arm.  That keeps fMinT live across
			// the store, so the two cannot share a frame slot: the shipped
			// build loads fMinT into %eax, then `movl $0x1,-0x28(%ebp)`,
			// then stores fT1 (0x1002014f..0x10020159), with bMinFirst in a
			// slot of its own and a 0x94 frame.  Written last, VC6 packs
			// bMinFirst into fMinT's slot and the frame is 0x90.
			if (fMinT < fMaxT)
			{
				bMinFirst = 1;
				fT1       = fMinT;
				fT2       = fMaxT;
			}
			else
			{
				bMinFirst = 0;
				fT1       = fMaxT;
				fT2       = fMinT;
			}

			if ((0.0f < fT1 || bNeedsGeom) &&
			    (!bBothSolid ||
			     (bMinFirst ? (&pGeom->m_vMove.x)[nAxis] > 0.0f
			                : (&pGeom->m_vMove.x)[nAxis] < 0.0f)))
			{
				bAny = 1;

				vNew.x = (pGeom->m_vPos.x - pGeom->m_pOrigPos->x) * fT1 +
					pGeom->m_pOrigPos->x;
				vNew.y = (pGeom->m_vPos.y - pGeom->m_pOrigPos->y) * fT1 +
					pGeom->m_pOrigPos->y;
				vNew.z = (pGeom->m_vPos.z - pGeom->m_pOrigPos->z) * fT1 +
					pGeom->m_pOrigPos->z;

				pState->m_pObj->SetPos(vNew);

				WM_RETRANSFORM(pState->m_pObj, 1);

				if (wm_BoxesOverlap(&pState->m_pObj->m_BBoxMin,
				                    &pState->m_pObj->m_BBoxMax,
				                    &pObj->m_BBoxMin, &pObj->m_BBoxMax,
				                    0.01f))
				{
					bMoved = 1;
				}
			}

			if (!bMoved)
			{
				if ((0.0f < fT2 || bNeedsGeom) &&
				    (!bBothSolid ||
				     (bMinFirst ? (&pGeom->m_vMove.x)[nAxis] < 0.0f
				                : (&pGeom->m_vMove.x)[nAxis] > 0.0f)))
				{
					bAny = 1;

					vNew.x = (pGeom->m_vPos.x - pGeom->m_pOrigPos->x) * fT2 +
						pGeom->m_pOrigPos->x;
					vNew.y = (pGeom->m_vPos.y - pGeom->m_pOrigPos->y) * fT2 +
						pGeom->m_pOrigPos->y;
					vNew.z = (pGeom->m_vPos.z - pGeom->m_pOrigPos->z) * fT2 +
						pGeom->m_pOrigPos->z;

					pState->m_pObj->SetPos(vNew);

					WM_RETRANSFORM(pState->m_pObj, 1);

					if (wm_BoxesOverlap(&pState->m_pObj->m_BBoxMin,
					                    &pState->m_pObj->m_BBoxMax,
					                    &pObj->m_BBoxMin, &pObj->m_BBoxMax,
					                    0.01f))
					{
						bMoved = 1;
					}
				}

				// Tried and got nowhere: put the mover back where the move
				// wanted it. bAny is cleared FIRST on both builds
				// (0x004530a1, 0x10020324).
				if (bAny && !bMoved)
				{
					bAny = 0;

					pState->m_pObj->SetPos(*pGeom->m_pOrigPos +
					                       pGeom->m_vMove);

					WM_RETRANSFORM(pState->m_pObj, 1);
				}
			}
		}

		if (bMoved)
		{
			if (bBothSolid)
			{
				if (bNeedsGeom)
				{
					pState->m_pObj->SetPos(pGeom->m_vPos);
					WM_RETRANSFORM(pState->m_pObj, 1);

					// Sixth argument is &bMoved, not a scratch: the retry
					// condition below reads bMoved AFTER this writes through
					// it (0x00453ea2 / 0x1002045c).
					nResult = wm_PushWithGeometry(pState, pObj,
					                              pGeom->m_pOrigPos,
					                              &pGeom->m_vPos, 1, &bMoved);

					if (nResult)
					{
						pState->m_pObj->SetPos(pGeom->m_vPos);
						WM_RETRANSFORM(pState->m_pObj, 1);
					}
				}
				else
				{
					nResult = wm_PushWithBox(pState, pObj, pGeom->m_pOrigPos,
					                         &pState->m_pObj->m_Pos);
				}
			}
			else
			{
				// One `nResult = 0;` for both sub-arms: every path through
				// here lands on the same store (0x00453e25 / 0x10020554),
				// which is also where the geometry arm's `return 0` paths
				// are tail-merged.
				if (bNeedsGeom)
				{
					LTVector vTarget;
					int      bHit;

					vTarget = pGeom->m_vPos;

					// bHit IS PRE-SET AND THERE IS NO `else` ARM, and that
					// is measured.  Written `else { bHit = 1; }` VC6 keeps
					// the merge honest -- `push $0x1 ; pop %eax` on the
					// else path and a `test %eax,%eax` both paths reach --
					// and it also hosts the shared wm_MoveThroughSim call
					// in the FIRST arm instead of the second.  The shipped
					// build threads the else straight past the test
					// (0x100204f1 and 0x100204fa both `jne 0x1002051c`,
					// which is the body of `if (bHit)`) and hosts the call
					// in the SECOND arm (0x100204de `jmp 0x1002050f`), which
					// is the keep-the-second-copy rule.  Pre-setting bHit here produces
					// exactly that: the function goes from 1324 bytes and
					// three mnemonic blocks to 1319 and one.
					bHit = 1;

					// The two calls differ only in which end is the world
					// model; their last five arguments are identical, which
					// is what lets the shipped build share the tail
					// (`jmp 0x00453ce2` / `jmp 0x1002050f`).
					if ((pObj->m_ObjectType == OT_WORLDMODEL ||
					     pObj->m_ObjectType == OT_CONTAINER) &&
					    !(pObj->m_Flags & FLAG_BOXPHYSICS))
					{
						bHit = wm_MoveThroughSim(pState,
						                         (IWorldBsp*)pObj->m_pWMIface,
						                         pObj, pState->m_pObj,
						                         pGeom->m_pOrigPos, &vTarget,
						                         0, 1);
					}
					else if ((pState->m_pObj->m_ObjectType == OT_WORLDMODEL ||
					          pState->m_pObj->m_ObjectType == OT_CONTAINER) &&
					         !(pState->m_pObj->m_Flags & FLAG_BOXPHYSICS))
					{
						bHit = wm_MoveThroughSim(pState,
						                         (IWorldBsp*)pState->m_pObj->m_pWMIface,
						                         pState->m_pObj, pObj,
						                         pGeom->m_pOrigPos, &vTarget,
						                         0, 1);
					}

					if (bHit)
						pGeom->m_vMove = vTarget - *pGeom->m_pOrigPos;
				}
				else
				{
					wm_NotifyContact((IWMSetupSource*)pState->m_pUnknown04,
					                 pObj, pState->m_pObj);
				}

				nResult = 0;
			}
		}

		if (bAny && !nResult)
		{
			pState->m_pObj->SetPos(*pGeom->m_pOrigPos + pGeom->m_vMove);
			WM_RETRANSFORM(pState->m_pObj, 1);
		}
	}
	while (bBothSolid && bMoved && !nResult && nTries++ < 10);

	return nResult;
}


// *********************************************************************** //
//  TWO SMALL ROWS FROM THE lithtech.exe UNEXAMINED SWEEP
// *********************************************************************** //

// 0x004451a0, 11 bytes — reset the mover's counter.
//
//      c7 05 64 40 4b 00 00 00 00 00   movl $0x0,0x004b4064
//      c3                              ret
//
// EXTENT CORRECTION: the enumeration gives this row 22 bytes. It is 11 —
// 0x004451a0..0x004451aa, then 0x004451ab..0x004451af is nop padding, and
// 0x004451b0 is a SEPARATE function src/engine/lithtech/cmodelload.cpp:282
// already names ("`mov $0x46,%eax; ret`, i.e. LT_NOCHANGE and nothing
// else"). The 22 swallows it.
//
// The global is NOT this file's: its other readers (0x0047c7af, 0x0047c7b7,
// 0x0047c8ab, 0x0047c8b3, 0x0047cb2a, 0x0047cb34, 0x0047cd0f, 0x0047cd16,
// 0x0047d001, 0x0047d009) are the sound-data code, and it is
// src/engine/lithtech/sndinst2.cpp's running total of sample memory,
// referenced here under that file's name.  This reset has one caller,
// 0x0040f8fc, in a client-side init sequence.
//
// lithtech.exe only: server.dll has no such reset (no `c7 05 <addr> 00 00 00
// 00 c3` anywhere in its .text), so the server build leaves it out rather
// than reference a global the DLL does not have.
#ifndef LT_SERVER_BUILD
extern uint32 g_nSoundMemoryQ47C;            // 0x004b4064

void phys_ResetMoveCounter()
{
	g_nSoundMemoryQ47C = 0;
}
#endif


// 0x00453ee0, 16 bytes — the constructor of wm_PushWithBox's stack local.
// It is in no map; the
// neighbours place it, wm_PushWithBox at 0x004526f0 and wm_ResizeAxis at
// 0x00454250 bracketing it.
//
//      8b c1         mov %ecx,%eax
//      33 c9         xor %ecx,%ecx
//      89 08         mov %ecx,(%eax)
//      89 48 04      mov %ecx,0x4(%eax)
//      89 48 08      mov %ecx,0x8(%eax)
//      89 48 64      mov %ecx,0x64(%eax)
//      c3            ret
//
// __thiscall, no arguments, `this` back in %eax. One call site, 0x00453750,
// inside wm_PushWithBox, with `lea 0x230(%esp),%ecx` — a stack local at
// least 0x68 wide of which only four dwords are initialised: +0x00, +0x04,
// +0x08 and +0x64. The three consecutive dwords and the isolated one 0x5c
// further on are two different members rather than one array; the zero is
// materialised ONCE into %ecx and reused, which is what VC6 does for a
// constructor whose initialisers are all literal zero.
//
// Written before its only caller because an out-of-line member definition
// is emitted whether or not the TU calls it.
class CPushState453EE0
{
public:
	CPushState453EE0();                      // 0x00453ee0

	void*  m_pUnknown00;                     // 0x00
	void*  m_pUnknown04;                     // 0x04
	uint32 m_nUnknown08;                     // 0x08
	uint8  m_pad_0c[0x64 - 0x0c];
	uint32 m_nUnknown64;                     // 0x64
};

CPushState453EE0::CPushState453EE0()
{
	m_pUnknown00 = LTNULL;
	m_pUnknown04 = LTNULL;
	m_nUnknown08 = 0;
	m_nUnknown64 = 0;
}
