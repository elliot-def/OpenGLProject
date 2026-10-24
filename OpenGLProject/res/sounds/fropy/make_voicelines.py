# Genere les voice lines de fropy a partir des enregistrements de ce dossier :
# debruitage, decoupe en phrases (silences), effet bitcrush (noise gate + 8 bits
# 8 kHz + passe-bas), WAV mono 16 bits (spatialisable par OpenAL) dans voicelines/.
# Relancer apres avoir change une source.
#   python -I make_voicelines.py        (necessite numpy, scipy, soundfile)
import os
import numpy as np
import soundfile as sf
from scipy.signal import butter, istft, resample_poly, sosfiltfilt, stft
from scipy.ndimage import uniform_filter1d

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCES = ['potentiel.wav', 'signal_puissance.mp3']   # "Le potentiel en mecanique.mp3" = potentiel.wav
OUT_DIR = os.path.join(HERE, 'voicelines')
RATE = 8000                  # Hz : echantillonnage du bitcrush (les fichiers sont ecrits a ce rythme)
BITS = 8                     # profondeur du bitcrush (6 bits : pas a -30 dB, le souffle de fond basculait en craquements)
GATE_DB = -40.0              # dB sous le pic : en dessous, silence franc (aucun bit ne bascule)
LOWPASS = 3400.0             # Hz : adoucit le souffle de quantification apres le crush
FRAME = 0.02                 # s : trame d'analyse
MIN_SILENCE = 0.40           # s : un silence plus long separe deux phrases
MIN_LEN, MAX_LEN = 1.0, 6.0  # s : duree d'une voice line
MAX_LINES = 80               # garde-fou : une source OpenAL par ligne en jeu
PAD, FADE = 0.08, 0.015      # s

def load_mono(path):
    x, sr = sf.read(path, dtype='float32', always_2d=True)
    x = x.mean(axis=1)
    g = np.gcd(sr, RATE)
    return resample_poly(x, RATE // g, sr // g).astype(np.float32)

def phrases(x):
    n = int(FRAME * RATE)
    frames = x[:len(x) // n * n].reshape(-1, n)
    db = 20 * np.log10(np.sqrt((frames ** 2).mean(axis=1)) + 1e-9)
    floor, speech = np.percentile(db, 10), np.percentile(db, 90)
    voiced = db > floor + 0.35 * (speech - floor)
    segs, start, quiet = [], None, 0
    gap = int(MIN_SILENCE / FRAME)
    for i, v in enumerate(voiced):
        if v:
            if start is None: start = i
            quiet = 0
        elif start is not None:
            quiet += 1
            if quiet >= gap:
                segs.append((start, i - quiet + 1)); start, quiet = None, 0
    if start is not None: segs.append((start, len(voiced)))
    out = []
    def split(a, b):   # phrase trop longue : coupe au point le plus calme entre MIN_LEN et MAX_LEN
        if (b - a) * FRAME <= MAX_LEN:
            out.append((a, b)); return
        lo, hi = a + int(MIN_LEN / FRAME), a + int(MAX_LEN / FRAME)
        cut = lo + int(np.argmin(db[lo:hi]))
        split(a, cut); split(cut, b)
    for a, b in segs: split(a, b)
    return [(a * n, b * n) for a, b in out if (b - a) * FRAME >= MIN_LEN]

def denoise(x):
    """Soustraction spectrale : profil de bruit = trames les plus calmes de l'enregistrement."""
    _, _, X = stft(x, RATE, nperseg=256)
    mag = np.abs(X)
    energy = mag.sum(axis=0)
    noise = np.median(mag[:, energy <= np.percentile(energy, 15)], axis=1, keepdims=True)
    gain = np.clip(1.0 - (1.6 * noise / (mag + 1e-9)) ** 2, 0.0, 1.0) ** 0.5
    gain = np.maximum(uniform_filter1d(gain, 3, axis=1), 0.03)   # lisse dans le temps : pas de "bulles" musicales
    _, y = istft(X * gain, RATE, nperseg=256)
    return y[:len(x)].astype(np.float32)

def gate(y):
    """Noise gate : enveloppe RMS 10 ms, gain 0 sous GATE_DB (fondu sur 6 dB), attaque 2 ms / relachement 60 ms."""
    env = np.sqrt(uniform_filter1d(y ** 2, int(0.01 * RATE)) + 1e-12)
    db = 20 * np.log10(env / (np.abs(y).max() + 1e-9))
    target = np.clip((db - GATE_DB) / 6.0, 0.0, 1.0)
    g = np.empty_like(target)
    a_att, a_rel = 1 - np.exp(-1 / (0.002 * RATE)), 1 - np.exp(-1 / (0.060 * RATE))
    cur = 0.0
    for i, t in enumerate(target):
        cur += (t - cur) * (a_att if t > cur else a_rel)
        g[i] = cur
    return y * g

def bitcrush(y):
    y = gate(y / (np.abs(y).max() + 1e-9) * 0.89)                # -1 dBFS, puis silences francs
    levels = 2 ** (BITS - 1)
    y = np.round(y * levels) / levels                           # 0 reste 0 (quantification centree)
    y = sosfiltfilt(butter(4, LOWPASS, fs=RATE, output='sos'), y)
    return (y / (np.abs(y).max() + 1e-9) * 0.89).astype(np.float32)

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for f in os.listdir(OUT_DIR):
        if f.endswith('.wav'): os.remove(os.path.join(OUT_DIR, f))
    lines = []
    for src in SOURCES:
        x = denoise(load_mono(os.path.join(HERE, src)))
        for a, b in phrases(x):
            a, b = max(0, a - int(PAD * RATE)), min(len(x), b + int(PAD * RATE))
            lines.append(x[a:b].copy())
    # Trop de lignes : on garde les plus proches de 3 s (phrases completes, ni mots isoles ni tirades).
    if len(lines) > MAX_LINES:
        lines.sort(key=lambda y: abs(len(y) / RATE - 3.0))
        lines = lines[:MAX_LINES]
    f = int(FADE * RATE)
    ramp = np.linspace(0.0, 1.0, f, dtype=np.float32)
    for k, y in enumerate(lines):
        y = bitcrush(y)
        y[:f] *= ramp; y[-f:] *= ramp[::-1]
        sf.write(os.path.join(OUT_DIR, f'fropy_{k:03d}.wav'), y, RATE, subtype='PCM_16')
    total = sum(len(y) for y in lines) / RATE
    print(f'{len(lines)} voice lines, {total:.0f} s au total -> {OUT_DIR}')

if __name__ == '__main__':
    main()
