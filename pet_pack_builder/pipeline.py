from __future__ import annotations

import argparse
import base64
import io
import json
import math
import os
from datetime import UTC, datetime
from pathlib import Path

import requests
from PIL import Image, ImageChops, ImageEnhance, ImageFilter, ImageOps

from .atlas64 import (
    build_gif_groups,
    build_grid64_prompt,
    export_frame_semantics,
    export_state_gifs,
    render_grid64_preview,
    slice_grid64_atlas,
)
from .spec import (
    DEFAULT_SCALE_HINT,
    GRID64_COLUMNS,
    GRID64_FRAME_COUNT,
    GRID64_ROW_SPECS,
    GRID64_ROWS,
    INDEX_BITS,
    PACK_VERSION,
    PALETTE_SIZE,
    SPRITE_SIZE,
    STATE_SPECS,
    TRANSPARENT_INDEX,
)


class GenerationError(RuntimeError):
    pass


class PartialGenerationError(GenerationError):
    def __init__(self, message: str, report: dict[str, object]) -> None:
        super().__init__(message)
        self.report = report


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def normalize_base_image(path: Path) -> Image.Image:
    image = Image.open(path).convert("RGBA")
    image = ImageOps.exif_transpose(image)
    alpha = image.getchannel("A")
    bbox = alpha.getbbox()
    if bbox:
        image = image.crop(bbox)
    fitted = ImageOps.contain(image, SPRITE_SIZE, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0))
    offset = ((SPRITE_SIZE[0] - fitted.width) // 2, (SPRITE_SIZE[1] - fitted.height) // 2)
    canvas.paste(fitted, offset, fitted)
    return canvas


def normalize_api_base(api_base: str) -> str:
    base = api_base.rstrip("/")
    if not base.endswith("/v1"):
        base = base + "/v1"
    return base


def shift_image(image: Image.Image, dx: int = 0, dy: int = 0) -> Image.Image:
    offset = ImageChops.offset(image, dx, dy)
    if dx > 0:
        offset.paste((0, 0, 0, 0), (0, 0, dx, image.height))
    elif dx < 0:
        offset.paste((0, 0, 0, 0), (image.width + dx, 0, image.width, image.height))
    if dy > 0:
        offset.paste((0, 0, 0, 0), (0, 0, image.width, dy))
    elif dy < 0:
        offset.paste((0, 0, 0, 0), (0, image.height + dy, image.width, image.height))
    return offset


def add_overlay(image: Image.Image, color: tuple[int, int, int, int]) -> Image.Image:
    overlay = Image.new("RGBA", image.size, color)
    masked = Image.new("RGBA", image.size, (0, 0, 0, 0))
    masked.paste(overlay, mask=image.getchannel("A"))
    return Image.alpha_composite(image, masked)


def add_outline(image: Image.Image, color: tuple[int, int, int, int]) -> Image.Image:
    alpha = image.getchannel("A")
    expanded = alpha.filter(ImageFilter.MaxFilter(3))
    outline_mask = ImageChops.subtract(expanded, alpha)
    base = Image.new("RGBA", image.size, (0, 0, 0, 0))
    outline = Image.new("RGBA", image.size, color)
    base.paste(outline, mask=outline_mask)
    return Image.alpha_composite(base, image)


def sharpen_pixel_art(image: Image.Image) -> Image.Image:
    enlarged = image.resize((128, 128), Image.Resampling.NEAREST)
    sharpened = ImageEnhance.Contrast(enlarged).enhance(1.08)
    sharpened = sharpened.filter(ImageFilter.SHARPEN)
    return sharpened.resize(SPRITE_SIZE, Image.Resampling.NEAREST)


def remove_connected_light_background(
    image: Image.Image,
    *,
    light_threshold: int = 220,
    max_chroma_delta: int = 18,
) -> Image.Image:
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    queue: list[tuple[int, int]] = []
    visited: set[tuple[int, int]] = set()

    def is_light_background(x: int, y: int) -> bool:
        r, g, b, a = pixels[x, y]
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
        x, y = queue.pop()
        if (x, y) in visited or not (0 <= x < width and 0 <= y < height):
            continue
        visited.add((x, y))
        if not is_light_background(x, y):
            continue
        pixels[x, y] = (0, 0, 0, 0)
        queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
    return image


def quantize_rgba_to_palette(image: Image.Image, colors: int = PALETTE_SIZE) -> Image.Image:
    image = image.convert("RGBA")
    opaque = Image.new("RGB", image.size, (0, 0, 0))
    opaque.paste(image.convert("RGB"), mask=image.getchannel("A"))
    rgb = opaque.quantize(colors=max(1, colors - 1), method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    rgba = rgb.convert("RGBA")
    hard_alpha = image.getchannel("A").point(lambda a: 255 if a >= 128 else 0)
    rgba.putalpha(hard_alpha)
    return rgba


def tighten_sprite_to_canvas(image: Image.Image, target_inner_size: int = 62) -> Image.Image:
    image = image.convert("RGBA")
    bbox = image.getchannel("A").getbbox()
    if not bbox:
        return Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0))
    cropped = image.crop(bbox)
    fitted = ImageOps.contain(cropped, (target_inner_size, target_inner_size), Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0))
    offset_x = (SPRITE_SIZE[0] - fitted.width) // 2
    offset_y = max(0, min(1, SPRITE_SIZE[1] - fitted.height))
    canvas.paste(fitted, (offset_x, offset_y), fitted)
    return canvas


