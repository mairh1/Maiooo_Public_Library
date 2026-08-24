# CH32 BSP 接入模板

此目录只解决 AW32257 通用驱动与 CH32 板级代码之间的接口边界，不绑定具体 CH32 型号、WCH SDK 版本、I²C 外设、引脚或时钟。`aw32257_ch32_port_example.c` 本身不包含任何 WCH 头文件，可作为 ARM Cortex-M 和 32 位 RISC-V CH32 工程的共同起点。

## 需要由具体 BSP 实现的三个函数

应用先准备一个 `aw32257_ch32_port_context_t`：

```c
static aw32257_ch32_port_context_t charger_port_context =
{
    .board_context = &board_i2c_context,
    .read_reg_8bit_base = board_i2c_read_aw32257,
    .write_reg_8bit_base = board_i2c_write_aw32257,
    .delay_ms = board_delay_ms
};
```

`board_i2c_read_aw32257()` 和 `board_i2c_write_aw32257()` 接收的地址是左移后的 8 位基地址 `0xD4`。常见 WCH 外设库可把它传给“地址+方向”API：

```c
/* 伪代码：名称必须替换成目标 CH32 SDK 的真实 API。 */
I2C_Send7bitAddress(i2c, address_8bit_base, I2C_Direction_Transmitter);
/* 发送寄存器地址；read 时 repeated START 后再使用 Receiver 方向。 */
I2C_Send7bitAddress(i2c, address_8bit_base, I2C_Direction_Receiver);
```

如果 BSP 直接发送线路字节，写阶段使用 `0xD4`，读地址阶段使用 `address_8bit_base | 1U`，即 `0xD5`。不要在核心回调外再次左移。

每个硬件 I²C 实现都必须：

- 用单调时基计算总事务截止时间；
- 对 BUSY、START、ADDR、TXE、BTF、RXNE、STOP 等所有等待设置同一个硬截止时间；
- 正确清除 ACK/错误标志并恢复外设状态；
- 超时时产生 STOP，并按目标 BSP 策略进行有界总线恢复；
- 返回稳定的平台错误码，不在回调内部无限重试；
- 支持寄存器地址写入后 repeated START，或手册允许的 STOP+START 读序列。

`board_delay_ms()` 必须在主循环/任务上下文至少等待指定时长。它用于软件复位后的强制 32 ms I²C 静默期，不能用可能提前返回的普通事件等待替代。

## 初始化

安全参数必须来自目标产品的硬件审查结果：

```c
aw32257_status_t status;
aw32257_device_info_t info;
aw32257_safety_config_t safety =
{
    .max_charge_current = APP_AW32257_SAFE_CURRENT_CODE,
    .max_charge_voltage_mv = APP_AW32257_SAFE_VOLTAGE_MV
};

status = aw32257_ch32_bind_and_power_on_init(
    &charger,
    &charger_port_context,
    APP_AW32257_IO_TIMEOUT_MS,
    &safety,
    &info);
```

调用之前必须已经完成真实硬 POR，并确保没有 bootloader、诊断代码或其他任务提前读取 AW32257。REG06 写入必须是 POR 后首笔器件访问。

## STAT EXTI 延迟处理

STAT 故障脉冲只有约 128 µs，推荐使用 EXTI 捕获。ISR 只清中断源并置位，不执行 I²C：

```c
static volatile uint32_t charger_stat_pending;

void BOARD_STAT_EXTI_IRQHandler(void)
{
    board_clear_stat_exti_flag();
    charger_stat_pending = 1U;
}
```

在主循环或 RTOS 任务中，使用目标平台的临界区原语原子地取走标志，再读取状态：

```c
uint32_t pending;

board_enter_critical();
pending = charger_stat_pending;
charger_stat_pending = 0U;
board_exit_critical();

if (pending != 0U)
{
    aw32257_status_snapshot_t snapshot;

    status = aw32257_read_status(&charger, &snapshot);
    if (status != AW32257_OK)
    {
        app_report_charger_io_error(status,
                                    aw32257_get_last_port_error(&charger));
    }
    else
    {
        app_handle_charger_status(&snapshot);
    }
}
```

上面的 ISR、临界区和错误处理函数名都是占位符，必须替换成目标 CH32 工程的实现。若多个任务或设备共享 I²C，还需要在 BSP/服务层统一串行化总线访问；驱动实例本身不提供锁。
