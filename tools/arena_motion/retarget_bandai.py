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
DANCE_H = ROOT / "main/apps/app_gundam_arena/model/arena_dance_clips.h"
DANCE_SCALE = 10000
K_PLANT = 0.26
GESTURE_IN, GESTURE_OUT, GESTURE_WINDOW = 6, 8, 36
# Chest/hips world-Y is ~1.3; bow then reads as ~3deg. Use hip-relative
# direction and SD-readable gain so a real bow hits Chest +0.20.
TORSO_PITCH_GAIN = 1.8
DANCE_DIP_GAIN = 1.35
DANCE_DIP_MAX = 0.18
DANCE_HOP_GAIN = 2.8
DANCE_HOP_MAX = 0.40
DANCE_AIR_FOOT = 0.03
DANCE_STEP_LIFT = 2.2
DANCE_STEP_LIFT_MAX = 0.32
DANCE_YAW_AMP = 0.42
DANCE_YAW_RAD = 18
DANCE_STAND_RAD = 45
DANCE_RAISE_KEEP = 0.88
DANCE_LOOP_PREROLL = 20
DANCE_LOOP_MIN_S = 8.0
PUNCH_CHAMBER_IN, PUNCH_CHAMBER_HOLD, PUNCH_BLEND = 6, 8, 12
PUNCH_STRIKE_HOLD, PUNCH_SETTLE = 8, 16
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
REVIEW_GESTURE_SLOTS = [
    ("BOW", "dataset-1_bow_normal_001.bvh", []),
    ("BYE", "dataset-1_bye_normal_001.bvh", []),
    ("BYE2", "dataset-1_byebye_normal_001.bvh", []),
    ("GUIDE", "dataset-1_guide_normal_001.bvh", []),
    ("PUNCH", "dataset-1_punch_normal_001.bvh", []),
    ("RUN", "dataset-1_run_normal_001.bvh", []),
    ("DASH", "dataset-1_dash_normal_001.bvh", []),
]
DANCE_SLOTS = [
    ("DNC L", "dataset-1_dance-long_normal_001.bvh"),
    ("DNC S", "dataset-1_dance-short_normal_001.bvh"),
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

    hips_y = loc["Hips"][1]
    chest = loc["Chest"]
    head = loc["Head"]
    neck = loc["Neck"]
    rel_y = max(chest[1] - hips_y, 1e-4)
    # Hip-relative, then SD gain. atan2(worldZ, worldY) is ~3deg for a bow
    # because Y is ground height (~1.3), not chest-from-hip. See §6.5.
    spine_pitch = math.atan2(chest[2], rel_y) * TORSO_PITCH_GAIN
    spine_yaw = math.atan2(chest[0], rel_y) * TORSO_PITCH_GAIN
    pose["Pelvis"] = (0.0, 0.35 * spine_pitch, 0.35 * spine_yaw)
    pose["Chest"] = (0.0, 0.65 * spine_pitch, 0.65 * spine_yaw)
    head_pitch = math.atan2(head[2] - neck[2], max(head[1] - neck[1], 1e-4)) * TORSO_PITCH_GAIN
    head_yaw = math.atan2(head[0] - neck[0], max(head[1] - neck[1], 1e-4)) * TORSO_PITCH_GAIN
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
    dips = []
    sat = 0
    total = 0
    for idx in range(start, end + 1):
        world = fk(root, frames[idx])
        dips.append(max(0.0, (hips_y - world["Hips"][1]) * scale))
        pose = retarget_frame(world, yaw, scale, y_shift)
        clamped = {}
        for name, (r, p, y) in pose.items():
            cr, cp, cy, hit = clamp_joint(name, r, p, y)
            clamped[name] = (cr, cp, cy)
            total += 1
            if hit:
                sat += 1
        out.append(clamped)
    return out, sat, total, dt, yaw, dips


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


def set_leg(pose: dict[str, tuple[float, float, float]], side: str,
            thigh: float, shin: float) -> None:
    pose[f"{side}Thigh"] = (0.0, thigh, 0.0)
    pose[f"{side}Shin"] = (0.0, shin, 0.0)
    pose[f"{side}Foot"] = (0.0, clamp(-(thigh + shin), -.45, .35), 0.0)


def plant_legs(pose: dict[str, tuple[float, float, float]], dip: float = 0.0
               ) -> dict[str, tuple[float, float, float]]:
    dip = max(0.0, dip)
    ankle = K_PLANT + dip
    lt, ls = two_bone_pitch(-K_HIP_X, K_HIP_Y, -K_HIP_X, ankle, K_THIGH, K_SHIN, -.80, .90)
    rt, rs = two_bone_pitch(K_HIP_X, K_HIP_Y, K_HIP_X, ankle, K_THIGH, K_SHIN, -.80, .90)
    out = dict(pose)
    set_leg(out, "L", lt, ls)
    set_leg(out, "R", rt, rs)
    return out


def hop_leg(air: float, bias: float) -> tuple[float, float]:
    air = clamp(air, 0.0, 1.0)
    ankle = K_PLANT
    t0, s0 = two_bone_pitch(0.0, K_HIP_Y, 0.0, ankle, K_THIGH, K_SHIN, -.80, .90)
    t1, s1 = -0.62 + bias, 1.22
    return t0 + (t1 - t0) * air, s0 + (s1 - s0) * air


def step_leg(hip_x: float, foot_z: float, lift: float, dip: float) -> tuple[float, float]:
    sag = clamp(foot_z, -0.36, 0.36)
    ankle = K_PLANT + max(0.0, dip) + clamp(lift * DANCE_STEP_LIFT, 0.0, DANCE_STEP_LIFT_MAX)
    return two_bone_pitch(hip_x, K_HIP_Y, hip_x + sag, ankle, K_THIGH, K_SHIN, -.80, .90)


def sd_gesture_torso(pose: dict[str, tuple[float, float, float]]
                     ) -> dict[str, tuple[float, float, float]]:
    # Kick floors pelvis/chest at +0.02/+0.04 so a tiny back-lean reads as
    # a frozen upright. Gestures only strip true back-lean (pitch < 0).
    pr, pp, py = pose["Pelvis"]
    cr, cp, cy = pose["Chest"]
    out = dict(pose)
    out["Pelvis"] = (pr, max(pp, 0.0), py)
    out["Chest"] = (cr, max(cp, 0.0), cy)
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


def arena_side(src_side: str) -> str:
    # bake() swaps L/R onto the kick convention. Authored keys follow that.
    return "R" if src_side == "L" else "L"


def outward_yaw(side: str, base: float, wave: float = 0.0) -> float:
    # SD helmet is ~±0.90. Overhead keys at yaw=0 sit in the visor.
    signed = -base if side == "L" else base
    return clamp(signed + wave, -0.80, 0.80)


def set_arm(pose: dict[str, tuple[float, float, float]], side: str,
            shoulder: tuple[float, float, float],
            upper: tuple[float, float, float],
            forearm: tuple[float, float, float]) -> None:
    pose[f"{side}Shoulder"] = shoulder
    pose[f"{side}UpperArm"] = upper
    pose[f"{side}Forearm"] = forearm


# SD-readable silhouettes. +upper-arm pitch swings the hand to -Z (jump extend);
# overhead / character-forward use negative shoulder+upper pitch. See §6.5.
KEY_RAISE_SH = (0.0, -1.10, 0.0)
KEY_RAISE_UA = (0.0, -1.20, 0.0)
KEY_RAISE_FA = (0.0, -0.30, 0.0)
KEY_CHEST_SH = (0.0, -0.20, 0.0)
KEY_CHEST_UA = (0.0, -0.45, 0.0)
KEY_CHEST_FA = (0.0, -0.70, 0.0)
KEY_BYE_SH = (0.0, -0.72, 0.0)
KEY_BYE_UA = (0.0, -0.88, 0.0)
KEY_BYE_FA = (0.0, -0.62, 0.0)
KEY_HEADSIDE_SH = (0.0, -0.85, 0.0)
KEY_HEADSIDE_UA = (0.0, -0.85, 0.0)
KEY_HEADSIDE_FA = (0.0, -0.55, 0.0)
KEY_POINT_SH = (0.0, -0.80, 0.0)
KEY_POINT_UA = (0.0, -0.60, 0.0)
KEY_POINT_FA = (0.0, -0.20, 0.0)
KEY_TEMPLE_SH = (0.0, -0.60, 0.0)
KEY_TEMPLE_UA = (0.0, -0.75, 0.0)
KEY_TEMPLE_FA = (0.0, -1.55, 0.0)
KEY_ANS_SH = (0.0, -0.70, 0.0)
KEY_ANS_UA = (0.0, -1.00, 0.0)
KEY_ANS_FA = (0.0, -0.25, 0.0)
KEY_GUARD_SH = (0.0, -0.15, 0.0)
KEY_GUARD_UA = (0.0, -0.35, 0.0)
KEY_GUARD_FA = (0.0, -1.20, 0.0)
KEY_JAB_SH = (0.0, -0.80, 0.0)
KEY_JAB_UA = (0.0, -0.60, 0.0)
KEY_JAB_FA = (0.0, -0.20, 0.0)
KEY_CHOP_HI_SH = (0.0, -1.10, 0.0)
KEY_CHOP_HI_UA = (0.0, -1.15, 0.0)
KEY_CHOP_HI_FA = (0.0, -0.30, 0.0)
KEY_CHOP_LO_SH = (0.0, -0.20, 0.0)
KEY_CHOP_LO_UA = (0.0, -0.30, 0.0)
KEY_CHOP_LO_FA = (0.0, -0.25, 0.0)


def planted() -> dict[str, tuple[float, float, float]]:
    return plant_legs(rest_pose(), 0.0)


def envelope(body: list[dict[str, tuple[float, float, float]]],
             dips: list[float] | None = None
             ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    rest = planted()
    d0 = dips[0] if dips else 0.0
    d1 = dips[-1] if dips else 0.0
    dip_body = dips if dips is not None else [0.0] * len(body)
    out: list[dict[str, tuple[float, float, float]]] = []
    dip_out: list[float] = []
    first, last = body[0], body[-1]
    for i in range(GESTURE_IN):
        t = smoothstep((i + 1) / GESTURE_IN)
        out.append(sd_gesture_torso(lerp_pose(rest, first, t)))
        dip_out.append(d0 * t)
    for pose, dip in zip(body, dip_body):
        out.append(sd_gesture_torso(pose))
        dip_out.append(dip)
    for i in range(GESTURE_OUT):
        t = smoothstep((i + 1) / GESTURE_OUT)
        out.append(sd_gesture_torso(lerp_pose(last, rest, t)))
        dip_out.append(d1 * (1.0 - t))
    return out, dip_out


def pick_track_window(n: int, width: int, score_at) -> tuple[int, int]:
    if n <= width:
        return 0, n - 1
    best_i, best_e = 0, -1.0
    for i in range(0, n - width + 1):
        energy = score_at(i)
        if energy > best_e:
            best_e = energy
            best_i = i
    return best_i, best_i + width - 1


def x_travel(track: list[dict], start: int, width: int, side: str) -> float:
    xs = [track[start + k][side][0] for k in range(width)]
    return max(xs) - min(xs)


def yaw_from_xs(xs: list[float], amp: float, min_span: float = 0.0) -> list[float]:
    mu = sum(xs) / max(1, len(xs))
    d = [x - mu for x in xs]
    span = max((abs(v) for v in d), default=0.0)
    floor = max(span, min_span) if min_span > 0.0 else span
    gain = amp / floor if floor > 1e-4 else 0.0
    return [clamp(v * gain, -amp, amp) for v in d]


def rolling_high(xs: list[float], rad: int, q: float = 0.85) -> list[float]:
    n = len(xs)
    out = [0.0] * n
    for i in range(n):
        w = xs[max(0, i - rad):min(n, i + rad + 1)]
        s = sorted(w)
        out[i] = s[int((len(s) - 1) * q)]
    return out


def rolling_yaw(track: list[dict], side: str, amp: float, rad: int) -> list[float]:
    xs = [p[side][0] for p in track]
    n = len(xs)
    out = [0.0] * n
    for i in range(n):
        lo, hi = max(0, i - rad), min(n, i + rad + 1)
        chunk = yaw_from_xs(xs[lo:hi], amp, min_span=0.06)
        out[i] = chunk[i - lo]
    return out


def dance_pose_delta(a: dict[str, tuple[float, float, float]],
                     b: dict[str, tuple[float, float, float]]) -> float:
    e = 0.0
    for name in ("LUpperArm", "RUpperArm", "LThigh", "RThigh", "LShoulder", "RShoulder"):
        e += abs(a[name][1] - b[name][1]) + abs(a[name][2] - b[name][2])
    return e


def dance_loop_range(clip: list[dict[str, tuple[float, float, float]]],
                     dips: list[float], fps: float = 30.0) -> tuple[int, int]:
    # Open/close on hops only. Thigh-span "steps" false-trigger on pegged
    # limit holds (DNC S ~2s / DNC L ~14s dead outro looked frozen, then wrap).
    n = len(clip)
    if n < 2 or len(dips) != n:
        return 0, n

    def hop_at(i: int) -> bool:
        return dips[i] < -0.04

    hops = [i for i in range(n) if hop_at(i)]
    if not hops:
        return 0, n
    start = max(0, hops[0] - DANCE_LOOP_PREROLL)
    end = min(n, hops[-1] + DANCE_LOOP_PREROLL + 1)
    if end - start < int(DANCE_LOOP_MIN_S * fps):
        return 0, n
    return start, end


def active_src_side(track: list[dict], start: int, end: int, key: str = "y") -> str:
    ax = 1 if key == "y" else 2
    l = max(track[i]["L"][ax] for i in range(start, end + 1))
    r = max(track[i]["R"][ax] for i in range(start, end + 1))
    return "L" if l >= r else "R"


def torso_flex(frames: list[dict[str, tuple[float, float, float]]], i: int, width: int
               ) -> float:
    return sum(
        max(0.0, frames[i + k]["Pelvis"][1]) + max(0.0, frames[i + k]["Chest"][1])
        for k in range(width))


def compose_wave(track: list[dict], both: bool, yaw_amp: float, raise_key: bool,
                 hud_side: str | None = None,
                 keys: tuple[tuple[float, float, float], tuple[float, float, float],
                             tuple[float, float, float]] | None = None,
                 outward_base: float | None = None
                 ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    n = len(track)
    if keys is not None:
        sh, ua0, fa = keys
    elif raise_key:
        sh, ua0, fa = KEY_RAISE_SH, KEY_RAISE_UA, KEY_RAISE_FA
    else:
        sh, ua0, fa = KEY_CHEST_SH, KEY_CHEST_UA, KEY_CHEST_FA

    def score(i: int) -> float:
        if both:
            high = 0.0
            for k in range(GESTURE_WINDOW):
                ly, ry, hy = track[i + k]["L"][1], track[i + k]["R"][1], track[i + k]["H"][1]
                if (not raise_key) or max(ly, ry) > hy - 0.02:
                    high += 1.0
            return high * (1.0 + x_travel(track, i, GESTURE_WINDOW, "L")
                           + x_travel(track, i, GESTURE_WINDOW, "R"))
        side = hud_side or active_src_side(track, i, i + GESTURE_WINDOW - 1)
        high = 0.0
        for k in range(GESTURE_WINDOW):
            if (not raise_key) or track[i + k][side][1] > track[i + k]["H"][1] - 0.02:
                high += 1.0
        return high * (1.0 + x_travel(track, i, GESTURE_WINDOW, side))

    start, end = pick_track_window(n, GESTURE_WINDOW, score)
    src_side = hud_side or active_src_side(track, start, end)
    sides = ("L", "R") if both else (src_side,)
    yaws = {
        src: yaw_from_xs([track[j][src][0] for j in range(start, end + 1)], yaw_amp)
        for src in sides
    }
    body = []
    for i in range(start, end + 1):
        pose = planted()
        for src in sides:
            side = arena_side(src)
            wave = yaws[src][i - start]
            base = 0.58 if raise_key else outward_base
            yaw = outward_yaw(side, base, wave) if base is not None else wave
            ua = (ua0[0], ua0[1], yaw)
            set_arm(pose, side, sh, ua, fa)
        body.append(clamp_pose(pose))
    return envelope(body)


def compose_hold(track: list[dict], hud: str
                 ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    n = len(track)
    if hud.startswith("UP"):
        def score(i: int) -> float:
            return sum(max(track[i + k]["L"][1], track[i + k]["R"][1]) for k in range(GESTURE_WINDOW))
        both = hud == "UP 2"
        src_side = "L" if hud == "UP L" else "R" if hud == "UP R" else "L"
        sh, ua, fa = KEY_RAISE_SH, KEY_RAISE_UA, KEY_RAISE_FA
    elif hud == "GUIDE":
        def score(i: int) -> float:
            return sum(
                max(track[i + k]["L"][2], track[i + k]["R"][2]) for k in range(GESTURE_WINDOW))
        both = False
        src_side = "R"
        sh, ua, fa = KEY_POINT_SH, KEY_POINT_UA, KEY_POINT_FA
    else:
        raise SystemExit(f"compose_hold: {hud}")
    start, end = pick_track_window(n, GESTURE_WINDOW, score)
    if hud == "GUIDE":
        src_side = active_src_side(track, start, end, "z")
    elif hud == "UP L":
        src_side = "L"
    elif hud == "UP R":
        src_side = "R"
    body = []
    for _ in range(start, end + 1):
        pose = planted()
        sides = ("L", "R") if both else (src_side,)
        for src in sides:
            this_sh, this_ua, this_fa = sh, ua, fa
            if hud.startswith("UP"):
                side = arena_side(src)
                this_ua = (ua[0], ua[1], outward_yaw(side, 0.55))
            set_arm(pose, arena_side(src), this_sh, this_ua, this_fa)
        body.append(clamp_pose(pose))
    return envelope(body)


def compose_punch_id(track: list[dict]
                     ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    del track  # source only picked the slot; both Arena hands jab.
    rest = planted()
    guard = planted()
    set_arm(guard, "L", KEY_GUARD_SH, KEY_GUARD_UA, KEY_GUARD_FA)
    set_arm(guard, "R", KEY_GUARD_SH, KEY_GUARD_UA, KEY_GUARD_FA)
    jab_l = planted()
    set_arm(jab_l, "L", KEY_JAB_SH, KEY_JAB_UA, KEY_JAB_FA)
    set_arm(jab_l, "R", KEY_GUARD_SH, KEY_GUARD_UA, KEY_GUARD_FA)
    jab_l["Chest"] = (0.0, 0.10, 0.08)
    jab_r = planted()
    set_arm(jab_r, "L", KEY_GUARD_SH, KEY_GUARD_UA, KEY_GUARD_FA)
    set_arm(jab_r, "R", KEY_JAB_SH, KEY_JAB_UA, KEY_JAB_FA)
    jab_r["Chest"] = (0.0, 0.10, -0.08)

    def blend(a, b, n: int, side: str | None = None) -> list:
        rows = []
        prev = sd_gesture_torso(a)
        for i in range(n):
            t = smoothstep((i + 1) / n)
            pose = lerp_pose(a, b, t)
            if side is not None and pose[f"{side}Forearm"][1] < prev[f"{side}Forearm"][1] - 0.04:
                pose[f"{side}Forearm"] = prev[f"{side}Forearm"]
            prev = sd_gesture_torso(pose)
            rows.append(prev)
        return rows

    out: list[dict[str, tuple[float, float, float]]] = []
    out.extend(blend(rest, guard, 4))
    out.extend(sd_gesture_torso(guard) for _ in range(4))
    out.extend(blend(guard, jab_l, 7, "L"))
    out.extend(sd_gesture_torso(jab_l) for _ in range(5))
    out.extend(blend(jab_l, guard, 6))
    out.extend(blend(guard, jab_r, 7, "R"))
    out.extend(sd_gesture_torso(jab_r) for _ in range(5))
    settle = 50 - len(out)
    out.extend(blend(jab_r, rest, max(1, settle)))
    return out[:50], [0.0] * 50


def compose_slash_id(track: list[dict]
                     ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    n = len(track)
    def score(i: int) -> float:
        ys = [max(track[i + k]["L"][1], track[i + k]["R"][1]) for k in range(GESTURE_WINDOW)]
        return max(ys) - min(ys) + 0.25 * max(ys)
    start, end = pick_track_window(n, GESTURE_WINDOW, score)
    src_side = active_src_side(track, start, end, "y")
    side = arena_side(src_side)
    other = "L" if side == "R" else "R"
    out_yaw = 0.72 if side == "R" else -0.72
    across_yaw = -0.42 if side == "R" else 0.42
    wind = -0.28 if side == "R" else 0.28
    follow = 0.38 if side == "R" else -0.38
    high = planted()
    low = planted()
    # One arm high-outside, then across the fat belly. Two-arm V read as jumping jacks.
    set_arm(high, side, KEY_CHOP_HI_SH, (0.0, KEY_CHOP_HI_UA[1], out_yaw), KEY_CHOP_HI_FA)
    set_arm(high, other, KEY_CHEST_SH, (0.0, 0.12, 0.0), KEY_CHEST_FA)
    set_arm(low, side, (0.0, -0.50, 0.0), (0.0, -0.55, across_yaw), (0.0, -0.35, 0.0))
    set_arm(low, other, KEY_CHEST_SH, (0.0, 0.12, 0.0), KEY_CHEST_FA)
    high["Chest"] = (0.0, 0.06, wind)
    low["Chest"] = (0.0, 0.16, follow)
    rest = planted()
    out: list[dict[str, tuple[float, float, float]]] = []
    for i in range(GESTURE_IN):
        t = smoothstep((i + 1) / GESTURE_IN)
        out.append(sd_gesture_torso(lerp_pose(rest, high, t)))
    out.extend(sd_gesture_torso(high) for _ in range(10))
    for i in range(10):
        t = smoothstep((i + 1) / 10)
        out.append(sd_gesture_torso(lerp_pose(high, low, t)))
    out.extend(sd_gesture_torso(low) for _ in range(8))
    settle = 50 - len(out)
    for i in range(settle):
        t = smoothstep((i + 1) / max(1, settle))
        out.append(sd_gesture_torso(lerp_pose(low, rest, t)))
    return out, [0.0] * len(out)


def lerp3(a: tuple[float, float, float], b: tuple[float, float, float], t: float
          ) -> tuple[float, float, float]:
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def compose_dance_id(track: list[dict], drops: list[float]
                     ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    # Full source timeline. Local WAVE yaw so a held raise still waves.
    # Local hip stand so crouch does not pin at DANCE_DIP_MAX. drops unused.
    del drops
    ls, rs = arena_side("L"), arena_side("R")
    hip_y = [p["hips"][1] for p in track] if track else [K_HIP_Y]
    stand = rolling_high(hip_y, DANCE_STAND_RAD)
    yaw_l = rolling_yaw(track, "L", DANCE_YAW_AMP, DANCE_YAW_RAD)
    yaw_r = rolling_yaw(track, "R", DANCE_YAW_AMP, DANCE_YAW_RAD)

    def raise_amt(hand_y: float, head_y: float) -> float:
        lo = head_y - 0.70
        hi = head_y - 0.08
        return clamp((hand_y - lo) / max(0.20, hi - lo), 0.0, 1.0) * DANCE_RAISE_KEEP

    def put_arm(pose: dict[str, tuple[float, float, float]], side: str,
                hand: tuple[float, float, float], head_y: float, wave: float) -> None:
        amt = raise_amt(hand[1], head_y)
        fwd = clamp(hand[2] / 0.48, 0.0, 1.0) * (1.0 - 0.55 * amt)
        sh = lerp3(lerp3(KEY_CHEST_SH, KEY_RAISE_SH, amt), KEY_POINT_SH, fwd)
        ua_p = (KEY_CHEST_UA[1] + (KEY_RAISE_UA[1] - KEY_CHEST_UA[1]) * amt
                + (KEY_POINT_UA[1] - KEY_CHEST_UA[1]) * fwd)
        fa = lerp3(lerp3(KEY_CHEST_FA, KEY_RAISE_FA, amt), KEY_POINT_FA, fwd)
        set_arm(pose, side, sh, (0.0, ua_p, outward_yaw(side, 0.22 + 0.40 * amt, wave)), fa)

    body = []
    body_dips = []
    for i, hands in enumerate(track):
        hips_y = hands["hips"][1]
        signed = stand[i] - hips_y
        sink = DANCE_DIP_MAX * math.tanh(max(0.0, signed) * 8.0)
        src_fl, src_fr = hands["FL"], hands["FR"]
        lift_l = max(0.0, src_fr[1] - K_SOLE_Y)
        lift_r = max(0.0, src_fl[1] - K_SOLE_Y)
        both_air = lift_l > DANCE_AIR_FOOT and lift_r > DANCE_AIR_FOOT
        pose = rest_pose()
        if both_air:
            hop = max(-signed, min(lift_l, lift_r))
            raw = clamp(hop * DANCE_HOP_GAIN / DANCE_HOP_MAX, 0.0, 1.0)
            air = 0.78 + 0.22 * raw
            root = -air * DANCE_HOP_MAX
            lt, ls_ = hop_leg(air, -0.03)
            rt, rs_ = hop_leg(air, 0.02)
            set_leg(pose, "L", lt, ls_)
            set_leg(pose, "R", rt, rs_)
        else:
            root = sink
            lt, ls_ = step_leg(-K_HIP_X, src_fr[2], lift_l, sink)
            rt, rs_ = step_leg(K_HIP_X, src_fl[2], lift_r, sink)
            set_leg(pose, "L", lt, ls_)
            set_leg(pose, "R", rt, rs_)
        put_arm(pose, ls, hands["L"], hands["H"][1], yaw_l[i])
        put_arm(pose, rs, hands["R"], hands["H"][1], yaw_r[i])
        sway = clamp((hands["L"][0] + hands["R"][0]) * 0.45, -0.22, 0.22)
        twist = clamp((hands["L"][1] - hands["R"][1]) * 0.28, -0.16, 0.16)
        air_lean = 0.08 if both_air else 0.0
        pose["Chest"] = (0.0, clamp(0.02 + max(root, 0.0) * 0.50 + air_lean, 0.0, 0.18), sway)
        pose["Pelvis"] = (0.0, clamp(max(root, 0.0) * 0.55, 0.0, 0.16), -0.55 * sway + twist)
        body.append(sd_gesture_torso(pose))
        body_dips.append(root)
    return body, body_dips


def compose_loco_id(kind: str
                    ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    # In-place SD gait. Source only names the slot; human run/dash angles
    # read as a walk on short limbs. Do not plant, do not replace stick IK.
    if kind == "RUN":
        cycles, period = 3, 16
        lean, pel = 0.10, 0.08
        thigh_fwd, thigh_back = -0.42, 0.50
        shin_ext, shin_fold = 0.22, 1.20
        ua_fwd, ua_back = -0.55, 0.30
        fa_fwd, fa_back = -1.15, -0.50
        dip_amp, yaw = 0.07, 0.08
    elif kind == "DASH":
        cycles, period = 4, 10
        lean, pel = 0.18, 0.14
        thigh_fwd, thigh_back = -0.62, 0.72
        shin_ext, shin_fold = 0.16, 1.50
        ua_fwd, ua_back = -0.78, 0.40
        fa_fwd, fa_back = -1.40, -0.40
        dip_amp, yaw = 0.11, 0.12
    else:
        raise SystemExit(f"compose_loco: {kind}")

    def limb(phase: float) -> tuple[float, float, float, float, float]:
        t = 0.5 + 0.5 * math.sin(phase)
        thigh = thigh_fwd * (1.0 - t) + thigh_back * t
        shin = shin_ext * (1.0 - t) + shin_fold * t
        ua = ua_fwd * t + ua_back * (1.0 - t)
        fa = fa_fwd * t + fa_back * (1.0 - t)
        foot = clamp(-(thigh + shin) * 0.28, -0.45, 0.35)
        return thigh, shin, ua, fa, foot

    body = []
    dips = []
    for i in range(cycles * period):
        phase = 2.0 * math.pi * i / period
        lt, ls, lua, lfa, lf = limb(phase)
        rt, rs, rua, rfa, rf = limb(phase + math.pi)
        pose = rest_pose()
        pose["LThigh"] = (0.0, lt, 0.0)
        pose["LShin"] = (0.0, ls, 0.0)
        pose["LFoot"] = (0.0, lf, 0.0)
        pose["RThigh"] = (0.0, rt, 0.0)
        pose["RShin"] = (0.0, rs, 0.0)
        pose["RFoot"] = (0.0, rf, 0.0)
        pose["LUpperArm"] = (0.0, lua, 0.0)
        pose["RUpperArm"] = (0.0, rua, 0.0)
        pose["LForearm"] = (0.0, lfa, 0.0)
        pose["RForearm"] = (0.0, rfa, 0.0)
        pose["LShoulder"] = (0.0, -0.12, 0.0)
        pose["RShoulder"] = (0.0, -0.12, 0.0)
        sway = math.sin(phase) * yaw
        pose["Chest"] = (0.0, lean, sway)
        pose["Pelvis"] = (0.0, pel, -0.6 * sway)
        body.append(clamp_pose(pose))
        dips.append(dip_amp * abs(math.sin(phase * 2.0)))
    return envelope(body, dips)


def compose_bow(raw: list[dict[str, tuple[float, float, float]]]
                ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    def score(i: int) -> float:
        return torso_flex(raw, i, GESTURE_WINDOW)
    start, end = pick_track_window(len(raw), GESTURE_WINDOW, score)
    rest = planted()
    body = []
    for i in range(start, end + 1):
        pose = plant_legs(raw[i], 0.0)
        for name in ("LShoulder", "RShoulder", "LUpperArm", "RUpperArm", "LForearm", "RForearm"):
            pose[name] = (0.0, 0.0, 0.0)
        cr, cp, cy = pose["Chest"]
        pr, pp, py = pose["Pelvis"]
        pose["Chest"] = (cr, min(cp, 0.20), cy)
        pose["Pelvis"] = (pr, min(pp, 0.25), py)
        body.append(clamp_pose(pose))
    return envelope(body)


def sd_dip(drop: float, window_peak: float) -> float:
    if drop <= 0.0:
        return 0.0
    gain = DANCE_DIP_GAIN
    if window_peak > 1e-4:
        gain = max(gain, 0.10 / window_peak)
    return clamp(drop * gain, 0.0, DANCE_DIP_MAX)


def source_track(path: Path, frames: list, yaw: float, scale: float, y_shift: float
                 ) -> list[dict]:
    root, _, _ = parse_bvh(path)
    out = []
    for fr in frames:
        w = fk(root, fr)
        hips = w["Hips"]
        hx, hz = rotate_xz(hips[0], hips[2], yaw)
        def loc(name: str) -> tuple[float, float, float]:
            p = w[name]
            x, z = rotate_xz(p[0], p[2], yaw)
            return ((x - hx) * scale, p[1] * scale + y_shift, (z - hz) * scale)
        out.append({
            "L": loc("Hand_L"), "R": loc("Hand_R"), "H": loc("Head"),
            "FL": loc("Foot_L"), "FR": loc("Foot_R"), "hips": loc("Hips"),
        })
    return out


def compose_gesture(raw: list[dict[str, tuple[float, float, float]]], hud: str,
                    drops: list[float], track: list[dict]
                    ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float]]:
    if hud == "WAVE L":
        return compose_wave(track, both=False, yaw_amp=0.22, raise_key=True, hud_side="L")
    if hud == "WAVE R":
        return compose_wave(track, both=False, yaw_amp=0.22, raise_key=True, hud_side="R")
    if hud == "WAVE 2":
        return compose_wave(track, both=True, yaw_amp=0.20, raise_key=True)
    if hud in ("UP L", "UP R", "UP 2"):
        return compose_hold(track, hud)
    if hud == "BOW":
        return compose_bow(raw)
    if hud == "BYE":
        return compose_wave(
            track, both=False, yaw_amp=0.34, raise_key=False,
            keys=(KEY_BYE_SH, KEY_BYE_UA, KEY_BYE_FA),
            outward_base=0.32)
    if hud == "BYE2":
        return compose_wave(
            track, both=True, yaw_amp=0.18, raise_key=False,
            keys=(KEY_HEADSIDE_SH, KEY_HEADSIDE_UA, KEY_HEADSIDE_FA),
            outward_base=0.50)
    if hud == "GUIDE":
        return compose_hold(track, hud)
    if hud == "PUNCH":
        return compose_punch_id(track)
    if hud == "RUN":
        return compose_loco_id("RUN")
    if hud == "DASH":
        return compose_loco_id("DASH")
    raise SystemExit(f"unknown gesture {hud}")


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
    lyaw = [pose["LUpperArm"][2] for pose in clip]
    ryaw = [pose["RUpperArm"][2] for pose in clip]
    yaw = max(max(lyaw) - min(lyaw), max(ryaw) - min(ryaw))
    return {
        "peak": peak,
        "yaw": yaw,
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
    raise_key = hud.startswith("WAVE") or hud.startswith("UP") or hud in ("DNC L", "DNC S")
    if (not raise_key) and float(score["shoulder_lim"]) > 0.20:
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
    if hud.startswith("WAVE"):
        if hud in ("WAVE L", "WAVE R") and abs(float(style["yaw"]) - float(normal["yaw"])) <= 0.06:
            if float(style["clamp"]) > float(normal["clamp"]) + 0.03:
                return False, "more clamp"
            return True, "active, similar mapped yaw"
        extra = 0.02 if hud in ("WAVE L", "WAVE R") else 0.05
        if float(style["yaw"]) < float(normal["yaw"]) + extra:
            return False, "not more yaw travel"
        if float(style["clamp"]) > float(normal["clamp"]) + 0.03:
            return False, "more clamp"
        return True, "more yaw travel"
    if float(style["peak"]) < float(normal["peak"]) + 0.08:
        return False, "not more readable"
    if float(style["clamp"]) > float(normal["clamp"]) + 0.03:
        return False, "more clamp"
    if float(style["shoulder_lim"]) > float(normal["shoulder_lim"]) + 0.05:
        return False, "more shoulder saturation"
    return True, "higher peak, limits ok"


def load_gesture(source: str, hud: str, min_peak: float = 0.35
                 ) -> tuple[list[dict[str, tuple[float, float, float]]], list[float], int, int, float]:
    src = RAW / source
    if not src.exists():
        raise SystemExit(f"missing {source}; run fetch_bandai.py and fetch_bandai2.py")
    _root, frames, dt = parse_bvh(src)
    raw, sat, total, _, yaw, drops = bake(src, 0, len(frames) - 1)
    world0 = fk(_root, frames[0])
    scale = K_HIP_Y / world0["Hips"][1]
    plant = min(world0["Foot_L"][1], world0["Foot_R"][1]) * scale
    y_shift = K_SOLE_Y - plant
    track = source_track(src, frames, yaw, scale, y_shift)
    clip, dips = compose_gesture(raw, hud, drops, track)
    mid = clip[len(clip) // 2]
    mid_dip = dips[len(dips) // 2] if dips else 0.0
    plant_lt, plant_ls = two_bone_pitch(
        -K_HIP_X, K_HIP_Y, -K_HIP_X, K_PLANT + mid_dip, K_THIGH, K_SHIN, -.80, .90)
    if hud not in ("RUN", "DASH"):
        if abs(mid["LThigh"][1] - plant_lt) > 0.04 or abs(mid["LShin"][1] - plant_ls) > 0.04:
            raise SystemExit(f"{source} feet not planted")
    peak = max(arm_energy(pose) for pose in clip)
    if hud.startswith("WAVE") and peak < min_peak:
        raise SystemExit(f"{source} arm motion too small; peak={peak:.3f}")
    min_pel = min(pose["Pelvis"][1] for pose in clip)
    min_chest = min(pose["Chest"][1] for pose in clip)
    if min_pel < -1e-4 or min_chest < -1e-4:
        raise SystemExit(f"{source} torso leans back; pel={min_pel:.3f} chest={min_chest:.3f}")
    return clip, dips, sat, total, dt


def fmt_score(score: dict[str, float | int | bool]) -> str:
    return (
        f"frames={score['frames']} flash={score['flash']}B peak={float(score['peak']):.2f} "
        f"yaw={float(score['yaw']):.2f} "
        f"clamp={int(score['sat'])}/{int(score['total'])} "
        f"sh_lim={float(score['shoulder_lim']):.2f} ua_lim={float(score['upper_lim']):.2f} "
        f"both_up={float(score['both_up']):.2f} head_sh={float(score['head_sh']):.2f}"
    )


def write_gesture_bank(path: Path, clips: list[tuple[str, str, list[dict[str, tuple[float, float, float]]], list[float], int, int]]
                       ) -> None:
    offsets = []
    packed: list[str] = []
    dip_rows: list[str] = []
    cursor = 0
    n_frames = max(len(c[2]) for c in clips) if clips else 0
    for _hud, _src, frames, dips, _sat, _total in clips:
        offsets.append(cursor)
        for pose in frames:
            vals = []
            for name in BONES:
                r, p, y = pose[name]
                vals.extend((r, p, y))
            packed.append("    " + ", ".join(f"{v:.5f}f" for v in vals) + ",")
            cursor += 18 * 3
        if len(dips) != len(frames):
            raise SystemExit(f"{_hud} dip count {len(dips)} != frames {len(frames)}")
        padded = list(dips) + [0.0] * (n_frames - len(dips))
        dip_rows.append("    {" + ", ".join(f"{v:.5f}f" for v in padded) + "},")
    if packed:
        packed[-1] = packed[-1].rstrip(",")
    if dip_rows:
        dip_rows[-1] = dip_rows[-1].rstrip(",")
    lines = [
        "#pragma once",
        "// Generated by tools/arena_motion/retarget_bandai.py. Do not edit.",
        "// Bandai-Namco-Research-Motiondataset-2 P5 winners plus dataset-1 review clips.",
        "// Feet planted except RUN/DASH in-place gait. Dance/loco Root dip is a parallel channel. CC BY-NC 4.0.",
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
        f"inline constexpr float kGestureRootDip[{len(clips)}][{n_frames}]={{",
    ])
    lines.extend(dip_rows)
    lines.extend([
        "};",
        "} // namespace gundam_arena",
        "",
    ])
    path.write_text("\n".join(lines))


def q16(v: float) -> int:
    return int(max(-32767, min(32767, round(v * DANCE_SCALE))))


def write_dance_bank(path: Path, clips: list[tuple[str, str, list[dict[str, tuple[float, float, float]]], list[float], int, int]]
                     ) -> None:
    offsets = []
    packed: list[str] = []
    dip_rows: list[str] = []
    cursor = 0
    dip_cursor = 0
    dip_off = []
    for _hud, _src, frames, dips, _loop0, _loop1 in clips:
        offsets.append(cursor)
        dip_off.append(dip_cursor)
        for pose in frames:
            vals = []
            for name in BONES:
                r, p, y = pose[name]
                vals.extend((q16(r), q16(p), q16(y)))
            packed.append("    " + ", ".join(str(v) for v in vals) + ",")
            cursor += 18 * 3
        if len(dips) != len(frames):
            raise SystemExit(f"{_hud} dip count {len(dips)} != frames {len(frames)}")
        dip_rows.append("    " + ", ".join(str(q16(v)) for v in dips) + ",")
        dip_cursor += len(dips)
    if packed:
        packed[-1] = packed[-1].rstrip(",")
    if dip_rows:
        dip_rows[-1] = dip_rows[-1].rstrip(",")
    n_joint = cursor
    lines = [
        "#pragma once",
        "// Generated by tools/arena_motion/retarget_bandai.py. Do not edit.",
        "// Full dataset-1 dance-long / dance-short, SD identity keys, int16 / 10000.",
        "// Playback loops kDanceLoopStart..kDanceLoopEnd (skips source holds).",
        "// Not part of the player A/B ring. CC BY-NC 4.0.",
        "#include <cstdint>",
        "namespace gundam_arena {",
        f"inline constexpr int kDanceCount={len(clips)};",
        "inline constexpr float kDanceFps=30.f;",
        f"inline constexpr int kDanceScale={DANCE_SCALE};",
        "inline constexpr int kDanceFrames[" + str(len(clips)) + "]={"
        + ", ".join(str(len(c[2])) for c in clips) + "};",
        "inline constexpr int kDanceLoopStart[" + str(len(clips)) + "]={"
        + ", ".join(str(c[4]) for c in clips) + "};",
        "inline constexpr int kDanceLoopEnd[" + str(len(clips)) + "]={"
        + ", ".join(str(c[5]) for c in clips) + "};",
        "inline constexpr int kDanceOffset[" + str(len(clips)) + "]={"
        + ", ".join(str(o) for o in offsets) + "};",
        "inline constexpr int kDanceDipOffset[" + str(len(clips)) + "]={"
        + ", ".join(str(o) for o in dip_off) + "};",
        "inline constexpr int kDanceFlashBytes[" + str(len(clips)) + "]={"
        + ", ".join(str(len(c[2]) * 18 * 3 * 2 + len(c[3]) * 2) for c in clips) + "};",
        "inline constexpr const char* kDanceHud[" + str(len(clips)) + "]={"
        + ", ".join(f'"{c[0]}"' for c in clips) + "};",
        "inline constexpr const char* kDanceSource[" + str(len(clips)) + "]={"
        + ", ".join(f'"{c[1]}"' for c in clips) + "};",
        f"inline const int16_t kDanceJoints[{n_joint}]={{",
    ]
    lines.extend(packed)
    lines.extend([
        "};",
        f"inline const int16_t kDanceRootDip[{dip_cursor}]={{",
    ])
    lines.extend(dip_rows)
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
    _ref, _sat, _total, _dt, yaw, _dips = bake(kick_src, 56, 56, align=40)
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
    walk, wsat, wtotal, wdt, wyaw, _wdips = bake(walk_src, WALK_START, WALK_END)
    write_clip(WALK_H, walk, WALK_SOURCE, WALK_START, WALK_END, wsat, wtotal, "Walk")
    print(f"wrote {WALK_H} frames={len(walk)} dt={wdt:.4f} yaw={math.degrees(wyaw):.1f}deg clamp {wsat}/{wtotal}")
    print("runtime walk remains IK; walk clip is comparison-only")

    baked: list[tuple[str, str, list[dict[str, tuple[float, float, float]]], list[float], int, int]] = []
    style_lines = [
        "Bandai 2 P5 style compare (firmware keeps one take per slot)",
        "Clip flash bytes are frames*18*3*4. Host raster us comes from gundam_arena_test.",
        "Device FPS is not measured in this script.",
        "",
    ]
    for hud, normal_src, alts in GESTURE_SLOTS:
        clip, dips, sat, total, dt = load_gesture(normal_src, hud)
        best_src, best_clip, best_dips, best_sat, best_total = normal_src, clip, dips, sat, total
        normal_score = score_clip(clip, sat, total)
        style_lines.append(f"{hud} normal {normal_src} {fmt_score(normal_score)}")
        print(f"gesture {hud} {normal_src} {fmt_score(normal_score)} dt={dt:.4f}")
        chosen = "normal"
        note = "keep normal"
        for alt in alts:
            alt_clip, alt_dips, alt_sat, alt_total, alt_dt = load_gesture(alt, hud)
            alt_score = score_clip(alt_clip, alt_sat, alt_total)
            win, why = style_beats(hud, alt_score, normal_score)
            style_lines.append(f"  vs {alt} {fmt_score(alt_score)} -> {why}")
            print(f"  vs {alt} {fmt_score(alt_score)} -> {why} dt={alt_dt:.4f}")
            if win:
                best_src, best_clip, best_dips, best_sat, best_total = alt, alt_clip, alt_dips, alt_sat, alt_total
                chosen = alt
                note = why
        style_lines.append(f"  keep {chosen} ({note})")
        style_lines.append("")
        baked.append((hud, best_src, best_clip, best_dips, best_sat, best_total))
    style_lines.append("Dataset-1 review slots (normal only, A/B cycle)")
    style_lines.append("")
    for hud, normal_src, _alts in REVIEW_GESTURE_SLOTS:
        clip, dips, sat, total, dt = load_gesture(normal_src, hud, min_peak=0.12)
        score = score_clip(clip, sat, total)
        style_lines.append(f"{hud} {normal_src} {fmt_score(score)}")
        print(f"gesture {hud} {normal_src} {fmt_score(score)} dt={dt:.4f}")
        baked.append((hud, normal_src, clip, dips, sat, total))
    write_gesture_bank(GESTURE_H, baked)
    print(f"wrote {GESTURE_H}")

    dances: list[tuple[str, str, list[dict[str, tuple[float, float, float]]], list[float], int, int]] = []
    for hud, source in DANCE_SLOTS:
        src = RAW / source
        if not src.exists():
            raise SystemExit(f"missing {source}; run fetch_bandai.py")
        _root, frames, dt = parse_bvh(src)
        world0 = fk(_root, frames[0])
        left, right = world0["UpperLeg_L"], world0["UpperLeg_R"]
        yaw = math.atan2(left[2] - right[2], left[0] - right[0])
        scale = K_HIP_Y / world0["Hips"][1]
        plant = min(world0["Foot_L"][1], world0["Foot_R"][1]) * scale
        y_shift = K_SOLE_Y - plant
        track = source_track(src, frames, yaw, scale, y_shift)
        clip, dips = compose_dance_id(track, [])
        if len(clip) != len(frames):
            raise SystemExit(f"{hud} sliced; {len(clip)} vs {len(frames)} source frames")
        loop0, loop1 = dance_loop_range(clip, dips, 1.0 / dt if dt > 1e-4 else 30.0)
        if loop1 - loop0 < int(DANCE_LOOP_MIN_S * 30):
            raise SystemExit(f"{hud} loop too short; {loop0}..{loop1}")
        peak = max(arm_energy(pose) for pose in clip)
        max_dip = max(dips) if dips else 0.0
        min_dip = min(dips) if dips else 0.0
        thighs = [pose["LThigh"][1] for pose in clip]
        luas = [pose["LUpperArm"][1] for pose in clip]
        if peak < 0.35:
            raise SystemExit(f"{hud} arm motion too small; peak={peak:.3f}")
        if max_dip < 0.04:
            raise SystemExit(f"{hud} missing hip dip; max={max_dip:.3f}")
        if min_dip > -0.04:
            raise SystemExit(f"{hud} missing hop lift; min={min_dip:.3f}")
        if max(thighs) - min(thighs) < 0.25:
            raise SystemExit(f"{hud} legs glued; thigh span={max(thighs)-min(thighs):.3f}")
        late = luas[400:] if len(luas) > 400 else luas
        if max(late) - min(late) < 0.18:
            raise SystemExit(f"{hud} arms frozen after intro; span={max(late)-min(late):.3f}")
        loop_luas = [pose["LUpperArm"][1] for pose in clip[loop0:loop1]]
        loop_yaws = [pose["LUpperArm"][2] for pose in clip[loop0:loop1]]
        if max(loop_yaws) - min(loop_yaws) < 0.16:
            raise SystemExit(f"{hud} loop yaw frozen; span={max(loop_yaws)-min(loop_yaws):.3f}")
        if max(loop_luas) - min(loop_luas) < 0.18:
            raise SystemExit(f"{hud} loop arms frozen; span={max(loop_luas)-min(loop_luas):.3f}")
        first_hop = next((i for i in range(loop0, loop1) if dips[i] < -0.04), None)
        if first_hop is None or first_hop - loop0 > int(2.0 * 30):
            raise SystemExit(f"{hud} loop does not hop in first 2s; hop={first_hop}")
        last_hop = max(i for i in range(loop0, loop1) if dips[i] < -0.04)
        if loop1 - last_hop > DANCE_LOOP_PREROLL + 1:
            raise SystemExit(
                f"{hud} loop keeps dead outro after last hop; "
                f"last_hop={last_hop} end={loop1}")
        tail0 = max(loop0, loop1 - int(1.5 * 30))
        tail_span = 0.0
        for name in ("LUpperArm", "RUpperArm", "LThigh", "RThigh"):
            vals = [pose[name][1] for pose in clip[tail0:loop1]]
            yaws = [pose[name][2] for pose in clip[tail0:loop1]]
            tail_span += max(vals) - min(vals) + max(yaws) - min(yaws)
        if min(dips[tail0:loop1]) >= -0.04 and tail_span < 0.20:
            raise SystemExit(f"{hud} loop tail still frozen; span={tail_span:.3f}")
        score = score_clip(clip, 0, 1)
        print(f"dance {hud} {source} frames={len(clip)} loop={loop0}..{loop1} "
              f"flash={len(clip)*54*2+len(dips)*2}B "
              f"peak={peak:.2f} dip={max_dip:.3f} hop={min_dip:.3f} "
              f"thigh={max(thighs)-min(thighs):.3f} dt={dt:.4f} {fmt_score(score)}")
        dances.append((hud, source, clip, dips, loop0, loop1))
    write_dance_bank(DANCE_H, dances)
    print(f"wrote {DANCE_H}")

    report = Path(__file__).resolve().parent / "dataset2_preview.txt"
    lines = ["Bandai 2 locomotion preview (not in firmware)", ""]
    for source, label in PREVIEW_LOCO:
        src = RAW / source
        if not src.exists():
            lines.append(f"{label}: missing {source}")
            continue
        root, frames, dt = parse_bvh(src)
        raw, sat, total, _, yaw, _dips = bake(src, 0, min(59, len(frames) - 1))
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
