# DRV2605L 通用触觉电机驱动

适用于 TI DRV2605L 低电压 ERM/LRA 触觉驱动器。驱动依据
[DRV2605L 数据手册 Rev. D](https://www.ti.com/lit/ds/symlink/drv2605l.pdf)，
采用纯 C99、无动态内存、无平台头文件、可多实例的 I2C 寄存器架构。

| 项目 | 说明 |
|---|---|
| 芯片 | DRV2605L，支持 ERM 与 LRA |
| 接口 | I2C，固定 7 位地址 0x5A，最高 400 kHz |
| 电源 | VDD 2.0–5.2 V |
| 核心功能 | ROM 波形序列、RTP 实时幅度、模式/待机/执行器/库选择 |
| 高级功能 | 诊断、自动校准、PWM/模拟、Audio-to-vibe、OTP 通过原始寄存器完成 |
| 语言 | C99；不使用 malloc/free、VLA、递归或编译器扩展 |

## 1. 分层架构

~~~text
+---------------------------------------------+
| 应用层                                       | 用户代码
+---------------------------------------------+
                    | drv2605l.h
+---------------------------------------------+
| 驱动核心 drv2605l.c                          | 纯 C99
| 配置 drv2605l_conf.h / 寄存器 drv2605l_regs.h |
+---------------------------------------------+
                    | drv2605l_io.h
+---------------------------------------------+
| 移植层 port/drv2605l_io_template.c           | 仅此处接触平台 I2C
+---------------------------------------------+
                    |
| STM32 / CH32 / ESP32 / Linux / 模拟 I2C      |
+---------------------------------------------+
~~~

核心只调用 drv2605l_io.h 的四个必选函数，不知道 I2C 控制器、HAL、
RTOS 或 GPIO 类型。EN 和 IN/TRIG 不属于 I2C 契约，由板级代码管理。

## 2. 移植步骤

1. 将 drv2605l.h、drv2605l.c、drv2605l_conf.h、
   drv2605l_regs.h、drv2605l_io.h 加入目标工程。
2. 复制 port/drv2605l_io_template.c，实选四个必选函数：
   drv2605l_io_init()、drv2605l_io_read_reg()、
   drv2605l_io_write_reg()、drv2605l_io_delay_ms()。
3. 确保总线使用 7 位地址 0x5A，或在移植层转换为平台要求的地址格式。
   每次 I2C 事务都必须有有限超时。
4. 上电后将 EN 拉高；若只使用 I2C 触发，IN/TRIG 可接地。
5. 按需用编译器 -D 覆盖 drv2605l_conf.h 中的默认配置。

## 3. 配置项

| 宏 | 默认值 | 说明 |
|---|---:|---|
| DRV2605L_USE_SEQUENCE | 1 | ROM 波形序列 API；置 0 后相关 API 消失 |
| DRV2605L_USE_RTP | 1 | RTP 实时播放 API；置 0 后相关 API 消失 |
| DRV2605L_DEFAULT_ACTUATOR | 0 | 0=ERM，1=LRA |
| DRV2605L_DEFAULT_LIBRARY | 1 | 默认 TS2200 Library A；LRA 应配置为 6 |
| DRV2605L_DEFAULT_MODE | 0 | 默认内部触发模式 |
| DRV2605L_DEFAULT_STANDBY | 1 | 初始化最后进入软件待机 |
| DRV2605L_VERIFY_WRITES | 0 | 类型化持久寄存器写入后回读校验 |
| DRV2605L_THREAD_SAFE | 0 | 置 1 后要求实现 lock/unlock 钩子 |

功能裁剪采用编译期消失策略：关闭开关后，对应 API 不再声明，调用会在
编译期报错。原始寄存器 API 始终保留。

## 4. 使用片段

~~~c
#include "drv2605l.h"

static drv2605l_dev_t haptic;
static const uint8_t sequence[DRV2605L_SEQUENCE_SLOTS] =
{
    1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
};

void app_haptic_init(void)
{
    if (drv2605l_init(&haptic, NULL, 0u) != DRV2605L_OK)
    {
        return;
    }

    drv2605l_set_sequence(&haptic, sequence);
    drv2605l_play_sequence(&haptic);
}

void app_haptic_rtp(void)
{
    drv2605l_set_rtp_format(&haptic, false);
    drv2605l_start_rtp(&haptic, 0x50u);
    drv2605l_set_rtp_input(&haptic, 0x20u);
    drv2605l_stop_rtp(&haptic);
}
~~~

RTP_INPUT 是无物理单位的 8 位码：默认按有符号补码解释，也可设置为
无符号 0–255。ROM 序列中的等待项单位为 10 ms/LSB；波形编号范围为
1–123，0 终止序列。

## 5. API 参考

所有公共函数返回 drv2605l_result_t：

| 结果码 | 语义 |
|---|---|
| DRV2605L_OK | 成功 |
| DRV2605L_ERR_IO | I2C 通信失败 |
| DRV2605L_ERR_PARAM | 空指针、地址或参数非法 |
| DRV2605L_ERR_NOT_READY | 未初始化或复位后未重新初始化 |
| DRV2605L_ERR_NOT_SUPPORTED | 器件 ID 不匹配或功能不支持 |
| DRV2605L_ERR_VERIFY | 启用写校验时回读不一致 |

| 功能 | API |
|---|---|
| 生命周期 | drv2605l_init()、drv2605l_reset() |
| 原始寄存器 | drv2605l_read_reg()、drv2605l_write_reg() |
| 通用配置 | set/get_mode()、set/get_standby()、set/get_actuator()、set/get_library() |
| RTP | start_rtp()、set/get_rtp_input()、stop_rtp()、set/get_rtp_format() |
| ROM 序列 | set/get_sequence()、play_sequence()、stop_sequence()、is_sequence_playing() |

### 原始寄存器和高级功能

drv2605l_regs.h 包含 0x00–0x22 全部寄存器地址和字段掩码。驱动不提供
诊断、自动校准、PWM/模拟、Audio-to-vibe 或 OTP 的高层流程 API；应用可以
使用原始读写接口自行实现，但必须遵守数据手册规定的模式、GO 位、等待时间
和 OTP 一次性编程条件。

## 6. 线程安全与 ISR 约束

默认模型为：不同设备实例的句柄状态彼此独立；同一实例不能并发调用，若
共享 I2C 总线还必须满足总线自身的并发要求。置
DRV2605L_THREAD_SAFE=1 后，核心会通过 drv2605l_io_lock() /
drv2605l_io_unlock() 保护读-改-写和多笔事务。

init()、reset() 与运行期 API 的并发不受支持，由调用者保证生命周期顺序。
驱动公共 API 不适用于 ISR；ISR 只能置事件标志，随后在任务或主循环中调用
I2C API。

## 7. 已知坑

1. 上电后至少等待 250 µs 才能接受 I2C 命令；核心使用至少 1 ms 的移植层延时。
2. EN 必须为高才能按初始化流程工作；I2C-only 设计可将 IN/TRIG 接地。
3. IN/TRIG 外部触发、PWM、模拟和音频模式均依赖外部信号，核心不会配置
   GPIO 或产生波形。
4. MODE=5 为 RTP；切换到 RTP 前应先写入 RTP_INPUT，停止时切回其它模式。
5. Status 中的 DIAG_RESULT、OVER_TEMP 和 OC_DETECT 属于状态/清除语义，读取
   可能清除标志，不能把它当作普通影子寄存器。
6. 执行器切换不会自动切换 ROM 库；LRA 通常使用 Library 6。TS2200
   Library A 带有固定 overdrive 编程，不应随意用于闭环双向模式。
7. GO 既用于 ROM 播放，也用于诊断和自动校准；播放完成后会自行清零，
   写 0 可取消当前序列。
8. 诊断和自动校准的完成判断必须轮询 GO，并在数据手册规定的最大时间内
   退出；本精简驱动不替应用实现阻塞等待。
9. VDD 旁路和 REG 引脚电容、OUT± 走线及执行器匹配属于硬件设计责任；应按
   数据手册的推荐布局和电容要求执行。
