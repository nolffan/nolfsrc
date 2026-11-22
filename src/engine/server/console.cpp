// server.dll — the console's lookup and the command tokenizer
// (reconstruction). Build /O1.
//
// Console variables live in a hash keyed by name, including the terminating
// zero — which is why the length passed to the lookup is one more than the
// string length. The file name is ours.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "servermgr.h"
#include "de_error.h"

// 0x10005bf8 — a console variable by name.
HCONVAR con_FindVar(ConsoleState* pState, char* pName)
{
	void* hElement = dhash_FindElement(pState->m_hVarHash, pName, strlen(pName) + 1);

	if (hElement)
		return (HCONVAR)dhash_GetData(hElement);

	return LTNULL;
}

// 0x10005d3d — one command off the front of the parser's text. Running out
// clears the position so the next call reports nothing left, and the zero
// that says so is the same zero the test just produced.
int CommandParser::Next()
{
	if (!m_pPos)
		return 0;

	if (!parse_Command(m_pPos, &m_pPos, m_ArgBuffer, m_ArgPointers, &m_nArgs))
		m_pPos = LTNULL;

	return 1;
}


// ---- names and values --------------------------------------------------- //

// Console names are compared without regard to case. The raw character is
// kept alongside the folded one because it is the raw character that says
// where the string ends.
inline char con_ToUpper(char c)
{
	if (c >= 'a' && c <= 'z')
		return (char)(c - ('a' - 'A'));

	return c;
}

// 0x10005851 (server.dll) -- `inline`, which /O1 declines everywhere, so
// server.dll calls its out-of-line copy; lithtech.exe's /O2 expands it in
// con_FindBinding (0x0041b070) and con_RunStringEx (0x0041ac89), and its
// out-of-line 0x0042ee70 is another TU's (see clientconsole.cpp).  `*p1` is
// read at both tests with no `c1` local: with the local, the expanded copies
// walk p2 and index p1 off the difference, where shipped walks p1 (esi in
// FindBinding, edi in RunStringEx) and indexes p2.  Same bytes out of line.
inline LTBOOL con_ICompare(char* p1, char* p2)
{
	for (;;)
	{
		if (con_ToUpper(*p1) != con_ToUpper(*p2))
			return LTFALSE;

		if (*p1 == 0)
			return LTTRUE;

		p1++;
		p2++;
	}
}

// 0x10005892 — every value a variable is ever set to is filed in a hash and
// handed back as the stored key, so two variables holding the same text hold
// the same pointer and a change can be detected by comparing pointers.
//
// The key length is the string plus its terminator, and the `+ 1` is written
// at the two call sites rather than folded into the length: the shipped build
// forms it with `lea 0x1(%eax),%edi` — a *new* value in a *new* register —
// where folding it into the initialiser gives `mov %eax,%esi; inc %esi`, an
// update of the variable itself. Same three bytes, different register roles,
// and one instruction more. Two functions here turned on that distinction.
//
// A failed add returns NULL from inside the `if`, and the key lookup is the
// one return at the end: lithtech.exe's /O2 twin (0x0041af00) gives the
// failed add its own `pop; pop; pop; ret` and jumps both successes forward
// into the dhash_GetKey call. The single-result form (`if (pRet) pRet =
// GetKey(...)`) shared the epilogue there and came out four bytes short;
// server.dll's /O1 0x10005892 is identical either way.
char* con_InternString(ConsoleState* pState, char* pStr)
{
	uint32 nLen = strlen(pStr);
	char*  pRet;

	pRet = (char*)dhash_FindElement((CHashTable*)pState->m_hStringHash, pStr, nLen + 1);

	if (!pRet)
	{
		pRet = (char*)dhash_AddElement((CHashTable*)pState->m_hStringHash, pStr, nLen + 1);

		if (!pRet)
			return LTNULL;
	}

	return dhash_GetKey(pRet, LTNULL);
}

// 0x100058d9 — the variable for a name, made if it is not there yet. The
// name it keeps is the hash's copy of the key, not the caller's string.
//
// The same key-length shape as con_InternString: `strlen` into the length and
// the `+ 1` at each call site.
LTCommandVar* con_CreateVar(ConsoleState* pState, char* pName)
{
	uint32        nLen = strlen(pName);
	void*         hElement;
	void*         hNew;
	LTCommandVar* pVar;

	hElement = dhash_FindElement((CHashTable*)pState->m_hVarHash, pName, nLen + 1);

	if (hElement)
		return (LTCommandVar*)dhash_GetData(hElement);

	pVar = (LTCommandVar*)pState->m_pAlloc(sizeof(LTCommandVar));

	if (!pVar)
		return LTNULL;

	memset(pVar, 0, sizeof(LTCommandVar));

	hNew = dhash_AddElement((CHashTable*)pState->m_hVarHash, pName, nLen + 1);

	if (!hNew)
	{
		pState->m_pFree(pVar);
		return LTNULL;
	}

	pVar->m_pName = dhash_GetKey(hNew, LTNULL);
	pVar->m_hElement = hNew;

	dhash_SetData(hNew, pVar);

	return pVar;
}


// ---- the bindings ------------------------------------------------------- //

// 0x100059ad — the binding that wants this name, if any. A linear walk: the
// table is small and fixed at startup.
ConsoleBinding* con_FindBinding(ConsoleState* pState, char* pName)
{
	int i;

	for (i = 0; i < pState->m_nBindings; i++)
	{
		if (con_ICompare(pState->m_pBindings[i].m_pName, pName))
			return &pState->m_pBindings[i];
	}

	return LTNULL;
}

// 0x1000595c — push a variable's value out to whichever C variables the
// binding named. Each one is optional, so a binding can take just the float,
// just the integer, or the text.
void con_ApplyBinding(ConsoleState* pState, LTCommandVar* pVar)
{
	ConsoleBinding* pBind = con_FindBinding(pState, pVar->m_pName);

	if (pBind)
	{
		if (pBind->m_ppVar)
			*pBind->m_ppVar = pVar;

		if (pBind->m_pFloat)
			*pBind->m_pFloat = pVar->m_FloatVal;

		if (pBind->m_pInt)
			*pBind->m_pInt = (int)pVar->m_FloatVal;

		if (pBind->m_ppString)
			*pBind->m_ppString = pVar->m_pStringVal;
	}
}

