/**
 * @file    bq25622e_regs.h
 * @brief   BQ25622E 寄存器地址、位域与电气换算常量定义
 * @details 寄存器布局依据 TI 数据手册 SLUSFA3C（2025-02 修订）。芯片寄存器
 *          地址空间为 0x02~0x38，稀疏分布共 37 个地址；其中充电参数类寄存器
 *          为 16 位小端格式（低字节在前，如 REG02/03 构成一个逻辑寄存器），
 *          其余为 8 位。本文件中：
 *          - 寄存器地址宏一律取逻辑寄存器的低字节地址，16 位寄存器由驱动
 *            核心在单次 I2C 事务内读写两个字节；
 *          - 位域 SHIFT/MASK 统一按"寄存器值"坐标系定义：16 位寄存器按
 *            uint16_t 全值计位，8 位寄存器按低字节计位（在 uint16_t 中
 *            位置相同），驱动核心不做第二套坐标系。
 * @note    本头文件同时供驱动核心与高级用户直接寄存器访问使用；普通用户
 *          仅需包含 bq25622e.h（其内部已包含本文件）。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-21
 */

#ifndef BQ25622E_REGS_H
#define BQ25622E_REGS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════
 *  总线与器件标识
 * ═══════════════════════════════════════════════════ */

#define BQ25622E_I2C_ADDR           0x6Bu    /**< 7 位 I2C 从机地址（勿左移） */
#define BQ25622E_PART_PN            0x03u    /**< REG38 PN[5:3] = 3h 即 BQ25622E */
#define BQ25622E_NUM_RW_REGS        21u      /**< R/W 逻辑寄存器个数（影子缓存深度） */

/* ═══════════════════════════════════════════════════
 *  寄存器地址（16 位寄存器取低字节地址）
 * ═══════════════════════════════════════════════════ */

#define BQ25622E_REG02              0x02u    /* 充电电流限制 ICHG（16 位）        */
#define BQ25622E_REG04              0x04u    /* 充电电压限制 VREG（16 位）        */
#define BQ25622E_REG06              0x06u    /* 输入电流限制 IINDPM（16 位）      */
#define BQ25622E_REG08              0x08u    /* 输入电压限制 VINDPM（16 位）      */
#define BQ25622E_REG0E              0x0Eu    /* 最小系统电压 VSYSMIN（16 位）     */
#define BQ25622E_REG10              0x10u    /* 预充电流 IPRECHG（16 位）         */
#define BQ25622E_REG12              0x12u    /* 终止电流 ITERM（16 位）           */
#define BQ25622E_REG14              0x14u    /* 充电控制 0                        */
#define BQ25622E_REG15              0x15u    /* 充电定时器控制                    */
#define BQ25622E_REG16              0x16u    /* 充电控制 1（看门狗/EN_CHG/HIZ）   */
#define BQ25622E_REG17              0x17u    /* 充电控制 2（软复位/频率/OVP）     */
#define BQ25622E_REG18              0x18u    /* 充电控制 3（BATFET 控制）         */
#define BQ25622E_REG19              0x19u    /* 充电控制 4（峰值保护/EXTILIM）    */
#define BQ25622E_REG1A              0x1Au    /* NTC 控制 0                        */
#define BQ25622E_REG1B              0x1Bu    /* NTC 控制 1（温度阈值组合）        */
#define BQ25622E_REG1C              0x1Cu    /* NTC 控制 2（预冷/预热区）         */
#define BQ25622E_REG1D              0x1Du    /* 充电状态 0（只读）                */
#define BQ25622E_REG1E              0x1Eu    /* 充电状态 1（只读）                */
#define BQ25622E_REG1F              0x1Fu    /* 故障状态 0（只读）                */
#define BQ25622E_REG20              0x20u    /* 充电标志 0（读清零）              */
#define BQ25622E_REG21              0x21u    /* 充电标志 1（读清零）              */
#define BQ25622E_REG22              0x22u    /* 故障标志 0（读清零）              */
#define BQ25622E_REG23              0x23u    /* 充电屏蔽 0（INT 掩码）            */
#define BQ25622E_REG24              0x24u    /* 充电屏蔽 1（INT 掩码）            */
#define BQ25622E_REG25              0x25u    /* 故障屏蔽 0（INT 掩码）            */
#define BQ25622E_REG26              0x26u    /* ADC 控制                          */
#define BQ25622E_REG27              0x27u    /* ADC 通道禁用 0                    */
#define BQ25622E_REG28              0x28u    /* IBUS 电流 ADC（16 位只读）        */
#define BQ25622E_REG2A              0x2Au    /* IBAT 电流 ADC（16 位只读）        */
#define BQ25622E_REG2C              0x2Cu    /* VBUS 电压 ADC（16 位只读）        */
#define BQ25622E_REG2E              0x2Eu    /* VPMID 电压 ADC（16 位只读）       */
#define BQ25622E_REG30              0x30u    /* VBAT 电压 ADC（16 位只读）        */
#define BQ25622E_REG32              0x32u    /* VSYS 电压 ADC（16 位只读）        */
#define BQ25622E_REG34              0x34u    /* TS 电压百分比 ADC（16 位只读）    */
#define BQ25622E_REG36              0x36u    /* 结温 TDIE ADC（16 位只读）        */
#define BQ25622E_REG38              0x38u    /* 器件信息 PN/DEV_REV（只读）       */

