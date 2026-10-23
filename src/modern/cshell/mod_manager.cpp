#include "stdafx.h"
#ifdef NOLFSRC_MODERN
#include "FolderMgr.h"
#include "FolderCommands.h"
#include "ClientRes.h"
#include "GameClientShell.h"
#include "InterfaceMgr.h"
#include "cshell/p3_ui.h"
#include "cshell/mod_manager.h"
#include "ClientUtilities.h"
#include <stdio.h>

namespace {
enum { SAVE_CHOICES=FOLDER_CMD_CUSTOM+901, OPEN_MANAGER, RESTART_GAME, REVIEW_OVERRIDES, PACKAGE_NOTICES };
CFolderModManager* activeMenu=LTNULL;
void FitLabel(char* text,CLTGUIFont* font,int width) {
    for(;;) {
        HSTRING value=g_pLTClient->CreateString(text);
        int measured=font->GetTextExtents(value).x;g_pLTClient->FreeString(value);
        int length=strlen(text);if(measured<=width || length<=4) return;
        text[length-4]=0;strcat(text,"...");
    }
}
void RestartCallback(LTBOOL accepted,void* data) {
    ((CFolderModManager*)data)->ConfirmRestart(accepted);
}
void OverrideCallback(LTBOOL accepted,void* data) { ((CFolderModManager*)data)->ConfirmOverride(accepted); }
void NoticeCallback(LTBOOL accepted,void* data) { ((CFolderModManager*)data)->QueueNotice(); }
void InfoCallback(LTBOOL accepted,void* data) { ((CFolderModManager*)data)->DismissInfo(); }
}

CFolderModManager::CFolderModManager()
{
    m_count=0; m_status=LTNULL; m_ini[0]=m_request[0]=0; m_wheel=0; m_confirming=LTFALSE;
    m_profile=m_previousProfile=m_profileCount=0; m_presetSource=-1;
    m_review=m_overrideCount=m_notice=0;memset(m_approved,0,sizeof(m_approved));
    m_nextDialog=0;m_detail=-1;
    memset(m_running,0,sizeof(m_running)); memset(m_profileIds,0,sizeof(m_profileIds));
    memset(m_enabled,0,sizeof(m_enabled)); memset(m_previous,0,sizeof(m_previous));
    memset(m_ids,0,sizeof(m_ids)); memset(m_requires,0,sizeof(m_requires));
}

