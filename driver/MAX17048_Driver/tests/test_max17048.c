/**
 * @file    test_max17048.c
 * @brief   Mock-I2C unit tests for the portable MAX17048 driver core
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 *
 * @details
 * Implements the max17048_io.h contract against an in-memory register image
 * with hardware-faithful semantics (STATUS write-1-to-clear, VRESET OTP ID
 * low byte, read-only registers), then exercises the public API. Designed to
 * run on the host or under the MounRiver RV32 simulator (riscv32-wch-elf-run).
 * Exit code 0 = all checks passed.
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "max17048.h"
#include "max17048_io.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 极简断言框架
 * ════════════════════════════════════════════════════════════════════════ */

static int g_failures;
static int g_checks;

static void check_equal(long expected, long actual, const char *expression,
                        const char *function, int line)
{
    g_checks++;
    if (expected != actual)
    {
        g_failures++;
        printf("FAIL %s:%d %s: expected %ld, got %ld\n", function, line,
               expression, expected, actual);
    }
}

static void check_true(bool condition, const char *expression,
                       const char *function, int line)
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

/* ══════════════════════════════════════════════════════════════════════════
 * 模拟 I2C 器件（寄存器镜像 + 故障注入 + 写序列记录）
 * ════════════════════════════════════════════════════════════════════════ */

#define MOCK_REG_SPACE      256u
#define MOCK_LOG_CAPACITY   16u

typedef struct {
    uint8_t is_write;      /**< 1=写事务 0=读事务 */
    uint8_t reg;           /**< 寄存器地址 */
    uint16_t val;          /**< 写入值 / 读出值 */
} mock_log_entry_t;

typedef struct {
    uint16_t regs[MOCK_REG_SPACE];
    mock_log_entry_t log[MOCK_LOG_CAPACITY];
    uint32_t log_count;
    uint32_t total_writes;
    uint32_t total_reads;
    uint32_t delay_ms_sum;
    int fail_next_read;    /**< 下一次读返回失败 */
    int fail_next_write;   /**< 下一次写返回失败 */
    int fail_write_at;     /**< 第 N 笔写返回失败（1 起，0 不启用） */
    int corrupt_reads_left; /**< 接下来 N 次读返回篡改值 */
    uint16_t corrupt_value;
} mock_bus_t;

static mock_bus_t s_bus;

static void mock_log_push(uint8_t is_write, uint8_t reg, uint16_t val)
{
    if (s_bus.log_count < MOCK_LOG_CAPACITY)
    {
        s_bus.log[s_bus.log_count].is_write = is_write;
        s_bus.log[s_bus.log_count].reg = reg;
        s_bus.log[s_bus.log_count].val = val;
        s_bus.log_count++;
    }
}

/** 按 Table 2 加载 POR 默认值 */
static void mock_load_por(void)
{
    uint16_t i;

    for (i = 0u; i < MOCK_REG_SPACE; i++)
    {
        s_bus.regs[i] = 0xFFFFu;
    }
    s_bus.regs[MAX17048_REG_MODE] = MAX17048_MODE_POR;
    s_bus.regs[MAX17048_REG_VERSION] = 0x0013u;
    s_bus.regs[MAX17048_REG_HIBRT] = MAX17048_HIBRT_POR;
    s_bus.regs[MAX17048_REG_CONFIG] = MAX17048_CONFIG_POR;
    s_bus.regs[MAX17048_REG_VALRT] = MAX17048_VALRT_POR;
    s_bus.regs[MAX17048_REG_VRESET] = 0x9655u;    /* 低字节 OTP ID = 0x55 */
    s_bus.regs[MAX17048_REG_STATUS] = MAX17048_STATUS_POR;
}

static void mock_reset(void)
{
    uint32_t i;

    for (i = 0u; i < MOCK_REG_SPACE; i++)
    {
        s_bus.regs[i] = 0u;
    }
    for (i = 0u; i < MOCK_LOG_CAPACITY; i++)
    {
        s_bus.log[i].is_write = 0u;
        s_bus.log[i].reg = 0u;
        s_bus.log[i].val = 0u;
    }
    s_bus.log_count = 0u;
    s_bus.total_writes = 0u;
    s_bus.total_reads = 0u;
    s_bus.delay_ms_sum = 0u;
    s_bus.fail_next_read = 0;
    s_bus.fail_next_write = 0;
    s_bus.fail_write_at = 0;
    s_bus.corrupt_reads_left = 0;
    s_bus.corrupt_value = 0u;
    mock_load_por();
}

