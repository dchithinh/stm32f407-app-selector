"""Generate Bsp/lcd/font8x16.c from Linux lib/fonts/font_8x16.c (ASCII 32..126)."""
import re
import urllib.request

URL = (
    "https://raw.githubusercontent.com/torvalds/linux/master/lib/fonts/font_8x16.c"
)
data = urllib.request.urlopen(URL, timeout=30).read().decode()

start = data.find("fontdata_8x16")
if start < 0:
    raise SystemExit("fontdata_8x16 not found")
chunk = data[start:]

blocks = re.findall(
    r"/\*\s*\d+\s+0x[0-9A-Fa-f]+\s+'.*?'\s*\*/\s*((?:0x[0-9A-Fa-f]{2}\s*,?\s*(?:/\*[^*]*\*/\s*)?)*)",
    chunk,
    re.S,
)
if len(blocks) < 256:
    raise SystemExit(f"expected 256 glyph blocks, got {len(blocks)}")

glyphs256 = []
for block in blocks[:256]:
    rows = re.findall(r"0x[0-9A-Fa-f]{2}", block)
    if len(rows) != 16:
        raise SystemExit(f"glyph has {len(rows)} rows, expected 16")
    glyphs256.append([int(x, 16) for x in rows])

subset = glyphs256[32:127]
if len(subset) != 95:
    raise SystemExit(f"expected 95 glyphs, got {len(subset)}")

out = r"Bsp/lcd/font8x16.c"
with open(out, "w", newline="\n", encoding="utf-8") as f:
    f.write('#include "font8x16.h"\n\n')
    f.write("const uint8_t font8x16_basic[95][16] = {\n")
    for i, g in enumerate(subset):
        line = ", ".join(f"0x{b:02X}" for b in g)
        f.write(f"  {{ {line} }}{',' if i < 94 else ''}\n")
    f.write("};\n")

a = subset[ord("A") - 32]
print("ok", out, "A rows=", [hex(b) for b in a[:8]])
