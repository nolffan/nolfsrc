// *********************************************************************** //
//
//  MODULE:   CLIENTMUSIC (.CPP)
//
//  PURPOSE:  The engine's DirectMusic implementation.
//
// *********************************************************************** //

#include <string.h>

#include "clientmusic.h"


// Playing is asking for the level's starting intensity, immediately.
// The level-not-initialised refusal's callee, under a name of its own. It is
// the same 0x0043ef90 as dm_NotInitError (link/aliases.rsp), but it has to be
// a SECOND SYMBOL to the compiler: with both refusals calling one function,
// VC6 /O2 tail-merges ChangeIntensity's second refusal into the first (`push
// ; jmp`, 777 bytes) where the shipped 0x00440620 keeps both whole (798).
// No one-symbol spelling stops that merge (an inline wrapper, a cast or a
// local for the name, else-if, a literal return value: all 777). Two
// identical refusal functions that the linker folded into one would explain
// it; that is a guess, the second symbol is what the bytes need. Every
// function here uses it for the level test, so the eight sites stay alike.
void dm_NotLevelInitError(char* pFnName);

LTRESULT CLTDirectMusicMgr::Play()
{
	dm_Printf(4, "CLTDirectMusicMgr::Play\n");

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::Play");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::Play");
		return LT_ERROR;
	}

	return ChangeIntensity(m_nInitialIntensity, LTDMEnactImmediatly);
}

// The master volume is a performance-wide parameter, not a segment one, so
// this needs nothing but the performance — and the address of its own
// argument, which is why VC6 writes the register-held parameter back to its
// stack home first.
LTRESULT CLTDirectMusicMgr::SetVolume(const long nVolume)
{
	long nSetVolume = nVolume;

	dm_Printf(4, "CLTDirectMusicMgr::SetVolume nVolume=%i\n", nVolume);

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::SetVolume");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::SetVolume");
		return LT_ERROR;
	}

	return m_pPerformance->SetGlobalParam(GUID_PerfMasterVolume, &nSetVolume,
	                                      sizeof(long)) < 0;
}

// Stopping the secondary segments: all of them, or the one with this name.
//
// The two halves were clearly edited independently. The all-of-them loop asks
// each node whether it is playing and decides the enact type in an if/else
// with a call in each arm — which VC6 merges back into one, both values in
// the same register. The by-name loop keeps the enact type in a variable and
// then tests the wrong object: `pNode->m_pSegment->m_pDMSegment`, the loaded
// segment, where the other loop tests `pNode->m_pSegState`, the playing one.
// It works, but only because a node is never added to the list without a
// segment state; a node that could fail that test would be found again on the
// next pass and never removed.
//
// Two more quirks the shipped code carries (0x00440b4e..0x00440b8b), and
// both are what made these nears before:
// * nEnact is ONE function-scope variable, set from nStart once. A node whose
//   segment has its own stop type overwrites it, and every later node inherits
//   that value.
// * The all-of-them loop moves to the next node only inside the "is playing"
//   test. The unguarded tail is `test %eax,%eax`, reached straight from the
//   segment-state `je`. So a node with no segment state would spin forever;
//   that is the same list invariant the by-name loop relies on.
#ifdef NOLFSRC_MODERN
// fixed: StopSecondary and StopMotif test the wrong object when given a name
// (RETAIL_BUGS.md). One walk for both cases: each node is visited once, the
// next link read before the node can go back to the bank, and only a node
// with a segment state is stopped. Nothing is found twice, so a node that is
// not playing, or one left to its next-segment stop, cannot hang the loop.
static void dm_StopPlayingNodes(CLTDirectMusicMgr* pMgr, CDMPlayingList& List,
                                const char* sName, const LTDMEnactTypes nStart)
{
	CDMPlayingNode* pNode = List.m_pHead;

	while (pNode)
	{
		CDMPlayingNode* pNext   = pNode->m_pNext;
		CDMSegment*     pSegment = pNode->m_pSegment;
		LTDMEnactTypes  nEnact  = nStart;

		if (pNode->m_pSegState &&
		    (!sName || (pSegment && pSegment->GetName() &&
		                stricmp(pSegment->GetName(), sName) == 0)))
		{
			if (nStart == LTDMEnactDefault && pSegment)
				nEnact = pSegment->m_nStopEnact;

			if (nEnact != LTDMEnactNextSegment)
				List.StopNode(pMgr, pNode, nEnact, 0);
		}

		pNode = pNext;
	}
}
#endif
LTRESULT CLTDirectMusicMgr::StopSecondary(const char* sSecondarySegment,
                                          const LTDMEnactTypes nStart)
{
	CDMPlayingNode* pNode;
	LTDMEnactTypes  nEnact = nStart;

	dm_Printf(4, "CLTDirectMusicMgr::StopSecondary sSecondarySegment=%s nStart=%i\n",
	          sSecondarySegment, nStart);

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::StopSecondary");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::StopSecondary");
		return LT_ERROR;
	}

	EnterCriticalSection(&m_CriticalSection);

#ifdef NOLFSRC_MODERN
	dm_StopPlayingNodes(this, m_SecondaryList, sSecondarySegment, nStart);
