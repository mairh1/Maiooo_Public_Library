/**
 * @file    rgbled.h
 * @brief   独立、低内存、可扩展的 RGB 灯阵列驱动
 * @details 核心只管理像素和时间驱动效果，底层数据发送由 rgbled_io_t
 *          回调注入。该库不兼容或依赖 NeoPixel、FastLED、WS2812FX。
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 * @note    所有 API 运行于线程上下文，禁止在 ISR 中调用。
 */

#ifndef RGBLED_H
#define RGBLED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rgbled_conf.h"
#include "rgbled_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 结果码与公共类型
 * ══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RGBLED_OK = 0,             /**< 成功 */
    RGBLED_ERR_PARAM,          /**< 参数非法（空指针、越界） */
    RGBLED_ERR_NOT_READY,      /**< 未初始化 */
    RGBLED_ERR_IO,             /**< 底层发送失败，细节见 rgbled_get_last_io_error() */
    RGBLED_ERR_STATE,          /**< 当前状态不允许该操作（如未设置特效就 update） */
    RGBLED_ERR_NOT_SUPPORTED   /**< 功能被 conf 裁剪或器件不支持 */
} rgbled_result_t;

typedef struct {
    uint8_t r;                 /**< 红色分量 0~255 */
    uint8_t g;                 /**< 绿色分量 0~255 */
    uint8_t b;                 /**< 蓝色分量 0~255 */
} rgbled_color_t;

typedef struct {
    uint8_t h;                 /**< 色相 0~255（对应 0~360°） */
    uint8_t s;                 /**< 饱和度 0~255 */
    uint8_t v;                 /**< 明度 0~255 */
} rgbled_hsv_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 设备句柄与特效回调类型
 * ══════════════════════════════════════════════════════════════════════════ */

struct rgbled_dev;
typedef rgbled_result_t (*rgbled_effect_fn_t)(struct rgbled_dev *dev,
                                               uint32_t now_ms,
                                               uint32_t elapsed_ms,
                                               void *effect_ctx);

typedef struct rgbled_dev {
    rgbled_color_t       *pixels;       /**< 调用方提供的像素缓冲 */
    uint16_t              count;        /**< 灯珠数量 */
    uint8_t               brightness;   /**< 全局亮度 0~255，仅在 show 时应用 */
    uint8_t               initialized;  /**< init 成功标志 */
    uint8_t               effect_enabled; /**< 特效运行标志 */
    uint32_t              last_update_ms; /**< 上次 update 的时间戳 */
    rgbled_effect_fn_t    effect;       /**< 当前特效函数 */
    void                 *effect_ctx;   /**< 特效上下文，由用户管理 */
    const rgbled_io_t    *io;           /**< 底层回调集合 */
    void                 *io_ctx;       /**< 总线上下文，透传给 io 回调 */
    int                   last_io_error; /**< 最近一次 io 错误码 */
} rgbled_dev_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 公共 API：生命周期与像素操作
 * ══════════════════════════════════════════════════════════════════════════ */

rgbled_result_t rgbled_init(rgbled_dev_t *dev, rgbled_color_t *pixels,
                            uint16_t count, const rgbled_io_t *io,
                            void *io_ctx);
rgbled_result_t rgbled_deinit(rgbled_dev_t *dev);
rgbled_result_t rgbled_set_pixel(rgbled_dev_t *dev, uint16_t index,
                                 rgbled_color_t color);
rgbled_result_t rgbled_get_pixel(const rgbled_dev_t *dev, uint16_t index,
                                 rgbled_color_t *color);
rgbled_result_t rgbled_fill(rgbled_dev_t *dev, rgbled_color_t color);
rgbled_result_t rgbled_clear(rgbled_dev_t *dev);
rgbled_result_t rgbled_set_brightness(rgbled_dev_t *dev, uint8_t brightness);
rgbled_result_t rgbled_get_brightness(const rgbled_dev_t *dev,
                                       uint8_t *brightness);
rgbled_result_t rgbled_show(rgbled_dev_t *dev);
int rgbled_get_last_io_error(const rgbled_dev_t *dev);

/* ══════════════════════════════════════════════════════════════════════════
 * 公共 API：特效（RGBLED_ENABLE_EFFECTS = 1 时可用）
 * ══════════════════════════════════════════════════════════════════════════ */

#if RGBLED_ENABLE_EFFECTS
rgbled_result_t rgbled_set_effect(rgbled_dev_t *dev, rgbled_effect_fn_t effect,
                                  void *effect_ctx);
rgbled_result_t rgbled_stop_effect(rgbled_dev_t *dev);
rgbled_result_t rgbled_update(rgbled_dev_t *dev, uint32_t now_ms);
rgbled_result_t rgbled_effect_static(rgbled_dev_t *dev, uint32_t now_ms,
                                     uint32_t elapsed_ms, void *effect_ctx);
rgbled_result_t rgbled_effect_breathe(rgbled_dev_t *dev, uint32_t now_ms,
                                      uint32_t elapsed_ms, void *effect_ctx);
rgbled_result_t rgbled_effect_rainbow(rgbled_dev_t *dev, uint32_t now_ms,
                                      uint32_t elapsed_ms, void *effect_ctx);
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 公共 API：颜色工具（纯计算，不依赖设备实例）
 * ══════════════════════════════════════════════════════════════════════════ */

rgbled_color_t rgbled_color(uint8_t r, uint8_t g, uint8_t b);
rgbled_color_t rgbled_color_scale(rgbled_color_t color, uint8_t scale);
rgbled_color_t rgbled_color_mix(rgbled_color_t a, rgbled_color_t b,
                                uint8_t amount);
#if RGBLED_ENABLE_HSV
rgbled_color_t rgbled_color_from_hsv(rgbled_hsv_t hsv);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RGBLED_H */
