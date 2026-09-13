#!/usr/bin/env python3
"""Compare actual head renders; only the private reference photo is resized."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw

p = argparse.ArgumentParser()
p.add_argument('references', type=Path)
p.add_argument('baseline', type=Path)
p.add_argument('current', type=Path)
p.add_argument('output', type=Path, help='Private output directory for reference composites')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
refpath = a.references / 'rx78-r5-head-022.jpg'
# Manual frontal registration: approximate helmet width/centre and crown
# height. This is a uniform fit, not a lens calibration or similarity score.
with Image.open(refpath) as im:
    fit = im.crop((0, 0, 500, 349)).resize((750, 524), Image.Resampling.LANCZOS)
reference = Image.new('RGB', (640, 640), (50, 50, 50))
reference.paste(fit, (-57, 42))
with Image.open(a.baseline/'study-face-front.ppm') as im:
    previous = im.copy()
with Image.open(a.current/'study-face-front.ppm') as im:
    current = im.copy()
assert previous.size == current.size == (640, 640)
for name, panels in [('face-frontal-comparison', [(reference, 'HGUC 191 / actual kit / uniform fit'),
                      (previous, 'R4 / previous C++ render'), (current, 'R5 / current C++ render')]),
                     ('face-reference', [(reference, 'HGUC 191 / actual kit / uniform fit'),
                      (current, 'R5 / current C++ render')]),
                     ('face-before-after', [(previous, 'R4 / previous C++ render'),
                      (current, 'R5 / current C++ render')])]:
    sheet = Image.new('RGB', (640*len(panels), 700), (16, 18, 25))
    draw = ImageDraw.Draw(sheet)
    for i, (im, label) in enumerate(panels):
        draw.text((i*640+16, 12), label, fill='white')
        sheet.paste(im, (i*640, 40))
    sheet.save(a.output/f'{name}.png')
ledger = {'kit': '2015 HGUC 191 RX-78-2 Revive',
          'source_page': 'https://schizophonic9.com/re3/hguc_rx78.html',
          'reference_use': 'Unpainted close-ups for proportions; official product photos for colours.',
          'fit': {'crop_xyxy': [0, 0, 500, 349], 'resized_pixels': [750, 524],
                  'offset_xy': [-57, 42], 'model_pixels': [640, 640],
                  'limitation': 'Approximate width registration; camera/lens and pose are not calibrated.'},
          'sources': [], 'models': []}
for n in ['021', '022', '004']:
    path = a.references/f'rx78-r5-head-{n}.jpg'
    with Image.open(path) as im:
        size = list(im.size)
    ledger['sources'].append({'url': f'https://schizophonic9.com/re3/hguc_rx78{n}.jpg',
        'pixels': size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
for folder, revision in [(a.baseline, '1b2fc26'), (a.current, 'R5')]:
    for name in ['face-front', 'face-quarter', 'face-side']:
        path = folder/f'study-{name}.ppm'
        ledger['models'].append({'revision': revision, 'view': name,
                                'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
(a.output/'face-reference-ledger.json').write_text(json.dumps(ledger, indent=2)+'\n')
print(a.output)
