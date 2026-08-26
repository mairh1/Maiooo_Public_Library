/*
 * @file    max17048.h
 * @brief   MAX17048/MAX17049 ModelGauge 电量计通用驱动公共 API
 * @details 器件：MAX17048（1 节锂电）/ MAX17049（2 节锂电），
 *          Analog Devices/Maxim 3µA ModelGauge 电压法电量计，
 *          I2C 接口（7 位地址 0x36，最高 400kHz），16 位寄存器，
 *          TDFN-8 / WLP-8 封装（本库料号 MAX17048G+T10 为 TDFN-8）。
 *
 *          分层架构（详见 README.md）：
 *
 *            +--------------------------------------+
 *            |  应用层                               |  用户代码
 *            +--------------------------------------+
 *                        |  本 API（max17048.h）
 *            +--------------------------------------+
 *            |  驱动核心  max17048.c                  |  纯 C99、零平台代码
 *            |  配置      max17048_conf.h             |  无动态内存
 *            +--------------------------------------+
 *                        |  4~6 个 io 函数（max17048_io.h）
 *            +--------------------------------------+
 *            |  移植层（用户提供）                     |  STM32/CH32/ESP32/
 *            |                                        |  Linux/RTOS/模拟 I2C
 *            +--------------------------------------+
 *
 *          移植 = 实现 max17048_io.h + 按需调整 max17048_conf.h，
 *          核心文件零改动。
 *
 *          单位约定（贯穿全部 API）：电压 mV，SOC %（精确版为 0.01%），
 *          充放电速率 0.1%/h，温度 0.1℃。全部定点运算，无浮点。
 * @note    set 类函数按硬件档位就近取整并钳位到支持范围，对应 get 返回
 *          实际生效值。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 */

#ifndef MAX17048_H
#define MAX17048_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "max17048_conf.h"
#include "max17048_regs.h"  /* 寄存器地址与位定义（也开放寄存器级访问） */

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 结果码（全部公共 API 的统一返回类型）
 * ════════════════════════════════════════════════════════════════════════ */

typedef enum {
    MAX17048_OK = 0,            /**< 成功 */
    MAX17048_ERR_IO,            /**< I2C 通信失败（移植层返回 ERROR 时上抛） */
    MAX17048_ERR_PARAM,         /**< 参数非法（空指针、超范围） */
    MAX17048_ERR_NOT_READY,     /**< 未初始化 / 器件无应答 / 版本不符 */
    MAX17048_ERR_NOT_SUPPORTED, /**< 功能被 conf 裁剪或器件不支持 */
    MAX17048_ERR_VERIFY,        /**< 写后回读不一致（MAX17048_VERIFY_WRITES） */
} max17048_result_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 设备句柄（每片芯片一个，由调用者分配）
 * ════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void    *io_ctx;    /**< 总线上下文，原样透传给 io 函数（多总线设计用，
                             单总线可为 NULL） */
    uint8_t dev_addr;   /**< 7 位 I2C 地址，通常填 MAX17048_I2C_ADDR(0x36) */
    uint8_t inited;     /**< max17048_init() 置 1 */
} max17048_dev_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 状态与标识类型
 * ════════════════════════════════════════════════════════════════════════ */

/** STATUS 寄存器（0x1A）解码结果，告警位写 1 清除 */
typedef struct {
    bool reset_indicator;   /**< RI：上电/复位后置位，模型需重载并清零 */
    bool vhigh;             /**< VH：VCELL 高于 VALRT.MAX */
    bool vlow;              /**< VL：VCELL 低于 VALRT.MIN */
    bool vreset;            /**< VR：发生电压复位（需 STATUS.ENVR=1） */
    bool soc_low;           /**< HD：SOC 跌破 CONFIG.ATHD 阈值 */
    bool soc_change;        /**< SC：SOC 变化 ≥1%（需 CONFIG.ALSC=1） */
    uint16_t raw;           /**< 原始 STATUS 值（含使能位） */
} max17048_status_t;

