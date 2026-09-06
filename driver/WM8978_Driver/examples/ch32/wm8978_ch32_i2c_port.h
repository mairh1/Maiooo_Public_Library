/**
 * @file    wm8978_ch32_i2c_port.h
 * @brief   WM8978 的 SDK 无关 CH32 2 线适配器契约
 *
 * 本适配器刻意不包含 WCH 器件头文件。CH32 各家族与各版本 SDK 暴露
 * 的 I2C API 不同，因此由板级层提供一个带超时保护、7 位地址的写
 * 函数和一个可选的延时函数。
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef WM8978_CH32_I2C_PORT_H
#define WM8978_CH32_I2C_PORT_H

#include <stdint.h>

#include "wm8978.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 板级提供的阻塞式 I2C 写，使用未移位的 7 位地址。
 *
 * 仅在地址 ACK、两个数据字节 ACK 与 STOP 都完成后返回 0。
 * 实现必须以 timeout_ms 约束全部等待。
 * 非零返回可能发生在部分或全部传输完成之后。核心将其视为不确定
 * 的硬件状态，要求复位 codec。
 */
typedef int32_t (*wm8978_ch32_i2c_write7_fn)(void * context,
                                             uint8_t address_7bit,
                                             const uint8_t * data,
                                             uint32_t length,
                                             uint32_t timeout_ms);

typedef void (*wm8978_ch32_delay_ms_fn)(void * context,
                                        uint32_t milliseconds);

/** @brief 调用者持有的适配器上下文；wm8978_t 使用期间须保持存活。 */
typedef struct
{
    wm8978_ch32_i2c_write7_fn i2c_write7;
    wm8978_ch32_delay_ms_fn delay_ms;
    void * board_context;
} wm8978_ch32_i2c_adapter_t;

/** @brief 把 codec 实例绑定到板级带超时保护的 I2C 操作。 */
wm8978_status_t wm8978_ch32_i2c_bind(
    wm8978_t * codec,
    wm8978_ch32_i2c_adapter_t * adapter,
    uint32_t io_timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_CH32_I2C_PORT_H */
