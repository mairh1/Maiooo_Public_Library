/*
 * sgm41513.h - Universal driver for the SGM41513 battery charger family.
 *
 * Device   : SGM41513 / SGM41513A / SGM41513D (SG Micro Corp.)
 *            3.9-13.5 V input, 3 A single-cell Li-Ion charger with NVDC
 *            power path, I2C interface (7-bit address 0x1A, 400 kHz),
 *            OTG reverse boost, JEITA, ship mode.
 * Datasheet: SGM41513_SGM41513A_SGM41513D, APRIL 2025 REV. C.1
 *
 * Architecture (layered design):
 *
 *   +--------------------------------------+
 *   |  Application                         |   your code
 *   +--------------------------------------+
 *              |  this API (sgm41513.h)
 *   +--------------------------------------+
 *   |  Driver core  (sgm41513.c)           |   portable, pure C99,
 *   |  config      (sgm41513_conf.h)       |   no dynamic memory
 *   +--------------------------------------+
 *              |  4 io functions (sgm41513_io.h)
 *   +--------------------------------------+
 *   |  Porting layer (you provide)         |   STM32 / ESP-IDF / Linux /
 *   |                                      |   RTOS / bit-banged I2C ...
 *   +--------------------------------------+
 *
 * Porting = implement sgm41513_io.h and adjust sgm41513_conf.h. Nothing
 * else in the driver touches the hardware.
 *
 * Units used throughout the API: current in mA (uint32_t), voltage in mV
 * (uint32_t). Every set function rounds to the nearest hardware step and
 * clamps to the supported range; the matching get function returns the
 * actually effective value.
 */

#ifndef SGM41513_H
#define SGM41513_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "sgm41513_conf.h"
#include "sgm41513_regs.h"   /* register count + addresses (also enables
                                register-level access for advanced users) */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* Result codes (common to every public API)                              */
/* ---------------------------------------------------------------------- */

typedef enum {
    SGM41513_OK = 0,            /* no error                                  */
    SGM41513_ERR_IO,            /* I2C communication failure (porting layer) */
    SGM41513_ERR_PARAM,         /* invalid argument (NULL pointer, ...)      */
    SGM41513_ERR_NOT_READY,     /* chip not responding / unexpected ID       */
    SGM41513_ERR_NOT_SUPPORTED, /* feature disabled in sgm41513_conf.h       */
    SGM41513_ERR_VERIFY,        /* read-back mismatch (SGM41513_VERIFY_WRITES)*/
} sgm41513_result_t;

/* ---------------------------------------------------------------------- */
/* Device handle - one per charger chip                                    */
/* ---------------------------------------------------------------------- */

typedef struct {
    void    *io_ctx;    /* opaque pointer forwarded to the io functions,
                           use it as bus handle for multi-bus designs,
                           NULL for single-bus systems                    */
    uint8_t dev_addr;   /* 7-bit I2C address, usually SGM41513_I2C_ADDR   */
#if SGM41513_REG_SHADOW
    uint8_t shadow[SGM41513_NUM_REGS]; /* cached R/W register images      */
#endif
    uint8_t variant;    /* detected variant, see sgm41513_variant_t       */
    uint8_t inited;     /* set by sgm41513_init()                         */
} sgm41513_dev_t;

typedef enum {
    SGM41513_CHIP_SGM41513  = 0,
    SGM41513_CHIP_SGM41513A = 1,
    SGM41513_CHIP_SGM41513D = 2,
} sgm41513_variant_t;
/* Note: SGM41513_VARIANT_SGM41513/A/D (without CHIP) are the compile-time
 * selection macros from sgm41513_conf.h - do not confuse the two.        */

/* Part identification (REG0B). Note: PN bits cannot distinguish the A    */
/* from the D variant; 'variant' resolves that using SGM41513_VARIANT.    */
typedef struct {
    uint8_t pn_raw;             /* raw PN[3:0]: 0 = SGM41513, 1 = A or D  */
    sgm41513_variant_t variant; /* best-known variant of this chip        */
    uint8_t dev_rev;            /* DEV_REV[1:0] silicon revision          */
} sgm41513_id_t;

