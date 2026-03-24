#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdio.h>

#include "sprites/diver_swimming_sprite.h"
#include "sprites/diver_title_sprite.h"
#include "sprites/fish_yellow_sprite.h"

typedef enum ScreenState {
    SCREEN_TITLE = 0,
    SCREEN_LEVEL_SELECT,
    SCREEN_LEVEL_ONE,
    SCREEN_LEVEL_PLACEHOLDER
} ScreenState;

typedef struct PlayerState {
    INT16 x;
    INT16 y;
    INT16 velocity_x;
    INT16 velocity_y;
    UINT8 facing_left;
} PlayerState;

typedef struct FishState {
    INT16 x;
    UINT8 y;
    INT16 velocity_x;
    UINT8 minimum_x;
    UINT8 maximum_x;
    UINT8 sprite_index;
} FishState;

#define LEVEL_COUNT 10u
#define TITLE_SCREEN_DURATION_FRAMES 180u
#define LEVEL_SELECT_CURSOR_TILE 0u
#define LEVEL_SELECT_CURSOR_SPRITE 0u

#define FIX_SHIFT 4u
#define FIX_SCALE 16
#define TO_FIXED(value) ((INT16)((value) << FIX_SHIFT))
#define FROM_FIXED(value) ((UINT8)((value) >> FIX_SHIFT))

#define PLAYER_TILE_FIRST 0u
#define PLAYER_SPRITE_FIRST 0u
#define PLAYER_SPRITE_COUNT DIVER_SWIMMING_SPRITE_TILES_PER_FRAME
#define FISH_TILE_FIRST (PLAYER_TILE_FIRST + DIVER_SWIMMING_SPRITE_TILE_COUNT)
#define FISH_A_SPRITE (PLAYER_SPRITE_FIRST + PLAYER_SPRITE_COUNT)
#define FISH_B_SPRITE (FISH_A_SPRITE + FISH_YELLOW_SPRITE_TILES_PER_FRAME)

#define LEVEL_ONE_MIN_X 8u
#define LEVEL_ONE_MAX_X (168u - DIVER_SWIMMING_SPRITE_WIDTH)
#define LEVEL_ONE_MIN_Y 40u
#define LEVEL_ONE_MAX_Y (160u - DIVER_SWIMMING_SPRITE_HEIGHT)

#define PLAYER_MAX_SPEED_X 24
#define PLAYER_ACCELERATION_X 3
#define PLAYER_DECELERATION_X 4
#define PLAYER_VERTICAL_IMPULSE 18
#define PLAYER_MAX_SPEED_Y 24
#define PLAYER_VERTICAL_DRAG 2

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

static const INT8 swim_bob_offsets[8] = {
    0, -1, -1, 0, 0, 1, 1, 0
};

static const INT8 idle_sway_offsets[8] = {
    0, 1, 1, 0, 0, -1, -1, 0
};

static PlayerState player;
static FishState level_fish[2];
static UINT8 level_animation_tick = 0u;

