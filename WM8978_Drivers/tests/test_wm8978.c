#include "wm8978.h"

#include <stdint.h>

#define TEST_MAX_FRAMES 96U

typedef struct
{
    uint8_t first;
    uint8_t second;
} test_frame_t;

typedef struct
{
    test_frame_t frames[TEST_MAX_FRAMES];
    uint32_t frame_count;
    uint32_t attempt_count;
    uint32_t fail_on_attempt;
    bool fail_after_frame;
    int32_t fail_code;
    uint32_t delay_count;
    uint32_t last_delay_ms;
} fake_port_t;

static uint32_t failures;

#define CHECK(expression)                  \
    do                                     \
    {                                      \
        if (!(expression))                \
        {                                  \
            ++failures;                    \
        }                                  \
    } while (0)

static void fake_init(fake_port_t * fake)
{
    fake->frame_count = 0U;
    fake->attempt_count = 0U;
    fake->fail_on_attempt = 0xFFFFFFFFUL;
    fake->fail_after_frame = false;
    fake->fail_code = -77;
    fake->delay_count = 0U;
    fake->last_delay_ms = 0U;
}

static int32_t fake_write(void * context,
                           uint8_t first_byte,
                           uint8_t second_byte,
                           uint32_t timeout_ms)
{
    fake_port_t * fake;
    uint32_t attempt;

    fake = (fake_port_t *)context;
    CHECK(timeout_ms == 10U);
    attempt = fake->attempt_count;
    ++fake->attempt_count;
    if ((attempt == fake->fail_on_attempt) && !fake->fail_after_frame)
    {
        return fake->fail_code;
    }

    if (fake->frame_count < TEST_MAX_FRAMES)
    {
        fake->frames[fake->frame_count].first = first_byte;
        fake->frames[fake->frame_count].second = second_byte;
        ++fake->frame_count;
    }
    else
    {
        ++failures;
    }

    if (attempt == fake->fail_on_attempt)
    {
        return fake->fail_code;
    }

    return 0;
}

static void fake_delay(void * context, uint32_t milliseconds)
{
    fake_port_t * fake;

    fake = (fake_port_t *)context;
    ++fake->delay_count;
    fake->last_delay_ms = milliseconds;
}

static wm8978_status_t make_ready(wm8978_t * codec,
                                   fake_port_t * fake,
                                   bool with_delay)
{
    wm8978_status_t status;
    wm8978_port_t port;

    fake_init(fake);
    port.write_control = fake_write;
    port.delay_ms = with_delay ? fake_delay : (wm8978_delay_ms_fn)0;
    port.context = fake;
    port.io_timeout_ms = 10U;
    status = wm8978_bind(codec, &port);
    if (status != WM8978_OK)
    {
        return status;
    }
    return wm8978_soft_reset(codec);
}

static void test_register_contract(void)
{
    uint8_t frame[2];
    uint8_t address;
    uint32_t count;

    CHECK(wm8978_pack_control_frame(WM8978_REG_AUDIO_INTERFACE,
                                     0x1ABU,
                                     frame) == WM8978_OK);
    CHECK(frame[0] == 0x09U);
    CHECK(frame[1] == 0xABU);
    CHECK(wm8978_pack_control_frame(0x11U, 0U, frame) ==
          WM8978_ERR_INVALID_REGISTER);
    CHECK(wm8978_pack_control_frame(WM8978_REG_AUDIO_INTERFACE,
                                     0x200U,
                                     frame) == WM8978_ERR_RANGE);
    CHECK(wm8978_pack_control_frame(WM8978_REG_AUDIO_INTERFACE,
                                     0U,
                                     (uint8_t *)0) ==
          WM8978_ERR_NULL_POINTER);

    count = 0U;
    for (address = 0U; address < 0x80U; ++address)
    {
        if (wm8978_register_is_valid(address))
        {
            ++count;
        }
    }
    CHECK(count == WM8978_IMPLEMENTED_REGISTER_COUNT);
    CHECK(!wm8978_register_is_valid(0x11U));
    CHECK(!wm8978_register_is_valid(0x28U));
    CHECK(!wm8978_register_is_valid(0x3AU));
}

