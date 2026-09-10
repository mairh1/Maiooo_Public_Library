/**
 * @file    sgm41513.h
 * @brief   SGM41513 系列电池充电芯片通用驱动公共 API
 * @details 器件：SGM41513 / SGM41513A / SGM41513D（SGMicro 圣邦微），
 *          输入 3.9-13.5 V、3 A 单节锂离子电池充电器，NVDC
 *          电源路径(power path)，I2C 接口（7 位地址 0x1A，400 kHz），
 *          OTG 反向升压(boost)，JEITA，船运模式(ship mode)。
 *
 *          分层架构：
 *
 *          +--------------------------------------+
 *          |  应用层                               |   用户代码
 *          +--------------------------------------+
 *                     |  本 API（sgm41513.h）
 *          +--------------------------------------+
 *          |  驱动核心  (sgm41513.c)              |   可移植、纯 C99、
 *          |  配置      (sgm41513_conf.h)         |   无动态内存
 *          +--------------------------------------+
 *                     |  io 契约（sgm41513_io.h）：
 *                     |  4 必选 + 2 可选（THREAD_SAFE=1 时）
 *          +--------------------------------------+
 *          |  移植层（用户提供）                    |   STM32 / ESP-IDF /
 *          |                                      |   Linux / RTOS ...
 *          +--------------------------------------+
 *
 *          移植 = 实现 sgm41513_io.h 并按需调整 sgm41513_conf.h，驱动
 *          其余部分不触碰硬件。
 *
 *          API 贯穿使用的单位约定：电流 mA（uint32_t），电压 mV
 *          （uint32_t）。每个 set 函数按硬件档位就近取整并钳位到支持
 *          范围；对应的 get 函数返回实际生效值。
 * @note    数据手册：SGM41513_SGM41513A_SGM41513D, APRIL 2025 REV. C.1。
 *          全部公共 API 须在线程上下文调用，禁止在 ISR 中调用。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-10
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

/* ═════════════════════════════════════════════════════════════════════════════
 * 结果码（全部公共 API 通用）
 * ═════════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    SGM41513_OK = 0,            /**< 成功 */
    SGM41513_ERR_IO,            /**< I2C 通信失败（来自移植层） */
    SGM41513_ERR_PARAM,         /**< 参数非法（空指针等） */
    SGM41513_ERR_NOT_READY,     /**< 器件无应答 / ID 异常 */
    SGM41513_ERR_NOT_SUPPORTED, /**< 预留：当前功能裁剪走编译期移除
                                     （见 sgm41513_conf.h），此结果码
                                     暂无产生路径 */
    SGM41513_ERR_VERIFY,        /**< 回读校验不一致（SGM41513_VERIFY_WRITES） */
} sgm41513_result_t;

/* ═════════════════════════════════════════════════════════════════════════════
 * 设备句柄——每片充电芯片一个
 * ═════════════════════════════════════════════════════════════════════════════ */

typedef struct
{
    void    *io_ctx;    /**< 透传给 io 函数的不透明指针，多总线设计时用作
                             总线句柄，单总线系统填 NULL */
    uint8_t dev_addr;   /**< 7 位 I2C 地址，通常为 SGM41513_I2C_ADDR */
#if SGM41513_REG_SHADOW
    uint8_t shadow[SGM41513_NUM_REGS]; /**< 可读可写寄存器的影子缓存(shadow) */
#endif
    uint8_t variant;    /**< 检测到的器件变体，见 sgm41513_variant_t */
    uint8_t inited;     /**< 由 sgm41513_init() 置位 */
} sgm41513_dev_t;

typedef enum
{
    SGM41513_CHIP_SGM41513  = 0,    /**< 基础型号（init 检测到 PN = 0000） */
    SGM41513_CHIP_SGM41513A = 1,    /**< A 变体（PN = 0001 且
                                         SGM41513_VARIANT 指定为 A） */
    SGM41513_CHIP_SGM41513D = 2,    /**< D 变体（PN = 0001 且
                                         SGM41513_VARIANT 指定为 D） */
} sgm41513_variant_t;
/* 注意：SGM41513_VARIANT_SGM41513/A/D（不带 CHIP）是 sgm41513_conf.h
 * 中的编译期选择宏——两者不要混淆。 */

/* 器件识别信息（REG0B）。注意：PN 位无法区分 A 与 D 变体；            */
/* 'variant' 借助 SGM41513_VARIANT 宏解决该歧义。                        */
typedef struct
{
    uint8_t pn_raw;             /**< PN[3:0] 原始值：0 = SGM41513，1 = A 或 D */
    sgm41513_variant_t variant; /**< 当前所能确定的最准变体 */
    uint8_t dev_rev;            /**< DEV_REV[1:0] 芯片版本 */
} sgm41513_id_t;

/* ═════════════════════════════════════════════════════════════════════════════
 * 状态类型（REG08 / REG09 / REG0A）
 * ═════════════════════════════════════════════════════════════════════════════ */

