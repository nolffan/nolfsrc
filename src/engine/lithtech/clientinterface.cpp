// *********************************************************************** //
//
//  MODULE:   CLIENTINTERFACE (.CPP)
//
//  PURPOSE:  The function that fills the client's interface table.
//
//  See clientinterface.h. It is a pure assignment sequence, which makes it
//  the cheapest large exact match in the image and the most informative: its
//  bytes give both the struct layout and 166 function addresses.
//
// *********************************************************************** //

#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <string.h>
#include <math.h>

#include "clientinterface.h"
#include "clientmgr.h"
#include "cluglob.h"                // g_CluClientGlob, 0x004b0ac0
#include "commonmodel.h"
#include "de_error.h"
#include "iclientshell.h"


// clientrender.cpp's renderer table (0x004b4358), through clientmgr.h's view.
extern RenderStruct g_RenderStruct;                          // 0x004b4358

// The FMConvertMgr the colour conversions go through (clientmgr.h).
#define CI_FMCONVERT() ((FMConvertMgrAPI*)cis_GetBlitterQ40C())


void ci_SetupInterface(CLTClient* pClient)
{
	pClient->StartGame                  = ci_StartGame;
	pClient->GetGameMode                = ci_GetGameMode;
	pClient->GetLocalClientID           = ci_GetLocalClientID;
	pClient->IsConnected                = ci_IsConnected;
	pClient->Disconnect                 = ci_Disconnect;
	pClient->Shutdown                   = ci_Shutdown;
	pClient->ShutdownWithMessage        = ci_ShutdownWithMessage;
	pClient->FlipScreen                 = ci_FlipScreen;
	pClient->ClearScreen                = ci_ClearScreen;
	pClient->Start3D                    = ci_Start3D;
	pClient->RenderCamera               = ci_RenderCamera;
	pClient->RenderObjects              = ci_RenderObjects;
	pClient->StartOptimized2D           = ci_StartOptimized2D;
	pClient->EndOptimized2D             = ci_EndOptimized2D;
	pClient->SetOptimized2DBlend        = ci_SetOptimized2DBlend;
	pClient->SetOptimized2DColor        = ci_SetOptimized2DColor;
	pClient->End3D                      = ci_End3D;
	pClient->GetRenderModes             = ci_GetRenderModes;
	pClient->RelinquishRenderModes      = ci_RelinquishRenderModes;
	pClient->SetRenderMode              = ci_SetRenderMode;
	pClient->GetRenderMode              = ci_GetRenderMode;
	pClient->ShutdownRender             = ci_ShutdownRender;
	pClient->GetFileList                = ci_GetFileList;
	pClient->FreeFileList               = ci_FreeFileList;
	pClient->GetWorldInfoString         = ci_GetWorldInfoString;
	pClient->ReadConfigFile             = ci_ReadConfigFile;
	pClient->WriteConfigFile            = ci_WriteConfigFile;
	pClient->GetAxisOffsets             = ci_GetAxisOffsets;
	pClient->PlayJoystickEffect         = ci_PlayJoystickEffect;
	pClient->InitMusic                  = ci_InitMusic;
	pClient->SetMusicDirectory          = ci_SetMusicDirectory;
	pClient->InitInstruments            = ci_InitInstruments;
	pClient->LoadSong                   = ci_LoadSong;
	pClient->DestroyAllSongs            = ci_DestroyAllSongs;
	pClient->PlayBreak                  = ci_PlayBreak;
	pClient->StopMusic                  = ci_StopMusic;
	pClient->PauseMusic                 = ci_PauseMusic;
	pClient->ResumeMusic                = ci_ResumeMusic;
	pClient->AddSongToPlayList          = ci_AddSongToPlayList;
	pClient->DeletePlayList             = ci_DeletePlayList;
	pClient->PlayList                   = ci_PlayList;
	pClient->PlayMotif                  = ci_PlayMotif;
	pClient->StopMotif                  = ci_StopMotif;
	pClient->GetMusicVolume             = ci_GetMusicVolume;
	pClient->SetMusicVolume             = ci_SetMusicVolume;
	pClient->InitSound                  = ci_InitSound;
	pClient->GetSound3DProviderLists    = ci_GetSound3DProviderLists;
	pClient->ReleaseSound3DProviderList = ci_ReleaseSound3DProviderList;
	pClient->GetSoundVolume             = ci_GetSoundVolume;
	pClient->SetSoundVolume             = ci_SetSoundVolume;
	pClient->SetReverbProperties        = ci_SetReverbProperties;
	pClient->GetReverbProperties        = ci_GetReverbProperties;
	pClient->PlaySound                  = ci_PlaySound;
	pClient->SetSoundPosition           = ci_SetSoundPosition;
	pClient->GetSoundPosition           = ci_GetSoundPosition;
	pClient->PauseSounds                = ci_PauseSounds;
	pClient->ResumeSounds               = ci_ResumeSounds;
	pClient->GetSoundDuration           = ci_GetSoundDuration;
	pClient->GetSoundTimer              = ci_GetSoundTimer;
	pClient->GetSoundData               = ci_GetSoundData;
	pClient->GetSoundOffset             = ci_GetSoundOffset;
	pClient->IsDone                     = ci_IsDone;
	pClient->KillSound                  = ci_KillSound;
	pClient->KillSoundLoop              = ci_KillSoundLoop;
	pClient->SetListener                = ci_SetListener;
	pClient->GetListener                = ci_GetListener;
	pClient->IntersectSegment           = ci_IntersectSegment;
	pClient->CastRay                    = ci_CastRay;
	pClient->FindObjectsInSphere        = ci_FindObjectsInSphere;
	pClient->RegisterConsoleProgram     = ci_RegisterConsoleProgram;
	pClient->UnregisterConsoleProgram   = ci_UnregisterConsoleProgram;
	pClient->GetConsoleVar              = ci_GetConsoleVar;
	pClient->StartCounter               = ci_StartCounter;
	pClient->EndCounter                 = ci_EndCounter;
	pClient->SetGlobalPanTexture        = ci_SetGlobalPanTexture;
	pClient->SetGlobalPanInfo           = ci_SetGlobalPanInfo;
	pClient->AddSurfaceEffect           = ci_AddSurfaceEffect;
	pClient->SetInputState              = ci_SetInputState;
	pClient->ClearInput                 = ci_ClearInput;
	pClient->GetDeviceBindings          = ci_GetDeviceBindings;
	pClient->FreeDeviceBindings         = ci_FreeDeviceBindings;
	pClient->StartDeviceTrack           = ci_StartDeviceTrack;
	pClient->TrackDevice                = ci_TrackDevice;
	pClient->EndDeviceTrack             = ci_EndDeviceTrack;
	pClient->GetDeviceObjects           = ci_GetDeviceObjects;
	pClient->FreeDeviceObjects          = ci_FreeDeviceObjects;
	pClient->GetDeviceName              = ci_GetDeviceName;
	pClient->IsDeviceEnabled            = ci_IsDeviceEnabled;
	pClient->EnableDevice               = ci_EnableDevice;
	pClient->GetGameTime                = ci_GetGameTime;
	pClient->GetGameFrameTime           = ci_GetGameFrameTime;
	pClient->DebugOut                   = ci_DebugOut;
	pClient->GetSkyDef                  = ci_GetSkyDef;
	pClient->IsCommandOn                = ci_IsCommandOn;
	pClient->RunConsoleString           = ci_RunConsoleString;
	pClient->GetClientObject            = ci_GetClientObject;
	pClient->GetGlobalLightScale        = ci_GetGlobalLightScale;
	pClient->SetGlobalLightScale        = ci_SetGlobalLightScale;
	pClient->OffsetGlobalLightScale     = ci_OffsetGlobalLightScale;
	pClient->CreateObject               = ci_CreateObject;
	pClient->GetObjectScale             = ci_GetObjectScale;
	pClient->SetObjectScale             = ci_SetObjectScale;
	pClient->GetObjectPos               = ci_GetObjectPos;
	pClient->GetObjectRotation          = ci_GetObjectRotation;
	pClient->SetObjectRotation          = ci_SetObjectRotation;
	pClient->SetObjectPosAndRotation    = ci_SetObjectPosAndRotation;
	pClient->GetObjectColor             = ci_GetObjectColor;
	pClient->SetObjectColor             = ci_SetObjectColor;
	pClient->GetObjectUserFlags         = ci_GetObjectUserFlags;
	pClient->SetObjectUserFlags         = ci_SetObjectUserFlags;
	pClient->GetObjectClientFlags       = ci_GetObjectClientFlags;
	pClient->SetObjectClientFlags       = ci_SetObjectClientFlags;
	pClient->GetObjectUserData          = ci_GetObjectUserData;
	pClient->SetObjectUserData          = ci_SetObjectUserData;
	pClient->Get3DCameraPt              = ci_Get3DCameraPt;
	pClient->GetCameraFOV               = ci_GetCameraFOV;
	pClient->SetCameraFOV               = ci_SetCameraFOV;
	pClient->GetCameraRect              = ci_GetCameraRect;
	pClient->SetCameraRect              = ci_SetCameraRect;
	pClient->GetCameraLightAdd          = ci_GetCameraLightAdd;
	pClient->SetCameraLightAdd          = ci_SetCameraLightAdd;
	pClient->SetupParticleSystem        = ci_SetupParticleSystem;
	pClient->SetSoftwarePSColor         = ci_SetSoftwarePSColor;
	pClient->AddParticles               = ci_AddParticles;
	pClient->GetParticles               = ci_GetParticles;
	pClient->GetParticleLifetime        = ci_GetParticleLifetime;
	pClient->GetParticleTotalLifetime   = ci_GetParticleTotalLifetime;
	pClient->GetParticlePos             = ci_GetParticlePos;
	pClient->SetParticlePos             = ci_SetParticlePos;
	pClient->RemoveParticle             = ci_RemoveParticle;
	pClient->OptimizeParticles          = ci_OptimizeParticles;
	pClient->GetNextLine                = ci_GetNextLine;
	pClient->GetLineInfo                = ci_GetLineInfo;
	pClient->SetLineInfo                = ci_SetLineInfo;
	pClient->AddLine                    = ci_AddLine;
	pClient->RemoveLine                 = ci_RemoveLine;
	pClient->SetupPolyGrid              = ci_SetupPolyGrid;
	pClient->SetPolyGridEnvMap          = ci_SetPolyGridEnvMap;
	pClient->SetPolyGridTexture         = ci_SetPolyGridTexture;
	pClient->GetPolyGridTextureInfo     = ci_GetPolyGridTextureInfo;
	pClient->SetPolyGridTextureInfo     = ci_SetPolyGridTextureInfo;
	pClient->GetPolyGridInfo            = ci_GetPolyGridInfo;
	pClient->FitPolyGrid                = ci_FitPolyGrid;
	pClient->GetLightColor              = ci_GetLightColor;
	pClient->SetLightColor              = ci_SetLightColor;
	pClient->GetLightRadius             = ci_GetLightRadius;
	pClient->SetLightRadius             = ci_SetLightRadius;
	pClient->ClipSprite                 = ci_ClipSprite;
	pClient->GetNextModelNode           = ci_GetNextModelNode;
	pClient->GetModelNodeName           = ci_GetModelNodeName;
	pClient->Parse                      = ci_Parse;
	pClient->GetSurfaceBounds           = ci_GetSurfaceBounds;
	pClient->SetPolyHideStatus          = ci_SetPolyHideStatus;
	pClient->GetPolyTextureFlags        = ci_GetPolyTextureFlags;
	pClient->SetModelHook               = ci_SetModelHook;
	pClient->InitNetworking             = ci_InitNetworking;
	pClient->GetServiceList             = ci_GetServiceList;
	pClient->FreeServiceList            = ci_FreeServiceList;
	pClient->SelectService              = ci_SelectService;
	pClient->GetSessionList             = ci_GetSessionList;
	pClient->FreeSessionList            = ci_FreeSessionList;
	pClient->AddInternetDriver          = ci_AddInternetDriver;
	pClient->RemoveInternetDriver       = ci_RemoveInternetDriver;
	pClient->GetLobbyLaunchInfo         = ci_GetLobbyLaunchInfo;
	pClient->IsLobbyLaunched            = ci_IsLobbyLaunched;
	pClient->GetTcpIpAddress            = ci_GetTcpIpAddress;

	ci_SetupInterface2(pClient);
}


// 0x0040c800 -- the surface module's init, reached only as the tail of
// ci_SetupInterface. HOME: src/engine/lithtech/cissurf.cpp (the cis_ run
// 0x0040c000..0x0040f2ff); written here because this file declares and
// calls it. The pointer stores are in the shipped STORE order (0x1a0 before
// 0x19c, 0x330 GetEngineHook between FillRect and GetSurfaceAlpha), which is
// statement order; the `push $0xa` / `push $0x2c` at 0x0040c806/0x0040c812
// are sb_Init's arguments hoisted above them by /O2. Then the screen
// surface's flags (0x004b1620 + 0x1c = 0x004b163c) are set to 1, the
// surface bank (0x004b1600) is initialised for 0x2c-byte surfaces, the
// 257-byte dimension-mask table at 0x004b164c (cissurf.cpp's
// g_DimMaskTableQ40C) is cleared and filled with (1<<i)-1 at 1<<i for
// i < 8 (`jb`: unsigned), and fntdraw.cpp's font init (0x00488980) installs
// the seven font slots.
struct CisStructBankQ19 { uint32 m_Data[7]; };

void sb_InitQ19(CisStructBankQ19* pBank, uint32 nStructSize, uint32 nCacheSize);  // 0x00490b50
void fnt_InitQ19(CLTClient* pClient);                                             // 0x00488980

// cissurf.cpp's surface bank and mask table, with that file's names and
// types (it owns the definitions).
class CStructBankQ40C;
extern CStructBankQ40C  g_SurfaceBankQ40C;                   // 0x004b1600
extern uint8            g_DimMaskTableQ40C[257];             // 0x004b164c

void ci_SetupInterface2(CLTClient* pClient)
{
	uint32 i;

	pClient->CreateColor                = ci_CreateColor;
	pClient->DeleteColor                = ci_DeleteColor;
	pClient->SetupColor1                = ci_SetupColor1;
	pClient->SetupColor2                = ci_SetupColor2;
	pClient->GetBorderSize              = ci_GetBorderSize;
	pClient->OptimizeSurface            = ci_OptimizeSurface;
	pClient->UnoptimizeSurface          = ci_UnoptimizeSurface;
	pClient->GetScreenSurface           = ci_GetScreenSurface;
	pClient->CreateSurfaceFromBitmap    = ci_CreateSurfaceFromBitmap;
	pClient->CreateSurface              = ci_CreateSurface;
	pClient->DeleteSurface              = ci_DeleteSurface;
	pClient->GetSurfaceUserData         = ci_GetSurfaceUserData;
	pClient->SetSurfaceUserData         = ci_SetSurfaceUserData;
	pClient->GetPixel                   = ci_GetPixel;
	pClient->SetPixel                   = ci_SetPixel;
	pClient->GetSurfaceDims             = ci_GetSurfaceDims;
	pClient->DrawBitmapToSurface        = ci_DrawBitmapToSurface;
	pClient->DrawSurfaceSolidColor      = ci_DrawSurfaceSolidColor;
	pClient->DrawSurfaceMasked          = ci_DrawSurfaceMasked;
	pClient->DrawSurfaceToSurface       = ci_DrawSurfaceToSurface;
	pClient->DrawSurfaceToSurfaceTransparent = ci_DrawSurfaceToSurfaceTransparent;
	pClient->ScaleSurfaceToSurface      = ci_ScaleSurfaceToSurface;
	pClient->ScaleSurfaceToSurfaceTransparent = ci_ScaleSurfaceToSurfaceTransparent;
	pClient->ScaleSurfaceToSurfaceSolidColor = ci_ScaleSurfaceToSurfaceSolidColor;
	pClient->WarpSurfaceToSurface       = ci_WarpSurfaceToSurface;
	pClient->WarpSurfaceToSurfaceTransparent = ci_WarpSurfaceToSurfaceTransparent;
	pClient->WarpSurfaceToSurfaceSolidColor = ci_WarpSurfaceToSurfaceSolidColor;
	pClient->TransformSurfaceToSurface  = ci_TransformSurfaceToSurface;
	pClient->TransformSurfaceToSurfaceTransparent = ci_TransformSurfaceToSurfaceTransparent;
	pClient->FillRect                   = ci_FillRect;
	pClient->GetEngineHook              = ci_GetEngineHook;
	pClient->GetSurfaceAlpha            = ci_GetSurfaceAlpha;
	pClient->SetSurfaceAlpha            = ci_SetSurfaceAlpha;

	((CSurface*)&g_ScreenSurface)->m_nFlags = 1;

	sb_InitQ19((CisStructBankQ19*)&g_SurfaceBankQ40C, 0x2c, 0xa);

	memset(g_DimMaskTableQ40C, 0, sizeof(g_DimMaskTableQ40C));

	for (i = 0; i < 8; i++)
		g_DimMaskTableQ40C[1 << i] = (uint8)((1 << i) - 1);

	fnt_InitQ19(pClient);
}


