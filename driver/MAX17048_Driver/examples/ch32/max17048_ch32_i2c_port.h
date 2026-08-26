/**
 * @file    max17048_ch32_i2c_port.h
 * @brief   SDK-neutral CH32 I2C adapter contract for the MAX17048 driver
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 *
 * @details
 * This adapter deliberately includes no WCH device header. CH32 families and
 * SDK revisions expose different I2C APIs, so the board layer supplies one
 * bounded memory-read, one memory-write and one delay function, all keyed by
 * the unshifted 7-bit address 0x36. The adapter is passed to
 * max17048_init() as io_ctx, so multiple buses/devices can coexist.
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef MAX17048_CH32_I2C_PORT_H
#define MAX17048_CH32_I2C_PORT_H

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
typedef int32_t (*max17048_ch32_mem_write_fn)(void *board_context,
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
typedef int32_t (*max17048_ch32_mem_read_fn)(void *board_context,
                                             uint8_t addr7,
                                             uint8_t reg,
                                             uint8_t *data,
                                             uint16_t len,
                                             uint32_t timeout_ms);

/** @brief Delay at least the requested duration in thread/main context. */
typedef void (*max17048_ch32_delay_fn)(void *board_context,
                                       uint32_t milliseconds);

/** @brief Caller-owned CH32 board callbacks bound to one I2C bus. */
typedef struct
{
    void *board_context;            /**< Opaque board I2C context. */
    max17048_ch32_mem_write_fn mem_write; /**< Register write callback. */
    max17048_ch32_mem_read_fn mem_read;   /**< Register read callback. */
    max17048_ch32_delay_fn delay_ms;      /**< Optional; NULL allowed when
                                               the model table is unused. */
    uint32_t io_timeout_ms;         /**< Hard per-transaction bound, > 0. */
} max17048_ch32_adapter_t;

/**
 * @brief Board hook required by max17048_io_delay_ms().
 *
 * The io contract is a set of global functions without per-call context,
 * so the millisecond delay used by the model-table load sequence binds to
 * this named board function. Implement it with the CH32 systick/RTOS delay
 * that never returns early. It is never called when
 * MAX17048_USE_MODEL_TABLE = 0.
 */
void max17048_ch32_board_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* MAX17048_CH32_I2C_PORT_H */
