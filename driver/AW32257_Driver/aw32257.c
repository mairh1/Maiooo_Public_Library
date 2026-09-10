/**
 * @file    aw32257.c
 * @brief   AW32257 可移植 C99 驱动实现
 * @details 实现 POR 安全初始化（REG06 首笔写入与回读校验）、类型化
 *          寄存器配置（读-改-写）、状态/配置快照与软件复位时序。
 *          所有总线访问经 aw32257_io.h 契约函数完成，核心不含任何
 *          平台代码；契约函数的返回值原样保存到实例的
 *          last_port_error，供 aw32257_get_last_port_error() 诊断。
 * @note    仅依赖自身头文件与 C99 标准类型头（stddef.h）；核心不可
 *          重入，禁止在中断服务程序(ISR)中调用。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "aw32257.h"
#include "aw32257_conf.h"
#include "aw32257_io.h"

#include <stddef.h>

/* ══════════════════════════ 常量与查找表 ══════════════════════════ */

/* 数值取自 AW32257 V1.5 手册寄存器表，对应 RSNS = 33 mOhm。 */
static const uint16_t aw32257_current_ma_33mohm[16] =
{
    496, 620, 868, 992, 1116, 1240, 1364, 1488,
    1612, 1736, 1860, 1984, 2108, 2232, 2356, 2480
};

static const uint16_t aw32257_term_current_ma_33mohm[8] =
{
    62, 124, 186, 248, 310, 372, 434, 496
};

/* ══════════════════════════ 私有辅助函数 ══════════════════════════ */

/**
 * @brief   判断快充电流码点是否在手册 16 档范围内。
 * @param   current_code 待校验的快充电流码点。
 * @retval  true  码点合法。
 * @retval  false 码点超出 AW32257_CURRENT_CODE_0F。
 */
static bool
aw32257_current_code_is_valid(aw32257_current_code_t current_code)
{
    return (uint32_t)current_code <= (uint32_t)AW32257_CURRENT_CODE_0F;
}

/**
 * @brief   判断充电终止电流码点是否在手册 8 档范围内。
 * @param   current_code 待校验的终止电流码点。
 * @retval  true  码点合法。
 * @retval  false 码点超出 AW32257_TERM_CURRENT_CODE_7。
 */
static bool
aw32257_term_current_code_is_valid(
    aw32257_term_current_code_t current_code)
{
    return (uint32_t)current_code <=
           (uint32_t)AW32257_TERM_CURRENT_CODE_7;
}

/**
 * @brief   校验实例处于可访问寄存器的 READY 状态。
 * @details 按错误优先级依次检查：空指针、未绑定、POR_REQUIRED、
 *          其余非 READY 状态。
 * @param   device 驱动实例，允许为 NULL。
 * @retval  AW32257_OK 实例处于 READY 状态。
 * @retval  AW32257_ERR_NULL_POINTER @p device 为 NULL。
 * @retval  AW32257_ERR_NOT_BOUND 实例未绑定上下文。
 * @retval  AW32257_ERR_POR_REQUIRED 实例锁存为必须真实硬件 POR。
 * @retval  AW32257_ERR_NOT_INITIALIZED 实例未完成上电初始化。
 */
