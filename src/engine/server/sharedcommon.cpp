// server.dll — the shared ILTCommon implementation (reconstruction). Build /O1.
#include <string.h>
#include "sharedcommon.h"
#include "servermath.h"
#include "servermgr.h"
#include "servermsg.h"
#include "ilttransform.h"
#include "iltmodel.h"
#include "de_error.h"

// The name an argument error reports: server.dll's copy of these functions
// says "ILTCommon::...", lithtech.exe's "CommonLT::..." (the retail images'
// strings: the two were built from different revisions of this file).
#ifdef LT_SERVER_BUILD
#define COMMON_ERROR_NAME(fn)	"ILTCommon::" fn
#else
#define COMMON_ERROR_NAME(fn)	"CommonLT::" fn
#endif

// 0x004347a0 (lithtech.exe) / 0x1001214c (server.dll) -- the constructor,
// the first function of the file: the vptr, then m_pMath cleared.  Called
// out of line by CLTServer's constructor on its +0x234 member (commonphysics
// .cpp).  lithtech.exe only for now: server.dll links svnew.cpp's
// CSharedCommonView stand-in under this name (src/engine/server/link/
// aliases.rsp), which this would duplicate.  Too small to be credited in
// either image.
#ifndef LT_SERVER_BUILD
SharedCommonLT::SharedCommonLT()
{
	m_pMath = LTNULL;
}
#endif

LTRESULT SharedCommonLT::GetObjectFlags(const HOBJECT hObj, const ObjFlagType flagType, uint32& dwFlags)
{
	dwFlags = 0;

	if (!hObj)
		RETURN_ERROR_NAMED(2, "SharedCommonLT::GetObjectFlags", LT_INVALIDPARAMS);

	if (flagType == OFT_Flags)
		dwFlags = hObj->m_Flags;
	else
		dwFlags = hObj->m_Flags2;

	return LT_OK;
}

LTRESULT SharedCommonLT::SetObjectFlags(HOBJECT hObj, const ObjFlagType flagType, uint32 dwFlags)
{
	if (!hObj)
		RETURN_ERROR_NAMED(2, "SharedCommonLT::SetObjectFlags", LT_INVALIDPARAMS);

	if (flagType == OFT_Flags)
		hObj->m_Flags = dwFlags;
	else
		hObj->m_Flags2 = dwFlags;

	return LT_OK;
}

LTRESULT SharedCommonLT::GetObjectType(HOBJECT hObj, uint32* type)
{
	if (!hObj)
		RETURN_ERROR(2, COMMON_ERROR_NAME("GetObjectType"), LT_INVALIDPARAMS);

	*type = hObj->m_ObjectType;
	return LT_OK;
}

LTRESULT SharedCommonLT::GetModelAnimUserDims(HOBJECT hObject, LTVector* pDims, HMODELANIM hAnim)
{
	if (!hObject || hObject->m_ObjectType != OT_MODEL)
		RETURN_ERROR(1, "CommonLT::GetModelAnimUserDims", LT_INVALIDPARAMS);

	Model* pModel = ((ModelInstance*)hObject)->m_AnimTracker.m_TimeRef.m_pModel;
	if (hAnim >= pModel->m_AnimArray.m_nElements)
		RETURN_ERROR_INFO(1, "CommonLT::GetModelAnimUserDims", LT_INVALIDPARAMS, "invalid hAnim");

	*pDims = pModel->m_AnimArray.m_pArray[hAnim].m_vDims;
	return LT_OK;
}

LTRESULT SharedCommonLT::GetAttachments(HLOCALOBJ hObj, HLOCALOBJ* inList, uint32 inListSize,
                                        uint32& outListSize, uint32& outNumAttachments)
{
	static char* pFnName = "SharedCommonLT::GetAttachments";
	HOBJECT hParent, hChild;

	outNumAttachments = 0;
	outListSize = 0;

	if (!hObj || !inList)
		RETURN_ERROR(1, pFnName, LT_INVALIDPARAMS);

	// hParent and hChild have their addresses taken, and VC6 gives them the
	// home slots of outListSize and outNumAttachments — dead, because both
	// live in registers for the whole loop.
	for (Attachment* pAttach = hObj->m_pAttachments; pAttach; pAttach = pAttach->m_pNext)
	{
		if (GetAttachmentObjects((HATTACHMENT)pAttach, hParent, hChild) == LT_OK)
		{
			if (outListSize < inListSize)
			{
				inList[outListSize] = hChild;
				outListSize++;
			}

			outNumAttachments++;
		}
	}

	return LT_OK;
}