/* VBUS / 输入源类型，按变体从 VBUS_STAT[2:0] 解码 */
typedef enum
{
    SGM41513_VBUS_NONE = 0,       /**< 000：无输入 */
    SGM41513_VBUS_USB_SDP = 1,    /**< 001：USB 主机 SDP（500 mA） */
    SGM41513_VBUS_USB_CDP = 2,    /**< 010：USB CDP 1.5 A（A/D 变体） */
    SGM41513_VBUS_USB_DCP = 3,    /**< 011：USB DCP 2.4 A（A/D 变体） */
    SGM41513_VBUS_UNKNOWN = 5,    /**< 101：未知适配器 500 mA（A/D） */
    SGM41513_VBUS_NONSTD = 6,     /**< 110：非标准适配器（A/D） */
    SGM41513_VBUS_OTG = 7,        /**< 111：OTG 升压(boost)工作中 */
    SGM41513_VBUS_ADAPTER_PSEL = 9, /**< 仅 SGM41513（PSEL 变体）：
                                         010 表示「适配器 2.4 A，
                                         PSEL 低电平」 */
    SGM41513_VBUS_RESERVED = 10,  /**< 上表未列出的编码 */
} sgm41513_vbus_type_t;

/* 充电阶段，从 CHRG_STAT[1:0] 解码 */
typedef enum
{
    SGM41513_CHRG_OFF = 0,        /**< 未充电 */
    SGM41513_CHRG_PRECHARGE = 1,  /**< 涓流 / 预充电阶段 */
    SGM41513_CHRG_FAST = 2,       /**< 快充（CC 或 CV） */
    SGM41513_CHRG_DONE = 3,       /**< 充电已终止 */
} sgm41513_charge_status_t;

typedef struct
{
    sgm41513_vbus_type_t vbus_type;         /**< 解码后的输入源类型 */
    uint8_t vbus_raw;                       /**< VBUS_STAT[2:0] 原始编码 */
    sgm41513_charge_status_t charge_status; /**< 充电阶段 */
    bool power_good;                        /**< PG_STAT：输入电源正常 */
    bool thermal_regulating;                /**< THERM_STAT：热调节中 */
    bool vsys_regulating;                   /**< VSYS_STAT：VSYS_MIN 调节中 */
} sgm41513_status_t;

typedef enum
{
    SGM41513_CHRG_FAULT_NONE = 0,       /**< 无充电故障 */
    SGM41513_CHRG_FAULT_INPUT = 1,      /**< VAC 过压或 VBAT<VVBUS<3.8 V */
    SGM41513_CHRG_FAULT_THERMAL_SD = 2, /**< 热关断 */
    SGM41513_CHRG_FAULT_TIMER = 3,      /**< 充电安全计时器超时 */
} sgm41513_charge_fault_t;

typedef enum
{
    SGM41513_NTC_NORMAL = 0,  /**< 热敏电阻状态正常 */
    SGM41513_NTC_WARM = 2,    /**< 仅降压(buck)，降电压继续充电 */
    SGM41513_NTC_COOL = 3,    /**< 仅降压(buck)，降电流继续充电 */
    SGM41513_NTC_COLD = 5,    /**< 充电暂停 */
    SGM41513_NTC_HOT = 6,     /**< 充电暂停 */
    SGM41513_NTC_UNKNOWN = 7, /**< 无法识别的编码 */
} sgm41513_ntc_fault_t;

typedef struct
{
    bool watchdog_fault;              /**< I2C 看门狗超时（已锁存） */
    bool boost_fault;                 /**< 升压(boost)无法启动 / 过载 */
    sgm41513_charge_fault_t charge_fault; /**< 充电故障类型 */
    bool battery_ovp;                 /**< 电池过压（BATOVP） */
    sgm41513_ntc_fault_t ntc_fault;   /**< 实时热敏电阻状态 */
    uint8_t raw;                      /**< 第二次读取的 REG09 原始值 */
} sgm41513_faults_t;

typedef struct
{
    bool vbus_good;      /**< VBUS_GD：检测到良好 VBUS */
    bool vindpm_active;  /**< 输入电压调节环路工作中 */
    bool iindpm_active;  /**< 输入电流调节环路工作中 */
    bool topoff_active;  /**< 涓流补充(top-off)计时中 */
    bool input_ovp;      /**< ACOV_STAT：输入过压事件 */
} sgm41513_dpm_status_t;

/* ═════════════════════════════════════════════════════════════════════════════
 * 配置枚举
 * ═════════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    SGM41513_VINDPM_OS_3900MV = 0,   /**< 适用 5 V 适配器 */
    SGM41513_VINDPM_OS_5900MV = 1,   /**< 适用 9 V 适配器 */
    SGM41513_VINDPM_OS_7500MV = 2,   /**< 适用 9 V 适配器 */
    SGM41513_VINDPM_OS_10500MV = 3,  /**< 适用 12 V 适配器 */
} sgm41513_vindpm_os_t;

typedef enum
{
    SGM41513_BAT_TRACK_OFF = 0,      /**< 关闭跟踪 */
    SGM41513_BAT_TRACK_200MV = 1,    /**< VINDPM = VBAT + 200 mV */
    SGM41513_BAT_TRACK_250MV = 2,    /**< VINDPM = VBAT + 250 mV */
    SGM41513_BAT_TRACK_300MV = 3,    /**< VINDPM = VBAT + 300 mV */
} sgm41513_bat_track_t;              /* 仅在 OS = 3900 mV 档生效 */

typedef enum
{
    SGM41513_INPUT_OVP_5500MV = 0,   /**< 输入过压阈值 5.5 V */
    SGM41513_INPUT_OVP_6500MV = 1,   /**< 5 V 输入 */
    SGM41513_INPUT_OVP_10500MV = 2,  /**< 9 V 输入 */
    SGM41513_INPUT_OVP_14000MV = 3,  /**< 12 V 输入（POR 默认） */
} sgm41513_input_ovp_t;

