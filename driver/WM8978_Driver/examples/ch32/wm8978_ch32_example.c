/**
 * @file    wm8978_ch32_example.c
 * @brief   显式配置 I2S/16 位/从机/48 kHz 系数的 WM8978 示例
 *
 * 本示例只配置 codec。CH32 BSP 须另行提供稳定的 256fs MCLK（如
 * 48 kHz 用 12.288 MHz）、配置音频外设与 DMA、在 codec 启动期间
 * 保持 DACDAT 为零，并在解除耳机静音前启动数据流。
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "wm8978_ch32_example.h"

wm8978_status_t wm8978_ch32_example_init_i2s16_slave(
    wm8978_t * codec,
    wm8978_ch32_i2c_adapter_t * adapter,
    uint32_t io_timeout_ms,
    uint32_t board_vmid_settle_ms)
{
    wm8978_status_t status;
    wm8978_audio_interface_config_t audio;
    wm8978_clock_config_t clock;
    wm8978_output_volume_config_t headphone;

    status = wm8978_ch32_i2c_bind(codec, adapter, io_timeout_ms);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_soft_reset(codec);
    if (status != WM8978_OK)
    {
        return status;
    }

    /* 在任何可选的 PLL 操作之前先选择外部 MCLK。 */
    clock.codec_is_master = false;
    clock.use_pll = false;
    clock.mclk_divider = WM8978_MCLK_DIV_1;
    clock.bclk_divider = WM8978_BCLK_DIV_1;
    status = wm8978_configure_clock(codec, &clock);
    if (status != WM8978_OK)
    {
        return status;
    }

    audio.format = WM8978_AUDIO_FORMAT_I2S;
    audio.word_length = WM8978_WORD_LENGTH_16_BITS;
    audio.invert_bclk = false;
    audio.invert_lrc = false;
    audio.dsp_mode_b = false;
    audio.swap_dac_channels = false;
    audio.swap_adc_channels = false;
    audio.mono = false;
    status = wm8978_configure_audio_interface(codec, &audio);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_set_filter_sample_rate(codec, WM8978_FILTER_SR_48_KHZ);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_set_dac_digital_volume(codec, 255U, 255U);
    if (status != WM8978_OK)
    {
        return status;
    }

    headphone.left_volume_code = 57U;  /* 0 dB */
    headphone.right_volume_code = 57U; /* 0 dB */
    headphone.mute = true;
    headphone.zero_cross = true;
    status = wm8978_set_output_volume(codec,
                                       WM8978_OUTPUT_HEADPHONE,
                                       &headphone);
    if (status != WM8978_OK)
    {
        return status;
    }

    return wm8978_power_up_nonboost_out1(codec,
                                          WM8978_VMID_5K,
                                          board_vmid_settle_ms);
}
