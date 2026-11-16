#!/usr/bin/env python3
"""Read-only PS2 LITH_PS2.REZ reader, a Python port of the logic in
StenApp/ps2rezdecoder (Rez.cpp / ps2rezdecoder.cpp).

Layout (little endian):
  0x000  u32 unknown(=3), u32 entries_per_table, u32 hash_magic,
         u32 data_offset (2048-byte sectors), u32 table_count
  0x800 + t*0x19000: table t, entries_per_table x 16-byte entries
         {u32 name_hash, u32 sector, u32 size, u32 size_in_sectors}
  hash: h = 0; for c in upper(name) ('/'->'\\'): h = h + c + h*magic (u32)

Usage:
  ps2rez.py info   REZ
  ps2rez.py tables REZ                 per-table summary
  ps2rez.py list   REZ NAMES.txt OUT   full listing (all tables)
  ps2rez.py x      REZ NAMES.txt OUTDIR [regex] [table]  extract named files
"""
import os, re, struct, sys
from collections import defaultdict

TABLE_START, TABLE_STRIDE, ENTRY = 0x800, 0x19000, 16


def rez_hash(name, magic):
    h = 0
    for ch in name:
        c = ord(ch.upper())
        if c == ord('/'):
            c = ord('\\')
        h = (h + c + h * magic) & 0xFFFFFFFF
    return h


def open_rez(path):
    f = open(path, 'rb')
    unk, n, magic, data_off, ntab = struct.unpack('<5I', f.read(20))
    tables = []
    for t in range(ntab):
        f.seek(TABLE_START + t * TABLE_STRIDE)
        raw = f.read(n * ENTRY)
        tables.append([struct.unpack_from('<4I', raw, i * ENTRY) for i in range(n)])
    return f, dict(unk=unk, n=n, magic=magic, data_off=data_off, ntab=ntab), tables


def load_names(path, magic):
    names = {}
    with open(path, encoding='latin-1') as fh:
        for line in fh:
            s = line.strip()
            if not s or s.startswith('#'):
                continue
            s = s.split()[0]
            names.setdefault(rez_hash(s, magic), s)
    return names


def main():
    cmd, rez = sys.argv[1], sys.argv[2]
    f, hdr, tables = open_rez(rez)
    if cmd == 'info':
        print(hdr)
        return
    if cmd == 'tables':
        for t, tab in enumerate(tables):
            live = [e for e in tab if e[2]]
            hashes = len(set(e[0] for e in tab))
            secs = [e[1] for e in live]
            print(t, 'live', len(live), 'uniqhash', hashes,
                  'sectors', (min(secs), max(secs)) if secs else None,
                  'bytes', sum(e[2] for e in live))
        return
    names = load_names(sys.argv[3], hdr['magic'])
    if cmd == 'list':
        with open(sys.argv[4], 'w') as out:
            out.write('# header %r\n# table\tidx\thash\tsector\tsize\tsectors\tname\n' % hdr)
            for t, tab in enumerate(tables):
                for i, (h, sec, size, xs) in enumerate(tab):
                    if size == 0 and sec == 0 and h == 0:
                        continue
                    out.write('%d\t%d\t0x%08X\t%d\t%d\t%d\t%s\n' % (
                        t, i, h, sec, size, xs, names.get(h, '?')))
        return
    if cmd == 'x':
        outdir = sys.argv[4]
        rx = re.compile(sys.argv[5], re.I) if len(sys.argv) > 5 else None
        want_t = int(sys.argv[6]) if len(sys.argv) > 6 else None
        done = set()
        for t, tab in enumerate(tables):
            if want_t is not None and t != want_t:
                continue
            for h, sec, size, xs in tab:
                nm = names.get(h)
                if not nm or size == 0 or h in done:
                    continue
                if rx and not rx.search(nm):
                    continue
                done.add(h)
                dst = os.path.join(outdir, *nm.replace('\\', '/').split('/'))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                f.seek(sec * 2048)
                with open(dst, 'wb') as o:
                    o.write(f.read(size))
                print('%s\t%d\ttable %d' % (nm, size, t))
        return


if __name__ == '__main__':
    main()
