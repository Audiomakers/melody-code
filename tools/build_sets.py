#!/usr/bin/env python3
"""Builds the instruments of the sound sets ("conjuntos") into Assets/Samples and writes
Source/SampleTable.inc (one row per zone) and Source/InstrumentTable.inc (one row per instrument).

usage: build_sets.py <samples root> <vsco root> <repo root>
  samples root: the folder that holds the Audio Makers packs (AudioMakers Trap Essentials, Submundo 808
                Sample Pack), i.e. D:\\Samples\\AudioMakers; the funk kits (Atlantida, Anthem) sit next to
                it in D:\\Samples\\funk
  vsco root:    a checkout of VSCO 2 Community Edition (CC0), github.com/sgossner/VSCO-2-CE

Three kinds of instrument:
  oneshot  one recorded note (Ed's packs, KING DOUDOU, the Atlantida and Anthem funk kits), played as an
           instrument (root measured or checked by hand: the file names lie)
  vsco     a multi-sampled orchestral instrument (names are one octave low: measured, then snapped)
  synth    a timbre synthesized here (FM keys, plucks, sub) and stored as samples every few
           semitones, so the plugin plays it exactly like the demos Ed approved
Every zone is mono 44.1 kHz Ogg Vorbis, its body at -16 dBFS RMS; sustained sounds get a crossfade loop.
"""
import glob, math, os, re, subprocess, sys, tempfile, wave
import numpy as np

SR = 44100
NOTE = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}

# ------------------------------------------------------------------------------------------ synthesis
def hz(m): return 440.0 * 2 ** ((m - 69) / 12)

def saw(freq, phase0=0.0):
    ph = (phase0 + np.cumsum(freq / SR)) % 1.0
    dt = freq / SR
    v = 2 * ph - 1
    a = ph < dt; t = ph[a] / dt[a]; v[a] -= 2 * t - t * t - 1           # polyBLEP
    b = ph > 1 - dt; t = (ph[b] - 1) / dt[b]; v[b] -= t * t + 2 * t + 1
    return v

def svf_lp(x, cutoff, res):
    out = np.empty_like(x); ic1 = ic2 = 0.0; k = 2 - 2 * res
    g_all = np.tan(np.pi * np.minimum(cutoff, SR * 0.45) / SR)
    for i in range(len(x)):
        g = g_all[i]; a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
        v3 = x[i] - ic2; v1 = a1 * ic1 + a2 * v3; v2 = ic2 + a2 * ic1 + a3 * v3
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; out[i] = v2
    return out

def env(n, a, d, s):
    t = np.arange(n) / SR
    return np.where(t < a, t / max(a, 1e-4), s + (1 - s) * np.exp(-(t - a) / max(d, 1e-4)))

def pluck(m, secs, p, vel=100):
    n = int(secs * SR); t = np.arange(n) / SR; f = hz(m); det = p.get('detune', 8) / 1200
    x = sum(saw(np.full(n, f * 2 ** d), (k * 0.31) % 1) for k, d in enumerate((-det, 0, det))) / 3
    if p.get('square'): x = x + p['square'] * np.sign(np.sin(2 * np.pi * f * t))
    cut = p.get('cut', 900) + p.get('env', 5000) * (vel / 127) * np.exp(-t / p.get('fdecay', 0.12))
    return svf_lp(x, cut, p.get('res', 0.25)) * env(n, 0.002, p.get('decay', 0.35), p.get('sustain', 0.0))

def fm(m, secs, p, vel=100):
    n = int(secs * SR); t = np.arange(n) / SR; f = hz(m)
    idx = p.get('index', 2.0) * (0.4 + 0.6 * vel / 127) * np.exp(-t / p.get('idecay', 0.4)) + p.get('index_sus', 0.2)
    car = np.sin(2 * np.pi * f * t + np.sin(2 * np.pi * f * p.get('ratio', 1.0) * t) * idx)
    if p.get('tine'): car += p['tine'] * np.sin(2 * np.pi * f * 14 * t) * np.exp(-t / 0.03)
    return car * env(n, p.get('attack', 0.002), p.get('decay', 1.2), p.get('sustain', 0.0))

def sub(m, secs, p, vel=100):
    n = int(secs * SR); ph = 2 * np.pi * hz(m) * np.arange(n) / SR
    x = np.sin(ph) + p.get('h2', 0.15) * np.sin(2 * ph) + p.get('h3', 0.05) * np.sin(3 * ph)
    d = p.get('drive', 1.3)
    return np.tanh(x * d) / np.tanh(d) * env(n, 0.004, p.get('decay', 3.0), p.get('sustain', 0.8))

