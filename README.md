# Maiooo Public Library

个人嵌入式公共代码库：收集通用、可移植、经过实践的嵌入式组件，按用途分为**驱动（driver）**、**中间件（middleware）** 与**工程技能（skills）** 三类。

## 目录结构

```
Public_Library/
├── driver/                       # 外设 / 芯片驱动
│   ├── sgm41513/                 # SGM41513 系列锂电充电管理芯片驱动（圣邦微）
│   └── usb_pd/                   # USB PD Sink 协议栈（可移植，内置 CH32L103 移植）
├── middleware/                   # 与硬件无关的通用中间件
│   ├── fsm/                      # 查表法有限状态机框架
│   └── sort/                     # 冒泡排序算法（多类型）
└── skills/                       # 工程技能
    └── mcu-code-style/           # MCU C 代码规范检查与自动修复
```

## 组件说明

### middleware/fsm —— 有限状态机 v2.0.0

基于查表法的轻量级 FSM 框架，适用于嵌入式场景：

- 状态处理函数以数组注册，按状态值索引调用，用户自定义 `uint8_t` 状态枚举
- 支持 entry / exit 动作，状态切换时自动执行初始化与清理逻辑
- 支持切换钩子（调试 / 日志）、`user_data` 上下文指针、记录上一个状态

### middleware/sort —— 冒泡排序 v2.0.0

多类型冒泡排序实现：

- 基础版与带提前终止优化的版本，均支持升序 / 降序
- 覆盖 `int`、`uint16_t`、`uint32_t` 类型
- v2.0.0 修复了 `n == 1` 时误报 `BUBBLE_ERR_SIZE` 的 bug，补齐全部降序变体，ABI 兼容 v1.1

### driver/sgm41513 —— SGM41513 充电管理驱动

适用于 **SGM41513 / SGM41513A / SGM41513D** 单节锂电充电管理芯片的纯 C99 跨平台驱动，仿照 FatFS 分层设计：

- 接口：I2C（7 位地址 `0x1A`），无动态内存分配，可重入（多实例）
- 移植：只需实现 `sgm41513_io.h` 中的 4 个函数，模板内含 STM32 HAL 与 ESP-IDF 示例
- 功能：充电控制、输入限流 / 欠压管理、OTG 反向升压、JEITA 温窗、船运模式、PUMPX 调压协议、看门狗与寄存器影子缓存

详见 [driver/sgm41513/README.md](driver/sgm41513/README.md)。

### driver/usb_pd —— USB PD Sink 协议栈 v2.0.0

分层可移植的 USB PD（Power Delivery）受电端协议栈，协议流程派生自 WCH CH32L103 USBPD 例程：

```
应用层
  │  usbpd.h            公有 API（Init / TickIsr / Task / 事件回调）
协议核心  usbpd.c        纯 C99，零寄存器访问、零 printf
  │  usbpd_io.h         移植接口
移植层    usbpd_io_ch32l103.c   CH32L103 寄存器 / 中断 / PHY（即插即用）
```

- 功能：CC 接入消抖检测、SRC_CAP 解析、PDO 申请协商（REQUEST → ACCEPT → PS_RDY）、软 / 硬复位恢复、GET_SNK_CAP / GET_STATUS / VDM 等请求应答
- 事件回调上报：源接入、PDO 列表就绪、协商完成（电压 / 电流）、发送失败、硬复位、缓冲错误
- 移植到其他 MCU：实现 `usbpd_io.h` 的 14 个函数即可（模板见 `port/usbpd_io_template.c`），协议核心无需改动
- 集成：上电 `USBPD_Init()`，SysTick 1ms 中断 `USBPD_TickIsr()`，主循环 `USBPD_Task()`

### skills/mcu-code-style —— 代码规范技能

基于 ZCode 的 MCU 嵌入式 C 代码规范检查与自动修复技能：

- 覆盖命名、格式、中文 Doxygen 注释、分层依赖、ISR 与嵌入式安全五类规则
- 规则与具体芯片无关，适用于 CH32 / STM32 / GD32 / N32 等任何 MCU 项目
- 可生成 `.clang-format` / `.editorconfig`

## 许可证

本项目采用 [WTFPL](http://www.wtfpl.net/) 许可证 —— 你想怎么用就怎么用。

[![License: WTFPL](https://img.shields.io/badge/License-WTFPL-blue.svg)](http://www.wtfpl.net/)

> 注：`driver/usb_pd/` 的协议流程派生自 WCH 官方 CH32L103 USBPD 例程，遵循其原始版权声明。