/* ---------------------------------------------------------------------- */
/* Status types (REG08 / REG09 / REG0A)                                    */
/* ---------------------------------------------------------------------- */

/* VBUS / input source type, decoded from VBUS_STAT[2:0] per variant      */
typedef enum {
    SGM41513_VBUS_NONE = 0,       /* 000: no input                        */
    SGM41513_VBUS_USB_SDP = 1,    /* 001: USB host SDP (500 mA)           */
    SGM41513_VBUS_USB_CDP = 2,    /* 010: USB CDP 1.5 A (A/D variants)    */
    SGM41513_VBUS_USB_DCP = 3,    /* 011: USB DCP 2.4 A (A/D variants)    */
    SGM41513_VBUS_UNKNOWN = 5,    /* 101: unknown adapter 500 mA (A/D)    */
    SGM41513_VBUS_NONSTD = 6,     /* 110: non-standard adapter (A/D)      */
    SGM41513_VBUS_OTG = 7,        /* 111: OTG boost active                */
    /* SGM41513 (PSEL variant) only: 010 means "adapter 2.4 A, PSEL low"  */
    SGM41513_VBUS_ADAPTER_PSEL = 9,
    SGM41513_VBUS_RESERVED = 10,  /* any encoding not listed above        */
} sgm41513_vbus_type_t;

/* Charging phase, decoded from CHRG_STAT[1:0]                            */
typedef enum {
    SGM41513_CHRG_OFF = 0,        /* not charging                         */
    SGM41513_CHRG_PRECHARGE = 1,  /* trickle / pre-charge phase           */
    SGM41513_CHRG_FAST = 2,       /* fast charge (CC or CV)               */
    SGM41513_CHRG_DONE = 3,       /* charge terminated                    */
} sgm41513_charge_status_t;

typedef struct {
    sgm41513_vbus_type_t vbus_type;    /* decoded input source type       */
    uint8_t vbus_raw;                  /* raw VBUS_STAT[2:0] code         */
    sgm41513_charge_status_t charge_status; /* charging phase             */
    bool power_good;                   /* PG_STAT: input power good       */
    bool thermal_regulating;           /* THERM_STAT: in thermal reg.     */
    bool vsys_regulating;              /* VSYS_STAT: in VSYS_MIN reg.     */
} sgm41513_status_t;

typedef enum {
    SGM41513_CHRG_FAULT_NONE = 0,
    SGM41513_CHRG_FAULT_INPUT = 1,      /* VAC OVP or VBAT<VVBUS<3.8 V   */
    SGM41513_CHRG_FAULT_THERMAL_SD = 2, /* thermal shutdown              */
    SGM41513_CHRG_FAULT_TIMER = 3,      /* charge safety timer expired   */
} sgm41513_charge_fault_t;

typedef enum {
    SGM41513_NTC_NORMAL = 0,
    SGM41513_NTC_WARM = 2,   /* buck only, charging continues at reduced V */
    SGM41513_NTC_COOL = 3,   /* buck only, charging continues at reduced I */
    SGM41513_NTC_COLD = 5,   /* charging suspended                          */
    SGM41513_NTC_HOT = 6,    /* charging suspended                          */
    SGM41513_NTC_UNKNOWN = 7,
} sgm41513_ntc_fault_t;

typedef struct {
    bool watchdog_fault;              /* I2C watchdog expired (latched)   */
    bool boost_fault;                 /* boost could not run / overload   */
    sgm41513_charge_fault_t charge_fault;
    bool battery_ovp;                 /* battery over-voltage (BATOVP)    */
    sgm41513_ntc_fault_t ntc_fault;   /* real-time thermistor status      */
    uint8_t raw;                      /* raw REG09 of the second read     */
} sgm41513_faults_t;

typedef struct {
    bool vbus_good;      /* VBUS_GD: good VBUS attached                    */
    bool vindpm_active;  /* input voltage regulation loop active          */
    bool iindpm_active;  /* input current regulation loop active          */
    bool topoff_active;  /* top-off timer counting                        */
    bool input_ovp;      /* ACOV_STAT: input over-voltage event           */
} sgm41513_dpm_status_t;

