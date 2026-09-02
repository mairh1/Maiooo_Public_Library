# MAX17260 1 节锂电 ModelGauge m5 EZ 电量计通用驱动

面向 32 位 MCU 的 MAX17260 1 节锂电 ModelGauge m5 EZ 电量计驱动。核心使用纯 C99，不含任何厂商头文件，不分配动态内存，不使用浮点、递归或全局可变状态；CH32 等平台通过 `max17260_io.h` 移植契约接入。

实现依据本目录中的数据手册 `max17260.pdf`：

- 芯片手册：**MAX17260, 19-100249; Rev 2; 7/24**（Analog Devices / Maxim）
- 封装：14-pin TDFN（3mm × 3mm）或 9-pin WLP（1.5mm × 1.5mm）
- 关键料号：MAX17260SEWL+ / SETD+（I2C 地址 0x36）、MAX17260BEWL+（I2C 地址 0x0D）
- 驱动版本：1.0.0

> ModelGauge m5 EZ 算法对绝大多数锂离子电池无需电池表征；ModelID=0 适用大部分钴酸锂，ModelID=2 适用 NCR/NCA，ModelID=6 适用 LiFePO4（手册 Table 4）。对 ModelID=2/6 等特殊电池建议向 ADI 申请定制模型以获得最佳精度。

## 1. 架构（分层设计）

```
+--------------------------------------+
|  应用层                              |  你的代码
+--------------------------------------+
           |  本 API（max17260.h）
+--------------------------------------+
|  驱动核心   max17260.c               |  纯 C99，零平台代码
|  配置       max17260_conf.h          |  可多实例
|  寄存器定义 max17260_regs.h          |
+--------------------------------------+
           |  4~6 个 io 函数（max17260_io.h）
+--------------------------------------+
|  移植层（你实现）                     |  CH32 / STM32 / ESP32 /
|                                      |  RTOS / Linux / 模拟 I2C
+--------------------------------------+
```

**线程安全（默认）**：不同 `max17260_dev_t` 实例之间并发安全；同一实例的并发访问需外部保护。置 `MAX17260_THREAD_SAFE=1` 后，驱动在多笔总线访问的 API 前后调用 `max17260_io_lock/unlock()`（由移植层实现，如 RTOS 互斥量）。

## 2. 移植三步走

1. 复制 `port/max17260_io_template.c` 到工程（可改名），用平台 I2C API 填空 4 个函数。
2. 按需调整 `max17260_conf.h`（变体、RSENSE 毫欧值、功能开关），或用编译器 `-D` 覆盖。
3. 调用 `max17260_init()`，随后 `max17260_configure_model()` 写入 DesignCap / VEmpty / IChgTerm 三件套——之后核心文件零改动。

**移植契约的硬性要求**：器件全部数据寄存器为 16 位（DieTemp 0x034 是 8 位地址但同事务内仍按 16 位寄存器访问），**两个字节必须在同一 I2C 事务中传输**（写：S+地址W+寄存器+高字节+低字节+P；读：带 Sr 重复起始），高字节在前。8 位写无效，不完整字节不写入。

## 3. 使用示例（伪代码）

```c
#include "max17260.h"
#include "max17260_io.h"   /* 你的移植层实现 */

/* 句柄由调用者分配，可多实例 */
static max17260_dev_t g_gauge;

void app_init(void)
{
    /* 7 位地址：SEWL/SETD=0x36，BEWL=0x0D */
    max17260_init(&g_gauge, NULL, MAX17260_I2C_ADDR_DEFAULT);

    /* Model m5 EZ 上电必须配置三件套（手册 Application Notes） */
    max17260_configure_model(&g_gauge,
                             2000,    /* DesignCap mAh */
                             3300,    /* VEmpty mV */
                             3880,    /* VRecovery mV */
                             100);    /* IChgTerm mA */

    /* 电压告警窗口（20mV 档） */
    max17260_set_voltage_alerts(&g_gauge, 3300, 4250);
}

void app_periodic_1s(void)
{
    uint32_t mv;
    uint8_t  soc;
    int16_t  temp_x10;
    int32_t  ma;
    uint32_t mah;

    max17260_read_vcell(&g_gauge, &mv);            /* mV */
    max17260_read_soc(&g_gauge, &soc);             /* % */
    max17260_read_temp(&g_gauge, &temp_x10);       /* 0.1℃ */
    max17260_read_current(&g_gauge, &ma);          /* mA */
    max17260_read_repcap(&g_gauge, &mah);          /* mAh */
}

/* ALRT 引脚 EXTI 中断里只置标志，线程上下文再服务告警 */
void app_alert_task(void)
{
    max17260_status_t st;
    max17260_get_status(&g_gauge, &st);
    if (st.vcell_low)  { /* 欠压处理 */ }
    if (st.soc_low)    { /* 低 SOC 处理 */ }
    max17260_clear_alerts(&g_gauge, MAX17260_STATUS_CLEAR_MASK);
}
```

