/*
 * sgm41513_io.h - Platform I/O interface for the SGM41513 driver.
 *
 * This header plays the same role as diskio.h in FatFS: the driver core
 * (sgm41513.c) calls ONLY the functions below, and YOU implement them on
 * your platform. A ready-to-edit template is provided in
 * port/sgm41513_io_template.c.
 *
 * Implementation notes:
 *  - The driver only performs single-byte register reads/writes because
 *    REG09 and REG0E must not be accessed with burst (multi-byte) I2C
 *    transfers. Your port does not need block transfer support.
 *  - 'io_ctx' is an opaque pointer copied from sgm41513_dev_t::io_ctx on
 *    every call. Use it to identify the I2C bus when several SGM41513
 *    devices sit on different buses (pass NULL if you have only one).
 *  - 'dev_addr' is the 7-bit slave address (0x1A) taken from
 *    sgm41513_dev_t::dev_addr. Do NOT shift it left - most I2C APIs
 *    either take a 7-bit address directly or provide a macro such as
 *    (dev_addr << 1) for 8-bit APIs.
 */

#ifndef SGM41513_IO_H
#define SGM41513_IO_H

#include <stdint.h>
#include "sgm41513_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes for the io functions (similar in spirit to FatFS DRESULT) */
#define SGM41513_IO_OK      0   /* function succeeded                */
#define SGM41513_IO_ERROR  -1   /* any communication failure         */

/*
 * Optional one-time initialization of the bus (called once by
 * sgm41513_init()). Return SGM41513_IO_OK even if nothing is needed.
 * Most platforms initialize their I2C elsewhere and just return OK.
 */
int sgm41513_io_init(void);

/*
 * Read one register:  dev_addr[7-bit], reg address 0x00..0x0F.
 * Store the value in *val. Return SGM41513_IO_OK on success.
 */
int sgm41513_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint8_t *val);

/*
 * Write one register: dev_addr[7-bit], reg address 0x00..0x0F, new value.
 * Return SGM41513_IO_OK on success.
 */
int sgm41513_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t val);

/*
 * Millisecond delay. Only needed for the OTG start sequence
 * (>= 30 ms between leaving HIZ and enabling boost, tSM_DLY related
 * waits). Safe to make it a no-op if you never call sgm41513_otg_enable().
 */
void sgm41513_io_delay_ms(uint32_t ms);

#if SGM41513_THREAD_SAFE
/*
 * Optional concurrency hooks, only compiled when SGM41513_THREAD_SAFE = 1
 * in sgm41513_conf.h. Typical implementation: RTOS mutex take/give.
 * They are called around complete driver API calls.
 */
void sgm41513_io_lock(void);
void sgm41513_io_unlock(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_IO_H */
