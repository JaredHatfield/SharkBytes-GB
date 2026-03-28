#include <gb/gb.h>
#include <gbdk/console.h>

#include "display_helpers.h"
#include "level_scene.h"
#include "levels_generated.h"
#include "sprites/diver_swimming_sprite.h"
#include "sprites/fish_clown_sprite.h"
#include "sprites/fish_puffer_sprite.h"
#include "sprites/hud_placeholder_tiles.h"

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
    UINT16 animation_frames;
    UINT8 active;
} ActiveFishState;

typedef struct AirPickupState {
    INT16 x;
    INT16 y;
    INT16 velocity_y;
    UINT16 spawn_timer;
    UINT8 active;
} AirPickupState;

#define FIX_SHIFT 4u
#define FIX_SCALE 16
#define TO_FIXED(value) ((INT16)((value) << FIX_SHIFT))
#define FROM_FIXED(value) ((UINT8)((value) >> FIX_SHIFT))

#define SCREEN_TILE_WIDTH 20u

#define PLAYER_TILE_FIRST 0u
#define PLAYER_SPRITE_FIRST 0u
#define PLAYER_SPRITE_COUNT DIVER_SWIMMING_SPRITE_TILES_PER_FRAME
#define FISH_CLOWN_TILE_FIRST (PLAYER_TILE_FIRST + DIVER_SWIMMING_SPRITE_TILE_COUNT)
#define FISH_PUFFER_TILE_FIRST (FISH_CLOWN_TILE_FIRST + FISH_CLOWN_SPRITE_TILE_COUNT)
#define AIR_PICKUP_TILE_FIRST (FISH_PUFFER_TILE_FIRST + FISH_PUFFER_SPRITE_TILE_COUNT)

#define FISH_SLOT_SPRITE_STRIDE 1u
#define FISH_SPRITE_FIRST (PLAYER_SPRITE_FIRST + PLAYER_SPRITE_COUNT)
#define AIR_PICKUP_SPRITE_FIRST (FISH_SPRITE_FIRST + (LEVELS_MAX_FISH_ON_SCREEN * FISH_SLOT_SPRITE_STRIDE))

#define HUD_BKG_TILE_FIRST 128u
#define HUD_BKG_BLANK_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_BLANK_TILE_INDEX)
#define HUD_BKG_LINE_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_LINE_TILE_INDEX)
#define HUD_BKG_HEART_FULL_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_HEART_FULL_TILE_INDEX)
#define HUD_BKG_HEART_EMPTY_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_HEART_EMPTY_TILE_INDEX)
#define HUD_BKG_AIR_FULL_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_AIR_FULL_TILE_INDEX)
#define HUD_BKG_AIR_EMPTY_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_AIR_EMPTY_TILE_INDEX)
#define HUD_BKG_CAPTURE_EMPTY_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_CAPTURE_EMPTY_TILE_INDEX)
#define HUD_BKG_CAPTURE_CLOWN_TILE (HUD_BKG_TILE_FIRST + HUD_PLACEHOLDER_TILE_COUNT)
#define HUD_BKG_CAPTURE_PUFFER_TILE (HUD_BKG_CAPTURE_CLOWN_TILE + 1u)

#define HEART_HUD_X 1u
#define HEART_HUD_Y 0u
#define AIR_HUD_X 1u
#define AIR_HUD_Y 1u
#define TOP_HUD_DIVIDER_ROW 2u

#define BOTTOM_HUD_DIVIDER_ROW 15u
#define CAPTURE_ROW 16u
#define CAPTURE_HUD_X 1u

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

static PlayerState player;
static ActiveFishState active_fish[LEVELS_MAX_FISH_ON_SCREEN];
static AirPickupState air_pickup;
static UINT16 level_animation_tick = 0u;
static UINT8 player_air = 0u;
static UINT8 player_health = 0u;
static UINT8 captured_fish_count = 0u;
static UINT8 captured_fish_kinds[MAX_CAPTURED_FISH];
static UINT8 next_fish_type_index = 0u;
static UINT8 rng_state = 0x5Au;
static UINT8 hud_row_buffer[SCREEN_TILE_WIDTH];
static UINT16 air_loss_timer = 0u;
static const LevelDefinition *current_level = 0;
static LevelFailureReason current_failure_reason = LEVEL_FAILURE_AIR;

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

