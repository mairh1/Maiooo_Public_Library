/*
 * sgm41513.h - SGM41513 系列电池充电芯片通用驱动公共 API。
 *
 * 器件    : SGM41513 / SGM41513A / SGM41513D（SGMicro 圣邦微）
 *           输入 3.9-13.5 V、3 A 单节锂离子电池充电器，NVDC
 *           电源路径(power path)，I2C 接口（7 位地址 0x1A，
 *           400 kHz），OTG 反向升压(boost)，JEITA，船运模式(ship mode)。
 * 数据手册: SGM41513_SGM41513A_SGM41513D, APRIL 2025 REV. C.1
 *
 * 分层架构：
 *
 *   +--------------------------------------+
 *   |  应用层                               |   用户代码
 *   +--------------------------------------+
 *              |  本 API（sgm41513.h）
 *   +--------------------------------------+
 *   |  驱动核心  (sgm41513.c)              |   可移植、纯 C99、
 *   |  配置      (sgm41513_conf.h)         |   无动态内存
 *   +--------------------------------------+
 *              |  4 个 io 函数（sgm41513_io.h）
 *   +--------------------------------------+
 *   |  移植层（用户提供）                    |   STM32 / ESP-IDF / Linux /
 *   |                                      |   RTOS / 模拟 I2C ...
 *   +--------------------------------------+
 *
 * 移植 = 实现 sgm41513_io.h 并按需调整 sgm41513_conf.h，驱动其余部分
 * 不触碰硬件。
 *
 * API 贯穿使用的单位约定：电流 mA（uint32_t），电压 mV（uint32_t）。
 * 每个 set 函数按硬件档位就近取整并钳位到支持范围；对应的 get 函数
 * 返回实际生效值。
 */

#ifndef SGM41513_H
#define SGM41513_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "sgm41513_conf.h"
#include "sgm41513_regs.h"   /* 寄存器数量与地址（同时向高级用户
                                开放寄存器级访问） */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* 结果码（全部公共 API 通用）                                             */
/* ---------------------------------------------------------------------- */

typedef enum {
    SGM41513_OK = 0,            /* 成功                                      */
    SGM41513_ERR_IO,            /* I2C 通信失败（来自移植层）                */
    SGM41513_ERR_PARAM,         /* 参数非法（空指针等）                      */
    SGM41513_ERR_NOT_READY,     /* 器件无应答 / ID 异常                      */
    SGM41513_ERR_NOT_SUPPORTED, /* 功能被 sgm41513_conf.h 裁剪               */
    SGM41513_ERR_VERIFY,        /* 回读校验不一致（SGM41513_VERIFY_WRITES）  */
} sgm41513_result_t;

/* ---------------------------------------------------------------------- */
/* 设备句柄——每片充电芯片一个                                              */
/* ---------------------------------------------------------------------- */

typedef struct {
    void    *io_ctx;    /* 透传给 io 函数的不透明指针，多总线设计时用作
                           总线句柄，单总线系统填 NULL                    */
    uint8_t dev_addr;   /* 7 位 I2C 地址，通常为 SGM41513_I2C_ADDR        */
#if SGM41513_REG_SHADOW
    uint8_t shadow[SGM41513_NUM_REGS]; /* 可读可写寄存器的影子缓存(shadow) */
#endif
    uint8_t variant;    /* 检测到的器件变体，见 sgm41513_variant_t         */
    uint8_t inited;     /* 由 sgm41513_init() 置位                        */
} sgm41513_dev_t;

typedef enum {
    SGM41513_CHIP_SGM41513  = 0,
    SGM41513_CHIP_SGM41513A = 1,
    SGM41513_CHIP_SGM41513D = 2,
} sgm41513_variant_t;
/* 注意：SGM41513_VARIANT_SGM41513/A/D（不带 CHIP）是 sgm41513_conf.h
 * 中的编译期选择宏——两者不要混淆。 */

/* 器件识别信息（REG0B）。注意：PN 位无法区分 A 与 D 变体；            */
/* 'variant' 借助 SGM41513_VARIANT 宏解决该歧义。                        */
typedef struct {
    uint8_t pn_raw;             /* PN[3:0] 原始值：0 = SGM41513，1 = A 或 D */
    sgm41513_variant_t variant; /* 当前所能确定的最准变体                  */
    uint8_t dev_rev;            /* DEV_REV[1:0] 芯片版本                   */
} sgm41513_id_t;

