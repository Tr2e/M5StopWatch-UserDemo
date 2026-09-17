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
# Bandai 48-62 after IK is a knee whip: shin folds, extends, folds again.
# That reads as 半途回退 then a small second kick. Do not play it. Chamber
# and strike poses are authored; source is only a hierarchy/fps check.
KICK_STRIKE_START, KICK_STRIKE_END = 48, 62
KICK_CHAMBER_IN, KICK_CHAMBER_HOLD, KICK_BLEND = 10, 5, 10
KICK_STRIKE_HOLD, KICK_SETTLE = 6, 8
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
GESTURE_H = ROOT / "main/apps/app_gundam_arena/model/arena_gesture_clips.h"
K_PLANT = 0.26
GESTURE_IN, GESTURE_OUT, GESTURE_WINDOW = 6, 8, 36
# Firmware still holds one clip per HUD slot. Style takes are scored offline;
# only a strictly better, non-rejected take replaces normal.
GESTURE_SLOTS = [
    ("WAVE L", "dataset-2_wave-left-hand_normal_001.bvh",
     ["dataset-2_wave-left-hand_active_001.bvh"]),
    ("WAVE R", "dataset-2_wave-right-hand_normal_001.bvh",
     ["dataset-2_wave-right-hand_active_001.bvh"]),
    ("WAVE 2", "dataset-2_wave-both-hands_normal_001.bvh",
     ["dataset-2_wave-both-hands_youthful_001.bvh"]),
    ("UP L", "dataset-2_raise-up-left-hand_normal_001.bvh", []),
    ("UP R", "dataset-2_raise-up-right-hand_normal_001.bvh", []),
    ("UP 2", "dataset-2_raise-up-both-hands_normal_001.bvh",
     ["dataset-2_raise-up-both-hands_active_001.bvh"]),
]
PREVIEW_LOCO = [
    ("dataset-2_walk_normal_001.bvh", "walk"),
    ("dataset-2_walk_active_001.bvh", "walk-active"),
    ("dataset-2_walk_youthful_001.bvh", "walk-youthful"),
    ("dataset-2_run_normal_001.bvh", "run"),
    ("dataset-2_run_active_001.bvh", "run-active"),
    ("dataset-2_run_youthful_001.bvh", "run-youthful"),
    ("dataset-2_walk-turn-left_normal_001.bvh", "walk-turn-left"),
    ("dataset-2_walk-turn-left_active_001.bvh", "walk-turn-left-active"),
    ("dataset-2_walk-turn-right_normal_001.bvh", "walk-turn-right"),
    ("dataset-2_walk-turn-right_active_001.bvh", "walk-turn-right-active"),
]
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


# Chamber: kicking (left) foot back. Strike: one extension, shin never
# re-folds. Support leg stays close to idle so the kick is a standing snap.
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
STRIKE_POSE = clamp_pose({
    **rest_pose(),
    "LThigh": (0.0, -0.80, 0.0),
    "LShin": (0.0, 0.50, 0.0),
    "LFoot": (0.0, -0.20, 0.0),
    "RThigh": (0.0, -0.08, 0.0),
    "RShin": (0.0, 0.22, 0.0),
    "RFoot": (0.0, -0.10, 0.0),
    "Chest": (0.0, 0.16, 0.0),
    "Pelvis": (0.0, 0.12, 0.0),
})


def compose_kick() -> list[dict[str, tuple[float, float, float]]]:
    rest = rest_pose()
    chamber = CHAMBER_POSE
    strike = STRIKE_POSE
    chamber_arms, strike_arms = sd_kick_arm_keys()
    out: list[dict[str, tuple[float, float, float]]] = []
    for i in range(KICK_CHAMBER_IN):
        t = smoothstep((i + 1) / KICK_CHAMBER_IN)
        out.append(with_arms(lerp_pose(rest, chamber, t), lerp_pose(rest, chamber_arms, t)))
    out.extend(with_arms(chamber, chamber_arms) for _ in range(KICK_CHAMBER_HOLD))
    for i in range(KICK_BLEND):
        t = smoothstep((i + 1) / KICK_BLEND)
        out.append(with_arms(lerp_pose(chamber, strike, t), lerp_pose(chamber_arms, strike_arms, t)))
    out.extend(with_arms(strike, strike_arms) for _ in range(KICK_STRIKE_HOLD))
    for i in range(KICK_SETTLE):
        t = smoothstep((i + 1) / KICK_SETTLE)
        out.append(with_arms(lerp_pose(strike, rest, t), lerp_pose(strike_arms, rest, t)))
    return [sd_kick_torso(pose) for pose in out]


