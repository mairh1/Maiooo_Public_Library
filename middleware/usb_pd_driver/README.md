# USB PD 中间层 v3

纯 C99、多实例、非阻塞的 USB Type-C / **USB PD 2.0** 电源协议核心。
支持 Sink、Source、DRP、固定 PDO（5V 至 20V，最高 3A）及双向 PR_Swap。
v3 是本库接口版本，不表示 PD 3.0。

本轮交付核心和移植模板，**未适配具体硬件、未执行报文交互测试或硬件联调，未通过
USB-IF 认证**。编译通过只证明构建兼容性，不证明协议互操作性。

## 分层与文件

```text
应用：能力配置 / 申请固定 PDO / 请求交换 / 事件处理
                         |
                     usbpd.h
                         |
核心：usbpd.c + usbpd_conf.h + usbpd_regs.h
      Type-C 状态机 / PD 策略 / 消息 ID / 超时恢复
                         |
                   usbpd_io.h
                         |
移植：异步传输后端 / CC / VBUS / 电源开关 / VCONN
                         |
             MCU 外设或外置 TCPC + 电源硬件
```

| 文件 | 职责 |
|---|---|
| `usbpd.h` | 公共类型、实例存储和 API |
| `usbpd.c` | 不含平台符号的协议核心 |
| `usbpd_conf.h` | 裁剪、任务预算、带范围校验的时序 |
| `usbpd_regs.h` | PD 消息、电气阈值、对象编码常量 |
| `usbpd_io.h` | 固定原型的移植契约，不包含公共 API 头 |
| `port/usbpd_io_template.c` | 可编译的填空模板，未实现操作一律返回失败 |
| `examples/ch32l103/` | **旧版接口示例，不兼容 v3，不加入新版构建** |

核心没有可变静态状态、动态分配、C 位域、内联汇编和硬件等待。实例由调用者分配；
内存中的缓冲无需 DMA 对齐，后端复制到自己的 DMA 缓冲。

## 能力边界与策略

- 只在 USB Type-C 普通端口上处理 **SOP**。不支持 Type-A/B PD、附件模式、线缆
  SOP'/SOP''、5A 线缆识别、PPS、EPR、AVS、FR_Swap、DR_Swap、VCONN_Swap、
  Alt Mode、USB 数据功能和 BIST 测试模式。
- 初始化提供本机固定能力表：最多 7 项、第一项 5V、按电压递增。电压须为 50mV
  整数倍，电流须为 10mA 整数倍，非法配置不静默钳位。Source 的 5V 能力至少 500mA。
  应用负责按真实电源能力和 PD Power Rules 提供完整档位；本机能力表首版不动态修改。
- Sink 首次请求对端第一项 5V，取本机 5V 电流上限与对端能力的较小值。之后应用可用
  **原始 PDO 序号**重新申请。非固定 PDO 保留在查询结果中，但不能申请。
- 本机 Sink 只能申请其初始化能力表中列出的电压。对端更新能力时，若原合同仍匹配，
  重新查找对象位置并维持原目标；否则回到 5V。此前应用事务以 CANCELED 终止。
- Source 验证原始 RDO 的对象位置、保留位、GiveBack、Operating/Maximum 电流。
  接受后留出 tSrcTransition，再设置电源；**实际就绪且电压合格**才发送 PS_RDY。
- 初次和无 PD 合同期间按 Rp 限制取电；默认 USB 电流按 500mA 上限处理。
  5V/0mA 单对象能力通告表示不可取电，核心关闭 Sink 通路并等待新能力。
- DRP 不偏好角色；默认每半周期为 40±5ms，每轮独立随机化以避免双 DRP 锁步。
  未连接时交替 Rp/Rd，出现候选后暂停切换，完成接入消抖。
- PR_Swap 必须具备本机两组能力及显式合同。对端发起时两个方向的许可默认均为
  false；不限制应用主动发起。PR_Swap 只改变电源角色，**保持数据和 VCONN 角色**。
- 交换中的旧 Source 必须关闭并放电到 vSafe0V，再改为 Rd 并发 PS_RDY；旧 Sink
  关闭取电，等待对端 PS_RDY 与实际 0V，再改为 Rp、建立 5V 并发送 PS_RDY。
  交换结束后重新协商。SWAP_DONE 成功不表示新显式合同已经建立。
- Reject/Wait 保留尚未被 Accept 取代的旧合同，报告失败；Wait 后至少等待 100ms
  才接受新的应用申请。应用收到 WAIT 后自行决定重试，不无限自动重试应用事务。
