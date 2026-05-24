/**
 * @file level_defaults.h
 * @brief 關卡 1 預設佈局（與 app.py DEFAULT_LEVEL 一致）
 */

#ifndef ANGRYBIRD_LEVEL_DEFAULTS_H
#define ANGRYBIRD_LEVEL_DEFAULTS_H

#include "level.h"

/** 每關待發射小鳥佇列容量（malloc bird_queue 時使用） */
#define LEVEL_BIRD_QUEUE_CAPACITY 3

size_t level_default_obstacle_count(void);
const ObstacleDef *level_default_obstacles(void);

size_t level_default_bird_count(void);
const BirdDef *level_default_birds(void);

#endif /* ANGRYBIRD_LEVEL_DEFAULTS_H */