## 4. API 参考

单位约定：电压 **mV**、SOC **%**（精确版 0.01%）、温度 **0.1℃**、容量 **mAh**、电流 **mA**、时间 **s**。全部定点运算。容量/电流的 LSB 单位在数据手册中是 µVh / µV（与 RSENSE 相关），当 `MAX17260_RSENSE_MOHM != 0` 时 API 自动换算为 mAh / mA；设为 0 时返回原始 µVh / µV 值（不关心实际安培数的应用）。set 类函数按硬件档位就近取整并钳位，对应 get 返回实际生效值。

### 4.1 初始化 / 标识

| 函数 | 说明 |
| --- | --- |
| `max17260_init(dev, io_ctx, dev_addr)` | 读 Status 验证器件在位；POR 置位则自动清除（自定义模型需随后 `configure_model`） |
| `max17260_is_por(dev, por)` | 查询 Status.POR 位 |

### 4.2 测量

| 函数 | 单位/换算 | 说明 |
| --- | --- | --- |
| `max17260_read_vcell(dev, mv)` | mV = raw × 78.125µV / 1000 | 满量程 0~5119.92mV |
| `max17260_read_vcell_raw` | raw | |
| `max17260_read_soc(dev, percent)` | % = raw/256，四舍五入 | POR 后 351ms 内无效 |
| `max17260_read_soc_precise(dev, x100)` | 0.01% | 原始分辨率 1/256% |
| `max17260_read_soc_raw` | raw | |
| `max17260_read_temp(dev, temp_x10)` | 0.1℃ = raw × 10 / 256 | 内部或 NTC 由 Config.TSEL 决定 |
| `max17260_read_temp_raw` | raw（补码） | |
| `max17260_read_current(dev, ma)` | mA = raw × 1.5625µV / RSENSE | 充正放负 |
| `max17260_read_current_raw` | raw（补码） | |
| `max17260_read_repcap(dev, mah)` | mAh = raw × 5.0µVh / RSENSE | 剩余容量 |
| `max17260_read_repcap_raw` | raw | |
| `max17260_read_fullcaprep(dev, mah)` | mAh | 满充容量 |
| `max17260_read_tte(dev, s)` | s = raw × 5.625 | 仅当 Current<0 有效 |
| `max17260_read_ttf(dev, s)` | s = raw × 5.625 | 仅当 Current>0 有效 |
| `max17260_read_power(dev, mw)` | mW = raw × 8µV² / RSENSE | 瞬时功率 |
| `max17260_read_cycles(dev, pct)` | 1% / LSB | 100% = 一次完整充放电 |
| `max17260_read_age(dev, pct)` | 1% / LSB | 电池老化程度 |
| `max17260_read_dietemp(dev, temp_x10)` | 0.1℃ | 内部 die 温度（与 Temp 一致当 TSEL=0） |

### 4.3 平均与最大最小

