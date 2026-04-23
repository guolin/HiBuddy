from __future__ import annotations

import json
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

from .spec import GRID64_COLUMNS, GRID64_FRAME_COUNT, GRID64_ROWS, GRID64_ROW_SPECS, SPRITE_SIZE, STATE_SPECS


def build_grid64_prompt() -> str:
    row_lines: list[str] = []
    for row_spec in GRID64_ROW_SPECS:
        labels = ", ".join(
            f"{row_spec['state']} {label.replace('-', ' ')}" for label, _description in row_spec["labels"]
        )
        row_lines.append(f"Row {int(row_spec['row']) + 1}: {labels}")

    return (
        "Use case: stylized-concept\n"
        "Asset type: one single large pixel art sprite atlas for Buddy V1\n"
        "Primary request: generate exactly 64 frame cells arranged in a strict 8 by 8 grid, every cell occupied by one sprite of the same character\n"
        "Subject: exactly the same single pet identity as the reference pixel master image\n"
        "Style/medium: true pixel art atlas, crisp square pixels, hard edges, retro handheld game sprite look, no anti-aliasing, no soft illustration rendering\n"
        "Composition/framing: each of the 64 cells has the same size; each cell contains one centered full-body sprite; leave clearly visible empty gutters between cells; no sprite or effect may cross into adjacent cells; use a clean bright separator grid between cells so the 8 by 8 split is obvious; transparent background preferred; if transparency is not possible, use only clean white gutters and no other background decoration\n"
        "Color palette: limited palette around 32 colors or fewer, simple readable shading, no gradients, no texture noise\n"
        "Frame plan:\n"
        + "\n".join(row_lines)
        + "\nConstraints: keep same silhouette, same face layout, same proportions, same body count and limb count; keep the same recognizable identity in every cell; each cell must be visibly different from neighboring cells; all 64 cells must be present and filled; each sprite must stay fully inside its own cell; no labels; no text; no watermark; no outer border frame"
    )


def remove_connected_light_background(
    image: Image.Image,
    *,
    light_threshold: int = 220,
    max_chroma_delta: int = 22,
) -> Image.Image:
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    queue: deque[tuple[int, int]] = deque()
    visited: set[tuple[int, int]] = set()

    def is_light(px: tuple[int, int, int, int]) -> bool:
        r, g, b, a = px
        if a == 0:
            return True
        if min(r, g, b) < light_threshold:
            return False
        return max(r, g, b) - min(r, g, b) <= max_chroma_delta

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
        if not is_light(pixels[x, y]):
            continue
        pixels[x, y] = (0, 0, 0, 0)
        queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
    return image


def find_light_gutter_segments(image: Image.Image, axis: str, expected_cells: int) -> list[tuple[int, int]]:
    image = image.convert("RGBA")
    width, height = image.size
    values: list[int] = []
    if axis == "x":
        total = width
        full = height
        for x in range(width):
            count = 0
            for y in range(height):
                r, g, b, a = image.getpixel((x, y))
                if a == 0 or (min(r, g, b) >= 220 and max(r, g, b) - min(r, g, b) <= 22):
                    count += 1
            values.append(count)
    else:
        total = height
        full = width
        for y in range(height):
            count = 0
            for x in range(width):
                r, g, b, a = image.getpixel((x, y))
                if a == 0 or (min(r, g, b) >= 220 and max(r, g, b) - min(r, g, b) <= 22):
                    count += 1
            values.append(count)

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

    if len(gutters) >= expected_cells + 1:
        occupied: list[tuple[int, int]] = []
        left = gutters[0][1] + 1
        for gutter in gutters[1:]:
            right = gutter[0] - 1
            if right >= left:
                occupied.append((left, right + 1))
            left = gutter[1] + 1
        if len(occupied) >= expected_cells:
            return occupied[:expected_cells]

    step = total // expected_cells
    return [(i * step, (i + 1) * step if i < expected_cells - 1 else total) for i in range(expected_cells)]


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


def extract_cell_sprite(cell: Image.Image, postprocess_fn) -> tuple[Image.Image, dict[str, int | bool]]:
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
        near_above = by1 <= maxy + 14 and by0 >= miny - 26
        near_side = bx0 >= minx - 18 and bx1 <= maxx + 18
        if comp["pixels"] >= 18 and (near_main or (near_above and near_side)):
            minx = min(minx, bx0)
            miny = min(miny, by0)
            maxx = max(maxx, bx1)
            maxy = max(maxy, by1)
            included += 1

    minx = max(0, minx - 4)
    miny = max(0, miny - 8)
    maxx = min(cell.width, maxx + 4)
    maxy = min(cell.height, maxy + 4)

    cropped = cell.crop((minx, miny, maxx, maxy))
    cropped = postprocess_fn(cropped)
    fitted = ImageOps.contain(cropped, (62, 62), Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0))
    x = (SPRITE_SIZE[0] - fitted.width) // 2
    y = max(0, min(2, SPRITE_SIZE[1] - fitted.height))
    canvas.paste(fitted, (x, y), fitted)
    return canvas, {
        "nontransparent_pixels": sum(1 for px in cell.getdata() if px[3] > 0),
        "bbox_w": maxx - minx,
        "bbox_h": maxy - miny,
        "component_count": len(components),
        "merged_component_count": included,
        "flag_sparse": False,
    }


