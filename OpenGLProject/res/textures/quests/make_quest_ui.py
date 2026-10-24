# Visuels low poly de l'UI des quetes (panneaux, icones), palette du jeu
# (constants/color.h) : fond sombre facette, accents tomate, lueur teal (cf. logo).
#   python -I make_quest_ui.py      (necessite numpy, scipy, Pillow)
import os
import numpy as np
from PIL import Image, ImageDraw
from scipy.spatial import Delaunay

HERE = os.path.dirname(os.path.abspath(__file__))
SS = 2   # sur-echantillonnage (bords nets apres reduction)

SHADOW = (35, 31, 32)
TOMATO = (187, 68, 48)
TEAL = (126, 189, 194)
VANILLA = (243, 223, 162)
LINEN = (239, 230, 221)


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def shade(c, k):
    return tuple(max(0, min(255, int(round(v * k)))) for v in c)


def chamfer(x0, y0, x1, y1, c):
    return [(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c), (x1 - c, y1), (x0 + c, y1), (x0, y1 - c), (x0, y0 + c)]


def facets(draw, w, h, color_at, step, rng, mask=None):
    """Triangulation jitteree de [0,w]x[0,h] ; color_at(cx, cy, rnd) donne la couleur d'une facette."""
    xs = np.arange(0, w + step, step, dtype=float)
    ys = np.arange(0, h + step, step, dtype=float)
    pts = np.array([(x, y) for y in ys for x in xs])
    inner = (pts[:, 0] > 0) & (pts[:, 0] < w) & (pts[:, 1] > 0) & (pts[:, 1] < h)
    pts[inner] += rng.uniform(-0.42, 0.42, size=(inner.sum(), 2)) * step
    for tri in Delaunay(pts).simplices:
        p = pts[tri]
        cx, cy = p[:, 0].mean(), p[:, 1].mean()
        if mask is not None and not mask(cx, cy): continue
        draw.polygon([tuple(q) for q in p], fill=color_at(cx, cy, rng.random()))


def finish(img, path):
    img = img.resize((img.width // SS, img.height // SS), Image.LANCZOS)
    img.save(os.path.join(HERE, path))
    print('ecrit', path, img.size)


def panel(path, W, H, title_band=True, seed=1):
    """Grand panneau de mini-jeu : facettes sombres, haut rougeoyant, bas teal, cadre tomate chanfreine."""
    w, h, c = W * SS, H * SS, 34 * SS
    rng = np.random.default_rng(seed)
    img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    m = Image.new('L', (w, h), 0)
    ImageDraw.Draw(m).polygon(chamfer(0, 0, w - 1, h - 1, c), fill=255)
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 255))
    d = ImageDraw.Draw(layer)

    def col(cx, cy, r):
        t = cy / h
        base = shade(SHADOW, 0.72 + 0.38 * r)
        if title_band and cy < 120 * SS:
            base = mix(base, TOMATO, 0.10 + 0.08 * r)          # bandeau de titre
        base = mix(base, TEAL, max(0.0, t - 0.55) * 0.30)      # lueur teal en bas (logo)
        return base + (240,)
    facets(d, w, h, col, 70 * SS, rng)
    img.paste(layer, (0, 0), m)
    d = ImageDraw.Draw(img)
    d.line(chamfer(4 * SS, 4 * SS, w - 5 * SS, h - 5 * SS, c - 2 * SS) + [(4 * SS + c - 2 * SS, 4 * SS)], fill=TOMATO + (255,), width=5 * SS)
    d.line(chamfer(16 * SS, 16 * SS, w - 17 * SS, h - 17 * SS, c - 8 * SS) + [(16 * SS + c - 8 * SS, 16 * SS)], fill=TEAL + (70,), width=2 * SS)
    if title_band:
        d.line([(40 * SS, 120 * SS), (w - 40 * SS, 120 * SS)], fill=TOMATO + (160,), width=2 * SS)
    finish(img, path)


def hud_panel(path, W, H, seed=2):
    """Panneau de la liste des taches (HUD) : plus discret, barre tomate a gauche."""
    w, h, c = W * SS, H * SS, 18 * SS
    rng = np.random.default_rng(seed)
    img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    m = Image.new('L', (w, h), 0)
    ImageDraw.Draw(m).polygon(chamfer(0, 0, w - 1, h - 1, c), fill=255)
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 255))
    facets(ImageDraw.Draw(layer), w, h, lambda cx, cy, r: shade(SHADOW, 0.65 + 0.35 * r) + (215,), 46 * SS, rng)
    img.paste(layer, (0, 0), m)
    d = ImageDraw.Draw(img)
    b = 8 * SS   # barre tomate le long du bord gauche (suit les chanfreins)
    d.polygon([(0, c), (c, 0), (c + b, 0), (b, c + b), (b, h - c - b), (c + b, h), (c, h), (0, h - c)], fill=TOMATO + (255,))
    d.line(chamfer(2 * SS, 2 * SS, w - 3 * SS, h - 3 * SS, c - 1 * SS) + [(2 * SS + c - SS, 2 * SS)], fill=TEAL + (60,), width=2 * SS)
    finish(img, path)


def facet_poly(d, poly, color, rng, light=(0.6, -0.8)):
    """Polygone convexe decoupe en eventail de facettes eclairees (aspect low poly)."""
    poly = np.array(poly, dtype=float)
    ctr = poly.mean(axis=0) + rng.uniform(-0.08, 0.08, 2) * (poly.max(axis=0) - poly.min(axis=0))
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        n = np.array([(a[1] + b[1]) / 2 - ctr[1], -((a[0] + b[0]) / 2 - ctr[0])])
        mid = (a + b) / 2 - ctr
        k = 0.78 + 0.32 * max(-1.0, min(1.0, (mid[0] * light[0] + mid[1] * light[1]) / (np.linalg.norm(mid) + 1e-6)))
        d.polygon([tuple(ctr), tuple(a), tuple(b)], fill=shade(color, k * (0.95 + 0.1 * rng.random())) + (255,))


