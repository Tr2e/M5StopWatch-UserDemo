#!/usr/bin/env python3
"""Generate the mechanical-hook StopWatch C152 to LEGO Technic back clip (V6).

The adapter nests inside the circular rear outline, grips the rear case edge
with four narrow diagonal C-hooks, and carries one two-hole Technic crossbeam at
the lower edge. It contains no magnets. Units are millimetres.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import trimesh


# Official StopWatch C152 rear geometry.
CASE_DIAMETER = 51.95
REAR_INSERT_DIAMETER = 49.20
BACKPLATE_THICKNESS = 2.80

# Four narrow C-hooks reach the front-shell shoulder measured from the official
# STL. These finished-part angles avoid all side protrusions across the full
# 5.5 mm hook width while leaving the cardinal I/O positions open.
HOOK_ANGLES = (70.0, 110.0, 250.0, 290.0)
HOOK_CASE_CLEARANCE = 0.25
HOOK_WALL_THICKNESS = 1.25
HOOK_WIDTH = 5.50
HOOK_WRAP_DEPTH = 16.40
HOOK_FRONT_SHOULDER_RADIUS = 25.49
HOOK_TOE_INTERFERENCE = 0.35
HOOK_TOE_HEIGHT = 1.00

# The rear pin headers are vertical on the left and right of the rear panel.
# These windows expose the complete 2.54 mm buses and allow a Dupont housing.
HEADER_WINDOW_X = 16.65
HEADER_WINDOW_WIDTH = 6.40
HEADER_WINDOW_HEIGHT = 19.20
HEADER_WINDOW_RADIUS = 2.00

# Speaker/vent opening. The opening is intentionally larger than the grille.
SPEAKER_WINDOW_Y = 18.10
SPEAKER_WINDOW_WIDTH = 13.00
SPEAKER_WINDOW_HEIGHT = 9.20
SPEAKER_WINDOW_RADIUS = 3.20

# LEGO Technic interface. The two holes match the top corner holes of the 5x7
# frame in the user's assembly: four 8 mm pitches = 32 mm.
TECHNIC_PITCH = 8.00
TECHNIC_HOLE_SPACING = 4 * TECHNIC_PITCH
TECHNIC_HOLE_DIAMETER = 4.90
TECHNIC_BEAM_THICKNESS = 7.80
CROSSBEAM_WIDTH = 44.00
CROSSBEAM_HEIGHT = 8.00
CROSSBEAM_CENTER_Y = -21.30


def box(extents: tuple[float, float, float], center=(0.0, 0.0, 0.0)) -> trimesh.Trimesh:
    mesh = trimesh.creation.box(extents=extents)
    mesh.apply_translation(center)
    return mesh


def cylinder(radius: float, height: float, center=(0.0, 0.0, 0.0), sections=96) -> trimesh.Trimesh:
    mesh = trimesh.creation.cylinder(radius=radius, height=height, sections=sections)
    mesh.apply_translation(center)
    return mesh


def union(meshes: list[trimesh.Trimesh]) -> trimesh.Trimesh:
    return trimesh.boolean.union(meshes, engine="manifold")


def difference(mesh: trimesh.Trimesh, cutters: list[trimesh.Trimesh]) -> trimesh.Trimesh:
    return trimesh.boolean.difference([mesh, *cutters], engine="manifold")


def rounded_bar(width: float, height: float, depth: float, center_y: float) -> trimesh.Trimesh:
    """A horizontal capsule-ended bar."""
    radius = height / 2
    z = depth / 2
    straight = box((width - 2 * radius, height, depth), (0.0, center_y, z))
    left = cylinder(radius, depth, (-width / 2 + radius, center_y, z))
    right = cylinder(radius, depth, (width / 2 - radius, center_y, z))
    return union([straight, left, right])


def rounded_window(
    width: float,
    height: float,
    radius: float,
    center_x: float,
    center_y: float,
    depth: float,
) -> trimesh.Trimesh:
    """Rounded rectangular cutter built from boxes and four cylinders."""
    z = BACKPLATE_THICKNESS / 2
    parts = [
        box((width - 2 * radius, height, depth), (center_x, center_y, z)),
        box((width, height - 2 * radius, depth), (center_x, center_y, z)),
    ]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(
                cylinder(
                    radius,
                    depth,
                    (
                        center_x + sx * (width / 2 - radius),
                        center_y + sy * (height / 2 - radius),
                        z,
                    ),
                )
            )
    return union(parts)


def radial_box(
    radial_size: float,
    tangential_size: float,
    z_size: float,
    radial_center: float,
    z_center: float,
    angle_deg: float,
) -> trimesh.Trimesh:
    mesh = box((radial_size, tangential_size, z_size), (radial_center, 0.0, z_center))
    mesh.apply_transform(
        trimesh.transformations.rotation_matrix(np.radians(angle_deg), (0.0, 0.0, 1.0))
    )
    return mesh


def micro_c_hook(angle_deg: float, wrap_depth: float = HOOK_WRAP_DEPTH) -> trimesh.Trimesh:
    """Short rear-edge hook; it does not run along the full case thickness."""
    case_radius = CASE_DIAMETER / 2
    wall_inner = case_radius + HOOK_CASE_CLEARANCE
    wall_outer = wall_inner + HOOK_WALL_THICKNESS

    # Root bridge overlaps the embedded plate and turns over its outer edge.
    bridge_inner = REAR_INSERT_DIAMETER / 2 - 1.2
    bridge = radial_box(
        wall_outer - bridge_inner,
        HOOK_WIDTH,
        1.20,
        (wall_outer + bridge_inner) / 2,
        0.55,
        angle_deg,
    )
    wall = radial_box(
        HOOK_WALL_THICKNESS,
        HOOK_WIDTH,
        wrap_depth + 1.0,
        (wall_inner + wall_outer) / 2,
        -wrap_depth / 2 + 0.30,
        angle_deg,
    )

    # The inward toe creates 0.4 mm radial preload against the rear shell.
    toe_inner = HOOK_FRONT_SHOULDER_RADIUS - HOOK_TOE_INTERFERENCE
    toe = radial_box(
        wall_outer - toe_inner,
        HOOK_WIDTH,
        HOOK_TOE_HEIGHT,
        (wall_outer + toe_inner) / 2,
        -wrap_depth + HOOK_TOE_HEIGHT / 2,
        angle_deg,
    )
    return union([bridge, wall, toe])


def embedded_back_clip(hook_wrap_depth: float = HOOK_WRAP_DEPTH) -> trimesh.Trimesh:
    plate = cylinder(
        REAR_INSERT_DIAMETER / 2,
        BACKPLATE_THICKNESS,
        (0.0, 0.0, BACKPLATE_THICKNESS / 2),
        sections=192,
    )
    beam = rounded_bar(
        CROSSBEAM_WIDTH,
        CROSSBEAM_HEIGHT,
        TECHNIC_BEAM_THICKNESS,
        CROSSBEAM_CENTER_Y,
    )
    hooks = [micro_c_hook(angle, hook_wrap_depth) for angle in HOOK_ANGLES]
    body = union([plate, beam, *hooks])

    through_depth = TECHNIC_BEAM_THICKNESS + 1.0
    cutters = [
        rounded_window(
            HEADER_WINDOW_WIDTH, HEADER_WINDOW_HEIGHT, HEADER_WINDOW_RADIUS,
            -HEADER_WINDOW_X, 0.0, through_depth,
        ),
        rounded_window(
            HEADER_WINDOW_WIDTH, HEADER_WINDOW_HEIGHT, HEADER_WINDOW_RADIUS,
            HEADER_WINDOW_X, 0.0, through_depth,
        ),
        rounded_window(
            SPEAKER_WINDOW_WIDTH, SPEAKER_WINDOW_HEIGHT, SPEAKER_WINDOW_RADIUS,
            0.0, SPEAKER_WINDOW_Y, through_depth,
        ),
    ]

    # Two pin holes exactly match the arrowed top holes of the 5x7 frame.
    for x in (-TECHNIC_HOLE_SPACING / 2, TECHNIC_HOLE_SPACING / 2):
        cutters.append(
            cylinder(
                TECHNIC_HOLE_DIAMETER / 2,
                through_depth,
                (x, CROSSBEAM_CENTER_Y, TECHNIC_BEAM_THICKNESS / 2),
            )
        )

    result = difference(body, cutters)
    # Put the deepest hook toe at Z=0 in the exported STL.
    result.apply_translation((0.0, 0.0, hook_wrap_depth))
    return result


def hook_fit_coupon(hook_wrap_depth: float = HOOK_WRAP_DEPTH) -> trimesh.Trimesh:
    """Two opposing production hooks on a narrow diagonal test strap."""
    strap = radial_box(
        CASE_DIAMETER - 5.0,
        7.0,
        1.60,
        0.0,
        0.80,
        HOOK_ANGLES[0],
    )
    coupon = union([
        strap,
        micro_c_hook(HOOK_ANGLES[0], hook_wrap_depth),
        micro_c_hook(HOOK_ANGLES[2], hook_wrap_depth),
    ])
    coupon.apply_translation((0.0, 0.0, hook_wrap_depth))
    return coupon


def technic_hole_coupon() -> trimesh.Trimesh:
    """Three through holes: 4.8, 4.9 and 5.0 mm."""
    body = rounded_bar(32.0, 10.0, TECHNIC_BEAM_THICKNESS, 0.0)
    cutters = [
        cylinder(d / 2, TECHNIC_BEAM_THICKNESS + 1.0, (x, 0.0, TECHNIC_BEAM_THICKNESS / 2))
        for x, d in zip((-8.0, 0.0, 8.0), (4.80, 4.90, 5.00))
    ]
    return difference(body, cutters)


def validate(name: str, mesh: trimesh.Trimesh) -> None:
    if not mesh.is_watertight:
        raise RuntimeError(f"{name} is not watertight")
    if mesh.volume <= 0:
        raise RuntimeError(f"{name} has non-positive volume")
    print(
        f"{name}: vertices={len(mesh.vertices)} faces={len(mesh.faces)} "
        f"extents={np.round(mesh.extents, 3)} mm volume={mesh.volume:.1f} mm^3"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("output_v6"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    meshes = {
        "stopwatch_hook_technic_back_clip_v6.stl": embedded_back_clip(),
        "rear_hook_fit_15p2mm_v6.stl": hook_fit_coupon(15.20),
        "rear_hook_fit_15p8mm_v6.stl": hook_fit_coupon(15.80),
        "rear_hook_fit_16p4mm_v6.stl": hook_fit_coupon(16.40),
        "technic_pin_hole_coupon_v6.stl": technic_hole_coupon(),
    }
    for filename, mesh in meshes.items():
        validate(filename, mesh)
        mesh.export(args.output / filename)


if __name__ == "__main__":
    main()