def postprocess_pixel_master(image: Image.Image) -> Image.Image:
    processed = remove_connected_light_background(image)
    processed = tighten_sprite_to_canvas(processed, target_inner_size=62)
    processed = quantize_rgba_to_palette(processed, colors=PALETTE_SIZE)
    return processed.resize(SPRITE_SIZE, Image.Resampling.NEAREST)


def build_master_prompt() -> str:
    return (
        "Use case: stylized-concept\n"
        "Asset type: tiny 64x64 pixel art character sprite master for Buddy V1\n"
        "Primary request: convert the reference character into a stable pixel art master sprite\n"
        "Subject: the exact same single character identity as the input image, preserving the recognizable silhouette and face layout\n"
        "Style/medium: true 64x64 pixel art sprite, crisp square pixels, hard edges, retro handheld game sprite look, no anti-aliasing, no soft illustration rendering\n"
        "Composition/framing: centered horizontally, fill most of the 64x64 canvas, head very close to the top edge, feet or tentacles very close to the bottom edge, almost no empty margin, transparent background only\n"
        "Color palette: limited palette, around 32 colors or fewer, simple flat shading, no gradients, no texture noise\n"
        "Constraints: preserve character identity, preserve main colors, preserve body count and limbs, keep proportions stable, no text, no watermark, no white background, no border, no outline frame, no shadow plate, no floating platform, no large empty background"
    )


def extract_response_image(payload: dict[str, object], timeout: int) -> Image.Image:
    item = (payload.get("data") or [{}])[0]
    if item.get("b64_json"):
        raw = base64.b64decode(item["b64_json"])
        return Image.open(io.BytesIO(raw)).convert("RGBA")
    if item.get("url"):
        fetched = requests.get(item["url"], timeout=timeout)
        fetched.raise_for_status()
        return Image.open(io.BytesIO(fetched.content)).convert("RGBA")
    raise GenerationError(f"Unsupported response payload: {json.dumps(payload)[:500]}")


