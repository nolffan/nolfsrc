"""Create a data-only manager from an owned game directory, without gameplay modules.

Input resources are copied once; profiles reuse that private base. No links to
the source installation, copyrighted downloads or source-save migration.
"""
import argparse,json,os,re,shutil,sys,uuid
from pathlib import Path
from nolf_mod_core import sha,check_space,safe_member,atomic_text,error
from nolf_mod_manager import Manager

MODULES=('nolf_mod_manager.py','nolf_mod_core.py','nolf_mod_profiles.py','nolf_mod_ui.py','nolf_mod_discovery.py','nolf_mod_portable.py')

def install(game,root,resources=None,game_version='unknown',runner=None,launcher=None):
    game=Path(game).resolve();root=Path(root).resolve()
    if root.exists():error('Use a new output directory. Existing installations are preserved.')
    if root.is_relative_to(game) or game.is_relative_to(root):error('Manager and input game must be separate directories')
    files={p.name.lower():p for p in game.iterdir() if p.is_file()}
    if 'lithtech.exe' not in files:error('Select a game directory containing lithtech.exe')
    if resources is None:
        command_file=files.get('nolfcmds.txt')
        if command_file:
            resources=[a or b for a,b in re.findall(r'(?i)-rez\s+(?:"([^"]+)"|([^\s]+))',command_file.read_text(errors='replace'))]
            # The current directory is mounted separately from the copied runtime.
            resources=[n for n in resources if n not in ('.','./','.\\')]
        if not resources:resources=[name for name in ('nolf.rez','nolf2.rez','nolfu003.rez','nolfgoty.rez') if name in files]
        if not command_file and (game/'custom').is_dir():resources.append('custom')
        extra=sorted(name for name in files if name.endswith('.rez') and name not in {n.lower() for n in resources})
        if extra:error('Additional REZ archives need an explicit --resources load order: '+', '.join(extra))
    if not resources or len(resources)>16 or len(set(n.lower() for n in resources))!=len(resources):error('Supply 1-16 distinct base resources in load order')
    base=[]
    for name in resources:
        if Path(name).name!=name or name in ('.','..'):error('Base resource must be a direct game resource: '+name)
        source=game/name
        if not source.exists():source=files.get(name.lower(),source)
        if not source.is_dir() and not (source.is_file() and source.suffix.lower()=='.rez'):error('Base resource must be an existing REZ or resource directory: '+name)
        base.append(source)
    # These are trusted files supplied by the player, never ZIP mod contents.
    extensions={'.exe','.dll','.lto','.ren','.cfg','.fnt','.manifest','.txt','.asi','.m3d','.flt','.as6'}
    runtime=[p for n,p in files.items() if p.suffix.lower() in extensions and not n.startswith(('readme','log','modern','crash'))]
    base_files=[p for item in base for p in (item.rglob('*') if item.is_dir() else [item]) if p.is_file()]
    for item in base:
        safe_member(game,item.name)
        if item.is_dir():
            for p in item.rglob('*'):safe_member(game,p.relative_to(game))
    for p in runtime:safe_member(game,p.name)
    needed=sum(p.stat().st_size for p in base_files+runtime)+sum(p.stat().st_size for p in runtime)
    check_space(root.parent,needed)
    stage=root.with_name('.staging-manager-'+uuid.uuid4().hex)
    stage.mkdir()
    try:
        for name in ('base','library/full','library/runtime','mods','profiles','reports','tools'):(stage/name).mkdir(parents=True,exist_ok=True)
        for source in base:
            if source.is_dir():shutil.copytree(source,stage/'base'/source.name)
            else:shutil.copy2(source,stage/'base'/source.name)
        for source in runtime:shutil.copy2(source,stage/'library/runtime'/source.name)
        for name in MODULES:shutil.copy2(Path(__file__).with_name(name),stage/'tools'/name)
        info=dict(schema=1,portable=True,adapter='resources',game_version=game_version,builtins=[],owners={},
            runner=runner or [],base_resources=[p.name for p in base],
            composer_hash='resource-manager-v2',library_hashes={p.relative_to(stage/'library').as_posix():sha(p) for p in (stage/'library').rglob('*') if p.is_file()},
            base_hashes={p.relative_to(game).as_posix():sha(p) for p in base_files})
        for source in base_files+runtime:
            dest=stage/'base'/source.relative_to(game) if source in base_files else stage/'library/runtime'/source.name
            if sha(source)!=sha(dest):error('Source changed during installation: '+source.name)
        atomic_text(stage/'manager.json',json.dumps(info,indent=2))
        for name,mode in [('Open Mod Manager.cmd','--gui'),('Launch NOLF.cmd','--launch')]:
            (stage/name).write_text('@echo off\nstart "" "%~dp0ModManager\\NolfModHelper.exe" --root "%~dp0." '+mode+'\n')
        (stage/'run-manager.sh').write_text('#!/bin/sh\ncd "$(dirname "$0")" || exit 1\nif [ "$#" -eq 0 ]; then set -- --gui; fi\nexec python3 tools/nolf_mod_manager.py --root . "$@"\n',newline='\n')
        os.chmod(stage/'run-manager.sh',0o755)
        stage.rename(root)
    except BaseException:
        # Keep failed initialization for diagnosis; it has no active profile.
        if stage.exists():print('Incomplete initialization retained at',stage,file=sys.stderr)
        raise
    manager=Manager(root);manager.choose_configuration('classic');manager.apply()
    candidate=launcher or files.get('nolf.exe')
    if candidate and (launcher or b'NOLFSRC_MOD_MANAGER_BOOTSTRAP_V2' in Path(candidate).read_bytes()):
        from nolf_mod_discovery import install_native_startup
        install_native_startup(root,candidate)
    return root

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game',type=Path,required=True);parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--resources',nargs='+');parser.add_argument('--game-version',default='unknown')
    parser.add_argument('--wine',help='Wine executable for a Linux host; not a shell command')
    parser.add_argument('--launcher',type=Path,help='Trusted modern NOLF.exe with native Mods startup support')
    parser.add_argument('--helper-bundle',type=Path,help='Release ModManager directory from package_mod_helper.py')
    args=parser.parse_args();root=install(args.game,args.root,args.resources,args.game_version,[args.wine] if args.wine else [],args.launcher)
    if args.helper_bundle:
        from nolf_mod_portable import install_bundle
        install_bundle(root,args.helper_bundle)
    print(root)

if __name__=='__main__':main()
