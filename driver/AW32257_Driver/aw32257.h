/**
 * @file    aw32257.h
 * @brief   AW32257 充电与升压芯片的可移植 C99 通用驱动
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * @details
 * 驱动不持有任何硬件资源。调用者提供带超时保护的寄存器读写回调、
 * 毫秒级延时回调，以及每个驱动实例的存储空间。核心不可重入，
 * 禁止在中断服务程序(ISR)中调用。
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef AW32257_H
#define AW32257_H

#include <stdbool.h>
#include <stdint.h>

#include "aw32257_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AW32257_DRIVER_VERSION_MAJOR          1
#define AW32257_DRIVER_VERSION_MINOR          0
#define AW32257_DRIVER_VERSION_PATCH          0

/** @brief 驱动结果码。 */
typedef enum
{
    AW32257_OK                         =  0,
    AW32257_ERR_NULL_POINTER           = -1,
    AW32257_ERR_INVALID_ARGUMENT       = -2,
    AW32257_ERR_NOT_BOUND              = -3,
    AW32257_ERR_NOT_INITIALIZED        = -4,
    AW32257_ERR_RANGE                  = -5,
    AW32257_ERR_IO                     = -6,
    AW32257_ERR_ID_MISMATCH            = -7,
    AW32257_ERR_SAFETY_MISMATCH        = -8,
    AW32257_ERR_STATE                  = -9,
    AW32257_ERR_POR_REQUIRED           = -10
} aw32257_status_t;

/** @brief 驱动实例的本地生命周期状态。 */
typedef enum
{
    AW32257_LIFECYCLE_UNBOUND = 0,
    AW32257_LIFECYCLE_BOUND,
    AW32257_LIFECYCLE_READY,
    AW32257_LIFECYCLE_POR_REQUIRED
} aw32257_lifecycle_t;

#include "aw32257_io.h"

/**
 * @brief 快充电流与安全限流寄存器码点。
 *
 * 实际物理电流取决于外部检流电阻。仅当设计确认使用 33 mOhm
 * 检流电阻时才可使用 aw32257_current_code_to_ma_33mohm() 换算。
 */
typedef enum
{
    AW32257_CURRENT_CODE_00 = 0x00,
    AW32257_CURRENT_CODE_01 = 0x01,
    AW32257_CURRENT_CODE_02 = 0x02,
    AW32257_CURRENT_CODE_03 = 0x03,
    AW32257_CURRENT_CODE_04 = 0x04,
    AW32257_CURRENT_CODE_05 = 0x05,
    AW32257_CURRENT_CODE_06 = 0x06,
    AW32257_CURRENT_CODE_07 = 0x07,
    AW32257_CURRENT_CODE_08 = 0x08,
    AW32257_CURRENT_CODE_09 = 0x09,
    AW32257_CURRENT_CODE_0A = 0x0A,
    AW32257_CURRENT_CODE_0B = 0x0B,
    AW32257_CURRENT_CODE_0C = 0x0C,
    AW32257_CURRENT_CODE_0D = 0x0D,
    AW32257_CURRENT_CODE_0E = 0x0E,
    AW32257_CURRENT_CODE_0F = 0x0F
} aw32257_current_code_t;

/** @brief 充电终止电流寄存器码点。 */
typedef enum
{
    AW32257_TERM_CURRENT_CODE_0 = 0,
    AW32257_TERM_CURRENT_CODE_1 = 1,
    AW32257_TERM_CURRENT_CODE_2 = 2,
    AW32257_TERM_CURRENT_CODE_3 = 3,
    AW32257_TERM_CURRENT_CODE_4 = 4,
    AW32257_TERM_CURRENT_CODE_5 = 5,
    AW32257_TERM_CURRENT_CODE_6 = 6,
    AW32257_TERM_CURRENT_CODE_7 = 7
} aw32257_term_current_code_t;

/** @brief 通过 REG01 请求的工作模式。外部引脚可能覆盖该请求。 */
typedef enum
{
    AW32257_MODE_CHARGE = 0,
    AW32257_MODE_HIGH_IMPEDANCE,
    AW32257_MODE_BOOST
} aw32257_mode_t;

