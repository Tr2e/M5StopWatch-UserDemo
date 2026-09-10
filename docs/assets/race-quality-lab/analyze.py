"""Rebuild six-mode summaries from the archived device log (stdlib only)."""
import csv
import json
from pathlib import Path
import re
import statistics

root = Path(__file__).resolve().parent
log = (root / "ab-device.log").read_text()
rows = []
cache = []
for line in log.splitlines():
    if "[QualityAB]" not in line:
        continue
    values = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)}
    if "variant=" in line:
        rows.append(values)
    elif "cache car=" in line:
        cache.append(values)
assert len(rows) == 96 and len(cache) == 16
assert {(r["car"], r["track"], r["variant"]) for r in rows} == {
    (c, t, v) for c in range(8) for t in range(2) for v in range(6)
}
assert all(r["n"] == 12 for r in rows)
assert all(r["restore_mismatches"] == 0 and r["fallback_panels"] == 0 for r in cache)
assert "[QualityAB] END restore_mismatches=0" in log
with (root / "per-group.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

summary = []
baseline = {(r["car"], r["track"]): r for r in rows if r["variant"] == 0}
for variant in range(6):
    group = [r for r in rows if r["variant"] == variant]
    means = {key: statistics.mean(r[key] for r in group) for key in
             ("draw_us", "present_us", "player_us", "opponents_us", "road_us")}
    means["total_us"] = means["draw_us"] + means["present_us"]
    paired = [r["draw_us"] + r["present_us"] - baseline[r["car"], r["track"]]["draw_us"]
              - baseline[r["car"], r["track"]]["present_us"] for r in group]
    summary.append({"variant": variant, "raster_pct": group[0]["raster_pct"],
                    "atlas": variant >= 3, "timed_frames": sum(r["n"] for r in group),
                    "means_us": means, "paired_total_delta_us": statistics.mean(paired),
                    "groups_faster_than_baseline": sum(delta < 0 for delta in paired)})

cold = [r["cold_us"] for r in cache if r["cold_us"]]
result = {"variants": summary, "cache_groups": cache,
          "cold_build_us": {"min": min(cold), "max": max(cold), "mean": statistics.mean(cold)},
          "checks": {"timed_renders": 1152, "warmup_renders": 96,
                     "restore_renders": 208, "restore_mismatches": 0},
          "limits": "Per-group averages are truncated to integer microseconds. Raw per-frame times were not logged; group P95 (n=12) equals max and cannot yield pooled P95."}
(root / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
for item in summary:
    print(item["variant"], "total_ms=", round(item["means_us"]["total_us"] / 1000, 3),
          "delta_ms=", round(item["paired_total_delta_us"] / 1000, 3),
          "faster_groups=", item["groups_faster_than_baseline"])
