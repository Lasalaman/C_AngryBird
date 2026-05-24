/**
 * @file level.h
 * @brief 關卡全域狀態：邊界、重力、計數與模擬參數
 *
 * 封裝「不屬於單一實體」的環境資料，供物理步進與 Flask 同步關卡進度。
 * 不含關卡載入檔解析或 main。
 */

#ifndef ANGRYBIRD_LEVEL_H
#define ANGRYBIRD_LEVEL_H

#include "bird.h"
#include "bird_queue.h"
#include "common.h"
#include "obstacle.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*  矩形邊界（畫面 / 可玩區域）                                                */
/* -------------------------------------------------------------------------- */

/**
 * @struct Bounds
 * @brief 軸對齊矩形邊界，表示可玩區域或攝影機視窗
 *
 * 記憶體：4 × float = 16 bytes，無 padding 問題。
 * 座標系：x 向右、y 向下為正（與 common.h Vec2 一致）。
 */
typedef struct Bounds {
    float min_x; /**< 左邊界 */
    float min_y; /**< 上邊界 */
    float max_x; /**< 右邊界（不含或含邊界由碰撞實作約定） */
    float max_y; /**< 下邊界 */
} Bounds;

/* -------------------------------------------------------------------------- */
/*  關卡狀態                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @struct LevelState
 * @brief 單一關卡的模擬環境與勝負計數
 *
 * 記憶體配置示意：
 *
 *   偏移   欄位                    大小（約）
 *   ----   ----------------------  ----------
 *   0      level_id                4
 *   4      play_bounds             16 (Bounds)
 *   20     gravity_y               4  (float)
 *   24     time_step               4  (float)
 *   28     birds_remaining         4  (int)
 *   32     pigs_remaining          4  (int)
 *   36     is_paused               1  (bool)
 *   37     is_level_complete       1  (bool)
 *   38     is_level_failed         1  (bool)
 *   ----   合計約 40~48 bytes（含 bool 後 padding）
 *
 * 規劃說明：
 * - play_bounds：小鳥與物件不得超出之區域；落出邊界可標記移除。
 * - gravity_y：垂直重力加速度（像素/秒²），向下為正時取正值。
 * - time_step：固定時間步長（秒），供 C 核心固定 Δt 積分。
 * - birds_remaining / pigs_remaining：與佇列、障礙物陣列同步更新，供 Web 顯示。
 *
 * 配置方式：
 * - 建議單一 LevelState  per 模擬 session：malloc(sizeof(LevelState)) 或
 *   嵌入遊戲 Session 結構（後續擴充）。
 */
typedef struct LevelState {
    uint32_t level_id; /**< 關卡編號，對應關卡 JSON / 資料檔 */

    Bounds play_bounds; /**< 可玩區域邊界 */

    float gravity_y; /**< 重力常數（y 方向加速度） */
    float time_step; /**< 物理更新步長 Δt（秒） */

    int birds_remaining; /**< 尚未發射或未摧毀的己方單位計數（語意由遊戲層定義） */
    int pigs_remaining;  /**< 尚未摧毀的豬數量 */

    bool is_paused;          /**< 暫停模擬（不推進物理） */
    bool is_level_complete;  /**< 通關條件已滿足 */
    bool is_level_failed;    /**< 失敗條件已滿足（例如鳥用盡） */
} LevelState;

/* -------------------------------------------------------------------------- */
/*  關卡載入用描述（不含指標，供 init_level 複製進堆積陣列）                    */
/* -------------------------------------------------------------------------- */

/**
 * @struct ObstacleDef
 * @brief 單一障礙物/豬的佈局描述（由關卡 JSON 或 Flask 轉入）
 */
typedef struct ObstacleDef {
    uint32_t id;
    EntityKind kind;
    ObstacleMaterial material;
    float x;
    float y;
    float width;
    float height;
    int hit_points;
    bool is_static;
    bool is_alive; /**< false 表示已摧毀，launch 時不參與碰撞且前端不繪製 */
} ObstacleDef;

/**
 * @struct BirdDef
 * @brief 待發射小鳥佇列的初始項目
 */
typedef struct BirdDef {
    uint32_t id;
    BirdSpecies species;
    float spawn_x;
    float spawn_y;
} BirdDef;

/* -------------------------------------------------------------------------- */
/*  完整關卡實例（聚合動態配置區塊）                                            */
/* -------------------------------------------------------------------------- */

