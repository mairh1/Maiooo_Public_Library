/**
 * @file    aw32257.h
 * @brief   Portable C99 driver for the AW32257 battery charger and boost IC
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * @details
 * The driver owns no hardware resources. The caller provides bounded register
 * read/write callbacks, a millisecond delay callback, and storage for each
 * driver instance. The core is non-reentrant and must not be called from an
 * interrupt service routine.
 *
 * SPDX-License-Identifier: WTFPL
 */

#ifndef AW32257_H
#define AW32257_H

#include <stdbool.h>
#include <stdint.h>

#include "aw32257_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AW32257_DRIVER_VERSION_MAJOR          1
#define AW32257_DRIVER_VERSION_MINOR          0
#define AW32257_DRIVER_VERSION_PATCH          0

/** @brief Driver result codes. */
typedef enum
{
    AW32257_OK                         =  0,
    AW32257_ERR_NULL_POINTER           = -1,
    AW32257_ERR_INVALID_ARGUMENT       = -2,
    AW32257_ERR_NOT_BOUND              = -3,
    AW32257_ERR_NOT_INITIALIZED        = -4,
    AW32257_ERR_RANGE                  = -5,
    AW32257_ERR_IO                     = -6,
    AW32257_ERR_ID_MISMATCH            = -7,
    AW32257_ERR_SAFETY_MISMATCH        = -8,
    AW32257_ERR_STATE                  = -9,
    AW32257_ERR_POR_REQUIRED           = -10
} aw32257_status_t;

/** @brief Local lifecycle state of a driver instance. */
typedef enum
{
    AW32257_LIFECYCLE_UNBOUND = 0,
    AW32257_LIFECYCLE_BOUND,
    AW32257_LIFECYCLE_READY,
    AW32257_LIFECYCLE_POR_REQUIRED
} aw32257_lifecycle_t;

/**
 * @brief Read one AW32257 register.
 *
 * @param[in]  context     Caller-owned port context.
 * @param[in]  address_7bit Always @ref AW32257_I2C_ADDRESS_7BIT.
 * @param[in]  register_address Register address in the range 0x00 to 0x0A.
 * @param[out] value       Byte read from the device.
 * @param[in]  timeout_ms  Non-zero upper bound for the transaction.
 *
 * @return 0 on success; otherwise a platform-defined error code.
 */
typedef int32_t (*aw32257_read_reg_fn)(void * context,
                                       uint8_t address_7bit,
                                       uint8_t register_address,
                                       uint8_t * value,
                                       uint32_t timeout_ms);

/**
 * @brief Write one AW32257 register.
 *
 * @return 0 on success; otherwise a platform-defined error code.
 */
typedef int32_t (*aw32257_write_reg_fn)(void * context,
                                        uint8_t address_7bit,
                                        uint8_t register_address,
                                        uint8_t value,
                                        uint32_t timeout_ms);

/**
 * @brief Delay for at least the requested number of milliseconds.
 *
 * The callback is used for the mandatory 32 ms quiet interval after a
 * software-reset command. It must not return early.
 */
typedef void (*aw32257_delay_ms_fn)(void * context, uint32_t milliseconds);

/** @brief Platform callbacks and ownership contract. */
typedef struct
{
    aw32257_read_reg_fn read_reg;
    aw32257_write_reg_fn write_reg;
    aw32257_delay_ms_fn delay_ms;
    void * context;
    uint32_t io_timeout_ms;
} aw32257_port_t;

/**
 * @brief Fast-charge and safety-current register code.
 *
 * Physical current depends on the external sense resistor. Use
 * aw32257_current_code_to_ma_33mohm() only for a verified 33 mOhm design.
 */
typedef enum
{
    AW32257_CURRENT_CODE_00 = 0x00,
    AW32257_CURRENT_CODE_01 = 0x01,
    AW32257_CURRENT_CODE_02 = 0x02,
    AW32257_CURRENT_CODE_03 = 0x03,
    AW32257_CURRENT_CODE_04 = 0x04,
    AW32257_CURRENT_CODE_05 = 0x05,
    AW32257_CURRENT_CODE_06 = 0x06,
    AW32257_CURRENT_CODE_07 = 0x07,
    AW32257_CURRENT_CODE_08 = 0x08,
    AW32257_CURRENT_CODE_09 = 0x09,
    AW32257_CURRENT_CODE_0A = 0x0A,
    AW32257_CURRENT_CODE_0B = 0x0B,
    AW32257_CURRENT_CODE_0C = 0x0C,
    AW32257_CURRENT_CODE_0D = 0x0D,
    AW32257_CURRENT_CODE_0E = 0x0E,
    AW32257_CURRENT_CODE_0F = 0x0F
} aw32257_current_code_t;

