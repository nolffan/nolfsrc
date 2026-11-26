// server.dll — LMessageImpl reader/writer bodies (reconstruction). Build /O1.
// (The neighboring localdriver TU is the /Os one — WriteMessage matches
// under /O1, so this TU keeps the server default.)
#include <string.h>
#include "servermsg.h"
#include "iltmessage.h"
#include "de_error.h"

LTRESULT LMessageImpl::ReadStringFL(char* pStr, uint32 maxLen)
{
	uint32 i;

	// lithtech.exe 0x0043d596 zeroes i AFTER both parameter checks and
	// re-tests the loop bound (`test %ebp,%ebp; jbe`), so the zero is the
	// for's own; and the drain is a read, then `while (ch)` read again
	// (0x0043d5bd, a peeled copy then 0x0043d5d1's loop).  /O1 gives the
	// same bytes as the old spelling for both, which is why server.dll
	// never told them apart.
	if (!pStr || maxLen <= 0)
		RETURN_ERROR_NAMED(2, "LMessageImpl::ReadStringFL", LT_INVALIDPARAMS);

	for (i = 0; i < maxLen; i++)
	{
		uint8 ch;
		((IMessageRead*)this)->ReadByte(&ch);
		pStr[i] = ch;
		if (!ch)
			return LT_OK;
	}

	// Buffer full: terminate what fits and drain the rest of the string.
	pStr[maxLen - 1] = 0;
	{
		uint8 ch;

		((IMessageRead*)this)->ReadByte(&ch);
		while (ch)
			((IMessageRead*)this)->ReadByte(&ch);
	}

	return LT_OK;
}

// The /O2 copy (lithtech.exe 0x43d6e0) read the shape where the /O1 one
// could not: the copy count is `limit, then nLen if smaller` (LTMIN), a
// fresh local rather than the reused parameter; the drain runs its own
// counter (`sub %ebp,%edi ; mov %edi,%esi`) instead of decrementing nLen;
// and the empty/absent test guards the body rather than returning early,
// so its exit shares the one epilogue. With the reused-parameter shape the
// /O1 build was a callee-saved rotation and the length word sat in the
// wrong dead slot; this shape is byte-identical on both images.
LTRESULT LMessageImpl::ReadHStringAsStringFL(char* pStr, uint32 maxLen)
{
	uint32 nLen, i, nCopy;

	if (!pStr || maxLen <= 0)
		RETURN_ERROR_NAMED(2, "ILTMessage::ReadHStringAsStringFL", LT_INVALIDPARAMS);

	pStr[0] = 0;

	{
		uint16 nRead;

		((IMessageRead*)this)->ReadWord(&nRead);

		nLen = nRead;
	}

	if (nLen != 0 && nLen != 0xffff)
	{
		nCopy = LTMIN(nLen, maxLen - 1);

		for (i = 0; i < nCopy; i++)
		{
			uint8 ch;
			((IMessageRead*)this)->ReadByte(&ch);
			pStr[i] = ch;
		}
		pStr[i] = 0;

		// Longer than the buffer: drain what did not fit.
		if (nLen > nCopy)
		{
			for (i = nLen - nCopy; i; i--)
			{
				uint8 ch;
				((IMessageRead*)this)->ReadByte(&ch);
			}
		}
	}

	return LT_OK;
}

LTRESULT LMessageImpl::WriteMessage(IMessageWrite* hMsg)
{
	LMessageImpl* pMsg = (LMessageImpl*)hMsg;

	if (hMsg && hMsg->Verify() != LT_OK)
	{
		// A dead message writes the 0xffff sentinel instead.
		((IMessageWrite*)this)->WriteWord(0xffff);
		RETURN_ERROR_NAMED_NUM(1, "LMessageImpl::WriteMessage", LT_NOTINITIALIZED);
	}

	// The length word, then the payload minus its leading id byte — the
	// length re-read from the owner every iteration.
	((IMessageWrite*)this)->WriteWord(pMsg->m_pOwner->m_Unknown34 - 1);

	int i = 0;
	while (i < (int)(uint32)(pMsg->m_pOwner->m_Unknown34 - 1))
	{
		((IMessageWrite*)this)->WriteByte(pMsg->m_pOwner->m_pData[i + 1]);
		i++;
	}

	return LT_OK;
}


// ---- the rest of ILTMessage ---------------------------------------------- //
//
// Every fixed-width reader and writer below is the same one raw call with a
// constant length, dispatched through the message's own vtable rather than
// called directly — so a message kind that overrides the raw pair gets the
// whole interface for free. That is also why the dword and float versions
// are one function each: at this level a float is four bytes.

