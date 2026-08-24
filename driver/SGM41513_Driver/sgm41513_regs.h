/*
 * sgm41513_regs.h - Register and bit-field definitions for the SGM41513 family.
 *
 * Device : SGM41513 / SGM41513A / SGM41513D (SG Micro Corp.)
 * Datasheet : SGM41513_SGM41513A_SGM41513D, APRIL 2025 REV. C.1
 *
 * All registers are 8-bit. 16 registers total (0x00 - 0x0F). Reading beyond
 * 0x0F returns 0xFF. REG09 and REG0E must NOT be accessed with multi-byte
 * (burst) I2C transfers - the driver therefore only ever performs single
 * byte read/write operations.
 *
 * This header is internal to the driver but exposed so that advanced users
 * can perform register-level access through sgm41513_read_reg() /
 * sgm41513_write_reg() / sgm41513_update_bits().
 *
 * NOTE: normally you should not need to touch anything in this file.
 */

#ifndef SGM41513_REGS_H
#define SGM41513_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* Device constants                                                        */
/* ---------------------------------------------------------------------- */

#define SGM41513_NUM_REGS           16u     /* REG00 .. REG0F              */
#define SGM41513_I2C_ADDR           0x1Au   /* 7-bit slave address         */
#define SGM41513_I2C_MAX_FREQ_HZ    400000u /* fast mode                   */

/* ---------------------------------------------------------------------- */
/* Register addresses                                                      */
/* ---------------------------------------------------------------------- */

#define SGM41513_REG00  0x00u /* EN_HIZ, EN_ICHG_MON, IINDPM[4:0]          */
#define SGM41513_REG01  0x01u /* PFM_DIS, WD_RST, OTG_CONFIG, CHG_CONFIG,
                                 SYS_MIN[2:0], MIN_BAT_SEL                */
#define SGM41513_REG02  0x02u /* BOOST_LIM, Q1_FULLON, ICHG[5:0]          */
#define SGM41513_REG03  0x03u /* IPRECHG[3:0], ITERM[3:0]                 */
#define SGM41513_REG04  0x04u /* VREG[4:0], TOPOFF_TIMER[1:0], VRECHG     */
#define SGM41513_REG05  0x05u /* EN_TERM, ITERM_TIMER, WATCHDOG[1:0],
                                 EN_TIMER, CHG_TIMER, TREG, JEITA_ISET_L  */
#define SGM41513_REG06  0x06u /* OVP[1:0], BOOSTV[1:0], VINDPM[3:0]       */
#define SGM41513_REG07  0x07u /* IINDET_EN, TMR2X_EN, BATFET_DIS,
                                 JEITA_VSET_H, BATFET_DLY, BATFET_RST_EN,
                                 VDPM_BAT_TRACK[1:0]                      */
#define SGM41513_REG08  0x08u /* (RO) VBUS_STAT, CHRG_STAT, PG_STAT,
                                 THERM_STAT, VSYS_STAT                    */
#define SGM41513_REG09  0x09u /* (RO) fault flags, latched until read,
                                 NTC_FAULT is real time. NO burst access. */
#define SGM41513_REG0A  0x0Au /* VBUS_GD, VINDPM_STAT, IINDPM_STAT,
                                 TOPOFF_ACTIVE, ACOV_STAT, INT masks      */
#define SGM41513_REG0B  0x0Bu /* REG_RST (W), PN[3:0], SGMPART, DEV_REV   */
#define SGM41513_REG0C  0x0Cu /* JEITA configuration                      */
#define SGM41513_REG0D  0x0Du /* PUMPX, DP/DM voltage, OTGF_ITREMR        */
#define SGM41513_REG0E  0x0Eu /* (RO) INPUT_DET_DONE. NO burst access.    */
#define SGM41513_REG0F  0x0Fu /* VREG_FT, ISHORT_SET, STAT_SET, VINDPM_OS */

/* Power-on reset values of the R/W registers (used by the shadow cache)   */
#define SGM41513_REG00_POR   0x17u
#define SGM41513_REG01_POR   0x1Au
#define SGM41513_REG02_POR   0xB4u
#define SGM41513_REG03_POR   0xAAu
#define SGM41513_REG04_POR   0x58u
#define SGM41513_REG05_POR   0xBFu
#define SGM41513_REG06_POR   0xE6u
#define SGM41513_REG07_POR   0x4Cu
#define SGM41513_REG0C_POR   0x75u
#define SGM41513_REG0D_POR   0x01u
#define SGM41513_REG0F_POR   0x00u

