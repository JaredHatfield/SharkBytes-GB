#!/usr/bin/env python3

import argparse
import base64
import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


@dataclass(frozen=True)
class SpriteSpec:
    name: str
    output: str
    palette: dict[str, str]
    grid: str
    description: str


def parse_hex_color(value: str) -> tuple[int, int, int, int]:
    value = value.lstrip("#")
    if len(value) == 6:
        return (
            int(value[0:2], 16),
            int(value[2:4], 16),
            int(value[4:6], 16),
            255,
        )
    if len(value) == 8:
        return (
            int(value[0:2], 16),
            int(value[2:4], 16),
            int(value[4:6], 16),
            int(value[6:8], 16),
        )
    raise ValueError(f"unsupported color literal: {value}")


def normalize_grid(grid_text: str) -> list[str]:
    rows = [row.rstrip() for row in grid_text.strip("\n").splitlines()]
    if not rows:
        raise ValueError("sprite grid is empty")

    width = len(rows[0])
    if width == 0:
        raise ValueError("sprite grid has zero width")

    for row in rows:
        if len(row) != width:
            raise ValueError("sprite grid rows must all have the same width")

    if width % 8 != 0 or len(rows) % 8 != 0:
        raise ValueError("sprite dimensions must be multiples of 8")

    return rows


def rows_to_pixels(rows: list[str], palette: dict[str, tuple[int, int, int, int]]) -> list[list[tuple[int, int, int, int]]]:
    opaque_symbols = set()
    pixels = []

    for row in rows:
        pixel_row = []
        for symbol in row:
            if symbol not in palette:
                raise ValueError(f"unknown pixel symbol: {symbol!r}")
            pixel = palette[symbol]
            if pixel[3] != 0:
                opaque_symbols.add(symbol)
            pixel_row.append(pixel)
        pixels.append(pixel_row)

    if len(opaque_symbols) > 3:
        raise ValueError("Game Boy OBJ sprites support at most 3 opaque colors plus transparency")

    return pixels


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def encode_png(pixels: list[list[tuple[int, int, int, int]]]) -> bytes:
    height = len(pixels)
    width = len(pixels[0])

    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for pixel in row:
            raw.extend(pixel)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw), level=9)

    return b"".join(
        [
            PNG_SIGNATURE,
            png_chunk(b"IHDR", ihdr),
            png_chunk(b"IDAT", idat),
            png_chunk(b"IEND", b""),
        ]
    )


def build_piskel(spec: SpriteSpec) -> dict:
    rows = normalize_grid(spec.grid)
    palette = {".": (0, 0, 0, 0)}
    for symbol, color in spec.palette.items():
        palette[symbol] = parse_hex_color(color)

    pixels = rows_to_pixels(rows, palette)
    png_data = "data:image/png;base64," + base64.b64encode(encode_png(pixels)).decode("ascii")
    height = len(rows)
    width = len(rows[0])

    layer = {
        "name": "Layer 1",
        "opacity": 1,
        "frameCount": 1,
        "chunks": [{"layout": [[0]], "base64PNG": png_data}],
    }

    return {
        "modelVersion": 2,
        "piskel": {
            "name": spec.name,
            "description": spec.description,
            "fps": 12,
            "height": height,
            "width": width,
            "layers": [json.dumps(layer, separators=(",", ":"))],
            "hiddenFrames": [""],
        },
    }


