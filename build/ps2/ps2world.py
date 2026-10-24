#!/usr/bin/env python3
"""Reader for PS2 No One Lives Forever world files (worlds/*.ltb, version 66).

Written from scratch from the layout documented in ../spec-ps2-world.md.
Walks the file section by section and checks that every byte is accounted for.

Importable:
    import ps2world
    w = ps2world.load('thiefmission1.ltb')
    w.models            # list of Bsp (world models, PhysicsBSP = main world, VisBSP)
    w.main              # the PhysicsBSP (WORLDINFO_MAINWORLD) Bsp
    w.textures          # texture names (index = Surface.texture)
    w.objects           # list of WorldObject (cls, props: list of Prop)
    w.lights            # static light list
    w.light_grid        # LightGrid (model lighting volume)
    w.lm_pages          # list of LMPage (64x64 8-bit CLUT lightmap pages)
    for poly in w.main.polys:
        w.main.poly_xyz(poly)          # [(x,y,z), ...]
        [(v.u, v.v) for v in poly.verts]
        w.main.surfaces[poly.surface].texture  ->  w.textures[...]
        w.main.planes[poly.plane]      # (nx, ny, nz, dist)

Command line:  ps2world.py file.ltb [--obj out.obj [--model NAME]] [--quiet]
"""
import struct
import sys
from collections import Counter

# WorldBsp info flags (first dword of a BSP record)
WI_VISBSP = 0x02      # (a world model) uses/has a separate vis BSP
WI_MAINWORLD = 0x04
WI_PHYSICS = 0x10
WI_VIS = 0x20

# Surface flags (the low ones are the LithTech SURF_* bits)
SURF_SOLID = 0x1
SURF_NONEXISTENT = 0x2
SURF_INVISIBLE = 0x4      # PS2: polys of such a surface store bare 4-byte point indices
SURF_TRANSLUCENT = 0x8
SURF_SKY = 0x10
SURF_BRIGHT = 0x20
SURF_FLATSHADE = 0x40
SURF_LIGHTMAP = 0x80      # PS2: with !TRANSLUCENT, the vertex carries lightmap-page UVs
SURF_NOSUBDIV = 0x100
SURF_GOURAUDSHADE = 0x1000

PROP_STRING, PROP_VECTOR, PROP_COLOR, PROP_REAL, PROP_FLAGS, PROP_BOOL, PROP_LONGINT, PROP_ROTATION = range(8)
PROP_TYPE_NAMES = ['string', 'vector', 'color', 'real', 'flags', 'bool', 'longint', 'rotation']


class FormatError(Exception):
    pass


class Reader:
    __slots__ = ('b', 'p')

    def __init__(self, data, pos=0):
        self.b = data
        self.p = pos

    def take(self, fmt):
        s = struct.Struct('<' + fmt)
        v = s.unpack_from(self.b, self.p)
        self.p += s.size
        return v

    def one(self, fmt):
        return self.take(fmt)[0]

    def u8(self):
        v = self.b[self.p]
        self.p += 1
        return v

    def u16(self):
        return self.one('H')

    def u32(self):
        return self.one('I')

    def i32(self):
        return self.one('i')

    def f32(self):
        return self.one('f')

    def vec(self):
        return self.take('3f')

    def raw(self, n):
        if self.p + n > len(self.b):
            raise FormatError('read past EOF at %d (+%d)' % (self.p, n))
        v = self.b[self.p:self.p + n]
        self.p += n
        return v

    def str16(self):
        """u16 length, then that many bytes (the count includes a trailing NUL when present)."""
        n = self.u16()
        return self.raw(n).rstrip(b'\0').decode('latin-1')

    def str32(self):
        n = self.u32()
        return self.raw(n).rstrip(b'\0').decode('latin-1')


class Obj:
    """Plain attribute bag with a readable repr."""
    def __init__(self, **kw):
        self.__dict__.update(kw)

    def __repr__(self):
        items = ', '.join('%s=%r' % (k, v) for k, v in self.__dict__.items()
                          if not isinstance(v, (list, bytes)) or len(v) < 8)
        return '%s(%s)' % (type(self).__name__, items)


class Vert(Obj):
    """One polygon vertex. point: index into Bsp.points. u, v: base texture coords
    (texture repeats, already divided by the texture size). lm_u, lm_v: lightmap-page
    coords (0..1 of the 64x64 page) or None. color: (r, g, b, a) bytes or None."""