typedef enum
{
    SGM41513_BOOST_V_4850MV = 0,     /**< 升压输出 4.85 V */
    SGM41513_BOOST_V_5000MV = 1,     /**< 升压输出 5.00 V */
    SGM41513_BOOST_V_5150MV = 2,     /**< 升压输出 5.15 V（POR 默认） */
    SGM41513_BOOST_V_5300MV = 3,     /**< 升压输出 5.30 V */
} sgm41513_boost_volt_t;

typedef enum
{
    SGM41513_BOOST_LIM_500MA = 0,    /**< 升压限流 500 mA */
    SGM41513_BOOST_LIM_1200MA = 1,   /**< 升压限流 1200 mA（POR 默认） */
} sgm41513_boost_lim_t;

typedef enum
{
    SGM41513_BOOST_FREQ_500KHZ = 0,  /**< 升压开关频率 500 kHz */
    SGM41513_BOOST_FREQ_1500KHZ = 1, /**< 升压开关频率 1.5 MHz（POR 默认） */
} sgm41513_boost_freq_t;

typedef enum
{
    SGM41513_WDT_OFF = 0,            /**< 关闭看门狗 */
    SGM41513_WDT_40S = 1,            /**< 看门狗周期 40 s */
    SGM41513_WDT_80S = 2,            /**< 看门狗周期 80 s */
    SGM41513_WDT_160S = 3,           /**< 看门狗周期 160 s（POR 默认） */
} sgm41513_watchdog_t;

typedef enum
{
    SGM41513_SAFETY_TIMER_7H = 0,    /**< 充电安全计时器 7 h */
    SGM41513_SAFETY_TIMER_16H = 1,   /**< 充电安全计时器 16 h（POR 默认） */
} sgm41513_safety_timer_t;

typedef enum
{
    SGM41513_TREG_80C = 0,           /**< 热调节阈值 80 C */
    SGM41513_TREG_120C = 1,          /**< 热调节阈值 120 C（POR 默认） */
} sgm41513_treg_t;

typedef enum
{
    SGM41513_TOPOFF_OFF = 0,         /**< 关闭（POR 默认） */
    SGM41513_TOPOFF_15MIN = 1,       /**< 补充计时 15 min */
    SGM41513_TOPOFF_30MIN = 2,       /**< 补充计时 30 min */
    SGM41513_TOPOFF_45MIN = 3,       /**< 补充计时 45 min */
} sgm41513_topoff_t;

typedef enum
{
    SGM41513_VREG_FT_OFF = 0,        /**< 不微调（POR 默认） */
    SGM41513_VREG_FT_PLUS_8MV = 1,   /**< VREG + 8 mV */
    SGM41513_VREG_FT_MINUS_8MV = 2,  /**< VREG - 8 mV */
    SGM41513_VREG_FT_MINUS_16MV = 3, /**< VREG - 16 mV */
} sgm41513_vreg_ft_t;

typedef enum
{
    SGM41513_VRECHG_100MV = 0,       /**< 再充电阈值 VREG - 100 mV（POR 默认） */
    SGM41513_VRECHG_200MV = 1,       /**< 再充电阈值 VREG - 200 mV */
} sgm41513_vrechg_t;

typedef enum
{
    SGM41513_TRICKLE_90MA = 0,       /**< 涓流 90 mA（POR 默认） */
    SGM41513_TRICKLE_30MA = 1,       /**< 涓流 30 mA */
} sgm41513_trickle_t;

typedef enum
{
    SGM41513_ITERM_DEGLITCH_230MS = 0, /**< 终止检测去抖 230 ms（POR 默认） */
    SGM41513_ITERM_DEGLITCH_16MS = 1,  /**< 终止检测去抖 16 ms */
} sgm41513_term_deglitch_t;

typedef enum
{
    SGM41513_MIN_VBAT_OTG_2950MV = 0, /**< OTG 最低电池电压 2.95 V（POR 默认） */
    SGM41513_MIN_VBAT_OTG_2600MV = 1, /**< OTG 最低电池电压 2.6 V */
} sgm41513_min_vbat_otg_t;

typedef enum
{
    SGM41513_STAT_PIN_CHARGE = 0,    /**< STAT 引脚跟随充电状态 */
    SGM41513_STAT_PIN_MANUAL = 1,    /**< STAT 引脚跟随 STAT_SET 图案 */
    SGM41513_STAT_PIN_DISABLE = 2,   /**< STAT 引脚悬空 */
} sgm41513_stat_pin_mode_t;

typedef enum
{
    SGM41513_STAT_PATTERN_OFF = 0,       /**< LED 熄灭 */
    SGM41513_STAT_PATTERN_ON = 1,        /**< LED 常亮 */
    SGM41513_STAT_PATTERN_BLINK_1S = 2,  /**< 亮 1 s / 灭 1 s */
    SGM41513_STAT_PATTERN_BLINK_3S = 3,  /**< 亮 1 s / 灭 3 s */
} sgm41513_stat_pattern_t;

typedef enum
{
    SGM41513_DPDM_HIZ = 0,           /**< D+/D- 高阻（POR 默认） */
    SGM41513_DPDM_0V = 1,            /**< D+/D- 0 V */
    SGM41513_DPDM_0V6 = 2,           /**< D+/D- 0.6 V */
    SGM41513_DPDM_3V3 = 3,           /**< D+/D- 3.3 V */
} sgm41513_dpdm_vset_t;

