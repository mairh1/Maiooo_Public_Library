/**
 * @file    rgbled.c
 * @brief   RGB 灯库平台无关核心实现
 * @details 实现要点：
 *          - 像素缓冲由调用者提供，init 时清零，避免首次 show 送出随机色
 *          - 亮度缩放只发生在 show 发送路径，不回写像素缓冲
 *          - 发送经 io 回调完成，任何失败路径都保证 lock/unlock 成对
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 */

#include "rgbled.h"

#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════
 * 私有工具函数
 * ══════════════════════════════════════════════════════════════════════════ */

static rgbled_result_t rgbled_require_ready(const rgbled_dev_t *dev)
{
    if (dev == NULL) {
        return RGBLED_ERR_PARAM;
    }
    return dev->initialized ? RGBLED_OK : RGBLED_ERR_NOT_READY;
}

static uint8_t rgbled_scale_channel(uint8_t value, uint8_t scale)
{
    return (uint8_t)(((uint16_t)value * scale + 127U) / 255U);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 生命周期
 * ══════════════════════════════════════════════════════════════════════════ */

rgbled_result_t rgbled_init(rgbled_dev_t *dev, rgbled_color_t *pixels,
                            uint16_t count, const rgbled_io_t *io,
                            void *io_ctx)
{
    if ((dev == NULL) || (pixels == NULL) || (count == 0U) ||
        (io == NULL) || (io->write == NULL)) {
        return RGBLED_ERR_PARAM;
    }

    /* 清零调用方缓冲，保证首次 show 输出全黑而非随机色。 */
    memset(pixels, 0, (size_t)count * sizeof(*pixels));

    dev->pixels = pixels;
    dev->count = count;
    dev->brightness = 255U;
    dev->initialized = 1U;
    dev->effect_enabled = 0U;
    dev->last_update_ms = 0U;
    dev->effect = NULL;
    dev->effect_ctx = NULL;
    dev->io = io;
    dev->io_ctx = io_ctx;
    dev->last_io_error = RGBLED_IO_OK;
    return RGBLED_OK;
}

rgbled_result_t rgbled_deinit(rgbled_dev_t *dev)
{
    if (dev == NULL) {
        return RGBLED_ERR_PARAM;
    }
    dev->initialized = 0U;
    dev->effect_enabled = 0U;
    dev->effect = NULL;
    dev->effect_ctx = NULL;
    return RGBLED_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 像素与亮度操作
 * ══════════════════════════════════════════════════════════════════════════ */

rgbled_result_t rgbled_set_pixel(rgbled_dev_t *dev, uint16_t index,
                                 rgbled_color_t color)
{
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }
    if (index >= dev->count) {
        return RGBLED_ERR_PARAM;
    }
    dev->pixels[index] = color;
    return RGBLED_OK;
}

rgbled_result_t rgbled_get_pixel(const rgbled_dev_t *dev, uint16_t index,
                                 rgbled_color_t *color)
{
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }
    if ((color == NULL) || (index >= dev->count)) {
        return RGBLED_ERR_PARAM;
    }
    *color = dev->pixels[index];
    return RGBLED_OK;
}

rgbled_result_t rgbled_fill(rgbled_dev_t *dev, rgbled_color_t color)
{
    uint16_t index;
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }
    for (index = 0U; index < dev->count; ++index) {
        dev->pixels[index] = color;
    }
    return RGBLED_OK;
}

rgbled_result_t rgbled_clear(rgbled_dev_t *dev)
{
    return rgbled_fill(dev, rgbled_color(0U, 0U, 0U));
}

rgbled_result_t rgbled_set_brightness(rgbled_dev_t *dev, uint8_t brightness)
{
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }
    dev->brightness = brightness;
    return RGBLED_OK;
}

rgbled_result_t rgbled_get_brightness(const rgbled_dev_t *dev,
                                       uint8_t *brightness)
{
    rgbled_result_t result = rgbled_require_ready(dev);
    if ((result != RGBLED_OK) || (brightness == NULL)) {
        return (result == RGBLED_OK) ? RGBLED_ERR_PARAM : result;
    }
    *brightness = dev->brightness;
    return RGBLED_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 数据发送
 * ══════════════════════════════════════════════════════════════════════════ */

rgbled_result_t rgbled_show(rgbled_dev_t *dev)
{
    uint16_t index;
    uint8_t channels[3];
    int io_result;
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }

    if (dev->io->lock != NULL) {
        dev->io->lock(dev->io_ctx);
    }
    for (index = 0U; index < dev->count; ++index) {
        rgbled_color_t color = rgbled_color_scale(dev->pixels[index],
                                                   dev->brightness);
#if RGBLED_COLOR_ORDER == RGBLED_ORDER_GRB
        channels[0] = color.g;
        channels[1] = color.r;
        channels[2] = color.b;
#else
        channels[0] = color.r;
        channels[1] = color.g;
        channels[2] = color.b;
#endif
        io_result = dev->io->write(dev->io_ctx, channels, 3U);
        if (io_result != RGBLED_IO_OK) {
            dev->last_io_error = io_result;
            if (dev->io->unlock != NULL) {
                dev->io->unlock(dev->io_ctx);
            }
            return RGBLED_ERR_IO;
        }
    }
    if (dev->io->latch != NULL) {
        io_result = dev->io->latch(dev->io_ctx, RGBLED_LATCH_US);
        if (io_result != RGBLED_IO_OK) {
            dev->last_io_error = io_result;
            if (dev->io->unlock != NULL) {
                dev->io->unlock(dev->io_ctx);
            }
            return RGBLED_ERR_IO;
        }
    }
    if (dev->io->unlock != NULL) {
        dev->io->unlock(dev->io_ctx);
    }
    dev->last_io_error = RGBLED_IO_OK;
    return RGBLED_OK;
}