| 函数 | 单位/换算 | 说明 |
| --- | --- | --- |
| `max17260_read_avg_vcell(dev, mv)` | mV | 平均电压 |
| `max17260_read_avg_current(dev, ma)` | mA | 平均电流 |
| `max17260_read_avg_temp(dev, x10)` | 0.1℃ | 平均温度 |
| `max17260_read_avg_power(dev, mw)` | mW | 平均功率 |
| `max17260_read_maxmin_volt(dev, max, min)` | 20mV/LSB | 写 0x00FF 复位 |
| `max17260_read_maxmin_curr(dev, max, min)` | 0.4mV/RSENSE/LSB | 写 0x807F 复位 |
| `max17260_read_maxmin_temp(dev, max, min)` | 1℃/LSB | 写 0x807F 复位 |
| `max17260_reset_maxmin(dev)` | — | 三个 MaxMin 寄存器全部复位 |

### 4.4 Model m5 EZ 配置

| 函数 | 说明 |
| --- | --- |
| `max17260_configure_model(dev, dcap_mah, ve_mv, vr_mv, ichg_term_ma)` | 一站式写入 DesignCap / VEmpty / IChgTerm；POR 后 351ms 内输出无效；必调 |
| `max17260_get_design_cap` | 读 DesignCap |
| `max17260_get_vempty(dev, ve_mv, vr_mv)` | 读 VEmpty（VE 10mV 档 / VR 40mV 档） |
| `max17260_get_ichg_term` | 读 IChgTerm |
| `max17260_get/set_modelcfg(dev, val)` | 读 / 写 ModelCfg 原始值（ModelID / R100 / VChg / CSEL / Refresh） |
| `max17260_get/set_config(dev, val)` | 读 / 写 Config 原始值（TSEL / SS/TS/VS/IS / Aen 等） |
| `max17260_get/set_config2(dev, val)` | 读 / 写 Config2 原始值（AtRateEn / DPEn / dSOCen / TAlrtEn 等） |

### 4.5 告警（ALRT 引脚，开漏低有效）

| 函数 | 说明 |
| --- | --- |
| `max17260_set/get_voltage_alerts` | VAlrtTh 窗口，20mV 档，0~5100mV；写 0xFF00 禁用 |
| `max17260_set/get_temp_alerts` | TAlrtTh 窗口，1℃ 档，-128~+127℃；写 0x7F80 禁用 |
| `max17260_set/get_soc_alerts` | SAlrtTh 窗口，1% 档，0~255%；写 0xFF00 禁用 |
| `max17260_set/get_current_alerts` | IAlrtTh 窗口，0.4mV/RSENSE/LSB（补码）；写 0x7F80 禁用 |
| `max17260_get_status(dev, st)` | 解码 Br/Bst/Bi/BI/Smx/Smn/Vmx/Vmn/Tmx/Tmn/Imx/Imn/DSOCI/POR（不清除） |
| `max17260_clear_alerts(dev, bits)` | 对 Status 中被置 1 的指定位写 1 清除 |

> 告警触发方式由 Config.SS / TS / VS / IS 控制：1 = sticky（须软件清零），0 = 自动清除。`Bst` 反映电池是否在位（0=在，1=不在），`Br`/`Bi`/`dSOCi`/`POR` 必须软件清零。

### 4.6 序列号（`MAX17260_USE_SERIAL_NUMBER`）

| 函数 | 说明 |
| --- | --- |
| `max17260_read_serial(dev, sn)` | 内部清 Config2.AtRateEn/DPEn 切到序列号模式，读 0xD4~0xDF 8 个 16 位字后恢复 |

读取过程会暂时覆盖 Dynamic Power / AtRate 输出寄存器；中途失败仍会尝试恢复。

### 4.7 寄存器级原始访问

`max17260_read_reg / write_reg / update_bits` —— 调试与未封装功能用。注意勿对 Status 用 `update_bits`（会把已置位告警位写 1 而误清除），Status 一律走 `get_status / clear_alerts`。

## 5. 寄存器速查（手册 Table 17 Memory Map）