// ---- the implementations ------------------------------------------------ //

// Whether there is a server on the other end. The connection object is the
// only thing that says so, which is what named it.
LTBOOL ci_IsConnected()
{
	return g_pClientMgr->m_pConnection != NULL;
}


// Every message is formatted into a 500-byte frame buffer first, which is
// why the vsnprintf limit is one less.
void ci_ShutdownWithMessage(char* pMsg, ...)
{
	char    szBuf[500];
	va_list marker;

	va_start(marker, pMsg);
	_vsnprintf(szBuf, sizeof(szBuf) - 1, pMsg, marker);

	sub_42e6a0(szBuf);
}


void ci_Shutdown()
{
	ci_ShutdownWithMessage(g_ShutdownMessage);
}


// A colour handle is a value, not an allocation, so there is nothing to free.
void ci_DeleteColor(HLTCOLOR hColor)
{
}


ILTClientSoundMgr* ci_GetSoundMgr()
{
	return (ILTClientSoundMgr*)&g_pClientMgr->m_SoundMgr;
}


void ci_PauseSounds()
{
	ci_GetSoundMgr()->PauseSounds();
}


void ci_ResumeSounds()
{
	ci_GetSoundMgr()->ResumeSounds();
}


// CClientMgr::ClearInput (0x00410900) is defined in clientwin.cpp on that
// file's view of the manager; called through the same view so it links.
class CluClientMgrView
{
public:
	LTRESULT ClearInput();                                   // 0x00410900
};

LTRESULT ci_ClearInput()
{
	return ((CluClientMgrView*)g_pClientMgr)->ClearInput();
}


HSURFACE ci_GetScreenSurface()
{
	return g_pRender ? (HSURFACE)&g_ScreenSurface : LTNULL;
}


// NOT ci_GetSurfaceUserData: that is cissurf.cpp's 0x0040cfb0 (the one
// ci_SetupInterface2 installs, `je` to `xor %eax,%eax`).  This body is the
// shipped 0x0042f7c0 (`jne` over a bare `ret` that hands the null back), the
// message-record queue's user-pointer getter at +0x24 that clientwin.cpp
// calls clu_sub_42f7c0 -- so it carries that name.
void* clu_sub_42f7c0(void* pQueue)
{
	if (!pQueue)
		return LTNULL;

	return ((CSurface*)pQueue)->m_pUserData;
}


// 0x0042f7d0 -- 16 bytes.  The getter's partner: the queue's user-pointer
// setter at +0x24 (clientwin.cpp's clu_sub_42f7d0).  Same bytes as
// ci_SetSurfaceUserData at 0x0040cfc0, but the shipped image keeps both
// copies, so it is its own function (it was an alias onto clientshell.cpp's
// pool_FreeArray50 stand-in, which would have freed the queue).
void clu_sub_42f7d0(void* pQueue, void* pUser)
{
	if (!pQueue)
		return;

	((CSurface*)pQueue)->m_pUserData = pUser;
}


void ci_SetSurfaceUserData(HSURFACE hSurface, void* pUserData)
{
	if (!hSurface)
		return;

	((CSurface*)hSurface)->m_pUserData = pUserData;
}


// GAMEMODE_NONE is 5 (ltbasedefs.h) — with no connection there is no mode,
// and the call still succeeds.
LTRESULT ci_GetGameMode(int* mode)
{
	CClientConnection* pConn = g_pClientMgr->m_pConnection;

	if (pConn)
		*mode = pConn->m_nGameMode;
	else
		*mode = GAMEMODE_NONE;

	return LT_OK;
}


LTRESULT ci_GetServiceList(NetService*& pListHead)
{
	return g_pClientMgr->m_NetMgr.GetServiceList(pListHead);
}


// The config file is console commands, so reading one is running one. Both
// of these invert a LTBOOL into an LTRESULT — `neg`/`sbb`/`inc` in the
// shipped code, which is (x == 0), and LT_OK is 0 while LT_ERROR is 1.
LTRESULT ci_ReadConfigFile(char* pFilename)
{
	return con_RunFile(&g_ClientConsoleState, pFilename, 0, 1) ? LT_OK : LT_ERROR;
}


LTRESULT ci_WriteConfigFile(char* pFilename)
{
	return con_SaveFile(&g_ClientConsoleState, pFilename) ? LT_OK : LT_ERROR;
}


LTRESULT ci_GetSessionList(NetSession*& pListHead, char* pInfo)
{
	return g_pClientMgr->m_NetMgr.GetSessionList(pListHead, pInfo);
}


LTRESULT ci_FreeSessionList(NetSession* pListHead)
{
	return g_pClientMgr->m_NetMgr.FreeSessionList(pListHead);
}


LTRESULT ci_StartGame(StartGameRequest* pRequest)
{
	return g_pClientMgr->StartGame(pRequest);
}


// The client keeps its file manager behind a pointer; the server has one
// inline. Neither checks the directory name here.
//
// The callee is NOT the server's fs_GetFileList, though this function is
// byte-identical either way -- the call's four bytes are masked. The
// shipped call resolves to 0x00404610,
// where our fs_GetFileList sits at 0x0046b970.
//
// 0x00404610 -- 98 bytes. HOME: src/engine/lithtech/clientwin.cpp, the
// client file manager run (0x00403fd0..0x0040488d; it sits between
// clu_fm_AddTrees 0x00404520 and clu_fm_GetFileRec 0x00404680). Written
// here because this file is its caller; the views are local and suffixed. It null-tests the manager,
// gathers up to 40 open trees (the 0xa0-byte frame) off m_TreeList
// (+0x04 m_pNext, node = link->m_pData at +0x08, tree at +0x0c), and hands
// them WITH the directory name to clu_fs_BuildFileList (0x00436520): the
// name is read at 0x00404653 as 0xb0(%esp), which is the second argument
// once %esi and %edi are both pushed. (The earlier comment here said the
// name was never read; that counted from the wrong push depth.)
struct CluLinkQ19
{
	CluLinkQ19* m_pPrev;                     // 0x00
	CluLinkQ19* m_pNext;                     // 0x04
	void*       m_pData;                     // 0x08
};

struct CluTreeNodeQ19
{
	CluLinkQ19  m_Link;                      // 0x00
	void*       m_pTree;                     // 0x0c
};

struct CluFileMgrQ19
{
	CluLinkQ19  m_TreeList;                  // 0x00
};

FileEntry* clu_fs_BuildFileListQ19(void** pTrees, int nTrees, char* pDirName);  // 0x00436520

FileEntry* sub_404610(CFileMgr* pFileMgr, char* pDirName)
{
	CluFileMgrQ19* pMgr = (CluFileMgrQ19*)pFileMgr;
	void*          pTrees[40];
	CluLinkQ19*    pCur;
	int            nTrees;

	if (!pMgr)
		return LTNULL;

	nTrees = 0;
	for (pCur = pMgr->m_TreeList.m_pNext; pCur != &pMgr->m_TreeList; pCur = pCur->m_pNext)
	{
		pTrees[nTrees] = ((CluTreeNodeQ19*)pCur->m_pData)->m_pTree;
		nTrees++;

		if (nTrees >= 40)
			break;
	}

	return clu_fs_BuildFileListQ19(pTrees, nTrees, pDirName);
}

FileEntry* ci_GetFileList(char* pDirName)
{
	return sub_404610(g_pClientMgr->m_pFileMgr, pDirName);
}


LTBOOL ci_InitMusic(char* szMusicDLL)
{
	return !g_pClientMgr->InitMusic(szMusicDLL);
}


// NOT COMPILED: server/sharedcommon.cpp's ci_StartCounter is the one
// lithtech.exe has (0x00435720, the shared-code run); same body.
#if 0
void ci_StartCounter(LTCounter* pCounter)
{
	cnt_StartCounter((CBaseCounter*)pCounter);
}
#endif


// ---- the sound manager ---------------------------------------------------- //
//
// Everything here is one forwarding call through the sub-object at
// g_pClientMgr+0x774, and the vtable slots map straight onto iltsoundmgr.h's
// declaration order: 0x14 is its sixth virtual (GetSound3DProviderLists) and
// 0x5c its twenty-fourth (PauseSounds), counted across ILTSoundMgr and the
// ILTClientSoundMgr that continues its vtable. Where the table's signature
// takes a pointer and the interface a reference, the wrapper dereferences;
// where the table returns a value and the interface fills an out-parameter,
// the wrapper keeps the local.

LTRESULT ci_GetSound3DProviderLists(Sound3DProvider*& pSound3DProviderList, LTBOOL bVerify)
{
	return ci_GetSoundMgr()->GetSound3DProviderLists(pSound3DProviderList, bVerify);
}


void ci_ReleaseSound3DProviderList(Sound3DProvider* pSound3DProviderList)
{
	ci_GetSoundMgr()->ReleaseSound3DProviderList(pSound3DProviderList);
}


LTRESULT ci_InitSound(InitSoundInfo* pSoundInfo)
{
	return ci_GetSoundMgr()->InitSound(pSoundInfo);
}


unsigned short ci_GetSoundVolume()
{
	uint16 nVolume;

	ci_GetSoundMgr()->GetVolume(nVolume);

	return nVolume;
}


void ci_SetSoundVolume(unsigned short nVolume)
{
	ci_GetSoundMgr()->SetVolume(nVolume);
}


LTRESULT ci_SetReverbProperties(ReverbProperties* pReverbProperties)
{
	return ci_GetSoundMgr()->SetReverbProperties(pReverbProperties);
}


LTRESULT ci_GetReverbProperties(ReverbProperties* pReverbProperties)
{
	return ci_GetSoundMgr()->GetReverbProperties(pReverbProperties);
}


LTRESULT ci_SetSoundPosition(HLTSOUND hSound, LTVector* pPos)
{
	return ci_GetSoundMgr()->SetSoundPosition(hSound, pPos);
}


LTRESULT ci_GetSoundPosition(HLTSOUND hSound, LTVector* pPos)
{
	return ci_GetSoundMgr()->GetSoundPosition(hSound, pPos);
}


LTRESULT ci_GetSoundDuration(HLTSOUND hSound, LTFLOAT* fDuration)
{
	return ci_GetSoundMgr()->GetSoundDuration(hSound, *fDuration);
}


LTBOOL ci_IsDone(HLTSOUND pSoundHandle)
{
	LTBOOL bDone;

	ci_GetSoundMgr()->IsSoundDone(pSoundHandle, bDone);

	return bDone;
}


void ci_KillSound(HLTSOUND pSoundHandle)
{
	ci_GetSoundMgr()->KillSound(pSoundHandle);
}


void ci_KillSoundLoop(HLTSOUND hSoundHandle)
{
	ci_GetSoundMgr()->KillSoundLoop(hSoundHandle);
}


// ---- the input table ------------------------------------------------------ //
//
// Every one of these tolerates a missing input table by answering LT_ERROR,
// which is 1, or a null pointer where the return type is one — and the null
// case returns the tested value itself, so the shipped code has no separate
// `xor eax,eax`.

void ci_SetInputState(LTBOOL bOn)
{
	g_pClientMgr->m_bInputEnabled = bOn;
}


DeviceBinding* ci_GetDeviceBindings(uint32 nDevice)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return LTNULL;

	return pInput->GetDeviceBindings(nDevice);
}


void ci_FreeDeviceBindings(DeviceBinding* pBindings)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return;

	pInput->FreeDeviceBindings(pBindings);
}


// The tracking flag goes up before the call, not after it.
LTRESULT ci_StartDeviceTrack(uint32 nDevices, uint32 nBufferSize)
{
	if (!g_pClientMgr->m_pInput)
		return LT_ERROR;

	g_pClientMgr->m_bTrackingDevices = LTTRUE;

	return !g_pClientMgr->m_pInput->StartDeviceTrack(g_pClientMgr->m_pInput,
	                                                 nDevices, nBufferSize);
}


LTRESULT ci_TrackDevice(DeviceInput* pInputArray, uint32* pnInOut)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return LT_ERROR;

	if (!g_pClientMgr->m_bTrackingDevices)
		return LT_ERROR;

	return !pInput->TrackDevice(pInputArray, pnInOut);
}


LTRESULT ci_EndDeviceTrack()
{
	if (!g_pClientMgr->m_pInput)
		return LT_ERROR;

	g_pClientMgr->m_bTrackingDevices = LTFALSE;

	return !g_pClientMgr->m_pInput->EndDeviceTrack();
}


DeviceObject* ci_GetDeviceObjects(uint32 nDeviceFlags)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return LTNULL;

	return pInput->GetDeviceObjects(nDeviceFlags);
}


void ci_FreeDeviceObjects(DeviceObject* pList)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return;

	pInput->FreeDeviceObjects(pList);
}


LTRESULT ci_GetDeviceName(uint32 nDeviceType, char* pStrBuffer, uint32 nBufferSize)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return LT_ERROR;

	// LT_NOTFOUND, not LT_ERROR: the name is what was missing.
	return pInput->GetDeviceName(nDeviceType, pStrBuffer, nBufferSize)
	           ? LT_OK : LT_NOTFOUND;
}


LTRESULT ci_IsDeviceEnabled(char* strDeviceName, LTBOOL* pIsEnabled)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return LT_ERROR;

	*pIsEnabled = pInput->IsDeviceEnabled(strDeviceName);

	return LT_OK;
}


LTRESULT ci_EnableDevice(char* strDeviceName)
{
	CClientInput* pInput = g_pClientMgr->m_pInput;

	if (!pInput)
		return LT_ERROR;

	return !pInput->EnableDevice(pInput, strDeviceName);
}


// ---- the music table ------------------------------------------------------ //
//
// Every entry refuses when no music DLL is loaded, and every one of them asks
// for the manager again rather than keeping it: the shipped code calls the
// getter once per use, which is what says the source did too.

CClientMusicMgr* ci_GetMusicMgr()
{
	return (CClientMusicMgr*)&g_pClientMgr->m_MusicMgr;
}


void ci_DestroyAllSongs()
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return;

	ci_GetMusicMgr()->DestroyAllSongs();
}


void ci_StopMusic(uint32 dwBoundaryFlags)
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return;

	ci_GetMusicMgr()->Stop(dwBoundaryFlags);
}


LTBOOL ci_PauseMusic()
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return LTFALSE;

	return ci_GetMusicMgr()->Pause(0);
}


LTBOOL ci_ResumeMusic()
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return LTFALSE;

	return ci_GetMusicMgr()->UnPause();
}


// ---- the connection, the world and one object accessor -------------------- //

// 0x0041fbe0 is clientmgr3.cpp's cmg_AddSurfaceEffect, declared with that
// file's names so the call links to it.
class  CCmgClientMgr;
struct CCmgSurfaceEffectDesc;
LTRESULT cmg_AddSurfaceEffect(CCmgClientMgr* pClientMgr, CCmgSurfaceEffectDesc* pDesc); // 0x0041fbe0

LTRESULT ci_AddSurfaceEffect(SurfaceEffectDesc* pDesc)
{
	return cmg_AddSurfaceEffect((CCmgClientMgr*)g_pClientMgr, (CCmgSurfaceEffectDesc*)pDesc);
}


// Both clocks live on the connection, so with no server there is no time.
float ci_GetGameTime()
{
	CClientConnection* pConn = g_pClientMgr->m_pConnection;

	if (pConn)
		return pConn->m_fGameTime;

	return 0.0f;
}


float ci_GetGameFrameTime()
{
	CClientConnection* pConn = g_pClientMgr->m_pConnection;

	if (pConn)
		return pConn->m_fFrameTime;

	return 0.0f;
}


