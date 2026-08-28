# Maiooo Public Library

个人嵌入式公共代码库：收集通用、可移植、经过实践的嵌入式组件，按用途分为驱动（driver）、中间件（middleware）与工程技能（skills）三类。

## 目录结构

```
Public_Library/
├── driver/                    # 外设 / 芯片驱动
│   ├── SGM41513_Driver/       # SGM41513 系列锂电充电管理驱动
│   ├── USB_PD_Driver/         # USB PD Sink 协议栈（内置 CH32L103 移植）
│   ├── AW32257_Driver/        # AW32257 充电/升压芯片驱动
│   ├── WM8978_Driver/         # WM8978 音频编解码芯片驱动
│   ├── MAX17048_Driver/       # MAX17048/49 电量计芯片驱动
│   └── INA219_Driver/         # INA219 电流/功率监测芯片驱动
├── middleware/                # 与硬件无关的通用中间件
│   ├── fsm/                   # 查表法有限状态机框架
│   └── sort/                  # 冒泡排序算法（多类型）
└── skills/                    # 工程技能
    ├── mcu-code-style/        # MCU C 代码规范检查与自动修复
    ├── mcu-universal-driver/  # 通用驱动框架约束（架构 + 代码规范）
    └── mimo-editorial-web-style/     # 编辑排版风格网页生成技能
```

## 组件说明

- **driver/SGM41513_Driver**：SGM41513 / 41513A / 41513D 锂电充电管理驱动，I2C 接口，纯 C99 跨平台，无动态内存分配，可多实例。
- **driver/USB_PD_Driver**：分层可移植的 USB PD 受电端协议栈（协议核心零寄存器访问），移植只需实现 `usbpd_io.h` 的 14 个函数。
- **driver/AW32257_Driver**：AW32257 充电/升压芯片通用驱动，ISO C99，不含厂商头文件，通过 BSP 回调接入。
- **driver/WM8978_Driver**：WM8978 音频编解码芯片通用驱动，ISO C99，通过 I2C 接口接入。
- **driver/MAX17048_Driver**：MAX17048/MAX17049 ModelGauge 锂电电量计通用驱动，I2C 接口（7 位地址 0x36，16 位寄存器），纯 C99 定点运算，可多实例，含 CH32 移植示例与 RV32 模拟单测。
- **driver/INA219_Driver**：INA219A/B 电流/功率监测芯片通用驱动，I2C 接口（7 位地址 0x40~0x4F，16 位寄存器），默认按 1Ω 采样电阻配置（±320mA 量程 / 10µA 分辨率），纯 C99 定点运算，可多实例，含 CH32 移植示例与 RV32 模拟单测。
- **middleware/fsm**：基于查表法的轻量级 FSM 框架，配置与实例分离（实例仅 8 字节 RAM），entry / exit 动作、切换钩子与上一状态回退支持编译期裁剪，纯 C99 零平台依赖。
- **middleware/sort**：多类型冒泡排序（int / uint16_t / uint32_t），支持升序 / 降序与提前终止优化。
- **skills/mcu-code-style**：MCU 嵌入式 C 代码规范检查与自动修复技能，规则与具体芯片无关。
- **skills/mcu-universal-driver**：跨平台可移植驱动框架约束，覆盖四层架构（应用层 → 驱动核心 → 移植契约 → 移植层）与单片机 C 代码规范，与具体芯片、总线、工具链无关。

## 许可证

本项目采用 [WTFPL](http://www.wtfpl.net/) 许可证 —— 你想怎么用就怎么用。

[![License: WTFPL](https://img.shields.io/badge/License-WTFPL-blue.svg)](http://www.wtfpl.net/)
