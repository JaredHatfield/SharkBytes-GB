#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdio.h>

#include "levels_generated.h"
#include "sprites/diver_swimming_sprite.h"
#include "sprites/diver_title_sprite.h"
#include "sprites/fish_striped_sprite.h"
#include "sprites/fish_yellow_sprite.h"

typedef enum ScreenState {
    SCREEN_TITLE = 0,
    SCREEN_LEVEL_SELECT,
    SCREEN_LEVEL_PLAYING,
    SCREEN_LEVEL_COMPLETE,
    SCREEN_LEVEL_FAILED,
    SCREEN_LEVEL_PLACEHOLDER
} ScreenState;

typedef enum LevelFailureReason {
    LEVEL_FAILURE_AIR = 0,
    LEVEL_FAILURE_HEALTH
} LevelFailureReason;

typedef enum LevelUpdateResult {
    LEVEL_RUNNING = 0,
    LEVEL_WON,
    LEVEL_LOST
} LevelUpdateResult;

typedef struct PlayerState {
    INT16 x;
    INT16 y;
    INT16 velocity_x;
    INT16 velocity_y;
    UINT8 facing_left;
} PlayerState;

typedef struct ActiveFishState {
    INT16 x;
    UINT8 y;
    INT16 velocity_x;
    UINT8 minimum_x;
    UINT8 maximum_x;
    UINT8 width;
    UINT8 height;
    UINT8 tile_first;
    UINT8 sprite_count;
    UINT8 kind;
    UINT16 lifetime_remaining;
    UINT8 active;
} ActiveFishState;

typedef struct AirPickupState {
    INT16 x;
    INT16 y;
    INT16 velocity_y;
    UINT16 spawn_timer;
    UINT8 active;
} AirPickupState;

#define TITLE_SCREEN_DURATION_FRAMES 180u
#define LEVEL_SELECT_CURSOR_TILE 0u
#define LEVEL_SELECT_CURSOR_SPRITE 0u

#define FIX_SHIFT 4u
#define FIX_SCALE 16
#define TO_FIXED(value) ((INT16)((value) << FIX_SHIFT))
#define FROM_FIXED(value) ((UINT8)((value) >> FIX_SHIFT))

#define SCREEN_TILE_WIDTH 20u

#define PLAYER_TILE_FIRST 0u
#define PLAYER_SPRITE_FIRST 0u
#define PLAYER_SPRITE_COUNT DIVER_SWIMMING_SPRITE_TILES_PER_FRAME
#define FISH_YELLOW_TILE_FIRST (PLAYER_TILE_FIRST + DIVER_SWIMMING_SPRITE_TILE_COUNT)
#define FISH_STRIPED_TILE_FIRST (FISH_YELLOW_TILE_FIRST + FISH_YELLOW_SPRITE_TILE_COUNT)
#define AIR_PICKUP_TILE_FIRST (FISH_STRIPED_TILE_FIRST + FISH_STRIPED_SPRITE_TILE_COUNT)

#define FISH_SLOT_SPRITE_STRIDE 2u
#define FISH_SPRITE_FIRST (PLAYER_SPRITE_FIRST + PLAYER_SPRITE_COUNT)
#define AIR_PICKUP_SPRITE_FIRST (FISH_SPRITE_FIRST + (LEVELS_MAX_FISH_ON_SCREEN * FISH_SLOT_SPRITE_STRIDE))

#define HUD_BKG_TILE_FIRST 128u
#define HUD_BKG_BLANK_TILE HUD_BKG_TILE_FIRST
#define HUD_BKG_LINE_TILE (HUD_BKG_TILE_FIRST + 1u)
#define HUD_BKG_HEART_FULL_TILE (HUD_BKG_TILE_FIRST + 2u)
#define HUD_BKG_HEART_EMPTY_TILE (HUD_BKG_TILE_FIRST + 3u)
#define HUD_BKG_AIR_TILE (HUD_BKG_TILE_FIRST + 4u)
#define HUD_BKG_CAUGHT_FISH_TILE (HUD_BKG_TILE_FIRST + 5u)

