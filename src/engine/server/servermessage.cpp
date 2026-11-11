// server.dll — the ILTServer message starters (reconstruction). Build /O1.
#include <string.h>
#include <stdio.h>
#include "iltserver.h"
#include "servermsg.h"
#include "commonphysics.h"
#include "sharedcommon.h"
#include "commonmodel.h"
#include "servermodel.h"
#include "servermgr.h"
#include "objbank.h"
#include "de_error.h"
#include "packet.h"

// FormatString's callees (names ours): format a message-table string from
// the class module, wrap the result in a string handle, release the raw.
void*   str_FormatMessage(void* hModule, int messageCode, va_list* pMarker,
                          uint32* pLen);   // 0x10035caa
void    str_FreeMessage(void* pRaw);       // 0x10035d39

// 0x10034fb2 — one dereference, cdecl, seven bytes: an accessor VC6 left out
// of line. WriteFormattedString hands it CClassMgr::m_pUnknown08 and passes
// the result to str_FormatMessage, which dereferences again -- so the 0x814
// field points at a record whose first word is the module handle, one
// indirection further out than the m_hModule at 0x810 that FormatString uses
// (name ours).
// 0x10034fb2 / 0x00477f30, seven bytes and no relocation, byte-identical
// between the two images. Declared here since this file was written and never
// defined, which is the third of the three ways a function hides from an
// address scan: the address lives beside a declaration, so a bare-name grep
// finds this file and an address grep finds nothing. Three callers in
// lithtech.exe -- 0x0040edf4, 0x0041560a, 0x0047be4a.
//
//      8b 44 24 04   mov 0x4(%esp),%eax
//      8b 00         mov (%eax),%eax
//      c3            ret
void* clm_GetRecordModule(void* pRecord)
{
	return *(void**)pRecord;
}

// The engine's string-handle helpers (all cdecl, defined elsewhere).
HSTRING sb_CopyString(HSTRING hString);                        // 0x10035e3e
HSTRING sb_CreateString(char* pString);                        // 0x10035dca
void    sb_FreeString(HSTRING hString);                        // 0x1001372f
LTBOOL  sb_CompareStrings(HSTRING h1, HSTRING h2);             // 0x10035e76
LTBOOL  sb_CompareStringsUpper(HSTRING h1, HSTRING h2);        // 0x10035e9e
char*   sb_GetStringData(HSTRING hString);                     // 0x10035ec6
// (the identifier's name is its hash key: see dhash_GetKey, 0x1000ddfb)

// Where GetObjectName looks: the object's engine-side data at +0x188 carries
// an identifier handle at +0xc; objects without one share this "" in .data
// (0x10056ff4, directly after g_DebugLevel).
extern char g_szNoName[];

// CMessageRef::Init now lives in servermsg.h, `inline`, where the original
// had it -- the local copy this file used to carry was a workaround for its
// absence and would now be a duplicate definition.

// ---- class reflection (the TU's opening block, 0x1002e074-0x1002e34b) ---- //
//
// HCLASS is a CClassInfo*, whose slot table at +0x24 starts with the SDK
// ClassDef. Both property methods misname their error site
// "CLTServer::GetClassDef" — one pooled literal, two static name pointers.

LTRESULT CLTServer::GetNumClassProps(const HCLASS hClass, uint32 &count)
{
	ClassDef* pDef;

	count = 0;

	if (!hClass)
		RETURN_ERROR_NAMED(2, "CLTServer::GetClassDef", LT_INVALIDPARAMS);

	// Inherited properties count: the walk climbs the parent chain.
	count = 0;
	for (pDef = ((CClassInfo*)hClass)->m_pClassDef; pDef; pDef = pDef->m_ParentClass)
		count += pDef->m_nProps;

	return LT_OK;
}

LTRESULT CLTServer::GetClassProp(const HCLASS hClass, const uint32 iProp, ClassPropInfo &info)
{
	ClassDef* pDefs[256];
	ClassDef* pDef;
	ClassDef** ppDef;
	uint32 nDefs, iBase, i;

	info.m_PropName[0] = 0;

	if (!hClass)
		RETURN_ERROR_NAMED(2, "CLTServer::GetClassDef", LT_INVALIDPARAMS);

	pDef = ((CClassInfo*)hClass)->m_pClassDef;
	nDefs = 0;
	if (pDef)
	{
		ppDef = pDefs;
		do
		{
			*ppDef = pDef;
			pDef = pDef->m_ParentClass;
			nDefs++;
			ppDef++;
		} while (pDef);
	}

	// Index from the root ancestor down, so a derived class's own properties
	// come after everything it inherited.
	iBase = 0;
	if (nDefs > 0)
	{
		ppDef = &pDefs[nDefs - 1];
		for (i = nDefs; i > 0; i--)
		{
			pDef = *ppDef;
			if (iProp >= iBase && iProp < iBase + pDef->m_nProps)
			{
				PropDef* pProp = &pDef->m_Props[iProp - iBase];
				strncpy(info.m_PropName, pProp->m_PropName, MAX_CLASSPROPINFONAME_LEN - 1);
				info.m_PropName[MAX_CLASSPROPINFONAME_LEN - 1] = 0;
				info.m_PropType = pProp->m_PropType;
			}

			iBase += pDef->m_nProps;
			ppDef--;
		}
	}

	return LT_OK;
}

LTRESULT CLTServer::GetClassName(const HCLASS hClass, char *pName, uint32 maxNameBytes)
{
	if (!hClass)
		RETURN_ERROR_NAMED(2, "CLTServer::GetClassName", LT_INVALIDPARAMS);

	if (maxNameBytes)
	{
		strncpy(pName, ((CClassInfo*)hClass)->m_pClassDef->m_ClassName, maxNameBytes - 1);
		pName[maxNameBytes - 1] = 0;
	}

	return LT_OK;
}

LTRESULT CLTServer::GetClientPing(HCLIENT hClient, float &ping)
{
	if (!hClient)
		RETURN_ERROR(2, "ILTPhysics::GetClientPing", LT_INVALIDPARAMS);

	// Success body first: the epilogue belongs to the lexically-last return,
	// and the shipped bytes put the not-connected error there.
	NetConnection* pConn = ((Client*)hClient)->m_pConnection;
	if (pConn)
	{
		ping = pConn->m_Ping;
		return LT_OK;
	}

	RETURN_ERROR(2, "ILTPhysics::GetPing", LT_NOTINITIALIZED);
}

LTRESULT CLTServer::GetNetFlags(HOBJECT hObj, uint32 &flags)
{
	if (!hObj)
		RETURN_ERROR(2, "GetNetFlags", LT_INVALIDPARAMS);

	flags = hObj->m_pSData->m_NetFlags;
	return LT_OK;
}

LTRESULT CLTServer::SetNetFlags(HOBJECT hObj, uint32 flags)
{
	if (!hObj)
		RETURN_ERROR(2, "SetNetFlags", LT_INVALIDPARAMS);

	// Unguaranteed animation only means something on a model.
	if (hObj->m_ObjectType != OT_MODEL)
		flags &= ~NETFLAG_ANIMUNGUARANTEED;

	hObj->m_pSData->m_NetFlags = (uint16)flags;
	return LT_OK;
}

LTRESULT CLTServer::GetClientData(HCLIENT hClient, void *&pData, uint32 &nLength)
{
	if (!hClient)
		RETURN_ERROR(2, "ILTServer::GetClientData", LT_INVALIDPARAMS);

	pData = ((Client*)hClient)->m_pClientData;
	nLength = ((Client*)hClient)->m_ClientDataLen;
	return LT_OK;
}

// ---- the threaded-loader halves (0x1002ec9a-0x1002ef14) ------------------ //

// 0x1002eceb — tell every client about one file, and mark it `inline`.
//
// lithtech.exe has NO out-of-line copy of this function: at /O2 it is expanded
// into all four of the loader halves below (and, through them, into
// ThreadLoadFile and UnloadFile), which is why its call to sm_SendToClient and
// its msg_AllocPacket appear only inside those two in the shipped image. VC6's
// inliner is forward-only, so it has to sit above ThreadLoadModel; at 199
// bytes it is far past what /Os will take, so server.dll's /O1 build still
// calls it out of line and keeps its own byte-identical copy.
inline void CLTServer::BroadcastFileMessage(uint32 nFileType, uint32 nMsgID, uint16 nFileID, int bToAll)
{
	CMessageRef msg;
	LTLink* pListHead;
	LTLink* pCur;

	pListHead = &m_pServerMgr->m_ClientList.m_Head;
	for (pCur = pListHead->m_pNext; pCur != pListHead; pCur = pCur->m_pNext)
	{
		Client* pClient = (Client*)pCur->m_pData;

		if (!bToAll && (pClient->m_ClientFlags & 2))
			continue;

		{
			CAbstractIO* pIO;
			msg.Init(msg_AllocPacket(&pIO, 0x1000, 0x1000));
			if (pIO)
				pIO->Release();
		}

		msg.m_pMsg->WriteByte((uint8)nFileType);
		msg.m_pMsg->WriteWord((uint16)nFileID);

		sm_SendToClient(m_pServerMgr, pClient, nMsgID, msg.m_pMsg, 0, 0x80);
	}
}

// The four loader halves are `inline` for the same reason: none of them has
// an out-of-line body in lithtech.exe either -- ThreadLoadFile (732 bytes at
// 0x0046dd50) and UnloadFile (744 at 0x0046e030) are the two of them expanded
// end to end, BroadcastFileMessage and all. At 84-150 bytes apiece /Os leaves
// them alone, so server.dll's /O1 halves are untouched.
inline LTRESULT CLTServer::ThreadLoadModel(char *pFilename)
{
	Model* pModel;
	CFileIdentifier* pFile;
	LTRESULT dResult;

	dResult = sm_LoadModel(m_pServerMgr, pFilename, &pModel, &pFile, 1, 0);
	if (pFile && (dResult == LT_OK || dResult == LT_INPROGRESS))
		BroadcastFileMessage(FT_MODEL, 0x17, pFile->m_FileID, 0);

	return dResult;
}

inline LTRESULT CLTServer::ThreadLoadTexture(char *pFilename)
{
	CFileIdentifier* pFile;

	if (!fs_GetFileIdentifier(&m_pServerMgr->m_FileMgr, pFilename, 0, (void**)&pFile))
		RETURN_ERROR_INFO(1, "ThreadLoadTexture", LT_MISSINGFILE, pFilename);

	BroadcastFileMessage(FT_TEXTURE, 0x17, pFile->m_FileID, 1);
	return LT_OK;
}

// Both unload halves report their missing-file error as "ThreadLoadTexture"
// — Monolith's copy-paste, and what the shipped literals say.

inline LTRESULT CLTServer::UnloadModel(char *pFilename)
{
	CFileIdentifier* pFile;

	if (!fs_GetFileIdentifier(&m_pServerMgr->m_FileMgr, pFilename, 0, (void**)&pFile))
#ifdef NOLFSRC_MODERN
		RETURN_ERROR_INFO(1, "UnloadModel", LT_MISSINGFILE, pFilename);
#else
		RETURN_ERROR_INFO(1, "ThreadLoadTexture", LT_MISSINGFILE, pFilename);
#endif

	LTRESULT dResult = sm_UnloadModel(m_pServerMgr, pFilename, pFile);
	if (dResult != LT_OK)
		return dResult;

	BroadcastFileMessage(FT_MODEL, 0x18, pFile->m_FileID, 0);
	return LT_OK;
}

