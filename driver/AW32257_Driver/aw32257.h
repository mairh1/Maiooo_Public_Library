/**
 * @file    aw32257.h
 * @brief   AW32257 充电与升压芯片的可移植 C99 通用驱动
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * @details
 * 驱动不持有任何硬件资源。带超时保护的寄存器读写与毫秒级延时由
 * aw32257_io.h 声明的固定契约函数（aw32257_io_read_reg、
 * aw32257_io_write_reg、aw32257_io_delay_ms）在移植层实现，实例只
 * 保存调用者提供的 io_ctx 与非零 I/O 超时，以及每个实例的存储空间。
 * 核心不可重入，禁止在中断服务程序(ISR)中调用。
 *
 * @note    本公共头不包含 aw32257_io.h（单向包含：io 契约只被核心
 *          aw32257.c 与移植层包含，需要实现契约函数的文件请显式
 *          包含之）。仅依赖自身头文件与 C99 固定宽度类型头。
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
    AW32257_OK                         =  0,   /**< 成功 */
    AW32257_ERR_NULL_POINTER           = -1,   /**< 必需的指针参数为 NULL */
    AW32257_ERR_INVALID_ARGUMENT       = -2,   /**< 参数取值非法（如零超时） */
    AW32257_ERR_NOT_BOUND              = -3,   /**< 实例未执行 aw32257_init() 绑定上下文 */
    AW32257_ERR_NOT_INITIALIZED        = -4,   /**< 实例未通过 aw32257_power_on_init() 进入 READY */
    AW32257_ERR_RANGE                  = -5,   /**< 数值超出手册档位/范围，未访问总线即拒绝 */
    AW32257_ERR_IO                     = -6,   /**< 寄存器 I/O 失败；原始码可经 aw32257_get_last_port_error() 读取 */
    AW32257_ERR_ID_MISMATCH            = -7,   /**< REG03 厂商/型号掩码与手册不符 */
    AW32257_ERR_SAFETY_MISMATCH        = -8,   /**< REG06 安全限值回读与写入值不一致 */
    AW32257_ERR_STATE                  = -9,   /**< 当前生命周期/器件状态不允许该操作 */
    AW32257_ERR_POR_REQUIRED           = -10   /**< 实例已锁存为必须真实硬件 POR 后重新初始化 */
} aw32257_status_t;

/** @brief 驱动实例的本地生命周期状态。 */
typedef enum
{
    AW32257_LIFECYCLE_UNBOUND = 0,      /**< 未绑定：尚未通过 aw32257_init() 提供上下文 */
    AW32257_LIFECYCLE_BOUND,            /**< 已绑定：等待 aw32257_power_on_init() */
    AW32257_LIFECYCLE_READY,            /**< 就绪：POR 校验通过，可访问寄存器 */
    AW32257_LIFECYCLE_POR_REQUIRED      /**< 需 POR：失败锁存，须真实硬件复位后重新 init */
} aw32257_lifecycle_t;

/**
 * @brief 快充电流与安全限流寄存器码点。
 *
 * 实际物理电流取决于外部检流电阻。仅当设计确认使用 33 mOhm
 * 检流电阻时才可使用 aw32257_current_code_to_ma_33mohm() 换算。
 */
