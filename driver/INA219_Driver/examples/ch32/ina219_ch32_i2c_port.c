/**
 * @file    ina219_ch32_i2c_port.c
 * @brief   INA219 driver io contract bound to a CH32 board adapter
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 *
 * @details
 * Implements the functions from ina219_io.h on top of the SDK-neutral board
 * callbacks in ina219_ch32_i2c_port.h. Pass the ina219_ch32_adapter_t
 * pointer to ina219_init() as io_ctx:
 *
 *     ina219_init(&meter, &g_i2c_adapter, INA219_I2C_ADDR);
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stddef.h>

#include "ina219_io.h"
#include "ina219_ch32_i2c_port.h"

/** Adapter sanity check: read/write callbacks and a positive timeout. */
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
