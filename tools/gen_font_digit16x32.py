"""Generate Bsp/lcd/font_digit16x32.c from Linux font_ter16x32.c (digits '0'..'9')."""
import re
import urllib.request

URL = (
    "https://raw.githubusercontent.com/torvalds/linux/master/lib/fonts/font_ter16x32.c"
)
data = urllib.request.urlopen(URL, timeout=30).read().decode()

start = data.find("fontdata_ter16x32")
if start < 0:
    raise SystemExit("fontdata_ter16x32 not found")
chunk = data[start:]

glyph_map = {}
for m in re.finditer(r"/\*\s*(\d+)\s*\*/", chunk):
    code = int(m.group(1))
    end = m.end()
    nxt = re.search(r"/\*\s*\d+\s*\*/", chunk[end:])
    body = chunk[end : end + nxt.start()] if nxt else chunk[end : end + 800]
    rows = re.findall(r"0x[0-9A-Fa-f]{2}", body)
    if len(rows) >= 64:
        glyph_map[code] = [int(x, 16) for x in rows[:64]]

digits = []
for code in range(ord("0"), ord("9") + 1):
    if code not in glyph_map:
        raise SystemExit(f"missing glyph for {code!r}")
    digits.append(glyph_map[code])

out = r"Bsp/lcd/font_digit16x32.c"
with open(out, "w", newline="\n", encoding="utf-8") as f:
    f.write('#include "font_digit16x32.h"\n\n')
    f.write("const uint8_t font_digit16x32[10][64] = {\n")
    for i, g in enumerate(digits):
        f.write("  {\n")
        for row in range(32):
            b0 = g[row * 2]
            b1 = g[row * 2 + 1]
            f.write(f"    0x{b0:02X}, 0x{b1:02X},\n")
        f.write(f"  }}{',' if i < 9 else ''}\n")
    f.write("};\n")

print("ok", out)
