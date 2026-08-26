# CH32 BSP 接入模板

此目录只解决 MAX17048 通用驱动与 CH32 板级代码之间的接口边界，不绑定具体 CH32 型号、WCH SDK 版本、I2C 外设、引脚或时钟。`max17048_ch32_i2c_port.c` 不包含任何 WCH 头文件，可作为 ARM Cortex-M（CH32F 系列）与 32 位 RISC-V（CH32V/X 系列）工程的共同起点。

## 需要由具体 BSP 提供的内容

**1. 三个板级回调**（装进 `max17048_ch32_adapter_t`，随 `max17048_init()` 作为 `io_ctx` 传入）：

```c
static max17048_ch32_adapter_t g_gauge_adapter =
{
    .board_context = &board_i2c_ctx,        /* 单总线可为 NULL */
    .mem_write     = board_i2c_mem_write,
    .mem_read      = board_i2c_mem_read,
    .io_timeout_ms = 10
};

max17048_init(&g_gauge, &g_gauge_adapter, MAX17048_I2C_ADDR);
```

- `mem_write`：一次事务内发 S + 写地址 + 寄存器地址 + 数据字节 + P，高字节在前；
- `mem_read`：一次事务内发 S + 写地址 + 寄存器地址 + Sr + 读地址 + 收数据 + N + P；
- 回调收到的是**未左移的 7 位地址 0x36**。WCH 标准库按"地址 + 方向"使用的，直接传 `0x36`；按线路字节发送的，写阶段用 `0x6C`、读地址阶段用 `0x6D`。不要再额外左移。

**2. 一个板级延时钩子** `max17048_ch32_board_delay_ms()`（见 `max17048_ch32_i2c_port.h`）：仅模型表加载（`MAX17048_USE_MODEL_TABLE=1`）会走到，用 systick/RTOS 延时实现，不得提前返回。

**3.（可选）并发钩子**：`MAX17048_THREAD_SAFE=1` 时还需实现 `max17048_ch32_board_lock/unlock()`（在 port .c 中已转发到这两个名字）。

## 每个硬件 I2C 实现必须

- 用单调时基对 BUSY、START、ADDR、TXE、BTX、RXNE、STOP 等所有等待设硬截止时间（`io_timeout_ms`）；
- 超时产生 STOP 并做有界总线恢复，返回非 0 错误码；回调内部不无限重试；
- 读序列支持寄存器地址后的 repeated START（手册推荐；STOP+START 也可用）。

## 使用示例

`max17048_ch32_example.c` 演示完整应用流：初始化 + 告警配置 → 1s 周期采样与 RCOMP 温度补偿 → ALRT 中断置标志、线程上下文读取 STATUS 并清除。ALRT 引脚中断服务程序里**只置标志，不碰 I2C**。

低功耗场景提示：CH32 进入停机/待机前，若还需电量计继续跟踪，保持 MAX17048 供电即可（休眠模式约 3µA）；若系统长期不采样，可调 `max17048_sleep_enter()` 降到 <1µA（唤醒需写 CONFIG.SLEEP=0，见驱动 README 已知坑）。
