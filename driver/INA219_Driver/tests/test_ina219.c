/**
 * @file    test_ina219.c
 * @brief   Mock-I2C unit tests for the portable INA219 driver core
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-27
 *
 * @details
 * Implements the ina219_io.h contract against an in-memory register image
 * with hardware-faithful semantics (config RST self-clear + register file
 * restore, MODE writes / power reads clearing CNVR, calibration bit-0
 * forced low, read-only measurement registers), then exercises the public
 * API. Conversion anchors use the worked example from the datasheet
 * (Table 8: 2 mOhm shunt, 1 mA LSB, cal 0x5000, 10 A / 119.8 W). Designed
 * to run on the host or under the MounRiver RV32 simulator
 * (riscv32-wch-elf-run). Exit code 0 = all checks passed.
 *
 * SPDX-License-Identifier: WTFPL
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ina219.h"
#include "ina219_io.h"

/* ══════════════════════════════════════════════════════════════════════════
 * 极简断言框架
 * ══════════════════════════════════════════════════════════════════════════ */

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
 * 模拟 I2C 器件（寄存器镜像 + 硬件语义 + 故障注入 + 写序列记录）
 * ══════════════════════════════════════════════════════════════════════════ */

#define MOCK_REG_SPACE      8u
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
    uint8_t i;

    for (i = 0u; i < MOCK_REG_SPACE; i++)
    {
        s_bus.regs[i] = 0x0000u;
    }
    s_bus.regs[INA219_REG_CONFIG] = INA219_CONFIG_POR;
}

static void mock_reset(void)
{
    uint8_t i;

    mock_load_por();
    for (i = 0u; i < MOCK_REG_SPACE; i++)
    {
        (void)i;   /* 寄存器已在 mock_load_por 覆盖 */
    }
    s_bus.log_count = 0u;
    s_bus.total_writes = 0u;
    s_bus.total_reads = 0u;
    s_bus.delay_ms_sum = 0u;
    s_bus.fail_next_read = 0;
    s_bus.fail_next_write = 0;
    s_bus.fail_write_at = 0;
}

int ina219_io_init(void)
{
    return INA219_IO_OK;
}

int ina219_io_read_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                         uint16_t *val)
{
    (void)io_ctx;
    (void)dev_addr;

    s_bus.total_reads++;
    if (s_bus.fail_next_read)
    {
        s_bus.fail_next_read = 0;
        return INA219_IO_ERROR;
    }
    if (reg >= MOCK_REG_SPACE)
    {
        return INA219_IO_ERROR;
    }

    *val = s_bus.regs[reg];
    mock_log_push(0u, reg, *val);

    /* 硬件语义：读功率寄存器清除 CNVR */
    if (reg == INA219_REG_POWER)
    {
        s_bus.regs[INA219_REG_BUS] =
            (uint16_t)(s_bus.regs[INA219_REG_BUS] & ~INA219_BUS_CNVR);
    }
    return INA219_IO_OK;
}

int ina219_io_write_reg16(void *io_ctx, uint8_t dev_addr, uint8_t reg,
                          uint16_t val)
{
    (void)io_ctx;
    (void)dev_addr;

    s_bus.total_writes++;
    if (s_bus.fail_next_write)
    {
        s_bus.fail_next_write = 0;
        return INA219_IO_ERROR;
    }
    if ((s_bus.fail_write_at != 0) &&
        ((int32_t)s_bus.total_writes == s_bus.fail_write_at))
    {
        return INA219_IO_ERROR;
    }
    if (reg >= MOCK_REG_SPACE)
    {
        return INA219_IO_ERROR;
    }

    if (reg == INA219_REG_CONFIG)
    {
        if ((val & INA219_CFG_RST) != 0u)
        {
            /* RST：全部寄存器回 POR 默认，RST 位自清零 */
            mock_load_por();
            mock_log_push(1u, reg, val);
            return INA219_IO_OK;
        }
        /* 写 MODE 字段清 CNVR（任何配置寄存器整字写均按此模拟） */
        s_bus.regs[INA219_REG_BUS] =
            (uint16_t)(s_bus.regs[INA219_REG_BUS] & ~INA219_BUS_CNVR);
    }
    if ((reg >= INA219_REG_SHUNT) && (reg <= INA219_REG_CURRENT))
    {
        /* 测量寄存器只读，写入被器件忽略 */
        mock_log_push(1u, reg, val);
        return INA219_IO_OK;
    }
    if (reg == INA219_REG_CALIBRATION)
    {
        /* FS0 为 void 位，读回恒 0 */
        val = (uint16_t)(val & 0xFFFEu);
    }

    s_bus.regs[reg] = val;
    mock_log_push(1u, reg, val);
    return INA219_IO_OK;
}

