/*
 * sgm41513.c - Driver core for the SGM41513 battery charger family.
 *
 * Portable C99, no dynamic memory, no platform headers. All hardware
 * access goes through the four functions declared in sgm41513_io.h.
 *
 * Datasheet: SGM41513_SGM41513A_SGM41513D, APRIL 2025 REV. C.1
 */

#include "sgm41513.h"
#include "sgm41513_regs.h"
#include "sgm41513_io.h"

/* ---------------------------------------------------------------------- */
/* Concurrency helpers                                                     */
/* ---------------------------------------------------------------------- */

#if SGM41513_THREAD_SAFE
#define DRV_LOCK()     sgm41513_io_lock()
#define DRV_UNLOCK()   sgm41513_io_unlock()
#else
#define DRV_LOCK()     ((void)0)
#define DRV_UNLOCK()   ((void)0)
#endif

#define CHK_DEV(d)     do { \
    if ((d) == NULL) return SGM41513_ERR_PARAM; \
    if ((d)->inited == 0u) return SGM41513_ERR_NOT_READY; \
} while (0)

/* ---------------------------------------------------------------------- */
/* Conversion tables (datasheet "Linear Table" sections)                    */
/* ---------------------------------------------------------------------- */

/* ICHG[5:0] fast charge current in mA (REG02), non-linear, 0..3000 mA   */
static const uint16_t sgm_ichg_tab[64] = {
       0,    5,   10,   15,   20,   25,   30,   35,
      40,   50,   60,   70,   80,   90,  100,  110,
     130,  150,  170,  190,  210,  230,  250,  270,
     300,  330,  360,  390,  420,  450,  480,  510,
     540,  600,  660,  720,  780,  840,  900,  960,
    1020, 1080, 1140, 1200, 1260, 1320, 1380, 1440,
    1500, 1620, 1740, 1860, 1980, 2100, 2220, 2340,
    2460, 2580, 2700, 2820, 2940, 3000, 3000, 3000
};

/* IPRECHG[3:0] / ITERM[3:0] in mA (REG03), 5..240 mA                    */
static const uint16_t sgm_preterm_tab[16] = {
      5,  10,  15,  20,  30,  40,  50,  60,
     80, 100, 120, 140, 160, 180, 200, 240
};

/* SYS_MIN[2:0] minimum system voltage in mV (REG01), non-uniform steps  */
static const uint16_t sgm_sysmin_tab[8] = {
    2600, 2800, 3000, 3200, 3400, 3500, 3600, 3700
};

/* VINDPM_OS[1:0] window offsets in mV (REG0F)                           */
static const uint16_t sgm_vindpm_os_tab[4] = { 3900, 5900, 7500, 10500 };

/* Registers that are fully read/write; they form the shadow cache and
 * are replayed by sgm41513_restore_settings(). REG08/09/0E are read
 * only, REG0A/0B only carry individual R/W bits.                        */
static const uint8_t sgm_rw_regs[11] = {
    SGM41513_REG00, SGM41513_REG01, SGM41513_REG02, SGM41513_REG03,
    SGM41513_REG04, SGM41513_REG05, SGM41513_REG06, SGM41513_REG07,
    SGM41513_REG0C, SGM41513_REG0D, SGM41513_REG0F
};

static const uint8_t sgm_rw_por[11] = {
    SGM41513_REG00_POR, SGM41513_REG01_POR, SGM41513_REG02_POR,
    SGM41513_REG03_POR, SGM41513_REG04_POR, SGM41513_REG05_POR,
    SGM41513_REG06_POR, SGM41513_REG07_POR, SGM41513_REG0C_POR,
    SGM41513_REG0D_POR, SGM41513_REG0F_POR
};

static bool sgm_is_rw_reg(uint8_t reg)
{
    uint8_t i;
    for (i = 0; i < (uint8_t)(sizeof(sgm_rw_regs)); i++) {
        if (sgm_rw_regs[i] == reg) {
            return true;
        }
    }
    return false;
}

/* Bits that self-clear or change without host action; excluded from the
 * optional write verification so it does not produce false failures.    */
static uint8_t sgm_volatile_bits(uint8_t reg)
{
    switch (reg) {
    case SGM41513_REG01: return SGM41513_WD_RST;
    case SGM41513_REG07: return SGM41513_IINDET_EN;
    case SGM41513_REG0D: return SGM41513_PUMPX_UP | SGM41513_PUMPX_DN;
    default:             return 0u;
    }
}

/* ---------------------------------------------------------------------- */
/* Low-level register access (lock must be held by the caller)             */
/* ---------------------------------------------------------------------- */

