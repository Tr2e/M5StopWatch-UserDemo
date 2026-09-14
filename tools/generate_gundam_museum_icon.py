#!/usr/bin/env python3
"""Encode the extracted head artwork as the launcher's 200px RGB565 icon.

The RGBA source preserves transparency; the existing launcher uses a black
RGB565 tile. This script only sizes/encodes that source, never redraws it.
"""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
out = root / 'main/assets/images'
with Image.open(out / 'source/gundam_museum_head.png') as source:
    rgba = source.convert('RGBA').resize((200, 200), Image.Resampling.LANCZOS)
rgba.save(out / 'icon_gundam_museum.png')
tile = Image.new('RGBA', rgba.size, (0, 0, 0, 255))
tile.alpha_composite(rgba)
pixels = bytearray()
for r, g, b in tile.convert('RGB').getdata():
    value = ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)
    pixels.extend((value & 255, value >> 8))
rows = ['    ' + ','.join(str(v) for v in pixels[i:i + 32]) + ','
        for i in range(0, len(pixels), 32)]
source = '#include <lvgl.h>\n\n'
source += 'const LV_ATTRIBUTE_MEM_ALIGN uint8_t icon_gundam_museum_map[] = {\n'
source += '\n'.join(rows) + '\n};\n'
source += '''const lv_image_dsc_t icon_gundam_museum = {
    .header.cf=LV_COLOR_FORMAT_RGB565,
    .header.magic=LV_IMAGE_HEADER_MAGIC,
    .header.w=200,
    .header.h=200,
    .data_size=sizeof(icon_gundam_museum_map),
    .data=icon_gundam_museum_map,
};
'''
(out / 'icon_gundam_museum.c').write_text(source)
