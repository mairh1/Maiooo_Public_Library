/*
 * @file    ina219_io_template.c
 * @brief   INA219 驱动移植模板
 * @details 移植三步走：
 *          1. 把本文件复制进工程（可改名 ina219_io.c）；
 *          2. 用目标平台 I2C API 填空下方 3~4 个函数（含三个平台的
 *             参考实现注释，用不到的删掉）；
 *          3. 按需调整 ina219_conf.h（采样电阻、Current_LSB、量程、
 *             功能开关等）。
 * @note    器件全部寄存器为 16 位，两字节必须在同一 I2C 事务内传输，
 *          高字节在前（CH32/STM32 HAL 的 Mem_Read/Mem_Write 天然满足）；
 *          dev_addr 为 7 位地址（0x40~0x4F），多数 HAL 需左移一位后传入。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 */

#include "ina219_io.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 1. 总线初始化（必选）
 * ══════════════════════════════════════════════════════════════════════════ */
/* ina219_init() 会调用一次；I2C 在别处初始化时直接返回 OK 即可。 */

int ina219_io_init(void)
{
    return INA219_IO_OK;

    /* ── CH32 标准库示例 ─────────────────────────────────────────────
    I2C_GenerateSTART(I2C1, ENABLE);
    ...（总线通常在 bsp 中初始化，这里一般直接返回 OK）
    ───────────────────────────────────────────────────────────────── */

    /* ── STM32 HAL 示例 ──────────────────────────────────────────────
    extern I2C_HandleTypeDef hi2c1;
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x40 << 1, 3, 100) != HAL_OK)
    {
        return INA219_IO_ERROR;
    }
    return INA219_IO_OK;
    ───────────────────────────────────────────────────────────────── */
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. 读一个 16 位寄存器（必选）
 * ══════════════════════════════════════════════════════════════════════════ */

int ina219_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint16_t *val)
{
    (void)io_ctx;   /* 总线上下文，单总线系统忽略 */
    (void)dev_addr;
    (void)reg;
    (void)val;

    /* ── 伪代码（换成你的 I2C API）───────────────────────────────────
    uint8_t buf[2];
    i2c_start();
    i2c_write_byte(dev_addr << 1);      // 7 位地址左移 + 写位
    i2c_write_byte(reg);                // 寄存器指针，不产生 STOP
    i2c_repeated_start();               // Sr 重复起始
    i2c_write_byte(dev_addr << 1 | 1);  // 读位
    buf[0] = i2c_read_byte(ACK);        // 高字节
    buf[1] = i2c_read_byte(NACK);       // 低字节，最后一字节 NACK
    i2c_stop();
    *val = (uint16_t)((uint16_t)buf[0] << 8) | buf[1];
    return INA219_IO_OK;
    ───────────────────────────────────────────────────────────────── */

    /* ── STM32 HAL 示例 ──────────────────────────────────────────────
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)io_ctx;
    uint8_t buf[2];
    if (HAL_I2C_Mem_Read(hi2c, (uint16_t)(dev_addr << 1), reg,
                         I2C_MEMADD_SIZE_8BIT, buf, 2, 100) == HAL_OK)
    {
        *val = (uint16_t)((uint16_t)buf[0] << 8) | buf[1];
        return INA219_IO_OK;
    }
    return INA219_IO_ERROR;
    ───────────────────────────────────────────────────────────────── */

    /* ── ESP-IDF 示例（i2c_master 驱动）──────────────────────────────
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)io_ctx;
    uint8_t buf[2];
    if (i2c_master_transmit_receive(dev, &reg, 1, buf, 2, -1) == ESP_OK)
    {
        *val = (uint16_t)((uint16_t)buf[0] << 8) | buf[1];
        return INA219_IO_OK;
    }
    return INA219_IO_ERROR;
    ───────────────────────────────────────────────────────────────── */

    *val = 0u;
    return INA219_IO_ERROR;   /* 桩：尚未移植 */
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. 写一个 16 位寄存器（必选）
 * ══════════════════════════════════════════════════════════════════════════ */

int ina219_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint16_t val)
{
    (void)io_ctx;
    (void)dev_addr;
    (void)reg;
    (void)val;

    /* ── 伪代码（换成你的 I2C API）───────────────────────────────────
    uint8_t buf[3] =
    {
        reg,
        (uint8_t)(val >> 8),            // 高字节在前
        (uint8_t)(val & 0xFFu)          // 低字节
    };
    i2c_write(dev_addr << 1, buf, 3);   // 一次事务发完 3 字节再 STOP
    return INA219_IO_OK;
    ───────────────────────────────────────────────────────────────── */

    /* ── STM32 HAL 示例 ──────────────────────────────────────────────
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)io_ctx;
    uint8_t buf[2] =
    {
        (uint8_t)(val >> 8),
        (uint8_t)(val & 0xFFu)
    };
    if (HAL_I2C_Mem_Write(hi2c, (uint16_t)(dev_addr << 1), reg,
                          I2C_MEMADD_SIZE_8BIT, buf, 2, 100) == HAL_OK)
    {
        return INA219_IO_OK;
    }
    return INA219_IO_ERROR;
    ───────────────────────────────────────────────────────────────── */

    /* ── ESP-IDF 示例（i2c_master 驱动）──────────────────────────────
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)io_ctx;
    uint8_t buf[3] =
    {
        reg,
        (uint8_t)(val >> 8),
        (uint8_t)(val & 0xFFu)
    };
    if (i2c_master_transmit(dev, buf, 3, -1) == ESP_OK)
    {
        return INA219_IO_OK;
    }
    return INA219_IO_ERROR;
    ───────────────────────────────────────────────────────────────── */

    return INA219_IO_ERROR;   /* 桩：尚未移植 */
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. 毫秒延时（仅 INA219_USE_TRIGGERED = 1 时必选，其余配置可放空）
 * ══════════════════════════════════════════════════════════════════════════ */
/* 仅 ina219_wait_conversion() 轮询等待使用；只用连续转换模式且不等待的
 * 应用可在 conf 置 INA219_USE_TRIGGERED=0 后删除本函数。 */

#if INA219_USE_TRIGGERED
void ina219_io_delay_ms(uint32_t ms)
{
    /* ── 裸机 / SysTick ─────────────────────────────────────────────
    while (ms--)
    {
        delay_us(1000);
    }
    ───────────────────────────────────────────────────────────────── */

    /* ── RTOS ─────────────────────────────────────────────────────────
    vTaskDelay(pdMS_TO_TICKS(ms));      // FreeRTOS
    osDelay(ms);                        // CMSIS-RTOS2
    ───────────────────────────────────────────────────────────────── */

    (void)ms;   /* 桩：尚未移植 */
}
#endif /* INA219_USE_TRIGGERED */

/* ══════════════════════════════════════════════════════════════════════════
 * 5. 并发保护钩子（仅 INA219_THREAD_SAFE = 1 时编译）
 * ══════════════════════════════════════════════════════════════════════════ */

#if INA219_THREAD_SAFE

void ina219_io_lock(void)
{
    /* 例：osMutexAcquire(g_i2c_mutex, osWaitForever);  CMSIS-RTOS2   */
    /* 例：xSemaphoreTake(g_i2c_sem, portMAX_DELAY);    FreeRTOS      */
}

void ina219_io_unlock(void)
{
    /* 例：osMutexRelease(g_i2c_mutex);                 CMSIS-RTOS2   */
    /* 例：xSemaphoreGive(g_i2c_sem);                   FreeRTOS      */
}

#endif /* INA219_THREAD_SAFE */