static sgm41513_result_t sgm_rd(sgm41513_dev_t *dev, uint8_t reg, uint8_t *val)
{
    if (sgm41513_io_read_reg(dev->io_ctx, dev->dev_addr, reg, val)
            != SGM41513_IO_OK) {
        return SGM41513_ERR_IO;
    }
    return SGM41513_OK;
}

static sgm41513_result_t sgm_wr(sgm41513_dev_t *dev, uint8_t reg, uint8_t val)
{
    if (sgm41513_io_write_reg(dev->io_ctx, dev->dev_addr, reg, val)
            != SGM41513_IO_OK) {
        return SGM41513_ERR_IO;
    }

#if SGM41513_REG_SHADOW
    if (sgm_is_rw_reg(reg)) {
        dev->shadow[reg] = val;
    }
#endif

#if SGM41513_VERIFY_WRITES
    if (sgm_is_rw_reg(reg)) {
        uint8_t rb;
        uint8_t mask = (uint8_t)(0xFFu & (uint8_t)~sgm_volatile_bits(reg));
        if (sgm_rd(dev, reg, &rb) != SGM41513_OK) {
            return SGM41513_ERR_IO;
        }
        if ((uint8_t)(rb & mask) != (uint8_t)(val & mask)) {
            return SGM41513_ERR_VERIFY;
        }
    }
#else
    (void)sgm_volatile_bits; /* keep the helper referenced */
#endif

    return SGM41513_OK;
}

static sgm41513_result_t sgm_rmw(sgm41513_dev_t *dev, uint8_t reg,
                                 uint8_t mask, uint8_t val)
{
    sgm41513_result_t res;
    uint8_t old;

    res = sgm_rd(dev, reg, &old);
    if (res != SGM41513_OK) {
        return res;
    }
    return sgm_wr(dev, reg, (uint8_t)((old & (uint8_t)~mask) | (val & mask)));
}

/* ---------------------------------------------------------------------- */
/* Unit conversion helpers                                                 */
/* ---------------------------------------------------------------------- */

static uint8_t sgm_lut_nearest(const uint16_t *tab, uint8_t n, uint32_t val)
{
    uint8_t i, best = 0;
    uint32_t bestd = 0xFFFFFFFFu;

    for (i = 0; i < n; i++) {
        uint32_t t = tab[i];
        uint32_t d = (t > val) ? (t - val) : (val - t);
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return best;
}

/* VREG[4:0] (REG04): 3856 + 32*code mV for code <= 24, except code 15
 * which is a 4350 mV special case; codes above 24 clamp to code 24.     */
static uint32_t sgm_vreg_from_code(uint8_t code)
{
    if (code > SGM41513_VREG_CODE_MAX) {
        code = SGM41513_VREG_CODE_MAX;
    }
    if (code == 15u) {
        return 4350u;
    }
    return 3856u + (SGM41513_VREG_STEP_MV * (uint32_t)code);
}

static uint8_t sgm_vreg_to_code(uint32_t mv)
{
    uint8_t code, best = 0;
    uint32_t bestd = 0xFFFFFFFFu;

    for (code = 0; code <= SGM41513_VREG_CODE_MAX; code++) {
        uint32_t v = sgm_vreg_from_code(code);
        uint32_t d = (v > mv) ? (v - mv) : (mv - v);
        if (d < bestd) {
            bestd = d;
            best = code;
        }
    }
    return best;
}

/* IINDPM[4:0] (REG00): 100 + 100*code mA, rounded to nearest step       */
static uint8_t sgm_iindpm_to_code(uint32_t ma)
{
    uint32_t code;

    if (ma <= SGM41513_IINDPM_MIN_MA) {
        return 0u;
    }
    code = (ma - SGM41513_IINDPM_MIN_MA + (SGM41513_IINDPM_STEP_MA / 2u))
           / SGM41513_IINDPM_STEP_MA;
    if (code > 31u) {
        code = 31u;
    }
    return (uint8_t)code;
}

/* ---------------------------------------------------------------------- */
/* 1. Initialization / identification                                       */
/* ---------------------------------------------------------------------- */

sgm41513_result_t sgm41513_init(sgm41513_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr)
{
    sgm41513_result_t res;
    uint8_t v, pn;

    if (dev == NULL) {
        return SGM41513_ERR_PARAM;
    }

    dev->io_ctx = io_ctx;
    dev->dev_addr = (dev_addr != 0u) ? dev_addr : SGM41513_I2C_ADDR;
    dev->inited = 0u;
    dev->variant = (uint8_t)SGM41513_VARIANT;

    if (sgm41513_io_init() != SGM41513_IO_OK) {
        return SGM41513_ERR_IO;
    }

    DRV_LOCK();

    /* Validate the part: SGMPART must read 0, PN must be 0 or 1.       */
    res = sgm_rd(dev, SGM41513_REG0B, &v);
    if (res != SGM41513_OK) {
        DRV_UNLOCK();
        return res;
    }
    if ((v & SGM41513_SGMPART_MASK) != 0u) {
        DRV_UNLOCK();
        return SGM41513_ERR_NOT_READY;
    }
    pn = (uint8_t)((v & SGM41513_PN_MASK) >> SGM41513_PN_SHIFT);
    if (pn > 1u) {
        DRV_UNLOCK();
        return SGM41513_ERR_NOT_READY;
    }
    /* PN = 0 -> SGM41513; PN = 1 -> SGM41513A or D (config resolves).  */
    if (pn == 0u) {
        dev->variant = (uint8_t)SGM41513_CHIP_SGM41513;
    }

#if SGM41513_REG_SHADOW
    {
        /* Snapshot the current R/W register images so that
         * restore_settings() replays the live configuration.           */
        uint8_t i;
        for (i = 0; i < (uint8_t)(sizeof(sgm_rw_regs)); i++) {
            res = sgm_rd(dev, sgm_rw_regs[i], &dev->shadow[sgm_rw_regs[i]]);
            if (res != SGM41513_OK) {
                DRV_UNLOCK();
                return res;
            }
        }
    }
#else
    (void)sgm_rw_regs; (void)sgm_rw_por; (void)sgm_is_rw_reg;
#endif

    dev->inited = 1u;
    DRV_UNLOCK();
    return SGM41513_OK;
}

sgm41513_result_t sgm41513_get_id(sgm41513_dev_t *dev, sgm41513_id_t *id)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (id == NULL) {
        return SGM41513_ERR_PARAM;
    }

    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG0B, &v);
    DRV_UNLOCK();

    if (res != SGM41513_OK) {
        return res;
    }
    id->pn_raw = (uint8_t)((v & SGM41513_PN_MASK) >> SGM41513_PN_SHIFT);
    id->variant = (sgm41513_variant_t)dev->variant;
    id->dev_rev = (uint8_t)(v & SGM41513_DEV_REV_MASK);
    return SGM41513_OK;
}

