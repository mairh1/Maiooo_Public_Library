# INA219 电流/功率监测通用驱动

面向 32 位 MCU 的 TI INA219A/INA219B 零漂移电流监测驱动。核心使用纯 C99，不含任何厂商头文件，不分配动态内存，不使用浮点、递归或全局可变状态；CH32 等平台通过 `ina219_io.h` 移植契约接入。

实现依据本目录中的数据手册 `ina219_ti_zhcsfn9g.pdf`：

- 芯片手册：INA219, **ZHCSFN9G**（2008-08 初版，2015-12 修订，Texas Instruments，中文版）
- 文件 SHA-256：`2C973858ED8290732F2AEC3EFA66230F69FD741C1DD9E888382632BB400E7770`
- 驱动版本：1.0.0

> 默认配置按 **1Ω 采样电阻** 设计：PGA /8（±320mV 满量程）配 Current_LSB = 10µA，校准寄存器恰为 4096（0x1000），电流量程 ±320mA、分辨率 10µA、功率分辨率 200µW，常用换算零舍入损失。换采样电阻时改 `INA219_SHUNT_UOHMS` 或运行时调 `ina219_set_calibration()` 即可。

## 1. 架构（分层设计）

```
+--------------------------------------+
|  应用层                              |  你的代码
+--------------------------------------+
           |  本 API（ina219.h）
+--------------------------------------+
|  驱动核心   ina219.c                 |  纯 C99，零平台代码
|  配置       ina219_conf.h            |  可多实例
|  寄存器定义 ina219_regs.h            |
+--------------------------------------+
           |  3~6 个 io 函数（ina219_io.h）
+--------------------------------------+
|  移植层（你实现）                     |  CH32 / STM32 / ESP32 /
|                                      |  RTOS / Linux / 模拟 I2C
+--------------------------------------+
```

**线程安全（默认）**：不同 `ina219_dev_t` 实例之间并发安全；同一实例的并发访问需外部保护。置 `INA219_THREAD_SAFE=1` 后，驱动在多笔总线访问的 API 前后调用 `ina219_io_lock/unlock()`（由移植层实现，如 RTOS 互斥量）。

**多实例**：A1/A0 引脚组合出 16 个地址（0x40~0x4F），同一条总线上可挂多片 INA219，各用一个句柄分别 `init`；io_ctx 支持多总线路由。

## 2. 移植三步走

1. 复制 `port/ina219_io_template.c` 到工程（可改名），用平台 I2C API 填空 3~4 个函数；或直接用 `examples/ch32/` 的现成桥接。
2. 按需调整 `ina219_conf.h`（采样电阻、Current_LSB、量程、功能开关），或用编译器 `-D` 覆盖。
3. 调用 `ina219_init()`，之后核心文件零改动。

**移植契约的硬性要求**：器件全部寄存器为 16 位，**两字节必须在同一 I2C 事务中传输**（写：S+地址W+寄存器+高字节+低字节+P；读：带 Sr 重复起始），高字节在前。`INA219_USE_TRIGGERED=0` 时 `ina219_io_delay_ms()` 可放空实现。

## 3. 使用示例（CH32）

```c
#include "ina219.h"
#include "ina219_ch32_i2c_port.h"

static ina219_dev_t g_meter;                    /* 句柄由调用者分配，可多实例 */
static ina219_ch32_adapter_t g_adapter =
{
    .board_context = NULL,                      /* 单总线可为 NULL */
    .mem_write     = board_i2c_mem_write,       /* 板级 I2C 回调 */
    .mem_read      = board_i2c_mem_read,
    .io_timeout_ms = 10,
};

void app_init(void)
{
    /* conf 默认：1Ω 采样电阻 / PGA /8（±320mA 量程）/
     * Current_LSB 10µA / 双 12bit 连续转换，init 一次写入 */
    ina219_init(&g_meter, &g_adapter, INA219_I2C_ADDR);
}

void app_periodic_1s(void)
{
    uint16_t bus_mv;  int32_t shunt_uv, current_ua;  uint32_t power_uw;
    bool overflow;

    ina219_read_bus_voltage(&g_meter, &bus_mv);     /* mV */
    ina219_read_shunt_voltage(&g_meter, &shunt_uv); /* µV，正负号=方向 */
    ina219_read_current(&g_meter, &current_ua);     /* µA */
    ina219_read_power(&g_meter, &power_uw);         /* µW */

    if (ina219_is_math_overflow(&g_meter, &overflow) == INA219_OK && overflow)
    {
        /* 电流/功率乘法溢出，数据无意义：检查校准与量程 */
    }
}

/* 低频省电路径：触发单次转换并等待完成（代替连续模式） */
void app_single_shot(void)
{
    ina219_set_mode(&g_meter, INA219_MODE_TRIG_SHUNT_BUS_E);
    ina219_trigger(&g_meter);
    if (ina219_wait_conversion(&g_meter, 150) == INA219_OK)
    {
        app_periodic_1s();
    }
}
```

