/**
 * @file    drv2605l.c
 * @brief   DRV2605L 通用触觉电机驱动核心
 * @details 通过 drv2605l_io.h 完成单字节 I2C 寄存器访问，提供 ROM 波形
 *          序列和 RTP 实时播放的状态编排。核心不包含任何平台代码。
 * @note    仅依赖本模块头文件与 C99 标准类型，不在 ISR 中执行。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-01
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "drv2605l.h"
#include "drv2605l_conf.h"
#include "drv2605l_regs.h"
#include "drv2605l_io.h"

/* ══════════════════════════ 私有辅助函数 ══════════════════════════ */

static bool
drv2605l_is_ready(const drv2605l_dev_t *dev)
{
    return (dev != NULL) && (dev->inited != 0u);
}

static bool
drv2605l_is_valid_reg(uint8_t reg)
{
    return reg <= DRV2605L_REG_LAST;
}

static drv2605l_result_t
drv2605l_map_io_result(int io_result)
{
    if (io_result == DRV2605L_IO_OK)
    {
        return DRV2605L_OK;
    }

    return DRV2605L_ERR_IO;
}

static void
drv2605l_lock(drv2605l_dev_t *dev)
{
#if DRV2605L_THREAD_SAFE
    drv2605l_io_lock(dev->io_ctx);
#else
    (void)dev;
#endif
}

static void
drv2605l_unlock(drv2605l_dev_t *dev)
{
#if DRV2605L_THREAD_SAFE
    drv2605l_io_unlock(dev->io_ctx);
#else
    (void)dev;
#endif
}

static drv2605l_result_t
drv2605l_read_reg_internal(drv2605l_dev_t *dev, uint8_t reg, uint8_t *value)
{
    return drv2605l_map_io_result(
        drv2605l_io_read_reg(dev->io_ctx, dev->dev_addr, reg, value));
}

static drv2605l_result_t
drv2605l_write_reg_internal(drv2605l_dev_t *dev, uint8_t reg, uint8_t value)
{
    return drv2605l_map_io_result(
        drv2605l_io_write_reg(dev->io_ctx, dev->dev_addr, reg, value));
}

static drv2605l_result_t
drv2605l_write_persistent(drv2605l_dev_t *dev, uint8_t reg, uint8_t value,
                          uint8_t verify_mask)
{
    drv2605l_result_t result;
#if DRV2605L_VERIFY_WRITES
    uint8_t actual;
#endif

    result = drv2605l_write_reg_internal(dev, reg, value);

#if DRV2605L_VERIFY_WRITES
    if ((result == DRV2605L_OK) && (verify_mask != 0u))
    {
        result = drv2605l_read_reg_internal(dev, reg, &actual);
        if ((result == DRV2605L_OK) &&
            ((actual & verify_mask) != (value & verify_mask)))
        {
            result = DRV2605L_ERR_VERIFY;
        }
    }
#else
    (void)verify_mask;
#endif

    return result;
}

static drv2605l_result_t
drv2605l_update_bits_internal(drv2605l_dev_t *dev, uint8_t reg, uint8_t mask,
                             uint8_t value, uint8_t verify_mask)
{
    drv2605l_result_t result;
    uint8_t current;
    uint8_t updated;

    result = drv2605l_read_reg_internal(dev, reg, &current);
    if (result != DRV2605L_OK)
    {
        return result;
    }

    updated = (uint8_t)((current & (uint8_t)(~mask)) | (value & mask));
    if (updated == current)
    {
        return DRV2605L_OK;
    }

    return drv2605l_write_persistent(dev, reg, updated, verify_mask);
}

#if DRV2605L_USE_SEQUENCE

static bool
drv2605l_is_valid_sequence_value(uint8_t value)
{
    if (value == 0u)
    {
        return true;
    }

    if ((value & DRV2605L_SEQUENCE_WAIT_MASK) != 0u)
    {
        return true;
    }

    return value <= DRV2605L_WAVEFORM_ID_MAX;
}

#endif /* DRV2605L_USE_SEQUENCE */

/* ══════════════════════════ 生命周期与原始访问 ══════════════════════════ */

