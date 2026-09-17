#!/usr/bin/env python3
"""Retarget Bandai Namco BVH (22 joints) onto the Arena 19-bone skeleton.

World positions are recovered with CHANNELS order ZXY applied as Rz*Rx*Ry.
Spine and toes are folded away; limb angles come from the same sagittal
two-bone IK Arena already uses, then clamped with limitOf().

Hip height is scaled to K_HIP_Y, but IK still solves from a *fixed* hip at
K_HIP_Y. That throws away source hip drop (crouch/land). Do not copy the
resulting joint angles onto SD and call it a squat — plant the feet, sink
Root.y, then IK. Jump height is a fraction of SD body height, not human
metres. Short limbs (arms, aerial tuck) also need SD-readable exaggeration;
human-mapped arm swing looks like a walk. Kick/punch arms must not copy
human balance pumps; drive them from the active limb. See
docs/Gundam-Arena-技术文档.md §6.5.
"""
from __future__ import annotations

import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RAW = Path(__file__).resolve().parent / "raw"
CLIP_H = ROOT / "main/apps/app_gundam_arena/model/arena_kick_clip.h"
WALK_H = Path(__file__).resolve().parent / "arena_walk_clip.h"

K_HIP_Y, K_HIP_X = 1.02, 0.285
K_THIGH, K_SHIN = 0.33, 0.44
K_SHOULDER_Y = 1.90 - 0.14
K_UPPER_ARM, K_FOREARM = 0.405, 0.35
K_SOLE_Y = 0.025
K_PI = 3.14159265359

# BoneId order excluding Root.
BONES = [
    "Pelvis", "Chest", "Neck", "Head",
    "LShoulder", "LUpperArm", "LForearm", "LHand",
    "RShoulder", "RUpperArm", "RForearm", "RHand",
    "LThigh", "LShin", "LFoot",
    "RThigh", "RShin", "RFoot",
]
LIMITS = {
    "Head": (-.20, .20, -.26, .44, -.87, .87),
    "Neck": (-.10, .10, -.12, .18, -.35, .35),
    "Chest": (-.12, .12, -.15, .20, -.40, .40),
    "Pelvis": (-.10, .10, -.20, .25, -K_PI, K_PI),
    "LShoulder": (-.55, .55, -1.10, .55, -.70, .70),
    "RShoulder": (-.55, .55, -1.10, .55, -.70, .70),
    "LUpperArm": (-.35, .35, -1.40, .80, -.80, .80),
    "RUpperArm": (-.35, .35, -1.40, .80, -.80, .80),
    "LForearm": (-.20, .20, -2.20, .05, -.40, .40),
    "RForearm": (-.20, .20, -2.20, .05, -.40, .40),
    "LHand": (-.40, .40, -.70, .70, -.60, .60),
    "RHand": (-.40, .40, -.70, .70, -.60, .60),
    "LThigh": (-.35, .35, -.80, .90, -.40, .40),
    "RThigh": (-.35, .35, -.80, .90, -.40, .40),
    "LShin": (-.08, .08, 0.0, 1.80, -.12, .12),
    "RShin": (-.08, .08, 0.0, 1.80, -.12, .12),
    "LFoot": (-.15, .15, -.45, .35, -.35, .35),
    "RFoot": (-.15, .15, -.45, .35, -.35, .35),
}

KICK_SOURCE = "dataset-1_kick_normal_001.bvh"
# Strike only: BVH first kick's forward swing. Recovery 64+ is a back-swing
# and is dropped. Chamber is authored, then blended into this window.
KICK_STRIKE_START, KICK_STRIKE_END = 48, 62
KICK_CHAMBER_IN, KICK_CHAMBER_HOLD, KICK_BLEND, KICK_SETTLE = 10, 5, 6, 8
# Arena ball sits at -X (screen-right foot = LThigh). Swap limbs after IK so
# that kick lands on the same foot the ball is in front of.
LR_PAIRS = (
    ("LShoulder", "RShoulder"),
    ("LUpperArm", "RUpperArm"),
    ("LForearm", "RForearm"),
    ("LHand", "RHand"),
    ("LThigh", "RThigh"),
    ("LShin", "RShin"),
    ("LFoot", "RFoot"),
)
WALK_SOURCE = "dataset-1_walk_normal_001.bvh"
WALK_START, WALK_END = 0, 59  # 2s in-place comparison clip; not used at runtime
EXPECTED_JOINTS = [
    "joint_Root", "Hips", "Spine", "Chest", "Neck", "Head",
    "Shoulder_L", "UpperArm_L", "LowerArm_L", "Hand_L",
    "Shoulder_R", "UpperArm_R", "LowerArm_R", "Hand_R",
    "UpperLeg_L", "LowerLeg_L", "Foot_L", "Toes_L",
    "UpperLeg_R", "LowerLeg_R", "Foot_R", "Toes_R",
]