| 地址 | 名称 | 类型 | POR | 关键位 / 分辨率 |
| --- | --- | --- | --- | --- |
| 0x00 | Status | R/W | 0x8082 | Br/Smx/Tmx/Vmx/Bi/Smn/Tmn/Vmn/dSOCi/Imx/X/X/Bst/Imn/POR/X，告警位写 1 清除 |
| 0x01 | VAlrtTh | R/W | 0xFF00 | [15:8] VMAX / [7:0] VMIN，20mV 档 |
| 0x02 | TAlrtTh | R/W | 0x7F80 | [15:8] TMAX / [7:0] TMIN，1℃ 档（补码） |
| 0x03 | SAlrtTh | R/W | 0xFF00 | [15:8] SMAX / [7:0] SMIN，1% 档 |
| 0x04 | AtRate | R/W | 0x0000 | 虚拟负载电流 mA |
| 0x05 | RepCap | R | — | 剩余容量 mAh（5µVh/RSENSE/LSB） |
| 0x06 | RepSOC | R | — | SOC 百分比（1/256%/LSB） |
| 0x07 | Age | R | — | 老化系数（1%/LSB） |
| 0x08 | Temp | R | — | 温度（1/256℃/LSB，补码） |
| 0x09 | VCell | R | — | 电池电压（78.125µV/LSB） |
| 0x0A | Current | R | — | 瞬时电流（1.5625µV/RSENSE/LSB，补码） |
| 0x0B | AvgCurrent | R | — | 平均电流 |
| 0x10 | FullCapRep | R | — | 满充容量 mAh |
| 0x11 | TTE | R | — | 剩余放电时间（5.625s/LSB） |
| 0x16 | AvgTA | R | — | 平均温度 |
| 0x17 | Cycles | R | — | 充放电循环（1%/LSB） |
| 0x18 | DesignCap | R/W | 0x0BB8 | 设计容量 mAh |
| 0x19 | AvgVCell | R | — | 平均电压 |
| 0x1A | MaxMinTemp | R/W | 0x807F | [15:8] Max / [7:0] Min，1℃ 档 |
| 0x1B | MaxMinVolt | R/W | 0x00FF | [15:8] Max / [7:0] Min，20mV 档 |
| 0x1C | MaxMinCurr | R/W | 0x807F | [15:8] Max / [7:0] Min，0.4mV/RSENSE/LSB |
| 0x1D | Config | R/W | 0x2210 | TSEL/SS/TS/VS/IS/THSH/TEN/TEX/SHDN/COMMSH/ETHRM/FTHRM/Aen/Bei/Ber |
| 0x1E | IChgTerm | R/W | 0x0640 | 充电终止电流（高 8 位有效） |
| 0x20 | TTF | R | — | 剩余充电时间（5.625s/LSB） |
| 0x034 | DieTemp | R | — | 芯片内部温度（8 位地址） |
| 0x3A | VEmpty | R/W | 0xA561 | [15:7] VE 10mV 档 / [6:0] VR 40mV 档 |
| 0xB1 | Power | R | — | 瞬时功率（8µV²/RSENSE/LSB） |
| 0xB3 | AvgPower | R | — | 平均功率 |
| 0xB4 | IAlrtTh | R/W | 0x7F80 | [15:8] IMAX / [7:0] IMIN，0.4mV/RSENSE/LSB |
| 0xBB | Config2 | R/W | 0x3658 | AtRateEn/DPEn/POWR/dSOCen/TAlrtEn/LDMdl 等 |
| 0xD4~0xDF | SN | 特殊 | — | 128 位序列号（须先清 AtRateEn/DPEn） |
| 0xDB | ModelCfg | R/W | 0x0000 | Refresh/R100/VChg/ModelID/CSEL |

I2C：7 位地址 **0x36**（SEWL+ / SETD+）或 **0x0D**（BEWL+），最高 400kHz，16 位寄存器高字节在偶地址。

## 6. 已知坑（来自数据手册）

