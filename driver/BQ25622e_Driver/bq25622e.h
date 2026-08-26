/**
 * @file    bq25622e.h
 * @brief   TI BQ25622E 单节锂电池充电器通用驱动公共 API
 * @details BQ25622E 为 I2C 控制的单节 3A 降压充电器，带 NVDC 电源路径
 *          管理、12 位 ADC 与丰富 JEITA/NTC 保护，无 OTG 升压（区别于
 *          BQ25620/22）。输入 3.9~18V，充电电压 3.5~4.8V，I2C 7 位地址
 *          0x6B。本驱动为纯 C99 可移植实现：无动态内存、无递归，所有
 *          器件实例由调用方静态/栈上分配。
 *
 *          架构（分层设计）：
 *
 *           +--------------------------------------+
 *           |  应用层                              |   你的代码
 *           +--------------------------------------+
 *                     |  本 API（bq25622e.h）
 *           +--------------------------------------+
 *           |  驱动核心（bq25622e.c）              |   可移植纯 C99，
 *           |  配置裁剪（bq25622e_conf.h）         |   无动态内存
 *           +--------------------------------------+
 *                     |  移植层接口（bq25622e_io.h）
 *           +--------------------------------------+
 *           |  移植层（你提供）                    |   STM32 / ESP-IDF /
 *           |                                      |   RTOS / 软件模拟 I2C …
 *           +--------------------------------------+
 *
 *          移植 = 实现 bq25622e_io.h 的 4 个函数并按需调整
 *          bq25622e_conf.h，驱动其余部分不接触任何硬件。
 *
 *          全 API 的单位约定：电流 mA、电压 mV（uint32_t）；ADC 读数中
 *          温度为 0.1 摄氏度单位（int32_t）、TS 百分比为 0.01% 单位
 *          （uint16_t）。所有 set 函数就近取整到硬件档位并钳位到支持
 *          范围；配套 get 函数返回芯片内实际生效值。
 * @note    上电后芯片处于默认模式并以 POR 参数自动充电（EN_CHG=1）；
 *          任意 I2C 写操作即进入 Host 模式并启动看门狗（默认 50s），
 *          看门狗超时会把 R/W 寄存器复位为 POR 且 ICHG 减半——初始化后
 *          请尽快配置看门狗策略（喂狗或禁用），详见 README。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-21
 */

#ifndef BQ25622E_H
#define BQ25622E_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "bq25622e_conf.h"
#include "bq25622e_regs.h"   /* 寄存器地址与位域（同时供高级用户直接访问） */

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════
 *  结果码（全 API 统一返回）
 * ═══════════════════════════════════════════════════ */

typedef enum
{
    BQ25622E_OK = 0,             /**< 无错误 */
    BQ25622E_ERR_IO,             /**< I2C 通信失败（移植层返回错误） */
    BQ25622E_ERR_PARAM,          /**< 参数非法（空指针 / 越界枚举等） */
    BQ25622E_ERR_NOT_READY,      /**< 芯片无应答 / PN 校验不符 */
    BQ25622E_ERR_NOT_SUPPORTED,  /**< 功能在 bq25622e_conf.h 中被裁剪 */
    BQ25622E_ERR_VERIFY,         /**< 写后读回不一致（BQ25622E_VERIFY_WRITES） */
} bq25622e_result_t;

/* ═══════════════════════════════════════════════════
 *  器件句柄（每片芯片一个实例）
 * ═══════════════════════════════════════════════════ */

typedef struct
{
    void *io_ctx;    /**< 不透明指针，透传给移植层 io 函数，多总线时作
                           总线句柄，单总线传 NULL */
    uint8_t dev_addr;/**< 7 位 I2C 地址，通常取 BQ25622E_I2C_ADDR */
#if BQ25622E_REG_SHADOW
    uint16_t shadow[BQ25622E_NUM_RW_REGS]; /**< R/W 寄存器映像缓存 */
#endif
    uint8_t inited;  /**< 由 bq25622e_init() 置位 */
} bq25622e_dev_t;

/* ═══════════════════════════════════════════════════
 *  器件识别
 * ═══════════════════════════════════════════════════ */

typedef struct
{
    uint8_t pn;       /**< PN[5:3] 原始值，BQ25622E 恒为 3h */
    uint8_t dev_rev;  /**< DEV_REV[2:0] 硅片版本 */
} bq25622e_part_info_t;

/* ═══════════════════════════════════════════════════
 *  状态类型（REG1D / REG1E，实时值）
 * ═══════════════════════════════════════════════════ */

/** 充电阶段，解码自 CHG_STAT[1:0] */
typedef enum
{
    BQ25622E_CHG_STAT_OFF = 0,        /**< 00: 未充电或已终止 */
    BQ25622E_CHG_STAT_CC = 1,         /**< 01: 涓流 / 预充 / 快充恒流段 */
    BQ25622E_CHG_STAT_CV = 2,         /**< 10: 恒压段（电流 taper） */
    BQ25622E_CHG_STAT_TOPOFF = 3,     /**< 11: 补充充电进行中 */
} bq25622e_charge_status_t;

/** 输入源类型，解码自 VBUS_STAT[2:0]。手册仅定义两种编码，其余归保留值 */
typedef enum
{
    BQ25622E_VBUS_NONE = 0,           /**< 000: 无 VBUS 输入 */
    BQ25622E_VBUS_UNKNOWN = 4,        /**< 100: 未知适配器（用默认 IINDPM） */
    BQ25622E_VBUS_RESERVED = 8,       /**< 其余编码（手册未定义） */
} bq25622e_vbus_type_t;

