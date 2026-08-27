/*
 * @file    ina219_io.h
 * @brief   INA219 驱动移植契约
 * @details 驱动核心（ina219.c）只调用本文件声明的函数，由使用者在
 *          目标平台实现。模板见 port/ina219_io_template.c。
 * @note    本文件只被驱动核心与移植层包含，不得反向包含 ina219.h。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 */

#ifndef INA219_IO_H
#define INA219_IO_H

#include <stdint.h>
#include "ina219_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 移植层返回码
 * ══════════════════════════════════════════════════════════════════════════ */

#define INA219_IO_OK       0   /**< 操作成功 */
#define INA219_IO_ERROR    -1  /**< 任意通信失败（不透传平台错误码） */

/* ══════════════════════════════════════════════════════════════════════════
 * 移植契约函数（必选 3 个 + 条件必选 1 个）
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   总线一次性初始化
 * @details ina219_init() 会调用一次。多数平台的 I2C 在别处初始化，
 *          直接返回 INA219_IO_OK 即可。
 * @retval  int  INA219_IO_OK 成功；INA219_IO_ERROR 失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int ina219_io_init(void);

/**
 * @brief   读一个 16 位寄存器
 * @details 必须在一次 I2C 事务内完成，格式（手册 Figure 16/19）：
 *          S + 写地址 + A + 寄存器指针 + A + Sr + 读地址 + A +
 *          高字节数据 + A + 低字节数据 + N + P。
 *          两个字节必须同事务传输；器件在读事务中保持指针，本驱动
 *          每次读前都会重新写指针，移植层无需维持指针状态。
 * @param   io_ctx    总线上下文，原样取自 ina219_dev_t::io_ctx，
 *                    单总线系统传 NULL 即可。
 * @param   dev_addr  7 位从机地址（0x40~0x4F），不要左移。
 * @param   reg       寄存器地址（0x00~0x05）。
 * @param   val       输出：读到的 16 位值（高字节 << 8 | 低字节）。
 * @retval  int       INA219_IO_OK 成功；INA219_IO_ERROR 无应答/超时。
 * @note    线程上下文调用，禁止在 ISR 中调用；内部不得无限等待，
 *          总线超时后必须返回（器件接口自身有 28ms 超时兼容 SMBus）。
 */
int ina219_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint16_t *val);

/**
 * @brief   写一个 16 位寄存器
 * @details 必须在一次 I2C 事务内完成，格式（手册 Figure 15）：
 *          S + 写地址 + A + 寄存器指针 + A + 高字节数据 + A +
 *          低字节数据 + A + P。高字节在前；不完整字节不写入。
 *          器件在写完成 4µs 后寄存器内容才生效（仅 SCL > 1MHz 时
 *          需在写后间隔再读；400kHz 及以下无影响）。
 * @param   io_ctx    总线上下文，同上。
 * @param   dev_addr  7 位从机地址，不要左移。
 * @param   reg       寄存器地址。
 * @param   val       待写 16 位值（高字节在前发送）。
 * @retval  int       INA219_IO_OK 成功；INA219_IO_ERROR 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int ina219_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint16_t val);

#if INA219_USE_TRIGGERED
/**
 * @brief   毫秒级延时（INA219_USE_TRIGGERED=1 时必选，其余配置可放空）
 * @details 仅 ina219_wait_conversion() 轮询等待使用。实际延时不允许
 *          小于请求值（宁长勿短）。
 * @param   ms  延时毫秒数。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void ina219_io_delay_ms(uint32_t ms);
#endif /* INA219_USE_TRIGGERED */

/* ══════════════════════════════════════════════════════════════════════════
 * 移植契约函数（可选，INA219_THREAD_SAFE=1 时必须实现）
 * ══════════════════════════════════════════════════════════════════════════ */

#if INA219_THREAD_SAFE
/**
 * @brief   进入临界区
 * @details 典型实现为 RTOS 互斥量加锁，包裹驱动内多笔总线访问的 API。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void ina219_io_lock(void);

/**
 * @brief   退出临界区
 * @details   与 ina219_io_lock() 配对的解锁。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void ina219_io_unlock(void);
#endif /* INA219_THREAD_SAFE */

#ifdef __cplusplus
}
#endif

#endif /* INA219_IO_H */