#define HEART_HUD_X 1u
#define HEART_HUD_Y 0u
#define AIR_HUD_X 1u
#define AIR_HUD_Y 1u
#define TOP_HUD_DIVIDER_ROW 2u

#define BOTTOM_HUD_DIVIDER_ROW 15u
#define CAPTURE_ROW 16u
#define CAPTURE_HUD_X 5u

#define PLAYFIELD_VISIBLE_TOP 24u
#define PLAYFIELD_VISIBLE_BOTTOM 119u
#define LEVEL_MIN_X 8u
#define LEVEL_MAX_X (168u - DIVER_SWIMMING_SPRITE_WIDTH)
#define LEVEL_MIN_Y (PLAYFIELD_VISIBLE_TOP + 16u)
#define LEVEL_MAX_Y (PLAYFIELD_VISIBLE_BOTTOM - DIVER_SWIMMING_SPRITE_HEIGHT + 17u)
#define AIR_PICKUP_START_Y (PLAYFIELD_VISIBLE_BOTTOM - 8u + 17u)

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

static const unsigned char hud_tiles[] = {
    /* blank */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* divider line */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    /* full heart */
    0x00, 0x00, 0x6C, 0x6C, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0x7C, 0x7C, 0x38, 0x38, 0x10, 0x10,
    /* empty heart */
    0x00, 0x00, 0x6C, 0x6C, 0xFE, 0xFE, 0x92, 0x92,
    0x82, 0x82, 0x44, 0x44, 0x28, 0x28, 0x10, 0x10,
    /* bubble */
    0x18, 0x00, 0x24, 0x00, 0x42, 0x00, 0x81, 0x00,
    0x81, 0x00, 0x42, 0x00, 0x24, 0x00, 0x18, 0x00
};

static PlayerState player;
static ActiveFishState active_fish[LEVELS_MAX_FISH_ON_SCREEN];
static AirPickupState air_pickup;
static UINT8 level_unlocked[LEVEL_COUNT] = { 1u };
static UINT8 level_animation_tick = 0u;
static UINT8 player_air = 0u;
static UINT8 player_health = 0u;
static UINT8 captured_fish_count = 0u;
static UINT8 current_level_index = 0u;
static UINT8 next_fish_type_index = 0u;
static UINT8 rng_state = 0x5Au;
static UINT8 hud_row_buffer[SCREEN_TILE_WIDTH];
static UINT16 air_loss_timer = 0u;
static const LevelDefinition *current_level = 0;
static LevelFailureReason level_failure_reason = LEVEL_FAILURE_AIR;

static const INT8 swim_bob_offsets[8] = {
    0, -1, -1, 0, 0, 1, 1, 0
};

static const INT8 idle_sway_offsets[8] = {
    0, 1, 1, 0, 0, -1, -1, 0
};

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

static UINT8 next_random_u8(void) {
    UINT8 lsb = rng_state & 0x01u;

    rng_state >>= 1u;
    if (lsb != 0u) {
        rng_state ^= 0xB8u;
    }

    if (rng_state == 0u) {
        rng_state = 0x5Au;
    }

    return rng_state;
}

static UINT8 sprite_to_left(UINT8 sprite_x) {
    return sprite_x - 8u;
}

static UINT8 sprite_to_top(UINT8 sprite_y) {
    return sprite_y - 16u;
}

static UINT8 boxes_overlap(UINT8 ax, UINT8 ay, UINT8 aw, UINT8 ah, UINT8 bx, UINT8 by, UINT8 bw, UINT8 bh) {
    if ((ax + aw) <= bx) {
        return 0u;
    }

    if ((bx + bw) <= ax) {
        return 0u;
    }

    if ((ay + ah) <= by) {
        return 0u;
    }

    if ((by + bh) <= ay) {
        return 0u;
    }

    return 1u;
}

static UINT8 fish_slot_sprite_index(UINT8 slot_index) {
    return (UINT8)(FISH_SPRITE_FIRST + (slot_index * FISH_SLOT_SPRITE_STRIDE));
}

