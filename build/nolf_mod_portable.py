"""Portable release bootstrap. No interpreter discovery or package downloads.

The resource manager remains the sole importer/composer. A read-only release is
copied once to private user storage, without links back into its source files.
"""
import hashlib,json,os,shutil,sys,tempfile,uuid
from contextlib import contextmanager
from pathlib import Path
from nolf_mod_core import atomic_text,check_space,error,manager_lock,safe_member,sha

HELPER='ModManager/NolfModHelper.exe'

def portable_info(info):
    info=dict(info)
    for key in ('root','python','pythonw'):info.pop(key,None)
    info.update(portable=True)
    return info

def writable(root):
    """Probe the directories we actually mutate; directory attributes aren't ACLs."""
    try:
        for name in ('','mods','profiles','reports','configurations'):
            folder=safe_member(root,name) if name else root
            if not folder.exists():continue
            probe=folder/('.write-probe-'+uuid.uuid4().hex)
            # tempfile retries thousands of names on Windows ACL denial because
            # os.access cannot distinguish that denial from a sharing conflict.
            with probe.open('xb'):pass
            probe.unlink()
        return True
    except PermissionError:return False

def user_home():
    if os.name=='nt':
        local=os.environ.get('LOCALAPPDATA')
        if not local:error('Windows local application data folder is unavailable')
        return Path(local)/'NOLFSRC/ModManager'
    return Path(os.environ.get('XDG_DATA_HOME',Path.home()/'.local/share'))/'nolfsrc/mod-manager'

def seed_files(root):
    """Only trusted installation state, never arbitrary siblings or raw inbox ZIPs."""
    names=['manager.json','base','library','profiles','configurations.json','request.ini','active.json']
    for receipt in ('ui.json',):
        path=safe_member(root,receipt)
        if path.exists():
            data=json.loads(path.read_text());directory=data.get('directory','ui' if receipt=='ui.json' else '')
            if not directory or Path(directory).name!=directory:error('Invalid trusted overlay directory')
            names += [receipt,directory]
    mods=safe_member(root,'mods')
    if mods.exists():names += [p.relative_to(root).as_posix() for p in mods.iterdir() if p.is_dir()]
    files={}
    for name in names:
        item=safe_member(root,name)
        if not item.exists():continue
        for p in item.rglob('*') if item.is_dir() else [item]:
            safe_member(root,p.relative_to(root))
            if p.is_file():files[p.relative_to(root).as_posix()]=p
    return files

def state_root(install):
    info=json.loads((install/'manager.json').read_text())
    # Old private developer installations remain supported without rewriting.
    if not info.get('portable'):return install
    identity=hashlib.sha256(os.path.normcase(str(install)).encode()).hexdigest()[:20]
    home=user_home();target=home/identity
    # Once relocated, keep using the same saves even if permissions later change.
    if not target.exists() and writable(install):return install
    home.mkdir(parents=True,exist_ok=True)
    target=safe_member(home,identity)
    seed=hashlib.sha256(json.dumps({n:sha(install/n) for n in ('manager.json','ui.json') if (install/n).exists()},sort_keys=True).encode()).hexdigest()
    with manager_lock(home):
        if target.exists():
            record=json.loads(safe_member(target,'install-origin.json').read_text())
            if record!=dict(install=str(install),seed=seed):
                error('This read-only release has changed. Keep your user-data folder and use a matching release or migrate it with the advanced tools; existing saves were not changed.')
            return target
        files=seed_files(install)
        check_space(home,sum(p.stat().st_size for p in files.values()))
        stage=home/('.staging-'+identity+'-'+uuid.uuid4().hex);stage.mkdir()
        # Failed/interrupted stages are retained, never activated or reused.
        for name,source in files.items():
            dest=safe_member(stage,name);dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(source,dest)
            if sha(source)!=sha(dest):error('Installation changed during user-data initialization: '+name)
        for name in ('base','library/full','mods','profiles','reports','configurations'):
            (stage/name).mkdir(parents=True,exist_ok=True)
        atomic_text(stage/'install-origin.json',json.dumps(dict(install=str(install),seed=seed)))
        stage.rename(target)
    return target

@contextmanager
def external_dll_search():
    """Do not pass PyInstaller's x64 DLL search directory to the x86 game."""
    if os.name!='nt' or not getattr(sys,'frozen',False):
        yield
        return
    import ctypes
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    previous=ctypes.create_unicode_buffer(32768)
    kernel.GetDllDirectoryW(len(previous),previous)
    kernel.SetDllDirectoryW(None)
    try:yield
    finally:kernel.SetDllDirectoryW(previous.value or None)

def open_manager(root):
    from nolf_mod_manager import Manager
    install=Path(root).resolve();state=state_root(install)
    manager=Manager(state);manager.install_root=install
    manager.helper_path=install/HELPER
    if getattr(sys,'frozen',False):
        # A frozen helper never launches the system interpreter, even if stale
        # developer metadata happens to contain an old Python path.
        manager.helper_path=Path(sys.executable).resolve()
        import ctypes
        from ctypes import wintypes
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.GetModuleHandleW.argtypes=[wintypes.LPCWSTR];kernel.GetModuleHandleW.restype=wintypes.HMODULE
        kernel.GetModuleFileNameW.argtypes=[wintypes.HMODULE,wintypes.LPWSTR,wintypes.DWORD]
        dll=ctypes.create_unicode_buffer(32768)
        handle=kernel.GetModuleHandleW('python%d%d.dll'%sys.version_info[:2])
        kernel.GetModuleFileNameW(handle,dll,len(dll))
        atomic_text(state/'reports/helper-runtime.json',json.dumps(dict(frozen=True,
            executable=str(manager.helper_path),runtime_dll=dll.value,
            install=str(install),state=str(state),version=sys.version.split()[0]),indent=2))
    return manager

def install_bundle(root,bundle):
    """Release-builder operation. Refuse replacement of an existing component."""
    root=Path(root).resolve();bundle=Path(bundle).resolve()
    receipt=json.loads((bundle/'bundle.json').read_text())
    expected=receipt['files'];actual={p.relative_to(bundle).as_posix() for p in bundle.rglob('*') if p.is_file() and p.name!='bundle.json'}
    if actual!=set(expected) or 'NolfModHelper.exe' not in expected:error('Incomplete bundled helper')
    for name,digest in expected.items():
        if sha(safe_member(bundle,name))!=digest:error('Bundled helper hash mismatch: '+name)
    destination=safe_member(root,'ModManager')
    if destination.exists():error('A bundled helper already exists; build a new isolated release')
    check_space(root,sum((bundle/n).stat().st_size for n in expected))
    stage=root/('.staging-helper-'+uuid.uuid4().hex)
    shutil.copytree(bundle,stage);stage.rename(destination)
    info=portable_info(json.loads((root/'manager.json').read_text()))
    atomic_text(root/'manager.json',json.dumps(info,indent=2))
    for name,mode in [('Launch NOLF.cmd','--launch'),('Play Selected Mods.cmd','--launch'),('Open Mod Manager.cmd','--gui')]:
        atomic_text(root/name,'@echo off\nstart "" "%~dp0ModManager\\NolfModHelper.exe" --root "%~dp0." '+mode+'\n')
    atomic_text(root/'README-Mods.txt','Place supported data-only ZIP or REZ mods in Mods. Start NOLF.exe, open Mods, enable the mod, save choices and restart. No other application or runtime installation is required. New packages start disabled. Open Mod Manager.cmd is an optional advanced interface. Keep the whole ModManager folder together.\n')