// The sky box is handed out by value — forty-eight bytes copied whole.
LTRESULT ci_GetSkyDef(SkyDef* pDef)
{
	*pDef = g_pClientMgr->m_SkyDef;

	return LT_OK;
}


// 0x0040a920, 12 bytes -- ccclient.cpp's console-command runner.
void cc_RunConsoleString(char* pCommand);                       // 0x0041e930

void ci_RunConsoleString(char* pString)
{
	cc_RunConsoleString(pString);
}


HLOCALOBJ ci_GetClientObject()
{
	if (g_pClientMgr->m_pConnection)
		return sub_47bcd0();

	return LTNULL;
}


void ci_GetGlobalLightScale(LTVector* pScale)
{
	*pScale = g_pClientMgr->m_vGlobalLightScale;
}


// The flags are a uint16 on the object and a uint32 to the caller.
uint32 ci_GetObjectClientFlags(HLOCALOBJ hObj)
{
	if (hObj)
		return ((CClientObject*)hObj)->m_ClientFlags;

	return 0;
}


void ci_SetObjectClientFlags(HLOCALOBJ hObj, uint32 flags)
{
	if (hObj)
		((CClientObject*)hObj)->m_ClientFlags = (uint16)flags;
}


void* ci_GetObjectUserData(HLOCALOBJ hObj)
{
	if (hObj)
		return ((CClientObject*)hObj)->m_pUserData;

	return LTNULL;
}


void ci_SetObjectUserData(HLOCALOBJ hObj, void* pData)
{
	if (hObj)
		((CClientObject*)hObj)->m_pUserData = pData;
}


void ci_GetLightColor(HLOCALOBJ hObj, float* r, float* g, float* b)
{
	sub_40ad40(hObj, r, g, b, 0);
}


// 0x0040bef0 -- the same body as smgr.cpp's si_SetLightColor (0x00472ab0,
// byte-identical): a light keeps its colour in the object's colour bytes, so
// the unused alpha byte (+0x97) is scaled back out and passed through to
// ci_SetObjectColor (0x0040add0).
void ci_SetLightColor(HLOCALOBJ hObj, float r, float g, float b)
{
	if (!hObj)
		return;

	ci_SetObjectColor(hObj, r, g, b, ((CClientObject*)hObj)->m_ColorA * (1.0f / 255.0f));
}


// The two colour setups are the same call with the same arguments; the
// interface offers both names and the engine implements them once each.
// The callee, 0x0040c9b0, is ci_CreateColor below.
HLTCOLOR ci_SetupColor1(float r, float g, float b, LTBOOL bTransparent)
{
	return ci_CreateColor(r, g, b, bTransparent);
}


HLTCOLOR ci_SetupColor2(float r, float g, float b, LTBOOL bTransparent)
{
	return ci_CreateColor(r, g, b, bTransparent);
}


// The mode list is singly linked through +0x210. The shipped loop is a
// do/while under a guard, not a while.
void ci_RelinquishRenderModes(RMode* pModes)
{
	if (pModes)
	{
		do
		{
			RMode* pNext = pModes->m_pNext;

			dfree(pModes);

			pModes = pNext;
		}
		while (pModes);
	}
}


LTRESULT ci_GetRenderMode(RMode* pMode)
{
	*pMode = g_CurRenderMode;

	return LT_OK;
}


HCONSOLEVAR ci_GetConsoleVar(char* pName)
{
	return (HCONSOLEVAR)con_FindVar(&g_ClientConsoleState, pName);
}


// Two guards, both positive, with the one failure answer at the end: the
// manager itself may not exist yet, and without a connection there is no
// world to intersect.
LTBOOL ci_IntersectSegment(ClientIntersectQuery* pQuery, ClientIntersectInfo* pInfo)
{
	if (g_pClientMgr && g_pClientMgr->m_pConnection)
		return sub_4305f0(pQuery, pInfo, g_pClientMgr->m_World, 0);

	return LTFALSE;
}


// The shipped code reads the global again between the two stores, so the
// source names it twice rather than keeping it.
LTRESULT ci_SetModelHook(ModelHookFn fn, void* pUser)
{
	g_pClientMgr->m_ModelHookFn = fn;
	g_pClientMgr->m_pModelHookUser = pUser;

	return LT_OK;
}


// Disconnecting drops the manager's pointer to the connection, so a second
// call does nothing.
void ci_Disconnect()
{
	if (!g_pClientMgr->m_pConnection)
		return;

	g_pClientMgr->m_pConnection->Disconnect(1);

	g_pClientMgr->m_pConnection = LTNULL;
}


void ci_GetAxisOffsets(LTFLOAT* offsets)
{
	offsets[0] = g_pClientMgr->m_fAxisOffsets[0];
	offsets[1] = g_pClientMgr->m_fAxisOffsets[1];
	offsets[2] = g_pClientMgr->m_fAxisOffsets[2];
}


// The only input entry that does not check for a missing table first.
void ci_PlayJoystickEffect(char* pEffectName, float x, float y)
{
	g_pClientMgr->m_pInput->PlayJoystickEffect(g_pClientMgr->m_pInput,
	                                           pEffectName, x, y);
}


// Changing directory stops whatever is playing first.
LTBOOL ci_SetMusicDirectory(char* szMusicDirectory)
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return LTFALSE;

	ci_GetMusicMgr()->Stop(0);

	return ci_GetMusicMgr()->SetDirectory(szMusicDirectory);
}


// Bit 4 of the manager's flags is what says motifs are available.
void ci_StopMotif(char* szMotif)
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return;

	if (!(ci_GetMusicMgr()->m_nFlags & 4))
		return;

	ci_GetMusicMgr()->StopMotif(szMotif);
}


// Bit 1 of the music flags is the one that gates the volume pair.
short ci_GetMusicVolume()
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return 0;

	if (!(ci_GetMusicMgr()->m_nFlags & 1))
		return 0;

	return ci_GetMusicMgr()->GetVolume();
}


void ci_SetMusicVolume(short wVolume)
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return;

	if (!(ci_GetMusicMgr()->m_nFlags & 1))
		return;

	ci_GetMusicMgr()->SetVolume(wVolume);
}


void ci_SetObjectPosAndRotation(HLOCALOBJ hObj, LTVector* pPos, LTRotation* pRotation)
{
	if (hObj && pPos && pRotation)
		sub_420930(g_pClientMgr, hObj, pPos, pRotation);
}


void ci_GetObjectPos(HLOCALOBJ hObj, LTVector* pPos)
{
	if (hObj && pPos)
		*pPos = ((CClientObject*)hObj)->m_Pos;
}


void ci_SetObjectRotation(HLOCALOBJ hObj, LTRotation* pRotation)
{
	if (hObj && pRotation)
		sub_420880(g_pClientMgr, hObj, pRotation);
}


// The system handle is not checked — only the particle and the destination.
void ci_GetParticlePos(HLOCALOBJ hSystem, LTParticle* pParticle, LTVector* pPos)
{
	if (pParticle && pPos)
		*pPos = ((CClientParticle*)pParticle)->m_Pos;
}


// Only a poly-grid object can be set up as one, and only a sprite can be
// clipped: both check the object's type byte at +0xbc against OT_.
LTBOOL ci_SetupPolyGrid(HLOCALOBJ hObj, uint32 width, uint32 height, LTBOOL bHalfTrianges)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_POLYGRID)
		return sub_460bf0(hObj, width, height, bHalfTrianges);

	return LTFALSE;
}


LTRESULT ci_ClipSprite(HLOCALOBJ hObj, HPOLY hPoly)
{
	if (!hObj)
		return LT_ERROR;

	if (((CClientObject*)hObj)->m_ObjectType != OT_SPRITE)
		return LT_ERROR;

	((CClientObject*)hObj)->m_hClipPoly = hPoly;

	return LT_OK;
}


// NOT COMPILED: server/sharedcommon.cpp's ci_EndCounter is the one
// lithtech.exe has (0x00435730); same body.
#if 0
uint32 ci_EndCounter(LTCounter* pCounter)
{
	return cnt_EndCounter((CBaseCounter*)pCounter);
}
#endif


// Bringing networking up drops any connection first, then hands the net
// manager its own initialisation — the driver name and flags the interface
// takes are not passed on.
LTRESULT ci_InitNetworking(char* pDriver, uint32 dwFlags)
{
	if (g_pClientMgr->m_pConnection)
	{
		g_pClientMgr->m_pConnection->Disconnect(1);

		g_pClientMgr->m_pConnection = LTNULL;
	}

	g_pClientMgr->m_NetMgr.Init();

	return LT_OK;
}


LTRESULT ci_AddInternetDriver()
{
	if (g_pClientMgr->m_NetMgr.FindDriver("internet"))
		return LT_ALREADYEXISTS;

	return !g_pClientMgr->m_NetMgr.AddDriver("internet");
}


LTRESULT ci_RemoveInternetDriver()
{
	CBaseDriver* pDriver = g_pClientMgr->m_NetMgr.FindDriver("internet");

	if (pDriver)
	{
		g_pClientMgr->m_NetMgr.RemoveDriver(pDriver);

		return LT_OK;
	}

	return LT_ERROR;
}

// The local client's ID is the connection's, and 0xffff is the "not assigned
// yet" value the server sends before it has picked one, so both the missing
// connection and the placeholder answer LT_NOTCONNECTED.
LTRESULT ci_GetLocalClientID(uint32* pID)
{
	*pID = 0;

	CClientConnection* pConn = g_pClientMgr->m_pConnection;

	if (pConn && pConn->m_nLocalID != 0xffff)
	{
		*pID = pConn->m_nLocalID;
		return LT_OK;
	}

	return LT_NOTCONNECTED;
}

LTRESULT ci_FlipScreen(uint32 flags)
{
	if (!g_RenderStruct.m_bInited)
		RETURN_ERROR(1, "FlipScreen", LT_NOTINITIALIZED);

	g_pClientMgr->FlipScreen(flags);
	return LT_OK;
}

// A null rect means the whole screen. Either way the rect handed to the
// renderer is a local copy clamped to the screen, so a caller cannot make it
// clear outside the surface.
LTRESULT ci_ClearScreen(LTRect* pClearRect, uint32 flags)
{
	LTRect rect;

	if (!g_RenderStruct.m_bInited)
		RETURN_ERROR(1, "ClearScreen", LT_NOTINITIALIZED);

	if (pClearRect)
	{
		rect = *pClearRect;

		if (rect.left < 0)
			rect.left = 0;
		else if (rect.left > g_RenderStruct.m_Width)
			rect.left = g_RenderStruct.m_Width;

		if (rect.right < 0)
			rect.right = 0;
		else if (rect.right > g_RenderStruct.m_Width)
			rect.right = g_RenderStruct.m_Width;

		if (rect.top < 0)
			rect.top = 0;
		else if (rect.top > g_RenderStruct.m_Height)
			rect.top = g_RenderStruct.m_Height;

		if (rect.bottom < 0)
			rect.bottom = 0;
		else if (rect.bottom > g_RenderStruct.m_Height)
			rect.bottom = g_RenderStruct.m_Height;
	}
	else
	{
		rect.left = rect.top = 0;
		rect.right = g_RenderStruct.m_Width;
		rect.bottom = g_RenderStruct.m_Height;
	}

	if (g_CV_ForceClear)
		flags |= CLEARSCREEN_SCREEN;

	g_RenderStruct.ClearScreen(&rect, flags);
	return LT_OK;
}

LTBOOL ci_InitInstruments(char* szDLSFile, char* szStyleFile)
{
	if (ci_GetMusicMgr()->m_pMusicDLL)
	{
		ci_GetMusicMgr()->Stop(0);

		if (ci_GetMusicMgr()->m_nFlags & 2)
			return ci_GetMusicMgr()->InitInstruments(szDLSFile, szStyleFile);
	}

	return LTFALSE;
}

// Two ways in, tried in turn; either one working is a success.
LTBOOL ci_LoadSong(char* szSong)
{
	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return LTFALSE;

	if (ci_GetMusicMgr()->Fn_14(szSong) || ci_GetMusicMgr()->Fn_10(szSong))
		return LTTRUE;

	return LTFALSE;
}

// Every music entry refuses without the DLL, and every one that names
// something refuses a null name too; the pair is one condition, which is why
// both tests jump to the same exit.
void ci_DeletePlayList(char* szPlayList)
{
	if (ci_GetMusicMgr()->m_pMusicDLL && szPlayList)
	{
		void* pPlayList = ci_GetMusicMgr()->Fn_24(szPlayList);

		if (pPlayList)
			ci_GetMusicMgr()->Fn_30(pPlayList);
	}
}

LTBOOL ci_PlayMotif(char* szMotif, LTBOOL bLoop)
{
	if (ci_GetMusicMgr()->m_pMusicDLL && szMotif && (ci_GetMusicMgr()->m_nFlags & 4))
		return ci_GetMusicMgr()->PlayMotif(szMotif, bLoop);

	return LTFALSE;
}

// The handle the sound manager fills in is thrown away here - the caller gets
// its copy through pPlaySoundInfo - but the game DLL is told, which is what
// IClientShell's twenty-fourth slot is for.
LTRESULT ci_PlaySound(PlaySoundInfo* pPlaySoundInfo)
{
	HLTSOUND hSound;
	LTRESULT dResult;

	dResult = ci_GetSoundMgr()->PlaySound(pPlaySoundInfo, hSound);

	if (dResult == LT_OK)
		g_pClientMgr->m_pClientShell->OnPlaySound(pPlaySoundInfo);

	return dResult;
}

LTBOOL ci_IsCommandOn(int commandNum)
{
	if (commandNum >= MAX_COMMANDS)
		return LTFALSE;

	return g_pClientMgr->m_CommandOn[g_pClientMgr->m_nCurCommandSet][commandNum] != 0;
}

void ci_GetObjectRotation(HLOCALOBJ hObj, LTRotation* pRotation)
{
	if (hObj && pRotation)
		*pRotation = ((CClientObject*)hObj)->m_Rotation;
}

// 0x0040b040, 656 bytes -- ILTClient::Get3DCameraPt (the literal
// "Get3DCameraPt" at 0x004a7f28 is both reports' name).  Maps a screen point
// inside the camera's rect onto the view plane one unit in front of it:
//   * both guards and pOut jump to the LT_INVALIDPARAMS report hosted LAST
//     (0x0040b291), the rect test to the LT_OUTSIDE (65) one before it.
//   * the half extents are `sar $1` -- a shift, not a signed divide.
//   * each ratio is divided by 1/(1/tan(PI/2 - fov/2)): two `fdivrs 1.0`
//     back to back (0x0040b131, 0x0040b16f); y is negated last (fchs).
//   * 0x00444ac0 is quat_GetVectors(&m_Rotation, &right, &up, &forward).
void quat_GetVectors(LTRotation* pRot, LTVector* pRight, LTVector* pUp,
                     LTVector* pForward);

LTRESULT ci_Get3DCameraPt(HLOCALOBJ hCamera, int sx, int sy, LTVector* pOut)
{
	CClientCamera* pCamera;
	int left, top, right, bottom;
	float halfW, halfH, xRatio, yRatio;
	LTVector vRight, vUp, vForward;

	if (hCamera && ((CClientObject*)hCamera)->m_ObjectType == OT_CAMERA && pOut)
	{
		pCamera = (CClientCamera*)hCamera;

		if (pCamera->m_bFullScreen)
		{
			left = top = 0;
			right = g_RenderStruct.m_Width;
			bottom = g_RenderStruct.m_Height;
		}
		else
		{
			left = pCamera->m_Rect.left;
			top = pCamera->m_Rect.top;
			right = pCamera->m_Rect.right;
			bottom = pCamera->m_Rect.bottom;
		}

		if (sx < left || sx >= right || sy < top || sy >= bottom)
			RETURN_ERROR(1, "Get3DCameraPt", LT_OUTSIDE);

		halfW = (float)((right - left) >> 1);
		halfH = (float)((bottom - top) >> 1);

		xRatio = ((float)sx - ((float)left + halfW)) / halfW;
		xRatio = xRatio / (1.0f / (1.0f / (float)tan(MATH_HALFPI - pCamera->m_fFovX * 0.5f)));

		yRatio = ((float)sy - ((float)top + halfH)) / halfH;
		yRatio = -(yRatio / (1.0f / (1.0f / (float)tan(MATH_HALFPI - pCamera->m_fFovY * 0.5f))));

		quat_GetVectors(&((CClientObject*)hCamera)->m_Rotation, &vRight, &vUp, &vForward);

		*pOut = vRight * xRatio;
		*pOut += vUp * yRatio;
		*pOut += vForward;
		*pOut += ((CClientObject*)hCamera)->m_Pos;
		return LT_OK;
	}

	RETURN_ERROR(1, "Get3DCameraPt", LT_INVALIDPARAMS);
}