def plant_legs(pose: dict[str, tuple[float, float, float]]
               ) -> dict[str, tuple[float, float, float]]:
    lt, ls = two_bone_pitch(-K_HIP_X, K_HIP_Y, -K_HIP_X, K_PLANT, K_THIGH, K_SHIN, -.80, .90)
    rt, rs = two_bone_pitch(K_HIP_X, K_HIP_Y, K_HIP_X, K_PLANT, K_THIGH, K_SHIN, -.80, .90)
    out = dict(pose)
    out["LThigh"] = (0.0, lt, 0.0)
    out["LShin"] = (0.0, ls, 0.0)
    out["LFoot"] = (0.0, clamp(-(lt + ls), -.45, .35), 0.0)
    out["RThigh"] = (0.0, rt, 0.0)
    out["RShin"] = (0.0, rs, 0.0)
    out["RFoot"] = (0.0, clamp(-(rt + rs), -.45, .35), 0.0)
    return out


def sd_gesture_torso(pose: dict[str, tuple[float, float, float]]
                     ) -> dict[str, tuple[float, float, float]]:
    pr, pp, py = pose["Pelvis"]
    cr, cp, cy = pose["Chest"]
    nr, np, ny = pose["Neck"]
    hr, hp, hy = pose["Head"]
    out = dict(pose)
    out["Pelvis"] = (pr, max(pp, 0.02), py)
    out["Chest"] = (cr, max(cp, 0.04), cy)
    out["Neck"] = (nr, max(np, -0.04), ny)
    out["Head"] = (hr, max(hp, -0.06), hy)
    return clamp_pose(out)


def sd_gesture_arms(pose: dict[str, tuple[float, float, float]], scale: float
                    ) -> dict[str, tuple[float, float, float]]:
    out = dict(pose)
    for name in ("LUpperArm", "RUpperArm"):
        r, p, y = pose[name]
        out[name] = (r, p * scale, y)
    for name in ("LForearm", "RForearm"):
        r, p, y = pose[name]
        out[name] = (r, p * 1.20 if p < 0.0 else p, y)
    for name in ("LShoulder", "RShoulder"):
        r, p, y = pose[name]
        out[name] = (r, p * 1.30, y)
    return clamp_pose(out)


def arm_energy(pose: dict[str, tuple[float, float, float]]) -> float:
    return (
        abs(pose["LUpperArm"][1]) + abs(pose["RUpperArm"][1])
        + 0.5 * (abs(pose["LForearm"][1]) + abs(pose["RForearm"][1]))
        + abs(pose["LShoulder"][1]) + abs(pose["RShoulder"][1])
    )


def pick_window(frames: list[dict[str, tuple[float, float, float]]], width: int
                ) -> tuple[int, int]:
    n = len(frames)
    if n <= width:
        return 0, n - 1
    best_i, best_e = 0, -1.0
    for i in range(0, n - width + 1):
        e = sum(arm_energy(frames[i + k]) for k in range(width))
        if e > best_e:
            best_e = e
            best_i = i
    return best_i, best_i + width - 1


def compose_gesture(raw: list[dict[str, tuple[float, float, float]]]
                    ) -> list[dict[str, tuple[float, float, float]]]:
    start, end = pick_window(raw, GESTURE_WINDOW)
    body = [plant_legs(frame) for frame in raw[start:end + 1]]
    scale = 1.45
    peak = max(arm_energy(sd_gesture_arms(frame, scale)) for frame in body)
    if peak < 0.55:
        scale = 1.80
    rest = plant_legs(rest_pose())
    out: list[dict[str, tuple[float, float, float]]] = []
    first = sd_gesture_arms(body[0], scale)
    last = sd_gesture_arms(body[-1], scale)
    for i in range(GESTURE_IN):
        t = smoothstep((i + 1) / GESTURE_IN)
        out.append(sd_gesture_torso(lerp_pose(rest, first, t)))
    for frame in body:
        out.append(sd_gesture_torso(sd_gesture_arms(frame, scale)))
    for i in range(GESTURE_OUT):
        t = smoothstep((i + 1) / GESTURE_OUT)
        out.append(sd_gesture_torso(lerp_pose(last, rest, t)))
    return out