// 0x100059ec — set a variable by name, making it if it is new. The value is
// interned first so "did it change" is a pointer comparison; the numeric form
// is only re-parsed when the text actually moved. A variable that came into
// existence and one that merely changed go to different notifications.
void con_SetVar(ConsoleState* pState, char* pName, char* pVal)
{
	LTCommandVar* pVar;
	char*         pInterned;
	int           bCreated = 0;
	int           bChanged = 0;

	pVar = (LTCommandVar*)con_FindVar(pState, pName);

	if (!pVar)
	{
		bChanged = 1;
		bCreated = 1;

		pVar = con_CreateVar(pState, pName);
	}

	pInterned = con_InternString(pState, pVal);

	if (pInterned != pVar->m_pStringVal)
	{
		pVar->m_pStringVal = pInterned;
		pVar->m_FloatVal = (float)atof(pVal);

		bChanged = 1;
	}

	con_ApplyBinding(pState, pVar);

	if (bChanged)
	{
		if (bCreated)
		{
			if (pState->m_pOnCreate)
				pState->m_pOnCreate(pState, pVar);
		}
		else
		{
			if (pState->m_pOnChange)
				pState->m_pOnChange(pState, pVar);
		}
	}
}


// ---- the command list --------------------------------------------------- //

// 0x10005b91 — register one command. A record that cannot be allocated is
// simply not registered.
//
// It RETURNS the record (NULL when the allocation failed). Both images keep
// pCmd in eax untouched to the final ret, and the null test's early exit
// gets its own `pop esi; ret` because eax already holds the NULL it must
// return; declared void, VC6 merged that exit into the tail and came out 61
// bytes. The twin is lithtech 0x0041b2f0, identical with the same change.
ConsoleCommand* con_AddCommand(ConsoleState* pState, char* pName, void* pFn, void* pUser)
{
	ConsoleCommand* pCmd = (ConsoleCommand*)pState->m_pAlloc(sizeof(ConsoleCommand));

	if (!pCmd)
		return NULL;

	pCmd->m_pFn = pFn;
	pCmd->m_pName = pName;
	pCmd->m_pUser = pUser;
	pCmd->m_Link.m_pData = pCmd;

	dl_Insert(&pState->m_CommandHead, &pCmd->m_Link);
	return pCmd;
}

// 0x10005bd0 — unregister one and hand the record back.
void con_RemoveCommand(ConsoleState* pState, ConsoleCommand* pCmd)
{
	LTLink* pLink = &pCmd->m_Link;

	pLink->m_pPrev->m_pNext = pLink->m_pNext;
	pLink->m_pNext->m_pPrev = pLink->m_pPrev;
	pLink->TieOff();

	pState->m_pFree(pCmd);
}

// 0x10005a6b — the two hashes and the built-in commands. Both hashes are
// case-insensitive string hashes of the same size: 500 buckets, of which the
// last only ever holds collisions.
void con_Init(ConsoleState* pState)
{
	int i;

	pState->m_hStringHash = dhash_Create(500, DHASHKEY_STRING);
	pState->m_hVarHash = dhash_Create(500, DHASHKEY_STRING);

	pState->m_CommandHead.TieOff();

	for (i = 0; i < pState->m_nBuiltins; i++)
	{
		con_AddCommand(pState, pState->m_pBuiltins[i].m_pName,
		               pState->m_pBuiltins[i].m_pFn,
		               pState->m_pBuiltins[i].m_pUser);
	}
}

// 0x10005ac4 — the whole console goes away. Every binding that named a
// variable is pointed at nothing first: the records are about to be freed and
// the C variables outlive the console.
void con_Term(ConsoleState* pState)
{
	int     i;
	LTLink* pCur;
	LTLink* pNext;
	void*   hIter;
	void*   hCur;

	for (i = 0; i < pState->m_nBindings; i++)
	{
		if (pState->m_pBindings[i].m_ppString)
			*pState->m_pBindings[i].m_ppString = LTNULL;
	}

	pCur = pState->m_CommandHead.m_pNext;

	while (pCur && pCur != &pState->m_CommandHead)
	{
		pNext = pCur->m_pNext;

		con_RemoveCommand(pState, (ConsoleCommand*)pCur->m_pData);

		pCur = pNext;
	}

	hIter = dhash_GetFirst(pState->m_hStringHash);

	while (hIter)
	{
		hCur = dhash_GetNext(&hIter);

		if (hCur)
			pState->m_pFree(dhash_GetData(hCur));
	}

	dhash_Free(pState->m_hStringHash);

	hIter = dhash_GetFirst(pState->m_hVarHash);

	while (hIter)
	{
		hCur = dhash_GetNext(&hIter);

		if (hCur)
			pState->m_pFree(dhash_GetData(hCur));
	}

	dhash_Free(pState->m_hVarHash);

	memset(pState, 0, 0x44);
}


// ---- the tokenizer ------------------------------------------------------ //

