# Player model de fropy : marionnette low poly articulee + tete de fropy, riggee sur le
# squelette Mixamo (compatible avec les animations de res/rigging/mixamo/animation).
#   python -I make_fropy_player.py      (necessite numpy, Pillow)
#
# Entrees : mixamo_rig.json (squelette de repos des clips Mixamo, exporte par
#   "OpenGLProject.exe -dumprig res/rigging/mixamo/animation/idle.fbx mixamo_rig.json"),
#   fropy_head.obj + fropy_low_poly_color.png (tete).
# Sortie  : fropy_player.glb (glTF 2.0 binaire : noeuds = squelette Mixamo, skin, texture embarquee).
#
# Chaque segment (bassin, buste, bras, jambes...) est un prisme facette attache rigidement a un
# os (poids 1), avec une rotule a chaque articulation : ca plie sans etirement, style low poly.
import io, json, os, re, struct
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'fropy_player.glb')

# ── Squelette ────────────────────────────────────────────────────────────────
rig = json.load(open(os.path.join(HERE, 'mixamo_rig.json')))
nodes = rig['nodes']
local = [np.array(n['local'], dtype=np.float64).reshape(4, 4) for n in nodes]   # assimp : ligne par ligne, vecteurs colonnes
world = []
for i, n in enumerate(nodes):
    world.append(local[i] if n['parent'] < 0 else world[n['parent']] @ local[i])
short = [re.sub(r'^mixamorig\d*:', '', n['name']) for n in nodes]
idx = {s: i for i, s in enumerate(short)}
P = {s: world[i][:3, 3] for i, s in enumerate(short)}            # positions de repos (cm)
joints = [i for i, n in enumerate(nodes) if n['name'].startswith('mixamorig')]

# ── Texture : atlas de la tete (moitie gauche) + aplats du corps (moitie droite) ──
head_tex = Image.open(os.path.join(HERE, 'fropy_low_poly_color.png')).convert('RGBA')
TS = head_tex.width
pal = np.array(head_tex.convert('RGB')).reshape(-1, 3).astype(float)
def mean_near(c, tol=40):
    m = np.abs(pal - np.array(c)).max(axis=1) < tol
    return tuple(int(v) for v in pal[m].mean(axis=0))