static UINT8 clamp_uint8(UINT8 value, UINT8 minimum, UINT8 maximum) {
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static UINT8 absolute_difference_uint8(UINT8 left, UINT8 right) {
    return (left > right) ? (UINT8)(left - right) : (UINT8)(right - left);
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

static UINT8 hitbox_value(const unsigned char *hitboxes, UINT8 frame, UINT8 offset) {
    return hitboxes[(frame * 4u) + offset];
}

static void resolve_frame_hitbox(
    UINT8 sprite_x,
    UINT8 sprite_y,
    UINT8 sprite_width,
    const unsigned char *hitboxes,
    UINT8 frame,
    UINT8 mirrored,
    UINT8 *left,
    UINT8 *top,
    UINT8 *width,
    UINT8 *height
) {
    UINT8 local_left = hitbox_value(hitboxes, frame, 0u);
    UINT8 local_top = hitbox_value(hitboxes, frame, 1u);

    *width = hitbox_value(hitboxes, frame, 2u);
    *height = hitbox_value(hitboxes, frame, 3u);

    if (mirrored) {
        local_left = (UINT8)(sprite_width - local_left - *width);
    }

    *left = (UINT8)(sprite_to_left(sprite_x) + local_left);
    *top = (UINT8)(sprite_to_top(sprite_y) + local_top);
}

static UINT8 fish_slot_sprite_index(UINT8 slot_index) {
    return (UINT8)(FISH_SPRITE_FIRST + (slot_index * FISH_SLOT_SPRITE_STRIDE));
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

static UINT8 current_player_frame(void) {
    if ((current_level == 0) ||
        (DIVER_SWIMMING_SPRITE_FRAME_COUNT < 2u) ||
        (current_level->player.animation_frames == 0u)) {
        return 0u;
    }

    return (UINT8)((level_animation_tick / current_level->player.animation_frames) % DIVER_SWIMMING_SPRITE_FRAME_COUNT);
}

static void compute_diver_render_position(UINT8 *render_x, UINT8 *render_y) {
    UINT8 player_x = FROM_FIXED(player.x);
    UINT8 player_y = FROM_FIXED(player.y);
    UINT8 diver_wave = (level_animation_tick >> 2) & 0x07u;
    UINT8 diver_sway_wave = (level_animation_tick >> 3) & 0x07u;
    INT8 sway_offset;

    *render_x = player_x;
    *render_y = apply_render_offset(player_y, swim_bob_offsets[diver_wave], LEVEL_MIN_Y, LEVEL_MAX_Y);

    if ((player.velocity_x > -4) && (player.velocity_x < 4)) {
        sway_offset = idle_sway_offsets[diver_sway_wave];
        if (player.facing_left) {
            sway_offset = (INT8)(-sway_offset);
        }

        *render_x = apply_render_offset(player_x, sway_offset, LEVEL_MIN_X, LEVEL_MAX_X);
    }
}

static void compute_fish_render_position(UINT8 slot_index, UINT8 *render_x, UINT8 *render_y) {
    UINT8 fish_wave = (UINT8)(((level_animation_tick >> 2) + (slot_index << 2)) & 0x07u);

    *render_x = FROM_FIXED(active_fish[slot_index].x);
    *render_y = apply_render_offset(
        active_fish[slot_index].y,
        swim_bob_offsets[fish_wave],
        LEVEL_MIN_Y,
        (UINT8)(PLAYFIELD_VISIBLE_BOTTOM - active_fish[slot_index].height + 17u)
    );
}

static const unsigned char *fish_hitboxes_for_kind(UINT8 kind) {
    if (kind == LEVEL_FISH_KIND_CLOWN) {
        return fish_clown_sprite_frame_hitboxes;
    }

    return fish_puffer_sprite_frame_hitboxes;
}

static UINT8 fish_capture_bkg_tile_for_kind(UINT8 kind) {
    if (kind == LEVEL_FISH_KIND_CLOWN) {
        return HUD_BKG_CAPTURE_CLOWN_TILE;
    }

    return HUD_BKG_CAPTURE_PUFFER_TILE;
}

static UINT8 fish_frame_count_for_kind(UINT8 kind) {
    if (kind == LEVEL_FISH_KIND_CLOWN) {
        return FISH_CLOWN_SPRITE_FRAME_COUNT;
    }

    return FISH_PUFFER_SPRITE_FRAME_COUNT;
}

static UINT8 current_fish_frame(UINT8 slot_index, const ActiveFishState *fish) {
    UINT8 frame_count = fish_frame_count_for_kind(fish->kind);

    if ((frame_count < 2u) || (fish->animation_frames == 0u)) {
        return 0u;
    }

    return (UINT8)(((level_animation_tick + (slot_index * fish->animation_frames)) / fish->animation_frames) % frame_count);
}

static void draw_diver_sprite(UINT8 x, UINT8 y, UINT8 frame, UINT8 facing_left) {
    UINT8 sprite_index = PLAYER_SPRITE_FIRST;
    UINT8 tilemap_index = (UINT8)(frame * DIVER_SWIMMING_SPRITE_TILES_PER_FRAME);
    UINT8 row;
    UINT8 col;
    UINT8 source_col;

    for (row = 0u; row != DIVER_SWIMMING_SPRITE_HEIGHT_TILES; ++row) {
        for (col = 0u; col != DIVER_SWIMMING_SPRITE_WIDTH_TILES; ++col) {
            source_col = facing_left
                ? (UINT8)(DIVER_SWIMMING_SPRITE_WIDTH_TILES - 1u - col)
                : col;

            set_sprite_tile(
                sprite_index,
                (UINT8)(
                    PLAYER_TILE_FIRST + diver_swimming_sprite_frame_tilemap[
                        tilemap_index + (row * DIVER_SWIMMING_SPRITE_WIDTH_TILES) + source_col
                    ]
                )
            );
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
        tile = (index < player_air) ? HUD_BKG_AIR_FULL_TILE : HUD_BKG_AIR_EMPTY_TILE;
        set_single_bkg_tile((UINT8)(AIR_HUD_X + index), AIR_HUD_Y, tile);
    }

    for (index = 0u; index != MAX_CAPTURED_FISH; ++index) {
        tile = (index < captured_fish_count)
            ? fish_capture_bkg_tile_for_kind(captured_fish_kinds[index])
            : HUD_BKG_CAPTURE_EMPTY_TILE;
        set_single_bkg_tile((UINT8)(CAPTURE_HUD_X + index), CAPTURE_ROW, tile);
    }
}

static void load_level_assets(void) {
    set_sprite_data(PLAYER_TILE_FIRST, DIVER_SWIMMING_SPRITE_TILE_COUNT, diver_swimming_sprite_tiles);
    set_sprite_data(FISH_CLOWN_TILE_FIRST, FISH_CLOWN_SPRITE_TILE_COUNT, fish_clown_sprite_tiles);
    set_sprite_data(FISH_PUFFER_TILE_FIRST, FISH_PUFFER_SPRITE_TILE_COUNT, fish_puffer_sprite_tiles);
    set_sprite_data(AIR_PICKUP_TILE_FIRST, 1u, HUD_PLACEHOLDER_AIR_PICKUP_TILE_DATA);

    set_bkg_data(HUD_BKG_TILE_FIRST, HUD_PLACEHOLDER_TILE_COUNT, hud_placeholder_tiles);
    set_bkg_data(HUD_BKG_CAPTURE_CLOWN_TILE, 1u, fish_clown_sprite_tiles);
    set_bkg_data(HUD_BKG_CAPTURE_PUFFER_TILE, 1u, fish_puffer_sprite_tiles);
}

static void configure_active_fish_from_kind(ActiveFishState *fish) {
    if (fish->kind == LEVEL_FISH_KIND_CLOWN) {
        fish->tile_first = FISH_CLOWN_TILE_FIRST;
        fish->sprite_count = FISH_CLOWN_SPRITE_TILES_PER_FRAME;
        fish->width = FISH_CLOWN_SPRITE_WIDTH;
        fish->height = FISH_CLOWN_SPRITE_HEIGHT;
    } else {
        fish->tile_first = FISH_PUFFER_TILE_FIRST;
        fish->sprite_count = FISH_PUFFER_SPRITE_TILES_PER_FRAME;
        fish->width = FISH_PUFFER_SPRITE_WIDTH;
        fish->height = FISH_PUFFER_SPRITE_HEIGHT;
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

static UINT8 spawn_fish_from_left_side(void) {
    UINT8 player_midpoint_x = (UINT8)((LEVEL_MIN_X + LEVEL_MAX_X) >> 1);

    return FROM_FIXED(player.x) >= player_midpoint_x;
}

static UINT8 choose_fish_spawn_y(const LevelFishTypeDefinition *fish_type, const ActiveFishState *fish) {
    UINT8 minimum_y = LEVEL_MIN_Y;
    UINT8 maximum_y = (UINT8)(PLAYFIELD_VISIBLE_BOTTOM - fish->height + 17u);
    UINT8 preferred_y = clamp_uint8(fish_type->spawn_y, minimum_y, maximum_y);
    UINT8 mirrored_y = (UINT8)(minimum_y + (maximum_y - preferred_y));
    UINT8 player_y = FROM_FIXED(player.y);

    if (absolute_difference_uint8(player_y, mirrored_y) > absolute_difference_uint8(player_y, preferred_y)) {
        return mirrored_y;
    }

    return preferred_y;
}

static void spawn_fish_into_slot(UINT8 slot_index) {
    const LevelFishTypeDefinition *fish_type;
    ActiveFishState *fish;
    UINT8 spawn_from_left;

    if (current_level->fish_type_count == 0u) {
        return;
    }

    fish_type = &current_level->fish_types[next_fish_type_index];
    next_fish_type_index = (UINT8)((next_fish_type_index + 1u) % current_level->fish_type_count);

    fish = &active_fish[slot_index];
    fish->active = 1u;
    fish->kind = fish_type->kind;
    fish->minimum_x = fish_type->minimum_x;
    fish->maximum_x = fish_type->maximum_x;
    fish->lifetime_remaining = fish_type->lifetime_frames;
    fish->animation_frames = fish_type->animation_frames;
    configure_active_fish_from_kind(fish);
    fish->y = choose_fish_spawn_y(fish_type, fish);
    spawn_from_left = spawn_fish_from_left_side();
    fish->velocity_x = spawn_from_left ? fish_type->speed_fixed : (INT16)(-(INT16)fish_type->speed_fixed);
    fish->x = TO_FIXED(spawn_from_left ? fish_type->minimum_x : fish_type->maximum_x);
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
    current_failure_reason = LEVEL_FAILURE_AIR;

    for (slot_index = 0u; slot_index != MAX_CAPTURED_FISH; ++slot_index) {
        captured_fish_kinds[slot_index] = LEVEL_FISH_KIND_CLOWN;
    }

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        active_fish[slot_index].active = 0u;
        display_hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);
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
    display_hide_all_sprites();
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
            display_hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);
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
            display_hide_sprite_range(AIR_PICKUP_SPRITE_FIRST, 1u);
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
    UINT8 fish_attributes = (fish->velocity_x < 0) ? S_FLIPX : 0u;
    UINT8 frame = current_fish_frame(slot_index, fish);

    if (fish->kind == LEVEL_FISH_KIND_CLOWN) {
        fish_clown_sprite_show(fish->tile_first, sprite_index, frame, x, y);
    } else {
        fish_puffer_sprite_show(fish->tile_first, sprite_index, frame, x, y);
    }

    if (fish->sprite_count < FISH_SLOT_SPRITE_STRIDE) {
        display_hide_sprite_range((UINT8)(sprite_index + fish->sprite_count), (UINT8)(FISH_SLOT_SPRITE_STRIDE - fish->sprite_count));
    }

    display_set_sprite_prop_range(sprite_index, fish->sprite_count, fish_attributes);
}

static void render_level(void) {
    UINT8 diver_render_x;
    UINT8 diver_render_y;
    UINT8 player_frame = current_player_frame();
    UINT8 slot_index;
    UINT8 fish_x;
    UINT8 fish_y;

    compute_diver_render_position(&diver_render_x, &diver_render_y);
    draw_diver_sprite(diver_render_x, diver_render_y, player_frame, player.facing_left);

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        if (!active_fish[slot_index].active) {
            display_hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);
            continue;
        }

        compute_fish_render_position(slot_index, &fish_x, &fish_y);
        render_fish(slot_index, &active_fish[slot_index], fish_x, fish_y);
    }

    if (air_pickup.active) {
        set_sprite_tile(AIR_PICKUP_SPRITE_FIRST, AIR_PICKUP_TILE_FIRST);
        move_sprite(AIR_PICKUP_SPRITE_FIRST, FROM_FIXED(air_pickup.x), FROM_FIXED(air_pickup.y));
        set_sprite_prop(AIR_PICKUP_SPRITE_FIRST, 0u);
    } else {
        display_hide_sprite_range(AIR_PICKUP_SPRITE_FIRST, 1u);
    }
}

static void capture_fish(UINT8 slot_index) {
    active_fish[slot_index].active = 0u;
    display_hide_sprite_range(fish_slot_sprite_index(slot_index), FISH_SLOT_SPRITE_STRIDE);

    if (captured_fish_count < MAX_CAPTURED_FISH) {
        captured_fish_kinds[captured_fish_count] = active_fish[slot_index].kind;
        ++captured_fish_count;
    }
}

static void collect_air_pickup(void) {
    air_pickup.active = 0u;
    air_pickup.spawn_timer = current_level->air.spawn_frames;
    display_hide_sprite_range(AIR_PICKUP_SPRITE_FIRST, 1u);
    player_air = (UINT8)(player_air + current_level->air.air_gain);
    if (player_air > PLAYER_MAX_AIR) {
        player_air = PLAYER_MAX_AIR;
    }
}

static void check_level_collisions(void) {
    UINT8 player_render_x;
    UINT8 player_render_y;
    UINT8 player_frame = current_player_frame();
    UINT8 player_left;
    UINT8 player_top;
    UINT8 player_width;
    UINT8 player_height;
    UINT8 slot_index;
    UINT8 fish_left;
    UINT8 fish_top;
    UINT8 fish_width;
    UINT8 fish_height;
    UINT8 fish_frame;
    UINT8 bubble_left;
    UINT8 bubble_top;

    compute_diver_render_position(&player_render_x, &player_render_y);
    resolve_frame_hitbox(
        player_render_x,
        player_render_y,
        DIVER_SWIMMING_SPRITE_WIDTH,
        diver_swimming_sprite_frame_hitboxes,
        player_frame,
        player.facing_left,
        &player_left,
        &player_top,
        &player_width,
        &player_height
    );

    for (slot_index = 0u; slot_index != LEVELS_MAX_FISH_ON_SCREEN; ++slot_index) {
        if (!active_fish[slot_index].active) {
            continue;
        }

        compute_fish_render_position(slot_index, &fish_left, &fish_top);
        fish_frame = current_fish_frame(slot_index, &active_fish[slot_index]);
        resolve_frame_hitbox(
            fish_left,
            fish_top,
            active_fish[slot_index].width,
            fish_hitboxes_for_kind(active_fish[slot_index].kind),
            fish_frame,
            active_fish[slot_index].velocity_x < 0,
            &fish_left,
            &fish_top,
            &fish_width,
            &fish_height
        );

        if (boxes_overlap(
            player_left,
            player_top,
            player_width,
            player_height,
            fish_left,
            fish_top,
            fish_width,
            fish_height
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
        player_width,
        player_height,
        bubble_left,
        bubble_top,
        8u,
        8u
    )) {
        collect_air_pickup();
    }
}

void level_scene_begin(UINT8 level_index) {
    initialize_level(level_index);
    draw_level_screen();
    render_level();
    update_level_hud();
}

LevelUpdateResult level_scene_update(UINT8 current_keys, UINT8 pressed_keys) {
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
        return LEVEL_WON;
    }

    if (current_level->air.enabled && (player_air == 0u)) {
        current_failure_reason = LEVEL_FAILURE_AIR;
        return LEVEL_LOST;
    }

    if (player_health == 0u) {
        current_failure_reason = LEVEL_FAILURE_HEALTH;
        return LEVEL_LOST;
    }

    return LEVEL_RUNNING;
}

LevelFailureReason level_scene_failure_reason(void) {
    return current_failure_reason;
}
