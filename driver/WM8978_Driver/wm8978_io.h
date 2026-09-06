/*
 * @file    wm8978_io.h
 * @brief   WM8978 驱动移植契约
 * @details 核心仅通过下列固定函数访问硬件。在应用/BSP 中实现它们，
 *          模板见 port/wm8978_io_template.c。
 *          本头文件不得 include wm8978.h 或厂商头文件。
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef WM8978_IO_H
#define WM8978_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WM8978_IO_OK       0
#define WM8978_IO_ERROR   (-1)

/**
 * @brief 写入一个完整的、已打包的 WM8978 控制帧。
 * @param io_ctx 调用者持有的总线上下文，由 wm8978_bind() 原样传入。
 * @param first_byte 控制位 B15:B8。
 * @param second_byte 控制位 B7:B0。
 * @param timeout_ms 有限的事务超时，非零。
 * @retval WM8978_IO_OK 仅在地址/数据均 ACK 且 STOP/锁存完成后返回；
 *         超时、NACK 或其它总线失败返回 WM8978_IO_ERROR。
 * @note 失败时硬件副作用不确定。核心进入 DESYNCHRONIZED，复位前
 *       不得重试。每个 WM8978 实例都必须实现本函数。
 */
int32_t wm8978_io_write_control(void *io_ctx,
                                 uint8_t first_byte,
                                 uint8_t second_byte,
                                 uint32_t timeout_ms);

/**
 * @brief 延时至少所请求的毫秒数。
 * @param io_ctx 调用者持有的板级上下文。
 * @param milliseconds 请求的延时。
 * @note 上电类 API 需要；仅当应用从不调用需要延时的 API 时才可以
 *       实现为空函数。在线程上下文调用；可能阻塞，非 ISR 安全。
 */
void wm8978_io_delay_ms(void *io_ctx, uint32_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_IO_H */