/* ---------------------------------------------------------------------- */
/* Setting enumerations                                                    */
/* ---------------------------------------------------------------------- */

typedef enum {
    SGM41513_VINDPM_OS_3900MV = 0,   /* for  5 V adapters                 */
    SGM41513_VINDPM_OS_5900MV = 1,   /* for  9 V adapters                 */
    SGM41513_VINDPM_OS_7500MV = 2,   /* for  9 V adapters                 */
    SGM41513_VINDPM_OS_10500MV = 3,  /* for 12 V adapters                 */
} sgm41513_vindpm_os_t;

typedef enum {
    SGM41513_BAT_TRACK_OFF = 0,
    SGM41513_BAT_TRACK_200MV = 1,    /* VINDPM = VBAT + 200 mV            */
    SGM41513_BAT_TRACK_250MV = 2,
    SGM41513_BAT_TRACK_300MV = 3,
} sgm41513_bat_track_t;              /* only effective when OS = 3900 mV  */

typedef enum {
    SGM41513_INPUT_OVP_5500MV = 0,
    SGM41513_INPUT_OVP_6500MV = 1,   /* 5 V input                         */
    SGM41513_INPUT_OVP_10500MV = 2,  /* 9 V input                         */
    SGM41513_INPUT_OVP_14000MV = 3,  /* 12 V input (POR default)          */
} sgm41513_input_ovp_t;

typedef enum {
    SGM41513_BOOST_V_4850MV = 0,
    SGM41513_BOOST_V_5000MV = 1,
    SGM41513_BOOST_V_5150MV = 2,     /* POR default                       */
    SGM41513_BOOST_V_5300MV = 3,
} sgm41513_boost_volt_t;

typedef enum {
    SGM41513_BOOST_LIM_500MA = 0,
    SGM41513_BOOST_LIM_1200MA = 1,   /* POR default                       */
} sgm41513_boost_lim_t;

typedef enum {
    SGM41513_BOOST_FREQ_500KHZ = 0,
    SGM41513_BOOST_FREQ_1500KHZ = 1, /* POR default                       */
} sgm41513_boost_freq_t;

typedef enum {
    SGM41513_WDT_OFF = 0,
    SGM41513_WDT_40S = 1,
    SGM41513_WDT_80S = 2,
    SGM41513_WDT_160S = 3,           /* POR default                       */
} sgm41513_watchdog_t;

typedef enum {
    SGM41513_SAFETY_TIMER_7H = 0,
    SGM41513_SAFETY_TIMER_16H = 1,   /* POR default                       */
} sgm41513_safety_timer_t;

typedef enum {
    SGM41513_TREG_80C = 0,
    SGM41513_TREG_120C = 1,          /* POR default                       */
} sgm41513_treg_t;

typedef enum {
    SGM41513_TOPOFF_OFF = 0,         /* POR default                       */
    SGM41513_TOPOFF_15MIN = 1,
    SGM41513_TOPOFF_30MIN = 2,
    SGM41513_TOPOFF_45MIN = 3,
} sgm41513_topoff_t;

typedef enum {
    SGM41513_VREG_FT_OFF = 0,        /* POR default                       */
    SGM41513_VREG_FT_PLUS_8MV = 1,
    SGM41513_VREG_FT_MINUS_8MV = 2,
    SGM41513_VREG_FT_MINUS_16MV = 3,
} sgm41513_vreg_ft_t;

typedef enum {
    SGM41513_VRECHG_100MV = 0,       /* POR default                       */
    SGM41513_VRECHG_200MV = 1,
} sgm41513_vrechg_t;

typedef enum {
    SGM41513_TRICKLE_90MA = 0,       /* POR default                       */
    SGM41513_TRICKLE_30MA = 1,
} sgm41513_trickle_t;

typedef enum {
    SGM41513_ITERM_DEGLITCH_230MS = 0, /* POR default                     */
    SGM41513_ITERM_DEGLITCH_16MS = 1,
} sgm41513_term_deglitch_t;