typedef enum
{
    SGM41513_JEITA_VT2_5_5C = 0,     /**< 冷区阈值 T2 约 5.5 C */
    SGM41513_JEITA_VT2_10C = 1,      /**< 冷区阈值 T2 约 10 C（POR 默认） */
    SGM41513_JEITA_VT2_15C = 2,      /**< 冷区阈值 T2 约 15 C */
    SGM41513_JEITA_VT2_20C = 3,      /**< 冷区阈值 T2 约 20 C */
} sgm41513_jeita_vt2_t;

typedef enum
{
    SGM41513_JEITA_VT3_40C = 0,      /**< 暖区阈值 T3 约 40 C */
    SGM41513_JEITA_VT3_44_5C = 1,    /**< 暖区阈值 T3 约 44.5 C（POR 默认） */
    SGM41513_JEITA_VT3_50_5C = 2,    /**< 暖区阈值 T3 约 50.5 C */
    SGM41513_JEITA_VT3_54_5C = 3,    /**< 暖区阈值 T3 约 54.5 C */
} sgm41513_jeita_vt3_t;

typedef enum
{
    SGM41513_JEITA_COOL_I_50PCT = 0, /**< 冷区电流 50% ICHG */
    SGM41513_JEITA_COOL_I_20PCT = 1, /**< 冷区电流 20% ICHG（POR 默认） */
} sgm41513_jeita_cool_i_t;

typedef enum
{
    SGM41513_JEITA_WARM_I_0PCT = 0,   /**< 暖区电流 0% ICHG（停止充电） */
    SGM41513_JEITA_WARM_I_20PCT = 1,  /**< 暖区电流 20% ICHG */
    SGM41513_JEITA_WARM_I_50PCT = 2,  /**< 暖区电流 50% ICHG */
    SGM41513_JEITA_WARM_I_100PCT = 3, /**< 暖区电流 100% ICHG（POR 默认） */
} sgm41513_jeita_warm_i_t;

/* 由 sgm41513_jeita_configure() 写入的 JEITA 配置 */
typedef struct
{
    sgm41513_jeita_vt2_t cool_threshold;    /**< T2：0-10C 区间边界 */
    sgm41513_jeita_vt3_t warm_threshold;    /**< T3：45-60C 区间边界 */
    bool  cool_charge_enable;    /**< 冷区允许充电（JEITA_ISET_L_EN） */
    bool  cool_voltage_4v1;      /**< true：冷区内取 min(VREG, 4.1 V) */
    sgm41513_jeita_cool_i_t cool_current;   /**< 冷区充电电流 */
    bool  warm_voltage_use_vreg; /**< true：暖区用 VREG，
                                     false：取 min(VREG, 4.1 V) */
    sgm41513_jeita_warm_i_t warm_current;   /**< 暖区充电电流 */
} sgm41513_jeita_cfg_t;

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 1. 初始化 / 器件识别
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   初始化设备句柄并确认芯片应答正常
 * @details - 调用一次 sgm41513_io_init()。
 *          - 读取 REG0B 并校验器件（SGMPART = 0，PN = 0 或 1）。
 *          - 检测变体：PN 0000 -> SGM41513，PN 0001 -> A 或 D
 *            （借助 sgm41513_conf.h 的 SGM41513_VARIANT 确定）。
 *          - SGM41513_REG_SHADOW = 1 时从芯片加载全部可读可写寄存器
 *            镜像，供之后 sgm41513_restore_settings() 重放。
 *          注意：此后写入任意寄存器都会使芯片进入 I2C「主机模式」——
 *          必须周期性喂看门狗（sgm41513_feed_watchdog()）或关闭看门狗
 *          （sgm41513_set_watchdog()）。
 * @param   dev       设备句柄，由调用者分配。
 * @param   io_ctx    透传给 io 契约函数的不透明指针（如 I2C 总线句柄），
 *                    单总线系统传 NULL。
 * @param   dev_addr  7 位 I2C 从机地址；传 0 时使用默认 SGM41513_I2C_ADDR
 *                    (0x1A)。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM dev 为空；
 *          SGM41513_ERR_IO 总线初始化或通信失败；SGM41513_ERR_NOT_READY
 *          器件无应答或 ID 校验不符。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_init(sgm41513_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr);