/* ═══════════════════════════════════════════════════
 *  上电复位默认值（POR）
 * ═══════════════════════════════════════════════════ */

#define BQ25622E_REG02_POR          0x0340u  /* ICHG = 1040 mA                    */
#define BQ25622E_REG04_POR          0x0D20u  /* VREG = 4200 mV                    */
#define BQ25622E_REG06_POR          0x0A00u  /* IINDPM = 3200 mA                  */
#define BQ25622E_REG08_POR          0x0E60u  /* VINDPM = 4600 mV                  */
#define BQ25622E_REG0E_POR          0x0B00u  /* VSYSMIN = 3520 mV                 */
#define BQ25622E_REG10_POR          0x0050u  /* IPRECHG = 100 mA                  */
#define BQ25622E_REG12_POR          0x0030u  /* ITERM = 60 mA                     */
#define BQ25622E_REG14_POR          0x06u    /* EN_TERM=1, VINDPM_BAT_TRACK=1     */
#define BQ25622E_REG15_POR          0x0Cu    /* 安全定时器使能, TMR2X 使能        */
#define BQ25622E_REG16_POR          0xA1u    /* EN_CHG=1, WATCHDOG=50s            */
#define BQ25622E_REG17_POR          0x4Fu    /* TREG=120C, 1.5MHz, VBUS_OVP=18.5V */
#define BQ25622E_REG18_POR          0x04u    /* BATFET_DLY=12.5s                  */
#define BQ25622E_REG19_POR          0xC0u    /* IBAT_PK=12A, EN_EXTILIM=0         */
#define BQ25622E_REG1A_POR          0x0Du    /* WARM 区不变流, COOL 区 20%        */
#define BQ25622E_REG1B_POR          0x25u    /* 冷 0/10/15C, 热 35/45/60C         */
#define BQ25622E_REG1C_POR          0x3Fu    /* PRECOOL/PREWARM 区不变流不变压    */
#define BQ25622E_REG1D_POR          0x00u
#define BQ25622E_REG1E_POR          0x00u
#define BQ25622E_REG1F_POR          0x00u
#define BQ25622E_REG20_POR          0x00u
#define BQ25622E_REG21_POR          0x00u
#define BQ25622E_REG22_POR          0x00u
#define BQ25622E_REG23_POR          0x00u    /* 全部 INT 不屏蔽                   */
#define BQ25622E_REG24_POR          0x00u
#define BQ25622E_REG25_POR          0x00u
#define BQ25622E_REG26_POR          0x30u    /* 连续转换, 9bit/3.75ms 采样        */
#define BQ25622E_REG27_POR          0x00u    /* 全通道使能                        */
#define BQ25622E_REG38_POR          0x02u    /* PN=3h, DEV_REV=2h                 */

/* ═══════════════════════════════════════════════════
 *  16 位寄存器位域（按 uint16_t 全值计位）
 * ═══════════════════════════════════════════════════ */