typedef enum {
    SGM41513_MIN_VBAT_OTG_2950MV = 0, /* POR default                      */
    SGM41513_MIN_VBAT_OTG_2600MV = 1,
} sgm41513_min_vbat_otg_t;

typedef enum {
    SGM41513_STAT_PIN_CHARGE = 0,    /* STAT follows the charge state     */
    SGM41513_STAT_PIN_MANUAL = 1,    /* STAT follows STAT_SET pattern     */
    SGM41513_STAT_PIN_DISABLE = 2,   /* STAT pin floating                 */
} sgm41513_stat_pin_mode_t;

typedef enum {
    SGM41513_STAT_PATTERN_OFF = 0,   /* LED off                           */
    SGM41513_STAT_PATTERN_ON = 1,    /* LED on                            */
    SGM41513_STAT_PATTERN_BLINK_1S = 2,  /* 1 s on / 1 s off              */
    SGM41513_STAT_PATTERN_BLINK_3S = 3,  /* 1 s on / 3 s off              */
} sgm41513_stat_pattern_t;

typedef enum {
    SGM41513_DPDM_HIZ = 0,           /* POR default                       */
    SGM41513_DPDM_0V = 1,
    SGM41513_DPDM_0V6 = 2,
    SGM41513_DPDM_3V3 = 3,
} sgm41513_dpdm_vset_t;

typedef enum {
    SGM41513_JEITA_VT2_5_5C = 0,     /* cool threshold T2                 */
    SGM41513_JEITA_VT2_10C = 1,      /* POR default                       */
    SGM41513_JEITA_VT2_15C = 2,
    SGM41513_JEITA_VT2_20C = 3,
} sgm41513_jeita_vt2_t;

typedef enum {
    SGM41513_JEITA_VT3_40C = 0,      /* warm threshold T3                 */
    SGM41513_JEITA_VT3_44_5C = 1,    /* POR default                       */
    SGM41513_JEITA_VT3_50_5C = 2,
    SGM41513_JEITA_VT3_54_5C = 3,
} sgm41513_jeita_vt3_t;

typedef enum {
    SGM41513_JEITA_COOL_I_50PCT = 0,
    SGM41513_JEITA_COOL_I_20PCT = 1, /* POR default                       */
} sgm41513_jeita_cool_i_t;

typedef enum {
    SGM41513_JEITA_WARM_I_0PCT = 0,
    SGM41513_JEITA_WARM_I_20PCT = 1,
    SGM41513_JEITA_WARM_I_50PCT = 2,
    SGM41513_JEITA_WARM_I_100PCT = 3, /* POR default                      */
} sgm41513_jeita_warm_i_t;

/* JEITA configuration written by sgm41513_jeita_configure()              */
typedef struct {
    sgm41513_jeita_vt2_t cool_threshold;    /* T2: 0-10C region boundary  */
    sgm41513_jeita_vt3_t warm_threshold;    /* T3: 45-60C region boundary */
    bool  cool_charge_enable;    /* charge in cool region (JEITA_ISET_L_EN)*/
    bool  cool_voltage_4v1;      /* true: min(VREG, 4.1 V) in cool region */
    sgm41513_jeita_cool_i_t cool_current;   /* cool region current        */
    bool  warm_voltage_use_vreg; /* true: VREG in warm region,
                                    false: min(VREG, 4.1 V)               */
    sgm41513_jeita_warm_i_t warm_current;   /* warm region current        */
} sgm41513_jeita_cfg_t;

/* ====================================================================== */
/* API                                                                     */
/* ====================================================================== */

/* ---- 1. Initialization / identification ------------------------------- */

/*
 * Initialize the device handle and verify the chip responds.
 * - Calls sgm41513_io_init() once.
 * - Reads REG0B and validates the part (SGMPART = 0, PN = 0 or 1).
 * - Detects the variant: PN 0000 -> SGM41513, PN 0001 -> A or D
 *   (resolved to SGM41513_VARIANT from sgm41513_conf.h).
 * - With SGM41513_REG_SHADOW = 1 the R/W register images are loaded
 *   from the chip so restore_settings() can replay them later.
 *
 * NOTE: writing any register afterwards puts the chip into I2C "host
 * mode": you must then either feed the watchdog periodically
 * (sgm41513_feed_watchdog()) or disable it (sgm41513_set_watchdog()).
 */