typedef struct
{
    bq25622e_vbus_type_t vbus_type;   /**< 解码后的输入源类型 */
    uint8_t vbus_raw;                 /**< VBUS_STAT[2:0] 原始编码 */
    bq25622e_charge_status_t charge_status; /**< 充电阶段 */
    bool adc_done;                    /**< ADC_DONE_STAT：一轮转换完成 */
    bool thermal_regulating;          /**< TREG_STAT：结温热调节中 */
    bool vsys_regulating;             /**< VSYS_STAT：VSYSMIN 稳压中 */
    bool iindpm_active;               /**< IINDPM_STAT：输入限流生效 */
    bool vindpm_active;               /**< VINDPM_STAT：输入限压生效 */
    bool safety_timer_expired;        /**< SAFETY_TMR_STAT：安全定时器超时 */
    bool watchdog_expired;            /**< WD_STAT：看门狗超时 */
} bq25622e_status_t;

/* ═══════════════════════════════════════════════════
 *  故障类型（REG1F，实时值）
 * ═══════════════════════════════════════════════════ */

/** TS 温度区，解码自 TS_STAT[2:0] */
typedef enum
{
    BQ25622E_TS_NORMAL = 0,           /**< 000: 温度正常 */
    BQ25622E_TS_COLD = 1,             /**< 001: 过冷（或 TS 偏置轨不可用） */
    BQ25622E_TS_HOT = 2,              /**< 010: 过热，充电暂停 */
    BQ25622E_TS_COOL = 3,             /**< 011: 冷，降流充电 */
    BQ25622E_TS_WARM = 4,             /**< 100: 热，降流降压充电 */
    BQ25622E_TS_PRECOOL = 5,          /**< 101: 预冷区 */
    BQ25622E_TS_PREWARM = 6,          /**< 110: 预热区 */
    BQ25622E_TS_BIAS_FAULT = 7,       /**< 111: TS 引脚偏置参考故障 */
} bq25622e_ts_stat_t;

typedef struct
{
    bool vbus_fault;                  /**< VBUS OVP 或睡眠比较器致不开关 */
    bool bat_fault;                   /**< 电池 OVP / 放电过流 */
    bool sys_fault;                   /**< VSYS 短路 / 过压 */
    bool tshut;                       /**< 热关断（140 摄氏度触发） */
    bq25622e_ts_stat_t ts_stat;       /**< TS 温度区 */
    uint8_t raw;                      /**< REG1F 原始字节 */
} bq25622e_faults_t;

/* ═══════════════════════════════════════════════════
 *  中断标志（REG20/21/22，锁存读清零）
 * ═══════════════════════════════════════════════════ */

typedef struct
{
    bool adc_done;                    /**< 一轮 ADC 转换完成 */
    bool treg;                        /**< 进入/退出结温热调节 */
    bool vsys;                        /**< 进入/退出 VSYSMIN 稳压 */
    bool iindpm;                      /**< 进入/退出输入限流 */
    bool vindpm;                      /**< 进入/退出输入限压 */
    bool safety_timer;                /**< 安全定时器超时 */
    bool watchdog;                    /**< 看门狗超时 */
    bool chg_stat_change;             /**< 充电状态跳变 */
    bool vbus_stat_change;            /**< VBUS 状态跳变（插拔适配器） */
    bool vbus_fault;                  /**< VBUS 故障置位 */
    bool bat_fault;                   /**< 电池故障置位 */
    bool sys_fault;                   /**< 系统故障置位 */
    bool tshut;                       /**< 热关断置位 */
    bool ts_change;                   /**< TS 温度区跳变 */
} bq25622e_flags_t;

/* ═══════════════════════════════════════════════════
 *  INT 中断掩码（REG23/24/25，true = 屏蔽该事件脉冲）
 * ═══════════════════════════════════════════════════ */

typedef struct
{
    bool adc_done;                    /**< 屏蔽 ADC 完成脉冲 */
    bool treg;                        /**< 屏蔽热调节脉冲 */
    bool vsys;                        /**< 屏蔽 VSYS 稳压脉冲 */
    bool iindpm;                      /**< 屏蔽输入限流脉冲 */
    bool vindpm;                      /**< 屏蔽输入限压脉冲 */
    bool safety_timer;                /**< 屏蔽安全定时器脉冲 */
    bool watchdog;                    /**< 屏蔽看门狗脉冲 */
    bool chg_stat_change;             /**< 屏蔽充电状态跳变脉冲 */
    bool vbus_stat_change;            /**< 屏蔽 VBUS 跳变脉冲 */
    bool vbus_fault;                  /**< 屏蔽 VBUS 故障脉冲 */
    bool bat_fault;                   /**< 屏蔽电池故障脉冲 */
    bool sys_fault;                   /**< 屏蔽系统故障脉冲 */
    bool tshut;                       /**< 屏蔽热关断脉冲 */
    bool ts_change;                   /**< 屏蔽 TS 区跳变脉冲 */
} bq25622e_int_mask_t;

/* ═══════════════════════════════════════════════════
 *  设置枚举（值即寄存器编码）
 * ═══════════════════════════════════════════════════ */