sgm41513_result_t sgm41513_reset(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);

    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG0B, &v);
    if (res == SGM41513_OK) {
        /* REG_RST self-clears; REG0B is not part of the shadow cache.  */
        res = sgm_wr(dev, SGM41513_REG0B, (uint8_t)(v | SGM41513_REG_RST));
    }
#if SGM41513_REG_SHADOW
    if (res == SGM41513_OK) {
        uint8_t i;
        for (i = 0; i < (uint8_t)(sizeof(sgm_rw_regs)); i++) {
            dev->shadow[sgm_rw_regs[i]] = sgm_rw_por[i];
        }
    }
#endif
    DRV_UNLOCK();
    return res;
}

#if SGM41513_REG_SHADOW
sgm41513_result_t sgm41513_restore_settings(sgm41513_dev_t *dev)
{
    sgm41513_result_t res = SGM41513_OK;
    uint8_t i;

    CHK_DEV(dev);

    DRV_LOCK();
    for (i = 0; i < (uint8_t)(sizeof(sgm_rw_regs)); i++) {
        uint8_t reg = sgm_rw_regs[i];
        res = sgm_wr(dev, reg, dev->shadow[reg]);
        if (res != SGM41513_OK) {
            break;
        }
    }
    DRV_UNLOCK();
    return res;
}
#endif

/* ---------------------------------------------------------------------- */
/* 2. Charging control                                                     */
/* ---------------------------------------------------------------------- */

sgm41513_result_t sgm41513_charge_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG01, SGM41513_CHG_CONFIG,
                  enable ? SGM41513_CHG_CONFIG : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_ichg(sgm41513_dev_t *dev, uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    CHK_DEV(dev);
    code = sgm_lut_nearest(sgm_ichg_tab, 64, ma);

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG02, SGM41513_ICHG_MASK,
                  (uint8_t)(code << SGM41513_ICHG_SHIFT));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_get_ichg(sgm41513_dev_t *dev, uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (ma == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG02, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *ma = sgm_ichg_tab[(v & SGM41513_ICHG_MASK) >> SGM41513_ICHG_SHIFT];
    }
    return res;
}

sgm41513_result_t sgm41513_set_vreg(sgm41513_dev_t *dev, uint32_t mv)
{
    sgm41513_result_t res;
    uint8_t code;

    CHK_DEV(dev);
    code = sgm_vreg_to_code(mv);

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG04, SGM41513_VREG_MASK,
                  (uint8_t)(code << SGM41513_VREG_SHIFT));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_get_vreg(sgm41513_dev_t *dev, uint32_t *mv)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (mv == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG04, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *mv = sgm_vreg_from_code(
            (v & SGM41513_VREG_MASK) >> SGM41513_VREG_SHIFT);
    }
    return res;
}

