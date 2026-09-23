#!/usr/bin/env python3
"""Compile a deliberately small, deterministic glTF 2.0/GLB subset for Soft3D.

The runtime's first portable asset ABI is solid-color geometry with optional
rigid node bindings. Unsupported glTF features are rejected instead of being
silently approximated on the device.
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import re
import struct
import sys
import tomllib
from dataclasses import dataclass
from typing import Any, Iterable


class CompileError(ValueError):
    pass


COMPONENTS = {
    5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2),
    5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4),
}
TYPE_WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


@dataclass(frozen=True)
class Material:
    color: int
    two_sided: bool
    name: str


@dataclass
class Triangle:
    indices: tuple[int, int, int]
    material: int
    rigid_part: int
    two_sided: bool
    source_order: int


def _finite(values: Iterable[float], context: str) -> tuple[float, ...]:
    result = tuple(float(v) for v in values)
    if not all(math.isfinite(v) for v in result):
        raise CompileError(f"{context} contains NaN or infinity")
    return result


def _mul(a: tuple[float, ...], b: tuple[float, ...]) -> tuple[float, ...]:
    out = [0.0] * 16
    for row in range(4):
        for col in range(4):
            out[row * 4 + col] = sum(a[row * 4 + k] * b[k * 4 + col] for k in range(4))
    return tuple(out)


IDENTITY = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)


def _node_matrix(node: dict[str, Any]) -> tuple[float, ...]:
    if "matrix" in node:
        src = _finite(node["matrix"], "node matrix")
        if len(src) != 16:
            raise CompileError("node matrix must contain 16 values")
        return tuple(src[col * 4 + row] for row in range(4) for col in range(4))
    t = _finite(node.get("translation", (0, 0, 0)), "node translation")
    s = _finite(node.get("scale", (1, 1, 1)), "node scale")
    q = _finite(node.get("rotation", (0, 0, 0, 1)), "node rotation")
    if len(t) != 3 or len(s) != 3 or len(q) != 4:
        raise CompileError("invalid node TRS width")
    x, y, z, w = q
    length = math.sqrt(x*x + y*y + z*z + w*w)
    if length <= 1e-12:
        raise CompileError("zero-length node quaternion")
    x, y, z, w = x/length, y/length, z/length, w/length
    return (
        (1-2*y*y-2*z*z)*s[0], (2*x*y-2*z*w)*s[1], (2*x*z+2*y*w)*s[2], t[0],
        (2*x*y+2*z*w)*s[0], (1-2*x*x-2*z*z)*s[1], (2*y*z-2*x*w)*s[2], t[1],
        (2*x*z-2*y*w)*s[0], (2*y*z+2*x*w)*s[1], (1-2*x*x-2*y*y)*s[2], t[2],
        0.0, 0.0, 0.0, 1.0,
    )


def _point(matrix: tuple[float, ...], point: Iterable[float]) -> tuple[float, float, float]:
    x, y, z = point
    return (matrix[0]*x+matrix[1]*y+matrix[2]*z+matrix[3],
            matrix[4]*x+matrix[5]*y+matrix[6]*z+matrix[7],
            matrix[8]*x+matrix[9]*y+matrix[10]*z+matrix[11])


def _det3(matrix: tuple[float, ...]) -> float:
    a, b, c, d, e, f, g, h, i = (matrix[0], matrix[1], matrix[2], matrix[4],
                                  matrix[5], matrix[6], matrix[8], matrix[9], matrix[10])
    return a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g)


def _axis(value: str) -> tuple[float, float, float]:
    match = re.fullmatch(r"([+-])([XYZ])", value.upper())
    if not match:
        raise CompileError(f"invalid axis {value!r}; expected +X, -X, +Y, ...")
    result = [0.0, 0.0, 0.0]
    result["XYZ".index(match.group(2))] = 1.0 if match.group(1) == "+" else -1.0
    return tuple(result)  # type: ignore[return-value]


def _cross(a: tuple[float, ...], b: tuple[float, ...]) -> tuple[float, float, float]:
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def _dot(a: tuple[float, ...], b: tuple[float, ...]) -> float:
    return sum(x*y for x, y in zip(a, b))


def _coordinate_matrix(up_name: str, front_name: str, scale: float) -> tuple[float, ...]:
    up, front = _axis(up_name), _axis(front_name)
    if abs(_dot(up, front)) > 1e-6:
        raise CompileError("up and front axes must be perpendicular")
    right = _cross(up, front)
    return (right[0]*scale, right[1]*scale, right[2]*scale, 0,
            up[0]*scale, up[1]*scale, up[2]*scale, 0,
            front[0]*scale, front[1]*scale, front[2]*scale, 0,
            0, 0, 0, 1)


def load_glb(path: pathlib.Path) -> tuple[dict[str, Any], bytes]:
    raw = path.read_bytes()
    if len(raw) < 20:
        raise CompileError("GLB is truncated")
    magic, version, total = struct.unpack_from("<4sII", raw)
    if magic != b"glTF" or version != 2 or total != len(raw):
        raise CompileError("invalid GLB 2.0 header or declared length")
    offset, json_chunk, bin_chunk = 12, None, b""
    while offset < len(raw):
        if offset + 8 > len(raw):
            raise CompileError("truncated GLB chunk header")
        length, kind = struct.unpack_from("<II", raw, offset)
        offset += 8
        payload = raw[offset:offset+length]
        if len(payload) != length:
            raise CompileError("truncated GLB chunk")
        offset += length
        if kind == 0x4E4F534A:
            if json_chunk is not None:
                raise CompileError("GLB contains multiple JSON chunks")
            json_chunk = payload
        elif kind == 0x004E4942:
            if bin_chunk:
                raise CompileError("GLB contains multiple BIN chunks")
            bin_chunk = payload
    if json_chunk is None:
        raise CompileError("GLB has no JSON chunk")
    try:
        document = json.loads(json_chunk.rstrip(b" \t\r\n\0"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CompileError(f"invalid GLB JSON: {error}") from error
    if document.get("asset", {}).get("version") != "2.0":
        raise CompileError("glTF asset.version must be 2.0")
    buffers = document.get("buffers", [])
    if len(buffers) != 1 or "uri" in buffers[0]:
        raise CompileError("MVP compiler requires one embedded GLB buffer")
    if int(buffers[0].get("byteLength", -1)) > len(bin_chunk):
        raise CompileError("declared buffer is larger than the BIN chunk")
    return document, bin_chunk


def _accessor(document: dict[str, Any], blob: bytes, index: int) -> list[Any]:
    accessors, views = document.get("accessors", []), document.get("bufferViews", [])
    if not 0 <= index < len(accessors):
        raise CompileError(f"accessor {index} is out of range")
    acc = accessors[index]
    if "sparse" in acc:
        raise CompileError("sparse accessors are not supported")
    if "bufferView" not in acc:
        raise CompileError("accessors without bufferView are not supported")
    view_index = int(acc["bufferView"])
    if not 0 <= view_index < len(views):
        raise CompileError("accessor bufferView is out of range")
    view = views[view_index]
    component = int(acc.get("componentType", 0))
    kind = acc.get("type")
    if component not in COMPONENTS or kind not in TYPE_WIDTH:
        raise CompileError("unsupported accessor component or shape")
    fmt, size = COMPONENTS[component]
    width, count = TYPE_WIDTH[kind], int(acc.get("count", -1))
    if count < 0:
        raise CompileError("invalid accessor count")
    packed = size * width
    stride = int(view.get("byteStride", packed))
    if stride < packed:
        raise CompileError("accessor byteStride is smaller than its element")
    start = int(view.get("byteOffset", 0)) + int(acc.get("byteOffset", 0))
    limit = int(view.get("byteOffset", 0)) + int(view.get("byteLength", -1))
    if start < 0 or limit < start or (count and start+(count-1)*stride+packed > limit) or limit > len(blob):
        raise CompileError("accessor exceeds its bufferView")
    normalized = bool(acc.get("normalized", False))
    result = []
    for item in range(count):
        values = struct.unpack_from("<" + fmt*width, blob, start+item*stride)
        if normalized and component != 5126:
            if component in (5120, 5122):
                divisor = 127.0 if component == 5120 else 32767.0
                values = tuple(max(-1.0, value/divisor) for value in values)
            else:
                divisor = 255.0 if component == 5121 else 65535.0
                values = tuple(value/divisor for value in values)
        result.append(values[0] if width == 1 else tuple(values))
    return result


def _rgb565(factor: Iterable[float]) -> int:
    rgba = _finite(factor, "baseColorFactor")
    if len(rgba) != 4 or not all(0 <= value <= 1 for value in rgba):
        raise CompileError("baseColorFactor must contain four values in [0, 1]")
    if rgba[3] < 0.999:
        raise CompileError("transparent baseColorFactor is unsupported")
    r, g, b = (round(rgba[0]*31), round(rgba[1]*63), round(rgba[2]*31))
    return (r << 11) | (g << 5) | b


def _materials(document: dict[str, Any], manifest: dict[str, Any]) -> list[Material]:
    fallback = manifest.get("default_color", [0.75, 0.75, 0.75, 1.0])
    source = document.get("materials") or [{"name": "default", "pbrMetallicRoughness": {"baseColorFactor": fallback}}]
    result = []
    for index, material in enumerate(source):
        if material.get("alphaMode", "OPAQUE") != "OPAQUE" or "extensions" in material:
            raise CompileError(f"material {index} uses unsupported transparency or extensions")
        pbr = material.get("pbrMetallicRoughness", {})
        if any(key in pbr for key in ("baseColorTexture", "metallicRoughnessTexture")) or any(
                key in material for key in ("normalTexture", "occlusionTexture", "emissiveTexture")):
            raise CompileError(f"material {index} uses textures; current ABI is solid-color only")
        result.append(Material(_rgb565(pbr.get("baseColorFactor", fallback)),
                               bool(material.get("doubleSided", manifest.get("two_sided", False))),
                               str(material.get("name", f"material_{index}"))))
    return result


def _normal(a: tuple[float, ...], b: tuple[float, ...], c: tuple[float, ...]) -> tuple[float, float, float]:
    u, v = tuple(b[i]-a[i] for i in range(3)), tuple(c[i]-a[i] for i in range(3))
    value = _cross(u, v)
    length = math.sqrt(_dot(value, value))
    if length <= 1e-10:
        raise CompileError("degenerate triangle detected")
    return tuple(component/length for component in value)  # type: ignore[return-value]


def _pack_primitives(triangles: list[Triangle], positions: list[tuple[float, float, float]]) -> tuple[list[dict[str, Any]], int]:
    output, packed = [], 0
    index = 0
    while index < len(triangles):
        first = triangles[index]
        if index+1 < len(triangles):
            second = triangles[index+1]
            a, b, c = first.indices
            # Safe form only: consecutive ABC + ACD with identical semantic state.
            if (second.indices[0] == a and second.indices[1] == c and
                    first.material == second.material and first.rigid_part == second.rigid_part and
                    first.two_sided == second.two_sided):
                d = second.indices[2]
                n0, n1 = _normal(positions[a], positions[b], positions[c]), _normal(positions[a], positions[c], positions[d])
                if _dot(n0, n1) >= 0.9999:
                    output.append({"index": (a, b, c, d), "material": first.material,
                                   "rigid": first.rigid_part, "two_sided": first.two_sided})
                    packed += 1
                    index += 2
                    continue
        a, b, c = first.indices
        output.append({"index": (a, b, c, c), "material": first.material,
                       "rigid": first.rigid_part, "two_sided": first.two_sided})
        index += 1
    return output, packed


def compile_asset(document: dict[str, Any], blob: bytes, manifest: dict[str, Any]) -> dict[str, Any]:
    name = str(manifest.get("name", "")).strip()
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name):
        raise CompileError("manifest name must be a C identifier")
    scale = float(manifest.get("unit_scale", 1.0))
    if not math.isfinite(scale) or scale <= 0:
        raise CompileError("unit_scale must be finite and positive")
    coordinate = _coordinate_matrix(str(manifest.get("up", "+Y")), str(manifest.get("front", "+Z")), scale)
    materials = _materials(document, manifest)
    nodes, meshes = document.get("nodes", []), document.get("meshes", [])
    scene_index = int(document.get("scene", 0))
    scenes = document.get("scenes", [])
    if not scenes or not 0 <= scene_index < len(scenes):
        raise CompileError("glTF must select a valid scene")
    rigid_names = list(manifest.get("rigid_nodes", []))
    rigid_map = {name: index for index, name in enumerate(rigid_names)}
    if len(rigid_map) != len(rigid_names) or len(rigid_map) > 31:
        raise CompileError("rigid_nodes must contain at most 31 unique names")
    positions: list[tuple[float, float, float]] = []
    vertex_map: dict[tuple[bytes, int], int] = {}
    triangles: list[Triangle] = []
    visited: set[int] = set()

    def visit(node_index: int, parent: tuple[float, ...]) -> None:
        if not 0 <= node_index < len(nodes):
            raise CompileError("scene node index is out of range")
        if node_index in visited:
            raise CompileError("node graph is cyclic or instanced; duplicate nodes explicitly")
        visited.add(node_index)
        node = nodes[node_index]
        world = _mul(parent, _node_matrix(node))
        rigid = rigid_map.get(str(node.get("name", "")), 0)
        if "skin" in node:
            raise CompileError("weighted glTF skins are unsupported; use rigid_nodes")
        if "mesh" in node:
            mesh_index = int(node["mesh"])
            if not 0 <= mesh_index < len(meshes):
                raise CompileError("node mesh is out of range")
            final = _mul(coordinate, world)
            mirrored = _det3(final) < 0
            for primitive in meshes[mesh_index].get("primitives", []):
                if int(primitive.get("mode", 4)) != 4:
                    raise CompileError("only indexed TRIANGLES primitives are supported")
                attributes = primitive.get("attributes", {})
                if "POSITION" not in attributes or "indices" not in primitive:
                    raise CompileError("each primitive needs POSITION and indices")
                unsupported = set(attributes) - {"POSITION", "NORMAL"}
                if unsupported:
                    raise CompileError("unsupported vertex attributes: " + ", ".join(sorted(unsupported)))
                source_positions = _accessor(document, blob, int(attributes["POSITION"]))
                source_indices = _accessor(document, blob, int(primitive["indices"]))
                if len(source_indices) % 3:
                    raise CompileError("triangle index count is not divisible by three")
                material_index = int(primitive.get("material", 0))
                if not 0 <= material_index < len(materials):
                    raise CompileError("primitive material is out of range")
                local: list[int] = []
                for raw_point in source_positions:
                    if not isinstance(raw_point, tuple) or len(raw_point) != 3:
                        raise CompileError("POSITION accessor must be VEC3")
                    point = _finite(_point(final, raw_point), "transformed position")
                    key = (struct.pack("<3f", *point), rigid)
                    if key not in vertex_map:
                        if len(positions) >= 65535:
                            raise CompileError("asset exceeds 16-bit shared vertex capacity")
                        vertex_map[key] = len(positions)
                        positions.append(point)  # type: ignore[arg-type]
                    local.append(vertex_map[key])
                for offset in range(0, len(source_indices), 3):
                    source = [int(source_indices[offset+i]) for i in range(3)]
                    if any(value < 0 or value >= len(local) for value in source):
                        raise CompileError("primitive index is out of range")
                    mapped = (local[source[0]], local[source[1]], local[source[2]])
                    if mirrored:
                        mapped = (mapped[0], mapped[2], mapped[1])
                    _normal(positions[mapped[0]], positions[mapped[1]], positions[mapped[2]])
                    triangles.append(Triangle(mapped, material_index, rigid,
                                              materials[material_index].two_sided, len(triangles)))
        for child in node.get("children", []):
            visit(int(child), world)

    for root in scenes[scene_index].get("nodes", []):
        visit(int(root), IDENTITY)
    if not positions or not triangles:
        raise CompileError("scene contains no renderable triangles")

    pivot = str(manifest.get("pivot", "ground_center"))
    minimum = [min(point[i] for point in positions) for i in range(3)]
    maximum = [max(point[i] for point in positions) for i in range(3)]
    if pivot == "ground_center":
        shift = (-(minimum[0]+maximum[0])/2, -minimum[1], -(minimum[2]+maximum[2])/2)
    elif pivot == "source":
        shift = (0.0, 0.0, 0.0)
    else:
        raise CompileError("pivot must be ground_center or source")
    positions = [tuple(point[i]+shift[i] for i in range(3)) for point in positions]
    primitives, packed_quads = _pack_primitives(triangles, positions)
    normals, anchors = [], []
    for primitive in primitives:
        a, b, c, _ = primitive["index"]
        normals.append(_normal(positions[a], positions[b], positions[c]))
        anchors.append(positions[a])
    minimum = [min(point[i] for point in positions) for i in range(3)]
    maximum = [max(point[i] for point in positions) for i in range(3)]
    center = tuple((minimum[i]+maximum[i])/2 for i in range(3))
    radius = max(math.sqrt(sum((point[i]-center[i])**2 for i in range(3))) for point in positions)
    lod_tables = [(key, value) for key, value in manifest.items() if re.fullmatch(r"lod\d+", key)]
    lod_tables.sort(key=lambda item: int(item[0][3:]))
    if not lod_tables:
        lod_tables = [("lod0", {"max_screen_radius": 9999})]
    lods = []
    for expected, (key, value) in enumerate(lod_tables):
        if key != f"lod{expected}" or not isinstance(value, dict):
            raise CompileError("LOD tables must be contiguous from lod0")
        first = int(value.get("first_primitive", 0))
        count = int(value.get("primitive_count", len(primitives)-first))
        threshold = float(value.get("max_screen_radius", 9999))
        if first < 0 or count <= 0 or first+count > len(primitives) or not math.isfinite(threshold):
            raise CompileError(f"invalid {key} range or threshold")
        lods.append((first, count, threshold))
    bones = []
    for index, bone_name in enumerate(rigid_names):
        node_index = next((i for i, node in enumerate(nodes) if node.get("name") == bone_name), None)
        if node_index is None:
            raise CompileError(f"rigid node {bone_name!r} does not exist")
        parent = -1
        for candidate, node in enumerate(nodes):
            if node_index in node.get("children", []):
                parent_name = str(nodes[candidate].get("name", ""))
                parent = rigid_map.get(parent_name, -1)
                break
        bones.append((parent, _fnv16(bone_name), (0.0, 0.0, 0.0)))
    return {
        "name": name, "positions": positions, "normals": normals, "anchors": anchors,
        "primitives": primitives, "materials": materials, "lods": lods, "bones": bones,
        "bounds": {"minimum": minimum, "maximum": maximum, "center": center,
                   "radius": radius, "ground_y": minimum[1]},
        "source_triangles": len(triangles), "packed_quads": packed_quads,
        "target_profile": str(manifest.get("target_profile", "30fps")),
    }


def _fnv16(value: str) -> int:
    result = 2166136261
    for byte in value.encode("utf-8"):
        result = ((result ^ byte) * 16777619) & 0xFFFFFFFF
    return ((result >> 16) ^ result) & 0xFFFF


def _f(value: float) -> str:
    if value == 0:
        value = 0.0
    text = f"{value:.9g}"
    if "." not in text and "e" not in text.lower():
        text += ".0"
    return text + "f"


def write_header(asset: dict[str, Any], path: pathlib.Path) -> None:
    namespace = asset["name"]
    positions, primitives = asset["positions"], asset["primitives"]
    materials, lods, bones = asset["materials"], asset["lods"], asset["bones"]
    flags = "AssetDeterministicOrder|AssetAllSolid" + ("|AssetRigidSkeleton" if bones else "")
    profile = "SolidRigid" if bones else "SolidStatic"
    lines = ["#pragma once", "", '#include "main/apps/common/soft3d/asset/model_asset.h"', "",
             f"namespace soft3d::assets::{namespace} {{", ""]
    def vector_array(name: str, values: list[tuple[float, ...]]) -> None:
        lines.append(f"inline constexpr Vec3 {name}[]={{")
        lines.extend("    {" + ",".join(_f(v) for v in value) + "}," for value in values)
        lines.append("};")
    vector_array("kPositions", positions)
    vector_array("kNormals", asset["normals"])
    vector_array("kAnchors", asset["anchors"])
    lines.append("inline constexpr Primitive kPrimitives[]={")
    for primitive in primitives:
        topology = "PrimitiveTopology::Quad" if primitive["index"][2] != primitive["index"][3] else "PrimitiveTopology::Triangle"
        pflags = "PrimitiveTwoSided" if primitive["two_sided"] else "0"
        lines.append(f"    {{{{{','.join(str(v) for v in primitive['index'])}}},{primitive['material']},{primitive['rigid']},{topology},{pflags}}},")
    lines.append("};")
    lines.append("inline constexpr Material kMaterials[]={")
    lines.extend(f"    {{{material.color},MaterialKind::Solid,0,255,0}}," for material in materials)
    lines.append("};")
    lines.append("inline constexpr uint16_t kOrder[]={" + ",".join(str(i) for i in range(len(primitives))) + "};")
    lines.append("inline constexpr LodRange kLods[]={")
    lines.extend(f"    {{{first},{count},{_f(threshold)}}}," for first, count, threshold in lods)
    lines.append("};")
    if bones:
        lines.append("inline constexpr Bone kBones[]={")
        lines.extend(f"    {{{parent},{name_hash},{{{','.join(_f(v) for v in pivot)}}}}}," for parent, name_hash, pivot in bones)
        lines.append("};")
    resident = len(positions)*12 + len(primitives)*(14+24+2) + len(materials)*6 + len(lods)*12 + len(bones)*16
    bounds = asset["bounds"]
    vec = lambda value: "{" + ",".join(_f(v) for v in value) + "}"
    bone_span = f"{{kBones,{len(bones)}}}" if bones else "{nullptr,0}"
    lines.extend([
        "inline const ModelAsset kAsset={",
        f'    "{namespace}",{{kPositions,{len(positions)}}},{{kNormals,{len(primitives)}}},{{kAnchors,{len(primitives)}}},',
        f"    {{kPrimitives,{len(primitives)}}},{{kMaterials,{len(materials)}}},{{kOrder,{len(primitives)}}},{{kLods,{len(lods)}}},{bone_span},",
        f"    {{{vec(bounds['minimum'])},{vec(bounds['maximum'])},{vec(bounds['center'])},{_f(bounds['radius'])},{_f(bounds['ground_y'])}}},",
        f"    {{{resident},{len(positions)*12},{len(primitives)*12},{len(primitives)}}},{flags},AssetProfile::{profile}",
        "};", "inline const ModelAsset& model(){return kAsset;}", "", f"}} // namespace soft3d::assets::{namespace}", "",
    ])
    path.write_text("\n".join(lines), encoding="utf-8")


def write_report(asset: dict[str, Any], path: pathlib.Path) -> None:
    primitive_count, position_count = len(asset["primitives"]), len(asset["positions"])
    material_switches = sum(asset["primitives"][i]["material"] != asset["primitives"][i-1]["material"]
                            for i in range(1, primitive_count))
    resident = position_count*12 + primitive_count*(14+24+2) + len(asset["materials"])*6 + len(asset["lods"])*12 + len(asset["bones"])*16
    # RigidRenderScratch keeps camera + projected Vec3 arrays, rigid-part keys,
    # two validity arrays, one 3x4 matrix and one flag per bone, then counters.
    # Recommend at least one bone slot for static assets because std::array<T,0>
    # has implementation-specific non-zero object size.
    scratch_bones = max(1, len(asset["bones"]))
    rigid_scratch_bytes = (position_count*28 + scratch_bones*49 + 12 + 3) & ~3
    report = {
        "schema": 1, "name": asset["name"], "target_profile": asset["target_profile"],
        "counts": {"unique_vertices": position_count, "source_triangles": asset["source_triangles"],
                   "runtime_primitives": primitive_count, "packed_quads": asset["packed_quads"],
                   "materials": len(asset["materials"]), "material_switches": material_switches,
                   "bones": len(asset["bones"]), "lods": len(asset["lods"])},
        "memory": {"resident_bytes": resident, "maximum_projection_bytes": position_count*12,
                   "maximum_command_bytes": primitive_count*12,
                   "rigid_scratch": {"vertex_capacity": position_count,
                                     "bone_capacity": scratch_bones,
                                     "estimated_bytes": rigid_scratch_bytes}},
        "materials": {"solid_primitives": primitive_count, "general_primitives": 0,
                      "solid_fast_path_ratio": 1.0},
        "bounds": asset["bounds"],
        "warnings": (["more than 50% of primitives are double-sided"] if
                     sum(p["two_sided"] for p in asset["primitives"])*2 > primitive_count else []),
    }
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=pathlib.Path)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path,
                        help="output stem or directory (writes model_asset.h and asset_report.json)")
    args = parser.parse_args(argv)
    try:
        if args.model.suffix.lower() != ".glb":
            raise CompileError("input must be a binary .glb file")
        document, blob = load_glb(args.model)
        with args.manifest.open("rb") as stream:
            manifest = tomllib.load(stream)
        asset = compile_asset(document, blob, manifest)
        output = args.out
        if output.suffix:
            header = output
            report = output.with_suffix(".report.json")
        else:
            output.mkdir(parents=True, exist_ok=True)
            header, report = output/"model_asset.h", output/"asset_report.json"
        write_header(asset, header)
        write_report(asset, report)
        print(f"compiled {asset['name']}: {len(asset['positions'])} vertices, "
              f"{len(asset['primitives'])} primitives, {asset['packed_quads']} packed quads")
        print(f"header: {header}\nreport: {report}")
        return 0
    except (CompileError, OSError, tomllib.TOMLDecodeError) as error:
        print(f"soft3d asset compile failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