void ci_GetCameraFOV(HLOCALOBJ hObj, float* pX, float* pY)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_CAMERA && pX && pY)
	{
		*pX = ((CClientCamera*)hObj)->m_fFovX;
		*pY = ((CClientCamera*)hObj)->m_fFovY;
	}
}

LTBOOL ci_GetCameraLightAdd(HLOCALOBJ hCamera, LTVector* pAdd)
{
	if (hCamera && ((CClientObject*)hCamera)->m_ObjectType == OT_CAMERA)
	{
		*pAdd = ((CClientCamera*)hCamera)->m_LightAdd;
		return LTTRUE;
	}

	return LTFALSE;
}

// The one accessor in the whole table that trusts its object handle: the type
// byte is read straight off it with no null check first.
void ci_SetLightRadius(HLOCALOBJ hObj, float radius)
{
#ifdef NOLFSRC_MODERN
	// fixed: a null handle is ignored, as every other accessor does (RETAIL_BUGS.md)
	if (!hObj) return;
#endif
	if (((CClientObject*)hObj)->m_ObjectType == OT_LIGHT)
	{
		if (radius > ((CClientLight*)hObj)->m_fLightRadius)
		{
			((CClientLight*)hObj)->m_fLightRadius = radius;
			sub_420670(g_pClientMgr, (CClientLight*)hObj);
		}
		else
		{
			((CClientLight*)hObj)->m_fLightRadius = radius;
		}
	}
}

// A null surface zeroes both, without checking either pointer; a real one
// checks each and fills in whichever was asked for.
void ci_GetSurfaceDims(HSURFACE hSurf, uint32* pWidth, uint32* pHeight)
{
	if (hSurf)
	{
		if (pWidth)
			*pWidth = ((CSurface*)hSurf)->m_nWidth;

		if (pHeight)
			*pHeight = ((CSurface*)hSurf)->m_nHeight;
	}
	else
	{
#ifdef NOLFSRC_MODERN
		// fixed: only the pointers given are written (RETAIL_BUGS.md)
		if (pWidth) *pWidth = 0;
		if (pHeight) *pHeight = 0;
#else
		*pWidth = *pHeight = 0;
#endif
	}
}

void ci_SetCameraRect(HLOCALOBJ hObj, LTBOOL bFullscreen, int left, int top, int right, int bottom)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_CAMERA)
	{
		((CClientCamera*)hObj)->m_bFullScreen = bFullscreen;
		((CClientCamera*)hObj)->m_Rect.left = left;
		((CClientCamera*)hObj)->m_Rect.top = top;
		((CClientCamera*)hObj)->m_Rect.right = right;
		((CClientCamera*)hObj)->m_Rect.bottom = bottom;
	}
}

LTBOOL ci_SetCameraLightAdd(HLOCALOBJ hCamera, LTVector* pAdd)
{
	if (hCamera && ((CClientObject*)hCamera)->m_ObjectType == OT_CAMERA)
	{
		((CClientCamera*)hCamera)->m_LightAdd = *pAdd;
		return LTTRUE;
	}

	return LTFALSE;
}

void ci_AddParticles(HLOCALOBJ hObj, uint32 nParticles, LTVector* pMinOffset,
                     LTVector* pMaxOffset, LTVector* pMinVelocity, LTVector* pMaxVelocity,
                     LTVector* pMinColor, LTVector* pMaxColor, float minLifetime,
                     float maxLifetime)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_PARTICLESYSTEM)
		sub_45c780(hObj, nParticles, pMinOffset, pMaxOffset, pMinVelocity, pMaxVelocity,
		           pMinColor, pMaxColor, minLifetime, maxLifetime);
}

// The tail is not a particle at all: it is a node embedded in the system, so
// walking the list means stopping when the next pointer reaches it.
LTBOOL ci_GetParticles(HLOCALOBJ hObj, LTParticle** pHead, LTParticle** pTail)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_PARTICLESYSTEM)
	{
		*pHead = ((CClientParticleSystem*)hObj)->m_ParticleTail.m_pNext;
		*pTail = &((CClientParticleSystem*)hObj)->m_ParticleTail;
		return LTTRUE;
	}

	return LTFALSE;
}

LTRESULT ci_GetParticleLifetime(HLOCALOBJ hSystem, LTParticle* pParticle, LTFLOAT& fLifetime)
{
	if (!pParticle)
		RETURN_ERROR(1, "CLTClient::GetParticleLifetime", LT_INVALIDPARAMS);

	fLifetime = ((CClientParticle*)pParticle)->m_Lifetime;
	return LT_OK;
}

// Unlinking a line does not free it: it goes back on the system's free list,
// through the same word at offset zero the allocator hands out.
void ci_RemoveLine(HLOCALOBJ hObj, HLTLINE hLine)
{
	if (hObj && hLine && ((CClientObject*)hObj)->m_ObjectType == OT_LINESYSTEM)
	{
		CClientLine* pLine = (CClientLine*)hLine;

		pLine->m_pNext->m_pPrev = pLine->m_pPrev;
		pLine->m_pPrev->m_pNext = pLine->m_pNext;

		sb_Free(((CClientLineSystem*)hObj)->m_pLineBank, pLine);
	}
}

// The shipped tag is GetParticleLifetime's, not this function's: the two were
// written by copy and the name was not changed.
LTRESULT ci_GetParticleTotalLifetime(HLOCALOBJ hSystem, LTParticle* pParticle, LTFLOAT& fTotalLifetime)
{
	if (!pParticle)
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "CLTClient::GetParticleTotalLifetime", LT_INVALIDPARAMS);
#else
		RETURN_ERROR(1, "CLTClient::GetParticleLifetime", LT_INVALIDPARAMS);
#endif

	fTotalLifetime = ((CClientParticle*)pParticle)->m_TotalLifetime;
	return LT_OK;
}

void ci_RemoveParticle(HLOCALOBJ hSystem, LTParticle* pParticle)
{
	if (hSystem && pParticle && ((CClientObject*)hSystem)->m_ObjectType == OT_PARTICLESYSTEM)
	{
		CClientParticle*       pParticle2 = (CClientParticle*)pParticle;
		CClientParticleSystem* pSystem = (CClientParticleSystem*)hSystem;

		pParticle2->m_pPrev->m_pNext = pParticle2->m_pNext;
		((CClientParticle*)pParticle2->m_pNext)->m_pPrev = pParticle2->m_pPrev;

		sb_Free(pSystem->m_pParticleBank, pParticle2);
		--pSystem->m_nParticles;
	}
}

LTRESULT ci_GetSurfaceAlpha(HSURFACE hSurface, float& alpha)
{
	if (!hSurface)
		RETURN_ERROR_NAMED(2, "ILTClient::GetSurfaceAlpha", LT_INVALIDPARAMS);

	alpha = ((CSurface*)hSurface)->m_fAlpha;
	return LT_OK;
}

LTRESULT ci_ShutdownRender(uint32 flags)
{
	sub_4627d0(g_pClientMgr, 1);

	if (flags & RSHUTDOWN_MINIMIZEWINDOW)
		ShowWindow(g_CluClientGlob.m_hMainWnd, SW_MINIMIZE);

	if (flags & RSHUTDOWN_HIDEWINDOW)
		ShowWindow(g_CluClientGlob.m_hMainWnd, SW_HIDE);

	g_CluClientGlob.m_bRenderShutdown = 1;
	return LT_OK;
}

// A null previous line means start at the head, which is the tail node's own
// next pointer; reaching the tail node itself is the end of the list.
//
// IDENTICAL.  The shipped code writes the "is it the tail" test out twice,
// once per branch.  An early refusal, then an if/else whose arms each test
// and return with their own if/else (reading m_pNext at the test and again
// at the return), keeps the two copies; the ternary form let VC6 cross-jump
// them into one shared tail (56 bytes).
HLTLINE ci_GetNextLine(HLOCALOBJ hObj, HLTLINE hPrev)
{
	CClientLineSystem* pSystem;
	CClientLine* pLine;

	if (!hObj || ((CClientObject*)hObj)->m_ObjectType != OT_LINESYSTEM)
		return LTNULL;

	pSystem = (CClientLineSystem*)hObj;
	if (hPrev)
	{
		pLine = (CClientLine*)hPrev;
		if (pLine->m_pNext == &pSystem->m_LineTail)
			return LTNULL;
		else
			return (HLTLINE)pLine->m_pNext;
	}
	else
	{
		if (pSystem->m_LineTail.m_pNext == &pSystem->m_LineTail)
			return LTNULL;
		else
			return (HLTLINE)pSystem->m_LineTail.m_pNext;
	}
}

// The tag is GetServiceList's; this is the third function in the file to
// carry a name it inherited by copy.
LTRESULT ci_FreeServiceList(NetService* pListHead)
{
	if (!pListHead)
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(2, "CLTClient::FreeServiceList", LT_INVALIDPARAMS);
#else
		RETURN_ERROR(2, "CLTClient::GetServiceList", LT_INVALIDPARAMS);
#endif

	return g_pClientMgr->m_NetMgr.FreeServiceList(pListHead);
}

LTRESULT ci_SelectService(HNETSERVICE hNetService)
{
	if (!hNetService)
		RETURN_ERROR(2, "CLTClient::SelectService", LT_INVALIDPARAMS);

	return g_pClientMgr->m_NetMgr.SelectService(hNetService);
}

// The interface has no port to report, so the port the net manager insists on
// writing goes into a local nobody reads.
LTRESULT ci_GetTcpIpAddress(char* sAddress, uint32 dwBufferSize)
{
	unsigned short hostPort;

	if (!sAddress)
		RETURN_ERROR(1, "CLTClient::GetTcpIpAddress", LT_INVALIDPARAMS);

	return g_pClientMgr->m_NetMgr.GetTcpIpAddress(sAddress, dwBufferSize, hostPort);
}

LTBOOL ci_PlayBreak(char* szSong, uint32 dwBoundaryFlags)
{
	void* pSong;

	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return LTFALSE;

	pSong = ci_GetMusicMgr()->Fn_14(szSong);

	if (!pSong)
	{
		pSong = ci_GetMusicMgr()->Fn_10(szSong);

		if (!pSong)
			return LTFALSE;
	}

	return ci_GetMusicMgr()->Fn_1c(pSong, dwBoundaryFlags);
}

LTRESULT ci_GetObjectScale(HLOCALOBJ hObj, LTVector* pScale)
{
	if (!hObj)
		RETURN_ERROR(1, "CLTClient::GetObjectScale", LT_INVALIDPARAMS);

	pScale->x = ((CClientObject*)hObj)->m_Scale.x;
	pScale->y = ((CClientObject*)hObj)->m_Scale.y;
	pScale->z = ((CClientObject*)hObj)->m_Scale.z;
	return LT_OK;
}

LTRESULT ci_SetObjectScale(HLOCALOBJ hObj, LTVector* pScale)
{
	if (!hObj)
		RETURN_ERROR(1, "CLTClient::SetObjectScale", LT_INVALIDPARAMS);

	sub_420580(g_pClientMgr, hObj, pScale);
	return LT_OK;
}

// The interface takes the colour as four floats and the object keeps four
// bytes, so each one is scaled by 255 and truncated on the way in.
void ci_SetObjectColor(HLOCALOBJ hObject, float r, float g, float b, float a)
{
	if (hObject)
	{
		((CClientObject*)hObject)->m_ColorR = (uint8)(r * 255.0f);
		((CClientObject*)hObject)->m_ColorG = (uint8)(g * 255.0f);
		((CClientObject*)hObject)->m_ColorB = (uint8)(b * 255.0f);
		((CClientObject*)hObject)->m_ColorA = (uint8)(a * 255.0f);
	}
}

LTRESULT ci_GetObjectUserFlags(HLOCALOBJ hObj, uint32* pFlags)
{
	if (hObj && pFlags)
	{
		*pFlags = ((CClientObject*)hObj)->m_UserFlags;
		return LT_OK;
	}

	RETURN_ERROR(1, "CLTClient::GetObjectUserFlags", LT_INVALIDPARAMS);
}

// The SDK's SETRGB_FT / SETRGB_F (ltbasedefs.h) written out, one per arm.
// VC6 re-associates the three shifted terms into (r << 8 | g) << 8 | b, folds
// the transparency bit into the red term as 0xffff8000, and hoists the red
// multiply above the test. An accumulator spelling (nColor = nColor << 8 | g)
// was four bytes over: it spent %edi on the pending g term.
HLTCOLOR ci_CreateColor(float r, float g, float b, LTBOOL bTransparent)
{
	if (bTransparent)
		return (((uint32)(r * 255.0f) << 16) | ((uint32)(g * 255.0f) << 8) | ((uint32)(b * 255.0f))) | 0x80000000;
	else
		return (((uint32)(r * 255.0f) << 16) | ((uint32)(g * 255.0f) << 8) | ((uint32)(b * 255.0f)));
}

// Every .ren beside the executable is a candidate renderer. The find handle
// is never closed.
RMode* ci_GetRenderModes()
{
	RMode* pModeList;
	struct _finddata_t findData;
	long hFind;

	pModeList = LTNULL;

	hFind = _findfirst("*.ren", &findData);

	if (hFind == -1)
		return pModeList;

	do
	{
		if (!(findData.attrib & _A_SUBDIR))
			sub_42de60(findData.name, &pModeList);
	}
	while (_findnext(hFind, &findData) != -1);

#ifdef NOLFSRC_MODERN
	// fixed: the find handle is closed (RETAIL_BUGS.md)
	_findclose(hFind);
#endif
	return pModeList;
}

void ci_GetLineInfo(HLTLINE hLine, LTLine* pLine)
{
	if (hLine && pLine)
	{
		uint32 i;

		for (i = 0; i < 2; i++)
		{
			pLine->m_Points[i].m_Pos = ((LTLine*)hLine)->m_Points[i].m_Pos;
			pLine->m_Points[i].r = ((LTLine*)hLine)->m_Points[i].r;
			pLine->m_Points[i].g = ((LTLine*)hLine)->m_Points[i].g;
			pLine->m_Points[i].b = ((LTLine*)hLine)->m_Points[i].b;
			pLine->m_Points[i].a = ((LTLine*)hLine)->m_Points[i].a;
		}
	}
}

// The line system as ci_SetLineInfo (0x0043ceb0) and ci_AddLine (0x0043d1c0)
// see it: clientmgr.h's CClientLine / CClientLineSystem with the fields
// between their pads named (views, names ours).  A line is the SDK's LTLine
// (two 0x1c-byte points) followed by its owner and its links; the system
// keeps a changed flag at 0x1ac and the bounding box, centre and radius the
// two functions maintain from 0x1f4.
class CLineSystemQ43c;

class CLineQ43c : public LTLine
{
public:
	CLineSystemQ43c* m_pSystem;              // 0x038
	CLineQ43c*       m_pPrev;                // 0x03c
	CLineQ43c*       m_pNext;                // 0x040
};

class CLineSystemQ43c
{
public:
	uint8       m_pad_00[0xbc];
	uint8       m_ObjectType;                // 0x0bc
	uint8       m_pad_bd[0x1a8 - 0xbd];
	StructBank* m_pLineBank;                 // 0x1a8
	LTBOOL      m_bChanged;                  // 0x1ac
	CLineQ43c   m_LineHead;                  // 0x1b0
	LTVector    m_MinPos;                    // 0x1f4
	LTVector    m_MaxPos;                    // 0x200
	LTVector    m_SystemCenter;              // 0x20c
	float       m_SystemRadius;              // 0x218
};

// What both functions do per point and then once: SetLineInfo reloads the
// line's owner exactly once per group (0x0043cf1f, 0x0043cfec, 0x0043d0bd),
// which is an argument evaluated per call, not a member path per statement.
// Stand-in, name ours.
inline void ls_ExtendBoxQ43c(CLineSystemQ43c* pSystem, const LTVector& vPos)
{
	VEC_MIN(pSystem->m_MinPos, pSystem->m_MinPos, vPos);
	VEC_MAX(pSystem->m_MaxPos, pSystem->m_MaxPos, vPos);
}