/** 看门狗超时周期（REG16 WATCHDOG[1:0]） */
typedef enum
{
    BQ25622E_WDT_DISABLE = 0,         /**< 禁用看门狗 */
    BQ25622E_WDT_50S = 1,             /**< 50 秒（POR 默认） */
    BQ25622E_WDT_100S = 2,            /**< 100 秒 */
    BQ25622E_WDT_200S = 3,            /**< 200 秒 */
} bq25622e_watchdog_t;

/** 再充电阈值（低于 VREG 多少毫伏触发再充电，REG14 VRECHG） */
typedef enum
{
    BQ25622E_VRECHG_100MV = 0,        /**< 100 mV（POR 默认） */
    BQ25622E_VRECHG_200MV = 1,        /**< 200 mV */
} bq25622e_vrechg_t;

/** 涓流充电电流（电池短路恢复初期，REG14 ITRICKLE） */
typedef enum
{
    BQ25622E_TRICKLE_20MA = 0,        /**< 20 mA（POR 默认） */
    BQ25622E_TRICKLE_80MA = 1,        /**< 80 mA */
} bq25622e_trickle_t;

/** 补充充电（top-off）定时器时长（REG14 TOPOFF_TMR[1:0]） */
typedef enum
{
    BQ25622E_TOPOFF_DISABLE = 0,      /**< 禁用（POR 默认） */
    BQ25622E_TOPOFF_17MIN = 1,        /**< 17 分钟 */
    BQ25622E_TOPOFF_35MIN = 2,        /**< 35 分钟 */
    BQ25622E_TOPOFF_52MIN = 3,        /**< 52 分钟 */
} bq25622e_topoff_t;

/** 快充安全定时器时长（REG15 CHG_TMR） */
typedef enum
{
    BQ25622E_CHG_TMR_14_5H = 0,       /**< 14.5 小时（POR 默认） */
    BQ25622E_CHG_TMR_28H = 1,         /**< 28 小时 */
} bq25622e_chg_timer_t;

/** 预充安全定时器时长（REG15 PRECHG_TMR） */
typedef enum
{
    BQ25622E_PRECHG_TMR_2_5H = 0,     /**< 2.5 小时（POR 默认） */
    BQ25622E_PRECHG_TMR_0_62H = 1,    /**< 0.62 小时 */
} bq25622e_prechg_timer_t;

/** 结温热调节阈值（REG17 TREG） */
typedef enum
{
    BQ25622E_TREG_60C = 0,            /**< 60 摄氏度 */
    BQ25622E_TREG_120C = 1,           /**< 120 摄氏度（POR 默认） */
} bq25622e_treg_t;

/** 开关频率（REG17 SET_CONV_FREQ[1:0]，编码非单调） */
typedef enum
{
    BQ25622E_FREQ_1_50MHZ = 0,        /**< 1.5 MHz（POR 默认） */
    BQ25622E_FREQ_1_35MHZ = 1,        /**< 1.35 MHz */
    BQ25622E_FREQ_1_65MHZ = 2,        /**< 1.65 MHz */
} bq25622e_conv_freq_t;

/** VBUS 过压保护阈值（REG17 VBUS_OVP） */
typedef enum
{
    BQ25622E_VBUS_OVP_6_3V = 0,       /**< 6.3 V（用于低压适配器场景） */
    BQ25622E_VBUS_OVP_18_5V = 1,      /**< 18.5 V（POR 默认） */
} bq25622e_vbus_ovp_t;

/** 快充电流折返倍率分母（REG19 CHG_RATE[1:0]，热调节/DPM 时限流用） */
typedef enum
{
    BQ25622E_CHG_RATE_1C = 0,         /**< 1C（POR 默认） */
    BQ25622E_CHG_RATE_2C = 1,         /**< 2C */
    BQ25622E_CHG_RATE_4C = 2,         /**< 4C */
    BQ25622E_CHG_RATE_6C = 3,         /**< 6C */
} bq25622e_chg_rate_t;

/** BATFET 工作模式（REG18 BATFET_CTRL[1:0]） */
typedef enum
{
    BQ25622E_BATFET_NORMAL = 0,       /**< 正常导通 */
    BQ25622E_BATFET_SHUTDOWN = 1,     /**< 关断模式（系统掉电，可唤醒） */
    BQ25622E_BATFET_SHIP = 2,         /**< 船运模式（最低功耗存储运输） */
    BQ25622E_BATFET_SYS_RST = 3,      /**< 系统电源复位（短暂断电重启） */
} bq25622e_batfet_mode_t;

/** TS 温度区充电电流动作（REG1A/1C 各区共用编码） */
typedef enum
{
    BQ25622E_TS_CURR_PAUSE = 0,       /**< 暂停充电 */
    BQ25622E_TS_CURR_20PCT = 1,       /**< 降为 ICHG 的 20% */
    BQ25622E_TS_CURR_40PCT = 2,       /**< 降为 ICHG 的 40% */
    BQ25622E_TS_CURR_KEEP = 3,        /**< 保持不变 */
} bq25622e_ts_current_t;

/** TS 高温区充电电压降（REG1B TS_VSET_WARM / REG1C TS_VSET_PREWARM 共用） */
typedef enum
{
    BQ25622E_TS_VDROP_300MV = 0,      /**< VREG - 300 mV */
    BQ25622E_TS_VDROP_200MV = 1,      /**< VREG - 200 mV（WARM 区 POR 默认） */
    BQ25622E_TS_VDROP_100MV = 2,      /**< VREG - 100 mV */
    BQ25622E_TS_VDROP_KEEP = 3,       /**< 保持不变（PREWARM 区 POR 默认） */
} bq25622e_ts_vdrop_t;

