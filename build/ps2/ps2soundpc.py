#!/usr/bin/env python3
"""Which sounds do the PS2-only worlds need that the PC archives lack, and
extract those from the PS2 sound banks as PC WAV files.

    ps2soundpc.py [--ps2 EXTRACT] [--pc REZDIR] [--out OUTDIR] [--map MAP]
                  [--worlds thiefmission1,thiefmission2,thiefmission3] [--dry-run]

EXTRACT   the PS2 extraction (default out/tmp/ps2-research/extract): it has
          AssetLists/<world>.txt (whose [Sounds] names the world's banks),
          the banks themselves (lps/..., stream/...) and Worlds/<world>.ltb
REZDIR    the PC game's .rez files (default rez/games/nolf-eu-goty)
OUTDIR    where the WAVs go, in the PC layout with upper-case names and a
          lower-case extension, e.g. VOICE/14002.wav, SND/EVENT/SCARE.wav
          (default out/tmp/ps2-research/sound)
MAP       the mapping list written (default build/ps2/sound-map.txt)

Every sound in a world's banks whose path (from the bank's group tree) is in
no PC archive is extracted; the map also lists the worlds' references (object
string properties: *.wav names, `playsound` commands, CinematicTrigger
DialogueN ids = VOICE\\<id>.wav) and the PS2 attribute files' *.wav names,
and flags world references found nowhere.
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools'))
import ps2sound      # noqa: E402
import ps2world      # noqa: E402
import rezextract    # noqa: E402


def norm(p):
    return p.replace('/', '\\').upper()


def pc_names(rezdir):
    names = {}
    for f in sorted(os.listdir(rezdir)):
        if f.lower().endswith('.rez'):
            for p, _, _ in rezextract.Rez(os.path.join(rezdir, f)).walk():
                names.setdefault(norm(p), f)
    return names


def world_banks(extract, world):
    banks = []
    with open(os.path.join(extract, 'AssetLists', world + '.txt'), encoding='latin-1') as fh:
        sec = None
        for line in fh:
            m = re.match(r'\s*\[(\w+)\]', line)
            if m:
                sec = m.group(1)
                continue
            m = re.match(r'\s*(LPSFile\w*)\s*=\s*"([^"]+)"', line)
            if sec == 'Sounds' and m:
                banks.append(m.group(2))
    return banks


def world_refs(path):
    """{PATH: [where, ...]} for the sounds a world's objects name."""
    refs = {}
    w = ps2world.load(path)
    for o in w.objects:
        for pr in o.props:
            if not isinstance(pr.value, str):
                continue
            where = '%s %s.%s' % (o.cls, o.name, pr.name)
            for m in re.findall(r'[\w\\/&.-]+\.wav', pr.value, re.I):
                refs.setdefault(norm(m), []).append(where)
            if re.match(r'Dialogue\d+$', pr.name):
                for t in re.split(r'[,;\s]+', pr.value.strip()):
                    if t.isdigit():
                        refs.setdefault('VOICE\\%s.WAV' % t, []).append(where)
    return refs


def attribute_refs(extract):
    """{PATH: ['Weapons.txt:5288', ...]} for the .wav names in the PS2 attribute files."""
    refs = {}
    d = os.path.join(extract, 'Attributes')
    for f in sorted(os.listdir(d)):
        if not f.lower().endswith('.txt'):
            continue
        with open(os.path.join(d, f), encoding='latin-1') as fh:
            for n, line in enumerate(fh, 1):
                for m in re.findall(r'"([^"]+\.wav)"', line, re.I):
                    refs.setdefault(norm(m), []).append('%s:%d' % (f, n))
    return refs


def out_name(path):
    parts = path.split('\\')
    base = parts[-1]
    if base.upper().endswith('.WAV'):
        base = base[:-4].upper() + '.wav'
    return [x.upper() for x in parts[:-1]] + [base]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--ps2', default=os.path.join(REPO, 'out/tmp/ps2-research/extract'))
    ap.add_argument('--pc', default=os.path.join(REPO, 'rez/games/nolf-eu-goty'))
    ap.add_argument('--out', default=os.path.join(REPO, 'out/tmp/ps2-research/sound'))
    ap.add_argument('--map', default=os.path.join(HERE, 'sound-map.txt'))
    ap.add_argument('--worlds', default='thiefmission1,thiefmission2,thiefmission3')
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()

    pc = pc_names(a.pc)
    have = {}        # PATH -> (bank, sound) first seen
    dups = {}        # PATH -> [(bank, sound)] further copies
    refs = {}        # PATH -> [world: where]
    for world in a.worlds.split(','):
        for bank in world_banks(a.ps2, world):
            b = ps2sound.load(os.path.join(a.ps2, bank))
            for s in b.sounds:
                k = s.path.upper()
                if k in have:
                    dups.setdefault(k, []).append((bank, s))
                else:
                    have[k] = (bank, s)
        for k, where in world_refs(os.path.join(a.ps2, 'Worlds', world + '.ltb')).items():
            refs.setdefault(k, []).extend('%s: %s' % (world, x) for x in where)

    attrs = attribute_refs(a.ps2)
    lines = ['# PC path <- PS2 source (bank #index, rate, ADPCM bytes) ; decoded length ; referenced by',
             '# generated by build/ps2/ps2soundpc.py; WAVs in out/tmp/ps2-research/sound/<PC path>',
             '# worlds: ' + a.worlds, '']
    n = 0
    for k in sorted(have):
        if k in pc:
            continue
        bank, s = have[k]
        pcm = ps2sound.decode(s.adpcm)
        other = [d for d in dups.get(k, []) if d[1].adpcm != s.adpcm or d[1].rate != s.rate]
        st = ps2sound.stats(pcm)
        r = refs.get(k, []) + ['PS2 Attributes/' + x for x in attrs.get(k, [])]
        lines.append('%s <- %s #%d (%d Hz%s, %d B) ; %.2fs rms %.0f ; %s%s' % (
            '/'.join(out_name(k)), bank, s.index, s.rate, ', loop' if s.loop else '', s.size,
            len(pcm) / s.rate, st.rms,
            '; '.join(sorted(set(r))[:3]) + (' (+%d)' % (len(set(r)) - 3) if len(set(r)) > 3 else '')
            if r else 'not referenced by the worlds or attributes (bank only)',
            ' ; DIFFERS in ' + ','.join(d[0] for d in other) if other else ''))
        if not a.dry_run:
            ps2sound.write_wav(os.path.join(a.out, *out_name(k)), pcm, s.rate)
        n += 1
    missing = sorted(k for k in refs if k not in pc and k not in have)
    lines += ['', '# referenced by the worlds, in neither the PC archives nor the PS2 banks:']
    lines += ['#   %s ; %s' % (k, '; '.join(sorted(set(refs[k]))[:3])) for k in missing]
    lines += ['', '# %d sounds extracted; %d world references, %d of them on PC, %d from PS2 banks, %d nowhere' % (
        n, len(refs), sum(1 for k in refs if k in pc),
        sum(1 for k in refs if k not in pc and k in have), len(missing))]
    with open(a.map, 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-1:]))


if __name__ == '__main__':
    main()
