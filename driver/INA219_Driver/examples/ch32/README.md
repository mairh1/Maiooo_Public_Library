# CH32 BSP 接入模板

此目录只解决 INA219 通用驱动与 CH32 板级代码之间的接口边界，不绑定具体 CH32 型号、WCH SDK 版本、I2C 外设、引脚或时钟。`ina219_ch32_i2c_port.c` 不包含任何 WCH 头文件，可作为 ARM Cortex-M（CH32F 系列）与 32 位 RISC-V（CH32V/X 系列）工程的共同起点。

## 需要由具体 BSP 提供的内容

**1. 三个板级回调**（装进 `ina219_ch32_adapter_t`，随 `ina219_init()` 作为 `io_ctx` 传入）：

```c
static ina219_ch32_adapter_t g_meter_adapter =
{
    .board_context = &board_i2c_ctx,        /* 单总线可为 NULL */
    .mem_write     = board_i2c_mem_write,
    .mem_read      = board_i2c_mem_read,
    .io_timeout_ms = 10
};

ina219_init(&g_meter, &g_meter_adapter, INA219_I2C_ADDR);
```

- `mem_write`：一次事务内发 S + 写地址 + 寄存器地址 + 数据字节 + P，高字节在前；
- `mem_read`：一次事务内发 S + 写地址 + 寄存器地址 + Sr + 读地址 + 收数据 + N + P；
- 回调收到的是**未左移的 7 位地址**（默认 `INA219_I2C_ADDR` = 0x40，A1/A0 引脚其它组合为 0x41~0x4F）。WCH 标准库按"地址 + 方向"使用的，直接传 7 位值；按线路字节发送的，写阶段用 `0x80`、读地址阶段用 `0x81`（0x40 左移后）。不要再额外左移。

**2. 一个板级延时钩子** `ina219_ch32_board_delay_ms()`（见 `ina219_ch32_i2c_port.h`）：仅 `ina219_wait_conversion()` 轮询会走到（`INA219_USE_TRIGGERED=1` 时），用 systick/RTOS 延时实现，不得提前返回；只用连续转换且不等待的应用可在 conf 裁剪该开关。

**3.（可选）并发钩子**：`INA219_THREAD_SAFE=1` 时还需实现 `ina219_ch32_board_lock/unlock()`（在 port .c 中已转发到这两个名字）。

## 每个硬件 I2C 实现必须

- 用单调时基对 BUSY、START、ADDR、TXE、BTX、RXNE、STOP 等所有等待设硬截止时间（`io_timeout_ms`）；
- 超时产生 STOP 并做有界总线恢复，返回非 0 错误码；回调内部不无限重试；
- 读序列支持寄存器地址后的 repeated START（手册推荐；STOP+START 也可用）。

## 使用示例

`ina219_ch32_example.c` 演示完整应用流：适配器接线 + `ina219_init()`（conf 默认 1Ω 采样电阻 / ±320mA 量程 / 10µA 电流分辨率 / 连续转换）→ 1Hz 周期采样与溢出检查 → 触发单次转换的省电路径 → 停机前切掉电档。INA219 没有中断引脚，无需 ISR 处理。

低功耗场景提示：连续转换模式下器件自身耗电约 0.7mA，电池设备建议改用触发单次模式或采样间隙 `INA219_MODE_POWER_DOWN_E`（典型 5µA）；器件 VS 完全掉电后寄存器全部丢失，唤醒必须重新 `ina219_init()`。
