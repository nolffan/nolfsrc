#!/usr/bin/env python3
"""Install the PS2 "Nine Years Ago" levels (Worlds\\thiefmission1-3) into a PC
NOLF game directory: every converted file they need, from the PS2 disc.

  python3 -I build/ps2/ps2install.py --ps2 <ISO | LITH_PS2.REZ> --game <game dir> [--out DIR]
  python3 -I build/ps2/ps2install.py --ps2 ... --game ... --check [-v]

--ps2    the PS2 disc image (ISO9660; LITH_PS2.REZ is read straight out of it)
         or an extracted LITH_PS2.REZ. US and European discs both work (the
         European one: English, directory table 0).
--game   the PC game directory: its nolf*.rez are the retail archives the
         converters measure against (textures, the PC child models of
         hero_thief), and what --check resolves references in.
--out    where the files go (default <game>/custom). Directory names are
         upper case, or the case of a directory already there (Wine sees one
         directory per case-insensitive name). Every file is written to a
         temporary name and renamed over the old one, never rewritten in place
         (game directories are often hard-linked copies).
--check  write nothing; list every file the levels and the attribute patches
         (build/ps2/data/ATTRIBUTES/*.patch) reference and where it resolves
         (custom dir or retail .rez, case-insensitive). An install ends with
         the same check. -v lists the resolved ones too.

Written (counts are for the US disc):
  WORLDS/thiefmission1-3.dat   ps2topc (PS2 lightmaps, scale 1.0), the PS2
                               string IDs remapped (ps2strings), the door sound
                               typo Snd\\Doors\\0wood_c.wav fixed
  WORLDS/m07s03_A, _B.dat      the same (no remap): the PS2's M07S03, with the
                               secret basement; WORLD_FIXES
  TEX/...                      the world textures the PC archives lack
  models                       ps2model (MODELS below)
  skins                        PS2-only .dtx the levels, the patches, the
                               model styles and the code reference (ps2dtx)
  sounds                       the 81 WAVs of sound-map.txt (ps2sound)
  INTERFACE/PHOTOS/...pcx      weapon/ammo icons (32x32 -> 48x48) and the
                               loading-screen photo (the 180x148 card of the
                               PS2's 256x256 texture), 8-bit PCX
"""
import argparse, os, re, struct, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools'))
import ps2rez        # noqa: E402
import ps2world      # noqa: E402
import ps2topc       # noqa: E402
import ps2dtx        # noqa: E402
import ps2model      # noqa: E402
import ps2sound      # noqa: E402
import ps2strings    # noqa: E402
import abcmodel      # noqa: E402
from rezextract import Rez   # noqa: E402

# the "Nine years ago" levels (new: their string IDs are remapped), and the
# PS2's two halves of M07S03 with the secret basement under the Baron's
# courtyard (its strings are the PC's own: no remap)
WORLDS = ['thiefmission1', 'thiefmission2', 'thiefmission3', 'm07s03_A', 'm07s03_B']
REMAP_WORLDS = ('thiefmission1', 'thiefmission2', 'thiefmission3')

# PS2 commands the PC must not run: (world, object, property, PS2 value, PC value).
# The PS2 has no loadout screen and sets the inventory in the level; on the PC
# that would throw away the loadout the player chose. In m07s03_A, clean1
# (after Goodman's coin toss) resets it and gives the barrette and poodle
# back, and Trigger13 (the courtyard shaft, on the way out of the basement)
# resets it again -- taking the basement's weapons, and everything else.
# The PC keeps the player's inventory; the basement's crossbow and rocket
# launcher are one-time weapons of the mission (MISSIONS.txt).
WORLD_FIXES = [
    ('m07s03_A', 'clean1', 'MessageName3',
     'msg player resetinventory;msg Player (AcquireWeapon barrette);msg Player (AcquireWeapon poodle)', ''),
    ('m07s03_A', 'Trigger13', 'MessageName3', 'msg player resetinventory', ''),
]

# (PS2 model, installed name as the game references it, PC child models)
MODELS = [
    ('guns/models_pv/ps2lockpick_pv.ltb', r'Guns\Models_PV\PS2lockpick_pv.abc', []),
    ('guns/models_pv/ps2safecracker_pv.ltb', r'Guns\Models_PV\PS2SafeCracker_pv.abc', []),
    ('guns/models_hh/ps2lockpick_hh.ltb', r'Guns\Models_HH\PS2lockpick_hh.abc', []),
    ('guns/models_hh/ps2safecracker_hh.ltb', r'Guns\Models_HH\PS2SafeCracker_hh.abc', []),
    ('interface/models/hud/moneybag.ltb', r'Interface\Models\MoneyBag.abc', []),
    ('interface/models/hud/purses_01.ltb', r'Interface\Models\Purses_01.abc', []),
    ('interface/models/hud/change_01.ltb', r'Interface\Models\Change_01.abc', []),
    ('props/models/moneybag.ltb', r'Props\Models\MoneyBag.abc', []),
    ('attachments/models/britishpolicehat.ltb', r'Attachments\Models\BritishPoliceHat.abc', []),
    # ModelButes: chars\models\<model>_<style>.abc
    ('chars/models/hero_thief.ltb', r'Chars\Models\Hero_Thief.abc',
     ['basemodel.abc', 'fembase.abc', 'playerbase.abc']),
]