/* REG02/03 - 充电电流限制 ICHG[11:6] */
#define BQ25622E_ICHG_SHIFT         6u
#define BQ25622E_ICHG_MASK          (0x3Fu << BQ25622E_ICHG_SHIFT)
/* ICHG(mA) = 80 + 80 * (code - 1)，code 范围 1h~26h，越界自动钳位 */
#define BQ25622E_ICHG_MIN_MA        80u
#define BQ25622E_ICHG_MAX_MA        3040u
#define BQ25622E_ICHG_STEP_MA       80u
#define BQ25622E_ICHG_CODE_MIN      0x01u
#define BQ25622E_ICHG_CODE_MAX      0x26u

/* REG04/05 - 充电电压限制 VREG[11:3] */
#define BQ25622E_VREG_SHIFT         3u
#define BQ25622E_VREG_MASK          (0x1FFu << BQ25622E_VREG_SHIFT)
/* VREG(mV) = 3500 + 10 * (code - 15Eh)，code 范围 15Eh~1E0h，越界自动钳位 */
#define BQ25622E_VREG_MIN_MV        3500u
#define BQ25622E_VREG_MAX_MV        4800u
#define BQ25622E_VREG_STEP_MV       10u
#define BQ25622E_VREG_CODE_MIN      0x015Eu
#define BQ25622E_VREG_CODE_MAX      0x01E0u

/* REG06/07 - 输入电流限制 IINDPM[11:4] */
#define BQ25622E_IINDPM_SHIFT       4u
#define BQ25622E_IINDPM_MASK        (0xFFu << BQ25622E_IINDPM_SHIFT)
/* IINDPM(mA) = 100 + 20 * (code - 5h)，code 范围 5h~A0h；适配器拔出自动回 POR */
#define BQ25622E_IINDPM_MIN_MA      100u
#define BQ25622E_IINDPM_MAX_MA      3200u
#define BQ25622E_IINDPM_STEP_MA     20u
#define BQ25622E_IINDPM_CODE_MIN    0x05u
#define BQ25622E_IINDPM_CODE_MAX    0xA0u

/* REG08/09 - 输入电压限制 VINDPM[13:5] */
#define BQ25622E_VINDPM_SHIFT       5u
#define BQ25622E_VINDPM_MASK        (0x1FFu << BQ25622E_VINDPM_SHIFT)
/* VINDPM(mV) = 3800 + 40 * (code - 5Fh)，code 范围 5Fh~1A4h，越界自动钳位 */
#define BQ25622E_VINDPM_MIN_MV      3800u
#define BQ25622E_VINDPM_MAX_MV      16800u
#define BQ25622E_VINDPM_STEP_MV     40u
#define BQ25622E_VINDPM_CODE_MIN    0x5Fu
#define BQ25622E_VINDPM_CODE_MAX    0x1A4u

/* REG0E/0F - 最小系统电压 VSYSMIN[11:6] */
#define BQ25622E_VSYSMIN_SHIFT      6u
#define BQ25622E_VSYSMIN_MASK       (0x3Fu << BQ25622E_VSYSMIN_SHIFT)
/* VSYSMIN(mV) = 2560 + 80 * (code - 20h)，code 范围 20h~30h，越界自动钳位 */
#define BQ25622E_VSYSMIN_MIN_MV     2560u
#define BQ25622E_VSYSMIN_MAX_MV     3840u
#define BQ25622E_VSYSMIN_STEP_MV    80u
#define BQ25622E_VSYSMIN_CODE_MIN   0x20u
#define BQ25622E_VSYSMIN_CODE_MAX   0x30u

/* REG10/11 - 预充电流 IPRECHG[8:4] */
#define BQ25622E_IPRECHG_SHIFT      4u
#define BQ25622E_IPRECHG_MASK       (0x1Fu << BQ25622E_IPRECHG_SHIFT)
/* IPRECHG(mA) = 20 + 20 * (code - 1h)，code 范围 1h~1Fh，仅向下钳位 */
#define BQ25622E_IPRECHG_MIN_MA     20u
#define BQ25622E_IPRECHG_MAX_MA     620u
#define BQ25622E_IPRECHG_STEP_MA    20u
#define BQ25622E_IPRECHG_CODE_MIN   0x01u
#define BQ25622E_IPRECHG_CODE_MAX   0x1Fu