/* ---------------------------------------------------------------------- */
/* REG00 - input current limit / HIZ / STAT pin function                   */
/* ---------------------------------------------------------------------- */

#define SGM41513_EN_HIZ              (0x01u << 7) /* 1: VBUS disconnected
                                                     from internal circuit */
#define SGM41513_EN_ICHG_MON_SHIFT   5u
#define SGM41513_EN_ICHG_MON_MASK    (0x03u << 5)
#define SGM41513_EN_ICHG_MON_CHARGE  (0x00u << 5) /* STAT follows charge   */
#define SGM41513_EN_ICHG_MON_STATSET (0x01u << 5) /* STAT follows STAT_SET*/
#define SGM41513_EN_ICHG_MON_DISABLE (0x02u << 5) /* STAT disabled        */

#define SGM41513_IINDPM_SHIFT        0u
#define SGM41513_IINDPM_MASK         0x1Fu
/* IINDPM(mA) = 100 + 100 * code, range 100 .. 3200 mA, 100 mA steps     */
#define SGM41513_IINDPM_MIN_MA       100u
#define SGM41513_IINDPM_MAX_MA       3200u
#define SGM41513_IINDPM_STEP_MA      100u

/* ---------------------------------------------------------------------- */
/* REG01 - converter configuration                                         */
/* ---------------------------------------------------------------------- */

#define SGM41513_PFM_DIS             (0x01u << 7) /* 1: PFM disabled       */
#define SGM41513_WD_RST              (0x01u << 6) /* W1C: kick watchdog    */
#define SGM41513_OTG_CONFIG          (0x01u << 5) /* 1: OTG boost on
                                                     (priority over CHG)  */
#define SGM41513_CHG_CONFIG          (0x01u << 4) /* 1: charging enabled
                                                     (nCE pin must be low)*/
#define SGM41513_SYS_MIN_SHIFT       1u
#define SGM41513_SYS_MIN_MASK        (0x07u << 1)
/* SYS_MIN lookup (mV): 2600,2800,3000,3200,3400,3500,3600,3700          */
#define SGM41513_SYS_MIN_MIN_MV      2600u
#define SGM41513_SYS_MIN_MAX_MV      3700u
#define SGM41513_MIN_BAT_SEL         (0x01u << 0) /* OTG low-battery gate:
                                                     0: 2.95/3.15 V
                                                     1: 2.6/2.8 V         */

/* ---------------------------------------------------------------------- */
/* REG02 - charge current                                                  */
/* ---------------------------------------------------------------------- */

#define SGM41513_BOOST_LIM           (0x01u << 7) /* 0: 0.5A, 1: 1.2A      */
#define SGM41513_Q1_FULLON           (0x01u << 6) /* 1: Q1 always full on */
#define SGM41513_ICHG_SHIFT          0u
#define SGM41513_ICHG_MASK           0x3Fu
/* ICHG uses a non-linear 64-entry lookup table (see sgm41513.c), 0..3 A */
#define SGM41513_ICHG_MIN_MA         0u
#define SGM41513_ICHG_MAX_MA         3000u

/* ---------------------------------------------------------------------- */
/* REG03 - pre-charge / termination current                                */
/* ---------------------------------------------------------------------- */

#define SGM41513_IPRECHG_SHIFT       4u
#define SGM41513_IPRECHG_MASK        (0x0Fu << 4)
#define SGM41513_ITERM_SHIFT         0u
#define SGM41513_ITERM_MASK          0x0Fu
/* IPRECHG/ITERM share one 16-entry table (mA):
 * 5,10,15,20,30,40,50,60,80,100,120,140,160,180,200,240                */
#define SGM41513_IPRECHG_TERM_MIN_MA 5u
#define SGM41513_IPRECHG_TERM_MAX_MA 240u

/* ---------------------------------------------------------------------- */
/* REG04 - charge voltage / top-off / recharge                             */
/* ---------------------------------------------------------------------- */