LTBOOL CFolderModManager::Build()
{
    CreateTitle("Mods");
    // The trusted installer creates this beside the managed game executable.
    GetModuleFileName(NULL,m_ini,sizeof(m_ini));
    char* slash=strrchr(m_ini,'\\');
    if(slash && slash-m_ini<(int)sizeof(m_ini)-16) strcpy(slash+1,"mods.ini");
    else m_ini[0]=0;
    GetPrivateProfileString("Manager","Request","",m_request,sizeof(m_request),m_ini);
    m_count=GetPrivateProfileInt("Manager","Count",0,m_ini);
    if(m_count<0 || m_count>MAX_MODS || !m_request[0]) m_count=0;
    // Use the same layout-selected fonts and inherited colors as Options.
    m_status=AddTextItem("Changes apply next launch.",0,0,LTFALSE,GetDefaultFont());
    m_status->Enable(LTFALSE);
    m_profileCount=GetPrivateProfileInt("Manager","ProfileCount",0,m_ini);
    if(m_profileCount<0 || m_profileCount>MAX_MODS) m_profileCount=0;
    if(m_profileCount) {
        CCycleCtrl* profiles=AddCycleItem("Profile",MOD_HELP_PROFILE,P3_MenuLen(60),P3_MenuLen(5),&m_profile);
        HSTRING label=g_pLTClient->CreateString("Custom");profiles->AddString(label);g_pLTClient->FreeString(label);
        for(int p=0;p<m_profileCount;++p) {
            char tag[24],name[64]; sprintf(tag,"Profile%d",p);
            GetPrivateProfileString(tag,"Id","",m_profileIds[p+1],sizeof(m_profileIds[p+1]),m_ini);
            GetPrivateProfileString(tag,"Name","Profile",name,sizeof(name),m_ini);
            FitLabel(name,GetDefaultFont(),P3_MenuLen(86));label=g_pLTClient->CreateString(name);profiles->AddString(label);g_pLTClient->FreeString(label);
        }
    }
    if(m_request[0]) {
        AddTextItem("Save choices",SAVE_CHOICES,MOD_HELP_SAVE);
        AddTextItem("Restart game",RESTART_GAME,MOD_HELP_RESTART);
        if(GetPrivateProfileInt("Manager","OverrideCount",0,m_ini))
            AddTextItem("Review overrides",REVIEW_OVERRIDES,MOD_HELP_REVIEW);
        if(GetPrivateProfileInt("Manager","NoticeCount",0,m_ini))
            AddTextItem("Package notices",PACKAGE_NOTICES,MOD_HELP_NOTICES);
        AddTextItem("Advanced mod manager",OPEN_MANAGER,MOD_HELP_ZIP);
    } else {
        AddTextItem("Start the managed NOLF.exe.",0,0)->Enable(LTFALSE);
    }
    int gap=134;
    if(g_pLayoutMgr->HasCustomValue(FOLDER_ID_MOD_MANAGER,"ColumnWidth"))
        gap=g_pLayoutMgr->GetFolderCustomInt(FOLDER_ID_MOD_MANAGER,"ColumnWidth");
    gap=P3_MenuLen(gap);
    char previousType[64]="";
    for(int i=0;i<m_count;++i) {
        char tag[24],name[96]; sprintf(tag,"Mod%d",i);
        char type[64];GetPrivateProfileString(tag,"Type","Built-in feature",type,sizeof(type),m_ini);
        if(strcmp(previousType,type)) {
            AddTextItem(!strcmp(type,"Imported data mod")?"Imported data mods":"Built-in features",0,0)->Enable(LTFALSE);
            strcpy(previousType,type);
        }
        GetPrivateProfileString(tag,"Id","",m_ids[i],sizeof(m_ids[i]),m_ini);
        GetPrivateProfileString(tag,"Name",m_ids[i],name,sizeof(name),m_ini);
        GetPrivateProfileString(tag,"Requires","",m_requires[i],sizeof(m_requires[i]),m_ini);
        FitLabel(name,GetDefaultFont(),gap-P3_MenuLen(8));
        CToggleCtrl* toggle=AddToggle(name,MOD_HELP_FIRST+i,gap,&m_enabled[i]);
        toggle->SetOnString(IDS_ON); toggle->SetOffString(IDS_OFF);
    }
    if(!CBaseFolder::Build()) return LTFALSE;
    UseBack(LTTRUE); return LTTRUE;
}

void CFolderModManager::Status(char* text)
{
    if(m_status) {
        char fitted[256];strncpy(fitted,text,sizeof(fitted)-1);fitted[sizeof(fitted)-1]=0;
        FitLabel(fitted,GetDefaultFont(),P3_MenuLen(158));
        HSTRING value=g_pLTClient->CreateString(fitted);
        m_status->RemoveAll(); m_status->AddString(value);
        g_pLTClient->FreeString(value);
    }
}

void CFolderModManager::OnFocus(LTBOOL focus)
{
    activeMenu=focus?this:LTNULL; m_wheel=0;m_nextDialog=0;m_confirming=LTFALSE;
    if(focus) {
        m_overrideCount=GetPrivateProfileInt("Manager","OverrideCount",0,m_ini);
        if(m_overrideCount<0 || m_overrideCount>128)m_overrideCount=0;
        for(int o=0;o<m_overrideCount;++o) {
            char tag[24],key[72];sprintf(tag,"Override%d",o);
            GetPrivateProfileString(tag,"Key","",key,sizeof(key),m_ini);
            m_approved[o]=GetPrivateProfileInt(tag,"Approved",0,m_ini) || GetPrivateProfileInt("NativeApprovals",key,0,m_request);
        }
        char selected[64];GetPrivateProfileString("Selection","Configuration","",selected,sizeof(selected),m_request);
        m_profile=0;m_presetSource=-1;
        for(int p=1;p<=m_profileCount;++p) if(!strcmp(selected,m_profileIds[p])) m_profile=p;
        m_previousProfile=m_profile;
        for(int i=0;i<m_count;++i) {
            char tag[24]; sprintf(tag,"Mod%d",i);
            int current=GetPrivateProfileInt(tag,"Enabled",0,m_ini);
            m_running[i]=current!=0;
            m_enabled[i]=GetPrivateProfileInt("Enabled",m_ids[i],current,m_request)!=0;
            m_previous[i]=m_enabled[i];
        }
        PendingStatus();
        UpdateData(LTFALSE);
    }
    CBaseFolder::OnFocus(focus);
}

