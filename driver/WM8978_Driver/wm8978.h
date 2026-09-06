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
    WM8978_OK                         =  0,
    WM8978_ERR_NULL_POINTER           = -1,
    WM8978_ERR_INVALID_ARGUMENT       = -2,
    WM8978_ERR_NOT_BOUND              = -3,
    WM8978_ERR_NOT_READY              = -4,
    WM8978_ERR_RANGE                  = -5,
    WM8978_ERR_INVALID_REGISTER       = -6,
    WM8978_ERR_RESERVED_BITS          = -7,
    WM8978_ERR_IO                     = -8,
    WM8978_ERR_STATE                  = -9,
    WM8978_ERR_NO_SHADOW              = -10,
    WM8978_ERR_DELAY_REQUIRED         = -11,
    WM8978_ERR_UNSUPPORTED            = -12,
    WM8978_ERR_DESYNCHRONIZED         = -13
} wm8978_status_t;

/** @brief 驱动实例的本地生命周期。 */
typedef enum
{
    WM8978_LIFECYCLE_UNBOUND = 0,
    WM8978_LIFECYCLE_BOUND,
    WM8978_LIFECYCLE_READY,
    WM8978_LIFECYCLE_DESYNCHRONIZED
} wm8978_lifecycle_t;

/** @brief 数字音频串行格式，对应 R4 FMT 编码。 */
typedef enum
{
    WM8978_AUDIO_FORMAT_RIGHT_JUSTIFIED = 0,
    WM8978_AUDIO_FORMAT_LEFT_JUSTIFIED  = 1,
    WM8978_AUDIO_FORMAT_I2S             = 2,
    WM8978_AUDIO_FORMAT_DSP_PCM         = 3
} wm8978_audio_format_t;

/** @brief 音频字长，对应 R4 WL 编码。 */
typedef enum
{
    WM8978_WORD_LENGTH_16_BITS = 0,
    WM8978_WORD_LENGTH_20_BITS = 1,
    WM8978_WORD_LENGTH_24_BITS = 2,
    WM8978_WORD_LENGTH_32_BITS = 3
} wm8978_word_length_t;

/** @brief R4 数字音频接口配置。 */
typedef struct
{
    wm8978_audio_format_t format;
    wm8978_word_length_t word_length;
    bool invert_bclk;
    /** 右对齐/左对齐与 I2S 格式的 LRC 极性；DSP 格式必须为 false。 */
    bool invert_lrc;
    /** true 为 DSP/PCM 模式 B，false 为模式 A；其它格式必须为 false。 */
    bool dsp_mode_b;
    bool swap_dac_channels;
    bool swap_adc_channels;
    bool mono;
} wm8978_audio_interface_config_t;

/** @brief MCLK 或 PLL 输出分频，对应 R6 MCLKDIV 编码。 */
typedef enum
{
    WM8978_MCLK_DIV_1 = 0,
    WM8978_MCLK_DIV_1_5,
    WM8978_MCLK_DIV_2,
    WM8978_MCLK_DIV_3,
    WM8978_MCLK_DIV_4,
    WM8978_MCLK_DIV_6,
    WM8978_MCLK_DIV_8,
    WM8978_MCLK_DIV_12
} wm8978_mclk_div_t;

/** @brief SYSCLK 到 BCLK 的分频，对应 R6 BCLKDIV 的合法编码。 */
typedef enum
{
    WM8978_BCLK_DIV_1 = 0,
    WM8978_BCLK_DIV_2,
    WM8978_BCLK_DIV_4,
    WM8978_BCLK_DIV_8,
    WM8978_BCLK_DIV_16,
    WM8978_BCLK_DIV_32
} wm8978_bclk_div_t;

/** @brief R6 时钟源 / 主从模式配置。 */
typedef struct
{
    bool codec_is_master;
    bool use_pll;
    wm8978_mclk_div_t mclk_divider;
    wm8978_bclk_div_t bclk_divider;
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
    WM8978_FILTER_SR_48_KHZ = 0,
    WM8978_FILTER_SR_32_KHZ,
    WM8978_FILTER_SR_24_KHZ,
    WM8978_FILTER_SR_16_KHZ,
    WM8978_FILTER_SR_12_KHZ,
    WM8978_FILTER_SR_8_KHZ
} wm8978_filter_sample_rate_t;

/** @brief R36-R39 的原始 PLL 比率配置。 */
typedef struct
{
    bool divide_mclk_by_2;
    uint8_t n;
    uint32_t k;
} wm8978_pll_config_t;

/** @brief 音量编程选择的模拟输出对。 */
typedef enum
{
    WM8978_OUTPUT_HEADPHONE = 0,
    WM8978_OUTPUT_SPEAKER
} wm8978_output_pair_t;

/** @brief 立体声输入 PGA 编程。 */
typedef struct
{
    uint8_t left_volume_code;
    uint8_t right_volume_code;
    bool mute;
    bool zero_cross;
} wm8978_input_pga_config_t;