#else
	if (!sSecondarySegment)
	{
		pNode = m_SecondaryList.m_pHead;

		while (pNode)
		{
			CDMPlayingNode* pNext = pNode->m_pNext;

			if (pNode->m_pSegState)
			{
				if (nStart == LTDMEnactDefault)
					nEnact = pNode->m_pSegment->m_nStopEnact;

				if (nEnact != LTDMEnactNextSegment)
					m_SecondaryList.StopNode(this, pNode, nEnact, 0);

				pNode = pNext;
			}
		}
	}
	else
	{
		while ((pNode = m_SecondaryList.FindByName(sSecondarySegment)) != LTNULL)
		{
			void*          pDMSegment;

			if (nStart == LTDMEnactDefault)
				nEnact = pNode->m_pSegment->m_nStopEnact;

			pDMSegment = pNode->m_pSegment->m_pDMSegment;

			if (pDMSegment && nEnact != LTDMEnactNextSegment)
				m_SecondaryList.StopNode(this, pNode, nEnact, 0);
		}
	}
#endif

	LeaveCriticalSection(&m_CriticalSection);
	return LT_OK;
}

// The motifs, from their own list eight bytes along, and otherwise the same
// function down to the wrong-object test.
LTRESULT CLTDirectMusicMgr::StopMotif(const char* sMotifName, const LTDMEnactTypes nStart)
{
	CDMPlayingNode* pNode;
	LTDMEnactTypes  nEnact = nStart;

	dm_Printf(4, "CLTDirectMusicMgr::StopMotif sMotifName=%s nStart=%i\n",
	          sMotifName, nStart);

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::StopMotif");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::StopMotif");
		return LT_ERROR;
	}

	EnterCriticalSection(&m_CriticalSection);

#ifdef NOLFSRC_MODERN
	dm_StopPlayingNodes(this, m_MotifList, sMotifName, nStart);
#else
	if (!sMotifName)
	{
		pNode = m_MotifList.m_pHead;

		while (pNode)
		{
			CDMPlayingNode* pNext = pNode->m_pNext;

			if (pNode->m_pSegState)
			{
				if (nStart == LTDMEnactDefault)
					nEnact = pNode->m_pSegment->m_nStopEnact;

				if (nEnact != LTDMEnactNextSegment)
					m_MotifList.StopNode(this, pNode, nEnact, 0);

				pNode = pNext;
			}
		}
	}
	else
	{
		while ((pNode = m_MotifList.FindByName(sMotifName)) != LTNULL)
		{
			void*          pDMSegment;

			if (nStart == LTDMEnactDefault)
				nEnact = pNode->m_pSegment->m_nStopEnact;

			pDMSegment = pNode->m_pSegment->m_pDMSegment;

			if (pDMSegment && nEnact != LTDMEnactNextSegment)
				m_MotifList.StopNode(this, pNode, nEnact, 0);
		}
	}
#endif

	LeaveCriticalSection(&m_CriticalSection);
	return LT_OK;
}

// Playing a secondary segment: find the definition the control file gave that
// name, and either hand it to DirectMusic now or queue it behind whatever is
// playing. The segment flags always carry SECONDARY and CONTROL on top of
// whatever the enact type asks for.
//
// Both of this function's dead argument slots get reused — the segment name's
// for the critical section's address and the enact type's for the
// performance's vtable — which is why it reserves only one word of its own.
//
// A near, size-exact at 412 bytes and 152 instructions, after one change:
// the PlaySegment argument is now named. Shipped loads m_pDMSegment into ebx
// before the EnactToSegFlags call (0x004409f5), then re-zeroes ebx afterwards.
// Nested, the load came after the call and the function was 8 bytes long.
// What is left is register allocation only: shipped keeps the name and then
// pSegment in ebp and the zero in ebx, and ours swaps the two. Reordering the
// locals, declaring pSegment at the FindByName, and dropping nEnact for a
// non-const nStart gave no change or worse. PlayMotif is the same.
LTRESULT CLTDirectMusicMgr::PlaySecondary(const char* sSecondarySegment,
                                          const LTDMEnactTypes nStart)
{
	IDirectMusicSegmentState* pSegState = LTNULL;
	CDMSegment*    pSegment;
	LTDMEnactTypes nEnact = nStart;

	dm_Printf(4, "CLTDirectMusicMgr::PlaySecondary sSecondarySegment=%s nStart=%i\n",
	          sSecondarySegment, nStart);

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::PlaySecondary");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::PlaySecondary");
		return LT_ERROR;
	}

	pSegment = m_SecondaryDefs.FindByName(sSecondarySegment);

	if (!pSegment)
		return LT_ERROR;

	if (!pSegment->m_pDMSegment)
		return LT_ERROR;

	if (nEnact == LTDMEnactDefault)
		nEnact = pSegment->m_nStopEnact;

	EnterCriticalSection(&m_CriticalSection);

	if (nEnact != LTDMEnactNextSegment)
	{
		void* pDMSegment = pSegment->m_pDMSegment;
		m_pPerformance->PlaySegment(pDMSegment,
		                            EnactToSegFlags(nEnact) | 0x280, 0, &pSegState);
	}
	else
	{
		CDMPendingNode* pPending = (CDMPendingNode*)g_PendingBank.Alloc();

		if (pPending)
		{
			pPending->m_nType = DMPENDING_PLAYSECONDARY;
			pPending->m_pSegment = pSegment;

			if (!m_pQueueAfter)
				m_PendingList.AddHead(pPending);
			else
				m_PendingList.InsertAfter(m_pQueueAfter, pPending);
		}
	}

	if (pSegState)
	{
		CDMPlayingNode* pNode = (CDMPlayingNode*)g_PlayingBank.Alloc();

		if (pNode)
		{
			pNode->m_pSegState = LTNULL;
			pNode->m_pSegment = LTNULL;
			pNode->m_pSegState = pSegState;
			pNode->m_pSegment = pSegment;
			m_SecondaryList.AddHead(pNode);
		}
	}

	LeaveCriticalSection(&m_CriticalSection);
	return LT_OK;
}

