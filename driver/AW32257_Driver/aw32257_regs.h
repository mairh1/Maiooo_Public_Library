/**
 * @file    aw32257_regs.h
 * @brief   AW32257 寄存器地址、位域掩码与手册给出的复位值
 * @details 数值取自 AW32257 V1.5 数据手册（Nov. 2023）寄存器表，
 *          按“器件/协议常量、寄存器地址、复位值、逐寄存器位域”
 *          分节组织。电流码点到 mA 的换算表在 aw32257.c 中，不在
 *          此处。
 * @note    AW32257_SOFT_RESET_DELAY_MS 是数据手册协议常量（软件复位
 *          后的强制静默时长），归属本文件而非 aw32257_conf.h。
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
#define AW32257_I2C_ADDRESS_7BIT              ((uint8_t)0x6A)      /**< 默认 7 位 I2C 器件地址 */
#define AW32257_I2C_MAX_FREQUENCY_HZ          ((uint32_t)400000)   /**< I2C 时钟上限，单位 Hz */
#define AW32257_SOFT_RESET_DELAY_MS           ((uint32_t)32)       /**< 软件复位后要求的 I2C 静默等待，单位 ms */

/* 寄存器地址。 */
#define AW32257_REG_STATUS_CONTROL            ((uint8_t)0x00)      /**< REG00：状态/控制 */
#define AW32257_REG_CONTROL                   ((uint8_t)0x01)      /**< REG01：控制 */
#define AW32257_REG_BATTERY_VOLTAGE           ((uint8_t)0x02)      /**< REG02：电池调节电压与 OTG 引脚控制 */
#define AW32257_REG_DEVICE_ID                 ((uint8_t)0x03)      /**< REG03：器件识别 */
#define AW32257_REG_CHARGE_CURRENT            ((uint8_t)0x04)      /**< REG04：软件复位与充电电流设置 */
#define AW32257_REG_DPM_STATUS                ((uint8_t)0x05)      /**< REG05：DPM 状态与引脚状态 */
#define AW32257_REG_SAFETY_LIMIT              ((uint8_t)0x06)      /**< REG06：仅 POR 可写的安全限值 */
#define AW32257_REG_TERMINATION                ((uint8_t)0x07)     /**< REG07：充电终止算法 */
#define AW32257_REG_VENDOR                    ((uint8_t)0x08)      /**< REG08：AWINIC 厂商编号 */
#define AW32257_REG_BOOST_FAULT               ((uint8_t)0x09)      /**< REG09：升压故障 */
#define AW32257_REG_BOOST_CONFIG              ((uint8_t)0x0A)      /**< REG0A：升压输出与驱动配置 */
#define AW32257_REG_FIRST                     AW32257_REG_STATUS_CONTROL  /**< 最小合法寄存器地址 */
#define AW32257_REG_LAST                      AW32257_REG_BOOST_CONFIG    /**< 最大合法寄存器地址 */

/* 手册给出的复位值。REG00 含动态/未定义位。 */
#define AW32257_REG00_RESET_KNOWN_MASK        ((uint8_t)0x48)      /**< REG00 中复位值可校验的位集合 */
#define AW32257_REG00_RESET_KNOWN_VALUE       ((uint8_t)0x40)      /**< 上述已知位的上电复位值 */
#define AW32257_REG01_RESET_VALUE             ((uint8_t)0x30)      /**< REG01 上电复位值 */
#define AW32257_REG02_RESET_VALUE             ((uint8_t)0x0A)      /**< REG02 上电复位值 */
#define AW32257_REG03_RESET_VALUE             ((uint8_t)0x53)      /**< REG03 上电复位值 */
#define AW32257_REG04_RESET_VALUE             ((uint8_t)0x01)      /**< REG04 上电复位值 */
#define AW32257_REG05_RESET_VALUE             ((uint8_t)0x24)      /**< REG05 上电复位值 */
#define AW32257_REG06_RESET_VALUE             ((uint8_t)0x40)      /**< REG06 上电复位值 */
#define AW32257_REG07_RESET_VALUE             ((uint8_t)0x11)      /**< REG07 上电复位值 */
#define AW32257_REG08_RESET_VALUE             ((uint8_t)0xFF)      /**< REG08 上电复位值 */
#define AW32257_REG09_RESET_VALUE             ((uint8_t)0x00)      /**< REG09 上电复位值 */
#define AW32257_REG0A_RESET_VALUE             ((uint8_t)0x00)      /**< REG0A 上电复位值 */

