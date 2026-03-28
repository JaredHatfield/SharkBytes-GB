#!/usr/bin/env python3

import json
import sys
from pathlib import Path


FPS = 60
FIX_SCALE = 16
MAX_HEARTS = 3
MAX_AIR = 10

FISH_KIND_MAP = {
    "clown": "LEVEL_FISH_KIND_CLOWN",
    "puffer": "LEVEL_FISH_KIND_PUFFER",
}

START_SIDE_MAP = {
    "left": 1,
    "right": 0,
}


def fail(message: str) -> None:
    raise ValueError(message)


def require_int(value, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        fail(f"{label} must be an integer")
    return value


def require_number(value, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        fail(f"{label} must be a number")
    return float(value)


def require_string(value, label: str) -> str:
    if not isinstance(value, str) or not value:
        fail(f"{label} must be a non-empty string")
    return value


def seconds_to_frames(value: float, label: str) -> int:
    if value <= 0:
        fail(f"{label} must be greater than 0")
    return max(1, int(round(value * FPS)))


def pixels_per_second_to_fixed(value: float, label: str) -> int:
    if value <= 0:
        fail(f"{label} must be greater than 0")
    return max(1, int(round((value * FIX_SCALE) / FPS)))


def validate_range(value: int, minimum: int, maximum: int, label: str) -> int:
    if value < minimum or value > maximum:
        fail(f"{label} must be between {minimum} and {maximum}")
    return value


def parse_fish_type(level_index: int, fish_index: int, raw: dict) -> dict:
    prefix = f"levels[{level_index}].fish.types[{fish_index}]"
    kind = require_string(raw.get("kind"), f"{prefix}.kind")
    if kind not in FISH_KIND_MAP:
        fail(f"{prefix}.kind must be one of: {', '.join(sorted(FISH_KIND_MAP))}")

    start_side = require_string(raw.get("start_side"), f"{prefix}.start_side")
    if start_side not in START_SIDE_MAP:
        fail(f"{prefix}.start_side must be one of: {', '.join(sorted(START_SIDE_MAP))}")

    return {
        "kind_symbol": FISH_KIND_MAP[kind],
        "spawn_y": validate_range(require_int(raw.get("spawn_y"), f"{prefix}.spawn_y"), 24, 119, f"{prefix}.spawn_y"),
        "minimum_x": validate_range(require_int(raw.get("min_x"), f"{prefix}.min_x"), 8, 152, f"{prefix}.min_x"),
        "maximum_x": validate_range(require_int(raw.get("max_x"), f"{prefix}.max_x"), 8, 160, f"{prefix}.max_x"),
        "speed_fixed": pixels_per_second_to_fixed(
            require_number(raw.get("speed_pixels_per_second"), f"{prefix}.speed_pixels_per_second"),
            f"{prefix}.speed_pixels_per_second",
        ),
        "lifetime_frames": seconds_to_frames(
            require_number(raw.get("lifetime_seconds"), f"{prefix}.lifetime_seconds"),
            f"{prefix}.lifetime_seconds",
        ),
        "animation_frames": seconds_to_frames(
            require_number(
                raw.get("animation_seconds_per_frame"),
                f"{prefix}.animation_seconds_per_frame",
            ),
            f"{prefix}.animation_seconds_per_frame",
        ),
        "start_from_left": START_SIDE_MAP[start_side],
    }


def parse_level(level_index: int, raw: dict) -> dict:
    prefix = f"levels[{level_index}]"
    name = require_string(raw.get("name"), f"{prefix}.name")
    implemented = raw.get("implemented")

    if not isinstance(implemented, bool):
        fail(f"{prefix}.implemented must be true or false")

    level = {
        "name": name,
        "implemented": 1 if implemented else 0,
        "starting_hearts": 0,
        "starting_air": 0,
        "goal_fish": 0,
        "player_animation_frames": 0,
        "max_fish_on_screen": 0,
        "fish_types": [],
        "air_enabled": 0,
        "air_gain": 0,
        "air_loss_frames": 0,
        "air_spawn_frames": 0,
        "air_rise_speed_fixed": 0,
    }

    if not implemented:
        return level

    level["starting_hearts"] = validate_range(
        require_int(raw.get("starting_hearts"), f"{prefix}.starting_hearts"),
        0,
        MAX_HEARTS,
        f"{prefix}.starting_hearts",
    )
    level["starting_air"] = validate_range(
        require_int(raw.get("starting_air"), f"{prefix}.starting_air"),
        0,
        MAX_AIR,
        f"{prefix}.starting_air",
    )
    level["goal_fish"] = validate_range(
        require_int(raw.get("goal_fish"), f"{prefix}.goal_fish"),
        1,
        MAX_AIR,
        f"{prefix}.goal_fish",
    )

    player = raw.get("player")
    if not isinstance(player, dict):
        fail(f"{prefix}.player must be an object")

    level["player_animation_frames"] = seconds_to_frames(
        require_number(
            player.get("animation_seconds_per_frame"),
            f"{prefix}.player.animation_seconds_per_frame",
        ),
        f"{prefix}.player.animation_seconds_per_frame",
    )

    air = raw.get("air")
    if not isinstance(air, dict):
        fail(f"{prefix}.air must be an object")

    air_enabled = air.get("enabled")
    if not isinstance(air_enabled, bool):
        fail(f"{prefix}.air.enabled must be true or false")

    level["air_enabled"] = 1 if air_enabled else 0

    if air_enabled:
        level["air_gain"] = validate_range(
            require_int(air.get("air_gain"), f"{prefix}.air.air_gain"),
            1,
            MAX_AIR,
            f"{prefix}.air.air_gain",
        )
        level["air_loss_frames"] = seconds_to_frames(
            require_number(air.get("loss_every_seconds"), f"{prefix}.air.loss_every_seconds"),
            f"{prefix}.air.loss_every_seconds",
        )
        level["air_spawn_frames"] = seconds_to_frames(
            require_number(air.get("spawn_every_seconds"), f"{prefix}.air.spawn_every_seconds"),
            f"{prefix}.air.spawn_every_seconds",
        )
        level["air_rise_speed_fixed"] = pixels_per_second_to_fixed(
            require_number(air.get("rise_pixels_per_second"), f"{prefix}.air.rise_pixels_per_second"),
            f"{prefix}.air.rise_pixels_per_second",
        )

    fish = raw.get("fish")
    if not isinstance(fish, dict):
        fail(f"{prefix}.fish must be an object")

    level["max_fish_on_screen"] = validate_range(
        require_int(fish.get("max_on_screen"), f"{prefix}.fish.max_on_screen"),
        1,
        MAX_AIR,
        f"{prefix}.fish.max_on_screen",
    )

    fish_types = fish.get("types")
    if not isinstance(fish_types, list) or not fish_types:
        fail(f"{prefix}.fish.types must be a non-empty array")

    for fish_index, fish_type in enumerate(fish_types):
        if not isinstance(fish_type, dict):
            fail(f"levels[{level_index}].fish.types[{fish_index}] must be an object")
        parsed = parse_fish_type(level_index, fish_index, fish_type)
        if parsed["minimum_x"] >= parsed["maximum_x"]:
            fail(f"levels[{level_index}].fish.types[{fish_index}] min_x must be less than max_x")
        level["fish_types"].append(parsed)

    return level


def render_header(level_count: int, max_fish_on_screen: int) -> str:
    return f"""#ifndef LEVELS_GENERATED_H
#define LEVELS_GENERATED_H

#include <gb/gb.h>

#define LEVEL_COUNT {level_count}u
#define PLAYER_MAX_HEALTH {MAX_HEARTS}u
#define PLAYER_MAX_AIR {MAX_AIR}u
#define MAX_CAPTURED_FISH {MAX_AIR}u
#define LEVELS_MAX_FISH_ON_SCREEN {max_fish_on_screen}u

typedef enum LevelFishKind {{
    LEVEL_FISH_KIND_CLOWN = 0,
    LEVEL_FISH_KIND_PUFFER
}} LevelFishKind;

typedef struct LevelFishTypeDefinition {{
    UINT8 kind;
    UINT8 spawn_y;
    UINT8 minimum_x;
    UINT8 maximum_x;
    UINT8 speed_fixed;
    UINT16 lifetime_frames;
    UINT16 animation_frames;
    UINT8 start_from_left;
}} LevelFishTypeDefinition;

typedef struct LevelAirDefinition {{
    UINT8 enabled;
    UINT8 air_gain;
    UINT16 loss_frames;
    UINT16 spawn_frames;
    UINT8 rise_speed_fixed;
}} LevelAirDefinition;

typedef struct LevelPlayerDefinition {{
    UINT16 animation_frames;
}} LevelPlayerDefinition;

typedef struct LevelDefinition {{
    const char *name;
    UINT8 implemented;
    UINT8 starting_hearts;
    UINT8 starting_air;
    UINT8 goal_fish;
    LevelPlayerDefinition player;
    UINT8 max_fish_on_screen;
    UINT8 fish_type_count;
    const LevelFishTypeDefinition *fish_types;
    LevelAirDefinition air;
}} LevelDefinition;

extern const LevelDefinition level_definitions[LEVEL_COUNT];

#endif
"""


def render_c(levels: list[dict], source_name: str) -> str:
    lines = [
        '#include "levels_generated.h"',
        "",
        f"/* Generated from {source_name} by scripts/levels_to_c.py. */",
        "",
    ]

    for index, level in enumerate(levels):
        fish_types = level["fish_types"]
        if fish_types:
            lines.append(f"static const LevelFishTypeDefinition level_{index}_fish_types[] = {{")
            for fish_type in fish_types:
                lines.append(
                    "    { %s, %uu, %uu, %uu, %uu, %uu, %uu, %uu },"
                    % (
                        fish_type["kind_symbol"],
                        fish_type["spawn_y"],
                        fish_type["minimum_x"],
                        fish_type["maximum_x"],
                        fish_type["speed_fixed"],
                        fish_type["lifetime_frames"],
                        fish_type["animation_frames"],
                        fish_type["start_from_left"],
                    )
                )
            lines.append("};")
            lines.append("")

    lines.append("const LevelDefinition level_definitions[LEVEL_COUNT] = {")

    for index, level in enumerate(levels):
        fish_pointer = f"level_{index}_fish_types" if level["fish_types"] else "0"
        lines.append(
            '    { "%s", %uu, %uu, %uu, %uu, { %uu }, %uu, %uu, %s, { %uu, %uu, %uu, %uu, %uu } },'
            % (
                level["name"].replace("\\", "\\\\").replace('"', '\\"'),
                level["implemented"],
                level["starting_hearts"],
                level["starting_air"],
                level["goal_fish"],
                level["player_animation_frames"],
                level["max_fish_on_screen"],
                len(level["fish_types"]),
                fish_pointer,
                level["air_enabled"],
                level["air_gain"],
                level["air_loss_frames"],
                level["air_spawn_frames"],
                level["air_rise_speed_fixed"],
            )
        )

    lines.append("};")
    lines.append("")

    return "\n".join(lines)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: levels_to_c.py <levels.json> <output_prefix>", file=sys.stderr)
        return 1

    source = Path(sys.argv[1])
    output_prefix = Path(sys.argv[2])
    data = json.loads(source.read_text())

    levels_raw = data.get("levels")
    if not isinstance(levels_raw, list) or not levels_raw:
        fail("levels must be a non-empty array")

    levels = []
    max_fish_on_screen = 1

    for level_index, raw_level in enumerate(levels_raw):
        if not isinstance(raw_level, dict):
            fail(f"levels[{level_index}] must be an object")
        level = parse_level(level_index, raw_level)
        levels.append(level)
        if level["max_fish_on_screen"] > max_fish_on_screen:
            max_fish_on_screen = level["max_fish_on_screen"]

    output_prefix.with_suffix(".h").write_text(render_header(len(levels), max_fish_on_screen))
    output_prefix.with_suffix(".c").write_text(render_c(levels, source.name))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