static void test_lifecycle_and_shadow(void)
{
    wm8978_t codec;
    wm8978_port_t port;
    fake_port_t fake;
    uint16_t value;

    fake_init(&fake);
    port.write_control = fake_write;
    port.delay_ms = fake_delay;
    port.context = &fake;
    port.io_timeout_ms = 10U;

    CHECK(wm8978_bind(&codec, &port) == WM8978_OK);
    CHECK(wm8978_get_lifecycle(&codec) == WM8978_LIFECYCLE_BOUND);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_AUDIO_INTERFACE,
                                      &value) == WM8978_ERR_NOT_READY);
    CHECK(wm8978_soft_reset(&codec) == WM8978_OK);
    CHECK(fake.frame_count == 1U);
    CHECK(fake.frames[0].first == 0x00U);
    CHECK(fake.frames[0].second == 0x00U);
    CHECK(wm8978_get_lifecycle(&codec) == WM8978_LIFECYCLE_READY);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_AUDIO_INTERFACE,
                                      &value) == WM8978_OK);
    CHECK(value == WM8978_R04_RESET_VALUE);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_SOFTWARE_RESET,
                                      &value) == WM8978_ERR_NO_SHADOW);
}

static void test_write_guards_and_failure_commit(void)
{
    wm8978_t codec;
    fake_port_t fake;
    uint16_t value;
    uint32_t attempts;

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    attempts = fake.attempt_count;
    CHECK(wm8978_write_register(&codec,
                                 WM8978_REG_POWER_MANAGEMENT_3,
                                 0x010U) == WM8978_ERR_RESERVED_BITS);
    CHECK(wm8978_write_register(&codec,
                                 WM8978_REG_3D_CONTROL,
                                 0x010U) == WM8978_ERR_RESERVED_BITS);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_3,
                              0x010U,
                              0x010U) == WM8978_ERR_RESERVED_BITS);
    CHECK(wm8978_write_register(&codec, 0x11U, 0U) ==
          WM8978_ERR_INVALID_REGISTER);
    CHECK(fake.attempt_count == attempts);

    fake.fail_on_attempt = fake.attempt_count;
    fake.fail_after_frame = true;
    CHECK(wm8978_write_register(&codec,
                                 WM8978_REG_AUDIO_INTERFACE,
                                 0x010U) == WM8978_ERR_IO);
    CHECK(wm8978_get_last_port_error(&codec) == -77);
    CHECK(wm8978_get_lifecycle(&codec) ==
          WM8978_LIFECYCLE_DESYNCHRONIZED);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_AUDIO_INTERFACE,
                                      &value) == WM8978_ERR_DESYNCHRONIZED);
    CHECK(codec.shadow[WM8978_REG_AUDIO_INTERFACE] ==
          WM8978_R04_RESET_VALUE);

    fake.fail_on_attempt = 0xFFFFFFFFUL;
    attempts = fake.attempt_count;
    CHECK(wm8978_write_register(&codec,
                                 WM8978_REG_AUDIO_INTERFACE,
                                 0x010U) == WM8978_ERR_DESYNCHRONIZED);
    CHECK(fake.attempt_count == attempts);
    CHECK(wm8978_soft_reset(&codec) == WM8978_OK);
    CHECK(wm8978_get_lifecycle(&codec) == WM8978_LIFECYCLE_READY);
    CHECK(wm8978_write_register(&codec,
                                 WM8978_REG_AUDIO_INTERFACE,
                                 0x010U) == WM8978_OK);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_AUDIO_INTERFACE,
                                      &value) == WM8978_OK);
    CHECK(value == 0x010U);

    attempts = fake.attempt_count;
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_AUDIO_INTERFACE,
                              WM8978_R04_MONO,
                              0U) == WM8978_OK);
    CHECK(fake.attempt_count == attempts);
}

static void test_enum_validation(void)
{
    wm8978_t codec;
    fake_port_t fake;
    wm8978_audio_interface_config_t audio;
    wm8978_clock_config_t clock;
    uint32_t attempts;

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    audio.format = WM8978_AUDIO_FORMAT_I2S;
    audio.word_length = WM8978_WORD_LENGTH_16_BITS;
    audio.invert_bclk = false;
    audio.invert_lrc = false;
    audio.dsp_mode_b = false;
    audio.swap_dac_channels = false;
    audio.swap_adc_channels = false;
    audio.mono = false;
    clock.codec_is_master = false;
    clock.use_pll = false;
    clock.mclk_divider = WM8978_MCLK_DIV_1;
    clock.bclk_divider = WM8978_BCLK_DIV_1;

    attempts = fake.attempt_count;
    audio.format = (wm8978_audio_format_t)-1;
    CHECK(wm8978_configure_audio_interface(&codec, &audio) ==
          WM8978_ERR_RANGE);
    audio.format = WM8978_AUDIO_FORMAT_I2S;
    audio.word_length = (wm8978_word_length_t)-1;
    CHECK(wm8978_configure_audio_interface(&codec, &audio) ==
          WM8978_ERR_RANGE);
    clock.mclk_divider = (wm8978_mclk_div_t)-1;
    CHECK(wm8978_configure_clock(&codec, &clock) == WM8978_ERR_RANGE);
    clock.mclk_divider = WM8978_MCLK_DIV_1;
    clock.bclk_divider = (wm8978_bclk_div_t)-1;
    CHECK(wm8978_configure_clock(&codec, &clock) == WM8978_ERR_RANGE);
    CHECK(wm8978_set_filter_sample_rate(
              &codec,
              (wm8978_filter_sample_rate_t)-1) == WM8978_ERR_RANGE);
    CHECK(wm8978_power_up_nonboost_out1(
              &codec,
              (wm8978_vmid_t)-1,
              500U) == WM8978_ERR_INVALID_ARGUMENT);
    CHECK(fake.attempt_count == attempts);
}

