/**
 * @file    wm8978_regs.h
 * @brief   WM8978 register addresses, fields, and reset values
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * Values are derived from WM8978 Production Data, Rev 4.5, October 2011.
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef WM8978_REGS_H
#define WM8978_REGS_H

#include <stdint.h>

/* Control-interface constants. */
#define WM8978_I2C_ADDRESS_7BIT                 ((uint8_t)0x1AU)
#define WM8978_I2C_WRITE_ADDRESS_8BIT           ((uint8_t)0x34U)
#define WM8978_CONTROL_2WIRE_MAX_SCLK_HZ        ((uint32_t)526000UL)
#define WM8978_REGISTER_VALUE_MASK              ((uint16_t)0x01FFU)
#define WM8978_REGISTER_SPACE_SIZE              ((uint8_t)58U)
#define WM8978_IMPLEMENTED_REGISTER_COUNT       ((uint8_t)52U)

#define WM8978_FIELD_PREP(mask, shift, value) \
    ((uint16_t)((((uint16_t)(value)) << (shift)) & (uint16_t)(mask)))
#define WM8978_FIELD_GET(mask, shift, value) \
    ((uint16_t)((((uint16_t)(value)) & (uint16_t)(mask)) >> (shift)))

/* Implemented register addresses from Table 69. Gaps are reserved. */
#define WM8978_REG_SOFTWARE_RESET               ((uint8_t)0x00U)
#define WM8978_REG_POWER_MANAGEMENT_1           ((uint8_t)0x01U)
#define WM8978_REG_POWER_MANAGEMENT_2           ((uint8_t)0x02U)
#define WM8978_REG_POWER_MANAGEMENT_3           ((uint8_t)0x03U)
#define WM8978_REG_AUDIO_INTERFACE              ((uint8_t)0x04U)
#define WM8978_REG_COMPANDING_CONTROL            ((uint8_t)0x05U)
#define WM8978_REG_CLOCK_GENERATION              ((uint8_t)0x06U)
#define WM8978_REG_ADDITIONAL_CONTROL            ((uint8_t)0x07U)
#define WM8978_REG_GPIO                          ((uint8_t)0x08U)
#define WM8978_REG_JACK_DETECT_1                 ((uint8_t)0x09U)
#define WM8978_REG_DAC_CONTROL                   ((uint8_t)0x0AU)
#define WM8978_REG_LEFT_DAC_VOLUME               ((uint8_t)0x0BU)
#define WM8978_REG_RIGHT_DAC_VOLUME              ((uint8_t)0x0CU)
#define WM8978_REG_JACK_DETECT_2                 ((uint8_t)0x0DU)
#define WM8978_REG_ADC_CONTROL                   ((uint8_t)0x0EU)
#define WM8978_REG_LEFT_ADC_VOLUME               ((uint8_t)0x0FU)
#define WM8978_REG_RIGHT_ADC_VOLUME              ((uint8_t)0x10U)
#define WM8978_REG_EQ1                           ((uint8_t)0x12U)
#define WM8978_REG_EQ2                           ((uint8_t)0x13U)
#define WM8978_REG_EQ3                           ((uint8_t)0x14U)
#define WM8978_REG_EQ4                           ((uint8_t)0x15U)
#define WM8978_REG_EQ5                           ((uint8_t)0x16U)
#define WM8978_REG_DAC_LIMITER_1                 ((uint8_t)0x18U)
#define WM8978_REG_DAC_LIMITER_2                 ((uint8_t)0x19U)
#define WM8978_REG_NOTCH_FILTER_1                ((uint8_t)0x1BU)
#define WM8978_REG_NOTCH_FILTER_2                ((uint8_t)0x1CU)
#define WM8978_REG_NOTCH_FILTER_3                ((uint8_t)0x1DU)
#define WM8978_REG_NOTCH_FILTER_4                ((uint8_t)0x1EU)
#define WM8978_REG_ALC_CONTROL_1                 ((uint8_t)0x20U)
#define WM8978_REG_ALC_CONTROL_2                 ((uint8_t)0x21U)
#define WM8978_REG_ALC_CONTROL_3                 ((uint8_t)0x22U)
#define WM8978_REG_NOISE_GATE                    ((uint8_t)0x23U)
#define WM8978_REG_PLL_N                         ((uint8_t)0x24U)
#define WM8978_REG_PLL_K1                        ((uint8_t)0x25U)
#define WM8978_REG_PLL_K2                        ((uint8_t)0x26U)
#define WM8978_REG_PLL_K3                        ((uint8_t)0x27U)
#define WM8978_REG_3D_CONTROL                    ((uint8_t)0x29U)
#define WM8978_REG_BEEP_CONTROL                  ((uint8_t)0x2BU)
#define WM8978_REG_INPUT_CONTROL                 ((uint8_t)0x2CU)
#define WM8978_REG_LEFT_INPUT_PGA                ((uint8_t)0x2DU)
#define WM8978_REG_RIGHT_INPUT_PGA               ((uint8_t)0x2EU)
#define WM8978_REG_LEFT_ADC_BOOST                ((uint8_t)0x2FU)
#define WM8978_REG_RIGHT_ADC_BOOST               ((uint8_t)0x30U)
#define WM8978_REG_OUTPUT_CONTROL                ((uint8_t)0x31U)
#define WM8978_REG_LEFT_MIXER                    ((uint8_t)0x32U)
#define WM8978_REG_RIGHT_MIXER                   ((uint8_t)0x33U)
#define WM8978_REG_LEFT_HEADPHONE_VOLUME         ((uint8_t)0x34U)
#define WM8978_REG_RIGHT_HEADPHONE_VOLUME        ((uint8_t)0x35U)
#define WM8978_REG_LEFT_SPEAKER_VOLUME           ((uint8_t)0x36U)
#define WM8978_REG_RIGHT_SPEAKER_VOLUME          ((uint8_t)0x37U)
#define WM8978_REG_OUT3_MIXER                    ((uint8_t)0x38U)
#define WM8978_REG_OUT4_MIXER                    ((uint8_t)0x39U)