/* ── io 契约实现：直接操作寄存器镜像 ─────────────────────────────── */

int max17048_io_init(void)
{
    return MAX17048_IO_OK;
}

int max17048_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                           uint16_t *val)
{
    (void)io_ctx;
    (void)dev_addr;

    if (val == NULL)
    {
        return MAX17048_IO_ERROR;
    }
    s_bus.total_reads++;
    if (s_bus.fail_next_read)
    {
        s_bus.fail_next_read = 0;
        return MAX17048_IO_ERROR;
    }
    *val = s_bus.regs[reg];
    if (s_bus.corrupt_reads_left > 0)
    {
        s_bus.corrupt_reads_left--;
        *val = s_bus.corrupt_value;
    }
    mock_log_push(0u, reg, *val);
    return MAX17048_IO_OK;
}

int max17048_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                            uint16_t val)
{
    uint16_t old;

    (void)io_ctx;
    (void)dev_addr;

    s_bus.total_writes++;
    if (s_bus.fail_next_write)
    {
        s_bus.fail_next_write = 0;
        /* CMD 复位命令在最后一位时钟移入后即生效，仅 ACK 缺失，
         * 因此注入失败时仍记录该写入 */
        if (reg == MAX17048_REG_CMD)
        {
            s_bus.regs[reg] = val;
        }
        return MAX17048_IO_ERROR;
    }
    if ((s_bus.fail_write_at > 0) &&
        ((int)s_bus.total_writes == s_bus.fail_write_at))
    {
        return MAX17048_IO_ERROR;
    }

    old = s_bus.regs[reg];

    if (reg == MAX17048_REG_STATUS)
    {
        /* 告警位写 1 清除（写 0 保持）；使能位 EnVR 正常读写 */
        s_bus.regs[reg] = (uint16_t)(old &
                                     (uint16_t)~(uint16_t)(val &
                                         MAX17048_STATUS_ALERT_MASK));
        s_bus.regs[reg] =
            (uint16_t)((s_bus.regs[reg] & (uint16_t)~MAX17048_STATUS_ENVR) |
                       (uint16_t)(val & MAX17048_STATUS_ENVR));
    }
    else if (reg == MAX17048_REG_VRESET)
    {
        /* 低字节为 OTP ID，写入被忽略 */
        s_bus.regs[reg] = (uint16_t)((val & 0xFF00u) | (old & 0x00FFu));
    }
    else if ((reg == MAX17048_REG_VCELL) || (reg == MAX17048_REG_SOC) ||
             (reg == MAX17048_REG_VERSION) || (reg == MAX17048_REG_CRATE))
    {
        /* 只读寄存器，写入被忽略 */
    }
    else
    {
        s_bus.regs[reg] = val;
    }

    mock_log_push(1u, reg, val);
    return MAX17048_IO_OK;
}

void max17048_io_delay_ms(uint32_t ms)
{
    s_bus.delay_ms_sum += ms;
}

#if MAX17048_THREAD_SAFE
/* 单线程测试环境下的空实现 */
void max17048_io_lock(void)  { }
void max17048_io_unlock(void) { }
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 测试用例
 * ════════════════════════════════════════════════════════════════════════ */

static max17048_dev_t t_dev;