/* ---------------------------------------------------------------------- */
/* 状态类型（REG08 / REG09 / REG0A）                                       */
/* ---------------------------------------------------------------------- */

/* VBUS / 输入源类型，按变体从 VBUS_STAT[2:0] 解码 */
typedef enum {
    SGM41513_VBUS_NONE = 0,       /* 000：无输入                          */
    SGM41513_VBUS_USB_SDP = 1,    /* 001：USB 主机 SDP（500 mA）          */
    SGM41513_VBUS_USB_CDP = 2,    /* 010：USB CDP 1.5 A（A/D 变体）       */
    SGM41513_VBUS_USB_DCP = 3,    /* 011：USB DCP 2.4 A（A/D 变体）       */
    SGM41513_VBUS_UNKNOWN = 5,    /* 101：未知适配器 500 mA（A/D）        */
    SGM41513_VBUS_NONSTD = 6,     /* 110：非标准适配器（A/D）             */
    SGM41513_VBUS_OTG = 7,        /* 111：OTG 升压(boost)工作中           */
    /* 仅 SGM41513（PSEL 变体）：010 表示「适配器 2.4 A，PSEL 低电平」  */
    SGM41513_VBUS_ADAPTER_PSEL = 9,
    SGM41513_VBUS_RESERVED = 10,  /* 上表未列出的编码                     */
} sgm41513_vbus_type_t;

/* 充电阶段，从 CHRG_STAT[1:0] 解码 */
typedef enum {
    SGM41513_CHRG_OFF = 0,        /* 未充电                               */
    SGM41513_CHRG_PRECHARGE = 1,  /* 涓流 / 预充电阶段                    */
    SGM41513_CHRG_FAST = 2,       /* 快充（CC 或 CV）                     */
    SGM41513_CHRG_DONE = 3,       /* 充电已终止                           */
} sgm41513_charge_status_t;

typedef struct {
    sgm41513_vbus_type_t vbus_type;    /* 解码后的输入源类型             */
    uint8_t vbus_raw;                  /* VBUS_STAT[2:0] 原始编码        */
    sgm41513_charge_status_t charge_status; /* 充电阶段                  */
    bool power_good;                   /* PG_STAT：输入电源正常          */
    bool thermal_regulating;           /* THERM_STAT：热调节中           */
    bool vsys_regulating;              /* VSYS_STAT：VSYS_MIN 调节中     */
} sgm41513_status_t;

typedef enum {
    SGM41513_CHRG_FAULT_NONE = 0,
    SGM41513_CHRG_FAULT_INPUT = 1,      /* VAC 过压或 VBAT<VVBUS<3.8 V    */
    SGM41513_CHRG_FAULT_THERMAL_SD = 2, /* 热关断                         */
    SGM41513_CHRG_FAULT_TIMER = 3,      /* 充电安全计时器超时             */
} sgm41513_charge_fault_t;

typedef enum {
    SGM41513_NTC_NORMAL = 0,
    SGM41513_NTC_WARM = 2,   /* 仅降压(buck)，降电压继续充电              */
    SGM41513_NTC_COOL = 3,   /* 仅降压(buck)，降电流继续充电              */
    SGM41513_NTC_COLD = 5,   /* 充电暂停                                  */
    SGM41513_NTC_HOT = 6,    /* 充电暂停                                  */
    SGM41513_NTC_UNKNOWN = 7,
} sgm41513_ntc_fault_t;

typedef struct {
    bool watchdog_fault;              /* I2C 看门狗超时（已锁存）         */
    bool boost_fault;                 /* 升压(boost)无法启动 / 过载       */
    sgm41513_charge_fault_t charge_fault;
    bool battery_ovp;                 /* 电池过压（BATOVP）               */
    sgm41513_ntc_fault_t ntc_fault;   /* 实时热敏电阻状态                 */
    uint8_t raw;                      /* 第二次读取的 REG09 原始值        */
} sgm41513_faults_t;

