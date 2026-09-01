/**
 * @file    drv2605l_io_template.c
 * @brief   DRV2605L I2C 移植层模板
 * @details 复制本文件到目标工程后，将 TODO 部分替换为平台 I2C 和延时
 *          实现。模板不包含任何厂商头文件，避免把平台依赖带入驱动核心。
 * @note    平台 I2C API 使用 7 位地址还是左移后的地址必须在此处转换；
 *          核心传入的地址始终为 0x5A。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-01
 */

#include "drv2605l_io.h"

/* ══════════════════════════ I2C 初始化 ══════════════════════════ */

int
drv2605l_io_init(void *io_ctx)
{
    (void)io_ctx;

    /*
     * TODO：如目标工程已在 BSP 初始化 I2C，此处直接返回 OK。
     * 例如：
     *     board_i2c_init(io_ctx);
     *     return board_i2c_is_ready(io_ctx) ? DRV2605L_IO_OK
     *                                       : DRV2605L_IO_ERROR;
     */
    return DRV2605L_IO_ERROR;
}

/* ══════════════════════════ 单字节寄存器访问 ══════════════════════════ */

int
drv2605l_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                     uint8_t *val)
{
    (void)io_ctx;
    (void)dev_addr;
    (void)reg;
    (void)val;

    /*
     * TODO：使用一次“写寄存器地址 + 重启 + 读 1 字节”事务。
     * 对多数 8 位地址 I2C API，应把 dev_addr 转为 dev_addr << 1；
     * 不要把 0xB4 固定写进驱动核心。
     */
    return DRV2605L_IO_ERROR;
}

int
drv2605l_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t val)
{
    (void)io_ctx;
    (void)dev_addr;
    (void)reg;
    (void)val;

    /*
     * TODO：使用一次“写寄存器地址 + 写 1 字节数据”事务。
     * 必须设置有限的总线超时，不能无限等待 BUSY、ACK 或事件标志。
     */
    return DRV2605L_IO_ERROR;
}

/* ══════════════════════════ 延时 ══════════════════════════ */

void
drv2605l_io_delay_ms(uint32_t ms)
{
    (void)ms;

    /*
     * TODO：实际延时不得短于 ms。
     * drv2605l_init() 和 drv2605l_reset() 至少需要 1ms。
     */
}

#if DRV2605L_THREAD_SAFE

/* ══════════════════════════ 并发钩子 ══════════════════════════ */

void
drv2605l_io_lock(void *io_ctx)
{
    (void)io_ctx;

    /*
     * TODO：裸机可关中断，RTOS 应获取保护同一 I2C 总线/实例的互斥量。
     */
}

void
drv2605l_io_unlock(void *io_ctx)
{
    (void)io_ctx;

    /*
     * TODO：实现与 drv2605l_io_lock() 配对的解锁。
     */
}

#endif /* DRV2605L_THREAD_SAFE */
