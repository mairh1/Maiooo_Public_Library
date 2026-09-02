/**
 * @file    max17260.c
 * @brief   MAX17260 ModelGauge m5 EZ 电量计驱动核心
 * @details 纯 C99、无动态内存、无平台头文件，全部硬件访问经由
 *          max17260_io.h 声明的移植契约函数完成。
 *          实现依据本目录数据手册（MAX17260, 19-100249; Rev 2; 7/24）。
 * @note    所有换算为定点运算，单位约定见 max17260.h 文件头。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-02
 */

#include "max17260.h"
#include "max17260_regs.h"
#include "max17260_io.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 私有常量与宏
 * ════════════════════════════════════════════════════════════════════════ */

#if MAX17260_VARIANT == MAX17260_VARIANT_B_SERIES
#define MAX17260_I2C_ADDR    MAX17260_I2C_ADDR_ALT
#else
#define MAX17260_I2C_ADDR    MAX17260_I2C_ADDR_DEFAULT
#endif

#if MAX17260_THREAD_SAFE
#define MAX17260_LOCK()      max17260_io_lock()    /**< 进入临界区 */
#define MAX17260_UNLOCK()    max17260_io_unlock()  /**< 退出临界区 */
#else
#define MAX17260_LOCK()      ((void)0)             /**< 未启用时为空操作 */
#define MAX17260_UNLOCK()    ((void)0)             /**< 未启用时为空操作 */
#endif

/** 句柄与初始化状态检查，未通过直接返回 */
#define MAX17260_CHK_DEV(d)  do { \
    if ((d) == NULL) return MAX17260_ERR_PARAM; \
    if ((d)->inited == 0u) return MAX17260_ERR_NOT_READY; \
} while (0)

/* 换算常量：手册 Table 2 Standard Resolutions，分子用整数表达。 */
#define MAX17260_UV_PER_VCELL_LSB    78u       /**< VCell 78.125µV/LSB 分子（×10 后整） */
#define MAX17260_UV_PER_VCELL_DEN    10u       /**< VCell 78.125µV/LSB 分母（×10 后） */

/* ══════════════════════════════════════════════════════════════════════════
 * 私有函数 —— 定点换算辅助
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   无符号数四舍五入除法
 * @param   num  被除数。
 * @param   den  除数（正数）。
 * @retval  uint32_t  就近取整的商。
 */
static uint32_t max17260_round_div_u32(uint32_t num, uint32_t den)
{
    return (num + den / 2u) / den;
}

/**
 * @brief   有符号数四舍五入除法（对称舍入，远离零半档进位）
 * @param   num  被除数。
 * @param   den  除数（正数）。
 * @retval  int32_t  就近取整的商。
 */
static int32_t max17260_round_div_i32(int32_t num, int32_t den)
{
    if (num >= 0)
    {
        return (num + den / 2) / den;
    }
    return (num - den / 2) / den;
}

/**
 * @brief   数值钳位到闭区间
 * @param   val  输入值。
 * @param   lo   下限。
 * @param   hi   上限。
 * @retval  uint16_t  钳位后的值。
 */
