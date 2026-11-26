// *********************************************************************** //
//
//  MODULE:   DE_ERROR (.H)
//
//  PURPOSE:  Reconstructed engine-internal error-reporting macro used by
//            server.dll's interface methods. NOT part of the public SDK —
//            recovered from the shared parameter-check idiom in the binary:
//              dsi_OnReturnError(code);                     // empty release hook
//              if (g_DebugLevel >= level)                   // 0x10056ff0
//                  dsi_ServerError(fmt, fn, "60", " ");     // 0x1000e3f9
//              return code;
//
//  Three things about that idiom are call-site arguments, not constants:
//
//    * the debug level. Sites compare g_DebugLevel against 1, 2, 3 and 5
//      (and, in three places, 0), so the threshold is a macro parameter.
//      146 sites use 1 and 143 use 2, which is why a hard-coded 2 got the
//      first block of methods right and would have silently broken the next.
//    * the return code. LT_INVALIDPARAMS (60) dominates, but LT_NOTINITIALIZED
//      (73) and others appear.
//    * the function name, which the caller passes as a string literal. Those
//      literals are the only naming information left in the stripped image;
//      a function enumeration recovers them.
//
//  The third argument to the printer is the *numeric* code as a string: the
//  shipped data has "60" and "73", not "LT_INVALIDPARAMS". That is what
//  stringizing a macro through one extra level of expansion produces, since
//  ltcodes.h defines the codes as plain integer literals.
//
// *********************************************************************** //

#ifndef __DE_ERROR_H__
#define __DE_ERROR_H__

#include "ltcodes.h"

// Empty in release (0x10036d21 is a bare `ret`); a breakpoint hook in debug.
extern "C" void dsi_OnReturnError(int code);

// Debug verbosity level (0x10056ff0); error text is only printed at >= level.
extern int g_DebugLevel;

// The shared format string pointer (0x100510c0 -> "LT ERROR: %s returned %s (%s)").
extern char* g_pErrorFormat;

// The console/error printer (0x1000e3f9).
void dsi_ServerError(char* pFormat, ...);

// Expand the code macro first, then stringize the number it expands to: this
// is the pair that turns LT_INVALIDPARAMS into "60". Where a form wants
// "LT_INVALIDPARAMS" instead, it applies `#` to its own parameter directly,
// which stringizes the argument as written. Getting this backwards is
// invisible to a masked byte comparison — the operand is a relocation, so it is masked — but
// it puts the wrong string in .data and would surface at link time.
#define LT_ERRSTR_(x) #x
#define LT_ERRSTR(x) LT_ERRSTR_(x)

// The trailing description argument is a lone space in every shipped call.
#define LT_ERRDESC " "

// 0x004b0ab4 in lithtech.exe: the engine's message buffer, empty unless a
// shutdown message is pending - ci_Shutdown hands this same address to
// ci_ShutdownWithMessage. The ILTCSBase compatibility wrappers pass it as
// their error description, so those descriptions are blank for the same
// reason the shutdown message usually is.
extern char g_ShutdownMessage[];

// Core: report a failed check. `level` is the g_DebugLevel threshold,
// `fnName` the string literal naming the call, `code` the LTRESULT.
#define REPORT_ERROR(level, fnName, code) \
	dsi_OnReturnError(code); \
	if (g_DebugLevel >= level) \
		dsi_ServerError(g_pErrorFormat, fnName, LT_ERRSTR(code), LT_ERRDESC);

// Returns the code (the LTRESULT-returning interface methods).
#define RETURN_ERROR(level, fnName, code) \
	{ \
		REPORT_ERROR(level, fnName, code) \
		return code; \
	}

// Returns a typed zero (the value-returning readers).
#define RETURN_ERROR_VALUE_L(level, fnName, code, val) \
	{ \
		REPORT_ERROR(level, fnName, code) \
		return val; \
	}

// Returns nothing (the void readers / End / Reset).
#define RETURN_ERROR_VOID_L(level, fnName, code) \
	{ \
		REPORT_ERROR(level, fnName, code) \
		return; \
	}

// Two things vary independently across the check sites, and conflating them
// wastes an afternoon:
//
//   * How the code is reported. "60" comes from expanding LT_INVALIDPARAMS
//     before stringizing it (RETURN_ERROR); "LT_INVALIDPARAMS" comes from
//     applying `#` to the macro's own parameter (RETURN_ERROR_SYM). That is a
//     property of *which macro* the site uses.
//   * How the name is passed. A string literal assembles as `push imm32`; a
//     `char*` variable holding it assembles as `push [imm32]`. That is a
//     property of *what the caller passes*, not of the macro — which is why
//     all four combinations occur, sometimes two of them in one function.
//
// ILTModel::AddTracker is the case that proves the split: its two sites push
// the same name pointer but different code strings, so one name variable
// serves both and the macros differ.
// The same, with the description the printer's last argument carries. Most
// sites pass a lone space; the file loaders pass the name of the file they
// could not find, which is the only place the description says anything.
#define RETURN_ERROR_SYM_DESC(level, fnName, code, desc) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, #code, desc); \
		return code; \
	}

