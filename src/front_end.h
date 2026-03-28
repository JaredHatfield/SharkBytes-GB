#ifndef FRONT_END_H
#define FRONT_END_H

#include <gb/gb.h>

#include "game_types.h"

void front_end_draw_title_screen(void);
void front_end_draw_level_select_screen(UINT8 selected_level, const UINT8 *level_unlocked);
void front_end_move_level_select_cursor(UINT8 selected_level);
void front_end_draw_level_placeholder_screen(UINT8 level_index);
void front_end_draw_level_result_message(UINT8 won, LevelFailureReason reason);

#endif
