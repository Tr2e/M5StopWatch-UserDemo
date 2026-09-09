#!/usr/bin/env python3
"""Validate production frames; never substitute this report for visual review."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import warnings

from PIL import Image, ImageDraw

Image.MAX_IMAGE_PIXELS = 16_000_000
warnings.simplefilter("error", Image.DecompressionBombWarning)


def relative_file(root, value):
    if not isinstance(value, str) or not value:
        raise ValueError("frame path must be a nonempty string")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError(f"unsafe frame path: {value}")
    resolved = (root / path).resolve()
    if not resolved.is_relative_to(root.resolve()):
        raise ValueError(f"frame escapes root: {value}")
    return resolved


def validate_manifest(data):
    if not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1:
        raise ValueError("manifest requires schema: 1")
    if set(data) - {"schema", "identity_view", "models"}:
        raise ValueError("unknown manifest fields")
    identity = data.get("identity_view")
    if not isinstance(identity, str) or not identity:
        raise ValueError("identity_view must be nonempty")
    models = data.get("models")
    if not isinstance(models, list) or not models:
        raise ValueError("models must be a nonempty list")
    ids = set()
    for model in models:
        if not isinstance(model, dict):
            raise ValueError("model must be an object")
        if set(model) - {"id", "required_views", "frames"}:
            raise ValueError("unknown model fields")
        name = model.get("id")
        if not isinstance(name, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", name) or name in ids:
            raise ValueError(f"invalid or duplicate model id: {name}")
        ids.add(name)
        required = model.get("required_views")
        if (not isinstance(required, list) or not required
                or any(not isinstance(v, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", v) for v in required)
                or len(set(required)) != len(required) or identity not in required):
            raise ValueError(f"{name}: required_views must be unique and include identity_view")
        frames = model.get("frames")
        if not isinstance(frames, list):
            raise ValueError(f"{name}: frames must be a list")
        views = set()
        for frame in frames:
            if not isinstance(frame, dict):
                raise ValueError(f"{name}: frame must be an object")
            if set(frame) - {"view", "file", "baseline_file", "expect_unchanged"}:
                raise ValueError(f"{name}: unknown frame fields (check comparison flag spelling)")
            view = frame.get("view")
            if (not isinstance(view, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", view)
                    or view in views):
                raise ValueError(f"{name}: invalid or duplicate view: {view}")
            views.add(view)
            if type(frame.get("expect_unchanged", False)) is not bool:
                raise ValueError(f"{name}/{view}: expect_unchanged must be boolean")
            for key in ("file", "baseline_file"):
                if key == "file" or key in frame:
                    relative_file(Path.cwd(), frame.get(key))
    return data


def read_image(path):
    with Image.open(path) as source:
        source.load()
        return source.convert("RGBA")


def fingerprint(frame):
    digest = hashlib.sha256()
    digest.update(f"{frame.width}x{frame.height}:RGBA:".encode("ascii"))
    digest.update(frame.tobytes())
    return digest.hexdigest()


def contact_sheet(frames, destination):
    if not frames:
        return
    columns = min(3, len(frames))
    width = max(frame.width for _, frame in frames)
    height = max(frame.height for _, frame in frames) + 28
    rows = (len(frames) + columns - 1) // columns
    if width * columns * height * rows > 100_000_000:
        raise ValueError("contact sheet exceeds 100 megapixels")
    sheet = Image.new("RGBA", (columns * width, rows * height), "#e8e5df")
    draw = ImageDraw.Draw(sheet)
    for index, (label, frame) in enumerate(frames):
        x, y = (index % columns) * width, (index // columns) * height
        draw.text((x + 6, y + 6), label, fill="#202020")
        sheet.paste(frame, (x, y + 28))  # No resampling, recoloring or repainting.
    sheet.save(destination)


def check(data, frames_root, baseline_root, output):
    data = validate_manifest(data)
    frames_root = frames_root.resolve(strict=True)
    if not frames_root.is_dir():
        raise ValueError("frames root must be a directory")
    if baseline_root is not None:
        baseline_root = baseline_root.resolve(strict=True)
        if not baseline_root.is_dir():
            raise ValueError("baseline root must be a directory")
    output.mkdir(parents=True, exist_ok=False)
    report = {
        "schema": 1, "evidence_checks_passed": False,
        "frames_root": str(frames_root),
        "baseline_root": str(baseline_root) if baseline_root else None,
        "limitations": ["No reference-fidelity or hardware certification.",
                        "Distinct hashes do not prove distinct model geometry."],
        "models": [], "errors": [],
    }
    identities = {}
    for model in data["models"]:
        name = model["id"]
        entry = {"id": name, "frames": []}
        report["models"].append(entry)
        supplied = {f["view"] for f in model["frames"]}
        for missing in sorted(set(model["required_views"]) - supplied):
            report["errors"].append(f"{name}: missing required view {missing}")
        sheet_frames = []
        for spec in model["frames"]:
            view = spec["view"]
            result = {"view": view, "file": spec["file"]}
            entry["frames"].append(result)
            try:
                frame = read_image(relative_file(frames_root, spec["file"]))
                digest = fingerprint(frame)
                result.update(size=list(frame.size), pixel_sha256=digest)
                sheet_frames.append((f"{name} / {view}", frame))
                if view == data["identity_view"]:
                    if digest in identities:
                        report["errors"].append(f"duplicate identity frame: {identities[digest]} and {name}")
                    identities[digest] = name
                if baseline_root is not None:
                    baseline_file = spec.get("baseline_file", spec["file"])
                    baseline = read_image(relative_file(baseline_root, baseline_file))
                    result["baseline_file"] = baseline_file
                    result["pixels_equal_to_baseline"] = digest == fingerprint(baseline)
                    if spec.get("expect_unchanged", False) and not result["pixels_equal_to_baseline"]:
                        report["errors"].append(f"{name}/{view}: expected unchanged pixels")
                elif spec.get("expect_unchanged", False):
                    report["errors"].append(f"{name}/{view}: baseline required for expect_unchanged")
            except (OSError, ValueError, Image.DecompressionBombError, Image.DecompressionBombWarning) as exc:
                result["error"] = str(exc)
                report["errors"].append(f"{name}/{view}: {exc}")
        try:
            contact_sheet(sheet_frames, output / f"{name}.png")
        except (OSError, ValueError) as exc:
            report["errors"].append(f"{name}/contact_sheet: {exc}")
    report["evidence_checks_passed"] = not report["errors"]
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--frames", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        data = json.loads(args.manifest.read_text(encoding="utf-8"))
        report = check(data, args.frames, args.baseline, args.output)
    except (OSError, ValueError) as exc:
        print(f"evidence input error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps({"evidence_checks_passed": report["evidence_checks_passed"],
                      "errors": report["errors"], "report": str(args.output / "report.json")}, ensure_ascii=False))
    return 0 if report["evidence_checks_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
