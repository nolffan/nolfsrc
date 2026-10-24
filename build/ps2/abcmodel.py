#!/usr/bin/env python3
"""NOLF PC model file (.abc, versions 11/12): an in-memory model, a reader and a
writer. The layout is in spec-models.md. The writer is byte-exact against the
shipped PC files (abcmodel.py check FILE... reads, rewrites and compares).

  abcmodel.py check FILE...   round-trip test
  abcmodel.py dump FILE       summary
"""
import struct, sys


class Node:
    def __init__(self, name='', index=0, flags=0, matrix=None, nchildren=0):
        self.name, self.index, self.flags = name, index, flags
        # 4x4 row-major, rows are (r0..r3); translation in column 3
        self.matrix = matrix or [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
        self.nchildren = nchildren


class Vertex:
    def __init__(self, pos, normal, weights, sublod=0xCDCD):
        self.pos, self.normal = pos, normal
        self.weights = weights          # [(node, (x,y,z) node-space pos, bias)]
        self.sublod = sublod


class LOD:
    def __init__(self):
        self.faces = []                 # [((u,v,vi), (u,v,vi), (u,v,vi))]
        self.verts = []


class Piece:
    def __init__(self, name='', material=0, spec_power=0.0, spec_scale=1.0,
                 lod_weight=1.0, unknown=0):
        self.name, self.material = name, material
        self.spec_power, self.spec_scale, self.lod_weight = spec_power, spec_scale, lod_weight
        self.unknown = unknown
        self.lods = []


class ChildModel:
    def __init__(self, name='', build=0, xforms=None):
        self.name, self.build = name, build
        self.xforms = xforms or []      # per node: ((x,y,z), (qx,qy,qz,qw))


class Anim:
    def __init__(self, name='base', dims=(0, 0, 0)):
        self.name, self.dims = name, dims
        self.val = -1                   # v12: always -1 in what we write
        self.interp = 200
        self.keys = []                  # [(time ms, command string)]
        self.tracks = []                # per node: [((x,y,z), (qx,qy,qz,qw))] per key


class Socket:
    def __init__(self, node, name, rot, pos):
        self.node, self.name, self.rot, self.pos = node, name, rot, pos


class Model:
    def __init__(self):
        self.version = 12
        self.command = ''
        self.radius = 0.0
        self.lod_dists = []
        self.nodes, self.pieces, self.weight_sets = [], [], []
        self.children = [ChildModel()]   # [0] is the model itself (empty name)
        self.anims, self.sockets = [], []
        self.bindings = []               # [(name, dims, origin)] for own anims
        self.child_bindings = []         # one list per children[1:]
        self.lod_count = 1
        # header counts as read (None = compute when writing)
        self.hdr = None

    # ---- counts the header carries
    def strings(self):
        """The distinct names the engine copies into the model's string pool:
        the command string, node, animation, keyframe-command and child-model
        names, de-duplicated case-sensitively, empty ones not counted (piece,
        socket and weight-set names go into fixed buffers). This reproduces the
        header's two string counts of every v12 .abc NOLF ships."""
        seen, s = [], set()
        for n in ([self.command] + [x.name for x in self.nodes] +
                  [a.name for a in self.anims] +
                  [k[1] for a in self.anims for k in a.keys] +
                  [c.name for c in self.children]):
            if n and n not in s:
                s.add(n)
                seen.append(n)
        return seen

    def counts(self):
        st = self.strings()
        nfaces = sum(len(l.faces) for p in self.pieces for l in p.lods)
        nverts = sum(len(l.verts) for p in self.pieces for l in p.lods)
        nweights = sum(len(v.weights) for p in self.pieces for l in p.lods for v in l.verts)
        return [sum(len(a.keys) for a in self.anims), len(self.anims), len(self.nodes),
                len(self.pieces), len(self.children), nfaces, nverts, nweights,
                self.lod_count, len(self.sockets), len(self.weight_sets),
                len(st), sum(len(x) for x in st)]


# ------------------------------------------------------------------ reading

class R:
    def __init__(self, d, o=0):
        self.d, self.o = d, o

    def u(self, fmt):
        v = struct.unpack_from('<' + fmt, self.d, self.o)
        self.o += struct.calcsize('<' + fmt)
        return v if len(v) > 1 else v[0]

    def s(self):
        n = self.u('H')
        v = self.d[self.o:self.o + n].decode('latin-1')
        self.o += n
        return v


def read_abc(data):
    r = R(data)
    m = Model()
    secs = {}
    while True:
        name = r.s()
        nxt = r.u('I')
        secs[name] = r.o
        if name == 'Header':
            m.version = r.u('I')
            c = list(r.u('11I'))
            if m.version >= 13:
                r.u('I')
            c += list(r.u('2I'))
            m.hdr = c
            m.lod_count = c[8]
            m.command = r.s()
            m.radius = r.u('f')
            nd = r.u('I')
            r.o += 60
            m.lod_dists = list(r.u('%df' % nd)) if nd > 1 else ([r.u('f')] if nd else [])
        elif name == 'Pieces':
            r.u('I')                     # weight count
            for _ in range(r.u('I')):
                p = Piece()
                p.material, p.spec_power, p.spec_scale = r.u('H'), r.u('f'), r.u('f')
                if m.version > 9:
                    p.lod_weight = r.u('f')
                p.unknown = r.u('H')
                p.name = r.s()
                for _ in range(m.lod_count):
                    l = LOD()
                    for _ in range(r.u('I')):
                        fv = []
                        for _ in range(3):
                            u, v = r.u('2f')
                            fv.append((u, v, r.u('H')))
                        l.faces.append(tuple(fv))
                    for _ in range(r.u('I')):
                        nw, sub = r.u('2H')
                        ws = []
                        for _ in range(nw):
                            ni = r.u('I')
                            ws.append((ni, r.u('3f'), r.u('f')))
                        l.verts.append(Vertex(r.u('3f'), r.u('3f'), ws, sub))
                    p.lods.append(l)
                m.pieces.append(p)
        elif name == 'Nodes':
            for _ in range(c[2]):
                n = Node(r.s(), r.u('H'), r.u('B'))
                f = r.u('16f')
                n.matrix = [list(f[0:4]), list(f[4:8]), list(f[8:12]), list(f[12:16])]
                n.nchildren = r.u('I')
                m.nodes.append(n)
            for _ in range(r.u('I')):
                wn = r.s()
                k = r.u('I')
                m.weight_sets.append((wn, list(r.u('%df' % k)) if k > 1 else ([r.u('f')] if k else [])))
        elif name == 'ChildModels':
            m.children = []
            for _ in range(r.u('H')):
                cm = ChildModel(r.s(), r.u('I'))
                for _ in range(c[2]):
                    cm.xforms.append((r.u('3f'), r.u('4f')))
                m.children.append(cm)
        elif name == 'Animation':
            for _ in range(r.u('I')):
                a = Anim()
                a.dims = r.u('3f')
                a.name = r.s()
                a.val = r.u('i')
                if m.version >= 12:
                    a.interp = r.u('I')
                    nk = r.u('I')
                else:
                    nk = r.u('I') if a.val == -1 else a.val
                for _ in range(nk):
                    t = r.u('I')
                    a.keys.append((t, r.s()))
                for _ in range(c[2]):
                    a.tracks.append([(r.u('3f'), r.u('4f')) for _ in range(nk)])
                m.anims.append(a)
        elif name == 'Sockets':
            for _ in range(r.u('I')):
                ni = r.u('I')
                sn = r.s()
                m.sockets.append(Socket(ni, sn, r.u('4f'), r.u('3f')))
        elif name == 'AnimBindings':
            def binds():
                return [(r.s(), r.u('3f'), r.u('3f')) for _ in range(r.u('I'))]
            m.bindings = binds()
            m.child_bindings = [binds() for _ in m.children[1:]]
        if nxt == 0xFFFFFFFF:
            break
        r.o = nxt
    m.sections = secs
    return m


# ------------------------------------------------------------------ writing

class W:
    def __init__(self):
        self.b = bytearray()

    def p(self, fmt, *v):
        self.b += struct.pack('<' + fmt, *v)

    def s(self, txt):
        e = txt.encode('latin-1')
        self.p('H', len(e))
        self.b += e

    def section(self, name):
        if hasattr(self, 'last'):
            struct.pack_into('<I', self.b, self.last, len(self.b))
        self.s(name)
        self.last = len(self.b)
        self.p('I', 0xFFFFFFFF)


def write_abc(m):
    w = W()
    c = m.counts()
    w.section('Header')
    w.p('I', m.version)
    w.p('11I', *c[:11])
    w.p('2I', *c[11:])
    w.s(m.command)
    w.p('f', m.radius)
    w.p('I', len(m.lod_dists))
    w.b += bytes(60)
    for x in m.lod_dists:
        w.p('f', x)

    w.section('Pieces')
    w.p('I', c[7])
    w.p('I', len(m.pieces))
    for p in m.pieces:
        w.p('Hfff', p.material, p.spec_power, p.spec_scale, p.lod_weight)
        w.p('H', p.unknown)
        w.s(p.name)
        assert len(p.lods) == m.lod_count
        for l in p.lods:
            w.p('I', len(l.faces))
            for f in l.faces:
                for u, v, vi in f:
                    w.p('2fH', u, v, vi)
            w.p('I', len(l.verts))
            for v in l.verts:
                w.p('2H', len(v.weights), v.sublod)
                for ni, loc, bias in v.weights:
                    w.p('I3ff', ni, *loc, bias)
                w.p('3f', *v.pos)
                w.p('3f', *v.normal)

    w.section('Nodes')
    for n in m.nodes:
        w.s(n.name)
        w.p('HB', n.index, n.flags)
        for row in n.matrix:
            w.p('4f', *row)
        w.p('I', n.nchildren)
    w.p('I', len(m.weight_sets))
    for wn, ws in m.weight_sets:
        w.s(wn)
        w.p('I', len(ws))
        for x in ws:
            w.p('f', x)

    w.section('ChildModels')
    w.p('H', len(m.children))
    for cm in m.children:
        w.s(cm.name)
        w.p('I', cm.build)
        assert len(cm.xforms) == len(m.nodes)
        for pos, q in cm.xforms:
            w.p('3f4f', *pos, *q)

    w.section('Animation')
    w.p('I', len(m.anims))
    for a in m.anims:
        w.p('3f', *a.dims)
        w.s(a.name)
        w.p('i', a.val)
        w.p('II', a.interp, len(a.keys))
        for t, cmd in a.keys:
            w.p('I', t)
            w.s(cmd)
        assert len(a.tracks) == len(m.nodes)
        for tr in a.tracks:
            assert len(tr) == len(a.keys)
            for pos, q in tr:
                w.p('3f4f', *pos, *q)

    w.section('Sockets')
    w.p('I', len(m.sockets))
    for s in m.sockets:
        w.p('I', s.node)
        w.s(s.name)
        w.p('4f', *s.rot)
        w.p('3f', *s.pos)

    w.section('AnimBindings')
    for bl in [m.bindings] + m.child_bindings:
        w.p('I', len(bl))
        for name, dims, origin in bl:
            w.s(name)
            w.p('3f', *dims)
            w.p('3f', *origin)
    return bytes(w.b)


def main(argv):
    if argv[0] == 'check':
        bad = 0
        for fn in argv[1:]:
            d = open(fn, 'rb').read()
            m = read_abc(d)
            out = write_abc(m)
            ok = out == d
            hc = m.counts() == m.hdr
            if not ok or not hc:
                bad += 1
                i = next((i for i in range(min(len(d), len(out))) if d[i] != out[i]), None)
                print('DIFF %s len %d/%d first diff at %s; header counts %s vs computed %s'
                      % (fn, len(d), len(out), i, m.hdr, m.counts()))
        print('%d files, %d differ' % (len(argv) - 1, bad))
    elif argv[0] == 'dump':
        m = read_abc(open(argv[1], 'rb').read())
        print('version', m.version, 'counts', m.hdr, 'cmd', repr(m.command), 'radius', m.radius,
              'lod dists', m.lod_dists)
        for i, n in enumerate(m.nodes):
            print('node', i, n.name, n.index, n.flags, n.nchildren, [round(x, 3) for x in n.matrix[0] + n.matrix[1] + n.matrix[2]])
        for p in m.pieces:
            print('piece', p.name, p.material, p.spec_power, p.spec_scale, p.lod_weight, p.unknown,
                  [(len(l.faces), len(l.verts)) for l in p.lods])
        for cm in m.children:
            print('child', repr(cm.name), cm.build)
        for a in m.anims:
            print('anim', a.name, a.dims, a.val, a.interp, len(a.keys), a.keys[:3])
        for s in m.sockets:
            print('socket', s.node, s.name, s.rot, s.pos)
        print('weightsets', [w[0] for w in m.weight_sets])
        print('bindings', len(m.bindings), [len(b) for b in m.child_bindings])


if __name__ == '__main__':
    main(sys.argv[1:])