# The PS2 colours some parts of a model, with a glow on the safe cracker's
# LEDs; the PC model cannot, so those models get a skin of their own with the
# colours in it (ps2model.bake_colours): model -> (PS2 skin, installed skin).
# Only the first-person safe cracker has coloured parts; its in-hand model
# keeps the plain skin.
BAKED_SKINS = {
    r'Guns\Models_PV\PS2SafeCracker_pv.abc':
        ('guns/skins_pv/ps2safecracker_pv.dtx', r'Guns\Skins_PV\PS2SafeCracker_pv_leds.dtx'),
}

# the PS2 executable in the disc image's root (SLUS_200.28 on the US disc)
PS2_EXE = re.compile(r'S[CL][EU]S_\d{3}\.\d{2}')
PS2_ATTRIBUTES = ['AIButes', 'AnimationsHuman', 'Attachments', 'ClientButes', 'ClientSnd',
                  'DamageFX', 'Debris', 'Fx', 'IntelItems', 'Layout', 'Missions', 'ModelButes',
                  'PropTypes', 'ServerButes', 'ServerSnd', 'SoundFilters', 'Surface', 'Weapons']


def model_names(ps2, pc):
    """The PS2 models' name dictionary (ps2model.collect_names), from the
    discs: the PC models' names, the PS2 executable's and attribute files'."""
    t = time.time()
    abcs = (pc.get(k) for k in pc.idx if k.endswith('.abc'))    # archive order: first spelling wins
    texts = []
    try:
        loc = iso_find(ps2.path, PS2_EXE)
    except SystemExit:
        loc = None
    if loc:
        with open(ps2.path, 'rb') as fh:
            fh.seek(loc[0])
            texts.append(fh.read(loc[1]))
    for n in PS2_ATTRIBUTES:
        d = ps2.get('attributes/%s.txt' % n)
        if d:
            texts.append(d)
    names = ps2model.Names(names=ps2model.collect_names(abcs, texts))
    print('  %d names for the models\' hashes (%s)  [%.1fs]' % (
        len(names.names), 'PC models, PS2 executable and attributes' if loc
        else 'PC models and PS2 attributes: not an ISO, no executable', time.time() - t))
    return names


# M-P8-10: the main menu's young Cate, the model ModelButes names for the
# menu model IntHero in the PS2 style Thief: Hero_Thief's mesh (the same 25
# nodes as the menu model) on the retail IntHero_Action's one animation
# (interface4) -- as the menu's casual and winter Cates are the in-game
# outfits on the menu skeleton (MODERN.md M-P3-11). Her skins are
# Hero_Thief's, under the menu model's names.
YOUNG_CATE = (r'Chars\Models\Hero_Thief.abc', r'Chars\Models\IntHero_Thief.abc',
              r'Chars\Models\IntHero_Action.abc')
YOUNG_CATE_SKINS = [('chars/skins/hero_thief.dtx', r'Chars\Skins\IntHero_Thief.dtx'),
                    ('chars/skins/hero_thief_head.dtx', r'Chars\Skins\IntHero_Thief_head.dtx')]


def menu_cate(thief, pc):
    data = pc.get(YOUNG_CATE[2])
    if data is None:
        raise SystemExit('%s not in the retail archives' % YOUNG_CATE[2])
    menu = abcmodel.read_abc(data)
    if [n.name.lower() for n in menu.nodes[1:]] != [n.name.lower() for n in thief.nodes[1:]]:
        raise SystemExit('%s and %s have different skeletons' % (YOUNG_CATE[0], YOUNG_CATE[2]))
    thief.children = thief.children[:1]      # no animation models: its own one
    thief.child_bindings = []
    thief.anims, thief.bindings = menu.anims, menu.bindings
    thief.command = menu.command
    return abcmodel.write_abc(thief)


# referenced by code, not by data: the PS2 thief hands (ModelButeMgr::
# GetHandsSkinFilename for style "Thief"), the loading-screen photo
CODE_REFS = [r'Guns\Skins_PV\ThiefHands_pv.dtx', r'Interface\Photos\Missions\thief.pcx']

# the PS2 Attachments.txt police hats (Head attachments of the thief levels'
# police): the PC ATTACHMENTS.txt needs these entries pointing at these files
ATTACHMENT_FILES = [r'Attachments\Models\BritishPoliceHat.abc',
                    r'Attachments\Skins\MaleBystander_Action_BritHat.dtx',
                    r'Attachments\Skins\MaleBystander_Action_ScotHat.dtx']

