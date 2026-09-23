#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import struct
import sys
import tempfile


HERE = pathlib.Path(__file__).resolve().parent
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("soft3d_compile_model", HERE / "compile_model.py")
assert SPEC and SPEC.loader
compiler = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = compiler
SPEC.loader.exec_module(compiler)


def make_glb(path: pathlib.Path, *, alpha: str = "OPAQUE", bad_index: bool = False,
             nan_position: bool = False) -> None:
    positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0),
                 (1.0, 1.0, 0.0), (0.0, 1.0, 0.0)]
    if nan_position:
        positions[2] = (float("nan"), 1.0, 0.0)
    indices = (0, 1, 2, 0, 2, 9 if bad_index else 3)
    position_blob = b"".join(struct.pack("<3f", *value) for value in positions)
    index_blob = struct.pack("<6H", *indices)
    blob = position_blob + index_blob
    document = {
        "asset": {"version": "2.0", "generator": "soft3d-test"},
        "buffers": [{"byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(position_blob)},
            {"buffer": 0, "byteOffset": len(position_blob), "byteLength": len(index_blob)},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
        "materials": [{"name": "orange", "alphaMode": alpha,
                       "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.5, 0.0, 1.0]}}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "material": 0}]}],
        "nodes": [{"name": "panel", "mesh": 0, "translation": [1, 2, 3]}],
        "scenes": [{"nodes": [0]}], "scene": 0,
    }
    json_chunk = json.dumps(document, separators=(",", ":")).encode()
    json_chunk += b" " * (-len(json_chunk) % 4)
    blob += b"\0" * (-len(blob) % 4)
    total = 12 + 8 + len(json_chunk) + 8 + len(blob)
    raw = (struct.pack("<4sII", b"glTF", 2, total) +
           struct.pack("<II", len(json_chunk), 0x4E4F534A) + json_chunk +
           struct.pack("<II", len(blob), 0x004E4942) + blob)
    path.write_bytes(raw)


def manifest(path: pathlib.Path) -> None:
    path.write_text('''name = "fixture_quad"
up = "+Y"
front = "+Z"
unit_scale = 2.0
pivot = "source"
target_profile = "30fps"
rigid_nodes = ["panel"]

[lod0]
max_screen_radius = 9999
''', encoding="utf-8")


def expect_failure(model: pathlib.Path, manifest_path: pathlib.Path, needle: str) -> None:
    document, blob = compiler.load_glb(model)
    import tomllib
    with manifest_path.open("rb") as stream:
        options = tomllib.load(stream)
    try:
        compiler.compile_asset(document, blob, options)
    except compiler.CompileError as error:
        assert needle in str(error), (needle, str(error))
    else:
        raise AssertionError(f"expected failure containing {needle!r}")


def run(output: pathlib.Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="soft3d-compiler-") as temporary:
        root = pathlib.Path(temporary)
        model, config = root / "model.glb", root / "model.toml"
        make_glb(model)
        manifest(config)
        assert compiler.main([str(model), "--manifest", str(config), "--out", str(output)]) == 0
        report = json.loads((output / "asset_report.json").read_text())
        assert report["counts"] == {
            "bones": 1, "lods": 1, "material_switches": 0, "materials": 1,
            "packed_quads": 1, "runtime_primitives": 1,
            "source_triangles": 2, "unique_vertices": 4,
        }
        assert report["materials"]["solid_fast_path_ratio"] == 1.0
        assert report["memory"]["maximum_projection_bytes"] == 48
        assert report["memory"]["rigid_scratch"] == {
            "vertex_capacity": 4, "bone_capacity": 1, "estimated_bytes": 176,
        }
        assert report["bounds"]["minimum"] == [2.0, 4.0, 6.0]
        assert report["bounds"]["maximum"] == [4.0, 6.0, 6.0]
        assert "PrimitiveTopology::Quad" in (output / "model_asset.h").read_text()

        invalid = root / "bad_index.glb"
        make_glb(invalid, bad_index=True)
        expect_failure(invalid, config, "out of range")
        make_glb(invalid, alpha="BLEND")
        expect_failure(invalid, config, "transparency")
        make_glb(invalid, nan_position=True)
        expect_failure(invalid, config, "NaN or infinity")
    print("soft3d compiler: quad packing, transforms, report, and rejection gates passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, type=pathlib.Path)
    run(parser.parse_args().out)
