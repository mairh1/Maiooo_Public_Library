/**
 * @file    sgm41513.c
 * @brief   SGM41513 系列电池充电芯片的驱动核心
 * @details 可移植 C99 实现，无动态内存、无平台头文件。全部硬件访问都通过
 *          sgm41513_io.h 声明的 io 契约函数完成（4 必选；
 *          SGM41513_THREAD_SAFE=1 时追加 lock/unlock 2 个可选）。
 *          - 单位约定：电流 mA、电压 mV；set 就近取整到硬件档位并钳位，
 *            对应 get 返回实际生效值。
 *          - 可选影子缓存(shadow)（SGM41513_REG_SHADOW）：保存可读可写
 *            寄存器副本，供 sgm41513_restore_settings() 重放。
 *          - 可选写校验（SGM41513_VERIFY_WRITES）：写后回读比较。
 * @note    仅依赖自身头文件与 C99 固定宽度类型头；全部公共 API 须在
 *          线程上下文调用，禁止在 ISR 中调用。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-10
 */

#include "sgm41513.h"
#include "sgm41513_regs.h"
#include "sgm41513_io.h"

/* ═════════════════════════════════════════════════════════════════════════════
 * 并发保护辅助宏
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_THREAD_SAFE
#define SGM41513_LOCK()     sgm41513_io_lock()      /**< 进入临界区 */
#define SGM41513_UNLOCK()   sgm41513_io_unlock()    /**< 退出临界区 */
#else
#define SGM41513_LOCK()     ((void)0)               /**< 单线程下的空实现 */
#define SGM41513_UNLOCK()   ((void)0)               /**< 单线程下的空实现 */
#endif

/* 句柄有效性检查：指针为空返回 ERR_PARAM，未初始化返回 ERR_NOT_READY。 */
#define SGM41513_CHK_DEV(d)                                            \
    do                                                                 \
    {                                                                  \
        if ((d) == NULL)                                               \
        {                                                              \
            return SGM41513_ERR_PARAM;                                 \
        }                                                              \
        if ((d)->inited == 0u)                                         \
        {                                                              \
            return SGM41513_ERR_NOT_READY;                             \
        }                                                              \
    } while (0)

/* ═════════════════════════════════════════════════════════════════════════════
 * 换算表（数据手册 "Linear Table" 各节）
 * ═════════════════════════════════════════════════════════════════════════════ */

/* ICHG[5:0] 快充电流 mA（REG02），非线性，0..3000 mA                     */
static const uint16_t sgm41513_ichg_tab[64] = {
       0,    5,   10,   15,   20,   25,   30,   35,
      40,   50,   60,   70,   80,   90,  100,  110,
     130,  150,  170,  190,  210,  230,  250,  270,
     300,  330,  360,  390,  420,  450,  480,  510,
     540,  600,  660,  720,  780,  840,  900,  960,
    1020, 1080, 1140, 1200, 1260, 1320, 1380, 1440,
    1500, 1620, 1740, 1860, 1980, 2100, 2220, 2340,
    2460, 2580, 2700, 2820, 2940, 3000, 3000, 3000
};

/* IPRECHG[3:0] / ITERM[3:0] 电流 mA（REG03），5..240 mA                  */
static const uint16_t sgm41513_preterm_tab[16] = {
      5,  10,  15,  20,  30,  40,  50,  60,
     80, 100, 120, 140, 160, 180, 200, 240
};

/* SYS_MIN[2:0] 最小系统电压 mV（REG01），步进不均匀                       */
static const uint16_t sgm41513_sysmin_tab[8] = {
    2600, 2800, 3000, 3200, 3400, 3500, 3600, 3700
};

/* VINDPM_OS[1:0] 各档窗口的偏置电压 mV（REG0F）                          */
static const uint16_t sgm41513_vindpm_os_tab[4] = { 3900, 5900, 7500, 10500 };

/* 完全可读可写的寄存器集合；它们构成影子缓存(shadow)，由
 * sgm41513_restore_settings() 重放。REG08/09/0E 只读，REG0A/0B 只有个别
 * 位可读可写。                                                            */
static const uint8_t sgm41513_rw_regs[11] = {
    SGM41513_REG00, SGM41513_REG01, SGM41513_REG02, SGM41513_REG03,
    SGM41513_REG04, SGM41513_REG05, SGM41513_REG06, SGM41513_REG07,
    SGM41513_REG0C, SGM41513_REG0D, SGM41513_REG0F
};

/* 上电复位(POR)默认值表，与 sgm41513_rw_regs 一一对应，供
 * sgm41513_reset() 将影子缓存重置为默认值。                                */
static const uint8_t sgm41513_rw_por[11] = {
    SGM41513_REG00_POR, SGM41513_REG01_POR, SGM41513_REG02_POR,
    SGM41513_REG03_POR, SGM41513_REG04_POR, SGM41513_REG05_POR,
    SGM41513_REG06_POR, SGM41513_REG07_POR, SGM41513_REG0C_POR,
    SGM41513_REG0D_POR, SGM41513_REG0F_POR
};

/**
 * @brief   判断寄存器是否属于"完全可读可写"集合
 * @param   reg  寄存器地址。
 * @retval  bool true 表示该寄存器参与影子缓存(shadow)与写校验。
 */
