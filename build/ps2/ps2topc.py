#!/usr/bin/env python3
"""Convert a PS2 NOLF world (worlds/*.ltb, v66) into a PC NOLF world (.dat, v66).

  ps2topc.py <in.ltb> <out.dat> [--pc-rez DIR] [--ps2-rez LITH_PS2.REZ]

First version: geometry, BSPs, collision grids, leaves/PVS, portals, terrain
and objects are carried over; each polygon gets its own PC surface whose
O/P/Q reproduce its PS2 per-vertex UVs; there are no PC lightmaps -- the
PS2's baked lighting (lightmap pages, vertex colours) becomes the PC vertex
colours. Layouts: spec-pc-world.md, spec-ps2-world.md.
"""
import math, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ps2world as P   # noqa: E402

SURF_INVISIBLE, SURF_TRANSLUCENT, SURF_LIGHTMAP = 0x4, 0x8, 0x80

# A world texture under this root is the PS2 disc's texture of the name that
# follows (ps2install.py installs it there), never the PC's
PS2_TEX_ROOT = 'PS2\\'

# ------------------------------------------------------------------ texture sizes

class TexSizes:
    """Width/height of a texture by name: the PC archives first (that is what the
    engine will load), then the PS2 archive, else 64x64."""
    def __init__(self, pc_dir, ps2_rez):
        self.pc = {}
        self.cache = {}
        if pc_dir:
            d = HERE
            while d != os.path.dirname(d) and not os.path.isfile(os.path.join(d, 'tools', 'rezextract.py')):
                d = os.path.dirname(d)
            sys.path.insert(0, os.path.join(d, 'tools'))
            from rezextract import Rez
            for n in sorted(os.listdir(pc_dir)):
                if n.lower().endswith('.rez'):
                    path = os.path.join(pc_dir, n)
                    for p, pos, sz in Rez(path).walk():
                        self.pc.setdefault(p.replace('/', '\\').lower(), (path, pos))
        self.ps2 = None
        if ps2_rez:
            import ps2rez
            f, hdr, tables = ps2rez.open_rez(ps2_rez)
            self.ps2 = (ps2rez, f, hdr, {e[0]: e for e in tables[0] if e[2]})

    def head(self, name):
        key = name.replace('/', '\\').lower()
        if key.startswith(PS2_TEX_ROOT.lower()):
            name, key = name[len(PS2_TEX_ROOT):], None
        if key in self.pc:
            path, pos = self.pc[key]
            with open(path, 'rb') as fh:
                fh.seek(pos)
                return fh.read(16), 'pc'
        if self.ps2:
            mod, f, hdr, t0 = self.ps2
            e = t0.get(mod.rez_hash(name, hdr['magic']))
            if e:
                f.seek(e[1] * 2048)
                return f.read(16), 'ps2'
        return None, None

    def ps2_bytes(self, name):
        if not self.ps2:
            return None
        mod, f, hdr, t0 = self.ps2
        e = t0.get(mod.rez_hash(name, hdr['magic']))
        if not e:
            return None
        f.seek(e[1] * 2048)
        return f.read(e[2])

    def size(self, name):
        if name not in self.cache:
            h, where = self.head(name)
            if h and len(h) >= 12 and struct.unpack_from('<i', h, 4)[0] == -5:
                w, hh = struct.unpack_from('<HH', h, 8)
                self.cache[name] = (w, hh, where)
            else:
                self.cache[name] = (64, 64, None)
        return self.cache[name]

# ------------------------------------------------------------------ maths

def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def solve3(r0, r1, r2, rhs):
    """x with rows . x = rhs (Cramer); None if singular."""
    det = dot(r0, cross(r1, r2))
    if abs(det) < 1e-12:
        return None
    c0, c1, c2 = cross(r1, r2), cross(r2, r0), cross(r0, r1)
    # inverse columns are c0, c1, c2 / det (rows of the cofactor matrix)
    return tuple((rhs[0] * c0[i] + rhs[1] * c1[i] + rhs[2] * c2[i]) / det for i in range(3))


