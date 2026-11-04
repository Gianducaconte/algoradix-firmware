# Patch guide — how to write Algoradix patches that are fast, safe and use the hardware properly

For humans and for AI agents generating patches. Everything here is either measured on the module or enforced by the host; where a number is an estimate it says so. The ABI itself is `apx.h` (read it, it is short).

---

## 1. What a patch is

A patch is one flat binary file, `/ALGORADIX/PATCHES/<NAME>.APX` on the SD card. The host firmware copies it into RAM, clears its variables, calls `init()`, and from then on calls `process()` 3000 times per second with 16 stereo frames each. There is no operating system, no C library, no heap, no threads.

| The host does | The patch does |
|---|---|
| pots: filtering, pick-up, FUNC layer, MIDI CC take-over, the name overlay | reads `panel->k[]`, `panel->fn[]`, `panel->cc[]` |
| MIDI: DIN + USB host merged, channel filter, thru | gets a list of events per block |
| the menu; standard pages the patch asked for; drawing; **a preset per patch** (FUNC values, effect settings, page choices, Library item — restored when the patch is selected again) | optionally answers "how many options / name of option i / select i" for its own pages |
| two effect slots after the patch, master volume, soft clip | writes dry audio to `out_l / out_r` |
| SD card, SDRAM allocator, progress display | loads its files in `init()` / `menu_select()` only |
| loading, CRC check, fade-out / fade-in, crash and overload recovery | — |

### Where and when patch code runs
| Function | Context | May it take long? | May it use files / `progress()`? |
|---|---|---|---|
| `init(api)` | main loop, audio silent | yes (seconds are fine, show progress) | yes |
| `menu_select(page, i)` | main loop, audio silent | yes | yes |
| `menu_count / menu_name / menu_current` | main loop, audio running | no — return stored values only | no |
| `active()` | main loop, audio running | no | no |
| **`process(blk)`** | **audio interrupt**, every 333 µs | **never: it has about 250 µs at most, see §4** | **never** |

`process()` interrupts the main-loop functions at any moment. Share data between them only through plain `volatile` flags / pointers written in one step (see `wavsynth.c`: `ready` goes to 0 before the sample memory is touched and back to 1 after).

---

## 2. The hardware as a patch sees it

