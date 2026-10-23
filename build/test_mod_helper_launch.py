"""Owned, isolated Windows launcher/restart smoke test. Never controls UI input.

Run only against a disposable review installation. It starts and stops only its
own verified game processes. Final visual/gameplay acceptance belongs to owner.
"""
import argparse,ctypes,json,os,shutil,subprocess,sys,time,zipfile
from ctypes import wintypes
from pathlib import Path
from nolf_mod_manager import read_ini,ini_text
from nolf_mod_core import sha
from make_mod_manager_examples import generate

def build_bridge(tools,out):
    out.mkdir(parents=True,exist_ok=True)
    header=Path(__file__).resolve().parents[1]/'src/modern/launcher/mod_startup.h'
    source=out/'bridge.cpp'
    source.write_text('#include <windows.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include "'+header.as_posix()+'"\n'
        'int main(int argc,char** argv) { if(argc==1) return NolfModStartup()?0:2; if(argc!=4)return 3; return NolfLaunchModHelper(argv[2],argv[1],strtoul(argv[3],0,10),FALSE)?0:4; }\n')
    vc=tools/'toolchains/msvc-6.00-9782';env=os.environ.copy()
    env['PATH']=str(vc/'VC98/Bin')+';'+str(vc/'Common/MSDev98/Bin')+';'+env['PATH']
    env['INCLUDE']=str(vc/'VC98/Include');env['LIB']=str(vc/'VC98/Lib')
    exe=out/'native-menu-bridge.exe'
    r=subprocess.run([str(vc/'VC98/Bin/CL.EXE'),'/nologo','/MT',str(source),'/Fe'+str(exe),'/link','user32.lib'],cwd=out,env=env,capture_output=True,text=True)
    (out/'build.log').write_text(r.stdout+r.stderr);r.check_returncode()
    assert subprocess.run([str(exe)],cwd=out,capture_output=True).returncode==2,'Absent helper must retain original startup path'
    return exe

kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
kernel.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
kernel.TerminateProcess.argtypes=[wintypes.HANDLE,wintypes.UINT]
kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
kernel.CloseHandle.argtypes=[wintypes.HANDLE]

def owned_handle(root,launch,access=0x1000):
    h=kernel.OpenProcess(access,False,launch['pid'])
    if not h:raise AssertionError('Game process exited before startup validation')
    path=ctypes.create_unicode_buffer(32768);size=wintypes.DWORD(len(path))
    expected=root/'profiles'/launch['profile']/'runtime/lithtech.exe'
    if not kernel.QueryFullProcessImageNameW(h,0,path,ctypes.byref(size)) or Path(path.value).resolve()!=expected.resolve():
        kernel.CloseHandle(h);raise AssertionError('Unexpected game process identity')
    return h

def stop_owned(root,launch):
    h=owned_handle(root,launch,0x1000|0x100000|1)
    try:kernel.TerminateProcess(h,0);assert kernel.WaitForSingleObject(h,10000)==0
    finally:kernel.CloseHandle(h)


def restart_owned(root,launch,bridge,helper,env):
    # Retain a handle to the terminated process. This deterministically tests
    # a fast exit before the bundled helper can query the game's image name.
    h=owned_handle(root,launch,0x1000|0x100000|1)
    try:
        kernel.TerminateProcess(h,0);assert kernel.WaitForSingleObject(h,10000)==0
        subprocess.run([str(bridge),str(root),str(helper),str(launch['pid'])],env=env,check=True,capture_output=True)
        return wait_launch(root,launch['pid'])
    finally:kernel.CloseHandle(h)

