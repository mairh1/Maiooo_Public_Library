/**
 * @file    max17260_conf.h
 * @brief   MAX17260 ModelGauge m5 EZ 电量计驱动配置
 * @details 全部配置宏集中在本文件，均带 #ifndef 默认值，可通过编译器
 *          命令行 -D 覆盖，禁止修改驱动源码来配置。
 * @note    置 0 关闭某功能开关后，对应公共 API、核心实现与移植契约
 *          函数一起被 #if 裁剪，被裁剪 API 不再存在（编译期报错）。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-02
 */

#ifndef MAX17260_CONF_H
#define MAX17260_CONF_H

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17260_VARIANT —— 器件变体（决定 I2C 7 位地址）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_VARIANT_DEFAULT   0   /**< SEWL+ / SETD+ 系列，地址 0x36 */
#define MAX17260_VARIANT_B_SERIES  1   /**< BEWL+ 系列，地址 0x0D */

#ifndef MAX17260_VARIANT
#define MAX17260_VARIANT    MAX17260_VARIANT_DEFAULT
#endif

#if (MAX17260_VARIANT != MAX17260_VARIANT_DEFAULT) && \
    (MAX17260_VARIANT != MAX17260_VARIANT_B_SERIES)
#error "MAX17260_VARIANT 必须为 MAX17260_VARIANT_DEFAULT 或 MAX17260_VARIANT_B_SERIES"
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17260_RSENSE_MOHM —— 检流电阻值（毫欧）
 * ════════════════════════════════════════════════════════════════════════ */
/* 容量 / 电流寄存器原始单位为 µVh / µV（手册 Table 2）。填入实际 RSENSE
 * 毫欧值后，read_repcap / read_current 等 API 自动换算为 mAh / mA。
 * 设为 0 时 API 返回原始 µVh / µV 值（用于不关心 mAh / mA 的应用）。
 * 手册推荐 10mΩ（典型值 1mΩ~1000mΩ）。 */

#ifndef MAX17260_RSENSE_MOHM
#define MAX17260_RSENSE_MOHM      10
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17260_USE_SERIAL_NUMBER —— 128 位唯一序列号读取 API
 * ════════════════════════════════════════════════════════════════════════ */
/* 置 1 提供 max17260_read_serial()；置 0 裁剪该 API 与 AtRateEn/DPEn 切换
 * 逻辑。序列号在 0xD4~0xDF 复用 Dynamic Power / AtRate 寄存器，需先清
 * Config2.AtRateEn 与 DPEn 才能读，读取后需恢复。 */

#ifndef MAX17260_USE_SERIAL_NUMBER
#define MAX17260_USE_SERIAL_NUMBER    1
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17260_VERIFY_WRITES —— 写后回读校验
 * ════════════════════════════════════════════════════════════════════════ */
/* 置 1 时每次写 R/W 寄存器后回读比较（Status / 序列号 / 只读寄存器自动排除），
 * 不一致返回 MAX17260_ERR_VERIFY。每次写多一笔 I2C 读，量产默认关闭。 */

#ifndef MAX17260_VERIFY_WRITES
#define MAX17260_VERIFY_WRITES      0
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17260_THREAD_SAFE —— 并发保护钩子
 * ════════════════════════════════════════════════════════════════════════ */
/* 置 1 时驱动在多笔总线访问的 API 前后调用 max17260_io_lock()/unlock()，
 * 由移植层实现（如 RTOS 互斥量）。默认线程安全性见 README：不同实例
 * 并发安全，同一实例需外部保护。单线程系统保持 0 零开销。 */

#ifndef MAX17260_THREAD_SAFE
#define MAX17260_THREAD_SAFE        0
#endif

#endif /* MAX17260_CONF_H */
