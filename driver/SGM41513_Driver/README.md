# SGM41513 通用驱动

适用于 **SGM41513 / SGM41513A / SGM41513D**（圣邦微 SG Micro）单节锂电充电管理
芯片的纯 C99 跨平台驱动。参考数据手册：*SGM41513_SGM41513A_SGM41513D,
APRIL 2025 REV. C.1*。

| 项目 | 说明 |
|---|---|
| 芯片 | 3.9–13.5 V 输入、3 A 单节锂电 NVDC 电源路径充电器 + OTG 反向升压 |
| 接口 | I2C，7 位地址 `0x1A`，最高 400 kHz，16 个 8 位寄存器（REG00–REG0F） |
| 语言 | C99，无动态内存分配，无平台头文件依赖，可重入（多实例） |
| 移植 | 只需实现 `sgm41513_io.h` 中的 4 个函数（见下文） |

## 1. 架构（分层设计）

```
+--------------------------------------------+
|  应用层                                     |  你的代码
+--------------------------------------------+
        |  sgm41513.h        （公共 API）
+--------------------------------------------+
|  驱动核心 sgm41513.c                        |  可移植，不含任何
|  配置     sgm41513_conf.h                   |  平台代码
|  寄存器   sgm41513_regs.h                   |
+--------------------------------------------+
        |  sgm41513_io.h      （移植接口，仅 4 个函数）
+--------------------------------------------+
|  移植层（你实现）                             |  STM32 / ESP-IDF /
|  参考 port/sgm41513_io_template.c           |  FreeRTOS / Linux / ...
+--------------------------------------------+
```

## 2. 移植三步走

1. **复制文件**：把 `sgm41513.c`、`sgm41513.h`、`sgm41513_regs.h`、
   `sgm41513_conf.h`、`sgm41513_io.h` 加入工程。
2. **实现移植层**：复制 `port/sgm41513_io_template.c` 为 `sgm41513_io.c`，
   按注释填入你的 I2C 读/写/延时实现（模板内含 STM32 HAL 与 ESP-IDF 示例）。
   驱动只做**单字节**寄存器访问，不需要块传输支持。
3. **按需修改配置** `sgm41513_conf.h`（或用编译器 `-D` 覆盖）：

| 配置项 | 默认 | 说明 |
|---|---|---|
| `SGM41513_VARIANT` | `...D` | PN 位无法区分 A 与 D，用此项指定（影响 VBUS 状态解码）；基础版 SGM41513 由驱动在 `init` 时自动识别 |
| `SGM41513_USE_JEITA` | 1 | JEITA 温度窗口配置，置 0 裁剪掉相关 API |
| `SGM41513_USE_OTG` | 1 | OTG 反向升压 API |
| `SGM41513_USE_PUMPX` | 1 | PUMPX 适配器调压协议 API |
| `SGM41513_USE_SHIP` | 1 | 船运模式 / BATFET API |
| `SGM41513_REG_SHADOW` | 1 | 寄存器影子缓存，支持 `restore_settings()`（看门狗超时后恢复配置） |
| `SGM41513_VERIFY_WRITES` | 0 | 写后读回校验（自清除位除外），多一次 I2C 读 |
| `SGM41513_THREAD_SAFE` | 0 | 使能 `sgm41513_io_lock/unlock()` 并发保护钩子 |

## 3. 使用示例

