/**
 * @file    scheduler.h
 * @brief   轻量级时间片调度器与事件队列组件（通用裸机调度核心）
 * @note    支持周期任务时间片轮询，以及前后台事件驱动
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 最大支持的事件队列深度 */
#define SCHED_EVENT_QUEUE_SIZE   16

/**
 * @brief 事件对象定义
 */
typedef struct
{
    uint16_t id;      /**< 事件类型 ID */
    uint16_t param;   /**< 事件携带的 16 位简短参数 */
} Sched_Event_t;

/**
 * @brief 时间片任务项定义
 */
typedef struct
{
    void (*task_func)(void);  /**< 任务执行函数（必须非阻塞） */
    uint32_t interval_ms;     /**< 执行周期 (ms), 设为 0 表示每次主循环均检查 */
    uint32_t last_run_tick;   /**< 上次运行的时间戳 (内部管理) */
} Sched_Task_t;

/**
 * @brief 初始化调度器
 * @param tasks       任务表数组指针
 * @param task_count  任务数量
 */
void Scheduler_Init(Sched_Task_t *tasks, uint8_t task_count);

/**
 * @brief 在 SysTick 1ms 中断中调用，维护系统时间基准
 */
void Scheduler_TickInc(void);

/**
 * @brief 获取系统从启动至今的毫秒数
 */
uint32_t Scheduler_GetTick(void);

/**
 * @brief 向事件队列投递一个事件（可在 ISR 中安全调用）
 * @param event 待投递事件
 * @return true 投递成功; false 队列已满
 */
bool Scheduler_PostEvent(Sched_Event_t event);

/**
 * @brief 从事件队列拉取一个事件进行消费
 * @param out_event 接收事件指针
 * @return true 成功获取; false 队列为空
 */
bool Scheduler_GetEvent(Sched_Event_t *out_event);

/**
 * @brief 调度器主轮询函数，放置在 main() 的 while(1) 大循环中
 */
void Scheduler_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