static bool
sgm41513_is_rw_reg(uint8_t reg)
{
    uint8_t i;
    for (i = 0; i < (uint8_t)(sizeof(sgm41513_rw_regs)); i++)
    {
        if (sgm41513_rw_regs[i] == reg)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief   返回指定寄存器中会自清零或无需主机动作即自行变化的位掩码
 * @details 这些位被排除在可选的写校验（SGM41513_VERIFY_WRITES）之外，
 *          避免回读比较产生误报。
 * @param   reg  寄存器地址。
 * @retval  uint8_t 该寄存器的易变位掩码；不在下表中则返回 0。
 */
static uint8_t
sgm41513_volatile_bits(uint8_t reg)
{
    switch (reg)
    {
    case SGM41513_REG01: return SGM41513_WD_RST;
    case SGM41513_REG07: return SGM41513_IINDET_EN;
    case SGM41513_REG0D: return SGM41513_PUMPX_UP | SGM41513_PUMPX_DN;
    default:             return 0u;
    }
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 底层寄存器访问（调用方必须已持有锁）
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   读取一个寄存器
 * @param   dev  设备句柄，须已初始化。
 * @param   reg  寄存器地址（0x00..0x0F）。
 * @param   val  输出：读到的寄存器值。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败。
 */
static sgm41513_result_t
sgm41513_rd(sgm41513_dev_t *dev, uint8_t reg, uint8_t *val)
{
    if (sgm41513_io_read_reg(dev->io_ctx, dev->dev_addr, reg, val)
            != SGM41513_IO_OK)
    {
        return SGM41513_ERR_IO;
    }
    return SGM41513_OK;
}

/**
 * @brief   写一个寄存器，并按配置联动影子缓存与写校验
 * @details SGM41513_REG_SHADOW=1 时同步更新影子缓存（REG07 的 BATFET_DIS
 *          属一次性船运模式命令，不记入缓存）；SGM41513_VERIFY_WRITES=1 时
 *          写后回读比较（排除自清零位），不一致返回 ERR_VERIFY。
 * @param   dev  设备句柄，须已初始化。
 * @param   reg  寄存器地址。
 * @param   val  待写入的寄存器值。
 * @retval  sgm41513_result_t SGM41513_OK 成功；SGM41513_ERR_IO 通信失败；
 *          SGM41513_ERR_VERIFY 回读校验不一致。
 */
static sgm41513_result_t
sgm41513_wr(sgm41513_dev_t *dev, uint8_t reg, uint8_t val)
{
    if (sgm41513_io_write_reg(dev->io_ctx, dev->dev_addr, reg, val)
            != SGM41513_IO_OK)
    {
        return SGM41513_ERR_IO;
    }

#if SGM41513_REG_SHADOW
    if (sgm41513_is_rw_reg(reg))
    {
        dev->shadow[reg] = val;
        if (reg == SGM41513_REG07)
        {
            /* BATFET_DIS 是一次性的船运模式命令，不属于配置项。          */
            dev->shadow[reg] = (uint8_t)(dev->shadow[reg]
                                         & (uint8_t)~SGM41513_BATFET_DIS);
        }
    }
#endif

#if SGM41513_VERIFY_WRITES
    if (sgm41513_is_rw_reg(reg))
    {
        uint8_t rb;
        uint8_t mask = (uint8_t)(0xFFu & (uint8_t)~sgm41513_volatile_bits(reg));
        if (sgm41513_rd(dev, reg, &rb) != SGM41513_OK)
        {
            return SGM41513_ERR_IO;
        }
        if ((uint8_t)(rb & mask) != (uint8_t)(val & mask))
        {
            return SGM41513_ERR_VERIFY;
        }
    }
#else
    (void)sgm41513_volatile_bits; /* 保持该辅助函数被引用 */
#endif

    return SGM41513_OK;
}

/**
 * @brief   读-改-写指定寄存器的部分位
 * @param   dev   设备句柄，须已初始化。
 * @param   reg   寄存器地址。
 * @param   mask  位掩码：仅 mask 中的位被更新。
 * @param   val   新位值：按 (old & ~mask) | (val & mask) 合入。
 * @retval  sgm41513_result_t SGM41513_OK 成功；否则透传读/写失败的结果码。
 */
static sgm41513_result_t
sgm41513_rmw(sgm41513_dev_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    sgm41513_result_t res;
    uint8_t old;

    res = sgm41513_rd(dev, reg, &old);
    if (res != SGM41513_OK)
    {
        return res;
    }
    return sgm41513_wr(dev, reg, (uint8_t)((old & (uint8_t)~mask) | (val & mask)));
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 物理量换算辅助函数
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   在档位表中取与目标值最接近的档位下标
 * @details 距离并列时取先出现的档位；调用方须保证 val 已在合理量级
 *          （各 set 函数依赖档位表自身的边界值完成钳位）。
 * @param   tab  档位表（升序物理量数组）。
 * @param   n    档位表项数。
 * @param   val  目标物理量。
 * @retval  uint8_t 最接近 val 的档位下标（0..n-1）。
 */
static uint8_t
sgm41513_lut_nearest(const uint16_t *tab, uint8_t n, uint32_t val)
{
    uint8_t i, best = 0;
    uint32_t bestd = 0xFFFFFFFFu;

    for (i = 0; i < n; i++)
    {
        uint32_t t = tab[i];
        uint32_t d = (t > val) ? (t - val) : (val - t);
        if (d < bestd)
        {
            bestd = d;
            best = i;
        }
    }
    return best;
}

/**
 * @brief   VREG 码点 -> 调节电压(mV)
 * @details VREG[4:0]（REG04）：码点 <= 24 时为 3856 + 32*码点 mV，但码点 15
 *          为 4350 mV 特例；码点高于 24 钳位到码点 24。
 * @param   code  VREG 码点（0..SGM41513_VREG_CODE_MAX）。
 * @retval  uint32_t 对应的调节电压，单位 mV。
 */
static uint32_t
sgm41513_vreg_from_code(uint8_t code)
{
    if (code > SGM41513_VREG_CODE_MAX)
    {
        code = SGM41513_VREG_CODE_MAX;
    }
    if (code == 15u)
    {
        return 4350u;
    }
    return 3856u + (SGM41513_VREG_STEP_MV * (uint32_t)code);
}

/**
 * @brief   调节电压(mV) -> 最接近的 VREG 码点
 * @param   mv  目标电压，单位 mV。
 * @retval  uint8_t 最接近的 VREG 码点（0..SGM41513_VREG_CODE_MAX）。
 */
static uint8_t
sgm41513_vreg_to_code(uint32_t mv)
{
    uint8_t code, best = 0;
    uint32_t bestd = 0xFFFFFFFFu;

    for (code = 0; code <= SGM41513_VREG_CODE_MAX; code++)
    {
        uint32_t v = sgm41513_vreg_from_code(code);
        uint32_t d = (v > mv) ? (v - mv) : (mv - v);
        if (d < bestd)
        {
            bestd = d;
            best = code;
        }
    }
    return best;
}

/**
 * @brief   输入限流(mA) -> IINDPM 码点（四舍五入到最近档）
 * @details IINDPM[4:0]（REG00）：100 + 100*码点 mA；低于下限钳位到码点 0，
 *          高于上限钳位到码点 31。
 * @param   ma  目标输入限流，单位 mA。
 * @retval  uint8_t IINDPM 码点（0..31）。
 */
static uint8_t
sgm41513_iindpm_to_code(uint32_t ma)
{
    uint32_t code;

    if (ma <= SGM41513_IINDPM_MIN_MA)
    {
        return 0u;
    }
    code = (ma - SGM41513_IINDPM_MIN_MA + (SGM41513_IINDPM_STEP_MA / 2u))
           / SGM41513_IINDPM_STEP_MA;
    if (code > 31u)
    {
        code = 31u;
    }
    return (uint8_t)code;
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 1. 初始化 / 器件识别
 * ═════════════════════════════════════════════════════════════════════════════ */

sgm41513_result_t
sgm41513_init(sgm41513_dev_t *dev, void *io_ctx, uint8_t dev_addr)
{
    sgm41513_result_t res;
    uint8_t v, pn;

    if (dev == NULL)
    {
        return SGM41513_ERR_PARAM;
    }

    dev->io_ctx = io_ctx;
    dev->dev_addr = (dev_addr != 0u) ? dev_addr : SGM41513_I2C_ADDR;
    dev->inited = 0u;
    dev->variant = (uint8_t)SGM41513_VARIANT;

    if (sgm41513_io_init() != SGM41513_IO_OK)
    {
        return SGM41513_ERR_IO;
    }

    SGM41513_LOCK();

    /* 校验器件：SGMPART 必须读出 0，PN 必须为 0 或 1。                   */
    res = sgm41513_rd(dev, SGM41513_REG0B, &v);
    if (res != SGM41513_OK)
    {
        SGM41513_UNLOCK();
        return res;
    }
    if ((v & SGM41513_SGMPART_MASK) != 0u)
    {
        SGM41513_UNLOCK();
        return SGM41513_ERR_NOT_READY;
    }
    pn = (uint8_t)((v & SGM41513_PN_MASK) >> SGM41513_PN_SHIFT);
    if (pn > 1u)
    {
        SGM41513_UNLOCK();
        return SGM41513_ERR_NOT_READY;
    }
    /* PN = 0 -> SGM41513；PN = 1 -> SGM41513A 或 D（由配置确定）。     */
    if (pn == 0u)
    {
        dev->variant = (uint8_t)SGM41513_CHIP_SGM41513;
    }

#if SGM41513_REG_SHADOW
    {
        /* 保存当前可读可写寄存器镜像，使 restore_settings()
         * 重放的是实际生效的配置。                                        */
        uint8_t i;
        for (i = 0; i < (uint8_t)(sizeof(sgm41513_rw_regs)); i++)
        {
            res = sgm41513_rd(dev, sgm41513_rw_regs[i],
                              &dev->shadow[sgm41513_rw_regs[i]]);
            if (res != SGM41513_OK)
            {
                SGM41513_UNLOCK();
                return res;
            }
        }
    }
#else
    (void)sgm41513_rw_regs; (void)sgm41513_rw_por; (void)sgm41513_is_rw_reg;
#endif

    dev->inited = 1u;
    SGM41513_UNLOCK();
    return SGM41513_OK;
}

sgm41513_result_t
sgm41513_get_id(sgm41513_dev_t *dev, sgm41513_id_t *id)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (id == NULL)
    {
        return SGM41513_ERR_PARAM;
    }

    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG0B, &v);
    SGM41513_UNLOCK();

    if (res != SGM41513_OK)
    {
        return res;
    }
    id->pn_raw = (uint8_t)((v & SGM41513_PN_MASK) >> SGM41513_PN_SHIFT);
    id->variant = (sgm41513_variant_t)dev->variant;
    id->dev_rev = (uint8_t)(v & SGM41513_DEV_REV_MASK);
    return SGM41513_OK;
}

sgm41513_result_t
sgm41513_reset(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);

    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG0B, &v);
    if (res == SGM41513_OK)
    {
        /* REG_RST 自清零；REG0B 不在影子缓存(shadow)范围内。            */
        res = sgm41513_wr(dev, SGM41513_REG0B, (uint8_t)(v | SGM41513_REG_RST));
    }
#if SGM41513_REG_SHADOW
    if (res == SGM41513_OK)
    {
        uint8_t i;
        for (i = 0; i < (uint8_t)(sizeof(sgm41513_rw_regs)); i++)
        {
            dev->shadow[sgm41513_rw_regs[i]] = sgm41513_rw_por[i];
        }
    }
#endif
    SGM41513_UNLOCK();
    return res;
}

#if SGM41513_REG_SHADOW
sgm41513_result_t
sgm41513_restore_settings(sgm41513_dev_t *dev)
{
    sgm41513_result_t res = SGM41513_OK;
    uint8_t i;

    SGM41513_CHK_DEV(dev);

    SGM41513_LOCK();
    for (i = 0; i < (uint8_t)(sizeof(sgm41513_rw_regs)); i++)
    {
        uint8_t reg = sgm41513_rw_regs[i];
        res = sgm41513_wr(dev, reg, dev->shadow[reg]);
        if (res != SGM41513_OK)
        {
            break;
        }
    }
    SGM41513_UNLOCK();
    return res;
}
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * 2. 充电控制
 * ═════════════════════════════════════════════════════════════════════════════ */

sgm41513_result_t
sgm41513_charge_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG01, SGM41513_CHG_CONFIG,
                       enable ? SGM41513_CHG_CONFIG : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_ichg(sgm41513_dev_t *dev, uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    SGM41513_CHK_DEV(dev);
    code = sgm41513_lut_nearest(sgm41513_ichg_tab, 64, ma);

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG02, SGM41513_ICHG_MASK,
                       (uint8_t)(code << SGM41513_ICHG_SHIFT));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_get_ichg(sgm41513_dev_t *dev, uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (ma == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG02, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *ma = sgm41513_ichg_tab[(v & SGM41513_ICHG_MASK) >> SGM41513_ICHG_SHIFT];
    }
    return res;
}

sgm41513_result_t
sgm41513_set_vreg(sgm41513_dev_t *dev, uint32_t mv)
{
    sgm41513_result_t res;
    uint8_t code;

    SGM41513_CHK_DEV(dev);
    code = sgm41513_vreg_to_code(mv);

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG04, SGM41513_VREG_MASK,
                       (uint8_t)(code << SGM41513_VREG_SHIFT));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_get_vreg(sgm41513_dev_t *dev, uint32_t *mv)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (mv == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG04, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *mv = sgm41513_vreg_from_code(
            (v & SGM41513_VREG_MASK) >> SGM41513_VREG_SHIFT);
    }
    return res;
}

