#!/usr/bin/env python3
"""axb_convert.py — build Algoradix .AXB sample presets.

Inputs (choose one per preset):
  tal   <preset.talsmpl>              TAL-Sampler preset: zones (url/rootkey/lowkey/highkey/loop) + amp envelope + effects
  wavs  <folder or glob>              a set of WAVs; root note from the WAV 'smpl' chunk or from the file name (e.g. c#3, a#17f...)
  sfz   <file.sfz>                    SFZ subset: <region> sample= lokey hikey pitch_keycenter key lovel hivel tune loop_mode loop_start loop_end volume
  bank  <spec.json>                   many presets at once (list of {"type":..., "src":..., "name":..., options})

Options: --name NAME --f1 F1 --f2 F2 --out FILE.AXB --mono --max-bytes N --autoloop --loop-min-sec S
         --defaults '{"chorus":0.4,"rev_mix":0.3,...}' --keyrange lo-hi --nozone-fill
Unsupported source features are printed as warnings, never silently dropped.
"""
import sys, os, re, json, glob, struct, argparse, wave
import xml.etree.ElementTree as ET
import numpy as np

MAX_BYTES_DEFAULT = 7_000_000
NOTE_RE = re.compile(r'([a-gA-G])(#|b)?(-?\d)')
NOTE_BASE = {'c':0,'d':2,'e':4,'f':5,'g':7,'a':9,'b':11}

def warn(msg): print("  ! " + msg)