sgm41513_result_t sgm41513_set_vreg_ft(sgm41513_dev_t *dev,
                                        sgm41513_vreg_ft_t ft)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0F, SGM41513_VREG_FT_MASK,
                  (uint8_t)(((uint8_t)ft << SGM41513_VREG_FT_SHIFT)
                            & SGM41513_VREG_FT_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    CHK_DEV(dev);
    code = sgm_lut_nearest(sgm_preterm_tab, 16, ma);

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG03, SGM41513_IPRECHG_MASK,
                  (uint8_t)(code << SGM41513_IPRECHG_SHIFT));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_get_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (ma == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG03, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *ma = sgm_preterm_tab[(v & SGM41513_IPRECHG_MASK)
                              >> SGM41513_IPRECHG_SHIFT];
    }
    return res;
}

sgm41513_result_t sgm41513_set_term_current(sgm41513_dev_t *dev, uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    CHK_DEV(dev);
    code = sgm_lut_nearest(sgm_preterm_tab, 16, ma);

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG03, SGM41513_ITERM_MASK,
                  (uint8_t)(code << SGM41513_ITERM_SHIFT));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_get_term_current(sgm41513_dev_t *dev, uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (ma == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG03, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *ma = sgm_preterm_tab[v & SGM41513_ITERM_MASK];
    }
    return res;
}

sgm41513_result_t sgm41513_set_term_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG05, SGM41513_EN_TERM,
                  enable ? SGM41513_EN_TERM : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_term_deglitch(sgm41513_dev_t *dev,
                                             sgm41513_term_deglitch_t t)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG05, SGM41513_ITERM_TIMER,
                  (t == SGM41513_ITERM_DEGLITCH_16MS)
                      ? SGM41513_ITERM_TIMER : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_recharge_threshold(sgm41513_dev_t *dev,
                                                  sgm41513_vrechg_t mv)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG04, SGM41513_VRECHG,
                  (mv == SGM41513_VRECHG_200MV) ? SGM41513_VRECHG : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_topoff_timer(sgm41513_dev_t *dev,
                                            sgm41513_topoff_t t)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG04, SGM41513_TOPOFF_TIMER_MASK,
                  (uint8_t)(((uint8_t)t << SGM41513_TOPOFF_TIMER_SHIFT)
                            & SGM41513_TOPOFF_TIMER_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_trickle_current(sgm41513_dev_t *dev,
                                               sgm41513_trickle_t ma)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0F, SGM41513_ISHORT_SET,
                  (ma == SGM41513_TRICKLE_30MA) ? SGM41513_ISHORT_SET : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t mv)
{
    sgm41513_result_t res;
    uint8_t code;

    CHK_DEV(dev);
    code = sgm_lut_nearest(sgm_sysmin_tab, 8, mv);

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG01, SGM41513_SYS_MIN_MASK,
                  (uint8_t)(code << SGM41513_SYS_MIN_SHIFT));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_get_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t *mv)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (mv == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG01, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *mv = sgm_sysmin_tab[(v & SGM41513_SYS_MIN_MASK)
                             >> SGM41513_SYS_MIN_SHIFT];
    }
    return res;
}

sgm41513_result_t sgm41513_set_min_vbat_otg(sgm41513_dev_t *dev,
                                            sgm41513_min_vbat_otg_t sel)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG01, SGM41513_MIN_BAT_SEL,
                  (sel == SGM41513_MIN_VBAT_OTG_2600MV)
                      ? SGM41513_MIN_BAT_SEL : 0u);
    DRV_UNLOCK();
    return res;
}

/* ---------------------------------------------------------------------- */
/* 3. Input management                                                     */
/* ---------------------------------------------------------------------- */

sgm41513_result_t sgm41513_set_iindpm(sgm41513_dev_t *dev, uint32_t ma)
{
    sgm41513_result_t res;
    uint8_t code;

    CHK_DEV(dev);
    code = sgm_iindpm_to_code(ma);

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG00, SGM41513_IINDPM_MASK, code);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_get_iindpm(sgm41513_dev_t *dev, uint32_t *ma)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (ma == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG00, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *ma = SGM41513_IINDPM_MIN_MA
              + SGM41513_IINDPM_STEP_MA
                * (uint32_t)(v & SGM41513_IINDPM_MASK);
    }
    return res;
}

