/**
 * @file    usbpd.h
 * @brief   多实例、非阻塞 USB PD 2.0 公共接口
 * @details 固定 PDO、Sink/Source/DRP 和 PR_Swap；调用者提供存储与时钟。
 * @note    同实例串行访问，回调禁止重入；不同实例可独立运行。
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-09-28
 */
#ifndef USBPD_H
#define USBPD_H
#include <stdint.h>
#include <stdbool.h>
#include <usbpd_conf.h>
#include "usbpd_regs.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════ 公共数据类型 ═══════════════════ */
typedef enum
{
    USBPD_OK = 0,             /**< 成功或请求已排队 */
    USBPD_ERR_IO,             /**< 底层失败 */
    USBPD_ERR_PARAM,          /**< 参数或配置不合法 */
    USBPD_ERR_NOT_READY,      /**< 未初始化或尚未具备操作条件 */
    USBPD_ERR_NOT_SUPPORTED,  /**< 功能或对象不支持 */
    USBPD_ERR_BUSY,           /**< 同实例重入或已有事务 */
    USBPD_ERR_TIMEOUT,        /**< 协议或硬件等待超时 */
    USBPD_ERR_REJECTED,       /**< 对端拒绝 */
    USBPD_ERR_WAIT,           /**< 对端要求稍后重试 */
    USBPD_ERR_PROTOCOL,       /**< 非法消息或协议顺序错误 */
    USBPD_ERR_CANCELED,       /**< 脱离、复位或能力变化取消事务 */
    USBPD_ERR_FAULT           /**< 硬件故障，需重新初始化 */
} usbpd_result_t;

typedef enum
{
    USBPD_ROLE_SINK = 0, /**< 当前受电角色 */
    USBPD_ROLE_SOURCE   /**< 当前供电角色 */
} usbpd_role_t;

typedef enum
{
#if USBPD_SINK_ENABLE
    USBPD_MODE_SINK = 0, /**< 初始使用 Rd；允许的 PR_Swap 可改变连接中角色 */
#endif
#if USBPD_SOURCE_ENABLE
    USBPD_MODE_SOURCE = 1, /**< 初始使用 Rp */
#endif
#if USBPD_DRP_ENABLE
    USBPD_MODE_DRP = 2, /**< 未连接时交替探测 */
#endif
    USBPD_MODE_INVALID = 255 /**< 非法模式 */
} usbpd_mode_t;

