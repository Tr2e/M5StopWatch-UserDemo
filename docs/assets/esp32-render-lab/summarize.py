"""Usage: python3 summarize.py device.log; outputs CSV and JSON beside the log."""
from pathlib import Path
import re, csv, json, sys
source=Path(sys.argv[1]); log=source.read_text()
assert re.search(r"\[RaceAB\] END copy_mismatches=0",log), "Missing successful END"
rows=[]
for line in log.splitlines():
    if "[RaceAB]" in line and "variant=" in line:
        rows.append({k:int(v) for k,v in re.findall(r"(\w+)=(\d+)",line)})
assert len(rows)==64, len(rows)
assert len({(r['car'],r['track'],r['variant']) for r in rows})==64
with source.with_suffix('.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
summary={}
for variant in range(4):
    group=[r for r in rows if r['variant']==variant]
    fields=['draw_us','present_us','road_us','player_us','opponents_us','upscale_us','hud_us']
    mean={k.replace('_us','_ms'):sum(r[k]*r['n'] for r in group)/sum(r['n'] for r in group)/1000 for k in fields}
    mean['total_ms']=mean['draw_ms']+mean['present_ms']
    summary[str(variant)]=mean
baseline=summary['0']['total_ms']
for variant in range(1,4):
    summary[str(variant)]['reduction_pct']=(1-summary[str(variant)]['total_ms']/baseline)*100
# Each group's P95 remains in CSV; never average them to invent a global P95.
end=[line for line in log.splitlines() if '[RaceAB] END' in line][-1]
summary['validation']={k:int(v) for k,v in re.findall(r"(\w+)=(\d+)",end)}
source.with_suffix('.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
