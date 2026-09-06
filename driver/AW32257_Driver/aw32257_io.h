/**
 * @file    aw32257_io.h
 * @brief   AW32257 固定的平台 I/O 移植契约。
 * @details 可移植核心只调用下列函数。在目标平台的移植层实现它们，
 *          模板见 port/aw32257_io_template.c。
 */
#ifndef AW32257_IO_H
#define AW32257_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AW32257_IO_OK       0
#define AW32257_IO_ERROR   -1

/** 用核心提供的 7 位器件地址读一个寄存器。 */
int32_t aw32257_io_read_reg(void * io_ctx,
                            uint8_t address_7bit,
                            uint8_t register_address,
                            uint8_t * value,
                            uint32_t timeout_ms);

/** 用核心提供的 7 位器件地址写一个寄存器。 */
int32_t aw32257_io_write_reg(void * io_ctx,
                             uint8_t address_7bit,
                             uint8_t register_address,
                             uint8_t value,
                             uint32_t timeout_ms);

/** 延时至少所请求的毫秒数。 */
void aw32257_io_delay_ms(void * io_ctx, uint32_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* AW32257_IO_H */
