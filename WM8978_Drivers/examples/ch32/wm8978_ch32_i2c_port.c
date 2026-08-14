/**
 * @file    wm8978_ch32_i2c_port.c
 * @brief   SDK-neutral CH32 2-wire adapter for WM8978
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "wm8978_ch32_i2c_port.h"

#include <stddef.h>

static int32_t wm8978_ch32_write_control(void * context,
                                          uint8_t first_byte,
                                          uint8_t second_byte,
                                          uint32_t timeout_ms)
{
    wm8978_ch32_i2c_adapter_t * adapter;
    uint8_t data[2];

    adapter = (wm8978_ch32_i2c_adapter_t *)context;
    data[0] = first_byte;
    data[1] = second_byte;
    return adapter->i2c_write7(adapter->board_context,
                               WM8978_I2C_ADDRESS_7BIT,
                               data,
                               2U,
                               timeout_ms);
}

static void wm8978_ch32_delay(void * context, uint32_t milliseconds)
{
    wm8978_ch32_i2c_adapter_t * adapter;

    adapter = (wm8978_ch32_i2c_adapter_t *)context;
    adapter->delay_ms(adapter->board_context, milliseconds);
}

wm8978_status_t wm8978_ch32_i2c_make_port(
    wm8978_ch32_i2c_adapter_t * adapter,
    uint32_t io_timeout_ms,
    wm8978_port_t * port)
{
    if ((adapter == NULL) || (port == NULL))
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if ((adapter->i2c_write7 == NULL) || (io_timeout_ms == 0U))
    {
        return WM8978_ERR_INVALID_ARGUMENT;
    }

    port->write_control = wm8978_ch32_write_control;
    port->delay_ms = (adapter->delay_ms != NULL) ? wm8978_ch32_delay : NULL;
    port->context = adapter;
    port->io_timeout_ms = io_timeout_ms;
    return WM8978_OK;
}
