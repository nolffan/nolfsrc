#!/usr/bin/env python3
"""PS2 No One Lives Forever sound banks ("LithTech PSX Sound Data 1.20", *.lps).

Written from scratch from the layout documented in spec-sound.md. The same
container holds the per-level effect banks (lps/<world>.lps) and the per-level
voice-over banks (stream/mNN/vo_<world>.lps); the samples are mono PS-ADPCM
(SPU "VAG" frames: 16 bytes -> 28 samples).

Importable:
    import ps2sound
    b = ps2sound.load('thiefmission1.lps')
    for s in b.sounds:
        s.path          # PC-style path from the group tree, e.g. 'SND\\EVENT\\STEAM.WAV'
        s.name, s.rate, s.loop, s.adpcm (raw bytes)
        pcm = ps2sound.decode(s.adpcm)          # list of int16
    ps2sound.write_wav(path, pcm, rate)

Command line:
    ps2sound.py list BANK.lps                 every sound: index, path, rate, size, length
    ps2sound.py x    BANK.lps OUTDIR [regex]  decode to OUTDIR/<path> as 16-bit PCM WAV
                                              (duplicate paths get a ~N suffix)
    ps2sound.py stat FILE.wav...              RMS / peak / clipping / zero-crossing check
"""
import os
import re
import struct
import sys

MAGIC = b'LithTech PSX Sound Data 1.20\r\n\x1a\0'
HDR = 0x34          # magic(32) + 5 x u32
SOUND_REC = 72      # name[32] + 10 x u32
GROUP_REC = 48      # name[32] + 4 x u32

# PS-ADPCM prediction filters (SPU), in 1/64 units
FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))

FLAG_END, FLAG_LOOP, FLAG_LOOP_START = 1, 2, 4


class Obj:
    def __init__(self, **kw):
        self.__dict__.update(kw)

    def __repr__(self):
        return '%s(%s)' % (type(self).__name__, ', '.join(
            '%s=%r' % kv for kv in self.__dict__.items() if kv[0] not in ('adpcm', 'lip', 'spu_image')))


def cstr(b):
    return b.split(b'\0', 1)[0].decode('latin-1')


def load(path_or_bytes):
    d = path_or_bytes if isinstance(path_or_bytes, bytes) else open(path_or_bytes, 'rb').read()
    if d[:32] != MAGIC:
        raise ValueError('not a LithTech PSX sound bank')
    data_size, n_groups, n_sounds, lip_size, spu_image_size = struct.unpack_from('<5I', d, 32)
    sounds = []
    for i in range(n_sounds):
        o = HDR + i * SOUND_REC
        f = struct.unpack_from('<10I', d, o + 32)
        sounds.append(Obj(index=i, name=cstr(d[o:o + 32]), flags=f[0], offset=f[1],
                          size=f[2], rate=f[3], loop_start=f[4], play_size=f[5],
                          lip_start=f[6], lip_count=f[7], spu_addr=f[8], spu_size=f[9],
                          loop=bool(f[0] & 2)))
    go = HDR + n_sounds * SOUND_REC
    groups = []
    for i in range(n_groups):
        o = go + i * GROUP_REC
        nch, nsnd, first_child, first_sound = struct.unpack_from('<4I', d, o + 32)
        groups.append(Obj(index=i, name=cstr(d[o:o + 32]), n_children=nch,
                          n_sounds=nsnd, first_child=first_child, first_sound=first_sound))
    data = go + n_groups * GROUP_REC
    if data + data_size + lip_size + spu_image_size != len(d):
        raise ValueError('size mismatch: %d + %d + %d + %d != %d' % (
            data, data_size, lip_size, spu_image_size, len(d)))
    for s in sounds:
        s.adpcm = d[data + s.offset:data + s.offset + s.size]
    # group tree: the root is the last group; children are consecutive
    # groups [first_child, first_child + n_children), a leaf's sounds are
    # [first_sound, first_sound + n_sounds)
    def walk(g, prefix, seen):
        if g.index in seen:
            raise ValueError('group cycle')
        seen.add(g.index)
        p = prefix + [g.name] if g.name else prefix
        for c in range(g.first_child, g.first_child + g.n_children):
            walk(groups[c], p, seen)
        for k in range(g.first_sound, g.first_sound + g.n_sounds):
            sounds[k].group = '\\'.join(p)
            sounds[k].path = '\\'.join(p + [sounds[k].name])
    for s in sounds:
        s.group, s.path = '', s.name
    if groups:
        walk(groups[-1], [], set())
    lip = d[data + data_size:data + data_size + lip_size]
    for s in sounds:
        s.lip = lip[s.lip_start:s.lip_start + s.lip_count]
    img = data + data_size + lip_size
    return Obj(data_size=data_size, n_groups=n_groups, n_sounds=n_sounds, lip_size=lip_size,
               spu_image_size=spu_image_size, data_pos=data, spu_image_pos=img,
               spu_image=d[img:img + spu_image_size], sounds=sounds, groups=groups)


