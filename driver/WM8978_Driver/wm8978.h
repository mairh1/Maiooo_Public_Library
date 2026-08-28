/**
 * @file    wm8978.h
 * @brief   Portable C99 driver for the WM8978 stereo audio codec
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * @details
 * The core owns no peripheral, pin, clock, DMA, interrupt, or memory resource.
 * The caller supplies one bounded control-frame writer and, when using the
 * power-sequence helper, one millisecond delay callback. The same core works
 * with WM8978 2-wire and 3-wire control modes because the platform adapter
 * owns the physical transaction.
 *
 * WM8978 registers are write-only through the documented control interface.
 * This driver therefore maintains a software shadow. Call wm8978_soft_reset()
 * or wm8978_assume_power_on_reset() before any ordinary register operation.
 * The driver is non-reentrant; serialize access outside interrupt context.
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef WM8978_H
#define WM8978_H

#include <stdbool.h>
#include <stdint.h>

#include "wm8978_io.h"
#include "wm8978_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WM8978_DRIVER_VERSION_MAJOR             1
#define WM8978_DRIVER_VERSION_MINOR             0
#define WM8978_DRIVER_VERSION_PATCH             0

/** @brief Driver result codes. */
typedef enum
{
    WM8978_OK                         =  0,
    WM8978_ERR_NULL_POINTER           = -1,
    WM8978_ERR_INVALID_ARGUMENT       = -2,
    WM8978_ERR_NOT_BOUND              = -3,
    WM8978_ERR_NOT_READY              = -4,
    WM8978_ERR_RANGE                  = -5,
    WM8978_ERR_INVALID_REGISTER       = -6,
    WM8978_ERR_RESERVED_BITS          = -7,
    WM8978_ERR_IO                     = -8,
    WM8978_ERR_STATE                  = -9,
    WM8978_ERR_NO_SHADOW              = -10,
    WM8978_ERR_DELAY_REQUIRED         = -11,
    WM8978_ERR_UNSUPPORTED            = -12,
    WM8978_ERR_DESYNCHRONIZED         = -13
} wm8978_status_t;

/** @brief Local lifecycle of a driver instance. */
typedef enum
{
    WM8978_LIFECYCLE_UNBOUND = 0,
    WM8978_LIFECYCLE_BOUND,
    WM8978_LIFECYCLE_READY,
    WM8978_LIFECYCLE_DESYNCHRONIZED
} wm8978_lifecycle_t;

/** @brief Digital audio serial format, matching R4 FMT encoding. */
typedef enum
{
    WM8978_AUDIO_FORMAT_RIGHT_JUSTIFIED = 0,
    WM8978_AUDIO_FORMAT_LEFT_JUSTIFIED  = 1,
    WM8978_AUDIO_FORMAT_I2S             = 2,
    WM8978_AUDIO_FORMAT_DSP_PCM         = 3
} wm8978_audio_format_t;

/** @brief Audio word length, matching R4 WL encoding. */
typedef enum
{
    WM8978_WORD_LENGTH_16_BITS = 0,
    WM8978_WORD_LENGTH_20_BITS = 1,
    WM8978_WORD_LENGTH_24_BITS = 2,
    WM8978_WORD_LENGTH_32_BITS = 3
} wm8978_word_length_t;

/** @brief R4 digital audio interface configuration. */
typedef struct
{
    wm8978_audio_format_t format;
    wm8978_word_length_t word_length;
    bool invert_bclk;
    /** LRC polarity for right/left-justified and I2S; must be false for DSP. */
    bool invert_lrc;
    /** DSP/PCM mode B when true, mode A when false; false for other formats. */
    bool dsp_mode_b;
    bool swap_dac_channels;
    bool swap_adc_channels;
    bool mono;
} wm8978_audio_interface_config_t;

/** @brief MCLK or PLL-output divider, matching R6 MCLKDIV encoding. */
typedef enum
{
    WM8978_MCLK_DIV_1 = 0,
    WM8978_MCLK_DIV_1_5,
    WM8978_MCLK_DIV_2,
    WM8978_MCLK_DIV_3,
    WM8978_MCLK_DIV_4,
    WM8978_MCLK_DIV_6,
    WM8978_MCLK_DIV_8,
    WM8978_MCLK_DIV_12
} wm8978_mclk_div_t;

/** @brief SYSCLK-to-BCLK divider, matching valid R6 BCLKDIV encodings. */
typedef enum
{
    WM8978_BCLK_DIV_1 = 0,
    WM8978_BCLK_DIV_2,
    WM8978_BCLK_DIV_4,
    WM8978_BCLK_DIV_8,
    WM8978_BCLK_DIV_16,
    WM8978_BCLK_DIV_32
} wm8978_bclk_div_t;

