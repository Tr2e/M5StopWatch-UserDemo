#!/usr/bin/env python3
"""Compose registered private references and unmodified production render pairs.

The JSON describes rows with reference/previous/current file paths, a reference
crop [l,t,r,b], uniform scale, offset [x,y], and label. No image is downloaded;
only the reference may be cropped/resized. Output may contain third-party photos
and belongs in a private directory. Registration is not a similarity score.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw


def compose(spec_path, output):
    spec = json.loads(spec_path.read_text())
    output.mkdir(parents=True, exist_ok=False)
    comparisons, pairs, ledger = [], [], []
    for row in spec['rows']:
        paths = {k: (spec_path.parent / row[k]).resolve()
                 for k in ('reference', 'previous', 'current')}
        with Image.open(paths['previous']) as im:
            previous = im.convert('RGB')
        with Image.open(paths['current']) as im:
            current = im.convert('RGB')
        if previous.size != current.size:
            raise ValueError('Old/new render sizes must match; model images are never resized')
        width, height = current.size
        with Image.open(paths['reference']) as im:
            crop = im.convert('RGB').crop(row['crop'])
        scale = row['scale']
        if scale <= 0:
            raise ValueError('Reference scale must be positive')
        fit = crop.resize((round(crop.width*scale), round(crop.height*scale)), Image.Resampling.LANCZOS)
        reference = Image.new('RGB', current.size, (35, 35, 35))
        reference.paste(fit, tuple(row['offset']))
        for images, labels, collection in [
            ([reference, previous, current], ['REFERENCE / '+row['label'], 'PREVIOUS', 'CURRENT'], comparisons),
            ([previous, current], ['PREVIOUS / '+row['label'], 'CURRENT'], pairs),
        ]:
            sheet = Image.new('RGB', (width*len(images), height+32), (16, 18, 25))
            draw = ImageDraw.Draw(sheet)
            for i, (im, label) in enumerate(zip(images, labels)):
                draw.text((i*width+12, 10), label, fill='white')
                sheet.paste(im, (i*width, 32))
            collection.append(sheet)
        ledger.append({**row, 'files': {k: {'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                                      for k, p in paths.items()}})
    for rows, name in [(comparisons, 'reference-comparison'), (pairs, 'before-after')]:
        sheet = Image.new('RGB', (max(im.width for im in rows), sum(im.height for im in rows)), (16, 18, 25))
        y = 0
        for im in rows:
            sheet.paste(im, (0, y))
            y += im.height
        sheet.save(output / (name+'.png'))
    (output/'registration.json').write_text(json.dumps({
        'limitation': 'Uniform reference registration; camera/pose differences remain. No similarity score.',
        'rows': ledger}, indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('spec', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    compose(args.spec.resolve(), args.output)