// The motifs, from their own definition list and their own playing list, and
// with only SECONDARY in the flags where the secondary segments also take
// CONTROL. The queued record's type code is the one number that says which of
// the two this was.
//
// A near for the same reason PlaySecondary is: the shipped build carries a
// function-long zero in a fifth callee-saved register and compares memory
// against it. Nine bytes over four hundred and forty.
//


LTRESULT CLTDirectMusicMgr::PlayMotif(const char* sMotifName,
                                          const LTDMEnactTypes nStart)
{
	IDirectMusicSegmentState* pSegState = LTNULL;
	CDMSegment*    pSegment;
	LTDMEnactTypes nEnact = nStart;

	dm_Printf(4, "CLTDirectMusicMgr::PlayMotif sMotifName=%s nStart=%i\n",
	          sMotifName, nStart);

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::PlayMotif");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::PlayMotif");
		return LT_ERROR;
	}

	pSegment = m_MotifDefs.FindByName(sMotifName);

	if (!pSegment)
		return LT_ERROR;

	if (!pSegment->m_pDMSegment)
		return LT_ERROR;

	if (nEnact == LTDMEnactDefault)
		nEnact = pSegment->m_nStopEnact;

	EnterCriticalSection(&m_CriticalSection);

	if (nEnact != LTDMEnactNextSegment)
	{
		// Named, not nested: shipped loads m_pDMSegment into a callee-saved
		// register BEFORE the EnactToSegFlags call (0x00440cb7); nested,
		// it loads after and the function is 4 bytes long.  Size-exact
		// with this; the residual is a pure ebx/edi/ebp register rotation.
		void* pDMSegment = pSegment->m_pDMSegment;
		m_pPerformance->PlaySegment(pDMSegment,
		                            EnactToSegFlags(nEnact) | 0x80, 0, &pSegState);
	}
	else
	{
		CDMPendingNode* pPending = (CDMPendingNode*)g_PendingBank.Alloc();

		if (pPending)
		{
			pPending->m_nType = DMPENDING_PLAYMOTIF;
			pPending->m_pSegment = pSegment;

			if (!m_pQueueAfter)
				m_PendingList.AddHead(pPending);
			else
				m_PendingList.InsertAfter(m_pQueueAfter, pPending);
		}
	}

	if (pSegState)
	{
		CDMPlayingNode* pNode = (CDMPlayingNode*)g_PlayingBank.Alloc();

		if (pNode)
		{
			pNode->m_pSegState = LTNULL;
			pNode->m_pSegment = LTNULL;
			pNode->m_pSegState = pSegState;
			pNode->m_pSegment = pSegment;
			m_MotifList.AddHead(pNode);
		}
	}

	LeaveCriticalSection(&m_CriticalSection);
	return LT_OK;
}

// Stopping everything: the primary segment through DirectMusic, then the
// queue of pending requests, then both lists of playing segments.
//
// Only the primary stop honours the enact type. It is skipped for
// LTDMEnactNextSegment - the primary is left to run out on its own - and the
// pending queue is emptied regardless, so what was going to follow it never
// does. The secondaries and motifs are stopped immediately, whatever was
// asked; that is the helper's own choice, not this function's.
//
// The performance's vtable is read before the enact conversion is called and
// the performance itself reloaded for the call, which is how VC6 orders a
// virtual call whose last argument is itself a call. The pending loop tests
// the node it has just unlinked before pushing it back on the bank, because
// the bank's inline free has a null test of its own and VC6 does not carry
// the loop's condition across the call to prove it.
LTRESULT CLTDirectMusicMgr::Stop(const LTDMEnactTypes nStart)
{
	dm_Printf(4, "CLTDirectMusicMgr::Stop nStart=%i\n", nStart);

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::Stop");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::Stop");
		return LT_ERROR;
	}

	if (m_pPerformance && nStart != LTDMEnactNextSegment)
	{
		m_pPerformance->Stop(LTNULL, LTNULL, 0, EnactToSegFlags(nStart));
	}

	EnterCriticalSection(&m_CriticalSection);

	{
		CDMPendingNode* pPending = m_PendingList.m_pHead;

		while (pPending)
		{
			m_PendingList.Remove(pPending);
			g_PendingBank.Free(pPending);

			pPending = m_PendingList.m_pHead;
		}
	}

	m_SecondaryList.StopAll(this, 0);
	m_MotifList.StopAll(this, 0);

	m_pQueueAfter = LTNULL;

	LeaveCriticalSection(&m_CriticalSection);
	return LT_OK;
}

