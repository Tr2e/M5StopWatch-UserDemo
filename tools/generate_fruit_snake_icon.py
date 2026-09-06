#!/usr/bin/env python3
"""Regenerate the Fruit Snake LVGL launcher asset from its PNG."""

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


rgba = Image.open(IMAGE_DIR / "icon_fruit_snake.png").convert("RGBA")
if rgba.size != (200, 200):
    raise ValueError(f"launcher icon must be 200x200, got {rgba.size}")

# The launcher is black, and RGB565 has no alpha channel. Composite the
# generated sticker onto that exact backdrop so transparent corners stay clean.
background = Image.new("RGBA", rgba.size, (0, 0, 0, 255))
source = Image.alpha_composite(background, rgba).convert("RGB")

byte_values: list[str] = []
for red, green, blue in source.getdata():
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

#ifndef LV_ATTRIBUTE_IMAGE_ICON_FRUIT_SNAKE
#define LV_ATTRIBUTE_IMAGE_ICON_FRUIT_SNAKE
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ICON_FRUIT_SNAKE uint8_t icon_fruit_snake_map[] = {{
{wrapped(byte_values)}
}};

const lv_image_dsc_t icon_fruit_snake = {{
    .header.cf    = LV_COLOR_FORMAT_RGB565,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w     = 200,
    .header.h     = 200,
    .data_size    = 40000 * 2,
    .data         = icon_fruit_snake_map,
}};
'''

(IMAGE_DIR / "icon_fruit_snake.c").write_text(output)
print("Generated main/assets/images/icon_fruit_snake.c")