/** @brief REG00 上报的充电状态。 */
typedef enum
{
    AW32257_CHARGE_STATE_READY       = 0,
    AW32257_CHARGE_STATE_IN_PROGRESS = 1,
    AW32257_CHARGE_STATE_DONE        = 2,
    AW32257_CHARGE_STATE_FAULT       = 3
} aw32257_charge_state_t;

/** @brief REG00 上报的充电故障。 */
typedef enum
{
    AW32257_CHARGE_FAULT_NORMAL                  = 0,
    AW32257_CHARGE_FAULT_VBUS_OVP                = 1,
    AW32257_CHARGE_FAULT_SLEEP                   = 2,
    AW32257_CHARGE_FAULT_BAD_ADAPTER_OR_VBUS_UVLO = 3,
    AW32257_CHARGE_FAULT_BATTERY_OVP             = 4,
    AW32257_CHARGE_FAULT_THERMAL_SHUTDOWN        = 5,
    AW32257_CHARGE_FAULT_RESERVED_6              = 6,
    AW32257_CHARGE_FAULT_NO_BATTERY              = 7
} aw32257_charge_fault_t;

/** @brief REG09 上报的升压(boost)故障。 */
typedef enum
{
    AW32257_BOOST_FAULT_NORMAL              = 0,
    AW32257_BOOST_FAULT_VBUS_OVP            = 1,
    AW32257_BOOST_FAULT_OVERLOAD            = 2,
    AW32257_BOOST_FAULT_BATTERY_LOW         = 3,
    AW32257_BOOST_FAULT_RESERVED_4          = 4,
    AW32257_BOOST_FAULT_THERMAL_SHUTDOWN    = 5,
    AW32257_BOOST_FAULT_RESERVED_6          = 6,
    AW32257_BOOST_FAULT_RESERVED_7          = 7
} aw32257_boost_fault_t;

/** @brief 符号化的升压驱动压摆率(slew rate)码点；手册未给出物理速率。 */
typedef enum
{
    AW32257_SLEW_RATE_DEFAULT = 0,
    AW32257_SLEW_RATE_SLOW,
    AW32257_SLEW_RATE_SLOWER,
    AW32257_SLEW_RATE_SLOWEST
} aw32257_slew_rate_t;

/** @brief 仅上电复位(POR)时可写入的安全配置。 */
typedef struct
{
    aw32257_current_code_t max_charge_current;
    uint16_t max_charge_voltage_mv;
} aw32257_safety_config_t;

/** @brief 充电终止判定算法配置。 */
typedef struct
{
    uint8_t window_periods;
    uint8_t valid_periods;
    uint8_t deglitch_ms;
    uint16_t recharge_threshold_mv;
} aw32257_termination_config_t;

/** @brief 升压输出与功率级驱动配置。 */
typedef struct
{
    uint16_t output_voltage_mv;
    uint16_t frequency_khz;
    aw32257_slew_rate_t slew_rate;
    bool fixed_dead_time;
    bool force_pwm;
} aw32257_boost_config_t;

/** @brief 解码后的 REG03 器件信息。 */
typedef struct
{
    uint8_t raw_reg03;
    uint8_t vendor_code;
    uint8_t part_code;
    uint8_t revision_code;
} aw32257_device_info_t;

/**
 * @brief 顺序读取的状态快照。
 *
 * 依次读取 REG00、REG05、REG09。这些值不是硬件锁存的原子快照。
 */
typedef struct
{
    uint8_t raw_reg00;
    uint8_t raw_reg05;
    uint8_t raw_reg09;
    bool otg_pin_high;
    bool stat_output_enabled;
    aw32257_charge_state_t charge_state;
    bool boost_active;
    aw32257_charge_fault_t charge_fault;
    bool dpm_active;
    bool cd_pin_high;
    aw32257_boost_fault_t boost_fault;
} aw32257_status_snapshot_t;

/**
 * @brief 顺序读取的配置快照。
 *
 * 全部寄存器都读取成功后才更新调用者的输出对象。快照在寄存器间
 * 不保证原子性。
 */
