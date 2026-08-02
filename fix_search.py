#!/usr/bin/env python3
"""Valide le choix final : idle=finger_gun_idle, offset bras G = (60,70) X/X, propagation."""
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
JOINT_IDX = {name_of[j]: k for k, j in enumerate(skin["joints"])}
scene = js["scenes"][js.get("scene", 0)]
roots = scene["nodes"]


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


def runtime_world(anim, t, offsets=None, propagate=True):
    trs = eval_anim(anim, t) if anim is not None else {}
    channelled = set(trs.keys())
    G = {}
    final = {}

    def walk(i, P):
        n = nodes[i]
        L = anim_local(n, trs.get(i, {}), i in channelled)
        G[i] = P @ L
        off = offsets.get(name_of[i]) if offsets else None
        O = IBM[JOINT_IDX[name_of[i]]] if name_of[i] in JOINT_IDX else np.eye(4)
        final[i] = (G[i] @ off @ O) if off is not None else (G[i] @ O)
        for c in children_of[i]:
            walk(c, G[i] @ off if (off is not None and propagate) else G[i])

    for r in roots:
        walk(r, np.eye(4))
    return G, final


def rot_x(deg):
    th = np.radians(deg)
    c, s = np.cos(th), np.sin(th)
    return np.array([[1, 0, 0, 0], [0, c, -s, 0], [0, s, c, 0], [0, 0, 0, 1]])


def cam_from_rig(p):
    p = p * np.array([0.65, 0.5, 0.5])
    p = np.array([-p[0], p[1], -p[2]])
    return p + np.array([0.0, -1.0, -0.55])


JOINTS = skin["joints"]  # index de skin -> index de noeud


def skinned_aabb(final, verts, j0, w0):
    mn = np.full(3, 1e9)
    mx = np.full(3, -1e9)
    for v, jv, wv in zip(verts, j0, w0):
        bt = np.zeros((4, 4))
        for b, wt in zip(jv, wv):
            if wt > 0.0001 and b < len(JOINTS) and JOINTS[b] < len(final):
                bt += final[JOINTS[b]] * wt
        if np.sum(wv) < 0.001:
            bt = np.eye(4)
        p = cam_from_rig((bt @ np.append(v, 1.0))[:3])
        mn = np.minimum(mn, p)
        mx = np.maximum(mx, p)
    return mn, mx


mesh = js["meshes"][0]
prim = mesh["primitives"][0]
verts = accessor(prim["attributes"]["POSITION"])
j0 = accessor(prim["attributes"]["JOINTS_0"]).astype(int)
w0 = accessor(prim["attributes"]["WEIGHTS_0"]).astype(float)

idle = next(a for a in js["animations"] if a.get("name") == "finger_gun_idle")
fire = next(a for a in js["animations"] if a.get("name") == "finger_gun_fire")

offs = {
    "upper_arm.L": rot_x(60.0),
    "forearm.L": rot_x(70.0),
}
for aname, anim, t in [("idle", idle, 0.0), ("fire", fire, 0.0)]:
    G, final = runtime_world(anim, t, offs)
    hL, hR = G[idx_of["hand.L"]][:3, 3], G[idx_of["hand.R"]][:3, 3]
    eL = G[idx_of["forearm.L"]][:3, 3]
    print(f"[{aname}] main.G {np.round(hL,3)} cam {np.round(cam_from_rig(hL),3)} | main.D {np.round(hR,3)} cam {np.round(cam_from_rig(hR),3)} | coude.G {np.round(eL,3)}")
    mn, mx = skinned_aabb(final, verts, j0, w0)
    print(f"         AABB cam: min=({mn[0]:.3f},{mn[1]:.3f},{mn[2]:.3f}) max=({mx[0]:.3f},{mx[1]:.3f},{mx[2]:.3f})")

# sans offsets (pour memoire)
G0, final0 = runtime_world(idle, 0.0, None)
mn0, mx0 = skinned_aabb(final0, verts, j0, w0)
print("sans offsets   : AABB cam min=({:.3f},{:.3f},{:.3f}) max=({:.3f},{:.3f},{:.3f})".format(*mn0, *mx0))

# REPRO de l'ANCIEN runtime (rest + offsets ±60/55, SANS propagation) -> compare au log du jeu
rest = next(a for a in js["animations"] if a.get("name") == "rest")
old_offs = {
    "upper_arm.R": rot_x(60.0), "forearm.R": rot_x(55.0),
    "upper_arm.L": rot_x(60.0), "forearm.L": rot_x(55.0),
}
G1, final1 = runtime_world(rest, 0.0, old_offs, propagate=False)
mn1, mx1 = skinned_aabb(final1, verts, j0, w0)
print("ANCIEN runtime (rest+offsets, sans propagation):")
print("  AABB cam: min=({:.3f},{:.3f},{:.3f}) max=({:.3f},{:.3f},{:.3f})".format(*mn1, *mx1))
print("  log du jeu : min=(-0.054,-0.127,-0.697) max=(0.054,0.296,-0.553)")