def ring(cx, cy, r, n, start=0.0):
    return [(cx + r * np.cos(start + 2 * np.pi * i / n), cy + r * np.sin(start + 2 * np.pi * i / n)) for i in range(n)]


def icon(path, painter, seed):
    S = 192 * SS
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(seed)
    facet_poly(d, ring(S / 2, S / 2, S * 0.47, 8, np.pi / 8), shade(SHADOW, 1.25), rng)   # medaillon octogonal
    d.line(ring(S / 2, S / 2, S * 0.47, 8, np.pi / 8) + [ring(S / 2, S / 2, S * 0.47, 8, np.pi / 8)[0]], fill=TOMATO + (255,), width=4 * SS)
    painter(d, S, rng)
    finish(img, path)


def paint_wires(d, S, rng):
    for k, colr in enumerate((TOMATO, TEAL, VANILLA)):
        y0 = S * (0.33 + 0.17 * k)
        y1 = S * (0.33 + 0.17 * ((k + 1) % 3))
        pts = [(S * 0.22, y0), (S * 0.42, y0), (S * 0.58, y1), (S * 0.78, y1)]
        d.line(pts, fill=shade(colr, 0.8) + (255,), width=int(S * 0.07), joint='curve')
        d.line(pts, fill=colr + (255,), width=int(S * 0.045), joint='curve')
        for x in (S * 0.22, S * 0.78):
            facet_poly(d, ring(x, y0 if x < S / 2 else y1, S * 0.06, 6), LINEN, rng)


def paint_simon(d, S, rng):
    c, r = S / 2, S * 0.17
    for (dx, dy), colr in zip(((0, -1), (1, 0), (0, 1), (-1, 0)), (TOMATO, TEAL, VANILLA, LINEN)):
        x, y = c + dx * r * 1.15, c + dy * r * 1.15
        facet_poly(d, [(x, y - r * 0.8), (x + r * 0.8, y), (x, y + r * 0.8), (x - r * 0.8, y)], colr, rng)


def paint_gauge(d, S, rng):
    c = (S / 2, S * 0.60)
    n = 9
    for i in range(n):
        a0, a1 = np.pi + np.pi * i / n, np.pi + np.pi * (i + 1) / n - 0.06
        ro, ri = S * 0.36, S * 0.24
        poly = [(c[0] + ro * np.cos(a0), c[1] + ro * np.sin(a0)), (c[0] + ro * np.cos(a1), c[1] + ro * np.sin(a1)),
                (c[0] + ri * np.cos(a1), c[1] + ri * np.sin(a1)), (c[0] + ri * np.cos(a0), c[1] + ri * np.sin(a0))]
        colr = TEAL if i in (5, 6) else (TOMATO if i > 6 else VANILLA)
        facet_poly(d, poly, colr, rng)
    a = np.pi * 1.68
    facet_poly(d, [(c[0] + S * 0.33 * np.cos(a), c[1] + S * 0.33 * np.sin(a)),
                   (c[0] + S * 0.035 * np.cos(a + np.pi / 2), c[1] + S * 0.035 * np.sin(a + np.pi / 2)),
                   (c[0] - S * 0.05 * np.cos(a), c[1] - S * 0.05 * np.sin(a)),
                   (c[0] + S * 0.035 * np.cos(a - np.pi / 2), c[1] + S * 0.035 * np.sin(a - np.pi / 2))], LINEN, rng)


def paint_wave(d, S, rng):
    xs = np.linspace(S * 0.18, S * 0.82, 13)
    top = [(x, S * 0.5 - S * 0.16 * np.sin((x - S * 0.18) / (S * 0.64) * 2 * np.pi * 1.5) - S * 0.03) for x in xs]
    bot = [(x, y + S * 0.06) for x, y in top]
    for i in range(len(xs) - 1):
        facet_poly(d, [top[i], top[i + 1], bot[i + 1], bot[i]], TEAL if i % 2 else shade(TEAL, 0.85), rng)
    facet_poly(d, [(S * 0.47, S * 0.20), (S * 0.53, S * 0.20), (S * 0.55, S * 0.30), (S * 0.45, S * 0.30)], TOMATO, rng)


def small_icon(path, kind, seed):
    S = 64 * SS
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(seed)
    if kind == 'done':
        facet_poly(d, ring(S / 2, S / 2, S * 0.44, 6, np.pi / 6), shade(TEAL, 0.55), rng)
        d.line([(S * 0.28, S * 0.52), (S * 0.44, S * 0.68), (S * 0.74, S * 0.34)], fill=LINEN + (255,), width=int(S * 0.11), joint='curve')
    else:
        d.polygon(ring(S / 2, S / 2, S * 0.40, 4), outline=VANILLA + (255,), width=int(S * 0.08))
        facet_poly(d, ring(S / 2, S / 2, S * 0.16, 4), TOMATO, rng)
    finish(img, path)


if __name__ == '__main__':
    panel('panel.png', 1180, 760)
    hud_panel('hud_panel.png', 600, 360)
    icon('icon_wires.png', paint_wires, 11)
    icon('icon_simon.png', paint_simon, 12)
    icon('icon_timing.png', paint_gauge, 13)
    icon('icon_wave.png', paint_wave, 14)
    small_icon('task_done.png', 'done', 21)
    small_icon('task_todo.png', 'todo', 22)
    Image.new('RGBA', (4, 4), (8, 6, 7, 255)).save(os.path.join(HERE, 'dim.png'))   # voile plein ecran (opacite en jeu)
