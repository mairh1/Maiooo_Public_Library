# AW32257 通用 32 位 MCU 驱动

这是一个面向 32 位 MCU 的 AW32257 充电/升压芯片通用驱动。核心使用 ISO C99，不包含 CH32/WCH 头文件，不分配动态内存，不使用递归、浮点、C 位域或全局可变状态。CH32 通过独立 BSP 回调接入，同一核心也可用于其他 MCU。

实现依据本目录中的 [AW32257.pdf](AW32257.pdf)：

- 数据手册版本：AW32257 V1.5，Nov. 2023
- 文件 SHA-256：`845B5A4ADC89922A47A360298B52B579A6F94AE9580101600E2E9FC216294563`
- 驱动版本：1.0.0

> 充电电压、电流和升压参数必须由产品的电池规格、33 mΩ 或实际采样电阻、功率器件、热设计及硬件保护共同确定。本驱动不提供可直接套用的电池安全默认值。

## 文件结构

| 文件 | 用途 |
| --- | --- |
| `aw32257.h` | 公共类型、生命周期、状态及类型化 API |
| `aw32257_io.h` | 固定平台 I/O 函数契约 |
| `port/aw32257_io_template.c` | I/O 移植层桩模板 |
| `aw32257.c` | 与 MCU/SDK 无关的实现 |
| `aw32257_regs.h` | REG00–REG0A 地址、掩码、移位和复位值 |
| `examples/ch32/` | 不绑定具体型号和 SDK 的 CH32 BSP 桥接模板 |
| `tests/test_aw32257.c` | 模拟寄存器、故障注入和访问顺序测试 |
| `tests/run_tests.ps1` | MounRiver ARM/RV32 编译及 RV32 模拟执行脚本 |

## 端口契约

`aw32257_io.h` 定义固定的移植契约；核心实例只保存调用方拥有的 `io_ctx` 和非零 I/O 超时。实现见 `port/aw32257_io_template.c`。

```c
int32_t aw32257_io_read_reg(void *io_ctx, uint8_t address_7bit,
                            uint8_t reg, uint8_t *value,
                            uint32_t timeout_ms);
int32_t aw32257_io_write_reg(void *io_ctx, uint8_t address_7bit,
                             uint8_t reg, uint8_t value,
                             uint32_t timeout_ms);
void aw32257_io_delay_ms(void *io_ctx, uint32_t milliseconds);
```

- 读写回调返回 `0` 表示成功，其他值是平台原始错误码。
- 核心始终传入 7 位地址 `0x6A`。若 WCH API 使用左移后的地址，BSP 层转换为基地址 `0xD4`；在线路读地址阶段 R/W 位为 1，即 `0xD5`。
- 每次 I²C 操作必须在 `timeout_ms` 内完成，禁止无限等待 BUSY、ACK、事件或标志位。
- `delay_ms` 必须至少延时请求时长，不能提前返回。
- 上下文和回调的生命周期必须覆盖 `aw32257_t` 实例。

核心不自动重试。`AW32257_ERR_IO` 时可用 `aw32257_get_last_port_error()` 读取最近一次端口错误。成功的端口调用会把该值更新为 `0`。

## 严格上电初始化

REG06 是 POR 后的一次性安全寄存器：访问其他寄存器会将其锁定，软件复位不能解锁。因此顺序不能改动：

1. 执行真实硬件 POR，且在本驱动之前没有任何主机访问 AW32257。
2. `aw32257_init()` 只保存 `io_ctx`/超时并校验本地参数，不访问 I²C。
3. `aw32257_power_on_init()` 先在本地检查安全参数。
4. 第一笔 I²C 操作直接写 REG06。
5. 回读 REG06 并逐字节核对。
6. 读取 REG03，并按 `(REG03 & 0xF8) == 0x50` 验证 Vendor+Part；修订码 B2:B0 原样返回，因此不会拒绝未来修订。

