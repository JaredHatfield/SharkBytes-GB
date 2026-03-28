#ifndef LEVELS_GENERATED_H
#define LEVELS_GENERATED_H

#include <gb/gb.h>

#define LEVEL_COUNT 10u
#define PLAYER_MAX_HEALTH 3u
#define PLAYER_MAX_AIR 10u
#define MAX_CAPTURED_FISH 10u
#define LEVELS_MAX_FISH_ON_SCREEN 2u

typedef enum LevelFishKind {
    LEVEL_FISH_KIND_YELLOW = 0,
    LEVEL_FISH_KIND_STRIPED
} LevelFishKind;

typedef struct LevelFishTypeDefinition {
    UINT8 kind;
    UINT8 spawn_y;
    UINT8 minimum_x;
    UINT8 maximum_x;
    UINT8 speed_fixed;
    UINT16 lifetime_frames;
    UINT8 start_from_left;
} LevelFishTypeDefinition;

typedef struct LevelAirDefinition {
    UINT8 enabled;
    UINT8 air_gain;
    UINT16 loss_frames;
    UINT16 spawn_frames;
    UINT8 rise_speed_fixed;
} LevelAirDefinition;

typedef struct LevelPlayerDefinition {
    UINT16 animation_frames;
} LevelPlayerDefinition;

typedef struct LevelDefinition {
    const char *name;
    UINT8 implemented;
    UINT8 starting_hearts;
    UINT8 starting_air;
    UINT8 goal_fish;
    LevelPlayerDefinition player;
    UINT8 max_fish_on_screen;
    UINT8 fish_type_count;
    const LevelFishTypeDefinition *fish_types;
    LevelAirDefinition air;
} LevelDefinition;

extern const LevelDefinition level_definitions[LEVEL_COUNT];

#endif