/**
 * @struct Level
 * @brief 一關遊戲所需的全部堆積資源
 *
 * 記憶體擁有權：
 * - 本結構本身：init_level 以 malloc(sizeof(Level)) 配置，free_level 以 free 釋放。
 * - obstacles：指向連續 Obstacle 陣列，大小 obstacle_count * sizeof(Obstacle)。
 * - launch_queue：BirdQueue 及其內部 buffer，由 bird_queue_create / destroy 管理。
 *
 * state 為內嵌值（非指標），隨 Level 一併配置與釋放，無需單獨 free。
 */
typedef struct Level {
    LevelState state;       /**< 邊界、重力、勝負計數等全域狀態 */
    Obstacle *obstacles;    /**< 動態陣列；NULL 表示 obstacle_count == 0 */
    size_t obstacle_count;  /**< 陣列元素個數（豬 + 結構物總和） */
    BirdQueue *launch_queue; /**< 待發射小鳥 FIFO；可為 NULL（無鳥關卡） */
} Level;

/* -------------------------------------------------------------------------- */
/*  函式原型                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief 初始化一整關：配置 Level、Obstacle 陣列與發射佇列
 *
 * @param level_id        關卡編號
 * @param bounds          可玩區域
 * @param gravity_y       重力加速度（y 向下為正）
 * @param time_step       物理步長（秒）
 * @param obstacle_defs   障礙物/豬描述陣列；可為 NULL（當 obstacle_count == 0）
 * @param obstacle_count  陣列長度
 * @param bird_defs       小鳥描述陣列；可為 NULL（當 bird_count == 0）
 * @param bird_count      待發射小鳥數量
 * @return 成功回傳 Level*；任何 malloc 失敗回傳 NULL（內部已釋放部分資源）
 */
/** 由 ObstacleDef 初始化，並尊重 def->is_alive（持久化關卡用） */
void obstacle_init_from_def(Obstacle *out, const ObstacleDef *def);

Level *init_level(
    uint32_t level_id,
    Bounds bounds,
    float gravity_y,
    float time_step,
    const ObstacleDef *obstacle_defs,
    size_t obstacle_count,
    const BirdDef *bird_defs,
    size_t bird_count
);

/**
 * @brief 釋放 init_level 配置的所有堆積記憶體
 * @param level 可為 NULL（安全 no-op）
 */
void free_level(Level *level);


/**
 * @brief 配置並初始化關卡狀態
 * @param level_id      關卡 ID
 * @param bounds        可玩區域
 * @param gravity_y     重力
 * @param time_step     時間步長
 * @param initial_birds 初始可用小鳥數
 * @param initial_pigs  初始豬數量
 * @return LevelState* 或 NULL（malloc 失敗）
 */
LevelState *level_create(
    uint32_t level_id,
    Bounds bounds,
    float gravity_y,
    float time_step,
    int initial_birds,
    int initial_pigs
);

/**
 * @brief 釋放 level_create 配置的關卡狀態
 */
void level_destroy(LevelState *level);

/**
 * @brief 就地初始化 LevelState（不配置堆積）
 */
void level_init(
    LevelState *out,
    uint32_t level_id,
    Bounds bounds,
    float gravity_y,
    float time_step,
    int initial_birds,
    int initial_pigs
);

/**
 * @brief 判斷世界座標是否在可玩邊界內
 * @return true 表示 (x, y) 位於 [min_x, max_x] × [min_y, max_y]（邊界含義由實作統一）
 */
bool level_contains_point(const LevelState *level, float x, float y);

/**
 * @brief 將關卡標記為通關（僅設旗標，不釋放資源）
 */
void level_mark_complete(LevelState *level);

/**
 * @brief 將關卡標記為失敗
 */
void level_mark_failed(LevelState *level);

/**
 * @brief 統計場上仍存活且未移除的豬數量
 */
int level_count_alive_pigs(const Level *level);

/**
 * @brief 從 FIFO 佇列取出下一隻待發射小鳥（Dequeue）
 * @param out_bird 輸出拷貝；不可為 NULL
 * @return true 成功；false 佇列為空或參數無效
 *
 * 記憶體：僅讀寫 launch_queue 環形緩衝區內的 Bird 值，不額外 malloc。
 * 呼叫端須在 free_level() 時一併釋放整個 queue，避免逐鳥 free 造成雙重釋放。
 */
bool level_consume_bird_from_queue(Level *level, Bird *out_bird);

#endif /* ANGRYBIRD_LEVEL_H */
