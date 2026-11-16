#!/usr/bin/env python3
"""PS2 NOLF textures (DTX v-5, BPP_32P: 8-bit indices + a "PALLETE32" section
of 256 32-bit entries) -> PC DTX v-5, BPP_32 (BGRA), with mipmaps.

  ps2dtx.py <in.dtx> <out.dtx>

Also decodes PC DXT1/3/5 and BPP_32 DTX (for comparisons).
"""
import struct, sys

HDR = 0xa4


def clut_index(i):
    """PS2 CSM1 CLUT storage order: bits 3 and 4 of the index swapped (the
    textures are not stored so; measured against PC copies, which also say BGRA)."""
    return (i & ~0x18) | ((i & 0x08) << 1) | ((i & 0x10) >> 1)


def ps2_read(data, swizzle=False, order='bgra'):
    """-> (w, h, [list of mips as bytes RGBA], header bytes)."""
    w, h, nm, ns = struct.unpack_from('<4H', data, 8)
    bpp = data[0x1a]
    if struct.unpack_from('<i', data, 4)[0] != -5 or bpp != 7:
        raise ValueError('not a PS2 BPP_32P DTX (bpp %d)' % bpp)
    p = HDR
    mips = []
    for i in range(nm):
        mw, mh = max(1, w >> i), max(1, h >> i)
        mips.append((mw, mh, data[p:p + mw * mh]))
        p += mw * mh
    pal = None
    for _ in range(ns):
        name = data[p:p + 15].split(b'\0')[0]
        ln = struct.unpack_from('<I', data, p + 28)[0]
        body = data[p + 32:p + 32 + ln]
        p += 32 + ln
        if name == b'PALLETE32':
            pal = body
    if pal is None:
        raise ValueError('no palette')
    # A palette with no alpha at all (every entry 0) is an opaque texture: the
    # PS2 draws those without alpha. On the PC, alpha 0 is see-through.
    no_alpha = all(pal[4 * i + 3] == 0 for i in range(256))
    entries = []
    for i in range(256):
        j = clut_index(i) if swizzle else i
        e = pal[4 * j:4 * j + 4]
        if order == 'rgba':
            r, g, b, a = e
        else:
            b, g, r, a = e
        if no_alpha:
            a = 255
        else:
            a = min(255, a * 2) if a <= 0x80 else 255     # PS2 alpha: 0x80 = opaque
        entries.append(bytes((r, g, b, a)))
    out = []
    for mw, mh, idx in mips:
        out.append((mw, mh, b''.join(entries[k] for k in idx)))
    return w, h, out, data[:HDR]


def downsample(w, h, rgba):
    w2, h2 = max(1, w // 2), max(1, h // 2)
    o = bytearray()
    for y in range(h2):
        for x in range(w2):
            acc = [0, 0, 0, 0]
            for dy in (0, 1):
                for dx in (0, 1):
                    sx, sy = min(w - 1, 2 * x + dx), min(h - 1, 2 * y + dy)
                    q = 4 * (sy * w + sx)
                    for c in range(4):
                        acc[c] += rgba[q + c]
            o += bytes((a + 2) // 4 for a in acc)
    return w2, h2, bytes(o)


def write_pc32(hdr_in, w, h, rgba, n_mips=None):
    """PC DTX, BPP_32 (BGRA), full mip chain built from the top level."""
    mips = [(w, h, rgba)]
    while (n_mips is None or len(mips) < n_mips) and (mips[-1][0] > 1 or mips[-1][1] > 1) and len(mips) < 8:
        mips.append(downsample(*mips[-1]))
    hdr = bytearray(hdr_in[:HDR])
    struct.pack_into('<4H', hdr, 8, w, h, len(mips), 0)
    fl = struct.unpack_from('<I', hdr, 16)[0]
    struct.pack_into('<I', hdr, 16, (fl | 8))          # sections fixed, none
    hdr[0x18 + 2] = 3                                   # BPP_32
    hdr[0x24:HDR] = bytes(HDR - 0x24)                   # command string: none
    out = bytearray(hdr)
    for mw, mh, px in mips:
        b = bytearray(px)
        b[0::4], b[2::4] = px[2::4], px[0::4]          # RGBA -> BGRA
        out += b
    return bytes(out)


def ps2_to_pc(data, swizzle=False, order='bgra'):
    if data[0x1a] == 3:
        # Already 32-bit, stored BGRA as on the PC (the PS2 reverses it at
        # load). Only the alpha needs the PS2 meaning: none at all = opaque,
        # 0x80 = opaque.
        w, h = struct.unpack_from('<HH', data, 8)
        px = bytearray(data[HDR:HDR + w * h * 4])
        alphas = px[3::4]
        if not any(alphas):
            px[3::4] = bytes([255]) * (w * h)
        else:
            px[3::4] = bytes(min(255, a * 2) if a <= 0x80 else 255 for a in alphas)
        rgba = bytearray(px)
        rgba[0::4], rgba[2::4] = px[2::4], px[0::4]
        return write_pc32(data[:HDR], w, h, bytes(rgba))
    w, h, mips, hdr = ps2_read(data, swizzle, order)
    return write_pc32(hdr, w, h, mips[0][2])


# ---------------------------------------------------------------- PC decode

def _c565(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def pc_read_top(data):
    """Top mip of a PC DTX as (w, h, RGBA bytes). DXT1/3/5 and BPP_32."""
    w, h = struct.unpack_from('<HH', data, 8)
    bpp = data[0x1a] or 3
    p = HDR
    if bpp == 3:
        px = bytearray(data[p:p + w * h * 4])
        px[0::4], px[2::4] = data[p + 2:p + w * h * 4:4], data[p:p + w * h * 4:4]
        return w, h, bytes(px)
    if bpp not in (4, 5, 6):
        raise ValueError('bpp %d' % bpp)
    blk = 8 if bpp == 4 else 16
    out = bytearray(w * h * 4)
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    for by in range(bh):
        for bx in range(bw):
            q = p + (by * bw + bx) * blk
            cq = q if bpp == 4 else q + 8
            c0, c1, bits = struct.unpack_from('<HHI', data, cq)
            a, b = _c565(c0), _c565(c1)
            if c0 > c1 or bpp != 4:
                cols = [a, b, tuple((2 * x + y) // 3 for x, y in zip(a, b)), tuple((x + 2 * y) // 3 for x, y in zip(a, b))]
            else:
                cols = [a, b, tuple((x + y) // 2 for x, y in zip(a, b)), (0, 0, 0)]
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x < w and y < h:
                    c = cols[(bits >> (2 * i)) & 3]
                    o = 4 * (y * w + x)
                    out[o:o + 4] = bytes((c[0], c[1], c[2], 255))
    return w, h, bytes(out)


if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    open(dst, 'wb').write(ps2_to_pc(open(src, 'rb').read()))