class Joint:
    def __init__(self, name: str):
        self.name = name
        self.offset = (0.0, 0.0, 0.0)
        self.channels: list[str] = []
        self.children: list[Joint] = []


def parse_bvh(path: Path) -> tuple[Joint, list[list[float]], float]:
    tokens = path.read_text().replace("\t", " ").split()
    i = 0

    def tok() -> str:
        nonlocal i
        i += 1
        return tokens[i - 1]

    assert tok() == "HIERARCHY"
    root = None
    stack: list[Joint] = []
    ordered: list[Joint] = []
    while i < len(tokens):
        t = tokens[i]
        if t in ("ROOT", "JOINT"):
            i += 1
            joint = Joint(tok())
            if stack:
                stack[-1].children.append(joint)
            else:
                root = joint
            assert tok() == "{"
            stack.append(joint)
            continue
        if t == "OFFSET":
            i += 1
            stack[-1].offset = (float(tok()), float(tok()), float(tok()))
            continue
        if t == "CHANNELS":
            i += 1
            count = int(tok())
            stack[-1].channels = [tok() for _ in range(count)]
            ordered.append(stack[-1])
            continue
        if t == "End":
            i += 1
            assert tok() == "Site" and tok() == "{" and tok() == "OFFSET"
            _ = (float(tok()), float(tok()), float(tok()))
            assert tok() == "}"
            continue
        if t == "}":
            i += 1
            stack.pop()
            if not stack:
                break
            continue
        raise SystemExit(f"unexpected BVH token {t}")
    assert tok() == "MOTION" and tok() == "Frames:"
    nframes = int(tok())
    assert tok() == "Frame" and tok() == "Time:"
    dt = float(tok())
    nch = sum(len(j.channels) for j in ordered)
    frames = [[float(tok()) for _ in range(nch)] for _ in range(nframes)]
    assert root is not None
    return root, frames, dt


def mat_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [[sum(a[r][k] * b[k][c] for k in range(3)) for c in range(3)] for r in range(3)]


def rx(a: float) -> list[list[float]]:
    c, s = math.cos(a), math.sin(a)
    return [[1, 0, 0], [0, c, -s], [0, s, c]]


def ry(a: float) -> list[list[float]]:
    c, s = math.cos(a), math.sin(a)
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


def rz(a: float) -> list[list[float]]:
    c, s = math.cos(a), math.sin(a)
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]


I = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]


def local_rotation(channels: list[str], values: list[float]) -> list[list[float]]:
    angles = dict(zip(channels, values))
    rot = I
    for axis in ("Zrotation", "Xrotation", "Yrotation"):
        rad = math.radians(angles.get(axis, 0.0))
        rot = mat_mul(rot, {"Z": rz, "X": rx, "Y": ry}[axis[0]](rad))
    return rot


def fk(root: Joint, frame: list[float]) -> dict[str, tuple[float, float, float]]:
    idx = 0
    world: dict[str, tuple[float, float, float]] = {}

    def rec(joint: Joint, parent_r: list[list[float]], parent_t: tuple[float, float, float]) -> None:
        nonlocal idx
        vals = frame[idx: idx + len(joint.channels)]
        idx += len(joint.channels)
        pos = list(joint.offset)
        for k, name in enumerate(joint.channels):
            if name == "Xposition":
                pos[0] = vals[k]
            elif name == "Yposition":
                pos[1] = vals[k]
            elif name == "Zposition":
                pos[2] = vals[k]
        world_r = mat_mul(parent_r, local_rotation(joint.channels, vals))
        world_t = (
            parent_t[0] + parent_r[0][0] * pos[0] + parent_r[0][1] * pos[1] + parent_r[0][2] * pos[2],
            parent_t[1] + parent_r[1][0] * pos[0] + parent_r[1][1] * pos[1] + parent_r[1][2] * pos[2],
            parent_t[2] + parent_r[2][0] * pos[0] + parent_r[2][1] * pos[1] + parent_r[2][2] * pos[2],
        )
        world[joint.name] = world_t
        for child in joint.children:
            rec(child, world_r, world_t)

    rec(root, I, (0.0, 0.0, 0.0))
    return world


def clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def two_bone_pitch(hip_x: float, hip_y: float, target_x: float, target_y: float,
                   l1: float, l2: float, pitch_lo: float, pitch_hi: float) -> tuple[float, float]:
    dx = target_x - hip_x
    dy = target_y - hip_y
    dist = clamp(math.hypot(dx, dy), 0.08, l1 + l2 - 0.02)
    direction = math.atan2(dx, -dy)
    cos_a = clamp((l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist), -1.0, 1.0)
    extra = math.acos(cos_a)
    cos_k = clamp((l1 * l1 + l2 * l2 - dist * dist) / (2 * l1 * l2), -1.0, 1.0)
    thigh = clamp(direction - extra, pitch_lo, pitch_hi)
    shin = clamp(K_PI - math.acos(cos_k), 0.0, 1.80)
    return thigh, shin


def clamp_joint(name: str, roll: float, pitch: float, yaw: float) -> tuple[float, float, float, bool]:
    lim = LIMITS[name]
    out = (clamp(roll, lim[0], lim[1]), clamp(pitch, lim[2], lim[3]), clamp(yaw, lim[4], lim[5]))
    sat = out != (roll, pitch, yaw)
    return out[0], out[1], out[2], sat


def rotate_xz(x: float, z: float, yaw: float) -> tuple[float, float]:
    c, s = math.cos(yaw), math.sin(yaw)
    return x * c + z * s, -x * s + z * c


def retarget_frame(world: dict[str, tuple[float, float, float]], yaw: float,
                   scale: float, y_shift: float) -> dict[str, tuple[float, float, float]]:
    hips = world["Hips"]
    hx, hz = rotate_xz(hips[0], hips[2], yaw)

    def local(name: str) -> tuple[float, float, float]:
        p = world[name]
        x, z = rotate_xz(p[0], p[2], yaw)
        return ((x - hx) * scale, p[1] * scale + y_shift, (z - hz) * scale)

    loc = {name: local(name) for name in world}
    pose = {name: (0.0, 0.0, 0.0) for name in BONES}

    chest = loc["Chest"]
    head = loc["Head"]
    neck = loc["Neck"]
    spine_pitch = math.atan2(chest[2], max(chest[1], 1e-4))
    spine_yaw = math.atan2(chest[0], max(chest[1], 1e-4))
    # +spine_pitch is forward lean (Arena pelvis/chest +pitch → +Z). Kick
    # post-process strips the human back-lean; do not negate this sign.
    pose["Pelvis"] = (0.0, 0.35 * spine_pitch, 0.35 * spine_yaw)
    pose["Chest"] = (0.0, 0.65 * spine_pitch, 0.65 * spine_yaw)
    head_pitch = math.atan2(head[2] - neck[2], max(head[1] - neck[1], 1e-4))
    head_yaw = math.atan2(head[0] - neck[0], max(head[1] - neck[1], 1e-4))
    pose["Neck"] = (0.0, 0.4 * head_pitch, 0.4 * head_yaw)
    pose["Head"] = (0.0, 0.6 * head_pitch, 0.6 * head_yaw)

    def leg(side: str, hip_x: float) -> None:
        foot = loc[f"Foot_{side}"]
        # Arena +pitch swings the foot to -Z; BVH +Z is forward, so subtract.
        thigh_p, shin_p = two_bone_pitch(
            hip_x, K_HIP_Y, hip_x - foot[2], foot[1], K_THIGH, K_SHIN, -.80, .90)
        lift = max(0.0, foot[1] - 0.12)
        foot_p = clamp(-(thigh_p + shin_p) - 0.55 * lift, -.45, .35)
        prefix = "L" if side == "L" else "R"
        pose[f"{prefix}Thigh"] = (0.0, thigh_p, 0.0)
        pose[f"{prefix}Shin"] = (0.0, shin_p, 0.0)
        pose[f"{prefix}Foot"] = (0.0, foot_p, 0.0)

    def arm(side: str) -> None:
        hand = loc[f"Hand_{side}"]
        ua, fa = two_bone_pitch(
            0.0, K_SHOULDER_Y, hand[2], hand[1], K_UPPER_ARM, K_FOREARM, -1.40, .80)
        prefix = "L" if side == "L" else "R"
        pose[f"{prefix}UpperArm"] = (0.0, ua, 0.0)
        pose[f"{prefix}Forearm"] = (0.0, -fa, 0.0)

    leg("L", -K_HIP_X)
    leg("R", K_HIP_X)
    arm("L")
    arm("R")
    swapped = dict(pose)
    for left, right in LR_PAIRS:
        swapped[left], swapped[right] = pose[right], pose[left]
    return swapped


