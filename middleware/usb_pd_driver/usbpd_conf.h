/**
 * @file    usbpd_conf.h
 * @brief   USB PD 2.0 功能裁剪及毫秒级时序
 * @details 依据 PD 2.0 v1.3 表 6-32、表 7-22/7-23 及 Type-C 2.0。
 * @note    -D 优先；USBPD_USER_CONF_HEADER 可指定工程配置头。
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-09-28
 */
#ifndef USBPD_CONF_H
#define USBPD_CONF_H
#ifdef USBPD_USER_CONF_HEADER
#include USBPD_USER_CONF_HEADER
#endif

/* ═══════════════════ 功能裁剪 ═══════════════════ */
#ifndef USBPD_SINK_ENABLE
#define USBPD_SINK_ENABLE 1 /**< 支持受电 */
#endif
#ifndef USBPD_SOURCE_ENABLE
#define USBPD_SOURCE_ENABLE 1 /**< 支持供电 */
#endif
#ifndef USBPD_DRP_ENABLE
#define USBPD_DRP_ENABLE (USBPD_SINK_ENABLE && USBPD_SOURCE_ENABLE) /**< 自动探测角色 */
#endif
#ifndef USBPD_PR_SWAP_ENABLE
#define USBPD_PR_SWAP_ENABLE (USBPD_SINK_ENABLE && USBPD_SOURCE_ENABLE) /**< 交换电源角色 */
#endif
#if !USBPD_SINK_ENABLE && !USBPD_SOURCE_ENABLE
#error "USBPD requires at least one power role"
#endif
#if (USBPD_DRP_ENABLE || USBPD_PR_SWAP_ENABLE) && \
    (!USBPD_SINK_ENABLE || !USBPD_SOURCE_ENABLE)
#error "DRP and PR_Swap require both roles"
#endif

/* ═══════════════════ 调度与时序（ms） ═══════════════════ */
#ifndef USBPD_EVENT_BUDGET
#define USBPD_EVENT_BUDGET 4U /**< 每次 Task 最多处理事件数 */
#endif
#ifndef USBPD_T_CC_DEBOUNCE
#define USBPD_T_CC_DEBOUNCE 150U /**< 接入消抖，100..200ms */
#endif
#ifndef USBPD_T_PD_DEBOUNCE
#define USBPD_T_PD_DEBOUNCE 15U /**< 拔出消抖，10..20ms */
#endif
#ifndef USBPD_T_DRP_HALF
#define USBPD_T_DRP_HALF 40U /**< 对称 DRP 半周期，总周期 80ms */
#endif
#ifndef USBPD_T_DRP_JITTER
#define USBPD_T_DRP_JITTER 5U /**< 每半周期独立抖动，避免两个精确时钟端口锁步 */
#endif
#if USBPD_T_DRP_JITTER < 1 || USBPD_T_DRP_JITTER > 10 || \
    USBPD_T_DRP_HALF < (25 + USBPD_T_DRP_JITTER) || \
    USBPD_T_DRP_HALF + USBPD_T_DRP_JITTER > 49
