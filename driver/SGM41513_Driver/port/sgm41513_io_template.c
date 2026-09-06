/*
 * sgm41513_io_template.c - SGM41513 驱动的移植模板。
 *
 * 移植步骤（3 步）：
 *
 *   1. 把本文件复制进你的工程（文件名随意，如 sgm41513_io.c）。
 *   2. 用你平台的 I2C API 填写下面四个函数。
 *   3. 需要时调整 sgm41513_conf.h（变体、功能开关等）。
 *
 * 驱动只做单字节寄存器访问，无需支持块传输。dev_addr 是 7 位地址
 * （0x1A）。
 *
 * 每个函数底部以注释形式给出两份已填好的示例（STM32 HAL 与
 * ESP-IDF）；用不到的删掉即可。
 */

#include "sgm41513_io.h"

/* ====================================================================== */
/* 1. 总线初始化（可选）                                                    */
/* ====================================================================== */
/* 由 sgm41513_init() 调用一次。若 I2C 总线已在别处初始化（如 main()
 * 或 bootloader 中），直接返回 OK 即可。                                  */

int sgm41513_io_init(void)
{
    return SGM41513_IO_OK;

    /* --- STM32 HAL 示例 ------------------------------------------------
    extern I2C_HandleTypeDef hi2c1;
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x1A << 1, 3, 100) != HAL_OK) {
        return SGM41513_IO_ERROR;
    }
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */

    /* --- ESP-IDF 示例 --------------------------------------------------
    i2c_master_bus_handle_t bus = app_i2c_get_bus();   // 你的初始化代码
    (void)bus;
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */
}

/* ====================================================================== */
/* 2. 读一个寄存器                                                         */
/* ====================================================================== */

int sgm41513_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint8_t *val)
{
    (void)io_ctx;      /* 总线上下文，单总线系统为 NULL                   */
    (void)dev_addr;
    (void)reg;

    /* --- 伪实现——替换为你的 I2C API ------------------------------------
    if (i2c_write(dev_addr, &reg, 1, no_stop) != 0) return ERROR;
    if (i2c_read(dev_addr, val, 1) != 0)             return ERROR;
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */

    /* --- STM32 HAL 示例 ------------------------------------------------
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)io_ctx;   // 或用全局句柄
    if (HAL_I2C_Mem_Read(hi2c, dev_addr << 1, reg,
                         I2C_MEMADD_SIZE_8BIT, val, 1, 100) == HAL_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    /* --- ESP-IDF 示例（i2c_master 驱动）--------------------------------
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)io_ctx;
    if (i2c_master_transmit_receive(dev, &reg, 1, val, 1, -1) == ESP_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    *val = 0u;
    return SGM41513_IO_ERROR;   /* 桩：尚未移植 */
}

/* ====================================================================== */
/* 3. 写一个寄存器                                                         */
/* ====================================================================== */

int sgm41513_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t val)
{
    (void)io_ctx;
    (void)dev_addr;
    (void)reg;
    (void)val;

    /* --- 伪实现——替换为你的 I2C API ------------------------------------
    uint8_t buf[2] = { reg, val };
    if (i2c_write(dev_addr, buf, 2, stop) != 0) return ERROR;
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */

    /* --- STM32 HAL 示例 ------------------------------------------------
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)io_ctx;
    if (HAL_I2C_Mem_Write(hi2c, dev_addr << 1, reg,
                          I2C_MEMADD_SIZE_8BIT, &val, 1, 100) == HAL_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    /* --- ESP-IDF 示例（i2c_master 驱动）--------------------------------
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)io_ctx;
    uint8_t buf[2] = { reg, val };
    if (i2c_master_transmit(dev, buf, 2, -1) == ESP_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    return SGM41513_IO_ERROR;   /* 桩：尚未移植 */
}

/* ====================================================================== */
/* 4. 毫秒延时                                                             */
/* ====================================================================== */
/* 仅 sgm41513_otg_enable() 使用（退出 HIZ 后 >= 30 ms 延时）。
 * 从不使能 OTG 时留空即可。                                               */

void sgm41513_io_delay_ms(uint32_t ms)
{
    /* --- 裸机 / SysTick -------------------------------------------------
    while (ms--) { delay_us(1000); }
    ------------------------------------------------------------------- */

    /* --- RTOS -----------------------------------------------------------
    vTaskDelay(pdMS_TO_TICKS(ms));          // FreeRTOS
    osDelay(ms);                            // CMSIS-RTOS2
    ------------------------------------------------------------------- */

    /* --- ESP-IDF --------------------------------------------------------
    vTaskDelay(ms / portTICK_PERIOD_MS);
    ------------------------------------------------------------------- */

    (void)ms;                   /* 桩：尚未移植 */
}

/* ====================================================================== */
/* 5. 并发保护钩子（仅当 SGM41513_THREAD_SAFE = 1 时编译）                  */
/* ====================================================================== */

#if SGM41513_THREAD_SAFE

void sgm41513_io_lock(void)
{
    /* 如 osMutexAcquire(g_i2c_mutex, osWaitForever);   CMSIS-RTOS2      */
    /* 如 xSemaphoreTake(g_i2c_sem, portMAX_DELAY);     FreeRTOS         */
}

void sgm41513_io_unlock(void)
{
    /* 如 osMutexRelease(g_i2c_mutex);                  CMSIS-RTOS2      */
    /* 如 xSemaphoreGive(g_i2c_sem);                    FreeRTOS         */
}

#endif /* SGM41513_THREAD_SAFE */
