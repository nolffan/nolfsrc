#!/usr/bin/env python3
"""Convert a PS2 NOLF model (.ltb, file type 2, version 16) into a PC NOLF model
(.abc, version 12). Layouts: spec-models.md.

  ps2model.py conv  IN.ltb OUT.abc [--child NAME.abc ...] [--names FILE]
  ps2model.py dump  IN.ltb [--names FILE]          what the PS2 file holds
  ps2model.py names OUT.txt [--pc-rez DIR] [--ps2-dir DIR]
                                                   build the name dictionary
  ps2model.py thief OUTDIR [--names FILE]          every model the thief levels
                                                   need, PS2 archive -> OUTDIR

PS2 files keep piece, animation, socket and weight-set names only as hashes
(ps2rez's hash with the model's own multiplier); they are looked up in a name
dictionary (collect_names: every PC .abc name, the PS2 executable's strings
and the PS2 attribute files; build/ps2/ps2install.py collects it from the
discs, and the research default, out/tmp/ps2-research/ps2model_names.txt,
is made by `names`). It is game data, so it is never committed. A hash
with no match becomes "h<hex>".
"""
import math, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import abcmodel as A          # noqa: E402
from ps2rez import rez_hash   # noqa: E402

MT_RIGID, MT_SKEL, MT_VERTANIM = 4, 5, 6


# CWeaponModel's animation names (WeaponModel.cpp: Select, Deselect, Reload,
# Idle_N, FireN, AltIdle_N, AltFireN), the weapon-ani overrides' (Select1:
# WEAPONS.txt BarretteLockAnis), and KeyPad's decipherer "Animation".
PRIORITY = (['Select', 'Deselect', 'Reload', 'Fire', 'AltFire', 'Animation', 'Select1', 'Dec1'] +
            ['Idle_%d' % i for i in range(10)] + ['AltIdle_%d' % i for i in range(10)] +
            ['Fire%d' % i for i in range(10)] + ['AltFire%d' % i for i in range(10)])


class Names:
    def __init__(self, path=None, names=None):
        path = path or os.path.join(os.path.dirname(os.path.dirname(HERE)),
                                    'out', 'tmp', 'ps2-research', 'ps2model_names.txt')
        self.names = list(names or [])
        if names is None and os.path.isfile(path):
            with open(path, encoding='latin-1') as fh:
                self.names = [l.rstrip('\n') for l in fh if l.strip()]
        # The names the game code asks models for come first: a model's hash
        # multiplier can be as small as 2 (h = 3h + c), and then names collide
        # ("Report" and "Select" hash the same); the one the code uses wins.
        self.names = PRIORITY + self.names
        self.tables = {}

    def add(self, names):
        self.names = list(names) + self.names
        self.tables = {}

    def look(self, h, magic):
        t = self.tables.get(magic)
        if t is None:
            t = {}
            for n in reversed(self.names):   # earlier entries win
                t[rez_hash(n, magic)] = n
            self.tables[magic] = t
        return t.get(h)


# ------------------------------------------------------------------ PS2 reading

class PS2LOD:
    def __init__(self):
        self.mesh_type = 0
        self.node = 0          # rigid: the node; skeletal: number of bones
        self.strips = []       # [(reversed, [(pos, normal, (u,v), extra)])]
        self.qwords = 0
        # skeletal only
        self.chain_qwords = 0
        self.wmap = []         # per unique vertex: [qword offsets into the DMA chain]
        self.uverts = []       # per unique vertex: (pos, normal)
        self.bones = []        # local bone index -> node index
        self.weights = []      # per unique vertex: [(node, bias)]
        self.strip_qw = []     # per strip vertex: its position qword's offset in the chain


class PS2Piece:
    pass