def decode(adpcm, stop_at_end=True):
    """PS-ADPCM -> list of int16. Stops after the first frame flagged END
    (when stop_at_end); a looping sound's END|LOOP frame is decoded once."""
    out = []
    s1 = s2 = 0
    for o in range(0, len(adpcm) - 15, 16):
        hdr, flags = adpcm[o], adpcm[o + 1]
        shift, filt = hdr & 0x0F, hdr >> 4
        if filt > 4:
            filt = 0          # invalid on hardware; treat as raw
        if shift > 12:
            shift = 9         # SPU behaviour for shift 13..15
        f0, f1 = FILTERS[filt]
        if flags == 7:        # "void" terminator frame: not played
            break
        for b in adpcm[o + 2:o + 16]:
            for nib in (b & 0x0F, b >> 4):
                v = (nib << 12) & 0xFFFF
                if v & 0x8000:
                    v -= 0x10000
                v = (v >> shift) + ((s1 * f0 + s2 * f1 + 32) >> 6)
                v = -32768 if v < -32768 else 32767 if v > 32767 else v
                out.append(v)
                s2, s1 = s1, v
        if stop_at_end and (flags & FLAG_END):
            break
    return out


def write_wav(path, pcm, rate):
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    raw = struct.pack('<%dh' % len(pcm), *pcm)
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(raw)) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16))
        f.write(b'data' + struct.pack('<I', len(raw)) + raw)


def read_wav(path):
    d = open(path, 'rb').read()
    o, fmt, pcm = 12, None, b''
    while o + 8 <= len(d):
        cid, n = d[o:o + 4], struct.unpack_from('<I', d, o + 4)[0]
        if cid == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', d, o + 8)
        elif cid == b'data':
            pcm = d[o + 8:o + 8 + n]
        o += 8 + n + (n & 1)
    tag, ch, rate, _, _, bits = fmt
    if tag != 1 or bits != 16:
        return Obj(rate=rate, channels=ch, bits=bits, samples=None)
    return Obj(rate=rate, channels=ch, bits=bits,
               samples=list(struct.unpack('<%dh' % (len(pcm) // 2), pcm[:len(pcm) // 2 * 2])))


def stats(pcm):
    """Cheap signal sanity numbers: rms, peak, fraction of clipped samples,
    zero-crossing rate, and the energy ratio of the first difference to the
    signal (white noise ~2.0, speech/effects well below 1)."""
    n = len(pcm) or 1
    e = sum(x * x for x in pcm) or 1
    rms = (e / n) ** 0.5
    peak = max((abs(x) for x in pcm), default=0)
    clip = sum(1 for x in pcm if x >= 32767 or x <= -32768) / n
    zc = sum(1 for a, b in zip(pcm, pcm[1:]) if (a < 0) != (b < 0)) / n
    de = sum((b - a) ** 2 for a, b in zip(pcm, pcm[1:]))
    return Obj(rms=rms, peak=peak, clip=clip, zcr=zc, diff_ratio=de / e)


def unique_paths(sounds):
    seen = {}
    for s in sounds:
        k = s.path.upper()
        seen[k] = seen.get(k, 0) + 1
        s.out_path = s.path if seen[k] == 1 else '%s~%d%s' % (
            s.path[:-4], seen[k], s.path[-4:])


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == 'stat':
        for p in sys.argv[2:]:
            w = read_wav(p)
            st = stats(w.samples)
            print('%-50s %5d Hz %6.2fs rms %6.0f peak %5d clip %.4f zcr %.3f d/s %.3f' % (
                p, w.rate, len(w.samples) / w.rate, st.rms, st.peak, st.clip, st.zcr,
                st.diff_ratio))
        return
    b = load(sys.argv[2])
    unique_paths(b.sounds)
    if cmd == 'list':
        print('# %d sounds, %d groups, data %d bytes at 0x%x, lip-sync %d bytes, SPU image %d bytes' % (
            b.n_sounds, b.n_groups, b.data_size, b.data_pos, b.lip_size, b.spu_image_size))
        for s in b.sounds:
            n = sum(1 for _ in range(0, s.size, 16)) * 28
            print('%4d %-44s %5d Hz %7d B ~%6.2fs %s%s' % (
                s.index, s.out_path, s.rate, s.size, n / s.rate, 'loop ' if s.loop else '',
                'lip %d' % s.lip_count if s.lip_count else ''))
    elif cmd == 'x':
        out = sys.argv[3]
        rx = re.compile(sys.argv[4], re.I) if len(sys.argv) > 4 else None
        for s in b.sounds:
            if rx and not rx.search(s.out_path):
                continue
            dst = os.path.join(out, *s.out_path.split('\\'))
            write_wav(dst, decode(s.adpcm), s.rate)
            print(dst)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