typedef enum
{
    USBPD_STATE_DISABLED,       /**< 未运行 */
    USBPD_STATE_UNATTACHED,     /**< 探测或接入消抖 */
    USBPD_STATE_TYPEC_ONLY,     /**< 只有 Type-C 默认供电，无显式合同 */
    USBPD_STATE_SNK_NO_POWER,   /**< 对端通过 5V/0mA 撤销供电能力，停止取电 */
    USBPD_STATE_READY,          /**< 显式合同就绪 */
    USBPD_STATE_SNK_WAIT_CAP,   /**< 等待 Source 能力 */
    USBPD_STATE_SNK_SEND_REQUEST, /**< 发送受电请求 */
    USBPD_STATE_SNK_WAIT_ACCEPT,  /**< 等待请求应答 */
    USBPD_STATE_SNK_WAIT_PS,    /**< 等待 PS_RDY */
    USBPD_STATE_SNK_ENABLE,     /**< 等待受电通路就绪 */
    USBPD_STATE_SRC_START,      /**< 启动 5V */
    USBPD_STATE_SRC_SEND_CAP,   /**< 发送 Source 能力 */
    USBPD_STATE_SRC_WAIT_REQUEST, /**< 等待 Request */
    USBPD_STATE_SRC_CAP_RETRY,  /**< 无 PD 对端时重发能力 */
    USBPD_STATE_SRC_DELAY,      /**< Accept 后给 Sink 留出准备时间 */
    USBPD_STATE_SRC_TRANSITION, /**< 等待调压完成 */
    USBPD_STATE_SRC_COMMIT,     /**< PS_RDY 发送完成后提交合同 */
    USBPD_STATE_SOFT_SEND,      /**< 发送软复位 */
    USBPD_STATE_SOFT_WAIT,      /**< 等待软复位 Accept */
    USBPD_STATE_RESTART,        /**< 软复位后重新协商 */
    USBPD_STATE_HARD_SEND,      /**< 等待硬复位信号发送完成 */
    USBPD_STATE_HARD_DELAY,     /**< 硬复位断电延迟 */
    USBPD_STATE_HARD_OFF,       /**< 等待 VBUS 归零 */
    USBPD_STATE_HARD_RECOVER,   /**< Source 电源恢复间隔 */
    USBPD_STATE_HARD_WAIT_FIVE, /**< Sink 等待恢复到 5V */
#if USBPD_PR_SWAP_ENABLE
    USBPD_STATE_SWAP_SEND,      /**< 主动发送 PR_Swap */
    USBPD_STATE_SWAP_WAIT,      /**< 等待 PR_Swap 应答 */
    USBPD_STATE_SWAP_BEGIN,     /**< 已接受，开始电源交接 */
    USBPD_STATE_SWAP_SOURCE_DELAY, /**< 原 Source 等待 tSrcTransition */
    USBPD_STATE_SWAP_SOURCE_OFF,   /**< 原 Source 关闭并放电 */
    USBPD_STATE_SWAP_WAIT_SOURCE,  /**< 原 Source 等待新 Source 的 PS_RDY */
    USBPD_STATE_SWAP_WAIT_OFF,     /**< 原 Sink 等待原 Source 的 PS_RDY */
    USBPD_STATE_SWAP_SOURCE_ON,    /**< 原 Sink 建立 5V */
    USBPD_STATE_SWAP_FINISH_SOURCE, /**< 新 Source 的 PS_RDY 已确认 */
    USBPD_STATE_SWAP_FINISH_SINK,   /**< 新 Sink 等待受电通路稳定 */
    USBPD_STATE_SWAP_CAP_DELAY,    /**< 新 Source 延迟发送能力 */
#endif
    USBPD_STATE_ERROR_RECOVERY, /**< CC 高阻后重新探测 */
    USBPD_STATE_FAULT           /**< 故障锁定，不自动重新供电 */
} usbpd_state_t;

typedef struct
{
    uint16_t voltage_mv; /**< 固定电压，5000..20000mV，50mV 对齐 */
    uint16_t current_ma; /**< 最大电流，10..3000mA，10mA 对齐 */
} usbpd_fixed_pdo_t;

typedef struct
{
    uint32_t raw;        /**< 对端原始 PDO，保留全部类型 */
    uint16_t voltage_mv; /**< 固定 PDO 电压，其他类型为 0 */
    uint16_t current_ma; /**< 固定 PDO 电流，其他类型为 0 */
    uint8_t index;       /**< 原始报文中的 1 起序号 */
    bool fixed;         /**< 是否为固定 PDO，不表示本机一定能申请 */
} usbpd_pdo_t;

typedef struct
{
    bool valid;         /**< 仅完成协议及电源确认后为 true */
    uint8_t pdo_index;  /**< 合同对象序号，从 1 开始 */
    uint16_t voltage_mv; /**< 生效或目标电压，mV */
    uint16_t current_ma; /**< 生效或目标电流上限，mA */
} usbpd_contract_t;

typedef struct
{
    usbpd_mode_t mode; /**< 初始角色或 DRP */
#if USBPD_SINK_ENABLE
    uint8_t sink_count; /**< 本机固定受电能力数；不支持受电时为 0 */
    usbpd_fixed_pdo_t sink_pdos[USBPD_MAX_PDOS]; /**< 第一项 5V，电压严格递增 */
#endif
#if USBPD_SOURCE_ENABLE
    uint8_t source_count; /**< 本机固定供电能力数；不支持供电时为 0 */
    usbpd_fixed_pdo_t source_pdos[USBPD_MAX_PDOS]; /**< 第一项 5V，电压严格递增 */
#endif
} usbpd_config_t;

