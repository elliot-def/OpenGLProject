#!/usr/bin/env python3
"""Inspecte les matrices de f_pinky.03.R et voisins (idle) pour comprendre l'anomalie."""
import json
import struct
import numpy as np

GLB = "OpenGLProject/res/rigging/arm/arms_rig.glb"
d = open(GLB, "rb").read()
off = 12
chunks = {}
while off < len(d):
    clen, ctype = struct.unpack_from("<II", d, off)
    chunks[ctype] = (off + 8, clen)
    off += 8 + clen
js = json.loads(d[chunks[0x4E4F534A][0]:chunks[0x4E4F534A][0] + chunks[0x4E4F534A][1]].decode("utf-8"))
BIN = d[chunks[0x004E4942][0]:chunks[0x004E4942][0] + chunks[0x004E4942][1]]


def accessor(ai):
    acc = js["accessors"][ai]
    bv = js["bufferViews"][acc["bufferView"]]
    start = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    fmt = {5121: "B", 5123: "H", 5126: "f"}[acc["componentType"]]
    elems = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[acc["type"]]
    return np.frombuffer(BIN, dtype="<" + fmt, count=acc["count"] * elems, offset=start).reshape(acc["count"], elems)


def m4_flat(v):
    return np.array(v).reshape(4, 4, order="F")


def quat_m4(q):
    q = q / np.linalg.norm(q)
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0],
        [0, 0, 0, 1.0]])


def node_local(n):
    if "matrix" in n:
        return m4_flat(n["matrix"])
    t = n.get("translation", [0, 0, 0])
    r = n.get("rotation", [0, 0, 0, 1])
    s = n.get("scale", [1, 1, 1])
    T = np.eye(4); T[:3, 3] = t
    S = np.eye(4); S[0, 0], S[1, 1], S[2, 2] = s
    return T @ quat_m4(r) @ S


nodes = js["nodes"]
N = len(nodes)
children_of = {i: [] for i in range(N)}
for i, n in enumerate(nodes):
    for c in n.get("children", []):
        children_of[i].append(c)
name_of = [n.get("name", f"node{i}") for i, n in enumerate(nodes)]
idx_of = {name_of[i]: i for i in range(N)}
skin = js["skins"][0]
ibm = accessor(skin["inverseBindMatrices"])
IBM = np.stack([m4_flat(r) for r in ibm])
JOINT_NAME = [name_of[j] for j in skin["joints"]]
JOINT_IDX = {nm: k for k, nm in enumerate(JOINT_NAME)}
scene = js["scenes"][js.get("scene", 0)]
roots = scene["nodes"]

world_bind = {}


def walk_bind(i, W):
    world_bind[i] = W
    for c in children_of[i]:
        walk_bind(c, W @ node_local(nodes[c]))


for r in roots:
    walk_bind(r, np.eye(4))


def eval_anim(anim, t):
    out = {}
    for ch in anim["channels"]:
        node = ch["target"]["node"]
        path = ch["target"]["path"]
        sam = anim["samplers"][ch["sampler"]]
        times = accessor(sam["input"]).flatten()
        vals = accessor(sam["output"])
        interp = sam.get("interpolation", "LINEAR")
        nk = len(times)
        if t <= times[0]:
            k, f = 0, 0.0
        elif t >= times[-1]:
            k, f = (nk - 2, 1.0) if nk >= 2 else (0, 0.0)
        else:
            k = max(0, min(int(np.searchsorted(times, t, side="right")) - 1, nk - 2))
            f = 0.0 if times[k + 1] == times[k] else (t - times[k]) / (times[k + 1] - times[k])
        if path == "rotation":
            q0 = vals[k].astype(float)
            q1 = vals[k + 1].astype(float)
            dot = float(np.dot(q0, q1))
            if dot < 0:
                q1 = -q1
                dot = -dot
            if dot > 0.9999 or interp == "STEP" or f >= 1.0:
                q = q0 if f < 1.0 else q1
            else:
                th = np.arccos(min(1.0, dot))
                w = np.sin((1 - f) * th) / np.sin(th)
                v = np.sin(f * th) / np.sin(th)
                q = w * q0 + v * q1
            out.setdefault(node, {})["rotation"] = q
        else:
            v0 = vals[k].astype(float)
            v1 = vals[k + 1].astype(float)
            out.setdefault(node, {})[path] = v0 if interp == "STEP" or f == 0 else (1 - f) * v0 + f * v1
    return out


def anim_local(n, trs, channelled):
    if channelled:
        t = trs.get("translation", np.zeros(3))
        r = trs.get("rotation", np.array([0.0, 0.0, 0.0, 1.0]))
        T = np.eye(4); T[:3, 3] = t
        return T @ quat_m4(r)
    return node_local(n)


def runtime_world(anim, t):
    trs = eval_anim(anim, t)
    channelled = set(trs.keys())
    G = {}

    def walk(i, P):
        n = nodes[i]
        L = anim_local(n, trs.get(i, {}), i in channelled)
        G[i] = P @ L
        for c in children_of[i]:
            walk(c, G[i])

    for r in roots:
        walk(r, np.eye(4))
    return G


idle = next(a for a in js["animations"] if a.get("name") == "finger_gun_idle")
G = runtime_world(idle, 0.0)

print("hierarchie droite :")
h = idx_of["hand.R"]
print("  hand.R enfants:", [name_of[c] for c in children_of[h]])

for nm in ["hand.R", "palm.01.R", "f_ring.01.R", "f_ring.02.R", "f_ring.03.R", "f_pinky.01.R", "f_pinky.02.R", "f_pinky.03.R"]:
    i = idx_of[nm]
    jk = JOINT_IDX[nm]
    bw = world_bind[i][:3, 3]
    g = G[i][:3, 3]
    O = IBM[jk]
    M = G[i] @ O
    # position visuelle du joint = G (col3)
    print(f"  {nm:13s} bind=({bw[0]:6.3f},{bw[1]:6.3f},{bw[2]:6.3f})  G(idle)=({g[0]:6.3f},{g[1]:6.3f},{g[2]:6.3f})  M col3=({M[0,3]:6.3f},{M[1,3]:6.3f},{M[2,3]:6.3f})")
