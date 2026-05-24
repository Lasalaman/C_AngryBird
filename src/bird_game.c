/**
 * @file bird_game.c
 * @brief 關卡狀態持久化：game_state.json
 *
 * 記憶體與指標安全（連續 launch 不洩漏）：
 * - 每個 subprocess 內 Level* 仍由 init_level malloc、free_level 釋放。
 * - 可變狀態在 free_level 前複製到 GameSession（值陣列），再寫入 JSON 檔。
 * - 下一個 subprocess 從檔案重建 Level，不保留上一行程指標，故無懸空指標。
 * - RESET 覆寫整個檔案，等同重新配置乾淨關卡。
 */

#include "bird_game.h"

#include "bird_queue.h"
#include "json_io.h"
#include "level_defaults.h"
#include "obstacle.h"

#include <stdio.h>
#include <string.h>

void game_session_reset_defaults(GameSession *session)
{
    if (session == NULL) {
        return;
    }

    memset(session, 0, sizeof(*session));
    session->level_id = 1;
    session->bounds.min_x = 0.0f;
    session->bounds.min_y = 0.0f;
    session->bounds.max_x = 1280.0f;
    session->bounds.max_y = 720.0f;
    session->gravity_y = 980.0f;
    session->time_step = 1.0f / 60.0f;

    session->obstacle_count = level_default_obstacle_count();
    memcpy(
        session->persistent_obstacles,
        level_default_obstacles(),
        session->obstacle_count * sizeof(ObstacleDef));

    session->bird_count = level_default_bird_count();
    memcpy(
        session->bird_queue,
        level_default_birds(),
        session->bird_count * sizeof(BirdDef));
}

bool game_state_load(GameSession *session)
{
    if (session == NULL) {
        return false;
    }

    FILE *fp = fopen(GAME_STATE_FILENAME, "r");
    if (fp == NULL) {
        return false;
    }

    char buffer[JSON_INPUT_MAX];
    const size_t n = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);

    if (n == 0) {
        return false;
    }

    buffer[n] = '\0';

    SimRequest req;
    memset(&req, 0, sizeof(req));
    if (!json_parse_request(buffer, &req)) {
        return false;
    }

    game_session_reset_defaults(session);

    session->gravity_y = req.launch.gravity_y;
    session->time_step = req.launch.time_step;
    session->bounds = req.launch.bounds;

    if (req.obstacle_count > 0) {
        session->obstacle_count = req.obstacle_count;
        memcpy(
            session->persistent_obstacles,
            req.obstacle_defs,
            req.obstacle_count * sizeof(ObstacleDef));
    }

    if (req.bird_count > 0) {
        session->bird_count = req.bird_count;
        memcpy(session->bird_queue, req.bird_defs, req.bird_count * sizeof(BirdDef));
    }

    return true;
}

bool game_state_save(const GameSession *session)
{
    if (session == NULL) {
        return false;
    }

    Level *level = game_session_build_level(session);
    if (level == NULL) {
        return false;
    }

    FILE *fp = fopen(GAME_STATE_FILENAME, "w");
    if (fp == NULL) {
        free_level(level);
        return false;
    }

    const int remaining_birds =
        level->launch_queue != NULL ? (int)bird_queue_size(level->launch_queue) : 0;
    const int remaining_pigs = level_count_alive_pigs(level);

    fprintf(
        fp,
        "{\"action\":\"reset\",\"angle\":0,\"velocity\":0,"
        "\"remaining_birds\":%d,\"remaining_pigs\":%d,"
        "\"total_birds\":%d,\"gravity\":%.1f,\"time_step\":%.6f,"
        "\"min_x\":%.1f,\"min_y\":%.1f,\"max_x\":%.1f,\"max_y\":%.1f",
        remaining_birds,
        remaining_pigs,
        LEVEL_BIRD_QUEUE_CAPACITY,
        session->gravity_y,
        session->time_step,
        session->bounds.min_x,
        session->bounds.min_y,
        session->bounds.max_x,
        session->bounds.max_y);

    fprintf(fp, ",\"obstacles\":[");
    for (size_t i = 0; i < level->obstacle_count; i++) {
        const Obstacle *obs = &level->obstacles[i];
        if (i > 0) {
            fprintf(fp, ",");
        }
        fprintf(
            fp,
            "{\"id\":%u,\"kind\":%d,\"material\":%d,\"x\":%.1f,\"y\":%.1f,"
            "\"width\":%.1f,\"height\":%.1f,\"hit_points\":%d,\"is_alive\":%s}",
            obs->id,
            (int)obs->kind,
            (int)obs->material,
            obs->position.x,
            obs->position.y,
            obs->width,
            obs->height,
            obs->hit_points,
            obs->is_alive ? "true" : "false");
    }
    fprintf(fp, "],\"birds\":[");
    if (level->launch_queue != NULL) {
        const BirdQueue *q = level->launch_queue;
        for (size_t i = 0; i < q->count; i++) {
            const size_t idx = (q->head + i) % q->capacity;
            const Bird *bird = &q->buffer[idx];
            if (i > 0) {
                fprintf(fp, ",");
            }
            fprintf(
                fp,
                "{\"id\":%u,\"species\":%d,\"spawn_x\":%.1f,\"spawn_y\":%.1f}",
                bird->id,
                (int)bird->species,
                bird->position.x,
                bird->position.y);
        }
    }
    fprintf(fp, "]}\n");

    fclose(fp);
    free_level(level);
    return true;
}

bool game_state_reset_file(const GameSession *session)
{
    remove(GAME_STATE_FILENAME);
    return game_state_save(session);
}

void game_session_sync_from_level(const Level *level, GameSession *session)
{
    if (level == NULL || session == NULL) {
        return;
    }

    session->bounds = level->state.play_bounds;
    session->gravity_y = level->state.gravity_y;
    session->time_step = level->state.time_step;
    session->obstacle_count = level->obstacle_count;

    for (size_t i = 0; i < level->obstacle_count; i++) {
        const Obstacle *obs = &level->obstacles[i];
        ObstacleDef *def = &session->persistent_obstacles[i];

        def->id = obs->id;
        def->kind = obs->kind;
        def->material = obs->material;
        def->x = obs->position.x;
        def->y = obs->position.y;
        def->width = obs->width;
        def->height = obs->height;
        def->hit_points = obs->hit_points;
        def->is_static = obs->is_static;
        def->is_alive = obs->is_alive;
    }

    session->bird_count = 0;
    if (level->launch_queue != NULL) {
        const BirdQueue *q = level->launch_queue;
        for (size_t i = 0; i < q->count && session->bird_count < LEVEL_BIRD_QUEUE_CAPACITY;
             i++) {
            const size_t idx = (q->head + i) % q->capacity;
            const Bird *bird = &q->buffer[idx];
            BirdDef *def = &session->bird_queue[session->bird_count];

            def->id = bird->id;
            def->species = bird->species;
            def->spawn_x = bird->position.x;
            def->spawn_y = bird->position.y;
            session->bird_count++;
        }
    }
}

Level *game_session_build_level(const GameSession *session)
{
    if (session == NULL) {
        return NULL;
    }

    return init_level(
        session->level_id,
        session->bounds,
        session->gravity_y,
        session->time_step,
        session->persistent_obstacles,
        session->obstacle_count,
        session->bird_queue,
        session->bird_count);
}
