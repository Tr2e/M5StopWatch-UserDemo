"""Rebuild paired selection-mode statistics from the archived device log."""
import csv
import json
import math
from pathlib import Path
import re
import statistics

root = Path(__file__).resolve().parent
log = (root / "ab-device.log").read_text()
rows = [{key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
        for line in log.splitlines() if "[SelectionAB]" in line and " mode=" in line]
assert len(rows) == 456
assert {(r["car"], r["frame"], r["mode"]) for r in rows} == {
    (car, frame, mode) for car in range(8) for frame in range(1, 20) for mode in range(3)}
assert "[SelectionAB] END native_checks=216 mismatches=0" in log
for row in rows:
    row["total"] = row["draw"] + row["present"]
    row["phase"] = "drag" if row["frame"] <= 5 else "transition" if row["frame"] in (6, 7, 8, 9, 11, 12, 13, 14) else "native"
with (root / "frames.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

def summarize(group):
    total = sorted(r["total"] for r in group)
    return {"n": len(group), "mean_us": {k: statistics.mean(r[k] for r in group)
            for k in ("draw", "present", "total")},
            "p95_us": total[math.ceil(len(total) * .95) - 1], "max_us": max(total)}

result = {"phases": {}, "cars": [], "native_crc_checks": 216, "native_crc_mismatches": 0}
for phase in ("all", "drag", "transition", "native"):
    result["phases"][phase] = [summarize([r for r in rows if r["mode"] == mode and
                                      (phase == "all" or r["phase"] == phase)]) for mode in range(3)]
for car in range(8):
    result["cars"].append([summarize([r for r in rows if r["car"] == car and r["mode"] == mode])
                           for mode in range(3)])
(root / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result["phases"], indent=2))
