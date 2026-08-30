/**
 * @file    uart_ring.c
 * @brief   串口收发环形缓冲中间件核心实现
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-30
 *
 * @details
 * 实现要点：
 * - 环缓冲内部以静态 helper（rb_* ）实现，uart_rb_* 公共 API 仅在
 *   UART_RING_USE_RB_API=1 时作为薄封装导出，避免算法随开关重复
 * - head / tail 为自由递增 uint16_t 计数器，数据量 = (uint16_t)(head - tail)，
 *   满 / 空无歧义且容量全部可用
 * - TX 自动泵的正确性依赖"生产者在非 ISR 上下文写入、不会被消费侧
 *   ISR 抢占"这一约束（详见 uart_ring.h @attention），消费者在吐空后
 *   "查空即关断" 与生产者 "写空即踢发" 两条互补路径闭合竞态窗口
 *
 * @note 仅依赖 stdint / stdbool / stddef 与本模块自身头文件，无平台符号。
 */

#include "uart_ring.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ========================================================================== */
/*  内部诊断断言                                                               */
/* ========================================================================== */

/* 空指针短路检查：参数校验始终开启，仅统一写法 */
#define UART_RING_ASSERT_PTR(p, ret)  \
    do { if ((p) == NULL) { return (ret); } } while (0)

/* ========================================================================== */
/*  内部环形缓冲 helper（不受 UART_RING_USE_RB_API 开关影响，始终编译）         */
/* ========================================================================== */

/**
 * @brief   初始化环字段（内部）
 * @return  true 参数合法并完成初始化；false 容量非法
 */
static bool rb_setup(uart_rb_t * rb, uint8_t *buf, uint16_t cap)
{
    if ((buf == NULL) || (cap == 0u) || (cap > UART_RING_MAX_CAPACITY))
    {
        return false;
    }
    if ((cap & (uint16_t)(cap - 1u)) != 0u)   /* 非 2 的幂 */
    {
        return false;
    }

    rb->buf   = buf;
    rb->cap   = cap;
    rb->mask  = (uint16_t)(cap - 1u);
    rb->head  = 0u;
    rb->tail  = 0u;
    return true;
}

/** @brief 未启用环（cap==0）与已初始化环统一安全复位 */
static void rb_clear(uart_rb_t * rb)
{
    rb->head = 0u;
    rb->tail = 0u;
}

/** @brief 写入一字节（生产者） */
static bool rb_push(uart_rb_t * rb, uint8_t byte)
{
    uint16_t next;

    if (rb->cap == 0u)
    {
        return false;
    }
    next = (uint16_t)(rb->head + 1u);
    if ((uint16_t)(next - rb->tail) > rb->cap)   /* 环满 */
    {
        return false;
    }
    rb->buf[rb->head & rb->mask] = byte;
    rb->head = next;
    return true;
}

/** @brief 弹出一字节（消费者） */
static bool rb_pop(uart_rb_t * rb, uint8_t *byte)
{
    if ((byte == NULL) || ((uint16_t)(rb->head - rb->tail) == 0u))
    {
        return false;                             /* 环空 */
    }
    *byte = rb->buf[rb->tail & rb->mask];
    rb->tail = (uint16_t)(rb->tail + 1u);
    return true;
}

/** @brief 批量写入（生产者，逐字节非原子） */
static uint16_t rb_write_bulk(uart_rb_t * rb, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0u; i < len; i++)
    {
        if (!rb_push(rb, data[i]))
        {
            break;                                /* 环满即停 */
        }
    }
    return i;
}

/** @brief 批量读出（消费者，逐字节非原子） */
static uint16_t rb_read_bulk(uart_rb_t * rb, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t  tmp;

    for (i = 0u; i < len; i++)
    {
        if (!rb_pop(rb, &tmp))
        {
            break;                                /* 环空即停 */
        }
        buf[i] = tmp;
    }
    return i;
}

/** @brief 当前数据量（字节） */
static uint16_t rb_count(const uart_rb_t * rb)
{
    return (uint16_t)(rb->head - rb->tail);
}

/** @brief 当前剩余空间（字节） */
static uint16_t rb_space(const uart_rb_t * rb)
{
    return (uint16_t)(rb->cap - rb_count(rb));
}

/* ========================================================================== */
/*  裸环形缓冲公共工具面（UART_RING_USE_RB_API = 1 时导出）                     */
/* ========================================================================== */

#if UART_RING_USE_RB_API

uart_ring_status_t uart_rb_init(uart_rb_t * rb, uint8_t *buf, uint16_t cap)
{
    UART_RING_ASSERT_PTR(rb, UART_RING_ERR_NULL_PTR);

    if (!rb_setup(rb, buf, cap))
    {
        return UART_RING_ERR_PARAM;
    }
    return UART_RING_OK;
}

bool uart_rb_push(uart_rb_t * rb, uint8_t byte)
{
    if (rb == NULL)
    {
        return false;
    }
    return rb_push(rb, byte);
}