def ps2_strips_from_chain(d, tag, lod):
    """Walk a DMA chain (from the tag at `tag` until a RET/END tag), run its VIF
    codes into a VU-memory map and, at each MSCAL*, decode the buffer: a header
    qword (strip count-1, vertex count) and then GIF tags (NLOOP vertices of 3
    qwords: xyz1, normal, s t q). Returns the offset after the chain."""
    start = tag
    while True:
        lo, = struct.unpack_from('<Q', d, tag)
        qwc, tid = lo & 0xFFFF, (lo >> 28) & 7
        p, end = tag + 8, tag + 16 + qwc * 16
        mem = {}
        while p < end:
            w, = struct.unpack_from('<I', d, p)
            cmd, num, imm = (w >> 24) & 0x7F, (w >> 16) & 0xFF, w & 0xFFFF
            p += 4
            if cmd & 0x60 == 0x60:
                vn, vl = (cmd >> 2) & 3, cmd & 3
                n = num or 256
                if (vn, vl) != (3, 0):
                    raise ValueError('UNPACK V%d-%d not handled' % (vn + 1, 32 >> vl))
                for i in range(n):
                    mem[(imm & 0x3FF) + i] = p + 16 * i
                p += 16 * n
            elif cmd in (0x14, 0x15, 0x17):
                decode_vu_buffer(d, mem, lod, start)
                mem = {}
            elif cmd == 0x20:
                p += 4
            elif cmd in (0x30, 0x31):
                p += 16
            elif cmd == 0x4A:
                p += (num or 256) * 8
            elif cmd in (0x50, 0x51):
                p += (imm or 65536) * 16
        tag = end
        if tid in (0, 6, 7):
            return tag