sgm41513_result_t
sgm41513_set_vreg_ft(sgm41513_dev_t *dev, sgm41513_vreg_ft_t ft)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0F, SGM41513_VREG_FT_MASK,
                       (uint8_t)(((uint8_t)ft << SGM41513_VREG_FT_SHIFT)
                                 & SGM41513_VREG_FT_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_precharge_current(sgm41513_dev_t *dev, uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    SGM41513_CHK_DEV(dev);
    code = sgm41513_lut_nearest(sgm41513_preterm_tab, 16, ma);

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG03, SGM41513_IPRECHG_MASK,
                       (uint8_t)(code << SGM41513_IPRECHG_SHIFT));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_get_precharge_current(sgm41513_dev_t *dev, uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (ma == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG03, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *ma = sgm41513_preterm_tab[(v & SGM41513_IPRECHG_MASK)
                                   >> SGM41513_IPRECHG_SHIFT];
    }
    return res;
}

sgm41513_result_t
sgm41513_set_term_current(sgm41513_dev_t *dev, uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    SGM41513_CHK_DEV(dev);
    code = sgm41513_lut_nearest(sgm41513_preterm_tab, 16, ma);

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG03, SGM41513_ITERM_MASK,
                       (uint8_t)(code << SGM41513_ITERM_SHIFT));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_get_term_current(sgm41513_dev_t *dev, uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (ma == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG03, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *ma = sgm41513_preterm_tab[v & SGM41513_ITERM_MASK];
    }
    return res;
}

sgm41513_result_t
sgm41513_set_term_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG05, SGM41513_EN_TERM,
                       enable ? SGM41513_EN_TERM : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_term_deglitch(sgm41513_dev_t *dev, sgm41513_term_deglitch_t t)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG05, SGM41513_ITERM_TIMER,
                       (t == SGM41513_ITERM_DEGLITCH_16MS)
                           ? SGM41513_ITERM_TIMER : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_recharge_threshold(sgm41513_dev_t *dev, sgm41513_vrechg_t mv)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG04, SGM41513_VRECHG,
                       (mv == SGM41513_VRECHG_200MV) ? SGM41513_VRECHG : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_topoff_timer(sgm41513_dev_t *dev, sgm41513_topoff_t t)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG04, SGM41513_TOPOFF_TIMER_MASK,
                       (uint8_t)(((uint8_t)t << SGM41513_TOPOFF_TIMER_SHIFT)
                                 & SGM41513_TOPOFF_TIMER_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_trickle_current(sgm41513_dev_t *dev, sgm41513_trickle_t ma)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0F, SGM41513_ISHORT_SET,
                       (ma == SGM41513_TRICKLE_30MA) ? SGM41513_ISHORT_SET : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_sys_min_voltage(sgm41513_dev_t *dev, uint32_t mv)
{
    sgm41513_result_t res;
    uint8_t code;

    SGM41513_CHK_DEV(dev);
    code = sgm41513_lut_nearest(sgm41513_sysmin_tab, 8, mv);

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG01, SGM41513_SYS_MIN_MASK,
                       (uint8_t)(code << SGM41513_SYS_MIN_SHIFT));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_get_sys_min_voltage(sgm41513_dev_t *dev, uint32_t *mv)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (mv == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG01, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *mv = sgm41513_sysmin_tab[(v & SGM41513_SYS_MIN_MASK)
                                  >> SGM41513_SYS_MIN_SHIFT];
    }
    return res;
}

sgm41513_result_t
sgm41513_set_min_vbat_otg(sgm41513_dev_t *dev, sgm41513_min_vbat_otg_t sel)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG01, SGM41513_MIN_BAT_SEL,
                       (sel == SGM41513_MIN_VBAT_OTG_2600MV)
                           ? SGM41513_MIN_BAT_SEL : 0u);
    SGM41513_UNLOCK();
    return res;
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 3. 输入管理
 * ═════════════════════════════════════════════════════════════════════════════ */

