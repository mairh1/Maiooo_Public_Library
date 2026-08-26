/*
 * @file    max17048_io.h
 * @brief   MAX17048/MAX17049 驱动移植契约
 * @details 驱动核心（max17048.c）只调用本文件声明的函数，由使用者在
 *          目标平台实现。模板见 port/max17048_io_template.c。
 * @note    本文件只被驱动核心与移植层包含，不得反向包含 max17048.h。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 */

#ifndef MAX17048_IO_H
#define MAX17048_IO_H

#include <stdint.h>
#include "max17048_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 移植层返回码
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_IO_OK      0   /**< 操作成功 */
#define MAX17048_IO_ERROR   -1  /**< 任意通信失败（不透传平台错误码） */

/* ══════════════════════════════════════════════════════════════════════════
 * 移植契约函数（必选 4 个）
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   总线一次性初始化
 * @details max17048_init() 会调用一次。多数平台的 I2C 在别处初始化，
 *          直接返回 MAX17048_IO_OK 即可。
 * @retval  int  MAX17048_IO_OK 成功；MAX17048_IO_ERROR 失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int max17048_io_init(void);

/**
 * @brief   读一个 16 位寄存器
 * @details 必须在一次 I2C 事务内完成，格式（手册 Read Data Protocol）：
 *          S + 写地址 + A + 寄存器地址 + A + Sr + 读地址 + A +
 *          高字节数据 + A + 低字节数据 + N + P。
 *          两个字节必须同事务传输，否则读到的数据无效。
 * @param   io_ctx    总线上下文，原样取自 max17048_dev_t::io_ctx，
 *                    单总线系统传 NULL 即可。
 * @param   dev_addr  7 位从机地址（0x36），不要左移。
 * @param   reg       寄存器基地址（偶数地址，如 0x02）。
 * @param   val       输出：读到的 16 位值（高字节 << 8 | 低字节）。
 * @retval  int       MAX17048_IO_OK 成功；MAX17048_IO_ERROR 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int max17048_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                           uint16_t *val);

/**
 * @brief   写一个 16 位寄存器
 * @details 必须在一次 I2C 事务内完成，格式（手册 Write Data Protocol）：
 *          S + 写地址 + A + 寄存器地址 + A + 高字节数据 + A +
 *          低字节数据 + A + P。
 *          8 位写无效、不完整字节不写入。注意：向 CMD(0xFE) 写 0x5400
 *          触发全复位时器件不回 ACK，移植层可能返回 ERROR，属预期现象
 *          （驱动在该 API 中已忽略此错误）。
 * @param   io_ctx    总线上下文，同上。
 * @param   dev_addr  7 位从机地址（0x36），不要左移。
 * @param   reg       寄存器基地址（偶数地址）。
 * @param   val       待写 16 位值（高字节在前发送）。
 * @retval  int       MAX17048_IO_OK 成功；MAX17048_IO_ERROR 通信失败
 *                    （CMD 全复位写无 ACK 除外）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int max17048_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                            uint16_t val);

/**
 * @brief   毫秒级延时
 * @details 仅模型表加载序列使用（MAX17048_USE_MODEL_TABLE=1 时必选，
 *          其余配置可放空实现）。实际延时不得小于请求值。
 * @param   ms  延时毫秒数。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void max17048_io_delay_ms(uint32_t ms);

/* ══════════════════════════════════════════════════════════════════════════
 * 移植契约函数（可选，MAX17048_THREAD_SAFE=1 时必须实现）
 * ════════════════════════════════════════════════════════════════════════ */

#if MAX17048_THREAD_SAFE
/**
 * @brief   进入临界区
 * @details 典型实现为 RTOS 互斥量加锁，包裹驱动内多笔总线访问的 API。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void max17048_io_lock(void);

/**
 * @brief   退出临界区
 * @details   与 max17048_io_lock() 配对的解锁。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void max17048_io_unlock(void);
#endif /* MAX17048_THREAD_SAFE */

#ifdef __cplusplus
}
#endif

#endif /* MAX17048_IO_H */
