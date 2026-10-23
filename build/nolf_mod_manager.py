"""Local NOLF mod profiles and data-only ZIP/REZ imports. Python standard library.

Each selection produces an immutable resource overlay and its own saves. Existing
profiles and the original installation are never rewritten by Apply.
"""
import argparse, configparser, hashlib, io, json, os, re, shutil, stat, struct
import subprocess, sys, tempfile, uuid, zipfile, threading
from contextlib import contextmanager
from pathlib import Path
from nolf_mod_core import sha
from nolf_mod_profiles import Configurations
from nolf_mod_discovery import Discovery,printable
from nolf_mod_core import (canonical, asset_path, error, unpack_rez, inspect_zip, inspect_package,
    validate_manifest, resolve_mods, check_space, manager_lock, safe_member, atomic_text, resource_conflicts, fingerprint, verify_library)

from nolf_mod_core import SCHEMA,MAX_FILES,MAX_BYTES,MAX_MODS,ID,ALLOWED,ROOTS,MANAGED

def atomic(path,text):
    return atomic_text(path,text)

def read_ini(path):
    c=configparser.ConfigParser(interpolation=None); c.optionxform=str
    if Path(path).exists(): c.read(path,encoding='utf-8-sig')
    return c

def ini_text(c):
    s=io.StringIO(); c.write(s); return s.getvalue()

def safe_cleanup(path,parent):
    path=Path(path).resolve(); parent=Path(parent).resolve()
    if path.parent!=parent or not path.name.startswith('.staging-'): error('Refusing non-staging cleanup')
    if path.exists(): shutil.rmtree(path)

