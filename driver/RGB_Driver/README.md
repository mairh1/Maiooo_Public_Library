# RGB_Driver

独立的低内存 RGB 可寻址灯阵列驱动。设计思想参考 NeoPixel 的像素模型、FastLED 的整数颜色运算和 WS2812FX 的时间驱动特效，但本库不兼容、不依赖、也不复制上述项目。

## 特点

- 纯 C99，核心零平台依赖；不包含 MCU、GPIO、SPI、DMA 或 RTOS 头文件。
- 不使用 `malloc`、浮点数或隐藏全局像素缓冲区。
- 像素缓冲区由调用者提供，每颗 RGB 灯占 3 字节。
- RGB/GRB 通道顺序由 `rgbled_conf.h` 配置。
- 发送协议由 `rgbled_io_t` 回调实现，可接 GPIO、SPI、DMA、定时器或其他外设。
- `rgbled_update()` 与 `rgbled_show()` 分离：特效只改像素，发送由应用显式触发。
- 可通过回调扩展特效，不要求为特效分配动态内存。

## 分层架构

```
+----------------------------------------------+
|  应用层（你的代码）                           |
+----------------------------------------------+
        |
        |  公共 API + 统一结果码（rgbled.h）
+----------------------------------------------+
|  驱动核心    rgbled.c                         |  纯 C99，零平台代码
|  内置特效    rgbled_effects.c                 |  亮度/发送流程/颜色算法
|  编译期配置  rgbled_conf.h                    |  时间驱动特效
+----------------------------------------------+
        |
        |  输出契约（rgbled_io.h）：rgbled_io_t 回调集合
        |  write/latch/lock/unlock + io_ctx 透传，
        |  由应用填充后在 rgbled_init() 时注入（回调结构体方式，
        |  非固定 _io.h 函数契约，属待迁移旧范式）
+----------------------------------------------+
|  平台适配层（你实现回调）                      |  GPIO 位崩 /
|                                              |  SPI+DMA / 定时器 / RMT
+----------------------------------------------+
        |
+----------------------------------------------+
|  WS2812 等可寻址 RGB 灯带硬件                  |
+----------------------------------------------+
```

核心层不含任何 MCU、GPIO、SPI、DMA 或 RTOS 头文件；所有碰硬件的动作收敛在 `rgbled_io_t` 四个回调里，换平台只替换回调实现，核心文件零改动。

## 文件

| 文件 | 作用 |
|---|---|
| `rgbled.h` | 公共类型和 API |
| `rgbled.c` | 平台无关核心 |
| `rgbled_effects.c` | 内置参考特效（static / breathe / rainbow） |
| `rgbled_io.h` | 底层发送回调契约 |
| `rgbled_conf.h` | 编译期配置 |
| `examples/ch32/` | 不含 WCH 头文件的回调绑定示例 |

## 最小接入

```c
#include "rgbled.h"

static rgbled_color_t pixels[16];
static rgbled_io_t io = {
    .write = board_rgb_write,
    .latch = board_rgb_latch,
    .lock = NULL,
    .unlock = NULL,
};
static rgbled_dev_t led;

rgbled_init(&led, pixels, 16, &io, board_context);
rgbled_fill(&led, rgbled_color(8, 0, 20));
rgbled_set_brightness(&led, 180);
rgbled_show(&led);
```

`write` 每次接收一个灯的 3 个通道字节。默认顺序为 GRB，底层回调只负责按目标硬件时序发送这 3 个字节；复位/锁存由 `latch` 完成。对于需要编码为 SPI 波形或 DMA 数据的平台，编码缓冲区由应用适配层自行管理。

## API 参考