- 正常协议错误尝试 Soft Reset，失败升级 Hard Reset；电源转换异常用 Hard Reset，
  已接受的 Type-C PR_Swap 失败进入 ErrorRecovery。无响应硬复位最多初次加两次重试。
  没见过 PD 报文的 Sink 最后保留 Type-C 5V 供电，不把普通充电口当作 PD 合同。
- 不用一个通用响应处理所有未知消息。DR_Swap/VCONN_Swap 返回 Reject；支持范围外
  的结构化 VDM 请求由 UFP 返回 NAK，忽略 Attention、非结构化消息和 VDM 响应。
  不伪造身份、状态扩展数据或模式 ACK。

## 接入方式

1. 在工程中加入 `usbpd.c`，include 路径包含本目录；复制移植模板并实现底层。
2. 配置编译开关和每个端口的能力表，零初始化 `usbpd_dev_t`。
3. 初始化后注册回调；每个实例以 **不超过 1ms** 的间隔调用 Task。
4. 回调中只复制事件或记录待处理标志；Task 返回后再调用申请、查询等 API。

默认双角色构建的配置示意（下面的 Board 对象/时钟由工程提供）：

```c
static usbpd_dev_t s_pd;
static const usbpd_config_t s_config =
{
    .mode = USBPD_MODE_DRP,
    .sink_count = 4,
    .sink_pdos = {{5000, 3000}, {9000, 3000}, {15000, 3000}, {20000, 3000}},
    .source_count = 4,
    .source_pdos = {{5000, 3000}, {9000, 3000}, {15000, 3000}, {20000, 3000}}
};

/* 只有硬件确实支持这些电压/电流时才能使用此配置。 */
result = USBPD_Init(&s_pd, &board_port, &s_config, Board_Millis());
/* 检查 result，再注册回调。 */
result = USBPD_SetEventCallback(&s_pd, App_PdEvent, &app_context);

/* 主循环或 RTOS 单一所有者任务。 */
result = USBPD_Task(&s_pd, Board_Millis());
/* 例如在读取能力后，申请原始序号为 2 的固定档位 1A。 */
result = USBPD_RequestFixed(&s_pd, 2, 1000);
```

不再提供 `USBPD_TickIsr()`。Task 参数、底层事件时间戳必须使用同一单调 `uint32_t`
毫秒时基，允许自然回绕，但两次调用之间不能跨越 2^31ms。接收/发送响应计时从线上
事件完成时刻开始，不能把晚轮询时刻用作新起点。禁止将毫秒 Task 当作 GoodCRC 引擎。

### 线程与生命周期

不同实例可独立并发；同一实例所有 API（包括查询）由调用者串行访问。核心不依赖
RTOS，不实现锁。`task_active` 只防同步回调重入，**不是并发锁**。Init/Deinit 不得与
任何运行期调用并发。底层 ISR 与线程共享数据的原子性、临界区、FIFO 溢出检测均由后端负责。

回调只在 Task 上下文执行，`info` 指针只在回调期间有效；事件携带合同副本，没有内部
能力数组借用。回调不得阻塞或调用同实例 API。Deinit 主动取消整个实例且不调用回调。
发生硬件或不可恢复 I/O 故障后，实例锁定在 FAULT，需要检查硬件、Deinit 后重新 Init。

## 公共 API

所有 API 返回 `usbpd_result_t`。异步 API 的 OK 只表示排队，不能立即按目标电压取电。

| API | 功能与完成条件 |
|---|---|
| `USBPD_Init(dev, io_ctx, config, now_ms)` | 复制配置、初始化后端，首次 Task 开始探测 |
| `USBPD_Deinit(dev)` | 安全关闭并释放资源；失败保留故障状态 |
| `USBPD_Task(dev, now_ms)` | 有界事件处理和非阻塞状态推进 |
| `USBPD_GetStatus(dev, status)` | 状态、角色、CC、事务忙及已生效合同快照 |
| `USBPD_SetEventCallback(dev, cb, ctx)` | 注册回调；NULL 取消 |
| `USBPD_GetSourceCapabilities(dev, pdos, capacity, count)` | 复制完整对端 PDO 表，保留原始序号 |
| `USBPD_RequestFixed(dev, index, current_ma)` | Sink 排队申请，REQUEST_DONE 给出最终结果 |
| `USBPD_RequestPowerRoleSwap(dev)` | 排队 PR_Swap，SWAP_DONE 给出最终结果 |
| `USBPD_SetSwapPolicy(dev, source_to_sink, sink_to_source)` | 分方向控制对端请求许可 |

