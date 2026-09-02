/**
 * @file    max17260_regs.h
 * @brief   MAX17260 ModelGauge m5 EZ 电量计寄存器地址、位定义与常量
 * @details 全部数据寄存器为 16 位（Status / 模型寄存器为 16 位），高字节
 *          位于偶数存储地址，读写必须在同一 I2C 事务中传输两个字节
 *          （高字节在前）。
 *          寄存器表 / 复位值 / 分辨率依据本目录数据手册
 *          （MAX17260, 19-100249; Rev 2; 7/24）Table 2 / Table 17。
 * @note    容量与电流的 LSB 单位为 µVh / µV（与 RSENSE 相关），实际
 *          mAh / mA 需用 max17260_conf.h 中 MAX17260_RSENSE_MOHM 换算。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-02
 */

#ifndef MAX17260_REGS_H
#define MAX17260_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * I2C 从机地址（手册 Ordering Information 表）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_I2C_ADDR_DEFAULT   0x36    /**< 7 位从机地址（MAX17260SEWL+/SETD+ 系列） */
#define MAX17260_I2C_ADDR_ALT       0x0D    /**< 7 位从机地址（MAX17260BEWL+ 系列） */

/* ══════════════════════════════════════════════════════════════════════════
 * 换算常量（手册 Table 2 Standard Resolutions）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_VCELL_LSB_UV       78125       /**< VCell 分辨率 78.125µV/LSB */
#define MAX17260_TEMP_LSB_C_X256    1           /**< Temp 分辨率 1/256 ℃/LSB */
#define MAX17260_SOC_LSB_DIV        256         /**< RepSOC 分辨率 1/256 %/LSB */
#define MAX17260_CURRENT_LSB_UV     15625       /**< Current 分辨率 1.5625µV/RSENSE/LSB ×10 */
#define MAX17260_CAPACITY_LSB_UVH   50000       /**< RepCap 分辨率 5.0µVh/RSENSE/LSB ×10 */
#define MAX17260_TIME_LSB_S_X10     5625        /**< TTE/TTF 分辨率 5.625s/LSB ×10 */
#define MAX17260_VEMPTY_LSB_MV      10          /**< VEmpty.VE 分辨率 10mV/LSB */
#define MAX17260_VRECOV_LSB_MV      40          /**< VEmpty.VR 分辨率 40mV/LSB */
#define MAX17260_ALRT_LSB_MV        20          /**< VAlrtTh 分辨率 20mV/LSB */
#define MAX17260_TALRT_LSB_C        1           /**< TAlrtTh 分辨率 1℃/LSB */
#define MAX17260_SALRT_LSB_PCT      1           /**< SAlrtTh 分辨率 1%/LSB */
#define MAX17260_IALRT_LSB_UV       400         /**< IAlrtTh 分辨率 0.4mV/RSENSE/LSB ×1000 */

/* ══════════════════════════════════════════════════════════════════════════
 * 寄存器地址（手册 Table 17 Memory Map）
 * ════════════════════════════════════════════════════════════════════════ */

/* 0x 页 */
#define MAX17260_REG_STATUS         0x00    /**< Status，R/W，POR 0x8082 */
#define MAX17260_REG_VALRTTH        0x01    /**< 电压告警阈值，R/W，POR 0xFF00 */
#define MAX17260_REG_TALRTTH        0x02    /**< 温度告警阈值，R/W，POR 0x7F80 */
#define MAX17260_REG_SALRTTH        0x03    /**< SOC 告警阈值，R/W，POR 0xFF00 */
#define MAX17260_REG_ATRATE         0x04    /**< 虚拟负载电流，R/W，POR 0x0000 */
#define MAX17260_REG_REPCAP         0x05    /**< 剩余容量 mAh，R */
#define MAX17260_REG_REPSOC         0x06    /**< SOC 百分比，R */
#define MAX17260_REG_AGE            0x07    /**< 电池老化系数，R */
#define MAX17260_REG_TEMP           0x08    /**< 温度，R */
#define MAX17260_REG_VCELL          0x09    /**< 电池电压，R */
#define MAX17260_REG_CURRENT        0x0A    /**< 瞬时电流，R */
#define MAX17260_REG_AVGCURRENT     0x0B    /**< 平均电流，R */

