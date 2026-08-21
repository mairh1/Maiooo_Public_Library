/**
 * @file    drv_io_template.h
 * @brief   通用驱动移植契约模板
 * @details 复制后把 drv_xxx_ / DRV_XXX_ 前缀替换为实际模块前缀，按芯片实际
 *          需要增删函数，保持「必选函数最少、可选函数按配置裁剪」。
 *          本头文件只被驱动核心 include；移植层实现文件建议放 port/ 目录。
 * @note    契约三要素缺一不可：返回码语义、时序要求、调用上下文。
 *          可选函数区块依赖 conf 宏，本文件直接包含自己的 _conf.h 保证自包含。
 * @author  （填写作者）
 * @version 1.0
 * @date    2026-08-21
 */
#ifndef DRV_IO_TEMPLATE_H
#define DRV_IO_TEMPLATE_H

#include <stdint.h>

#include "drv_xxx_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════ 移植层返回码 ══════════════════════════ */

#define DRV_IO_OK               0       /**< 操作成功 */
#define DRV_IO_ERROR            (-1)    /**< 通用错误：总线无应答 / 超时 / 参数非法 */

/* io_ctx 说明：
 * 驱动核心不关心总线句柄的具体类型——init 时由调用者传入、原样透传给全部
 * io 函数。单总线工程可传 NULL。移植层内部自行转换为目标句柄类型。 */

/* ══════════════════════════ 必选函数（4 个） ══════════════════════════ */

/**
 * @brief   初始化移植层依赖的总线 / 外设资源
 * @details 若调用者保证已在驱动外完成总线初始化，可实现为空函数直接返回
 *          DRV_IO_OK。核心在 drv_xxx_init() 中调用一次。
 * @param   io_ctx 总线上下文，原样透传，允许为 NULL
 * @retval  DRV_IO_OK 初始化成功
 * @retval  DRV_IO_ERROR 总线初始化失败
 */
int drv_xxx_io_init(void *io_ctx);

/**
 * @brief   读器件寄存器（单字节）
 * @details 线程与 ISR 上下文均可调用（若总线驱动自身支持）；内部不得无限
 *          等待，总线超时后返回 DRV_IO_ERROR。
 * @param   io_ctx 总线上下文
 * @param   dev_addr 7 位器件地址（片选型器件可当作片选编号使用）
 * @param   reg 目标寄存器地址
 * @param   val 读出值存放地址
 * @retval  DRV_IO_OK 读取成功
 * @retval  DRV_IO_ERROR 无应答 / 超时
 */
int drv_xxx_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg, uint8_t *val);

/**
 * @brief   写器件寄存器（单字节）
 * @details 线程上下文调用；内部不得无限等待。
 * @param   io_ctx 总线上下文
 * @param   dev_addr 7 位器件地址
 * @param   reg 目标寄存器地址
 * @param   val 待写入值
 * @retval  DRV_IO_OK 写入成功
 * @retval  DRV_IO_ERROR 无应答 / 超时
 */
int drv_xxx_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);

/**
 * @brief   毫秒延时
 * @note    时序契约：实际延时不允许小于请求值（宁长勿短）。
 *          核心仅在普通线程上下文调用，不得在 ISR 中调用。
 * @param   ms 延时毫秒数
 */
void drv_xxx_io_delay_ms(uint32_t ms);

/* ══════════════════════════ 可选函数（按 conf 裁剪） ══════════════════════════ */

#if DRV_USE_BURST

/**
 * @brief   连续读多个寄存器（可选）
 * @details 仅当 conf 中 DRV_USE_BURST 置 1 时才要求实现；不支持突发传输的
 *          平台可循环调用单字节读实现。
 * @param   io_ctx 总线上下文
 * @param   dev_addr 7 位器件地址
 * @param   reg 起始寄存器地址
 * @param   buf 读出数据存放地址
 * @param   len 读取字节数
 * @retval  DRV_IO_OK 读取成功
 * @retval  DRV_IO_ERROR 无应答 / 超时
 */
int drv_xxx_io_read_burst(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint16_t len);

/**
 * @brief   连续写多个寄存器（可选）
 * @details 仅当 conf 中 DRV_USE_BURST 置 1 时才要求实现；芯片手册禁止突发
 *          访问的寄存器不得纳入突发路径（由核心侧保证不走此函数）。
 * @param   io_ctx 总线上下文
 * @param   dev_addr 7 位器件地址
 * @param   reg 起始寄存器地址
 * @param   buf 待写数据
 * @param   len 写入字节数
 * @retval  DRV_IO_OK 写入成功
 * @retval  DRV_IO_ERROR 无应答 / 超时
 */
int drv_xxx_io_write_burst(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                           const uint8_t *buf, uint16_t len);

#endif /* DRV_USE_BURST */

#if DRV_THREAD_SAFE

/**
 * @brief   互斥上锁（可选，DRV_THREAD_SAFE=1 时必须实现）
 * @details 保护同一实例的读-改-写序列。裸机工程可实现为关全局中断；
 *          RTOS 工程建议实现为互斥量。锁的粒度与嵌套语义由移植层自行保证。
 * @param   io_ctx 总线上下文
 */
void drv_xxx_io_lock(void *io_ctx);

/**
 * @brief   互斥解锁，与 drv_xxx_io_lock() 配对
 * @param   io_ctx 总线上下文
 */
void drv_xxx_io_unlock(void *io_ctx);

#endif /* DRV_THREAD_SAFE */

#ifdef __cplusplus
}
#endif

#endif /* DRV_IO_TEMPLATE_H */
