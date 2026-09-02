/**
 * @file    usbpd_io_ch32l103.c
 * @brief   USB PD 移植层实现（CH32L103 USBPD 外设）
 * @details 实现 usbpd_io.h 定义的移植接口，承接全部硬件相关操作：
 *          - 外设时钟 / CC 引脚（PB6 = CC1，PB7 = CC2）初始化
 *          - BMC 物理层收发（DMA 搬运、SOP 序列编码）
 *          - CC 线比较器检测与通道选择
 *          - USBPD 中断服务（GoodCRC 自动应答）
 *          寄存器位定义在本文件内私有维护，不对外泄露；
 *          协议核心（usbpd.c）不包含本文件。
 * @note    1. BMC 收发定时常量按系统主频 96MHz 计算，若主频不同需
 *          同步调整 UPD_TMR_TX_96M / UPD_TMR_RX_96M；
 *          2. 中断内 Delay_Us(30) 为 PD 规范 GoodCRC 应答窗口
 *          （约 195us 内）的硬性时序要求，不可移除。
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-08-18
 */

#include <stdint.h>
#include <string.h>
#include "usbpd_io.h"
#include "ch32l103.h"
#include "debug.h"

/* ═══════════════════════════════════════════════════
 *  寄存器位定义（USBPD->CONFIG）
 * ═══════════════════════════════════════════════════ */

#define PD_FILT_ED              (1 << 0)    /**< CC 引脚输入滤波使能 */
#define PD_ALL_CLR             (1 << 1)     /**< 清全部中断标志（写脉冲） */
#define CC_SEL                  (1 << 2)    /**< PD 通信端口选择：0 = CC1；1 = CC2 */
#define PD_DMA_EN               (1 << 3)    /**< USBPD DMA 使能 */
#define IE_RX_ACT              (1 << 13)    /**< 接收完成中断使能 */
#define IE_RX_RESET            (1 << 14)    /**< 复位序列接收中断使能 */
#define IE_TX_END              (1 << 15)    /**< 发送完成中断使能 */

/* ═══════════════════════════════════════════════════
 *  寄存器位定义（USBPD->CONTROL）
 * ═══════════════════════════════════════════════════ */

#define PD_TX_EN                (1 << 0)    /**< 收发器发送使能 */
#define BMC_START               (1 << 1)    /**< BMC 启动 */

/* ═══════════════════════════════════════════════════
 *  寄存器位定义（USBPD->STATUS，写 1 清除）
 * ═══════════════════════════════════════════════════ */

#define BUF_ERR                 (1 << 2)    /**< 缓冲 / DMA 错误标志 */
#define IF_RX_BIT               (1 << 3)    /**< 接收位 / 5bit 中断标志 */
#define IF_RX_BYTE              (1 << 4)    /**< 接收字节 / SOP 中断标志 */
#define IF_RX_ACT               (1 << 5)    /**< 接收完成中断标志 */
#define IF_RX_RESET             (1 << 6)    /**< 复位序列接收中断标志 */
#define IF_TX_END               (1 << 7)    /**< 发送完成中断标志 */

#define MASK_PD_STAT            0x03        /**< 接收状态位掩码（STATUS[1:0]） */
#define PD_RX_SOP0              0x01        /**< 收到 SOP' 普通报文 */
#define BMC_AUX_INVALID         (0 << 0)    /**< BMC 辅助状态无效编码（以写 0 方式清辅助位） */

/* ═══════════════════════════════════════════════════
 *  寄存器位定义（USBPD->PORT_CC1 / PORT_CC2）
 * ═══════════════════════════════════════════════════ */

