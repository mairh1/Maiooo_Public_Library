/**
 * @file    max17260.h
 * @brief   MAX17260 ModelGauge m5 EZ 1 节锂电电量计通用驱动公共 API
 * @details 器件：Analog Devices / Maxim MAX17260，5.1µA 超低功耗 1 节
 *          ModelGauge m5 EZ 电量计，I2C 接口（7 位地址 0x36 / 0x0D
 *          视变体而定），16 位寄存器，支持高/低边电流检测，集成
 *          coulomb counter + voltage-based fuel gauge 混合算法。
 *
 *          分层架构（详见 README.md）：
 *
 *            +--------------------------------------+
 *            |  应用层                               |  用户代码
 *            +--------------------------------------+
 *                        |  本 API（max17260.h）
 *            +--------------------------------------+
 *            |  驱动核心  max17260.c                  |  纯 C99、零平台代码
 *            |  配置      max17260_conf.h             |  无动态内存
 *            |  寄存器    max17260_regs.h             |
 *            +--------------------------------------+
 *                        |  4~6 个 io 函数（max17260_io.h）
 *            +--------------------------------------+
 *            |  移植层（用户提供）                     |  CH32 / STM32 / ESP32 /
 *            |                                        |  RTOS / Linux / 模拟 I2C
 *            +--------------------------------------+
 *
 *          移植 = 实现 max17260_io.h + 按需调整 max17260_conf.h，
 *          核心文件零改动。
 *
 *          单位约定（贯穿全部 API）：电压 mV，SOC %（原始 1/256%），
 *          温度 0.1℃（原始 1/256℃），容量 mAh，电流 mA，时间 s
 *          （原始 5.625s/LSB）。全部定点运算，无浮点。
 * @note    set 类函数按硬件档位就近取整并钳位到支持范围，对应 get 返回
 *          实际生效值。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-02
 */

#ifndef MAX17260_H
#define MAX17260_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "max17260_conf.h"
#include "max17260_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 结果码（全部公共 API 的统一返回类型）
 * ════════════════════════════════════════════════════════════════════════ */

typedef enum {
    MAX17260_OK = 0,            /**< 成功 */
    MAX17260_ERR_IO,            /**< I2C 通信失败（移植层返回 ERROR 时上抛） */
    MAX17260_ERR_PARAM,         /**< 参数非法（空指针、超范围） */
    MAX17260_ERR_NOT_READY,     /**< 未初始化 / 器件无应答 */
    MAX17260_ERR_NOT_SUPPORTED, /**< 功能被 conf 裁剪或器件不支持 */
    MAX17260_ERR_VERIFY,        /**< 写后回读不一致（MAX17260_VERIFY_WRITES） */
} max17260_result_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 设备句柄（每片芯片一个，由调用者分配）
 * ════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void    *io_ctx;    /**< 总线上下文，原样透传给 io 函数（多总线设计用，
                             单总线可为 NULL） */
    uint8_t dev_addr;   /**< 7 位 I2C 地址，填 MAX17260_I2C_ADDR_DEFAULT
                             或 MAX17260_I2C_ADDR_ALT */
    uint8_t inited;     /**< max17260_init() 置 1 */
} max17260_dev_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 状态与标识类型
 * ════════════════════════════════════════════════════════════════════════ */

/** Status 寄存器（0x00）解码结果，告警位写 1 清除 */
typedef struct {
    bool por;           /**< POR：发生 POR 事件，须软件清零 */
    bool batt_present;  /**< Bst：true 表示电池在位 */
    bool batt_removed;  /**< Br：电池移除事件，须软件清零 */
    bool batt_inserted; /**< Bi：电池插入事件，须软件清零 */
    bool soc_high;      /**< Smx：RepSOC > SAlrtTh.MAX */
    bool soc_low;       /**< Smn：RepSOC < SAlrtTh.MIN */
    bool vcell_high;    /**< Vmx：VCell > VAlrtTh.MAX */
    bool vcell_low;     /**< Vmn：VCell < VAlrtTh.MIN */
    bool temp_high;     /**< Tmx：Temp > TAlrtTh.MAX */
    bool temp_low;      /**< Tmn：Temp < TAlrtTh.MIN */
    bool curr_high;     /**< Imx：Current > IAlrtTh.MAX */
    bool curr_low;      /**< Imn：Current < IAlrtTh.MIN */
    bool dsoc_change;   /**< dSOCi：RepSOC 跨过 1% 整数边界 */
    uint16_t raw;       /**< 原始 Status 值 */
} max17260_status_t;

