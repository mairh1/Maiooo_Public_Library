# usb_pd_driver — 可移植 USB PD Sink 协议栈

分层可移植的 USB PD（Power Delivery）受电端协议栈：协议核心为纯 C99、零寄存器访问、无动态内存，全部硬件操作收敛到 `usbpd_io.h` 移植契约。内置 CH32L103 现成移植示例。

## 分层架构

```
应用层（用户代码）
    │  usbpd.h 公共 API + 统一结果码 usbpd_result_t
┌────────────────────────────────────────────┐
│ 协议核心层                 纯 C99，零平台代码 │
│   usbpd.c        协商状态机 / 报文收发 / PDO   │
│   usbpd_conf.h   编译期配置裁剪               │
└────────────────────────────────────────────┘
    │  usbpd_io.h 移植契约（14 个函数 + 返回码 + 时序约束）
┌────────────────────────────────────────────┐
│ 移植层（唯一碰硬件的一层）                     │
│   port/usbpd_io_template.c     移植模板       │
│   examples/ch32l103/           CH32L103 示例  │
└────────────────────────────────────────────┘
    │
硬件（USBPD PHY / CC 引脚 / 中断 / DMA / 延时）
```

依赖单向：应用层 → 核心 → io 契约 → 移植层。核心 `.c` 的 include 白名单仅 `string.h` + 自身头文件；`_io.h` 不反向包含核心实现。

## 目录结构

| 文件 | 职责 |
|---|---|
| `usbpd.h` | 公共 API：类型、结果码、函数声明 |
| `usbpd.c` | 协议核心：状态机、报文收发、PDO 解析 |
| `usbpd_conf.h` | 全部编译期配置宏（带默认值，可 `-D` 覆盖） |
| `usbpd_io.h` | 移植契约：io 函数声明 + 每函数时序/上下文要求 |
| `port/usbpd_io_template.c` | 移植模板（不参与编译），复制填空即可 |
| `examples/ch32l103/` | CH32L103 完整移植示例（含 MounRiver 接入说明） |

## 快速上手（移植三步走）

1. **移植**：复制 `port/usbpd_io_template.c` 改名为 `usbpd_io_<芯片型号>.c`，按注释实现全部 14 个 io 函数（或直接使用 `examples/ch32l103/` 现成移植）。核心文件零改动——若发现必须改核心才能移植，请先报告这是驱动的缺陷。
2. **配置**：按需覆盖 `usbpd_conf.h` 中的宏（编译器 `-D` 或直接改文件均可）。
3. **接入**：
   - 上电调用一次 `USBPD_Init()`；
   - SysTick / 定时器 1ms 中断中调用 `USBPD_TickIsr()`（仅递增一个 volatile 计数，开销确定）；
   - 主循环持续调用 `USBPD_Task()`；
   - （可选）调用 `USBPD_SetEventCallback()` 注册事件回调，回调在主循环上下文执行，内禁止阻塞。

## 配置参考（usbpd_conf.h）

| 宏 | 默认值 | 说明 |
|---|---|---|
| `USBPD_PD30_ENABLE` | `0` | PD 版本：0 = PD2.0；1 = PD3.0（影响消息头 SpecRev 位） |
| `USBPD_DET_DEBOUNCE` | `5` | CC 接入消抖连续确认次数 |
| `USBPD_DEFAULT_PDO_IDX` | `1` | 收到 SRC_CAP 后默认申请的 PDO 序号（1 起，通常为 5V 档） |
| `USBPD_TX_RETRY_CNT` | `3` | 报文发送最大重试次数（GoodCRC 等待失败后重发） |
| `USBPD_MSG_BUF_SIZE` | `34` | 单帧最大字节数（消息头 2 + 7×4 数据对象 + CRC 4） |
| `USBPD_LOG_ENABLE` | `0` | 日志开关；开启时须由用户平台层实现 `usbpd_log_output()` |
| `USBPD_ALIGNED_4` | GNUC 下生效 | DMA 缓冲 4 字节对齐属性，非 GNUC 自动退化为空 |

