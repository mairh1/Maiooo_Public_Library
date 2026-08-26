/*
 * @file    max17048_regs.h
 * @brief   MAX17048/MAX17049 电量计寄存器地址、位定义与常量
 * @details 全部寄存器为 16 位，高字节位于偶数存储地址，读写必须在同一
 *          I2C 事务中传输两个字节（高字节在前）。
 *          寄存器位序与复位值依据本目录数据手册 Table 2 / Figure 8~13：
 *          MAX17048/MAX17049, 19-6171; Rev 7; 11/16。
 * @note    MODE 寄存器在 Table 2 中标注为只写（W），但其中 HibStat 位
 *          手册描述为只读状态位，读取该位在部分器件上可能返回未定义值。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-25
 */

#ifndef MAX17048_REGS_H
#define MAX17048_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 常量与换算参数
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_I2C_ADDR           0x36    /**< 7 位从机地址（8 位写 0x6C / 读 0x6D） */

#define MAX17048_VCELL_LSB_UV       78125   /**< VCELL 分辨率：78.125µV/节 */
#define MAX17048_SOC_LSB_DIV        256     /**< SOC 分辨率：1/256 % */
#define MAX17048_CRATE_MILLI_PCT_H  208     /**< CRATE 分辨率：0.208 %/h（千分比/小时） */
#define MAX17048_VALRT_LSB_MV       20      /**< VALRT.MIN/MAX 分辨率：20mV */
#define MAX17048_HIBRT_ACT_LSB_MV   1250    /**< ActThr 分辨率：1.25mV（×1000） */
#define MAX17048_HIBRT_HIB_LSB_MPH  208     /**< HibThr 分辨率：0.208 %/h（千分比/小时） */
#define MAX17048_VRESET_LSB_MV      40      /**< VRESET[7:1] 分辨率：40mV */
#define MAX17048_VRESET_MIN_MV      2280    /**< VRESET 可编程下限（特性表 2.28~3.48V） */
#define MAX17048_VRESET_MAX_MV      3480    /**< VRESET 可编程上限 */

/* ══════════════════════════════════════════════════════════════════════════
 * 寄存器地址（Table 2）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_REG_VCELL          0x02    /**< 电池电压，R，78.125µV/cell */
#define MAX17048_REG_SOC            0x04    /**< 电量百分比，R，1/256 % */
#define MAX17048_REG_MODE           0x06    /**< 命令寄存器，W，POR 0x0000 */
#define MAX17048_REG_VERSION        0x08    /**< 版本寄存器，R，POR 0x001x */
#define MAX17048_REG_HIBRT          0x0A    /**< 休眠阈值，R/W，POR 0x8030 */
#define MAX17048_REG_CONFIG         0x0C    /**< 配置寄存器，R/W，POR 0x971C */
#define MAX17048_REG_VALRT          0x14    /**< 电压告警窗，R/W，POR 0x00FF */
#define MAX17048_REG_CRATE          0x16    /**< 充放电速率，R，0.208 %/h */
#define MAX17048_REG_VRESET         0x18    /**< 复位阈值/ID，R/W，POR 0x96xx */
#define MAX17048_REG_STATUS         0x1A    /**< 状态/告警位，R/W，POR 0x01xx */
#define MAX17048_REG_UNLOCK         0x3E    /**< 模型表解锁（0x3E/0x3F 两字节地址） */
#define MAX17048_REG_TABLE_FIRST    0x40    /**< 模型表首地址 */
#define MAX17048_REG_TABLE_LAST     0x7F    /**< 模型表末地址 */
#define MAX17048_REG_CMD            0xFE    /**< POR 命令寄存器，POR 0xFFFF */

#define MAX17048_TABLE_WORDS        64      /**< 模型表字数（0x40~0x7F 共 64 字） */

/* ══════════════════════════════════════════════════════════════════════════
 * 复位（POR）默认值
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_MODE_POR           0x0000  /**< MODE 复位值 */
#define MAX17048_HIBRT_POR          0x8030  /**< HIBRT 复位值（HibThr=0x80, ActThr=0x30） */
#define MAX17048_CONFIG_POR         0x971C  /**< CONFIG 复位值（RCOMP=0x97, ATHD=4%） */
#define MAX17048_VALRT_POR          0x00FF  /**< VALRT 复位值（0mV ~ 5100mV） */
#define MAX17048_VRESET_POR         0x9600  /**< VRESET/ID 复位值（低字节为 OTP ID） */
#define MAX17048_STATUS_POR         0x0100  /**< STATUS 复位值（RI 置位） */
#define MAX17048_CMD_POR            0xFFFF  /**< CMD 复位值 */

/* ══════════════════════════════════════════════════════════════════════════
 * MODE 寄存器位（Figure 8）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_MODE_QUICKSTART    (1u << 14)  /**< 写 1 触发快速启动 */
#define MAX17048_MODE_ENSLEEP       (1u << 13)  /**< 睡眠模式使能（配合 CONFIG.SLEEP） */
#define MAX17048_MODE_HIBSTAT       (1u << 12)  /**< 休眠状态指示（只读） */