# ---------- WAV reading (16/24/32-bit PCM, float32), with smpl chunk ----------
def read_wav(path):
    d = open(path, 'rb').read()
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE': raise ValueError("not a WAV: " + path)
    fmt = None; data = None; root = None; loops = []; i = 12
    while i + 8 <= len(d):
        cid = d[i:i+4]; sz = struct.unpack('<I', d[i+4:i+8])[0]; body = d[i+8:i+8+sz]
        if cid == b'fmt ': fmt = struct.unpack('<HHIIHH', body[:16])
        elif cid == b'data': data = body
        elif cid == b'smpl':
            root = struct.unpack('<I', body[12:16])[0]
            n = struct.unpack('<I', body[28:32])[0]
            for k in range(n):
                cue, typ, st, en, fr, pc = struct.unpack('<IIIIII', body[36+24*k:60+24*k]); loops.append((st, en, typ))
        i += 8 + sz + (sz & 1)
    tag, ch, rate, _, _, bits = fmt
    if tag == 3 and bits == 32: x = np.frombuffer(data, dtype='<f4').astype(np.float32)
    elif bits == 16: x = np.frombuffer(data, dtype='<i2').astype(np.float32) / 32768.0
    elif bits == 24:
        b = np.frombuffer(data, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        x = ((b[:,0] | (b[:,1] << 8) | (b[:,2] << 16)) << 8).astype(np.int32).astype(np.float32) / 2147483648.0
    elif bits == 32: x = np.frombuffer(data, dtype='<i4').astype(np.float32) / 2147483648.0
    else: raise ValueError(f"unsupported WAV bits={bits} tag={tag}: {path}")
    x = x.reshape(-1, ch)
    return x, rate, root, loops

def note_from_name(name):
    m = NOTE_RE.search(os.path.basename(name))
    if not m: return None
    n = NOTE_BASE[m.group(1).lower()] + (1 if m.group(2) == '#' else -1 if m.group(2) == 'b' else 0)
    return (int(m.group(3)) + 1) * 12 + n     # C4 = 60 (c-1 = 0)

# ---------- pitch verification ----------
def measure_f0(x, rate, fmin=25.0, fmax=2500.0):
    """Autocorrelation pitch estimate on the sustained part of the sample; None if unvoiced."""
    mono = x.mean(axis=1) if x.ndim == 2 else x
    n = len(mono); a, b = int(n * 0.25), int(n * 0.25) + int(rate * 0.4)
    if b > n: a, b = 0, min(n, int(rate * 0.4))
    seg = mono[a:b] - mono[a:b].mean()
    if len(seg) < rate * 0.05 or np.abs(seg).max() < 1e-3: return None
    ac = np.correlate(seg, seg, 'full')[len(seg)-1:]; ac /= ac[0] + 1e-12
    lo, hi = int(rate / fmax), int(rate / fmin)
    if hi >= len(ac): hi = len(ac) - 1
    i = int(np.argmax(ac[lo:hi]) + lo)
    for cand in range(lo, hi):                       # first strong peak beats a later octave-down peak
        if ac[cand] > 0.85 * ac[i] and ac[cand-1] < ac[cand] >= ac[cand+1]: i = cand; break
    if ac[i] < 0.3: return None
    return rate / i, float(ac[i])

def verify_root(z):
    """Pick the root candidate (declared root, name root, ±1/2 octaves) closest to the measured pitch;
    store the remaining fine offset as tune (limited to +/-50 cents)."""
    r = measure_f0(z.audio, z.rate)
    nr = getattr(z, 'name_root', None)
    if r is None:
        if nr is not None and nr != z.root: warn(f"{z.src}: pitch not measurable; declared {z.root}, name says {nr} -> using name"); z.root = nr
        else: warn(f"{z.src}: pitch not measurable, keeping root {z.root}")
        return
    f, conf = r
    midi = 69.0 + 12.0 * np.log2(f / 440.0)
    cands = [z.root + k for k in (-24, -12, 0, 12, 24)]
    if nr is not None: cands += [nr + k for k in (-24, -12, 0, 12, 24)]
    best = min(cands, key=lambda r_: abs(r_ - midi))
    diff = midi - best
    if abs(diff) > 0.75:
        if conf >= 0.7:
            warn(f"{z.src}: measured {f:.1f} Hz (MIDI {midi:.2f}, conf {conf:.2f}) matches no declared root ({z.root}{'' if nr is None else '/name '+str(nr)}); using measured")
            best = int(round(midi)); diff = midi - best
        else:
            best = nr if nr is not None else z.root; diff = 0.0
            warn(f"{z.src}: inharmonic/unclear pitch ({f:.1f} Hz, conf {conf:.2f}); keeping root {best} UNVERIFIED")
    if best != z.root: warn(f"{z.src}: root {z.root} -> {best} (measured {f:.1f} Hz, conf {conf:.2f})")
    elif abs(diff) > 0.03: print(f"    {z.src}: root {z.root} verified ({f:.1f} Hz, {diff*100:+.0f} cents)")
    z.root = int(best); z.tune = int(round(-diff * 100))
    z.tune = max(-50, min(50, z.tune))

# ---------- loop handling ----------
def find_loop(x, rate, min_sec=0.6, max_sec=2.5, search_from=0.35):
    """Pick a loop in the sustained part: maximise similarity between the two loop ends."""
    mono = x.mean(axis=1) if x.ndim == 2 else x
    n = len(mono); w = 2048
    start_min = int(n * search_from); best = None
    lens = np.linspace(min_sec * rate, min(max_sec * rate, n - start_min - w), 24).astype(int)
    for L in lens:
        if L < w * 2: continue
        for s in range(start_min, n - L - w, max(256, (n - L - start_min) // 40)):
            a = mono[s:s+w]; b = mono[s+L:s+L+w]
            den = np.sqrt((a*a).sum() * (b*b).sum()) + 1e-9
            score = (a*b).sum() / den - 0.02 * abs(np.abs(a).mean() - np.abs(b).mean())
            if best is None or score > best[0]: best = (score, s, s + L)
    return (best[1], best[2], best[0]) if best else None

def bake_crossfade(x, ls, le, xf):
    """Equal-power crossfade of the audio before loop_start into the audio before loop_end, so a plain jump at le -> ls is seamless."""
    xf = int(min(xf, ls, le - ls))
    if xf < 64: return x, ls, le
    y = x.copy()
    t = np.linspace(0.0, 1.0, xf, dtype=np.float32)[:, None]
    gin = np.sin(t * np.pi / 2); gout = np.cos(t * np.pi / 2)
    y[le-xf:le] = y[le-xf:le] * gout + y[ls-xf:ls] * gin
    return y, ls, le

# ---------- zone model ----------
class Zone:
    def __init__(self, audio, rate, root, key_lo=0, key_hi=127, vel_lo=0, vel_hi=127, tune=0, loop=None, gain=1.0, src=''):
        self.audio, self.rate, self.root = audio, rate, root
        self.key_lo, self.key_hi, self.vel_lo, self.vel_hi = key_lo, key_hi, vel_lo, vel_hi
        self.tune, self.loop, self.gain, self.src = tune, loop, gain, src

def fill_key_ranges(zones):
    """Zones with identical (0..127) ranges but different roots: split the keyboard at midpoints between roots."""
    zs = sorted(zones, key=lambda z: z.root)
    if len(zs) < 2: return zs
    for i, z in enumerate(zs):
        lo = 0 if i == 0 else (zs[i-1].root + z.root) // 2 + 1
        hi = 127 if i == len(zs) - 1 else (z.root + zs[i+1].root) // 2
        z.key_lo, z.key_hi = lo, hi
    return zs

# ---------- front ends ----------
def load_tal(path, opts):
    r = ET.parse(path).getroot(); pr = r.find('.//program')
    base = os.path.dirname(path); zones = []
    for m in r.iter('multisample'):
        url = m.get('url', '')
        if not url: continue
        p = os.path.join(base, url)
        if not os.path.exists(p): p = os.path.join(base, os.path.basename(url))     # presets often carry the author's absolute paths
        if not os.path.exists(p): warn(f"missing sample {url}"); continue
        x, rate, root_smpl, loops = read_wav(p)
        st, en = int(m.get('startsample', 0)), int(m.get('endsample', 0))
        if en and en < len(x): x = x[st:en]
        elif st: x = x[st:]
        loop = None
        if m.get('loopenabled') == '1':
            ls, le = int(m.get('loopstartsample', 0)) - st, int(m.get('loopendsample', 0)) - st
            if 0 <= ls < le <= len(x): loop = (ls, le)
            else: warn(f"{url}: loop points outside the sample, ignored")
        det = (float(m.get('detune', 0.5)) - 0.5) * 200.0   # assume +/-100 cents range
        if m.get('reverse') == '1': warn(f"{url}: reverse not supported")
        if m.get('slice', '0') != '0': warn(f"{url}: slices not supported")
        zones.append(Zone(x, rate, int(m.get('rootkey', 60)), int(m.get('lowkey', 0)), int(m.get('highkey', 127)),
                          tune=int(round(det)), loop=loop, gain=float(m.get('volume', 1)), src=url))
    a, s_, r_ = float(pr.get('adsrampattack', 0)), float(pr.get('adsrampsustain', 1)), float(pr.get('adsramprelease', 0.3))
    defaults = {'env_mode': 1 if s_ > 0.05 else 0, 'rev_mix': 0.25 if pr.get('effectreverbon') == '1' else 0.0,
                'chorus': 0.35 if pr.get('effectchoruson') == '1' else 0.0, 'resonance': float(pr.get('filterresonance', 0)) * 0.7}
    info = f"TAL amp env A={a:.2f} S={s_:.2f} R={r_:.2f} (A/D come from the pots on the board)"
    ignored = [k for k in ('lforate1', 'portamentointensity', 'effecteq1enabled') if pr.get(k) not in (None, '0', '0.5')]
    if ignored: warn("TAL features not converted: " + ", ".join(ignored))
    return zones, defaults, pr.get('programname', os.path.basename(path)), info

VEL_RE = re.compile(r'[a-gA-G][#b]?-?\d([0-9a-fA-F]{2})[0-9a-fA-F]{4}\.wav$')
def load_wavs(src, opts):
    files = sorted(glob.glob(os.path.join(src, '*.wav')) + glob.glob(os.path.join(src, '*.WAV'))) if os.path.isdir(src) else sorted(glob.glob(src))
    every = int(opts.get('every') or 1)          # keep every n-th zone to save memory
    zones = []
    for p in files:
        x, rate, root, loops = read_wav(p)
        rn = note_from_name(p)
        if root is None and rn is None: warn(f"{os.path.basename(p)}: no root note (no smpl chunk, none in name) — skipped"); continue

        loop = (loops[0][0], loops[0][1] + 1) if loops else None
        m = VEL_RE.search(os.path.basename(p)); vel = int(m.group(1), 16) if m else 127
        z = Zone(x, rate, rn if (rn is not None and not opts.get('trust_smpl')) else root, loop=loop, src=os.path.basename(p)); z.velsrc = vel; z.name_root = rn; z.smpl_root = root
        zones.append(z)
    # velocity layers: group by recorded velocity, split the 0..127 range at midpoints
    vels = sorted(set(z.velsrc for z in zones))
    if opts.get('top_velocity_only') and len(vels) > 1:
        zones = [z for z in zones if z.velsrc == vels[-1]]; vels = vels[-1:]
        print(f"    kept only the loudest velocity layer ({vels[0]})")
    out = []
    for i, v in enumerate(vels):
        lo = 0 if i == 0 else (vels[i-1] + v) // 2 + 1
        hi = 127 if i == len(vels) - 1 else (v + vels[i+1]) // 2
        layer = fill_key_ranges([z for z in zones if z.velsrc == v])
        if every > 1:
            kept = layer[::every]
            if kept[-1] is not layer[-1]: kept.append(layer[-1])
            layer = fill_key_ranges(kept)
        for z in layer: z.vel_lo, z.vel_hi = lo, hi
        out += layer
    return out, {}, os.path.basename(os.path.normpath(src)), f"{len(out)} multisample zones from WAV roots, {len(vels)} velocity layer(s)"

def load_sfz(path, opts):
    base = os.path.dirname(path); zones = []; glob_ = {}; cur = None; scope = None
    text = re.sub(r'//.*', '', open(path, errors='replace').read())
    for tok in re.split(r'(<[a-z]+>)', text):
        tok = tok.strip()
        if not tok: continue
        if tok.startswith('<'):
            if cur is not None: zones.append(cur)
            cur = None
            scope = tok
            if scope == '<region>': cur = dict(glob_)
            continue
        kv = dict(re.findall(r'(\w+)=("[^"]*"|\S+)', tok))
        kv = {k: v.strip('"') for k, v in kv.items()}
        if scope in ('<global>', '<group>', '<master>'): glob_.update(kv)
        elif scope == '<region>' and cur is not None: cur.update(kv)
    if cur is not None: zones.append(cur)
    out = []
    for z in zones:
        if 'sample' not in z: warn("region without sample= skipped"); continue
        p = os.path.join(base, z['sample'].replace('\\', '/'))
        if not os.path.exists(p): warn(f"missing {z['sample']}"); continue
        x, rate, root_smpl, loops = read_wav(p)
        key = z.get('key'); root = int(z.get('pitch_keycenter', key if key else (root_smpl if root_smpl is not None else 60)))
        lo, hi = int(z.get('lokey', key if key else 0)), int(z.get('hikey', key if key else 127))
        loop = None
        lm = z.get('loop_mode', 'loop_continuous' if loops else 'no_loop')
        if lm in ('loop_continuous', 'loop_sustain'):
            if 'loop_start' in z and 'loop_end' in z: loop = (int(z['loop_start']), int(z['loop_end']) + 1)
            elif loops: loop = (loops[0][0], loops[0][1] + 1)
            if lm == 'loop_sustain': warn(f"{z['sample']}: loop_sustain treated as loop_continuous")
        for k in ('offset', 'ampeg_attack', 'ampeg_release', 'cutoff', 'transpose'):
            if k in z: warn(f"{z['sample']}: SFZ opcode {k} not converted (use the board's pots/menu)")
        gain = 10 ** (float(z.get('volume', 0)) / 20)
        out.append(Zone(x, rate, root, lo, hi, int(z.get('lovel', 0)), int(z.get('hivel', 127)), int(float(z.get('tune', 0))), loop, gain, z['sample']))
    return out, {}, os.path.splitext(os.path.basename(path))[0], f"{len(out)} SFZ regions"

# ---------- writer ----------
def build(zones, name, f1, f2, defaults, out, mono=False, max_bytes=MAX_BYTES_DEFAULT, autoloop=False, loop_min=0.6, xfade_sec=0.08, keyrange=None, verify=True):
    if not zones: raise SystemExit("no zones")
    if keyrange:
        lo, hi = keyrange
        for z in zones: z.key_lo, z.key_hi = max(z.key_lo, lo), min(z.key_hi, hi)
        zones = [z for z in zones if z.key_lo <= z.key_hi]
    if verify:
        for z in zones: verify_root(z)
    else: print('    root verification OFF (declared/name roots kept)')
    # loops
    for z in zones:
        if z.loop is None and autoloop and len(z.audio) / z.rate >= loop_min * 1.6:
            r = find_loop(z.audio, z.rate, min_sec=loop_min)
            if r: z.loop = (r[0], r[1]); print(f"    autoloop {z.src}: {r[0]/z.rate:.2f}s..{r[1]/z.rate:.2f}s score {r[2]:.3f}")
        if z.loop:
            ls, le = z.loop
            z.audio, ls, le = bake_crossfade(z.audio, ls, le, int(xfade_sec * z.rate))
            z.audio = z.audio[:le]                       # nothing after the loop end is ever played
            z.loop = (ls, le)
    # size management
    def total(): return sum(len(z.audio) * (1 if mono else z.audio.shape[1]) * 2 for z in zones)
    if total() > max_bytes and not mono:
        mono = True; print(f"    downmix to mono to fit ({total()/1e6:.1f} MB > {max_bytes/1e6:.1f} MB)")
    if total() > max_bytes:
        scale = max_bytes / total()
        for z in zones:
            if z.loop: continue
            keep = int(len(z.audio) * scale)
            if keep < len(z.audio): z.audio = z.audio[:keep]; print(f"    shortened one-shot {z.src} to {keep/z.rate:.1f}s")
    if total() > max_bytes: raise SystemExit("preset too large even after downmix/shortening")
    # PCM
    pcm = bytearray(); table = []
    for z in zones:
        a = z.audio.mean(axis=1, keepdims=True) if mono else z.audio
        a = np.clip(a * z.gain, -1.0, 1.0)
        peak = float(np.abs(a).max()) if len(a) else 0.0
        off = len(pcm)
        pcm += (a * 32767.0).astype('<i2').tobytes()
        while len(pcm) & 3: pcm += b'\0'
        table.append((z, a.shape[1], off, peak))
    # header
    d = {'resonance': -1, 'env_to_cutoff': -1, 'key_track': -1, 'chorus': -1, 'dly_mix': -1, 'rev_mix': -1, 'dly_time': -1, 'rev_size': -1}
    d.update({k: v for k, v in defaults.items() if k in d})
    vel_layers = any(z.vel_lo > 0 or z.vel_hi < 127 for z in zones)
    hdr = struct.pack('<4sI16s8s8sHHIBBxx8f12x', b'AXB1', 96, name.encode()[:16], f1.encode()[:8], f2.encode()[:8],
                      len(zones), 1 if vel_layers else 0, len(pcm),
                      int(defaults.get('env_mode', 1)), int(defaults.get('filter_type', 0)),
                      d['resonance'], d['env_to_cutoff'], d['key_track'], d['chorus'], d['dly_mix'], d['rev_mix'], d['dly_time'], d['rev_size'])
    assert len(hdr) == 96
    zt = b''
    for z, ch, off, peak in table:
        ls, le = z.loop if z.loop else (0, 0)
        zt += struct.pack('<BBBBBBBBhHIIIIf16x', z.key_lo, z.key_hi, z.vel_lo, z.vel_hi, z.root, ch, 1 if z.loop else 0, 0,
                          int(z.tune), int(z.rate), len(z.audio), ls, le, off, 1.0)
    os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
    open(out, 'wb').write(hdr + zt + pcm)
    print(f"  -> {out}: {len(zones)} zones, {'mono' if mono else 'stereo/mixed'}, {(96+len(zt)+len(pcm))/1e6:.2f} MB, "
          + ", ".join(f"[{z.key_lo}-{z.key_hi} r{z.root}{' loop' if z.loop else ''}]" for z, _, _, _ in table[:6]) + (" ..." if len(table) > 6 else ""))

def run_one(kind, src, args, extra=None):
    opts = vars(args).copy(); opts.update(extra or {})
    loader = {'tal': load_tal, 'wavs': load_wavs, 'sfz': load_sfz}[kind]
    zones, defaults, autoname, info = loader(src, opts)
    print(f"{kind} {src}: {info}")
    if opts.get('defaults'): defaults.update(json.loads(opts['defaults']) if isinstance(opts['defaults'], str) else opts['defaults'])
    name = opts.get('name') or autoname
    out = opts.get('out') or os.path.join('runtime', re.sub(r'[^A-Z0-9]', '', name.upper())[:8] + '.AXB')
    kr = opts.get('keyrange'); kr = tuple(int(v) for v in kr.split('-')) if isinstance(kr, str) else kr
    build(zones, name, opts.get('f1') or 'BRIGHT', opts.get('f2') or 'MOVE', defaults, out, mono=bool(opts.get('mono')),
          max_bytes=int(opts.get('max_bytes') or MAX_BYTES_DEFAULT), autoloop=bool(opts.get('autoloop')),
          loop_min=float(opts.get('loop_min_sec') or 0.6), keyrange=kr, verify=opts.get('verify', True) not in (False, 'false', 0))

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('kind', choices=['tal', 'wavs', 'sfz', 'bank']); ap.add_argument('src')
    ap.add_argument('--name'); ap.add_argument('--f1'); ap.add_argument('--f2'); ap.add_argument('--out')
    ap.add_argument('--mono', action='store_true'); ap.add_argument('--max-bytes', type=int, dest='max_bytes')
    ap.add_argument('--autoloop', action='store_true'); ap.add_argument('--loop-min-sec', type=float, dest='loop_min_sec')
    ap.add_argument('--defaults'); ap.add_argument('--keyrange'); ap.add_argument('--every', type=int); ap.add_argument('--top-velocity-only', action='store_true', dest='top_velocity_only')
    args = ap.parse_args()
    if args.kind == 'bank':
        spec = json.load(open(args.src)); base = os.path.dirname(os.path.abspath(args.src)); outdir = args.out or os.path.join(base, 'runtime', spec.get('folder', 'BANK'))
        os.makedirs(outdir, exist_ok=True)
        open(os.path.join(outdir, 'BANK.TXT'), 'w').write(spec.get('credits', ''))
        for p in spec['presets']:
            extra = dict(p); extra.pop('type'); extra.pop('src')
            extra['out'] = os.path.join(outdir, p['file'])
            run_one(p['type'], os.path.join(base, p['src']) if not os.path.isabs(p['src']) else p['src'], args, extra)
    else:
        run_one(args.kind, args.src, args)

if __name__ == '__main__': main()
