from __future__ import annotations

import argparse
import json
import statistics
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

from .pipeline import (
    PALETTE_SIZE,
    SPRITE_SIZE,
    STATE_SPECS,
    build_shared_palette,
    call_openai_image_endpoint,
    ensure_dir,
    normalize_api_base,
    postprocess_pixel_master,
    render_palette_preview,
    render_preview_sheet,
)


GRID_COLUMNS = 6
GRID_ROWS = len(STATE_SPECS)
FRAME_COUNT = sum(state.frame_count for state in STATE_SPECS)


def build_spritesheet_prompt() -> str:
    return (
        "Use case: stylized-concept\n"
        "Asset type: one single big pixel art sprite sheet image for Buddy V1\n"
        "Primary request: generate a complete sprite sheet for the same octopus-cat character, containing 32 different animation frames arranged in a strict 6 by 6 grid, with the last 4 cells empty\n"
        "Subject: exactly the same single character identity as the reference pixel master image\n"
        "Style/medium: true pixel art sprite sheet, crisp square pixels, hard edges, no anti-aliasing, retro handheld game sprite look\n"
        "Composition/framing: each occupied grid cell contains exactly one centered full-body character sprite that fills most of that cell with very little empty margin; leave a clearly transparent gutter between cells; every sprite and every effect must stay fully inside its own cell and must not cross into adjacent cells; transparent background only, no white background, no border, no labels, no text\n"
        "Frame plan: first 4 occupied cells are sleep frames with eyes closed; next 6 occupied cells are idle frames with friendly open eyes and small variations; next 5 are busy with more focused serious eyes; next 4 are attention with alert raised posture; next 5 are celebrate with happy energetic expression; next 4 are dizzy with woozy confused expression; final 4 are heart with affectionate welcoming expression\n"
        "Constraints: keep same silhouette, same face layout, same tentacle count, same proportions, same black hood-like head shape, same peach face, same cute identity; each state must be visibly different; use around 32 colors or fewer; no gradients; no texture noise; empty cells must stay transparent; preserve a visibly regular grid layout with stable sprite placement"
    )