def write_clip(path: Path, frames: list[dict[str, tuple[float, float, float]]],
               source: str, start: int, end: int, sat: int, total: int,
               prefix: str, note: str = "") -> None:
    n = len(frames)
    frame_note = note or f"Frames {start}-{end} @ 30fps. License: CC BY-NC 4.0."
    lines = [
        "#pragma once",
        "// Generated by tools/arena_motion/retarget_bandai.py. Do not edit.",
        f"// Source: Bandai-Namco-Research-Motiondataset-1 / {source}",
        f"// {frame_note}",
        "// Limbs L/R swapped so Arena -X (LThigh / ball side) matches the screen-right foot.",
        "// Sagittal target uses -footZ so BVH forward maps to Arena +Z.",
        "#include <cstdint>",
        "namespace gundam_arena {",
        f"inline constexpr int k{prefix}Frames={n};",
        f"inline constexpr float k{prefix}Fps=30.f;",
        f"inline constexpr float k{prefix}Joints[{n * 18 * 3}]={{",
    ]
    for pose in frames:
        vals = []
        for name in BONES:
            r, p, y = pose[name]
            vals.extend((r, p, y))
        lines.append("    " + ", ".join(f"{v:.5f}f" for v in vals) + ",")
    lines[-1] = lines[-1].rstrip(",")
    lines.extend([
        "};",
        f"inline constexpr int k{prefix}ClampHits={sat};",
        f"inline constexpr int k{prefix}ClampTotal={total};",
        "} // namespace gundam_arena",
        "",
    ])
    path.write_text("\n".join(lines))


def joint_names(root: Joint) -> list[str]:
    names: list[str] = []

    def collect(j: Joint) -> None:
        names.append(j.name)
        for child in j.children:
            collect(child)

    collect(root)
    return names


def bake(path: Path, start: int, end: int, align: int | None = None
         ) -> tuple[list[dict[str, tuple[float, float, float]]], int, int, float, float]:
    root, frames, dt = parse_bvh(path)
    names = joint_names(root)
    if names != EXPECTED_JOINTS:
        raise SystemExit(f"unexpected hierarchy {names}")
    world0 = fk(root, frames[start if align is None else align])
    left = world0["UpperLeg_L"]
    right = world0["UpperLeg_R"]
    yaw = math.atan2(left[2] - right[2], left[0] - right[0])
    hips_y = world0["Hips"][1]
    scale = K_HIP_Y / hips_y
    plant = min(world0["Foot_L"][1], world0["Foot_R"][1]) * scale
    y_shift = K_SOLE_Y - plant
    out = []
    sat = 0
    total = 0
    for idx in range(start, end + 1):
        pose = retarget_frame(fk(root, frames[idx]), yaw, scale, y_shift)
        clamped = {}
        for name, (r, p, y) in pose.items():
            cr, cp, cy, hit = clamp_joint(name, r, p, y)
            clamped[name] = (cr, cp, cy)
            total += 1
            if hit:
                sat += 1
        out.append(clamped)
    return out, sat, total, dt, yaw


def rest_pose() -> dict[str, tuple[float, float, float]]:
    return {name: (0.0, 0.0, 0.0) for name in BONES}


def lerp_pose(a: dict[str, tuple[float, float, float]],
              b: dict[str, tuple[float, float, float]], t: float) -> dict[str, tuple[float, float, float]]:
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    out = {}
    for name in BONES:
        ar, ap, ay = a[name]
        br, bp, by = b[name]
        out[name] = (ar + (br - ar) * t, ap + (bp - ap) * t, ay + (by - ay) * t)
    return out


def smoothstep(t: float) -> float:
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    return t * t * (3.0 - 2.0 * t)


def clamp_pose(pose: dict[str, tuple[float, float, float]]) -> dict[str, tuple[float, float, float]]:
    out = {}
    for name, (r, p, y) in pose.items():
        cr, cp, cy, _ = clamp_joint(name, r, p, y)
        out[name] = (cr, cp, cy)
    return out


