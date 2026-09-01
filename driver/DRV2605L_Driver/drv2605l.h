/**
 * @file    drv2605l.h
 * @brief   DRV2605L 通用触觉电机驱动公共 API
 * @details 提供 ROM 波形序列、RTP 实时幅度、模式、待机、执行器类型、
 *          ROM 库选择及原始寄存器访问。核心不依赖任何 MCU、HAL、RTOS
 *          或具体 I2C 控制器。
 * @note    EN 与 IN/TRIG 为外部引脚，由板级代码管理；所有 API 均在普通
 *          线程上下文调用，不能在 ISR 中执行 I2C 或延时。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-01
 */

#ifndef DRV2605L_H
#define DRV2605L_H

#include <stdbool.h>
#include <stdint.h>

#include "drv2605l_conf.h"
#include "drv2605l_regs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ══════════════════════════ 结果码 ══════════════════════════ */

typedef enum {
    DRV2605L_OK = 0,             /**< 操作成功 */
    DRV2605L_ERR_IO,             /**< I2C 通信失败 */
    DRV2605L_ERR_PARAM,          /**< 空指针、地址或参数非法 */
    DRV2605L_ERR_NOT_READY,      /**< 未初始化或句柄已被复位 */
    DRV2605L_ERR_NOT_SUPPORTED,  /**< 器件 ID 不匹配或功能不支持 */
    DRV2605L_ERR_VERIFY          /**< 写后回读校验失败 */
} drv2605l_result_t;

/* ══════════════════════════ 模式与器件枚举 ══════════════════════════ */

typedef enum {
    DRV2605L_MODE_INTERNAL_TRIGGER = 0, /**< I2C GO 位触发 ROM 波形 */
    DRV2605L_MODE_EXTERNAL_EDGE     = 1, /**< IN/TRIG 上升沿触发 */
    DRV2605L_MODE_EXTERNAL_LEVEL    = 2, /**< IN/TRIG 电平触发 */
    DRV2605L_MODE_PWM_ANALOG        = 3, /**< PWM 或模拟输入 */
    DRV2605L_MODE_AUDIO_TO_VIBE     = 4, /**< Audio-to-vibe */
    DRV2605L_MODE_RTP               = 5, /**< RTP 实时播放 */
    DRV2605L_MODE_DIAGNOSTICS       = 6, /**< 诊断流程 */
    DRV2605L_MODE_AUTO_CALIBRATION  = 7  /**< 自动校准流程 */
} drv2605l_mode_t;

typedef enum {
    DRV2605L_ACTUATOR_ERM = 0, /**< 偏心转子电机 */
    DRV2605L_ACTUATOR_LRA = 1  /**< 线性谐振执行器 */
} drv2605l_actuator_t;

typedef enum {
    DRV2605L_LIBRARY_EMPTY = 0, /**< 空库 */
    DRV2605L_LIBRARY_ERM_A  = 1, /**< TS2200 Library A */
    DRV2605L_LIBRARY_ERM_B  = 2, /**< TS2200 Library B */
    DRV2605L_LIBRARY_ERM_C  = 3, /**< TS2200 Library C */
    DRV2605L_LIBRARY_ERM_D  = 4, /**< TS2200 Library D */
    DRV2605L_LIBRARY_ERM_E  = 5, /**< TS2200 Library E */
    DRV2605L_LIBRARY_LRA    = 6, /**< LRA Library */
    DRV2605L_LIBRARY_ERM_F  = 7  /**< TS2200 Library F */
} drv2605l_library_t;

/* ══════════════════════════ 设备句柄 ══════════════════════════ */

typedef struct {
    void    *io_ctx;     /**< 总线上下文，由调用者拥有并负责保持有效 */
    uint8_t dev_addr;    /**< 7 位 I2C 地址，DRV2605L 固定为 0x5A */
    uint8_t inited;      /**< 初始化成功为 1，复位或失败后为 0 */
} drv2605l_dev_t;

/* ══════════════════════════ 生命周期与原始访问 ══════════════════════════ */

/**
 * @brief   初始化 DRV2605L 设备
 * @details 调用 IO 初始化，等待至少 1ms，读取 Status.DEVICE_ID 验证器件，
 *          写入默认模式、执行器类型和 ROM 库，最后按配置进入待机。
 * @param   dev 由调用者分配的设备句柄。
 * @param   io_ctx 总线上下文，允许为 NULL。
 * @param   dev_addr 7 位地址；传 0 使用默认地址 0x5A。
 * @retval  DRV2605L_OK 初始化成功。
 * @retval  DRV2605L_ERR_PARAM 句柄为空或地址不是 0/0x5A。
 * @retval  DRV2605L_ERR_IO 总线初始化或读写失败。
 * @retval  DRV2605L_ERR_NOT_SUPPORTED 读取到的器件 ID 不是 DRV2605L。
 * @retval  DRV2605L_ERR_VERIFY 默认配置写回读校验失败。
 * @note    线程上下文调用；调用前必须由板级代码将 EN 置高。
 */
