/*
 * sgm41513_conf.h - Configuration options for the SGM41513 driver.
 *
 * This file holds the driver's compile-time configuration: every option
 * can be changed here directly, or overridden from the compiler command
 * line (e.g. -DSGM41513_THREAD_SAFE=1), because every default below is
 * wrapped in #ifndef.
 */

#ifndef SGM41513_CONF_H
#define SGM41513_CONF_H

/* ---------------------------------------------------------------------- */
/* SGM41513_VARIANT - which silicon variant is on the board                */
/* ---------------------------------------------------------------------- */
/* The part number field (REG0B PN[3:0]) only distinguishes PN=0000
 * (SGM41513) from PN=0001 (SGM41513A or SGM41513D). The A and D variants
 * are electrically identical on I2C; they differ in the VBUS_STAT status
 * encoding. The driver auto-detects SGM41513 vs A/D at init() time and
 * uses the option below to decode VBUS_STAT when PN = 0001.
 *
 *  SGM41513_VARIANT_SGM41513  (0): base part, PSEL pin input detection
 *  SGM41513_VARIANT_SGM41513A (1): D+/D- BC1.2 detection
 *  SGM41513_VARIANT_SGM41513D (2): D+/D- BC1.2 detection (full status set)
 */

#define SGM41513_VARIANT_SGM41513   0
#define SGM41513_VARIANT_SGM41513A  1
#define SGM41513_VARIANT_SGM41513D  2

#ifndef SGM41513_VARIANT
#define SGM41513_VARIANT    SGM41513_VARIANT_SGM41513D
#endif

/* ---------------------------------------------------------------------- */
/* Feature switches (set to 0 to compile the feature out - the related    */
/* API functions then return SGM41513_ERR_NOT_SUPPORTED and cost no code) */
/* ---------------------------------------------------------------------- */

/* JEITA temperature window configuration (REG0C and friends)             */
#ifndef SGM41513_USE_JEITA
#define SGM41513_USE_JEITA  1
#endif

/* OTG reverse boost mode (REG01 OTG_CONFIG, REG06 BOOSTV, REG02 LIM)     */
#ifndef SGM41513_USE_OTG
#define SGM41513_USE_OTG    1
#endif

/* PUMPX current-pulse protocol for adjustable adapters (REG0D)           */
#ifndef SGM41513_USE_PUMPX
#define SGM41513_USE_PUMPX  1
#endif

/* Ship mode / BATFET control (REG07)                                     */
#ifndef SGM41513_USE_SHIP
#define SGM41513_USE_SHIP   1
#endif

/* ---------------------------------------------------------------------- */
/* SGM41513_REG_SHADOW - register shadow cache                             */
/* ---------------------------------------------------------------------- */
/* 1: the driver keeps a copy of every R/W register and provides
 *    sgm41513_restore_settings() to re-apply all settings in one call.
 *    Use it after a watchdog expiry (the chip resets R/W registers to
 *    their POR values when the watchdog times out) or after leaving
 *    ship mode (the chip then forces EN_HIZ = 1).
 * 0: saves 16 bytes per device instance, no restore support.             */

#ifndef SGM41513_REG_SHADOW
#define SGM41513_REG_SHADOW 1
#endif

/* ---------------------------------------------------------------------- */
/* SGM41513_VERIFY_WRITES - read-back verification                         */
/* ---------------------------------------------------------------------- */
/* 1: after every R/W register write the value is read back and compared
 *    (self-clearing bits excluded). Returns SGM41513_ERR_VERIFY on
 *    mismatch. Costs one extra I2C read per write - keep it off in
 *    production unless you suspect bus integrity problems.               */

#ifndef SGM41513_VERIFY_WRITES
#define SGM41513_VERIFY_WRITES  0
#endif

/* ---------------------------------------------------------------------- */
/* SGM41513_THREAD_SAFE - concurrency protection hooks                     */
/* ---------------------------------------------------------------------- */
/* 1: the driver brackets every multi-access API call with
 *    sgm41513_io_lock() / sgm41513_io_unlock() which YOU must implement
 *    (e.g. mutex take/give). Single-threaded systems should leave this
 *    at 0 for zero overhead.                                             */

#ifndef SGM41513_THREAD_SAFE
#define SGM41513_THREAD_SAFE   0
#endif

#endif /* SGM41513_CONF_H */
