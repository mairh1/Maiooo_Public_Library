/**
 * @file    wm8978_ch32_i2c_port.h
 * @brief   SDK-neutral CH32 2-wire adapter contract for WM8978
 *
 * This adapter deliberately includes no WCH device header. CH32 families and
 * SDK revisions expose different I2C APIs, so the board layer supplies one
 * bounded 7-bit-address write function and one optional delay function.
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
 * @brief Board-owned blocking I2C write using an unshifted 7-bit address.
 *
 * Return 0 only after address ACK, both data-byte ACKs, and STOP completion.
 * The implementation must bound all waits by timeout_ms.
 * A non-zero return may occur after partial or complete transmission. The
 * core treats it as uncertain hardware state and requires codec reset.
 */
typedef int32_t (*wm8978_ch32_i2c_write7_fn)(void * context,
                                             uint8_t address_7bit,
                                             const uint8_t * data,
                                             uint32_t length,
                                             uint32_t timeout_ms);

typedef void (*wm8978_ch32_delay_ms_fn)(void * context,
                                        uint32_t milliseconds);

/** @brief Caller-owned adapter context; keep it alive while wm8978_t uses it. */
typedef struct
{
    wm8978_ch32_i2c_write7_fn i2c_write7;
    wm8978_ch32_delay_ms_fn delay_ms;
    void * board_context;
} wm8978_ch32_i2c_adapter_t;

/** @brief Bind a codec instance to the board's bounded I2C operation. */
wm8978_status_t wm8978_ch32_i2c_bind(
    wm8978_t * codec,
    wm8978_ch32_i2c_adapter_t * adapter,
    uint32_t io_timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_CH32_I2C_PORT_H */