static aw32257_status_t
aw32257_require_ready(const aw32257_t * device)
{
    if (device == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if (device->lifecycle == AW32257_LIFECYCLE_UNBOUND)
    {
        return AW32257_ERR_NOT_BOUND;
    }

    if (device->lifecycle == AW32257_LIFECYCLE_POR_REQUIRED)
    {
        return AW32257_ERR_POR_REQUIRED;
    }

    if (device->lifecycle != AW32257_LIFECYCLE_READY)
    {
        return AW32257_ERR_NOT_INITIALIZED;
    }

    return AW32257_OK;
}

/**
 * @brief   经 io 读契约函数读取一个寄存器并记录原始返回值。
 * @details 无条件把契约函数的返回值保存到 device->last_port_error；
 *          非零一律映射为 AW32257_ERR_IO，原始码不丢失。
 * @param[in,out] device 驱动实例。
 * @param   register_address 寄存器地址。
 * @param   value            输出：读到的字节。
 * @retval  AW32257_OK 读取成功。
 * @retval  AW32257_ERR_IO 契约函数返回非零。
 */
static aw32257_status_t
aw32257_port_read(aw32257_t * device,
                  uint8_t register_address,
                  uint8_t * value)
{
    int32_t port_status;

    port_status = aw32257_io_read_reg(device->io_ctx,
                                        AW32257_I2C_ADDRESS_7BIT,
                                        register_address,
                                        value,
                                        device->io_timeout_ms);
    device->last_port_error = port_status;

    if (port_status != 0)
    {
        return AW32257_ERR_IO;
    }

    return AW32257_OK;
}

/**
 * @brief   经 io 写契约函数写入一个寄存器并记录原始返回值。
 * @details 无条件把契约函数的返回值保存到 device->last_port_error；
 *          非零一律映射为 AW32257_ERR_IO，原始码不丢失。
 * @param[in,out] device 驱动实例。
 * @param   register_address 寄存器地址。
 * @param   value            待写入的字节。
 * @retval  AW32257_OK 写入成功。
 * @retval  AW32257_ERR_IO 契约函数返回非零。
 */
static aw32257_status_t
aw32257_port_write(aw32257_t * device,
                   uint8_t register_address,
                   uint8_t value)
{
    int32_t port_status;

    port_status = aw32257_io_write_reg(device->io_ctx,
                                         AW32257_I2C_ADDRESS_7BIT,
                                         register_address,
                                         value,
                                         device->io_timeout_ms);
    device->last_port_error = port_status;

    if (port_status != 0)
    {
        return AW32257_ERR_IO;
    }

    return AW32257_OK;
}

/**
 * @brief   对一个寄存器做读-改-写，只改 @p mask 覆盖的位。
 * @details 要求实例处于 READY 状态；非目标位保持原值；新值与旧值
 *          相同时跳过写事务；读失败时不写，写失败不重试。
 * @param[in,out] device 驱动实例。
 * @param   register_address 目标寄存器地址。
 * @param   mask             受影响位的掩码。
 * @param   field_value      目标位的新值（掩码外的位被忽略）。
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
static aw32257_status_t
aw32257_update_bits(aw32257_t * device,
                    uint8_t register_address,
                    uint8_t mask,
                    uint8_t field_value)
{
    aw32257_status_t status;
    uint8_t old_value;
    uint8_t new_value;

    status = aw32257_require_ready(device);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_port_read(device, register_address, &old_value);
    if (status != AW32257_OK)
    {
        return status;
    }

    new_value = (uint8_t)((old_value & (uint8_t)(~mask)) |
                          (field_value & mask));
    if (new_value == old_value)
    {
        return AW32257_OK;
    }

    return aw32257_port_write(device, register_address, new_value);
}

/**
 * @brief   把 REG03 原始值解码为厂商/型号/修订码。
 * @param   raw_reg03   REG03 原始值。
 * @param   device_info 输出：解码结果。
 */
static void
aw32257_decode_device_info(uint8_t raw_reg03,
                           aw32257_device_info_t * device_info)
{
    device_info->raw_reg03 = raw_reg03;
    device_info->vendor_code = (uint8_t)((raw_reg03 & AW32257_REG03_VENDOR_MASK) >>
                                         AW32257_REG03_VENDOR_SHIFT);
    device_info->part_code = (uint8_t)((raw_reg03 & AW32257_REG03_PART_MASK) >>
                                       AW32257_REG03_PART_SHIFT);
    device_info->revision_code = (uint8_t)(raw_reg03 &
                                           AW32257_REG03_REVISION_MASK);
}

/**
 * @brief   校验 REG03 的厂商+型号掩码是否与手册一致。
 * @details 修订码不参与比较，因此芯片未来修订不会误报。
 * @param   raw_reg03 REG03 原始值。
 * @retval  true  身份匹配。
 * @retval  false 身份不匹配。
 */
static bool
aw32257_device_id_is_valid(uint8_t raw_reg03)
{
    return (raw_reg03 & AW32257_REG03_ID_MASK) ==
           AW32257_REG03_ID_EXPECTED;
}

/**
 * @brief   把充电电压编码为 REG02 VOREG 档位。
 * @details 只接受能被 20 mV 档位精确表示的 3500..4500 mV 值，不取整、
 *          不钳位；非法值由调用者在访问总线前拒绝。
 * @param   voltage_mv 期望电压，单位 mV。
 * @param   encoded    输出：VOREG 编码值。
 * @retval  AW32257_OK 编码成功。
 * @retval  AW32257_ERR_NULL_POINTER @p encoded 为 NULL。
 * @retval  AW32257_ERR_RANGE 电压超界或不是 20 mV 的整数倍。
 */
static aw32257_status_t
aw32257_encode_charge_voltage(uint16_t voltage_mv,
                              uint8_t * encoded)
{
    uint16_t delta;

    if (encoded == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if ((voltage_mv < 3500U) || (voltage_mv > 4500U))
    {
        return AW32257_ERR_RANGE;
    }

    delta = (uint16_t)(voltage_mv - 3500U);
    if ((delta % 20U) != 0U)
    {
        return AW32257_ERR_RANGE;
    }

    *encoded = (uint8_t)((delta / 20U) << AW32257_REG02_VOREG_SHIFT);
    return AW32257_OK;
}

/**
 * @brief   把 REG02 原始值解码为充电电压。
 * @details 手册允许 VOREG 码点 0x32..0x3F 同样表示 4.50 V，这里把
 *          该区间统一解码为 4500 mV。
 * @param   raw_reg02 REG02 原始值。
 * @retval  uint16_t 充电电压，单位 mV。
 */
static uint16_t
aw32257_decode_charge_voltage(uint8_t raw_reg02)
{
    uint8_t code;

    code = (uint8_t)((raw_reg02 & AW32257_REG02_VOREG_MASK) >>
                     AW32257_REG02_VOREG_SHIFT);
    if (code >= 0x32U)
    {
        return 4500U;
    }

    return (uint16_t)(3500U + ((uint16_t)code * 20U));
}

/**
 * @brief   把 DPM 电压编码为 REG05 档位。
 * @details 只接受能被 75 mV 档位精确表示的 4250..4775 mV 值，
 *          不取整、不钳位。
 * @param   voltage_mv 期望电压，单位 mV。
 * @param   encoded    输出：DPM 电压档位编码值。
 * @retval  AW32257_OK 编码成功。
 * @retval  AW32257_ERR_NULL_POINTER @p encoded 为 NULL。
 * @retval  AW32257_ERR_RANGE 电压超界或不是 75 mV 的整数倍。
 */
static aw32257_status_t
aw32257_encode_dpm_voltage(uint16_t voltage_mv,
                           uint8_t * encoded)
{
    uint16_t delta;

    if (encoded == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if ((voltage_mv < 4250U) || (voltage_mv > 4775U))
    {
        return AW32257_ERR_RANGE;
    }

    delta = (uint16_t)(voltage_mv - 4250U);
    if ((delta % 75U) != 0U)
    {
        return AW32257_ERR_RANGE;
    }

    *encoded = (uint8_t)(delta / 75U);
    return AW32257_OK;
}

/**
 * @brief   把 REG05 原始值解码为 DPM 电压。
 * @param   raw_reg05 REG05 原始值。
 * @retval  uint16_t DPM 电压，单位 mV。
 */
static uint16_t
aw32257_decode_dpm_voltage(uint8_t raw_reg05)
{
    uint8_t code;

    code = (uint8_t)(raw_reg05 & AW32257_REG05_DPM_VOLTAGE_MASK);
    return (uint16_t)(4250U + ((uint16_t)code * 75U));
}

/**
 * @brief   把安全电压编码为 REG06 档位。
 * @details 只接受能被 20 mV 档位精确表示的 4200..4500 mV 值；
 *          REG06 仅在硬件 POR 后可写，编码失败意味着本次上电初始化
 *          中止。
 * @param   voltage_mv 期望电压，单位 mV。
 * @param   encoded    输出：REG06 低 4 位的安全电压编码值。
 * @retval  AW32257_OK 编码成功。
 * @retval  AW32257_ERR_NULL_POINTER @p encoded 为 NULL。
 * @retval  AW32257_ERR_RANGE 电压超界或不是 20 mV 的整数倍。
 */
static aw32257_status_t
aw32257_encode_safety_voltage(uint16_t voltage_mv,
                              uint8_t * encoded)
{
    uint16_t delta;

    if (encoded == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if ((voltage_mv < 4200U) || (voltage_mv > 4500U))
    {
        return AW32257_ERR_RANGE;
    }

    delta = (uint16_t)(voltage_mv - 4200U);
    if ((delta % 20U) != 0U)
    {
        return AW32257_ERR_RANGE;
    }

    *encoded = (uint8_t)(delta / 20U);
    return AW32257_OK;
}

/**
 * @brief   把 REG06 原始值解码为安全电压。
 * @param   raw_reg06 REG06 原始值。
 * @retval  uint16_t 最大安全电压，单位 mV。
 */
static uint16_t
aw32257_decode_safety_voltage(uint8_t raw_reg06)
{
    uint8_t code;

    code = (uint8_t)(raw_reg06 & AW32257_REG06_SAFE_VOLTAGE_MASK);
    return (uint16_t)(4200U + ((uint16_t)code * 20U));
}

/**
 * @brief   把终止判定配置编码为 REG07 可写字段值。
 * @details 逐项校验手册档位：窗口 8/16 周期、有效周期数 1/2/4/8、
 *          单周期去抖 8/16/32/64 ms、再充电阈值 50..200 mV（50 mV
 *          步进）。另外按手册电气特性拒绝“有效周期数 × 单周期去抖
 *          大于 256 ms”的组合——寄存器本身允许到 512 ms，但手册给出
 *          的终止检测上限是 256 ms。
 * @param   config  终止判定配置。
 * @param   encoded 输出：REG07 可写字段的编码值。
 * @retval  AW32257_OK 编码成功。
 * @retval  AW32257_ERR_NULL_POINTER @p config 或 @p encoded 为 NULL。
 * @retval  AW32257_ERR_RANGE 某字段超出手册档位或组合超出时序上限。
 */
static aw32257_status_t
aw32257_encode_termination_config(
    const aw32257_termination_config_t * config,
    uint8_t * encoded)
{
    uint8_t window_code;
    uint8_t valid_code;
    uint8_t deglitch_code;
    uint8_t recharge_code;

    if ((config == NULL) || (encoded == NULL))
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if (config->window_periods == 8U)
    {
        window_code = 0U;
    }
    else if (config->window_periods == 16U)
    {
        window_code = AW32257_REG07_WINDOW_PERIODS_MASK;
    }
    else
    {
        return AW32257_ERR_RANGE;
    }

    switch (config->valid_periods)
    {
        case 1U:
            valid_code = 0U;
            break;
        case 2U:
            valid_code = 1U;
            break;
        case 4U:
            valid_code = 2U;
            break;
        case 8U:
            valid_code = 3U;
            break;
        default:
            return AW32257_ERR_RANGE;
    }

    switch (config->deglitch_ms)
    {
        case 8U:
            deglitch_code = 0U;
            break;
        case 16U:
            deglitch_code = 1U;
            break;
        case 32U:
            deglitch_code = 2U;
            break;
        case 64U:
            deglitch_code = 3U;
            break;
        default:
            return AW32257_ERR_RANGE;
    }

    if (((uint16_t)config->valid_periods *
         (uint16_t)config->deglitch_ms) > 256U)
    {
        return AW32257_ERR_RANGE;
    }

    if ((config->recharge_threshold_mv < 50U) ||
        (config->recharge_threshold_mv > 200U) ||
        ((config->recharge_threshold_mv % 50U) != 0U))
    {
        return AW32257_ERR_RANGE;
    }
    recharge_code = (uint8_t)((config->recharge_threshold_mv / 50U) - 1U);

    *encoded = (uint8_t)(window_code |
                         (uint8_t)(valid_code << AW32257_REG07_VALID_PERIODS_SHIFT) |
                         (uint8_t)(deglitch_code << AW32257_REG07_DEGLITCH_SHIFT) |
                         recharge_code);
    return AW32257_OK;
}

/**
 * @brief   把 REG07 原始值解码为终止判定配置。
 * @param   raw_reg07 REG07 原始值。
 * @param   config    输出：解码结果。
 */
static void
aw32257_decode_termination_config(
    uint8_t raw_reg07,
    aw32257_termination_config_t * config)
{
    uint8_t valid_code;
    uint8_t deglitch_code;
    uint8_t recharge_code;

    valid_code = (uint8_t)((raw_reg07 & AW32257_REG07_VALID_PERIODS_MASK) >>
                           AW32257_REG07_VALID_PERIODS_SHIFT);
    deglitch_code = (uint8_t)((raw_reg07 & AW32257_REG07_DEGLITCH_MASK) >>
                              AW32257_REG07_DEGLITCH_SHIFT);
    recharge_code = (uint8_t)(raw_reg07 & AW32257_REG07_RECHARGE_MASK);

    config->window_periods = ((raw_reg07 & AW32257_REG07_WINDOW_PERIODS_MASK) != 0U) ?
                             16U : 8U;
    config->valid_periods = (uint8_t)(1U << valid_code);
    config->deglitch_ms = (uint8_t)(8U << deglitch_code);
    config->recharge_threshold_mv = (uint16_t)(50U +
                                               ((uint16_t)recharge_code * 50U));
}

/**
 * @brief   把升压配置编码为 REG0A 可写字段值。
 * @details 校验频率档位（1500/1700 kHz）、压摆率码点与输出电压
 *          5050..5350 mV（100 mV 步进），组合出频率、压摆率、死时间、
 *          强制 PWM 与输出电压字段。
 * @param   config  升压配置。
 * @param   encoded 输出：REG0A 可写字段的编码值。
 * @retval  AW32257_OK 编码成功。
 * @retval  AW32257_ERR_NULL_POINTER @p config 或 @p encoded 为 NULL。
 * @retval  AW32257_ERR_RANGE 某字段超出手册档位。
 */
static aw32257_status_t
aw32257_encode_boost_config(
    const aw32257_boost_config_t * config,
    uint8_t * encoded)
{
    uint8_t frequency_code;
    uint8_t output_code;

    if ((config == NULL) || (encoded == NULL))
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if (config->frequency_khz == 1500U)
    {
        frequency_code = 0U;
    }
    else if (config->frequency_khz == 1700U)
    {
        frequency_code = AW32257_REG0A_FREQUENCY_MASK;
    }
    else
    {
        return AW32257_ERR_RANGE;
    }

    if ((uint32_t)config->slew_rate >
        (uint32_t)AW32257_SLEW_RATE_SLOWEST)
    {
        return AW32257_ERR_RANGE;
    }

    if ((config->output_voltage_mv < 5050U) ||
        (config->output_voltage_mv > 5350U) ||
        (((config->output_voltage_mv - 5050U) % 100U) != 0U))
    {
        return AW32257_ERR_RANGE;
    }
    output_code = (uint8_t)((config->output_voltage_mv - 5050U) / 100U);

    *encoded = (uint8_t)(frequency_code |
                         (uint8_t)((uint8_t)config->slew_rate <<
                                   AW32257_REG0A_SLEW_RATE_SHIFT) |
                         (config->fixed_dead_time ?
                          AW32257_REG0A_FIXED_DEAD_TIME_MASK : 0U) |
                         (config->force_pwm ?
                          AW32257_REG0A_FORCE_PWM_MASK : 0U) |
                         output_code);
    return AW32257_OK;
}

/**
 * @brief   把 REG0A 原始值解码为升压配置。
 * @param   raw_reg0a REG0A 原始值。
 * @param   config    输出：解码结果。
 */
static void
aw32257_decode_boost_config(uint8_t raw_reg0a,
                            aw32257_boost_config_t * config)
{
    uint8_t output_code;

    output_code = (uint8_t)(raw_reg0a & AW32257_REG0A_OUTPUT_VOLTAGE_MASK);
    config->output_voltage_mv = (uint16_t)(5050U +
                                           ((uint16_t)output_code * 100U));
    config->frequency_khz = ((raw_reg0a & AW32257_REG0A_FREQUENCY_MASK) != 0U) ?
                            1700U : 1500U;
    config->slew_rate = (aw32257_slew_rate_t)(
        (raw_reg0a & AW32257_REG0A_SLEW_RATE_MASK) >>
        AW32257_REG0A_SLEW_RATE_SHIFT);
    config->fixed_dead_time =
        (raw_reg0a & AW32257_REG0A_FIXED_DEAD_TIME_MASK) != 0U;
    config->force_pwm = (raw_reg0a & AW32257_REG0A_FORCE_PWM_MASK) != 0U;
}

/* ══════════════════════════ 生命周期与初始化 ══════════════════════════ */

aw32257_status_t
aw32257_init(aw32257_t * device,
             void * io_ctx,
             uint32_t io_timeout_ms)
{
    if (device == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if (io_timeout_ms == 0U)
    {
        return AW32257_ERR_INVALID_ARGUMENT;
    }

    device->io_ctx = io_ctx;
    device->io_timeout_ms = io_timeout_ms;
    device->lifecycle = AW32257_LIFECYCLE_BOUND;
    device->last_port_error = 0;

    return AW32257_OK;
}

aw32257_status_t
aw32257_power_on_init(aw32257_t * device,
                      const aw32257_safety_config_t * safety,
                      aw32257_device_info_t * device_info)
{
    aw32257_status_t status;
    aw32257_device_info_t local_info;
    uint8_t safe_voltage_code;
    uint8_t expected_safety;
    uint8_t actual_safety;
    uint8_t raw_reg03;

    if (device == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if (device->lifecycle == AW32257_LIFECYCLE_UNBOUND)
    {
        return AW32257_ERR_NOT_BOUND;
    }

    if (device->lifecycle == AW32257_LIFECYCLE_POR_REQUIRED)
    {
        return AW32257_ERR_POR_REQUIRED;
    }

    if (device->lifecycle != AW32257_LIFECYCLE_BOUND)
    {
        return AW32257_ERR_STATE;
    }

    if (safety == NULL)
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return AW32257_ERR_NULL_POINTER;
    }

    if (!aw32257_current_code_is_valid(safety->max_charge_current))
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return AW32257_ERR_RANGE;
    }

    status = aw32257_encode_safety_voltage(safety->max_charge_voltage_mv,
                                            &safe_voltage_code);
    if (status != AW32257_OK)
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return status;
    }

    expected_safety = (uint8_t)(
        ((uint8_t)safety->max_charge_current << AW32257_REG06_SAFE_CURRENT_SHIFT) |
        safe_voltage_code);

    /* REG06 必须是真实硬件 POR 之后的第一个总线事务。 */
    status = aw32257_port_write(device,
                                AW32257_REG_SAFETY_LIMIT,
                                expected_safety);
    if (status != AW32257_OK)
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return status;
    }

    status = aw32257_port_read(device,
                               AW32257_REG_SAFETY_LIMIT,
                               &actual_safety);
    if (status != AW32257_OK)
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return status;
    }

    if (actual_safety != expected_safety)
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return AW32257_ERR_SAFETY_MISMATCH;
    }

    status = aw32257_port_read(device, AW32257_REG_DEVICE_ID, &raw_reg03);
    if (status != AW32257_OK)
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return status;
    }

    if (!aw32257_device_id_is_valid(raw_reg03))
    {
        device->lifecycle = AW32257_LIFECYCLE_POR_REQUIRED;
        return AW32257_ERR_ID_MISMATCH;
    }

    aw32257_decode_device_info(raw_reg03, &local_info);
    device->lifecycle = AW32257_LIFECYCLE_READY;

    if (device_info != NULL)
    {
        *device_info = local_info;
    }

    return AW32257_OK;
}

