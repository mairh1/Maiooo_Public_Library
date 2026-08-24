/**
 * @file    aw32257_ch32_port_example.c
 * @brief   SDK-neutral CH32 BSP bridge example for the AW32257 driver
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "aw32257_ch32_port_example.h"

#include <stddef.h>

static int32_t aw32257_ch32_read_bridge(void * context,
                                         uint8_t address_7bit,
                                         uint8_t register_address,
                                         uint8_t * value,
                                         uint32_t timeout_ms)
{
    aw32257_ch32_port_context_t * port_context;
    uint8_t address_8bit_base;

    port_context = (aw32257_ch32_port_context_t *)context;
    if ((port_context == NULL) ||
        (port_context->read_reg_8bit_base == NULL) ||
        (value == NULL) ||
        (address_7bit != AW32257_I2C_ADDRESS_7BIT) ||
        (timeout_ms == 0U))
    {
        return -1;
    }

    address_8bit_base = (uint8_t)(address_7bit << 1);
    return port_context->read_reg_8bit_base(port_context->board_context,
                                             address_8bit_base,
                                             register_address,
                                             value,
                                             timeout_ms);
}

static int32_t aw32257_ch32_write_bridge(void * context,
                                          uint8_t address_7bit,
                                          uint8_t register_address,
                                          uint8_t value,
                                          uint32_t timeout_ms)
{
    aw32257_ch32_port_context_t * port_context;
    uint8_t address_8bit_base;

    port_context = (aw32257_ch32_port_context_t *)context;
    if ((port_context == NULL) ||
        (port_context->write_reg_8bit_base == NULL) ||
        (address_7bit != AW32257_I2C_ADDRESS_7BIT) ||
        (timeout_ms == 0U))
    {
        return -1;
    }

    address_8bit_base = (uint8_t)(address_7bit << 1);
    return port_context->write_reg_8bit_base(port_context->board_context,
                                              address_8bit_base,
                                              register_address,
                                              value,
                                              timeout_ms);
}

static void aw32257_ch32_delay_bridge(void * context, uint32_t milliseconds)
{
    aw32257_ch32_port_context_t * port_context;

    port_context = (aw32257_ch32_port_context_t *)context;
    if ((port_context != NULL) && (port_context->delay_ms != NULL))
    {
        port_context->delay_ms(port_context->board_context, milliseconds);
    }
}

aw32257_status_t aw32257_ch32_make_port(
    aw32257_port_t * port,
    aw32257_ch32_port_context_t * port_context,
    uint32_t io_timeout_ms)
{
    if ((port == NULL) || (port_context == NULL))
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if ((port_context->read_reg_8bit_base == NULL) ||
        (port_context->write_reg_8bit_base == NULL) ||
        (port_context->delay_ms == NULL) ||
        (io_timeout_ms == 0U))
    {
        return AW32257_ERR_INVALID_ARGUMENT;
    }

    port->read_reg = aw32257_ch32_read_bridge;
    port->write_reg = aw32257_ch32_write_bridge;
    port->delay_ms = aw32257_ch32_delay_bridge;
    port->context = port_context;
    port->io_timeout_ms = io_timeout_ms;

    return AW32257_OK;
}

aw32257_status_t aw32257_ch32_bind_and_power_on_init(
    aw32257_t * device,
    aw32257_ch32_port_context_t * port_context,
    uint32_t io_timeout_ms,
    const aw32257_safety_config_t * safety,
    aw32257_device_info_t * device_info)
{
    aw32257_port_t port;
    aw32257_status_t status;

    status = aw32257_ch32_make_port(&port, port_context, io_timeout_ms);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_bind(device, &port);
    if (status != AW32257_OK)
    {
        return status;
    }

    return aw32257_power_on_init(device, safety, device_info);
}