class Poly(Obj):
    pass


class Surface(Obj):
    pass


class Bsp(Obj):
    def poly_xyz(self, poly):
        pts = self.points
        return [pts[v.point] for v in poly.verts]


# ---------------------------------------------------------------- sections

def read_world_tree(r):
    box_min = r.vec()
    box_max = r.vec()
    n_nodes, terrain_depth = r.take('2I')
    # Layout: one bit per node, LSB first, depth-first; a set bit means "has 4 children".
    bits = []
    state = [0, 8]

    def node():
        if state[1] == 8:
            state[0] = r.u8()
            state[1] = 0
        sub = (state[0] >> state[1]) & 1
        state[1] += 1
        bits.append(sub)
        if sub:
            for _ in range(4):
                node()
    node()
    if len(bits) != n_nodes:
        raise FormatError('world tree: %d nodes in layout, header says %d' % (len(bits), n_nodes))
    return Obj(box_min=box_min, box_max=box_max, n_nodes=n_nodes,
               terrain_depth=terrain_depth, layout=bits)


def read_bsp(r, textures):
    """One BSP record (r at the name). Returns Bsp."""
    m = Bsp()
    m.offset = r.p
    m.name = r.str16()
    (m.info_flags, m.unk_dc, n_points, n_planes, n_surfaces, n_portals, n_polys,
     n_leaves, m.n_verts, m.vis_data_size, m.n_leaf_lists, n_nodes,
     m.n_leaf_poly_refs, m.n_lm_polys) = r.take('14I')
    m.box_min = r.vec()
    m.box_max = r.vec()
    m.translation = r.vec()
    m.unk_size = r.u32()   # replaces PC's texture-name block; meaning unknown (looks like a memory size)

    vcounts = [r.take('BB') for _ in range(n_polys)]   # (n_verts, render mode)
    m.poly_vcounts = vcounts
    if sum(a for a, _ in vcounts) != m.n_verts:
        raise FormatError('%s: vertex counts sum %d != %d' % (m.name, sum(a for a, _ in vcounts), m.n_verts))

    # leaves (same as PC)
    m.leaves = []
    for _ in range(n_leaves):
        lf = Obj(share=None, lists=[], poly_refs=None, unk=None)
        n = r.u16()
        if n == 0xFFFF:
            lf.share = r.u16()
        else:
            for _ in range(n):
                portal, size = r.take('hH')
                lf.lists.append((portal, r.raw(size)))
        npr = r.u32()
        lf.poly_refs = list(struct.unpack_from('<%dH' % (2 * npr), r.b, r.p))
        lf.poly_refs = list(zip(lf.poly_refs[0::2], lf.poly_refs[1::2]))
        r.p += 4 * npr
        lf.unk = r.f32()
        m.leaves.append(lf)

    m.planes = [r.take('4f') for _ in range(n_planes)]

    m.surfaces = []
    for _ in range(n_surfaces):
        s = Surface()
        s.O = r.vec()
        s.P = r.vec()
        s.Q = r.vec()
        s.plane, s.flags, s.texture, s.texture_flags, has_effect = r.take('IIIHH')
        s.effect = s.effect_param = None
        if has_effect:
            s.effect = r.str16()
            s.effect_param = r.str16()
        m.surfaces.append(s)

    m.polys = []
    for i in range(n_polys):
        p = Poly()
        p.index = i
        (p.lm_page, p.lm_w, p.lm_h, p.lm_x, p.lm_y, p.surface, p.plane,
         p.unk_f, cx, cy, cz, p.radius) = r.take('i4BII5f')
        p.center = (cx, cy, cz)
        p.mode = vcounts[i][1]    # 0 lightmapped, 2 bright/unlit, 3 vertex-coloured, 4 invisible
        fl = m.surfaces[p.surface].flags
        nv = vcounts[i][0]
        verts = []
        if fl & SURF_INVISIBLE:
            p.vkind = 'index'
            for pt in struct.unpack_from('<%dI' % nv, r.b, r.p):
                verts.append(Vert(point=pt, u=None, v=None, lm_u=None, lm_v=None, color=None))
            r.p += 4 * nv
        elif (fl & SURF_LIGHTMAP) and not (fl & SURF_TRANSLUCENT):
            p.vkind = 'lightmap'
            for _ in range(nv):
                pt, pad, u, v, lu, lv = r.take('HHffff')
                verts.append(Vert(point=pt, pad=pad, u=u, v=v, lm_u=lu, lm_v=lv, color=None))
        else:
            p.vkind = 'color'
            for _ in range(nv):
                pt, pad, u, v, cr, cg, cb, ca = r.take('HHff4B')
                verts.append(Vert(point=pt, pad=pad, u=u, v=v, lm_u=None, lm_v=None,
                                  color=(cr, cg, cb, ca)))
        p.verts = verts
        m.polys.append(p)

    m.nodes = [r.take('Iiii') for _ in range(n_nodes)]   # poly, leaf, side0, side1

    m.portals = []
    for _ in range(n_portals):
        name = r.str16()
        a, b = r.take('IH')            # as on PC: dword, word (seen: 0, 0x1c)
        m.portals.append(Obj(name=name, unk1=a, flags=b, center=r.vec(), dims=r.vec()))

    m.points = []
    ws = []
    for _ in range(n_points):
        x, y, z, w = r.take('4f')
        m.points.append((x, y, z))
        ws.append(w)
    m.point_w = set(ws)

    m.pblock = read_pblock(r)

    m.root_node = r.i32()
    n_sections = r.u32()
    m.terrain_sections = [read_terrain_section(r) for _ in range(n_sections)]
    return m