typedef enum
{
    USBPD_EVENT_ATTACHED,       /**< 已连接，尚不表示电源合同生效 */
    USBPD_EVENT_DETACHED,       /**< 已断开，合同失效 */
    USBPD_EVENT_SOURCE_CAPS,    /**< 对端能力已更新，可在 Task 后查询 */
    USBPD_EVENT_CONTRACT,       /**< 新合同生效 */
    USBPD_EVENT_CONTRACT_LOST,  /**< 原合同失效 */
    USBPD_EVENT_REQUEST_DONE,   /**< 应用受电请求终止，result 给出结果 */
    USBPD_EVENT_SWAP_DONE,      /**< 角色交换终止，result 给出结果 */
    USBPD_EVENT_SOFT_RESET,     /**< 发起或收到软复位 */
    USBPD_EVENT_HARD_RESET,     /**< 发起或收到硬复位 */
    USBPD_EVENT_ERROR           /**< 协议、I/O、硬件或恢复错误 */
} usbpd_event_t;

typedef struct
{
    usbpd_event_t event;       /**< 事件类型 */
    usbpd_result_t result;     /**< 结果或错误原因 */
    usbpd_role_t role;         /**< 当前电源角色 */
    uint8_t cc_line;           /**< 0 未连接，1/2 为 CC1/CC2 */
    usbpd_contract_t contract; /**< 当前已生效合同快照 */
} usbpd_event_info_t;

/** @brief 仅在 Task 内同步调用；info 只在回调期间有效，需保存时自行复制。 */
typedef void (*usbpd_event_cb_t)(void *user_ctx, const usbpd_event_info_t *info);

typedef struct
{
    usbpd_state_t state;       /**< 当前状态 */
    usbpd_role_t role;         /**< 当前电源角色 */
    bool connected;            /**< Type-C 已接入 */
    bool busy;                 /**< 应用事务尚未终止 */
    bool dfp;                  /**< 数据角色，PR_Swap 不改变 */
    uint8_t cc_line;           /**< 通信 CC */
    usbpd_contract_t contract; /**< 已生效合同 */
} usbpd_status_t;

