#!/usr/bin/env python3
"""make_demo_wavs.py <out dir> — writes the demo library of the SampSy patch: 20 synthetic multisampled sounds,
one folder each (the folder name is what the Library page shows). Nothing is sampled from anywhere, so there is
nothing to license. 16-bit mono; 48 kHz for the first two, 32 kHz for the rest (smaller files, faster loading).

Sustained sounds are looped with no crossfade at all: every partial is given a whole number of cycles inside the loop
(detuned layers complete K+1 or K-1 cycles while the main one completes K, so even the beating repeats exactly),
the period is a whole number of samples, and the small pitch error that causes is written into the WAV's 'smpl'
chunk (unity note + pitch fraction), which SampSy honours. One-shots carry their root note in the file name."""
import sys, os, struct, math
import numpy as np
NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
def hz(m): return 440.0 * 2 ** ((m - 69) / 12)
def name(m): return f"{NAMES[m % 12]}{m // 12 - 1}"

def write_wav(path, x, fs, smpl=None):
    x = np.asarray(x, dtype=np.float64); x = 0.85 * x / (np.abs(x).max() + 1e-12)
    pcm = (x * 32767).astype('<i2').tobytes()
    chunks = b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, fs, fs * 2, 2, 16) + b'data' + struct.pack('<I', len(pcm)) + pcm
    if smpl:
        unity, frac, ls, le = smpl; nl = 0 if ls is None else 1                 # ls None: root + fine pitch only, no loop
        body = struct.pack('<9I', 0, 0, int(1e9 / fs), unity, int(frac * 4294967296) & 0xFFFFFFFF, 0, 0, nl, 0) + (struct.pack('<6I', 0, 0, ls, le - 1, 0, 0) if nl else b'')
        chunks += b'smpl' + struct.pack('<I', len(body)) + body
    open(path, 'wb').write(b'RIFF' + struct.pack('<I', 4 + len(chunks)) + b'WAVE' + chunks)

