/**
 * @file    rgbled_effects.c
 * @brief   RGB 灯库内置参考特效
 * @details 特效只改写像素缓冲，不触发发送；应用每帧调用
 *          rgbled_update() 后需自行调用 rgbled_show()。
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 */

#include "rgbled.h"

#if RGBLED_ENABLE_EFFECTS

/* ══════════════════════════════════════════════════════════════════════════
 * 私有工具函数
 * ══════════════════════════════════════════════════════════════════════════ */

static uint8_t rgbled_effect_wave(uint32_t now_ms)
{
    uint16_t phase = (uint16_t)(now_ms % 2000U);
    if (phase < 1000U) {
        return (uint8_t)((phase * 255U) / 1000U);
    }
    return (uint8_t)(((2000U - phase) * 255U) / 1000U);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 参考特效（公共 API，也可被应用直接调用）
 * ══════════════════════════════════════════════════════════════════════════ */

rgbled_result_t rgbled_effect_static(rgbled_dev_t *dev, uint32_t now_ms,
                                     uint32_t elapsed_ms, void *effect_ctx)
{
    const rgbled_color_t *color = (const rgbled_color_t *)effect_ctx;
    (void)now_ms;
    (void)elapsed_ms;
    if (color == NULL) {
        return RGBLED_ERR_PARAM;
    }
    return rgbled_fill(dev, *color);
}

rgbled_result_t rgbled_effect_breathe(rgbled_dev_t *dev, uint32_t now_ms,
                                      uint32_t elapsed_ms, void *effect_ctx)
{
    const rgbled_color_t *color = (const rgbled_color_t *)effect_ctx;
    (void)elapsed_ms;
    if (color == NULL) {
        return RGBLED_ERR_PARAM;
    }
    return rgbled_fill(dev, rgbled_color_scale(*color,
                                                rgbled_effect_wave(now_ms)));
}

rgbled_result_t rgbled_effect_rainbow(rgbled_dev_t *dev, uint32_t now_ms,
                                      uint32_t elapsed_ms, void *effect_ctx)
{
#if RGBLED_ENABLE_HSV
    uint16_t index;
    uint8_t offset;
    rgbled_result_t result;
#endif
    (void)elapsed_ms;
    (void)effect_ctx;
    if (dev == NULL) {
        return RGBLED_ERR_PARAM;
    }
#if RGBLED_ENABLE_HSV
    offset = (uint8_t)((now_ms / 20U) & 0xFFU);
    for (index = 0U; index < dev->count; ++index) {
        rgbled_hsv_t hsv;
        hsv.h = (uint8_t)(offset + (uint8_t)((index * 256U) / dev->count));
        hsv.s = 255U;
        hsv.v = 255U;
        result = rgbled_set_pixel(dev, index, rgbled_color_from_hsv(hsv));
        if (result != RGBLED_OK) {
            return result;
        }
    }
    return RGBLED_OK;
#else
    (void)now_ms;
    return RGBLED_ERR_NOT_SUPPORTED;
#endif
}

#endif /* RGBLED_ENABLE_EFFECTS */
