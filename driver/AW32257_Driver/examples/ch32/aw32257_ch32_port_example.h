/**
 * @file    aw32257_ch32_port_example.h
 * @brief   SDK-neutral CH32 context example for the AW32257 driver
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * @details
 * This file deliberately contains no CH32/WCH device header. A concrete BSP
 * supplies the three bounded hardware callbacks below for its selected CH32,
 * SDK, I2C instance, pins, clock tree, and scheduler.
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
 * @brief Read one register through a concrete CH32 hardware-I2C BSP.
 *
 * @param[in]  board_context Caller-owned CH32 BSP context.
 * @param[in]  address_8bit_base Address 0xD4 for AW32257. For WCH APIs that
 *              take a direction separately, pass this value together with
 *              the receiver direction. A byte-oriented implementation sets
 *              bit 0 only for the read-address phase, producing 0xD5.
 * @param[in]  register_address AW32257 register address.
 * @param[out] value Byte read from the device.
 * @param[in]  timeout_ms Hard upper bound for the complete transaction.
 *
 * @return 0 on success; otherwise a BSP-defined negative or positive error.
 */
typedef int32_t (*aw32257_ch32_read_reg_fn)(void * board_context,
                                            uint8_t address_8bit_base,
                                            uint8_t register_address,
                                            uint8_t * value,
                                            uint32_t timeout_ms);

/** @brief Write one register through a concrete CH32 hardware-I2C BSP. */
typedef int32_t (*aw32257_ch32_write_reg_fn)(void * board_context,
                                             uint8_t address_8bit_base,
                                             uint8_t register_address,
                                             uint8_t value,
                                             uint32_t timeout_ms);

/** @brief Delay for at least the requested duration in thread/main context. */
typedef void (*aw32257_ch32_delay_ms_fn)(void * board_context,
                                         uint32_t milliseconds);

/** @brief Caller-owned CH32 BSP callbacks used by the bridge. */
typedef struct
{
    void * board_context;
    aw32257_ch32_read_reg_fn read_reg_8bit_base;
    aw32257_ch32_write_reg_fn write_reg_8bit_base;
    aw32257_ch32_delay_ms_fn delay_ms;
} aw32257_ch32_port_context_t;

/**
 * @brief Initialize an AW32257 instance with this CH32 context.
 *
 * The application must provide the fixed aw32257_io_* functions declared by
 * aw32257_io.h; this helper only stores the opaque context and timeout.
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
