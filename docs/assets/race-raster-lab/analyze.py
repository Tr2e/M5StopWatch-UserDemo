"""Summarize the last complete paired RasterAB run; no third-party modules."""
import csv
import json
import math
from pathlib import Path
import re
import statistics

root = Path(__file__).resolve().parent
log = (root / "ab-device.log").read_text(errors="replace")
begin = log.rfind("[RasterAB] BEGIN")
end = log.find("[RasterAB] END", begin)
assert begin >= 0 and end >= 0
run = log[begin:end + 200]
rows = []
for line in run.splitlines():
    if "[RasterAB]" in line and " mode=" in line:
        row = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
        row["total"] = row["draw"] + row["present"]
        rows.append(row)

expected = {(car, track, frame, mode) for car in range(8) for track in range(3)
            for frame in range(1, 25) for mode in range(2)}
actual = {(row["car"], row["track"], row["frame"], row["mode"]) for row in rows}
assert len(rows) == 1152 and actual == expected
assert "[RasterAB] END mismatches=0" in run
for car, track, frame in {(r["car"], r["track"], r["frame"]) for r in rows}:
    pair = [r for r in rows if (r["car"], r["track"], r["frame"]) == (car, track, frame)]
    assert len(pair) == 2 and pair[0]["crc"] == pair[1]["crc"]

fields = list(rows[0])
with (root / "frames.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=fields)
    writer.writeheader()
    writer.writerows(rows)

metrics = ("total", "draw", "present", "track_us", "prepare", "raster", "blit",
           "player", "opponents", "upscale", "hud")

def summarize(group):
    totals = sorted(r["total"] for r in group)
    return {
        "frames": len(group),
        "mean_us": {key: round(statistics.mean(r[key] for r in group), 2) for key in metrics},
        "median_us": {key: round(statistics.median(r[key] for r in group), 2) for key in metrics},
        "p95_total_us": totals[math.ceil(len(totals) * .95) - 1],
    }

def percent(before, after):
    return round((before - after) * 100 / before, 3)

modes = [summarize([r for r in rows if r["mode"] == mode]) for mode in range(2)]
groups = []
for car in range(8):
    for track in range(3):
        group_modes = [summarize([r for r in rows if r["car"] == car and
                                  r["track"] == track and r["mode"] == mode])
                       for mode in range(2)]
        groups.append({
            "car": car,
            "track": track,
            "raster_saved_us": round(group_modes[0]["mean_us"]["raster"] -
                                     group_modes[1]["mean_us"]["raster"], 2),
            "raster_saved_percent": percent(group_modes[0]["mean_us"]["raster"],
                                             group_modes[1]["mean_us"]["raster"]),
            "modes": group_modes,
        })

result = {
    "paired_frames": 576,
    "crc_mismatches": 0,
    "modes": modes,
    "raster_saved_us": round(modes[0]["mean_us"]["raster"] - modes[1]["mean_us"]["raster"], 2),
    "raster_saved_percent": percent(modes[0]["mean_us"]["raster"], modes[1]["mean_us"]["raster"]),
    "draw_saved_us": round(modes[0]["mean_us"]["draw"] - modes[1]["mean_us"]["draw"], 2),
    "draw_saved_percent": percent(modes[0]["mean_us"]["draw"], modes[1]["mean_us"]["draw"]),
    "groups_faster": sum(group["raster_saved_us"] > 0 for group in groups),
    "groups": groups,
}
(root / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({key: value for key, value in result.items() if key != "groups"}, indent=2))
