/**
 * @file    aw32257_regs.h
 * @brief   AW32257 寄存器地址、位域掩码与手册给出的复位值
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef AW32257_REGS_H
#define AW32257_REGS_H

#include <stdint.h>

/* 器件与协议常量。 */
#define AW32257_I2C_ADDRESS_7BIT              ((uint8_t)0x6A)
#define AW32257_I2C_MAX_FREQUENCY_HZ          ((uint32_t)400000)
#define AW32257_SOFT_RESET_DELAY_MS           ((uint32_t)32)

/* 寄存器地址。 */
#define AW32257_REG_STATUS_CONTROL            ((uint8_t)0x00)
#define AW32257_REG_CONTROL                   ((uint8_t)0x01)
#define AW32257_REG_BATTERY_VOLTAGE           ((uint8_t)0x02)
#define AW32257_REG_DEVICE_ID                 ((uint8_t)0x03)
#define AW32257_REG_CHARGE_CURRENT            ((uint8_t)0x04)
#define AW32257_REG_DPM_STATUS                ((uint8_t)0x05)
#define AW32257_REG_SAFETY_LIMIT              ((uint8_t)0x06)
#define AW32257_REG_TERMINATION                ((uint8_t)0x07)
#define AW32257_REG_VENDOR                    ((uint8_t)0x08)
#define AW32257_REG_BOOST_FAULT               ((uint8_t)0x09)
#define AW32257_REG_BOOST_CONFIG              ((uint8_t)0x0A)
#define AW32257_REG_FIRST                     AW32257_REG_STATUS_CONTROL
#define AW32257_REG_LAST                      AW32257_REG_BOOST_CONFIG

/* 手册给出的复位值。REG00 含动态/未定义位。 */
#define AW32257_REG00_RESET_KNOWN_MASK        ((uint8_t)0x48)
#define AW32257_REG00_RESET_KNOWN_VALUE       ((uint8_t)0x40)
#define AW32257_REG01_RESET_VALUE             ((uint8_t)0x30)
#define AW32257_REG02_RESET_VALUE             ((uint8_t)0x0A)
#define AW32257_REG03_RESET_VALUE             ((uint8_t)0x53)
#define AW32257_REG04_RESET_VALUE             ((uint8_t)0x01)
#define AW32257_REG05_RESET_VALUE             ((uint8_t)0x24)
#define AW32257_REG06_RESET_VALUE             ((uint8_t)0x40)
#define AW32257_REG07_RESET_VALUE             ((uint8_t)0x11)
#define AW32257_REG08_RESET_VALUE             ((uint8_t)0xFF)
#define AW32257_REG09_RESET_VALUE             ((uint8_t)0x00)
#define AW32257_REG0A_RESET_VALUE             ((uint8_t)0x00)

/* REG00 - 状态/控制。 */
#define AW32257_REG00_OTG_PIN_MASK             ((uint8_t)0x80)
#define AW32257_REG00_EN_STAT_MASK             ((uint8_t)0x40)
#define AW32257_REG00_CHARGE_STATE_MASK        ((uint8_t)0x30)
#define AW32257_REG00_CHARGE_STATE_SHIFT       ((uint8_t)4)
#define AW32257_REG00_BOOST_ACTIVE_MASK        ((uint8_t)0x08)
#define AW32257_REG00_CHARGE_FAULT_MASK        ((uint8_t)0x07)
#define AW32257_REG00_WRITABLE_MASK            AW32257_REG00_EN_STAT_MASK

/* REG01 - 控制。 */
#define AW32257_REG01_NA_MASK                  ((uint8_t)0xF0)
#define AW32257_REG01_TERMINATION_ENABLE_MASK  ((uint8_t)0x08)
#define AW32257_REG01_CHARGE_DISABLE_MASK      ((uint8_t)0x04)
#define AW32257_REG01_HIGH_Z_MASK              ((uint8_t)0x02)
#define AW32257_REG01_BOOST_REQUEST_MASK       ((uint8_t)0x01)
#define AW32257_REG01_MODE_MASK                ((uint8_t)0x03)
#define AW32257_REG01_WRITABLE_MASK            ((uint8_t)0x0F)

/* REG02 - 电池调节电压与 OTG 引脚控制。 */
#define AW32257_REG02_VOREG_MASK               ((uint8_t)0xFC)
#define AW32257_REG02_VOREG_SHIFT              ((uint8_t)2)
#define AW32257_REG02_OTG_ACTIVE_HIGH_MASK     ((uint8_t)0x02)
#define AW32257_REG02_OTG_PIN_ENABLE_MASK      ((uint8_t)0x01)
#define AW32257_REG02_OTG_CONTROL_MASK         ((uint8_t)0x03)
#define AW32257_REG02_WRITABLE_MASK            ((uint8_t)0xFF)