def call_openai_image_endpoint(
    endpoint: str,
    prompt: str,
    model: str,
    api_key: str,
    timeout: int,
    image: Image.Image | None = None,
) -> tuple[Image.Image, dict[str, object]]:
    headers = {"Authorization": f"Bearer {api_key}"}
    if image is None:
        response = requests.post(
            endpoint,
            headers=headers,
            json={"model": model, "prompt": prompt, "size": "1024x1024"},
            timeout=timeout,
        )
    else:
        buf = io.BytesIO()
        image.save(buf, format="PNG")
        buf.seek(0)
        response = requests.post(
            endpoint,
            headers=headers,
            data={"model": model, "prompt": prompt, "size": "1024x1024"},
            files={"image": ("reference.png", buf.getvalue(), "image/png")},
            timeout=timeout,
        )
    if response.status_code >= 400:
        raise GenerationError(f"{response.status_code} {response.text}")
    payload = response.json()
    return extract_response_image(payload, timeout), payload


def generate_pixel_master(
    base_image: Image.Image,
    api_key: str,
    api_base: str,
    model: str,
    out_dir: Path,
    timeout: int = 180,
) -> tuple[Image.Image, dict[str, object]]:
    prompt = build_master_prompt()
    endpoint = api_base.rstrip("/") + "/images/edits"
    attempts: list[dict[str, object]] = []
    last_error = "unknown error"
    for attempt in range(1, 4):
        try:
            raw_image, payload = call_openai_image_endpoint(
                endpoint=endpoint,
                prompt=prompt,
                model=model,
                api_key=api_key,
                timeout=timeout,
                image=base_image,
            )
            raw_image.save(out_dir / "raw_master_response.png")
            pixel_master = postprocess_pixel_master(raw_image)
            pixel_master.save(out_dir / "pixel_master.png")
            (out_dir / "pixel_master_prompt.txt").write_text(prompt + "\n", encoding="utf-8")
            return pixel_master, {
                "status": "ok",
                "prompt_file": "pixel_master_prompt.txt",
                "endpoint": endpoint,
                "attempts": attempts + [{"attempt": attempt, "status": "ok"}],
                "response_summary": {"created": payload.get("created"), "data_count": len(payload.get("data") or [])},
            }
        except Exception as exc:  # noqa: BLE001
            last_error = str(exc)
            attempts.append({"attempt": attempt, "status": "error", "error": last_error})
    raise PartialGenerationError(
        f"pixel master generation failed: {last_error}",
        {"status": "error", "attempts": attempts, "endpoint": endpoint, "prompt": prompt},
    )


