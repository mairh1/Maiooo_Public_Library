/**
 * @file    usbpd.c
 * @brief   USB PD 2.0 Type-C 双角色协议核心
 * @details 显式状态机、异步传输、电源反馈及每实例独立的协议计时。
 * @note    无平台符号、动态分配或硬件等待；只在普通线程运行。
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-09-28
 */
#include "usbpd.h"
#include "usbpd_io.h"
#include <stddef.h>
#include <string.h>

/* ═══════════════════ 基础状态及错误收敛 ═══════════════════ */
/**
 * @brief   用无符号差值处理毫秒计数回绕；单次间隔小于 2^31ms。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   now 当前毫秒时间。
 * @param   start 计时起点，与 now 使用同一时基。
 * @param   duration 所需时间间隔，ms。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_expired(uint32_t now, uint32_t start, uint32_t duration)
{
    return (uint32_t)(now - start) >= duration;
}

/**
 * @brief   从小端帧读取对象，不依赖对齐或别名规则。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   bytes 对象起始字节地址，至少 4 字节可访问。
 * @return  解码后的 32 位对象。
 */
static uint32_t
usbpd_read_object(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/**
 * @brief   将对象编码为小端，帧缓冲由当前实例持有。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   bytes 对象起始字节地址，至少 4 字节可访问。
 * @param   value 待编码的 32 位协议对象。
 */
static void
usbpd_write_object(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

/**
 * @brief   只在 Task 上下文同步通知，快照不暴露内部数组指针。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   event 待处理事件或要上报的事件类型。
 * @param   result 操作结果或待检查的底层返回码。
 */
static void
usbpd_emit(usbpd_dev_t *dev, usbpd_event_t event, usbpd_result_t result)
{
    usbpd_event_info_t info;

    if (dev->callback == NULL)
    {
        return;
    }
    info.event = event;
    info.result = result;
    info.role = dev->role;
    info.cc_line = dev->cc_line;
    info.contract = dev->contract;
    dev->callback(dev->callback_ctx, &info);
}

/**
 * @brief   完成一个排队或执行中的应用事务，保证只有一个终止通知。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   result 操作结果或待检查的底层返回码。
 */
static void
usbpd_finish_operation(usbpd_dev_t *dev, usbpd_result_t result)
{
    uint8_t operation = dev->operation;

    dev->operation = 0;
    dev->command = 0;
    if (operation == 1U)
    {
        usbpd_emit(dev, USBPD_EVENT_REQUEST_DONE, result);
    }
    else if (operation == 2U)
    {
        usbpd_emit(dev, USBPD_EVENT_SWAP_DONE, result);
    }
}

/**
 * @brief   清除已生效合同，避免复位/脱离后继续使用旧电源参数。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 */
static void
usbpd_invalidate_contract(usbpd_dev_t *dev)
{
    bool valid = dev->contract.valid;

    memset(&dev->contract, 0, sizeof(dev->contract));
    if (valid)
    {
        usbpd_emit(dev, USBPD_EVENT_CONTRACT_LOST, USBPD_ERR_CANCELED);
    }
}

/**
 * @brief   转移状态并使用线上事件时刻启动计时，不以轮询时刻替代。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   state 要进入的协议状态。
 * @param   at_ms 线上事件时刻或状态起点，ms。
 */
static void
usbpd_enter(usbpd_dev_t *dev, usbpd_state_t state, uint32_t at_ms)
{
    dev->state = state;
    dev->state_at = at_ms;
    dev->entered = true;
}

/**
 * @brief   I/O 失败统一关闭并锁定；不得把错误当作未就绪继续供电。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   result 操作结果或待检查的底层返回码。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_io_check(usbpd_dev_t *dev, int result)
{
    if (result == DRV_IO_OK)
    {
        return true;
    }
    (void)usbpd_io_shutdown(dev->io_ctx);
    dev->tx_busy = false;
    dev->swap_active = false;
    dev->fault_reason = USBPD_ERR_IO;
    usbpd_enter(dev, USBPD_STATE_FAULT, dev->now_ms);
    usbpd_invalidate_contract(dev);
    usbpd_finish_operation(dev, USBPD_ERR_IO);
    usbpd_emit(dev, USBPD_EVENT_ERROR, USBPD_ERR_IO);
    return false;
}

/**
 * @brief   每次读取都检查保护故障，包括一轮 Task 中途新发生的故障。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_read_status(usbpd_dev_t *dev, usbpd_io_status_t *status)
{
    if (!usbpd_io_check(dev, usbpd_io_get_status(dev->io_ctx, status)))
    {
        return false;
    }
    if (status->fault)
    {
        (void)usbpd_io_shutdown(dev->io_ctx);
        dev->fault_reason = USBPD_ERR_FAULT;
        dev->tx_busy = false;
        dev->swap_active = false;
        usbpd_enter(dev, USBPD_STATE_FAULT, dev->now_ms);
        usbpd_invalidate_contract(dev);
        usbpd_finish_operation(dev, USBPD_ERR_FAULT);
        usbpd_emit(dev, USBPD_EVENT_ERROR, USBPD_ERR_FAULT);
        return false;
    }
    if (status->cc1 > USBPD_IO_CC_RP_3000 || status->cc2 > USBPD_IO_CC_RP_3000)
    {
        return usbpd_io_check(dev, DRV_IO_ERROR);
    }
    return true;
}

/**
 * @brief   同时具备两组能力才能通告双角色，不凭编译开关伪造能力。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_dual_capable(const usbpd_dev_t *dev)
{
#if USBPD_PR_SWAP_ENABLE
    return dev->config.sink_count != 0U && dev->config.source_count != 0U;
#else
    (void)dev;
    return false;
#endif
}

/**
 * @brief   根据 5V 能力选择不会过度承诺的 Rp 档位。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @return  电流值（mA）；无可用能力时为 0。
 */
static uint16_t
usbpd_rp_current(const usbpd_dev_t *dev)
{
#if USBPD_SOURCE_ENABLE
    uint16_t current = dev->config.source_pdos[0].current_ma;

    if (current >= 3000U)
    {
        return 3000U;
    }
    if (current >= 1500U)
    {
        return 1500U;
    }
#else
    (void)dev;
#endif
    return 500U;
}

/**
 * @brief   更新电源角色和 CC；数据角色仅在初次连接及硬复位时重置。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   role 目标电源角色。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_set_role(usbpd_dev_t *dev, usbpd_role_t role)
{
    usbpd_io_term_t term = USBPD_IO_TERM_RD;
    uint16_t rp_ma = 0;

    if (role == USBPD_ROLE_SOURCE)
    {
        term = USBPD_IO_TERM_RP;
        rp_ma = usbpd_rp_current(dev);
    }
    if (!usbpd_io_check(dev, usbpd_io_set_cc(dev->io_ctx, term, rp_ma)))
    {
        return false;
    }
    dev->role = role;
    return usbpd_io_check(dev, usbpd_io_set_link(dev->io_ctx, dev->cc_line,
                                                role == USBPD_ROLE_SOURCE, dev->dfp));
}

/**
 * @brief   复位传输层、消息 ID 及去重状态，但不切断已生效合同。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_reset_protocol(usbpd_dev_t *dev)
{
    if (!usbpd_io_check(dev, usbpd_io_reset(dev->io_ctx)))
    {
        return false;
    }
    dev->tx_busy = false;
    dev->tx_id = 0;
    dev->rx_valid = false;
    return true;
}

/**
 * @brief   错误恢复打开 CC 并停止电源；等待放电反馈后才允许重新探测。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   reason 导致恢复或取消的原因。
 */
static void
usbpd_recover(usbpd_dev_t *dev, usbpd_result_t reason)
{
    bool connected = dev->connected;

    dev->recovery_wait_ms = dev->vconn_cc != 0U ? USBPD_T_VCONN_RECOVERY :
                                                   USBPD_T_ERROR_RECOVERY;
    if (!usbpd_io_check(dev, usbpd_io_shutdown(dev->io_ctx)))
    {
        return;
    }
    dev->connected = false;
    dev->tx_busy = false;
    dev->swap_active = false;
    dev->peer_count = 0;
    dev->cc_line = 0;
    dev->vconn_cc = 0;
    dev->candidate_cc = 0;
    dev->typec_current_ma = 0;
    dev->detaching = false;
    usbpd_enter(dev, USBPD_STATE_ERROR_RECOVERY, dev->now_ms);
    usbpd_invalidate_contract(dev);
    usbpd_finish_operation(dev, reason);
    if (connected)
    {
        usbpd_emit(dev, USBPD_EVENT_DETACHED, reason);
    }
    if (reason != USBPD_ERR_CANCELED)
    {
        usbpd_emit(dev, USBPD_EVENT_ERROR, reason);
    }
}

/**
 * @brief   非阻塞发送；只有完成事件才推进 ID 和后继状态。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   type 消息类型编码；调用点明确控制/数据类别。
 * @param   objects 数据对象数组；控制消息为 NULL。
 * @param   count 数据对象或能力表元素数。
 * @param   after 成功发送后进入的状态。
 */
static void
usbpd_send(usbpd_dev_t *dev, uint8_t type, const uint32_t *objects,
           uint8_t count, usbpd_state_t after)
{
    uint16_t header;
    uint8_t index;

    header = (uint16_t)((uint32_t)type | (USBPD_REV20 << 6) |
                       ((uint32_t)dev->tx_id << 9) | ((uint32_t)count << 12));
    if (dev->role == USBPD_ROLE_SOURCE)
    {
        header |= 0x0100U;
    }
    if (dev->dfp)
    {
        header |= 0x0020U;
    }
    dev->tx_data[0] = (uint8_t)header;
    dev->tx_data[1] = (uint8_t)(header >> 8);
    for (index = 0; index < count; index++)
    {
        usbpd_write_object(&dev->tx_data[2U + 4U * index], objects[index]);
    }
    if (!usbpd_io_check(dev, usbpd_io_transmit(dev->io_ctx, dev->tx_data,
                                              (uint8_t)(2U + 4U * count),
                                              USBPD_TX_RETRIES)))
    {
        return;
    }
    dev->tx_busy = true;
    dev->tx_at = dev->now_ms;
    dev->after_tx = after;
}

/**
 * @brief   编码本机固定能力，不承诺 USB 数据、挂起或数据角色交换。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   source true 发送 Source 能力；false 发送 Sink 能力。
 * @param   after 成功发送后进入的状态。
 */
static void
usbpd_send_caps(usbpd_dev_t *dev, bool source, usbpd_state_t after)
{
    uint32_t objects[USBPD_MAX_PDOS];
    const usbpd_fixed_pdo_t *pdos = NULL;
    uint8_t count = 0;
    uint8_t index;

#if USBPD_SOURCE_ENABLE
    if (source)
    {
        pdos = dev->config.source_pdos;
        count = dev->config.source_count;
    }
#endif
#if USBPD_SINK_ENABLE
    if (!source)
    {
        pdos = dev->config.sink_pdos;
        count = dev->config.sink_count;
    }
#endif
    if (count == 0U)
    {
        usbpd_send(dev, USBPD_CTRL_REJECT, NULL, 0, after);
        return;
    }
    for (index = 0; index < count; index++)
    {
        objects[index] = ((uint32_t)(pdos[index].voltage_mv / 50U) << 10) |
                         (uint32_t)(pdos[index].current_ma / 10U);
    }
    if (usbpd_dual_capable(dev))
    {
        objects[0] |= USBPD_PDO_DUAL_POWER;
    }
    usbpd_send(dev, source ? USBPD_DATA_SOURCE_CAP : USBPD_DATA_SINK_CAP,
               objects, count, after);
}

/**
 * @brief   验证当前 VBUS 在固定目标的稳态容差范围内。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   measured 实测电压，mV。
 * @param   target 目标电压，mV。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_voltage_ready(uint16_t measured, uint16_t target)
{
    uint32_t lower = (uint32_t)target * 95U / 100U;
    uint32_t upper = (uint32_t)target * 105U / 100U;

    return measured >= lower && measured <= upper;
}

/**
 * @brief   判断 Type-C 的安全 5V 范围。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   voltage 固定电压或实测 VBUS，mV。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_safe_five(uint16_t voltage)
{
    return voltage >= USBPD_SAFE_FIVE_MIN_MV && voltage <= USBPD_SAFE_FIVE_MAX_MV;
}

/**
 * @brief   发起硬复位，交换中失败改走 Type-C ErrorRecovery。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   reason 导致恢复或取消的原因。
 */
static void
usbpd_start_hard_reset(usbpd_dev_t *dev, usbpd_result_t reason)
{
    if (dev->swap_active)
    {
        usbpd_recover(dev, reason);
        return;
    }
    if (dev->hard_count > USBPD_HARD_RESET_LIMIT)
    {
        if (!dev->pd_seen && dev->role == USBPD_ROLE_SINK)
        {
            usbpd_finish_operation(dev, reason);
            usbpd_enter(dev, USBPD_STATE_TYPEC_ONLY, dev->now_ms);
            return;
        }
        usbpd_recover(dev, reason);
        return;
    }
    if (!usbpd_reset_protocol(dev))
    {
        return;
    }
    dev->hard_count++;
    dev->peer_count = 0;
    usbpd_invalidate_contract(dev);
    usbpd_finish_operation(dev, reason);
    if (!usbpd_io_check(dev, usbpd_io_hard_reset(dev->io_ctx)))
    {
        return;
    }
    usbpd_enter(dev, USBPD_STATE_HARD_SEND, dev->now_ms);
    usbpd_emit(dev, USBPD_EVENT_HARD_RESET, reason);
}

/**
 * @brief   普通协议错误先软复位，失败再硬复位；电源转换中直接硬复位。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   reason 导致恢复或取消的原因。
 */
static void
usbpd_protocol_error(usbpd_dev_t *dev, usbpd_result_t reason)
{
    usbpd_emit(dev, USBPD_EVENT_ERROR, reason);
    if (dev->swap_active || dev->state == USBPD_STATE_SOFT_WAIT ||
        dev->state == USBPD_STATE_SOFT_SEND || dev->state == USBPD_STATE_SNK_WAIT_PS ||
        dev->state == USBPD_STATE_SNK_ENABLE || dev->state == USBPD_STATE_SRC_DELAY ||
        dev->state == USBPD_STATE_SRC_TRANSITION || dev->state == USBPD_STATE_SRC_COMMIT)
    {
        usbpd_start_hard_reset(dev, reason);
        return;
    }
    usbpd_finish_operation(dev, reason);
    if (usbpd_reset_protocol(dev))
    {
        usbpd_enter(dev, USBPD_STATE_SOFT_SEND, dev->now_ms);
        usbpd_emit(dev, USBPD_EVENT_SOFT_RESET, reason);
    }
}

/* ═══════════════════ 能力解析与请求校验 ═══════════════════ */
#if USBPD_SINK_ENABLE
/**
 * @brief   查询本机对给定固定电压允许的最大取电电流。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   voltage 固定电压或实测 VBUS，mV。
 * @return  电流值（mA）；无可用能力时为 0。
 */
static uint16_t
usbpd_sink_limit(const usbpd_dev_t *dev, uint16_t voltage)
{
    uint8_t index;

    for (index = 0; index < dev->config.sink_count; index++)
    {
        if (dev->config.sink_pdos[index].voltage_mv == voltage)
        {
            return dev->config.sink_pdos[index].current_ma;
        }
    }
    return 0;
}

/**
 * @brief   接收完整能力表，非固定对象原位保留，非法首 PDO 不进入协商。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   data 小端 Source_Capabilities 对象数据。
 * @param   count 数据对象或能力表元素数。
 */
static void
usbpd_receive_caps(usbpd_dev_t *dev, const uint8_t *data, uint8_t count)
{
    uint8_t index;
    uint16_t current;
    usbpd_contract_t previous = dev->contract;

    if ((usbpd_read_object(data) >> 30) != 0U ||
        ((usbpd_read_object(data) >> 10) & 0x3FFU) != 100U)
    {
        usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    usbpd_finish_operation(dev, USBPD_ERR_CANCELED);
    for (index = 0; index < count; index++)
    {
        usbpd_pdo_t *pdo = &dev->peer_pdos[index];

        pdo->raw = usbpd_read_object(&data[4U * index]);
        pdo->index = (uint8_t)(index + 1U);
        pdo->fixed = (pdo->raw >> 30) == 0U;
        pdo->voltage_mv = 0;
        pdo->current_ma = 0;
        if (pdo->fixed)
        {
            pdo->voltage_mv = (uint16_t)(((pdo->raw >> 10) & 0x3FFU) * 50U);
            pdo->current_ma = (uint16_t)((pdo->raw & 0x3FFU) * 10U);
            if ((pdo->current_ma == 0U && !(count == 1U && index == 0U)) ||
                pdo->voltage_mv == 0U)
            {
                dev->peer_count = 0;
                usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
                return;
            }
        }
    }
    dev->peer_count = count;
    dev->hard_count = 0;
    if (dev->peer_pdos[0].current_ma == 0U)
    {
        dev->typec_current_ma = 0;
        usbpd_invalidate_contract(dev);
        if (usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, false, 0)))
        {
            usbpd_enter(dev, USBPD_STATE_SNK_NO_POWER, dev->now_ms);
            usbpd_emit(dev, USBPD_EVENT_SOURCE_CAPS, USBPD_ERR_NOT_READY);
        }
        return;
    }
    dev->target.valid = false;
    dev->target.pdo_index = 1;
    dev->target.voltage_mv = 5000;
    current = usbpd_sink_limit(dev, 5000);
    if (current > dev->peer_pdos[0].current_ma)
    {
        current = dev->peer_pdos[0].current_ma;
    }
    dev->target.current_ma = current;
    /* 能力更新优先保持现有电压和电流；对象位置可能发生变化，必须重新查找。 */
    if (previous.valid)
    {
        for (index = 0; index < count; index++)
        {
            const usbpd_pdo_t *pdo = &dev->peer_pdos[index];

            if (pdo->fixed && pdo->voltage_mv == previous.voltage_mv &&
                pdo->current_ma >= previous.current_ma)
            {
                dev->target = previous;
                dev->target.valid = false;
                dev->target.pdo_index = pdo->index;
                break;
            }
        }
    }
    usbpd_enter(dev, USBPD_STATE_SNK_SEND_REQUEST, dev->now_ms);
    usbpd_emit(dev, USBPD_EVENT_SOURCE_CAPS, USBPD_OK);
}

/**
 * @brief   按 Rp 与本机上限确定尚无 PD 合同时允许的电流。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @return  电流值（mA）；无可用能力时为 0。
 */
static uint16_t
usbpd_typec_current(const usbpd_dev_t *dev, const usbpd_io_status_t *status)
{
    usbpd_io_cc_t cc = dev->cc_line == 1U ? status->cc1 : status->cc2;
    uint16_t current = 500;
    uint16_t limit = usbpd_sink_limit(dev, 5000);

    if (cc == USBPD_IO_CC_RP_1500)
    {
        current = 1500;
    }
    else if (cc == USBPD_IO_CC_RP_3000)
    {
        current = 3000;
    }
    return current < limit ? current : limit;
}

/**
 * @brief   默认供电跟踪 Rp 电流变化，只在目标变化时重新设置限流。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_set_typec_sink(usbpd_dev_t *dev, const usbpd_io_status_t *status)
{
    uint16_t current = usbpd_typec_current(dev, status);

    if (dev->typec_current_ma == current)
    {
        return true;
    }
    if (!usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, true, current)))
    {
        return false;
    }
    dev->typec_current_ma = current;
    return true;
}
#endif

#if USBPD_SOURCE_ENABLE
/**
 * @brief   校验 Request 的原始位置、保留位、GiveBack 和两项电流。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   request 收到的原始 RDO。
 */
static void
usbpd_receive_request(usbpd_dev_t *dev, uint32_t request)
{
    uint8_t index = (uint8_t)((request >> 28) & 7U);
    uint16_t operating = (uint16_t)(((request >> 10) & 0x3FFU) * 10U);
    uint16_t maximum = (uint16_t)((request & 0x3FFU) * 10U);
    const usbpd_fixed_pdo_t *pdo;
    usbpd_state_t resume = dev->contract.valid ? USBPD_STATE_READY :
                                                USBPD_STATE_SRC_WAIT_REQUEST;

    if (index == 0U || index > dev->config.source_count ||
        (request & 0x88F00000UL) != 0U || operating == 0U || maximum < operating)
    {
        usbpd_send(dev, USBPD_CTRL_REJECT, NULL, 0, resume);
        return;
    }
    pdo = &dev->config.source_pdos[index - 1U];
    if (operating > pdo->current_ma ||
        (((request & USBPD_RDO_MISMATCH) == 0U) && maximum > pdo->current_ma))
    {
        usbpd_send(dev, USBPD_CTRL_REJECT, NULL, 0, resume);
        return;
    }
    usbpd_finish_operation(dev, USBPD_ERR_CANCELED);
    dev->target.valid = false;
    dev->target.pdo_index = index;
    dev->target.voltage_mv = pdo->voltage_mv;
    dev->target.current_ma = maximum < pdo->current_ma ? maximum : pdo->current_ma;
    usbpd_send(dev, USBPD_CTRL_ACCEPT, NULL, 0, USBPD_STATE_SRC_DELAY);
}
#endif

/* ═══════════════════ Type-C 接入与合同生命周期 ═══════════════════ */
/**
 * @brief   完成合同前必须经过 PS_RDY 和本机电源就绪确认。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 */
static void
usbpd_commit_contract(usbpd_dev_t *dev)
{
    dev->contract = dev->target;
    dev->contract.valid = true;
    dev->hard_count = 0;
    dev->wait_active = false;
    usbpd_enter(dev, USBPD_STATE_READY, dev->now_ms);
    usbpd_emit(dev, USBPD_EVENT_CONTRACT, USBPD_OK);
    usbpd_finish_operation(dev, USBPD_OK);
}

/**
 * @brief   仅接受恰好一条通信 CC，避免把附件模式误识别为普通端口。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @return  通信 CC 编号；无合法候选时为 0。
 */
static uint8_t
usbpd_candidate(const usbpd_dev_t *dev, const usbpd_io_status_t *status)
{
    bool first;
    bool second;

    if (dev->role == USBPD_ROLE_SOURCE)
    {
        first = status->cc1 == USBPD_IO_CC_RD;
        second = status->cc2 == USBPD_IO_CC_RD;
        if ((first && status->cc2 != USBPD_IO_CC_OPEN && status->cc2 != USBPD_IO_CC_RA) ||
            (second && status->cc1 != USBPD_IO_CC_OPEN && status->cc1 != USBPD_IO_CC_RA))
        {
            return 0;
        }
    }
    else
    {
        first = status->cc1 >= USBPD_IO_CC_RP_DEFAULT;
        second = status->cc2 >= USBPD_IO_CC_RP_DEFAULT;
    }
    if (first == second)
    {
        return 0;
    }
    return first ? 1U : 2U;
}

/**
 * @brief   硬复位及已接受的交换期间屏蔽正常 VBUS/CC 脱离判断。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_power_interruption(const usbpd_dev_t *dev)
{
    return dev->swap_active ||
           (dev->state >= USBPD_STATE_HARD_SEND &&
            dev->state <= USBPD_STATE_HARD_WAIT_FIVE);
}

/**
 * @brief   任务内检测断开；Source 看 CC，Sink 在非切换期看 VBUS。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 */
static void
usbpd_check_detach(usbpd_dev_t *dev, const usbpd_io_status_t *status)
{
    bool absent;
    usbpd_io_cc_t cc;

    if (!dev->connected || usbpd_power_interruption(dev))
    {
        dev->detaching = false;
        return;
    }
    cc = dev->cc_line == 1U ? status->cc1 : status->cc2;
    absent = dev->role == USBPD_ROLE_SOURCE ? cc != USBPD_IO_CC_RD :
              status->vbus_mv < USBPD_SINK_DISCONNECT_MV;
    if (dev->role == USBPD_ROLE_SINK &&
        (dev->state == USBPD_STATE_SNK_WAIT_PS || dev->state == USBPD_STATE_SNK_ENABLE))
    {
        /* 合同切换期间允许电压下降，仍由协议转换计时器约束。 */
        absent = false;
    }
    if (!absent)
    {
        dev->detaching = false;
        return;
    }
    if (!dev->detaching)
    {
        dev->detaching = true;
        dev->detach_at = dev->now_ms;
    }
    else if (usbpd_expired(dev->now_ms, dev->detach_at, USBPD_T_PD_DEBOUNCE))
    {
        usbpd_recover(dev, USBPD_ERR_CANCELED);
    }
}

/**
 * @brief   建立通信角色并进入初始供受电流程。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @param   cc 通信方向，1 或 2。
 */
static void
usbpd_attach(usbpd_dev_t *dev, const usbpd_io_status_t *status, uint8_t cc)
{
    dev->cc_line = cc;
    dev->dfp = dev->role == USBPD_ROLE_SOURCE;
    dev->vconn_cc = 0;
#if USBPD_SOURCE_ENABLE
    if (dev->role == USBPD_ROLE_SOURCE)
    {
        usbpd_io_cc_t other = cc == 1U ? status->cc2 : status->cc1;

        if (other == USBPD_IO_CC_RA)
        {
            dev->vconn_cc = cc == 1U ? 2U : 1U;
        }
    }
#endif
    if (!usbpd_reset_protocol(dev) || !usbpd_set_role(dev, dev->role))
    {
        return;
    }
    dev->connected = true;
    dev->pd_seen = false;
    dev->hard_count = 0;
    dev->caps_count = 0;
    dev->no_response_at = dev->now_ms;
    dev->wait_active = false;
    if (dev->role == USBPD_ROLE_SOURCE)
    {
        usbpd_enter(dev, USBPD_STATE_SRC_START, dev->now_ms);
    }
    else
    {
#if USBPD_SINK_ENABLE
        if (!usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, true,
                                                  usbpd_typec_current(dev, status))))
        {
            return;
        }
#endif
        usbpd_enter(dev, USBPD_STATE_SNK_WAIT_CAP, dev->now_ms);
    }
    usbpd_emit(dev, USBPD_EVENT_ATTACHED, USBPD_OK);
}

