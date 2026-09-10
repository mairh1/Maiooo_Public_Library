/**
 * @file    sgm41513_regs.h
 * @brief   SGM41513 系列寄存器与位域定义
 * @details 器件：SGM41513 / SGM41513A / SGM41513D（SG Micro 圣邦微）。
 *          全部寄存器 8 位，共 16 个（0x00 - 0x0F）。读 0x0F 以外越界
 *          返回 0xFF。REG09 与 REG0E 禁止多字节（突发(burst)）I2C
 *          访问——因此驱动只执行单字节读写。
 * @note    本头文件属于驱动内部文件，但对外暴露，便于高级用户通过
 *          sgm41513_read_reg() / sgm41513_write_reg() /
 *          sgm41513_update_bits() 进行寄存器级访问。
 *          正常使用不需要改动本文件任何内容。
 *          数据手册：SGM41513_SGM41513A_SGM41513D, APRIL 2025 REV. C.1。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-10
 */

#ifndef SGM41513_REGS_H
#define SGM41513_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * 器件常量
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_NUM_REGS           16u     /**< REG00 .. REG0F */
#define SGM41513_I2C_ADDR           0x1Au   /**< 7 位从机地址 */
#define SGM41513_I2C_MAX_FREQ_HZ    400000u /**< I2C 快速模式频率上限 */

/* ═════════════════════════════════════════════════════════════════════════════
 * 寄存器地址
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_REG00  0x00u /**< EN_HIZ, EN_ICHG_MON, IINDPM[4:0] */
#define SGM41513_REG01  0x01u /**< PFM_DIS, WD_RST, OTG_CONFIG, CHG_CONFIG,
                                   SYS_MIN[2:0], MIN_BAT_SEL */
#define SGM41513_REG02  0x02u /**< BOOST_LIM, Q1_FULLON, ICHG[5:0] */
#define SGM41513_REG03  0x03u /**< IPRECHG[3:0], ITERM[3:0] */
#define SGM41513_REG04  0x04u /**< VREG[4:0], TOPOFF_TIMER[1:0], VRECHG */
#define SGM41513_REG05  0x05u /**< EN_TERM, ITERM_TIMER, WATCHDOG[1:0],
                                   EN_TIMER, CHG_TIMER, TREG, JEITA_ISET_L */
#define SGM41513_REG06  0x06u /**< OVP[1:0], BOOSTV[1:0], VINDPM[3:0] */
#define SGM41513_REG07  0x07u /**< IINDET_EN, TMR2X_EN, BATFET_DIS,
                                   JEITA_VSET_H, BATFET_DLY, BATFET_RST_EN,
                                   VDPM_BAT_TRACK[1:0] */
#define SGM41513_REG08  0x08u /**< （RO）VBUS_STAT, CHRG_STAT, PG_STAT,
                                   THERM_STAT, VSYS_STAT */
#define SGM41513_REG09  0x09u /**< （RO）故障标志，锁存直至读取，
                                   NTC_FAULT 为实时值。禁止突发(burst)访问 */
#define SGM41513_REG0A  0x0Au /**< VBUS_GD, VINDPM_STAT, IINDPM_STAT,
                                   TOPOFF_ACTIVE, ACOV_STAT, INT 屏蔽位 */
#define SGM41513_REG0B  0x0Bu /**< REG_RST (W), PN[3:0], SGMPART, DEV_REV */
#define SGM41513_REG0C  0x0Cu /**< JEITA 配置 */
#define SGM41513_REG0D  0x0Du /**< PUMPX, DP/DM 电压, OTGF_ITREMR */
#define SGM41513_REG0E  0x0Eu /**< （RO）INPUT_DET_DONE。禁止突发(burst)访问 */
#define SGM41513_REG0F  0x0Fu /**< VREG_FT, ISHORT_SET, STAT_SET, VINDPM_OS */

