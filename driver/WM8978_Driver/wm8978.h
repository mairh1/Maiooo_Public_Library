/**
 * @file    wm8978.h
 * @brief   WM8978 立体声音频编解码器(codec)的可移植 C99 驱动
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * @details
 * 核心不持有任何外设、引脚、时钟、DMA、中断或内存资源。调用者提供
 * 一个带超时保护的控制帧写入回调，以及（使用上电时序辅助函数时）
 * 一个毫秒延时回调。同一核心可配合 WM8978 的 2 线 / 3 线控制模式
 * 工作，因为物理传输由平台适配层负责。
 *
 * WM8978 的寄存器经手册定义的控制接口只写。因此驱动维护软件影子
 * 缓存(shadow)。进行任何常规寄存器操作前，先调用 wm8978_soft_reset()
 * 或 wm8978_assume_power_on_reset()。驱动不可重入；请在中断上下文
 * 之外串行化访问。
 *
 * @note    仅依赖 <stdbool.h>/<stdint.h> 与自身头文件；硬件动作经
 *          wm8978_io.h 的固定契约由移植层实现，核心不再接收运行期
 *          回调结构体。
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef WM8978_H
#define WM8978_H

#include <stdbool.h>
#include <stdint.h>

#include "wm8978_io.h"
#include "wm8978_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WM8978_DRIVER_VERSION_MAJOR             1
#define WM8978_DRIVER_VERSION_MINOR             0
#define WM8978_DRIVER_VERSION_PATCH             0

/** @brief 驱动结果码。 */
typedef enum
{
    WM8978_OK                         =  0,   /**< 成功 */
    WM8978_ERR_NULL_POINTER           = -1,   /**< 必需的指针参数为 NULL */
    WM8978_ERR_INVALID_ARGUMENT       = -2,   /**< 参数取值非法（如超时为 0、掩码与值不匹配、DSP 模式位与 LRC 极性混用） */
    WM8978_ERR_NOT_BOUND              = -3,   /**< 实例尚未调用 wm8978_bind() */
    WM8978_ERR_NOT_READY              = -4,   /**< 已绑定但影子未同步，须先软复位或确认 POR */
    WM8978_ERR_RANGE                  = -5,   /**< 数值超出编码范围（音量码点、分频档位、PLL N/K 等） */
    WM8978_ERR_INVALID_REGISTER       = -6,   /**< 地址不在 52 个有效寄存器内，或该操作不接受此地址 */
    WM8978_ERR_RESERVED_BITS          = -7,   /**< 试图让保留位偏离数据手册复位值 */
    WM8978_ERR_IO                     = -8,   /**< 控制帧传输失败（io 层返回非零，原始值经 wm8978_get_last_port_error() 获取） */
    WM8978_ERR_STATE                  = -9,   /**< 器件状态不允许该操作（如 EQ3DMODE 运行期切换、PLL 顺序错误、上电前置条件不满足） */
    WM8978_ERR_NO_SHADOW              = -10,  /**< 该地址没有影子值（R0 为非锁存命令） */
    WM8978_ERR_DELAY_REQUIRED         = -11,  /**< 需要板级延时支持的场合（当前核心未使用，保留语义） */
    WM8978_ERR_UNSUPPORTED            = -12,  /**< 不支持的组合（如右对齐 + 32 位字长） */
    WM8978_ERR_DESYNCHRONIZED         = -13   /**< 实例因端口错误失步，须经确认的复位恢复，不得直接重试 */
} wm8978_status_t;

/** @brief 驱动实例的本地生命周期。 */
typedef enum
{
    WM8978_LIFECYCLE_UNBOUND = 0,             /**< 未绑定 io 上下文 */
    WM8978_LIFECYCLE_BOUND,                   /**< 已绑定但影子未同步，普通写入被拒绝 */
    WM8978_LIFECYCLE_READY,                   /**< 影子与芯片同步，可执行常规操作 */
    WM8978_LIFECYCLE_DESYNCHRONIZED           /**< 端口错误后失步，须经确认的复位恢复 */
} wm8978_lifecycle_t;