LTRESULT SharedCommonLT::GetAttachmentTransform(HATTACHMENT hAttachment, LTransform &transform,
                                                LTBOOL bWorldSpace)
{
	LTransform tParent;
	LTMatrix mAttach;
	LTMatrix mParent;
	LTMatrix mFinal;
	HOBJECT hParent;
	HOBJECT hChild;
	LTRESULT dResult;
	Attachment* pAttachment = (Attachment*)hAttachment;

	// Both interfaces cached up front: the shipped prologue loads them
	// before the parameter check.
	ILTTransform* pTransformLT = m_pTransformLT;
	ILTModel* pModelLT = m_pModelLT;

	if (!hAttachment)
		RETURN_ERROR_NAMED_NUM(1, "SharedCommonLT::GetAttachmentTransform", LT_INVALIDPARAMS);

	dResult = GetAttachmentObjects(hAttachment, hParent, hChild);
	if (dResult != LT_OK)
		return dResult;

	// A model parent with a real node attaches to the node's transform;
	// everything else uses the object transform (or identity in local space).
	if (hParent->m_ObjectType == OT_MODEL && pAttachment->m_iNode != -1)
	{
		dResult = ((ILTModelI*)pModelLT)->GetNodeTransform(hParent, pAttachment->m_iNode, tParent, bWorldSpace);
		if (dResult != LT_OK)
			return dResult;
	}
	else if (bWorldSpace)
	{
		tParent.m_Pos = hParent->m_Pos;
		tParent.m_Rot = hParent->m_Rotation;
	}
	else
	{
		tParent.m_Pos.Init();
		tParent.m_Rot.Init();
	}

	pTransformLT->ToMatrix(tParent, mParent);

	quat_ConvertToMatrix(&pAttachment->m_Offset.m_Rot, &mAttach);
	mAttach.SetTranslation(pAttachment->m_Offset.m_Pos);

	mat_Multiply(&mFinal, &mParent, &mAttach);
	pTransformLT->FromMatrix(transform, mFinal);
	return LT_OK;
}


// ---- three interface forwards the client shares (0x10012d28-0x10012d4d) - //
//
// Both tables point at these rather than at the functions behind them, so
// each is one line. The counter pair hides the fact that the engine's own
// counter is eight bytes where the SDK's LTCounter is sixteen.

void ci_StartCounter(LTCounter* pCounter)
{
	cnt_StartCounter((CBaseCounter*)pCounter);
}

uint32 ci_EndCounter(LTCounter* pCounter)
{
	return cnt_EndCounter((CBaseCounter*)pCounter);
}

// 0x00490cd0 / 0x1003ddc0 (byte-identical between the images) -- NOT stricmp.
// It is stdlith's CHelpers::UpperStrcmp (src/lithshared/stdlith/helpers.cpp),
// a case-insensitive EQUALITY predicate folding case through a 256-byte table
// and returning 1 when the strings match, 0 when they do not. clientinput.cpp
// and ltnew2.cpp both already identify it that way.
//
// The previous spelling here was `return stricmp(...)`, which is wrong twice
// over: stricmp is a different function (0x00492f30, also called from this
// image) and it is a three-way compare whose zero means EQUAL -- so the truth
// value came out inverted as well. The byte verifier can never catch this,
// because it masks call relocations and both spellings are one `call`; only
// a check of the call targets can see it.
//
// Declared as the real class member (helpers.h's static, mangled
// ?UpperStrcmp@CHelpers@@SAHPBD0@Z) so the call's relocation names the
// stdlith symbol itself -- a free-function stand-in has no definition to
// link against. A byte diff of src/lithshared/stdlith/Release/helpers.obj's
// UpperStrcmp against 0x1003ddc0 and 0x00490cd0: byte-identical, 64 bytes.
// Declared here rather than by including helpers.h, as serverengine.cpp does
// for CHelpers::ExtractNames.
class CHelpers
{
public:
	static int UpperStrcmp(const char* pStr1, const char* pStr2);   // 0x1003ddc0
};

