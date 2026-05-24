/**
 * @file obstacle.c
 * @brief 障礙物/豬的欄位初始化與狀態更新（不含關卡載入）
 */

#include "obstacle.h"

#include <stdlib.h>
#include <string.h>

void obstacle_init(
    Obstacle *out,
    uint32_t id,
    EntityKind kind,
    ObstacleMaterial material,
    float x,
    float y,
    float width,
    float height,
    int hit_points,
    bool is_static)
{
    if (out == NULL) {
        return;
    }

    out->id = id;
    out->kind = kind;
    out->material = material;
    out->position.x = x;
    out->position.y = y;
    out->velocity.x = 0.0f;
    out->velocity.y = 0.0f;
    out->width = width;
    out->height = height;
    out->hit_points = hit_points;
    out->max_hit_points = hit_points;
    out->life_state = LIFE_STATE_ACTIVE;
    out->is_alive = true;
    out->is_static = is_static;
}

Obstacle *obstacle_create(
    uint32_t id,
    EntityKind kind,
    ObstacleMaterial material,
    float x,
    float y,
    float width,
    float height,
    int hit_points,
    bool is_static)
{
    Obstacle *obstacle = (Obstacle *)malloc(sizeof(Obstacle));
    if (obstacle == NULL) {
        return NULL;
    }

    obstacle_init(
        obstacle, id, kind, material, x, y, width, height, hit_points, is_static);
    return obstacle;
}

void obstacle_destroy(Obstacle *obstacle)
{
    free(obstacle);
}

bool obstacle_apply_damage(Obstacle *obstacle, int damage)
{
    if (obstacle == NULL || damage <= 0) {
        return false;
    }

    obstacle->hit_points -= damage;
    if (obstacle->hit_points <= 0) {
        obstacle->hit_points = 0;
        obstacle_mark_destroyed(obstacle);
        return true;
    }

    return false;
}

void obstacle_mark_destroyed(Obstacle *obstacle)
{
    if (obstacle == NULL) {
        return;
    }

    obstacle->is_alive = false;
    obstacle->life_state = LIFE_STATE_DESTROYED;
}

void obstacle_copy(Obstacle *dest, const Obstacle *src)
{
    if (dest == NULL || src == NULL) {
        return;
    }

    memcpy(dest, src, sizeof(Obstacle));
}