/** @brief 数字音频串行格式，对应 R4 FMT 编码。 */
typedef enum
{
    WM8978_AUDIO_FORMAT_RIGHT_JUSTIFIED = 0,  /**< 右对齐格式（FMT=00，字长最大 24 位） */
    WM8978_AUDIO_FORMAT_LEFT_JUSTIFIED  = 1,  /**< 左对齐格式（FMT=01） */
    WM8978_AUDIO_FORMAT_I2S             = 2,  /**< I2S 标准格式（FMT=10） */
    WM8978_AUDIO_FORMAT_DSP_PCM         = 3   /**< DSP/PCM 模式（FMT=11，配合模式 A/B 选择） */
} wm8978_audio_format_t;

/** @brief 音频字长，对应 R4 WL 编码。 */
typedef enum
{
    WM8978_WORD_LENGTH_16_BITS = 0,           /**< 16 位（WL=00） */
    WM8978_WORD_LENGTH_20_BITS = 1,           /**< 20 位（WL=01） */
    WM8978_WORD_LENGTH_24_BITS = 2,           /**< 24 位（WL=10） */
    WM8978_WORD_LENGTH_32_BITS = 3            /**< 32 位（WL=11） */
} wm8978_word_length_t;

/** @brief R4 数字音频接口配置。 */
typedef struct
{
    wm8978_audio_format_t format;             /**< 数字音频串行格式 */
    wm8978_word_length_t word_length;         /**< 音频字长 */
    bool invert_bclk;                         /**< true 反转 BCLK 极性（BCP） */
    bool invert_lrc;                          /**< 右对齐/左对齐与 I2S 格式的 LRC 极性；DSP 格式必须为 false。 */
    bool dsp_mode_b;                          /**< true 为 DSP/PCM 模式 B，false 为模式 A；其它格式必须为 false。 */
    bool swap_dac_channels;                   /**< true 交换左右 DAC 输出通道（DACLRSWAP） */
    bool swap_adc_channels;                   /**< true 交换左右 ADC 输入通道（ADCLRSWAP） */
    bool mono;                                /**< true 把左声道数据复制到右声道（MONO） */
} wm8978_audio_interface_config_t;

/** @brief MCLK 或 PLL 输出分频，对应 R6 MCLKDIV 编码。 */
typedef enum
{
    WM8978_MCLK_DIV_1 = 0,                    /**< 不分频（MCLKDIV=000） */
    WM8978_MCLK_DIV_1_5,                      /**< 1.5 分频（MCLKDIV=001） */
    WM8978_MCLK_DIV_2,                        /**< 2 分频（MCLKDIV=010） */
    WM8978_MCLK_DIV_3,                        /**< 3 分频（MCLKDIV=011） */
    WM8978_MCLK_DIV_4,                        /**< 4 分频（MCLKDIV=100） */
    WM8978_MCLK_DIV_6,                        /**< 6 分频（MCLKDIV=101） */
    WM8978_MCLK_DIV_8,                        /**< 8 分频（MCLKDIV=110） */
    WM8978_MCLK_DIV_12                        /**< 12 分频（MCLKDIV=111） */
} wm8978_mclk_div_t;

/** @brief SYSCLK 到 BCLK 的分频，对应 R6 BCLKDIV 的合法编码。 */
typedef enum
{
    WM8978_BCLK_DIV_1 = 0,                    /**< 不分频（BCLKDIV=00） */
    WM8978_BCLK_DIV_2,                        /**< 2 分频（BCLKDIV=01） */
    WM8978_BCLK_DIV_4,                        /**< 4 分频（BCLKDIV=10） */
    WM8978_BCLK_DIV_8,                        /**< 8 分频（BCLKDIV=11） */
    WM8978_BCLK_DIV_16,                       /**< 16 分频 */
    WM8978_BCLK_DIV_32                        /**< 32 分频 */
} wm8978_bclk_div_t;

