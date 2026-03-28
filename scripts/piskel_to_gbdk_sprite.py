#!/usr/bin/env python3

import argparse
import base64
import json
import re
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DMG_BACKGROUND = (155, 188, 15, 255)
DMG_LIGHT = (139, 172, 15, 255)
DMG_DARK = (48, 98, 48, 255)
DMG_DARKEST = (15, 56, 15, 255)
DMG_OPAQUE = [DMG_LIGHT, DMG_DARK, DMG_DARKEST]
REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SPRITES_DIR = REPO_ROOT / "src" / "sprites"


@dataclass(frozen=True)
class BatchSpriteConfig:
    output_base_name: str | None = None
    symbol: str | None = None
    scale: int = 1
    include_in_batch: bool = True


BATCH_CONFIG = {
    "diver_standing.piskel": BatchSpriteConfig(
        output_base_name="diver_title_sprite",
        symbol="diver_title_sprite",
        scale=3,
    ),
    "diver_front.piskel": BatchSpriteConfig(include_in_batch=False),
}


def sanitize_symbol(name):
    symbol = re.sub(r"[^0-9A-Za-z_]+", "_", name).strip("_").lower()
    if not symbol:
        raise ValueError("could not derive a valid C symbol name")
    if symbol[0].isdigit():
        symbol = f"sprite_{symbol}"
    return symbol