#define RETURN_ERROR_SYM(level, fnName, code) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, #code, LT_ERRDESC); \
		return code; \
	}

// Convenience for the common case: one check site, so the variable holding the
// name can live in the macro's own braces. A function with two sites shares one
// variable — the shipped code pushes the same pointer at both — so those
// declare it themselves and call RETURN_ERROR_SYM / RETURN_ERROR directly.
// Do not write this macro through RETURN_ERROR_SYM: the preprocessor expands
// `code` before RETURN_ERROR_SYM can apply `#` to it. Then the code string is
// "60", and the shipped code string is "LT_INVALIDPARAMS" (the literal check
// of ext/tools 3d9e068 and later shows this). The body is duplicated on
// purpose.
#define RETURN_ERROR_NAMED(level, fnName, code) \
	{ \
		static char* pErrFnName = fnName; \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, pErrFnName, #code, LT_ERRDESC); \
		return code; \
	}

// The same, but the code string is the number ("60", "61", "73"). The
// shipped level-1 sites use this form (LT_NOTINITIALIZED, LT_NOTFOUND, and
// the LT_INVALIDPARAMS check of SharedCommonLT::GetAttachmentTransform),
// while the level-2 LT_INVALIDPARAMS sites use the symbolic form above: the
// shipped server.dll has no "LT_NOTINITIALIZED" string at all. RETURN_ERROR_SYM gets `code` expanded, so its `#` gives the
// number.
#define RETURN_ERROR_NAMED_NUM(level, fnName, code) \
	{ \
		static char* pErrFnName = fnName; \
		RETURN_ERROR_SYM(level, pErrFnName, code) \
	}

// The two forms the ILTCSBase compatibility wrappers use. One returning
// LTRESULT uses RETURN_ERROR above; one returning void or a value cannot,
// and those report the code by name and pass the message buffer as the
// description rather than the lone space. Which form a wrapper uses follows
// from its return type alone, across all forty-four of them.
#ifdef NOLFSRC_MODERN
// fixed: the value-returning readers quote the shutdown message
// (RETAIL_BUGS.md). They pass the description every other check passes.
#define RETURN_ERROR_MSG_VOID(level, fnName, code) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, #code, LT_ERRDESC); \
		return; \
	}

#define RETURN_ERROR_MSG_VALUE(level, fnName, code, val) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, #code, LT_ERRDESC); \
		return val; \
	}
#else
#define RETURN_ERROR_MSG_VOID(level, fnName, code) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, #code, g_ShutdownMessage); \
		return; \
	}

#define RETURN_ERROR_MSG_VALUE(level, fnName, code, val) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, #code, g_ShutdownMessage); \
		return val; \
	}
#endif

// A third form takes the description as an argument instead of leaving it a
// blank, and reports the code symbolically like the pointer-name form does.
// GetModelAnimUserDims uses this one and the plain RETURN_ERROR above in the
// same function, so the two really are separate macros and not one with
// defaults — the shipped code even shares their common tail, pushing the name
// and the format once for both.
#define REPORT_ERROR_INFO(level, fnName, code, info) \
	dsi_OnReturnError(code); \
	if (g_DebugLevel >= level) \
		dsi_ServerError(g_pErrorFormat, fnName, #code, info);

// NOT defined via REPORT_ERROR_INFO: an argument passed through a second
// macro is expanded before that macro's # can stringize it, so the wrapper
// form reports "60" where the bytes say "LT_INVALIDPARAMS". The body is
// duplicated on purpose.
#define RETURN_ERROR_INFO(level, fnName, code, info) \
	{ \
		dsi_OnReturnError(code); \
		if (g_DebugLevel >= level) \
			dsi_ServerError(g_pErrorFormat, fnName, #code, info); \
		return code; \
	}

// The ILTCSBase message block checks at level 2 and always reports
// LT_INVALIDPARAMS; these are the shorthands it uses.
#define RETURN_ERROR_PARAM(fnName)       RETURN_ERROR(2, fnName, LT_INVALIDPARAMS)
#define RETURN_ERROR_VALUE(fnName, val)  RETURN_ERROR_VALUE_L(2, fnName, LT_INVALIDPARAMS, val)
#define RETURN_ERROR_VOID(fnName)        RETURN_ERROR_VOID_L(2, fnName, LT_INVALIDPARAMS)

#endif  // __DE_ERROR_H__
