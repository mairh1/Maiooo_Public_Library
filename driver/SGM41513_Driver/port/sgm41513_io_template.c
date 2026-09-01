/*
 * sgm41513_io_template.c - Porting template for the SGM41513 driver.
 *
 * HOW TO PORT (3 steps):
 *
 *   1. Copy this file into your project (any name, e.g. sgm41513_io.c).
 *   2. Fill in the four functions below using your platform's I2C API.
 *   3. Adjust sgm41513_conf.h if needed (variant, feature switches...).
 *
 * The driver only performs single-byte register accesses, so no block
 * transfer support is required. dev_addr is the 7-bit address (0x1A).
 *
 * Two filled-in examples are provided at the bottom of each function as
 * comments (STM32 HAL and ESP-IDF); delete whichever you don't need.
 */

#include "sgm41513_io.h"

/* ====================================================================== */
/* 1. Bus initialization (optional)                                        */
/* ====================================================================== */
/* Called once by sgm41513_init(). If your I2C bus is initialized
 * elsewhere (e.g. in main() or the bootloader), just return OK.        */

int sgm41513_io_init(void)
{
    return SGM41513_IO_OK;

    /* --- STM32 HAL example --------------------------------------------
    extern I2C_HandleTypeDef hi2c1;
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x1A << 1, 3, 100) != HAL_OK) {
        return SGM41513_IO_ERROR;
    }
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */

    /* --- ESP-IDF example ----------------------------------------------
    i2c_master_bus_handle_t bus = app_i2c_get_bus();   // your init code
    (void)bus;
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */
}

/* ====================================================================== */
/* 2. Read one register                                                    */
/* ====================================================================== */

int sgm41513_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint8_t *val)
{
    (void)io_ctx;      /* bus context, NULL on single-bus systems       */
    (void)dev_addr;
    (void)reg;

    /* --- pseudo implementation - replace with your I2C API -----------
    if (i2c_write(dev_addr, &reg, 1, no_stop) != 0) return ERROR;
    if (i2c_read(dev_addr, val, 1) != 0)             return ERROR;
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */

    /* --- STM32 HAL example --------------------------------------------
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)io_ctx;   // or a global
    if (HAL_I2C_Mem_Read(hi2c, dev_addr << 1, reg,
                         I2C_MEMADD_SIZE_8BIT, val, 1, 100) == HAL_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    /* --- ESP-IDF example (i2c_master driver) --------------------------
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)io_ctx;
    if (i2c_master_transmit_receive(dev, &reg, 1, val, 1, -1) == ESP_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    *val = 0u;
    return SGM41513_IO_ERROR;   /* stub: not ported yet */
}

/* ====================================================================== */
/* 3. Write one register                                                   */
/* ====================================================================== */

int sgm41513_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t val)
{
    (void)io_ctx;
    (void)dev_addr;
    (void)reg;
    (void)val;

    /* --- pseudo implementation - replace with your I2C API -----------
    uint8_t buf[2] = { reg, val };
    if (i2c_write(dev_addr, buf, 2, stop) != 0) return ERROR;
    return SGM41513_IO_OK;
    ------------------------------------------------------------------- */

    /* --- STM32 HAL example --------------------------------------------
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)io_ctx;
    if (HAL_I2C_Mem_Write(hi2c, dev_addr << 1, reg,
                          I2C_MEMADD_SIZE_8BIT, &val, 1, 100) == HAL_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    /* --- ESP-IDF example (i2c_master driver) --------------------------
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)io_ctx;
    uint8_t buf[2] = { reg, val };
    if (i2c_master_transmit(dev, buf, 2, -1) == ESP_OK) {
        return SGM41513_IO_OK;
    }
    return SGM41513_IO_ERROR;
    ------------------------------------------------------------------- */

    return SGM41513_IO_ERROR;   /* stub: not ported yet */
}

/* ====================================================================== */
/* 4. Millisecond delay                                                    */
/* ====================================================================== */
/* Only used by sgm41513_otg_enable() (>= 30 ms HIZ exit delay).
 * Safe to leave empty if you never enable OTG.                          */

void sgm41513_io_delay_ms(uint32_t ms)
{
    /* --- bare metal / SysTick -----------------------------------------
    while (ms--) { delay_us(1000); }
    ------------------------------------------------------------------- */

    /* --- RTOS -----------------------------------------------------------
    vTaskDelay(pdMS_TO_TICKS(ms));          // FreeRTOS
    osDelay(ms);                            // CMSIS-RTOS2
    ------------------------------------------------------------------- */

    /* --- ESP-IDF --------------------------------------------------------
    vTaskDelay(ms / portTICK_PERIOD_MS);
    ------------------------------------------------------------------- */

    (void)ms;                   /* stub: not ported yet */
}

/* ====================================================================== */
/* 5. Concurrency hooks (only compiled when SGM41513_THREAD_SAFE = 1)      */
/* ====================================================================== */

#if SGM41513_THREAD_SAFE

void sgm41513_io_lock(void)
{
    /* e.g. osMutexAcquire(g_i2c_mutex, osWaitForever);   CMSIS-RTOS2   */
    /* e.g. xSemaphoreTake(g_i2c_sem, portMAX_DELAY);     FreeRTOS      */
}

void sgm41513_io_unlock(void)
{
    /* e.g. osMutexRelease(g_i2c_mutex);                  CMSIS-RTOS2   */
    /* e.g. xSemaphoreGive(g_i2c_sem);                    FreeRTOS      */
}

#endif /* SGM41513_THREAD_SAFE */