HSTRING CFolderModManager::GetHelpString(uint32 id,int index)
{
    if(id==MOD_HELP_SAVE) return g_pLTClient->CreateString("Save your mod choices. Restart the game to apply them.");
    if(id==MOD_HELP_RESTART) return g_pLTClient->CreateString("Save choices and restart NOLF with the selected mods.");
    if(id==MOD_HELP_ZIP) return g_pLTClient->CreateString("Optional tools for profiles, import previews and complex conflicts. Simple mods: drop ZIP/REZ files in Mods, then launch NOLF.exe.");
    if(id==MOD_HELP_REVIEW) return g_pLTClient->CreateString("Review each declared resource replacement for the pending selection. Only approve overrides you want. Undeclared conflicts must be disabled.");
    if(id==MOD_HELP_NOTICES) return g_pLTClient->CreateString("Read rejected-package and failed-activation details. A rejected ZIP cannot replace your working resources.");
    if(id==MOD_HELP_PROFILE) return g_pLTClient->CreateString("Left/Right selects a saved profile. Restart to apply. Manage profiles in the companion.");
    if(id>=MOD_HELP_FIRST && id<MOD_HELP_FIRST+m_count) {
        int n=id-MOD_HELP_FIRST;
        m_detail=n;
        char tag[24],text[512],type[64],combined[1024],author[96],version[64],warning[192]; sprintf(tag,"Mod%d",n);
        GetPrivateProfileString(tag,"Description","Enable or disable this mod.",text,sizeof(text),m_ini);
        GetPrivateProfileString(tag,"Type","Built-in feature",type,sizeof(type),m_ini);
        GetPrivateProfileString(tag,"Author","",author,sizeof(author),m_ini);
        GetPrivateProfileString(tag,"Version","",version,sizeof(version),m_ini);
        GetPrivateProfileString(tag,"Warning","",warning,sizeof(warning),m_ini);
        sprintf(combined,"%s %s. Active %s / pending %s. F1: details. %s%s%s",type,version,m_running[n]?"On":"Off",m_enabled[n]?"On":"Off",warning,warning[0]?". ":"",text);
        // The original help panel has bounded height. Keep its stock layout,
        // but mark a shortened description instead of clipping mid-sentence.
        for(;;) {
            HSTRING value=g_pLTClient->CreateString(combined);
            LTIntPt size=GetHelpFont()->GetTextExtentsFormat(value,m_HelpRect.right-m_HelpRect.left);
            int length=strlen(combined);
            if(size.y<=m_HelpRect.bottom-m_HelpRect.top || length<=4) return value;
            g_pLTClient->FreeString(value);combined[length-4]=0;strcat(combined,"...");
        }
    }
    return CBaseFolder::GetHelpString(id,index);
}

LTBOOL CFolderModManager::ScrollWheel(int delta)
{
    if(m_confirming) return LTTRUE;
    m_wheel+=delta;
    for(int count=0;count<8 && (m_wheel>=120 || m_wheel<=-120);++count) {
        int direction=m_wheel>0?-1:1;
        m_wheel+=direction*120;
        CalculateLastDrawn();
        if((direction<0 && IsFirstPage()) || (direction>0 && IsLastPage())) continue;
        m_nFirstDrawn+=direction;
        CalculateLastDrawn();
        if(m_nSelection<m_nFirstDrawn || m_nSelection>m_nLastDrawn)
            SetSelection(direction>0?m_nFirstDrawn:m_nLastDrawn,LTTRUE);
        CheckArrows();
    }
    if(m_wheel>119) m_wheel=119; if(m_wheel< -119) m_wheel=-119;
    return LTTRUE;
}