// THE CENTRE AND RADIUS -- AN INLINE-DEPTH STAND-IN.  Shipped expands the
// LTVector operator bodies here but CALLS the three-float constructor
// (0x00411d40, both functions) and Mag (0x00436760, both), and ci_AddLine
// calls operator+ whole (0x004316d0) where ci_SetLineInfo expands it.  That
// is VC6's inline depth: the calls appear only this far down.  Measured:
// with 5 forwarders or fewer everything expands; 6 calls the constructors
// only; 7 calls every operator.  Mag and AddLine's operator+ each sit one
// level deeper again.  The original presumably reached the depth through
// its line-system helpers; this chain is only the depth (names ours).
inline float ls_MagQ43c(const LTVector& v) { return v.Mag(); }
inline LTVector ls_AddQ43c(const LTVector& a, const LTVector& b) { return a + b; }

inline void ls_UpdateCenterQ43c(CLineSystemQ43c* pSystem)
{
	LTVector vHalf = pSystem->m_MaxPos - pSystem->m_MinPos;
	vHalf *= 0.5f;
	pSystem->m_SystemCenter = pSystem->m_MinPos + vHalf;
	pSystem->m_SystemRadius = ls_MagQ43c(vHalf) + 1.0f;
}

// ci_AddLine's: the same, with operator+ one level down.
inline void ls_UpdateCenterAddQ43c(CLineSystemQ43c* pSystem)
{
	LTVector vHalf = pSystem->m_MaxPos - pSystem->m_MinPos;
	vHalf *= 0.5f;
	pSystem->m_SystemCenter = ls_AddQ43c(pSystem->m_MinPos, vHalf);
	pSystem->m_SystemRadius = ls_MagQ43c(vHalf) + 1.0f;
}

inline void ls_Center1Q43c(CLineSystemQ43c* p) { ls_UpdateCenterQ43c(p); }
inline void ls_Center2Q43c(CLineSystemQ43c* p) { ls_Center1Q43c(p); }
inline void ls_Center3Q43c(CLineSystemQ43c* p) { ls_Center2Q43c(p); }
inline void ls_Center4Q43c(CLineSystemQ43c* p) { ls_Center3Q43c(p); }
inline void ls_Center5Q43c(CLineSystemQ43c* p) { ls_Center4Q43c(p); }
inline void ls_Center6Q43c(CLineSystemQ43c* p) { ls_Center5Q43c(p); }

inline void ls_CenterAdd1Q43c(CLineSystemQ43c* p) { ls_UpdateCenterAddQ43c(p); }
inline void ls_CenterAdd2Q43c(CLineSystemQ43c* p) { ls_CenterAdd1Q43c(p); }
inline void ls_CenterAdd3Q43c(CLineSystemQ43c* p) { ls_CenterAdd2Q43c(p); }
inline void ls_CenterAdd4Q43c(CLineSystemQ43c* p) { ls_CenterAdd3Q43c(p); }
inline void ls_CenterAdd5Q43c(CLineSystemQ43c* p) { ls_CenterAdd4Q43c(p); }
inline void ls_CenterAdd6Q43c(CLineSystemQ43c* p) { ls_CenterAdd5Q43c(p); }

// 0x0043ceb0.  Copy the new points in, grow the owner's box to take them,
// and recompute its centre and radius.
void ci_SetLineInfo(HLTLINE hLine, LTLine* pLine)
{
	if (hLine && pLine)
	{
		CLineQ43c* pDst = (CLineQ43c*)hLine;
		uint32 i;

		for (i = 0; i < 2; i++)
		{
			pDst->m_Points[i].m_Pos = pLine->m_Points[i].m_Pos;
			pDst->m_Points[i].r = pLine->m_Points[i].r;
			pDst->m_Points[i].g = pLine->m_Points[i].g;
			pDst->m_Points[i].b = pLine->m_Points[i].b;
			pDst->m_Points[i].a = pLine->m_Points[i].a;
		}

		ls_ExtendBoxQ43c(pDst->m_pSystem, pDst->m_Points[0].m_Pos);
		ls_ExtendBoxQ43c(pDst->m_pSystem, pDst->m_Points[1].m_Pos);
		ls_Center6Q43c(pDst->m_pSystem);

		pDst->m_pSystem->m_bChanged = LTTRUE;
	}
}

// 0x0043d1c0.  A new line from the system's bank, linked in before the head
// node (so at the end of the walk), filled, and folded into the box.  The
// allocation is not tested (0x0043d20b skips only the pop).
HLTLINE ci_AddLine(HLOCALOBJ hObj, LTLine* pLine)
{
	CLineSystemQ43c* pSystem;
	CLineQ43c* pNew;
	uint32 i;

	if (!hObj || !pLine || ((CLineSystemQ43c*)hObj)->m_ObjectType != OT_LINESYSTEM)
		return LTNULL;

	pSystem = (CLineSystemQ43c*)hObj;
	pNew = (CLineQ43c*)sb_Allocate(pSystem->m_pLineBank);

	// Both back-link stores go through the new line's own links: 0x0043d227
	// stores the head's back link through the just-written m_pNext (%eax),
	// not as a system field.
	pNew->m_pNext = &pSystem->m_LineHead;
	pNew->m_pPrev = pSystem->m_LineHead.m_pPrev;
	pNew->m_pNext->m_pPrev = pNew;
	pNew->m_pPrev->m_pNext = pNew;
	pNew->m_pSystem = pSystem;

	for (i = 0; i < 2; i++)
	{
		pNew->m_Points[i].m_Pos = pLine->m_Points[i].m_Pos;
		pNew->m_Points[i].r = pLine->m_Points[i].r;
		pNew->m_Points[i].g = pLine->m_Points[i].g;
		pNew->m_Points[i].b = pLine->m_Points[i].b;
		pNew->m_Points[i].a = pLine->m_Points[i].a;
	}

	ls_ExtendBoxQ43c(pSystem, pNew->m_Points[0].m_Pos);
	ls_ExtendBoxQ43c(pSystem, pNew->m_Points[1].m_Pos);
	ls_CenterAdd6Q43c(pSystem);

	pSystem->m_bChanged = LTTRUE;

	return (HLTLINE)pNew;
}

LTRESULT ci_RegisterConsoleProgram(char* pName, ConsoleProgramFn fn)
{
	if (con_FindCommand(&g_ClientConsoleState, pName))
		RETURN_ERROR(1, "CLTClient::RegisterConsoleProgram", LT_ALREADYEXISTS);

	con_AddCommand(&g_ClientConsoleState, pName, (void*)fn, (void*)1);
	return LT_OK;
}

LTRESULT ci_SetGlobalPanInfo(uint32 index, float xOffset, float zOffset, float xScale, float zScale)
{
	if (index >= 2)
		RETURN_ERROR(1, "SetGlobalPanInfo", LT_INVALIDPARAMS);

	{
		PanInfo* pInfo = &g_RenderStruct.m_GlobalPanInfo[index];

		pInfo->m_xOffset = xOffset;
		pInfo->m_zOffset = zOffset;
		pInfo->m_xScale = xScale;
		pInfo->m_zScale = zScale;
	}

	return LT_OK;
}

void ci_GetCameraRect(HLOCALOBJ hObj, LTBOOL* bFullscreen, int* left, int* top,
                      int* right, int* bottom)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_CAMERA
	    && bFullscreen && left && top && right && bottom)
	{
		*bFullscreen = ((CClientCamera*)hObj)->m_bFullScreen;
		*left = ((CClientCamera*)hObj)->m_Rect.left;
		*top = ((CClientCamera*)hObj)->m_Rect.top;
		*right = ((CClientCamera*)hObj)->m_Rect.right;
		*bottom = ((CClientCamera*)hObj)->m_Rect.bottom;
	}
}

LTRESULT ci_OptimizeParticles(HLOCALOBJ hSystem)
{
	if (hSystem && ((CClientObject*)hSystem)->m_ObjectType == OT_PARTICLESYSTEM)
	{
		sub_45cfb0(hSystem);
		sub_4207a0(g_pClientMgr, hSystem, &((CClientObject*)hSystem)->m_Pos, 1);
		return LT_OK;
	}

	RETURN_ERROR(1, "CLTClient::OptimizeParticles", LT_INVALIDPARAMS);
}

// Another inherited tag: it reports itself as OptimizeSurface.
LTRESULT ci_UnoptimizeSurface(HSURFACE hSurface)
{
	if (!hSurface)
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "UnoptimizeSurface", LT_INVALIDPARAMS);
#else
		RETURN_ERROR(1, "OptimizeSurface", LT_INVALIDPARAMS);
#endif

	if (((CSurface*)hSurface)->m_nFlags & 2)
	{
		((CSurface*)hSurface)->m_nFlags &= ~2;

		if (g_pRender->m_nCanOptimizeSurfaces)
			g_pRender->UnoptimizeSurface(((CSurface*)hSurface)->m_pRenderData);
	}

	return LT_OK;
}

LTRESULT ci_DrawSurfaceToSurface(HSURFACE hDest, HSURFACE hSrc, LTRect* pSrcRect,
                                 int destX, int destY)
{
	if (!g_pRender)
		RETURN_ERROR(1, "DrawSurfaceToSurface", LT_NOTINITIALIZED);

	return sub_40d290(hDest, hSrc, pSrcRect, destX, destY, (void*)sub_40d660);
}

// Reported at level 4, where every other entry in the family uses 1.
LTRESULT ci_ScaleSurfaceToSurface(HSURFACE hDest, HSURFACE hSrc, LTRect* pDestRect,
                                  LTRect* pSrcRect)
{
	if (!g_pRender)
#ifdef NOLFSRC_MODERN
	// fixed: checks reported at the wrong debug level (RETAIL_BUGS.md).
		RETURN_ERROR(1, "ScaleSurfaceToSurface", LT_NOTINITIALIZED);
#else
		RETURN_ERROR(4, "ScaleSurfaceToSurface", LT_NOTINITIALIZED);
#endif

	return sub_40dcc0(hDest, hSrc, pDestRect, pSrcRect, 1, 0, 0);
}


// ----------------------------------------------------------------------- //
// 0x0040dcc0, 764 bytes -- the scale worker behind the three
// ScaleSurfaceToSurface* entries ("InternalScaleSurfaceToSurface", the
// literal at 0x004a81e4, is its report name; the C name is clientmgr.h's).
//   * the two rects and the request are constructed at the TOP
//     (0x0040dcd5..0x0040dd05), before the argument test: they are
//     locals with inline constructors, in that declaration order.
//   * a copy or a straight blit onto the screen surface is offered to the
//     renderer's RenderStruct+0xec (0x004b4444) once the clipper
//     0x0043a0e0 says something is visible; everything else becomes four
//     LTWarpPts and goes to the matching WarpSurfaceToSurface* entry.
//   * a missing surface reports LT_INVALIDPARAMS at debug level 4.
// ----------------------------------------------------------------------- //

// The request RenderStruct+0xec / +0xf0 / +0xfc take (clientblit.cpp
// measures it).  Five of the six head fields constructed, +0x08 skipped;
// the two tail fields are 0x0040e4d0's and are not constructed.
class CIBlitRequest
{
public:
	CIBlitRequest()
	{
		m_pSurfaceData = LTNULL;
		m_bTransparent = 0;
		m_pSrcRect     = LTNULL;
		m_pDestRect    = LTNULL;
		m_fAlpha       = 1.0f;
	}

	void*   m_pSurfaceData;                  // 0x00
	uint32  m_bTransparent;                  // 0x04
	void*   m_pUnknown08;                    // 0x08
	LTRect* m_pSrcRect;                      // 0x0c
	LTRect* m_pDestRect;                     // 0x10
	float   m_fAlpha;                        // 0x14
	void*   m_pPoints;                       // 0x18
	uint32  m_nPoints;                       // 0x1c
};

// RenderStruct+0xec (0x004b4444): g_RenderStruct.BlitToScreen.

class CSurfaceView;
void   sub_40d7d0(CSurfaceView* pSurface);                         // 0x0040d7d0
LTBOOL cis_ClipScaledBlitQ438(int nSrcWidth, int nSrcHeight,
                              int nSrcLeft, int nSrcTop,
                              int nSrcRight, int nSrcBottom,
                              int nDstWidth, int nDstHeight,
                              int nDstLeft, int nDstTop,
                              int nDstRight, int nDstBottom,
                              LTRect* pSrcOut, LTRect* pDstOut);   // 0x0043a0e0

LTRESULT sub_40dcc0(HSURFACE hDest, HSURFACE hSrc, LTRect* pDestRect,
                    LTRect* pSrcRect, int nMode, HLTCOLOR hTransColor,
                    HLTCOLOR hFillColor)
{
	LTRect        destRect, srcRect;
	CIBlitRequest request;
	LTWarpPt      warpPts[4];
	CSurface*     pDest = (CSurface*)hDest;
	CSurface*     pSrc = (CSurface*)hSrc;

	if (hSrc && hDest)
	{
		if (pDestRect)
		{
			destRect = *pDestRect;
		}
		else
		{
			destRect.left = destRect.top = 0;
			destRect.right = pDest->m_nWidth;
			destRect.bottom = pDest->m_nHeight;
		}

		if (pSrcRect)
		{
			srcRect = *pSrcRect;
		}
		else
		{
			srcRect.left = srcRect.top = 0;
			srcRect.right = pSrc->m_nWidth;
			srcRect.bottom = pSrc->m_nHeight;
		}

		if ((nMode == 0 || nMode == 1) && hDest == (HSURFACE)&g_ScreenSurface)
		{
			if (cis_ClipScaledBlitQ438(pSrc->m_nWidth, pSrc->m_nHeight,
			        srcRect.left, srcRect.top, srcRect.right, srcRect.bottom,
			        pDest->m_nWidth, pDest->m_nHeight,
			        destRect.left, destRect.top, destRect.right, destRect.bottom,
			        &srcRect, &destRect)
			    && g_RenderStruct.BlitToScreen)
			{
				sub_40d7d0((CSurfaceView*)hSrc);

				request.m_pSurfaceData = pSrc->m_pRenderData;
				request.m_pUnknown08   = (void*)g_TransColorQ40C;
				request.m_pSrcRect     = &srcRect;
				request.m_pDestRect    = &destRect;
				request.m_bTransparent = (nMode == 0);
				request.m_fAlpha       = pSrc->m_fAlpha;
				g_RenderStruct.BlitToScreen(&request);

				pDest->m_nFlags |= 4;
				return LT_OK;
			}
		}

		warpPts[0].dest_x   = (float)destRect.left;
		warpPts[0].dest_y   = (float)destRect.top;
		warpPts[0].source_x = (float)srcRect.left;
		warpPts[0].source_y = (float)srcRect.top;

		warpPts[1].dest_x   = (float)destRect.right;
		warpPts[1].dest_y   = (float)destRect.top;
		warpPts[1].source_x = (float)srcRect.right;
		warpPts[1].source_y = (float)srcRect.top;

		warpPts[2].dest_x   = (float)destRect.right;
		warpPts[2].dest_y   = (float)destRect.bottom;
		warpPts[2].source_x = (float)srcRect.right;
		warpPts[2].source_y = (float)srcRect.bottom;

		warpPts[3].dest_x   = (float)destRect.left;
		warpPts[3].dest_y   = (float)destRect.bottom;
		warpPts[3].source_x = (float)srcRect.left;
		warpPts[3].source_y = (float)srcRect.bottom;

		if (nMode == 0)
			return ci_WarpSurfaceToSurfaceTransparent(hDest, hSrc, warpPts, 4, hTransColor);
		else if (nMode == 1)
			return ci_WarpSurfaceToSurface(hDest, hSrc, warpPts, 4);
		else
			return ci_WarpSurfaceToSurfaceSolidColor(hDest, hSrc, warpPts, 4, hTransColor, hFillColor);
	}

#ifdef NOLFSRC_MODERN
	RETURN_ERROR(1, "InternalScaleSurfaceToSurface", LT_INVALIDPARAMS);
#else
	RETURN_ERROR(4, "InternalScaleSurfaceToSurface", LT_INVALIDPARAMS);
#endif
}

