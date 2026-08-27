/*
 * @file    ina219.h
 * @brief   INA219 电流/功率监测芯片通用驱动公共 API
 * @details 器件：TI INA219A/INA219B（零漂移电流监测，I2C/SMBus 接口，
 *          7 位地址 0x40~0x4F 共 16 个，最高 400kHz 快速模式，16 位
 *          寄存器，MSB-8 / SOT-23-8 封装）。监测范围：总线电压 0~26V
 *          （量程档 16V/32V），分流电压 ±40/±80/±160/±320mV 四档 PGA，
 *          内部乘法器按校准值输出电流与功率。
 *
 *          分层架构（详见 README.md）：
 *
 *            +--------------------------------------+
 *            |  应用层                               |  用户代码
 *            +--------------------------------------+
 *                        |  本 API（ina219.h）
 *            +--------------------------------------+
 *            |  驱动核心  ina219.c                    |  纯 C99、零平台代码
 *            |  配置      ina219_conf.h               |  无动态内存
 *            +--------------------------------------+
 *                        |  3~6 个 io 函数（ina219_io.h）
 *            +--------------------------------------+
 *            |  移植层（用户提供）                     |  STM32/CH32/ESP32/
 *            |                                        |  Linux/RTOS/模拟 I2C
 *            +--------------------------------------+
 *
 *          移植 = 实现 ina219_io.h + 按需调整 ina219_conf.h，
 *          核心文件零改动。
 *
 *          单位约定（贯穿全部 API）：分流电压 µV，总线电压 mV，
 *          电流 µA，功率 µW，采样电阻 µΩ，Current_LSB nA。
 *          全部定点运算，无浮点。
 * @note    set 类函数按硬件档位就近取整并钳位到支持范围，对应 get
 *          返回实际生效值。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 */

#ifndef INA219_H
#define INA219_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ina219_conf.h"
#include "ina219_regs.h"  /* 寄存器地址与位定义（也开放寄存器级访问） */

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 结果码（全部公共 API 的统一返回类型）
 * ══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    INA219_OK = 0,             /**< 成功 */
    INA219_ERR_IO,             /**< I2C 通信失败（移植层返回 ERROR 时上抛） */
    INA219_ERR_PARAM,          /**< 参数非法（空指针、超范围） */
    INA219_ERR_NOT_READY,      /**< 未初始化 / 器件无应答 */
    INA219_ERR_NOT_SUPPORTED,  /**< 功能被 conf 裁剪或器件不支持 */
    INA219_ERR_VERIFY,         /**< 写后回读不一致（INA219_VERIFY_WRITES） */
    INA219_ERR_TIMEOUT,        /**< 等待转换完成超时 */
} ina219_result_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 工作模式（枚举值 = 配置寄存器 MODE 字段编码，Table 6）
 * ══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    INA219_MODE_POWER_DOWN_E     = 0,  /**< 掉电（挂起），唤醒需重设模式 */
    INA219_MODE_TRIG_SHUNT_E     = 1,  /**< 分流电压，触发单次 */
    INA219_MODE_TRIG_BUS_E       = 2,  /**< 总线电压，触发单次 */
    INA219_MODE_TRIG_SHUNT_BUS_E = 3,  /**< 分流+总线，触发单次 */
    INA219_MODE_ADC_OFF_E        = 4,  /**< ADC 关闭（I2C 保持可达） */
    INA219_MODE_CONT_SHUNT_E     = 5,  /**< 分流电压，连续转换 */
    INA219_MODE_CONT_BUS_E       = 6,  /**< 总线电压，连续转换 */
    INA219_MODE_CONT_SHUNT_BUS_E = 7,  /**< 分流+总线，连续转换（默认） */
} ina219_mode_t;

