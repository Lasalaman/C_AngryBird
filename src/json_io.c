/**
 * @file json_io.c
 * @brief 精簡 JSON 解析與軌跡結果輸出
 */

#include "json_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_key(const char *json, const char *key)
{
    if (json == NULL || key == NULL) {
        return NULL;
    }

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (pos == NULL) {
        return NULL;
    }

    return strchr(pos, ':');
}

static bool parse_double_after_colon(const char *colon, double *out)
{
    if (colon == NULL || out == NULL) {
        return false;
    }

    char *end = NULL;
    *out = strtod(colon + 1, &end);
    return end != colon + 1;
}

static bool json_read_double(const char *json, const char *key, double *out)
{
    const char *colon = find_key(json, key);
    if (colon == NULL) {
        return false;
    }

    return parse_double_after_colon(colon, out);
}

static bool parse_obstacle_object(const char *obj, ObstacleDef *def)
{
    if (obj == NULL || def == NULL) {
        return false;
    }

    double value = 0.0;

    if (!json_read_double(obj, "id", &value)) {
        return false;
    }
    def->id = (uint32_t)value;

    if (!json_read_double(obj, "kind", &value)) {
        def->kind = ENTITY_KIND_OBSTACLE;
    } else {
        def->kind = (EntityKind)(int)value;
    }

    if (!json_read_double(obj, "material", &value)) {
        def->material = MATERIAL_NONE;
    } else {
        def->material = (ObstacleMaterial)(int)value;
    }

    if (!json_read_double(obj, "x", &value)) {
        return false;
    }
    def->x = (float)value;

    if (!json_read_double(obj, "y", &value)) {
        return false;
    }
    def->y = (float)value;

    if (!json_read_double(obj, "width", &value)) {
        return false;
    }
    def->width = (float)value;

    if (!json_read_double(obj, "height", &value)) {
        return false;
    }
    def->height = (float)value;

    if (!json_read_double(obj, "hit_points", &value)) {
        def->hit_points = 100;
    } else {
        def->hit_points = (int)value;
    }

    def->is_static = true;
    return true;
}

static size_t parse_obstacles_array(const char *json, ObstacleDef *defs, size_t max_defs)
{
    const char *arr = strstr(json, "\"obstacles\"");
    if (arr == NULL || defs == NULL || max_defs == 0) {
        return 0;
    }

    arr = strchr(arr, '[');
    if (arr == NULL) {
        return 0;
    }

    size_t count = 0;
    const char *cursor = arr + 1;

    while (count < max_defs) {
        const char *obj_start = strchr(cursor, '{');
        if (obj_start == NULL) {
            break;
        }

        const char *obj_end = strchr(obj_start, '}');
        if (obj_end == NULL) {
            break;
        }

        char buffer[512];
        const size_t len = (size_t)(obj_end - obj_start + 1);
        if (len >= sizeof(buffer)) {
            break;
        }

        memcpy(buffer, obj_start, len);
        buffer[len] = '\0';

        if (parse_obstacle_object(buffer, &defs[count])) {
            count++;
        }

        cursor = obj_end + 1;
        if (*cursor == ']') {
            break;
        }
    }

    return count;
}

bool json_parse_request(const char *json, SimRequest *out)
{
    if (json == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    double value = 0.0;

    if (!json_read_double(json, "angle", &value)) {
        return false;
    }
    out->launch.angle_deg = (float)value;

    if (!json_read_double(json, "velocity", &value)) {
        return false;
    }
    out->launch.velocity = (float)value;

    if (!json_read_double(json, "start_x", &value)) {
        out->launch.start_x = 120.0;
    } else {
        out->launch.start_x = (float)value;
    }

    if (!json_read_double(json, "start_y", &value)) {
        out->launch.start_y = 380.0;
    } else {
        out->launch.start_y = (float)value;
    }

    if (!json_read_double(json, "bird_radius", &value)) {
        out->launch.bird_radius = 16.0f;
    } else {
        out->launch.bird_radius = (float)value;
    }

    if (!json_read_double(json, "gravity", &value)) {
        out->launch.gravity_y = 980.0f;
    } else {
        out->launch.gravity_y = (float)value;
    }

    if (!json_read_double(json, "time_step", &value)) {
        out->launch.time_step = 1.0f / 60.0f;
    } else {
        out->launch.time_step = (float)value;
    }

    if (!json_read_double(json, "max_steps", &value)) {
        out->launch.max_steps = 600;
    } else {
        out->launch.max_steps = (int)value;
    }

    if (!json_read_double(json, "min_x", &value)) {
        out->launch.bounds.min_x = 0.0f;
    } else {
        out->launch.bounds.min_x = (float)value;
    }

    if (!json_read_double(json, "min_y", &value)) {
        out->launch.bounds.min_y = 0.0f;
    } else {
        out->launch.bounds.min_y = (float)value;
    }

    if (!json_read_double(json, "max_x", &value)) {
        out->launch.bounds.max_x = 1280.0f;
    } else {
        out->launch.bounds.max_x = (float)value;
    }

    if (!json_read_double(json, "max_y", &value)) {
        out->launch.bounds.max_y = 720.0f;
    } else {
        out->launch.bounds.max_y = (float)value;
    }

    out->obstacle_count =
        parse_obstacles_array(json, out->obstacle_defs, JSON_MAX_OBSTACLES);

    if (out->launch.max_steps <= 0 || out->launch.max_steps > PHYSICS_MAX_STEPS) {
        return false;
    }

    return true;
}

void json_print_response(const TrajectoryResult *result)
{
    if (result == NULL) {
        printf("{\"error\":\"null_result\"}\n");
        return;
    }

    printf("{\"trajectory\":[");

    for (size_t i = 0; i < result->count; i++) {
        if (i > 0) {
            printf(",");
        }

        printf(
            "{\"t\":%.4f,\"x\":%.2f,\"y\":%.2f}",
            result->points[i].t,
            result->points[i].x,
            result->points[i].y);
    }

    printf(
        "],\"hit\":%s,\"hit_id\":%u,\"out_of_bounds\":%s}\n",
        result->hit ? "true" : "false",
        result->hit_id,
        result->out_of_bounds ? "true" : "false");
}