#if INA219_USE_TRIGGERED
void ina219_io_delay_ms(uint32_t ms)
{
    s_bus.delay_ms_sum += ms;
}
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 用例
 * ══════════════════════════════════════════════════════════════════════════ */

static ina219_dev_t g_dev;

/** 统一的 init 前置：恢复 POR 镜像并完成 init */
static void fresh_init(void)
{
    mock_reset();
    g_dev.io_ctx = NULL;
    g_dev.dev_addr = INA219_I2C_ADDR;
    g_dev.inited = 0u;
    CHECK_EQ(INA219_OK, ina219_init(&g_dev, NULL, INA219_I2C_ADDR));
}

static void test_null_and_not_ready(void)
{
    uint16_t v16;
    int32_t i32;
    uint16_t u16;

    CHECK_EQ(INA219_ERR_PARAM, ina219_init(NULL, NULL, 0x40));
    CHECK_EQ(INA219_ERR_PARAM, ina219_read_bus_voltage(NULL, &u16));
    CHECK_EQ(INA219_ERR_PARAM, ina219_read_bus_voltage(&g_dev, NULL));
    CHECK_EQ(INA219_ERR_PARAM, ina219_write_reg(NULL, 0, 0));

    g_dev.inited = 0u;
    CHECK_EQ(INA219_ERR_NOT_READY, ina219_read_bus_voltage(&g_dev, &u16));
    CHECK_EQ(INA219_ERR_NOT_READY, ina219_read_shunt_voltage(&g_dev, &i32));
    CHECK_EQ(INA219_ERR_NOT_READY, ina219_read_reg(&g_dev, 0u, &v16));
    CHECK_EQ(INA219_ERR_NOT_READY, ina219_reset(&g_dev));
}

static void test_init_writes_config_and_calibration(void)
{
    uint16_t cal;
    uint32_t shunt;
    uint32_t lsb;

    fresh_init();

    /* conf 默认（1Ω / 10µA / 32V / /8 / 12bit / 连续）恰为 0x399F，
     * 校准值 4096 = 0x1000；通过事务日志验证 init 序列：
     * R(config) → W(config) → W(cal)（VERIFY_WRITES=1 时每次写后
     * 追加一条回读事务） */
#if INA219_VERIFY_WRITES
    CHECK_EQ(5u, s_bus.log_count);
#else
    CHECK_EQ(3u, s_bus.log_count);
#endif
    CHECK_EQ(0u, s_bus.log[0].is_write);
    CHECK_EQ((uint16_t)INA219_REG_CONFIG, (uint16_t)s_bus.log[0].reg);
    CHECK_EQ(1u, s_bus.log[1].is_write);
    CHECK_EQ((uint16_t)INA219_REG_CONFIG, (uint16_t)s_bus.log[1].reg);
    CHECK_EQ((long)0x399F, (long)s_bus.log[1].val);

    CHECK_EQ(0x1000, s_bus.regs[INA219_REG_CALIBRATION]);
    CHECK_TRUE(g_dev.inited != 0u);

    CHECK_EQ(INA219_OK,
             ina219_get_calibration(&g_dev, &shunt, &lsb, &cal));
    CHECK_EQ((long)1000000UL, (long)shunt);
    CHECK_EQ((long)10000UL, (long)lsb);
    CHECK_EQ((long)0x1000, (long)cal);
}

static void test_init_no_device(void)
{
    mock_reset();
    g_dev.inited = 0u;
    s_bus.fail_next_read = 1;
    CHECK_EQ(INA219_ERR_IO, ina219_init(&g_dev, NULL, INA219_I2C_ADDR));
    CHECK_TRUE(g_dev.inited == 0u);
}

