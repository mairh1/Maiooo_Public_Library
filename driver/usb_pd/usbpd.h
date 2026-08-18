/**
 * @file    usbpd.h
 * @brief   USB PD（Power Delivery）Sink 协议栈公有 API
 * @details 可移植的 USB PD 协议核心对外接口，实现与适配器/电源的
 *          PD 协商流程：
 *          - CC 线接入检测（带消抖）
 *          - SRC_CAP 解析（固定 PDO 的电压/电流）
 *          - REQUEST / ACCEPT / PS_RDY 协商状态机
 *          - 软复位 / 硬复位异常恢复
 *          - GET_SNK_CAP、GET_SRC_CAP_EX、GET_STATUS、VDM 等请求应答
 *
 *          分层结构与依赖方向：
 *
 *              +--------------------------------------+
 *              |  应用层                              |
 *              +--------------------------------------+
 *                        |  本 API（usbpd.h）
 *              +--------------------------------------+
 *              |  协议核心  usbpd.c + usbpd_conf.h    |  纯 C99，不含任何
 *              +--------------------------------------+  厂商头文件
 *                        |  移植接口（usbpd_io.h）
 *              +--------------------------------------+
 *              |  移植层（用户提供或使用现成移植）    |  寄存器 / 中断 /
 *              +--------------------------------------+  定时 / GPIO
 *
 * @note    集成步骤：
 *          1. 移植：实现 usbpd_io.h 全部函数（或直接使用
 *             usbpd_io_ch32l103.c 现成 CH32L103 移植）；
 *          2. 配置：按需调整 usbpd_conf.h；
 *          3. 接入：上电调用 USBPD_Init()，SysTick 1ms 中断调用
 *             USBPD_TickIsr()，主循环调用 USBPD_Task()。
 *          协议核心不依赖任何 app / bsp 层头文件。
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-08-18
 */

#ifndef USBPD_H
#define USBPD_H

#include <stdint.h>
#include "usbpd_conf.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════
 *  类型定义
 * ═══════════════════════════════════════════════════ */

/** @brief 执行结果码 */
typedef enum
{
    USBPD_OK             = 0,       /**< 执行成功 */
    USBPD_ERR_IO         = 1,       /**< 移植层初始化失败 */
    USBPD_ERR_PARAM      = 2,       /**< 参数非法（空指针 / 越界） */
    USBPD_ERR_NOT_READY  = 3,       /**< 协议栈未初始化或状态未就绪 */
} usbpd_result_t;

/** @brief CC 线检测结果 */
typedef enum
{
    USBPD_CC_NONE        = 0,       /**< 无接入 */
    USBPD_CC_LINE_1      = 1,       /**< CC1 接入（CH32L103 对应 PB6） */
    USBPD_CC_LINE_2      = 2,       /**< CC2 接入（CH32L103 对应 PB7） */
} usbpd_cc_line_t;

/** @brief 协议状态机状态（对外只读，由 USBPD_Task 推进） */
typedef enum
{
    USBPD_STA_IDLE               = 0,   /**< 空闲 */
    USBPD_STA_DISCONNECT         = 1,   /**< 已断开 */
    USBPD_STA_SRC_CONNECT        = 2,   /**< 检测到源接入 */
    USBPD_STA_RX_SRC_CAP_WAIT    = 3,   /**< 等待接收 SRC_CAP */
    USBPD_STA_RX_SRC_CAP         = 4,   /**< 已收到 SRC_CAP */
    USBPD_STA_TX_REQ             = 5,   /**< 发送 REQUEST */
    USBPD_STA_RX_ACCEPT_WAIT     = 6,   /**< 等待接收 ACCEPT */
    USBPD_STA_RX_ACCEPT          = 7,   /**< 已收到 ACCEPT */
    USBPD_STA_RX_REJECT          = 8,   /**< 收到 REJECT */
    USBPD_STA_RX_PS_RDY_WAIT     = 9,   /**< 等待接收 PS_RDY */
    USBPD_STA_RX_PS_RDY          = 10,  /**< 已收到 PS_RDY（协商完成） */
    USBPD_STA_SINK_CONNECT       = 11,  /**< 检测到 Sink 接入 */
    USBPD_STA_TX_SRC_CAP         = 12,  /**< 发送 SRC_CAP */
    USBPD_STA_RX_REQ_WAIT        = 13,  /**< 等待接收 REQUEST */
    USBPD_STA_RX_REQ             = 14,  /**< 已收到 REQUEST */
    USBPD_STA_TX_ACCEPT          = 15,  /**< 发送 ACCEPT */
    USBPD_STA_TX_REJECT          = 16,  /**< 发送 REJECT */
    USBPD_STA_ADJ_VOL            = 17,  /**< 调整输出电压电流 */
    USBPD_STA_TX_PS_RDY          = 18,  /**< 发送 PS_RDY */
    USBPD_STA_TX_DR_SWAP         = 19,  /**< 发送 DR_SWAP */
    USBPD_STA_RX_DR_SWAP_ACCEPT  = 20,  /**< 等待 DR_SWAP 的 ACCEPT 应答 */
    USBPD_STA_TX_PR_SWAP         = 21,  /**< 发送 PR_SWAP */
    USBPD_STA_RX_PR_SWAP_ACCEPT  = 22,  /**< 等待 PR_SWAP 的 ACCEPT 应答 */
    USBPD_STA_RX_PR_SWAP_PS_RDY  = 23,  /**< 等待 PR_SWAP 的 PS_RDY 应答 */
    USBPD_STA_TX_PR_SWAP_PS_RDY  = 24,  /**< 发送 PR_SWAP 的 PS_RDY 应答 */
    USBPD_STA_PR_SWAP_RECON_WAIT = 25,  /**< PR_SWAP 后等待重新连接 */
    USBPD_STA_SRC_RECON_WAIT     = 26,  /**< 等待源重新连接 */
    USBPD_STA_SINK_RECON_WAIT    = 27,  /**< 等待 Sink 重新连接 */
    USBPD_STA_RX_APD_PS_RDY_WAIT = 28,  /**< 等待适配器 PS_RDY */
    USBPD_STA_RX_APD_PS_RDY      = 29,  /**< 已收到适配器 PS_RDY */
    USBPD_STA_MODE_SWITCH        = 30,  /**< 模式切换中 */
    USBPD_STA_TX_SOFTRST         = 31,  /**< 发送软复位 */
    USBPD_STA_TX_HRST            = 32,  /**< 发送硬复位 */
    USBPD_STA_PHY_RST            = 33,  /**< PHY 复位 */
    USBPD_STA_APD_IDLE_WAIT      = 34,  /**< 等待适配器空闲 */
} usbpd_state_t;