/* 可读可写寄存器的上电复位值（影子缓存(shadow)使用）                     */
#define SGM41513_REG00_POR   0x17u  /**< REG00 POR 默认值 */
#define SGM41513_REG01_POR   0x1Au  /**< REG01 POR 默认值 */
#define SGM41513_REG02_POR   0xB4u  /**< REG02 POR 默认值 */
#define SGM41513_REG03_POR   0xAAu  /**< REG03 POR 默认值 */
#define SGM41513_REG04_POR   0x58u  /**< REG04 POR 默认值 */
#define SGM41513_REG05_POR   0xBFu  /**< REG05 POR 默认值 */
#define SGM41513_REG06_POR   0xE6u  /**< REG06 POR 默认值 */
#define SGM41513_REG07_POR   0x4Cu  /**< REG07 POR 默认值 */
#define SGM41513_REG0C_POR   0x75u  /**< REG0C POR 默认值 */
#define SGM41513_REG0D_POR   0x01u  /**< REG0D POR 默认值 */
#define SGM41513_REG0F_POR   0x00u  /**< REG0F POR 默认值 */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG00 - 输入限流 / HIZ / STAT 引脚功能
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_EN_HIZ              (0x01u << 7) /**< 1：VBUS 与内部电路断开 */
#define SGM41513_EN_ICHG_MON_SHIFT   5u           /**< EN_ICHG_MON[6:5] 位移 */
#define SGM41513_EN_ICHG_MON_MASK    (0x03u << 5) /**< EN_ICHG_MON[6:5] 掩码 */
#define SGM41513_EN_ICHG_MON_CHARGE  (0x00u << 5) /**< STAT 跟随充电状态 */
#define SGM41513_EN_ICHG_MON_STATSET (0x01u << 5) /**< STAT 跟随 STAT_SET */
#define SGM41513_EN_ICHG_MON_DISABLE (0x02u << 5) /**< STAT 关闭 */

#define SGM41513_IINDPM_SHIFT        0u           /**< IINDPM[4:0] 位移 */
#define SGM41513_IINDPM_MASK         0x1Fu        /**< IINDPM[4:0] 掩码 */
/* IINDPM(mA) = 100 + 100 * 码点，范围 100 .. 3200 mA，100 mA 步进       */
#define SGM41513_IINDPM_MIN_MA       100u         /**< IINDPM 下限（mA） */
#define SGM41513_IINDPM_MAX_MA       3200u        /**< IINDPM 上限（mA） */
#define SGM41513_IINDPM_STEP_MA      100u         /**< IINDPM 步进（mA） */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG01 - 变换器配置
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_PFM_DIS             (0x01u << 7) /**< 1：关闭 PFM */
#define SGM41513_WD_RST              (0x01u << 6) /**< W1C：喂看门狗 */
#define SGM41513_OTG_CONFIG          (0x01u << 5) /**< 1：OTG 升压(boost)开启
                                                         （优先于充电） */
#define SGM41513_CHG_CONFIG          (0x01u << 4) /**< 1：充电使能
                                                         （nCE 引脚须为低） */
#define SGM41513_SYS_MIN_SHIFT       1u           /**< SYS_MIN[3:1] 位移 */
#define SGM41513_SYS_MIN_MASK        (0x07u << 1) /**< SYS_MIN[3:1] 掩码 */
/* SYS_MIN 查表(mV)：2600,2800,3000,3200,3400,3500,3600,3700             */
#define SGM41513_SYS_MIN_MIN_MV      2600u        /**< SYS_MIN 下限（mV） */
#define SGM41513_SYS_MIN_MAX_MV      3700u        /**< SYS_MIN 上限（mV） */
#define SGM41513_MIN_BAT_SEL         (0x01u << 0) /**< OTG 低电池门限：
                                                         0：2.95/3.15 V
                                                         1：2.6/2.8 V */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG02 - 充电电流
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_BOOST_LIM           (0x01u << 7) /**< 0：0.5A，1：1.2A */
#define SGM41513_Q1_FULLON           (0x01u << 6) /**< 1：Q1 始终全开 */
#define SGM41513_ICHG_SHIFT          0u           /**< ICHG[5:0] 位移 */
#define SGM41513_ICHG_MASK           0x3Fu        /**< ICHG[5:0] 掩码 */
/* ICHG 采用非线性 64 项查表（见 sgm41513.c），0..3 A                    */
#define SGM41513_ICHG_MIN_MA         0u           /**< ICHG 下限（mA） */
#define SGM41513_ICHG_MAX_MA         3000u        /**< ICHG 上限（mA） */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG03 - 预充电 / 终止电流
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_IPRECHG_SHIFT       4u           /**< IPRECHG[7:4] 位移 */
#define SGM41513_IPRECHG_MASK        (0x0Fu << 4) /**< IPRECHG[7:4] 掩码 */
#define SGM41513_ITERM_SHIFT         0u           /**< ITERM[3:0] 位移 */
#define SGM41513_ITERM_MASK          0x0Fu        /**< ITERM[3:0] 掩码 */
/* IPRECHG/ITERM 共用一张 16 项表(mA)：
 * 5,10,15,20,30,40,50,60,80,100,120,140,160,180,200,240                */