LTRESULT LMessageImpl::Release()
{
	m_pOwner->Release();

	return LT_OK;
}

LTRESULT LMessageImpl::ReadByteFL(uint8& val)
{
	return ((IMessageRead*)this)->ReadRawFL(&val, 1);
}

LTRESULT LMessageImpl::ReadWordFL(uint16& val)
{
	return ((IMessageRead*)this)->ReadRawFL(&val, 2);
}

LTRESULT LMessageImpl::ReadDWordFL(uint32& val)
{
	return ((IMessageRead*)this)->ReadRawFL(&val, 4);
}

LTRESULT LMessageImpl::ReadFloatFL(float& val)
{
	return ((IMessageRead*)this)->ReadRawFL(&val, 4);
}

LTRESULT LMessageImpl::WriteByte(uint8 val)
{
	return ((IMessageWrite*)this)->WriteRaw(&val, 1);
}

LTRESULT LMessageImpl::WriteWord(uint16 val)
{
	return ((IMessageWrite*)this)->WriteRaw(&val, 2);
}

LTRESULT LMessageImpl::WriteDWord(uint32 val)
{
	return ((IMessageWrite*)this)->WriteRaw(&val, 4);
}

LTRESULT LMessageImpl::WriteFloat(float val)
{
	return ((IMessageWrite*)this)->WriteRaw(&val, 4);
}

// 0x10014cf8 (server.dll, out of line) / inlined into lithtech.exe's
// ReadRawFL at 0x0043ddd5. It lives HERE, `inline`, not in servermsg.cpp:
// server.dll places it between ReadRawFL (0x10014cd8) and WriteRaw
// (0x10014d1a) — an inline /O1 declined, emitted after its first use — and
// lithtech.exe's /O2 ReadRawFL (0x0043ddc0, 64 bytes) has it expanded.
// Both images byte-identical this way.
//
// What the reader does after every read: if the cursor has
// caught up with the payload length and flag bit 0 is set, wind it back to
// one. The subtraction is real — the shipped code subtracts and tests the
// result rather than comparing the two words (name ours).
//
// (Historical, now identical:) NEAR: 34 of 34 bytes, eleven of eleven instructions, four bytes different
// and all of them the same fact — the shipped build loads m_Unknown38 first
// (into ecx) and m_Unknown34 second (into edx), so its `sub`/`test` name the
// opposite pair from ours. Our VC6 always puts the minuend in the
// first-loaded register, and nine source shapes fail to move it: the two
// members inline, each through a uint16 local, each through an int local,
// `(int)` casts on both, `-(m38 - m34) <= 0`, `0 >= m34 - m38`,
// `nLeft = m34; nLeft -= m38;`, the pOwner local removed so both reads go
// through pMsg->m_pOwner, and an inlined two-argument helper (which would
// evaluate right-to-left but comes out nine bytes longer because the uint16
// parameters get promoted twice). cl 8168, 8966 and 8804 all emit what 8447
// emits. Load-order lottery; documented and left.
inline void lmsg_AfterRead(LMessageImpl* pMsg)
{
	ServerMessage* pOwner = pMsg->m_pOwner;

	// The store goes back through pMsg->m_pOwner, not the local, and that is
	// what orders the loads: with the store through pOwner the compiler
	// loads +0x34 first and subtracts into it (`sub %edx,%ecx`), with this
	// store it loads +0x38 first and subtracts into the +0x34 register
	// (0x10014cff..0x10014d07 `movzwl 0x38 ; movzwl 0x34 ; sub %ecx,%edx`).
	// lithtech.exe's inlined copy (0x0043dde9) loads +0x38 first too. The
	// same load order falls out of sm_ReadClientCommands in smgr.cpp.
	// Writing the store to any other field also flips the order, so it is
	// the pOwner store aliasing the +0x38 read that forces +0x34 first.
	if (pOwner->m_Unknown34 - pOwner->m_Unknown38 <= 0 && (pOwner->m_MsgFlags & 1))
		pMsg->m_pOwner->m_Unknown38 = 1;
}


// The raw pair is where the queue entry finally sees the bytes.
LTRESULT LMessageImpl::ReadRawFL(void* pData, uint32 len)
{
	m_pOwner->ReadData(pData, len);

	lmsg_AfterRead(this);

	return LT_OK;
}

LTRESULT LMessageImpl::WriteRaw(void* pData, uint32 len)
{
	m_pOwner->WriteData(pData, len);

	return LT_OK;
}

