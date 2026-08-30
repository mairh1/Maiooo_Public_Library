/**
 * @file    uart_ring.h
 * @brief   串口收发环形缓冲中间件
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-30
 *
 * @details
 * 为串口外设提供与硬件无关的数据收发处理层：RX / TX 双环形缓冲，
 * 接收中断灌入、主循环消费；发送侧支持"自动泵"模式（环空自关中断）。
 *
 * 核心设计：
 * - 实例 uart_ring_t 由调用者定义分配（全局 / 结构体成员 / 数组均可），
 *   缓冲存储由调用者静态提供，天然多实例、无动态内存
 * - 环形缓冲采用自由递增 uint16_t head / tail 计数器 + 2 的幂容量掩码，
 *   单生产者 / 单消费者场景（ISR ↔ 主循环）无需关中断即并发安全
 * - 硬件访问经 uart_ring_hw_t 回调注入（仅异步 TX 需要），本模块不含
 *   任何寄存器 / 厂商库符号，纯 C99 跨平台编译
 * - 异步 TX 泵、溢出统计、裸环工具面三个特性可通过 uart_ring_conf.h
 *   编译期裁剪，不使用则零代码空间
 *
 * @par 使用示例
 * @code
 * // 1. 静态分配缓冲（容量必须为 2 的幂）与实例
 * static uint8_t s_rx_mem[256];
 * static uint8_t s_tx_mem[256];
 * static uart_ring_t s_uart1;
 *
 * // 2. 对接硬件：send_byte 在 TXE 已置位时直写数据寄存器；
 * //    tx_irq_enable 开关发送空中断（上下文要求见 uart_ring_hw_t 注释）
 * static void uart1_send_byte(void * ctx, uint8_t byte)
 * {
 *     (void)ctx;
 *     USART1->DATAR = byte;
 * }
 * static void uart1_tx_irq_en(void * ctx, bool en)
 * {
 *     (void)ctx;
 *     if (en) { USART1->CTLR1 |= USART_CTLR1_TXEIE; }
 *     else    { USART1->CTLR1 &= (uint16_t)~USART_CTLR1_TXEIE; }
 * }
 * static const uart_ring_hw_t s_uart1_hw = {
 *     .send_byte     = uart1_send_byte,
 *     .tx_irq_enable = uart1_tx_irq_en,
 * };
 *
 * // 3. 初始化（uart_ring_init 不触碰硬件，外设寄存器由调用者自行配置）
 * uart_ring_init(&s_uart1, s_rx_mem, 256, s_tx_mem, 256, &s_uart1_hw, NULL);
 *
 * // 4. 中断服务程序转调（每中断一次转调一次）
 * void USART1_IRQHandler(void)
 * {
 *     if (RXNE 挂起) { uart_ring_rx_isr_byte(&s_uart1, USART1->DATAR); }
 *     if (TXE  挂起 && TXEIE 使能) { uart_ring_tx_isr(&s_uart1); }
 * }
 *
 * // 5. 主循环消费 / 投递
 * uint8_t buf[32]; uint16_t got;
 * uart_ring_read(&s_uart1, buf, sizeof(buf), &got);      // 非阻塞取数据
 * uart_ring_write(&s_uart1, msg, strlen(msg), &got);     // 入队后自动泵发送
 * @endcode
 *
 * @attention
 * - 并发模型为单生产者 / 单消费者（SPSC）：同一实例的接收路径由
 *   一个 ISR + 一个消费上下文访问，发送路径同理。多生产者并发写入
 *   同一环（如两个任务同时 uart_ring_write）需调用者自行互斥。
 * - uart_ring_init / uart_ring_reset 不得与 ISR 转调并发执行，
 *   调用顺序由用户保证（先关外设中断再 init / reset）。
 * - TX 环"空"不等于"线路空闲"：最后一个字节可能仍在移位寄存器中，
 *   需要严格发送完成时刻的工程请结合外设 TC（发送完成）中断自行判定。
 */

#ifndef UART_RING_H
#define UART_RING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "uart_ring_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*  返回值定义                                                                  */
/* ========================================================================== */

/**
 * @brief 串口环形缓冲操作返回码
 */
typedef enum {
    UART_RING_OK           =  0,   /**< 操作成功                                   */
    UART_RING_ERR_NULL_PTR = -1,   /**< 空指针错误                                 */
    UART_RING_ERR_PARAM    = -2,   /**< 参数非法（容量非 2 的幂 / 超上限 / len 非法）*/
    UART_RING_ERR_NOT_READY = -3,  /**< 实例未初始化，或 TX 路径未配置缓冲         */
    UART_RING_ERR_EMPTY    = -4    /**< 单字节读取时环内无数据                     */
} uart_ring_status_t;