/** 128 位器件唯一序列号 */
typedef struct {
    uint16_t word[8];   /**< word[0]=0xD4, word[7]=0xDF，跳过 0xD6/0xD7/0xD8/0xDB */
} max17260_serial_t;

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 1. 初始化 / 标识
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   初始化设备句柄并确认器件在位
 * @details 调用 max17260_io_init() 一次；读 Status 校验，若 POR 置位
 *          则清除（表示此前发生过 POR 事件，使用自定义模型的应用需
 *          随后调用 max17260_configure_model() 写入 DesignCap /
 *          VEmpty / IChgTerm）。
 * @param   dev       设备句柄，调用者分配。
 * @param   io_ctx    总线上下文，透传给 io 函数，可为 NULL。
 * @param   dev_addr  7 位 I2C 地址，填 MAX17260_I2C_ADDR_DEFAULT 或
 *                    MAX17260_I2C_ADDR_ALT。
 * @retval  max17260_result_t  OK 成功；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17260_result_t max17260_init(max17260_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr);

/**
 * @brief   查询 Status.POR 位
 * @details POR 在上电 / 硬件复位后置位，可由用户清零。
 * @param   dev      设备句柄。
 * @param   por      输出：true 表示发生 POR 事件。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_is_por(max17260_dev_t *dev, bool *por);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 2. 测量读取（全部定点换算，无浮点）
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读电池电压，单位 mV
 * @details 换算：mV = 原始值 × 78.125µV / 1000，四舍五入。
 *          满量程 0~5119.92mV。
 * @param   dev  设备句柄。
 * @param   mv   输出：电池电压 mV。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
max17260_result_t max17260_read_vcell(max17260_dev_t *dev, uint32_t *mv);

/**
 * @brief   读电池电压原始值（VCELL 寄存器 16 位）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，mV = raw × 78.125µV。
 * @retval  max17260_result_t  同 max17260_read_vcell()。
 */
max17260_result_t max17260_read_vcell_raw(max17260_dev_t *dev, uint16_t *raw);

/**
 * @brief   读电量百分比（整数，四舍五入到 1%）
 * @details RepSOC 原始单位 1/256%。
 * @param   dev      设备句柄。
 * @param   percent  输出：电量百分比 0~100。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    POR 后 351ms 内输出无效；线程上下文调用。
 */
max17260_result_t max17260_read_soc(max17260_dev_t *dev, uint8_t *percent);

/**
 * @brief   读电量百分比（精确到 0.01%）
 * @param   dev          设备句柄。
 * @param   percent_x100 输出：百分比 ×100（原始分辨率 1/256% ≈ 0.0039%）。
 * @retval  max17260_result_t  同 max17260_read_soc()。
 */
max17260_result_t max17260_read_soc_precise(max17260_dev_t *dev,
                                            uint16_t *percent_x100);

/**
 * @brief   读 SOC 原始值（RepSOC 寄存器 16 位）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，% = raw / 256。
 * @retval  max17260_result_t  同 max17260_read_soc()。
 */
max17260_result_t max17260_read_soc_raw(max17260_dev_t *dev, uint16_t *raw);

/**
 * @brief   读电池温度，单位 0.1℃（有符号）
 * @details 换算：0.1℃ = 原始值 × 10 / 256，四舍五入。
 *          原始范围 -128.0℃ ~ +127.996℃。
 * @param   dev        设备句柄。
 * @param   temp_x10   输出：温度 ×10 ℃。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_temp(max17260_dev_t *dev, int16_t *temp_x10);

/**
 * @brief   读温度原始值（Temp 寄存器 16 位有符号）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值（补码），℃ = raw / 256。
 * @retval  max17260_result_t  同 max17260_read_temp()。
 */
max17260_result_t max17260_read_temp_raw(max17260_dev_t *dev, int16_t *raw);