/** @brief R6 时钟源 / 主从模式配置。 */
typedef struct
{
    bool codec_is_master;                     /**< true 为主机模式：codec 输出 BCLK/LRC */
    bool use_pll;                             /**< true 选择 PLL 输出为 SYSCLK（须先使能 PLL 且 VMID 非 0） */
    wm8978_mclk_div_t mclk_divider;           /**< 时钟源到 SYSCLK 的分频 */
    wm8978_bclk_div_t bclk_divider;           /**< SYSCLK 到 BCLK 的分频 */
} wm8978_clock_config_t;

/**
 * @brief 内部数字滤波器系数组，对应 R7 SR。
 *
 * 该字段不产生采样时钟。真实采样率由 MCLK/PLL、分频器与音频总线
 * 时钟决定。对 44.1/22.05/11.025 kHz，数据手册要求选择最接近的
 * 48/24/12 kHz 系数组。
 */
typedef enum
{
    WM8978_FILTER_SR_48_KHZ = 0,              /**< 48 kHz 系数组（兼作 44.1 kHz 最近值） */
    WM8978_FILTER_SR_32_KHZ,                  /**< 32 kHz 系数组 */
    WM8978_FILTER_SR_24_KHZ,                  /**< 24 kHz 系数组（兼作 22.05 kHz 最近值） */
    WM8978_FILTER_SR_16_KHZ,                  /**< 16 kHz 系数组 */
    WM8978_FILTER_SR_12_KHZ,                  /**< 12 kHz 系数组（兼作 11.025 kHz 最近值） */
    WM8978_FILTER_SR_8_KHZ                    /**< 8 kHz 系数组 */
} wm8978_filter_sample_rate_t;

/** @brief R36-R39 的原始 PLL 比率配置。 */
typedef struct
{
    bool divide_mclk_by_2;                    /**< true 使能 MCLK 预分频：f1 = MCLK/2（PLLPRESCALE） */
    uint8_t n;                                /**< PLL 整数分频 N，驱动接受 6..12 */
    uint32_t k;                               /**< PLL 小数 K，24 位（0..0xFFFFFF），由调用者按目标频率计算 */
} wm8978_pll_config_t;

/** @brief 音量编程选择的模拟输出对。 */
typedef enum
{
    WM8978_OUTPUT_HEADPHONE = 0,              /**< OUT1 耳机输出对（R52/R53） */
    WM8978_OUTPUT_SPEAKER                     /**< OUT2 喇叭输出对（R54/R55） */
} wm8978_output_pair_t;

/** @brief 立体声输入 PGA 编程。 */
typedef struct
{
    uint8_t left_volume_code;                 /**< 左声道增益码点 0..63（-12..+35.25 dB，步进 0.75 dB） */
    uint8_t right_volume_code;                /**< 右声道增益码点 0..63（同左声道刻度） */
    bool mute;                                /**< true 静音输入 PGA */
    bool zero_cross;                          /**< true 等待信号过零再更新增益 */
} wm8978_input_pga_config_t;

/** @brief 立体声 OUT1/OUT2 输出 PGA 编程。 */
typedef struct
{
    uint8_t left_volume_code;                 /**< 左声道音量码点 0..63（-57..+6 dB，步进 1 dB，57 为 0 dB） */
    uint8_t right_volume_code;                /**< 右声道音量码点 0..63（同左声道刻度） */
    bool mute;                                /**< true 静音该输出对 */
    bool zero_cross;                          /**< true 等待信号过零再更新音量 */
} wm8978_output_volume_config_t;

/** @brief VMID 阻抗选择，对应 R1 VMIDSEL 编码。 */
typedef enum
{
    WM8978_VMID_OFF = 0,                      /**< 关闭（VMIDSEL=00） */
    WM8978_VMID_75K,                          /**< 75 kΩ（VMIDSEL=01，低功耗/慢启动） */
    WM8978_VMID_300K,                         /**< 300 kΩ（VMIDSEL=10，常用于低采样率） */
    WM8978_VMID_5K                            /**< 5 kΩ（VMIDSEL=11，最快建立，上电序列常用） */
} wm8978_vmid_t;