SPRITES: list[SpriteSpec] = [
    SpriteSpec(
        name="big_shark",
        output="big_shark.piskel",
        palette={"B": "#0B6FB8"},
        description="Hand-drawn big shark reconstructed from the reference sheet.",
        grid="""
............BB..........
...........BBBB.........
..BB....BBBBBBBB........
.BBBB..BBBBBBBBBB.......
BBBBBBBBBBBBBBBBBBB.....
BBBBBBBBBBBBBBBBBBBBB...
BBBBBBBBBBBBBBBBBBBBBB..
BBBBBBBBBBBBBBBBBBBBBBB.
BBBBBBBBBBBBBBBBBBBBBB..
.BBBBBBBBBBBBBBBBBBBB...
..BBBBBBBBBBBBBBBBBB....
...BBBBBBBBBBBBBBB......
....BBBBBBBBBBBB........
......BBBBBBBB..........
........BBBB............
.........BB.............
""",
    ),
    SpriteSpec(
        name="little_shark",
        output="little_shark.piskel",
        palette={"B": "#0B6FB8"},
        description="Hand-drawn little shark reconstructed from the reference sheet.",
        grid="""
................
....BB....BB....
..BBBBBBBBBBBB..
.BBBBBBBBBBBBBB.
BBBB..BBB..BBBBB
.BBBBBBBBBBBBBB.
..BBBBBBBBBBBB..
................
""",
    ),
    SpriteSpec(
        name="fish_striped",
        output="fish_striped.piskel",
        palette={"O": "#C98E24"},
        description="Hand-drawn striped fish reconstructed from the reference sheet.",
        grid="""
........
...OO...
..O..O..
..O..O..
.O.O.O..
.O.O.O..
.OOOOO..
.O.O.O..
.O.O.O..
..O..O..
..O..O..
...OO...
...OO...
..O..O..
..O..O..
........
""",
    ),
    SpriteSpec(
        name="air_pickup",
        output="air_pickup.piskel",
        palette={"B": "#0B6FB8", "Y": "#D9C646"},
        description="Hand-drawn air pickup reconstructed from the reference sheet.",
        grid="""
................
............BB..
............BB..
....BBBB....BB..
...BBBBBB...BB..
....YYYY........
....YYYY........
................
....BBBB........
...BBBBBB.......
....YYYY........
....YYYY........
................
................
................
................
""",
    ),
    SpriteSpec(
        name="diver_swimming",
        output="diver_swimming.piskel",
        palette={"B": "#0B6FB8", "O": "#C98E24"},
        description="Hand-drawn swimming diver reconstructed from the reference sheet.",
        grid="""
........................
........................
........................
.....BBBBBBBB...........
...BBBBBBBBBBB.OOO......
..BBBBBBBBBBBBBOOOO.....
...BBBBBBBBBBB.OOO......
....BBBBBBBBBB..........
........BBBBB...........
.......BB..BB...........
.......BB..BB...........
........B..B............
........B..B............
.......BB..BB...........
......BB....BB..........
........................
""",
    ),
    SpriteSpec(
        name="diver_front",
        output="diver_front.piskel",
        palette={"B": "#0B6FB8", "O": "#C98E24"},
        description="Hand-drawn front-view diver reconstructed from the reference sheet.",
        grid="""
................
................
......OOOO......
.....OOOOOO.....
....OOO..OOO....
....OO....OO....
....OO....OO....
.....OOOOOO.....
......BBBB......
....BBBBBBBB....
...BBBBBBBBBB...
...BBB....BBB...
...BBB....BBB...
....BBB..BBB....
....BBBBBBBB....
.....BBBBBB.....
.....BBBBBB.....
....BBB..BBB....
....BBB..BBB....
...BBB....BBB...
...BBB....BBB...
...BBB....BBB...
...BBB....BBB...
...BBB....BBB...
....BB....BB....
....BB....BB....
....BB....BB....
....BB....BB....
....BB....BB....
...BBB....BBB...
...BBB....BBB...
..BBB......BBB..
""",
    ),
    SpriteSpec(
        name="fish_yellow",
        output="fish_yellow.piskel",
        palette={"Y": "#D9C646"},
        description="Hand-drawn yellow fish reconstructed from the reference sheet.",
        grid="""
................
.....YY.........
....YYYY........
...YYYYYY.......
..YYY..YYY......
..YY....YY......
..YY....YY......
...YY..YY.......
....YYYY........
...YYYYYY.......
..YY....YY......
..YY....YY......
..YYY..YYY......
...YYYYYY.......
.....YY.........
................
""",
    ),
]


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate hand-authored .piskel files from simple pixel grids.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("src/sprites"),
        help="Directory where the generated .piskel files should be written.",
    )
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)

    for spec in SPRITES:
        output_path = args.output_dir / spec.output
        output_path.write_text(json.dumps(build_piskel(spec), separators=(",", ":")))
        print(output_path)


if __name__ == "__main__":
    main()