typedef enum
{
    AW32257_CURRENT_CODE_00 = 0x00,     /**< 快充电流码点 0x00 */
    AW32257_CURRENT_CODE_01 = 0x01,     /**< 快充电流码点 0x01 */
    AW32257_CURRENT_CODE_02 = 0x02,     /**< 快充电流码点 0x02 */
    AW32257_CURRENT_CODE_03 = 0x03,     /**< 快充电流码点 0x03 */
    AW32257_CURRENT_CODE_04 = 0x04,     /**< 快充电流码点 0x04 */
    AW32257_CURRENT_CODE_05 = 0x05,     /**< 快充电流码点 0x05 */
    AW32257_CURRENT_CODE_06 = 0x06,     /**< 快充电流码点 0x06 */
    AW32257_CURRENT_CODE_07 = 0x07,     /**< 快充电流码点 0x07 */
    AW32257_CURRENT_CODE_08 = 0x08,     /**< 快充电流码点 0x08 */
    AW32257_CURRENT_CODE_09 = 0x09,     /**< 快充电流码点 0x09 */
    AW32257_CURRENT_CODE_0A = 0x0A,     /**< 快充电流码点 0x0A */
    AW32257_CURRENT_CODE_0B = 0x0B,     /**< 快充电流码点 0x0B */
    AW32257_CURRENT_CODE_0C = 0x0C,     /**< 快充电流码点 0x0C */
    AW32257_CURRENT_CODE_0D = 0x0D,     /**< 快充电流码点 0x0D */
    AW32257_CURRENT_CODE_0E = 0x0E,     /**< 快充电流码点 0x0E */
    AW32257_CURRENT_CODE_0F = 0x0F      /**< 快充电流码点 0x0F */
} aw32257_current_code_t;

/** @brief 充电终止电流寄存器码点。 */
typedef enum
{
    AW32257_TERM_CURRENT_CODE_0 = 0,    /**< 终止电流码点 0 */
    AW32257_TERM_CURRENT_CODE_1 = 1,    /**< 终止电流码点 1 */
    AW32257_TERM_CURRENT_CODE_2 = 2,    /**< 终止电流码点 2 */
    AW32257_TERM_CURRENT_CODE_3 = 3,    /**< 终止电流码点 3 */
    AW32257_TERM_CURRENT_CODE_4 = 4,    /**< 终止电流码点 4 */
    AW32257_TERM_CURRENT_CODE_5 = 5,    /**< 终止电流码点 5 */
    AW32257_TERM_CURRENT_CODE_6 = 6,    /**< 终止电流码点 6 */
    AW32257_TERM_CURRENT_CODE_7 = 7     /**< 终止电流码点 7 */
} aw32257_term_current_code_t;

/** @brief 通过 REG01 请求的工作模式。外部引脚可能覆盖该请求。 */
typedef enum
{
    AW32257_MODE_CHARGE = 0,            /**< 充电模式（REG01 模式位 00） */
    AW32257_MODE_HIGH_IMPEDANCE,        /**< 高阻模式（REG01 高阻请求位置位） */
    AW32257_MODE_BOOST                  /**< 升压模式（REG01 升压请求位置位） */
} aw32257_mode_t;

/** @brief REG00 上报的充电状态。 */
typedef enum
{
    AW32257_CHARGE_STATE_READY       = 0,   /**< 就绪（不在充电循环中） */
    AW32257_CHARGE_STATE_IN_PROGRESS = 1,   /**< 充电进行中 */
    AW32257_CHARGE_STATE_DONE        = 2,   /**< 充电完成 */
    AW32257_CHARGE_STATE_FAULT       = 3    /**< 充电故障（见充电故障码） */
} aw32257_charge_state_t;

/** @brief REG00 上报的充电故障。 */
typedef enum
{
    AW32257_CHARGE_FAULT_NORMAL                  = 0,  /**< 正常充电 */
    AW32257_CHARGE_FAULT_VBUS_OVP                = 1,  /**< VBUS 过压故障 */
    AW32257_CHARGE_FAULT_SLEEP                   = 2,  /**< 睡眠故障 */
    AW32257_CHARGE_FAULT_BAD_ADAPTER_OR_VBUS_UVLO = 3, /**< 坏适配器或 VBUS 低于 UVLO（手册不作区分） */
    AW32257_CHARGE_FAULT_BATTERY_OVP             = 4,  /**< 电池过压故障 */
    AW32257_CHARGE_FAULT_THERMAL_SHUTDOWN        = 5,  /**< 热关断故障 */
    AW32257_CHARGE_FAULT_RESERVED_6              = 6,  /**< 保留码点（原样解码） */
    AW32257_CHARGE_FAULT_NO_BATTERY              = 7   /**< 无电池故障 */
} aw32257_charge_fault_t;