/** @brief 立体声 OUT1/OUT2 输出 PGA 编程。 */
typedef struct
{
    uint8_t left_volume_code;
    uint8_t right_volume_code;
    bool mute;
    bool zero_cross;
} wm8978_output_volume_config_t;

/** @brief VMID 阻抗选择，对应 R1 VMIDSEL 编码。 */
typedef enum
{
    WM8978_VMID_OFF = 0,
    WM8978_VMID_75K,
    WM8978_VMID_300K,
    WM8978_VMID_5K
} wm8978_vmid_t;

/** @brief 调用者持有的驱动实例。不要直接修改成员。 */
typedef struct
{
    void * io_ctx;
    uint32_t io_timeout_ms;
    wm8978_lifecycle_t lifecycle;
    int32_t last_port_error;
    uint16_t shadow[WM8978_REGISTER_SPACE_SIZE];
} wm8978_t;

wm8978_status_t wm8978_bind(wm8978_t * device,
                             void * io_ctx,
                             uint32_t io_timeout_ms);

/**
 * @brief 不写控制总线，直接装载复位默认值。
 *
 * 仅当板级证据能保证刚完成 POR、或另一控制器成功写过 R0、codec
 * 已处于手册记载的复位状态时才调用。WM8978 没有外部 RESET 引脚。
 */
wm8978_status_t wm8978_assume_power_on_reset(wm8978_t * device);

/** @brief 写 R0，并把软件影子缓存(shadow)同步为复位默认值。 */
wm8978_status_t wm8978_soft_reset(wm8978_t * device);

wm8978_lifecycle_t wm8978_get_lifecycle(const wm8978_t * device);

int32_t wm8978_get_last_port_error(const wm8978_t * device);

bool wm8978_register_is_valid(uint8_t register_address);

/** @brief 把一组校验过的寄存器/值打包到 B15:B8 与 B7:B0。 */
wm8978_status_t wm8978_pack_control_frame(uint8_t register_address,
                                           uint16_t value,
                                           uint8_t frame[2]);

/** @brief 读取驱动影子缓存(shadow)，而非只写的硬件。 */
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
 */
wm8978_status_t wm8978_update_bits(wm8978_t * device,
                                    uint8_t register_address,
                                    uint16_t mask,
                                    uint16_t field_value);

wm8978_status_t wm8978_configure_audio_interface(
    wm8978_t * device,
    const wm8978_audio_interface_config_t * config);

/**
 * @brief 配置 R6 时钟选择与分频。
 *
 * 选择 PLL 要求 PLLEN 已置位且 VMIDSEL 非零。退出 PLL 时，先以
 * use_pll=false 调用本函数，时钟源切换完成后再关 PLL。
 * 按数据手册要求，CLKSEL 切换后原时钟源必须再保持至少一个下降沿。
 */
wm8978_status_t wm8978_configure_clock(
    wm8978_t * device,
    const wm8978_clock_config_t * config);

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
 */
wm8978_status_t wm8978_configure_pll(wm8978_t * device,
                                      const wm8978_pll_config_t * config);

/**
 * @brief 带时钟源/VMID 顺序安全检查地使能/关闭 PLL。
 *
 * 使用 PLL 时，板级须保证 DCVDD >= 1.9 V 并验证生成的时钟；电源
 * 电压与 PLL 锁定状态在此处均无法读取。
 */
wm8978_status_t wm8978_set_pll_enabled(wm8978_t * device, bool enabled);

/**
 * @brief 一次同步更新设置双声道 DAC 数字音量。
 *
 * 码点 0 为数字静音；码点 1 为 -127 dB；此后每码点递增 0.5 dB，
 * 码点 255 为 0 dB。
 */
wm8978_status_t wm8978_set_dac_digital_volume(wm8978_t * device,
                                               uint8_t left_code,
                                               uint8_t right_code);

/** @brief 一次同步更新设置双声道 ADC 数字音量。 */
wm8978_status_t wm8978_set_adc_digital_volume(wm8978_t * device,
                                               uint8_t left_code,
                                               uint8_t right_code);

/**
 * @brief 一次同步更新设置双声道输入 PGA。
 *
 * 码点 0..63 表示 -12 dB 到 +35.25 dB，步进 0.75 dB。
 */
wm8978_status_t wm8978_set_input_pga(
    wm8978_t * device,
    const wm8978_input_pga_config_t * config);

/**
 * @brief 设置 OUT1 耳机或 OUT2 喇叭的立体声音量对。
 *
 * 码点 0..63 表示 -57 dB 到 +6 dB，步进 1 dB。
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
 */
wm8978_status_t wm8978_power_up_nonboost_out1(wm8978_t * device,
                                               wm8978_vmid_t vmid,
                                               uint32_t vmid_settle_ms);

/**
 * @brief 按数据手册顺序静音输出并写 R1=0、R2=0、R3=0。
 *
 * 本函数成功返回后，停止非零 DACDAT 与移除外部供电仍由调用者负责。
 */
wm8978_status_t wm8978_power_down(wm8978_t * device);

#ifdef __cplusplus
}
#endif

#endif /* WM8978_H */
