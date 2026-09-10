#!/usr/bin/env python3
"""Summarize the fixed-pose ESP32 experiment without mixing firmware runs."""
from pathlib import Path
import csv,json,math,re,statistics
root=Path(__file__).resolve().parent
log=(root/'ab-device.log').read_text(errors='replace')
rows=[];preview=[]
for line in log.splitlines():
    if '[SpiralAB] car=' in line:
        d={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
        d['total']=d['draw']+d['present'];rows.append(d)
    if '[SpiralPreview] track=' in line:
        d={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
        d['total']=d['draw']+d['present'];preview.append(d)
assert len(rows)==576 and len(preview)==72,(len(rows),len(preview))
assert len({(r['car'],r['track'],r['frame'],r['mode']) for r in rows})==576
assert '[SpiralAB] END mismatches=0 ' in log
assert all(0<r['faces']<=1008 for r in rows)
def stats(group):
    total=sorted(r['total'] for r in group)
    return {'n':len(group),'mean_us':statistics.mean(total),'p95_us':total[math.ceil(len(total)*.95)-1],
            'max_us':max(total),'draw_us':statistics.mean(r['draw'] for r in group),
            'present_us':statistics.mean(r['present'] for r in group),
            **({'road_us':statistics.mean(r['road'] for r in group),'max_faces':max(r['faces'] for r in group)} if 'road' in group[0] else {})}
summary={'race':{f'track{track}_cull{mode}':stats([r for r in rows if r['track']==track and r['mode']==mode]) for track in range(3) for mode in range(2)},
         'preview':{f'track{track}_clean{mode}':stats([r for r in preview if r['track']==track and r['mode']==mode]) for track in range(3) for mode in range(2)},
         'paired_crc_checks':312,'mismatches':0}
a=summary['race']['track2_cull0'];b=summary['race']['track2_cull1']
summary['new_track_cull_mean_change_pct']=(b['mean_us']/a['mean_us']-1)*100
control=stats([r for r in rows if r['track']<2 and r['mode']==1])
summary['old_track_control']=control
summary['new_vs_old_control_mean_change_pct']=(b['mean_us']/control['mean_us']-1)*100
with (root/'frames.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=rows[0],lineterminator='\n');w.writeheader();w.writerows(rows)
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
