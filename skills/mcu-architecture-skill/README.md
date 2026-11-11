# 通用单片机（MCU）固件架构设计规范 Skill

这是一套可直接用于各类单片机裸机固件开发的通用软件架构规范与参考实现。

## 🎯 核心目标

1. **消除代码耦合**：业务逻辑、板级硬件接线、芯片外设寄存器彻底解耦。
2. **解决系统假死**：全面消灭无超时死循环（`while (flag == 0)`）与长延时阻塞。
3. **保证代码可维护**：两轨布局、Allman/大括号规范、模块对象化封装。
4. **提升代码复用率**：更换单片机平台（如从 STM32 迁移至 GD32/CH32/ESP32）时，上层业务与通用驱动零改动。

---

## 📂 目录结构

```text
mcu-architecture-skill/
├── SKILL.md                  # Skill 核心规则与规范总览
├── README.md                 # 快速上手指南
├── references/               # 深度技术实现与架构指南
│   ├── layout.md             # 工程两轨布局与分层包含规则
│   ├── scheduling.md         # 时间片轮询 + 事件队列 + 状态机实现细节
│   ├── driver-port.md        # Driver-Port 外设解耦设计模式
│   └── safety-rules.md       # 防御式编程：超时、中断与内存红线
└── templates/                # 开箱即用的 C 语言通用组件
    ├── scheduler.h/.c        # 轻量级时间片调度器与事件队列实现
    ├── ringbuffer.h          # 无锁单读单写（SPSC）环形缓冲区
    └── driver_template.h/.c  # 符合 Driver-Port 规范的标准外设驱动模板
```

---

## 🚀 快速上手：如何将本架构引入新工程

1. **建立两轨目录**：
   - 将原厂 HAL/标准库/启动文件归入 `vendor/`，保持其原始命名与文件结构，只读使用。
   - 创建 `app/`、`bsp/`、`driver/`、`middleware/`、`config/` 等项目轨目录。
2. **引入核心基础组件**：
   - 复制 `templates/scheduler.h/.c` 到 `middleware/scheduler/`。
   - 复制 `templates/ringbuffer.h` 到 `middleware/ringbuffer/`。
3. **实现板级引脚与时钟初始化**：
   - 在 `bsp/bsp_pinmap.h` 中集中管理所有 GPIO 引脚定义。
   - 在 `bsp/bsp_init.c` 中初始化系统滴答定时器（SysTick 1ms）并调用外设初始化。
4. **主循环编写**：
   - 在 `app/app_main.c` 中配置时间片任务列表，在 `main()` 循环中调用 `Scheduler_Run()`。