/* ═══════════════════ 实例存储（调用者分配，禁止直接修改成员） ═══════════════════ */
typedef struct
{
    void *io_ctx;               /**< 底层实例上下文 */
    usbpd_config_t config;       /**< 初始化配置副本 */
    usbpd_event_cb_t callback;   /**< 应用事件接收器 */
    void *callback_ctx;          /**< 应用回调上下文 */
    usbpd_state_t state;         /**< 内部状态 */
    usbpd_state_t after_tx;      /**< 成功发送后的状态 */
    usbpd_role_t role;           /**< 电源角色 */
    usbpd_result_t fault_reason; /**< 锁定故障的原因 */
    uint16_t typec_current_ma;   /**< 上一次设置的默认取电电流 */
    uint16_t recovery_wait_ms;   /**< 根据原 VCONN 供电状态选择的恢复等待 */
#if USBPD_DRP_ENABLE
    uint16_t drp_hold_ms;        /**< 本轮随机化后的探测角色保持时间 */
#endif
    usbpd_contract_t contract;   /**< 已生效合同 */
    usbpd_contract_t target;     /**< 待协商目标 */
    usbpd_pdo_t peer_pdos[USBPD_MAX_PDOS]; /**< 对端原始能力快照 */
    uint32_t now_ms;             /**< 本次任务时间 */
    uint32_t state_at;           /**< 状态开始时间 */
    uint32_t tx_at;              /**< 发送开始时间 */
    uint32_t candidate_at;       /**< CC 接入候选开始时间 */
    uint32_t detach_at;          /**< CC/VBUS 异常开始时间 */
    uint32_t no_response_at;     /**< 无响应观察起点 */
    uint32_t wait_at;            /**< 对端 Wait 收到时间 */
    uint32_t swap_at;            /**< 电源交换开始时间 */
    uint8_t tx_data[USBPD_MAX_FRAME]; /**< 编码工作缓冲 */
    uint8_t peer_count;          /**< 原始 PDO 数量 */
    uint8_t tx_id;               /**< 本机消息 ID */
    uint8_t rx_id;               /**< 对端最近消息 ID */
    uint8_t cc_line;             /**< 已连接通信 CC */
    uint8_t candidate_cc;        /**< 接入候选 CC */
    uint8_t vconn_cc;            /**< 本机 VCONN 所在脚 */
    uint8_t hard_count;          /**< 无响应硬复位计数 */
    uint8_t caps_count;          /**< Source 能力发送次数 */
    uint8_t command;             /**< 应用排队命令，内部使用 */
    uint8_t operation;           /**< 尚未完成的应用操作 */
    bool initialized;           /**< 初始化成功 */
    bool task_active;           /**< 回调/任务重入保护 */
    bool entered;               /**< 本状态入口动作待执行 */
    bool connected;             /**< 物理连接 */
    bool dfp;                   /**< 数据角色 */
    bool rx_valid;              /**< 上次接收 ID 是否有效 */
    bool tx_busy;               /**< 后端有发送事务 */
    bool detaching;             /**< 正在消抖断开 */
    bool wait_active;           /**< 对端 Wait 冷却窗口 */
    bool partner_ps;            /**< 交换中的对端 PS_RDY 已收到 */
    bool swap_active;           /**< 交换已接受，禁止普通软复位恢复 */
    bool pd_seen;               /**< 本连接见过 PD 消息 */
#if USBPD_PR_SWAP_ENABLE
    bool allow_source_to_sink;  /**< 是否接受对端要求本机转 Sink */
    bool allow_sink_to_source;  /**< 是否接受对端要求本机转 Source */
#endif
} usbpd_dev_t;

/* ═══════════════════ 公共 API ═══════════════════ */
/**
 * @brief   初始化调用者提供的实例。
 * @details dev 必须零初始化或已经 Deinit；本函数复制配置，不保留配置指针。
 * @param   dev 实例，生命周期覆盖全部异步操作。
 * @param   io_ctx 底层上下文，透传给全部 io 函数。
 * @param   config 能力及初始角色；至少提供当前角色的能力表。
 * @param   now_ms 与后端事件相同的单调毫秒时间，可自然回绕。
 * @retval  USBPD_OK 成功；ERR_PARAM 配置错误；ERR_IO 底层失败；ERR_BUSY 已运行。
 */
usbpd_result_t USBPD_Init(usbpd_dev_t *dev, void *io_ctx,
                          const usbpd_config_t *config, uint32_t now_ms);
/**
 * @brief   关闭电源并释放实例。
 * @details 普通线程调用，不与运行期并发；失败保留故障实例以便再次关闭。
 * @param   dev 已初始化实例。
 * @retval  USBPD_OK 已停止；ERR_PARAM/NOT_READY/BUSY/IO 分别表示相应错误。
 */
usbpd_result_t USBPD_Deinit(usbpd_dev_t *dev);
/**
 * @brief   非阻塞推进协议状态机。
 * @details 最多处理 EVENT_BUDGET 个事件；调用间隔不超过 1ms，禁止 ISR 调用。
 * @param   dev 实例。
 * @param   now_ms 当前毫秒时间；两次调用间隔必须小于 2^31ms。
 * @retval  USBPD_OK 已推进；ERR_PARAM/NOT_READY/BUSY/IO/FAULT 表示调用或硬件失败。
 */
