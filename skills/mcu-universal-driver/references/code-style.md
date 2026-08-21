# 单片机 C 代码规范详解（S1–S4）

`mcu-universal-driver` skill 的参考文档：架构约束（C1–C10）见 [SKILL.md](../SKILL.md)，本文给出代码规范的完整规则与 ❌/✅ 对照示例。示例取材自仓库项目真实代码，规则本身适用于任何 MCU 项目。

原架构类规则（include 方向、内部状态 static、分层依赖）不在此重复——已由架构约束覆盖：include 方向见 C2、模块内部状态见 C7、四层分层见 C1。

## S1 命名

| 对象 | 规则 | 正例 |
|---|---|---|
| 文件名 | 全小写 + 下划线，私有头加 `_` 前缀 | `led.c`、`_led_priv.h` |
| 公有函数 | `Module_Function`（模块前缀大写下划线） | `LED_Init()` |
| 文件内静态函数 | 小写 snake_case + 模块前缀 | `ws2812_gpio_init()` |
| 全局变量 | `g_` 前缀 | `g_adc_buffer` |
| 文件级静态变量 | `s_` 前缀 | `s_led_manager` |
| 宏 / 枚举值 | 全大写 | `LED_HW_COUNT` |
| 类型（typedef） | 小写 snake_case + `_t` 后缀 | `led_state_t` |

### S1.1 公有函数：`Module_Function`

❌ 无模块前缀，Grep 无法按模块定位：

```c
void init(void)
void set_state(int s)
```

✅ 模块前缀 + 大写下划线：

```c
void LED_Init(void)
void LED_SetHwState(led_id_t led_id, bool state)
```

### S1.2 文件内静态函数：小写 snake_case

❌ 与公有函数混用同一种命名，分不清链接范围：

```c
static void GpioInit(void)
```

✅ 小写 + 模块前缀：

```c
static void ws2812_gpio_init(void)
static uint32_t ws2812_hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
```

### S1.3 变量前缀：`g_` / `s_`

❌ 静态文件级变量无前缀，读代码时无法判断作用域：

```c
static multi_led_manager_t led_manager;
uint16_t adc_buffer[8];
```

✅ 作用域一目了然：

```c
static multi_led_manager_t s_led_manager;
uint16_t g_adc_buffer[ADC_BUFFER_SIZE];
```

### S1.4 类型：小写 snake_case + `_t` 后缀

❌ PascalCase 且无后缀：

```c
typedef enum { ... } LedId;
```

✅：

```c
typedef enum { ... } led_state_t;
typedef struct { ... } led_handle_t;
```

> 公共 API 类型重命名会波及所有调用点——先列出影响面征询用户，确认后再改。

### S1.5 宏 / 枚举值全大写 + 尾注释

```c
#define LED_HW_COUNT            4                   /**< LED 硬件数量 */
```

## S2 格式

### S2.1 Allman 大括号（BSD 风格）+ 返回类型独立成行

❌ K&R：`{` 紧跟语句不换行，`} else {` 同行：

```c
void LED_Init(void) {
    LED_GpioInit();

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        s_led_manager.leds[i].state = LED_STATE_OFF;
    }
}
```

✅ Allman：`{` 与 `}` 各自单独成行、与控制语句同缩进；函数定义的返回类型独立成行（BSD KNF）：

```c
void
LED_Init(void)
{
    LED_GpioInit();

    for (uint8_t i = 0; i < LED_COUNT; i++)
    {
        s_led_manager.leds[i].state = LED_STATE_OFF;
    }
}
```

`else` 也单独成行，不与 `}` 同行：

```c
if (led->state == LED_STATE_BLINKING)
{
    LED_SetHwState(led->id, true);
}
else
{
    LED_SetHwState(led->id, false);
}
```

switch 的写法（case 与 switch 同级不缩进，case 块需作用域时 `{` 同样单独成行）：

```c
switch (led->state)
{
case LED_STATE_ON:
{
    ...
    break;
}

default:
    LED_SetHwState(led->id, false);
    break;
}
```

修复时**整个文件统一**，不允许函数间混用。

