/**
 * @file    usbpd_io.h
 * @brief   USB PD 协议栈移植层 I/O 接口
 * @details 协议核心（usbpd.c）访问硬件的唯一通道：只调用本文件声明的
 *          函数，不接触任何寄存器与厂商头文件。将本模块移植到其他
 *          MCU 时，仅需实现这些函数（可参考 port/usbpd_io_template.c
 *          模板，或直接使用现成的 usbpd_io_ch32l103.c）。
 *
 *          实现约束：
 *          - 所有函数须可在线程上下文调用；除 usbpd_io_phy_send 的
 *            wait=1 分支与 usbpd_io_wait_goodcrc 为协议时序要求的
 *            微秒级阻塞外，其余函数不得阻塞；
 *          - 中断服务中收包完成后置就绪标志，由 usbpd_io_read_packet
 *            在线程上下文取走，避免核心层直接触碰中断共享数据；
 *          - usbpd_io_delay_us / delay_ms 的实际延时不得小于请求值
 *            （PD 应答时序下限约束）。
 * @note    依赖：usbpd.h。不依赖任何 app / bsp 层头文件。
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-08-18
 */

#ifndef USBPD_IO_H
#define USBPD_IO_H

#include "usbpd.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════
 *  移植层返回码
 * ═══════════════════════════════════════════════════ */

#define USBPD_IO_OK             0       /**< 操作成功 */
#define USBPD_IO_ERROR         (-1)    /**< 操作失败（通信异常 / 超时） */

/* ═══════════════════════════════════════════════════
 *  跨层共用协议常量
 * ═══════════════════════════════════════════════════ */

/** @brief 报文起始序列（SOP）类型，由移植层映射为硬件编码 */
#define USBPD_IO_SOP_NORMAL     0x00    /**< SOP'：普通报文（含 GoodCRC 应答） */
#define USBPD_IO_SOP_DEBUG1     0x01    /**< SOP''：调试报文 */
#define USBPD_IO_SOP_DEBUG2     0x02    /**< SOP'''：调试报文 */
#define USBPD_IO_SOP_HARD_RESET 0x03    /**< Hard Reset 序列 */
#define USBPD_IO_SOP_CABLE_RST  0x04    /**< Cable Reset 序列 */

/** @brief GoodCRC 控制消息类型（消息头 Bit4-0），中断内应答判断需要 */
#define USBPD_IO_MSG_GOODCRC    0x01

/* ═══════════════════════════════════════════════════
 *  初始化与工作模式
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   移植层初始化
 * @details 完成外设时钟使能、CC 引脚 GPIO 配置、USBPD 外设寄存器
 *          初始化、清除全部挂起中断标志。由 USBPD_Init 调用一次。
 * @retval  USBPD_IO_OK     成功
 * @retval  USBPD_IO_ERROR  失败（时钟 / 引脚配置异常）
 */
int usbpd_io_init(void);

/**
 * @brief   进入 Sink（受电端）模式
 * @details CC1/CC2 配置 0.66V 比较器并使能下拉电阻，同时记录
 *          GoodCRC 自动应答角色为 Sink。
 */
void usbpd_io_sink_init(void);

/**
 * @brief   进入 Source（供电端）模式
 * @details CC1/CC2 配置 0.66V 比较器与 330uA 上拉（默认档），
 *          同时记录 GoodCRC 自动应答角色为 Source。
 */
void usbpd_io_source_init(void);

/**
 * @brief   进入接收模式
 * @details 清全部中断标志、重挂接收 DMA、启动 BMC 接收并使能
 *          USBPD 中断。报文应答处理完毕后由协议核心调用本函数
 *          恢复接收。
 */
void usbpd_io_enter_rx(void);

/* ═══════════════════════════════════════════════════
 *  报文收发
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   发送一帧 BMC 编码报文
 * @param   wait  0 = 启动发送立即返回；1 = 阻塞等待发送完成
 * @param   buf   发送数据缓冲（不含 SOP 与 CRC，由硬件附加），
 *                传 NULL 且 len=0 表示仅发送复位序列
 * @param   len   发送字节数（0-30）
 * @param   sop   SOP 序列类型，取 USBPD_IO_SOP_xxx
 * @retval  USBPD_IO_OK     发送完成（或已启动）
 * @retval  USBPD_IO_ERROR  wait=1 时等待发送完成超时
 * @note    wait=1 的等待带超时保护，硬件异常不会永久阻塞
 */
int usbpd_io_phy_send(uint8_t wait, const uint8_t *buf, uint8_t len, uint8_t sop);

/**
 * @brief   等待本帧发送的 GoodCRC 应答
 * @details 在约 750us 窗口内轮询接收完成标志，判断对端是否回
 *          GoodCRC。阻塞时长由 PD 规范应答窗口决定。
 * @retval  1 收到 GoodCRC
 * @retval  0 超时未收到
 */
uint8_t usbpd_io_wait_goodcrc(void);

/**
 * @brief   非阻塞读取已收到的报文
 * @param   buf   输出缓冲，由调用方提供
 * @param   size  输出缓冲容量
 * @retval  >0 报文实际字节数（含 2 字节消息头）
 * @retval  0  无新报文
 * @note    中断收包完成后置就绪标志，本函数取走并清除；
 *          多字节拷贝期间建议关 USBPD 中断以保证一致性
 */
uint8_t usbpd_io_read_packet(uint8_t *buf, uint8_t size);

/* ═══════════════════════════════════════════════════
 *  连接检测
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   检测 CC 线接入状态
 * @details 将两根 CC 线配置 0.22V 比较器并读取比较结果，仅在
 *          Sink 下拉使能时有意义。
 * @retval  USBPD_CC_NONE / USBPD_CC_LINE_1 / USBPD_CC_LINE_2
 */
usbpd_cc_line_t usbpd_io_detect_cc(void);

/**
 * @brief   选择 PD 通信使用的 CC 通道
 * @param   cc  目标通道（接入检测结果）
 */
void usbpd_io_select_cc(usbpd_cc_line_t cc);

/* ═══════════════════════════════════════════════════
 *  中断侧事件
 * ═══════════════════════════════════════════════════ */

/** @brief 中断侧累积事件位（usbpd_io_get_isr_events 返回值） */
#define USBPD_IO_ISR_EVT_PHY_RESET      0x01    /**< 收到硬复位序列（中断内已完成 Sink 重新初始化） */
#define USBPD_IO_ISR_EVT_BUF_ERROR      0x02    /**< 收发缓冲 / DMA 错误 */

/**
 * @brief   读取并清除中断侧累积的事件标志
 * @details 中断服务中只置位不处理（禁止在中断内打印/耗时），
 *          协议核心在任务上下文中调用本函数取走事件并上报。
 * @retval  USBPD_IO_ISR_EVT_xxx 的按位或，0 表示无 pending 事件
 */
uint8_t usbpd_io_get_isr_events(void);

/* ═══════════════════════════════════════════════════
 *  中断与时序
 * ═══════════════════════════════════════════════════ */

/**
 * @brief   使能 USBPD 中断（NVIC / 平台中断控制器层面）
 */
void usbpd_io_irq_enable(void);

/**
 * @brief   禁能 USBPD 中断（发送事务期间互斥使用）
 */
void usbpd_io_irq_disable(void);

/**
 * @brief   微秒级延时（阻塞）
 * @param   us  延时微秒数
 */
void usbpd_io_delay_us(uint32_t us);

/**
 * @brief   毫秒级延时（阻塞）
 * @param   ms  延时毫秒数
 */
void usbpd_io_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* USBPD_IO_H */
