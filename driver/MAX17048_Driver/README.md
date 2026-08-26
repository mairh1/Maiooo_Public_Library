# MAX17048/MAX17049 电量计通用驱动

面向 32 位 MCU 的 ModelGauge 锂电电量计驱动。核心使用纯 C99，不含任何厂商头文件，不分配动态内存，不使用浮点、递归或全局可变状态；CH32 等平台通过 `max17048_io.h` 移植契约接入。

实现依据本目录中的数据手册 `C2682616_电池管理_MAX17048G+T10_规格书_电池管理_MAX17048G+T10_英文规格书.PDF`：

- 芯片手册：MAX17048/MAX17049, 19-6171; **Rev 7; 11/16**（Analog Devices/Maxim）
- 采购料号：MAX17048G+T10（TDFN-8，1 节）
- 文件 SHA-256：`70DC8EEF0E012276DCDC58B6DCE64AF08258304BCF865CEACE64E856B8029330`
- 驱动版本：1.0.0

> 电量计的准确性依赖电池模型与 RCOMP 温度补偿参数。POR 默认 ROM 模型只对部分电池表现良好；定制模型与 RCOMP0/TempCo 参数应向厂商申请或对实际电池表征获得。

## 1. 架构（仿照 FatFS 分层）

```
+--------------------------------------+
|  应用层                              |  你的代码
+--------------------------------------+
           |  本 API（max17048.h）
+--------------------------------------+
|  驱动核心   max17048.c               |  纯 C99，零平台代码
|  配置       max17048_conf.h          |  可多实例
|  寄存器定义 max17048_regs.h          |
+--------------------------------------+
           |  4~6 个 io 函数（max17048_io.h）
+--------------------------------------+
|  移植层（你实现）                     |  CH32 / STM32 / ESP32 /
|                                      |  RTOS / Linux / 模拟 I2C
+--------------------------------------+
```

**线程安全（默认）**：不同 `max17048_dev_t` 实例之间并发安全；同一实例的并发访问需外部保护。置 `MAX17048_THREAD_SAFE=1` 后，驱动在多笔总线访问的 API 前后调用 `max17048_io_lock/unlock()`（由移植层实现，如 RTOS 互斥量）。

## 2. 移植三步走

1. 复制 `port/max17048_io_template.c` 到工程（可改名），用平台 I2C API 填空 4 个函数；或直接用 `examples/ch32/` 的现成桥接。
2. 按需调整 `max17048_conf.h`（变体 48/49、功能开关），或用编译器 `-D` 覆盖。
3. 调用 `max17048_init()`，之后核心文件零改动。

**移植契约的硬性要求**：器件全部寄存器为 16 位，**两个字节必须在同一 I2C 事务中传输**（写：S+地址W+寄存器+高字节+低字节+P；读：带 Sr 重复起始），高字节在前。8 位写无效，不完整字节不写入。

## 3. 使用示例（CH32）

```c
#include "max17048.h"
#include "max17048_ch32_i2c_port.h"

static max17048_dev_t g_gauge;                  /* 句柄由调用者分配，可多实例 */
static max17048_ch32_adapter_t g_adapter =
{
    .board_context = NULL,                      /* 单总线可为 NULL */
    .mem_write     = board_i2c_mem_write,       /* 板级 I2C 回调 */
    .mem_read      = board_i2c_mem_read,
    .io_timeout_ms = 10,
};

void app_init(void)
{
    /* io_ctx 传适配器指针；地址为 7 位 0x36（MAX17048_I2C_ADDR） */
    max17048_init(&g_gauge, &g_adapter, MAX17048_I2C_ADDR);
    max17048_set_soc_alert_threshold(&g_gauge, 10);       /* SOC<10% 告警 */
    max17048_set_voltage_alerts(&g_gauge, 3300, 4250);
}

void app_periodic_1s(int16_t temp_x10)
{
    uint32_t mv;  uint8_t soc;  int16_t rate;

    max17048_temp_compensate(&g_gauge, temp_x10);        /* ≥1 次/分钟 */
    max17048_read_vcell(&g_gauge, &mv);                  /* mV */
    max17048_read_soc(&g_gauge, &soc);                   /* % */
    max17048_read_crate(&g_gauge, &rate);                /* 0.1%/h，正充负放 */
}

/* ALRT 引脚 EXTI 中断里只置标志，线程上下文再服务告警 */
void app_alert_task(void)
{
    max17048_status_t st;
    max17048_get_status(&g_gauge, &st);
    if (st.soc_low) { /* 低电量处理 */ }
    max17048_clear_alerts(&g_gauge, MAX17048_STATUS_ALERT_MASK);
}
```

完整流程见 `examples/ch32/max17048_ch32_example.c`。

## 4. API 参考

单位约定：电压 **mV**，SOC **%**（精确版 0.01%），充放电速率 **0.1%/h**，温度 **0.1℃**。全部定点运算。set 类函数按硬件档位就近取整并钳位，对应 get 返回实际生效值。

### 4.1 初始化 / 复位 / 标识

