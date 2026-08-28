/**
 * @file    aw32257_io.h
 * @brief   AW32257 fixed platform I/O contract.
 * @details The portable core calls only these functions. Implement them in
 *          the target port; see port/aw32257_io_template.c.
 */
#ifndef AW32257_IO_H
#define AW32257_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AW32257_IO_OK       0
#define AW32257_IO_ERROR   -1

/** Read one register using the 7-bit device address supplied by the core. */
int32_t aw32257_io_read_reg(void * io_ctx,
                            uint8_t address_7bit,
                            uint8_t register_address,
                            uint8_t * value,
                            uint32_t timeout_ms);

/** Write one register using the 7-bit device address supplied by the core. */
int32_t aw32257_io_write_reg(void * io_ctx,
                             uint8_t address_7bit,
                             uint8_t register_address,
                             uint8_t value,
                             uint32_t timeout_ms);

/** Delay at least the requested number of milliseconds. */
void aw32257_io_delay_ms(void * io_ctx, uint32_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* AW32257_IO_H */
