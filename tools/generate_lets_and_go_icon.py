#!/usr/bin/env python3
"""Generate the private Let's & Go!! TAMIYA twin-star launcher asset."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


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


rect((14, 41, 100, 127), (218, 38, 47))
rect((100, 41, 186, 127), (26, 72, 154))
draw.polygon([(x * SCALE, y * SCALE) for x, y in star(57, 84, 34, 14)], fill=(255, 255, 255))
draw.polygon([(x * SCALE, y * SCALE) for x, y in star(143, 84, 34, 14)], fill=(255, 255, 255))

font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 31 * SCALE)
label = "TAMIYA"
left, top, right, bottom = draw.textbbox((0, 0), label, font=font)
draw.text(((SIZE * SCALE - (right - left)) / 2, 138 * SCALE), label,
          font=font, fill=(238, 241, 247), stroke_width=0)

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
