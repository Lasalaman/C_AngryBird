/**
 * @file bird.h
 * @brief 小鳥（Bird）遊戲物件的資料封裝與生命週期介面
 *
 * 本模組僅宣告結構體與函式原型，不含物理積分、碰撞或 main。
 * 動態配置的小鳥須透過 bird_create / bird_destroy 成對管理 malloc/free。
 */

#ifndef ANGRYBIRD_BIRD_H
#define ANGRYBIRD_BIRD_H

#include "common.h"
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  小鳥品種（影響質量、技能，後續由 C 核心或 Flask 傳入）                      */
/* -------------------------------------------------------------------------- */

/**
 * @enum BirdSpecies
 * @brief 可發射小鳥的種類代號
 *
 * 以 int 底層儲存，單一欄位通常 4 bytes，利於與 JSON / Python 整數對接。
 */
typedef enum BirdSpecies {
    BIRD_SPECIES_RED   = 0, /**< 標準直線衝擊 */
    BIRD_SPECIES_BLUE  = 1, /**< 分裂（後續邏輯擴充） */
    BIRD_SPECIES_YELLOW = 2, /**< 加速（後續邏輯擴充） */
    BIRD_SPECIES_BLACK = 3  /**< 爆炸（後續邏輯擴充） */
} BirdSpecies;

/* -------------------------------------------------------------------------- */
/*  小鳥遊戲物件                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @struct Bird
 * @brief 單隻待發射或飛行中的小鳥狀態
 *
 * 記憶體配置示意（典型 32/64 位元，預設對齊，實際 sizeof 以編譯器為準）：
 *
 *   偏移   欄位           大小（約）
 *   ----   -------------  ----------
 *   0      id             4  (uint32_t)
 *   4      species        4  (enum ≈ int)
 *   8      position       8  (Vec2: 2×float)
 *   16     velocity       8  (Vec2)
 *   24     radius         4  (float)
 *   28     mass           4  (float)
 *   32     life_state     4  (enum)
 *   36     is_alive       1  (bool) + 可能 3 bytes padding
 *   40     has_been_launched 1 (bool) + padding
 *   ----   合計約 44~48 bytes（含 padding）
 *
 * 設計要點：
 * - position / velocity 分開為兩個 Vec2，方便物理引擎分別讀寫，且各 8 bytes 對齊友好。
 * - is_alive 供熱路徑快速判斷；life_state 供「已摧毀但延遲回收」等細分狀態。
 * - has_been_launched 區分「彈弓上 / 佇列中」與「已離開發射點」的鳥。
 *
 * 動態配置：Bird* 單筆通常 bird_create() 以 malloc(sizeof(Bird)) 配置；
 * 批次場景可另建 Bird* 陣列，由呼叫端統一 free。
 */
typedef struct Bird {
    uint32_t id; /**< 全關卡唯一識別碼，供 Flask 與 C 引擎對帳 */

    BirdSpecies species; /**< 品種，決定預設質量與特殊行為 */

    Vec2 position; /**< 世界座標 (x, y)，單位：像素 */
    Vec2 velocity; /**< 速度 (vx, vy)，單位：像素/秒 */

    float radius; /**< 碰撞圓半徑（像素），用於圓形簡化碰撞 */
    float mass;   /**< 質量（任意單位，與力/加速度計算一致即可） */

    LifeState life_state; /**< 生命週期狀態（見 common.h） */
    bool is_alive;        /**< true：仍可參與物理與碰撞 */

    bool has_been_launched; /**< false：仍在佇列或彈弓；true：已進入飛行模擬 */
} Bird;

/* -------------------------------------------------------------------------- */
/*  函式原型（僅配置/釋放與欄位初始化，不含遊戲主迴圈）                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief 在堆積上配置一隻小鳥並填入預設欄位
 * @param id         唯一識別碼
 * @param species    品種
 * @param spawn_x    初始 x（通常為彈弓或佇列邏輯位置）
 * @param spawn_y    初始 y
 * @return 成功回傳 Bird*（呼叫端負責 bird_destroy）；失敗回傳 NULL（malloc 失敗）
 *
 * 記憶體：malloc(sizeof(Bird))，失敗時不配置、無需 free。
 */
Bird *bird_create(uint32_t id, BirdSpecies species, float spawn_x, float spawn_y);

/**
 * @brief 釋放由 bird_create 配置的小鳥
 * @param bird 可為 NULL（安全 no-op）
 *
 * 記憶體：free(bird)。勿對棧上 Bird 或佇列內嵌緩衝區內的元素呼叫此函式，
 * 除非該元素本身是以 malloc 獨立配置的拷貝。
 */
void bird_destroy(Bird *bird);

/**
 * @brief 就地初始化已存在的 Bird（堆積或棧皆可）
 * @param out        目標結構指標，不可為 NULL
 * @param id         唯一識別碼
 * @param species    品種
 * @param spawn_x    初始 x
 * @param spawn_y    初始 y
 *
 * 記憶體：不配置新區塊，僅寫入 *out。適用於 BirdQueue 環形緩衝區內的元素。
 */
void bird_init(Bird *out, uint32_t id, BirdSpecies species, float spawn_x, float spawn_y);

/**
 * @brief 標記小鳥為已摧毀（不釋放記憶體）
 * @param bird 目標小鳥，不可為 NULL
 *
 * 將 is_alive 設為 false、life_state 設為 LIFE_STATE_DESTROYED。
 * 實際 free 由關卡清理或佇列彈出邏輯另行處理。
 */
void bird_mark_destroyed(Bird *bird);

/**
 * @brief 深拷貝來源小鳥狀態至目的地
 * @param dest 目的地，不可為 NULL
 * @param src  來源，不可為 NULL
 *
 * 記憶體：僅複製 sizeof(Bird) 位元組，不涉及指標欄位深拷。
 */
void bird_copy(Bird *dest, const Bird *src);

#endif /* ANGRYBIRD_BIRD_H */
