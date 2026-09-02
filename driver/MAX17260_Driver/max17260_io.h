/**
 * @file    max17260_io.h
 * @brief   MAX17260 驱动移植契约
 * @details 驱动核心（max17260.c）只调用本文件声明的函数，由使用者在
 *          目标平台实现。模板见 port/max17260_io_template.c。
 * @note    本文件只被驱动核心与移植层包含，不得反向包含 max17260.h。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-02
 */

#ifndef MAX17260_IO_H
#define MAX17260_IO_H

#include <stdint.h>
#include "max17260_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 移植层返回码
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_IO_OK      0   /**< 操作成功 */
#define MAX17260_IO_ERROR   -1  /**< 任意通信失败（不透传平台错误码） */

/* ══════════════════════════════════════════════════════════════════════════
 * 移植契约函数（必选 4 个）
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   总线一次性初始化
 * @details max17260_init() 会调用一次。多数平台的 I2C 在别处初始化，
 *          直接返回 MAX17260_IO_OK 即可。
 * @retval  int  MAX17260_IO_OK 成功；MAX17260_IO_ERROR 失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int max17260_io_init(void);

/**
 * @brief   读一个 16 位寄存器
 * @details 必须在一次 I2C 事务内完成，格式（手册 Read Data Protocol）：
 *          S + 写地址 + A + 寄存器地址 + A + Sr + 读地址 + A +
 *          高字节数据 + A + 低字节数据 + N + P。
 *          两个字节必须同事务传输，否则读到的数据无效。
 * @param   io_ctx    总线上下文，原样取自 max17260_dev_t::io_ctx，
 *                    单总线系统传 NULL 即可。
 * @param   dev_addr  7 位从机地址（0x36 / 0x0D），不要左移。
 * @param   reg       寄存器基地址（手册 Table 17 中偶数地址，如 0x09）。
 *                    DieTemp（0x034）是 8 位地址，写入同样的字节即可。
 * @param   val       输出：读到的 16 位值（高字节 << 8 | 低字节）。
 * @retval  int       MAX17260_IO_OK 成功；MAX17260_IO_ERROR 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int max17260_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                           uint16_t *val);

/**
 * @brief   写一个 16 位寄存器
 * @details 必须在一次 I2C 事务内完成，格式（手册 Write Data Protocol）：
 *          S + 写地址 + A + 寄存器地址 + A + 高字节数据 + A +
 *          低字节数据 + A + P。
 *          8 位写无效、不完整字节不写入。
 * @param   io_ctx    总线上下文，同上。
 * @param   dev_addr  7 位从机地址，不要左移。
 * @param   reg       寄存器基地址（偶数地址）。
 * @param   val       待写 16 位值（高字节在前发送）。
 * @retval  int       MAX17260_IO_OK 成功；MAX17260_IO_ERROR 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int max17260_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                            uint16_t val);

/**
 * @brief   毫秒级延时
 * @details 当前驱动未使用延时函数（保留供未来序列使用）。实际延时
 *          不得小于请求值。
 * @param   ms  延时毫秒数。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void max17260_io_delay_ms(uint32_t ms);

/* ══════════════════════════════════════════════════════════════════════════
 * 移植契约函数（可选，MAX17260_THREAD_SAFE=1 时必须实现）
 * ════════════════════════════════════════════════════════════════════════ */

#if MAX17260_THREAD_SAFE
/**
 * @brief   进入临界区
 * @details 典型实现为 RTOS 互斥量加锁，包裹驱动内多笔总线访问的 API。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void max17260_io_lock(void);

/**
 * @brief   退出临界区
 * @details   与 max17260_io_lock() 配对的解锁。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void max17260_io_unlock(void);
#endif /* MAX17260_THREAD_SAFE */

#ifdef __cplusplus
}
#endif

#endif /* MAX17260_IO_H */
