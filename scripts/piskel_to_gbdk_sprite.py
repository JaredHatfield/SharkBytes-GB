#!/usr/bin/env python3

import argparse
import base64
import json
import re
import struct
import sys
import zlib
from pathlib import Path


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def sanitize_symbol(name):
    symbol = re.sub(r"[^0-9A-Za-z_]+", "_", name).strip("_").lower()
    if not symbol:
        raise ValueError("could not derive a valid C symbol name")
    if symbol[0].isdigit():
        symbol = f"sprite_{symbol}"
    return symbol


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

    if frame_width % 8 != 0 or frame_height % 8 != 0:
        raise ValueError("sprite dimensions must be multiples of 8 pixels")
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


def palette_map(frames):
    opaque_colors = {}
    for frame in frames:
        for row in frame:
            for pixel in row:
                if pixel[3] != 0:
                    opaque_colors[(pixel[0], pixel[1], pixel[2])] = None

    if len(opaque_colors) > 3:
        raise ValueError(
            "sprite uses more than 3 opaque colors; Game Boy OBJ sprites support 3 plus transparency"
        )

    def brightness(color):
        return (color[0] * 299) + (color[1] * 587) + (color[2] * 114)

    ordered_colors = sorted(opaque_colors, key=brightness, reverse=True)
    palette = {(0, 0, 0, 0): 0}
    for index, color in enumerate(ordered_colors, start=1):
        palette[(color[0], color[1], color[2], 255)] = index
    return palette


def sprite_value(pixel, palette):
    if pixel[3] == 0:
        return 0
    key = (pixel[0], pixel[1], pixel[2], 255)
    if key not in palette:
        raise ValueError(f"opaque pixel color {key[:3]} is not in the generated palette")
    return palette[key]


def encode_tiles(frames, width, height, palette):
    tiles = []
    width_tiles = width // 8
    height_tiles = height // 8

    for frame in frames:
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
                tiles.append(tile_bytes)

    return tiles


def render_header(symbol, guard, width, height, frame_count):
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
#define {guard}_TILE_COUNT ({guard}_FRAME_COUNT * {guard}_TILES_PER_FRAME)

extern const unsigned char {symbol}_tiles[];

void {symbol}_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y);

#endif
"""


def render_source(symbol, header_name, guard, input_path, tiles):
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
            f"void {symbol}_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y) {{",
            "    UINT8 sprite_index = first_sprite;",
            f"    UINT8 tile_index = first_tile + (frame * {guard}_TILES_PER_FRAME);",
            "    UINT8 row;",
            "    UINT8 col;",
            "",
            f"    for (row = 0; row != {guard}_HEIGHT_TILES; ++row) {{",
            f"        for (col = 0; col != {guard}_WIDTH_TILES; ++col) {{",
            "            set_sprite_tile(sprite_index, tile_index);",
            "            move_sprite(sprite_index, x + (col << 3), y + (row << 3));",
            "            ++sprite_index;",
            "            ++tile_index;",
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
    palette = palette_map(frames)
    tiles = encode_tiles(frames, width, height, palette)

    guard = sanitize_symbol(symbol).upper()
    header_path = output_base.with_suffix(".h")
    source_path = output_base.with_suffix(".c")

    header_text = render_header(symbol, guard, width, height, len(frames))
    source_text = render_source(symbol, header_path.name, guard, input_path.as_posix(), tiles)

    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(header_text)
    source_path.write_text(source_text)


def main():
    parser = argparse.ArgumentParser(
        description="Convert a Piskel sprite into GBDK-friendly C source and header files."
    )
    parser.add_argument("input", type=Path, help="Path to the source .piskel file")
    parser.add_argument(
        "output_base",
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
        symbol = sanitize_symbol(args.symbol or args.output_base.name)
        if args.scale < 1:
            raise ValueError("scale must be at least 1")
        generate_sprite(args.input, args.output_base, symbol, args.scale)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
