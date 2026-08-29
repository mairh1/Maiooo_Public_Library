/**
 * @file    rgbled_conf.h
 * @brief   RGB 灯库编译期配置
 * @details 所有配置宏均可用编译器命令行 -D 覆盖，无需修改本文件。
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-08-29
 */

#ifndef RGBLED_CONF_H
#define RGBLED_CONF_H

/* 颜色字节顺序选项常量（非用户配置，勿用 -D 覆盖）。 */
#define RGBLED_ORDER_RGB       0
#define RGBLED_ORDER_GRB       1

/* 颜色字节在底层数据流中的顺序，按灯珠型号选择。 */
#ifndef RGBLED_COLOR_ORDER
#define RGBLED_COLOR_ORDER     RGBLED_ORDER_GRB
#endif

/* 关闭后可移除对应颜色工具，进一步缩小代码体积。 */
#ifndef RGBLED_ENABLE_HSV
#define RGBLED_ENABLE_HSV       1
#endif

#ifndef RGBLED_ENABLE_EFFECTS
#define RGBLED_ENABLE_EFFECTS   1
#endif

#ifndef RGBLED_LATCH_US
#define RGBLED_LATCH_US         80U
#endif

#if (RGBLED_COLOR_ORDER != RGBLED_ORDER_RGB) && \
    (RGBLED_COLOR_ORDER != RGBLED_ORDER_GRB)
#error "RGBLED_COLOR_ORDER must be RGBLED_ORDER_RGB or RGBLED_ORDER_GRB"
#endif

#if (RGBLED_ENABLE_HSV != 0) && (RGBLED_ENABLE_HSV != 1)
#error "RGBLED_ENABLE_HSV must be 0 or 1"
#endif

#if (RGBLED_ENABLE_EFFECTS != 0) && (RGBLED_ENABLE_EFFECTS != 1)
#error "RGBLED_ENABLE_EFFECTS must be 0 or 1"
#endif

#endif /* RGBLED_CONF_H */