def sd_kick_torso(pose: dict[str, tuple[float, float, float]]
                  ) -> dict[str, tuple[float, float, float]]:
    """Arena pelvis/chest +pitch is forward lean.

    Bandai's kick counterbalances a forward leg with a rear torso. That is a
    small human angle; on SD it reads as 后仰 (same pit as the jump). Keep the
    torso upright or leaning into the kick, and pull neck/head out of the
    back-lean. See docs/Gundam-Arena-技术文档.md §6.5.
    """
    pr, pp, py = pose["Pelvis"]
    cr, cp, cy = pose["Chest"]
    nr, np, ny = pose["Neck"]
    hr, hp, hy = pose["Head"]
    pp = max(pp, 0.04)
    cp = max(cp, 0.06)
    swing = pose["LThigh"][1]
    if swing < 0.0:
        t = min(1.0, -swing / 0.55)
        pp = max(pp, 0.08 + 0.08 * t)
        cp = max(cp, 0.10 + 0.08 * t)
    np = max(np, -0.04)
    hp = max(hp, -0.06)
    out = dict(pose)
    out["Pelvis"] = (pr, pp, py)
    out["Chest"] = (cr, cp, cy)
    out["Neck"] = (nr, np, ny)
    out["Head"] = (hr, hp, hy)
    return clamp_pose(out)


ARM_BONES = (
    "LShoulder", "RShoulder", "LUpperArm", "RUpperArm",
    "LForearm", "RForearm", "LHand", "RHand",
)


def with_arms(pose: dict[str, tuple[float, float, float]],
              arms: dict[str, tuple[float, float, float]]
              ) -> dict[str, tuple[float, float, float]]:
    out = dict(pose)
    for name in ARM_BONES:
        out[name] = arms[name]
    return out


def sd_kick_arm_keys() -> tuple[dict[str, tuple[float, float, float]],
                                 dict[str, tuple[float, float, float]]]:
    """Authored kick arm keys. Do not copy Bandai hand IK.

    Human kicks pump both arms and fold the elbows to stay balanced. After
    L/R swap the clip still copies that: chamber is quiet, then the kicking
    upper arm swings forward (−0.56) and back (+0.32) while both forearms
    hit −1.80. On SD that reads as extra flailing. Driving arms from thigh
    pitch is also wrong: the chamber→strike blend sends the thigh through 0,
    so the arms collapse to idle and pop back up. Hold one chamber pose and
    one strike pose; blend those two. See docs §6.5.
    """
    idle = rest_pose()
    chamber = {
        **idle,
        "LUpperArm": (0.0, 0.18, 0.0),
        "RUpperArm": (0.0, -0.48, 0.0),
        "LForearm": (0.0, -0.32, 0.0),
        "RForearm": (0.0, -0.50, 0.0),
    }
    strike = {
        **idle,
        "LUpperArm": (0.0, 0.08, 0.0),
        "RUpperArm": (0.0, -0.55, 0.0),
        "LForearm": (0.0, -0.28, 0.0),
        "RForearm": (0.0, -0.42, 0.0),
    }
    return clamp_pose(chamber), clamp_pose(strike)


# Chamber the kicking (left) leg behind the character (thigh +pitch = foot back)
# then Bandai forward swing, then settle without playing the source recovery.
CHAMBER_POSE = clamp_pose({
    **rest_pose(),
    "LThigh": (0.0, 0.62, 0.0),
    "LShin": (0.0, 0.95, 0.0),
    "LFoot": (0.0, -0.22, 0.0),
    "RThigh": (0.0, -0.06, 0.0),
    "RShin": (0.0, 0.20, 0.0),
    "RFoot": (0.0, -0.10, 0.0),
    "Chest": (0.0, 0.06, 0.0),
    "Pelvis": (0.0, 0.04, 0.0),
})


def compose_kick(strike: list[dict[str, tuple[float, float, float]]]
                 ) -> list[dict[str, tuple[float, float, float]]]:
    rest = rest_pose()
    chamber = CHAMBER_POSE
    chamber_arms, strike_arms = sd_kick_arm_keys()
    out: list[dict[str, tuple[float, float, float]]] = []
    for i in range(KICK_CHAMBER_IN):
        t = smoothstep((i + 1) / KICK_CHAMBER_IN)
        out.append(with_arms(lerp_pose(rest, chamber, t), lerp_pose(rest, chamber_arms, t)))
    out.extend(with_arms(chamber, chamber_arms) for _ in range(KICK_CHAMBER_HOLD))
    first = strike[0]
    for i in range(KICK_BLEND):
        t = smoothstep((i + 1) / KICK_BLEND)
        out.append(with_arms(lerp_pose(chamber, first, t), lerp_pose(chamber_arms, strike_arms, t)))
    out.extend(with_arms(pose, strike_arms) for pose in strike)
    last = strike[-1]
    for i in range(KICK_SETTLE):
        t = smoothstep((i + 1) / KICK_SETTLE)
        out.append(with_arms(lerp_pose(last, rest, t), lerp_pose(strike_arms, rest, t)))
    return [sd_kick_torso(pose) for pose in out]