sgm41513_result_t
sgm41513_set_iindpm(sgm41513_dev_t *dev, uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    SGM41513_CHK_DEV(dev);
    code = sgm41513_iindpm_to_code(ma);

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG00, SGM41513_IINDPM_MASK, code);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_get_iindpm(sgm41513_dev_t *dev, uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (ma == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG00, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *ma = SGM41513_IINDPM_MIN_MA
              + SGM41513_IINDPM_STEP_MA
                * (uint32_t)(v & SGM41513_IINDPM_MASK);
    }
    return res;
}

sgm41513_result_t
sgm41513_set_vindpm_os(sgm41513_dev_t *dev, sgm41513_vindpm_os_t os)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0F, SGM41513_VINDPM_OS_MASK,
                       (uint8_t)(((uint8_t)os << SGM41513_VINDPM_OS_SHIFT)
                                 & SGM41513_VINDPM_OS_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_vindpm(sgm41513_dev_t *dev, uint32_t mv)
{
    sgm41513_result_t res;
    uint8_t reg0f, code;
    uint32_t off;

    SGM41513_CHK_DEV(dev);

    SGM41513_LOCK();
    /* 绝对 mV -> 码点，相对当前生效的 VINDPM_OS 偏置档。                  */
    res = sgm41513_rd(dev, SGM41513_REG0F, &reg0f);
    if (res == SGM41513_OK)
    {
        off = sgm41513_vindpm_os_tab[(reg0f & SGM41513_VINDPM_OS_MASK)
                                     >> SGM41513_VINDPM_OS_SHIFT];
        if (mv > off)
        {
            code = (uint8_t)((mv - off + (SGM41513_VINDPM_STEP_MV / 2u))
                             / SGM41513_VINDPM_STEP_MV);
            if (code > (SGM41513_VINDPM_CODES - 1u))
            {
                code = (uint8_t)(SGM41513_VINDPM_CODES - 1u);
            }
        }
        else
        {
            code = 0u;
        }
        res = sgm41513_rmw(dev, SGM41513_REG06, SGM41513_VINDPM_MASK, code);
    }
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_get_vindpm(sgm41513_dev_t *dev, uint32_t *mv)
{
    sgm41513_result_t res;
    uint8_t reg06, reg0f;

    SGM41513_CHK_DEV(dev);
    if (mv == NULL)
    {
        return SGM41513_ERR_PARAM;
    }

    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG06, &reg06);
    if (res == SGM41513_OK)
    {
        res = sgm41513_rd(dev, SGM41513_REG0F, &reg0f);
    }
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *mv = sgm41513_vindpm_os_tab[(reg0f & SGM41513_VINDPM_OS_MASK)
                                     >> SGM41513_VINDPM_OS_SHIFT]
              + SGM41513_VINDPM_STEP_MV
                * (uint32_t)(reg06 & SGM41513_VINDPM_MASK);
    }
    return res;
}