完整流程见 `examples/ch32/ina219_ch32_example.c`。

## 4. API 参考

单位约定：分流电压 **µV**，总线电压 **mV**，电流 **µA**，功率 **µW**，采样电阻 **µΩ**，Current_LSB **nA**。全部定点运算（电流/功率换算与校准计算内部使用 64 位整数）。set 类函数按硬件档位就近取整并钳位，对应 get 返回实际生效值。

### 4.1 初始化 / 复位

| 函数 | 说明 |
| --- | --- |
| `ina219_init(dev, io_ctx, dev_addr)` | 探测应答 → 按 conf 写配置与校准寄存器（每次上电必须调用：寄存器易失） |
| `ina219_reset(dev)` | 配置寄存器 RST 位全复位；复位后句柄回未初始化态，须重新 init |

### 4.2 量程 / ADC / 模式

| 函数 | 说明 |
| --- | --- |
| `ina219_set_bus_range(dev, range32v)` / `get` | 总线量程 16V / 32V（BRNG 位），分辨率固定 4mV |
| `ina219_set_pga_range(dev, mv)` / `get` | 分流满量程 ±40/±80/±160/±320mV 四档就近取整；量程(mA) = 档位(mV)/R(Ω) |
| `ina219_set_adc(dev, badc, sadc)` / `get` | BADC/SADC 档位（`INA219_ADC_*` 宏：9~12bit 或 12bit×2~128 次平均，转换时间 84µs~68.1ms） |
| `ina219_set_mode(dev, mode)` / `get` | 8 种模式：掉电 / 触发单次×3 / ADC 关 / 连续×3 |

### 4.3 校准

| 函数 | 说明 |
| --- | --- |
| `ina219_set_calibration(dev, shunt_uohms, current_lsb_na)` | 按手册公式 Cal = trunc(0.04096/(LSB×R)) 写校准寄存器（奇数值自动清 void 位）；超界返回 ERR_PARAM |
| `ina219_get_calibration(dev, &shunt, &lsb, &cal)` | 回读器件校准寄存器 + 句柄缓存参数 |

### 4.4 测量读取

| 函数 | 说明 |
| --- | --- |
| `ina219_read_shunt_voltage(dev, &uv)` / `_raw` | µV = (int16)raw × 10（LSB 10µV，各 PGA 档通用） |
| `ina219_read_bus_voltage(dev, &mv)` / `_raw` | mV = (raw >> 3) × 4（寄存器左对齐，标志位自动剔除） |
| `ina219_read_current(dev, &ua)` / `_raw` | µA = raw × Current_LSB / 1000，四舍五入，依赖校准 |
| `ina219_read_power(dev, &uw)` / `_raw` | µW = raw × Current_LSB × 20 / 1000（功率 LSB 恒为电流 LSB 的 20 倍）；`INA219_USE_POWER=0` 裁剪 |
| `ina219_is_conversion_ready(dev, &ready)` | CNVR 位：最近一次转换是否完成 |
| `ina219_is_math_overflow(dev, &ovf)` | OVF 位：电流/功率乘法是否溢出 |

### 4.5 触发转换（`INA219_USE_TRIGGERED=1` 时提供）

| 函数 | 说明 |
| --- | --- |
| `ina219_trigger(dev)` | 整字重写配置寄存器，触发档启动一次新转换 |
| `ina219_wait_conversion(dev, timeout_ms)` | 轮询 CNVR 位（间隔 `INA219_WAIT_POLL_MS`），超时返回 ERR_TIMEOUT |

### 4.6 寄存器级原始访问

| 函数 | 说明 |
| --- | --- |
| `ina219_read_reg / write_reg / update_bits` | 16 位原始读写与位域读-改-写（调试 / 覆盖未封装功能） |

### 4.7 结果码

| 结果码 | 语义 |
| --- | --- |
| `INA219_OK` | 成功 |
| `INA219_ERR_IO` | I2C 通信失败（含器件无应答） |
| `INA219_ERR_PARAM` | 参数非法（空指针、档位/校准超界） |
| `INA219_ERR_NOT_READY` | 未初始化（含复位后未重新 init） |
| `INA219_ERR_NOT_SUPPORTED` | 功能被 conf 裁剪或器件不支持 |
| `INA219_ERR_VERIFY` | 写后回读不一致（`INA219_VERIFY_WRITES=1`） |
| `INA219_ERR_TIMEOUT` | 等待转换完成超时 |

