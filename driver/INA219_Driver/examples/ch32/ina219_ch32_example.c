/**
 * @file    ina219_ch32_example.c
 * @brief   INA219 driver usage example on CH32 (any family)
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 *
 * @details
 * Complete application flow: adapter wiring + init (1-ohm shunt defaults
 * come from ina219_conf.h) -> periodic 1 Hz sampling in continuous mode ->
 * optional single-shot trigger path -> power-down before stop mode.
 *
 * The board I2C callbacks referenced here (board_i2c_mem_write/read,
 * board_delay_ms) live in the project BSP; see this folder's README.md.
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stdio.h>

#include "ina219.h"
#include "ina219_ch32_i2c_port.h"

/* ─── 板级桥接（由工程 BSP 提供，见 examples/ch32/README.md）──────────── */

extern int32_t board_i2c_mem_write(void *ctx, uint8_t addr7, uint8_t reg,
                                   const uint8_t *data, uint16_t len,
                                   uint32_t timeout_ms);
extern int32_t board_i2c_mem_read(void *ctx, uint8_t addr7, uint8_t reg,
                                  uint8_t *data, uint16_t len,
                                  uint32_t timeout_ms);
extern void board_delay_ms(uint32_t ms);

/* ─── 设备实例（句柄与适配器都由调用者分配，可多实例多总线）────────────── */

static ina219_dev_t g_meter;
static ina219_ch32_adapter_t g_meter_adapter =
{
    .board_context = NULL,                      /* 单总线可为 NULL */
    .mem_write     = board_i2c_mem_write,
    .mem_read      = board_i2c_mem_read,
    .io_timeout_ms = 10,
};

/**
 * @brief  上电初始化：conf 默认 1Ω 采样电阻 / PGA /8（±320mA 量程）/
 *         Current_LSB 10µA / 双 12bit 连续转换，全部在 ina219_init() 内写入。
 * @return 0 成功；非 0 为 ina219_result_t 错误码。
 */
int app_meter_init(void)
{
    ina219_result_t res;

    res = ina219_init(&g_meter, &g_meter_adapter, INA219_I2C_ADDR);
    if (res != INA219_OK)
    {
        return (int)res;
    }

    /* 需要改动默认量程时按需追加，例如换 0.1Ω 采样电阻后重新校准：
     * ina219_set_calibration(&g_meter, 100000u, 10000u);  0.1Ω, 10µA LSB
     * 换小电流档位：
     * ina219_set_pga_range(&g_meter, 40);                 ±40mA @1Ω
     */
    return 0;
}

/** @brief 1Hz 周期采样（连续转换模式：数据寄存器始终是最近一次结果） */
void app_meter_periodic_1s(void)
{
    int32_t shunt_uv;
    int32_t current_ua;
    uint16_t bus_mv;
    uint32_t power_uw;
    bool overflow;

    (void)ina219_read_bus_voltage(&g_meter, &bus_mv);   /* mV */
    (void)ina219_read_shunt_voltage(&g_meter, &shunt_uv);   /* µV，正负号=方向 */
    (void)ina219_read_current(&g_meter, &current_ua);   /* µA */
    (void)ina219_read_power(&g_meter, &power_uw);       /* µW */

    if ((ina219_is_math_overflow(&g_meter, &overflow) == INA219_OK) &&
        overflow)
    {
        /* 电流/功率乘法溢出：数据无意义，检查校准与量程设置 */
        printf("INA219 math overflow!\r\n");
        return;
    }

    printf("bus=%lumV shunt=%lduV I=%lduA P=%luuW\r\n",
           (unsigned long)bus_mv, (long)shunt_uv, (long)current_ua,
           (unsigned long)power_uw);
}

/** @brief 低频场景：触发单次转换并等待完成（替代连续模式省电） */
void app_meter_single_shot(void)
{
    ina219_result_t res;

    (void)ina219_set_mode(&g_meter, INA219_MODE_TRIG_SHUNT_BUS_E);
    res = ina219_trigger(&g_meter);
    if (res == INA219_OK)
    {
        /* 12bit 双通道最长约 2×532µs + 乘法时间，150ms 超时余量充足 */
        res = ina219_wait_conversion(&g_meter, 150u);
    }
    if (res == INA219_OK)
    {
        app_meter_periodic_1s();
    }
}

/** @brief 进入停机模式前把器件切到掉电档（典型 5µA） */
void app_meter_suspend(void)
{
    /* 唤醒后在任务上下文调用 ina219_set_mode() 恢复连续转换即可；
     * 若停机期间完全断电（VS 掉电），唤醒后必须重新 ina219_init()。 */
    (void)ina219_set_mode(&g_meter, INA219_MODE_POWER_DOWN_E);
}