def wait_launch(root,previous_pid):
    deadline=time.monotonic()+300;path=root/'last-launch.json'
    while time.monotonic()<deadline:
        if path.exists():
            launch=json.loads(path.read_text())
            # exFAT timestamps can round a previous receipt into the future.
            # Process identity, not file timestamp granularity, defines restart.
            if launch['pid']!=previous_pid:
                h=owned_handle(root,launch);kernel.CloseHandle(h)
                return launch
        time.sleep(.2)
    raise AssertionError('No successful game launch receipt')

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,required=True)
    ap.add_argument('--tools-root',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--resume-initial',action='store_true',help='Resume after a slow initial startup completed beyond the test timeout.')
    args=ap.parse_args();root=args.root.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    bridge=build_bridge(args.tools_root.resolve(),out/'bridge without helper')
    helper=root/'ModManager/NolfModHelper.exe'
    env=os.environ.copy();env.update(PATH=str(Path(os.environ['SystemRoot'])/'System32'),PYTHONHOME=str(out/'no interpreter'),PYTHONPATH=str(out/'no modules'))
    env.pop('PSModulePath',None)
    # Public-domain generated image; no retail data is added to the test package.
    generate(out/'examples')
    with zipfile.ZipFile(out/'examples/example-loose.zip') as z:entries={n:z.read(n) for n in z.namelist()}
    meta=json.loads(entries['nolf-mod.json']);meta.update(id='bundled-smoke',name='Bundled startup smoke texture')
    entries['nolf-mod.json']=json.dumps(meta).encode()
    with zipfile.ZipFile(root/'mods/bundled-smoke.zip','w') as z:
        for n,b in entries.items():z.writestr(n,b)
    with zipfile.ZipFile(root/'mods/rejected-test.zip','w') as z:z.writestr('../outside.dll',b'MZforbidden')
    launch=None;checks=[]
    try:
        if args.resume_initial:launch=json.loads((root/'last-launch.json').read_text())
        else:
            previous=json.loads((root/'last-launch.json').read_text())['pid'] if (root/'last-launch.json').exists() else None
            subprocess.run([str(root/'NOLF.exe')],cwd=out,env=env,check=True,timeout=20)
            launch=wait_launch(root,previous)
        time.sleep(3)
        h=owned_handle(root,launch);kernel.CloseHandle(h)
        runtime=json.loads((root/'reports/helper-runtime.json').read_text())
        assert runtime['frozen'] and Path(runtime['runtime_dll']).is_relative_to(root/'ModManager')
        profile=root/'profiles'/launch['profile'];menu=read_ini(profile/'runtime/mods.ini')
        row=next(menu[s] for s in menu.sections() if s.startswith('Mod') and menu[s].get('Id')=='bundled-smoke')
        assert row['Enabled']=='0' and row['Name']==meta['name']
        assert Path(menu['Manager']['Helper'])==helper and 'Python' not in menu['Manager']
        assert json.loads((root/'discovery.json').read_text())['errors']
        checks+=['normal NOLF.exe launched bundled helper and real lithtech.exe','discovered metadata in native menu INI; initially off','invalid archive rejected without blocking game','bundled runtime DLL verified; Python absent from PATH','missing helper returns original launcher path']
        baseline=launch['profile'];request=read_ini(root/'request.ini');request['Enabled']['bundled-smoke']='1'
        (root/'request.ini').write_text(ini_text(request));previous=launch['pid']
        old=launch;launch=None
        launch=restart_owned(root,old,bridge,helper,env);assert launch['profile']!=baseline
        profile=root/'profiles'/launch['profile']
        assert (profile/'mod/INTERFACE/MODMANAGER.PCX').read_bytes()==entries['payload/INTERFACE/MODMANAGER.PCX']
        checks.append('production native restart bridge waited for owned game and composed enabled texture')
        request=read_ini(root/'request.ini');request['Enabled']['bundled-smoke']='0';(root/'request.ini').write_text(ini_text(request));previous=launch['pid']
        old=launch;launch=None;launch=restart_owned(root,old,bridge,helper,env)
        assert launch['profile']==baseline
        checks.append('production restart bridge disabled texture and restored exact baseline profile')
        report=dict(checks=checks,final_profile=baseline,helper_runtime=runtime,
            no_desktop_manager_used=True,actual_menu_clicks=False,texture_visual_check=False,owner_gameplay_acceptance='pending')
        (out/'launcher-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
    finally:
        if launch:stop_owned(root,launch)

if __name__=='__main__':main()
