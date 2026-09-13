#!/usr/bin/env python3
"""Package unscaled production-render frames; no retouching of model pixels."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw

p=argparse.ArgumentParser();p.add_argument('frames',type=Path);p.add_argument('output',type=Path)
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
tour=[]
for i in range(48):
    with Image.open(a.frames/f'tour-{i}.ppm') as im:tour.append(im.convert('RGB'))
tour[0].save(a.output/'rx78-orbit.gif',save_all=True,append_images=tour[1:],duration=420,loop=0)
manifest={'schema':1,'identity_view':'equipped','models':[{'id':'rx78-2-hguc-191','required_views':names,
    'frames':[{'view':n,'file':f'rx78-{n}.png'} for n in names]}]}
(a.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(a.output/'host-results.txt').write_text((a.frames/'results.txt').read_text())
print(a.output)