/** @brief REG09 上报的升压(boost)故障。 */
typedef enum
{
    AW32257_BOOST_FAULT_NORMAL              = 0,  /**< 升压正常 */
    AW32257_BOOST_FAULT_VBUS_OVP            = 1,  /**< VBUS 过压故障 */
    AW32257_BOOST_FAULT_OVERLOAD            = 2,  /**< 升压过载故障 */
    AW32257_BOOST_FAULT_BATTERY_LOW         = 3,  /**< 电池电压过低故障 */
    AW32257_BOOST_FAULT_RESERVED_4          = 4,  /**< 保留码点（原样解码） */
    AW32257_BOOST_FAULT_THERMAL_SHUTDOWN    = 5,  /**< 热关断故障 */
    AW32257_BOOST_FAULT_RESERVED_6          = 6,  /**< 保留码点（原样解码） */
    AW32257_BOOST_FAULT_RESERVED_7          = 7   /**< 保留码点（原样解码） */
} aw32257_boost_fault_t;

/** @brief 符号化的升压驱动压摆率(slew rate)码点；手册未给出物理速率。 */
typedef enum
{
    AW32257_SLEW_RATE_DEFAULT = 0,      /**< 压摆率码点 0（手册未给出物理速率） */
    AW32257_SLEW_RATE_SLOW,             /**< 压摆率码点 1（较慢） */
    AW32257_SLEW_RATE_SLOWER,           /**< 压摆率码点 2（更慢） */
    AW32257_SLEW_RATE_SLOWEST           /**< 压摆率码点 3（最慢） */
} aw32257_slew_rate_t;

/** @brief 仅上电复位(POR)时可写入的安全配置。 */
typedef struct
{
    aw32257_current_code_t max_charge_current;    /**< 最大快充电流码点（REG06 高 4 位） */
    uint16_t max_charge_voltage_mv;               /**< 最大安全电压，单位 mV（REG06 低 4 位） */
} aw32257_safety_config_t;

/** @brief 充电终止判定算法配置。 */
typedef struct
{
    uint8_t window_periods;           /**< CTA 窗口周期数：8 或 16 */
    uint8_t valid_periods;            /**< 终止有效周期数：1、2、4 或 8 */
    uint8_t deglitch_ms;              /**< 单周期去抖时间：8、16、32 或 64 ms */
    uint16_t recharge_threshold_mv;   /**< 再充电阈值：50..200 mV、50 mV 步进 */
} aw32257_termination_config_t;

/** @brief 升压输出与功率级驱动配置。 */
typedef struct
{
    uint16_t output_voltage_mv;     /**< 升压输出电压：5050..5350 mV、100 mV 步进 */
    uint16_t frequency_khz;         /**< 开关频率：1500 或 1700 kHz */
    aw32257_slew_rate_t slew_rate;  /**< 功率级驱动压摆率码点 */
    bool fixed_dead_time;           /**< true = 固定死时间使能 */
    bool force_pwm;                 /**< true = 强制 PWM 模式 */
} aw32257_boost_config_t;

/** @brief 解码后的 REG03 器件信息。 */
typedef struct
{
    uint8_t raw_reg03;        /**< REG03 原始值 */
    uint8_t vendor_code;      /**< 厂商标识码（REG03[7:5]） */
    uint8_t part_code;        /**< 型号码（REG03[4:3]） */
    uint8_t revision_code;    /**< 修订码（REG03[2:0]） */
} aw32257_device_info_t;

/**
 * @brief 顺序读取的状态快照。
 *
 * 依次读取 REG00、REG05、REG09。这些值不是硬件锁存的原子快照。
 */