static void test_init_and_param(void)
{
    max17048_result_t res;
    uint32_t mv;

    mock_reset();

    /* 未初始化先访问 → NOT_READY；空指针 → PARAM */
    res = max17048_read_vcell(&t_dev, &mv);
    CHECK_EQ(MAX17048_ERR_NOT_READY, res);
    res = max17048_read_vcell(&t_dev, NULL);
    CHECK_EQ(MAX17048_ERR_PARAM, res);
    res = max17048_init(NULL, NULL, MAX17048_I2C_ADDR);
    CHECK_EQ(MAX17048_ERR_PARAM, res);

    /* 正常初始化：VERSION 校验通过，POR 后 STATUS.RI 被清除 */
    res = max17048_init(&t_dev, NULL, MAX17048_I2C_ADDR);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0, s_bus.regs[MAX17048_REG_STATUS] & MAX17048_STATUS_RI);

    /* 器件在位但读失败 → ERR_IO */
    mock_reset();
    t_dev.inited = 0u;
    s_bus.fail_next_read = 1;
    res = max17048_init(&t_dev, NULL, MAX17048_I2C_ADDR);
    CHECK_EQ(MAX17048_ERR_IO, res);

    /* 版本不符 → ERR_NOT_READY */
    mock_reset();
    s_bus.regs[MAX17048_REG_VERSION] = 0x0FF0u;
    res = max17048_init(&t_dev, NULL, MAX17048_I2C_ADDR);
    CHECK_EQ(MAX17048_ERR_NOT_READY, res);

    /* 重新初始化成功供后续用例使用 */
    mock_reset();
    res = max17048_init(&t_dev, NULL, MAX17048_I2C_ADDR);
    CHECK_EQ(MAX17048_OK, res);
}

static void test_measurements(void)
{
    max17048_result_t res;
    uint32_t mv;
    uint16_t raw16;
    uint8_t percent;
    uint16_t percent_x100;
    int16_t tenth;
    int16_t raw_i;

    /* VCELL：raw=40000 → 40000×5/64 = 3125mV */
    s_bus.regs[MAX17048_REG_VCELL] = 40000u;
    res = max17048_read_vcell(&t_dev, &mv);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(3125, mv);
    res = max17048_read_vcell_raw(&t_dev, &raw16);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(40000, raw16);

    /* VCELL 舍入：raw=8191 → 8191×5/64 = 639.9 → 640 */
    s_bus.regs[MAX17048_REG_VCELL] = 8191u;
    res = max17048_read_vcell(&t_dev, &mv);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(640, mv);

    /* SOC：raw=0x3200 → 恰好 50% */
    s_bus.regs[MAX17048_REG_SOC] = 0x3200u;
    res = max17048_read_soc(&t_dev, &percent);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(50, percent);

    /* SOC 舍入：raw=0x2080（32.5%）→ 33 */
    s_bus.regs[MAX17048_REG_SOC] = 0x2080u;
    res = max17048_read_soc(&t_dev, &percent);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(33, percent);
    res = max17048_read_soc_precise(&t_dev, &percent_x100);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(3250, percent_x100);
    res = max17048_read_soc_raw(&t_dev, &raw16);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x2080, raw16);

    /* CRATE：raw=25 → 5.2%/h → 52 个 0.1%/h */
    s_bus.regs[MAX17048_REG_CRATE] = 25u;
    res = max17048_read_crate(&t_dev, &tenth);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(52, tenth);

    /* CRATE 负数：raw=-13 → -2.704%/h → -27 */
    s_bus.regs[MAX17048_REG_CRATE] = (uint16_t)-13;
    res = max17048_read_crate(&t_dev, &tenth);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(-27, tenth);
    res = max17048_read_crate_raw(&t_dev, &raw_i);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(-13, raw_i);
}