sgm41513_result_t sgm41513_set_vindpm_os(sgm41513_dev_t *dev,
                                         sgm41513_vindpm_os_t os)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0F, SGM41513_VINDPM_OS_MASK,
                  (uint8_t)(((uint8_t)os << SGM41513_VINDPM_OS_SHIFT)
                            & SGM41513_VINDPM_OS_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_vindpm(sgm41513_dev_t *dev, uint32_t mv)
{
    sgm41513_result_t res;
    uint8_t reg0f, code;
    uint32_t off;

    CHK_DEV(dev);

    DRV_LOCK();
    /* Absolute mV -> code, relative to the active VINDPM_OS offset.    */
    res = sgm_rd(dev, SGM41513_REG0F, &reg0f);
    if (res == SGM41513_OK) {
        off = sgm_vindpm_os_tab[(reg0f & SGM41513_VINDPM_OS_MASK)
                                >> SGM41513_VINDPM_OS_SHIFT];
        if (mv > off) {
            code = (uint8_t)((mv - off + (SGM41513_VINDPM_STEP_MV / 2u))
                             / SGM41513_VINDPM_STEP_MV);
            if (code > (SGM41513_VINDPM_CODES - 1u)) {
                code = (uint8_t)(SGM41513_VINDPM_CODES - 1u);
            }
        } else {
            code = 0u;
        }
        res = sgm_rmw(dev, SGM41513_REG06, SGM41513_VINDPM_MASK, code);
    }
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_get_vindpm(sgm41513_dev_t *dev, uint32_t *mv)
{
    sgm41513_result_t res;
    uint8_t reg06, reg0f;

    CHK_DEV(dev);
    if (mv == NULL) {
        return SGM41513_ERR_PARAM;
    }

    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG06, &reg06);
    if (res == SGM41513_OK) {
        res = sgm_rd(dev, SGM41513_REG0F, &reg0f);
    }
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *mv = sgm_vindpm_os_tab[(reg0f & SGM41513_VINDPM_OS_MASK)
                                >> SGM41513_VINDPM_OS_SHIFT]
              + SGM41513_VINDPM_STEP_MV
                * (uint32_t)(reg06 & SGM41513_VINDPM_MASK);
    }
    return res;
}