/* REG00 - 状态/控制。 */
#define AW32257_REG00_OTG_PIN_MASK             ((uint8_t)0x80)     /**< OTG 引脚电平状态（只读） */
#define AW32257_REG00_EN_STAT_MASK             ((uint8_t)0x40)     /**< STAT 开漏输出使能（可读写） */
#define AW32257_REG00_CHARGE_STATE_MASK        ((uint8_t)0x30)     /**< 充电状态字段掩码 */
#define AW32257_REG00_CHARGE_STATE_SHIFT       ((uint8_t)4)        /**< 充电状态字段右移位数 */
#define AW32257_REG00_BOOST_ACTIVE_MASK        ((uint8_t)0x08)     /**< 升压工作标志（只读） */
#define AW32257_REG00_CHARGE_FAULT_MASK        ((uint8_t)0x07)     /**< 充电故障码字段掩码 */
#define AW32257_REG00_WRITABLE_MASK            AW32257_REG00_EN_STAT_MASK /**< REG00 可写位集合（仅 EN_STAT） */

/* REG01 - 控制。 */
#define AW32257_REG01_NA_MASK                  ((uint8_t)0xF0)     /**< 保留位（不使用，读-改-写时原样保持） */
#define AW32257_REG01_TERMINATION_ENABLE_MASK  ((uint8_t)0x08)     /**< 充电终止判定使能 */
#define AW32257_REG01_CHARGE_DISABLE_MASK      ((uint8_t)0x04)     /**< 充电禁止（写 1 关闭充电，反相语义） */
#define AW32257_REG01_HIGH_Z_MASK              ((uint8_t)0x02)     /**< 高阻模式请求 */
#define AW32257_REG01_BOOST_REQUEST_MASK       ((uint8_t)0x01)     /**< 升压模式请求 */
#define AW32257_REG01_MODE_MASK                ((uint8_t)0x03)     /**< 工作模式字段掩码（高阻/升压请求位组合） */
#define AW32257_REG01_WRITABLE_MASK            ((uint8_t)0x0F)     /**< REG01 可写位集合 */

/* REG02 - 电池调节电压与 OTG 引脚控制。 */
#define AW32257_REG02_VOREG_MASK               ((uint8_t)0xFC)     /**< 充电调节电压 VOREG 字段掩码 */
#define AW32257_REG02_VOREG_SHIFT              ((uint8_t)2)        /**< VOREG 字段右移位数 */
#define AW32257_REG02_OTG_ACTIVE_HIGH_MASK     ((uint8_t)0x02)     /**< OTG 引脚极性：1 = 高电平有效 */
#define AW32257_REG02_OTG_PIN_ENABLE_MASK      ((uint8_t)0x01)     /**< OTG 引脚控制使能 */
#define AW32257_REG02_OTG_CONTROL_MASK         ((uint8_t)0x03)     /**< OTG 引脚控制字段掩码 */
#define AW32257_REG02_WRITABLE_MASK            ((uint8_t)0xFF)     /**< REG02 可写位集合 */

/* REG03 - 器件识别。 */
#define AW32257_REG03_VENDOR_MASK              ((uint8_t)0xE0)     /**< 厂商标识码字段掩码 */
#define AW32257_REG03_VENDOR_SHIFT             ((uint8_t)5)        /**< 厂商标识码右移位数 */
#define AW32257_REG03_PART_MASK                ((uint8_t)0x18)     /**< 型号码字段掩码 */
#define AW32257_REG03_PART_SHIFT               ((uint8_t)3)        /**< 型号码右移位数 */
#define AW32257_REG03_REVISION_MASK            ((uint8_t)0x07)     /**< 修订码字段掩码 */
#define AW32257_REG03_ID_MASK                  ((uint8_t)0xF8)     /**< 身份校验掩码（厂商 + 型号） */
#define AW32257_REG03_ID_EXPECTED              ((uint8_t)0x50)     /**< 身份校验期望值（修订码不参与比较） */
#define AW32257_REG03_WRITABLE_MASK            ((uint8_t)0x00)     /**< REG03 只读 */

/* REG04 - 软件复位与充电电流设置。 */
#define AW32257_REG04_SOFT_RESET_MASK          ((uint8_t)0x80)     /**< 写 1 触发软件复位（触发位，普通写须显式清零） */
#define AW32257_REG04_FAST_CURRENT_MASK        ((uint8_t)0x78)     /**< 快充电流码点字段掩码 */
#define AW32257_REG04_FAST_CURRENT_SHIFT       ((uint8_t)3)        /**< 快充电流码点右移位数 */
#define AW32257_REG04_TERM_CURRENT_MASK        ((uint8_t)0x07)     /**< 终止电流码点字段掩码 */
#define AW32257_REG04_WRITABLE_MASK            ((uint8_t)0xFF)     /**< REG04 可写位集合 */

