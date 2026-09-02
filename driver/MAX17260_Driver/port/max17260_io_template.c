/**
 * @file    max17260_io_template.c
 * @brief   MAX17260 驱动移植模板
 * @details 移植三步走：
 *          1. 把本文件复制进工程（可改名 max17260_io.c）；
 *          2. 用目标平台 I2C API 填空下方 4 个函数（含多个平台的参考
 *             实现注释，用不到的删掉）；
 *          3. 按需调整 max17260_conf.h（变体、功能开关、RSENSE 等）。
 * @note    器件全部寄存器为 16 位（DieTemp 0x034 是 8 位地址），两个字节
 *          必须在同一 I2C 事务内传输，高字节在前（CH32/STM32 HAL 的
 *          Mem_Read/Mem_Write 天然满足）；dev_addr 为 7 位地址
 *          （SEWL/SETD=0x36，BEWL=0x0D），多数 HAL 需左移一位后传入。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-02
 */

#include "max17260_io.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 1. 总线初始化（可选）
 * ════════════════════════════════════════════════════════════════════════ */
/* max17260_init() 会调用一次；I2C 在别处初始化时直接返回 OK 即可。 */

int max17260_io_init(void)
{
    return MAX17260_IO_OK;

    /* ── CH32 标准库示例 ─────────────────────────────────────────────
    I2C_GenerateSTART(I2C1, ENABLE);
    ...（总线通常在 bsp 中初始化，这里一般直接返回 OK）
    ───────────────────────────────────────────────────────────────── */

    /* ── STM32 HAL 示例 ──────────────────────────────────────────────
    extern I2C_HandleTypeDef hi2c1;
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x36 << 1, 3, 100) != HAL_OK)
    {
        return MAX17260_IO_ERROR;
    }
    return MAX17260_IO_OK;
    ───────────────────────────────────────────────────────────────── */
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. 读一个 16 位寄存器
 * ════════════════════════════════════════════════════════════════════════ */

int max17260_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
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
    i2c_write_byte(reg);                // 寄存器地址，不产生 STOP
    i2c_repeated_start();               // Sr 重复起始
    i2c_write_byte(dev_addr << 1 | 1);  // 读位
    buf[0] = i2c_read_byte(ACK);        // 高字节
    buf[1] = i2c_read_byte(NACK);       // 低字节，最后一字节 NACK
    i2c_stop();
    *val = (uint16_t)((uint16_t)buf[0] << 8) | buf[1];
    return MAX17260_IO_OK;
    ───────────────────────────────────────────────────────────────── */

    /* ── STM32 HAL 示例 ──────────────────────────────────────────────
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)io_ctx;
    uint8_t buf[2];
    if (HAL_I2C_Mem_Read(hi2c, (uint16_t)(dev_addr << 1), reg,
                         I2C_MEMADD_SIZE_8BIT, buf, 2, 100) == HAL_OK)
    {
        *val = (uint16_t)((uint16_t)buf[0] << 8) | buf[1];
        return MAX17260_IO_OK;
    }
    return MAX17260_IO_ERROR;
    ───────────────────────────────────────────────────────────────── */

    /* ── ESP-IDF 示例（i2c_master 驱动）──────────────────────────────
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)io_ctx;
    uint8_t buf[2];
    if (i2c_master_transmit_receive(dev, &reg, 1, buf, 2, -1) == ESP_OK)
    {
        *val = (uint16_t)((uint16_t)buf[0] << 8) | buf[1];
        return MAX17260_IO_OK;
    }
    return MAX17260_IO_ERROR;
    ───────────────────────────────────────────────────────────────── */

    *val = 0u;
    return MAX17260_IO_ERROR;   /* 桩：尚未移植 */
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. 写一个 16 位寄存器
 * ════════════════════════════════════════════════════════════════════════ */

int max17260_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
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
    return MAX17260_IO_OK;
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
        return MAX17260_IO_OK;
    }
    return MAX17260_IO_ERROR;
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
        return MAX17260_IO_OK;
    }
    return MAX17260_IO_ERROR;
    ───────────────────────────────────────────────────────────────── */

    return MAX17260_IO_ERROR;   /* 桩：尚未移植 */
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. 毫秒延时
 * ════════════════════════════════════════════════════════════════════════ */
/* 当前驱动未使用延时函数（保留供未来序列使用），可放空实现。
 * 实际延时不得小于请求值。 */

void max17260_io_delay_ms(uint32_t ms)
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

/* ══════════════════════════════════════════════════════════════════════════
 * 5. 并发保护钩子（仅 MAX17260_THREAD_SAFE = 1 时编译）
 * ════════════════════════════════════════════════════════════════════════ */

#if MAX17260_THREAD_SAFE

void max17260_io_lock(void)
{
    /* 例：osMutexAcquire(g_i2c_mutex, osWaitForever);  CMSIS-RTOS2   */
    /* 例：xSemaphoreTake(g_i2c_sem, portMAX_DELAY);    FreeRTOS      */
}

void max17260_io_unlock(void)
{
    /* 例：osMutexRelease(g_i2c_mutex);                 CMSIS-RTOS2   */
    /* 例：xSemaphoreGive(g_i2c_sem);                   FreeRTOS      */
}

#endif /* MAX17260_THREAD_SAFE */
