"""Exercise the shipped EXE with no Python interpreter in its environment.

Synthetic resources only. The test driver uses Python; every manager operation
is performed by the frozen executable, from an unrelated working directory.
"""
import argparse,configparser,json,os,shutil,subprocess,sys,tempfile,unittest,zipfile
from pathlib import Path
from nolf_mod_core import sha
from nolf_mod_manager import read_ini,ini_text
from test_mod_manager import rez_one
from test_mod_manager_v2 import package

BUNDLE=None

class Frozen(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='NOLF packaged spaces ')
        self.home=Path(self.temp.name);self.root=self.home/'Game with spaces';self.root.mkdir()
        for name in ('mods','profiles','reports','library/full/TEX','library/runtime'):(self.root/name).mkdir(parents=True,exist_ok=True)
        (self.root/'library/full/TEX/A.DTX').write_bytes(b'original pixels')
        (self.root/'library/runtime/fixture.txt').write_bytes(b'private runtime fixture')
        info=dict(schema=1,portable=True,adapter='resources',game_version='1.004',builtins=[],owners={},composer_hash='frozen-fixture-v1',
            library_hashes={p.relative_to(self.root/'library').as_posix():sha(p) for p in (self.root/'library').rglob('*') if p.is_file()})
        (self.root/'manager.json').write_text(json.dumps(info))
        # Deliberately invalid interpreter variables cannot affect a frozen exe.
        self.env=os.environ.copy()
        self.env.update(PATH=str(Path(os.environ['SystemRoot'])/'System32'),PYTHONHOME=str(self.home/'missing interpreter'),
            PYTHONPATH=str(self.home/'missing modules'),LOCALAPPDATA=str(self.home/'User Data'))
        self.env.pop('PSModulePath',None)
        self.helper=BUNDLE/'NolfModHelper.exe'
    def tearDown(self):self.temp.cleanup()
    def run_helper(self,*args,ok=True):
        result=self.home/'result.json'
        if result.exists():result.unlink()
        child=subprocess.run([str(self.helper),'--root',str(self.root),'--result',str(result),*args],
            cwd=self.home,env=self.env,timeout=40,capture_output=True)
        data=json.loads(result.read_text()) if result.exists() else {'missing_result':str(child)}
        diagnostic=Path(self.env['LOCALAPPDATA'])/'NOLFSRC/ModManager/last-error.txt'
        if child.returncode and diagnostic.exists():data['traceback']=diagnostic.read_text()
        self.assertEqual(child.returncode,0 if ok else 1,data)
        self.assertEqual(data['ok'],ok,data)
        return data
    def archive(self,name='one.zip',identifier='one',**meta):
        return package(self.root/'mods'/name,identifier,{'TEX/A.DTX':b'modified pixels'},author='Synthetic Author',description='Bundled resource test',**meta)
    def choose(self,enabled):
        c=read_ini(self.root/'request.ini')
        if not c.has_section('Enabled'):c.add_section('Enabled')
        for key in c['Enabled']:c['Enabled'][key]=str(int(key in enabled))
        (self.root/'request.ini').write_text(ini_text(c))
    def profile(self):return self.root/'profiles'/json.loads((self.root/'active.json').read_text())['profile']
    def test_frozen_runtime_and_native_metadata(self):
        self.archive();self.run_helper('--prepare')
        runtime=json.loads((self.root/'reports/helper-runtime.json').read_text())
        self.assertTrue(runtime['frozen'])
        self.assertTrue(Path(runtime['runtime_dll']).is_relative_to(BUNDLE))
        ini=read_ini(self.profile()/'runtime/mods.ini')
        self.assertEqual(ini['Mod0']['Author'],'Synthetic Author');self.assertEqual(ini['Mod0']['Enabled'],'0')
        self.assertEqual(Path(ini['Manager']['Helper']),self.helper)
        self.assertNotIn('Python',ini['Manager']);self.assertNotIn('Script',ini['Manager'])
    def test_enable_disable_restore_saves(self):
        self.archive();self.run_helper('--prepare');classic=self.profile()
        (classic/'runtime/Save/Slot01.sav').write_bytes(b'classic progress')
        self.choose({'one'});self.run_helper('--prepare');modded=self.profile()
        self.assertNotEqual(classic,modded);self.assertEqual((modded/'mod/TEX/A.DTX').read_bytes(),b'modified pixels')
        self.assertFalse((modded/'runtime/Save/Slot01.sav').exists())
        self.choose(set());self.run_helper('--prepare');self.assertEqual(self.profile(),classic)
        self.assertEqual((classic/'mod/TEX/A.DTX').read_bytes(),b'original pixels')
        self.assertEqual((classic/'runtime/Save/Slot01.sav').read_bytes(),b'classic progress')
    def test_rez_duplicate_and_removed_inbox(self):
        p=self.root/'mods/test.rez';p.write_bytes(rez_one('B','DTX',b'rez pixels'))
        self.run_helper('--discover');first=self.run_helper('--list')['mods']
        p.rename(self.root/'mods/renamed.rez');self.run_helper('--discover')
        self.assertEqual(self.run_helper('--list')['mods'],first)
        (self.root/'mods/renamed.rez').unlink();self.run_helper('--prepare')
        self.assertEqual(self.run_helper('--list')['mods'],first)
    def test_invalid_archives_cannot_block_launch_preparation(self):
        self.archive()
        for name,entries in [('escape.zip',[('../evil.dll',b'MZ')]),('code.zip',[('TEX/A.DTX',b'MZpayload')]),
            ('duplicate.zip',[('TEX/A.DTX',b'a'),('tex/a.dtx',b'b')]),('managed.zip',[('Attributes/Weapons.txt',b'bad')])]:
            with zipfile.ZipFile(self.root/'mods'/name,'w') as z:
                for n,b in entries:z.writestr(n,b)
        (self.root/'mods/broken.zip').write_bytes(b'broken');(self.root/'mods/fake.rez').write_bytes(b'bad rez')
        self.run_helper('--prepare');report=json.loads((self.root/'discovery.json').read_text())
        self.assertEqual(len(report['errors']),6);self.assertTrue((self.root/'active.json').exists())
        self.assertFalse((self.root/'evil.dll').exists())
    def test_failure_preserves_active_and_pending(self):
        self.archive(requires=['missing']);self.run_helper('--prepare');previous=(self.root/'active.json').read_bytes()
        self.choose({'one'});pending=(self.root/'request.ini').read_bytes();self.run_helper('--prepare')
        self.assertEqual((self.root/'active.json').read_bytes(),previous)
        self.assertEqual((self.root/'request.ini').read_bytes(),pending)
        self.assertTrue(json.loads((self.root/'startup.json').read_text())['fallback'])
    def test_relocated_installation(self):
        self.archive();self.run_helper('--prepare');old=self.profile().name
        moved=self.home/'Relocated game';self.root.rename(moved);self.root=moved
        self.run_helper('--prepare');self.assertEqual(self.profile().name,old)
        self.assertEqual(read_ini(self.profile()/'runtime/mods.ini')['Manager']['Root'],str(moved))
    def test_interrupted_import_recovered(self):
        stage=self.root/'mods/.staging-interrupted';stage.mkdir();(stage/'partial').write_bytes(b'unpublished')
        self.archive();self.run_helper('--prepare')
        self.assertTrue((self.root/'recovery/mods-interrupted/partial').exists())
        self.assertIn('one',[m['id'] for m in self.run_helper('--list')['mods']])
    def test_conflict_requires_native_approval(self):
        self.archive()
        package(self.root/'mods/two.zip','two',{'TEX/A.DTX':b'approved winner'},requires=['one'],overrides=['one'])
        self.run_helper('--prepare');original=self.profile()
        self.choose({'one','two'});self.run_helper('--prepare')
        self.assertEqual(self.profile(),original)
        menu=read_ini(original/'runtime/mods.ini');key=menu['Override0']['Key']
        request=read_ini(self.root/'request.ini');request['NativeApprovals']={key:'1'}
        (self.root/'request.ini').write_text(ini_text(request));self.run_helper('--prepare')
        self.assertEqual((self.profile()/'mod/TEX/A.DTX').read_bytes(),b'approved winner')
        self.assertEqual(json.loads((self.profile()/'profile.json').read_text())['enabled'],['one','two'])
    def test_advanced_import_uses_same_library(self):
        p=self.archive();outside=self.home/'outside.zip';p.rename(outside)
        self.run_helper('--import-package',str(outside))
        self.assertEqual(self.run_helper('--list')['enabled'],[])
        shutil.copy2(outside,self.root/'mods/renamed.zip');self.run_helper('--discover')
        self.assertEqual(len(self.run_helper('--list')['mods']),1)
    def test_corrupt_library_never_activates(self):
        self.archive();self.run_helper('--prepare');previous=(self.root/'active.json').read_bytes()
        (self.root/'library/full/TEX/A.DTX').write_bytes(b'corrupted')
        self.run_helper('--prepare',ok=False)
        self.assertEqual((self.root/'active.json').read_bytes(),previous)
    def test_read_only_installation_uses_private_user_data(self):
        self.archive()
        self.run_helper('--prepare');original=self.profile()
        (original/'runtime/Save/Slot01.sav').write_bytes(b'prior progress')
        (self.root/'configurations.json').write_text(json.dumps(dict(schema=1,profiles={})))
        before={p.relative_to(self.root).as_posix():sha(p) for p in self.root.rglob('*') if p.is_file()}
        who=subprocess.check_output(['whoami','/user','/fo','csv','/nh'],text=True).strip().split(',')[-1].strip('"')
        # Real Windows ACL, scoped to this generated fixture and restored in finally.
        subprocess.run(['icacls',str(self.root),'/inheritance:r','/grant:r','*'+who+':(OI)(CI)RX'],check=True,capture_output=True)
        try:
            with self.assertRaises(PermissionError):(self.root/'write-probe').write_bytes(b'forbidden')
            self.assertTrue(json.loads((self.root/'manager.json').read_text())['portable'])
            result=self.run_helper('--prepare');state=Path(result['root'])
            self.assertNotEqual(state,self.root);self.assertTrue(state.is_relative_to(self.home/'User Data'))
            self.assertTrue((state/'mods/one/manifest.json').exists())
            self.assertEqual((Path(result['profile'])/'runtime/Save/Slot01.sav').read_bytes(),b'prior progress')
            self.assertEqual((state/'configurations.json').read_bytes(),(self.root/'configurations.json').read_bytes())
            self.assertEqual(before,{p.relative_to(self.root).as_posix():sha(p) for p in self.root.rglob('*') if p.is_file()})
            first=result['profile'];self.assertEqual(self.run_helper('--prepare')['profile'],first)
        finally:subprocess.run(['icacls',str(self.root),'/grant:r','*'+who+':(OI)(CI)F'],check=True,capture_output=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--bundle',type=Path,required=True);args=ap.parse_args()
    BUNDLE=args.bundle.resolve();unittest.main(argv=[sys.argv[0]],verbosity=2)
