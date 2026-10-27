#!/usr/bin/env python3
"""Walk a PC NOLF (LithTech 2.x, version 66) world .dat section by section.

Follows build/ps2/spec-pc-world.md, which follows the engine
reconstruction:
  header ............ commonmodel.cpp w_ReadWorldHeader
  info/extents/tree . commonmodel.cpp CServerWorldFull::Load, worldtree.cpp LoadLayout
  world models ...... worldbsp.cpp w_LoadWorldModel, CWorldSimple::Load, worldmodels.cpp CSubGrid::Load
  object list ....... servermodel.cpp sm_LoadWorldObjects, commonmodel.cpp w_ReadWorldLights
  lightmap anims .... worldmodels.cpp CWorldData::LoadObjects

Usage: pcworld.py [-v] world.dat [world.dat ...]
Every byte is accounted for: the script reports gaps/overlaps between sections
and whether the last section ends at EOF.
"""
import struct
import sys
from collections import Counter


class R:
    def __init__(self, data, pos=0):
        self.d = data
        self.p = pos

    def u8(self):
        v = self.d[self.p]; self.p += 1; return v

    def u16(self):
        v = struct.unpack_from('<H', self.d, self.p)[0]; self.p += 2; return v

    def i16(self):
        v = struct.unpack_from('<h', self.d, self.p)[0]; self.p += 2; return v

    def u32(self):
        v = struct.unpack_from('<I', self.d, self.p)[0]; self.p += 4; return v

    def i32(self):
        v = struct.unpack_from('<i', self.d, self.p)[0]; self.p += 4; return v

    def f32(self):
        v = struct.unpack_from('<f', self.d, self.p)[0]; self.p += 4; return v

    def vec(self):
        v = struct.unpack_from('<3f', self.d, self.p); self.p += 12; return v

    def raw(self, n):
        v = self.d[self.p:self.p + n]
        if len(v) != n:
            raise EOFError('short read at %d (+%d)' % (self.p, n))
        self.p += n; return v

    def lstr(self):
        """ILTStream::ReadString: uint16 length, then that many chars, no NUL."""
        n = self.u16()
        return self.raw(n).decode('latin-1')


