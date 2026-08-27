/*
 * @file    ina219_regs.h
 * @brief   INA219 电流/功率监测芯片寄存器地址、位定义与常量
 * @details 全部寄存器为 16 位，I2C 传输高字节在前，两字节必须在同一
 *          事务内完成。寄存器位序与复位值依据本目录数据手册 Table 2~
 *          8 / Figure 19~27：INA219, ZHCSFN9G（2008-08 初版，2015-12
 *          修订，Texas Instruments）。
 * @note    寄存器全部为易失性：软件复位（RST 位）或掉电后回到 POR
 *          默认值，每次上电必须重新写入配置与校准值。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 */

#ifndef INA219_REGS_H
#define INA219_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 常量与换算参数
 * ══════════════════════════════════════════════════════════════════════════ */

#define INA219_I2C_ADDR             0x40    /**< 7 位从机地址基址（A1=A0=GND），
                                                 A1/A0 引脚组合共 16 个地址
                                                 0x40~0x4F */

#define INA219_SHUNT_LSB_UV         10      /**< 分流电压分辨率：10µV/LSB（各 PGA 档通用） */
#define INA219_BUS_LSB_MV           4       /**< 总线电压分辨率：4mV/LSB */
#define INA219_BUS_SHIFT            3       /**< 总线电压寄存器左对齐，需右移 3 位 */
#define INA219_CAL_FACTOR_NUM       40960000000000ULL /**< 校准公式分子
                                                 0.04096V 按 nA×µΩ 量纲放大 10^15 倍：
                                                 Cal = 0.04096 / (Current_LSB × R_SHUNT) */
#define INA219_CAL_MAX              0x7FFF  /**< 校准值上限（15 位字段极值；
                                                 bit0 为 void 位恒读 0，
                                                 驱动写入前自动清零） */
#define INA219_POWER_LSB_FACTOR     20      /**< 功率 LSB = 20 × Current_LSB（固定倍率） */

/* ══════════════════════════════════════════════════════════════════════════
 * 寄存器地址（Table 2）
 * ══════════════════════════════════════════════════════════════════════════ */

#define INA219_REG_CONFIG           0x00    /**< 配置寄存器，R/W，POR 0x399F */
#define INA219_REG_SHUNT            0x01    /**< 分流电压测量，R，二进制补码 */
#define INA219_REG_BUS              0x02    /**< 总线电压测量，R，左对齐 + 状态位 */
#define INA219_REG_POWER            0x03    /**< 功率测量，R，POR 0x0000（未校准时恒 0） */
#define INA219_REG_CURRENT          0x04    /**< 电流测量，R，POR 0x0000（未校准时恒 0） */
#define INA219_REG_CALIBRATION      0x05    /**< 校准寄存器，R/W，POR 0x0000 */

/* ══════════════════════════════════════════════════════════════════════════
 * 配置寄存器（0x00，POR 0x399F）位定义（Figure 19 / Table 3）
 * ══════════════════════════════════════════════════════════════════════════ */

#define INA219_CFG_RST              (1u << 15) /**< 写 1 触发全复位（同 POR），自清零 */
#define INA219_CFG_BRNG             (1u << 13) /**< 总线电压量程：0=16V，1=32V（默认） */

#define INA219_CFG_PG_SHIFT         11          /**< PGA 增益字段右移数（bit12:11） */
#define INA219_CFG_PG_MASK          (0x3u << 11) /**< PGA 增益字段：00=/1(±40mV)，
                                                 01=/2(±80mV)，10=/4(±160mV)，
                                                 11=/8(±320mV，POR 默认) */
#define INA219_CFG_BADC_SHIFT       7           /**< 总线 ADC 档位字段右移数（bit10:7） */
#define INA219_CFG_BADC_MASK        (0xFu << 7) /**< 总线 ADC 分辨率/平均档位字段 */
#define INA219_CFG_SADC_SHIFT       3           /**< 分流 ADC 档位字段右移数（bit6:3） */
#define INA219_CFG_SADC_MASK        (0xFu << 3) /**< 分流 ADC 分辨率/平均档位字段 */
#define INA219_CFG_MODE_SHIFT       0           /**< 工作模式字段右移数（bit2:0） */
#define INA219_CFG_MODE_MASK        (0x7u << 0) /**< 工作模式字段 */