drv2605l_result_t drv2605l_init(drv2605l_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr);

/**
 * @brief   发出软件复位
 * @details 写入 MODE.DEV_RESET 后等待至少 1ms，并立即使句柄失效；复位
 *          会清除芯片配置，后续必须重新调用 drv2605l_init()。
 * @param   dev 已初始化的设备句柄。
 * @retval  DRV2605L_OK 复位命令已写入。
 * @retval  DRV2605L_ERR_PARAM 句柄为空。
 * @retval  DRV2605L_ERR_NOT_READY 设备尚未初始化。
 * @retval  DRV2605L_ERR_IO 写入命令失败。
 * @note    即使写事务返回错误也会执行 1ms 延时，因为器件可能已接受命令。
 */
drv2605l_result_t drv2605l_reset(drv2605l_dev_t *dev);

/**
 * @brief   原始读取一个寄存器
 * @details 仅校验寄存器地址范围，不解释寄存器访问权限和副作用；Status
 *          寄存器的诊断/温度/过流标志可能在读取时清除。
 * @param   dev 已初始化的设备句柄。
 * @param   reg 寄存器地址，范围 0x00–0x22。
 * @param   value 输出的寄存器值。
 * @retval  DRV2605L_OK 读取成功。
 * @retval  DRV2605L_ERR_PARAM 参数为空或寄存器地址超范围。
 * @retval  DRV2605L_ERR_NOT_READY 设备尚未初始化。
 * @retval  DRV2605L_ERR_IO I2C 读取失败。
 * @note    线程上下文调用，不能在 ISR 中执行。
 */
drv2605l_result_t drv2605l_read_reg(drv2605l_dev_t *dev, uint8_t reg,
                                    uint8_t *value);

/**
 * @brief   原始写入一个寄存器
 * @details 仅校验寄存器地址范围，不屏蔽保留位、不自动校验写回，也不
 *          阻止写入只读或一次性 OTP 寄存器；高级用法必须遵守数据手册。
 * @param   dev 已初始化的设备句柄。
 * @param   reg 寄存器地址，范围 0x00–0x22。
 * @param   value 待写入值。
 * @retval  DRV2605L_OK 写入成功。
 * @retval  DRV2605L_ERR_PARAM 句柄为空或寄存器地址超范围。
 * @retval  DRV2605L_ERR_NOT_READY 设备尚未初始化。
 * @retval  DRV2605L_ERR_IO I2C 写入失败。
 * @note    线程上下文调用；不建议在 ISR 中执行。
 */
drv2605l_result_t drv2605l_write_reg(drv2605l_dev_t *dev, uint8_t reg,
                                     uint8_t value);

/* ══════════════════════════ 通用配置 ══════════════════════════ */

/**
 * @brief   设置接口模式
 * @param   dev 已初始化的设备句柄。
 * @param   mode 0–7 对应 drv2605l_mode_t。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 * @note    线程上下文调用；诊断和自动校准模式仅改变 MODE，不自动执行流程。
 */
drv2605l_result_t drv2605l_set_mode(drv2605l_dev_t *dev,
                                    drv2605l_mode_t mode);

/**
 * @brief   读取接口模式
 * @param   dev 已初始化的设备句柄。
 * @param   mode 输出当前模式。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_get_mode(drv2605l_dev_t *dev,
                                    drv2605l_mode_t *mode);

/**
 * @brief   设置软件待机状态
 * @param   dev 已初始化的设备句柄。
 * @param   enable true 进入待机，false 退出待机。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 */
drv2605l_result_t drv2605l_set_standby(drv2605l_dev_t *dev, bool enable);

/**
 * @brief   读取软件待机状态
 * @param   dev 已初始化的设备句柄。
 * @param   enable 输出当前待机状态。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_get_standby(drv2605l_dev_t *dev, bool *enable);

/**
 * @brief   设置执行器类型
 * @details 仅修改 Feedback.N_ERM_LRA，不自动改变 ROM 库；切换到 LRA 后
 *          应由调用者显式选择 DRV2605L_LIBRARY_LRA。
 * @param   dev 已初始化的设备句柄。
 * @param   actuator ERM 或 LRA。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 */
drv2605l_result_t drv2605l_set_actuator(drv2605l_dev_t *dev,
                                        drv2605l_actuator_t actuator);

/**
 * @brief   读取执行器类型
 * @param   dev 已初始化的设备句柄。
 * @param   actuator 输出当前执行器类型。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_get_actuator(drv2605l_dev_t *dev,
                                        drv2605l_actuator_t *actuator);

/**
 * @brief   设置 ROM 波形库
 * @details 库编号遵循数据手册 Library Selection：0 为空库，1–5 和 7 为
 *          ERM 库，6 为 LRA 库。
 * @param   dev 已初始化的设备句柄。
 * @param   library ROM 库编号。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 */