// One token off the front of the text, copied into the argument buffer with
// its own terminator. The answer says what was found: nothing left at all,
// an argument, or the end of a statement.
//
// Quoting is a depth counter rather than a flag, so "(a (b c))" comes through
// as one argument with its inner brackets intact, while a double quote is a
// one-deep bracket of its own. A per-cent sign escapes whatever follows it,
// including a bracket or a quote. A token longer than 0x100 characters simply
// stops growing — the last character is overwritten rather than the buffer.
//
// The token loop is `while (TRUE)` with the zero test as a `break` at its
// head, and that alone is what keeps it UNROTATED as shipped (the test at
// the top, a bare `jmp` back): `for (;;)` with the same body rotates it,
// tail-duplicating the load and compare onto the back edge, and every other
// head shape measured (c-first, `while ((c = **ppPos) != 0)`, `while
// (**ppPos)`, a goto loop) rotates or peels too. The two "skip this
// character" exits -- the opening bracket and the opening quote -- are
// `goto next` onto the loop's own `(*ppPos)++` at the bottom: shipped jumps
// both of them there (0x10005e90, 0x10005eb2 -> 0x10005ed9); written as
// `(*ppPos)++; continue;` VC6 hosts the shared increment-and-jump in the
// bracket arm instead and the bottom jumps back up to it.
#define PARSE_DONE  0
#define PARSE_ARG   1
#define PARSE_END   2

// 0x10005e24. lithtech.exe 0x0041b710 (272 bytes) is BYTE-IDENTICAL.
//
// server.dll is NEAR, 238 of 238 and 98 instructions of 98, and the one
// residual is the shared-exit host of the two statement-end returns: shipped
// keeps `pop %eax; jmp <epilogue>` in the PARSE_END arm (`push $2; pop %eax`)
// and has PARSE_ARG's `push $1` jump back into it; ours hosts it in the
// PARSE_ARG arm. Measured, none moving it: an explicit else, the tests
// inverted (`if (*pStart) return PARSE_ARG;`, je and worse), a goto to an
// `arg:` label, `?:` with a comma, an nRet local set on both arms, literal
// 1/2, and `else if` (234). The esi/edi swap and the rotation that used to
// sit here went with the loop shape above.
static int parse_Token(char** ppPos, char** ppOut)
{
	char* pStart;
	char  c;
	int   nDepth = 0;

	while (**ppPos == ' ')
		(*ppPos)++;

	if (**ppPos == 0)
		return PARSE_DONE;

	pStart = *ppOut;

	while (TRUE)
	{
		if (**ppPos == 0)
			break;

		c = **ppPos;

		if (c == '%')
		{
			(*ppPos)++;
			c = **ppPos;

			if (c == 0)
				break;
		}
#ifdef NOLFSRC_MODERN
		// M-P1-33: a character above 127 is not a control character. The
		// signed char made it a negative index into the CRT's type table,
		// and some of them (for example 0xF3, "o" with an acute accent)
		// ended the command there: a binding of a mouse button with such a
		// name in Customize Controls did nothing (RETAIL_BUGS.md).
		else if (c == ';' || iscntrl((unsigned char)c))
#else
		else if (c == ';' || iscntrl(c))
#endif
		{
			**ppOut = 0;
			(*ppOut)++;

			if (*pStart == 0)
			{
				(*ppPos)++;
				return PARSE_END;
			}

			return PARSE_ARG;
		}
		else if (c == '(')
		{
			nDepth++;

			if (nDepth == 1)
				goto next;
		}
		else if (c == ')')
		{
			nDepth--;

			if (nDepth == 0)
			{
				(*ppPos)++;
				break;
			}
		}
		else if (c == '"')
		{
			if (nDepth != 0)
			{
				(*ppPos)++;
				break;
			}

			nDepth = 1;
			goto next;
		}
		else if (c == ' ' && nDepth == 0)
		{
			break;
		}

		**ppOut = c;

		if (*ppOut - pStart >= 0x100)
		{
			**ppOut = 0;
			(*ppOut)--;
		}

		(*ppOut)++;
	next:
		(*ppPos)++;
	}

	**ppOut = 0;
	(*ppOut)++;

	return (*pStart != 0);
}

// 0x10005dc6 — one statement's worth of tokens, up to thirty arguments. The
// buffer cursor runs on across the arguments, so each one keeps its own
// terminated copy and the pointers hand them back in order.
int parse_Command(char* pCommand, char** pNewCommandPos, char* argBuffer,
                  char** argPointers, int* nArgs)
{
	char* pCur = pCommand;
	char* pOut = argBuffer;
	int   nType;

	*nArgs = 0;

	for (;;)
	{
		nType = parse_Token(&pCur, &pOut);

		if (nType == PARSE_DONE)
			return 0;

		if (nType == PARSE_ARG)
		{
			argPointers[*nArgs] = argBuffer;
			(*nArgs)++;

			if (*nArgs >= 30)
				return 0;
		}
		else if (nType == PARSE_END)
		{
			break;
		}

		argBuffer = pOut;
	}

	*pNewCommandPos = pCur;

	return 1;
}

// The walk stops on the first command whose name matches; the parser is left
// pointing past it, so the caller reads its arguments and calls again.
// The flag says CASE SENSITIVE, and the name it used to carry here said the
// opposite. Both arms of the ternary were wrong and wrong in opposite
// directions, so the function was byte-identical either way -- the two calls
// sit at the same offsets and only the relocation behind each one differs.
// A check of the call targets is what separated them: the shipped call at +47 goes to
// strcmp (0x1003e1f0) where ours named stricmp, and the one at +54 goes to
// stricmp (0x10047080) where ours named strcmp. The one reconstructed call
// site passes 0 and so wants the case-INSENSITIVE compare, which is what a
// world file's "AmbientLight" property name needs.
LTBOOL CommandParser::Find(char* pName, LTBOOL bCaseSensitive, int nMinArgs)
{
	if (nMinArgs == 0)
		nMinArgs = 1;

	while (Next())
	{
		if (m_nArgs >= nMinArgs)
		{
			int bMatch = bCaseSensitive ? (strcmp(m_ArgPointers[0], pName) == 0)
			                            : (stricmp(m_ArgPointers[0], pName) == 0);

			if (bMatch)
				return LTTRUE;
		}
	}

	return LTFALSE;
}


// ---- the runner --------------------------------------------------------- //