/* Reset values. R0 is a non-latched command and has no hardware reset value. */
#define WM8978_R00_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R01_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R02_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R03_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R04_RESET_VALUE                   ((uint16_t)0x050U)
#define WM8978_R05_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R06_RESET_VALUE                   ((uint16_t)0x140U)
#define WM8978_R07_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R08_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R09_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R10_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R11_RESET_VALUE                   ((uint16_t)0x0FFU)
#define WM8978_R12_RESET_VALUE                   ((uint16_t)0x0FFU)
#define WM8978_R13_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R14_RESET_VALUE                   ((uint16_t)0x100U)
#define WM8978_R15_RESET_VALUE                   ((uint16_t)0x0FFU)
#define WM8978_R16_RESET_VALUE                   ((uint16_t)0x0FFU)
#define WM8978_R18_RESET_VALUE                   ((uint16_t)0x12CU)
#define WM8978_R19_RESET_VALUE                   ((uint16_t)0x02CU)
#define WM8978_R20_RESET_VALUE                   ((uint16_t)0x02CU)
#define WM8978_R21_RESET_VALUE                   ((uint16_t)0x02CU)
#define WM8978_R22_RESET_VALUE                   ((uint16_t)0x02CU)
#define WM8978_R24_RESET_VALUE                   ((uint16_t)0x032U)
#define WM8978_R25_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R27_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R28_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R29_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R30_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R32_RESET_VALUE                   ((uint16_t)0x038U)
#define WM8978_R33_RESET_VALUE                   ((uint16_t)0x00BU)
#define WM8978_R34_RESET_VALUE                   ((uint16_t)0x032U)
#define WM8978_R35_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R36_RESET_VALUE                   ((uint16_t)0x008U)
#define WM8978_R37_RESET_VALUE                   ((uint16_t)0x00CU)
#define WM8978_R38_RESET_VALUE                   ((uint16_t)0x093U)
#define WM8978_R39_RESET_VALUE                   ((uint16_t)0x0E9U)
#define WM8978_R41_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R43_RESET_VALUE                   ((uint16_t)0x000U)
#define WM8978_R44_RESET_VALUE                   ((uint16_t)0x033U)
#define WM8978_R45_RESET_VALUE                   ((uint16_t)0x010U)
#define WM8978_R46_RESET_VALUE                   ((uint16_t)0x010U)
#define WM8978_R47_RESET_VALUE                   ((uint16_t)0x100U)
#define WM8978_R48_RESET_VALUE                   ((uint16_t)0x100U)
#define WM8978_R49_RESET_VALUE                   ((uint16_t)0x002U)
#define WM8978_R50_RESET_VALUE                   ((uint16_t)0x001U)
#define WM8978_R51_RESET_VALUE                   ((uint16_t)0x001U)
#define WM8978_R52_RESET_VALUE                   ((uint16_t)0x039U)
#define WM8978_R53_RESET_VALUE                   ((uint16_t)0x039U)
#define WM8978_R54_RESET_VALUE                   ((uint16_t)0x039U)
#define WM8978_R55_RESET_VALUE                   ((uint16_t)0x039U)
#define WM8978_R56_RESET_VALUE                   ((uint16_t)0x001U)
#define WM8978_R57_RESET_VALUE                   ((uint16_t)0x001U)

