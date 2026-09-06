/**
 * @file    aw32257_ch32_port_example.h
 * @brief   AW32257 驱动的 SDK 无关 CH32 上下文示例
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * @details
 * 本文件刻意不包含任何 CH32/WCH 器件头文件。由具体 BSP 为其选定的
 * CH32 型号、SDK、I2C 实例、引脚、时钟树与调度器实现下列三个带
 * 超时保护的硬件回调。
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef AW32257_CH32_PORT_EXAMPLE_H
#define AW32257_CH32_PORT_EXAMPLE_H

#include <stdint.h>

#include "aw32257.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 经具体 CH32 硬件 I2C BSP 读一个寄存器。
 *
 * @param[in]  board_context 调用者持有的 CH32 BSP 上下文。
 * @param[in]  address_8bit_base AW32257 的基地址 0xD4。对方向位单独
 *              传入的 WCH API，将该值与接收方向一起传入；按字节
 *              组织的实现仅在读地址相位置位 bit 0，得到 0xD5。
 * @param[in]  register_address AW32257 寄存器地址。
 * @param[out] value 从器件读到的字节。
 * @param[in]  timeout_ms 整个事务的硬性超时上限。
 *
 * @retval 0 成功；否则为 BSP 自定义的负值或正值错误。
 */
typedef int32_t (*aw32257_ch32_read_reg_fn)(void * board_context,
                                            uint8_t address_8bit_base,
                                            uint8_t register_address,
                                            uint8_t * value,
                                            uint32_t timeout_ms);

/** @brief 经具体 CH32 硬件 I2C BSP 写一个寄存器。 */
typedef int32_t (*aw32257_ch32_write_reg_fn)(void * board_context,
                                             uint8_t address_8bit_base,
                                             uint8_t register_address,
                                             uint8_t value,
                                             uint32_t timeout_ms);

/** @brief 在线程/主循环上下文延时至少所请求的时长。 */
typedef void (*aw32257_ch32_delay_ms_fn)(void * board_context,
                                         uint32_t milliseconds);

/** @brief 桥接层使用的、调用者持有的 CH32 BSP 回调。 */
typedef struct
{
    void * board_context;
    aw32257_ch32_read_reg_fn read_reg_8bit_base;
    aw32257_ch32_write_reg_fn write_reg_8bit_base;
    aw32257_ch32_delay_ms_fn delay_ms;
} aw32257_ch32_port_context_t;

/**
 * @brief 用该 CH32 上下文初始化一个 AW32257 实例。
 *
 * 应用必须提供 aw32257_io.h 声明的固定 aw32257_io_* 函数；本辅助
 * 函数只保存不透明上下文与超时。
 */
aw32257_status_t aw32257_ch32_init(aw32257_t * device,
                                   aw32257_ch32_port_context_t * port_context,
                                   uint32_t io_timeout_ms,
                                   const aw32257_safety_config_t * safety,
                                   aw32257_device_info_t * device_info);


#ifdef __cplusplus
}
#endif

#endif /* AW32257_CH32_PORT_EXAMPLE_H */
