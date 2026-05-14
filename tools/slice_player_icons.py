# One-off: slice 3x4 grid sheet into project icon/*.png (white on transparent).
# Run: python tools/slice_player_icons.py [optional_path_to_grid.png]

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

GRID_TO_FILE: dict[int, str] = {
    1: "11beat.png",
    2: "10list.png",
    3: "4next.png",
    4: "3pause.png",
    5: "2play.png",
    6: "1previous.png",
    7: "7repeatall.png",
    8: "6repeatlist.png",
    9: "5repeatone.png",
    10: "8shuffle.png",
    12: "9volume.png",
}

OUTPUT_SIZE = 128
# Bottom strip of each grid cell is the 1–12 label; never include it in output.
ICON_TOP_FRACTION = 0.68
# BBox: only treat pixels clearly darker than paper as ink
SUM_THRESH = 720


def to_white_transparent_rgba(src: Image.Image) -> Image.Image:
    src = src.convert("RGB")
    w, h = src.size
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = src.load()
    po = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            l = (r + g + b) // 3
            if l >= 252:
                po[x, y] = (255, 255, 255, 0)
            else:
                a = min(255, max(0, 255 - l))
                po[x, y] = (255, 255, 255, a)
    return out


def bbox_ink(rgb_img: Image.Image, max_y: int) -> tuple[int, int, int, int] | None:
    px = rgb_img.load()
    w, h = rgb_img.size
    max_y = min(max_y, h)
    min_x, min_y = w, h
    max_x, max_y2 = -1, -1
    for y in range(max_y):
        for x in range(w):
            r, g, b = px[x, y]
            if r + g + b < SUM_THRESH:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y2 = max(max_y2, y)
    if max_x < 0:
        return None
    return min_x, min_y, max_x + 1, max_y2 + 1


def trim_detached_bottom_alpha_cluster(
    img: Image.Image,
    min_gap_rows: int = 10,
    bottom_start_frac: float = 0.80,
    alpha_thresh: int = 16,
) -> Image.Image:
    """
    Grid index labels often survive as a tiny ink cluster at the canvas bottom,
    separated from the real icon by several fully-transparent rows. List-style
    icons can have multiple mid-body gaps, so we only strip when the last cluster
    starts low on the canvas (bottom_start_frac).
    """
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()

    def row_active(y: int) -> bool:
        for x in range(w):
            if px[x, y][3] > alpha_thresh:
                return True
        return False

    segments: list[tuple[int, int]] = []
    y = 0
    while y < h:
        while y < h and not row_active(y):
            y += 1
        if y >= h:
            break
        y0 = y
        while y < h and row_active(y):
            y += 1
        segments.append((y0, y - 1))

    if len(segments) < 2:
        return img

    top_end = segments[-2][1]
    bot_start = segments[-1][0]
    gap = bot_start - top_end - 1
    if gap < min_gap_rows or bot_start < int(h * bottom_start_frac):
        return img

    new_h = top_end + 1
    cropped = img.crop((0, 0, w, new_h))
    canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    oy = (h - new_h) // 2
    canvas.paste(cropped, (0, oy), cropped)
    return canvas


def fit_center_on_square(rgba: Image.Image, side: int, margin: int) -> Image.Image:
    """Scale uniformly to fit inside (side-2*margin), center on transparent square."""
    tw = side - 2 * margin
    th = side - 2 * margin
    w, h = rgba.size
    if w <= 0 or h <= 0:
        return Image.new("RGBA", (side, side), (0, 0, 0, 0))
    scale = min(tw / w, th / h)
    nw = max(1, int(round(w * scale)))
    nh = max(1, int(round(h * scale)))
    scaled = rgba.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    ox = (side - nw) // 2
    oy = (side - nh) // 2
    canvas.paste(scaled, (ox, oy), scaled)
    return canvas


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    default_src = Path(
        r"C:\Users\czj14\.cursor\projects\d-Study-Qt-practice-MusicPlayer\assets"
        r"\c__Users_czj14_AppData_Roaming_Cursor_User_workspaceStorage_5077b8fdb13b63246fa9b6fb265eb4c0_images_image-16e78ac5-6665-41c4-a02b-0cac4de1ba95.png"
    )
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else default_src
    if not src.is_file():
        print("Source not found:", src)
        return 1

    icon_dir = repo / "icon"
    icon_dir.mkdir(parents=True, exist_ok=True)

    sheet = Image.open(src).convert("RGB")
    sw, sh = sheet.size
    rows, cols = 3, 4
    cw = sw // cols
    ch = sh // rows
    inset = max(1, min(cw, ch) // 48)

    for row in range(rows):
        for col in range(cols):
            idx = row * cols + col + 1
            name = GRID_TO_FILE.get(idx)
            if not name:
                continue

            x0 = col * cw + inset
            y0 = row * ch + inset
            x1 = (col + 1) * cw - inset
            y1 = (row + 1) * ch - inset
            cell = sheet.crop((x0, y0, x1, y1))

            clip_h = max(1, int(cell.height * ICON_TOP_FRACTION))
            icon_band = cell.crop((0, 0, cell.width, clip_h))

            bb = bbox_ink(icon_band, clip_h)
            if bb is None:
                bb = (0, 0, icon_band.width, clip_h)

            pad = max(2, min(icon_band.width, icon_band.height) // 40)
            xb0 = max(0, bb[0] - pad)
            yb0 = max(0, bb[1] - pad)
            xb1 = min(icon_band.width, bb[2] + pad)
            yb1 = min(icon_band.height, bb[3] + pad)
            ink = icon_band.crop((xb0, yb0, xb1, yb1))

            rgba = to_white_transparent_rgba(ink)
            margin = max(4, OUTPUT_SIZE // 16)
            out = fit_center_on_square(rgba, OUTPUT_SIZE, margin)
            out = trim_detached_bottom_alpha_cluster(out)

            dest = icon_dir / name
            out.save(dest, "PNG", optimize=True)
            print("Wrote", dest.relative_to(repo), "bbox", bb, "ink", ink.size)

    print("Done.", OUTPUT_SIZE, "px RGBA white on transparent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
