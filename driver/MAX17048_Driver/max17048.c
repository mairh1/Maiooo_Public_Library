/*
 * @file    max17048.c
 * @brief   MAX17048/MAX17049 ModelGauge 电量计驱动核心
 * @details 纯 C99、无动态内存、无平台头文件，全部硬件访问经由
 *          max17048_io.h 声明的移植契约函数完成。
 *          实现依据本目录数据手册（MAX17048/MAX17049, 19-6171; Rev 7）。
 * @note    所有换算为定点运算，单位约定见 max17048.h 文件头。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 */

#include "max17048.h"
#include "max17048_regs.h"
#include "max17048_io.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 私有常量与宏
 * ════════════════════════════════════════════════════════════════════════ */

#if MAX17048_VARIANT == MAX17048_VARIANT_MAX17049
#define MAX17048_CELLS   2   /**< MAX17049 电池节数 */
#else
#define MAX17048_CELLS   1   /**< MAX17048 电池节数 */
#endif

#if MAX17048_THREAD_SAFE
#define MAX17048_LOCK()      max17048_io_lock()    /**< 进入临界区 */
#define MAX17048_UNLOCK()    max17048_io_unlock()  /**< 退出临界区 */
#else
#define MAX17048_LOCK()      ((void)0)             /**< 未启用时为空操作 */
#define MAX17048_UNLOCK()    ((void)0)             /**< 未启用时为空操作 */
#endif

/** 句柄与初始化状态检查，未通过直接返回 */
#define MAX17048_CHK_DEV(d)  do { \
    if ((d) == NULL) return MAX17048_ERR_PARAM; \
    if ((d)->inited == 0u) return MAX17048_ERR_NOT_READY; \
} while (0)

/* ══════════════════════════════════════════════════════════════════════════
 * 私有函数 —— 定点换算辅助
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   无符号数四舍五入除法
 * @param   num  被除数。
 * @param   den  除数（正数）。
 * @retval  uint32_t  就近取整的商。
 */
static uint32_t max17048_round_div_u32(uint32_t num, uint32_t den)
{
    return (num + den / 2u) / den;
}

/**
 * @brief   有符号数四舍五入除法（对称舍入，远离零半档进位）
 * @param   num  被除数。
 * @param   den  除数（正数）。
 * @retval  int32_t  就近取整的商。
 */
static int32_t max17048_round_div_i32(int32_t num, int32_t den)
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
static uint16_t max17048_clamp_u16(uint16_t val, uint16_t lo, uint16_t hi)
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
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败。
 */
static max17048_result_t max17048_read_hw(max17048_dev_t *dev, uint8_t reg,
                                          uint16_t *val)
{
    if (max17048_io_read_reg16(dev->io_ctx, dev->dev_addr, reg, val)
            != MAX17048_IO_OK)
    {
        return MAX17048_ERR_IO;
    }
    return MAX17048_OK;
}

#if MAX17048_VERIFY_WRITES
/**
 * @brief   判断寄存器是否跳过写后校验
 * @details MODE 为只写；STATUS 为写 1 清除语义；CMD 写后器件复位且无
 *          ACK；TABLE(0x40~0x7F) 为只写；0x3E 为解锁命令字——以上均无法
 *          通过回读校验。
 * @param   reg  寄存器地址。
 * @retval  bool  true 表示跳过校验。
 */
static bool max17048_verify_skip(uint8_t reg)
{
    if ((reg == MAX17048_REG_MODE) || (reg == MAX17048_REG_STATUS) ||
        (reg == MAX17048_REG_CMD) || (reg == MAX17048_REG_UNLOCK))
    {
        return true;
    }
    if ((reg >= MAX17048_REG_TABLE_FIRST) && (reg <= MAX17048_REG_TABLE_LAST))
    {
        return true;
    }
    return false;
}

/**
 * @brief   取寄存器参与回读比较的位掩码
 * @details 排除硬件可自行改变或只读的位：CONFIG.ALRT 会被硬件随时置 1；
 *          VRESET 低字节为 OTP ID。
 * @param   reg  寄存器地址。
 * @retval  uint16_t  参与比较的位集合。
 */