/** @brief 调用者持有的驱动实例。不要直接修改成员。 */
typedef struct
{
    void * io_ctx;                            /**< 调用者持有的总线/板级上下文，bind 时存入并原样透传给 io 契约函数 */
    uint32_t io_timeout_ms;                   /**< 每笔控制事务的有限超时（毫秒，非零） */
    wm8978_lifecycle_t lifecycle;             /**< 当前生命周期状态 */
    int32_t last_port_error;                  /**< 最近一次 io 契约函数的原始返回值（0 表示成功） */
    uint16_t shadow[WM8978_REGISTER_SPACE_SIZE]; /**< 软件影子缓存，按寄存器地址索引，成功写入后提交 */
} wm8978_t;

/**
 * @brief   把驱动实例绑定到 io 上下文，进入 BOUND 生命周期。
 * @details 绑定只保存指针与超时并清零影子，不访问总线；实例仍须经
 *          wm8978_soft_reset() 或 wm8978_assume_power_on_reset()
 *          同步后才能执行常规写入。
 * @param   device 驱动实例，由调用者分配并在使用期间保持存活。
 * @param   io_ctx 调用者持有的总线/板级上下文，原样透传给 io 契约
 *          函数；是否允许 NULL 由移植层解释。
 * @param   io_timeout_ms 每笔控制事务的有限超时（毫秒），必须非零。
 * @retval  WM8978_OK 绑定成功。
 * @retval  WM8978_ERR_NULL_POINTER device 为 NULL。
 * @retval  WM8978_ERR_INVALID_ARGUMENT io_timeout_ms 为 0。
 */
wm8978_status_t wm8978_bind(wm8978_t * device,
                             void * io_ctx,
                             uint32_t io_timeout_ms);

/**
 * @brief 不写控制总线，直接装载复位默认值。
 *
 * 仅当板级证据能保证刚完成 POR、或另一控制器成功写过 R0、codec
 * 已处于手册记载的复位状态时才调用。WM8978 没有外部 RESET 引脚。
 * @param   device 驱动实例。
 * @retval  WM8978_OK 影子已装载复位默认值，生命周期进入 READY。
 * @retval  WM8978_ERR_NULL_POINTER device 为 NULL。
 * @retval  WM8978_ERR_NOT_BOUND 实例尚未绑定。
 */
wm8978_status_t wm8978_assume_power_on_reset(wm8978_t * device);

/**
 * @brief 写 R0，并把软件影子缓存(shadow)同步为复位默认值。
 * @param   device 驱动实例。
 * @retval  WM8978_OK 复位写入成功，影子同步，生命周期进入 READY。
 * @retval  WM8978_ERR_NULL_POINTER device 为 NULL。
 * @retval  WM8978_ERR_NOT_BOUND 实例尚未绑定。
 * @retval  WM8978_ERR_IO 控制帧传输失败（实例进入 DESYNCHRONIZED）。
 */
wm8978_status_t wm8978_soft_reset(wm8978_t * device);

/**
 * @brief   读取实例当前生命周期。
 * @param   device 驱动实例；允许为 NULL。
 * @retval  生命周期值；device 为 NULL 时返回 WM8978_LIFECYCLE_UNBOUND。
 */
wm8978_lifecycle_t wm8978_get_lifecycle(const wm8978_t * device);

/**
 * @brief   读取最近一次 io 契约函数的原始返回值。
 * @details 与 WM8978_ERR_IO 配合定位总线故障细节；语义由移植层
 *          定义（契约约定 0 为成功、-1 为失败，实现可扩展）。
 * @param   device 驱动实例；允许为 NULL。
 * @retval  最近一次 io 层原始返回值；device 为 NULL 时返回 0。
 */