/** @brief Termination-current register code. */
typedef enum
{
    AW32257_TERM_CURRENT_CODE_0 = 0,
    AW32257_TERM_CURRENT_CODE_1 = 1,
    AW32257_TERM_CURRENT_CODE_2 = 2,
    AW32257_TERM_CURRENT_CODE_3 = 3,
    AW32257_TERM_CURRENT_CODE_4 = 4,
    AW32257_TERM_CURRENT_CODE_5 = 5,
    AW32257_TERM_CURRENT_CODE_6 = 6,
    AW32257_TERM_CURRENT_CODE_7 = 7
} aw32257_term_current_code_t;

/** @brief Requested operation mode through REG01. External pins may override it. */
typedef enum
{
    AW32257_MODE_CHARGE = 0,
    AW32257_MODE_HIGH_IMPEDANCE,
    AW32257_MODE_BOOST
} aw32257_mode_t;

/** @brief Charge state reported by REG00. */
typedef enum
{
    AW32257_CHARGE_STATE_READY       = 0,
    AW32257_CHARGE_STATE_IN_PROGRESS = 1,
    AW32257_CHARGE_STATE_DONE        = 2,
    AW32257_CHARGE_STATE_FAULT       = 3
} aw32257_charge_state_t;

/** @brief Charge fault reported by REG00. */
typedef enum
{
    AW32257_CHARGE_FAULT_NORMAL                  = 0,
    AW32257_CHARGE_FAULT_VBUS_OVP                = 1,
    AW32257_CHARGE_FAULT_SLEEP                   = 2,
    AW32257_CHARGE_FAULT_BAD_ADAPTER_OR_VBUS_UVLO = 3,
    AW32257_CHARGE_FAULT_BATTERY_OVP             = 4,
    AW32257_CHARGE_FAULT_THERMAL_SHUTDOWN        = 5,
    AW32257_CHARGE_FAULT_RESERVED_6              = 6,
    AW32257_CHARGE_FAULT_NO_BATTERY              = 7
} aw32257_charge_fault_t;

/** @brief Boost fault reported by REG09. */
typedef enum
{
    AW32257_BOOST_FAULT_NORMAL              = 0,
    AW32257_BOOST_FAULT_VBUS_OVP            = 1,
    AW32257_BOOST_FAULT_OVERLOAD            = 2,
    AW32257_BOOST_FAULT_BATTERY_LOW         = 3,
    AW32257_BOOST_FAULT_RESERVED_4          = 4,
    AW32257_BOOST_FAULT_THERMAL_SHUTDOWN    = 5,
    AW32257_BOOST_FAULT_RESERVED_6          = 6,
    AW32257_BOOST_FAULT_RESERVED_7          = 7
} aw32257_boost_fault_t;

/** @brief Symbolic boost-driver slew-rate code; no physical rates are documented. */
typedef enum
{
    AW32257_SLEW_RATE_DEFAULT = 0,
    AW32257_SLEW_RATE_SLOW,
    AW32257_SLEW_RATE_SLOWER,
    AW32257_SLEW_RATE_SLOWEST
} aw32257_slew_rate_t;

/** @brief POR-only safety configuration. */
typedef struct
{
    aw32257_current_code_t max_charge_current;
    uint16_t max_charge_voltage_mv;
} aw32257_safety_config_t;

/** @brief Charge termination algorithm configuration. */
typedef struct
{
    uint8_t window_periods;
    uint8_t valid_periods;
    uint8_t deglitch_ms;
    uint16_t recharge_threshold_mv;
} aw32257_termination_config_t;

/** @brief Boost output and power-train driver configuration. */
typedef struct
{
    uint16_t output_voltage_mv;
    uint16_t frequency_khz;
    aw32257_slew_rate_t slew_rate;
    bool fixed_dead_time;
    bool force_pwm;
} aw32257_boost_config_t;