typedef struct
{
    uint8_t raw_reg00;                    /**< REG00 原始值 */
    uint8_t raw_reg05;                    /**< REG05 原始值 */
    uint8_t raw_reg09;                    /**< REG09 原始值 */
    bool otg_pin_high;                    /**< OTG 引脚当前电平为高 */
    bool stat_output_enabled;             /**< STAT 开漏输出已使能 */
    aw32257_charge_state_t charge_state;  /**< 充电状态 */
    bool boost_active;                    /**< 升压正在工作 */
    aw32257_charge_fault_t charge_fault;  /**< 充电故障码 */
    bool dpm_active;                      /**< DPM（输入电压动态调节）已激活 */
    bool cd_pin_high;                     /**< CD 引脚当前电平为高 */
    aw32257_boost_fault_t boost_fault;    /**< 升压故障码 */
} aw32257_status_snapshot_t;

/**
 * @brief 顺序读取的配置快照。
 *
 * 全部寄存器都读取成功后才更新调用者的输出对象。快照在寄存器间
 * 不保证原子性。
 */
typedef struct
{
    uint8_t raw_reg00;                               /**< REG00 原始值 */
    uint8_t raw_reg01;                               /**< REG01 原始值 */
    uint8_t raw_reg02;                               /**< REG02 原始值 */
    uint8_t raw_reg04;                               /**< REG04 原始值 */
    uint8_t raw_reg05;                               /**< REG05 原始值 */
    uint8_t raw_reg06;                               /**< REG06 原始值 */
    uint8_t raw_reg07;                               /**< REG07 原始值 */
    uint8_t raw_reg0a;                               /**< REG0A 原始值 */
    bool stat_output_enabled;                        /**< STAT 开漏输出已使能 */
    bool termination_enabled;                        /**< 充电终止判定已使能 */
    bool charge_enabled;                             /**< 充电已使能（REG01.CEN 反相语义已屏蔽） */
    bool high_impedance_requested;                   /**< 已请求高阻模式 */
    bool boost_requested;                            /**< 已请求升压模式 */
    uint16_t charge_voltage_mv;                      /**< 充电调节电压 VOREG，单位 mV */
    bool otg_active_high;                            /**< OTG 引脚高电平有效 */
    bool otg_pin_control_enabled;                    /**< OTG 引脚控制已使能 */
    aw32257_current_code_t fast_charge_current;      /**< 快充电流码点 */
    aw32257_term_current_code_t termination_current; /**< 终止电流码点 */
    uint16_t dpm_voltage_mv;                         /**< DPM 电压，单位 mV */
    aw32257_current_code_t max_charge_current;       /**< 最大快充电流码点（REG06） */
    uint16_t max_charge_voltage_mv;                  /**< 最大安全电压，单位 mV（REG06） */
    aw32257_termination_config_t termination;        /**< 终止判定算法解码结果 */
    aw32257_boost_config_t boost;                    /**< 升压配置解码结果 */
} aw32257_config_snapshot_t;

/** @brief 调用者持有的驱动实例。 */
typedef struct
{
    void * io_ctx;                  /**< 调用者持有的总线/平台上下文，核心按不透明指针透传 */
    uint32_t io_timeout_ms;         /**< 单笔 I/O 事务的超时上限，单位毫秒，必须非零 */
    aw32257_lifecycle_t lifecycle;  /**< 本地生命周期状态 */
    int32_t last_port_error;        /**< 最近一次 io 读/写契约函数的原始返回值 */
} aw32257_t;