SYNTHS = {'pluck': pluck, 'fm': fm, 'sub': sub}

EP = dict(ratio=1.0, index=1.6, idecay=0.6, index_sus=0.15, decay=1.8, sustain=0.25, tine=0.25)
KEYS = dict(ratio=1.0, index=1.2, idecay=0.5, index_sus=0.1, decay=1.0, sustain=0.2, tine=0.15)
MARIMBA = dict(ratio=4.0, index=1.4, idecay=0.05, index_sus=0.0, decay=0.45)
SOFT_PLUCK = dict(cut=700, env=2200, fdecay=0.15, decay=0.9, sustain=0.2, detune=6)
STABS = dict(cut=900, env=2500, fdecay=0.08, decay=0.22, sustain=0.05, detune=10)
SUB_ROUND = dict(h2=0.25, h3=0.08, drive=1.6, decay=1.2, sustain=0.6)
SUB_WARM = dict(h2=0.35, h3=0.12, drive=1.4, decay=0.9, sustain=0.5)

# ------------------------------------------------------------------------------------------ instruments
# name: source, then the playing shape: center (where a line should sit; the plugin moves whole lines by
# octaves toward it), lowest/highest (single notes fold inside), attack and release seconds, level.
MEL = 'Submundo 808 Sample Pack/Melodic_One_Shots/'
AMK = 'AudioMakers Trap Essentials/One_Shots/Melodic/'
T808 = 'AudioMakers Trap Essentials/One_Shots/808/'
# third-party kits, next to the Audio Makers folder
ATL = '../funk/NEW BRAZILIAN FUNK PHONK DRUMKIT FROM ATLANTIDA/'
RIO = ATL + 'LIFE IN RIO STASH/'
ANTHEM = '../funk/[Anthem] Funk Drum Kit Vol. 1/'
I = {}
def inst(name, center, lo, hi, attack, release, level, **src):
    I[name] = dict(center=center, lo=lo, hi=hi, attack=attack, release=release, level=level, **src)

# melody
inst('bell_glass', 74, 60, 88, 0.002, 0.20, 1.00, oneshot=MEL + 'BELL NS 10.wav', keep=5.0)
inst('flute',      72, 60, 86, 0.004, 0.09, 0.95, oneshot=MEL + 'FLAUTA NS 8.wav', keep=1.5)
inst('marimba',    76, 60, 92, 0.001, 0.05, 1.00, synth=('fm', MARIMBA), roots=range(56, 97, 4), secs=1.2)
inst('music_box',  76, 64, 92, 0.001, 0.30, 1.00, oneshot=MEL + 'KING_DOUDOU_one_shot_music_box_C.wav', root=59.93, keep=1.0)
inst('bell_soft',  74, 60, 88, 0.002, 0.15, 1.00, oneshot=MEL + 'BELL NS 7.wav', keep=2.2)
inst('bleep',      74, 60, 90, 0.001, 0.08, 0.90, oneshot=MEL + 'KING_DOUDOU_one_shot_synth_digi_bleep_C.wav', root=71.97, keep=1.5)
inst('velvet',     72, 58, 86, 0.003, 0.15, 1.00, oneshot=MEL + '@kotimoficial - One Shot 36.wav', keep=5.0)
inst('horn',       67, 45, 77, 0.030, 0.12, 0.95, vsco='Brass/F Horn/sus', pick='_v2', loop=(1.0, 5.0))
inst('clarinet',   70, 52, 89, 0.020, 0.10, 0.95, vsco='Woodwinds/Clarinet/susLong', pick='_v2', loop=(1.0, 4.0))
inst('harp',       72, 40, 96, 0.001, 0.20, 1.00, vsco='Strings/Harp', keep=4.0)
inst('cowbell',    68, 56, 84, 0.001, 0.05, 0.85, oneshot=RIO + '$B-COWBELLS (16).wav', root=66.77, keep=1.2)
inst('spiccato',   72, 55, 86, 0.002, 0.04, 1.00, vsco='Strings/Violin Section/Spic', pick='_v2_rr1', keep=1.2)
# chords
inst('ep',         60, 46, 76, 0.002, 0.08, 1.00, synth=('fm', EP), roots=range(44, 81, 3), secs=4.0, loop=(1.6, 3.8))
inst('piano',      60, 46, 76, 0.002, 0.25, 1.00, oneshot=MEL + 'PIANO NS 2.wav', keep=6.0)
inst('violin_ns',  62, 52, 76, 0.150, 0.12, 0.85, oneshot=MEL + 'VIOLINO NS 4.wav', loop=(1.0, 2.7))
inst('soft_pluck', 62, 46, 78, 0.002, 0.06, 1.00, synth=('pluck', SOFT_PLUCK), roots=range(44, 81, 4), secs=4.0, loop=(2.0, 3.8))
inst('air_pluck',  64, 52, 80, 0.002, 0.15, 0.90, oneshot=AMK + 'AMKR_Melodic_pluck_air_G#.wav', keep=6.0)
inst('stabs',      62, 46, 78, 0.001, 0.03, 1.00, synth=('pluck', STABS), roots=range(44, 81, 4), secs=1.0)
inst('strings',    58, 36, 86, 0.080, 0.15, 0.85, vsco_ens=[('Strings/Cello Section/susvib', '_v3', 0, 55),
                                                            ('Strings/Viola Section/susvib', '_v2', 55, 64),
                                                            ('Strings/Violin Section/susVib', '_v2', 64, 128)], loop=(1.2, 6.0))
