# Maiooo Public Library

个人嵌入式公共代码库：收集通用、可移植、经过实践的嵌入式组件，按用途分为驱动（driver）、中间件（middleware）与工程技能（skills）三类。

## 目录结构

```
Public_Library/
├── driver/                    # 外设 / 芯片驱动
│   ├── sgm41513/              # SGM41513 系列锂电充电管理驱动
│   ├── usb_pd/                # USB PD Sink 协议栈（内置 CH32L103 移植）
│   ├── AW32257_Drivers/       # AW32257 充电/升压芯片驱动
│   └── WM8978_Drivers/        # WM8978 音频编解码芯片驱动
├── middleware/                # 与硬件无关的通用中间件
│   ├── fsm/                   # 查表法有限状态机框架
│   └── sort/                  # 冒泡排序算法（多类型）
└── skills/                    # 工程技能
    ├── mcu-code-style/        # MCU C 代码规范检查与自动修复
    └── mimo-editorial-image-style/   # 文生图编辑风格技能
```

## 组件说明

- **driver/sgm41513**：SGM41513 / 41513A / 41513D 锂电充电管理驱动，I2C 接口，纯 C99 跨平台，无动态内存分配，可多实例。
- **driver/usb_pd**：分层可移植的 USB PD 受电端协议栈（协议核心零寄存器访问），移植只需实现 `usbpd_io.h` 的 14 个函数。
- **driver/AW32257_Drivers**：AW32257 充电/升压芯片通用驱动，ISO C99，不含厂商头文件，通过 BSP 回调接入。
- **driver/WM8978_Drivers**：WM8978 音频编解码芯片通用驱动，ISO C99，通过 I2C 接口接入。
- **middleware/fsm**：基于查表法的轻量级 FSM 框架，支持 entry / exit 动作与切换钩子。
- **middleware/sort**：多类型冒泡排序（int / uint16_t / uint32_t），支持升序 / 降序与提前终止优化。
- **skills/mcu-code-style**：MCU 嵌入式 C 代码规范检查与自动修复技能，规则与具体芯片无关。

## 许可证

本项目采用 [WTFPL](http://www.wtfpl.net/) 许可证 —— 你想怎么用就怎么用。

[![License: WTFPL](https://img.shields.io/badge/License-WTFPL-blue.svg)](http://www.wtfpl.net/)

> 注：`driver/usb_pd/` 的协议流程派生自 WCH 官方 CH32L103 USBPD 例程，遵循其原始版权声明。
