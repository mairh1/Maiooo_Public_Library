/**
 * @file    wm8978.c
 * @brief   WM8978 可移植 C99 驱动实现
 * @details 实现要点：
 *          - 以静态常量表维护 52 个有效寄存器地址白名单、数据手册
 *            复位默认值、可写位掩码与非锁存触发位（VU/UPDATE/NFU）
 *            掩码，写式控制接口所必需的软件影子缓存由实例携带；
 *          - 控制帧成功发送后才提交影子；任何端口错误都会把实例
 *            锁定为 DESYNCHRONIZED，防止继续使用可能失真的影子；
 *          - 拒绝保留位偏离复位值，并在任意 ADC/DAC 使能时拒绝
 *            切换 R18 EQ3DMODE，避免硅片拒位导致影子失真。
 * @note    仅依赖自身头文件与 <stddef.h>；驱动不可重入，不在
 *          中断上下文执行。
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "wm8978.h"
#include "wm8978_conf.h"
#include "wm8978_io.h"

#include <stddef.h>

/* ══════════════════════════ 私有常量表 ══════════════════════════ */

static const uint8_t wm8978_valid_registers[WM8978_REGISTER_SPACE_SIZE] =
{
    [WM8978_REG_SOFTWARE_RESET] = 1U,
    [WM8978_REG_POWER_MANAGEMENT_1] = 1U,
    [WM8978_REG_POWER_MANAGEMENT_2] = 1U,
    [WM8978_REG_POWER_MANAGEMENT_3] = 1U,
    [WM8978_REG_AUDIO_INTERFACE] = 1U,
    [WM8978_REG_COMPANDING_CONTROL] = 1U,
    [WM8978_REG_CLOCK_GENERATION] = 1U,
    [WM8978_REG_ADDITIONAL_CONTROL] = 1U,
    [WM8978_REG_GPIO] = 1U,
    [WM8978_REG_JACK_DETECT_1] = 1U,
    [WM8978_REG_DAC_CONTROL] = 1U,
    [WM8978_REG_LEFT_DAC_VOLUME] = 1U,
    [WM8978_REG_RIGHT_DAC_VOLUME] = 1U,
    [WM8978_REG_JACK_DETECT_2] = 1U,
    [WM8978_REG_ADC_CONTROL] = 1U,
    [WM8978_REG_LEFT_ADC_VOLUME] = 1U,
    [WM8978_REG_RIGHT_ADC_VOLUME] = 1U,
    [WM8978_REG_EQ1] = 1U,
    [WM8978_REG_EQ2] = 1U,
    [WM8978_REG_EQ3] = 1U,
    [WM8978_REG_EQ4] = 1U,
    [WM8978_REG_EQ5] = 1U,
    [WM8978_REG_DAC_LIMITER_1] = 1U,
    [WM8978_REG_DAC_LIMITER_2] = 1U,
    [WM8978_REG_NOTCH_FILTER_1] = 1U,
    [WM8978_REG_NOTCH_FILTER_2] = 1U,
    [WM8978_REG_NOTCH_FILTER_3] = 1U,
    [WM8978_REG_NOTCH_FILTER_4] = 1U,
    [WM8978_REG_ALC_CONTROL_1] = 1U,
    [WM8978_REG_ALC_CONTROL_2] = 1U,
    [WM8978_REG_ALC_CONTROL_3] = 1U,
    [WM8978_REG_NOISE_GATE] = 1U,
    [WM8978_REG_PLL_N] = 1U,
    [WM8978_REG_PLL_K1] = 1U,
    [WM8978_REG_PLL_K2] = 1U,
    [WM8978_REG_PLL_K3] = 1U,
    [WM8978_REG_3D_CONTROL] = 1U,
    [WM8978_REG_BEEP_CONTROL] = 1U,
    [WM8978_REG_INPUT_CONTROL] = 1U,
    [WM8978_REG_LEFT_INPUT_PGA] = 1U,
    [WM8978_REG_RIGHT_INPUT_PGA] = 1U,
    [WM8978_REG_LEFT_ADC_BOOST] = 1U,
    [WM8978_REG_RIGHT_ADC_BOOST] = 1U,
    [WM8978_REG_OUTPUT_CONTROL] = 1U,
    [WM8978_REG_LEFT_MIXER] = 1U,
    [WM8978_REG_RIGHT_MIXER] = 1U,
    [WM8978_REG_LEFT_HEADPHONE_VOLUME] = 1U,
    [WM8978_REG_RIGHT_HEADPHONE_VOLUME] = 1U,
    [WM8978_REG_LEFT_SPEAKER_VOLUME] = 1U,
    [WM8978_REG_RIGHT_SPEAKER_VOLUME] = 1U,
    [WM8978_REG_OUT3_MIXER] = 1U,
    [WM8978_REG_OUT4_MIXER] = 1U
};