def poly_opq(pts, uvs, normal):
    """O, P, Q with u = (p - O).P and v = (p - O).Q at the poly's vertices (u, v in
    texels). Uses the three vertices spanning the largest triangle."""
    n = len(pts)
    best, tri = -1.0, None
    for i in range(n):
        for j in range(i + 1, n):
            for k in range(j + 1, n):
                a = cross(sub(pts[j], pts[i]), sub(pts[k], pts[i]))
                ar = dot(a, a)
                if ar > best:
                    best, tri = ar, (i, j, k)
                if n > 12:
                    break
    if tri is None or best < 1e-8:
        return (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), 0.0
    i, j, k = tri
    e1, e2 = sub(pts[j], pts[i]), sub(pts[k], pts[i])
    nn = cross(e1, e2)
    Pv = solve3(e1, e2, nn, (uvs[j][0] - uvs[i][0], uvs[k][0] - uvs[i][0], 0.0))
    Qv = solve3(e1, e2, nn, (uvs[j][1] - uvs[i][1], uvs[k][1] - uvs[i][1], 0.0))
    if Pv is None or Qv is None:
        return (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), 0.0
    X = solve3(Pv, Qv, nn, (uvs[i][0], uvs[i][1], 0.0))
    if X is None:   # P and Q parallel: a degenerate mapping
        X = (0.0, 0.0, 0.0)
    O = sub(pts[i], X)
    err = 0.0
    for p, (u, v) in zip(pts, uvs):
        d = sub(p, O)
        err = max(err, abs(dot(d, Pv) - u), abs(dot(d, Qv) - v))
    return O, Pv, Qv, err

# ------------------------------------------------------------------ lighting

class Lightmaps:
    """PS2 lightmap pages: colour = L - d per channel (spec-ps2-world.md §8)."""
    def __init__(self, pages):
        self.pages = []
        for pg in pages:
            pal = []
            for i in range(256):
                e = pg.clut[4 * P.clut_index(i):4 * P.clut_index(i) + 4]
                dr, dg, db, L = e
                pal.append((max(0, L - dr), max(0, L - dg), max(0, L - db)))
            self.pages.append([pal[i] for i in pg.pixels])

    def sample_px(self, page, x, y):
        """Bilinear, at page pixel coordinates (pixel centres on integers)."""
        px = self.pages[page]
        x = min(63.0, max(0.0, x))
        y = min(63.0, max(0.0, y))
        x0, y0 = int(x), int(y)
        x1, y1 = min(63, x0 + 1), min(63, y0 + 1)
        fx, fy = x - x0, y - y0
        out = []
        for c in range(3):
            a = px[y0 * 64 + x0][c] * (1 - fx) + px[y0 * 64 + x1][c] * fx
            b = px[y1 * 64 + x0][c] * (1 - fx) + px[y1 * 64 + x1][c] * fx
            out.append(a * (1 - fy) + b * fy)
        return out

    def sample(self, page, u, v):
        px = self.pages[page]
        x = min(63.0, max(0.0, u * 64.0 - 0.5))
        y = min(63.0, max(0.0, v * 64.0 - 0.5))
        x0, y0 = int(x), int(y)
        x1, y1 = min(63, x0 + 1), min(63, y0 + 1)
        fx, fy = x - x0, y - y0
        out = []
        for c in range(3):
            a = px[y0 * 64 + x0][c] * (1 - fx) + px[y0 * 64 + x1][c] * fx
            b = px[y1 * 64 + x0][c] * (1 - fx) + px[y1 * 64 + x1][c] * fx
            out.append(a * (1 - fy) + b * fy)
        return out


def to_pc_colour(rgb, scale):
    return tuple(max(0, min(255, int(round(c * scale)))) for c in rgb)


# The engine's six lightmap plane types (math_FindClosestAxisBasis over
# g_AxisBases, svlast.cpp): the basis normal, and the "up" its lightmap
# vectors are built from (P = up x N, Q = N x P; mr_LMVectors).
AXIS_NORMALS = [(0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1), (1, 0, 0), (-1, 0, 0)]
AXIS_UPS = [(0, 0, -1), (0, 0, 1), (0, 1, 0), (0, -1, 0), (0, -1, 0), (0, -1, 0)]
LM_MAX = 32          # texels a side (the renderers' 1024-texel buffers)