inline LTRESULT CLTServer::UnloadTexture(char *pFilename)
{
	CFileIdentifier* pFile;

	if (!fs_GetFileIdentifier(&m_pServerMgr->m_FileMgr, pFilename, 0, (void**)&pFile))
#ifdef NOLFSRC_MODERN
		RETURN_ERROR_INFO(1, "UnloadTexture", LT_MISSINGFILE, pFilename);
#else
		RETURN_ERROR_INFO(1, "ThreadLoadTexture", LT_MISSINGFILE, pFilename);
#endif

	BroadcastFileMessage(FT_TEXTURE, 0x18, pFile->m_FileID, 1);
	return LT_OK;
}

// 0x1002ef12 (23 bytes) / lithtech 0x0046eb30 — ILTServer::SetModelFilenames,
// the one iltserver.h marks "OBSOLETE: use ILTCommon::SetObjectFilenames".
// It is slot 117 of the server interface's vtable (ILTCSBase's 71 slots plus
// ILTServer index 46), and its whole body forwards all three arguments,
// unchanged, to the NEXT slot: `call *0x1d8(%eax)` is 0x1d8 / 4 = 118,
// SetObjectFilenames (below). Placed here because it sits between UnloadTexture
// (ends 0x1002ef0f) and SetObjectFilenames (0x1002ef29) in the image.
//
// servermsg.h's CLTServer has no vtable of its own, so the virtual call is
// written against the SDK interface it implements, through a view that adds
// nothing to it (name ours). The two images encode the three pushes
// differently from this one source: `push 0xc(%esp)` at /O1, `mov
// 0xc(%esp),%edx; push %edx` at /O2.
class CLTServerModelView : public ILTServer
{
public:
	virtual LTRESULT SetModelFilenames(HOBJECT hObj, char *pFilename, char *pSkinName);
	virtual LTRESULT SetObjectFilenames(HOBJECT hObj, char *pFilename, char *pSkinName);
};

LTRESULT CLTServerModelView::SetModelFilenames(HOBJECT hObj, char *pFilename, char *pSkinName)
{
	return SetObjectFilenames(hObj, pFilename, pSkinName);
}

// The obsolete flat form (lithtech 0x0046eb50): build a create struct around
// the two names and hand it to ILTCommon. (The declaration's ctor already
// runs Clear; the explicit Clear is Monolith's, and the linker folds the two
// identical COMDATs onto one address.)
//
// Written against ILTServer (through the view above) because the reach for
// the common interface is ILTCSBase's inline Common(), and the two copies
// are LTStrCpy expansions (what SAFE_STRCPY becomes for a 128-byte buffer).
// /O1 folds all three to the same bytes as a raw member read and strncpy +
// terminator; /O2 does not, because they spend lithtech's per-function
// inline budget: shipped leaves LTVector::Init (0x0041ab50) x2 and
// LTRotation::Init (0x00402ea0) as calls in the constructor's Clear and
// LTRotation::Init as a call in the explicit one. strncpy + m_pCommonLT
// expands everything (301 bytes), LTStrCpy alone runs one unit short (380);
// with Common() it is 409, identical. (servermsg.h still declares a
// CLTServer::SetObjectFilenames; this is its one definition.)
LTRESULT CLTServerModelView::SetObjectFilenames(HOBJECT hObj, char *pFilename, char *pSkinName)
{
	ObjectCreateStruct ocs;

	ocs.Clear();
	LTStrCpy(ocs.m_Filename, pFilename, sizeof(ocs.m_Filename));
	LTStrCpy(ocs.m_SkinName, pSkinName, sizeof(ocs.m_SkinName));

	return Common()->SetObjectFilenames(hObj, &ocs);
}

// ---- the old flat model-state block and friends (0x1002e8a0-0x1002eb36) -- //

LTRESULT CLTServer::CopyFile(const char *pszSourceFile, const char *pszDestFile)
{
	return fs_CopyFile(&m_pServerMgr->m_FileMgr, pszSourceFile, pszDestFile);
}

void CLTServer::SetModelAnimation(HOBJECT hObj, HMODELANIM hAnim)
{
	ModelInstance* pInst = (ModelInstance*)hObj;

	if (hObj->m_ObjectType != OT_MODEL)
		return;

	LTAnimTracker* pTracker = &pInst->m_AnimTracker;
	if (hAnim == pTracker->m_TimeRef.m_Cur.m_iAnim)
		return;

	// bit 7 = FLAG_ANIMTRANSITION (the literal shift is what the bytes say)
	at_SetCurAnim(pTracker, hAnim, (hObj->m_Flags >> 7) & 1);
	sm_SetObjectChangeFlag(g_pServerMgr, hObj, CF_MODELINFO);
}

HMODELANIM CLTServer::GetModelAnimation(HOBJECT hObj)
{
	if (hObj->m_ObjectType == OT_MODEL)
		return ((ModelInstance*)hObj)->m_AnimTracker.m_TimeRef.m_Cur.m_iAnim;

	return (HMODELANIM)-1;
}

void CLTServer::SetModelLooping(HOBJECT hObj, LTBOOL bLoop)
{
	ModelInstance* pInst = (ModelInstance*)hObj;

	if (hObj->m_ObjectType != OT_MODEL)
		return;

	// Direct field read-modify-write: the compiler loads m_Flags into ax
	// once, keeps the arithmetic 16-bit, and stores once. Through a tracker
	// pointer, for the test AND the update: lithtech.exe (0x0046e582) keeps
	// the inlined change-flag's `mov 0x120(%esi),%cl` BELOW the 16-bit store,
	// which it only does when the store is not off the object's own base;
	// spelled `pInst->m_AnimTracker.m_Flags` the load is hoisted above it.
	LTAnimTracker* pTracker = &pInst->m_AnimTracker;
	if (((pTracker->m_Flags >> 1) & 1) != (bLoop != 0))
	{
		if (bLoop)
			pTracker->m_Flags |= AT_LOOPING;
		else
			pTracker->m_Flags &= ~AT_LOOPING;

		sm_SetObjectChangeFlag(g_pServerMgr, hObj, CF_MODELINFO);
	}
}

LTBOOL CLTServer::GetModelLooping(HOBJECT hObj)
{
	if (hObj->m_ObjectType == OT_MODEL)
		return (((ModelInstance*)hObj)->m_AnimTracker.m_Flags >> 1) & 1;

	return LTFALSE;
}

LTRESULT CLTServer::ResetModelAnimation(HOBJECT hObj)
{
	if (hObj && hObj->m_ObjectType == OT_MODEL)
	{
		at_ResetAnim(&((ModelInstance*)hObj)->m_AnimTracker);
		sm_SetObjectChangeFlag(g_pServerMgr, hObj, CF_MODELINFO);
		return LT_OK;
	}

	RETURN_ERROR(1, "ILTServer::ResetModelAnimation", LT_ERROR);
}

uint32 CLTServer::GetModelPlaybackState(HOBJECT hObj)
{
	if (hObj->m_ObjectType == OT_MODEL && at_IsDone(&((ModelInstance*)hObj)->m_AnimTracker))
		return MS_PLAYDONE;

	return 0;
}

HMODELANIM CLTServer::GetAnimIndex(HOBJECT hObj, char *pAnimName)
{
	return ci_GetAnimIndex(hObj, pAnimName);
}

void CLTServer::CPrint(char *pMsg, ...)
{
	char msg[500];
	va_list marker;

	// No va_end here — FormatString has one, this never did.
	va_start(marker, pMsg);
	_vsnprintf(msg, sizeof(msg) - 1, pMsg, marker);
	dsi_ServerError(msg);
}

uint32 CLTServer::GetPointContainers(LTVector *pPoint, HOBJECT *pList, uint32 maxListSize)
{
	PointContainerGather gather;

	gather.m_nElements = 0;
	gather.m_pList = pList;
	gather.m_MaxListSize = maxListSize;
	gather.m_Position = *pPoint;

	((IObjectQuery*)&g_pServerMgr->m_Unknown210)->FindPointContainers(
		pPoint, sm_GatherContainersCB, &gather, 0);

	return gather.m_nElements;
}

LTBOOL CLTServer::GetContainerCode(HOBJECT hObj, uint16 *pCode)
{
	// Two separate returns: the compiler tail-merges the identical bodies
	// into the inline block both checks reach (an `||` jumps the other way).
	if (!hObj)
		return LTFALSE;
	if (hObj->m_ObjectType != OT_CONTAINER)
		return LTFALSE;

	*pCode = ((ContainerInstance*)hObj)->m_ContainerCode;
	return LTTRUE;
}

// *********************************************************************** //
//  0x10035caa — str_FormatMessage, the body behind CLTServer::FormatString.
//
//  Two Win32 calls in series: LoadString pulls the format out of the class
//  module's string table, FormatMessage expands the caller's arguments into
//  a buffer it allocates itself (FORMAT_MESSAGE_ALLOCATE_BUFFER, which is
//  why lpBuffer is the *address* of the pointer and why the caller has to
//  hand the result to str_FreeMessage).
//
//  The first parameter is not the HINSTANCE: the shipped code loads it and
//  then dereferences it (`mov 0x8(%ebp),%eax` / `push (%eax)`), so
//  CClassMgr::m_hModule is a pointer to a module record whose first field is
//  the instance handle. FormatString pushes the field's value, so the extra
//  indirection is here and not at the call site.
//
//  `*pLen` is FormatMessage's own return scaled by three plus one — the
//  shipped `lea 0x1(%eax,%eax,2)`. Nothing in this function reads it back.
//
//  The two failure arms are one tail-merged block: the second pushes its
//  literal and jumps backwards into the first's call. That direction comes
//  from the single-return shape below -- one result pointer set on every
//  path, the FormatMessage call inside the LoadString guard's `else`. With
//  two early `return LTNULL`s cl 8447 kept the second site's tail and jumped
//  the first forward (143/143, two blocks); the nested form with early
//  returns and a `goto` were both worse. The literal order is unchanged by
//  the single-return shape, and it is a check on any other: the LoadString
//  guard has to come first to put the two literals into .data in the shipped order
//  (VC6 emits them in reverse of their .text use, so "FormatMessage error on
//  string %d" at 0x10055298 before "Couldn't get string %d" at 0x100552bc).
// *********************************************************************** //

// Both Win32 entry points come in through windows.h, which lthread.h (and so
// servermgr.h) already pulls into this TU; the A-suffixed names are spelled
// out so the header's UNICODE macros cannot pick the other one. Both are
// dllimport there, which is the `call *[__imp__...]` the shipped code makes.

// 0x10058500 — the console variable "DebugStrings". Its name and its slot
// are in the console-variable record at 0x10052034, whose value pointer is
// this address; nothing else in the image reads it. Integer, not float: the
// shipped test is `cmp %edi,0x10058500` against a zeroed register.
extern int g_CV_DebugStrings;

void* str_FormatMessage(void* pModule, int messageCode, va_list* pMarker, uint32* pLen)
{
	char   szFormat[5000];
	char*  pOut;
	uint32 nLen;
	void*  pRet;

	*pLen = 0;

	if (LoadStringA(*(HINSTANCE*)pModule, messageCode, szFormat, sizeof(szFormat)) == 0)
	{
		if (g_CV_DebugStrings)
			dsi_ServerError("Couldn't get string %d", messageCode);

		pRet = LTNULL;
	}
	else
	{
		nLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
		                      szFormat, 0, 0, (char*)&pOut, 1, pMarker);

		if (nLen == 0)
		{
			if (g_CV_DebugStrings)
				dsi_ServerError("FormatMessage error on string %d", messageCode);

			pRet = LTNULL;
		}
		else
		{
			*pLen = nLen * 3 + 1;
			pRet = pOut;
		}
	}

	return pRet;
}