inst('pizzicato',  66, 55, 86, 0.001, 0.08, 1.00, vsco='Strings/Violin Section/Pizz', pick='_v2_rr1', keep=1.5)
inst('horn_stac',  60, 45, 77, 0.002, 0.04, 1.00, vsco='Brass/F Horn/stac', pick='_v2_rr1', keep=1.0)
inst('rio_synth',  50, 40, 62, 0.001, 0.03, 1.00, oneshot=RIO + 'Rio Synth [C].wav', root=48.26, keep=0.5)   # played on chord roots only
# bass
inst('sub_round',  36, 26, 48, 0.004, 0.02, 1.00, synth=('sub', SUB_ROUND), roots=range(24, 53, 4), secs=3.0, loop=(1.5, 2.9))
inst('sub_warm',   38, 26, 50, 0.004, 0.02, 1.00, synth=('sub', SUB_WARM), roots=range(24, 53, 4), secs=3.0, loop=(1.5, 2.9))
inst('bass808',        33, 24, 45, 0.001, 0.03, 1.00, oneshot=T808 + 'AMKR_808_deal_C.wav', keep=3.5)
inst('bass_rio',    33, 24, 45, 0.001, 0.03, 1.00, oneshot=RIO + 'Rio Bass.wav', root=30.98, keep=1.9)
inst('bass_grave2', 34, 24, 46, 0.001, 0.03, 1.00, oneshot=ATL + 'BASS/Grave 2.wav', root=34.3, keep=3.5)
inst('bass_anthem', 31, 22, 43, 0.001, 0.03, 1.00, oneshot=ANTHEM + 'Baixos/Anthem [Baixo] (3).wav', root=24.01, keep=1.0)
inst('tuba',       38, 28, 55, 0.020, 0.10, 1.00, vsco='Brass/Tuba/sus', pick='_v2', loop=(1.0, 3.0))
inst('contrabass', 40, 28, 55, 0.001, 0.10, 1.00, vsco='Strings/Solo Contrabass/Pizz', pick='_v3_rr1', keep=2.0)

# ------------------------------------------------------------------------------------------ helpers
def decode(path):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-ac', '1', '-ar', str(SR), '-f', 'f32le', '-'],
                         check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).astype(np.float64)

def measure_root(x, guess=None):
    """fundamental in fractional MIDI: pyin for the octave, then an FFT peak for the exact pitch"""
    import librosa
    seg = x[int(0.03 * SR):int(min(len(x) / SR, 1.2) * SR)]
    f, _, _ = librosa.pyin(seg, fmin=25, fmax=2500, sr=SR, frame_length=4096)
    f = f[~np.isnan(f)]
    est = float(np.median(librosa.hz_to_midi(f))) if len(f) > 3 else float(guess)
    if guess is not None: est -= 12 * round((est - guess) / 12)   # pyin octave slips: trust the octave we expect
    w = x[int(0.02 * SR):int(min(len(x) / SR, 1.5) * SR)]
    w = w * np.hanning(len(w)); S = np.abs(np.fft.rfft(w, 1 << 20)); fr = np.fft.rfftfreq(1 << 20, 1 / SR)
    t = hz(round(est)); band = (fr > t * 0.96) & (fr < t * 1.04)
    return float(69 + 12 * np.log2(fr[np.argmax(S * band)] / 440.0))

def named(fn):
    m = re.search(r'_([A-G])(#?)(-?\d)(?=[_.])', fn)
    return None if not m else 12 * (int(m.group(3)) + 1) + NOTE[m.group(1)] + (1 if m.group(2) else 0)

