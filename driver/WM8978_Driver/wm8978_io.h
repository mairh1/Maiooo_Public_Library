/*
 * @file    wm8978_io.h
 * @brief   WM8978 driver porting contract
 * @details The core accesses hardware only through these fixed functions.
 *          Implement them in the application/BSP; see port/wm8978_io_template.c.
 *          This header must not include wm8978.h or vendor headers.
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef WM8978_IO_H
#define WM8978_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WM8978_IO_OK       0
#define WM8978_IO_ERROR   (-1)

/**
 * @brief Write one complete, already-packed WM8978 control frame.
 * @param io_ctx Caller-owned bus context, passed unchanged from wm8978_bind().
 * @param first_byte Control bits B15:B8.
 * @param second_byte Control bits B7:B0.
 * @param timeout_ms Finite transaction timeout, non-zero.
 * @return WM8978_IO_OK only after address/data ACKs and STOP/latch complete;
 *         WM8978_IO_ERROR on timeout, NACK, or any other bus failure.
 * @note A failure may have uncertain hardware side effects. The core enters
 *       DESYNCHRONIZED and must not be retried until reset. This function is
 *       required by every WM8978 instance.
 */
int32_t wm8978_io_write_control(void *io_ctx,
                                 uint8_t first_byte,
                                 uint8_t second_byte,
                                 uint32_t timeout_ms);

/**
 * @brief Delay for at least the requested number of milliseconds.
 * @param io_ctx Caller-owned board context.
 * @param milliseconds Requested delay.
 * @note Required by power-up APIs; implementations may be a no-op only when
 *       the application never calls an API requiring delay. Call from thread
 *       context; it is not ISR-safe because it may block.
 */
void wm8978_io_delay_ms(void *io_ctx, uint32_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_IO_H */