static void test_config_fields(void)
{
    max17048_result_t res;
    uint8_t percent;
    uint8_t rcomp;
    uint16_t min_mv;
    uint16_t max_mv;
    uint8_t hib;
    uint8_t act;

    /* ATHD：阈值 10% → ATHD=22 */
    res = max17048_set_soc_alert_threshold(&t_dev, 10u);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ((MAX17048_CONFIG_POR & (uint16_t)~MAX17048_CONFIG_ATHD_MASK) | 22u,
             s_bus.regs[MAX17048_REG_CONFIG]);
    res = max17048_get_soc_alert_threshold(&t_dev, &percent);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(10, percent);

    /* 钳位：0 → 1%，99 → 32% */
    res = max17048_set_soc_alert_threshold(&t_dev, 0u);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_get_soc_alert_threshold(&t_dev, &percent);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(1, percent);
    res = max17048_set_soc_alert_threshold(&t_dev, 99u);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_get_soc_alert_threshold(&t_dev, &percent);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(32, percent);

    /* 恢复 10% 并关闭 SOC 变化告警 */
    res = max17048_set_soc_alert_threshold(&t_dev, 10u);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_set_soc_change_alert(&t_dev, true);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(MAX17048_CONFIG_ALSC,
             s_bus.regs[MAX17048_REG_CONFIG] & MAX17048_CONFIG_ALSC);
    res = max17048_set_soc_change_alert(&t_dev, false);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0, s_bus.regs[MAX17048_REG_CONFIG] & MAX17048_CONFIG_ALSC);

    /* VALRT：3200~4200mV → 160/210 档 */
    res = max17048_set_voltage_alerts(&t_dev, 3200u, 4200u);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ((160u << 8) | 210u, s_bus.regs[MAX17048_REG_VALRT]);
    res = max17048_get_voltage_alerts(&t_dev, &min_mv, &max_mv);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(3200, min_mv);
    CHECK_EQ(4200, max_mv);

    /* 上限钳位 5100mV，下限 0mV */
    res = max17048_set_voltage_alerts(&t_dev, 0u, 9999u);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_get_voltage_alerts(&t_dev, &min_mv, &max_mv);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0, min_mv);
    CHECK_EQ(5100, max_mv);

    /* min > max → PARAM */
    res = max17048_set_voltage_alerts(&t_dev, 4200u, 3200u);
    CHECK_EQ(MAX17048_ERR_PARAM, res);

    /* HIBRT */
    res = max17048_hibernate_disable(&t_dev);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x0000, s_bus.regs[MAX17048_REG_HIBRT]);
    res = max17048_hibernate_force(&t_dev);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0xFFFF, s_bus.regs[MAX17048_REG_HIBRT]);
    res = max17048_hibernate_set_thresholds(&t_dev, 0x40u, 0x10u);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x4010, s_bus.regs[MAX17048_REG_HIBRT]);
    res = max17048_hibernate_get_thresholds(&t_dev, &hib, &act);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x40, hib);
    CHECK_EQ(0x10, act);

    /* RCOMP 直接读写 */
    res = max17048_set_rcomp(&t_dev, 0xA0u);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_get_rcomp(&t_dev, &rcomp);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0xA0, rcomp);
}

static void test_temp_compensate(void)
{
    max17048_result_t res;

    /* 30.0℃：0x97 + 10×(-0.5) = 0x97-5 = 0x92 */
    res = max17048_temp_compensate(&t_dev, 300);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x92, (s_bus.regs[MAX17048_REG_CONFIG] >>
                    MAX17048_CONFIG_RCOMP_SHIFT) & 0xFFu);

    /* 20.0℃：不变 0x97 */
    res = max17048_temp_compensate(&t_dev, 200);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x97, (s_bus.regs[MAX17048_REG_CONFIG] >>
                    MAX17048_CONFIG_RCOMP_SHIFT) & 0xFFu);

    /* 10.0℃：0x97 + (-10)×(-5.0) = 0x97+50 = 201 → 0xC9 */
    res = max17048_temp_compensate(&t_dev, 100);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0xC9, (s_bus.regs[MAX17048_REG_CONFIG] >>
                    MAX17048_CONFIG_RCOMP_SHIFT) & 0xFFu);

    /* -40.0℃：0x97 + (-60)×(-5.0) = 247 → 钳位 255 */
    res = max17048_temp_compensate(&t_dev, -400);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0xFF, (s_bus.regs[MAX17048_REG_CONFIG] >>
                    MAX17048_CONFIG_RCOMP_SHIFT) & 0xFFu);
}

static void test_vreset(void)
{
    max17048_result_t res;
    uint16_t mv;
    bool dis;

    /* 2500mV → 就近档 2520mV；低字节 OTP ID 0x55 保持不变 */
    res = max17048_set_vreset_threshold(&t_dev, 2500u, true);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ((63u << MAX17048_VRESET_SHIFT) | MAX17048_VRESET_DIS | 0x55u,
             s_bus.regs[MAX17048_REG_VRESET]);
    res = max17048_get_vreset_threshold(&t_dev, &mv, &dis);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(2520, mv);
    CHECK_EQ(true, dis);

    /* 钳位到 2280~3480mV */
    res = max17048_set_vreset_threshold(&t_dev, 1000u, false);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_get_vreset_threshold(&t_dev, &mv, &dis);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(2280, mv);
    res = max17048_set_vreset_threshold(&t_dev, 5000u, false);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_get_vreset_threshold(&t_dev, &mv, &dis);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(3480, mv);
    CHECK_EQ(false, dis);
}