sgm41513_result_t
sgm41513_set_vindpm_bat_track(sgm41513_dev_t *dev, sgm41513_bat_track_t track)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG07, SGM41513_VDPM_BAT_TRACK_MASK,
                       (uint8_t)(((uint8_t)track
                                  << SGM41513_VDPM_BAT_TRACK_SHIFT)
                                 & SGM41513_VDPM_BAT_TRACK_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_input_ovp(sgm41513_dev_t *dev, sgm41513_input_ovp_t ovp)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG06, SGM41513_OVP_MASK,
                       (uint8_t)(((uint8_t)ovp << SGM41513_OVP_SHIFT)
                                 & SGM41513_OVP_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_hiz(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG00, SGM41513_EN_HIZ,
                       enable ? SGM41513_EN_HIZ : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_force_input_detection(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG07, SGM41513_IINDET_EN,
                       SGM41513_IINDET_EN);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_input_detect_done(sgm41513_dev_t *dev, bool *done)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (done == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG0E, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        *done = ((v & SGM41513_INPUT_DET_DONE) != 0u);
    }
    return res;
}

sgm41513_result_t
sgm41513_set_pfm_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG01, SGM41513_PFM_DIS,
                       enable ? 0u : SGM41513_PFM_DIS);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_q1_fullon(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG02, SGM41513_Q1_FULLON,
                       enable ? SGM41513_Q1_FULLON : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_dpm_int_mask(sgm41513_dev_t *dev, bool mask_vindpm,
                          bool mask_iindpm)
{
    sgm41513_result_t res;
    uint8_t val;

    SGM41513_CHK_DEV(dev);
    val = (uint8_t)((mask_vindpm ? SGM41513_VINDPM_INT_MASK : 0u)
                    | (mask_iindpm ? SGM41513_IINDPM_INT_MASK : 0u));

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0A,
                       SGM41513_VINDPM_INT_MASK | SGM41513_IINDPM_INT_MASK, val);
    SGM41513_UNLOCK();
    return res;
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 4. OTG 反向升压(boost)
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_OTG
sgm41513_result_t
sgm41513_otg_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);

    SGM41513_LOCK();
    res = SGM41513_OK;
    if (enable)
    {
        /* HIZ 优先于 OTG，必须先清除；数据手册要求升压(boost)启动前
         * 等待 >= 30 ms。                                                  */
        res = sgm41513_rd(dev, SGM41513_REG00, &v);
        if (res == SGM41513_OK)
        {
            if ((v & SGM41513_EN_HIZ) != 0u)
            {
                res = sgm41513_wr(dev, SGM41513_REG00,
                                  (uint8_t)(v & (uint8_t)~SGM41513_EN_HIZ));
                if (res == SGM41513_OK)
                {
                    SGM41513_UNLOCK();        /* 退出锁后再睡眠 */
                    sgm41513_io_delay_ms(SGM41513_OTG_START_DELAY_MS);
                    SGM41513_LOCK();
                }
            }
        }
    }
    if (res == SGM41513_OK)
    {
        res = sgm41513_rmw(dev, SGM41513_REG01, SGM41513_OTG_CONFIG,
                           enable ? SGM41513_OTG_CONFIG : 0u);
    }
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_boost_voltage(sgm41513_dev_t *dev, sgm41513_boost_volt_t volt)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG06, SGM41513_BOOSTV_MASK,
                       (uint8_t)(((uint8_t)volt << SGM41513_BOOSTV_SHIFT)
                                 & SGM41513_BOOSTV_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_boost_current_limit(sgm41513_dev_t *dev, sgm41513_boost_lim_t lim)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG02, SGM41513_BOOST_LIM,
                       (lim == SGM41513_BOOST_LIM_1200MA)
                           ? SGM41513_BOOST_LIM : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_boost_freq(sgm41513_dev_t *dev, sgm41513_boost_freq_t freq)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0D, SGM41513_OTGF_ITREMR,
                       (freq == SGM41513_BOOST_FREQ_1500KHZ)
                           ? SGM41513_OTGF_ITREMR : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_iterm_x6(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    /* OTGF_ITREMR 的充电模式语义：0 -> ICHG > 300 mA 时 ITERM x 6，
     * 1 -> ITERM 按编程值（POR）。                                        */
    res = sgm41513_rmw(dev, SGM41513_REG0D, SGM41513_OTGF_ITREMR,
                       enable ? 0u : SGM41513_OTGF_ITREMR);
    SGM41513_UNLOCK();
    return res;
}
#endif /* SGM41513_USE_OTG */

/* ═════════════════════════════════════════════════════════════════════════════
 * 5. 状态与故障
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief   将 VBUS_STAT[2:0] 原始编码解码为输入源类型
 * @details 编码含义因变体而异：基础型 SGM41513（PSEL 检测）与
 *          SGM41513A/D（D+/D- BC1.2 检测）仅在部分码点上不同，
 *          由 'variant'（init 时确定）选择解码表。
 * @param   raw      VBUS_STAT[2:0] 原始编码。
 * @param   variant  器件变体，见 sgm41513_variant_t。
 * @retval  sgm41513_vbus_type_t 解码后的输入源类型；
 *          未定义编码返回 SGM41513_VBUS_RESERVED。
 */
static sgm41513_vbus_type_t
sgm41513_decode_vbus(uint8_t raw, uint8_t variant)
{
    bool is_ad = (variant != (uint8_t)SGM41513_CHIP_SGM41513);

    switch (raw)
    {
    case 0u: return SGM41513_VBUS_NONE;
    case 1u: return SGM41513_VBUS_USB_SDP;
    case 7u: return SGM41513_VBUS_OTG;
    case 2u:
        /* SGM41513（PSEL）：适配器 2.4 A；A/D：USB CDP 1.5 A。            */
        return is_ad ? SGM41513_VBUS_USB_CDP : SGM41513_VBUS_ADAPTER_PSEL;
    case 3u: return is_ad ? SGM41513_VBUS_USB_DCP : SGM41513_VBUS_RESERVED;
    case 5u: return is_ad ? SGM41513_VBUS_UNKNOWN : SGM41513_VBUS_RESERVED;
    case 6u: return is_ad ? SGM41513_VBUS_NONSTD : SGM41513_VBUS_RESERVED;
    default: return SGM41513_VBUS_RESERVED;
    }
}

sgm41513_result_t
sgm41513_get_status(sgm41513_dev_t *dev, sgm41513_status_t *status)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (status == NULL)
    {
        return SGM41513_ERR_PARAM;
    }

    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG08, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        status->vbus_raw = (uint8_t)((v & SGM41513_VBUS_STAT_MASK)
                                     >> SGM41513_VBUS_STAT_SHIFT);
        status->vbus_type = sgm41513_decode_vbus(status->vbus_raw, dev->variant);
        status->charge_status = (sgm41513_charge_status_t)
            ((v & SGM41513_CHRG_STAT_MASK) >> SGM41513_CHRG_STAT_SHIFT);
        status->power_good = ((v & SGM41513_PG_STAT) != 0u);
        status->thermal_regulating = ((v & SGM41513_THERM_STAT) != 0u);
        status->vsys_regulating = ((v & SGM41513_VSYS_STAT) != 0u);
    }
    return res;
}