def read_pblock(r):
    """PBlock table (a coarse grid of poly lists, used for collision); same as PC."""
    sx, sy, sz = r.take('3I')
    pb = Obj(size=(sx, sy, sz), block_size=r.vec(), origin=r.vec())
    n_rec = r.u32()
    pb.n_data = n_rec
    pb.data = r.raw(n_rec * 6)          # n_rec records {u16 node, i16 side0, i16 side1}, block-local BSPs
    pb.blocks = [r.take('IHH') for _ in range(sx * sy * sz)]   # (0 [runtime ptr], n_records, root = n-1)
    return pb


def read_terrain_section(r):
    """One terrain section (a sub-BSP over the parent's polys), as on PC (CWorldSimple::Load)."""
    t = Obj(offset=r.p)
    n = r.u32()
    t.nodes = [r.take('Iii') for _ in range(n)]     # poly index, side0, side1 (-1 in / -2 out)
    t.root = r.i32()
    n = r.u32()
    t.polys = list(r.take('%dI' % n)) if n else []
    t.pblock = read_pblock(r)
    t.header = r.take('4BI')
    t.center = r.vec()
    return t


def read_world_models(r, textures, file_size):
    n = r.u32()
    models = []
    for i in range(n):
        start = r.p
        nxt = r.u32()
        if not (start < nxt <= file_size):
            raise FormatError('world model %d: bad next offset %d' % (i, nxt))
        m = read_bsp(r, textures)
        m.record_offset = start
        m.record_end = nxt
        if r.p != nxt:
            raise FormatError('world model %d %r: ended at %d, next record at %d' % (i, m.name, r.p, nxt))
        models.append(m)
    return models


def read_lights(r):
    n = r.u32()
    lights = []
    for _ in range(n):
        f = r.take('14f')
        lights.append(Obj(radius=f[0], color=f[1:4], unk=f[4:7], pos=f[7:10],
                          dir=f[10:13], cos_cone=f[13]))
    return lights


def read_light_grid(r):
    g = Obj()
    g.count = r.u32()
    g.dims = r.take('3I')
    g.dims_m1 = r.take('3I')
    g.stride_z = r.u32()          # dims[0]*dims[1]
    g.cell = r.vec()
    g.inv_cell = r.vec()
    g.origin = r.vec()
    if g.count != g.dims[0] * g.dims[1] * g.dims[2]:
        raise FormatError('light grid count mismatch')
    g.data = r.raw(4 * g.count)   # R, G, B, 0 per cell; x fastest, then y, then z
    return g


def clut_index(i):
    """PS2 CSM1 CLUT order: bits 3 and 4 of the index are swapped in storage."""
    return (i & ~0x18) | ((i & 0x08) << 1) | ((i & 0x10) >> 1)


def read_lm_pages(r):
    total, n = r.take('2I')
    pages = []
    for _ in range(n):
        clut = r.raw(1024)
        pix = r.raw(4096)
        pages.append(Obj(clut=clut, pixels=pix))
    return total, pages


def lm_page_rgba(page):
    """Decode a lightmap page to 64*64 (r, g, b, a) tuples, row-major. Byte semantics
    of the CLUT entries are not fully understood (see spec): returned raw."""
    pal = [tuple(page.clut[4 * clut_index(i):4 * clut_index(i) + 4]) for i in range(256)]
    return [pal[i] for i in page.pixels]