/* ═══════════════════════════════════════════════════
 *  NTC / JEITA 配置
 * ═══════════════════════════════════════════════════ */

/**
 * NTC 一次性配置（写入 REG1A/1B/1C 三个寄存器）。
 *
 * 冷端阈值组合 cold_thresholds 写入 TS_TH1_TH2_TH3[2:0]、热端阈值组合
 * hot_thresholds 写入 TS_TH4_TH5_TH6[2:0]，均为手册 7.5 节查表编码
 * （基于 103AT 热敏电阻，RT1=5.24k / RT2=30.31k）。常用组合：
 *   冷端 000 = TH1/TH2/TH3 = 0/5/15 C     001 = 0/10/15 C（POR 默认）
 *         010 = 0/15/20 C                 100 = -5/5/15 C
 *   热端 000 = TH4/TH5/TH6 = 35/40/60 C   001 = 35/45/60 C（POR 默认）
 *         010 = 35/50/60 C                111 = 40/50/60 C
 * 其余组合请查阅数据手册 7.5 节完整表格。
 */
typedef struct
{
    bq25622e_ts_current_t cool_current;     /**< TS_COOL 区电流动作 */
    bq25622e_ts_current_t warm_current;     /**< TS_WARM 区电流动作 */
    bq25622e_ts_current_t precool_current;  /**< TS_PRECOOL 区电流动作 */
    bq25622e_ts_current_t prewarm_current;  /**< TS_PREWARM 区电流动作 */
    bq25622e_ts_vdrop_t warm_vdrop;         /**< TS_WARM 区电压降 */
    bq25622e_ts_vdrop_t prewarm_vdrop;      /**< TS_PREWARM 区电压降 */
    uint8_t cold_thresholds;                /**< TS_TH1_TH2_TH3 编码 0~7 */
    uint8_t hot_thresholds;                 /**< TS_TH4_TH5_TH6 编码 0~7 */
    bool sym_mode;                          /**< true: PRECOOL/COOL 复用高温
                                                 区电压设置（TS_VSET_SYM） */
} bq25622e_ntc_cfg_t;

/* ═══════════════════════════════════════════════════
 *  ADC 配置（BQ25622E_USE_ADC = 1 时编译）
 * ═══════════════════════════════════════════════════ */

/** ADC 采样分辨率与单通道转换时间（REG26 ADC_SAMPLE[1:0]） */
typedef enum
{
    BQ25622E_ADC_SAMPLE_12BIT = 0,    /**< 12 位有效分辨率，30 ms/通道 */
    BQ25622E_ADC_SAMPLE_11BIT = 1,    /**< 11 位，15 ms/通道 */
    BQ25622E_ADC_SAMPLE_10BIT = 2,    /**< 10 位，7.5 ms/通道 */
    BQ25622E_ADC_SAMPLE_9BIT = 3,     /**< 9 位，3.75 ms/通道（POR 默认） */
} bq25622e_adc_sample_t;

/** ADC 通道禁用掩码（供 bq25622e_adc_set_channel_disable() 按位或组合） */
#define BQ25622E_ADC_DIS_IBUS    BQ25622E_IBUS_ADC_DIS
#define BQ25622E_ADC_DIS_IBAT    BQ25622E_IBAT_ADC_DIS
#define BQ25622E_ADC_DIS_VBUS    BQ25622E_VBUS_ADC_DIS
#define BQ25622E_ADC_DIS_VBAT    BQ25622E_VBAT_ADC_DIS
#define BQ25622E_ADC_DIS_VSYS    BQ25622E_VSYS_ADC_DIS
#define BQ25622E_ADC_DIS_TS      BQ25622E_TS_ADC_DIS
#define BQ25622E_ADC_DIS_TDIE    BQ25622E_TDIE_ADC_DIS
#define BQ25622E_ADC_DIS_VPMID   BQ25622E_VPMID_ADC_DIS

/* ═══════════════════════════════════════════════════
 *  1. 初始化与器件识别
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   初始化器件实例并校验芯片身份
 * @details 填充句柄、调用移植层一次性初始化，然后读 REG38 校验
 *          PN[5:3] == 3h（BQ25622E）。影子缓存使能时同步快照全部
 *          R/W 寄存器。注意本函数不修改任何芯片设置——上电后芯片
 *          已按 POR 参数自动充电，请随后配置看门狗与充电参数。
 * @param   dev      器件句柄（调用方分配），不得为 NULL
 * @param   io_ctx   不透明总线句柄，单总线系统传 NULL
 * @param   dev_addr 7 位 I2C 地址，传 0 自动使用 BQ25622E_I2C_ADDR
 * @retval  BQ25622E_OK 成功
 * @retval  BQ25622E_ERR_PARAM 句柄为空
 * @retval  BQ25622E_ERR_IO 移植层初始化或通信失败
 * @retval  BQ25622E_ERR_NOT_READY PN 校验不符（不是 BQ25622E）
 */
bq25622e_result_t bq25622e_init(bq25622e_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr);

/**
 * @brief   读取器件信息（PN 与硅片版本）
 * @param   dev  器件句柄
 * @param   info 输出：器件信息
 * @retval  BQ25622E_OK 成功
 * @retval  BQ25622E_ERR_PARAM 参数为空 / 未初始化
 * @retval  BQ25622E_ERR_IO 通信失败
 */
