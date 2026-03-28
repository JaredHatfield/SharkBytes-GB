#include <gb/gb.h>

#include "app.h"
#include "front_end.h"
#include "level_scene.h"
#include "levels_generated.h"

typedef enum ScreenState {
    SCREEN_TITLE = 0,
    SCREEN_LEVEL_SELECT,
    SCREEN_LEVEL_PLAYING,
    SCREEN_LEVEL_COMPLETE,
    SCREEN_LEVEL_FAILED,
    SCREEN_LEVEL_PLACEHOLDER
} ScreenState;

#define TITLE_SCREEN_DURATION_FRAMES 180u

static UINT8 level_unlocked[LEVEL_COUNT] = { 1u };
static UINT8 previous_keys = 0u;
static UINT8 selected_level = 0u;
static UINT8 title_timer = 0u;
static ScreenState screen = SCREEN_TITLE;

static void show_level_select(void) {
    screen = SCREEN_LEVEL_SELECT;
    front_end_draw_level_select_screen(selected_level, level_unlocked);
}

static void unlock_next_level(void) {
    if ((selected_level + 1u) < LEVEL_COUNT) {
        level_unlocked[selected_level + 1u] = 1u;
    }
}

void app_init(void) {
    previous_keys = 0u;
    selected_level = 0u;
    title_timer = 0u;
    screen = SCREEN_TITLE;
    front_end_draw_title_screen();
}

void app_tick(void) {
    UINT8 current_keys;
    UINT8 pressed_keys;
    LevelUpdateResult level_result;

    wait_vbl_done();
    current_keys = joypad();
    pressed_keys = current_keys & (UINT8)~previous_keys;
    previous_keys = current_keys;

    if (screen == SCREEN_TITLE) {
        ++title_timer;
        if ((title_timer >= TITLE_SCREEN_DURATION_FRAMES) || (pressed_keys & (J_A | J_START))) {
            show_level_select();
        }
    } else if (screen == SCREEN_LEVEL_SELECT) {
        if ((pressed_keys & J_UP) && (selected_level > 0u)) {
            --selected_level;
            front_end_move_level_select_cursor(selected_level);
        } else if ((pressed_keys & J_DOWN) && (selected_level + 1u < LEVEL_COUNT)) {
            ++selected_level;
            front_end_move_level_select_cursor(selected_level);
        } else if ((pressed_keys & (J_A | J_START)) && level_unlocked[selected_level]) {
            if (level_definitions[selected_level].implemented) {
                level_scene_begin(selected_level);
                screen = SCREEN_LEVEL_PLAYING;
            } else {
                screen = SCREEN_LEVEL_PLACEHOLDER;
                front_end_draw_level_placeholder_screen(selected_level);
            }
        }
    } else if (screen == SCREEN_LEVEL_PLAYING) {
        if (pressed_keys & J_START) {
            show_level_select();
        } else {
            level_result = level_scene_update(current_keys, pressed_keys);

            if (level_result == LEVEL_WON) {
                unlock_next_level();
                front_end_draw_level_result_message(1u, LEVEL_FAILURE_AIR);
                screen = SCREEN_LEVEL_COMPLETE;
            } else if (level_result == LEVEL_LOST) {
                front_end_draw_level_result_message(0u, level_scene_failure_reason());
                screen = SCREEN_LEVEL_FAILED;
            }
        }
    } else if ((screen == SCREEN_LEVEL_COMPLETE) || (screen == SCREEN_LEVEL_FAILED)) {
        if (pressed_keys & J_START) {
            show_level_select();
        }
    } else if (screen == SCREEN_LEVEL_PLACEHOLDER) {
        if (pressed_keys & J_START) {
            show_level_select();
        }
    }
}
