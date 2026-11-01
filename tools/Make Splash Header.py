"""
FPV Channel Scanner - Make Splash Header.py
Revision: 46

Converts the splash picture into "Splash Image.h" for the sketch.

    python "Make Splash Header.py" Splash.PNG "Splash Image.h"

The picture must be 320 x 320. It is reduced to a 256-colour palette (no
dithering) and stored as one palette index per pixel: about 100 KB of flash
instead of 200 KB for RGB565. Needs Pillow (pip install pillow).
"""
import sys
from PIL import Image

def main(src, dst):
    im = Image.open(src).convert("RGB")
    if im.size != (320, 320):
        sys.exit(f"{src} is {im.size[0]} x {im.size[1]}; it must be 320 x 320")
    q = im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:256 * 3]
    pal += [0] * (256 * 3 - len(pal))
    idx = q.tobytes()                      # one byte per pixel, row by row

    out = []
    out.append("/*")
    out.append(" * FPV Channel Scanner - Splash Image.h")
    out.append(" * Revision: 46")
    out.append(" *")
    out.append(f" * Generated from {src} by tools/Make Splash Header.py - do not edit.")
    out.append(" * 320 x 320, 256-colour palette (R, G, B), one index byte per pixel.")
    out.append(" */")
    out.append("")
    out.append("#pragma once")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("#define SPLASH_W 320")
    out.append("#define SPLASH_H 320")
    out.append("")
    out.append("static const uint8_t SPLASH_PALETTE[256][3] = {")
    for i in range(256):
        r, g, b = pal[i * 3:i * 3 + 3]
        out.append(f"  {{ {r:3d}, {g:3d}, {b:3d} }},")
    out.append("};")
    out.append("")
    out.append("static const uint8_t SPLASH_PIXELS[SPLASH_W * SPLASH_H] = {")
    for row in range(0, len(idx), 32):
        out.append("  " + ",".join(str(v) for v in idx[row:row + 32]) + ",")
    out.append("};")
    with open(dst, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print(f"wrote {dst}: {len(idx)} pixels, 256 colours")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit('usage: python "Make Splash Header.py" Splash.PNG "Splash Image.h"')
    main(sys.argv[1], sys.argv[2])