def find_axis_boundaries(mask: Image.Image, axis: str, expected_cells: int) -> list[tuple[int, int]]:
    width, height = mask.size
    if axis == "x":
        values = [sum(mask.getpixel((x, y)) > 0 for y in range(height)) for x in range(width)]
        length = width
    else:
        values = [sum(mask.getpixel((x, y)) > 0 for x in range(width)) for y in range(height)]
        length = height

    if not values or max(values) == 0:
        step = length // expected_cells
        return [(i * step, (i + 1) * step if i < expected_cells - 1 else length) for i in range(expected_cells)]

    peak = max(values)
    threshold = max(2, int(peak * 0.12))
    active = [value > threshold for value in values]

    segments: list[tuple[int, int]] = []
    start: int | None = None
    for idx, flag in enumerate(active):
        if flag and start is None:
            start = idx
        elif not flag and start is not None:
            segments.append((start, idx))
            start = None
    if start is not None:
        segments.append((start, length))

    if len(segments) == expected_cells:
        return segments

    if len(segments) > expected_cells:
        ranked = sorted(segments, key=lambda item: item[1] - item[0], reverse=True)[:expected_cells]
        return sorted(ranked)

    if len(segments) >= 2:
        med = statistics.median([(end - start) for start, end in segments])
        padded: list[tuple[int, int]] = []
        for start, end in segments:
            center = (start + end) // 2
            half = int(max(med // 2, 1))
            padded.append((max(0, center - half), min(length, center + half)))
        if len(padded) == expected_cells:
            return padded

    step = length // expected_cells
    return [(i * step, (i + 1) * step if i < expected_cells - 1 else length) for i in range(expected_cells)]


def find_light_gutter_segments(image: Image.Image, axis: str, expected_cells: int) -> list[tuple[int, int]]:
    image = image.convert("RGBA")
    width, height = image.size
    if axis == "x":
        values = []
        for x in range(width):
            c = 0
            for y in range(height):
                r, g, b, _ = image.getpixel((x, y))
                if r >= 220 and g >= 220 and b >= 220 and max(r, g, b) - min(r, g, b) <= 22:
                    c += 1
            values.append(c)
        full = height
        total = width
    else:
        values = []
        for y in range(height):
            c = 0
            for x in range(width):
                r, g, b, _ = image.getpixel((x, y))
                if r >= 220 and g >= 220 and b >= 220 and max(r, g, b) - min(r, g, b) <= 22:
                    c += 1
            values.append(c)
        full = width
        total = height

    threshold = int(full * 0.985)
    gutters: list[tuple[int, int]] = []
    start: int | None = None
    for idx, value in enumerate(values):
        if value >= threshold and start is None:
            start = idx
        elif value < threshold and start is not None:
            gutters.append((start, idx - 1))
            start = None
    if start is not None:
        gutters.append((start, total - 1))

    if len(gutters) < 2:
        step = total // expected_cells
        return [(i * step, (i + 1) * step if i < expected_cells - 1 else total) for i in range(expected_cells)]

    occupied: list[tuple[int, int]] = []
    left = gutters[0][1] + 1
    for gutter in gutters[1:]:
        right = gutter[0] - 1
        if right >= left:
            occupied.append((left, right + 1))
        left = gutter[1] + 1

    if len(occupied) == expected_cells:
        return occupied
    if len(occupied) > expected_cells:
        return occupied[:expected_cells]

    step = total // expected_cells
    return [(i * step, (i + 1) * step if i < expected_cells - 1 else total) for i in range(expected_cells)]


def remove_sheet_background(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    queue: deque[tuple[int, int]] = deque()
    visited: set[tuple[int, int]] = set()

    def is_bg(px: tuple[int, int, int, int]) -> bool:
        r, g, b, _ = px
        return min(r, g, b) >= 220 and max(r, g, b) - min(r, g, b) <= 22

    for x in range(width):
        queue.append((x, 0))
        queue.append((x, height - 1))
    for y in range(height):
        queue.append((0, y))
        queue.append((width - 1, y))

    while queue:
        x, y = queue.popleft()
        if not (0 <= x < width and 0 <= y < height) or (x, y) in visited:
            continue
        visited.add((x, y))
        if not is_bg(pixels[x, y]):
            continue
        pixels[x, y] = (0, 0, 0, 0)
        queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
    return image


def render_rgba_preview_sheet(frames: list[Image.Image], output_path: Path) -> None:
    pixel = 4
    padding = 16
    label_h = 24
    text_pad = 20
    card = (238, 229, 208, 255)
    bg = (250, 244, 231, 255)
    text = (42, 37, 30, 255)

    max_frames = max(state.frame_count for state in STATE_SPECS)
    frame_w = SPRITE_SIZE[0] * pixel
    frame_h = SPRITE_SIZE[1] * pixel
    width = padding * 2 + max_frames * (frame_w + padding)
    height = padding * 2 + len(STATE_SPECS) * (label_h + frame_h + text_pad + padding)

    sheet = Image.new("RGBA", (width, height), bg)
    draw = ImageDraw.Draw(sheet)
    frame_index = 0
    y = padding
    for state in STATE_SPECS:
        draw.text((padding, y), f"{state.code} {state.name}", fill=text)
        y += label_h
        for column in range(max_frames):
            x = padding + column * (frame_w + padding)
            draw.rounded_rectangle((x - 4, y - 4, x + frame_w + 4, y + frame_h + 4), radius=8, fill=card)
            if column < state.frame_count and frame_index < len(frames):
                frame = frames[frame_index].resize((frame_w, frame_h), Image.Resampling.NEAREST)
                sheet.alpha_composite(frame, (x, y))
                draw.text((x, y + frame_h + 4), f"F{column:02d}", fill=text)
                frame_index += 1
        y += frame_h + text_pad + padding
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)


def connected_components(mask: Image.Image, min_pixels: int = 8) -> list[dict[str, object]]:
    width, height = mask.size
    visited: set[tuple[int, int]] = set()
    components: list[dict[str, object]] = []
    for y in range(height):
        for x in range(width):
            if mask.getpixel((x, y)) == 0 or (x, y) in visited:
                continue
            queue: deque[tuple[int, int]] = deque([(x, y)])
            visited.add((x, y))
            minx = maxx = x
            miny = maxy = y
            count = 0
            while queue:
                cx, cy = queue.popleft()
                count += 1
                minx = min(minx, cx)
                maxx = max(maxx, cx)
                miny = min(miny, cy)
                maxy = max(maxy, cy)
                for nx, ny in (
                    (cx - 1, cy),
                    (cx + 1, cy),
                    (cx, cy - 1),
                    (cx, cy + 1),
                    (cx - 1, cy - 1),
                    (cx + 1, cy - 1),
                    (cx - 1, cy + 1),
                    (cx + 1, cy + 1),
                ):
                    if 0 <= nx < width and 0 <= ny < height and mask.getpixel((nx, ny)) > 0 and (nx, ny) not in visited:
                        visited.add((nx, ny))
                        queue.append((nx, ny))
            if count >= min_pixels:
                components.append(
                    {
                        "pixels": count,
                        "bbox": (minx, miny, maxx + 1, maxy + 1),
                        "center": ((minx + maxx + 1) / 2, (miny + maxy + 1) / 2),
                    }
                )
    components.sort(key=lambda item: item["pixels"], reverse=True)
    return components


def extract_cell_sprite(cell: Image.Image) -> tuple[Image.Image, dict[str, int | bool]]:
    mask = cell.getchannel("A")
    components = connected_components(mask)
    if not components:
        return Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0)), {
            "nontransparent_pixels": 0,
            "bbox_w": 0,
            "bbox_h": 0,
            "component_count": 0,
            "flag_sparse": True,
        }

    main = components[0]
    minx, miny, maxx, maxy = main["bbox"]
    cx, cy = main["center"]
    included = 1
    for comp in components[1:]:
        bx0, by0, bx1, by1 = comp["bbox"]
        ccx, ccy = comp["center"]
        near_main = abs(ccx - cx) <= 42 and abs(ccy - cy) <= 42
        vertical_effect = by1 <= maxy + 12 and by0 >= miny - 20
        if comp["pixels"] >= 24 and (near_main or vertical_effect):
            minx = min(minx, bx0)
            miny = min(miny, by0)
            maxx = max(maxx, bx1)
            maxy = max(maxy, by1)
            included += 1

    pad_left = 4
    pad_right = 4
    pad_top = 6
    pad_bottom = 4
    minx = max(0, minx - pad_left)
    miny = max(0, miny - pad_top)
    maxx = min(cell.width, maxx + pad_right)
    maxy = min(cell.height, maxy + pad_bottom)

    cropped = cell.crop((minx, miny, maxx, maxy))
    cropped = postprocess_pixel_master(cropped)
    fitted = ImageOps.contain(cropped, (60, 60), Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0))
    x = (SPRITE_SIZE[0] - fitted.width) // 2
    y = (SPRITE_SIZE[1] - fitted.height) // 2
    canvas.paste(fitted, (x, y), fitted)
    return canvas, {
        "nontransparent_pixels": sum(1 for px in cell.getdata() if px[3] > 0),
        "bbox_w": maxx - minx,
        "bbox_h": maxy - miny,
        "component_count": len(components),
        "flag_sparse": False,
    }