LTBOOL ci_UpperStrcmp(char* pStr1, char* pStr2)
{
	return CHelpers::UpperStrcmp(pStr1, pStr2);
}


// A node transform and a socket transform are one function with a flag: a
// node is one and a socket zero. `this` rides through in ecx, so neither
// forwarder touches it.
LTRESULT SharedCommonLT::GetAttachedModelNodeTransform(HATTACHMENT hAttachment,
                                                       HMODELNODE hNode,
                                                       LTransform& transform)
{
	return GetAttachedTransform(hAttachment, hNode, transform, LTTRUE);
}

LTRESULT SharedCommonLT::GetAttachedModelSocketTransform(HATTACHMENT hAttachment,
                                                         HMODELSOCKET hSocket,
                                                         LTransform& transform)
{
	return GetAttachedTransform(hAttachment, hSocket, transform, LTFALSE);
}

// Running out of commands is not an error, so the parser's "no more" answers
// LT_FINISHED rather than a failure code.
LTRESULT SharedCommonLT::Parse(ConParse* pParse)
{
	return ((CommandParser*)pParse)->Next() ? LT_OK : LT_FINISHED;
}

// The two rotation helpers are the interface's, but the arithmetic is not:
// both hand straight on to the math implementation. Note the argument swap —
// ILTCommon asks for up before right and the math takes right before up.
LTRESULT SharedCommonLT::GetRotationVectors(LTRotation& rot, LTVector& up,
                                            LTVector& right, LTVector& forward)
{
	return m_pMath->GetRotationVectors(&rot, &right, &up, &forward);
}

LTRESULT SharedCommonLT::SetupEuler(LTRotation& rot, float pitch, float yaw,
                                    float roll)
{
	return m_pMath->SetupEuler(&rot, pitch, yaw, roll);
}


// 0x10012176 — the only writer of m_pMath. CLTServer's constructor
// (0x1002d3xx) reaches this class at its +0x234 and hands over the address of
// the CCommonMath embedded in itself at +4, which is the one place the shared
// half learns where the math lives. Name ours; ten bytes, one store.
void SharedCommonLT::SetMath(ICommonMath* pMath)
{
	m_pMath = pMath;
}


// 0x100124ce — one function behind both attachment-piece accessors.
//
// Three interface calls in a row, each aborting on failure, then the compose.
// Two things are worth reading off the bytes rather than the headers:
//
//   * the flag picks ILTModel slots 0x18 and 0x1c, and those two functions
//     name themselves "ILTModel::GetNodeTransform" (0x1001d478) and
//     "ILTModel::GetSocketTransform" (0x1001d77f) in their own parameter
//     checks. Slot numbering here is iltmodel.h's — ILTModel has no virtual
//     destructor, so GetSocket is slot 0. (commonmodel.h's ILTModelI carries
//     an extra leading slot, which shifts every transform accessor by one;
//     that is why the neighbouring GetAttachmentTransform, which reaches
//     0x1c through it, is calling GetSocketTransform under the name
//     GetNodeTransform.)
//
//   * Multiply's return value is discarded. The three earlier results are
//     each tested and returned; the last is not, and the function ends
//     `call *0x1c(%eax); xor eax,eax`, so the source returns LT_OK.
//
// The attachment transform is asked for in WORLD space and the piece
// transform in LOCAL space, which is what makes the product meaningful.
LTRESULT SharedCommonLT::GetAttachedTransform(HATTACHMENT hAttachment, uint32 hPiece,
                                              LTransform& transform, LTBOOL bNode)
{
	LTransform tAttach;
	LTransform tPiece;
	HOBJECT    hParent;
	HOBJECT    hChild;
	LTRESULT   dResult;

	// Both interfaces are cached before the first call, model first: the
	// shipped prologue reads +0x08 into edi and +0x04 into ebx and never
	// re-addresses `this` after the second call (it becomes the zero the
	// three result compares use).
	ILTModel*     pModelLT     = m_pModelLT;
	ILTTransform* pTransformLT = m_pTransformLT;

	dResult = GetAttachmentObjects(hAttachment, hParent, hChild);
	if (dResult != LT_OK)
		return dResult;

	dResult = GetAttachmentTransform(hAttachment, tAttach, LTTRUE);
	if (dResult != LT_OK)
		return dResult;

	if (bNode)
		dResult = pModelLT->GetNodeTransform(hChild, hPiece, tPiece, LTFALSE);
	else
		dResult = pModelLT->GetSocketTransform(hChild, hPiece, tPiece, LTFALSE);

	if (dResult != LT_OK)
		return dResult;

	pTransformLT->Multiply(transform, tAttach, tPiece);
	return LT_OK;
}


