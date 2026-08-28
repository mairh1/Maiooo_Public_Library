/**
 * @file    aw32257_io_template.c
 * @brief   AW32257 platform I/O port template.
 * @details Copy this file into the application and replace the bounded
 *          transaction stubs with the selected MCU SDK implementation.
 */
#include "aw32257_io.h"

int32_t aw32257_io_read_reg(void * io_ctx,
                            uint8_t address_7bit,
                            uint8_t register_address,
                            uint8_t * value,
                            uint32_t timeout_ms)
{
    (void)io_ctx;
    (void)address_7bit;
    (void)register_address;
    (void)value;
    (void)timeout_ms;
    /* Perform S + write address + register + Sr + read address + byte + P. */
    return AW32257_IO_ERROR;
}

int32_t aw32257_io_write_reg(void * io_ctx,
                             uint8_t address_7bit,
                             uint8_t register_address,
                             uint8_t value,
                             uint32_t timeout_ms)
{
    (void)io_ctx;
    (void)address_7bit;
    (void)register_address;
    (void)value;
    (void)timeout_ms;
    /* Perform S + write address + register + value + P. */
    return AW32257_IO_ERROR;
}

void aw32257_io_delay_ms(void * io_ctx, uint32_t milliseconds)
{
    (void)io_ctx;
    (void)milliseconds;
    /* Implement a non-early-returning delay in thread/main context. */
}
