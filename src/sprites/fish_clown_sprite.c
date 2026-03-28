#include "fish_clown_sprite.h"

/* Generated from src/sprites/fish_clown.piskel by scripts/piskel_to_gbdk_sprite.py. */
const unsigned char fish_clown_sprite_tiles[] = {
    /* tile 0 */
    0x00, 0x08, 0x14, 0x1C, 0x55, 0x5F, 0x54, 0x7F,
    0x54, 0x5F, 0x14, 0x1C, 0x00, 0x08, 0x00, 0x00
};

const unsigned char fish_clown_sprite_frame_tilemap[] = {
    0u
};

const unsigned char fish_clown_sprite_frame_hitboxes[] = {
    1u, 0u, 7u, 7u
};

void fish_clown_sprite_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y) {
    UINT8 sprite_index = first_sprite;
    UINT8 tilemap_index = frame * FISH_CLOWN_SPRITE_TILES_PER_FRAME;
    UINT8 row;
    UINT8 col;

    for (row = 0; row != FISH_CLOWN_SPRITE_HEIGHT_TILES; ++row) {
        for (col = 0; col != FISH_CLOWN_SPRITE_WIDTH_TILES; ++col) {
            set_sprite_tile(sprite_index, first_tile + fish_clown_sprite_frame_tilemap[tilemap_index]);
            move_sprite(sprite_index, x + (col << 3), y + (row << 3));
            ++sprite_index;
            ++tilemap_index;
        }
    }
}