// 0x10015bcd — quaternion to matrix. This one needed no reconstruction: the
// SDK ships the source in src/LT2/sdk/inc/ltquatbase.cpp and the shipped bytes are
// that function, statement for statement, including the six-deep chained
// assignment of the last row and column (the zero stores come out in reverse
// order, 0x38 down to 0x0c, which is that chain evaluating right to left).
// Its real home is ltquatbase.cpp; it sits here because that TU is not part of
// the reconstruction yet.
//
// The declaration this file uses takes the quaternion and the matrix as their
// LT types rather than the SDK's `const float*` / `float[4][4]`, because the
// callers in this TU pass those; the bodies are the same arithmetic on the
// same offsets.
void quat_ConvertToMatrix(LTRotation* pQuat, LTMatrix* pMat)
{
	float s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

	// get the values for matrix calcuation...
	s = 2.0f / ((pQuat->m_Quat[0] * pQuat->m_Quat[0]) + (pQuat->m_Quat[1] * pQuat->m_Quat[1]) +
		(pQuat->m_Quat[2] * pQuat->m_Quat[2]) + (pQuat->m_Quat[3] * pQuat->m_Quat[3]));

	xs = pQuat->m_Quat[0] * s;
	ys = pQuat->m_Quat[1] * s;
	zs = pQuat->m_Quat[2] * s;

	wx = pQuat->m_Quat[3] * xs;
	wy = pQuat->m_Quat[3] * ys;
	wz = pQuat->m_Quat[3] * zs;

	xx = pQuat->m_Quat[0] * xs;
	xy = pQuat->m_Quat[0] * ys;
	xz = pQuat->m_Quat[0] * zs;

	yy = pQuat->m_Quat[1] * ys;
	yz = pQuat->m_Quat[1] * zs;

	zz = pQuat->m_Quat[2] * zs;

	// Fill in matrix
	pMat->m[0][0] = 1.0f - (yy + zz);
	pMat->m[0][1] = xy - wz;
	pMat->m[0][2] = xz + wy;

	pMat->m[1][0] = xy + wz;
	pMat->m[1][1] = 1.0f - (xx + zz);
	pMat->m[1][2] = yz - wx;

	pMat->m[2][0] = xz - wy;
	pMat->m[2][1] = yz + wx;
	pMat->m[2][2] = 1.0f - (xx + yy);

	pMat->m[0][3] = pMat->m[1][3] = pMat->m[2][3] =
		pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f;
	pMat->m[3][3] = 1.0f;
}


// ---- the console's two server-side notifications ------------------------- //

// ServerMessage::WriteString (0x10026137): the string and its terminator
// through WriteData, or one zero byte when the pointer is null. servermsg.h
// does not carry it and is not this reconstruction's to change, so the
// message is cast to reach it (name ours).
class CMsgStringWriter
{
public:
	void WriteString(char* pStr);            // 0x10026137
	void WriteData(void* pData, uint32 nBytes);   // 0x1002616a
};

// A null pointer still puts a byte on the wire — one zero — so the reader's
// side is symmetric either way. The terminator is included in the length
// (`inc %eax` on strlen's result), and the one-byte local's address is what
// gives this function its ebp frame; `andb $0x0` is /O1's zero store.
//
// Both arms reach one shared `call`: the shipped code cross-jumps the two
// WriteData calls after their arguments are pushed, which is what the
// `jmp` at 0x10026152 into 0x1002615e is.
void CMsgStringWriter::WriteString(char* pStr)
{
	char cNull;

	if (pStr)
	{
		WriteData(pStr, strlen(pStr) + 1);
	}
	else
	{
		cNull = 0;
		WriteData(&cNull, 1);
	}
}