/* ══════════════════════════════════════════════════════════════════════════
 * 设备句柄（每片芯片一个，由调用者分配）
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void    *io_ctx;          /**< 总线上下文，原样透传给 io 函数（多总线
                                   设计用，单总线可为 NULL） */
    uint8_t dev_addr;         /**< 7 位 I2C 地址，通常填 INA219_I2C_ADDR(0x40) */
    uint8_t inited;           /**< ina219_init() 置 1，复位后清 0 */
    uint32_t shunt_uohms;     /**< 当前校准使用的采样电阻 µΩ（换算电流用） */
    uint32_t current_lsb_na;  /**< 当前校准使用的 Current_LSB nA（换算电流/功率用） */
    uint16_t cal_reg;         /**< 已写入器件的校准寄存器值 */
} ina219_dev_t;

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 1. 初始化 / 复位
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   初始化设备句柄并配置器件
 * @details 调用 ina219_io_init() 一次；读配置寄存器确认器件应答；
 *          按 conf 默认值整字写入配置寄存器（量程/PGA/ADC/模式），
 *          并按 INA219_SHUNT_UOHMS × INA219_CURRENT_LSB_NA 计算写入
 *          校准寄存器。器件寄存器为易失性，每次上电后必须重新 init。
 * @param   dev       设备句柄，调用者分配。
 * @param   io_ctx    总线上下文，透传给 io 函数，可为 NULL。
 * @param   dev_addr  7 位 I2C 地址，填 INA219_I2C_ADDR 或 A1/A0 引脚
 *                    组合对应的 0x40~0x4F。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针或校准参数超界；
 *          ERR_IO 通信失败（含器件无应答）。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_init(ina219_dev_t *dev, void *io_ctx,
                            uint8_t dev_addr);