任何 `aw32257_power_on_init()` 失败都会使已绑定实例进入 `AW32257_LIFECYCLE_POR_REQUIRED`。即使失败发生在本地参数检查、尚未访问总线，也按失效安全契约处理：调用方必须真实断电/上电复位芯片，再重新 `bind/init`。该规则避免应用错误地猜测之前是否已有其他代码访问过器件。

处于 `POR_REQUIRED` 的实例拒绝全部寄存器访问。再次调用 `bind()` 仅表示调用方确认已经完成真实硬 POR；软件无法自行证明电源循环发生过。

## 最小调用示例

安全值必须来自已审查的产品配置，以下宏故意不定义数值：

```c
#include "aw32257.h"

static aw32257_t charger;

static int app_charger_start(void)
{
    void * io_ctx = &board_i2c_context;
    aw32257_safety_config_t safety =
    {
        .max_charge_current = APP_AW32257_SAFE_CURRENT_CODE,
        .max_charge_voltage_mv = APP_AW32257_SAFE_VOLTAGE_MV
    };
    aw32257_device_info_t info;
    aw32257_status_t status;

    status = aw32257_init(&charger, io_ctx, APP_AW32257_IO_TIMEOUT_MS);
    if (status != AW32257_OK)
    {
        return (int)status;
    }

    status = aw32257_power_on_init(&charger, &safety, &info);
    if (status != AW32257_OK)
    {
        /* 真实硬 POR 后才允许重新 bind/init。 */
        return (int)status;
    }

    /* 下面仍须使用产品已验证的精确档位，不要直接复制示例数值。 */
    return 0;
}
```

## 类型化配置与精确档位

驱动不公开任意寄存器写，只提供以下类别的类型化设置：

- 充电使能、终止使能、STAT 输出；
- 充电、高阻、软件请求升压模式；
- 充电电压、快充电流代码、终止电流代码、DPM 电压；
- 电池重充/CTA 终止算法；
- OTG 引脚使能和极性；
- 升压输出电压、频率、开关沿、死区和强制 PWM。

电压 API 只接受能被寄存器精确表示的值，不进行夹紧或取整。非法值返回 `AW32257_ERR_RANGE`，且不会访问总线。

| 项目 | 接受值 |
| --- | --- |
| 充电调节电压 VOREG | 3500–4500 mV，20 mV 步进 |
| 安全电压 VSAFE | 4200–4500 mV，20 mV 步进，仅 POR 初始化 |
| DPM 电压 VSP | 4250–4775 mV，75 mV 步进 |
| 重充阈值 VRCH | 50、100、150、200 mV |
| CTA 窗口 | 8 或 16 个周期 |
| CTA 有效周期数 | 1、2、4、8 |
| CTA 单周期去抖 | 8、16、32、64 ms |
| 升压输出 | 5050、5150、5250、5350 mV |
| 升压频率 | 1500 或 1700 kHz |

数据手册允许 VOREG 原始代码 `0x33–0x3F` 与 `0x32` 一样表示 4.50 V。配置快照会把这些代码解码为 4500 mV；setter 只生成规范代码 `0x32`。

CTA 寄存器允许 `8 × 64 ms = 512 ms`，但手册电气特性给出的终止检测最大值是 256 ms。驱动拒绝有效周期数乘单周期去抖时间大于 256 ms 的组合。

## 电流和 RSNS 限制

快充/安全电流使用 `aw32257_current_code_t`，终止电流使用 `aw32257_term_current_code_t`。这是寄存器档位，而不是与采样电阻无关的 mA 值。

仅当硬件已经确认 `RSNS = 33 mΩ` 时，才可调用：

- `aw32257_current_code_to_ma_33mohm()`；
- `aw32257_termination_current_code_to_ma_33mohm()`。