/**
 * @brief   读取并解码器件识别信息（REG0B）
 * @param   dev  已初始化的设备句柄。
 * @param   id   输出：器件识别信息（PN 原始值、变体、芯片版本）。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM id 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_id(sgm41513_dev_t *dev, sgm41513_id_t *id);

/**
 * @brief   将全部可读可写寄存器恢复为上电默认值（REG0B REG_RST）
 * @details 影子缓存(shadow)（若启用）也一并重置为 POR 默认值。复位后
 *          芯片回到默认的自主充电模式。
 * @param   dev  已初始化的设备句柄。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_reset(sgm41513_dev_t *dev);

#if SGM41513_REG_SHADOW
/**
 * @brief   重放本驱动此前写过的全部可读可写寄存器
 * @details 在看门狗超时（芯片寄存器被复位）后、或器件退出船运模式
 *          （EN_HIZ 会被强制置 1）后调用。
 * @param   dev  已初始化的设备句柄。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化；SGM41513_ERR_VERIFY 回读
 *          校验不一致（SGM41513_VERIFY_WRITES=1 时）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_restore_settings(sgm41513_dev_t *dev);
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 2. 充电控制（REG01/02/03/04/05/0F）
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   充电总开关（REG01 CHG_CONFIG）
 * @details 物理 nCE 引脚也必须拉低充电才会真正进行。将 ICHG 设为 0
 *          同样会关闭充电。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能充电，false 关闭充电。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_charge_enable(sgm41513_dev_t *dev, bool enable);

/**
 * @brief   设置快充电流，单位 mA
 * @details 范围 0..3000 mA，就近取整到非线性 64 档表并钳位。
 * @param   dev  已初始化的设备句柄。
 * @param   ma   目标快充电流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_ichg(sgm41513_dev_t *dev, uint32_t ma);

/**
 * @brief   读取实际生效的快充电流，单位 mA
 * @param   dev  已初始化的设备句柄。
 * @param   ma   输出：实际生效的快充电流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM ma 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_ichg(sgm41513_dev_t *dev, uint32_t *ma);

/**
 * @brief   设置电池调节电压(VREG)，单位 mV
 * @details 范围 3856..4624 mV（32 mV 步进，码点 15 为 4350 mV 特例），
 *          就近取整并钳位。细调通过 sgm41513_set_vreg_ft()。
 * @param   dev  已初始化的设备句柄。
 * @param   mv   目标调节电压，单位 mV。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_vreg(sgm41513_dev_t *dev, uint32_t mv);

/**
 * @brief   读取实际生效的电池调节电压，单位 mV
 * @param   dev  已初始化的设备句柄。
 * @param   mv   输出：实际生效的调节电压，单位 mV。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM mv 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_vreg(sgm41513_dev_t *dev, uint32_t *mv);

/**
 * @brief   VREG 微调：在 VREG 之上 0 / +8 / -8 / -16 mV
 * @param   dev  已初始化的设备句柄。
 * @param   ft   微调档位，见 sgm41513_vreg_ft_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_vreg_ft(sgm41513_dev_t *dev,
                                       sgm41513_vreg_ft_t ft);

/**
 * @brief   设置预充电(涓流)电流，单位 mA
 * @details 范围 5..240 mA（16 档表），就近取整并钳位。
 * @param   dev  已初始化的设备句柄。
 * @param   ma   目标预充电电流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t ma);

/**
 * @brief   读取实际生效的预充电电流，单位 mA
 * @param   dev  已初始化的设备句柄。
 * @param   ma   输出：实际生效的预充电电流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM ma 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t *ma);

/**
 * @brief   设置充电终止电流，单位 mA
 * @details 范围 5..240 mA（16 档表），就近取整并钳位。
 * @param   dev  已初始化的设备句柄。
 * @param   ma   目标终止电流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_term_current(sgm41513_dev_t *dev,
                                            uint32_t ma);

/**
 * @brief   读取实际生效的充电终止电流，单位 mA
 * @param   dev  已初始化的设备句柄。
 * @param   ma   输出：实际生效的终止电流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM ma 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_term_current(sgm41513_dev_t *dev,
                                            uint32_t *ma);

/**
 * @brief   使能 / 关闭按电流判定充电终止（REG05 EN_TERM）
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能终止判定，false 关闭。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_term_enable(sgm41513_dev_t *dev, bool enable);

/**
 * @brief   设置终止检测去抖时间：230 ms（默认）或 16 ms
 * @param   dev  已初始化的设备句柄。
 * @param   t    去抖时间档位，见 sgm41513_term_deglitch_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_term_deglitch(sgm41513_dev_t *dev,
                                             sgm41513_term_deglitch_t t);

/**
 * @brief   设置低于 VREG 的再充电阈值：100 mV（默认）或 200 mV
 * @param   dev  已初始化的设备句柄。
 * @param   mv   再充电阈值档位，见 sgm41513_vrechg_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_recharge_threshold(sgm41513_dev_t *dev,
                                                  sgm41513_vrechg_t mv);

/**
 * @brief   设置可选的充电终止后涓流补充(top-off)延时计时器
 * @param   dev  已初始化的设备句柄。
 * @param   t    补充计时时长，见 sgm41513_topoff_t（off/15/30/45 min）。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_topoff_timer(sgm41513_dev_t *dev,
                                            sgm41513_topoff_t t);

/**
 * @brief   设置深度放电电芯（VBAT < 约 2.2 V）的涓流电流
 * @param   dev  已初始化的设备句柄。
 * @param   ma   涓流档位，见 sgm41513_trickle_t（90/30 mA）。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_trickle_current(sgm41513_dev_t *dev,
                                               sgm41513_trickle_t ma);

/**
 * @brief   设置最小系统电压，单位 mV
 * @details 范围 2600..3700 mV（NVDC 架构 REG01 SYS_MIN），就近取整
 *          并钳位。
 * @param   dev  已初始化的设备句柄。
 * @param   mv   目标最小系统电压，单位 mV。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t mv);

/**
 * @brief   读取实际生效的最小系统电压，单位 mV
 * @param   dev  已初始化的设备句柄。
 * @param   mv   输出：实际生效的最小系统电压，单位 mV。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM mv 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t *mv);

/**
 * @brief   设置启动 OTG 升压(boost)的最低电池电压（REG01 MIN_BAT_SEL）
 * @param   dev  已初始化的设备句柄。
 * @param   sel  门限档位，见 sgm41513_min_vbat_otg_t（2.95/2.6 V）。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_min_vbat_otg(sgm41513_dev_t *dev,
                                            sgm41513_min_vbat_otg_t sel);

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 3. 输入管理（REG00/06/07/0F）
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   设置输入电流限流(IINDPM)，单位 mA
 * @details 范围 100..3200 mA，100 mA 步进，四舍五入到最近档。
 *          警告：输入源检测（PSEL / D+/D- BC1.2）完成后硬件会自动
 *          覆盖该寄存器。待 sgm41513_input_detect_done() 返回 true 后
 *          重新写入。
 * @param   dev  已初始化的设备句柄。
 * @param   ma   目标输入限流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_iindpm(sgm41513_dev_t *dev, uint32_t ma);

/**
 * @brief   读取实际生效的输入电流限流，单位 mA
 * @param   dev  已初始化的设备句柄。
 * @param   ma   输出：实际生效的输入限流，单位 mA。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM ma 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_iindpm(sgm41513_dev_t *dev, uint32_t *ma);

/**
 * @brief   VINDPM 偏置电压档选择
 * @details 每档覆盖 16 个 100 mV 步进：
 *            3900 mV -> 3.9 .. 5.4 V    5900 mV -> 5.9 .. 7.4 V
 *            7500 mV -> 7.5 .. 9.0 V   10500 mV -> 10.5 .. 12.0 V
 *          VBAT 跟踪仅在 3900 mV 档有效。
 * @param   dev  已初始化的设备句柄。
 * @param   os   偏置档位，见 sgm41513_vindpm_os_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_vindpm_os(sgm41513_dev_t *dev,
                                         sgm41513_vindpm_os_t os);

/**
 * @brief   设置输入电压调节阈值(VINDPM)，绝对值 mV
 * @details 内部按当前 VINDPM_OS 档换算并钳位到该档 1.5 V 宽的窗口；
 *          需要时先调 sgm41513_set_vindpm_os()。
 * @param   dev  已初始化的设备句柄。
 * @param   mv   目标输入电压调节阈值，单位 mV。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_vindpm(sgm41513_dev_t *dev, uint32_t mv);

/**
 * @brief   读取实际生效的输入电压调节阈值，单位 mV
 * @param   dev  已初始化的设备句柄。
 * @param   mv   输出：实际生效的阈值（含 OS 偏置），单位 mV。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM mv 为空；
 *          SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_vindpm(sgm41513_dev_t *dev, uint32_t *mv);

/**
 * @brief   VINDPM 跟踪 VBAT + 200/250/300 mV（需 OS = 3900 mV 档）
 * @param   dev    已初始化的设备句柄。
 * @param   track  跟踪档位，见 sgm41513_bat_track_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_vindpm_bat_track(sgm41513_dev_t *dev,
                                                sgm41513_bat_track_t track);

/**
 * @brief   设置输入过压保护阈值
 * @param   dev  已初始化的设备句柄。
 * @param   ovp  阈值档位，见 sgm41513_input_ovp_t（5.5/6.5/10.5/14 V）。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_input_ovp(sgm41513_dev_t *dev,
                                         sgm41513_input_ovp_t ovp);

/**
 * @brief   高阻模式(HIZ)：将 VBUS 与内部电路断开
 * @details 变换器停止，电池漏流约 8.5 uA。
 *          sgm41513_otg_enable() 流程中也会自动清除该位。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 进入高阻模式，false 退出。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_hiz(sgm41513_dev_t *dev, bool enable);

/**
 * @brief   强制重新进行一次输入源检测（REG07 IINDET_EN）
 * @param   dev  已初始化的设备句柄。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_force_input_detection(sgm41513_dev_t *dev);

/**
 * @brief   查询输入源检测是否完成
 * @details VBUS 插入后的 PSEL / D+/D- 检测完成时 *done 置为 true。
 * @param   dev   已初始化的设备句柄。
 * @param   done  输出：true 表示检测完成。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM done
 *          为空；SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY
 *          未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_input_detect_done(sgm41513_dev_t *dev,
                                             bool *done);

/**
 * @brief   降压(buck)变换器轻载 PFM 模式开关
 * @details 默认开 = 轻载效率更高；追求最低纹波时关闭。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能 PFM（默认），false 强制连续导通。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_pfm_enable(sgm41513_dev_t *dev, bool enable);

/**
 * @brief   低 IINDPM 档下仍保持输入 FET（Q1）全开
 * @details 效率更高，电流检测精度略降。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 保持 Q1 全开，false 正常开关。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_q1_fullon(sgm41513_dev_t *dev, bool enable);

/**
 * @brief   屏蔽（true）或放开 VINDPM / IINDPM 的 nINT 脉冲（REG0A）
 * @param   dev           已初始化的设备句柄。
 * @param   mask_vindpm   true 屏蔽 VINDPM 中断。
 * @param   mask_iindpm   true 屏蔽 IINDPM 中断。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_dpm_int_mask(sgm41513_dev_t *dev,
                                            bool mask_vindpm,
                                            bool mask_iindpm);

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 4. OTG 反向升压（REG01/02/06/0D）
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_OTG
/**
 * @brief   使能 / 关闭 OTG 反向升压(boost)
 * @details 使能时会先自动清除 EN_HIZ 并等待 >= 30 ms（数据手册要求）
 *          再置位 OTG_CONFIG。升压前提：VBAT 高于 MIN_BAT_OTG 门限、
 *          VBUS 低于 VBAT + VSLEEP、TS 处于升压温度窗口内。
 *          OTG 优先于充电；HIZ 优先于 OTG。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能 OTG 升压，false 关闭。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用（内部含毫秒级延时）。
 */