#define INA219_CONFIG_POR           0x399F  /**< 配置寄存器复位值
                                                 （32V 量程 /8 增益 双 12bit 连续模式） */

/* ══════════════════════════════════════════════════════════════════════════
 * PGA 分流电压量程档位（Table 4，宏值 = PG 字段编码）
 * ══════════════════════════════════════════════════════════════════════════ */

#define INA219_PGA_RANGE_40MV       0   /**< 增益 /1，满量程 ±40mV */
#define INA219_PGA_RANGE_80MV       1   /**< 增益 /2，满量程 ±80mV */
#define INA219_PGA_RANGE_160MV      2   /**< 增益 /4，满量程 ±160mV */
#define INA219_PGA_RANGE_320MV      3   /**< 增益 /8，满量程 ±320mV（POR 默认） */

/* ══════════════════════════════════════════════════════════════════════════
 * ADC 分辨率/平均档位（Table 5，宏值 = BADC/SADC 4 位字段编码）
 * ══════════════════════════════════════════════════════════════════════════ */
/* ADC4=0 时 ADC3 位无关（手册 Don't care），本组取规范编码 0x0~0x3；
 * 0x4~0x8 与本组等价或非法，驱动不接受。 */

#define INA219_ADC_9BIT             0x0 /**< 9 bit，单次转换 84µs */
#define INA219_ADC_10BIT            0x1 /**< 10 bit，单次转换 148µs */
#define INA219_ADC_11BIT            0x2 /**< 11 bit，单次转换 276µs */
#define INA219_ADC_12BIT            0x3 /**< 12 bit，单次转换 532µs（POR 默认） */
#define INA219_ADC_2_SAMPLES        0x9 /**< 12 bit×2 次平均，1.06ms */
#define INA219_ADC_4_SAMPLES        0xA /**< 12 bit×4 次平均，2.13ms */
#define INA219_ADC_8_SAMPLES        0xB /**< 12 bit×8 次平均，4.26ms */
#define INA219_ADC_16_SAMPLES       0xC /**< 12 bit×16 次平均，8.51ms */
#define INA219_ADC_32_SAMPLES       0xD /**< 12 bit×32 次平均，17.02ms */
#define INA219_ADC_64_SAMPLES       0xE /**< 12 bit×64 次平均，34.05ms */
#define INA219_ADC_128_SAMPLES      0xF /**< 12 bit×128 次平均，68.10ms */

/* ══════════════════════════════════════════════════════════════════════════
 * 工作模式（Table 6，宏值 = MODE 字段编码）
 * ══════════════════════════════════════════════════════════════════════════ */

#define INA219_MODE_POWER_DOWN      0x0 /**< 掉电（挂起），电流典型 5µA */
#define INA219_MODE_TRIG_SHUNT      0x1 /**< 分流电压，触发单次 */
#define INA219_MODE_TRIG_BUS        0x2 /**< 总线电压，触发单次 */
#define INA219_MODE_TRIG_SHUNT_BUS  0x3 /**< 分流+总线，触发单次 */
#define INA219_MODE_ADC_OFF         0x4 /**< ADC 关闭（寄存器保持可达） */
#define INA219_MODE_CONT_SHUNT      0x5 /**< 分流电压，连续转换 */
#define INA219_MODE_CONT_BUS        0x6 /**< 总线电压，连续转换 */
#define INA219_MODE_CONT_SHUNT_BUS  0x7 /**< 分流+总线，连续转换（POR 默认） */

/* ══════════════════════════════════════════════════════════════════════════
 * 总线电压寄存器（0x02）状态位（Figure 24）
 * ══════════════════════════════════════════════════════════════════════════ */

#define INA219_BUS_CNVR             (1u << 1) /**< 转换完成标志：写 MODE 位或读
                                                  功率寄存器时硬件清除 */
#define INA219_BUS_OVF              (1u << 0) /**< 数学溢出标志：电流/功率计算
                                                  超范围，数据可能无意义 */

#ifdef __cplusplus
}
#endif

#endif /* INA219_REGS_H */