int32_t wm8978_get_last_port_error(const wm8978_t * device);

/**
 * @brief   判断地址是否属于数据手册 Table 69 的 52 个有效寄存器。
 * @param   register_address 待检查的寄存器地址。
 * @retval  true 地址有效且可经控制接口写入。
 * @retval  false 地址为保留空洞或超出寄存器空间。
 */
bool wm8978_register_is_valid(uint8_t register_address);

/**
 * @brief 把一组校验过的寄存器/值打包到 B15:B8 与 B7:B0。
 * @param   register_address 已校验的寄存器地址。
 * @param   value 已校验的 9 位寄存器值。
 * @param   frame 输出缓冲（至少 2 字节）：frame[0]=B15:B8，
 *          frame[1]=B7:B0。
 * @retval  WM8978_OK 打包成功。
 * @retval  WM8978_ERR_NULL_POINTER frame 为 NULL。
 * @retval  WM8978_ERR_INVALID_REGISTER 地址无效。
 * @retval  WM8978_ERR_RANGE value 超出 9 位。
 */
wm8978_status_t wm8978_pack_control_frame(uint8_t register_address,
                                           uint16_t value,
                                           uint8_t frame[2]);

/**
 * @brief 读取驱动影子缓存(shadow)，而非只写的硬件。
 * @param   device 驱动实例。
 * @param   register_address 寄存器地址。
 * @param   value 输出影子值；不允许为 NULL。
 * @retval  WM8978_OK 成功返回影子值。
 * @retval  WM8978_ERR_NULL_POINTER value 为 NULL。
 * @retval  WM8978_ERR_NOT_BOUND / WM8978_ERR_NOT_READY 实例未同步。
 * @retval  WM8978_ERR_DESYNCHRONIZED 实例失步，须复位恢复。
 * @retval  WM8978_ERR_INVALID_REGISTER 地址无效。
 * @retval  WM8978_ERR_NO_SHADOW R0 为非锁存命令，没有影子值。
 */
wm8978_status_t wm8978_get_shadow_register(const wm8978_t * device,
                                             uint8_t register_address,
                                             uint16_t * value);

/**
 * @brief 写入一个完整寄存器值。
 *
 * 保留位必须保持手册给出的复位值。写 R0 是软件复位并重载整个影子
 * 缓存。非锁存的更新位、以及按手册一次性软件策略处理的 NFU 位，
 * 会发送到总线上，但在 I/O 成功后从保存的影子缓存中清零。
 * 这是裸的逃生通道，不强制高层时序。但仍会返回 WM8978_ERR_STATE：
 * 当写 R18 会在任意 ADC 或 DAC 使能时改变 EQ3DMODE——硅片会拒绝
 * 该位。
 * @param   device 驱动实例。
 * @param   register_address 寄存器地址。
 * @param   value 完整 9 位寄存器值（保留位须等于复位值）。
 * @retval  WM8978_OK 写入成功且影子已提交。
 * @retval  WM8978_ERR_NULL_POINTER device 为 NULL。
 * @retval  WM8978_ERR_INVALID_REGISTER 地址无效。
 * @retval  WM8978_ERR_RANGE value 超出 9 位。
 * @retval  WM8978_ERR_RESERVED_BITS 保留位偏离复位值。
 * @retval  WM8978_ERR_NOT_BOUND / WM8978_ERR_NOT_READY 生命周期不允许。
 * @retval  WM8978_ERR_DESYNCHRONIZED 实例失步，须复位恢复。
 * @retval  WM8978_ERR_STATE 在 ADC/DAC 使能时切换 R18 EQ3DMODE。
 * @retval  WM8978_ERR_IO 控制帧传输失败（实例进入 DESYNCHRONIZED）。
 */
wm8978_status_t wm8978_write_register(wm8978_t * device,
                                       uint8_t register_address,
                                       uint16_t value);

