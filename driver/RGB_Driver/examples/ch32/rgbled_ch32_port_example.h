/**
 * @file    rgbled_ch32_port_example.h
 * @brief   CH32 平台 rgbled_io_t 回调绑定示例（接口）
 * @details 演示如何把板级 SPI/DMA/定时器函数包装成 rgbled_io_t 的
 *          write/latch 回调。本示例不含任何 WCH 设备头文件，实际
 *          工程把占位示意替换为自己的 BSP 函数即可。
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 */

#ifndef RGBLED_CH32_PORT_EXAMPLE_H
#define RGBLED_CH32_PORT_EXAMPLE_H

#include <stdint.h>

#include "../../rgbled_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 板级上下文：字段按实际工程调整，回调里原样收回。 */
typedef struct {
    uint8_t spi_id;      /**< 占位：SPI 外设编号 */
    void   *dma_handle;  /**< 占位：DMA 通道句柄 */
} rgbled_ch32_ctx_t;

/**
 * @brief   填充 rgbled_io_t 回调集合。
 * @param   io   待填充的回调集合。
 * @param   ctx  板级上下文，将原样透传给各回调。
 * @retval  0  成功（RGBLED_IO_OK）。
 * @retval  -1 参数为空（RGBLED_IO_ERROR）。
 */
int rgbled_ch32_port_fill(rgbled_io_t *io, rgbled_ch32_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* RGBLED_CH32_PORT_EXAMPLE_H */
