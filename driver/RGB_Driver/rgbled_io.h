/**
 * @file    rgbled_io.h
 * @brief   RGB 灯库底层输出契约
 * @details 本文件只描述向灯带发送数据所需的回调，不包含任何 MCU、
 *          GPIO、SPI、DMA 或 RTOS 头文件。应用在 rgbled_init() 时通过
 *          rgbled_io_t 把传输实现注入驱动；一个实例绑定一组回调，
 *          不同灯带可各自使用不同传输后端（GPIO 位崩 / SPI / DMA 等）。
 * @note    本文件不得反向包含 rgbled.h。
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 */

#ifndef RGBLED_IO_H
#define RGBLED_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGBLED_IO_OK       0
#define RGBLED_IO_ERROR   (-1)

/**
 * @brief 发送一颗灯的通道字节序列。
 * @param io_ctx  调用方总线上下文，rgbled_init() 传入，原样透传。
 * @param data    已按 RGBLED_COLOR_ORDER 完成通道排序的 3 字节。
 * @param length  字节数，当前恒为 3。
 * @retval RGBLED_IO_OK    全部字节已按灯带位时序完整发出。
 * @retval RGBLED_IO_ERROR 任何发送失败（超时、总线错误等）。
 * @note 时序：须在调用返回前完成发送，且与前后 write 之间保持灯带
 *       要求的连续性（复位只会由 latch 产生）。线程上下文调用，
 *       不得在 ISR 中实现阻塞等待。
 */
typedef int (*rgbled_io_write_fn_t)(void *io_ctx,
                                    const uint8_t *data,
                                    uint32_t length);

/**
 * @brief 产生复位/锁存并阻塞等待灯带数据生效。
 * @param io_ctx    调用方总线上下文，原样透传。
 * @param delay_us  要求阻塞的微秒数（默认 RGBLED_LATCH_US）。
 * @retval RGBLED_IO_OK    等待完成，可以开始下一帧。
 * @retval RGBLED_IO_ERROR 延时机制失败（如定时器异常）。
 * @note 时序：实际等待时间不得小于 delay_us；WS2812 要求 >50us 复位，
 *       部分兼容灯珠需 280us 以上。线程上下文调用，不得在 ISR 中实现。
 */
typedef int (*rgbled_io_latch_fn_t)(void *io_ctx, uint32_t delay_us);

/**
 * @brief 进入发送临界区（可选，NULL 表示不加锁）。
 * @param io_ctx 调用方总线上下文，原样透传。
 * @note 仅在多实例共享同一物理发送通道时需要；整个 rgbled_show()
 *       期间持有，须与 unlock 成对实现且临界区尽量短（含全部 write
 *       与 latch，即完整一帧）。线程上下文调用。
 */
typedef void (*rgbled_io_lock_fn_t)(void *io_ctx);

/**
 * @brief 退出发送临界区（可选，与 lock 成对出现）。
 * @param io_ctx 调用方总线上下文，原样透传。
 * @note rgbled_show() 的任何失败路径都会调用 unlock，实现必须保证
 *       与 lock 严格成对。线程上下文调用。
 */
typedef void (*rgbled_io_unlock_fn_t)(void *io_ctx);

/**
 * @brief 底层输出回调集合，由应用填充后经 rgbled_init() 绑定。
 */
typedef struct {
    rgbled_io_write_fn_t  write;    /**< 必选：发送通道字节 */
    rgbled_io_latch_fn_t  latch;    /**< 建议提供：复位/锁存，NULL 则跳过 */
    rgbled_io_lock_fn_t   lock;     /**< 可选：共享通道保护，NULL 不加锁 */
    rgbled_io_unlock_fn_t unlock;   /**< 可选：与 lock 成对 */
} rgbled_io_t;

#ifdef __cplusplus
}
#endif

#endif /* RGBLED_IO_H */