/** 器件标识信息 */
typedef struct {
    uint16_t version;   /**< VERSION 寄存器原始值（期望 0x001x） */
    uint8_t  id;        /**< VRESET/ID 低字节，工厂一次性烧写 OTP */
} max17048_id_t;

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 1. 初始化 / 复位 / 标识
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   初始化设备句柄并确认器件在位
 * @details 调用 max17048_io_init() 一次；读 VERSION 校验高 12 位为
 *          0x001；读 STATUS，若 RI 置位则清除（表示此前发生过 POR，
 *          使用自定义模型的应用需随后调用 max17048_model_load()，
 *          ROM 默认模型无需额外动作）。
 * @param   dev       设备句柄，调用者分配。
 * @param   io_ctx    总线上下文，透传给 io 函数，可为 NULL。
 * @param   dev_addr  7 位 I2C 地址，填 MAX17048_I2C_ADDR。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败；
 *          ERR_NOT_READY 器件无应答或版本不符。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_init(max17048_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr);

/**
 * @brief   读取器件标识（VERSION 与 OTP ID）
 * @param   dev  设备句柄。
 * @param   id   输出：标识信息。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_get_id(max17048_dev_t *dev, max17048_id_t *id);

/**
 * @brief   发送 POR 全复位命令（CMD 0x5400）
 * @details 器件在最后一个时钟移入后立即复位，手册明示复位后不回 ACK，
 *          因此本函数忽略该次写的 io 错误。全部寄存器恢复默认值，
 *          自定义模型需重新加载；首次 SOC 约 1s 后才有效。
 * @param   dev  设备句柄。
 * @retval  max17048_result_t  OK 命令已发出；ERR_PARAM 空指针。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_full_reset(max17048_dev_t *dev);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 2. 测量读取（全部定点换算，无浮点）
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读电池电压，单位 mV
 * @details 换算：mV = 原始值 × 78.125µV × 节数 / 1000，四舍五入。
 *          更新周期：活动模式 250ms，休眠模式 45s。
 * @param   dev  设备句柄。
 * @param   mv   输出：电池电压 mV（1 节满量程约 5120mV）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_vcell(max17048_dev_t *dev, uint32_t *mv);

/**
 * @brief   读电池电压原始值（VCELL 寄存器 16 位）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，mV = raw × 78.125µV × 节数。
 * @retval  max17048_result_t  同 max17048_read_vcell()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_vcell_raw(max17048_dev_t *dev, uint16_t *raw);

/**
 * @brief   读电量百分比（整数，四舍五入到 1%）
 * @param   dev      设备句柄。
 * @param   percent  输出：电量百分比 0~100（算法可能短暂略越界，按
 *                   原始值折算不额外钳位）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    POR 后首次有效值约 1s；线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_soc(max17048_dev_t *dev, uint8_t *percent);

/**
 * @brief   读电量百分比（精确到 0.01%）
 * @param   dev          设备句柄。
 * @param   percent_x100 输出：百分比 ×100（原始分辨率 1/256% ≈ 0.0039%）。
 * @retval  max17048_result_t  同 max17048_read_soc()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_soc_precise(max17048_dev_t *dev,
                                            uint16_t *percent_x100);

/**
 * @brief   读电量原始值（SOC 寄存器 16 位）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，% = raw / 256。
 * @retval  max17048_result_t  同 max17048_read_soc()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_soc_raw(max17048_dev_t *dev, uint16_t *raw);

/**
 * @brief   读充放电速率，单位 0.1%/h（正充负放）
 * @details 换算：0.1%/h = 原始值 × 0.208 × 10，四舍五入。手册注明该值
 *          为 SOC 变化率的近似值，不可换算为安培。
 * @param   dev                设备句柄。
 * @param   tenth_pct_per_hour 输出：速率 ×10 %/h，如 -52 表示 -5.2%/h。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_crate(max17048_dev_t *dev,
                                      int16_t *tenth_pct_per_hour);

/**
 * @brief   读充放电速率原始值（CRATE 寄存器 16 位有符号）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值（补码），%/h = raw × 0.208。
 * @retval  max17048_result_t  同 max17048_read_crate()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_crate_raw(max17048_dev_t *dev, int16_t *raw);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 3. RCOMP 温度补偿
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读当前 CONFIG.RCOMP 补偿系数
 * @param   dev   设备句柄。
 * @param   rcomp 输出：RCOMP 原始值（POR 默认 0x97）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_get_rcomp(max17048_dev_t *dev, uint8_t *rcomp);

/**
 * @brief   写 CONFIG.RCOMP 补偿系数
 * @param   dev    设备句柄。
 * @param   rcomp  新 RCOMP 值（0~255）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_set_rcomp(max17048_dev_t *dev, uint8_t rcomp);

/**
 * @brief   按电池温度自动计算并写入 RCOMP
 * @details 手册公式：T > 20℃ 时 RCOMP = RCOMP0 + (T-20)×TempCoUp，
 *          否则用 TempCoDown；系数取自 max17048_conf.h，结果四舍五入
 *          并钳位到 0~255。主机应至少每分钟调用一次。
 * @param   dev      设备句柄。
 * @param   temp_x10 电池温度 ×10 ℃（如 253 表示 25.3℃）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_temp_compensate(max17048_dev_t *dev,
                                           int16_t temp_x10);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 4. 睡眠 / 休眠
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   进入睡眠模式（<1µA）
 * @details 序列：写 MODE.EnSleep=1，再写 CONFIG.SLEEP=1。睡眠期间芯片
 *          停止一切运算、不检测自放电，充放电前必须先唤醒。手册建议
 *          能接受 4µA 的应用优先使用休眠模式（默认已开启）。
 * @param   dev  设备句柄。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_sleep_enter(max17048_dev_t *dev);

/**
 * @brief   退出睡眠模式
 * @details 序列：写 CONFIG.SLEEP=0（手册注明其它通信不会唤醒器件），
 *          随后清 MODE.EnSleep 以解除睡眠武装。
 * @param   dev  设备句柄。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_sleep_exit(max17048_dev_t *dev);

/**
 * @brief   查询休眠状态（MODE.HibStat 位）
 * @param   dev  设备句柄。
 * @param   hib  输出：true 表示芯片处于休眠模式。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    手册 Table 2 将 MODE 标注为只写，个别批次读取该位可能返回
 *          未定义值，仅作参考；线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_is_hibernating(max17048_dev_t *dev, bool *hib);

/**
 * @brief   关闭休眠模式（HIBRT = 0x0000，始终保持活动模式）
 * @param   dev  设备句柄。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    活动模式电流约 23µA；线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_hibernate_disable(max17048_dev_t *dev);

/**
 * @brief   强制休眠模式（HIBRT = 0xFFFF，约 3µA）
 * @details 适合最大负载低于 C/4 的应用；负载较高时保持自动休眠。
 * @param   dev  设备句柄。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_hibernate_force(max17048_dev_t *dev);

/**
 * @brief   设置自动休眠进入/退出阈值（HIBRT 原始档位）
 * @details HibThr：|CRATE| 低于该值持续 6 分钟进入休眠，1 LSB = 0.208%/h；
 *          ActThr：任一次 ADC 采样 |OCV-CELL| 超过该值立即退出休眠，
 *          1 LSB = 1.25mV。POR 默认 HibThr=0x80、ActThr=0x30。
 * @param   dev      设备句柄。
 * @param   hib_thr  休眠阈值原始值（0~255）。
 * @param   act_thr  活动阈值原始值（0~255）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 参数超范围；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_hibernate_set_thresholds(max17048_dev_t *dev,
                                                    uint8_t hib_thr,
                                                    uint8_t act_thr);

/**
 * @brief   读取自动休眠阈值（HIBRT 原始档位）
 * @param   dev      设备句柄。
 * @param   hib_thr  输出：休眠阈值原始值（×0.208%/h）。
 * @param   act_thr  输出：活动阈值原始值（×1.25mV）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_hibernate_get_thresholds(max17048_dev_t *dev,
                                                    uint8_t *hib_thr,
                                                    uint8_t *act_thr);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 5. 告警配置与服务
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   设置低电量告警阈值（CONFIG.ATHD）
 * @details SOC 自上而下穿越该阈值时置 STATUS.HD 并拉低 ALRT 引脚，
 *          仅下降沿触发一次。
 * @param   dev      设备句柄。
 * @param   percent  阈值百分比，钳位到 1~32（POR 默认 4%）。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_set_soc_alert_threshold(max17048_dev_t *dev,
                                                   uint8_t percent);

/**
 * @brief   读取低电量告警阈值
 * @param   dev      设备句柄。
 * @param   percent  输出：阈值百分比（1~32）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_get_soc_alert_threshold(max17048_dev_t *dev,
                                                   uint8_t *percent);

/**
 * @brief   使能/关闭 SOC 变化 1% 告警（CONFIG.ALSC）
 * @details 使能后 SOC 每变化 1% 置 STATUS.SC 并拉低 ALRT。手册注明不要
 *          用此告警累积统计 SOC 变化量。
 * @param   dev     设备句柄。
 * @param   enable  true 使能。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_set_soc_change_alert(max17048_dev_t *dev,
                                                bool enable);

/**
 * @brief   设置电压告警窗口（VALRT.MIN / VALRT.MAX）
 * @details VCELL 高于 max_mv 置 STATUS.VH，低于 min_mv 置 STATUS.VL。
 *          按 20mV 档就近取整，各自钳位到 0~5100mV；min 必须不大于 max。
 * @param   dev     设备句柄。
 * @param   min_mv  欠压告警阈值 mV。
 * @param   max_mv  过压告警阈值 mV。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM min>max；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_set_voltage_alerts(max17048_dev_t *dev,
                                              uint16_t min_mv,
                                              uint16_t max_mv);

/**
 * @brief   读取电压告警窗口
 * @param   dev     设备句柄。
 * @param   min_mv  输出：欠压告警阈值 mV（档位 ×20）。
 * @param   max_mv  输出：过压告警阈值 mV（档位 ×20）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_get_voltage_alerts(max17048_dev_t *dev,
                                              uint16_t *min_mv,
                                              uint16_t *max_mv);

/**
 * @brief   设置电池更换复位阈值与比较器开关（VRESET/ID 寄存器）
 * @details VCELL 跌破阈值后再回升超过阈值时芯片自动快速启动，用于
 *          更换电池后重估 SOC。固定电池建议 2.5V；可拆卸电池建议比系统
 *          放空电压至少低 300mV。阈值按 40mV 档就近取整并钳位到
 *          2280~3480mV。
 * @param   dev                设备句柄。
 * @param   mv                 复位阈值 mV。
 * @param   disable_comparator true 时休眠模式下禁用模拟比较器（省约
 *                             0.5µA，复位检测从 1ms 变为 250ms 数字比较）。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_set_vreset_threshold(max17048_dev_t *dev,
                                                uint16_t mv,
                                                bool disable_comparator);

/**
 * @brief   读取电池更换复位阈值与比较器开关
 * @param   dev                设备句柄。
 * @param   mv                 输出：复位阈值 mV（档位 ×40）。
 * @param   disable_comparator 输出：true 表示休眠时比较器禁用。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_get_vreset_threshold(max17048_dev_t *dev,
                                                uint16_t *mv,
                                                bool *disable_comparator);

/**
 * @brief   使能/关闭电压复位事件告警（STATUS.ENVR）
 * @details 使能后发生 VRESET 事件时置 STATUS.VR 并拉低 ALRT 引脚。
 * @param   dev     设备句柄。
 * @param   enable  true 使能。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_set_vreset_alert_enable(max17048_dev_t *dev,
                                                   bool enable);

/**
 * @brief   读取并解码 STATUS 寄存器（不清除任何位）
 * @param   dev     设备句柄。
 * @param   status  输出：解码后的状态位。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    ALRT 引脚 ISR 只应置标志，本函数须在线程上下文调用。
 */
