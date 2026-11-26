#!/usr/bin/env python3
"""Turn a PETITE /SHOT video memory dump into a PNG, as a 4:3 monitor shows it.

    dump2png.py PAPER0.BIN paper0.png [--native]

16,384 bytes is CGA mode 06h (even rows at 0, odd rows at 0x2000, 1 bit a pixel);
64,000 is EGA mode 0Eh (four planes of 16,000 bytes, plane 0 first). The PNG is 640x480,
each of the 200 rows stretched 2.4 times, unless --native asks for 640x200.
Standard library only.
"""
import struct
import sys
import zlib

W, H = 640, 200
CGA16 = [
    (0x00, 0x00, 0x00), (0x00, 0x00, 0xAA), (0x00, 0xAA, 0x00), (0x00, 0xAA, 0xAA),
    (0xAA, 0x00, 0x00), (0xAA, 0x00, 0xAA), (0xAA, 0x55, 0x00), (0xAA, 0xAA, 0xAA),
    (0x55, 0x55, 0x55), (0x55, 0x55, 0xFF), (0x55, 0xFF, 0x55), (0x55, 0xFF, 0xFF),
    (0xFF, 0x55, 0x55), (0xFF, 0x55, 0xFF), (0xFF, 0xFF, 0x55), (0xFF, 0xFF, 0xFF),
]


def decode(data):
    """Rows of colour indices, and the palette."""
    rows = []
    if len(data) == 0x4000:
        for y in range(H):
            base = (0x2000 if y & 1 else 0) + (y >> 1) * 80
            rows.append([(data[base + (x >> 3)] >> (7 - (x & 7))) & 1 for x in range(W)])
        return rows, [CGA16[0], CGA16[15]]
    if len(data) == 64000:
        for y in range(H):
            row = []
            for x in range(W):
                off, bit = y * 80 + (x >> 3), 7 - (x & 7)
                c = 0
                for p in range(4):
                    c |= ((data[p * 16000 + off] >> bit) & 1) << p
                row.append(c)
            rows.append(row)
        return rows, CGA16
    raise SystemExit(f"unknown dump size {len(data)}")


def write_png(path, rows, palette, height):
    raw = bytearray()
    for y in range(height):
        src = rows[y * H // height]
        raw.append(0)
        for c in src:
            raw.extend(palette[c])

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", W, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        raise SystemExit(__doc__)
    with open(args[0], "rb") as f:
        rows, palette = decode(f.read())
    write_png(args[1], rows, palette, H if "--native" in sys.argv else 480)


if __name__ == "__main__":
    main()