HSTRING CLTServer::FormatString(int messageCode, ...)
{
	va_list marker;
	uint32 nLen;

	if (g_pServerMgr->m_ClassMgr.m_hModule)
	{
		va_start(marker, messageCode);
		void* pRaw = str_FormatMessage(g_pServerMgr->m_ClassMgr.m_hModule,
		                               messageCode, &marker, &nLen);
		va_end(marker);

		if (pRaw)
		{
			// servermsg.h's declaration, not a local one: this file used
			// to carry a second `void*` overload of the same address,
			// which a call-target check reports as a merge and a whole-DLL link
			// would reject as an unresolved external.
			HSTRING hString = sb_CreateStringRaw((char*)pRaw);
			str_FreeMessage(pRaw);
			return hString;
		}
	}

	return LTNULL;
}

// The buffered name reader (0x1002f1b3) and two ILTCommon forwards.

LTRESULT CLTServer::GetObjectName(HOBJECT hObject, char *pName, uint32 nameBufSize)
{
	if (hObject && pName && nameBufSize > 0)
	{
		void* hIdent = hObject->m_pSData->m_hIdentifier;
		if (hIdent)
		{
			// The temp keeps the length computing after the call, so the two
			// cdecl cleanups batch into one add esp.
			char* pString = dhash_GetKey(hIdent, 0);
			strncpy(pName, pString, nameBufSize - 1);
			pName[nameBufSize - 1] = 0;
		}
		else
		{
			pName[0] = 0;
		}

		return LT_OK;
	}

	RETURN_ERROR_NAMED(2, "ILTPhysics::GetObjectName", LT_INVALIDPARAMS);
}

LTRESULT CLTServer::GetPolyTextureFlags(HPOLY hPoly, uint32 *pFlags)
{
	return m_pCommonLT->GetPolyTextureFlags(hPoly, pFlags);
}

LTRESULT CLTServer::GetModelAnimUserDims(HOBJECT hObject, LTVector *pDims, HMODELANIM hAnim)
{
	return m_pCommonLT->GetModelAnimUserDims(hObject, pDims, hAnim);
}

// ---- the object finders (0x1002e349-0x1002e4c1) -------------------------- //

// An HPOLY is (world-model index << 16) | poly index; the index selects the
// record in the 0x330 table, and the object whose m_pWorldBsp is that
// record's +0x04 pointer is the world model it came from.
LTRESULT CLTServer::GetHPolyObject(const HPOLY hPoly, HOBJECT &hObject)
{
	static char* pErrFnName = "CLTServer::GetHPolyObject";
	LTLink* pListHead;
	LTLink* pLink;
	LTObject* pObj;
	CFileTree* pWorld;

	uint32 iWorld = hPoly >> 16;
	CServerMgr* pMgr = m_pServerMgr;

	// Two separate identical not-found sites: the compiler tail-merges them
	// into the one inline error block both checks reach (an `||` writes the
	// same test pair but sinks the merged block past the walk).
	if (iWorld >= pMgr->m_nFileTrees)
		RETURN_ERROR(5, "CLTServer::GetHPolyObject", LT_NOTFOUND);

	pWorld = pMgr->m_pFileTrees[iWorld];
	if (!pWorld)
		RETURN_ERROR(5, "CLTServer::GetHPolyObject", LT_NOTFOUND);

	// pListHead first: the lea binds before the cursor load (sm_UpdateClients).
	pListHead = pMgr->WorldModelListHead();
	for (pLink = pListHead->m_pNext; pLink != pListHead; pLink = pLink->m_pNext)
	{
		pObj = (LTObject*)pLink->m_pData;
		if (pObj->m_pWorldBsp == (IWorldBsp*)pWorld->m_pList)
		{
			hObject = pObj;
			return LT_OK;
		}
	}

	RETURN_ERROR(2, pErrFnName, LT_INVALIDPARAMS);
}

LTRESULT CLTServer::FindNamedObjects(char *pName, BaseObjArray<HOBJECT> &objArray, uint32 *nTotalFound)
{
	uint32 nTotal;
	uint32 nLen;
	void* hElement;

	nTotal = 0;
	if (!pName)
		RETURN_ERROR_NAMED(2, "CLTServer::FindNamedObjects", LT_INVALIDPARAMS);

	objArray.Reset();
	nLen = strlen(pName) + 1;

	hElement = dhash_FindElement(g_pServerMgr->m_hHash8c0, pName, nLen);
	while (hElement)
	{
		LTObject* pObj = (LTObject*)dhash_GetData(hElement);

		// Objects carrying change-flag 1 are invisible to the finder, but
		// everything else counts toward the total even once the array fills.
		if (!(pObj->m_ChangeFlags & 1))
		{
			objArray.AddObject((HOBJECT)pObj);
			nTotal++;
		}

		hElement = dhash_FindNextElement(g_pServerMgr->m_hHash8c0, hElement, pName, nLen);
	}

	if (nTotalFound)
		*nTotalFound = nTotal;

	return LT_OK;
}

// Move the world model to the candidate transform, gather everything its
// bounding sphere touches, test each candidate's box against the transformed
// BSP, then put the world model back where it was.
LTRESULT CLTServer::FindWorldModelObjectIntersections(HOBJECT hWorldModel, LTVector vNewPos,
                                                      LTRotation rNewRot, BaseObjArray<HOBJECT> &objArray)
{
	static char* pErrFnName = "CLTServer::FindObjectIntersections";
	LTVector vOldPos;
	LTRotation rOldRot;
	ObjectList* pList;
	ObjectLink* pLink;
	float       fRadius;

	if (!hWorldModel)
		RETURN_ERROR_NAMED(2, "CLTServer::FindObjectIntersections", LT_INVALIDPARAMS);

	if (hWorldModel->m_ObjectType != OT_WORLDMODEL && hWorldModel->m_ObjectType != OT_CONTAINER)
		RETURN_ERROR(1, "ILTPhysics::FindWorldModelObjectIntersections", LT_ERROR);

	vOldPos = hWorldModel->m_Pos;
	rOldRot = hWorldModel->m_Rotation;

	hWorldModel->SetPos(vNewPos);
	hWorldModel->m_Rotation = rNewRot;
	obj_RetransformWorldModel(hWorldModel);

	// The radius goes through a named local: the shipped /O2 body stores it
	// to the frame (`mov %edx,0x34(%esp)`, into the slot pList takes two
	// instructions later) and pushes a register copy, which is the local's
	// dead initialisation surviving into a slot the optimiser then reuses.
	// Those two instructions are the whole 544-vs-538 difference.
	fRadius = hWorldModel->m_Radius;
	pList = sm_FindObjectsTouchingSphere(&vNewPos, fRadius);
	if (pList)
	{
		for (pLink = pList->m_pFirstLink; pLink; pLink = pLink->m_pNext)
		{
			LTObject* pObj = (LTObject*)pLink->m_hObject;
			if (!pObj || pObj == hWorldModel)
				continue;

			if (bsp_IntersectBox(hWorldModel->m_pWMIface->GetTransformedBsp(),
			                     &pObj->m_BBoxMin, &pObj->m_BBoxMax)
				&& !(pObj->m_ChangeFlags & 1))
			{
				objArray.AddObject(pLink->m_hObject);
			}
		}

		sm_RelinquishList(pList);
	}

	hWorldModel->SetPos(vOldPos);
	hWorldModel->m_Rotation = rOldRot;
	obj_RetransformWorldModel(hWorldModel);

	return LT_OK;
}

LTRESULT CLTServer::GetWorldBox(LTVector &min, LTVector &max)
{
	if (!m_pServerMgr->m_pWorldFile)
		RETURN_ERROR(2, "ILTPhysics::GetWorldBox", LT_NOTINITIALIZED);

	min = m_pServerMgr->m_WorldBoxMin;
	max = m_pServerMgr->m_WorldBoxMax;
	return LT_OK;
}

// ---- the threaded file loader (0x1002e6e6-0x1002e8a0) -------------------- //

LTRESULT CLTServer::ThreadLoadFile(char *pFilename, uint32 type)
{
	if (!pFilename)
		RETURN_ERROR(2, "ILTPhysics::ThreadLoadFile", LT_INVALIDPARAMS);

	if (type == FT_MODEL)
		return ThreadLoadModel(pFilename);
	else if (type == FT_TEXTURE)
		return ThreadLoadTexture(pFilename);

	RETURN_ERROR(1, "ILTPhysics::ThreadLoadFile", LT_UNSUPPORTED);
}

LTRESULT CLTServer::UnloadFile(char *pFilename, uint32 type)
{
	if (!pFilename)
		RETURN_ERROR(2, "ILTPhysics::UnloadFile", LT_INVALIDPARAMS);

	if (type == FT_MODEL)
		return UnloadModel(pFilename);
	else if (type == FT_TEXTURE)
		return UnloadTexture(pFilename);

	RETURN_ERROR(1, "ILTPhysics::UnloadFile", LT_UNSUPPORTED);
}

LTRESULT CLTServer::OpenFile(char *pFilename, ILTStream **pStream)
{
	// One name variable, two macros: the parameter check reports the code
	// symbolically, the missing-file site reports it expanded ("22").
	static char* pErrFnName = "ILTPhysics::OpenFile";

	if (!pFilename || !pStream)
		RETURN_ERROR_SYM(2, pErrFnName, LT_INVALIDPARAMS);

	*pStream = fs_OpenFile(&m_pServerMgr->m_FileMgr, pFilename);
	if (*pStream)
		return LT_OK;

	RETURN_ERROR(2, pErrFnName, LT_MISSINGFILE);
}

// The four that hand back a raw HMESSAGEWRITE report a failure by returning
// LTNULL, and pass that same LTNULL to the error macro as the "code" — which
// is why the shipped data has a string "0" alongside the usual "60" and "73".

HMESSAGEWRITE CLTServer::StartMessageToObject(LPBASECLASS pSender, HOBJECT hSendTo, uint32 messageID)
{
	if (!hSendTo)
		RETURN_ERROR(2, "ILTPhysics::StartMessageToObject", LTNULL);

	LMessageImpl* pWrite = &m_pServerMgr->StartMessage()->m_Write;

	pWrite->m_Type = MT_TOOBJECT;
	pWrite->m_hSender = pSender ? pSender->m_hObject : LTNULL;
	pWrite->m_hSendTo = hSendTo;
	pWrite->m_MessageID = messageID;

	return (HMESSAGEWRITE)pWrite;
}

LTRESULT CLTServer::StartMessageToServer(LPBASECLASS pSender, uint32 messageID, HMESSAGEWRITE* hWrite)
{
	if (!pSender || !hWrite)
		RETURN_ERROR(2, "ILTPhysics::StartMessageToServer", LT_INVALIDPARAMS);

	LMessageImpl* pWrite = &m_pServerMgr->StartMessage()->m_Write;

	pWrite->m_Type = MT_TOSERVER;
	pWrite->m_MessageID = messageID;
	pWrite->m_hSender = pSender->m_hObject;

	*hWrite = (HMESSAGEWRITE)pWrite;
	return LT_OK;
}