// 0x10021c55 — the net manager's queued send, reached through a view of the
// embedded manager because servermgr.h's CNetMgr does not model it. Same
// class, name and signature smgr.cpp uses, so the two translation units
// reference one symbol.
class CNetMgrSendView
{
public:
	int SendToConnection(class ServerMessage* pMsg, class NetConnection* pConn,
	                     uint32 flags);      // 0x10021c55
};

// 0x10029609 — put the packet id in the first payload byte and hand the
// message to every client that still has a connection. The id is stored as a
// byte and the walk is the manager's client list at +0x7e8.
//
// This is sm_BroadcastToClients (0x100297b8, smgr.cpp) with the send flags
// handed in and *without* the m_State == 3 test: every connected client gets
// it, in the world or not. smgr.cpp already declares it under this name.
void sm_BroadcastToClientsEx(CServerMgr* pMgr, int nPacketID,
                             ServerMessage* pMsg, uint32 flags)
{
	LTLink* pHead;
	LTLink* pCur;

	pMsg->m_pData[0] = (uint8)nPacketID;

	pHead = &pMgr->m_ClientList.m_Head;
	pCur = pHead->m_pNext;

	while (pCur != pHead)
	{
		Client* pClient = (Client*)pCur->m_pData;

		if (pClient->m_pConnection)
		{
			((CNetMgrSendView*)&pMgr->m_NetMgr)->SendToConnection(
				pMsg, pClient->m_pConnection, flags);
		}

		pCur = pCur->m_pNext;
	}
}

// 0x1003485a — restart every object's deactivation countdown from its stored
// time, re-file the ones that were waiting on it, and wake their cells (name
// ours). Only the AutoDeactivate variable reaches it.
//
// The body is si_SetDeactivationTime (0x10031583) run over every live object
// in the 0x8dc list, with two differences the bytes make plain: the countdown
// is reloaded from the object's own stored time rather than from a parameter
// (so the 0x14 -> 0x18 copy is an integer move, not an FPU round trip), and
// the container bump is gated on the object's +0x60 list being empty instead
// of on m_Unknown58 == -1. ltobject_internal.h still carries +0x60 as
// padding and what that list holds is not established, so it is reached by
// offset here rather than named in the header.
//
// The list is walked with the node advanced before the body, which is what
// lets the object pointer and the cursor share edi/esi.
class CObjLink60View
{
public:
	uint8  m_pad_00[0x60];
	LTLink m_Link60;                         // +0x60
};

void sm_ResetDeactivationTimers(CServerMgr* pMgr)
{
	LTLink* pHead;
	LTLink* pCur;

	pHead = &pMgr->m_List8dc.m_Head;
	pCur = pHead->m_pNext;

	while (pCur != pHead)
	{
		LTObject* pObj = (LTObject*)pCur->m_pData;
		SObjData* pSData;

		pCur = pCur->m_pNext;

		pSData = pObj->m_pSData;

		if (pSData)
		{
			uint32 nChangeFlags;

			pSData->m_UpdateTimer = pSData->m_DeactivationTime;

			nChangeFlags = pObj->m_ChangeFlags;

			if (nChangeFlags & 0x20)
				sm_SetObjectState(pMgr, pObj, nChangeFlags & 0x18);

			if (g_bAutoDeactivate)
			{
				CObjLink60View* pView = (CObjLink60View*)pObj;

				if (pView->m_Link60.m_pNext == &pView->m_Link60)
					pMgr->m_Unknown210.BumpObject(pObj, 0);
			}
		}
	}
}