static void hide_sprite_range(UINT8 first_sprite, UINT8 sprite_count) {
    UINT8 sprite_index;

    for (sprite_index = first_sprite; sprite_index != first_sprite + sprite_count; ++sprite_index) {
        move_sprite(sprite_index, 0u, 0u);
        set_sprite_prop(sprite_index, 0u);
    }
}

static void hide_all_sprites(void) {
    hide_sprite_range(0u, 40u);
}

static void set_sprite_prop_range(UINT8 first_sprite, UINT8 sprite_count, UINT8 prop) {
    UINT8 sprite_index;

    for (sprite_index = first_sprite; sprite_index != first_sprite + sprite_count; ++sprite_index) {
        set_sprite_prop(sprite_index, prop);
    }
}

static void set_single_bkg_tile(UINT8 x, UINT8 y, UINT8 tile) {
    set_bkg_tiles(x, y, 1u, 1u, &tile);
}

static void fill_bkg_row(UINT8 row, UINT8 tile) {
    UINT8 column;

    for (column = 0u; column != SCREEN_TILE_WIDTH; ++column) {
        hud_row_buffer[column] = tile;
    }

    set_bkg_tiles(0u, row, SCREEN_TILE_WIDTH, 1u, hud_row_buffer);
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
    printf("%s", level_definitions[level_index].name);

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
    printf("%s", level_definitions[level_index].name);

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

static void draw_level_hud_frame(void) {
    fill_bkg_row(0u, HUD_BKG_BLANK_TILE);
    fill_bkg_row(1u, HUD_BKG_BLANK_TILE);
    fill_bkg_row(TOP_HUD_DIVIDER_ROW, HUD_BKG_LINE_TILE);
    fill_bkg_row(CAPTURE_ROW, HUD_BKG_BLANK_TILE);
    fill_bkg_row(CAPTURE_ROW + 1u, HUD_BKG_BLANK_TILE);
    fill_bkg_row(BOTTOM_HUD_DIVIDER_ROW, HUD_BKG_LINE_TILE);
}

static void update_level_hud(void) {
    UINT8 index;
    UINT8 tile;

    for (index = 0u; index != PLAYER_MAX_HEALTH; ++index) {
        tile = (index < player_health) ? HUD_BKG_HEART_FULL_TILE : HUD_BKG_HEART_EMPTY_TILE;
        set_single_bkg_tile((UINT8)(HEART_HUD_X + index), HEART_HUD_Y, tile);
    }

    for (index = 0u; index != PLAYER_MAX_AIR; ++index) {
        tile = (index < player_air) ? HUD_BKG_AIR_TILE : HUD_BKG_BLANK_TILE;
        set_single_bkg_tile((UINT8)(AIR_HUD_X + index), AIR_HUD_Y, tile);
    }

    for (index = 0u; index != MAX_CAPTURED_FISH; ++index) {
        tile = (index < captured_fish_count) ? HUD_BKG_CAUGHT_FISH_TILE : HUD_BKG_BLANK_TILE;
        set_single_bkg_tile((UINT8)(CAPTURE_HUD_X + index), CAPTURE_ROW, tile);
    }
}

static void print_centered(UINT8 row, const char *text) {
    UINT8 length = 0u;
    UINT8 column;

    while (text[length] != '\0') {
        ++length;
    }

    column = (UINT8)((SCREEN_TILE_WIDTH - length) >> 1);
    gotoxy(column, row);
    printf("%s", text);
}

static void draw_level_result_message(UINT8 won) {
    hide_all_sprites();

    gotoxy(0u, 7u);
    printf("                    ");
    gotoxy(0u, 9u);
    printf("                    ");
    gotoxy(0u, 11u);
    printf("                    ");

    if (won) {
        print_centered(7u, "LEVEL CLEAR");
        print_centered(9u, "A: AGAIN");
        print_centered(11u, "START: BACK");
    } else {
        print_centered(7u, level_failure_reason == LEVEL_FAILURE_AIR ? "OUT OF AIR" : "NO HEALTH");
        print_centered(9u, "A: RETRY");
        print_centered(11u, "START: BACK");
    }
}

static void load_level_assets(void) {
    set_sprite_data(PLAYER_TILE_FIRST, DIVER_SWIMMING_SPRITE_TILE_COUNT, diver_swimming_sprite_tiles);
    set_sprite_data(FISH_YELLOW_TILE_FIRST, FISH_YELLOW_SPRITE_TILE_COUNT, fish_yellow_sprite_tiles);
    set_sprite_data(FISH_STRIPED_TILE_FIRST, FISH_STRIPED_SPRITE_TILE_COUNT, fish_striped_sprite_tiles);
    set_sprite_data(AIR_PICKUP_TILE_FIRST, 1u, &hud_tiles[64]);

    set_bkg_data(HUD_BKG_TILE_FIRST, 5u, hud_tiles);
    set_bkg_data(HUD_BKG_CAUGHT_FISH_TILE, 1u, fish_yellow_sprite_tiles);
}

static void configure_active_fish_from_kind(ActiveFishState *fish) {
    if (fish->kind == LEVEL_FISH_KIND_YELLOW) {
        fish->tile_first = FISH_YELLOW_TILE_FIRST;
        fish->sprite_count = FISH_YELLOW_SPRITE_TILES_PER_FRAME;
        fish->width = FISH_YELLOW_SPRITE_WIDTH;
        fish->height = FISH_YELLOW_SPRITE_HEIGHT;
    } else {
        fish->tile_first = FISH_STRIPED_TILE_FIRST;
        fish->sprite_count = FISH_STRIPED_SPRITE_TILES_PER_FRAME;
        fish->width = FISH_STRIPED_SPRITE_WIDTH;
        fish->height = FISH_STRIPED_SPRITE_HEIGHT;
    }
}

static UINT8 count_active_fish(void) {
    UINT8 count = 0u;
    UINT8 slot_index;

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        if (active_fish[slot_index].active) {
            ++count;
        }
    }

    return count;
}

