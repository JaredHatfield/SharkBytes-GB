#include <gb/gb.h>
#include <gbdk/console.h>
#include <stdio.h>

#include "display_helpers.h"
#include "front_end.h"
#include "levels_generated.h"
#include "sprites/diver_title_sprite.h"

#define LEVEL_SELECT_CURSOR_TILE 0u
#define LEVEL_SELECT_CURSOR_SPRITE 0u
#define SCREEN_TILE_WIDTH 20u

static const unsigned char level_select_cursor_tile[] = {
    0x01, 0x00,
    0x03, 0x00,
    0x07, 0x00,
    0x0F, 0x00,
    0x07, 0x00,
    0x03, 0x00,
    0x01, 0x00,
    0x00, 0x00
};

static void draw_level_select_row(UINT8 level_index, const UINT8 *level_unlocked) {
    UINT8 row = 4u + level_index;

    gotoxy(0u, row);
    printf("                    ");

    gotoxy(2u, row);
    printf("%u", level_index + 1u);

    gotoxy(5u, row);
    printf("%s", level_definitions[level_index].name);

    if (!level_unlocked[level_index]) {
        gotoxy(13u, row);
        printf("LOCK");
    }
}

void front_end_draw_title_screen(void) {
    cls();
    display_hide_all_sprites();
    set_sprite_data(0u, DIVER_TITLE_SPRITE_TILE_COUNT, diver_title_sprite_tiles);
    diver_title_sprite_show(0u, 0u, 0u, 76u, 72u);

    gotoxy(4u, 3u);
    printf("Shark Bytes");
}

void front_end_move_level_select_cursor(UINT8 selected_level) {
    move_sprite(
        LEVEL_SELECT_CURSOR_SPRITE,
        8u,
        (UINT8)(16u + ((4u + selected_level) << 3))
    );
}

void front_end_draw_level_select_screen(UINT8 selected_level, const UINT8 *level_unlocked) {
    UINT8 level_index;

    cls();
    display_hide_all_sprites();
    set_sprite_data(LEVEL_SELECT_CURSOR_TILE, 1u, level_select_cursor_tile);
    set_sprite_tile(LEVEL_SELECT_CURSOR_SPRITE, LEVEL_SELECT_CURSOR_TILE);

    gotoxy(4u, 0u);
    printf("Shark Bytes");

    gotoxy(3u, 1u);
    printf("Level Select");

    for (level_index = 0u; level_index != LEVEL_COUNT; ++level_index) {
        draw_level_select_row(level_index, level_unlocked);
    }

    front_end_move_level_select_cursor(selected_level);

    gotoxy(0u, 16u);
    printf("A: PLAY");
    gotoxy(0u, 17u);
    printf("UP/DOWN");
}

void front_end_draw_level_placeholder_screen(UINT8 level_index) {
    cls();
    display_hide_all_sprites();

    gotoxy(4u, 3u);
    printf("Level %u", level_index + 1u);

    gotoxy(3u, 6u);
    printf("%s", level_definitions[level_index].name);

    gotoxy(1u, 10u);
    printf("Level screen TBD");

    gotoxy(0u, 15u);
    printf("START: BACK");
}

void front_end_draw_level_result_message(UINT8 won, LevelFailureReason reason) {
    display_hide_all_sprites();

    gotoxy(0u, 7u);
    printf("                    ");
    gotoxy(0u, 9u);
    printf("                    ");
    gotoxy(0u, 11u);
    printf("                    ");

    if (won) {
        display_print_centered(7u, SCREEN_TILE_WIDTH, "LEVEL CLEAR");
        display_print_centered(9u, SCREEN_TILE_WIDTH, "START: CONT");
    } else {
        display_print_centered(7u, SCREEN_TILE_WIDTH, reason == LEVEL_FAILURE_AIR ? "OUT OF AIR" : "NO HEALTH");
        display_print_centered(9u, SCREEN_TILE_WIDTH, "START: CONT");
    }
}