static void test_audio_and_volume(void)
{
    wm8978_t codec;
    fake_port_t fake;
    wm8978_audio_interface_config_t config;
    uint16_t value;
    uint32_t before;

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    config.format = WM8978_AUDIO_FORMAT_I2S;
    config.word_length = WM8978_WORD_LENGTH_16_BITS;
    config.invert_bclk = false;
    config.invert_lrc = false;
    config.dsp_mode_b = false;
    config.swap_dac_channels = false;
    config.swap_adc_channels = false;
    config.mono = false;
    CHECK(wm8978_configure_audio_interface(&codec, &config) == WM8978_OK);
    CHECK(fake.frames[fake.frame_count - 1U].first == 0x08U);
    CHECK(fake.frames[fake.frame_count - 1U].second == 0x10U);

    before = fake.attempt_count;
    config.format = WM8978_AUDIO_FORMAT_RIGHT_JUSTIFIED;
    config.word_length = WM8978_WORD_LENGTH_32_BITS;
    CHECK(wm8978_configure_audio_interface(&codec, &config) ==
          WM8978_ERR_UNSUPPORTED);
    CHECK(fake.attempt_count == before);

    config.format = WM8978_AUDIO_FORMAT_I2S;
    config.word_length = WM8978_WORD_LENGTH_16_BITS;
    config.dsp_mode_b = true;
    CHECK(wm8978_configure_audio_interface(&codec, &config) ==
          WM8978_ERR_INVALID_ARGUMENT);
    CHECK(fake.attempt_count == before);

    config.format = WM8978_AUDIO_FORMAT_DSP_PCM;
    config.invert_lrc = true;
    config.dsp_mode_b = false;
    CHECK(wm8978_configure_audio_interface(&codec, &config) ==
          WM8978_ERR_INVALID_ARGUMENT);
    CHECK(fake.attempt_count == before);

    config.invert_lrc = false;
    config.dsp_mode_b = true;
    CHECK(wm8978_configure_audio_interface(&codec, &config) == WM8978_OK);
    CHECK(fake.frames[fake.frame_count - 1U].first == 0x08U);
    CHECK(fake.frames[fake.frame_count - 1U].second == 0x98U);

    before = fake.frame_count;
    CHECK(wm8978_set_dac_digital_volume(&codec, 0x80U, 0x40U) ==
          WM8978_OK);
    CHECK(fake.frame_count == (before + 2U));
    CHECK(fake.frames[before].first == 0x16U);
    CHECK(fake.frames[before].second == 0x80U);
    CHECK(fake.frames[before + 1U].first == 0x19U);
    CHECK(fake.frames[before + 1U].second == 0x40U);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_LEFT_DAC_VOLUME,
                                      &value) == WM8978_OK);
    CHECK(value == 0x080U);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_RIGHT_DAC_VOLUME,
                                      &value) == WM8978_OK);
    CHECK(value == 0x040U);
}

static void test_multiframe_failure_desynchronizes(void)
{
    wm8978_t codec;
    fake_port_t fake;
    uint32_t before;

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    before = fake.attempt_count;
    fake.fail_on_attempt = before + 1U;
    fake.fail_after_frame = true;
    CHECK(wm8978_set_dac_digital_volume(&codec, 0x80U, 0x40U) ==
          WM8978_ERR_IO);
    CHECK(fake.attempt_count == (before + 2U));
    CHECK(wm8978_get_lifecycle(&codec) ==
          WM8978_LIFECYCLE_DESYNCHRONIZED);
    CHECK(codec.shadow[WM8978_REG_LEFT_DAC_VOLUME] == 0x080U);
    CHECK(codec.shadow[WM8978_REG_RIGHT_DAC_VOLUME] ==
          WM8978_R12_RESET_VALUE);

    fake.fail_on_attempt = 0xFFFFFFFFUL;
    CHECK(wm8978_soft_reset(&codec) == WM8978_OK);
    CHECK(wm8978_get_lifecycle(&codec) == WM8978_LIFECYCLE_READY);
}