static void test_sleep_and_hibstat(void)
{
    max17048_result_t res;
    bool hib;

    res = max17048_sleep_enter(&t_dev);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(MAX17048_MODE_ENSLEEP, s_bus.regs[MAX17048_REG_MODE]);
    CHECK_EQ(MAX17048_CONFIG_SLEEP,
             s_bus.regs[MAX17048_REG_CONFIG] & MAX17048_CONFIG_SLEEP);

    res = max17048_sleep_exit(&t_dev);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0, s_bus.regs[MAX17048_REG_MODE]);
    CHECK_EQ(0, s_bus.regs[MAX17048_REG_CONFIG] & MAX17048_CONFIG_SLEEP);

    /* HibStat 只读指示位 */
    s_bus.regs[MAX17048_REG_MODE] = MAX17048_MODE_HIBSTAT;
    res = max17048_is_hibernating(&t_dev, &hib);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(true, hib);
    s_bus.regs[MAX17048_REG_MODE] = 0u;
    res = max17048_is_hibernating(&t_dev, &hib);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(false, hib);
}

static void test_alert_service(void)
{
    max17048_result_t res;
    max17048_status_t status;

    /* 预置 RI|HD|VH 三位告警 */
    s_bus.regs[MAX17048_REG_STATUS] =
        MAX17048_STATUS_RI | MAX17048_STATUS_HD | MAX17048_STATUS_VH;
    /* 模拟硬件已拉低 ALRT：CONFIG.ALRT 置位 */
    s_bus.regs[MAX17048_REG_CONFIG] |= MAX17048_CONFIG_ALRT;

    res = max17048_get_status(&t_dev, &status);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(true, status.reset_indicator);
    CHECK_EQ(true, status.soc_low);
    CHECK_EQ(true, status.vhigh);
    CHECK_EQ(false, status.vlow);
    CHECK_EQ(false, status.soc_change);
    CHECK_EQ(false, status.vreset);

    /* 清 HD|VH：RI 保留，CONFIG.ALRT 清零 */
    res = max17048_clear_alerts(&t_dev,
                                MAX17048_STATUS_HD | MAX17048_STATUS_VH);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(MAX17048_STATUS_RI,
             s_bus.regs[MAX17048_REG_STATUS] &
             (MAX17048_STATUS_RI | MAX17048_STATUS_HD | MAX17048_STATUS_VH));
    CHECK_EQ(0, s_bus.regs[MAX17048_REG_CONFIG] & MAX17048_CONFIG_ALRT);

    /* 使能 VRESET 告警：仅写 EnVR 位，不清已置位的 RI */
    res = max17048_set_vreset_alert_enable(&t_dev, true);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(MAX17048_STATUS_ENVR | MAX17048_STATUS_RI,
             s_bus.regs[MAX17048_REG_STATUS] &
             (MAX17048_STATUS_ENVR | MAX17048_STATUS_RI));
}

static void test_quick_start_and_reset(void)
{
    max17048_result_t res;
    uint32_t writes_before;

    res = max17048_quick_start(&t_dev);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(MAX17048_MODE_QUICKSTART, s_bus.regs[MAX17048_REG_MODE]);

    /* 全复位：注入写失败仍返回 OK（手册：复位后无 ACK） */
    writes_before = s_bus.total_writes;
    s_bus.fail_next_write = 1;
    res = max17048_full_reset(&t_dev);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(MAX17048_CMD_POR_RESET,
             s_bus.regs[MAX17048_REG_CMD]);
    CHECK_EQ(writes_before + 1u, s_bus.total_writes);
}

