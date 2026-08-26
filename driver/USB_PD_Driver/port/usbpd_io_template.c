/**
 * @file    usbpd_io_template.c
 * @brief   USB PD 移植层模板（移植到其他 MCU 时复制本文件实现）
 * @details 将 USB PD 协议栈移植到目标平台时：
 *          1. 复制本文件到工程并改名为 usbpd_io_<芯片型号>.c；
 *          2. 实现 usbpd_io.h 中的全部函数（本文件即函数桩清单）；
 *          3. 不需要改动 usbpd.c / usbpd.h / usbpd_conf.h。
 *
 *          实现要点：
 *          - usbpd_io_phy_send：硬件须自动附加 SOP 序列与 CRC；
 *            wait=1 时阻塞等待发送完成并做收尾（关发送、回接收），
 *            等待必须带超时（参考 5ms 量级）；
 *          - usbpd_io_wait_goodcrc：约 750us 窗口内查询对端 GoodCRC
 *            应答，时序上限由 PD 规范决定，不可随意放大；
 *          - usbpd_io_read_packet：接收完成中断置就绪标志，本函数
 *            在任务上下文取走数据，多字节拷贝须关中断保护；
 *          - 延时函数的实际延时不得小于请求值。
 * @note    本文件不参与编译（函数体为占位说明）；CH32L103 平台可
 *          直接使用 examples/ch32l103/ 的现成移植，无需本模板。
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-08-18
 */

#include <stdint.h>
#include "usbpd_io.h"

/* TODO: 包含目标平台的头文件（外设库 / 厂商 SDK） */

/* TODO: 定义接收 DMA 缓冲（若硬件 DMA 搬运需 4 字节对齐，
 *       使用 USBPD_ALIGNED_4 属性）：
 *   static USBPD_ALIGNED_4 uint8_t s_rx_buf[USBPD_MSG_BUF_SIZE];
 */

/* TODO: 定义中断共享的 volatile 标志：
 *   static volatile uint8_t s_msg_ready;
 *   static volatile uint8_t s_isr_events;
 */

int
usbpd_io_init(void)
{
    /* TODO: 外设时钟使能、CC 引脚 GPIO 配置、PD 外设寄存器初始化、
     *       清除全部挂起中断标志 */
    return USBPD_IO_OK;
}

void
usbpd_io_sink_init(void)
{
    /* TODO: CC 配置为 Sink（Rp 上拉检测 + Rd 下拉），记录 GoodCRC
     *       自动应答角色为 Sink */
}

void
usbpd_io_source_init(void)
{
    /* TODO: CC 配置为 Source（Rp 上拉），记录 GoodCRC 自动应答
     *       角色为 Source */
}

void
usbpd_io_enter_rx(void)
{
    /* TODO: 清中断标志、重挂接收 DMA、启动接收、使能 PD 中断 */
}

int
usbpd_io_phy_send(uint8_t wait, const uint8_t *buf, uint8_t len, uint8_t sop)
{
    /* TODO: 选择当前 CC 通道、装载 DMA 地址与 SOP 编码、启动发送；
     *       wait=1 时带超时等待发送完成并做收尾后返回 USBPD_IO_OK，
     *       超时返回 USBPD_IO_ERROR */
    return USBPD_IO_ERROR;
}

uint8_t
usbpd_io_wait_goodcrc(void)
{
    /* TODO: 约 750us 窗口内轮询接收完成标志，判断对端 GoodCRC；
     *       收到返回 1，超时返回 0 */
    return 0;
}

uint8_t
usbpd_io_read_packet(uint8_t *buf, uint8_t size)
{
    /* TODO: 无新报文返回 0；有则关中断拷贝到 buf（不超过 size）
     *       后返回实际字节数 */
    return 0;
}

uint8_t
usbpd_io_get_isr_events(void)
{
    /* TODO: 返回并清除中断侧累积事件（USBPD_IO_ISR_EVT_xxx 按位或） */
    return 0;
}

usbpd_cc_line_t
usbpd_io_detect_cc(void)
{
    /* TODO: 两根 CC 线分别做 0.22V 门限比较，仅 Sink 下拉使能时
     *       判定有效；返回 USBPD_CC_NONE / LINE_1 / LINE_2 */
    return USBPD_CC_NONE;
}

void
usbpd_io_select_cc(usbpd_cc_line_t cc)
{
    /* TODO: 按接入检测结果选择通信使用的 CC 通道 */
}

void
usbpd_io_irq_enable(void)
{
    /* TODO: 平台中断控制器层面使能 PD 中断 */
}

void
usbpd_io_irq_disable(void)
{
    /* TODO: 平台中断控制器层面禁能 PD 中断 */
}

void
usbpd_io_delay_us(uint32_t us)
{
    /* TODO: 微秒级阻塞延时（不小于请求值） */
}

void
usbpd_io_delay_ms(uint32_t ms)
{
    /* TODO: 毫秒级阻塞延时（不小于请求值） */
}