/* REG12/13 - 终止电流 ITERM[8:3] */
#define BQ25622E_ITERM_SHIFT        3u
#define BQ25622E_ITERM_MASK         (0x3Fu << BQ25622E_ITERM_SHIFT)
/* ITERM(mA) = 10 + 10 * (code - 1h)，code 范围 1h~3Eh，仅向下钳位 */
#define BQ25622E_ITERM_MIN_MA       10u
#define BQ25622E_ITERM_MAX_MA       620u
#define BQ25622E_ITERM_STEP_MA      10u
#define BQ25622E_ITERM_CODE_MIN     0x01u
#define BQ25622E_ITERM_CODE_MAX     0x3Eu

/* ═══════════════════════════════════════════════════
 *  8 位寄存器位域（按低字节计位）
 * ═══════════════════════════════════════════════════ */

/* REG14 - 充电控制 0 */
#define BQ25622E_Q1_FULLON          (0x01u << 7)  /**< 1: 强制 Q1 低阻 26mΩ     */
#define BQ25622E_Q4_FULLON          (0x01u << 6)  /**< 1: 强制 BATFET 低阻 15mΩ */
#define BQ25622E_ITRICKLE           (0x01u << 5)  /**< 0: 涓流 20mA, 1: 80mA    */
#define BQ25622E_TOPOFF_TMR_SHIFT   3u
#define BQ25622E_TOPOFF_TMR_MASK    (0x03u << BQ25622E_TOPOFF_TMR_SHIFT)
#define BQ25622E_EN_TERM            (0x01u << 2)  /**< 1: 充电终止使能          */
#define BQ25622E_VINDPM_BAT_TRACK   (0x01u << 1)  /**< 1: VINDPM 跟踪 VBAT+400mV */
#define BQ25622E_VRECHG             (0x01u << 0)  /**< 0: 再充阈值 100mV, 1: 200mV */

/* REG15 - 充电定时器控制 */
#define BQ25622E_DIS_STAT           (0x01u << 7)  /**< 1: 禁用 STAT 引脚输出    */
#define BQ25622E_TMR2X_EN           (0x01u << 3)  /**< 1: DPM 期间定时器半速    */
#define BQ25622E_EN_SAFETY_TMRS     (0x01u << 2)  /**< 1: 安全定时器使能        */
#define BQ25622E_PRECHG_TMR         (0x01u << 1)  /**< 0: 预充 2.5h, 1: 0.62h   */
#define BQ25622E_CHG_TMR            (0x01u << 0)  /**< 0: 快充 14.5h, 1: 28h    */

/* REG16 - 充电控制 1 */
#define BQ25622E_EN_AUTO_IBATDIS    (0x01u << 7)  /**< 1: BAT OVP 自动放电      */
#define BQ25622E_FORCE_IBATDIS      (0x01u << 6)  /**< 1: 强制电池放电 30mA     */
#define BQ25622E_EN_CHG             (0x01u << 5)  /**< 1: 充电使能              */
#define BQ25622E_EN_HIZ             (0x01u << 4)  /**< 1: HIZ 模式（断开输入）   */
#define BQ25622E_FORCE_PMID_DIS     (0x01u << 3)  /**< 1: 强制 PMID 放电 30mA   */
#define BQ25622E_WD_RST             (0x01u << 2)  /**< 写 1 喂狗，自动回 0      */
#define BQ25622E_WATCHDOG_SHIFT     0u
#define BQ25622E_WATCHDOG_MASK      (0x03u << BQ25622E_WATCHDOG_SHIFT)

/* REG17 - 充电控制 2 */
#define BQ25622E_REG_RST            (0x01u << 7)  /**< 写 1 软复位，自动回 0    */
#define BQ25622E_TREG               (0x01u << 6)  /**< 0: 热调节 60C, 1: 120C   */
#define BQ25622E_SET_CONV_FREQ_SHIFT 4u
#define BQ25622E_SET_CONV_FREQ_MASK (0x03u << BQ25622E_SET_CONV_FREQ_SHIFT)
#define BQ25622E_SET_CONV_STRN_SHIFT 2u
#define BQ25622E_SET_CONV_STRN_MASK (0x03u << BQ25622E_SET_CONV_STRN_SHIFT)
#define BQ25622E_VBUS_OVP           (0x01u << 0)  /**< 0: VBUS OVP 6.3V, 1: 18.5V */