// 0x10005623 — run a console string: one statement at a time, each either a
// registered command or a variable assignment.
//
// nSource is a bit mask and not an enumeration: bit 1 says "commands only",
// bit 2 says "variables only", and neither is checked for anything else. The
// fourth argument is OR'd straight into the variable's flags, which is how a
// caller marks everything a file sets as saveable.
//
// A leading '+' on the name adds flag bit 0 (the one con_SaveFile writes) and
// a leading '-' takes it away, and either way the sign is not part of the
// name. A value of "!" toggles: a variable with no text yet becomes "1", and
// one holding a number becomes the number's logical negation. Anything else
// under "!" is left alone -- the '!' itself is then stored as the value.
//
// The value is only re-stored when the text actually changed, and a value
// shorter than sixteen characters is copied into the variable's own first
// sixteen bytes rather than interned: m_pStringVal then points at the record
// itself, which is why LTCommandVar begins with a run of otherwise unnamed
// bytes.
//
// Two shapes in here are not free choices. The three flags are declared one
// at a time with a statement between each, because that is what puts three
// separate `and dword ptr [x],0` in the output: VC6 selects that four-byte
// form per statement, but batches *consecutive* zero initialisers into one
// `xor` plus three three-byte stores, which is a byte shorter and not what
// ships. And the "!" handling is a positive `if (pVar->m_pStringVal)` with
// an `else`, not a negative test with an `else if`: the shipped build defers
// the "1" assignment to a block after the numeric one and falls through from
// it into the join, which is the else arm's placement and not the then arm's.
//
// NEAR on lithtech.exe (0x0041ac20, 736/736): the reloads of pState for the
// change callback and con_PrintVar rotate through edx/ecx/edx/eax where ours
// uses ecx/edx/eax/ecx. Allocation; measured without effect: the callback
// select as if/else, a fresh local for the one-argument lookup, pVar at
// function scope.
void con_RunStringEx(ConsoleState* pState, char* pString, int nSource, int nFlags)
{
	CommandParser parser;
	char          szTemp[52];

	parser.m_pPos = pString;

	while (parser.Next())
	{
		LTCommandVar* pVar;
		char*         pName;
		uint32        nSet;

		if (parser.m_nArgs <= 0)
			continue;

		if (!(nSource & 2))
		{
			LTLink* pCur;

			for (pCur = pState->m_CommandHead.m_pNext;
			     pCur != &pState->m_CommandHead;
			     pCur = pCur->m_pNext)
			{
				ConsoleCommand* pCmd = (ConsoleCommand*)pCur->m_pData;

				if (con_ICompare(pCmd->m_pName, parser.m_ArgPointers[0]))
				{
					((void (*)(int, char**))pCmd->m_pFn)(
						parser.m_nArgs - 1, &parser.m_ArgPointers[1]);

					goto next;
				}
			}
		}

		if (parser.m_nArgs == 2)
		{
			if (nSource & 1)
				continue;

			// Both flags before pName: lithtech's /O2 stores both slots and
			// then copies pName (0x0041ad0f..0x0041ad17); server.dll matches
			// either way.
			int bChanged = 0;
			int bCreated = 0;

			pName = parser.m_ArgPointers[0];

			nSet = nFlags;

			int bMinus = 0;

			if (pName[0] == '+')
			{
				nSet |= 1;
				pName++;
			}
			else if (pName[0] == '-')
			{
				bMinus = 1;
				pName++;
			}

			pVar = (LTCommandVar*)con_FindVar(pState, pName);

			if (!pVar)
			{
				bChanged = 1;
				bCreated = 1;

				pVar = con_CreateVar(pState, pName);
			}

			pVar->m_Flags |= nSet;

			if (bMinus)
				pVar->m_Flags &= ~1;

			if (parser.m_ArgPointers[1][0] == '!' &&
			    parser.m_ArgPointers[1][1] == 0)
			{
				if (pVar->m_pStringVal)
				{
					if (isdigit(pVar->m_pStringVal[0]) ||
					    pVar->m_pStringVal[0] == '-')
					{
						sprintf(szTemp, "%d", (int)pVar->m_FloatVal == 0);

						parser.m_ArgPointers[1] = szTemp;
					}
				}
				else
				{
					parser.m_ArgPointers[1] = "1";
				}
			}

			if (!pVar->m_pStringVal ||
			    strcmp(pVar->m_pStringVal, parser.m_ArgPointers[1]) != 0)
			{
				char* pNew;

				if (strlen(parser.m_ArgPointers[1]) < 16)
				{
					strcpy((char*)pVar, parser.m_ArgPointers[1]);

					pNew = (char*)pVar;
				}
				else
				{
					pNew = con_InternString(pState, parser.m_ArgPointers[1]);
				}

				pVar->m_pStringVal = pNew;

				pVar->m_FloatVal = (float)atof(parser.m_ArgPointers[1]);

				bChanged = 1;
			}

			con_ApplyBinding(pState, pVar);

			if (bChanged)
			{
				void (*pFn)(ConsoleState*, LTCommandVar*) =
					bCreated ? pState->m_pOnCreate : pState->m_pOnChange;

				if (pFn)
					pFn(pState, pVar);
			}
		}
		else if (parser.m_nArgs == 1)
		{
			pVar = (LTCommandVar*)con_FindVar(pState, parser.m_ArgPointers[0]);

			if (pVar)
				con_PrintVar(pState, pVar);
		}

	next:
		;
	}
}


// ---- files -------------------------------------------------------------- //

// 0x10005c25 — run a file a line at a time. A file that will not open is not
// an error worth saying anything about: it simply did not run.
LTBOOL con_RunFile(ConsoleState* pState, char* pFilename, int nSource, int n)
{
	char  szLine[500];
	FILE* pFile;

	pFile = fopen(pFilename, "rt");

	if (!pFile)
		return LTFALSE;

	while (fgets(szLine, sizeof(szLine), pFile))
		con_RunStringEx(pState, szLine, nSource, n);

	fclose(pFile);

	return LTTRUE;
}

