#!/usr/bin/env python3
"""Compare official local references with unscaled C++ structural studies.

No fetching or bundling of official photographs. Keep references in a private
local directory. Only reference photographs are uniformly resized; rendered
model pixels are never modified. Output is a review aid, not a similarity score.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw

p=argparse.ArgumentParser()
p.add_argument('references',type=Path)
p.add_argument('studies',type=Path)
p.add_argument('output',type=Path)
a=p.parse_args()
board=Image.new('RGB',(1280,2040),(12,16,24));d=ImageDraw.Draw(board)
ledger=[]
for row,(name,index) in enumerate([('standing',1),('salute',2),('saber',3)]):
    reference=a.references/f'rx78-official-{index}.jpg'
    study=a.studies/f'study-{name}.png'
    if not study.exists():study=a.studies/f'study-{name}.ppm'
    y=row*680
    with Image.open(reference) as photo,Image.open(study) as model:
        if model.size!=(640,640):raise ValueError('Structural studies must be native 640 x 640')
        factor=min(640/photo.width,640/photo.height)
        resized=photo.resize((round(photo.width*factor),round(photo.height*factor)),Image.Resampling.LANCZOS)
        board.paste(resized,((640-resized.width)//2,y+30+(640-resized.height)//2))
        board.paste(model,(640,y+30))
        d.text((12,y+8),f'OFFICIAL HGUC 191 / photo {index} / uniformly resized reference',fill='white')
        d.text((652,y+8),'SAME C++ ASSET + RASTER / reference articulation / native 640px study',fill='white')
        ledger.append({'view':name,'reference_url':f'https://bandai-a.akamaihd.net/bc/img/model/b/1000098610_{index}.jpg',
            'reference_sha256':hashlib.sha256(reference.read_bytes()).hexdigest(),
            'reference_original_size':photo.size,'reference_resize_factor':factor,
            'study_sha256':hashlib.sha256(study.read_bytes()).hexdigest(),'study_size':model.size})
a.output.parent.mkdir(parents=True,exist_ok=True)
board.save(a.output)
a.output.with_suffix('.json').write_text(json.dumps({'comparison':'visual review; not a pixel-error metric',
    'product_page':'https://www.bandaispirits.co.jp/products/search/detail.php?grp_id=5325&prd_id=4543112967169000',
    'views':ledger},indent=2)+'\n')
print(a.output)
