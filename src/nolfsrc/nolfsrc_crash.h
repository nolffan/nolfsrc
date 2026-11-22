/*
 * nolfsrc_crash.h -- the debug build's crash handler: on an unhandled
 * exception, write a backtrace (module, function, source file and line) to
 * nolfsrc.log, then let the crash carry on as it would have.
 *
 * Include it in ONE file, which already includes <windows.h>, and install
 * the handler once at startup:
 *
 *   #include "nolfsrc_crash.h"
 *   ...
 *   NOLFSRC_INSTALL_CRASH_HANDLER();
 *
 * lithtech.exe does this in WinMain (src/engine/lithtech/clientwin.cpp); the
 * handler covers the whole process, so server code, d3d.ren and the game
 * DLLs too. Release builds get nothing: without NOLFSRC_DEBUG the macro is
 * empty and this header includes nothing.
 *
 * The names and lines come from the .pdb files build/variant.sh puts beside
 * each binary, read through dbghelp.dll (Wine has its own), which is loaded
 * only when a crash happens, so nothing is linked against it. Wine's own
 * crash backtrace (winedbg) shows the same, but only when it can open its
 * dialog or a console; this works under the playtest too.
 */
#ifndef NOLFSRC_CRASH_H
#define NOLFSRC_CRASH_H

#ifdef NOLFSRC_DEBUG

#include <imagehlp.h>
#include "nolfsrc_debug.h"

/* crash lines are tagged "crash", not with this header's position */
#define NOLFSRC_CRASH_LOG(args) (NolfsrcLogAt("crash", 0), NolfsrcLogf args)

typedef DWORD (WINAPI *NolfsrcSymSetOptions_t)(DWORD);
typedef BOOL (WINAPI *NolfsrcSymInitialize_t)(HANDLE, LPSTR, BOOL);
typedef BOOL (WINAPI *NolfsrcStackWalk_t)(DWORD, HANDLE, HANDLE, LPSTACKFRAME, LPVOID,
    PREAD_PROCESS_MEMORY_ROUTINE, PFUNCTION_TABLE_ACCESS_ROUTINE,
    PGET_MODULE_BASE_ROUTINE, PTRANSLATE_ADDRESS_ROUTINE);
typedef BOOL (WINAPI *NolfsrcSymGetSymFromAddr_t)(HANDLE, DWORD, LPDWORD, PIMAGEHLP_SYMBOL);
typedef BOOL (WINAPI *NolfsrcSymGetLineFromAddr_t)(HANDLE, DWORD, LPDWORD, PIMAGEHLP_LINE);

/* one code address as a line of the log: module, function, source line
   when the .pdb has them, else the module offset, else nothing */
static void NolfsrcCrashAddr(HANDLE process, const char *tag, DWORD addr,
                             PGET_MODULE_BASE_ROUTINE pModuleBase,
                             NolfsrcSymGetSymFromAddr_t pSymFromAddr,
                             NolfsrcSymGetLineFromAddr_t pLineFromAddr)
{
    char symbuf[sizeof(IMAGEHLP_SYMBOL) + 256];
    PIMAGEHLP_SYMBOL sym = (PIMAGEHLP_SYMBOL)symbuf;
    IMAGEHLP_LINE line;
    char module[MAX_PATH], *modname;
    DWORD base, disp = 0, linedisp = 0;

    modname = (char *)"?";
    base = pModuleBase(process, addr);
    if (base && GetModuleFileNameA((HMODULE)base, module, sizeof(module))) {
        modname = strrchr(module, '\\');
        modname = modname ? modname + 1 : module;
    }
    memset(symbuf, 0, sizeof(symbuf));
    sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
    sym->MaxNameLength = 256;
    memset(&line, 0, sizeof(line));
    line.SizeOfStruct = sizeof(line);

    if (pSymFromAddr(process, addr, &disp, sym)) {
        if (pLineFromAddr && pLineFromAddr(process, addr, &linedisp, &line)) {
            /* the compilers record bare file names, which dbghelp
               prefixes with the current directory: keep the name */
            const char *file = strrchr(line.FileName, '\\');
            file = file ? file + 1 : line.FileName;
            NOLFSRC_CRASH_LOG(("  %s 0x%08lx %s!%s+0x%lx  [%s:%lu]", tag, addr, modname,
                               sym->Name, disp, file, line.LineNumber));
        } else
            NOLFSRC_CRASH_LOG(("  %s 0x%08lx %s!%s+0x%lx", tag, addr, modname, sym->Name, disp));
    } else if (base) {
        /* no symbols (the CRT, or a d3d.ren file built with /Qip, for
           which icl 4.0 emits no debug information): give the address
           at the image's preferred base too, which is what its .map
           file (beside it in out/nolf-legacy-debug/) lists */
        IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
        NOLFSRC_CRASH_LOG(("  %s 0x%08lx %s+0x%lx  (0x%08lx in its .map)", tag, addr, modname,
                           addr - base, nt->OptionalHeader.ImageBase + (addr - base)));
    } else {
        NOLFSRC_CRASH_LOG(("  %s 0x%08lx ?", tag, addr));
    }
}