// 0x10005c95 — write every variable that asked to be saved, then let the
// registered writers add whatever else belongs in the file.
LTBOOL con_SaveFile(ConsoleState* pState, char* pFilename)
{
	FILE*         pFile;
	void*         hIter;
	void*         hCur;
	LTCommandVar* pVar;
	int           i;

	pFile = fopen(pFilename, "wt");

	if (!pFile)
		return LTFALSE;

	hIter = dhash_GetFirst(pState->m_hVarHash);

	while (hIter)
	{
		hCur = dhash_GetNext(&hIter);

		if (hCur)
		{
			pVar = (LTCommandVar*)dhash_GetData(hCur);

			if (pVar->m_Flags & 1)
				fprintf(pFile, "\"%s\" \"%s\"\n", pVar->m_pName, pVar->m_pStringVal);
		}
	}

	for (i = 0; i < pState->m_nSavers; i++)
		pState->m_pSavers[i](pFile);

	fclose(pFile);

	return LTTRUE;
}

// 0x10005d1e — one variable, as the console shows it. A variable with no
// text yet says nothing at all.
void con_PrintVar(ConsoleState* pState, LTCommandVar* pVar)
{
	if (pVar->m_pStringVal)
		pState->m_pPrint("%s = %s", pVar->m_pName, pVar->m_pStringVal);
}


// The two forwarders the rest of the engine reaches the runner through: the
// source and the extra flag both default to nothing (names ours).
void con_RunString(ConsoleState* pState, char* pString)
{
	con_RunString3(pState, pString, 0);
}

void con_RunString3(ConsoleState* pState, char* pString, int nSource)
{
	con_RunStringEx(pState, pString, nSource, 0);
}


// ---- the server's console ------------------------------------------------ //

// 0x10028f52 — the server's own console. Everything the console reaches the
// outside world through is filled in here, which is what lets the same
// console code serve a server and a client: the printer, the allocator, and
// the two notifications the server hangs off variable creation and change.
//
// The clear covers 0x44 bytes, not the whole object: the eight bytes past
// that belong to whoever embedded it.
void cc_InitState(CServerMgr* pMgr, ConsoleState* pState)
{
	memset(pState, 0, 0x44);

	pState->m_pSavers = LTNULL;
	pState->m_nSavers = 0;

	pState->m_pBindings = dsi_GetConsoleBindings();
	pState->m_nBindings = dsi_GetNumConsoleBindings();

	pState->m_pBuiltins = g_ConsoleCommands;
	pState->m_nBuiltins = 8;

	pState->m_pPrint = dsi_ServerError;
	pState->m_pAlloc = (void* (*)(uint32))(void* (*)(size_t))::operator new;
	pState->m_pFree = ::operator delete;
	pState->m_pOnCreate = cc_OnVarCreated;
	pState->m_pOnChange = cc_OnVarChanged;

	pState->m_pServerMgr = pMgr;

	con_Init(pState);
}

void cc_TermState(ConsoleState* pState)
{
	con_Term(pState);
}


// ---- the eight built-in console commands -------------------------------- //
//
// servermgr.h declares 0x10034521 as `void sm_BroadcastPrint(char* pMsg)`
// with one fixed argument; it is variadic, and smgr.cpp already carries the
// corrected declaration under the name below.
void sm_BroadcastPrintf(char* pFormat, ...);
//
// The names are not ours: g_ConsoleCommands at 0x10053d80 is eight
// {name, fn, user} triples and reading it names every handler outright --
// "Portal" 0x10028ad9, "ShowGameVars" 0x10028b47, "ShowUsedFiles" 0x10028b00,
// "world" 0x10028b9d, "objectinfo" 0x10028be3, "DisableWMPhysics" 0x10028d7e,
// "ExhaustMemory" 0x10036d21 and "SpawnObject" 0x10028da3. Every handler is
// cdecl (int argc, char** argv).
//
// "ExhaustMemory" is worth a note: its entry points at 0x10036d21, the image's
// one shared empty function, so the command parses and does nothing. It is a
// debug command that was compiled out of the release build without being
// taken off the table.

// 0x10028ad9 — "Portal <name> <0|1>": open or close one of the world's
// portals (lithtech twin 0x00466870). The guard is `argc < 2` with the whole
// body under it; both images test the count before touching argv and there
// is no usage line. The name is argv[0] and the flag argv[1] -- argv does not
// carry the command name, the same convention cc_World is written to.
void cc_Portal(int argc, char** argv)
{
	if (argc < 2)
		return;

	sm_SetPortalFlags(g_pServerMgr, argv[0], atoi(argv[1]));
}

// 0x10028b00 — "ShowUsedFiles": every filename the file manager has a record
// for, broadcast one line each. The names come back as the HASH KEYS rather
// than out of the records, which is what dhash_GetKey is for.
void cc_ShowUsedFiles(int argc, char** argv)
{
	void* hIter;
	void* hElement;

	if (!g_pServerMgr)
		return;

	hIter = dhash_GetFirst(g_pServerMgr->m_FileMgr.m_hFileHash);

	if (!hIter)
		return;

	do
	{
		// The walk step has to be its own statement: nested inside the
		// GetKey call, VC6's right-to-left argument order hoists GetKey's
		// null length ABOVE the advance and the shipped batching is lost.
		hElement = dhash_GetNext(&hIter);

		sm_BroadcastPrintf("Used file: %s", dhash_GetKey(hElement, LTNULL));
	}
	while (hIter);
}