bool uart_rb_pop(uart_rb_t * rb, uint8_t *byte)
{
    if (rb == NULL)
    {
        return false;
    }
    return rb_pop(rb, byte);
}

uint16_t uart_rb_write(uart_rb_t * rb, const uint8_t *data, uint16_t len)
{
    if ((rb == NULL) || (data == NULL))
    {
        return 0u;
    }
    return rb_write_bulk(rb, data, len);
}

uint16_t uart_rb_read(uart_rb_t * rb, uint8_t *buf, uint16_t len)
{
    if ((rb == NULL) || (buf == NULL))
    {
        return 0u;
    }
    return rb_read_bulk(rb, buf, len);
}

uint16_t uart_rb_count(const uart_rb_t * rb)
{
    if (rb == NULL)
    {
        return 0u;
    }
    return rb_count(rb);
}

uint16_t uart_rb_space(const uart_rb_t * rb)
{
    if (rb == NULL)
    {
        return 0u;
    }
    return rb_space(rb);
}

void uart_rb_reset(uart_rb_t * rb)
{
    if (rb != NULL)
    {
        rb_clear(rb);
    }
}

#endif /* UART_RING_USE_RB_API */

/* ========================================================================== */
/*  串口实例生命周期                                                           */
/* ========================================================================== */

#if UART_RING_USE_TX_IRQ

uart_ring_status_t uart_ring_init(uart_ring_t * r,
                                  uint8_t *rx_buf, uint16_t rx_cap,
                                  uint8_t *tx_buf, uint16_t tx_cap,
                                  const uart_ring_hw_t *hw, void *ctx)
{
    UART_RING_ASSERT_PTR(r, UART_RING_ERR_NULL_PTR);

    /* RX 环为必配项 */
    if (!rb_setup(&r->rx, rx_buf, rx_cap))
    {
        return UART_RING_ERR_PARAM;
    }

    /* TX 环可选：tx_cap==0 表示不启用发送缓冲，tx_buf 与 tx_cap 需匹配 */
    if (tx_cap == 0u)
    {
        r->tx.buf  = NULL;
        r->tx.cap  = 0u;
        r->tx.mask = 0u;
        rb_clear(&r->tx);
    }
    else if (!rb_setup(&r->tx, tx_buf, tx_cap))
    {
        return UART_RING_ERR_PARAM;
    }

    r->hw       = hw;
    r->user_ctx = ctx;
    r->tx_on    = 0u;
#if UART_RING_USE_STATS
    r->rx_overflow = 0u;
#endif
    return UART_RING_OK;
}

#else /* !UART_RING_USE_TX_IRQ */

uart_ring_status_t uart_ring_init(uart_ring_t * r,
                                  uint8_t *rx_buf, uint16_t rx_cap,
                                  uint8_t *tx_buf, uint16_t tx_cap)
{
    UART_RING_ASSERT_PTR(r, UART_RING_ERR_NULL_PTR);

    if (!rb_setup(&r->rx, rx_buf, rx_cap))
    {
        return UART_RING_ERR_PARAM;
    }

    if (tx_cap == 0u)
    {
        r->tx.buf  = NULL;
        r->tx.cap  = 0u;
        r->tx.mask = 0u;
        rb_clear(&r->tx);
    }
    else if (!rb_setup(&r->tx, tx_buf, tx_cap))
    {
        return UART_RING_ERR_PARAM;
    }

#if UART_RING_USE_STATS
    r->rx_overflow = 0u;
#endif
    return UART_RING_OK;
}

#endif /* UART_RING_USE_TX_IRQ */

void uart_ring_reset(uart_ring_t * r)
{
    if (r == NULL)
    {
        return;
    }

    rb_clear(&r->rx);
    rb_clear(&r->tx);
#if UART_RING_USE_STATS
    r->rx_overflow = 0u;
#endif
#if UART_RING_USE_TX_IRQ
    r->tx_on = 0u;
#endif
}

/* ========================================================================== */
/*  接收路径（ISR 生产 → 主循环消费）                                          */
/* ========================================================================== */

void uart_ring_rx_isr_byte(uart_ring_t * r, uint8_t byte)
{
    if (r == NULL)
    {
        return;
    }
    if (!rb_push(&r->rx, byte))
    {
#if UART_RING_USE_STATS
        /* 环满丢弃：累加溢出计数。非原子自增，仅在 ISR 上下文写入，
         * 与主循环的读取清零并发见 README「溢出计数为尽力值」 */
        r->rx_overflow++;
#endif
    }
}

uint16_t uart_ring_rx_available(const uart_ring_t * r)
{
    if (r == NULL)
    {
        return 0u;
    }
    return rb_count(&r->rx);
}

uart_ring_status_t uart_ring_read_byte(uart_ring_t * r, uint8_t *byte)
{
    UART_RING_ASSERT_PTR(r, UART_RING_ERR_NULL_PTR);
    UART_RING_ASSERT_PTR(byte, UART_RING_ERR_NULL_PTR);

    if (!rb_pop(&r->rx, byte))
    {
        return UART_RING_ERR_EMPTY;
    }
    return UART_RING_OK;
}

