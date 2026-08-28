/**
 * @file    test_aw32257.c
 * @brief   Mock-I2C unit tests for the portable AW32257 driver
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aw32257.h"

#define MOCK_LOG_CAPACITY 128U

typedef enum
{
    MOCK_LOG_READ = 0,
    MOCK_LOG_WRITE,
    MOCK_LOG_DELAY
} mock_log_type_t;

typedef struct
{
    mock_log_type_t type;
    uint8_t address;
    uint8_t reg;
    uint8_t value;
    uint32_t timeout_ms;
    uint32_t at_ms;
} mock_log_entry_t;

typedef struct
{
    uint8_t regs[AW32257_REG_LAST + 1U];
    mock_log_entry_t log[MOCK_LOG_CAPACITY];
    uint32_t log_count;
    uint32_t i2c_call_count;
    uint32_t now_ms;
    uint32_t reset_ready_at_ms;
    int32_t fail_on_i2c_call;
    int32_t fail_code;
    bool safety_locked;
} mock_bus_t;

static int g_failures;
static int g_checks;

static void check_equal(long expected,
                        long actual,
                        const char * expression,
                        const char * function,
                        int line)
{
    g_checks++;
    if (expected != actual)
    {
        g_failures++;
        printf("FAIL %s:%d %s: expected %ld, got %ld\n",
               function,
               line,
               expression,
               expected,
               actual);
    }
}

static void check_true(bool condition,
                       const char * expression,
                       const char * function,
                       int line)
{
    g_checks++;
    if (!condition)
    {
        g_failures++;
        printf("FAIL %s:%d: %s\n", function, line, expression);
    }
}

#define CHECK_EQ(expected, actual) \
    check_equal((long)(expected), (long)(actual), #actual, __func__, __LINE__)
#define CHECK_TRUE(expression) \
    check_true((expression), #expression, __func__, __LINE__)

static void mock_load_reset_values(mock_bus_t * bus)
{
    uint8_t safety;

    safety = bus->regs[AW32257_REG_SAFETY_LIMIT];
    bus->regs[AW32257_REG_STATUS_CONTROL] = AW32257_REG00_RESET_KNOWN_VALUE;
    bus->regs[AW32257_REG_CONTROL] = AW32257_REG01_RESET_VALUE;
    bus->regs[AW32257_REG_BATTERY_VOLTAGE] = AW32257_REG02_RESET_VALUE;
    bus->regs[AW32257_REG_DEVICE_ID] = AW32257_REG03_RESET_VALUE;
    bus->regs[AW32257_REG_CHARGE_CURRENT] = AW32257_REG04_RESET_VALUE;
    bus->regs[AW32257_REG_DPM_STATUS] = AW32257_REG05_RESET_VALUE;
    bus->regs[AW32257_REG_SAFETY_LIMIT] = safety;
    bus->regs[AW32257_REG_TERMINATION] = AW32257_REG07_RESET_VALUE;
    bus->regs[AW32257_REG_VENDOR] = AW32257_REG08_RESET_VALUE;
    bus->regs[AW32257_REG_BOOST_FAULT] = AW32257_REG09_RESET_VALUE;
    bus->regs[AW32257_REG_BOOST_CONFIG] = AW32257_REG0A_RESET_VALUE;
}

static void mock_init(mock_bus_t * bus)
{
    memset(bus, 0, sizeof(*bus));
    bus->regs[AW32257_REG_SAFETY_LIMIT] = AW32257_REG06_RESET_VALUE;
    mock_load_reset_values(bus);
    bus->fail_on_i2c_call = -1;
    bus->fail_code = -900;
}

static void mock_clear_log(mock_bus_t * bus)
{
    bus->log_count = 0U;
    bus->i2c_call_count = 0U;
    bus->fail_on_i2c_call = -1;
    bus->fail_code = -900;
}

static void mock_log(mock_bus_t * bus,
                     mock_log_type_t type,
                     uint8_t address,
                     uint8_t reg,
                     uint8_t value,
                     uint32_t timeout_ms)
{
    if (bus->log_count < MOCK_LOG_CAPACITY)
    {
        bus->log[bus->log_count].type = type;
        bus->log[bus->log_count].address = address;
        bus->log[bus->log_count].reg = reg;
        bus->log[bus->log_count].value = value;
        bus->log[bus->log_count].timeout_ms = timeout_ms;
        bus->log[bus->log_count].at_ms = bus->now_ms;
        bus->log_count++;
    }
}

static bool mock_should_fail(mock_bus_t * bus)
{
    bus->i2c_call_count++;
    return (bus->fail_on_i2c_call > 0) &&
           ((int32_t)bus->i2c_call_count == bus->fail_on_i2c_call);
}

int32_t aw32257_io_read_reg(void * context,
                             uint8_t address_7bit,
                             uint8_t register_address,
                             uint8_t * value,
                             uint32_t timeout_ms)
{
    mock_bus_t * bus;

    bus = (mock_bus_t *)context;
    mock_log(bus,
             MOCK_LOG_READ,
             address_7bit,
             register_address,
             0U,
             timeout_ms);

    if (mock_should_fail(bus))
    {
        return bus->fail_code;
    }

    if ((value == NULL) || (register_address > AW32257_REG_LAST))
    {
        return -901;
    }

    if (bus->now_ms < bus->reset_ready_at_ms)
    {
        return -902;
    }

    if (register_address != AW32257_REG_SAFETY_LIMIT)
    {
        bus->safety_locked = true;
    }

    *value = bus->regs[register_address];
    return 0;
}

int32_t aw32257_io_write_reg(void * context,
                              uint8_t address_7bit,
                              uint8_t register_address,
                              uint8_t value,
                              uint32_t timeout_ms)
{
    mock_bus_t * bus;

    bus = (mock_bus_t *)context;
    mock_log(bus,
             MOCK_LOG_WRITE,
             address_7bit,
             register_address,
             value,
             timeout_ms);

    if (mock_should_fail(bus))
    {
        return bus->fail_code;
    }

    if (register_address > AW32257_REG_LAST)
    {
        return -903;
    }

    if (bus->now_ms < bus->reset_ready_at_ms)
    {
        return -904;
    }

    if (register_address == AW32257_REG_SAFETY_LIMIT)
    {
        if (!bus->safety_locked)
        {
            bus->regs[register_address] = value;
        }
        return 0;
    }

    bus->safety_locked = true;

    if ((register_address == AW32257_REG_CHARGE_CURRENT) &&
        ((value & AW32257_REG04_SOFT_RESET_MASK) != 0U))
    {
        mock_load_reset_values(bus);
        bus->reset_ready_at_ms = bus->now_ms + AW32257_SOFT_RESET_DELAY_MS;
        return 0;
    }

    bus->regs[register_address] = value;
    return 0;
}

void aw32257_io_delay_ms(void * context, uint32_t milliseconds)
{
    mock_bus_t * bus;

    bus = (mock_bus_t *)context;
    mock_log(bus, MOCK_LOG_DELAY, 0U, 0U, 0U, milliseconds);
    bus->now_ms += milliseconds;
}

static aw32257_status_t mock_prepare_ready(mock_bus_t * bus,
                                            aw32257_t * device)
{
    aw32257_safety_config_t safety;
    aw32257_status_t status;

    mock_init(bus);
    status = aw32257_init(device, bus, 25U);
    if (status != AW32257_OK)
    {
        return status;
    }

    safety.max_charge_current = AW32257_CURRENT_CODE_05;
    safety.max_charge_voltage_mv = 4240U;
    return aw32257_power_on_init(device, &safety, NULL);
}

static void test_bind_and_power_on_sequence(void)
{
    mock_bus_t bus;
    aw32257_t device;
    aw32257_safety_config_t safety;
    aw32257_device_info_t info;

    mock_init(&bus);
    CHECK_EQ(AW32257_ERR_NULL_POINTER, aw32257_init(NULL, &bus, 25U));
    CHECK_EQ(AW32257_ERR_INVALID_ARGUMENT, aw32257_init(&device, &bus, 0U));

    CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
    CHECK_EQ(0, bus.log_count);
    CHECK_EQ(AW32257_LIFECYCLE_BOUND, aw32257_get_lifecycle(&device));

    CHECK_EQ(AW32257_ERR_NULL_POINTER,
             aw32257_power_on_init(&device, NULL, &info));
    CHECK_EQ(0, bus.log_count);
    CHECK_EQ(AW32257_LIFECYCLE_POR_REQUIRED,
             aw32257_get_lifecycle(&device));

    mock_init(&bus);
    CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
    safety.max_charge_current = (aw32257_current_code_t)16;
    safety.max_charge_voltage_mv = 4240U;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_power_on_init(&device, &safety, &info));
    CHECK_EQ(0, bus.log_count);
    CHECK_EQ(AW32257_LIFECYCLE_POR_REQUIRED,
             aw32257_get_lifecycle(&device));

    mock_init(&bus);
    CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
    safety.max_charge_current = AW32257_CURRENT_CODE_05;
    safety.max_charge_voltage_mv = 4210U;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_power_on_init(&device, &safety, &info));
    CHECK_EQ(0, bus.log_count);
    CHECK_EQ(AW32257_LIFECYCLE_POR_REQUIRED,
             aw32257_get_lifecycle(&device));

    mock_init(&bus);
    CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
    safety.max_charge_voltage_mv = 4240U;
    CHECK_EQ(AW32257_OK,
             aw32257_power_on_init(&device, &safety, &info));
    CHECK_EQ(3, bus.log_count);
    CHECK_EQ(MOCK_LOG_WRITE, bus.log[0].type);
    CHECK_EQ(AW32257_REG_SAFETY_LIMIT, bus.log[0].reg);
    CHECK_EQ(0x52, bus.log[0].value);
    CHECK_EQ(MOCK_LOG_READ, bus.log[1].type);
    CHECK_EQ(AW32257_REG_SAFETY_LIMIT, bus.log[1].reg);
    CHECK_EQ(MOCK_LOG_READ, bus.log[2].type);
    CHECK_EQ(AW32257_REG_DEVICE_ID, bus.log[2].reg);
    CHECK_EQ(AW32257_I2C_ADDRESS_7BIT, bus.log[0].address);
    CHECK_EQ(25, bus.log[0].timeout_ms);
    CHECK_EQ(2, info.vendor_code);
    CHECK_EQ(2, info.part_code);
    CHECK_EQ(3, info.revision_code);
    CHECK_EQ(AW32257_LIFECYCLE_READY, aw32257_get_lifecycle(&device));
    CHECK_EQ(AW32257_ERR_STATE,
             aw32257_power_on_init(&device, &safety, NULL));
}

static void test_init_failures_and_future_revision(void)
{
    mock_bus_t bus;
    aw32257_t device;
    aw32257_safety_config_t safety;
    aw32257_device_info_t info;
    uint8_t value;
    uint8_t revision;
    int32_t fail_index;

    safety.max_charge_current = AW32257_CURRENT_CODE_05;
    safety.max_charge_voltage_mv = 4240U;

    mock_init(&bus);
    bus.safety_locked = true;
    CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
    CHECK_EQ(AW32257_ERR_SAFETY_MISMATCH,
             aw32257_power_on_init(&device, &safety, NULL));
    CHECK_EQ(AW32257_LIFECYCLE_POR_REQUIRED,
             aw32257_get_lifecycle(&device));
    CHECK_EQ(AW32257_ERR_POR_REQUIRED,
             aw32257_read_register(&device, AW32257_REG_DEVICE_ID, &value));

    for (fail_index = 1; fail_index <= 3; fail_index++)
    {
        mock_init(&bus);
        CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
        bus.fail_on_i2c_call = fail_index;
        bus.fail_code = -1234;
        CHECK_EQ(AW32257_ERR_IO,
                 aw32257_power_on_init(&device, &safety, NULL));
        CHECK_EQ(fail_index, bus.i2c_call_count);
        CHECK_EQ(-1234, aw32257_get_last_port_error(&device));
        CHECK_EQ(AW32257_LIFECYCLE_POR_REQUIRED,
                 aw32257_get_lifecycle(&device));
    }

    mock_init(&bus);
    bus.regs[AW32257_REG_DEVICE_ID] = 0x40U;
    CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
    CHECK_EQ(AW32257_ERR_ID_MISMATCH,
             aw32257_power_on_init(&device, &safety, NULL));
    CHECK_EQ(AW32257_LIFECYCLE_POR_REQUIRED,
             aw32257_get_lifecycle(&device));

    for (revision = 0U; revision < 8U; revision++)
    {
        mock_init(&bus);
        bus.regs[AW32257_REG_DEVICE_ID] = (uint8_t)(0x50U | revision);
        CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
        CHECK_EQ(AW32257_OK,
                 aw32257_power_on_init(&device, &safety, &info));
        CHECK_EQ(revision, info.revision_code);
    }
}

static void test_safety_encodings(void)
{
    mock_bus_t bus;
    aw32257_t device;
    aw32257_safety_config_t safety;
    uint8_t code;

    for (code = 0U; code < 16U; code++)
    {
        mock_init(&bus);
        CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
        safety.max_charge_current = (aw32257_current_code_t)code;
        safety.max_charge_voltage_mv = (uint16_t)(4200U +
                                                   ((uint16_t)code * 20U));
        CHECK_EQ(AW32257_OK,
                 aw32257_power_on_init(&device, &safety, NULL));
        CHECK_EQ((uint8_t)((code << 4) | code),
                 bus.regs[AW32257_REG_SAFETY_LIMIT]);
        CHECK_EQ(AW32257_REG_SAFETY_LIMIT, bus.log[0].reg);
    }
}

static void test_register_access_and_rmw(void)
{
    mock_bus_t bus;
    aw32257_t device;
    uint8_t value;

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));
    mock_clear_log(&bus);

    CHECK_EQ(AW32257_ERR_NULL_POINTER,
             aw32257_read_register(&device, AW32257_REG_CONTROL, NULL));
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_read_register(&device, 0x0BU, &value));
    CHECK_EQ(0, bus.i2c_call_count);

    bus.regs[AW32257_REG_CONTROL] = 0xF8U;
    CHECK_EQ(AW32257_OK, aw32257_set_charge_enabled(&device, false));
    CHECK_EQ(2, bus.i2c_call_count);
    CHECK_EQ(0xFC, bus.log[1].value);
    CHECK_EQ(0xFC, bus.regs[AW32257_REG_CONTROL]);

    mock_clear_log(&bus);
    CHECK_EQ(AW32257_OK, aw32257_set_charge_enabled(&device, false));
    CHECK_EQ(1, bus.i2c_call_count);
    CHECK_EQ(1, bus.log_count);

    mock_clear_log(&bus);
    bus.fail_on_i2c_call = 1;
    bus.fail_code = -88;
    CHECK_EQ(AW32257_ERR_IO,
             aw32257_set_termination_enabled(&device, true));
    CHECK_EQ(1, bus.i2c_call_count);
    CHECK_EQ(1, bus.log_count);
    CHECK_EQ(-88, aw32257_get_last_port_error(&device));

    mock_clear_log(&bus);
    bus.regs[AW32257_REG_CONTROL] = 0xF0U;
    bus.fail_on_i2c_call = 2;
    bus.fail_code = -89;
    CHECK_EQ(AW32257_ERR_IO,
             aw32257_set_termination_enabled(&device, true));
    CHECK_EQ(2, bus.i2c_call_count);
    CHECK_EQ(2, bus.log_count);
    CHECK_EQ(0xF0, bus.regs[AW32257_REG_CONTROL]);
    CHECK_EQ(-89, aw32257_get_last_port_error(&device));

    mock_clear_log(&bus);
    bus.regs[AW32257_REG_DPM_STATUS] = 0xF8U;
    CHECK_EQ(AW32257_OK, aw32257_set_dpm_voltage_mv(&device, 4550U));
    CHECK_EQ(0xFC, bus.log[1].value);

    mock_clear_log(&bus);
    bus.regs[AW32257_REG_STATUS_CONTROL] = 0xFFU;
    CHECK_EQ(AW32257_OK,
             aw32257_set_stat_output_enabled(&device, false));
    CHECK_EQ(0xBF, bus.log[1].value);
    CHECK_EQ(AW32257_OK,
             aw32257_set_stat_output_enabled(&device, true));
    CHECK_EQ(0xFF, bus.regs[AW32257_REG_STATUS_CONTROL]);

    bus.regs[AW32257_REG_CONTROL] = 0xF0U;
    CHECK_EQ(AW32257_OK,
             aw32257_set_termination_enabled(&device, true));
    CHECK_EQ(0xF8, bus.regs[AW32257_REG_CONTROL]);
    CHECK_EQ(AW32257_OK,
             aw32257_set_termination_enabled(&device, false));
    CHECK_EQ(0xF0, bus.regs[AW32257_REG_CONTROL]);
    bus.regs[AW32257_REG_CONTROL] = 0xF4U;
    CHECK_EQ(AW32257_OK, aw32257_set_charge_enabled(&device, true));
    CHECK_EQ(0xF0, bus.regs[AW32257_REG_CONTROL]);
}

static void test_value_encodings_and_current_tables(void)
{
    static const uint16_t expected_current[16] =
    {
        496, 620, 868, 992, 1116, 1240, 1364, 1488,
        1612, 1736, 1860, 1984, 2108, 2232, 2356, 2480
    };
    static const uint16_t expected_term[8] =
    {
        62, 124, 186, 248, 310, 372, 434, 496
    };
    mock_bus_t bus;
    aw32257_t device;
    uint16_t value;
    uint8_t code;

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));
    mock_clear_log(&bus);

    CHECK_EQ(AW32257_OK, aw32257_set_charge_voltage_mv(&device, 3500U));
    CHECK_EQ(0x02, bus.regs[AW32257_REG_BATTERY_VOLTAGE]);
    CHECK_EQ(AW32257_OK, aw32257_set_charge_voltage_mv(&device, 4500U));
    CHECK_EQ(0xCA, bus.regs[AW32257_REG_BATTERY_VOLTAGE]);

    mock_clear_log(&bus);
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_charge_voltage_mv(&device, 3499U));
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_charge_voltage_mv(&device, 3510U));
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_charge_voltage_mv(&device, 4501U));
    CHECK_EQ(0, bus.i2c_call_count);

    for (code = 0U; code < 8U; code++)
    {
        bus.regs[AW32257_REG_DPM_STATUS] = 0xF8U;
        CHECK_EQ(AW32257_OK,
                 aw32257_set_dpm_voltage_mv(
                     &device,
                     (uint16_t)(4250U + ((uint16_t)code * 75U))));
        CHECK_EQ((uint8_t)(0xF8U | code),
                 bus.regs[AW32257_REG_DPM_STATUS]);
    }
    mock_clear_log(&bus);
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_dpm_voltage_mv(&device, 4175U));
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_dpm_voltage_mv(&device, 4300U));
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_dpm_voltage_mv(&device, 4850U));
    CHECK_EQ(0, bus.i2c_call_count);

    for (code = 0U; code < 16U; code++)
    {
        CHECK_EQ(AW32257_OK,
                 aw32257_current_code_to_ma_33mohm(
                     (aw32257_current_code_t)code,
                     &value));
        CHECK_EQ(expected_current[code], value);
    }
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_current_code_to_ma_33mohm(
                 (aw32257_current_code_t)16,
                 &value));
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_current_code_to_ma_33mohm(
                 (aw32257_current_code_t)-1,
                 &value));
    CHECK_EQ(AW32257_ERR_NULL_POINTER,
             aw32257_current_code_to_ma_33mohm(
                 AW32257_CURRENT_CODE_00,
                 NULL));

    for (code = 0U; code < 8U; code++)
    {
        CHECK_EQ(AW32257_OK,
                 aw32257_termination_current_code_to_ma_33mohm(
                     (aw32257_term_current_code_t)code,
                     &value));
        CHECK_EQ(expected_term[code], value);
    }
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_termination_current_code_to_ma_33mohm(
                 (aw32257_term_current_code_t)-1,
                 &value));

    mock_clear_log(&bus);
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_fast_charge_current(
                 &device,
                 (aw32257_current_code_t)-1));
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_termination_current(
                 &device,
                 (aw32257_term_current_code_t)8));
    CHECK_EQ(0, bus.i2c_call_count);

    for (code = 0U; code < 16U; code++)
    {
        bus.regs[AW32257_REG_CHARGE_CURRENT] = 0x87U;
        CHECK_EQ(AW32257_OK,
                 aw32257_set_fast_charge_current(
                     &device,
                     (aw32257_current_code_t)code));
        CHECK_EQ((uint8_t)((code << AW32257_REG04_FAST_CURRENT_SHIFT) |
                           AW32257_REG04_TERM_CURRENT_MASK),
                 bus.regs[AW32257_REG_CHARGE_CURRENT]);
    }
    for (code = 0U; code < 8U; code++)
    {
        bus.regs[AW32257_REG_CHARGE_CURRENT] = 0xF8U;
        CHECK_EQ(AW32257_OK,
                 aw32257_set_termination_current(
                     &device,
                     (aw32257_term_current_code_t)code));
        CHECK_EQ((uint8_t)(AW32257_REG04_FAST_CURRENT_MASK | code),
                 bus.regs[AW32257_REG_CHARGE_CURRENT]);
    }

    bus.regs[AW32257_REG_CHARGE_CURRENT] = 0x80U;
    CHECK_EQ(AW32257_OK,
             aw32257_set_fast_charge_current(&device,
                                              AW32257_CURRENT_CODE_0F));
    CHECK_EQ(0x78, bus.regs[AW32257_REG_CHARGE_CURRENT]);
    CHECK_EQ(AW32257_OK,
             aw32257_set_termination_current(
                 &device,
                 AW32257_TERM_CURRENT_CODE_7));
    CHECK_EQ(0x7F, bus.regs[AW32257_REG_CHARGE_CURRENT]);
}

static void test_termination_config(void)
{
    static const uint8_t windows[2] = {8U, 16U};
    static const uint8_t valid_periods[4] = {1U, 2U, 4U, 8U};
    static const uint8_t deglitch_times[4] = {8U, 16U, 32U, 64U};
    static const uint16_t recharge_thresholds[4] = {50U, 100U, 150U, 200U};
    mock_bus_t bus;
    aw32257_t device;
    aw32257_termination_config_t config;
    uint8_t expected;
    uint8_t window_code;
    uint8_t valid_code;
    uint8_t deglitch_code;
    uint8_t recharge_code;

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));
    for (window_code = 0U; window_code < 2U; window_code++)
    {
        for (valid_code = 0U; valid_code < 4U; valid_code++)
        {
            for (deglitch_code = 0U; deglitch_code < 4U; deglitch_code++)
            {
                for (recharge_code = 0U; recharge_code < 4U; recharge_code++)
                {
                    config.window_periods = windows[window_code];
                    config.valid_periods = valid_periods[valid_code];
                    config.deglitch_ms = deglitch_times[deglitch_code];
                    config.recharge_threshold_mv =
                        recharge_thresholds[recharge_code];
                    bus.regs[AW32257_REG_TERMINATION] = 0x04U;
                    mock_clear_log(&bus);

                    if (((uint16_t)config.valid_periods *
                         (uint16_t)config.deglitch_ms) > 256U)
                    {
                        CHECK_EQ(AW32257_ERR_RANGE,
                                 aw32257_set_termination_config(&device,
                                                                &config));
                        CHECK_EQ(0, bus.i2c_call_count);
                    }
                    else
                    {
                        expected = (uint8_t)(
                            0x04U |
                            (window_code << 7) |
                            (valid_code << AW32257_REG07_VALID_PERIODS_SHIFT) |
                            (deglitch_code << AW32257_REG07_DEGLITCH_SHIFT) |
                            recharge_code);
                        CHECK_EQ(AW32257_OK,
                                 aw32257_set_termination_config(&device,
                                                                &config));
                        CHECK_EQ(expected,
                                 bus.regs[AW32257_REG_TERMINATION]);
                    }
                }
            }
        }
    }

    config.window_periods = 12U;
    config.valid_periods = 4U;
    config.deglitch_ms = 32U;
    config.recharge_threshold_mv = 100U;
    mock_clear_log(&bus);
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_termination_config(&device, &config));
    config.window_periods = 8U;
    config.valid_periods = 3U;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_termination_config(&device, &config));
    config.valid_periods = 2U;
    config.deglitch_ms = 24U;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_termination_config(&device, &config));
    config.deglitch_ms = 16U;
    config.recharge_threshold_mv = 75U;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_termination_config(&device, &config));
    CHECK_EQ(0, bus.i2c_call_count);
    CHECK_EQ(AW32257_ERR_NULL_POINTER,
             aw32257_set_termination_config(&device, NULL));
}

static void test_boost_modes_and_otg(void)
{
    mock_bus_t bus;
    aw32257_t device;
    aw32257_boost_config_t boost;
    uint8_t enabled;
    uint8_t active_high;

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));

    bus.regs[AW32257_REG_BOOST_CONFIG] = 0x04U;
    boost.output_voltage_mv = 5350U;
    boost.frequency_khz = 1700U;
    boost.slew_rate = AW32257_SLEW_RATE_SLOWEST;
    boost.fixed_dead_time = true;
    boost.force_pwm = true;
    CHECK_EQ(AW32257_OK, aw32257_set_boost_config(&device, &boost));
    CHECK_EQ(0xFF, bus.regs[AW32257_REG_BOOST_CONFIG]);

    boost.output_voltage_mv = 5050U;
    boost.frequency_khz = 1500U;
    boost.slew_rate = AW32257_SLEW_RATE_DEFAULT;
    boost.fixed_dead_time = false;
    boost.force_pwm = false;
    CHECK_EQ(AW32257_OK, aw32257_set_boost_config(&device, &boost));
    CHECK_EQ(0x04, bus.regs[AW32257_REG_BOOST_CONFIG]);

    mock_clear_log(&bus);
    boost.frequency_khz = 1600U;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_boost_config(&device, &boost));
    CHECK_EQ(0, bus.i2c_call_count);
    CHECK_EQ(AW32257_ERR_NULL_POINTER,
             aw32257_set_boost_config(&device, NULL));

    boost.frequency_khz = 1500U;
    boost.slew_rate = (aw32257_slew_rate_t)-1;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_boost_config(&device, &boost));
    boost.slew_rate = AW32257_SLEW_RATE_DEFAULT;
    boost.output_voltage_mv = 5100U;
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_boost_config(&device, &boost));
    CHECK_EQ(0, bus.i2c_call_count);

    bus.regs[AW32257_REG_CONTROL] = 0xFCU;
    CHECK_EQ(AW32257_OK, aw32257_set_mode(&device, AW32257_MODE_BOOST));
    CHECK_EQ(0xFD, bus.regs[AW32257_REG_CONTROL]);
    CHECK_EQ(AW32257_OK,
             aw32257_set_mode(&device, AW32257_MODE_HIGH_IMPEDANCE));
    CHECK_EQ(0xFE, bus.regs[AW32257_REG_CONTROL]);
    CHECK_EQ(AW32257_OK, aw32257_set_mode(&device, AW32257_MODE_CHARGE));
    CHECK_EQ(0xFC, bus.regs[AW32257_REG_CONTROL]);
    CHECK_EQ(AW32257_ERR_RANGE,
             aw32257_set_mode(&device, (aw32257_mode_t)99));

    for (enabled = 0U; enabled < 2U; enabled++)
    {
        for (active_high = 0U; active_high < 2U; active_high++)
        {
            bus.regs[AW32257_REG_BATTERY_VOLTAGE] = 0xFCU;
            CHECK_EQ(AW32257_OK,
                     aw32257_configure_otg_pin(&device,
                                                enabled != 0U,
                                                active_high != 0U));
            CHECK_EQ((uint8_t)(0xFCU | enabled | (active_high << 1)),
                     bus.regs[AW32257_REG_BATTERY_VOLTAGE]);
        }
    }
}

static void test_status_and_configuration_snapshots(void)
{
    mock_bus_t bus;
    aw32257_t device;
    aw32257_status_snapshot_t status;
    aw32257_status_snapshot_t status_before;
    aw32257_config_snapshot_t config;
    aw32257_config_snapshot_t config_before;
    uint8_t fault_code;
    uint8_t voltage_code;

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));

    bus.regs[AW32257_REG_STATUS_CONTROL] = 0xFFU;
    bus.regs[AW32257_REG_DPM_STATUS] = 0x18U;
    bus.regs[AW32257_REG_BOOST_FAULT] = 0x05U;
    CHECK_EQ(AW32257_OK, aw32257_read_status(&device, &status));
    CHECK_TRUE(status.otg_pin_high);
    CHECK_TRUE(status.stat_output_enabled);
    CHECK_EQ(AW32257_CHARGE_STATE_FAULT, status.charge_state);
    CHECK_TRUE(status.boost_active);
    CHECK_EQ(AW32257_CHARGE_FAULT_NO_BATTERY, status.charge_fault);
    CHECK_TRUE(status.dpm_active);
    CHECK_TRUE(status.cd_pin_high);
    CHECK_EQ(AW32257_BOOST_FAULT_THERMAL_SHUTDOWN, status.boost_fault);

    for (fault_code = 0U; fault_code < 8U; fault_code++)
    {
        bus.regs[AW32257_REG_STATUS_CONTROL] = fault_code;
        bus.regs[AW32257_REG_BOOST_FAULT] = fault_code;
        CHECK_EQ(AW32257_OK, aw32257_read_status(&device, &status));
        CHECK_EQ(fault_code, status.charge_fault);
        CHECK_EQ(fault_code, status.boost_fault);
    }

    memset(&status, 0xA5, sizeof(status));
    status_before = status;
    mock_clear_log(&bus);
    bus.fail_on_i2c_call = 2;
    CHECK_EQ(AW32257_ERR_IO, aw32257_read_status(&device, &status));
    CHECK_EQ(0, memcmp(&status, &status_before, sizeof(status)));

    bus.regs[AW32257_REG_STATUS_CONTROL] = 0x40U;
    bus.regs[AW32257_REG_CONTROL] = 0x3BU;
    bus.regs[AW32257_REG_BATTERY_VOLTAGE] = 0xCFU;
    bus.regs[AW32257_REG_CHARGE_CURRENT] = 0x7FU;
    bus.regs[AW32257_REG_DPM_STATUS] = 0x07U;
    bus.regs[AW32257_REG_SAFETY_LIMIT] = 0xFFU;
    bus.regs[AW32257_REG_TERMINATION] = 0xFBU;
    bus.regs[AW32257_REG_BOOST_CONFIG] = 0xFBU;
    mock_clear_log(&bus);
    CHECK_EQ(AW32257_OK,
             aw32257_read_configuration(&device, &config));
    CHECK_EQ(4500, config.charge_voltage_mv);
    CHECK_TRUE(config.termination_enabled);
    CHECK_TRUE(config.charge_enabled);
    CHECK_TRUE(config.high_impedance_requested);
    CHECK_TRUE(config.boost_requested);
    CHECK_TRUE(config.otg_active_high);
    CHECK_TRUE(config.otg_pin_control_enabled);
    CHECK_EQ(AW32257_CURRENT_CODE_0F, config.fast_charge_current);
    CHECK_EQ(AW32257_TERM_CURRENT_CODE_7, config.termination_current);
    CHECK_EQ(4775, config.dpm_voltage_mv);
    CHECK_EQ(AW32257_CURRENT_CODE_0F, config.max_charge_current);
    CHECK_EQ(4500, config.max_charge_voltage_mv);
    CHECK_EQ(16, config.termination.window_periods);
    CHECK_EQ(8, config.termination.valid_periods);
    CHECK_EQ(64, config.termination.deglitch_ms);
    CHECK_EQ(200, config.termination.recharge_threshold_mv);
    CHECK_EQ(5350, config.boost.output_voltage_mv);
    CHECK_EQ(1700, config.boost.frequency_khz);
    CHECK_EQ(AW32257_SLEW_RATE_SLOWEST, config.boost.slew_rate);
    CHECK_TRUE(config.boost.fixed_dead_time);
    CHECK_TRUE(config.boost.force_pwm);

    for (voltage_code = 0x32U; voltage_code <= 0x3FU; voltage_code++)
    {
        bus.regs[AW32257_REG_BATTERY_VOLTAGE] =
            (uint8_t)((voltage_code << AW32257_REG02_VOREG_SHIFT) | 0x03U);
        CHECK_EQ(AW32257_OK,
                 aw32257_read_configuration(&device, &config));
        CHECK_EQ(4500, config.charge_voltage_mv);
    }

    memset(&config, 0x5A, sizeof(config));
    config_before = config;
    mock_clear_log(&bus);
    bus.fail_on_i2c_call = 4;
    CHECK_EQ(AW32257_ERR_IO,
             aw32257_read_configuration(&device, &config));
    CHECK_EQ(0, memcmp(&config, &config_before, sizeof(config)));
}

static void test_soft_reset_ordering(void)
{
    mock_bus_t bus;
    aw32257_t device;

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));
    mock_clear_log(&bus);
    bus.regs[AW32257_REG_STATUS_CONTROL] = 0x40U;
    CHECK_EQ(AW32257_OK, aw32257_soft_reset(&device));
    CHECK_EQ(3, bus.log_count);
    CHECK_EQ(MOCK_LOG_READ, bus.log[0].type);
    CHECK_EQ(AW32257_REG_STATUS_CONTROL, bus.log[0].reg);
    CHECK_EQ(MOCK_LOG_WRITE, bus.log[1].type);
    CHECK_EQ(AW32257_REG_CHARGE_CURRENT, bus.log[1].reg);
    CHECK_EQ(AW32257_REG04_SOFT_RESET_MASK, bus.log[1].value);
    CHECK_EQ(MOCK_LOG_DELAY, bus.log[2].type);
    CHECK_EQ(AW32257_SOFT_RESET_DELAY_MS, bus.log[2].timeout_ms);
    CHECK_EQ(32, bus.now_ms);
    CHECK_EQ(32, bus.reset_ready_at_ms);

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));
    mock_clear_log(&bus);
    bus.regs[AW32257_REG_STATUS_CONTROL] = 0x40U;
    bus.fail_on_i2c_call = 2;
    bus.fail_code = -222;
    CHECK_EQ(AW32257_ERR_IO, aw32257_soft_reset(&device));
    CHECK_EQ(3, bus.log_count);
    CHECK_EQ(MOCK_LOG_DELAY, bus.log[2].type);
    CHECK_EQ(32, bus.now_ms);
    CHECK_EQ(-222, aw32257_get_last_port_error(&device));

    CHECK_EQ(AW32257_OK, mock_prepare_ready(&bus, &device));
    mock_clear_log(&bus);
    bus.regs[AW32257_REG_STATUS_CONTROL] = 0x50U;
    CHECK_EQ(AW32257_ERR_STATE, aw32257_soft_reset(&device));
    CHECK_EQ(1, bus.log_count);

    mock_clear_log(&bus);
    bus.regs[AW32257_REG_STATUS_CONTROL] = 0x48U;
    CHECK_EQ(AW32257_ERR_STATE, aw32257_soft_reset(&device));
    CHECK_EQ(1, bus.log_count);
}

static void test_unready_states(void)
{
    mock_bus_t bus;
    aw32257_t device;
    uint8_t value;

    memset(&device, 0, sizeof(device));
    CHECK_EQ(AW32257_ERR_NOT_BOUND,
             aw32257_read_register(&device, AW32257_REG_DEVICE_ID, &value));
    CHECK_EQ(AW32257_LIFECYCLE_UNBOUND, aw32257_get_lifecycle(NULL));
    CHECK_EQ(0, aw32257_get_last_port_error(NULL));

    mock_init(&bus);
    CHECK_EQ(AW32257_OK, aw32257_init(&device, &bus, 25U));
    CHECK_EQ(AW32257_ERR_NOT_INITIALIZED,
             aw32257_read_register(&device, AW32257_REG_DEVICE_ID, &value));
}


int main(void)
{
    test_bind_and_power_on_sequence();
    test_init_failures_and_future_revision();
    test_safety_encodings();
    test_register_access_and_rmw();
    test_value_encodings_and_current_tables();
    test_termination_config();
    test_boost_modes_and_otg();
    test_status_and_configuration_snapshots();
    test_soft_reset_ordering();
    test_unready_states();

    if (g_failures == 0)
    {
        printf("AW32257 tests: PASS (%d checks)\n", g_checks);
        return 0;
    }

    printf("AW32257 tests: FAIL (%d failures, %d checks)\n",
           g_failures,
           g_checks);
    return 1;
}