| 函数 | 说明 |
| --- | --- |
| `max17048_init(dev, io_ctx, dev_addr)` | 校验 VERSION（期望 0x001x）；POR 后自动清除 STATUS.RI |
| `max17048_get_id(dev, id)` | VERSION 原始值 + VRESET/ID 低字节 OTP 出厂标识 |
| `max17048_full_reset(dev)` | CMD←0x5400 全复位（器件无 ACK，驱动忽略该写错误）；复位后需重载自定义模型 |

### 4.2 测量

| 函数 | 单位/换算 | 说明 |
| --- | --- | --- |
| `max17048_read_vcell(dev, mv)` | mV = raw×78.125µV×节数 | 活动模式 250ms 更新，休眠 45s |
| `max17048_read_vcell_raw` | raw | |
| `max17048_read_soc(dev, percent)` | % = raw/256，四舍五入 | POR 后首次有效值约 1s |
| `max17048_read_soc_precise(dev, x100)` | 0.01% | 分辨率 1/256% |
| `max17048_read_soc_raw` | raw | |
| `max17048_read_crate(dev, 0.1%/h)` | = raw×0.208×10，有符号 | 近似值，不可换算为安培 |
| `max17048_read_crate_raw` | raw（补码） | |

### 4.3 RCOMP 温度补偿

| 函数 | 说明 |
| --- | --- |
| `max17048_get_rcomp / set_rcomp` | CONFIG.RCOMP 原始字节（POR 0x97） |
| `max17048_temp_compensate(dev, temp_x10)` | 按手册公式 `RCOMP = RCOMP0 + (T-20)×系数` 计算并写入；系数/基准在 conf 中配置，**至少每分钟调用一次** |

### 4.4 睡眠 / 休眠

| 函数 | 说明 |
| --- | --- |
| `max17048_sleep_enter / sleep_exit` | 睡眠 <1µA（停止一切运算，不检测自放电）；进：EnSleep=1 再 SLEEP=1，出：SLEEP=0（其它通信不唤醒） |
| `max17048_is_hibernating` | 读 MODE.HibStat（见已知坑 #9） |
| `max17048_hibernate_disable / force` | HIBRT←0x0000（始终活动 23µA）/ 0xFFFF（强制休眠 3µA） |
| `max17048_hibernate_set/get_thresholds` | 原始档位：HibThr×0.208%/h（进入，6min 判定）、ActThr×1.25mV（退出） |

### 4.5 告警（ALRT 引脚，开漏低有效）

| 函数 | 说明 |
| --- | --- |
| `max17048_set/get_soc_alert_threshold` | 低电量阈值 1~32%（POR 4%），仅下降沿触发 |
| `max17048_set_soc_change_alert` | SOC 每变 1% 告警（勿用于累计统计 SOC） |
| `max17048_set/get_voltage_alerts` | VALRT 窗口，20mV 档，0~5100mV |
| `max17048_set/get_vreset_threshold` | 电池更换复位阈值，40mV 档，钳位 2280~3480mV；固定电池建议 2.5V，可拆卸电池比放空电压至少低 300mV |
| `max17048_set_vreset_alert_enable` | 电压复位事件告警使能（STATUS.ENVR） |
| `max17048_get_status` | 解码 RI/VH/VL/VR/HD/SC（只读不清除） |
| `max17048_clear_alerts(dev, bits)` | 写 1 清指定 STATUS 位并清 CONFIG.ALRT 释放引脚 |

### 4.6 快速启动 / 模型表（`MAX17048_USE_MODEL_TABLE`）

| 函数 | 说明 |
| --- | --- |
| `max17048_quick_start` | MODE.QuickStart=1，按即时电压重估 SOC（慎用，见已知坑 #8） |
| `max17048_model_load(dev, table[64])` | 解锁（0x3E←0x4A57）→ 逐字写 0x40~0x7F → 复锁（0x3E←0x0000）；中途失败也尽快复锁 |

### 4.7 寄存器级原始访问

`max17048_read_reg / write_reg / update_bits` —— 调试与未封装功能用。注意勿对 STATUS 用 update_bits（读-改-写会把已置位告警位写 1 而误清除），STATUS 一律走 `get_status/clear_alerts`。

## 5. 寄存器速查

| 地址 | 名称 | 类型 | POR | 关键位 |
| --- | --- | --- | --- | --- |
| 0x02 | VCELL | R | — | 78.125µV/cell |
| 0x04 | SOC | R | — | 1/256 % |
| 0x06 | MODE | W | 0x0000 | bit14 QuickStart / bit13 EnSleep / bit12 HibStat(RO) |
| 0x08 | VERSION | R | 0x001x | 高 12 位 = 0x001 |
| 0x0A | HIBRT | RW | 0x8030 | [15:8] HibThr 0.208%/h / [7:0] ActThr 1.25mV |
| 0x0C | CONFIG | RW | 0x971C | [15:8] RCOMP / bit7 SLEEP / bit6 ALSC / bit5 ALRT / [4:0] ATHD(=32-%) |
| 0x14 | VALRT | RW | 0x00FF | [15:8] MIN / [7:0] MAX，20mV 档 |
| 0x16 | CRATE | R | — | 0.208%/h 有符号 |
| 0x18 | VRESET/ID | RW | 0x96xx | [15:9] 阈值 40mV 档 / bit8 Dis / [7:0] OTP ID(RO) |
| 0x1A | STATUS | RW | 0x01xx | bit14 EnVR / bit13 SC / bit12 HD / bit11 VR / bit10 VL / bit9 VH / bit8 RI，告警位写 1 清除 |
| 0x3E/0x3F | 解锁 | W | — | 0x4A57 解锁 / 0x0000 复锁模型表 |
| 0x40~0x7F | TABLE | W | — | 64×16 位电池模型 |
| 0xFE | CMD | RW | 0xFFFF | 0x5400 = 全复位（无 ACK） |

