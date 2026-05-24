/**
 * @file bird_game.h
 * @brief 關卡級遊戲狀態（障礙物存活、小鳥佇列）與跨 subprocess 持久化
 *
 * 因 Flask 每次 launch 啟動新的 C 子行程，無法保留堆積記憶體；
 * 故以專案根目錄的 game_state.json 作為「關卡級 Struct」的持久化載體。
 * 僅 RESET 時才還原為完整初始關卡。
 */

#ifndef ANGRYBIRD_BIRD_GAME_H
#define ANGRYBIRD_BIRD_GAME_H

#include "level.h"
#include "level_defaults.h"
#include <stddef.h>
#include <stdbool.h>

#define BIRD_GAME_MAX_OBSTACLES 128

/** 持久化檔路徑（相對於 subprocess cwd = 專案根） */
#define GAME_STATE_FILENAME "game_state.json"

/**
 * @struct GameSession
 * @brief 當前關卡所有可變狀態（對應 Level 內 Obstacle 與 BirdQueue 的「存檔快照」）
 *
 * persistent_obstacles[]：每個元素的 is_alive 在碰撞後由 C 核心改為 false，
 * 並在 launch 結束時寫回此陣列與 JSON 檔，下次 launch 絕不讀取「乾淨預設關卡」。
 */
typedef struct GameSession {
    uint32_t level_id;
    Bounds bounds;
    float gravity_y;
    float time_step;

    ObstacleDef persistent_obstacles[BIRD_GAME_MAX_OBSTACLES];
    size_t obstacle_count;

    BirdDef bird_queue[LEVEL_BIRD_QUEUE_CAPACITY];
    size_t bird_count;
} GameSession;

/** 還原為關卡 1 預設（3 鳥、全障礙物存活） */
void game_session_reset_defaults(GameSession *session);

/** 從 game_state.json 載入；檔案不存在則 false */
bool game_state_load(GameSession *session);

/** 將目前關卡狀態寫入 game_state.json；成功回傳 true */
bool game_state_save(const GameSession *session);

/** RESET 時刪除舊檔並寫入全新狀態 */
bool game_state_reset_file(const GameSession *session);

/** 由執行中的 Level 回寫障礙物存活與小鳥佇列至 Session */
void game_session_sync_from_level(const Level *level, GameSession *session);

/** 由 Session 建立 Level（malloc 障礙物陣列 + BirdQueue） */
Level *game_session_build_level(const GameSession *session);

#endif /* ANGRYBIRD_BIRD_GAME_H */