static void spawn_fish_into_slot(UINT8 slot_index) {
    const LevelFishTypeDefinition *fish_type;
    ActiveFishState *fish;

    if (current_level->fish_type_count == 0u) {
        return;
    }

    fish_type = &current_level->fish_types[next_fish_type_index];
    next_fish_type_index = (UINT8)((next_fish_type_index + 1u) % current_level->fish_type_count);

    fish = &active_fish[slot_index];
    fish->active = 1u;
    fish->kind = fish_type->kind;
    fish->y = fish_type->spawn_y;
    fish->minimum_x = fish_type->minimum_x;
    fish->maximum_x = fish_type->maximum_x;
    fish->lifetime_remaining = fish_type->lifetime_frames;
    fish->velocity_x = fish_type->start_from_left ? fish_type->speed_fixed : (INT16)(-(INT16)fish_type->speed_fixed);
    fish->x = TO_FIXED(fish_type->start_from_left ? fish_type->minimum_x : fish_type->maximum_x);
    configure_active_fish_from_kind(fish);
}

static void maintain_fish_population(void) {
    UINT8 active_count;
    UINT8 slot_index;

    if (current_level->goal_fish == 0u) {
        return;
    }

    active_count = count_active_fish();

    while ((active_count < current_level->max_fish_on_screen) &&
           ((UINT8)(captured_fish_count + active_count) < current_level->goal_fish)) {
        for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
            if (!active_fish[slot_index].active) {
                spawn_fish_into_slot(slot_index);
                ++active_count;
                break;
            }
        }
    }
}