sgm41513_result_t
sgm41513_get_faults(sgm41513_dev_t *dev, sgm41513_faults_t *faults)
{
    sgm41513_result_t res;
    uint8_t first, live;

    SGM41513_CHK_DEV(dev);
    if (faults == NULL)
    {
        return SGM41513_ERR_PARAM;
    }

    /* REG09 的故障位锁存直至读取：第一次读返回历史（并清除），
     * 第二次读返回实时值。NTC_FAULT 两次读取均为实时值。                 */
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG09, &first);
    if (res == SGM41513_OK)
    {
        res = sgm41513_rd(dev, SGM41513_REG09, &live);
    }
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        uint8_t ntc = (uint8_t)(live & SGM41513_NTC_FAULT_MASK);
        faults->raw = live;
        faults->watchdog_fault = ((live & SGM41513_WATCHDOG_FAULT) != 0u);
        faults->boost_fault = ((live & SGM41513_BOOST_FAULT) != 0u);
        faults->charge_fault = (sgm41513_charge_fault_t)
            ((live & SGM41513_CHRG_FAULT_MASK) >> SGM41513_CHRG_FAULT_SHIFT);
        faults->battery_ovp = ((live & SGM41513_BAT_FAULT) != 0u);
        switch (ntc)
        {
        case 0u: faults->ntc_fault = SGM41513_NTC_NORMAL; break;
        case 2u: faults->ntc_fault = SGM41513_NTC_WARM;   break;
        case 3u: faults->ntc_fault = SGM41513_NTC_COOL;   break;
        case 5u: faults->ntc_fault = SGM41513_NTC_COLD;   break;
        case 6u: faults->ntc_fault = SGM41513_NTC_HOT;    break;
        default: faults->ntc_fault = SGM41513_NTC_UNKNOWN; break;
        }
    }
    return res;
}