/** @brief R6 clock source/master configuration. */
typedef struct
{
    bool codec_is_master;
    bool use_pll;
    wm8978_mclk_div_t mclk_divider;
    wm8978_bclk_div_t bclk_divider;
} wm8978_clock_config_t;

/**
 * @brief Internal digital-filter coefficient group, matching R7 SR.
 *
 * This does not generate the sampling clock. The real sample rate is set by
 * MCLK/PLL, dividers, and the audio bus clocks. For 44.1/22.05/11.025 kHz,
 * the datasheet says to choose the nearest 48/24/12 kHz coefficient group.
 */
typedef enum
{
    WM8978_FILTER_SR_48_KHZ = 0,
    WM8978_FILTER_SR_32_KHZ,
    WM8978_FILTER_SR_24_KHZ,
    WM8978_FILTER_SR_16_KHZ,
    WM8978_FILTER_SR_12_KHZ,
    WM8978_FILTER_SR_8_KHZ
} wm8978_filter_sample_rate_t;

/** @brief Raw PLL ratio configuration for R36-R39. */
typedef struct
{
    bool divide_mclk_by_2;
    uint8_t n;
    uint32_t k;
} wm8978_pll_config_t;

/** @brief Analogue output pair selected for volume programming. */
typedef enum
{
    WM8978_OUTPUT_HEADPHONE = 0,
    WM8978_OUTPUT_SPEAKER
} wm8978_output_pair_t;

/** @brief Stereo input-PGA programming. */
typedef struct
{
    uint8_t left_volume_code;
    uint8_t right_volume_code;
    bool mute;
    bool zero_cross;
} wm8978_input_pga_config_t;

/** @brief Stereo OUT1/OUT2 output-PGA programming. */
typedef struct
{
    uint8_t left_volume_code;
    uint8_t right_volume_code;
    bool mute;
    bool zero_cross;
} wm8978_output_volume_config_t;

/** @brief VMID impedance selection, matching R1 VMIDSEL encoding. */
typedef enum
{
    WM8978_VMID_OFF = 0,
    WM8978_VMID_75K,
    WM8978_VMID_300K,
    WM8978_VMID_5K
} wm8978_vmid_t;

/** @brief Caller-owned driver instance. Do not modify members directly. */
typedef struct
{
    void * io_ctx;
    uint32_t io_timeout_ms;
    wm8978_lifecycle_t lifecycle;
    int32_t last_port_error;
    uint16_t shadow[WM8978_REGISTER_SPACE_SIZE];
} wm8978_t;

wm8978_status_t wm8978_bind(wm8978_t * device,
                             void * io_ctx,
                             uint32_t io_timeout_ms);

/**
 * @brief Load reset defaults without writing the control bus.
 *
 * Call this only when board-level evidence guarantees that a completed POR,
 * or a successful R0 write by another controller, has just placed the codec
 * in its documented reset state. WM8978 has no external RESET pin.
 */
wm8978_status_t wm8978_assume_power_on_reset(wm8978_t * device);

/** @brief Write R0 and synchronize the software shadow to reset defaults. */
wm8978_status_t wm8978_soft_reset(wm8978_t * device);

wm8978_lifecycle_t wm8978_get_lifecycle(const wm8978_t * device);

int32_t wm8978_get_last_port_error(const wm8978_t * device);

bool wm8978_register_is_valid(uint8_t register_address);

/** @brief Pack one validated register/value pair into B15:B8 and B7:B0. */
wm8978_status_t wm8978_pack_control_frame(uint8_t register_address,
                                           uint16_t value,
                                           uint8_t frame[2]);

/** @brief Read the driver's shadow, not the write-only hardware. */
wm8978_status_t wm8978_get_shadow_register(const wm8978_t * device,
                                             uint8_t register_address,
                                             uint16_t * value);

/**
 * @brief Write one complete register value.
 *
 * Reserved bits must remain at their documented reset value. Writing R0 is a
 * software reset and reloads the entire shadow. Non-latched update bits, plus
 * NFU under the documented one-shot software policy, are sent on the wire but
 * cleared in the stored shadow after successful I/O.
 * This is a raw escape hatch and does not enforce high-level sequencing.
 * It nevertheless returns WM8978_ERR_STATE if an R18 write would change
 * EQ3DMODE while any ADC or DAC is enabled, because silicon rejects that bit.
 */
wm8978_status_t wm8978_write_register(wm8978_t * device,
                                       uint8_t register_address,
                                       uint16_t value);