/**
 * @brief   读瞬时电流，单位 mA（充正放负）
 * @details 换算：µV = 原始值 × 1.5625µV/RSENSE；mA = µV / RSENSE_mΩ。
 *          当 MAX17260_RSENSE_MOHM=0 时输出原始 µV 值。
 *          量程 ±51.2mV / RSENSE。
 * @param   dev   设备句柄。
 * @param   ma    输出：电流 mA（MAX17260_RSENSE_MOHM=0 时为原始 µV）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_current(max17260_dev_t *dev, int32_t *ma);

/**
 * @brief   读电流原始值（Current 寄存器 16 位有符号）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值（补码），µV = raw × 1.5625µV。
 * @retval  max17260_result_t  同 max17260_read_current()。
 */
max17260_result_t max17260_read_current_raw(max17260_dev_t *dev,
                                            int16_t *raw);

/**
 * @brief   读剩余容量，单位 mAh
 * @details 换算：µVh = 原始值 × 5.0µVh/RSENSE；mAh = µVh / RSENSE_mΩ。
 *          当 MAX17260_RSENSE_MOHM=0 时输出原始 µVh 值。
 * @param   dev   设备句柄。
 * @param   mah   输出：剩余容量 mAh（MAX17260_RSENSE_MOHM=0 时为原始 µVh）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_repcap(max17260_dev_t *dev, uint32_t *mah);

/**
 * @brief   读剩余容量原始值（RepCap 寄存器 16 位）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，µVh = raw × 5.0µVh。
 * @retval  max17260_result_t  同 max17260_read_repcap()。
 */
max17260_result_t max17260_read_repcap_raw(max17260_dev_t *dev,
                                           uint16_t *raw);

/**
 * @brief   读满充容量，单位 mAh
 * @param   dev   设备句柄。
 * @param   mah   输出：满充容量 mAh（MAX17260_RSENSE_MOHM=0 时为原始 µVh）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_fullcaprep(max17260_dev_t *dev, uint32_t *mah);

/**
 * @brief   读剩余放电时间（TTE），单位秒
 * @details 换算：s = 原始值 × 5.625。
 *          仅当 Current < 0 时有效；手册明确：正电流时该值无意义。
 * @param   dev        设备句柄。
 * @param   seconds    输出：剩余放电时间 s。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_tte(max17260_dev_t *dev, uint32_t *seconds);

/**
 * @brief   读剩余充电时间（TTF），单位秒
 * @details 换算：s = 原始值 × 5.625。
 *          仅当 Current > 0 时有效。
 * @param   dev        设备句柄。
 * @param   seconds    输出：剩余充电时间 s。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_ttf(max17260_dev_t *dev, uint32_t *seconds);

/**
 * @brief   读瞬时功率，单位 mW
 * @details 换算：µV² = 原始值 × 8µV²/RSENSE；mW = µV² / RSENSE_mΩ。
 * @param   dev   设备句柄。
 * @param   mw    输出：功率 mW（MAX17260_RSENSE_MOHM=0 时为原始 µV²）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_power(max17260_dev_t *dev, int32_t *mw);

/**
 * @brief   读充放电循环数（百分比，1% / LSB）
 * @details 寄存器范围 0.0~655.35 cycle，100% 一次完整充放电。
 * @param   dev      设备句柄。
 * @param   cycles   输出：循环百分比（如 100 表示 1.00 次循环）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_cycles(max17260_dev_t *dev, uint16_t *cycles);

/**
 * @brief   读电池老化系数（百分比，1% / LSB）
 * @details 反映电池老化程度，0% 表示新电池。
 * @param   dev   设备句柄。
 * @param   age   输出：老化百分比。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_age(max17260_dev_t *dev, uint8_t *age);

/**
 * @brief   读芯片内部温度，单位 0.1℃（有符号）
 * @details DieTemp 寄存器（8 位地址 0x034），仅受芯片内部传感器影响。
 * @param   dev        设备句柄。
 * @param   temp_x10   输出：内部温度 ×10 ℃。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_dietemp(max17260_dev_t *dev, int16_t *temp_x10);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 3. 平均与最大最小（模拟测量）
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读平均电压（AvgVCell）
 * @param   dev  设备句柄。
 * @param   mv   输出：平均电池电压 mV。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_avg_vcell(max17260_dev_t *dev, uint32_t *mv);

/**
 * @brief   读平均电流（AvgCurrent）
 * @param   dev  设备句柄。
 * @param   ma   输出：平均电流 mA（MAX17260_RSENSE_MOHM=0 时为原始 µV）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_avg_current(max17260_dev_t *dev, int32_t *ma);

/**
 * @brief   读平均温度（AvgTA）
 * @param   dev        设备句柄。
 * @param   temp_x10   输出：平均温度 ×10 ℃。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_avg_temp(max17260_dev_t *dev, int16_t *temp_x10);

/**
 * @brief   读平均功率（AvgPower）
 * @param   dev  设备句柄。
 * @param   mw   输出：平均功率 mW（MAX17260_RSENSE_MOHM=0 时为原始 µV²）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_avg_power(max17260_dev_t *dev, int32_t *mw);

/**
 * @brief   读最大/最小电压（MaxMinVolt）
 * @details 20mV/LSB。上电默认 0x00FF，写 0x00FF 可复位。
 * @param   dev       设备句柄。
 * @param   max_mv    输出：最大电压 mV。
 * @param   min_mv    输出：最小电压 mV。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_maxmin_volt(max17260_dev_t *dev,
                                            uint32_t *max_mv,
                                            uint32_t *min_mv);

/**
 * @brief   读最大/最小电流（MaxMinCurr）
 * @details 0.4mV/RSENSE/LSB。写 0x807F 复位。
 * @param   dev       设备句柄。
 * @param   max_ma    输出：最大电流 mA（MAX17260_RSENSE_MOHM=0 时为原始 µV）。
 * @param   min_ma    输出：最小电流 mA（MAX17260_RSENSE_MOHM=0 时为原始 µV）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_maxmin_curr(max17260_dev_t *dev,
                                            int32_t *max_ma,
                                            int32_t *min_ma);

/**
 * @brief   读最大/最小温度（MaxMinTemp）
 * @details 1℃/LSB。写 0x807F 复位。
 * @param   dev        设备句柄。
 * @param   max_x10    输出：最高温度 ×10 ℃。
 * @param   min_x10    输出：最低温度 ×10 ℃。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_maxmin_temp(max17260_dev_t *dev,
                                            int16_t *max_x10,
                                            int16_t *min_x10);

/**
 * @brief   复位所有 MaxMin* 寄存器为 POR 默认值
 * @details 写 0x00FF / 0x807F / 0x807F 到 MaxMinVolt/Curr/Temp。
 * @param   dev  设备句柄。
 * @retval  max17260_result_t  OK 成功；ERR_IO 通信失败。
 */
