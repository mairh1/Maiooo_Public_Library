/**
 * @file    drv2605l_regs.h
 * @brief   DRV2605L 寄存器地址、位定义和数据手册常量
 * @details 本文件完整描述 DRV2605L 的 8 位寄存器映射，供核心驱动和需要
 *          使用诊断、自动校准或其它高级模式的应用做原始访问。
 * @note    高级寄存器写入可能改变输出行为或触发一次性 OTP 操作，调用者
 *          必须依据数据手册确认写入顺序和硬件条件。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-01
 */

#ifndef DRV2605L_REGS_H
#define DRV2605L_REGS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ══════════════════════════ 器件与协议常量 ══════════════════════════ */

#define DRV2605L_I2C_ADDR                0x5Au   /**< 固定 7 位 I2C 地址 */
#define DRV2605L_I2C_MAX_HZ              400000UL /**< 推荐最高 SCL 频率 */
#define DRV2605L_POWERUP_DELAY_MS       1u      /**< 覆盖上电后至少 250µs */
#define DRV2605L_SEQUENCE_SLOTS         8u      /**< ROM 序列槽数量 */
#define DRV2605L_WAVEFORM_ID_MAX        123u    /**< 合法波形编号上限 */
#define DRV2605L_SEQUENCE_WAIT_MASK     0x80u   /**< 序列项等待标志 */
#define DRV2605L_SEQUENCE_VALUE_MASK    0x7Fu   /**< 序列项低 7 位 */
#define DRV2605L_SEQUENCE_WAIT_UNIT_MS  10u     /**< 等待项每 LSB 的毫秒数 */
#define DRV2605L_DEVICE_ID_EXPECTED     7u      /**< Status.DEVICE_ID=DRV2605L */

/* ══════════════════════════ 寄存器地址 ══════════════════════════ */

#define DRV2605L_REG_STATUS             0x00u   /**< 状态与器件 ID */
#define DRV2605L_REG_MODE               0x01u   /**< 模式、待机和复位 */
#define DRV2605L_REG_RTP_INPUT          0x02u   /**< RTP 实时输入 */
#define DRV2605L_REG_LIBRARY_SELECTION  0x03u   /**< HI_Z 与 ROM 库选择 */
#define DRV2605L_REG_WAVEFORM_SEQ1      0x04u   /**< 波形序列槽 1 */
#define DRV2605L_REG_WAVEFORM_SEQ2      0x05u   /**< 波形序列槽 2 */
#define DRV2605L_REG_WAVEFORM_SEQ3      0x06u   /**< 波形序列槽 3 */
#define DRV2605L_REG_WAVEFORM_SEQ4      0x07u   /**< 波形序列槽 4 */
#define DRV2605L_REG_WAVEFORM_SEQ5      0x08u   /**< 波形序列槽 5 */
#define DRV2605L_REG_WAVEFORM_SEQ6      0x09u   /**< 波形序列槽 6 */
#define DRV2605L_REG_WAVEFORM_SEQ7      0x0Au   /**< 波形序列槽 7 */
#define DRV2605L_REG_WAVEFORM_SEQ8      0x0Bu   /**< 波形序列槽 8 */
#define DRV2605L_REG_GO                 0x0Cu   /**< 过程触发与播放状态 */
#define DRV2605L_REG_ODT                0x0Du   /**< Overdrive 时间偏移 */
#define DRV2605L_REG_SPT                0x0Eu   /**< Sustain 正偏移 */
#define DRV2605L_REG_SNT                0x0Fu   /**< Sustain 负偏移 */
#define DRV2605L_REG_BRT                0x10u   /**< Brake 时间偏移 */
#define DRV2605L_REG_AUDIO_VIBE_CTRL    0x11u   /**< Audio-to-vibe 控制 */
#define DRV2605L_REG_ATH_MIN_INPUT      0x12u   /**< Audio 最小输入 */
#define DRV2605L_REG_ATH_MAX_INPUT      0x13u   /**< Audio 最大输入 */
#define DRV2605L_REG_ATH_MIN_DRIVE      0x14u   /**< Audio 最小输出 */
#define DRV2605L_REG_ATH_MAX_DRIVE      0x15u   /**< Audio 最大输出 */
#define DRV2605L_REG_RATED_VOLTAGE      0x16u   /**< 额定电压 */
#define DRV2605L_REG_OD_CLAMP           0x17u   /**< Overdrive 电压钳位 */
#define DRV2605L_REG_A_CAL_COMP         0x18u   /**< 自动校准补偿结果 */
#define DRV2605L_REG_A_CAL_BEMF         0x19u   /**< 自动校准反电动势结果 */
#define DRV2605L_REG_FEEDBACK_CTRL      0x1Au   /**< ERM/LRA 与反馈控制 */
#define DRV2605L_REG_CONTROL1           0x1Bu   /**< 启动与 LRA drive time */
#define DRV2605L_REG_CONTROL2           0x1Cu   /**< 采样、制动与耗散 */
#define DRV2605L_REG_CONTROL3           0x1Du   /**< RTP、ERM/LRA 和输入格式 */
#define DRV2605L_REG_CONTROL4           0x1Eu   /**< 校准时间与 OTP */
#define DRV2605L_REG_CONTROL5           0x1Fu   /**< LRA 自动开环与间隔 */
#define DRV2605L_REG_OL_LRA_PERIOD      0x20u   /**< 开环 LRA 周期 */
#define DRV2605L_REG_VBAT               0x21u   /**< 电源电压测量 */
#define DRV2605L_REG_LRA_PERIOD         0x22u   /**< LRA 周期测量 */
#define DRV2605L_REG_FIRST              DRV2605L_REG_STATUS
#define DRV2605L_REG_LAST               DRV2605L_REG_LRA_PERIOD