static void test_safe_analogue_mute_clears_zero_cross(void)
{
    wm8978_t codec;
    fake_port_t fake;
    wm8978_output_volume_config_t output;
    uint16_t value;

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    output.left_volume_code = 57U;
    output.right_volume_code = 57U;
    output.mute = false;
    output.zero_cross = true;
    CHECK(wm8978_set_output_volume(&codec,
                                    WM8978_OUTPUT_HEADPHONE,
                                    &output) == WM8978_OK);
    CHECK(wm8978_set_output_volume(&codec,
                                    WM8978_OUTPUT_SPEAKER,
                                    &output) == WM8978_OK);
    CHECK(wm8978_mute_analogue_outputs(&codec, true) == WM8978_OK);

    CHECK(wm8978_get_shadow_register(
              &codec,
              WM8978_REG_LEFT_HEADPHONE_VOLUME,
              &value) == WM8978_OK);
    CHECK((value & WM8978_OUTPUT_MUTE) != 0U);
    CHECK((value & WM8978_OUTPUT_ZC) == 0U);
    CHECK(wm8978_get_shadow_register(
              &codec,
              WM8978_REG_RIGHT_SPEAKER_VOLUME,
              &value) == WM8978_OK);
    CHECK((value & WM8978_OUTPUT_MUTE) != 0U);
    CHECK((value & WM8978_OUTPUT_ZC) == 0U);
}

static void test_pll_ordering(void)
{
    wm8978_t codec;
    fake_port_t fake;
    wm8978_clock_config_t clock;
    wm8978_pll_config_t pll;

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    pll.divide_mclk_by_2 = false;
    pll.n = 8U;
    pll.k = 0x3126E9UL;
    CHECK(wm8978_configure_pll(&codec, &pll) == WM8978_ERR_STATE);

    clock.codec_is_master = false;
    clock.use_pll = false;
    clock.mclk_divider = WM8978_MCLK_DIV_1;
    clock.bclk_divider = WM8978_BCLK_DIV_1;
    CHECK(wm8978_configure_clock(&codec, &clock) == WM8978_OK);
    CHECK(wm8978_set_pll_enabled(&codec, true) == WM8978_ERR_STATE);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_1,
                              WM8978_R01_PLLEN,
                              WM8978_R01_PLLEN) == WM8978_OK);
    clock.use_pll = true;
    CHECK(wm8978_configure_clock(&codec, &clock) == WM8978_ERR_STATE);
    clock.use_pll = false;
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_1,
                              WM8978_R01_PLLEN,
                              0U) == WM8978_OK);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_1,
                              WM8978_R01_VMIDSEL_MASK,
                              WM8978_VMID_5K) == WM8978_OK);
    CHECK(wm8978_configure_pll(&codec, &pll) == WM8978_OK);
    CHECK(wm8978_set_pll_enabled(&codec, true) == WM8978_OK);

    clock.use_pll = true;
    CHECK(wm8978_configure_clock(&codec, &clock) == WM8978_OK);
    CHECK(wm8978_set_pll_enabled(&codec, false) == WM8978_ERR_STATE);
    clock.use_pll = false;
    CHECK(wm8978_configure_clock(&codec, &clock) == WM8978_OK);
    CHECK(wm8978_set_pll_enabled(&codec, false) == WM8978_OK);
}

static void test_eq_mode_transition_guard(void)
{
    wm8978_t codec;
    fake_port_t fake;
    uint16_t value;
    uint32_t attempts;

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    CHECK((codec.shadow[WM8978_REG_EQ1] & WM8978_R18_EQ3DMODE) != 0U);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_2,
                              WM8978_R02_ADCENL,
                              WM8978_R02_ADCENL) == WM8978_OK);
    attempts = fake.attempt_count;
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_EQ1,
                              WM8978_R18_EQ3DMODE,
                              0U) == WM8978_ERR_STATE);
    CHECK(fake.attempt_count == attempts);
    CHECK(codec.shadow[WM8978_REG_EQ1] == WM8978_R18_RESET_VALUE);

    value = (uint16_t)(WM8978_R18_RESET_VALUE ^ 0x001U);
    CHECK(wm8978_write_register(&codec,
                                 WM8978_REG_EQ1,
                                 value) == WM8978_OK);
    CHECK((codec.shadow[WM8978_REG_EQ1] & WM8978_R18_EQ3DMODE) != 0U);

    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_2,
                              WM8978_R02_ADCENL,
                              0U) == WM8978_OK);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_3,
                              WM8978_R03_DACENR,
                              WM8978_R03_DACENR) == WM8978_OK);
    attempts = fake.attempt_count;
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_EQ1,
                              WM8978_R18_EQ3DMODE,
                              0U) == WM8978_ERR_STATE);
    CHECK(fake.attempt_count == attempts);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_3,
                              WM8978_R03_DACENR,
                              0U) == WM8978_OK);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_EQ1,
                              WM8978_R18_EQ3DMODE,
                              0U) == WM8978_OK);
    CHECK((codec.shadow[WM8978_REG_EQ1] & WM8978_R18_EQ3DMODE) == 0U);
}

