#!/usr/bin/env python3
"""Download Bandai-Namco-Research-Motiondataset-1 clips used by Arena retarget."""
from pathlib import Path
from urllib.request import urlopen

BASE = (
    "https://raw.githubusercontent.com/BandaiNamcoResearchInc/"
    "Bandai-Namco-Research-Motiondataset/master/"
    "dataset/Bandai-Namco-Research-Motiondataset-1"
)
FILES = [
    "data/dataset-1_kick_normal_001.bvh",
    "data/dataset-1_walk_normal_001.bvh",
    "data/dataset-1_bow_normal_001.bvh",
    "data/dataset-1_bye_normal_001.bvh",
    "data/dataset-1_byebye_normal_001.bvh",
    "data/dataset-1_respond_normal_001.bvh",
    "data/dataset-1_call_normal_001.bvh",
    "data/dataset-1_guide_normal_001.bvh",
    "data/dataset-1_punch_normal_001.bvh",
    "data/dataset-1_slash_normal_001.bvh",
    "data/dataset-1_dance-long_normal_001.bvh",
    "data/dataset-1_dance-short_normal_001.bvh",
    "data/dataset-1_run_normal_001.bvh",
    "data/dataset-1_dash_normal_001.bvh",
    "LICENSE",
]
OUT = Path(__file__).resolve().parent / "raw"


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for rel in FILES:
        dest = OUT / Path(rel).name
        if dest.exists() and dest.stat().st_size > 0:
            print(f"skip {dest.name}")
            continue
        url = f"{BASE}/{rel}"
        print(f"fetch {url}")
        dest.write_bytes(urlopen(url).read())
        print(f"  -> {dest} ({dest.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