#define SGM41513_IPRECHG_TERM_MIN_MA 5u           /**< 共用表下限（mA） */
#define SGM41513_IPRECHG_TERM_MAX_MA 240u         /**< 共用表上限（mA） */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG04 - 充电电压 / 涓流补充(top-off) / 再充电
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_VREG_SHIFT          3u           /**< VREG[7:3] 位移 */
#define SGM41513_VREG_MASK           (0x1Fu << 3) /**< VREG[7:3] 掩码 */
/* VREG(mV) = 3856 + 32*码点（码点 <= 24），但码点 15 = 4350 mV 特例。
 * 码点高于 24 钳位到 4624 mV（码点 24）。范围 3856 .. 4624 mV。          */
#define SGM41513_VREG_MIN_MV         3856u        /**< VREG 下限（mV） */
#define SGM41513_VREG_MAX_MV         4624u        /**< VREG 上限（mV） */
#define SGM41513_VREG_STEP_MV        32u          /**< VREG 步进（mV） */
#define SGM41513_VREG_CODE_MAX       24u          /**< VREG 最大码点 */

#define SGM41513_TOPOFF_TIMER_SHIFT  1u           /**< TOPOFF_TIMER[2:1] 位移 */
#define SGM41513_TOPOFF_TIMER_MASK   (0x03u << 1) /**< TOPOFF_TIMER[2:1] 掩码 */
/* 00：关闭，01：15min，10：30min，11：45min                              */
#define SGM41513_VRECHG              (0x01u << 0) /**< 0：100mV，1：200mV */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG05 - 终止 / 看门狗 / 计时器 / 热调节 / JEITA 冷区
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_EN_TERM             (0x01u << 7) /**< 1：开启终止判定 */
#define SGM41513_ITERM_TIMER         (0x01u << 6) /**< 0：230ms，1：16ms */
#define SGM41513_WATCHDOG_SHIFT      4u           /**< WATCHDOG[5:4] 位移 */
#define SGM41513_WATCHDOG_MASK       (0x03u << 4) /**< WATCHDOG[5:4] 掩码 */
/* 00：关闭，01：40s，10：80s，11：160s（POR 默认）                       */
#define SGM41513_EN_TIMER            (0x01u << 3) /**< 安全计时器使能 */
#define SGM41513_CHG_TIMER           (0x01u << 2) /**< 0：7h，1：16h */
#define SGM41513_TREG                (0x01u << 1) /**< 0：80C，1：120C */
#define SGM41513_JEITA_ISET_L        (0x01u << 0) /**< 冷区电流：
                                                         0：50% ICHG，1：20% */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG06 - 输入 OVP / 升压电压 / VINDPM 阈值
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_OVP_SHIFT           6u           /**< OVP[7:6] 位移 */
#define SGM41513_OVP_MASK            (0x03u << 6) /**< OVP[7:6] 掩码 */
/* 00：5.5V，01：6.5V（5V 输入），10：10.5V（9V），11：14V（12V，POR）    */