# ---------------------------------------------------------------- looped sounds
def looped(m, fs, partials, layers=(0,), attack=0.08, loop_s=1.0, noise=0.0, noise_lp=0.2, seed=1):
    """partials: function k -> amplitude (k = 1..); layers: cycle offsets (0 = in tune, +1/-1 = detuned copies)."""
    P = max(2, round(fs / hz(m))); K = max(8, int(loop_s * fs / P)); L = K * P
    n_att = (int(attack * fs) // P + 1) * P; n = n_att + L; t = np.arange(n)
    x = np.zeros(n); rng = np.random.default_rng(seed + m); kmax = int(0.42 * fs / (fs / P))
    for li, off in enumerate(layers):
        cyc = (K + off) / L                                          # cycles per sample of this layer's fundamental
        for k in range(1, kmax + 1):
            a = partials(k)
            if a <= 0: continue
            x += a * np.sin(2 * np.pi * k * cyc * t + rng.uniform(0, 2 * np.pi)) / len(layers)
    if noise > 0:                                                    # breath: noise that repeats with the loop
        nz = rng.standard_normal(L); nz = np.fft.irfft(np.fft.rfft(nz) * np.exp(-np.arange(L // 2 + 1) / (noise_lp * L / 2)), L)
        reps = n // L + 2; x += noise * np.abs(x).max() * np.tile(nz / (np.abs(nz).max() + 1e-12), reps)[(L - n_att % L) % L:][:n]
    x *= np.minimum(1.0, t / (attack * fs)) ** 1.3
    f = fs / P; mf = 69 + 12 * math.log2(f / 440.0); unity = int(math.floor(mf)); frac = mf - unity
    return x, (unity, frac, n_att, n)

# ---------------------------------------------------------------- one-shots
def oneshot(m, fs, secs, partials, inharm=0.0, noise_hit=0.0, seed=1):
    """partials: list of (ratio, amplitude, decay seconds). inharm stretches the ratios like a stiff string."""
    t = np.arange(int(secs * fs)) / fs; f = hz(m); x = np.zeros(len(t)); rng = np.random.default_rng(seed + m)
    ks = 2 ** ((60 - m) / 30)                                        # low notes ring longer
    for r, a, d in partials:
        fr = f * r * math.sqrt(1 + inharm * r * r)
        if fr > 0.45 * fs: continue
        x += a * np.sin(2 * np.pi * fr * t + rng.uniform(0, 0.3)) * np.exp(-t * 6.9 / (d * ks))
    if noise_hit > 0: x += noise_hit * rng.standard_normal(len(t)) * np.exp(-t * 200)
    x *= np.minimum(1.0, t / 0.0015); x[-int(0.01 * fs):] *= np.linspace(1, 0, int(0.01 * fs))
    return x

def fm_oneshot(m, fs, secs, ratio, index, idx_decay, amp_decay, detune=0.0):
    t = np.arange(int(secs * fs)) / fs; f = hz(m); ks = 2 ** ((60 - m) / 30)
    i = index * np.exp(-t * 6.9 / (idx_decay * ks)) + 0.25 * index * np.exp(-t * 6.9 / (3 * idx_decay * ks))
    x = np.sin(2 * np.pi * f * t + i * np.sin(2 * np.pi * f * ratio * t))
    if detune: x = 0.6 * x + 0.4 * np.sin(2 * np.pi * f * (1 + detune) * t + 0.7 * i * np.sin(2 * np.pi * f * ratio * (1 + detune) * t))
    x *= np.exp(-t * 6.9 / (amp_decay * ks)) * np.minimum(1.0, t / 0.002); x[-int(0.01 * fs):] *= np.linspace(1, 0, int(0.01 * fs))
    return x

def pluck(m, fs, secs, damp, bright, seed=1):
    n = int(secs * fs); P = fs / hz(m); N = max(2, int(round(P - 0.5))); rng = np.random.default_rng(seed + m)
    buf = rng.uniform(-1, 1, N); e = 0.0
    for i in range(N): e += bright * (buf[i] - e); buf[i] = e
    y = np.zeros(n); lp = 0.0; frac = 0.0
    for i in range(n):
        j = i % N; nxt = buf[(j + 1) % N]; cur = buf[j]; v = cur + (nxt - cur) * frac * 0.0
        lp += damp[1] * (0.5 * (cur + nxt) - lp); buf[j] = lp * damp[0]; y[i] = v
    y[-int(0.01 * fs):] *= np.linspace(1, 0, int(0.01 * fs))
    # the loop filters detune the string a little: measure the pitch that came out and store it in the smpl chunk
    seg = y[:min(n, fs)] * np.hanning(min(n, fs)); sp = np.abs(np.fft.rfft(seg, 1 << 19)); fr = np.fft.rfftfreq(1 << 19, 1 / fs)
    lo, hi = np.searchsorted(fr, hz(m) * 0.94), np.searchsorted(fr, hz(m) * 1.06); f_real = fr[lo + int(np.argmax(sp[lo:hi]))]
    mf = 69 + 12 * math.log2(f_real / 440.0); unity = int(math.floor(mf))
    return y, (unity, mf - unity, None, None)

out = sys.argv[1] if len(sys.argv) > 1 else 'SAMPSY'
def folder(n): p = os.path.join(out, n); os.makedirs(p, exist_ok=True); return p
NOTES = (36, 48, 60, 72); LOW = (24, 36, 48); HIGH = (48, 60, 72, 84)

# the original two (48 kHz)
d = folder('E-Piano')
for m in (36, 48, 60, 72, 84):
    t = np.arange(int(2.5 * 48000)) / 48000; f = hz(m)
    idx = 2.2 * np.exp(-t * 6.0) + 0.35 * np.exp(-t * 1.2)
    x = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * f * 14.0 * t) * np.exp(-t * 9.0) + 0.6 * idx * np.sin(2 * np.pi * f * t)) * np.exp(-t * (1.1 + m / 60.0)) * np.minimum(1.0, t / 0.002)
    x[-480:] *= np.linspace(1, 0, 480); write_wav(os.path.join(d, f"EP_{name(m)}.wav"), x, 48000)
d = folder('Pad')
for m in (36, 48, 60, 72, 84):
    x, s = looped(m, 48000, lambda k: (1.0 / k ** 1.15) * (1.0 + 0.5 * math.sin(k * 1.7)) if k * hz(m) < 9000 else 0, attack=0.35, loop_s=1.2); write_wav(os.path.join(d, f"PAD_{name(m)}.wav"), x, 48000, s)

FS = 32000
SUST = {   # name: (notes, partial function, layers, attack, noise, noise colour)
  'Strings': (NOTES, lambda k: 1.0 / k ** 1.05 * (0.6 if k > 12 else 1.0), (0, 1, -1), 0.22, 0.0, 0.2),
  'Choir':   (NOTES, None, (0, 1, -1), 0.18, 0.04, 0.12),
  'Brass':   (NOTES, lambda k: 1.0 / k ** 0.9 * math.exp(-((k - 1) / 9.0) ** 2) * (1.15 if k % 2 else 1.0), (0, 1), 0.05, 0.0, 0.2),
  'Organ':   (NOTES, lambda k: {1: 0.9, 2: 1.0, 3: 0.75, 4: 0.6, 6: 0.35, 8: 0.45, 10: 0.12, 12: 0.1, 16: 0.18}.get(k, 0.0), (0,), 0.01, 0.0, 0.2),
  'SubBass': (LOW,   lambda k: {1: 1.0, 2: 0.32, 3: 0.12, 4: 0.04}.get(k, 0.0), (0,), 0.01, 0.0, 0.2),
  'Flute':   (HIGH,  lambda k: {1: 1.0, 2: 0.42, 3: 0.2, 4: 0.07, 5: 0.03}.get(k, 0.0), (0,), 0.07, 0.10, 0.5),
  'Glass':   (HIGH,  lambda k: (0.9 if k in (1, 3, 7, 11, 15) else 0.12 / k), (0, 1), 0.3, 0.0, 0.2),
  'SawPad':  (NOTES, lambda k: 1.0 / k, (0, 1, -1, 2), 0.3, 0.0, 0.2),
  'Pulse':   (NOTES, lambda k: abs(math.sin(math.pi * k * 0.25)) / k, (0,), 0.005, 0.0, 0.2),
}
for nm, (notes, fn, layers, att, nz, nzc) in SUST.items():
    d = folder(nm)
    for m in notes:
        if fn is None:                                               # choir: "ah" formants over a harmonic series
            f0 = hz(m); fn_m = lambda k, f0=f0: (1.0 / k ** 0.6) * (1.0 / (1 + ((k * f0 - 700) / 160) ** 2) + 0.6 / (1 + ((k * f0 - 1150) / 190) ** 2) + 0.25 / (1 + ((k * f0 - 2700) / 300) ** 2) + 0.02)
        else: fn_m = fn
        x, s = looped(m, FS, fn_m, layers=layers, attack=att, loop_s=1.0, noise=nz, noise_lp=nzc); write_wav(os.path.join(d, f"{nm[:4].upper()}_{name(m)}.wav"), x, FS, s)

SHOTS = {
  'Bell':    (HIGH,  lambda m: oneshot(m, FS, 3.0, [(1, 1, 3.5), (2.76, 0.6, 2.6), (5.4, 0.45, 1.8), (8.93, 0.3, 1.1), (13.3, 0.18, 0.7), (0.5, 0.25, 3.0)])),
  'Marimba': (NOTES, lambda m: oneshot(m, FS, 1.4, [(1, 1, 1.1), (3.93, 0.45, 0.22), (9.2, 0.18, 0.08)], noise_hit=0.25)),
  'Vibes':   (HIGH,  lambda m: oneshot(m, FS, 2.8, [(1, 1, 3.2), (3.98, 0.35, 1.2), (9.9, 0.12, 0.4)], noise_hit=0.08) * (1 + 0.25 * np.sin(2 * np.pi * 5.2 * np.arange(int(2.8 * FS)) / FS))),
  'Kalimba': (HIGH,  lambda m: oneshot(m, FS, 1.6, [(1, 1, 1.5), (5.43, 0.4, 0.15), (2.0, 0.12, 0.7), (12.2, 0.15, 0.05)], noise_hit=0.3)),
  'Piano':   (NOTES, lambda m: oneshot(m, FS, 2.6, [(k, 1.0 / k ** 1.3 * (0.6 if k % 7 == 0 else 1.0), 2.6 / k ** 0.55) for k in range(1, 24)], inharm=0.00035, noise_hit=0.12)),
  'Clav':    (NOTES, lambda m: oneshot(m, FS, 1.2, [(k, abs(math.sin(math.pi * k * 0.18)) / k ** 0.7, 0.9 / k ** 0.35) for k in range(1, 28)], noise_hit=0.2)),
  'FMBass':  (LOW,   lambda m: fm_oneshot(m, FS, 1.4, 1.0, 5.5, 0.22, 1.6)),
  'DXBell':  (HIGH,  lambda m: fm_oneshot(m, FS, 2.6, 3.5, 3.0, 1.4, 3.0, detune=0.003)),
  'Pluck':   (NOTES, lambda m: pluck(m, FS, 1.6, (0.992, 0.55), 0.6)),
  'Harp':    (NOTES, lambda m: pluck(m, FS, 2.4, (0.997, 0.4), 0.35)),
}
for nm, (notes, fn) in SHOTS.items():
    d = folder(nm)
    for m in notes:
        r = fn(m); x, s = r if isinstance(r, tuple) else (r, None)
        write_wav(os.path.join(d, f"{nm[:4].upper()}_{name(m)}.wav"), x, FS, s)
open(os.path.join(out, 'README.TXT'), 'w').write(
  "SampSy library. One folder = one sound (shown on the LIBRARY page, named like the folder).\r\n"
  "Put 16- or 24-bit WAV files in a folder; name them with their note (PIANO_C3.wav, F#2.wav) or store the root\r\n"
  "note and a loop in the WAV's sampler chunk. Mono or stereo, any sample rate, about 7 MB per sound.\r\n")
tot = sum(os.path.getsize(os.path.join(r, f)) for r, _, fs_ in os.walk(out) for f in fs_)
print(f"  demo library -> {out}: {len(next(os.walk(out))[1])} sounds, {tot / 1e6:.1f} MB")
