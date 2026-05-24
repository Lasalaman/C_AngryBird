/**
 * @file level.c
 * @brief 關卡初始化（init_level）與完整記憶體釋放（free_level）
 */

#include "level.h"

#include "bird.h"

#include <stdlib.h>

/* 前向宣告：失敗時回滾已配置資源 */
static void free_level_contents(Level *level);

static int count_pigs_in_defs(const ObstacleDef *defs, size_t count)
{
    int pigs = 0;

    if (defs == NULL) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        if (defs[i].kind == ENTITY_KIND_PIG) {
            pigs++;
        }
    }

    return pigs;
}

void level_init(
    LevelState *out,
    uint32_t level_id,
    Bounds bounds,
    float gravity_y,
    float time_step,
    int initial_birds,
    int initial_pigs)
{
    if (out == NULL) {
        return;
    }

    out->level_id = level_id;
    out->play_bounds = bounds;
    out->gravity_y = gravity_y;
    out->time_step = time_step;
    out->birds_remaining = initial_birds;
    out->pigs_remaining = initial_pigs;
    out->is_paused = false;
    out->is_level_complete = false;
    out->is_level_failed = false;
}

LevelState *level_create(
    uint32_t level_id,
    Bounds bounds,
    float gravity_y,
    float time_step,
    int initial_birds,
    int initial_pigs)
{
    LevelState *state = (LevelState *)malloc(sizeof(LevelState));
    if (state == NULL) {
        return NULL;
    }

    level_init(
        state, level_id, bounds, gravity_y, time_step, initial_birds, initial_pigs);
    return state;
}

void level_destroy(LevelState *level)
{
    free(level);
}

bool level_contains_point(const LevelState *level, float x, float y)
{
    if (level == NULL) {
        return false;
    }

    const Bounds *b = &level->play_bounds;
    return x >= b->min_x && x <= b->max_x && y >= b->min_y && y <= b->max_y;
}

void level_mark_complete(LevelState *level)
{
    if (level == NULL) {
        return;
    }

    level->is_level_complete = true;
}

void level_mark_failed(LevelState *level)
{
    if (level == NULL) {
        return;
    }

    level->is_level_failed = true;
}

int level_count_alive_pigs(const Level *level)
{
    int count = 0;

    if (level == NULL || level->obstacles == NULL) {
        return 0;
    }

    for (size_t i = 0; i < level->obstacle_count; i++) {
        const Obstacle *obs = &level->obstacles[i];
        if (obs->kind == ENTITY_KIND_PIG && obs->is_alive) {
            count++;
        }
    }

    return count;
}

bool level_consume_bird_from_queue(Level *level, Bird *out_bird)
{
    if (level == NULL || out_bird == NULL || level->launch_queue == NULL) {
        return false;
    }

    if (!bird_queue_dequeue(level->launch_queue, out_bird)) {
        return false;
    }

    level->state.birds_remaining = (int)bird_queue_size(level->launch_queue);
    return true;
}

/**
 * @brief 釋放 Level 內各子資源（不 free Level 外殼本身）
 */
static void free_level_contents(Level *level)
{
    if (level == NULL) {
        return;
    }

    /*
     * 釋放障礙物連續陣列。
     * obstacles 在 init_level 中以 malloc(obstacle_count * sizeof(Obstacle)) 取得；
     * 單一 free 即可歸還整塊，無需對每個元素個別 free（元素為值型別，無內嵌指標）。
     */
    free(level->obstacles);
    level->obstacles = NULL;
    level->obstacle_count = 0;

    bird_queue_destroy(level->launch_queue);
    level->launch_queue = NULL;
}

void free_level(Level *level)
{
    if (level == NULL) {
        return;
    }

    free_level_contents(level);

    /*
     * 最後釋放 Level 控制結構本身（init_level 第一個 malloc(sizeof(Level))）。
     */
    free(level);
}

Level *init_level(
    uint32_t level_id,
    Bounds bounds,
    float gravity_y,
    float time_step,
    const ObstacleDef *obstacle_defs,
    size_t obstacle_count,
    const BirdDef *bird_defs,
    size_t bird_count)
{
    if (obstacle_count > 0 && obstacle_defs == NULL) {
        return NULL;
    }
    if (bird_count > 0 && bird_defs == NULL) {
        return NULL;
    }

    /*
     * 步驟 1：配置 Level 外殼
     * 請求位元組數 = sizeof(Level)
     * 回傳指標 level 指向這塊記憶體的起始位址。
     */
    Level *level = (Level *)malloc(sizeof(Level));
    if (level == NULL) {
        return NULL;
    }

    level->obstacles = NULL;
    level->obstacle_count = 0;
    level->launch_queue = NULL;

    const int pig_count = count_pigs_in_defs(obstacle_defs, obstacle_count);
    level_init(
        &level->state,
        level_id,
        bounds,
        gravity_y,
        time_step,
        (int)bird_count,
        pig_count);

    /*
     * 步驟 2：依本關障礙物數量配置 Obstacle 連續陣列
     *
     * malloc 大小計算：
     *   obstacle_bytes = obstacle_count * sizeof(Obstacle)
     *
     * 例：obstacle_count = 5，sizeof(Obstacle) ≈ 64 bytes
     *     => 請求 5 * 64 = 320 bytes
     *
     * level->obstacles 指向這塊記憶體的第 0 個元素；
     * 第 i 個元素位址為 level->obstacles + i（或 &level->obstacles[i]）。
     */
    if (obstacle_count > 0) {
        const size_t obstacle_bytes = obstacle_count * sizeof(Obstacle);
        level->obstacles = (Obstacle *)malloc(obstacle_bytes);
        if (level->obstacles == NULL) {
            free_level(level);
            return NULL;
        }

        level->obstacle_count = obstacle_count;

        for (size_t i = 0; i < obstacle_count; i++) {
            const ObstacleDef *def = &obstacle_defs[i];
            /*
             * 就地寫入陣列槽位，不另 malloc 單一 Obstacle。
             * &level->obstacles[i] 與 (level->obstacles + i) 等價。
             */
            obstacle_init_from_def(&level->obstacles[i], def);
        }
    }

    /*
     * 步驟 3：建立待發射小鳥 FIFO 佇列
     *
     * bird_queue_create 內部會再 malloc：
     *   - sizeof(BirdQueue) 控制結構
     *   - bird_count * sizeof(Bird) 環形緩衝區
     *
     * level->launch_queue 指向 BirdQueue；其 buffer 成員再指向 Bird 陣列。
     */
    if (bird_count > 0) {
        level->launch_queue = bird_queue_create(bird_count);
        if (level->launch_queue == NULL) {
            free_level(level);
            return NULL;
        }

        for (size_t i = 0; i < bird_count; i++) {
            const BirdDef *def = &bird_defs[i];
            Bird bird;

            bird_init(&bird, def->id, def->species, def->spawn_x, def->spawn_y);
            if (!bird_queue_enqueue(level->launch_queue, &bird)) {
                free_level(level);
                return NULL;
            }
        }
    }

    return level;
}
