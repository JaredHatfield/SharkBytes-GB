#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdio.h>

#include "display_helpers.h"

void display_hide_sprite_range(UINT8 first_sprite, UINT8 sprite_count) {
    UINT8 sprite_index;

    for (sprite_index = first_sprite; sprite_index != first_sprite + sprite_count; ++sprite_index) {
        move_sprite(sprite_index, 0u, 0u);
        set_sprite_prop(sprite_index, 0u);
    }
}

void display_hide_all_sprites(void) {
    display_hide_sprite_range(0u, 40u);
}

void display_set_sprite_prop_range(UINT8 first_sprite, UINT8 sprite_count, UINT8 prop) {
    UINT8 sprite_index;

    for (sprite_index = first_sprite; sprite_index != first_sprite + sprite_count; ++sprite_index) {
        set_sprite_prop(sprite_index, prop);
    }
}

void display_print_centered(UINT8 row, UINT8 screen_width, const char *text) {
    UINT8 length = 0u;
    UINT8 column;

    while (text[length] != '\0') {
        ++length;
    }

    column = (UINT8)((screen_width - length) >> 1);
    gotoxy(column, row);
    printf("%s", text);
}
