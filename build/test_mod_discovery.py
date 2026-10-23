"""Native inbox, activation fallback and approval regression tests (synthetic data)."""
import configparser,json,os,shutil,subprocess,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
from nolf_mod_manager import Manager,read_ini,ini_text
from nolf_mod_core import sha,inspect_package
from nolf_mod_discovery import approval_key,install_native_startup
from test_mod_manager import ProfileTransactions,rez_one
from test_mod_manager_v2 import package

class Inbox(unittest.TestCase):
    setUp=ProfileTransactions.setUp
    tearDown=ProfileTransactions.tearDown
    def archive(self,name='one.zip',identifier='one',data=b'pixels',**meta):
        return package(self.root/'mods'/name,identifier,{'TEX/A.DTX':data},**meta)
    def test_discover_disabled_and_repeat(self):
        self.archive();r=self.m.discover();self.assertEqual(r['packages'][0]['status'],'imported-disabled')
        self.assertNotIn('one',self.m.requested());self.assertEqual(self.m.discover()['packages'][0]['status'],'registered')
    def test_renamed_and_repacked_no_manifest(self):
        path=self.root/'mods/a.zip'
        with zipfile.ZipFile(path,'w') as z:z.writestr('TEX/A.DTX',b'pixels')
        self.m.discover();identifier=next(i for i in self.m.catalog if i.startswith('data-'))
        path.rename(path.with_name('renamed.zip'))
        self.assertEqual(self.m.discover()['packages'][0]['id'],identifier)
        with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('Wrap/TEX/A.DTX',b'pixels')
        self.m.discover();self.assertEqual(len(self.m.catalog),3)
    def test_changed_archive_retains_pin(self):
        self.archive();self.m.discover();self.m.save({'one'});self.m.save_configuration('Pinned')
        before=(self.root/'mods/one/payload/TEX/A.DTX').read_bytes()
        self.archive(data=b'changed');r=self.m.discover();self.assertIn('different version/content',r['errors'][0])
        self.assertEqual((self.root/'mods/one/payload/TEX/A.DTX').read_bytes(),before)
        self.assertIn('one',self.m.requested())
    def test_deleted_inbox_keeps_library_and_saves(self):
        p=self.archive();self.m.discover();self.m.save({'one'});active=self.m.apply()
        (active/'runtime/Save/test.sav').write_bytes(b'save');p.unlink();self.m.discover()
        self.assertEqual(self.m.apply(),active);self.assertEqual((active/'runtime/Save/test.sav').read_bytes(),b'save')
    def test_standalone_rez_shared_parser(self):
        p=self.root/'mods/data.rez';p.write_bytes(rez_one('A','DTX',b'pixels'))
        m,data=inspect_package(p);self.assertEqual(data,{'TEX/A.DTX':b'pixels'})
        self.m.discover();self.assertIn(m['id'],self.m.catalog);self.assertNotIn(m['id'],self.m.requested())
    def test_invalid_packages_do_not_block_good_package(self):
        self.archive();(self.root/'mods/bad.zip').write_bytes(b'bad');(self.root/'mods/bad.rez').write_bytes(b'MZ executable')
        with zipfile.ZipFile(self.root/'mods/escape.zip','w') as z:z.writestr('../outside.exe',b'MZ')
        r=self.m.discover();self.assertEqual(len(r['errors']),3);self.assertIn('one',self.m.catalog)
        self.assertFalse((self.root/'outside.exe').exists());self.m.startup_profile()
    def test_case_colliding_filenames(self):
        if os.name=='nt':self.skipTest('Windows cannot create case-colliding sibling filenames')
        self.archive('a.zip');self.archive('A.zip','two');self.assertEqual(len(self.m.discover()['errors']),2)
    def test_symlink_inbox(self):
        p=self.root/'safe.zip';package(p,'one',{'TEX/A.DTX':b'a'})
        try:(self.root/'mods/link.zip').symlink_to(p)
        except OSError:self.skipTest('Symlink creation unavailable')
        self.assertIn('links',self.m.discover()['errors'][0])
    def test_fallback_preserves_pending_and_active(self):
        active=self.m.apply({'texture-pack'});before=(self.root/'active.json').read_bytes()
        self.archive(requires=['missing']);self.m.discover()
        c=read_ini(self.root/'request.ini');c['Enabled']['one']='1';(self.root/'request.ini').write_text(ini_text(c))
        pending=(self.root/'request.ini').read_bytes();self.assertEqual(self.m.startup_profile(),active)
        self.assertEqual((self.root/'request.ini').read_bytes(),pending)
        self.assertEqual((self.root/'active.json').read_bytes(),before)
        self.assertTrue(json.loads((self.root/'startup.json').read_text())['fallback'])
        menu=read_ini(active/'runtime/mods.ini');self.assertIn('Activation failed',menu['Notice0']['Text'])
    def test_native_explicit_approval_only(self):
        self.archive();self.archive('two.zip','two',b'new',requires=['one'],overrides=['one']);self.m.discover()
        active=self.m.apply({'texture-pack'});self.m.save({'one','two'})
        self.assertEqual(self.m.startup_profile(),active)
        token=self.m.conflicts()[0];c=read_ini(self.root/'request.ini');c['NativeApprovals']={approval_key(token):'1'}
        (self.root/'request.ini').write_text(ini_text(c));applied=self.m.startup_profile()
        self.assertNotEqual(applied,active);self.assertEqual(self.m.resolve({'two','one'}),['one','two'])
        self.assertFalse(self.m.conflicts())
    def test_forged_or_stale_approval_not_accepted(self):
        self.archive();self.archive('two.zip','two',b'new',requires=['one'],overrides=['one']);self.m.discover()
        active=self.m.apply({'texture-pack'});self.m.save({'one','two'})
        c=read_ini(self.root/'request.ini');c['NativeApprovals']={'a'*64:'1'};(self.root/'request.ini').write_text(ini_text(c))
        self.assertEqual(self.m.startup_profile(),active)
    def test_undeclared_override_cannot_be_approved(self):
        self.archive();self.archive('two.zip','two',b'new');self.m.discover();active=self.m.apply({'texture-pack'});self.m.save({'one','two'})
        token=self.m.conflicts()[0];c=read_ini(self.root/'request.ini');c['NativeApprovals']={approval_key(token):'1'};(self.root/'request.ini').write_text(ini_text(c))
        self.assertEqual(self.m.startup_profile(),active)
    def test_interrupted_import_recovered(self):
        self.archive();stage=self.root/'mods/.staging-interrupted';stage.mkdir();(stage/'partial').write_bytes(b'incomplete')
        self.m.discover();self.assertTrue((self.root/'recovery/mods-interrupted/partial').exists());self.assertIn('one',self.m.catalog)
    def test_transient_import_failure_retries(self):
        self.archive()
        with patch.object(self.m,'import_zip',side_effect=OSError('disk full')):self.assertIn('disk full',self.m.discover()['errors'][0])
        self.assertEqual(self.m.discover()['packages'][0]['status'],'imported-disabled')
    def test_native_metadata(self):
        self.archive(author='Test Author',description='Synthetic data');self.m.discover();p=self.m.apply()
        c=read_ini(p/'runtime/mods.ini');self.assertEqual(c['Mod2']['Author'],'Test Author');self.assertEqual(c['Mod2']['Version'],'1.0')
    def test_launcher_requires_capability(self):
        launcher=self.root/'old.exe';launcher.write_bytes(b'MZ old launcher')
        with self.assertRaisesRegex(ValueError,'modern NOLF.exe'):install_native_startup(self.root,launcher)
    def test_native_startup_sidecar(self):
        launcher=self.root/'new.exe';launcher.write_bytes(b'MZ NOLFSRC_MOD_MANAGER_BOOTSTRAP_V2')
        install_native_startup(self.root,launcher)
        self.assertEqual(sha(launcher),sha(self.root/'NOLF.exe'))
        self.assertFalse((self.root/'mod-manager.ini').exists())
    def test_changed_manifestless_content_is_new_disabled_identity(self):
        p=self.root/'mods/local.zip'
        with zipfile.ZipFile(p,'w') as z:z.writestr('TEX/A.DTX',b'old')
        self.m.discover();old=next(i for i in self.m.catalog if i.startswith('data-'));self.m.save({old})
        with zipfile.ZipFile(p,'w') as z:z.writestr('TEX/A.DTX',b'new')
        self.m.discover();self.assertEqual(len(self.m.catalog),4);self.assertEqual(self.m.requested(),{old})
    def test_native_preview_is_bounded(self):
        files={'TEX/A%d.DTX'%i:b'a' for i in range(180)}
        package(self.root/'mods/a.zip','first',files)
        package(self.root/'mods/b.zip','second',files,overrides=['first'])
        self.m.discover();self.assertEqual(len(self.m.override_candidates()),129)
        p=self.m.apply();c=read_ini(p/'runtime/mods.ini')
        self.assertEqual(c['Manager']['OverrideCount'],'128');self.assertEqual(c['Manager']['OverrideOverflow'],'1')
    def test_partial_override_remains_hard_conflict(self):
        files={'TEX/A.DTX':b'a','TEX/B.DTX':b'b'}
        package(self.root/'mods/a.zip','first',files)
        package(self.root/'mods/b.zip','second',files,overrides=[dict(id='first',paths=['TEX/A.DTX'])])
        self.m.discover();p=self.m.apply();c=read_ini(p/'runtime/mods.ini')
        self.assertIn('first',c['Mod3']['HardConflicts']);self.assertEqual(len(self.m.override_candidates()),1)
    def test_conflict_hashing_is_per_package(self):
        from nolf_mod_core import fingerprint,resource_conflicts
        files={'TEX/A%d.DTX'%i:'a'*64 for i in range(100)}
        catalog={'a':dict(files=files),'b':dict(files=files,overrides=['a'])}
        with patch('nolf_mod_core.fingerprint',wraps=fingerprint) as hash_mod:
            self.assertEqual(len(resource_conflicts(catalog,['a','b'],{})),100)
            self.assertEqual(hash_mod.call_count,2)
    def test_invalid_approval_values(self):
        (self.root/'request.ini').write_text('[Selection]\nApprovals = [{"path": []}]\n')
        with self.assertRaisesRegex(ValueError,'Invalid pending'):self.m.selection()
    def test_native_inbox_and_desktop_share_imports(self):
        p=self.archive();self.m.import_zip(p);self.assertEqual(self.m.discover()['packages'][0]['status'],'registered')
        self.assertEqual(len(self.m.catalog),3)
    def test_discovered_enable_disable_resources_and_named_saves(self):
        full=self.root/'library/full/TEX';full.mkdir(parents=True);(full/'A.DTX').write_bytes(b'base pixels')
        self.m.info['adapter']='resources';self.m.info['library_hashes']['full/TEX/A.DTX']=sha(full/'A.DTX')
        del self.m.compose
        self.m.save(set());base=self.m.apply();base_key=self.m.save_configuration('Original')
        self.archive();self.m.discover();self.m.save({'one'});modded=self.m.startup_profile()
        self.assertEqual((modded/'mod/TEX/A.DTX').read_bytes(),b'pixels')
        (modded/'runtime/Save/mod.sav').write_bytes(b'progress');mod_key=self.m.save_configuration('Texture')
        self.m.choose_configuration(base_key);restored=self.m.startup_profile()
        self.assertEqual(restored,base);self.assertEqual((restored/'mod/TEX/A.DTX').read_bytes(),b'base pixels')
        self.assertFalse((restored/'runtime/Save/mod.sav').exists())
        self.m.choose_configuration(mod_key);self.assertEqual(self.m.startup_profile(),modded)
        self.assertEqual((modded/'runtime/Save/mod.sav').read_bytes(),b'progress')
    def test_no_space_activation_falls_back(self):
        active=self.m.apply({'texture-pack'});self.m.save({'sound-pack'})
        with patch('nolf_mod_manager.check_space',side_effect=ValueError('Insufficient storage')):
            self.assertEqual(self.m.startup_profile(),active)
        self.assertEqual(self.m.requested(),{'sound-pack'})
        self.assertIn('Insufficient storage',json.loads((self.root/'startup.json').read_text())['error'])
    def test_oversized_package_rejected_before_hash(self):
        p=self.root/'mods/big.zip';p.write_bytes(b'x')
        actual=Path.stat
        def size(path,*args,**kw):
            result=actual(path,*args,**kw)
            if path==p and not kw.get('follow_symlinks') is False:
                values=list(result);values[6]=300*1024**2;return os.stat_result(values)
            return result
        with patch.object(Path,'stat',size),patch('nolf_mod_discovery.sha') as hashing:
            self.assertIn('size limit',self.m.discover()['errors'][0]);hashing.assert_not_called()
    def test_windows_installer_preserves_checksum_checks(self):
        if os.name!='nt':self.skipTest('Windows installer checker')
        # Execute the actual installer preflight, without its disc extraction.
        source=Path(__file__).with_name('game-scripts')/'windows-install.ps1'
        script=source.read_text();check=script[script.index('$sumsPath ='):script.index('if ($bad.Count -or $extra.Count)')]
        fixture=self.root/'release';fixture.mkdir();(fixture/'stock.dll').write_bytes(b'owned test fixture')
        (fixture/'Mods').mkdir();(fixture/'Mods/mod.zip').write_bytes(b'not loaded by installer')
        (fixture/'SHA256SUMS').write_text(sha(fixture/'stock.dll')+'  stock.dll\n')
        driver=self.root/'check.ps1'
        driver.write_text('param([string]$here)\n$ErrorActionPreference="Stop"\n$lines=@()\nfunction Fail($m) { throw $m }\nfunction Get-Sha256($p) { [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($p))).Replace(\'-\',\'\').ToLowerInvariant() }\n'+check+'\n@{ bad=@($bad); extra=@($extra) } | ConvertTo-Json -Compress\n')
        def run():
            command='& { '+driver.read_text()+" } '"+str(fixture).replace("'","''")+"'"
            r=subprocess.run(['powershell','-NoProfile','-NonInteractive','-Command',command],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(r.returncode,0,r.stderr)
            return json.loads(r.stdout.splitlines()[-1])
        self.assertEqual(run(),dict(bad=[],extra=[]))
        (fixture/'stock.dll').write_bytes(b'changed');(fixture/'outside.dll').write_bytes(b'unexpected')
        result=run();self.assertEqual(result['bad'],['changed: stock.dll']);self.assertEqual(result['extra'],['outside.dll'])

if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Inbox))
    if not result.wasSuccessful():raise SystemExit(1)
