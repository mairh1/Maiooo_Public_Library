/**
 * @file    uart_ring_conf.h
 * @brief   串口收发环形缓冲中间件配置
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-30
 *
 * @details
 * 全部配置宏集中在本文件，均带 #ifndef 默认值，可通过编译器命令行
 * -D 覆盖，禁止修改模块源码来配置。
 *
 * 功能开关置 0 后，对应的公共 API、结构体字段与核心实现一起被 #if
 * 裁剪，被裁剪的 API 不再存在（误用时编译期报错），不占用任何
 * Flash / RAM 空间。
 */

#ifndef UART_RING_CONF_H
#define UART_RING_CONF_H

/* ========================================================================== */
/*  功能开关（1 = 启用，0 = 编译期裁剪）                                        */
/* ========================================================================== */

/**
 * @brief 异步发送自动泵开关
 *
 * 置 1：提供 uart_ring_hw_t 硬件回调契约、uart_ring_tx_isr() 中断转调入口
 *       及实例内 tx_on 状态字段。uart_ring_write() 在 TX 环由空转非空时
 *       自动开启发送中断，uart_ring_tx_isr() 吐完环内数据后自动关闭。
 * 置 0：发送退化为纯缓冲——uart_ring_write() 仅入环，用户自行在 TXE 中断
 *       里调用 uart_ring_pop_tx_byte() 取字节发送（该函数不受本开关影响）。
 */
#ifndef UART_RING_USE_TX_IRQ
#define UART_RING_USE_TX_IRQ    1
#endif

#if (UART_RING_USE_TX_IRQ != 0) && (UART_RING_USE_TX_IRQ != 1)
#error "UART_RING_USE_TX_IRQ 必须为 0 或 1"
#endif

/**
 * @brief 接收溢出统计开关
 *
 * 置 1：RX 环满时丢弃新字节并累加 uart_ring_t::rx_overflow，
 *       提供 uart_ring_get_rx_overflow() 读取并清零。
 * 置 0：环满仅静默丢弃，剔除计数字段与查询 API（每实例省 2 字节 RAM）。
 */
#ifndef UART_RING_USE_STATS
#define UART_RING_USE_STATS     1
#endif

#if (UART_RING_USE_STATS != 0) && (UART_RING_USE_STATS != 1)
#error "UART_RING_USE_STATS 必须为 0 或 1"
#endif

/**
 * @brief 裸环形缓冲工具面开关
 *
 * 置 1：uart_rb_t 类型与全部 uart_rb_* 函数对外公开，可脱离串口实例
 *       单独用于其它按字节搬运的缓冲场景。
 * 置 0：uart_rb_* 仅供本模块内部使用（降级为 .c 内静态实现），
 *       对外只保留 uart_ring_* 串口语义 API。
 * 注意：无论本开关如何，uart_ring_t 实例内部都内嵌 rx / tx 两个环。
 */
#ifndef UART_RING_USE_RB_API
#define UART_RING_USE_RB_API    1
#endif

#if (UART_RING_USE_RB_API != 0) && (UART_RING_USE_RB_API != 1)
#error "UART_RING_USE_RB_API 必须为 0 或 1"
#endif

/* ========================================================================== */
/*  容量约束                                                                   */
/* ========================================================================== */

/**
 * @brief 单个环形缓冲的最大容量（字节）
 *
 * 必须 ≤ 32768 且为 2 的幂。上限受 uint16_t head/tail 与指针位宽约束；
 * uart_ring_init() 运行时校验实际容量不得超逾此值。
 */
#ifndef UART_RING_MAX_CAPACITY
#define UART_RING_MAX_CAPACITY  4096u
#endif

#if (UART_RING_MAX_CAPACITY < 2) || (UART_RING_MAX_CAPACITY > 32768u)
#error "UART_RING_MAX_CAPACITY 必须在 [2, 32768] 范围内"
#endif
#if (UART_RING_MAX_CAPACITY & (UART_RING_MAX_CAPACITY - 1)) != 0
#error "UART_RING_MAX_CAPACITY 必须为 2 的幂"
#endif

#endif /* UART_RING_CONF_H */