sgm41513_result_t
sgm41513_get_dpm_status(sgm41513_dev_t *dev, sgm41513_dpm_status_t *dpm)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (dpm == NULL)
    {
        return SGM41513_ERR_PARAM;
    }

    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG0A, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        dpm->vbus_good = ((v & SGM41513_VBUS_GD) != 0u);
        dpm->vindpm_active = ((v & SGM41513_VINDPM_STAT) != 0u);
        dpm->iindpm_active = ((v & SGM41513_IINDPM_STAT) != 0u);
        dpm->topoff_active = ((v & SGM41513_TOPOFF_ACTIVE) != 0u);
        dpm->input_ovp = ((v & SGM41513_ACOV_STAT) != 0u);
    }
    return res;
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 6. JEITA
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_JEITA
sgm41513_result_t
sgm41513_jeita_configure(sgm41513_dev_t *dev, const sgm41513_jeita_cfg_t *cfg)
{
    sgm41513_result_t res;
    uint8_t reg0c;

    SGM41513_CHK_DEV(dev);
    if (cfg == NULL)
    {
        return SGM41513_ERR_PARAM;
    }

    reg0c = (uint8_t)(((cfg->cool_voltage_4v1 ? 1u : 0u) << 7)
                      | ((cfg->cool_charge_enable ? 1u : 0u) << 6)
                      | (((uint8_t)cfg->warm_current & 0x03u) << 4)
                      | (((uint8_t)cfg->cool_threshold & 0x03u) << 2)
                      | ((uint8_t)cfg->warm_threshold & 0x03u));

    SGM41513_LOCK();
    res = sgm41513_wr(dev, SGM41513_REG0C, reg0c);
    if (res == SGM41513_OK)
    {
        /* JEITA_ISET_L（REG05 D0）：冷区电流 50% / 20%。                  */
        res = sgm41513_rmw(dev, SGM41513_REG05, SGM41513_JEITA_ISET_L,
                           (cfg->cool_current == SGM41513_JEITA_COOL_I_20PCT)
                               ? SGM41513_JEITA_ISET_L : 0u);
    }
    if (res == SGM41513_OK)
    {
        /* JEITA_VSET_H（REG07 D4）：暖区电压 VREG 或 4.1 V。              */
        res = sgm41513_rmw(dev, SGM41513_REG07, SGM41513_JEITA_VSET_H,
                           cfg->warm_voltage_use_vreg ? SGM41513_JEITA_VSET_H
                                                      : 0u);
    }
    SGM41513_UNLOCK();
    return res;
}
#endif /* SGM41513_USE_JEITA */

/* ═════════════════════════════════════════════════════════════════════════════
 * 7. 看门狗 / 计时器 / 热调节
 * ═════════════════════════════════════════════════════════════════════════════ */

