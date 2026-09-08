"""Convert the host production-renderer pixel captures into a review sheet.

Requires Pillow. This only presents actual renderer output; it does not redraw
the scene, and is not an AMOLED/driver/timing emulator.
"""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument("frames", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--columns", type=int, choices=range(1,5), default=3)
parser.add_argument("--names", nargs="+", default=[
    "car-0", "car-2", "track", "race-0", "race-4", "race-7",
])
args = parser.parse_args()
columns = args.columns
sheet = Image.new("RGB", (466 * columns, 490 * ((len(args.names) + columns - 1) // columns)), "#191b1d")
labels = ImageDraw.Draw(sheet)
for index, name in enumerate(args.names):
    x, y = index % columns * 466, index // columns * 490
    with Image.open(args.frames / f"{name}.ppm") as frame:
        sheet.paste(frame, (x, y + 24))
    labels.text((x + 18, y + 5), name, fill="white")
args.output.parent.mkdir(parents=True, exist_ok=True)
sheet.save(args.output)
