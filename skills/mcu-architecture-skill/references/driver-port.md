# Driver-Port 硬件解耦模式详解

## 1. 传统驱动编写的通病

很多初中级嵌入式工程师习惯这样写驱动：
```c
/* oled.c - 深度绑定硬件的错误示范 */
#include "stm32f10x.h"

void OLED_WriteByte(uint8_t data)
{
    /* 直接写死操作 GPIOB 的 Pin 8 和 Pin 9 */
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
    // ...
}
```
**这种写法带来的灾难**：
1. 如果另一块板子上 OLED 接在 GPIOA 上，必须重改甚至重写驱动代码。
2. 同一个工程里挂两块 OLED 时，无法复用同一套函数。
3. 换成 GD32 或 CH32 芯片时，整个驱动报废重修。

---

## 2. Driver-Port 标准解耦范式

标准解耦模式将驱动分解为：
- `<device>.h / .c`：负责该外设的协议、时序逻辑和高级命令包装（纯逻辑，芯片无关）。
- `<device>_port.h`：定义该设备依赖的硬件底层读写接口（函数指针或操作句柄）。

### 2.1 驱动接口定义（面向对象思想）

```c
/* driver/oled/oled.h */
#ifndef OLED_H
#define OLED_H

#include <stdint.h>

/**
 * @brief OLED 硬件适配操作接口 (Port)
 */
typedef struct
{
    void (*set_cs)(uint8_t level);
    void (*set_dc)(uint8_t level);
    void (*set_rst)(uint8_t level);
    void (*write_byte)(uint8_t byte);
    void (*delay_ms)(uint32_t ms);
} Oled_Port_Ops_t;

/**
 * @brief OLED 驱动控制块 (Handle)
 */
typedef struct
{
    const Oled_Port_Ops_t *ops;     /**< 底层操作句柄 */
    uint8_t width;                  /**< 屏幕宽度 */
    uint8_t height;                 /**< 屏幕高度 */
    uint8_t is_initialized;
} Oled_Device_t;

int  Oled_Init(Oled_Device_t *dev, const Oled_Port_Ops_t *ops);
void Oled_Clear(Oled_Device_t *dev);
void Oled_ShowChar(Oled_Device_t *dev, uint8_t x, uint8_t y, char chr);

#endif
```

### 2.2 在板级支持包（BSP）中注入实现

```c
/* bsp/bsp_oled.c */
#include "oled.h"
#include "bsp_pinmap.h"

static void Bsp_Oled_SetCS(uint8_t level)
{
    // 调用具体硬件平台的 GPIO 控制
    HAL_GPIO_WritePin(OLED_CS_GPIO_PORT, OLED_CS_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static const Oled_Port_Ops_t s_oled_ops = {
    .set_cs     = Bsp_Oled_SetCS,
    .set_dc     = Bsp_Oled_SetDC,
    .set_rst    = Bsp_Oled_SetRst,
    .write_byte = Bsp_Spi_WriteByte,
    .delay_ms   = Bsp_DelayMs,
};

static Oled_Device_t g_main_screen;

void Bsp_Oled_Init(void)
{
    Oled_Init(&g_main_screen, &s_oled_ops);
}
```

---

## 3. 解耦带来的工程收益

1. **零成本跨平台迁移**：移植到任何芯片架构，只需重新编写 `bsp_oled.c` 的 5 个钩子函数，`oled.c` 核心驱动源码 1 行都不用动。
2. **多实例支持**：只需定义 `g_screen_left` 和 `g_screen_right` 两个句柄，分别传入不同的操作集即可驱动双屏。
3. **单元测试与仿真**：可以在 PC 上用控制台输出模拟这 5 个函数，在 Windows/Linux 环境直接测试复杂的绘图算法或字模渲染。
