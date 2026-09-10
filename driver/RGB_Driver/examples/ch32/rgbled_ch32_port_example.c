/**
 * @file    rgbled_ch32_port_example.c
 * @brief   CH32 平台 rgbled_io_t 回调绑定示例（实现）
 * @details 给出 write/latch 两个回调的最小骨架：
 *          - write：把 3 个通道字节编码为 WS2812 波形经 SPI+DMA 发出
 *          - latch：等待不小于请求值的复位时间
 *          所有硬件操作均为占位示意，编译可直接通过，实际工程替换
 *          为自己的 BSP 调用。
 * @note    占位实现固定返回 RGBLED_IO_OK；真实实现必须处理超时等
 *          失败路径，且 write 返回时数据须已按位时序完整发出。
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 */

#include "rgbled_ch32_port_example.h"

#include <stddef.h>

/* ══════════════════════════════════════════════════════════════════════════
 * 底层回调实现（占位示意）
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   write 回调占位实现：把通道字节编码为 WS2812 波形发出。
 * @details 真实实现通常经 SPI+DMA 发送：先逐字节展开为波形缓冲，
 *          再启动传输并带超时等待完成（示意见函数体内注释块）。
 * @param   io_ctx  rgbled_init() 透传的板级上下文。
 * @param   data    已按 RGBLED_COLOR_ORDER 完成通道排序的字节序列。
 * @param   length  字节数，当前每颗灯恒为 3。
 * @retval  RGBLED_IO_OK     发送成功（占位实现恒返回成功）。
 * @retval  RGBLED_IO_ERROR  发送失败（超时、总线错误等，真实实现返回）。
 */
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

/**
 * @brief   latch 回调占位实现：保持数据线空闲，等待复位生效。
 * @details 实际等待时间不得小于 delay_us，否则灯带会把帧尾误判为
 *          新帧数据（示意见函数体内注释块）。
 * @param   io_ctx    rgbled_init() 透传的板级上下文。
 * @param   delay_us  要求的最短复位/锁存时间，单位微秒。
 * @retval  RGBLED_IO_OK     等待完成（占位实现恒返回成功）。
 * @retval  RGBLED_IO_ERROR  延时机制失败（真实实现返回）。
 */
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
    if ((io == NULL) || (ctx == NULL))
    {
        return RGBLED_IO_ERROR;
    }
    io->write = rgbled_ch32_write;
    io->latch = rgbled_ch32_latch;
    io->lock = NULL;      /* 多实例共享同一 SPI 时填互斥实现 */
    io->unlock = NULL;
    return RGBLED_IO_OK;
}