drv2605l_result_t
drv2605l_init(drv2605l_dev_t *dev, void *io_ctx, uint8_t dev_addr)
{
    drv2605l_result_t result;
    uint8_t address;
    uint8_t status;
    uint8_t device_id;
    uint8_t default_mode;
    uint8_t default_actuator;
    uint8_t default_library;
    uint8_t default_standby;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (dev_addr == 0u)
    {
        address = DRV2605L_I2C_ADDR;
    }
    else if (dev_addr == DRV2605L_I2C_ADDR)
    {
        address = dev_addr;
    }
    else
    {
        return DRV2605L_ERR_PARAM;
    }

    dev->io_ctx = io_ctx;
    dev->dev_addr = address;
    dev->inited = 0u;

    default_mode = (uint8_t)(DRV2605L_DEFAULT_MODE &
                             DRV2605L_MODE_VALUE_MASK);
    default_actuator = (DRV2605L_DEFAULT_ACTUATOR != 0) ?
                       DRV2605L_FEEDBACK_LRA_MASK : 0u;
    default_library = (uint8_t)(DRV2605L_DEFAULT_LIBRARY &
                                DRV2605L_LIBRARY_VALUE_MASK);
    default_standby = (DRV2605L_DEFAULT_STANDBY != 0) ?
                      DRV2605L_MODE_STANDBY : 0u;

    result = DRV2605L_OK;
    drv2605l_lock(dev);

    if (drv2605l_io_init(io_ctx) != DRV2605L_IO_OK)
    {
        result = DRV2605L_ERR_IO;
        goto drv2605l_init_exit;
    }

    drv2605l_io_delay_ms(DRV2605L_POWERUP_DELAY_MS);

    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_STATUS, &status);
    if (result != DRV2605L_OK)
    {
        goto drv2605l_init_exit;
    }

    device_id = (uint8_t)((status & DRV2605L_STATUS_DEVICE_ID_MASK) >>
                          DRV2605L_STATUS_DEVICE_ID_SHIFT);
    if (device_id != DRV2605L_DEVICE_ID_EXPECTED)
    {
        result = DRV2605L_ERR_NOT_SUPPORTED;
        goto drv2605l_init_exit;
    }

    result = drv2605l_update_bits_internal(
        dev,
        DRV2605L_REG_MODE,
        (uint8_t)(DRV2605L_MODE_VALUE_MASK | DRV2605L_MODE_STANDBY),
        default_mode,
        (uint8_t)(DRV2605L_MODE_VALUE_MASK | DRV2605L_MODE_STANDBY));
    if (result != DRV2605L_OK)
    {
        goto drv2605l_init_exit;
    }

    result = drv2605l_update_bits_internal(
        dev,
        DRV2605L_REG_FEEDBACK_CTRL,
        DRV2605L_FEEDBACK_LRA_MASK,
        default_actuator,
        DRV2605L_FEEDBACK_LRA_MASK);
    if (result != DRV2605L_OK)
    {
        goto drv2605l_init_exit;
    }

    result = drv2605l_update_bits_internal(
        dev,
        DRV2605L_REG_LIBRARY_SELECTION,
        DRV2605L_LIBRARY_VALUE_MASK,
        default_library,
        DRV2605L_LIBRARY_VALUE_MASK);
    if (result != DRV2605L_OK)
    {
        goto drv2605l_init_exit;
    }

    result = drv2605l_update_bits_internal(
        dev,
        DRV2605L_REG_MODE,
        DRV2605L_MODE_STANDBY,
        default_standby,
        DRV2605L_MODE_STANDBY);

drv2605l_init_exit:
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        dev->inited = 1u;
    }

    return result;
}

drv2605l_result_t
drv2605l_reset(drv2605l_dev_t *dev)
{
    drv2605l_result_t result;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_write_reg_internal(dev, DRV2605L_REG_MODE,
                                         DRV2605L_MODE_DEV_RESET);
    drv2605l_io_delay_ms(DRV2605L_POWERUP_DELAY_MS);
    drv2605l_unlock(dev);

    dev->inited = 0u;
    return result;
}

drv2605l_result_t
drv2605l_read_reg(drv2605l_dev_t *dev, uint8_t reg, uint8_t *value)
{
    drv2605l_result_t result;

    if ((dev == NULL) || (value == NULL) || !drv2605l_is_valid_reg(reg))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, reg, value);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_write_reg(drv2605l_dev_t *dev, uint8_t reg, uint8_t value)
{
    drv2605l_result_t result;

    if ((dev == NULL) || !drv2605l_is_valid_reg(reg))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_write_reg_internal(dev, reg, value);
    drv2605l_unlock(dev);

    return result;
}

/* ══════════════════════════ 通用配置 ══════════════════════════ */