sgm41513_result_t sgm41513_set_vindpm_bat_track(sgm41513_dev_t *dev,
                                                sgm41513_bat_track_t track)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG07, SGM41513_VDPM_BAT_TRACK_MASK,
                  (uint8_t)(((uint8_t)track << SGM41513_VDPM_BAT_TRACK_SHIFT)
                            & SGM41513_VDPM_BAT_TRACK_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_input_ovp(sgm41513_dev_t *dev,
                                         sgm41513_input_ovp_t ovp)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG06, SGM41513_OVP_MASK,
                  (uint8_t)(((uint8_t)ovp << SGM41513_OVP_SHIFT)
                            & SGM41513_OVP_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_hiz(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG00, SGM41513_EN_HIZ,
                  enable ? SGM41513_EN_HIZ : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_force_input_detection(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG07, SGM41513_IINDET_EN,
                  SGM41513_IINDET_EN);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_input_detect_done(sgm41513_dev_t *dev, bool *done)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (done == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG0E, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        *done = ((v & SGM41513_INPUT_DET_DONE) != 0u);
    }
    return res;
}

sgm41513_result_t sgm41513_set_pfm_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG01, SGM41513_PFM_DIS,
                  enable ? 0u : SGM41513_PFM_DIS);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_q1_fullon(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG02, SGM41513_Q1_FULLON,
                  enable ? SGM41513_Q1_FULLON : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_dpm_int_mask(sgm41513_dev_t *dev,
                                            bool mask_vindpm,
                                            bool mask_iindpm)
{
    sgm41513_result_t res;
    uint8_t val;

    CHK_DEV(dev);
    val = (uint8_t)((mask_vindpm ? SGM41513_VINDPM_INT_MASK : 0u)
                    | (mask_iindpm ? SGM41513_IINDPM_INT_MASK : 0u));

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0A,
                  SGM41513_VINDPM_INT_MASK | SGM41513_IINDPM_INT_MASK, val);
    DRV_UNLOCK();
    return res;
}

/* ---------------------------------------------------------------------- */
/* 4. OTG reverse boost                                                    */
/* ---------------------------------------------------------------------- */

#if SGM41513_USE_OTG
sgm41513_result_t sgm41513_otg_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);

    DRV_LOCK();
    res = SGM41513_OK;
    if (enable) {
        /* HIZ has priority over OTG and must be cleared first; the
         * datasheet requires >= 30 ms before the boost can start.      */
        res = sgm_rd(dev, SGM41513_REG00, &v);
        if (res == SGM41513_OK) {
            if ((v & SGM41513_EN_HIZ) != 0u) {
                res = sgm_wr(dev, SGM41513_REG00,
                             (uint8_t)(v & (uint8_t)~SGM41513_EN_HIZ));
                if (res == SGM41513_OK) {
                    DRV_UNLOCK();             /* sleep outside the lock */
                    sgm41513_io_delay_ms(SGM41513_OTG_START_DELAY_MS);
                    DRV_LOCK();
                }
            }
        }
    }
    if (res == SGM41513_OK) {
        res = sgm_rmw(dev, SGM41513_REG01, SGM41513_OTG_CONFIG,
                      enable ? SGM41513_OTG_CONFIG : 0u);
    }
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_boost_voltage(sgm41513_dev_t *dev,
                                             sgm41513_boost_volt_t volt)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG06, SGM41513_BOOSTV_MASK,
                  (uint8_t)(((uint8_t)volt << SGM41513_BOOSTV_SHIFT)
                            & SGM41513_BOOSTV_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_boost_current_limit(sgm41513_dev_t *dev,
                                                   sgm41513_boost_lim_t lim)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG02, SGM41513_BOOST_LIM,
                  (lim == SGM41513_BOOST_LIM_1200MA)
                      ? SGM41513_BOOST_LIM : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_boost_freq(sgm41513_dev_t *dev,
                                          sgm41513_boost_freq_t freq)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0D, SGM41513_OTGF_ITREMR,
                  (freq == SGM41513_BOOST_FREQ_1500KHZ)
                      ? SGM41513_OTGF_ITREMR : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_iterm_x6(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    /* Charge-mode meaning of OTGF_ITREMR: 0 -> ITERM x 6 when
     * ICHG > 300 mA, 1 -> ITERM as programmed (POR).                   */
    res = sgm_rmw(dev, SGM41513_REG0D, SGM41513_OTGF_ITREMR,
                  enable ? 0u : SGM41513_OTGF_ITREMR);
    DRV_UNLOCK();
    return res;
}
#endif /* SGM41513_USE_OTG */

/* ---------------------------------------------------------------------- */
/* 5. Status and faults                                                    */
/* ---------------------------------------------------------------------- */

static sgm41513_vbus_type_t sgm_decode_vbus(uint8_t raw, uint8_t variant)
{
    bool is_ad = (variant != (uint8_t)SGM41513_CHIP_SGM41513);

    switch (raw) {
    case 0u: return SGM41513_VBUS_NONE;
    case 1u: return SGM41513_VBUS_USB_SDP;
    case 7u: return SGM41513_VBUS_OTG;
    case 2u:
        /* SGM41513 (PSEL): adapter 2.4 A; A/D: USB CDP 1.5 A.          */
        return is_ad ? SGM41513_VBUS_USB_CDP : SGM41513_VBUS_ADAPTER_PSEL;
    case 3u: return is_ad ? SGM41513_VBUS_USB_DCP : SGM41513_VBUS_RESERVED;
    case 5u: return is_ad ? SGM41513_VBUS_UNKNOWN : SGM41513_VBUS_RESERVED;
    case 6u: return is_ad ? SGM41513_VBUS_NONSTD : SGM41513_VBUS_RESERVED;
    default: return SGM41513_VBUS_RESERVED;
    }
}

sgm41513_result_t sgm41513_get_status(sgm41513_dev_t *dev,
                                      sgm41513_status_t *status)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (status == NULL) {
        return SGM41513_ERR_PARAM;
    }

    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG08, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        status->vbus_raw = (uint8_t)((v & SGM41513_VBUS_STAT_MASK)
                                     >> SGM41513_VBUS_STAT_SHIFT);
        status->vbus_type = sgm_decode_vbus(status->vbus_raw, dev->variant);
        status->charge_status = (sgm41513_charge_status_t)
            ((v & SGM41513_CHRG_STAT_MASK) >> SGM41513_CHRG_STAT_SHIFT);
        status->power_good = ((v & SGM41513_PG_STAT) != 0u);
        status->thermal_regulating = ((v & SGM41513_THERM_STAT) != 0u);
        status->vsys_regulating = ((v & SGM41513_VSYS_STAT) != 0u);
    }
    return res;
}

sgm41513_result_t sgm41513_get_faults(sgm41513_dev_t *dev,
                                      sgm41513_faults_t *faults)
{
    sgm41513_result_t res;
    uint8_t first, live;

    CHK_DEV(dev);
    if (faults == NULL) {
        return SGM41513_ERR_PARAM;
    }

    /* REG09 latches fault bits until read: the first read returns the
     * history (and clears it), the second read returns live values.
     * NTC_FAULT is real time in both reads.                            */
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG09, &first);
    if (res == SGM41513_OK) {
        res = sgm_rd(dev, SGM41513_REG09, &live);
    }
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        uint8_t ntc = (uint8_t)(live & SGM41513_NTC_FAULT_MASK);
        faults->raw = live;
        faults->watchdog_fault = ((live & SGM41513_WATCHDOG_FAULT) != 0u);
        faults->boost_fault = ((live & SGM41513_BOOST_FAULT) != 0u);
        faults->charge_fault = (sgm41513_charge_fault_t)
            ((live & SGM41513_CHRG_FAULT_MASK) >> SGM41513_CHRG_FAULT_SHIFT);
        faults->battery_ovp = ((live & SGM41513_BAT_FAULT) != 0u);
        switch (ntc) {
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

sgm41513_result_t sgm41513_get_dpm_status(sgm41513_dev_t *dev,
                                          sgm41513_dpm_status_t *dpm)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (dpm == NULL) {
        return SGM41513_ERR_PARAM;
    }

    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG0A, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        dpm->vbus_good = ((v & SGM41513_VBUS_GD) != 0u);
        dpm->vindpm_active = ((v & SGM41513_VINDPM_STAT) != 0u);
        dpm->iindpm_active = ((v & SGM41513_IINDPM_STAT) != 0u);
        dpm->topoff_active = ((v & SGM41513_TOPOFF_ACTIVE) != 0u);
        dpm->input_ovp = ((v & SGM41513_ACOV_STAT) != 0u);
    }
    return res;
}