static uint16_t max17048_verify_mask(uint8_t reg)
{
    switch (reg)
    {
    case MAX17048_REG_CONFIG:
        return (uint16_t)(0xFFFFu & ~(uint16_t)MAX17048_CONFIG_ALRT);
    case MAX17048_REG_VRESET:
        return (uint16_t)MAX17048_VRESET_RW_MASK;
    default:
        return 0xFFFFu;
    }
}
#endif /* MAX17048_VERIFY_WRITES */

/**
 * @brief   写一个 16 位寄存器（含可选写后回读校验）
 * @param   dev  设备句柄。
 * @param   reg  寄存器基地址。
 * @param   val  待写 16 位值。
 * @retval  max17048_result_t  OK 成功；ERR_IO 通信失败；
 *          ERR_VERIFY 回读不一致（VERIFY_WRITES=1 时）。
 */
static max17048_result_t max17048_write_hw(max17048_dev_t *dev, uint8_t reg,
                                           uint16_t val)
{
#if MAX17048_VERIFY_WRITES
    uint16_t rb;
    uint16_t mask;
#endif

    if (max17048_io_write_reg16(dev->io_ctx, dev->dev_addr, reg, val)
            != MAX17048_IO_OK)
    {
        return MAX17048_ERR_IO;
    }

#if MAX17048_VERIFY_WRITES
    if (max17048_verify_skip(reg))
    {
        return MAX17048_OK;
    }
    if (max17048_read_hw(dev, reg, &rb) != MAX17048_OK)
    {
        return MAX17048_ERR_IO;
    }
    mask = max17048_verify_mask(reg);
    if ((uint16_t)(rb & mask) != (uint16_t)(val & mask))
    {
        return MAX17048_ERR_VERIFY;
    }
#endif

    return MAX17048_OK;
}

/**
 * @brief   读-改-写：向 reg 写入 (val & mask)
 * @param   dev   设备句柄。
 * @param   reg   寄存器基地址。
 * @param   mask  位掩码，0 的位保持原值。
 * @param   val   已位于目标位位置的新值。
 * @retval  max17048_result_t  见 max17048_read_hw/max17048_write_hw。
 */
