#ifndef NOLF_MOD_MANAGER_H
#define NOLF_MOD_MANAGER_H
#include "BaseFolder.h"
// The pinned VC6 SDK hides this message at the reconstruction's old WINVER.
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif
enum { MOD_HELP_MAIN=60000, MOD_HELP_SAVE, MOD_HELP_RESTART, MOD_HELP_ZIP, MOD_HELP_PROFILE, MOD_HELP_REVIEW, MOD_HELP_NOTICES, MOD_HELP_FIRST=60100 };
LTBOOL ModMenuMouseWheel(int delta);
void ModManagerProbeUpdate();
class CFolderModManager : public CBaseFolder
{
public:
    CFolderModManager();
    LTBOOL Build();
    void OnFocus(LTBOOL focus);
    uint32 OnCommand(uint32 command,uint32 p1,uint32 p2);
    HSTRING GetHelpString(uint32 helpId,int controlIndex);
    LTBOOL ScrollWheel(int delta);
    void ConfirmRestart(LTBOOL accepted);
    void ConfirmOverride(LTBOOL accepted);
    void NextNotice();
    void QueueNotice() { m_nextDialog=2; }
    void DismissInfo() { m_confirming=LTFALSE; }
    LTBOOL Render(HSURFACE surface);
    LTBOOL HandleKeyDown(int key,int rep);
    LTBOOL OnLeft();
    LTBOOL OnRight();
    LTBOOL OnEnter();
    LTBOOL OnLButtonUp(int x,int y);
    LTBOOL OnRButtonUp(int x,int y);
private:
    enum { MAX_MODS=64 };
    int m_count;
    char m_ini[MAX_PATH],m_request[MAX_PATH];
    char m_ids[MAX_MODS][64],m_requires[MAX_MODS][512];
    LTBOOL m_enabled[MAX_MODS],m_previous[MAX_MODS];
    LTBOOL m_running[MAX_MODS];
    int m_profile,m_previousProfile,m_profileCount;
    int m_presetSource;
    int m_review,m_overrideCount,m_notice;
    int m_nextDialog,m_detail;
    LTBOOL m_approved[128];
    char m_profileIds[MAX_MODS+1][64];
    CLTGUITextItemCtrl* m_status;
    int m_wheel;
    LTBOOL m_confirming;
    LTBOOL SaveChoices();
    void Status(char* text);
    void Dependencies(int index,LTBOOL on,int depth);
    LTBOOL LaunchHelper(LTBOOL restart);
    void SelectionChanged();
    void PendingStatus();
    void ProfilePath(int index,char* path);
    void ReviewNext();
    LTBOOL IsEnabled(const char* id);
    LTBOOL PairApproved(int index,const char* other);
};
#endif