// A string is its bytes and then a zero, one call each: nothing here writes
// a length, so the reader has to look for the terminator.
LTRESULT LMessageImpl::WriteString(char* pData)
{
	if (pData)
	{
		while (*pData)
		{
			((IMessageWrite*)this)->WriteByte(*pData);
			pData++;
		}
	}

	((IMessageWrite*)this)->WriteByte(0);

	return LT_OK;
}

LTRESULT LMessageImpl::WriteCompVector(LTVector& vec)
{
	lmsg_WriteCompVector(this, &vec);

	return LT_OK;
}

LTRESULT LMessageImpl::WriteCompRotation(LTRotation& rot)
{
	lmsg_WriteCompRotation(this, &rot);

	return LT_OK;
}

LTRESULT LMessageImpl::ReadCompRotationFL(LTRotation& rot)
{
	lmsg_ReadCompRotation(this, &rot);

	return LT_OK;
}

// Object handles do not go on the wire as themselves; whatever the message
// was set up with decides how.
LTRESULT LMessageImpl::ReadObjectFL(HOBJECT& hObj)
{
	return m_pObjectIO->ReadObject(this, &hObj);
}

LTRESULT LMessageImpl::WriteObject(HOBJECT hObj)
{
	return m_pObjectIO->WriteObject(this, hObj);
}

// Rewinding leaves the cursor at one, not zero: byte zero is the message id
// and was never part of the payload.
LTRESULT LMessageImpl::ResetPos()
{
	m_pOwner->m_Unknown38 = 1;

	return LT_OK;
}

// The queue entry's two overflow bits, reported in the interface's own
// vocabulary.
LTRESULT LMessageImpl::GetStatus(uint32& flags)
{
	flags = 0;

	if (m_pOwner->m_MsgFlags & 2)
		flags = LMSTAT_READOVERFLOW;

	if (m_pOwner->m_MsgFlags & 4)
		flags |= LMSTAT_WRITEOVERFLOW;

	return LT_OK;
}


// Vectors and rotations go component by component through the float
// primitives, so nothing here has to know how a float is encoded.
//
// BOTH NEAR (69 of 69 and 86 of 86 bytes): the shipped build reserves one
// local slot and quietly reuses the dead parameter slot for the second and
// later reads, so its first component copies through the FPU and the rest
// as integers. One local reproduces the size and every instruction but puts
// all the reads in the same slot; two or three locals reserve more frame and
// come out longer.
LTRESULT LMessageImpl::ReadVectorFL(LTVector& vec)
{
	{
		float f;
		((IMessageRead*)this)->ReadFloat(&f);
		vec.x = f;
	}
	{
		float f;
		((IMessageRead*)this)->ReadFloat(&f);
		vec.y = f;
	}
	{
		float f;
		((IMessageRead*)this)->ReadFloat(&f);
		vec.z = f;
	}

	return LT_OK;
}

LTRESULT LMessageImpl::ReadRotationFL(LTRotation& rot)
{
	{
		float f;
		((IMessageRead*)this)->ReadFloat(&f);
		rot.m_Quat[0] = f;
	}
	{
		float f;
		((IMessageRead*)this)->ReadFloat(&f);
		rot.m_Quat[1] = f;
	}
	{
		float f;
		((IMessageRead*)this)->ReadFloat(&f);
		rot.m_Quat[2] = f;
	}
	{
		float f;
		((IMessageRead*)this)->ReadFloat(&f);
		rot.m_Quat[3] = f;
	}

	return LT_OK;
}