sgm41513_result_t sgm41513_init(sgm41513_dev_t *dev, void *io_ctx,
                                uint8_t dev_addr);

/* Read and decode the part identification (REG0B). */
sgm41513_result_t sgm41513_get_id(sgm41513_dev_t *dev, sgm41513_id_t *id);

/*
 * Reset all R/W registers to their power-on defaults (REG0B REG_RST).
 * The shadow cache (if enabled) is reset to the POR values as well.
 * The chip stays in its default autonomous charging mode afterwards.
 */
sgm41513_result_t sgm41513_reset(sgm41513_dev_t *dev);

#if SGM41513_REG_SHADOW
/*
 * Re-apply every R/W register previously written through this driver.
 * Call it after the watchdog expired (the chip resets its registers) or
 * after the device left ship mode (EN_HIZ is then forced to 1).
 */
sgm41513_result_t sgm41513_restore_settings(sgm41513_dev_t *dev);
#endif

/* ---- 2. Charging control (REG01/02/03/04/05/0F) ------------------------ */

/* Master charge switch (REG01 CHG_CONFIG). The physical nCE pin must    */
/* also be pulled low for charging to actually run. Setting ICHG to 0    */
/* disables charging as well.                                            */
sgm41513_result_t sgm41513_charge_enable(sgm41513_dev_t *dev, bool enable);

/* Fast charge current, 0..3000 mA (non-linear 64-step table).          */
sgm41513_result_t sgm41513_set_ichg(sgm41513_dev_t *dev, uint32_t ma);
sgm41513_result_t sgm41513_get_ichg(sgm41513_dev_t *dev, uint32_t *ma);

/* Battery regulation voltage, 3856..4624 mV (32 mV steps, code 15 is a */
/* 4350 mV special case). Fine adjustment via sgm41513_set_vreg_ft().   */
sgm41513_result_t sgm41513_set_vreg(sgm41513_dev_t *dev, uint32_t mv);
sgm41513_result_t sgm41513_get_vreg(sgm41513_dev_t *dev, uint32_t *mv);

/* VREG fine tune: 0 / +8 / -8 / -16 mV on top of VREG.                 */
sgm41513_result_t sgm41513_set_vreg_ft(sgm41513_dev_t *dev,
                                       sgm41513_vreg_ft_t ft);

/* Pre-charge current, 5..240 mA (16-step table).                      */
sgm41513_result_t sgm41513_set_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t ma);
sgm41513_result_t sgm41513_get_precharge_current(sgm41513_dev_t *dev,
                                                 uint32_t *ma);

/* Charge termination current, 5..240 mA (16-step table).               */
sgm41513_result_t sgm41513_set_term_current(sgm41513_dev_t *dev,
                                            uint32_t ma);
sgm41513_result_t sgm41513_get_term_current(sgm41513_dev_t *dev,
                                            uint32_t *ma);

/* Enable / disable current-based charge termination (REG05 EN_TERM).   */
sgm41513_result_t sgm41513_set_term_enable(sgm41513_dev_t *dev, bool enable);

/* Termination detection deglitch time: 230 ms (default) or 16 ms.      */
sgm41513_result_t sgm41513_set_term_deglitch(sgm41513_dev_t *dev,
                                             sgm41513_term_deglitch_t t);

/* Recharge threshold below VREG: 100 mV (default) or 200 mV.           */
sgm41513_result_t sgm41513_set_recharge_threshold(sgm41513_dev_t *dev,
                                                  sgm41513_vrechg_t mv);

/* Optional top-off extension timer after termination.                  */
sgm41513_result_t sgm41513_set_topoff_timer(sgm41513_dev_t *dev,
                                            sgm41513_topoff_t t);