// Tearing a level down: silence everything inside the critical section, then
// give every record the level loaded back to its bank and free the tables.
//
// The order is the shipped one and it is not the tidy one. The queue of
// pending requests goes first, then the interface's own Stop, then the
// secondary and motif definitions and the two playing lists — all under the
// lock — and only after the lock is dropped do the bands, the two lists whose
// objects this function only ever releases, and the four allocations go. A
// level that was never initialised is not an error here, where every other
// entry point makes it one: the warning is a warning and the answer is LT_OK.
//
// Each list is emptied the same way: release what the record holds, unlink
// it, and push it onto its bank's free list — the bank's own inline fast
// path, whose null test survives right after a Remove that cannot have
// changed the pointer.
//
// The four frees all reach 0x004290c0, which is the engine's operator delete
// (the one-argument wrapper round dfree at 0x00429080), so they are deletes
// and not dfree calls. The array at 0xb8 is a `delete []` of a class with a
// destructor, and with exceptions off VC6 expands the vector destructor in
// place rather than calling it: the element count read from the word before
// the array, a pointer parked past the end and walked down with `sub $0x10`
// before each destructor call, `dec; js` on the count, and operator delete
// on the cookie word. Neither `for (i = n - 1; i >= 0; i--)` nor
// `while (--i >= 0)` over `pArray[i]` reproduces it — both start the pointer
// at the last element and decrement after the call. The compiler also emits
// the stand-alone iterator (`??_I`) as an unreferenced COMDAT, the way it
// emits `??_H` for `new T[n]`. The null tests round each free are the
// source's, not the compiler's: the store of the null sits inside them.
LTRESULT CLTDirectMusicMgr::TermLevel()
{
	dm_Printf(3, "CLTDirectMusicMgr::TermLevel\n");

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::TermLevel");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_Warning("WARNING! Level already terminated or never initialized. (CLTDirectMusicMgr::TermLevel)\n");
		return LT_OK;
	}

	EnterCriticalSection(&m_CriticalSection);

	ClearPendingList();
	Stop(LTDMEnactDefault);
	Unknown4426c0();

	m_SecondaryDefs.StopAll(this, 0);
	m_MotifDefs.StopAll(this, 0);
	m_SecondaryList.StopAll(this, 0);
	m_MotifList.StopAll(this, 0);

	LeaveCriticalSection(&m_CriticalSection);

	{
		CDMBand* pBand = m_BandList.m_pHead;

		while (pBand)
		{
			pBand->m_pDMBandSegment->Release();
			pBand->m_pDMBand->Unload(m_pPerformance);
			m_BandList.Remove(pBand);
			g_BandBank.Free(pBand);

			pBand = m_BandList.m_pHead;
		}
	}

	{
		CDMUnknown58* pNode = m_Unknown58List.m_pHead;

		while (pNode)
		{
			pNode->m_pDMObject->Release();
			m_Unknown58List.Remove(pNode);

			if (pNode)
			{
				if (pNode->m_pName)
					delete [] pNode->m_pName;

				g_Unknown58Bank.Free(pNode);
			}

			pNode = m_Unknown58List.m_pHead;
		}
	}

	{
		CDMUnknown60* pNode = m_Unknown60List.m_pHead;

		while (pNode)
		{
			pNode->m_pDMObject->Release();
			m_Unknown60List.Remove(pNode);
			g_Unknown60Bank.Free(pNode);

			pNode = m_Unknown60List.m_pHead;
		}
	}

	if (m_pUnknownC4)
	{
		operator delete(m_pUnknownC4);
		m_pUnknownC4 = LTNULL;
	}

	if (m_pUnknownC8)
	{
		operator delete(m_pUnknownC8);
		m_pUnknownC8 = LTNULL;
	}

	if (m_aIntensities)
	{
		delete [] m_aIntensities;
		m_aIntensities = LTNULL;
	}

	if (m_aTransitions)
	{
		operator delete(m_aTransitions);
		m_aTransitions = LTNULL;
	}

	m_bLevelInitialized = LTFALSE;
	return LT_OK;
}