aw32257_status_t
aw32257_soft_reset(aw32257_t * device)
{
    aw32257_status_t status;
    uint8_t raw_reg00;
    uint8_t charge_state;

    status = aw32257_require_ready(device);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_port_read(device, AW32257_REG_STATUS_CONTROL, &raw_reg00);
    if (status != AW32257_OK)
    {
        return status;
    }

    charge_state = (uint8_t)((raw_reg00 & AW32257_REG00_CHARGE_STATE_MASK) >>
                             AW32257_REG00_CHARGE_STATE_SHIFT);
    if (((raw_reg00 & AW32257_REG00_BOOST_ACTIVE_MASK) != 0U) ||
        (charge_state == (uint8_t)AW32257_CHARGE_STATE_IN_PROGRESS))
    {
        return AW32257_ERR_STATE;
    }

    status = aw32257_port_write(device,
                                AW32257_REG_CHARGE_CURRENT,
                                AW32257_REG04_SOFT_RESET_MASK);

    /* 器件接受 RESET 后 ACK 可能丢失，因此无条件等待。 */
    aw32257_io_delay_ms(device->io_ctx, AW32257_SOFT_RESET_DELAY_MS);

    return status;
}

aw32257_lifecycle_t
aw32257_get_lifecycle(const aw32257_t * device)
{
    if (device == NULL)
    {
        return AW32257_LIFECYCLE_UNBOUND;
    }

    return device->lifecycle;
}