#define SGM41513_BOOSTV_SHIFT        4u           /**< BOOSTV[5:4] 位移 */
#define SGM41513_BOOSTV_MASK         (0x03u << 4) /**< BOOSTV[5:4] 掩码 */
/* 00：4.85V，01：5.00V，10：5.15V（POR），11：5.30V                      */

#define SGM41513_VINDPM_SHIFT        0u           /**< VINDPM[3:0] 位移 */
#define SGM41513_VINDPM_MASK         0x0Fu        /**< VINDPM[3:0] 掩码 */
/* VINDPM(mV) = VINDPM_OS 偏置 + 100 * 码点，16 个 100 mV 步进。
 * 偏置（REG0F VINDPM_OS）：3900/5900/7500/10500 mV                      */
#define SGM41513_VINDPM_STEP_MV      100u         /**< VINDPM 步进（mV） */
#define SGM41513_VINDPM_CODES        16u          /**< VINDPM 码点数 */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG07 - 检测 / 计时器 / BATFET / JEITA 暖区 / VDPM 跟踪
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_IINDET_EN           (0x01u << 7) /**< W1SC：强制输入电流检测 */
#define SGM41513_TMR2X_EN            (0x01u << 6) /**< 1：DPM/JEITA 冷区/
                                                         热调节期间安全计时器
                                                         以半速运行 */
#define SGM41513_BATFET_DIS          (0x01u << 5) /**< 1：BATFET 断开
                                                         （船运模式） */
#define SGM41513_JEITA_VSET_H        (0x01u << 4) /**< 暖区电压：
                                                         0：min(VREG,4.1V)
                                                         1：VREG */
#define SGM41513_BATFET_DLY          (0x01u << 3) /**< 1：BATFET 延迟
                                                         tSM_DLY（12s）后断开 */
#define SGM41513_BATFET_RST_EN       (0x01u << 2) /**< 1：使能 nQON 10s
                                                         BATFET 复位 */
#define SGM41513_VDPM_BAT_TRACK_SHIFT 0u          /**< VDPM_BAT_TRACK[1:0] 位移 */
#define SGM41513_VDPM_BAT_TRACK_MASK  0x03u       /**< VDPM_BAT_TRACK[1:0] 掩码 */
/* 00：关闭，01：VBAT+200mV，10：VBAT+250mV，11：VBAT+300mV。
 * 生效 VINDPM = max(VINDPM, 跟踪值)。仅当 VINDPM_OS = 00 时有效。       */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG08（RO）- 状态
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_VBUS_STAT_SHIFT     5u           /**< VBUS_STAT[7:5] 位移 */
#define SGM41513_VBUS_STAT_MASK      (0x07u << 5) /**< VBUS_STAT[7:5] 掩码 */
/* 编码因变体而异：
 * SGM41513        ：000 无输入，001 USB SDP 500mA（PSEL 高），
 *                   010 适配器 2.4A（PSEL 低），111 OTG
 * SGM41513A/D     ：000 无输入，001 SDP，010 CDP 1.5A，011 DCP 2.4A，
 *                   101 未知适配器 500mA，
 *                   110 非标准适配器，111 OTG                           */
#define SGM41513_CHRG_STAT_SHIFT     3u           /**< CHRG_STAT[4:3] 位移 */
#define SGM41513_CHRG_STAT_MASK      (0x03u << 3) /**< CHRG_STAT[4:3] 掩码 */
/* 00：未充电，01：涓流/预充电，10：快充（CC/CV），
 * 11：充电已终止                                                        */
#define SGM41513_PG_STAT             (0x01u << 2) /**< 电源正常(power good) */
#define SGM41513_THERM_STAT          (0x01u << 1) /**< 热调节中 */
#define SGM41513_VSYS_STAT           (0x01u << 0) /**< VSYS_MIN 调节中 */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG09（RO）- 故障（锁存直至读取；读两次取实时值；NTC_FAULT 为实时值；
 *                 禁止突发(burst)访问）
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_WATCHDOG_FAULT      (0x01u << 7) /**< I2C 看门狗超时（锁存） */
#define SGM41513_BOOST_FAULT         (0x01u << 6) /**< 升压(boost)故障（锁存） */
#define SGM41513_CHRG_FAULT_SHIFT    4u           /**< CHRG_FAULT[5:4] 位移 */
#define SGM41513_CHRG_FAULT_MASK     (0x03u << 4) /**< CHRG_FAULT[5:4] 掩码 */
/* 00：正常，01：输入故障（VAC 过压或 VBAT<VVBUS<3.8V），
 * 10：热关断，11：安全计时器超时                                        */