def read_objects(r):
    count, n_strings = r.take('2I')
    hdr_unk = r.take('3I2H')
    strings = [r.str16() for _ in range(n_strings)]
    objs = []
    for _ in range(count):
        dlen, ci, npr = r.take('HII')
        start = r.p
        o = Obj(cls=strings[ci], data_len=dlen, props=[])
        for _ in range(npr):
            ni, code, plen = r.take('IBH')
            q = r.p
            if code == PROP_STRING:
                val = strings[r.u32()]
            elif code in (PROP_VECTOR, PROP_COLOR):
                val = r.vec()
            elif code == PROP_REAL:
                val = r.f32()
            elif code == PROP_BOOL:
                val = r.u8()
            elif code in (PROP_FLAGS, PROP_LONGINT):
                val = r.f32()     # stored as float, like the PC
            elif code == PROP_ROTATION:
                val = r.take('4f')
            else:
                raise FormatError('unknown property code %d' % code)
            if r.p - q != plen:
                raise FormatError('property %s: length %d, read %d' % (strings[ni], plen, r.p - q))
            o.props.append(Obj(name=strings[ni], type=code, value=val))
        o.name = next((p.value for p in o.props if p.name == 'Name'), None)
        objs.append(o)
    return Obj(header_unk=hdr_unk, strings=strings, objects=objs)


# ---------------------------------------------------------------- top level

class World(Obj):
    pass


def load(path_or_bytes):
    data = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, 'rb').read()
    r = Reader(data)
    w = World()
    w.size = len(data)
    w.sections = []

    def sec(name, start):
        w.sections.append((name, start, r.p - start))

    s = r.p
    w.header = r.take('11I')
    (w.version, w.object_pos, w.lightmap_pos, w.hdr_unk3, w.hdr_unk4,
     w.lights_pos, w.grid_pos) = w.header[:7]
    if w.version != 66:
        raise FormatError('version %d' % w.version)
    sec('header', s)

    s = r.p
    w.info = r.str32()
    w.lm_grid_size = r.f32()
    w.extents_min = r.vec()
    w.extents_max = r.vec()
    sec('world info', s)

    s = r.p
    w.tree = read_world_tree(r)
    sec('world tree', s)

    s = r.p
    n_tex, tex_bytes = r.take('2I')
    t0 = r.p
    w.textures = [r.str16() for _ in range(n_tex)]
    if r.p - t0 != tex_bytes:
        raise FormatError('texture list size %d != %d' % (r.p - t0, tex_bytes))
    sec('texture list', s)

    s = r.p
    w.models = read_world_models(r, w.textures, len(data))
    sec('world models (%d BSPs)' % len(w.models), s)
    w.main = next((m for m in w.models if m.info_flags & WI_MAINWORLD), None)
    w.vis = next((m for m in w.models if m.info_flags & WI_VIS), None)

    for name, pos, fn, attr in (('static lights', w.lights_pos, read_lights, 'lights'),
                                ('light grid', w.grid_pos, read_light_grid, 'light_grid')):
        if r.p != pos:
            raise FormatError('%s expected at %d, at %d' % (name, pos, r.p))
        s = r.p
        setattr(w, attr, fn(r))
        sec(name, s)

    if r.p != w.lightmap_pos:
        raise FormatError('lightmap pages expected at %d, at %d' % (w.lightmap_pos, r.p))
    s = r.p
    w.lm_total, w.lm_pages = read_lm_pages(r)
    if w.lm_total != r.p - s:
        raise FormatError('lightmap section size')
    sec('lightmap pages', s)

    if r.p != w.object_pos:
        raise FormatError('objects expected at %d, at %d' % (w.object_pos, r.p))
    s = r.p
    od = read_objects(r)
    w.object_strings = od.strings
    w.object_header_unk = od.header_unk
    w.objects = od.objects
    sec('objects', s)

    if r.p != len(data):
        raise FormatError('%d bytes left at EOF' % (len(data) - r.p))
    return w


# ---------------------------------------------------------------- checks / export

def bsp_bbox(m):
    if not m.points:
        return None
    xs, ys, zs = zip(*m.points)
    return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))