/* ========================================================================== */
/*  类型定义                                                                   */
/* ========================================================================== */

/**
 * @brief 字节环形缓冲（生产者写 head、消费者写 tail，SPSC 无锁）
 *
 * 计数器自由递增、自然回绕，有效下标 = 计数器 & mask，
 * 数据量 = (uint16_t)(head - tail)，因此无"满 / 空歧义"，
 * 容量 cap 字节全部可用（不浪费一格）。
 *
 * @note UART_RING_USE_RB_API = 0 时本类型仍作为 uart_ring_t 内部字段
 *       存在，但 uart_rb_* 工具函数不对外公开。
 */
typedef struct {
    uint8_t *buf;                  /**< 环存储（调用者静态分配，容量 cap）      */
    uint16_t cap;                  /**< 容量（字节，2 的幂；0 = 该环未启用）    */
    uint16_t mask;                 /**< 下标掩码（cap - 1）                     */
    volatile uint16_t head;        /**< 写计数器（仅生产者修改）                */
    volatile uint16_t tail;        /**< 读计数器（仅消费者修改）                */
} uart_rb_t;

#if UART_RING_USE_TX_IRQ

/**
 * @brief 发送路径硬件回调契约（UART_RING_USE_TX_IRQ = 1 时存在）
 *
 * 实例保存其 const 指针，多实例可共享同一份回调表。
 */
typedef struct {
    /**
     * @brief 向硬件写一个字节
     *
     * 契约：调用时 TXE（发送数据寄存器空）已确认置位，实现直接写数据
     *       寄存器即可，不得在内部等待 TXE。
     * 上下文：仅由 uart_ring_tx_isr() 间接调用，即 ISR 上下文。
     */
    void (*send_byte)(void *ctx, uint8_t byte);

    /**
     * @brief 开启 / 关闭发送空中断（TXE 中断使能位）
     *
     * 契约：enable=true 使能 TXE 中断，false 屏蔽；重复设置同值无害。
     *       上下文：ISR（经 uart_ring_tx_isr 关断路径）与线程
     *       （uart_ring_write 踢发路径）均会调用，实现必须两侧安全
     *       （对寄存器位操作天然满足）。
     */
    void (*tx_irq_enable)(void *ctx, bool enable);
} uart_ring_hw_t;

#endif /* UART_RING_USE_TX_IRQ */

/**
 * @brief 串口收发实例（调用者定义，一个实例对应一个串口外设）
 */
typedef struct {
    uart_rb_t rx;                  /**< 接收环：ISR 生产，主循环消费            */
    uart_rb_t tx;                  /**< 发送环：主循环生产，ISR 消费            */
#if UART_RING_USE_STATS
    volatile uint16_t rx_overflow; /**< RX 环满丢弃字节累计计数                 */
#endif
#if UART_RING_USE_TX_IRQ
    volatile uint8_t tx_on;        /**< TX 泵运行中（TXE 中断已由本模块使能）   */
    const uart_ring_hw_t *hw;      /**< 硬件回调表（NULL = 退化为手动取字节）   */
    void *user_ctx;                /**< 透传给 hw 回调的上下文（外设句柄等）    */
#endif
} uart_ring_t;

/* ========================================================================== */
/*  裸环形缓冲工具 API（UART_RING_USE_RB_API = 1 时公开，可独立复用）           */
/* ========================================================================== */

#if UART_RING_USE_RB_API

/**
 * @brief 初始化裸环形缓冲
 * @param[out] rb       环对象
 * @param[in]  buf      调用者分配的存储，长度 cap
 * @param[in]  cap      容量，必须为 2 的幂且 ≤ UART_RING_MAX_CAPACITY
 * @retval UART_RING_OK      成功
 * @retval UART_RING_ERR_NULL_PTR rb / buf 为空
 * @retval UART_RING_ERR_PARAM    cap 非法
 */
uart_ring_status_t uart_rb_init(uart_rb_t * rb, uint8_t *buf, uint16_t cap);

/**
 * @brief 写入一个字节（生产者侧）
 * @param[in,out] rb   环对象
 * @param[in]     byte 待写入字节
 * @retval true  成功；false 环满已丢弃
 */
bool uart_rb_push(uart_rb_t * rb, uint8_t byte);

/**
 * @brief 弹出一个字节（消费者侧）
 * @param[in,out] rb   环对象
 * @param[out]    byte 读出字节存放地址
 * @retval true  成功；false 环空
 */
bool uart_rb_pop(uart_rb_t * rb, uint8_t *byte);