#if MAX17048_USE_MODEL_TABLE
static void test_model_load(void)
{
    max17048_result_t res;
    uint16_t table[MAX17048_TABLE_WORDS];
    uint16_t i;

    for (i = 0u; i < MAX17048_TABLE_WORDS; i++)
    {
        table[i] = (uint16_t)(0x1000u + i);
    }

    mock_reset();
    res = max17048_init(&t_dev, NULL, MAX17048_I2C_ADDR);
    CHECK_EQ(MAX17048_OK, res);
    s_bus.log_count = 0u;   /* 清掉 init 的事务记录，只看加载序列 */

    res = max17048_model_load(&t_dev, table);
    CHECK_EQ(MAX17048_OK, res);
    /* 解锁字与复锁字 */
    CHECK_EQ(MAX17048_UNLOCK_VALUE, s_bus.log[0].val);
    CHECK_EQ(MAX17048_REG_UNLOCK, s_bus.log[0].reg);
    CHECK_EQ(MAX17048_LOCK_VALUE,
             s_bus.regs[MAX17048_REG_UNLOCK]);
    /* 表内容逐字写入（首字 0x1000，末字 0x103F） */
    CHECK_EQ(0x1000u, s_bus.regs[MAX17048_REG_TABLE_FIRST]);
    CHECK_EQ(0x103Fu, s_bus.regs[MAX17048_REG_TABLE_LAST]);
    /* 延时被调用（解锁后 + 复锁前各一次） */
    CHECK_TRUE(s_bus.delay_ms_sum >= (2u * MAX17048_MODEL_DELAY_MS));

    /* 中途写失败：返回 ERR_IO 但仍复锁 */
    mock_reset();
    (void)max17048_init(&t_dev, NULL, MAX17048_I2C_ADDR);
    s_bus.fail_write_at = 30;   /* 第 30 笔写失败（表中间） */
    res = max17048_model_load(&t_dev, table);
    CHECK_EQ(MAX17048_ERR_IO, res);
    CHECK_EQ(MAX17048_LOCK_VALUE, s_bus.regs[MAX17048_REG_UNLOCK]);

    /* 空指针 */
    res = max17048_model_load(&t_dev, NULL);
    CHECK_EQ(MAX17048_ERR_PARAM, res);
}
#endif /* MAX17048_USE_MODEL_TABLE */

static void test_raw_access(void)
{
    max17048_result_t res;
    uint16_t val;

    res = max17048_write_reg(&t_dev, MAX17048_REG_HIBRT, 0x1234u);
    CHECK_EQ(MAX17048_OK, res);
    res = max17048_read_reg(&t_dev, MAX17048_REG_HIBRT, &val);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x1234, val);

    /* 读-改-写：只动掩码内位 */
    res = max17048_update_bits(&t_dev, MAX17048_REG_HIBRT, 0x00FFu, 0x0021u);
    CHECK_EQ(MAX17048_OK, res);
    CHECK_EQ(0x1221, s_bus.regs[MAX17048_REG_HIBRT]);

    /* io 读失败上抛 */
    s_bus.fail_next_read = 1;
    res = max17048_read_reg(&t_dev, MAX17048_REG_HIBRT, &val);
    CHECK_EQ(MAX17048_ERR_IO, res);
}

#if MAX17048_VERIFY_WRITES
static void test_verify_writes(void)
{
    max17048_result_t res;

    /* 正常写通过回读校验 */
    res = max17048_set_rcomp(&t_dev, 0x97u);
    CHECK_EQ(MAX17048_OK, res);

    /* RMW 读与校验读均被篡改 → ERR_VERIFY */
    s_bus.corrupt_reads_left = 2;
    s_bus.corrupt_value = 0x0000u;
    res = max17048_set_rcomp(&t_dev, 0x98u);
    CHECK_EQ(MAX17048_ERR_VERIFY, res);
}
#endif /* MAX17048_VERIFY_WRITES */

/* ══════════════════════════════════════════════════════════════════════════
 * 入口
 * ════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    test_init_and_param();
    test_measurements();
    test_config_fields();
    test_temp_compensate();
    test_vreset();
    test_sleep_and_hibstat();
    test_alert_service();
    test_quick_start_and_reset();
#if MAX17048_USE_MODEL_TABLE
    test_model_load();
#endif
    test_raw_access();
#if MAX17048_VERIFY_WRITES
    test_verify_writes();
#endif

    printf("max17048 tests: %d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