/* 0x1 页 */
#define MAX17260_REG_FULLCAPREP     0x10    /**< 满充容量 mAh，R */
#define MAX17260_REG_TTE            0x11    /**< 剩余放电时间，R */
#define MAX17260_REG_AVGTA          0x16    /**< 平均温度，R */
#define MAX17260_REG_CYCLES         0x17    /**< 充放电循环，R */
#define MAX17260_REG_DESIGNCAP      0x18    /**< 设计容量 mAh，R/W，POR 0x0BB8 */
#define MAX17260_REG_AVGVCELL       0x19    /**< 平均电压，R */
#define MAX17260_REG_MAXMINTEMP     0x1A    /**< 温度最大/最小，R/W，POR 0x807F */
#define MAX17260_REG_MAXMINVOLT     0x1B    /**< 电压最大/最小，R/W，POR 0x00FF */
#define MAX17260_REG_MAXMINCURR     0x1C    /**< 电流最大/最小，R/W，POR 0x807F */
#define MAX17260_REG_CONFIG         0x1D    /**< 主配置，R/W，POR 0x2210 */
#define MAX17260_REG_ICHGTERM       0x1E    /**< 充电终止电流，R/W，POR 0x0640 */

/* 0x2 页 */
#define MAX17260_REG_TTF            0x20    /**< 剩余充电时间，R */

/* 0x3 页 */
#define MAX17260_REG_DIETEMP        0x034   /**< 芯片内部温度（8 位地址），R */
#define MAX17260_REG_VEMPTY         0x3A    /**< 空载/恢复电压，R/W，POR 0xA561 */

/* 0xB 页 */
#define MAX17260_REG_POWER          0xB1    /**< 瞬时功率，R */
#define MAX17260_REG_AVGPOWER       0xB3    /**< 平均功率，R */
#define MAX17260_REG_IALRTTH        0xB4    /**< 电流告警阈值，R/W，POR 0x7F80 */
#define MAX17260_REG_CONFIG2        0xBB    /**< 第二配置，R/W，POR 0x3658 */

/* 0xD 页 */
#define MAX17260_REG_MODELCFG       0xDB    /**< Model m5 EZ 配置，R/W，POR 0x0000 */
/* 0xD4~0xDF 序列号（8 字）需先清 Config2.AtRateEn 与 Config2.DPEn */
#define MAX17260_REG_SN_WORD0       0xD4
#define MAX17260_REG_SN_WORD1       0xD5
#define MAX17260_REG_SN_WORD2       0xD9
#define MAX17260_REG_SN_WORD3       0xDA
#define MAX17260_REG_SN_WORD4       0xDC
#define MAX17260_REG_SN_WORD5       0xDD
#define MAX17260_REG_SN_WORD6       0xDE
#define MAX17260_REG_SN_WORD7       0xDF

#define MAX17260_SN_WORDS          8       /**< 序列号字数（0xD4~0xDF，跳过 0xD6/0xD7/0xD8/0xDB） */

/* ══════════════════════════════════════════════════════════════════════════
 * 复位（POR）默认值
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_STATUS_POR         0x8082u
#define MAX17260_VALRTTH_POR        0xFF00u
#define MAX17260_TALRTTH_POR        0x7F80u
#define MAX17260_SALRTTH_POR        0xFF00u
#define MAX17260_DESIGNCAP_POR      0x0BB8u
#define MAX17260_CONFIG_POR         0x2210u
#define MAX17260_ICHGTERM_POR       0x0640u
#define MAX17260_VEMPTY_POR         0xA561u
#define MAX17260_MAXMINVOLT_POR     0x00FFu
#define MAX17260_MAXMINCURR_POR     0x807Fu
#define MAX17260_MAXMINTEMP_POR     0x807Fu
#define MAX17260_IALRTTH_POR        0x7F80u
#define MAX17260_CONFIG2_POR        0x3658u
#define MAX17260_MODELCFG_POR       0x0000u

/* ══════════════════════════════════════════════════════════════════════════
 * Status 寄存器位（手册 Table 7）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_STATUS_BR          (1u << 15) /**< 电池移除（须软件清零） */
#define MAX17260_STATUS_SMX         (1u << 14) /**< RepSOC > SAlrtTh.MAX */
#define MAX17260_STATUS_TMX         (1u << 13) /**< Temp > TAlrtTh.MAX */
#define MAX17260_STATUS_VMX         (1u << 12) /**< VCell > VAlrtTh.MAX */
#define MAX17260_STATUS_BI          (1u << 11) /**< 电池插入（须软件清零） */
#define MAX17260_STATUS_SMN         (1u << 10) /**< RepSOC < SAlrtTh.MIN */
#define MAX17260_STATUS_TMN         (1u << 9)  /**< Temp < TAlrtTh.MIN */
#define MAX17260_STATUS_VMN         (1u << 8)  /**< VCell < VAlrtTh.MIN */
#define MAX17260_STATUS_DSOCI       (1u << 7)  /**< RepSOC 跨过 1% 整数边界 */
#define MAX17260_STATUS_IMX         (1u << 6)  /**< Current > IAlrtTh.MAX */
#define MAX17260_STATUS_BST         (1u << 3)  /**< 电池状态：1=不在位，0=在位 */
#define MAX17260_STATUS_IMN         (1u << 2)  /**< Current < IAlrtTh.MIN */
#define MAX17260_STATUS_POR         (1u << 1)  /**< 发生 POR 事件（须软件清零） */