drv2605l_result_t drv2605l_set_library(drv2605l_dev_t *dev,
                                       drv2605l_library_t library);

/**
 * @brief   读取 ROM 波形库
 * @param   dev 已初始化的设备句柄。
 * @param   library 输出当前库编号。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_get_library(drv2605l_dev_t *dev,
                                       drv2605l_library_t *library);

#if DRV2605L_USE_RTP

/**
 * @brief   设置 RTP 数据格式
 * @details false 表示默认的有符号补码格式，true 表示无符号 0–255 格式。
 * @param   dev 已初始化的设备句柄。
 * @param   unsigned_format true 使用无符号格式。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 */
drv2605l_result_t drv2605l_set_rtp_format(drv2605l_dev_t *dev,
                                          bool unsigned_format);

/**
 * @brief   读取 RTP 数据格式
 * @param   dev 已初始化的设备句柄。
 * @param   unsigned_format 输出 true 表示无符号格式。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_get_rtp_format(drv2605l_dev_t *dev,
                                          bool *unsigned_format);

#endif /* DRV2605L_USE_RTP */

#if DRV2605L_USE_SEQUENCE

/* ══════════════════════════ ROM 波形序列器 ══════════════════════════ */

/**
 * @brief   写入完整的 8 槽 ROM 波形序列
 * @details sequence[0] 对应槽 1。值 0 表示终止；1–123 表示波形编号；
 *          置位 bit7 表示等待，低 7 位为等待 10ms 的倍数。函数会先验证
 *          全部 8 项，再逐项写入，失败时不回滚已成功写入的槽。
 * @param   dev 已初始化的设备句柄。
 * @param   sequence 8 字节序列数组。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 * @note    线程上下文调用；序列写入不使用 burst 事务。
 */
drv2605l_result_t drv2605l_set_sequence(
    drv2605l_dev_t *dev,
    const uint8_t sequence[DRV2605L_SEQUENCE_SLOTS]);

/**
 * @brief   读取完整的 8 槽 ROM 波形序列
 * @param   dev 已初始化的设备句柄。
 * @param   sequence 输出 8 字节序列数组。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_get_sequence(
    drv2605l_dev_t *dev,
    uint8_t sequence[DRV2605L_SEQUENCE_SLOTS]);

/**
 * @brief   触发当前 ROM 波形序列
 * @details 清除软件待机、切换到内部触发模式并置位 GO。调用前应先写入
 *          序列和正确的 ROM 库。
 * @param   dev 已初始化的设备句柄。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 * @note    线程上下文调用，GO 运行期间可由 stop_sequence() 取消。
 */
drv2605l_result_t drv2605l_play_sequence(drv2605l_dev_t *dev);

/**
 * @brief   取消当前 ROM 波形序列
 * @param   dev 已初始化的设备句柄。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_stop_sequence(drv2605l_dev_t *dev);

/**
 * @brief   查询 ROM 波形序列是否仍在播放
 * @param   dev 已初始化的设备句柄。
 * @param   playing 输出 true 表示 GO 位为 1。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_is_sequence_playing(drv2605l_dev_t *dev,
                                               bool *playing);

#endif /* DRV2605L_USE_SEQUENCE */

#if DRV2605L_USE_RTP

/* ══════════════════════════ RTP 实时播放 ══════════════════════════ */

/**
 * @brief   启动 RTP 实时播放
 * @details 先写入 RTP_INPUT，再清除待机并切换到 RTP 模式；value 是原始
 *          8 位数据，具体物理含义由当前 RTP 格式和反馈模式决定。
 * @param   dev 已初始化的设备句柄。
 * @param   value RTP 原始输入值，无物理单位。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 * @note    线程上下文调用。
 */
drv2605l_result_t drv2605l_start_rtp(drv2605l_dev_t *dev, uint8_t value);

/**
 * @brief   更新 RTP 实时输入值
 * @param   dev 已初始化的设备句柄。
 * @param   value RTP 原始输入值，无物理单位。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 */
drv2605l_result_t drv2605l_set_rtp_input(drv2605l_dev_t *dev, uint8_t value);

/**
 * @brief   读取 RTP 实时输入值
 * @param   dev 已初始化的设备句柄。
 * @param   value 输出 RTP 原始输入值。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM 或 ERR_NOT_READY。
 */
drv2605l_result_t drv2605l_get_rtp_input(drv2605l_dev_t *dev, uint8_t *value);

/**
 * @brief   停止 RTP 播放
 * @details 将 RTP 输入清零并切回内部触发模式，保留当前 STANDBY 位。
 * @param   dev 已初始化的设备句柄。
 * @retval  drv2605l_result_t OK、ERR_IO、ERR_PARAM、ERR_NOT_READY 或
 *          ERR_VERIFY。
 */
drv2605l_result_t drv2605l_stop_rtp(drv2605l_dev_t *dev);

#endif /* DRV2605L_USE_RTP */

#ifdef __cplusplus
}
#endif

#endif /* DRV2605L_H */