/**
 * @brief 批量写入（逐字节，非原子）
 * @param[in,out] rb   环对象
 * @param[in]     data 源数据
 * @param[in]     len  期望写入字节数
 * @return 实际写入字节数（环满即停止）
 */
uint16_t uart_rb_write(uart_rb_t * rb, const uint8_t *data, uint16_t len);

/**
 * @brief 批量读出（逐字节，非原子）
 * @param[in,out] rb   环对象
 * @param[out]    buf  目标缓冲
 * @param[in]     len  期望读出字节数
 * @return 实际读出字节数（环空即停止）
 */
uint16_t uart_rb_read(uart_rb_t * rb, uint8_t *buf, uint16_t len);

/**
 * @brief 查询环内可读字节数
 */
uint16_t uart_rb_count(const uart_rb_t * rb);

/**
 * @brief 查询环内可写字节数
 */
uint16_t uart_rb_space(const uart_rb_t * rb);

/**
 * @brief 复位环（清空数据）。仅限无任何并发访问的静默期调用。
 */
void uart_rb_reset(uart_rb_t * rb);

#endif /* UART_RING_USE_RB_API */

/* ========================================================================== */
/*  串口实例生命周期                                                            */
/* ========================================================================== */

#if UART_RING_USE_TX_IRQ

/**
 * @brief 初始化串口收发实例（不触碰硬件寄存器）
 * @param[out] r       实例
 * @param[in]  rx_buf  接收环存储，容量 rx_cap（接收路径必配，不得为 NULL）
 * @param[in]  rx_cap  接收容量，2 的幂，2 ~ UART_RING_MAX_CAPACITY
 * @param[in]  tx_buf  发送环存储；不需要发送路径时传 NULL 且 tx_cap 传 0
 * @param[in]  tx_cap  发送容量，2 的幂，2 ~ UART_RING_MAX_CAPACITY
 * @param[in]  hw      硬件回调表；传 NULL 则退化为手动 TXE 取字节模式
 * @param[in]  ctx     透传给 hw 回调的上下文（外设句柄），可为 NULL
 * @retval UART_RING_OK         成功
 * @retval UART_RING_ERR_NULL_PTR  r / rx_buf 为空
 * @retval UART_RING_ERR_PARAM     容量非法（非 2 的幂 / 超上限 / tx_buf 与 tx_cap 不匹配）
 * @note 调用前请先关闭对应外设中断，避免半初始化状态被 ISR 访问。
 */
uart_ring_status_t uart_ring_init(uart_ring_t * r,
                                  uint8_t *rx_buf, uint16_t rx_cap,
                                  uint8_t *tx_buf, uint16_t tx_cap,
                                  const uart_ring_hw_t *hw, void *ctx);

#else /* !UART_RING_USE_TX_IRQ */

/**
 * @brief 初始化串口收发实例（不触碰硬件寄存器；UART_RING_USE_TX_IRQ=0 形态）
 * @param[out] r      实例
 * @param[in]  rx_buf 接收环存储，容量 rx_cap（接收路径必配，不得为 NULL）
 * @param[in]  rx_cap 接收容量，2 的幂，2 ~ UART_RING_MAX_CAPACITY
 * @param[in]  tx_buf 发送环存储；不需要发送路径时传 NULL 且 tx_cap 传 0
 * @param[in]  tx_cap 发送容量，2 的幂，2 ~ UART_RING_MAX_CAPACITY
 * @retval UART_RING_OK         成功
 * @retval UART_RING_ERR_NULL_PTR  r / rx_buf 为空
 * @retval UART_RING_ERR_PARAM     容量非法
 * @note 调用前请先关闭对应外设中断，避免半初始化状态被 ISR 访问。
 */
uart_ring_status_t uart_ring_init(uart_ring_t * r,
                                  uint8_t *rx_buf, uint16_t rx_cap,
                                  uint8_t *tx_buf, uint16_t tx_cap);

#endif /* UART_RING_USE_TX_IRQ */

/**
 * @brief 清空实例收发缓冲（结构字段保留，重新可用）
 *
 * 仅限静默期调用：先关外设中断，再 reset，之后再 init 或开中断。
 */
void uart_ring_reset(uart_ring_t * r);

/* ========================================================================== */
/*  接收路径（ISR 生产 → 主循环消费）                                           */
/* ========================================================================== */

/**
 * @brief 接收中断转调入口：把硬件读出的一字节灌入 RX 环
 * @param[in,out] r    实例
 * @param[in]     byte 从数据寄存器读出的字节
 * @note ISR 上下文调用；环满则丢弃新字节（统计开启时累加 rx_overflow），
 *       内部无锁、无阻塞、无延时，耗时恒定。
 */