/** Status 中需软件清零的位（Br/BI/DSOCI/POR） */
#define MAX17260_STATUS_SW_CLEAR_MASK   (MAX17260_STATUS_BR | MAX17260_STATUS_BI | \
                                         MAX17260_STATUS_DSOCI | MAX17260_STATUS_POR)

/** Status 中其余可清告警位（阈值型，sticky 模式由 Config 决定） */
#define MAX17260_STATUS_ALERT_MASK      (MAX17260_STATUS_SMX | MAX17260_STATUS_TMX | \
                                         MAX17260_STATUS_VMX | MAX17260_STATUS_SMN | \
                                         MAX17260_STATUS_TMN | MAX17260_STATUS_VMN | \
                                         MAX17260_STATUS_IMX | MAX17260_STATUS_IMN)

/** Status 全部可清位 */
#define MAX17260_STATUS_CLEAR_MASK      (MAX17260_STATUS_SW_CLEAR_MASK | \
                                         MAX17260_STATUS_ALERT_MASK)

/* ══════════════════════════════════════════════════════════════════════════
 * Config 寄存器位（手册 Table 5，POR 0x2210）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_CONFIG_TSEL       (1u << 15) /**< 0=内部 die，1=外部 NTC */
#define MAX17260_CONFIG_SS         (1u << 14) /**< SOC ALRT sticky */
#define MAX17260_CONFIG_TS         (1u << 13) /**< Temp ALRT sticky */
#define MAX17260_CONFIG_VS         (1u << 12) /**< VCell ALRT sticky */
#define MAX17260_CONFIG_IS         (1u << 11) /**< Current ALRT sticky */
#define MAX17260_CONFIG_THSH       (1u << 10) /**< TH 引脚关断使能 */
#define MAX17260_CONFIG_TEN        (1u << 9)  /**< 温度通道使能 */
#define MAX17260_CONFIG_TEX        (1u << 8)  /**< 1=温度由主机提供 */
#define MAX17260_CONFIG_SHDN       (1u << 7)  /**< 强制进入关断 */
#define MAX17260_CONFIG_COMMSH     (1u << 6)  /**< 总线拉低超时关断 */
#define MAX17260_CONFIG_ETHRM      (1u << 4)  /**< NTC 使能 */
#define MAX17260_CONFIG_FTHRM      (1u << 3)  /**< 强制 NTC 偏置 */
#define MAX17260_CONFIG_AEN        (1u << 2)  /**< Alert 引脚使能 */
#define MAX17260_CONFIG_BEI        (1u << 1)  /**< 电池插入告警使能 */
#define MAX17260_CONFIG_BER        (1u << 0)  /**< 电池移除告警使能 */

/* ══════════════════════════════════════════════════════════════════════════
 * Config2 寄存器位（手册 Table 6，POR 0x3658）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_CONFIG2_D8        (1u << 8)  /**< 固定为 1 */
#define MAX17260_CONFIG2_ATRATEEN  (1u << 6)  /**< AtRate 计算使能 */
#define MAX17260_CONFIG2_DPEN      (1u << 5)  /**< 动态功率计算使能 */
#define MAX17260_CONFIG2_POWR_SHIFT 4         /**< AvgPower 时间常数 4 位字段 */
#define MAX17260_CONFIG2_POWR_MASK  (0x0Fu << MAX17260_CONFIG2_POWR_SHIFT)
#define MAX17260_CONFIG2_DSOCEN    (1u << 2)  /**< dSOCi Alert 使能 */
#define MAX17260_CONFIG2_TALRTEN   (1u << 1)  /**< 温度告警使能 */
#define MAX17260_CONFIG2_LDMDL     (1u << 0)  /**< 主机置 1 通知固件模型加载完成 */

/* ══════════════════════════════════════════════════════════════════════════
 * ModelCfg 寄存器位（手册 Table 4，POR 0x0000）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_MODELCFG_REFRESH  (1u << 15) /**< 写 1 触发模型重载，固件完成后清 0 */