#define SGM41513_VREG_SHIFT          3u
#define SGM41513_VREG_MASK           (0x1Fu << 3)
/* VREG(mV) = 3856 + 32*code for code <= 24, except code 15 = 4350 mV.
 * Codes above 24 clamp to 4624 mV (code 24). Range 3856 .. 4624 mV.     */
#define SGM41513_VREG_MIN_MV         3856u
#define SGM41513_VREG_MAX_MV         4624u
#define SGM41513_VREG_STEP_MV        32u
#define SGM41513_VREG_CODE_MAX       24u

#define SGM41513_TOPOFF_TIMER_SHIFT  1u
#define SGM41513_TOPOFF_TIMER_MASK   (0x03u << 1)
/* 00: disabled, 01: 15min, 10: 30min, 11: 45min                         */
#define SGM41513_VRECHG              (0x01u << 0) /* 0: 100mV, 1: 200mV    */

/* ---------------------------------------------------------------------- */
/* REG05 - termination / watchdog / timers / thermal / JEITA cool          */
/* ---------------------------------------------------------------------- */

#define SGM41513_EN_TERM             (0x01u << 7) /* 1: termination on     */
#define SGM41513_ITERM_TIMER         (0x01u << 6) /* 0: 230ms, 1: 16ms     */
#define SGM41513_WATCHDOG_SHIFT      4u
#define SGM41513_WATCHDOG_MASK       (0x03u << 4)
/* 00: off, 01: 40s, 10: 80s, 11: 160s (POR default)                     */
#define SGM41513_EN_TIMER            (0x01u << 3) /* safety timer enable   */
#define SGM41513_CHG_TIMER           (0x01u << 2) /* 0: 7h, 1: 16h         */
#define SGM41513_TREG                (0x01u << 1) /* 0: 80C, 1: 120C       */
#define SGM41513_JEITA_ISET_L        (0x01u << 0) /* cool current:
                                                     0: 50% ICHG, 1: 20%  */

/* ---------------------------------------------------------------------- */
/* REG06 - input OVP / boost voltage / VINDPM threshold                    */
/* ---------------------------------------------------------------------- */

#define SGM41513_OVP_SHIFT           6u
#define SGM41513_OVP_MASK            (0x03u << 6)
/* 00: 5.5V, 01: 6.5V (5V input), 10: 10.5V (9V), 11: 14V (12V, POR)     */

#define SGM41513_BOOSTV_SHIFT        4u
#define SGM41513_BOOSTV_MASK         (0x03u << 4)
/* 00: 4.85V, 01: 5.00V, 10: 5.15V (POR), 11: 5.30V                      */

#define SGM41513_VINDPM_SHIFT        0u
#define SGM41513_VINDPM_MASK         0x0Fu
/* VINDPM(mV) = VINDPM_OS offset + 100 * code, 16 steps of 100 mV.
 * Offsets (REG0F VINDPM_OS): 3900/5900/7500/10500 mV                   */
#define SGM41513_VINDPM_STEP_MV      100u
#define SGM41513_VINDPM_CODES        16u

/* ---------------------------------------------------------------------- */
/* REG07 - detection / timers / BATFET / JEITA warm / VDPM tracking        */
/* ---------------------------------------------------------------------- */

#define SGM41513_IINDET_EN           (0x01u << 7) /* W1SC: force input
                                                     current detection    */
#define SGM41513_TMR2X_EN            (0x01u << 6) /* 1: safety timer runs
                                                     at half rate while
                                                     in DPM/JEITA-cool/
                                                     thermal regulation   */
#define SGM41513_BATFET_DIS          (0x01u << 5) /* 1: BATFET off
                                                     (ship mode)          */
#define SGM41513_JEITA_VSET_H        (0x01u << 4) /* warm voltage:
                                                     0: min(VREG,4.1V)
                                                     1: VREG              */
#define SGM41513_BATFET_DLY          (0x01u << 3) /* 1: BATFET off delayed
                                                     by tSM_DLY (12s)     */
#define SGM41513_BATFET_RST_EN       (0x01u << 2) /* 1: nQON 10s BATFET
                                                     reset enabled        */
#define SGM41513_VDPM_BAT_TRACK_SHIFT 0u
#define SGM41513_VDPM_BAT_TRACK_MASK  0x03u
/* 00: off, 01: VBAT+200mV, 10: VBAT+250mV, 11: VBAT+300mV.
 * Effective VINDPM = max(VINDPM, tracked). Only when VINDPM_OS = 00.   */