def at_limit(name: str, axis: int, pose: dict[str, tuple[float, float, float]]) -> bool:
    lim = LIMITS[name]
    v = pose[name][axis]
    lo, hi = lim[axis * 2], lim[axis * 2 + 1]
    return abs(v - lo) < 0.02 or abs(v - hi) < 0.02


def score_clip(clip: list[dict[str, tuple[float, float, float]]], sat: int, total: int
               ) -> dict[str, float | int | bool]:
    peak = max(arm_energy(pose) for pose in clip)
    min_pel = min(pose["Pelvis"][1] for pose in clip)
    min_chest = min(pose["Chest"][1] for pose in clip)
    n = len(clip)
    sh_lim = sum(
        1 for pose in clip
        if at_limit("LShoulder", 1, pose) or at_limit("RShoulder", 1, pose))
    ua_lim = sum(
        1 for pose in clip
        if at_limit("LUpperArm", 1, pose) or at_limit("RUpperArm", 1, pose))
    both_up = sum(
        1 for pose in clip
        if at_limit("LUpperArm", 1, pose) and at_limit("RUpperArm", 1, pose))
    head_sh = sum(
        1 for pose in clip
        if at_limit("Head", 1, pose) and (
            at_limit("LShoulder", 1, pose) or at_limit("RShoulder", 1, pose)))
    return {
        "peak": peak,
        "clamp": (sat / total) if total else 0.0,
        "sat": sat,
        "total": total,
        "back": min_pel < -1e-4 or min_chest < -1e-4,
        "shoulder_lim": sh_lim / n,
        "upper_lim": ua_lim / n,
        "both_up": both_up / n,
        "head_sh": head_sh / n,
        "flash": n * 18 * 3 * 4,
        "frames": n,
    }


def reject_reason(hud: str, score: dict[str, float | int | bool]) -> str | None:
    if score["back"]:
        return "torso back-lean"
    if float(score["shoulder_lim"]) > 0.20:
        return "over-shoulder limit"
    if float(score["head_sh"]) > 0.10:
        return "head/shoulder collide proxy"
    if hud in ("WAVE 2", "UP 2") and float(score["both_up"]) > 0.25:
        return "hands collapse together"
    return None


def style_beats(hud: str, style: dict[str, float | int | bool],
                normal: dict[str, float | int | bool]) -> tuple[bool, str]:
    reason = reject_reason(hud, style)
    if reason:
        return False, f"reject: {reason}"
    if float(style["peak"]) < float(normal["peak"]) + 0.08:
        return False, "not more readable"
    if float(style["clamp"]) > float(normal["clamp"]) + 0.03:
        return False, "more clamp"
    if float(style["shoulder_lim"]) > float(normal["shoulder_lim"]) + 0.05:
        return False, "more shoulder saturation"
    return True, "higher peak, limits ok"