/**
 * @brief   将调用者持有的实例与平台上下文绑定。
 *
 * 本函数只做本地校验并按值保存 @p io_ctx 与 @p io_timeout_ms，绝不
 * 访问 I2C 总线，也不调用任何 io 契约函数。调用成功即进入 BOUND
 * 状态。
 *
 * @warning 重新绑定任何用过的实例，即表示调用者明确确认 AW32257 自
 * 上一次 init/power_on_init 周期之后经历过真实的硬件上电复位。对
 * POR_REQUIRED 状态尤其关键：驱动无法观测或证明电源周期。未复位
 * 就重新绑定，可能使 REG06 在本次电源周期内永久锁定。
 *
 * @param[out] device        调用者持有的实例，无需预先初始化。
 * @param[in]  io_ctx        调用者持有的平台上下文，核心按不透明指针
 *                           透传给 io 契约函数。
 * @param[in]  io_timeout_ms 传给每笔 I/O 事务的超时上限，单位毫秒，
 *                           必须非零。
 *
 * @retval  AW32257_OK 绑定成功。
 * @retval  AW32257_ERR_NULL_POINTER @p device 为 NULL。
 * @retval  AW32257_ERR_INVALID_ARGUMENT @p io_timeout_ms 为 0。
 */
aw32257_status_t
aw32257_init(aw32257_t * device, void * io_ctx, uint32_t io_timeout_ms);

/**
 * @brief   执行强制要求的 POR 安全写入与器件校验。
 *
 * 本地校验通过后，第一个总线操作是直接写 REG06，随后回读 REG06
 * 并读 REG03 身份寄存器。身份校验接受任意版本码，但要求厂商/型号
 * 掩码与手册一致。
 *
 * 以 BOUND 实例进入本函数后，任何失败——包括首次 I2C 访问之前的
 * 本地安全参数失败——都会将实例锁存到 POR_REQUIRED。调用者必须
 * 真实硬件 POR 后再重新调用 aw32257_init() 与本函数。
 * @p device_info 为可选参数，仅在完全成功时更新。
 *
 * @param[in,out] device      紧随硬件 POR 之后的 BOUND 实例。
 * @param[in]     safety      产品专属的 POR 专用电流电压限值。
 * @param[out]    device_info 可选的解码身份结果，允许为 NULL。
 *
 * @retval  AW32257_OK 校验与安全写入全部成功，实例进入 READY。
 * @retval  AW32257_ERR_NULL_POINTER @p device 或 @p safety 为 NULL。
 * @retval  AW32257_ERR_RANGE 安全电流码点或电压超出手册范围。
 * @retval  AW32257_ERR_IO 寄存器 I/O 失败。
 * @retval  AW32257_ERR_SAFETY_MISMATCH REG06 回读与写入值不一致。
 * @retval  AW32257_ERR_ID_MISMATCH REG03 身份掩码与手册不符。
 * @retval  AW32257_ERR_STATE 实例不处于 BOUND 状态。
 * @retval  AW32257_ERR_POR_REQUIRED 实例已锁存为必须真实 POR。
 */
aw32257_status_t
aw32257_power_on_init(aw32257_t * device,
                      const aw32257_safety_config_t * safety,
                      aw32257_device_info_t * device_info);

/**
 * @brief   在充电与升压均未工作时发起软件复位。
 *
 * 驱动先采样 REG00，充电进行中或升压工作时拒绝复位。一旦尝试写入
 * 复位，即使写事务报错，函数返回前也一定调用
 * aw32257_io_delay_ms(io_ctx, AW32257_SOFT_RESET_DELAY_MS)（32 ms）：
 * 器件可能已接受 RESET 而只是 ACK 丢失。要求的静默间隔内不进行
 * 任何 I2C 访问。
 *
 * @param[in,out] device READY 状态的实例。
 *
 * @retval  AW32257_OK 复位写入与静默等待完成。
 * @retval  AW32257_ERR_STATE 正在充电或升压工作，未写入复位。
 * @retval  AW32257_ERR_IO 寄存器 I/O 失败。
 * @retval  AW32257_ERR_NULL_POINTER、AW32257_ERR_NOT_BOUND、
 *          AW32257_ERR_NOT_INITIALIZED 或 AW32257_ERR_POR_REQUIRED：
 *          生命周期校验失败。
 */
aw32257_status_t
aw32257_soft_reset(aw32257_t * device);