电压单位 mV，电流单位 mA，时间单位 ms，PDO 序号从 **1** 开始。传入电流必须是
10mA 整数倍；不会静默向上取整。查询能力数组不足时返回 ERR_PARAM，并通过 count
给出所需数量，不进行部分复制。没有能力时 count=0、返回 NOT_READY。

| 结果 | 语义 |
|---|---|
| `USBPD_OK` | 调用成功或异步请求已排队 |
| `USBPD_ERR_IO` | 后端通信/控制失败 |
| `USBPD_ERR_PARAM` | 空指针、非法范围/步进、缓冲不足或错误时间参数 |
| `USBPD_ERR_NOT_READY` | 未初始化、未连接、没有能力或未达到可申请状态 |
| `USBPD_ERR_NOT_SUPPORTED` | PDO 类型/电压或运行配置不支持 |
| `USBPD_ERR_BUSY` | 回调重入、发送忙或已有应用事务 |
| `USBPD_ERR_TIMEOUT` | 协议或电源等待超时 |
| `USBPD_ERR_REJECTED` / `USBPD_ERR_WAIT` | 对端拒绝 / 暂缓；WAIT 要求稍后重试 |
| `USBPD_ERR_PROTOCOL` | 帧、事件顺序或协议状态不符 |
| `USBPD_ERR_CANCELED` | 能力更新、复位或断开终止旧事务 |
| `USBPD_ERR_FAULT` | 硬件保护故障或实例已锁定 |

### 事件

ATTACHED/DETACHED 表示 Type-C 连接；SOURCE_CAPS 表示可查询最新能力；CONTRACT 表示
新合同生效；CONTRACT_LOST 使旧电源承诺失效；REQUEST_DONE/SWAP_DONE 是应用事务
终止通知。SOFT_RESET/HARD_RESET/ERROR 用于恢复与错误报告。

已接受的调压过程和角色交换会使旧合同失效。应用须依据 CONTRACT，不能仅依据
ATTACHED、Source_Capabilities、Accept 或成功提交 API 判断高压已经生效。

## 移植契约

| 分组 | 函数 | 必要性 |
|---|---|---|
| 生命周期 | init / shutdown / deinit | 建立后端、立即安全关闭、释放资源 |
| 快照 | get_status | CC、实测 VBUS、电源就绪及保护反馈 |
| CC/通信角色 | set_cc / set_link | Type-C 探测、方向、GoodCRC 电源/数据角色 |
| 传输 | reset / transmit / hard_reset / get_event | 可取消的异步事务及有序事件 |
| Source | set_source / set_discharge / set_vconn | 输出、断电放电、线缆基础供电 |
| Sink | set_sink | 受电路径及取电限流 |
| DRP | get_random | 对探测半周期施加独立抖动，避免对端长期锁步 |

默认构建 15 个接口；Source、Sink、DRP 分组随对应宏裁剪。PR_Swap 复用已有电源接口。
所有接口的返回码为 `DRV_IO_OK=0` / `DRV_IO_ERROR=-1`，普通状态通过输出参数表达。
接口均在普通线程调用、立即返回，不允许轮询等待；异步电源完成通过 status 反馈。
移植模板默认全部失败，填完前不会假装能正常协商。

### 传输后端必须保证

- 为 RX 做 CRC 校验、SOP 过滤和及时 GoodCRC；GoodCRC 版本为 PD 2.0，并使用 set_link
  的电源/数据角色。重复报文也须 GoodCRC，重复业务交给核心丢弃。
- transmit 返回前复制消息；后端按核心给出的 3 次重试执行 tReceive/tRetry，核验
  GoodCRC MessageID。成功只上报一次 TX_DONE；重试耗尽报告 TX_FAILED。
- 发生收发竞争时中止尚未完成的本机发送，先报告 TX_DISCARD，再报告对应 RX。
  正常响应必须在对应 TX_DONE 之后进入事件队列；不得暴露 GoodCRC 为普通完成响应。
- get_event 非阻塞且一次取一个事件；只返回时间戳不晚于传入 until_ms 的事件，
  更晚事件保留到下一轮 Task。NONE 也是读取成功。FIFO 溢出/DMA 错误报告
  ERROR。Hard Reset 优先并使旧 TX/RX 事件失效。reset 返回后不能再出现旧事务完成。
- RX 的 at_ms 取本机 GoodCRC 发送完成的时刻；TX_DONE 取收到匹配 GoodCRC 的时刻；
  复位事件取信号完成时刻。事件必须按真实顺序处理，不能只用一个覆盖式“最后收到包”。
- 微秒时序由硬件控制器或后端实时传输引擎保障。若 MCU 只有原始 PHY，移植工作还
  必须补齐该引擎；**旧 CH32L103 ISR/收发函数不能直接套入这个接口**。协议策略、
  电源状态机及应用回调均不进入 ISR。

