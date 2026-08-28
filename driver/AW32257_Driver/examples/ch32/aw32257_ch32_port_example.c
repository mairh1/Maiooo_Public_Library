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

int32_t aw32257_io_read_reg(void * io_ctx,
                            uint8_t address_7bit,
                            uint8_t register_address,
                            uint8_t * value,
                            uint32_t timeout_ms)
{
    aw32257_ch32_port_context_t * port_context;

    port_context = (aw32257_ch32_port_context_t *)io_ctx;
    if ((port_context == NULL) ||
        (port_context->read_reg_8bit_base == NULL) ||
        (value == NULL) ||
        (address_7bit != AW32257_I2C_ADDRESS_7BIT) ||
        (timeout_ms == 0U))
    {
        return AW32257_IO_ERROR;
    }

    return port_context->read_reg_8bit_base(port_context->board_context,
                                            (uint8_t)(address_7bit << 1),
                                            register_address,
                                            value,
                                            timeout_ms);
}

int32_t aw32257_io_write_reg(void * io_ctx,
                             uint8_t address_7bit,
                             uint8_t register_address,
                             uint8_t value,
                             uint32_t timeout_ms)
{
    aw32257_ch32_port_context_t * port_context;

    port_context = (aw32257_ch32_port_context_t *)io_ctx;
    if ((port_context == NULL) ||
        (port_context->write_reg_8bit_base == NULL) ||
        (address_7bit != AW32257_I2C_ADDRESS_7BIT) ||
        (timeout_ms == 0U))
    {
        return AW32257_IO_ERROR;
    }

    return port_context->write_reg_8bit_base(port_context->board_context,
                                             (uint8_t)(address_7bit << 1),
                                             register_address,
                                             value,
                                             timeout_ms);
}

void aw32257_io_delay_ms(void * io_ctx, uint32_t milliseconds)
{
    aw32257_ch32_port_context_t * port_context;

    port_context = (aw32257_ch32_port_context_t *)io_ctx;
    if ((port_context != NULL) && (port_context->delay_ms != NULL))
    {
        port_context->delay_ms(port_context->board_context, milliseconds);
    }
}

aw32257_status_t aw32257_ch32_init(aw32257_t * device,
                                   aw32257_ch32_port_context_t * port_context,
                                   uint32_t io_timeout_ms,
                                   const aw32257_safety_config_t * safety,
                                   aw32257_device_info_t * device_info)
{
    if ((device == NULL) || (port_context == NULL))
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

    if (aw32257_init(device, port_context, io_timeout_ms) != AW32257_OK)
    {
        return AW32257_ERR_INVALID_ARGUMENT;
    }

    return aw32257_power_on_init(device, safety, device_info);
}