def lm_vectors(n):
    best, t = -2.0, 0
    for i, a in enumerate(AXIS_NORMALS):
        d = dot(n, a)
        if d > best:
            best, t = d, i
    P = cross(AXIS_UPS[t], n)
    Q = cross(n, P)
    return P, Q


def lm_compress(texels):
    """The engine's 16-bit RLE (mr_DecompressLM): a word per texel, or a word
    with bit 15 set and a count byte for a run."""
    out = bytearray()
    i, n = 0, len(texels)
    while i < n:
        j = i
        while j < n and j - i < 255 and texels[j] == texels[i]:
            j += 1
        if j - i > 1:
            out += struct.pack('<HB', texels[i] | 0x8000, j - i)
        else:
            out += struct.pack('<H', texels[i])
        i = j
    return bytes(out)


def make_lightmap(m, p, lm, grid, scale):
    """(lmOrigin, width, height, compressed data) for a PS2 lightmapped poly
    on the PC's lightmap grid, sampled from its PS2 page; None if the poly
    has no PS2 lightmap or would not fit LM_MAX texels a side."""
    if p.lm_page < 0 or not p.verts or p.verts[0].lm_u is None or p.lm_w == 0:
        return None
    nx, ny, nz, dist = m.planes[p.plane]
    N = (nx, ny, nz)
    P, Q = lm_vectors(N)
    pp, qq = dot(P, P), dot(Q, Q)
    if pp < 1e-9 or qq < 1e-9:
        return None
    pts = m.poly_xyz(p)
    s = [dot(v, P) / grid for v in pts]
    t = [dot(v, Q) / grid for v in pts]
    s0, t0 = math.floor(min(s)), math.floor(min(t))
    W = int(math.ceil(max(s) - s0)) + 1
    H = int(math.ceil(max(t) - t0)) + 1
    if W > LM_MAX or H > LM_MAX:
        return None
    origin = tuple(s0 * grid * P[i] / pp + t0 * grid * Q[i] / qq + dist * N[i] for i in range(3))
    # world -> PS2 page pixels, from the poly's own vertices
    O2, P2, Q2, _ = poly_opq(pts, [(v.lm_u * 64.0, v.lm_v * 64.0) for v in p.verts], N)
    x0, y0 = p.lm_x, p.lm_y
    x1, y1 = p.lm_x + max(1, p.lm_w) - 1, p.lm_y + max(1, p.lm_h) - 1
    texels = []
    for j in range(H):
        for i in range(W):
            pos = tuple(origin[k] + i * grid * P[k] / pp + j * grid * Q[k] / qq for k in range(3))
            d = sub(pos, O2)
            u = min(x1, max(x0, dot(d, P2) - 0.5))
            v = min(y1, max(y0, dot(d, Q2) - 0.5))
            rgb = to_pc_colour(lm.sample_px(p.lm_page, u, v), scale)
            texels.append(((rgb[0] >> 3) << 10) | ((rgb[1] >> 3) << 5) | (rgb[2] >> 3))
    return origin, W, H, lm_compress(texels)

# ------------------------------------------------------------------ writer

class W:
    def __init__(self):
        self.b = bytearray()

    def pack(self, fmt, *a):
        self.b += struct.pack('<' + fmt, *a)

    def u8(self, x): self.pack('B', x)
    def u16(self, x): self.pack('H', x)
    def u32(self, x): self.pack('I', x)
    def i32(self, x): self.pack('i', x)
    def f32(self, x): self.pack('f', x)
    def vec(self, v): self.pack('3f', *v)

    def lstr(self, s):
        e = s.encode('latin-1')
        self.u16(len(e))
        self.b += e

    def pos(self):
        return len(self.b)


def strip(s):
    return s.rstrip('\0') if isinstance(s, str) else s