/** @brief 固定 PDO 解析结果 */
typedef struct
{
    uint16_t voltage_mv;                   /**< 电压（mV，50mV 步进解析） */
    uint16_t current_ma;                   /**< 电流（mA，10mA 步进解析） */
} usbpd_pdo_t;

/** @brief 协议事件类型（经回调上报应用层） */
typedef enum
{
    USBPD_EVT_ATTACHED      = 0,       /**< 源接入，info->cc_line 有效 */
    USBPD_EVT_SRCCAP_READY  = 1,       /**< 收到并解析完 SRC_CAP，info->pdo_list / pdo_count 有效 */
    USBPD_EVT_PS_READY      = 2,       /**< 协商完成（PS_RDY），info 三字段有效 */
    USBPD_EVT_TX_FAILED     = 3,       /**< 报文发送重试耗尽 */
    USBPD_EVT_PHY_RESET     = 4,       /**< 收到硬复位（中断中检测，任务上下文上报） */
    USBPD_EVT_BUF_ERROR     = 5,       /**< 收发缓冲 / DMA 错误（中断中检测，任务上下文上报） */
} usbpd_event_t;

/** @brief 事件附带信息，按事件类型取用对应字段 */
typedef struct
{
    usbpd_cc_line_t    cc_line;           /**< 接入的 CC 线（EVT_ATTACHED） */
    const usbpd_pdo_t *pdo_list;          /**< 解析后的 PDO 数组（EVT_SRCCAP_READY） */
    uint8_t            pdo_count;         /**< PDO 数量（EVT_SRCCAP_READY） */
    uint8_t            pdo_index;         /**< 本次申请的 PDO 序号，1 起（EVT_PS_READY） */
    uint16_t           voltage_mv;        /**< 协商电压（EVT_PS_READY） */
    uint16_t           current_ma;        /**< 协商电流（EVT_PS_READY） */
} usbpd_event_info_t;

/** @brief 事件回调函数原型 */
typedef void (*usbpd_event_cb_t)(usbpd_event_t evt, const usbpd_event_info_t *info);

/* ═══════════════════════════════════════════════════
 *  公有 API
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   初始化 USB PD 协议栈
 * @details 依次完成：移植层初始化（外设时钟 / GPIO / 寄存器）、
 *          协议变量复位、PHY 复位并进入接收模式、使能中断。
 *          上电调用一次。
 * @retval  USBPD_OK         初始化成功
 * @retval  USBPD_ERR_IO     移植层初始化失败
 */
usbpd_result_t USBPD_Init(void);

/**
 * @brief   1ms 节拍喂入（SysTick 中断服务中调用）
 * @details 仅递增一个 volatile 计数器，不做任何其他工作，
 *          保证中断执行时间确定。
 */
void USBPD_TickIsr(void);

/**
 * @brief   协议栈主任务（主循环中持续调用）
 * @details 单次调用完成：毫秒增量计算、CC 接入消抖检测、
 *          协商状态机推进、已收报文的解析与应答、
 *          中断侧累积事件（PHY 复位 / 缓冲错误）的回调上报。
 */
void USBPD_Task(void);

/**
 * @brief   获取当前协议状态机状态
 * @retval  当前 usbpd_state_t 状态值
 */
usbpd_state_t USBPD_GetState(void);

/**
 * @brief   查询 CC 物理连接状态
 * @retval  1 已连接；0 未连接
 */
uint8_t USBPD_IsConnected(void);

/**
 * @brief   注册事件回调
 * @param   cb  回调函数指针，传 NULL 取消注册；
 *              回调在 USBPD_Task 上下文（主循环）中执行，
 *              回调内禁止阻塞
 */
void USBPD_SetEventCallback(usbpd_event_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* USBPD_H */
