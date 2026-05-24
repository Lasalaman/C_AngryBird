/**
 * @file json_io.h
 * @brief 供 Flask subprocess 使用的精簡 JSON 讀寫（無第三方函式庫）
 */

#ifndef ANGRYBIRD_JSON_IO_H
#define ANGRYBIRD_JSON_IO_H

#include "level.h"
#include "physics.h"
#include <stddef.h>
#include <stdbool.h>

/** stdin 單行 JSON 最大長度（含結尾 \\0） */
#define JSON_INPUT_MAX 65536

/** 可解析的障礙物數量上限 */
#define JSON_MAX_OBSTACLES 128

/**
 * @struct SimRequest
 * @brief 從 stdin 解析出的完整模擬請求
 */
typedef struct SimRequest {
    LaunchParams launch;
    ObstacleDef obstacle_defs[JSON_MAX_OBSTACLES];
    size_t obstacle_count;
} SimRequest;

/**
 * @brief 從 JSON 字串解析模擬參數
 * @return true 成功；false 格式錯誤
 */
bool json_parse_request(const char *json, SimRequest *out);

/**
 * @brief 將軌跡與碰撞結果以 JSON 印至 stdout（單行）
 */
void json_print_response(const TrajectoryResult *result);

#endif /* ANGRYBIRD_JSON_IO_H */