/**
 * @brief 用已同步的软件影子缓存(shadow)更新可写位。
 *
 * 这是裸的逃生通道：保留保留位，但不强制每个字段的编码与高层
 * PLL/上电顺序规则。会拒绝在任意 ADC/DAC 使能时切换 EQ3DMODE——
 * 否则 codec 会拒绝该位，导致只写影子缓存失真。
 * @param   device 驱动实例。
 * @param   register_address 寄存器地址（不能是 R0）。
 * @param   mask 要更新的位掩码（不能触及保留位）。
 * @param   field_value 按掩码对齐的新字段值（掩码外位必须为 0）。
 * @retval  WM8978_OK 更新成功（无触发位且值未变化时跳过总线访问）。
 * @retval  WM8978_ERR_NULL_POINTER device 为 NULL。
 * @retval  WM8978_ERR_INVALID_ARGUMENT 掩码为 0、越界或与值不匹配。
 * @retval  WM8978_ERR_INVALID_REGISTER 地址无效或为 R0。
 * @retval  WM8978_ERR_RESERVED_BITS 掩码触及保留位。
 * @retval  WM8978_ERR_STATE 在 ADC/DAC 使能时切换 R18 EQ3DMODE。
 * @retval  WM8978_ERR_DESYNCHRONIZED 实例失步，须复位恢复。
 * @retval  WM8978_ERR_IO 控制帧传输失败（实例进入 DESYNCHRONIZED）。
 */
wm8978_status_t wm8978_update_bits(wm8978_t * device,
                                    uint8_t register_address,
                                    uint16_t mask,
                                    uint16_t field_value);

/**
 * @brief   一次写入 R4，配置数字音频接口。
 * @details 拒绝“右对齐 + 32 位”（手册限定右对齐最大 24 位），并
 *          拒绝 DSP 模式位与 LRC 极性语义混用；字段含义见
 *          wm8978_audio_interface_config_t。
 * @param   device 驱动实例。
 * @param   config 接口配置；不允许为 NULL。
 * @retval  WM8978_OK 写入成功。
 * @retval  WM8978_ERR_NULL_POINTER device 或 config 为 NULL。
 * @retval  WM8978_ERR_RANGE format/word_length 超出编码。
 * @retval  WM8978_ERR_UNSUPPORTED 右对齐 + 32 位组合。
 * @retval  WM8978_ERR_INVALID_ARGUMENT invert_lrc/dsp_mode_b 混用。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_configure_audio_interface(
    wm8978_t * device,
    const wm8978_audio_interface_config_t * config);

/**
 * @brief 配置 R6 时钟选择与分频。
 *
 * 选择 PLL 要求 PLLEN 已置位且 VMIDSEL 非零。退出 PLL 时，先以
 * use_pll=false 调用本函数，时钟源切换完成后再关 PLL。
 * 按数据手册要求，CLKSEL 切换后原时钟源必须再保持至少一个下降沿。
 * @param   device 驱动实例。
 * @param   config 时钟配置；不允许为 NULL。
 * @retval  WM8978_OK 更新成功。
 * @retval  WM8978_ERR_NULL_POINTER device 或 config 为 NULL。
 * @retval  WM8978_ERR_RANGE 分频档位超出编码。
 * @retval  WM8978_ERR_STATE 选择 PLL 但 PLLEN 未置位或 VMIDSEL 为 0。
 * @retval  其余同 wm8978_update_bits()。
 */
wm8978_status_t wm8978_configure_clock(
    wm8978_t * device,
    const wm8978_clock_config_t * config);

/**
 * @brief   写 R7 SR，选择数字滤波器系数组。
 * @details 只影响滤波器系数与 ALC 时间尺度，不产生采样时钟；对
 *          44.1/22.05/11.025 kHz 应选择最接近的 48/24/12 kHz 组。
 * @param   device 驱动实例。
 * @param   sample_rate_group 系数组档位。
 * @retval  WM8978_OK 更新成功。
 * @retval  WM8978_ERR_RANGE 档位超出编码。
 * @retval  其余同 wm8978_update_bits()。
 */
