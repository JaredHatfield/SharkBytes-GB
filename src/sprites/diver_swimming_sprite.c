#include "diver_swimming_sprite.h"

/* Generated from src/sprites/diver_swimming.piskel by scripts/piskel_to_gbdk_sprite.py. */
const unsigned char diver_swimming_sprite_tiles[] = {
    /* tile 0 */
    0x00, 0x00, 0x05, 0x07, 0x05, 0x07, 0x00, 0x3F,
    0x00, 0x3F, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00,
    /* tile 1 */
    0x00, 0x00, 0x40, 0xC0, 0x5C, 0xC4, 0x1C, 0xE0,
    0x1C, 0xE4, 0x1C, 0x80, 0x00, 0x80, 0x00, 0x00
};

void diver_swimming_sprite_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y) {
    UINT8 sprite_index = first_sprite;
    UINT8 tile_index = first_tile + (frame * DIVER_SWIMMING_SPRITE_TILES_PER_FRAME);
    UINT8 row;
    UINT8 col;

    for (row = 0; row != DIVER_SWIMMING_SPRITE_HEIGHT_TILES; ++row) {
        for (col = 0; col != DIVER_SWIMMING_SPRITE_WIDTH_TILES; ++col) {
            set_sprite_tile(sprite_index, tile_index);
            move_sprite(sprite_index, x + (col << 3), y + (row << 3));
            ++sprite_index;
            ++tile_index;
        }
    }
}
