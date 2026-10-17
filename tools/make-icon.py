"""Write src/tip/fkeyime.ico: a white "F" on a dark blue rounded square, 16/24/32/48 px.

No dependencies (writes 32-bit BMP icon images by hand), so the icon can be regenerated anywhere:
    python tools/make-icon.py
"""
import pathlib
import struct

# 5x7 "F", drawn on a 7x9 grid with a 1-cell margin.
GLYPH = ["11111", "10000", "10000", "11110", "10000", "10000", "10000"]
BACKGROUND = (0x2B, 0x4A, 0x8C, 0xFF)  # RGBA
FOREGROUND = (0xFF, 0xFF, 0xFF, 0xFF)


def pixel(size, x, y):
    radius = size // 6
    # Rounded corners: transparent outside the corner circles.
    for cx, cy in ((radius, radius), (size - 1 - radius, radius), (radius, size - 1 - radius),
                   (size - 1 - radius, size - 1 - radius)):
        if (x < radius or x > size - 1 - radius) and (y < radius or y > size - 1 - radius):
            if (x - cx) ** 2 + (y - cy) ** 2 > radius * radius and abs(x - cx) <= radius and \
                    abs(y - cy) <= radius:
                return (0, 0, 0, 0)
    gx = (x * 7) // size - 1
    gy = (y * 9) // size - 1
    if 0 <= gy < 7 and 0 <= gx < 5 and GLYPH[gy][gx] == "1":
        return FOREGROUND
    return BACKGROUND


def image(size):
    rows = []
    for y in range(size - 1, -1, -1):  # BMP rows are bottom-up
        row = b""
        for x in range(size):
            r, g, b, a = pixel(size, x, y)
            row += bytes((b, g, r, a))
        rows.append(row)
    mask_row = ((size + 31) // 32) * 4
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    return header + b"".join(rows) + b"\0" * (mask_row * size)


def main():
    sizes = [16, 24, 32, 48]
    images = [image(s) for s in sizes]
    out = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    for size, data in zip(sizes, images):
        out += struct.pack("<BBBBHHII", size, size, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    out += b"".join(images)
    path = pathlib.Path(__file__).resolve().parent.parent / "src" / "tip" / "fkeyime.ico"
    path.write_bytes(out)
    print(f"wrote {path} ({len(out)} bytes)")


if __name__ == "__main__":
    main()
