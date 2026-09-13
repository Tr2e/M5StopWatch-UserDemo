#!/usr/bin/env python3
"""Package unscaled production-render frames; no retouching of model pixels."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw

p=argparse.ArgumentParser();p.add_argument('frames',type=Path);p.add_argument('output',type=Path)
p.add_argument('--baseline',type=Path,help='Optional previous production frames; compare without rescaling.')
p.add_argument('--studies',type=Path,help='Optional larger structural studies from the same asset/raster.')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
names=['equipped','front','rear','side','other-side','top','unarmed','drag','gray','head']
for n in names:
    with Image.open(a.frames/f'rx78-{n}.ppm') as im:
        im.save(a.output/f'rx78-{n}.png')
selected=['equipped','rear','head']
sheet=Image.new('RGB',(468*3,490),(12,16,24));d=ImageDraw.Draw(sheet)
for i,n in enumerate(selected):
    d.text((i*468+10,5),f'RX-78-2 / {n.upper()} / production C++',fill=(225,230,235))
    with Image.open(a.output/f'rx78-{n}.png') as im:sheet.paste(im,(i*468,24))
sheet.save(a.output/'rx78-review.png')
if a.baseline:
    comparison=Image.new('RGB',(936,1012),(12,16,24));d=ImageDraw.Draw(comparison)
    for row,n in enumerate(['equipped','head']):
        for col,(folder,title) in enumerate([(a.baseline,'PREVIOUS'),(a.output,'CURRENT')]):
            d.text((col*468+14,row*506+10),f'{title} / {n} / native production C++',fill=(225,230,235))
            with Image.open(folder/f'rx78-{n}.png') as im:
                if im.size!=(468,466):raise ValueError('Comparison requires native 468 x 466 frames')
                comparison.paste(im,(col*468,row*506+30))
    comparison.save(a.output/'rx78-before-after.png')
tour=[]
for i in range(48):
    with Image.open(a.frames/f'tour-{i}.ppm') as im:tour.append(im.convert('RGB'))
tour[0].save(a.output/'rx78-orbit.gif',save_all=True,append_images=tour[1:],duration=420,loop=0)
manifest={'schema':1,'identity_view':'equipped','models':[{'id':'rx78-2-hguc-191','required_views':names,
    'frames':[{'view':n,'file':f'rx78-{n}.png'} for n in names]}]}
if a.studies:
    studies=sorted(a.studies.glob('study-*.ppm'))
    if not studies:raise ValueError('No structural study frames found')
    for path in studies:
        with Image.open(path) as im:im.save(a.output/f'{path.stem}.png')
        manifest['models'][0]['required_views'].append(path.stem)
        manifest['models'][0]['frames'].append({'view':path.stem,'file':f'{path.stem}.png'})
    (a.output/'study-cameras.txt').write_text((a.studies/'cameras.txt').read_text())
(a.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(a.output/'host-results.txt').write_text((a.frames/'results.txt').read_text())
print(a.output)