def slice_grid64_atlas(sheet: Image.Image, postprocess_fn) -> tuple[list[Image.Image], Image.Image, dict[str, object]]:
    bg_removed = remove_connected_light_background(sheet)
    x_segments = find_light_gutter_segments(sheet, "x", GRID64_COLUMNS)
    y_segments = find_light_gutter_segments(sheet, "y", GRID64_ROWS)
    frames: list[Image.Image] = []
    cell_reports: list[dict[str, object]] = []

    frame_index = 0
    for row, (top, bottom) in enumerate(y_segments):
        for col, (left, right) in enumerate(x_segments):
            cell = bg_removed.crop((left, top, right, bottom)).convert("RGBA")
            sprite, metrics = extract_cell_sprite(cell, postprocess_fn)
            frames.append(sprite)
            cell_reports.append(
                {
                    "frame_id": f"F{frame_index:02d}",
                    "frame_index": frame_index,
                    "row": row,
                    "col": col,
                    "segment": [left, top, right, bottom],
                    **metrics,
                    "flag_sparse": bool(metrics["flag_sparse"]) or int(metrics["nontransparent_pixels"]) < 90,
                }
            )
            frame_index += 1

    return frames[:GRID64_FRAME_COUNT], bg_removed, {
        "x_segments": x_segments,
        "y_segments": y_segments,
        "cells": cell_reports[:GRID64_FRAME_COUNT],
    }


def render_grid64_preview(frames: list[Image.Image], output_path: Path) -> None:
    pixel = 4
    padding = 12
    label_h = 22
    card = (238, 229, 208, 255)
    bg = (250, 244, 231, 255)
    text = (42, 37, 30, 255)
    tile_w = SPRITE_SIZE[0] * pixel
    tile_h = SPRITE_SIZE[1] * pixel
    width = padding * 2 + GRID64_COLUMNS * (tile_w + padding)
    height = padding * 2 + GRID64_ROWS * (label_h + tile_h + padding)

    sheet = Image.new("RGBA", (width, height), bg)
    draw = ImageDraw.Draw(sheet)
    frame_index = 0
    for row_spec in GRID64_ROW_SPECS:
        row = int(row_spec["row"])
        y = padding + row * (label_h + tile_h + padding)
        draw.text((padding, y), str(row_spec["state"]), fill=text)
        y += label_h
        for col in range(GRID64_COLUMNS):
            x = padding + col * (tile_w + padding)
            draw.rounded_rectangle((x - 4, y - 4, x + tile_w + 4, y + tile_h + 4), radius=8, fill=card)
            if frame_index < len(frames):
                frame = frames[frame_index].resize((tile_w, tile_h), Image.Resampling.NEAREST)
                sheet.alpha_composite(frame, (x, y))
            frame_index += 1
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)


def export_frame_semantics(output_path: Path) -> list[dict[str, object]]:
    from .spec import build_grid64_frame_semantics

    semantics = build_grid64_frame_semantics()
    output_path.write_text(json.dumps(semantics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return semantics


def build_gif_groups() -> dict[str, dict[str, int | str]]:
    groups: dict[str, dict[str, int | str]] = {}
    for row_spec in GRID64_ROW_SPECS:
        if row_spec["gif_group"] is None:
            continue
        name = str(row_spec["gif_group"])
        start_frame = int(row_spec["row"]) * GRID64_COLUMNS
        duration = next((state.frame_interval_ms for state in STATE_SPECS if state.name == name), 180)
        groups[name] = {
            "state": str(row_spec["state"]),
            "start_frame": start_frame,
            "frame_count": GRID64_COLUMNS,
            "duration_ms": duration,
        }
    return groups


def export_state_gifs(frames: list[Image.Image], output_dir: Path) -> dict[str, str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs: dict[str, str] = {}
    for name, group in build_gif_groups().items():
        start = int(group["start_frame"])
        count = int(group["frame_count"])
        duration = int(group["duration_ms"])
        gif_frames = [frame.copy() for frame in frames[start : start + count]]
        if not gif_frames:
            continue
        path = output_dir / f"{name}.gif"
        gif_frames[0].save(
            path,
            save_all=True,
            append_images=gif_frames[1:],
            duration=duration,
            loop=0,
            disposal=2,
            transparency=0,
        )
        outputs[name] = str(path)
    return outputs