// 0x10028b47 — "ShowGameVars": a banner, then every console variable through
// con_PrintVar. The list comes off the console state's own name hash, and the
// records out of the elements rather than the keys -- the mirror of
// ShowUsedFiles above, which wants the keys and not the records.
//
// The banner goes through dsi_ServerError, not the console's own m_pPrint:
// the whole point of these commands is that their output reaches the host
// application's log.
//
// lithtech.exe carries this body (0x004668f0) AND the client console's own
// cc_ShowGameVars (clientconsole.cpp, 0x0041e840), each reached only through
// its own TU's command table -- so in the client build this one is file-local
// (static), which is what lets both link.  Likewise cc_World below
// (0x00466950 against the client's 0x0041d840).  server.dll is unchanged.
#ifndef LT_SERVER_BUILD
static
#endif
void cc_ShowGameVars(int argc, char** argv)
{
	ConsoleState* pState;
	void*         hIter;
	void*         hElement;

	if (!g_pServerMgr)
		return;

	pState = &g_pServerMgr->m_ConsoleState;

	dsi_ServerError("Game vars --------------------");

	hIter = dhash_GetFirst(pState->m_hVarHash);

	if (!hIter)
		return;

	do
	{
		hElement = dhash_GetNext(&hIter);

		if (hElement)
			con_PrintVar(pState, (LTCommandVar*)dhash_GetData(hElement));
	}
	while (hIter);
}

// 0x10028be2 is `ret` -- see below.
//
// 0x100587a8 (.bss) -- the one global a console command's answer is left in:
// cc_World writes it, and sharedcommon.cpp's cc_RunConsoleString clears it
// before running a string and reads it back after, declaring it `extern`
// under this name (name ours).
int g_nConsoleResult;

// 0x10028b9d — "world <name> [mode]": start a world from the console.
//
// argv does NOT carry the command name: DoStartWorld is handed argv[0] as the
// world name, so the parser has already stripped it, and the `argc < 1` guard
// is "was there an argument at all".
//
// The mode defaults to 3 and drops to 2 when a second argument is present and
// atoi()s to zero -- so `world foo 0` and `world foo bar` both mean 2, and
// anything else leaves 3. Reading it the other way round (a nonzero second
// argument selecting 2) is what the `jne` forbids.
//
// The answer goes into a global rather than being returned: the handler is
// void, and 0x100587a8 is where a console-started world's result is left for
// whoever asks later.
#ifndef LT_SERVER_BUILD
static   // see cc_ShowGameVars above
#endif
void cc_World(int argc, char** argv)
{
	uint32 nMode;

	if (argc < 1)
		return;

	nMode = 3;

	if (argc >= 2 && atoi(argv[1]) == 0)
		nMode = 2;

	g_nConsoleResult =
		g_pServerMgr->DoStartWorld(argv[0], nMode, dsi_GetTime());
}

// 0x10028be3 / 0x004669a0 — "objectinfo": every live object, tallied by
// class name. Moved here out of src/engine/lithtech/ccobj.cpp's holding pen,
// which is a lithtech-only TU: parked there the body could only ever reach
// lithtech.exe's copy, and server.dll's 0x10028be3 had no source at all.
// console.cpp compiles into BOTH images, so this is where the pair lives.
//
// THE lithtech.exe ROW THE FUNCTION ENUMERATION REPORTS (0x004669aa, 370 bytes) IS
// NOT A FUNCTION.  0x004669a0 is `mov $0x7924,%eax ; call 0x00492c60` -- the
// __chkstk probe a >4KB frame needs -- and 0x004669aa is the instruction
// after it.  The enumeration split them because the probe is reached by a
// data reference (the command table) and the body only by fall-through.
// The real extent is 0x004669a0..0x00466b1c, 380 bytes, one `ret`, no
// branch target outside it.  g_ConsoleCommands' "objectinfo" slot is
// 0x10028be3 in server.dll; 0x004ad0d8 in lithtech.exe holds
// { "objectinfo", 0x004669a0, 0 }.

// strncpy (0x00492b60 / 0x1003f050) and strcmp (inlined / 0x1003e1f0) both
// come from <string.h>, included at the top.  VC6 has strcmp on its
// intrinsic list and strncpy NOT, which is exactly the split the shipped /O2
// body shows: the strcmp in the helper below is expanded and the strncpy
// here is still a call.

// What the tally reads off a class, at the offsets both images name.
//
// +0x00 is servermgr.h's CClassInfo::m_pClassName, reached as
// `pObject->m_pSData->m_pClass` then one dereference (0x00466a18 /
// 0x10028c66).
//
// +0x20 IS A NEW FIELD.  servermgr.h pads CClassInfo from 0x04 to 0x24 and
// this is inside that pad: `mov 0x24(%edx),%ecx ; mov 0x20(%ecx),%edx`
// (0x00466a76, and 0x10028cb6 at /O1) reads a dword that the tally sums as
// BYTES, which is ClassDef::m_ClassObjectSize copied onto the class record.
// Declared on a local view here rather than in servermgr.h because adding a
// field to CClassInfo restages every TU that includes it; it belongs in
// CClassInfo.
class CConClassInfo
{
public:
	char*  m_pClassName;                                // +0x00
	uint8  m_pad_04[0x20 - 0x04];
	uint32 m_nObjectSize;                               // +0x20
};

class CConObjClassView
{
public:
	uint8 m_pad_00[0x24];
	CConClassInfo* m_pClass;                           // +0x24  SObjData+0x24
};

// The manager's two fields this reads: the live-object list at 0x8dc
// (servermgr.h's m_List8dc) and nothing else.  Written through the global
// rather than a parameter -- a console handler gets only (argc, argv).

#define OBJINFO_MAXCLASSES 500
#define OBJINFO_NAMELEN     50

// 0x10028d47 / 0x00466b20 — which row of the name table holds this class
// name, if any.  A file-static helper: nothing else in either image calls it.
//
// The /O1 image CALLS strcmp (0x1003e1f0) and the /O2 image EXPANDS it --
// the `sbb %eax,%eax ; sbb $0xffffffff,%eax` pair at 0x00466b5c is VC6's
// inline strcmp returning the sign, which is the intrinsic and not a
// hand-written compare.  So the source says `strcmp(...) == 0`.
//
// The table row is 50 bytes: `add $0x32,%edi` in both images.
//
// The answer goes out through a pointer and the return value is a flag --
// `mov %esi,(%eax) ; mov $0x1,%eax` at 0x00466b7a -- not a -1/index return.
static int oi_FindClass(char* pszName, char (*pNames)[OBJINFO_NAMELEN],
                            int nCount, int* pIndex)
{
	int i;

	for (i = 0; i < nCount; i++)
	{
		if (strcmp(pszName, pNames[i]) == 0)
		{
			*pIndex = i;
			return 1;
		}
	}

	return 0;
}

