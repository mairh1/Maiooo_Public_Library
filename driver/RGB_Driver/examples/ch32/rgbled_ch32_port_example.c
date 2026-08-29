/**
 * @file    rgbled_ch32_port_example.c
 * @brief   CH32 平台 rgbled_io_t 回调绑定示例（实现）
 * @details 给出 write/latch 两个回调的最小骨架：
 *          - write：把 3 个通道字节编码为 WS2812 波形经 SPI+DMA 发出
 *          - latch：等待不小于请求值的复位时间
 *          所有硬件操作均为占位示意，编译可直接通过，实际工程替换
 *          为自己的 BSP 调用。
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 */

#include "rgbled_ch32_port_example.h"

#include <stddef.h>

/* ══════════════════════════════════════════════════════════════════════════
 * 底层回调实现（占位示意）
 * ══════════════════════════════════════════════════════════════════════════ */

static int rgbled_ch32_write(void *io_ctx, const uint8_t *data,
                             uint32_t length)
{
    rgbled_ch32_ctx_t *ctx = (rgbled_ch32_ctx_t *)io_ctx;
    (void)ctx;

    /* ── 占位示意：编码为 WS2812 波形后经 SPI+DMA 发出 ──────────────
    for (uint32_t i = 0U; i < length; ++i) {
        ws2812_encode_byte(ctx->dma_buffer, data[i]);   // 每 bit 8 个 SPI bit
    }
    spi_dma_start(ctx->spi_id, ctx->dma_buffer, length * 8U);
    if (spi_dma_wait_done(ctx->spi_id) != 0) {          // 必须带超时
        return RGBLED_IO_ERROR;
    }
    ──────────────────────────────────────────────────────────────────── */
    (void)data;
    (void)length;
    return RGBLED_IO_OK;
}

static int rgbled_ch32_latch(void *io_ctx, uint32_t delay_us)
{
    rgbled_ch32_ctx_t *ctx = (rgbled_ch32_ctx_t *)io_ctx;
    (void)ctx;

    /* ── 占位示意：保持数据线空闲，等待不小于 delay_us ──────────────
    timer_delay_us(delay_us);
    ──────────────────────────────────────────────────────────────────── */
    (void)delay_us;
    return RGBLED_IO_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 回调集合组装
 * ══════════════════════════════════════════════════════════════════════════ */

int rgbled_ch32_port_fill(rgbled_io_t *io, rgbled_ch32_ctx_t *ctx)
{
    if ((io == NULL) || (ctx == NULL)) {
        return RGBLED_IO_ERROR;
    }
    io->write = rgbled_ch32_write;
    io->latch = rgbled_ch32_latch;
    io->lock = NULL;      /* 多实例共享同一 SPI 时填互斥实现 */
    io->unlock = NULL;
    return RGBLED_IO_OK;
}
