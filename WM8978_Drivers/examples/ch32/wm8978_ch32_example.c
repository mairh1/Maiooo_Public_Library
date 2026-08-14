/**
 * @file    wm8978_ch32_example.c
 * @brief   Explicit I2S/16-bit/slave/48-kHz-coefficient WM8978 example
 *
 * This configures only the codec. The CH32 BSP must separately provide a
 * stable 256fs MCLK (for example 12.288 MHz for 48 kHz), configure its audio
 * peripheral and DMA, keep DACDAT at zero during codec startup, and start the
 * stream before unmuting the headphone outputs.
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
    wm8978_port_t port;
    wm8978_audio_interface_config_t audio;
    wm8978_clock_config_t clock;
    wm8978_output_volume_config_t headphone;

    status = wm8978_ch32_i2c_make_port(adapter, io_timeout_ms, &port);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_bind(codec, &port);
    if (status != WM8978_OK)
    {
        return status;
    }

    status = wm8978_soft_reset(codec);
    if (status != WM8978_OK)
    {
        return status;
    }

    /* Select external MCLK before any optional PLL work. */
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
