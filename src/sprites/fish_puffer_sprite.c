#include "fish_puffer_sprite.h"

/* Generated from src/sprites/fish_puffer.piskel by scripts/piskel_to_gbdk_sprite.py. */
const unsigned char fish_puffer_sprite_tiles[] = {
    /* tile 0 */
    0x00, 0x2A, 0x00, 0xFE, 0x14, 0x7F, 0x00, 0xFE,
    0x00, 0x7F, 0x1C, 0xFE, 0x1C, 0x7F, 0x00, 0x54,
    /* tile 1 */
    0x00, 0x00, 0x00, 0x3E, 0x94, 0xBE, 0x80, 0xBE,
    0x9C, 0xFE, 0x9C, 0xFE, 0x80, 0x80, 0x00, 0x00
};

const unsigned char fish_puffer_sprite_frame_tilemap[] = {
    0u, 1u
};

const unsigned char fish_puffer_sprite_frame_hitboxes[] = {
    0u, 0u, 8u, 8u, 0u, 1u, 7u, 6u
};

void fish_puffer_sprite_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y) {
    UINT8 sprite_index = first_sprite;
    UINT8 tilemap_index = frame * FISH_PUFFER_SPRITE_TILES_PER_FRAME;
    UINT8 row;
    UINT8 col;

    for (row = 0; row != FISH_PUFFER_SPRITE_HEIGHT_TILES; ++row) {
        for (col = 0; col != FISH_PUFFER_SPRITE_WIDTH_TILES; ++col) {
            set_sprite_tile(sprite_index, first_tile + fish_puffer_sprite_frame_tilemap[tilemap_index]);
            move_sprite(sprite_index, x + (col << 3), y + (row << 3));
            ++sprite_index;
            ++tilemap_index;
        }
    }
}