/**
 * @brief Update writable bits using the synchronized software shadow.
 *
 * This is a raw escape hatch: it preserves reserved bits but does not enforce
 * every field encoding or the high-level PLL/power ordering rules. It does
 * reject an EQ3DMODE transition while any ADC/DAC is enabled, because the
 * codec would refuse that bit and make the write-only shadow inaccurate.
 */
wm8978_status_t wm8978_update_bits(wm8978_t * device,
                                    uint8_t register_address,
                                    uint16_t mask,
                                    uint16_t field_value);

wm8978_status_t wm8978_configure_audio_interface(
    wm8978_t * device,
    const wm8978_audio_interface_config_t * config);

/**
 * @brief Configure R6 clock selection and dividers.
 *
 * Selecting PLL requires PLLEN already set and VMIDSEL non-zero. When leaving
 * PLL, call this first with use_pll=false and disable PLL only after the
 * source has switched.
 * The original clock source must remain active for at least one falling edge
 * after a CLKSEL change, as required by the datasheet.
 */
wm8978_status_t wm8978_configure_clock(
    wm8978_t * device,
    const wm8978_clock_config_t * config);

wm8978_status_t wm8978_set_filter_sample_rate(
    wm8978_t * device,
    wm8978_filter_sample_rate_t sample_rate_group);

/**
 * @brief Program R36-R39 while PLL is disabled and MCLK is selected.
 *
 * To satisfy both conflicting range wordings in Rev 4.5, N is conservatively
 * limited to 6..12. K is a 24-bit fraction. Here f1 is MCLK after the optional
 * divide-by-2 prescaler; choose the board-validated f2 near the datasheet's
 * preferred 90..100 MHz region, with N near 8. This function does not enable
 * or select the PLL.
 */
wm8978_status_t wm8978_configure_pll(wm8978_t * device,
                                      const wm8978_pll_config_t * config);

/**
 * @brief Enable/disable PLL with safe source/VMID ordering checks.
 *
 * The board must guarantee DCVDD >= 1.9 V when the PLL is used and validate
 * the generated clock; neither supply voltage nor PLL lock can be read here.
 */
wm8978_status_t wm8978_set_pll_enabled(wm8978_t * device, bool enabled);

/**
 * @brief Set both DAC digital volumes with one synchronized update.
 *
 * Code 0 is digital mute; code 1 is -127 dB; codes then advance by 0.5 dB
 * until code 255 is 0 dB.
 */
wm8978_status_t wm8978_set_dac_digital_volume(wm8978_t * device,
                                               uint8_t left_code,
                                               uint8_t right_code);

/** @brief Set both ADC digital volumes with one synchronized update. */
wm8978_status_t wm8978_set_adc_digital_volume(wm8978_t * device,
                                               uint8_t left_code,
                                               uint8_t right_code);

/**
 * @brief Set both input PGAs with one synchronized update.
 *
 * Codes 0..63 represent -12 dB to +35.25 dB in 0.75 dB steps.
 */
wm8978_status_t wm8978_set_input_pga(
    wm8978_t * device,
    const wm8978_input_pga_config_t * config);

/**
 * @brief Set the OUT1 headphone or OUT2 speaker stereo volume pair.
 *
 * Codes 0..63 represent -57 dB to +6 dB in 1 dB steps.
 */
wm8978_status_t wm8978_set_output_volume(
    wm8978_t * device,
    wm8978_output_pair_t output,
    const wm8978_output_volume_config_t * config);

/**
 * @brief Set/clear every unambiguous analogue-output MUTE field.
 *
 * Muting also clears OUT1/OUT2 zero-cross wait so the power sequence cannot
 * stall indefinitely while SLOWCLKEN is disabled. Unmuting preserves it.
 */
wm8978_status_t wm8978_mute_analogue_outputs(wm8978_t * device, bool mute);

/**
 * @brief Execute the datasheet non-1.5x-boost OUT1 power-up register sequence.
 *
 * The caller must first stabilize external supplies and keep DACDAT at zero.
 * vmid_settle_ms is board-derived and must be non-zero. R1/R2/R3 must all be
 * zero at entry. The helper leaves all analogue outputs muted; unmute
 * explicitly after clocks/data are stable.
 * This and every other multi-frame API are non-atomic. Any port error moves
 * the instance to DESYNCHRONIZED; recover with a verified reset, not a retry.
 */
wm8978_status_t wm8978_power_up_nonboost_out1(wm8978_t * device,
                                               wm8978_vmid_t vmid,
                                               uint32_t vmid_settle_ms);

/**
 * @brief Mute outputs and write R1=0, R2=0, R3=0 in datasheet order.
 *
 * The caller remains responsible for stopping non-zero DACDAT and removing
 * external power after this function succeeds.
 */
wm8978_status_t wm8978_power_down(wm8978_t * device);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_H */