/**
 * @brief   返回本地生命周期。
 *
 * 纯本地查询，不访问总线。空指针返回 UNBOUND。
 *
 * @param[in] device 驱动实例，允许为 NULL。
 *
 * @retval  aw32257_lifecycle_t 实例当前的生命周期；@p device 为 NULL
 *          时返回 AW32257_LIFECYCLE_UNBOUND。
 */
aw32257_lifecycle_t
aw32257_get_lifecycle(const aw32257_t * device);

/**
 * @brief   返回最近一次寄存器 I/O 的原始结果。
 *
 * io 读写契约函数成功时保存零；失败时原样保存平台返回的错误值。
 * 本驱动刻意透传原始平台错误码（返回类型因此为 int32_t 而非结果码
 * 枚举），供应用诊断具体通信故障。void 的 aw32257_io_delay_ms() 与
 * 本地校验错误不改变保存值。@p device 为 NULL 时同样返回零。这不是
 * 持久化的错误历史。
 *
 * @param[in] device 驱动实例，允许为 NULL。
 *
 * @retval  int32_t 最近一次读/写契约函数的原始返回值；无历史时为 0。
 */
int32_t
aw32257_get_last_port_error(const aw32257_t * device);

/**
 * @brief   初始化成功后读取一个手册内寄存器。
 *
 * 该诊断 API 接受 REG00 到 REG0A。刻意不提供对应的任意寄存器
 * 写入 API。
 *
 * @param[in,out] device           READY 状态的实例。
 * @param[in]     register_address 寄存器地址（REG00 到 REG0A）。
 * @param[out]    value            输出：读到的寄存器字节。
 *
 * @retval  AW32257_OK 读取成功。
 * @retval  AW32257_ERR_NULL_POINTER @p device 或 @p value 为 NULL。
 * @retval  AW32257_ERR_RANGE @p register_address 超出 REG0A。
 * @retval  AW32257_ERR_IO 寄存器 I/O 失败。
 * @retval  AW32257_ERR_NOT_BOUND、AW32257_ERR_NOT_INITIALIZED 或
 *          AW32257_ERR_POR_REQUIRED：生命周期校验失败。
 */
aw32257_status_t
aw32257_read_register(aw32257_t * device,
                      uint8_t register_address,
                      uint8_t * value);

/**
 * @brief   读取并校验 REG03，仅在完全成功时提交输出。
 *
 * @param[in,out] device      READY 状态的实例。
 * @param[out]    device_info 输出：解码后的身份信息。
 *
 * @retval  AW32257_OK 读取与身份校验成功。
 * @retval  AW32257_ERR_NULL_POINTER @p device 或 @p device_info 为 NULL。
 * @retval  AW32257_ERR_ID_MISMATCH REG03 身份掩码与手册不符。
 * @retval  AW32257_ERR_IO 寄存器 I/O 失败。
 * @retval  AW32257_ERR_NOT_BOUND、AW32257_ERR_NOT_INITIALIZED 或
 *          AW32257_ERR_POR_REQUIRED：生命周期校验失败。
 */
aw32257_status_t
aw32257_read_device_info(aw32257_t * device,
                         aw32257_device_info_t * device_info);

/**
 * @brief   顺序读取 REG00/REG05/REG09 状态快照。
 *
 * 先写局部对象，全部寄存器读取成功后才更新调用者的输出对象。
 *
 * @param[in,out] device   READY 状态的实例。
 * @param[out]    snapshot 输出：状态快照（顺序采样，非原子快照）。
 *
 * @retval  AW32257_OK 快照完整提交。
 * @retval  AW32257_ERR_NULL_POINTER @p device 或 @p snapshot 为 NULL。
 * @retval  AW32257_ERR_IO 任一寄存器读取失败（输出对象不变）。
 * @retval  AW32257_ERR_NOT_BOUND、AW32257_ERR_NOT_INITIALIZED 或
 *          AW32257_ERR_POR_REQUIRED：生命周期校验失败。
 */