uart_ring_status_t uart_ring_read(uart_ring_t * r, uint8_t *buf,
                                  uint16_t len, uint16_t *out)
{
    uint16_t got;

    UART_RING_ASSERT_PTR(r, UART_RING_ERR_NULL_PTR);
    UART_RING_ASSERT_PTR(buf, UART_RING_ERR_NULL_PTR);

    if (len == 0u)
    {
        return UART_RING_ERR_PARAM;
    }

    got = rb_read_bulk(&r->rx, buf, len);
    if (out != NULL)
    {
        *out = got;
    }
    return UART_RING_OK;
}

/* ========================================================================== */
/*  发送路径（主循环生产 → ISR 消费）                                          */
/* ========================================================================== */

uart_ring_status_t uart_ring_write(uart_ring_t * r, const uint8_t *data,
                                   uint16_t len, uint16_t *out)
{
    uint16_t wrote;
#if UART_RING_USE_TX_IRQ
    bool kick;
#endif

    UART_RING_ASSERT_PTR(r, UART_RING_ERR_NULL_PTR);
    UART_RING_ASSERT_PTR(data, UART_RING_ERR_NULL_PTR);

    if (len == 0u)
    {
        return UART_RING_ERR_PARAM;
    }
    if (r->tx.cap == 0u)
    {
        return UART_RING_ERR_NOT_READY;   /* init 时未配置发送环 */
    }

#if UART_RING_USE_TX_IRQ
    /* 写入前若环已空，则本次写入需要负责踢发（互补于消费侧的查空关断） */
    kick = ((r->hw != NULL) && (rb_count(&r->tx) == 0u));
#endif

    wrote = rb_write_bulk(&r->tx, data, len);
    if (out != NULL)
    {
        *out = wrote;
    }

#if UART_RING_USE_TX_IRQ
    /* 生产者运行于非 ISR 上下文，不会抢占消费侧 uart_ring_tx_isr 的
     * "查空 → 关断" 窗口，故此处使能不会与该关断交错造成丢唤醒 */
    if (kick && (wrote > 0u))
    {
        r->tx_on = 1u;
        r->hw->tx_irq_enable(r->user_ctx, true);
    }
#endif

    return UART_RING_OK;
}

uart_ring_status_t uart_ring_pop_tx_byte(uart_ring_t * r, uint8_t *byte)
{
    UART_RING_ASSERT_PTR(r, UART_RING_ERR_NULL_PTR);
    UART_RING_ASSERT_PTR(byte, UART_RING_ERR_NULL_PTR);

    if (r->tx.cap == 0u)
    {
        return UART_RING_ERR_NOT_READY;
    }
    if (!rb_pop(&r->tx, byte))
    {
        return UART_RING_ERR_EMPTY;
    }
    return UART_RING_OK;
}

#if UART_RING_USE_TX_IRQ

uart_ring_status_t uart_ring_tx_isr(uart_ring_t * r)
{
    uint8_t byte;
    bool    got;

    UART_RING_ASSERT_PTR(r, UART_RING_ERR_NULL_PTR);

    if ((r->hw == NULL) || (r->tx.cap == 0u))
    {
        return UART_RING_ERR_NOT_READY;   /* 手动模式或未配 TX 环：空操作 */
    }

    got = rb_pop(&r->tx, &byte);
    if (got)
    {
        r->hw->send_byte(r->user_ctx, byte);
    }

    /* 吐空即关断：此后由 uart_ring_write 的写空踢发重新拉起。
     * 本关断窗口对生产者（非 ISR 上下文）原子，无丢唤醒。 */
    if (rb_count(&r->tx) == 0u)
    {
        r->tx_on = 0u;
        r->hw->tx_irq_enable(r->user_ctx, false);
    }

    return got ? UART_RING_OK : UART_RING_ERR_EMPTY;
}

#endif /* UART_RING_USE_TX_IRQ */

bool uart_ring_tx_busy(const uart_ring_t * r)
{
    if ((r == NULL) || (r->tx.cap == 0u))
    {
        return false;
    }
#if UART_RING_USE_TX_IRQ
    return (rb_count(&r->tx) > 0u) || (r->tx_on != 0u);
#else
    return rb_count(&r->tx) > 0u;
#endif
}

uint16_t uart_ring_tx_free(const uart_ring_t * r)
{
    if ((r == NULL) || (r->tx.cap == 0u))
    {
        return 0u;
    }
    return rb_space(&r->tx);
}

/* ========================================================================== */
/*  统计（UART_RING_USE_STATS = 1 时存在）                                     */
/* ========================================================================== */

#if UART_RING_USE_STATS

uint16_t uart_ring_get_rx_overflow(uart_ring_t * r)
{
    uint16_t n;

    if (r == NULL)
    {
        return 0u;
    }
    n = r->rx_overflow;
    r->rx_overflow = 0u;
    return n;
}

#endif /* UART_RING_USE_STATS */
