/**
 * @file common.h
 * @brief 憤怒鳥專案共用型別、列舉與二維向量
 *
 * 本檔定義全專案共用的基礎型別，供遊戲物件、關卡與佇列標頭引用。
 * 不包含任何實作邏輯或 main 函式。
 */

#ifndef ANGRYBIRD_COMMON_H
#define ANGRYBIRD_COMMON_H

#include <stddef.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*  實體型別代號（物件種類）                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @enum EntityKind
 * @brief 遊戲世界中可互動實體的種類
 *
 * 用於碰撞、計分與繪製時區分處理流程，不佔用大量記憶體（通常為 4 bytes int）。
 */
typedef enum EntityKind {
    ENTITY_KIND_BIRD     = 0, /**< 玩家發射的小鳥 */
    ENTITY_KIND_PIG      = 1, /**< 敵方豬（通關目標） */
    ENTITY_KIND_OBSTACLE = 2  /**< 結構障礙物（木塊、石塊等） */
} EntityKind;

/* -------------------------------------------------------------------------- */
/*  生命週期 / 存活狀態                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @enum LifeState
 * @brief 實體是否仍可參與物理與碰撞
 *
 * 與 bool is_alive 可並存：is_alive 為快速布林判斷；
 * life_state 保留更細粒度（例如「已摧毀但尚未從畫面移除」）。
 */
typedef enum LifeState {
    LIFE_STATE_ACTIVE    = 0, /**< 正常運作中 */
    LIFE_STATE_DESTROYED = 1, /**< 已被摧毀（HP 歸零或觸發銷毀條件） */
    LIFE_STATE_REMOVED   = 2  /**< 已從模擬中移除，可安全回收記憶體 */
} LifeState;

/* -------------------------------------------------------------------------- */
/*  二維向量（座標、速度共用表示）                                              */
/* -------------------------------------------------------------------------- */

/**
 * @struct Vec2
 * @brief 二維浮點向量，表示位置 (x, y) 或速度 (vx, vy)
 *
 * 記憶體配置（典型 32/64 位元平台，#pragma pack 預設）：
 *   偏移 0:  float x  (4 bytes)
 *   偏移 4:  float y  (4 bytes)
 *   總大小:  8 bytes（無額外 padding）
 *
 * 使用 float 而非 double 可減少快取佔用，對 2D 休閒遊戲精度足夠。
 * 若 Bird / Obstacle 內嵌兩個 Vec2（position + velocity），
 * 該區塊約 16 bytes，利於連續陣列迭代時的空間局部性。
 */
typedef struct Vec2 {
    float x; /**< 水平分量（螢幕座標：向右為正） */
    float y; /**< 垂直分量（螢幕座標：向下為正，與常見圖形 API 一致） */
} Vec2;

#endif /* ANGRYBIRD_COMMON_H */