max17048_result_t max17048_get_status(max17048_dev_t *dev,
                                      max17048_status_t *status);

/**
 * @brief   清除告警位并释放 ALRT 引脚
 * @details 对 STATUS 中被置 1 的指定位写 1 清除（可传
 *          MAX17048_STATUS_ALERT_MASK 一次清全部告警），随后清
 *          CONFIG.ALRT 使 ALRT 引脚恢复高电平——不清 CONFIG.ALRT
 *          引脚会一直保持低。
 * @param   dev          设备句柄。
 * @param   status_bits  待清除的 STATUS 位掩码（超出已知位自动忽略）。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    须在线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_clear_alerts(max17048_dev_t *dev,
                                        uint16_t status_bits);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 6. 快速启动 / 自定义模型表
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   触发快速启动（MODE.QuickStart）
 * @details 丢弃当前 SOC 估计，按即时电压重新估算 OCV/SOC（等效重新上
 *          电）。手册警告：多数系统不需要快速启动，仅在上电波形导致
 *          初始 SOC 明显错误时、且电池电压已稳定时谨慎使用。
 * @param   dev  设备句柄。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_quick_start(max17048_dev_t *dev);

#if MAX17048_USE_MODEL_TABLE
/**
 * @brief   加载自定义电池模型表（TABLE 0x40~0x7F，64 字）
 * @details 序列：解锁模型表（0x3E←0x4A57）→ 停顿 → 逐字写入 64 个
 *          16 位表项（每字独立 I2C 事务，手册规定自增写越过 0x4F 会被
 *          忽略，故不可整块突发写）→ 停顿 → 复锁（0x3E←0x0000）。
 *          解锁期间 ModelGauge 引擎停止更新，本函数尽快复锁。
 *          模型参数需向厂商申请或对电池表征获得，POR 的 ROM 默认模型
 *          对部分电池已够用。
 * @param   dev   设备句柄。
 * @param   table 64 个 16 位模型字（调用者保证生命周期覆盖本调用）。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    加载后 SOC 需要一个更新周期才生效；线程上下文调用，
 *          禁止在 ISR 中调用。
 */