#define PA_CC_AI                (1 << 0)    /**< CC 比较器模拟输入状态 */
#define CC_PD                   (1 << 1)    /**< CC 下拉电阻使能（Sink） */
#define CC_PU_Mask              (3 << 2)    /**< CC 上拉电流位掩码 */
#define CC_PU_330               (1 << 2)    /**< 上拉 330uA（Source 默认档） */
#define CC_LVE                  (1 << 4)    /**< CC 低电平输出使能（发送期间） */
#define CC_CE                   (7 << 5)    /**< CC 电压比较器使能位掩码 */
#define CC_CMP_22               (2 << 5)    /**< 比较门限 0.22V（接入检测） */
#define CC_CMP_66               (4 << 5)    /**< 比较门限 0.66V（通信监听） */

/* ═══════════════════════════════════════════════════
 *  SOP 序列硬件编码（USBPD->TX_SEL）
 * ═══════════════════════════════════════════════════ */

#define TX_SEL1_SYNC1           (0 << 0)    /**< 0 = SYNC1 */
#define TX_SEL1_RST1            (1 << 0)    /**< 1 = RST1 */
#define TX_SEL2_SYNC1           (0 << 2)    /**< 00 = SYNC1 */
#define TX_SEL2_SYNC3           (1 << 2)    /**< 01 = SYNC3 */
#define TX_SEL2_RST1            (2 << 2)    /**< 1x = RST1 */
#define TX_SEL2_SYNC3_ALT       (3 << 2)    /**< 11 = SYNC3 */
#define TX_SEL3_SYNC1           (0 << 4)    /**< 00 = SYNC1 */
#define TX_SEL3_SYNC3           (1 << 4)    /**< 01 = SYNC3 */
#define TX_SEL3_RST1            (2 << 4)    /**< 1x = RST1 */
#define TX_SEL4_SYNC2           (0 << 6)    /**< 00 = SYNC2 */
#define TX_SEL4_SYNC3           (1 << 6)    /**< 01 = SYNC3 */
#define TX_SEL4_RST2            (2 << 6)    /**< 1x = RST2 */

/** SOP' 普通报文序列 */
#define UPD_SOP0                (TX_SEL1_SYNC1 | TX_SEL2_SYNC1 | TX_SEL3_SYNC1 | TX_SEL4_SYNC2)
/** SOP'' 调试探文序列 */
#define UPD_SOP1                (TX_SEL1_SYNC1 | TX_SEL2_SYNC1 | TX_SEL3_SYNC3 | TX_SEL4_SYNC3)
/** SOP''' 调试探文序列 */
#define UPD_SOP2                (TX_SEL1_SYNC1 | TX_SEL2_SYNC3_ALT | TX_SEL3_SYNC1 | TX_SEL4_SYNC3)
/** 硬复位序列 */
#define UPD_HARD_RESET          (TX_SEL1_RST1 | TX_SEL2_RST1 | TX_SEL3_RST1 | TX_SEL4_RST2)
/** 线缆复位序列 */
#define UPD_CABLE_RESET         (TX_SEL1_RST1 | TX_SEL2_SYNC1 | TX_SEL3_RST1 | TX_SEL4_SYNC3)

/* ═══════════════════════════════════════════════════
 *  BMC 定时常量（系统主频 96MHz）
 * ═══════════════════════════════════════════════════ */

#define UPD_TMR_TX_96M          (160 - 1)   /**< BMC 发送位定时 @96MHz */
#define UPD_TMR_RX_96M          (240 - 1)   /**< BMC 接收位定时 @96MHz */

/** @brief 发送完成等待超时循环次数（约 5ms @96MHz，防硬件异常卡死） */
#define TX_END_TIMEOUT_CNT      100000UL

/* ═══════════════════════════════════════════════════
 *  其他硬件定义
 * ═══════════════════════════════════════════════════ */

#define USBPD_IN_HVT            (1 << 9)    /**< AFIO：PD 引脚高门限输入（2.2V，降低通信功耗） */
#define PIN_CC1                 GPIO_Pin_6  /**< CC1 引脚：PB6 */
#define PIN_CC2                 GPIO_Pin_7  /**< CC2 引脚：PB7 */

/* ═══════════════════════════════════════════════════
 *  私有数据
 * ═══════════════════════════════════════════════════ */