HMESSAGEWRITE CLTServer::StartMessage(HCLIENT hSendTo, uint8 messageID)
{
	LMessageImpl* pWrite = &m_pServerMgr->StartMessage()->m_Write;

	// m_Type first: lithtech.exe (0x46f12c) stores it first; server.dll's
	// /O1 build moves the constant store below m_hClient either way.
	pWrite->m_Type = MT_TOCLIENT;
	pWrite->m_hClient = hSendTo;
	pWrite->m_MessageID = messageID;

	return (HMESSAGEWRITE)pWrite;
}

HMESSAGEWRITE CLTServer::StartInstantSpecialEffectMessage(LTVector* pPos)
{
	if (!pPos)
		RETURN_ERROR(2, "ILTPhysics::StartInstantSpecialEffectMessage", LTNULL);

	LMessageImpl* pWrite = &m_pServerMgr->StartMessage()->m_Write;

	pWrite->m_Type = MT_INSTANTSFX;
	pWrite->m_vPos = *pPos;

	return (HMESSAGEWRITE)pWrite;
}

HMESSAGEWRITE CLTServer::StartSpecialEffectMessage(LPBASECLASS pObject)
{
	if (!pObject)
		RETURN_ERROR(2, "ILTPhysics::StartSpecialEffectMessage", LTNULL);

	LMessageImpl* pWrite = &m_pServerMgr->StartMessage()->m_Write;

	pWrite->m_Type = MT_SFX;
	pWrite->m_hSendTo = pObject->m_hObject;

	return (HMESSAGEWRITE)pWrite;
}

// 0x1002f416 — ILTCSBase::StartHMessageWrite, the sixth message starter and
// the only one servermsg.h's "starters (0x1002f25f-0x1002f415)" block does
// not cover: it sits immediately after them and immediately before
// EndMessage2. It is the SDK's `virtual HMESSAGEWRITE StartHMessageWrite()`
// (iltcsbase.h), and it is the only starter that sizes the packet itself —
// 0x2000 bytes with a 0x1000 grow step, where the five above take whatever
// CServerMgr::StartMessage hands them.
//
// Read off the bytes: no null check on the manager's message (the shipped
// code goes straight from the call into `Init`), and the return is
// `lea 0x8(%esi),%eax` — the embedded LMessageImpl, exactly as the other
// five return it.
//
// servermsg.h's CLTServer does not declare this method and that header is
// not this reconstruction's to change, so it is written against a view of
// the same layout; the padding to 0x224 is servermsg.h's own offset for
// m_pServerMgr. WHAT SHOULD REPLACE THIS: one line in servermsg.h's
// CLTServer, `HMESSAGEWRITE StartHMessageWrite();`, next to the other
// starters, and this view can go.
class CLTServerWriteView
{
public:
	HMESSAGEWRITE StartHMessageWrite();

	uint8       m_pad_000[0x224];
	CServerMgr* m_pServerMgr;                // 0x224
};

#ifndef LT_SERVER_BUILD
// lithtech.exe 0x0046f290 (138 bytes) expands ServerMessage::Init and,
// inside it, CMoArray<uint8>::Init: _InitArray's four stores are inline,
// and all three SetSize2 calls (the explicit Term, Init's own Term, Init's
// SetSize) are CALLS to 0x0040bfe0 (three `push $0x4b4e88`). servermsg.h's
// ServerMessage holds the array as a vptr-only stand-in whose Init is out
// of line (server.dll's /O1 calls 0x1000be24), so lithtech's copy is
// written against this STAND-IN view, the same one cltrest.cpp's twin
// CLTClient::StartHMessageWrite (0x00407630) uses: collection.h's
// CMoArray<uint8> with SetSize2 declared out of line. The real template
// lets /O2 expand SetSize2 here as well (236 bytes against 138, measured) --
// the shipped split is VC6's inline budget, which this view states rather
// than reproduces. The cache is a sub-object with its own setters, as in
// cltrest.cpp (that is what schedules the _InitArray stores between the
// third call's pushes). Store order is lithtech's (0x0046f2a6..0x0046f2bb).
// (names ours)
class CMsgArrayQ12
{
public:
	LTBOOL SetSize2(uint32 newSize, LAlloc* pAlloc);        // 0x0040bfe0

	void   Term(LAlloc* pAlloc = &g_DefAlloc) { SetSize2(0, pAlloc); }
	LTBOOL SetSize(uint32 newSize) { return SetSize2(newSize, &g_DefAlloc); }
	void   SetCacheSize(uint32 size) { m_Cache.SetWantedCache(size); }
	uint32 GetSize() const { return m_nElements; }

	void   _InitArray(uint32 wantedCache)
	{
		m_pArray     = 0;
		m_nElements  = 0;
		m_Cache.SetWantedCache(wantedCache);
		m_Cache.SetCacheSize(0);
	}

	LTBOOL Init(uint32 size, uint32 cacheSize)
	{
		Term();
		_InitArray(cacheSize);

		return SetSize(size);
	}

	uint8  m_vptr[4];                                       // 0x00
	uint8* m_pArray;                                        // 0x04
	uint32 m_nElements;                                     // 0x08
	class CCache
	{
	public:
		void SetCacheSize(uint32 val)   { m_CacheSize = val; }
		void SetWantedCache(uint32 val) { m_WantedCache = val; }
		uint32 m_CacheSize;                                 // 0x0c
		uint32 m_WantedCache;                               // 0x10
	} m_Cache;
};

class CServerMessageQ12
{
public:
	void Init(uint16 nMaxDataLen, uint16 nGrowSize)
	{
		m_DataLen    = 1;
		m_MaxDataLen = nMaxDataLen;
		m_Pos        = 1;
		m_Unknown3a  = 1;
		m_MsgFlags   = 0;

		m_pOwner = this;

		if (nMaxDataLen > m_Data.GetSize())
		{
			m_Data.Term();
			m_Data.Init(nMaxDataLen, nGrowSize);
		}
		else
		{
			m_Data.SetCacheSize(nGrowSize);
		}
	}

	uint8        m_pad_00[0x08];
	uint8        m_Write[0x08];                             // 0x08  LMessageImpl
	void*        m_pOwner;                                  // 0x10  m_Write.m_pOwner
	uint8        m_pad_14[0x34 - 0x14];
	uint16       m_DataLen;                                 // 0x34
	uint16       m_MaxDataLen;                              // 0x36
	uint16       m_Pos;                                     // 0x38
	uint16       m_Unknown3a;                               // 0x3a
	void*        m_pConnection;                             // 0x3c
	uint32       m_MsgFlags;                                // 0x40
	uint8        m_pad_44[0x50 - 0x44];
	CMsgArrayQ12 m_Data;                                    // 0x50 .. 0x63
};
#endif

HMESSAGEWRITE CLTServerWriteView::StartHMessageWrite()
{
#ifdef LT_SERVER_BUILD
	ServerMessage* pMsg = m_pServerMgr->StartMessage();

	pMsg->Init(0x2000, 0x1000);

	return (HMESSAGEWRITE)&pMsg->m_Write;
#else
	CServerMessageQ12* pMsg = (CServerMessageQ12*)m_pServerMgr->StartMessage();

	pMsg->Init(0x2000, 0x1000);

	return (HMESSAGEWRITE)&pMsg->m_Write;
#endif
}

// ILTServer's global-force accessors. Same names as the ILTPhysics pair in
// commonphysics.cpp, but this header takes pointers, so these validate and
// forward — and the vtable slots they call (0x3c/0x40) are exactly where
// SPhysicsLT's own force methods sit, which is how m_pPhysicsLT was pinned.

LTRESULT CLTServer::GetGlobalForce(LTVector* pVec)
{
	if (!pVec)
		RETURN_ERROR(2, "ILTPhysics::GetGlobalForce", LT_INVALIDPARAMS);

	return m_pPhysicsLT->GetGlobalForce(*pVec);
}

LTRESULT CLTServer::SetGlobalForce(LTVector* pVec)
{
	if (!pVec)
		RETURN_ERROR(2, "ILTPhysics::SetGlobalForce", LT_INVALIDPARAMS);

	return m_pPhysicsLT->SetGlobalForce(*pVec);
}


// ---- the old flat API ---------------------------------------------------- //
// ILTServer predates the split interfaces, so CLTServer keeps every original
// method as a wrapper over whichever interface superseded it.

LTRESULT CLTServer::SetFrictionCoefficient(HOBJECT hObj, float coeff)
{
	return m_pPhysicsLT->SetFrictionCoefficient(hObj, coeff);
}

// The out-parameter is faithful to the header and faithfully ignored: the
// shipped code fills a zeroed local and returns that instead.
float CLTServer::GetForceIgnoreLimit(HOBJECT hObj, float &limit)
{
	float val = 0.0f;
	m_pPhysicsLT->GetForceIgnoreLimit(hObj, val);
	return val;
}

void CLTServer::SetForceIgnoreLimit(HOBJECT hObj, float limit)
{
	m_pPhysicsLT->SetForceIgnoreLimit(hObj, limit);
}

LTRESULT CLTServer::GetVelocity(HOBJECT hObj, LTVector *pVel)
{
	return m_pPhysicsLT->GetVelocity(hObj, pVel);
}

LTRESULT CLTServer::SetVelocity(HOBJECT hObj, LTVector *pVel)
{
	return m_pPhysicsLT->SetVelocity(hObj, pVel);
}

LTRESULT CLTServer::GetAcceleration(HOBJECT hObj, LTVector *pAccel)
{
	return m_pPhysicsLT->GetAcceleration(hObj, pAccel);
}

LTRESULT CLTServer::SetAcceleration(HOBJECT hObj, LTVector *pAccel)
{
	return m_pPhysicsLT->SetAcceleration(hObj, pAccel);
}

float CLTServer::GetObjectMass(HOBJECT hObj)
{
	float mass;

	m_pPhysicsLT->GetObjectMass(hObj, mass);
	return mass;
}

void CLTServer::SetObjectMass(HOBJECT hObj, float mass)
{
	m_pPhysicsLT->SetObjectMass(hObj, mass);
}

void CLTServer::GetObjectDims(HOBJECT hObj, LTVector *pNewDims)
{
	m_pPhysicsLT->GetObjectDims(hObj, pNewDims);
}

LTRESULT CLTServer::SetObjectDims(HOBJECT hObj, LTVector *pNewDims)
{
	return m_pPhysicsLT->SetObjectDims(hObj, pNewDims, 0);
}

LTRESULT CLTServer::SetObjectDims2(HOBJECT hObj, LTVector *pNewDims)
{
	return m_pPhysicsLT->SetObjectDims(hObj, pNewDims, SETDIMS_PUSHOBJECTS);
}

LTRESULT CLTServer::MoveObject(HOBJECT hObj, LTVector *pNewPos)
{
	return m_pPhysicsLT->MoveObject(hObj, pNewPos, 0);
}

LTRESULT CLTServer::GetStandingOn(HOBJECT hObj, CollisionInfo *pInfo)
{
	return m_pPhysicsLT->GetStandingOn(hObj, pInfo);
}

uint32 CLTServer::GetObjectFlags(HOBJECT hObj)
{
	uint32 flags = 0;

	m_pCommonLT->GetObjectFlags(hObj, OFT_Flags, flags);
	return flags;
}

void CLTServer::SetObjectFlags(HOBJECT hObj, uint32 flags)
{
	m_pCommonLT->SetObjectFlags(hObj, OFT_Flags, flags);
}

char* CLTServer::GetObjectName(HOBJECT hObject)
{
	if (hObject && hObject->m_pSData->m_hIdentifier)
		return dhash_GetKey(hObject->m_pSData->m_hIdentifier, 0);

	return g_szNoName;
}