// Setting up a level: read its control file, size the intensity and
// transition tables from it, and fill them.
//
// The control file is read through a reader derived from lithshared's, whose
// four file-access virtuals go through the engine's file streams instead of
// stdio. It lives on the stack for the whole function and is the whole frame,
// which is why the full path, when one has to be built, is parked in the dead
// slot of sDefine2.
//
// The path is built only when the control file name carries no directory of
// its own: then it is the working directory, a backslash unless the directory
// already ends in one (or a slash, or a colon), and the name. If the reader
// then refuses the file the function returns without freeing that path - the
// path is only released at 0x0043fecb, on the success side of that test, and
// the failure block returns at 0x0043fec8 without touching it. The leak is
// shipped, and reproduced.
//
// The trace runs at level three where the play calls run at four, and the
// format string misspells the last parameter as "sDeine3".
//
// One block short of identical. The shipped code reports a refused control
// file from two blocks, not one: an inline copy at 0x0043fea6 for the built
// path and a second at 0x00440025 for the name that already had a directory,
// each with its own dm_Error call, its own destructor call and its own
// return, and the four earlier error returns cross-jump into the second of
// them. Every compiler this tree can run merges the first into the second and
// leaves a jump, whatever shape the two failures are written in - an else
// against an early return, a shared flag tested after the branch, a goto to
// one block, the literal spelled out twice, a local static array - and
// whatever the pooling and optimisation flags. The residual is that one block:
// 1340 bytes against 1373, 446 instructions against 454.
LTRESULT CLTDirectMusicMgr::InitLevel(const char* sWorkingDirectory, const char* sControlFileName,
                                      const char* sDefine1, const char* sDefine2,
                                      const char* sDefine3)
{
	int i;
	static char sReadError[] = "ERROR! Unable to read control file. (CLTDirectMusicMgr::InitLevel)\n";
	static char sSeparator[] = "\\";

#ifdef NOLFSRC_MODERN
	// fixed: "sDefine3", not "sDeine3" (RETAIL_BUGS.md)
	dm_Printf(3, "CLTDirectMusicMgr::InitLevel sWorkingDirectory=%s sControlFileName=%s sDefine1=%s sDefine2=%s sDefine3=%s\n",
	          sWorkingDirectory, sControlFileName, sDefine1, sDefine2, sDefine3);
#else
	dm_Printf(3, "CLTDirectMusicMgr::InitLevel sWorkingDirectory=%s sControlFileName=%s sDefine1=%s sDefine2=%s sDeine3=%s\n",
	          sWorkingDirectory, sControlFileName, sDefine1, sDefine2, sDefine3);
#endif

	CDMControlFileMgr controlFile;

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::InitLevel");
		return LT_ERROR;
	}

	if (m_bLevelInitialized)
	{
		dm_Error("ERROR! InitLevel has already been called and not terminated. (CLTDirectMusicMgr::InitLevel)\n");
		return LT_ERROR;
	}

	if (!sControlFileName)
	{
		dm_Error("ERROR! Control file name not valid. (CLTDirectMusicMgr::InitLevel)\n");
		return LT_ERROR;
	}

	if (!sWorkingDirectory)
	{
		dm_Error("ERROR! Working directory not valid. (CLTDirectMusicMgr::InitLevel)\n");
		return LT_ERROR;
	}

	SetWorkingDirectory(sWorkingDirectory, 1);

	if (sDefine1)
		controlFile.AddDefine(sDefine1);
	if (sDefine2)
		controlFile.AddDefine(sDefine2);
	if (sDefine3)
		controlFile.AddDefine(sDefine3);

	if (strchr(sControlFileName, '\\') == LTNULL && strchr(sControlFileName, '/') == LTNULL
	    && strchr(sControlFileName, ':') == LTNULL)
	{
		char* sFullName = new char[strlen(sWorkingDirectory) + strlen(sControlFileName) + 2];

		if (!sFullName)
			return LT_ERROR;

		strcpy(sFullName, sWorkingDirectory);

		if (strlen(sWorkingDirectory) != 0)
		{
			int nLen = strlen(sWorkingDirectory) - 1;

			if (sWorkingDirectory[nLen] != '\\' && sWorkingDirectory[nLen] != '/'
			    && sWorkingDirectory[nLen] != ':')
				strcat(sFullName, sSeparator);
		}

		strcat(sFullName, sControlFileName);

		// The shipped code keeps a whole second copy of this failure block
		// inline here instead of jumping to the one below; the reconstruction
		// cannot stop the cross-jumper merging them. See the head of the
		// function.
		if (!controlFile.Init(sFullName))
		{
#ifdef NOLFSRC_MODERN
			// fixed: the error names the file (RETAIL_BUGS.md)
			dm_Error("ERROR! Unable to read control file \"%s\". (CLTDirectMusicMgr::InitLevel)\n", sFullName);
			delete [] sFullName;	// fixed: not leaked (RETAIL_BUGS.md)
#else
			dm_Error(sReadError);
#endif
			return LT_ERROR;
		}

		delete [] sFullName;
	}
	else
	{
		if (!controlFile.Init(sControlFileName))
		{
#ifdef NOLFSRC_MODERN
			// fixed: the error names the file (RETAIL_BUGS.md)
			dm_Error("ERROR! Unable to read control file \"%s\". (CLTDirectMusicMgr::InitLevel)\n", sControlFileName);
#else
			dm_Error(sReadError);
#endif
			return LT_ERROR;
		}
	}

	m_nNumIntensities = 0;
	m_nInitialIntensity = 0;
	m_nInitialVolume = 0;
	m_nNumPChannels = 64;
	m_nNumVoices = 64;
	m_nSynthSampleRate = 44100;

	controlFile.GetKeyVal(LTNULL, "NUMINTENSITIES", m_nNumIntensities);
	controlFile.GetKeyVal(LTNULL, "INITIALINTENSITY", m_nInitialIntensity);
	controlFile.GetKeyVal(LTNULL, "INITIALVOLUME", m_nInitialVolume);
	controlFile.GetKeyVal(LTNULL, "PCHANNELS", m_nNumPChannels);
	controlFile.GetKeyVal(LTNULL, "VOICES", m_nNumVoices);
	controlFile.GetKeyVal(LTNULL, "SYNTHSAMPLERATE", m_nSynthSampleRate);

	if (m_PortParams.dwVoices != m_nNumVoices
	    || m_PortParams.dwAudioChannels != m_nNumPChannels
	    || m_PortParams.dwSampleRate != m_nSynthSampleRate)
	{
		CreatePort();
	}

	{
		CControlFileKey* pKey = controlFile.GetKey(LTNULL, "DLSBANKDIRECTORY");

		if (pKey)
		{
			CControlFileWord* pWord = pKey->GetFirstWord();

			if (pWord)
				dm_SetSearchDirectory(m_pLoader, CLSID_DirectMusicCollection, pWord->GetVal(), 0);
		}
	}

	ReadReverbParameters(&controlFile);

	if (m_nNumIntensities < 1)
	{
		dm_Error("ERROR! Number of intensities is not valid. NumIntensities = %i (CLTDirectMusicMgr::InitLevel)\n",
		         m_nNumIntensities);
		controlFile.Term();
		return LT_ERROR;
	}

	m_aIntensities = new CDMIntensity[m_nNumIntensities + 1];

	for (i = 0; i <= m_nNumIntensities; i++)
	{
		m_aIntensities[i].m_nUnknown00 = 0;
		m_aIntensities[i].m_nUnknown04 = 0;
	}

	m_nNumTransitions = (m_nNumIntensities + 1) * (m_nNumIntensities + 1);
	m_aTransitions = new CDMTransition[m_nNumTransitions];

	for (i = 0; i < m_nNumTransitions; i++)
	{
		m_aTransitions[i].m_nEnact = LTDMEnactNextMeasure;
		m_aTransitions[i].m_bManual = 1;
		m_aTransitions[i].m_pDMSegment = LTNULL;
	}

	if (!m_aIntensities || !m_aTransitions)
	{
		dm_Error("ERROR! Unable to allocate memory for Intensity and Transition arrays. (CLTDirectMusicMgr::InitLevel)\n",
		         m_nNumIntensities);
		controlFile.Term();

		if (m_aIntensities)
		{
			delete [] m_aIntensities;
			m_aIntensities = LTNULL;
		}

		if (m_aTransitions)
		{
			delete [] m_aTransitions;
			m_aTransitions = LTNULL;
		}

		return LT_ERROR;
	}

	ReadDLSBanks(&controlFile);
	ReadStyles(&controlFile);
	ReadIntensities(&controlFile);
	ReadSecondarySegments(&controlFile);
	ReadMotifs(&controlFile);
	ReadTransitions(&controlFile);

	m_bLevelInitialized = LTTRUE;

	controlFile.Term();

	m_pQueueAfter = LTNULL;
	m_nCurIntensity = 0;

	return LT_OK;
}

