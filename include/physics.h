/**
 * @file physics.h
 * @brief 拋體運動軌跡計算、邊界檢查與簡化碰撞
 */

#ifndef ANGRYBIRD_PHYSICS_H
#define ANGRYBIRD_PHYSICS_H

#include "common.h"
#include "level.h"
#include "obstacle.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** 單次模擬允許的最大步數（防止無限迴圈佔滿記憶體） */
#define PHYSICS_MAX_STEPS 2000

/**
 * @struct TrajectoryPoint
 * @brief 軌跡上某一時間點的狀態
 */
typedef struct TrajectoryPoint {
    float t; /**< 時間（秒） */
    float x; /**< 水平座標 */
    float y; /**< 垂直座標 */
} TrajectoryPoint;

/**
 * @struct TrajectoryResult
 * @brief 拋體模擬輸出（堆積配置）
 *
 * points 由 physics_simulate_trajectory 以 malloc 配置，
 * 須以 trajectory_result_free 釋放。
 */
typedef struct TrajectoryResult {
    TrajectoryPoint *points; /**< 動態陣列；NULL 表示 count == 0 */
    size_t count;            /**< 有效點數，必 <= capacity */
    size_t capacity;         /**< 配置容量（上限） */
    bool hit;                /**< 是否與障礙物/豬碰撞 */
    uint32_t hit_id;         /**< 碰撞對象 id；無碰撞時為 0 */
    bool out_of_bounds;      /**< 是否因超出畫面邊界而終止 */
} TrajectoryResult;

/**
 * @struct LaunchParams
 * @brief 發射與環境參數（由 stdin JSON 填入）
 */
typedef struct LaunchParams {
    float angle_deg;   /**< 發射角度 θ（度），0° 為水平向右，正值為向上 */
    float velocity;    /**< 初始速率 v（像素/秒） */
    float start_x;     /**< 發射起點 x */
    float start_y;     /**< 發射起點 y */
    float bird_radius; /**< 小鳥碰撞半徑 */
    float gravity_y;   /**< 重力加速度（y 向下為正） */
    float time_step;   /**< 時間步長 Δt（秒） */
    int max_steps;     /**< 最大模擬步數 */
    Bounds bounds;     /**< 可玩區域邊界 */
} LaunchParams;

/**
 * @brief 計算拋體在時間 t 的位置（不寫入陣列，僅純函式）
 *
 * 公式（螢幕座標 y 向下）：
 *   vx = v * cos(θ)
 *   vy = -v * sin(θ)          // 向上發射時 vy 為負
 *   x(t) = x0 + vx * t
 *   y(t) = y0 + vy * t + 0.5 * g * t²
 */
void physics_position_at_time(
    float x0,
    float y0,
    float angle_deg,
    float velocity,
    float gravity_y,
    float t,
    float *out_x,
    float *out_y);

/**
 * @brief 判斷座標是否在邊界內（含邊界）
 */
bool physics_is_inside_bounds(const Bounds *bounds, float x, float y);

/**
 * @brief 將座標限制在邊界內（避免寫出畫外數值供顯示用）
 */
void physics_clamp_to_bounds(const Bounds *bounds, float *x, float *y);

/**
 * @brief 模擬完整軌跡直到碰撞、出界或達 max_steps
 *
 * @param params           發射參數，不可為 NULL
 * @param obstacles        障礙物陣列；obstacle_count==0 時可為 NULL
 * @param obstacle_count   陣列長度
 * @param out              輸出結果；不可為 NULL
 * @return true 成功；false 參數無效或 malloc 失敗
 */
bool physics_simulate_trajectory(
    const LaunchParams *params,
    const Obstacle *obstacles,
    size_t obstacle_count,
    TrajectoryResult *out);

/**
 * @brief 釋放 TrajectoryResult 內的 points 陣列
 */
void trajectory_result_free(TrajectoryResult *result);

#endif /* ANGRYBIRD_PHYSICS_H */
