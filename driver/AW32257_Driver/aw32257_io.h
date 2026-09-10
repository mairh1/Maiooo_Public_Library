/**
 * @file    aw32257_io.h
 * @brief   AW32257 固定的平台 I/O 移植契约。
 * @details 可移植核心（aw32257.c）只调用下列函数；在目标平台的移植
 *          层实现它们，模板见 port/aw32257_io_template.c。三个契约
 *          函数全部必选（Required），本驱动没有可选钩子。
 * @note    本文件只应被驱动核心 aw32257.c 与移植层包含；公共头
 *          aw32257.h 不包含本文件。上下文经 void *io_ctx 透传，
 *          本契约不感知上层类型，也不包含 aw32257.h。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 */
#ifndef AW32257_IO_H
#define AW32257_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════ 移植层返回码 ══════════════════════════ */

/*
 * 读写契约函数成功固定返回 0；失败时返回 AW32257_IO_ERROR 或平台
 * 原始错误码。本驱动刻意透传原始码：核心只判非零，并把该值原样
 * 保存，供应用经 aw32257_get_last_port_error() 诊断通信故障。
 */
#define AW32257_IO_OK       0   /**< 操作成功 */
#define AW32257_IO_ERROR   -1   /**< 通用失败码；实现也可返回平台原始错误码 */

/* ══════════════════════════ 移植契约函数（全部必选） ══════════════════════════ */

/**
 * @brief   用核心提供的 7 位器件地址读一个寄存器。
 * @details 必须在一次 I2C 事务内完成，格式（手册随机地址读协议）：
 *          S + 写地址 + 寄存器地址 + Sr + 读地址 + 数据字节 + P，
 *          或手册允许的 STOP+START 读序列。整个事务必须在
 *          @p timeout_ms 内完成：等待 BUSY、ACK、事件或标志位时
 *          必须带硬性超时，禁止无限等待。
 * @param   io_ctx           驱动实例透传的总线上下文，由移植层解释。
 * @param   address_7bit     7 位器件地址（0x6A），不要左移。
 * @param   register_address 寄存器地址（REG00 到 REG0A）。
 * @param   value            输出：读到的数据字节。
 * @retval  int32_t          成功返回 0（AW32257_IO_OK）；失败返回 -1
 *                           （AW32257_IO_ERROR）或平台原始错误码——
 *                           核心会保存该值供
 *                           aw32257_get_last_port_error() 诊断。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int32_t
aw32257_io_read_reg(void * io_ctx,
                    uint8_t address_7bit,
                    uint8_t register_address,
                    uint8_t * value,
                    uint32_t timeout_ms);

/**
 * @brief   用核心提供的 7 位器件地址写一个寄存器。
 * @details 必须在一次 I2C 事务内完成，格式：S + 写地址 + 寄存器地址
 *          + 数据字节 + P。整个事务必须在 @p timeout_ms 内完成：
 *          等待 BUSY、ACK、事件或标志位时必须带硬性超时，禁止无限
 *          等待。实现内部不得重试，核心不自动重试。
 * @param   io_ctx           驱动实例透传的总线上下文，由移植层解释。
 * @param   address_7bit     7 位器件地址（0x6A），不要左移。
 * @param   register_address 寄存器地址（REG00 到 REG0A）。
 * @param   value            待写入的数据字节。
 * @retval  int32_t          成功返回 0（AW32257_IO_OK）；失败返回 -1
 *                           （AW32257_IO_ERROR）或平台原始错误码——
 *                           核心会保存该值供
 *                           aw32257_get_last_port_error() 诊断。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int32_t
aw32257_io_write_reg(void * io_ctx,
                     uint8_t address_7bit,
                     uint8_t register_address,
                     uint8_t value,
                     uint32_t timeout_ms);

/**
 * @brief   延时至少所请求的毫秒数。
 * @details 用于软件复位后的强制 I2C 静默等待，核心在 RESET 写事务
 *          发出后无条件调用（时长为 AW32257_SOFT_RESET_DELAY_MS =
 *          32 ms），不关心写事务是否报错——器件可能已接受 RESET 而
 *          只是 ACK 丢失。本函数没有返回值、不向核心上报错误，因此
 *          实现不得提前返回，也不能用可能提前唤醒的事件等待替代。
 * @param   io_ctx       驱动实例透传的上下文，由移植层解释。
 * @param   milliseconds 请求的最短延时，单位毫秒。
 * @note    线程/主循环上下文调用，禁止在 ISR 中调用。
 */
void
aw32257_io_delay_ms(void * io_ctx, uint32_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* AW32257_IO_H */
