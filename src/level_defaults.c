/**
 * @file level_defaults.c
 * @brief 關卡 1 預設障礙物與 3 隻小鳥佇列初始資料
 */

#include "level_defaults.h"

static const ObstacleDef DEFAULT_OBSTACLES[] = {
    {1, ENTITY_KIND_PIG, MATERIAL_NONE, 980, 580, 52, 52, 100, true, true},
    {2, ENTITY_KIND_OBSTACLE, MATERIAL_WOOD, 860, 630, 70, 28, 40, true, true},
    {3, ENTITY_KIND_OBSTACLE, MATERIAL_WOOD, 930, 600, 70, 28, 40, true, true},
    {4, ENTITY_KIND_OBSTACLE, MATERIAL_WOOD, 900, 565, 32, 70, 40, true, true},
};

static const BirdDef DEFAULT_BIRDS[] = {
    {101, BIRD_SPECIES_RED, 120.0f, 348.0f},
    {102, BIRD_SPECIES_RED, 120.0f, 348.0f},
    {103, BIRD_SPECIES_RED, 120.0f, 348.0f},
};

size_t level_default_obstacle_count(void)
{
    return sizeof(DEFAULT_OBSTACLES) / sizeof(DEFAULT_OBSTACLES[0]);
}

const ObstacleDef *level_default_obstacles(void)
{
    return DEFAULT_OBSTACLES;
}

size_t level_default_bird_count(void)
{
    return LEVEL_BIRD_QUEUE_CAPACITY;
}

const BirdDef *level_default_birds(void)
{
    return DEFAULT_BIRDS;
}