static void test_init_midway_write_failure(void)
{
    mock_reset();
    g_dev.inited = 0u;
    s_bus.fail_write_at = 2;   /* 第 2 笔写 = 校准寄存器写失败 */
    CHECK_EQ(INA219_ERR_IO, ina219_init(&g_dev, NULL, INA219_I2C_ADDR));
    CHECK_TRUE(g_dev.inited == 0u);
}

static void test_bus_range(void)
{
    bool range32v = false;

    fresh_init();

    CHECK_EQ(INA219_OK, ina219_set_bus_range(&g_dev, false));
    CHECK_EQ(0u, s_bus.regs[INA219_REG_CONFIG] & INA219_CFG_BRNG);
    CHECK_EQ(INA219_OK, ina219_get_bus_range(&g_dev, &range32v));
    CHECK_TRUE(range32v == false);

    CHECK_EQ(INA219_OK, ina219_set_bus_range(&g_dev, true));
    CHECK_EQ((long)INA219_CFG_BRNG,
             (long)(s_bus.regs[INA219_REG_CONFIG] & INA219_CFG_BRNG));
    CHECK_EQ(INA219_OK, ina219_get_bus_range(&g_dev, &range32v));
    CHECK_TRUE(range32v);
}

static void test_pga_nearest(void)
{
    uint16_t mv = 0u;

    fresh_init();

    /* 就近取整；等距取较小档（宁欠不过量程） */
    CHECK_EQ(INA219_OK, ina219_set_pga_range(&g_dev, 100));
    CHECK_EQ(INA219_OK, ina219_get_pga_range(&g_dev, &mv));
    CHECK_EQ(80, mv);

    CHECK_EQ(INA219_OK, ina219_set_pga_range(&g_dev, 200));
    CHECK_EQ(INA219_OK, ina219_get_pga_range(&g_dev, &mv));
    CHECK_EQ(160, mv);

    CHECK_EQ(INA219_OK, ina219_set_pga_range(&g_dev, 60));
    CHECK_EQ(INA219_OK, ina219_get_pga_range(&g_dev, &mv));
    CHECK_EQ(40, mv);

    CHECK_EQ(INA219_OK, ina219_set_pga_range(&g_dev, 1000));
    CHECK_EQ(INA219_OK, ina219_get_pga_range(&g_dev, &mv));
    CHECK_EQ(320, mv);

    CHECK_EQ(INA219_OK, ina219_get_pga_range(&g_dev, &mv));
    CHECK_EQ(320, mv);   /* 超范围钳位到最大档 */
}

static void test_adc_settings(void)
{
    uint8_t badc = 0u;
    uint8_t sadc = 0u;

    fresh_init();

    /* 0x4~0x8 为冗余/非法编码 */
    CHECK_EQ(INA219_ERR_PARAM, ina219_set_adc(&g_dev, 0x8, 0x3));
    CHECK_EQ(INA219_ERR_PARAM, ina219_set_adc(&g_dev, 0x3, 0x4));

    CHECK_EQ(INA219_OK, ina219_set_adc(&g_dev, INA219_ADC_128_SAMPLES,
                                       INA219_ADC_9BIT));
    CHECK_EQ(INA219_OK, ina219_get_adc(&g_dev, &badc, &sadc));
    CHECK_EQ((long)INA219_ADC_128_SAMPLES, (long)badc);
    CHECK_EQ((long)INA219_ADC_9BIT, (long)sadc);
    CHECK_EQ((long)INA219_ADC_128_SAMPLES,
             (long)((s_bus.regs[INA219_REG_CONFIG] & INA219_CFG_BADC_MASK) >>
                    INA219_CFG_BADC_SHIFT));
}

static void test_mode(void)
{
    ina219_mode_t mode = INA219_MODE_CONT_SHUNT_BUS_E;

    fresh_init();

    CHECK_EQ(INA219_ERR_PARAM,
             ina219_set_mode(&g_dev, (ina219_mode_t)8));

    CHECK_EQ(INA219_OK, ina219_set_mode(&g_dev, INA219_MODE_TRIG_SHUNT_BUS_E));
    CHECK_EQ((long)INA219_MODE_TRIG_SHUNT_BUS,
             (long)((s_bus.regs[INA219_REG_CONFIG] & INA219_CFG_MODE_MASK) >>
                    INA219_CFG_MODE_SHIFT));
    CHECK_EQ(INA219_OK, ina219_get_mode(&g_dev, &mode));
    CHECK_EQ((long)INA219_MODE_TRIG_SHUNT_BUS_E, (long)mode);

    CHECK_EQ(INA219_OK, ina219_set_mode(&g_dev, INA219_MODE_POWER_DOWN_E));
    CHECK_EQ((long)INA219_MODE_POWER_DOWN,
             (long)((s_bus.regs[INA219_REG_CONFIG] & INA219_CFG_MODE_MASK) >>
                    INA219_CFG_MODE_SHIFT));
}