/* REG18 - 充电控制 3 */
#define BQ25622E_PFM_FWD_DIS        (0x01u << 4)  /**< 1: 禁用正向 PFM          */
#define BQ25622E_BATFET_CTRL_WVBUS  (0x01u << 3)  /**< 1: 允许带适配器关 BATFET */
#define BQ25622E_BATFET_DLY         (0x01u << 2)  /**< 0: 延迟 25ms, 1: 12.5s   */
#define BQ25622E_BATFET_CTRL_SHIFT  0u
#define BQ25622E_BATFET_CTRL_MASK   (0x03u << BQ25622E_BATFET_CTRL_SHIFT)

/* REG19 - 充电控制 4 */
#define BQ25622E_IBAT_PK_SHIFT      6u
#define BQ25622E_IBAT_PK_MASK       (0x03u << BQ25622E_IBAT_PK_SHIFT)
#define BQ25622E_VBAT_UVLO          (0x01u << 5)  /**< 0: UVLO 2.2V, 1: 1.8V    */
#define BQ25622E_EN_EXTILIM         (0x01u << 2)  /**< 1: 使能 ILIM 引脚限流    */
#define BQ25622E_CHG_RATE_SHIFT     0u
#define BQ25622E_CHG_RATE_MASK      (0x03u << BQ25622E_CHG_RATE_SHIFT)

/* REG1A - NTC 控制 0 */
#define BQ25622E_TS_IGNORE          (0x01u << 7)  /**< 1: 忽略 TS 强制充电      */
#define BQ25622E_TS_ISET_WARM_SHIFT 2u
#define BQ25622E_TS_ISET_WARM_MASK  (0x03u << BQ25622E_TS_ISET_WARM_SHIFT)
#define BQ25622E_TS_ISET_COOL_SHIFT 0u
#define BQ25622E_TS_ISET_COOL_MASK  (0x03u << BQ25622E_TS_ISET_COOL_SHIFT)

/* REG1B - NTC 控制 1（阈值组合编码见 bq25622e.h 枚举说明） */
#define BQ25622E_TS_TH_COLD_SHIFT   5u
#define BQ25622E_TS_TH_COLD_MASK    (0x07u << BQ25622E_TS_TH_COLD_SHIFT)
#define BQ25622E_TS_TH_HOT_SHIFT    2u
#define BQ25622E_TS_TH_HOT_MASK     (0x07u << BQ25622E_TS_TH_HOT_SHIFT)
#define BQ25622E_TS_VSET_WARM_SHIFT 0u
#define BQ25622E_TS_VSET_WARM_MASK  (0x03u << BQ25622E_TS_VSET_WARM_SHIFT)

/* REG1C - NTC 控制 2 */
#define BQ25622E_TS_VSET_SYM        (0x01u << 6)  /**< 1: 对称复用 WARM 电压设置 */
#define BQ25622E_TS_VSET_PREWARM_SHIFT 4u
#define BQ25622E_TS_VSET_PREWARM_MASK (0x03u << BQ25622E_TS_VSET_PREWARM_SHIFT)
#define BQ25622E_TS_ISET_PREWARM_SHIFT 2u
#define BQ25622E_TS_ISET_PREWARM_MASK (0x03u << BQ25622E_TS_ISET_PREWARM_SHIFT)
#define BQ25622E_TS_ISET_PRECOOL_SHIFT 0u
#define BQ25622E_TS_ISET_PRECOOL_MASK (0x03u << BQ25622E_TS_ISET_PRECOOL_SHIFT)

/* REG1D - 充电状态 0（只读） */
#define BQ25622E_ADC_DONE_STAT      (0x01u << 6)  /**< 1: ADC 转换完成          */
#define BQ25622E_TREG_STAT          (0x01u << 5)  /**< 1: 结温热调节中          */
#define BQ25622E_VSYS_STAT          (0x01u << 4)  /**< 1: VSYSMIN 稳压中        */
#define BQ25622E_IINDPM_STAT        (0x01u << 3)  /**< 1: 输入限流生效          */
#define BQ25622E_VINDPM_STAT        (0x01u << 2)  /**< 1: 输入限压生效          */
#define BQ25622E_SAFETY_TMR_STAT    (0x01u << 1)  /**< 1: 安全定时器超时        */
#define BQ25622E_WD_STAT            (0x01u << 0)  /**< 1: 看门狗超时            */

