from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


def unpack_frame(frame_bytes: bytes, pixel_count: int) -> list[int]:
    values: list[int] = []
    bit_cursor = 0
    for _ in range(pixel_count):
        byte_index = bit_cursor // 8
        bit_offset = bit_cursor % 8
        window = frame_bytes[byte_index]
        if byte_index + 1 < len(frame_bytes):
            window |= frame_bytes[byte_index + 1] << 8
        if byte_index + 2 < len(frame_bytes):
            window |= frame_bytes[byte_index + 2] << 16
        values.append((window >> bit_offset) & 0x1F)
        bit_cursor += 5
    return values


def render_pack_preview(pack_dir: Path, output_path: Path) -> None:
    manifest = json.loads((pack_dir / "manifest.json").read_text(encoding="utf-8"))
    palette = json.loads((pack_dir / "palette.json").read_text(encoding="utf-8"))
    transparent_index = manifest.get("transparent_index", len(palette) - 1)
    frame_w = manifest["frame_width"]
    frame_h = manifest["frame_height"]
    pixel_count = frame_w * frame_h
    frame_bytes = math.ceil(pixel_count * manifest["index_bits"] / 8)
    all_bytes = (pack_dir / "frames_indexed.bin").read_bytes()
    state_entries = manifest["states"]
    pixel = 4
    padding = 16
    label_h = 24
    text_pad = 20
    max_frames = max(state["frame_count"] for state in state_entries)

    sheet = Image.new(
        "RGBA",
        (padding * 2 + max_frames * (frame_w * pixel + padding), padding * 2 + len(state_entries) * (label_h + frame_h * pixel + text_pad + padding)),
        (250, 244, 231, 255),
    )
    draw = ImageDraw.Draw(sheet)
    card = (238, 229, 208, 255)
    text = (42, 37, 30, 255)

    for state_index, state in enumerate(state_entries):
        y = padding + state_index * (label_h + frame_h * pixel + text_pad + padding)
        draw.text((padding, y), f'{state["code"]} {state["name"]}', fill=text)
        y += label_h
        for column in range(max_frames):
            x = padding + column * (frame_w * pixel + padding)
            draw.rounded_rectangle((x - 4, y - 4, x + frame_w * pixel + 4, y + frame_h * pixel + 4), radius=8, fill=card)
            if column >= state["frame_count"]:
                continue
            frame_index = state["start_frame"] + column
            chunk = all_bytes[frame_index * frame_bytes : (frame_index + 1) * frame_bytes]
            indices = unpack_frame(chunk, pixel_count)
            image = Image.new("RGBA", (frame_w, frame_h), (0, 0, 0, 0))
            for py in range(frame_h):
                for px in range(frame_w):
                    palette_index = indices[py * frame_w + px]
                    if palette_index == transparent_index:
                        image.putpixel((px, py), (0, 0, 0, 0))
                    else:
                        palette_entry = palette[palette_index]
                        image.putpixel((px, py), (*palette_entry["rgb"], 255))
            image = image.resize((frame_w * pixel, frame_h * pixel), Image.Resampling.NEAREST)
            sheet.alpha_composite(image, (x, y))
            draw.text((x, y + frame_h * pixel + 4), f"F{column:02d}", fill=text)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Render a preview sheet from a pet pack directory.")
    parser.add_argument("--pack-dir", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    render_pack_preview(Path(args.pack_dir), Path(args.output))


if __name__ == "__main__":
    main()