// Changing intensity is the function that decides what plays next. It empties
// the pending queue and rebuilds it for the new intensity: the transition's
// segment first if there is one, then every segment of the new intensity, then
// the intensity's two numbers as requests of their own. Only one thing is
// handed to DirectMusic straight away - the transition segment, or failing
// that the intensity's first segment - and only when the enact type is not
// "next segment", which EnactToSegFlags reports as the QUEUE flag and this
// function tests as the flag rather than as the enact type.
//
// Intensity zero is a stop, and reported as an error afterwards; so is a
// change with no performance to play on. The queue cursor is left on the first
// queued request only when that request is the segment already sent to
// DirectMusic, otherwise it is cleared.
//
// The four pending nodes are allocated with `new`, which is why only the type
// store is guarded by the allocation's null test: the constructor runs on
// success and the assignments after it run regardless, so a failed allocation
// here writes through null. The shipped code has the same four unguarded
// stores, and the first of them keeps two copies of the whole tail - the null
// arm materialises the zero with `xor %eax,%eax` and then stores through it -
// which is what says `new` and not a written-out allocate-and-test.
//
// Byte-identical (798). It was 777 while both not-initialised blocks called
// one symbol: VC6 found the two tails identical and cross-jumped the second
// block into the first from the call onwards, where the shipped build emits
// it whole. The level test now calls dm_NotLevelInitError (see its
// declaration at the top of this file).
//
// The four allocations are three shapes, and that part now matches:
// * PLAYTRANSITION and PLAYSEGMENT (0x00440755, 0x00440805) are
//   CDMBank::AllocPop: Pop out of line on both paths, folded into one call.
// * INTENSITY00 (0x00440847) is CDMBank::Alloc: the fast path written out,
//   and a Pop CALL after the Grow. PlaySecondary and PlayMotif have the same
//   shape (0x00440a25).
// * INTENSITY04 (0x004408a6) is the same fast path, but the pop after the Grow
//   is expanded inline, with its own null test.
// Each site stores its type behind the allocator's null test, so all four are
// `new`. The last two therefore go through their own operator new. The two
// classes below are those operators, and nothing else. What the original
// source spelled here is not known. It may be one inline Pop that VC6
// expanded at some sites and not others: an inline Pop in AllocPop gives the
// full inline shape at every site until there are six or more sites, and the
// earliest sites lose it first, always both pops of a site together.
class CDMPendingAllocQ440 : public CDMPendingNode
{
public:
	CDMPendingAllocQ440(uint32 nType) : CDMPendingNode(nType) {}
	void* operator new(unsigned int nSize) { return g_PendingBank.Alloc(); }
};

inline void* CDMBankPopInlineQ440(CDMBank* pBank)
{
	void* pRecord = pBank->m_pFree;

	if (pRecord)
		pBank->m_pFree = *(void**)pRecord;

	return pRecord;
}

class CDMPendingAllocInlineQ440 : public CDMPendingNode
{
public:
	CDMPendingAllocInlineQ440(uint32 nType) : CDMPendingNode(nType) {}
	void* operator new(unsigned int nSize)
	{
		void* pRecord = g_PendingBank.m_pFree;

		if (pRecord)
		{
			g_PendingBank.m_pFree = *(void**)pRecord;
			return pRecord;
		}

		if (!g_PendingBank.Grow())
			return LTNULL;

		return CDMBankPopInlineQ440(&g_PendingBank);
	}
};

