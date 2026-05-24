/**
 * @file json_io.h
 * @brief 供 Flask subprocess 使用的精簡 JSON 讀寫（無第三方函式庫）
 */

#ifndef ANGRYBIRD_JSON_IO_H
#define ANGRYBIRD_JSON_IO_H

#include "level.h"
#include "level_defaults.h"
#include "physics.h"
#include <stddef.h>
#include <stdbool.h>

/** stdin 單行 JSON 最大長度（含結尾 \\0） */
#define JSON_INPUT_MAX 65536

/** 可解析的障礙物數量上限 */
#define JSON_MAX_OBSTACLES 128

typedef enum SimAction {
    SIM_ACTION_LAUNCH = 0,
    SIM_ACTION_RESET = 1
} SimAction;

/**
 * @struct SimRequest
 * @brief 從 stdin 解析出的完整模擬請求
 */
typedef struct SimRequest {
    SimAction action;
    LaunchParams launch;
    ObstacleDef obstacle_defs[JSON_MAX_OBSTACLES];
    size_t obstacle_count;
    BirdDef bird_defs[LEVEL_BIRD_QUEUE_CAPACITY];
    size_t bird_count;
} SimRequest;

bool json_parse_request(const char *json, SimRequest *out);

/** 輸出發射結果（含 trajectory、remaining_birds、remaining_pigs） */
void json_print_launch_result(
    const TrajectoryResult *trajectory,
    const Level *level,
    uint32_t launched_bird_id);

/** 輸出關卡重置結果（無 trajectory） */
void json_print_reset_result(const Level *level);

#endif /* ANGRYBIRD_JSON_IO_H */
