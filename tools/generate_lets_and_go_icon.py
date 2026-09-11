#!/usr/bin/env python3
"""Generate the private Mini 4WD star-and-diamond launcher asset."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
IMAGE_DIR = ROOT / "main" / "assets" / "images"
SIZE = 200
SCALE = 4


def star(cx: float, cy: float, outer: float, inner: float) -> list[tuple[float, float]]:
    points: list[tuple[float, float]] = []
    for index in range(10):
        angle = -math.pi / 2.0 + index * math.pi / 5.0
        radius = outer if index % 2 == 0 else inner
        points.append((cx + math.cos(angle) * radius, cy + math.sin(angle) * radius))
    return points


def diamond(cx: float, cy: float, width: float, height: float) -> None:
    """Draw the faceted white diamond used by the classic blue AULDEY mark."""
    left = cx - width / 2.0
    right = cx + width / 2.0
    top = cy - height / 2.0
    shoulder = top + height * 0.25
    bottom = cy + height / 2.0
    top_left = left + width * 0.14
    top_right = right - width * 0.14
    crown_left = left + width * 0.34
    crown_right = right - width * 0.34

    white = (255, 255, 255)
    stroke = (26, 72, 154)
    polygon = [
        (top_left, top), (top_right, top), (right, shoulder),
        (cx, bottom), (left, shoulder),
    ]
    scaled = [(round(x * SCALE), round(y * SCALE)) for x, y in polygon]
    draw.polygon(scaled, fill=white)

    lines = [
        ((top_left, top), (crown_left, shoulder)),
        ((crown_left, shoulder), (cx, top)),
        ((cx, top), (crown_right, shoulder)),
        ((crown_right, shoulder), (top_right, top)),
        ((left, shoulder), (right, shoulder)),
        ((crown_left, shoulder), (cx, bottom)),
        ((cx, top), (cx, bottom)),
        ((crown_right, shoulder), (cx, bottom)),
    ]
    for start, end in lines:
        draw.line(
            (round(start[0] * SCALE), round(start[1] * SCALE),
             round(end[0] * SCALE), round(end[1] * SCALE)),
            fill=stroke,
            width=2 * SCALE,
            joint="curve",
        )


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def wrapped(values: list[str], columns: int = 20) -> str:
    return "\n".join(
        "    " + ", ".join(values[index:index + columns]) + ","
        for index in range(0, len(values), columns)
    )


canvas = Image.new("RGB", (SIZE * SCALE, SIZE * SCALE), (0, 0, 0))
draw = ImageDraw.Draw(canvas)

def rect(box: tuple[int, int, int, int], fill: tuple[int, int, int]) -> None:
    draw.rounded_rectangle(tuple(value * SCALE for value in box), radius=5 * SCALE, fill=fill)


rect((8, 54, 100, 146), (218, 38, 47))
rect((100, 54, 192, 146), (26, 72, 154))
draw.polygon([(x * SCALE, y * SCALE) for x, y in star(54, 100, 36, 15)], fill=(255, 255, 255))
# Align the diamond's longest shoulder line with the star's widest arm line;
# matching only the outer bounds makes the diamond look perceptually too high.
diamond(146, 105, 64, 65)

source = canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
png_path = IMAGE_DIR / "icon_lets_and_go.png"
source.save(png_path)

byte_values: list[str] = []
for red, green, blue in source.get_flattened_data():
    pixel = rgb565(red, green, blue)
    byte_values.extend((f"0x{pixel & 0xFF:02x}", f"0x{pixel >> 8:02x}"))

output = f'''#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMAGE_ICON_LETS_AND_GO
#define LV_ATTRIBUTE_IMAGE_ICON_LETS_AND_GO
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ICON_LETS_AND_GO uint8_t icon_lets_and_go_map[] = {{
{wrapped(byte_values)}
}};

const lv_image_dsc_t icon_lets_and_go = {{
    .header.cf    = LV_COLOR_FORMAT_RGB565,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w     = 200,
    .header.h     = 200,
    .data_size    = 40000 * 2,
    .data         = icon_lets_and_go_map,
}};
'''

(IMAGE_DIR / "icon_lets_and_go.c").write_text(output)
print(f"Generated {png_path} and icon_lets_and_go.c")