/* REG1E - 充电状态 1（只读） */
#define BQ25622E_CHG_STAT_SHIFT     3u
#define BQ25622E_CHG_STAT_MASK      (0x03u << BQ25622E_CHG_STAT_SHIFT)
#define BQ25622E_VBUS_STAT_SHIFT    0u
#define BQ25622E_VBUS_STAT_MASK     (0x07u << BQ25622E_VBUS_STAT_SHIFT)

/* REG1F - 故障状态 0（只读） */
#define BQ25622E_VBUS_FAULT_STAT    (0x01u << 7)  /**< 1: VBUS OVP/睡眠不开关   */
#define BQ25622E_BAT_FAULT_STAT     (0x01u << 6)  /**< 1: 电池 OVP/OCP          */
#define BQ25622E_SYS_FAULT_STAT     (0x01u << 5)  /**< 1: VSYS 短路/过压        */
#define BQ25622E_TSHUT_STAT         (0x01u << 3)  /**< 1: 热关断（140C）        */
#define BQ25622E_TS_STAT_SHIFT      0u
#define BQ25622E_TS_STAT_MASK       (0x07u << BQ25622E_TS_STAT_SHIFT)

/* REG20 - 充电标志 0（读清零，位定义同 REG1D） */
#define BQ25622E_ADC_DONE_FLAG      (0x01u << 6)
#define BQ25622E_TREG_FLAG          (0x01u << 5)
#define BQ25622E_VSYS_FLAG          (0x01u << 4)
#define BQ25622E_IINDPM_FLAG        (0x01u << 3)
#define BQ25622E_VINDPM_FLAG        (0x01u << 2)
#define BQ25622E_SAFETY_TMR_FLAG    (0x01u << 1)
#define BQ25622E_WD_FLAG            (0x01u << 0)

/* REG21 - 充电标志 1（读清零） */
#define BQ25622E_CHG_FLAG           (0x01u << 3)  /**< 充电状态跳变             */
#define BQ25622E_VBUS_FLAG          (0x01u << 0)  /**< VBUS 状态跳变            */

/* REG22 - 故障标志 0（读清零） */
#define BQ25622E_VBUS_FAULT_FLAG    (0x01u << 7)
#define BQ25622E_BAT_FAULT_FLAG     (0x01u << 6)
#define BQ25622E_SYS_FAULT_FLAG     (0x01u << 5)
#define BQ25622E_TSHUT_FLAG         (0x01u << 3)
#define BQ25622E_TS_FLAG            (0x01u << 0)  /**< TS 温度区跳变            */

/* REG23 - 充电屏蔽 0（1 = 屏蔽 INT 脉冲；位名统一带 _MASK_BIT 后缀，
 *         避免与位域宏 SHIFT/MASK 配对命名混淆，IINDPM/VINDPM 另与 16 位
 *         位域掩码重名） */
#define BQ25622E_ADC_DONE_MASK_BIT  (0x01u << 6)
#define BQ25622E_TREG_MASK_BIT      (0x01u << 5)
#define BQ25622E_VSYS_MASK_BIT      (0x01u << 4)
#define BQ25622E_IINDPM_MASK_BIT    (0x01u << 3)
#define BQ25622E_VINDPM_MASK_BIT    (0x01u << 2)
#define BQ25622E_SAFETY_TMR_MASK_BIT (0x01u << 1)
#define BQ25622E_WD_MASK_BIT        (0x01u << 0)

/* REG24 - 充电屏蔽 1 */
#define BQ25622E_CHG_MASK_BIT       (0x01u << 3)
#define BQ25622E_VBUS_MASK_BIT      (0x01u << 0)

/* REG25 - 故障屏蔽 0 */
#define BQ25622E_VBUS_FAULT_MASK_BIT (0x01u << 7)
#define BQ25622E_BAT_FAULT_MASK_BIT (0x01u << 6)
#define BQ25622E_SYS_FAULT_MASK_BIT (0x01u << 5)
#define BQ25622E_TSHUT_MASK_BIT     (0x01u << 3)
#define BQ25622E_TS_MASK_BIT        (0x01u << 0)