def main() -> None:
    kick_src = RAW / KICK_SOURCE
    walk_src = RAW / WALK_SOURCE
    if not kick_src.exists() or not walk_src.exists():
        raise SystemExit("missing BVH; run fetch_bandai.py")
    strike, sat, total, dt, yaw = bake(kick_src, KICK_STRIKE_START, KICK_STRIKE_END, align=40)
    kick = compose_kick(strike)
    early = max(pose["LThigh"][1] for pose in kick[:KICK_CHAMBER_IN + KICK_CHAMBER_HOLD])
    min_l = min(pose["LThigh"][1] for pose in kick)
    min_r = min(pose["RThigh"][1] for pose in kick)
    late = max(pose["LThigh"][1] for pose in kick[-KICK_SETTLE:])
    if early < 0.40:
        raise SystemExit(f"kick missing chamber; early LThigh={early:.3f}")
    if min_l >= min_r - 0.15 or min_l > -0.50:
        raise SystemExit(
            f"kick should swing LThigh forward (ball side); Lmin={min_l:.3f} Rmin={min_r:.3f}")
    if late > 0.22:
        raise SystemExit(f"kick recovers with a back-swing; late LThigh={late:.3f}")
    min_pel = min(pose["Pelvis"][1] for pose in kick)
    min_chest = min(pose["Chest"][1] for pose in kick)
    if min_pel < -1e-4 or min_chest < -1e-4:
        raise SystemExit(f"kick torso leans back; pel={min_pel:.3f} chest={min_chest:.3f}")
    lua_min = min(pose["LUpperArm"][1] for pose in kick)
    lfa_min = min(pose["LForearm"][1] for pose in kick)
    rfa_min = min(pose["RForearm"][1] for pose in kick)
    if lua_min < -0.02:
        raise SystemExit(f"kicking-side arm pumped forward; lua={lua_min:.3f}")
    if lfa_min < -0.55 or rfa_min < -0.55:
        raise SystemExit(f"kick elbow folded; L={lfa_min:.3f} R={rfa_min:.3f}")
    strike0 = KICK_CHAMBER_IN + KICK_CHAMBER_HOLD + KICK_BLEND
    strike1 = strike0 + len(strike)
    for a, b in zip(kick[strike0:strike1], kick[strike0 + 1:strike1]):
        if abs(a["RUpperArm"][1] - b["RUpperArm"][1]) > 1e-4:
            raise SystemExit("kick arms must hold still through the forward swing")
    for pose in kick:
        if pose["LThigh"][1] < -0.50 and pose["RUpperArm"][1] > -0.30:
            raise SystemExit("support arm should stay back on the forward swing")
    note = (
        f"Chamber+hold+blend then Bandai frames {KICK_STRIKE_START}-{KICK_STRIKE_END} "
        f"@ 30fps, settle to idle. License: CC BY-NC 4.0."
    )
    write_clip(CLIP_H, kick, KICK_SOURCE, KICK_STRIKE_START, KICK_STRIKE_END, sat, total, "Kick", note)
    print(f"wrote {CLIP_H} frames={len(kick)} dt={dt:.4f} yaw={math.degrees(yaw):.1f}deg clamp {sat}/{total} chamber={early:.2f} strike={min_l:.2f}")
    walk, wsat, wtotal, wdt, wyaw = bake(walk_src, WALK_START, WALK_END)
    write_clip(WALK_H, walk, WALK_SOURCE, WALK_START, WALK_END, wsat, wtotal, "Walk")
    print(f"wrote {WALK_H} frames={len(walk)} dt={wdt:.4f} yaw={math.degrees(wyaw):.1f}deg clamp {wsat}/{wtotal}")
    print("runtime walk remains IK; walk clip is comparison-only")


if __name__ == "__main__":
    main()