LTRESULT ci_ScaleSurfaceToSurfaceSolidColor(HSURFACE hDest, HSURFACE hSrc,
                                            LTRect* pDestRect, LTRect* pSrcRect,
                                            HLTCOLOR hTransColor, HLTCOLOR hFillColor)
{
	if (!g_pRender)
		RETURN_ERROR(1, "ScaleSurfaceToSurfaceSolidColor", LT_NOTINITIALIZED);

	return sub_40dcc0(hDest, hSrc, pDestRect, pSrcRect, 2, hTransColor, hFillColor);
}

LTRESULT ci_WarpSurfaceToSurface(HSURFACE hDest, HSURFACE hSrc, LTWarpPt* pCoords,
                                 int nCoords)
{
	if (!g_pRender)
		RETURN_ERROR(1, "WarpSurfaceToSurface", LT_NOTINITIALIZED);

	return sub_40e120(hDest, hSrc, pCoords, nCoords, (void*)sub_43aa20);
}


// ----------------------------------------------------------------------- //
//  0x0040e120 -- InternalWarpSurfaceToSurface (its own de_error name,
//  0x004a8260), the worker the three WarpSurfaceToSurface entries share.
//  Shipped order puts it straight after ci_WarpSurfaceToSurface (0x0040e0b0)
//  and before ...Transparent (0x0040e330).
//
//  Clamp every source point into the source surface, clip the polygon to
//  the destination (0x0043a4f0, which may replace the point array and
//  count through the two pointers -- which is why pCoords and nCoords are
//  re-read from their argument slots on every trip), build the two edge
//  tables (0x0043a390, warpblit.cpp's cis_SetupWarpEdgesQ438) and hand
//  everything to the per-pixel warp routine the entry chose.  The two
//  0x3000-byte tables are 1024 twelve-byte edges each (warpblit.cpp's
//  SWarpEdgeW12Q438).  (names ours except the function's)
// ----------------------------------------------------------------------- //

#define MAX_WARP_POINTS_Q40E 10

struct SWarpEdgeQ40E
{
	int16 x;                                 // 0x00
	int16 m_pad02;                           // 0x02
	int32 u;                                 // 0x04  16.16
	int32 v;                                 // 0x08  16.16
};

typedef LTRESULT (*WarpFnQ40E)(HSURFACE hDest, HSURFACE hSrc,
                               SWarpEdgeQ40E* pLeft, SWarpEdgeQ40E* pRight,
                               uint32 nMinY, uint32 nMaxY);

// ----------------------------------------------------------------------- //
//  0x0043a4f0, 1130 bytes -- the warp polygon clipper (warpblit.cpp's and
//  diblit.cpp's notes: "cis_ClipWarpPoly").  HOME: the warp blitter's TU
//  (0x0043a390..0x0043b40f); written here beside its one caller.
//
//  The four planes are published into globals and the polygon is clipped
//  against each in turn with the same Sutherland-Hodgman pass diblit.cpp
//  reconstructs out of line for the last one (0x0043b270), ping-ponging
//  between the two halves of g_WarpClipBufferQ438.  The first three passes
//  are expanded here; the second and third still CALL their plane's
//  interpolation (warpblit.cpp's two InterpolateQ438 members, with `this`
//  an empty clipper object copied into the pass -- the `mov 0x17(%esp),%cl
//  ; mov %cl,0x20(%esp)` before each), where the first expands it.  The
//  fourth pass gets two dwords it never reads (diblit.cpp's note), loaded
//  from one uninitialised slot.
// ----------------------------------------------------------------------- //

struct LTWarpPtQ438;

class CWarpClipTopQ438
{
public:
	void InterpolateQ438(LTWarpPtQ438* pPrev, LTWarpPtQ438* pCur,
	                     LTWarpPtQ438* pOut);                 // 0x0043a9c0
};

class CWarpClipRightQ438
{
public:
	void InterpolateQ438(LTWarpPtQ438* pPrev, LTWarpPtQ438* pCur,
	                     LTWarpPtQ438* pOut);                 // 0x0043a960
};

LTBOOL cis_ClipWarpDestBottomQ438(int nUnused1Q438, int nUnused2Q438,
                                  LTWarpPt** ppPts, int* pnPts);     // 0x0043b270

float g_fWarpClipLeftQ40E;                                    // 0x004b3e18
extern float    g_fWarpClipTopQ438;                           // 0x004b3690
extern float    g_fWarpClipRightQ438;                         // 0x004b3e1c
extern float    g_fWarpClipBottomQ438;                        // 0x004b3e24
extern int      g_nWarpClipBufferQ438;                        // 0x004b3e20
extern LTWarpPt g_WarpClipBufferQ438[2][60];                  // 0x004b3698

// Pass 1, dest_x >= left, interpolation expanded.
inline LTBOOL cis_ClipLeftQ40E(LTWarpPt** ppPts, int* pnPts)
{
	LTBOOL bInside[50];
	int nInside;
	int nPrev;
	int i;

	LTWarpPt* pOutStart;
	LTWarpPt* pOut;
	LTWarpPt* pPrev;
	LTWarpPt* pCur;

	float fT;

	nInside = 0;

	for (i = 0; i < *pnPts; i++)
	{
		LTBOOL bIn = !((*ppPts)[i].dest_x < g_fWarpClipLeftQ40E);
		bInside[i] = bIn ? LTTRUE : LTFALSE;

		nInside += bInside[i];
	}

	if (!nInside)
		return LTFALSE;

	if (nInside != *pnPts)
	{
		pOut = g_WarpClipBufferQ438[g_nWarpClipBufferQ438];
		pOutStart = pOut;

		g_nWarpClipBufferQ438 = !g_nWarpClipBufferQ438;

		nPrev = *pnPts - 1;
		pPrev = &(*ppPts)[nPrev];

		for (i = 0; i < *pnPts; i++)
		{
			pCur = &(*ppPts)[i];

			if (bInside[nPrev])
			{
				*pOut = *pPrev;

				pOut++;
			}

			if (bInside[nPrev] != bInside[i])
			{
				fT = (g_fWarpClipLeftQ40E - pPrev->dest_x) /
				     (pCur->dest_x - pPrev->dest_x);

				pOut->dest_x   = g_fWarpClipLeftQ40E;
				pOut->dest_y   = (pCur->dest_y - pPrev->dest_y) * fT +
				                 pPrev->dest_y;
				pOut->source_x = (pCur->source_x - pPrev->source_x) * fT +
				                 pPrev->source_x;
				pOut->source_y = (pCur->source_y - pPrev->source_y) * fT +
				                 pPrev->source_y;

				pOut++;
			}

			nPrev = i;
			pPrev = pCur;
		}

		*pnPts = pOut - pOutStart;
		*ppPts = pOutStart;
	}

	return LTTRUE;
}

// Passes 2 and 3: the same loop on another plane, the interpolation called.
#define CIS_CLIP_PASS_Q40E(NAME, CLIPPER, TEST)                                  \
inline LTBOOL NAME(CLIPPER clipper, LTWarpPt** ppPts, int* pnPts)                \
{                                                                                \
	LTBOOL bInside[50];                                                          \
	int nInside;                                                                 \
	int nPrev;                                                                   \
	int i;                                                                       \
                                                                                 \
	LTWarpPt* pOutStart;                                                         \
	LTWarpPt* pOut;                                                              \
	LTWarpPt* pPrev;                                                             \
	LTWarpPt* pCur;                                                              \
                                                                                 \
	nInside = 0;                                                                 \
                                                                                 \
	for (i = 0; i < *pnPts; i++)                                                 \
	{                                                                            \
		LTBOOL bIn = TEST((*ppPts)[i]);                                          \
		bInside[i] = bIn ? LTTRUE : LTFALSE;                                     \
                                                                                 \
		nInside += bInside[i];                                                   \
	}                                                                            \
                                                                                 \
	if (!nInside)                                                                \
		return LTFALSE;                                                          \
                                                                                 \
	if (nInside != *pnPts)                                                       \
	{                                                                            \
		pOut = g_WarpClipBufferQ438[g_nWarpClipBufferQ438];                      \
		pOutStart = pOut;                                                        \
                                                                                 \
		g_nWarpClipBufferQ438 = !g_nWarpClipBufferQ438;                          \
                                                                                 \
		nPrev = *pnPts - 1;                                                      \
		pPrev = &(*ppPts)[nPrev];                                                \
                                                                                 \
		for (i = 0; i < *pnPts; i++)                                             \
		{                                                                        \
			pCur = &(*ppPts)[i];                                                 \
                                                                                 \
			if (bInside[nPrev])                                                  \
			{                                                                    \
				*pOut = *pPrev;                                                  \
                                                                                 \
				pOut++;                                                          \
			}                                                                    \
                                                                                 \
			if (bInside[nPrev] != bInside[i])                                    \
			{                                                                    \
				clipper.InterpolateQ438((LTWarpPtQ438*)pPrev,                    \
				                        (LTWarpPtQ438*)pCur,                     \
				                        (LTWarpPtQ438*)pOut);                    \
                                                                                 \
				pOut++;                                                          \
			}                                                                    \
                                                                                 \
			nPrev = i;                                                           \
			pPrev = pCur;                                                        \
		}                                                                        \
                                                                                 \
		*pnPts = pOut - pOutStart;                                               \
		*ppPts = pOutStart;                                                      \
	}                                                                            \
                                                                                 \
	return LTTRUE;                                                               \
}

#define CIS_INSIDE_TOP_Q40E(pt)    !((pt).dest_y < g_fWarpClipTopQ438)
#define CIS_INSIDE_RIGHT_Q40E(pt)  ((pt).dest_x < g_fWarpClipRightQ438)

CIS_CLIP_PASS_Q40E(cis_ClipTopQ40E, CWarpClipTopQ438, CIS_INSIDE_TOP_Q40E)
CIS_CLIP_PASS_Q40E(cis_ClipRightQ40E, CWarpClipRightQ438, CIS_INSIDE_RIGHT_Q40E)

LTBOOL cis_ClipWarpPolyQ40E(LTWarpPt** ppPts, int* pnPts, float fLeft,
                            float fTop, float fRight, float fBottom)
{
	int nUnused1, nUnused2;

	g_fWarpClipLeftQ40E   = fLeft;
	g_fWarpClipTopQ438    = fTop;
	g_fWarpClipRightQ438  = fRight;
	g_fWarpClipBottomQ438 = fBottom;

	g_nWarpClipBufferQ438 = 0;

	if (cis_ClipLeftQ40E(ppPts, pnPts))
	{
		if (cis_ClipTopQ40E(CWarpClipTopQ438(), ppPts, pnPts))
		{
			if (cis_ClipRightQ40E(CWarpClipRightQ438(), ppPts, pnPts))
				return cis_ClipWarpDestBottomQ438(nUnused1, nUnused2, ppPts, pnPts) ? LTTRUE : LTFALSE;
		}
	}

	return LTFALSE;
}
void   cis_SetupWarpEdgesQ40E(SWarpEdgeQ40E* pLeft, SWarpEdgeQ40E* pRight,
                              LTWarpPt* pPts, int nPts,
                              uint32* pMinY, uint32* pMaxY);           // 0x0043a390

LTRESULT sub_40e120(HSURFACE hDest, HSURFACE hSrc, LTWarpPt* pCoords,
                    int nCoords, void* pWarpFn)
{
	CSurface*     pDest;
	CSurface*     pSrc;
	int           i;
	uint32        nMinY, nMaxY;
#ifdef NOLFSRC_MODERN
	// fixed: the edge tables are indexed by the destination's row, and
	// retail's held 1024 rows on the stack: a warp onto rows 1024 and down
	// of a taller surface (the screen at 1080p and up) wrote past them, over
	// this function's own arguments, and the call below jumped to whatever
	// pWarpFn had become (RETAIL_BUGS.md). Static tables, for screens up to
	// 8192 rows; the warps all run on the game's thread.
	static SWarpEdgeQ40E right[8192];
	static SWarpEdgeQ40E left[8192];
#else
	SWarpEdgeQ40E right[1024];
	SWarpEdgeQ40E left[1024];
#endif

	pDest = (CSurface*)hDest;
	pSrc  = (CSurface*)hSrc;

	if (!pSrc || !pDest)                                            // 0x0040e136
		RETURN_ERROR(1, "InternalWarpSurfaceToSurface", LT_INVALIDPARAMS);
#ifdef NOLFSRC_MODERN
	if (pDest->m_nHeight > sizeof(left) / sizeof(left[0]))
		RETURN_ERROR(1, "InternalWarpSurfaceToSurface", LT_INVALIDPARAMS);
#endif

	if (nCoords > MAX_WARP_POINTS_Q40E)                             // 0x0040e154
		nCoords = MAX_WARP_POINTS_Q40E;
	else if (nCoords < 3)                                           // 0x0040e191
		RETURN_ERROR_SYM_DESC(1, "InternalWarpSurfaceToSurface",
		                      LT_INVALIDPARAMS, "nCoords < 3")

	for (i = 0; i < nCoords; i++)                                   // 0x0040e168
	{
		pCoords[i].source_x = LTCLAMP(pCoords[i].source_x, 0.0f,
		                              (float)(pSrc->m_nWidth - 1));
		pCoords[i].source_y = LTCLAMP(pCoords[i].source_y, 0.0f,
		                              (float)(pSrc->m_nHeight - 1));
	}

	if (!cis_ClipWarpPolyQ40E(&pCoords, &nCoords, 0.0f, 0.0f,
	                          (float)(pDest->m_nWidth - 1),
	                          (float)(pDest->m_nHeight - 1)))       // 0x0040e275
		return LT_OK;

	cis_SetupWarpEdgesQ40E(left, right, pCoords, nCoords,
	                       &nMinY, &nMaxY);                         // 0x0040e2b3

	return ((WarpFnQ40E)pWarpFn)(hDest, hSrc, left, right,
	                             nMinY, nMaxY);                     // 0x0040e2d1
}

// No driver name means DirectPlay, and asking the question is enough to make
// the driver exist.
LTRESULT ci_IsLobbyLaunched(char* sDriver)
{
	CBaseDriver* pDriver;

	if (!sDriver)
		sDriver = "dplay2";

	pDriver = g_pClientMgr->m_NetMgr.FindDriver(sDriver);

	if (!pDriver)
	{
		pDriver = g_pClientMgr->m_NetMgr.AddDriver(sDriver);

		if (!pDriver)
			RETURN_ERROR(1, "CLTClient::IsLobbyLaunched", LT_ERROR);
	}

	return !pDriver->IsLobbyLaunched();
}

LTBOOL ci_AddSongToPlayList(char* szPlayList, char* szSong)
{
	if (ci_GetMusicMgr()->m_pMusicDLL && szPlayList && szSong)
	{
		void* pPlayList;
		void* pSong;

		pPlayList = ci_GetMusicMgr()->Fn_24(szPlayList);

		if (!pPlayList)
		{
			pPlayList = ci_GetMusicMgr()->Fn_20(szPlayList);

			if (!pPlayList)
				return LTFALSE;
		}

		pSong = ci_GetMusicMgr()->Fn_14(szSong);

		if (!pSong)
		{
			pSong = ci_GetMusicMgr()->Fn_10(szSong);

			if (!pSong)
				return LTFALSE;
		}

		return ci_GetMusicMgr()->Fn_28(pPlayList, pSong);
	}

	return LTFALSE;
}

// Removing by name until the lookup comes up empty, because a name can have
// been registered more than once; a name that was never registered is the
// only failure.
LTRESULT ci_UnregisterConsoleProgram(char* pName)
{
	ConsoleCommand* pCmd;
	int nRemoved = 0;

	pCmd = con_FindCommand(&g_ClientConsoleState, pName);

	while (pCmd)
	{
		nRemoved++;
		con_RemoveCommand(&g_ClientConsoleState, pCmd);
		pCmd = con_FindCommand(&g_ClientConsoleState, pName);
	}

	if (!nRemoved)
		RETURN_ERROR(1, "CLTClient::UnregisterConsoleProgram", LT_NOTFOUND);

	return LT_OK;
}

// The one check site in the file that says what went wrong: the description
// argument the printer usually gets as a blank space carries "invalid
// PolyGrid" here, and the code is reported by name rather than by number.
LTRESULT ci_SetPolyGridTextureInfo(HLOCALOBJ hObj, float xPan, float yPan,
                                   float xScale, float yScale)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_POLYGRID)
	{
		((CClientPolyGrid*)hObj)->m_fXPan = xPan;
		((CClientPolyGrid*)hObj)->m_fYPan = yPan;
		((CClientPolyGrid*)hObj)->m_fXScale = xScale;
		((CClientPolyGrid*)hObj)->m_fYScale = yScale;
		return LT_OK;
	}

	RETURN_ERROR_SYM_DESC(1, "CLTClient::SetPolyGridTextureInfo", LT_ERROR, "invalid PolyGrid")
}