sgm41513_result_t
sgm41513_set_watchdog(sgm41513_dev_t *dev, sgm41513_watchdog_t wdt)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG05, SGM41513_WATCHDOG_MASK,
                       (uint8_t)(((uint8_t)wdt << SGM41513_WATCHDOG_SHIFT)
                                 & SGM41513_WATCHDOG_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_feed_watchdog(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG01, SGM41513_WD_RST, SGM41513_WD_RST);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_safety_timer(sgm41513_dev_t *dev, sgm41513_safety_timer_t hours,
                          bool enable)
{
    sgm41513_result_t res;
    uint8_t val;

    SGM41513_CHK_DEV(dev);
    val = (uint8_t)((enable ? SGM41513_EN_TIMER : 0u)
                    | ((hours == SGM41513_SAFETY_TIMER_16H)
                           ? SGM41513_CHG_TIMER : 0u));

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG05,
                       SGM41513_EN_TIMER | SGM41513_CHG_TIMER, val);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_safety_timer_slow2x(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG07, SGM41513_TMR2X_EN,
                       enable ? SGM41513_TMR2X_EN : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_thermal_reg_threshold(sgm41513_dev_t *dev, sgm41513_treg_t treg)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG05, SGM41513_TREG,
                       (treg == SGM41513_TREG_120C) ? SGM41513_TREG : 0u);
    SGM41513_UNLOCK();
    return res;
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 8. 船运模式 / BATFET
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_SHIP
sgm41513_result_t
sgm41513_enter_ship_mode(sgm41513_dev_t *dev, bool delayed)
{
    sgm41513_result_t res;

    SGM41513_CHK_DEV(dev);

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG07, SGM41513_BATFET_DLY,
                       delayed ? SGM41513_BATFET_DLY : 0u);
    if (res == SGM41513_OK)
    {
        res = sgm41513_rmw(dev, SGM41513_REG07, SGM41513_BATFET_DIS,
                           SGM41513_BATFET_DIS);
    }
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_batfet_reset_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG07, SGM41513_BATFET_RST_EN,
                       enable ? SGM41513_BATFET_RST_EN : 0u);
    SGM41513_UNLOCK();
    return res;
}
#endif /* SGM41513_USE_SHIP */

/* ═════════════════════════════════════════════════════════════════════════════
 * 9. PUMPX
 * ═════════════════════════════════════════════════════════════════════════════ */

#if SGM41513_USE_PUMPX
sgm41513_result_t
sgm41513_pumpx_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0D, SGM41513_EN_PUMPX,
                       enable ? SGM41513_EN_PUMPX : 0u);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_pumpx_trigger_up(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0D, SGM41513_PUMPX_UP,
                       SGM41513_PUMPX_UP);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_pumpx_trigger_down(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0D, SGM41513_PUMPX_DN,
                       SGM41513_PUMPX_DN);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_pumpx_busy(sgm41513_dev_t *dev, bool *busy)
{
    sgm41513_result_t res;
    uint8_t v;

    SGM41513_CHK_DEV(dev);
    if (busy == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, SGM41513_REG0D, &v);
    SGM41513_UNLOCK();

    if (res == SGM41513_OK)
    {
        /* 脉冲序列完成后 PUMPX_UP/DN 自清零。                             */
        *busy = ((v & (SGM41513_PUMPX_UP | SGM41513_PUMPX_DN)) != 0u);
    }
    return res;
}
#endif /* SGM41513_USE_PUMPX */

/* ═════════════════════════════════════════════════════════════════════════════
 * 10. STAT 引脚 / D+ D- 线
 * ═════════════════════════════════════════════════════════════════════════════ */

sgm41513_result_t
sgm41513_set_stat_pin_mode(sgm41513_dev_t *dev, sgm41513_stat_pin_mode_t mode)
{
    sgm41513_result_t res;
    uint8_t val;

    SGM41513_CHK_DEV(dev);
    switch (mode)
    {
    case SGM41513_STAT_PIN_CHARGE: val = SGM41513_EN_ICHG_MON_CHARGE;  break;
    case SGM41513_STAT_PIN_MANUAL: val = SGM41513_EN_ICHG_MON_STATSET; break;
    default:                       val = SGM41513_EN_ICHG_MON_DISABLE; break;
    }

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG00, SGM41513_EN_ICHG_MON_MASK, val);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_stat_pin_pattern(sgm41513_dev_t *dev, sgm41513_stat_pattern_t p)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0F, SGM41513_STAT_SET_MASK,
                       (uint8_t)(((uint8_t)p << SGM41513_STAT_SET_SHIFT)
                                 & SGM41513_STAT_SET_MASK));
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_set_dpdm_voltage(sgm41513_dev_t *dev, sgm41513_dpdm_vset_t dp,
                          sgm41513_dpdm_vset_t dm)
{
    sgm41513_result_t res;
    uint8_t val;

    SGM41513_CHK_DEV(dev);
    val = (uint8_t)((((uint8_t)dp & 0x03u) << SGM41513_DP_VSET_SHIFT)
                    | (((uint8_t)dm & 0x03u) << SGM41513_DM_VSET_SHIFT));

    SGM41513_LOCK();
    res = sgm41513_rmw(dev, SGM41513_REG0D,
                       SGM41513_DP_VSET_MASK | SGM41513_DM_VSET_MASK, val);
    SGM41513_UNLOCK();
    return res;
}

/* ═════════════════════════════════════════════════════════════════════════════
 * 11. 寄存器级访问
 * ═════════════════════════════════════════════════════════════════════════════ */

sgm41513_result_t
sgm41513_read_reg(sgm41513_dev_t *dev, uint8_t reg, uint8_t *val)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    if (val == NULL)
    {
        return SGM41513_ERR_PARAM;
    }
    SGM41513_LOCK();
    res = sgm41513_rd(dev, reg, val);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_write_reg(sgm41513_dev_t *dev, uint8_t reg, uint8_t val)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_wr(dev, reg, val);
    SGM41513_UNLOCK();
    return res;
}

sgm41513_result_t
sgm41513_update_bits(sgm41513_dev_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    sgm41513_result_t res;
    SGM41513_CHK_DEV(dev);
    SGM41513_LOCK();
    res = sgm41513_rmw(dev, reg, mask, val);
    SGM41513_UNLOCK();
    return res;
}
