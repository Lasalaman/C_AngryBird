/**
 * @file main.c
 * @brief 從 stdin 讀取 Flask 傳入的 JSON，執行拋體模擬並以 JSON 輸出至 stdout
 *
 * 用法（Flask subprocess）：
 *   echo '{"angle":45,"velocity":400,...}' | ./angrybird_sim
 */

#include "json_io.h"
#include "level.h"
#include "physics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 從 stdin 讀取一行 JSON（長度受 JSON_INPUT_MAX 限制）
 *
 * 安全設計：
 * - 使用固定大小堆積緩衝區 input[JSON_INPUT_MAX]，fgets 絕不寫超過其大小。
 * - 讀取後檢查是否因過長而被截斷（無換行且未 EOF 則視為錯誤）。
 */
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
        /* 輸入超過 JSON_INPUT_MAX-1，避免後續解析不完整卻繼續存取 */
        free(line);
        return NULL;
    }

    return line;
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

    Level *level = init_level(
        0,
        request.launch.bounds,
        request.launch.gravity_y,
        request.launch.time_step,
        request.obstacle_defs,
        request.obstacle_count,
        NULL,
        0);

    if (level == NULL) {
        printf("{\"error\":\"level_init_failed\"}\n");
        return 1;
    }

    TrajectoryResult trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    const bool ok = physics_simulate_trajectory(
        &request.launch,
        level->obstacles,
        level->obstacle_count,
        &trajectory);

    if (!ok) {
        free_level(level);
        printf("{\"error\":\"simulation_failed\"}\n");
        return 1;
    }

    json_print_response(&trajectory);

    trajectory_result_free(&trajectory);
    free_level(level);

    return 0;
}