/* ══════════════════════════════════════════════════════════════════════════
 * CONFIG 寄存器位（Figure 10）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_CONFIG_RCOMP_SHIFT 8           /**< RCOMP 字段右移数 */
#define MAX17048_CONFIG_RCOMP_MASK  (0xFFu << 8) /**< 温度补偿系数字段 */
#define MAX17048_CONFIG_SLEEP       (1u << 7)   /**< 写 1 进入睡眠（需 EnSleep=1） */
#define MAX17048_CONFIG_ALSC        (1u << 6)   /**< SOC 变化 1% 告警使能 */
#define MAX17048_CONFIG_ALRT        (1u << 5)   /**< 告警状态位（写 0 清除并释放 ALRT 引脚） */
#define MAX17048_CONFIG_ATHD_SHIFT  0           /**< ATHD 字段右移数 */
#define MAX17048_CONFIG_ATHD_MASK   (0x1Fu << 0) /**< 低电量告警阈值字段，值 = 32 - 百分比 */

/* ══════════════════════════════════════════════════════════════════════════
 * STATUS 寄存器位（Figure 13，写 1 清除）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_STATUS_ENVR        (1u << 14)  /**< 电压复位事件告警使能（可读写） */
#define MAX17048_STATUS_SC          (1u << 13)  /**< SOC 变化 ≥1% 告警 */
#define MAX17048_STATUS_HD          (1u << 12)  /**< SOC 跌破 ATHD 阈值告警 */
#define MAX17048_STATUS_VR          (1u << 11)  /**< 电压复位事件告警（需 EnVR=1） */
#define MAX17048_STATUS_VL          (1u << 10)  /**< VCELL < VALRT.MIN 欠压告警 */
#define MAX17048_STATUS_VH          (1u << 9)   /**< VCELL > VALRT.MAX 过压告警 */
#define MAX17048_STATUS_RI          (1u << 8)   /**< 复位指示：上电置位，需清零 */

/** STATUS 中全部可清除的告警位（不含使能位 EnVR） */
#define MAX17048_STATUS_ALERT_MASK  (MAX17048_STATUS_SC | MAX17048_STATUS_HD | \
                                     MAX17048_STATUS_VR | MAX17048_STATUS_VL | \
                                     MAX17048_STATUS_VH | MAX17048_STATUS_RI)

/* ══════════════════════════════════════════════════════════════════════════
 * HIBRT / VALRT 寄存器字段（Figure 9 / Figure 11）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17048_HIBRT_HIB_SHIFT    8           /**< HibThr 字段右移数（×0.208%/h） */
#define MAX17048_HIBRT_HIB_MASK     (0xFFu << 8) /**< 休眠进入阈值字段 */
#define MAX17048_HIBRT_ACT_MASK     (0xFFu << 0) /**< 休眠退出阈值字段（×1.25mV） */

#define MAX17048_VALRT_MIN_SHIFT    8           /**< VALRT.MIN 字段右移数（×20mV） */
#define MAX17048_VALRT_MIN_MASK     (0xFFu << 8) /**< 欠压阈值字段 */
#define MAX17048_VALRT_MAX_MASK     (0xFFu << 0) /**< 过压阈值字段（×20mV） */

/* ══════════════════════════════════════════════════════════════════════════
 * VRESET/ID 寄存器位（Figure 12）
 * ════════════════════════════════════════════════════════════════════════ */

/** VRESET[7:1] 位于 MSB 字节位 7..1，即 16 位寄存器 bit15..9，阈值 = 字段值×40mV */
#define MAX17048_VRESET_SHIFT       9           /**< VRESET 字段右移数（40mV 档） */
#define MAX17048_VRESET_MASK        (0x7Fu << 9) /**< 复位阈值字段（7 位） */
#define MAX17048_VRESET_DIS         (1u << 8)   /**< MSB 位 0：休眠时禁用模拟比较器（省约 0.5µA） */
#define MAX17048_VRESET_ID_MASK     (0xFFu << 0) /**< 低字节 OTP ID（只读，写入被忽略） */
#define MAX17048_VRESET_RW_MASK     (0xFFu << 8) /**< 可写位集合（阈值 + Dis） */

/* ══════════════════════════════════════════════════════════════════════════
 * 命令常量
 * ════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 模型表解锁字
 * @details 手册表述为"向地址 0x3F 写 0x57、向地址 0x3E 写 0x4A"。按本器件
 *          16 位寄存器规则（高字节在偶地址），等效为向 0x3E 一次写入
 *          0x4A57（0x4A 落在 0x3E、0x57 落在 0x3F）。
 */
#define MAX17048_UNLOCK_VALUE       0x4A57
#define MAX17048_LOCK_VALUE         0x0000      /**< 模型表复锁字（0x3E/0x3F 写 0） */

/** CMD 寄存器 POR 全复位命令（最后一个时钟后器件复位，无 ACK） */
#define MAX17048_CMD_POR_RESET      0x5400

/** VERSION 寄存器期望值（高 12 位，低 4 位为生产批次） */
#define MAX17048_VERSION_EXPECTED   0x0010
#define MAX17048_VERSION_MASK       0xFFF0

#ifdef __cplusplus
}
#endif

#endif /* MAX17048_REGS_H */