static USBPD_ALIGNED_4 uint8_t s_rx_buf[USBPD_MSG_BUF_SIZE];   /**< 接收 DMA 缓冲（硬件直接写入） */
static USBPD_ALIGNED_4 uint8_t s_ack_buf[2];                   /**< GoodCRC 应答缓冲 */

static volatile uint8_t s_msg_ready;    /**< 报文就绪标志（中断置位，任务取走） */
static volatile uint8_t s_isr_events;   /**< 中断侧累积事件（ISR_EVT_xxx） */
static uint8_t s_auto_ack_role;         /**< GoodCRC 应答角色：0 = Sink；1 = Source */

/* ═══════════════════════════════════════════════════
 *  私有函数前置声明
 * ═══════════════════════════════════════════════════ */

static volatile uint32_t *ch32l103_cc_reg(uint8_t ccx);
static uint8_t ch32l103_sop_encode(uint8_t sop);
static void ch32l103_phy_start_tx(const uint8_t *buf, uint8_t len, uint8_t tx_sel);

/* ═══════════════════════════════════════════════════
 *  中断服务
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   USBPD 中断服务函数（向量名与启动文件绑定，不可改名）
 * @details 接收完成且非 GoodCRC 时，在 195us 应答窗口内自动回
 *          GoodCRC；应答发送完成后置就绪标志并关闭中断，交由
 *          协议核心在任务上下文处理报文。中断内只做标志位更新，
 *          不打印、不处理协议（硬复位 / 缓冲错误以事件标志转交
 *          任务上下文上报）。
 */
void USBPD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void
USBPD_IRQHandler(void)
{
    if ((USBPD->STATUS & IF_RX_ACT) != 0)
    {
        USBPD->STATUS |= IF_RX_ACT;                     /* 写 1 清除 */

        if ((USBPD->STATUS & MASK_PD_STAT) == PD_RX_SOP0)
        {
            if (USBPD->BMC_BYTE_CNT >= 6)
            {
                /* 收到 GoodCRC 本身时不应答，其余报文均需应答 */
                if ((USBPD->BMC_BYTE_CNT != 6) || ((s_rx_buf[0] & 0x1F) != USBPD_IO_MSG_GOODCRC))
                {
                    /* PD 规范要求约 195us 内发出 GoodCRC，此延时为
                     * 硬性时序约束，不可移除或放大 */
                    Delay_Us(30);

                    s_ack_buf[0] = 0x41;
                    s_ack_buf[1] = (uint8_t)((s_rx_buf[1] & 0x0E) | s_auto_ack_role);
                    USBPD->CONFIG |= IE_TX_END;
                    ch32l103_phy_start_tx(s_ack_buf, 2, UPD_SOP0);
                }
            }
        }
    }

    if ((USBPD->STATUS & IF_TX_END) != 0)
    {
        /* GoodCRC 发送完成：关闭低电平输出与中断，
         * 报文交由任务上下文处理（usbpd_io_read_packet） */
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;
        NVIC_DisableIRQ(USBPD_IRQn);
        s_msg_ready = 1;
        USBPD->STATUS |= IF_TX_END;                     /* 写 1 清除 */
    }

    if ((USBPD->STATUS & IF_RX_RESET) != 0)
    {
        USBPD->STATUS |= IF_RX_RESET;                   /* 写 1 清除 */
        usbpd_io_sink_init();                           /* 硬复位后重新进入 Sink 模式 */
        s_isr_events |= USBPD_IO_ISR_EVT_PHY_RESET;     /* 事件转任务上下文上报 */
    }

    if ((USBPD->STATUS & BUF_ERR) != 0)
    {
        USBPD->STATUS |= BUF_ERR;                       /* 写 1 清除 */
        s_isr_events |= USBPD_IO_ISR_EVT_BUF_ERROR;     /* 事件转任务上下文上报 */
    }
}

