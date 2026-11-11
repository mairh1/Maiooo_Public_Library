/**
 * @file    driver_template.c
 * @brief   符合架构规范的通用外设驱动模板实现
 */

#include "driver_template.h"

Drv_Status_t Device_Init(Device_Driver_t *driver, void *hw_instance, const Device_Port_Ops_t *ops)
{
    if (!driver || !hw_instance || !ops)
    {
        return DRV_ERR_PARAM;
    }

    driver->hw_instance = hw_instance;
    driver->ops = ops;
    driver->is_inited = false;

    if (driver->ops->lowlevel_init)
    {
        Drv_Status_t status = driver->ops->lowlevel_init(driver->hw_instance);
        if (status != DRV_OK)
        {
            return status;
        }
    }

    driver->is_inited = true;
    return DRV_OK;
}

Drv_Status_t Device_Write(Device_Driver_t *driver, const uint8_t *data, uint16_t length, uint32_t timeout_ticks)
{
    if (!driver || !driver->is_inited || !data || length == 0)
    {
        return DRV_ERR_PARAM;
    }

    if (!driver->ops->send_byte)
    {
        return DRV_ERR_IO;
    }

    for (uint16_t i = 0; i < length; i++)
    {
        Drv_Status_t status = driver->ops->send_byte(driver->hw_instance, data[i], timeout_ticks);
        if (status != DRV_OK)
        {
            return status; /* 超时或硬件故障立即向上抛出错误 */
        }
    }

    return DRV_OK;
}

void Device_IrqHandler(Device_Driver_t *driver)
{
    if (!driver || !driver->is_inited)
    {
        return;
    }

    /* 中断处理逻辑：只清除中断标志并搬移数据，严禁阻塞等待 */
}
