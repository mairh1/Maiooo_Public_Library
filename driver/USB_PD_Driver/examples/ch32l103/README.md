# CH32L103 移植示例

`usbpd_io_ch32l103.c` 是基于 CH32L103 片上 USBPD 外设的完整移植实现，覆盖 `usbpd_io.h` 全部 14 个契约函数。

## 文件说明

| 文件 | 内容 |
|---|---|
| `usbpd_io_ch32l103.c` | 移植层实现：时钟 / GPIO / 外设初始化、BMC 收发（DMA + SOP 编码）、CC 比较器检测、USBPD 中断服务（GoodCRC 自动应答） |

## MounRiver 接入步骤

1. 将驱动目录下 `usbpd.c` 与本目录 `usbpd_io_ch32l103.c` 加入工程，include 路径加入驱动目录（`usbpd.h / usbpd_conf.h / usbpd_io.h` 所在目录）。
2. 工程需包含 WCH 官方标准外设库（`ch32l103.h`、`debug.h`，提供 GPIO/RCC/AFIO/NVIC/Delay 函数）。
3. 中断向量无需用户接线：移植文件已定义 `USBPD_IRQHandler`（带快速中断属性，与启动文件向量表绑定，不可改名）。
4. 调用约定：
   - 上电调用一次 `USBPD_Init()`；
   - SysTick 1ms 中断中调用 `USBPD_TickIsr()`；
   - 主循环持续调用 `USBPD_Task()`；
   - 可选注册 `USBPD_SetEventCallback()` 接收接入 / 协商完成等事件。

## 注意事项

- BMC 定时常量按 **96MHz 系统主频**计算（`UPD_TMR_TX_96M = 159`、`UPD_TMR_RX_96M = 239`），主频不同必须同步修改。
- CC 引脚固定为 PB6（CC1）/ PB7（CC2），输入使能 AFIO 高门限（2.2V）以降低通信功耗。
- 中断内的 `Delay_Us(30)` 是 GoodCRC 195us 应答窗口的硬性时序要求，不可移除。
