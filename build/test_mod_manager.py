"""ZIP threat cases, dependency behavior and profile transaction checks."""
import argparse,copy,hashlib,json,os,re,stat,struct,subprocess,sys,tempfile,unittest,zipfile
from unittest.mock import patch
from pathlib import Path
from nolf_mod_manager import Manager,inspect_zip,unpack_rez,canonical
from nolf_mod_core import sha

def rez_one(name,extension,data):
    header=b'RezMgr'+b'\0'*(127-6)
    payload_offset=139; directory=payload_offset+len(data)
    entry=struct.pack('<5I',0,payload_offset,len(data),0,1)
    entry+=extension.encode()[::-1].ljust(4,b'\0')+struct.pack('<I',0)+name.encode()+b'\0\0'
    # Root contains a TEX directory, which contains the actual resource entry.
    root=directory+len(entry)
    parent=struct.pack('<4I',1,directory,len(entry),0)+b'TEX\0'
    return header+struct.pack('<3I',1,root,len(parent))+data+entry+parent

class ZipTests(unittest.TestCase):
    def setUp(self): self.temp=tempfile.TemporaryDirectory(); self.root=Path(self.temp.name)
    def tearDown(self): self.temp.cleanup()
    def zip(self,entries):
        path=self.root/'example.zip'
        with zipfile.ZipFile(path,'w') as z:
            for name,data in entries: z.writestr(name,data)
        return path
    def test_legacy_loose(self):
        m,p=inspect_zip(self.zip([('TEX/a.dtx',b'abc')]))
        self.assertTrue(m['id'].startswith('data-')); self.assertEqual(p,{'TEX/A.DTX':b'abc'})
    def test_manifest_hash(self):
        meta=dict(schema=1,id='example',name='Example',version='1',files={'tex/a.dtx':hashlib.sha256(b'abc').hexdigest()})
        m,p=inspect_zip(self.zip([('nolf-mod.json',json.dumps(meta)),('payload/tex/a.dtx',b'abc')]))
        self.assertEqual(m['id'],'example')
        with self.assertRaisesRegex(ValueError,'hashes'): inspect_zip(self.zip([('nolf-mod.json',json.dumps(meta)),('payload/tex/a.dtx',b'changed')]))
    def test_unsafe_paths(self):
        for name in ('../x.dtx','/x.dtx','C:/x.dtx','TEX/x:stream.dtx','TEX/CON.dtx','TEX/x.','TEX/../x.dtx','TEX//x.dtx','TEX/x\x00.dtx'):
            with self.subTest(name=name),self.assertRaises(ValueError): canonical(name)
    def test_case_collision(self):
        with self.assertRaisesRegex(ValueError,'colliding'): inspect_zip(self.zip([('tex/A.dtx',b'a'),('TEX/a.DTX',b'b')]))
    def test_native_code_rejected(self):
        for name in ('cshell.dll','payload/object.lto','TEX/run.exe','TEX/start.py','autoexec.cfg'):
            with self.subTest(name=name),self.assertRaises(ValueError): inspect_zip(self.zip([(name,b'code')]))
    def test_symlink_rejected(self):
        info=zipfile.ZipInfo('TEX/link.dtx'); info.create_system=3; info.external_attr=(stat.S_IFLNK|0o777)<<16
        with self.assertRaisesRegex(ValueError,'links'): inspect_zip(self.zip([(info,b'../../outside')]))
    def test_managed_table_rejected(self):
        with self.assertRaisesRegex(ValueError,'managed'): inspect_zip(self.zip([('ATTRIBUTES/WEAPONS.TXT',b'table')]))
    def test_invalid_metadata(self):
        for meta in ([],dict(schema=1,id='../evil'),dict(schema=1,id='valid',name='x\ny',version='1')):
            with self.subTest(meta=meta),self.assertRaises(ValueError): inspect_zip(self.zip([('nolf-mod.json',json.dumps(meta)),('TEX/x.dtx',b'a')]))
    def test_data_rez(self):
        data=rez_one('TEST','DTX',b'texture')
        self.assertEqual(unpack_rez(data),{'TEX/TEST.DTX':b'texture'})
        m,p=inspect_zip(self.zip([('example.rez',data),('README.txt','Example')]))
        self.assertEqual(p,{'TEX/TEST.DTX':b'texture'})
    def test_native_code_inside_rez(self):
        with self.assertRaisesRegex(ValueError,'data'): unpack_rez(rez_one('EVIL','DLL',b'code'))
    def test_rez_bounds(self):
        b=bytearray(rez_one('TEST','DTX',b'abc'));struct.pack_into('<I',b,131,len(b)+99)
        with self.assertRaisesRegex(ValueError,'directory'): unpack_rez(bytes(b))