#define MAX17260_MODELCFG_R100     (1u << 13) /**< 0=10k NTC，1=100k NTC */
#define MAX17260_MODELCFG_VCHG     (1u << 10) /**< 0=4.2V，1=4.3~4.4V */
#define MAX17260_MODELCFG_MODELID_SHIFT 4    /**< ModelID 4 位字段 */
#define MAX17260_MODELCFG_MODELID_MASK (0x0Fu << MAX17260_MODELCFG_MODELID_SHIFT)
#define MAX17260_MODELCFG_CSEL     (1u << 0)  /**< 0=low-side，1=high-side（启动自动） */

/* ══════════════════════════════════════════════════════════════════════════
 * VEmpty 寄存器字段（手册 Table 3，POR 0xA561）
 * VE[15:7] 占 9 位（10mV/LSB），VR[6:0] 占 7 位（40mV/LSB）
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX17260_VEMPTY_VE_SHIFT   7
#define MAX17260_VEMPTY_VE_MASK    (0x1FFu << MAX17260_VEMPTY_VE_SHIFT) /**< 9 位 VE 字段 */
#define MAX17260_VEMPTY_VR_MASK    (0x7Fu << 0)                          /**< 7 位 VR 字段 */

/* POR 默认值解析：VE = 0xA561 >> 7 = 0x14A = 330 → 3.30V
 *                  VR = 0xA561 & 0x7F = 0x61 = 97  → 3.88V */

/* ══════════════════════════════════════════════════════════════════════════
 * MaxMin* / AlrtTh 寄存器字段
 * ════════════════════════════════════════════════════════════════════════ */

/** MaxMinVolt：高 8 位 MaxVCELL，低 8 位 MinVCELL，20mV/LSB */
#define MAX17260_MAXMINVOLT_MAX_SHIFT  8
#define MAX17260_MAXMINVOLT_MAX_MASK   (0xFFu << MAX17260_MAXMINVOLT_MAX_SHIFT)
#define MAX17260_MAXMINVOLT_MIN_MASK   (0xFFu << 0)

/** MaxMinCurr：高 8 位 MaxCurrent，低 8 位 MinCurrent，0.4mV/RSENSE/LSB */
#define MAX17260_MAXMINCURR_MAX_SHIFT  8
#define MAX17260_MAXMINCURR_MAX_MASK   (0xFFu << MAX17260_MAXMINCURR_MAX_SHIFT)
#define MAX17260_MAXMINCURR_MIN_MASK   (0xFFu << 0)

/** MaxMinTemp：高 8 位 MaxTemperature，低 8 位 MinTemperature，1℃/LSB（补码） */
#define MAX17260_MAXMINTEMP_MAX_SHIFT  8
#define MAX17260_MAXMINTEMP_MAX_MASK   (0xFFu << MAX17260_MAXMINTEMP_MAX_SHIFT)
#define MAX17260_MAXMINTEMP_MIN_MASK   (0xFFu << 0)

/** VAlrtTh：高 8 位 VMAX，低 8 位 VMIN，20mV/LSB */
#define MAX17260_VALRTTH_MAX_SHIFT 8
#define MAX17260_VALRTTH_MAX_MASK  (0xFFu << MAX17260_VALRTTH_MAX_SHIFT)
#define MAX17260_VALRTTH_MIN_MASK  (0xFFu << 0)

/** TAlrtTh：高 8 位 TMAX，低 8 位 TMIN，1℃/LSB（补码） */
#define MAX17260_TALRTTH_MAX_SHIFT 8
#define MAX17260_TALRTTH_MAX_MASK  (0xFFu << MAX17260_TALRTTH_MAX_SHIFT)
#define MAX17260_TALRTTH_MIN_MASK  (0xFFu << 0)

/** SAlrtTh：高 8 位 SMAX，低 8 位 SMIN，1%/LSB */
#define MAX17260_SALRTTH_MAX_SHIFT 8
#define MAX17260_SALRTTH_MAX_MASK  (0xFFu << MAX17260_SALRTTH_MAX_SHIFT)
#define MAX17260_SALRTTH_MIN_MASK  (0xFFu << 0)

/** IAlrtTh：高 8 位 IMAX，低 8 位 IMIN，0.4mV/RSENSE/LSB（补码） */
#define MAX17260_IALRTTH_MAX_SHIFT 8
#define MAX17260_IALRTTH_MAX_MASK  (0xFFu << MAX17260_IALRTTH_MAX_SHIFT)
#define MAX17260_IALRTTH_MIN_MASK  (0xFFu << 0)

/* ══════════════════════════════════════════════════════════════════════════
 * 命令常量
 * ════════════════════════════════════════════════════════════════════════ */

/** POR 后稳定时间（手册：算法输出约 351ms 后有效） */
#define MAX17260_POR_STABILIZE_MS  351

#ifdef __cplusplus
}
#endif

#endif /* MAX17260_REGS_H */
