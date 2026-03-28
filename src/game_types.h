#ifndef GAME_TYPES_H
#define GAME_TYPES_H

typedef enum LevelFailureReason {
    LEVEL_FAILURE_AIR = 0,
    LEVEL_FAILURE_HEALTH
} LevelFailureReason;

typedef enum LevelUpdateResult {
    LEVEL_RUNNING = 0,
    LEVEL_WON,
    LEVEL_LOST
} LevelUpdateResult;

#endif
