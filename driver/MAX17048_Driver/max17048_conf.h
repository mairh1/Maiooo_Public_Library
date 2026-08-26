/*
 * @file    max17048_conf.h
 * @brief   MAX17048/MAX17049 电量计驱动配置
 * @details 全部配置宏集中在本文件，均带 #ifndef 默认值，可通过编译器
 *          命令行 -D 覆盖，禁止修改驱动源码来配置。
 * @note    置 0 关闭某功能开关后，对应公共 API、核心实现与移植契约
 *          函数一起被 #if 裁剪，被裁剪 API 不再存在（编译期报错）。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 */

#ifndef MAX17048_CONF_H
#define MAX17048_CONF_H

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17048_VARIANT —— 芯片变体（决定 VCELL 换算的节数）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_VARIANT_MAX17048   0   /**< 1 节锂电，CELL 引脚不接，VDD 直接接电池 */
#define MAX17048_VARIANT_MAX17049   1   /**< 2 节锂电，CELL 接电池组正极，VDD 接 2.5~4.5V 电源 */

#ifndef MAX17048_VARIANT
#define MAX17048_VARIANT    MAX17048_VARIANT_MAX17048
#endif

#if (MAX17048_VARIANT != MAX17048_VARIANT_MAX17048) && \
    (MAX17048_VARIANT != MAX17048_VARIANT_MAX17049)
#error "MAX17048_VARIANT 必须为 MAX17048_VARIANT_MAX17048 或 MAX17048_VARIANT_MAX17049"
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17048_USE_MODEL_TABLE —— 自定义电池模型表加载（0x40~0x7F）
 * ════════════════════════════════════════════════════════════════════════ */
/* 置 1 提供 max17048_model_load()；置 0 裁剪该 API 与模型解锁逻辑，
 * 使用 POR 预置 ROM 模型的应用可置 0 省代码。 */

#ifndef MAX17048_USE_MODEL_TABLE
#define MAX17048_USE_MODEL_TABLE    1
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17048_VERIFY_WRITES —— 写后回读校验
 * ════════════════════════════════════════════════════════════════════════ */
/* 置 1 时每次写 R/W 寄存器后回读比较（易变位与只写字段自动排除），
 * 不一致返回 MAX17048_ERR_VERIFY。每次写多一笔 I2C 读，量产默认关闭。 */

#ifndef MAX17048_VERIFY_WRITES
#define MAX17048_VERIFY_WRITES      0
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17048_THREAD_SAFE —— 并发保护钩子
 * ════════════════════════════════════════════════════════════════════════ */
/* 置 1 时驱动在多笔总线访问的 API 前后调用 max17048_io_lock()/unlock()，
 * 由移植层实现（如 RTOS 互斥量）。默认线程安全性见 README：不同实例
 * 并发安全，同一实例需外部保护。单线程系统保持 0 零开销。 */

#ifndef MAX17048_THREAD_SAFE
#define MAX17048_THREAD_SAFE        0
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * RCOMP 温度补偿参数（手册 Temperature Compensation 节）
 * ════════════════════════════════════════════════════════════════════════ */

#ifndef MAX17048_RCOMP0
#define MAX17048_RCOMP0            0x97    /**< 20℃ 基准 RCOMP（POR 默认） */
#endif

#ifndef MAX17048_TEMPCO_UP_X10
#define MAX17048_TEMPCO_UP_X10     (-5)    /**< T > 20℃ 斜率 -0.5/℃（×10 表示） */
#endif

#ifndef MAX17048_TEMPCO_DOWN_X10
#define MAX17048_TEMPCO_DOWN_X10   (-50)   /**< T ≤ 20℃ 斜率 -5.0/℃（×10 表示） */
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * MAX17048_MODEL_DELAY_MS —— 模型表加载过程中的停顿
 * ════════════════════════════════════════════════════════════════════════ */
/* 解锁后与复锁前的等待时间，保证表数据在复锁前写入稳定。仅
 * MAX17048_USE_MODEL_TABLE = 1 时使用。 */

#ifndef MAX17048_MODEL_DELAY_MS
#define MAX17048_MODEL_DELAY_MS    10
#endif

#endif /* MAX17048_CONF_H */
