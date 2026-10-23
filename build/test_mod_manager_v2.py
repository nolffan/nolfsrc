"""Generated package, configuration and interrupted-publication regression cases."""
import hashlib,json,os,shutil,stat,struct,subprocess,sys,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
from nolf_mod_core import inspect_zip,unpack_rez,resolve_mods,validate_manifest,manager_lock,sha,MAX_BYTES
from nolf_mod_manager import Manager
from test_mod_manager import rez_one,ProfileTransactions

def package(path,identifier,files,**extra):
    meta=dict(schema=2,id=identifier,name=identifier,version='1.0',files={n:hashlib.sha256(b).hexdigest() for n,b in files.items()},**extra)
    with zipfile.ZipFile(path,'w') as z:
        z.writestr('nolf-mod.json',json.dumps(meta))
        for name,data in files.items():z.writestr(name,data)
    return path

class Archives(unittest.TestCase):
    def setUp(self):self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
    def tearDown(self):self.temp.cleanup()
    def make(self,files):
        p=self.root/'sample.zip'
        with zipfile.ZipFile(p,'w') as z:
            for n,data in files:z.writestr(n,data)
        return p
    def test_common_wrapper(self):
        m,p=inspect_zip(self.make([('Release 1/data/TEX/demo.dtx',b'pixels')]))
        self.assertEqual(p,{'TEX/DEMO.DTX':b'pixels'})
    def test_wrapped_manifest(self):
        p=package(self.root/'one.zip','test',{'TEX/A.DTX':b'pixels'},author='Example author')
        with zipfile.ZipFile(p) as z:entries=[('Example/'+n,z.read(n)) for n in z.namelist()]
        m,p=inspect_zip(self.make(entries));self.assertEqual(m['author'],'Example author')
    def test_ambiguous_roots(self):
        with self.assertRaisesRegex(ValueError,'Ambiguous'):inspect_zip(self.make([('A/TEX/x.dtx',b'a'),('B/TEX/x.dtx',b'b')]))
    def test_root_and_payload_collision(self):
        with self.assertRaisesRegex(ValueError,'Duplicate'):inspect_zip(self.make([('TEX/a.dtx',b'a'),('payload/TEX/a.dtx',b'b')]))
    def test_rez_and_loose_collision(self):
        with self.assertRaisesRegex(ValueError,'Duplicate'):inspect_zip(self.make([('TEX/A.DTX',b'a'),('pack.rez',rez_one('A','DTX',b'b'))]))
    def test_nested_archives_and_renamed_code(self):
        for magic in (b'MZ',b'\x7fELF',b'#!',b'PK\x03\x04',b'7z\xbc\xaf\x27\x1c',b'Rar!',b'\x1f\x8b'):
            with self.subTest(magic=magic),self.assertRaisesRegex(ValueError,'content'):inspect_zip(self.make([('TEX/A.DTX',magic+b'payload')]))
    def test_renamed_code_in_rez(self):
        with self.assertRaisesRegex(ValueError,'content'):unpack_rez(rez_one('A','DTX',b'MZexecutable'))
    def test_unknown_or_non_rez(self):
        for data in (b'fake archive',rez_one('A','DTX',b'a')[:140]):
            with self.subTest(data=data[:8]),self.assertRaises(ValueError):inspect_zip(self.make([('data.rez',data)]))
    def test_rez_overlapping_directory_and_payload(self):
        data=bytearray(rez_one('A','DTX',b'data'));struct.pack_into('<I',data,143+4,143)
        with self.assertRaisesRegex(ValueError,'Overlapping'):unpack_rez(data)
    def test_rez_directory_loop(self):
        data=bytearray(rez_one('A','DTX',b'data'));_,root,size=struct.unpack_from('<3I',data,127);struct.pack_into('<2I',data,root+4,root,size)
        with self.assertRaisesRegex(ValueError,'directory'):unpack_rez(data)
    def test_corrupt_zip(self):
        p=self.make([('TEX/A.DTX',b'abcdefghijk')]);data=p.read_bytes().replace(b'abcdefghijk',b'abcdefghijkl'[:11][::-1]);p.write_bytes(data)
        with self.assertRaisesRegex(ValueError,'Corrupt'):inspect_zip(p)
    def test_truncated_zip(self):
        p=self.make([('TEX/A.DTX',b'data')]);p.write_bytes(p.read_bytes()[:-20])
        with self.assertRaisesRegex(ValueError,'Corrupt'):inspect_zip(p)
    def test_decompression_preflight(self):
        p=self.root/'bomb.zip'
        with zipfile.ZipFile(p,'w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('TEX/A.DTX',b'0'*2**21)
        with self.assertRaisesRegex(ValueError,'limits'):inspect_zip(p)
    def test_excessive_size_without_allocating(self):
        p=self.make([('TEX/A.DTX',b'data')]);data=bytearray(p.read_bytes());index=data.index(b'PK\x01\x02');struct.pack_into('<I',data,index+24,MAX_BYTES+1);p.write_bytes(data)
        with self.assertRaisesRegex(ValueError,'limits'):inspect_zip(p)
    def test_encrypted_flag(self):
        p=self.make([('TEX/A.DTX',b'data')]);data=bytearray(p.read_bytes());index=data.index(b'PK\x01\x02');struct.pack_into('<H',data,index+8,1);p.write_bytes(data)
        with self.assertRaisesRegex(ValueError,'Encrypted'):inspect_zip(p)
    def test_ntfs_reparse_flag(self):
        info=zipfile.ZipInfo('TEX/A.DTX');info.external_attr=0x400
        with self.assertRaisesRegex(ValueError,'links'):inspect_zip(self.make([(info,b'data')]))
    def test_duplicate_manifest_keys(self):
        with self.assertRaisesRegex(ValueError,'Duplicate JSON'):inspect_zip(self.make([('nolf-mod.json','{"schema":1,"schema":2}'),('TEX/A.DTX',b'data')]))
    def test_file_directory_collision(self):
        with self.assertRaisesRegex(ValueError,'Conflicting file'):inspect_zip(self.make([('TEX/A.DTX',b'a'),('TEX/A.DTX/B.DTX',b'b')]))
    def test_self_extracting_archive_rejected(self):
        p=self.make([('TEX/A.DTX',b'a')]);p.write_bytes(b'MZstub'+p.read_bytes())
        with self.assertRaisesRegex(ValueError,'self-extracting'):inspect_zip(p)
    def test_invalid_constraint_types(self):
        for extra in ({'requires':[{'id':[],'version':'1'}]},{'dependency_versions':[]},{'overrides':[{'id':{},'paths':[]}]}):
            p=package(self.root/'a.zip','a',{'TEX/A.DTX':b'pixels'},**extra)
            with self.subTest(extra=extra),self.assertRaises(ValueError):inspect_zip(p)
    def test_preview_must_be_local_declared_asset(self):
        p=package(self.root/'a.zip','a',{'TEX/A.DTX':b'data'},preview={'image':'https://example.com/a.png'})
        with self.assertRaises(ValueError):inspect_zip(p)
    def test_author_and_version_constraints(self):
        p=package(self.root/'a.zip','a',{'TEX/A.DTX':b'data'},author='Author',requires=[{'id':'b','version':'>=1,<2'}],manager_version='>=2.0',game_version='==1.004')
        meta,_=inspect_zip(p);self.assertEqual(meta['dependency_versions'],{'b':'>=1,<2'})

class Versions(unittest.TestCase):
    def setUp(self):self.catalog={'a':dict(version='1',requires=['b'],dependency_versions={'b':'>=1.2,<2'}),'b':dict(version='1.4')}
    def test_compatible_dependency(self):self.assertEqual(resolve_mods(self.catalog,{'a','b'}),['b','a'])
    def test_incompatible_dependency(self):
        self.catalog['b']['version']='2.0'
        with self.assertRaisesRegex(ValueError,'installed 2.0'):resolve_mods(self.catalog,{'a','b'})
    def test_missing_dependency(self):
        with self.assertRaisesRegex(ValueError,'requires b'):resolve_mods(self.catalog,{'a'})
    def test_full_cycle_explanation(self):
        self.catalog['b']['requires']=['a']
        with self.assertRaisesRegex(ValueError,'a -> b -> a'):resolve_mods(self.catalog,{'a','b'})
    def test_manager_requirement(self):
        self.catalog['b']['manager_version']='>=3'
        with self.assertRaisesRegex(ValueError,'requires manager'):resolve_mods(self.catalog,{'b'})
    def test_game_version_not_guessed(self):
        self.catalog['b']['game_version']='==1.004'
        with self.assertRaisesRegex(ValueError,'configured version is unknown'):resolve_mods(self.catalog,{'b'})
        self.assertEqual(resolve_mods(self.catalog,{'b'},game_version='1.004'),['b'])
    def test_preferred_order_respects_dependencies(self):self.assertEqual(resolve_mods(self.catalog,{'a','b'},['a','b']),['b','a'])

class Transactions(ProfileTransactions):
    # Inherit the existing save-preservation tests against the new framework.
    def setup_resources(self):
        self.m.info['adapter']='resources';self.m.info['owners']={};(self.root/'library/full').mkdir(exist_ok=True)
        self.m.compose=Manager.compose.__get__(self.m)
    def test_import_disabled_and_actual_composition(self):
        self.setup_resources();p=package(self.root/'test.zip','data',{'TEX/A.DTX':b'pixels'})
        self.m.import_zip(p);self.assertNotIn('data',self.m.requested())
        target=self.m.apply({'data'});self.assertEqual((target/'mod/TEX/A.DTX').read_bytes(),b'pixels')
    def test_stale_request_cannot_enable_new_import(self):
        (self.root/'request.ini').write_text('[Enabled]\ndata=1\n')
        self.m.import_zip(package(self.root/'data.zip','data',{'TEX/A.DTX':b'pixels'}))
        self.assertNotIn('data',self.m.requested())
    def test_archive_changed_after_preview(self):
        p=package(self.root/'data.zip','data',{'TEX/A.DTX':b'pixels'});digest=sha(p)
        package(p,'data',{'TEX/A.DTX':b'changed'})
        with self.assertRaisesRegex(ValueError,'changed after preview'):self.m.import_zip(p,expected_sha=digest)
    def test_named_roundtrip_preserves_saves(self):
        self.m.save({'texture-pack'});key=self.m.save_configuration('My configuration');a=self.m.apply();(a/'runtime/Save/Slot01.sav').write_bytes(b'keep')
        duplicate=self.m.duplicate_configuration(key,'Copy');self.m.rename_configuration(duplicate,'Renamed')
        self.m.choose_configuration('classic');b=self.m.apply();self.assertNotEqual(a,b)
        self.m.choose_configuration(duplicate);self.assertEqual(self.m.apply(),a)
        self.m.delete_configuration(key);self.m.delete_configuration(duplicate);self.assertEqual((a/'runtime/Save/Slot01.sav').read_bytes(),b'keep')
    def test_profile_export_import_no_assets(self):
        self.m.save({'texture-pack'});key=self.m.save_configuration('Export');path=self.root/'profile.json';self.m.export_configuration(key,path)
        new=self.m.import_configuration(path);self.m.choose_configuration(new);self.assertEqual(self.m.requested(),{'texture-pack'})
        self.assertNotIn('payload',path.read_text());self.assertNotIn(str(self.root),path.read_text())
    def test_profile_rejects_changed_content(self):
        key=self.m.save_configuration('Pinned');self.m.catalog['texture-pack']['version']='2'
        with self.assertRaisesRegex(ValueError,'different version'):self.m.choose_configuration(key)
    def test_changed_order_marks_named_profile_modified(self):
        self.m.save({'texture-pack','sound-pack'},order=['texture-pack','sound-pack']);self.m.save_configuration('Ordered')
        self.assertEqual(self.m.configuration_name(),'Ordered')
        self.m.save({'texture-pack','sound-pack'},order=['sound-pack','texture-pack'])
        self.assertEqual(self.m.configuration_name(),'Ordered (modified)')
    def test_invalid_pending_selection_types(self):
        for text in ('Order = {}','Approvals = null','Pins = []'):
            (self.root/'request.ini').write_text('[Selection]\n'+text+'\n')
            with self.subTest(text=text),self.assertRaisesRegex(ValueError,'Invalid pending'):self.m.selection()
    def test_import_profile_cannot_grant_override_approval(self):
        key=self.m.save_configuration('Export');path=self.root/'profile.json';self.m.export_configuration(key,path);d=json.loads(path.read_text())
        d['approvals']=[dict(path='TEX/A.DTX',winner='texture-pack',loser='core',winner_hash='fake',loser_hash='core')];path.write_text(json.dumps(d))
        new=self.m.import_configuration(path);self.assertEqual(self.m.configurations()[new]['approvals'],[])
    def test_conflicting_resources_need_declaration(self):
        self.setup_resources()
        for identifier in ('first','second'):self.m.import_zip(package(self.root/(identifier+'.zip'),identifier,{'TEX/A.DTX':identifier.encode()}))
        with self.assertRaisesRegex(ValueError,'TEX/A.DTX.*package must declare'):self.m.apply({'first','second'})
    def test_declared_override_needs_user_approval(self):
        self.setup_resources()
        self.m.import_zip(package(self.root/'first.zip','first',{'TEX/A.DTX':b'first'}))
        self.m.import_zip(package(self.root/'second.zip','second',{'TEX/A.DTX':b'second'},overrides=[{'id':'first','paths':['TEX/A.DTX']}]))
        self.m.save({'first','second'})
        with self.assertRaisesRegex(ValueError,'review and approve'):self.m.apply()
        self.m.approve_overrides([{k:v for k,v in c.items() if k!='declared'} for c in self.m.conflicts()])
        target=self.m.apply();self.assertEqual((target/'mod/TEX/A.DTX').read_bytes(),b'second')
        self.assertEqual(json.loads((target/'profile.json').read_text())['enabled'],['first','second'])
    def test_insufficient_space_preserves_active(self):
        self.m.apply({'texture-pack'});before=(self.root/'active.json').read_bytes()
        with patch('nolf_mod_core.shutil.disk_usage',return_value=shutil._ntuple_diskusage(100,99,1)),self.assertRaisesRegex(ValueError,'Insufficient storage'):self.m.apply({'sound-pack'})
        self.assertEqual(before,(self.root/'active.json').read_bytes())
    def test_interrupted_stage_quarantined(self):
        self.m.apply({'texture-pack'});stage=self.root/'profiles/.staging-interrupted';stage.mkdir();(stage/'partial').write_bytes(b'partial')
        self.m.apply({'sound-pack'});self.assertTrue((self.root/'recovery/profiles-interrupted/partial').exists())
        self.assertFalse(stage.exists())
    def test_import_failure_never_publishes(self):
        p=package(self.root/'data.zip','data',{'TEX/A.DTX':b'pixels'})
        original=Path.rename
        def fail(path,target):
            if path.name.startswith('.staging-'):raise OSError('simulated interruption')
            return original(path,target)
        with patch.object(Path,'rename',fail),self.assertRaises(OSError):self.m.import_zip(p)
        self.assertFalse((self.root/'mods/data').exists());self.assertNotIn('data',self.m.catalog)
    def test_active_publication_failure_keeps_old(self):
        self.m.apply({'texture-pack'});before=(self.root/'active.json').read_bytes();original=os.replace
        def fail(a,b):
            if Path(b).name=='active.json':raise OSError('simulated interruption')
            return original(a,b)
        with patch('nolf_mod_core.os.replace',fail),self.assertRaises(OSError):self.m.apply({'sound-pack'})
        self.assertEqual(before,(self.root/'active.json').read_bytes());self.m.apply({'sound-pack'})
    def test_lock_released_when_child_crashes(self):
        code='import os,sys;from pathlib import Path;from nolf_mod_core import manager_lock\nwith manager_lock(Path(sys.argv[1])): os._exit(7)'
        result=subprocess.run([sys.executable,'-c',code,str(self.root)],cwd=Path(__file__).parent)
        self.assertEqual(result.returncode,7)
        with manager_lock(self.root):pass
    def test_lock_blocks_concurrent_writes(self):
        with manager_lock(self.root),self.assertRaisesRegex(ValueError,'Another'):
            with manager_lock(self.root):pass
    def test_installed_payload_hash_checked(self):
        self.setup_resources();self.m.import_zip(package(self.root/'a.zip','data',{'TEX/A.DTX':b'pixels'}));(self.root/'mods/data/payload/TEX/A.DTX').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'hash changed'):self.m.apply({'data'})
    def test_unexpected_library_file_rejected(self):
        (self.root/'library/runtime/unexpected.dll').write_bytes(b'never executed')
        with self.assertRaisesRegex(ValueError,'library changed'):self.m.apply({'texture-pack'})
    def test_unexpected_profile_file_rejected(self):
        p=self.m.apply({'texture-pack'});(p/'mod/extra.txt').write_bytes(b'unexpected')
        with self.assertRaisesRegex(ValueError,'Profile content changed'):self.m.apply({'texture-pack'})