def decode_vu_buffer(d, mem, lod, chain_start):
    a = min(mem)
    nm1, nverts = struct.unpack_from('<2I', d, mem[a])
    a += 1
    got = 0
    while True:
        lo, hi = struct.unpack_from('<QQ', d, mem[a])
        nloop, eop = lo & 0x7FFF, (lo >> 15) & 1
        rev = bool(hi & 0x8000)          # REGS nibble 3: the strip starts reversed
        a += 1
        verts = []
        for _ in range(nloop):
            o = mem[a]
            pos = struct.unpack_from('<3f', d, o)
            nrm = struct.unpack_from('<3f', d, o + 16)
            s, t, q = struct.unpack_from('<3f', d, o + 32)
            extra, = struct.unpack_from('<I', d, o + 44)
            verts.append((pos, nrm, (s / q if q else s, t / q if q else t), extra))
            lod.strip_qw.append((o - chain_start) // 16)
            a += 3
        lod.strips.append((rev, verts))
        got += nloop
        if eop:
            break
    if got != nverts:
        raise ValueError('VU buffer: %d vertices, header says %d' % (got, nverts))


class PS2Model:
    def __init__(self, data, names=None):
        self.d = d = data
        self.names = names or Names()
        ftype, ver = struct.unpack_from('<IH', d, 0)
        if ftype != 2 or ver != 16:
            raise ValueError('not a PS2 v16 model (type %d version %d)' % (ftype, ver))
        (self.off_off, self.off_pieces, self.off_nodes, self.off_children,
         self.off_anims, self.off_sockets, self.file_size) = struct.unpack_from('<7I', d, 0x14)
        c = struct.unpack_from('<14I', d, 0x34)
        (self.n_keys, self.n_anims, self.n_nodes, self.n_pieces, self.n_children,
         self.n_tris, self.n_verts, self.n_weights, self.n_lods, self.n_sockets,
         self.n_weightsets, self.n_strings, self.n_strlen, self.n_unk) = c
        o = 0x6C
        n, = struct.unpack_from('<H', d, o)
        self.command = d[o + 2:o + 2 + n].decode('latin-1')
        o += 2 + n
        self.radius, self.magic, self.ext1, self.ext2 = struct.unpack_from('<fIII', d, o)
        o += 16
        self.lod_dists = list(struct.unpack_from('<%df' % self.ext2, d, o))
        o += 4 * self.ext2
        assert o == self.off_pieces, (hex(o), hex(self.off_pieces))
        self.read_pieces()
        self.read_nodes()
        self.read_children()
        self.read_anims()
        self.read_sockets()

    def name(self, h, magic=None):
        n = self.names.look(h, self.magic if magic is None else magic)
        return n if n is not None else 'h%08x' % h

    def read_pieces(self):
        d = self.d
        o = self.off_pieces
        npieces, = struct.unpack_from('<I', d, o)
        o += 4
        self.pieces = []
        for _ in range(npieces):
            p = PS2Piece()
            p.hash, = struct.unpack_from('<I', d, o)
            p.colors = struct.unpack_from('<12f', d, o + 4)
            p.texture, p.unk1, p.unk2, p.four = struct.unpack_from('<4I', d, o + 52)
            o += 68
            p.lods = []
            for _ in range(self.n_lods):
                l = PS2LOD()
                l.mesh_type, = struct.unpack_from('<I', d, o)
                o += 4
                if l.mesh_type == MT_SKEL:
                    l.chain_qwords, nwmap = struct.unpack_from('<2I', d, o)
                    o += 8
                elif l.mesh_type != MT_RIGID:
                    raise ValueError('mesh type %d not handled' % l.mesh_type)
                l.qwords, l.node = struct.unpack_from('<2I', d, o)
                o += 8
                o = ps2_strips_from_chain(d, o, l)
                if l.mesh_type == MT_SKEL:
                    nuniq = l.qwords
                    vals = struct.unpack_from('<%dH' % nwmap, d, o)
                    o += (2 * nwmap + 15) & ~15
                    i = 0
                    while i < nwmap:
                        k = vals[i]
                        l.wmap.append(list(vals[i + 1:i + 1 + k]))
                        i += 1 + k
                    assert len(l.wmap) == nuniq, (len(l.wmap), nuniq)
                    for _ in range(nuniq):
                        f = struct.unpack_from('<8f', d, o)
                        l.uverts.append((f[0:3], f[4:7]))
                        o += 32
                    l.bones = list(struct.unpack_from('<%dI' % l.node, d, o))
                    o += 4 * l.node
                    for _ in range(nuniq):
                        ws = struct.unpack_from('<4h4B', d, o)
                        o += 12
                        vw = []
                        for j in range(4):
                            if ws[4 + j] != 0xFF and ws[j]:
                                vw.append((l.bones[ws[4 + j] // 4], ws[j] / 4096.0))
                        l.weights.append(vw)
                p.lods.append(l)
            self.pieces.append(p)
        assert o == self.off_nodes, (hex(o), hex(self.off_nodes))

    def read_nodes(self):
        d = self.d
        o = self.off_nodes
        self.nodes = []
        for _ in range(self.n_nodes):
            n, = struct.unpack_from('<H', d, o)
            name = d[o + 2:o + 2 + n].decode('latin-1')
            o += 2 + n
            f = struct.unpack_from('<16f', d, o)
            h, nch, idx, unk = struct.unpack_from('<IIHH', d, o + 64)
            o += 76
            self.nodes.append(dict(name=name, m=[list(f[0:4]), list(f[4:8]), list(f[8:12]), list(f[12:16])],
                                   hash=h, nchildren=nch, index=idx, unk=unk))
        nws, = struct.unpack_from('<I', d, o)
        o += 4
        self.weight_sets = []
        for _ in range(nws):
            h, k = struct.unpack_from('<2I', d, o)
            self.weight_sets.append((h, list(struct.unpack_from('<%df' % k, d, o + 8))))
            o += 8 + 4 * k
        assert o == self.off_children, (hex(o), hex(self.off_children))

    def read_children(self):
        d = self.d
        o = self.off_children
        n, = struct.unpack_from('<I', d, o)
        o += 4
        self.children = []
        for _ in range(max(n - 1, 0)):
            k, = struct.unpack_from('<H', d, o)
            self.children.append(d[o + 2:o + 2 + k].decode('latin-1'))
            o += 2 + k
        assert o == self.off_anims, (hex(o), hex(self.off_anims))

    def read_anims(self):
        d = self.d
        o = self.off_anims
        n, = struct.unpack_from('<I', d, o)
        o += 4
        self.anims = []
        for _ in range(n):
            dims = struct.unpack_from('<3f', d, o)
            trans = struct.unpack_from('<3f', d, o + 12)
            h, interp, nk = struct.unpack_from('<3I', d, o + 24)
            o += 36
            keys = []
            for _ in range(nk):
                t, k = struct.unpack_from('<IH', d, o)
                keys.append((t, d[o + 6:o + 6 + k].decode('latin-1')))
                o += 6 + k
            tracks = []
            for _ in range(self.n_nodes):
                marker, = struct.unpack_from('<I', d, o)
                o += 4
                tr = []
                for _ in range(nk):
                    v = struct.unpack_from('<8h', d, o)
                    o += 16
                    sc = 16.0 if v[3] else 4096.0
                    tr.append(((v[0] / sc, v[1] / sc, v[2] / sc),
                               (v[4] / 16384.0, v[5] / 16384.0, v[6] / 16384.0, v[7] / 16384.0)))
                tracks.append((marker, tr))
            self.anims.append(dict(hash=h, dims=dims, trans=trans, interp=interp, keys=keys, tracks=tracks))
        assert o == self.off_sockets, (hex(o), hex(self.off_sockets))

    def read_sockets(self):
        d = self.d
        o = self.off_sockets
        n, = struct.unpack_from('<I', d, o)
        o += 4
        self.sockets = []
        for _ in range(n):
            f = struct.unpack_from('<7f', d, o)
            pad, node, h, h1, pad2 = struct.unpack_from('<5I', d, o + 28)
            self.sockets.append(dict(rot=f[0:4], pos=f[4:7], node=node, hash=h, hash1=h1))
            o += 48
        assert o == self.file_size == len(d), (hex(o), hex(self.file_size), hex(len(d)))

    def socket_name(self, s):
        n = self.names.look(s['hash'], self.magic)
        if n is None:
            n = self.names.look(s['hash1'], 1)
        return n if n is not None else 'h%08x' % s['hash']


# ------------------------------------------------------------------ conversion

def mat_apply(m, v, w=1.0):
    return tuple(m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2] + m[r][3] * w for r in range(3))


def mat_inverse_rigid(m):
    """Inverse of a rotation+translation 4x4 (row-major, translation in column 3)."""
    r = [[m[c][r] for c in range(3)] for r in range(3)]
    t = [m[0][3], m[1][3], m[2][3]]
    out = [[r[i][0], r[i][1], r[i][2], -(r[i][0] * t[0] + r[i][1] * t[1] + r[i][2] * t[2])] for i in range(3)]
    return out + [[0.0, 0.0, 0.0, 1.0]]


def mat_inverse(m):
    """General 4x4 inverse (Gauss-Jordan); bind matrices can carry scale."""
    a = [list(map(float, m[i])) + [1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    for c in range(4):
        piv = max(range(c, 4), key=lambda r: abs(a[r][c]))
        a[c], a[piv] = a[piv], a[c]
        pv = a[c][c]
        if abs(pv) < 1e-12:
            return mat_inverse_rigid(m)
        a[c] = [x / pv for x in a[c]]
        for r in range(4):
            if r != c and a[r][c]:
                f = a[r][c]
                a[r] = [x - f * y for x, y in zip(a[r], a[c])]
    return [row[4:] for row in a]


def norm(v):
    l = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / l, v[1] / l, v[2] / l) if l > 1e-12 else (0.0, 0.0, 1.0)


def sub3(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def cross3(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def dot3(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


STATS = {}


def strip_faces(lod, vindex):
    """Triangles of a LOD's strips, as ((strip vertex, ...) x3) in PC (clockwise
    seen from the front) order. vindex(strip, k) gives the PC vertex index."""
    faces = []
    for si, (rev, verts) in enumerate(lod.strips):
        for k in range(2, len(verts)):
            a, b, c = k - 2, k - 1, k
            # A strip alternates its winding every triangle; REGS nibble 3 (0x8)
            # says the first one is the other way round.
            if (k % 2 == 1) != rev:
                a, b = b, a
            ia, ib, ic = vindex(si, a), vindex(si, b), vindex(si, c)
            if ia == ib or ib == ic or ia == ic:
                STATS['degenerate'] = STATS.get('degenerate', 0) + 1
                continue
            faces.append(((si, a), (si, b), (si, c)))
    return faces


# The PS2 player-view projection is not the PC's: the same first-person model
# with the same weapon Pos carries FovXOffset/FovYOffset 31/41 on the PS2
# where the PC file has 20/50 (lighter, coin, poodle, sterling, s&w, atomlaser,
# smgsky, snow, moto, perfume*, spacechop, karatechop, cameradis, windshield:
# all PS2 twins at 31/41 but bucklezip are 20/50 on the PC). So the PS2 offsets move by the
# same amount (degrees) to look on the PC as they did on the PS2.
PV_FOV_SHIFT = (-11.0, 9.0)


def pc_command(cmd):
    def shift(mo):
        k = 0 if mo.group(1) == 'X' else 1
        v = float(mo.group(2)) + PV_FOV_SHIFT[k]
        return 'Fov%sOffset %s' % (mo.group(1), ('%d' % v) if v == int(v) else ('%g' % v))
    return re.sub(r'Fov([XY])Offset\s+(-?[0-9.]+)', shift, cmd)


def convert(m, children=(), flip=None):
    """PS2Model -> abcmodel.Model. children: [(file name, .abc bytes)] of the PC
    child models to reference (the engine loads them from the parent's own
    directory); their animation bindings and build numbers are copied, as
    ModelEdit does."""
    out = A.Model()
    out.version = 12
    out.command = pc_command(m.command)
    out.radius = m.radius
    out.lod_count = m.n_lods
    out.lod_dists = list(m.lod_dists)
    binds = [n['m'] for n in m.nodes]
    inv = [mat_inverse(b) for b in binds]
    for i, n in enumerate(m.nodes):
        out.nodes.append(A.Node(n['name'], n['index'], n['unk'] | (1 if i == 0 else 0),
                                [list(r) for r in n['m']], n['nchildren']))
    # piece names: the node names of this model first (rigid pieces are
    # usually named after their node)
    m.names.add([n['name'] for n in m.nodes])
    for p in m.pieces:
        pc = A.Piece(m.name(p.hash), p.texture, 5.0, 1.0, 1.0, 0)
        for l in p.lods:
            L = A.LOD()
            if l.mesh_type == MT_RIGID:
                b = binds[l.node]
                key2idx = {}
                idx = []
                for rev, verts in l.strips:
                    row = []
                    for pos, nrm, uv, extra in verts:
                        key = (tuple(round(x, 4) for x in pos), tuple(round(x, 3) for x in nrm))
                        vi = key2idx.get(key)
                        if vi is None:
                            vi = key2idx[key] = len(L.verts)
                            L.verts.append(A.Vertex(mat_apply(b, pos), norm(mat_apply(b, nrm, 0.0)),
                                                    [(l.node, pos, 1.0)]))
                        row.append(vi)
                    idx.append(row)
                vindex = lambda si, k: idx[si][k]
            else:
                qw2u = {}
                for u, offs in enumerate(l.wmap):
                    for q in offs:
                        qw2u[q] = u
                flat = []
                for qw in l.strip_qw:
                    if qw not in qw2u:
                        raise ValueError('strip vertex at qword %d not in the weight map' % qw)
                    flat.append(qw2u[qw])
                starts, n0 = [], 0
                for rev, verts in l.strips:
                    starts.append(n0)
                    n0 += len(verts)
                for u, (pos, nrm) in enumerate(l.uverts):
                    ws = l.weights[u]
                    tot = sum(w for _, w in ws) or 1.0
                    # A PC weight's position is the node-space position
                    # pre-multiplied by the weight: the engine skins with
                    # sum(M_i * (pos_i, w_i)) / sum(w_i) (every shipped .abc
                    # stores it so).
                    L.verts.append(A.Vertex(pos, norm(nrm),
                                            [(ni, tuple(x * (w / tot) for x in mat_apply(inv[ni], pos)), w / tot)
                                             for ni, w in ws]))
                vindex = lambda si, k: flat[starts[si] + k]
            for tri in strip_faces(l, vindex):
                L.faces.append(tuple((l.strips[si][1][k][2][0], l.strips[si][1][k][2][1], vindex(si, k))
                                     for si, k in tri))
            pc.lods.append(L)
        out.pieces.append(pc)
    out.weight_sets = [(m.name(h), list(w)) for h, w in m.weight_sets]
    ident = [((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0))] * len(m.nodes)
    out.children = [A.ChildModel('', 1, list(ident))]
    for a in m.anims:
        an = A.Anim(m.name(a['hash']), a['dims'])
        an.interp = a['interp']
        an.keys = list(a['keys'])
        an.tracks = [list(tr) for marker, tr in a['tracks']]
        out.anims.append(an)
        out.bindings.append((an.name, a['dims'], a['trans']))
    for s in m.sockets:
        out.sockets.append(A.Socket(s['node'], m.socket_name(s), s['rot'], s['pos']))
    for name, data in children:
        cm = A.read_abc(data)
        if len(cm.children) > 1:
            raise ValueError('%s has child models of its own' % name)
        out.children.append(A.ChildModel(name, cm.children[0].build, list(ident)))
        out.child_bindings.append(list(cm.bindings))
    return out


# ------------------------------------------------------------------ name dictionary

def repo_root():
    d = HERE
    while d != os.path.dirname(d) and not os.path.isfile(os.path.join(d, 'tools', 'rezextract.py')):
        d = os.path.dirname(d)
    return d


def collect_names(abcs, texts):
    """Candidate names for a PS2 model's hashes: every name in the PC models
    `abcs` (their bytes), and the identifiers in `texts` (the bytes of the
    PS2 executable and attribute files)."""
    names, seen = [], set()

    def add(n):
        n = n.strip()
        if n and n not in seen and len(n) < 64:
            seen.add(n)
            names.append(n)
    # 1. every name in every PC model
    for data in abcs:
        try:
            m = A.read_abc(data)
        except Exception:
            continue
        for n in ([x.name for x in m.nodes] + [x.name for x in m.pieces] +
                  [a.name for a in m.anims] + [s.name for s in m.sockets] +
                  [w[0] for w in m.weight_sets] + [c.name for c in m.children] +
                  [b[0] for bl in m.child_bindings for b in bl]):
            add(n)
    # 2. identifiers in the PS2 executable and attribute files
    tok = re.compile(rb'[A-Za-z_+][A-Za-z0-9_+&\-. ]{1,40}')
    for data in texts:
        for t in tok.findall(data):
            t = t.decode('latin-1')
            for part in [t] + re.split(r'[ .]', t):
                add(part)
                if '%d' in part:
                    for i in range(10):
                        add(part.replace('%d', str(i)))
    # 3. weapon animations with numbered variants
    for b in ('Idle_', 'Fire', 'PreFire', 'PostFire', 'Fire_'):
        for i in range(10):
            add(b + str(i))
    return names


def build_names(out, pc_dir=None, ps2_dir=None):
    """collect_names from the PC game's archives and a PS2 research
    extraction (disc/SLUS_200.28, extract/Attributes/*.txt), into a file
    (the default Names() reads it, for research; build/ps2/ps2install.py
    collects its own)."""
    root = repo_root()
    pc_dir = pc_dir or os.path.join(root, 'rez', 'games', 'nolf-eu-goty')
    ps2_dir = ps2_dir or os.path.join(root, 'out', 'tmp', 'ps2-research')
    sys.path.insert(0, os.path.join(root, 'tools'))
    from rezextract import Rez

    def abcs():
        for rz in sorted(os.listdir(pc_dir)):
            if not rz.lower().endswith('.rez'):
                continue
            r = Rez(os.path.join(pc_dir, rz))
            for p, pos, size in r.walk():
                if p.lower().endswith('.abc'):
                    r.f.seek(pos)
                    yield r.f.read(size)
    srcs = [os.path.join(ps2_dir, 'disc', 'SLUS_200.28')]
    att = os.path.join(ps2_dir, 'extract', 'Attributes')
    if os.path.isdir(att):
        srcs += [os.path.join(att, x) for x in sorted(os.listdir(att)) if x.lower().endswith('.txt')]
    texts = [open(x, 'rb').read() for x in srcs if os.path.isfile(x)]
    names = collect_names(abcs(), texts)
    with open(out, 'w', encoding='latin-1') as fh:
        for n in names:
            fh.write(n + '\n')
    print('%d names -> %s' % (len(names), out))


def dump(m):
    print('cmd %r radius %g magic %d ext %d,%d lod dists %s' % (m.command, m.radius, m.magic, m.ext1, m.ext2, m.lod_dists))
    for i, n in enumerate(m.nodes):
        print('node %2d %-28s idx %d unk %d children %d t=(%.2f %.2f %.2f)' % (
            i, n['name'], n['index'], n['unk'], n['nchildren'], n['m'][0][3], n['m'][1][3], n['m'][2][3]))
    for p in m.pieces:
        print('piece %-24s tex %d colors %s unk %d %d four %d lods %s' % (
            m.name(p.hash), p.texture, [round(x, 2) for x in p.colors], p.unk1, p.unk2, p.four,
            [(l.mesh_type, l.node, len(l.strips), sum(len(v) for r, v in l.strips), len(l.uverts)) for l in p.lods]))
    print('weight sets', [m.name(h) for h, w in m.weight_sets])
    print('children', m.children)
    for a in m.anims:
        print('anim %-20s dims %s trans %s interp %d keys %d %s markers %s' % (
            m.name(a['hash']), [round(x, 2) for x in a['dims']], [round(x, 2) for x in a['trans']],
            a['interp'], len(a['keys']), [k for k in a['keys'] if k[1]][:4],
            sorted(set('%x' % t[0] for t in a['tracks']))[:4]))
    for s in m.sockets:
        print('socket %-12s node %d rot %s pos %s' % (m.socket_name(s), s['node'],
              [round(x, 3) for x in s['rot']], [round(x, 3) for x in s['pos']]))


def main(argv):
    names_path = None
    if '--names' in argv:
        i = argv.index('--names')
        names_path = argv[i + 1]
        del argv[i:i + 2]
    if argv[0] == 'names':
        kw = {}
        for k in ('--pc-rez', '--ps2-dir'):
            if k in argv:
                i = argv.index(k)
                kw[k[2:].replace('-', '_').replace('rez', 'dir')] = argv[i + 1]
                del argv[i:i + 2]
        build_names(argv[1], **kw)
        return
    if argv[0] == 'thief':
        convert_thief(argv[1], names=names_path)
        return
    names = Names(names_path)
    m = PS2Model(open(argv[1], 'rb').read(), names)
    if argv[0] == 'dump':
        dump(m)
    elif argv[0] == 'conv':
        children = []
        while '--child' in argv:
            i = argv.index('--child')
            children.append(argv[i + 1])
            del argv[i:i + 2]
        out = convert(m, [(os.path.basename(c).lower(), open(c, 'rb').read()) for c in children])
        with open(argv[2], 'wb') as fh:
            fh.write(A.write_abc(out))
        print('%s: %d nodes, %d pieces, %d anims, %d sockets, children %s' % (
            argv[2], len(out.nodes), len(out.pieces), len(out.anims), len(out.sockets),
            [c.name for c in out.children[1:]]))



# ------------------------------------------------------------------ the thief levels' models

# (PS2 name, PC child models to reference). The character's PS2 children
# (playerbase.ltb, base_general.ltb, base_death.ltb) are replaced by the PC
# player's own (CHARS/MODELS/PLAYER_ACTION.abc references these three): same
# 25-node skeleton, and the PC animation names the game code asks for.
THIEF_MODELS = [
    ('guns/models_pv/ps2lockpick_pv.ltb', []),
    ('guns/models_pv/ps2safecracker_pv.ltb', []),
    ('guns/models_pv/delislescope.ltb', []),
    ('guns/models_hh/ps2lockpick_hh.ltb', []),
    ('guns/models_hh/ps2safecracker_hh.ltb', []),
    ('interface/models/hud/briefcase.ltb', []),
    ('interface/models/hud/change_01.ltb', []),
    ('interface/models/hud/moneybag.ltb', []),
    ('interface/models/hud/purses_01.ltb', []),
    ('attachments/models/britishpolicehat.ltb', []),
    ('props/models/moneybag.ltb', []),
    ('props/models/payphone_01.ltb', []),
    ('chars/models/hero_thief.ltb', ['basemodel.abc', 'fembase.abc', 'playerbase.abc']),
    # the PS2's animation-only child models, converted as they are (not
    # referenced by the above; see spec-models.md)
    ('chars/models/base_general.ltb', []),
    ('chars/models/base_death.ltb', []),
    ('chars/models/base_pistol.ltb', []),
    ('chars/models/base_rifle.ltb', []),
]


def pc_file(pc_dir, name):
    sys.path.insert(0, os.path.join(repo_root(), 'tools'))
    from rezextract import Rez
    want = name.replace('\\', '/').lower()
    for rz in sorted(os.listdir(pc_dir)):
        if rz.lower().endswith('.rez'):
            r = Rez(os.path.join(pc_dir, rz))
            for p, pos, size in r.walk():
                if p.lower() == want:
                    r.f.seek(pos)
                    return r.f.read(size)
    return None


def convert_thief(outdir, ps2_rez=None, pc_dir=None, names=None):
    """Every model the thief levels need, from the PS2 archive into
    outdir/<UPPER-CASE DIRS>/<NAME>.abc."""
    import ps2rez
    root = repo_root()
    ps2_rez = ps2_rez or os.path.join(root, 'out', 'tmp', 'ps2-research', 'disc', 'LITH_PS2.REZ')
    pc_dir = pc_dir or os.path.join(root, 'rez', 'games', 'nolf-eu-goty')
    f, hdr, tables = ps2rez.open_rez(ps2_rez)
    t0 = {e[0]: e for e in tables[0] if e[2]}
    for name, kids in THIEF_MODELS:
        e = t0.get(ps2rez.rez_hash(name, hdr['magic']))
        if not e:
            print('not in the PS2 archive:', name)
            continue
        f.seek(e[1] * 2048)
        m = PS2Model(f.read(e[2]), Names(names))
        children = []
        for k in kids:
            data = pc_file(pc_dir, os.path.dirname(name) + '/' + k)
            if data is None:
                raise ValueError('PC child model %s not found' % k)
            children.append((k, data))
        out = convert(m, children)
        parts = name.split('/')
        dst = os.path.join(outdir, *[x.upper() for x in parts[:-1]], os.path.splitext(parts[-1])[0].upper() + '.abc')
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        data = A.write_abc(out)
        assert A.write_abc(A.read_abc(data)) == data
        with open(dst, 'wb') as fh:
            fh.write(data)
        unnamed = [p.name for p in out.pieces if re.match(r'h[0-9a-f]{8}$', p.name)]
        unnamed += [a.name for a in out.anims if re.match(r'h[0-9a-f]{8}$', a.name)]
        print('%-44s %3d nodes %3d pieces %4d tris %3d anims %2d sockets%s%s' % (
            dst, len(out.nodes), len(out.pieces),
            sum(len(l.faces) for p in out.pieces for l in p.lods), len(out.anims), len(out.sockets),
            ' children ' + ','.join(kids) if kids else '',
            ' unnamed: ' + ' '.join(unnamed) if unnamed else ''))


if __name__ == '__main__':
    main(sys.argv[1:])