sgm41513_result_t sgm41513_otg_enable(sgm41513_dev_t *dev, bool enable);

/**
 * @brief   设置升压(boost)调节电压（OTG 工作时的 VBUS）
 * @param   dev   已初始化的设备句柄。
 * @param   volt  电压档位，见 sgm41513_boost_volt_t（4.85-5.30 V）。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_boost_voltage(sgm41513_dev_t *dev,
                                             sgm41513_boost_volt_t volt);

/**
 * @brief   设置升压(boost)限流：500 mA 或 1200 mA
 * @param   dev  已初始化的设备句柄。
 * @param   lim  限流档位，见 sgm41513_boost_lim_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_boost_current_limit(sgm41513_dev_t *dev,
                                                   sgm41513_boost_lim_t lim);

/**
 * @brief   设置升压(boost)开关频率：1.5 MHz（默认）或 500 kHz
 * @param   dev   已初始化的设备句柄。
 * @param   freq  频率档位，见 sgm41513_boost_freq_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_boost_freq(sgm41513_dev_t *dev,
                                          sgm41513_boost_freq_t freq);

/**
 * @brief   ITERM 缩放（REG0D OTGF_ITREMR 充电模式语义）
 * @details 与升压频率为同一 bit 的充电模式语义：enable = true 时若
 *          ICHG > 300 mA，编程的 ITERM 按 6 倍缩放（有利于大容量
 *          电芯的终止判定精度）。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能 6 倍缩放，false 按编程值。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_iterm_x6(sgm41513_dev_t *dev, bool enable);
#endif /* SGM41513_USE_OTG */

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 5. 状态与故障（REG08/09/0A）
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读取运行状态（REG08）
 * @details 输入源类型（按变体解码）、充电阶段、电源正常(power good)、
 *          热调节、VSYS_MIN 调节。
 * @param   dev     已初始化的设备句柄。
 * @param   status  输出：解码后的运行状态。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM status
 *          为空；SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY
 *          未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_status(sgm41513_dev_t *dev,
                                      sgm41513_status_t *status);