#define SGM41513_BAT_FAULT           (0x01u << 3) /**< 电池过压(OVP) */
#define SGM41513_NTC_FAULT_SHIFT     0u           /**< NTC_FAULT[2:0] 位移 */
#define SGM41513_NTC_FAULT_MASK      0x07u        /**< NTC_FAULT[2:0] 掩码 */
/* 000：正常，010：暖区（仅降压(buck)），011：冷区（仅降压(buck)），
 * 101：过冷，110：过热                                                  */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG0A - DPM 状态（RO）+ 中断屏蔽（R/W）
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_VBUS_GD             (0x01u << 7) /**< 检测到良好 VBUS */
#define SGM41513_VINDPM_STAT         (0x01u << 6) /**< 处于 VINDPM 调节 */
#define SGM41513_IINDPM_STAT         (0x01u << 5) /**< 处于 IINDPM 调节 */
#define SGM41513_TOPOFF_ACTIVE       (0x01u << 3) /**< 涓流补充(top-off)
                                                         计时中 */
#define SGM41513_ACOV_STAT           (0x01u << 2) /**< 检测到输入过压 */
#define SGM41513_VINDPM_INT_MASK     (0x01u << 1) /**< 1：屏蔽 VINDPM nINT */
#define SGM41513_IINDPM_INT_MASK     (0x01u << 0) /**< 1：屏蔽 IINDPM nINT */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG0B - 复位（W）+ 器件识别（RO）
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_REG_RST             (0x01u << 7) /**< W1SC：全部可读可写
                                                         寄存器复位到 POR */
#define SGM41513_PN_SHIFT            3u           /**< PN[6:3] 位移 */
#define SGM41513_PN_MASK             (0x0Fu << 3) /**< PN[6:3] 掩码 */
#define SGM41513_PN_SGM41513         (0x00u << 3) /**< PN = 0000 */
#define SGM41513_PN_SGM41513A_D      (0x01u << 3) /**< PN = 0001（A 或 D） */
#define SGM41513_SGMPART_SHIFT       2u           /**< SGMPART[2] 位移 */
#define SGM41513_SGMPART_MASK        (0x01u << 2) /**< SGMPART 掩码；
                                                         本器件读出 0 */
#define SGM41513_DEV_REV_SHIFT       0u           /**< DEV_REV[1:0] 位移 */
#define SGM41513_DEV_REV_MASK        0x03u        /**< DEV_REV[1:0] 掩码 */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG0C - JEITA 配置
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_JEITA_VSET_L        (0x01u << 7) /**< 冷区电压：
                                                         0：VREG
                                                         1：min(VREG,4.1V) */
