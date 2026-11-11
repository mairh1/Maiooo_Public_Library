# 工程两轨布局与分层包含规则

## 1. 为什么采用“两轨布局”？

在嵌入式固件开发中，直接修改芯片官方生成的 `main.c` 或把业务逻辑写在 `stm32f1xx_it.c` 是绝大多数工程走向混乱的开端。
当芯片官方库版本升级、更换型号或引入第三方协议栈时，混杂修改的代码往往造成难以追踪的冲突。

因此，本项目架构严格执行**两轨布局（Two-Track Layout）**：

### 1.1 Vendor 轨（只读官方轨）
- 包含厂商代码生成工具（CubeMX、MounRiver、Keil Pack 等）自动生成的文件：
  - `Core/` 或 `CMSIS/`：内核支持代码；
  - `Startup/`：汇编启动文件；
  - `Peripheral/`：芯片标准外设库或 HAL 库；
  - `User/` 或工程根目录的默认入口文件。
- **规则**：Vendor 轨文件**绝不随意做业务修改**，保持其原始命名与文件结构，只用作底层编译输入。

### 1.2 Project 轨（项目自有轨）
- 全小写命名规范，划分为 5 大职能域：
  - `app/`：应用程序业务层，按业务功能分目录（如 `app_sensor`、`app_ui`）；
  - `bsp/`：板级支持包，包括特定电路板的引脚映射（`bsp_pinmap.h`）、电源控制、板级外设挂载；
  - `driver/`：外设驱动，按控制器独立分类（如 `driver/uart`、`driver/w25qxx`）；
  - `middleware/`：通用算法与数据结构，与具体硬件 0 关联（如环形队列、调度器、数据帧解包）；
  - `config/`：全局纯宏配置（宏定义、类型别名），不包含实际变量或可执行代码。

---

## 2. 单向分层包含规范

为避免“循环包含（Circular Dependency）”和“层级倒置”，头文件包含必须严格遵守**单向依赖**：

```text
app/          -----> 可以包含 bsp/, middleware/, config/
bsp/          -----> 可以包含 driver/, vendor/, config/
driver/       -----> 可以包含 driver_port.h, vendor/, config/ (绝对禁止包含 bsp/)
middleware/   -----> 只能包含自身抽象接口与 config/ (绝对禁止包含 driver/, bsp/, vendor/)
```

### 常见违规与整改方案：

| 错误示范 | 违规原因 | 正确改造方式 |
| :--- | :--- | :--- |
| `driver/oled/oled.c` 中包含 `bsp_pinmap.h` | 驱动层依赖了具体的某块开发板引脚，换板子驱动直接报错 | 在驱动内部定义 `port` 函数指针或操作句柄，引脚映射在 `bsp_oled.c` 中绑定传入 |
| `middleware/protocol.c` 中直接调用 `USART1_Send()` | 中间件被硬件绑定，无法在 PC 模拟器上跑单元测试 | 协议解析完成后只将待发送帧存入输出缓冲区，由上层 app 或 bsp 调用发送 |
| `bsp_led.c` 包含 `app_alarm.h` | 底层反向依赖顶层业务 | 通过事件分发或状态机反向解耦，BSP 只向上汇报按键或向下提供控制接口 |