/* R1 - Power management 1. */
#define WM8978_R01_BUFDCOPEN                     ((uint16_t)0x100U)
#define WM8978_R01_OUT4MIXEN                     ((uint16_t)0x080U)
#define WM8978_R01_OUT3MIXEN                     ((uint16_t)0x040U)
#define WM8978_R01_PLLEN                         ((uint16_t)0x020U)
#define WM8978_R01_MICBEN                        ((uint16_t)0x010U)
#define WM8978_R01_BIASEN                        ((uint16_t)0x008U)
#define WM8978_R01_BUFIOEN                       ((uint16_t)0x004U)
#define WM8978_R01_VMIDSEL_MASK                  ((uint16_t)0x003U)
#define WM8978_R01_VMIDSEL_SHIFT                 ((uint8_t)0U)

/* R2 - Power management 2. */
#define WM8978_R02_ROUT1EN                       ((uint16_t)0x100U)
#define WM8978_R02_LOUT1EN                       ((uint16_t)0x080U)
#define WM8978_R02_SLEEP                         ((uint16_t)0x040U)
#define WM8978_R02_BOOSTENR                      ((uint16_t)0x020U)
#define WM8978_R02_BOOSTENL                      ((uint16_t)0x010U)
#define WM8978_R02_INPPGAENR                     ((uint16_t)0x008U)
#define WM8978_R02_INPPGAENL                     ((uint16_t)0x004U)
#define WM8978_R02_ADCENR                        ((uint16_t)0x002U)
#define WM8978_R02_ADCENL                        ((uint16_t)0x001U)

/* R3 - Power management 3. */
#define WM8978_R03_OUT4EN                        ((uint16_t)0x100U)
#define WM8978_R03_OUT3EN                        ((uint16_t)0x080U)
#define WM8978_R03_LOUT2EN                       ((uint16_t)0x040U)
#define WM8978_R03_ROUT2EN                       ((uint16_t)0x020U)
#define WM8978_R03_RMIXEN                        ((uint16_t)0x008U)
#define WM8978_R03_LMIXEN                        ((uint16_t)0x004U)
#define WM8978_R03_DACENR                        ((uint16_t)0x002U)
#define WM8978_R03_DACENL                        ((uint16_t)0x001U)

/* R4 - Digital audio interface. */
#define WM8978_R04_BCP                           ((uint16_t)0x100U)
#define WM8978_R04_LRP                           ((uint16_t)0x080U)
#define WM8978_R04_WL_MASK                       ((uint16_t)0x060U)
#define WM8978_R04_WL_SHIFT                      ((uint8_t)5U)
#define WM8978_R04_FMT_MASK                      ((uint16_t)0x018U)
#define WM8978_R04_FMT_SHIFT                     ((uint8_t)3U)
#define WM8978_R04_DACLRSWAP                     ((uint16_t)0x004U)
#define WM8978_R04_ADCLRSWAP                     ((uint16_t)0x002U)
#define WM8978_R04_MONO                          ((uint16_t)0x001U)

/* R5 - Companding and loopback. */
#define WM8978_R05_WL8                           ((uint16_t)0x020U)
#define WM8978_R05_DAC_COMP_MASK                 ((uint16_t)0x018U)
#define WM8978_R05_DAC_COMP_SHIFT                ((uint8_t)3U)
#define WM8978_R05_ADC_COMP_MASK                 ((uint16_t)0x006U)
#define WM8978_R05_ADC_COMP_SHIFT                ((uint8_t)1U)
#define WM8978_R05_LOOPBACK                      ((uint16_t)0x001U)