static uint16_t max17260_clamp_u16(uint16_t val, uint16_t lo, uint16_t hi)
{
    if (val < lo)
    {
        return lo;
    }
    if (val > hi)
    {
        return hi;
    }
    return val;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 私有函数 —— 寄存器访问
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读一个 16 位寄存器（io 封装）
 * @param   dev  设备句柄。
 * @param   reg  寄存器基地址。
 * @param   val  输出：16 位值。
 * @retval  max17260_result_t  OK 成功；ERR_IO 通信失败。
 */
static max17260_result_t max17260_read_hw(max17260_dev_t *dev, uint8_t reg,
                                          uint16_t *val)
{
    if (max17260_io_read_reg16(dev->io_ctx, dev->dev_addr, reg, val)
            != MAX17260_IO_OK)
    {
        return MAX17260_ERR_IO;
    }
    return MAX17260_OK;
}

#if MAX17260_VERIFY_WRITES
/**
 * @brief   判断寄存器是否跳过写后校验
 * @details Status 写 1 清除语义；序列号 0xD4~0xDF 复用其它功能寄存器
 *          回读值非用户写入值；只读寄存器 / DieTemp 不需校验。
 * @param   reg  寄存器地址。
 * @retval  bool  true 表示跳过校验。
 */
static bool max17260_verify_skip(uint8_t reg)
{
    if (reg == MAX17260_REG_STATUS)
    {
        return true;
    }
    if (reg == MAX17260_REG_DIETEMP)
    {
        return true;
    }
    /* 序列号 0xD4~0xDF 跳过 */
    if ((reg >= 0xD4u) && (reg <= 0xDFu))
    {
        return true;
    }
    return false;
}
#endif /* MAX17260_VERIFY_WRITES */

/**
 * @brief   写一个 16 位寄存器（含可选写后回读校验）
 * @param   dev  设备句柄。
 * @param   reg  寄存器基地址。
 * @param   val  待写 16 位值。
 * @retval  max17260_result_t  OK 成功；ERR_IO 通信失败；
 *          ERR_VERIFY 回读不一致（VERIFY_WRITES=1 时）。
 */
static max17260_result_t max17260_write_hw(max17260_dev_t *dev, uint8_t reg,
                                           uint16_t val)
{
#if MAX17260_VERIFY_WRITES
    uint16_t rb;
#endif

    if (max17260_io_write_reg16(dev->io_ctx, dev->dev_addr, reg, val)
            != MAX17260_IO_OK)
    {
        return MAX17260_ERR_IO;
    }

#if MAX17260_VERIFY_WRITES
    if (max17260_verify_skip(reg))
    {
        return MAX17260_OK;
    }
    if (max17260_read_hw(dev, reg, &rb) != MAX17260_OK)
    {
        return MAX17260_ERR_IO;
    }
    if (rb != val)
    {
        return MAX17260_ERR_VERIFY;
    }
#endif

    return MAX17260_OK;
}

/**
 * @brief   读-改-写：向 reg 写入 (val & mask)
 * @param   dev   设备句柄。
 * @param   reg   寄存器基地址。
 * @param   mask  位掩码，0 的位保持原值。
 * @param   val   已位于目标位位置的新值。
 * @retval  max17260_result_t  见 max17260_read_hw/max17260_write_hw。
 */
static max17260_result_t max17260_update_hw(max17260_dev_t *dev, uint8_t reg,
                                            uint16_t mask, uint16_t val)
{
    max17260_result_t res;
    uint16_t tmp;

    res = max17260_read_hw(dev, reg, &tmp);
    if (res != MAX17260_OK)
    {
        return res;
    }
    tmp = (uint16_t)((tmp & (uint16_t)~mask) | (val & mask));
    return max17260_write_hw(dev, reg, tmp);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 私有函数 —— 单位换算
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   容量（µVh / mAh）单位转换
 * @details MAX17260_RSENSE_MOHM=0 时返回原始 µVh；否则换算 mAh。
 */
static max17260_result_t max17260_capacity_from_raw(uint16_t raw, uint32_t *out)
{
    if (MAX17260_RSENSE_MOHM == 0)
    {
        /* 原始 µVh = raw × 5.0 µVh/RSENSE（手册 Table 2） */
        *out = (uint32_t)raw * 5u;
    }
    else
    {
        /* mAh = raw × 5.0 / RSENSE_mΩ */
        *out = max17260_round_div_u32((uint32_t)raw * 5u,
                                     (uint32_t)MAX17260_RSENSE_MOHM);
    }
    return MAX17260_OK;
}

/**
 * @brief   容量（mAh / µVh）反向单位转换
 * @details MAX17260_RSENSE_MOHM=0 时入参为 µVh；否则入参为 mAh。
 */
static uint16_t max17260_capacity_to_raw(uint16_t val)
{
    if (MAX17260_RSENSE_MOHM == 0)
    {
        /* µVh → LSB：raw = val / 5 */
        return (uint16_t)max17260_round_div_u32(val, 5u);
    }
    /* mAh → LSB：raw = val × RSENSE_mΩ / 5 */
    return (uint16_t)max17260_round_div_u32(
               (uint32_t)val * (uint32_t)MAX17260_RSENSE_MOHM, 5u);
}

/**
 * @brief   电流（µV / mA）单位转换
 * @details MAX17260_RSENSE_MOHM=0 时返回原始 µV；否则换算 mA。
 */
static max17260_result_t max17260_current_from_raw(int16_t raw, int32_t *out)
{
    int32_t uv;

    /* µV = raw × 1.5625µV（手册 Table 2）= raw × 15625/10000 */
    uv = (int32_t)raw * 15625 / 10000;

    if (MAX17260_RSENSE_MOHM == 0)
    {
        *out = uv;
    }
    else
    {
        /* mA = µV × 1000 / RSENSE_mΩ */
        *out = uv * 1000 / (int32_t)MAX17260_RSENSE_MOHM;
    }
    return MAX17260_OK;
}

/**
 * @brief   电流（mA / µV）反向单位转换
 */
static int16_t max17260_current_to_raw(int32_t val)
{
    int32_t uv;

    if (MAX17260_RSENSE_MOHM == 0)
    {
        uv = val;
    }
    else
    {
        /* mA → µV = val × RSENSE_mΩ / 1000 */
        uv = val * (int32_t)MAX17260_RSENSE_MOHM / 1000;
    }
    /* µV → LSB：raw = uv × 10000 / 15625 */
    return (int16_t)((int32_t)uv * 10000 / 15625);
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 1. 初始化 / 标识
 * ════════════════════════════════════════════════════════════════════════ */

max17260_result_t max17260_init(max17260_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr)
{
    max17260_result_t res;
    uint16_t status;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    if (max17260_io_init() != MAX17260_IO_OK)
    {
        return MAX17260_ERR_IO;
    }

    dev->io_ctx = io_ctx;
    dev->dev_addr = dev_addr;
    dev->inited = 0u;

    /* 读 Status 验证器件在位。若 POR 置位则清除（使用自定义模型的应用
     * 需随后调用 max17260_configure_model() 写入三件套）。 */
    MAX17260_LOCK();
    res = max17260_read_hw(dev, MAX17260_REG_STATUS, &status);
    if (res == MAX17260_OK)
    {
        if ((status & MAX17260_STATUS_POR) != 0u)
        {
            res = max17260_write_hw(dev, MAX17260_REG_STATUS,
                                    MAX17260_STATUS_POR);
        }
    }
    MAX17260_UNLOCK();

    if (res == MAX17260_OK)
    {
        dev->inited = 1u;
    }
    return res;
}

max17260_result_t max17260_is_por(max17260_dev_t *dev, bool *por)
{
    max17260_result_t res;
    uint16_t status;

    if ((dev == NULL) || (por == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_STATUS, &status);
    if (res == MAX17260_OK)
    {
        *por = ((status & MAX17260_STATUS_POR) != 0u);
    }
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 2. 测量读取
 * ════════════════════════════════════════════════════════════════════════ */

max17260_result_t max17260_read_vcell_raw(max17260_dev_t *dev, uint16_t *raw)
{
    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, MAX17260_REG_VCELL, raw);
}

max17260_result_t max17260_read_vcell(max17260_dev_t *dev, uint32_t *mv)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (mv == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_VCELL, &raw);
    if (res == MAX17260_OK)
    {
        /* mV = raw × 78.125µV / 1000 = raw × 78.125 / 1000
         *  放大 10 倍避免小数：× 781 / 10000 */
        *mv = max17260_round_div_u32((uint32_t)raw * 781u, 10000u);
    }
    return res;
}

max17260_result_t max17260_read_soc_raw(max17260_dev_t *dev, uint16_t *raw)
{
    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, MAX17260_REG_REPSOC, raw);
}

max17260_result_t max17260_read_soc(max17260_dev_t *dev, uint8_t *percent)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (percent == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_REPSOC, &raw);
    if (res == MAX17260_OK)
    {
        *percent = (uint8_t)((raw + 128u) >> 8);
    }
    return res;
}

max17260_result_t max17260_read_soc_precise(max17260_dev_t *dev,
                                            uint16_t *percent_x100)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (percent_x100 == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_REPSOC, &raw);
    if (res == MAX17260_OK)
    {
        /* %×100 = raw × 100 / 256 = raw × 25 / 64 */
        *percent_x100 = (uint16_t)max17260_round_div_u32(
                            (uint32_t)raw * 25u, 64u);
    }
    return res;
}