V1.5 手册 REG04 的 33 mΩ 快充电流表与 RSNS 章节的通用快充公式数值矛盾；终止电流也没有足够明确的任意 RSNS 换算依据。因此驱动按表格保留 16/8 个原始代码，不提供通用 RSNS→mA setter，避免产生看似精确但可能错误的电流。

## RMW、快照和寄存器副作用

普通字段设置执行读—改—写：

- 读失败时不写；
- 非目标位保持原值，包括 REG01/REG05 中复位为 1 的 NA 位；
- 新值与旧值相同时跳过写；
- 写失败不重试；
- REG04 的 RESET 是写 1 触发位，普通电流写会显式清零该位，不能把它当作普通保留位写回。

`aw32257_read_status()` 顺序读取 REG00、REG05、REG09；`aw32257_read_configuration()` 顺序读取全部相关配置寄存器。它们先写局部对象，只有所有读取都成功才更新调用方输出。AW32257 没有跨寄存器硬件锁存，所以快照是顺序采样，不代表同一硬件时刻。

驱动不缓存配置。升压故障会清除 `OPA_MODE`，无电池事件可能让芯片恢复默认配置；驱动不会自动重启升压、自动重写配置或维护可能失真的软件镜像。应用应读取当前状态和配置后，基于产品策略显式处理。

## 模式、软复位和故障

`aw32257_set_mode()` 只请求 REG01 的软件模式；OTG、CD 等外部引脚仍可能影响实际硬件状态。`AW32257_MODE_BOOST` 会同时清除高阻请求并设置升压请求，`AW32257_MODE_HIGH_IMPEDANCE` 则清除升压请求。

`aw32257_soft_reset()`：

- 先读 REG00；检测到正在充电或升压时返回 `AW32257_ERR_STATE`，不写 RESET；
- 发出 RESET 写后总是调用 `delay_ms(..., 32)`；即使端口报告写失败也必须等待，因为芯片可能已经接受命令但 ACK 丢失；
- 32 ms 回调返回前不再访问 I²C；
- 软件复位不会解锁或重置 REG06。

故障枚举保留手册含义。充电故障码 3 只能表示“坏适配器或 VBUS 低于 UVLO”，驱动不会伪造二者的区分。保留故障码也会原样解码。

## 并发、RTOS 与 STAT 中断

核心默认非重入、非 ISR 安全，不提供内部锁。应用必须串行化同一实例以及共享 I²C 总线的访问。

STAT 故障脉冲约 128 µs，建议由 CH32 EXTI 捕获，但 ISR 只能清硬件中断标志并设置事件标志。主循环或任务在安全地取走该标志后，再调用 `aw32257_read_status()`；不要在 ISR 中进行 I²C。参见 [examples/ch32/README.md](examples/ch32/README.md)。

## 测试

在安装了 MounRiver Studio 2 的 Windows PowerShell 中运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\run_tests.ps1
```

测试脚本会：

- 独立编译公共头；
- 以 `-std=c99 -Wall -Wextra -Werror -pedantic` 编译核心及 CH32 桥接示例；
- 用 MounRiver ARM GCC 检查 Cortex-M0 目标；
- 用 WCH RV32 GCC 检查 `RV32IMAC` 目标；
- 用 `riscv32-wch-elf-run --model RV32IMAC` 执行模拟 I²C 单元测试；
- 检查核心是否误引入 `main.h`、CH32/WCH SDK 头文件。

单元测试覆盖 REG06 首笔访问、ID 掩码、POR_REQUIRED、安全回读、所有编码边界、保留位 RMW、无重试、错误传播、模式组合、快照全量提交、故障枚举及 RESET→32 ms 延时顺序。

这些结果只能证明通用源码、模拟测试和相应工具链编译；不能替代具体 CH32 工程的时钟/GPIO/I²C 集成、链接、烧录、逻辑分析仪或实板 V4–V6 验证。

## 许可证

原创驱动源码采用 WTFPL，文件头包含 `SPDX-License-Identifier: WTFPL`。