/* ══════════════════════════ 复位值 ══════════════════════════ */

#define DRV2605L_RESET_STATUS           0xE0u
#define DRV2605L_RESET_MODE             0x40u
#define DRV2605L_RESET_RTP_INPUT        0x00u
#define DRV2605L_RESET_LIBRARY          0x01u
#define DRV2605L_RESET_WAVEFORM_SEQ     0x00u
#define DRV2605L_RESET_GO               0x00u
#define DRV2605L_RESET_ODT              0x00u
#define DRV2605L_RESET_SPT              0x00u
#define DRV2605L_RESET_SNT              0x00u
#define DRV2605L_RESET_BRT              0x00u
#define DRV2605L_RESET_AUDIO_VIBE_CTRL  0x05u
#define DRV2605L_RESET_ATH_MIN_INPUT    0x19u
#define DRV2605L_RESET_ATH_MAX_INPUT    0xFFu
#define DRV2605L_RESET_ATH_MIN_DRIVE    0x19u
#define DRV2605L_RESET_ATH_MAX_DRIVE    0xFFu
#define DRV2605L_RESET_RATED_VOLTAGE    0x3Eu
#define DRV2605L_RESET_OD_CLAMP         0x8Cu
#define DRV2605L_RESET_A_CAL_COMP       0x0Cu
#define DRV2605L_RESET_A_CAL_BEMF       0x6Cu
#define DRV2605L_RESET_FEEDBACK_CTRL    0x36u
#define DRV2605L_RESET_CONTROL1         0x93u
#define DRV2605L_RESET_CONTROL2         0xF5u
#define DRV2605L_RESET_CONTROL3         0xA0u
#define DRV2605L_RESET_CONTROL4         0x20u
#define DRV2605L_RESET_CONTROL5         0x80u
#define DRV2605L_RESET_OL_LRA_PERIOD    0x33u
#define DRV2605L_RESET_VBAT             0x00u
#define DRV2605L_RESET_LRA_PERIOD       0x00u

/* ══════════════════════════ Status（0x00） ══════════════════════════ */

#define DRV2605L_STATUS_DEVICE_ID_MASK  0xE0u
#define DRV2605L_STATUS_DEVICE_ID_SHIFT 5u
#define DRV2605L_STATUS_DIAG_RESULT     0x08u
#define DRV2605L_STATUS_OVER_TEMP       0x02u
#define DRV2605L_STATUS_OC_DETECT       0x01u

/* ══════════════════════════ Mode（0x01） ══════════════════════════ */

#define DRV2605L_MODE_DEV_RESET         0x80u
#define DRV2605L_MODE_STANDBY           0x40u
#define DRV2605L_MODE_VALUE_MASK        0x07u

/* ══════════════════════════ Library（0x03） ══════════════════════════ */

#define DRV2605L_LIBRARY_HI_Z           0x80u
#define DRV2605L_LIBRARY_VALUE_MASK     0x07u

/* ══════════════════════════ Sequence（0x04–0x0B） ══════════════════════════ */

