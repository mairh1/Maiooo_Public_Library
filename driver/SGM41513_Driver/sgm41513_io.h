/*
 * sgm41513_io.h - SGM41513 驱动的平台 I/O 移植契约。
 *
 * 本头文件定义移植层契约：驱动核心（sgm41513.c）只调用下列函数，
 * 由移植者在自己的平台上实现。可直接编辑的模板见
 * port/sgm41513_io_template.c。
 *
 * 实现说明：
 *  - 驱动只做单字节寄存器读写，因为 REG09 与 REG0E 禁止突发(burst)
 *    （多字节）I2C 传输；移植层无需支持块传输。
 *  - 'io_ctx' 是不透明指针，每次调用时从 sgm41513_dev_t::io_ctx 原样
 *    传入。多片 SGM41513 分布在不同总线时用它区分总线（只有一片时
 *    传 NULL 即可）。
 *  - 'dev_addr' 是取自 sgm41513_dev_t::dev_addr 的 7 位从机地址
 *    （0x1A）。不要自行左移——多数 I2C API 直接收 7 位地址，
 *    8 位 API 一般提供 (dev_addr << 1) 之类的宏。
 */

#ifndef SGM41513_IO_H
#define SGM41513_IO_H

#include <stdint.h>
#include "sgm41513_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 移植层实现的 io 函数统一返回码 */
#define SGM41513_IO_OK      0   /* 成功 */
#define SGM41513_IO_ERROR  -1   /* 任意通信失败 */

/*
 * 可选的总线一次性初始化（sgm41513_init() 调用一次）。
 * 即使无事可做也返回 SGM41513_IO_OK。多数平台在别处初始化 I2C，
 * 直接返回 OK 即可。
 */
int sgm41513_io_init(void);

/*
 * 读一个寄存器：dev_addr[7 位]，寄存器地址 0x00..0x0F。
 * 值存入 *val。成功返回 SGM41513_IO_OK。
 */
int sgm41513_io_read_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint8_t *val);

/*
 * 写一个寄存器：dev_addr[7 位]，寄存器地址 0x00..0x0F，写入值。
 * 成功返回 SGM41513_IO_OK。
 */
int sgm41513_io_write_reg(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t val);

/*
 * 毫秒延时。仅 OTG 启动序列需要（退出 HIZ 到使能升压(boost)之间
 * >= 30 ms，以及 tSM_DLY 相关等待）。从不调用 sgm41513_otg_enable()
 * 时可以安全地做成空函数。
 */
void sgm41513_io_delay_ms(uint32_t ms);

#if SGM41513_THREAD_SAFE
/*
 * 可选并发保护钩子，仅当 sgm41513_conf.h 中 SGM41513_THREAD_SAFE = 1
 * 时参与编译。典型实现：RTOS 互斥锁 take/give。
 * 调用位置在完整驱动 API 调用的前后。
 */
void sgm41513_io_lock(void);
void sgm41513_io_unlock(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_IO_H */