wm8978_status_t wm8978_set_filter_sample_rate(
    wm8978_t * device,
    wm8978_filter_sample_rate_t sample_rate_group);

/**
 * @brief 在 PLL 关闭且时钟选择 MCLK 时编程 R36-R39。
 *
 * 为同时满足 Rev 4.5 中两处冲突的范围表述，N 保守限制在 6..12。
 * K 为 24 位小数。此处 f1 指经可选 2 分频预分频器之后的 MCLK；
 * f2 选择经板级验证、靠近手册推荐的 90..100 MHz 区间的值，N 取 8
 * 附近。本函数不使能、也不选择 PLL。
 * @param   device 驱动实例。
 * @param   config PLL 比率配置；不允许为 NULL。
 * @retval  WM8978_OK 四个 PLL 寄存器全部写入成功。
 * @retval  WM8978_ERR_NULL_POINTER device 或 config 为 NULL。
 * @retval  WM8978_ERR_RANGE N 不在 6..12 或 K 超过 24 位。
 * @retval  WM8978_ERR_STATE PLL 已使能或时钟源已选 PLL。
 * @retval  WM8978_ERR_IO 中途某帧失败（实例进入 DESYNCHRONIZED，
 *          影子可能不完整）。
 */
wm8978_status_t wm8978_configure_pll(wm8978_t * device,
                                      const wm8978_pll_config_t * config);

/**
 * @brief 带时钟源/VMID 顺序安全检查地使能/关闭 PLL。
 *
 * 使用 PLL 时，板级须保证 DCVDD >= 1.9 V 并验证生成的时钟；电源
 * 电压与 PLL 锁定状态在此处均无法读取。
 * @param   device 驱动实例。
 * @param   enabled true 使能 PLL，false 关闭。
 * @retval  WM8978_OK 更新成功。
 * @retval  WM8978_ERR_STATE 时钟源已选 PLL，或使能时 VMIDSEL 为 0。
 * @retval  其余同 wm8978_update_bits()。
 */
wm8978_status_t wm8978_set_pll_enabled(wm8978_t * device, bool enabled);