def parse_bsp(r, verbose=False):
    """One world-model record (w_LoadWorldModel)."""
    b = {}
    start = r.p
    b['junk8'] = [r.u32() for _ in range(8)]
    b['flags'] = r.u32()            # WorldBsp+0x48 world info flags
    b['dc'] = r.u32()               # WorldBsp+0xdc
    b['name'] = r.lstr()            # ReadString(m_Name, 0x40)
    (nPoints, nPlanes, nSurfaces, nPortals, nPolies, nLeafs, nUnk,
     visBytes, nLeafLists, nNodes, nLeafPolyRefs, nEdges) = [r.u32() for _ in range(12)]
    b.update(nPoints=nPoints, nPlanes=nPlanes, nSurfaces=nSurfaces,
             nPortals=nPortals, nPolies=nPolies, nLeafs=nLeafs, nUnk=nUnk,
             visBytes=visBytes, nLeafLists=nLeafLists, nNodes=nNodes,
             nLeafPolyRefs=nLeafPolyRefs, nEdges=nEdges)
    b['boxmin'] = r.vec(); b['boxmax'] = r.vec(); b['translation'] = r.vec()

    # texture name table
    nStrBytes = r.u32(); nTex = r.u32()
    s0 = r.p
    names = []
    for _ in range(nTex):
        e = r.d.index(b'\0', r.p)
        names.append(r.d[r.p:e].decode('latin-1')); r.p = e + 1
    assert r.p - s0 <= nStrBytes, 'texture names overflow pool'
    b['texStrBytes'] = nStrBytes; b['textures'] = names
    b['texPad'] = nStrBytes - (r.p - s0)   # the engine reads exactly the strings; pool may be larger

    # per-poly vertex counts (read twice by the engine: sizing pass + carve pass)
    vcounts = [(r.u8(), r.u8()) for _ in range(nPolies)]
    b['fixverts_polys'] = sum(1 for a, c in vcounts if c)
    b['sumVerts'] = sum(a for a, c in vcounts)
    b['sumAllVerts'] = sum(a + c for a, c in vcounts)
    b['maxverts'] = max([a for a, c in vcounts] or [0])

    # leaves
    sumEntries = 0; sumVis = 0; sumRefs = 0; shared = 0
    for i in range(nLeafs):
        w = r.u16()
        if w == 0xffff:
            idx = r.u16(); assert idx < i or True; shared += 1
        else:
            for _ in range(w):
                portal = r.u16(); size = r.u16(); r.raw(size)
                sumEntries += 1; sumVis += size
        n = r.u32()
        for _ in range(n):
            w = r.u16(); p = r.u16()
            b.setdefault('leafrefs', Counter())[w] += 1
        sumRefs += n
        b.setdefault('leafFloat', Counter())[round(r.f32(), 3)] += 1
    assert sumEntries <= nLeafLists, (sumEntries, nLeafLists)
    assert sumVis <= visBytes, (sumVis, visBytes)
    assert sumRefs <= nLeafPolyRefs, (sumRefs, nLeafPolyRefs)
    b['leafListsUsed'] = sumEntries; b['visUsed'] = sumVis
    b['leafRefsUsed'] = sumRefs; b['sharedLeaves'] = shared

    # planes
    planes = [struct.unpack_from('<4f', r.raw(16)) for _ in range(nPlanes)]

    # surfaces
    sflags = Counter(); effects = 0; texidx_bad = 0
    for i in range(nSurfaces):
        r.vec(); r.vec(); r.vec()
        tex = r.u16()
        plane = r.u32()
        assert plane < nPlanes, 'surface plane out of range'
        if tex >= nTex:
            texidx_bad += 1
        fl = r.u32(); sflags[fl] += 1
        b.setdefault('s44', Counter())[r.u8()] += 1   # +0x44
        b.setdefault('sRGB', Counter())[(r.u8(), r.u8(), r.u8())] += 1  # ignored colour
        has = r.u8()
        if has == 1:
            effects += 1
            r.lstr(); r.lstr()      # effect class, params
        b.setdefault('texFlags', Counter())[r.u16()] += 1   # texture flags (surface type)
    b['surfaceFlags'] = sflags; b['surfEffects'] = effects; b['surfBadTex'] = texidx_bad

    # polies
    edgeTotal = 0
    for i in range(nPolies):
        r.vec()                     # lightmap origin (+0x34)
        lw = r.u16(); lh = r.u16()  # lightmap w/h (stored as bytes)
        b.setdefault('lmSizes', Counter())[(lw, lh)] += 1
        ne = r.u16(); edgeTotal += ne
        for _ in range(ne):
            a = r.u16(); k = r.u16()
            b.setdefault('lmrefs', []).append((i, a, k))
        surf = r.u32(); assert surf < nSurfaces, 'poly surface out of range'
        nv = vcounts[i][0] + vcounts[i][1]
        for _ in range(nv):
            pt = r.u16(); assert pt < nPoints, 'poly point out of range'
            r.u8(); r.u8(); r.u8()
    assert edgeTotal <= nEdges, (edgeTotal, nEdges)
    b['edgesUsed'] = edgeTotal

    # nodes
    term = Counter()
    nodes = []
    for i in range(nNodes):
        p = r.u32(); assert p < nPolies, 'node poly out of range'
        leaf = r.u16()              # leaf index (0xffff none)
        assert leaf == 0xffff or leaf < nLeafs, 'node leaf out of range'
        cs = []
        for _ in range(2):
            c = r.i32()
            if c < 0:
                assert c in (-1, -2); term[c] += 1
            else:
                assert c < nNodes
            cs.append(c)
        nodes.append((p, leaf, cs[0], cs[1]))
    b['nodes'] = nodes
    b['nodesWithLeaf'] = sum(1 for n in nodes if n[1] != 0xffff)
    b['nodeTerminals'] = dict(term)

    # user portals
    for i in range(nPortals):
        r.lstr(); r.u32(); r.u16(); r.vec(); r.vec()

    # points (position + an ignored normal)
    for i in range(nPoints):
        r.vec(); b.setdefault('ptNormal', Counter())[r.vec()] += 1

    # PBlock table (CSubGrid::Load)
    cx, cy, cz = r.i32(), r.i32(), r.i32()
    csize = r.vec(); corg = r.vec()
    ncells = cx * cy * cz
    nodes6 = 0; empty = 0
    cells = []
    for i in range(ncells):
        cnt = r.u16(); root = r.u16()
        recs = [struct.unpack_from('<3H', r.raw(6)) for _ in range(cnt)]
        nodes6 += cnt
        if cnt == 0:
            empty += 1
        # WSNode {u16 bspNode, u16 back, u16 front}; children index this cell's run
        # or are 0xfffe (empty) / 0xffff (solid); root likewise
        assert root in (0xfffe, 0xffff) or root < cnt, ('pblock root', root, cnt)
        for (bn, c0, c1) in recs:
            assert bn < nNodes, 'pblock node -> bsp node out of range'
            for c in (c0, c1):
                assert c in (0xfffe, 0xffff) or c < cnt
        cells.append((root, recs))
    b['cells'] = cells
    b['pblock'] = dict(dims=(cx, cy, cz), cell=csize, origin=corg,
                       cells=ncells, nodes=nodes6, emptyCells=empty)

    root = r.i32()
    assert root in (-1, -2) or 0 <= root < nNodes
    b['root'] = root

    nTerrain = r.u32()
    b['nTerrain'] = nTerrain
    for t in range(nTerrain):
        nn = r.u32()
        for _ in range(nn):
            r.u32(); r.i32(); r.i32()
        r.i32()
        np_ = r.u32()
        for _ in range(np_):
            r.u32()
        cx, cy, cz = r.i32(), r.i32(), r.i32(); r.vec(); r.vec()
        for i in range(cx * cy * cz):
            cnt = r.u16(); r.u16(); r.raw(cnt * 6)
        r.raw(8)                    # wb_ReadSimpleHeader: 4 bytes + 1 dword
        r.vec()                     # centre
    b['size'] = r.p - start
    return b


