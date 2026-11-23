"""Turns any pictures and animated GIFs into 240x240 files for the PixelCharm screen.

    python tools/prepare_images.py                       themes-private/ -> firmware/data/themes/
    python tools/prepare_images.py my_pics out --preview preview.png

Each picture is cut to a square (portrait pictures keep their upper part, where faces usually are) and
shrunk to 240x240. Still pictures become baseline JPEGs, the kind the board's decoder can read.
Animated GIFs stay GIFs: every frame is cut and shrunk the same way, and keeps its timing.
Then "Upload Filesystem Image" in PlatformIO copies firmware/data/ onto the board.

Needs Pillow: pip install pillow
Pictures you don't own (anime art...) belong in themes-private/, which git ignores. Never commit them.
"""
import argparse
import re
import unicodedata
from pathlib import Path

from PIL import Image, ImageSequence

SIZE = 240
EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".gif"}
# Where the square sits in a tall picture: 0 = top, 0.5 = middle. Faces are usually in the upper part.
PORTRAIT_FOCUS = 0.3
# Browsers show GIF frames shorter than this at 100 ms, so we do the same
MIN_FRAME_MS = 20


def square_crop(image):
    width, height = image.size
    side = min(width, height)
    if height > width:
        top = round((height - side) * PORTRAIT_FOCUS)
        return image.crop((0, top, side, top + side))
    left = (width - side) // 2
    return image.crop((left, 0, left + side, side))


def to_rgb(image):
    """JPEG has no transparency: put see-through pictures on black, like the screen's background."""
    if image.mode in ("RGBA", "LA", "P"):
        image = image.convert("RGBA")
        background = Image.new("RGB", image.size, (0, 0, 0))
        background.paste(image, mask=image.getchannel("A"))
        return background
    return image.convert("RGB")


def fit(image):
    return square_crop(to_rgb(image)).resize((SIZE, SIZE), Image.LANCZOS)


def short_name(path, number, extension):
    """01_name.jpg: the number keeps the order on the board, the name stays ASCII and short."""
    # Turkish ı and İ don't break down into an ASCII letter plus a mark like ç or ö do, so map them first
    stem = path.stem.translate(str.maketrans({"ı": "i", "İ": "I"}))
    ascii_name = unicodedata.normalize("NFKD", stem).encode("ascii", "ignore").decode()
    slug = re.sub(r"[^a-z0-9]+", "_", ascii_name.lower()).strip("_")[:20] or "picture"
    return f"{number:02d}_{slug}{extension}"


def save_jpeg(path, number, output):
    with Image.open(path) as original:
        image = fit(original)
    target = output / short_name(path, number, ".jpg")
    # subsampling=0 keeps full colour detail, which keeps anime outlines crisp. progressive=False: the
    # board's decoder only reads baseline JPEGs.
    image.save(target, "JPEG", quality=90, subsampling=0, progressive=False, optimize=True)
    return target, image


def save_gif(path, number, output):
    frames, durations = [], []
    with Image.open(path) as original:
        for frame in ImageSequence.Iterator(original):
            duration = frame.info.get("duration", 100)
            durations.append(duration if duration >= MIN_FRAME_MS else 100)
            # A GIF frame can only have 256 colours. No dithering: its dots would shimmer from frame to frame.
            frames.append(fit(frame).quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE))
    target = output / short_name(path, number, ".gif")
    # loop=0: play forever. Pillow stores only the part of each frame that changed, which keeps the file small.
    frames[0].save(target, save_all=True, append_images=frames[1:], duration=durations, loop=0, disposal=1)
    print(f"  {len(frames)} frames, {sum(durations) / 1000:.2f} s per loop")
    return target, frames[0].convert("RGB")


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", nargs="?", default=root / "themes-private", type=Path)
    parser.add_argument("output", nargs="?", default=root / "firmware" / "data" / "themes", type=Path)
    parser.add_argument("--preview", type=Path, help="also save all results side by side in one PNG")
    args = parser.parse_args()

    pictures = sorted(p for p in args.source.iterdir() if p.suffix.lower() in EXTENSIONS)
    if not pictures:
        raise SystemExit(f"no pictures in {args.source}")
    args.output.mkdir(parents=True, exist_ok=True)
    for old in [*args.output.glob("*.jpg"), *args.output.glob("*.gif")]:
        old.unlink()  # the folder mirrors the source: a picture you removed disappears from the board too

    results = []
    for number, path in enumerate(pictures, start=1):
        with Image.open(path) as probe:
            animated = getattr(probe, "n_frames", 1) > 1
            size = probe.size
        target, first_frame = (save_gif if animated else save_jpeg)(path, number, args.output)
        print(f"{path.name} ({size[0]}x{size[1]}) -> {target.name}, {target.stat().st_size / 1024:.1f} KB")
        results.append(first_frame)

    if args.preview:
        sheet = Image.new("RGB", (len(results) * (SIZE + 10) - 10, SIZE), (40, 40, 40))
        for i, image in enumerate(results):
            sheet.paste(image, (i * (SIZE + 10), 0))
        sheet.save(args.preview)
        print(f"preview: {args.preview}")


if __name__ == "__main__":
    main()