class GenericInstall(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.game=self.root/'Owned European game';self.game.mkdir()
        for name,data in {'lithtech.exe':b'never executed','nolf.rez':b'owned base fixture','autoexec.cfg':b'settings'}.items():(self.game/name).write_bytes(data)
        (self.game/'Save').mkdir();(self.game/'Save/original.sav').write_bytes(b'original save')
    def tearDown(self):self.temp.cleanup()
    def test_resource_only_install_and_save_isolation(self):
        from install_resource_manager import install
        before={str(p):sha(p) for p in self.game.rglob('*') if p.is_file()};root=install(self.game,self.root/'Manager',game_version='1.004')
        m=Manager(root);self.assertEqual(m.catalog,{});self.assertEqual(m.info['adapter'],'resources')
        self.assertFalse(any(root.rglob('original.sav')))
        m.import_zip(package(self.root/'example.zip','example',{'TEX/A.DTX':b'pixels'}));m.save({'example'});p=m.apply();self.assertEqual((p/'mod/TEX/A.DTX').read_bytes(),b'pixels')
        self.assertEqual(before,{str(p):sha(p) for p in self.game.rglob('*') if p.is_file()})
    def test_reject_nested_destination(self):
        from install_resource_manager import install
        with self.assertRaisesRegex(ValueError,'separate'):install(self.game,self.game/'manager')
    def test_unexpected_archives_need_explicit_order(self):
        from install_resource_manager import install
        (self.game/'patch.rez').write_bytes(b'patch')
        with self.assertRaisesRegex(ValueError,'explicit'):install(self.game,self.root/'manager')
    def test_upstream_custom_resource_directory(self):
        from install_resource_manager import install
        (self.game/'custom/ATTRIBUTES').mkdir(parents=True);(self.game/'custom/ATTRIBUTES/LAYOUT.TXT').write_bytes(b'trusted modern data')
        (self.game/'nolfcmds.txt').write_text('-rez nolf.rez -rez custom')
        root=install(self.game,self.root/'Manager');m=Manager(root)
        self.assertEqual(m.info['base_resources'],['nolf.rez','custom'])
        self.assertEqual((root/'base/custom/ATTRIBUTES/LAYOUT.TXT').read_bytes(),b'trusted modern data')
        (root/'base/custom/CShell.dll').write_bytes(b'not approved')
        with self.assertRaisesRegex(ValueError,'library changed'):m.launch()

if __name__=='__main__':
    suite=unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(t) for t in (Archives,Versions,Transactions,GenericInstall))
    if not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful():raise SystemExit(1)