LTRESULT CLTDirectMusicMgr::ChangeIntensity(const int nNewIntensity,
                                            const LTDMEnactTypes nStart)
{
	CDMTransition*  pTransition;
	CDMSegment*     pSegment;
	CDMPendingNode* pPending;
	LTDMEnactTypes  nEnact;
	uint32          dwFlags;

	dm_Printf(4, "CLTDirectMusicMgr::ChangeIntensity nNewIntensity=%i nStart=%i\n",
	          nNewIntensity, nStart);

	if (!m_bInitialized)
	{
		dm_NotInitError("CLTDirectMusicMgr::ChangeIntensity");
		return LT_ERROR;
	}

	if (!m_bLevelInitialized)
	{
		dm_NotLevelInitError("CLTDirectMusicMgr::ChangeIntensity");
		return LT_ERROR;
	}

	if (!m_pPerformance)
		return LT_ERROR;

	if (nNewIntensity == 0)
	{
		Stop(nStart);
		return LT_ERROR;
	}

	if (nNewIntensity <= 0 || nNewIntensity > m_nNumIntensities)
		return LT_ERROR;

	EnterCriticalSection(&m_CriticalSection);

	if (m_nCurIntensity == nNewIntensity)
	{
		LeaveCriticalSection(&m_CriticalSection);
		dm_Printf(4, "CLTDirectMusicMgr::ChangeIntensity new intensity same as old nothing to do, exiting.\n");
		return LT_OK;
	}

	pTransition = GetTransition(m_nCurIntensity, nNewIntensity);

	if (!pTransition)
	{
		LeaveCriticalSection(&m_CriticalSection);
		dm_Error("ERROR! Invalid transition found, exiting. (CLTDirectMusicMgr::ChangeIntensity)\n");
		return LT_ERROR;
	}

	ClearPendingList();

	if (nStart == LTDMEnactInvalid)
		nEnact = pTransition->m_nEnact;
	else
		nEnact = nStart;

	dwFlags = EnactToSegFlags(nEnact);

	if (pTransition->m_pDMSegment)
	{
		if (dwFlags == 0x100)
		{
			pPending = new CDMPendingNode(DMPENDING_PLAYTRANSITION);
			pPending->m_pSegment = (CDMSegment*)pTransition;
			m_PendingList.AddTail(pPending);
		}
		else
		{
			m_pPerformance->PlaySegment(pTransition->m_pDMSegment, dwFlags, 0, LTNULL);
		}
	}
	else
	{
		if (dwFlags != 0x100)
		{
			pSegment = m_aIntensities[nNewIntensity].m_Segments.m_pHead;

			if (pSegment && pSegment->m_pDMSegment)
				m_pPerformance->PlaySegment(pSegment->m_pDMSegment, dwFlags, 0, LTNULL);
		}
	}

	pSegment = m_aIntensities[nNewIntensity].m_Segments.m_pHead;

	while (pSegment)
	{
		if (pSegment->m_pDMSegment)
		{
			pPending = new CDMPendingNode(DMPENDING_PLAYSEGMENT);
			pPending->m_pSegment = pSegment;
			m_PendingList.AddTail(pPending);
		}

		pSegment = pSegment->m_pNext;
	}

	pPending = new CDMPendingAllocQ440(DMPENDING_INTENSITY00);
	pPending->m_nValue = m_aIntensities[nNewIntensity].m_nUnknown00;
	m_PendingList.AddTail(pPending);

	if (m_aIntensities[nNewIntensity].m_nUnknown04 > 0)
	{
		pPending = new CDMPendingAllocInlineQ440(DMPENDING_INTENSITY04);
		pPending->m_nValue = m_aIntensities[nNewIntensity].m_nUnknown04;
		m_PendingList.AddTail(pPending);
	}

	if (!pTransition->m_pDMSegment && dwFlags != 0x100)
		m_pQueueAfter = m_PendingList.m_pHead;
	else
		m_pQueueAfter = LTNULL;

	m_nCurIntensity = nNewIntensity;

	LeaveCriticalSection(&m_CriticalSection);
	return LT_OK;
}


// The thread's one allocation (0x0043f6dc..0x0043f70e) is the fast path
// written out AND the pop after the Grow expanded, ChangeIntensity's
// INTENSITY04 shape (CDMPendingAllocInlineQ440) on the playing bank.
// STAND-IN, name ours.
inline void* dmPlayingAllocInlineQ440()
{
	void* pRecord = g_PlayingBank.m_pFree;

	if (pRecord)
	{
		g_PlayingBank.m_pFree = *(void**)pRecord;
		return pRecord;
	}

	if (!g_PlayingBank.Grow())
		return LTNULL;

	return CDMBankPopInlineQ440(&g_PlayingBank);
}

// 0x0043f520 -- the notification thread (name ours).  Init hands it to
// _beginthread (0x0043fa79 pushes its address) and Term stops it by setting
// m_bQuitThread under the lock and waiting on the thread handle.  Everything
// goes through the published singleton (0x004b3fe0), reloaded after every
// call.
//
// Poll the performance's event every 100 ms; for each segment notification:
// on SEGALMOSTEND run the pending queue from behind m_pQueueAfter, and on
// SEGEND drop the playing node the finished state belongs to.  pSegState is
// set only when a message carries one and never cleared (0x0043f532 zeroes
// %ebp once, outside both loops), which is the shipped behaviour.

// The singleton itself: zero-initialised, in .bss (0x004b3fe0).
CLTDirectMusicMgr* g_pDirectMusicMgr;

