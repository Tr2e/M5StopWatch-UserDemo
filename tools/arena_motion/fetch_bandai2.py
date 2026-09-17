#!/usr/bin/env python3
"""Download the 10+10 Bandai dataset-2 takes used by Arena. Does not keep data.zip."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from urllib.request import urlopen

BASE = (
    "https://raw.githubusercontent.com/BandaiNamcoResearchInc/"
    "Bandai-Namco-Research-Motiondataset/master/"
    "dataset/Bandai-Namco-Research-Motiondataset-2"
)
OUT = Path(__file__).resolve().parent / "raw"

# 10 contents x normal_001, then the 10 style backups from the motion plan.
CLIPS = [
    ("walk", "normal", "001"),
    ("run", "normal", "001"),
    ("walk-turn-left", "normal", "001"),
    ("walk-turn-right", "normal", "001"),
    ("wave-left-hand", "normal", "001"),
    ("wave-right-hand", "normal", "001"),
    ("wave-both-hands", "normal", "001"),
    ("raise-up-left-hand", "normal", "001"),
    ("raise-up-right-hand", "normal", "001"),
    ("raise-up-both-hands", "normal", "001"),
    ("walk", "active", "001"),
    ("walk", "youthful", "001"),
    ("run", "active", "001"),
    ("run", "youthful", "001"),
    ("walk-turn-left", "active", "001"),
    ("walk-turn-right", "active", "001"),
    ("wave-left-hand", "active", "001"),
    ("wave-right-hand", "active", "001"),
    ("wave-both-hands", "youthful", "001"),
    ("raise-up-both-hands", "active", "001"),
]


def stem(content: str, style: str, take: str) -> str:
    return f"dataset-2_{content}_{style}_{take}"


def bvh_meta(path: Path) -> tuple[int, float]:
    frames = 0
    dt = 0.0
    for line in path.read_text().splitlines():
        if line.startswith("Frames:"):
            frames = int(line.split()[1])
        elif line.startswith("Frame Time:"):
            dt = float(line.split()[2])
            break
    return frames, dt


def fetch(rel: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 0:
        print(f"skip {dest.name}")
        return
    url = f"{BASE}/{rel}"
    print(f"fetch {url}")
    dest.write_bytes(urlopen(url, timeout=120).read())
    print(f"  -> {dest} ({dest.stat().st_size} bytes)")


def joint_names(path: Path) -> list[str]:
    names: list[str] = []
    tokens = path.read_text().replace("\t", " ").split()
    i = 0
    while i < len(tokens):
        if tokens[i] in ("ROOT", "JOINT"):
            names.append(tokens[i + 1])
            i += 2
            continue
        if tokens[i] == "MOTION":
            break
        i += 1
    return names


def channel_order(path: Path) -> str:
    text = path.read_text()
    for line in text.splitlines():
        if "Zrotation" in line and "Xrotation" in line:
            bits = line.split()
            axes = [b for b in bits if b.endswith("rotation")]
            return "".join(a[0] for a in axes)
    return "?"


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fetch("LICENSE", OUT / "dataset-2_LICENSE")
    rows = []
    for content, style, take in CLIPS:
        name = stem(content, style, take)
        bvh = OUT / f"{name}.bvh"
        fetch(f"data/{name}.bvh", bvh)
        frames, dt = bvh_meta(bvh)
        digest = hashlib.sha256(bvh.read_bytes()).hexdigest()
        fps = 0.0 if dt <= 0 else round(1.0 / dt, 3)
        rows.append({
            "file": bvh.name,
            "content": content,
            "style": style,
            "id": take,
            "frames": frames,
            "fps": fps,
            "sha256": digest,
        })
        print(f"  {name}: frames={frames} fps={fps} sha256={digest[:12]}")

    manifest = OUT / "dataset-2_manifest.json"
    manifest.write_text(json.dumps(rows, indent=2) + "\n")
    print(f"wrote {manifest} ({len(rows)} clips)")

    sample = OUT / "dataset-2_wave-left-hand_normal_001.bvh"
    ds1 = OUT / "dataset-1_kick_normal_001.bvh"
    expected = [
        "joint_Root", "Hips", "Spine", "Chest", "Neck", "Head",
        "Shoulder_L", "UpperArm_L", "LowerArm_L", "Hand_L",
        "Shoulder_R", "UpperArm_R", "LowerArm_R", "Hand_R",
        "UpperLeg_L", "LowerLeg_L", "Foot_L", "Toes_L",
        "UpperLeg_R", "LowerLeg_R", "Foot_R", "Toes_R",
    ]
    names = joint_names(sample)
    order = channel_order(sample)
    print("dataset-2 joints", names)
    print("dataset-2 rotation", order)
    if ds1.exists():
        names1 = joint_names(ds1)
        order1 = channel_order(ds1)
        print("dataset-1 joints match", names1 == names)
        print("dataset-1 rotation", order1, "match", order1 == order)
    if names != expected:
        raise SystemExit(f"dataset-2 hierarchy differs: {names}")
    if order != "ZXY":
        raise SystemExit(f"dataset-2 rotation is {order}, not ZXY")
    print("P1.3 ok: 22 bones and ZXY match dataset-1")


if __name__ == "__main__":
    main()