static void initialize_level(UINT8 level_index) {
    UINT8 slot_index;

    current_level_index = level_index;
    current_level = &level_definitions[level_index];

    player.x = TO_FIXED(80u);
    player.y = TO_FIXED(80u);
    player.velocity_x = 0;
    player.velocity_y = 0;
    player.facing_left = 0u;

    level_animation_tick = 0u;
    player_air = current_level->starting_air;
    player_health = current_level->starting_hearts;
    captured_fish_count = 0u;
    next_fish_type_index = 0u;
    air_loss_timer = current_level->air.loss_frames;
    level_failure_reason = LEVEL_FAILURE_AIR;

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        active_fish[slot_index].active = 0u;
        hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);
    }

    air_pickup.active = 0u;
    air_pickup.spawn_timer = current_level->air.spawn_frames;
    air_pickup.x = TO_FIXED(0u);
    air_pickup.y = TO_FIXED(AIR_PICKUP_START_Y);
    air_pickup.velocity_y = (INT16)(-(INT16)current_level->air.rise_speed_fixed);

    maintain_fish_population();
}

static void draw_level_screen(void) {
    cls();
    hide_all_sprites();
    load_level_assets();
    draw_level_hud_frame();
    update_level_hud();
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

    player.velocity_y = clamp_int16(player.velocity_y, -PLAYER_MAX_SPEED_Y, PLAYER_MAX_SPEED_Y);

    player.x += player.velocity_x;
    player.y += player.velocity_y;

    if (player.x < TO_FIXED(LEVEL_MIN_X)) {
        player.x = TO_FIXED(LEVEL_MIN_X);
        player.velocity_x = 0;
    } else if (player.x > TO_FIXED(LEVEL_MAX_X)) {
        player.x = TO_FIXED(LEVEL_MAX_X);
        player.velocity_x = 0;
    }

    if (player.y < TO_FIXED(LEVEL_MIN_Y)) {
        player.y = TO_FIXED(LEVEL_MIN_Y);
        player.velocity_y = 0;
    } else if (player.y > TO_FIXED(LEVEL_MAX_Y)) {
        player.y = TO_FIXED(LEVEL_MAX_Y);
        player.velocity_y = 0;
    }

    player.velocity_y = approach_int16(player.velocity_y, 0, PLAYER_VERTICAL_DRAG);
}

static void update_active_fish(void) {
    UINT8 slot_index;
    ActiveFishState *fish;

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        fish = &active_fish[slot_index];

        if (!fish->active) {
            continue;
        }

        fish->x += fish->velocity_x;

        if (fish->x < TO_FIXED(fish->minimum_x)) {
            fish->x = TO_FIXED(fish->minimum_x);
            fish->velocity_x = (INT16)(-fish->velocity_x);
        } else if (fish->x > TO_FIXED(fish->maximum_x)) {
            fish->x = TO_FIXED(fish->maximum_x);
            fish->velocity_x = (INT16)(-fish->velocity_x);
        }

        if (fish->lifetime_remaining > 0u) {
            --fish->lifetime_remaining;
        }

        if (fish->lifetime_remaining == 0u) {
            fish->active = 0u;
            hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);
        }
    }
}

static void spawn_air_pickup(void) {
    UINT8 x_range = LEVEL_MAX_X - LEVEL_MIN_X;

    air_pickup.active = 1u;
    air_pickup.x = TO_FIXED((UINT8)(LEVEL_MIN_X + (next_random_u8() % (x_range + 1u))));
    air_pickup.y = TO_FIXED(AIR_PICKUP_START_Y);
    air_pickup.velocity_y = (INT16)(-(INT16)current_level->air.rise_speed_fixed);
}

static void update_air_system(void) {
    if (!current_level->air.enabled) {
        return;
    }

    if (air_loss_timer > 0u) {
        --air_loss_timer;
    }

    if (air_loss_timer == 0u) {
        if (player_air > 0u) {
            --player_air;
        }
        air_loss_timer = current_level->air.loss_frames;
    }

    if (air_pickup.active) {
        air_pickup.y += air_pickup.velocity_y;

        if (air_pickup.y < TO_FIXED(LEVEL_MIN_Y)) {
            air_pickup.active = 0u;
            air_pickup.spawn_timer = current_level->air.spawn_frames;
            hide_sprite_range(AIR_PICKUP_SPRITE_FIRST, 1u);
        }
        return;
    }

    if (air_pickup.spawn_timer > 0u) {
        --air_pickup.spawn_timer;
    }

    if (air_pickup.spawn_timer == 0u) {
        spawn_air_pickup();
    }
}

