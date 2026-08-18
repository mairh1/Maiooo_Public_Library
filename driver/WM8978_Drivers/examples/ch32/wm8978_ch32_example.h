/**
 * @file    wm8978_ch32_example.h
 * @brief   Declaration for the explicit CH32 WM8978 initialization example
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef WM8978_CH32_EXAMPLE_H
#define WM8978_CH32_EXAMPLE_H

#include "wm8978_ch32_i2c_port.h"

#ifdef __cplusplus
extern "C" {
#endif

wm8978_status_t wm8978_ch32_example_init_i2s16_slave(
    wm8978_t * codec,
    wm8978_ch32_i2c_adapter_t * adapter,
    uint32_t io_timeout_ms,
    uint32_t board_vmid_settle_ms);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_CH32_EXAMPLE_H */