max17260_result_t max17260_reset_maxmin(max17260_dev_t *dev);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 4. Model m5 EZ 配置
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   一站式配置 Model m5 EZ
 * @details 写 DesignCap（mAh）、VEmpty（mV）、IChgTerm（mA）。
 *          手册 Application Notes：EZ 算法上电必须配置这三项之一
 *          （否则使用 POR 默认）。对绝大多数电池推荐用此函数一次完成。
 * @param   dev         设备句柄。
 * @param   design_mah  设计容量 mAh（1~32767）。
 * @param   vempty_mv   空载电压 mV（0~5110，10mV 档）。
 * @param   vrecovery_mv 恢复电压 mV（0~5080，40mV 档；通常 vempty+80mV 左右）。
 * @param   ichg_term_ma 充电终止电流 mA。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 超范围；ERR_IO 通信失败。
 * @note    写完后需等待约 351ms 让算法稳定；线程上下文调用。
 */
max17260_result_t max17260_configure_model(max17260_dev_t *dev,
                                           uint16_t design_mah,
                                           uint16_t vempty_mv,
                                           uint16_t vrecovery_mv,
                                           uint16_t ichg_term_ma);

/**
 * @brief   读 DesignCap
 * @param   dev   设备句柄。
 * @param   mah   输出：设计容量 mAh（MAX17260_RSENSE_MOHM=0 时为原始 µVh）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_design_cap(max17260_dev_t *dev, uint32_t *mah);

/**
 * @brief   读 VEmpty（VE 与 VR）
 * @details 10mV / 40mV 档。
 * @param   dev            设备句柄。
 * @param   ve_mv          输出：空载电压 mV。
 * @param   vr_mv          输出：恢复电压 mV。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_vempty(max17260_dev_t *dev, uint16_t *ve_mv,
                                      uint16_t *vr_mv);

/**
 * @brief   读 IChgTerm
 * @param   dev     设备句柄。
 * @param   ma      输出：充电终止电流 mA（MAX17260_RSENSE_MOHM=0 时为原始 µV）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_ichg_term(max17260_dev_t *dev, uint16_t *ma);

/**
 * @brief   读 ModelCfg
 * @param   dev  设备句柄。
 * @param   val  输出：ModelCfg 原始 16 位值。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_modelcfg(max17260_dev_t *dev, uint16_t *val);

/**
 * @brief   写 ModelCfg
 * @param   dev  设备句柄。
 * @param   val  待写 ModelCfg 原始值（按手册 Table 4 编码）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_set_modelcfg(max17260_dev_t *dev, uint16_t val);

/**
 * @brief   读 Config
 * @param   dev  设备句柄。
 * @param   val  输出：Config 原始 16 位值。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_config(max17260_dev_t *dev, uint16_t *val);

/**
 * @brief   写 Config
 * @param   dev  设备句柄。
 * @param   val  待写 Config 原始值。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_set_config(max17260_dev_t *dev, uint16_t val);

/**
 * @brief   读 Config2
 * @param   dev  设备句柄。
 * @param   val  输出：Config2 原始 16 位值。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_config2(max17260_dev_t *dev, uint16_t *val);

/**
 * @brief   写 Config2
 * @param   dev  设备句柄。
 * @param   val  待写 Config2 原始值。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_set_config2(max17260_dev_t *dev, uint16_t val);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 5. Alert 阈值与状态服务
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   设置电压告警窗口（VAlrtTh）
 * @details VCell > max_mv 置 Status.Vmx，< min_mv 置 Status.Vmn。
 *          20mV 档就近取整，各自钳位到 0~5100mV；min 不大于 max。
 *          写 0xFF00 = 禁用电压告警。
 * @param   dev     设备句柄。
 * @param   min_mv  欠压告警阈值 mV。
 * @param   max_mv  过压告警阈值 mV。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM min>max；ERR_IO 通信失败。
 */
