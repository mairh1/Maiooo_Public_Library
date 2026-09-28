/**
 * @file    usbpd_io.h
 * @brief   异步 PD 传输、CC 与电源移植契约
 * @details 后端实现 BMC/CRC/GoodCRC/重试、FIFO 同步和硬件保护。
 * @note    不依赖 usbpd.h；全部接口仅在普通线程调用，禁止等待硬件完成。
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-09-28
 */
#ifndef USBPD_IO_H
#define USBPD_IO_H
#include <stdint.h>
#include <stdbool.h>
#include <usbpd_conf.h>
#include "usbpd_regs.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════ 独立底层类型 ═══════════════════ */
#ifndef DRV_IO_OK
#define DRV_IO_OK 0 /**< 成功或异步操作已受理 */
#endif
#ifndef DRV_IO_ERROR
#define DRV_IO_ERROR (-1) /**< I/O 失败，不透传厂商错误码 */
#endif

typedef enum
{
    USBPD_IO_CC_OPEN,       /**< 未连接 */
    USBPD_IO_CC_RA,         /**< 线缆 Ra */
    USBPD_IO_CC_RD,         /**< 对端 Sink */
    USBPD_IO_CC_RP_DEFAULT, /**< 默认 USB 电流 */
    USBPD_IO_CC_RP_1500,    /**< 对端 1.5A */
    USBPD_IO_CC_RP_3000     /**< 对端 3A */
} usbpd_io_cc_t;

typedef enum
{
    USBPD_IO_TERM_OPEN, /**< CC 高阻 */
    USBPD_IO_TERM_RD,   /**< CC 下拉 */
    USBPD_IO_TERM_RP    /**< CC 上拉，VCONN 所在脚除外 */
} usbpd_io_term_t;

typedef struct
{
    usbpd_io_cc_t cc1;  /**< 当前终端配置下的 CC1 检测值 */
    usbpd_io_cc_t cc2;  /**< 当前终端配置下的 CC2 检测值 */
    uint16_t vbus_mv;   /**< 实测 VBUS，mV */
    bool source_off;   /**< Source 通路已物理断开 */
    bool sink_off;     /**< Sink 通路已物理断开 */
    bool source_ready; /**< 最新 Source 电压及限流目标均已稳定 */
    bool sink_ready;   /**< 最新 Sink 使能及限流目标已完成 */
    bool vconn_ready;  /**< 最新 VCONN 目标已完成，包括关闭 */
    bool fault;        /**< 硬件故障且后端已执行保护 */
} usbpd_io_status_t;

typedef enum
{
    USBPD_IO_EVENT_NONE,       /**< 暂无事件，仍返回成功 */
    USBPD_IO_EVENT_RX,         /**< CRC 合法的 SOP，已回应 GoodCRC */
    USBPD_IO_EVENT_TX_DONE,    /**< 收到匹配 ID 的 GoodCRC */
    USBPD_IO_EVENT_TX_FAILED,  /**< 重试耗尽 */
    USBPD_IO_EVENT_TX_DISCARD, /**< 对端来包使本次发送丢弃 */
    USBPD_IO_EVENT_HARD_RESET, /**< 收到复位，优先于旧收发事件 */
    USBPD_IO_EVENT_RESET_SENT, /**< 本机复位信号发送完毕 */
    USBPD_IO_EVENT_ERROR       /**< FIFO 溢出或 DMA 等错误 */
} usbpd_io_event_kind_t;

typedef struct
{
    usbpd_io_event_kind_t kind;    /**< 事件类型 */
    uint32_t at_ms;               /**< 发生时刻，与 Task 同一毫秒时基 */
    uint8_t length;               /**< RX 长度，2..30，不含 CRC */
    uint8_t data[USBPD_MAX_FRAME]; /**< 小端帧副本，非 RX 时忽略 */
} usbpd_io_event_t;

/* ═══════════════════ 始终必选的生命周期与传输接口 ═══════════════════ */
/**
 * @brief   初始化后端，CC 高阻且所有电源关闭。
 * @details 线程上下文非阻塞；失败也须保持安全状态。
 * @param   io_ctx 端口上下文。
 * @retval  DRV_IO_OK 已初始化；DRV_IO_ERROR 硬件不可用。
 */