LTRESULT ci_ScaleSurfaceToSurfaceTransparent(HSURFACE hDest, HSURFACE hSrc,
                                             LTRect* pDestRect, LTRect* pSrcRect,
                                             HLTCOLOR hColor)
{
	if (!g_pRender)
		RETURN_ERROR(1, "ScaleSurfaceToSurfaceTransparent", LT_NOTINITIALIZED);

	CI_FMCONVERT()->PValueToFormatColor((FMFormat*)&g_ScreenFormat, hColor, &g_TransColorQ40C);
	return sub_40dcc0(hDest, hSrc, pDestRect, pSrcRect, 0, hColor, 0);
}

LTRESULT ci_WarpSurfaceToSurfaceTransparent(HSURFACE hDest, HSURFACE hSrc,
                                            LTWarpPt* pCoords, int nCoords,
                                            HLTCOLOR hColor)
{
	if (!g_pRender)
		RETURN_ERROR(1, "WarpSurfaceToSurfaceTransparent", LT_NOTINITIALIZED);

	CI_FMCONVERT()->PValueToFormatColor((FMFormat*)&g_ScreenFormat, hColor, &g_TransColorQ40C);
	return sub_40e120(hDest, hSrc, pCoords, nCoords, (void*)sub_43ad40);
}

// 0x0040e4d0, 1204 bytes -- the transform worker both TransformSurface
// entries forward to ("InternalTransformSurfaceToSurface").  The source
// rectangle is placed with its origin at the given point (the source's
// centre when there is none), scaled, rotated about that point, and handed
// on as a four-point warp: to the renderer's warp-to-screen entry
// (RenderStruct +0xf0) when drawing to the screen, otherwise to the two
// software warps.  nMode non-zero is the transparent flavour.
LTRESULT sub_40e4d0(HSURFACE hDest, HSURFACE hSrc, LTFloatPt* pOrigin, int destX,
                    int destY, float angle, float scaleX, float scaleY,
                    int nMode, HLTCOLOR hTransColor)
{
	CIBlitRequest request;
	LTFloatPt     origin;
	LTFloatPt     pts[4];
	LTWarpPt      warpPts[4];
	CSurface*     pDest = (CSurface*)hDest;
	CSurface*     pSrc = (CSurface*)hSrc;
	float         fDestX, fDestY, fLeft, fTop;
	float         fCos, fSin, fCenterX, fCenterY;
	int           i;

	if (hSrc && hDest)
	{
		if (pOrigin)
		{
			origin = *pOrigin;
		}
		else
		{
			origin.x = pSrc->m_nWidth * 0.5f + destX;
			origin.y = pSrc->m_nHeight * 0.5f + destY;
		}

		fDestX = (float)destX;
		fLeft  = fDestX - origin.x;
		fDestY = (float)destY;
		fTop   = fDestY - origin.y;

		pts[0].x = fLeft;
		pts[0].y = fTop;
		pts[1].x = fLeft + pSrc->m_nWidth;
		pts[1].y = fTop;
		pts[2].x = fLeft + pSrc->m_nWidth;
		pts[2].y = fTop + pSrc->m_nHeight;
		pts[3].x = fLeft;
		pts[3].y = fTop + pSrc->m_nHeight;

		fCos = (float)cos(angle);
		fSin = (float)sin(angle);

		fCenterX = fDestX - fLeft;
		fCenterY = fDestY - fTop;

		for (i = 0; i < 4; i++)
		{
			pts[i].x *= scaleX;
			pts[i].y *= scaleY;

			warpPts[i].dest_x = fCos * pts[i].x - pts[i].y * fSin;
			warpPts[i].dest_y = pts[i].y * fCos + fSin * pts[i].x;

			warpPts[i].dest_x += fCenterX;
			warpPts[i].dest_y += fCenterY;
		}

		warpPts[0].source_y = 0.0f;
		warpPts[0].source_x = 0.0f;
		warpPts[1].source_x = (float)(pSrc->m_nWidth - 1);
		warpPts[1].source_y = 0.0f;
		warpPts[2].source_x = (float)(pSrc->m_nWidth - 1);
		warpPts[2].source_y = (float)(pSrc->m_nHeight - 1);
		warpPts[3].source_x = 0.0f;
		warpPts[3].source_y = (float)(pSrc->m_nHeight - 1);

		if (hDest == (HSURFACE)&g_ScreenSurface && g_RenderStruct.WarpToScreen)
		{
			warpPts[0].source_x = LTCLAMP(warpPts[0].source_x, 0.0f,
			                              (float)(pSrc->m_nWidth - 1));
			warpPts[0].source_y = LTCLAMP(warpPts[0].source_y, 0.0f,
			                              (float)(pSrc->m_nHeight - 1));
			warpPts[1].source_x = LTCLAMP(warpPts[1].source_x, 0.0f,
			                              (float)(pSrc->m_nWidth - 1));
			warpPts[1].source_y = LTCLAMP(warpPts[1].source_y, 0.0f,
			                              (float)(pSrc->m_nHeight - 1));
			warpPts[2].source_x = LTCLAMP(warpPts[2].source_x, 0.0f,
			                              (float)(pSrc->m_nWidth - 1));
			warpPts[2].source_y = LTCLAMP(warpPts[2].source_y, 0.0f,
			                              (float)(pSrc->m_nHeight - 1));
			warpPts[3].source_x = LTCLAMP(warpPts[3].source_x, 0.0f,
			                              (float)(pSrc->m_nWidth - 1));
			warpPts[3].source_y = LTCLAMP(warpPts[3].source_y, 0.0f,
			                              (float)(pSrc->m_nHeight - 1));

			LTRect srcRect(0, 0, pSrc->m_nWidth - 1, pSrc->m_nHeight - 1);
			LTRect destRect(0, 0, pDest->m_nWidth - 1, pDest->m_nHeight - 1);

			sub_40d7d0((CSurfaceView*)hSrc);

			request.m_pSurfaceData = pSrc->m_pRenderData;
			request.m_pUnknown08   = (void*)g_TransColorQ40C;
			request.m_pSrcRect     = &srcRect;
			request.m_pDestRect    = &destRect;
			request.m_bTransparent = (nMode != 0);
			request.m_fAlpha       = pSrc->m_fAlpha;
			request.m_pPoints      = warpPts;
			request.m_nPoints      = 4;

			if (g_RenderStruct.WarpToScreen(&request))
			{
				pDest->m_nFlags |= 4;
				return LT_OK;
			}
		}

		if (nMode)
			return ci_WarpSurfaceToSurfaceTransparent(hDest, hSrc, warpPts, 4, hTransColor);
		else
			return ci_WarpSurfaceToSurface(hDest, hSrc, warpPts, 4);
	}

	RETURN_ERROR(1, "InternalTransformSurfaceToSurface", LT_INVALIDPARAMS);
}

LTRESULT ci_TransformSurfaceToSurface(HSURFACE hDest, HSURFACE hSrc, LTFloatPt* pOrigin,
                                      int destX, int destY, float angle, float scaleX,
                                      float scaleY)
{
	if (!g_pRender)
		RETURN_ERROR(1, "TransformSurfaceToSurface", LT_NOTINITIALIZED);

	return sub_40e4d0(hDest, hSrc, pOrigin, destX, destY, angle, scaleX, scaleY, 0, 0);
}

LTRESULT ci_TransformSurfaceToSurfaceTransparent(HSURFACE hDest, HSURFACE hSrc,
                                                 LTFloatPt* pOrigin, int destX, int destY,
                                                 float angle, float scaleX, float scaleY,
                                                 HLTCOLOR hColor)
{
	if (!g_pRender)
		RETURN_ERROR(1, "TransformSurfaceToSurfaceTransparent", LT_NOTINITIALIZED);

	CI_FMCONVERT()->PValueToFormatColor((FMFormat*)&g_ScreenFormat, hColor, &g_TransColorQ40C);
	return sub_40e4d0(hDest, hSrc, pOrigin, destX, destY, angle, scaleX, scaleY, 1, hColor);
}

LTRESULT ci_Start3D()
{
	if (!g_RenderStruct.m_bInited)
		RETURN_ERROR(1, "Start3D", LT_NOTINITIALIZED);

	if (g_RenderStruct.IsIn3D())
		RETURN_ERROR(1, "Start3D", LT_ALREADYIN3D);

	g_RenderStruct.Start3D();
	return LT_OK;
}

LTRESULT ci_StartOptimized2D()
{
	if (!g_RenderStruct.m_bInited)
		RETURN_ERROR(1, "StartOptimized2D", LT_NOTINITIALIZED);

	if (!g_RenderStruct.IsIn3D())
		RETURN_ERROR(1, "StartOptimized2D", LT_NOTIN3D);

	g_RenderStruct.StartOptimized2D();
	return LT_OK;
}

LTRESULT ci_SetOptimized2DColor(HLTCOLOR hColor)
{
	if (!g_RenderStruct.m_bInited)
		RETURN_ERROR(1, "SetOptimized2DColor", LT_NOTINITIALIZED);

	if (!g_RenderStruct.IsIn3D())
		RETURN_ERROR(1, "SetOptimized2DColor", LT_NOTIN3D);

	return !g_RenderStruct.SetOptimized2DColor(hColor);
}

// Reported at level 15, which is as good as silent: nothing sets the debug
// level that high.
LTRESULT ci_RenderCamera(HLOCALOBJ hCamera)
{
	if (!g_RenderStruct.m_bInited)
#ifdef NOLFSRC_MODERN
	// fixed: checks reported at the wrong debug level (RETAIL_BUGS.md).
		RETURN_ERROR(1, "RenderCamera", LT_NOTINITIALIZED);
#else
		RETURN_ERROR(15, "RenderCamera", LT_NOTINITIALIZED);
#endif

	if (!g_RenderStruct.IsIn3D())
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "RenderCamera", LT_NOTIN3D);
#else
		RETURN_ERROR(15, "RenderCamera", LT_NOTIN3D);
#endif

	if (hCamera && ((CClientObject*)hCamera)->m_ObjectType == OT_CAMERA)
	{
		sub_411700(g_pClientMgr, hCamera, 1, LTNULL, 0);
		return LT_OK;
	}

#ifdef NOLFSRC_MODERN
	RETURN_ERROR(1, "RenderCamera", LT_INVALIDPARAMS);
#else
	RETURN_ERROR(15, "RenderCamera", LT_INVALIDPARAMS);
#endif
}

LTRESULT ci_End3D()
{
	if (!g_RenderStruct.m_bInited)
		RETURN_ERROR(1, "End3D", LT_NOTINITIALIZED);

	if (!g_RenderStruct.IsIn3D())
		RETURN_ERROR(1, "End3D", LT_NOTIN3D);

	g_RenderStruct.End3D();
	return LT_OK;
}

LTRESULT ci_EndOptimized2D()
{
	if (!g_RenderStruct.m_bInited)
		RETURN_ERROR(1, "EndOptimized2D", LT_NOTINITIALIZED);

	if (!g_RenderStruct.IsIn3D())
		RETURN_ERROR(1, "EndOptimized2D", LT_NOTIN3D);

	g_RenderStruct.EndOptimized2D();
	return LT_OK;
}

// Three of the four check sites here report themselves as RenderCamera; only
// the last one, on the render call itself, uses this function's own name. An
// empty object list is not an error - it just does nothing.
LTRESULT ci_RenderObjects(HLOCALOBJ hCamera, HLOCALOBJ* pObjects, int nObjects)
{
	if (!g_RenderStruct.m_bInited)
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "RenderObjects", LT_NOTINITIALIZED);
#else
		RETURN_ERROR(1, "RenderCamera", LT_NOTINITIALIZED);
#endif

	if (!g_RenderStruct.IsIn3D())
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "RenderObjects", LT_NOTIN3D);
#else
		RETURN_ERROR(1, "RenderCamera", LT_NOTIN3D);
#endif

	if (!hCamera || ((CClientObject*)hCamera)->m_ObjectType != OT_CAMERA)
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "RenderObjects", LT_INVALIDPARAMS);
#else
		RETURN_ERROR(1, "RenderCamera", LT_INVALIDPARAMS);
#endif

	if (!pObjects || nObjects <= 0)
		return LT_OK;

	if (sub_411700(g_pClientMgr, hCamera, 2, pObjects, nObjects))
		return LT_OK;

	RETURN_ERROR(1, "RenderObjects", LT_ERROR);
}

// The node handles are indices, so walking them is counting; the invalid
// handle is -1 so that the first step lands on zero, and VC6 spends a
// two-byte `or %ecx,%ecx` rather than reloading a value the compare it just
// made already proved.
//
// A near: the whole function comes out size-exact with the object handle and
// the node index in each other's registers - shipped keeps the handle in %eax
// through the guard and moves the model over it, ours keeps the handle in
// %ecx - and no ordering of the two loads flips it back.  Also measured with
// no change (11 insns): modifying hNode in place as commonmodel.cpp does, a
// guard-first refusal with one result local, a ternary for the -1, and
// m_NodeTransforms.GetSize(); an iNode local initialised first, a pModel
// assignment after the -1 test, or no pModel local made it worse.
// NOT COMPILED: server/commonmodel.cpp's ci_GetNextModelNode is the one
// lithtech.exe has (0x00436370, among commonmodel's ci_ run); kept as the
// near described above.
#if 0
LTRESULT ci_GetNextModelNode(HLOCALOBJ hObject, HMODELNODE hNode, HMODELNODE* pNext)
{
	if (hObject && ((CClientObject*)hObject)->m_ObjectType == OT_MODEL)
	{
		uint32 iNode;
		Model* pModel = ((CClientModel*)hObject)->m_pModel;

		if (hNode == INVALID_MODEL_NODE)
			iNode = (uint32)-1;
		else
			iNode = hNode;

		iNode++;

		// Same bound as ci_GetModelNodeName: Model+0x78, which is
		// m_NodeTransforms' count, not m_Nodes'.  Read off 0x00436392.
		if (iNode >= pModel->m_NodeTransforms.m_nElements)
			return LT_FINISHED;

		*pNext = iNode;
		return LT_OK;
	}

	RETURN_ERROR(1, "GetNextModelNode", LT_INVALIDPARAMS);
}
#endif

// The reciprocal is a literal in the source, not a division: the shipped code
// multiplies by 1/255 rather than dividing by 255.
void ci_GetObjectColor(HLOCALOBJ hObject, float* r, float* g, float* b, float* a)
{
	if (hObject)
	{
		if (r)
			*r = ((CClientObject*)hObject)->m_ColorR * (1.0f / 255.0f);

		if (g)
			*g = ((CClientObject*)hObject)->m_ColorG * (1.0f / 255.0f);

		if (b)
			*b = ((CClientObject*)hObject)->m_ColorB * (1.0f / 255.0f);

		if (a)
			*a = ((CClientObject*)hObject)->m_ColorA * (1.0f / 255.0f);
	}
}

// User flags belong to whoever created the object, so an object that mirrors
// a server one refuses.
LTRESULT ci_SetObjectUserFlags(HLOCALOBJ hObj, uint32 flags)
{
	if (!hObj)
		RETURN_ERROR(1, "CLTClient::SetObjectUserFlags", LT_INVALIDPARAMS);

	if (((CClientObject*)hObj)->m_nServerID == 0xffff)
	{
		((CClientObject*)hObj)->m_UserFlags = flags;
		return LT_OK;
	}

	RETURN_ERROR(1, "CLTClient::SetObjectUserFlags", LT_ERROR);
}

LTRESULT ci_SetSoftwarePSColor(HLOCALOBJ hObj, float r, float g, float b)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_PARTICLESYSTEM)
	{
		((CClientParticleSystem*)hObj)->m_SoftwareR = (uint8)(r * 255.0f);
		((CClientParticleSystem*)hObj)->m_SoftwareG = (uint8)(g * 255.0f);
		((CClientParticleSystem*)hObj)->m_SoftwareB = (uint8)(b * 255.0f);
		return LT_OK;
	}

	RETURN_ERROR(1, "CLTClient::SetSoftwarePSColor", LT_INVALIDPARAMS);
}