max17260_result_t max17260_read_temp_raw(max17260_dev_t *dev, int16_t *raw)
{
    max17260_result_t res;
    uint16_t tmp;

    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_TEMP, &tmp);
    if (res == MAX17260_OK)
    {
        *raw = (int16_t)tmp;
    }
    return res;
}

max17260_result_t max17260_read_temp(max17260_dev_t *dev, int16_t *temp_x10)
{
    max17260_result_t res;
    int16_t raw;

    if ((dev == NULL) || (temp_x10 == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_temp_raw(dev, &raw);
    if (res == MAX17260_OK)
    {
        /* 0.1℃ = raw × 10 / 256 = raw × 5 / 128 */
        *temp_x10 = (int16_t)max17260_round_div_i32(
                        (int32_t)raw * 5, 128);
    }
    return res;
}

max17260_result_t max17260_read_current_raw(max17260_dev_t *dev, int16_t *raw)
{
    max17260_result_t res;
    uint16_t tmp;

    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_CURRENT, &tmp);
    if (res == MAX17260_OK)
    {
        *raw = (int16_t)tmp;
    }
    return res;
}

max17260_result_t max17260_read_current(max17260_dev_t *dev, int32_t *ma)
{
    max17260_result_t res;
    int16_t raw;

    if ((dev == NULL) || (ma == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_current_raw(dev, &raw);
    if (res == MAX17260_OK)
    {
        res = max17260_current_from_raw(raw, ma);
    }
    return res;
}

max17260_result_t max17260_read_repcap_raw(max17260_dev_t *dev, uint16_t *raw)
{
    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, MAX17260_REG_REPCAP, raw);
}

max17260_result_t max17260_read_repcap(max17260_dev_t *dev, uint32_t *mah)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (mah == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_repcap_raw(dev, &raw);
    if (res == MAX17260_OK)
    {
        res = max17260_capacity_from_raw(raw, mah);
    }
    return res;
}

max17260_result_t max17260_read_fullcaprep(max17260_dev_t *dev, uint32_t *mah)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (mah == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_FULLCAPREP, &raw);
    if (res == MAX17260_OK)
    {
        res = max17260_capacity_from_raw(raw, mah);
    }
    return res;
}

max17260_result_t max17260_read_tte(max17260_dev_t *dev, uint32_t *seconds)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (seconds == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_TTE, &raw);
    if (res == MAX17260_OK)
    {
        /* s = raw × 5.625 = raw × 5625 / 1000 */
        *seconds = max17260_round_div_u32((uint32_t)raw * 5625u, 1000u);
    }
    return res;
}