typedef struct {
    bool vbus_good;      /* VBUS_GD：检测到良好 VBUS                        */
    bool vindpm_active;  /* 输入电压调节环路工作中                          */
    bool iindpm_active;  /* 输入电流调节环路工作中                          */
    bool topoff_active;  /* 涓流补充(top-off)计时中                         */
    bool input_ovp;      /* ACOV_STAT：输入过压事件                         */
} sgm41513_dpm_status_t;

/* ---------------------------------------------------------------------- */
/* 配置枚举                                                                */
/* ---------------------------------------------------------------------- */

typedef enum {
    SGM41513_VINDPM_OS_3900MV = 0,   /* 适用 5 V 适配器                   */
    SGM41513_VINDPM_OS_5900MV = 1,   /* 适用 9 V 适配器                   */
    SGM41513_VINDPM_OS_7500MV = 2,   /* 适用 9 V 适配器                   */
    SGM41513_VINDPM_OS_10500MV = 3,  /* 适用 12 V 适配器                  */
} sgm41513_vindpm_os_t;

typedef enum {
    SGM41513_BAT_TRACK_OFF = 0,
    SGM41513_BAT_TRACK_200MV = 1,    /* VINDPM = VBAT + 200 mV            */
    SGM41513_BAT_TRACK_250MV = 2,
    SGM41513_BAT_TRACK_300MV = 3,
} sgm41513_bat_track_t;              /* 仅在 OS = 3900 mV 档生效          */

typedef enum {
    SGM41513_INPUT_OVP_5500MV = 0,
    SGM41513_INPUT_OVP_6500MV = 1,   /* 5 V 输入                         */
    SGM41513_INPUT_OVP_10500MV = 2,  /* 9 V 输入                         */
    SGM41513_INPUT_OVP_14000MV = 3,  /* 12 V 输入（POR 默认）            */
} sgm41513_input_ovp_t;

typedef enum {
    SGM41513_BOOST_V_4850MV = 0,
    SGM41513_BOOST_V_5000MV = 1,
    SGM41513_BOOST_V_5150MV = 2,     /* POR 默认                         */
    SGM41513_BOOST_V_5300MV = 3,
} sgm41513_boost_volt_t;

typedef enum {
    SGM41513_BOOST_LIM_500MA = 0,
    SGM41513_BOOST_LIM_1200MA = 1,   /* POR 默认                         */
} sgm41513_boost_lim_t;

typedef enum {
    SGM41513_BOOST_FREQ_500KHZ = 0,
    SGM41513_BOOST_FREQ_1500KHZ = 1, /* POR 默认                         */
} sgm41513_boost_freq_t;

typedef enum {
    SGM41513_WDT_OFF = 0,
    SGM41513_WDT_40S = 1,
    SGM41513_WDT_80S = 2,
    SGM41513_WDT_160S = 3,           /* POR 默认                         */
} sgm41513_watchdog_t;

typedef enum {
    SGM41513_SAFETY_TIMER_7H = 0,
    SGM41513_SAFETY_TIMER_16H = 1,   /* POR 默认                         */
} sgm41513_safety_timer_t;

typedef enum {
    SGM41513_TREG_80C = 0,
    SGM41513_TREG_120C = 1,          /* POR 默认                         */
} sgm41513_treg_t;

typedef enum {
    SGM41513_TOPOFF_OFF = 0,         /* POR 默认                         */
    SGM41513_TOPOFF_15MIN = 1,
    SGM41513_TOPOFF_30MIN = 2,
    SGM41513_TOPOFF_45MIN = 3,
} sgm41513_topoff_t;

typedef enum {
    SGM41513_VREG_FT_OFF = 0,        /* POR 默认                         */
    SGM41513_VREG_FT_PLUS_8MV = 1,
    SGM41513_VREG_FT_MINUS_8MV = 2,
    SGM41513_VREG_FT_MINUS_16MV = 3,
} sgm41513_vreg_ft_t;

typedef enum {
    SGM41513_VRECHG_100MV = 0,       /* POR 默认                         */
    SGM41513_VRECHG_200MV = 1,
} sgm41513_vrechg_t;

typedef enum {
    SGM41513_TRICKLE_90MA = 0,       /* POR 默认                         */
    SGM41513_TRICKLE_30MA = 1,
} sgm41513_trickle_t;