static void test_power_sequence(void)
{
    wm8978_t codec;
    fake_port_t fake;
    uint16_t value;
    uint32_t attempts;
    uint32_t stage;

    CHECK(make_ready(&codec, &fake, false) == WM8978_OK);
    CHECK(wm8978_power_up_nonboost_out1(&codec,
                                         WM8978_VMID_5K,
                                         500U) ==
          WM8978_ERR_DELAY_REQUIRED);
    CHECK(fake.frame_count == 1U);

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    CHECK(wm8978_update_bits(&codec,
                              WM8978_REG_POWER_MANAGEMENT_2,
                              WM8978_R02_SLEEP,
                              WM8978_R02_SLEEP) == WM8978_OK);
    attempts = fake.attempt_count;
    CHECK(wm8978_power_up_nonboost_out1(&codec,
                                         WM8978_VMID_5K,
                                         500U) == WM8978_ERR_STATE);
    CHECK(fake.attempt_count == attempts);

    CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
    CHECK(wm8978_power_up_nonboost_out1(&codec,
                                         WM8978_VMID_5K,
                                         500U) == WM8978_OK);
    CHECK(fake.frame_count == 11U);
    CHECK(fake.delay_count == 1U);
    CHECK(fake.last_delay_ms == 500U);
    CHECK(fake.frames[7U].first == 0x06U);
    CHECK(fake.frames[7U].second == 0x0FU);
    CHECK(fake.frames[8U].first == 0x02U);
    CHECK(fake.frames[8U].second == 0x07U);
    CHECK(fake.frames[9U].first == 0x02U);
    CHECK(fake.frames[9U].second == 0x0FU);
    CHECK(fake.frames[10U].first == 0x05U);
    CHECK(fake.frames[10U].second == 0x80U);

    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_POWER_MANAGEMENT_1,
                                      &value) == WM8978_OK);
    CHECK(value == 0x00FU);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_POWER_MANAGEMENT_2,
                                      &value) == WM8978_OK);
    CHECK(value == 0x180U);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_POWER_MANAGEMENT_3,
                                      &value) == WM8978_OK);
    CHECK(value == 0x00FU);

    CHECK(wm8978_power_down(&codec) == WM8978_OK);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_POWER_MANAGEMENT_1,
                                      &value) == WM8978_OK);
    CHECK(value == 0U);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_POWER_MANAGEMENT_2,
                                      &value) == WM8978_OK);
    CHECK(value == 0U);
    CHECK(wm8978_get_shadow_register(&codec,
                                      WM8978_REG_POWER_MANAGEMENT_3,
                                      &value) == WM8978_OK);
    CHECK(value == 0U);

    for (stage = 0U; stage < 10U; ++stage)
    {
        CHECK(make_ready(&codec, &fake, true) == WM8978_OK);
        fake.fail_on_attempt = fake.attempt_count + stage;
        fake.fail_after_frame = true;
        CHECK(wm8978_power_up_nonboost_out1(&codec,
                                             WM8978_VMID_5K,
                                             500U) == WM8978_ERR_IO);
        CHECK(wm8978_get_lifecycle(&codec) ==
              WM8978_LIFECYCLE_DESYNCHRONIZED);
        attempts = fake.attempt_count;
        CHECK(wm8978_power_down(&codec) == WM8978_ERR_DESYNCHRONIZED);
        CHECK(fake.attempt_count == attempts);
    }
}

int main(void)
{
    failures = 0U;
    test_register_contract();
    test_lifecycle_and_shadow();
    test_write_guards_and_failure_commit();
    test_enum_validation();
    test_audio_and_volume();
    test_multiframe_failure_desynchronizes();
    test_safe_analogue_mute_clears_zero_cross();
    test_pll_ordering();
    test_eq_mode_transition_guard();
    test_power_sequence();
    return (int)failures;
}
