# 规则对照示例（❌/✅）

每条规则一节，反例与正例对照。示例取材自 CH32_RGBLed 项目真实代码；为与当前规范保持一致，标记为 ✅ 的片段已做规范化整理。规则本身适用于任何 MCU 项目。S1–S5 的规则、优先级和例外以 `SKILL.md` 为唯一准则；本文件仅提供 ❌/✅ 对照，不新增或覆盖规则。单个示例只说明其所属小节的规则，其余标识符不作为额外规范依据。

## S1 命名

### S1.1 公有函数：`Module_Function`

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

### S1.2 文件内静态函数：小写 snake_case

❌ 与公有函数混用同一种命名，分不清链接范围：

```c
static void GpioInit(void)
```

✅ 来自 ws2812.c，小写 + 模块前缀：

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

✅ 来自 led.c / adc.c，作用域一目了然：

```c
static multi_led_manager_t s_led_manager;
uint16_t g_adc_buffer[ADC_BUFFER_SIZE];
```

### S1.4 类型：小写 snake_case + `_t` 后缀

❌ PascalCase 且无后缀：

```c
typedef enum { ... } LedId;
```

✅ 来自 led.h：

```c
typedef enum { ... } led_state_t;
typedef struct { ... } led_handle_t;
```

> **标记案例**：CH32_RGBLed 的 `led.h` 中 `LedId` 是故意保留的反例和历史遗留不一致，仅用于展示 S1.4 违规。它是公共 API，重命名波及 `led.c`、`led.h` 及所有 app 层调用点——按 SKILL.md 第四步第 4 条，先列影响面征询用户，确认后再改。

### S1.5 宏 / 枚举值全大写

❌：

```c
#define LedCount 4
```

✅ 来自 led.h，全大写 + 尾注释：

```c
#define LED_HW_COUNT            4                   /**< LED 硬件数量 */
```

### S1.6 文件名

❌：`LedDriver.c`、`ledDriver.c`

✅：`led.c`、`led.h`；模块私有头加下划线前缀：`_led_priv.h`

### S1.7 驱动命名例外与芯片驱动目录命名

规则正文见 `SKILL.md` 的 S1。要点：芯片 / 片上外设驱动公共 API 用模块前缀小写 snake_case（如 `ina219_init()`）；芯片驱动目录为 `<芯片型号>_Driver`（如 `SGM41513_Driver`）。

## S2 格式

### S2.1 Allman 大括号（BSD 风格）

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

### S2.2 缩进与列宽

❌ Tab 字符、2 空格缩进、超过 100 列的长行

✅ 4 空格缩进；超长行在合理断点折行。

### S2.3 头文件保护 + `extern "C"`（来自 led.h）

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

### S2.4 `═══` 分节注释（来自 led.c）

```c
/* ═══════════════════════════════════════════════════
 *  私有数据
 * ═══════════════════════════════════════════════════ */
```

## S3 注释（中文 Doxygen）

### S3.1 注释语言：中文优先，难译词保留原文

- 说明性注释一律优先用中文书写，不写整段英文注释
- 没有通用中文译法、不好翻译的术语单词保留英文或原文，推荐「中文(原文)」夹注写法，禁止强行硬译造成歧义
- Doxygen 关键字（`@brief` 等）、寄存器名、引脚 / 信号名保持原样

❌ 整段英文注释：

```c
/* Configure the fuel gauge and enable burst transfer */
```

❌ 强行硬译，读者无法对照数据手册原文：

```c
/**< 燃料计模式 */          /* fuel gauge 被硬译 */
/**< 汇端请求电压 */        /* Sink 的硬译，与协议手册用词对不上 */
```

✅ 中文为主，难译词夹注原文：

```c
/**< 配置电量计(fuel gauge)并使能突发(burst)传输 */
/**< Sink(受电端)请求的电压，单位 mV */
/**< JEITA 温度档充电限值 */
```

### S3.2 文件头（来自 led.c）

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

### S3.3 函数注释（来自 led.c）

```c
/**
 * @brief  硬件层设置单个 LED 状态
 * @param  led_id  LED 编号
 * @param  state   true 为点亮，false 为熄灭
 */
```

❌ 只有 `// 初始化` 一行；或 `@param` 与函数实参对不上（改签名后忘了同步）。

### S3.4 宏 / 成员尾注释（来自 led.h）

```c
uint32_t blink_interval;        /**< 闪烁周期(ms) */
uint16_t blink_count_target;    /**< 目标闪烁次数（0 表示无限闪烁） */
```

## S4 ISR 与共享数据

### S4.1 ISR 共享数据：只做标志位 / 计数器更新

正文见 SKILL.md 的 S4 与「通用架构与嵌入式安全检查项」的 ISR 约束。

❌ ISR 内 printf / 延时，阻塞且不可重入：

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

### S4.2 共享数据保护

- ISR 与主循环共享的变量加 `volatile`
- 多字节共享数据（缓冲区、结构体）读写进临界区，防撕裂

### S4.3 重命名强制顺序（违反会引入编译错误）

Grep 旧名列出全部引用点 → 逐处同步修改 → 再 Grep 旧名确认**零残留**。公共 API 重命名波及 ≥2 个模块时，先列出影响面征询用户，确认后再动手。

## S5 可读性与可维护性

规则正文见 `SKILL.md` 的 S5。对照要点：

✅ 控制流程分步命名、注释说明硬件约束与单位；测试数据有协议或手册依据。

❌ 为省行数把多步硬件操作压成一行、用晦涩宏隐藏副作用；无需求来源的断言堆砌。

## 通用架构与嵌入式安全检查项

规则正文见 `SKILL.md`「通用架构与嵌入式安全检查项」。

### 架构：include 方向

❌ 驱动反向依赖应用层，模块无法复用：

```c
/* driver/led/led.c */
#include "app_state.h"      /* 反向依赖！ */
```

✅ 来自 led.h 的 `@note`："依赖：ch32l103.h。不依赖任何 app/ 层头文件。"

分层与依赖方向的完整规则以 `SKILL.md` 的通用架构检查项为准；以下片段只用于说明反向 include 的风险。

### 架构：内部状态 static + 访问函数

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

### 安全：禁 malloc / free / 递归

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

### 安全：硬件等待必须带超时

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