/* ---------------------------------------------------------------------- */
/* 6. JEITA                                                                */
/* ---------------------------------------------------------------------- */

#if SGM41513_USE_JEITA
sgm41513_result_t sgm41513_jeita_configure(sgm41513_dev_t *dev,
                                           const sgm41513_jeita_cfg_t *cfg)
{
    sgm41513_result_t res;
    uint8_t reg0c;

    CHK_DEV(dev);
    if (cfg == NULL) {
        return SGM41513_ERR_PARAM;
    }

    reg0c = (uint8_t)(((cfg->cool_voltage_4v1 ? 1u : 0u) << 7)
                      | ((cfg->cool_charge_enable ? 1u : 0u) << 6)
                      | (((uint8_t)cfg->warm_current & 0x03u) << 4)
                      | (((uint8_t)cfg->cool_threshold & 0x03u) << 2)
                      | ((uint8_t)cfg->warm_threshold & 0x03u));

    DRV_LOCK();
    res = sgm_wr(dev, SGM41513_REG0C, reg0c);
    if (res == SGM41513_OK) {
        /* JEITA_ISET_L (REG05 D0): cool range current 50% / 20%.       */
        res = sgm_rmw(dev, SGM41513_REG05, SGM41513_JEITA_ISET_L,
                      (cfg->cool_current == SGM41513_JEITA_COOL_I_20PCT)
                          ? SGM41513_JEITA_ISET_L : 0u);
    }
    if (res == SGM41513_OK) {
        /* JEITA_VSET_H (REG07 D4): warm range voltage VREG or 4.1 V.   */
        res = sgm_rmw(dev, SGM41513_REG07, SGM41513_JEITA_VSET_H,
                      cfg->warm_voltage_use_vreg ? SGM41513_JEITA_VSET_H
                                                 : 0u);
    }
    DRV_UNLOCK();
    return res;
}
#endif /* SGM41513_USE_JEITA */

/* ---------------------------------------------------------------------- */
/* 7. Watchdog / timers / thermal                                          */
/* ---------------------------------------------------------------------- */

sgm41513_result_t sgm41513_set_watchdog(sgm41513_dev_t *dev,
                                        sgm41513_watchdog_t wdt)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG05, SGM41513_WATCHDOG_MASK,
                  (uint8_t)(((uint8_t)wdt << SGM41513_WATCHDOG_SHIFT)
                            & SGM41513_WATCHDOG_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_feed_watchdog(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG01, SGM41513_WD_RST, SGM41513_WD_RST);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_safety_timer(sgm41513_dev_t *dev,
                                            sgm41513_safety_timer_t hours,
                                            bool enable)
{
    sgm41513_result_t res;
    uint8_t val;

    CHK_DEV(dev);
    val = (uint8_t)((enable ? SGM41513_EN_TIMER : 0u)
                    | ((hours == SGM41513_SAFETY_TIMER_16H)
                           ? SGM41513_CHG_TIMER : 0u));

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG05,
                  SGM41513_EN_TIMER | SGM41513_CHG_TIMER, val);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_safety_timer_slow2x(sgm41513_dev_t *dev,
                                                   bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG07, SGM41513_TMR2X_EN,
                  enable ? SGM41513_TMR2X_EN : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_thermal_reg_threshold(sgm41513_dev_t *dev,
                                                     sgm41513_treg_t treg)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG05, SGM41513_TREG,
                  (treg == SGM41513_TREG_120C) ? SGM41513_TREG : 0u);
    DRV_UNLOCK();
    return res;
}

/* ---------------------------------------------------------------------- */
/* 8. Ship mode / BATFET                                                   */
/* ---------------------------------------------------------------------- */