// NEAR (161 bytes against 163): two instructions. Giving the length its own
// scope puts it in the dead parameter slot as shipped — the reference it was
// handed is cached in a register before the read, which frees the slot — but
// the shipped build then keeps it in a register across both tests and the
// trip count, where ours re-reads the slot and needs a copy to carry it past
// the scope. Widening the scope over the loop puts the length back in a
// local of its own and costs more than it saves.
//
// A compressed vector is one component at full precision and the other two
// as eighteen-bit fractions of it — which is what makes it worth compressing
// at all: a direction's largest component bounds the others, so eighteen bits
// of each is plenty once they are scaled by it.
//
// The flag byte carries everything else in eight bits: two bits of each
// fraction's high end (the words hold the low sixteen), a sign for each, and
// two bits saying which axis kept its full float. Both signs are applied by
// multiplying by minus one rather than by negating, which is in the shipped
// bytes and not a spelling of ours.
//
// The second word and the flag byte live in sibling scopes, and the frame
// says so: both images give them one slot (/O2 the same address, /O1 the
// word at -0x2 and the byte at -0x1 of one dword; lithtech.exe 0x43d820,
// server.dll 0x10014830). Declared together at function scope, or with only
// the word scoped, they get disjoint slots.
LTRESULT LMessageImpl::ReadCompVectorFL(LTVector& vec)
{
	float  fMax;
	uint16 nLowA;
	uint32 nA;
	uint32 nB;
	float  fA;
	float  fB;

	((IMessageRead*)this)->ReadFloat(&fMax);
	((IMessageRead*)this)->ReadWord(&nLowA);
	{
		uint16 nLowB;
		((IMessageRead*)this)->ReadWord(&nLowB);
		nB = nLowB;
	}

	{
		uint8 nFlags;
		((IMessageRead*)this)->ReadByte(&nFlags);

		nA = ((uint32)(nFlags & 0x18) << 13) + nLowA;
#ifdef NOLFSRC_MODERN
		// fixed: the compressed-vector codec does not round-trip
		// (RETAIL_BUGS.md). The writer scales by 2^18 - 1, so the reader
		// divides by the same; a retail writer is read exactly too.
		fA = (float)nA * fMax * (1.0f / 262143.0f);
#else
		fA = (float)nA * fMax * (1.0f / 262144.0f);
#endif

		if (nFlags & 0x20)
			fA = fA * -1.0f;

		nB += (uint32)(nFlags & 0x03) << 16;
#ifdef NOLFSRC_MODERN
		fB = (float)nB * fMax * (1.0f / 262143.0f);
#else
		fB = (float)nB * fMax * (1.0f / 262144.0f);
#endif

		if (nFlags & 0x04)
			fB = fB * -1.0f;

		switch (nFlags >> 6)
		{
			case 1:
				vec.x = fA;
				vec.y = fMax;
				vec.z = fB;
				break;

			case 2:
				vec.x = fA;
				vec.y = fB;
				vec.z = fMax;
				break;

			default:
				vec.x = fMax;
				vec.y = fA;
				vec.z = fB;
				break;
		}
	}

	return LT_OK;
}

// A message inside a message. The one that comes out is a real message of
// its own — its own packet, its own writer — rather than a window onto this
// one, so the caller can hold it after this message is gone. The reference
// the scoped handle takes is balanced by the extra one added at the end: the
// destructor's release then leaves the caller holding the last count.
//
// A length of all-ones means there was no message, and the caller gets
// nothing rather than an empty one.
//
// The copy loop is `while (i--)` on the sixteen-bit count, not
// `for (i = 0; i < nCount; i++)`. The two run the same number of times, and
// the shipped code says which was written: a post-decrement needs the
// decremented value, so the trip count comes out as `lea -0x1(%eax),%ecx;
// movzwl %cx,%edi; inc %edi` — the sixteen-bit `i - 1`, widened, plus one.
// The `for` form lets VC6 simplify that to one `movzwl` of the count, which
// is two instructions and two bytes shorter and not what shipped.
LTRESULT LMessageImpl::ReadMessageFL(LMessageImpl*& pMsg)
{
	CMessageRef  msg;
	CAbstractIO* pIO;
	uint16       i;
	uint16       nCount;

	msg.Init(msg_AllocPacket(&pIO, 0x1000, 0x1000));

	if (pIO)
		pIO->Release();

	pMsg = &msg.m_pMsg->m_Write;

	{
		uint16 nLen;

		((IMessageRead*)this)->ReadWord(&nLen);

		if (nLen == 0xffff)
		{
			pMsg = LTNULL;

			return LT_OK;
		}

		nCount = nLen;
	}

	i = nCount;

	while (i--)
	{
		uint8 ch;

		((IMessageRead*)this)->ReadByte(&ch);

		msg.m_pMsg->WriteByte(ch);
	}

	msg.m_pMsg->AddRef();
	msg.m_pMsg->m_Unknown38 = 1;

	return LT_OK;
}

