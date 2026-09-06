/**
 * @file    ina219_ch32_i2c_port.c
 * @brief   绑定到 CH32 板级适配器的 INA219 驱动 io 契约实现
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 *
 * @details
 * 基于 ina219_ch32_i2c_port.h 中 SDK 无关的板级回调，实现
 * ina219_io.h 的各函数。把 ina219_ch32_adapter_t 指针作为 io_ctx
 * 传给 ina219_init()：
 *
 *     ina219_init(&meter, &g_i2c_adapter, INA219_I2C_ADDR);
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stddef.h>

#include "ina219_io.h"
#include "ina219_ch32_i2c_port.h"

/** 适配器健全性检查：读写回调齐全且超时为正。 */
static int ina219_ch32_adapter_valid(const ina219_ch32_adapter_t *adapter)
{
    if ((adapter == NULL) || (adapter->mem_write == NULL) ||
        (adapter->mem_read == NULL) || (adapter->io_timeout_ms == 0U))
    {
        return 0;
    }
#if INA219_USE_TRIGGERED
    if (adapter->delay_ms == NULL)
    {
        return 0;
    }
#endif
    return 1;
}

int ina219_io_init(void)
{
    /* CH32 的 I2C 外设通常在 BSP 中初始化，这里无需动作。 */
    return INA219_IO_OK;
}

int ina219_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint16_t *val)
{
    const ina219_ch32_adapter_t *adapter = (ina219_ch32_adapter_t *)io_ctx;
    uint8_t buf[2];

    if ((adapter == NULL) || (val == NULL) ||
        (ina219_ch32_adapter_valid(adapter) == 0))
    {
        return INA219_IO_ERROR;
    }
    if (adapter->mem_read(adapter->board_context, dev_addr, reg, buf, 2U,
                          adapter->io_timeout_ms) != 0)
    {
        return INA219_IO_ERROR;
    }
    *val = (uint16_t)((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    return INA219_IO_OK;
}

int ina219_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint16_t val)
{
    const ina219_ch32_adapter_t *adapter = (ina219_ch32_adapter_t *)io_ctx;
    uint8_t buf[2];

    if ((adapter == NULL) || (ina219_ch32_adapter_valid(adapter) == 0))
    {
        return INA219_IO_ERROR;
    }
    buf[0] = (uint8_t)(val >> 8);   /* 高字节在前 */
    buf[1] = (uint8_t)(val & 0xFFu);
    if (adapter->mem_write(adapter->board_context, dev_addr, reg, buf, 2U,
                           adapter->io_timeout_ms) != 0)
    {
        return INA219_IO_ERROR;
    }
    return INA219_IO_OK;
}

#if INA219_USE_TRIGGERED
void ina219_io_delay_ms(uint32_t ms)
{
    /* ina219_wait_conversion() 轮询会走到这里；USE_TRIGGERED=0 时不编译 */
    ina219_ch32_board_delay_ms(ms);
}
#endif /* INA219_USE_TRIGGERED */

#if INA219_THREAD_SAFE
extern void ina219_ch32_board_lock(void);
extern void ina219_ch32_board_unlock(void);

void ina219_io_lock(void)
{
    ina219_ch32_board_lock();
}

void ina219_io_unlock(void)
{
    ina219_ch32_board_unlock();
}
#endif /* INA219_THREAD_SAFE */