int rgbled_get_last_io_error(const rgbled_dev_t *dev)
{
    return (dev == NULL) ? RGBLED_IO_ERROR : dev->last_io_error;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 特效控制
 * ══════════════════════════════════════════════════════════════════════════ */

#if RGBLED_ENABLE_EFFECTS
rgbled_result_t rgbled_set_effect(rgbled_dev_t *dev, rgbled_effect_fn_t effect,
                                  void *effect_ctx)
{
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }
    if (effect == NULL) {
        return RGBLED_ERR_PARAM;
    }
    dev->effect = effect;
    dev->effect_ctx = effect_ctx;
    dev->effect_enabled = 1U;
    dev->last_update_ms = 0U;
    return RGBLED_OK;
}

rgbled_result_t rgbled_stop_effect(rgbled_dev_t *dev)
{
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }
    dev->effect_enabled = 0U;
    dev->effect = NULL;
    dev->effect_ctx = NULL;
    return RGBLED_OK;
}

rgbled_result_t rgbled_update(rgbled_dev_t *dev, uint32_t now_ms)
{
    uint32_t elapsed_ms;
    rgbled_result_t result = rgbled_require_ready(dev);
    if (result != RGBLED_OK) {
        return result;
    }
    if ((dev->effect_enabled == 0U) || (dev->effect == NULL)) {
        return RGBLED_ERR_STATE;
    }
    elapsed_ms = now_ms - dev->last_update_ms;
    dev->last_update_ms = now_ms;
    return dev->effect(dev, now_ms, elapsed_ms, dev->effect_ctx);
}
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 颜色工具
 * ══════════════════════════════════════════════════════════════════════════ */

rgbled_color_t rgbled_color(uint8_t r, uint8_t g, uint8_t b)
{
    rgbled_color_t color = { r, g, b };
    return color;
}

rgbled_color_t rgbled_color_scale(rgbled_color_t color, uint8_t scale)
{
    color.r = rgbled_scale_channel(color.r, scale);
    color.g = rgbled_scale_channel(color.g, scale);
    color.b = rgbled_scale_channel(color.b, scale);
    return color;
}

rgbled_color_t rgbled_color_mix(rgbled_color_t a, rgbled_color_t b,
                                uint8_t amount)
{
    rgbled_color_t color;
    color.r = (uint8_t)(((uint16_t)a.r * (255U - amount) +
                         (uint16_t)b.r * amount + 127U) / 255U);
    color.g = (uint8_t)(((uint16_t)a.g * (255U - amount) +
                         (uint16_t)b.g * amount + 127U) / 255U);
    color.b = (uint8_t)(((uint16_t)a.b * (255U - amount) +
                         (uint16_t)b.b * amount + 127U) / 255U);
    return color;
}

#if RGBLED_ENABLE_HSV
rgbled_color_t rgbled_color_from_hsv(rgbled_hsv_t hsv)
{
    uint8_t region = (uint8_t)(hsv.h / 43U);
    uint8_t remainder = (uint8_t)((hsv.h - region * 43U) * 6U);
    uint8_t p = (uint8_t)(((uint16_t)hsv.v * (255U - hsv.s)) >> 8U);
    uint8_t q = (uint8_t)(((uint16_t)hsv.v *
                           (255U - ((uint16_t)hsv.s * remainder >> 8U))) >> 8U);
    uint8_t t = (uint8_t)(((uint16_t)hsv.v *
                           (255U - ((uint16_t)hsv.s * (255U - remainder) >> 8U))) >> 8U);
    switch (region) {
    case 0U: return rgbled_color(hsv.v, t, p);
    case 1U: return rgbled_color(q, hsv.v, p);
    case 2U: return rgbled_color(p, hsv.v, t);
    case 3U: return rgbled_color(p, q, hsv.v);
    case 4U: return rgbled_color(t, p, hsv.v);
    default: return rgbled_color(hsv.v, p, q);
    }
}
#endif
