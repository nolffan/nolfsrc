# PS2 NOLF sound banks (`*.lps`, "LithTech PSX Sound Data 1.20")

Reader/decoder: `build/ps2/ps2sound.py`. PC-side gap analysis and extraction:
`build/ps2/ps2soundpc.py` (writes `build/ps2/sound-map.txt`).

## Where they are

Each world's asset list (`AssetLists/<world>.txt` in LITH_PS2.REZ) names its sound files:

    [Music]   Music000         = "stream/m03/nolf_ps2_03.wav"
    [Sounds]  LPSFile000       = "lps/thiefmission1.lps"
              LPSFileStream000 = "stream/m03/vo_thiefmission1.lps"

- `lps/<world>.lps`: the level's effect bank. It holds every sample the level can
  play (doors, events, ambience, weapons, character barks, and sometimes a voice line),
  copied in from the shared sources. Banks of different levels share content.
- `stream/mNN/vo_<world>.lps`: the voice-over bank (VOICE\<id>.WAV). It uses the same
  container and adds a stored SPU preload image (see below).
- `stream/mNN/nolf_ps2_NN.wav`: music. These are ordinary RIFF PCM files (16-bit mono,
  24000 Hz), not ADPCM. The PC game plays DirectMusic, so they are not used here.

All values are little-endian. The banks do not use the "VAGp" header.

## Layout

    0x00  char[32]  "LithTech PSX Sound Data 1.20\r\n\x1a\0"
    0x20  u32       data_size        bytes of ADPCM sample data
    0x24  u32       n_groups
    0x28  u32       n_sounds
    0x2c  u32       lip_size         bytes of lip-sync data (= sum of lip_count)
    0x30  u32       spu_image_size   0 in effect banks; the preload image in VO banks
    0x34  sound[n_sounds]  72 bytes each
          group[n_groups]  48 bytes each
          u8 data[data_size]          sample data; sound offsets are relative to here
          u8 lip[lip_size]
          u8 spu_image[spu_image_size]
    (end of file: the sizes add up exactly)

### Sound record (72 bytes)

    +0   char[32] name        e.g. "LOCKPICK.WAV", "14002.WAV" (upper case, NUL-terminated,
                              the bytes after the NUL are uninitialised garbage)
    +32  u32 flags            bit 1 (2): looping sound; no other bits seen
    +36  u32 offset           into data[]. Sounds are stored in reverse order: index 0
                              has the highest offset and the last index has offset 0
    +40  u32 size             ADPCM bytes, a multiple of 16
    +44  u32 rate             sample rate in Hz (2000..22050; voice is mostly 12000)
    +48  u32 loop_start       looping only: byte offset of the loop-start frame (0x20 for
                              a sound that loops whole); 0 for one-shots
    +52  u32 play_size        one-shot: size-32 (leaves out the leading zero frame and
                              the pad frame after END); whole loop: size-16; intro+loop+
                              tail: the offset of the loop-end frame
    +56  u32 lip_start        index into lip[]
    +60  u32 lip_count        lip-sync bytes for this sound (0 if none)
    +64  u32 spu_addr         where the sound's head goes in SPU RAM (relative to the bank's
                              SPU area), laid out from the last sound upward
    +68  u32 spu_size         min(size, 0x3000): the resident head. Longer sounds are
                              streamed through a 12 KB SPU buffer
                              (the EXE's CLTPS2SoundEEADPCMStream; inferred, not traced)

### Group record (48 bytes): the directory tree

    +0   char[32] name        "SND", "EVENT", "GUNS", "WALTHERSMG", "S&W", "VOICE", ...
    +32  u32 n_children       child groups are groups[first_child .. +n_children)
    +36  u32 n_sounds         sounds are sounds[first_sound .. +n_sounds)
    +40  u32 first_child      (in a leaf, a copy of its parent's first_child: ignore it)
    +44  u32 first_sound      (in an inner node, the first sound below it)

The last group is the root. It has an empty name. A sound's PC path is the chain of
group names from the root down to the sound's leaf, followed by the sound's name.
Example tree (thiefmission1):

    root -> VOICE, SND{SWITCH,EVENT,DOORS,AMB}, GUNS->SND{WALTHERSMG,S&W,P38,...},
            CHARS->SND{PLAYER,MALE,FEMALE,ENGLISH,AMERICAN}

so CHARS\SND\ENGLISH\COMBAGGR11.WAV and GUNS\SND\P38\FIRE.WAV match the PC paths
case-insensitively. A path can occur more than once across banks (the same sample),
but never twice in one bank.

### Lip-sync data

One byte per 50 ms (20 Hz) gives the mouth-open amplitude, roughly 0x01..0x50. For each
voice sound, lip_count / duration is 19.9..20.8. The exception is VOICE\14038 (stored
at 16000 Hz): it gives 18.2, so it may be pitched up on purpose. Only sounds that
characters speak have lip data. The PC game has no use for it.

### SPU image (VO banks only)

spu_image[spu_addr : spu_addr+spu_size] is byte-identical to the first spu_size bytes
of each sound's data. Checked for every sound in the three VO banks. It is a
ready-to-DMA copy of the sound heads so a voice line can start at once while the rest
streams from disc. Effect banks give spu_addr/spu_size but do not store the image; the
runtime builds it.

## Sample encoding: PS-ADPCM (SPU "VAG" frames)

Mono only. 16-byte frames, each giving 28 samples:

    byte 0   high nibble = filter (0..4), low nibble = shift (0..12)
    byte 1   flags: 1 = END, 2 = LOOP (repeat), 4 = LOOP START
    bytes 2..15   28 x 4-bit signed samples, low nibble first

    s = (nibble << 12, as int16) >> shift
    s += (s1*f0 + s2*f1 + 32) >> 6,  (f0,f1) = (0,0) (60,0) (115,-52) (98,-55) (122,-60)
    clamp to int16; s2 = s1; s1 = s

Frame flags as observed (830 sounds):
- One-shot: frame 0 is all zero bytes; every frame has flags 0 except the
  second-to-last, which has 1 (END); the last frame is padding after END.
- Whole loop: frame 0 is zero (flags 0), frame 1 has flags 2, frame 2 has 6 (loop start),
  then 2 ... 2, and the last frame has 3 (END+LOOP, jump to the loop start).
- Intro + loop + tail (lighter fire/altfire, zipcord): the loop start (6) is at
  loop_start and the loop end (3) is at play_size, followed by a tail up to a final 1.

`ps2sound.decode` decodes up to and including the first frame with the END bit (for a
loop, one pass through the loop). It skips a frame whose flags are 7 and keeps the
leading zero frame (28 silent samples).

## Validation

- Every bank parses with no bytes left over. For every sound, offset+size lies inside
  data[]. The sounds tile data[] exactly (VO banks: no gaps).
- Compared with the PC originals, resampled to the PS2 rate (SND\DOORS\LOCKPICK,
  SND\SWITCH\CALLBUTTON, SND\EVENT\TARGTDONEBELL): the durations are equal to within
  0.01 s and the normalised cross-correlation is 0.57..0.70 at a constant lag of exactly
  28 samples (the leading zero frame). The remaining difference comes from the PS2
  versions' lower sample rate.
- Voice lines decode to speech-like signals (first-difference/signal energy 0.06..1.0;
  white noise gives about 2). The 20 Hz lip-sync count agrees with the decoded length.
- No clipping beyond isolated single samples.
