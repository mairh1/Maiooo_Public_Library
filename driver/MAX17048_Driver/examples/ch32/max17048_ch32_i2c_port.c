/**
 * @file    max17048_ch32_i2c_port.c
 * @brief   MAX17048 driver io contract bound to a CH32 board adapter
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 *
 * @details
 * Implements the four functions from max17048_io.h on top of the SDK-neutral
 * board callbacks in max17048_ch32_i2c_port.h. Pass the
 * max17048_ch32_adapter_t pointer to max17048_init() as io_ctx:
 *
 *     max17048_init(&gauge, &g_i2c_adapter, MAX17048_I2C_ADDR);
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stddef.h>

#include "max17048_io.h"
#include "max17048_ch32_i2c_port.h"

/** Adapter sanity check: read/write callbacks and a positive timeout. */
static int max17048_ch32_adapter_valid(const max17048_ch32_adapter_t *adapter)
{
    if ((adapter == NULL) || (adapter->mem_write == NULL) ||
        (adapter->mem_read == NULL) || (adapter->io_timeout_ms == 0U))
    {
        return 0;
    }
    return 1;
}

int max17048_io_init(void)
{
    /* CH32 的 I2C 外设通常在 BSP 中初始化，这里无需动作。 */
    return MAX17048_IO_OK;
}

int max17048_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                           uint16_t *val)
{
    const max17048_ch32_adapter_t *adapter =
        (const max17048_ch32_adapter_t *)io_ctx;
    uint8_t buf[2];

    if ((adapter == NULL) || (val == NULL) ||
        (max17048_ch32_adapter_valid(adapter) == 0))
    {
        return MAX17048_IO_ERROR;
    }
    if (adapter->mem_read(adapter->board_context, dev_addr, reg, buf, 2U,
                          adapter->io_timeout_ms) != 0)
    {
        return MAX17048_IO_ERROR;
    }
    *val = (uint16_t)((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    return MAX17048_IO_OK;
}

int max17048_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                            uint16_t val)
{
    const max17048_ch32_adapter_t *adapter =
        (const max17048_ch32_adapter_t *)io_ctx;
    uint8_t buf[2];

    if ((adapter == NULL) ||
        (max17048_ch32_adapter_valid(adapter) == 0))
    {
        return MAX17048_IO_ERROR;
    }
    buf[0] = (uint8_t)(val >> 8);   /* 高字节在前 */
    buf[1] = (uint8_t)(val & 0xFFu);
    if (adapter->mem_write(adapter->board_context, dev_addr, reg, buf, 2U,
                           adapter->io_timeout_ms) != 0)
    {
        return MAX17048_IO_ERROR;
    }
    return MAX17048_IO_OK;
}

void max17048_io_delay_ms(uint32_t ms)
{
    /* 模型表加载序列会走到这里；USE_MODEL_TABLE=0 时不会被调用 */
    max17048_ch32_board_delay_ms(ms);
}

#if MAX17048_THREAD_SAFE
extern void max17048_ch32_board_lock(void);
extern void max17048_ch32_board_unlock(void);

void max17048_io_lock(void)
{
    max17048_ch32_board_lock();
}

void max17048_io_unlock(void)
{
    max17048_ch32_board_unlock();
}
#endif /* MAX17048_THREAD_SAFE */