```c
#include "sgm41513.h"

static sgm41513_dev_t charger;          /* 每片芯片一个实例 */

void charger_setup(void)
{
    sgm41513_id_t id;

    /* io_ctx = 你的 I2C 总线句柄（单总线可传 NULL）；
       dev_addr 传 0 表示用默认 0x1A */
    if (sgm41513_init(&charger, NULL, 0) != SGM41513_OK) { /* 处理错误 */ }

    sgm41513_get_id(&charger, &id);     /* 校验 PN / 版本 */

    /* 注意：写过寄存器后芯片进入 host 模式，必须处理看门狗：
       要么周期调用 sgm41513_feed_watchdog()，要么直接关闭看门狗 */
    sgm41513_set_watchdog(&charger, SGM41513_WDT_OFF);

    sgm41513_set_ichg(&charger, 1500);              /* 快充 1.5 A      */
    sgm41513_set_vreg(&charger, 4208);              /* 4.208 V        */
    sgm41513_set_precharge_current(&charger, 120);  /* 预充 120 mA     */
    sgm41513_set_term_current(&charger, 120);       /* 终止 120 mA     */
    sgm41513_set_sys_min_voltage(&charger, 3500);   /* 系统最低 3.5 V  */
    sgm41513_charge_enable(&charger, true);

    /* 输入电流门限会在适配器插入、类型检测完成后被硬件自动改写，
       检测完成后重新写入你的目标值 */
    bool det_done = false;
    sgm41513_input_detect_done(&charger, &det_done);
    if (det_done) {
        sgm41513_set_iindpm(&charger, 2000);        /* 输入限流 2 A    */
        sgm41513_set_vindpm_os(&charger, SGM41513_VINDPM_OS_3900MV);
        sgm41513_set_vindpm(&charger, 4500);        /* 输入欠压门限    */
    }
}

void charger_poll_1s(void)             /* 周期调用 */
{
    sgm41513_status_t st;
    sgm41513_faults_t  f;

    sgm41513_feed_watchdog(&charger);

    if (sgm41513_get_status(&charger, &st) == SGM41513_OK) {
        /* st.vbus_type / st.charge_status / st.power_good ... */
    }
    if (sgm41513_get_faults(&charger, &f) == SGM41513_OK) {
        /* f.watchdog_fault / f.charge_fault / f.ntc_fault ...
           内部已对 REG09 做两次读取，返回的是实时故障 */
    }
}

/* OTG 反向供电（升压到 5.15 V，限流 1.2 A） */
void charger_otg_on(void)
{
    sgm41513_set_boost_voltage(&charger, SGM41513_BOOST_V_5150MV);
    sgm41513_set_boost_current_limit(&charger, SGM41513_BOOST_LIM_1200MA);
    sgm41513_otg_enable(&charger, true);  /* 内部自动清 HIZ 并延时 30 ms */
}
```

## 4. API 参考

所有函数返回 `sgm41513_result_t`：`SGM41513_OK` 成功；`ERR_IO` 通信失败；
`ERR_PARAM` 参数错误；`ERR_NOT_READY` 未初始化/芯片无应答；
`ERR_NOT_SUPPORTED` 功能被 `sgm41513_conf.h` 裁剪；`ERR_VERIFY` 写入校验失败。

电流单位 mA、电压单位 mV。所有 set 函数**就近取整到硬件档位并钳位**，
对应 get 函数返回实际生效值。

### 初始化 / 复位
| 函数 | 说明 |
|---|---|
| `sgm41513_init(dev, io_ctx, addr)` | 初始化、校验芯片 ID（SGMPART/PN）、载入影子缓存 |
| `sgm41513_get_id(dev, *id)` | 读 PN / 版本 |
| `sgm41513_reset(dev)` | REG_RST，全部 R/W 寄存器回到上电默认值 |
| `sgm41513_restore_settings(dev)` | 把影子缓存整体写回（看门狗超时/出船运模式后用） |

### 充电控制
| 函数 | 范围 / 说明 |
|---|---|
| `charge_enable(dev, en)` | 主开关；外部 nCE 引脚也必须为低 |
| `set/get_ichg` | 0–3000 mA，非线性 64 档表（0 档 = 禁止充电） |
| `set/get_vreg` | 3856–4624 mV，32 mV 步进（码 15 特例 4350 mV） |
| `set_vreg_ft` | 细调 0/+8/−8/−16 mV |
| `set/get_precharge_current` | 5–240 mA，16 档表 |
| `set/get_term_current` | 5–240 mA，16 档表 |
| `set_term_enable` / `set_term_deglitch` | 终止使能 / 去抖 230 ms 或 16 ms |
| `set_recharge_threshold` | 再充电门限 VREG−100/−200 mV |
| `set_topoff_timer` | 终止后补足计时 off/15/30/45 min |
| `set_trickle_current` | 深放电涓流 90/30 mA |
| `set/get_sys_min_voltage` | NVDC 系统最低电压 2600–3700 mV |
| `set_min_vbat_otg` | OTG 启动最低电池电压 2.95/2.6 V |

