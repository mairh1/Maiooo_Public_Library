# WM8978 通用驱动

这是依据归档在项目根 [datasheet/WM8978_C323850.pdf](../../datasheet/WM8978_C323850.pdf)（WM8978 Production Data，Rev 4.5，2011-10）编写的可移植 C99 驱动。核心层不包含任何 CH32/WCH 头文件，可用于 ARM Cortex-M、RV32 以及其他 32 位单片机；CH32 只在移植层实现固定 IO 契约并绑定具体 I2C、GPIO、延时和音频外设。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `wm8978_regs.h` | 52 个有效寄存器、复位值、位域与协议常量 |
| `wm8978.h` | 平台无关公共 API、句柄与固定 IO 契约引用 |
| `wm8978_io.h` | 固定控制帧写入与毫秒延时移植契约 |
| `wm8978_conf.h` | 编译期配置入口（当前版本无功能裁剪项，五件套固定文件与未来配置入口） |
| `port/wm8978_io_template.c` | 移植层实现模板 |
| `wm8978.c` | 写控制字、影子寄存器、保留位保护及常用高层功能 |
| `examples/ch32/wm8978_ch32_i2c_port.*` | 不绑定具体 WCH SDK 的 CH32 2-wire 适配层 |
| `examples/ch32/wm8978_ch32_example.*` | I2S、16 位、Codec 从机、48 kHz 系数组的明确示例 |
| `docs/datasheet-notes.md` | 数据手册页码依据、内部矛盾及实现取舍 |
| `tests/` | 假总线行为测试、RV32I 解释执行器、公共头和多翻译单元编译夹具 |
| [../../datasheet/WM8978_C323850.pdf](../../datasheet/WM8978_C323850.pdf) | 芯片手册归档：项目根 `datasheet/` 目录，WM8978 Production Data，Rev 4.5（2011-10） |

## 分层架构

```
+----------------------------------------------------+
|  应用层                                             |  用户代码 / examples/ch32
+----------------------------------------------------+
          |  公共 API（wm8978.h）+ 统一结果码 wm8978_status_t
+----------------------------------------------------+
|  驱动核心    wm8978.c                               |  纯 C99，零平台代码
|  配置入口    wm8978_conf.h（当前无功能裁剪项）       |
|  寄存器定义  wm8978_regs.h                          |
+----------------------------------------------------+
          |  移植契约（wm8978_io.h）：2 个固定 io 函数
          |    wm8978_io_write_control()  控制帧写入（必选）
          |    wm8978_io_delay_ms()       毫秒延时（上电类 API 必选）
+----------------------------------------------------+
|  移植层（你实现）                                    |  port/wm8978_io_template.c
|                                                     |  CH32 / STM32 / ESP32 / ...
+----------------------------------------------------+
          |
硬件（2/3 线控制接口、I2S 音频数据接口、MCLK 与电源）
```

核心层只调用 `wm8978_io.h` 的契约函数，不出现任何厂商符号；`wm8978_conf.h` 当前没有配置宏——控制事务超时（`wm8978_bind()` 传入）与 VMID 稳定延时（随板级电容实测）都是运行期参数，因此不存在可裁剪的编译期开关，该文件仅作为五件套固定文件与未来配置入口保留。

## 驱动边界

驱动核心负责：

- 把 7 位寄存器地址与 9 位数据打包为 16 位控制字；
- 校验 52 个非连续有效地址，拒绝保留地址；
- 维护写式控制接口所必需的软件影子寄存器；
- 在总线成功后才提交影子值，并清除 `VU/UPDATE` 非锁存位及按一次性请求处理的 `NFU` 位；
- 保护保留位，不允许它们偏离数据手册复位值；
- 提供音频格式、时钟分频、PLL 原始参数、立体声音量和上下电顺序 API；
- 把总线超时/NACK/错误映射为 `WM8978_ERR_IO`，原始错误由 `wm8978_get_last_port_error()` 获取，并把实例锁定为 `DESYNCHRONIZED`，防止继续使用可能失真的影子。

