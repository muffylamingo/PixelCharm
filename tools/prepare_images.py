"""Turns any pictures into 240x240 JPEGs for the PixelCharm screen.

    python tools/prepare_images.py                       themes-private/ -> firmware/data/themes/
    python tools/prepare_images.py my_pics out --preview preview.png

Each picture is cut to a square (portrait pictures keep their upper part, where faces usually are),
shrunk to 240x240 and saved as a baseline JPEG, the kind the board's decoder can read.
Then "Upload Filesystem Image" in PlatformIO copies firmware/data/ onto the board.

Needs Pillow: pip install pillow
Pictures you don't own (anime art...) belong in themes-private/, which git ignores. Never commit them.
"""
import argparse
import re
import unicodedata
from pathlib import Path

from PIL import Image

SIZE = 240
EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".gif"}
# Where the square sits in a tall picture: 0 = top, 0.5 = middle. Faces are usually in the upper part.
PORTRAIT_FOCUS = 0.3


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


def short_name(path, number):
    """01_name.jpg: the number keeps the order on the board, the name stays ASCII and short."""
    # Turkish ı and İ don't break down into an ASCII letter plus a mark like ç or ö do, so map them first
    stem = path.stem.translate(str.maketrans({"ı": "i", "İ": "I"}))
    ascii_name = unicodedata.normalize("NFKD", stem).encode("ascii", "ignore").decode()
    slug = re.sub(r"[^a-z0-9]+", "_", ascii_name.lower()).strip("_")[:20] or "picture"
    return f"{number:02d}_{slug}.jpg"


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", nargs="?", default=root / "themes-private", type=Path)
    parser.add_argument("output", nargs="?", default=root / "firmware" / "data" / "themes", type=Path)
    parser.add_argument("--quality", type=int, default=90, help="JPEG quality 1-95 (default 90)")
    parser.add_argument("--preview", type=Path, help="also save all results side by side in one PNG")
    args = parser.parse_args()

    pictures = sorted(p for p in args.source.iterdir() if p.suffix.lower() in EXTENSIONS)
    if not pictures:
        raise SystemExit(f"no pictures in {args.source}")
    args.output.mkdir(parents=True, exist_ok=True)
    for old in args.output.glob("*.jpg"):
        old.unlink()  # the folder mirrors the source: a picture you removed disappears from the board too

    results = []
    for number, path in enumerate(pictures, start=1):
        with Image.open(path) as original:
            image = square_crop(to_rgb(original)).resize((SIZE, SIZE), Image.LANCZOS)
        target = args.output / short_name(path, number)
        # subsampling=0 keeps full colour detail, which keeps anime outlines crisp. progressive=False: the
        # board's decoder only reads baseline JPEGs.
        image.save(target, "JPEG", quality=args.quality, subsampling=0, progressive=False, optimize=True)
        print(f"{path.name} ({original.width}x{original.height}) -> {target.name}, {target.stat().st_size / 1024:.1f} KB")
        results.append(image)

    if args.preview:
        sheet = Image.new("RGB", (len(results) * (SIZE + 10) - 10, SIZE), (40, 40, 40))
        for i, image in enumerate(results):
            sheet.paste(image, (i * (SIZE + 10), 0))
        sheet.save(args.preview)
        print(f"preview: {args.preview}")


if __name__ == "__main__":
    main()
