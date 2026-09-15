#!/usr/bin/env python3
"""Unscaled production-image sheets for visual review, including all orbit frames."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw

p = argparse.ArgumentParser()
p.add_argument('frames', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--pattern', default='tour-*.ppm')
p.add_argument('--columns', type=int, default=3)
p.add_argument('--rows', type=int, default=4)
a = p.parse_args()
files = sorted(a.frames.glob(a.pattern), key=lambda f: int(f.stem.split('-')[-1]) if f.stem.split('-')[-1].isdigit() else f.stem)
if not files:
    raise ValueError('No source frames')
a.output.mkdir(parents=True, exist_ok=True)
page_size = a.columns * a.rows
for offset in range(0, len(files), page_size):
    batch = files[offset:offset+page_size]
    images = [Image.open(f).convert('RGB') for f in batch]
    w, h = images[0].size
    if any(im.size != (w, h) for im in images):
        raise ValueError('All source frames must have matching native dimensions')
    sheet = Image.new('RGB', (a.columns*w, a.rows*(h+24)), (12,16,24))
    d = ImageDraw.Draw(sheet)
    for i, (f, im) in enumerate(zip(batch, images)):
        x, y = (i % a.columns)*w, (i // a.columns)*(h+24)
        d.text((x+8,y+4), f.name+' / unscaled production', fill=(230,230,230))
        sheet.paste(im, (x,y+24))
    sheet.save(a.output/f'contact-{offset//page_size}.png')
print(f'{len(files)} original-size frames, {(len(files)+page_size-1)//page_size} sheets')