板级 BSP 负责：

- MODE 管脚电平、I2C/SPI/GPIO 实例和引脚；
- MCLK 来源与频率；
- CH32 的 I2S/PCM、DMA、IRQ 和音频数据流；
- 电源控制、VMID 电容对应的稳定时间以及外部功放静音；
- 控制总线每一步的有限超时。

核心不动态分配内存、不递归、不包含厂商寄存器、不在 ISR 中阻塞。`wm8978_t` 由调用者持有；同一实例不是可重入对象，任务与中断之间必须由上层串行化。

## 线程安全

- 不同 `wm8978_t` 实例之间并发安全：全部可变状态都在实例内部，核心没有任何可变静态变量，`io_ctx` 支持各实例绑定不同总线。
- 同一实例不是可重入对象：任务与中断之间必须由上层串行化（核心不会在 ISR 中调用或阻塞）。
- `wm8978_bind()`、软复位等初始化/同步操作与运行期调用的并发不受支持，由调用者保证时序。

## 控制接口接线

| MODE | 控制模式 | `wm8978_io_write_control()` 应完成的事务 |
| --- | --- | --- |
| 低 | 2-wire | 向 7 位地址 `0x1A` 写连续两个控制字节并检查全部 ACK |
| 高 | 3-wire | CSB 有效期间按 MSB first 移出两个字节，再满足 CSB 上升沿锁存时序 |

`0x1A` 是未左移的 7 位地址；线上写地址字节是 `0x34`。有的 SDK 参数接收 `0x1A`，有的接收左移后的 `0x34`，只能在板级适配器中按该 SDK 的文档处理一次，核心不会移位器件地址。

2-wire 数据手册上限为 526 kHz。实际工程建议使用标准 100 kHz 或 400 kHz，并验证上拉、电容和时序余量。

音频数据接口与控制接口相互独立：`DACDAT/ADCDAT/LRC/BCLK` 不能当作寄存器控制总线；MCLK 也必须由板级提供或正确配置 PLL。

## 最小使用流程

下面的 `board_i2c_write7()` 和 `board_delay_ms()` 由具体 CH32 工程实现。前者必须使用有限超时，且只有在地址、两个数据字节和 STOP 都成功后才返回 0。

```c
#include "wm8978.h"
#include "examples/ch32/wm8978_ch32_i2c_port.h"

static wm8978_t codec;
static wm8978_ch32_i2c_adapter_t codec_adapter;

static int32_t board_i2c_write7(void *ctx,
                                uint8_t address_7bit,
                                const uint8_t *data,
                                uint32_t length,
                                uint32_t timeout_ms);
static void board_delay_ms(void *ctx, uint32_t milliseconds);

wm8978_status_t codec_init(void *board_i2c, uint32_t vmid_settle_ms)
{
    wm8978_status_t status;
    wm8978_audio_interface_config_t audio = {
        WM8978_AUDIO_FORMAT_I2S,
        WM8978_WORD_LENGTH_16_BITS,
        false, false, false, false, false, false
    };
    wm8978_clock_config_t clock = {
        false,                 /* WM8978 is slave */
        false,                 /* use external MCLK */
        WM8978_MCLK_DIV_1,
        WM8978_BCLK_DIV_1
    };

    codec_adapter.i2c_write7 = board_i2c_write7;
    codec_adapter.delay_ms = board_delay_ms;
    codec_adapter.board_context = board_i2c;

    status = wm8978_ch32_i2c_bind(&codec, &codec_adapter, 10U);
    if (status != WM8978_OK) return status;
    status = wm8978_soft_reset(&codec);
    if (status != WM8978_OK) return status;

    status = wm8978_configure_clock(&codec, &clock);
    if (status != WM8978_OK) return status;
    status = wm8978_configure_audio_interface(&codec, &audio);
    if (status != WM8978_OK) return status;
    status = wm8978_set_filter_sample_rate(&codec,
                                            WM8978_FILTER_SR_48_KHZ);
    if (status != WM8978_OK) return status;

    /* 先让 CH32 的 DACDAT 保持 0，再执行防爆音上电；返回后仍是静音状态。 */
    return wm8978_power_up_nonboost_out1(&codec,
                                          WM8978_VMID_5K,
                                          vmid_settle_ms);
}
```