max17048_result_t max17048_model_load(max17048_dev_t *dev,
                                      const uint16_t *table);
#endif /* MAX17048_USE_MODEL_TABLE */

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 7. 寄存器级原始访问（调试 / 覆盖未封装功能）
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读 16 位寄存器原始值
 * @param   dev  设备句柄。
 * @param   reg  寄存器基地址（偶数地址）。
 * @param   val  输出：16 位原始值。
 * @retval  max17048_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_read_reg(max17048_dev_t *dev, uint8_t reg,
                                    uint16_t *val);

/**
 * @brief   写 16 位寄存器原始值
 * @param   dev  设备句柄。
 * @param   reg  寄存器基地址（偶数地址）。
 * @param   val  待写 16 位值。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    写只读地址被器件忽略；线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_write_reg(max17048_dev_t *dev, uint8_t reg,
                                     uint16_t val);

/**
 * @brief   读-改-写：向 reg 写入 (val & mask)
 * @param   dev   设备句柄。
 * @param   reg   寄存器基地址（偶数地址）。
 * @param   mask  保留位掩码（0 的位保持原值）。
 * @param   val   新字段值（已位于目标位位置）。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17048_result_t max17048_update_bits(max17048_dev_t *dev, uint8_t reg,
                                       uint16_t mask, uint16_t val);

#ifdef __cplusplus
}
#endif

#endif /* MAX17048_H */
