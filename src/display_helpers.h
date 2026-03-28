#ifndef DISPLAY_HELPERS_H
#define DISPLAY_HELPERS_H

#include <gb/gb.h>

void display_hide_sprite_range(UINT8 first_sprite, UINT8 sprite_count);
void display_hide_all_sprites(void);
void display_set_sprite_prop_range(UINT8 first_sprite, UINT8 sprite_count, UINT8 prop);
void display_print_centered(UINT8 row, UINT8 screen_width, const char *text);

#endif