drv2605l_result_t
drv2605l_set_mode(drv2605l_dev_t *dev, drv2605l_mode_t mode)
{
    drv2605l_result_t result;

    if ((dev == NULL) || ((uint8_t)mode > DRV2605L_MODE_AUTO_CALIBRATION))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_update_bits_internal(
        dev, DRV2605L_REG_MODE, DRV2605L_MODE_VALUE_MASK, (uint8_t)mode,
        DRV2605L_MODE_VALUE_MASK);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_get_mode(drv2605l_dev_t *dev, drv2605l_mode_t *mode)
{
    drv2605l_result_t result;
    uint8_t value;

    if ((dev == NULL) || (mode == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_MODE, &value);
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        *mode = (drv2605l_mode_t)(value & DRV2605L_MODE_VALUE_MASK);
    }

    return result;
}

drv2605l_result_t
drv2605l_set_standby(drv2605l_dev_t *dev, bool enable)
{
    drv2605l_result_t result;
    uint8_t value;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    value = enable ? DRV2605L_MODE_STANDBY : 0u;
    drv2605l_lock(dev);
    result = drv2605l_update_bits_internal(
        dev, DRV2605L_REG_MODE, DRV2605L_MODE_STANDBY, value,
        DRV2605L_MODE_STANDBY);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_get_standby(drv2605l_dev_t *dev, bool *enable)
{
    drv2605l_result_t result;
    uint8_t value;

    if ((dev == NULL) || (enable == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_MODE, &value);
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        *enable = (value & DRV2605L_MODE_STANDBY) != 0u;
    }

    return result;
}

drv2605l_result_t
drv2605l_set_actuator(drv2605l_dev_t *dev, drv2605l_actuator_t actuator)
{
    drv2605l_result_t result;
    uint8_t value;

    if ((dev == NULL) || ((uint8_t)actuator > DRV2605L_ACTUATOR_LRA))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    value = (actuator == DRV2605L_ACTUATOR_LRA) ?
            DRV2605L_FEEDBACK_LRA_MASK : 0u;
    drv2605l_lock(dev);
    result = drv2605l_update_bits_internal(
        dev, DRV2605L_REG_FEEDBACK_CTRL, DRV2605L_FEEDBACK_LRA_MASK, value,
        DRV2605L_FEEDBACK_LRA_MASK);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_get_actuator(drv2605l_dev_t *dev, drv2605l_actuator_t *actuator)
{
    drv2605l_result_t result;
    uint8_t value;

    if ((dev == NULL) || (actuator == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_FEEDBACK_CTRL,
                                        &value);
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        *actuator = ((value & DRV2605L_FEEDBACK_LRA_MASK) != 0u) ?
                    DRV2605L_ACTUATOR_LRA : DRV2605L_ACTUATOR_ERM;
    }

    return result;
}

drv2605l_result_t
drv2605l_set_library(drv2605l_dev_t *dev, drv2605l_library_t library)
{
    drv2605l_result_t result;

    if ((dev == NULL) || ((uint8_t)library > DRV2605L_LIBRARY_ERM_F))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_update_bits_internal(
        dev, DRV2605L_REG_LIBRARY_SELECTION,
        DRV2605L_LIBRARY_VALUE_MASK, (uint8_t)library,
        DRV2605L_LIBRARY_VALUE_MASK);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_get_library(drv2605l_dev_t *dev, drv2605l_library_t *library)
{
    drv2605l_result_t result;
    uint8_t value;

    if ((dev == NULL) || (library == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_LIBRARY_SELECTION,
                                        &value);
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        *library = (drv2605l_library_t)(value &
                                        DRV2605L_LIBRARY_VALUE_MASK);
    }

    return result;
}

#if DRV2605L_USE_RTP

drv2605l_result_t
drv2605l_set_rtp_format(drv2605l_dev_t *dev, bool unsigned_format)
{
    drv2605l_result_t result;
    uint8_t value;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    value = unsigned_format ? DRV2605L_CONTROL3_DATA_FORMAT_RTP : 0u;
    drv2605l_lock(dev);
    result = drv2605l_update_bits_internal(
        dev, DRV2605L_REG_CONTROL3, DRV2605L_CONTROL3_DATA_FORMAT_RTP, value,
        DRV2605L_CONTROL3_DATA_FORMAT_RTP);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_get_rtp_format(drv2605l_dev_t *dev, bool *unsigned_format)
{
    drv2605l_result_t result;
    uint8_t value;

    if ((dev == NULL) || (unsigned_format == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_CONTROL3, &value);
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        *unsigned_format = (value & DRV2605L_CONTROL3_DATA_FORMAT_RTP) != 0u;
    }

    return result;
}

#endif /* DRV2605L_USE_RTP */

#if DRV2605L_USE_SEQUENCE

/* ══════════════════════════ ROM 波形序列器 ══════════════════════════ */

drv2605l_result_t
drv2605l_set_sequence(drv2605l_dev_t *dev,
                      const uint8_t sequence[DRV2605L_SEQUENCE_SLOTS])
{
    drv2605l_result_t result;
    uint8_t index;

    if ((dev == NULL) || (sequence == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    for (index = 0u; index < DRV2605L_SEQUENCE_SLOTS; index++)
    {
        if (!drv2605l_is_valid_sequence_value(sequence[index]))
        {
            return DRV2605L_ERR_PARAM;
        }
    }

    result = DRV2605L_OK;
    drv2605l_lock(dev);
    for (index = 0u; index < DRV2605L_SEQUENCE_SLOTS; index++)
    {
        result = drv2605l_write_persistent(
            dev, (uint8_t)(DRV2605L_REG_WAVEFORM_SEQ1 + index), sequence[index],
            0xFFu);
        if (result != DRV2605L_OK)
        {
            break;
        }
    }
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_get_sequence(drv2605l_dev_t *dev,
                      uint8_t sequence[DRV2605L_SEQUENCE_SLOTS])
{
    drv2605l_result_t result;
    uint8_t values[DRV2605L_SEQUENCE_SLOTS];
    uint8_t index;

    if ((dev == NULL) || (sequence == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    result = DRV2605L_OK;
    drv2605l_lock(dev);
    for (index = 0u; index < DRV2605L_SEQUENCE_SLOTS; index++)
    {
        result = drv2605l_read_reg_internal(
            dev, (uint8_t)(DRV2605L_REG_WAVEFORM_SEQ1 + index), &values[index]);
        if (result != DRV2605L_OK)
        {
            break;
        }
    }
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        for (index = 0u; index < DRV2605L_SEQUENCE_SLOTS; index++)
        {
            sequence[index] = values[index];
        }
    }

    return result;
}

drv2605l_result_t
drv2605l_play_sequence(drv2605l_dev_t *dev)
{
    drv2605l_result_t result;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_update_bits_internal(
        dev, DRV2605L_REG_MODE,
        (uint8_t)(DRV2605L_MODE_VALUE_MASK | DRV2605L_MODE_STANDBY),
        DRV2605L_MODE_INTERNAL_TRIGGER,
        (uint8_t)(DRV2605L_MODE_VALUE_MASK | DRV2605L_MODE_STANDBY));
    if (result == DRV2605L_OK)
    {
        result = drv2605l_write_reg_internal(dev, DRV2605L_REG_GO,
                                             DRV2605L_GO_BIT);
    }
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_stop_sequence(drv2605l_dev_t *dev)
{
    drv2605l_result_t result;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_write_reg_internal(dev, DRV2605L_REG_GO, 0u);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_is_sequence_playing(drv2605l_dev_t *dev, bool *playing)
{
    drv2605l_result_t result;
    uint8_t value;

    if ((dev == NULL) || (playing == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_GO, &value);
    drv2605l_unlock(dev);

    if (result == DRV2605L_OK)
    {
        *playing = (value & DRV2605L_GO_BIT) != 0u;
    }

    return result;
}

#endif /* DRV2605L_USE_SEQUENCE */

#if DRV2605L_USE_RTP

/* ══════════════════════════ RTP 实时播放 ══════════════════════════ */

drv2605l_result_t
drv2605l_start_rtp(drv2605l_dev_t *dev, uint8_t value)
{
    drv2605l_result_t result;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_write_persistent(dev, DRV2605L_REG_RTP_INPUT, value,
                                       0xFFu);
    if (result == DRV2605L_OK)
    {
        result = drv2605l_update_bits_internal(
            dev, DRV2605L_REG_MODE,
            (uint8_t)(DRV2605L_MODE_VALUE_MASK | DRV2605L_MODE_STANDBY),
            DRV2605L_MODE_RTP,
            (uint8_t)(DRV2605L_MODE_VALUE_MASK | DRV2605L_MODE_STANDBY));
    }
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_set_rtp_input(drv2605l_dev_t *dev, uint8_t value)
{
    drv2605l_result_t result;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_write_persistent(dev, DRV2605L_REG_RTP_INPUT, value,
                                       0xFFu);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_get_rtp_input(drv2605l_dev_t *dev, uint8_t *value)
{
    drv2605l_result_t result;

    if ((dev == NULL) || (value == NULL))
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_read_reg_internal(dev, DRV2605L_REG_RTP_INPUT, value);
    drv2605l_unlock(dev);

    return result;
}

drv2605l_result_t
drv2605l_stop_rtp(drv2605l_dev_t *dev)
{
    drv2605l_result_t result;

    if (dev == NULL)
    {
        return DRV2605L_ERR_PARAM;
    }

    if (!drv2605l_is_ready(dev))
    {
        return DRV2605L_ERR_NOT_READY;
    }

    drv2605l_lock(dev);
    result = drv2605l_write_persistent(dev, DRV2605L_REG_RTP_INPUT, 0u,
                                       0xFFu);
    if (result == DRV2605L_OK)
    {
        result = drv2605l_update_bits_internal(
            dev, DRV2605L_REG_MODE, DRV2605L_MODE_VALUE_MASK,
            DRV2605L_MODE_INTERNAL_TRIGGER, DRV2605L_MODE_VALUE_MASK);
    }
    drv2605l_unlock(dev);

    return result;
}

#endif /* DRV2605L_USE_RTP */