void __cdecl dm_NotificationThread(void* pParam)
{
	DMUS_NOTIFICATION_PMSG* pMsg;
	IDirectMusicSegmentState* pSegState = LTNULL;
	void* pSegment = LTNULL;
	CDMPendingNode* pNode;

	EnterCriticalSection(&g_pDirectMusicMgr->m_CriticalSection);

	while (!g_pDirectMusicMgr->m_bQuitThread)
	{
		LeaveCriticalSection(&g_pDirectMusicMgr->m_CriticalSection);
		WaitForSingleObject(g_pDirectMusicMgr->m_hNotifyEvent, 100);
		EnterCriticalSection(&g_pDirectMusicMgr->m_CriticalSection);

		while (g_pDirectMusicMgr->m_pPerformance->GetNotificationPMsg(&pMsg) == 0)
		{
			if (!g_pDirectMusicMgr->m_bQuitThread &&
			    pMsg->guidNotificationType == GUID_NOTIFICATION_SEGMENT)
			{
				if (pMsg->punkUser)
				{
					pSegState = pMsg->punkUser;
					if (pSegState->GetSegment(&pSegment) < 0)
						pSegment = LTNULL;
				}

				if (pMsg->dwNotificationOption == DMUS_NOTIFICATION_SEGALMOSTEND)
				{
					if (g_pDirectMusicMgr->m_pQueueAfter)
						pNode = g_pDirectMusicMgr->m_pQueueAfter->m_pNext;
					else
						pNode = g_pDirectMusicMgr->m_PendingList.m_pHead;

					while (pNode)
					{
						if (pNode->m_nType == 0 || pNode->m_nType == 1 ||
						    pNode->m_nType == 2 || pNode->m_nType == 5 ||
						    pNode->m_nType == 6 || pNode->m_nType == 7)
						{
							pNode = pNode->m_pNext;
						}
						else if (pNode->m_nType == DMPENDING_INTENSITY00)
						{
							if (pNode->m_nValue == 0)
								pNode = pNode->m_pNext;
							else
							{
								if (pNode->m_nValue > 0)
									pNode->m_nValue--;

								if (pNode->m_nValue == 0)
									pNode = pNode->m_pNext;
								else
									pNode = g_pDirectMusicMgr->m_PendingList.m_pHead;
							}
						}
						else if (pNode->m_nType == DMPENDING_INTENSITY04)
						{
							g_pDirectMusicMgr->ChangeIntensity(pNode->m_nValue, LTDMEnactInvalid);
							pNode = g_pDirectMusicMgr->m_PendingList.m_pHead;
						}
						else if (pNode->m_nType == DMPENDING_PLAYSEGMENT)
						{
							g_pDirectMusicMgr->m_pPerformance->PlaySegment(
								pNode->m_pSegment->m_pDMSegment, 0x100, 0, LTNULL);
							break;
						}
						else if (pNode->m_nType == DMPENDING_PLAYTRANSITION)
						{
							g_pDirectMusicMgr->m_pPerformance->PlaySegment(
								pNode->m_pSegment->m_pDMSegment, 0x100, 0, LTNULL);
							g_pDirectMusicMgr->m_PendingList.Remove(pNode);
							pNode = LTNULL;
							break;
						}
						else
						{
							if (pNode->m_nType == DMPENDING_PLAYSECONDARY)
							{
								IDirectMusicSegmentState* pNewState = LTNULL;

								g_pDirectMusicMgr->m_pPerformance->PlaySegment(
									pNode->m_pSegment->m_pDMSegment, 0x2080, 0, &pNewState);

								if (pNewState)
								{
									CDMPlayingNode* pPlaying = (CDMPlayingNode*)dmPlayingAllocInlineQ440();

									if (pPlaying)
									{
										pPlaying->m_pSegState = LTNULL;
										pPlaying->m_pSegment = LTNULL;
										pPlaying->m_pSegState = pNewState;
										pPlaying->m_pSegment = pNode->m_pSegment;
										g_pDirectMusicMgr->m_SecondaryList.AddHead(pPlaying);
									}
								}
							}

							if (pNode->m_nType == 0xa)
							{
								g_pDirectMusicMgr->m_pPerformance->Stop(
									pNode->m_pSegment->m_pDMSegment, LTNULL, 0, 0x2000);
							}
						}
					}

					g_pDirectMusicMgr->m_pQueueAfter = pNode;
				}

				if (pMsg->dwNotificationOption == DMUS_NOTIFICATION_SEGEND && pSegState)
				{
					CDMPlayingNode* pPlaying =
						g_pDirectMusicMgr->m_SecondaryList.FindBySegState(pSegState);

					if (pPlaying)
					{
						g_pDirectMusicMgr->m_SecondaryList.StopNode(g_pDirectMusicMgr,
							pPlaying, LTDMEnactImmediatly, 0);
					}
					else
					{
						pPlaying = g_pDirectMusicMgr->m_MotifList.FindBySegState(pSegState);

						if (pPlaying)
						{
							g_pDirectMusicMgr->m_MotifList.StopNode(g_pDirectMusicMgr,
								pPlaying, LTDMEnactImmediatly, 0);
						}
					}
				}
			}

			g_pDirectMusicMgr->m_pPerformance->FreePMsg(pMsg);
		}
	}

	LeaveCriticalSection(&g_pDirectMusicMgr->m_CriticalSection);
	g_pDirectMusicMgr->m_pPerformance->SetNotificationHandle(LTNULL, 0);
	CloseHandle(g_pDirectMusicMgr->m_hNotifyEvent);
	_endthread();
}


// 0x00440da0 and 0x00440ee0, 18 bytes each: iltdirectmusic.h's
// three-argument PlayMotif and StopMotif (vtable 0x004a2500 slots 12 and 13).
// The shipped build takes no style name, so each drops it and hands the rest
// to the two-argument overload -- through the vtable, `call *0x50(%eax)` /
// `call *0x54(%eax)`, because those are virtual (slots 20 and 21).
LTRESULT CLTDirectMusicMgr::PlayMotif(const char* sStyleName,
                                      const char* sMotifName,
                                      const LTDMEnactTypes nStart)
{
	return PlayMotif(sMotifName, nStart);
}

LTRESULT CLTDirectMusicMgr::StopMotif(const char* sStyleName,
                                      const char* sMotifName,
                                      const LTDMEnactTypes nStart)
{
	return StopMotif(sMotifName, nStart);
}


// 0x004420a0, 11 bytes: CDMSegment::GetName.  The allocated name when there
// is one, else the inline buffer -- `mov 0x4c(%ecx),%eax; test; jne` to the
// `ret`, then `lea 0xc(%ecx),%eax`.  Among the list helpers at
// 0x00441fe0..0x00442240 (both FindByNames call it twice, once to test and
// once for the compare).
const char* CDMSegment::GetName()
{
	if (m_pName)
		return m_pName;

	return m_szName;
}
