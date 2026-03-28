#ifndef DIVER_SWIMMING_SPRITE_H
#define DIVER_SWIMMING_SPRITE_H

#include <gb/gb.h>

#define DIVER_SWIMMING_SPRITE_WIDTH 32u
#define DIVER_SWIMMING_SPRITE_HEIGHT 16u
#define DIVER_SWIMMING_SPRITE_WIDTH_TILES 4u
#define DIVER_SWIMMING_SPRITE_HEIGHT_TILES 2u
#define DIVER_SWIMMING_SPRITE_FRAME_COUNT 2u
#define DIVER_SWIMMING_SPRITE_TILES_PER_FRAME (DIVER_SWIMMING_SPRITE_WIDTH_TILES * DIVER_SWIMMING_SPRITE_HEIGHT_TILES)
#define DIVER_SWIMMING_SPRITE_TILE_COUNT 13u
#define DIVER_SWIMMING_SPRITE_FRAME_TILEMAP_LENGTH (DIVER_SWIMMING_SPRITE_FRAME_COUNT * DIVER_SWIMMING_SPRITE_TILES_PER_FRAME)
#define DIVER_SWIMMING_SPRITE_FRAME_HITBOX_STRIDE 4u

extern const unsigned char diver_swimming_sprite_tiles[];
extern const unsigned char diver_swimming_sprite_frame_tilemap[];
extern const unsigned char diver_swimming_sprite_frame_hitboxes[];

void diver_swimming_sprite_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y);

#endif