/* REG26 - ADC 控制 */
#define BQ25622E_ADC_EN             (0x01u << 7)  /**< 1: ADC 使能              */
#define BQ25622E_ADC_RATE           (0x01u << 6)  /**< 0: 连续, 1: 单次         */
#define BQ25622E_ADC_SAMPLE_SHIFT   4u
#define BQ25622E_ADC_SAMPLE_MASK    (0x03u << BQ25622E_ADC_SAMPLE_SHIFT)
#define BQ25622E_ADC_AVG            (0x01u << 3)  /**< 1: 滑动平均              */
#define BQ25622E_ADC_AVG_INIT       (0x01u << 2)  /**< 1: 从新转换开始平均      */

/* REG27 - ADC 通道禁用（1 = 禁用对应通道） */
#define BQ25622E_IBUS_ADC_DIS       (0x01u << 7)
#define BQ25622E_IBAT_ADC_DIS       (0x01u << 6)
#define BQ25622E_VBUS_ADC_DIS       (0x01u << 5)
#define BQ25622E_VBAT_ADC_DIS       (0x01u << 4)
#define BQ25622E_VSYS_ADC_DIS       (0x01u << 3)
#define BQ25622E_TS_ADC_DIS         (0x01u << 2)
#define BQ25622E_TDIE_ADC_DIS       (0x01u << 1)
#define BQ25622E_VPMID_ADC_DIS      (0x01u << 0)

/* REG38 - 器件信息（只读） */
#define BQ25622E_PN_SHIFT           3u
#define BQ25622E_PN_MASK            (0x07u << BQ25622E_PN_SHIFT)
#define BQ25622E_DEV_REV_SHIFT      0u
#define BQ25622E_DEV_REV_MASK       (0x07u << BQ25622E_DEV_REV_SHIFT)

/* ═══════════════════════════════════════════════════
 *  ADC 结果换算（16 位只读寄存器）
 * ═══════════════════════════════════════════════════ */

/* IBUS_ADC[15:1]：15 位补码，2 mA/LSB，VBUS→PMID 方向为正 */
#define BQ25622E_IBUS_ADC_SHIFT     1u
#define BQ25622E_IBUS_ADC_MASK      (0x7FFFu << BQ25622E_IBUS_ADC_SHIFT)
#define BQ25622E_IBUS_ADC_LSB_MA    2

/* IBAT_ADC[15:2]：14 位补码，4 mA/LSB，充电方向为正 */
#define BQ25622E_IBAT_ADC_SHIFT     2u
#define BQ25622E_IBAT_ADC_MASK      (0x3FFFu << BQ25622E_IBAT_ADC_SHIFT)
#define BQ25622E_IBAT_ADC_LSB_MA    4

/* VBUS/VPMID_ADC[14:2]：13 位无符号，3.97 mV/LSB（×397/100 整数换算） */
#define BQ25622E_VBUS_ADC_SHIFT     2u
#define BQ25622E_VBUS_ADC_MASK      (0x1FFFu << BQ25622E_VBUS_ADC_SHIFT)
#define BQ25622E_VBUS_ADC_LSB_MV_X100 397

/* VBAT/VSYS_ADC[12:1]：12 位无符号，1.99 mV/LSB（×199/100 整数换算） */
#define BQ25622E_VBAT_ADC_SHIFT     1u
#define BQ25622E_VBAT_ADC_MASK      (0x0FFFu << BQ25622E_VBAT_ADC_SHIFT)
#define BQ25622E_VBAT_ADC_LSB_MV_X100 199

/* TS_ADC[11:0]：12 位无符号，0.0961 %/LSB（×961/10000 换算，满量程 98.31%） */
#define BQ25622E_TS_ADC_SHIFT       0u
#define BQ25622E_TS_ADC_MASK        (0x0FFFu << BQ25622E_TS_ADC_SHIFT)
#define BQ25622E_TS_ADC_LSB_PCT_X10000 961

/* TDIE_ADC[11:0]：12 位补码，0.5 C/LSB */
#define BQ25622E_TDIE_ADC_SHIFT     0u
#define BQ25622E_TDIE_ADC_MASK      (0x0FFFu << BQ25622E_TDIE_ADC_SHIFT)

#ifdef __cplusplus
}
#endif

#endif /* BQ25622E_REGS_H */
