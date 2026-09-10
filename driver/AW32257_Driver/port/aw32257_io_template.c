/**
 * @file    aw32257_io_template.c
 * @brief   AW32257 平台 I/O 移植模板。
 * @details 把本文件复制进应用工程，用所选 MCU SDK 的实现替换
 *          这些带超时保护的事务桩函数。契约语义见 aw32257_io.h：
 *          读写成功返回 0，失败返回 -1 或平台原始错误码。
 * @note    移植层只允许包含 aw32257_io.h 与平台头文件，不要包含
 *          aw32257.h；所有等待必须带硬性超时，禁止无限等待。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 */
#include "aw32257_io.h"

/**
 * @brief   读一个寄存器（契约实现，桩返回错误）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int32_t
aw32257_io_read_reg(void * io_ctx,
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

/**
 * @brief   写一个寄存器（契约实现，桩返回错误）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int32_t
aw32257_io_write_reg(void * io_ctx,
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

/**
 * @brief   延时至少所请求的毫秒数（契约实现，桩为空）。
 * @note    线程/主循环上下文实现，不得提前返回。
 */
void
aw32257_io_delay_ms(void * io_ctx, uint32_t milliseconds)
{
    (void)io_ctx;
    (void)milliseconds;
    /* 在线程/主循环上下文实现不提前返回的延时。 */
}