## 5. 配置项（ina219_conf.h，均可 -D 覆盖）

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `INA219_SHUNT_UOHMS` | 1000000（1Ω） | 采样电阻 µΩ，init 时写入校准 |
| `INA219_CURRENT_LSB_NA` | 10000（10µA） | Current_LSB nA；1Ω 下限约 1.25µA |
| `INA219_BUS_RANGE_32V` | 1 | 总线量程 1=32V / 0=16V |
| `INA219_PGA_RANGE_MV` | 320 | 分流满量程档 40/80/160/320 |
| `INA219_BADC` / `INA219_SADC` | 0x3（12bit） | ADC 分辨率/平均档位 |
| `INA219_MODE` | 0x7（连续分流+总线） | 上电工作模式 |
| `INA219_USE_TRIGGERED` | 1 | 触发/等待 API；置 0 后移植层免实现 delay |
| `INA219_USE_POWER` | 1 | 功率 API；置 0 裁剪 |
| `INA219_VERIFY_WRITES` | 0 | 写后回读校验 |
| `INA219_THREAD_SAFE` | 0 | 置 1 要求移植层实现 lock/unlock |
| `INA219_WAIT_POLL_MS` | 1 | 转换等待轮询间隔 |

被裁剪的 API 直接从声明中消失（编译期报错），不保留"返回 NOT_SUPPORTED"的桩。

## 6. 已知坑（来自数据手册）

1. **寄存器全部易失**：软件复位或掉电后回到 POR 默认，每次上电必须重新 init（写配置 + 校准）。
2. **总线电压寄存器左对齐**：bit15:3 是电压值，换算前必须右移 3 位；bit2 保留、bit1 CNVR、bit0 OVF。
3. **CNVR 清除条件**：写 MODE 字段、读功率寄存器都会硬件清除 CNVR——`ina219_wait_conversion()` 期间不要并发读功率。
4. **未校准时电流/功率恒 0**：校准寄存器 POR 为 0，电流与功率寄存器不更新；`ina219_init()` 已自动写入校准。
5. **校准寄存器 bit0 是 void 位**：恒读 0，写 1 无效。驱动写入前自动清零（奇数值等效），回读一致。
6. **校准 15 位上限**：Cal ≤ 0x7FFF。1Ω 电阻下 Current_LSB 下限约 1.25µA；需要更小 LSB 时换更小采样电阻。
7. **OVF 置位时电流/功率数据可能无意义**：分流电压与总线电压本身仍有效；应增大 Current_LSB 或降量程。
8. **写后 4µs 生效窗口**：SCL > 1MHz 时写寄存器后 4µs 内不要回读同一寄存器；400kHz 及以下无影响。
9. **读指针保持**：器件在读事务后保持寄存器指针，本驱动每次读前重写指针，移植层无需关心；器件不支持跨寄存器自增读。
10. **28ms 接口超时**：SMBus 兼容的时钟超时机制，I2C 主机每字节间隔不应超过 28ms。
11. **总线电压测的是 IN- 引脚（负载侧）**：高压侧采样时负载电压不含采样电阻压降。
12. **1Ω 采样电阻的功耗与压降**：满量程 320mA 时电阻功耗 0.1W、压降 320mV——确认电阻功率额定与负载电压裕量；大电流场景应换毫欧级采样电阻并重设校准。
13. **触发模式读数时机**：单次转换完成后数据才有效，用 CNVR/`ina219_wait_conversion()` 确认，别触发后立刻读。

## 7. 验证

`tests/run_tests.ps1` 一键门禁（需 MounRiver 工具链，路径可 `-ArmToolchainBin`/`-RvToolchainBin` 或环境变量 `INA219_ARM_TOOLCHAIN_BIN`/`INA219_RV_TOOLCHAIN_BIN` 指定）：

- 数据手册 PDF SHA-256 校验；
- 核心五件套平台 include 边界扫描；
- ARM Cortex-M0 + RV32IMAC 双工具链 `-Wall -Wextra -Werror -pedantic` 编译（核心 + CH32 桥接 + 示例）；
- RV32 模拟器 mock-I2C 单元测试（默认与 `VERIFY_WRITES=1` 两套配置，153 项检查，换算锚点取自数据手册 Table 8：2mΩ/1mA 校准 → 10A/119.8W）；
- `THREAD_SAFE=1`、`USE_TRIGGERED=0 + USE_POWER=0` 裁剪配置编译检查。

## 8. 文件清单

| 文件 | 说明 |
| --- | --- |
| `ina219.h` / `ina219.c` | 公共 API 与驱动核心（纯 C99） |
| `ina219_conf.h` | 全部配置宏（单头文件裁剪） |
| `ina219_regs.h` | 寄存器地址 / 位定义 / 档位宏 |
| `ina219_io.h` | 移植契约（3 必选 + 条件必选 delay + 可选锁） |
| `port/ina219_io_template.c` | 移植模板（STM32 HAL / ESP-IDF / 伪代码示例） |
| `examples/ch32/` | CH32 桥接与完整使用示例 |
| `tests/` | mock 单元测试与门禁脚本 |
| `ina219_ti_zhcsfn9g.pdf` | TI 数据手册存档 |