int usbpd_io_init(void *io_ctx);
/**
 * @brief   强制关闭供受电、VCONN 和收发，CC 高阻，取消全部事务。
 * @details 线程非阻塞；立即禁止电源驱动，只对本机原先 Source 的 VBUS 放电。
 *          不能对外部 Source 放电；保留状态检测。硬件保护不能依赖 Task 调度。
 * @param   io_ctx 端口上下文。
 * @retval  DRV_IO_OK 已执行关闭；DRV_IO_ERROR 无法保证关闭。
 */
int usbpd_io_shutdown(void *io_ctx);
/**
 * @brief   释放资源，不得重新打开电源。
 * @details 线程非阻塞；在 shutdown 后调用。
 * @param   io_ctx 端口上下文。
 * @retval  DRV_IO_OK 已释放；DRV_IO_ERROR 失败。
 */
int usbpd_io_deinit(void *io_ctx);
/**
 * @brief   读取一致的 CC、电源与故障快照。
 * @details 线程非阻塞；必须是真实反馈，不能用命令缓存伪造 ready。
 *          CC 电流档位必须经过 BMC Idle 采样或规范允许的底层滤波，不能把
 *          PD 波形的瞬时电压当作 Rp 电流变化。关闭后的非通信 CC 应重新检测 Ra。
 * @param   io_ctx 端口上下文。
 * @param   status 输出快照，所有字段每次均需更新。
 * @retval  DRV_IO_OK 快照有效；DRV_IO_ERROR 失败。
 */
int usbpd_io_get_status(void *io_ctx, usbpd_io_status_t *status);
/**
 * @brief   配置 CC 终端，返回时已经生效。
 * @details 线程非阻塞；保留 VCONN 所在脚，不改变供受电通路。
 * @param   io_ctx 端口上下文。
 * @param   term 高阻、Rd 或 Rp；已裁剪角色不会被请求。
 * @param   rp_ma Rp 电流通告，500/1500/3000mA；非 Rp 时为 0。
 * @retval  DRV_IO_OK 已设置；DRV_IO_ERROR 失败。
 */
int usbpd_io_set_cc(void *io_ctx, usbpd_io_term_t term, uint16_t rp_ma);
/**
 * @brief   选择 SOP 通信 CC 并更新 GoodCRC 角色，返回前生效。
 * @details 线程非阻塞；GoodCRC 使用 PD 2.0，SOP'/SOP'' 不应答也不上报。
 *          不改变 VCONN/电源；PR_Swap 保持原数据角色。
 * @param   io_ctx 端口上下文。
 * @param   cc_line 0 禁止收发，1/2 选择 CC1/CC2。
 * @param   source 是否为 Source。
 * @param   dfp 是否为 DFP。
 * @retval  DRV_IO_OK 已设置；DRV_IO_ERROR 失败。
 */
int usbpd_io_set_link(void *io_ctx, uint8_t cc_line, bool source, bool dfp);
/**
 * @brief   中止 TX 并清空旧事件，保留 CC、电源与 GoodCRC 角色。
 * @details 线程立即完成；返回后不能出现旧事务事件。
 * @param   io_ctx 端口上下文。
 * @retval  DRV_IO_OK 已清空；DRV_IO_ERROR 失败。
 */
int usbpd_io_reset(void *io_ctx);
/**
 * @brief   提交 SOP 事务，立即复制报文后返回。
 * @details 线程非阻塞；后端执行 GoodCRC ID 校验及 tReceive/tRetry，最终只
 *          报告一次完成/失败/丢弃。响应 RX 前必须先报告相应 TX_DONE。
 * @param   io_ctx 端口上下文。
 * @param   data 小端头及对象；后端必须复制，不能借用缓冲区。
 * @param   length 2..30 字节，不含 SOP/CRC。
 * @param   retries 首次之外的重试次数，PD 2.0 为 3；重试保持消息 ID。
 * @retval  DRV_IO_OK 已受理；DRV_IO_ERROR 未受理且不产生完成事件。
 */
int usbpd_io_transmit(void *io_ctx, const uint8_t *data, uint8_t length,
                      uint8_t retries);
/**
 * @brief   异步发送 Hard Reset，终止旧收发。
 * @details 线程立即返回；信号结束报告 RESET_SENT。
 * @param   io_ctx 端口上下文。
 * @retval  DRV_IO_OK 已受理；DRV_IO_ERROR 失败。
 */