/* Trickle current for deeply discharged cells (VBAT < ~2.2 V).         */
sgm41513_result_t sgm41513_set_trickle_current(sgm41513_dev_t *dev,
                                               sgm41513_trickle_t ma);

/* Minimum system voltage, 2600..3700 mV (NVDC, REG01 SYS_MIN).         */
sgm41513_result_t sgm41513_set_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t mv);
sgm41513_result_t sgm41513_get_sys_min_voltage(sgm41513_dev_t *dev,
                                               uint32_t *mv);

/* Minimum battery voltage to start OTG boost (REG01 MIN_BAT_SEL).      */
sgm41513_result_t sgm41513_set_min_vbat_otg(sgm41513_dev_t *dev,
                                            sgm41513_min_vbat_otg_t sel);

/* ---- 3. Input management (REG00/06/07/0F) ------------------------------ */

/*
 * Input current limit (IINDPM), 100..3200 mA in 100 mA steps.
 * WARNING: the hardware overwrites this register automatically after
 * input source detection (PSEL / D+/D- BC1.2). Re-apply your value once
 * sgm41513_input_detect_done() reports true.
 */
sgm41513_result_t sgm41513_set_iindpm(sgm41513_dev_t *dev, uint32_t ma);
sgm41513_result_t sgm41513_get_iindpm(sgm41513_dev_t *dev, uint32_t *ma);

/*
 * VINDPM offset range selection. Each offset spans 16 steps of 100 mV:
 *   3900 mV -> 3.9 .. 5.4 V    5900 mV -> 5.9 .. 7.4 V
 *   7500 mV -> 7.5 .. 9.0 V   10500 mV -> 10.5 .. 12.0 V
 * VBAT tracking is only effective with the 3900 mV offset.
 */
sgm41513_result_t sgm41513_set_vindpm_os(sgm41513_dev_t *dev,
                                         sgm41513_vindpm_os_t os);

/*
 * Input voltage regulation threshold (VINDPM), absolute mV. The value is
 * converted using the currently selected VINDPM_OS offset and clamped to
 * that offset's 1.5 V wide window; call set_vindpm_os() first if needed.
 */
sgm41513_result_t sgm41513_set_vindpm(sgm41513_dev_t *dev, uint32_t mv);
sgm41513_result_t sgm41513_get_vindpm(sgm41513_dev_t *dev, uint32_t *mv);

/* Track VINDPM to VBAT + 200/250/300 mV (needs OS = 3900 mV).          */
sgm41513_result_t sgm41513_set_vindpm_bat_track(sgm41513_dev_t *dev,
                                                sgm41513_bat_track_t track);

/* Input over-voltage protection threshold.                             */
sgm41513_result_t sgm41513_set_input_ovp(sgm41513_dev_t *dev,
                                         sgm41513_input_ovp_t ovp);

/*
 * High-impedance mode: disconnect VBUS from the internal circuitry
 * ( converter stops, ~8.5 uA battery drain). Also clears automatically
 * as part of sgm41513_otg_enable().
 */
sgm41513_result_t sgm41513_set_hiz(sgm41513_dev_t *dev, bool enable);

/* Force a new input source detection cycle (REG07 IINDET_EN).          */
sgm41513_result_t sgm41513_force_input_detection(sgm41513_dev_t *dev);

/* True once PSEL / D+/D- detection finished after VBUS plug-in.        */
sgm41513_result_t sgm41513_input_detect_done(sgm41513_dev_t *dev,
                                             bool *done);

/* Buck converter light-load PFM mode (default on = better light-load
 * efficiency; disable for lowest ripple).                              */
sgm41513_result_t sgm41513_set_pfm_enable(sgm41513_dev_t *dev, bool enable);

/* Keep the input FET (Q1) fully on even at low IINDPM settings
 * (better efficiency, slightly worse current-sense accuracy).          */
sgm41513_result_t sgm41513_set_q1_fullon(sgm41513_dev_t *dev, bool enable);