// 0x10028e97 — a console variable was created: tell the clients, and if it
// was AutoDeactivate, act on it here as well.
//
// Three things read off the bytes:
//
//   * the packet is allocated and then *released* immediately — the scoped
//     reference keeps the only count. That is the same pair of statements
//     ServerCommonLT::SetObjectFilenames opens with.
//   * `pIO` lives in the dead first parameter's stack slot: the console
//     state is cached in edi before the allocation, so `lea 0x8(%ebp)` is
//     the local's address and not an argument's.
//   * the deactivation sweep is reached through the second global manager
//     pointer, not through the state's own — `push 0x10058bf0` is a memory
//     push, so what it hands over is g_pServerMgr's value.
void cc_OnVarCreated(ConsoleState* pState, LTCommandVar* pVar)
{
	CMessageRef msg;

	if (pState->m_pServerMgr)
	{
		CAbstractIO* pIO;

		msg.Init(msg_AllocPacket(&pIO, 0x1000, 0x1000));
		if (pIO)
			pIO->Release();

		if (stricmp(pVar->m_pName, "AutoDeactivate") == 0)
			sm_ResetDeactivationTimers(g_pServerMgr);

		{
			ServerMessage* pMsg = msg.m_pMsg;

			pMsg->m_Unknown38 = 1;
			pMsg->m_Unknown34 = 1;
		}

		((CMsgStringWriter*)msg.m_pMsg)->WriteString(pVar->m_pName);
		((CMsgStringWriter*)msg.m_pMsg)->WriteString(pVar->m_pStringVal);

		sm_BroadcastToClientsEx(pState->m_pServerMgr, 0x10, msg.m_pMsg, 0x80);
	}
}

// 0x10028f42 / 0x0046ef0 — a console variable changed: exactly the same
// notification, forwarded. (The two share one enumerated range in
// the server.dll function enumeration, which is why the entry reads 187 bytes and
// the created half is 171.)
//
// THE LITHTECH ADDRESS IS 0x00466ef0, and writing it down here is the point:
// this row spent the whole project on the unexamined list because the banner
// carried only the server address. It also corrects the decoded map, which
// names 0x00466e30 `cc_OnVarCreated` on `bridged-funcptr` evidence -- the
// pointer that is actually STORED is 0x00466ef0, at 0x00466f67
// (`movl $0x00466ef0,0x34(%esi)`, bytes c7 46 34 f0 6e 46 00), and
// 0x00466e30's only inbound `e8` in the image is from inside this forwarder.
// So 0x00466ef0 is the installed hook and 0x00466e30 is its out-of-line body.
void cc_OnVarChanged(ConsoleState* pState, LTCommandVar* pVar)
{
	cc_OnVarCreated(pState, pVar);
}


// 0x100587a8 — where a console command's failure is left for the caller to
// pick up. Nothing passes it; the runner clears it, the command handler
// writes it, and the wrapper below reads it back (name ours).
extern int g_nConsoleResult;

// 0x10028fc6 — run one console string and answer what it did. The result
// does not come back through con_RunString, which is void: it comes out of a
// global the handler wrote, which is why this wrapper exists at all and why
// it has to clear the global first. The clear lands between the two argument
// pushes in the shipped code, which is scheduling rather than source order.
int cc_RunConsoleString(ConsoleState* pState, char* pString)
{
	g_nConsoleResult = 0;

	con_RunString(pState, pString);

	return g_nConsoleResult;
}


// 0x1001236b — iltcommon.h's CreateMessage, and a stub. Slot 0x1004d738,
// immediately after SetupEuler: the out-parameter is nulled and the answer is
// LT_ERROR (`push $1; pop %eax`), so this half of the engine hands out no
// message objects at all.
LTRESULT SharedCommonLT::CreateMessage(ILTMessage*& pMsg)
{
	pMsg = LTNULL;

	return LT_ERROR;
}


// 0x00466f80, 12 bytes — the console teardown forwarder.
//
//      8b 44 24 04   mov 0x4(%esp),%eax
//      50            push %eax
//      e8 ..         call 0x0041b200
//      59            pop %ecx
//      c3            ret
//
// cdecl, one argument, forwarded unchanged. The callee 0x0041b200 is
// con_Term — clientconsole.cpp:975 and cmgrinit.cpp:88 both name it. The one
// caller, 0x00473696, passes `lea 0xa74(%esi),%edx`, the ConsoleState
// embedded in the manager at +0xa74. `pop %ecx` rather than `add $4,%esp` is
// the one-argument cdecl cleanup /O2 always picks.
void con_Term(ConsoleState* pState);         // 0x0041b200

void cc_TermConsole(ConsoleState* pState)
{
	con_Term(pState);
}