`vmid_settle_ms` 不能写成与所有板卡都相同的魔数。数据手册给出的约 500 ms 只是特定 VMID 电容、耦合电容及电源上升条件下的典型值；应从原理图参数和板级测试确定。

## 影子寄存器规则

WM8978 数据手册只定义写控制流程，没有普通寄存器读回事务，所以字段更新不能先读芯片再改位。本驱动采用以下规则：

1. `wm8978_bind(device, io_ctx, timeout_ms)` 后实例只有 `BOUND` 状态，普通写入会返回 `WM8978_ERR_NOT_READY`。固定契约实现 `wm8978_io_write_control()` 与 `wm8978_io_delay_ms()`，核心不再接收运行期回调结构体。
2. 优先调用 `wm8978_soft_reset()`，写 R0 成功后载入全部数据手册复位值。
3. 只有在板级已经确认 POR 完成时，才可用 `wm8978_assume_power_on_reset()` 跳过总线复位写入。
4. 每次控制帧成功后才修改影子；但超时或帧末错误可能发生在芯片已经接收写入之后，所以任何端口错误都会进入 `DESYNCHRONIZED`。
5. 失步状态下，普通写入、位更新和影子读取均返回 `WM8978_ERR_DESYNCHRONIZED`。先按目标 CH32 手册恢复控制总线，再调用 `wm8978_soft_reset()`；只有板级证据确认 Codec 已重新 POR 时，才可调用 `wm8978_assume_power_on_reset()`。
6. `DACVU/ADCVU/INPPGAUPDATE/HPVU/SPKVU` 是非锁存触发位；`NFU` 的锁存行为在本版 PDF 中没有同等级明确说明。为避免后续原始更新意外重复提交 Notch 系数，驱动也把 `NFU` 当作一次性请求，成功后不留在影子中。
7. 掉电、其他主机改写寄存器，或另一控制器写 R0 后，必须重新同步，通常由当前实例重新调用 `wm8978_soft_reset()`。WM8978 没有独立 RESET 引脚。

`wm8978_get_shadow_register()` 返回的是软件影子，不是硬件读值；R0 是非锁存命令，因此没有可返回的影子值。

## 常用配置

### 音频格式

`wm8978_configure_audio_interface()` 支持右对齐、左对齐、I2S 和 DSP/PCM，以及 16/20/24/32 位、BCLK/LRC 极性和左右交换。R4[7] 在右/左对齐和 I2S 下由 `invert_lrc` 表示 LRC 极性，在 DSP/PCM 下则由 `dsp_mode_b` 选择模式 A/B；驱动拒绝把两种语义混用。数据手册说明右对齐模式最多 24 位，驱动会拒绝“右对齐 + 32 位”，避免芯片静默按 24 位运行。

### 采样率与时钟

`wm8978_set_filter_sample_rate()` 只写 R7 `SR`，作用是选择数字滤波器系数和 ALC 时间尺度，不会产生采样时钟。真实采样率仍由 MCLK/PLL、MCLKDIV、主从模式和 BCLK/LRC 决定。

44.1/22.05/11.025 kHz 没有独立 SR 编码，按数据手册“选择最接近值”分别使用 48/24/12 kHz 系数组，但板级时钟仍必须准确地产生 44.1 kHz 系列采样率。

PLL 安全顺序：

使用 PLL 前必须先由硬件设计确认 `DCVDD >= 1.9 V`；驱动无法测量电源，也无法读回锁定状态。