1. **16 位单事务**：8 位写无效、不完整字节不写入、读也必须两字节同事务，否则数据无效。io 层必须保证（CH32/STM32 HAL 的 Mem_Read/Mem_Write 天然满足）。
2. **地址混淆**：0x36 / 0x0D 是 7 位地址；HAL 需左移一位后传入。BEWL+ 与 SEWL+ 仅丝印末位不同，注意料号。
3. **POR 后输出无效**：ModelGauge m5 算法输出 351ms 后才有效，刚上电读 RepCap/RepSOC/Tte/Ttf 可能是 POR 前的旧值。建议上电后等 ≥351ms 再读测量类 API，或确认 Status.POR 清零。
4. **POR 位粘滞**：POR 须软件清零（`clear_alerts(MAX17260_STATUS_POR)`）；使用自定义模型的应用在 POR（含换电池触发 VRESET 等）后必须重新 `configure_model`。
5. **RSENSE 必填项**：`MAX17260_RSENSE_MOHM` 不配置或填 0 时容量/电流相关 API 返回原始 µVh / µV 值；填非 0 毫欧值才能换算到 mAh / mA。建议与硬件实际取值一致。
6. **RSENSE 与 LSB**：手册 Table 2 明示容量 LSB = 5.0µVh/RSENSE、电流 LSB = 1.5625µV/RSENSE、功率 LSB = 8µV²/RSENSE——所有换算均依赖 RSENSE 真实值；典型 10mΩ，范围 1mΩ~1000mΩ。
7. **TTE/TTF 方向**：TTE 仅在 Current<0 时有意义，TTF 仅在 Current>0 时有意义（手册 § TTE/TTF Register）。
8. **Bst / Br / Bi**：`Bst` 直接反映电池在位状态（0=在，1=不在）；`Br`/`Bi` 是事件位（电池移除 / 插入），须软件清零。
9. **告警清除语义**：Status 告警位写 1 清除、写 0 不影响。`update_bits` 会做读-改-写，会把已置位告警位写 1 而误清——Status 一律走 `get_status / clear_alerts`。
10. **告警触发方式**：Config.SS/TS/VS/IS 决定 SOC/温度/电压/电流告警是否 sticky（1 = 软件清，0 = 自动清）。`Bst/Br/Bi/dSOCi/POR` 始终须软件清零。
11. **序列号读取窗口**：`read_serial` 内部会清 Config2.AtRateEn/DPEn，**期间 Dynamic Power / AtRate 输出寄存器被覆盖**；中途失败仍会尝试恢复。应用层应避免在该窗口调用动态功率 / AtRate 类接口。
12. **Model m5 EZ 上电必配**：DesignCap / VEmpty / IChgTerm 三件套至少要在 POR 后配置一次，否则算法使用手册默认值（DesignCap=0x0BB8=3000mAh，VEmpty=3.30V/3.88V，IChgTerm=0x0640 对应 10mΩ 下 250mA）——多数电池需调整。
13. **ModelID 选择**：ModelID=0 适用大部分钴酸锂（手册建议）；ModelID=2 适用 NCR/NCA，ModelID=6 适用 LiFePO4（建议申请定制模型）。
14. **ALERT 引脚**：开漏低有效，外部须上拉；可连接到主控 EXTI，中断里只置标志，由线程上下文调 `get_status / clear_alerts`。
15. **关断模式**：写 Config.SHDN=1 后等待 ShdnTimer（默认 45s）进入关断；进入前可先写 HibCFG=0x0000（参见手册 User Guide 6597 / 旧版 Data Sheet 描述，本驱动不单独封装）以便 ≤45s 内能响应 SHDN。
16. **温度传感器选择**：Config.TSEL=0 用内部 die（默认），TSEL=1 用外部 NTC（须同时 ETHRM=1 + ModelCfg.R100 匹配 10k/100k）。
17. **Hibernate 模式**：手册 § Detailed Description；唤醒时芯片自动恢复，无需软件干预。
18. **读越界**：读超过 0xFF 返回 0xFF；保留地址返回未定义值。写入只读地址被器件忽略。

## 7. 文件清单

| 文件 | 用途 |
| --- | --- |
| `max17260.h` | 公共类型、结果码与全部 API 声明 |
| `max17260.c` | 驱动核心（协议/换算/序列，零平台代码） |
| `max17260_conf.h` | 配置宏（变体、RSENSE、功能裁剪、线程安全），全部可 `-D` 覆盖 |
| `max17260_regs.h` | 寄存器地址、位定义、POR 值与换算常量 |
| `max17260_io.h` | 移植契约（4 必选 + 2 可选函数） |
| `port/max17260_io_template.c` | 移植模板（填空注释 + STM32 HAL/ESP-IDF/裸机示例） |
| `max17260.pdf` | 数据手册副本（Rev 2） |