class Dependencies(unittest.TestCase):
    def setUp(self):
        self.m=Manager.__new__(Manager)
        self.m.catalog={'base-art':dict(requires=[]),'detail-art':dict(requires=['base-art']),'sound-pack':dict(requires=[]),'texture-pack':dict(requires=[])}
    def test_enable_requirements(self): self.assertEqual(self.m.toggle(set(),'detail-art',True),{'base-art','detail-art'})
    def test_disable_dependents(self): self.assertEqual(self.m.toggle({'base-art','detail-art'},'base-art',False),set())
    def test_independent_package(self): self.assertEqual(self.m.toggle(set(),'sound-pack',True),{'sound-pack'})
    def test_missing_dependency(self):
        self.m.catalog['custom']={'requires':['absent']}
        with self.assertRaisesRegex(ValueError,'requires|Missing'): self.m.resolve({'custom'})
    def test_cycles(self):
        self.m.catalog['a']={'requires':['b']};self.m.catalog['b']={'requires':['a']}
        with self.assertRaisesRegex(ValueError,'cycle'): self.m.resolve({'a','b'})
    def test_conflicts(self):
        self.m.catalog['a']={'conflicts':['texture-pack']}
        with self.assertRaisesRegex(ValueError,'Conflicting'): self.m.resolve({'a','texture-pack'})