class Manager(Configurations,Discovery):
    def __init__(self,root):
        self._mutex=threading.RLock();self._lock_depth=0
        self.root=Path(root).resolve()
        self.info=json.loads((self.root/'manager.json').read_text())
        if self.info.get('schema')!=SCHEMA or (not self.info.get('portable') and Path(self.info['root']).resolve()!=self.root): error('Not an initialized mod manager')
        self.install_root=self.root
        self.helper_path=self.root/'ModManager/NolfModHelper.exe'
        self.catalog={m['id']:m for m in self.info['builtins']}
        for p in sorted((self.root/'mods').glob('*/manifest.json')):
            safe_member(self.root,p.relative_to(self.root))
            m=json.loads(p.read_text())
            m=validate_manifest(m)
            if m.get('builtin') or 'files' not in m:error('Invalid installed data-only mod manifest')
            if not ID.fullmatch(m.get('id','')) or m['id']!=p.parent.name or m['id'] in self.catalog: error('Invalid installed mod identity')
            self.catalog[m['id']]=m
        if len(self.catalog)>MAX_MODS: error('Native menu supports at most 64 mods')

    @contextmanager
    def lock(self):
        with self._mutex:
            if self._lock_depth:
                yield
                return
            with manager_lock(self.root):
                self._lock_depth=1
                try:
                    for parent in ('mods','profiles'):
                        for stage in (self.root/parent).glob('.staging-*'):
                            safe_member(self.root,stage.relative_to(self.root))
                            recovery=safe_member(self.root,'recovery');recovery.mkdir(exist_ok=True)
                            stage.rename(recovery/(parent+'-'+stage.name[9:]))
                    yield
                finally:self._lock_depth=0

    def requested(self):
        c=read_ini(self.root/'request.ini')
        return {i for i,m in self.catalog.items() if c.get('Enabled',i,fallback=str(int(m.get('builtin',False))))=='1'}

    def save(self,enabled,order=None,approvals=None,configuration=None,pins=None):
        with self.lock():return self._save(enabled,order,approvals,configuration,pins)

    def _save(self,enabled,order=None,approvals=None,configuration=None,pins=None):
        enabled=set(enabled);self.resolve(enabled)
        current=self.selection()
        order=current['order'] if order is None else order
        order=[i for i in order if i in enabled]
        order=resolve_mods(self.catalog,enabled,order,self.info.get('game_version','unknown'))
        c=read_ini(self.root/'request.ini');c['Enabled']={i:str(int(i in enabled)) for i in self.catalog}
        c['Selection']=dict(Configuration=current['configuration'] if configuration is None else configuration,
            Order=json.dumps(order),Approvals=json.dumps(current['approvals'] if approvals is None else approvals),
            Pins=json.dumps({i:v for i,v in (current['pins'] if pins is None else pins).items() if i in enabled}))
        atomic(self.root/'request.ini',ini_text(c))

    def resolve(self,enabled):
        order=self.selection()['order'] if hasattr(self,'root') else []
        return resolve_mods(self.catalog,enabled,[i for i in order if i in enabled],getattr(self,'info',{}).get('game_version','unknown'))

    def conflicts(self,enabled=None):
        enabled=self.requested() if enabled is None else set(enabled)
        return resource_conflicts(self.catalog,self.resolve(enabled),self.info.get('owners',{}),self.selection()['approvals'])

    def approve_overrides(self,tokens):
        conflicts=self.conflicts();approved=[]
        for conflict in conflicts:
            token={k:v for k,v in conflict.items() if k!='declared'}
            if token in tokens:
                if not conflict['declared']:error('The package must explicitly declare this override: '+conflict['path'])
                approved.append(token)
        if len(approved)!=len(tokens):error('Override preview changed; review it again')
        self.save(self.requested(),approvals=self.selection()['approvals']+approved)

    def validate_selection(self,enabled):
        order=self.resolve(enabled)
        for i,pin in self.selection()['pins'].items():
            if i in enabled and pin!=dict(version=self.catalog[i].get('version','local'),hash=fingerprint(self.catalog[i])):
                error('Selected profile requires a different version or content of '+i)
        conflicts=self.conflicts(enabled)
        if conflicts:
            lines=[c['path']+': '+c['winner']+' replaces '+c['loser']+(' (review and approve override)' if c['declared'] else ' (package must declare an override, or disable one mod)') for c in conflicts]
            error('Resource conflicts:\n'+'\n'.join(lines[:12]))
        return order

    def toggle(self,enabled,which,on):
        result=set(enabled); visiting=set()
        def enable(i):
            if i not in self.catalog: error('Missing dependency: '+i)
            if i in visiting: error('Dependency cycle at '+i)
            visiting.add(i)
            for d in self.catalog[i].get('requires',[]): enable(d)
            visiting.remove(i); result.add(i)
        if on: enable(which)
        else:
            result.discard(which)
            while True:
                remove={i for i in result if set(self.catalog[i].get('requires',[]))-result}
                if not remove: break
                result-=remove
        self.resolve(result); return result

    def import_zip(self,path,expected_sha=None):
        digest=sha(path)
        if expected_sha and digest!=expected_sha:error("Archive changed after preview; inspect it again.")
        meta,payload=inspect_package(path)
        if sha(path)!=digest:error("Archive changed during inspection; inspect it again.")
        if meta['id'] in self.catalog: error('This mod ID is already installed; keep versions separate or remove the inactive import first.')
        if len(self.catalog)>=MAX_MODS: error('Mod limit reached')
        with self.lock():
            check_space(self.root,sum(map(len,payload.values())))
            if (self.root/'mods'/meta['id']).exists():error('This mod ID is already installed')
            stage=self.root/'mods'/('.staging-'+uuid.uuid4().hex); stage.mkdir()
            try:
                for n,b in payload.items():
                    p=safe_member(stage,'payload/'+n); p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(b)
                (stage/'manifest.json').write_text(json.dumps(meta,indent=2))
                (stage/'source.json').write_text(json.dumps(dict(filename=Path(path).name,sha256=sha(path))))
                # Even an old request entry for a removed package cannot enable
                # a freshly imported archive without a deliberate new choice.
                request=read_ini(self.root/'request.ini')
                if not request.has_section('Enabled'):request.add_section('Enabled')
                request['Enabled'][meta['id']]='0'
                atomic(self.root/'request.ini',ini_text(request))
                stage.rename(self.root/'mods'/meta['id'])
            finally: safe_cleanup(stage,self.root/'mods')
        self.catalog[meta['id']]=meta
        return meta

    def live_game(self):
        if os.name!='nt':
            # Linux/Wine: the launched wrapper may exit before its Windows child.
            # Match executable/cwd inside this manager, not every Wine process.
            proc=Path('/proc')
            if not proc.exists():error('Process checks are unavailable on this platform; launching is not yet supported.')
            for entry in proc.iterdir():
                if not entry.name.isdigit():continue
                try:
                    args=(entry/'cmdline').read_bytes().replace(b'\0',b' ').decode(errors='replace').lower()
                    if 'lithtech.exe' in args and (entry/'cwd').resolve().is_relative_to(self.root):return True
                except (OSError,RuntimeError):continue
            return False
        command="Get-CimInstance Win32_Process -Filter \"Name='lithtech.exe'\" | Select-Object -ExpandProperty ExecutablePath"
        powershell=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
        environment=os.environ.copy();environment.pop('PSModulePath',None)
        r=subprocess.run([str(powershell),'-NoProfile','-Command',command],capture_output=True,text=True,env=environment,creationflags=subprocess.CREATE_NO_WINDOW)
        if r.returncode: error('Could not check whether the managed game is running')
        return any(Path(p.strip()).is_relative_to(self.root) for p in r.stdout.splitlines() if p.strip())

    def compose(self,dest,enabled):
        def put(name,data):
            path=safe_member(dest,name);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        shutil.copytree(self.root/'library/full',dest)
        owners=dict(self.info.get('owners',{}))
        for i in self.resolve(enabled):
            m=self.catalog[i]
            if m.get('builtin'): continue
            for name,digest in m['files'].items():
                name=asset_path(name); source=self.root/'mods'/i/'payload'/name
                safe_member(self.root,source.relative_to(self.root))
                if sha(source)!=digest: error('Installed resource hash changed: '+i+'/'+name)
                previous=owners.get(name)
                if previous and (previous=='core' or previous in enabled) and previous not in m.get('overrides',[]):
                    error(i+' conflicts at '+name+'; manifest must explicitly override '+previous)
                put(name,source.read_bytes()); owners[name]=i

    def menu_ini(self,runtime,enabled):
        blocked={i:set(m.get('conflicts',[])) for i,m in self.catalog.items()};replacements={}
        hard={i:set(v) for i,v in blocked.items()}
        for path,owner in self.info.get('owners',{}).items():replacements[path]=[owner]
        for i,m in self.catalog.items():
            for path in m.get('files',{}):replacements.setdefault(path,[]).append(i)
        approvals=self.selection()['approvals']
        hashes={i:fingerprint(m) for i,m in self.catalog.items()}
        approved={(a.get('path',''),a.get('winner',''),a.get('loser',''),a.get('winner_hash',''),a.get('loser_hash','')) for a in approvals}
        for path,writers in replacements.items():
            for index,a in enumerate(writers):
                for b in writers[index+1:]:
                    if a==b:continue
                    winner,loser=(a,b) if b in self.catalog.get(a,{}).get('overrides',[]) else (b,a)
                    token=(path,winner,loser,hashes[winner],hashes.get(loser,'core'))
                    mod=self.catalog[winner]
                    declared=loser in mod.get('overrides',[]) and (loser not in mod.get('override_paths',{}) or path in mod['override_paths'][loser])
                    if not declared:
                        if a in hard:hard[a].add(b)
                        if b in hard:hard[b].add(a)
                    if token not in approved:
                        if a in blocked:blocked[a].add(b)
                        if b in blocked:blocked[b].add(a)
        c=read_ini(runtime/'mods.ini')
        c['Manager']=dict(Count=str(len(self.catalog)),Root=str(self.install_root),Helper=str(self.helper_path),
            Request=str(self.root/'request.ini'),
            Profile=self.configuration_name(),SelectedProfile=self.selection()['configuration'])
        notices=[]
        for filename in ('startup.json','discovery.json'):
            if (self.root/filename).exists():
                report=json.loads((self.root/filename).read_text())
                if report.get('error'):notices.append('Activation failed. Previous configuration retained. '+report['error'])
                notices+=report.get('errors',[])
        c['Manager']['NoticeCount']=str(len(notices))
        for index,notice in enumerate(notices):c['Notice'+str(index)]=dict(Text=printable(notice))
        candidates=self.override_candidates()
        c['Manager']['OverrideCount']=str(min(len(candidates),128))
        c['Manager']['OverrideOverflow']=str(int(len(candidates)>128))
        for index,(key,token) in enumerate(list(candidates.items())[:128]):
            c['Override'+str(index)]=dict(Key=key,Path=token['path'],Winner=token['winner'],Loser=token['loser'],Approved=str(int(token in approvals)))
        profiles=self.configurations();c['Manager']['ProfileCount']=str(len(profiles))
        for index,(key,profile) in enumerate(profiles.items()):
            tag='Profile'+str(index)
            try:self.check_configuration(profile);valid=True
            except ValueError:valid=False
            c[tag]=dict(Id=key,Name=profile['name'],Valid=str(int(valid)))
            template=configparser.ConfigParser(interpolation=None);template.optionxform=str
            template['Enabled']={i:str(int(i in profile['enabled'])) for i in self.catalog}
            template['Selection']=dict(Configuration=key,Order=json.dumps(profile['order']),Pins=json.dumps(profile['mods']),Approvals=json.dumps(profile['approvals']))
            atomic(runtime/('mod-profile-'+str(index)+'.ini'),ini_text(template))
        for n,(i,m) in enumerate(self.catalog.items()):
            description=m.get('description','Enable or disable this mod.')
            if m.get('requires'): description+=' Requires '+', '.join(self.catalog[d]['name'] if d in self.catalog else d for d in m['requires'])+'.'
            try:self.toggle(set(),i,True);warning=''
            except ValueError as exc:warning=str(exc)[:180]
            c['Mod'+str(n)]=dict(Id=i,Name=m['name'],Enabled=str(int(i in enabled)),Requires=','.join(m.get('requires',[])),
                Conflicts=','.join(sorted(blocked[i])),HardConflicts=','.join(sorted(hard[i])),Warning=warning,Author=m.get('author',''),Version=m.get('version','local'),Type='Built-in feature' if m.get('builtin') else 'Imported data mod',Category=m.get('category','Gameplay' if m.get('builtin') else 'Resources'),Description=description[:240])
        (runtime/'mods.ini').write_text(ini_text(c),encoding='ascii')

    def apply(self,enabled=None):
        if self.live_game(): error('Close the managed game before applying or launching a profile. Your choices can be saved while it is running.')
        enabled=self.requested() if enabled is None else set(enabled)
        order=self.validate_selection(enabled)
        with self.lock():
            verify_library(self.root/'library',self.info['library_hashes'])
            # UI/launcher updates must not start a new save profile.
            # The installer pins this identity; advance it for composition changes.
            composer=self.info['composer_hash'] if 'composer_hash' in self.info else sha(__file__)
            if self.info.get('base_hashes'):composer=[composer,self.info['base_hashes']]
            key=hashlib.sha256(json.dumps([composer,self.info['library_hashes'],[(i,self.catalog[i]) for i in order]],sort_keys=True).encode()).hexdigest()[:16]
            target=safe_member(self.root,'profiles/'+key)
            if not target.exists():
                needed=sum(p.stat().st_size for folder in ('library/full','library/runtime') for p in (self.root/folder).rglob('*') if p.is_file())+sum(sum((self.root/'mods'/i/'payload'/n).stat().st_size for n in self.catalog[i].get('files',{})) for i in order if not self.catalog[i].get('builtin'))
                check_space(self.root,needed)
                stage=self.root/'profiles'/('.staging-'+uuid.uuid4().hex); stage.mkdir()
                try:
                    self.compose(stage/'mod',enabled)
                    shutil.copytree(self.root/'library/runtime',stage/'runtime')
                    (stage/'runtime/Save').mkdir(exist_ok=True)
                    self.menu_ini(stage/'runtime',enabled)
                    receipt=dict(schema=SCHEMA,profile=key,enabled=order,
                        files={p.relative_to(stage).as_posix():sha(p) for d in ('mod','runtime') for p in (stage/d).rglob('*') if p.is_file() and p.suffix.lower() not in ('.cfg','.ini')})
                    (stage/'profile.json').write_text(json.dumps(receipt,indent=2))
                    stage.rename(target)
                finally: safe_cleanup(stage,self.root/'profiles')
            else:
                receipt=json.loads(safe_member(target,'profile.json').read_text())
                if receipt.get('profile')!=key or receipt.get('enabled')!=order:error('Profile receipt does not match the selected combination')
                actual={p.relative_to(target).as_posix() for p in (target/'mod').rglob('*') if p.is_file()}
                expected={n for n in receipt['files'] if n.startswith('mod/')}
                if actual!=expected:error('Profile content changed: missing or unexpected resource files')
                for name,digest in receipt['files'].items():
                    if sha(safe_member(target,name))!=digest: error('Profile content changed: '+name)
                # Refresh catalog labels only when no managed game is running.
                self.menu_ini(target/'runtime',enabled)
            self.save(enabled)
            atomic(self.root/'active.json',json.dumps(dict(profile=key,enabled=order,selection=self.selection()),indent=2))
        return target

    def launch(self):
        with self.lock():return self._launch(self.startup_profile())

    def _launch(self,profile=None):
        if self.info.get('base_hashes'):
            verify_library(self.root/'base',self.info['base_hashes'])
        profile=self.apply() if profile is None else profile
        enabled=set(json.loads((profile/'profile.json').read_text())['enabled'])
        args=['-windowtitle','NOLF - Mod Manager']
        # Relative mounts keep long user-data paths below LithTech's 1023-byte
        # argument limit. Each target is still validated before it is mounted.
        def mount(path):return os.path.relpath(path,profile/'runtime') if self.info.get('portable') else str(path)
        for name in self.info.get('base_resources',('nolf.rez','nolf2.rez','nolfu003.rez','nolfgoty.rez')):
            args+=['-rez',mount(safe_member(self.root,'base/'+name))]
        # Modern releases can carry their rebuilt DLLs and loose resources next
        # to lithtech.exe. Mount the private runtime before the final overlay.
        args+=['-rez','.', '-rez',mount(profile/'mod')]
        # Trusted UI-only updates are mounted after the immutable profile. They
        # can change client presentation without moving saves or weapon tables.
        ui_receipt=self.root/'ui.json'
        if ui_receipt.exists():
            ui_data=json.loads(ui_receipt.read_text())
            location=ui_data.get('directory','ui')
            if not isinstance(location,str) or not re.fullmatch(r'ui(?:-[a-f0-9]{16})?',location): error('Invalid UI update directory')
            ui=self.root/location; installed=ui_data['files']
            allowed={'CSHELL.DLL','ATTRIBUTES/LAYOUT.TXT'}
            actual={p.relative_to(ui).as_posix().upper() for p in ui.rglob('*') if p.is_file()}
            if set(installed)!=allowed or actual!=allowed: error('Invalid UI update resource set')
            for name,digest in installed.items():
                if sha(ui/name)!=digest: error('UI update resource changed: '+name)
            args+=['-rez',mount(ui)]
        environment=os.environ.copy()
        options=dict(multiplayer=0)
        options['ModManagerProbe']=0
        options.update(self.info.get('launch_options',{}))
        for k,v in options.items(): args+=['+'+k,str(v)]
        if len(subprocess.list2cmdline(args))>=1023: error('Installation path is too long for the legacy engine command line')
        prefix=self.info.get('runner',[]) if os.name!='nt' else []
        if os.name!='nt' and not prefix:error('Configure a Wine runner when initializing this manager.')
        from nolf_mod_portable import external_dll_search
        with external_dll_search():
            child=subprocess.Popen([*prefix,str(profile/'runtime/lithtech.exe'),*args],cwd=profile/'runtime',env=environment)
        launch=dict(profile=profile.name,pid=child.pid,args=args)
        atomic(self.root/'last-launch.json',json.dumps(launch,indent=2))
        return child

    def restart_after(self,pid):
        """Wait for the exact managed game to exit; never terminate a process."""
        if not isinstance(pid,int) or not 0<pid<2**32: error('Invalid restart process')
        previous=json.loads((self.root/'last-launch.json').read_text())
        if previous.get('pid')!=pid: error('Restart does not match the last managed game')
        profile=previous.get('profile','')
        if not re.fullmatch(r'[a-f0-9]{16}',profile): error('Invalid restart profile')
        expected=(self.root/'profiles'/profile/'runtime/lithtech.exe').resolve()
        if os.name!='nt': error('In-game restart requires Windows')
        import ctypes
        from ctypes import wintypes
        k=ctypes.WinDLL('kernel32',use_last_error=True)
        k.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]; k.OpenProcess.restype=wintypes.HANDLE
        k.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
        k.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]; k.WaitForSingleObject.restype=wintypes.DWORD
        k.CloseHandle.argtypes=[wintypes.HANDLE]
        handle=k.OpenProcess(0x100000|0x1000,False,pid)
        if handle:
            try:
                buf=ctypes.create_unicode_buffer(32768);size=wintypes.DWORD(len(buf))
                if not k.QueryFullProcessImageNameW(handle,0,buf,ctypes.byref(size)):
                    # The game can exit while the bundled runtime is starting.
                    # A retained handle may still open, but its image query can
                    # fail after termination. Accept only a signalled handle;
                    # an inaccessible or unrelated live process still fails.
                    if k.WaitForSingleObject(handle,0)!=0:
                        error('Could not verify the managed game process')
                elif Path(buf.value).resolve()!=expected:
                    error('Restart process is not the expected managed game')
                if k.WaitForSingleObject(handle,30000)!=0: error('The game did not exit; restart cancelled')
            finally: k.CloseHandle(handle)
        elif ctypes.get_last_error()!=87: error('Could not wait for the managed game to exit')
        child=self.launch()
        atomic(self.root/'reports/restart.json',json.dumps(dict(previous_pid=pid,new_pid=child.pid,profile=json.loads((self.root/'active.json').read_text())['profile']),indent=2))
        return child

