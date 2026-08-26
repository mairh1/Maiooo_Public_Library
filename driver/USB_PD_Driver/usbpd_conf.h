/**
 * @file    usbpd_conf.h
 * @brief   USB PD 协议栈编译期配置
 * @details 协议核心与移植层共用的裁剪/参数开关，全部为编译期宏：
 *          - PD 版本选择（PD2.0 / PD3.0）
 *          - 接入消抖次数
 *          - 收到 SRC_CAP 后默认申请的 PDO 序号
 *          - 日志开关与日志输出重定向
 * @note    修改本文件无需改动协议核心与移植层代码。
 * @author  Maiooo
 * @version 2.0.1
 * @date    2026-08-18
 */

#ifndef USBPD_CONF_H
#define USBPD_CONF_H

/* ═══════════════════════════════════════════════════
 *  协议参数配置
 * ═══════════════════════════════════════════════════ */

#define USBPD_PD30_ENABLE          0       /**< PD 版本：0 = PD2.0；1 = PD3.0（影响消息头 SpecRev 位） */

#define USBPD_DET_DEBOUNCE         5       /**< CC 接入消抖次数，连续检测成功该次数才判定接入 */

#define USBPD_DEFAULT_PDO_IDX      1       /**< 收到 SRC_CAP 后默认申请的 PDO 序号（1 起，通常 1 = 5V 档） */

#define USBPD_TX_RETRY_CNT         3       /**< 报文发送最大重试次数（等待 GoodCRC 失败后重发） */

#define USBPD_MSG_BUF_SIZE         34      /**< PD 单帧最大字节数（消息头 2 + 数据对象 7*4 + CRC 4 = 34） */

/* ═══════════════════════════════════════════════════
 *  日志配置
 * ═══════════════════════════════════════════════════ */

#define USBPD_LOG_ENABLE           0       /**< 日志开关：0 = 关闭（发布版本）；1 = 开启（调试版本） */

#if USBPD_LOG_ENABLE

/**
 * @brief  日志底层输出函数（开启日志时由用户平台层实现）
 * @param  fmt  格式串（调用点均为字符串字面量）
 * @note   协议栈不直接依赖 printf / stdio，用户在实现中自行对接
 *         串口、RTT 等输出通道；仅在主循环上下文被调用
 */
extern void usbpd_log_output(const char *fmt, ...);

/**
 * @brief  协议栈日志输出宏
 * @note   仅允许在主循环上下文（USBPD_Task 内）输出，中断与协议核心
 *         不做日志 IO，保证日志不破坏实时性
 */
#define USBPD_LOG(fmt, ...)        usbpd_log_output(fmt, ##__VA_ARGS__)

#else

#define USBPD_LOG(fmt, ...)                                  /**< 日志关闭时代码原地消除，零开销 */

#endif /* USBPD_LOG_ENABLE */

/* ═══════════════════════════════════════════════════
 *  缓冲对齐
 * ═══════════════════════════════════════════════════ */

/**
 * @brief  4 字节对齐属性
 * @note   发送缓冲地址会直接写入 DMA 地址寄存器，部分平台要求 4 字节
 *         对齐；非 GNUC 工具链编译协议核心时退化为无属性
 */
#if defined(__GNUC__)
#define USBPD_ALIGNED_4           __attribute__((aligned(4)))
#else
#define USBPD_ALIGNED_4
#endif

#endif /* USBPD_CONF_H */
