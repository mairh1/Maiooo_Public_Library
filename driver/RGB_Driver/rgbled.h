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
extern "C"
{
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 结果码与公共类型
 * ══════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    RGBLED_OK = 0,             /**< 成功 */
    RGBLED_ERR_PARAM,          /**< 参数非法（空指针、越界） */
    RGBLED_ERR_NOT_READY,      /**< 未初始化 */
    RGBLED_ERR_IO,             /**< 底层发送失败，细节见 rgbled_get_last_io_error() */
    RGBLED_ERR_STATE,          /**< 当前状态不允许该操作（如未设置特效就 update） */
    RGBLED_ERR_NOT_SUPPORTED   /**< 功能被 conf 裁剪或器件不支持 */
} rgbled_result_t;

typedef struct
{
    uint8_t r;                 /**< 红色分量 0~255 */
    uint8_t g;                 /**< 绿色分量 0~255 */
    uint8_t b;                 /**< 蓝色分量 0~255 */
} rgbled_color_t;

typedef struct
{
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

typedef struct rgbled_dev
{
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

/**
 * @brief   初始化设备实例并绑定像素缓冲与底层回调。
 * @details 像素缓冲归调用方所有，驱动不复制；init 会把缓冲整体清零，
 *          保证首次 rgbled_show() 输出全黑而非随机数据。io 回调集合
 *          须为 const 且生命周期覆盖实例整个使用期。
 * @param   dev      设备句柄，由调用方定义分配，可多实例。
 * @param   pixels   调用方提供的像素缓冲，容量须不小于 count 颗灯。
 * @param   count    灯珠数量，须大于 0。
 * @param   io       底层回调集合，write 必选，其余按需提供。
 * @param   io_ctx   透传给 io 回调的总线上下文，无需要可为 NULL。
 * @retval  RGBLED_OK          初始化成功。
 * @retval  RGBLED_ERR_PARAM   dev/pixels/io 为 NULL 或 count 为 0。
 */
rgbled_result_t rgbled_init(rgbled_dev_t *dev, rgbled_color_t *pixels,
                            uint16_t count, const rgbled_io_t *io,
                            void *io_ctx);

/**
 * @brief   复位设备实例：清除初始化标志并停止、解绑特效。
 * @details 不触碰像素缓冲内容，也不操作底层硬件；实例可重新
 *          rgbled_init() 复用。
 * @param   dev  设备句柄，允许 NULL。
 * @retval  RGBLED_OK          已复位。
 * @retval  RGBLED_ERR_PARAM   dev 为 NULL。
 */
rgbled_result_t rgbled_deinit(rgbled_dev_t *dev);

/**
 * @brief   设置单颗灯的目标颜色（只写缓冲，不发送）。
 * @param   dev      已初始化的设备句柄。
 * @param   index    灯珠索引，从 0 起，须小于 count。
 * @param   color    目标颜色，各分量 0~255。
 * @retval  RGBLED_OK             写入成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL 或 index 越界。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_set_pixel(rgbled_dev_t *dev, uint16_t index,
                                 rgbled_color_t color);

/**
 * @brief   读取单颗灯当前缓冲颜色。
 * @param   dev      已初始化的设备句柄。
 * @param   index    灯珠索引，从 0 起，须小于 count。
 * @param   color    输出参数，接收读取到的颜色。
 * @retval  RGBLED_OK             读取成功。
 * @retval  RGBLED_ERR_PARAM      dev/color 为 NULL 或 index 越界。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_get_pixel(const rgbled_dev_t *dev, uint16_t index,
                                 rgbled_color_t *color);

/**
 * @brief   把全部灯珠设为同一颜色（只写缓冲，不发送）。
 * @param   dev    已初始化的设备句柄。
 * @param   color  目标颜色，各分量 0~255。
 * @retval  RGBLED_OK             填充成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_fill(rgbled_dev_t *dev, rgbled_color_t color);

/**
 * @brief   把全部灯珠设为黑色（只写缓冲，不发送）。
 * @param   dev  已初始化的设备句柄。
 * @retval  RGBLED_OK             清空成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_clear(rgbled_dev_t *dev);

/**
 * @brief   设置全局亮度系数（只写状态，不发送）。
 * @details 亮度只在 rgbled_show() 发送路径上按比例缩放后应用，
 *          不回写像素缓冲，因此 set 与 get 值始终一致。
 * @param   dev         已初始化的设备句柄。
 * @param   brightness  亮度系数 0~255，255 表示不缩放。
 * @retval  RGBLED_OK             设置成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_set_brightness(rgbled_dev_t *dev, uint8_t brightness);

/**
 * @brief   读取当前全局亮度系数。
 * @param   dev         已初始化的设备句柄。
 * @param   brightness  输出参数，接收亮度系数 0~255。
 * @retval  RGBLED_OK             读取成功。
 * @retval  RGBLED_ERR_PARAM      dev 或 brightness 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_get_brightness(const rgbled_dev_t *dev,
                                       uint8_t *brightness);

/**
 * @brief   按亮度缩放后把整帧像素逐灯发送到灯带并复位锁存。
 * @details 逐灯调用 io->write 发送 3 个通道字节，全部完成后调用
 *          io->latch 产生复位；任何一步失败都会先执行 unlock 再上抛
 *          RGBLED_ERR_IO，底层原始错误码经 rgbled_get_last_io_error()
 *          查询。
 * @param   dev  已初始化的设备句柄。
 * @retval  RGBLED_OK             整帧发送并锁存成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 * @retval  RGBLED_ERR_IO         write 或 latch 回调报告发送失败。
 */
rgbled_result_t rgbled_show(rgbled_dev_t *dev);

/**
 * @brief   查询最近一次底层 io 回调的原始返回码。
 * @param   dev  设备句柄，允许 NULL。
 * @retval  RGBLED_IO_OK     最近一次发送链路无错误。
 * @retval  RGBLED_IO_ERROR  dev 为 NULL 或底层回调报告失败。
 * @note    仅在上一个 API 返回 RGBLED_ERR_IO 后读取才有意义；
 *          移植层也可返回自定义非零值。
 */
int rgbled_get_last_io_error(const rgbled_dev_t *dev);

/* ══════════════════════════════════════════════════════════════════════════
 * 公共 API：特效（RGBLED_ENABLE_EFFECTS = 1 时可用）
 * ══════════════════════════════════════════════════════════════════════════ */

#if RGBLED_ENABLE_EFFECTS
/**
 * @brief   注册特效函数并启动特效。
 * @details 注册后需由应用周期性调用 rgbled_update() 推进帧；
 *          重复调用本函数即更换特效，时间戳重新从 0 计。
 * @param   dev         已初始化的设备句柄。
 * @param   effect      特效回调函数，不能为 NULL。
 * @param   effect_ctx  传给特效的上下文指针，由调用方管理，可为 NULL。
 * @retval  RGBLED_OK             注册成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL 或 effect 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_set_effect(rgbled_dev_t *dev, rgbled_effect_fn_t effect,
                                  void *effect_ctx);

/**
 * @brief   停止并解绑当前特效。
 * @details 只改状态，不清除像素缓冲；已写入的颜色保持不变。
 * @param   dev  已初始化的设备句柄。
 * @retval  RGBLED_OK             已停止。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_stop_effect(rgbled_dev_t *dev);

/**
 * @brief   推进一帧特效：计算与上次调用的时间差并执行特效回调。
 * @details elapsed 用无符号差值 now_ms - last_update_ms 计算，
 *          支持计时器回绕；特效只写像素缓冲，发送需另调 rgbled_show()。
 * @param   dev     已初始化且已 set_effect 的设备句柄。
 * @param   now_ms  当前时基（毫秒），通常取系统滴答计数。
 * @retval  RGBLED_OK             帧推进成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 * @retval  RGBLED_ERR_STATE      未设置特效时就调用本函数。
 * @retval  （其它）              透传特效回调自身的返回值。
 */
rgbled_result_t rgbled_update(rgbled_dev_t *dev, uint32_t now_ms);

/**
 * @brief   参考特效：静态显示 effect_ctx 指向的颜色。
 * @details 每帧把全部灯珠填充为同一颜色，不随时间变化。
 * @param   dev         已初始化的设备句柄。
 * @param   now_ms      当前时基（毫秒），本特效不使用。
 * @param   elapsed_ms  距上一帧的毫秒数，本特效不使用。
 * @param   effect_ctx  指向 rgbled_color_t 的基准颜色，NULL 报参数错误。
 * @retval  RGBLED_OK             填充成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL 或 effect_ctx 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_effect_static(rgbled_dev_t *dev, uint32_t now_ms,
                                     uint32_t elapsed_ms, void *effect_ctx);

/**
 * @brief   参考特效：以 2 秒周期呼吸显示 effect_ctx 指向的颜色。
 * @details 亮度取三角波：前 1000ms 自 0 线性升至 255，后 1000ms
 *          线性回落，相位由 now_ms 对 2000 取模得到，不依赖
 *          elapsed_ms。
 * @param   dev         已初始化的设备句柄。
 * @param   now_ms      当前时基（毫秒），决定呼吸相位。
 * @param   elapsed_ms  距上一帧的毫秒数，本特效不使用。
 * @param   effect_ctx  指向 rgbled_color_t 的基准颜色，NULL 报参数错误。
 * @retval  RGBLED_OK             填充成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL 或 effect_ctx 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 */
rgbled_result_t rgbled_effect_breathe(rgbled_dev_t *dev, uint32_t now_ms,
                                      uint32_t elapsed_ms, void *effect_ctx);

/**
 * @brief   参考特效：整条灯带流动彩虹。
 * @details 以 now_ms/20 生成整带公共色相偏移（约 12.75 圈/秒的
 *          8 位相位步进），灯珠 index 的色相再偏移 (index*256)/count，
 *          饱和度与明度固定 255，经整数 HSV 换算写入像素缓冲。
 * @param   dev         已初始化的设备句柄。
 * @param   now_ms      当前时基（毫秒），决定流动相位。
 * @param   elapsed_ms  距上一帧的毫秒数，本特效不使用。
 * @param   effect_ctx  本特效不使用，可为 NULL。
 * @retval  RGBLED_OK             填充成功。
 * @retval  RGBLED_ERR_PARAM      dev 为 NULL。
 * @retval  RGBLED_ERR_NOT_READY  实例未初始化。
 * @retval  RGBLED_ERR_NOT_SUPPORTED  conf 关闭 RGBLED_ENABLE_HSV。
 */
rgbled_result_t rgbled_effect_rainbow(rgbled_dev_t *dev, uint32_t now_ms,
                                      uint32_t elapsed_ms, void *effect_ctx);
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 公共 API：颜色工具（纯计算，不依赖设备实例）
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   用三个分量构造 rgbled_color_t。
 * @param   r  红色分量 0~255。
 * @param   g  绿色分量 0~255。
 * @param   b  蓝色分量 0~255。
 * @return  构造好的颜色值。
 */
rgbled_color_t rgbled_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief   按比例缩放颜色的三个分量（四舍五入）。
 * @details 与 show 发送路径使用同一缩放算法，可用来预览亮度效果。
 * @param   color  原始颜色。
 * @param   scale  缩放系数 0~255，255 表示不变，0 表示全黑。
 * @return  缩放后的颜色。
 */
rgbled_color_t rgbled_color_scale(rgbled_color_t color, uint8_t scale);

/**
 * @brief   按权重线性混合两个颜色（四舍五入）。
 * @details 每个分量按 a*(255-amount)/255 + b*amount/255 计算。
 * @param   a       基准颜色，amount=0 时的结果。
 * @param   b       目标颜色，amount=255 时的结果。
 * @param   amount  混合比例 0~255，取 b 的权重。
 * @return  混合后的颜色。
 */
rgbled_color_t rgbled_color_mix(rgbled_color_t a, rgbled_color_t b,
                                uint8_t amount);
#if RGBLED_ENABLE_HSV
/**
 * @brief   整数 HSV 转 RGB（纯计算，不依赖设备实例）。
 * @details 色相域 0~255 映射 0~360°，先除以 43 划分 6 个扇区，余数
 *          乘 6 得扇区内相位；p/q/t 为由明度 v 与饱和度 s 派生的
 *          三个中间分量。定点实现：乘法后右移 8 位近似除以 255，
 *          向零取整（截断），s=0 时次要通道可能比 v 低 1。
 * @param   hsv  输入 HSV，h/s/v 各分量 0~255。
 * @return  换算出的 RGB 颜色。
 */
rgbled_color_t rgbled_color_from_hsv(rgbled_hsv_t hsv);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RGBLED_H */
