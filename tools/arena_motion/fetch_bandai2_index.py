#!/usr/bin/env python3
"""Fetch Bandai dataset-2 cfg labels only. Does not download BVH."""
from __future__ import annotations

from pathlib import Path
from urllib.request import urlopen

BASE = (
    "https://raw.githubusercontent.com/BandaiNamcoResearchInc/"
    "Bandai-Namco-Research-Motiondataset/master/"
    "dataset/Bandai-Namco-Research-Motiondataset-2"
)
FILES = ("cfg/content_label.txt", "cfg/style_label.txt")
OUT = Path(__file__).resolve().parent / "raw"


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    contents: list[str] = []
    styles: list[str] = []
    for rel in FILES:
        dest = OUT / Path(rel).name
        url = f"{BASE}/{rel}"
        print(f"fetch {url}")
        text = urlopen(url).read().decode("utf-8")
        dest.write_text(text)
        print(f"  -> {dest} ({dest.stat().st_size} bytes)")
        rows = [line.strip() for line in text.splitlines() if line.strip() and not line.startswith("#")]
        if "content" in rel:
            contents = rows
        else:
            styles = rows
    print("contents", len(contents), contents)
    print("styles", len(styles), styles)
    left = [c for c in contents if "left" in c.lower()]
    right = [c for c in contents if "right" in c.lower() and "raise" in c.lower()]
    print("raise/left tags", left)
    print("raise-right tags", right)
    if "raise-up-left-hand" not in contents and "raise-up-right-hand" in contents:
        print("NOTE: cfg still duplicates right-hand; Visualization listed raise-up-left-hand")


if __name__ == "__main__":
    main()