/* Mask (true) or unmask the VINDPM / IINDPM nINT pulses (REG0A).       */
sgm41513_result_t sgm41513_set_dpm_int_mask(sgm41513_dev_t *dev,
                                            bool mask_vindpm,
                                            bool mask_iindpm);

/* ---- 4. OTG reverse boost (REG01/02/06/0D) ----------------------------- */

#if SGM41513_USE_OTG
/*
 * Enable / disable the OTG reverse boost. Enabling automatically clears
 * EN_HIZ first and waits >= 30 ms (datasheet requirement) before setting
 * OTG_CONFIG. Boost prerequisites: VBAT above the MIN_VBAT_OTG gate,
 * VBUS below VBAT + VSLEEP, TS within the boost temperature window.
 * OTG has priority over charging; HIZ has priority over OTG.
 */
sgm41513_result_t sgm41513_otg_enable(sgm41513_dev_t *dev, bool enable);

/* Boost regulation voltage (VBUS while OTG active).                    */
sgm41513_result_t sgm41513_set_boost_voltage(sgm41513_dev_t *dev,
                                             sgm41513_boost_volt_t volt);

/* Boost current limit: 500 mA or 1200 mA.                              */
sgm41513_result_t sgm41513_set_boost_current_limit(sgm41513_dev_t *dev,
                                                   sgm41513_boost_lim_t lim);

/* Boost switching frequency: 1.5 MHz (default) or 500 kHz.             */
sgm41513_result_t sgm41513_set_boost_freq(sgm41513_dev_t *dev,
                                          sgm41513_boost_freq_t freq);

/*
 * ITERM scaling (REG0D OTGF_ITREMR, charge-mode meaning of the same
 * bit as the boost frequency): enable = true scales the programmed ITERM
 * by 6 when ICHG > 300 mA (useful for large-cell termination accuracy).
 */
sgm41513_result_t sgm41513_set_iterm_x6(sgm41513_dev_t *dev, bool enable);
#endif /* SGM41513_USE_OTG */

/* ---- 5. Status and faults (REG08/09/0A) --------------------------------- */

/*
 * Read the operating status (REG08): input source type, charging phase,
 * power good, thermal regulation, VSYS_MIN regulation.
 */
sgm41513_result_t sgm41513_get_status(sgm41513_dev_t *dev,
                                      sgm41513_status_t *status);

/*
 * Read live fault flags (REG09). The hardware latches fault bits until
 * read; this function therefore reads REG09 TWICE and decodes the second
 * value, so the returned flags are the current ones. NTC_FAULT is real
 * time and taken as-is. Reading also clears the latched history.
 */
sgm41513_result_t sgm41513_get_faults(sgm41513_dev_t *dev,
                                      sgm41513_faults_t *faults);

/*
 * Read the power-path / DPM status (REG0A): VBUS good, VINDPM / IINDPM
 * loops active, top-off counting, input OVP event.
 */
sgm41513_result_t sgm41513_get_dpm_status(sgm41513_dev_t *dev,
                                          sgm41513_dpm_status_t *dpm);

/* ---- 6. JEITA (REG05/07/0C) --------------------------------------------- */

#if SGM41513_USE_JEITA
/*
 * Program the complete JEITA temperature profile (TS pin NTC divider).
 * Charging is suspended outside the T1..T4 hardware window (0..60 C);
 * this call configures the cool (T2) and warm (T3) inner thresholds and
 * the voltage/current derating applied inside those regions.
 */
sgm41513_result_t sgm41513_jeita_configure(sgm41513_dev_t *dev,
                                           const sgm41513_jeita_cfg_t *cfg);
#endif /* SGM41513_USE_JEITA */

/* ---- 7. Watchdog / timers / thermal (REG05/07) --------------------------- */

/* I2C watchdog period; disabled, 40 s, 80 s or 160 s (POR default).    */
sgm41513_result_t sgm41513_set_watchdog(sgm41513_dev_t *dev,
                                        sgm41513_watchdog_t wdt);

/* Kick the watchdog (REG01 WD_RST, write-1 self-clearing).             */
sgm41513_result_t sgm41513_feed_watchdog(sgm41513_dev_t *dev);

