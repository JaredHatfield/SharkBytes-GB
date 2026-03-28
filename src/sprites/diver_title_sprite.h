#ifndef DIVER_TITLE_SPRITE_H
#define DIVER_TITLE_SPRITE_H

#include <gb/gb.h>

#define DIVER_TITLE_SPRITE_WIDTH 24u
#define DIVER_TITLE_SPRITE_HEIGHT 48u
#define DIVER_TITLE_SPRITE_WIDTH_TILES 3u
#define DIVER_TITLE_SPRITE_HEIGHT_TILES 6u
#define DIVER_TITLE_SPRITE_FRAME_COUNT 1u
#define DIVER_TITLE_SPRITE_TILES_PER_FRAME (DIVER_TITLE_SPRITE_WIDTH_TILES * DIVER_TITLE_SPRITE_HEIGHT_TILES)
#define DIVER_TITLE_SPRITE_TILE_COUNT 18u
#define DIVER_TITLE_SPRITE_FRAME_TILEMAP_LENGTH (DIVER_TITLE_SPRITE_FRAME_COUNT * DIVER_TITLE_SPRITE_TILES_PER_FRAME)
#define DIVER_TITLE_SPRITE_FRAME_HITBOX_STRIDE 4u

extern const unsigned char diver_title_sprite_tiles[];
extern const unsigned char diver_title_sprite_frame_tilemap[];
extern const unsigned char diver_title_sprite_frame_hitboxes[];

void diver_title_sprite_show(UINT8 first_tile, UINT8 first_sprite, UINT8 frame, UINT8 x, UINT8 y);

#endif