max17260_result_t max17260_set_voltage_alerts(max17260_dev_t *dev,
                                              uint16_t min_mv,
                                              uint16_t max_mv);

/**
 * @brief   读电压告警窗口
 * @param   dev     设备句柄。
 * @param   min_mv  输出：欠压告警阈值 mV（档位 ×20）。
 * @param   max_mv  输出：过压告警阈值 mV（档位 ×20）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_voltage_alerts(max17260_dev_t *dev,
                                              uint16_t *min_mv,
                                              uint16_t *max_mv);

/**
 * @brief   设置温度告警窗口（TAlrtTh）
 * @details 1℃ 档，-128~+127℃；0x7F80 = 禁用。
 * @param   dev       设备句柄。
 * @param   min_x10   欠温告警阈值 ×10 ℃。
 * @param   max_x10   过温告警阈值 ×10 ℃。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM min>max 或超范围；ERR_IO。
 */
max17260_result_t max17260_set_temp_alerts(max17260_dev_t *dev,
                                           int16_t min_x10, int16_t max_x10);

/**
 * @brief   读温度告警窗口
 * @param   dev       设备句柄。
 * @param   min_x10   输出：欠温告警阈值 ×10 ℃。
 * @param   max_x10   输出：过温告警阈值 ×10 ℃。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_temp_alerts(max17260_dev_t *dev,
                                           int16_t *min_x10,
                                           int16_t *max_x10);

/**
 * @brief   设置 SOC 告警窗口（SAlrtTh）
 * @details 1% 档，0~255%；0xFF00 = 禁用。
 * @param   dev     设备句柄。
 * @param   min_pct  低 SOC 告警百分比。
 * @param   max_pct  高 SOC 告警百分比。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM min>max；ERR_IO 通信失败。
 */
max17260_result_t max17260_set_soc_alerts(max17260_dev_t *dev,
                                          uint8_t min_pct, uint8_t max_pct);