bq25622e_result_t bq25622e_get_part_info(bq25622e_dev_t *dev,
                                         bq25622e_part_info_t *info);

/**
 * @brief   软复位芯片（写 REG17 REG_RST，完成后自动回 0）
 * @details 全部可复位寄存器回到 POR 值；影子缓存使能时缓存同步重置
 *          为 POR 映像。复位后需重新配置充电参数。
 * @param   dev 器件句柄
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_reset(bq25622e_dev_t *dev);

#if BQ25622E_REG_SHADOW
/**
 * @brief   将影子缓存中的全部设置一次性回写芯片
 * @details 用于看门狗超时（R/W 寄存器被复位为 POR 且 ICHG 减半）或
 *          退出船运模式后恢复主机配置。回写顺序与寄存器表一致。
 * @param   dev 器件句柄
 * @retval  BQ25622E_OK 成功
 * @retval  BQ25622E_ERR_IO 任一寄存器回写失败
 */
bq25622e_result_t bq25622e_restore_settings(bq25622e_dev_t *dev);
#endif

/* ═══════════════════════════════════════════════════
 *  2. 充电控制
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   使能/禁止充电（REG16 EN_CHG）
 * @param   dev 器件句柄
 * @param   en  true 使能充电，false 禁止
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_charge_enable(bq25622e_dev_t *dev, bool en);

/**
 * @brief   设置快速充电电流上限（REG02/03 ICHG，80~3040 mA，步进 80 mA）
 * @param   dev 器件句柄
 * @param   ma  目标电流 mA，就近取整到档位并钳位
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_ichg(bq25622e_dev_t *dev, uint32_t ma);

/**
 * @brief   读取当前生效的快速充电电流上限
 * @param   dev 器件句柄
 * @param   ma  输出：实际生效电流 mA
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_ichg(bq25622e_dev_t *dev, uint32_t *ma);

/**
 * @brief   设置充电截止电压 VREG（REG04/05，3500~4800 mV，步进 10 mV）
 * @param   dev 器件句柄
 * @param   mv  目标电压 mV，就近取整到档位并钳位
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_vreg(bq25622e_dev_t *dev, uint32_t mv);

/**
 * @brief   读取当前生效的充电截止电压
 * @param   dev 器件句柄
 * @param   mv  输出：实际生效电压 mV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_vreg(bq25622e_dev_t *dev, uint32_t *mv);

/**
 * @brief   设置预充电流（REG10/11 IPRECHG，20~620 mA，步进 20 mA）
 * @param   dev 器件句柄
 * @param   ma  目标电流 mA，就近取整到档位并钳位
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_precharge_current(bq25622e_dev_t *dev,
                                                 uint32_t ma);

/**
 * @brief   读取当前生效的预充电流
 * @param   dev 器件句柄
 * @param   ma  输出：实际生效电流 mA
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_precharge_current(bq25622e_dev_t *dev,
                                                 uint32_t *ma);

/**
 * @brief   设置充电终止电流（REG12/13 ITERM，10~620 mA，步进 10 mA）
 * @param   dev 器件句柄
 * @param   ma  目标电流 mA，就近取整到档位并钳位
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_term_current(bq25622e_dev_t *dev, uint32_t ma);

/**
 * @brief   读取当前生效的充电终止电流
 * @param   dev 器件句柄
 * @param   ma  输出：实际生效电流 mA
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_term_current(bq25622e_dev_t *dev, uint32_t *ma);

/**
 * @brief   使能/禁止充电终止功能（REG14 EN_TERM）
 * @param   dev 器件句柄
 * @param   en  true 使能终止（POR 默认），false 充满后持续 CV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_term_enable(bq25622e_dev_t *dev, bool en);

/**
 * @brief   设置再充电阈值（REG14 VRECHG）
 * @param   dev      器件句柄
 * @param   threshold 阈值枚举（低于 VREG 100 或 200 mV 触发再充）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_recharge_threshold(bq25622e_dev_t *dev,
                                                  bq25622e_vrechg_t threshold);

/**
 * @brief   设置涓流充电电流（REG14 ITRICKLE）
 * @param   dev 器件句柄
 * @param   curr 电流枚举（20 或 80 mA）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_trickle_current(bq25622e_dev_t *dev,
                                               bq25622e_trickle_t curr);

/**
 * @brief   设置补充充电（top-off）定时器（REG14 TOPOFF_TMR）
 * @param   dev  器件句柄
 * @param   timer 时长枚举（禁用 / 17 / 35 / 52 分钟）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_topoff_timer(bq25622e_dev_t *dev,
                                            bq25622e_topoff_t timer);

/**
 * @brief   设置快充电流折返倍率分母（REG19 CHG_RATE）
 * @details 热调节或 DPM 折返时以 ICHG/倍率为界，1C 表示以设定电流为基准。
 * @param   dev  器件句柄
 * @param   rate 倍率枚举（1C/2C/4C/6C）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_chg_rate(bq25622e_dev_t *dev,
                                        bq25622e_chg_rate_t rate);

/* ═══════════════════════════════════════════════════
 *  3. 输入管理（DPM）
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   设置输入电流限制 IINDPM（REG06/07，100~3200 mA，步进 20 mA）
 * @note    适配器拔出再插入后此设置自动回到 POR 值 3200 mA，需重新下发。
 * @param   dev 器件句柄
 * @param   ma  目标电流 mA，就近取整到档位并钳位
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_iindpm(bq25622e_dev_t *dev, uint32_t ma);

/**
 * @brief   读取当前生效的输入电流限制
 * @param   dev 器件句柄
 * @param   ma  输出：实际生效电流 mA
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_iindpm(bq25622e_dev_t *dev, uint32_t *ma);

/**
 * @brief   设置输入电压限制 VINDPM（REG08/09，3800~16800 mV，步进 40 mV）
 * @param   dev 器件句柄
 * @param   mv  目标电压 mV，就近取整到档位并钳位
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_vindpm(bq25622e_dev_t *dev, uint32_t mv);

/**
 * @brief   读取当前生效的输入电压限制
 * @param   dev 器件句柄
 * @param   mv  输出：实际生效电压 mV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_vindpm(bq25622e_dev_t *dev, uint32_t *mv);

/**
 * @brief   使能/禁止 VINDPM 电池跟踪（REG14 VINDPM_BAT_TRACK）
 * @details 使能时实际 VINDPM 取 max(寄存器值, VBAT + 400 mV)，防止轻载
 *          下适配器被误判拔出（POR 默认使能）。
 * @param   dev 器件句柄
 * @param   en  true 使能跟踪
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_vindpm_bat_track(bq25622e_dev_t *dev, bool en);

/**
 * @brief   进入/退出 HIZ 输入高阻模式（REG16 EN_HIZ）
 * @details HIZ 下 Q1 关断、VBUS 与变换器断开，仅剩 LDO 供电。
 * @param   dev 器件句柄
 * @param   en  true 进入 HIZ
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_hiz(bq25622e_dev_t *dev, bool en);

/**
 * @brief   强制 Q1 低阻模式（REG14 Q1_FULLON，RBFET 固定 26 毫欧）
 * @param   dev 器件句柄
 * @param   en  true 强制低阻
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_q1_fullon(bq25622e_dev_t *dev, bool en);

/**
 * @brief   强制 BATFET 低阻模式（REG14 Q4_FULLON，固定 15 毫欧）
 * @note    仅 VBAT > VSYSMIN 时生效；同时 ICHG/IPRECHG/ITERM 最小档抬升。
 * @param   dev 器件句柄
 * @param   en  true 强制低阻
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_q4_fullon(bq25622e_dev_t *dev, bool en);

/**
 * @brief   设置最小系统电压 VSYSMIN（REG0E/0F，2560~3840 mV，步进 80 mV）
 * @param   dev 器件句柄
 * @param   mv  目标电压 mV，就近取整到档位并钳位
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_sys_min_voltage(bq25622e_dev_t *dev,
                                               uint32_t mv);

/**
 * @brief   读取当前生效的最小系统电压
 * @param   dev 器件句柄
 * @param   mv  输出：实际生效电压 mV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_sys_min_voltage(bq25622e_dev_t *dev,
                                               uint32_t *mv);

/**
 * @brief   使能 ILIM 引脚外部输入限流（REG19 EN_EXTILIM）
 * @param   dev 器件句柄
 * @param   en  true 使能（限流值 = 2500 A·Ω / R_ILIM）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_extilim_enable(bq25622e_dev_t *dev, bool en);

/**
 * @brief   设置 VBUS 过压保护阈值（REG17 VBUS_OVP）
 * @param   dev 器件句柄
 * @param   ovp 阈值枚举（6.3 V 或 18.5 V）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_vbus_ovp(bq25622e_dev_t *dev,
                                        bq25622e_vbus_ovp_t ovp);

/**
 * @brief   设置开关频率（REG17 SET_CONV_FREQ）
 * @param   dev  器件句柄
 * @param   freq 频率枚举（1.35 / 1.5 / 1.65 MHz）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_conv_freq(bq25622e_dev_t *dev,
                                         bq25622e_conv_freq_t freq);

/* ═══════════════════════════════════════════════════
 *  4. NTC / JEITA 温度管理
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   忽略 TS 反馈强制允许充电（REG1A TS_IGNORE）
 * @warning 仅用于无电池 NTC 的调试场景！量产禁用将失去温度保护。
 * @param   dev 器件句柄
 * @param   en  true 忽略 TS
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_ts_ignore(bq25622e_dev_t *dev, bool en);

/**
 * @brief   一次性配置 NTC/JEITA 全部参数（写 REG1A/1B/1C）
 * @param   dev 器件句柄
 * @param   cfg 配置结构体，不得为 NULL
 * @retval  BQ25622E_OK 成功
 * @retval  BQ25622E_ERR_PARAM cfg 为空或阈值编码越界
 */
