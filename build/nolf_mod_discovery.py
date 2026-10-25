"""Headless inbox discovery. No archive parser or gameplay knowledge lives here."""
import configparser,hashlib,json,os,shutil
from pathlib import Path
from nolf_mod_core import (MAX_BYTES,atomic_text,error,fingerprint,inspect_package,
    safe_member,sha)

def printable(value,limit=700):
    return ''.join(c if 32<=ord(c)<127 else ' ' for c in str(value))[:limit]

def approval_key(token):
    return hashlib.sha256(json.dumps({k:v for k,v in token.items() if k!='declared'},sort_keys=True).encode()).hexdigest()

class Discovery:
    def inbox(self):
        # Legacy managers store their library in mods/<id>. On Windows Mods is
        # that same directory. Only immediate package files are ever scanned.
        name='mods' if os.name=='nt' else 'Mods'
        return safe_member(getattr(self,'install_root',self.root),name)

    def discover(self):
        """Validate new hashes, report failures individually, never enable imports."""
        with self.lock():
            inbox=self.inbox()
            if getattr(self,'install_root',self.root)==self.root:inbox.mkdir(exist_ok=True)
            report=dict(schema=1,packages=[],errors=[])
            sources={}
            for i,m in self.catalog.items():
                if m.get('builtin'):continue
                source=safe_member(self.root,'mods/'+i+'/source.json')
                if source.exists():sources[json.loads(source.read_text()).get('sha256')]=i
            seen=set();names={};scanned_bytes=0
            folders=[inbox] if inbox.exists() else []
            if os.name!='nt' and (self.root/'mods').exists():folders.append(safe_member(self.root,'mods'))
            paths=sorted((p for folder in folders for p in folder.iterdir() if p.suffix.lower() in ('.zip','.rez')),key=lambda p:(p.name.casefold(),p.name))
            for p in paths:names.setdefault(p.name.casefold(),[]).append(p)
            for p in paths[:128]:
                entry=dict(filename=printable(p.name),status='rejected')
                try:
                    owner=getattr(self,'install_root',self.root) if p.parent==inbox else self.root
                    safe_member(owner,p.relative_to(owner))
                    if not p.is_file():error('Package is not a regular file')
                    if len(names[p.name.casefold()])!=1:error('Case-colliding package filenames; rename one package')
                    if p.stat().st_size>MAX_BYTES+8*1024**2:error('Package exceeds size limit')
                    scanned_bytes+=p.stat().st_size
                    if scanned_bytes>1024*1024**2:error('Inbox exceeds 1 GiB per scan; move registered archives out of Mods')
                    digest=sha(p);entry['sha256']=digest
                    if digest in sources:
                        entry.update(status='registered',id=sources[digest])
                    else:
                        meta,payload=inspect_package(p)
                        if sha(p)!=digest:error('Package changed while reading; finish copying it and restart')
                        existing=self.catalog.get(meta['id'])
                        if existing:
                            # Manifest-less names are presentation only; explicit
                            # manifests must match every pinned metadata field.
                            same=fingerprint(existing)==fingerprint(meta)
                            if meta['id'].startswith('data-'):same=existing.get('files')==meta['files']
                            if not same:error('Installed ID '+meta['id']+' has different version/content. Kept the pinned copy; use a new stable ID for a separate edition.')
                            entry.update(status='duplicate',id=meta['id'])
                        else:
                            # Do not retain the preview payload while the shared
                            # importer performs its final race-checked read.
                            del payload
                            self.import_zip(p,expected_sha=digest)
                            entry.update(status='imported-disabled',id=meta['id'])
                        sources[digest]=meta['id']
                    if digest in seen:entry['status']='duplicate'
                    seen.add(digest)
                except (ValueError,OSError,configparser.Error,json.JSONDecodeError) as exc:
                    entry['message']=printable(exc)
                    report['errors'].append(entry['filename']+': '+entry['message'])
                report['packages'].append(entry)
            if len(paths)>128:report['errors'].append('More than 128 inbox packages. Move older archives out of Mods and restart.')
            atomic_text(self.root/'discovery.json',json.dumps(report,indent=2))
            return report

    def override_candidates(self):
        """Bounded native review; the final resolver still checks the full graph."""
        candidates={};hashes={i:fingerprint(m) for i,m in self.catalog.items()}
        writers={}
        for path,owner in self.info.get('owners',{}).items():writers[path]=[owner]
        for i,m in self.catalog.items():
            for path in m.get('files',{}):writers.setdefault(path,[]).append(i)
        for path,ids in sorted(writers.items()):
            for a in ids:
                for b in ids:
                    if a==b or a not in self.catalog:continue
                    m=self.catalog[a]
                    if b not in m.get('overrides',[]):continue
                    allowed=m.get('override_paths',{}).get(b,['*'])
                    if '*' not in allowed and path not in allowed:continue
                    token=dict(path=path,winner=a,loser=b,winner_hash=hashes[a],loser_hash=hashes.get(b,'core'))
                    candidates[approval_key(token)]=token
                    # One extra entry conveys overflow without building an
                    # unbounded preview for large overlapping texture packs.
                    if len(candidates)>128:return candidates
        return candidates

    def consume_native_approvals(self):
        from nolf_mod_manager import read_ini
        c=read_ini(self.root/'request.ini')
        if not c.has_section('NativeApprovals'):return
        conflicts=self.conflicts();approved=[]
        for token in conflicts:
            if token['declared'] and c.get('NativeApprovals',approval_key(token),fallback='0')=='1':
                approved.append({k:v for k,v in token.items() if k!='declared'})
        if approved:self.approve_overrides(approved)

    def startup_profile(self):
        """Prepare pending choices, or validated last-good resources on failure.

        Pending choices are retained after a failed activation so the native
        menu can explain and fix them. No success is reported for that attempt.
        """
        from nolf_mod_manager import read_ini,ini_text
        with self.lock():
            self.discover()
            atomic_text(self.root/'startup.json',json.dumps(dict(error='',fallback=False)))
            try:
                self.consume_native_approvals()
                return self.apply()
            except (ValueError,OSError,configparser.Error) as exc:
                message=printable(exc)
                active=json.loads((self.root/'active.json').read_text()) if (self.root/'active.json').exists() else None
                if not active or 'selection' not in active:raise
                pending=(self.root/'request.ini').read_bytes().decode('utf-8')
                try:
                    c=read_ini(self.root/'request.ini');c.remove_section('NativeApprovals')
                    c['Enabled']={i:str(int(i in active['enabled'])) for i in self.catalog}
                    s=active['selection'];c['Selection']=dict(Configuration=s['configuration'],Order=json.dumps(s['order']),Pins=json.dumps(s['pins']),Approvals=json.dumps(s['approvals']))
                    atomic_text(self.root/'request.ini',ini_text(c))
                    profile=self.apply()
                    if profile.name!=active['profile']:error('Fallback identity changed; refusing to substitute resources')
                finally:atomic_text(self.root/'request.ini',pending)
                atomic_text(self.root/'startup.json',json.dumps(dict(error=message,fallback=True),indent=2))
                self.menu_ini(profile/'runtime',set(active['enabled']))
                return profile

def install_native_startup(root,launcher):
    """Trusted deployment only, never called from the package import pipeline."""
    root=Path(root).resolve();launcher=Path(launcher).resolve()
    data=launcher.read_bytes()
    if not data.startswith(b'MZ') or b'NOLFSRC_MOD_MANAGER_BOOTSTRAP_V2' not in data:
        error('Supply a modern NOLF.exe built with native mod startup support')
    dest=safe_member(root,'NOLF.exe')
    if dest.exists() and sha(dest)!=sha(launcher):error('An existing root NOLF.exe differs; use a new isolated installation')
    if not dest.exists():shutil.copy2(launcher,dest)
    # The stock launcher links Miles even when the modern startup path returns
    # before playing its splash audio. Use only the private trusted runtime.
    miles=safe_member(root,'library/runtime/mss32.dll')
    if miles.exists():
        target=safe_member(root,'mss32.dll')
        if target.exists() and sha(target)!=sha(miles):error('Root Miles runtime differs from the trusted library')
        if not target.exists():shutil.copy2(miles,target)
    (root/'mods').mkdir(exist_ok=True)
