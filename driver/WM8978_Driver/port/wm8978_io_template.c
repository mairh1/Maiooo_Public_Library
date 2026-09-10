/**
 * @file    wm8978_io_template.c
 * @brief   WM8978 固定移植契约模板
 * @details 把本文件复制进板级工程并改名为 wm8978_io.c，用经平台
 *          验证的 I2C/GPIO 与延时调用替换这些桩实现。
 *          控制帧为两个字节；核心提供未移位的 WM8978 帧，物理
 *          2 线/3 线总线由板级负责。
 * @note    本文件只依赖 wm8978_io.h 与平台外设驱动；两个桩实现
 *          在移植完成前必须保持返回错误/空操作，避免误用。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "wm8978_io.h"

int32_t wm8978_io_write_control(void *io_ctx,
                                 uint8_t first_byte,
                                 uint8_t second_byte,
                                 uint32_t timeout_ms)
{
    (void)io_ctx;
    (void)first_byte;
    (void)second_byte;
    (void)timeout_ms;

    /*
     * 2 线伪代码：
     *   i2c_write(0x1A, { first_byte, second_byte }, 2, timeout_ms);
     * 仅在两个数据字节均 ACK 且 STOP 完成后返回 WM8978_IO_OK。
     * 3 线模式下，CSB 有效期间按 MSB 在前移出两个字节，并在 CSB
     * 上升沿锁存。所有等待都必须受 timeout_ms 约束。
     */
    return WM8978_IO_ERROR; /* 桩：尚未移植。 */
}

void wm8978_io_delay_ms(void *io_ctx, uint32_t milliseconds)
{
    (void)io_ctx;
    (void)milliseconds;

    /* 替换为绝不短于 milliseconds 的板级延时。 */
}