/**
 * @brief   软件全复位（配置寄存器 RST 位置 1）
 * @details 效果等同上电复位：全部寄存器回到 POR 默认值，RST 位自清零。
 *          复位后配置与校准全部丢失，本函数同时清除句柄初始化状态，
 *          必须重新调用 ina219_init() 后方可继续使用其它 API。
 * @param   dev  设备句柄。
 * @retval  ina219_result_t  OK 命令已发出；ERR_PARAM 空指针；
 *          ERR_NOT_READY 未初始化；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_reset(ina219_dev_t *dev);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 2. 量程 / ADC / 工作模式配置
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   设置总线电压量程（配置寄存器 BRNG 位）
 * @details 更改后总线电压满量程切换（16V 或 32V），分辨率固定 4mV。
 * @param   dev      设备句柄。
 * @param   range32v true 选 32V 量程，false 选 16V 量程。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_set_bus_range(ina219_dev_t *dev, bool range32v);

/**
 * @brief   读取当前总线电压量程设置
 * @param   dev      设备句柄。
 * @param   range32v 输出：true 表示 32V 量程。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_get_bus_range(ina219_dev_t *dev, bool *range32v);

/**
 * @brief   设置分流电压 PGA 满量程档位
 * @details 输入 mV 就近取整到 ±40/±80/±160/±320mV 四档。档位决定电流
 *          测量满量程：量程(mA) = 档位(mV) / 采样电阻(Ω)；1Ω 下
 *          ±320mV 即 ±320mA。
 * @param   dev  设备句柄。
 * @param   mv   期望满量程 mV，就近档位生效。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_set_pga_range(ina219_dev_t *dev, uint16_t mv);

/**
 * @brief   读取当前分流电压 PGA 满量程档位
 * @param   dev  设备句柄。
 * @param   mv   输出：生效档位（40/80/160/320）。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_get_pga_range(ina219_dev_t *dev, uint16_t *mv);

/**
 * @brief   设置总线/分流 ADC 分辨率与平均次数档位
 * @details 档位取 ina219_regs.h 中的 INA219_ADC_* 宏（0x0~0x3 = 9~12bit
 *          单次，0x9~0xF = 12bit×2~128 次平均）；转换时间 84µs~68.1ms。
 *          平均档位可平滑噪声，代价是更新周期加长。
 * @param   dev   设备句柄。
 * @param   badc  总线 ADC 档位（INA219_ADC_* 宏值）。
 * @param   sadc  分流 ADC 档位（INA219_ADC_* 宏值）。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针或档位非法；
 *          ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_set_adc(ina219_dev_t *dev, uint8_t badc, uint8_t sadc);

/**
 * @brief   读取当前 ADC 档位
 * @param   dev   设备句柄。
 * @param   badc  输出：总线 ADC 档位（INA219_ADC_* 宏值）。
 * @param   sadc  输出：分流 ADC 档位。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_get_adc(ina219_dev_t *dev, uint8_t *badc, uint8_t *sadc);

/**
 * @brief   设置工作模式（连续 / 触发 / 掉电 / ADC 关闭）
 * @details 写入 MODE 字段会清除 CNVR 转换完成标志并在触发档启动一次
 *          新转换。掉电模式下典型静态电流约 5µA，唤醒只需重设模式。
 * @param   dev   设备句柄。
 * @param   mode  目标模式（ina219_mode_t 枚举）。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针或模式非法；
 *          ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_set_mode(ina219_dev_t *dev, ina219_mode_t mode);

/**
 * @brief   读取当前工作模式
 * @param   dev   设备句柄。
 * @param   mode  输出：当前模式。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_get_mode(ina219_dev_t *dev, ina219_mode_t *mode);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 3. 校准（电流 / 功率换算基准）
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   设置电流校准（采样电阻 + Current_LSB）
 * @details 按手册公式 Cal = trunc(0.04096 / (Current_LSB × R_SHUNT))
 *          计算并写入校准寄存器，同时把两个参数缓存进句柄供
 *          电流/功率换算。校准寄存器为 15 位有效（最大 0x7FFE），
 *          参数组合使校准值超界时返回 ERR_PARAM——1Ω 电阻下
 *          Current_LSB 下限约 1.25µA。
 *          校准写入前电流/功率寄存器恒为 0。
 * @param   dev            设备句柄。
 * @param   shunt_uohms    采样电阻阻值 µΩ（如 1Ω 传 1000000）。
 * @param   current_lsb_na Current_LSB，单位 nA（如 10µA 传 10000）。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针、参数为 0 或
 *          校准值超界；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_set_calibration(ina219_dev_t *dev,
                                       uint32_t shunt_uohms,
                                       uint32_t current_lsb_na);

/**
 * @brief   读取当前校准参数（回读器件校准寄存器 + 句柄缓存）
 * @param   dev            设备句柄。
 * @param   shunt_uohms    输出：句柄中的采样电阻 µΩ。
 * @param   current_lsb_na 输出：句柄中的 Current_LSB nA。
 * @param   cal_reg        输出：器件校准寄存器当前值（bit0 恒 0）。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_get_calibration(ina219_dev_t *dev,
                                       uint32_t *shunt_uohms,
                                       uint32_t *current_lsb_na,
                                       uint16_t *cal_reg);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 4. 测量读取（全部定点换算，无浮点）
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读分流电压，单位 µV
 * @details 换算：µV = (int16)原始值 × 10（LSB 10µV，负数二进制补码，
 *          各 PGA 档通用——符号扩展设计使 16 位补码直接左乘即可）。
 * @param   dev  设备句柄。
 * @param   uv   输出：分流电压 µV，量程 ±320000µV（/8 档）。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_shunt_voltage(ina219_dev_t *dev, int32_t *uv);

/**
 * @brief   读分流电压原始值（16 位二进制补码）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，µV = (int16)raw × 10。
 * @retval  ina219_result_t  同 ina219_read_shunt_voltage()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_shunt_raw(ina219_dev_t *dev, int16_t *raw);

/**
 * @brief   读总线电压，单位 mV
 * @details 寄存器值左对齐：mV = (原始值 >> 3) × 4，低位 CNVR/OVF
 *          标志位不参与换算。总线电压在 IN- 引脚（负载侧）测量。
 * @param   dev  设备句柄。
 * @param   mv   输出：总线电压 mV（32V 档满量程 32000mV）。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_bus_voltage(ina219_dev_t *dev, uint16_t *mv);

/**
 * @brief   读总线电压原始值（含 CNVR/OVF 标志位）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始 16 位值，mV = (raw >> 3) × 4，
 *               bit1 = CNVR，bit0 = OVF。
 * @retval  ina219_result_t  同 ina219_read_bus_voltage()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_bus_raw(ina219_dev_t *dev, uint16_t *raw);

/**
 * @brief   读电流，单位 µA（依赖已写入的校准值）
 * @details 换算：µA = 原始值(int16) × Current_LSB(nA) / 1000，
 *          四舍五入，64 位中间量防溢出。正方向为 IN+ 流向 IN-。
 * @param   dev  设备句柄。
 * @param   ua   输出：电流 µA，正充负放由接线方向决定。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    未校准时器件电流寄存器恒 0；线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_current(ina219_dev_t *dev, int32_t *ua);

/**
 * @brief   读电流原始值（16 位二进制补码）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，µA = raw × Current_LSB(nA) / 1000。
 * @retval  ina219_result_t  同 ina219_read_current()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_current_raw(ina219_dev_t *dev, int16_t *raw);

#if INA219_USE_POWER
/**
 * @brief   读功率，单位 µW（依赖已写入的校准值）
 * @details 换算：µW = 原始值 × Current_LSB(nA) × 20 / 1000，四舍五入
 *          （功率 LSB 恒为电流 LSB 的 20 倍）。注意：读功率寄存器会
 *          硬件清除 CNVR 转换完成标志。
 * @param   dev  设备句柄。
 * @param   uw   输出：功率 µW。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    未校准时器件功率寄存器恒 0；线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_power(ina219_dev_t *dev, uint32_t *uw);

/**
 * @brief   读功率原始值（16 位无符号）
 * @param   dev  设备句柄。
 * @param   raw  输出：原始值，µW = raw × Current_LSB(nA) × 20 / 1000。
 * @retval  ina219_result_t  同 ina219_read_power()。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_power_raw(ina219_dev_t *dev, uint16_t *raw);
#endif /* INA219_USE_POWER */