typedef enum {
    SGM41513_ITERM_DEGLITCH_230MS = 0, /* POR 默认                       */
    SGM41513_ITERM_DEGLITCH_16MS = 1,
} sgm41513_term_deglitch_t;

typedef enum {
    SGM41513_MIN_VBAT_OTG_2950MV = 0, /* POR 默认                        */
    SGM41513_MIN_VBAT_OTG_2600MV = 1,
} sgm41513_min_vbat_otg_t;

typedef enum {
    SGM41513_STAT_PIN_CHARGE = 0,    /* STAT 引脚跟随充电状态             */
    SGM41513_STAT_PIN_MANUAL = 1,    /* STAT 引脚跟随 STAT_SET 图案       */
    SGM41513_STAT_PIN_DISABLE = 2,   /* STAT 引脚悬空                     */
} sgm41513_stat_pin_mode_t;

typedef enum {
    SGM41513_STAT_PATTERN_OFF = 0,   /* LED 熄灭                         */
    SGM41513_STAT_PATTERN_ON = 1,    /* LED 常亮                         */
    SGM41513_STAT_PATTERN_BLINK_1S = 2,  /* 亮 1 s / 灭 1 s              */
    SGM41513_STAT_PATTERN_BLINK_3S = 3,  /* 亮 1 s / 灭 3 s              */
} sgm41513_stat_pattern_t;

typedef enum {
    SGM41513_DPDM_HIZ = 0,           /* POR 默认                         */
    SGM41513_DPDM_0V = 1,
    SGM41513_DPDM_0V6 = 2,
    SGM41513_DPDM_3V3 = 3,
} sgm41513_dpdm_vset_t;

typedef enum {
    SGM41513_JEITA_VT2_5_5C = 0,     /* 冷区阈值 T2                      */
    SGM41513_JEITA_VT2_10C = 1,      /* POR 默认                         */
    SGM41513_JEITA_VT2_15C = 2,
    SGM41513_JEITA_VT2_20C = 3,
} sgm41513_jeita_vt2_t;

typedef enum {
    SGM41513_JEITA_VT3_40C = 0,      /* 暖区阈值 T3                      */
    SGM41513_JEITA_VT3_44_5C = 1,    /* POR 默认                         */
    SGM41513_JEITA_VT3_50_5C = 2,
    SGM41513_JEITA_VT3_54_5C = 3,
} sgm41513_jeita_vt3_t;

typedef enum {
    SGM41513_JEITA_COOL_I_50PCT = 0,
    SGM41513_JEITA_COOL_I_20PCT = 1, /* POR 默认                         */
} sgm41513_jeita_cool_i_t;

typedef enum {
    SGM41513_JEITA_WARM_I_0PCT = 0,
    SGM41513_JEITA_WARM_I_20PCT = 1,
    SGM41513_JEITA_WARM_I_50PCT = 2,
    SGM41513_JEITA_WARM_I_100PCT = 3, /* POR 默认                        */
} sgm41513_jeita_warm_i_t;

/* 由 sgm41513_jeita_configure() 写入的 JEITA 配置 */
typedef struct {
    sgm41513_jeita_vt2_t cool_threshold;    /* T2：0-10C 区间边界        */
    sgm41513_jeita_vt3_t warm_threshold;    /* T3：45-60C 区间边界       */
    bool  cool_charge_enable;    /* 冷区允许充电（JEITA_ISET_L_EN）       */
    bool  cool_voltage_4v1;      /* true：冷区内取 min(VREG, 4.1 V)       */
    sgm41513_jeita_cool_i_t cool_current;   /* 冷区充电电流              */
    bool  warm_voltage_use_vreg; /* true：暖区用 VREG，
                                    false：取 min(VREG, 4.1 V)           */
    sgm41513_jeita_warm_i_t warm_current;   /* 暖区充电电流              */
} sgm41513_jeita_cfg_t;

/* ====================================================================== */
/* API                                                                    */
/* ====================================================================== */

/* ---- 1. 初始化 / 器件识别 ---------------------------------------------- */