/* ---------------------------------------------------------------------- */
/* REG08 (RO) - status                                                     */
/* ---------------------------------------------------------------------- */

#define SGM41513_VBUS_STAT_SHIFT     5u
#define SGM41513_VBUS_STAT_MASK      (0x07u << 5)
/* Encoding differs per variant:
 * SGM41513        : 000 none, 001 USB SDP 500mA (PSEL high),
 *                   010 adapter 2.4A (PSEL low), 111 OTG
 * SGM41513A/D     : 000 none, 001 SDP, 010 CDP 1.5A, 011 DCP 2.4A,
 *                   101 unknown adapter 500mA,
 *                   110 non-standard adapter, 111 OTG                  */
#define SGM41513_CHRG_STAT_SHIFT     3u
#define SGM41513_CHRG_STAT_MASK      (0x03u << 3)
/* 00: not charging, 01: trickle/pre-charge, 10: fast (CC/CV),
 * 11: charge terminated                                               */
#define SGM41513_PG_STAT             (0x01u << 2) /* power good            */
#define SGM41513_THERM_STAT          (0x01u << 1) /* thermal regulation    */
#define SGM41513_VSYS_STAT           (0x01u << 0) /* VSYS_MIN regulation   */

/* ---------------------------------------------------------------------- */
/* REG09 (RO) - faults (latched until read; read twice for live values;
 *                NTC_FAULT is real time; NO burst access)                 */
/* ---------------------------------------------------------------------- */

#define SGM41513_WATCHDOG_FAULT      (0x01u << 7)
#define SGM41513_BOOST_FAULT         (0x01u << 6)
#define SGM41513_CHRG_FAULT_SHIFT    4u
#define SGM41513_CHRG_FAULT_MASK     (0x03u << 4)
/* 00: normal, 01: input fault (VAC OVP or VBAT<VVBUS<3.8V),
 * 10: thermal shutdown, 11: safety timer expired                      */
#define SGM41513_BAT_FAULT           (0x01u << 3) /* battery OVP           */
#define SGM41513_NTC_FAULT_SHIFT     0u
#define SGM41513_NTC_FAULT_MASK      0x07u
/* 000: normal, 010: warm (buck only), 011: cool (buck only),
 * 101: cold, 110: hot                                                 */

/* ---------------------------------------------------------------------- */
/* REG0A - DPM status (RO) + INT masks (R/W)                               */
/* ---------------------------------------------------------------------- */

#define SGM41513_VBUS_GD             (0x01u << 7) /* good VBUS attached    */
#define SGM41513_VINDPM_STAT         (0x01u << 6) /* in VINDPM             */
#define SGM41513_IINDPM_STAT         (0x01u << 5) /* in IINDPM             */
#define SGM41513_TOPOFF_ACTIVE       (0x01u << 3) /* top-off timer running */
#define SGM41513_ACOV_STAT           (0x01u << 2) /* input OVP detected    */
#define SGM41513_VINDPM_INT_MASK     (0x01u << 1) /* 1: mask VINDPM nINT   */
#define SGM41513_IINDPM_INT_MASK     (0x01u << 0) /* 1: mask IINDPM nINT   */

/* ---------------------------------------------------------------------- */
/* REG0B - reset (W) + part identification (RO)                            */
/* ---------------------------------------------------------------------- */

#define SGM41513_REG_RST             (0x01u << 7) /* W1SC: reset all R/W
                                                     registers to POR     */
#define SGM41513_PN_SHIFT            3u
#define SGM41513_PN_MASK             (0x0Fu << 3)
#define SGM41513_PN_SGM41513         (0x00u << 3) /* PN = 0000             */
#define SGM41513_PN_SGM41513A_D      (0x01u << 3) /* PN = 0001 (A or D)    */
#define SGM41513_SGMPART_SHIFT       2u
#define SGM41513_SGMPART_MASK        (0x01u << 2) /* reads 0 on this part  */
#define SGM41513_DEV_REV_SHIFT       0u
#define SGM41513_DEV_REV_MASK        0x03u

/* ---------------------------------------------------------------------- */
/* REG0C - JEITA configuration                                             */
/* ---------------------------------------------------------------------- */

