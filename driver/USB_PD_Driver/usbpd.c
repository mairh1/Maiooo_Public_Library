/**
 * @file    usbpd.c
 * @brief   USB PD 协议核心实现
 * @details 可移植的 PD Sink 协议栈，承载原样保留的协商流程：
 *          - CC 接入消抖检测与通道选择
 *          - 消息头装载、报文发送与 GoodCRC 重试
 *          - SRC_CAP 解析保存、PDO 申请（REQUEST/ACCEPT/PS_RDY）
 *          - 软复位 / 硬复位异常恢复
 *          - GET_SNK_CAP / GET_SRC_CAP_EX / GET_STATUS / VCONN_SWAP /
 *            VDM 等请求的应答
 *          本文件为纯 C99 代码：不包含任何厂商头文件，不访问寄存器，
 *          不直接 printf，全部硬件与平台差异经 usbpd_io.h 移植接口隔离。
 * @note    依赖：usbpd.h、usbpd_io.h、string.h。不依赖任何 app / bsp
 *          层头文件。
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-08-18
 */

#include <string.h>
#include "usbpd.h"
#include "usbpd_io.h"

/* ═══════════════════════════════════════════════════
 *  私有类型定义
 * ═══════════════════════════════════════════════════ */

/** @brief 协议标志位 */
typedef struct
{
    uint8_t connected      : 1;     /**< CC 物理层已连接 */
    uint8_t stop_det_chk   : 1;     /**< 1 = 暂停断开检测（协商关键阶段置位） */
    uint8_t pd_role        : 1;     /**< 数据角色：0 = UFP；1 = DFP */
    uint8_t pr_role        : 1;     /**< 电源角色：0 = Sink；1 = Source */
    uint8_t vdm_version    : 1;     /**< VDM 版本：0 = 1.0；1 = 2.0 */
    uint8_t pd_comm_succ   : 1;     /**< 1 = PD 通信已成功过至少一次 */
} usbpd_flags_t;

/** @brief 协议控制块（模块内部状态，外部经 USBPD_GetState 等访问） */
typedef struct
{
    usbpd_state_t   state;              /**< 协议状态机当前状态 */
    usbpd_state_t   state_last;         /**< 协议状态机上一状态 */
    uint8_t         msg_id;             /**< 已发送消息 ID（每次成功 +2） */
    uint8_t         det_cnt;            /**< 接入检测连续成功计数 */
    uint16_t        comm_timer;         /**< 协商公共计时器（ms） */
    uint8_t         req_pdo_idx;        /**< 最近一次申请的 PDO 序号（1 起） */
    uint16_t        busidle_timer;      /**< 总线空闲计时器（ms） */
    uint8_t         err_op_cnt;         /**< 异常操作计数（超限放弃重试） */
    uint8_t         adapter_idle_cnt;   /**< 适配器通信空闲计数 */
    usbpd_flags_t   flag;               /**< 标志位集合 */
} usbpd_ctl_t;

/* ═══════════════════════════════════════════════════
 *  私有宏定义
 * ═══════════════════════════════════════════════════ */

#define USBPD_MSG_SRC_CAP            0x01    /**< 数据消息：Source 能力 */
#define USBPD_MSG_REQUEST            0x02    /**< 数据消息：电源请求 */
#define USBPD_MSG_ACCEPT             0x03    /**< 控制消息：接受 */
#define USBPD_MSG_REJECT             0x04    /**< 控制消息：拒绝 */
#define USBPD_MSG_SNK_CAP            0x04    /**< 数据消息：Sink 能力 */
#define USBPD_MSG_PS_RDY             0x06    /**< 控制消息：电源就绪 */
#define USBPD_MSG_GET_SNK_CAP        0x08    /**< 控制消息：读取 Sink 能力 */
#define USBPD_MSG_VCONN_SWAP         0x0B    /**< 控制消息：VCONN 交换 */
#define USBPD_MSG_WAIT               0x0C    /**< 控制消息：等待 */
#define USBPD_MSG_SOFT_RESET         0x0D    /**< 控制消息：软复位 */
#define USBPD_MSG_VENDOR_DEFINED     0x0F    /**< 数据消息：厂商自定义 VDM */
#define USBPD_MSG_GET_SRC_CAP_EX     0x11    /**< 控制消息：读取扩展 Source 能力 */
#define USBPD_MSG_GET_STATUS         0x12    /**< 控制消息：读取状态 */
#define USBPD_MSG_GET_STATUS_R       0x02    /**< 扩展消息：状态应答（Ext=1 时编码复用） */