# PS2 .dtx -> PCX: (PS2 name, installed name, crop box or None, size)
PHOTOS = [
    ('interface/photos/weapons/ps2sc.dtx', r'Interface\Photos\Weapons\PS2SC.pcx', None, (48, 48)),
    ('interface/photos/weapons/ps2lp.dtx', r'Interface\Photos\Weapons\PS2lp.pcx', None, (48, 48)),
    ('interface/photos/ammo/func_cracker.dtx', r'Interface\Photos\Ammo\Func_Cracker.pcx', None, (48, 48)),
    # and their 24x24 small icons, as the retail ones (_SM)
    ('interface/photos/weapons/ps2sc.dtx', r'Interface\Photos\Weapons\PS2SC_SM.pcx', None, (24, 24)),
    ('interface/photos/weapons/ps2lp.dtx', r'Interface\Photos\Weapons\PS2lp_SM.pcx', None, (24, 24)),
    ('interface/photos/ammo/func_cracker.dtx', r'Interface\Photos\Ammo\Func_Cracker_SM.pcx', None, (24, 24)),
    # the PS2 mission photos are the PC's 180x148 card inside a 256x256 texture
    ('interface/photos/missions/thief.dtx', r'Interface\Photos\Missions\thief.pcx', (38, 54, 180, 148), (180, 148)),
]

# references that stay unresolved, and why
# references that are missing from the retail PC game too, as it ships
# (so the check does not count them)
KNOWN = {
    r'guns\snd\safe\deselect.wav':
        'on no PS2 bank either (the PS2 Weapons.txt names it, the PS2 plays nothing)',
    r'chars\skins\dog_action_head.dtx':
        'no dog has a head skin; the retail M02S01, M07S02, T06S01 dogs ask for it too',
    r'tex\sky\snow_dn.dtx':
        'the retail M07S03 has it in its texture list too, unused',
}
KNOWN.update((r'voice\%d.wav' % i,
              "an interview question with no voice, as in the retail M07S03") for i in range(9002, 9010))

DOOR_TYPO = (r'Snd\Doors\0wood_c.wav', r'Snd\Doors\01wood_c.wav')

FILE_RE = re.compile(r'[\w\\/&.+-]+\.(?:abc|dtx|wav|pcx|spr)\b', re.I)
ATTACH_PROPS = ('LeftHand', 'RightHand', 'Head', 'Eyes', 'Nose', 'Chin', 'Back',
                'LeftFoot', 'RightFoot', 'Motorcycle', 'Snowmobile')


def key(p):
    return p.replace('/', '\\').lower()


# ------------------------------------------------------------------ the PS2 archive

class SubFile:
    """A byte range of a file as a file of its own (LITH_PS2.REZ inside the ISO)."""
    def __init__(self, path, base, size):
        self.f, self.base, self.size, self.p = open(path, 'rb'), base, size, 0

    def seek(self, p, whence=0):
        self.p = p if whence == 0 else (self.p + p if whence == 1 else self.size + p)
        return self.p

    def tell(self):
        return self.p

    def read(self, n=-1):
        n = self.size - self.p if n is None or n < 0 else min(n, self.size - self.p)
        self.f.seek(self.base + self.p)
        d = self.f.read(n)
        self.p += len(d)
        return d

    def close(self):
        self.f.close()