def mock_generate_grid64_atlas(reference_image: Image.Image) -> Image.Image:
    cell = 96
    gutter = 12
    width = gutter + GRID64_COLUMNS * (cell + gutter)
    height = gutter + GRID64_ROWS * (cell + gutter)
    atlas = Image.new("RGBA", (width, height), (255, 255, 255, 255))
    frame_index = 0
    for row in range(GRID64_ROWS):
        for col in range(GRID64_COLUMNS):
            sprite = reference_image.copy()
            if row == 0:
                sprite = add_overlay(sprite, (120, 150, 255, 18))
                sprite = shift_image(sprite, dy=frame_index % 2)
            elif row == 1:
                sprite = shift_image(sprite, dx=(col % 3) - 1)
            elif row == 2:
                sprite = ImageEnhance.Contrast(sprite).enhance(1.1)
                sprite = shift_image(sprite, dy=col % 2)
            elif row == 3:
                sprite = add_outline(sprite, (255, 240, 160, 130))
            elif row == 4:
                sprite = add_overlay(sprite, (255, 220, 90, 24))
                sprite = shift_image(sprite, dy=-1 if col % 2 == 0 else 1)
            elif row == 5:
                sprite = add_overlay(sprite, (110, 150, 255, 22))
                sprite = shift_image(sprite, dx=-1 if col % 2 == 0 else 1)
            elif row == 6:
                sprite = add_overlay(sprite, (255, 120, 160, 20))
                sprite = add_outline(sprite, (255, 180, 205, 160))
            else:
                sprite = shift_image(sprite, dx=(col % 2), dy=(col // 2) % 2)
            sprite = sprite.resize((cell, cell), Image.Resampling.NEAREST)
            x = gutter + col * (cell + gutter)
            y = gutter + row * (cell + gutter)
            atlas.alpha_composite(sprite, (x, y))
            frame_index += 1
    return atlas


def generate_grid64_atlas(
    reference_image: Image.Image,
    *,
    mode: str,
    api_key: str | None,
    api_base: str | None,
    model: str | None,
    out_dir: Path,
    timeout: int = 300,
) -> tuple[Image.Image, dict[str, object]]:
    prompt = build_grid64_prompt()
    (out_dir / "grid64_prompt.txt").write_text(prompt + "\n", encoding="utf-8")
    if mode == "mock":
        atlas = mock_generate_grid64_atlas(reference_image)
        atlas.save(out_dir / "grid64_raw.png")
        return atlas, {
            "status": "ok",
            "mode": "mock",
            "prompt_file": "grid64_prompt.txt",
            "attempts": [{"attempt": 1, "status": "ok"}],
        }

    if not api_key or not api_base or not model:
        raise GenerationError("openai mode requires --api-key, --api-base, and --model")

    endpoint = api_base.rstrip("/") + "/images/edits"
    attempts: list[dict[str, object]] = []
    last_error = "unknown error"
    for attempt in range(1, 4):
        try:
            atlas, payload = call_openai_image_endpoint(
                endpoint=endpoint,
                prompt=prompt,
                model=model,
                api_key=api_key,
                timeout=timeout,
                image=reference_image,
            )
            atlas.save(out_dir / "grid64_raw.png")
            return atlas, {
                "status": "ok",
                "endpoint": endpoint,
                "prompt_file": "grid64_prompt.txt",
                "attempts": attempts + [{"attempt": attempt, "status": "ok"}],
                "response_summary": {"created": payload.get("created"), "data_count": len(payload.get("data") or [])},
            }
        except Exception as exc:  # noqa: BLE001
            last_error = str(exc)
            attempts.append({"attempt": attempt, "status": "error", "error": last_error})

    raise PartialGenerationError(
        f"grid64 generation failed: {last_error}",
        {"status": "error", "attempts": attempts, "prompt": prompt, "endpoint": endpoint},
    )


def index_image_to_palette(image: Image.Image, palette: list[tuple[int, int, int]]) -> Image.Image:
    rgba = image.convert("RGBA")
    indexed = Image.new("P", rgba.size, TRANSPARENT_INDEX)
    flat_palette: list[int] = []
    for rgb in palette:
        flat_palette.extend(rgb)
    flat_palette.extend([0] * (256 * 3 - len(flat_palette)))
    indexed.putpalette(flat_palette)

    src = rgba.load()
    dst = indexed.load()
    width, height = rgba.size
    for y in range(height):
        for x in range(width):
            r, g, b, a = src[x, y]
            if a < 128:
                dst[x, y] = TRANSPARENT_INDEX
                continue
            best_index = 0
            best_distance: int | None = None
            for idx, color in enumerate(palette[:TRANSPARENT_INDEX]):
                dr = r - color[0]
                dg = g - color[1]
                db = b - color[2]
                distance = dr * dr + dg * dg + db * db
                if best_distance is None or distance < best_distance:
                    best_distance = distance
                    best_index = idx
            dst[x, y] = best_index
    return indexed


def build_shared_palette(frames: list[Image.Image]) -> tuple[list[Image.Image], list[tuple[int, int, int]]]:
    opaque_pixels: list[tuple[int, int, int]] = []
    for frame in frames:
        for r, g, b, a in frame.convert("RGBA").getdata():
            if a >= 128:
                opaque_pixels.append((r, g, b))

    if not opaque_pixels:
        palette = [(0, 0, 0)] * TRANSPARENT_INDEX + [(0, 0, 0)]
        return [Image.new("P", SPRITE_SIZE, TRANSPARENT_INDEX) for _ in frames], palette

    strip = Image.new("RGB", (len(opaque_pixels), 1))
    strip.putdata(opaque_pixels)
    paletted = strip.quantize(colors=TRANSPARENT_INDEX, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    palette_bytes = paletted.getpalette()[: TRANSPARENT_INDEX * 3]
    palette = [
        (palette_bytes[i], palette_bytes[i + 1], palette_bytes[i + 2])
        for i in range(0, len(palette_bytes), 3)
    ]
    while len(palette) < TRANSPARENT_INDEX:
        palette.append((0, 0, 0))
    palette.append((0, 0, 0))

    indexed_frames = [index_image_to_palette(frame, palette) for frame in frames]
    return indexed_frames, palette


def rgb_to_rgb565(rgb: tuple[int, int, int]) -> int:
    r, g, b = rgb
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def pack_indices_5bit(indices: list[int]) -> bytes:
    buffer = 0
    bits = 0
    output = bytearray()
    for value in indices:
        buffer |= (value & 0x1F) << bits
        bits += INDEX_BITS
        while bits >= 8:
            output.append(buffer & 0xFF)
            buffer >>= 8
            bits -= 8
    if bits:
        output.append(buffer & 0xFF)
    return bytes(output)


def indexed_to_rgba(frame: Image.Image, palette: list[tuple[int, int, int]]) -> Image.Image:
    image = Image.new("RGBA", frame.size, (0, 0, 0, 0))
    for idx, value in enumerate(frame.getdata()):
        x = idx % frame.width
        y = idx // frame.width
        if value == TRANSPARENT_INDEX:
            image.putpixel((x, y), (0, 0, 0, 0))
        else:
            image.putpixel((x, y), (*palette[value], 255))
    return image


def render_preview_sheet(indexed_frames: list[Image.Image], output_path: Path, palette: list[tuple[int, int, int]] | None = None) -> None:
    palette = palette or [(0, 0, 0)] * PALETTE_SIZE
    frames = [indexed_to_rgba(frame, palette) for frame in indexed_frames]
    render_grid64_preview(frames, output_path)


def render_palette_preview(palette: list[tuple[int, int, int]], output_path: Path) -> None:
    swatch = 28
    columns = 8
    rows = math.ceil(len(palette) / columns)
    preview = Image.new("RGBA", (columns * swatch, rows * swatch), (18, 18, 18, 255))
    for idx, color in enumerate(palette):
        tile = Image.new("RGBA", (swatch, swatch), (*color, 255))
        if idx == TRANSPARENT_INDEX:
            tile = Image.new("RGBA", (swatch, swatch), (255, 255, 255, 255))
            for y in range(0, swatch, 8):
                for x in range(0, swatch, 8):
                    if (x // 8 + y // 8) % 2 == 0:
                        for dy in range(8):
                            for dx in range(8):
                                px = x + dx
                                py = y + dy
                                if px < swatch and py < swatch:
                                    tile.putpixel((px, py), (220, 220, 220, 255))
        preview.alpha_composite(tile, ((idx % columns) * swatch, (idx // columns) * swatch))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    preview.save(output_path)


def export_palette_json(palette: list[tuple[int, int, int]], output_path: Path) -> None:
    data = []
    for index, color in enumerate(palette):
        data.append(
            {
                "index": index,
                "rgb": list(color),
                "rgb565": rgb_to_rgb565(color),
                "is_transparent": index == TRANSPARENT_INDEX,
            }
        )
    output_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def export_frames_bin(indexed_frames: list[Image.Image], output_path: Path) -> None:
    output = bytearray()
    for frame in indexed_frames:
        output.extend(pack_indices_5bit(list(frame.getdata())))
    output_path.write_bytes(bytes(output))


def build_state_entries() -> list[dict[str, object]]:
    timing_lookup = {state.name: state for state in STATE_SPECS}
    entries: list[dict[str, object]] = []
    for row_spec in GRID64_ROW_SPECS:
        state_name = str(row_spec["state"])
        start_frame = int(row_spec["row"]) * GRID64_COLUMNS
        spec = timing_lookup.get(state_name)
        code = spec.code if spec else "S07"
        entries.append(
            {
                "code": code,
                "name": state_name,
                "start_frame": start_frame,
                "frame_count": GRID64_COLUMNS,
                "is_bonus": bool(row_spec["is_bonus"]),
                "is_looping": False if state_name == "bonus" else bool(spec.is_looping if spec else True),
                "is_transient": bool(spec.is_transient) if spec else False,
                "frame_interval_ms": int(spec.frame_interval_ms) if spec else 180,
                "hold_on_last_frame_ms": int(spec.hold_on_last_frame_ms) if spec else 0,
            }
        )
    return entries


def export_manifest(pack_id: str, output_path: Path) -> dict[str, object]:
    manifest = {
        "pack_id": pack_id,
        "version": PACK_VERSION,
        "frame_width": SPRITE_SIZE[0],
        "frame_height": SPRITE_SIZE[1],
        "frame_count": GRID64_FRAME_COUNT,
        "palette_size": PALETTE_SIZE,
        "transparent_index": TRANSPARENT_INDEX,
        "default_scale_hint": DEFAULT_SCALE_HINT,
        "index_bits": INDEX_BITS,
        "grid_rows": GRID64_ROWS,
        "grid_columns": GRID64_COLUMNS,
        "states": build_state_entries(),
        "gif_groups": build_gif_groups(),
        "frame_semantics_file": "frame_semantics.json",
        "bonus_range": {"start_frame": 56, "frame_count": 8},
    }
    output_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def export_arduino_pack(
    manifest: dict[str, object],
    palette: list[tuple[int, int, int]],
    indexed_frames: list[Image.Image],
    output_path: Path,
) -> None:
    bytes_per_frame = len(pack_indices_5bit([0] * (SPRITE_SIZE[0] * SPRITE_SIZE[1])))
    lines = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"static const uint16_t BUDDY_PACK_FRAME_WIDTH = {SPRITE_SIZE[0]};",
        f"static const uint16_t BUDDY_PACK_FRAME_HEIGHT = {SPRITE_SIZE[1]};",
        f"static const uint16_t BUDDY_PACK_FRAME_COUNT = {GRID64_FRAME_COUNT};",
        f"static const uint16_t BUDDY_PACK_FRAME_BYTES = {bytes_per_frame};",
        f"static const uint8_t BUDDY_PACK_PALETTE_SIZE = {PALETTE_SIZE};",
        f"static const uint8_t BUDDY_PACK_TRANSPARENT_INDEX = {TRANSPARENT_INDEX};",
        "",
    ]
    palette_values = ", ".join(f"0x{rgb_to_rgb565(color):04X}" for color in palette)
    lines.append(f"static const uint16_t PALETTE_RGB565[{len(palette)}] PROGMEM = {{{palette_values}}};")
    lines.append("")

    for entry in manifest["states"]:
        state_name = str(entry["name"]).upper()
        lines.append(f"static const uint8_t BUDDY_STATE_{state_name}_START = {entry['start_frame']};")
        lines.append(f"static const uint8_t BUDDY_STATE_{state_name}_COUNT = {entry['frame_count']};")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("  uint8_t start_frame;")
    lines.append("  uint8_t frame_count;")
    lines.append("} BuddyFrameRange;")
    lines.append("")
    lines.append(f"static const BuddyFrameRange BUDDY_STATE_RANGES[{len(manifest['states'])}] PROGMEM = {{")
    for entry in manifest["states"]:
        lines.append(f"  {{{entry['start_frame']}, {entry['frame_count']}}},")
    lines.append("};")
    lines.append("")

    for idx, frame in enumerate(indexed_frames):
        packed = pack_indices_5bit(list(frame.getdata()))
        lines.append(f"static const uint8_t BUDDY_FRAME_{idx:02d}[{len(packed)}] PROGMEM = {{")
        for start in range(0, len(packed), 20):
            chunk = packed[start : start + 20]
            lines.append("  " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
        lines.append("};")
        lines.append("")

    lines.append(f"static const uint8_t* const BUDDY_PACK_FRAMES[{len(indexed_frames)}] PROGMEM = {{")
    lines.append("  " + ", ".join(f"BUDDY_FRAME_{idx:02d}" for idx in range(len(indexed_frames))))
    lines.append("};")
    lines.append("")
    lines.append("static inline uint8_t buddy_pack_palette_index(const uint8_t* frame, uint16_t pixel_index) {")
    lines.append("  const uint32_t bit_index = pixel_index * 5U;")
    lines.append("  const uint16_t byte_index = bit_index / 8U;")
    lines.append("  const uint8_t bit_offset = bit_index % 8U;")
    lines.append("  uint32_t window = 0;")
    lines.append("  window |= (uint32_t)pgm_read_byte(frame + byte_index);")
    lines.append("  window |= ((uint32_t)pgm_read_byte(frame + byte_index + 1U)) << 8U;")
    lines.append("  window |= ((uint32_t)pgm_read_byte(frame + byte_index + 2U)) << 16U;")
    lines.append("  return (window >> bit_offset) & 0x1FU;")
    lines.append("}")
    lines.append("")
    lines.append(f"// Bonus frames: {json.dumps(manifest['bonus_range'])}")
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_run_report(output_path: Path, report: dict[str, object]) -> None:
    output_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Build a Buddy 64-frame pet pack from one input image.")
    parser.add_argument("--input", required=True, help="Base character image")
    parser.add_argument("--out-dir", required=True, help="Output directory")
    parser.add_argument("--pack-id", default="buddy-pack", help="Pack identifier")
    parser.add_argument("--model", default=os.getenv("OPENAI_IMAGE_MODEL", "gpt-image-2"), help="OpenAI image model")
    parser.add_argument("--api-key", default=os.getenv("OPENAI_API_KEY"), help="OpenAI API key")
    parser.add_argument("--api-base", default=os.getenv("OPENAI_BASE_URL"), help="OpenAI API base URL")
    parser.add_argument("--mode", choices=["openai", "mock"], default="openai", help="Generation backend")
    parser.add_argument("--phase", choices=["master-only", "full"], default="full", help="Generate just the pixel master or the full pack")
    return parser


def run(args: argparse.Namespace) -> Path:
    if args.mode == "openai":
        missing = [
            flag
            for flag, value in (
                ("--api-key", args.api_key),
                ("--api-base", args.api_base),
                ("--model", args.model),
            )
            if not value
        ]
        if missing:
            raise SystemExit(f"openai mode requires: {', '.join(missing)}")
        args.api_base = normalize_api_base(args.api_base)

    out_dir = Path(args.out_dir)
    frames_dir = out_dir / "frames_64"
    gifs_dir = out_dir / "gifs"
    ensure_dir(out_dir)
    ensure_dir(frames_dir)
    ensure_dir(gifs_dir)

    base_image = normalize_base_image(Path(args.input))
    base_image.save(out_dir / "base_normalized.png")
    run_report: dict[str, object] = {
        "pack_id": args.pack_id,
        "mode": args.mode,
        "model": args.model,
        "api_base": args.api_base,
        "input": str(Path(args.input).resolve()),
        "generated_at": datetime.now(UTC).isoformat(),
        "phase": args.phase,
        "steps": {},
    }

    if args.mode == "mock":
        pixel_master = sharpen_pixel_art(base_image)
        pixel_master.save(out_dir / "pixel_master.png")
        (out_dir / "pixel_master_prompt.txt").write_text("mock master generation\n", encoding="utf-8")
        run_report["steps"]["pixel_master"] = {"status": "ok", "mode": "mock"}
    else:
        try:
            pixel_master, master_report = generate_pixel_master(
                base_image=base_image,
                api_key=args.api_key,
                api_base=args.api_base,
                model=args.model,
                out_dir=out_dir,
            )
            run_report["steps"]["pixel_master"] = master_report
        except PartialGenerationError as exc:
            run_report["steps"]["pixel_master"] = exc.report
            write_run_report(out_dir / "run_report.json", run_report)
            raise SystemExit(str(exc))

    if args.phase == "master-only":
        write_run_report(out_dir / "run_report.json", run_report)
        return out_dir

    try:
        atlas, grid_report = generate_grid64_atlas(
            pixel_master,
            mode=args.mode,
            api_key=args.api_key,
            api_base=args.api_base,
            model=args.model,
            out_dir=out_dir,
        )
        run_report["steps"]["grid64"] = grid_report
    except PartialGenerationError as exc:
        run_report["steps"]["grid64"] = exc.report
        write_run_report(out_dir / "run_report.json", run_report)
        raise SystemExit(str(exc))

    frames, bg_removed, slice_report = slice_grid64_atlas(atlas, postprocess_pixel_master)
    if len(frames) != GRID64_FRAME_COUNT:
        run_report["steps"]["slice"] = {"status": "error", "frame_count": len(frames), "report": slice_report}
        write_run_report(out_dir / "run_report.json", run_report)
        raise SystemExit(f"grid64 slicing produced {len(frames)} frames instead of {GRID64_FRAME_COUNT}")

    bg_removed.save(out_dir / "grid64_bg_removed.png")
    for index, frame in enumerate(frames):
        frame.save(frames_dir / f"F{index:02d}.png")
    render_grid64_preview(frames, out_dir / "preview_grid64_sliced.png")
    semantics = export_frame_semantics(out_dir / "frame_semantics.json")
    gif_outputs = export_state_gifs(frames, gifs_dir)
    run_report["steps"]["slice"] = {
        "status": "ok",
        "frame_count": len(frames),
        "grid_rows": GRID64_ROWS,
        "grid_columns": GRID64_COLUMNS,
        "sparse_frames": [cell["frame_id"] for cell in slice_report["cells"] if cell["flag_sparse"]],
        "report_file": "frame_semantics.json",
    }

    indexed_frames, palette = build_shared_palette(frames)
    export_frames_bin(indexed_frames, out_dir / "frames_indexed.bin")
    export_palette_json(palette, out_dir / "palette.json")
    render_palette_preview(palette, out_dir / "palette_preview.png")
    render_preview_sheet(indexed_frames, out_dir / "preview_sheet.png", palette)
    manifest = export_manifest(args.pack_id, out_dir / "manifest.json")
    export_arduino_pack(manifest, palette, indexed_frames, out_dir / "arduino_pack.h")

    run_report["steps"]["pack"] = {
        "status": "ok",
        "frame_count": GRID64_FRAME_COUNT,
        "palette_size": len(palette),
        "transparent_index": TRANSPARENT_INDEX,
        "gif_count": len(gif_outputs),
        "gif_files": {name: str(Path(path).name) for name, path in gif_outputs.items()},
    }
    run_report["outputs"] = {
        "pixel_master": "pixel_master.png",
        "grid64_raw": "grid64_raw.png",
        "grid64_bg_removed": "grid64_bg_removed.png",
        "preview_grid64_sliced": "preview_grid64_sliced.png",
        "frame_semantics": "frame_semantics.json",
        "manifest": "manifest.json",
        "palette": "palette.json",
        "frames_indexed_bin": "frames_indexed.bin",
        "arduino_pack": "arduino_pack.h",
        "gifs_dir": "gifs",
        "frames_dir": "frames_64",
    }
    write_run_report(out_dir / "run_report.json", run_report)
    return out_dir