static void render_fish(UINT8 slot_index, const ActiveFishState *fish, UINT8 x, UINT8 y) {
    UINT8 sprite_index = fish_slot_sprite_index(slot_index);
    UINT8 fish_attributes = (fish->velocity_x > 0) ? S_FLIPX : 0u;

    if (fish->kind == LEVEL_FISH_KIND_YELLOW) {
        fish_yellow_sprite_show(fish->tile_first, sprite_index, 0u, x, y);
        hide_sprite_range((UINT8)(sprite_index + 1u), 1u);
    } else {
        fish_striped_sprite_show(fish->tile_first, sprite_index, 0u, x, y);
    }

    set_sprite_prop_range(sprite_index, fish->sprite_count, fish_attributes);
}

static void render_level(void) {
    UINT8 player_x = FROM_FIXED(player.x);
    UINT8 player_y = FROM_FIXED(player.y);
    UINT8 diver_wave = (level_animation_tick >> 2) & 0x07u;
    UINT8 diver_sway_wave = (level_animation_tick >> 3) & 0x07u;
    UINT8 diver_render_x = player_x;
    UINT8 diver_render_y = apply_render_offset(player_y, swim_bob_offsets[diver_wave], LEVEL_MIN_Y, LEVEL_MAX_Y);
    UINT8 slot_index;
    UINT8 fish_wave;
    UINT8 fish_x;
    UINT8 fish_y;
    INT8 sway_offset;

    if ((player.velocity_x > -4) && (player.velocity_x < 4)) {
        sway_offset = idle_sway_offsets[diver_sway_wave];
        if (player.facing_left) {
            sway_offset = (INT8)(-sway_offset);
        }

        diver_render_x = apply_render_offset(player_x, sway_offset, LEVEL_MIN_X, LEVEL_MAX_X);
    }

    draw_diver_sprite(diver_render_x, diver_render_y, player.facing_left);

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        if (!active_fish[slot_index].active) {
            hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);
            continue;
        }

        fish_wave = (UINT8)(((level_animation_tick >> 2) + (slot_index << 2)) & 0x07u);
        fish_x = FROM_FIXED(active_fish[slot_index].x);
        fish_y = apply_render_offset(
            active_fish[slot_index].y,
            swim_bob_offsets[fish_wave],
            LEVEL_MIN_Y,
            (UINT8)(PLAYFIELD_VISIBLE_BOTTOM - active_fish[slot_index].height + 17u)
        );
        render_fish(slot_index, &active_fish[slot_index], fish_x, fish_y);
    }

    if (air_pickup.active) {
        set_sprite_tile(AIR_PICKUP_SPRITE_FIRST, AIR_PICKUP_TILE_FIRST);
        move_sprite(AIR_PICKUP_SPRITE_FIRST, FROM_FIXED(air_pickup.x), FROM_FIXED(air_pickup.y));
        set_sprite_prop(AIR_PICKUP_SPRITE_FIRST, 0u);
    } else {
        hide_sprite_range(AIR_PICKUP_SPRITE_FIRST, 1u);
    }
}

static void capture_fish(UINT8 slot_index) {
    active_fish[slot_index].active = 0u;
    hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);

    if (captured_fish_count < MAX_CAPTURED_FISH) {
        ++captured_fish_count;
    }
}

static void collect_air_pickup(void) {
    air_pickup.active = 0u;
    air_pickup.spawn_timer = current_level->air.spawn_frames;
    hide_sprite_range(AIR_PICKUP_SPRITE_FIRST, 1u);
    player_air = (UINT8)(player_air + current_level->air.air_gain);
    if (player_air > PLAYER_MAX_AIR) {
        player_air = PLAYER_MAX_AIR;
    }
}

