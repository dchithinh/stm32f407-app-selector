"""Generate Bsp/lcd/font8x8.c from dhepper/font8x8_basic.h (ASCII 32..126)."""
import re
import urllib.request

URL = "https://raw.githubusercontent.com/dhepper/font8x8/master/font8x8_basic.h"
data = urllib.request.urlopen(URL, timeout=15).read().decode()

start = data.find("font8x8_basic")
if start < 0:
    raise SystemExit("font8x8_basic not found")
chunk = data[start:]
sub = chunk[chunk.find("{", chunk.find("=")) + 1 :]
hex_bytes = re.findall(r"0x[0-9A-Fa-f]{2}", sub)
glyphs = []
for i in range(0, min(len(hex_bytes), 128 * 8), 8):
    row = [int(h, 16) for h in hex_bytes[i : i + 8]]
    if len(row) == 8:
        glyphs.append(row)
if len(glyphs) < 127:
    raise SystemExit(f"expected 128 glyphs, got {len(glyphs)}")

subset = glyphs[32:127]
if len(subset) != 95:
    raise SystemExit(f"expected 95 glyphs, got {len(subset)}")

out = r"Bsp/lcd/font8x8.c"
with open(out, "w", newline="\n", encoding="utf-8") as f:
    f.write('#include "font8x8.h"\n\n')
    f.write("const uint8_t font8x8_basic[95][8] = {\n")
    for i, g in enumerate(subset):
        line = ", ".join(f"0x{b:02X}" for b in g)
        f.write(f"  {{ {line} }}{',' if i < 94 else ''}\n")
    f.write("};\n")

b = subset[ord("B") - 32]
print("ok", out, "B=", [hex(x) for x in b])