// ---- the senders (0x1002f43a-0x1002f8b0) ---------------------------------- //

// Case order is the lithtech.exe block order: /O2 lays the cases out in
// SOURCE order, and 0x46f320 has TOCLIENT, TOOBJECT, SFX, INSTANTSFX,
// TOSERVER. server.dll's /O1 layout does not follow the source (it comes out
// TOSERVER, INSTANTSFX, SFX, TOCLIENT, TOOBJECT either way), but its tail
// merging does: with TOSERVER last, a `return LT_OK` inside that case gets
// cross-jumped into INSTANTSFX's dResult tail (`xor %edi,%edi`, 15 bytes
// short). TOSERVER `break`s to one `return LT_OK` after the switch instead,
// which keeps its own `xor %eax,%eax` tail at 0x1002f4c9 as shipped.
LTRESULT CLTServer::EndMessage2(HMESSAGEWRITE_I hMessage, uint32 flags)
{
	LTRESULT dResult;
	LMessageImpl* pMsg = (LMessageImpl*)hMessage;

	if (!hMessage || hMessage->Verify() != LT_OK)
		RETURN_ERROR_NAMED(2, "CLTServer::EndMessage2", LT_INVALIDPARAMS);

	switch (pMsg->m_Type)
	{
		default:
			RETURN_ERROR(1, "EndMessage", LT_ERROR);

		case MT_TOCLIENT:
			dResult = ((ILTServerSendView*)this)->SendToClient(*(IMessageWrite*)hMessage,
				(uint8)pMsg->m_MessageID, pMsg->m_hClient, flags);
			hMessage->EndWrite();
			return dResult;

		case MT_TOOBJECT:
			dResult = ((ILTServerSendView*)this)->SendToObject(*(IMessageWrite*)hMessage,
				pMsg->m_MessageID, pMsg->m_hSender, pMsg->m_hSendTo, flags);
			hMessage->EndWrite();
			return dResult;

		case MT_SFX:
			dResult = ((ILTServerSendView*)this)->SetObjectSFXMessage(pMsg->m_hSendTo,
				*(IMessageWrite*)hMessage);
			hMessage->EndWrite();
			return dResult;

		case MT_INSTANTSFX:
			((ILTServerSendView*)this)->SendSFXMessage(*(IMessageWrite*)hMessage,
				pMsg->m_vPos, flags);
			dResult = LT_OK;
			hMessage->EndWrite();
			return dResult;

		case MT_TOSERVER:
			((ILTServerSendView*)this)->SendToServer(*(IMessageWrite*)hMessage,
				pMsg->m_MessageID, pMsg->m_hSender, flags);
			hMessage->EndWrite();
			break;
	}

	return LT_OK;
}

LTRESULT CLTServer::EndMessage(HMESSAGEWRITE_I hMessage)
{
	return ((ILTServerSendView*)this)->EndMessage2(*(IMessageWrite*)hMessage, MESSAGE_GUARANTEED);
}

LTRESULT CLTServer::SendToClient(IMessageWrite &msg, uint8 msgID, HCLIENT hSendTo, uint32 flags)
{
	LMessageImpl* pMsg = (LMessageImpl*)&msg;

	if (!&msg || msg.Verify() != LT_OK)
		RETURN_ERROR_NAMED(2, "CLTServer::SendToClient", LT_INVALIDPARAMS);

	((CPacket*)pMsg->m_pOwner)->WriteType(msgID);

	if (hSendTo)
	{
		sm_QueueMessageToClient(m_pServerMgr, (Client*)hSendTo, 0xd, pMsg->m_pOwner, flags);
	}
	else
	{
		LTLink* pCur;
		for (pCur = m_pServerMgr->m_ClientList.m_Head.m_pNext;
		     pCur != &m_pServerMgr->m_ClientList.m_Head; pCur = pCur->m_pNext)
		{
			sm_QueueMessageToClient(m_pServerMgr, (Client*)pCur->m_pData, 0xd,
				pMsg->m_pOwner, flags);
		}
	}

	return LT_OK;
}

LTRESULT CLTServer::SendToObject(IMessageWrite &msg, uint32 msgID, HOBJECT hSender,
                                 HOBJECT hSendTo, uint32 flags)
{
	LMessageImpl* pMsg = (LMessageImpl*)&msg;

	if (!hSendTo || msg.Verify() != LT_OK)
		RETURN_ERROR(2, "ILTPhysics::SendToObject", LT_INVALIDPARAMS);

	pMsg->m_pOwner->m_Unknown38 = 1;
	pMsg->m_MessageID = msgID;

	hSendTo->m_pSData->m_pBaseClass->ObjectMessageFn(hSender, msgID, (HMESSAGEREAD)&msg);
	return LT_OK;
}

LTRESULT CLTServer::SetObjectSFXMessage(HOBJECT hObject, IMessageWrite &msg)
{
	LMessageImpl* pMsg = (LMessageImpl*)&msg;

	if (!&msg || msg.Verify() != LT_OK)
		RETURN_ERROR_NAMED(2, "CLTServer::SetObjectSFXMessage", LT_INVALIDPARAMS);

	sm_SetObjectSFXMessage(m_pServerMgr, hObject, pMsg->m_pOwner);
	return LT_OK;
}

LTRESULT CLTServer::SendSFXMessage(IMessageWrite &msg, LTVector &pos, uint32 flags)
{
	LMessageImpl* pMsg = (LMessageImpl*)&msg;

	if (!&msg || msg.Verify() != LT_OK)
		RETURN_ERROR_NAMED(2, "CLTServer::SendSFXMessage", LT_INVALIDPARAMS);

	sm_SendSFXMessage(m_pServerMgr, 0x12, pMsg->m_pOwner, LTNULL, &pos, flags);
	return LT_OK;
}

LTRESULT CLTServer::SendToServer(IMessageWrite &msg, uint32 msgID, HOBJECT hSender, uint32 flags)
{
	LMessageImpl* pMsg = (LMessageImpl*)&msg;

	if (!&msg || msg.Verify() != LT_OK)
		RETURN_ERROR_NAMED(2, "CLTServer::SendToServer", LT_INVALIDPARAMS);

	// Success first; the not-initialized error owns the lexically-last return.
	IServerShell* pShell = g_pServerMgr->m_ClassMgr.m_pServerShell;
	if (pShell)
	{
		// The message's own routing fields, not the parameters, feed the shell.
		pMsg->m_pOwner->m_Unknown38 = 1;
		pShell->OnObjectMessage(pMsg->m_hSender->m_pSData->m_pBaseClass,
			pMsg->m_MessageID, (HMESSAGEREAD)&msg);
		return LT_OK;
	}

	RETURN_ERROR(2, "ILTPhysics::SendToServer", LT_NOTINITIALIZED);
}

// ---- strings, console variables, time ------------------------------------ //

HSTRING CLTServer::CopyString(HSTRING hString)
{
	return sb_CopyString(hString);
}

HSTRING CLTServer::CreateString(char *pString)
{
	return sb_CreateString(pString);
}

void CLTServer::FreeString(HSTRING hString)
{
	sb_FreeString(hString);
}

LTBOOL CLTServer::CompareStrings(HSTRING hString1, HSTRING hString2)
{
	return sb_CompareStrings(hString1, hString2);
}

LTBOOL CLTServer::CompareStringsUpper(HSTRING hString1, HSTRING hString2)
{
	return sb_CompareStringsUpper(hString1, hString2);
}

char* CLTServer::GetStringData(HSTRING hString)
{
	return sb_GetStringData(hString);
}

float CLTServer::GetVarValueFloat(HCONSOLEVAR hVar)
{
	if (!hVar)
		return 0.0f;

	return ((LTCommandVar*)hVar)->m_FloatVal;
}

char* CLTServer::GetVarValueString(HCONSOLEVAR hVar)
{
	if (!hVar)
		return LTNULL;

	return ((LTCommandVar*)hVar)->m_pStringVal;
}

float CLTServer::GetTime()
{
	return g_pServerMgr->m_Time;
}

float CLTServer::GetFrameTime()
{
	return g_pServerMgr->m_FrameTime;
}

LTRESULT CLTServer::RemoveObject(HOBJECT hObj)
{
	if (!hObj)
		return LT_ERROR;

	sm_RemoveObject(g_pServerMgr, hObj);
	return LT_OK;
}

LTRESULT CLTServer::FreeUnusedModels()
{
	return m_pServerMgr->FreeUnusedModels();
}

// The interface's own copy of the driver scan — same loop as
// MyServerInterface::SendTo, but here the index is a uint16. m_pServerMgr is
// read at each use, not through a local: lithtech.exe (0x46ea70) then loads
// it back into %ecx and gives the counter %edx; a local took %edx.
LTRESULT CLTServer::SendTo(const void *pData, uint32 len, const char *sAddr, uint32 port)
{
	for (uint16 i = 0; i < m_pServerMgr->m_NetMgr.m_Drivers.m_nElements; i++)
	{
		CBaseDriver* pDriver = m_pServerMgr->m_NetMgr.m_Drivers.m_pArray[i];
		if (pDriver->m_DriverFlags & NETDRIVER_TCPIP)
			return pDriver->SendTo(pData, len, sAddr, port);
	}

	return LT_NOTINITIALIZED;
}


// ---- the counted references --------------------------------------------- //
//
// The cursor takes a reference on what arrives and drops the one it was
// holding, in that order, so assigning a reference to itself is safe, and
// hands back what it now holds. (CMessageRef::Init is the same body, hoisted
// to the head of this file so /O2 can expand it — see the note there.)

// 0x10023e86 — the cursor takes the message directly.
ServerMessage* CMsgCursor::Init(ServerMessage* pMsg)
{
	if (m_pMsg)
		m_pMsg->Release();

	m_pMsg = pMsg;

	if (m_pMsg)
		m_pMsg->AddRef();

	return m_pMsg;
}

// 0x1002860b and 0x1002861f — the update context is nothing but its four
// references; construction empties them and teardown releases them, the
// cursor array last-to-first.
#ifdef LT_SERVER_BUILD   // inline in lithtech.exe -- see servermsg.h
CUpdateContext::CUpdateContext()
{
}

CUpdateContext::~CUpdateContext()
{
}
#endif


// *********************************************************************** //
//  THE CLIENT FILE LIST AND FILE TRANSFER (0x1000e650-0x1000eba9)
//
//  A block of eight functions that live between the message allocator and
//  CPacket's template instantiations, and that no reconstruction had
//  reached. Nothing in it pushes a string literal, so nothing names it: the
//  names below are ours, read off what the code does and off its two call
//  sites — 0x10027052/0x10027066 walk a client's file hash and hand each
//  entry here (the caller is a Client, `0x3c0(%esi)` being m_ClientFlags),
//  and 0x1002bf6d does the same from the file manager. The state object is
//  Client+0x18c.
//
//  Two structural facts that took reading to settle:
//
//   * a file record IS its own list node. 0x1000e650 stores the record's
//     own address into its `m_pData` (`mov %esi,0x8(%esi)`), so the walkers
//     can read the record out of the link's +0x08 while the remover
//     unlinks the same object through +0x00/+0x04. Without that store the
//     two halves look like they are talking about different objects.
//   * the object at state+0x18 is one of the engine's vptr-over-StructBank
//     banks, 0x24 bytes, sharing the three-instruction Allocate forward at
//     0x1000ed6f with the hash-element and thread-message banks. That is
//     what puts its free list at state+0x34 — and the free in
//     0x1000e887 is servermgr.h's inline sb_Free on it, not a call.
// *********************************************************************** //

