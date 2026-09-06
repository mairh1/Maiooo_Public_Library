/**
 * @file    aw32257_io_template.c
 * @brief   AW32257 平台 I/O 移植模板。
 * @details 把本文件复制进应用工程，用所选 MCU SDK 的实现替换
 *          这些带超时保护的事务桩函数。
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
    /* 执行 S + 写地址 + 寄存器 + Sr + 读地址 + 字节 + P。 */
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
    /* 执行 S + 写地址 + 寄存器 + 值 + P。 */
    return AW32257_IO_ERROR;
}

void aw32257_io_delay_ms(void * io_ctx, uint32_t milliseconds)
{
    (void)io_ctx;
    (void)milliseconds;
    /* 在线程/主循环上下文实现不提前返回的延时。 */
}
