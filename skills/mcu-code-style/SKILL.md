---
name: mcu-code-style
description: 单片机（MCU）嵌入式 C 代码规范检查与自动修复，规则与具体芯片无关，适用任何 MCU 项目（CH32/STM32/GD32/N32 等，MounRiver/Keeb/CubeIDE 等工具链）。覆盖五类规则：命名（Module_Function、g_/s_ 前缀、宏全大写、_t 后缀）、格式（4 空格缩进、Allman 大括号、返回类型独立成行、#ifndef 头保护、═══ 分节注释）、中文 Doxygen 注释、分层依赖（驱动不依赖应用层）、ISR 与嵌入式安全。当用户要求检查代码规范、格式化代码、做规范审查、修复命名或注释、生成 .clang-format 或 .editorconfig 时使用——即使没有明说"规范"二字（如"帮我看看这段代码"/"这代码写得行不行"）也应触发。
---

# 单片机 C 代码规范检查与自动修复

把单片机嵌入式 C 编码规范落地为可执行动作：按规则清单检查目标代码并**直接修复**，按需生成 `.clang-format` / `.editorconfig`。规则与具体芯片无关，适用于任何 MCU 项目的自有代码层。

## 第一步：解析目标范围

先确定用户要检查什么，范围不明时再问：

- **单文件**：如 `driver/led/led.c`——读该文件及其头文件
- **模块目录**：如 `driver/ws2812/`——Glob 收集目录下所有 `.c`/`.h`
- **全仓库**：Glob 项目自有层的所有 `.c`/`.h`，先统计数量告知用户再动手
- **新写的代码**：以 git diff 或最近改动确定范围

同时定位项目根（含 `.git`、`.cproject` 等标志），后续所有相对路径以项目根为基准。

## 第二步：套用保护清单（只读，绝对不写）

以下内容属于厂商或第三方，**任何情况都不修改**：

| 类型 | 典型目录 / 文件 |
|------|-----------------|
| 官方外设库 / SDK | `Peripheral/`、`SDK/`、`Drivers/`（HAL）、CMSIS |
| 内核支持 | `Core/` |
| 启动文件 | `Startup/` |
| 链接脚本 | `Ld/` |
| 官方系统文件 | `system_*.c/h`、`*_conf.h`、中断文件 `*_it.c` |
| 调试支持 | `Debug/` |
| IDE 工程文件 | `.cproject` `.project` `.template` `.launch` `.wvproj` |
| 构建输出 | `Obj/` `Build/` `release/` |
| 第三方库 | FastLED、u8g2 等（看 LICENSE / README 判定） |

项目自有代码层（可修改）：`app/`、`bsp/`、`driver/`、`middleware/`、`config/` 及入口 `main.c`。目录名各项目不同，按"厂商库之外的业务代码"判断；拿不准时先读文件头注释与 git log，仍拿不准就问用户。

## 第三步：逐规则检查并修复

详细的 ❌/✅ 代码对照示例见[规则对照示例](references/rule-examples.md)——开始逐规则检查前先读它。

### R1 命名

| 对象 | 规则 | 正例 |
|------|------|------|
| 文件名 | 全小写 + 下划线，私有头加 `_` 前缀 | `led.c`、`_led_priv.h` |
| 公有函数 | `Module_Function`（模块前缀大写下划线） | `LED_Init()` |
| 文件内静态函数 | 小写 snake_case + 模块前缀 | `ws2812_gpio_init()` |
| 全局变量 | `g_` 前缀 | `g_adc_buffer` |
| 文件级静态变量 | `s_` 前缀 | `s_led_manager` |
| 宏 / 枚举值 | 全大写 | `LED_GPIO_GROUP` |
| 类型（typedef） | 小写 snake_case + `_t` 后缀 | `led_state_t` |

检查方法：Grep 批量扫（函数定义、`^static`、全局定义等模式），不必逐文件通读。

### R2 格式

- 4 空格缩进，禁止 Tab；单行不超过 100 列；LF 行尾
- **Allman 大括号（BSD 风格）**：`{` 与 `}` 各自单独成行、与控制语句同缩进，`else` 也独立成行；整个文件必须统一，不允许函数间混用
- **返回类型独立成行**（BSD KNF）：函数定义的返回类型单独一行，函数名另起一行
- 注释一律 `/* */`，不使用 `//`
- 头文件保护：`#ifndef XXX_H` → `#define XXX_H` → 内容 → `#endif /* XXX_H */`，并包 `extern "C"`
- 用 `═══` 分节注释（私有数据 / 类型定义 / API 等分节）组织文件结构

### R3 注释（中文）

- 文件头：`@file @brief @details @note @author @version @date`
- 函数：`@brief @details @param @retval`（每个参数一行对齐）
- 宏、结构体成员、枚举值：尾部 `/**< 说明 */`
- 注释写"为什么"和约束，不复述代码字面

### R4 架构

- 分层：`main → app → bsp → driver/middleware → 厂商库`；上层 include 下层，**禁止反向**
- 驱动、中间件不得 include `app/`、`bsp/` 层头文件
- 模块内部状态一律 `static` + 访问函数，不裸露全局变量
- 头文件自包含：include 自己直接用到的依赖

### R5 嵌入式安全

- ISR 内只做标志位 / 计数器更新，**禁止 printf、延时、阻塞等待、大循环**
- 禁止 `malloc/free`、VLA、递归
- 硬件等待循环必须带超时退出
- ISR 与主循环共享的变量加 `volatile`；多字节共享数据进临界区

## 第四步：执行流程

默认**检查 + 直接修复**（用户明说"只检查不修"时只输出报告）：

1. **收集目标**：按第一步范围 Glob `*.c` `*.h`，排除保护清单目录
2. **逐规则检查**：R1 用 Grep 批量；R2/R3 读文件逐段看；R4 Grep include 语句确认依赖方向；R5 定位 ISR 函数体查 printf/Delay/malloc/无超时 while
3. **直接 Edit 修复**：同一文件内的格式、注释问题一次修完
4. **命名重构强制顺序**（违反会引入编译错误，必须严格执行）：
   Grep 旧名列出全部引用点 → 逐处同步 Edit → 再 Grep 旧名确认**零残留**。
   公共 API 重命名波及 ≥2 个模块时，先列出影响面征询用户，确认后再动手
5. **提醒用户重新编译**验证（本 skill 不执行编译）
6. **输出汇总表格**：

   | 文件:行号 | 规则 | 问题 | 处理 |
   |-----------|------|------|------|
   | driver/led/led.c:89 | R2 | for 循环 K&R 大括号 | 已转 Allman |

## 修复红线（不可越过）

- 不改任何执行逻辑、运算、时序——只动标识符、空白、注释
- 不删用户已有注释，只补充和修正
- 中断向量名、`*_IRQHandler`、启动文件中注册的符号**绝对不改**
- 保护清单内的文件绝对不动
- 保持 LF 行尾，不引入 CRLF / BOM
- 修复涉及行为语义时（如给等待循环加超时会改变返回路径），只标记问题不动手，说明理由留给用户决定

## 配置生成模式

用户要求生成 `.clang-format` / `.editorconfig` 时：

1. 复制 `assets/clang-format` → 项目根 `.clang-format`
2. 复制 `assets/editorconfig` → 项目根 `.editorconfig`

并告知用户：`.clang-format` 只能覆盖 R2 格式类规则；R1 命名、R3 注释、R4 架构、R5 ISR 安全是任何格式化工具都做不了的——这些正是需要本 skill 的部分。