/**
 * @brief   读 SOC 告警窗口
 * @param   dev     设备句柄。
 * @param   min_pct 输出：低 SOC 告警百分比。
 * @param   max_pct 输出：高 SOC 告警百分比。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_soc_alerts(max17260_dev_t *dev,
                                          uint8_t *min_pct,
                                          uint8_t *max_pct);

/**
 * @brief   设置电流告警窗口（IAlrtTh）
 * @details 0.4mV/RSENSE/LSB（补码）；0x7F80 = 禁用。
 * @param   dev       设备句柄。
 * @param   min_ma    下限电流 mA。
 * @param   max_ma    上限电流 mA。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM min>max；ERR_IO 通信失败。
 */
max17260_result_t max17260_set_current_alerts(max17260_dev_t *dev,
                                              int32_t min_ma, int32_t max_ma);

/**
 * @brief   读电流告警窗口
 * @param   dev       设备句柄。
 * @param   min_ma    输出：下限电流 mA（MAX17260_RSENSE_MOHM=0 时为原始 µV）。
 * @param   max_ma    输出：上限电流 mA（MAX17260_RSENSE_MOHM=0 时为原始 µV）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_get_current_alerts(max17260_dev_t *dev,
                                              int32_t *min_ma,
                                              int32_t *max_ma);

/**
 * @brief   读取并解码 Status 寄存器（不清除任何位）
 * @param   dev     设备句柄。
 * @param   status  输出：解码后的状态位。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    ALRT 引脚 ISR 只应置标志，本函数须在线程上下文调用。
 */
max17260_result_t max17260_get_status(max17260_dev_t *dev,
                                      max17260_status_t *status);

/**
 * @brief   清除告警位（写 1 清除指定 Status 位）
 * @details 对 Status 中被置 1 的指定位写 1 清除，写 0 不影响。
 *          通常传入 MAX17260_STATUS_CLEAR_MASK 一次清全部已知告警。
 * @param   dev          设备句柄。
 * @param   status_bits  待清除的 Status 位掩码（超出已知位自动忽略）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    须在线程上下文调用，禁止在 ISR 中调用。
 */
max17260_result_t max17260_clear_alerts(max17260_dev_t *dev,
                                        uint16_t status_bits);

#if MAX17260_USE_SERIAL_NUMBER
/**
 * @brief   读 128 位唯一序列号
 * @details 内部清 Config2.AtRateEn 与 DPEn 切到序列号模式，读取 0xD4~0xDF
 *          8 个 16 位字后恢复 AtRateEn/DPEn。读取过程中动态功率与
 *          AtRate 输出寄存器被覆盖，应用须自行避免该窗口。
 * @param   dev   设备句柄。
 * @param   sn    输出：序列号结构。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用；中途失败仍会尝试恢复
 *          AtRateEn/DPEn。
 */
max17260_result_t max17260_read_serial(max17260_dev_t *dev,
                                       max17260_serial_t *sn);
#endif /* MAX17260_USE_SERIAL_NUMBER */

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 6. 寄存器级原始访问（调试 / 覆盖未封装功能）
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读 16 位寄存器原始值
 * @param   dev  设备句柄。
 * @param   reg  寄存器基地址（手册 Table 17 中偶数地址或 8 位地址）。
 * @param   val  输出：16 位原始值。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 */
max17260_result_t max17260_read_reg(max17260_dev_t *dev, uint8_t reg,
                                    uint16_t *val);

/**
 * @brief   写 16 位寄存器原始值
 * @param   dev  设备句柄。
 * @param   reg  寄存器基地址（偶数地址）。
 * @param   val  待写 16 位值。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    写只读地址被器件忽略。
 */
max17260_result_t max17260_write_reg(max17260_dev_t *dev, uint8_t reg,
                                     uint16_t val);

/**
 * @brief   读-改-写：向 reg 写入 (val & mask)
 * @param   dev   设备句柄。
 * @param   reg   寄存器基地址（偶数地址）。
 * @param   mask  保留位掩码（0 的位保持原值）。
 * @param   val   新字段值（已位于目标位位置）。
 * @retval  max17260_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    不要对 Status 用本函数（会误清告警位）；Status 一律走
 *          get_status / clear_alerts。
 */
max17260_result_t max17260_update_bits(max17260_dev_t *dev, uint8_t reg,
                                       uint16_t mask, uint16_t val);

#ifdef __cplusplus
}
#endif

#endif /* MAX17260_H */