LTRESULT ci_DrawSurfaceToSurfaceTransparent(HSURFACE hDest, HSURFACE hSrc, LTRect* pSrcRect,
                                            int destX, int destY, HLTCOLOR hColor)
{
	if (!g_pRender)
		RETURN_ERROR(1, "DrawSurfaceToSurfaceTransparent", LT_NOTINITIALIZED);

	CI_FMCONVERT()->PValueToFormatColor((FMFormat*)&g_ScreenFormat, hColor, &g_TransColorQ40C);
	return sub_40d290(hDest, hSrc, pSrcRect, destX, destY, (void*)sub_40d710);
}

// The tag is GetSurfaceAlpha's: both functions declare their own static name
// pointer and both were given the same literal, which is why the two pointers
// hold the same address.
LTRESULT ci_SetSurfaceAlpha(HSURFACE hSurface, float alpha)
{
	if (!hSurface)
#ifdef NOLFSRC_MODERN
		RETURN_ERROR_NAMED(2, "ILTClient::SetSurfaceAlpha", LT_INVALIDPARAMS);
#else
		RETURN_ERROR_NAMED(2, "ILTClient::GetSurfaceAlpha", LT_INVALIDPARAMS);
#endif

	((CSurface*)hSurface)->m_fAlpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
	return LT_OK;
}

// Both angles are clamped to a hundredth of pi either side of a full turn,
// and the clamp writes back over the argument rather than into a local.
void ci_SetCameraFOV(HLOCALOBJ hObj, float fovX, float fovY)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_CAMERA)
	{
		if (fovX < MIN_FOV)
			fovX = MIN_FOV;
		else if (fovX > MAX_FOV)
			fovX = MAX_FOV;

		if (fovY < MIN_FOV)
			fovY = MIN_FOV;
		else if (fovY > MAX_FOV)
			fovY = MAX_FOV;

		((CClientCamera*)hObj)->m_fFovX = fovX;
		((CClientCamera*)hObj)->m_fFovY = fovY;
	}
}

// Reported at debug level zero, so this one always prints.
LTRESULT ci_GetPolyGridInfo(HLOCALOBJ hObj, char** pBytes, uint32* pWidth, uint32* pHeight,
                            PGColor** pColorTable)
{
	if (hObj && ((CClientObject*)hObj)->m_ObjectType == OT_POLYGRID
	    && pBytes && pWidth && pHeight && pColorTable)
	{
		*pBytes = ((CClientPolyGrid*)hObj)->m_pBytes;
		*pWidth = ((CClientPolyGrid*)hObj)->m_nWidth;
		*pHeight = ((CClientPolyGrid*)hObj)->m_nHeight;
		*pColorTable = ((CClientPolyGrid*)hObj)->m_ColorTable;
		return LT_OK;
	}

#ifdef NOLFSRC_MODERN
	// fixed: checks reported at the wrong debug level (RETAIL_BUGS.md).
	RETURN_ERROR(1, "GetPolyGridInfo", LT_INVALIDPARAMS);
#else
	RETURN_ERROR(0, "GetPolyGridInfo", LT_INVALIDPARAMS);
#endif
}

// Two colours, two conversions, two destination globals - which is the
// clearest evidence yet that the call ahead of each transparent blit is
// putting a colour somewhere the blitter reads it from.
LTRESULT ci_WarpSurfaceToSurfaceSolidColor(HSURFACE hDest, HSURFACE hSrc, LTWarpPt* pCoords,
                                           int nCoords, HLTCOLOR hTransColor,
                                           HLTCOLOR hFillColor)
{
	if (!g_pRender)
		RETURN_ERROR(1, "WarpSurfaceToSurfaceSolidColor", LT_NOTINITIALIZED);

	CI_FMCONVERT()->PValueToFormatColor((FMFormat*)&g_ScreenFormat, hTransColor, &g_TransColorQ40C);
	CI_FMCONVERT()->PValueToFormatColor((FMFormat*)&g_ScreenFormat, hFillColor, &g_FillColorQ40C);
	return sub_40e120(hDest, hSrc, pCoords, nCoords, (void*)sub_43aff0);
}

// Node zero has no name as far as this function is concerned: the handle is
// checked for null before it is checked against the count. Both complaints
// come out as GetNextModelNode, and the copy is not terminated - maxLen - 1
// characters go across and nothing writes the nought.
// NOT COMPILED: server/commonmodel.cpp's ci_GetModelNodeName is the one
// lithtech.exe has (0x004363f0, byte-identical to this one).
#if 0
LTRESULT ci_GetModelNodeName(HLOCALOBJ hObject, HMODELNODE hNode, char* pName, uint32 maxLen)
{
	Model* pModel;

#ifdef NOLFSRC_MODERN
	// fixed: node 0 is valid, and the copy is terminated (RETAIL_BUGS.md).
	if (!maxLen || !hObject
	    || ((CClientObject*)hObject)->m_ObjectType != OT_MODEL)
#else
	if (!hNode || !maxLen || !hObject
	    || ((CClientObject*)hObject)->m_ObjectType != OT_MODEL)
#endif
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "GetModelNodeName", LT_INVALIDPARAMS);
#else
		RETURN_ERROR(1, "GetNextModelNode", LT_INVALIDPARAMS);
#endif

	pModel = ((CClientModel*)hObject)->m_pModel;

	// The bound is m_NodeTransforms' count, NOT m_Nodes' -- the shipped
	// compare reads Model+0x78, which is m_NodeTransforms (0x070) plus
	// CMoArray's m_nElements at +0x08; m_Nodes' count would be +0x2c.  The
	// two are normally equal (commonmodel.cpp:1843 tests exactly that), so
	// the choice is invisible at runtime and visible in one displacement
	// byte.
	if (hNode >= pModel->m_NodeTransforms.m_nElements)
#ifdef NOLFSRC_MODERN
		RETURN_ERROR(1, "GetModelNodeName", LT_INVALIDPARAMS);
#else
		RETURN_ERROR(1, "GetNextModelNode", LT_INVALIDPARAMS);
#endif

	strncpy(pName, pModel->m_Nodes.m_pArray[hNode]->m_pName, maxLen - 1);
#ifdef NOLFSRC_MODERN
	pName[maxLen - 1] = '\0';
#endif
	return LT_OK;
}
#endif

// The creator wants one file reference per file the object might need - its
// own, and one per skin - and hands back the object through an out-parameter,
// so a failure has to be turned into a null handle here.
HLOCALOBJ ci_CreateObject(ObjectCreateStruct* pStruct)
{
	CClientCreateRequest req;
	HLOCALOBJ            hObject;
	LTRESULT             dResult;
	uint32               i;

	req.m_ObjectFile.m_nFileType = 1;
	req.m_pStruct = pStruct;
	req.m_ObjectFile.m_pFilename = pStruct->m_Filename;

	for (i = 0; i < MAX_MODEL_TEXTURES; i++)
	{
		req.m_SkinFiles[i].m_pFilename = pStruct->m_SkinNames[i];
		req.m_SkinFiles[i].m_nFileType = 1;
	}

	dResult = sub_411a70(g_pClientMgr, 0xffff, &req, &hObject, 1, 1);
	return dResult ? LTNULL : hObject;
}

// The top bit of an HLTCOLOR is the one CreateColor sets for a transparent
// colour; a colour without it means the surface has no transparency at all.
LTRESULT ci_OptimizeSurface(HSURFACE hSurface, HLTCOLOR hTransparentColor)
{
	CSurface* pSurface = (CSurface*)hSurface;

	if (!hSurface)
		RETURN_ERROR(1, "OptimizeSurface", LT_INVALIDPARAMS);

	pSurface->m_hTransColor = hTransparentColor;

	if (hTransparentColor & 0x80000000)
		pSurface->m_hTransColor = hTransparentColor & 0x7fffffff;
	else
		pSurface->m_hTransColor = 0xffffffff;

	pSurface->m_nFlags |= 2;

	if (g_pRender && g_pRender->m_nCanOptimizeSurfaces)
	{
		if (g_pRender->OptimizeSurface(pSurface->m_pRenderData, pSurface->m_hTransColor))
			pSurface->m_nFlags &= ~4;
		else
			return LT_ERROR;
	}

	return LT_OK;
}

// A transition is optional, and the pair-of-lookups shape applies to it the
// same way it does to a plain song.
LTBOOL ci_PlayList(char* szPlayList, char* szTransition, LTBOOL bLoop, uint32 dwBoundaryFlags)
{
	void* pPlayList;
	void* pTransition;

	if (!ci_GetMusicMgr()->m_pMusicDLL)
		return LTFALSE;

	if (!szPlayList)
		return LTFALSE;

	pPlayList = ci_GetMusicMgr()->Fn_24(szPlayList);

	if (!pPlayList)
		return LTFALSE;

	pTransition = LTNULL;

	if (szTransition)
	{
		pTransition = ci_GetMusicMgr()->Fn_14(szTransition);

		if (!pTransition)
		{
			pTransition = ci_GetMusicMgr()->Fn_10(szTransition);

			if (!pTransition)
				return LTFALSE;
		}
	}

	return ci_GetMusicMgr()->Fn_34(pPlayList, pTransition, bLoop, dwBoundaryFlags);
}

// The three numbers go straight onto the system; only the texture can fail,
// and it fails as LT_NOTFOUND.
LTRESULT ci_SetupParticleSystem(HLOCALOBJ hObj, char* pTextureName, float gravityAccel,
                                uint32 flags, float particleRadius)
{
	if (!hObj || ((CClientObject*)hObj)->m_ObjectType != OT_PARTICLESYSTEM || !pTextureName)
		RETURN_ERROR(1, "SetupParticleSystem", LT_INVALIDPARAMS);

	((CClientParticleSystem*)hObj)->m_fGravityAccel = gravityAccel;
	((CClientParticleSystem*)hObj)->m_nPSFlags = flags;
	((CClientParticleSystem*)hObj)->m_fParticleRadius = particleRadius;

	{
		LTRESULT dResult = sub_45c680(hObj, g_pClientMgr, pTextureName);

		if (!dResult)
			return dResult;

		RETURN_ERROR(1, "SetupParticleSystem", LT_NOTFOUND);
	}
}

// The texture is asked for through a file reference built on the stack, and
// the grid keeps whatever comes back; the second call hands that texture to
// something the grid also carries, four bytes further on.
LTRESULT ci_SetPolyGridTexture(HLOCALOBJ hObj, char* pFilename)
{
	CClientFileRef ref;

	if (!hObj || ((CClientObject*)hObj)->m_ObjectType != OT_POLYGRID)
		RETURN_ERROR_SYM_DESC(1, "CLTClient::SetPolyGridTexture", LT_ERROR, "invalid PolyGrid")

	{
		CClientPolyGrid* pGrid = (CClientPolyGrid*)hObj;
		LTRESULT         dResult;

		ref.m_nFileType = 1;
		ref.m_pFilename = pFilename;

		dResult = sub_476e80(g_pClientMgr, &ref, &pGrid->m_pTexture);

		if (dResult)
			return dResult;

		sub_483cc0(&pGrid->m_Unknown1b4, pGrid->m_pTexture);
		return LT_OK;
	}
}

// Like IsLobbyLaunched, this creates the driver if it is not already there,
// and refuses inside that block when the creation fails, exactly as its twin
// does. The two identical LT_ERROR reports are merged by VC6 into one, which
// is what puts the LT_OK return at the very end: folding both refusals into
// a single `!pDriver || !...` test lays the LT_OK return out first instead.
LTRESULT ci_GetLobbyLaunchInfo(char* sDriver, void** ppLobbyLaunchData)
{
	CBaseDriver* pDriver;

	if (!ppLobbyLaunchData)
		RETURN_ERROR(1, "CLTClient::GetLobbyLaunchInfo", LT_INVALIDPARAMS);

	if (!sDriver)
		sDriver = "dplay2";

	pDriver = g_pClientMgr->m_NetMgr.FindDriver(sDriver);

	if (!pDriver)
	{
		pDriver = g_pClientMgr->m_NetMgr.AddDriver(sDriver);

		if (!pDriver)
			RETURN_ERROR(1, "CLTClient::GetLobbyLaunchInfo", LT_ERROR);
	}

	if (!pDriver->GetLobbyLaunchInfo(ppLobbyLaunchData))
		RETURN_ERROR(1, "CLTClient::GetLobbyLaunchInfo", LT_ERROR);

	return LT_OK;
}


// 0x0040c6f0 -- ILTClient::SetPixel. HOME: src/engine/lithtech/cissurf.cpp
// (the cis_ run; GetPixel 0x0040c5f0 is its twin just above it). Both refusals are RETURN_ERROR at level 1 with the numeric
// code strings: "60" (0x004a7160) for a null surface or a coordinate out
// of range (`jae`: unsigned), "1" (0x004a7494) when the renderer will not
// lock it. The pixel is renderer bits + y * pitch (+0x18) + x *
// g_nBytesPerPixel (0x004b1794); the colour is converted into it by the
// blitter sub-object (0x0040c120 -> 0x0045f5e0) against the screen format
// 0x004b1758. Unlock (render +0xd8), then the surface's bit 4 (+0x1c) says
// it has been written.
class CSurfaceQ19
{
public:
	uint8  m_pad_00[0xc];
	void*  m_pRenderData;                    // 0x00c
	uint32 m_nWidth;                         // 0x010
	uint32 m_nHeight;                        // 0x014
	uint32 m_nPitch;                         // 0x018
	uint32 m_nFlags;                         // 0x01c
};

class RenderStructQ19
{
public:
	uint8  m_pad_00[0xd4];
	void*  (*LockSurface)(void* pRenderData);    // 0x0d4
	void   (*UnlockSurface)(void* pRenderData);  // 0x0d8
};

extern uint32 g_nBytesPerPixel;              // 0x004b1794  (cissurf.cpp)

// 0x0045f620 -- the reading twin of 0x0045f5e0 on the same blitter
// sub-object: fmconvert.cpp's FMConvertMgrAPI::FormatColorToPValue (format,
// raw pixel value, HLTCOLOR out), declared in clientmgr.h.

// 0x0040c5f0 -- ILTClient::GetPixel, SetPixel's twin: same refusals under
// "ILTClient::GetPixel" (0x004a8120), the pixel's 32-bit value is read
// (`mov (%eax),%edx` at 0x0040c688) and converted back through 0x0045f620
// into *color; no flag is set on the surface afterwards.
LTRESULT ci_GetPixel(HSURFACE hSurface, uint32 x, uint32 y, HLTCOLOR* color)
{
	CSurfaceQ19* pSurface = (CSurfaceQ19*)hSurface;
	uint8*       pBits;

	if (!pSurface || x >= pSurface->m_nWidth || y >= pSurface->m_nHeight)
		RETURN_ERROR(1, "ILTClient::GetPixel", LT_INVALIDPARAMS);

	pBits = (uint8*)((RenderStructQ19*)g_pRender)->LockSurface(pSurface->m_pRenderData);
	if (!pBits)
		RETURN_ERROR(1, "ILTClient::GetPixel", LT_ERROR);

	pBits += x * g_nBytesPerPixel + y * pSurface->m_nPitch;
	CI_FMCONVERT()->FormatColorToPValue((FMFormat*)&g_ScreenFormat, *(uint32*)pBits, color);

	((RenderStructQ19*)g_pRender)->UnlockSurface(pSurface->m_pRenderData);

	return LT_OK;
}

LTRESULT ci_SetPixel(HSURFACE hSurface, uint32 x, uint32 y, HLTCOLOR color)
{
	CSurfaceQ19* pSurface = (CSurfaceQ19*)hSurface;
	uint8*       pBits;

	if (!pSurface || x >= pSurface->m_nWidth || y >= pSurface->m_nHeight)
		RETURN_ERROR(1, "ILTClient::SetPixel", LT_INVALIDPARAMS);

	pBits = (uint8*)((RenderStructQ19*)g_pRender)->LockSurface(pSurface->m_pRenderData);
	if (!pBits)
		RETURN_ERROR(1, "ILTClient::SetPixel", LT_ERROR);

	pBits += x * g_nBytesPerPixel + y * pSurface->m_nPitch;
	CI_FMCONVERT()->PValueToFormatColor((FMFormat*)&g_ScreenFormat, color, (uint32*)pBits);

	((RenderStructQ19*)g_pRender)->UnlockSurface(pSurface->m_pRenderData);
	pSurface->m_nFlags |= 4;

	return LT_OK;
}
