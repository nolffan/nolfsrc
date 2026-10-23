"""Data-only package validation, independent of gameplay modules and the UI.

Uses only the Python standard library. Imported code is never loaded.
"""
import hashlib,json,os,re,shutil,stat,struct,uuid,zipfile,zlib
from pathlib import Path
from contextlib import contextmanager

SCHEMA=1
MANAGER_VERSION='2.1.0'
MAX_FILES=4096
MAX_BYTES=256*1024*1024
MAX_MODS=64
ID=re.compile(r'[a-z][a-z0-9-]{0,47}\Z')
ALLOWED={'.abc','.dtx','.wav','.spr','.pcx','.dat','.txt','.ltb','.ogg'}
ROOTS={'GUNS','CHARS','SFX','SOUNDS','SOUND','INTERFACE','MENU','WORLDS','TEX','TEXTURES','SPRITES','GORE','MUSIC','ATTRIBUTES','FX'}
MANAGED={'ATTRIBUTES/WEAPONS.TXT','ATTRIBUTES/MISSIONS.TXT',
         'ATTRIBUTES/MODELBUTES.TXT','ATTRIBUTES/ATTACHMENTS.TXT','ATTRIBUTES/LAYOUT.TXT','ATTRIBUTES/SERVERBUTES.TXT'}
def error(message): raise ValueError(message)

def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def canonical(name):
    if not isinstance(name,str) or not name or len(name)>180: error('Invalid resource path')
    name=name.replace('\\','/')
    parts=name.split('/')
    if any(not p or p in ('.','..') or p[-1] in '. ' or
           re.search(r'[\x00-\x1f\x7f-\uffff:*?"<>|]',p) or
           re.fullmatch(r'(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?',p) for p in parts):
        error('Unsafe resource path: '+name)
    return '/'.join(parts).upper()

def asset_path(name):
    path=canonical(name)
    if path.split('/')[0] not in ROOTS or Path(path).suffix.lower() not in ALLOWED:
        error('Only game data resources are supported: '+path)
    if path in MANAGED: error('This table is managed by the trusted game/profile configuration: '+path)
    return path

def unpack_rez(data):
    """Read bounded data resources, never mount an opaque REZ containing DLLs."""
    if len(data)<139 or b'RezMgr' not in data[:127]: error('Invalid REZ header')
    version,pos,size=struct.unpack_from('<3I',data,127)
    if version!=1: error('Only REZ v1 is supported')
    result={}; seen=set(); total=0; ranges=[]
    def reserve(pos,size):
        if pos<139 or pos+size>len(data): error('Invalid REZ data bounds')
        if size and any(pos<b and a<pos+size for a,b in ranges): error('Overlapping REZ entries')
        if size: ranges.append((pos,pos+size))
    def read_dir(pos,size,prefix,depth):
        nonlocal total
        if depth>24 or (pos,size) in seen or pos<139 or size<0 or pos+size>len(data): error('Invalid REZ directory')
        if len(seen)>=MAX_FILES: error('REZ exceeds directory limits')
        seen.add((pos,size)); reserve(pos,size); off=pos; end=pos+size
        def number(fmt):
            nonlocal off
            n=struct.calcsize(fmt)
            if off+n>end: error('Truncated REZ entry')
            r=struct.unpack_from(fmt,data,off); off+=n; return r
        def string():
            nonlocal off
            z=data.find(b'\0',off,end)
            if z<0: error('Truncated REZ name')
            s=data[off:z].decode('ascii'); off=z+1; return s
        while off<end:
            kind,=number('<I')
            if kind==1:
                p,n,_=number('<3I'); name=canonical(string())
                if '/' in name: error('Invalid REZ directory name')
                read_dir(p,n,prefix+name+'/',depth+1)
            elif kind==0:
                p,n,_,_=number('<4I'); tag,keys=number('<4sI'); name=string(); number('<B')
                if keys>1024 or off+4*keys>end: error('Invalid REZ keys')
                off+=4*keys
                ext=tag.rstrip(b'\0')[::-1].decode('ascii')
                path=asset_path(prefix+name+('.'+ext if ext else ''))
                if path in result or p<139 or p+n>len(data): error('Invalid or duplicate REZ resource')
                total+=n
                if total>MAX_BYTES or len(result)>=MAX_FILES: error('REZ exceeds resource limits')
                reserve(p,n); raw=data[p:p+n]; check_content(raw,path); result[path]=raw
            else: error('Unknown REZ entry type')
    read_dir(pos,size,'',0)
    return result