def prepare(x, keep, loop):
    above = np.nonzero(np.abs(x) > 0.004 * max(1e-9, np.abs(x).max()))[0]
    x = x[max(0, above[0] - 16):] if len(above) else x
    ls = le = 0
    if loop:
        ls = int(loop[0] * SR); le = min(int(loop[1] * SR), len(x) - 1)
        xf = int(min(0.25, (le - ls) / 3 / SR) * SR)
        if le - ls > 2 * xf and ls > xf:
            f = np.linspace(0, 1, xf)
            x = x[:le].copy()
            x[le - xf:le] = x[le - xf:le] * np.sqrt(1 - f) + x[ls - xf:ls] * np.sqrt(f)   # equal power
        else:
            ls = le = 0
    if not le:
        x = x[:int(keep * SR)].copy()
        n = min(len(x) // 3, int(0.4 * SR))
        x[-n:] *= np.linspace(1, 0, n) ** 2
    body = x[:int(0.4 * SR)]
    rms = float(np.sqrt(np.mean(body ** 2)) + 1e-9)
    g = min(10 ** (-16 / 20) / rms, 0.89 / (float(np.max(np.abs(x))) + 1e-9))
    return (x * g).astype(np.float32), ls, le

def encode(x, out):
    with tempfile.NamedTemporaryFile(suffix='.wav') as t:
        with wave.open(t.name, 'wb') as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
            w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', t.name, '-c:a', 'libvorbis', '-q:a', '4', out], check=True)

def vsco_zones(root, folder, pick, lo=0, hi=128):
    byn = {}
    for f in sorted(glob.glob(os.path.join(root, folder, '*.wav'))):
        n = named(os.path.basename(f))
        if n is not None: byn.setdefault(n, []).append(f)
    out = []
    for n, lst in sorted(byn.items()):
        f = next((g for g in lst if pick and pick in os.path.basename(g)), lst[len(lst) // 2])
        x = decode(f)
        r = measure_root(x[np.argmax(np.abs(x) > 0.01 * np.abs(x).max()):], n + (0 if 'Harp' in folder else 12))
        if lo <= r < hi: out.append((r, x))
    return out

# ------------------------------------------------------------------------------------------ main
def main(samples, vsco, repo):
    out_dir = os.path.join(repo, 'Assets', 'Samples')
    os.makedirs(out_dir, exist_ok=True)
    for f in glob.glob(os.path.join(out_dir, '*.ogg')): os.remove(f)
    zones, rows = [], []
    for name, d in I.items():
        loop = d.get('loop'); keep = d.get('keep', 6.0)
        if 'oneshot' in d:
            x = decode(os.path.join(samples, d['oneshot']))
            # hand-checked roots where pyin is unreliable (bells, gliding 808s)
            src = [(d['root'] if 'root' in d else measure_root(x[np.argmax(np.abs(x) > 0.01 * np.abs(x).max()):]), x)]
        elif 'synth' in d:
            kind, p = d['synth']
            src = [(float(r), SYNTHS[kind](r, d['secs'], p)) for r in d['roots']]
        elif 'vsco' in d:
            src = vsco_zones(vsco, d['vsco'], d.get('pick'))
        else:
            src = [z for folder, pick, lo, hi in d['vsco_ens'] for z in vsco_zones(vsco, folder, pick, lo, hi)]
        for k, (r, x) in enumerate(sorted(src, key=lambda z: z[0])):
            y, ls, le = prepare(x, keep, loop)
            res = f'{name}_{k}'
            encode(y, os.path.join(out_dir, res + '.ogg'))
            # root in cents, so detuned recordings play in tune
            zones.append((name, int(round(r * 100)), ls, le, res + '_ogg'))
        rows.append((name, d['center'], d['lo'], d['hi'], d['attack'], d['release'], d['level']))
        print(f'{name:12s} {len(src):2d} zones  roots ' + ' '.join(f'{r:.2f}' for r, _ in sorted(src, key=lambda z: z[0])))
    with open(os.path.join(repo, 'Source', 'SampleTable.inc'), 'w') as t:
        t.write('// generated by tools/build_sets.py: instrument, root (MIDI cents), loop start, loop end, resource\n')
        for z in zones: t.write('{ "%s", %d, %d, %d, "%s" },\n' % z)
    with open(os.path.join(repo, 'Source', 'InstrumentTable.inc'), 'w') as t:
        t.write('// generated by tools/build_sets.py: instrument, center, lowest, highest, attack s, release s, level\n')
        for r in rows: t.write('{ "%s", %d, %d, %d, %.3ff, %.3ff, %.2ff },\n' % r)
    print(len(zones), 'zones,', len(rows), 'instruments')

if __name__ == '__main__':
    main(*sys.argv[1:4])