#define USBPD_PDO_MAX_CNT            7       /**< 单帧 SRC_CAP 最多 PDO 数（NDO 为 3bit） */
#define USBPD_ADAPTER_CAP_SIZE       30      /**< 适配器 SrcCap 缓冲大小（1 计数 + 7 PDO） */

/* 状态机超时门限（ms），与原实现一致 */
#define USBPD_SRC_CAP_TIMEOUT_MS     999     /**< SRC_CONNECT 态等待 SRC_CAP 超时 */
#define USBPD_RSP_TIMEOUT_MS         499     /**< 等待 ACCEPT / PS_RDY 超时 */
#define USBPD_ERR_OP_MAX             5       /**< 异常重试上限（超过转入 IDLE） */

/* ═══════════════════════════════════════════════════
 *  私有数据
 * ═══════════════════════════════════════════════════ */

/** 5V/3A 固定 PDO（上电默认 SrcCap 镜像） */
static const uint8_t s_src_cap_5v3a_tab[4]  = { 0x2C, 0x91, 0x01, 0x3E };

/** 5V/2A 固定 PDO */
static const uint8_t s_src_cap_5v2a_tab[4]  = { 0xC8, 0x90, 0x01, 0x3E };

/** 5V/1A Sink 能力（GET_SNK_CAP 应答） */
static const uint8_t s_sink_cap_5v1a_tab[4] = { 0x64, 0x90, 0x01, 0x36 };

/** 扩展 Source 能力（PD3.0 GET_SRC_CAP_EX 应答） */
static const uint8_t s_src_cap_ext_tab[28] =
{
    0x18, 0x80, 0x63, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x07, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x12, 0x00, 0x00,
};