#define DRV2605L_SEQUENCE_WAIT_BIT      0x80u
#define DRV2605L_SEQUENCE_WAVEFORM_MASK 0x7Fu

/* ══════════════════════════ GO（0x0C） ══════════════════════════ */

#define DRV2605L_GO_BIT                0x01u

/* ══════════════════════════ Audio-to-vibe（0x11） ══════════════════════════ */

#define DRV2605L_AUDIO_PEAK_TIME_MASK   0x0Cu
#define DRV2605L_AUDIO_PEAK_TIME_SHIFT  2u
#define DRV2605L_AUDIO_FILTER_MASK      0x03u

/* ══════════════════════════ Feedback（0x1A） ══════════════════════════ */

#define DRV2605L_FEEDBACK_LRA_MASK      0x80u
#define DRV2605L_FEEDBACK_BRAKE_MASK    0x70u
#define DRV2605L_FEEDBACK_BRAKE_SHIFT   4u
#define DRV2605L_FEEDBACK_LOOP_GAIN_MASK 0x0Cu
#define DRV2605L_FEEDBACK_LOOP_GAIN_SHIFT 2u
#define DRV2605L_FEEDBACK_BEMF_GAIN_MASK 0x03u

/* ══════════════════════════ Control1（0x1B） ══════════════════════════ */

#define DRV2605L_CONTROL1_STARTUP_BOOST 0x80u
#define DRV2605L_CONTROL1_AC_COUPLE     0x20u
#define DRV2605L_CONTROL1_DRIVE_TIME_MASK 0x1Fu

/* ══════════════════════════ Control2（0x1C） ══════════════════════════ */

#define DRV2605L_CONTROL2_BIDIR_INPUT   0x80u
#define DRV2605L_CONTROL2_BRAKE_STABILIZER 0x40u
#define DRV2605L_CONTROL2_SAMPLE_TIME_MASK 0x30u
#define DRV2605L_CONTROL2_SAMPLE_TIME_SHIFT 4u
#define DRV2605L_CONTROL2_BLANKING_TIME_MASK 0x0Cu
#define DRV2605L_CONTROL2_BLANKING_TIME_SHIFT 2u
#define DRV2605L_CONTROL2_IDISS_TIME_MASK 0x03u

/* ══════════════════════════ Control3（0x1D） ══════════════════════════ */

#define DRV2605L_CONTROL3_NG_THRESH_MASK 0xC0u
#define DRV2605L_CONTROL3_NG_THRESH_SHIFT 6u
#define DRV2605L_CONTROL3_ERM_OPEN_LOOP  0x20u
#define DRV2605L_CONTROL3_SUPPLY_COMP_DIS 0x10u
#define DRV2605L_CONTROL3_DATA_FORMAT_RTP 0x08u
#define DRV2605L_CONTROL3_LRA_DRIVE_MODE 0x04u
#define DRV2605L_CONTROL3_N_PWM_ANALOG  0x02u
#define DRV2605L_CONTROL3_LRA_OPEN_LOOP 0x01u

/* ══════════════════════════ Control4（0x1E） ══════════════════════════ */

#define DRV2605L_CONTROL4_ZC_DET_TIME_MASK 0xC0u
#define DRV2605L_CONTROL4_ZC_DET_TIME_SHIFT 6u
#define DRV2605L_CONTROL4_AUTO_CAL_TIME_MASK 0x30u
#define DRV2605L_CONTROL4_AUTO_CAL_TIME_SHIFT 4u
#define DRV2605L_CONTROL4_OTP_STATUS     0x04u
#define DRV2605L_CONTROL4_OTP_PROGRAM    0x01u

/* ══════════════════════════ Control5（0x1F） ══════════════════════════ */

#define DRV2605L_CONTROL5_AUTO_OL_CNT_MASK 0xC0u
#define DRV2605L_CONTROL5_AUTO_OL_CNT_SHIFT 6u
#define DRV2605L_CONTROL5_LRA_AUTO_OPEN_LOOP 0x20u
#define DRV2605L_CONTROL5_PLAYBACK_INTERVAL 0x10u
#define DRV2605L_CONTROL5_BLANKING_TIME_EXT 0x0Cu
#define DRV2605L_CONTROL5_IDISS_TIME_EXT 0x03u

#ifdef __cplusplus
}
#endif

#endif /* DRV2605L_REGS_H */
