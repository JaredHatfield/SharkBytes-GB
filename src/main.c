#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdio.h>

#include "sprites/diver_title_sprite.h"

typedef enum ScreenState {
    SCREEN_TITLE = 0,
    SCREEN_LEVEL_SELECT,
    SCREEN_LEVEL_PLACEHOLDER
} ScreenState;

#define LEVEL_COUNT 10u
#define TITLE_SCREEN_DURATION_FRAMES 180u
#define LEVEL_SELECT_CURSOR_TILE 0u
#define LEVEL_SELECT_CURSOR_SPRITE 0u

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

static const char * const level_names[LEVEL_COUNT] = {
    "Bubble",
    "TBD",
    "TBD",
    "TBD",
    "TBD",
    "TBD",
    "TBD",
    "TBD",
    "TBD",
    "TBD"
};

static UINT8 level_unlocked[LEVEL_COUNT] = {
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
};

static void hide_all_sprites(void) {
    UINT8 sprite_index;

    for (sprite_index = 0u; sprite_index != 40u; ++sprite_index) {
        move_sprite(sprite_index, 0u, 0u);
    }
}

static void draw_title_screen(void) {
    cls();
    hide_all_sprites();
    set_sprite_data(0u, DIVER_TITLE_SPRITE_TILE_COUNT, diver_title_sprite_tiles);
    diver_title_sprite_show(0u, 0u, 0u, 76u, 72u);

    gotoxy(4u, 3u);
    printf("Shark Bytes");
}

static void move_level_select_cursor(UINT8 selected_level) {
    move_sprite(
        LEVEL_SELECT_CURSOR_SPRITE,
        8u,
        (UINT8)(16u + ((4u + selected_level) << 3))
    );
}

static void draw_level_select_row(UINT8 level_index) {
    UINT8 row = 4u + level_index;

    gotoxy(0u, row);
    printf("                    ");

    gotoxy(2u, row);
    printf("%u", level_index + 1u);

    gotoxy(5u, row);
    printf("%s", level_names[level_index]);

    if (!level_unlocked[level_index]) {
        gotoxy(13u, row);
        printf("LOCK");
    }
}

static void draw_level_select_screen(UINT8 selected_level) {
    UINT8 level_index;

    cls();
    hide_all_sprites();
    set_sprite_data(LEVEL_SELECT_CURSOR_TILE, 1u, level_select_cursor_tile);
    set_sprite_tile(LEVEL_SELECT_CURSOR_SPRITE, LEVEL_SELECT_CURSOR_TILE);

    gotoxy(4u, 0u);
    printf("Shark Bytes");

    gotoxy(3u, 1u);
    printf("Level Select");

    for (level_index = 0u; level_index != LEVEL_COUNT; ++level_index) {
        draw_level_select_row(level_index);
    }

    move_level_select_cursor(selected_level);

    gotoxy(0u, 16u);
    printf("A: PLAY");
    gotoxy(0u, 17u);
    printf("UP/DOWN");
}

static void draw_level_placeholder_screen(UINT8 level_index) {
    cls();
    hide_all_sprites();

    gotoxy(4u, 3u);
    printf("Level %u", level_index + 1u);

    gotoxy(3u, 6u);
    printf("%s", level_names[level_index]);

    gotoxy(1u, 10u);
    printf("Level screen TBD");

    gotoxy(2u, 15u);
    printf("B: BACK");
}

void main(void) {
    font_t title_font;
    UINT8 current_keys;
    UINT8 pressed_keys;
    UINT8 previous_keys = 0u;
    UINT8 selected_level = 0u;
    UINT8 title_timer = 0u;
    ScreenState screen = SCREEN_TITLE;

    DISPLAY_OFF;

    font_init();
    title_font = font_load(font_min);
    font_set(title_font);

    BGP_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
    OBP0_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);

    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    draw_title_screen();

    while (1) {
        wait_vbl_done();
        current_keys = joypad();
        pressed_keys = current_keys & (UINT8)~previous_keys;
        previous_keys = current_keys;

        switch (screen) {
            case SCREEN_TITLE:
                ++title_timer;
                if ((title_timer >= TITLE_SCREEN_DURATION_FRAMES) || (pressed_keys & (J_A | J_START))) {
                    screen = SCREEN_LEVEL_SELECT;
                    draw_level_select_screen(selected_level);
                }
                break;

            case SCREEN_LEVEL_SELECT:
                if ((pressed_keys & J_UP) && (selected_level > 0u)) {
                    --selected_level;
                    move_level_select_cursor(selected_level);
                } else if ((pressed_keys & J_DOWN) && (selected_level + 1u < LEVEL_COUNT)) {
                    ++selected_level;
                    move_level_select_cursor(selected_level);
                } else if ((pressed_keys & (J_A | J_START)) && level_unlocked[selected_level]) {
                    screen = SCREEN_LEVEL_PLACEHOLDER;
                    draw_level_placeholder_screen(selected_level);
                }
                break;

            case SCREEN_LEVEL_PLACEHOLDER:
                if (pressed_keys & J_B) {
                    screen = SCREEN_LEVEL_SELECT;
                    draw_level_select_screen(selected_level);
                }
                break;
        }
    }
}