def check_content(data,name):
    # Extension checks alone must not allow a renamed executable or archive.
    if data.startswith((b'MZ',b'\x7fELF',b'#!',b'PK\x03\x04',b'PK\x05\x06',b'Rar!',b'7z\xbc\xaf\x27\x1c',b'\x1f\x8b')):
        error('Executable, script or nested archive content: '+name)


def unique_json(data):
    def pairs(items):
        result={}
        for key,value in items:
            if key in result: error('Duplicate JSON field: '+key)
            result[key]=value
        return result
    try: return json.loads(data,object_pairs_hook=pairs)
    except (ValueError,UnicodeError) as exc: error('Invalid JSON: '+str(exc))


def version_matches(version,requirement):
    if requirement in ('','*'): return True
    if not isinstance(requirement,str) or len(requirement)>120: error('Invalid version requirement')
    def numeric(value):
        if not re.fullmatch(r'\d+(?:\.\d+){0,3}',value): error('Numeric versions required for ranges: '+value)
        return tuple(int(n) for n in value.split('.'))+(0,)*(4-value.count('.')-1)
    for clause in requirement.split(','):
        m=re.fullmatch(r'\s*(>=|<=|==|!=|>|<|=)?\s*([A-Za-z0-9][A-Za-z0-9.+_-]{0,31})\s*',clause)
        if not m: error('Invalid version requirement: '+requirement)
        op,want=m.groups();op=op or '=='
        if op in ('=','==','!='):
            equal=version==want
            if re.fullmatch(r'\d+(?:\.\d+){0,3}',version) and re.fullmatch(r'\d+(?:\.\d+){0,3}',want): equal=numeric(version)==numeric(want)
            if equal==(op=='!='): return False
        else:
            a,b=numeric(version),numeric(want)
            if not {'>':a>b,'<':a<b,'>=':a>=b,'<=':a<=b}[op]: return False
    return True