class CXferFile;

// The record bank: objbank.h's bank over the records below (vtable
// 0x1004d648). Its constructor is the template's default one, which
// ph_CreateHolder's `new` reaches through CClientFiles': vptr, then
// sb_Init(0x1c, 0x20) (S:0x1000e5aa..0x1000e5b9, expanded whole at
// 0x0042fbe0..0x0042fbef). Slot 0 is the scalar deleting destructor
// 0x1000ed95, slot 1 the shared five-byte forward 0x1000de9b, slot 2 the
// shared Free 0x10015ac1 and slot 3 Init 0x1000ed7a.
//
// 0x1000ed6f — `add $0x4,%ecx; push %ecx; call sb_Allocate; pop %ecx; ret`
// — is the bank's typed AllocateT for this record, and the one body
// /OPT:ICF kept for every bank of LTLink-headed records (the hash elements,
// the list nodes, these). xfer_AddFile calls it (`lea 0x18(%edi),%ecx;
// call 0x1000ed6f` at 0x1000e668), where a forward with no constructor
// behind it would have been expanded at /O1: the record's LTLink member
// has an (empty) constructor, so the placement new in AllocateT calls one,
// and /O1 judges the body too big to expand before that call goes away.
// lithtech.exe's /O2 does expand it, sb_Allocate and all
// (0x0042fd22..0x0042fd43: `lea 0x1c(%edi),%ebx`, this+4, the free list at
// +0x18 and the grow count at +0x08 around the cdecl sb_Grow call).
typedef CObjectBank<CXferFile> CXferFileBank;

// One file on a client's list. The three payload fields are exactly what
// 0x1000e738 puts on the wire, in that order: the file's id as a WORD out of
// a dword field, a second dword, and the name as a string. The flag word is
// three bits, all of them read here: 1 = counted separately (and the record
// goes on the FRONT of the list rather than the back), 2 = still to be
// announced, 4 = this is the file currently being sent.
class CXferFile
{
public:
	LTLink m_Link;                           // 0x00  m_pData points at THIS
	uint32 m_nFileID;                        // 0x0c
	uint32 m_nUnknown10;                     // 0x10
	char*  m_pName;                          // 0x14
	uint16 m_Flags;                          // 0x18
};

#define XFERFILE_COUNTED  1
#define XFERFILE_PENDING  2
#define XFERFILE_SENDING  4

// Client+0x18c. Everything below 0x3c is the record bank; everything above
// is the list, the two counters, and the transfer in progress.
class CClientFiles : public CUnknownA14
{
public:
	// 0x1000e5a2 (svnew.cpp's CClientFilesView body): the base at +0x00 and
	// the bank below, nothing of its own. In the class body: /O1 declines it
	// (ph_CreateHolder calls 0x1000e5a2) and /O2 expands it (0x0042fbd9).
	CClientFiles() {}

	CXferFileBank m_FileBank;                // 0x18
	LTLink        m_ListHead;                // 0x3c
	uint32        m_nCounted;                // 0x48  the XFERFILE_COUNTED ones
	uint32        m_nFiles;                  // 0x4c  all of them
	uint32        m_State;                   // 0x50  1 while a file is going out
	uint32        m_Flags;                   // 0x54  bit 4 turns the add hook on
	float         m_fBudget;                 // 0x58  bytes this frame may cost
	float         m_fRate;                   // 0x5c  bytes per unit of budget;
	                                         //       zeroed after every chunk
	// The other two hooks, both read only by the frame pump at 0x1000e9ee
	// and both __cdecl for the same reason m_pfnDone is: each call site
	// cleans its own two arguments with a pair of `pop %ecx`. The open hook
	// answers the stream to send the file from (null = could not open) and
	// the failure hook answers 1 when the record should simply be dropped.
	ILTStream*  (*m_pfnOpen)(CClientFiles* pState, char* pName);          // 0x60
	// The completion hook, called with the state and the stream that has
	// just been drained. __cdecl: the shipped call site cleans its own two
	// arguments with a pair of `pop %ecx`.
	void        (*m_pfnDone)(CClientFiles* pState, ILTStream* pStream);   // 0x64
	int         (*m_pfnFail)(CClientFiles* pState, char* pName);          // 0x68
	class CNetMgrSendView* m_pNetMgr;        // 0x6c
	class NetConnection*   m_pConnection;    // 0x70
	uint8         m_pad_74[0x78 - 0x74];
	ILTStream*    m_pStream;                 // 0x78  the file being sent
	CXferFile*    m_pCurFile;                // 0x7c  its record
	uint32        m_nChunks;                 // 0x80  chunks sent; 6 stops it
	uint32        m_nRemaining;              // 0x84  bytes left in the file
};

// 0x10021c55 — the net manager's queued send. Same view class, name and
// signature sharedcommon.cpp and smgr.cpp use, so all three reference one
// symbol.
class CNetMgrSendView
{
public:
	int SendToConnection(class ServerMessage* pMsg, class NetConnection* pConn,
	                     uint32 flags);      // 0x10021c55
};

// The three ServerMessage members servermsg.h does not carry, on a
// layout-identical view: the dword writer, the string
// writer, and the space accessor serverlightanim.cpp already reaches the
// same way. 0x1000ecd6 is CPacket::WriteType<uint32> — the 153-byte twin of
// the WriteWord at 0x1000ec3d — and 0x10026137 is a length-prefixed string
// write that forwards into WriteData with strlen+1 bytes (a null argument
// writes one zero byte instead). Names ours.
class CMsgWriteView
{
public:
	void WriteDWord(uint32 nValue);          // 0x1000ecd6
	void WriteString(char* pStr);            // 0x10026137

	// 0x1000ebd0 — CPacket::ReadType<uint16>, the read side of the same
	// template family (netmgr.cpp's ServerMessageTypeView names all six).
	// The dummy pointer carries the template argument and every call site
	// pushes a literal zero for it.
	uint16 ReadWord(uint16* pDummy);         // 0x1000ebd0

	// 0x1000e845 — the payload bytes still free: the limit, less the write
	// cursor, less the seven-byte wire header, floored at zero. Eighteen
	// bytes, and `inline` because lithtech.exe has no out-of-line copy of it
	// inside xfer_SendPendingFiles: 0x0042fead/0x0042feb3 read m_Unknown38
	// and m_MaxDataLen straight off %esi and 0x0042febe/0x0042fec1/0x0042fec3
	// are the `sub $0x7 / jns / xor` floor, with the zero-extension of
	// m_MaxDataLen riding on the %eax the inline strlen had just cleared.
	//
	// /O1-safe by construction rather than by the size rule: the original
	// source must have had this body visible in this translation unit (only
	// an inline candidate can be expanded at /Ob1), and server.dll's build of
	// the SAME unit still CALLS 0x1000e845 from xfer_SendPendingFiles
	// (0x1000e7ce). So /Os declines it at this size and marking it here
	// cannot move the /O1 bytes. Nothing of ours defines the symbol today —
	// the view was declaration-only — so there is no standalone match to
	// lose either.
	int GetSpaceLeft()
	{
		int nLeft = (int)m_MaxDataLen - (int)m_Unknown38 - 7;

		// lithtech.exe: an if, not a ternary -- /O2 shows `sub $7; jns;
		// xor %eax,%eax` at 0x0042febe where the ternary gives the
		// branchless setl/dec/and. server.dll keeps the ternary: the if-form
		// is expanded at /O1 too (xfer_SendPendingFiles 218 -> 234 bytes,
		// server matched 110 -> 108), and shipped 0x1000e7ce still calls.
#ifndef LT_SERVER_BUILD
		if (nLeft < 0)
			nLeft = 0;

		return nLeft;
#else
		return nLeft < 0 ? 0 : nLeft;
#endif
	}

	uint8  m_pad_00[0x36];
	uint16 m_MaxDataLen;                     // 0x36  the payload limit
	uint16 m_Unknown38;                      // 0x38  the write cursor
};

// 0x10036d21 reached with THREE arguments. The engine's empty functions were
// all folded together by /OPT:ICF, so this is not dsi_OnReturnError even
// though it shares its address: it is a release no-op whose debug build
// logged the file being added. Which no-op the source named is
// unrecoverable; the argument list is not (name ours).
void xfer_OnFileAdded(uint32 nFileID, uint32 nUnknown, char* pName);


// The five dwords ph_CreateHolder copies to +0x60 in one piece: the three
// hooks, the net manager and the connection. smgr.cpp builds one on its
// frame (S:0x10026ea8..0x10026ebe) under the same name.
class CPacketHolderDesc
{
public:
	void*  m_pfnOpen;                        // +0x00  -> CClientFiles+0x60
	void*  m_pfnDone;                        // +0x04
	void*  m_pfnFail;                        // +0x08
	void*  m_pNetMgr;                        // +0x0c
	void*  m_pConn;                          // +0x10
};

// 0x1000e507 / 0x0042fbc0 -- build a client's file-transfer state. The base
// at +0x00 is servermgr.h's CUnknownA14 (the same 0x1003d500 constructor
// and 0x1003d600 Init); the bank the constructor made with sb_Init is
// thrown away and remade with sb_Init2's 0x80/0x80. The descriptor goes in
// with memcpy: a call at /O1, `rep movsl` of five dwords at /O2 (0x0042fc5e).
// m_State is cleared twice, as both images do (S:0x1000e53e, S:0x1000e592).
// Name smgr.cpp's.
void* ph_CreateHolder(CPacketHolderDesc* pDesc, uint32 nFlags)
{
	CClientFiles* pState = new CClientFiles;

	pState->m_nCounted   = 0;
	pState->m_nFiles     = 0;
	pState->m_State      = 0;
	pState->m_Flags      = nFlags;
	pState->m_fRate      = 0.0f;
	*(uint32*)pState->m_pad_74 = 0;
	pState->m_pStream    = LTNULL;
	pState->m_pCurFile   = LTNULL;
	pState->m_nChunks    = 0;
	pState->m_nRemaining = 0;

	pState->Init(0x1000);

	sb_FreeChunks(&pState->m_FileBank.m_Bank, 1);
	sb_Init2(&pState->m_FileBank.m_Bank, 0x1c, 0x80, 0x80);

	memcpy(&pState->m_pfnOpen, pDesc, sizeof(CPacketHolderDesc));

	dl_TieOff(&pState->m_ListHead);

	pState->m_State   = 0;
	pState->m_fBudget = 100000.0f;

	return pState;
}


// 0x1000e887 — take one record off the list and put it back in the bank.
// Both counters come down first, the unlink is CheapLTLink::Remove inline
// (including its TieOff, which writes m_pNext before m_pPrev because
// `m_pPrev = m_pNext = this` evaluates right to left), and the record goes
// onto the bank's own free list through its first dword.
void xfer_FreeFile(CClientFiles* pState, CXferFile* pFile)
{
	pState->m_nFiles--;

	if (pFile->m_Flags & XFERFILE_COUNTED)
		pState->m_nCounted--;

	dl_Remove(&pFile->m_Link);

	sb_Free(&pState->m_FileBank.m_Bank, pFile);
}

// 0x1000e857 — drop every record that is not the one being sent. The next
// link is taken before the body, so freeing the current record is safe.
void xfer_FreeAllFiles(CClientFiles* pState)
{
	LTLink* pCur = pState->m_ListHead.m_pNext;

	while (pCur != &pState->m_ListHead)
	{
		LTLink*    pNext = pCur->m_pNext;
		CXferFile* pFile = (CXferFile*)pCur->m_pData;

		if (!(pFile->m_Flags & XFERFILE_SENDING))
			xfer_FreeFile(pState, pFile);

		pCur = pNext;
	}
}

