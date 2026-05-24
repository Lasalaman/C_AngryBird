/**
 * @file physics.c
 * @brief 拋體運動、邊界檢查與圓形對 AABB 碰撞
 */

#include "physics.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float deg_to_rad(float deg)
{
    return deg * (float)M_PI / 180.0f;
}

void physics_position_at_time(
    float x0,
    float y0,
    float angle_deg,
    float velocity,
    float gravity_y,
    float t,
    float *out_x,
    float *out_y)
{
    if (out_x == NULL || out_y == NULL) {
        return;
    }

    const float theta = deg_to_rad(angle_deg);
    const float vx = velocity * cosf(theta);
    /* 螢幕座標 y 向下：向上發射時 vy 分量為負 */
    const float vy = -velocity * sinf(theta);

    *out_x = x0 + vx * t;
    *out_y = y0 + vy * t + 0.5f * gravity_y * t * t;
}

bool physics_is_inside_bounds(const Bounds *bounds, float x, float y)
{
    if (bounds == NULL) {
        return false;
    }

    return x >= bounds->min_x && x <= bounds->max_x && y >= bounds->min_y &&
           y <= bounds->max_y;
}

void physics_clamp_to_bounds(const Bounds *bounds, float *x, float *y)
{
    if (bounds == NULL || x == NULL || y == NULL) {
        return;
    }

    if (*x < bounds->min_x) {
        *x = bounds->min_x;
    } else if (*x > bounds->max_x) {
        *x = bounds->max_x;
    }

    if (*y < bounds->min_y) {
        *y = bounds->min_y;
    } else if (*y > bounds->max_y) {
        *y = bounds->max_y;
    }
}

/**
 * @brief 圓心 (cx,cy) 半徑 r 與左上角 (ox,oy) 寬高矩形是否相交
 */
static bool circle_hits_rect(
    float cx,
    float cy,
    float r,
    float ox,
    float oy,
    float w,
    float h)
{
    const float closest_x = fmaxf(ox, fminf(cx, ox + w));
    const float closest_y = fmaxf(oy, fminf(cy, oy + h));
    const float dx = cx - closest_x;
    const float dy = cy - closest_y;

    return (dx * dx + dy * dy) <= (r * r);
}

static bool check_collision(
    float x,
    float y,
    float radius,
    const Obstacle *obstacles,
    size_t obstacle_count,
    uint32_t *hit_id)
{
    if (obstacles == NULL || hit_id == NULL) {
        return false;
    }

    /*
     * 迴圈上界嚴格使用 obstacle_count，僅存取 obstacles[0..count-1]，
     * 避免陣列越界（Array Out-of-Bounds）。
     */
    for (size_t i = 0; i < obstacle_count; i++) {
        const Obstacle *obs = &obstacles[i];

        if (!obs->is_alive) {
            continue;
        }

        if (circle_hits_rect(
                x,
                y,
                radius,
                obs->position.x,
                obs->position.y,
                obs->width,
                obs->height)) {
            *hit_id = obs->id;
            return true;
        }
    }

    return false;
}

void trajectory_result_free(TrajectoryResult *result)
{
    if (result == NULL) {
        return;
    }

    free(result->points);
    result->points = NULL;
    result->count = 0;
    result->capacity = 0;
}

bool physics_simulate_trajectory(
    const LaunchParams *params,
    const Obstacle *obstacles,
    size_t obstacle_count,
    TrajectoryResult *out)
{
    if (params == NULL || out == NULL) {
        return false;
    }

    if (params->max_steps <= 0 || params->max_steps > PHYSICS_MAX_STEPS) {
        return false;
    }

    if (params->time_step <= 0.0f || params->velocity < 0.0f) {
        return false;
    }

    trajectory_result_free(out);

    const size_t capacity = (size_t)params->max_steps + 1u;
    const size_t bytes = capacity * sizeof(TrajectoryPoint);

    out->points = (TrajectoryPoint *)malloc(bytes);
    if (out->points == NULL) {
        return false;
    }

    out->capacity = capacity;
    out->count = 0;
    out->hit = false;
    out->hit_id = 0;
    out->out_of_bounds = false;

    for (int step = 0; step <= params->max_steps; step++) {
        const float t = (float)step * params->time_step;
        float x = 0.0f;
        float y = 0.0f;

        physics_position_at_time(
            params->start_x,
            params->start_y,
            params->angle_deg,
            params->velocity,
            params->gravity_y,
            t,
            &x,
            &y);

        if (!physics_is_inside_bounds(&params->bounds, x, y)) {
            out->out_of_bounds = true;
            break;
        }

        physics_clamp_to_bounds(&params->bounds, &x, &y);

        uint32_t hit_id = 0;
        if (check_collision(
                x,
                y,
                params->bird_radius,
                obstacles,
                obstacle_count,
                &hit_id)) {
            out->hit = true;
            out->hit_id = hit_id;

            if (out->count < out->capacity) {
                out->points[out->count].t = t;
                out->points[out->count].x = x;
                out->points[out->count].y = y;
                out->count++;
            }
            break;
        }

        if (out->count >= out->capacity) {
            break;
        }

        out->points[out->count].t = t;
        out->points[out->count].x = x;
        out->points[out->count].y = y;
        out->count++;
    }

    return true;
}