def load_gesture(source: str) -> tuple[list[dict[str, tuple[float, float, float]]], int, int, float]:
    src = RAW / source
    if not src.exists():
        raise SystemExit(f"missing {source}; run fetch_bandai2.py")
    _root, frames, dt = parse_bvh(src)
    raw, sat, total, _, _ = bake(src, 0, len(frames) - 1)
    clip = compose_gesture(raw)
    plant_lt, plant_ls = two_bone_pitch(
        -K_HIP_X, K_HIP_Y, -K_HIP_X, K_PLANT, K_THIGH, K_SHIN, -.80, .90)
    mid = clip[len(clip) // 2]
    if abs(mid["LThigh"][1] - plant_lt) > 0.04 or abs(mid["LShin"][1] - plant_ls) > 0.04:
        raise SystemExit(f"{source} feet not planted")
    peak = max(arm_energy(pose) for pose in clip)
    if peak < 0.35:
        raise SystemExit(f"{source} arm motion too small; peak={peak:.3f}")
    min_pel = min(pose["Pelvis"][1] for pose in clip)
    min_chest = min(pose["Chest"][1] for pose in clip)
    if min_pel < -1e-4 or min_chest < -1e-4:
        raise SystemExit(f"{source} torso leans back; pel={min_pel:.3f} chest={min_chest:.3f}")
    return clip, sat, total, dt


def fmt_score(score: dict[str, float | int | bool]) -> str:
    return (
        f"frames={score['frames']} flash={score['flash']}B peak={float(score['peak']):.2f} "
        f"clamp={int(score['sat'])}/{int(score['total'])} "
        f"sh_lim={float(score['shoulder_lim']):.2f} ua_lim={float(score['upper_lim']):.2f} "
        f"both_up={float(score['both_up']):.2f} head_sh={float(score['head_sh']):.2f}"
    )


def write_gesture_bank(path: Path, clips: list[tuple[str, str, list[dict[str, tuple[float, float, float]]], int, int]]
                       ) -> None:
    offsets = []
    packed: list[str] = []
    cursor = 0
    for _hud, _src, frames, _sat, _total in clips:
        offsets.append(cursor)
        for pose in frames:
            vals = []
            for name in BONES:
                r, p, y = pose[name]
                vals.extend((r, p, y))
            packed.append("    " + ", ".join(f"{v:.5f}f" for v in vals) + ",")
            cursor += 18 * 3
    if packed:
        packed[-1] = packed[-1].rstrip(",")
    lines = [
        "#pragma once",
        "// Generated by tools/arena_motion/retarget_bandai.py. Do not edit.",
        "// Bandai-Namco-Research-Motiondataset-2 P5 winners, feet planted. CC BY-NC 4.0.",
        "// Limbs L/R swapped to the same Arena -X convention as the kick clip.",
        "#include <cstdint>",
        "namespace gundam_arena {",
        f"inline constexpr int kGestureCount={len(clips)};",
        "inline constexpr float kGestureFps=30.f;",
        "inline constexpr int kGestureFrames[" + str(len(clips)) + "]={"
        + ", ".join(str(len(c[2])) for c in clips) + "};",
        "inline constexpr int kGestureOffset[" + str(len(clips)) + "]={"
        + ", ".join(str(o) for o in offsets) + "};",
        "inline constexpr int kGestureFlashBytes[" + str(len(clips)) + "]={"
        + ", ".join(str(len(c[2]) * 18 * 3 * 4) for c in clips) + "};",
        "inline constexpr const char* kGestureHud[" + str(len(clips)) + "]={"
        + ", ".join(f'"{c[0]}"' for c in clips) + "};",
        "inline constexpr const char* kGestureSource[" + str(len(clips)) + "]={"
        + ", ".join(f'"{c[1]}"' for c in clips) + "};",
        f"inline constexpr float kGestureJoints[{cursor}]={{",
    ]
    lines.extend(packed)
    lines.extend([
        "};",
        "} // namespace gundam_arena",
        "",
    ])
    path.write_text("\n".join(lines))


def main() -> None:
    kick_src = RAW / KICK_SOURCE
    walk_src = RAW / WALK_SOURCE
    if not kick_src.exists() or not walk_src.exists():
        raise SystemExit("missing BVH; run fetch_bandai.py")
    _, _, dt = parse_bvh(kick_src)
    _ref, _sat, _total, _dt, yaw = bake(kick_src, 56, 56, align=40)
    kick = compose_kick()
    sat = 0
    total = len(kick) * 18
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
    swing0 = KICK_CHAMBER_IN + KICK_CHAMBER_HOLD
    swing1 = swing0 + KICK_BLEND + KICK_STRIKE_HOLD
    for a, b in zip(kick[swing0:swing1], kick[swing0 + 1:swing1]):
        if b["LShin"][1] > a["LShin"][1] + 0.04:
            raise SystemExit(
                f"kick shin folded mid-swing; {a['LShin'][1]:.3f} -> {b['LShin'][1]:.3f}")
        if b["LThigh"][1] > a["LThigh"][1] + 0.04:
            raise SystemExit(
                f"kick thigh retracted mid-swing; {a['LThigh'][1]:.3f} -> {b['LThigh'][1]:.3f}")
    hold0 = swing0 + KICK_BLEND
    for a, b in zip(kick[hold0:swing1], kick[hold0 + 1:swing1]):
        if abs(a["RUpperArm"][1] - b["RUpperArm"][1]) > 1e-4:
            raise SystemExit("kick arms must hold still through the strike")
    for pose in kick:
        if pose["LThigh"][1] < -0.50 and pose["RUpperArm"][1] > -0.30:
            raise SystemExit("support arm should stay back on the forward swing")
    note = (
        "Authored chamber then one extension to idle. Bandai 48-62 knee-whip dropped. "
        "License: CC BY-NC 4.0."
    )
    write_clip(CLIP_H, kick, KICK_SOURCE, KICK_STRIKE_START, KICK_STRIKE_END, sat, total, "Kick", note)
    print(f"wrote {CLIP_H} frames={len(kick)} dt={dt:.4f} yaw={math.degrees(yaw):.1f}deg clamp {sat}/{total} chamber={early:.2f} strike={min_l:.2f}")
    walk, wsat, wtotal, wdt, wyaw = bake(walk_src, WALK_START, WALK_END)
    write_clip(WALK_H, walk, WALK_SOURCE, WALK_START, WALK_END, wsat, wtotal, "Walk")
    print(f"wrote {WALK_H} frames={len(walk)} dt={wdt:.4f} yaw={math.degrees(wyaw):.1f}deg clamp {wsat}/{wtotal}")
    print("runtime walk remains IK; walk clip is comparison-only")

    baked: list[tuple[str, str, list[dict[str, tuple[float, float, float]]], int, int]] = []
    style_lines = [
        "Bandai 2 P5 style compare (firmware keeps one take per slot)",
        "Clip flash bytes are frames*18*3*4. Host raster us comes from gundam_arena_test.",
        "Device FPS is not measured in this script.",
        "",
    ]
    for hud, normal_src, alts in GESTURE_SLOTS:
        clip, sat, total, dt = load_gesture(normal_src)
        best_src, best_clip, best_sat, best_total = normal_src, clip, sat, total
        normal_score = score_clip(clip, sat, total)
        style_lines.append(f"{hud} normal {normal_src} {fmt_score(normal_score)}")
        print(f"gesture {hud} {normal_src} {fmt_score(normal_score)} dt={dt:.4f}")
        chosen = "normal"
        note = "keep normal"
        for alt in alts:
            alt_clip, alt_sat, alt_total, alt_dt = load_gesture(alt)
            alt_score = score_clip(alt_clip, alt_sat, alt_total)
            win, why = style_beats(hud, alt_score, normal_score)
            style_lines.append(f"  vs {alt} {fmt_score(alt_score)} -> {why}")
            print(f"  vs {alt} {fmt_score(alt_score)} -> {why} dt={alt_dt:.4f}")
            if win:
                best_src, best_clip, best_sat, best_total = alt, alt_clip, alt_sat, alt_total
                chosen = alt
                note = why
        style_lines.append(f"  keep {chosen} ({note})")
        style_lines.append("")
        baked.append((hud, best_src, best_clip, best_sat, best_total))
    write_gesture_bank(GESTURE_H, baked)
    print(f"wrote {GESTURE_H}")

    report = Path(__file__).resolve().parent / "dataset2_preview.txt"
    lines = ["Bandai 2 locomotion preview (not in firmware)", ""]
    for source, label in PREVIEW_LOCO:
        src = RAW / source
        if not src.exists():
            lines.append(f"{label}: missing {source}")
            continue
        root, frames, dt = parse_bvh(src)
        raw, sat, total, _, yaw = bake(src, 0, min(59, len(frames) - 1))
        lines.append(
            f"{label}: {source} src_frames={len(frames)} preview={len(raw)} "
            f"fps={1.0 / dt:.1f} clamp={sat}/{total} yaw={math.degrees(yaw):.1f}deg")
    report.write_text("\n".join(lines) + "\n")
    print(f"wrote {report}")

    style_report = Path(__file__).resolve().parent / "dataset2_style_report.txt"
    style_report.write_text("\n".join(style_lines))
    print(f"wrote {style_report}")


if __name__ == "__main__":
    main()