/** @brief Decoded REG03 device information. */
typedef struct
{
    uint8_t raw_reg03;
    uint8_t vendor_code;
    uint8_t part_code;
    uint8_t revision_code;
} aw32257_device_info_t;

/**
 * @brief Sequential status snapshot.
 *
 * REG00, REG05, and REG09 are read in that order. The values are not a
 * hardware-latched atomic snapshot.
 */
typedef struct
{
    uint8_t raw_reg00;
    uint8_t raw_reg05;
    uint8_t raw_reg09;
    bool otg_pin_high;
    bool stat_output_enabled;
    aw32257_charge_state_t charge_state;
    bool boost_active;
    aw32257_charge_fault_t charge_fault;
    bool dpm_active;
    bool cd_pin_high;
    aw32257_boost_fault_t boost_fault;
} aw32257_status_snapshot_t;

/**
 * @brief Sequential configuration snapshot.
 *
 * Every register must be read successfully before the caller's output object
 * is updated. The snapshot is not atomic across registers.
 */
typedef struct
{
    uint8_t raw_reg00;
    uint8_t raw_reg01;
    uint8_t raw_reg02;
    uint8_t raw_reg04;
    uint8_t raw_reg05;
    uint8_t raw_reg06;
    uint8_t raw_reg07;
    uint8_t raw_reg0a;
    bool stat_output_enabled;
    bool termination_enabled;
    bool charge_enabled;
    bool high_impedance_requested;
    bool boost_requested;
    uint16_t charge_voltage_mv;
    bool otg_active_high;
    bool otg_pin_control_enabled;
    aw32257_current_code_t fast_charge_current;
    aw32257_term_current_code_t termination_current;
    uint16_t dpm_voltage_mv;
    aw32257_current_code_t max_charge_current;
    uint16_t max_charge_voltage_mv;
    aw32257_termination_config_t termination;
    aw32257_boost_config_t boost;
} aw32257_config_snapshot_t;

/** @brief Caller-owned driver instance. */
typedef struct
{
    aw32257_port_t port;
    aw32257_lifecycle_t lifecycle;
    int32_t last_port_error;
} aw32257_t;

/**
 * @brief Bind a caller-owned instance to its platform callbacks.
 *
 * This function performs local validation and copies @p port by value. It
 * never accesses the I2C bus. A successful call establishes the BOUND state.
 *
 * @warning Rebinding any previously used instance is the caller's explicit
 * acknowledgement that the AW32257 has undergone a real hardware power-on
 * reset since its previous bind/init cycle. This is especially critical for
 * POR_REQUIRED. The driver cannot observe or prove a power cycle. Rebinding
 * without that reset can leave REG06 permanently locked for the current power
 * cycle.
 *
 * @param[out] device Caller-owned instance. It need not be preinitialized.
 * @param[in]  port Platform callbacks, context, and non-zero I/O timeout.
 * @return AW32257_OK or a local argument error; no port callback is invoked.
 */
aw32257_status_t aw32257_bind(aw32257_t * device, const aw32257_port_t * port);

/**
 * @brief Perform the mandatory POR-safe safety write and device check.
 *
 * After local validation, the first bus operation is a direct REG06 write,
 * followed by a REG06 readback and a REG03 identity read. The identity check
 * accepts all revision codes while requiring the documented vendor/part mask.
 *
 * Every failure after entry with a BOUND instance, including a local safety
 * parameter failure before any I2C access, latches the instance into
 * POR_REQUIRED. The caller must perform a real hardware POR before binding and
 * initializing again. @p device_info is optional and is updated only on full
 * success.
 *
 * @param[in,out] device Bound instance immediately following a hardware POR.
 * @param[in] safety Product-specific POR-only current and voltage limits.
 * @param[out] device_info Optional decoded identity result.
 */
aw32257_status_t aw32257_power_on_init(aw32257_t * device,
                                        const aw32257_safety_config_t * safety,
                                        aw32257_device_info_t * device_info);

/**
 * @brief Issue a software reset when charging and boost are inactive.
 *
 * The driver first samples REG00 and refuses reset while charge is in progress
 * or boost is active. Once the reset write is attempted, delay_ms(context, 32)
 * is always called before this function returns, even if the write callback
 * reports an error: the device may have accepted RESET while its ACK was lost.
 * No I2C access occurs during that requested quiet interval.
 *
 * @return AW32257_OK, AW32257_ERR_STATE, or a lifecycle/port error.
 */
