# CH32 接入说明

这里的适配层不选择具体 WCH SDK 函数，因为“CH32”同时包含 ARM Cortex-M 与 RV32 多个系列，不同 SDK 的 I2C、I2S、超时和状态标志名称并不统一。未经具体型号和工程头文件验证，直接写某一套厂商调用会让所谓通用驱动变成不可验证的示例。

## 1. 加入文件

把以下文件加入 CH32 工程：

- `wm8978.c`
- `wm8978.h`
- `wm8978_regs.h`
- `examples/ch32/wm8978_ch32_i2c_port.c`
- `examples/ch32/wm8978_ch32_i2c_port.h`
- 可选的 `examples/ch32/wm8978_ch32_example.c/.h`

核心驱动层不要包含 `ch32xxxx.h`；具体厂商头只放在 BSP 自己的 `.c` 文件中。

## 2. 实现有界 I2C 写

BSP 实现 `wm8978_ch32_i2c_write7_fn`：

```c
static int32_t board_i2c_write7(void *context,
                                uint8_t address_7bit,
                                const uint8_t *data,
                                uint32_t length,
                                uint32_t timeout_ms)
{
    /* 在这里调用当前 CH32 型号/SDK 已验证的 I2C 主机发送函数。 */
    /* 每个 BUSY/START/ADDR/TXE/BTF/STOP 等待都必须受 timeout_ms 限制。 */
    /* SDK 若要求 8 位地址，只在这里使用 address_7bit << 1。 */
    /* 地址 ACK、两个数据 ACK、STOP 均成功才返回 0。 */
}
```

不要把 `0x34` 再左移，也不要把需要 `0x34` 的 SDK 参数误传成 `0x1A`。用逻辑分析仪看到的写地址字节应为 `0x34`，但高级 SDK 的参数通常仍写 7 位地址 `0x1A`。

驱动不会无限等待硬件标志。若现有 BSP 的 I2C API 没有超时参数，应在 BSP 中增加基于循环计数或单调时基的有限等待，并在超时后按该 CH32 参考手册恢复总线/外设。

NACK、超时或 STOP 阶段错误并不能证明 Codec 一定没有接收控制字。因此底层回调只要返回非 0，核心就进入 `DESYNCHRONIZED`；恢复 CH32 的 I2C 外设/总线后，必须调用 `wm8978_soft_reset()` 并从完整初始化流程重新开始。不要直接重试一个多帧 API 的剩余步骤。

## 3. 提供延时

只有调用 `wm8978_power_up_nonboost_out1()` 时必须提供 `delay_ms`。它要保证至少延时指定毫秒数，不能在 ISR 中调用。

VMID 延时由实际 VMID 电容、电源上升和耦合电容决定。数据手册的约 500 ms 是典型条件，不是所有板卡的固定最小值。

## 4. 音频外设单独配置

控制寄存器写成功不代表音频总线已经匹配。CH32 BSP 还需独立配置：

- MCLK 的来源、频率和启动顺序；
- I2S/PCM 格式、16/20/24/32 位、BCLK/LRC 极性；
- Codec/MCU 谁是主机；
- DMA 缓冲区、IRQ 所有者和欠载处理；
- 启动期间 DACDAT=0，停止前先静音且避免在非零采样点突然断流。

`wm8978_ch32_example.c` 明确假设：I2S、16 位、WM8978 从机、外部 256fs MCLK、48 kHz 滤波系数组、OUT1 耳机、非 1.5x Boost。它只配置 Codec，不配置 CH32 的 I2S/DMA。

## 5. 建议验证顺序

1. 目标工程无告警编译、链接，确认实际使用的 WCH SDK 与工具链。
2. MODE 拉低，100 kHz 控制总线，确认 `0x34` 写地址、地址 ACK，以及两个控制数据字节各自的 ACK。
3. 复位后检查发出的 R4/R6 控制字是否符合预期。
4. 示波器确认 MCLK、BCLK、LRC；再启用 DMA 且先发送全零。
5. 等 VMID 稳定后只解除目标输出静音，从低音量开始。
6. 测试 I2C NACK/超时、Codec 掉电重启、DMA 欠载和重复初始化。
7. 实测上电/掉电爆音、左右声道、采样率、增益与失真。

拿到具体 CH32 型号、SDK 工程和原理图后，才适合把 `board_i2c_write7()`、I2S、DMA 和 IRQ 写成该目标的正式 BSP 实现。
