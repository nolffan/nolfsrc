// server.dll / lithtech.exe — the engine's object banks (reconstruction).
//
// A bank is a vptr over a StructBank: fixed-size records out of chunks, a
// free list, and four virtual slots {deleting destructor, Allocate, Free,
// Init}. The eleven per-object-type banks are objbank.cpp's; the engine's
// banks of plain records (the hash elements, the transfer-file records, the
// list nodes, the thread messages) are the same template over other record
// types.
//
// The file name is ours.
#ifndef __OBJBANK_H__
#define __OBJBANK_H__

#include <new.h>
#include "servermgr.h"


// The interface the banks are reached through. It carries nothing: the
// concrete bank restores this vtable on the way out of its own destructor,
// which is what says it is here at all, and every slot is pure.
//
// NOT a template, and the teardown at 0x1002ca7c is what proves it: that
// function expands eleven bank destructors and restores this vftable after
// each one from a REGISTER, in two bytes, which it can only do if all eleven
// restores name one symbol. Templated here, ours were eleven and each cost a
// six-byte immediate -- 322 bytes against 285. objtypes.cpp reaches the same
// four slots through its own non-templated view, IObjectBank, and names the
// second one Allocate returning CObjBase*, which is the concrete type that
// side needs; void* is the generic one, because CNetBank, CMessageBank and
// CFileBank restore this same vftable and hand out net packets, messages and
// file records rather than objects.
class CObjectBankBase
{
public:
	virtual ~CObjectBankBase() {}
	virtual void* Allocate() = 0;
	virtual void  Free(void* pObj) = 0;
	virtual void  Init() = 0;
};


// The bank itself: a vptr, a struct bank, and nothing else it owns. The
// free list Free pushes onto is the struct bank's own, at +0x18 of it.
template<class T>
class CObjectBank : public CObjectBankBase
{
public:
	// 0x1002449b builds eleven of these, each one `vptr = <table> ;
	// sb_Init2(&bank, <size>, 0x20, <grow>)`. The cache size is 0x20 every
	// time and the record size is the type's own, so the only thing that
	// varies between the eleven is the grow size -- which makes it the one
	// constructor argument. (That sizeof(T) reproduces all eleven shipped
	// immediates -- 0x1a8, 0x248, 0x270, 0x1c8, 0x1ac, 0x1d0, 0x25c,
	// 0x11f8, 0x21c, 0x274, 0x1b4 -- is an independent check on the eleven
	// object layouts this file already carries.)
	CObjectBank(uint32 nGrowBy)
	{
		sb_Init2(&m_Bank, sizeof(T), 0x20, nGrowBy);
	}

	// A bank built without a grow size takes sb_Init's default: three
	// arguments, `add $0xc,%esp` (the transfer-file records' bank).
	CObjectBank()
	{
		sb_Init(&m_Bank, sizeof(T), 0x20);
	}

	virtual ~CObjectBank()
	{
		sb_FreeChunks(&m_Bank, 1);
	}

	// THE VTABLE SLOT ONLY FORWARDS. server.dll's eleven slot-1 entries
	// (0x1004deb8 + 16*k) are not the bodies but a run of eleven five-byte
	// `jmp`s, 0x10025ab0..0x10025ae2, each onto its own 28-byte body
	// (0x10025b04, 0x10025b3e, ...): the slot is a one-line forward onto the
	// typed allocation below, and /O1 declines to expand that into it (a
	// template member is an expansion candidate in VC6 whether its body is
	// in the class or not, and this one, with sb_Allocate and the
	// constructor call inside it, is over /Os's budget), so `return
	// AllocateT();` comes out as the tail jump. lithtech.exe's /O2 expands
	// it and its slots point straight at the bodies.
	//
	// The same holds for a record type with no members to construct, but
	// only when it has a constructor at all: with one (even an empty one)
	// the typed body is still judged too big before the empty call goes,
	// and comes out as the shared eleven bytes 0x1000ed6f; without one it
	// is expanded into the slot and there is no jump.
	virtual void* Allocate() { return AllocateT(); }

	// The bank hands back raw memory, so the construction is explicit.
	// (Name ours.)
	T* AllocateT()
	{
		T* pRet = (T*)sb_Allocate(&m_Bank);

		if (pRet)
			new(pRet) T;

		return pRet;
	}

	// Destructed, not released: the flag tells the deleting destructor to
	// leave the memory alone, and it goes back to the bank instead.
	virtual void Free(void* pObj)
	{
		((T*)pObj)->~T();

		sb_Free(&m_Bank, pObj);
	}

	// Starting over drops whatever the bank was holding first, so Init
	// doubles as a reset.
	virtual void Init()
	{
		sb_FreeChunks(&m_Bank, 1);
		sb_Init(&m_Bank, sizeof(T), 0x20);
	}

public:
	StructBank m_Bank;                       // +0x04
	uint32     m_Unknown20;                  // +0x20
};

#endif  // __OBJBANK_H__
