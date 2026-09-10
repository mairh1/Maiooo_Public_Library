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

/** @brief 经具体 CH32 硬件 I2C BSP 写一个寄存器。
 *
 * @param[in]  board_context    调用者持有的 CH32 BSP 上下文。
 * @param[in]  address_8bit_base AW32257 的基地址 0xD4，含义同读回调。
 * @param[in]  register_address AW32257 寄存器地址。
 * @param[in]  value            待写入的字节。
 * @param[in]  timeout_ms       整个事务的硬性超时上限。
 *
 * @retval 0 成功；否则为 BSP 自定义的负值或正值错误。
 */
typedef int32_t (*aw32257_ch32_write_reg_fn)(void * board_context,
                                             uint8_t address_8bit_base,
                                             uint8_t register_address,
                                             uint8_t value,
                                             uint32_t timeout_ms);

/** @brief 在线程/主循环上下文延时至少所请求的时长。
 *
 * @param[in] board_context 调用者持有的 CH32 BSP 上下文。
 * @param[in] milliseconds  请求的最短延时，单位毫秒。
 */
typedef void (*aw32257_ch32_delay_ms_fn)(void * board_context,
                                         uint32_t milliseconds);

/** @brief 桥接层使用的、调用者持有的 CH32 BSP 回调。 */
typedef struct
{
    void * board_context;                        /**< 透传给各回调的板级上下文 */
    aw32257_ch32_read_reg_fn read_reg_8bit_base;   /**< BSP 寄存器读回调 */
    aw32257_ch32_write_reg_fn write_reg_8bit_base; /**< BSP 寄存器写回调 */
    aw32257_ch32_delay_ms_fn delay_ms;             /**< BSP 毫秒延时回调 */
} aw32257_ch32_port_context_t;

/**
 * @brief   用该 CH32 上下文初始化一个 AW32257 实例。
 *
 * 应用必须提供 aw32257_io.h 声明的固定 aw32257_io_* 函数；本辅助
 * 函数只保存不透明上下文与超时，随后执行 POR 安全初始化。
 *
 * @param[out] device        调用者持有的驱动实例。
 * @param[in]  port_context  已填好 BSP 回调的桥接上下文。
 * @param[in]  io_timeout_ms 传给每笔 I/O 事务的超时上限，必须非零。
 * @param[in]  safety        产品专属的 POR 专用电流电压限值。
 * @param[out] device_info   可选的解码身份结果，允许为 NULL。
 *
 * @retval  AW32257_OK 初始化成功。
 * @retval  AW32257_ERR_NULL_POINTER @p device 或 @p port_context 为 NULL。
 * @retval  AW32257_ERR_INVALID_ARGUMENT 回调缺失或 @p io_timeout_ms 为 0。
 * @retval  AW32257_ERR_* aw32257_power_on_init() 失败时原样上抛。
 */
aw32257_status_t
aw32257_ch32_init(aw32257_t * device,
                  aw32257_ch32_port_context_t * port_context,
                  uint32_t io_timeout_ms,
                  const aw32257_safety_config_t * safety,
                  aw32257_device_info_t * device_info);


#ifdef __cplusplus
}
#endif

#endif /* AW32257_CH32_PORT_EXAMPLE_H */