int usbpd_io_hard_reset(void *io_ctx);
/**
 * @brief   取出一个事件，无事件返回 NONE。
 * @details 线程非阻塞；保证 FIFO/ISR 一致性与发生顺序。Hard Reset 清除
 *          旧事务事件；FIFO 溢出必须报告 ERROR。GoodCRC 不能等待本接口调用。
 *          只取 at_ms 不晚于 until_ms 的事件（回绕安全比较）；更晚的事件
 *          保留在 FIFO 并返回 NONE，避免 Task 执行中跨毫秒边界造成未来时间戳。
 * @param   io_ctx 端口上下文。
 * @param   until_ms 当前 Task 的时间快照，后端不可截断或改写真实事件时间戳。
 * @param   event 输出事件，时间戳为线上事件完成时刻。
 * @retval  DRV_IO_OK 读取完成；DRV_IO_ERROR 后端异常。
 */
int usbpd_io_get_event(void *io_ctx, uint32_t until_ms, usbpd_io_event_t *event);

/* ═══════════════════ 按角色裁剪的电源接口 ═══════════════════ */
#if USBPD_DRP_ENABLE
/**
 * @brief   获取用于 DRP 周期抖动的随机值。
 * @details 线程非阻塞；使用独立噪声源或经独立种子初始化的后端随机发生器，
 *          禁止所有端口返回同一个固定序列。依据 Type-C 2.0 表 4-30 的锁步说明。
 * @param   io_ctx 端口上下文。
 * @param   random_value 输出随机值，仅用于避免角色探测锁步，无密码学要求。
 * @retval  DRV_IO_OK 已取得；DRV_IO_ERROR 不能取得独立随机值。
 */
int usbpd_io_get_random(void *io_ctx, uint32_t *random_value);
#endif
#if USBPD_SOURCE_ENABLE
/**
 * @brief   异步控制供电及输出目标。
 * @details 线程立即返回；关闭时立即禁止输出驱动。Sink 未关闭时不得使能。
 * @param   io_ctx 端口上下文。
 * @param   enable 是否供电。
 * @param   voltage_mv 目标电压，关闭时 0，单位 mV。
 * @param   current_ma 电流上限，关闭时 0，单位 mA。
 * @retval  DRV_IO_OK 已受理，等待 source_ready/off；DRV_IO_ERROR 失败。
 */
int usbpd_io_set_source(void *io_ctx, bool enable, uint16_t voltage_mv,
                        uint16_t current_ma);
/**
 * @brief   设置本机 Source 的 VBUS 放电，返回前生效。
 * @details 线程非阻塞；禁止与本机或对端供电同时开启。
 * @param   io_ctx 端口上下文。
 * @param   enable 是否放电。
 * @retval  DRV_IO_OK 已设置；DRV_IO_ERROR 失败。
 */
int usbpd_io_set_discharge(void *io_ctx, bool enable);
/**
 * @brief   控制非通信 CC 的基础 VCONN 供电。
 * @details 线程非阻塞；硬件限流保护必需；PR_Swap 不改变 VCONN 角色。
 * @param   io_ctx 端口上下文。
 * @param   cc_line 0 关闭，1/2 向 CC1/CC2 供电。
 * @retval  DRV_IO_OK 已受理，等待 vconn_ready；DRV_IO_ERROR 失败。
 */
int usbpd_io_set_vconn(void *io_ctx, uint8_t cc_line);
#endif
#if USBPD_SINK_ENABLE
/**
 * @brief   设置受电通路和最大取电电流。
 * @details 线程非阻塞；调压期间核心关闭取电，平台需独立供电维持控制器运行。
 *          电流是限值，不保证实际负载；硬件必须防止反灌。
 * @param   io_ctx 端口上下文。
 * @param   enable 是否允许取电。
 * @param   current_ma 电流上限，关闭时 0，单位 mA。
 * @retval  DRV_IO_OK 已受理，等待 sink_ready/off；DRV_IO_ERROR 失败。
 */
int usbpd_io_set_sink(void *io_ctx, bool enable, uint16_t current_ma);
#endif
#ifdef __cplusplus
}
#endif
#endif /* USBPD_IO_H */