/**
 * @brief   读取实时故障标志（REG09）
 * @details 硬件将故障位锁存直到被读取；因此本函数连续读取 REG09 两次
 *          并解码第二次的值，保证返回的是当前故障。NTC_FAULT 为实时
 *          值，直接采用。读取同时清除锁存的历史记录。
 * @param   dev     已初始化的设备句柄。
 * @param   faults  输出：实时故障标志。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM faults
 *          为空；SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY
 *          未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_faults(sgm41513_dev_t *dev,
                                      sgm41513_faults_t *faults);

/**
 * @brief   读取电源路径 / DPM 状态（REG0A）
 * @details VBUS 正常、VINDPM / IINDPM 环路工作中、涓流补充(top-off)
 *          计时中、输入过压事件。
 * @param   dev  已初始化的设备句柄。
 * @param   dpm  输出：DPM 状态。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM dpm
 *          为空；SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY
 *          未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_get_dpm_status(sgm41513_dev_t *dev,
                                          sgm41513_dpm_status_t *dpm);

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 6. JEITA（REG05/07/0C）
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_JEITA
/**
 * @brief   编程完整 JEITA 温度曲线（TS 引脚 NTC 分压）
 * @details T1..T4 硬件窗口（0..60 C）之外充电暂停；本调用配置冷区
 *          （T2）与暖区（T3）内部阈值，以及区间内施加的电压 / 电流
 *          降额。
 * @param   dev  已初始化的设备句柄。
 * @param   cfg  JEITA 配置，见 sgm41513_jeita_cfg_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM cfg
 *          为空；SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY
 *          未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_jeita_configure(sgm41513_dev_t *dev,
                                           const sgm41513_jeita_cfg_t *cfg);
#endif /* SGM41513_USE_JEITA */

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 7. 看门狗 / 计时器 / 热（REG05/07）
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   设置 I2C 看门狗周期：关闭、40 s、80 s 或 160 s（POR 默认）
 * @param   dev  已初始化的设备句柄。
 * @param   wdt  看门狗周期，见 sgm41513_watchdog_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_watchdog(sgm41513_dev_t *dev,
                                        sgm41513_watchdog_t wdt);