static max17048_result_t max17048_update_hw(max17048_dev_t *dev, uint8_t reg,
                                            uint16_t mask, uint16_t val)
{
    max17048_result_t res;
    uint16_t tmp;

    res = max17048_read_hw(dev, reg, &tmp);
    if (res != MAX17048_OK)
    {
        return res;
    }
    tmp = (uint16_t)((tmp & (uint16_t)~mask) | (val & mask));
    return max17048_write_hw(dev, reg, tmp);
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 1. 初始化 / 复位 / 标识
 * ════════════════════════════════════════════════════════════════════════ */

max17048_result_t max17048_init(max17048_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr)
{
    max17048_result_t res;
    uint16_t ver;
    uint16_t status;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    if (max17048_io_init() != MAX17048_IO_OK)
    {
        return MAX17048_ERR_IO;
    }

    dev->io_ctx = io_ctx;
    dev->dev_addr = dev_addr;
    dev->inited = 0u;

    MAX17048_LOCK();
    res = max17048_read_hw(dev, MAX17048_REG_VERSION, &ver);
    if (res == MAX17048_OK)
    {
        if ((uint16_t)(ver & MAX17048_VERSION_MASK) != MAX17048_VERSION_EXPECTED)
        {
            res = MAX17048_ERR_NOT_READY;
        }
    }
    if (res == MAX17048_OK)
    {
        /* RI 在上电时置位：清除之。使用自定义模型的应用需在此后
         * 调用 max17048_model_load() 重载模型（ROM 模型无需动作）。 */
        res = max17048_read_hw(dev, MAX17048_REG_STATUS, &status);
        if ((res == MAX17048_OK) &&
            ((status & MAX17048_STATUS_RI) != 0u))
        {
            res = max17048_write_hw(dev, MAX17048_REG_STATUS,
                                    MAX17048_STATUS_RI);
        }
    }
    MAX17048_UNLOCK();

    if (res == MAX17048_OK)
    {
        dev->inited = 1u;
    }
    return res;
}

max17048_result_t max17048_get_id(max17048_dev_t *dev, max17048_id_t *id)
{
    max17048_result_t res;
    uint16_t ver;
    uint16_t vreset;

    if ((dev == NULL) || (id == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    res = max17048_read_hw(dev, MAX17048_REG_VERSION, &ver);
    if (res == MAX17048_OK)
    {
        res = max17048_read_hw(dev, MAX17048_REG_VRESET, &vreset);
    }
    MAX17048_UNLOCK();

    if (res == MAX17048_OK)
    {
        id->version = ver;
        id->id = (uint8_t)(vreset & MAX17048_VRESET_ID_MASK);
    }
    return res;
}

max17048_result_t max17048_full_reset(max17048_dev_t *dev)
{
    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    /* 手册：复位在最后一个时钟移入后发生，器件不回 ACK，
     * 因此此处忽略写返回值。 */
    (void)max17048_io_write_reg16(dev->io_ctx, dev->dev_addr,
                                  MAX17048_REG_CMD, MAX17048_CMD_POR_RESET);
    return MAX17048_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 2. 测量读取
 * ════════════════════════════════════════════════════════════════════════ */

max17048_result_t max17048_read_vcell_raw(max17048_dev_t *dev, uint16_t *raw)
{
    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);
    return max17048_read_hw(dev, MAX17048_REG_VCELL, raw);
}

max17048_result_t max17048_read_vcell(max17048_dev_t *dev, uint32_t *mv)
{
    max17048_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (mv == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_VCELL, &raw);
    if (res == MAX17048_OK)
    {
        /* 78.125µV = 5/64 mV，按节数放大后四舍五入 */
        *mv = max17048_round_div_u32((uint32_t)raw * 5u * MAX17048_CELLS, 64u);
    }
    return res;
}

max17048_result_t max17048_read_soc_raw(max17048_dev_t *dev, uint16_t *raw)
{
    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);
    return max17048_read_hw(dev, MAX17048_REG_SOC, raw);
}

max17048_result_t max17048_read_soc(max17048_dev_t *dev, uint8_t *percent)
{
    max17048_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (percent == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_SOC, &raw);
    if (res == MAX17048_OK)
    {
        *percent = (uint8_t)((raw + 128u) >> 8);
    }
    return res;
}

max17048_result_t max17048_read_soc_precise(max17048_dev_t *dev,
                                            uint16_t *percent_x100)
{
    max17048_result_t res;
    uint16_t raw;

    if ((dev == NULL) || (percent_x100 == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_SOC, &raw);
    if (res == MAX17048_OK)
    {
        /* %×100 = raw × 100/256 = raw × 25/64 */
        *percent_x100 =
            (uint16_t)max17048_round_div_u32((uint32_t)raw * 25u, 64u);
    }
    return res;
}

max17048_result_t max17048_read_crate_raw(max17048_dev_t *dev, int16_t *raw)
{
    max17048_result_t res;
    uint16_t tmp;

    if ((dev == NULL) || (raw == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_CRATE, &tmp);
    if (res == MAX17048_OK)
    {
        *raw = (int16_t)tmp;
    }
    return res;
}

max17048_result_t max17048_read_crate(max17048_dev_t *dev,
                                      int16_t *tenth_pct_per_hour)
{
    max17048_result_t res;
    uint16_t tmp;

    if ((dev == NULL) || (tenth_pct_per_hour == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_CRATE, &tmp);
    if (res == MAX17048_OK)
    {
        /* 0.1%/h = raw × 0.208 × 10 = raw × 208/100 = raw × 52/25 */
        *tenth_pct_per_hour =
            (int16_t)max17048_round_div_i32((int32_t)(int16_t)tmp * 52, 25);
    }
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 3. RCOMP 温度补偿
 * ════════════════════════════════════════════════════════════════════════ */

max17048_result_t max17048_get_rcomp(max17048_dev_t *dev, uint8_t *rcomp)
{
    max17048_result_t res;
    uint16_t cfg;

    if ((dev == NULL) || (rcomp == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_CONFIG, &cfg);
    if (res == MAX17048_OK)
    {
        *rcomp = (uint8_t)((cfg & MAX17048_CONFIG_RCOMP_MASK) >>
                           MAX17048_CONFIG_RCOMP_SHIFT);
    }
    return res;
}

max17048_result_t max17048_set_rcomp(max17048_dev_t *dev, uint8_t rcomp)
{
    max17048_result_t res;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    res = max17048_update_hw(dev, MAX17048_REG_CONFIG,
                             MAX17048_CONFIG_RCOMP_MASK,
                             (uint16_t)((uint16_t)rcomp <<
                                        MAX17048_CONFIG_RCOMP_SHIFT));
    MAX17048_UNLOCK();
    return res;
}

max17048_result_t max17048_temp_compensate(max17048_dev_t *dev,
                                           int16_t temp_x10)
{
    max17048_result_t res;
    int32_t delta_x10;
    int32_t coef_x10;
    int32_t rcomp_x10;
    int32_t rcomp;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    /* 手册公式：T > 20℃ 用 TempCoUp，否则用 TempCoDown。
     * RCOMP×10 = RCOMP0×10 + (T-20)×10 × 系数×10 / 10 */
    delta_x10 = (int32_t)temp_x10 - 200;
    if (temp_x10 > 200)
    {
        coef_x10 = MAX17048_TEMPCO_UP_X10;
    }
    else
    {
        coef_x10 = MAX17048_TEMPCO_DOWN_X10;
    }
    rcomp_x10 = (int32_t)MAX17048_RCOMP0 * 10 + delta_x10 * coef_x10 / 10;
    rcomp = max17048_round_div_i32(rcomp_x10, 10);
    if (rcomp < 0)
    {
        rcomp = 0;
    }
    if (rcomp > 255)
    {
        rcomp = 255;
    }

    MAX17048_LOCK();
    res = max17048_update_hw(dev, MAX17048_REG_CONFIG,
                             MAX17048_CONFIG_RCOMP_MASK,
                             (uint16_t)((uint16_t)rcomp <<
                                        MAX17048_CONFIG_RCOMP_SHIFT));
    MAX17048_UNLOCK();
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 4. 睡眠 / 休眠
 * ════════════════════════════════════════════════════════════════════════ */

max17048_result_t max17048_sleep_enter(max17048_dev_t *dev)
{
    max17048_result_t res;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    /* 先武装 EnSleep，再置 CONFIG.SLEEP 立即入睡 */
    res = max17048_write_hw(dev, MAX17048_REG_MODE, MAX17048_MODE_ENSLEEP);
    if (res == MAX17048_OK)
    {
        res = max17048_update_hw(dev, MAX17048_REG_CONFIG,
                                 MAX17048_CONFIG_SLEEP, MAX17048_CONFIG_SLEEP);
    }
    MAX17048_UNLOCK();
    return res;
}

max17048_result_t max17048_sleep_exit(max17048_dev_t *dev)
{
    max17048_result_t res;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    /* 手册：只有写 CONFIG.SLEEP=0 能唤醒，其它通信不会唤醒 */
    res = max17048_update_hw(dev, MAX17048_REG_CONFIG,
                             MAX17048_CONFIG_SLEEP, 0u);
    if (res == MAX17048_OK)
    {
        /* 清除 EnSleep 武装位，避免总线意外拉低再次入睡 */
        res = max17048_write_hw(dev, MAX17048_REG_MODE, 0u);
    }
    MAX17048_UNLOCK();
    return res;
}

max17048_result_t max17048_is_hibernating(max17048_dev_t *dev, bool *hib)
{
    max17048_result_t res;
    uint16_t mode;

    if ((dev == NULL) || (hib == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    /* 手册 Table 2 标注 MODE 只写，但 HibStat 位说明为只读指示，
     * 个别批次可能读不到有效值，调用方仅作参考。 */
    res = max17048_read_hw(dev, MAX17048_REG_MODE, &mode);
    if (res == MAX17048_OK)
    {
        *hib = ((mode & MAX17048_MODE_HIBSTAT) != 0u);
    }
    return res;
}

max17048_result_t max17048_hibernate_disable(max17048_dev_t *dev)
{
    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);
    return max17048_write_hw(dev, MAX17048_REG_HIBRT, 0x0000u);
}

max17048_result_t max17048_hibernate_force(max17048_dev_t *dev)
{
    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);
    return max17048_write_hw(dev, MAX17048_REG_HIBRT, 0xFFFFu);
}

max17048_result_t max17048_hibernate_set_thresholds(max17048_dev_t *dev,
                                                    uint8_t hib_thr,
                                                    uint8_t act_thr)
{
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    val = (uint16_t)((uint16_t)hib_thr << MAX17048_HIBRT_HIB_SHIFT) |
          (uint16_t)((uint16_t)act_thr & MAX17048_HIBRT_ACT_MASK);
    return max17048_write_hw(dev, MAX17048_REG_HIBRT, val);
}

max17048_result_t max17048_hibernate_get_thresholds(max17048_dev_t *dev,
                                                    uint8_t *hib_thr,
                                                    uint8_t *act_thr)
{
    max17048_result_t res;
    uint16_t val;

    if ((dev == NULL) || (hib_thr == NULL) || (act_thr == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_HIBRT, &val);
    if (res == MAX17048_OK)
    {
        *hib_thr = (uint8_t)((val & MAX17048_HIBRT_HIB_MASK) >>
                             MAX17048_HIBRT_HIB_SHIFT);
        *act_thr = (uint8_t)(val & MAX17048_HIBRT_ACT_MASK);
    }
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 5. 告警配置与服务
 * ════════════════════════════════════════════════════════════════════════ */

max17048_result_t max17048_set_soc_alert_threshold(max17048_dev_t *dev,
                                                   uint8_t percent)
{
    max17048_result_t res;
    uint8_t athd;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    percent = (uint8_t)max17048_clamp_u16(percent, 1u, 32u);
    athd = (uint8_t)(32u - percent);

    MAX17048_LOCK();
    res = max17048_update_hw(dev, MAX17048_REG_CONFIG,
                             MAX17048_CONFIG_ATHD_MASK,
                             (uint16_t)((uint16_t)athd <<
                                        MAX17048_CONFIG_ATHD_SHIFT));
    MAX17048_UNLOCK();
    return res;
}

max17048_result_t max17048_get_soc_alert_threshold(max17048_dev_t *dev,
                                                   uint8_t *percent)
{
    max17048_result_t res;
    uint16_t cfg;

    if ((dev == NULL) || (percent == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_CONFIG, &cfg);
    if (res == MAX17048_OK)
    {
        *percent = (uint8_t)(32u - ((cfg & MAX17048_CONFIG_ATHD_MASK) >>
                                    MAX17048_CONFIG_ATHD_SHIFT));
    }
    return res;
}

max17048_result_t max17048_set_soc_change_alert(max17048_dev_t *dev,
                                                bool enable)
{
    max17048_result_t res;
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    val = enable ? MAX17048_CONFIG_ALSC : 0u;

    MAX17048_LOCK();
    res = max17048_update_hw(dev, MAX17048_REG_CONFIG,
                             MAX17048_CONFIG_ALSC, val);
    MAX17048_UNLOCK();
    return res;
}

max17048_result_t max17048_set_voltage_alerts(max17048_dev_t *dev,
                                              uint16_t min_mv,
                                              uint16_t max_mv)
{
    uint16_t code_min;
    uint16_t code_max;
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    if (min_mv > max_mv)
    {
        return MAX17048_ERR_PARAM;
    }
    /* 20mV 档就近取整，范围 0~5100mV（8 位档位） */
    code_min = (uint16_t)max17048_round_div_u32(
                   max17048_clamp_u16(min_mv, 0u, 5100u), 20u);
    code_max = (uint16_t)max17048_round_div_u32(
                   max17048_clamp_u16(max_mv, 0u, 5100u), 20u);
    if (code_min > code_max)
    {
        return MAX17048_ERR_PARAM;
    }

    val = (uint16_t)((uint16_t)code_min << MAX17048_VALRT_MIN_SHIFT) |
          (uint16_t)code_max;
    return max17048_write_hw(dev, MAX17048_REG_VALRT, val);
}

max17048_result_t max17048_get_voltage_alerts(max17048_dev_t *dev,
                                              uint16_t *min_mv,
                                              uint16_t *max_mv)
{
    max17048_result_t res;
    uint16_t val;

    if ((dev == NULL) || (min_mv == NULL) || (max_mv == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_VALRT, &val);
    if (res == MAX17048_OK)
    {
        *min_mv = (uint16_t)((val & MAX17048_VALRT_MIN_MASK) >>
                             MAX17048_VALRT_MIN_SHIFT) * 20u;
        *max_mv = (uint16_t)(val & MAX17048_VALRT_MAX_MASK) * 20u;
    }
    return res;
}

max17048_result_t max17048_set_vreset_threshold(max17048_dev_t *dev,
                                                uint16_t mv,
                                                bool disable_comparator)
{
    max17048_result_t res;
    uint16_t code;
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    /* 特性表可编程范围 2.28~3.48V，40mV 档就近取整 */
    code = (uint16_t)max17048_round_div_u32(
               max17048_clamp_u16(mv, MAX17048_VRESET_MIN_MV,
                                  MAX17048_VRESET_MAX_MV), 40u);
    val = (uint16_t)(code << MAX17048_VRESET_SHIFT);
    if (disable_comparator)
    {
        val |= MAX17048_VRESET_DIS;
    }

    MAX17048_LOCK();
    /* 只写 bit15..8，保留低字节 OTP ID */
    res = max17048_update_hw(dev, MAX17048_REG_VRESET,
                             MAX17048_VRESET_RW_MASK, val);
    MAX17048_UNLOCK();
    return res;
}

max17048_result_t max17048_get_vreset_threshold(max17048_dev_t *dev,
                                                uint16_t *mv,
                                                bool *disable_comparator)
{
    max17048_result_t res;
    uint16_t val;

    if ((dev == NULL) || (mv == NULL) || (disable_comparator == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_VRESET, &val);
    if (res == MAX17048_OK)
    {
        *mv = (uint16_t)((val & MAX17048_VRESET_MASK) >>
                         MAX17048_VRESET_SHIFT) * 40u;
        *disable_comparator = ((val & MAX17048_VRESET_DIS) != 0u);
    }
    return res;
}

max17048_result_t max17048_set_vreset_alert_enable(max17048_dev_t *dev,
                                                   bool enable)
{
    uint16_t val;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    /* STATUS 告警位写 1 清除，直接整字写 EnVR 位、其余写 0，
     * 既更新使能位又不会误清已置位的告警位。 */
    val = enable ? MAX17048_STATUS_ENVR : 0u;
    return max17048_write_hw(dev, MAX17048_REG_STATUS, val);
}

max17048_result_t max17048_get_status(max17048_dev_t *dev,
                                      max17048_status_t *status)
{
    max17048_result_t res;
    uint16_t val;

    if ((dev == NULL) || (status == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    res = max17048_read_hw(dev, MAX17048_REG_STATUS, &val);
    if (res == MAX17048_OK)
    {
        status->raw = val;
        status->reset_indicator = ((val & MAX17048_STATUS_RI) != 0u);
        status->vhigh = ((val & MAX17048_STATUS_VH) != 0u);
        status->vlow = ((val & MAX17048_STATUS_VL) != 0u);
        status->vreset = ((val & MAX17048_STATUS_VR) != 0u);
        status->soc_low = ((val & MAX17048_STATUS_HD) != 0u);
        status->soc_change = ((val & MAX17048_STATUS_SC) != 0u);
    }
    return res;
}

max17048_result_t max17048_clear_alerts(max17048_dev_t *dev,
                                        uint16_t status_bits)
{
    max17048_result_t res;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    /* 写 1 清除指定位（写 0 不影响），使能位 EnVR 不在清除集合内 */
    res = max17048_write_hw(dev, MAX17048_REG_STATUS,
                            (uint16_t)(status_bits &
                                       MAX17048_STATUS_ALERT_MASK));
    if (res == MAX17048_OK)
    {
        /* 清 CONFIG.ALRT 释放 ALRT 引脚（不清则引脚保持低） */
        res = max17048_update_hw(dev, MAX17048_REG_CONFIG,
                                 MAX17048_CONFIG_ALRT, 0u);
    }
    MAX17048_UNLOCK();
    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 6. 快速启动 / 自定义模型表
 * ════════════════════════════════════════════════════════════════════════ */

max17048_result_t max17048_quick_start(max17048_dev_t *dev)
{
    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);
    /* MODE 整字写：QuickStart 位置 1，同时自然清掉 EnSleep 武装位 */
    return max17048_write_hw(dev, MAX17048_REG_MODE,
                             MAX17048_MODE_QUICKSTART);
}

#if MAX17048_USE_MODEL_TABLE
max17048_result_t max17048_model_load(max17048_dev_t *dev,
                                      const uint16_t *table)
{
    max17048_result_t res;
    uint8_t i;

    if ((dev == NULL) || (table == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    res = max17048_write_hw(dev, MAX17048_REG_UNLOCK, MAX17048_UNLOCK_VALUE);
    if (res == MAX17048_OK)
    {
        max17048_io_delay_ms(MAX17048_MODEL_DELAY_MS);
        for (i = 0u; i < MAX17048_TABLE_WORDS; i++)
        {
            res = max17048_write_hw(dev,
                                    (uint8_t)(MAX17048_REG_TABLE_FIRST + i),
                                    table[i]);
            if (res != MAX17048_OK)
            {
                break;
            }
        }
        /* 无论成败都尽快复锁：解锁期间 ModelGauge 引擎停止更新 */
        max17048_io_delay_ms(MAX17048_MODEL_DELAY_MS);
        (void)max17048_write_hw(dev, MAX17048_REG_UNLOCK, MAX17048_LOCK_VALUE);
    }
    MAX17048_UNLOCK();

    return res;
}
#endif /* MAX17048_USE_MODEL_TABLE */

/* ══════════════════════════════════════════════════════════════════════════
 * API —— 7. 寄存器级原始访问
 * ════════════════════════════════════════════════════════════════════════ */

max17048_result_t max17048_read_reg(max17048_dev_t *dev, uint8_t reg,
                                    uint16_t *val)
{
    if ((dev == NULL) || (val == NULL))
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);
    return max17048_read_hw(dev, reg, val);
}

max17048_result_t max17048_write_reg(max17048_dev_t *dev, uint8_t reg,
                                     uint16_t val)
{
    max17048_result_t res;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    res = max17048_write_hw(dev, reg, val);
    MAX17048_UNLOCK();
    return res;
}

max17048_result_t max17048_update_bits(max17048_dev_t *dev, uint8_t reg,
                                       uint16_t mask, uint16_t val)
{
    max17048_result_t res;

    if (dev == NULL)
    {
        return MAX17048_ERR_PARAM;
    }
    MAX17048_CHK_DEV(dev);

    MAX17048_LOCK();
    res = max17048_update_hw(dev, reg, mask, val);
    MAX17048_UNLOCK();
    return res;
}
