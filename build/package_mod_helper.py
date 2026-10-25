"""Build a self-contained Windows component with already installed PyInstaller.

Build-time Python only. No package installation, downloads or retail game data.
"""
import argparse,hashlib,importlib.metadata,json,os,shutil,subprocess,sys,zipfile
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,required=True)
    out=ap.parse_args().output.resolve()
    if os.name!='nt':raise ValueError('Build this Windows component on Windows')
    if out.exists():raise ValueError('Use a new output directory; previous artifacts are retained')
    version=importlib.metadata.version('pyinstaller')
    source=Path(__file__).resolve().parent;out.mkdir(parents=True)
    args=[sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onedir','--windowed','--noupx',
        '--name','NolfModHelper','--distpath',str(out/'dist'),'--workpath',str(out/'work'),
        '--specpath',str(out),'--paths',str(source),str(source/'nolf_mod_helper.py')]
    with (out/'build.log').open('w') as log:subprocess.run(args,check=True,stdout=log,stderr=subprocess.STDOUT)
    package=out/'dist/NolfModHelper';bundle=out/'ModManager';package.rename(bundle)
    licenses=bundle/'licenses';licenses.mkdir()
    shutil.copy2(Path(sys.base_prefix)/'LICENSE.txt',licenses/'Python-LICENSE.txt')
    dist=importlib.metadata.distribution('pyinstaller')
    license_path=next(p for p in dist.files if p.name=='COPYING.txt')
    shutil.copy2(dist.locate_file(license_path),licenses/'PyInstaller-COPYING.txt')
    # The official CPython Windows LICENSE already carries Tcl's full terms.
    for name in ('tk8.6',):
        license_path=Path(sys.base_prefix)/'tcl'/name/'license.terms'
        if not license_path.exists():raise ValueError('Missing Tcl/Tk license: '+str(license_path))
        shutil.copy2(license_path,licenses/(name+'-license.terms'))
    for license_path in (source/'mod_helper_licenses').iterdir():
        if license_path.is_file():shutil.copy2(license_path,licenses/license_path.name)
    (licenses/'RUNTIME-NOTICES.txt').write_text(
        'NOLF Mod Helper bundles CPython '+sys.version.split()[0]+' and its standard library, Tcl/Tk (optional advanced UI), and the PyInstaller '+version+' bootloader.\n'
        'Python: PSF license and included third-party notices. Tcl/Tk: BSD-style terms. PyInstaller: GPL with bootloader distribution exception; see COPYING.\n'
        'Microsoft VC runtime DLLs from the official Python runtime are included app-local under Microsoft Visual Studio runtime redistribution terms. No system installation is needed.\n'
        'NOLFSRC manager source licensing follows the repository; consult docs/LICENSE-NOTES.md before public distribution. No retail game assets are included.\n')
    files={p.relative_to(bundle).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(bundle.rglob('*')) if p.is_file()}
    receipt=dict(format=1,component='NolfModHelper',python=sys.version.split()[0],pyinstaller=version,
        architecture='x64' if sys.maxsize>2**32 else 'x86',files=files,
        sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.glob('nolf_mod_*.py'))},
        bytes=sum((bundle/n).stat().st_size for n in files))
    (bundle/'bundle.json').write_text(json.dumps(receipt,indent=2))
    archive=out/'NOLF-ModManager-Windows.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(bundle.rglob('*')):
            if p.is_file():z.write(p,'ModManager/'+p.relative_to(bundle).as_posix())
    print(json.dumps(dict(bundle=str(bundle),archive=str(archive),bytes=receipt['bytes'],compressed_bytes=archive.stat().st_size,files=len(files),architecture=receipt['architecture']),indent=2))

if __name__=='__main__':main()