### 输入管理
| 函数 | 说明 |
|---|---|
| `set/get_iindpm` | 输入限流 100–3200 mA、100 mA 步进（**检测完成后会被硬件覆盖**） |
| `set_vindpm_os` | 输入欠压档位 3.9/5.9/7.5/10.5 V 基准 |
| `set/get_vindpm` | 欠压门限绝对 mV（按当前 OS 档换算，窗口宽 1.5 V） |
| `set_vindpm_bat_track` | 跟随 VBAT+200/250/300 mV（仅 OS=3.9 V 有效） |
| `set_input_ovp` | 输入过压 5.5/6.5/10.5/14 V |
| `set_hiz` | 高阻模式（停止变换，电池漏电 ~8.5 µA） |
| `force_input_detection` / `input_detect_done` | 强制重检测 / 查询检测完成（REG0E） |
| `set_pfm_enable` / `set_q1_fullon` | 轻载 PFM / 输入 FET 常通 |
| `set_dpm_int_mask` | 屏蔽 VINDPM/IINDPM 的 nINT 脉冲 |

### OTG 反向升压（`SGM41513_USE_OTG`）
| 函数 | 说明 |
|---|---|
| `otg_enable(dev, en)` | 使能前自动清 HIZ 并延时 ≥30 ms（手册要求） |
| `set_boost_voltage` | 4.85/5.0/5.15/5.3 V |
| `set_boost_current_limit` | 0.5 / 1.2 A |
| `set_boost_freq` | 1.5 MHz（默认）/ 500 kHz |
| `set_iterm_x6` | ITERM×6（ICHG>300 mA 时提升大电流充电终止精度） |

### 状态 / 故障
| 函数 | 说明 |
|---|---|
| `get_status` | REG08：输入类型（按变体解码）、充电阶段、PG、热调节、VSYS 调节 |
| `get_faults` | REG09 **自动双读取实时值**（NTC 位实时），读取同时清除锁存历史 |
| `get_dpm_status` | REG0A：VBUS 有效、VINDPM/IINDPM 调节中、topoff、输入过压 |

### JEITA（`SGM41513_USE_JEITA`）
| 函数 | 说明 |
|---|---|
| `jeita_configure(dev, *cfg)` | 一次配置冷/热阈值（T2/T3）、冷区电流与电压策略、热区电流与电压策略 |

### 看门狗 / 定时器 / 热调节
| 函数 | 说明 |
|---|---|
| `set_watchdog` | off/40/80/160 s（上电默认 160 s） |
| `feed_watchdog` | 喂狗（WD_RST 写 1 自清） |
| `set_safety_timer` / `set_safety_timer_slow2x` | 安全定时器 7/16 h 与使能 / DPM·JEITA 冷区·热调节时半速 |
| `set_thermal_reg_threshold` | 降频阈值 80 / 120 °C |

### 船运模式 / PUMPX / STAT / D+D− / 原始访问
| 函数 | 说明 |
|---|---|
| `enter_ship_mode(dev, delayed)` | BATFET 断开（~2.5 µA），可延迟 ~12 s |
| `set_batfet_reset_enable` | nQON 长按 10 s 复位功能 |
| `pumpx_enable / trigger_up / trigger_down / busy` | 可调适配器电流脉冲调压协议 |
| `set_stat_pin_mode / set_stat_pin_pattern` | STAT 引脚：跟随充电态 / 手动常亮·闪烁 / 关闭 |
| `set_dpdm_voltage` | 手动驱动 D+/D− 电平（A/D 变体，HIZ/0/0.6/3.3 V） |
| `read_reg / write_reg / update_bits` | 寄存器级原始读写（调试用） |

## 5. 寄存器速查