/* ═══════════════════════════════════════════════════
 *  初始化与工作模式
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   移植层初始化
 * @details 使能 GPIOB / AFIO / USBPD 时钟，CC 引脚配置为浮空输入
 *          并使能高门限输入，配置外设（输入滤波 + DMA）并清除
 *          全部挂起中断标志。
 * @retval  USBPD_IO_OK  成功
 */
int
usbpd_io_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = { 0 };

    /* CC 引脚 GPIO 配置：PB6 = CC1，PB7 = CC2 */
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOB, ENABLE);
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO, ENABLE);
    gpio_init_struct.GPIO_Pin = PIN_CC1 | PIN_CC2;
    gpio_init_struct.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_struct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio_init_struct);

    /* PD 引脚高门限输入，降低通信期间的 IO 功耗 */
    AFIO->CR |= USBPD_IN_HVT;

    /* USBPD 外设初始化：输入滤波 + DMA */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBPD, ENABLE);
    USBPD->CONFIG = PD_FILT_ED | PD_DMA_EN;

    /* 清全部挂起中断标志 */
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;

    return USBPD_IO_OK;
}

/**
 * @brief   进入 Sink（受电端）模式
 */
void
usbpd_io_sink_init(void)
{
    s_auto_ack_role = 0;

    /* 0.66V 比较器 + 下拉电阻（Sink） */
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

/**
 * @brief   进入 Source（供电端）模式
 */
void
usbpd_io_source_init(void)
{
    s_auto_ack_role = 1;

    /* 0.66V 比较器 + 330uA 上拉（Source 默认档） */
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PU_330;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PU_330;
}

/**
 * @brief   进入接收模式
 */
void
usbpd_io_enter_rx(void)
{
    /* 清全部中断标志（脉冲式清除） */
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;

    /* 接收完成 / 复位接收中断 + DMA */
    USBPD->CONFIG |= IE_RX_ACT | IE_RX_RESET | PD_DMA_EN;

    /* 接收 DMA 指向接收缓冲 */
    USBPD->DMA = (uint32_t)(uintptr_t)s_rx_buf;

    /* 关发送、配接收定时并启动 BMC */
    USBPD->CONTROL &= ~PD_TX_EN;
    USBPD->BMC_CLK_CNT = UPD_TMR_RX_96M;
    USBPD->CONTROL |= BMC_START;

    /* 任务上下文处理完报文后由此恢复中断 */
    NVIC_EnableIRQ(USBPD_IRQn);
}

/* ═══════════════════════════════════════════════════
 *  报文收发
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   发送一帧 BMC 编码报文
 * @param   wait  1 = 阻塞等待发送完成（带超时）
 * @param   buf   发送数据（NULL 且 len = 0 表示仅发复位序列）
 * @param   len   发送字节数
 * @param   sop   SOP 序列类型（USBPD_IO_SOP_xxx）
 * @retval  USBPD_IO_OK     发送完成（或已启动）
 * @retval  USBPD_IO_ERROR  等待发送完成超时
 */
int
usbpd_io_phy_send(uint8_t wait, const uint8_t *buf, uint8_t len, uint8_t sop)
{
    uint32_t timeout = TX_END_TIMEOUT_CNT;
    int ret = USBPD_IO_OK;

    ch32l103_phy_start_tx(buf, len, ch32l103_sop_encode(sop));

    if (wait)
    {
        /* 等待发送完成，带超时防止硬件异常卡死 */
        while (((USBPD->STATUS & IF_TX_END) == 0) && (--timeout != 0))
        {
        }

        if (timeout == 0)
        {
            ret = USBPD_IO_ERROR;
        }
        USBPD->STATUS |= IF_TX_END;                     /* 写 1 清除 */

        /* 关闭当前 CC 通道低电平输出 */
        if ((USBPD->CONFIG & CC_SEL) == CC_SEL)
        {
            USBPD->PORT_CC2 &= ~CC_LVE;
        }
        else
        {
            USBPD->PORT_CC1 &= ~CC_LVE;
        }

        /* 回到接收模式 */
        USBPD->CONFIG |= PD_ALL_CLR;
        USBPD->CONFIG &= ~PD_ALL_CLR;
        USBPD->CONTROL &= ~PD_TX_EN;
        USBPD->DMA = (uint32_t)(uintptr_t)s_rx_buf;
        USBPD->BMC_CLK_CNT = UPD_TMR_RX_96M;
        USBPD->CONTROL |= BMC_START;
    }

    return ret;
}

/**
 * @brief   等待本帧发送的 GoodCRC 应答（约 750us 窗口）
 * @retval  1 收到 GoodCRC；0 超时
 */
uint8_t
usbpd_io_wait_goodcrc(void)
{
    uint8_t cnt = 250;

    while (--cnt)
    {
        if ((USBPD->STATUS & IF_RX_ACT) != 0)
        {
            USBPD->STATUS |= IF_RX_ACT;                 /* 写 1 清除 */
            if ((USBPD->BMC_BYTE_CNT == 6) &&
                ((s_rx_buf[0] & 0x1F) == USBPD_IO_MSG_GOODCRC))
            {
                return 1;
            }
        }
        Delay_Us(3);
    }

    return 0;
}

/**
 * @brief   非阻塞读取已收到的报文
 * @param   buf   输出缓冲
 * @param   size  输出缓冲容量
 * @retval  >0 报文字节数；0 无新报文
 */
uint8_t
usbpd_io_read_packet(uint8_t *buf, uint8_t size)
{
    uint8_t len;

    if (s_msg_ready == 0)
    {
        return 0;
    }
    s_msg_ready = 0;

    len = (uint8_t)USBPD->BMC_BYTE_CNT;
    if (len > size)
    {
        len = size;
    }

    /* 多字节共享数据临界区拷贝（R5） */
    NVIC_DisableIRQ(USBPD_IRQn);
    memcpy(buf, s_rx_buf, len);
    NVIC_EnableIRQ(USBPD_IRQn);

    return len;
}

/**
 * @brief   读取并清除中断侧累积的事件标志
 * @retval  USBPD_IO_ISR_EVT_xxx 按位或
 */
uint8_t
usbpd_io_get_isr_events(void)
{
    uint8_t evt = s_isr_events;
    s_isr_events = 0;
    return evt;
}

/* ═══════════════════════════════════════════════════
 *  连接检测
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   检测 CC 线接入状态
 * @details 两线分别配置 0.22V 比较器（Ra/Rd 上下拉分压判定门限）
 *          并延时 2us 等比较器稳定后读取。仅 Sink 下拉使能时
 *          检测结果有效。
 * @retval  USBPD_CC_NONE / USBPD_CC_LINE_1 / USBPD_CC_LINE_2
 */
usbpd_cc_line_t
usbpd_io_detect_cc(void)
{
    usbpd_cc_line_t ret = USBPD_CC_NONE;
    uint8_t cc1_hit;
    uint8_t cc2_hit;

    /* CC1：0.22V 门限检测 */
    *ch32l103_cc_reg(0) = (*ch32l103_cc_reg(0) & ~CC_CE) | CC_CMP_22;
    Delay_Us(2);
    cc1_hit = ((*ch32l103_cc_reg(0) & PA_CC_AI) != 0) ? 1 : 0;

    /* CC2：0.22V 门限检测 */
    *ch32l103_cc_reg(1) = (*ch32l103_cc_reg(1) & ~CC_CE) | CC_CMP_22;
    Delay_Us(2);
    cc2_hit = ((*ch32l103_cc_reg(1) & PA_CC_AI) != 0) ? 1 : 0;

    /* 仅 Sink（下拉使能）时判定接入 */
    if ((USBPD->PORT_CC1 & CC_PD) != 0)
    {
        if (cc1_hit)
        {
            ret = USBPD_CC_LINE_1;
        }
        if (cc2_hit)
        {
            if (ret != USBPD_CC_NONE)
            {
                /* 部分 A-C 线双 CC 上拉，按 CC1 处理 */
                ret = USBPD_CC_LINE_1;
            }
            else
            {
                ret = USBPD_CC_LINE_2;
            }
        }
    }

    return ret;
}

/**
 * @brief   选择 PD 通信使用的 CC 通道
 * @param   cc  目标通道
 */
void
usbpd_io_select_cc(usbpd_cc_line_t cc)
{
    if (cc == USBPD_CC_LINE_1)
    {
        USBPD->CONFIG &= ~CC_SEL;
    }
    else if (cc == USBPD_CC_LINE_2)
    {
        USBPD->CONFIG |= CC_SEL;
    }
    else
    {
        /* USBPD_CC_NONE：保持当前通道不动 */
    }
}

/* ═══════════════════════════════════════════════════
 *  中断与时序
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   使能 USBPD 中断
 */
void
usbpd_io_irq_enable(void)
{
    NVIC_EnableIRQ(USBPD_IRQn);
}

/**
 * @brief   禁能 USBPD 中断
 */
void
usbpd_io_irq_disable(void)
{
    NVIC_DisableIRQ(USBPD_IRQn);
}

/**
 * @brief   微秒级延时
 * @param   us  延时微秒数
 */
void
usbpd_io_delay_us(uint32_t us)
{
    Delay_Us(us);
}

/**
 * @brief   毫秒级延时
 * @param   ms  延时毫秒数
 */
void
usbpd_io_delay_ms(uint32_t ms)
{
    Delay_Ms(ms);
}

/* ═══════════════════════════════════════════════════
 *  私有函数实现
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   取 CC 通道寄存器地址
 * @param   ccx  0 = CC1（PORT_CC1）；其他 = CC2（PORT_CC2）
 * @retval  对应 PORT_CCx 寄存器指针
 */
static volatile uint32_t *
ch32l103_cc_reg(uint8_t ccx)
{
    if (ccx == 0)
    {
        return &USBPD->PORT_CC1;
    }
    return &USBPD->PORT_CC2;
}

/**
 * @brief   SOP 类型映射为 TX_SEL 硬件编码
 * @param   sop  USBPD_IO_SOP_xxx
 * @retval  TX_SEL 寄存器值
 */
static uint8_t
ch32l103_sop_encode(uint8_t sop)
{
    switch (sop)
    {
    case USBPD_IO_SOP_DEBUG1:
        return UPD_SOP1;

    case USBPD_IO_SOP_DEBUG2:
        return UPD_SOP2;

    case USBPD_IO_SOP_HARD_RESET:
        return UPD_HARD_RESET;

    case USBPD_IO_SOP_CABLE_RST:
        return UPD_CABLE_RESET;

    case USBPD_IO_SOP_NORMAL:
    default:
        return UPD_SOP0;
    }
}

/**
 * @brief   启动一次 BMC 发送（不等待完成）
 * @param   buf     发送数据（NULL 表示仅发复位序列）
 * @param   len     发送字节数
 * @param   tx_sel  SOP 硬件编码（UPD_SOP0 等）
 */
static void
ch32l103_phy_start_tx(const uint8_t *buf, uint8_t len, uint8_t tx_sel)
{
    /* 当前通信通道打开低电平输出 */
    if ((USBPD->CONFIG & CC_SEL) == CC_SEL)
    {
        USBPD->PORT_CC2 |= CC_LVE;
    }
    else
    {
        USBPD->PORT_CC1 |= CC_LVE;
    }

    USBPD->BMC_CLK_CNT = UPD_TMR_TX_96M;
    USBPD->DMA = (uint32_t)(uintptr_t)buf;
    USBPD->TX_SEL = tx_sel;
    USBPD->BMC_TX_SZ = len;
    USBPD->CONTROL |= PD_TX_EN;
    USBPD->STATUS &= BMC_AUX_INVALID;                   /* 清 BMC 辅助状态（写 0 清辅助位） */
    USBPD->CONTROL |= BMC_START;
}