/* R6 - Clock generation. */
#define WM8978_R06_CLKSEL                        ((uint16_t)0x100U)
#define WM8978_R06_MCLKDIV_MASK                  ((uint16_t)0x0E0U)
#define WM8978_R06_MCLKDIV_SHIFT                 ((uint8_t)5U)
#define WM8978_R06_BCLKDIV_MASK                  ((uint16_t)0x01CU)
#define WM8978_R06_BCLKDIV_SHIFT                 ((uint8_t)2U)
#define WM8978_R06_MS                            ((uint16_t)0x001U)

/* R7 - Filter sample-rate coefficients and zero-cross timeout clock. */
#define WM8978_R07_SR_MASK                       ((uint16_t)0x00EU)
#define WM8978_R07_SR_SHIFT                      ((uint8_t)1U)
#define WM8978_R07_SLOWCLKEN                     ((uint16_t)0x001U)

/* R8 - GPIO1. */
#define WM8978_R08_OPCLKDIV_MASK                 ((uint16_t)0x030U)
#define WM8978_R08_OPCLKDIV_SHIFT                ((uint8_t)4U)
#define WM8978_R08_GPIO1POL                      ((uint16_t)0x008U)
#define WM8978_R08_GPIO1SEL_MASK                 ((uint16_t)0x007U)
#define WM8978_R08_GPIO1SEL_SHIFT                ((uint8_t)0U)

/* R9 and R13 - Jack detection. */
#define WM8978_R09_JD_VMID_MASK                  ((uint16_t)0x180U)
#define WM8978_R09_JD_VMID_SHIFT                 ((uint8_t)7U)
#define WM8978_R09_JD_EN                         ((uint16_t)0x040U)
#define WM8978_R09_JD_SEL_MASK                   ((uint16_t)0x030U)
#define WM8978_R09_JD_SEL_SHIFT                  ((uint8_t)4U)
#define WM8978_R13_JD_EN1_MASK                   ((uint16_t)0x0F0U)
#define WM8978_R13_JD_EN1_SHIFT                  ((uint8_t)4U)
#define WM8978_R13_JD_EN0_MASK                   ((uint16_t)0x00FU)
#define WM8978_R13_JD_EN0_SHIFT                  ((uint8_t)0U)

/* R10 - DAC control. SOFTMUTE polarity conflicts within Rev 4.5. */
#define WM8978_R10_SOFTMUTE_RAW                  ((uint16_t)0x040U)
#define WM8978_R10_DACOSR128                     ((uint16_t)0x008U)
#define WM8978_R10_AMUTE                         ((uint16_t)0x004U)
#define WM8978_R10_DACPOLR                       ((uint16_t)0x002U)
#define WM8978_R10_DACPOLL                       ((uint16_t)0x001U)

/* R11/R12 and R15/R16 - Converter digital volumes. */
#define WM8978_CONVERTER_VU                      ((uint16_t)0x100U)
#define WM8978_CONVERTER_VOLUME_MASK             ((uint16_t)0x0FFU)
#define WM8978_CONVERTER_VOLUME_SHIFT            ((uint8_t)0U)

/* R14 - ADC control. */
#define WM8978_R14_HPFEN                         ((uint16_t)0x100U)
#define WM8978_R14_HPFAPP                        ((uint16_t)0x080U)
#define WM8978_R14_HPFCUT_MASK                   ((uint16_t)0x070U)
#define WM8978_R14_HPFCUT_SHIFT                  ((uint8_t)4U)
#define WM8978_R14_ADCOSR128                     ((uint16_t)0x008U)
#define WM8978_R14_ADCRPOL                       ((uint16_t)0x002U)
#define WM8978_R14_ADCLPOL                       ((uint16_t)0x001U)

/* R18-R22 - Five-band equalizer. */
#define WM8978_R18_EQ3DMODE                      ((uint16_t)0x100U)
#define WM8978_R19_EQ2BW                         ((uint16_t)0x100U)
#define WM8978_R20_EQ3BW                         ((uint16_t)0x100U)
#define WM8978_R21_EQ4BW                         ((uint16_t)0x100U)
#define WM8978_EQ_C_MASK                         ((uint16_t)0x060U)
#define WM8978_EQ_C_SHIFT                        ((uint8_t)5U)
#define WM8978_EQ_G_MASK                         ((uint16_t)0x01FU)
#define WM8978_EQ_G_SHIFT                        ((uint8_t)0U)

