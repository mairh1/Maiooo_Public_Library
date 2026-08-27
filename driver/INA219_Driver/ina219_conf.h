/*
 * @file    ina219_conf.h
 * @brief   INA219 电流/功率监测驱动配置
 * @details 全部配置宏集中在本文件，均带 #ifndef 默认值，可通过编译器
 *          命令行 -D 覆盖，禁止修改驱动源码来配置。
 *          默认值按「1Ω 采样电阻」设计：PGA /8（±320mV 满量程）配
 *          Current_LSB = 10µA，校准寄存器恰为 4096（0x1000），电流量程
 *          ±320mA、分辨率 10µA、功率分辨率 200µW，全部换算无舍入损失。
 * @note    置 0 关闭某功能开关后，对应公共 API、核心实现与移植契约
 *          函数一起被 #if 裁剪，被裁剪 API 不再存在（编译期报错）。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 */

#ifndef INA219_CONF_H
#define INA219_CONF_H

/* ══════════════════════════════════════════════════════════════════════════
 * 测量系统参数（init 时写入器件）
 * ══════════════════════════════════════════════════════════════════════════ */

#ifndef INA219_SHUNT_UOHMS
#define INA219_SHUNT_UOHMS     1000000UL  /**< 采样电阻阻值，单位 µΩ；
                                               默认 1Ω（1000000µΩ） */
#endif

#if INA219_SHUNT_UOHMS < 1UL
#error "INA219_SHUNT_UOHMS 必须为正整数（µΩ）"
#endif

#ifndef INA219_CURRENT_LSB_NA
#define INA219_CURRENT_LSB_NA  10000UL   /**< Current_LSB，单位 nA；默认 10µA。
                                              1Ω 下限约 1.25µA（校准寄存器
                                              15 位上限），过小 init 报参数错 */
#endif

#if INA219_CURRENT_LSB_NA < 1UL
#error "INA219_CURRENT_LSB_NA 必须为正整数（nA）"
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 器件配置默认值（init 时整字写入配置寄存器）
 * ══════════════════════════════════════════════════════════════════════════ */

#ifndef INA219_BUS_RANGE_32V
#define INA219_BUS_RANGE_32V   1   /**< 总线电压量程：1=32V，0=16V（与 POR 默认一致） */
#endif

#if (INA219_BUS_RANGE_32V != 0) && (INA219_BUS_RANGE_32V != 1)
#error "INA219_BUS_RANGE_32V 必须为 0（16V）或 1（32V）"
#endif

#ifndef INA219_PGA_RANGE_MV
#define INA219_PGA_RANGE_MV    320 /**< 分流电压满量程 mV：40/80/160/320 四档，
                                        就近取整；1Ω 下 320mV 即 ±320mA 量程 */
#endif

#if (INA219_PGA_RANGE_MV != 40) && (INA219_PGA_RANGE_MV != 80) && \
    (INA219_PGA_RANGE_MV != 160) && (INA219_PGA_RANGE_MV != 320)
#error "INA219_PGA_RANGE_MV 必须为 40 / 80 / 160 / 320 之一"
#endif

#ifndef INA219_BADC
#define INA219_BADC            0x3    /**< 总线 ADC 档位：0x0~0x3 = 9~12bit，
                                           0x9~0xF = 12bit×2~128 次平均 */
#endif

#ifndef INA219_SADC
#define INA219_SADC            0x3    /**< 分流 ADC 档位（编码同 INA219_BADC） */
#endif

#if !(((INA219_BADC >= 0x0) && (INA219_BADC <= 0x3)) || \
      ((INA219_BADC >= 0x9) && (INA219_BADC <= 0xF)))
#error "INA219_BADC 必须为 0x0~0x3（分辨率）或 0x9~0xF（平均）之一"
#endif

#if !(((INA219_SADC >= 0x0) && (INA219_SADC <= 0x3)) || \
      ((INA219_SADC >= 0x9) && (INA219_SADC <= 0xF)))
#error "INA219_SADC 必须为 0x0~0x3（分辨率）或 0x9~0xF（平均）之一"
#endif

#ifndef INA219_MODE
#define INA219_MODE            0x7    /**< 工作模式：0x0 掉电 / 0x1~0x3 触发
                                           单次 / 0x4 ADC 关 / 0x5~0x7 连续；
                                           默认 0x7 分流+总线连续（POR 同） */
#endif

#if (INA219_MODE < 0x0) || (INA219_MODE > 0x7)
#error "INA219_MODE 必须为 0x0~0x7（Table 6 编码）"
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * INA219_USE_TRIGGERED —— 触发模式与转换等待 API
 * ══════════════════════════════════════════════════════════════════════════ */
/* 置 1 提供 ina219_trigger() / ina219_wait_conversion()（依赖移植层
 * ina219_io_delay_ms）；只用连续转换模式的应用可置 0 裁剪，移植层
 * 免实现 delay。 */

#ifndef INA219_USE_TRIGGERED
#define INA219_USE_TRIGGERED   1
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * INA219_USE_POWER —— 功率测量 API
 * ══════════════════════════════════════════════════════════════════════════ */
/* 置 1 提供 ina219_read_power() / ina219_read_power_raw()；只关心电流的
 * 应用可置 0 裁剪（注意：读功率寄存器会硬件清除 CNVR 标志，裁剪后
 * wait_conversion 不再受该副作用影响）。 */

#ifndef INA219_USE_POWER
#define INA219_USE_POWER       1
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * INA219_VERIFY_WRITES —— 写后回读校验
 * ══════════════════════════════════════════════════════════════════════════ */
/* 置 1 时每次写 R/W 寄存器（配置/校准）后回读比较，不一致返回
 * INA219_ERR_VERIFY。每次写多一笔 I2C 读，量产默认关闭。 */

#ifndef INA219_VERIFY_WRITES
#define INA219_VERIFY_WRITES   0
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * INA219_THREAD_SAFE —— 并发保护钩子
 * ══════════════════════════════════════════════════════════════════════════ */
/* 置 1 时驱动在多笔总线访问的 API 前后调用 ina219_io_lock()/unlock()，
 * 由移植层实现（如 RTOS 互斥量）。默认线程安全性见 README：不同实例
 * 并发安全，同一实例需外部保护。单线程系统保持 0 零开销。 */

#ifndef INA219_THREAD_SAFE
#define INA219_THREAD_SAFE     0
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * INA219_WAIT_POLL_MS —— 转换等待的轮询间隔
 * ══════════════════════════════════════════════════════════════════════════ */

#ifndef INA219_WAIT_POLL_MS
#define INA219_WAIT_POLL_MS    1   /**< ina219_wait_conversion() 两次查询间隔 ms，
                                        仅 INA219_USE_TRIGGERED=1 时使用 */
#endif

#endif /* INA219_CONF_H */
