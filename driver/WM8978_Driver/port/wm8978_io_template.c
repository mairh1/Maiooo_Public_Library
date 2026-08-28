/*
 * @file    wm8978_io_template.c
 * @brief   WM8978 fixed porting-contract template
 * @details Copy this file into the board project as wm8978_io.c and replace
 *          the stubs with the validated platform I2C/GPIO and delay calls.
 *          The control frame is two bytes; the core supplies the unshifted
 *          WM8978 frame and the board owns the physical 2-wire/3-wire bus.
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "wm8978_io.h"

int32_t wm8978_io_write_control(void *io_ctx,
                                 uint8_t first_byte,
                                 uint8_t second_byte,
                                 uint32_t timeout_ms)
{
    (void)io_ctx;
    (void)first_byte;
    (void)second_byte;
    (void)timeout_ms;

    /*
     * 2-wire pseudo-code:
     *   i2c_write(0x1A, { first_byte, second_byte }, 2, timeout_ms);
     * Return WM8978_IO_OK only after both data ACKs and STOP complete.
     * For 3-wire, shift both bytes MSB first while CSB is active and latch
     * them at CSB rising edge. Every wait must be bounded by timeout_ms.
     */
    return WM8978_IO_ERROR; /* Stub: not yet ported. */
}

void wm8978_io_delay_ms(void *io_ctx, uint32_t milliseconds)
{
    (void)io_ctx;
    (void)milliseconds;

    /* Replace with a board delay that is never shorter than milliseconds. */
}
