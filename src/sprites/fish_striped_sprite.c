#include "fish_striped_sprite.h"

/* Generated from src/sprites/fish_striped.piskel by scripts/piskel_to_gbdk_sprite.py. */
const unsigned char fish_striped_sprite_tiles[] = {
    /* tile 0 */
    0x00, 0x00, 0x18, 0x00, 0x24, 0x00, 0x24, 0x00,
    0x54, 0x00, 0x54, 0x00, 0x7C, 0x00, 0x54, 0x00,
    /* tile 1 */
    0x54, 0x00, 0x24, 0x00, 0x24, 0x00, 0x18, 0x00,
    0x18, 0x00, 0x24, 0x00, 0x24, 0x00, 0x00, 0x00
};

const unsigned char fish_striped_sprite_frame_tilemap[] = {
    0u, 1u
};

const unsigned char fish_striped_sprite_frame_hitboxes[] = {
    1u, 1u, 5u, 14u
};

void fish_striped_sprite_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y) {
    UINT8 sprite_index = first_sprite;
    UINT8 tilemap_index = frame * FISH_STRIPED_SPRITE_TILES_PER_FRAME;
    UINT8 row;
    UINT8 col;

    for (row = 0; row != FISH_STRIPED_SPRITE_HEIGHT_TILES; ++row) {
        for (col = 0; col != FISH_STRIPED_SPRITE_WIDTH_TILES; ++col) {
            set_sprite_tile(sprite_index, first_tile + fish_striped_sprite_frame_tilemap[tilemap_index]);
            move_sprite(sprite_index, x + (col << 3), y + (row << 3));
            ++sprite_index;
            ++tilemap_index;
        }
    }
}