### 电源后端必须保证

- source_ready/sink_ready 指向**最近命令**，收到新命令就使旧 ready 失效；off 表示
  通路真实断开。get_status 不能用“已提交设置”冒充“已完成”。
- set_source(false)/set_sink(false) 立即停止输出驱动/负载；物理稳定由反馈确认。
  Source 与 Sink 硬件互锁，过压、过流、过温、防反灌不能依靠主循环轮询实现。
- 电压切换时 Sink 暂停取电。平台必须具备维持控制器运行的独立/保持供电方案。
- 放电只针对本机刚关闭的 Source；原 Sink 不对对端电源放电。shutdown 必须保留
  自主保护及必要放电能力，deinit 不得重新接通电源。
- VCONN 使用非通信 CC，检测到 Ra 时提供；PR_Swap 保持原 VCONN 角色，Hard Reset
  重新按当前角色恢复默认行为。不进行线缆消息交互或 5A 能力提升。
- CC 电流档位由后端在 BMC Idle 采样或按规范滤波，不能把 PD 波形误判为 Rp 变化。
- 所有需要提供 Source 的硬件都必须实现 VCONN、VBUS 检测和放电接口，不能用空函数
  假装成功。Sink-only 构建不要求实现这些 Source 接口。

## 编译期配置

优先级：**编译参数 `-D` > 工程配置 > 库默认值**。可用
`-DUSBPD_USER_CONF_HEADER=\"board_usbpd_conf.h\"` 引入工程头（工程头自身也用
`#ifndef` 保留命令行优先级），或把完整私有 `usbpd_conf.h` 放在 include 搜索路径前。
核心及公共头通过 `<usbpd_conf.h>` 引入，避免引号搜索优先命中库目录。

| 宏 | 默认值 | 作用 |
|---|---|---|
| `USBPD_SINK_ENABLE` | 1 | Sink API、能力字段和底层受电接口 |
| `USBPD_SOURCE_ENABLE` | 1 | Source 核心、能力字段和底层输出/放电/VCONN 接口 |
| `USBPD_DRP_ENABLE` | 两角色都启用时为 1 | DRP 模式及随机值接口 |
| `USBPD_PR_SWAP_ENABLE` | 两角色都启用时为 1 | PR_Swap API 和状态机 |
| `USBPD_EVENT_BUDGET` | 4 | 单次 Task 的底层事件处理预算 |
| `USBPD_TX_RETRIES` | 3 | PD 2.0 重试数，不允许偏离规范 |
| `USBPD_HARD_RESET_LIMIT` | 2 | 初次 Hard Reset 后的重试上限 |
| `USBPD_CAPS_LIMIT` | 50 | Source 能力无应答发送次数上限 |

关闭功能时 API **不声明**，调用在编译期报错。两角色全关、单角色却启用 DRP/PR_Swap
会触发 `#error`。裁剪构建的配置结构体布局不同，核心、应用及后端必须使用同一配置。
内部存储区仅用于调用者静态分配，不承诺跨版本 ABI。

### 时序默认值

时序宏均可覆盖，但必须满足编译期范围限制，并为最多 1ms Task 调度留出余量。

| 宏（前缀 `USBPD_`） | 默认 ms | 依据/用途 |
|---|---:|---|
| T_CC_DEBOUNCE / T_PD_DEBOUNCE | 150 / 15 | Type-C 2.0 表 4-31 |
| T_DRP_HALF / T_DRP_JITTER | 40 / 5 | Type-C 2.0 表 4-30，整体周期 50..100ms |
| T_ERROR_RECOVERY / T_VCONN_RECOVERY | 30 / 250 | 普通恢复 / 原来提供 VCONN 时至少 240ms |
| T_SENDER_RESPONSE | 27 | PD 2.0 v1.3 表 6-32，24..30ms |
| T_SINK_WAIT_CAP / T_SEND_SOURCE_CAP | 465 / 150 | Type-C 专用能力等待/重发 |
| T_PS_TRANSITION | 500 | 等待 PS_RDY，450..550ms |
| T_SRC_TRANSITION | 30 | 调压/交换关闭前准备，25..35ms |
| T_SRC_READY | 270 | 5V 启动/调压完成看门狗，保留调度余量 |
| T_PS_SOURCE_OFF / T_PS_SOURCE_ON | 835 / 435 | 交换等待，750..920 / 390..480ms |
| T_PS_HARD_RESET / T_SRC_RECOVER | 30 / 750 | 硬复位断电及电源恢复 |
| T_SAFE_ZERO | 640 | 关闭/放电等待上限 |
| T_SWAP_SOURCE_START | 25 | 交换后至少等 20ms 才发送 Source 能力 |
| T_REQUEST_WAIT | 100 | Wait 后重新申请间隔 |
| T_NO_RESPONSE | 5000 | 无响应/复位恢复窗口 |
| T_HARD_SIGNAL | 5 | Hard Reset 信号完成看门狗 |
| T_TX_WATCHDOG | 15 | 后端完成事件看门狗，**不是 GoodCRC 时限** |