#if USBPD_DRP_ENABLE
/**
 * @brief   独立选择每个半周期的长度，不改变平均角色偏好。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_schedule_toggle(usbpd_dev_t *dev)
{
    uint32_t random_value;

    if (!usbpd_io_check(dev, usbpd_io_get_random(dev->io_ctx, &random_value)))
    {
        return false;
    }
    dev->drp_hold_ms = (uint16_t)(USBPD_T_DRP_HALF - USBPD_T_DRP_JITTER +
                         random_value % (2U * USBPD_T_DRP_JITTER + 1U));
    return true;
}
#endif

/**
 * @brief   根据配置重新开始探测，脱离连接后恢复初始角色。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_unattached_start(usbpd_dev_t *dev)
{
    usbpd_role_t role = USBPD_ROLE_SINK;

#if USBPD_SOURCE_ENABLE
    if (dev->config.mode == USBPD_MODE_SOURCE)
    {
        role = USBPD_ROLE_SOURCE;
    }
    if (!USBPD_SINK_ENABLE)
    {
        role = USBPD_ROLE_SOURCE;
    }
    if (!usbpd_io_check(dev, usbpd_io_set_discharge(dev->io_ctx, false)))
    {
        return false;
    }
#endif
    dev->cc_line = 0;
    dev->candidate_cc = 0;
    dev->peer_count = 0;
    dev->swap_active = false;
    dev->wait_active = false;
    dev->connected = false;
    dev->hard_count = 0;
#if USBPD_DRP_ENABLE
    if (dev->config.mode == USBPD_MODE_DRP && !usbpd_schedule_toggle(dev))
    {
        return false;
    }
#endif
    return usbpd_reset_protocol(dev) && usbpd_set_role(dev, role);
}

/**
 * @brief   只在 Task 入口推进 CC 消抖；候选成立期间暂停 DRP 翻转。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 */