def parse_world(path, verbose=False, data=None):
    d = data if data is not None else open(path, 'rb').read()
    r = R(d)
    sec = []   # (name, start, end)
    out = {'file': path, 'size': len(d)}

    s = r.p
    version = r.u32(); assert version == 66, version
    objOfs = r.u32(); lmOfs = r.u32()
    hdr_unused = [r.u32() for _ in range(8)]
    sec.append(('header', s, r.p))
    out.update(version=version, objOfs=objOfs, lmOfs=lmOfs, hdrUnused=hdr_unused)

    s = r.p
    n = r.u32(); info = r.raw(n).decode('latin-1')
    lmgrid = r.f32(); emin = r.vec(); emax = r.vec()
    sec.append(('worldinfo+lmgrid+extents', s, r.p))
    out.update(info=info, lmGrid=lmgrid, extMin=emin, extMax=emax)

    s = r.p
    tmin = r.vec(); tmax = r.vec(); tUnused = r.u32(); tDepth = r.u32()
    # quadtree bit stream: preorder, 1 bit per node, LSB first, bytes read lazily
    cur = [0, 8]
    count = [0, 0]

    def node():
        if cur[1] == 8:
            cur[0] = r.u8(); cur[1] = 0
        bit = (cur[0] >> cur[1]) & 1
        cur[1] += 1
        count[0] += 1
        if bit:
            count[1] += 1
            for _ in range(4):
                node()
    node()
    sec.append(('worldtree', s, r.p))
    out.update(treeMin=tmin, treeMax=tmax, treeUnused=tUnused, treeDepthDword=tDepth,
               treeNodes=count[0], treeInner=count[1])

    s = r.p
    nWorlds = r.u32()
    sec.append(('nWorlds', s, r.p))
    worlds = []
    for i in range(nWorlds):
        s = r.p
        nxt = r.u32()
        b = parse_bsp(r, verbose)
        b['next'] = nxt
        b['slack'] = nxt - r.p       # bytes between record end and next offset
        worlds.append(b)
        sec.append(('world[%d] %s' % (i, b['name']), s, r.p))
        if nxt != r.p:
            sec.append(('world[%d] SLACK' % i, r.p, nxt))
        r.p = nxt
    out['worlds'] = worlds

    # object list
    assert r.p == objOfs, ('object list not right after last world', r.p, objOfs)
    s = r.p
    nObj = r.u32()
    classes = Counter(); ptypes = Counter(); propcount = 0
    objs = []
    for i in range(nObj):
        rs = r.u16(); p0 = r.p
        cls = r.lstr(); classes[cls] += 1
        nprops = r.u32()
        props = {}
        for j in range(nprops):
            pn = r.lstr(); pt = r.u8(); pf = r.u32(); pl = r.u16()
            ptypes[pt] += 1; propcount += 1
            if pt == 0:
                q = r.p
                sv = r.lstr()
                assert r.p - q == pl, ('string prop length', pn, pl, r.p - q)
                props[pn] = sv
            else:
                props[pn] = r.raw(pl)
        assert r.p - p0 == rs, ('record size mismatch', cls, rs, r.p - p0)
        objs.append((cls, props))
    sec.append(('objects', s, r.p))
    out.update(nObjects=nObj, classes=classes, propTypes=ptypes, nProps=propcount)
    out['objs'] = objs

    if r.p != lmOfs:
        sec.append(('GAP before lightmaps', r.p, lmOfs))
    r.p = lmOfs
    s = r.p
    nRefs, nFramePtrs, nData, nEntries, nAnims = [r.u32() for _ in range(5)]
    anims = []
    tRefs = tFrames = tData = tEnt = 0
    for a in range(nAnims):
        name = r.lstr()
        shadow = r.u32(); nfr = r.u32(); npol = r.u32()
        refs = [(r.u16(), r.u16()) for _ in range(npol)]
        out.setdefault('animrefs', []).append(refs)
        dbytes = 0
        for f in range(nfr):
            for p in range(npol):
                sz = r.u32(); r.raw(sz); dbytes += sz
        tRefs += npol; tFrames += nfr; tEnt += nfr * npol; tData += dbytes
        anims.append((name, shadow, nfr, npol, dbytes))
    assert tRefs <= nRefs and tFrames <= nFramePtrs and tData <= nData and tEnt <= nEntries
    sec.append(('lightanims', s, r.p))
    out.update(lmTotals=(nRefs, nFramePtrs, nData, nEntries, nAnims),
               lmUsed=(tRefs, tFrames, tData, tEnt), anims=anims)
    # cross-check: every poly's light-anim back-reference {iAnim, iSlot} must point
    # at the anim poly-ref {world, poly} that names that very poly
    bad = 0; n = 0
    for wi, b in enumerate(worlds):
        for (pi, a, k) in b.get('lmrefs', []):
            n += 1
            refs = out['animrefs'][a] if a < len(out.get('animrefs', [])) else None
            if not refs or k >= len(refs) or refs[k] != (wi, pi):
                bad += 1
    out['lmBackrefs'] = (n, bad)
    out['end'] = r.p
    out['sections'] = sec
    return out


