/**
 * @file bird_queue.c
 * @brief 待發射小鳥 FIFO 環形佇列實作
 */

#include "bird_queue.h"

#include <stdlib.h>

BirdQueue *bird_queue_create(size_t capacity)
{
    if (capacity == 0) {
        return NULL;
    }

    BirdQueue *queue = (BirdQueue *)malloc(sizeof(BirdQueue));
    if (queue == NULL) {
        return NULL;
    }

    /*
     * buffer 大小 = capacity * sizeof(Bird)
     * 指標 queue->buffer 指向這塊連續記憶體的第一個 Bird 元素。
     */
    queue->buffer = (Bird *)malloc(capacity * sizeof(Bird));
    if (queue->buffer == NULL) {
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    queue->head = 0;
    queue->count = 0;
    return queue;
}

void bird_queue_destroy(BirdQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    free(queue->buffer);
    queue->buffer = NULL;
    free(queue);
}

void bird_queue_clear(BirdQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    queue->head = 0;
    queue->count = 0;
}

bool bird_queue_is_empty(const BirdQueue *queue)
{
    return queue == NULL || queue->count == 0;
}

bool bird_queue_is_full(const BirdQueue *queue)
{
    return queue != NULL && queue->count >= queue->capacity;
}

size_t bird_queue_size(const BirdQueue *queue)
{
    if (queue == NULL) {
        return 0;
    }

    return queue->count;
}

size_t bird_queue_capacity(const BirdQueue *queue)
{
    if (queue == NULL) {
        return 0;
    }

    return queue->capacity;
}

bool bird_queue_enqueue(BirdQueue *queue, const Bird *bird)
{
    if (queue == NULL || bird == NULL || bird_queue_is_full(queue)) {
        return false;
    }

    size_t tail = (queue->head + queue->count) % queue->capacity;
    bird_copy(&queue->buffer[tail], bird);
    queue->count++;
    return true;
}

bool bird_queue_dequeue(BirdQueue *queue, Bird *out)
{
    if (queue == NULL || out == NULL || bird_queue_is_empty(queue)) {
        return false;
    }

    bird_copy(out, &queue->buffer[queue->head]);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return true;
}

bool bird_queue_peek_front(const BirdQueue *queue, Bird *out)
{
    if (queue == NULL || out == NULL || bird_queue_is_empty(queue)) {
        return false;
    }

    bird_copy(out, &queue->buffer[queue->head]);
    return true;
}
