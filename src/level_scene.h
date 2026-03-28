#ifndef LEVEL_SCENE_H
#define LEVEL_SCENE_H

#include <gb/gb.h>

#include "game_types.h"

void level_scene_begin(UINT8 level_index);
LevelUpdateResult level_scene_update(UINT8 current_keys, UINT8 pressed_keys);
LevelFailureReason level_scene_failure_reason(void);

#endif
