/**
 * @file    scheduler.c
 * @brief   轻量级时间片调度器与事件队列实现
 */

#include "scheduler.h"

static volatile uint32_t s_system_ticks = 0;
static Sched_Task_t     *s_task_table = (void *)0;
static uint8_t           s_task_count = 0;

/* 环形事件队列 */
static Sched_Event_t     s_event_queue[SCHED_EVENT_QUEUE_SIZE];
static volatile uint8_t  s_eq_head = 0;
static volatile uint8_t  s_eq_tail = 0;

void Scheduler_Init(Sched_Task_t *tasks, uint8_t task_count)
{
    s_task_table = tasks;
    s_task_count = task_count;
    s_system_ticks = 0;
    s_eq_head = 0;
    s_eq_tail = 0;

    for (uint8_t i = 0; i < task_count; i++)
    {
        s_task_table[i].last_run_tick = 0;
    }
}

void Scheduler_TickInc(void)
{
    s_system_ticks++;
}

uint32_t Scheduler_GetTick(void)
{
    return s_system_ticks;
}

bool Scheduler_PostEvent(Sched_Event_t event)
{
    uint8_t next_head = (s_eq_head + 1) % SCHED_EVENT_QUEUE_SIZE;
    if (next_head == s_eq_tail)
    {
        return false; /* 队列已满 */
    }

    s_event_queue[s_eq_head] = event;
    s_eq_head = next_head;
    return true;
}

bool Scheduler_GetEvent(Sched_Event_t *out_event)
{
    if (s_eq_head == s_eq_tail)
    {
        return false; /* 队列为空 */
    }

    *out_event = s_event_queue[s_eq_tail];
    s_eq_tail = (s_eq_tail + 1) % SCHED_EVENT_QUEUE_SIZE;
    return true;
}

void Scheduler_Run(void)
{
    if (!s_task_table || s_task_count == 0)
    {
        return;
    }

    uint32_t now = s_system_ticks;

    for (uint8_t i = 0; i < s_task_count; i++)
    {
        Sched_Task_t *task = &s_task_table[i];
        if (!task->task_func)
        {
            continue;
        }

        if (task->interval_ms == 0)
        {
            /* 每次循环都执行 */
            task->task_func();
        }
        else if ((now - task->last_run_tick) >= task->interval_ms)
        {
            task->last_run_tick = now;
            task->task_func();
        }
    }
}