// 0x1000eaa2 — the first record still waiting to be announced, or null.
// Unrotated: the head test stays at the top of the loop and the back edge is
// an unconditional `jmp` to it.
CXferFile* xfer_FindPendingFile(CClientFiles* pState)
{
	LTLink* pCur = pState->m_ListHead.m_pNext;

	while (pCur != &pState->m_ListHead)
	{
		CXferFile* pFile = (CXferFile*)pCur->m_pData;

		if (pFile->m_Flags & XFERFILE_PENDING)
			return pFile;

		pCur = pCur->m_pNext;
	}

	return LTNULL;
}

// 0x1000e738 — one record's three fields into a message, under packet id
// 0x32. The state is taken and never read: the message carries everything.
//
// `inline` because lithtech.exe has no out-of-line copy: xfer_AddFile
// (0x0042fd10) opens its announce block with `mov 0x54(%ebp),%ecx;
// movb $0x32,(%ecx)` and the two writers straight after it, and
// xfer_SendPendingFiles (0x0042fe40) does the same in its loop. 53 bytes is
// still past /Os, so the /O1 pair keep their calls.
inline void xfer_WriteFileInfo(CClientFiles* pState, CXferFile* pFile, ServerMessage* pMsg)
{
	pMsg->m_pData[0] = 0x32;

	pMsg->WriteWord((uint16)pFile->m_nFileID);
	((CMsgWriteView*)pMsg)->WriteDWord(pFile->m_nUnknown10);
	((CMsgWriteView*)pMsg)->WriteString(pFile->m_pName);
}

// 0x1000e650 — put one file on the list, and unless it is flagged as
// still-to-be-announced, tell the client about it there and then.
//
// Read off the bytes: the record's `m_pData = this` is stored before the
// list branch; a COUNTED record goes on the front of the list and bumps the
// second counter, everything else goes on the back (the shipped code
// reaches the tail as `m_ListHead.m_pPrev->AddAfter(...)`, which is
// dl_Insert on the previous link, not ltlink.h's dl_AddBefore); and the
// scoped message reference is constructed before the null guard, which is
// why that guard's return skips the destructor entirely.
int xfer_AddFile(CClientFiles* pState, char* pName, uint32 nUnknown,
                 uint32 nFileID, uint16 nFlags)
{
	CMessageRef msg;

	if (!pState)
		return 0;

	CXferFile* pFile = pState->m_FileBank.AllocateT();

	pFile->m_Link.m_pData = pFile;
	pFile->m_Flags = nFlags;
	pFile->m_nFileID = nFileID;
	pFile->m_nUnknown10 = nUnknown;
	pFile->m_pName = pName;

	if (nFlags & XFERFILE_COUNTED)
	{
		dl_Insert(&pState->m_ListHead, &pFile->m_Link);

		pState->m_nCounted++;
	}
	else
	{
		dl_Insert(pState->m_ListHead.m_pPrev, &pFile->m_Link);
	}

	if (!(nFlags & XFERFILE_PENDING))
	{
		{
			CAbstractIO* pIO;
			msg.Init(msg_AllocPacket(&pIO, 0x1000, 0x1000));
			if (pIO)
				pIO->Release();
		}

		// The message is a NAMED local here, where every other site in
		// this block reaches through `msg.m_pMsg`: the shipped code puts it
		// in %ebx across the two calls and leaves pName in its parameter
		// slot, which is the one byte and the whole register binding.
		ServerMessage* pMsg = msg.m_pMsg;

		xfer_WriteFileInfo(pState, pFile, pMsg);

		pState->m_pNetMgr->SendToConnection(pMsg, pState->m_pConnection, 0x80);
	}

	if (pState->m_Flags & 4)
		xfer_OnFileAdded(nFileID, nUnknown, pName);

	pState->m_nFiles++;

	return 1;
}

// 0x10036d21 — see the declaration above: empty in release, the image's one
// shared bare `ret`. Defined after its caller so nothing expands it.
void xfer_OnFileAdded(uint32 nFileID, uint32 nUnknown, char* pName)
{
}

// 0x1000e76b — announce every record that is still pending, packing as many
// as fit into one message and flushing whenever the next one would not.
//
// Read off the bytes: the room needed is `strlen(name) + 6` (the id word,
// the dword, and the string's own terminator), it is compared SIGNED against
// the space accessor's answer, and the flush happens when it does not fit —
// before the write, not after. A flushed message is reset by storing 1 into
// the write cursor and then into the payload length, in that order. The
// trailing flush is guarded by `m_Unknown34 - 1 > 0`, an int subtraction on
// a zero-extended word, so an empty message is not sent.
void xfer_SendPendingFiles(CClientFiles* pState)
{
	CMessageRef msg;

	if (!pState)
		return;

	{
		CAbstractIO* pIO;
		msg.Init(msg_AllocPacket(&pIO, 0x1000, 0x1000));
		if (pIO)
			pIO->Release();
	}

	LTLink* pCur = pState->m_ListHead.m_pNext;

	while (pCur != &pState->m_ListHead)
	{
		CXferFile* pFile = (CXferFile*)pCur->m_pData;

		if (pFile->m_Flags & XFERFILE_PENDING)
		{
			int nNeeded = strlen(pFile->m_pName) + 6;

			if (nNeeded >= ((CMsgWriteView*)msg.m_pMsg)->GetSpaceLeft())
			{
				pState->m_pNetMgr->SendToConnection(msg.m_pMsg,
					pState->m_pConnection, 0x80);

				// One named pointer for the pair of stores: written twice
				// through `msg.m_pMsg`, VC6 has to reload it between them
				// (the first store could alias the reference) and the
				// function is three bytes long.
				ServerMessage* pMsg = msg.m_pMsg;
				pMsg->m_Unknown38 = 1;
				pMsg->m_Unknown34 = 1;
			}

			xfer_WriteFileInfo(pState, pFile, msg.m_pMsg);

			pFile->m_Flags &= ~XFERFILE_PENDING;
		}

		pCur = pCur->m_pNext;
	}

	if (msg.m_pMsg->m_Unknown34 - 1 > 0)
	{
		pState->m_pNetMgr->SendToConnection(msg.m_pMsg,
			pState->m_pConnection, 0x80);
	}
}

// 0x1000e8ba — the transfer's end from the outside: send the empty packet id
// 0x34, run the completion hook, and clear everything the transfer was
// holding. Both guards return without running the reference's destructor,
// the reference still being null at that point.
void xfer_FinishTransfer(CClientFiles* pState)
{
	CMessageRef msg;

	if (!pState)
		return;

	if (pState->m_State != 1)
		return;

	{
		CAbstractIO* pIO;
		msg.Init(msg_AllocPacket(&pIO, 0x1000, 0x1000));
		if (pIO)
			pIO->Release();
	}

	msg.m_pMsg->m_pData[0] = 0x34;

	pState->m_pNetMgr->SendToConnection(msg.m_pMsg, pState->m_pConnection, 0x80);

	pState->m_pfnDone(pState, pState->m_pStream);

	pState->m_pCurFile->m_Flags &= ~XFERFILE_SENDING;

	pState->m_pCurFile = LTNULL;
	pState->m_pStream = LTNULL;
	pState->m_State = 0;
}

// 0x1000eac1 — send the next chunk of the file in progress, if this frame's
// budget will carry it.
//
// Read off the bytes:
//
//   * the chunk is 0xfd8 bytes and the budget test is the same number as a
//     FLOAT divided by the rate, compared against the budget. 4056.0f is in
//     .rdata, so the source carries it as a float literal rather than
//     converting the integer.
//   * the chunk count is checked against 6 before the message is allocated
//     and incremented after the send, so six chunks a frame is the cap.
//   * the last chunk is recognised BEFORE the read, by the remaining count
//     fitting: the flag it sets is what runs the completion hook, and the
//     record is freed before the hook is called, not after.
//   * the read buffer is 0x1000 bytes on the stack (VC6's `_chkstk` prologue
//     for a frame over a page) while the chunk itself is 0xfd8 — the wire
//     header's forty bytes are the difference.
//
// The budget test is a POSITIVE guard, and the two statements inside the
// last-chunk test are in the order the flag comes first. Neither is visible
// in server.dll: /O1 tail-merges the reference's destructor, so 0x1000eac1
// has one copy of it at 0x1000eb9b reached by `ja` and by fallthrough
// either way, and /O1's `push $1 / mov %eax,%edi / pop %ebx` schedules the
// two stores into one another whichever order they are written in.
// lithtech.exe separates them: 0x004302c0 jumps FORWARD past the body to a
// single destructor at 0x004303a6 (an early `return` gives that block twice,
// which is the 25 bytes we were over by), and it emits `mov $0x1,%ebx`
// before `mov %eax,%ebp`.
void xfer_SendNextChunk(CClientFiles* pState)
{
	uint8       buf[0x1000];
	CMessageRef msg;
	uint32      nSend = 0xfd8;

	if (pState->m_nChunks == 6)
		return;

	{
		CAbstractIO* pIO;
		msg.Init(msg_AllocPacket(&pIO, 0x1000, 0x1000));
		if (pIO)
			pIO->Release();
	}

	if (4056.0f / pState->m_fRate <= pState->m_fBudget)
	{
		int bLast = 0;

		msg.m_pMsg->m_pData[0] = 0x35;

		if (pState->m_nRemaining <= nSend)
		{
			bLast = 1;
			nSend = pState->m_nRemaining;
		}

		pState->m_pStream->Read(buf, nSend);

		msg.m_pMsg->WriteData(buf, nSend);

		pState->m_pNetMgr->SendToConnection(msg.m_pMsg, pState->m_pConnection, 0x80);

		if (bLast)
		{
			xfer_FreeFile(pState, pState->m_pCurFile);

			pState->m_pCurFile = LTNULL;

			pState->m_pfnDone(pState, pState->m_pStream);

			pState->m_pStream = LTNULL;
		}

		pState->m_nChunks++;

		// The RATE is what is cleared, not the budget: the next chunk's
		// division then produces +INF and fails the test until something
		// else refills it. `fstps 0x5c`, one byte from the other reading.
		pState->m_fRate = 0.0f;
	}
}

// 0x1000e9c7 — the record for one file id, or null. Reconstructed, but in
// svnew.cpp's holding pen and against its own view classes, so it is
// declared here in this file's terms rather than moved (the same thing
// svnew.cpp already does in reverse for xfer_FreeAllFiles and
// xfer_FinishTransfer, which it declares against CClientFilesView while
// this file defines them).
CXferFile* xfer_FindFileByID(CClientFiles* pState, uint16 nFileID);