aw32257_status_t aw32257_soft_reset(aw32257_t * device);

/** @brief Return the local lifecycle, or UNBOUND for a NULL pointer. */
aw32257_lifecycle_t aw32257_get_lifecycle(const aw32257_t * device);

/**
 * @brief Return the raw result of the most recent register I/O callback.
 *
 * A successful read_reg/write_reg callback stores zero. The void delay_ms
 * callback and local validation errors do not change the stored value. A NULL
 * @p device also yields zero. This is not a persistent error history.
 */
int32_t aw32257_get_last_port_error(const aw32257_t * device);

/**
 * @brief Read one documented register after successful initialization.
 *
 * This diagnostic API accepts REG00 through REG0A. Deliberately no matching
 * arbitrary-register write API is exposed.
 */
aw32257_status_t aw32257_read_register(aw32257_t * device,
                                        uint8_t register_address,
                                        uint8_t * value);

/** @brief Read and validate REG03, committing output only on success. */
aw32257_status_t aw32257_read_device_info(aw32257_t * device,
                                           aw32257_device_info_t * device_info);

/** @brief Read the sequential REG00/REG05/REG09 status snapshot. */
aw32257_status_t aw32257_read_status(aw32257_t * device,
                                      aw32257_status_snapshot_t * snapshot);

/** @brief Read a sequential, all-or-nothing configuration snapshot. */
aw32257_status_t aw32257_read_configuration(aw32257_t * device,
                                             aw32257_config_snapshot_t * snapshot);

/** @brief Enable or disable the open-drain STAT output through REG00 RMW. */
aw32257_status_t aw32257_set_stat_output_enabled(aw32257_t * device, bool enabled);

/** @brief Enable or disable charging while hiding REG01.CEN inversion. */
aw32257_status_t aw32257_set_charge_enabled(aw32257_t * device, bool enabled);

/** @brief Enable or disable charge termination through REG01 RMW. */
aw32257_status_t aw32257_set_termination_enabled(aw32257_t * device, bool enabled);

/** @brief Request charge, high-impedance, or boost mode through REG01. */
aw32257_status_t aw32257_set_mode(aw32257_t * device, aw32257_mode_t mode);

/** @brief Set an exact 3500..4500 mV, 20 mV-step VOREG value. */
aw32257_status_t aw32257_set_charge_voltage_mv(aw32257_t * device,
                                                uint16_t voltage_mv);

/** @brief Set the fast-charge current register code, independent of RSNS. */
aw32257_status_t aw32257_set_fast_charge_current(aw32257_t * device,
                                                  aw32257_current_code_t current_code);

/** @brief Set the termination-current register code, independent of RSNS. */
aw32257_status_t aw32257_set_termination_current(aw32257_t * device,
                                                  aw32257_term_current_code_t current_code);

/** @brief Set an exact 4250..4775 mV, 75 mV-step DPM value. */
aw32257_status_t aw32257_set_dpm_voltage_mv(aw32257_t * device,
                                             uint16_t voltage_mv);

/**
 * @brief Set CTA and recharge fields after full local validation.
 *
 * Combinations whose valid_periods times deglitch_ms exceeds the documented
 * 256 ms electrical-characteristic limit are rejected without bus access.
 */
aw32257_status_t aw32257_set_termination_config(aw32257_t * device,
                                                 const aw32257_termination_config_t * config);

/** @brief Configure use and active polarity of the external OTG pin. */
aw32257_status_t aw32257_configure_otg_pin(aw32257_t * device,
                                            bool enabled,
                                            bool active_high);

/** @brief Set boost output and symbolic power-train driver fields. */
aw32257_status_t aw32257_set_boost_config(aw32257_t * device,
                                           const aw32257_boost_config_t * config);

/** @brief Look up the datasheet current table for a verified 33 mOhm RSNS. */
aw32257_status_t aw32257_current_code_to_ma_33mohm(
    aw32257_current_code_t current_code,
    uint16_t * current_ma);

/** @brief Look up the termination table for a verified 33 mOhm RSNS. */
aw32257_status_t aw32257_termination_current_code_to_ma_33mohm(
    aw32257_term_current_code_t current_code,
    uint16_t * current_ma);

#ifdef __cplusplus
}
#endif

#endif /* AW32257_H */
