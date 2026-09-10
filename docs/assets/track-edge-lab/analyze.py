"""Summarize paired nearest / edge-aware scene upscale measurements."""
import csv
import json
import math
from pathlib import Path
import re
import statistics

root = Path(__file__).resolve().parent
log = (root / "ab-device.log").read_text()
rows = [{k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)} for line in log.splitlines()
        if "[EdgeAB]" in line and " mode=" in line]
assert len(rows) == 384
assert {(r["car"], r["track"], r["frame"], r["mode"]) for r in rows} == {
    (c, t, f, m) for c in range(8) for t in range(2) for f in range(1, 13) for m in range(2)}
assert "[EdgeAB] END mismatches=0 changed=208 copy_mismatches=0" in log
for row in rows:
    row["total"] = row["draw"] + row["present"]
with (root / "frames.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

def summarize(group):
    totals = sorted(r["total"] for r in group)
    return {"n": len(group), "mean_us": {k: statistics.mean(r[k] for r in group)
            for k in ("total", "draw", "present", "upscale", "player", "opponents")},
            "p95_us": totals[math.ceil(len(totals)*.95)-1], "max_us": max(totals)}

result = {"modes": [summarize([r for r in rows if r["mode"] == mode]) for mode in range(2)],
          "groups": [], "restore_crc_checks": 208, "restore_mismatches": 0,
          "changed_frames": 208, "filtered_copy_checks": 16, "filtered_copy_mismatches": 0}
for car in range(8):
    for track in range(2):
        modes = [summarize([r for r in rows if r["car"] == car and r["track"] == track
                            and r["mode"] == mode]) for mode in range(2)]
        result["groups"].append({"car": car, "track": track, "modes": modes})
(root / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result["modes"], indent=2))