static void test_calibration_bounds(void)
{
    uint16_t cal;
    uint32_t shunt;
    uint32_t lsb;

    fresh_init();

    /* 1Ω：lsb=1250nA → cal=32768 超出 15 位上限 0x7FFE */
    CHECK_EQ(INA219_ERR_PARAM, ina219_set_calibration(&g_dev, 1000000u, 1250u));
    /* 1Ω：lsb=1251nA → cal=32741 合法 */
    CHECK_EQ(INA219_OK, ina219_set_calibration(&g_dev, 1000000u, 1251u));
    /* lsb 过大使 cal 截断为 0 */
    CHECK_EQ(INA219_ERR_PARAM, ina219_set_calibration(&g_dev, 1000000u,
                                                      40960001u));
    /* 零参数 */
    CHECK_EQ(INA219_ERR_PARAM, ina219_set_calibration(&g_dev, 0u, 10000u));
    CHECK_EQ(INA219_ERR_PARAM, ina219_set_calibration(&g_dev, 1000000u, 0u));

    /* 手册 Table 8 数值：2mΩ（2000µΩ）/ 1mA → cal = 0x5000 */
    CHECK_EQ(INA219_OK, ina219_set_calibration(&g_dev, 2000u, 1000000u));
    CHECK_EQ((long)0x5000, (long)s_bus.regs[INA219_REG_CALIBRATION]);
    CHECK_EQ(INA219_OK,
             ina219_get_calibration(&g_dev, &shunt, &lsb, &cal));
    CHECK_EQ(2000, shunt);
    CHECK_EQ((long)1000000u, (long)lsb);
}

static void test_shunt_voltage(void)
{
    int32_t uv = 0;
    int16_t raw16 = 0;

    fresh_init();

    s_bus.regs[INA219_REG_SHUNT] = 2000;         /* +20.00mV */
    CHECK_EQ(INA219_OK, ina219_read_shunt_voltage(&g_dev, &uv));
    CHECK_EQ(20000, uv);
    CHECK_EQ(INA219_OK, ina219_read_shunt_raw(&g_dev, &raw16));
    CHECK_EQ(2000, raw16);

    s_bus.regs[INA219_REG_SHUNT] = 0x8300;       /* -320mV @/8（Figure 20） */
    CHECK_EQ(INA219_OK, ina219_read_shunt_voltage(&g_dev, &uv));
    CHECK_EQ(-320000, uv);

    s_bus.regs[INA219_REG_SHUNT] = 0xF060;       /* -40mV @/1（Figure 23） */
    CHECK_EQ(INA219_OK, ina219_read_shunt_voltage(&g_dev, &uv));
    CHECK_EQ(-40000, uv);

    s_bus.regs[INA219_REG_SHUNT] = 0x7D00;       /* +320mV @/8 */
    CHECK_EQ(INA219_OK, ina219_read_shunt_voltage(&g_dev, &uv));
    CHECK_EQ(320000, uv);
}

static void test_bus_voltage(void)
{
    uint16_t mv = 0u;
    uint16_t raw16 = 0u;

    fresh_init();

    /* 手册示例：shifted=2995 → 11.98V，raw = 2995<<3 */
    s_bus.regs[INA219_REG_BUS] = (uint16_t)(2995u << 3);
    CHECK_EQ(INA219_OK, ina219_read_bus_voltage(&g_dev, &mv));
    CHECK_EQ(11980, mv);

    /* CNVR/OVF 标志位不参与换算 */
    s_bus.regs[INA219_REG_BUS] =
        (uint16_t)((2995u << 3) | INA219_BUS_CNVR | INA219_BUS_OVF);
    CHECK_EQ(INA219_OK, ina219_read_bus_voltage(&g_dev, &mv));
    CHECK_EQ(11980, mv);

    /* 32V 满量程：shifted=8000 → 32000mV */
    s_bus.regs[INA219_REG_BUS] = (uint16_t)(8000u << 3);
    CHECK_EQ(INA219_OK, ina219_read_bus_voltage(&g_dev, &mv));
    CHECK_EQ(32000, mv);

    s_bus.regs[INA219_REG_BUS] = (uint16_t)(2995u << 3);
    CHECK_EQ(INA219_OK, ina219_read_bus_raw(&g_dev, &raw16));
    CHECK_EQ((long)(2995u << 3), (long)raw16);
}