usbpd_result_t USBPD_Task(usbpd_dev_t *dev, uint32_t now_ms);
/**
 * @brief   复制状态和已生效合同。
 * @details 与 Task 串行调用；回调应直接读取其事件快照。
 * @param   dev 实例。
 * @param   status 输出快照。
 * @retval  USBPD_OK 成功；ERR_PARAM/NOT_READY/BUSY 表示调用错误。
 */
usbpd_result_t USBPD_GetStatus(const usbpd_dev_t *dev, usbpd_status_t *status);
/**
 * @brief   注册或取消事件回调。
 * @details 回调仅由 Task 同步调用，禁止阻塞及重入同实例 API。
 * @param   dev 实例。
 * @param   callback 回调，NULL 取消。
 * @param   user_ctx 用户上下文，不由核心解引用。
 * @retval  USBPD_OK 成功；ERR_PARAM/NOT_READY/BUSY 表示调用错误。
 */
usbpd_result_t USBPD_SetEventCallback(usbpd_dev_t *dev,
                                      usbpd_event_cb_t callback, void *user_ctx);
#if USBPD_SINK_ENABLE
/**
 * @brief   复制对端完整能力表，不重新编号。
 * @details 包含不能申请的非固定 PDO；没有能力时 count=0 并返回 NOT_READY。
 * @param   dev 实例。
 * @param   pdos 输出数组，至少 capacity 个元素。
 * @param   capacity 输出数组容量，不足时不部分复制。
 * @param   count 输出所需/实际元素数。
 * @retval  USBPD_OK 成功；ERR_PARAM 缓冲不足；ERR_NOT_READY 无能力；ERR_BUSY 重入。
 */
usbpd_result_t USBPD_GetSourceCapabilities(const usbpd_dev_t *dev,
    usbpd_pdo_t *pdos, uint8_t capacity, uint8_t *count);
/**
 * @brief   排队申请固定 PDO。
 * @details 使用原始序号；电流必须为 10mA 整数倍，不静默增加或舍入请求。
 * @param   dev 当前为 Sink 的实例。
 * @param   pdo_index 对端原始 1 起序号。
 * @param   current_ma 所需电流，不超过对端、本机能力及 3000mA。
 * @retval  USBPD_OK 已排队；最终结果由 REQUEST_DONE 上报。
 * @retval  USBPD_ERR_PARAM 参数错误；ERR_NOT_SUPPORTED 档位不支持。
 * @retval  USBPD_ERR_NOT_READY 未就绪；ERR_BUSY 已有事务；ERR_WAIT 冷却中。
 */
usbpd_result_t USBPD_RequestFixed(usbpd_dev_t *dev, uint8_t pdo_index,
                                  uint16_t current_ma);
#endif
#if USBPD_PR_SWAP_ENABLE
/**
 * @brief   排队请求交换当前电源角色。
 * @details 必须有显式合同及双角色能力；完成经 SWAP_DONE 报告。
 * @param   dev 实例。
 * @retval  USBPD_OK 已排队；ERR_NOT_SUPPORTED 无双角色能力；ERR_NOT_READY 无合同。
 * @retval  USBPD_ERR_BUSY 已有事务；ERR_WAIT 对端 Wait 冷却中；ERR_PARAM 空指针。
 */
usbpd_result_t USBPD_RequestPowerRoleSwap(usbpd_dev_t *dev);
/**
 * @brief   设置对端发起交换时的许可，初始化默认均为 false。
 * @details 不限制应用主动发起；按本机当前角色解释两个方向。
 * @param   dev 实例。
 * @param   source_to_sink 允许本机由 Source 转为 Sink。
 * @param   sink_to_source 允许本机由 Sink 转为 Source。
 * @retval  USBPD_OK 成功；ERR_PARAM/NOT_READY/BUSY 表示调用错误。
 */
usbpd_result_t USBPD_SetSwapPolicy(usbpd_dev_t *dev, bool source_to_sink,
                                   bool sink_to_source);
#endif
#ifdef __cplusplus
}
#endif
#endif /* USBPD_H */
