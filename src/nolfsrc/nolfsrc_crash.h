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
    /* the .pdbs sit beside each binary; the game DLLs' (which the engine
       copies out of nolfsrc.rez to load) in the game directory itself */
    pSymInitialize(process, (LPSTR)".", TRUE);

    memset(&frame, 0, sizeof(frame));
    frame.AddrPC.Offset = ctx.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Esp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (i = 0; i < 64; i++) {
        char symbuf[sizeof(IMAGEHLP_SYMBOL) + 256];
        PIMAGEHLP_SYMBOL sym = (PIMAGEHLP_SYMBOL)symbuf;
        IMAGEHLP_LINE line;
        char module[MAX_PATH], *modname;
        DWORD addr, base, disp = 0, linedisp = 0;

        if (!pStackWalk(IMAGE_FILE_MACHINE_I386, process, thread, &frame, &ctx,
                        NULL, pTableAccess, pModuleBase, NULL))
            break;
        addr = frame.AddrPC.Offset;
        if (!addr)
            break;

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
                NOLFSRC_CRASH_LOG(("  #%-2d 0x%08lx %s!%s+0x%lx  [%s:%lu]", i, addr, modname,
                                   sym->Name, disp, file, line.LineNumber));
            } else
                NOLFSRC_CRASH_LOG(("  #%-2d 0x%08lx %s!%s+0x%lx", i, addr, modname, sym->Name, disp));
        } else if (base) {
            /* no symbols (the CRT, or a d3d.ren file built with /Qip, for
               which icl 4.0 emits no debug information): give the address
               at the image's preferred base too, which is what its .map
               file (beside it in out/nolf-legacy-debug/) lists */
            IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
            IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
            NOLFSRC_CRASH_LOG(("  #%-2d 0x%08lx %s+0x%lx  (0x%08lx in its .map)", i, addr, modname,
                               addr - base, nt->OptionalHeader.ImageBase + (addr - base)));
        } else {
            NOLFSRC_CRASH_LOG(("  #%-2d 0x%08lx ?", i, addr));
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