// 0x1000e941 — the client's half of the transfer protocol arriving back.
// servermgr.h called this msg_QueuePacket and typed it void, on nothing but
// the call site's position in smgr.cpp's "no handler" arm; the bytes say it
// is the xfer_* family's message handler and that it ANSWERS whether it took
// the packet (1) or not (0), which is what the dispatch loop's other
// handlers answer too.
//
// Two ids are understood:
//
//   * 0x36 is the client's reply list. Every word left in the payload is one
//     file id with a flag in its top bit: set means the client already has
//     the file and the record is dropped, clear means it still wants it and
//     the record goes back to pending. The word is read whole into a local
//     and masked twice — `and $0x7fff,%ax` for the lookup and
//     `test $0x8000,%di` for the flag — both sixteen-bit, which is what
//     types the local uint16.
//   * 0x38 restarts this frame's chunk budget, but only while a transfer is
//     actually running.
//
// Anything else is refused. The loop is the payload-remaining test
// xfer_SendPendingFiles uses at the other end, `m_Unknown34 - m_Unknown38`
// as an int subtraction of two zero-extended words (name ours).
//
// One result local set on all three arms (the refusal included, as an
// `else`) and returned once. With the arms returning 1 each, cl 8447 kept the
// LAST copy of the merged `return 1` epilogue; the single return makes it keep
// the first, so the 0x36 arm falls into it at 0x1000e9a7 and the 0x38 arm's
// two exits jump back to it, as shipped. Leaving the refusal as an early
// `return 0` with the local on the other two arms does not do it.
int xfer_HandleMessage(CClientFiles* pState, ServerMessage* pMsg)
{
	int bResult;
	uint8 nID = (uint8)(pMsg->m_pData[0] & 0x3f);

	if (nID == 0x36)
	{
		while (pMsg->m_Unknown34 - pMsg->m_Unknown38 > 0)
		{
			// CPacket::ReadType<uint16> itself: server.dll calls it
			// (0x1000ebd0), lithtech.exe expands it (0x00430080).
			uint16 nWord = ((CPacket*)pMsg)->ReadType((uint16*)0);

			CXferFile* pFile = xfer_FindFileByID(pState, nWord & 0x7fff);
			if (pFile)
			{
				if (nWord & 0x8000)
					xfer_FreeFile(pState, pFile);
				else
					pFile->m_Flags |= XFERFILE_PENDING;
			}
		}
		bResult = 1;
	}
	else if (nID == 0x38)
	{
		if (pState->m_State == 1)
			pState->m_nChunks = 0;

		bResult = 1;
	}
	else
	{
		bResult = 0;
	}

	return bResult;
}

// 0x1000e9ee — one frame of the transfer. servermgr.h called this
// msg_FlushPackets; it is the pump, and its two arms are the only things
// that ever move a transfer forward.
//
// Idle (m_State 0): find the first record still to be announced, clear the
// rate, and try to open it. The two gates in front of the open are read
// straight off the bytes — bit 2 of the state's flags stops the pump
// outright, and bit 1 restricts it to COUNTED records — and the rate is
// cleared BEFORE both of them, the store being unconditional while the
// tests are not. A successful open hands the stream over and moves to state
// 1; a refused one asks the failure hook, and only its answer of exactly 1
// drops the record. That drop is xfer_FreeFile's tail without the two
// counter decrements: dl_Remove inline, then servermgr.h's inline sb_Free
// on the same bank, the identical eleven instructions 0x1000e89b-0x1000e8b5
// carries.
//
// Running (m_State 1): add the frame's time to the rate and send a chunk.
// Any other state does nothing.
//
// The `1` the shipped code keeps in %ebx serves the two flag masks, the
// state store and the failure-hook compare; nothing in the source says so
// (name ours).
void xfer_Update(CClientFiles* pState, float fTime)
{
	if (!pState)
		return;

	if (pState->m_State == 0)
	{
		CXferFile* pFile = xfer_FindPendingFile(pState);
		if (!pFile)
			return;

		pState->m_fRate = 0.0f;

		if (pState->m_Flags & 2)
			return;

		if ((pState->m_Flags & 1) && !(pFile->m_Flags & XFERFILE_COUNTED))
			return;

		ILTStream* pStream = pState->m_pfnOpen(pState, pFile->m_pName);

		if (pStream)
		{
			pFile->m_Flags |= XFERFILE_SENDING;

			// m_nChunks last: that is lithtech.exe's store order
			// (0x43021b..0x430228); server.dll's /O1 hoists the zero store
			// (`andl $0`) to the front either way.
			pState->m_pCurFile   = pFile;
			pState->m_pStream    = pStream;
			pState->m_State      = 1;
			pState->m_nChunks    = 0;
			pState->m_nRemaining = pFile->m_nUnknown10;
		}
		else if (pState->m_pfnFail(pState, pFile->m_pName) == 1)
		{
			dl_Remove(&pFile->m_Link);

			sb_Free(&pState->m_FileBank.m_Bank, pFile);
		}
	}
	else if (pState->m_State == 1)
	{
		pState->m_fRate += fTime;

		xfer_SendNextChunk(pState);
	}
}


// 0x1003506e — put one object's identifier on a message, or 0xffff for none.
// Slot 0x04 of the helper's vftable; it never touches `this`.
//
// WHICH identifier depends on the message: Verify() answering 1 selects
// m_ObjectID2 at +0xba and anything else selects m_ObjectID at +0xb8, so the
// two fields are alternative ids for the same object and the message knows
// which side it is bound for. That is the only thing in the reconstruction
// that reaches +0xba, and it is what named it.
//
// One detail is forced. Each arm tests pObject POSITIVELY -- the shared 0xffff write is
// hosted after both arms, and writing the arms as `if (!pObject) ... else ...`
// hosts it at the first mention instead, two bytes short with the first branch
// inverted. Same fingerprint as the guarded-return run in 09-codegen-patterns:
// the shared block lands wherever the source first needs it.
//
// The null test really is inside both arms; VC6 merges the two 0xffff writes
// into one block, so there are three call sites in the source and two in the
// bytes (name ours).
LTRESULT CMessageObjectHelper::WriteObjectID(IMessageWrite* pMsg, LTObject* pObject)
{
	if (pMsg->Verify() == 1)
	{
		if (pObject)
			pMsg->WriteWord(pObject->m_ObjectID2);
		else
			pMsg->WriteWord(0xffff);
	}
	else
	{
		if (pObject)
			pMsg->WriteWord(pObject->m_ObjectID);
		else
			pMsg->WriteWord(0xffff);
	}

	return LT_OK;
}

// 0x1003511c — hand out the manager's embedded CServerWorld. Slot 0x0c.
LTRESULT CMessageObjectHelper::GetServerWorld(CServerWorld** ppWorld)
{
	*ppWorld = &m_pServerMgr->m_World;
	return LT_OK;
}

// 0x10034fcb — read an object id off a message and resolve it to the object.
// Slot 0x00, and WriteObjectID's exact inverse: Verify() answering 1 selects
// m_ObjectID2 and anything else selects m_ObjectID.
//
// The two halves resolve completely differently, which is the interesting
// part. The m_ObjectID path is a direct index into the manager's object map at
// 0x8fc, bounds-checked against m_nEntries and requiring the slot's flag byte
// to be exactly 1. The m_ObjectID2 path has no map: it sweeps ELEVEN object
// lists off the manager (see CServerMgr::ObjectLists) comparing every live
// object's m_ObjectID2, and it reaches them through the g_pServerMgr global
// rather than through its own m_pServerMgr -- reloading the global on every
// iteration, which is why the loop cannot be written against a hoisted local.
//
// The inner `break` is deliberate and matches the shipped control flow: a hit
// stores the object and leaves the inner loop only, so the sweep runs all
// eleven lists to the end and the LAST match wins.
LTRESULT CMessageObjectHelper::ReadObjectID(IMessageWrite* pMsg, LTObject** ppObject)
{
	uint16 nID;
	uint32 i;

	*ppObject = LTNULL;
	pMsg->ReadWord(&nID);

	if (pMsg->Verify() == 1)
	{
		if (nID == 0xffff)
			return LT_OK;

		for (i = 0; i < 11; i++)
		{
			LTLink* pHead = &g_pServerMgr->ObjectLists()[i].m_Head;
			LTLink* pCur;

			for (pCur = pHead->m_pNext; pCur != pHead; pCur = pCur->m_pNext)
			{
				LTObject* pObject = (LTObject*)pCur->m_pData;

				if (pObject->m_ObjectID2 == nID)
				{
					*ppObject = pObject;
					break;
				}
			}
		}
	}
	else
	{
		if (nID == 0xffff)
			return LT_OK;
		if (nID >= m_pServerMgr->m_Unknown8fc.m_nElements)
			return LT_OK;

		CObjectMapEntry* pEntry = &m_pServerMgr->m_Unknown8fc.m_pArray[nID];

		if (pEntry->m_nFlags != 1)
			return LT_OK;

		*ppObject = (LTObject*)pEntry->m_pRecord;
	}

	return LT_OK;
}

// 0x100350bc — put a formatted message-table string on a message, slot 0x08.
//
// Length-prefixed: a word for the length, then the bytes, then the raw goes
// back. An empty write (a bare zero word) is the failure arm, so the reader
// sees a zero-length string rather than a truncated message.
//
// The odd part is where the length comes from. There is no local for it --
// the frame has no `sub esp` at all. The pLen it hands str_FormatMessage is
// the address of its own pMarker PARAMETER, so the formatter writes the
// length over the argument slot, and both writes below then read that slot
// back as the length. pMarker is dead by then, which is what makes it legal;
// nothing but the shipped bytes would have suggested it.
LTRESULT CMessageObjectHelper::WriteFormattedString(IMessageWrite* pMsg,
                                            uint32 messageCode,
                                            va_list* pMarker)
{
	void* pRaw = str_FormatMessage(
		clm_GetRecordModule(m_pServerMgr->m_ClassMgr.m_pUnknown08),
		messageCode, pMarker, (uint32*)&pMarker);

	if (pRaw)
	{
		pMsg->WriteWord((uint16)(uint32)pMarker);
		pMsg->WriteRaw(pRaw, (uint32)pMarker);
		str_FreeMessage(pRaw);
	}
	else
	{
		pMsg->WriteWord(0);
	}

	return LT_OK;
}

// 0x10034fb9 — the constructor, and with it the vftable at 0x1004e900.
CMessageObjectHelper::CMessageObjectHelper(CServerMgr* pMgr)
{
	m_pServerMgr = pMgr;
}


// The 0x88-byte holder that 0x1000e507 allocates and Client::m_pUnknown18C
// points at, seen only through the one field these two touch. Its full
// layout is not modelled here (names ours).
class CPacketHolderView
{
public:
	uint8  m_pad_00[0x74];
	uint32 m_n74;                            // 0x74
};

// 0x1000e613 and 0x1000e622 — the field at +0x74, guarded both ways: a null
// holder reads as zero and swallows a write. Free functions, not members:
// both take the holder on the stack and neither uses %ecx, and 0x10027110 --
// one of the three function pointers the holder's descriptor carries --
// forwards straight to the first (names ours).
uint32 ph_GetUnknown74(CPacketHolderView* pHolder)
{
	if (pHolder)
		return pHolder->m_n74;

	return 0;
}

void ph_SetUnknown74(CPacketHolderView* pHolder, uint32 nValue)
{
	if (pHolder)
		pHolder->m_n74 = nValue;
}

// 0x10027110 / 0x00463cf0 -- the descriptor's first hook, which the frame
// pump calls to open a file for sending (CClientFiles::m_pfnOpen). The
// holder's +0x74 is the server manager it was made for; the file comes out of
// that manager's file manager at +0xa2c. __cdecl, two dwords: the shipped
// `add $0xc,%esp` cleans the one argument of ph_GetUnknown74 and the two of
// fs_OpenFile together (name smgr.cpp's).
void* ph_Forward74(void* pHolder, void* pArg)
{
	CServerMgr* pMgr = (CServerMgr*)ph_GetUnknown74((CPacketHolderView*)pHolder);

	return fs_OpenFile(&pMgr->m_FileMgr, (char*)pArg);
}
