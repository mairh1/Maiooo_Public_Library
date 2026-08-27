/*
 * @file    ina219.c
 * @brief   INA219 电流/功率监测芯片驱动核心
 * @details 纯 C99、无动态内存、无平台头文件，全部硬件访问经由
 *          ina219_io.h 声明的移植契约函数完成。
 *          实现依据本目录数据手册（INA219, ZHCSFN9G, 2015-12 修订）。
 * @note    所有换算为定点运算，单位约定见 ina219.h 文件头；64 位中间
 *          量仅用于电流/功率换算与校准值计算（非热路径）。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 */

#include "ina219.h"
#include "ina219_io.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 私有常量与宏
 * ══════════════════════════════════════════════════════════════════════════ */

#if INA219_THREAD_SAFE
#define INA219_LOCK()       ina219_io_lock()      /**< 进入临界区 */
#define INA219_UNLOCK()     ina219_io_unlock()    /**< 退出临界区 */
#else
#define INA219_LOCK()       ((void)0)             /**< 未启用时为空操作 */
#define INA219_UNLOCK()     ((void)0)             /**< 未启用时为空操作 */
#endif

/** 句柄与初始化状态检查，未通过直接返回 */
#define INA219_CHK_DEV(d)   do { \
    if ((d) == NULL) return INA219_ERR_PARAM; \
    if ((d)->inited == 0u) return INA219_ERR_NOT_READY; \
} while (0)

/** PGA 满量程档位表（Table 4），索引值即 PG 字段编码 */
static const uint16_t s_pga_range_mv[4] = { 40u, 80u, 160u, 320u };

/* ══════════════════════════════════════════════════════════════════════════
 * 私有函数 —— 定点换算辅助
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   有符号 64 位数四舍五入除法（对称舍入，远离零半档进位）
 * @param   num  被除数。
 * @param   den  除数（正数）。
 * @retval  int64_t  就近取整的商。
 */
static int64_t ina219_round_div_i64(int64_t num, int64_t den)
{
    if (num >= 0)
    {
        return (num + den / 2) / den;
    }
    return (num - den / 2) / den;
}

#if INA219_USE_POWER
/**
 * @brief   无符号 64 位数四舍五入除法
 * @param   num  被除数。
 * @param   den  除数（正数）。
 * @retval  uint64_t  就近取整的商。
 */
static uint64_t ina219_round_div_u64(uint64_t num, uint64_t den)
{
    return (num + den / 2u) / den;
}
#endif /* INA219_USE_POWER */

/**
 * @brief   PGA 满量程 mV 就近取整到支持档位
 * @details 在 ±40/±80/±160/±320mV 四档中取绝对差最小者；等距时取
 *          较小档位（宁欠不过量程）。
 * @param   mv  期望满量程 mV。
 * @retval  uint8_t  PG 字段编码（0~3）。
 */
static uint8_t ina219_pga_nearest(uint16_t mv)
{
    uint8_t best = 0u;
    uint8_t i;
    uint32_t best_dist;
    uint32_t dist;

    best_dist = (mv > 40u) ? (uint32_t)mv - 40u : 40u - (uint32_t)mv;
    for (i = 1u; i < 4u; i++)
    {
        dist = (mv > s_pga_range_mv[i])
                   ? (uint32_t)mv - s_pga_range_mv[i]
                   : (uint32_t)s_pga_range_mv[i] - mv;
        if (dist < best_dist)
        {
            best_dist = dist;
            best = i;
        }
    }
    return best;
}

/**
 * @brief   校验 ADC 档位编码是否合法
 * @details 合法集合（Table 5 规范编码）：0x0~0x3 = 9~12bit 分辨率档，
 *          0x9~0xF = 12bit×2~128 次平均档；0x4~0x8 为冗余/非法编码。
 * @param   adc  4 位档位值。
 * @retval  bool  true 表示合法。
 */