/* Charge safety timer: 7 h or 16 h, enable / disable. When TMR2X is
 * set the timer runs at half rate during DPM / JEITA-cool / thermal
 * regulation (see sgm41513_set_safety_timer_slow2x).                   */
sgm41513_result_t sgm41513_set_safety_timer(sgm41513_dev_t *dev,
                                            sgm41513_safety_timer_t hours,
                                            bool enable);
sgm41513_result_t sgm41513_set_safety_timer_slow2x(sgm41513_dev_t *dev,
                                                   bool enable);

/* Buck thermal regulation threshold: 80 C or 120 C.                    */
sgm41513_result_t sgm41513_set_thermal_reg_threshold(sgm41513_dev_t *dev,
                                                     sgm41513_treg_t treg);

/* ---- 8. Ship mode / BATFET (REG07) --------------------------------------- */

#if SGM41513_USE_SHIP
/*
 * Enter ship mode (BATFET off, ~2.5 uA battery drain). 'delayed' = true
 * waits tSM_DLY (~12 s) before disconnecting the battery. Exit by
 * applying VBUS, pulling nQON low >= 1 s, REG_RST, or clearing BATFET_DIS
 * (the driver's restore_settings() re-applies your configuration and
 * clears the HIZ state forced on ship-mode exit).
 */
sgm41513_result_t sgm41513_enter_ship_mode(sgm41513_dev_t *dev,
                                           bool delayed);

/* Enable / disable the nQON long-press (>= 10 s) BATFET reset feature. */
sgm41513_result_t sgm41513_set_batfet_reset_enable(sgm41513_dev_t *dev,
                                                   bool enable);
#endif /* SGM41513_USE_SHIP */

/* ---- 9. PUMPX (REG0D) ---------------------------------------------------- */

#if SGM41513_USE_PUMPX
/*
 * PUMPX voltage-step protocol for adjustable adapters (e.g. some QC
 * class adapters): triggers current pulses on VBUS to ask the adapter
 * for a higher (up) or lower (down) output voltage. Enable first, then
 * trigger; use sgm41513_pumpx_busy() to wait for completion.
 */
sgm41513_result_t sgm41513_pumpx_enable(sgm41513_dev_t *dev, bool enable);
sgm41513_result_t sgm41513_pumpx_trigger_up(sgm41513_dev_t *dev);
sgm41513_result_t sgm41513_pumpx_trigger_down(sgm41513_dev_t *dev);
sgm41513_result_t sgm41513_pumpx_busy(sgm41513_dev_t *dev, bool *busy);
#endif /* SGM41513_USE_PUMPX */

/* ---- 10. STAT pin / D+ D- lines (REG00/0D/0F) ----------------------------- */

/* STAT open-drain output function (LED driver or host GPIO).           */
sgm41513_result_t sgm41513_set_stat_pin_mode(sgm41513_dev_t *dev,
                                             sgm41513_stat_pin_mode_t mode);
sgm41513_result_t sgm41513_set_stat_pin_pattern(sgm41513_dev_t *dev,
                                                sgm41513_stat_pattern_t p);

/*
 * Manually drive D+ / D- voltages (A/D variants, e.g. for divider-type
 * adapter detection). Values reset on VBUS plug-in; only effective after
 * input detection has completed.
 */
sgm41513_result_t sgm41513_set_dpdm_voltage(sgm41513_dev_t *dev,
                                            sgm41513_dpdm_vset_t dp,
                                            sgm41513_dpdm_vset_t dm);

/* ---- 11. Register-level access -------------------------------------------- */

/* Raw single-register access for debugging / uncovered features.       */
/* update_bits() performs read-modify-write with (val & mask).          */
sgm41513_result_t sgm41513_read_reg(sgm41513_dev_t *dev, uint8_t reg,
                                    uint8_t *val);
sgm41513_result_t sgm41513_write_reg(sgm41513_dev_t *dev, uint8_t reg,
                                     uint8_t val);
sgm41513_result_t sgm41513_update_bits(sgm41513_dev_t *dev, uint8_t reg,
                                       uint8_t mask, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_H */