def planarity(m):
    """max |n.p - d| over all poly vertices, and how many polys exceed 0.5 units."""
    worst = 0.0
    bad = 0
    for p in m.polys:
        nx, ny, nz, d = m.planes[p.plane]
        e = max(abs(nx * x + ny * y + nz * z - d) for x, y, z in m.poly_xyz(p))
        worst = max(worst, e)
        if e > 0.5:
            bad += 1
    return worst, bad


def write_obj(w, models, path, mtl_name=None):
    """Write the given BSPs to a Wavefront OBJ: one 'o' per BSP, 'usemtl' = texture name,
    vt = base texture UV (v flipped for OBJ). Invisible-surface polys get no UVs."""
    out = open(path, 'w')
    if mtl_name:
        out.write('mtllib %s\n' % mtl_name)
    base = 1
    tbase = 1
    used = set()
    for m in models:
        out.write('o %s\n' % m.name)
        for x, y, z in m.points:
            out.write('v %.4f %.4f %.4f\n' % (x, y, z))
        # group polys by texture
        by_tex = {}
        for p in m.polys:
            by_tex.setdefault(m.surfaces[p.surface].texture, []).append(p)
        for ti, polys in sorted(by_tex.items()):
            tname = w.textures[ti] if 0 <= ti < len(w.textures) else 'tex%d' % ti
            used.add(tname)
            out.write('g %s__%s\nusemtl %s\n' % (m.name, tname.replace('\\', '/'), tname.replace('\\', '/')))
            for p in polys:
                if p.vkind == 'index':
                    out.write('f ' + ' '.join(str(base + v.point) for v in p.verts) + '\n')
                    continue
                for v in p.verts:
                    out.write('vt %.5f %.5f\n' % (v.u, -v.v))
                out.write('f ' + ' '.join('%d/%d' % (base + v.point, tbase + k)
                                          for k, v in enumerate(p.verts)) + '\n')
                tbase += len(p.verts)
        base += len(m.points)
    out.close()
    return used


def summary(w, out=sys.stdout):
    pr = lambda *a: print(*a, file=out)
    pr('file size', w.size, 'version', w.version)
    pr('sections:')
    for name, off, size in w.sections:
        pr('  %-28s @%9d  %9d bytes' % (name, off, size))
    pr('total accounted', sum(s for _, _, s in w.sections))
    pr('world info: %r  lm grid %g  extents %s %s' % (w.info, w.lm_grid_size, w.extents_min, w.extents_max))
    pr('world tree nodes', w.tree.n_nodes)
    pr('textures', len(w.textures))
    pr('world models', len(w.models))
    for i, m in enumerate(w.models):
        pr('  %3d %-26s flags %#04x pts %5d planes %5d surfs %3d polys %5d verts %5d leaves %4d nodes %5d portals %d pblock %s/%d terrain %d'
           % (i, m.name, m.info_flags, len(m.points), len(m.planes), len(m.surfaces), len(m.polys), m.n_verts,
              len(m.leaves), len(m.nodes), len(m.portals), 'x'.join(map(str, m.pblock.size)), m.pblock.n_data,
              len(m.terrain_sections)))
    if w.main:
        bb = bsp_bbox(w.main)
        pr('main world (%s) bbox %s .. %s' % (w.main.name, bb[0], bb[1]))
        pr('main world planarity (max dist, polys > 0.5):', planarity(w.main))
    pr('static lights', len(w.lights), ' light grid', w.light_grid.dims, 'cell', w.light_grid.cell)
    pr('lightmap pages', len(w.lm_pages))
    pr('objects', len(w.objects))
    for c, n in Counter(o.cls for o in w.objects).most_common():
        pr('  %5d %s' % (n, c))


def main(argv):
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--obj')
    ap.add_argument('--model', action='append', help='BSP name(s) to export (default: main world)')
    ap.add_argument('--textures', action='store_true')
    ap.add_argument('--models', action='store_true', help='list world-model names')
    a = ap.parse_args(argv)
    w = load(a.file)
    summary(w)
    if True:
        used = Counter()
        for m in w.models:
            for p in m.polys:
                used[m.surfaces[p.surface].texture] += 1
        print('texture list (index, polys using it, name):')
        for i, t in enumerate(w.textures):
            print('  %3d %5d %s' % (i, used[i], t))
    if a.obj:
        names = a.model or [w.main.name]
        ms = [m for m in w.models if m.name in names]
        write_obj(w, ms, a.obj)
        print('wrote', a.obj, [m.name for m in ms])


if __name__ == '__main__':
    main(sys.argv[1:])