def gui(manager):
    from nolf_mod_ui import gui as show
    show(manager)

def main():
    ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('--root',type=Path,required=True)
    ap.add_argument('--restart-after',type=int,help='Wait for this managed game PID to exit before launching.')
    ap.add_argument('--result',type=Path,help='Write a machine-readable operation result (advanced diagnostics).')
    group=ap.add_mutually_exclusive_group(); group.add_argument('--gui',action='store_true'); group.add_argument('--launch',action='store_true'); group.add_argument('--apply',action='store_true'); group.add_argument('--import-zip','--import-package',dest='import_zip',type=Path); group.add_argument('--list',action='store_true'); group.add_argument('--discover',action='store_true')
    group.add_argument('--prepare',action='store_true',help='Discover and validate pending choices without starting the game.')
    o=ap.parse_args()
    from nolf_mod_portable import open_manager
    m=open_manager(o.root)
    result=dict(ok=True,root=str(m.root))
    if o.restart_after is not None:
        if not o.launch: error('--restart-after requires --launch')
        m.restart_after(o.restart_after)
    elif o.launch: m.launch()
    elif o.prepare: result['profile']=str(m.startup_profile())
    elif o.apply: result['profile']=str(m.apply())
    elif o.discover: result['discovery']=m.discover()
    elif o.import_zip: result['imported']=m.import_zip(o.import_zip)
    elif o.list: result.update(enabled=sorted(m.requested()),mods=list(m.catalog.values()))
    else: gui(m)
    if o.result:atomic(o.result,json.dumps(result,indent=2))
    if sys.stdout is not None:print(json.dumps(result,indent=2))

if __name__=='__main__':
    try: main()
    except Exception as e:
        if Path(sys.executable).stem.lower()=='pythonw':
            import tkinter.messagebox
            tkinter.messagebox.showerror('NOLF Mod Manager',str(e))
        else: raise
