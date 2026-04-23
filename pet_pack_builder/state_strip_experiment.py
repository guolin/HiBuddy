from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageOps

from .pipeline import (
    PALETTE_SIZE,
    SPRITE_SIZE,
    STATE_SPECS,
    build_shared_palette,
    call_openai_image_endpoint,
    ensure_dir,
    normalize_api_base,
    postprocess_pixel_master,
)
from .spritesheet_experiment import render_rgba_preview_sheet


def build_strip_prompt(frame_count: int, state_name: str, expression_notes: str, pose_notes: str) -> str:
    return (
        "Use case: stylized-concept\n"
        "Asset type: one single horizontal pixel art animation strip for Buddy V1\n"
        f"Primary request: generate exactly {frame_count} animation frames for the {state_name} state in one horizontal strip\n"
        "Subject: exactly the same octopus-cat character identity as the reference pixel master image\n"
        "Style/medium: true pixel art strip, crisp square pixels, hard edges, no anti-aliasing, retro handheld game sprite look\n"
        f"Composition/framing: the image contains exactly {frame_count} occupied frame cells in one row; each cell contains one full-body sprite; keep equal frame widths; keep transparent gutters or clear empty spacing between frames; every sprite must stay inside its own frame cell; transparent background only\n"
        f"Expression: {expression_notes}\n"
        f"Pose: {pose_notes}\n"
        "Constraints: keep same silhouette, same face layout, same proportions, same tentacle count, same black hood-like head shape, same peach face, same cute identity; every frame should be a usable animation frame for that same state; no white background; no border; no text; no watermark; around 32 colors or fewer"
    )


def find_strip_segments(image: Image.Image, expected_frames: int) -> list[tuple[int, int]]:
    image = image.convert("RGBA")
    width, height = image.size
    values: list[int] = []
    for x in range(width):
        c = 0
        for y in range(height):
            r, g, b, a = image.getpixel((x, y))
            if a == 0 or (r >= 220 and g >= 220 and b >= 220 and max(r, g, b) - min(r, g, b) <= 22):
                c += 1
        values.append(c)

    threshold = int(height * 0.985)
    gutters: list[tuple[int, int]] = []
    start: int | None = None
    for idx, value in enumerate(values):
        if value >= threshold and start is None:
            start = idx
        elif value < threshold and start is not None:
            gutters.append((start, idx - 1))
            start = None
    if start is not None:
        gutters.append((start, width - 1))

    if len(gutters) >= 2:
        segments: list[tuple[int, int]] = []
        left = gutters[0][1] + 1
        for gutter in gutters[1:]:
            right = gutter[0] - 1
            if right >= left:
                segments.append((left, right + 1))
            left = gutter[1] + 1
        if len(segments) == expected_frames:
            return segments

    step = width // expected_frames
    return [(i * step, (i + 1) * step if i < expected_frames - 1 else width) for i in range(expected_frames)]


def slice_strip(image: Image.Image, expected_frames: int) -> tuple[list[Image.Image], list[tuple[int, int]]]:
    image = image.convert("RGBA")
    segments = find_strip_segments(image, expected_frames)
    frames: list[Image.Image] = []
    for left, right in segments:
        cell = image.crop((left, 0, right, image.height)).convert("RGBA")
        frame = postprocess_pixel_master(cell)
        frame = ImageOps.contain(frame, SPRITE_SIZE, Image.Resampling.NEAREST)
        canvas = Image.new("RGBA", SPRITE_SIZE, (0, 0, 0, 0))
        x = (SPRITE_SIZE[0] - frame.width) // 2
        y = (SPRITE_SIZE[1] - frame.height) // 2
        canvas.paste(frame, (x, y), frame)
        frames.append(canvas)
    return frames, segments


def run(args: argparse.Namespace) -> Path:
    api_base = normalize_api_base(args.api_base)
    out_dir = Path(args.out_dir)
    strips_dir = out_dir / "raw_strips"
    frames_dir = out_dir / "sliced_frames"
    ensure_dir(out_dir)
    ensure_dir(strips_dir)
    ensure_dir(frames_dir)

    reference = Image.open(args.reference).convert("RGBA")
    reference.save(out_dir / "reference_pixel_master.png")

    all_frames: list[Image.Image] = []
    report_states: list[dict[str, object]] = []

    for state in STATE_SPECS:
        prompt = build_strip_prompt(state.frame_count, state.name, state.expression_notes, state.pose_notes)
        strip_image, payload = call_openai_image_endpoint(
            endpoint=api_base + "/images/edits",
            prompt=prompt,
            model=args.model,
            api_key=args.api_key,
            timeout=240,
            image=reference,
        )
        raw_path = strips_dir / f"{state.code}_{state.name}_strip.png"
        strip_image.save(raw_path)
        frames, segments = slice_strip(strip_image, state.frame_count)
        for idx, frame in enumerate(frames):
            frame.save(frames_dir / f"{state.code}_{state.name}_f{idx:02d}.png")
            all_frames.append(frame)
        report_states.append(
            {
                "code": state.code,
                "name": state.name,
                "frame_count": state.frame_count,
                "segments": segments,
                "prompt": prompt,
                "response_summary": {"created": payload.get("created"), "data_count": len(payload.get("data") or [])},
            }
        )

    render_rgba_preview_sheet(all_frames, out_dir / "preview_sheet_from_strips.png")
    _, palette = build_shared_palette(all_frames)
    report = {
        "api_base": api_base,
        "model": args.model,
        "palette_size": PALETTE_SIZE,
        "frame_count": len(all_frames),
        "states": report_states,
    }
    (out_dir / "strip_experiment_report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return out_dir


def main() -> None:
    parser = argparse.ArgumentParser(description="Experimental per-state strip generation for Buddy.")
    parser.add_argument("--reference", required=True, help="Path to pixel_master.png")
    parser.add_argument("--api-key", required=True, help="OpenAI compatible API key")
    parser.add_argument("--api-base", required=True, help="OpenAI compatible API base URL")
    parser.add_argument("--model", default="gpt-image-2", help="Image model")
    parser.add_argument("--out-dir", required=True, help="Output directory")
    args = parser.parse_args()
    output = run(args)
    print(output)


if __name__ == "__main__":
    main()