def validate_manifest(meta):
    if not isinstance(meta,dict) or type(meta.get('schema')) is not int or meta.get('schema') not in (1,2) or not isinstance(meta.get('id'),str) or not ID.fullmatch(meta['id']): error('Invalid mod manifest id/schema')
    meta=dict(meta)
    allowed={'schema','id','name','author','version','description','category','requires','conflicts','overrides','files','manager_version','game_version','preview','builtin','restore_files','dependency_versions','override_paths'}
    if set(meta)-allowed: error('Unknown manifest fields: '+', '.join(sorted(set(meta)-allowed)))
    for key,limit in [('name',80),('version',32),('author',80),('description',800),('category',32)]:
        value=meta.get(key,'')
        if not isinstance(value,str) or len(value)>limit or any(ord(c)<32 or ord(c)>126 for c in value): error('Invalid manifest '+key)
    if not meta.get('name') or not meta.get('version'): error('Mod name/version are required')
    if not isinstance(meta.get('dependency_versions',{}),dict) or not isinstance(meta.get('override_paths',{}),dict):error('Invalid dependency or override constraints')
    constraints=dict(meta.get('dependency_versions',{})); override_paths=dict(meta.get('override_paths',{}))
    for key in ('requires','conflicts','overrides'):
        values=meta.get(key,[])
        if not isinstance(values,list) or len(values)>MAX_MODS: error('Invalid '+key)
        normalized=[]
        for item in values:
            if isinstance(item,dict) and meta['schema']==2:
                if key=='requires' and set(item)<={'id','version'}:
                    target=item.get('id')
                    if not isinstance(target,str):error('Invalid dependency id')
                    constraints[target]=item.get('version','*')
                elif key=='overrides' and set(item)=={'id','paths'}:
                    target=item['id'];paths=item['paths']
                    if not isinstance(target,str):error('Invalid override id')
                    if not isinstance(paths,list) or not paths or len(paths)>MAX_FILES: error('Invalid override paths')
                    override_paths[target]=[asset_path(p) for p in paths]
                else: error('Invalid '+key+' entry')
            else: target=item
            if not isinstance(target,str) or not ID.fullmatch(target) or target in normalized: error('Invalid '+key)
            normalized.append(target)
        meta[key]=normalized
    if set(constraints)-set(meta['requires']) or set(override_paths)-set(meta['overrides']): error('Unbound dependency or override constraint')
    for constraint in constraints.values(): version_matches('1.0.0',constraint)
    for paths in override_paths.values():
        if not isinstance(paths,list) or not paths or len(paths)>MAX_FILES: error('Invalid override paths')
        for p in paths: asset_path(p)
    if constraints: meta['dependency_versions']=constraints
    if override_paths: meta['override_paths']=override_paths
    for key in ('manager_version','game_version'):
        if key in meta: version_matches('1.0.0',meta[key])
    if 'preview' in meta:
        if not isinstance(meta['preview'],dict) or set(meta['preview'])-{'image','caption'}: error('Invalid preview metadata')
        if 'image' in meta['preview']: asset_path(meta['preview']['image'])
        caption=meta['preview'].get('caption','')
        if not isinstance(caption,str) or len(caption)>240 or any(ord(c)<32 or ord(c)>126 for c in caption): error('Invalid preview caption')
    if 'files' in meta:
        files=meta['files']
        if not isinstance(files,dict) or not files or len(files)>MAX_FILES:error('Invalid resource hash manifest')
        normalized={}
        for path,digest in files.items():
            name=asset_path(path)
            if name in normalized or not isinstance(digest,str) or not re.fullmatch('[a-f0-9]{64}',digest):error('Invalid or duplicate resource hashes')
            normalized[name]=digest
        meta['files']=normalized
    return meta


def inspect_zip(path):
    """Fully validate before returning metadata and a bounded in-memory payload."""
    try: return _inspect_zip(Path(path))
    except (zipfile.BadZipFile,EOFError,UnicodeError,struct.error,NotImplementedError,RuntimeError,zlib.error) as exc:
        error('Corrupt or unsupported archive: '+str(exc))


def inspect_package(path):
    """The same bounded validation for desktop imports and the native inbox."""
    path=Path(path)
    if path.suffix.lower()=='.zip':return inspect_zip(path)
    if path.suffix.lower()!='.rez':error('Use a ZIP or data-only REZ package')
    if path.stat().st_size>MAX_BYTES: error('REZ exceeds size limit')
    try:payload=unpack_rez(path.read_bytes())
    except (EOFError,UnicodeError,struct.error) as exc:error('Invalid REZ: '+str(exc))
    if not payload:error('Empty mod')
    hashes={n:hashlib.sha256(b).hexdigest() for n,b in sorted(payload.items())}
    identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:24]
    name=path.stem.encode('ascii','replace').decode()[:80]
    meta=validate_manifest(dict(schema=1,id='data-'+identity,name=name,version='local',files=hashes))
    meta['builtin']=False
    return meta,payload


