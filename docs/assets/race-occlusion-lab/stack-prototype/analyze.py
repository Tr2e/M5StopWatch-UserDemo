"""Summarize paired, per-frame StopWatch measurements; no third-party modules."""
import csv
import json
import math
from pathlib import Path
import re
import statistics

root = Path(__file__).resolve().parent
log = (root / "ab-device.log").read_text()
rows = []
for line in log.splitlines():
    if "[OcclusionAB]" in line and " mode=" in line:
        row = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)}
        row["total"] = row["draw"] + row["present"]
        rows.append(row)
assert len(rows) == 768
assert {(r["car"], r["track"], r["frame"], r["mode"]) for r in rows} == {
    (car, track, frame, mode) for car in range(8) for track in range(2)
    for frame in range(1, 25) for mode in range(2)
}
assert "[OcclusionAB] END mismatches=0" in log
with (root / "frames.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

def summarize(group):
    totals = sorted(r["total"] for r in group)
    means = {key: statistics.mean(r[key] for r in group) for key in
             ("total", "draw", "present", "prepare", "raster", "blit", "player", "opponents")}
    return {"frames": len(group), "mean_us": means,
            "p95_us": totals[math.ceil(len(totals) * .95) - 1], "max_us": max(totals),
            "candidate_row_pairs_before": sum(r["before"] for r in group),
            "candidate_row_pairs_after": sum(r["after"] for r in group)}

result = {"modes": [summarize([r for r in rows if r["mode"] == mode]) for mode in range(2)],
          "groups": [], "crc_pairs": 400, "crc_mismatches": 0}
for car in range(8):
    for track in range(2):
        modes = [summarize([r for r in rows if r["car"] == car and r["track"] == track
                            and r["mode"] == mode]) for mode in range(2)]
        result["groups"].append({"car": car, "track": track, "modes": modes,
                                 "saved_us": modes[0]["mean_us"]["total"] - modes[1]["mean_us"]["total"]})
result["groups_faster"] = sum(g["saved_us"] > 0 for g in result["groups"])
(root / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({k: v for k, v in result.items() if k != "groups"}, indent=2))