void uart_ring_rx_isr_byte(uart_ring_t * r, uint8_t byte);

/**
 * @brief 查询当前可读字节数（非破坏）
 */
uint16_t uart_ring_rx_available(const uart_ring_t * r);

/**
 * @brief 读取一个字节
 * @retval UART_RING_OK        成功
 * @retval UART_RING_ERR_EMPTY 环内暂无数据
 * @retval UART_RING_ERR_NULL_PTR / ERR_NOT_READY
 */
uart_ring_status_t uart_ring_read_byte(uart_ring_t * r, uint8_t *byte);

/**
 * @brief 批量读取（非阻塞，读到环空或读满为止）
 * @param[in,out] r   实例
 * @param[out]    buf 目标缓冲
 * @param[in]     len buf 容量
 * @param[out]   out 实际读出字节数（可传 NULL 忽略）
 * @retval UART_RING_OK           成功（含部分读出，实际量见 *out）
 * @retval UART_RING_ERR_NULL_PTR r / buf 为空
 * @retval UART_RING_ERR_PARAM    len 为 0
 */
uart_ring_status_t uart_ring_read(uart_ring_t * r, uint8_t *buf,
                                  uint16_t len, uint16_t *out);

/* ========================================================================== */
/*  发送路径（主循环生产 → ISR 消费）                                           */
/* ========================================================================== */

/**
 * @brief 批量写入 TX 环（非阻塞）
 * @param[in,out] r   实例
 * @param[in]     data 待发送数据
 * @param[in]     len  期望写入字节数
 * @param[out]   out  实际写入字节数（可传 NULL 忽略；环满即部分写入）
 * @retval UART_RING_OK           成功（含部分写入，实际量见 *out）
 * @retval UART_RING_ERR_NULL_PTR r / data 为空
 * @retval UART_RING_ERR_PARAM    len 为 0
 * @retval UART_RING_ERR_NOT_READY TX 环未配置（init 时 tx_cap 传了 0）
 * @note 线程上下文调用（与自身互斥见 @attention）；UART_RING_USE_TX_IRQ=1
 *       且注册了 hw 回调时，写入后自动踢发（开启 TXE 中断）。
 */
uart_ring_status_t uart_ring_write(uart_ring_t * r, const uint8_t *data,
                                   uint16_t len, uint16_t *out);

/**
 * @brief 从 TX 环弹出一个待发送字节
 *
 * UART_RING_USE_TX_IRQ=0 或 hw 未注册时，由用户在自己的 TXE 中断里
 * 调用本函数取字节写硬件，环空即代表缓冲已发完。
 * @retval UART_RING_OK        成功
 * @retval UART_RING_ERR_EMPTY 环空
 * @note ISR 上下文安全。
 */
uart_ring_status_t uart_ring_pop_tx_byte(uart_ring_t * r, uint8_t *byte);

#if UART_RING_USE_TX_IRQ

/**
 * @brief 发送空中断转调入口：弹出 TX 环字节经 hw.send_byte 写入硬件
 * @param[in,out] r 实例（须已注册 hw 回调；hw 为 NULL 时行为为空操作）
 * @retval UART_RING_OK       已发送一个字节
 * @retval UART_RING_ERR_EMPTY 环内无待发字节（仅完成关断检查）
 * @retval UART_RING_ERR_NOT_READY 未注册 hw 回调（空操作）
 * @note ISR 上下文调用；TX 环吐空时自动关闭 TXE 中断并在"新数据恰于
 *       关断瞬间入队"的竞态下重新拉回（见 uart_ring_write 踢发条件）。
 */
uart_ring_status_t uart_ring_tx_isr(uart_ring_t * r);

#endif /* UART_RING_USE_TX_IRQ */

/**
 * @brief TX 环是否仍有待发数据（含泵运行中）
 * @note 环空 ≠ 线路空闲，最后字节可能仍在移位寄存器，见文件头 @attention。
 */
bool uart_ring_tx_busy(const uart_ring_t * r);

/**
 * @brief 查询 TX 环当前可写字节数
 */
uint16_t uart_ring_tx_free(const uart_ring_t * r);

/* ========================================================================== */
/*  统计（UART_RING_USE_STATS = 1 时存在）                                      */
/* ========================================================================== */

#if UART_RING_USE_STATS

/**
 * @brief 读取并清零 RX 溢出丢弃计数
 *
 * 自上次读取以来因环满被丢弃的接收字节累计数。
 */
uint16_t uart_ring_get_rx_overflow(uart_ring_t * r);

#endif /* UART_RING_USE_STATS */

#ifdef __cplusplus
}
#endif

#endif /* UART_RING_H */