SWATCH = {   # couleurs de la tete pour le corps, + accent tomate de la DA (constants/color.h)
    'body': mean_near((40, 24, 24)),
    'skin': mean_near((248, 184, 104)),
    'accent': (187, 68, 48),
    'sole': (24, 20, 18),
    'collar': mean_near((247, 237, 233), 20),   # col blanc du bas de la tete
}
atlas = Image.new('RGBA', (TS * 2, TS), (0, 0, 0, 255))
atlas.paste(head_tex, (0, 0))
sw_uv = {}
B = TS // 4
for k, (name, col) in enumerate(SWATCH.items()):   # blocs de 256 px : surs pour les mipmaps
    x0, y0 = TS + (k % 4) * B, (k // 4) * B
    atlas.paste(Image.new('RGBA', (B, B), col + (255,)), (x0, y0))
    sw_uv[name] = ((x0 + B / 2) / (2 * TS), (y0 + B / 2) / TS)   # UV glTF (origine en haut a gauche)
png = io.BytesIO(); atlas.save(png, 'PNG'); png = png.getvalue()

# ── Geometrie (sommets non partages : facettes a normale plate) ────────────────
pos, nrm, uv, jnt = [], [], [], []
def tri(a, b, c, bone, color=None, uvs=None):
    """bone : un os (poids 1) pour les 3 sommets, ou un poids par sommet [[(os, poids), ...] x 3]."""
    n = np.cross(b - a, c - a)
    ln = np.linalg.norm(n)
    if ln < 1e-9: return
    n /= ln
    for k, p in enumerate((a, b, c)):
        pos.append(p); nrm.append(n)
        jnt.append(bone[k] if isinstance(bone, list) else [(bone, 1.0)])
        uv.append(uvs[k] if uvs is not None else sw_uv[color])

def frame(d):
    d = d / np.linalg.norm(d)
    ref = np.array([0.0, 0.0, 1.0]) if abs(d[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    u = np.cross(ref, d); u /= np.linalg.norm(u)
    return d, u, np.cross(d, u)

def prism(a, b, ra, rb, bone, color, sides=8, flat=(1.0, 1.0), twist=np.pi / 8):
    """Prisme facette de a a b (rayons ra -> rb), section elliptique (flat = echelles x/z), extremites fermees."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d, u, v = frame(b - a)
    if abs(d[1]) > 0.7:   # segment vertical (buste, cou) : section alignee sur x (largeur) / z (profondeur)
        u, v = np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0])
    ring = lambda c, r: [c + r * (flat[0] * np.cos(t) * u + flat[1] * np.sin(t) * v)
                         for t in twist + 2 * np.pi * np.arange(sides) / sides]
    A, B = ring(a, ra), ring(b, rb)
    for i in range(sides):
        j = (i + 1) % sides
        tri(A[i], A[j], B[j], bone, color); tri(A[i], B[j], B[i], bone, color)
        tri(a, A[j], A[i], bone, color); tri(b, B[i], B[j], bone, color)

ICO = None
def ball(c, r, bone, color):
    """Rotule : icosaedre facette."""
    global ICO
    if ICO is None:
        t = (1 + 5 ** 0.5) / 2
        vs = np.array([(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
                       (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)], float)
        vs /= np.linalg.norm(vs, axis=1, keepdims=True)
        fs = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
              (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
        ICO = (vs, fs)
    vs, fs = ICO
    for f in fs:
        tri(*(np.asarray(c) + r * vs[k] for k in f), bone, color)

def toward(a, b, t):
    return np.asarray(P[a]) + (np.asarray(P[b]) - np.asarray(P[a])) * t

# Buste : bassin, ventre, poitrine, epaules (section elliptique, plus large que profonde)
up = np.array([0.0, 1.0, 0.0])
prism(P['Hips'] - 16 * up, P['Spine'], 27, 28, idx['Hips'], 'body', flat=(1.0, 0.68))
prism(P['Hips'] - 2 * up, P['Hips'] + 5 * up, 29.5, 29.5, idx['Hips'], 'accent', flat=(1.0, 0.7))   # ceinture
prism(P['Spine'], P['Spine1'], 28, 29, idx['Spine'], 'body', flat=(1.0, 0.66))
prism(P['Spine1'], P['Spine2'], 29, 32, idx['Spine1'], 'body', flat=(1.0, 0.64))
CHEST_TOP = P['Neck'] - 3 * up
prism(P['Spine2'], CHEST_TOP, 32, 22, idx['Spine2'], 'body', flat=(1.0, 0.72))   # epaules ; le cou part d'ici (cf. col)

# Bras
for s in ('Left', 'Right'):
    prism(P[s + 'Shoulder'], P[s + 'Arm'], 10, 10, idx[s + 'Shoulder'], 'body')
    ball(P[s + 'Arm'], 11, idx[s + 'Arm'], 'body')
    prism(P[s + 'Arm'], P[s + 'ForeArm'], 9.5, 8, idx[s + 'Arm'], 'body')
    ball(P[s + 'ForeArm'], 8.5, idx[s + 'ForeArm'], 'body')
    prism(P[s + 'ForeArm'], P[s + 'Hand'], 8, 6.5, idx[s + 'ForeArm'], 'body')
    prism(toward(s + 'ForeArm', s + 'Hand', 0.82), P[s + 'Hand'], 7.6, 7.6, idx[s + 'ForeArm'], 'accent')   # manchette
    ball(P[s + 'Hand'], 6.5, idx[s + 'Hand'], 'skin')
    prism(P[s + 'Hand'], P[s + 'HandMiddle1'], 6.5, 6.5, idx[s + 'Hand'], 'skin', sides=6, flat=(1.0, 0.55))   # paume
    prism(P[s + 'HandMiddle1'], P[s + 'HandMiddle3'], 6.0, 4.5, idx[s + 'HandMiddle1'], 'skin', sides=6, flat=(1.0, 0.55))  # moufle
    prism(P[s + 'HandThumb1'], P[s + 'HandThumb3'], 3.6, 2.8, idx[s + 'HandThumb1'], 'skin', sides=6)

# Jambes
for s in ('Left', 'Right'):
    ball(P[s + 'UpLeg'], 14, idx[s + 'UpLeg'], 'body')
    prism(P[s + 'UpLeg'], P[s + 'Leg'], 13.5, 10.5, idx[s + 'UpLeg'], 'body')
    ball(P[s + 'Leg'], 11, idx[s + 'Leg'], 'body')
    prism(P[s + 'Leg'], P[s + 'Foot'], 10, 8, idx[s + 'Leg'], 'body')
    ball(P[s + 'Foot'], 8.5, idx[s + 'Foot'], 'sole')
    foot, toe, end = P[s + 'Foot'], P[s + 'ToeBase'], P[s + 'Toe_End']
    sole = lambda p: np.array([p[0], 0.0, p[2]])
    prism(sole(foot) + np.array([0, 8, -6]), sole(toe) + np.array([0, 7, 0]), 9, 8.5, idx[s + 'Foot'], 'sole', flat=(1.0, 0.75))
    prism(sole(toe) + np.array([0, 6.5, 0]), sole(end) + np.array([0, 5.5, 0]), 8, 6.5, idx[s + 'ToeBase'], 'sole', flat=(1.0, 0.7))

# Tete de fropy (base au cou, face +Z) : poids 1 sur Head
hv, hvt, hf = [], [], []
for l in open(os.path.join(HERE, 'fropy_head.obj')):
    t = l.split()
    if not t: continue
    if t[0] == 'v': hv.append(list(map(float, t[1:4])))
    elif t[0] == 'vt': hvt.append(list(map(float, t[1:3])))
    elif t[0] == 'f': hf.append([tuple(int(x) if x else 0 for x in (c.split('/') + ['', ''])[:2]) for c in t[1:]])
hv = np.array(hv)
head_h = P['HeadTop_End'][1] - P['Head'][1]
HEAD_SCALE = 2.0 * head_h / (hv[:, 1].max() - hv[:, 1].min())   # tete "chibi" : 2x la hauteur de la tete du squelette
base = P['Head'] + np.array([0.0, -10.0, 0.0])   # fond de la tete pose juste au-dessus du buste (cf. col)
for f in hf:
    for k in range(1, len(f) - 1):
        cs = (f[0], f[k], f[k + 1])
        ps = [base + HEAD_SCALE * hv[c[0] - 1] for c in cs]
        uvs = [(hvt[c[1] - 1][0] * 0.5, 1.0 - hvt[c[1] - 1][1]) if c[1] else sw_uv['skin'] for c in cs]   # OBJ -> glTF, moitie gauche
        tri(*ps, idx['Head'], uvs=uvs)

# Col : raccord lisse entre le fond (plat et large) de la tete et le haut du buste. Anneaux de
# 24 cotes : empreinte du fond de la tete (enveloppe convexe) -> etranglement -> epaules, avec
# des poids degrades Head -> Neck -> Spine2 pour que le cou se torde en douceur.
def footprint(n):
    from scipy.spatial import ConvexHull
    pts = hv[hv[:, 1] < 0.08][:, [0, 2]] * HEAD_SCALE
    hull = pts[ConvexHull(pts).vertices]
    c = hull.mean(axis=0)
    out = []
    for t in 2 * np.pi * np.arange(n) / n:   # rayon de l'enveloppe dans la direction t
        d = np.array([np.cos(t), np.sin(t)])
        best = 0.0
        for i in range(len(hull)):
            a, b = hull[i] - c, hull[(i + 1) % len(hull)] - c
            m = np.array([[d[0], a[0] - b[0]], [d[1], a[1] - b[1]]])
            if abs(np.linalg.det(m)) < 1e-9: continue
            r, s = np.linalg.solve(m, a)
            if r > 0 and 0 <= s <= 1: best = max(best, r)
        out.append(best)
    return c, np.array(out)
N_COL = 24
fc, fr = footprint(N_COL)
angles = 2 * np.pi * np.arange(N_COL) / N_COL
def ring_at(y, scale, center):
    return [np.array([center[0] + scale(k) * np.cos(t), y, center[1] + scale(k) * np.sin(t)]) for k, t in enumerate(angles)]
head_c = base[[0, 2]] + fc
chest_c = CHEST_TOP[[0, 2]]
chest_r = lambda k: 0.92 / np.sqrt((np.cos(angles[k]) / 22.0) ** 2 + (np.sin(angles[k]) / (22.0 * 0.72)) ** 2)   # ellipse du haut du buste
rings = [   # (anneau, poids)
    (ring_at(CHEST_TOP[1] - 3.0, chest_r, chest_c), [(idx['Spine2'], 1.0)]),
    (ring_at(P['Neck'][1] + 2.0, lambda k: 0.64 * fr[k], (chest_c + head_c) / 2), [(idx['Neck'], 1.0)]),
    (ring_at(base[1] - 2.5, lambda k: 0.72 * fr[k], head_c), [(idx['Neck'], 0.5), (idx['Head'], 0.5)]),
    (ring_at(base[1] + 4.0, lambda k: 0.76 * fr[k], head_c), [(idx['Head'], 1.0)]),   # col roule : rentre dans la tete, sans evasement
]
band_color = ['body', 'collar', 'collar']
for r in range(len(rings) - 1):
    (A, wa), (Bv, wb) = rings[r], rings[r + 1]
    for i in range(N_COL):
        j = (i + 1) % N_COL
        tri(A[i], Bv[j], A[j], [wa, wb, wa], band_color[r]); tri(A[i], Bv[i], Bv[j], [wa, wb, wb], band_color[r])

pos = np.array(pos, np.float32); nrm = np.array(nrm, np.float32); uv = np.array(uv, np.float32)
jidx = {node: k for k, node in enumerate(joints)}
J = np.zeros((len(pos), 4), np.uint16); W = np.zeros((len(pos), 4), np.float32)
for v, ws in enumerate(jnt):
    for k, (b, w) in enumerate(ws[:4]):
        J[v, k] = jidx[b]; W[v, k] = w
I = np.arange(len(pos), dtype=np.uint32)

# ── glTF binaire ─────────────────────────────────────────────────────────────
blob = bytearray(); views = []; accessors = []
def view(data, target=None):
    while len(blob) % 4: blob.append(0)
    views.append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(data), **({'target': target} if target else {})})
    blob.extend(data)
    return len(views) - 1
def accessor(arr, ctype, typ, target=None, minmax=False):
    a = {'bufferView': view(arr.tobytes(), target), 'componentType': ctype, 'count': len(arr), 'type': typ}
    if minmax: a['min'] = arr.min(axis=0).tolist(); a['max'] = arr.max(axis=0).tolist()
    accessors.append(a)
    return len(accessors) - 1
FLOAT, U16, U32, ARRAY, ELEM = 5126, 5123, 5125, 34962, 34963
acc_pos = accessor(pos, FLOAT, 'VEC3', ARRAY, True)
acc_nrm = accessor(nrm, FLOAT, 'VEC3', ARRAY)
acc_uv = accessor(uv, FLOAT, 'VEC2', ARRAY)
acc_j = accessor(J, U16, 'VEC4', ARRAY)
acc_w = accessor(W, FLOAT, 'VEC4', ARRAY)
acc_i = accessor(I, U32, 'SCALAR', ELEM)
ibm = np.array([np.linalg.inv(world[j]).T.reshape(-1) for j in joints], np.float32)   # colonne par colonne
acc_ibm = accessor(ibm, FLOAT, 'MAT4')
img_view = view(png)

gnodes = []
for i, n in enumerate(nodes):
    g = {'name': n['name'], 'matrix': local[i].T.reshape(-1).tolist()}
    kids = [k for k, m in enumerate(nodes) if m['parent'] == i]
    if kids: g['children'] = kids
    gnodes.append(g)
mesh_node = len(gnodes)
gnodes.append({'name': 'fropy_player', 'mesh': 0, 'skin': 0})
gltf = {
    'asset': {'version': '2.0', 'generator': 'make_fropy_player.py'},
    'scene': 0, 'scenes': [{'nodes': [0, mesh_node]}],
    'nodes': gnodes,
    'skins': [{'joints': joints, 'inverseBindMatrices': acc_ibm, 'skeleton': idx['Hips']}],
    'meshes': [{'name': 'fropy_player', 'primitives': [{'attributes': {'POSITION': acc_pos, 'NORMAL': acc_nrm, 'TEXCOORD_0': acc_uv,
                                                                         'JOINTS_0': acc_j, 'WEIGHTS_0': acc_w},
                                                         'indices': acc_i, 'material': 0}]}],
    'materials': [{'name': 'fropy', 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0.0, 'roughnessFactor': 0.85}}],
    'textures': [{'source': 0, 'sampler': 0}],
    'samplers': [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 33071, 'wrapT': 33071}],
    'images': [{'bufferView': img_view, 'mimeType': 'image/png'}],
    'buffers': [{'byteLength': len(blob)}],
    'bufferViews': views, 'accessors': accessors,
}
js = json.dumps(gltf, separators=(',', ':')).encode()
js += b' ' * (-len(js) % 4)
while len(blob) % 4: blob.append(0)
with open(OUT, 'wb') as f:
    f.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob)))
    f.write(struct.pack('<II', len(js), 0x4E4F534A)); f.write(js)
    f.write(struct.pack('<II', len(blob), 0x004E4942)); f.write(blob)
print(f'{OUT} : {len(pos)} sommets, {len(joints)} os, tete x{HEAD_SCALE:.1f}, hauteur {pos[:, 1].max() - pos[:, 1].min():.0f} cm')