I2C：7 位地址 **0x36**（8 位写 0x6C / 读 0x6D），最高 400kHz，16 位寄存器高字节在偶地址。

## 6. 已知坑（来自数据手册）

1. **16 位单事务**：8 位写无效、不完整字节不写入、读也必须两字节同事务，否则数据无效。io 层必须保证。
2. **地址混淆**：0x36 是 7 位地址；0x6C/0x6D 是左移后的 8 位写/读地址。HAL 各异，模板中已注明换算。
3. **全复位无 ACK**：CMD 写 0x5400 在最后一位时钟移入后复位，器件不回 ACK。`full_reset()` 已忽略该写的 io 错误；移植层若对该 NACK 报错属预期。
4. **POR 后 RI 置位**：`init()` 会自动清除。RI 置位期间器件未配置——使用自定义模型的应用必须在 RI 出现后（含每次 `full_reset`、换电池触发 VRESET）重新 `model_load()`。
5. **ALRT 引脚粘滞**：告警触发后引脚保持低直到软件写 CONFIG.ALRT=0。`clear_alerts()` 已包含此步；只清 STATUS 不清 CONFIG.ALRT 引脚不会释放。
6. **更新率**：VCELL 活动 250ms / 休眠 45s；SOC 首次更新在 POR 后约 1s。刚上电读 SOC 可能是复位前的旧值。
7. **睡眠 vs 休眠**：睡眠 <1µA 但停止一切运算、不检测自放电，充放电前必须唤醒（写 CONFIG.SLEEP=0，其它通信无效）；手册建议能接受 3~4µA 的应用用默认自动休眠。另外把总线拉低 tSLEEP(1.75~2.5s) 也会进睡眠——I2C 时序异常偏慢时注意。
8. **快速启动慎用**：手册明确"多数系统不应使用"，仅在上电波形导致初始 SOC 明显错误且电池电压已稳定时使用；POR 本身含快速启动，两者都要求电池充分弛豫。
9. **MODE 只写**：Table 2 标注 MODE 为 W，`is_hibernating()` 读 HibStat 位在个别批次可能返回无效值，仅作参考。
10. **模型表写入方式**：手册规定自增写越过 0x4F 后数据被忽略，因此 64 字模型表**不能一次突发写**，驱动按每字独立事务写入。解锁期间 ModelGauge 引擎停止更新，驱动尽快复锁。
11. **RCOMP 补偿**：主机应至少每分钟按电池温度更新 RCOMP（`temp_compensate()`）；默认系数（0x97/-0.5/-5.0）只是通用起点。
12. **VRESET 应用配置**：固定电池 2.5V（实际档位 2480/2520mV 二选一）；可拆卸电池比系统放空电压至少低 300mV。Dis=1 休眠省约 0.5µA，代价是复位检测从 1ms 模拟比较退化为 250ms 数字比较。
13. **读越界**：读超过 0xFF 返回 0xFF；保留地址返回未定义值。写入只读地址被忽略。

## 7. 文件清单

| 文件 | 用途 |
| --- | --- |
| `max17048.h` | 公共类型、结果码与全部 API 声明 |
| `max17048.c` | 驱动核心（协议/换算/序列，零平台代码） |
| `max17048_conf.h` | 配置宏（变体、功能裁剪、温补参数），全部可 `-D` 覆盖 |
| `max17048_regs.h` | 寄存器地址、位定义、POR 值与命令常量 |
| `max17048_io.h` | 移植契约（4 必选 + 2 可选函数） |
| `port/max17048_io_template.c` | 移植模板（填空注释 + STM32 HAL/ESP-IDF/裸机示例） |
| `examples/ch32/` | CH32 BSP 桥接模板与完整使用示例（不绑定型号/SDK） |
| `tests/test_max17048.c` | 寄存器镜像 mock 单测（换算、位打包、访问序列、故障注入） |
| `tests/run_tests.ps1` | MounRiver ARM/RV32 编译 + RV32 模拟执行脚本 |
| `C2682616_...PDF` | 数据手册副本（Rev 7） |

## 8. 运行测试

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

脚本自动校验 PDF 哈希、核心 include 边界，然后做 ARM Cortex-M0 / RV32IMAC 双目标 `-Wall -Wextra -Werror -pedantic` 编译，最后在 RV32 模拟器跑默认与 `VERIFY_WRITES=1` 两组单测（工具链路径可用参数或环境变量覆盖，缺省探测 MounRiver Studio 2 安装目录）。