// Identical on both images.  The lithtech.exe copy (0x0043d630) fixed the
// shape: ONE exit (the empty string and the real one share the
// sb_CreateStringRaw call, `push $g_EmptyString; jmp` at 0x0043d66c), the
// truncation as an if/else whose `nExtra = 0` arm comes first (0x0043d678
// `ja` over `xor %ebx,%ebx`), and nCount itself, not a widened copy, as the
// ReadRawFL count and the terminator index.  nLen is read once into a plain
// copy because its address went to ReadWord.
//
// A string handle off the wire. The length that precedes it is all-ones for
// no string at all and zero for an empty one, which is why those two answer
// differently: nothing, against a handle on the empty string.
//
// Anything longer than the buffer is truncated rather than refused, and the
// excess is still read out of the message one byte at a time — the stream has
// to be left at the right place whatever happens to the string.
LTRESULT LMessageImpl::ReadHStringFL(HSTRING& hString)
{
	char   szBuf[0x2000];
	uint8  ch;
	uint16 nLen;
	uint16 nCount;
	uint16 nExtra;
	uint32 n;

	hString = LTNULL;

	((IMessageRead*)this)->ReadWord(&nLen);

	nCount = nLen;

	if (nCount != 0xffff)
	{
		if (nCount == 0)
		{
			hString = sb_CreateStringRaw(g_EmptyString);
		}
		else
		{
			if (nCount <= 0x2000)
			{
				nExtra = 0;
			}
			else
			{
				nExtra = nCount - 0x2000;
				nCount = 0x2000;
			}

			((IMessageRead*)this)->ReadRawFL(szBuf, nCount);

			szBuf[nCount - 1] = 0;

			if (nExtra)
			{
				n = nExtra;

				do
				{
					((IMessageRead*)this)->ReadByte(&ch);

					n--;
				}
				while (n);
			}

			hString = sb_CreateStringRaw(szBuf);
		}
	}

	return LT_OK;
}

// A rotation goes out as its four quaternion components, one float each —
// the same shape as a vector with one more term, and through the same slot.
LTRESULT LMessageImpl::WriteRotation(LTRotation& rot)
{
	((IMessageWrite*)this)->WriteFloat(rot.m_Quat[0]);
	((IMessageWrite*)this)->WriteFloat(rot.m_Quat[1]);
	((IMessageWrite*)this)->WriteFloat(rot.m_Quat[2]);
	((IMessageWrite*)this)->WriteFloat(rot.m_Quat[3]);

	return LT_OK;
}

// A string handle goes out as its length and then its bytes. There is no
// handle on the wire: the receiver builds its own. A null handle is all-ones
// rather than a zero length, so an empty string and no string stay apart.
LTRESULT LMessageImpl::WriteHString(HSTRING hString)
{
	char*  pStr;
	uint32 nLen;

	if (hString)
	{
		pStr = sb_GetStringDataAndLen(hString, &nLen);

		((IMessageWrite*)this)->WriteWord((uint16)nLen);
		((IMessageWrite*)this)->WriteRaw(pStr, nLen);
	}
	else
	{
		((IMessageWrite*)this)->WriteWord(0xffff);
	}

	return LT_OK;
}

// The same wire format from a plain string, which saves the caller building
// a handle only to send it. The terminator is counted and sent.
LTRESULT LMessageImpl::WriteStringAsHString(char* pStr)
{
	uint16 nLen;

	if (pStr)
	{
		nLen = (uint16)(strlen(pStr) + 1);

		((IMessageWrite*)this)->WriteWord(nLen);
		((IMessageWrite*)this)->WriteRaw(pStr, nLen);
	}
	else
	{
		((IMessageWrite*)this)->WriteWord(0xffff);
	}

	return LT_OK;
}

LTRESULT LMessageImpl::WriteVector(LTVector& vec)
{
	((IMessageWrite*)this)->WriteFloat(vec.x);
	((IMessageWrite*)this)->WriteFloat(vec.y);
	((IMessageWrite*)this)->WriteFloat(vec.z);

	return LT_OK;
}

// A compressed position is relative to the world, so the scale comes from
// the side that owns one rather than from the message.
//
// BOTH NEAR (45 of 41 bytes): the shipped build hands the scale on as the
// four bytes it already is and ours reloads it through the FPU to pass it as
// a float.
LTRESULT LMessageImpl::WriteCompPos(LTVector& vec)
{
	CPosRef* pRef;

	m_pObjectIO->GetPosRef(&pRef);

	lmsg_WriteCompPos(this, &vec, pRef);

	return LT_OK;
}

LTRESULT LMessageImpl::ReadCompPosFL(LTVector& vec)
{
	CPosRef* pRef;

	m_pObjectIO->GetPosRef(&pRef);

	lmsg_ReadCompPos(this, &vec, pRef);

	return LT_OK;
}

// The varargs form only builds the list and hands it on; the list form is
// the one that does anything, and it too defers to the message's owner.
LTRESULT LMessageImpl::WriteHStringFormatted(int messageCode, ...)
{
	va_list marker;

	va_start(marker, messageCode);

	return ((IMessageWrite*)this)->WriteFormattedHString((char*)messageCode, &marker);
}

LTRESULT LMessageImpl::WriteHStringArgList(int messageCode, va_list* pList)
{
	return m_pObjectIO->WriteFormattedHString(this, messageCode, pList);
}