/**
 * @brief 一次同步更新设置双声道 DAC 数字音量。
 *
 * 码点 0 为数字静音；码点 1 为 -127 dB；此后每码点递增 0.5 dB，
 * 码点 255 为 0 dB。
 * @param   device 驱动实例。
 * @param   left_code 左声道音量码点 0..255。
 * @param   right_code 右声道音量码点 0..255。
 * @retval  WM8978_OK 两帧写入成功，左右同步生效。
 * @retval  WM8978_ERR_IO 第二帧失败时左声道可能已更新（实例失步）。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_set_dac_digital_volume(wm8978_t * device,
                                               uint8_t left_code,
                                               uint8_t right_code);

/**
 * @brief 一次同步更新设置双声道 ADC 数字音量。
 * @details 码点刻度与 DAC 数字音量相同：0 为数字静音，1 为
 *          -127 dB，此后每码点递增 0.5 dB，255 为 0 dB。
 * @param   device 驱动实例。
 * @param   left_code 左声道音量码点 0..255。
 * @param   right_code 右声道音量码点 0..255。
 * @retval  WM8978_OK 两帧写入成功，左右同步生效。
 * @retval  WM8978_ERR_IO 第二帧失败时左声道可能已更新（实例失步）。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_set_adc_digital_volume(wm8978_t * device,
                                               uint8_t left_code,
                                               uint8_t right_code);

/**
 * @brief 一次同步更新设置双声道输入 PGA。
 *
 * 码点 0..63 表示 -12 dB 到 +35.25 dB，步进 0.75 dB。
 * @param   device 驱动实例。
 * @param   config 输入 PGA 配置；不允许为 NULL。
 * @retval  WM8978_OK 两帧写入成功，左右同步生效。
 * @retval  WM8978_ERR_NULL_POINTER device 或 config 为 NULL。
 * @retval  WM8978_ERR_RANGE 增益码点超过 63。
 * @retval  WM8978_ERR_IO 第二帧失败时左声道可能已更新（实例失步）。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_set_input_pga(
    wm8978_t * device,
    const wm8978_input_pga_config_t * config);

/**
 * @brief 设置 OUT1 耳机或 OUT2 喇叭的立体声音量对。
 *
 * 码点 0..63 表示 -57 dB 到 +6 dB，步进 1 dB。
 * @param   device 驱动实例。
 * @param   output 目标输出对（OUT1 耳机或 OUT2 喇叭）。
 * @param   config 输出音量配置；不允许为 NULL。
 * @retval  WM8978_OK 两帧写入成功，左右同步生效。
 * @retval  WM8978_ERR_NULL_POINTER device 或 config 为 NULL。
 * @retval  WM8978_ERR_RANGE 音量码点超过 63 或 output 取值非法。
 * @retval  WM8978_ERR_IO 第二帧失败时左声道可能已更新（实例失步）。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_set_output_volume(
    wm8978_t * device,
    wm8978_output_pair_t output,
    const wm8978_output_volume_config_t * config);

/**
 * @brief 置位/清除所有无歧义的模拟输出 MUTE 字段。
 *
 * 静音同时清除 OUT1/OUT2 的过零等待，避免 SLOWCLKEN 关闭时上电
 * 序列无限卡住。解除静音则保留过零等待。
 * @param   device 驱动实例。
 * @param   mute true 静音全部模拟输出，false 解除静音。
 * @retval  WM8978_OK OUT1/OUT2/OUT3/OUT4 全部更新成功。
 * @retval  WM8978_ERR_IO 多帧操作中途失败即返回（实例失步）。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_mute_analogue_outputs(wm8978_t * device, bool mute);

/**
 * @brief 执行数据手册的非 1.5x 升压(non-1.5x-boost) OUT1 上电寄存器
 * 序列。
 *
 * 调用者必须先稳定外部供电并保持 DACDAT 为零。vmid_settle_ms 由
 * 板级实测得出，必须非零。入口处 R1/R2/R3 必须全为零。该辅助函数
 * 结束时所有模拟输出保持静音；时钟/数据稳定后再显式解除静音。
 * 本函数与所有其它多帧 API 均非原子。任何端口错误都会使实例进入
 * DESYNCHRONIZED；用经确认的复位恢复，不要重试。
 * @param   device 驱动实例。
 * @param   vmid VMID 阻抗档位，不能为 WM8978_VMID_OFF。
 * @param   vmid_settle_ms 板级实测的 VMID 稳定时间（毫秒），必须非零。
 * @retval  WM8978_OK 上电序列完成，输出保持静音。
 * @retval  WM8978_ERR_INVALID_ARGUMENT vmid 或 vmid_settle_ms 非法。
 * @retval  WM8978_ERR_STATE R1/R2/R3 非零、Boost 位已置位或
 *          BUFDCOPEN 已使能。
 * @retval  WM8978_ERR_IO 序列中途总线失败（实例进入 DESYNCHRONIZED）。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_power_up_nonboost_out1(wm8978_t * device,
                                               wm8978_vmid_t vmid,
                                               uint32_t vmid_settle_ms);

/**
 * @brief 按数据手册顺序静音输出并写 R1=0、R2=0、R3=0。
 *
 * 本函数成功返回后，停止非零 DACDAT 与移除外部供电仍由调用者负责。
 * @param   device 驱动实例。
 * @retval  WM8978_OK 静音并完成 R1/R2/R3 清零。
 * @retval  WM8978_ERR_IO 多帧操作中途失败即返回（实例失步）。
 * @retval  其余同 wm8978_write_register()。
 */
wm8978_status_t wm8978_power_down(wm8978_t * device);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_H */