#define SGM41513_JEITA_ISET_L_EN     (0x01u << 6) /**< 1：冷区允许充电 */
#define SGM41513_JEITA_ISET_H_SHIFT  4u           /**< JEITA_ISET_H[5:4] 位移 */
#define SGM41513_JEITA_ISET_H_MASK   (0x03u << 4) /**< JEITA_ISET_H[5:4] 掩码 */
/* 暖区电流：00：0%，01：20%，10：50%，11：100% ICHG（POR）              */
#define SGM41513_JEITA_VT2_SHIFT     2u           /**< JEITA_VT2[3:2] 位移 */
#define SGM41513_JEITA_VT2_MASK      (0x03u << 2) /**< JEITA_VT2[3:2] 掩码 */
/* 冷区阈值 T2：00：约 5.5C，01：约 10C（POR），10：约 15C，11：约 20C    */
#define SGM41513_JEITA_VT3_SHIFT     0u           /**< JEITA_VT3[1:0] 位移 */
#define SGM41513_JEITA_VT3_MASK      0x03u        /**< JEITA_VT3[1:0] 掩码 */
/* 暖区阈值 T3：00：约 40C，01：约 44.5C（POR），10：约 50.5C，11：约 54.5C */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG0D - PUMPX / DP-DM 电压 / OTG 频率
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_EN_PUMPX            (0x01u << 7) /**< 1：使能 PUMPX 协议 */
#define SGM41513_PUMPX_UP            (0x01u << 6) /**< 完成后 W1SC 自清零 */
#define SGM41513_PUMPX_DN            (0x01u << 5) /**< 完成后 W1SC 自清零 */
#define SGM41513_DP_VSET_SHIFT       3u           /**< DP_VSET[4:3] 位移 */
#define SGM41513_DP_VSET_MASK        (0x03u << 3) /**< DP_VSET[4:3] 掩码 */
#define SGM41513_DM_VSET_SHIFT       1u           /**< DM_VSET[2:1] 位移 */
#define SGM41513_DM_VSET_MASK        (0x03u << 1) /**< DM_VSET[2:1] 掩码 */
/* DP_VSET/DM_VSET：00：HIZ，01：0V，10：0.6V，11：3.3V                   */
#define SGM41513_OTGF_ITREMR         (0x01u << 0) /**< 双用途位（见下） */
/* 双用途位：
 *   OTG 模式    ：0：升压 fSW = 500kHz，1：1.5MHz（POR）
 *   充电模式    ：0：ICHG>300mA 时 ITERM x6 生效，1：ITERM 原值          */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG0E（RO）- 输入检测标志（禁止突发(burst)访问）
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_INPUT_DET_DONE      (0x01u << 7) /**< PSEL 或 D+/D- 检测
                                                         完成时置位 */

/* ═════════════════════════════════════════════════════════════════════════════
 * REG0F - VREG 微调 / 涓流 / STAT 图案 / VINDPM 偏置
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_VREG_FT_SHIFT       6u           /**< VREG_FT[7:6] 位移 */
#define SGM41513_VREG_FT_MASK        (0x03u << 6) /**< VREG_FT[7:6] 掩码 */
/* 00：关闭（POR），01：+8mV，10：-8mV，11：-16mV                         */
#define SGM41513_ISHORT_SET          (0x01u << 4) /**< 涓流：0：90mA（POR），
                                                         1：30mA */
#define SGM41513_STAT_SET_SHIFT      2u           /**< STAT_SET[3:2] 位移 */
#define SGM41513_STAT_SET_MASK       (0x03u << 2) /**< STAT_SET[3:2] 掩码 */
/* EN_ICHG_MON = 01 时的 STAT 图案：
 * 00：LED 熄灭，01：LED 常亮，10：亮 1s/灭 1s，11：亮 1s/灭 3s          */
#define SGM41513_VINDPM_OS_SHIFT     0u           /**< VINDPM_OS[1:0] 位移 */
#define SGM41513_VINDPM_OS_MASK      0x03u        /**< VINDPM_OS[1:0] 掩码 */
/* VINDPM 偏置：00：3900mV（POR），01：5900mV，10：7500mV，11：10500mV   */

/* 每档偏置的 VINDPM 窗口：[偏置, 偏置 + 15*100mV]                       */
#define SGM41513_VINDPM_OS_MV_0      3900u        /**< OS 档 0 偏置（mV） */
#define SGM41513_VINDPM_OS_MV_1      5900u        /**< OS 档 1 偏置（mV） */
#define SGM41513_VINDPM_OS_MV_2      7500u        /**< OS 档 2 偏置（mV） */
#define SGM41513_VINDPM_OS_MV_3      10500u       /**< OS 档 3 偏置（mV） */

/* OTG 启动时序：须先清除 EN_HIZ 并让 REGN 启动，再置位 OTG_CONFIG
 * （数据手册要求 >= 30 ms）。                                            */
#define SGM41513_OTG_START_DELAY_MS  30u          /**< OTG 启动最短间隔（ms） */

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_REGS_H */