static void
usbpd_unattached_step(usbpd_dev_t *dev, const usbpd_io_status_t *status)
{
    uint8_t cc = usbpd_candidate(dev, status);

    if (cc != dev->candidate_cc)
    {
        dev->candidate_cc = cc;
        dev->candidate_at = dev->now_ms;
    }
    if (cc != 0U)
    {
        if (usbpd_expired(dev->now_ms, dev->candidate_at, USBPD_T_CC_DEBOUNCE))
        {
            bool voltage_ok = dev->role == USBPD_ROLE_SOURCE ?
                              status->vbus_mv <= USBPD_SAFE_ZERO_MV :
                              usbpd_safe_five(status->vbus_mv);

            if (voltage_ok && status->source_off && status->sink_off)
            {
                usbpd_attach(dev, status, cc);
            }
        }
        return;
    }
#if USBPD_DRP_ENABLE
    if (dev->config.mode == USBPD_MODE_DRP &&
        usbpd_expired(dev->now_ms, dev->state_at, dev->drp_hold_ms))
    {
        usbpd_role_t role = dev->role == USBPD_ROLE_SINK ?
                            USBPD_ROLE_SOURCE : USBPD_ROLE_SINK;

        if (usbpd_set_role(dev, role))
        {
            dev->state_at = dev->now_ms;
            (void)usbpd_schedule_toggle(dev);
        }
    }
#endif
}

