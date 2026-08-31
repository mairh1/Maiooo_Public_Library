# 规则对照示例（❌/✅）

每条规则一节，反例与正例对照。示例取材自 CH32_RGBLed 项目真实代码；为与当前规范保持一致，标记为 ✅ 的片段已做规范化整理。规则本身适用于任何 MCU 项目。R1–R5 的规则、优先级和例外以 `SKILL.md` 为唯一准则；本文件仅提供 ❌/✅ 对照，不新增或覆盖规则。单个示例只说明其所属小节的规则，其余标识符不作为额外规范依据。

## R1 命名

### R1.1 公有函数：`Module_Function`

❌ 无模块前缀，Grep 无法按模块定位：

```c
void init(void)
void set_state(int s)
```

✅ 模块前缀 + 大写下划线（来自 led.c）：

```c
void LED_Init(void)
void LED_SetHwState(led_id_t led_id, bool state)
```

### R1.2 文件内静态函数：小写 snake_case

❌ 与公有函数混用同一种命名，分不清链接范围：

```c
static void GpioInit(void)
```

✅ 来自 ws2812.c，小写 + 模块前缀：

```c
static void ws2812_gpio_init(void)
static uint32_t ws2812_hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
```

### R1.3 变量前缀：`g_` / `s_`

❌ 静态文件级变量无前缀，读代码时无法判断作用域：

```c
static multi_led_manager_t led_manager;
uint16_t adc_buffer[8];
```

✅ 来自 led.c / adc.c，作用域一目了然：

```c
static multi_led_manager_t s_led_manager;
uint16_t g_adc_buffer[ADC_BUFFER_SIZE];
```

### R1.4 类型：小写 snake_case + `_t` 后缀

❌ PascalCase 且无后缀：

```c
typedef enum { ... } LedId;
```

✅ 来自 led.h：

```c
typedef enum { ... } led_state_t;
typedef struct { ... } led_handle_t;
```

> **标记案例**：CH32_RGBLed 的 `led.h` 中 `LedId` 是故意保留的反例和历史遗留不一致，仅用于展示 R1.4 违规。它是公共 API，重命名波及 `led.c`、`led.h` 及所有 app 层调用点——按 SKILL.md 第四步第 4 条，先列影响面征询用户，确认后再改。

### R1.5 宏 / 枚举值全大写

❌：

```c
#define LedCount 4
```

✅ 来自 led.h，全大写 + 尾注释：

```c
#define LED_HW_COUNT            4                   /**< LED 硬件数量 */
```

### R1.6 文件名

❌：`LedDriver.c`、`ledDriver.c`

✅：`led.c`、`led.h`；模块私有头加下划线前缀：`_led_priv.h`

## R2 格式

### R2.1 Allman 大括号（BSD 风格）

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
    LED_SetHwState((led_id_t)i, false);
    break;
}
```

修复时**整个文件统一**，不允许函数间混用。

### R2.2 缩进与列宽

❌ Tab 字符、2 空格缩进、超过 100 列的长行

✅ 4 空格缩进；超长行在合理断点折行。

### R2.3 头文件保护 + `extern "C"`（来自 led.h）

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

### R2.4 `═══` 分节注释（来自 led.c）

```c
/* ═══════════════════════════════════════════════════
 *  私有数据
 * ═══════════════════════════════════════════════════ */
```

## R3 注释（中文 Doxygen）

### R3.1 文件头（来自 led.c）

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

### R3.2 函数注释（来自 led.c）

```c
/**
 * @brief  硬件层设置单个 LED 状态
 * @param  led_id  LED 编号
 * @param  state   true 为点亮，false 为熄灭
 */
```

❌ 只有 `// 初始化` 一行；或 `@param` 与函数实参对不上（改签名后忘了同步）。

### R3.3 宏 / 成员尾注释（来自 led.h）

```c
uint32_t blink_interval;        /**< 闪烁周期(ms) */
uint16_t blink_count_target;    /**< 目标闪烁次数（0 表示无限闪烁） */
```

## R4 架构

### R4.1 include 方向

❌ 驱动反向依赖应用层，模块无法复用：

```c
/* driver/led/led.c */
#include "app_state.h"      /* 反向依赖！ */
```

✅ 来自 led.h 的 `@note`："依赖：ch32l103.h。不依赖任何 app/ 层头文件。"

分层与依赖方向的完整规则以 `SKILL.md` 的 R4 为准；以下片段只用于说明反向 include 的风险。

### R4.2 内部状态 static + 访问函数

❌ 全局裸露，任何文件都能改：

```c
multi_led_manager_t led_manager;   /* 全局可写 */
```

✅ 来自 led.c：

```c
static multi_led_manager_t s_led_manager;   /* 仅本文件可访问 */

void
LED_On(led_id_t id)                         /* 外部通过 API 操作 */
{
    ...
}
```

## R5 嵌入式安全

### R5.1 ISR 内禁止 printf / 延时 / 阻塞

❌：

```c
void
TIM2_IRQHandler(void)
{
    printf("tick\n");        /* ISR 内 printf，阻塞且不可重入 */
    Delay_Ms(10);            /* ISR 内延时，拖死系统 */
}
```

✅ 来自 led.c——ISR 只递增 volatile 计数，繁重工作留给主循环：

```c
void
LED_TickIsr(void)
{
    s_led_system_tick++;     /* s_led_system_tick 声明为 volatile */
}
```

### R5.2 禁 malloc / free / 递归

❌ 堆碎片与栈溢出在 MCU 上不可恢复：

```c
char *buf = (char *)malloc(len);

int
walk(int n)
{
    return walk(n - 1);
}
```

✅ 静态缓冲 / 固定数组，递归改循环或状态机。

### R5.3 硬件等待必须带超时

❌ 硬件异常时死循环，看门狗复位都查不到原因：

```c
while (!(FLASH->STATR & FLASH_FLAG_BSY))
{
}
```

✅ 带超时退出并返回错误码：

```c
uint32_t timeout = FLASH_TIMEOUT_COUNT;
while (!(FLASH->STATR & FLASH_FLAG_BSY))
{
    if (--timeout == 0)
    {
        return FLASH_STATUS_TIMEOUT;
    }
}
```

> 注意：给已有代码补超时会改变返回路径（行为语义变化），按修复红线只标记、说明理由，由用户决定是否修改。
