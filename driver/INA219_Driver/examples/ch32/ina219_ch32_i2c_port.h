/**
 * @file    ina219_ch32_i2c_port.h
 * @brief   INA219 驱动的 SDK 无关 CH32 I2C 适配器契约
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 *
 * @details
 * 本适配器刻意不包含 WCH 器件头文件。CH32 各家族与各版本 SDK 暴露
 * 的 I2C API 不同，因此由板级层提供一个带超时保护的存储器读、一个
 * 存储器写与一个延时函数，全部以未移位的 7 位地址 0x40..0x4F 为
 * 关键字。适配器作为 io_ctx 传给 ina219_init()，因此多总线/多器件
 * 可共存。
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef INA219_CH32_I2C_PORT_H
#define INA219_CH32_I2C_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 板级提供的阻塞式 I2C 存储器写。
 *
 * 线序必须是单次事务内 S + addr(W) + reg + data[0..len-1] + P，
 * MSB 在前。所有等待受 timeout_ms 约束。
 *
 * @retval 0 成功；否则为非零平台错误。
 */
typedef int32_t (*ina219_ch32_mem_write_fn)(void *board_context,
                                            uint8_t addr7,
                                            uint8_t reg,
                                            const uint8_t *data,
                                            uint16_t len,
                                            uint32_t timeout_ms);

/**
 * @brief 板级提供的阻塞式 I2C 存储器读。
 *
 * 线序必须是单次事务内 S + addr(W) + reg + Sr + addr(R) +
 * data[0..len-1] + N + P（对本器件，重复 START 与 STOP+START 均
 * 可接受），MSB 在前。
 *
 * @retval 0 成功；否则为非零平台错误。
 */
typedef int32_t (*ina219_ch32_mem_read_fn)(void *board_context,
                                           uint8_t addr7,
                                           uint8_t reg,
                                           uint8_t *data,
                                           uint16_t len,
                                           uint32_t timeout_ms);

/** @brief 在线程/主循环上下文延时至少所请求的时长。 */
typedef void (*ina219_ch32_delay_fn)(void *board_context,
                                     uint32_t milliseconds);

/** @brief 调用者持有的、绑定到一条 I2C 总线的 CH32 板级回调。 */
typedef struct
{
    void *board_context;                /**< 板级 I2C 的不透明上下文。 */
    ina219_ch32_mem_write_fn mem_write; /**< 寄存器写回调。 */
    ina219_ch32_mem_read_fn mem_read;   /**< 寄存器读回调。 */
    ina219_ch32_delay_fn delay_ms;      /**< INA219_USE_TRIGGERED = 1
                                             时必填；其它情况可填 NULL。 */
    uint32_t io_timeout_ms;             /**< 单次事务的硬性超时上限，> 0。 */
} ina219_ch32_adapter_t;

/**
 * @brief ina219_io_delay_ms() 要求的板级钩子。
 *
 * io 契约是一组无逐调用上下文的全局函数，因此
 * ina219_wait_conversion() 所用的毫秒延时绑定到这个具名板级函数。
 * 用绝不提前返回的 CH32 systick/RTOS 延时实现。INA219_USE_TRIGGERED = 0
 * 时从不调用。
 */
void ina219_ch32_board_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* INA219_CH32_I2C_PORT_H */
