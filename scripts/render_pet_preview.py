from pathlib import Path
import re

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "pet" / "demo_pet_frames.h"
OUTPUT = ROOT / "docs" / "device-client" / "pet-preview-sheet.png"

PALETTE = {
    ".": (0, 0, 0, 0),
    "n": (57, 103, 57, 255),
    "b": (123, 173, 107, 255),
    "t": (222, 219, 181, 255),
    "p": (218, 122, 122, 255),
    "w": (255, 255, 255, 255),
    "r": (209, 232, 120, 255),
    "y": (254, 224, 115, 255),
    "h": (251, 44, 96, 255),
}

STATE_NAMES = [
    "S00 sleep",
    "S01 idle",
    "S02 busy",
    "S03 attention",
    "S04 celebrate",
    "S05 dizzy",
    "S06 heart",
]

FRAME_COUNTS = [3, 5, 4, 4, 4, 3, 3]
PIXEL = 6
PADDING = 12
LABEL_HEIGHT = 24
BG = (250, 244, 231, 255)
TEXT = (42, 37, 30, 255)
CARD = (238, 229, 208, 255)


def parse_frames():
    text = HEADER.read_text(encoding="utf-8")
    block = re.search(r"constexpr DemoFrame kDemoFrames\[] = \{(.*?)\n\};", text, re.S)
    if not block:
      raise RuntimeError("Could not find kDemoFrames block")

    frame_rows = []
    for chunk in re.finditer(r'\{\{(.*?)\}\}', block.group(1), re.S):
        rows = re.findall(r'"([.nbtpwryh]{32})"', chunk.group(1))
        if len(rows) != 32:
            raise RuntimeError(f"Bad frame row count: expected 32, got {len(rows)}")
        frame_rows.append(rows)
    return frame_rows


def draw_frame(rows):
    sprite_size = len(rows)
    image = Image.new("RGBA", (sprite_size * PIXEL, sprite_size * PIXEL), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    for y, row in enumerate(rows):
        for x, key in enumerate(row):
            color = PALETTE[key]
            if color[3] == 0:
                continue
            draw.rectangle(
                (
                    x * PIXEL,
                    y * PIXEL,
                    (x + 1) * PIXEL - 1,
                    (y + 1) * PIXEL - 1,
                ),
                fill=color,
            )
    return image


def main():
    frames = parse_frames()
    max_frames = max(FRAME_COUNTS)
    sprite_size = len(frames[0][0])
    frame_w = sprite_size * PIXEL
    frame_h = sprite_size * PIXEL
    width = PADDING + max_frames * (frame_w + PADDING) + PADDING
    height = PADDING + len(STATE_NAMES) * (LABEL_HEIGHT + frame_h + PADDING) + PADDING

    sheet = Image.new("RGBA", (width, height), BG)
    draw = ImageDraw.Draw(sheet)

    frame_index = 0
    y = PADDING
    for state_name, frame_count in zip(STATE_NAMES, FRAME_COUNTS):
        draw.text((PADDING, y), state_name, fill=TEXT)
        y += LABEL_HEIGHT
        for col in range(max_frames):
            x = PADDING + col * (frame_w + PADDING)
            draw.rounded_rectangle(
                (x - 4, y - 4, x + frame_w + 4, y + frame_h + 4),
                radius=10,
                fill=CARD,
            )
            if col < frame_count:
                image = draw_frame(frames[frame_index])
                sheet.alpha_composite(image, (x, y))
                draw.text((x, y + frame_h + 4), f"F{col:02d}", fill=TEXT)
                frame_index += 1
        y += frame_h + PADDING + 8

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    main()
