#!/usr/bin/env python3
"""Convert the screenshots written by headless test runs (.ppm) to .png."""
import sys
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1] if len(sys.argv) > 1 else "build/test")
count = 0
for ppm in sorted(root.rglob("*.ppm")):
    Image.open(ppm).save(ppm.with_suffix(".png"))
    ppm.unlink()
    count += 1
print(f"{count} screenshots in {root}")
