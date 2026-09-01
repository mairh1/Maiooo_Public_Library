/**
 * @file    drv2605l_conf.h
 * @brief   DRV2605L 通用驱动配置
 * @details 所有配置集中在本文件，并使用 #ifndef 提供默认值，工程可以用
 *          编译器 -D 参数覆盖默认值，无需修改驱动核心。
 * @note    关闭功能开关时，对应公共 API 与核心实现会一并裁剪。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-01
 */

#ifndef DRV2605L_CONF_H
#define DRV2605L_CONF_H

/* ══════════════════════════ 功能裁剪 ══════════════════════════ */

#ifndef DRV2605L_USE_SEQUENCE
#define DRV2605L_USE_SEQUENCE       1   /**< ROM 波形序列器 API */
#endif

#ifndef DRV2605L_USE_RTP
#define DRV2605L_USE_RTP            1   /**< RTP 实时幅度 API */
#endif

#if (DRV2605L_USE_SEQUENCE != 0) && (DRV2605L_USE_SEQUENCE != 1)
#error "DRV2605L_USE_SEQUENCE 必须为 0 或 1"
#endif

#if (DRV2605L_USE_RTP != 0) && (DRV2605L_USE_RTP != 1)
#error "DRV2605L_USE_RTP 必须为 0 或 1"
#endif

/* ══════════════════════════ 初始化默认值 ══════════════════════════ */

#ifndef DRV2605L_DEFAULT_ACTUATOR
#define DRV2605L_DEFAULT_ACTUATOR   0   /**< 默认执行器：0=ERM，1=LRA */
#endif

#ifndef DRV2605L_DEFAULT_LIBRARY
#define DRV2605L_DEFAULT_LIBRARY     1   /**< 默认 ROM 库：1=TS2200 Library A */
#endif

#ifndef DRV2605L_DEFAULT_MODE
#define DRV2605L_DEFAULT_MODE        0   /**< 默认接口模式：0=内部触发 */
#endif

#ifndef DRV2605L_DEFAULT_STANDBY
#define DRV2605L_DEFAULT_STANDBY     1   /**< 初始化完成后进入软件待机 */
#endif

#if (DRV2605L_DEFAULT_ACTUATOR != 0) && (DRV2605L_DEFAULT_ACTUATOR != 1)
#error "DRV2605L_DEFAULT_ACTUATOR 必须为 0（ERM）或 1（LRA）"
#endif

#if (DRV2605L_DEFAULT_LIBRARY < 0) || (DRV2605L_DEFAULT_LIBRARY > 7)
#error "DRV2605L_DEFAULT_LIBRARY 必须为 0 到 7"
#endif

#if (DRV2605L_DEFAULT_MODE < 0) || (DRV2605L_DEFAULT_MODE > 7)
#error "DRV2605L_DEFAULT_MODE 必须为 0 到 7"
#endif

#if (DRV2605L_DEFAULT_STANDBY != 0) && (DRV2605L_DEFAULT_STANDBY != 1)
#error "DRV2605L_DEFAULT_STANDBY 必须为 0 或 1"
#endif

/* ══════════════════════════ 调试与并发 ══════════════════════════ */

#ifndef DRV2605L_VERIFY_WRITES
#define DRV2605L_VERIFY_WRITES       0   /**< 1=类型化写入后回读校验 */
#endif

#ifndef DRV2605L_THREAD_SAFE
#define DRV2605L_THREAD_SAFE         0   /**< 1=启用 IO 锁钩子 */
#endif

#if (DRV2605L_VERIFY_WRITES != 0) && (DRV2605L_VERIFY_WRITES != 1)
#error "DRV2605L_VERIFY_WRITES 必须为 0 或 1"
#endif

#if (DRV2605L_THREAD_SAFE != 0) && (DRV2605L_THREAD_SAFE != 1)
#error "DRV2605L_THREAD_SAFE 必须为 0 或 1"
#endif

#endif /* DRV2605L_CONF_H */
