/**
 * @file    usbpd_regs.h
 * @brief   PD 2.0 消息与电气常量
 * @details 依据 PD 2.0 v1.3 第 6、7 章；位移编码，不依赖 C 位域或主机字节序。
 * @note    仅处理 SOP；不与线缆通信。
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-09-28
 */
#ifndef USBPD_REGS_H
#define USBPD_REGS_H
#define USBPD_MAX_PDOS           7U     /**< 单帧最大对象数 */
#define USBPD_MAX_FRAME          30U    /**< 消息头及对象，不含 CRC */
#define USBPD_MAX_VOLTAGE_MV     20000U /**< 首版电压上限 */
#define USBPD_MAX_CURRENT_MA     3000U  /**< 首版电流上限 */
#define USBPD_SAFE_ZERO_MV       800U   /**< vSafe0V 上界 */
#define USBPD_SAFE_FIVE_MIN_MV   4750U  /**< vSafe5V 下界 */
#define USBPD_SAFE_FIVE_MAX_MV   5500U  /**< vSafe5V 上界 */
#define USBPD_SINK_DISCONNECT_MV 3670U  /**< vSinkDisconnect 检测阈值 */
#define USBPD_REV20              1U    /**< 消息头版本编码 */
#define USBPD_CTRL_GOODCRC       1U    /**< 由传输后端处理 */
#define USBPD_CTRL_GOTO_MIN      2U    /**< GiveBack 不支持 */
#define USBPD_CTRL_ACCEPT        3U    /**< 接受请求 */
#define USBPD_CTRL_REJECT        4U    /**< 拒绝请求 */
#define USBPD_CTRL_PING          5U    /**< 活动消息 */
#define USBPD_CTRL_PS_RDY        6U    /**< 电源就绪 */
#define USBPD_CTRL_GET_SOURCE    7U    /**< 查询 Source 能力 */
#define USBPD_CTRL_GET_SINK      8U    /**< 查询 Sink 能力 */
#define USBPD_CTRL_DR_SWAP       9U    /**< 数据角色交换 */
#define USBPD_CTRL_PR_SWAP       10U   /**< 电源角色交换 */
#define USBPD_CTRL_VCONN_SWAP    11U   /**< VCONN 角色交换 */
#define USBPD_CTRL_WAIT          12U   /**< 暂时无法接受 */
#define USBPD_CTRL_SOFT_RESET    13U   /**< 协议软复位 */
#define USBPD_DATA_SOURCE_CAP    1U    /**< Source 能力表 */
#define USBPD_DATA_REQUEST       2U    /**< 受电请求 */
#define USBPD_DATA_BIST          3U    /**< 测试模式 */
#define USBPD_DATA_SINK_CAP      4U    /**< Sink 能力表 */
#define USBPD_DATA_VDM           15U   /**< 厂商消息 */
#define USBPD_PDO_DUAL_POWER     0x20000000UL /**< 双电源角色能力 */
#define USBPD_RDO_MISMATCH       0x04000000UL /**< 所需电流超过可供能力 */
#define USBPD_RDO_NO_SUSPEND     0x01000000UL /**< 不支持 USB 挂起 */
#endif /* USBPD_REGS_H */