/*
 * 初始化设备句柄并确认芯片应答正常。
 * - 调用一次 sgm41513_io_init()。
 * - 读取 REG0B 并校验器件（SGMPART = 0，PN = 0 或 1）。
 * - 检测变体：PN 0000 -> SGM41513，PN 0001 -> A 或 D
 *   （借助 sgm41513_conf.h 的 SGM41513_VARIANT 确定）。
 * - SGM41513_REG_SHADOW = 1 时从芯片加载全部可读可写寄存器镜像，
 *   供之后 restore_settings() 重放。
 *
 * 注意：此后写入任意寄存器都会使芯片进入 I2C「主机模式」——必须
 * 周期性喂看门狗（sgm41513_feed_watchdog()）或关闭看门狗
 * （sgm41513_set_watchdog()）。
 */
sgm41513_result_t sgm41513_init(sgm41513_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr);

/* 读取并解码器件识别信息（REG0B）。 */
sgm41513_result_t sgm41513_get_id(sgm41513_dev_t *dev, sgm41513_id_t *id);

/*
 * 将全部可读可写寄存器恢复为上电默认值（REG0B REG_RST）。
 * 影子缓存(shadow)（若启用）也一并重置为 POR 默认值。
 * 复位后芯片回到默认的自主充电模式。
 */
sgm41513_result_t sgm41513_reset(sgm41513_dev_t *dev);

#if SGM41513_REG_SHADOW
/*
 * 重放本驱动此前写过的全部可读可写寄存器。
 * 看门狗超时（芯片寄存器被复位）后、或器件退出船运模式
 * （EN_HIZ 会被强制置 1）后调用。
 */
sgm41513_result_t sgm41513_restore_settings(sgm41513_dev_t *dev);
#endif

/* ---- 2. 充电控制（REG01/02/03/04/05/0F） ------------------------------- */

/* 充电总开关（REG01 CHG_CONFIG）。物理 nCE 引脚也必须拉低充电才会   */
/* 真正进行。将 ICHG 设为 0 同样会关闭充电。                          */
sgm41513_result_t sgm41513_charge_enable(sgm41513_dev_t *dev, bool enable);

/* 快充电流，0..3000 mA（非线性 64 档表）。                            */
sgm41513_result_t sgm41513_set_ichg(sgm41513_dev_t *dev, uint32_t ma);
sgm41513_result_t sgm41513_get_ichg(sgm41513_dev_t *dev, uint32_t *ma);

/* 电池调节电压，3856..4624 mV（32 mV 步进，码点 15 为 4350 mV 特例）。 */
/* 细调通过 sgm41513_set_vreg_ft()。                                   */
sgm41513_result_t sgm41513_set_vreg(sgm41513_dev_t *dev, uint32_t mv);
sgm41513_result_t sgm41513_get_vreg(sgm41513_dev_t *dev, uint32_t *mv);

/* VREG 微调：在 VREG 之上 0 / +8 / -8 / -16 mV。                      */
sgm41513_result_t sgm41513_set_vreg_ft(sgm41513_dev_t *dev,
                                       sgm41513_vreg_ft_t ft);

/* 预充电电流，5..240 mA（16 档表）。                                  */
sgm41513_result_t sgm41513_set_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t ma);
sgm41513_result_t sgm41513_get_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t *ma);

/* 充电终止电流，5..240 mA（16 档表）。                                */
sgm41513_result_t sgm41513_set_term_current(sgm41513_dev_t *dev,
                                            uint32_t ma);
sgm41513_result_t sgm41513_get_term_current(sgm41513_dev_t *dev,
                                            uint32_t *ma);

/* 使能 / 关闭按电流判定充电终止（REG05 EN_TERM）。                    */
sgm41513_result_t sgm41513_set_term_enable(sgm41513_dev_t *dev, bool enable);

/* 终止检测去抖时间：230 ms（默认）或 16 ms。                          */
sgm41513_result_t sgm41513_set_term_deglitch(sgm41513_dev_t *dev,
                                             sgm41513_term_deglitch_t t);

/* 低于 VREG 的再充电阈值：100 mV（默认）或 200 mV。                   */
sgm41513_result_t sgm41513_set_recharge_threshold(sgm41513_dev_t *dev,
                                                  sgm41513_vrechg_t mv);

/* 可选的充电终止后涓流补充(top-off)延时计时器。                       */
sgm41513_result_t sgm41513_set_topoff_timer(sgm41513_dev_t *dev,
                                            sgm41513_topoff_t t);