// 0x10028be3 (355 bytes, /O1) / 0x004669a0 (380 bytes, /O2).
//
// Three parallel 500-entry dword tables and a 500x50 name table -- 0x7924
// bytes of frame, which is why the client body opens with __chkstk and the
// server one with 0x1003e740.
//
// THE THREE CLEARS ARE THREE STATEMENTS AND THEIR ORDER IS THE SOURCE'S.
// server.dll makes three `memset(p, 0, 0x7d0)` calls in the order
// counts / active / bytes (0x10028c06, 0x10028c14, 0x10028c22) and
// lithtech.exe emits three `rep stosl` of 0x1f4 dwords in the same order
// (0x004669be, 0x004669cc, 0x004669d7).  The name table is NOT cleared.
// Which table lands where on the stack differs between the images and is
// allocation; the order of the three statements does not.
//
// The count guard is `>= 500` and it is checked ONLY on the miss path
// (0x00466a2a / 0x10028c7a): a class already in the table is still tallied
// after the table is full, and a 501st distinct class is dropped whole --
// the `jge` skips the tally as well as the insert.
//
// The active test is `!(pObject->m_ChangeFlags & 0x38)` -- one byte-wide
// `test $0x38` on LTObject+0x120 in both images.
//
// The per-class line's byte total is accumulated BEFORE the print, not
// after: `add %eax,%ebx` sits at 0x00466ad0 with the loads and ahead of
// every push, and at 0x10028d07 between them; neither image has it after
// the call, which a print-then-accumulate source would force.
//
// BYTE-IDENTICAL IN BOTH IMAGES once three more source facts were measured,
// and each one was worth exactly the bytes it looks worth:
//
//  * THE LIST HEAD IS A NAMED LOCAL. Written straight off the global in both
//    the init and the test (`pCur != &g_pServerMgr->m_List8dc.m_Head`) VC6
//    reloads 0x10058bf0 and recomputes the `lea 0x8e0` on every iteration --
//    it does not CSE a global across the calls in the body -- and the back
//    edge degenerates into `jmp` to a shared top test. With pHead in a local
//    the head sits in a callee-saved register for the whole loop, exactly
//    the shipped ebx. This also moved the three 500-dword arrays onto the
//    shipped frame slots (-0x7e0 counts / -0xfb0 bytes / -0x1780 actives,
//    names at -0x7928) and the frame from 0x7924 to the shipped 0x7928.
//
//  * THERE IS NO pszNext CURSOR: the source indexes szNames[nClasses] and
//    VC6 strength-reduces it. The derived pointer's initialisation is what
//    the shipped code puts in the loop's PREHEADER (`lea -0x7928(%ebp),%eax
//    ; mov %eax,-0x8(%ebp)` at 0x10028c46, below the empty-list `je`), and
//    a preheader is what forces the loop to rotate and carry its own bottom
//    test at 0x10028ce0. Written with an explicit cursor the store lands
//    above the test, the test is shared, and the function is 353 of 356 --
//    the three bytes being `jmp` rel32 against `cmp`+`jne`. The second loop
//    already showed the pattern: its szNames[i] is strength-reduced into edi
//    with the `lea` in its own preheader at 0x10028cf1. Rewriting loop one
//    as a `while` and as a guarded `do`/`while` changes NOTHING -- VC6
//    normalises all three forms to the same blocks; only the preheader does.
//
//  * `nClasses = 0` COMES BEFORE THE THREE memsets. Last three bytes: with
//    it after them both zero stores land together after the third call;
//    ahead of them the store schedules into the first memset's argument
//    run, which is the shipped 0x10028c03. (`nActive = 0` stays after.)
void cc_ObjectInfo(int argc, char** argv)
{
	uint32 nCounts[OBJINFO_MAXCLASSES];
	uint32 nActives[OBJINFO_MAXCLASSES];
	uint32 nBytes[OBJINFO_MAXCLASSES];
	char   szNames[OBJINFO_MAXCLASSES][OBJINFO_NAMELEN];

	LTLink* pHead;
	LTLink* pCur;
	int     nClasses;
	int     nActive;
	int     nTotalBytes;
	int     nIndex;
	int     i;

	nClasses = 0;

	memset(nCounts, 0, sizeof(nCounts));
	memset(nActives, 0, sizeof(nActives));
	memset(nBytes, 0, sizeof(nBytes));

	nActive = 0;

	pHead = &g_pServerMgr->m_List8dc.m_Head;

	for (pCur = pHead->m_pNext; pCur != pHead; pCur = pCur->m_pNext)
	{
		LTObject* pObject = (LTObject*)pCur->m_pData;

		if (!oi_FindClass(
			((CConObjClassView*)pObject->m_pSData)->m_pClass->m_pClassName,
			szNames, nClasses, &nIndex))
		{
			if (nClasses >= OBJINFO_MAXCLASSES)
				continue;

			nIndex = nClasses;

			strncpy(szNames[nClasses],
				((CConObjClassView*)pObject->m_pSData)->m_pClass->m_pClassName,
				OBJINFO_NAMELEN - 1);

			nClasses++;
		}

		nCounts[nIndex]++;

		nBytes[nIndex] +=
			((CConObjClassView*)pObject->m_pSData)->m_pClass->m_nObjectSize;

		if (!(pObject->m_ChangeFlags & 0x38))
		{
			nActives[nIndex]++;
			nActive++;
		}
	}

	nTotalBytes = 0;

	for (i = 0; i < nClasses; i++)
	{
		nTotalBytes += nBytes[i];

		dsi_ServerError("%d (%d) instance(s) of '%s' (%d bytes)",
			nCounts[i], nActives[i], szNames[i], nBytes[i]);
	}

	dsi_ServerError("--- Total objects: %d (Active: %d) (Bytes: %d) ---",
		g_pServerMgr->m_List8dc.m_nElements, nActive, nTotalBytes);
}