max17260_result_t max17260_read_ttf(max17260_dev_t *dev, uint32_t *seconds)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (seconds == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_TTF, &raw);
    if (res == MAX17260_OK)
    {
        *seconds = max17260_round_div_u32((uint32_t)raw * 5625u, 1000u);
    }
    return res;
}

max17260_result_t max17260_read_power(max17260_dev_t *dev, int32_t *mw)
{
    max17260_result_t res;
    int16_t raw;
    int32_t uv2;

    if ((dev == NULL) || (mw == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_POWER, (uint16_t *)&raw);
    if (res == MAX17260_OK)
    {
        /* µV² = raw × 8µV²/RSENSE（手册 Table 2） */
        uv2 = (int32_t)raw * 8;

        if (MAX17260_RSENSE_MOHM == 0)
        {
            *mw = uv2;
        }
        else
        {
            /* mW = µV² × 1000 / RSENSE_mΩ
             * 由于 µV² 在 RSENSE=10mΩ 时可达 8 × 32768 = 262144，
             * × 1000 后约 2.6e8，仍在 int32 范围。 */
            *mw = uv2 * 1000 / (int32_t)MAX17260_RSENSE_MOHM;
        }
    }
    return res;
}

max17260_result_t max17260_read_cycles(max17260_dev_t *dev, uint16_t *cycles)
{
    if ((dev == NULL) || (cycles == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, MAX17260_REG_CYCLES, cycles);
}

max17260_result_t max17260_read_age(max17260_dev_t *dev, uint8_t *age)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (age == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_AGE, &raw);
    if (res == MAX17260_OK)
    {
        *age = (uint8_t)(raw & 0xFFu);
    }
    return res;
}

max17260_result_t max17260_read_dietemp(max17260_dev_t *dev, int16_t *temp_x10)
{
    max17260_result_t res;
    int16_t raw;

    if ((dev == NULL) || (temp_x10 == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_temp_raw(dev, &raw);
    if (res != MAX17260_OK)
    {
        return res;
    }
    /* DieTemp 与 Temp 在 TSEL=0 时同为内部 die 温度；
     * 0.1℃ 换算与 read_temp 一致 */
    *temp_x10 = (int16_t)max17260_round_div_i32(
                    (int32_t)raw * 5, 128);
    return MAX17260_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 3. 平均与最大最小
 * ════════════════════════════════════════════════════════════════════════ */

max17260_result_t max17260_read_avg_vcell(max17260_dev_t *dev, uint32_t *mv)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (mv == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_AVGVCELL, &raw);
    if (res == MAX17260_OK)
    {
        *mv = max17260_round_div_u32((uint32_t)raw * 781u, 10000u);
    }
    return res;
}

max17260_result_t max17260_read_avg_current(max17260_dev_t *dev, int32_t *ma)
{
    max17260_result_t res;
    int16_t raw;

    if ((dev == NULL) || (ma == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_AVGCURRENT, (uint16_t *)&raw);
    if (res == MAX17260_OK)
    {
        res = max17260_current_from_raw(raw, ma);
    }
    return res;
}

max17260_result_t max17260_read_avg_temp(max17260_dev_t *dev, int16_t *temp_x10)
{
    max17260_result_t res;
    int16_t raw;

    if ((dev == NULL) || (temp_x10 == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_AVGTA, (uint16_t *)&raw);
    if (res == MAX17260_OK)
    {
        *temp_x10 = (int16_t)max17260_round_div_i32(
                        (int32_t)raw * 5, 128);
    }
    return res;
}

max17260_result_t max17260_read_avg_power(max17260_dev_t *dev, int32_t *mw)
{
    max17260_result_t res;
    int16_t raw;
    int32_t uv2;

    if ((dev == NULL) || (mw == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_AVGPOWER, (uint16_t *)&raw);
    if (res == MAX17260_OK)
    {
        uv2 = (int32_t)raw * 8;
        if (MAX17260_RSENSE_MOHM == 0)
        {
            *mw = uv2;
        }
        else
        {
            *mw = uv2 * 1000 / (int32_t)MAX17260_RSENSE_MOHM;
        }
    }
    return res;
}

max17260_result_t max17260_read_maxmin_volt(max17260_dev_t *dev,
                                            uint32_t *max_mv,
                                            uint32_t *min_mv)
{
    max17260_result_t res;
    uint16_t val;

    if ((dev == NULL) || (max_mv == NULL) || (min_mv == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_MAXMINVOLT, &val);
    if (res == MAX17260_OK)
    {
        /* 8 位字段 × 20mV */
        *max_mv = (uint32_t)(((val & MAX17260_MAXMINVOLT_MAX_MASK) >>
                              MAX17260_MAXMINVOLT_MAX_SHIFT) * 20u);
        *min_mv = (uint32_t)((val & MAX17260_MAXMINVOLT_MIN_MASK) * 20u);
    }
    return res;
}

max17260_result_t max17260_read_maxmin_curr(max17260_dev_t *dev,
                                            int32_t *max_ma,
                                            int32_t *min_ma)
{
    max17260_result_t res;
    uint16_t val;
    int8_t max_raw;
    int8_t min_raw;

    if ((dev == NULL) || (max_ma == NULL) || (min_ma == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_MAXMINCURR, &val);
    if (res == MAX17260_OK)
    {
        /* 8 位补码字段，0.4mV/RSENSE/LSB */
        max_raw = (int8_t)((val & MAX17260_MAXMINCURR_MAX_MASK) >>
                           MAX17260_MAXMINCURR_MAX_SHIFT);
        min_raw = (int8_t)(val & MAX17260_MAXMINCURR_MIN_MASK);

        {
            int32_t max_uv = (int32_t)max_raw * 400;          /* 0.4mV/LSB ×1000 = µV */
            int32_t min_uv = (int32_t)min_raw * 400;
            if (MAX17260_RSENSE_MOHM == 0)
            {
                *max_ma = max_uv;
                *min_ma = min_uv;
            }
            else
            {
                *max_ma = max_uv * 1000 / (int32_t)MAX17260_RSENSE_MOHM;
                *min_ma = min_uv * 1000 / (int32_t)MAX17260_RSENSE_MOHM;
            }
        }
    }
    return res;
}

max17260_result_t max17260_read_maxmin_temp(max17260_dev_t *dev,
                                            int16_t *max_x10,
                                            int16_t *min_x10)
{
    max17260_result_t res;
    uint16_t val;
    int8_t max_raw;
    int8_t min_raw;

    if ((dev == NULL) || (max_x10 == NULL) || (min_x10 == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_MAXMINTEMP, &val);
    if (res == MAX17260_OK)
    {
        /* 8 位补码字段，1℃/LSB */
        max_raw = (int8_t)((val & MAX17260_MAXMINTEMP_MAX_MASK) >>
                           MAX17260_MAXMINTEMP_MAX_SHIFT);
        min_raw = (int8_t)(val & MAX17260_MAXMINTEMP_MIN_MASK);
        *max_x10 = (int16_t)((int16_t)max_raw * 10);
        *min_x10 = (int16_t)((int16_t)min_raw * 10);
    }
    return res;
}

max17260_result_t max17260_reset_maxmin(max17260_dev_t *dev)
{
    max17260_result_t res;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    MAX17260_LOCK();
    res = max17260_write_hw(dev, MAX17260_REG_MAXMINVOLT,
                            MAX17260_MAXMINVOLT_POR);
    if (res == MAX17260_OK)
    {
        res = max17260_write_hw(dev, MAX17260_REG_MAXMINCURR,
                                MAX17260_MAXMINCURR_POR);
    }
    if (res == MAX17260_OK)
    {
        res = max17260_write_hw(dev, MAX17260_REG_MAXMINTEMP,
                                MAX17260_MAXMINTEMP_POR);
    }
    MAX17260_UNLOCK();
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 4. Model m5 EZ 配置
 * ════════════════════════════════════════════════════════════════════════ */

max17260_result_t max17260_configure_model(max17260_dev_t *dev,
                                           uint16_t design_mah,
                                           uint16_t vempty_mv,
                                           uint16_t vrecovery_mv,
                                           uint16_t ichg_term_ma)
{
    max17260_result_t res;
    uint16_t ve_code;
    uint16_t vr_code;
    uint16_t ve_val;
    uint16_t dcap;
    uint16_t iterm;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    if ((design_mah == 0u) || (design_mah > 32767u))
    {
        return MAX17260_ERR_PARAM;
    }
    if (vempty_mv > MAX17260_VEMPTY_VE_MASK >> MAX17260_VEMPTY_VE_SHIFT
            * MAX17260_VEMPTY_LSB_MV)
    {
        return MAX17260_ERR_PARAM;
    }
    if (vrecovery_mv > 5080u)
    {
        return MAX17260_ERR_PARAM;
    }
    if (vempty_mv > vrecovery_mv)
    {
        return MAX17260_ERR_PARAM;
    }

    /* 10mV / 40mV 档就近取整 */
    ve_code = (uint16_t)max17260_round_div_u32(vempty_mv,
                                               MAX17260_VEMPTY_LSB_MV);
    vr_code = (uint16_t)max17260_round_div_u32(vrecovery_mv,
                                               MAX17260_VRECOV_LSB_MV);
    if (ve_code > 0x1FFu)
    {
        ve_code = 0x1FFu;
    }
    if (vr_code > 0x7Fu)
    {
        vr_code = 0x7Fu;
    }
    ve_val = (uint16_t)((ve_code << MAX17260_VEMPTY_VE_SHIFT) | vr_code);

    dcap = max17260_capacity_to_raw(design_mah);
    iterm = (uint16_t)max17260_current_to_raw((int32_t)ichg_term_ma);
    if ((iterm >> 8) == 0u)
    {
        iterm <<= 8;   /* IChgTerm 是 16 位电流寄存器，LSB 8 位为 0 */
    }

    MAX17260_LOCK();
    res = max17260_write_hw(dev, MAX17260_REG_DESIGNCAP, dcap);
    if (res == MAX17260_OK)
    {
        res = max17260_write_hw(dev, MAX17260_REG_VEMPTY, ve_val);
    }
    if (res == MAX17260_OK)
    {
        res = max17260_write_hw(dev, MAX17260_REG_ICHGTERM, iterm);
    }
    MAX17260_UNLOCK();
    return res;
}

max17260_result_t max17260_get_design_cap(max17260_dev_t *dev, uint32_t *mah)
{
    max17260_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (mah == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_DESIGNCAP, &raw);
    if (res == MAX17260_OK)
    {
        res = max17260_capacity_from_raw(raw, mah);
    }
    return res;
}

max17260_result_t max17260_get_vempty(max17260_dev_t *dev, uint16_t *ve_mv,
                                      uint16_t *vr_mv)
{
    max17260_result_t res;
    uint16_t val;
    uint16_t ve_code;
    uint16_t vr_code;

    if ((dev == NULL) || (ve_mv == NULL) || (vr_mv == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_VEMPTY, &val);
    if (res == MAX17260_OK)
    {
        ve_code = (uint16_t)((val & MAX17260_VEMPTY_VE_MASK) >>
                             MAX17260_VEMPTY_VE_SHIFT);
        vr_code = (uint16_t)(val & MAX17260_VEMPTY_VR_MASK);
        *ve_mv = (uint16_t)(ve_code * MAX17260_VEMPTY_LSB_MV);
        *vr_mv = (uint16_t)(vr_code * MAX17260_VRECOV_LSB_MV);
    }
    return res;
}

max17260_result_t max17260_get_ichg_term(max17260_dev_t *dev, uint16_t *ma)
{
    max17260_result_t res;
    int16_t raw16;

    if ((dev == NULL) || (ma == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_ICHGTERM, (uint16_t *)&raw16);
    if (res == MAX17260_OK)
    {
        int32_t out;
        /* IChgTerm 的有效数据在高 8 位（手册 Initial Value 0x0640 隐含），LSB 为 0；
         * 直接除以 256 取高 8 位后按一般电流公式换算 */
        int16_t eff_raw = (int16_t)((int32_t)raw16 / 256);
        res = max17260_current_from_raw(eff_raw, &out);
        if (res == MAX17260_OK)
        {
            *ma = (uint16_t)out;
        }
    }
    return res;
}

max17260_result_t max17260_get_modelcfg(max17260_dev_t *dev, uint16_t *val)
{
    if ((dev == NULL) || (val == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, MAX17260_REG_MODELCFG, val);
}

max17260_result_t max17260_set_modelcfg(max17260_dev_t *dev, uint16_t val)
{
    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_write_hw(dev, MAX17260_REG_MODELCFG, val);
}

max17260_result_t max17260_get_config(max17260_dev_t *dev, uint16_t *val)
{
    if ((dev == NULL) || (val == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, MAX17260_REG_CONFIG, val);
}

max17260_result_t max17260_set_config(max17260_dev_t *dev, uint16_t val)
{
    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_write_hw(dev, MAX17260_REG_CONFIG, val);
}

max17260_result_t max17260_get_config2(max17260_dev_t *dev, uint16_t *val)
{
    if ((dev == NULL) || (val == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, MAX17260_REG_CONFIG2, val);
}

max17260_result_t max17260_set_config2(max17260_dev_t *dev, uint16_t val)
{
    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_write_hw(dev, MAX17260_REG_CONFIG2, val);
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 5. Alert 阈值与状态服务
 * ════════════════════════════════════════════════════════════════════════ */

max17260_result_t max17260_set_voltage_alerts(max17260_dev_t *dev,
                                              uint16_t min_mv,
                                              uint16_t max_mv)
{
    uint16_t code_min;
    uint16_t code_max;
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    if (min_mv > max_mv)
    {
        return MAX17260_ERR_PARAM;
    }
    /* 20mV 档就近取整，8 位档位最大 5100mV */
    code_min = (uint16_t)max17260_round_div_u32(
                   max17260_clamp_u16(min_mv, 0u, 5100u), 20u);
    code_max = (uint16_t)max17260_round_div_u32(
                   max17260_clamp_u16(max_mv, 0u, 5100u), 20u);
    val = (uint16_t)((code_min << MAX17260_VALRTTH_MIN_SHIFT) | code_max);
    return max17260_write_hw(dev, MAX17260_REG_VALRTTH, val);
}

max17260_result_t max17260_get_voltage_alerts(max17260_dev_t *dev,
                                              uint16_t *min_mv,
                                              uint16_t *max_mv)
{
    max17260_result_t res;
    uint16_t val;

    if ((dev == NULL) || (min_mv == NULL) || (max_mv == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_VALRTTH, &val);
    if (res == MAX17260_OK)
    {
        *max_mv = (uint16_t)(((val & MAX17260_VALRTTH_MAX_MASK) >>
                              MAX17260_VALRTTH_MAX_SHIFT) * 20u);
        *min_mv = (uint16_t)((val & MAX17260_VALRTTH_MIN_MASK) * 20u);
    }
    return res;
}

max17260_result_t max17260_set_temp_alerts(max17260_dev_t *dev,
                                           int16_t min_x10, int16_t max_x10)
{
    int16_t min_c;
    int16_t max_c;
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    if ((min_x10 > max_x10) || (min_x10 < -1280) || (max_x10 > 1270))
    {
        return MAX17260_ERR_PARAM;
    }
    /* 0.1℃ → 1℃ 档（4 舍 5 入） */
    min_c = (int16_t)max17260_round_div_i32((int32_t)min_x10, 10);
    max_c = (int16_t)max17260_round_div_i32((int32_t)max_x10, 10);
    if (min_c < -128) min_c = -128;
    if (min_c > 127) min_c = 127;
    if (max_c < -128) max_c = -128;
    if (max_c > 127) max_c = 127;
    val = (uint16_t)(((uint16_t)(uint8_t)max_c << MAX17260_TALRTTH_MAX_SHIFT) |
                     (uint16_t)(uint8_t)min_c);
    return max17260_write_hw(dev, MAX17260_REG_TALRTTH, val);
}

max17260_result_t max17260_get_temp_alerts(max17260_dev_t *dev,
                                           int16_t *min_x10,
                                           int16_t *max_x10)
{
    max17260_result_t res;
    uint16_t val;
    int8_t min_c;
    int8_t max_c;

    if ((dev == NULL) || (min_x10 == NULL) || (max_x10 == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_TALRTTH, &val);
    if (res == MAX17260_OK)
    {
        max_c = (int8_t)((val & MAX17260_TALRTTH_MAX_MASK) >>
                         MAX17260_TALRTTH_MAX_SHIFT);
        min_c = (int8_t)(val & MAX17260_TALRTTH_MIN_MASK);
        *max_x10 = (int16_t)((int16_t)max_c * 10);
        *min_x10 = (int16_t)((int16_t)min_c * 10);
    }
    return res;
}

max17260_result_t max17260_set_soc_alerts(max17260_dev_t *dev,
                                          uint8_t min_pct, uint8_t max_pct)
{
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    if (min_pct > max_pct)
    {
        return MAX17260_ERR_PARAM;
    }
    val = (uint16_t)(((uint16_t)max_pct << MAX17260_SALRTTH_MAX_SHIFT) |
                     (uint16_t)min_pct);
    return max17260_write_hw(dev, MAX17260_REG_SALRTTH, val);
}

max17260_result_t max17260_get_soc_alerts(max17260_dev_t *dev,
                                          uint8_t *min_pct,
                                          uint8_t *max_pct)
{
    max17260_result_t res;
    uint16_t val;

    if ((dev == NULL) || (min_pct == NULL) || (max_pct == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_SALRTTH, &val);
    if (res == MAX17260_OK)
    {
        *max_pct = (uint8_t)((val & MAX17260_SALRTTH_MAX_MASK) >>
                              MAX17260_SALRTTH_MAX_SHIFT);
        *min_pct = (uint8_t)(val & MAX17260_SALRTTH_MIN_MASK);
    }
    return res;
}

max17260_result_t max17260_set_current_alerts(max17260_dev_t *dev,
                                              int32_t min_ma, int32_t max_ma)
{
    int8_t imax;
    int8_t imin;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    if (min_ma > max_ma)
    {
        return MAX17260_ERR_PARAM;
    }
    {
        int16_t imax_raw = max17260_current_to_raw(max_ma);
        int16_t imin_raw = max17260_current_to_raw(min_ma);
        imax = (int8_t)(imax_raw >> 4);   /* 0.4mV/RSENSE/LSB，1.5625µV/LSB × 256 ≈ 0.4mV */
        imin = (int8_t)(imin_raw >> 4);
    }
    {
        uint16_t val = (uint16_t)(((uint16_t)(uint8_t)imax <<
                                   MAX17260_IALRTTH_MAX_SHIFT) |
                                  (uint16_t)(uint8_t)imin);
        return max17260_write_hw(dev, MAX17260_REG_IALRTTH, val);
    }
}

max17260_result_t max17260_get_current_alerts(max17260_dev_t *dev,
                                              int32_t *min_ma,
                                              int32_t *max_ma)
{
    max17260_result_t res;
    uint16_t val;
    int8_t imax;
    int8_t imin;
    int16_t imax_raw;
    int16_t imin_raw;
    int32_t out_max;
    int32_t out_min;

    if ((dev == NULL) || (min_ma == NULL) || (max_ma == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_IALRTTH, &val);
    if (res == MAX17260_OK)
    {
        imax = (int8_t)((val & MAX17260_IALRTTH_MAX_MASK) >>
                        MAX17260_IALRTTH_MAX_SHIFT);
        imin = (int8_t)(val & MAX17260_IALRTTH_MIN_MASK);
        imax_raw = (int16_t)((int16_t)imax << 4);
        imin_raw = (int16_t)((int16_t)imin << 4);
        (void)max17260_current_from_raw(imax_raw, &out_max);
        (void)max17260_current_from_raw(imin_raw, &out_min);
        *max_ma = out_max;
        *min_ma = out_min;
    }
    return res;
}

max17260_result_t max17260_get_status(max17260_dev_t *dev,
                                      max17260_status_t *status)
{
    max17260_result_t res;
    uint16_t val;

    if ((dev == NULL) || (status == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    res = max17260_read_hw(dev, MAX17260_REG_STATUS, &val);
    if (res == MAX17260_OK)
    {
        status->raw = val;
        status->por = ((val & MAX17260_STATUS_POR) != 0u);
        status->batt_present = ((val & MAX17260_STATUS_BST) == 0u);
        status->batt_removed = ((val & MAX17260_STATUS_BR) != 0u);
        status->batt_inserted = ((val & MAX17260_STATUS_BI) != 0u);
        status->soc_high = ((val & MAX17260_STATUS_SMX) != 0u);
        status->soc_low = ((val & MAX17260_STATUS_SMN) != 0u);
        status->vcell_high = ((val & MAX17260_STATUS_VMX) != 0u);
        status->vcell_low = ((val & MAX17260_STATUS_VMN) != 0u);
        status->temp_high = ((val & MAX17260_STATUS_TMX) != 0u);
        status->temp_low = ((val & MAX17260_STATUS_TMN) != 0u);
        status->curr_high = ((val & MAX17260_STATUS_IMX) != 0u);
        status->curr_low = ((val & MAX17260_STATUS_IMN) != 0u);
        status->dsoc_change = ((val & MAX17260_STATUS_DSOCI) != 0u);
    }
    return res;
}

max17260_result_t max17260_clear_alerts(max17260_dev_t *dev,
                                        uint16_t status_bits)
{
    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    /* Status 告警位写 1 清除，写 0 不影响；用 update_bits 保留未指定位 */
    return max17260_update_hw(dev, MAX17260_REG_STATUS,
                              (uint16_t)(status_bits &
                                         MAX17260_STATUS_CLEAR_MASK),
                              (uint16_t)(status_bits &
                                         MAX17260_STATUS_CLEAR_MASK));
}

#if MAX17260_USE_SERIAL_NUMBER
max17260_result_t max17260_read_serial(max17260_dev_t *dev,
                                       max17260_serial_t *sn)
{
    max17260_result_t res;
    uint16_t cfg2_old = 0u;
    bool    cfg2_saved = false;
    uint8_t i;
    const uint8_t sn_addrs[MAX17260_SN_WORDS] =
    {
        MAX17260_REG_SN_WORD0, MAX17260_REG_SN_WORD1,
        MAX17260_REG_SN_WORD2, MAX17260_REG_SN_WORD3,
        MAX17260_REG_SN_WORD4, MAX17260_REG_SN_WORD5,
        MAX17260_REG_SN_WORD6, MAX17260_REG_SN_WORD7,
    };

    if ((dev == NULL) || (sn == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    MAX17260_LOCK();
    /* 保存原 Config2 状态，强制清 AtRateEn / DPEn 切到序列号模式 */
    res = max17260_read_hw(dev, MAX17260_REG_CONFIG2, &cfg2_old);
    if (res == MAX17260_OK)
    {
        cfg2_saved = true;
        res = max17260_write_hw(dev, MAX17260_REG_CONFIG2,
                                (uint16_t)(cfg2_old &
                                           ~(MAX17260_CONFIG2_ATRATEEN |
                                             MAX17260_CONFIG2_DPEN)));
    }
    if (res == MAX17260_OK)
    {
        for (i = 0u; i < MAX17260_SN_WORDS; i++)
        {
            res = max17260_read_hw(dev, sn_addrs[i], &sn->word[i]);
            if (res != MAX17260_OK)
            {
                break;
            }
        }
    }
    /* 无论成败都恢复 Config2 */
    if (cfg2_saved)
    {
        (void)max17260_write_hw(dev, MAX17260_REG_CONFIG2, cfg2_old);
    }
    MAX17260_UNLOCK();
    return res;
}
#endif /* MAX17260_USE_SERIAL_NUMBER */

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 6. 寄存器级原始访问
 * ════════════════════════════════════════════════════════════════════════ */

max17260_result_t max17260_read_reg(max17260_dev_t *dev, uint8_t reg,
                                    uint16_t *val)
{
    if ((dev == NULL) || (val == NULL))
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);
    return max17260_read_hw(dev, reg, val);
}

max17260_result_t max17260_write_reg(max17260_dev_t *dev, uint8_t reg,
                                     uint16_t val)
{
    max17260_result_t res;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    MAX17260_LOCK();
    res = max17260_write_hw(dev, reg, val);
    MAX17260_UNLOCK();
    return res;
}

max17260_result_t max17260_update_bits(max17260_dev_t *dev, uint8_t reg,
                                       uint16_t mask, uint16_t val)
{
    max17260_result_t res;

    if (dev == NULL)
    {
        return MAX17260_ERR_PARAM;
    }
    MAX17260_CHK_DEV(dev);

    MAX17260_LOCK();
    res = max17260_update_hw(dev, reg, mask, val);
    MAX17260_UNLOCK();
    return res;
}