/* 深度放电电芯（VBAT < 约 2.2 V）的涓流电流。                         */
sgm41513_result_t sgm41513_set_trickle_current(sgm41513_dev_t *dev,
                                               sgm41513_trickle_t ma);

/* 最小系统电压，2600..3700 mV（NVDC，REG01 SYS_MIN）。                */
sgm41513_result_t sgm41513_set_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t mv);
sgm41513_result_t sgm41513_get_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t *mv);

/* 启动 OTG 升压(boost)的最低电池电压（REG01 MIN_BAT_SEL）。           */
sgm41513_result_t sgm41513_set_min_vbat_otg(sgm41513_dev_t *dev,
                                            sgm41513_min_vbat_otg_t sel);

/* ---- 3. 输入管理（REG00/06/07/0F） ------------------------------------- */

/*
 * 输入电流限流（IINDPM），100..3200 mA，100 mA 步进。
 * 警告：输入源检测（PSEL / D+/D- BC1.2）完成后硬件会自动覆盖该
 * 寄存器。待 sgm41513_input_detect_done() 返回 true 后重新写入。
 */
sgm41513_result_t sgm41513_set_iindpm(sgm41513_dev_t *dev, uint32_t ma);
sgm41513_result_t sgm41513_get_iindpm(sgm41513_dev_t *dev, uint32_t *ma);

/*
 * VINDPM 偏置电压档选择。每档覆盖 16 个 100 mV 步进：
 *   3900 mV -> 3.9 .. 5.4 V    5900 mV -> 5.9 .. 7.4 V
 *   7500 mV -> 7.5 .. 9.0 V   10500 mV -> 10.5 .. 12.0 V
 * VBAT 跟踪仅在 3900 mV 档有效。
 */
sgm41513_result_t sgm41513_set_vindpm_os(sgm41513_dev_t *dev,
                                         sgm41513_vindpm_os_t os);

/*
 * 输入电压调节阈值（VINDPM），绝对值 mV。内部按当前 VINDPM_OS 档
 * 换算并钳位到该档 1.5 V 宽的窗口；需要时先调 set_vindpm_os()。
 */
sgm41513_result_t sgm41513_set_vindpm(sgm41513_dev_t *dev, uint32_t mv);
sgm41513_result_t sgm41513_get_vindpm(sgm41513_dev_t *dev, uint32_t *mv);

/* VINDPM 跟踪 VBAT + 200/250/300 mV（需 OS = 3900 mV 档）。           */
sgm41513_result_t sgm41513_set_vindpm_bat_track(sgm41513_dev_t *dev,
                                                sgm41513_bat_track_t track);

/* 输入过压保护阈值。                                                   */
sgm41513_result_t sgm41513_set_input_ovp(sgm41513_dev_t *dev,
                                         sgm41513_input_ovp_t ovp);

/*
 * 高阻模式(HIZ)：将 VBUS 与内部电路断开（变换器停止，电池漏流
 * 约 8.5 uA）。sgm41513_otg_enable() 流程中也会自动清除该位。
 */
sgm41513_result_t sgm41513_set_hiz(sgm41513_dev_t *dev, bool enable);

/* 强制重新进行一次输入源检测（REG07 IINDET_EN）。                     */
sgm41513_result_t sgm41513_force_input_detection(sgm41513_dev_t *dev);

/* VBUS 插入后的 PSEL / D+/D- 检测完成时返回 true。                    */
sgm41513_result_t sgm41513_input_detect_done(sgm41513_dev_t *dev,
                                             bool *done);

/* 降压(buck)变换器轻载 PFM 模式（默认开 = 轻载效率更高；
 * 追求最低纹波时关闭）。                                              */
sgm41513_result_t sgm41513_set_pfm_enable(sgm41513_dev_t *dev, bool enable);

/* 低 IINDPM 档下仍保持输入 FET（Q1）全开（效率更高，
 * 电流检测精度略降）。                                                */
sgm41513_result_t sgm41513_set_q1_fullon(sgm41513_dev_t *dev, bool enable);

/* 屏蔽（true）或放开 VINDPM / IINDPM 的 nINT 脉冲（REG0A）。          */
sgm41513_result_t sgm41513_set_dpm_int_mask(sgm41513_dev_t *dev,
                                            bool mask_vindpm,
                                            bool mask_iindpm);

