/**
 * @file obstacle.h
 * @brief 豬（Pig）與結構障礙物（Obstacle）的共用遊戲物件封裝
 *
 * 豬與木石等障礙物在資料面上共用 Obstacle 結構，以 EntityKind 區分語意，
 * 減少重複程式碼並利於統一碰撞陣列迭代。僅標頭定義，無 main 與完整物理。
 */

#ifndef ANGRYBIRD_OBSTACLE_H
#define ANGRYBIRD_OBSTACLE_H

#include "common.h"
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  障礙物材質（影響耐久與得分，後續邏輯擴充）                                  */
/* -------------------------------------------------------------------------- */

/**
 * @enum ObstacleMaterial
 * @brief 結構物材質；豬（ENTITY_KIND_PIG）可忽略或設為 MATERIAL_NONE
 */
typedef enum ObstacleMaterial {
    MATERIAL_NONE  = 0, /**< 豬或無材質區分 */
    MATERIAL_WOOD  = 1, /**< 木塊：較低耐久 */
    MATERIAL_STONE = 2, /**< 石塊：較高耐久 */
    MATERIAL_GLASS = 3  /**< 玻璃：極低耐久、高得分 */
} ObstacleMaterial;

/* -------------------------------------------------------------------------- */
/*  豬 / 障礙物遊戲物件                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @struct Obstacle
 * @brief 關卡中靜態或動態的敵方目標與結構物
 *
 * 記憶體配置示意（預設對齊）：
 *
 *   偏移   欄位              大小（約）
 *   ----   ----------------  ----------
 *   0      id                4
 *   4      kind              4  (EntityKind)
 *   8      material          4  (ObstacleMaterial)
 *   12     position          8  (Vec2)
 *   20     velocity          8  (Vec2)
 *   28     width             4  (float)
 *   32     height            4  (float)
 *   36     hit_points        4  (int)
 *   40     max_hit_points    4  (int)
 *   44     life_state        4
 *   48     is_alive          1  + padding
 *   52     is_static         1  + padding
 *   ----   合計約 56~64 bytes
 *
 * 碰撞表示：
 * - 豬與小型物件：可用 width/height 相等形成近似圓形（取 min 為直徑參考）。
 * - 長方結構：以 position 為左上角或中心（由 level 約定），width × height 為 AABB。
 *
 * 動態配置：單一 Obstacle* 使用 obstacle_create / obstacle_destroy；
 * 關卡載入時可 malloc(sizeof(Obstacle) * n) 連續陣列，一次 free 整塊。
 */
typedef struct Obstacle {
    uint32_t id;       /**< 關卡內唯一識別碼 */
    EntityKind kind;   /**< ENTITY_KIND_PIG 或 ENTITY_KIND_OBSTACLE */
    ObstacleMaterial material; /**< 材質；豬通常為 MATERIAL_NONE */

    Vec2 position; /**< 參考點座標 (x, y) */
    Vec2 velocity; /**< 速度 (vx, vy)；靜態物為 (0, 0) */

    float width;  /**< 碰撞盒寬度（像素） */
    float height; /**< 碰撞盒高度（像素） */

    int hit_points;     /**< 當前耐久 / 生命值 */
    int max_hit_points; /**< 初始最大值，用於 UI 血條比例 */

    LifeState life_state; /**< 存活週期狀態 */
    bool is_alive;        /**< true：仍參與碰撞與計分 */

    bool is_static; /**< true：不每幀積分位置（優化靜態結構） */
} Obstacle;

/* -------------------------------------------------------------------------- */
/*  函式原型                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief 在堆積上配置一個障礙物/豬實體
 * @return 成功回傳 Obstacle*；malloc 失敗回傳 NULL
 */
Obstacle *obstacle_create(
    uint32_t id,
    EntityKind kind,
    ObstacleMaterial material,
    float x,
    float y,
    float width,
    float height,
    int hit_points,
    bool is_static
);

/**
 * @brief 釋放 obstacle_create 配置的實體
 * @param obstacle 可為 NULL
 */
void obstacle_destroy(Obstacle *obstacle);

/**
 * @brief 就地初始化 Obstacle（堆積或連續陣列元素）
 */
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
    bool is_static
);

/**
 * @brief 扣除耐久；若 hit_points <= 0 則標記摧毀
 * @param damage 傷害量（正整數）
 * @return true 表示本次傷害導致實體進入 DESTROYED 狀態
 *
 * 不含 free；僅更新欄位，供碰撞回呼使用。
 */
bool obstacle_apply_damage(Obstacle *obstacle, int damage);

/**
 * @brief 標記為已摧毀（不釋放記憶體）
 */
void obstacle_mark_destroyed(Obstacle *obstacle);

/**
 * @brief 位元組級拷貝
 */
void obstacle_copy(Obstacle *dest, const Obstacle *src);

#endif /* ANGRYBIRD_OBSTACLE_H */