| 地址 | 内容 | POR | 备注 |
|---|---|---|---|
| 0x00 | EN_HIZ, EN_ICHG_MON, IINDPM[4:0] | 0x17 | IINDPM=100+100n mA，默认 2400 mA |
| 0x01 | PFM_DIS, WD_RST, OTG_CONFIG, CHG_CONFIG, SYS_MIN, MIN_BAT_SEL | 0x1A | 写过任意寄存器即进入 host 模式 |
| 0x02 | BOOST_LIM, Q1_FULLON, ICHG[5:0] | 0xB4 | ICHG 非线性 64 档，默认 1980 mA |
| 0x03 | IPRECHG[3:0], ITERM[3:0] | 0xAA | 共用 16 档表，默认各 120 mA |
| 0x04 | VREG[4:0], TOPOFF_TIMER, VRECHG | 0x58 | VREG=3856+32n（n=15 为 4350），默认 4.208 V |
| 0x05 | EN_TERM, ITERM_TIMER, WATCHDOG, EN_TIMER, CHG_TIMER, TREG, JEITA_ISET_L | 0xBF | 看门狗默认 160 s |
| 0x06 | OVP, BOOSTV, VINDPM[3:0] | 0xE6 | VINDPM=OS+100n，默认 4.5 V |
| 0x07 | IINDET_EN, TMR2X_EN, BATFET_DIS, JEITA_VSET_H, BATFET_DLY, BATFET_RST_EN, VDPM_BAT_TRACK | 0x4C | |
| 0x08 | (RO) VBUS_STAT, CHRG_STAT, PG_STAT, THERM_STAT, VSYS_STAT | — | VBUS_STAT 编码随变体不同 |
| 0x09 | (RO) 看门狗/升压/充电/电池/NTC 故障 | — | **锁存需读两次；禁止 burst** |
| 0x0A | (RO) DPM 状态 + (R/W) INT 屏蔽位 | — | |
| 0x0B | (W) REG_RST, (RO) PN, SGMPART, DEV_REV | — | PN=0000 为基础版，0001 为 A/D |
| 0x0C | JEITA 全部配置 | 0x75 | |
| 0x0D | PUMPX, DP/DM_VSET, OTGF_ITREMR | 0x01 | OTGF_ITREMR 位双语义（升压频率 / ITERM×6） |
| 0x0E | (RO) INPUT_DET_DONE | — | **禁止 burst** |
| 0x0F | VREG_FT, ISHORT_SET, STAT_SET, VINDPM_OS | 0x00 | |

## 6. 已知坑（来自数据手册，驱动已处理或需注意）

1. **REG09/REG0E 禁止 burst 访问** —— 驱动只做单字节读写，移植层无需块传输。
2. **REG09 故障位锁存** —— `get_faults()` 内部连读两次、取第二次，返回实时故障；
   读取动作本身会清除锁存的历史故障。NTC_FAULT 始终实时。
3. **适配器类型检测会覆盖 IINDPM** —— VBUS 插入后 PSEL/D+/D− 检测完成时硬件
   自动改写 IINDPM。应在 `input_detect_done()` 为真后重新 `set_iindpm()`。
4. **host 模式与看门狗** —— POR 后芯片运行在自主模式；任何一次 I2C 写寄存器
   都会切换到 host 模式，此后必须周期 `feed_watchdog()` 或显式关闭看门狗。
   看门狗超时会把 R/W 寄存器恢复默认值（IINDPM/VINDPM/VINDPM_OS/BATFET_DLY/
   BATFET_DIS 除外）——检测到 `watchdog_fault` 后调用 `restore_settings()` 恢复。
5. **充电使能三条件** —— `CHG_CONFIG=1` 且外部 **nCE 引脚为低** 且 `ICHG≠0`。
6. **OTG 时序** —— EN_HIZ 优先于 OTG，须先清 HIZ 并等待 ≥30 ms 再置 OTG_CONFIG，
   `otg_enable()` 已内置该时序；升压输出建立另需 tOTG_ON，可轮询
   `get_status()` 的 `vbus_type == SGM41513_VBUS_OTG` 确认。
7. **船运模式退出后 EN_HIZ 会被硬件置 1** —— 退出后调用 `restore_settings()`
   可一并恢复 HIZ 与全部配置。
8. **A 与 D 变体 I2C 无法区分**（PN 同为 0001），VBUS_STAT 编码不同，
   由 `SGM41513_VARIANT` 配置指定；基础版（PN=0000）自动识别。
9. **I2C 有效条件** —— VVBUS > ~3.3 V 或 VBAT > 2.65 V，否则芯片不响应。

## 7. 文件清单

| 文件 | 说明 |
|---|---|
| `sgm41513.h` | 公共 API（类型 + 全部函数声明） |
| `sgm41513.c` | 驱动核心（唯一需要参与编译的 .c，外加你的移植层） |
| `sgm41513_regs.h` | 全部寄存器位域定义（调试/扩展用） |
| `sgm41513_conf.h` | 功能裁剪与配置 |
| `sgm41513_io.h` | 移植接口声明（4 个函数） |
| `port/sgm41513_io_template.c` | 移植模板（含 STM32 HAL / ESP-IDF 注释示例） |