int32_t
aw32257_get_last_port_error(const aw32257_t * device)
{
    if (device == NULL)
    {
        return 0;
    }

    return device->last_port_error;
}

/* ══════════════════════════ 状态与配置读取 ══════════════════════════ */

aw32257_status_t
aw32257_read_register(aw32257_t * device,
                      uint8_t register_address,
                      uint8_t * value)
{
    aw32257_status_t status;

    if (value == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    status = aw32257_require_ready(device);
    if (status != AW32257_OK)
    {
        return status;
    }

    if (register_address > AW32257_REG_LAST)
    {
        return AW32257_ERR_RANGE;
    }

    return aw32257_port_read(device, register_address, value);
}

aw32257_status_t
aw32257_read_device_info(aw32257_t * device,
                         aw32257_device_info_t * device_info)
{
    aw32257_status_t status;
    aw32257_device_info_t local_info;
    uint8_t raw_reg03;

    if (device_info == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    status = aw32257_require_ready(device);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_port_read(device, AW32257_REG_DEVICE_ID, &raw_reg03);
    if (status != AW32257_OK)
    {
        return status;
    }

    if (!aw32257_device_id_is_valid(raw_reg03))
    {
        return AW32257_ERR_ID_MISMATCH;
    }

    aw32257_decode_device_info(raw_reg03, &local_info);
    *device_info = local_info;
    return AW32257_OK;
}

aw32257_status_t
aw32257_read_status(aw32257_t * device,
                    aw32257_status_snapshot_t * snapshot)
{
    aw32257_status_t status;
    aw32257_status_snapshot_t local_snapshot;

    if (snapshot == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    status = aw32257_require_ready(device);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_port_read(device,
                               AW32257_REG_STATUS_CONTROL,
                               &local_snapshot.raw_reg00);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_port_read(device,
                               AW32257_REG_DPM_STATUS,
                               &local_snapshot.raw_reg05);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_port_read(device,
                               AW32257_REG_BOOST_FAULT,
                               &local_snapshot.raw_reg09);
    if (status != AW32257_OK)
    {
        return status;
    }

    local_snapshot.otg_pin_high =
        (local_snapshot.raw_reg00 & AW32257_REG00_OTG_PIN_MASK) != 0U;
    local_snapshot.stat_output_enabled =
        (local_snapshot.raw_reg00 & AW32257_REG00_EN_STAT_MASK) != 0U;
    local_snapshot.charge_state = (aw32257_charge_state_t)(
        (local_snapshot.raw_reg00 & AW32257_REG00_CHARGE_STATE_MASK) >>
        AW32257_REG00_CHARGE_STATE_SHIFT);
    local_snapshot.boost_active =
        (local_snapshot.raw_reg00 & AW32257_REG00_BOOST_ACTIVE_MASK) != 0U;
    local_snapshot.charge_fault = (aw32257_charge_fault_t)(
        local_snapshot.raw_reg00 & AW32257_REG00_CHARGE_FAULT_MASK);
    local_snapshot.dpm_active =
        (local_snapshot.raw_reg05 & AW32257_REG05_DPM_ACTIVE_MASK) != 0U;
    local_snapshot.cd_pin_high =
        (local_snapshot.raw_reg05 & AW32257_REG05_CD_PIN_MASK) != 0U;
    local_snapshot.boost_fault = (aw32257_boost_fault_t)(
        local_snapshot.raw_reg09 & AW32257_REG09_BOOST_FAULT_MASK);

    *snapshot = local_snapshot;
    return AW32257_OK;
}

aw32257_status_t
aw32257_read_configuration(aw32257_t * device,
                           aw32257_config_snapshot_t * snapshot)
{
    aw32257_status_t status;
    aw32257_config_snapshot_t local_snapshot;

    if (snapshot == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    status = aw32257_require_ready(device);
    if (status != AW32257_OK)
    {
        return status;
    }

    status = aw32257_port_read(device, AW32257_REG_STATUS_CONTROL,
                               &local_snapshot.raw_reg00);
    if (status != AW32257_OK)
    {
        return status;
    }
    status = aw32257_port_read(device, AW32257_REG_CONTROL,
                               &local_snapshot.raw_reg01);
    if (status != AW32257_OK)
    {
        return status;
    }
    status = aw32257_port_read(device, AW32257_REG_BATTERY_VOLTAGE,
                               &local_snapshot.raw_reg02);
    if (status != AW32257_OK)
    {
        return status;
    }
    status = aw32257_port_read(device, AW32257_REG_CHARGE_CURRENT,
                               &local_snapshot.raw_reg04);
    if (status != AW32257_OK)
    {
        return status;
    }
    status = aw32257_port_read(device, AW32257_REG_DPM_STATUS,
                               &local_snapshot.raw_reg05);
    if (status != AW32257_OK)
    {
        return status;
    }
    status = aw32257_port_read(device, AW32257_REG_SAFETY_LIMIT,
                               &local_snapshot.raw_reg06);
    if (status != AW32257_OK)
    {
        return status;
    }
    status = aw32257_port_read(device, AW32257_REG_TERMINATION,
                               &local_snapshot.raw_reg07);
    if (status != AW32257_OK)
    {
        return status;
    }
    status = aw32257_port_read(device, AW32257_REG_BOOST_CONFIG,
                               &local_snapshot.raw_reg0a);
    if (status != AW32257_OK)
    {
        return status;
    }

    local_snapshot.stat_output_enabled =
        (local_snapshot.raw_reg00 & AW32257_REG00_EN_STAT_MASK) != 0U;
    local_snapshot.termination_enabled =
        (local_snapshot.raw_reg01 & AW32257_REG01_TERMINATION_ENABLE_MASK) != 0U;
    local_snapshot.charge_enabled =
        (local_snapshot.raw_reg01 & AW32257_REG01_CHARGE_DISABLE_MASK) == 0U;
    local_snapshot.high_impedance_requested =
        (local_snapshot.raw_reg01 & AW32257_REG01_HIGH_Z_MASK) != 0U;
    local_snapshot.boost_requested =
        (local_snapshot.raw_reg01 & AW32257_REG01_BOOST_REQUEST_MASK) != 0U;
    local_snapshot.charge_voltage_mv =
        aw32257_decode_charge_voltage(local_snapshot.raw_reg02);
    local_snapshot.otg_active_high =
        (local_snapshot.raw_reg02 & AW32257_REG02_OTG_ACTIVE_HIGH_MASK) != 0U;
    local_snapshot.otg_pin_control_enabled =
        (local_snapshot.raw_reg02 & AW32257_REG02_OTG_PIN_ENABLE_MASK) != 0U;
    local_snapshot.fast_charge_current = (aw32257_current_code_t)(
        (local_snapshot.raw_reg04 & AW32257_REG04_FAST_CURRENT_MASK) >>
        AW32257_REG04_FAST_CURRENT_SHIFT);
    local_snapshot.termination_current = (aw32257_term_current_code_t)(
        local_snapshot.raw_reg04 & AW32257_REG04_TERM_CURRENT_MASK);
    local_snapshot.dpm_voltage_mv =
        aw32257_decode_dpm_voltage(local_snapshot.raw_reg05);
    local_snapshot.max_charge_current = (aw32257_current_code_t)(
        (local_snapshot.raw_reg06 & AW32257_REG06_SAFE_CURRENT_MASK) >>
        AW32257_REG06_SAFE_CURRENT_SHIFT);
    local_snapshot.max_charge_voltage_mv =
        aw32257_decode_safety_voltage(local_snapshot.raw_reg06);
    aw32257_decode_termination_config(local_snapshot.raw_reg07,
                                       &local_snapshot.termination);
    aw32257_decode_boost_config(local_snapshot.raw_reg0a,
                                &local_snapshot.boost);

    *snapshot = local_snapshot;
    return AW32257_OK;
}

/* ══════════════════════════ 配置写入 ══════════════════════════ */

aw32257_status_t
aw32257_set_stat_output_enabled(aw32257_t * device,
                                bool enabled)
{
    return aw32257_update_bits(device,
                               AW32257_REG_STATUS_CONTROL,
                               AW32257_REG00_EN_STAT_MASK,
                               enabled ? AW32257_REG00_EN_STAT_MASK : 0U);
}

aw32257_status_t
aw32257_set_charge_enabled(aw32257_t * device, bool enabled)
{
    return aw32257_update_bits(device,
                               AW32257_REG_CONTROL,
                               AW32257_REG01_CHARGE_DISABLE_MASK,
                               enabled ? 0U : AW32257_REG01_CHARGE_DISABLE_MASK);
}

aw32257_status_t
aw32257_set_termination_enabled(aw32257_t * device,
                                bool enabled)
{
    return aw32257_update_bits(
        device,
        AW32257_REG_CONTROL,
        AW32257_REG01_TERMINATION_ENABLE_MASK,
        enabled ? AW32257_REG01_TERMINATION_ENABLE_MASK : 0U);
}

aw32257_status_t
aw32257_set_mode(aw32257_t * device, aw32257_mode_t mode)
{
    uint8_t encoded;

    switch (mode)
    {
        case AW32257_MODE_CHARGE:
            encoded = 0U;
            break;
        case AW32257_MODE_HIGH_IMPEDANCE:
            encoded = AW32257_REG01_HIGH_Z_MASK;
            break;
        case AW32257_MODE_BOOST:
            encoded = AW32257_REG01_BOOST_REQUEST_MASK;
            break;
        default:
            return AW32257_ERR_RANGE;
    }

    return aw32257_update_bits(device,
                               AW32257_REG_CONTROL,
                               AW32257_REG01_MODE_MASK,
                               encoded);
}

aw32257_status_t
aw32257_set_charge_voltage_mv(aw32257_t * device,
                              uint16_t voltage_mv)
{
    aw32257_status_t status;
    uint8_t encoded;

    status = aw32257_encode_charge_voltage(voltage_mv, &encoded);
    if (status != AW32257_OK)
    {
        return status;
    }

    return aw32257_update_bits(device,
                               AW32257_REG_BATTERY_VOLTAGE,
                               AW32257_REG02_VOREG_MASK,
                               encoded);
}

aw32257_status_t
aw32257_set_fast_charge_current(
    aw32257_t * device,
    aw32257_current_code_t current_code)
{
    uint8_t encoded;

    if (!aw32257_current_code_is_valid(current_code))
    {
        return AW32257_ERR_RANGE;
    }

    encoded = (uint8_t)((uint8_t)current_code <<
                        AW32257_REG04_FAST_CURRENT_SHIFT);
    return aw32257_update_bits(device,
                               AW32257_REG_CHARGE_CURRENT,
                               (uint8_t)(AW32257_REG04_SOFT_RESET_MASK |
                                         AW32257_REG04_FAST_CURRENT_MASK),
                               encoded);
}

aw32257_status_t
aw32257_set_termination_current(
    aw32257_t * device,
    aw32257_term_current_code_t current_code)
{
    if (!aw32257_term_current_code_is_valid(current_code))
    {
        return AW32257_ERR_RANGE;
    }

    return aw32257_update_bits(device,
                               AW32257_REG_CHARGE_CURRENT,
                               (uint8_t)(AW32257_REG04_SOFT_RESET_MASK |
                                         AW32257_REG04_TERM_CURRENT_MASK),
                               (uint8_t)current_code);
}

aw32257_status_t
aw32257_set_dpm_voltage_mv(aw32257_t * device,
                           uint16_t voltage_mv)
{
    aw32257_status_t status;
    uint8_t encoded;

    status = aw32257_encode_dpm_voltage(voltage_mv, &encoded);
    if (status != AW32257_OK)
    {
        return status;
    }

    return aw32257_update_bits(device,
                               AW32257_REG_DPM_STATUS,
                               AW32257_REG05_DPM_VOLTAGE_MASK,
                               encoded);
}

aw32257_status_t
aw32257_set_termination_config(
    aw32257_t * device,
    const aw32257_termination_config_t * config)
{
    aw32257_status_t status;
    uint8_t encoded;

    status = aw32257_encode_termination_config(config, &encoded);
    if (status != AW32257_OK)
    {
        return status;
    }

    return aw32257_update_bits(device,
                               AW32257_REG_TERMINATION,
                               AW32257_REG07_WRITABLE_MASK,
                               encoded);
}

aw32257_status_t
aw32257_configure_otg_pin(aw32257_t * device,
                          bool enabled,
                          bool active_high)
{
    uint8_t encoded;

    encoded = (uint8_t)((enabled ? AW32257_REG02_OTG_PIN_ENABLE_MASK : 0U) |
                        (active_high ?
                         AW32257_REG02_OTG_ACTIVE_HIGH_MASK : 0U));
    return aw32257_update_bits(device,
                               AW32257_REG_BATTERY_VOLTAGE,
                               AW32257_REG02_OTG_CONTROL_MASK,
                               encoded);
}

aw32257_status_t
aw32257_set_boost_config(
    aw32257_t * device,
    const aw32257_boost_config_t * config)
{
    aw32257_status_t status;
    uint8_t encoded;

    status = aw32257_encode_boost_config(config, &encoded);
    if (status != AW32257_OK)
    {
        return status;
    }

    return aw32257_update_bits(device,
                               AW32257_REG_BOOST_CONFIG,
                               AW32257_REG0A_WRITABLE_MASK,
                               encoded);
}

/* ══════════════════════════ 电流换算与查表 ══════════════════════════ */

aw32257_status_t
aw32257_current_code_to_ma_33mohm(
    aw32257_current_code_t current_code,
    uint16_t * current_ma)
{
    if (current_ma == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if (!aw32257_current_code_is_valid(current_code))
    {
        return AW32257_ERR_RANGE;
    }

    *current_ma = aw32257_current_ma_33mohm[(uint8_t)current_code];
    return AW32257_OK;
}

aw32257_status_t
aw32257_termination_current_code_to_ma_33mohm(
    aw32257_term_current_code_t current_code,
    uint16_t * current_ma)
{
    if (current_ma == NULL)
    {
        return AW32257_ERR_NULL_POINTER;
    }

    if (!aw32257_term_current_code_is_valid(current_code))
    {
        return AW32257_ERR_RANGE;
    }

    *current_ma = aw32257_term_current_ma_33mohm[(uint8_t)current_code];
    return AW32257_OK;
}
