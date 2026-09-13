#!/usr/bin/env python3
"""Private reference comparison; reference photos are not game assets.

Uses separately obtained HGUC 191 Dalong photographs. Only reference photos
are uniformly fitted; structural C++ frames stay at their original 640 x 640.
This does not measure similarity or reconstruct hidden injection tooling.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

p = argparse.ArgumentParser()
p.add_argument('references', type=Path)
p.add_argument('studies', type=Path)
p.add_argument('output', type=Path, help='Private PNG path outside distributable assets')
a = p.parse_args()
rows = [('j_head', 'head', 'B19/B20/B25 - head shell and mask'),
        ('j_arm', 'arm', 'B13/B14/B15/B16 - shoulder and forearm'),
        ('j_leg', 'leg-side', 'B24/B27/B28/B30/B31 - shin and ankle'),
        ('j_body', 'chest', 'A6/A14 - chest cover and collar')]
sheet = Image.new('RGB', (1280, 700 * len(rows)), (16, 18, 25))
draw = ImageDraw.Draw(sheet)
ledger = {'kit': 'HGUC 191 RX-78-2 (2015 Revive)',
          'parts_authority': 'https://manual.bandai-hobby.net/pdf/1004.pdf',
          'photographer_page': 'https://www.dalong.net/reviews/hg/h191/h191_i_e.htm',
          'method': 'Unassembled reference / assembled authored geometry. No pose matching or similarity score.',
          'sources': [], 'comparisons': []}
for name in ['r01', 'r02', 'r03', 'j_head', 'j_arm', 'j_leg', 'j_body', 'j_gun']:
    path = a.references / f'rx78-dalong-{name}.jpg'
    with Image.open(path) as im:
        size = list(im.size)
    ledger['sources'].append({'id': name,
        'url': f'https://www.dalong.net/reviews/hg/h191/p/h191_{name}.jpg',
        'pixels': size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
for row, (reference, study, title) in enumerate(rows):
    y = row * 700
    draw.text((16, y + 8), f'HGUC 191 / DALONG PARTS PHOTO / {title}', fill=(230, 233, 240))
    draw.text((656, y + 8), 'CURRENT ASSEMBLED MODEL / actual C++ raster / 640 x 640', fill=(230, 233, 240))
    with Image.open(a.references / f'rx78-dalong-{reference}.jpg') as im:
        fit = ImageOps.contain(im.convert('RGB'), (640, 640), Image.Resampling.LANCZOS)
        sheet.paste(fit, ((640-fit.width)//2, y+36+(640-fit.height)//2))
    path = a.studies / f'study-{study}.ppm'
    with Image.open(path) as im:
        if im.size != (640, 640):
            raise ValueError('Expected an unscaled 640 x 640 structural frame')
        sheet.paste(im, (640, y+36))
    ledger['comparisons'].append({'reference': reference, 'study': study,
        'study_sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
a.output.parent.mkdir(parents=True, exist_ok=True)
sheet.save(a.output)
a.output.with_suffix('.json').write_text(json.dumps(ledger, indent=2)+'\n')
print(a.output)
