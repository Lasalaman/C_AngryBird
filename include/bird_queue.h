/**
 * @file bird_queue.h
 * @brief 待發射小鳥的 FIFO 佇列（環形緩衝區 + 動態記憶體）
 *
 * 以 Queue（先進先出）管理「下一隻要發射的小鳥」順序。
 * 內部使用 malloc 配置 Bird 陣列，透過 head / count 實作環形佇列，避免
 * dequeue 時搬移整段記憶體（O(1) 入隊與出隊）。
 *
 * 不含彈弓發射物理或 main；僅資料結構與佇列操作原型。
 */

#ifndef ANGRYBIRD_BIRD_QUEUE_H
#define ANGRYBIRD_BIRD_QUEUE_H

#include "bird.h"
#include <stddef.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*  佇列容器                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @struct BirdQueue
 * @brief 小鳥 FIFO 佇列的控制結構（環形緩衝區）
 *
 * 記憶體佈局（64 位元平台典型示意）：
 *
 *   欄位           說明
 *   -------------  ----------------------------------------------------------
 *   buffer         指向 Bird 陣列的指標（heap: malloc(capacity * sizeof(Bird))）
 *   capacity       環形陣列容量（最多可容納的元素個數）
 *   head           佇列前端索引 [0, capacity)，指向「下一個將被 dequeue」的槽位
 *   count          目前元素個數，0 <= count <= capacity
 *
 * 環形索引公式（不搬移記憶體）：
 *   - 佇列尾端下一個寫入位置 tail = (head + count) % capacity
 *   - enqueue：寫入 buffer[tail]，count++
 *   - dequeue：讀取 buffer[head]，head = (head + 1) % capacity，count--
 *
 * 為何存 Bird 值而非 Bird*：
 *   - 佇列內小鳥尚未進入「飛行中實體列表」，以值語意儲存可獨立於
 *     場上動態 Bird* 生命週期，避免懸空指標。
 *   - 每槽 sizeof(Bird) 約 48 bytes；capacity=8 約 384 bytes + 控制結構。
 *
 * 整體配置流程：
 *   1. bird_queue_create(capacity) -> malloc(BirdQueue) + malloc(Bird * capacity)
 *   2. bird_queue_enqueue 複製或初始化 Bird 至 buffer[tail]
 *   3. bird_queue_dequeue 將前端 Bird 複製至呼叫端提供的 out
 *   4. bird_queue_destroy -> free(buffer); free(queue)
 *
 * 注意：bird_queue_destroy 不會對佇列內 Bird 內含之指標做深釋放
 * （目前 Bird 無堆積子指標）；若日後擴充，須在 destroy 前清空並釋放子資源。
 */
typedef struct BirdQueue {
    Bird *buffer;   /**< 環形陣列基底；NULL 表示未配置或已釋放 */
    size_t capacity; /**< 陣列槽位總數，建立後不變 */
    size_t head;     /**< 前端索引 */
    size_t count;    /**< 目前元素數量 */
} BirdQueue;

/* -------------------------------------------------------------------------- */
/*  佇列操作函式原型（FIFO）                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief 建立空佇列
 * @param capacity 最大容量，須 > 0
 * @return BirdQueue*；失敗（malloc 或參數無效）回傳 NULL
 *
 * 記憶體：malloc(sizeof(BirdQueue)) + malloc(capacity * sizeof(Bird))
 */
BirdQueue *bird_queue_create(size_t capacity);

/**
 * @brief 釋放佇列及其環形緩衝區
 * @param queue 可為 NULL
 */
void bird_queue_destroy(BirdQueue *queue);

/**
 * @brief 重置佇列為空（不釋放 buffer，可重用）
 * @param queue 不可為 NULL
 */
void bird_queue_clear(BirdQueue *queue);

/**
 * @brief 佇列是否為空
 */
bool bird_queue_is_empty(const BirdQueue *queue);

/**
 * @brief 佇列是否已滿
 */
bool bird_queue_is_full(const BirdQueue *queue);

/**
 * @brief 目前元素個數
 */
size_t bird_queue_size(const BirdQueue *queue);

/**
 * @brief 佇列容量上限
 */
size_t bird_queue_capacity(const BirdQueue *queue);

/**
 * @brief 入隊（Enqueue）：將小鳥加到佇列尾端
 * @param queue 不可為 NULL
 * @param bird  要加入的小鳥狀態（會複製進 buffer，不取得 bird 的所有權）
 * @return true 成功；false 佇列已滿或參數無效
 *
 * 記憶體：僅寫入既有 buffer 槽位，無額外 malloc。
 */
bool bird_queue_enqueue(BirdQueue *queue, const Bird *bird);

/**
 * @brief 出隊（Dequeue）：取出佇列前端小鳥（FIFO）
 * @param queue 不可為 NULL
 * @param out   輸出拷貝目的地，不可為 NULL
 * @return true 成功；false 佇列為空或參數無效
 */
bool bird_queue_dequeue(BirdQueue *queue, Bird *out);

/**
 * @brief 窺視佇列前端（不移除）
 * @param out 接收前端 Bird 的拷貝
 * @return true 成功；false 佇列為空
 */
bool bird_queue_peek_front(const BirdQueue *queue, Bird *out);

#endif /* ANGRYBIRD_BIRD_QUEUE_H */