/* ═══════════════════ 报文分派：先区分控制/数据，后检查状态 ═══════════════════ */
/**
 * @brief   判断是否处于可处理新事务的稳定电源状态。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_ready(const usbpd_dev_t *dev)
{
    return dev->state == USBPD_STATE_READY || dev->state == USBPD_STATE_TYPEC_ONLY;
}

/**
 * @brief   受理对端软复位，重置消息 ID，保留电源合同直到重新协商。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 */
static void
usbpd_receive_soft_reset(usbpd_dev_t *dev)
{
    if (dev->swap_active)
    {
        usbpd_recover(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    if (dev->state == USBPD_STATE_SNK_WAIT_PS || dev->state == USBPD_STATE_SRC_DELAY ||
        dev->state == USBPD_STATE_SRC_TRANSITION || dev->state == USBPD_STATE_SNK_ENABLE)
    {
        usbpd_start_hard_reset(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    usbpd_finish_operation(dev, USBPD_ERR_CANCELED);
    if (!usbpd_reset_protocol(dev))
    {
        return;
    }
    usbpd_emit(dev, USBPD_EVENT_SOFT_RESET, USBPD_OK);
    /* 接收 Soft_Reset 的 ID 必须为 0；Accept 使用复位后的本机 ID 0。 */
    dev->rx_valid = true;
    dev->rx_id = 0;
    usbpd_send(dev, USBPD_CTRL_ACCEPT, NULL, 0, USBPD_STATE_RESTART);
}

#if USBPD_PR_SWAP_ENABLE
/**
 * @brief   接收交换请求，许可只在稳定显式合同和本机双角色能力下生效。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 */
static void
usbpd_receive_swap(usbpd_dev_t *dev)
{
    bool allowed = dev->role == USBPD_ROLE_SOURCE ?
                   dev->allow_source_to_sink : dev->allow_sink_to_source;

    if (!usbpd_ready(dev) || !dev->contract.valid)
    {
        usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    if (!allowed || !usbpd_dual_capable(dev))
    {
        usbpd_send(dev, USBPD_CTRL_REJECT, NULL, 0, USBPD_STATE_READY);
        return;
    }
    usbpd_finish_operation(dev, USBPD_ERR_CANCELED);
    dev->operation = 2;
    usbpd_send(dev, USBPD_CTRL_ACCEPT, NULL, 0, USBPD_STATE_SWAP_BEGIN);
}
#endif

/**
 * @brief   对端 Reject/Wait 不改变旧合同；Wait 限制后续主动请求频率。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   type 消息类型编码；调用点明确控制/数据类别。
 * @param   at_ms 线上事件时刻或状态起点，ms。
 */
static void
usbpd_request_denied(usbpd_dev_t *dev, uint8_t type, uint32_t at_ms)
{
    usbpd_result_t result = type == USBPD_CTRL_WAIT ? USBPD_ERR_WAIT : USBPD_ERR_REJECTED;

    if (type == USBPD_CTRL_WAIT)
    {
        dev->wait_active = true;
        dev->wait_at = at_ms;
    }
    usbpd_enter(dev, dev->contract.valid ? USBPD_STATE_READY : USBPD_STATE_TYPEC_ONLY,
                 at_ms);
    usbpd_finish_operation(dev, result);
    usbpd_emit(dev, USBPD_EVENT_ERROR, result);
}

/**
 * @brief   处理仅有消息头的控制消息。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   type 消息类型编码；调用点明确控制/数据类别。
 * @param   at_ms 线上事件时刻或状态起点，ms。
 */
static void
usbpd_receive_control(usbpd_dev_t *dev, uint8_t type, uint32_t at_ms)
{
    if (type == USBPD_CTRL_ACCEPT)
    {
        if (dev->state == USBPD_STATE_SNK_WAIT_ACCEPT)
        {
#if USBPD_SINK_ENABLE
            /* 在调压前暂停负载，满足 tSnkStdby，不把旧合同电流用于新电压。 */
            if (!usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, false, 0)))
            {
                return;
            }
#endif
            dev->typec_current_ma = 0;
            usbpd_invalidate_contract(dev);
            usbpd_enter(dev, USBPD_STATE_SNK_WAIT_PS, at_ms);
            return;
        }
        if (dev->state == USBPD_STATE_SOFT_WAIT)
        {
            usbpd_enter(dev, USBPD_STATE_RESTART, at_ms);
            return;
        }
#if USBPD_PR_SWAP_ENABLE
        if (dev->state == USBPD_STATE_SWAP_WAIT)
        {
            usbpd_enter(dev, USBPD_STATE_SWAP_BEGIN, at_ms);
            return;
        }
#endif
    }
    else if (type == USBPD_CTRL_REJECT || type == USBPD_CTRL_WAIT)
    {
        if (dev->state == USBPD_STATE_SNK_WAIT_ACCEPT
#if USBPD_PR_SWAP_ENABLE
            || dev->state == USBPD_STATE_SWAP_WAIT
#endif
            )
        {
            usbpd_request_denied(dev, type, at_ms);
            return;
        }
    }
    else if (type == USBPD_CTRL_PS_RDY)
    {
        if (dev->state == USBPD_STATE_SNK_WAIT_PS)
        {
            usbpd_enter(dev, USBPD_STATE_SNK_ENABLE, at_ms);
            return;
        }
#if USBPD_PR_SWAP_ENABLE
        if (dev->state == USBPD_STATE_SWAP_WAIT_OFF ||
            dev->state == USBPD_STATE_SWAP_WAIT_SOURCE)
        {
            dev->partner_ps = true;
            return;
        }
#endif
    }
    else if (type == USBPD_CTRL_PING)
    {
        return;
    }
    else if (type == USBPD_CTRL_GET_SOURCE || type == USBPD_CTRL_GET_SINK)
    {
        if (usbpd_ready(dev))
        {
            usbpd_state_t after = dev->state;

            if (type == USBPD_CTRL_GET_SOURCE && dev->role == USBPD_ROLE_SOURCE &&
                !dev->contract.valid)
            {
                after = USBPD_STATE_SRC_WAIT_REQUEST;
            }
            usbpd_send_caps(dev, type == USBPD_CTRL_GET_SOURCE, after);
            return;
        }
    }
    else if (type == USBPD_CTRL_PR_SWAP)
    {
#if USBPD_PR_SWAP_ENABLE
        usbpd_receive_swap(dev);
        return;
#else
        if (usbpd_ready(dev))
        {
            usbpd_send(dev, USBPD_CTRL_REJECT, NULL, 0, dev->state);
            return;
        }
#endif
    }
    else if (type == USBPD_CTRL_DR_SWAP || type == USBPD_CTRL_VCONN_SWAP)
    {
        if (usbpd_ready(dev))
        {
            usbpd_send(dev, USBPD_CTRL_REJECT, NULL, 0, dev->state);
            return;
        }
    }
    usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
}

/**
 * @brief   未实现结构化 VDM 用 NAK，不伪造 ACK；Attention/响应/非结构化消息忽略。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   vdm 收到的 VDM 头对象。
 */
static void
usbpd_receive_vdm(usbpd_dev_t *dev, uint32_t vdm)
{
    uint8_t command = (uint8_t)(vdm & 0x1FU);

    if (!usbpd_ready(dev))
    {
        usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    if ((vdm & 0x8000U) == 0U || (vdm & 0xC0U) != 0U || command == 6U || dev->dfp)
    {
        return;
    }
    vdm &= ~0x60C0UL;
    vdm |= 0x80U;
    usbpd_send(dev, USBPD_DATA_VDM, &vdm, 1, dev->state);
}

/**
 * @brief   检查帧长度/版本/类型后进行去重与状态相关分派。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   event 待处理事件或要上报的事件类型。
 */
static void
usbpd_receive(usbpd_dev_t *dev, const usbpd_io_event_t *event)
{
    uint16_t header;
    uint8_t count;
    uint8_t type;
    uint8_t id;

    if (event->length < 2U || event->length > USBPD_MAX_FRAME)
    {
        usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    header = (uint16_t)((uint32_t)event->data[0] | ((uint32_t)event->data[1] << 8));
    count = (uint8_t)((header >> 12) & 7U);
    type = (uint8_t)(header & 0x1FU);
    id = (uint8_t)((header >> 9) & 7U);
    if (event->length != (uint8_t)(2U + 4U * count) || (header & 0x8000U) != 0U ||
        ((header >> 6) & 3U) == 0U || ((header >> 6) & 3U) == 3U)
    {
        usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    /* 接受 3.0 对端的初始普通能力报文，但本机始终声明并执行 2.0。 */
    if (count == 0U && type == USBPD_CTRL_GOODCRC)
    {
        return;
    }
    if (count == 0U && type == USBPD_CTRL_SOFT_RESET)
    {
        if (id != 0U)
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_PROTOCOL);
            return;
        }
        usbpd_receive_soft_reset(dev);
        return;
    }
    if (dev->rx_valid && id == dev->rx_id)
    {
        return;
    }
    dev->rx_valid = true;
    dev->rx_id = id;
    dev->pd_seen = true;
    if (dev->tx_busy)
    {
        /* 后端必须先提交 TX_DONE/TX_DISCARD；事件顺序错误不能继续猜测。 */
        usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    if (count == 0U)
    {
        uint32_t timeout = 0;

        if (dev->state == USBPD_STATE_SNK_WAIT_ACCEPT ||
            dev->state == USBPD_STATE_SOFT_WAIT)
        {
            timeout = USBPD_T_SENDER_RESPONSE;
        }
        else if (dev->state == USBPD_STATE_SNK_WAIT_PS)
        {
            timeout = USBPD_T_PS_TRANSITION;
        }
#if USBPD_PR_SWAP_ENABLE
        else if (dev->state == USBPD_STATE_SWAP_WAIT)
        {
            timeout = USBPD_T_SENDER_RESPONSE;
        }
        else if (dev->state == USBPD_STATE_SWAP_WAIT_SOURCE)
        {
            timeout = USBPD_T_PS_SOURCE_ON;
        }
        else if (dev->state == USBPD_STATE_SWAP_WAIT_OFF)
        {
            timeout = USBPD_T_PS_SOURCE_OFF;
        }
#endif
        if (timeout != 0U &&
            usbpd_expired(event->at_ms, dev->state_at, timeout))
        {
#if USBPD_PR_SWAP_ENABLE
            if (dev->state == USBPD_STATE_SWAP_WAIT)
            {
                usbpd_enter(dev, USBPD_STATE_READY, dev->now_ms);
                usbpd_finish_operation(dev, USBPD_ERR_TIMEOUT);
                return;
            }
#endif
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
            return;
        }
        usbpd_receive_control(dev, type, event->at_ms);
        return;
    }
#if USBPD_SINK_ENABLE
    if (type == USBPD_DATA_SOURCE_CAP && dev->role == USBPD_ROLE_SINK &&
        (usbpd_ready(dev) || dev->state == USBPD_STATE_SNK_NO_POWER ||
         dev->state == USBPD_STATE_SNK_WAIT_CAP ||
         dev->state == USBPD_STATE_SNK_WAIT_ACCEPT))
    {
        usbpd_receive_caps(dev, &event->data[2], count);
        return;
    }
#endif
#if USBPD_SOURCE_ENABLE
    if (type == USBPD_DATA_REQUEST && count == 1U && dev->role == USBPD_ROLE_SOURCE &&
        (usbpd_ready(dev) || dev->state == USBPD_STATE_SRC_WAIT_REQUEST ||
         dev->state == USBPD_STATE_SRC_CAP_RETRY))
    {
        if (dev->state == USBPD_STATE_SRC_WAIT_REQUEST &&
            usbpd_expired(event->at_ms, dev->state_at, USBPD_T_SENDER_RESPONSE))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
            return;
        }
        usbpd_receive_request(dev, usbpd_read_object(&event->data[2]));
        return;
    }
#endif
    if (type == USBPD_DATA_VDM)
    {
        usbpd_receive_vdm(dev, usbpd_read_object(&event->data[2]));
        return;
    }
    usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
}

/* ═══════════════════ 角色状态机 ═══════════════════ */
#if USBPD_SINK_ENABLE
/**
 * @brief   推进受电流程；每次调用只执行有界操作，不等待电压变化。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @param   entry 是否首次进入当前状态，用于一次性电源命令。
 */
static void
usbpd_sink_step(usbpd_dev_t *dev, const usbpd_io_status_t *status, bool entry)
{
    uint32_t request;

    switch (dev->state)
    {
    case USBPD_STATE_SNK_WAIT_CAP:
        if (!dev->contract.valid && !usbpd_set_typec_sink(dev, status))
        {
            return;
        }
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SINK_WAIT_CAP))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SNK_SEND_REQUEST:
        request = ((uint32_t)dev->target.pdo_index << 28) | USBPD_RDO_NO_SUSPEND |
                  ((uint32_t)(dev->target.current_ma / 10U) << 10) |
                  (uint32_t)(dev->target.current_ma / 10U);
        if (dev->operation != 1U &&
            dev->target.current_ma < usbpd_sink_limit(dev, dev->target.voltage_mv))
        {
            request |= USBPD_RDO_MISMATCH;
        }
        usbpd_send(dev, USBPD_DATA_REQUEST, &request, 1, USBPD_STATE_SNK_WAIT_ACCEPT);
        break;
    case USBPD_STATE_SNK_WAIT_ACCEPT:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SENDER_RESPONSE))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SNK_WAIT_PS:
        if ((!status->sink_off &&
             usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SRC_TRANSITION)) ||
            usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_PS_TRANSITION))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SNK_ENABLE:
        if (entry)
        {
            if (!status->source_off ||
                !usbpd_voltage_ready(status->vbus_mv, dev->target.voltage_mv))
            {
                usbpd_start_hard_reset(dev, USBPD_ERR_PROTOCOL);
                return;
            }
            (void)usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, true,
                                                       dev->target.current_ma));
            return;
        }
        if (status->sink_ready && !status->sink_off &&
            usbpd_voltage_ready(status->vbus_mv, dev->target.voltage_mv))
        {
            usbpd_commit_contract(dev);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SRC_TRANSITION))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    default:
        break;
    }
}
#endif

