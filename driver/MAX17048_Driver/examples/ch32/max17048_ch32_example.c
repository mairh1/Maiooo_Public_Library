/**
 * @file    max17048_ch32_example.c
 * @brief   MAX17048 usage example on CH32 (board functions to be wired in)
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 *
 * @details
 * Shows the typical application flow: init, alert setup, 1s periodic
 * sampling with temperature compensation, and servicing the ALRT pin from
 * thread context (the GPIO ISR only sets a flag). Board I2C functions
 * referenced here are the adapter callbacks the concrete BSP provides.
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stdint.h>

#include "max17048.h"
#include "max17048_ch32_i2c_port.h"

/* ── 由具体 BSP 实现的板级函数（链接时由工程提供） ───────────────── */
extern int32_t board_i2c_mem_write(void *ctx, uint8_t addr7, uint8_t reg,
                                   const uint8_t *data, uint16_t len,
                                   uint32_t timeout_ms);
extern int32_t board_i2c_mem_read(void *ctx, uint8_t addr7, uint8_t reg,
                                  uint8_t *data, uint16_t len,
                                  uint32_t timeout_ms);
/* ─────────────────────────────────────────────────────────────────── */

static max17048_dev_t g_gauge;                  /**< 电量计句柄（调用者分配） */
static max17048_ch32_adapter_t g_gauge_adapter; /**< CH32 桥接适配器 */

/** ALRT 引脚 ISR 只置标志，不在中断里碰 I2C（C9 约束） */
static volatile uint8_t g_alert_pending;

/**
 * @brief ALRT 引脚下降沿中断服务程序（CH32 EXTI）
 */
void EXTI7_0_IRQHandler(void)
{
    /* 只置标志，不做任何数据处理与总线访问 */
    g_alert_pending = 1u;
}

/**
 * @brief 电量计初始化：绑定适配器并配置告警
 * @retval  int  0 成功；其它为 max17048_result_t 错误码
 */
int app_gauge_init(void)
{
    max17048_result_t res;

    g_gauge_adapter.board_context = 0;              /* 单总线可用 NULL */
    g_gauge_adapter.mem_write = board_i2c_mem_write;
    g_gauge_adapter.mem_read = board_i2c_mem_read;
    g_gauge_adapter.io_timeout_ms = 10u;

    res = max17048_init(&g_gauge, &g_gauge_adapter, MAX17048_I2C_ADDR);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }

    /* SOC 低于 10% 告警；电压告警窗 3.3~4.25V */
    res = max17048_set_soc_alert_threshold(&g_gauge, 10u);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }
    res = max17048_set_voltage_alerts(&g_gauge, 3300u, 4250u);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }
    return (int)MAX17048_OK;
}

/**
 * @brief 1 秒周期任务：温度补偿 + 采样
 * @param[in] temp_x10 电池温度 ×10 ℃（来自 NTC/测温芯片）
 * @retval int  0 成功；其它为 max17048_result_t 错误码
 */
int app_gauge_periodic_1s(int16_t temp_x10)
{
    uint32_t vcell_mv;
    uint8_t soc;
    int16_t crate;
    max17048_result_t res;

    /* 手册要求至少每分钟补偿一次 RCOMP，这里 1s 一次更保守 */
    res = max17048_temp_compensate(&g_gauge, temp_x10);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }

    res = max17048_read_vcell(&g_gauge, &vcell_mv);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }
    res = max17048_read_soc(&g_gauge, &soc);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }
    res = max17048_read_crate(&g_gauge, &crate);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }
    return (int)MAX17048_OK;
}

/**
 * @brief 主循环任务：服务 ALRT 告警（线程上下文）
 * @retval int  0 成功；其它为 max17048_result_t 错误码
 */
int app_gauge_alert_task(void)
{
    max17048_status_t status;
    max17048_result_t res;

    if (g_alert_pending == 0u)
    {
        return (int)MAX17048_OK;
    }
    g_alert_pending = 0u;

    res = max17048_get_status(&g_gauge, &status);
    if (res != MAX17048_OK)
    {
        return (int)res;
    }

    if (status.soc_low)
    {
        /* 低电量处理：提示用户 / 保存状态 / 降低负载 */
    }
    if (status.vhigh || status.vlow)
    {
        /* 过压/欠压处理 */
    }
    if (status.reset_indicator)
    {
        /* 器件复位过：使用自定义模型时需 max17048_model_load() 重载 */
    }

    /* 清全部告警位并释放 ALRT 引脚 */
    return (int)max17048_clear_alerts(&g_gauge, MAX17048_STATUS_ALERT_MASK);
}