/* R24/R25 - DAC limiter. */
#define WM8978_R24_LIMEN                         ((uint16_t)0x100U)
#define WM8978_R24_LIMDCY_MASK                   ((uint16_t)0x0F0U)
#define WM8978_R24_LIMDCY_SHIFT                  ((uint8_t)4U)
#define WM8978_R24_LIMATK_MASK                   ((uint16_t)0x00FU)
#define WM8978_R24_LIMATK_SHIFT                  ((uint8_t)0U)
#define WM8978_R25_LIMLVL_MASK                   ((uint16_t)0x070U)
#define WM8978_R25_LIMLVL_SHIFT                  ((uint8_t)4U)
#define WM8978_R25_LIMBOOST_MASK                 ((uint16_t)0x00FU)
#define WM8978_R25_LIMBOOST_SHIFT                ((uint8_t)0U)

/* R27-R30 - Notch filter. NFU is treated as a one-shot update request. */
#define WM8978_NOTCH_NFU                         ((uint16_t)0x100U)
#define WM8978_R27_NFEN                          ((uint16_t)0x080U)
#define WM8978_NOTCH_COEFFICIENT_MASK            ((uint16_t)0x07FU)

/* R32-R35 - ALC and noise gate. */
#define WM8978_R32_ALCSEL_MASK                   ((uint16_t)0x180U)
#define WM8978_R32_ALCSEL_SHIFT                  ((uint8_t)7U)
#define WM8978_R32_ALCMAXGAIN_MASK               ((uint16_t)0x038U)
#define WM8978_R32_ALCMAXGAIN_SHIFT              ((uint8_t)3U)
#define WM8978_R32_ALCMINGAIN_MASK               ((uint16_t)0x007U)
#define WM8978_R32_ALCMINGAIN_SHIFT              ((uint8_t)0U)
#define WM8978_R33_ALCHLD_MASK                   ((uint16_t)0x0F0U)
#define WM8978_R33_ALCHLD_SHIFT                  ((uint8_t)4U)
#define WM8978_R33_ALCLVL_MASK                   ((uint16_t)0x00FU)
#define WM8978_R33_ALCLVL_SHIFT                  ((uint8_t)0U)
#define WM8978_R34_ALCMODE                       ((uint16_t)0x100U)
#define WM8978_R34_ALCDCY_MASK                   ((uint16_t)0x0F0U)
#define WM8978_R34_ALCDCY_SHIFT                  ((uint8_t)4U)
#define WM8978_R34_ALCATK_MASK                   ((uint16_t)0x00FU)
#define WM8978_R34_ALCATK_SHIFT                  ((uint8_t)0U)
#define WM8978_R35_NGEN                          ((uint16_t)0x008U)
#define WM8978_R35_NGTH_MASK                     ((uint16_t)0x007U)
#define WM8978_R35_NGTH_SHIFT                    ((uint8_t)0U)

/* R36-R39 - PLL ratio. */
#define WM8978_R36_PLLPRESCALE                   ((uint16_t)0x010U)
#define WM8978_R36_PLLN_MASK                     ((uint16_t)0x00FU)
#define WM8978_R36_PLLN_SHIFT                    ((uint8_t)0U)
#define WM8978_R37_PLLK_MASK                     ((uint16_t)0x03FU)
#define WM8978_R38_PLLK_MASK                     ((uint16_t)0x1FFU)
#define WM8978_R39_PLLK_MASK                     ((uint16_t)0x1FFU)

/* R41/R43/R44 - 3D, beep, and analogue input selection. */
#define WM8978_R41_DEPTH3D_MASK                  ((uint16_t)0x00FU)
#define WM8978_R43_MUTERPGA2INV                  ((uint16_t)0x020U)
#define WM8978_R43_INVROUT2                      ((uint16_t)0x010U)
#define WM8978_R43_BEEPVOL_MASK                  ((uint16_t)0x00EU)
#define WM8978_R43_BEEPVOL_SHIFT                 ((uint8_t)1U)
#define WM8978_R43_BEEPEN                        ((uint16_t)0x001U)
#define WM8978_R44_MBVSEL                        ((uint16_t)0x100U)
#define WM8978_R44_R2_2INPPGA                    ((uint16_t)0x040U)
#define WM8978_R44_RIN2INPPGA                    ((uint16_t)0x020U)
#define WM8978_R44_RIP2INPPGA                    ((uint16_t)0x010U)
#define WM8978_R44_L2_2INPPGA                    ((uint16_t)0x004U)
#define WM8978_R44_LIN2INPPGA                    ((uint16_t)0x002U)
#define WM8978_R44_LIP2INPPGA                    ((uint16_t)0x001U)