static void test_current_and_power(void)
{
    int32_t ua = 0;
    uint32_t uw = 0u;
    int16_t raw16 = 0;
    uint16_t puraw = 0u;

    fresh_init();

    /* 手册 Table 8 场景：2mΩ 采样电阻、1mA Current_LSB、cal=0x5000。
     * 电流寄存器 10000 (2710h) = 10A；功率寄存器 5990 (1766h) = 119.8W */
    CHECK_EQ(INA219_OK,
             ina219_set_calibration(&g_dev, 2000u, 1000000u));

    s_bus.regs[INA219_REG_CURRENT] = 10000;
    CHECK_EQ(INA219_OK, ina219_read_current(&g_dev, &ua));
    CHECK_EQ((long)10000000L, (long)ua);
    CHECK_EQ(INA219_OK, ina219_read_current_raw(&g_dev, &raw16));
    CHECK_EQ(10000, raw16);

    s_bus.regs[INA219_REG_POWER] = 5990;
    CHECK_EQ(INA219_OK, ina219_read_power(&g_dev, &uw));
    CHECK_EQ((long)119800000UL, (long)uw);
    CHECK_EQ(INA219_OK, ina219_read_power_raw(&g_dev, &puraw));
    CHECK_EQ(5990, puraw);

    /* 负电流对称舍入：raw=-1、lsb=1250nA → -1.25µA → -1（远离零） */
    CHECK_EQ(INA219_OK, ina219_set_calibration(&g_dev, 1000000u, 1251u));
    s_bus.regs[INA219_REG_CURRENT] = (uint16_t)(int16_t)-1;
    CHECK_EQ(INA219_OK, ina219_read_current(&g_dev, &ua));
    CHECK_EQ(-1, ua);

    /* 极端校准下电流换算钳位到 INT32_MAX（1mΩ / lsb=1A，raw=32767） */
    CHECK_EQ(INA219_OK, ina219_set_calibration(&g_dev, 1000u, 1000000000u));
    s_bus.regs[INA219_REG_CURRENT] = 32767;
    CHECK_EQ(INA219_OK, ina219_read_current(&g_dev, &ua));
    CHECK_EQ((long)2147483647L, (long)ua);
}

static void test_status_flags(void)
{
    bool flag = false;

    fresh_init();

    s_bus.regs[INA219_REG_BUS] = (uint16_t)(INA219_BUS_CNVR | INA219_BUS_OVF);
    CHECK_EQ(INA219_OK, ina219_is_conversion_ready(&g_dev, &flag));
    CHECK_TRUE(flag);
    CHECK_EQ(INA219_OK, ina219_is_math_overflow(&g_dev, &flag));
    CHECK_TRUE(flag);

    /* 硬件语义：读功率寄存器清除 CNVR */
    {
        uint32_t uw = 0u;

        s_bus.regs[INA219_REG_POWER] = 1234;
        CHECK_EQ(INA219_OK, ina219_read_power(&g_dev, &uw));
        CHECK_EQ((long)1234u *
                     (long)(10000u * 20u / 1000u),
                 (long)uw);   /* 默认 1Ω/10µA 校准：1234×200µW */
    }
    CHECK_EQ(INA219_OK, ina219_is_conversion_ready(&g_dev, &flag));
    CHECK_TRUE(flag == false);
    CHECK_EQ(INA219_OK, ina219_is_math_overflow(&g_dev, &flag));
    CHECK_TRUE(flag);   /* OVF 不受影响 */
}

