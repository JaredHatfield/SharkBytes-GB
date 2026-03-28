#ifndef FISH_YELLOW_SPRITE_H
#define FISH_YELLOW_SPRITE_H

#include <gb/gb.h>

#define FISH_YELLOW_SPRITE_WIDTH 8u
#define FISH_YELLOW_SPRITE_HEIGHT 8u
#define FISH_YELLOW_SPRITE_WIDTH_TILES 1u
#define FISH_YELLOW_SPRITE_HEIGHT_TILES 1u
#define FISH_YELLOW_SPRITE_FRAME_COUNT 1u
#define FISH_YELLOW_SPRITE_TILES_PER_FRAME (FISH_YELLOW_SPRITE_WIDTH_TILES * FISH_YELLOW_SPRITE_HEIGHT_TILES)
#define FISH_YELLOW_SPRITE_TILE_COUNT 1u
#define FISH_YELLOW_SPRITE_FRAME_TILEMAP_LENGTH (FISH_YELLOW_SPRITE_FRAME_COUNT * FISH_YELLOW_SPRITE_TILES_PER_FRAME)
#define FISH_YELLOW_SPRITE_FRAME_HITBOX_STRIDE 4u

extern const unsigned char fish_yellow_sprite_tiles[];
extern const unsigned char fish_yellow_sprite_frame_tilemap[];
extern const unsigned char fish_yellow_sprite_frame_hitboxes[];

void fish_yellow_sprite_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y);

#endif