/* R45/R46 - Input PGA. UPDATE is a non-latched trigger. */
#define WM8978_INPUT_PGA_UPDATE                  ((uint16_t)0x100U)
#define WM8978_INPUT_PGA_ZC                      ((uint16_t)0x080U)
#define WM8978_INPUT_PGA_MUTE                    ((uint16_t)0x040U)
#define WM8978_INPUT_PGA_VOLUME_MASK             ((uint16_t)0x03FU)

/* R47/R48 - ADC boost paths. */
#define WM8978_ADC_BOOST_PGA                     ((uint16_t)0x100U)
#define WM8978_ADC_BOOST_STAGE2_MASK             ((uint16_t)0x070U)
#define WM8978_ADC_BOOST_STAGE2_SHIFT            ((uint8_t)4U)
#define WM8978_ADC_BOOST_AUX_MASK                ((uint16_t)0x007U)
#define WM8978_ADC_BOOST_AUX_SHIFT               ((uint8_t)0U)

/* R49 - Output controls. */
#define WM8978_R49_DACL2RMIX                     ((uint16_t)0x040U)
#define WM8978_R49_DACR2LMIX                     ((uint16_t)0x020U)
#define WM8978_R49_OUT4BOOST                     ((uint16_t)0x010U)
#define WM8978_R49_OUT3BOOST                     ((uint16_t)0x008U)
#define WM8978_R49_SPKBOOST                      ((uint16_t)0x004U)
#define WM8978_R49_TSDEN                         ((uint16_t)0x002U)
#define WM8978_R49_VROI                          ((uint16_t)0x001U)

/* R50/R51 - Output mixers. */
#define WM8978_MIX_AUXVOL_MASK                   ((uint16_t)0x1C0U)
#define WM8978_MIX_AUXVOL_SHIFT                  ((uint8_t)6U)
#define WM8978_MIX_AUX2MIX                      ((uint16_t)0x020U)
#define WM8978_MIX_BYPVOL_MASK                   ((uint16_t)0x01CU)
#define WM8978_MIX_BYPVOL_SHIFT                  ((uint8_t)2U)
#define WM8978_MIX_BYP2MIX                      ((uint16_t)0x002U)
#define WM8978_MIX_DAC2MIX                      ((uint16_t)0x001U)

/* R52-R55 - Headphone/speaker outputs. VU is a non-latched trigger. */
#define WM8978_OUTPUT_VU                         ((uint16_t)0x100U)
#define WM8978_OUTPUT_ZC                         ((uint16_t)0x080U)
#define WM8978_OUTPUT_MUTE                       ((uint16_t)0x040U)
#define WM8978_OUTPUT_VOLUME_MASK                ((uint16_t)0x03FU)

/* R56/R57 - OUT3/OUT4 mixers. */
#define WM8978_R56_OUT3MUTE                      ((uint16_t)0x040U)
#define WM8978_R56_OUT4_2OUT3                    ((uint16_t)0x008U)
#define WM8978_R56_BYPL2OUT3                     ((uint16_t)0x004U)
#define WM8978_R56_LMIX2OUT3                     ((uint16_t)0x002U)
#define WM8978_R56_LDAC2OUT3                     ((uint16_t)0x001U)
#define WM8978_R57_OUT4MUTE                      ((uint16_t)0x040U)
#define WM8978_R57_HALFSIG                       ((uint16_t)0x020U)
#define WM8978_R57_LMIX2OUT4                     ((uint16_t)0x010U)
#define WM8978_R57_LDAC2OUT4                     ((uint16_t)0x008U)
#define WM8978_R57_BYPR2OUT4                     ((uint16_t)0x004U)
#define WM8978_R57_RMIX2OUT4                     ((uint16_t)0x002U)
#define WM8978_R57_RDAC2OUT4                     ((uint16_t)0x001U)

#endif /* WM8978_REGS_H */