static INT16 clamp_int16(INT16 value, INT16 minimum, INT16 maximum) {
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static INT16 approach_int16(INT16 value, INT16 target, INT16 step) {
    if (value < target) {
        value += step;
        if (value > target) {
            value = target;
        }
    } else if (value > target) {
        value -= step;
        if (value < target) {
            value = target;
        }
    }

    return value;
}

static UINT8 apply_render_offset(UINT8 value, INT8 offset, UINT8 minimum, UINT8 maximum) {
    INT16 adjusted = (INT16)value + (INT16)offset;

    if (adjusted < minimum) {
        adjusted = minimum;
    } else if (adjusted > maximum) {
        adjusted = maximum;
    }

    return (UINT8)adjusted;
}

static void hide_all_sprites(void) {
    UINT8 sprite_index;

    for (sprite_index = 0u; sprite_index != 40u; ++sprite_index) {
        move_sprite(sprite_index, 0u, 0u);
        set_sprite_prop(sprite_index, 0u);
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

    gotoxy(0u, 15u);
    printf("START: BACK");
}

static void draw_diver_sprite(UINT8 x, UINT8 y, UINT8 facing_left) {
    UINT8 base_tile = PLAYER_TILE_FIRST;
    UINT8 sprite_index = PLAYER_SPRITE_FIRST;
    UINT8 row;
    UINT8 col;
    UINT8 source_col;
    UINT8 tile_index;

    for (row = 0u; row != DIVER_SWIMMING_SPRITE_HEIGHT_TILES; ++row) {
        for (col = 0u; col != DIVER_SWIMMING_SPRITE_WIDTH_TILES; ++col) {
            source_col = facing_left
                ? (UINT8)(DIVER_SWIMMING_SPRITE_WIDTH_TILES - 1u - col)
                : col;
            tile_index = (UINT8)(
                base_tile + (row * DIVER_SWIMMING_SPRITE_WIDTH_TILES) + source_col
            );

            set_sprite_tile(sprite_index, tile_index);
            move_sprite(sprite_index, x + (col << 3), y + (row << 3));
            set_sprite_prop(sprite_index, facing_left ? S_FLIPX : 0u);
            ++sprite_index;
        }
    }
}

static void draw_level_one_hud(void) {
    gotoxy(0u, 0u);
    printf("1 Bubble ST:BACK   ");
    gotoxy(0u, 1u);
    printf("< > swim  A^  Bv   ");
}

static void initialize_level_one(void) {
    player.x = TO_FIXED(80u);
    player.y = TO_FIXED(92u);
    player.velocity_x = 0;
    player.velocity_y = 0;
    player.facing_left = 0u;
    level_animation_tick = 0u;

    level_fish[0].x = TO_FIXED(32u);
    level_fish[0].y = 64u;
    level_fish[0].velocity_x = 8;
    level_fish[0].minimum_x = 24u;
    level_fish[0].maximum_x = 152u;
    level_fish[0].sprite_index = FISH_A_SPRITE;

    level_fish[1].x = TO_FIXED(136u);
    level_fish[1].y = 112u;
    level_fish[1].velocity_x = -6;
    level_fish[1].minimum_x = 16u;
    level_fish[1].maximum_x = 144u;
    level_fish[1].sprite_index = FISH_B_SPRITE;
}

static void draw_level_one_screen(void) {
    cls();
    hide_all_sprites();
    set_sprite_data(PLAYER_TILE_FIRST, DIVER_SWIMMING_SPRITE_TILE_COUNT, diver_swimming_sprite_tiles);
    set_sprite_data(FISH_TILE_FIRST, FISH_YELLOW_SPRITE_TILE_COUNT, fish_yellow_sprite_tiles);
    draw_level_one_hud();
}

static void update_level_fish(FishState *fish) {
    fish->x += fish->velocity_x;

    if (fish->x < TO_FIXED(fish->minimum_x)) {
        fish->x = TO_FIXED(fish->minimum_x);
        fish->velocity_x = (INT16)(-fish->velocity_x);
    } else if (fish->x > TO_FIXED(fish->maximum_x)) {
        fish->x = TO_FIXED(fish->maximum_x);
        fish->velocity_x = (INT16)(-fish->velocity_x);
    }
}

static void update_player(UINT8 current_keys, UINT8 pressed_keys) {
    INT16 target_velocity_x = 0;

    if ((current_keys & J_LEFT) && !(current_keys & J_RIGHT)) {
        target_velocity_x = -PLAYER_MAX_SPEED_X;
        player.facing_left = 1u;
    } else if ((current_keys & J_RIGHT) && !(current_keys & J_LEFT)) {
        target_velocity_x = PLAYER_MAX_SPEED_X;
        player.facing_left = 0u;
    }

    if (target_velocity_x == 0) {
        player.velocity_x = approach_int16(player.velocity_x, 0, PLAYER_DECELERATION_X);
    } else {
        player.velocity_x = approach_int16(player.velocity_x, target_velocity_x, PLAYER_ACCELERATION_X);
    }

    if ((pressed_keys & J_A) && !(pressed_keys & J_B)) {
        player.velocity_y -= PLAYER_VERTICAL_IMPULSE;
    } else if ((pressed_keys & J_B) && !(pressed_keys & J_A)) {
        player.velocity_y += PLAYER_VERTICAL_IMPULSE;
    }

    player.velocity_y = clamp_int16(
        player.velocity_y,
        -PLAYER_MAX_SPEED_Y,
        PLAYER_MAX_SPEED_Y
    );

    player.x += player.velocity_x;
    player.y += player.velocity_y;

    if (player.x < TO_FIXED(LEVEL_ONE_MIN_X)) {
        player.x = TO_FIXED(LEVEL_ONE_MIN_X);
        player.velocity_x = 0;
    } else if (player.x > TO_FIXED(LEVEL_ONE_MAX_X)) {
        player.x = TO_FIXED(LEVEL_ONE_MAX_X);
        player.velocity_x = 0;
    }

    if (player.y < TO_FIXED(LEVEL_ONE_MIN_Y)) {
        player.y = TO_FIXED(LEVEL_ONE_MIN_Y);
        player.velocity_y = 0;
    } else if (player.y > TO_FIXED(LEVEL_ONE_MAX_Y)) {
        player.y = TO_FIXED(LEVEL_ONE_MAX_Y);
        player.velocity_y = 0;
    }

    player.velocity_y = approach_int16(player.velocity_y, 0, PLAYER_VERTICAL_DRAG);
}

static void render_level_one(void) {
    UINT8 player_x = FROM_FIXED(player.x);
    UINT8 player_y = FROM_FIXED(player.y);
    UINT8 diver_wave = (level_animation_tick >> 2) & 0x07u;
    UINT8 diver_sway_wave = (level_animation_tick >> 3) & 0x07u;
    UINT8 diver_render_x = player_x;
    UINT8 diver_render_y = apply_render_offset(
        player_y,
        swim_bob_offsets[diver_wave],
        LEVEL_ONE_MIN_Y,
        LEVEL_ONE_MAX_Y
    );
    UINT8 fish_index;
    UINT8 fish_wave;
    UINT8 fish_x;
    UINT8 fish_y;
    UINT8 fish_attributes;
    INT8 sway_offset;

    if ((player.velocity_x > -4) && (player.velocity_x < 4)) {
        sway_offset = idle_sway_offsets[diver_sway_wave];

        if (player.facing_left) {
            sway_offset = (INT8)(-sway_offset);
        }

        diver_render_x = apply_render_offset(
            player_x,
            sway_offset,
            LEVEL_ONE_MIN_X,
            LEVEL_ONE_MAX_X
        );
    }

    draw_diver_sprite(diver_render_x, diver_render_y, player.facing_left);

    for (fish_index = 0u; fish_index != 2u; ++fish_index) {
        fish_wave = (UINT8)(((level_animation_tick >> 2) + (fish_index << 2)) & 0x07u);
        fish_x = FROM_FIXED(level_fish[fish_index].x);
        fish_y = apply_render_offset(
            level_fish[fish_index].y,
            swim_bob_offsets[fish_wave],
            24u,
            152u
        );
        fish_attributes = (level_fish[fish_index].velocity_x > 0) ? S_FLIPX : 0u;

        fish_yellow_sprite_show(
            FISH_TILE_FIRST,
            level_fish[fish_index].sprite_index,
            0u,
            fish_x,
            fish_y
        );
        set_sprite_prop(level_fish[fish_index].sprite_index, fish_attributes);
    }
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

        if (screen == SCREEN_TITLE) {
            ++title_timer;
            if ((title_timer >= TITLE_SCREEN_DURATION_FRAMES) || (pressed_keys & (J_A | J_START))) {
                screen = SCREEN_LEVEL_SELECT;
                draw_level_select_screen(selected_level);
            }
        } else if (screen == SCREEN_LEVEL_SELECT) {
            if ((pressed_keys & J_UP) && (selected_level > 0u)) {
                --selected_level;
                move_level_select_cursor(selected_level);
            } else if ((pressed_keys & J_DOWN) && (selected_level + 1u < LEVEL_COUNT)) {
                ++selected_level;
                move_level_select_cursor(selected_level);
            } else if ((pressed_keys & (J_A | J_START)) && level_unlocked[selected_level]) {
                if (selected_level == 0u) {
                    initialize_level_one();
                    draw_level_one_screen();
                    render_level_one();
                    screen = SCREEN_LEVEL_ONE;
                } else {
                    screen = SCREEN_LEVEL_PLACEHOLDER;
                    draw_level_placeholder_screen(selected_level);
                }
            }
        } else if (screen == SCREEN_LEVEL_ONE) {
            if (pressed_keys & J_START) {
                screen = SCREEN_LEVEL_SELECT;
                draw_level_select_screen(selected_level);
            } else {
                ++level_animation_tick;
                update_player(current_keys, pressed_keys);
                update_level_fish(&level_fish[0]);
                update_level_fish(&level_fish[1]);
                render_level_one();
            }
        } else if (screen == SCREEN_LEVEL_PLACEHOLDER) {
            if (pressed_keys & J_START) {
                screen = SCREEN_LEVEL_SELECT;
                draw_level_select_screen(selected_level);
            }
        }
    }
}