def slice_spritesheet(sheet: Image.Image) -> tuple[list[Image.Image], dict[str, object]]:
    prepared = remove_sheet_background(sheet.convert("RGBA"))
    alpha_mask = prepared.getchannel("A")
    x_segments = find_light_gutter_segments(sheet, "x", GRID_COLUMNS)
    y_segments = find_light_gutter_segments(sheet, "y", GRID_ROWS)
    frames: list[Image.Image] = []
    cell_reports: list[dict[str, object]] = []

    for row, (top, bottom) in enumerate(y_segments):
        for col, (left, right) in enumerate(x_segments):
            if len(frames) >= FRAME_COUNT:
                return frames, {"x_segments": x_segments, "y_segments": y_segments, "cells": cell_reports}
            cell = prepared.crop((left, top, right, bottom)).convert("RGBA")
            sprite, metrics = extract_cell_sprite(cell)
            frames.append(sprite)
            cell_reports.append(
                {
                    "row": row,
                    "col": col,
                    "segment": [left, top, right, bottom],
                    **metrics,
                    "flag_sparse": metrics["nontransparent_pixels"] < 180,
                }
            )
    return frames, {"x_segments": x_segments, "y_segments": y_segments, "cells": cell_reports}


def process_sheet_image(sheet: Image.Image, out_dir: Path, report: dict[str, object]) -> Path:
    raw_dir = out_dir / "sliced_frames"
    ensure_dir(out_dir)
    ensure_dir(raw_dir)
    sheet.save(out_dir / "spritesheet_raw.png")
    remove_sheet_background(sheet.copy()).save(out_dir / "spritesheet_bg_removed.png")

    frames, segmentation = slice_spritesheet(sheet)
    for index, frame in enumerate(frames):
        frame.save(raw_dir / f"frame_{index:02d}.png")

    render_rgba_preview_sheet(frames, out_dir / "preview_sheet_from_sheet.png")
    indexed_frames, palette = build_shared_palette(frames)
    render_palette_preview(palette, out_dir / "palette_preview.png")

    sparse_cells = [cell for cell in segmentation["cells"] if cell["flag_sparse"]]
    report.update(
        {
            "frame_count": len(frames),
            "palette_size": PALETTE_SIZE,
            "segmentation": segmentation,
            "sparse_cell_count": len(sparse_cells),
            "sparse_cells": sparse_cells[:12],
        }
    )
    (out_dir / "sheet_experiment_report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return out_dir


def run(args: argparse.Namespace) -> Path:
    api_base = normalize_api_base(args.api_base)
    out_dir = Path(args.out_dir)
    raw_dir = out_dir / "sliced_frames"
    ensure_dir(out_dir)
    ensure_dir(raw_dir)

    reference = Image.open(args.reference).convert("RGBA")
    reference.save(out_dir / "reference_pixel_master.png")

    prompt = build_spritesheet_prompt()
    sheet, payload = call_openai_image_endpoint(
        endpoint=api_base + "/images/edits",
        prompt=prompt,
        model=args.model,
        api_key=args.api_key,
        timeout=240,
        image=reference,
    )
    report = {
        "api_base": api_base,
        "model": args.model,
        "grid_columns": GRID_COLUMNS,
        "grid_rows": GRID_ROWS,
        "prompt": prompt,
        "response_summary": {"created": payload.get("created"), "data_count": len(payload.get("data") or [])},
    }
    return process_sheet_image(sheet, out_dir, report)


def reprocess(args: argparse.Namespace) -> Path:
    out_dir = Path(args.out_dir)
    ensure_dir(out_dir)
    sheet = Image.open(args.sheet).convert("RGBA")
    report = {
        "mode": "reprocess-existing-sheet",
        "source_sheet": str(Path(args.sheet).resolve()),
        "grid_columns": GRID_COLUMNS,
        "grid_rows": GRID_ROWS,
    }
    if args.reference:
        reference = Image.open(args.reference).convert("RGBA")
        reference.save(out_dir / "reference_pixel_master.png")
    return process_sheet_image(sheet, out_dir, report)


def main() -> None:
    parser = argparse.ArgumentParser(description="Experimental one-shot sprite sheet generation for Buddy.")
    parser.add_argument("--reference", help="Path to pixel_master.png")
    parser.add_argument("--api-key", help="OpenAI compatible API key")
    parser.add_argument("--api-base", help="OpenAI compatible API base URL")
    parser.add_argument("--model", default="gpt-image-2", help="Image model")
    parser.add_argument("--out-dir", required=True, help="Output directory")
    parser.add_argument("--sheet", help="Existing spritesheet_raw.png to reprocess locally without AI")
    args = parser.parse_args()
    if args.sheet:
        output = reprocess(args)
    else:
        if not args.reference or not args.api_key or not args.api_base:
            raise SystemExit("--reference, --api-key, and --api-base are required unless --sheet is used")
        output = run(args)
    print(output)


if __name__ == "__main__":
    main()
