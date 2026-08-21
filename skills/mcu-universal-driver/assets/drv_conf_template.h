/**
 * @file    drv_conf_template.h
 * @brief   通用驱动配置模板（单头文件功能裁剪）
 * @details 复制后把 DRV_ 前缀替换为实际模块前缀。全部宏必须 #ifndef 给默认值：
 *          - 工程可用编译器 -DRV_USE_BURST=0 覆盖，无需改库源码
 *          - 也可整体复制本文件修改默认值随库发布
 * @note    裁剪联动：置 0 一个开关后，对应公共 API 声明、核心实现、
 *          io 契约函数必须一并随 #if 消失，移植层不再被要求实现。
 * @author  （填写作者）
 * @version 1.0
 * @date    2026-08-21
 */
#ifndef DRV_CONF_TEMPLATE_H
#define DRV_CONF_TEMPLATE_H

/* ══════════════════════ 功能裁剪开关（1=启用，0=裁剪） ══════════════════════ */

#ifndef DRV_USE_BURST
#define DRV_USE_BURST           1   /**< 突发(块)传输 API；置 0 后移植层免实现 read/write_burst */
#endif

/* ══════════════════════ 器件变体选择 ══════════════════════ */

#ifndef DRV_VARIANT
#define DRV_VARIANT             0   /**< 子型号编号；硬件无法自识别的变体由此指定 */
#endif

/* ══════════════════════ 调试 / 校验选项 ══════════════════════ */

#ifndef DRV_VERIFY_WRITES
#define DRV_VERIFY_WRITES       0   /**< 1=写后读回校验（代价：每次写多一次读） */
#endif

/* ══════════════════════ 线程安全（OS 钩子） ══════════════════════ */

#ifndef DRV_THREAD_SAFE
#define DRV_THREAD_SAFE         0   /**< 1=核心在临界区前后调用 io_lock/io_unlock，
                                         移植层必须实现这两个钩子 */
#endif

#endif /* DRV_CONF_TEMPLATE_H */