// 0x10028d7e — "DisableWMPhysics": put every live object on box physics,
// which is what turns world-model collision off (lithtech twin 0x00466b90).
// FLAG_BOXPHYSICS is 1<<14; server.dll ORs the byte at LTObject+0x89 with
// 0x40 and lithtech.exe the whole dword at +0x88 with 0x4000 -- same source,
// /O1 narrowed it. Neither argument is read; the list is the manager's 0x8dc.
void cc_DisableWMPhysics(int argc, char** argv)
{
	LTLink* pHead = &g_pServerMgr->m_List8dc.m_Head;
	LTLink* pCur;

	for (pCur = pHead->m_pNext; pCur != pHead; pCur = pCur->m_pNext)
	{
		LTObject* pObject = (LTObject*)pCur->m_pData;

		pObject->m_Flags |= FLAG_BOXPHYSICS;
	}
}

// The create struct is the SDK's own ObjectCreateStruct (ltbasedefs.h), and
// its in-class Clear() is what the two images disagree about: lithtech.exe
// (/O2) EXPANDS both Clears into cc_SpawnObject (0x00466bdb..0x00466c6d for
// the constructor's, 0x00466ce2..0x00466d8a for the explicit one, every
// store at the SDK struct's offsets), while server.dll's /Os declines them
// and calls the out-of-line COMDAT (0x10001bd7). One spelling gives both.
// (The local view with an out-of-line Clear that stood here matched
// server.dll only.)

// The two CLTServer entries SpawnObject reaches. servermsg.h lays the whole
// table out; this is the local two-field view, because console.cpp does not
// include that header (names servermsg.h's).
class CConServerView
{
public:
	uint8  m_pad_00[0x24];
	HCLASS (*GetClass)(char* pName);                                  // 0x24
	uint8  m_pad_28[0x38 - 0x28];
	void*  (*CreateObjectProps)(HCLASS hClass,
	                            ObjectCreateStruct* pStruct,
	                            char* pszProps);                      // 0x38
};

// 0x10028da3 — "SpawnObject <class name> \"spawn args\"": make one object of a
// named class next to whoever is playing.
//
// The spawn point is the FIRST client's m_Position with 200 added to its Z --
// the client list's m_nElements is what the fourth guard tests, and the origin
// comes off m_Head.m_pNext->m_pData. (LTList declares m_nElements BEFORE
// m_Head, which is what makes +0x7e4 the count and +0x7ec the first link.)
//
// CServerMgr+0x928 is a CLTServer, and GetClass and CreateObjectProps are
// FUNCTION-POINTER MEMBERS of it rather than virtuals -- the shipped calls are
// `call *0x24(%ecx)` and `call *0x38(%eax)` straight through the object with
// one pushed argument and a cdecl cleanup, where a virtual would load a vtable
// and pass `this` in ecx. servermsg.h already lays that table out.
//
// The create struct is Cleared TWICE, once on construction and again after
// the class lookup succeeds. Both calls are in the shipped code; the second
// is redundant and is what the source says.
//
// The usage line is shared by all four guards -- no arguments, no manager, no
// factory, no clients -- so a console `SpawnObject` with nobody connected
// prints the usage rather than an error about the client list.
void cc_SpawnObject(int argc, char** argv)
{
	ObjectCreateStruct  ocs;
	Client*             pClient;
	char*               pszProps;
	HCLASS              hClass;

	if (argc == 0 || !g_pServerMgr ||
	    !g_pServerMgr->m_pUnknown928 ||
	    !g_pServerMgr->m_ClientList.m_nElements)
	{
		dsi_ServerError("SpawnObject <class name> \"spawn args\"");
		return;
	}

	if (argc >= 2)
		pszProps = argv[1];
	else
		pszProps = "";

	hClass = ((CConServerView*)g_pServerMgr->m_pUnknown928)->GetClass(argv[0]);

	if (!hClass)
	{
		dsi_ServerError("Can't find class %s", argv[0]);
		return;
	}

	ocs.Clear();

	pClient = (Client*)g_pServerMgr->m_ClientList.m_Head.m_pNext->m_pData;

	ocs.m_Pos = pClient->m_Position;
	ocs.m_Pos.z += 200.0f;

	if (((CConServerView*)g_pServerMgr->m_pUnknown928)->CreateObjectProps(
		hClass, &ocs, pszProps))
		dsi_ServerError("%s spawned successfully", argv[0]);
	else
		dsi_ServerError("Error in CreateObjectProps");
}


// "ExhaustMemory" -- a debug command the release build compiled to nothing:
// its table slot points at a bare `ret` (server.dll 0x10036d21, which other
// empty functions share; lithtech.exe's twin slot is the image's folded empty
// function 0x00490200). Written empty because the shipped body is (name ours).
void cc_ExhaustMemory(int argc, char** argv)
{
}


// 0x10053d80 -- the eight commands the console starts with: {name, handler,
// user} triples in this order, every user word zero (lithtech.exe 0x004ad0a8
// holds the same table). cc_InitState hands it to the console with a count of
// 8 (name ours).
ConsoleCommandDef g_ConsoleCommands[8] =
{
	{ "Portal",           (void*)cc_Portal,           LTNULL },
	{ "ShowGameVars",     (void*)cc_ShowGameVars,     LTNULL },
	{ "ShowUsedFiles",    (void*)cc_ShowUsedFiles,    LTNULL },
	{ "world",            (void*)cc_World,            LTNULL },
	{ "objectinfo",       (void*)cc_ObjectInfo,       LTNULL },
	{ "DisableWMPhysics", (void*)cc_DisableWMPhysics, LTNULL },
	{ "ExhaustMemory",    (void*)cc_ExhaustMemory,    LTNULL },
	{ "SpawnObject",      (void*)cc_SpawnObject,      LTNULL }
};