static const uint16_t wm8978_reset_defaults[WM8978_REGISTER_SPACE_SIZE] =
{
    [WM8978_REG_SOFTWARE_RESET] = WM8978_R00_RESET_VALUE,
    [WM8978_REG_POWER_MANAGEMENT_1] = WM8978_R01_RESET_VALUE,
    [WM8978_REG_POWER_MANAGEMENT_2] = WM8978_R02_RESET_VALUE,
    [WM8978_REG_POWER_MANAGEMENT_3] = WM8978_R03_RESET_VALUE,
    [WM8978_REG_AUDIO_INTERFACE] = WM8978_R04_RESET_VALUE,
    [WM8978_REG_COMPANDING_CONTROL] = WM8978_R05_RESET_VALUE,
    [WM8978_REG_CLOCK_GENERATION] = WM8978_R06_RESET_VALUE,
    [WM8978_REG_ADDITIONAL_CONTROL] = WM8978_R07_RESET_VALUE,
    [WM8978_REG_GPIO] = WM8978_R08_RESET_VALUE,
    [WM8978_REG_JACK_DETECT_1] = WM8978_R09_RESET_VALUE,
    [WM8978_REG_DAC_CONTROL] = WM8978_R10_RESET_VALUE,
    [WM8978_REG_LEFT_DAC_VOLUME] = WM8978_R11_RESET_VALUE,
    [WM8978_REG_RIGHT_DAC_VOLUME] = WM8978_R12_RESET_VALUE,
    [WM8978_REG_JACK_DETECT_2] = WM8978_R13_RESET_VALUE,
    [WM8978_REG_ADC_CONTROL] = WM8978_R14_RESET_VALUE,
    [WM8978_REG_LEFT_ADC_VOLUME] = WM8978_R15_RESET_VALUE,
    [WM8978_REG_RIGHT_ADC_VOLUME] = WM8978_R16_RESET_VALUE,
    [WM8978_REG_EQ1] = WM8978_R18_RESET_VALUE,
    [WM8978_REG_EQ2] = WM8978_R19_RESET_VALUE,
    [WM8978_REG_EQ3] = WM8978_R20_RESET_VALUE,
    [WM8978_REG_EQ4] = WM8978_R21_RESET_VALUE,
    [WM8978_REG_EQ5] = WM8978_R22_RESET_VALUE,
    [WM8978_REG_DAC_LIMITER_1] = WM8978_R24_RESET_VALUE,
    [WM8978_REG_DAC_LIMITER_2] = WM8978_R25_RESET_VALUE,
    [WM8978_REG_NOTCH_FILTER_1] = WM8978_R27_RESET_VALUE,
    [WM8978_REG_NOTCH_FILTER_2] = WM8978_R28_RESET_VALUE,
    [WM8978_REG_NOTCH_FILTER_3] = WM8978_R29_RESET_VALUE,
    [WM8978_REG_NOTCH_FILTER_4] = WM8978_R30_RESET_VALUE,
    [WM8978_REG_ALC_CONTROL_1] = WM8978_R32_RESET_VALUE,
    [WM8978_REG_ALC_CONTROL_2] = WM8978_R33_RESET_VALUE,
    [WM8978_REG_ALC_CONTROL_3] = WM8978_R34_RESET_VALUE,
    [WM8978_REG_NOISE_GATE] = WM8978_R35_RESET_VALUE,
    [WM8978_REG_PLL_N] = WM8978_R36_RESET_VALUE,
    [WM8978_REG_PLL_K1] = WM8978_R37_RESET_VALUE,
    [WM8978_REG_PLL_K2] = WM8978_R38_RESET_VALUE,
    [WM8978_REG_PLL_K3] = WM8978_R39_RESET_VALUE,
    [WM8978_REG_3D_CONTROL] = WM8978_R41_RESET_VALUE,
    [WM8978_REG_BEEP_CONTROL] = WM8978_R43_RESET_VALUE,
    [WM8978_REG_INPUT_CONTROL] = WM8978_R44_RESET_VALUE,
    [WM8978_REG_LEFT_INPUT_PGA] = WM8978_R45_RESET_VALUE,
    [WM8978_REG_RIGHT_INPUT_PGA] = WM8978_R46_RESET_VALUE,
    [WM8978_REG_LEFT_ADC_BOOST] = WM8978_R47_RESET_VALUE,
    [WM8978_REG_RIGHT_ADC_BOOST] = WM8978_R48_RESET_VALUE,
    [WM8978_REG_OUTPUT_CONTROL] = WM8978_R49_RESET_VALUE,
    [WM8978_REG_LEFT_MIXER] = WM8978_R50_RESET_VALUE,
    [WM8978_REG_RIGHT_MIXER] = WM8978_R51_RESET_VALUE,
    [WM8978_REG_LEFT_HEADPHONE_VOLUME] = WM8978_R52_RESET_VALUE,
    [WM8978_REG_RIGHT_HEADPHONE_VOLUME] = WM8978_R53_RESET_VALUE,
    [WM8978_REG_LEFT_SPEAKER_VOLUME] = WM8978_R54_RESET_VALUE,
    [WM8978_REG_RIGHT_SPEAKER_VOLUME] = WM8978_R55_RESET_VALUE,
    [WM8978_REG_OUT3_MIXER] = WM8978_R56_RESET_VALUE,
    [WM8978_REG_OUT4_MIXER] = WM8978_R57_RESET_VALUE
};