#error "DRP jitter must keep each half-cycle within 25..49ms"
#endif
#ifndef USBPD_T_ERROR_RECOVERY
#define USBPD_T_ERROR_RECOVERY 30U /**< 错误恢复时 CC 开路，至少 25ms */
#endif
#ifndef USBPD_T_VCONN_RECOVERY
#define USBPD_T_VCONN_RECOVERY 250U /**< 曾供应 VCONN 时恢复至少等待 240ms */
#endif
#if USBPD_T_VCONN_RECOVERY < 240 || USBPD_T_VCONN_RECOVERY > 500
#error "Invalid VCONN error-recovery timing"
#endif
#ifndef USBPD_T_SENDER_RESPONSE
#define USBPD_T_SENDER_RESPONSE 27U /**< 应答等待，24..30ms */
#endif
#ifndef USBPD_T_SINK_WAIT_CAP
#define USBPD_T_SINK_WAIT_CAP 465U /**< Type-C 等待能力，310..620ms */
#endif
#ifndef USBPD_T_SEND_SOURCE_CAP
#define USBPD_T_SEND_SOURCE_CAP 150U /**< Type-C 能力重发间隔，100..200ms */
#endif
#ifndef USBPD_T_PS_TRANSITION
#define USBPD_T_PS_TRANSITION 500U /**< 等待 PS_RDY，450..550ms */
#endif
#ifndef USBPD_T_SRC_TRANSITION
#define USBPD_T_SRC_TRANSITION 30U /**< Source 调压前等待，25..35ms */
#endif
#ifndef USBPD_T_SRC_READY
#define USBPD_T_SRC_READY 270U /**< 电源就绪看门狗，小于规范 285ms 上限 */
#endif
#ifndef USBPD_T_PS_SOURCE_OFF
#define USBPD_T_PS_SOURCE_OFF 835U /**< 交换时等待旧 Source 关闭，750..920ms */
#endif
#ifndef USBPD_T_PS_SOURCE_ON
#define USBPD_T_PS_SOURCE_ON 435U /**< 交换时等待新 Source，390..480ms */
#endif
#ifndef USBPD_T_PS_HARD_RESET
#define USBPD_T_PS_HARD_RESET 30U /**< 硬复位断电前等待，25..35ms */
#endif
#ifndef USBPD_T_SRC_RECOVER
#define USBPD_T_SRC_RECOVER 750U /**< 达到 0V 后恢复等待，660..1000ms */
#endif
#ifndef USBPD_T_SAFE_ZERO
#define USBPD_T_SAFE_ZERO 640U /**< 关闭电源并放电的最大等待 */
#endif
#ifndef USBPD_T_SWAP_SOURCE_START
#define USBPD_T_SWAP_SOURCE_START 25U /**< 新 Source 发送能力前至少 20ms */
#endif
#ifndef USBPD_T_REQUEST_WAIT
#define USBPD_T_REQUEST_WAIT 100U /**< Wait 后再次申请的最短间隔 */
#endif
#ifndef USBPD_T_NO_RESPONSE
#define USBPD_T_NO_RESPONSE 5000U /**< PD 无响应窗口，4500..5500ms */
#endif
#ifndef USBPD_T_HARD_SIGNAL
#define USBPD_T_HARD_SIGNAL 5U /**< Hard Reset 信号完成看门狗 */
#endif
#if USBPD_T_HARD_SIGNAL != 5
#error "Hard Reset signaling must finish within 5ms"
#endif
#ifndef USBPD_T_TX_WATCHDOG
#define USBPD_T_TX_WATCHDOG 15U /**< 底层完成事件看门狗，不是 GoodCRC 时限 */
#endif
#ifndef USBPD_TX_RETRIES
#define USBPD_TX_RETRIES 3U /**< 首次发送之外的重试次数 */
#endif
#ifndef USBPD_HARD_RESET_LIMIT
#define USBPD_HARD_RESET_LIMIT 2U /**< 无响应硬复位上限 */
#endif
#ifndef USBPD_CAPS_LIMIT
#define USBPD_CAPS_LIMIT 50U /**< nCapsCount 上限 */
#endif