/* ---- 4. OTG 反向升压（REG01/02/06/0D） --------------------------------- */

#if SGM41513_USE_OTG
/*
 * 使能 / 关闭 OTG 反向升压(boost)。使能时会先自动清除 EN_HIZ 并等待
 * >= 30 ms（数据手册要求）再置位 OTG_CONFIG。升压前提：VBAT 高于
 * MIN_BAT_OTG 门限、VBUS 低于 VBAT + VSLEEP、TS 处于升压温度窗口内。
 * OTG 优先于充电；HIZ 优先于 OTG。
 */
sgm41513_result_t sgm41513_otg_enable(sgm41513_dev_t *dev, bool enable);

/* 升压(boost)调节电压（OTG 工作时的 VBUS）。                          */
sgm41513_result_t sgm41513_set_boost_voltage(sgm41513_dev_t *dev,
                                             sgm41513_boost_volt_t volt);

/* 升压(boost)限流：500 mA 或 1200 mA。                                */
sgm41513_result_t sgm41513_set_boost_current_limit(sgm41513_dev_t *dev,
                                                   sgm41513_boost_lim_t lim);

/* 升压(boost)开关频率：1.5 MHz（默认）或 500 kHz。                    */
sgm41513_result_t sgm41513_set_boost_freq(sgm41513_dev_t *dev,
                                          sgm41513_boost_freq_t freq);

/*
 * ITERM 缩放（REG0D OTGF_ITREMR，与升压频率为同一 bit 的充电模式
 * 语义）：enable = true 时若 ICHG > 300 mA，编程的 ITERM 按 6 倍
 * 缩放（有利于大容量电芯的终止判定精度）。
 */
sgm41513_result_t sgm41513_set_iterm_x6(sgm41513_dev_t *dev, bool enable);
#endif /* SGM41513_USE_OTG */

/* ---- 5. 状态与故障（REG08/09/0A） --------------------------------------- */

/*
 * 读取运行状态（REG08）：输入源类型、充电阶段、电源正常
 * (power good)、热调节、VSYS_MIN 调节。
 */
sgm41513_result_t sgm41513_get_status(sgm41513_dev_t *dev,
                                      sgm41513_status_t *status);

/*
 * 读取实时故障标志（REG09）。硬件将故障位锁存直到被读取；因此本
 * 函数连续读取 REG09 两次并解码第二次的值，保证返回的是当前故障。
 * NTC_FAULT 为实时值，直接采用。读取同时清除锁存的历史记录。
 */
sgm41513_result_t sgm41513_get_faults(sgm41513_dev_t *dev,
                                      sgm41513_faults_t *faults);

/*
 * 读取电源路径 / DPM 状态（REG0A）：VBUS 正常、VINDPM / IINDPM
 * 环路工作中、涓流补充(top-off)计时中、输入过压事件。
 */
sgm41513_result_t sgm41513_get_dpm_status(sgm41513_dev_t *dev,
                                          sgm41513_dpm_status_t *dpm);

/* ---- 6. JEITA（REG05/07/0C） -------------------------------------------- */

#if SGM41513_USE_JEITA
/*
 * 编程完整 JEITA 温度曲线（TS 引脚 NTC 分压）。T1..T4 硬件窗口
 * （0..60 C）之外充电暂停；本调用配置冷区（T2）与暖区（T3）内部
 * 阈值，以及区间内施加的电压 / 电流降额。
 */
sgm41513_result_t sgm41513_jeita_configure(sgm41513_dev_t *dev,
                                           const sgm41513_jeita_cfg_t *cfg);
#endif /* SGM41513_USE_JEITA */

/* ---- 7. 看门狗 / 计时器 / 热（REG05/07） -------------------------------- */

/* I2C 看门狗周期：关闭、40 s、80 s 或 160 s（POR 默认）。             */
sgm41513_result_t sgm41513_set_watchdog(sgm41513_dev_t *dev,
                                        sgm41513_watchdog_t wdt);

/* 喂看门狗（REG01 WD_RST，写 1 自清零）。                             */
sgm41513_result_t sgm41513_feed_watchdog(sgm41513_dev_t *dev);