typedef struct
{
    uint8_t raw_reg00;
    uint8_t raw_reg01;
    uint8_t raw_reg02;
    uint8_t raw_reg04;
    uint8_t raw_reg05;
    uint8_t raw_reg06;
    uint8_t raw_reg07;
    uint8_t raw_reg0a;
    bool stat_output_enabled;
    bool termination_enabled;
    bool charge_enabled;
    bool high_impedance_requested;
    bool boost_requested;
    uint16_t charge_voltage_mv;
    bool otg_active_high;
    bool otg_pin_control_enabled;
    aw32257_current_code_t fast_charge_current;
    aw32257_term_current_code_t termination_current;
    uint16_t dpm_voltage_mv;
    aw32257_current_code_t max_charge_current;
    uint16_t max_charge_voltage_mv;
    aw32257_termination_config_t termination;
    aw32257_boost_config_t boost;
} aw32257_config_snapshot_t;

/** @brief 调用者持有的驱动实例。 */
typedef struct
{
    void * io_ctx;
    uint32_t io_timeout_ms;
    aw32257_lifecycle_t lifecycle;
    int32_t last_port_error;
} aw32257_t;

/**
 * @brief 将调用者持有的实例绑定到平台回调。
 *
 * 本函数只做本地校验并按值拷贝 @p port，绝不访问 I2C 总线。调用
 * 成功即进入 BOUND 状态。
 *
 * @warning 重新绑定任何用过的实例，即表示调用者明确确认 AW32257 自
 * 上一次 bind/init 周期之后经历过真实的硬件上电复位。对
 * POR_REQUIRED 状态尤其关键：驱动无法观测或证明电源周期。未复位
 * 就重新绑定，可能使 REG06 在本次电源周期内永久锁定。
 *
 * @param[out] device 调用者持有的实例，无需预先初始化。
 * @param[in]  port 平台回调、上下文与非零的 I/O 超时。
 * @retval    AW32257_OK 或本地参数错误；不调用任何 port 回调。
 */
aw32257_status_t aw32257_init(aw32257_t * device, void * io_ctx, uint32_t io_timeout_ms);

/**
 * @brief 执行强制要求的 POR 安全写入与器件校验。
 *
 * 本地校验通过后，第一个总线操作是直接写 REG06，随后回读 REG06
 * 并读 REG03 身份寄存器。身份校验接受任意版本码，但要求厂商/型号
 * 掩码与手册一致。
 *
 * 以 BOUND 实例进入本函数后，任何失败——包括首次 I2C 访问之前的
 * 本地安全参数失败——都会将实例锁存到 POR_REQUIRED。调用者必须
 * 真实硬件 POR 后再重新绑定并初始化。@p device_info 为可选参数，
 * 仅在完全成功时更新。
 *
 * @param[in,out] device 紧随硬件 POR 之后的 BOUND 实例。
 * @param[in]     safety 产品专属的 POR 专用电流电压限值。
 * @param[out]    device_info 可选的解码身份结果。
 */
aw32257_status_t aw32257_power_on_init(aw32257_t * device,
                                        const aw32257_safety_config_t * safety,
                                        aw32257_device_info_t * device_info);

/**
 * @brief 在充电与升压均未工作时发起软件复位。
 *
 * 驱动先采样 REG00，充电进行中或升压工作时拒绝复位。一旦尝试写入
 * 复位，即使写回调报错，函数返回前也一定调用
 * delay_ms(context, 32)：器件可能已接受 RESET 而只是 ACK 丢失。
 * 要求的静默间隔内不进行任何 I2C 访问。
 *
 * @retval AW32257_OK、AW32257_ERR_STATE，或生命周期/端口错误。
 */
aw32257_status_t aw32257_soft_reset(aw32257_t * device);

/** @brief 返回本地生命周期；空指针返回 UNBOUND。 */
aw32257_lifecycle_t aw32257_get_lifecycle(const aw32257_t * device);

/**
 * @brief 返回最近一次寄存器 I/O 回调的原始结果。
 *
 * read_reg/write_reg 回调成功时保存零。void 的 delay_ms 回调与本地
 * 校验错误不改变保存值。@p device 为 NULL 时同样返回零。这不是
 * 持久化的错误历史。
 */