bq25622e_result_t bq25622e_ntc_configure(bq25622e_dev_t *dev,
                                         const bq25622e_ntc_cfg_t *cfg);

/* ═══════════════════════════════════════════════════
 *  5. 看门狗与安全定时器
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   设置看门狗超时周期（REG16 WATCHDOG）
 * @details Host 模式下必须周期性喂狗或禁用看门狗，否则超时后 R/W
 *          寄存器复位为 POR 且 ICHG 减半。
 * @param   dev 器件句柄
 * @param   wdt 周期枚举（禁用 / 50 / 100 / 200 秒）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_watchdog(bq25622e_dev_t *dev,
                                        bq25622e_watchdog_t wdt);

/**
 * @brief   喂看门狗（写 REG16 WD_RST，写 1 后自动回 0）
 * @param   dev 器件句柄
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_feed_watchdog(bq25622e_dev_t *dev);

/**
 * @brief   使能/禁止充电安全定时器（REG15 EN_SAFETY_TMRS）
 * @param   dev 器件句柄
 * @param   en  true 使能（POR 默认）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_safety_timer_enable(bq25622e_dev_t *dev,
                                                   bool en);

/**
 * @brief   设置快充安全定时器时长（REG15 CHG_TMR）
 * @param   dev  器件句柄
 * @param   timer 时长枚举（14.5 或 28 小时）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_chg_safety_timer(bq25622e_dev_t *dev,
                                                bq25622e_chg_timer_t timer);

/**
 * @brief   设置预充安全定时器时长（REG15 PRECHG_TMR）
 * @param   dev  器件句柄
 * @param   timer 时长枚举（2.5 或 0.62 小时）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_prechg_safety_timer(
    bq25622e_dev_t *dev, bq25622e_prechg_timer_t timer);

/**
 * @brief   使能 DPM/热调节期间安全定时器半速计数（REG15 TMR2X_EN）
 * @param   dev 器件句柄
 * @param   en  true 使能（POR 默认）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_timer_2x(bq25622e_dev_t *dev, bool en);

/**
 * @brief   设置结温热调节阈值（REG17 TREG）
 * @param   dev 器件句柄
 * @param   treg 阈值枚举（60 或 120 摄氏度）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_thermal_reg_threshold(bq25622e_dev_t *dev,
                                                     bq25622e_treg_t treg);

/* ═══════════════════════════════════════════════════
 *  6. 状态 / 故障 / 中断标志
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   读取实时充电状态（REG1D + REG1E，解码为结构体）
 * @param   dev    器件句柄
 * @param   status 输出：状态结构体，不得为 NULL
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_status(bq25622e_dev_t *dev,
                                      bq25622e_status_t *status);

/**
 * @brief   读取实时故障状态（REG1F，解码为结构体）
 * @param   dev    器件句柄
 * @param   faults 输出：故障结构体，不得为 NULL
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_faults(bq25622e_dev_t *dev,
                                      bq25622e_faults_t *faults);

/**
 * @brief   读取并清除锁存中断标志（REG20/21/22，读清零）
 * @details INT 引脚输出 256 微秒低脉冲指示事件；本函数读取后对应
 *          标志自动清零，适合在 INT 中断服务中调用一次完成事件确认。
 * @param   dev   器件句柄
 * @param   flags 输出：标志结构体，不得为 NULL
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_flags(bq25622e_dev_t *dev,
                                     bq25622e_flags_t *flags);

/**
 * @brief   配置 INT 中断事件屏蔽（写 REG23/24/25）
 * @param   dev  器件句柄
 * @param   mask 掩码结构体，true = 屏蔽该事件的 INT 脉冲
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_set_int_mask(bq25622e_dev_t *dev,
                                        const bq25622e_int_mask_t *mask);

/**
 * @brief   读取当前 INT 中断事件屏蔽配置（读 REG23/24/25）
 * @param   dev  器件句柄
 * @param   mask 输出：掩码结构体，不得为 NULL
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_get_int_mask(bq25622e_dev_t *dev,
                                        bq25622e_int_mask_t *mask);

/* ═══════════════════════════════════════════════════
 *  7. BATFET 控制（BQ25622E_USE_BATFET = 1 时编译）
 * ═══════════════════════════════════════════════════ */