static void check_level_collisions(void) {
    UINT8 player_left = sprite_to_left(FROM_FIXED(player.x));
    UINT8 player_top = sprite_to_top(FROM_FIXED(player.y));
    UINT8 slot_index;
    UINT8 fish_left;
    UINT8 fish_top;
    UINT8 bubble_left;
    UINT8 bubble_top;

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        if (!active_fish[slot_index].active) {
            continue;
        }

        fish_left = sprite_to_left(FROM_FIXED(active_fish[slot_index].x));
        fish_top = sprite_to_top(active_fish[slot_index].y);

        if (boxes_overlap(
            player_left,
            player_top,
            DIVER_SWIMMING_SPRITE_WIDTH,
            DIVER_SWIMMING_SPRITE_HEIGHT,
            fish_left,
            fish_top,
            active_fish[slot_index].width,
            active_fish[slot_index].height
        )) {
            capture_fish(slot_index);
        }
    }

    if (!air_pickup.active) {
        return;
    }

    bubble_left = sprite_to_left(FROM_FIXED(air_pickup.x));
    bubble_top = sprite_to_top(FROM_FIXED(air_pickup.y));

    if (boxes_overlap(
        player_left,
        player_top,
        DIVER_SWIMMING_SPRITE_WIDTH,
        DIVER_SWIMMING_SPRITE_HEIGHT,
        bubble_left,
        bubble_top,
        8u,
        8u
    )) {
        collect_air_pickup();
    }
}

static LevelUpdateResult update_level(UINT8 current_keys, UINT8 pressed_keys) {
    ++level_animation_tick;

    update_player(current_keys, pressed_keys);
    update_active_fish();
    maintain_fish_population();
    update_air_system();
    check_level_collisions();
    maintain_fish_population();
    render_level();
    update_level_hud();

    if (captured_fish_count >= current_level->goal_fish) {
        if ((current_level_index + 1u) < LEVEL_COUNT) {
            level_unlocked[current_level_index + 1u] = 1u;
        }
        return LEVEL_WON;
    }

    if (current_level->air.enabled && (player_air == 0u)) {
        level_failure_reason = LEVEL_FAILURE_AIR;
        return LEVEL_LOST;
    }

    if (player_health == 0u) {
        level_failure_reason = LEVEL_FAILURE_HEALTH;
        return LEVEL_LOST;
    }

    return LEVEL_RUNNING;
}

static void begin_level(UINT8 level_index) {
    initialize_level(level_index);
    draw_level_screen();
    render_level();
    update_level_hud();
}

void main(void) {
    font_t title_font;
    UINT8 current_keys;
    UINT8 pressed_keys;
    UINT8 previous_keys = 0u;
    UINT8 selected_level = 0u;
    UINT8 title_timer = 0u;
    ScreenState screen = SCREEN_TITLE;
    LevelUpdateResult level_result;

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
                if (level_definitions[selected_level].implemented) {
                    begin_level(selected_level);
                    screen = SCREEN_LEVEL_PLAYING;
                } else {
                    screen = SCREEN_LEVEL_PLACEHOLDER;
                    draw_level_placeholder_screen(selected_level);
                }
            }
        } else if (screen == SCREEN_LEVEL_PLAYING) {
            if (pressed_keys & J_START) {
                screen = SCREEN_LEVEL_SELECT;
                draw_level_select_screen(selected_level);
            } else {
                level_result = update_level(current_keys, pressed_keys);

                if (level_result == LEVEL_WON) {
                    draw_level_result_message(1u);
                    screen = SCREEN_LEVEL_COMPLETE;
                } else if (level_result == LEVEL_LOST) {
                    draw_level_result_message(0u);
                    screen = SCREEN_LEVEL_FAILED;
                }
            }
        } else if ((screen == SCREEN_LEVEL_COMPLETE) || (screen == SCREEN_LEVEL_FAILED)) {
            if (pressed_keys & J_A) {
                begin_level(selected_level);
                screen = SCREEN_LEVEL_PLAYING;
            } else if (pressed_keys & J_START) {
                screen = SCREEN_LEVEL_SELECT;
                draw_level_select_screen(selected_level);
            }
        } else if (screen == SCREEN_LEVEL_PLACEHOLDER) {
            if (pressed_keys & J_START) {
                screen = SCREEN_LEVEL_SELECT;
                draw_level_select_screen(selected_level);
            }
        }
    }
}
