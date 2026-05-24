/**
 * @file json_io.c
 * @brief 精簡 JSON 解析與軌跡／關卡狀態輸出
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

static SimAction parse_action(const char *json)
{
    const char *colon = find_key(json, "action");
    if (colon == NULL) {
        return SIM_ACTION_LAUNCH;
    }

    const char *value = colon + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }

    if (strstr(value, "\"reset\"") != NULL) {
        return SIM_ACTION_RESET;
    }

    return SIM_ACTION_LAUNCH;
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
    def->is_alive = true;

    if (strstr(obj, "\"is_alive\":false") != NULL) {
        def->is_alive = false;
    } else if (strstr(obj, "\"is_alive\":true") != NULL) {
        def->is_alive = true;
    }

    return true;
}

static size_t parse_object_array(
    const char *json,
    const char *array_key,
    bool (*parse_item)(const char *obj, void *out),
    void *out_base,
    size_t item_size,
    size_t max_items)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", array_key);
    const char *arr = strstr(json, pattern);
    if (arr == NULL || out_base == NULL || parse_item == NULL || max_items == 0) {
        return 0;
    }

    arr = strchr(arr, '[');
    if (arr == NULL) {
        return 0;
    }

    size_t count = 0;
    const char *cursor = arr + 1;

    while (count < max_items) {
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

        void *slot = (char *)out_base + (count * item_size);
        if (parse_item(buffer, slot)) {
            count++;
        }

        cursor = obj_end + 1;
        if (*cursor == ']') {
            break;
        }
    }

    return count;
}

static bool parse_bird_object(const char *obj, void *out)
{
    BirdDef *def = (BirdDef *)out;
    double value = 0.0;

    if (!json_read_double(obj, "id", &value)) {
        return false;
    }
    def->id = (uint32_t)value;

    if (!json_read_double(obj, "species", &value)) {
        def->species = BIRD_SPECIES_RED;
    } else {
        def->species = (BirdSpecies)(int)value;
    }

    if (!json_read_double(obj, "spawn_x", &value)) {
        def->spawn_x = 120.0f;
    } else {
        def->spawn_x = (float)value;
    }

    if (!json_read_double(obj, "x", &value)) {
        /* 相容簡寫 x/y */
    } else {
        def->spawn_x = (float)value;
    }

    if (!json_read_double(obj, "spawn_y", &value)) {
        def->spawn_y = 380.0f;
    } else {
        def->spawn_y = (float)value;
    }

    if (json_read_double(obj, "y", &value)) {
        def->spawn_y = (float)value;
    }

    return true;
}

static bool parse_obstacle_wrapper(const char *obj, void *out)
{
    return parse_obstacle_object(obj, (ObstacleDef *)out);
}

static void print_obstacles_json(const Level *level)
{
    printf(",\"obstacles\":[");
    if (level != NULL && level->obstacles != NULL) {
        for (size_t i = 0; i < level->obstacle_count; i++) {
            const Obstacle *obs = &level->obstacles[i];
            if (i > 0) {
                printf(",");
            }
            printf(
                "{\"id\":%u,\"kind\":%d,\"x\":%.1f,\"y\":%.1f,"
                "\"width\":%.1f,\"height\":%.1f,\"hit_points\":%d,"
                "\"is_alive\":%s}",
                obs->id,
                (int)obs->kind,
                obs->position.x,
                obs->position.y,
                obs->width,
                obs->height,
                obs->hit_points,
                obs->is_alive ? "true" : "false");
        }
    }
    printf("]");
}

static void print_birds_queue_json(const Level *level)
{
    printf(",\"birds\":[");
    if (level != NULL && level->launch_queue != NULL) {
        const BirdQueue *q = level->launch_queue;
        bool first = true;
        for (size_t i = 0; i < q->count; i++) {
            const size_t idx = (q->head + i) % q->capacity;
            const Bird *bird = &q->buffer[idx];
            if (!first) {
                printf(",");
            }
            printf(
                "{\"id\":%u,\"species\":%d,\"spawn_x\":%.1f,\"spawn_y\":%.1f}",
                bird->id,
                (int)bird->species,
                bird->position.x,
                bird->position.y);
            first = false;
        }
    }
    printf("]");
}

void json_print_reset_result(const Level *level)
{
    if (level == NULL) {
        printf("{\"error\":\"null_level\"}\n");
        return;
    }

    const int remaining_birds =
        level->launch_queue != NULL ? (int)bird_queue_size(level->launch_queue) : 0;
    const int remaining_pigs = level_count_alive_pigs(level);

    const char *status = "playing";
    if (remaining_pigs == 0) {
        status = "won";
    }

    printf(
        "{\"action\":\"reset\",\"game_status\":\"%s\",\"remaining_birds\":%d,"
        "\"remaining_pigs\":%d,\"total_birds\":%d",
        status,
        remaining_birds,
        remaining_pigs,
        LEVEL_BIRD_QUEUE_CAPACITY);

    print_obstacles_json(level);
    print_birds_queue_json(level);
    printf("}\n");
}

void json_print_launch_result(
    const TrajectoryResult *trajectory,
    const Level *level,
    uint32_t launched_bird_id)
{
    if (trajectory == NULL || level == NULL) {
        printf("{\"error\":\"null_result\"}\n");
        return;
    }

    const int remaining_birds =
        level->launch_queue != NULL ? (int)bird_queue_size(level->launch_queue) : 0;
    const int remaining_pigs = level_count_alive_pigs(level);

    printf("{\"trajectory\":[");

    for (size_t i = 0; i < trajectory->count; i++) {
        if (i > 0) {
            printf(",");
        }
        printf(
            "{\"t\":%.4f,\"x\":%.2f,\"y\":%.2f}",
            trajectory->points[i].t,
            trajectory->points[i].x,
            trajectory->points[i].y);
    }

    const char *status = "playing";
    if (remaining_pigs == 0) {
        status = "won";
    } else if (remaining_birds == 0) {
        status = "lost";
    }

    printf(
        "],\"hit\":%s,\"hit_id\":%u,\"out_of_bounds\":%s,"
        "\"launched_bird_id\":%u,\"remaining_birds\":%d,\"remaining_pigs\":%d,"
        "\"game_status\":\"%s\"",
        trajectory->hit ? "true" : "false",
        trajectory->hit_id,
        trajectory->out_of_bounds ? "true" : "false",
        launched_bird_id,
        remaining_birds,
        remaining_pigs,
        status);

    print_obstacles_json(level);
    print_birds_queue_json(level);
    printf("}\n");
}

bool json_parse_request(const char *json, SimRequest *out)
{
    if (json == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->action = parse_action(json);

    double value = 0.0;

    if (out->action == SIM_ACTION_LAUNCH) {
        if (!json_read_double(json, "angle", &value)) {
            return false;
        }
        out->launch.angle_deg = (float)value;

        if (!json_read_double(json, "velocity", &value)) {
            return false;
        }
        out->launch.velocity = (float)value;
    }

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

    out->obstacle_count = parse_object_array(
        json,
        "obstacles",
        parse_obstacle_wrapper,
        out->obstacle_defs,
        sizeof(ObstacleDef),
        JSON_MAX_OBSTACLES);

    out->bird_count = parse_object_array(
        json,
        "birds",
        parse_bird_object,
        out->bird_defs,
        sizeof(BirdDef),
        LEVEL_BIRD_QUEUE_CAPACITY);

    if (out->action == SIM_ACTION_LAUNCH &&
        (out->launch.max_steps <= 0 || out->launch.max_steps > PHYSICS_MAX_STEPS)) {
        return false;
    }

    return true;
}