def report(o, verbose=False):
    print('=' * 72)
    print('%s: %d bytes, version %d, objOfs %d, lmOfs %d, unused hdr %s'
          % (o['file'], o['size'], o['version'], o['objOfs'], o['lmOfs'], o['hdrUnused']))
    print('info string: %r' % o['info'][:200])
    print('lmGrid %g  extents %s .. %s' % (o['lmGrid'], o['extMin'], o['extMax']))
    print('worldtree box %s..%s dwords (%d, %d), %d nodes (%d with children)'
          % (o['treeMin'], o['treeMax'], o['treeUnused'], o['treeDepthDword'],
             o['treeNodes'], o['treeInner']))
    print('%d world models' % len(o['worlds']))
    tot = Counter()
    for i, b in enumerate(o['worlds']):
        for k in ('nPoints', 'nPlanes', 'nSurfaces', 'nPortals', 'nPolies', 'nLeafs',
                  'nLeafLists', 'visBytes', 'nNodes', 'nLeafPolyRefs', 'nEdges'):
            tot[k] += b[k]
        tot['nTextures'] += len(b['textures'])
        tot['pblockCells'] += b['pblock']['cells']; tot['pblockNodes'] += b['pblock']['nodes']
        if verbose or i == 0:
            print('  [%d] %-28s flags %#04x dc %#x | pts %d planes %d surfs %d portals %d polys %d '
                  'leafs %d unk %d vis %d/%d leaflists %d/%d nodes %d leafrefs %d/%d edges %d/%d '
                  'tex %d (pool %d, pad %d) eff %d badTex %d fix %d maxv %d root %d terr %d slack %d'
                  % (i, b['name'], b['flags'], b['dc'], b['nPoints'], b['nPlanes'], b['nSurfaces'],
                     b['nPortals'], b['nPolies'], b['nLeafs'], b['nUnk'], b['visUsed'], b['visBytes'],
                     b['leafListsUsed'], b['nLeafLists'], b['nNodes'], b['leafRefsUsed'],
                     b['nLeafPolyRefs'], b['edgesUsed'], b['nEdges'], len(b['textures']),
                     b['texStrBytes'], b['texPad'], b['surfEffects'], b['surfBadTex'],
                     b['fixverts_polys'], b['maxverts'], b['root'], b['nTerrain'], b['slack']))
            print('       junk8 %s' % b['junk8'])
            print('       pblock %s' % b['pblock'])
            print('       nodeTerminals %s' % b['nodeTerminals'])
            print('       surface flags %s' % ', '.join('%#x:%d' % kv for kv in sorted(b['surfaceFlags'].items())))
    flagc = Counter(b['flags'] for b in o['worlds'])
    print('  world-model flags histogram: %s' % ', '.join('%#x:%d' % kv for kv in sorted(flagc.items())))
    print('  totals: %s' % dict(tot))
    print('objects: %d, props %d, prop types %s' % (o['nObjects'], o['nProps'], dict(o['propTypes'])))
    print('  top classes: %s' % o['classes'].most_common(12))
    print('lightanims: header totals %s used %s; poly back-refs %d (%d inconsistent)'
          % (o['lmTotals'], o['lmUsed'], o['lmBackrefs'][0], o['lmBackrefs'][1]))
    for a in o['anims'][:5]:
        print('   %s' % (a,))
    if len(o['anims']) > 5:
        print('   ... %d more' % (len(o['anims']) - 5))
    print('sections:')
    last = 0
    for name, s, e in o['sections']:
        flag = '' if s == last else '   <-- %+d from previous end' % (s - last)
        if verbose or 'world[' not in name or name.startswith('world[0]') or 'SLACK' in name:
            print('  %10d %10d %10d  %s%s' % (s, e, e - s, name, flag))
        last = e
    print('end of parse %d, file size %d -> %s' % (o['end'], o['size'],
          'EXACT' if o['end'] == o['size'] else 'MISMATCH (%+d)' % (o['size'] - o['end'])))


def walk_rez(rezpath, pattern='WORLDS/'):
    """Parse every world .dat inside a .rez (tools/rezextract.py), one summary line each."""
    import os
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'tools'))
    from rezextract import Rez
    rz = Rez(rezpath)
    f = open(rezpath, 'rb')
    for p, pos, sz in rz.walk():
        if not (p.upper().startswith(pattern.upper()) and p.upper().endswith('.DAT')):
            continue
        f.seek(pos); data = f.read(sz)
        try:
            o = parse_world(p, data=data)
            print('%-50s %9d %s worlds %4d objs %5d anims %3d backrefs-bad %d' % (
                p, sz, 'EXACT' if o['end'] == sz else 'MISMATCH%+d' % (sz - o['end']),
                len(o['worlds']), o['nObjects'], len(o['anims']), o['lmBackrefs'][1]))
        except Exception as e:
            print('%-50s %9d FAILED %r' % (p, sz, e))


if __name__ == '__main__':
    args = sys.argv[1:]
    v = '-v' in args
    if args and args[0] == '--rez':
        walk_rez(args[1])
        sys.exit(0)
    for a in [x for x in args if x != '-v']:
        report(parse_world(a, v), v)