def _inspect_zip(path):
    if path.stat().st_size>MAX_BYTES+8*1024**2: error('ZIP exceeds compressed size limit')
    with path.open('rb') as f:
        if f.read(4) not in (b'PK\x03\x04',b'PK\x05\x06'):error('Corrupt or unsupported archive header; self-extracting packages are not supported')
    entries={}; total=0; names=set()
    with zipfile.ZipFile(path) as z:
        if len(z.infolist())>MAX_FILES: error('Too many ZIP entries')
        for item in z.infolist():
            if '\0' in item.orig_filename: error('Unsafe resource path: embedded NUL')
            name=canonical(item.filename.rstrip('/') if item.is_dir() else item.filename)
            if name in names: error('Duplicate or case-colliding ZIP path: '+name)
            names.add(name)
            mode=(item.external_attr>>16)&0xffff
            if stat.S_ISLNK(mode) or (mode and stat.S_IFMT(mode) not in (0,stat.S_IFREG,stat.S_IFDIR)) or item.external_attr&0x400:
                error('ZIP links and special files are not supported')
            if item.flag_bits&1: error('Encrypted ZIPs are not supported')
            if item.compress_type not in (zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED): error('Unsupported ZIP compression')
            if item.is_dir(): continue
            total+=item.file_size
            if total>MAX_BYTES or item.file_size>max(1,item.compress_size)*1000: error('ZIP exceeds extraction limits')
        # All entries are preflighted before any decompression.
        for item in z.infolist():
            if not item.is_dir(): entries[canonical(item.filename)]=z.read(item)
    # Remove one common wrapper at a time, only when it encloses every file.
    # Files beside two independent mod roots are intentionally ambiguous.
    for depth in range(12):
        tops={n.split('/')[0] for n in entries}
        if len(tops)!=1: break
        top=next(iter(tops))
        if top in ROOTS or any('/' not in n for n in entries): break
        entries={n.split('/',1)[1]:b for n,b in entries.items()}
    raw=entries.pop('NOLF-MOD.JSON',None)
    if raw is not None:
        if len(raw)>65536: error('Manifest is too large')
        meta=unique_json(raw)
    else:
        slug=re.sub('[^a-z0-9-]+','-',path.stem.lower()).strip('-')[:40] or 'import'
        meta=dict(schema=1,id='zip-'+slug,name=path.stem[:80],version='local')
    meta=validate_manifest(meta)
    payload={}; expanded=0
    for name,data in entries.items():
        if '/' not in name and name in ('README.TXT','README.MD','LICENSE.TXT'): continue
        if name.startswith('PAYLOAD/'): name=name[8:]
        if name.endswith('.REZ'):
            resources=unpack_rez(data)
        else:
            if name.split('/')[0] not in ROOTS: error('Ambiguous or unsupported package root: '+name+'. Package one mod with game folders at its root.')
            check_content(data,name);resources={asset_path(name):data}
        for dest,data in resources.items():
            if dest in payload: error('Duplicate expanded resource: '+dest)
            expanded+=len(data)
            if len(payload)>=MAX_FILES or expanded>MAX_BYTES: error('Expanded resources exceed limits')
            payload[dest]=data
    if not payload: error('Empty mod')
    for name in payload:
        parts=name.split('/')
        if any('/'.join(parts[:i]) in payload for i in range(1,len(parts))):error('Conflicting file and directory resource paths: '+name)
    hashes={n:hashlib.sha256(b).hexdigest() for n,b in sorted(payload.items())}
    if raw is not None:
        declared=meta.get('files')
        if not isinstance(declared,dict): error('Manifest must declare files and SHA-256 hashes')
        expected={canonical(k):v for k,v in declared.items()}
        if len(expected)!=len(declared) or expected!=hashes: error('Manifest hashes do not match expanded resources')
    if meta.get('preview',{}).get('image') and canonical(meta['preview']['image']) not in payload: error('Preview image is missing')
    meta.update(files=hashes,builtin=False)
    if raw is None:
        # A renamed/repacked manifest-less package remains the same mod.
        meta['id']='data-'+hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:24]
    return meta,payload


def resolve_mods(catalog,enabled,preferred=(),game_version='unknown'):
    enabled=set(enabled)
    if enabled-set(catalog): error('Unknown selected mods: '+', '.join(sorted(enabled-set(catalog))))
    if len(preferred)!=len(set(preferred)) or set(preferred)-enabled: error('Invalid preferred load order')
    order=[];visiting=[]
    def visit(i):
        if i in order:return
        if i in visiting:error('Dependency cycle: '+' -> '.join(visiting+[i]))
        visiting.append(i);m=catalog[i]
        if not version_matches(MANAGER_VERSION,m.get('manager_version','*')): error(i+' requires manager '+m['manager_version'])
        if m.get('game_version') and (game_version=='unknown' or not version_matches(game_version,m['game_version'])): error(i+' requires game '+m['game_version']+'; configured version is '+game_version)
        for d in sorted(m.get('requires',[])):
            if d not in enabled: error(i+' requires '+d+'; enable or install it first')
            if not version_matches(catalog[d].get('version','local'),m.get('dependency_versions',{}).get(d,'*')): error(i+' requires '+d+' '+m['dependency_versions'][d]+'; installed '+catalog[d].get('version','local'))
            visit(d)
        for d in sorted(m.get('overrides',[])):
            if d in enabled:visit(d)
        bad=set(m.get('conflicts',[]))&enabled
        if bad:error('Conflicting mods: '+i+' and '+', '.join(sorted(bad)))
        visiting.pop();order.append(i)
    for i in list(preferred)+sorted(enabled-set(preferred)):visit(i)
    return order