def write_pblock(w, pb):
    sx, sy, sz = pb.size
    w.pack('3i', sx, sy, sz)
    w.vec(pb.block_size)
    w.vec(pb.origin)
    recs = pb.data
    at = 0
    for _, n, root in pb.blocks:
        w.u16(n)
        w.u16(root if n else 0xFFFF)
        w.b += recs[at * 6:(at + n) * 6]     # {u16 node, u16 back, u16 front}: the PC encoding already
        at += n
    if at * 6 != len(recs):
        raise ValueError('PBlock records not consumed: %d of %d' % (at, len(recs) // 6))


def convert_bsp(w, m, world, tex, lm, stats, scale, world_index, anim):
    is_world_model = not (m.info_flags & (0x10 | 0x20))
    lmaps = [make_lightmap(m, p, lm, world.lm_grid_size, scale) if anim is not None else None
             for p in m.polys]
    stats['lm_polys'] += sum(1 for x in lmaps if x)
    stats['lm_too_big'] += sum(1 for p, x in zip(m.polys, lmaps)
                               if x is None and p.lm_page >= 0 and p.lm_w and p.verts and p.verts[0].lm_u is not None)
    # textures used by this BSP, in first-use order
    tex_index = {}
    tex_names = []
    surf_of_poly = []
    for p in m.polys:
        s = m.surfaces[p.surface]
        t = strip(world.textures[s.texture])
        if t not in tex_index:
            tex_index[t] = len(tex_names)
            tex_names.append(t)

    w.pack('8I', *([0] * 8))
    w.u32(m.info_flags)
    w.u32(0)
    w.lstr(strip(m.name))
    n_total_verts = sum(len(p.verts) for p in m.polys)
    vis_bytes = sum(len(d) for lf in m.leaves if lf.share is None for _, d in lf.lists)
    n_lists = sum(len(lf.lists) for lf in m.leaves if lf.share is None)
    n_refs = sum(len(lf.poly_refs) for lf in m.leaves)
    w.pack('12I', len(m.points), len(m.planes), len(m.polys), len(m.portals), len(m.polys),
           len(m.leaves), n_total_verts, vis_bytes, n_lists, len(m.nodes), n_refs,
           sum(1 for x in lmaps if x))
    w.vec(m.box_min)
    w.vec(m.box_max)
    if is_world_model:
        tr = tuple((a + b) * 0.5 for a, b in zip(m.box_min, m.box_max))
    else:
        tr = m.translation
    w.vec(tr)

    pool = b''.join(t.encode('latin-1') + b'\0' for t in tex_names)
    w.u32(len(pool))
    w.u32(len(tex_names))
    w.b += pool

    for p in m.polys:
        if len(p.verts) > 255:
            raise ValueError('poly with %d verts' % len(p.verts))
        w.u8(len(p.verts))
        w.u8(0)

    for lf in m.leaves:
        if lf.share is not None:
            w.u16(0xFFFF)
            w.u16(lf.share)
        else:
            w.u16(len(lf.lists))
            for portal, data in lf.lists:
                w.pack('hH', portal, len(data))
                w.b += data
        w.u32(len(lf.poly_refs))
        for a, b in lf.poly_refs:
            w.pack('HH', a, b)
        w.f32(lf.unk)

    for nx, ny, nz, d in m.planes:
        w.pack('4f', nx, ny, nz, d)

    # one surface per poly
    for p, lmap in zip(m.polys, lmaps):
        s = m.surfaces[p.surface]
        t = strip(world.textures[s.texture])
        flags = s.flags & ~SURF_LIGHTMAP & 0x1FFFFFFF
        if lmap:
            flags |= SURF_LIGHTMAP
        pts = m.poly_xyz(p)
        if p.verts and p.verts[0].u is not None:
            tw, th, _ = tex.size(t)
            uvs = [(v.u * tw, v.v * th) for v in p.verts]
            O, Pv, Qv, err = poly_opq(pts, uvs, m.planes[p.plane][:3])
            stats['uv_err'] = max(stats['uv_err'], err if tw * th else 0)
            if err > 0.5:
                stats['uv_bad'] += 1
        else:
            O, Pv, Qv = (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)
        w.vec(O)
        w.vec(Pv)
        w.vec(Qv)
        w.u16(tex_index[t])
        w.u32(p.plane)
        w.u32(flags)
        w.u8(0)
        w.pack('3B', 0, 0, 0)
        if s.effect:
            w.u8(1)
            w.lstr(strip(s.effect))
            w.lstr(strip(s.effect_param or ''))
        else:
            w.u8(0)
        w.u16(s.texture_flags)

    for i, (p, lmap) in enumerate(zip(m.polys, lmaps)):
        if lmap:
            origin, lw, lh, data = lmap
            w.vec(origin)
            w.pack('HHH', lw, lh, 1)
            w.pack('HH', 0, len(anim))  # LightAnim_BASE (anim 0), this poly's slot
            anim.append((world_index, i, data))
        else:
            w.vec((0.0, 0.0, 0.0))
            w.pack('HHH', 0, 0, 0)      # lmWidth, lmHeight, nLMAnimRefs
        w.u32(i)                        # its own surface
        for v in p.verts:
            if v.color is not None:
                rgb = to_pc_colour(v.color[:3], scale)
            elif v.lm_u is not None and p.lm_page >= 0:
                rgb = to_pc_colour(lm.sample(p.lm_page, v.lm_u, v.lm_v), scale)
            else:
                rgb = (255, 255, 255)
            w.pack('H3B', v.point, *rgb)

    for poly, leaf, back, front in m.nodes:
        w.pack('IHii', poly, 0xFFFF if leaf < 0 else leaf, back, front)

    for pt in m.portals:
        w.lstr(strip(pt.name))
        w.u32(pt.unk1)
        w.u16(pt.flags)
        w.vec(pt.center)
        w.vec(pt.dims)

    for x, y, z in m.points:
        w.pack('6f', x, y, z, 0.0, 0.0, 0.0)

    write_pblock(w, m.pblock)
    w.i32(m.root_node)
    w.u32(len(m.terrain_sections))
    for t in m.terrain_sections:
        w.u32(len(t.nodes))
        for a, b, c in t.nodes:
            w.pack('Iii', a, b, c)
        w.i32(t.root)
        w.u32(len(t.polys))
        for pi in t.polys:
            w.u32(pi)
        write_pblock(w, t.pblock)
        w.pack('4BI', *t.header)
        w.vec(t.center)


def write_objects(w, objs):
    w.u32(len(objs))
    for o in objs:
        rec = W()
        rec.lstr(strip(o.cls))
        rec.u32(len(o.props))
        for pr in o.props:
            rec.lstr(strip(pr.name))
            rec.u8(pr.type)
            rec.u32(0)
            d = W()
            if pr.type == 0:
                d.lstr(strip(pr.value))
            elif pr.type in (1, 2):
                d.vec(pr.value)
            elif pr.type in (3, 4, 6):
                d.f32(pr.value)
            elif pr.type == 5:
                d.u8(pr.value)
            elif pr.type == 7:
                d.pack('4f', *pr.value)
            rec.u16(len(d.b))
            rec.b += d.b
        if len(rec.b) > 0xFFFF:
            raise ValueError('object record too long')
        w.u16(len(rec.b))
        w.b += rec.b


def light_objects(world):
    """The PS2's baked static lights as PC Light objects. The engine reads them
    at load into the light table that lights models and characters
    (w_ReadWorldLights: Pos, LightRadius, LightColor, LightObjects; fast by
    default); the game has no Light class, so nothing else is made of them."""
    out = []
    for i, L in enumerate(world.lights):
        if L.radius <= 0:
            continue
        props = [P.Obj(name='Name', type=0, value='PS2Light%d' % i),
                 P.Obj(name='Pos', type=1, value=tuple(L.pos)),
                 P.Obj(name='Rotation', type=7, value=(0.0, 0.0, 0.0, 0.0)),
                 P.Obj(name='LightRadius', type=3, value=float(L.radius)),
                 P.Obj(name='LightColor', type=2, value=tuple(float(c) for c in L.color)),
                 P.Obj(name='LightObjects', type=5, value=1),
                 P.Obj(name='FastLightObjects', type=5, value=1)]
        out.append(P.Obj(cls='Light', props=props))
    return out


def write_light_anims(w, anim):
    """One animation, LightAnim_BASE: a single frame of every lightmapped poly
    (spec-pc-world.md §7)."""
    if not anim:
        w.pack('5I', 0, 0, 0, 0, 0)
        return
    total = sum(len(d) for _, _, d in anim)
    w.pack('5I', len(anim), 1, total, len(anim), 1)
    w.lstr('LightAnim_BASE')
    w.pack('III', 0, 1, len(anim))
    for wi, pi, _ in anim:
        w.pack('HH', wi, pi)
    for _, _, d in anim:
        w.u32(len(d))
        w.b += d


def convert(src, dst, pc_dir, ps2_rez, scale=2.0, lightmaps=True):
    world = P.load(src)
    tex = TexSizes(pc_dir, ps2_rez)
    lm = Lightmaps(world.lm_pages)
    stats = dict(uv_err=0.0, uv_bad=0, lm_polys=0, lm_too_big=0)
    anim = [] if lightmaps else None
    w = W()
    w.pack('11I', 66, 0, 0, *([0] * 8))
    info = strip(world.info)
    w.u32(len(info))
    w.b += info.encode('latin-1')
    w.f32(world.lm_grid_size)
    w.vec(world.extents_min)
    w.vec(world.extents_max)
    t = world.tree
    w.vec(t.box_min)
    w.vec(t.box_max)
    w.u32(t.n_nodes)
    w.u32(t.terrain_depth)
    bits = t.layout
    for i in range(0, len(bits), 8):
        byte = 0
        for j, bit in enumerate(bits[i:i + 8]):
            byte |= bit << j
        w.u8(byte)
    w.u32(len(world.models))
    for wi, m in enumerate(world.models):
        at = w.pos()
        w.u32(0)
        convert_bsp(w, m, world, tex, lm, stats, scale, wi, anim)
        struct.pack_into('<I', w.b, at, w.pos())
    obj_ofs = w.pos()
    write_objects(w, world.objects + light_objects(world))
    lm_ofs = w.pos()
    write_light_anims(w, anim)
    struct.pack_into('<II', w.b, 4, obj_ofs, lm_ofs)
    with open(dst, 'wb') as fh:
        fh.write(w.b)
    missing = sorted(n for n, (a, b, where) in tex.cache.items() if where != 'pc')
    return world, stats, missing, len(w.b)


def main(argv):
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('dst')
    ap.add_argument('--pc-rez')
    ap.add_argument('--ps2-rez')
    ap.add_argument('--scale', type=float, default=2.0)
    ap.add_argument('--tex-out', help='write PC versions of the textures the PC archives lack here')
    ap.add_argument('--no-lightmaps', action='store_true', help='vertex colours only')
    a = ap.parse_args(argv)
    world, stats, missing, size = convert(a.src, a.dst, a.pc_rez, a.ps2_rez, a.scale, not a.no_lightmaps)
    if a.tex_out:
        import ps2dtx
        tex = TexSizes(None, a.ps2_rez)
        for n in missing:
            data = tex.ps2_bytes(n)
            if not data:
                print('    not on the PS2 either:', n)
                continue
            # Directories in upper case, as the game directory's own (on Linux a
            # second, differently-cased directory is not the same one, and Wine
            # finds only one of them).
            parts = n.replace('/', '\\').split('\\')
            out = os.path.join(a.tex_out, *[x.upper() for x in parts[:-1]], parts[-1])
            os.makedirs(os.path.dirname(out), exist_ok=True)
            open(out, 'wb').write(ps2dtx.ps2_to_pc(data))
    print('%s: %d bytes, %d BSPs, %d objects; worst UV fit %.3f texels, %d polys over 0.5'
          % (a.dst, size, len(world.models), len(world.objects), stats['uv_err'], stats['uv_bad']))
    print('lightmaps: %d polys; %d too big for %dx%d texels (vertex colours)'
          % (stats['lm_polys'], stats['lm_too_big'], LM_MAX, LM_MAX))
    print('%d textures not in the PC archives:' % len(missing))
    for n in missing:
        print('   ', n)


if __name__ == '__main__':
    main(sys.argv[1:])
