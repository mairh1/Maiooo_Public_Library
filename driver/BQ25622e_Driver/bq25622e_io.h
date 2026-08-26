/**
 * @file    bq25622e_io.h
 * @brief   BQ25622E 驱动移植层 I/O 接口
 * @details 本文件定义移植层契约：驱动核心（bq25622e.c）只调用
 *          下列函数访问硬件，不接触任何总线外设与厂商头文件。移植到新平台
 *          时仅需实现这些函数（可参考 port/bq25622e_io_template.c 模板）。
 *
 *          实现约束：
 *          - 16 位寄存器必须且只能在单次 I2C 事务内连续读写 2 个字节
 *            （小端：低字节地址在前），这是芯片手册的硬性要求，禁止拆成
 *            两次单字节访问；len 参数恒为 1（8 位寄存器）或 2（16 位寄存器），
 *            不需要更长的 burst 支持；
 *          - 手册规定任意两个 START 条件之间至少间隔 90 微秒。400 kHz 下
 *            一次 2 字节事务本身约 100~200 微秒，轮询式连续调用天然满足；
 *            若使用高速总线或 DMA 回调式驱动，请在事务之间保证该间隔
 *            （可借助 bq25622e_io_delay_us，或由驱动层的锁间接保证）；
 *          - dev_addr 为 7 位从机地址（默认 0x6B），不要自行左移一位——
 *            多数 I2C API 直接接受 7 位地址，8 位 API 请自行提供
 *            (dev_addr << 1) 之类的转换；
 *          - io_ctx 为不透明指针，取自 bq25622e_dev_t::io_ctx 原样透传，
 *            用于多总线设计区分 I2C 外设，单总线系统传 NULL 即可。
 * @note    依赖：bq25622e_conf.h。不依赖任何 app / bsp 层头文件。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-21
 */

#ifndef BQ25622E_IO_H
#define BQ25622E_IO_H

#include <stdint.h>

#include "bq25622e_conf.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════
 *  移植层返回码
 * ═══════════════════════════════════════════════════ */

#define BQ25622E_IO_OK          0       /**< 操作成功 */
#define BQ25622E_IO_ERROR       (-1)    /**< 操作失败（通信异常 / 超时 / NACK） */

/* ═══════════════════════════════════════════════════
 *  移植层接口函数
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   移植层一次性初始化（由 bq25622e_init() 调用一次）
 * @details 多数平台在别处已完成 I2C 外设初始化，直接返回
 *          BQ25622E_IO_OK 即可；需要在此使能时钟、配置引脚的平台
 *          在此实现。
 * @retval  BQ25622E_IO_OK 成功
 * @retval  BQ25622E_IO_ERROR 总线初始化失败
 */
int bq25622e_io_init(void);

/**
 * @brief   读寄存器（1 或 2 字节，单次事务）
 * @param   io_ctx   不透明总线句柄，原样透传自器件实例
 * @param   dev_addr 7 位从机地址（勿左移）
 * @param   reg      寄存器地址（逻辑寄存器的低字节地址）
 * @param   buf      读出数据存放缓冲区，小端序（buf[0] 为低字节）
 * @param   len      字节数，恒为 1 或 2
 * @retval  BQ25622E_IO_OK 成功
 * @retval  BQ25622E_IO_ERROR 通信失败
 */
int bq25622e_io_read_regs(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint8_t len);

/**
 * @brief   写寄存器（1 或 2 字节，单次事务）
 * @param   io_ctx   不透明总线句柄，原样透传自器件实例
 * @param   dev_addr 7 位从机地址（勿左移）
 * @param   reg      寄存器地址（逻辑寄存器的低字节地址）
 * @param   buf      待写数据，小端序（buf[0] 为低字节）
 * @param   len      字节数，恒为 1 或 2；len=2 时必须单次事务写完
 * @retval  BQ25622E_IO_OK 成功
 * @retval  BQ25622E_IO_ERROR 通信失败
 */
int bq25622e_io_write_regs(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                           const uint8_t *buf, uint8_t len);

/**
 * @brief   微秒延时（保证 I2C 事务间 90 微秒间隔用）
 * @details 仅当移植层的 I2C 实现无法天然满足事务间隔要求时才会被
 *          依赖；实现必须保证实际延时不少于请求值。轮询式驱动可
 *          提供空实现。
 * @param   us 延时微秒数
 */
void bq25622e_io_delay_us(uint32_t us);

#if BQ25622E_THREAD_SAFE
/**
 * @brief   并发保护钩子（仅 BQ25622E_THREAD_SAFE = 1 时需要实现）
 * @details 典型实现为 RTOS 互斥量的获取/释放，在完整的驱动 API
 *          调用外围成对调用。
 */
void bq25622e_io_lock(void);
void bq25622e_io_unlock(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BQ25622E_IO_H */