#define SGM41513_JEITA_VSET_L        (0x01u << 7) /* cool voltage:
                                                     0: VREG
                                                     1: min(VREG,4.1V)    */
#define SGM41513_JEITA_ISET_L_EN     (0x01u << 6) /* 1: charging allowed
                                                     in cool range        */
#define SGM41513_JEITA_ISET_H_SHIFT  4u
#define SGM41513_JEITA_ISET_H_MASK   (0x03u << 4)
/* warm current: 00: 0%, 01: 20%, 10: 50%, 11: 100% of ICHG (POR)       */
#define SGM41513_JEITA_VT2_SHIFT     2u
#define SGM41513_JEITA_VT2_MASK      (0x03u << 2)
/* cool threshold T2: 00: ~5.5C, 01: ~10C (POR), 10: ~15C, 11: ~20C      */
#define SGM41513_JEITA_VT3_SHIFT     0u
#define SGM41513_JEITA_VT3_MASK      0x03u
/* warm threshold T3: 00: ~40C, 01: ~44.5C (POR), 10: ~50.5C, 11: ~54.5C */

/* ---------------------------------------------------------------------- */
/* REG0D - PUMPX / DP-DM voltages / OTG frequency                          */
/* ---------------------------------------------------------------------- */

#define SGM41513_EN_PUMPX            (0x01u << 7)
#define SGM41513_PUMPX_UP            (0x01u << 6) /* W1SC when done        */
#define SGM41513_PUMPX_DN            (0x01u << 5) /* W1SC when done        */
#define SGM41513_DP_VSET_SHIFT       3u
#define SGM41513_DP_VSET_MASK        (0x03u << 3)
#define SGM41513_DM_VSET_SHIFT       1u
#define SGM41513_DM_VSET_MASK        (0x03u << 1)
/* DP_VSET/DM_VSET: 00: HIZ, 01: 0V, 10: 0.6V, 11: 3.3V                  */
#define SGM41513_OTGF_ITREMR         (0x01u << 0)
/* Dual-purpose bit:
 *   OTG mode    : 0: boost fSW = 500kHz, 1: 1.5MHz (POR)
 *   Charge mode : 0: ITERM x6 effective when ICHG>300mA, 1: ITERM as-is */

/* ---------------------------------------------------------------------- */
/* REG0E (RO) - input detection flag (NO burst access)                     */
/* ---------------------------------------------------------------------- */

#define SGM41513_INPUT_DET_DONE      (0x01u << 7) /* set when PSEL or
                                                     D+/D- detection done */

/* ---------------------------------------------------------------------- */
/* REG0F - VREG fine tune / trickle / STAT pattern / VINDPM offset         */
/* ---------------------------------------------------------------------- */

#define SGM41513_VREG_FT_SHIFT       6u
#define SGM41513_VREG_FT_MASK        (0x03u << 6)
/* 00: off (POR), 01: +8mV, 10: -8mV, 11: -16mV                          */
#define SGM41513_ISHORT_SET          (0x01u << 4) /* trickle: 0: 90mA
                                                     (POR), 1: 30mA       */
#define SGM41513_STAT_SET_SHIFT      2u
#define SGM41513_STAT_SET_MASK       (0x03u << 2)
/* STAT pattern when EN_ICHG_MON = 01:
 * 00: LED off, 01: LED on, 10: blink 1s on/1s off, 11: 1s on/3s off     */
#define SGM41513_VINDPM_OS_SHIFT     0u
#define SGM41513_VINDPM_OS_MASK      0x03u
/* VINDPM offset: 00: 3900mV (POR), 01: 5900mV, 10: 7500mV, 11: 10500mV */

/* VINDPM window per offset: [offset, offset + 15*100mV]                 */
#define SGM41513_VINDPM_OS_MV_0      3900u
#define SGM41513_VINDPM_OS_MV_1      5900u
#define SGM41513_VINDPM_OS_MV_2      7500u
#define SGM41513_VINDPM_OS_MV_3      10500u

/* OTG start timing: EN_HIZ must be cleared and REGN allowed to start
 * before OTG_CONFIG is set (datasheet requires >= 30 ms).               */
#define SGM41513_OTG_START_DELAY_MS  30u

#ifdef __cplusplus
}
#endif

#endif /* SGM41513_REGS_H */