#if USBPD_SOURCE_ENABLE
/**
 * @brief   推进供电流程；发送 PS_RDY 前必须同时验证硬件 ready 和实测电压。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @param   entry 是否首次进入当前状态，用于一次性电源命令。
 */
static void
usbpd_source_step(usbpd_dev_t *dev, const usbpd_io_status_t *status, bool entry)
{
    switch (dev->state)
    {
    case USBPD_STATE_SRC_START:
        if (entry)
        {
            if (!status->sink_off || !status->source_off ||
                status->vbus_mv > USBPD_SAFE_ZERO_MV)
            {
                usbpd_recover(dev, USBPD_ERR_PROTOCOL);
                return;
            }
            if (!usbpd_io_check(dev, usbpd_io_set_discharge(dev->io_ctx, false)) ||
                !usbpd_io_check(dev, usbpd_io_set_vconn(dev->io_ctx, dev->vconn_cc)))
            {
                return;
            }
            (void)usbpd_io_check(dev, usbpd_io_set_source(dev->io_ctx, true, 5000,
                                                         dev->config.source_pdos[0].current_ma));
            return;
        }
        if (status->source_ready && !status->source_off && status->vconn_ready &&
            usbpd_safe_five(status->vbus_mv))
        {
            usbpd_enter(dev, USBPD_STATE_SRC_SEND_CAP, dev->now_ms);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SRC_READY))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SRC_SEND_CAP:
        dev->caps_count++;
        usbpd_send_caps(dev, true, USBPD_STATE_SRC_WAIT_REQUEST);
        break;
    case USBPD_STATE_SRC_WAIT_REQUEST:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SENDER_RESPONSE))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SRC_CAP_RETRY:
        if (dev->pd_seen &&
            usbpd_expired(dev->now_ms, dev->no_response_at, USBPD_T_NO_RESPONSE))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        else if (dev->caps_count >= USBPD_CAPS_LIMIT)
        {
            usbpd_enter(dev, USBPD_STATE_TYPEC_ONLY, dev->now_ms);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SEND_SOURCE_CAP))
        {
            usbpd_enter(dev, USBPD_STATE_SRC_SEND_CAP, dev->now_ms);
        }
        break;
    case USBPD_STATE_SRC_DELAY:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SRC_TRANSITION))
        {
            usbpd_invalidate_contract(dev);
            usbpd_enter(dev, USBPD_STATE_SRC_TRANSITION, dev->now_ms);
        }
        break;
    case USBPD_STATE_SRC_TRANSITION:
        if (entry)
        {
            if (!status->sink_off)
            {
                usbpd_recover(dev, USBPD_ERR_PROTOCOL);
                return;
            }
            (void)usbpd_io_check(dev, usbpd_io_set_source(dev->io_ctx, true,
                dev->target.voltage_mv,
                dev->config.source_pdos[dev->target.pdo_index - 1U].current_ma));
            return;
        }
        if (status->source_ready && !status->source_off &&
            usbpd_voltage_ready(status->vbus_mv, dev->target.voltage_mv))
        {
            usbpd_send(dev, USBPD_CTRL_PS_RDY, NULL, 0, USBPD_STATE_SRC_COMMIT);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SRC_READY))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SRC_COMMIT:
        usbpd_commit_contract(dev);
        break;
    default:
        break;
    }
}
#endif

#if USBPD_PR_SWAP_ENABLE
/**
 * @brief   完成电源角色交换；仍须重新协商，不能恢复旧角色的显式合同。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   next 交换完成后的启动状态。
 */
static void
usbpd_finish_swap(usbpd_dev_t *dev, usbpd_state_t next)
{
    if (!usbpd_reset_protocol(dev))
    {
        return;
    }
    dev->swap_active = false;
    dev->peer_count = 0;
    dev->caps_count = 0;
    dev->hard_count = 0;
    dev->no_response_at = dev->now_ms;
    usbpd_enter(dev, next, dev->now_ms);
    usbpd_finish_operation(dev, USBPD_OK);
}

/**
 * @brief   Type-C PR_Swap 两个方向的电源交接，不交换数据/VCONN 角色。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @param   entry 是否首次进入当前状态，用于一次性电源命令。
 */