LTBOOL ModMenuMouseWheel(int delta)
{
    if(!activeMenu || !g_pInterfaceMgr || g_pInterfaceMgr->GetGameState()!=GS_FOLDER) return LTFALSE;
    return activeMenu->ScrollWheel(delta);
}

void ModManagerProbeUpdate()
{
    // Explicit native integration probe. Normal launches force this off.
    if(GetConsoleInt("ModManagerProbe",0)!=1 || !g_pLTClient->GetClientObject()) return;
    static LTBOOL recorded=LTFALSE;static float start=0;
    g_pLTClient->SetInputState(LTFALSE);g_pLTClient->ClearInput();
    if(!recorded) {
        HSURFACE surface=g_pLTClient->CreateSurfaceFromBitmap("Interface\\ModManager.pcx");
        uint32 width=0,height=0;HLTCOLOR pixel=0;
        if(surface) {
            g_pLTClient->GetSurfaceDims(surface,&width,&height);
            g_pLTClient->GetPixel(surface,0,0,&pixel);g_pLTClient->DeleteSurface(surface);
        }
        FILE* file=fopen("ModManagerProbe.json","w");
        if(file) {
            fprintf(file,"{\"world_ready\":true,\"surface_loaded\":%s,\"width\":%lu,\"height\":%lu,\"pixel\":%lu}\n",surface?"true":"false",width,height,(uint32)pixel);
            fclose(file);
        }
        start=g_pLTClient->GetTime();recorded=LTTRUE;
    }
    if(g_pLTClient->GetTime()-start>3.f) g_pLTClient->Shutdown();
}

void CFolderModManager::Dependencies(int index,LTBOOL on,int depth)
{
    if(depth>=MAX_MODS) return;
    m_enabled[index]=on;
    // Turning a dependency off also disables its dependents. Turning a mod on
    // enables its requirements. The manager validates the complete graph again.
    for(int i=0;i<m_count;++i) {
        char list[512]; strcpy(list,on?m_requires[index]:m_requires[i]);
        char* p=list;
        while(p && *p) {
            char* next=strchr(p,','); if(next) *next++=0;
            const char* wanted=on?m_ids[i]:m_ids[index];
            if(!strcmp(p,wanted) && m_enabled[i]!=on) Dependencies(i,on,depth+1);
            p=next;
        }
    }
}

