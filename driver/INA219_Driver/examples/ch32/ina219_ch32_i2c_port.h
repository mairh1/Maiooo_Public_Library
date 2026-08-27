/**
 * @file    ina219_ch32_i2c_port.h
 * @brief   SDK-neutral CH32 I2C adapter contract for the INA219 driver
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 *
 * @details
 * This adapter deliberately includes no WCH device header. CH32 families and
 * SDK revisions expose different I2C APIs, so the board layer supplies one
 * bounded memory-read, one memory-write and one delay function, all keyed by
 * the unshifted 7-bit address 0x40..0x4F. The adapter is passed to
 * ina219_init() as io_ctx, so multiple buses/devices can coexist.
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
 * @brief Board-owned blocking I2C memory write.
 *
 * Wire sequence must be S + addr(W) + reg + data[0..len-1] + P in one
 * transaction, MSB first. All waits bounded by timeout_ms.
 *
 * @return 0 on success; non-zero platform error otherwise.
 */
typedef int32_t (*ina219_ch32_mem_write_fn)(void *board_context,
                                            uint8_t addr7,
                                            uint8_t reg,
                                            const uint8_t *data,
                                            uint16_t len,
                                            uint32_t timeout_ms);

/**
 * @brief Board-owned blocking I2C memory read.
 *
 * Wire sequence must be S + addr(W) + reg + Sr + addr(R) + data[0..len-1]
 * + N + P in one transaction (repeated START or STOP+START both acceptable
 * for this device), MSB first.
 *
 * @return 0 on success; non-zero platform error otherwise.
 */
typedef int32_t (*ina219_ch32_mem_read_fn)(void *board_context,
                                           uint8_t addr7,
                                           uint8_t reg,
                                           uint8_t *data,
                                           uint16_t len,
                                           uint32_t timeout_ms);

/** @brief Delay at least the requested duration in thread/main context. */
typedef void (*ina219_ch32_delay_fn)(void *board_context,
                                     uint32_t milliseconds);

/** @brief Caller-owned CH32 board callbacks bound to one I2C bus. */
typedef struct
{
    void *board_context;                /**< Opaque board I2C context. */
    ina219_ch32_mem_write_fn mem_write; /**< Register write callback. */
    ina219_ch32_mem_read_fn mem_read;   /**< Register read callback. */
    ina219_ch32_delay_fn delay_ms;      /**< Required when
                                             INA219_USE_TRIGGERED = 1;
                                             NULL allowed otherwise. */
    uint32_t io_timeout_ms;             /**< Hard per-transaction bound, > 0. */
} ina219_ch32_adapter_t;

/**
 * @brief Board hook required by ina219_io_delay_ms().
 *
 * The io contract is a set of global functions without per-call context,
 * so the millisecond delay used by ina219_wait_conversion() binds to this
 * named board function. Implement it with the CH32 systick/RTOS delay that
 * never returns early. It is never called when INA219_USE_TRIGGERED = 0.
 */
void ina219_ch32_board_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* INA219_CH32_I2C_PORT_H */