def display_path(path):
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def read_png_rgba(png_bytes):
    if png_bytes[:8] != PNG_SIGNATURE:
        raise ValueError("embedded image is not a PNG")

    offset = 8
    width = None
    height = None
    bit_depth = None
    color_type = None
    interlace = None
    palette = None
    transparency = None
    idat = bytearray()

    while offset < len(png_bytes):
        chunk_length = struct.unpack(">I", png_bytes[offset : offset + 4])[0]
        chunk_type = png_bytes[offset + 4 : offset + 8]
        chunk_data = png_bytes[offset + 8 : offset + 8 + chunk_length]
        offset += chunk_length + 12

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", chunk_data
            )
        elif chunk_type == b"PLTE":
            palette = [tuple(chunk_data[i : i + 3]) for i in range(0, len(chunk_data), 3)]
        elif chunk_type == b"tRNS":
            transparency = chunk_data
        elif chunk_type == b"IDAT":
            idat.extend(chunk_data)

    if width is None or height is None:
        raise ValueError("PNG is missing an IHDR chunk")
    if bit_depth != 8:
        raise ValueError(f"unsupported PNG bit depth: {bit_depth}")
    if interlace != 0:
        raise ValueError("interlaced PNGs are not supported")

    if color_type == 6:
        bytes_per_pixel = 4
    elif color_type == 2:
        bytes_per_pixel = 3
    elif color_type == 3:
        bytes_per_pixel = 1
        if palette is None:
            raise ValueError("indexed PNG is missing a palette")
    else:
        raise ValueError(f"unsupported PNG color type: {color_type}")

    raw = zlib.decompress(bytes(idat))
    stride = width * bytes_per_pixel
    pixels = []
    previous = bytearray(stride)
    cursor = 0

    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        row = bytearray(raw[cursor : cursor + stride])
        cursor += stride
        decoded = bytearray(stride)

        for i, value in enumerate(row):
            left = decoded[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
            up = previous[i]
            up_left = previous[i - bytes_per_pixel] if i >= bytes_per_pixel else 0

            if filter_type == 0:
                result = value
            elif filter_type == 1:
                result = (value + left) & 0xFF
            elif filter_type == 2:
                result = (value + up) & 0xFF
            elif filter_type == 3:
                result = (value + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                predictor = left + up - up_left
                left_distance = abs(predictor - left)
                up_distance = abs(predictor - up)
                up_left_distance = abs(predictor - up_left)
                if left_distance <= up_distance and left_distance <= up_left_distance:
                    paeth = left
                elif up_distance <= up_left_distance:
                    paeth = up
                else:
                    paeth = up_left
                result = (value + paeth) & 0xFF
            else:
                raise ValueError(f"unsupported PNG filter type: {filter_type}")

            decoded[i] = result

        previous = decoded

        rgba_row = []
        for index in range(0, len(decoded), bytes_per_pixel):
            if color_type == 6:
                rgba_row.append(tuple(decoded[index : index + 4]))
            elif color_type == 2:
                r, g, b = decoded[index : index + 3]
                rgba_row.append((r, g, b, 255))
            else:
                palette_index = decoded[index]
                rgb = palette[palette_index]
                alpha = (
                    transparency[palette_index]
                    if transparency is not None and palette_index < len(transparency)
                    else 255
                )
                rgba_row.append((rgb[0], rgb[1], rgb[2], alpha))

        pixels.append(rgba_row)

    return width, height, pixels


def empty_canvas(width, height):
    return [[(0, 0, 0, 0) for _ in range(width)] for _ in range(height)]


def scale_frame(frame, scale):
    if scale == 1:
        return frame

    height = len(frame)
    width = len(frame[0]) if height else 0
    scaled = empty_canvas(width * scale, height * scale)

    for y, row in enumerate(frame):
        for x, pixel in enumerate(row):
            for dy in range(scale):
                for dx in range(scale):
                    scaled[(y * scale) + dy][(x * scale) + dx] = pixel

    return scaled


def alpha_composite(dst, src):
    src_alpha = src[3] / 255.0
    if src_alpha <= 0.0:
        return dst

    dst_alpha = dst[3] / 255.0
    out_alpha = src_alpha + dst_alpha * (1.0 - src_alpha)
    if out_alpha <= 0.0:
        return (0, 0, 0, 0)

    channels = []
    for channel in range(3):
        out = (
            (src[channel] * src_alpha) + (dst[channel] * dst_alpha * (1.0 - src_alpha))
        ) / out_alpha
        channels.append(int(round(out)))

    channels.append(int(round(out_alpha * 255.0)))
    return tuple(channels)


def apply_opacity(pixel, opacity):
    if pixel[3] == 0 or opacity >= 1.0:
        return pixel
    return (pixel[0], pixel[1], pixel[2], int(round(pixel[3] * opacity)))


def blit(dest_pixels, source_pixels, x_offset, y_offset):
    for y, row in enumerate(source_pixels):
        for x, pixel in enumerate(row):
            dest_pixels[y + y_offset][x + x_offset] = alpha_composite(
                dest_pixels[y + y_offset][x + x_offset], pixel
            )


def build_layer_frames(layer, frame_width, frame_height):
    chunks = []
    for chunk in layer["chunks"]:
        _, _, pixels = read_png_rgba(base64.b64decode(chunk["base64PNG"].split(",", 1)[1]))
        chunks.append(pixels)

    if frame_width % 8 != 0 or frame_height % 8 != 0:
        raise ValueError("sprite dimensions must be multiples of 8 pixels")

    if not chunks:
        raise ValueError("piskel layer has no chunks")

    if len(chunks) == 1:
        sheet = chunks[0]
    else:
        layout = layer["chunks"][0]["layout"]
        rows = len(layout)
        cols = len(layout[0]) if rows else 0
        chunk_height = len(chunks[0])
        chunk_width = len(chunks[0][0])
        sheet = empty_canvas(cols * chunk_width, rows * chunk_height)

        for row_index, row in enumerate(layout):
            for col_index, chunk_index in enumerate(row):
                blit(sheet, chunks[chunk_index], col_index * chunk_width, row_index * chunk_height)

    frames_per_row = len(sheet[0]) // frame_width
    frames_per_col = len(sheet) // frame_height
    frame_capacity = frames_per_row * frames_per_col
    frame_count = layer["frameCount"]

    if frame_capacity < frame_count:
        raise ValueError("layer sprite sheet does not contain all declared frames")

    frames = []
    for frame_index in range(frame_count):
        source_x = (frame_index % frames_per_row) * frame_width
        source_y = (frame_index // frames_per_row) * frame_height
        frame = empty_canvas(frame_width, frame_height)
        for y in range(frame_height):
            for x in range(frame_width):
                frame[y][x] = sheet[source_y + y][source_x + x]
        frames.append(frame)

    return frames


def load_piskel_frames(path):
    data = json.loads(path.read_text())
    sprite = data["piskel"]
    width = sprite["width"]
    height = sprite["height"]

    layers = [json.loads(layer_json) for layer_json in sprite["layers"]]
    if not layers:
        raise ValueError("piskel file has no layers")

    layer_frames = [build_layer_frames(layer, width, height) for layer in layers]
    frame_count = max(len(frames) for frames in layer_frames)
    frames = []

    for frame_index in range(frame_count):
        frame = empty_canvas(width, height)
        for layer, frames_for_layer in zip(layers, layer_frames):
            opacity = float(layer.get("opacity", 1.0))
            source = frames_for_layer[frame_index % len(frames_for_layer)]
            for y in range(height):
                for x in range(width):
                    frame[y][x] = alpha_composite(
                        frame[y][x], apply_opacity(source[y][x], opacity)
                    )
        frames.append(frame)

    return width, height, frames


def canonical_palette():
    return {
        (0, 0, 0, 0): 0,
        DMG_LIGHT: 1,
        DMG_DARK: 2,
        DMG_DARKEST: 3,
    }


def nearest_canonical_opaque(pixel):
    best_color = DMG_OPAQUE[0]
    best_distance = None

    for candidate in DMG_OPAQUE:
        distance = (
            ((pixel[0] - candidate[0]) * (pixel[0] - candidate[0]))
            + ((pixel[1] - candidate[1]) * (pixel[1] - candidate[1]))
            + ((pixel[2] - candidate[2]) * (pixel[2] - candidate[2]))
        )
        if best_distance is None or distance < best_distance:
            best_distance = distance
            best_color = candidate

    return best_color


def dominant_corner_background(frame):
    corners = [
        frame[0][0],
        frame[0][-1],
        frame[-1][0],
        frame[-1][-1],
    ]
    counts = {}

    for pixel in corners:
        counts[pixel] = counts.get(pixel, 0) + 1

    background = max(counts, key=counts.get)
    if background[3] == 0 or counts[background] < 3:
        return None
    return background


def normalize_frames(frames):
    normalized_frames = []

    for frame in frames:
        background = dominant_corner_background(frame)
        normalized = empty_canvas(len(frame[0]), len(frame))

        for y, row in enumerate(frame):
            for x, pixel in enumerate(row):
                if pixel[3] == 0:
                    normalized[y][x] = (0, 0, 0, 0)
                elif background is not None and pixel == background:
                    normalized[y][x] = (0, 0, 0, 0)
                else:
                    normalized[y][x] = nearest_canonical_opaque(pixel)

        normalized_frames.append(normalized)

    return normalized_frames


def sprite_value(pixel, palette):
    if pixel[3] == 0:
        return 0
    if pixel not in palette:
        raise ValueError(f"opaque pixel color {pixel[:3]} is not in the generated palette")
    return palette[pixel]


def encode_frames(frames, width, height, palette):
    unique_tiles = []
    tile_lookup = {}
    frame_tilemaps = []
    width_tiles = width // 8
    height_tiles = height // 8

    for frame in frames:
        tilemap = []
        for tile_y in range(height_tiles):
            for tile_x in range(width_tiles):
                tile_bytes = []
                for row in range(8):
                    low = 0
                    high = 0
                    for col in range(8):
                        value = sprite_value(
                            frame[(tile_y * 8) + row][(tile_x * 8) + col], palette
                        )
                        if value & 0x01:
                            low |= 1 << (7 - col)
                        if value & 0x02:
                            high |= 1 << (7 - col)
                    tile_bytes.extend((low, high))
                tile_key = tuple(tile_bytes)
                if tile_key not in tile_lookup:
                    tile_lookup[tile_key] = len(unique_tiles)
                    unique_tiles.append(tile_bytes)
                tilemap.append(tile_lookup[tile_key])
        frame_tilemaps.append(tilemap)

    return unique_tiles, frame_tilemaps


def compute_frame_hitboxes(frames):
    hitboxes = []

    for frame in frames:
        minimum_x = None
        minimum_y = None
        maximum_x = None
        maximum_y = None

        for y, row in enumerate(frame):
            for x, pixel in enumerate(row):
                if pixel[3] == 0:
                    continue
                if minimum_x is None or x < minimum_x:
                    minimum_x = x
                if minimum_y is None or y < minimum_y:
                    minimum_y = y
                if maximum_x is None or x > maximum_x:
                    maximum_x = x
                if maximum_y is None or y > maximum_y:
                    maximum_y = y

        if minimum_x is None:
            hitboxes.append((0, 0, 0, 0))
        else:
            hitboxes.append(
                (
                    minimum_x,
                    minimum_y,
                    (maximum_x - minimum_x) + 1,
                    (maximum_y - minimum_y) + 1,
                )
            )

    return hitboxes


def render_header(symbol, guard, width, height, frame_count, tile_count):
    width_tiles = width // 8
    height_tiles = height // 8
    tiles_per_frame = width_tiles * height_tiles
    return f"""#ifndef {guard}_H
#define {guard}_H

#include <gb/gb.h>

#define {guard}_WIDTH {width}u
#define {guard}_HEIGHT {height}u
#define {guard}_WIDTH_TILES {width_tiles}u
#define {guard}_HEIGHT_TILES {height_tiles}u
#define {guard}_FRAME_COUNT {frame_count}u
#define {guard}_TILES_PER_FRAME ({guard}_WIDTH_TILES * {guard}_HEIGHT_TILES)
#define {guard}_TILE_COUNT {tile_count}u
#define {guard}_FRAME_TILEMAP_LENGTH ({guard}_FRAME_COUNT * {guard}_TILES_PER_FRAME)
#define {guard}_FRAME_HITBOX_STRIDE 4u

extern const unsigned char {symbol}_tiles[];
extern const unsigned char {symbol}_frame_tilemap[];
extern const unsigned char {symbol}_frame_hitboxes[];

void {symbol}_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y);

#endif
"""


def render_source(symbol, header_name, guard, input_path, tiles, frame_tilemaps, frame_hitboxes):
    lines = [
        f'#include "{header_name}"',
        "",
        f"/* Generated from {input_path} by scripts/piskel_to_gbdk_sprite.py. */",
        f"const unsigned char {symbol}_tiles[] = {{",
    ]

    for tile_index, tile in enumerate(tiles):
        lines.append(f"    /* tile {tile_index} */")
        for row_index in range(0, len(tile), 8):
            row_bytes = ", ".join(f"0x{value:02X}" for value in tile[row_index : row_index + 8])
            suffix = "," if row_index + 8 < len(tile) or tile_index + 1 < len(tiles) else ""
            lines.append(f"    {row_bytes}{suffix}")

    lines.extend(
        [
            "};",
            "",
            f"const unsigned char {symbol}_frame_tilemap[] = {{",
        ]
    )

    flat_tilemap = [value for tilemap in frame_tilemaps for value in tilemap]
    if flat_tilemap:
        for offset in range(0, len(flat_tilemap), 8):
            row_values = ", ".join(f"{value}u" for value in flat_tilemap[offset : offset + 8])
            suffix = "," if offset + 8 < len(flat_tilemap) else ""
            lines.append(f"    {row_values}{suffix}")
    lines.extend(
        [
            "};",
            "",
            f"const unsigned char {symbol}_frame_hitboxes[] = {{",
        ]
    )

    flat_hitboxes = [value for hitbox in frame_hitboxes for value in hitbox]
    if flat_hitboxes:
        for offset in range(0, len(flat_hitboxes), 8):
            row_values = ", ".join(f"{value}u" for value in flat_hitboxes[offset : offset + 8])
            suffix = "," if offset + 8 < len(flat_hitboxes) else ""
            lines.append(f"    {row_values}{suffix}")
    lines.extend(
        [
            "};",
            "",
            f"void {symbol}_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y) {{",
            "    UINT8 sprite_index = first_sprite;",
            f"    UINT8 tilemap_index = frame * {guard}_TILES_PER_FRAME;",
            "    UINT8 row;",
            "    UINT8 col;",
            "",
            f"    for (row = 0; row != {guard}_HEIGHT_TILES; ++row) {{",
            f"        for (col = 0; col != {guard}_WIDTH_TILES; ++col) {{",
            f"            set_sprite_tile(sprite_index, first_tile + {symbol}_frame_tilemap[tilemap_index]);",
            "            move_sprite(sprite_index, x + (col << 3), y + (row << 3));",
            "            ++sprite_index;",
            "            ++tilemap_index;",
            "        }",
            "    }",
            "}",
            "",
        ]
    )

    return "\n".join(lines)


def generate_sprite(input_path, output_base, symbol, scale):
    width, height, frames = load_piskel_frames(input_path)
    width *= scale
    height *= scale
    frames = [scale_frame(frame, scale) for frame in frames]
    frames = normalize_frames(frames)
    palette = canonical_palette()
    tiles, frame_tilemaps = encode_frames(frames, width, height, palette)
    frame_hitboxes = compute_frame_hitboxes(frames)

    guard = sanitize_symbol(symbol).upper()
    header_path = output_base.with_suffix(".h")
    source_path = output_base.with_suffix(".c")

    header_text = render_header(symbol, guard, width, height, len(frames), len(tiles))
    source_text = render_source(
        symbol,
        header_path.name,
        guard,
        display_path(input_path),
        tiles,
        frame_tilemaps,
        frame_hitboxes,
    )

    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(header_text)
    source_path.write_text(source_text)


def batch_specs(sprite_dir):
    piskel_paths = sorted(sprite_dir.glob("*.piskel"))

    if not piskel_paths:
        raise ValueError(f"no .piskel files found in {sprite_dir}")

    for input_path in piskel_paths:
        config = BATCH_CONFIG.get(input_path.name, BatchSpriteConfig())
        if not config.include_in_batch:
            continue

        output_base_name = config.output_base_name or f"{input_path.stem}_sprite"
        symbol = sanitize_symbol(config.symbol or output_base_name)
        yield input_path, sprite_dir / output_base_name, symbol, config.scale


def generate_batch(sprite_dir):
    for input_path, output_base, symbol, scale in batch_specs(sprite_dir):
        generate_sprite(input_path, output_base, symbol, scale)


def main():
    parser = argparse.ArgumentParser(
        description="Convert a Piskel sprite into GBDK-friendly C source and header files."
    )
    parser.add_argument(
        "input",
        nargs="?",
        type=Path,
        help="Path to the source .piskel file. Omit to batch-generate the repo's runtime sprite assets.",
    )
    parser.add_argument(
        "output_base",
        nargs="?",
        type=Path,
        help="Output path without extension, for example src/sprites/mint_sprite",
    )
    parser.add_argument(
        "--symbol",
        help="C symbol prefix to use in the generated files (defaults to the output filename)",
    )
    parser.add_argument(
        "--scale",
        type=int,
        default=1,
        help="Integer nearest-neighbor scale factor to apply before tile generation",
    )
    args = parser.parse_args()

    try:
        if args.scale < 1:
            raise ValueError("scale must be at least 1")

        if args.input is None and args.output_base is None:
            if args.symbol is not None:
                raise ValueError("--symbol requires an explicit input and output_base")
            generate_batch(DEFAULT_SPRITES_DIR)
            return

        if args.input is None or args.output_base is None:
            raise ValueError("input and output_base must be provided together")

        symbol = sanitize_symbol(args.symbol or args.output_base.name)
        generate_sprite(args.input, args.output_base, symbol, args.scale)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