/* 掩码未覆盖的位必须保持复位值。 */
static const uint16_t wm8978_writable_masks[WM8978_REGISTER_SPACE_SIZE] =
{
    [WM8978_REG_SOFTWARE_RESET] = 0x1FFU,
    [WM8978_REG_POWER_MANAGEMENT_1] = 0x1FFU,
    [WM8978_REG_POWER_MANAGEMENT_2] = 0x1FFU,
    [WM8978_REG_POWER_MANAGEMENT_3] = 0x1EFU,
    [WM8978_REG_AUDIO_INTERFACE] = 0x1FFU,
    [WM8978_REG_COMPANDING_CONTROL] = 0x03FU,
    [WM8978_REG_CLOCK_GENERATION] = 0x1FDU,
    [WM8978_REG_ADDITIONAL_CONTROL] = 0x00FU,
    [WM8978_REG_GPIO] = 0x03FU,
    [WM8978_REG_JACK_DETECT_1] = 0x1F0U,
    [WM8978_REG_DAC_CONTROL] = 0x04FU,
    [WM8978_REG_LEFT_DAC_VOLUME] = 0x1FFU,
    [WM8978_REG_RIGHT_DAC_VOLUME] = 0x1FFU,
    [WM8978_REG_JACK_DETECT_2] = 0x0FFU,
    [WM8978_REG_ADC_CONTROL] = 0x1FBU,
    [WM8978_REG_LEFT_ADC_VOLUME] = 0x1FFU,
    [WM8978_REG_RIGHT_ADC_VOLUME] = 0x1FFU,
    [WM8978_REG_EQ1] = 0x17FU,
    [WM8978_REG_EQ2] = 0x17FU,
    [WM8978_REG_EQ3] = 0x17FU,
    [WM8978_REG_EQ4] = 0x17FU,
    [WM8978_REG_EQ5] = 0x07FU,
    [WM8978_REG_DAC_LIMITER_1] = 0x1FFU,
    [WM8978_REG_DAC_LIMITER_2] = 0x07FU,
    [WM8978_REG_NOTCH_FILTER_1] = 0x1FFU,
    [WM8978_REG_NOTCH_FILTER_2] = 0x17FU,
    [WM8978_REG_NOTCH_FILTER_3] = 0x17FU,
    [WM8978_REG_NOTCH_FILTER_4] = 0x17FU,
    [WM8978_REG_ALC_CONTROL_1] = 0x1BFU,
    [WM8978_REG_ALC_CONTROL_2] = 0x0FFU,
    [WM8978_REG_ALC_CONTROL_3] = 0x1FFU,
    [WM8978_REG_NOISE_GATE] = 0x00FU,
    [WM8978_REG_PLL_N] = 0x01FU,
    [WM8978_REG_PLL_K1] = 0x03FU,
    [WM8978_REG_PLL_K2] = 0x1FFU,
    [WM8978_REG_PLL_K3] = 0x1FFU,
    [WM8978_REG_3D_CONTROL] = 0x00FU,
    [WM8978_REG_BEEP_CONTROL] = 0x03FU,
    [WM8978_REG_INPUT_CONTROL] = 0x177U,
    [WM8978_REG_LEFT_INPUT_PGA] = 0x1FFU,
    [WM8978_REG_RIGHT_INPUT_PGA] = 0x1FFU,
    [WM8978_REG_LEFT_ADC_BOOST] = 0x177U,
    [WM8978_REG_RIGHT_ADC_BOOST] = 0x177U,
    [WM8978_REG_OUTPUT_CONTROL] = 0x07FU,
    [WM8978_REG_LEFT_MIXER] = 0x1FFU,
    [WM8978_REG_RIGHT_MIXER] = 0x1FFU,
    [WM8978_REG_LEFT_HEADPHONE_VOLUME] = 0x1FFU,
    [WM8978_REG_RIGHT_HEADPHONE_VOLUME] = 0x1FFU,
    [WM8978_REG_LEFT_SPEAKER_VOLUME] = 0x1FFU,
    [WM8978_REG_RIGHT_SPEAKER_VOLUME] = 0x1FFU,
    [WM8978_REG_OUT3_MIXER] = 0x04FU,
    [WM8978_REG_OUT4_MIXER] = 0x07FU
};

/* 硬件非锁存触发位，以及 NFU 的保守一次性策略。 */
static const uint16_t wm8978_transient_masks[WM8978_REGISTER_SPACE_SIZE] =
{
    [WM8978_REG_LEFT_DAC_VOLUME] = 0x100U,
    [WM8978_REG_RIGHT_DAC_VOLUME] = 0x100U,
    [WM8978_REG_LEFT_ADC_VOLUME] = 0x100U,
    [WM8978_REG_RIGHT_ADC_VOLUME] = 0x100U,
    [WM8978_REG_NOTCH_FILTER_1] = 0x100U,
    [WM8978_REG_NOTCH_FILTER_2] = 0x100U,
    [WM8978_REG_NOTCH_FILTER_3] = 0x100U,
    [WM8978_REG_NOTCH_FILTER_4] = 0x100U,
    [WM8978_REG_LEFT_INPUT_PGA] = 0x100U,
    [WM8978_REG_RIGHT_INPUT_PGA] = 0x100U,
    [WM8978_REG_LEFT_HEADPHONE_VOLUME] = 0x100U,
    [WM8978_REG_RIGHT_HEADPHONE_VOLUME] = 0x100U,
    [WM8978_REG_LEFT_SPEAKER_VOLUME] = 0x100U,
    [WM8978_REG_RIGHT_SPEAKER_VOLUME] = 0x100U
};

/* ══════════════════════════ 私有辅助函数 ══════════════════════════ */

/**
 * @brief   校验实例已绑定。
 * @param   device 驱动实例。
 * @retval  WM8978_OK 实例处于任一已绑定生命周期。
 * @retval  WM8978_ERR_NULL_POINTER device 为 NULL。
 * @retval  WM8978_ERR_NOT_BOUND 实例尚未绑定。
 */
