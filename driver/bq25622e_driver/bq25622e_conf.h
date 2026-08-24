/**
 * @file    bq25622e_conf.h
 * @brief   BQ25622E 驱动配置裁剪选项
 * @details 本文件作用等同 FatFS 的 ffconf.h：每个选项既可直接在此修改，
 *          也可通过编译命令行覆盖（如 -DBQ25622E_THREAD_SAFE=1），因为
 *          所有默认值均以 #ifndef 包裹。置 0 裁剪的功能，其相关 API 在
 *          编译期被移除（调用处返回 BQ25622E_ERR_NOT_SUPPORTED 或直接
 *          不存在），不产生任何代码与 RAM 开销。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-21
 */

#ifndef BQ25622E_CONF_H
#define BQ25622E_CONF_H

/* ═══════════════════════════════════════════════════
 *  功能裁剪开关
 * ═══════════════════════════════════════════════════ */

/* 片内 12 位 ADC 测量（REG26~REG36：IBUS/IBAT/VBUS/VPMID/VBAT/VSYS/TS/TDIE）
 * 置 0 后 adc_* 系列 API 不再编译，节省约 1 KB 代码空间                     */
#ifndef BQ25622E_USE_ADC
#define BQ25622E_USE_ADC    1
#endif

/* BATFET 控制（关机 / 船运模式 / 系统复位，REG18）
 * 注意：船运模式会切断电池与系统间的 BATFET，整机断电，调试时慎用           */
#ifndef BQ25622E_USE_BATFET
#define BQ25622E_USE_BATFET 1
#endif

/* ═══════════════════════════════════════════════════
 *  寄存器影子缓存
 * ═══════════════════════════════════════════════════ */
/* 1: 驱动为每个器件实例缓存全部 R/W 寄存器映像，并提供
 *    bq25622e_restore_settings() 一次性回写全部设置。用于看门狗超时后恢复
 *    （看门狗超时会把 R/W 寄存器复位为 POR 值且 ICHG 减半）。
 * 0: 每个实例节省 21 个字节的 uint16_t 数组，无恢复功能                      */

#ifndef BQ25622E_REG_SHADOW
#define BQ25622E_REG_SHADOW 1
#endif

/* ═══════════════════════════════════════════════════
 *  写后读回校验
 * ═══════════════════════════════════════════════════ */
/* 1: 每次 R/W 寄存器写入后立即读回比对（自清位除外），不一致返回
 *    BQ25622E_ERR_VERIFY。每次写多花一次 I2C 读事务——除非怀疑总线完整
 *    性问题，生产环境保持 0                                                 */

#ifndef BQ25622E_VERIFY_WRITES
#define BQ25622E_VERIFY_WRITES  0
#endif

/* ═══════════════════════════════════════════════════
 *  并发保护钩子
 * ═══════════════════════════════════════════════════ */
/* 1: 驱动在每个多事务 API 外围包裹 bq25622e_io_lock()/io_unlock()
 *    （由移植层实现，典型为 RTOS 互斥量）。单线程系统保持 0，零开销        */

#ifndef BQ25622E_THREAD_SAFE
#define BQ25622E_THREAD_SAFE   0
#endif

#endif /* BQ25622E_CONF_H */