#if USBPD_EVENT_BUDGET < 1 || USBPD_EVENT_BUDGET > 32
#error "USBPD_EVENT_BUDGET must be 1..32"
#endif
#if USBPD_TX_RETRIES != 3 || USBPD_HARD_RESET_LIMIT != 2 || USBPD_CAPS_LIMIT != 50
#error "PD 2.0 protocol counters must not be changed"
#endif
#if USBPD_T_CC_DEBOUNCE < 100 || USBPD_T_CC_DEBOUNCE > 199
#error "Invalid USBPD_T_CC_DEBOUNCE (reserve task scheduling margin)"
#endif
#if USBPD_T_PD_DEBOUNCE < 10 || USBPD_T_PD_DEBOUNCE > 19
#error "Invalid USBPD_T_PD_DEBOUNCE (reserve task scheduling margin)"
#endif
#if USBPD_T_DRP_HALF < 25 || USBPD_T_DRP_HALF > 49
#error "Invalid USBPD_T_DRP_HALF (reserve task scheduling margin)"
#endif
#if USBPD_T_ERROR_RECOVERY < 25 || USBPD_T_ERROR_RECOVERY > 1000
#error "Invalid USBPD_T_ERROR_RECOVERY (reserve task scheduling margin)"
#endif
#if USBPD_T_SENDER_RESPONSE < 24 || USBPD_T_SENDER_RESPONSE > 29
#error "Invalid USBPD_T_SENDER_RESPONSE (reserve task scheduling margin)"
#endif
#if USBPD_T_SINK_WAIT_CAP < 310 || USBPD_T_SINK_WAIT_CAP > 619
#error "Invalid USBPD_T_SINK_WAIT_CAP (reserve task scheduling margin)"
#endif
#if USBPD_T_SEND_SOURCE_CAP < 100 || USBPD_T_SEND_SOURCE_CAP > 199
#error "Invalid USBPD_T_SEND_SOURCE_CAP (reserve task scheduling margin)"
#endif
#if USBPD_T_PS_TRANSITION < 450 || USBPD_T_PS_TRANSITION > 549
#error "Invalid USBPD_T_PS_TRANSITION (reserve task scheduling margin)"
#endif
#if USBPD_T_SRC_TRANSITION < 25 || USBPD_T_SRC_TRANSITION > 34
#error "Invalid USBPD_T_SRC_TRANSITION (reserve task scheduling margin)"
#endif
#if USBPD_T_SRC_READY < 1 || USBPD_T_SRC_READY > 274
#error "Invalid USBPD_T_SRC_READY (reserve task scheduling margin)"
#endif
#if USBPD_T_PS_SOURCE_OFF < 750 || USBPD_T_PS_SOURCE_OFF > 919
#error "Invalid USBPD_T_PS_SOURCE_OFF (reserve task scheduling margin)"
#endif
#if USBPD_T_PS_SOURCE_ON < 390 || USBPD_T_PS_SOURCE_ON > 479
#error "Invalid USBPD_T_PS_SOURCE_ON (reserve task scheduling margin)"
#endif
#if USBPD_T_PS_HARD_RESET < 25 || USBPD_T_PS_HARD_RESET > 34
#error "Invalid USBPD_T_PS_HARD_RESET (reserve task scheduling margin)"
#endif
#if USBPD_T_SRC_RECOVER < 660 || USBPD_T_SRC_RECOVER > 999
#error "Invalid USBPD_T_SRC_RECOVER (reserve task scheduling margin)"
#endif
#if USBPD_T_SAFE_ZERO < 1 || USBPD_T_SAFE_ZERO > 649
#error "Invalid USBPD_T_SAFE_ZERO (reserve task scheduling margin)"
#endif
#if USBPD_T_SWAP_SOURCE_START < 20 || USBPD_T_SWAP_SOURCE_START > 100
#error "Invalid USBPD_T_SWAP_SOURCE_START (reserve task scheduling margin)"
#endif
#if USBPD_T_REQUEST_WAIT < 100 || USBPD_T_REQUEST_WAIT > 1000
#error "Invalid USBPD_T_REQUEST_WAIT (reserve task scheduling margin)"
#endif
#if USBPD_T_NO_RESPONSE < 4500 || USBPD_T_NO_RESPONSE > 5499
#error "Invalid USBPD_T_NO_RESPONSE (reserve task scheduling margin)"
#endif
#if USBPD_T_TX_WATCHDOG < 1 || USBPD_T_TX_WATCHDOG > 15
#error "Invalid USBPD_T_TX_WATCHDOG (reserve task scheduling margin)"
#endif
#endif /* USBPD_CONF_H */
