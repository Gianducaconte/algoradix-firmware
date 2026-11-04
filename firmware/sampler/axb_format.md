# AXB — Algoradix sample bank preset file (version 1)

One file = one playable preset: a small header, a zone table and all PCM data, in one sequential blob so the board can load it with a single streaming read into SDRAM. Little-endian. All offsets in bytes.

Card layout the firmware scans: `/SOUND-BANK/<bank folder>/<preset>.AXB`, optional `/SOUND-BANK/<bank folder>/BANK.TXT` (free text: source, author, licence). Nothing else on the card is read.

## Header (96 bytes)

| off | type | field |
|---|---|---|
| 0 | char[4] | magic `"AXB1"` |
| 4 | u32 | header size (96) |
| 8 | char[16] | preset name, zero padded, ≤ 10 chars shown on the OLED |
| 24 | char[8] | f1 macro name |
| 32 | char[8] | f2 macro name |
| 40 | u16 | number of zones (1..32) |
| 42 | u16 | flags: bit0 = velocity layers present |
| 44 | u32 | total PCM bytes following the zone table |
| 48 | u8 | default env_mode (0 AD, 1 ASR, 2 LOOP) |
| 49 | u8 | default filter_type (0 LP, 1 BP, 2 HP) |
| 50 | u8[2] | reserved |
| 52 | f32[8] | defaults: resonance, env_to_cutoff, key_track, chorus, dly_mix, rev_mix, dly_time, rev_size (all 0..1; a negative value = leave the current setting) |
| 84 | u8[12] | reserved (zero) |

Pot-driven parameters (FILT, A, D, LEVELS, OCT, f1, f2) have no defaults: pots are physical.

## Zone (48 bytes each, directly after the header)

| off | type | field |
|---|---|---|
| 0 | u8 | key_lo (MIDI) |
| 1 | u8 | key_hi (inclusive) |
| 2 | u8 | vel_lo |
| 3 | u8 | vel_hi (inclusive) |
| 4 | u8 | root note (MIDI) |
| 5 | u8 | channels (1 or 2) |
| 6 | u8 | loop_mode: 0 one-shot, 1 forward loop (crossfade already baked into the data) |
| 7 | u8 | reserved |
| 8 | s16 | tune, cents (playback = 2^((note - root + tune/100)/12)) |
| 10 | u16 | sample rate, Hz |
| 12 | u32 | frames |
| 16 | u32 | loop_start (frame) |
| 20 | u32 | loop_end (frame, exclusive) |
| 24 | u32 | data offset (bytes from the start of the PCM area, 4-aligned) |
| 28 | f32 | gain (linear) |
| 32 | u8[16] | reserved |

## PCM area

int16 samples, interleaved when stereo, zones concatenated, each zone padded to a 4-byte boundary.

## Limits enforced by the converter

- preset total ≤ 7 000 000 bytes (fits the free SDRAM next to the 2 s delay lines); the converter downmixes to mono and/or shortens one-shot tails to reach that, and reports what it did
- ≤ 32 zones, key ranges must not overlap within a velocity layer
- sample rate 8 000..65 535 Hz (played back through fractional read, no host resampling)