static void
usbpd_swap_step(usbpd_dev_t *dev, const usbpd_io_status_t *status, bool entry)
{
    switch (dev->state)
    {
    case USBPD_STATE_SWAP_SEND:
        usbpd_send(dev, USBPD_CTRL_PR_SWAP, NULL, 0, USBPD_STATE_SWAP_WAIT);
        break;
    case USBPD_STATE_SWAP_WAIT:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SENDER_RESPONSE))
        {
            usbpd_enter(dev, USBPD_STATE_READY, dev->now_ms);
            usbpd_finish_operation(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SWAP_BEGIN:
        dev->swap_active = true;
        dev->partner_ps = false;
        dev->swap_at = dev->state_at;
        usbpd_invalidate_contract(dev);
        if (dev->role == USBPD_ROLE_SOURCE)
        {
            usbpd_enter(dev, USBPD_STATE_SWAP_SOURCE_DELAY, dev->swap_at);
        }
        else
        {
            if (usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, false, 0)))
            {
                usbpd_enter(dev, USBPD_STATE_SWAP_WAIT_OFF, dev->swap_at);
            }
        }
        break;
    case USBPD_STATE_SWAP_SOURCE_DELAY:
        if (usbpd_expired(dev->now_ms, dev->swap_at, USBPD_T_SRC_TRANSITION))
        {
            if (!usbpd_io_check(dev, usbpd_io_set_source(dev->io_ctx, false, 0, 0)) ||
                !usbpd_io_check(dev, usbpd_io_set_discharge(dev->io_ctx, true)))
            {
                return;
            }
            usbpd_enter(dev, USBPD_STATE_SWAP_SOURCE_OFF, dev->now_ms);
        }
        break;
    case USBPD_STATE_SWAP_SOURCE_OFF:
        if (status->source_off && status->vbus_mv <= USBPD_SAFE_ZERO_MV)
        {
            if (!usbpd_io_check(dev, usbpd_io_set_discharge(dev->io_ctx, false)) ||
                !usbpd_set_role(dev, USBPD_ROLE_SINK))
            {
                return;
            }
            /* 首个 PS_RDY 已使用新 Sink 角色；GoodCRC 角色同步更新。 */
            usbpd_send(dev, USBPD_CTRL_PS_RDY, NULL, 0, USBPD_STATE_SWAP_WAIT_SOURCE);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SAFE_ZERO))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SWAP_WAIT_SOURCE:
        if (dev->partner_ps && usbpd_safe_five(status->vbus_mv) && status->source_off)
        {
            if (usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, true,
                                                     usbpd_typec_current(dev, status))))
            {
                usbpd_enter(dev, USBPD_STATE_SWAP_FINISH_SINK, dev->now_ms);
            }
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_PS_SOURCE_ON))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SWAP_FINISH_SINK:
        if (status->sink_ready && !status->sink_off)
        {
            usbpd_finish_swap(dev, USBPD_STATE_SNK_WAIT_CAP);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_TX_WATCHDOG))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SWAP_WAIT_OFF:
        if (dev->partner_ps && status->sink_off && status->source_off &&
            status->vbus_mv <= USBPD_SAFE_ZERO_MV)
        {
            if (!usbpd_set_role(dev, USBPD_ROLE_SOURCE) ||
                !usbpd_io_check(dev, usbpd_io_set_discharge(dev->io_ctx, false)) ||
                !usbpd_io_check(dev, usbpd_io_set_source(dev->io_ctx, true, 5000,
                                                         dev->config.source_pdos[0].current_ma)))
            {
                return;
            }
            usbpd_enter(dev, USBPD_STATE_SWAP_SOURCE_ON, dev->now_ms);
        }
        else if (usbpd_expired(dev->now_ms, dev->swap_at, USBPD_T_PS_SOURCE_OFF))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SWAP_SOURCE_ON:
        if (status->source_ready && !status->source_off && usbpd_safe_five(status->vbus_mv))
        {
            usbpd_send(dev, USBPD_CTRL_PS_RDY, NULL, 0, USBPD_STATE_SWAP_FINISH_SOURCE);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SRC_READY))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_SWAP_FINISH_SOURCE:
        usbpd_finish_swap(dev, USBPD_STATE_SWAP_CAP_DELAY);
        break;
    case USBPD_STATE_SWAP_CAP_DELAY:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SWAP_SOURCE_START))
        {
            usbpd_enter(dev, USBPD_STATE_SRC_SEND_CAP, dev->now_ms);
        }
        break;
    default:
        break;
    }
    (void)entry;
}
#endif

/* ═══════════════════ 复位与统一状态调度 ═══════════════════ */
/**
 * @brief   硬复位信号完成后建立受约束的电源恢复流程。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   at_ms 线上事件时刻或状态起点，ms。
 */
static void
usbpd_hard_reset_received(usbpd_dev_t *dev, uint32_t at_ms)
{
    if (dev->swap_active)
    {
        usbpd_recover(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    if (!usbpd_reset_protocol(dev))
    {
        return;
    }
    usbpd_invalidate_contract(dev);
    usbpd_finish_operation(dev, USBPD_ERR_CANCELED);
    dev->peer_count = 0;
    dev->caps_count = 0;
    dev->typec_current_ma = 0;
    dev->no_response_at = dev->now_ms;
    dev->dfp = dev->role == USBPD_ROLE_SOURCE;
    if (!usbpd_io_check(dev, usbpd_io_set_link(dev->io_ctx, dev->cc_line,
                                               dev->role == USBPD_ROLE_SOURCE, dev->dfp)))
    {
        return;
    }
#if USBPD_SINK_ENABLE
    if (dev->role == USBPD_ROLE_SINK &&
        !usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, false, 0)))
    {
        return;
    }
#endif
    usbpd_enter(dev, USBPD_STATE_HARD_DELAY, at_ms);
}

/**
 * @brief   硬复位电源恢复；Sink 不对外部 VBUS 启动放电。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 * @param   entry 是否首次进入当前状态，用于一次性电源命令。
 */
static void
usbpd_hard_step(usbpd_dev_t *dev, const usbpd_io_status_t *status, bool entry)
{
    switch (dev->state)
    {
    case USBPD_STATE_HARD_SEND:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_HARD_SIGNAL))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_HARD_DELAY:
        if (dev->role == USBPD_ROLE_SINK ||
            usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_PS_HARD_RESET))
        {
#if USBPD_SOURCE_ENABLE
            if (!usbpd_io_check(dev, usbpd_io_set_vconn(dev->io_ctx, 0)))
            {
                return;
            }
            if (dev->role == USBPD_ROLE_SOURCE)
            {
                if (!usbpd_io_check(dev, usbpd_io_set_source(dev->io_ctx, false, 0, 0)) ||
                    !usbpd_io_check(dev, usbpd_io_set_discharge(dev->io_ctx, true)))
                {
                    return;
                }
            }
#endif
            usbpd_enter(dev, USBPD_STATE_HARD_OFF, dev->now_ms);
        }
        break;
    case USBPD_STATE_HARD_OFF:
        if (status->vbus_mv <= USBPD_SAFE_ZERO_MV && status->source_off && status->sink_off)
        {
#if USBPD_SOURCE_ENABLE
            if (dev->role == USBPD_ROLE_SOURCE &&
                !usbpd_io_check(dev, usbpd_io_set_discharge(dev->io_ctx, false)))
            {
                return;
            }
#endif
            usbpd_enter(dev, dev->role == USBPD_ROLE_SOURCE ? USBPD_STATE_HARD_RECOVER :
                         USBPD_STATE_HARD_WAIT_FIVE, dev->now_ms);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at,
                              USBPD_T_SAFE_ZERO +
                              (dev->role == USBPD_ROLE_SINK ? USBPD_T_PS_HARD_RESET : 0U)))
        {
#if USBPD_SINK_ENABLE
            if (dev->role == USBPD_ROLE_SINK && !dev->pd_seen &&
                usbpd_safe_five(status->vbus_mv))
            {
                if (usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, true,
                                                         usbpd_typec_current(dev, status))))
                {
                    usbpd_enter(dev, USBPD_STATE_SNK_WAIT_CAP, dev->now_ms);
                }
                return;
            }
#endif
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_HARD_RECOVER:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SRC_RECOVER))
        {
            usbpd_io_cc_t other = dev->cc_line == 1U ? status->cc2 : status->cc1;

            dev->vconn_cc = other == USBPD_IO_CC_RA ?
                            (dev->cc_line == 1U ? 2U : 1U) : 0U;
            usbpd_enter(dev, USBPD_STATE_SRC_START, dev->now_ms);
        }
        break;
    case USBPD_STATE_HARD_WAIT_FIVE:
#if USBPD_SINK_ENABLE
        if (usbpd_safe_five(status->vbus_mv))
        {
            if (usbpd_io_check(dev, usbpd_io_set_sink(dev->io_ctx, true,
                                                     usbpd_typec_current(dev, status))))
            {
                usbpd_enter(dev, USBPD_STATE_SNK_WAIT_CAP, dev->now_ms);
            }
        }
        else
#endif
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_NO_RESPONSE))
        {
            usbpd_recover(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    default:
        break;
    }
    (void)entry;
}

/**
 * @brief   单次有界状态推进，每次使用重新读取的电源快照。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   status 最新硬件状态快照。
 */
