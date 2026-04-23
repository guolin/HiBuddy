from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image

from .pipeline import call_openai_image_endpoint, ensure_dir, normalize_api_base


GRID_SIZE = 8


def build_grid64_prompt() -> str:
    return (
        "Use case: stylized-concept\n"
        "Asset type: one single large pixel art sprite atlas for Buddy V1\n"
        "Primary request: generate exactly 64 frame cells arranged in a strict 8 by 8 grid, every cell occupied by one sprite of the same octopus-cat character\n"
        "Subject: exactly the same single octopus-cat identity as the reference pixel master image\n"
        "Style/medium: true pixel art atlas, crisp square pixels, hard edges, retro handheld game sprite look, no anti-aliasing\n"
        "Composition/framing: each of the 64 cells has the same size; each cell contains one centered full-body sprite; leave a clearly visible empty gutter between cells; no sprite or effect may cross into adjacent cells; transparent background only; no white background; no text; no labels; no border frame\n"
        "Color palette: limited palette around 32 colors or fewer, simple readable shading, no gradients, no texture noise\n"
        "Cell plan:\n"
        "Row 1: sleep closed-eyes, sleep deeper, sleep curled, sleep drifting, sleep calm, sleep tiny breathe, sleep rest tweak, sleep blink\n"
        "Row 2: idle neutral, idle soft smile, idle blink, idle tilt left, idle tilt right, idle tiny bounce, idle look left, idle look right\n"
        "Row 3: busy focused, busy stronger focus, busy typing mood, busy tool-holding mood, busy concentrated, busy determined, busy glance down, busy quick-check\n"
        "Row 4: attention alert, attention question, attention listening, attention raise head, attention wait, attention tiny surprise, attention signal, attention pause\n"
        "Row 5: celebrate smile, celebrate bigger smile, celebrate bounce, celebrate confetti, celebrate arms up, celebrate proud, celebrate sparkle, celebrate cheer\n"
        "Row 6: dizzy spiral eyes, dizzy tilted, dizzy wobble, dizzy recover, dizzy confused, dizzy shake-off, dizzy dazed, dizzy weak smile\n"
        "Row 7: heart small heart, heart hug feeling, heart blush, heart welcome, heart reward, heart floating hearts, heart grateful, heart happy-love\n"
        "Row 8: bonus neutral, bonus wink, bonus sleepy, bonus curious, bonus excited, bonus focused, bonus dizzy-lite, bonus heart-lite\n"
        "Constraints: keep same silhouette, same face layout, same proportions, same tentacle count, same black hood-like head shape, same peach face, same cute identity; each cell must be visibly different from neighboring cells; all 64 cells must be present and filled; preserve a visibly regular grid layout"
    )


def run(args: argparse.Namespace) -> Path:
    api_base = normalize_api_base(args.api_base)
    out_dir = Path(args.out_dir)
    ensure_dir(out_dir)

    reference = Image.open(args.reference).convert("RGBA")
    reference.save(out_dir / "reference_pixel_master.png")

    prompt = build_grid64_prompt()
    atlas, payload = call_openai_image_endpoint(
        endpoint=api_base + "/images/edits",
        prompt=prompt,
        model=args.model,
        api_key=args.api_key,
        timeout=300,
        image=reference,
    )
    atlas.save(out_dir / "grid64_raw.png")

    report = {
        "api_base": api_base,
        "model": args.model,
        "grid_size": GRID_SIZE,
        "prompt": prompt,
        "response_summary": {"created": payload.get("created"), "data_count": len(payload.get("data") or [])},
    }
    (out_dir / "grid64_report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return out_dir


def main() -> None:
    parser = argparse.ArgumentParser(description="One-shot 8x8 / 64-cell grid experiment for Buddy.")
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
