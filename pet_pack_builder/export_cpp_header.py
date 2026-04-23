from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


def rgb_to_rgb565(rgb: list[int]) -> int:
    r, g, b = rgb
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main() -> None:
    parser = argparse.ArgumentParser(description="Export a reference C++ header from a generated pet pack.")
    parser.add_argument("--pack-dir", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    pack_dir = Path(args.pack_dir)
    manifest = json.loads((pack_dir / "manifest.json").read_text(encoding="utf-8"))
    palette = json.loads((pack_dir / "palette.json").read_text(encoding="utf-8"))
    frame_bytes = (pack_dir / "frames_indexed.bin").read_bytes()
    pixel_count = manifest["frame_width"] * manifest["frame_height"]
    bytes_per_frame = math.ceil(pixel_count * manifest["index_bits"] / 8)

    lines = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f'// Generated from pack {manifest["pack_id"]}',
        f'static const uint16_t PET_PACK_FRAME_WIDTH = {manifest["frame_width"]};',
        f'static const uint16_t PET_PACK_FRAME_HEIGHT = {manifest["frame_height"]};',
        f'static const uint16_t PET_PACK_FRAME_COUNT = {manifest["frame_count"]};',
        f'static const uint16_t PET_PACK_FRAME_BYTES = {bytes_per_frame};',
        "",
    ]
    palette_values = ", ".join(f"0x{rgb_to_rgb565(entry['rgb']):04X}" for entry in palette)
    lines.append(f"static const uint16_t PET_PACK_PALETTE_RGB565[{len(palette)}] PROGMEM = {{{palette_values}}};")
    lines.append("")
    for index in range(manifest["frame_count"]):
        chunk = frame_bytes[index * bytes_per_frame : (index + 1) * bytes_per_frame]
        lines.append(f"static const uint8_t PET_PACK_FRAME_{index:02d}[{bytes_per_frame}] PROGMEM = {{")
        for start in range(0, len(chunk), 20):
            lines.append("  " + ", ".join(f"0x{value:02X}" for value in chunk[start : start + 20]) + ",")
        lines.append("};")
        lines.append("")
    lines.append(f"// states: {json.dumps(manifest['states'], ensure_ascii=False)}")
    Path(args.output).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()