/* 充电安全计时器：7 h 或 16 h，使能 / 关闭。TMR2X 置位时，DPM /
 * JEITA 冷区 / 热调节期间计时器以半速运行
 * （见 sgm41513_set_safety_timer_slow2x）。
 */
sgm41513_result_t sgm41513_set_safety_timer(sgm41513_dev_t *dev,
                                            sgm41513_safety_timer_t hours,
                                            bool enable);
sgm41513_result_t sgm41513_set_safety_timer_slow2x(sgm41513_dev_t *dev,
                                                   bool enable);

/* 降压(buck)热调节阈值：80 C 或 120 C。                               */
sgm41513_result_t sgm41513_set_thermal_reg_threshold(sgm41513_dev_t *dev,
                                                     sgm41513_treg_t treg);

/* ---- 8. 船运模式 / BATFET（REG07） --------------------------------------- */

#if SGM41513_USE_SHIP
/*
 * 进入船运模式(ship mode)（BATFET 断开，电池漏流约 2.5 uA）。
 * 'delayed' = true 时先等待 tSM_DLY（约 12 s）再断开电池。退出方式：
 * 施加 VBUS、nQON 拉低 >= 1 s、REG_RST、或清除 BATFET_DIS（驱动的
 * restore_settings() 会重放你的配置并清除船运模式退出时强制置位的
 * HIZ 状态）。
 */
sgm41513_result_t sgm41513_enter_ship_mode(sgm41513_dev_t *dev,
                                           bool delayed);

/* 使能 / 关闭 nQON 长按（>= 10 s）BATFET 复位功能。                   */
sgm41513_result_t sgm41513_set_batfet_reset_enable(sgm41513_dev_t *dev,
                                                   bool enable);
#endif /* SGM41513_USE_SHIP */

/* ---- 9. PUMPX（REG0D） --------------------------------------------------- */

#if SGM41513_USE_PUMPX
/*
 * 面向可调适配器（如部分 QC 类适配器）的 PUMPX 电压步进协议：在
 * VBUS 上产生电流脉冲，请求适配器升高(up)或降低(down)输出电压。
 * 先使能，再触发；用 sgm41513_pumpx_busy() 等待完成。
 */
sgm41513_result_t sgm41513_pumpx_enable(sgm41513_dev_t *dev, bool enable);
sgm41513_result_t sgm41513_pumpx_trigger_up(sgm41513_dev_t *dev);
sgm41513_result_t sgm41513_pumpx_trigger_down(sgm41513_dev_t *dev);
sgm41513_result_t sgm41513_pumpx_busy(sgm41513_dev_t *dev, bool *busy);
#endif /* SGM41513_USE_PUMPX */

/* ---- 10. STAT 引脚 / D+ D- 线（REG00/0D/0F） ----------------------------- */

/* STAT 开漏输出功能（驱动 LED 或接主机 GPIO）。                        */
sgm41513_result_t sgm41513_set_stat_pin_mode(sgm41513_dev_t *dev,
                                             sgm41513_stat_pin_mode_t mode);
sgm41513_result_t sgm41513_set_stat_pin_pattern(sgm41513_dev_t *dev,
                                                sgm41513_stat_pattern_t p);

/*
 * 手动驱动 D+ / D- 电压（A/D 变体，例如用于分压型适配器检测）。
 * VBUS 插入后自动复位；仅在输入检测完成后有效。
 */
sgm41513_result_t sgm41513_set_dpdm_voltage(sgm41513_dev_t *dev,
                                            sgm41513_dpdm_vset_t dp,
                                            sgm41513_dpdm_vset_t dm);

/* ---- 11. 寄存器级访问 ---------------------------------------------------- */

/* 调试 / 访问未封装功能用的裸寄存器读写。                              */
/* update_bits() 以 (val & mask) 执行读-改-写。                         */
sgm41513_result_t sgm41513_read_reg(sgm41513_dev_t *dev, uint8_t reg,
                                    uint8_t *val);
sgm41513_result_t sgm41513_write_reg(sgm41513_dev_t *dev, uint8_t reg,
                                     uint8_t val);
sgm41513_result_t sgm41513_update_bits(sgm41513_dev_t *dev, uint8_t reg,
                                       uint8_t mask, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_H */