def fingerprint(meta):
    return hashlib.sha256(json.dumps(meta,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def resource_conflicts(catalog,order,owners,approvals=()):
    owners=dict(owners);conflicts=[]
    hashes={i:fingerprint(m) for i,m in catalog.items()}
    keys=('path','winner','loser','winner_hash','loser_hash')
    approved={tuple(a.get(k,'') for k in keys) for a in approvals}
    for i in order:
        m=catalog[i]
        if m.get('builtin'):continue
        for path in sorted(m.get('files',{})):
            previous=owners.get(path)
            if previous and previous!=i and (previous=='core' or previous in order):
                declared=previous in m.get('overrides',[]) and (previous not in m.get('override_paths',{}) or path in m['override_paths'][previous])
                token=dict(path=path,winner=i,loser=previous,winner_hash=hashes[i],loser_hash=hashes.get(previous,'core'))
                if not declared or tuple(token[k] for k in keys) not in approved:conflicts.append(dict(**token,declared=declared))
            owners[path]=i
    return conflicts


def safe_member(root,relative):
    """A private tree must not escape through symlinks, junctions or receipts."""
    root=Path(root).absolute();rel=Path(relative)
    if rel.is_absolute() or any(p in ('..','.') for p in rel.parts) or ':' in str(rel):error('Unsafe stored path: '+str(relative))
    target=root/rel
    p=target
    while True:
        if p.exists() or p.is_symlink():
            s=p.lstat()
            if stat.S_ISLNK(s.st_mode) or getattr(s,'st_file_attributes',0)&0x400:error('Filesystem links are not permitted: '+str(p))
        if p==root:break
        p=p.parent
    if not target.resolve().is_relative_to(root.resolve()):error('Stored path escapes manager')
    return target


def check_space(path,needed):
    free=shutil.disk_usage(path).free
    reserve=16*1024**2
    if free<needed+reserve:error('Insufficient storage: need %.1f MiB, available %.1f MiB. Free space and retry.'%((needed+reserve)/1024**2,free/1024**2))


def verify_library(root,expected):
    root=Path(root);actual=set()
    for path in root.rglob('*'):
        safe_member(root,path.relative_to(root))
        if path.is_file():actual.add(path.relative_to(root).as_posix())
    if actual!=set(expected):error('Bundled library changed: missing or unexpected files')
    for path,digest in expected.items():
        if sha(safe_member(root,path))!=digest:error('Bundled library changed: '+path)


def atomic_text(path,text):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_name(path.name+'.'+uuid.uuid4().hex+'.tmp')
    try:
        with temp.open('w',encoding='utf-8',newline='\n') as f:f.write(text);f.flush();os.fsync(f.fileno())
        os.replace(temp,path)
    finally:
        if temp.exists():temp.unlink()


@contextmanager
def manager_lock(root):
    # OS locks release automatically on crash; the lock file is intentionally retained.
    path=safe_member(root,'operation.lock')
    with path.open('a+b') as f:
        f.seek(0,2)
        if not f.tell():f.write(b'0');f.flush()
        f.seek(0)
        try:
            if os.name=='nt':
                import msvcrt
                msvcrt.locking(f.fileno(),msvcrt.LK_NBLCK,1)
            else:
                import fcntl
                fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except OSError:error('Another manager operation is running; wait for it to finish.')
        try:yield
        finally:
            f.seek(0)
            if os.name=='nt':msvcrt.locking(f.fileno(),msvcrt.LK_UNLCK,1)
            else:fcntl.flock(f,fcntl.LOCK_UN)