#if SGM41513_USE_SHIP
sgm41513_result_t sgm41513_enter_ship_mode(sgm41513_dev_t *dev, bool delayed)
{
    sgm41513_result_t res;

    CHK_DEV(dev);

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG07, SGM41513_BATFET_DLY,
                  delayed ? SGM41513_BATFET_DLY : 0u);
    if (res == SGM41513_OK) {
        res = sgm_rmw(dev, SGM41513_REG07, SGM41513_BATFET_DIS,
                      SGM41513_BATFET_DIS);
    }
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_batfet_reset_enable(sgm41513_dev_t *dev,
                                                   bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG07, SGM41513_BATFET_RST_EN,
                  enable ? SGM41513_BATFET_RST_EN : 0u);
    DRV_UNLOCK();
    return res;
}
#endif /* SGM41513_USE_SHIP */

/* ---------------------------------------------------------------------- */
/* 9. PUMPX                                                                */
/* ---------------------------------------------------------------------- */

#if SGM41513_USE_PUMPX
sgm41513_result_t sgm41513_pumpx_enable(sgm41513_dev_t *dev, bool enable)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0D, SGM41513_EN_PUMPX,
                  enable ? SGM41513_EN_PUMPX : 0u);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_pumpx_trigger_up(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0D, SGM41513_PUMPX_UP, SGM41513_PUMPX_UP);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_pumpx_trigger_down(sgm41513_dev_t *dev)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0D, SGM41513_PUMPX_DN, SGM41513_PUMPX_DN);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_pumpx_busy(sgm41513_dev_t *dev, bool *busy)
{
    sgm41513_result_t res;
    uint8_t v;

    CHK_DEV(dev);
    if (busy == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, SGM41513_REG0D, &v);
    DRV_UNLOCK();

    if (res == SGM41513_OK) {
        /* PUMPX_UP/DN self-clear when the pulse sequence completes.    */
        *busy = ((v & (SGM41513_PUMPX_UP | SGM41513_PUMPX_DN)) != 0u);
    }
    return res;
}
#endif /* SGM41513_USE_PUMPX */

/* ---------------------------------------------------------------------- */
/* 10. STAT pin / D+ D- lines                                              */
/* ---------------------------------------------------------------------- */

sgm41513_result_t sgm41513_set_stat_pin_mode(sgm41513_dev_t *dev,
                                             sgm41513_stat_pin_mode_t mode)
{
    sgm41513_result_t res;
    uint8_t val;

    CHK_DEV(dev);
    switch (mode) {
    case SGM41513_STAT_PIN_CHARGE: val = SGM41513_EN_ICHG_MON_CHARGE;  break;
    case SGM41513_STAT_PIN_MANUAL: val = SGM41513_EN_ICHG_MON_STATSET; break;
    default:                       val = SGM41513_EN_ICHG_MON_DISABLE; break;
    }

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG00, SGM41513_EN_ICHG_MON_MASK, val);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_stat_pin_pattern(sgm41513_dev_t *dev,
                                                sgm41513_stat_pattern_t p)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0F, SGM41513_STAT_SET_MASK,
                  (uint8_t)(((uint8_t)p << SGM41513_STAT_SET_SHIFT)
                            & SGM41513_STAT_SET_MASK));
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_set_dpdm_voltage(sgm41513_dev_t *dev,
                                            sgm41513_dpdm_vset_t dp,
                                            sgm41513_dpdm_vset_t dm)
{
    sgm41513_result_t res;
    uint8_t val;

    CHK_DEV(dev);
    val = (uint8_t)((((uint8_t)dp & 0x03u) << SGM41513_DP_VSET_SHIFT)
                    | (((uint8_t)dm & 0x03u) << SGM41513_DM_VSET_SHIFT));

    DRV_LOCK();
    res = sgm_rmw(dev, SGM41513_REG0D,
                  SGM41513_DP_VSET_MASK | SGM41513_DM_VSET_MASK, val);
    DRV_UNLOCK();
    return res;
}

/* ---------------------------------------------------------------------- */
/* 11. Register-level access                                               */
/* ---------------------------------------------------------------------- */

sgm41513_result_t sgm41513_read_reg(sgm41513_dev_t *dev, uint8_t reg,
                                    uint8_t *val)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    if (val == NULL) {
        return SGM41513_ERR_PARAM;
    }
    DRV_LOCK();
    res = sgm_rd(dev, reg, val);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_write_reg(sgm41513_dev_t *dev, uint8_t reg,
                                     uint8_t val)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_wr(dev, reg, val);
    DRV_UNLOCK();
    return res;
}

sgm41513_result_t sgm41513_update_bits(sgm41513_dev_t *dev, uint8_t reg,
                                       uint8_t mask, uint8_t val)
{
    sgm41513_result_t res;
    CHK_DEV(dev);
    DRV_LOCK();
    res = sgm_rmw(dev, reg, mask, val);
    DRV_UNLOCK();
    return res;
}