/* REG03 - 器件识别。 */
#define AW32257_REG03_VENDOR_MASK              ((uint8_t)0xE0)
#define AW32257_REG03_VENDOR_SHIFT             ((uint8_t)5)
#define AW32257_REG03_PART_MASK                ((uint8_t)0x18)
#define AW32257_REG03_PART_SHIFT               ((uint8_t)3)
#define AW32257_REG03_REVISION_MASK            ((uint8_t)0x07)
#define AW32257_REG03_ID_MASK                  ((uint8_t)0xF8)
#define AW32257_REG03_ID_EXPECTED              ((uint8_t)0x50)
#define AW32257_REG03_WRITABLE_MASK            ((uint8_t)0x00)

/* REG04 - 软件复位与充电电流设置。 */
#define AW32257_REG04_SOFT_RESET_MASK          ((uint8_t)0x80)
#define AW32257_REG04_FAST_CURRENT_MASK        ((uint8_t)0x78)
#define AW32257_REG04_FAST_CURRENT_SHIFT       ((uint8_t)3)
#define AW32257_REG04_TERM_CURRENT_MASK        ((uint8_t)0x07)
#define AW32257_REG04_WRITABLE_MASK            ((uint8_t)0xFF)

/* REG05 - DPM 与引脚状态。 */
#define AW32257_REG05_NA_MASK                  ((uint8_t)0xE0)
#define AW32257_REG05_DPM_ACTIVE_MASK          ((uint8_t)0x10)
#define AW32257_REG05_CD_PIN_MASK              ((uint8_t)0x08)
#define AW32257_REG05_DPM_VOLTAGE_MASK         ((uint8_t)0x07)
#define AW32257_REG05_WRITABLE_MASK            AW32257_REG05_DPM_VOLTAGE_MASK

/* REG06 - 仅上电复位(POR)可写的安全限值。 */
#define AW32257_REG06_SAFE_CURRENT_MASK        ((uint8_t)0xF0)
#define AW32257_REG06_SAFE_CURRENT_SHIFT       ((uint8_t)4)
#define AW32257_REG06_SAFE_VOLTAGE_MASK        ((uint8_t)0x0F)
#define AW32257_REG06_WRITABLE_MASK            ((uint8_t)0xFF)

/* REG07 - 充电终止算法。 */
#define AW32257_REG07_WINDOW_PERIODS_MASK      ((uint8_t)0x80)
#define AW32257_REG07_VALID_PERIODS_MASK       ((uint8_t)0x60)
#define AW32257_REG07_VALID_PERIODS_SHIFT      ((uint8_t)5)
#define AW32257_REG07_DEGLITCH_MASK            ((uint8_t)0x18)
#define AW32257_REG07_DEGLITCH_SHIFT           ((uint8_t)3)
#define AW32257_REG07_NA_MASK                  ((uint8_t)0x04)
#define AW32257_REG07_RECHARGE_MASK            ((uint8_t)0x03)
#define AW32257_REG07_WRITABLE_MASK            ((uint8_t)0xFB)

/* REG08 - AWINIC 厂商编号。 */
#define AW32257_REG08_VENDOR_MASK              ((uint8_t)0xFF)
#define AW32257_REG08_WRITABLE_MASK            ((uint8_t)0x00)

/* REG09 - 升压(boost)故障。 */
#define AW32257_REG09_NA_MASK                  ((uint8_t)0xF8)
#define AW32257_REG09_BOOST_FAULT_MASK         ((uint8_t)0x07)
#define AW32257_REG09_WRITABLE_MASK            ((uint8_t)0x00)

/* REG0A - 升压输出与驱动配置。 */
#define AW32257_REG0A_FREQUENCY_MASK           ((uint8_t)0x80)
#define AW32257_REG0A_SLEW_RATE_MASK           ((uint8_t)0x60)
#define AW32257_REG0A_SLEW_RATE_SHIFT          ((uint8_t)5)
#define AW32257_REG0A_FIXED_DEAD_TIME_MASK     ((uint8_t)0x10)
#define AW32257_REG0A_FORCE_PWM_MASK           ((uint8_t)0x08)
#define AW32257_REG0A_NA_MASK                  ((uint8_t)0x04)
#define AW32257_REG0A_OUTPUT_VOLTAGE_MASK      ((uint8_t)0x03)
#define AW32257_REG0A_WRITABLE_MASK            ((uint8_t)0xFB)

#endif /* AW32257_REGS_H */
