"""Saves what the PixelCharm screen shows as a PNG, straight from the board's memory over USB.

    python tools/screenshot.py COM4                 saves screen.png
    python tools/screenshot.py COM4 dice.png --scale 1

Close the Serial Monitor first (only one program can use the port). Takes about 10 seconds.
The device side is sendScreenshot() in firmware/src/main.cpp: it answers "s" with "SCREEN 240 240"
and then every pixel as 2 bytes (RGB565, high byte first).
"""
import argparse
import struct
import zlib

from device_serial import open_without_reset, wait_for_line


def read_frame(port):
    with open_without_reset(port) as board:
        board.write(b"s")
        _, width, height = wait_for_line(board, "SCREEN", 5).split()
        width, height = int(width), int(height)
        size = width * height * 2
        board.timeout = 30
        data = board.read(size)
    if len(data) != size:
        raise SystemExit(f"got {len(data)} of {size} bytes, try again")
    return width, height, data


def rgb565_to_rgb(high, low):
    value = high << 8 | low
    red, green, blue = value >> 11, value >> 5 & 0x3F, value & 0x1F
    return red * 255 // 31, green * 255 // 63, blue * 255 // 31


def png_bytes(width, height, data, scale):
    """A minimal PNG writer (standard library only): 8-bit RGB, each pixel blown up to scale x scale."""
    rows = []
    for y in range(height):
        row = bytearray([0])  # each PNG row starts with its filter type, 0 = none
        for x in range(width):
            i = (y * width + x) * 2
            row += bytes(rgb565_to_rgb(data[i], data[i + 1])) * scale
        rows.append(bytes(row) * scale)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    header = struct.pack(">IIBBBBB", width * scale, height * scale, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port", help="the board's serial port, e.g. COM4 or /dev/ttyACM0")
    parser.add_argument("output", nargs="?", default="screen.png")
    parser.add_argument("--scale", type=int, default=2, help="pixel size in the PNG (default 2)")
    args = parser.parse_args()

    width, height, data = read_frame(args.port)
    with open(args.output, "wb") as image:
        image.write(png_bytes(width, height, data, args.scale))
    print(f"saved {args.output} ({width}x{height}, x{args.scale})")


if __name__ == "__main__":
    main()
