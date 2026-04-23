from __future__ import annotations

from dataclasses import dataclass


SPRITE_SIZE = (64, 64)
PALETTE_SIZE = 32
PACK_VERSION = "1.0.0"
DEFAULT_SCALE_HINT = 2
INDEX_BITS = 5
TRANSPARENT_INDEX = PALETTE_SIZE - 1
GRID64_COLUMNS = 8
GRID64_ROWS = 8
GRID64_FRAME_COUNT = GRID64_COLUMNS * GRID64_ROWS


@dataclass(frozen=True)
class StateSpec:
    code: str
    name: str
    frame_count: int
    is_looping: bool
    is_transient: bool
    loop_mode: str
    frame_interval_ms: int
    hold_on_last_frame_ms: int
    motion_profile: dict[str, int]
    trigger_rule: str
    variants: list[str]
    prompt: str
    expression_notes: str
    pose_notes: str


STATE_SPECS: list[StateSpec] = [
    StateSpec(
        code="S00",
        name="sleep",
        frame_count=4,
        is_looping=True,
        is_transient=False,
        loop_mode="ping_pong",
        frame_interval_ms=420,
        hold_on_last_frame_ms=180,
        motion_profile={
            "min_offset_x": 0,
            "max_offset_x": 0,
            "min_offset_y": 0,
            "max_offset_y": 2,
            "min_motion_interval_ms": 700,
            "max_motion_interval_ms": 1200,
            "pause_chance_pct": 24,
            "variant_chance_pct": 10,
            "rare_action_chance_pct": 5,
        },
        trigger_rule="energy_low",
        variants=["base", "rest_tweak"],
        prompt="sleeping peacefully, low-energy nap pose, subtle breathing, eyes closed",
        expression_notes="eyes fully closed, mouth relaxed, soft sleepy feeling that is visibly different from idle",
        pose_notes="body calmer and slightly drooped, no excited motion",
    ),
    StateSpec(
        code="S01",
        name="idle",
        frame_count=6,
        is_looping=True,
        is_transient=False,
        loop_mode="loop",
        frame_interval_ms=260,
        hold_on_last_frame_ms=120,
        motion_profile={
            "min_offset_x": -2,
            "max_offset_x": 2,
            "min_offset_y": 0,
            "max_offset_y": 4,
            "min_motion_interval_ms": 500,
            "max_motion_interval_ms": 950,
            "pause_chance_pct": 30,
            "variant_chance_pct": 20,
            "rare_action_chance_pct": 12,
        },
        trigger_rule="idle_variants",
        variants=["base", "blink", "tilt"],
        prompt="friendly idle pose, alive and relaxed, soft breathing, occasional blink or tiny head tilt",
        expression_notes="friendly neutral expression, open eyes, soft smile, occasional blink-ready face",
        pose_notes="balanced centered posture, tiny breathing motion only",
    ),
    StateSpec(
        code="S02",
        name="busy",
        frame_count=5,
        is_looping=True,
        is_transient=False,
        loop_mode="loop",
        frame_interval_ms=150,
        hold_on_last_frame_ms=0,
        motion_profile={
            "min_offset_x": -1,
            "max_offset_x": 1,
            "min_offset_y": 0,
            "max_offset_y": 2,
            "min_motion_interval_ms": 280,
            "max_motion_interval_ms": 520,
            "pause_chance_pct": 18,
            "variant_chance_pct": 18,
            "rare_action_chance_pct": 6,
        },
        trigger_rule="busy_focus",
        variants=["base", "focus"],
        prompt="focused working pose, stable and concentrated, slight lean forward, tighter rhythm",
        expression_notes="more focused eyes than idle, concentrated mouth, visibly more serious than idle",
        pose_notes="slight lean forward, tighter compact pose",
    ),
    StateSpec(
        code="S03",
        name="attention",
        frame_count=4,
        is_looping=True,
        is_transient=False,
        loop_mode="loop",
        frame_interval_ms=180,
        hold_on_last_frame_ms=60,
        motion_profile={
            "min_offset_x": -1,
            "max_offset_x": 1,
            "min_offset_y": 0,
            "max_offset_y": 2,
            "min_motion_interval_ms": 240,
            "max_motion_interval_ms": 420,
            "pause_chance_pct": 22,
            "variant_chance_pct": 10,
            "rare_action_chance_pct": 4,
        },
        trigger_rule="requires_user",
        variants=["base"],
        prompt="asking for attention, head slightly raised, waiting for the user to notice",
        expression_notes="alert eyes, expecting attention, mild reminder feeling",
        pose_notes="head raised or posture slightly lifted, clear waiting stance",
    ),
    StateSpec(
        code="S04",
        name="celebrate",
        frame_count=5,
        is_looping=False,
        is_transient=True,
        loop_mode="short_loop",
        frame_interval_ms=130,
        hold_on_last_frame_ms=120,
        motion_profile={
            "min_offset_x": -2,
            "max_offset_x": 2,
            "min_offset_y": 0,
            "max_offset_y": 3,
            "min_motion_interval_ms": 180,
            "max_motion_interval_ms": 320,
            "pause_chance_pct": 14,
            "variant_chance_pct": 0,
            "rare_action_chance_pct": 0,
        },
        trigger_rule="task_done",
        variants=["base"],
        prompt="happy celebration, upbeat bounce, success moment, short burst of excitement",
        expression_notes="clearly happy face, bigger smile than idle, joyful celebratory feeling",
        pose_notes="uplifted energetic pose, tiny bounce or raised gesture",
    ),
    StateSpec(
        code="S05",
        name="dizzy",
        frame_count=4,
        is_looping=False,
        is_transient=True,
        loop_mode="short_loop",
        frame_interval_ms=200,
        hold_on_last_frame_ms=80,
        motion_profile={
            "min_offset_x": -2,
            "max_offset_x": 2,
            "min_offset_y": 0,
            "max_offset_y": 2,
            "min_motion_interval_ms": 120,
            "max_motion_interval_ms": 240,
            "pause_chance_pct": 8,
            "variant_chance_pct": 0,
            "rare_action_chance_pct": 0,
        },
        trigger_rule="error_or_rough_motion",
        variants=["base"],
        prompt="dizzy and unsteady, brief confused wobble, abnormal or offline feeling, but still cute",
        expression_notes="dizzy confused face, dazed eyes or woozy expression, clearly not idle",
        pose_notes="slightly off-balance or wobbling pose without changing character identity",
    ),
    StateSpec(
        code="S06",
        name="heart",
        frame_count=4,
        is_looping=False,
        is_transient=True,
        loop_mode="play_once",
        frame_interval_ms=140,
        hold_on_last_frame_ms=160,
        motion_profile={
            "min_offset_x": -1,
            "max_offset_x": 1,
            "min_offset_y": -1,
            "max_offset_y": 2,
            "min_motion_interval_ms": 140,
            "max_motion_interval_ms": 240,
            "pause_chance_pct": 0,
            "variant_chance_pct": 0,
            "rare_action_chance_pct": 0,
        },
        trigger_rule="friendly_interaction",
        variants=["base"],
        prompt="warm friendly reaction with heart-like affection, welcome back energy, positive reward moment",
        expression_notes="loving welcoming face, affectionate smile, positive reward feeling",
        pose_notes="slight upward buoyant pose, gentle positive gesture",
    ),
]