aw32257_status_t
aw32257_read_status(aw32257_t * device,
                    aw32257_status_snapshot_t * snapshot);

/**
 * @brief   顺序读取全有或全无的配置快照。
 *
 * 全部寄存器都读取成功后才更新调用者的输出对象。快照在寄存器间
 * 不保证原子性。
 *
 * @param[in,out] device   READY 状态的实例。
 * @param[out]    snapshot 输出：配置快照。
 *
 * @retval  AW32257_OK 快照完整提交。
 * @retval  AW32257_ERR_NULL_POINTER @p device 或 @p snapshot 为 NULL。
 * @retval  AW32257_ERR_IO 任一寄存器读取失败（输出对象不变）。
 * @retval  AW32257_ERR_NOT_BOUND、AW32257_ERR_NOT_INITIALIZED 或
 *          AW32257_ERR_POR_REQUIRED：生命周期校验失败。
 */
aw32257_status_t
aw32257_read_configuration(aw32257_t * device,
                           aw32257_config_snapshot_t * snapshot);

/**
 * @brief   使能或关闭开漏 STAT 输出。
 *
 * 通过对 REG00 读-改-写实现，只改 EN_STAT 位。
 *
 * @param[in,out] device  READY 状态的实例。
 * @param[in]     enabled true = 使能 STAT 输出，false = 关闭。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_stat_output_enabled(aw32257_t * device, bool enabled);

/**
 * @brief   使能或关闭充电。
 *
 * 内部屏蔽 REG01.CEN 的反相语义：enabled = true 时把 CEN 位清零。
 *
 * @param[in,out] device  READY 状态的实例。
 * @param[in]     enabled true = 使能充电，false = 关闭充电。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_charge_enabled(aw32257_t * device, bool enabled);

/**
 * @brief   使能或关闭充电终止判定。
 *
 * 通过对 REG01 读-改-写实现，只改终止使能位。
 *
 * @param[in,out] device  READY 状态的实例。
 * @param[in]     enabled true = 使能终止判定，false = 关闭。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_termination_enabled(aw32257_t * device, bool enabled);

/**
 * @brief   通过 REG01 请求充电、高阻或升压模式。
 *
 * 只请求软件模式；OTG、CD 等外部引脚仍可能覆盖实际硬件状态。
 *
 * @param[in,out] device READY 状态的实例。
 * @param[in]     mode  目标工作模式。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_RANGE @p mode 不是合法的模式值。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_mode(aw32257_t * device, aw32257_mode_t mode);

/**
 * @brief   设置精确的 3500..4500 mV、20 mV 步进 VOREG 值。
 *
 * 只接受能被 20 mV 档位精确表示的值，不取整、不钳位；非法值不
 * 访问总线直接拒绝。
 *
 * @param[in,out] device     READY 状态的实例。
 * @param[in]     voltage_mv 充电调节电压，单位 mV。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_RANGE 电压超界或不是 20 mV 的整数倍。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_charge_voltage_mv(aw32257_t * device,
                              uint16_t voltage_mv);

/**
 * @brief   设置快充电流寄存器码点，与 RSNS 无关。
 *
 * 写入时显式清零 REG04 的 RESET 触发位，避免误触发软件复位。
 *
 * @param[in,out] device       READY 状态的实例。
 * @param[in]     current_code 快充电流码点。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_RANGE @p current_code 超出 16 档范围。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_fast_charge_current(aw32257_t * device,
                                aw32257_current_code_t current_code);

/**
 * @brief   设置充电终止电流寄存器码点，与 RSNS 无关。
 *
 * 写入时显式清零 REG04 的 RESET 触发位，避免误触发软件复位。
 *
 * @param[in,out] device       READY 状态的实例。
 * @param[in]     current_code 终止电流码点。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_RANGE @p current_code 超出 8 档范围。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_termination_current(aw32257_t * device,
                                aw32257_term_current_code_t current_code);

/**
 * @brief   设置精确的 4250..4775 mV、75 mV 步进 DPM 值。
 *
 * 只接受能被 75 mV 档位精确表示的值，不取整、不钳位。
 *
 * @param[in,out] device     READY 状态的实例。
 * @param[in]     voltage_mv DPM 电压阈值，单位 mV。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_RANGE 电压超界或不是 75 mV 的整数倍。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_dpm_voltage_mv(aw32257_t * device,
                           uint16_t voltage_mv);

/**
 * @brief   完成本地校验后设置 CTA 与再充电字段。
 *
 * valid_periods 乘 deglitch_ms 超出手册电气特性 256 ms 上限的组合，
 * 不访问总线直接拒绝。
 *
 * @param[in,out] device READY 状态的实例。
 * @param[in]     config 终止判定算法配置。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_NULL_POINTER @p config 为 NULL。
 * @retval  AW32257_ERR_RANGE 某字段超出手册档位或组合超出上限。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_termination_config(aw32257_t * device,
                               const aw32257_termination_config_t * config);

/**
 * @brief   配置外部 OTG 引脚的启用与有效极性。
 *
 * 通过对 REG02 读-改-写实现，只改 OTG 控制位。
 *
 * @param[in,out] device      READY 状态的实例。
 * @param[in]     enabled     true = 使能 OTG 引脚控制。
 * @param[in]     active_high true = 高电平有效，false = 低电平有效。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_configure_otg_pin(aw32257_t * device,
                          bool enabled,
                          bool active_high);

/**
 * @brief   设置升压输出与符号化功率级驱动字段。
 *
 * 校验频率、压摆率与输出电压档位后，经读-改-写写入 REG0A。
 *
 * @param[in,out] device READY 状态的实例。
 * @param[in]     config 升压输出与功率级驱动配置。
 *
 * @retval  AW32257_OK 写入成功或值未变化。
 * @retval  AW32257_ERR_NULL_POINTER @p config 为 NULL。
 * @retval  AW32257_ERR_RANGE 某字段超出手册档位。
 * @retval  AW32257_ERR_* 生命周期校验失败或 I/O 失败时原样上抛。
 */