/** 扩展状态（PD3.0 GET_STATUS 应答） */
static const uint8_t s_status_ext_tab[8] =
{
    0x06, 0x80, 0x16, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

static usbpd_ctl_t    s_pd_ctl;                                 /**< 协议控制块 */
static uint8_t        s_adapter_src_cap[USBPD_ADAPTER_CAP_SIZE];/**< 适配器 SrcCap（含 PDO 计数） */
static uint8_t        s_pdo_count;                              /**< 有效固定 PDO 数量 */
static usbpd_pdo_t    s_pdo_list[USBPD_PDO_MAX_CNT];            /**< 解析后的 PDO 列表（事件上报用） */

static uint8_t USBPD_ALIGNED_4 s_tx_buf[USBPD_MSG_BUF_SIZE];    /**< 发送缓冲（地址写入 DMA，需 4 字节对齐） */
static uint8_t        s_rx_msg[USBPD_MSG_BUF_SIZE];             /**< 接收报文副本（任务上下文处理） */

static volatile uint8_t s_tick_ms;                              /**< 1ms 节拍计数（TickIsr 递增） */
static uint8_t        s_tick_last;                              /**< 上次任务读取时的节拍值 */
static uint8_t        s_ms_dlt;                                 /**< 本次任务周期的毫秒增量 */

static usbpd_event_cb_t s_event_cb;                             /**< 应用层事件回调 */

/* ═══════════════════════════════════════════════════
 *  私有函数前置声明
 * ═══════════════════════════════════════════════════ */

static void    usbpd_phy_reset(void);
static void    usbpd_det_proc(void);
static void    usbpd_isr_event_proc(void);
static void    usbpd_state_proc(void);
static void    usbpd_msg_proc(void);
static void    usbpd_load_header(uint8_t ext, uint8_t msg_type);
static uint8_t usbpd_send_handle(const uint8_t *pbuf, uint8_t len);
static void    usbpd_pdo_request(uint8_t pdo_index);
static void    usbpd_save_src_cap(void);
static void    usbpd_pdo_analyse(uint8_t pdo_idx, const uint8_t *srccap,
                                 uint16_t *current, uint16_t *voltage);
static void    usbpd_emit(usbpd_event_t evt, const usbpd_event_info_t *info);

/* ═══════════════════════════════════════════════════
 *  公有 API 实现
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   初始化 USB PD 协议栈
 * @details 移植层初始化成功后复位协议变量，装入默认 SrcCap 镜像，
 *          复位 PHY 并进入接收模式。
 * @retval  USBPD_OK         初始化成功
 * @retval  USBPD_ERR_IO     移植层初始化失败
 */
usbpd_result_t
USBPD_Init(void)
{
    if (usbpd_io_init() != USBPD_IO_OK)
    {
        return USBPD_ERR_IO;
    }

    memset(&s_pd_ctl, 0x00, sizeof(s_pd_ctl));
    memset(s_pdo_list, 0x00, sizeof(s_pdo_list));
    s_pdo_count = 0;

    /* 装入默认 SrcCap 镜像（5V/3A），首次 SRC_CAP 到达后覆盖 */
    s_adapter_src_cap[0] = 1;
    memcpy(&s_adapter_src_cap[1], s_src_cap_5v3a_tab, sizeof(s_src_cap_5v3a_tab));

    usbpd_phy_reset();
    usbpd_io_enter_rx();

    return USBPD_OK;
}

/**
 * @brief   1ms 节拍喂入（SysTick 中断服务中调用）
 */
void
USBPD_TickIsr(void)
{
    s_tick_ms++;
}

/**
 * @brief   协议栈主任务（主循环中持续调用）
 */
void
USBPD_Task(void)
{
    /* 计算本周期毫秒增量（uint8 自然回绕，与原实现一致） */
    if (s_tick_ms != s_tick_last)
    {
        s_ms_dlt = s_tick_ms - s_tick_last;
        s_tick_last = s_tick_ms;
    }
    else
    {
        s_ms_dlt = 0;
    }

    usbpd_det_proc();
    usbpd_isr_event_proc();
    usbpd_state_proc();
    usbpd_msg_proc();
}

/**
 * @brief   获取当前协议状态机状态
 */
usbpd_state_t
USBPD_GetState(void)
{
    return s_pd_ctl.state;
}

/**
 * @brief   查询 CC 物理连接状态
 * @retval  1 已连接；0 未连接
 */
uint8_t
USBPD_IsConnected(void)
{
    return (uint8_t)s_pd_ctl.flag.connected;
}

/**
 * @brief   注册事件回调
 * @param   cb  回调函数指针，NULL 取消注册
 */
void
USBPD_SetEventCallback(usbpd_event_cb_t cb)
{
    s_event_cb = cb;
}

/* ═══════════════════════════════════════════════════
 *  私有函数实现
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   复位协议状态与 PHY
 * @details 进入 Sink 模式、恢复断开检测、状态机回到空闲。
 */
static void
usbpd_phy_reset(void)
{
    usbpd_io_sink_init();
    s_pd_ctl.flag.stop_det_chk = 0;                     /* 默认允许断开检测 */
    s_pd_ctl.state = USBPD_STA_IDLE;
    s_pd_ctl.flag.pd_comm_succ = 0;
}

/**
 * @brief   CC 接入消抖检测处理
 * @details 未连接时对硬件检测结果做连续 USBPD_DET_DEBOUNCE 次确认，
 *          确认后选择对应 CC 通道并进入 SRC_CONNECT 态。
 */
static void
usbpd_det_proc(void)
{
    usbpd_cc_line_t cc;

    if (s_pd_ctl.flag.connected)
    {
        /* 已连接：Sink 场景的拔出检测依赖 Vbus 电压检测，
         * 协议栈本身不重复处理 */
        return;
    }

    cc = usbpd_io_detect_cc();
    if (cc == USBPD_CC_NONE)
    {
        s_pd_ctl.det_cnt = 0;
        return;
    }

    s_pd_ctl.det_cnt++;
    if (s_pd_ctl.det_cnt < USBPD_DET_DEBOUNCE)
    {
        return;
    }

    s_pd_ctl.det_cnt = 0;
    s_pd_ctl.flag.connected = 1;

    if (s_pd_ctl.flag.stop_det_chk == 0)
    {
        /* 检测到非零 CC 即说明 Sink 下拉已使能，选择通信通道 */
        usbpd_io_select_cc(cc);
        s_pd_ctl.state = USBPD_STA_SRC_CONNECT;

        USBPD_LOG("CC%d attach\r\n", (int)cc);

        usbpd_event_info_t info = { 0 };
        info.cc_line = cc;
        usbpd_emit(USBPD_EVT_ATTACHED, &info);
    }

    s_pd_ctl.comm_timer = 0;
}

/**
 * @brief   中断侧累积事件处理
 * @details 将中断内置位的事件（硬复位 / 缓冲错误）在任务上下文
 *          上报，中断内不做打印与耗时处理。
 */
static void
usbpd_isr_event_proc(void)
{
    uint8_t evt = usbpd_io_get_isr_events();
    usbpd_event_info_t info = { 0 };

    if ((evt & USBPD_IO_ISR_EVT_PHY_RESET) != 0)
    {
        USBPD_LOG("IF_RX_RESET\r\n");
        usbpd_emit(USBPD_EVT_PHY_RESET, &info);
    }

    if ((evt & USBPD_IO_ISR_EVT_BUF_ERROR) != 0)
    {
        USBPD_LOG("BUFERR\r\n");
        usbpd_emit(USBPD_EVT_BUF_ERROR, &info);
    }
}

/**
 * @brief   协议状态机推进
 */
static void
usbpd_state_proc(void)
{
    uint8_t status;

    /* 总线空闲计时 */
    s_pd_ctl.busidle_timer += s_ms_dlt;

    switch (s_pd_ctl.state)
    {
    case USBPD_STA_DISCONNECT:
        /* 已断开：复位协议与 PHY */
        USBPD_LOG("Disconnect\r\n");
        usbpd_phy_reset();
        break;

    case USBPD_STA_SRC_CONNECT:
        /* 1S 内未收到 SRC_CAP 则复位重试，连续 5 次失败放弃 */
        s_pd_ctl.comm_timer += s_ms_dlt;
        if (s_pd_ctl.comm_timer > USBPD_SRC_CAP_TIMEOUT_MS)
        {
            s_pd_ctl.err_op_cnt++;
            if (s_pd_ctl.err_op_cnt > USBPD_ERR_OP_MAX)
            {
                s_pd_ctl.err_op_cnt = 0;
                s_pd_ctl.state = USBPD_STA_IDLE;
            }
            else
            {
                usbpd_phy_reset();
            }
        }
        break;

    case USBPD_STA_RX_ACCEPT_WAIT:
        /* 等待 ACCEPT */
    case USBPD_STA_RX_PS_RDY_WAIT:
        /* 等待 PS_RDY：500ms 超时转软复位 */
        s_pd_ctl.comm_timer += s_ms_dlt;
        if (s_pd_ctl.comm_timer > USBPD_RSP_TIMEOUT_MS)
        {
            s_pd_ctl.flag.stop_det_chk = 0;             /* 恢复连接检测 */
            s_pd_ctl.state = USBPD_STA_TX_SOFTRST;
            s_pd_ctl.comm_timer = 0;
        }
        break;

    case USBPD_STA_RX_PS_RDY:
        /* PS_RDY 已收到：协商完成，回到空闲 */
        s_pd_ctl.state = USBPD_STA_IDLE;
        break;

    case USBPD_STA_TX_SOFTRST:
        /* 发送软复位：成功回空闲，失败升级硬复位 */
        usbpd_load_header(0x00, USBPD_MSG_SOFT_RESET);
        status = usbpd_send_handle(NULL, 0);
        if (status == USBPD_IO_OK)
        {
            s_pd_ctl.state = USBPD_STA_IDLE;
        }
        else
        {
            s_pd_ctl.state = USBPD_STA_TX_HRST;
        }
        s_pd_ctl.comm_timer = 0;
        break;

    case USBPD_STA_TX_HRST:
        /* 发送硬复位后回空闲 */
        s_pd_ctl.flag.stop_det_chk = 1;
        (void)usbpd_io_phy_send(1, NULL, 0, USBPD_IO_SOP_HARD_RESET);
        usbpd_io_enter_rx();
        s_pd_ctl.state = USBPD_STA_IDLE;
        s_pd_ctl.comm_timer = 0;
        break;

    default:
        break;
    }
}

/**
 * @brief   已收报文的解析与应答
 */
static void
usbpd_msg_proc(void)
{
    uint8_t len;
    uint8_t msg_type;

    len = usbpd_io_read_packet(s_rx_msg, sizeof(s_rx_msg));
    if (len == 0)
    {
        return;
    }

    /* 适配器通信空闲计数复位 */
    s_pd_ctl.adapter_idle_cnt = 0;

    msg_type = s_rx_msg[0] & 0x1F;
    switch (msg_type)
    {
    case USBPD_MSG_SRC_CAP:
    {
        usbpd_io_delay_ms(5);
        s_pd_ctl.flag.stop_det_chk = 0;                 /* 恢复断开检测 */

        usbpd_save_src_cap();

        /* 逐组解析固定 PDO 电压电流并上报应用层 */
        for (uint8_t i = 0; i < s_pdo_count; i++)
        {
            usbpd_pdo_analyse((uint8_t)(i + 1), &s_rx_msg[2],
                              &s_pdo_list[i].current_ma, &s_pdo_list[i].voltage_mv);
            USBPD_LOG("PDO:%d\r\nCurrent:%d mA\r\nVoltage:%d mV\r\n",
                      (int)(i + 1), (int)s_pdo_list[i].current_ma, (int)s_pdo_list[i].voltage_mv);
        }

        usbpd_event_info_t info = { 0 };
        info.pdo_list = s_pdo_list;
        info.pdo_count = s_pdo_count;
        usbpd_emit(USBPD_EVT_SRCCAP_READY, &info);

        /* 默认申请配置指定的 PDO（通常为 5V 档） */
        usbpd_pdo_request(USBPD_DEFAULT_PDO_IDX);
        break;
    }

    case USBPD_MSG_ACCEPT:
        /* 收到 ACCEPT：等待 PS_RDY */
        s_pd_ctl.state = USBPD_STA_RX_PS_RDY_WAIT;
        s_pd_ctl.comm_timer = 0;
        break;

    case USBPD_MSG_PS_RDY:
    {
        /* 收到 PS_RDY：协商完成 */
        USBPD_LOG("Success\r\n");
        s_pd_ctl.state = USBPD_STA_RX_PS_RDY;

        usbpd_event_info_t info = { 0 };
        info.pdo_index = s_pd_ctl.req_pdo_idx;
        if ((s_pd_ctl.req_pdo_idx >= 1) && (s_pd_ctl.req_pdo_idx <= s_pdo_count))
        {
            info.voltage_mv = s_pdo_list[s_pd_ctl.req_pdo_idx - 1].voltage_mv;
            info.current_ma = s_pdo_list[s_pd_ctl.req_pdo_idx - 1].current_ma;
        }
        usbpd_emit(USBPD_EVT_PS_READY, &info);
        break;
    }

    case USBPD_MSG_WAIT:
        /* 收到 WAIT：多种请求都可能收到，需按具体场景分析，暂不处理 */
        break;

    case USBPD_MSG_GET_SNK_CAP:
        usbpd_io_delay_ms(1);
        usbpd_load_header(0x00, USBPD_MSG_SNK_CAP);
        (void)usbpd_send_handle(s_sink_cap_5v1a_tab, sizeof(s_sink_cap_5v1a_tab));
        break;

    case USBPD_MSG_SOFT_RESET:
        usbpd_io_delay_ms(1);
        usbpd_load_header(0x00, USBPD_MSG_ACCEPT);
        (void)usbpd_send_handle(NULL, 0);
        break;

    case USBPD_MSG_GET_SRC_CAP_EX:
        usbpd_io_delay_ms(1);
        usbpd_load_header(0x01, USBPD_MSG_SRC_CAP);
        (void)usbpd_send_handle(s_src_cap_ext_tab, sizeof(s_src_cap_ext_tab));
        break;

    case USBPD_MSG_GET_STATUS:
        usbpd_io_delay_ms(1);
        usbpd_load_header(0x01, USBPD_MSG_GET_STATUS_R);
        (void)usbpd_send_handle(s_status_ext_tab, sizeof(s_status_ext_tab));
        break;

    case USBPD_MSG_VCONN_SWAP:
        usbpd_io_delay_ms(1);
        usbpd_load_header(0x00, USBPD_MSG_REJECT);
        (void)usbpd_send_handle(NULL, 0);
        break;

    case USBPD_MSG_VENDOR_DEFINED:
        /* 结构化 VDM 请求：应答并置 ACK 标志位 */
        if ((s_rx_msg[2] & 0xC0) == 0)
        {
            usbpd_io_delay_ms(1);
            usbpd_load_header(0x00, USBPD_MSG_VENDOR_DEFINED);

            if ((s_rx_msg[3] & 0x60) == 0)
            {
                s_pd_ctl.flag.vdm_version = 0;
            }
            else
            {
                s_pd_ctl.flag.vdm_version = 1;
            }
            s_rx_msg[2] |= 0x80;
            (void)usbpd_send_handle(&s_rx_msg[2], 4);
        }
        break;

    default:
        USBPD_LOG("Unsupported Command\r\n");
        break;
    }

    /* 报文处理完毕，恢复接收 */
    usbpd_io_enter_rx();
    s_pd_ctl.busidle_timer = 0;
}

/**
 * @brief   装载 PD 消息头到发送缓冲
 * @param   ext       扩展消息标志（Bit15）
 * @param   msg_type  消息类型（Bit4-0）
 * @note    消息头位域：Bit15 扩展；Bit14-12 数据对象数；Bit11-9 消息 ID；
 *          Bit8 电源角色（0 = Sink）；Bit7-6 版本；Bit5 数据角色；
 *          Bit4-0 消息类型
 */
static void
usbpd_load_header(uint8_t ext, uint8_t msg_type)
{
    s_tx_buf[0] = msg_type;
    if (s_pd_ctl.flag.pd_role)
    {
        s_tx_buf[0] |= 0x20;
    }

#if USBPD_PD30_ENABLE
    s_tx_buf[0] |= 0x80;                                /* PD3.0 */
#else
    s_tx_buf[0] |= 0x40;                                /* PD2.0 */
#endif

    s_tx_buf[1] = s_pd_ctl.msg_id & 0x0E;
    if (s_pd_ctl.flag.pr_role)
    {
        s_tx_buf[1] |= 0x01;
    }
    if (ext)
    {
        s_tx_buf[1] |= 0x80;
    }
}

/**
 * @brief   发送事务处理（含 GoodCRC 等待与重试）
 * @param   pbuf  数据对象缓冲（不含消息头），NULL 表示纯控制消息
 * @param   len   数据对象字节数（须为 4 的倍数且不超过 28）
 * @retval  USBPD_IO_OK     发送成功（收到 GoodCRC）
 * @retval  USBPD_IO_ERROR  发送失败（参数非法 / 重试耗尽）
 */
static uint8_t
usbpd_send_handle(const uint8_t *pbuf, uint8_t len)
{
    uint8_t remain = USBPD_TX_RETRY_CNT;
    uint8_t ok = 0;
    uint8_t cnt;

    if (((len % 4) != 0) || (len > 28))
    {
        return USBPD_IO_ERROR;
    }

    cnt = len >> 2;
    s_tx_buf[1] |= (uint8_t)(cnt << 4);
    for (cnt = 0; cnt != len; cnt++)
    {
        s_tx_buf[2 + cnt] = pbuf[cnt];
    }

    while (remain--)
    {
        usbpd_io_irq_disable();
        if (usbpd_io_phy_send(1, s_tx_buf, (uint8_t)(len + 2), USBPD_IO_SOP_NORMAL) == USBPD_IO_OK)
        {
            if (usbpd_io_wait_goodcrc())
            {
                s_pd_ctl.msg_id += 2;
                ok = 1;
                break;
            }
        }
    }

    if (ok == 0)
    {
        USBPD_LOG("PD tx failed\r\n");
        usbpd_event_info_t info = { 0 };
        usbpd_emit(USBPD_EVT_TX_FAILED, &info);
    }

    /* 回到接收模式 */
    usbpd_io_enter_rx();
    return (ok != 0) ? USBPD_IO_OK : USBPD_IO_ERROR;
}

/**
 * @brief   申请指定 PDO（发送 REQUEST）
 * @param   pdo_index  PDO 序号（1 起，不超过当前有效 PDO 数）
 * @note    序号非法时不发送：记录错误并转入软复位流程恢复总线
 *          （替代原实现的死循环打印，仅异常路径行为变化）
 */
static void
usbpd_pdo_request(uint8_t pdo_index)
{
    uint16_t cur_ma = 0;
    uint16_t vol_mv = 0;

    if ((pdo_index > s_pdo_count) || (pdo_index == 0))
    {
        USBPD_LOG("pdo_index error!\r\n");

        usbpd_event_info_t info = { 0 };
        info.pdo_index = pdo_index;
        usbpd_emit(USBPD_EVT_TX_FAILED, &info);

        s_pd_ctl.state = USBPD_STA_TX_SOFTRST;
        s_pd_ctl.comm_timer = 0;
        return;
    }

    /* 将目标 PDO 复制到报文暂存区并组 REQUEST 数据对象 */
    memcpy(&s_rx_msg[2], &s_adapter_src_cap[4 * (pdo_index - 1) + 1], 4);
    usbpd_pdo_analyse(1, &s_rx_msg[2], &cur_ma, &vol_mv);
    USBPD_LOG("Request:\r\nCurrent:%d mA\r\nVoltage:%d mV\r\n", (int)cur_ma, (int)vol_mv);

    usbpd_load_header(0x00, USBPD_MSG_REQUEST);
    s_rx_msg[5] = 0x03;
    s_rx_msg[5] |= (uint8_t)(pdo_index << 4);
    s_rx_msg[3] = s_rx_msg[3] & 0x03;
    s_rx_msg[3] |= (uint8_t)(s_rx_msg[2] << 2);
    s_rx_msg[4] = s_rx_msg[3];
    s_rx_msg[4] <<= 2;
    s_rx_msg[4] = s_rx_msg[4] & 0x0C;
    s_rx_msg[4] |= (uint8_t)(s_rx_msg[2] >> 6);

    s_pd_ctl.req_pdo_idx = pdo_index;

    if (usbpd_send_handle(&s_rx_msg[2], 4) == USBPD_IO_OK)
    {
        s_pd_ctl.state = USBPD_STA_RX_ACCEPT_WAIT;
    }
    else
    {
        s_pd_ctl.state = USBPD_STA_TX_SOFTRST;
    }
    s_pd_ctl.comm_timer = 0;
    s_pd_ctl.flag.pd_comm_succ = 1;
}

/**
 * @brief   保存适配器 SrcCap 并剔除 PPS 段
 * @note    固定 PDO 位域：Bit31-30 固定电源；Bit29 双角色电源；
 *          Bit19-10 电压（50mV 步进）；Bit9-0 电流（10mA 步进）
 */
static void
usbpd_save_src_cap(void)
{
    uint8_t i;
    uint8_t ndo;

    /* 消息头 Bit14-12：数据对象数 */
    ndo = (s_rx_msg[1] >> 4) & 0x07;

    /* 遇到 PPS（Bit31-30 = 11）即截止，仅保留固定 PDO */
    for (i = 0; i < ndo; i++)
    {
        if ((s_rx_msg[2 + (i << 2) + 3] & 0xC0) == 0xC0)
        {
            break;
        }
    }

    s_pdo_count = i;

    /* 改写消息头的 NDO 与首个 PDO 的能力位后保存镜像 */
    s_rx_msg[5] = 0x3E;
    s_rx_msg[1] &= 0x8F;
    s_rx_msg[1] |= (uint8_t)(i << 4);
    s_adapter_src_cap[0] = i;
    memcpy(&s_adapter_src_cap[1], &s_rx_msg[2], (uint8_t)(i << 2));
}

/**
 * @brief   解析固定 PDO 的电压电流
 * @param   pdo_idx  PDO 序号（1 起）
 * @param   srccap   SrcCap 数据缓冲
 * @param   current  输出：电流（mA），可为 NULL
 * @param   voltage  输出：电压（mV），可为 NULL
 */
static void
usbpd_pdo_analyse(uint8_t pdo_idx, const uint8_t *srccap,
                  uint16_t *current, uint16_t *voltage)
{
    uint32_t temp32;

    temp32 = srccap[((pdo_idx - 1) << 2) + 0] +
             ((uint32_t)srccap[((pdo_idx - 1) << 2) + 1] << 8) +
             ((uint32_t)srccap[((pdo_idx - 1) << 2) + 2] << 16);

    if (current != NULL)
    {
        *current = (uint16_t)((temp32 & 0x000003FF) * 10);
    }

    if (voltage != NULL)
    {
        temp32 = temp32 >> 10;
        *voltage = (uint16_t)((temp32 & 0x000003FF) * 50);
    }
}

/**
 * @brief   派发事件到应用层回调
 * @param   evt   事件类型
 * @param   info  事件附带信息
 */
static void
usbpd_emit(usbpd_event_t evt, const usbpd_event_info_t *info)
{
    if (s_event_cb != NULL)
    {
        s_event_cb(evt, info);
    }
}
