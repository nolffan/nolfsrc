// Modern-only normal NOLF.exe entry point into the managed installation.
// No mod-provided paths or commands; absence of the bundled component leaves
// the original launcher untouched. Keep this outside the retail reconstruction.
#ifndef NOLF_MOD_STARTUP_H
#define NOLF_MOD_STARTUP_H
#include "../mod_helper_launch.h"
static BOOL NolfModStartup()
{
    char root[MAX_PATH],helper[MAX_PATH];
    DWORD length=GetModuleFileName(NULL,root,sizeof(root));
    if(!length || length>=sizeof(root)) return FALSE;
    char* slash=strrchr(root,'\\');if(!slash) return FALSE;*slash=0;
    if(strlen(root)>MAX_PATH-40) return FALSE;
    sprintf(helper,"%s\\ModManager\\NolfModHelper.exe",root);
    if(GetFileAttributes(helper)==0xffffffff) return FALSE;
    if(strchr(root,'"')) return FALSE;
    // Capability marker for trusted release installers; never an interpreter path.
    OutputDebugString("NOLFSRC_MOD_MANAGER_BOOTSTRAP_V2");
    if(!NolfLaunchModHelper(helper,root,0,FALSE))
        MessageBox(NULL,"NOLF could not start its bundled mod component. Restore the complete ModManager folder from this release.","NOLF Mods",MB_OK|MB_ICONERROR);
    return TRUE;
}
#endif