## 规范归档与旧版迁移

规范原文及 SHA-256/下载来源归档于仓库根目录 `datasheet/USB_PD_sources.md`：

- `USB_PD_R2.0_V1.3_20170112.pdf`：USB-IF 原文的镜像副本，核对封面版本、消息定义、
  表 6-32/6-34、Source/Sink 与 PR_Swap 状态图；镜像托管方不是规范发布方。
- `USB_Type-C_R2.0_201908.pdf`：USB-IF 官网原文，核对普通端口连接与 DRP 时序。

旧 API 为全局单实例。迁移时删除 `USBPD_TickIsr` 接线，将 Init/Task/查询调用改为
句柄 API，配置显式能力表，重写异步传输及电源后端。事件采用快照结构和用户上下文，
所有运行期 API 使用统一结果码。旧 CH32L103 示例保留用于查阅原 PHY 寄存器接法，
不是 v3 的可运行示例，也不作为新协议行为的依据。

## 验证状态与硬件验收清单

本轮不创建 tests 或新的 examples 目录，不新增 mock 报文测试。构建与静态检查结果
见下方记录；实际接入后至少验证以下场景，不能用“编译通过”替代这些结果：

- Sink/Source 各固定档位首次协商、重新协商和不同 CC 方向；两实例同时工作互不串扰。
- 对端能力含非固定 PDO、对象位置变化、能力降级、5V/0mA 撤销供电。
- Request 越界、过流、GiveBack/保留位非法；Accept 前 Reject/Wait 保持旧合同，
  PS_RDY 前不得使用新合同；Wait 后应用重新发起。
- 两个 DRP 同时上电的角色解析，Rp 电流变化，CC 抖动、拔出、重新插入和普通非 PD 电源。
- PR_Swap 两方向、主动/被动、默认拒绝/方向许可；电源交接不重叠，保留数据/VCONN 角色，
  vSafe0V 与新 5V 反馈异常时退出；新合同重新协商。
- GoodCRC 丢失、错误 ID、重复消息、消息 ID 回绕、TX_DISCARD 与 RX 的顺序、FIFO 溢出。
- Soft Reset、Hard Reset、连续无响应、调压/交换中掉线、发送超时、电源切换超时与硬件故障。
- 毫秒计数回绕，Task 1ms 调度上限，最差 ISR 延迟与 GoodCRC/重试时间，真实电源斜率、
  VCONN 限流/恢复、断电放电和负载维持供电。

2026-09-28 编译与静态检查记录：

- RISC-V GCC 15.2.0（RV32IMAC/ILP32）及 GNU Arm GCC 9.3.1（Cortex-M0/Thumb）
  均通过默认、仅 Sink、仅 Source、双角色无 DRP/交换、仅 DRP、仅 PR_Swap 六组构建。
- 核心与模板采用 `-std=c99 -pedantic-errors -Wall -Wextra -Werror -Wshadow
  -Wconversion -Wstrict-prototypes -Wmissing-prototypes -ffreestanding -Os`。
  核心/模板分别编译并执行可重定位链接，未遗留未实现的 `usbpd_io_*` 符号。
  模板返回错误，仅验证接口链接，不表示后端已经实现。
- 四个头文件在上述配置下通过 C99 和 C++11 包含检查。验证非法裁剪/时序配置
  会被编译期拒绝；关闭的申请 API 不再声明，对应可选 IO 符号也不再依赖。
- 工程配置头、编译参数优先级和 include 路径私有配置通过预处理检查。
  GCC `-fanalyzer` 未报告缺陷；修改文件通过 LF/无 BOM/缩进/行长及 diff 空白检查。
- 默认 32 位 RISC-V ABI 下 `sizeof(usbpd_dev_t)` 为 **284 字节**，核心对象无全局
  可变 `.data/.bss`。这不包含后端 FIFO、DMA 缓冲、调用栈或 C 库链接开销。
- 检查产物仅放系统临时目录，没有新增仓库测试目录或 mock 功能测试。
  **未执行报文往返、真实电源切换、时序测量、互操作或认证测试。**
