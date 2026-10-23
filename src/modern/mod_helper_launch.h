// Trusted modern launcher/menu bridge. No shell, PATH search or archive inputs.
#ifndef NOLF_MOD_HELPER_LAUNCH_H
#define NOLF_MOD_HELPER_LAUNCH_H
static BOOL NolfLaunchModHelper(const char* helper,const char* root,DWORD restartPid,BOOL gui)
{
    if(!helper[0] || !root[0] || strlen(helper)>=MAX_PATH || strlen(root)>=MAX_PATH ||
        strchr(helper,'"') || strchr(root,'"')) return FALSE;
    char cmd[MAX_PATH*2+80];
    if(restartPid) sprintf(cmd,"\"%s\" --root \"%s\" --restart-after %lu --launch",helper,root,restartPid);
    else sprintf(cmd,"\"%s\" --root \"%s\" %s",helper,root,gui?"--gui":"--launch");
    STARTUPINFO si;PROCESS_INFORMATION pi;memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));si.cb=sizeof(si);
    si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=gui?SW_SHOWNORMAL:SW_HIDE;
    if(!CreateProcess(helper,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,root,&si,&pi)) return FALSE;
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return TRUE;
}
#endif
