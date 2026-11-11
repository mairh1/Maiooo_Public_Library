/**
 * @file    ringbuffer.h
 * @brief   单生产者单消费者（SPSC）无锁环形缓冲区
 * @note    适用于 中断（生产者）与 主循环（消费者）之间的安全数据交换，无需关中断
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t *buffer;            /**< 缓冲区内存指针 */
    uint32_t capacity;          /**< 缓冲区总容量 */
    volatile uint32_t head;     /**< 写入游标 (由生产者更新) */
    volatile uint32_t tail;     /**< 读取游标 (由消费者更新) */
} RingBuffer_t;

/**
 * @brief 初始化环形缓冲区
 */
static inline void RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint32_t capacity)
{
    rb->buffer = storage;
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief 获取已存储的字节数
 */
static inline uint32_t RingBuffer_GetUsed(const RingBuffer_t *rb)
{
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    if (head >= tail)
    {
        return head - tail;
    }
    return rb->capacity - (tail - head);
}

/**
 * @brief 获取剩余可用空间
 */
static inline uint32_t RingBuffer_GetFree(const RingBuffer_t *rb)
{
    return rb->capacity - 1 - RingBuffer_GetUsed(rb);
}

/**
 * @brief 压入单个字节
 */
static inline bool RingBuffer_Put(RingBuffer_t *rb, uint8_t byte)
{
    uint32_t next_head = (rb->head + 1) % rb->capacity;
    if (next_head == rb->tail)
    {
        return false; /* 满 */
    }
    rb->buffer[rb->head] = byte;
    rb->head = next_head;
    return true;
}

/**
 * @brief 弹出单个字节
 */
static inline bool RingBuffer_Get(RingBuffer_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail)
    {
        return false; /* 空 */
    }
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* RINGBUFFER_H */