/**
 * @brief   查询转换完成标志（总线电压寄存器 CNVR 位）
 * @details CNVR 在全部转换、平均与乘法完成后置位；写 MODE 字段或读
 *          功率寄存器会清除。连续模式下可用该位判断数据新鲜度，
 *          触发模式下与 ina219_wait_conversion() 配合使用。
 * @param   dev    设备句柄。
 * @param   ready  输出：true 表示最近一次转换已完成。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_is_conversion_ready(ina219_dev_t *dev, bool *ready);

/**
 * @brief   查询数学溢出标志（总线电压寄存器 OVF 位）
 * @details OVF 置位表示电流/功率乘法结果超出寄存器范围，电流与功率
 *          数据可能无意义（分流电压与总线电压本身仍有效），应增大
 *          Current_LSB 或降低量程后重新校准。
 * @param   dev  设备句柄。
 * @param   ovf  输出：true 表示发生溢出。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_is_math_overflow(ina219_dev_t *dev, bool *ovf);

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 5. 触发转换与等待（INA219_USE_TRIGGERED=1 时提供）
 * ══════════════════════════════════════════════════════════════════════════ */

#if INA219_USE_TRIGGERED
/**
 * @brief   触发一次新转换（触发模式使用）
 * @details 读回当前配置寄存器并整字重写：写 MODE 字段清除 CNVR 并在
 *          触发档位（MODE 0x1~0x3）启动一次新转换；连续档位下等效于
 *          重启连续转换。触发后用 ina219_wait_conversion() 等完成。
 * @param   dev  设备句柄。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_trigger(ina219_dev_t *dev);

/**
 * @brief   阻塞等待转换完成（带超时，触发模式使用）
 * @details 每 INA219_WAIT_POLL_MS 毫秒查询一次总线电压寄存器 CNVR 位，
 *          置位即返回 OK；累计等待超过 timeout_ms 仍未见置位则返回
 *          ERR_TIMEOUT。超时时间应覆盖 BADC+SADC 档位总转换时间
 *          （最长 2×68.1ms ≈ 140ms）。
 * @param   dev         设备句柄。
 * @param   timeout_ms  最长等待毫秒数（0 表示只查询一次不等待）。
 * @retval  ina219_result_t  OK 转换完成；ERR_TIMEOUT 超时；
 *          ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用（内部走 io 延时）。
 */
ina219_result_t ina219_wait_conversion(ina219_dev_t *dev, uint32_t timeout_ms);
#endif /* INA219_USE_TRIGGERED */

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 6. 寄存器级原始访问（调试 / 覆盖未封装功能）
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读 16 位寄存器原始值
 * @param   dev  设备句柄。
 * @param   reg  寄存器地址（0x00~0x05）。
 * @param   val  输出：16 位原始值。
 * @retval  ina219_result_t  OK 成功；ERR_PARAM 空指针；ERR_IO 通信失败。
 * @note    线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_read_reg(ina219_dev_t *dev, uint8_t reg,
                                uint16_t *val);

/**
 * @brief   写 16 位寄存器原始值
 * @param   dev  设备句柄。
 * @param   reg  寄存器地址。
 * @param   val  待写 16 位值。
 * @retval  ina219_result_t  OK 成功；ERR_IO 通信失败。
 * @note    写只读地址（0x01~0x04）被器件忽略；线程上下文调用，
 *          禁止在 ISR 中调用。
 */
ina219_result_t ina219_write_reg(ina219_dev_t *dev, uint8_t reg, uint16_t val);

/**
 * @brief   读-改-写：向 reg 写入 (val & mask)
 * @param   dev   设备句柄。
 * @param   reg   寄存器地址。
 * @param   mask  保留位掩码（0 的位保持原值）。
 * @param   val   新字段值（已位于目标位位置）。
 * @retval  ina219_result_t  OK 成功；ERR_IO 通信失败。
 * @note    对配置寄存器操作时注意：写 MODE 字段会清 CNVR 并可能触发
 *          新转换；线程上下文调用，禁止在 ISR 中调用。
 */
ina219_result_t ina219_update_bits(ina219_dev_t *dev, uint8_t reg,
                                   uint16_t mask, uint16_t val);

#ifdef __cplusplus
}
#endif

#endif /* INA219_H */