static wm8978_status_t
wm8978_require_bound(const wm8978_t * device)
{
    if (device == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if ((device->lifecycle != WM8978_LIFECYCLE_BOUND) &&
        (device->lifecycle != WM8978_LIFECYCLE_READY) &&
        (device->lifecycle != WM8978_LIFECYCLE_DESYNCHRONIZED))
    {
        return WM8978_ERR_NOT_BOUND;
    }

    return WM8978_OK;
}

/**
 * @brief   校验实例已绑定且影子与芯片同步。
 * @param   device 驱动实例。
 * @retval  WM8978_OK 实例处于 READY 生命周期。
 * @retval  WM8978_ERR_NULL_POINTER device 为 NULL。
 * @retval  WM8978_ERR_NOT_BOUND 实例尚未绑定。
 * @retval  WM8978_ERR_DESYNCHRONIZED 实例失步，须复位恢复。
 * @retval  WM8978_ERR_NOT_READY 已绑定但未完成复位同步。
 */
static wm8978_status_t
wm8978_require_ready(const wm8978_t * device)
{
    wm8978_status_t status;

    status = wm8978_require_bound(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    if (device->lifecycle == WM8978_LIFECYCLE_DESYNCHRONIZED)
    {
        return WM8978_ERR_DESYNCHRONIZED;
    }

    if (device->lifecycle != WM8978_LIFECYCLE_READY)
    {
        return WM8978_ERR_NOT_READY;
    }

    return WM8978_OK;
}

/**
 * @brief   把影子整体装载为数据手册复位默认值并进入 READY。
 * @param   device 驱动实例（须已绑定）。
 */
static void
wm8978_load_reset_defaults(wm8978_t * device)
{
    uint8_t address;

    for (address = 0U; address < WM8978_REGISTER_SPACE_SIZE; ++address)
    {
        device->shadow[address] = wm8978_reset_defaults[address];
    }
    device->lifecycle = WM8978_LIFECYCLE_READY;
}

/**
 * @brief   打包并发送一个控制帧，失败时把实例锁定为失步。
 * @param   device 驱动实例。
 * @param   register_address 寄存器地址。
 * @param   value 9 位寄存器值。
 * @retval  WM8978_OK 控制帧发送成功。
 * @retval  WM8978_ERR_INVALID_REGISTER 地址无效。
 * @retval  WM8978_ERR_RANGE value 超出 9 位。
 * @retval  WM8978_ERR_IO io 层返回非零（实例进入 DESYNCHRONIZED，
 *          原始错误记录在 last_port_error）。
 */
static wm8978_status_t
wm8978_send_control(wm8978_t * device,
                    uint8_t register_address,
                    uint16_t value)
{
    wm8978_status_t status;
    int32_t port_status;
    uint8_t frame[2];

    status = wm8978_pack_control_frame(register_address, value, frame);
    if (status != WM8978_OK)
    {
        return status;
    }

    port_status = wm8978_io_write_control(device->io_ctx,
                                           frame[0],
                                           frame[1],
                                           device->io_timeout_ms);
    device->last_port_error = port_status;
    if (port_status != 0)
    {
        device->lifecycle = WM8978_LIFECYCLE_DESYNCHRONIZED;
        return WM8978_ERR_IO;
    }

    return WM8978_OK;
}

/**
 * @brief   校验完整寄存器值：地址有效、不超 9 位、保留位等于复位值。
 * @param   register_address 寄存器地址。
 * @param   value 待校验的完整寄存器值。
 * @retval  WM8978_OK 校验通过。
 * @retval  WM8978_ERR_INVALID_REGISTER 地址无效。
 * @retval  WM8978_ERR_RANGE value 超出 9 位。
 * @retval  WM8978_ERR_RESERVED_BITS 保留位偏离复位值。
 */
static wm8978_status_t
wm8978_validate_complete_value(
    uint8_t register_address,
    uint16_t value)
{
    uint16_t reserved_mask;

    if (!wm8978_register_is_valid(register_address))
    {
        return WM8978_ERR_INVALID_REGISTER;
    }

    if ((value & (uint16_t)(~WM8978_REGISTER_VALUE_MASK)) != 0U)
    {
        return WM8978_ERR_RANGE;
    }

    reserved_mask = (uint16_t)(WM8978_REGISTER_VALUE_MASK &
                               (uint16_t)(~wm8978_writable_masks[register_address]));
    if ((value & reserved_mask) !=
        (wm8978_reset_defaults[register_address] & reserved_mask))
    {
        return WM8978_ERR_RESERVED_BITS;
    }

    return WM8978_OK;
}

/**
 * @brief   提交一次写入：EQ3DMODE 状态检查、发送控制帧并更新影子。
 * @details 发送成功后按非锁存触发位掩码清除影子中的 VU/UPDATE/NFU
 *          位，使影子只保留可锁存的值。
 * @param   device 驱动实例（须处于 READY）。
 * @param   register_address 寄存器地址。
 * @param   wire_value 发送到总线的完整 9 位值。
 * @retval  WM8978_OK 写入成功且影子已提交。
 * @retval  WM8978_ERR_STATE 在 ADC/DAC 使能时改变 R18 EQ3DMODE。
 * @retval  WM8978_ERR_IO 控制帧发送失败（实例失步）。
 */
static wm8978_status_t
wm8978_write_and_commit(wm8978_t * device,
                        uint8_t register_address,
                        uint16_t wire_value)
{
    wm8978_status_t status;

    if ((register_address == WM8978_REG_EQ1) &&
        (((wire_value ^ device->shadow[WM8978_REG_EQ1]) &
          WM8978_R18_EQ3DMODE) != 0U) &&
        ((((device->shadow[WM8978_REG_POWER_MANAGEMENT_2]) &
           (WM8978_R02_ADCENR | WM8978_R02_ADCENL)) != 0U) ||
         (((device->shadow[WM8978_REG_POWER_MANAGEMENT_3]) &
           (WM8978_R03_DACENR | WM8978_R03_DACENL)) != 0U)))
    {
        return WM8978_ERR_STATE;
    }

    status = wm8978_send_control(device, register_address, wire_value);
    if (status != WM8978_OK)
    {
        return status;
    }

    device->shadow[register_address] = (uint16_t)(
        wire_value & (uint16_t)(~wm8978_transient_masks[register_address]));
    return WM8978_OK;
}

/**
 * @brief   同步更新立体声寄存器对：先写左声道（触发位清 0），再写
 *          右声道（触发位置 1）使左右同时生效。
 * @param   device 驱动实例。
 * @param   left_register 左声道寄存器地址。
 * @param   right_register 右声道寄存器地址。
 * @param   left_value 左声道寄存器值（触发位清 0）。
 * @param   right_value 右声道寄存器值（触发位置 1）。
 * @param   update_bit 该寄存器对的非锁存触发位。
 * @retval  WM8978_OK 两帧都成功。
 * @retval  WM8978_ERR_IO 第二帧失败时左声道可能已更新（实例失步）。
 * @retval  其余同 wm8978_write_register() 的校验结果。
 */
static wm8978_status_t
wm8978_write_stereo_update(wm8978_t * device,
                           uint8_t left_register,
                           uint8_t right_register,
                           uint16_t left_value,
                           uint16_t right_value,
                           uint16_t update_bit)
{
    wm8978_status_t status;

    status = wm8978_write_register(device,
                                    left_register,
                                    (uint16_t)(left_value &
                                               (uint16_t)(~update_bit)));
    if (status != WM8978_OK)
    {
        return status;
    }

    return wm8978_write_register(device,
                                  right_register,
                                  (uint16_t)(right_value | update_bit));
}

/* ══════════════════════════ 公共 API ══════════════════════════ */

/**
 * @brief 见 wm8978.h 中 wm8978_bind() 的完整契约。
 */
wm8978_status_t
wm8978_bind(wm8978_t * device,
            void * io_ctx,
            uint32_t io_timeout_ms)
{
    uint8_t address;

    if (device == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if (io_timeout_ms == 0U)
    {
        return WM8978_ERR_INVALID_ARGUMENT;
    }

    device->io_ctx = io_ctx;
    device->io_timeout_ms = io_timeout_ms;
    device->lifecycle = WM8978_LIFECYCLE_BOUND;
    device->last_port_error = 0;
    for (address = 0U; address < WM8978_REGISTER_SPACE_SIZE; ++address)
    {
        device->shadow[address] = 0U;
    }

    return WM8978_OK;
}

/**
 * @brief 见 wm8978.h 中 wm8978_assume_power_on_reset() 的完整契约。
 */
wm8978_status_t
wm8978_assume_power_on_reset(wm8978_t * device)
{
    wm8978_status_t status;

    status = wm8978_require_bound(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    wm8978_load_reset_defaults(device);
    return WM8978_OK;
}

/**
 * @brief 见 wm8978.h 中 wm8978_soft_reset() 的完整契约。
 */
wm8978_status_t
wm8978_soft_reset(wm8978_t * device)
{
    wm8978_status_t status;

    status = wm8978_require_bound(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_send_control(device, WM8978_REG_SOFTWARE_RESET, 0U);
    if (status != WM8978_OK)
    {
        return status;
    }

    wm8978_load_reset_defaults(device);
    return WM8978_OK;
}

/**
 * @brief 见 wm8978.h 中 wm8978_get_lifecycle() 的完整契约。
 */
wm8978_lifecycle_t
wm8978_get_lifecycle(const wm8978_t * device)
{
    if (device == NULL)
    {
        return WM8978_LIFECYCLE_UNBOUND;
    }

    return device->lifecycle;
}

/**
 * @brief 见 wm8978.h 中 wm8978_get_last_port_error() 的完整契约。
 */
int32_t
wm8978_get_last_port_error(const wm8978_t * device)
{
    if (device == NULL)
    {
        return 0;
    }

    return device->last_port_error;
}

/**
 * @brief 见 wm8978.h 中 wm8978_register_is_valid() 的完整契约。
 */
bool
wm8978_register_is_valid(uint8_t register_address)
{
    return (register_address < WM8978_REGISTER_SPACE_SIZE) &&
           (wm8978_valid_registers[register_address] != 0U);
}

/**
 * @brief 见 wm8978.h 中 wm8978_pack_control_frame() 的完整契约。
 */
wm8978_status_t
wm8978_pack_control_frame(uint8_t register_address,
                          uint16_t value,
                          uint8_t frame[2])
{
    if (frame == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if (!wm8978_register_is_valid(register_address))
    {
        return WM8978_ERR_INVALID_REGISTER;
    }

    if ((value & (uint16_t)(~WM8978_REGISTER_VALUE_MASK)) != 0U)
    {
        return WM8978_ERR_RANGE;
    }

    frame[0] = (uint8_t)(((uint16_t)register_address << 1U) |
                         ((value >> 8U) & 0x01U));
    frame[1] = (uint8_t)(value & 0x00FFU);
    return WM8978_OK;
}

/**
 * @brief 见 wm8978.h 中 wm8978_get_shadow_register() 的完整契约。
 */
wm8978_status_t
wm8978_get_shadow_register(const wm8978_t * device,
                           uint8_t register_address,
                           uint16_t * value)
{
    wm8978_status_t status;

    if (value == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    if (!wm8978_register_is_valid(register_address))
    {
        return WM8978_ERR_INVALID_REGISTER;
    }

    if (register_address == WM8978_REG_SOFTWARE_RESET)
    {
        return WM8978_ERR_NO_SHADOW;
    }

    *value = device->shadow[register_address];
    return WM8978_OK;
}

/**
 * @brief 见 wm8978.h 中 wm8978_write_register() 的完整契约。
 */
wm8978_status_t
wm8978_write_register(wm8978_t * device,
                      uint8_t register_address,
                      uint16_t value)
{
    wm8978_status_t status;

    status = wm8978_validate_complete_value(register_address, value);
    if (status != WM8978_OK)
    {
        return status;
    }

    if (register_address == WM8978_REG_SOFTWARE_RESET)
    {
        status = wm8978_require_bound(device);
        if (status != WM8978_OK)
        {
            return status;
        }

        status = wm8978_send_control(device, register_address, value);
        if (status == WM8978_OK)
        {
            wm8978_load_reset_defaults(device);
        }
        return status;
    }

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    return wm8978_write_and_commit(device, register_address, value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_update_bits() 的完整契约。
 */
wm8978_status_t
wm8978_update_bits(wm8978_t * device,
                   uint8_t register_address,
                   uint16_t mask,
                   uint16_t field_value)
{
    wm8978_status_t status;
    uint16_t old_value;
    uint16_t wire_value;
    bool transient_requested;

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    if (!wm8978_register_is_valid(register_address) ||
        (register_address == WM8978_REG_SOFTWARE_RESET))
    {
        return WM8978_ERR_INVALID_REGISTER;
    }

    if ((mask == 0U) ||
        ((mask & (uint16_t)(~WM8978_REGISTER_VALUE_MASK)) != 0U) ||
        ((field_value & (uint16_t)(~mask)) != 0U))
    {
        return WM8978_ERR_INVALID_ARGUMENT;
    }

    if ((mask & (uint16_t)(~wm8978_writable_masks[register_address])) != 0U)
    {
        return WM8978_ERR_RESERVED_BITS;
    }

    old_value = device->shadow[register_address];
    wire_value = (uint16_t)((old_value & (uint16_t)(~mask)) |
                            field_value);
    transient_requested =
        (wire_value & wm8978_transient_masks[register_address]) != 0U;

    if (!transient_requested && (wire_value == old_value))
    {
        return WM8978_OK;
    }

    return wm8978_write_and_commit(device, register_address, wire_value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_configure_audio_interface() 的完整契约。
 */
wm8978_status_t
wm8978_configure_audio_interface(
    wm8978_t * device,
    const wm8978_audio_interface_config_t * config)
{
    uint16_t value;

    if (config == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if (((uint32_t)config->format >
         (uint32_t)WM8978_AUDIO_FORMAT_DSP_PCM) ||
        ((uint32_t)config->word_length >
         (uint32_t)WM8978_WORD_LENGTH_32_BITS))
    {
        return WM8978_ERR_RANGE;
    }

    if ((config->format == WM8978_AUDIO_FORMAT_RIGHT_JUSTIFIED) &&
        (config->word_length == WM8978_WORD_LENGTH_32_BITS))
    {
        return WM8978_ERR_UNSUPPORTED;
    }

    if (((config->format == WM8978_AUDIO_FORMAT_DSP_PCM) &&
         config->invert_lrc) ||
        ((config->format != WM8978_AUDIO_FORMAT_DSP_PCM) &&
         config->dsp_mode_b))
    {
        return WM8978_ERR_INVALID_ARGUMENT;
    }

    value = WM8978_FIELD_PREP(WM8978_R04_FMT_MASK,
                               WM8978_R04_FMT_SHIFT,
                               config->format);
    value |= WM8978_FIELD_PREP(WM8978_R04_WL_MASK,
                                WM8978_R04_WL_SHIFT,
                                config->word_length);
    value |= config->invert_bclk ? WM8978_R04_BCP : 0U;
    value |= ((config->format == WM8978_AUDIO_FORMAT_DSP_PCM) ?
              config->dsp_mode_b : config->invert_lrc) ?
             WM8978_R04_LRP : 0U;
    value |= config->swap_dac_channels ? WM8978_R04_DACLRSWAP : 0U;
    value |= config->swap_adc_channels ? WM8978_R04_ADCLRSWAP : 0U;
    value |= config->mono ? WM8978_R04_MONO : 0U;

    return wm8978_write_register(device, WM8978_REG_AUDIO_INTERFACE, value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_configure_clock() 的完整契约。
 */
wm8978_status_t
wm8978_configure_clock(
    wm8978_t * device,
    const wm8978_clock_config_t * config)
{
    wm8978_status_t status;
    uint16_t value;
    uint16_t mask;

    if (config == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if (((uint32_t)config->mclk_divider >
         (uint32_t)WM8978_MCLK_DIV_12) ||
        ((uint32_t)config->bclk_divider >
         (uint32_t)WM8978_BCLK_DIV_32))
    {
        return WM8978_ERR_RANGE;
    }

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    if (config->use_pll &&
        (((device->shadow[WM8978_REG_POWER_MANAGEMENT_1] &
           WM8978_R01_PLLEN) == 0U) ||
         ((device->shadow[WM8978_REG_POWER_MANAGEMENT_1] &
           WM8978_R01_VMIDSEL_MASK) == 0U)))
    {
        return WM8978_ERR_STATE;
    }

    value = WM8978_FIELD_PREP(WM8978_R06_MCLKDIV_MASK,
                               WM8978_R06_MCLKDIV_SHIFT,
                               config->mclk_divider);
    value |= WM8978_FIELD_PREP(WM8978_R06_BCLKDIV_MASK,
                                WM8978_R06_BCLKDIV_SHIFT,
                                config->bclk_divider);
    value |= config->use_pll ? WM8978_R06_CLKSEL : 0U;
    value |= config->codec_is_master ? WM8978_R06_MS : 0U;
    mask = (uint16_t)(WM8978_R06_CLKSEL |
                      WM8978_R06_MCLKDIV_MASK |
                      WM8978_R06_BCLKDIV_MASK |
                      WM8978_R06_MS);

    return wm8978_update_bits(device,
                               WM8978_REG_CLOCK_GENERATION,
                               mask,
                               value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_set_filter_sample_rate() 的完整契约。
 */
wm8978_status_t
wm8978_set_filter_sample_rate(
    wm8978_t * device,
    wm8978_filter_sample_rate_t sample_rate_group)
{
    uint16_t value;

    if ((uint32_t)sample_rate_group >
        (uint32_t)WM8978_FILTER_SR_8_KHZ)
    {
        return WM8978_ERR_RANGE;
    }

    value = WM8978_FIELD_PREP(WM8978_R07_SR_MASK,
                               WM8978_R07_SR_SHIFT,
                               sample_rate_group);
    return wm8978_update_bits(device,
                               WM8978_REG_ADDITIONAL_CONTROL,
                               WM8978_R07_SR_MASK,
                               value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_configure_pll() 的完整契约。
 */
wm8978_status_t
wm8978_configure_pll(wm8978_t * device,
                     const wm8978_pll_config_t * config)
{
    wm8978_status_t status;
    uint16_t value;

    if (config == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if ((config->n < 6U) || (config->n > 12U) ||
        (config->k > 0x00FFFFFFUL))
    {
        return WM8978_ERR_RANGE;
    }

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    if (((device->shadow[WM8978_REG_POWER_MANAGEMENT_1] &
          WM8978_R01_PLLEN) != 0U) ||
        ((device->shadow[WM8978_REG_CLOCK_GENERATION] &
          WM8978_R06_CLKSEL) != 0U))
    {
        return WM8978_ERR_STATE;
    }

    value = (uint16_t)((config->k >> 18U) & 0x3FU);
    status = wm8978_write_register(device, WM8978_REG_PLL_K1, value);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = (uint16_t)((config->k >> 9U) & 0x1FFU);
    status = wm8978_write_register(device, WM8978_REG_PLL_K2, value);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = (uint16_t)(config->k & 0x1FFU);
    status = wm8978_write_register(device, WM8978_REG_PLL_K3, value);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = (uint16_t)(config->n & 0x0FU);
    value |= config->divide_mclk_by_2 ? WM8978_R36_PLLPRESCALE : 0U;
    return wm8978_write_register(device, WM8978_REG_PLL_N, value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_set_pll_enabled() 的完整契约。
 */
wm8978_status_t
wm8978_set_pll_enabled(wm8978_t * device, bool enabled)
{
    wm8978_status_t status;
    uint16_t source;
    uint16_t vmid;

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    source = device->shadow[WM8978_REG_CLOCK_GENERATION] &
             WM8978_R06_CLKSEL;
    if (source != 0U)
    {
        return WM8978_ERR_STATE;
    }

    if (enabled)
    {
        vmid = device->shadow[WM8978_REG_POWER_MANAGEMENT_1] &
               WM8978_R01_VMIDSEL_MASK;
        if (vmid == 0U)
        {
            return WM8978_ERR_STATE;
        }
    }

    return wm8978_update_bits(device,
                               WM8978_REG_POWER_MANAGEMENT_1,
                               WM8978_R01_PLLEN,
                               enabled ? WM8978_R01_PLLEN : 0U);
}

/**
 * @brief 见 wm8978.h 中 wm8978_set_dac_digital_volume() 的完整契约。
 */
wm8978_status_t
wm8978_set_dac_digital_volume(wm8978_t * device,
                              uint8_t left_code,
                              uint8_t right_code)
{
    return wm8978_write_stereo_update(device,
                                       WM8978_REG_LEFT_DAC_VOLUME,
                                       WM8978_REG_RIGHT_DAC_VOLUME,
                                       left_code,
                                       right_code,
                                       WM8978_CONVERTER_VU);
}

/**
 * @brief 见 wm8978.h 中 wm8978_set_adc_digital_volume() 的完整契约。
 */
wm8978_status_t
wm8978_set_adc_digital_volume(wm8978_t * device,
                              uint8_t left_code,
                              uint8_t right_code)
{
    return wm8978_write_stereo_update(device,
                                       WM8978_REG_LEFT_ADC_VOLUME,
                                       WM8978_REG_RIGHT_ADC_VOLUME,
                                       left_code,
                                       right_code,
                                       WM8978_CONVERTER_VU);
}

/**
 * @brief 见 wm8978.h 中 wm8978_set_input_pga() 的完整契约。
 */
wm8978_status_t
wm8978_set_input_pga(
    wm8978_t * device,
    const wm8978_input_pga_config_t * config)
{
    uint16_t left_value;
    uint16_t right_value;

    if (config == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if ((config->left_volume_code > 63U) ||
        (config->right_volume_code > 63U))
    {
        return WM8978_ERR_RANGE;
    }

    left_value = config->left_volume_code;
    right_value = config->right_volume_code;
    if (config->mute)
    {
        left_value |= WM8978_INPUT_PGA_MUTE;
        right_value |= WM8978_INPUT_PGA_MUTE;
    }
    if (config->zero_cross)
    {
        left_value |= WM8978_INPUT_PGA_ZC;
        right_value |= WM8978_INPUT_PGA_ZC;
    }

    return wm8978_write_stereo_update(device,
                                       WM8978_REG_LEFT_INPUT_PGA,
                                       WM8978_REG_RIGHT_INPUT_PGA,
                                       left_value,
                                       right_value,
                                       WM8978_INPUT_PGA_UPDATE);
}

/**
 * @brief 见 wm8978.h 中 wm8978_set_output_volume() 的完整契约。
 */
wm8978_status_t
wm8978_set_output_volume(
    wm8978_t * device,
    wm8978_output_pair_t output,
    const wm8978_output_volume_config_t * config)
{
    uint8_t left_register;
    uint8_t right_register;
    uint16_t left_value;
    uint16_t right_value;

    if (config == NULL)
    {
        return WM8978_ERR_NULL_POINTER;
    }

    if ((config->left_volume_code > 63U) ||
        (config->right_volume_code > 63U))
    {
        return WM8978_ERR_RANGE;
    }

    switch (output)
    {
        case WM8978_OUTPUT_HEADPHONE:
            left_register = WM8978_REG_LEFT_HEADPHONE_VOLUME;
            right_register = WM8978_REG_RIGHT_HEADPHONE_VOLUME;
            break;
        case WM8978_OUTPUT_SPEAKER:
            left_register = WM8978_REG_LEFT_SPEAKER_VOLUME;
            right_register = WM8978_REG_RIGHT_SPEAKER_VOLUME;
            break;
        default:
            return WM8978_ERR_RANGE;
    }

    left_value = config->left_volume_code;
    right_value = config->right_volume_code;
    if (config->mute)
    {
        left_value |= WM8978_OUTPUT_MUTE;
        right_value |= WM8978_OUTPUT_MUTE;
    }
    if (config->zero_cross)
    {
        left_value |= WM8978_OUTPUT_ZC;
        right_value |= WM8978_OUTPUT_ZC;
    }

    return wm8978_write_stereo_update(device,
                                       left_register,
                                       right_register,
                                       left_value,
                                       right_value,
                                       WM8978_OUTPUT_VU);
}

/**
 * @brief 见 wm8978.h 中 wm8978_mute_analogue_outputs() 的完整契约。
 */
wm8978_status_t
wm8978_mute_analogue_outputs(wm8978_t * device, bool mute)
{
    wm8978_status_t status;
    uint16_t left_value;
    uint16_t right_value;
    uint16_t value;

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    left_value = device->shadow[WM8978_REG_LEFT_HEADPHONE_VOLUME];
    right_value = device->shadow[WM8978_REG_RIGHT_HEADPHONE_VOLUME];
    if (mute)
    {
        left_value = (uint16_t)((left_value | WM8978_OUTPUT_MUTE) &
                                (uint16_t)(~WM8978_OUTPUT_ZC));
        right_value = (uint16_t)((right_value | WM8978_OUTPUT_MUTE) &
                                 (uint16_t)(~WM8978_OUTPUT_ZC));
    }
    else
    {
        left_value &= (uint16_t)(~WM8978_OUTPUT_MUTE);
        right_value &= (uint16_t)(~WM8978_OUTPUT_MUTE);
    }
    status = wm8978_write_stereo_update(device,
                                         WM8978_REG_LEFT_HEADPHONE_VOLUME,
                                         WM8978_REG_RIGHT_HEADPHONE_VOLUME,
                                         left_value,
                                         right_value,
                                         WM8978_OUTPUT_VU);
    if (status != WM8978_OK)
    {
        return status;
    }

    left_value = device->shadow[WM8978_REG_LEFT_SPEAKER_VOLUME];
    right_value = device->shadow[WM8978_REG_RIGHT_SPEAKER_VOLUME];
    if (mute)
    {
        left_value = (uint16_t)((left_value | WM8978_OUTPUT_MUTE) &
                                (uint16_t)(~WM8978_OUTPUT_ZC));
        right_value = (uint16_t)((right_value | WM8978_OUTPUT_MUTE) &
                                 (uint16_t)(~WM8978_OUTPUT_ZC));
    }
    else
    {
        left_value &= (uint16_t)(~WM8978_OUTPUT_MUTE);
        right_value &= (uint16_t)(~WM8978_OUTPUT_MUTE);
    }
    status = wm8978_write_stereo_update(device,
                                         WM8978_REG_LEFT_SPEAKER_VOLUME,
                                         WM8978_REG_RIGHT_SPEAKER_VOLUME,
                                         left_value,
                                         right_value,
                                         WM8978_OUTPUT_VU);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = device->shadow[WM8978_REG_OUT3_MIXER];
    value = mute ? (uint16_t)(value | WM8978_R56_OUT3MUTE) :
                   (uint16_t)(value & (uint16_t)(~WM8978_R56_OUT3MUTE));
    status = wm8978_write_register(device, WM8978_REG_OUT3_MIXER, value);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = device->shadow[WM8978_REG_OUT4_MIXER];
    value = mute ? (uint16_t)(value | WM8978_R57_OUT4MUTE) :
                   (uint16_t)(value & (uint16_t)(~WM8978_R57_OUT4MUTE));
    return wm8978_write_register(device, WM8978_REG_OUT4_MIXER, value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_power_up_nonboost_out1() 的完整契约。
 */
wm8978_status_t
wm8978_power_up_nonboost_out1(wm8978_t * device,
                              wm8978_vmid_t vmid,
                              uint32_t vmid_settle_ms)
{
    wm8978_status_t status;
    uint16_t value;
    uint16_t boost_mask;

    if ((vmid == WM8978_VMID_OFF) ||
        ((uint32_t)vmid > (uint32_t)WM8978_VMID_5K) ||
        (vmid_settle_ms == 0U))
    {
        return WM8978_ERR_INVALID_ARGUMENT;
    }

    status = wm8978_require_ready(device);
    if (status != WM8978_OK)
    {
        return status;
    }

    if ((device->shadow[WM8978_REG_POWER_MANAGEMENT_1] != 0U) ||
        (device->shadow[WM8978_REG_POWER_MANAGEMENT_2] != 0U) ||
        (device->shadow[WM8978_REG_POWER_MANAGEMENT_3] != 0U))
    {
        return WM8978_ERR_STATE;
    }

    boost_mask = (uint16_t)(WM8978_R49_SPKBOOST |
                            WM8978_R49_OUT3BOOST |
                            WM8978_R49_OUT4BOOST);
    if (((device->shadow[WM8978_REG_OUTPUT_CONTROL] & boost_mask) != 0U) ||
        ((device->shadow[WM8978_REG_POWER_MANAGEMENT_1] &
          WM8978_R01_BUFDCOPEN) != 0U))
    {
        return WM8978_ERR_STATE;
    }

    status = wm8978_mute_analogue_outputs(device, true);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = (uint16_t)(WM8978_R03_RMIXEN |
                       WM8978_R03_LMIXEN |
                       WM8978_R03_DACENR |
                       WM8978_R03_DACENL);
    status = wm8978_write_register(device,
                                    WM8978_REG_POWER_MANAGEMENT_3,
                                    value);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = (uint16_t)(WM8978_R01_BUFIOEN |
                       (uint16_t)vmid);
    status = wm8978_write_register(device,
                                    WM8978_REG_POWER_MANAGEMENT_1,
                                    value);
    if (status != WM8978_OK)
    {
        return status;
    }

    wm8978_io_delay_ms(device->io_ctx, vmid_settle_ms);

    value = (uint16_t)(device->shadow[WM8978_REG_POWER_MANAGEMENT_1] |
                       WM8978_R01_BIASEN);
    status = wm8978_write_register(device,
                                    WM8978_REG_POWER_MANAGEMENT_1,
                                    value);
    if (status != WM8978_OK)
    {
        return status;
    }

    value = (uint16_t)(WM8978_R02_LOUT1EN | WM8978_R02_ROUT1EN);
    return wm8978_write_register(device,
                                  WM8978_REG_POWER_MANAGEMENT_2,
                                  value);
}

/**
 * @brief 见 wm8978.h 中 wm8978_power_down() 的完整契约。
 */
wm8978_status_t
wm8978_power_down(wm8978_t * device)
{
    wm8978_status_t status;

    status = wm8978_mute_analogue_outputs(device, true);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_write_register(device,
                                    WM8978_REG_POWER_MANAGEMENT_1,
                                    0U);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_write_register(device,
                                    WM8978_REG_POWER_MANAGEMENT_2,
                                    0U);
    if (status != WM8978_OK)
    {
        return status;
    }

    return wm8978_write_register(device,
                                  WM8978_REG_POWER_MANAGEMENT_3,
                                  0U);
}
