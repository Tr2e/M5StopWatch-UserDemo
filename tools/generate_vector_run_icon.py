#!/usr/bin/env python3
"""Regenerate the Vector Run LVGL launcher asset from its reviewed PNG."""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
IMAGE_DIR = ROOT / "main" / "assets" / "images"


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def wrapped(values: list[str], columns: int = 20) -> str:
    return "\n".join(
        "    " + ", ".join(values[index:index + columns]) + ","
        for index in range(0, len(values), columns)
    )


source = Image.open(IMAGE_DIR / "icon_vector_run.png").convert("RGB")
if source.size != (200, 200):
    raise ValueError(f"launcher icon must be 200x200, got {source.size}")

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

#ifndef LV_ATTRIBUTE_IMAGE_ICON_VECTOR_RUN
#define LV_ATTRIBUTE_IMAGE_ICON_VECTOR_RUN
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ICON_VECTOR_RUN uint8_t icon_vector_run_map[] = {{
{wrapped(byte_values)}
}};

const lv_image_dsc_t icon_vector_run = {{
    .header.cf    = LV_COLOR_FORMAT_RGB565,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w     = 200,
    .header.h     = 200,
    .data_size    = 40000 * 2,
    .data         = icon_vector_run_map,
}};
'''

(IMAGE_DIR / "icon_vector_run.c").write_text(output)
print("Generated main/assets/images/icon_vector_run.c")
