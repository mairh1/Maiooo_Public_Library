/**
 * @file    sgm41513_io.h
 * @brief   SGM41513 驱动的平台 I/O 移植契约
 * @details 驱动核心（sgm41513.c）只调用本文件声明的函数，由移植者在
 *          自己的平台上实现。可直接编辑的模板见
 *          port/sgm41513_io_template.c。
 *          - 契约函数计数：必选 4 个；SGM41513_THREAD_SAFE=1 时追加
 *            lock/unlock 2 个可选。
 *          - 驱动只做单字节寄存器读写，因为 REG09 与 REG0E 禁止突发
 *            (burst)（多字节）I2C 传输；移植层无需支持块传输。
 *          - 'io_ctx' 是不透明指针，每次调用时从 sgm41513_dev_t::io_ctx
 *            原样传入。多片 SGM41513 分布在不同总线时用它区分总线
 *            （只有一片时传 NULL 即可）。
 *          - 'dev_addr' 是取自 sgm41513_dev_t::dev_addr 的 7 位从机地址
 *            （0x1A）。不要自行左移——多数 I2C API 直接收 7 位地址，
 *            8 位 API 一般提供 (dev_addr << 1) 之类的宏。
 * @note    本文件只被驱动核心与移植层包含，不得反向包含 sgm41513.h。
 *          全部 io 契约函数均须在线程上下文调用，禁止在 ISR 中调用。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-10
 */

#ifndef SGM41513_IO_H
#define SGM41513_IO_H

#include <stdint.h>
#include "sgm41513_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * 移植层返回码
 * ═════════════════════════════════════════════════════════════════════════════ */

#define SGM41513_IO_OK      0   /**< 操作成功 */
#define SGM41513_IO_ERROR   -1  /**< 任意通信失败（不透传平台错误码） */

/* ═════════════════════════════════════════════════════════════════════════════
 * 移植契约函数（必选 4 个；SGM41513_THREAD_SAFE=1 时追加 lock/unlock
 * 2 个可选）
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   总线一次性初始化（可选实现）
 * @details sgm41513_init() 会调用一次。多数平台在别处初始化 I2C，
 *          即使无事可做也直接返回 SGM41513_IO_OK 即可。
 * @retval  int  SGM41513_IO_OK 成功；SGM41513_IO_ERROR 失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int sgm41513_io_init(void);

/**
 * @brief   读一个寄存器（必选）
 * @details 必须在一次 I2C 事务内完成：写 7 位地址 + 寄存器地址后，
 *          以重复起始(Repeated START)切换读方向取回 1 字节，中途不得
 *          发送 STOP。寄存器地址范围 0x00..0x0F，读 0x0F 以外越界
 *          返回 0xFF（器件行为）。
 * @param   io_ctx    总线上下文，原样取自 sgm41513_dev_t::io_ctx，
 *                    单总线系统为 NULL。
 * @param   dev_addr  7 位从机地址（0x1A），不要左移。
 * @param   reg       寄存器地址（0x00..0x0F）。
 * @param   val       输出：读到的寄存器值。
 * @retval  int       SGM41513_IO_OK 成功；SGM41513_IO_ERROR 通信失败
 *                    （失败时 *val 内容不确定）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int sgm41513_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint8_t *val);

/**
 * @brief   写一个寄存器（必选）
 * @details 必须在一次 I2C 事务内连续发送寄存器地址与 1 字节数据，
 *          以 STOP 结束。寄存器地址范围 0x00..0x0F。
 * @param   io_ctx    总线上下文，同上。
 * @param   dev_addr  7 位从机地址（0x1A），不要左移。
 * @param   reg       寄存器地址。
 * @param   val       待写入的寄存器值。
 * @retval  int       SGM41513_IO_OK 成功；SGM41513_IO_ERROR 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
int sgm41513_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t val);

/**
 * @brief   毫秒延时（必选提供；仅 OTG / 船运相关流程使用）
 * @details 仅 sgm41513_otg_enable() 需要（退出 HIZ 到使能升压(boost)
 *          之间 >= 30 ms，以及 tSM_DLY 相关等待）。
 *          从不调用 sgm41513_otg_enable() 时可以安全地做成空函数。
 * @param   ms  请求的延时时长，单位毫秒。
 * @note    实际延时不得小于请求值 ms。
 *          线程上下文调用，禁止在 ISR 中调用。
 */
void sgm41513_io_delay_ms(uint32_t ms);

/* ═════════════════════════════════════════════════════════════════════════════
 * 移植契约函数（可选；SGM41513_THREAD_SAFE=1 时必须实现）
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_THREAD_SAFE
/**
 * @brief   进入临界区
 * @details 可选并发保护钩子，仅当 sgm41513_conf.h 中
 *          SGM41513_THREAD_SAFE = 1 时参与编译。典型实现：RTOS 互斥锁
 *          take/give。调用位置在完整驱动 API 调用的前后，与
 *          sgm41513_io_unlock() 配对。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void sgm41513_io_lock(void);

/**
 * @brief   退出临界区
 * @details 与 sgm41513_io_lock() 配对的解锁。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
void sgm41513_io_unlock(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_IO_H */