static LONG WINAPI NolfsrcCrashFilter(EXCEPTION_POINTERS *ep)
{
    HMODULE dbghelp;
    NolfsrcSymSetOptions_t pSymSetOptions;
    NolfsrcSymInitialize_t pSymInitialize;
    NolfsrcStackWalk_t pStackWalk;
    PFUNCTION_TABLE_ACCESS_ROUTINE pTableAccess;
    PGET_MODULE_BASE_ROUTINE pModuleBase;
    NolfsrcSymGetSymFromAddr_t pSymFromAddr;
    NolfsrcSymGetLineFromAddr_t pLineFromAddr;
    HANDLE process = GetCurrentProcess(), thread = GetCurrentThread();
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME frame;
    int i;

    NOLFSRC_CRASH_LOG(("CRASH: exception 0x%08lx at 0x%08lx",
                 ep->ExceptionRecord->ExceptionCode,
                 (unsigned long)ep->ExceptionRecord->ExceptionAddress));
    /* an access violation says what it touched: 0 read, 1 write, 8 run */
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ep->ExceptionRecord->NumberParameters >= 2)
        NOLFSRC_CRASH_LOG(("CRASH: %s of 0x%08lx",
                     ep->ExceptionRecord->ExceptionInformation[0] == 8 ? "execute" :
                     ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "read",
                     (unsigned long)ep->ExceptionRecord->ExceptionInformation[1]));
    /* which thread (the game has more than one: the loading screen draws
       from its own), and the registers: after a call through a bad
       pointer, ecx is often the object and esp holds the return address */
    NOLFSRC_CRASH_LOG(("CRASH: thread %lu, eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx",
                 GetCurrentThreadId(), ctx.Eax, ctx.Ebx, ctx.Ecx, ctx.Edx,
                 ctx.Esi, ctx.Edi, ctx.Ebp, ctx.Esp));

    dbghelp = LoadLibraryA("dbghelp.dll");
    if (!dbghelp)
        dbghelp = LoadLibraryA("imagehlp.dll");
    if (!dbghelp) {
        NOLFSRC_CRASH_LOG(("CRASH: no dbghelp.dll, no backtrace"));
        return EXCEPTION_CONTINUE_SEARCH;
    }
    pSymSetOptions = (NolfsrcSymSetOptions_t)GetProcAddress(dbghelp, "SymSetOptions");
    pSymInitialize = (NolfsrcSymInitialize_t)GetProcAddress(dbghelp, "SymInitialize");
    pStackWalk = (NolfsrcStackWalk_t)GetProcAddress(dbghelp, "StackWalk");
    pTableAccess = (PFUNCTION_TABLE_ACCESS_ROUTINE)GetProcAddress(dbghelp, "SymFunctionTableAccess");
    pModuleBase = (PGET_MODULE_BASE_ROUTINE)GetProcAddress(dbghelp, "SymGetModuleBase");
    pSymFromAddr = (NolfsrcSymGetSymFromAddr_t)GetProcAddress(dbghelp, "SymGetSymFromAddr");
    pLineFromAddr = (NolfsrcSymGetLineFromAddr_t)GetProcAddress(dbghelp, "SymGetLineFromAddr");
    if (!pSymInitialize || !pStackWalk || !pTableAccess || !pModuleBase || !pSymFromAddr) {
        NOLFSRC_CRASH_LOG(("CRASH: dbghelp.dll lacks the stack-walk functions, no backtrace"));
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (pSymSetOptions)
        pSymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    /* the .pdbs sit beside each binary; the game DLLs are loaded out of the
       custom/ rez directory, so look there as well (the modern build's,
       out of nolfsrc.rez, have theirs in the game directory) */
    pSymInitialize(process, (LPSTR)".;custom", TRUE);

    memset(&frame, 0, sizeof(frame));
    frame.AddrPC.Offset = ctx.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Esp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (i = 0; i < 64; i++) {
        char tag[8];
        DWORD addr;

        if (!pStackWalk(IMAGE_FILE_MACHINE_I386, process, thread, &frame, &ctx,
                        NULL, pTableAccess, pModuleBase, NULL))
            break;
        addr = frame.AddrPC.Offset;
        if (!addr)
            break;
        sprintf(tag, "#%-2d", i);
        NolfsrcCrashAddr(process, tag, addr, pModuleBase, pSymFromAddr, pLineFromAddr);
    }

    /* A crash at an address in no module (a call through a stale or
       broken function pointer or vtable) gives the walk nothing to start
       from. The caller's return address is still on the stack, at or near
       esp: list the stack words that point into a module, the first of
       them most likely the caller. */
    if (!pModuleBase(process, ep->ContextRecord->Eip)) {
        DWORD *sp = (DWORD *)ep->ContextRecord->Esp;
        int found = 0;

        NOLFSRC_CRASH_LOG(("CRASH: 0x%08lx is in no module; stack words that point into a module (the first is likely the caller):",
                     (unsigned long)ep->ContextRecord->Eip));
        for (i = 0; i < 256 && found < 16; i++) {
            char tag[16];
            DWORD word;

            if (IsBadReadPtr(sp + i, sizeof(DWORD)))
                break;
            word = sp[i];
            if (!word || !pModuleBase(process, word))
                continue;
            sprintf(tag, "[esp+0x%03x]", i * 4);
            NolfsrcCrashAddr(process, tag, word, pModuleBase, pSymFromAddr, pLineFromAddr);
            found++;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;   /* the crash goes on as before */
}

#define NOLFSRC_INSTALL_CRASH_HANDLER() \
    ((void)SetUnhandledExceptionFilter(NolfsrcCrashFilter))

#else /* release: nothing */

#define NOLFSRC_INSTALL_CRASH_HANDLER() ((void)0)

#endif /* NOLFSRC_DEBUG */

#endif /* NOLFSRC_CRASH_H */