static void
usbpd_step(usbpd_dev_t *dev, const usbpd_io_status_t *status)
{
    bool entry = dev->entered;

    if (dev->tx_busy || dev->state == USBPD_STATE_FAULT)
    {
        return;
    }
    dev->entered = false;
    if (dev->state >= USBPD_STATE_SNK_WAIT_CAP && dev->state <= USBPD_STATE_SNK_ENABLE)
    {
#if USBPD_SINK_ENABLE
        usbpd_sink_step(dev, status, entry);
#endif
        return;
    }
    if (dev->state >= USBPD_STATE_SRC_START && dev->state <= USBPD_STATE_SRC_COMMIT)
    {
#if USBPD_SOURCE_ENABLE
        usbpd_source_step(dev, status, entry);
#endif
        return;
    }
#if USBPD_PR_SWAP_ENABLE
    if (dev->state >= USBPD_STATE_SWAP_SEND && dev->state <= USBPD_STATE_SWAP_CAP_DELAY)
    {
        usbpd_swap_step(dev, status, entry);
        return;
    }
#endif
    if (dev->state >= USBPD_STATE_HARD_SEND && dev->state <= USBPD_STATE_HARD_WAIT_FIVE)
    {
        usbpd_hard_step(dev, status, entry);
        return;
    }
    switch (dev->state)
    {
    case USBPD_STATE_UNATTACHED:
        if (entry)
        {
            (void)usbpd_unattached_start(dev);
            return;
        }
        usbpd_unattached_step(dev, status);
        break;
    case USBPD_STATE_TYPEC_ONLY:
#if USBPD_SINK_ENABLE
        if (dev->role == USBPD_ROLE_SINK && usbpd_safe_five(status->vbus_mv))
        {
            if (!usbpd_set_typec_sink(dev, status))
            {
                return;
            }
        }
#endif
        /* 只有能力仍可用时，应用才可从 Type-C 默认供电重新申请。 */
        if (dev->command == 1U)
        {
            dev->command = 0;
            usbpd_enter(dev, USBPD_STATE_SNK_SEND_REQUEST, dev->now_ms);
        }
        break;
    case USBPD_STATE_READY:
        if (dev->command == 1U)
        {
            dev->command = 0;
            usbpd_enter(dev, USBPD_STATE_SNK_SEND_REQUEST, dev->now_ms);
        }
#if USBPD_PR_SWAP_ENABLE
        else if (dev->command == 2U)
        {
            dev->command = 0;
            usbpd_enter(dev, USBPD_STATE_SWAP_SEND, dev->now_ms);
        }
#endif
        break;
    case USBPD_STATE_SOFT_SEND:
        usbpd_send(dev, USBPD_CTRL_SOFT_RESET, NULL, 0, USBPD_STATE_SOFT_WAIT);
        break;
    case USBPD_STATE_SOFT_WAIT:
        if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SENDER_RESPONSE))
        {
            usbpd_start_hard_reset(dev, USBPD_ERR_TIMEOUT);
        }
        break;
    case USBPD_STATE_RESTART:
        dev->caps_count = 0;
        usbpd_enter(dev, dev->role == USBPD_ROLE_SOURCE ? USBPD_STATE_SRC_SEND_CAP :
                     USBPD_STATE_SNK_WAIT_CAP, dev->now_ms);
        break;
    case USBPD_STATE_ERROR_RECOVERY:
        if (usbpd_expired(dev->now_ms, dev->state_at, dev->recovery_wait_ms) &&
            status->source_off && status->sink_off && !status->fault &&
            (dev->role == USBPD_ROLE_SINK || status->vbus_mv <= USBPD_SAFE_ZERO_MV))
        {
            usbpd_enter(dev, USBPD_STATE_UNATTACHED, dev->now_ms);
        }
        else if (usbpd_expired(dev->now_ms, dev->state_at, USBPD_T_SAFE_ZERO))
        {
            (void)usbpd_io_shutdown(dev->io_ctx);
            dev->fault_reason = USBPD_ERR_TIMEOUT;
            usbpd_enter(dev, USBPD_STATE_FAULT, dev->now_ms);
            usbpd_emit(dev, USBPD_EVENT_ERROR, USBPD_ERR_TIMEOUT);
        }
        break;
    default:
        break;
    }
}

/* ═══════════════════ 底层事件与公共 API ═══════════════════ */
/**
 * @brief   处理异步发送完成，失败不能推进 MessageID。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   event 待处理事件或要上报的事件类型。
 */
