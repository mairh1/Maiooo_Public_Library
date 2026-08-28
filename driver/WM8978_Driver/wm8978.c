/**
 * @file    wm8978.c
 * @brief   Portable C99 WM8978 driver implementation
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "wm8978.h"
#include "wm8978_io.h"

#include <stddef.h>

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

/* Bits not present in these masks must remain at their reset value. */
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

/* Hardware non-latched triggers plus NFU's conservative one-shot policy. */
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

static wm8978_status_t wm8978_require_bound(const wm8978_t * device)
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

static wm8978_status_t wm8978_require_ready(const wm8978_t * device)
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

static void wm8978_load_reset_defaults(wm8978_t * device)
{
    uint8_t address;

    for (address = 0U; address < WM8978_REGISTER_SPACE_SIZE; ++address)
    {
        device->shadow[address] = wm8978_reset_defaults[address];
    }
    device->lifecycle = WM8978_LIFECYCLE_READY;
}

static wm8978_status_t wm8978_send_control(wm8978_t * device,
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

static wm8978_status_t wm8978_validate_complete_value(
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

static wm8978_status_t wm8978_write_and_commit(wm8978_t * device,
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

static wm8978_status_t wm8978_write_stereo_update(wm8978_t * device,
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

wm8978_status_t wm8978_bind(wm8978_t * device,
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

wm8978_status_t wm8978_assume_power_on_reset(wm8978_t * device)
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

wm8978_status_t wm8978_soft_reset(wm8978_t * device)
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

wm8978_lifecycle_t wm8978_get_lifecycle(const wm8978_t * device)
{
    if (device == NULL)
    {
        return WM8978_LIFECYCLE_UNBOUND;
    }

    return device->lifecycle;
}

int32_t wm8978_get_last_port_error(const wm8978_t * device)
{
    if (device == NULL)
    {
        return 0;
    }

    return device->last_port_error;
}

bool wm8978_register_is_valid(uint8_t register_address)
{
    return (register_address < WM8978_REGISTER_SPACE_SIZE) &&
           (wm8978_valid_registers[register_address] != 0U);
}

wm8978_status_t wm8978_pack_control_frame(uint8_t register_address,
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

wm8978_status_t wm8978_get_shadow_register(const wm8978_t * device,
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

wm8978_status_t wm8978_write_register(wm8978_t * device,
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

wm8978_status_t wm8978_update_bits(wm8978_t * device,
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

wm8978_status_t wm8978_configure_audio_interface(
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

wm8978_status_t wm8978_configure_clock(
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

wm8978_status_t wm8978_set_filter_sample_rate(
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

wm8978_status_t wm8978_configure_pll(wm8978_t * device,
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

wm8978_status_t wm8978_set_pll_enabled(wm8978_t * device, bool enabled)
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

wm8978_status_t wm8978_set_dac_digital_volume(wm8978_t * device,
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

wm8978_status_t wm8978_set_adc_digital_volume(wm8978_t * device,
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

wm8978_status_t wm8978_set_input_pga(
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

wm8978_status_t wm8978_set_output_volume(
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

wm8978_status_t wm8978_mute_analogue_outputs(wm8978_t * device, bool mute)
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

wm8978_status_t wm8978_power_up_nonboost_out1(wm8978_t * device,
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

wm8978_status_t wm8978_power_down(wm8978_t * device)
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