| Resource | Facts that matter |
|---|---|
| 8 pots `k[APX_K_*]` | 0..1, already filtered (≈ 16 ms) and with pick-up. **Pot noise is ≈ 0.3 %**: never recompute something expensive just because a value changed by less than 0.001. Positions: LEVEL, TUNE, OCT, F1, F2, A, D, FILT. `k[LEVEL]` is informative only: the host applies master volume |
| CV jacks | Five jacks are **electrically summed with their pot** on one ADC pin (F1, F2, TUNE, D, FILT): the patch just sees the pot value move. Inverting input stage already compensated, ≈ 400 ADC counts per volt, range about −4.6…+5.5 V. Only **ASSIGN** is a stand-alone input: `cv_assign` (volts) and `gate` (above ≈ 1 V with hysteresis). There are **no CV or gate outputs**; outputs are audio L/R and MIDI thru |
| FUNC + pot `fn[]` | F1 / OCT / F2 / LEVEL belong to the host's effect slots. **FILT, A, D are the patch's** (instrument SDK: resonance, filter-envelope attack / decay); name them with `fn_name[]`. TUNE is never layered |
| `octave`, `tune_fine`, `cv_note` | OCT pot as −2…+2 (plus ASSIGN when the player chose OCTAVE); TUNE as ±1 semitone fine tune with a centre dead zone; TUNE pot + jack as a 1 V/oct note number (C2 at 0 V, pot left) |
| MIDI | note on/off, bend, sustain, mod wheel, all-notes-off as events; every CC as `cc[n]` (0..1, negative = never received). The fixed CC map is in the root README — patch-specific functions should use **CC 102–115** (116 / 117 and Program Change are the host's Patch / Library recall) |
| Clock | `clock_running`, `beat_s` (seconds per quarter note from MIDI clock). Instrument-SDK voices read them through `apx_panel` (`apx_inst.h`, the current block's panel) — tempo-synced LFOs and arpeggiators follow the clock, 120 BPM without one. Clock on the ASSIGN jack is not implemented in the host yet: a patch can measure `gate` edges itself |
| Audio | 48 kHz, 16 frames per block, float −1..1, stereo in and out. Line-level input arrives around ±0.1…0.3, not ±1. A patch may raise the **analog input gain** 0–55 dB for a microphone: `api->set_input_gain(dB)` from init() / menu_select(), only when `api->ext == APX_HOST_EXT1` (older hosts lack it); the host resets it to 0 dB on every patch start |
| Effect slots | `fx1`, `fx2` in the header: `APX_FX_NONE / DELAY / REVERB / CHORUS / DIST / CRUSH`. They run after the patch, cost CPU (§4) and must be two different types |
| Display | not accessible. While `init()` / `menu_select()` run the host shows its loading spinner by itself (`progress()` is accepted and currently ignored). Names you give (patch, knobs, options, folders) are drawn in a proportional bold font, mixed case, full ASCII: the patch and option names at 13 px — **about 52 px are visible, i.e. 7–8 letters** ("SampSy", "E-Piano" just fit) — knob and page captions at 8 px capitals (≤ ≈ 10 letters). Check with `make -C sim ui` after adding your words to `sim/ui_preview.c` |
| SD card | read-only through `api`: one file open at a time, `dir_entry()` by index. The patch's folder — its **Library** — is `api->patch_dir` = `/ALGORADIX/<patch name reduced to A–Z 0–9>` ("SampSy" → `/ALGORADIX/SAMPSY`) |

---

## 3. Memory: what you have and where things must go

| Memory | Size | Speed | Use |
|---|---|---|---|
| The patch's own RAM (SRAM1 through the address-0 alias) | **63 KB for code + constants + variables together** (`apx_pack.py` refuses more) | fastest | code, voice structs, anything touched per sample. The instrument SDK's voice pool is 20 KB of it |
| SDRAM from `api->sdram_alloc()` | ≈ 7 MB, shared by nothing else while the patch runs | several times slower per access, fine for streaming-style access | samples, delay lines, wavetables, loop buffers. **Not cleared** by the host. Released automatically when another patch loads; there is no free() |
| Stack | the host's main stack (32 KB, shared with the host's own interrupt frames) | — | keep locals in `process()` under ≈ 1 KB, in `init()` under ≈ 4 KB. **No recursion, no variable-length arrays, no big local buffers**: make them `static` |

Rules:
- No `malloc`, no `printf`, no `<math.h>` (nothing is linked: `-nostdlib`). Available: `memcpy memset memmove memcmp strlen strcpy strncpy strcmp strncmp strchr`, and `dsp/fastmath.h` (`fm_sin01`, `fm_exp2`, `fm_note_to_hz`, `fm_clamp`).
- Static initialisers must be constants. There are no constructors; `.init_array` is not run. Everything not explicitly initialised is zero (the host clears bss) — and is zero again every time the patch is loaded, never "left over".
- With the instrument SDK, `pool_alloc()` transparently takes allocations of 6 KB or more from SDRAM. Put per-sample state in small structs, tables and lines in big allocations.
- Pointers are 32-bit absolute addresses. The patch is linked at a fixed address (`apx.ld`); do not try to make it relocatable.
- **Address 0 is valid RAM on this system** (that is where the patch lives), so a NULL dereference does not crash — it silently reads or corrupts the patch itself. Check every pointer that can be NULL (`sdram_alloc`, `pool_alloc`, the optional header callbacks).

---

## 4. CPU: the budget and how to stay inside it

One block is 333 µs. Everything — patch, two effect slots, host overhead — has to fit, and above ≈ 90 % the interrupt starves the rest of the system. Measured on the module (MON page, `DSP %`):

| Item | Cost |
|---|---|
| Host with nothing playing (event handling, panel, volume, soft clip) | 7 % |
| Delay slot with its mix above zero | + 5 % (at zero mix it is nearly free) |
| Reverb slot with its mix above zero | ≈ + 6 % (estimate: 4 combs + 3 all-passes) |
| One **POLY LEAD** voice = 3 polyBLEP oscillators + 4-pole ladder with saturation + 2 envelopes + voice frame | **9.5 %** |
| → 6 such voices + delay | 65–69 %: the practical ceiling for a voice of this weight |

Per-voice estimates to plan with (derived from that measurement, not individually measured): polyBLEP saw or square ≈ 1.2 %, pulse with PWM ≈ 1.8 %, morphing `osc_next` ≈ 2.5 %, sine / table read ≈ 0.6 %, state-variable filter ≈ 1 %, ladder ≈ 2 %, envelope ≈ 0.4 % each, voice-frame block work ≈ 1.3 %, Hermite sample read ≈ 1.5 % mono / 2.5 % stereo.

**Targets:** stay **under 70 % in the worst case you allow** (max voices, both effects on, fastest notes). Set `#define APX_INST_VOICES n` *before* including `apx_inst.h` to a number your voice can afford (default 6, maximum 8): heavy voice → 4–6, light voice (one oscillator + SVF) → 8.

**Reality check (measured on the module):** across ~700 patches, estimates from this table ran **1.6–2× below** the load measured on the module, and timing on the Mac does not predict it (the M4 pays for float divisions, per-note set-up and memory traffic the Mac hides). Since 2026-10-22 the instrument SDK has a **CPU guard**: it reads the cycle counter around your process() and, above 68 % of the block, fades out the oldest voice and caps the polyphony; the cap rises again when there is room. So a too-generous `APX_INST_VOICES` no longer overloads — it plays fewer notes at once — but a single voice must still fit comfortably. For anything new and heavy, measure: read `*(volatile uint32_t *)0xE0001004` (DWT->CYCCNT) at the start and end of process() and show the result in a Library name during tuning (build with `make READOUT=1`, which defines `APX_LOAD_READOUT`, and guard the readout with `#ifdef APX_LOAD_READOUT`).

### What made the difference on this hardware (in order of effect)
1. **Build flags are part of the design.** The SDK Makefile uses `-O3 -flto -ffast-math`; without LTO every `osc_next()` / `env_next()` / `svf_next()` is a real function call per sample per voice (27 calls per sample became 3; 12.4 % → 9.5 % per voice). Do not build patches any other way. Consequences of `-ffast-math`: no NaN / infinity checks will work, the compiler may reorder float arithmetic — write code that is correct without them.
2. **Control rate vs audio rate.** Anything that depends only on pots, MIDI or an envelope value — frequency → phase increment, cutoff → filter coefficients, `fm_exp2`, `fm_note_to_hz`, gain laws, LFOs — is computed **once per block**, before the sample loop. Inside the loop: multiply-adds, phase wraps, table reads.
3. **Cache what did not change.** `env_set()` skips its three `fm_exp2` when A and D moved less than 0.001. Do the same for your own coefficient maths.
4. **No divisions in the sample loop** (14 cycles each and they stall the pipeline): multiply by a reciprocal computed per block. No `%`. No double precision — every float constant carries the `f` suffix (`-Wdouble-promotion` is on, zero warnings is the rule).
5. **Few branches on data in the sample loop.** Prefer arithmetic selection or split the loop (one loop for looping samples, one for one-shots) over an `if` per sample.
6. **Keep hot state together and small.** One struct per voice, fields used per sample first. SDRAM is for bulk data, not for the voice struct.
7. **Skip silent work.** Free a voice as soon as it is inaudible (`vframe_done()`, or your own level check as in `p_modal.c`); do nothing at all for inactive voices; bypass a stage whose amount is 0.
8. **Effects cost too.** A patch that is already heavy should pick cheap slots (DIST, CRUSH, CHORUS) or `APX_FX_NONE`.

---

## 5. Rules that keep the module from crashing

The host catches the worst cases, but each of them is audible or visible to the player — a good patch never triggers them.

| Failure | What the host does | How to avoid it |
|---|---|---|
| Bad file (size, ABI, addresses, CRC) | refuses it, shows `BAD …`, the old patch keeps playing | always produce the file with the SDK Makefile + `apx_pack.py` |
| Processor fault (wild pointer, jump into data, undefined instruction) | reboots into the built-in engine, shows `PATCH ERR`, does not auto-load that patch again | bounds-check every index that comes from outside (MIDI note 0..127, `page`, `index`, zone counts, file contents); never trust a WAV header; check allocation results; remember that NULL does not fault here (§3) |
| CPU overload (> 92 % for 8 blocks) | sends `APX_EV_PANIC`, shows `OVERLOAD` | budget per §4; cap the voices; test the worst case |
| A patch that ignores `APX_EV_PANIC` and stays overloaded | the interrupt starves the system: display and knobs freeze until the load drops | handle it: **silence every voice immediately** (the instrument SDK does: all notes off + 100 ms of instant release) |
| Endless loop or long work in `process()` | same as overload, and it cannot recover | every loop in `process()` has a fixed, small upper bound. File access, table building, clearing megabytes belong in `init()` / `menu_select()` |
| NaN or a runaway feedback path | silence or full-scale noise; with `-ffast-math` you cannot test for NaN | clamp every feedback write (`fm_clamp(x, -1, 1)` before storing into a delay line), keep resonance / feedback coefficients strictly below their stability limit, clamp filter frequencies to the ranges the DSP blocks expect (`svf_set` ≤ 16 kHz, `ladder_set` ≤ 18 kHz) |
| Clipping with many voices | the host's soft clipper hides it, but it sounds compressed | per-voice gain so that the full chord peaks below ≈ 0.9 before master volume (the voice frame uses 0.35 × velocity curve; unison and CV-unison scale down further) |
| Stuck notes | — | **a voice's identity is the plain MIDI note number.** Offsets that can change while the note sounds (fine tune, bend, octave, glide) are applied when computing pitch, never added to the stored note. Match note-offs on the stored number |
| Clicks | — | start and end every voice through an envelope or a ≥ 5 ms ramp; smooth any pot-driven gain or mix per block (`y += 0.1f * (target - y)`); never jump a read pointer without a crossfade; when `menu_select()` replaces sample memory, clear every voice first (audio is silent then) |
| Denormals | not a speed problem on this FPU | nothing to do |

`init()` must leave the patch ready to have `process()` called immediately, even if loading failed (no card, empty folder): play silence, show a word (`NO WAVS`), never return with half-initialised state.

---

### Pitfalls found in the 700-patch batches
- `.def_env` in `DEFINE_POLY_PATCH_D` is **ignored** (the SDK forces ASR). For a one-shot, set `cc.env_mode = ENV_AD` on your voice's copy of the controls and pass `c->decay == 0` through untouched (that is the PANIC fast-kill).
- `fm_sin01()` is off by up to 0.066 near phase 0 / 1 (the polynomial is fitted for ±π/2): fine for audio oscillators, **not** for filter coefficients or anything that needs accuracy at tiny phases.
- A band-limited pulse has a mean of `2·pw − 1`: subtract it, or narrow widths pile DC into the delay / reverb.
- The ladder (`ladder_set`) self-oscillates near Nyquist above ~7 kHz with resonance: cap its cutoff, key tracking included.
- The DIST slot starts at amount 0.35 (strong drive): a patch meant to be clean should pick other slots or warn in its comment.
- Never clear large buffers in process() or note-on (audio interrupt): count samples written since the note started and treat older content as zero, or clear in init() / menu_select().

## 6. Writing an instrument patch (the normal case)

One C file: a voice struct, four small functions, two macro lines. The instrument SDK gives the voice everything that is common: note → Hz with octave, fine tune, bend, glide and drift; the amp envelope on A / D; the filter (HP / BP / LP from the FILTER page, state-variable or ladder) on FILT with resonance and its own envelope on FUNC + FILT / A / D; key tracking; the LFO and mod wheel; POLY / UNISON / MONO; ASSIGN as gate; audio-in MIX / RING; sustain pedal; PANIC.

```c
#include "poly_patch.h"
#include "osc.h"
#include "vframe.h"
#include "apx_inst.h"
typedef struct { voice_hdr_t h; /* MUST be first */ osc_t o; vframe_t vf; } v_t;
static void vreset(v_t *v, uint32_t seed) { (void)seed; osc_reset(&v->o); vframe_reset(&v->vf); }
static void n_on(voice_hdr_t *h, float note, float vel) { (void)note; (void)vel; env_gate(&((v_t *)h)->vf.env, 1); }
static void n_off(voice_hdr_t *h) { env_gate(&((v_t *)h)->vf.env, 0); }
static void proc(voice_hdr_t *h, const controls_t *c, float *out, int n, float bend)
{
  v_t *v = (v_t *)h;
  float hz = vframe_block(&v->vf, c, h, bend, 0);           /* control rate: once per block */
  osc_set_freq(&v->o, hz);
  float shape = c->macro1;                                   /* F1; c->macro2 = F2 */
  for (int i = 0; i < n; i++) out[i] += vframe_sample(&v->vf, osc_next(&v->o, shape));   /* ADD into out */
  if (vframe_done(&v->vf)) h->active = 0;                    /* give the voice back */
}
static const voice_ops_t ops = { n_on, n_off, proc };
DEFINE_POLY_PATCH_D(mysynth, "MY SYNTH", "SHAPE", "UNUSED", v_t, ops, vreset, (void)0,
  .def_env = ENV_ASR, .def_filter = FILT_LADDER,            /* FILT_LADDER: the FILTER page's "LP" is the ladder; else the state-variable LP */
  .def = { -1, -1, 0.4f, -1, -1, -1, -1, -1 },              /* only [2] = key tracking is used */
  .def_unison = 1, .def_lfo_dest = LFO_OFF,                 /* LFO_OFF: mod wheel = vibrato */
  .def_uni_detune = 0.3f, .def_lfo_rate = -1, .def_lfo_amt = -1, .def_drive = 0.25f)
APX_INSTRUMENT(apx_header, APX_HEADER_ATTR, patch_mysynth, "MY SYNTH", "SHAPE", "UNUSED",
               APX_FX_DELAY, APX_FX_REVERB, APX_PAGES_INSTRUMENT)
```
**Naming rule (applies to every patch, hand-written or generated):** the name shown on the module is an abbreviation or acronym of the full name, **at most 7 characters**, mixed case — "TB303", "Juno", "DX-EP", "Wtable", "VAPair". Same limit for F1 / F2 names and for every Library entry ("Square", "Feedbk", "ZLPF"). The full name and the description go in the first comment of the file. 7 characters is what fits the visible 52 px at 13 px bold.

**Library = the patch's variants.** When an instrument has a small set of models, waveforms, registrations or presets, expose them with `APX_INSTRUMENT_LIB(..., variant, "Saw", "Square")` instead of `APX_INSTRUMENT`: `variant` is a `static volatile int` the engine reads, the names appear on the Library page, FUNC + rotate steps through them, MIDI CC 117 recalls them and the host remembers the choice. Switching happens with the audio faded out, so the engine may change abruptly. Typical uses: two waveforms of one bass, four filter flavours, piano / organ / combo models, a set of wavetable banks.

**Unison:** `.def_unison = 2` or `3` (and the Mode page's Unison, which is 3) starts that many copies of the voice per note, spread by `uni_detune` and in stereo, each at equal-power gain (−3 dB per doubling, in the voice frame). Every copy is a full voice: `APX_INST_VOICES 6` with unison 2 plays 3 notes, and the DSP cost is per copy. (Before 2026-10-08 every copy landed on the same voice; patches written then put their detuned oscillators inside the voice instead.)

**Banks:** the Makefile line `BANK_<FILE> := <folder>` puts a patch into `/ALGORADIX/PATCHES/<folder>/` (one bank per folder, e.g. `3-Pad`); without it the patch is in the bank Core. The bank does not change anything inside the patch (its Library folder stays `/ALGORADIX/<NAME>`).

**Voice count:** `#define APX_INST_VOICES n` before `#include "apx_inst.h"` (1 = monophonic with legato: a note played while the previous one is held takes over the same voice without retriggering the envelope).

**Bending the shared voice frame:** `controls_t` is const, but a voice may pass its own copy to `vframe_block()`: `controls_t cc = *c; cc.resonance = ...; cc.glide = ...;` (a 160-byte copy per voice per block is nothing). Add a patch-owned filter envelope through `v->vf.cut_offset`. Notes that must ring out while held (piano, e-piano, plucks) multiply the sample by their own per-block decay (`pc_decay_block()` in `pcommon.h`) and free the voice when it is inaudible.

`pcommon.h` has the small shared helpers: polyBLEP residual, phase wrap, cosine, soft saturator, random numbers, per-block decay. Real examples: `demo/bass.c`, `demo/lead.c`; many more voice designs in `../patches/p_*.c`.

Add it to the Makefile (`PATCHES += 04MYSYN` and `SRC_04MYSYN := demo/mysynth.c`); the file name sets its place on the ALGO page.

## 7. Other kinds of patch

Fill the header yourself and read the panel directly. `templates/effect_template.c` is a complete, compile-checked effect patch in 35 lines (`make templates`). `demo/wavsynth.c` shows files, SDRAM, an own menu page with run-time options, and mixing the SDK adapter with a custom header.

- `.pages = 0` removes ASSIGN / FILTER / MODE / INPUT; list only the ones that mean something for the patch. An effect or looper leaves INPUT out because it uses the audio input itself.
- Own pages: `.n_custom_pages` (max 2) and the four `menu_*` callbacks. **Page 0 is the patch's Library**: the host shows it under the fixed name Library, reachable with FUNC + click, and FUNC + rotate steps through it from anywhere — so page 0 should be the list of the patch's content (sounds, kits, loops) and selecting an item should load it. With no page 0 the Library shows "Empty". Page 1, if any, appears after Library under `.page_name[1]`. The host remembers the chosen index of each and calls `menu_select(page, index)` once after `init()`. `menu_select` is called with the audio silent; stepping through the library with FUNC + rotate loads only the item the player pauses on (0.4 s).
- Name every knob you use in `.knob_name[]`; unnamed ones show the host default (LEVEL, TUNE, OCT, F1, F2, ATTACK, DECAY, CUTOFF).
- A patch without the SDK must handle `APX_EV_PANIC` and `APX_EV_ALL_OFF` itself if it has voices, and should honour `octave` / `tune_fine` if it is pitched.
- Not in the host yet: CLOCK / TRIG roles for ASSIGN in the menu, MIDI out from a patch, writing files, an `idle()` hook for streaming. The header carries its own size, so these can be added without breaking existing patches.

---

## 8. Workflow: never flash or copy something you have not heard

```bash
# 1. hear it on the Mac: same sources, same ABI, same effect slots, scripted chord + low and high note
cd "FRIMWARE - FABLE/sim" && make apx PATCH=demo/mysynth.c && ./build/apxsim /tmp/out.wav [folder for file-based patches] [option]
#    the last line must say: no NaN, peak below ~0.9, "left 0" (every voice was released)
#    FX=0.5 ./build/apxsim ...   both effect slots at 50 %        MODE=2 ...   MONO
# 2. build for the card: zero warnings; the packer prints image + bss against the 63 KB limit
cd "FRIMWARE - FABLE/firmware/patchsdk" && make
# 3. module: USB page -> On -> card mounts on the computer
make install CARD=/Volumes/<name>      # then EJECT on the Mac; the module rescans by itself
# 4. on the module, MON page (last line: V = voices, DSP %):
#    nothing held, one note, the biggest chord you allow, both effects on, fastest repeated notes. Worst case < 70 %.
```
Check-list before calling a patch done: zero warnings · simulator clean · worst-case DSP < 70 % · no click on note start / end / patch select / option select · long release frees its voices · survives an empty or missing sample folder · every outside index bounds-checked · names fit the display.

---

## 9. For an AI agent asked to "make a patch that …"

If the request is "make it sound like this preset / plug-in": rebuild what makes it recognisable (oscillator types, filter settings, envelopes, LFOs and above all the modulation routings) against the DSP library, map its macros onto F1 / F2, and send the user a simulator render before anything goes to the card. Never copy third-party code, wavetables or samples.

1. Decide the kind. Pitched and played from MIDI → instrument SDK (§6); it already solves pitch, envelopes, filter, polyphony and the panel conventions, so spend the design on the voice. Processes audio or sequences itself → own header (§7).
2. Map the idea onto the fixed surface: two macros (F1, F2) that are musically the most important controls, an optional own menu page for a choice among named variants, two effect slots that suit the sound, CC 102–115 for anything else. Do not invent controls the panel does not have; do not put secondary parameters in the menu (the UI rule: one big word per screen, nothing that is not fundamental).
3. Write the voice with control-rate / audio-rate separation (§4) and pick `APX_INST_VOICES` from the cost table. State the expected DSP % in a comment at the top of the file.
4. Apply every rule of §5. Clamp feedback, bound loops, check pointers and indices.
5. Run the simulator and read its last line before proposing the patch; report peak level and that all voices were released. Then give the user the install steps and the MON measurements to read back.
6. Do not change `apx.h` for one patch. If the ABI really lacks something, propose it separately: every existing patch file on every card depends on it (`APX_ABI` must be bumped and all patches rebuilt).