## API 参考

| 函数 | 说明 |
|---|---|
| `usbpd_result_t USBPD_Init(void)` | 初始化协议栈（移植层 init → 变量复位 → PHY 复位进接收） |
| `void USBPD_TickIsr(void)` | 1ms 节拍喂入，仅 ISR 中调用 |
| `void USBPD_Task(void)` | 主任务：CC 消抖、状态机推进、报文解析应答、事件上报 |
| `usbpd_state_t USBPD_GetState(void)` | 读当前协议状态机状态 |
| `uint8_t USBPD_IsConnected(void)` | CC 物理连接查询：1 已连接 / 0 未连接 |
| `void USBPD_SetEventCallback(usbpd_event_cb_t cb)` | 注册事件回调（NULL 取消） |

事件类型（`usbpd_event_t`）：`ATTACHED`（info->cc_line）、`SRCCAP_READY`（pdo_list/pdo_count）、`PS_READY`（pdo_index/voltage_mv/current_ma）、`TX_FAILED`、`PHY_RESET`、`BUF_ERROR`。

### 结果码语义（usbpd_result_t）

| 结果码 | 值 | 语义 |
|---|---|---|
| `USBPD_OK` | 0 | 成功 |
| `USBPD_ERR_IO` | 1 | 移植层初始化失败 |
| `USBPD_ERR_PARAM` | 2 | 参数非法（预留） |
| `USBPD_ERR_NOT_READY` | 3 | 未初始化 / 未就绪（预留） |

### 物理单位约定

电压一律 **mV**（PDO 解析步进 50mV），电流一律 **mA**（PDO 解析步进 10mA）。PDO 序号从 1 起。

## 架构决策说明

- **单实例模型**：协议状态为核心层模块级静态数据，单板上仅支持一路 USB PD 端口（多数 MCU 的 USBPD 外设本身也是单实例）。线程安全模型：不同上下文无需加锁的使用方式为——`USBPD_TickIsr` 仅在 1ms 中断中调用，其余全部公共 API 仅在主循环单线程上下文调用；io 层内部已用中断禁能保护多字节共享拷贝。多实例需求出现前不引入句柄化改造。
- **移植契约为 14 个函数**：通用芯片驱动的移植契约建议收敛到 ≤7 个函数；本组件是完整协议栈，物理层（BMC 收发 / GoodCRC 应答 / CC 比较）与协议层耦合在硬件时序上（如中断内 195us 自动应答），强行合并会把协议时序知识泄漏进移植接口、反而提高移植出错率，故保留显式 14 函数契约，每个函数的时序与上下文要求见 `usbpd_io.h` 注释。
- **日志经用户桥接**：开启 `USBPD_LOG_ENABLE` 后须实现 `extern void usbpd_log_output(const char *fmt, ...)`，协议栈不直接依赖 printf / stdio。

## 已知坑（来自数据手册与实践）

- **BMC 定时常量绑定主频**：CH32L103 移植中 `UPD_TMR_TX_96M / UPD_TMR_RX_96M` 按 96MHz 主频计算，换主频必须同步调整。
- **GoodCRC 应答窗口是硬约束**：PD 规范要求约 195us 内发出 GoodCRC，移植层中断内的 `Delay_Us(30)` 与约 750us 的 `usbpd_io_wait_goodcrc` 窗口不可移除或放大。
- **发送缓冲须 4 字节对齐**：发送缓冲地址直写 DMA 寄存器，用 `USBPD_ALIGNED_4` 声明。
- **PPS PDO 被剔除**：解析 SRC_CAP 时遇到 PPS（Bit31-30 = 11）即截止，只保留固定 PDO；请求 PPS 档位不受支持。
- **拔出检测不在协议栈内**：Sink 侧拔出依赖 Vbus 掉电检测，需应用层自行监测并复位（或等待硬复位流程恢复）。
