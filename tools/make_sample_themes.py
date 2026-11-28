"""Draws a few sample themes with code, so the repo has pictures anyone may use (MIT, like the rest of tools/).

    python tools/make_sample_themes.py          writes themes-sample/
    python tools/prepare_images.py themes-sample

Then "Upload Filesystem Image" puts them on the board. Needs Pillow: pip install pillow
"""
import math
import random
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 240
OUT = Path(__file__).resolve().parent.parent / "themes-sample"


def gradient(top, bottom):
    image = Image.new("RGB", (SIZE, SIZE))
    draw = ImageDraw.Draw(image)
    for y in range(SIZE):
        t = y / (SIZE - 1)
        draw.line((0, y, SIZE, y), fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return image, draw


def sunset():
    image, draw = gradient((255, 170, 110), (120, 60, 140))
    draw.ellipse((80, 70, 160, 150), fill=(255, 230, 150))
    for y, colour, wobble in ((150, (90, 50, 110), 18), (175, (60, 35, 85), 12), (200, (35, 22, 55), 8)):
        points = [(x, y - wobble * math.sin(x / 23 + y) - wobble) for x in range(0, SIZE + 10, 10)]
        draw.polygon([(0, SIZE), *points, (SIZE, SIZE)], fill=colour)
    return image


def night():
    image, draw = gradient((15, 20, 55), (60, 40, 110))
    rng = random.Random(7)
    for _ in range(60):
        x, y, r = rng.randrange(SIZE), rng.randrange(170), rng.choice((1, 1, 1, 2))
        draw.ellipse((x - r, y - r, x + r, y + r), fill=(255, 255, 230))
    # The crescent: a full moon with a second circle cut out of it, so the sky shows through the bite
    moon = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(moon).ellipse((140, 30, 200, 90), fill=255)
    ImageDraw.Draw(moon).ellipse((158, 22, 218, 82), fill=0)
    image.paste((250, 240, 200), mask=moon)
    draw.rectangle((0, 200, SIZE, SIZE), fill=(20, 18, 40))
    return image


def pixel_heart():
    image, draw = gradient((255, 220, 230), (230, 220, 255))
    heart = ["01100110", "11111111", "11111111", "11111111", "01111110", "00111100", "00011000"]
    cell, left, top = 20, 40, 55
    for row, line in enumerate(heart):
        for col, bit in enumerate(line):
            if bit == "1":
                x, y = left + col * cell, top + row * cell
                draw.rectangle((x, y, x + cell - 2, y + cell - 2), fill=(240, 90, 130))
    draw.rectangle((left + cell + 4, top + 4, left + cell + 14, top + 14), fill=(255, 200, 215))  # a little shine
    return image


def retro():
    image, draw = gradient((40, 10, 70), (255, 80, 160))
    for i, y in enumerate(range(40, 130, 6)):
        if i % 3 != 2:  # the striped sun of 80s posters
            half = math.sqrt(max(0, 45**2 - (y - 85) ** 2))
            draw.line((120 - half, y, 120 + half, y), fill=(255, 200, 80), width=4)
    draw.rectangle((0, 130, SIZE, SIZE), fill=(25, 5, 45))
    for x in range(-240, 480, 30):
        draw.line((120, 130, x, SIZE), fill=(255, 60, 200))
    for i in range(8):
        y = 130 + int(110 * (i / 8) ** 1.8)
        draw.line((0, y, SIZE, y), fill=(255, 60, 200))
    return image


def mascot_frames():
    """The face from the firmware, blinking and bobbing: a GIF anyone may use."""
    frames = []
    for i in range(16):
        image, draw = gradient((30, 30, 45), (12, 12, 20))
        bob = round(4 * math.sin(i / 16 * 2 * math.pi))
        eye = (90, 220, 255)
        blink = i in (6, 7)
        for side in (-1, 1):
            x = 120 + side * 50
            if blink:
                draw.rounded_rectangle((x - 28, 100 + bob - 3, x + 28, 100 + bob + 3), 3, fill=eye)
            else:
                draw.rounded_rectangle((x - 28, 64 + bob, x + 28, 136 + bob), 18, fill=eye)
                draw.ellipse((x - 17, 70 + bob, x - 3, 84 + bob), fill=(255, 255, 255))
        draw.arc((104, 140 + bob, 136, 172 + bob), 30, 150, fill=eye, width=5)
        frames.append(image.quantize(64))
    return frames


def main():
    OUT.mkdir(exist_ok=True)
    for name, painter in (("gun batimi", sunset), ("gece", night), ("kalp", pixel_heart), ("retro", retro)):
        painter().save(OUT / f"{name}.png")
    frames = mascot_frames()
    frames[0].save(OUT / "maskot.gif", save_all=True, append_images=frames[1:], duration=100, loop=0)
    print(f"sample themes in {OUT}")


if __name__ == "__main__":
    main()