### S2.2 缩进与列宽

❌ Tab 字符、2 空格缩进、超过 100 列的长行

✅ 4 空格缩进；超长行在合理断点折行。

### S2.3 头文件保护 + `extern "C"`

```c
#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ... 头文件内容 ... */

#ifdef __cplusplus
}
#endif
#endif /* LED_H */
```

### S2.4 `═══` 分节注释

```c
/* ═══════════════════════════════════════════════════
 *  私有数据
 * ═══════════════════════════════════════════════════ */
```

## S3 注释（中文 Doxygen）

### S3.1 文件头

```c
/**
 * @file    led.c
 * @brief   LED 驱动实现
 * @details 合并 HAL 层与状态机管理，基于 CH32L103 GPIO：
 *          - GPIO 初始化与硬件状态控制
 *          - 常亮、定时关闭、无限闪烁、指定次数闪烁
 * @note    依赖：led.h, _led_priv.h。不依赖任何 app/ 层头文件。
 * @author  Maiooo
 * @version 2.1.0
 * @date    2026-07-31
 */
```

❌ 常见缺失：没有 `@brief`；`@date` 写成不存在的格式；文件头用 `//`。

### S3.2 函数注释

```c
/**
 * @brief  硬件层设置单个 LED 状态
 * @param  led_id  LED 编号
 * @param  state   true 为点亮，false 为熄灭
 */
```

❌ 只有一行 `// 初始化`；或 `@param` 与函数实参对不上（改签名后忘了同步）。

### S3.3 宏 / 成员尾注释

```c
uint32_t blink_interval;        /**< 闪烁周期(ms) */
uint16_t blink_count_target;    /**< 目标闪烁次数（0 表示无限闪烁） */
```

## S4 ISR 与共享数据

### S4.1 ISR 内只做标志位 / 计数器更新

❌ ISR 内 printf / 延时，阻塞且不可重入：

```c
void
TIM2_IRQHandler(void)
{
    printf("tick\n");        /* ISR 内 printf，阻塞且不可重入 */
    Delay_Ms(10);            /* ISR 内延时，拖死系统 */
}
```

✅ ISR 只递增 volatile 计数，繁重工作留给主循环：

```c
void
LED_TickIsr(void)
{
    s_led_system_tick++;     /* s_led_system_tick 声明为 volatile */
}
```

### S4.2 共享数据保护

- ISR 与主循环共享的变量加 `volatile`
- 多字节共享数据（缓冲区、结构体）读写进临界区，防撕裂

### S4.3 重命名强制顺序（违反会引入编译错误）

Grep 旧名列出全部引用点 → 逐处同步修改 → 再 Grep 旧名确认**零残留**。公共 API 重命名波及 ≥2 个模块时，先列出影响面征询用户，确认后再动手。

## 配置生成模式

用户要求生成 `.clang-format` / `.editorconfig` 时：

1. 复制 `assets/clang-format` → 项目根 `.clang-format`
2. 复制 `assets/editorconfig` → 项目根 `.editorconfig`

并告知用户：`.clang-format` 只能覆盖 S2 格式类规则；S1 命名、S3 注释、S4 ISR 安全是任何格式化工具都做不了的——这些正是需要 AI 按 skill 执行的部分。

## 修复红线（不可越过）

- 不改任何执行逻辑、运算、时序——只动标识符、空白、注释
- 不删用户已有注释，只补充和修正
- 中断向量名、`*_IRQHandler`、启动文件中注册的符号**绝对不改**
- 以下厂商 / 第三方内容**任何情况都不修改**：官方外设库 / SDK（`Peripheral/`、`SDK/`、`Drivers/`、CMSIS）、内核支持 `Core/`、启动文件 `Startup/`、链接脚本 `Ld/`、`system_*.c/h` 与 `*_it.c`、IDE 工程文件（`.cproject` `.project` `.launch` 等）、构建输出目录、第三方库（看 LICENSE / README 判定）
- 保持 LF 行尾，不引入 CRLF / BOM
- 修复涉及行为语义时（如给等待循环加超时会改变返回路径），只标记问题不动手，说明理由留给用户决定