#if BQ25622E_USE_BATFET

/**
 * @brief   设置 BATFET 工作模式（REG18 BATFET_CTRL）
 * @warning 关断/船运模式会切断电池对系统的供电，整机断电；动作延迟由
 *          bq25622e_batfet_set_delay() 决定（POR 默认 12.5 秒）。
 * @param   dev  器件句柄
 * @param   mode 模式枚举（正常 / 关断 / 船运 / 系统复位）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_batfet_set_mode(bq25622e_dev_t *dev,
                                           bq25622e_batfet_mode_t mode);

/**
 * @brief   设置 BATFET 动作延迟（REG18 BATFET_DLY）
 * @param   dev        器件句柄
 * @param   delay_12_5s true = 12.5 秒（POR 默认），false = 25 毫秒
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_batfet_set_delay(bq25622e_dev_t *dev,
                                            bool delay_12_5s);

/**
 * @brief   允许带适配器时执行 BATFET 关断/复位（REG18 BATFET_CTRL_WVBUS）
 * @param   dev 器件句柄
 * @param   en  true 允许
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_batfet_set_ctrl_wvbus(bq25622e_dev_t *dev, bool en);

#endif /* BQ25622E_USE_BATFET */

/* ═══════════════════════════════════════════════════
 *  8. ADC 测量（BQ25622E_USE_ADC = 1 时编译）
 * ═══════════════════════════════════════════════════ */

#if BQ25622E_USE_ADC