class ProfileTransactions(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.root=Path(self.temp.name)
        for n in ('mods','profiles','library/runtime'): (self.root/n).mkdir(parents=True,exist_ok=True)
        (self.root/'library/runtime/game.exe').write_bytes(b'test fixture, never executed')
        mods=[dict(id='texture-pack',name='Texture pack',version='1',builtin=True),dict(id='sound-pack',name='Sound pack',version='1',builtin=True)]
        info=dict(schema=1,root=str(self.root),adapter='resources',builtins=mods,pythonw='test',library_hashes={'runtime/game.exe':sha(self.root/'library/runtime/game.exe')})
        (self.root/'manager.json').write_text(json.dumps(info)); self.m=Manager(self.root)
        self.m.live_game=lambda:False
        # The integration tests exercise the actual resource composer. These
        # cases isolate publication, failure recovery and save preservation.
        def compose(dest,enabled):
            dest.mkdir();(dest/'selection.txt').write_text(','.join(sorted(enabled)))
        self.m.compose=compose
    def tearDown(self): self.temp.cleanup()
    def test_separate_saves_and_return(self):
        a=self.m.apply({'texture-pack'}); (a/'runtime/Save/Slot01.sav').write_bytes(b'my save')
        b=self.m.apply({'sound-pack'});self.assertNotEqual(a,b)
        self.assertFalse((b/'runtime/Save/Slot01.sav').exists())
        again=self.m.apply({'texture-pack'});self.assertEqual(a,again)
        self.assertEqual((again/'runtime/Save/Slot01.sav').read_bytes(),b'my save')
    def test_failed_apply_retains_active(self):
        a=self.m.apply({'texture-pack'});before=(self.root/'active.json').read_bytes()
        def fail(dest,enabled): raise ValueError('deliberate composition conflict')
        self.m.compose=fail
        with self.assertRaisesRegex(ValueError,'conflict'): self.m.apply({'sound-pack'})
        self.assertEqual((self.root/'active.json').read_bytes(),before)
        self.assertEqual(list((self.root/'profiles').iterdir()),[a])
    def test_changed_profile_refused(self):
        a=self.m.apply({'texture-pack'});(a/'mod/selection.txt').write_text('changed')
        with self.assertRaisesRegex(ValueError,'Profile content changed'): self.m.apply({'texture-pack'})
    def test_live_game_blocks_apply(self):
        self.m.live_game=lambda:True
        with self.assertRaisesRegex(ValueError,'Close'): self.m.apply({'texture-pack'})
    def test_library_change_refused(self):
        (self.root/'library/runtime/game.exe').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'library changed'): self.m.apply({'texture-pack'})
    def test_ui_launcher_update_keeps_save_profile(self):
        import nolf_mod_manager as module
        a=self.m.apply({'texture-pack'})
        (a/'runtime/Save/Slot01.sav').write_bytes(b'game progress')
        self.m.info['composer_hash']=sha(module.__file__)
        original=module.sha
        with patch.object(module,'sha',side_effect=lambda p:'new-ui-launcher' if Path(p)==Path(module.__file__) else original(p)):
            self.assertEqual(self.m.apply({'texture-pack'}),a)
        self.assertEqual((a/'runtime/Save/Slot01.sav').read_bytes(),b'game progress')
    def test_ui_overlay_is_last_and_checked(self):
        import nolf_mod_manager as module
        self.m.apply({'texture-pack'})
        ui=self.root/'ui';(ui/'ATTRIBUTES').mkdir(parents=True)
        (ui/'CSHELL.DLL').write_bytes(b'client fixture, never executed')
        (ui/'ATTRIBUTES/LAYOUT.TXT').write_bytes(b'layout')
        (self.root/'ui.json').write_text(json.dumps(dict(files={p.relative_to(ui).as_posix():sha(p) for p in ui.rglob('*') if p.is_file()})))
        with patch.object(module.subprocess,'Popen') as process:
            process.return_value.pid=0
            self.m.launch()
            args=process.call_args.args[0]
            overlays=[args[n+1] for n,v in enumerate(args) if v=='-rez']
            self.assertEqual(overlays[-1],str(ui))
            self.assertEqual(overlays[-2],str(self.m.apply()/'mod'))
            (ui/'Object.lto').write_bytes(b'forbidden extra resource')
            with self.assertRaisesRegex(ValueError,'resource set'): self.m.launch()
    def test_versioned_ui_directory_and_path_validation(self):
        import nolf_mod_manager as module
        self.m.apply({'texture-pack'})
        ui=self.root/'ui-0123456789abcdef';(ui/'ATTRIBUTES').mkdir(parents=True)
        (ui/'CSHELL.DLL').write_bytes(b'never executed');(ui/'ATTRIBUTES/LAYOUT.TXT').write_bytes(b'layout')
        data=dict(directory=ui.name,files={p.relative_to(ui).as_posix():sha(p) for p in ui.rglob('*') if p.is_file()})
        receipt=self.root/'ui.json';receipt.write_text(json.dumps(data))
        with patch.object(module.subprocess,'Popen') as process:
            process.return_value.pid=0;self.m.launch()
            args=process.call_args.args[0]
            self.assertEqual([args[n+1] for n,v in enumerate(args) if v=='-rez'][-1],str(ui))
            data['directory']='../outside';receipt.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError,'directory'): self.m.launch()
    def test_restart_rejects_unrelated_process(self):
        p=self.m.apply({'texture-pack'})
        (self.root/'last-launch.json').write_text(json.dumps(dict(pid=1234,profile=p.name)))
        with self.assertRaisesRegex(ValueError,'last managed game'): self.m.restart_after(4321)
        if os.name=='nt':
            child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'],creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                (self.root/'last-launch.json').write_text(json.dumps(dict(pid=child.pid,profile=p.name)))
                with self.assertRaisesRegex(ValueError,'not the expected managed game'):self.m.restart_after(child.pid)
                self.assertIsNone(child.poll())
            finally:child.terminate();child.wait(timeout=10)
    def test_menu_description_includes_dependency(self):
        p=self.m.apply({'texture-pack','sound-pack'})
        self.m.catalog['texture-pack']['requires']=['sound-pack'];self.m.catalog['texture-pack']['description']='Replacement textures.'
        self.m.menu_ini(p/'runtime',{'texture-pack','sound-pack'})
        from nolf_mod_manager import read_ini
        ini=read_ini(p/'runtime/mods.ini')
        self.assertEqual(ini['Mod0']['Description'],'Replacement textures. Requires Sound pack.')
if __name__=='__main__':
    suite=unittest.TestSuite([unittest.defaultTestLoader.loadTestsFromTestCase(t) for t in (ZipTests,Dependencies,ProfileTransactions)])
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful(): raise SystemExit(1)