def iso_find(path, want='LITH_PS2.REZ'):
    """(offset, size) of a root-directory file of an ISO9660 image (want: its
    name, or a compiled pattern its whole name matches), or None if the file
    is not an ISO image."""
    with open(path, 'rb') as f:
        f.seek(16 * 2048)
        pvd = f.read(2048)
        if pvd[1:6] != b'CD001':
            return None
        bs = struct.unpack_from('<H', pvd, 128)[0]
        root = pvd[156:190]
        ext, size = struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0]
        f.seek(ext * bs)
        d = f.read(size)
    p = 0
    while p < len(d):
        ln = d[p]
        if ln == 0:
            p = (p // bs + 1) * bs
            continue
        e, s, nl = struct.unpack_from('<I', d, p + 2)[0], struct.unpack_from('<I', d, p + 10)[0], d[p + 32]
        name = d[p + 33:p + 33 + nl].decode('latin-1').split(';')[0]
        if (want.fullmatch(name.upper()) if hasattr(want, 'fullmatch') else name.upper() == want):
            return e * bs, s
        p += ln
    raise SystemExit('%s: an ISO9660 image without %s in its root' % (path, want))


_ISO = {}


def open_rez_any(path):
    """ps2rez.open_rez for a LITH_PS2.REZ or an ISO holding one."""
    if path not in _ISO:
        _ISO[path] = iso_find(path)
    loc = _ISO[path]
    f = open(path, 'rb') if loc is None else SubFile(path, *loc)
    unk, n, magic, data_off, ntab = struct.unpack('<5I', f.read(20))
    tables = []
    for t in range(ntab):
        f.seek(ps2rez.TABLE_START + t * ps2rez.TABLE_STRIDE)
        raw = f.read(n * ps2rez.ENTRY)
        tables.append([struct.unpack_from('<4I', raw, i * ps2rez.ENTRY) for i in range(n)])
    return f, dict(unk=unk, n=n, magic=magic, data_off=data_off, ntab=ntab), tables


ps2rez.open_rez = open_rez_any      # the converters' own reads go through it too


class PS2:
    def __init__(self, path):
        self.path = path
        self.f, self.hdr, self.tables = open_rez_any(path)
        if self.hdr['unk'] != 3 or not 0 < self.hdr['ntab'] < 4096:
            raise SystemExit('%s: not a LITH_PS2.REZ' % path)
        self.t0 = {e[0]: e for e in self.tables[0] if e[2]}
        self.rest = None

    def get(self, name):
        """An archive entry by name: directory table 0 (English, and nearly
        all the data), else the first other table that has it."""
        h = ps2rez.rez_hash(name, self.hdr['magic'])
        e = self.t0.get(h)
        if not e:
            if self.rest is None:
                self.rest = {}
                for tab in self.tables[1:]:
                    for x in tab:
                        if x[2]:
                            self.rest.setdefault(x[0], x)
            e = self.rest.get(h)
        if not e:
            return None
        self.f.seek(e[1] * 2048)
        return self.f.read(e[2])


# ------------------------------------------------------------------ the PC side

class PC:
    """The retail archives: <game>/nolf*.rez."""
    def __init__(self, game):
        self.rez = sorted(f for f in os.listdir(game) if re.match(r'(?i)nolf.*\.rez$', f))
        if not self.rez:
            raise SystemExit('%s: no nolf*.rez' % game)
        self.idx = {}
        for f in self.rez:
            path = os.path.join(game, f)
            for p, pos, size in Rez(path).walk():
                self.idx.setdefault(key(p), (path, pos, size))

    def has(self, name):
        return key(name) in self.idx

    def get(self, name):
        e = self.idx.get(key(name))
        if not e:
            return None
        with open(e[0], 'rb') as fh:
            fh.seek(e[1])
            return fh.read(e[2])


class Out:
    def __init__(self, root, dry=False):
        self.root, self.dry = root, dry
        self.counts = {}
        self.files = []

    def resolve(self, rel, create=True):
        """rel (game path, \\ or /) -> a path under root; each directory in
        the case of one already there, else upper case."""
        parts = re.split(r'[\\/]+', rel)
        d = self.root
        for part in parts[:-1]:
            hit = None
            if os.path.isdir(d):
                hit = next((x for x in os.listdir(d) if x.lower() == part.lower()
                            and os.path.isdir(os.path.join(d, x))), None)
            d = os.path.join(d, hit or part.upper())
            if create and not hit:
                os.makedirs(d, exist_ok=True)
        base = parts[-1]
        if os.path.isdir(d):
            base = next((x for x in os.listdir(d) if x.lower() == base.lower()), base)
        return os.path.join(d, base)

    def put(self, rel, data, kind):
        dst = self.resolve(rel)
        tmp = dst + '.ps2install.tmp'
        with open(tmp, 'wb') as fh:
            fh.write(data)
        os.replace(tmp, dst)      # replaces the name, never the old file's data
        self.counts[kind] = self.counts.get(kind, 0) + 1
        self.files.append(dst)
        return dst


# ------------------------------------------------------------------ images

def bilinear(w, h, rgb, nw, nh):
    out = bytearray(nw * nh * 3)
    for y in range(nh):
        sy = min(max((y + 0.5) * h / nh - 0.5, 0), h - 1)
        y0 = int(sy); y1 = min(y0 + 1, h - 1); fy = sy - y0
        for x in range(nw):
            sx = min(max((x + 0.5) * w / nw - 0.5, 0), w - 1)
            x0 = int(sx); x1 = min(x0 + 1, w - 1); fx = sx - x0
            for c in range(3):
                a = rgb[3 * (y0 * w + x0) + c] * (1 - fx) + rgb[3 * (y0 * w + x1) + c] * fx
                b = rgb[3 * (y1 * w + x0) + c] * (1 - fx) + rgb[3 * (y1 * w + x1) + c] * fx
                out[3 * (y * nw + x) + c] = int(a * (1 - fy) + b * fy + 0.5)
    return bytes(out)


def quantise(rgb, n=256):
    """(palette [(r,g,b)], indices) -- the exact colours if there are at most
    n, else a median cut."""
    px = [tuple(rgb[i:i + 3]) for i in range(0, len(rgb), 3)]
    counts = {}
    for c in px:
        counts[c] = counts.get(c, 0) + 1
    cols = list(counts)
    if len(cols) <= n:
        pal = sorted(cols)
    else:
        boxes = [cols]
        while len(boxes) < n:
            # split the box with the widest channel range (weighted median)
            best = None
            for i, b in enumerate(boxes):
                if len(b) < 2:
                    continue
                rng = [max(c[k] for c in b) - min(c[k] for c in b) for k in range(3)]
                k = rng.index(max(rng))
                if best is None or rng[k] > best[0]:
                    best = (rng[k], i, k)
            if best is None:
                break
            _, i, k = best
            b = sorted(boxes.pop(i), key=lambda c: c[k])
            tot, acc, cut = sum(counts[c] for c in b), 0, 1
            for j, c in enumerate(b):
                acc += counts[c]
                if acc * 2 >= tot:
                    cut = min(max(j + 1, 1), len(b) - 1)
                    break
            boxes += [b[:cut], b[cut:]]
        pal = []
        for b in boxes:
            t = sum(counts[c] for c in b)
            pal.append(tuple(int(sum(c[k] * counts[c] for c in b) / t + 0.5) for k in range(3)))
    lut = {}
    for c in cols:
        lut[c] = min(range(len(pal)), key=lambda i: sum((c[k] - pal[i][k]) ** 2 for k in range(3)))
    return pal, bytes(lut[c] for c in px)


def pcx(w, h, pal, idx):
    """8-bit, one plane, RLE, 256-colour palette at the end (as the retail ones)."""
    bpl = w + (w & 1)
    hdr = bytearray(128)
    struct.pack_into('<4B6H', hdr, 0, 10, 5, 1, 8, 0, 0, w - 1, h - 1, 72, 72)
    hdr[65] = 1
    struct.pack_into('<HH', hdr, 66, bpl, 1)
    out = bytearray(hdr)
    for y in range(h):
        row = idx[y * w:(y + 1) * w] + bytes(bpl - w)
        i = 0
        while i < bpl:
            v, n = row[i], 1
            while i + n < bpl and n < 63 and row[i + n] == v:
                n += 1
            if n > 1 or v >= 0xC0:
                out += bytes((0xC0 | n, v))
            else:
                out.append(v)
            i += n
    out.append(12)
    for i in range(256):
        out += bytes(pal[i]) if i < len(pal) else b'\0\0\0'
    return bytes(out)


def photo_pcx(data, crop, size):
    w, h, mips, _ = ps2dtx.ps2_read(data)
    rgba = mips[0][2]
    x0, y0, cw, ch = crop or (0, 0, w, h)
    rgb = bytearray()
    for y in range(y0, y0 + ch):
        for x in range(x0, x0 + cw):
            r, g, b, a = rgba[4 * (y * w + x):4 * (y * w + x) + 4]
            rgb += bytes((r * a // 255, g * a // 255, b * a // 255))      # over black
    if (cw, ch) != size:
        rgb = bilinear(cw, ch, rgb, *size)
    pal, idx = quantise(bytes(rgb))
    return pcx(size[0], size[1], pal, idx)


# ------------------------------------------------------------------ worlds

def fix_world(w, remap, name):
    n = ps2strings.remap_objects(w.objects, remap) if name in REMAP_WORLDS else 0
    for world, obj, prop, old, new in WORLD_FIXES:
        if world != name:
            continue
        hits = [p for o in w.objects
                if any(q.name == 'Name' and str(q.value).lower() == obj.lower() for q in o.props)
                for p in o.props if p.name == prop]
        if len(hits) != 1 or hits[0].value != old:
            raise SystemExit('%s: %s.%s is not %r' % (name, obj, prop, old))
        hits[0].value = new
    typo = 0
    for o in w.objects:
        for p in o.props:
            if isinstance(p.value, str) and DOOR_TYPO[0].lower() in p.value.lower():
                p.value = re.sub(re.escape(DOOR_TYPO[0]), DOOR_TYPO[1].replace('\\', '\\\\'), p.value, flags=re.I)
                typo += 1
    return n, typo


def install_world(ps2, game, out, name, remap):
    src = ps2.get('worlds/%s.ltb' % name)
    if src is None:
        raise SystemExit('worlds/%s.ltb not in the PS2 archive' % name)
    world = ps2world.load(src)
    nids, typo = fix_world(world, remap, name)
    dst = out.resolve(r'Worlds\%s.dat' % name)
    tmp = dst + '.ps2install.tmp'
    load = ps2topc.P.load
    ps2topc.P.load = lambda s: world
    try:
        _, stats, missing, size = ps2topc.convert(src, tmp, game, ps2.path, scale=1.0, lightmaps=True)
    finally:
        ps2topc.P.load = load
    os.replace(tmp, dst)
    out.counts['worlds'] = out.counts.get('worlds', 0) + 1
    out.files.append(dst)
    print('  %-14s %8d bytes, %d BSPs, %d objects, %d string IDs remapped, %d door-sound fix; '
          'lightmaps %d polys; %d textures not in the PC archives'
          % (name, size, len(world.models), len(world.objects), nids, typo,
             stats['lm_polys'], len(missing)))
    return missing


# ------------------------------------------------------------------ references

class Refs:
    def __init__(self):
        self.files = {}          # key -> (spelling, [where])
        self.styles = {}         # style name -> [where]
        self.attach = {}         # attachment name -> [where]

    def add(self, name, where):
        name = name.strip()
        if not name:
            return
        k = key(name)
        if k not in self.files:
            self.files[k] = (name, [])
        if where not in self.files[k][1]:
            self.files[k][1].append(where)


def pc_world_objects(data):
    """[(class, [(name, value)])] of a PC world .dat (string props only)."""
    obj_ofs = struct.unpack_from('<I', data, 4)[0]
    p = obj_ofs
    n = struct.unpack_from('<I', data, p)[0]
    p += 4

    def lstr(p):
        ln = struct.unpack_from('<H', data, p)[0]
        return data[p + 2:p + 2 + ln].decode('latin-1'), p + 2 + ln
    objs = []
    for _ in range(n):
        ln = struct.unpack_from('<H', data, p)[0]
        end = p + 2 + ln
        cls, q = lstr(p + 2)
        npr = struct.unpack_from('<I', data, q)[0]
        q += 4
        props = []
        for _ in range(npr):
            pn, q = lstr(q)
            t = data[q]
            dl = struct.unpack_from('<H', data, q + 5)[0]
            q += 7
            if t == 0:
                props.append((pn, lstr(q)[0]))
            q += dl
        objs.append((cls, props))
        p = end
    return objs


def world_refs(refs, name, objs):
    for cls, props in objs:
        d = dict(props)
        where = '%s %s %s' % (name, cls, d.get('Name', ''))
        for pn, v in props:
            for m in FILE_RE.findall(v):
                refs.add(m, where + '.' + pn)
            if re.match(r'Dialogue\d+$', pn):
                for t in re.split(r'[,;\s]+', v.strip()):
                    if t.isdigit():
                        refs.add('Voice\\%s.wav' % t, where + '.' + pn)
        m = re.match(r'AI_(?:BAD|GOOD|NEUTRAL)_(\w+)$', cls)
        if m:
            model, style = m.group(1), d.get('ModelStyle') or 'Action'
            refs.styles.setdefault(style, []).append(where)
            refs.add(r'Chars\Models\%s_%s%s.abc' % (model, style, d.get('CinematicExtension', '')), where + ' (ModelButes)')
            refs.add(r'Chars\Skins\%s_%s%s.dtx' % (model, style, d.get('BodySkinExtension', '')), where + ' (ModelButes)')
            refs.add(r'Chars\Skins\%s_%s%s.dtx' % (model, style, d.get('HeadExtension') or '_head'), where + ' (ModelButes)')
            for a in ATTACH_PROPS:
                v = d.get(a, '')
                if v and v.lower() not in ('<none>', 'none'):
                    refs.attach.setdefault(v, []).append(where + '.' + a)


def patch_refs(refs):
    d = os.path.join(HERE, 'data', 'ATTRIBUTES')
    added = {}
    if not os.path.isdir(d):
        return added
    for f in sorted(os.listdir(d)):
        if not f.endswith('.patch'):
            continue
        txt = []
        for n, line in enumerate(open(os.path.join(d, f), encoding='latin-1'), 1):
            if line.startswith('+') and not line.startswith('+++'):
                txt.append(line[1:])
                for m in re.findall(r'"([^"]+)"', line):
                    for fm in FILE_RE.findall(m):
                        refs.add(fm, '%s:%d' % (f, n))
        added[f[:-len('.txt.patch')].upper() if f.endswith('.txt.patch') else f.upper()] = ''.join(txt)
    return added


def attr_text(name, out_root, pc, added):
    """The attribute file as the game will read it (custom/ATTRIBUTES/<name>.txt
    or the retail one) plus the lines our patches add."""
    p = Out(out_root).resolve(r'Attributes\%s.txt' % name, create=False)
    if os.path.isfile(p):
        t = open(p, encoding='latin-1').read()
    else:
        t = (pc.get(r'Attributes\%s.txt' % name) or b'').decode('latin-1')
    return t, added.get(name.upper(), '')


def custom_index(root):
    idx = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            rel = os.path.relpath(os.path.join(d, f), root)
            idx[key(rel)] = rel
    return idx


def check(ps2, pc, out_root, verbose=False):
    refs = Refs()
    for name in WORLDS:
        p = Out(out_root).resolve(r'Worlds\%s.dat' % name, create=False)
        if not os.path.isfile(p):
            refs.add(r'Worlds\%s.dat' % name, 'MISSIONS.txt.patch')
            print('  (%s.dat not installed: its objects are not checked)' % name)
            continue
        refs.add(r'Worlds\%s.dat' % name, 'MISSIONS.txt.patch')
        world_refs(refs, name, pc_world_objects(open(p, 'rb').read()))
        w = ps2world.load(ps2.get('worlds/%s.ltb' % name))
        for t in w.textures:
            t = ps2topc.strip(t)
            if t:
                refs.add(t, '%s texture' % name)
    added = patch_refs(refs)
    for r in CODE_REFS:
        refs.add(r, 'code (PS2 executable)')
    for r in ATTACHMENT_FILES:
        refs.add(r, 'PS2 Attachments.txt (police hats)')
    for _, rel, _ in MODELS:
        refs.add(rel, 'installed model')
    cust = custom_index(out_root)
    missing, n_c, n_r = [], 0, 0
    for k in sorted(refs.files):
        spell, where = refs.files[k]
        if k in cust:
            st, n_c = 'custom', n_c + 1
        elif pc.has(k):
            st, n_r = 'retail', n_r + 1
        else:
            st = 'MISSING'
            if k not in KNOWN:
                missing.append(k)
        if st == 'MISSING' or verbose:
            print('  %-7s %-52s %s%s%s' % (st, spell, '; '.join(where[:2]),
                                          ' (+%d)' % (len(where) - 2) if len(where) > 2 else '',
                                          ' -- ' + KNOWN[k] if k in KNOWN and st == 'MISSING' else ''))
    print('  files: %d references, %d in custom, %d in the retail archives, %d MISSING'
          % (len(refs.files), n_c, n_r, len(missing)))
    # names that must exist in attribute files
    mb, mb_add = attr_text('ModelButes', out_root, pc, added)
    styles = set(m.lower() for m in re.findall(r'(?m)^\s*Name\s*=\s*"([^"]*)"', mb))
    styles_add = set(m.lower() for m in re.findall(r'(?m)^\s*Name\s*=\s*"([^"]*)"', mb_add))
    at, at_add = attr_text('Attachments', out_root, pc, added)
    atts = set(m.lower() for m in re.findall(r'(?m)^\s*Name\s*=\s*"([^"]*)"', at))
    atts_add = set(m.lower() for m in re.findall(r'(?m)^\s*Name\s*=\s*"([^"]*)"', at_add))
    bad = 0
    for kind, used, have, have_add in (('ModelStyle', refs.styles, styles, styles_add),
                                       ('attachment', refs.attach, atts, atts_add)):
        for n in sorted(used):
            st = 'ok' if n.lower() in have else 'patch' if n.lower() in have_add else 'MISSING'
            if st == 'MISSING':
                bad += 1
            if st != 'ok' or verbose:
                print('  %-7s %s "%s" (%d uses, e.g. %s)%s' % (
                    st, kind, n, len(used[n]), used[n][0],
                    ' -- in a build/ps2/data/ATTRIBUTES patch, not yet in the game dir' if st == 'patch' else ''))
    print('  names: %d model styles, %d attachments used; %d MISSING'
          % (len(refs.styles), len(refs.attach), bad))
    return refs, missing, bad


# ------------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--ps2', required=True)
    ap.add_argument('--game', required=True)
    ap.add_argument('--out')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('-v', action='store_true')
    a = ap.parse_args()
    out_root = a.out or os.path.join(a.game, 'custom')
    if not a.out:
        hit = next((x for x in os.listdir(a.game) if x.lower() == 'custom'), None)
        out_root = os.path.join(a.game, hit or 'custom')
    T = time.time()
    t = time.time()
    ps2 = PS2(a.ps2)
    pc = PC(a.game)
    print('PS2 archive %s%s; retail %s (%d files)  [%.1fs]' % (
        a.ps2, ' (in the ISO image)' if _ISO.get(a.ps2) else '', ', '.join(pc.rez), len(pc.idx), time.time() - t))
    if a.check:
        print('check:')
        _, missing, bad = check(ps2, pc, out_root, a.v)
        sys.exit(1 if missing or bad else 0)

    os.makedirs(out_root, exist_ok=True)
    out = Out(out_root)

    # worlds and their textures
    t = time.time()
    print('worlds:')
    remap = ps2strings.load_remap()
    tex = set()
    for w in WORLDS:
        tex.update(install_world(ps2, a.game, out, w, remap))
    for n in sorted(tex):
        data = ps2.get(n)
        if not data:
            print('  texture not on the PS2 either:', n)
            continue
        out.put(n, ps2dtx.ps2_to_pc(data), 'world textures')
    print('  %d worlds, %d textures  [%.1fs]' % (out.counts.get('worlds', 0), out.counts.get('world textures', 0), time.time() - t))

    # models
    t = time.time()
    print('models:')
    names = model_names(ps2, pc)
    for src, rel, kids in MODELS:
        data = ps2.get(src)
        if data is None:
            raise SystemExit('%s not in the PS2 archive' % src)
        m = ps2model.PS2Model(data, names)
        children = []
        for k in kids:
            cd = pc.get(rel.rsplit('\\', 1)[0] + '\\' + k)
            if cd is None:
                raise SystemExit('PC child model %s not in the retail archives' % k)
            children.append((k, cd))
        conv = ps2model.convert(m, children)
        if rel in BAKED_SKINS:
            skin_src, skin_rel = BAKED_SKINS[rel]
            sd = ps2.get(skin_src)
            if sd is None:
                raise SystemExit('%s not in the PS2 archive' % skin_src)
            w, h, mips, hdr = ps2dtx.ps2_read(sd)
            baked = ps2model.bake_colours(m, conv, mips[0][2], w, h)
            if baked:
                out.put(skin_rel, ps2dtx.write_pc32(hdr, *baked), 'skins')
                print('  %-44s %dx%d (its parts\' colours: LEDs, buttons)' % (skin_rel, baked[0], baked[1]))
        abc = abcmodel.write_abc(conv)
        out.put(rel, abc, 'models')
        print('  %-44s %7d bytes%s' % (rel, len(abc), ' children ' + ','.join(kids) if kids else ''))
        if rel == YOUNG_CATE[0]:
            menu = menu_cate(conv, pc)
            out.put(YOUNG_CATE[1], menu, 'models')
            print('  %-44s %7d bytes (the menu\'s young Cate)' % (YOUNG_CATE[1], len(menu)))
    print('  %d models  [%.1fs]' % (out.counts.get('models', 0), time.time() - t))

    # skins and other .dtx: whatever is referenced, missing from the retail
    # archives and on the PS2 disc
    t = time.time()
    print('skins:')
    refs, missing, _ = Refs(), None, None
    for name in WORLDS:
        world_refs(refs, name, pc_world_objects(open(out.resolve(r'Worlds\%s.dat' % name), 'rb').read()))
    patch_refs(refs)
    for r in CODE_REFS + ATTACHMENT_FILES:
        refs.add(r, 'code')
    for k in sorted(refs.files):
        spell = refs.files[k][0]
        if not k.endswith('.dtx') or pc.has(k):
            continue
        data = ps2.get(k)
        if data is None:
            continue
        out.put(spell, ps2dtx.ps2_to_pc(data), 'skins')
        print('  ' + spell)
    for src, rel in YOUNG_CATE_SKINS:
        data = ps2.get(src)
        if data is None:
            raise SystemExit('%s not in the PS2 archive' % src)
        out.put(rel, ps2dtx.ps2_to_pc(data), 'skins')
        print('  ' + rel)
    print('  %d skins  [%.1fs]' % (out.counts.get('skins', 0), time.time() - t))

    # sounds
    t = time.time()
    banks = {}
    for line in open(os.path.join(HERE, 'sound-map.txt')):
        m = re.match(r'(\S+) <- (\S+) #(\d+) \((\d+) Hz', line)
        if not m:
            continue
        dst, bank, i, rate = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
        if bank not in banks:
            bd = ps2.get(bank)
            if bd is None:
                raise SystemExit('sound bank %s not in the PS2 archive' % bank)
            banks[bank] = ps2sound.load(bd)
        s = banks[bank].sounds[i]
        if s.rate != rate:
            raise SystemExit('%s: %s #%d is %d Hz, the map says %d' % (dst, bank, i, s.rate, rate))
        pcm = ps2sound.decode(s.adpcm)
        raw = struct.pack('<%dh' % len(pcm), *pcm)
        wav = (b'RIFF' + struct.pack('<I', 36 + len(raw)) + b'WAVE' +
               b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16) +
               b'data' + struct.pack('<I', len(raw)) + raw)
        out.put(dst, wav, 'sounds')
    print('sounds: %d WAVs from %d banks  [%.1fs]' % (out.counts.get('sounds', 0), len(banks), time.time() - t))

    # icons and the loading-screen photo
    t = time.time()
    print('photos:')
    for src, rel, crop, size in PHOTOS:
        data = ps2.get(src)
        if data is None:
            raise SystemExit('%s not in the PS2 archive' % src)
        out.put(rel, photo_pcx(data, crop, size), 'photos')
        print('  %-44s %dx%d' % (rel, size[0], size[1]))
    print('  %d PCX  [%.1fs]' % (out.counts.get('photos', 0), time.time() - t))

    # the list of what came from the PS2 disc (build/package.sh: a game
    # directory with it gets no public archive)
    lst = os.path.join(out_root, 'PS2-DATA.txt')
    with open(lst + '.tmp', 'w') as fh:
        fh.write(''.join(os.path.relpath(f, out_root).replace(os.sep, '/') + '\n' for f in sorted(set(out.files))))
    os.replace(lst + '.tmp', lst)
    print('wrote %d files into %s: %s  [%.1fs]' % (
        len(out.files), out_root, ', '.join('%d %s' % (v, k) for k, v in out.counts.items()), time.time() - T))
    print('check:')
    _, missing, bad = check(ps2, pc, out_root, a.v)
    print('total %.1fs' % (time.time() - T))
    sys.exit(1 if missing or bad else 0)


if __name__ == '__main__':
    main()