GRID64_ROW_SPECS: list[dict[str, object]] = [
    {
        "row": 0,
        "state": "sleep",
        "gif_group": "sleep",
        "is_bonus": False,
        "labels": [
            ("closed-eyes", "eyes fully closed, calm sleeping face"),
            ("deeper-sleep", "deeper sleep, heavier eyelids and lower energy"),
            ("curled", "slightly curled sleeping posture"),
            ("drifting", "gentle drifting sleep pose"),
            ("calm", "peaceful calm sleep expression"),
            ("tiny-breathe", "tiny breathing motion while asleep"),
            ("rest-tweak", "small sleep twitch or rest adjustment"),
            ("blink", "sleepy micro blink while mostly resting"),
        ],
    },
    {
        "row": 1,
        "state": "idle",
        "gif_group": "idle",
        "is_bonus": False,
        "labels": [
            ("neutral", "friendly neutral idle face"),
            ("soft-smile", "soft smile while idling"),
            ("blink", "idle blink expression"),
            ("tilt-left", "small head tilt to the left"),
            ("tilt-right", "small head tilt to the right"),
            ("tiny-bounce", "tiny buoyant bounce feeling"),
            ("look-left", "eyes glance left while staying relaxed"),
            ("look-right", "eyes glance right while staying relaxed"),
        ],
    },
    {
        "row": 2,
        "state": "busy",
        "gif_group": "busy",
        "is_bonus": False,
        "labels": [
            ("focused", "focused working face"),
            ("stronger-focus", "stronger focus and tighter eyes"),
            ("typing-mood", "busy typing or tapping mood"),
            ("tool-holding", "working with a tiny tool-like action"),
            ("concentrated", "steady concentrated work posture"),
            ("determined", "determined expression, clearly more serious than idle"),
            ("glance-down", "working while glancing downward"),
            ("quick-check", "quick check motion during work"),
        ],
    },
    {
        "row": 3,
        "state": "attention",
        "gif_group": "attention",
        "is_bonus": False,
        "labels": [
            ("alert", "alert attention face"),
            ("question", "questioning attention pose"),
            ("listening", "listening carefully for the user"),
            ("raise-head", "head raised to ask for attention"),
            ("wait", "waiting for user response"),
            ("tiny-surprise", "small surprised attention cue"),
            ("signal", "clear signal that attention is needed"),
            ("pause", "brief pause while waiting"),
        ],
    },
    {
        "row": 4,
        "state": "celebrate",
        "gif_group": "celebrate",
        "is_bonus": False,
        "labels": [
            ("smile", "happy celebration smile"),
            ("bigger-smile", "even bigger smile than the previous frame"),
            ("bounce", "celebration bounce motion"),
            ("confetti", "confetti or spark-like celebration decoration"),
            ("arms-up", "uplifted celebratory body language"),
            ("proud", "proud success expression"),
            ("sparkle", "sparkly celebration moment"),
            ("cheer", "full cheerful success pose"),
        ],
    },
    {
        "row": 5,
        "state": "dizzy",
        "gif_group": "dizzy",
        "is_bonus": False,
        "labels": [
            ("spiral-eyes", "spiral or dazed eyes, clearly dizzy"),
            ("tilted", "body tilted in a woozy way"),
            ("wobble", "wobbling dizziness motion"),
            ("recover", "trying to recover from dizziness"),
            ("confused", "confused dizzy face"),
            ("shake-off", "shaking off the dizzy feeling"),
            ("dazed", "dazed low-energy dizzy expression"),
            ("weak-smile", "weak smile while still dizzy"),
        ],
    },
    {
        "row": 6,
        "state": "heart",
        "gif_group": "heart",
        "is_bonus": False,
        "labels": [
            ("small-heart", "small heart reaction"),
            ("hug-feeling", "warm hug-like affectionate feeling"),
            ("blush", "cute blush with affection"),
            ("welcome", "warm welcoming expression"),
            ("reward", "positive reward moment"),
            ("floating-hearts", "floating heart decorations near the character"),
            ("grateful", "grateful affectionate expression"),
            ("happy-love", "happy love reaction, still readable at 64x64"),
        ],
    },
    {
        "row": 7,
        "state": "bonus",
        "gif_group": None,
        "is_bonus": True,
        "labels": [
            ("neutral", "bonus neutral extra frame"),
            ("wink", "bonus wink expression"),
            ("sleepy", "bonus sleepy extra frame"),
            ("curious", "bonus curious extra frame"),
            ("excited", "bonus excited extra frame"),
            ("focused", "bonus focused extra frame"),
            ("dizzy-lite", "bonus mild dizzy extra frame"),
            ("heart-lite", "bonus mild heart extra frame"),
        ],
    },
]


def build_grid64_frame_semantics() -> list[dict[str, object]]:
    semantics: list[dict[str, object]] = []
    frame_index = 0
    for row_spec in GRID64_ROW_SPECS:
        row = int(row_spec["row"])
        state = str(row_spec["state"])
        gif_group = row_spec["gif_group"]
        is_bonus = bool(row_spec["is_bonus"])
        labels = list(row_spec["labels"])
        for col, (label, description) in enumerate(labels):
            semantics.append(
                {
                    "frame_id": f"F{frame_index:02d}",
                    "frame_index": frame_index,
                    "grid_row": row,
                    "grid_col": col,
                    "state": state,
                    "label": label,
                    "description": description,
                    "gif_group": gif_group,
                    "is_bonus": is_bonus,
                }
            )
            frame_index += 1
    return semantics