#if INA219_USE_TRIGGERED
static void test_trigger_and_wait(void)
{
    uint32_t writes_before;
    bool ready = true;

    fresh_init();

    /* 触发 = 整字重写配置寄存器 */
    writes_before = s_bus.total_writes;
    CHECK_EQ(INA219_OK, ina219_trigger(&g_dev));
    CHECK_EQ((long)(writes_before + 1u), (long)s_bus.total_writes);
    CHECK_EQ((long)0x399F, (long)s_bus.regs[INA219_REG_CONFIG]);

    /* CNVR 未置位：5ms 超时 → 轮询 6 次、延时 5ms */
    CHECK_EQ(INA219_ERR_TIMEOUT, ina219_wait_conversion(&g_dev, 5u));
    CHECK_EQ(5u, s_bus.delay_ms_sum);

    /* CNVR 置位：立即返回 */
    s_bus.regs[INA219_REG_BUS] = INA219_BUS_CNVR;
    CHECK_EQ(INA219_OK, ina219_wait_conversion(&g_dev, 5u));
    CHECK_EQ(INA219_OK, ina219_is_conversion_ready(&g_dev, &ready));
    CHECK_TRUE(ready);
}
#endif /* INA219_USE_TRIGGERED */

static void test_reset(void)
{
    uint16_t v16;

    fresh_init();
    CHECK_EQ(0x1000, s_bus.regs[INA219_REG_CALIBRATION]);

    CHECK_EQ(INA219_OK, ina219_reset(&g_dev));

    /* RST 写使镜像回到 POR：校准丢失、配置回默认 */
    CHECK_EQ((long)INA219_CONFIG_POR, (long)s_bus.regs[INA219_REG_CONFIG]);
    CHECK_EQ(0, s_bus.regs[INA219_REG_CALIBRATION]);

    /* 句柄回到未初始化态，后续 API 拒绝 */
    CHECK_TRUE(g_dev.inited == 0u);
    CHECK_EQ(INA219_ERR_NOT_READY, ina219_read_reg(&g_dev, 0u, &v16));

    /* 重新 init 后恢复 */
    CHECK_EQ(INA219_OK, ina219_init(&g_dev, NULL, INA219_I2C_ADDR));
    CHECK_EQ(0x1000, s_bus.regs[INA219_REG_CALIBRATION]);
}

static void test_reg_level_access(void)
{
    uint16_t v16 = 0u;

    fresh_init();

    CHECK_EQ(INA219_OK, ina219_write_reg(&g_dev, INA219_REG_CALIBRATION,
                                         0x2AAA));
    CHECK_EQ((long)0x2AAA, (long)s_bus.regs[INA219_REG_CALIBRATION]);

    CHECK_EQ(INA219_OK, ina219_read_reg(&g_dev, INA219_REG_CALIBRATION, &v16));
    CHECK_EQ((long)0x2AAA, (long)v16);

    CHECK_EQ(INA219_OK, ina219_update_bits(&g_dev, INA219_REG_CALIBRATION,
                                           0x00F0u, 0x0050u));
    CHECK_EQ((long)0x2A5Au, (long)s_bus.regs[INA219_REG_CALIBRATION]);

    /* 只读测量寄存器写被忽略 */
    CHECK_EQ(INA219_OK, ina219_write_reg(&g_dev, INA219_REG_SHUNT, 0xBEEF));
    CHECK_EQ(0, s_bus.regs[INA219_REG_SHUNT]);
}

static void test_io_error_propagation(void)
{
    uint16_t v16;

    fresh_init();
    s_bus.fail_next_read = 1;
    CHECK_EQ(INA219_ERR_IO, ina219_read_reg(&g_dev, INA219_REG_CONFIG, &v16));
    s_bus.fail_next_write = 1;
    CHECK_EQ(INA219_ERR_IO, ina219_write_reg(&g_dev, INA219_REG_CALIBRATION,
                                             0x1000u));
}

int main(void)
{
    test_null_and_not_ready();
    test_init_writes_config_and_calibration();
    test_init_no_device();
    test_init_midway_write_failure();
    test_bus_range();
    test_pga_nearest();
    test_adc_settings();
    test_mode();
    test_calibration_bounds();
    test_shunt_voltage();
    test_bus_voltage();
    test_current_and_power();
    test_status_flags();
#if INA219_USE_TRIGGERED
    test_trigger_and_wait();
#endif
    test_reset();
    test_reg_level_access();
    test_io_error_propagation();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
