/**
 * @file    driver_template.h
 * @brief   符合架构规范的通用外设驱动模板接口
 */

#ifndef DRIVER_TEMPLATE_H
#define DRIVER_TEMPLATE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 错误码定义
 */
typedef enum
{
    DRV_OK           =  0,
    DRV_ERR_PARAM    = -1,
    DRV_ERR_TIMEOUT  = -2,
    DRV_ERR_BUSY     = -3,
    DRV_ERR_IO       = -4,
} Drv_Status_t;

/**
 * @brief 外设底层 Port 操作接口（由 BSP 实现并注入）
 */
typedef struct
{
    Drv_Status_t (*lowlevel_init)(void *hw_inst);
    Drv_Status_t (*send_byte)(void *hw_inst, uint8_t byte, uint32_t timeout_ticks);
    Drv_Status_t (*recv_byte)(void *hw_inst, uint8_t *byte, uint32_t timeout_ticks);
} Device_Port_Ops_t;

/**
 * @brief 驱动实例控制块
 */
typedef struct
{
    void                     *hw_instance;   /**< 硬件外设基地址指针 (如 USART1, SPI2) */
    const Device_Port_Ops_t  *ops;           /**< 底层操作句柄 */
    bool                      is_inited;     /**< 初始化标志 */
} Device_Driver_t;

/**
 * @brief 初始化设备驱动
 */
Drv_Status_t Device_Init(Device_Driver_t *driver, void *hw_instance, const Device_Port_Ops_t *ops);

/**
 * @brief 发送数据包（带防御性超时）
 */
Drv_Status_t Device_Write(Device_Driver_t *driver, const uint8_t *data, uint16_t length, uint32_t timeout_ticks);

/**
 * @brief 中断服务入口（由 IRQ Handler 调用）
 */
void Device_IrqHandler(Device_Driver_t *driver);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_TEMPLATE_H */
