/**
 * @file main.c
 * @brief Flask subprocess：RESET 還原關卡 / LAUNCH 從持久化狀態發射
 */

#include "bird_game.h"
#include "json_io.h"
#include "level_defaults.h"
#include "physics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_stdin_line(void)
{
    char *line = (char *)malloc(JSON_INPUT_MAX);
    if (line == NULL) {
        return NULL;
    }

    if (fgets(line, JSON_INPUT_MAX, stdin) == NULL) {
        free(line);
        return NULL;
    }

    const size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    } else if (!feof(stdin)) {
        free(line);
        return NULL;
    }

    return line;
}

static void apply_hit_to_level(Level *level, uint32_t hit_id)
{
    if (level == NULL || level->obstacles == NULL || hit_id == 0) {
        return;
    }

    for (size_t i = 0; i < level->obstacle_count; i++) {
        Obstacle *obs = &level->obstacles[i];
        if (obs->id == hit_id && obs->is_alive) {
            obstacle_apply_damage(obs, obs->hit_points);
            break;
        }
    }

    level->state.pigs_remaining = level_count_alive_pigs(level);
    if (level->state.pigs_remaining == 0) {
        level_mark_complete(&level->state);
    }
}

static int handle_reset(const SimRequest *request)
{
    GameSession session;
    game_session_reset_defaults(&session);

    if (request != NULL) {
        session.gravity_y = request->launch.gravity_y;
        session.time_step = request->launch.time_step;
        session.bounds = request->launch.bounds;
    }

    Level *level = game_session_build_level(&session);
    if (level == NULL) {
        printf("{\"error\":\"level_init_failed\"}\n");
        return 1;
    }

    game_session_sync_from_level(level, &session);
    game_state_reset_file(&session);

    json_print_reset_result(level);
    free_level(level);
    return 0;
}

static int handle_launch(const SimRequest *request)
{
    GameSession session;

    /*
     * 優先從 game_state.json 載入持久化關卡（含已摧毀木箱 is_alive=false）。
     * 僅在檔案不存在時使用預設關卡。
     */
    if (!game_state_load(&session)) {
        game_session_reset_defaults(&session);
    }

    if (request != NULL) {
        session.gravity_y = request->launch.gravity_y;
        session.time_step = request->launch.time_step;
        session.bounds = request->launch.bounds;
    }

    Level *level = game_session_build_level(&session);
    if (level == NULL) {
        printf("{\"error\":\"level_init_failed\"}\n");
        return 1;
    }

    if (level->launch_queue == NULL || bird_queue_is_empty(level->launch_queue)) {
        free_level(level);
        printf("{\"error\":\"no_birds_remaining\"}\n");
        return 1;
    }

    Bird launched;
    if (!level_consume_bird_from_queue(level, &launched)) {
        free_level(level);
        printf("{\"error\":\"dequeue_failed\"}\n");
        return 1;
    }

    LaunchParams params = request->launch;
    params.start_x = launched.position.x;
    params.start_y = launched.position.y;
    params.bird_radius = 16.0f;

    TrajectoryResult trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    const bool ok = physics_simulate_trajectory(
        &params, level->obstacles, level->obstacle_count, &trajectory);

    if (!ok) {
        trajectory_result_free(&trajectory);
        free_level(level);
        printf("{\"error\":\"simulation_failed\"}\n");
        return 1;
    }

    if (trajectory.hit) {
        apply_hit_to_level(level, trajectory.hit_id);
    }

    game_session_sync_from_level(level, &session);
    game_state_save(&session);

    json_print_launch_result(&trajectory, level, launched.id);

    trajectory_result_free(&trajectory);
    free_level(level);
    return 0;
}

int main(void)
{
    char *input = read_stdin_line();
    if (input == NULL) {
        printf("{\"error\":\"invalid_input\"}\n");
        return 1;
    }

    SimRequest request;
    if (!json_parse_request(input, &request)) {
        free(input);
        printf("{\"error\":\"parse_failed\"}\n");
        return 1;
    }

    free(input);

    if (request.action == SIM_ACTION_RESET) {
        return handle_reset(&request);
    }

    return handle_launch(&request);
}
