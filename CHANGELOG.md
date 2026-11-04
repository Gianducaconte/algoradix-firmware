# Changelog

## v1.0.0 (2026-11-04)

The first public release. It's the firmware that runs on our own units (internal release 13), built from this tree.
The binary is byte-for-byte identical to `releases/algoradix-v1.0.0.bin`.

- **Patch host:** sound engines are `.APX` files on the SD card, loaded into RAM and swapped without a reboot. Each
  file is checked (ABI, sizes, addresses, CRC-32) before the running patch is touched. If a patch crashes the
  module, it falls back to the built-in engine.
- **Banks:** one bank per folder under `/ALGORADIX/PATCHES/`, up to 128 patches per bank. A Bank page, and MIDI
  CC 0 (Bank Select) followed by Program Change.
- **Panel:** knob names per patch, a FUNC layer (two effect slots, resonance, filter envelope) with pick-up, and a
  one-word-per-screen menu with horizontal slides.
- **Per-patch presets** saved to flash automatically. The MIDI channel and the last patch are global.
- **MIDI:** DIN and USB host merged, with thru to DIN out. Notes, velocity, pitch bend, sustain, mod wheel, clock,
  and a fixed CC map.
- **USB drive:** the SD card mounts on the computer from the USB page while the patch keeps playing.
- **Analog input gain** (0–55 dB) that patches can set, so a microphone can go straight into AUDIO IN.
- **CPU guard** in the instrument SDK: above 68 % load, the oldest voice fades out and polyphony is capped.
- **Patch SDK** with three demo patches (VA.Bass, Lead, SampSy with 21 demo sounds), two templates and
  PATCH-GUIDE.md.
- **Simulator:** renders patches and the UI screens on a computer with the same sources.