/**
 * @brief   启动/停止连续转换模式（REG26 ADC_EN + ADC_RATE=0）
 * @details 连续模式下各通道结果寄存器持续刷新；需 VBUS > 劣源阈值或
 *          VBAT > UVLO 阈值才真正启动。
 * @param   dev 器件句柄
 * @param   en  true 启动连续转换
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_enable(bq25622e_dev_t *dev, bool en);

/**
 * @brief   触发一轮单次转换（REG26 ADC_EN=1 + ADC_RATE=1）
 * @details 全部使能通道各转一次后 ADC_EN 自动清零，完成标志可用
 *          bq25622e_adc_done() 轮询。9 位采样 8 通道约需 30 ms。
 * @param   dev 器件句柄
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_trigger_once(bq25622e_dev_t *dev);

/**
 * @brief   设置 ADC 采样分辨率（REG26 ADC_SAMPLE）
 * @param   dev    器件句柄
 * @param   sample 分辨率枚举（9~12 位）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_set_sample(bq25622e_dev_t *dev,
                                          bq25622e_adc_sample_t sample);

/**
 * @brief   配置结果滑动平均（REG26 ADC_AVG / ADC_AVG_INIT）
 * @param   dev       器件句柄
 * @param   avg       true 使能滑动平均
 * @param   avg_init  true 从新一轮转换开始平均（false 用现有值起步）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_set_avg(bq25622e_dev_t *dev, bool avg,
                                       bool avg_init);

/**
 * @brief   设置禁用的 ADC 通道掩码（REG27，置 1 的通道停止转换）
 * @param   dev      器件句柄
 * @param   dis_mask BQ25622E_ADC_DIS_xxx 按位或组合，0 = 全通道使能
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_set_channel_disable(bq25622e_dev_t *dev,
                                                   uint8_t dis_mask);

/**
 * @brief   查询一轮转换是否完成（REG1D ADC_DONE_STAT）
 * @param   dev  器件句柄
 * @param   done 输出：true 完成
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_done(bq25622e_dev_t *dev, bool *done);

/**
 * @brief   读取输入电流测量值（IBUS，-4000~+4000 mA，2 mA 分辨率）
 * @param   dev 器件句柄
 * @param   ma  输出：电流 mA，VBUS 流向 PMID 为正
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_ibus(bq25622e_dev_t *dev, int32_t *ma);

/**
 * @brief   读取电池电流测量值（IBAT，-7500~+4000 mA，4 mA 分辨率）
 * @param   dev 器件句柄
 * @param   ma  输出：电流 mA，充电方向为正
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_ibat(bq25622e_dev_t *dev, int32_t *ma);

/**
 * @brief   读取 VBUS 电压测量值（0~18000 mV）
 * @param   dev 器件句柄
 * @param   mv  输出：电压 mV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_vbus(bq25622e_dev_t *dev, uint32_t *mv);

/**
 * @brief   读取 PMID 电压测量值（0~18000 mV）
 * @param   dev 器件句柄
 * @param   mv  输出：电压 mV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_vpmid(bq25622e_dev_t *dev, uint32_t *mv);

/**
 * @brief   读取电池电压测量值（0~5572 mV）
 * @param   dev 器件句柄
 * @param   mv  输出：电压 mV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_vbat(bq25622e_dev_t *dev, uint32_t *mv);

/**
 * @brief   读取系统电压测量值（0~5572 mV）
 * @param   dev 器件句柄
 * @param   mv  输出：电压 mV
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_vsys(bq25622e_dev_t *dev, uint32_t *mv);

/**
 * @brief   读取 TS 引脚电压占偏置基准百分比（0~98.31%）
 * @details 百分比可用于软件侧换算 NTC 温度（配合手册 7.5 节曲线）。
 * @param   dev          器件句柄
 * @param   percent_x100 输出：百分比 × 100（如 7330 表示 73.30%）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_ts(bq25622e_dev_t *dev,
                                       uint16_t *percent_x100);

/**
 * @brief   读取芯片结温（-40~+140 摄氏度，0.5 摄氏度分辨率）
 * @param   dev     器件句柄
 * @param   deg_x10 输出：摄氏度 × 10（如 255 表示 25.5 摄氏度）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_adc_read_tdie(bq25622e_dev_t *dev, int32_t *deg_x10);

#endif /* BQ25622E_USE_ADC */

/* ═══════════════════════════════════════════════════
 *  9. 寄存器级访问（高级用户）
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   读逻辑寄存器（宽度自适应：16 位寄存器单次事务读 2 字节）
 * @param   dev 器件句柄
 * @param   reg 寄存器地址（BQ25622E_REGxx 宏）
 * @param   val 输出：寄存器值（8 位寄存器仅低 8 位有效）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_read_reg(bq25622e_dev_t *dev, uint8_t reg,
                                    uint16_t *val);

/**
 * @brief   写逻辑寄存器（宽度自适应：16 位寄存器单次事务写 2 字节）
 * @param   dev 器件句柄
 * @param   reg 寄存器地址（BQ25622E_REGxx 宏）
 * @param   val 待写值（8 位寄存器仅低 8 位有效）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_write_reg(bq25622e_dev_t *dev, uint8_t reg,
                                     uint16_t val);

/**
 * @brief   读-改-写指定位域（保持其余位不变）
 * @param   dev  器件句柄
 * @param   reg  寄存器地址
 * @param   mask 位域掩码（BQ25622E_XXX_MASK）
 * @param   val  位域新值（未移位前置于低位，内部自动对齐）
 * @retval  BQ25622E_OK 成功
 */
bq25622e_result_t bq25622e_update_bits(bq25622e_dev_t *dev, uint8_t reg,
                                       uint16_t mask, uint16_t val);

#ifdef __cplusplus
}
#endif

#endif /* BQ25622E_H */