aw32257_status_t
aw32257_set_boost_config(aw32257_t * device,
                         const aw32257_boost_config_t * config);

/**
 * @brief   对确认使用 33 mOhm RSNS 的设计查表数据手册电流表。
 *
 * 仅当硬件确认 RSNS = 33 mOhm 时结果才有意义；其余设计禁止使用。
 *
 * @param[in]  current_code 快充电流码点。
 * @param[out] current_ma   输出：对应的快充电流，单位 mA。
 *
 * @retval  AW32257_OK 换算成功。
 * @retval  AW32257_ERR_NULL_POINTER @p current_ma 为 NULL。
 * @retval  AW32257_ERR_RANGE @p current_code 超出 16 档范围。
 */
aw32257_status_t
aw32257_current_code_to_ma_33mohm(
    aw32257_current_code_t current_code,
    uint16_t * current_ma);

/**
 * @brief   对确认使用 33 mOhm RSNS 的设计查表终止电流表。
 *
 * 仅当硬件确认 RSNS = 33 mOhm 时结果才有意义；其余设计禁止使用。
 *
 * @param[in]  current_code 终止电流码点。
 * @param[out] current_ma   输出：对应的终止电流，单位 mA。
 *
 * @retval  AW32257_OK 换算成功。
 * @retval  AW32257_ERR_NULL_POINTER @p current_ma 为 NULL。
 * @retval  AW32257_ERR_RANGE @p current_code 超出 8 档范围。
 */
aw32257_status_t
aw32257_termination_current_code_to_ma_33mohm(
    aw32257_term_current_code_t current_code,
    uint16_t * current_ma);

#ifdef __cplusplus
}
#endif

#endif /* AW32257_H */