static void
usbpd_transmit_event(usbpd_dev_t *dev, const usbpd_io_event_t *event)
{
    usbpd_state_t previous = dev->state;

    if (!dev->tx_busy)
    {
        usbpd_protocol_error(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    dev->tx_busy = false;
    if (event->kind == USBPD_IO_EVENT_TX_DONE)
    {
        dev->tx_id = (uint8_t)((dev->tx_id + 1U) & 7U);
        usbpd_enter(dev, dev->after_tx, event->at_ms);
        if (previous == USBPD_STATE_SRC_SEND_CAP)
        {
            dev->pd_seen = true;
            dev->hard_count = 0;
        }
        return;
    }
    if (dev->after_tx == USBPD_STATE_RESTART)
    {
        usbpd_start_hard_reset(dev, USBPD_ERR_IO);
        return;
    }
#if USBPD_PR_SWAP_ENABLE
    if (dev->after_tx == USBPD_STATE_SWAP_BEGIN)
    {
        usbpd_recover(dev, USBPD_ERR_IO);
        return;
    }
#endif
    if (event->kind == USBPD_IO_EVENT_TX_DISCARD && !dev->swap_active)
    {
        /* 对端消息优先：保留合同，取消应用事务，下一 FIFO 事件是导致丢弃的 RX。 */
        dev->tx_id = (uint8_t)((dev->tx_id + 1U) & 7U);
        usbpd_finish_operation(dev, USBPD_ERR_CANCELED);
        usbpd_enter(dev, dev->contract.valid ? USBPD_STATE_READY :
                     (dev->role == USBPD_ROLE_SOURCE ? USBPD_STATE_SRC_CAP_RETRY :
                                                       USBPD_STATE_SNK_WAIT_CAP), event->at_ms);
        return;
    }
    if (previous == USBPD_STATE_SRC_SEND_CAP && !dev->contract.valid)
    {
        usbpd_enter(dev, USBPD_STATE_SRC_CAP_RETRY, dev->tx_at);
        return;
    }
    usbpd_protocol_error(dev, USBPD_ERR_IO);
}

/**
 * @brief   处理有时间戳的 FIFO 事件；硬复位先使所有旧协议事务失效。
 * @details 仅由任务上下文的内部流程调用，不得从 ISR 或应用回调重入。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @param   event 待处理事件或要上报的事件类型。
 */
static void
usbpd_handle_event(usbpd_dev_t *dev, const usbpd_io_event_t *event)
{
    if (!dev->connected)
    {
        return;
    }
    if ((uint32_t)(dev->now_ms - event->at_ms) >= 0x80000000UL)
    {
        usbpd_recover(dev, USBPD_ERR_PROTOCOL);
        return;
    }
    if (event->kind == USBPD_IO_EVENT_HARD_RESET)
    {
        usbpd_hard_reset_received(dev, event->at_ms);
        usbpd_emit(dev, USBPD_EVENT_HARD_RESET, USBPD_OK);
    }
    else if (event->kind == USBPD_IO_EVENT_RESET_SENT)
    {
        if (dev->state == USBPD_STATE_HARD_SEND)
        {
            usbpd_hard_reset_received(dev, event->at_ms);
        }
    }
    else if (event->kind == USBPD_IO_EVENT_ERROR)
    {
        usbpd_recover(dev, USBPD_ERR_IO);
    }
    else if (dev->state >= USBPD_STATE_HARD_SEND &&
             dev->state <= USBPD_STATE_HARD_WAIT_FIVE)
    {
        /* 电源恢复期间旧 SOP 消息不应重新激活合同。 */
        return;
    }
    else if (event->kind == USBPD_IO_EVENT_RX)
    {
        usbpd_receive(dev, event);
    }
    else if (event->kind == USBPD_IO_EVENT_TX_DONE ||
             event->kind == USBPD_IO_EVENT_TX_FAILED ||
             event->kind == USBPD_IO_EVENT_TX_DISCARD)
    {
        usbpd_transmit_event(dev, event);
    }
}

/**
 * @brief   所有公开运行期 API 共享同实例重入和生命周期检查。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   dev 当前协议实例；调用者保证串行访问。
 * @retval  USBPD_OK 可调用；ERR_PARAM/NOT_READY/BUSY 表示调用条件不满足。
 */
static usbpd_result_t
usbpd_check_device(const usbpd_dev_t *dev)
{
    if (dev == NULL)
    {
        return USBPD_ERR_PARAM;
    }
    if (!dev->initialized)
    {
        return USBPD_ERR_NOT_READY;
    }
    if (dev->task_active)
    {
        return USBPD_ERR_BUSY;
    }
    return USBPD_OK;
}

/**
 * @brief   能力表必须从 5V 开始、递增且不超出首版能力范围。
 * @details 仅访问传入数据，不调用阻塞接口。
 * @param   pdos 按电压递增排列的本机固定能力表。
 * @param   count 数据对象或能力表元素数。
 * @retval  true 条件满足或操作成功；false 条件不满足或操作失败。
 */
static bool
usbpd_valid_table(const usbpd_fixed_pdo_t *pdos, uint8_t count)
{
    uint8_t index;
    uint16_t previous = 0;

    if (count > USBPD_MAX_PDOS)
    {
        return false;
    }
    for (index = 0; index < count; index++)
    {
        if (pdos[index].voltage_mv < 5000U ||
            pdos[index].voltage_mv > USBPD_MAX_VOLTAGE_MV ||
            pdos[index].voltage_mv % 50U != 0U ||
            pdos[index].voltage_mv <= previous ||
            pdos[index].current_ma == 0U ||
            pdos[index].current_ma > USBPD_MAX_CURRENT_MA ||
            pdos[index].current_ma % 10U != 0U)
        {
            return false;
        }
        previous = pdos[index].voltage_mv;
    }
    return count == 0U || pdos[0].voltage_mv == 5000U;
}

/* USBPD_Init 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_Init(usbpd_dev_t *dev, void *io_ctx, const usbpd_config_t *config,
            uint32_t now_ms)
{
    bool mode_ok = false;
    usbpd_config_t config_copy;

    if (dev == NULL || config == NULL)
    {
        return USBPD_ERR_PARAM;
    }
    if (dev->initialized || dev->task_active)
    {
        return USBPD_ERR_BUSY;
    }
#if USBPD_SINK_ENABLE
    if (!usbpd_valid_table(config->sink_pdos, config->sink_count))
    {
        return USBPD_ERR_PARAM;
    }
    if (config->mode == USBPD_MODE_SINK && config->sink_count != 0U)
    {
        mode_ok = true;
    }
#endif
#if USBPD_SOURCE_ENABLE
    if (!usbpd_valid_table(config->source_pdos, config->source_count) ||
        (config->source_count != 0U && config->source_pdos[0].current_ma < 500U))
    {
        return USBPD_ERR_PARAM;
    }
    if (config->mode == USBPD_MODE_SOURCE && config->source_count != 0U)
    {
        mode_ok = true;
    }
#endif
#if USBPD_DRP_ENABLE
    if (config->mode == USBPD_MODE_DRP && config->sink_count != 0U &&
        config->source_count != 0U)
    {
        mode_ok = true;
    }
#endif
    if (!mode_ok)
    {
        return USBPD_ERR_PARAM;
    }
    config_copy = *config;
    memset(dev, 0, sizeof(*dev));
    dev->config = config_copy;
    dev->io_ctx = io_ctx;
    dev->now_ms = now_ms;
    if (usbpd_io_init(io_ctx) != DRV_IO_OK)
    {
        (void)usbpd_io_shutdown(io_ctx);
        (void)usbpd_io_deinit(io_ctx);
        return USBPD_ERR_IO;
    }
    dev->initialized = true;
    usbpd_enter(dev, USBPD_STATE_UNATTACHED, now_ms);
    return USBPD_OK;
}

/* USBPD_Deinit 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_Deinit(usbpd_dev_t *dev)
{
    usbpd_result_t result = usbpd_check_device(dev);
    int shutdown_result;
    int deinit_result;

    if (result != USBPD_OK)
    {
        return result;
    }
    shutdown_result = usbpd_io_shutdown(dev->io_ctx);
    deinit_result = shutdown_result == DRV_IO_OK ? usbpd_io_deinit(dev->io_ctx) :
                                                DRV_IO_ERROR;
    if (shutdown_result != DRV_IO_OK || deinit_result != DRV_IO_OK)
    {
        dev->state = USBPD_STATE_FAULT;
        dev->contract.valid = false;
        return USBPD_ERR_IO;
    }
    memset(dev, 0, sizeof(*dev));
    return USBPD_OK;
}

/* USBPD_Task 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_Task(usbpd_dev_t *dev, uint32_t now_ms)
{
    usbpd_result_t result = usbpd_check_device(dev);
    usbpd_io_status_t status;
    usbpd_io_event_t event;
    uint8_t index;

    if (result != USBPD_OK)
    {
        return result;
    }
    if (dev->state == USBPD_STATE_FAULT)
    {
        return USBPD_ERR_FAULT;
    }
    if ((uint32_t)(now_ms - dev->now_ms) >= 0x80000000UL)
    {
        return USBPD_ERR_PARAM;
    }
    dev->task_active = true;
    dev->now_ms = now_ms;
    if (!usbpd_read_status(dev, &status))
    {
        dev->task_active = false;
        return dev->fault_reason;
    }
    /* 电源复位完成可先推进，以免对端紧随 5V 恢复的 Source_Cap 被忽略。 */
    if (dev->state == USBPD_STATE_HARD_WAIT_FIVE)
    {
        usbpd_step(dev, &status);
    }
    for (index = 0; index < USBPD_EVENT_BUDGET; index++)
    {
        if (dev->state == USBPD_STATE_FAULT)
        {
            break;
        }
        if (!usbpd_io_check(dev, usbpd_io_get_event(dev->io_ctx, now_ms, &event)))
        {
            break;
        }
        if (event.kind == USBPD_IO_EVENT_NONE)
        {
            break;
        }
        usbpd_handle_event(dev, &event);
        if (dev->state == USBPD_STATE_FAULT)
        {
            break;
        }
        /* 每次动作后重新读取 ready，不能把上一个目标的状态当作新目标完成。 */
        if (!usbpd_read_status(dev, &status))
        {
            break;
        }
        usbpd_step(dev, &status);
    }
    if (dev->state != USBPD_STATE_FAULT &&
        usbpd_read_status(dev, &status))
    {
        usbpd_check_detach(dev, &status);
        if (dev->tx_busy && usbpd_expired(now_ms, dev->tx_at, USBPD_T_TX_WATCHDOG))
        {
            usbpd_protocol_error(dev, USBPD_ERR_TIMEOUT);
        }
        if (dev->wait_active && usbpd_expired(now_ms, dev->wait_at, USBPD_T_REQUEST_WAIT))
        {
            dev->wait_active = false;
        }
        usbpd_step(dev, &status);
    }
    dev->task_active = false;
    return dev->state == USBPD_STATE_FAULT ? dev->fault_reason : USBPD_OK;
}

/* USBPD_GetStatus 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_GetStatus(const usbpd_dev_t *dev, usbpd_status_t *status)
{
    usbpd_result_t result = usbpd_check_device(dev);

    if (result != USBPD_OK)
    {
        return result;
    }
    if (status == NULL)
    {
        return USBPD_ERR_PARAM;
    }
    status->state = dev->state;
    status->role = dev->role;
    status->connected = dev->connected;
    status->busy = dev->operation != 0U;
    status->dfp = dev->dfp;
    status->cc_line = dev->cc_line;
    status->contract = dev->contract;
    return USBPD_OK;
}

/* USBPD_SetEventCallback 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_SetEventCallback(usbpd_dev_t *dev, usbpd_event_cb_t callback, void *user_ctx)
{
    usbpd_result_t result = usbpd_check_device(dev);

    if (result == USBPD_OK)
    {
        dev->callback = callback;
        dev->callback_ctx = user_ctx;
    }
    return result;
}

#if USBPD_SINK_ENABLE
/* USBPD_GetSourceCapabilities 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_GetSourceCapabilities(const usbpd_dev_t *dev, usbpd_pdo_t *pdos,
                            uint8_t capacity, uint8_t *count)
{
    usbpd_result_t result = usbpd_check_device(dev);

    if (result != USBPD_OK)
    {
        return result;
    }
    if (count == NULL || (capacity != 0U && pdos == NULL))
    {
        return USBPD_ERR_PARAM;
    }
    *count = dev->peer_count;
    if (dev->peer_count == 0U)
    {
        return USBPD_ERR_NOT_READY;
    }
    if (capacity < dev->peer_count)
    {
        return USBPD_ERR_PARAM;
    }
    memcpy(pdos, dev->peer_pdos, (size_t)dev->peer_count * sizeof(*pdos));
    return USBPD_OK;
}

/* USBPD_RequestFixed 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_RequestFixed(usbpd_dev_t *dev, uint8_t pdo_index, uint16_t current_ma)
{
    usbpd_result_t result = usbpd_check_device(dev);
    const usbpd_pdo_t *pdo;
    uint16_t limit;

    if (result != USBPD_OK)
    {
        return result;
    }
    if (dev->operation != 0U || dev->tx_busy)
    {
        return USBPD_ERR_BUSY;
    }
    if (!dev->connected || dev->role != USBPD_ROLE_SINK || !usbpd_ready(dev) ||
        dev->peer_count == 0U)
    {
        return USBPD_ERR_NOT_READY;
    }
    if (dev->wait_active)
    {
        return USBPD_ERR_WAIT;
    }
    if (pdo_index == 0U || pdo_index > dev->peer_count || current_ma == 0U ||
        current_ma > USBPD_MAX_CURRENT_MA || current_ma % 10U != 0U)
    {
        return USBPD_ERR_PARAM;
    }
    pdo = &dev->peer_pdos[pdo_index - 1U];
    limit = usbpd_sink_limit(dev, pdo->voltage_mv);
    if (!pdo->fixed || limit == 0U || pdo->voltage_mv > USBPD_MAX_VOLTAGE_MV)
    {
        return USBPD_ERR_NOT_SUPPORTED;
    }
    if (current_ma > pdo->current_ma || current_ma > limit)
    {
        return USBPD_ERR_PARAM;
    }
    dev->target.valid = false;
    dev->target.pdo_index = pdo_index;
    dev->target.voltage_mv = pdo->voltage_mv;
    dev->target.current_ma = current_ma;
    dev->command = 1;
    dev->operation = 1;
    return USBPD_OK;
}
#endif

#if USBPD_PR_SWAP_ENABLE
/* USBPD_RequestPowerRoleSwap 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_RequestPowerRoleSwap(usbpd_dev_t *dev)
{
    usbpd_result_t result = usbpd_check_device(dev);

    if (result != USBPD_OK)
    {
        return result;
    }
    if (!usbpd_dual_capable(dev))
    {
        return USBPD_ERR_NOT_SUPPORTED;
    }
    if (dev->operation != 0U || dev->tx_busy)
    {
        return USBPD_ERR_BUSY;
    }
    if (dev->state != USBPD_STATE_READY || !dev->contract.valid)
    {
        return USBPD_ERR_NOT_READY;
    }
    if (dev->wait_active)
    {
        return USBPD_ERR_WAIT;
    }
    dev->command = 2;
    dev->operation = 2;
    return USBPD_OK;
}

/* USBPD_SetSwapPolicy 的完整调用契约见 usbpd.h。 */
usbpd_result_t
USBPD_SetSwapPolicy(usbpd_dev_t *dev, bool source_to_sink, bool sink_to_source)
{
    usbpd_result_t result = usbpd_check_device(dev);

    if (result == USBPD_OK)
    {
        dev->allow_source_to_sink = source_to_sink;
        dev->allow_sink_to_source = sink_to_source;
    }
    return result;
}
#endif