static bool ina219_adc_valid(uint8_t adc)
{
    return (adc <= 0x3u) || (adc >= 0x9u);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 私有函数 —— 寄存器访问
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读一个 16 位寄存器（io 封装）
 * @param   dev  设备句柄。
 * @param   reg  寄存器地址。
 * @param   val  输出：16 位值。
 * @retval  ina219_result_t  OK 成功；ERR_IO 通信失败。
 */
static ina219_result_t ina219_read_hw(ina219_dev_t *dev, uint8_t reg,
                                      uint16_t *val)
{
    if (ina219_io_read_reg16(dev->io_ctx, dev->dev_addr, reg, val)
            != INA219_IO_OK)
    {
        return INA219_ERR_IO;
    }
    return INA219_OK;
}

/**
 * @brief   写一个 16 位寄存器（含可选写后回读校验）
 * @details 仅对两个 R/W 寄存器（配置/校准）执行回读校验；测量寄存器
 *          （0x01~0x04）为只读，写入被器件忽略，不参与校验。
 * @param   dev  设备句柄。
 * @param   reg  寄存器地址。
 * @param   val  待写 16 位值。
 * @retval  ina219_result_t  OK 成功；ERR_IO 通信失败；
 *          ERR_VERIFY 回读不一致（VERIFY_WRITES=1 时）。
 */
static ina219_result_t ina219_write_hw(ina219_dev_t *dev, uint8_t reg,
                                       uint16_t val)
{
    if (ina219_io_write_reg16(dev->io_ctx, dev->dev_addr, reg, val)
            != INA219_IO_OK)
    {
        return INA219_ERR_IO;
    }

#if INA219_VERIFY_WRITES
    if ((reg == INA219_REG_CONFIG) || (reg == INA219_REG_CALIBRATION))
    {
        uint16_t rb;

        if (ina219_read_hw(dev, reg, &rb) != INA219_OK)
        {
            return INA219_ERR_IO;
        }
        if (rb != val)
        {
            return INA219_ERR_VERIFY;
        }
    }
#endif

    return INA219_OK;
}

/**
 * @brief   读-改-写：向 reg 写入 (val & mask)
 * @param   dev   设备句柄。
 * @param   reg   寄存器地址。
 * @param   mask  位掩码，0 的位保持原值。
 * @param   val   已位于目标位位置的新值。
 * @retval  ina219_result_t  见 ina219_read_hw/ina219_write_hw。
 */
static ina219_result_t ina219_update_hw(ina219_dev_t *dev, uint8_t reg,
                                        uint16_t mask, uint16_t val)
{
    ina219_result_t res;
    uint16_t tmp;

    res = ina219_read_hw(dev, reg, &tmp);
    if (res != INA219_OK)
    {
        return res;
    }
    tmp = (uint16_t)((tmp & (uint16_t)~mask) | (val & mask));
    return ina219_write_hw(dev, reg, tmp);
}

/**
 * @brief   按当前 conf 组合配置寄存器整字
 * @retval  uint16_t  配置寄存器目标值（BRNG/PG/BADC/SADC/MODE）。
 */
static uint16_t ina219_build_config(void)
{
    uint16_t cfg = 0u;

#if INA219_BUS_RANGE_32V
    cfg |= INA219_CFG_BRNG;
#endif
    cfg |= (uint16_t)ina219_pga_nearest(INA219_PGA_RANGE_MV)
           << INA219_CFG_PG_SHIFT;
    cfg |= (uint16_t)((uint16_t)INA219_BADC & 0xFu) << INA219_CFG_BADC_SHIFT;
    cfg |= (uint16_t)((uint16_t)INA219_SADC & 0xFu) << INA219_CFG_SADC_SHIFT;
    cfg |= (uint16_t)((uint16_t)INA219_MODE & 0x7u);
    return cfg;
}

/**
 * @brief   计算校准寄存器值
 * @details Cal = trunc(0.04096 / (Current_LSB × R_SHUNT))，量纲按
 *          nA × µΩ 放大 10^15 倍后整数运算（手册 Equation 1）。
 *          FS0 为 void 位（恒读 0），写入前清零：奇数值清位后等效、
 *          回读一致；清位后为 0 视为未校准，拒绝。
 * @param   shunt_uohms    采样电阻 µΩ。
 * @param   current_lsb_na Current_LSB nA。
 * @param   cal_out        输出：校准寄存器值（偶数）。
 * @retval  bool  true 表示计算成功且结果在 15 位范围内。
 */
static bool ina219_calc_cal(uint32_t shunt_uohms, uint32_t current_lsb_na,
                            uint16_t *cal_out)
{
    uint64_t denominator = (uint64_t)shunt_uohms * (uint64_t)current_lsb_na;
    uint64_t cal = INA219_CAL_FACTOR_NUM / denominator;

    if ((cal < 2u) || (cal > (uint64_t)INA219_CAL_MAX))
    {
        return false;
    }
    *cal_out = (uint16_t)(cal & ~1ULL);
    return true;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 1. 初始化 / 复位
 * ══════════════════════════════════════════════════════════════════════════ */

ina219_result_t ina219_init(ina219_dev_t *dev, void *io_ctx,
                            uint8_t dev_addr)
{
    ina219_result_t res;
    uint16_t cfg_probe;
    uint16_t cfg;
    uint16_t cal;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    if (!ina219_calc_cal(INA219_SHUNT_UOHMS, INA219_CURRENT_LSB_NA, &cal))
    {
        return INA219_ERR_PARAM;
    }
    if (ina219_io_init() != INA219_IO_OK)
    {
        return INA219_ERR_IO;
    }

    dev->io_ctx = io_ctx;
    dev->dev_addr = dev_addr;
    dev->inited = 0u;

    cfg = ina219_build_config();

    INA219_LOCK();
    /* 先探测器件应答：读不到配置寄存器视为不在位 */
    res = ina219_read_hw(dev, INA219_REG_CONFIG, &cfg_probe);
    if (res == INA219_OK)
    {
        res = ina219_write_hw(dev, INA219_REG_CONFIG, cfg);
    }
    if (res == INA219_OK)
    {
        res = ina219_write_hw(dev, INA219_REG_CALIBRATION, cal);
    }
    INA219_UNLOCK();

    if (res == INA219_OK)
    {
        dev->shunt_uohms = (uint32_t)INA219_SHUNT_UOHMS;
        dev->current_lsb_na = (uint32_t)INA219_CURRENT_LSB_NA;
        dev->cal_reg = cal;
        dev->inited = 1u;
    }
    return res;
}

ina219_result_t ina219_reset(ina219_dev_t *dev)
{
    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    /* 写 RST 位后器件立即全复位、寄存器回 POR 默认，无法（也无需）
     * 回读校验，故不走 ina219_write_hw()。 */
    if (ina219_io_write_reg16(dev->io_ctx, dev->dev_addr, INA219_REG_CONFIG,
                              INA219_CFG_RST) != INA219_IO_OK)
    {
        return INA219_ERR_IO;
    }

    /* 复位后配置与校准全部丢失，句柄回到未初始化态 */
    dev->inited = 0u;
    dev->shunt_uohms = 0u;
    dev->current_lsb_na = 0u;
    dev->cal_reg = 0u;
    return INA219_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 2. 量程 / ADC / 工作模式配置
 * ══════════════════════════════════════════════════════════════════════════ */

ina219_result_t ina219_set_bus_range(ina219_dev_t *dev, bool range32v)
{
    ina219_result_t res;
    uint16_t val = range32v ? INA219_CFG_BRNG : 0u;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    INA219_LOCK();
    res = ina219_update_hw(dev, INA219_REG_CONFIG, INA219_CFG_BRNG, val);
    INA219_UNLOCK();
    return res;
}

ina219_result_t ina219_get_bus_range(ina219_dev_t *dev, bool *range32v)
{
    ina219_result_t res;
    uint16_t cfg;

    if ((dev == NULL) || (range32v == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_CONFIG, &cfg);
    if (res == INA219_OK)
    {
        *range32v = ((cfg & INA219_CFG_BRNG) != 0u);
    }
    return res;
}

ina219_result_t ina219_set_pga_range(ina219_dev_t *dev, uint16_t mv)
{
    ina219_result_t res;
    uint8_t code = ina219_pga_nearest(mv);
    uint16_t val = (uint16_t)((uint16_t)code << INA219_CFG_PG_SHIFT);

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    INA219_LOCK();
    res = ina219_update_hw(dev, INA219_REG_CONFIG, INA219_CFG_PG_MASK, val);
    INA219_UNLOCK();
    return res;
}

ina219_result_t ina219_get_pga_range(ina219_dev_t *dev, uint16_t *mv)
{
    ina219_result_t res;
    uint16_t cfg;

    if ((dev == NULL) || (mv == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_CONFIG, &cfg);
    if (res == INA219_OK)
    {
        *mv = s_pga_range_mv[(cfg & INA219_CFG_PG_MASK) >>
                             INA219_CFG_PG_SHIFT];
    }
    return res;
}

ina219_result_t ina219_set_adc(ina219_dev_t *dev, uint8_t badc, uint8_t sadc)
{
    ina219_result_t res;
    uint16_t val;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);
    if (!ina219_adc_valid(badc) || !ina219_adc_valid(sadc))
    {
        return INA219_ERR_PARAM;
    }

    val = (uint16_t)(((uint16_t)badc << INA219_CFG_BADC_SHIFT) |
                     ((uint16_t)sadc << INA219_CFG_SADC_SHIFT));

    INA219_LOCK();
    res = ina219_update_hw(dev, INA219_REG_CONFIG,
                           INA219_CFG_BADC_MASK | INA219_CFG_SADC_MASK, val);
    INA219_UNLOCK();
    return res;
}

ina219_result_t ina219_get_adc(ina219_dev_t *dev, uint8_t *badc, uint8_t *sadc)
{
    ina219_result_t res;
    uint16_t cfg;

    if ((dev == NULL) || (badc == NULL) || (sadc == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_CONFIG, &cfg);
    if (res == INA219_OK)
    {
        *badc = (uint8_t)((cfg & INA219_CFG_BADC_MASK) >>
                          INA219_CFG_BADC_SHIFT);
        *sadc = (uint8_t)((cfg & INA219_CFG_SADC_MASK) >>
                          INA219_CFG_SADC_SHIFT);
    }
    return res;
}

ina219_result_t ina219_set_mode(ina219_dev_t *dev, ina219_mode_t mode)
{
    ina219_result_t res;
    uint16_t val;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);
    if ((uint8_t)mode > (uint8_t)INA219_MODE_CONT_SHUNT_BUS)
    {
        return INA219_ERR_PARAM;
    }

    val = (uint16_t)((uint16_t)mode << INA219_CFG_MODE_SHIFT);

    INA219_LOCK();
    res = ina219_update_hw(dev, INA219_REG_CONFIG, INA219_CFG_MODE_MASK, val);
    INA219_UNLOCK();
    return res;
}

ina219_result_t ina219_get_mode(ina219_dev_t *dev, ina219_mode_t *mode)
{
    ina219_result_t res;
    uint16_t cfg;

    if ((dev == NULL) || (mode == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_CONFIG, &cfg);
    if (res == INA219_OK)
    {
        *mode = (ina219_mode_t)((cfg & INA219_CFG_MODE_MASK) >>
                                INA219_CFG_MODE_SHIFT);
    }
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 3. 校准
 * ══════════════════════════════════════════════════════════════════════════ */

ina219_result_t ina219_set_calibration(ina219_dev_t *dev,
                                       uint32_t shunt_uohms,
                                       uint32_t current_lsb_na)
{
    ina219_result_t res;
    uint16_t cal;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);
    if ((shunt_uohms == 0u) || (current_lsb_na == 0u))
    {
        return INA219_ERR_PARAM;
    }
    if (!ina219_calc_cal(shunt_uohms, current_lsb_na, &cal))
    {
        return INA219_ERR_PARAM;
    }

    INA219_LOCK();
    res = ina219_write_hw(dev, INA219_REG_CALIBRATION, cal);
    INA219_UNLOCK();

    if (res == INA219_OK)
    {
        dev->shunt_uohms = shunt_uohms;
        dev->current_lsb_na = current_lsb_na;
        dev->cal_reg = cal;
    }
    return res;
}

ina219_result_t ina219_get_calibration(ina219_dev_t *dev,
                                       uint32_t *shunt_uohms,
                                       uint32_t *current_lsb_na,
                                       uint16_t *cal_reg)
{
    ina219_result_t res;
    uint16_t rb;

    if ((dev == NULL) || (shunt_uohms == NULL) || (current_lsb_na == NULL) ||
        (cal_reg == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_CALIBRATION, &rb);
    if (res == INA219_OK)
    {
        *shunt_uohms = dev->shunt_uohms;
        *current_lsb_na = dev->current_lsb_na;
        *cal_reg = rb;
    }
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 4. 测量读取
 * ══════════════════════════════════════════════════════════════════════════ */

ina219_result_t ina219_read_shunt_raw(ina219_dev_t *dev, int16_t *raw)
{
    ina219_result_t res;
    uint16_t tmp;

    if ((dev == NULL) || (raw == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_SHUNT, &tmp);
    if (res == INA219_OK)
    {
        *raw = (int16_t)tmp;
    }
    return res;
}

ina219_result_t ina219_read_shunt_voltage(ina219_dev_t *dev, int32_t *uv)
{
    ina219_result_t res;
    uint16_t tmp;

    if ((dev == NULL) || (uv == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_SHUNT, &tmp);
    if (res == INA219_OK)
    {
        /* LSB 固定 10µV；PGA 符号扩展设计使 16 位补码直接换算即可 */
        *uv = (int32_t)(int16_t)tmp * INA219_SHUNT_LSB_UV;
    }
    return res;
}

ina219_result_t ina219_read_bus_raw(ina219_dev_t *dev, uint16_t *raw)
{
    if ((dev == NULL) || (raw == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);
    return ina219_read_hw(dev, INA219_REG_BUS, raw);
}

ina219_result_t ina219_read_bus_voltage(ina219_dev_t *dev, uint16_t *mv)
{
    ina219_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (mv == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_BUS, &raw);
    if (res == INA219_OK)
    {
        /* 电压值左对齐 bit15:3，右移 3 位后乘 4mV LSB，标志位被移出 */
        *mv = (uint16_t)((raw >> INA219_BUS_SHIFT) * INA219_BUS_LSB_MV);
    }
    return res;
}

ina219_result_t ina219_read_current_raw(ina219_dev_t *dev, int16_t *raw)
{
    ina219_result_t res;
    uint16_t tmp;

    if ((dev == NULL) || (raw == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_CURRENT, &tmp);
    if (res == INA219_OK)
    {
        *raw = (int16_t)tmp;
    }
    return res;
}

ina219_result_t ina219_read_current(ina219_dev_t *dev, int32_t *ua)
{
    ina219_result_t res;
    uint16_t tmp;
    int64_t scaled;

    if ((dev == NULL) || (ua == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_CURRENT, &tmp);
    if (res == INA219_OK)
    {
        scaled = ina219_round_div_i64(
            (int64_t)(int16_t)tmp * (int64_t)dev->current_lsb_na, 1000);
        if (scaled > (int64_t)INT32_MAX)
        {
            scaled = (int64_t)INT32_MAX;
        }
        else if (scaled < (int64_t)INT32_MIN)
        {
            scaled = (int64_t)INT32_MIN;
        }
        *ua = (int32_t)scaled;
    }
    return res;
}

#if INA219_USE_POWER
ina219_result_t ina219_read_power_raw(ina219_dev_t *dev, uint16_t *raw)
{
    if ((dev == NULL) || (raw == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);
    return ina219_read_hw(dev, INA219_REG_POWER, raw);
}

ina219_result_t ina219_read_power(ina219_dev_t *dev, uint32_t *uw)
{
    ina219_result_t res;
    uint16_t raw;
    uint64_t scaled;

    if ((dev == NULL) || (uw == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_POWER, &raw);
    if (res == INA219_OK)
    {
        /* 功率 LSB = 20 × Current_LSB；µW = raw × lsb(nA) × 20 / 1000 */
        scaled = ina219_round_div_u64(
            (uint64_t)raw * (uint64_t)dev->current_lsb_na *
                (uint64_t)INA219_POWER_LSB_FACTOR,
            1000u);
        if (scaled > (uint64_t)UINT32_MAX)
        {
            scaled = (uint64_t)UINT32_MAX;
        }
        *uw = (uint32_t)scaled;
    }
    return res;
}
#endif /* INA219_USE_POWER */

ina219_result_t ina219_is_conversion_ready(ina219_dev_t *dev, bool *ready)
{
    ina219_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (ready == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_BUS, &raw);
    if (res == INA219_OK)
    {
        *ready = ((raw & INA219_BUS_CNVR) != 0u);
    }
    return res;
}

ina219_result_t ina219_is_math_overflow(ina219_dev_t *dev, bool *ovf)
{
    ina219_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (ovf == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    res = ina219_read_hw(dev, INA219_REG_BUS, &raw);
    if (res == INA219_OK)
    {
        *ovf = ((raw & INA219_BUS_OVF) != 0u);
    }
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 5. 触发转换与等待
 * ══════════════════════════════════════════════════════════════════════════ */

#if INA219_USE_TRIGGERED
ina219_result_t ina219_trigger(ina219_dev_t *dev)
{
    ina219_result_t res;
    uint16_t cfg;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    INA219_LOCK();
    /* 整字重写配置寄存器：写 MODE 字段清 CNVR 并在触发档启动新转换 */
    res = ina219_read_hw(dev, INA219_REG_CONFIG, &cfg);
    if (res == INA219_OK)
    {
        res = ina219_write_hw(dev, INA219_REG_CONFIG, cfg);
    }
    INA219_UNLOCK();
    return res;
}

ina219_result_t ina219_wait_conversion(ina219_dev_t *dev, uint32_t timeout_ms)
{
    ina219_result_t res = INA219_OK;
    uint16_t raw;
    uint32_t waited = 0u;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    for (;;)
    {
        res = ina219_read_hw(dev, INA219_REG_BUS, &raw);
        if (res != INA219_OK)
        {
            return res;
        }
        if ((raw & INA219_BUS_CNVR) != 0u)
        {
            return INA219_OK;
        }
        if (waited >= timeout_ms)
        {
            return INA219_ERR_TIMEOUT;
        }
        ina219_io_delay_ms(INA219_WAIT_POLL_MS);
        waited += INA219_WAIT_POLL_MS;
    }
}
#endif /* INA219_USE_TRIGGERED */

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 6. 寄存器级原始访问
 * ══════════════════════════════════════════════════════════════════════════ */

ina219_result_t ina219_read_reg(ina219_dev_t *dev, uint8_t reg,
                                uint16_t *val)
{
    if ((dev == NULL) || (val == NULL))
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);
    return ina219_read_hw(dev, reg, val);
}

ina219_result_t ina219_write_reg(ina219_dev_t *dev, uint8_t reg, uint16_t val)
{
    ina219_result_t res;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    INA219_LOCK();
    res = ina219_write_hw(dev, reg, val);
    INA219_UNLOCK();
    return res;
}

ina219_result_t ina219_update_bits(ina219_dev_t *dev, uint8_t reg,
                                   uint16_t mask, uint16_t val)
{
    ina219_result_t res;

    if (dev == NULL)
    {
        return INA219_ERR_PARAM;
    }
    INA219_CHK_DEV(dev);

    INA219_LOCK();
    res = ina219_update_hw(dev, reg, mask, val);
    INA219_UNLOCK();
    return res;
}