单位约定：颜色与 HSV 各分量均为 0~255 的 `uint8_t`（按本库实现，h=255 对应 360°，非 0~65535 域）；`now_ms`/`elapsed_ms` 为毫秒，`RGBLED_LATCH_US` 为微秒。除特别注明外全部返回 `rgbled_result_t`，返回码语义见[结果码](#结果码)。

| 函数 | 参数 | 说明 |
|---|---|---|
| `rgbled_init` | `dev` 句柄；`pixels` 像素缓冲（3 字节/灯，调用方分配）；`count` 灯数 >0；`io` 回调集合（write 必选）；`io_ctx` 透传上下文 | 清零缓冲并绑定回调，复用实例前可再次调用 |
| `rgbled_deinit` | `dev` | 清初始化标志、停止特效，不动缓冲与硬件 |
| `rgbled_set_pixel` | `index` 灯珠序号 0 起；`color` 颜色 0~255 | 只写缓冲，不发送 |
| `rgbled_get_pixel` | `index`；`color` 输出 | 读缓冲中该灯当前颜色 |
| `rgbled_fill` | `color` | 全部灯写同一颜色，不发送 |
| `rgbled_clear` | — | 全部灯写黑色 (0,0,0)，不发送 |
| `rgbled_set_brightness` | `brightness` 0~255，255 不缩放 | 只写状态，show 时才应用 |
| `rgbled_get_brightness` | `brightness` 输出 | 读当前亮度系数 |
| `rgbled_show` | — | 逐灯缩放并发送 3 字节 + latch 复位；失败先解锁再返回 `RGBLED_ERR_IO` |
| `rgbled_get_last_io_error` | — | 返回 `int`：`RGBLED_IO_OK`/`RGBLED_IO_ERROR`，仅在上个 API 返回 `RGBLED_ERR_IO` 后有意义 |
| `rgbled_set_effect` | `effect` 特效回调；`effect_ctx` 上下文 | 注册并启动特效，重复调用即更换 |
| `rgbled_stop_effect` | — | 停止并解绑特效，缓冲保持不变 |
| `rgbled_update` | `now_ms` 当前毫秒时基 | 推进一帧特效；无设置特效时返回 `RGBLED_ERR_STATE` |
| `rgbled_effect_static` | `effect_ctx` 指向 `rgbled_color_t` | 静态显示基准色 |
| `rgbled_effect_breathe` | `effect_ctx` 指向 `rgbled_color_t` | 2 秒三角波呼吸基准色 |
| `rgbled_effect_rainbow` | — | 整带流动彩虹；关 HSV 时返回 `RGBLED_ERR_NOT_SUPPORTED` |
| `rgbled_color` | `r`/`g`/`b` 各 0~255 | 构造颜色，返回 `rgbled_color_t` |
| `rgbled_color_scale` | `scale` 0~255 | 三分量四舍五入缩放，返回颜色 |
| `rgbled_color_mix` | `amount` 0~255，b 的权重 | 线性混合 a→b，返回颜色 |
| `rgbled_color_from_hsv` | `hsv` h/s/v 各 0~255 | 整数 HSV 转 RGB，返回颜色（需 `RGBLED_ENABLE_HSV=1`） |

特效与 `rgbled_update`/`rgbled_set_effect`/`rgbled_stop_effect` 需 `RGBLED_ENABLE_EFFECTS=1`；`rgbled_color_from_hsv` 需 `RGBLED_ENABLE_HSV=1`，关闭后对应声明从编译单元消失。

## 特效

特效函数签名为：

```c
rgbled_result_t effect(rgbled_dev_t *dev, uint32_t now_ms,
                       uint32_t elapsed_ms, void *effect_ctx);
```

应用在主循环或任务中调用：

```c
rgbled_set_effect(&led, rgbled_effect_rainbow, NULL);
for (;;) {
    rgbled_update(&led, board_millis());
    rgbled_show(&led);
}
```

库提供三个小型参考效果：

- `rgbled_effect_static`：`effect_ctx` 指向 `rgbled_color_t`。
- `rgbled_effect_breathe`：`effect_ctx` 指向基准 `rgbled_color_t`，使用 `now_ms` 产生呼吸亮度。
- `rgbled_effect_rainbow`：使用整数 HSV 运算生成彩虹，不需要额外帧缓冲。

用户可以在独立源文件中实现更多效果。效果上下文由用户管理，库不会复制或释放它。`now_ms - last_update_ms` 使用无符号差值，支持计时器回绕。

## 配置

- `RGBLED_COLOR_ORDER`：`RGBLED_ORDER_RGB` 或 `RGBLED_ORDER_GRB`。
- `RGBLED_ENABLE_HSV`：是否编译 HSV 转 RGB 工具及彩虹效果。
- `RGBLED_ENABLE_EFFECTS`：是否编译效果接口。
- `RGBLED_LATCH_US`：默认锁存时间，单位微秒。

## 内存估算

像素区为 `3 × count` 字节；例如 60 颗灯需要 180 字节。`rgbled_dev_t` 只保存指针、计数、状态和回调信息，不创建第二份完整像素数组。底层若使用 SPI 编码、DMA 或 RMT，可由应用按硬件需要额外分配临时缓冲区。

## 移植原则

核心不判断具体 MCU，也不实现 GPIO/SPI 时序。应用只需提供：

1. `write`：发送已经完成通道排序的 RGB 三字节数据。
2. `latch`：等待灯带要求的复位/锁存时间。
3. 可选 `lock`/`unlock`：在 RTOS 或多线程场景保护发送过程。

CH32 示例见 `examples/ch32/rgbled_ch32_port_example.h/.c`。该示例刻意不包含任何 WCH 设备头文件，实际工程将自己的 SPI、DMA、GPIO 或定时器函数包装后绑定即可。位时序参数见归档 [WS2812B.pdf](WS2812B.pdf)（世界半导体 WS2812B，800Kbps NZR 协议）。

## 结果码

| 结果码 | 语义 |
|---|---|
| `RGBLED_OK` | 成功 |
| `RGBLED_ERR_PARAM` | 参数非法（空指针、索引越界） |
| `RGBLED_ERR_NOT_READY` | 设备未初始化 |
| `RGBLED_ERR_IO` | 底层发送失败 |
| `RGBLED_ERR_STATE` | 当前状态不允许该操作（如未设置特效就 `update`） |
| `RGBLED_ERR_NOT_SUPPORTED` | 功能被 conf 裁剪或器件不支持 |

`rgbled_get_last_io_error()` 返回最近一次底层回调的原始返回码（`RGBLED_IO_OK` / `RGBLED_IO_ERROR`，或移植层自定义值），仅在上一个 API 返回 `RGBLED_ERR_IO` 后有意义。

## 线程安全

- 不同 `rgbled_dev_t` 实例之间并发安全：各实例持有独立像素缓冲、状态与回调集合。
- 同一实例的 API 非线程安全，多线程访问需外部互斥；多个实例共享同一物理发送通道时，通过 `lock`/`unlock` 回调保护整帧发送（临界区覆盖全部 `write` 与 `latch`）。
- `rgbled_init()`/`rgbled_deinit()` 与任何运行期调用不支持并发，时序由调用者保证。
- 全部 API 仅限线程上下文，禁止在 ISR 中调用。

## 已知坑

- WS2812 复位时间要求大于 50µs，部分兼容灯珠需 280µs 以上；`RGBLED_LATCH_US` 默认 80µs，不满足时用 `-DRGBLED_LATCH_US=300` 覆盖。
- `rgbled_init()` 会清零调用方像素缓冲（保证首次 `show` 输出全黑）；应用若在 init 前预填了颜色，需在 init 后重新写入。
- 整数 HSV 在 `s=0`（纯白/灰）时次要通道可能比 `v` 低 1（255 显示为 254），属定点近似的固有精度损失。
- 关闭 `RGBLED_ENABLE_HSV` 后 `rgbled_effect_rainbow()` 返回 `RGBLED_ERR_NOT_SUPPORTED`。

## 限制

- 当前版本只支持三通道 RGB，不处理 RGBW。
- 核心 API 不可在 ISR 中调用。
- `rgbled_show()` 逐灯调用 `write`，如果平台要求连续 DMA，应在应用回调中聚合或自行编码。