/* REG05 - DPM 与引脚状态。 */
#define AW32257_REG05_NA_MASK                  ((uint8_t)0xE0)     /**< 保留位（不使用，读-改-写时原样保持） */
#define AW32257_REG05_DPM_ACTIVE_MASK          ((uint8_t)0x10)     /**< DPM（输入电压动态调节）激活标志 */
#define AW32257_REG05_CD_PIN_MASK              ((uint8_t)0x08)     /**< CD 引脚电平状态（只读） */
#define AW32257_REG05_DPM_VOLTAGE_MASK         ((uint8_t)0x07)     /**< DPM 电压档位字段掩码 */
#define AW32257_REG05_WRITABLE_MASK            AW32257_REG05_DPM_VOLTAGE_MASK /**< REG05 可写位集合（仅 DPM 电压档位） */

/* REG06 - 仅上电复位(POR)可写的安全限值。 */
#define AW32257_REG06_SAFE_CURRENT_MASK        ((uint8_t)0xF0)     /**< 最大安全电流码点字段掩码 */
#define AW32257_REG06_SAFE_CURRENT_SHIFT       ((uint8_t)4)        /**< 最大安全电流码点右移位数 */
#define AW32257_REG06_SAFE_VOLTAGE_MASK        ((uint8_t)0x0F)     /**< 最大安全电压档位字段掩码 */
#define AW32257_REG06_WRITABLE_MASK            ((uint8_t)0xFF)     /**< REG06 可写位集合（仅 POR 后有效，之后锁定） */

/* REG07 - 充电终止算法。 */
#define AW32257_REG07_WINDOW_PERIODS_MASK      ((uint8_t)0x80)     /**< CTA 窗口周期选择：0 = 8 周期，1 = 16 周期 */
#define AW32257_REG07_VALID_PERIODS_MASK       ((uint8_t)0x60)     /**< 终止有效周期数字段掩码 */
#define AW32257_REG07_VALID_PERIODS_SHIFT      ((uint8_t)5)        /**< 有效周期数右移位数 */
#define AW32257_REG07_DEGLITCH_MASK            ((uint8_t)0x18)     /**< 单周期去抖时间字段掩码 */
#define AW32257_REG07_DEGLITCH_SHIFT           ((uint8_t)3)        /**< 去抖时间右移位数 */
#define AW32257_REG07_NA_MASK                  ((uint8_t)0x04)     /**< 保留位（不使用，读-改-写时原样保持） */
#define AW32257_REG07_RECHARGE_MASK            ((uint8_t)0x03)     /**< 再充电阈值档位字段掩码 */
#define AW32257_REG07_WRITABLE_MASK            ((uint8_t)0xFB)     /**< REG07 可写位集合（保留位除外） */

/* REG08 - AWINIC 厂商编号。 */
#define AW32257_REG08_VENDOR_MASK              ((uint8_t)0xFF)     /**< 厂商编号字节掩码 */
#define AW32257_REG08_WRITABLE_MASK            ((uint8_t)0x00)     /**< REG08 只读 */

/* REG09 - 升压(boost)故障。 */
#define AW32257_REG09_NA_MASK                  ((uint8_t)0xF8)     /**< 保留位（不使用，读-改-写时原样保持） */
#define AW32257_REG09_BOOST_FAULT_MASK         ((uint8_t)0x07)     /**< 升压故障码字段掩码 */
#define AW32257_REG09_WRITABLE_MASK            ((uint8_t)0x00)     /**< REG09 只读 */

/* REG0A - 升压输出与驱动配置。 */
#define AW32257_REG0A_FREQUENCY_MASK           ((uint8_t)0x80)     /**< 开关频率选择：0 = 1500 kHz，1 = 1700 kHz */
#define AW32257_REG0A_SLEW_RATE_MASK           ((uint8_t)0x60)     /**< 功率级驱动压摆率字段掩码 */
#define AW32257_REG0A_SLEW_RATE_SHIFT          ((uint8_t)5)        /**< 压摆率字段右移位数 */
#define AW32257_REG0A_FIXED_DEAD_TIME_MASK     ((uint8_t)0x10)     /**< 固定死时间使能 */
#define AW32257_REG0A_FORCE_PWM_MASK           ((uint8_t)0x08)     /**< 强制 PWM 模式使能 */
#define AW32257_REG0A_NA_MASK                  ((uint8_t)0x04)     /**< 保留位（不使用，读-改-写时原样保持） */
#define AW32257_REG0A_OUTPUT_VOLTAGE_MASK      ((uint8_t)0x03)     /**< 升压输出电压档位字段掩码 */
#define AW32257_REG0A_WRITABLE_MASK            ((uint8_t)0xFB)     /**< REG0A 可写位集合（保留位除外） */

#endif /* AW32257_REGS_H */