/**
 * @brief   喂看门狗（REG01 WD_RST，写 1 自清零）
 * @param   dev  已初始化的设备句柄。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_feed_watchdog(sgm41513_dev_t *dev);

/**
 * @brief   设置充电安全计时器：7 h 或 16 h，使能 / 关闭
 * @details TMR2X 置位时，DPM / JEITA 冷区 / 热调节期间计时器以半速
 *          运行（见 sgm41513_set_safety_timer_slow2x）。
 * @param   dev     已初始化的设备句柄。
 * @param   hours   计时时长，见 sgm41513_safety_timer_t。
 * @param   enable  true 使能计时器，false 关闭。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_safety_timer(sgm41513_dev_t *dev,
                                            sgm41513_safety_timer_t hours,
                                            bool enable);

/**
 * @brief   使能 / 关闭安全计时器半速运行（REG07 TMR2X_EN）
 * @details 置位后 DPM / JEITA 冷区 / 热调节期间计时器以半速运行。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能半速，false 正常计时。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_safety_timer_slow2x(sgm41513_dev_t *dev,
                                                   bool enable);

/**
 * @brief   设置降压(buck)热调节阈值：80 C 或 120 C
 * @param   dev   已初始化的设备句柄。
 * @param   treg  阈值档位，见 sgm41513_treg_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_thermal_reg_threshold(sgm41513_dev_t *dev,
                                                     sgm41513_treg_t treg);

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 8. 船运模式 / BATFET（REG07）
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_SHIP
/**
 * @brief   进入船运模式(ship mode)（BATFET 断开，电池漏流约 2.5 uA）
 * @details 'delayed' = true 时先等待 tSM_DLY（约 12 s）再断开电池。
 *          退出方式：施加 VBUS、nQON 拉低 >= 1 s、REG_RST、或清除
 *          BATFET_DIS（驱动的 sgm41513_restore_settings() 会重放你的
 *          配置并清除船运模式退出时强制置位的 HIZ 状态）。
 * @param   dev      已初始化的设备句柄。
 * @param   delayed  true 延迟 tSM_DLY 后断开，false 立即断开。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_enter_ship_mode(sgm41513_dev_t *dev,
                                           bool delayed);

/**
 * @brief   使能 / 关闭 nQON 长按（>= 10 s）BATFET 复位功能
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能该功能，false 关闭。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_batfet_reset_enable(sgm41513_dev_t *dev,
                                                   bool enable);
#endif /* SGM41513_USE_SHIP */

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 9. PUMPX（REG0D）
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_PUMPX
/**
 * @brief   使能 / 关闭 PUMPX 电压步进协议
 * @details 面向可调适配器（如部分 QC 类适配器）的协议：在 VBUS 上
 *          产生电流脉冲，请求适配器升高(up)或降低(down)输出电压。
 *          先使能，再触发 sgm41513_pumpx_trigger_up() /
 *          sgm41513_pumpx_trigger_down()；用 sgm41513_pumpx_busy()
 *          等待完成。
 * @param   dev      已初始化的设备句柄。
 * @param   enable   true 使能 PUMPX，false 关闭。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_pumpx_enable(sgm41513_dev_t *dev, bool enable);

/**
 * @brief   触发一次 PUMPX 请求适配器升压(up)脉冲
 * @param   dev  已初始化的设备句柄。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_pumpx_trigger_up(sgm41513_dev_t *dev);

/**
 * @brief   触发一次 PUMPX 请求适配器降压(down)脉冲
 * @param   dev  已初始化的设备句柄。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_pumpx_trigger_down(sgm41513_dev_t *dev);

/**
 * @brief   查询 PUMPX 脉冲序列是否仍在执行
 * @details 脉冲序列完成后 PUMPX_UP/DN 位自清零，*busy 变为 false。
 * @param   dev   已初始化的设备句柄。
 * @param   busy  输出：true 表示脉冲序列执行中。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM busy
 *          为空；SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY
 *          未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_pumpx_busy(sgm41513_dev_t *dev, bool *busy);
#endif /* SGM41513_USE_PUMPX */

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 10. STAT 引脚 / D+ D- 线（REG00/0D/0F）
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   设置 STAT 开漏输出功能（驱动 LED 或接主机 GPIO）
 * @param   dev   已初始化的设备句柄。
 * @param   mode  STAT 引脚模式，见 sgm41513_stat_pin_mode_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_stat_pin_mode(sgm41513_dev_t *dev,
                                             sgm41513_stat_pin_mode_t mode);

/**
 * @brief   设置手动模式下 STAT 引脚的输出图案
 * @param   dev  已初始化的设备句柄。
 * @param   p    图案，见 sgm41513_stat_pattern_t（灭/亮/两种闪烁）。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_stat_pin_pattern(sgm41513_dev_t *dev,
                                                sgm41513_stat_pattern_t p);

/**
 * @brief   手动驱动 D+ / D- 电压（A/D 变体）
 * @details 例如用于分压型适配器检测。VBUS 插入后自动复位；仅在输入
 *          检测完成后有效。
 * @param   dev  已初始化的设备句柄。
 * @param   dp   D+ 电平档位，见 sgm41513_dpdm_vset_t。
 * @param   dm   D- 电平档位，见 sgm41513_dpdm_vset_t。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_set_dpdm_voltage(sgm41513_dev_t *dev,
                                            sgm41513_dpdm_vset_t dp,
                                            sgm41513_dpdm_vset_t dm);

/* ═════════════════════════════════════════════════════════════════════════════
 * API —— 11. 寄存器级访问
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   调试 / 访问未封装功能用的裸寄存器读取
 * @param   dev  已初始化的设备句柄。
 * @param   reg  寄存器地址（0x00..0x0F）。
 * @param   val  输出：读到的寄存器值。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_PARAM val
 *          为空；SGM41513_ERR_IO 通信失败；SGM41513_ERR_NOT_READY
 *          未初始化。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_read_reg(sgm41513_dev_t *dev, uint8_t reg,
                                    uint8_t *val);

/**
 * @brief   调试 / 访问未封装功能用的裸寄存器写入
 * @details 不屏蔽保留位；写后副作用（影子缓存、可选校验）与封装
 *          API 一致。
 * @param   dev  已初始化的设备句柄。
 * @param   reg  寄存器地址。
 * @param   val  待写入的寄存器值。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化；SGM41513_ERR_VERIFY 回读
 *          校验不一致（SGM41513_VERIFY_WRITES=1 时）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_write_reg(sgm41513_dev_t *dev, uint8_t reg,
                                     uint8_t val);

/**
 * @brief   以 (val & mask) 读-改-写指定寄存器的部分位
 * @param   dev   已初始化的设备句柄。
 * @param   reg   寄存器地址。
 * @param   mask  位掩码：仅 mask 中的位被更新。
 * @param   val   新位值。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_NOT_READY 未初始化；SGM41513_ERR_VERIFY 回读
 *          校验不一致（SGM41513_VERIFY_WRITES=1 时）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
sgm41513_result_t sgm41513_update_bits(sgm41513_dev_t *dev, uint8_t reg,
                                       uint8_t mask, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_H */
