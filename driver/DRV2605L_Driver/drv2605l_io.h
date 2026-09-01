/**
 * @file    drv2605l_io.h
 * @brief   DRV2605L 驱动移植层 I/O 契约
 * @details 驱动核心只通过本文件访问 I2C 和延时服务。移植实现应放在
 *          port/ 目录或由工程提供同名函数。
 * @note    本器件为寄存器类 I2C 芯片，默认只要求四个必选函数，不要求
 *          burst 传输。所有总线操作必须有有限超时。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-01
 */

#ifndef DRV2605L_IO_H
#define DRV2605L_IO_H

#include <stdint.h>

#include "drv2605l_conf.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ══════════════════════════ 移植层返回码 ══════════════════════════ */

#define DRV2605L_IO_OK       0       /**< 操作成功 */
#define DRV2605L_IO_ERROR    (-1)    /**< 总线失败、无应答或超时 */

/* ══════════════════════════ 必选契约函数 ══════════════════════════ */

/**
 * @brief   初始化移植层依赖的 I2C 资源
 * @details 驱动初始化时调用一次；如果工程已在其它位置初始化 I2C，可直接
 *          返回 DRV2605L_IO_OK。函数不得无限等待。
 * @param   io_ctx 总线上下文，原样由调用者传入，允许为 NULL。
 * @retval  DRV2605L_IO_OK 初始化成功。
 * @retval  DRV2605L_IO_ERROR 初始化失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int drv2605l_io_init(void *io_ctx);

/**
 * @brief   读取一个 8 位器件寄存器
 * @details 应在一次 I2C 事务中完成“写寄存器地址 + 重启 + 读数据”。内部
 *          必须设置有限超时，不能因 BUSY 或无应答永久阻塞。
 * @param   io_ctx 总线上下文。
 * @param   dev_addr 7 位器件地址，核心传入 0x5A，不要在此处左移。
 * @param   reg 寄存器地址。
 * @param   val 读出值存放地址。
 * @retval  DRV2605L_IO_OK 读取成功。
 * @retval  DRV2605L_IO_ERROR 无应答、超时或其它通信失败。
 * @note    线程上下文调用；除非平台明确保证，否则禁止在 ISR 中调用。
 */
int drv2605l_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint8_t *val);

/**
 * @brief   写入一个 8 位器件寄存器
 * @details 事务完成或平台超时后必须返回，不能无限等待。调用方负责遵守
 *          DRV2605L 各寄存器的访问权限和时序限制。
 * @param   io_ctx 总线上下文。
 * @param   dev_addr 7 位器件地址，核心传入 0x5A，不要在此处左移。
 * @param   reg 寄存器地址。
 * @param   val 待写入值。
 * @retval  DRV2605L_IO_OK 写入成功。
 * @retval  DRV2605L_IO_ERROR 无应答、超时或其它通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int drv2605l_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t val);

/**
 * @brief   延时至少指定的毫秒数
 * @details 实际延时不得小于请求值。核心使用本函数满足上电等待和软件
 *          复位后的稳定时间。
 * @param   ms 请求延时，单位 ms。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void drv2605l_io_delay_ms(uint32_t ms);

/* ══════════════════════════ 可选并发契约 ══════════════════════════ */

#if DRV2605L_THREAD_SAFE

/**
 * @brief   锁定移植层资源
 * @details 保护同一实例的读-改-写序列和多笔寄存器事务。裸机可以关中断，
 *          RTOS 应使用互斥量；实现必须与 unlock 成对。
 * @param   io_ctx 总线上下文。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void drv2605l_io_lock(void *io_ctx);

/**
 * @brief   解锁移植层资源
 * @param   io_ctx 总线上下文。
 * @note    与 drv2605l_io_lock() 配对，线程上下文调用。
 */
void drv2605l_io_unlock(void *io_ctx);

#endif /* DRV2605L_THREAD_SAFE */

#ifdef __cplusplus
}
#endif

#endif /* DRV2605L_IO_H */