LTBOOL CFolderModManager::SaveChoices()
{
    if(!m_request[0]) return LTFALSE;
    UpdateData();
    LTBOOL requested[MAX_MODS]; memcpy(requested,m_enabled,sizeof(requested));
    int i;
    for(i=0;i<m_count;++i) if(m_previous[i] && !requested[i]) Dependencies(i,LTFALSE,0);
    for(i=0;i<m_count;++i) if(!m_previous[i] && requested[i]) Dependencies(i,LTTRUE,0);
    char temp[MAX_PATH+32]; sprintf(temp,"%s.%lu.tmp",m_request,GetCurrentProcessId());
    // Keep companion metadata and newly imported IDs when this menu's catalog
    // predates an import. A named preset supplies its pinned selection instead.
    char source[MAX_PATH];strcpy(source,m_request);
    if(m_presetSource>=0) ProfilePath(m_presetSource,source);
    if(!CopyFile(source,temp,FALSE)) {
        if(GetFileAttributes(source)!=0xffffffff) { Status("Could not save choices.");return LTFALSE; }
        FILE* f=fopen(temp,"w");if(!f) return LTFALSE;fclose(f);
    }
    LTBOOL written=LTTRUE;
    for(i=0;i<m_count;++i) if(!WritePrivateProfileString("Enabled",m_ids[i],m_enabled[i]?"1":"0",temp)) written=LTFALSE;
    WritePrivateProfileString("NativeApprovals",NULL,NULL,temp);
    for(i=0;i<m_overrideCount;++i) if(m_approved[i]) {
        char tag[24],key[72];sprintf(tag,"Override%d",i);
        GetPrivateProfileString(tag,"Key","",key,sizeof(key),m_ini);
        if(!WritePrivateProfileString("NativeApprovals",key,"1",temp))written=LTFALSE;
    }
    if(!WritePrivateProfileString("Selection","Configuration",m_profileIds[m_profile],temp)) written=LTFALSE;
    WritePrivateProfileString(NULL,NULL,NULL,temp);
    if(!written || !MoveFileEx(temp,m_request,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFile(temp); Status("Could not save choices. Existing settings were retained."); return LTFALSE;
    }
    // Clear the Win32 profile API cache after the atomic replacement.
    WritePrivateProfileString(NULL,NULL,NULL,m_request);
    memcpy(m_previous,m_enabled,sizeof(m_previous)); UpdateData(LTFALSE);
    Status("Saved. Exit and restart NOLF to apply changes."); return LTTRUE;
}

void CFolderModManager::ProfilePath(int index,char* path)
{
    strcpy(path,m_ini);char* slash=strrchr(path,'\\');
    if(slash) sprintf(slash+1,"mod-profile-%d.ini",index);
}

void CFolderModManager::PendingStatus()
{
    // Advisory warnings use the launch catalog. The companion revalidates
    // exact package versions, paths and approvals before activating a profile.
    for(int n=0;n<m_count;++n) if(m_enabled[n]) {
        char tag[24],warning[192],list[4096];sprintf(tag,"Mod%d",n);
        GetPrivateProfileString(tag,"Warning","",warning,sizeof(warning),m_ini);
        if(warning[0]) { Status("Missing/incompatible requirement.");return; }
        GetPrivateProfileString(tag,"Conflicts","",list,sizeof(list),m_ini);
        char* item=list;
        while(item && *item) {
            char* next=strchr(item,',');if(next) *next++=0;
            if(!strcmp(item,"core") && !PairApproved(n,item)) { Status("Resource conflict - review overrides.");return; }
            for(int j=0;j<m_count;++j) if(m_enabled[j] && !strcmp(m_ids[j],item) && !PairApproved(n,item)) {
                Status("Resource conflict - review overrides.");return;
            }
            item=next;
        }
    }
    if(m_presetSource>=0) { Status("Profile selected - restart required.");return; }
    for(int i=0;i<m_count;++i) if(m_enabled[i]!=m_running[i]) {
        Status("Pending changes - restart required.");return;
    }
    if(GetPrivateProfileInt("Manager","NoticeCount",0,m_ini)) Status("Read package notices.");
    else Status("Selections match the active game.");
}

void CFolderModManager::SelectionChanged()
{
    UpdateData();
    if(m_profile!=m_previousProfile) {
        if(m_profile>0) {
            char tag[24],path[MAX_PATH];sprintf(tag,"Profile%d",m_profile-1);
            if(!GetPrivateProfileInt(tag,"Valid",0,m_ini)) {
                m_profile=m_previousProfile;UpdateData(LTFALSE);
                Status("Profile unavailable. Open ZIP manager.");return;
            }
            ProfilePath(m_profile-1,path);
            for(int i=0;i<m_count;++i) m_enabled[i]=GetPrivateProfileInt("Enabled",m_ids[i],0,path)!=0;
            memcpy(m_previous,m_enabled,sizeof(m_previous));
            m_presetSource=m_profile-1;
            memset(m_approved,0,sizeof(m_approved));
        }
        m_previousProfile=m_profile;UpdateData(LTFALSE);
    } else {
        for(int i=0;i<m_count;++i) if(m_previous[i]!=m_enabled[i]) {
            Dependencies(i,m_enabled[i],0);m_profile=m_previousProfile=0;
            memcpy(m_previous,m_enabled,sizeof(m_previous));UpdateData(LTFALSE);break;
        }
    }
    PendingStatus();
    m_dwCurrHelpID=0;UpdateHelpText();
}

LTBOOL CFolderModManager::OnLeft() { LTBOOL r=CBaseFolder::OnLeft();SelectionChanged();return r; }
LTBOOL CFolderModManager::OnRight() { LTBOOL r=CBaseFolder::OnRight();SelectionChanged();return r; }
LTBOOL CFolderModManager::OnEnter() { LTBOOL r=CBaseFolder::OnEnter();SelectionChanged();return r; }
LTBOOL CFolderModManager::OnLButtonUp(int x,int y) { LTBOOL r=CBaseFolder::OnLButtonUp(x,y);SelectionChanged();return r; }
LTBOOL CFolderModManager::OnRButtonUp(int x,int y) { LTBOOL r=CBaseFolder::OnRButtonUp(x,y);SelectionChanged();return r; }

uint32 CFolderModManager::OnCommand(uint32 command,uint32 p1,uint32 p2)
{
    if(command==REVIEW_OVERRIDES) { UpdateData();m_review=0;ReviewNext();return 1; }
    if(command==PACKAGE_NOTICES) { m_notice=0;NextNotice();return 1; }
    if(command==SAVE_CHOICES) { SaveChoices(); return 1; }
    if(command==RESTART_GAME) {
        if(g_pGameClientShell->IsInWorld()) {
            HSTRING text=g_pLTClient->CreateString("Restart NOLF and apply your mod choices? Unsaved progress will be lost.");
            m_confirming=LTTRUE;
            g_pInterfaceMgr->ShowMessageBox(text,LTMB_YESNO,RestartCallback,this,LTFALSE,LTFALSE);
            g_pLTClient->FreeString(text);
        } else ConfirmRestart(LTTRUE);
        return 1;
    }
    if(command==OPEN_MANAGER) { LaunchHelper(LTFALSE); return 1; }
    return CBaseFolder::OnCommand(command,p1,p2);
}

void CFolderModManager::ConfirmRestart(LTBOOL accepted)
{
    m_confirming=LTFALSE;
    if(accepted && LaunchHelper(LTTRUE)) {
        g_pLTClient->WriteConfigFile("autoexec.cfg");
        g_pLTClient->Shutdown();
    }
}

#include "../mod_helper_launch.h"
LTBOOL CFolderModManager::LaunchHelper(LTBOOL restart)
{
        if(!SaveChoices()) return LTFALSE;
        char helper[MAX_PATH],root[MAX_PATH];
        GetPrivateProfileString("Manager","Helper","",helper,sizeof(helper),m_ini);
        GetPrivateProfileString("Manager","Root","",root,sizeof(root),m_ini);
        // No shell and no arguments from imported mod manifests.
        if(!helper[0] || !root[0] || strchr(helper,'"') || strchr(root,'"')) {
            Status("Manager launch settings are missing."); return LTFALSE;
        }
        if(NolfLaunchModHelper(helper,root,restart?GetCurrentProcessId():0,!restart)) {
            Status(restart?"Validating choices for restart...":"Advanced manager opened.");
            return LTTRUE;
        }
        Status("Restore the complete ModManager folder from this release.");
        return LTFALSE;
}

LTBOOL CFolderModManager::IsEnabled(const char* id)
{
    if(!strcmp(id,"core"))return LTTRUE;
    for(int i=0;i<m_count;++i)if(!strcmp(id,m_ids[i]))return m_enabled[i];
    return LTFALSE;
}

void CFolderModManager::ReviewNext()
{
    for(;m_review<m_overrideCount;++m_review) {
        char tag[24],winner[64],loser[64],path[192],text[512];sprintf(tag,"Override%d",m_review);
        GetPrivateProfileString(tag,"Winner","",winner,sizeof(winner),m_ini);
        GetPrivateProfileString(tag,"Loser","",loser,sizeof(loser),m_ini);
        if(m_approved[m_review] || !IsEnabled(winner) || !IsEnabled(loser))continue;
        GetPrivateProfileString(tag,"Path","",path,sizeof(path),m_ini);
        sprintf(text,"Allow %s to replace %s from %s? This approval applies to these exact package contents.",winner,path,loser);
        HSTRING value=g_pLTClient->CreateString(text);m_confirming=LTTRUE;
        g_pInterfaceMgr->ShowMessageBox(value,LTMB_YESNO,OverrideCallback,this,LTFALSE,LTFALSE);
        g_pLTClient->FreeString(value);return;
    }
    m_confirming=LTFALSE;
    PendingStatus();
}

LTBOOL CFolderModManager::PairApproved(int index,const char* other)
{
    char tag[24],list[4096];sprintf(tag,"Mod%d",index);
    GetPrivateProfileString(tag,"HardConflicts","",list,sizeof(list),m_ini);
    char* item=list;
    while(item && *item) {
        char* next=strchr(item,',');if(next)*next++=0;
        if(!strcmp(item,other))return LTFALSE;item=next;
    }
    LTBOOL found=LTFALSE;
    for(int i=0;i<m_overrideCount;++i) {
        char winner[64],loser[64];sprintf(tag,"Override%d",i);
        GetPrivateProfileString(tag,"Winner","",winner,sizeof(winner),m_ini);
        GetPrivateProfileString(tag,"Loser","",loser,sizeof(loser),m_ini);
        if((!strcmp(winner,m_ids[index]) && !strcmp(loser,other)) || (!strcmp(loser,m_ids[index]) && !strcmp(winner,other))) {
            found=LTTRUE;if(!m_approved[i])return LTFALSE;
        }
    }
    if(GetPrivateProfileInt("Manager","OverrideOverflow",0,m_ini))return LTFALSE;
    return found;
}

void CFolderModManager::ConfirmOverride(LTBOOL accepted)
{
    m_confirming=LTFALSE;
    if(m_review<m_overrideCount)m_approved[m_review]=accepted;
    ++m_review;m_nextDialog=1;
}

LTBOOL CFolderModManager::Render(HSURFACE surface)
{
    // MessageBox::Close destroys its controls AFTER invoking the callback.
    // Defer the next question until that close has completed.
    int next=m_nextDialog;m_nextDialog=0;
    if(next==1)ReviewNext();else if(next==2)NextNotice();
    return CBaseFolder::Render(surface);
}

LTBOOL CFolderModManager::HandleKeyDown(int key,int rep)
{
    if(key==VK_F1 && m_detail>=0 && m_detail<m_count && !m_confirming) {
        char tag[24],name[96],author[96],version[64],desc[384],warning[192],conflicts[192],text[1200];
        sprintf(tag,"Mod%d",m_detail);
        GetPrivateProfileString(tag,"Name","",name,sizeof(name),m_ini);
        GetPrivateProfileString(tag,"Author","Not specified",author,sizeof(author),m_ini);
        GetPrivateProfileString(tag,"Version","local",version,sizeof(version),m_ini);
        GetPrivateProfileString(tag,"Description","",desc,sizeof(desc),m_ini);
        GetPrivateProfileString(tag,"Warning","Requirements available",warning,sizeof(warning),m_ini);
        GetPrivateProfileString(tag,"Conflicts","",conflicts,sizeof(conflicts),m_ini);
        sprintf(text,"%s (%s) by %s\nActive: %s. Pending: %s.\n%s\n%s%s%s",name,version,author,m_running[m_detail]?"On":"Off",m_enabled[m_detail]?"On":"Off",desc,warning,conflicts[0]?"\nPotential conflicts: ":"",conflicts);
        HSTRING value=g_pLTClient->CreateString(text);m_confirming=LTTRUE;
        g_pInterfaceMgr->ShowMessageBox(value,LTMB_OK,InfoCallback,this,LTFALSE,LTFALSE);
        g_pLTClient->FreeString(value);return LTTRUE;
    }
    return CBaseFolder::HandleKeyDown(key,rep);
}

void CFolderModManager::NextNotice()
{
    m_confirming=LTFALSE;
    int count=GetPrivateProfileInt("Manager","NoticeCount",0,m_ini);
    if(m_notice>=count || m_notice>=129)return;
    char tag[24],text[768];sprintf(tag,"Notice%d",m_notice++);
    GetPrivateProfileString(tag,"Text","",text,sizeof(text),m_ini);
    HSTRING value=g_pLTClient->CreateString(text);m_confirming=LTTRUE;
    g_pInterfaceMgr->ShowMessageBox(value,LTMB_OK,NoticeCallback,this,LTFALSE,LTFALSE);
    g_pLTClient->FreeString(value);
}
#endif