1. `wm8978_configure_clock(... use_pll=false ...)` 先选择 MCLK；
2. 先使 VMID 非 0；
3. `wm8978_configure_pll()` 写 N/K；
4. `wm8978_set_pll_enabled(true)`；
5. 由板级验证 PLL 输出频率与稳定条件后，`wm8978_configure_clock(... use_pll=true ...)`；
6. 退出时先切回 MCLK，再 `wm8978_set_pll_enabled(false)`。

每次切换 R6 `CLKSEL` 后，原时钟源至少还要继续提供一个下降沿，再停掉原时钟。驱动负责寄存器顺序，具体时钟边沿由 CH32 时钟/BSP 保证。

PLL 中 `f1 = MCLK / (PLLPRESCALE ? 2 : 1)`，公式为 `N=floor(f2/f1)`、`K=floor(2^24 * (f2/f1-N))`。数据手册建议把 `f2` 设计在约 90..100 MHz，并让 N 尽量接近 8；最终输出还要经过 R6 `MCLKDIV`。本版 PDF 对 N 的端点表述及一个 K 示例末位存在内部差异，所以 API 保守接受 `N=6..12`，K 由调用者依据目标时钟计算并在板上验证。

### 音量编码

各音量/增益码点与物理单位（dB）的对应关系整理进下文 [API 参考](#api-参考) 的「音量与增益码点单位」表。

立体声 API 总是先写左通道 `VU=0`，再写右通道 `VU=1`，使左右同步更新。

### 原始寄存器访问

特殊 EQ、ALC、Notch、Jack Detect 或路由功能可用 `wm8978_update_bits()` 与 `wm8978_regs.h` 位域组合。例如：

```c
wm8978_status_t status;

status = wm8978_update_bits(&codec,
                             WM8978_REG_LEFT_MIXER,
                             WM8978_MIX_DAC2MIX,
                             WM8978_MIX_DAC2MIX);
```

原始 API 仍会拒绝保留地址、超出 9 位的数据、修改保留位的掩码或完整值。它不会替调用者判断每个可写位域中的“保留编码”，也不会强制 PLL、电源或输出路径的全部顺序；使用高级功能时仍需对照规格书。一个必须保护影子一致性的例外是：ADC 或 DAC 开启时，驱动会拒绝改变 R18 `EQ3DMODE`，因为芯片会拒绝该位变化。

双声道更新、PLL 配置、全输出静音和上下电都是多控制帧操作，不具备原子性。任意一帧报错后都不要直接重试剩余步骤；恢复总线并软复位 Codec，再从完整初始化流程开始。

## API 参考

除三个查询函数外，全部公共函数返回 `wm8978_status_t`；`0` 为成功，负值为对应错误。多帧 API 不具备原子性，任意一帧报错后先恢复总线并软复位，不要直接重试剩余步骤。

### 初始化与查询

| 函数 | 返回 | 说明 |
| --- | --- | --- |
| `wm8978_bind(device, io_ctx, io_timeout_ms)` | status | 绑定 io 上下文与事务超时（毫秒，非零），进入 `BOUND`；不访问总线 |
| `wm8978_soft_reset(device)` | status | 写 R0，影子同步为复位默认值，进入 `READY` |
| `wm8978_assume_power_on_reset(device)` | status | 不写总线直接装载复位默认值；仅板级确认 POR 后可用 |
| `wm8978_get_lifecycle(device)` | lifecycle | 读取生命周期（device 为 NULL 返回 `UNBOUND`） |
| `wm8978_get_last_port_error(device)` | int32_t | 最近一次 io 层原始返回值（0 为成功；NULL 返回 0） |
| `wm8978_register_is_valid(addr)` | bool | 地址是否属于 52 个有效寄存器 |
| `wm8978_pack_control_frame(addr, value, frame[2])` | status | 寄存器地址/9 位值打包为 B15:B8 与 B7:B0 两个字节 |

### 寄存器级访问

| 函数 | 返回 | 说明 |
| --- | --- | --- |
| `wm8978_get_shadow_register(device, addr, &value)` | status | 读软件影子（非硬件读回）；R0 无影子 |
| `wm8978_write_register(device, addr, value)` | status | 写完整 9 位值并提交影子；写 R0 即软复位 |
| `wm8978_update_bits(device, addr, mask, field_value)` | status | 基于影子的位域更新；无变化且无触发位时不访问总线 |

### 音频接口 / 时钟 / PLL

| 函数 | 返回 | 说明 |
| --- | --- | --- |
| `wm8978_configure_audio_interface(device, &config)` | status | 一次写 R4：格式、字长、BCLK/LRC 极性、左右交换、单声道 |
| `wm8978_configure_clock(device, &config)` | status | 更新 R6：主从、时钟源、MCLKDIV、BCLKDIV（含 PLL 顺序检查） |
| `wm8978_set_filter_sample_rate(device, group)` | status | 写 R7 SR 滤波器系数组（不产生采样时钟） |
| `wm8978_configure_pll(device, &config)` | status | 写 R36-R39 原始 N/K；N 限 6..12，K 为 24 位小数 |
| `wm8978_set_pll_enabled(device, enabled)` | status | 带顺序检查地置位/清除 R1 `PLLEN` |

### 音量与增益

| 函数 | 返回 | 说明 |
| --- | --- | --- |
| `wm8978_set_dac_digital_volume(device, left_code, right_code)` | status | R11/R12 数字音量码点（单位见下表），左右同步 |
| `wm8978_set_adc_digital_volume(device, left_code, right_code)` | status | R15/R16 数字音量码点，左右同步 |
| `wm8978_set_input_pga(device, &config)` | status | R45/R46 输入 PGA 增益、静音、过零，左右同步 |
| `wm8978_set_output_volume(device, output, &config)` | status | R52/R53（OUT1）或 R54/R55（OUT2）音量、静音、过零 |
| `wm8978_mute_analogue_outputs(device, mute)` | status | 静音/解除 R52-R57 全部模拟输出 MUTE |

### 上下电

| 函数 | 返回 | 说明 |
| --- | --- | --- |
| `wm8978_power_up_nonboost_out1(device, vmid, vmid_settle_ms)` | status | 手册第 83 页非 1.5x Boost OUT1 上电序列；返回时保持静音 |
| `wm8978_power_down(device)` | status | 先静音再依次写 R1/R2/R3 = 0 |

### 音量与增益码点单位

| 码点 | 范围 | 步进 | 备注 |
| --- | --- | --- | --- |
| DAC/ADC 数字音量 0 | 数字静音 | — | 码点 0 等效数字静音 |
| DAC/ADC 数字音量 1..255 | -127 dB .. 0 dB | 0.5 dB/码点 | 255 为 0 dB |
| 输入 PGA 0..63 | -12 dB .. +35.25 dB | 0.75 dB/码点 | R45/R46 |
| OUT1/OUT2 0..63 | -57 dB .. +6 dB | 1 dB/码点 | 57 为 0 dB；`vmid_settle_ms` 单位为毫秒、须板级实测 |

### 结果码（wm8978_status_t）

| 结果码 | 语义 |
| --- | --- |
| `WM8978_OK` | 成功 |
| `WM8978_ERR_NULL_POINTER` | 必需指针参数为 NULL |
| `WM8978_ERR_INVALID_ARGUMENT` | 参数取值非法（超时 0、掩码与值不匹配、DSP/LRC 语义混用等） |
| `WM8978_ERR_NOT_BOUND` | 实例尚未 `wm8978_bind()` |
| `WM8978_ERR_NOT_READY` | 已绑定但影子未同步，须先软复位或确认 POR |
| `WM8978_ERR_RANGE` | 数值超出编码范围（码点、分频档位、PLL N/K） |
| `WM8978_ERR_INVALID_REGISTER` | 地址不在有效集合，或该操作不接受此地址 |
| `WM8978_ERR_RESERVED_BITS` | 保留位偏离数据手册复位值 |
| `WM8978_ERR_IO` | 控制帧传输失败；原始错误经 `wm8978_get_last_port_error()` 获取 |
| `WM8978_ERR_STATE` | 器件状态不允许（EQ3DMODE 运行期切换、PLL 顺序、上电前置条件） |
| `WM8978_ERR_NO_SHADOW` | 该地址没有影子值（R0 非锁存） |
| `WM8978_ERR_DELAY_REQUIRED` | 需要板级延时支持的场合（当前核心未使用，保留语义） |
| `WM8978_ERR_UNSUPPORTED` | 不支持的组合（右对齐 + 32 位字长） |
| `WM8978_ERR_DESYNCHRONIZED` | 端口错误后失步，须经确认的复位恢复，不得直接重试 |

## 防爆音上下电

`wm8978_power_up_nonboost_out1()` 实现数据手册第 83 页“不使用 1.5x Boost”的寄存器顺序：

1. 静音全部明确的模拟输出；
2. 打开左右 Mixer 与 DAC；
3. 打开 BUFIOEN 与所选 VMID；
4. 调用板级延时等待 VMID；
5. 打开 BIASEN；
6. 打开左右 OUT1；
7. 保持静音返回，等 CH32 音频数据与时钟稳定后再显式解除目标输出静音。

安全静音会同时清除 OUT1/OUT2 的 `ZC`，避免 `SLOWCLKEN=0` 且没有零交叉时静音迟迟不生效。需要零交叉音量更新时，可在完成上电后通过 `wm8978_set_output_volume()` 重新启用。

调用前必须确保外部电源稳定、让 DACDAT 保持 0，并保证影子中的 R1/R2/R3 全为 0；否则函数返回 `WM8978_ERR_STATE`，不会悄悄保留 `SLEEP` 或其他既有电源使能。该函数只面向非 Boost OUT1；Boost/扬声器拓扑依赖未使用输出和板级电路，应该由项目按第 84 页单独组合。

`wm8978_power_down()` 先静音，再依次写 R1、R2、R3 为 0。停止非零 DACDAT、等待耦合电容放电及移除外部电源仍由 BSP/应用负责。

## 已知规格书矛盾

Rev 4.5 的 R10[6] `SOFTMUTE` 极性在第 48 页与第 95 页互相矛盾。因此本驱动只公开 `WM8978_R10_SOFTMUTE_RAW` 原始位，不提供会假装知道极性的 `soft_mute(bool)`。安全流程使用定义一致的数字音量 0 以及 R52-R57 的模拟输出 MUTE 位。详见 `docs/datasheet-notes.md`。

## 验证边界

仓库测试覆盖控制字打包、默认影子、无效地址/保留位、负枚举、失败失步、非锁存更新位、音频/PLL/EQ 状态约束、双声道第二帧失败和上电每一写入阶段的故障注入。脚本把纯平台无关行为测试编译成基础 RV32I ELF 并解释执行 `main()`，同时使用 WCH 提供的 ARM 与 RV32 交叉编译器做严格 C99 编译、公共头 C/C++ 检查及多翻译单元链接。

这里的 RV32I 解释执行只验证 CPU 端控制逻辑和假端口断言，不模拟 CH32 外设或 WM8978 电气行为，也不等于具体芯片目标测试。

由于当前没有给出具体 CH32 型号、WCH SDK 版本、MODE 接线、MCLK、I2S 实例/引脚、DMA/IRQ 和板卡，本交付不声称已经完成具体目标编译、下载或板级音频验证。接入实际工程后至少还要完成：

- 对应 CH32 SDK 的完整编译与链接；
- 逻辑分析仪确认 `0x1A` 地址、ACK、两字节控制字和超时恢复；
- 确认 MCLK/BCLK/LRC 频率、相位、字宽及主从关系；
- 实测 VMID 稳定时间、上电/掉电爆音、左右声道与音量；
- 对目标扬声器/耳机/麦克风路由做功能和电气验收。

更具体的 CH32 接入说明见 [`examples/ch32/README.md`](examples/ch32/README.md)。