int32_t aw32257_get_last_port_error(const aw32257_t * device);

/**
 * @brief 初始化成功后读取一个手册内寄存器。
 *
 * 该诊断 API 接受 REG00 到 REG0A。刻意不提供对应的任意寄存器
 * 写入 API。
 */
aw32257_status_t aw32257_read_register(aw32257_t * device,
                                        uint8_t register_address,
                                        uint8_t * value);

/** @brief 读取并校验 REG03，仅在完全成功时提交输出。 */
aw32257_status_t aw32257_read_device_info(aw32257_t * device,
                                           aw32257_device_info_t * device_info);

/** @brief 顺序读取 REG00/REG05/REG09 状态快照。 */
aw32257_status_t aw32257_read_status(aw32257_t * device,
                                      aw32257_status_snapshot_t * snapshot);

/** @brief 顺序读取全有或全无的配置快照。 */
aw32257_status_t aw32257_read_configuration(aw32257_t * device,
                                             aw32257_config_snapshot_t * snapshot);

/** @brief 通过对 REG00 读-改-写，使能或关闭开漏 STAT 输出。 */
aw32257_status_t aw32257_set_stat_output_enabled(aw32257_t * device, bool enabled);

/** @brief 使能或关闭充电，内部屏蔽 REG01.CEN 的反相语义。 */
aw32257_status_t aw32257_set_charge_enabled(aw32257_t * device, bool enabled);

/** @brief 通过对 REG01 读-改-写，使能或关闭充电终止判定。 */
aw32257_status_t aw32257_set_termination_enabled(aw32257_t * device, bool enabled);

/** @brief 通过 REG01 请求充电、高阻或升压模式。 */
aw32257_status_t aw32257_set_mode(aw32257_t * device, aw32257_mode_t mode);

/** @brief 设置精确的 3500..4500 mV、20 mV 步进 VOREG 值。 */
aw32257_status_t aw32257_set_charge_voltage_mv(aw32257_t * device,
                                                uint16_t voltage_mv);

/** @brief 设置快充电流寄存器码点，与 RSNS 无关。 */
aw32257_status_t aw32257_set_fast_charge_current(aw32257_t * device,
                                                  aw32257_current_code_t current_code);

/** @brief 设置充电终止电流寄存器码点，与 RSNS 无关。 */
aw32257_status_t aw32257_set_termination_current(aw32257_t * device,
                                                  aw32257_term_current_code_t current_code);

/** @brief 设置精确的 4250..4775 mV、75 mV 步进 DPM 值。 */
aw32257_status_t aw32257_set_dpm_voltage_mv(aw32257_t * device,
                                             uint16_t voltage_mv);

/**
 * @brief 完成本地校验后设置 CTA 与再充电字段。
 *
 * valid_periods 乘 deglitch_ms 超出手册电气特性 256 ms 上限的组合，
 * 不访问总线直接拒绝。
 */
aw32257_status_t aw32257_set_termination_config(aw32257_t * device,
                                                 const aw32257_termination_config_t * config);

/** @brief 配置外部 OTG 引脚的启用与有效极性。 */
aw32257_status_t aw32257_configure_otg_pin(aw32257_t * device,
                                            bool enabled,
                                            bool active_high);

/** @brief 设置升压输出与符号化功率级驱动字段。 */
aw32257_status_t aw32257_set_boost_config(aw32257_t * device,
                                           const aw32257_boost_config_t * config);

/** @brief 对确认使用 33 mOhm RSNS 的设计查表数据手册电流表。 */
aw32257_status_t aw32257_current_code_to_ma_33mohm(
    aw32257_current_code_t current_code,
    uint16_t * current_ma);

/** @brief 对确认使用 33 mOhm RSNS 的设计查表终止电流表。 */
aw32257_status_t aw32257_termination_current_code_to_ma_33mohm(
    aw32257_term_current_code_t current_code,
    uint16_t * current_ma);

#ifdef __cplusplus
}
#endif

#endif /* AW32257_H */
