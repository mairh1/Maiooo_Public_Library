# 时间片轮询 + 事件队列 + 状态机实现指南

## 1. 裸机运行模型演进

对于没有使用 RTOS 的单片机系统，程序运行模型通常经历以下演进：

1. **超级循环（Super Loop）**：`while(1)` 内部满屏 `delay_ms()`，CPU 在死等延时中空转，无法对外界输入做及时响应。
2. **时间片轮询（Time-Slice Polling）**：利用定时器中断（如 1ms SysTick）提供系统心跳，将各个任务按预定周期分频执行。
3. **时间片 + 事件驱动（Event-Driven）**：常规周期事务靠时间片轮询，突发动作（按键触发、串口收包）由事件队列解耦，随到随处理。

---

## 2. 混合调度架构设计

### 2.1 任务时间片调度表
时间片调度器通过静态任务数组管理周期任务，消耗极少 RAM（每个任务仅需约 8~12 字节）：

```c
typedef struct
{
    void (*task_func)(void);  /**< 任务函数指针 */
    uint32_t interval_ms;     /**< 执行周期 (毫秒) */
    uint32_t last_run_tick;   /**< 上次执行的时间戳 */
} Task_Item_t;

static Task_Item_t s_task_table[] = {
    { Task_KeyScan,       10,  0 },  /* 10ms 扫描一次按键 */
    { Task_SensorRead,   200,  0 },  /* 200ms 采集一次传感器数据 */
    { Task_DisplayFlush,  50,  0 },  /* 50ms 刷新一次显示界面 */
    { Task_EventDispatch,  0,  0 },  /* 0ms 表示每次循环都检查（事件队列处理） */
};
```

### 2.2 事件队列（Event Queue）
事件驱动负责在前台中断与后台任务之间搭起桥梁。
中断处理函数（ISR）只负责创建事件并压入队列：

```c
typedef enum
{
    EVENT_NONE = 0,
    EVENT_KEY_CLICK,
    EVENT_UART_PACKET_READY,
    EVENT_BATTERY_LOW,
} Event_Id_t;

typedef struct
{
    Event_Id_t id;
    uint32_t    param;  /**< 携带的简短参数（如按键键值、缓冲区长度） */
} Event_t;
```

#### 中断中投递：
```c
void EXTI0_IRQHandler(void)
{
    EXTI_ClearFlag();
    Event_t evt = { .id = EVENT_KEY_CLICK, .param = KEY_USER };
    EventQueue_Post(&evt); // 极短耗时，立即退出中断
}
```

#### 主循环中消费：
```c
void Task_EventDispatch(void)
{
    Event_t evt;
    while (EventQueue_Get(&evt))
    {
        switch (evt.id)
        {
            case EVENT_KEY_CLICK:
                App_OnKeyClick(evt.param);
                break;
            case EVENT_UART_PACKET_READY:
                App_OnUartPacket(evt.param);
                break;
            default:
                break;
        }
    }
}
```

---

## 3. 有限状态机（FSM）消灭长耗时

对于必须分步完成、且步骤间存在耗时的业务（如发 AT 指令等待回应、传感器上电等待稳定），**严禁在任务内使用 `delay`**，必须使用非阻塞状态机：

```c
typedef enum
{
    SENSOR_STATE_IDLE,
    SENSOR_STATE_TRIGGER,
    SENSOR_STATE_WAITING_DATA,
} SensorState_t;

void Task_SensorRead(void)
{
    static SensorState_t s_state = SENSOR_STATE_IDLE;
    static uint32_t s_start_tick = 0;

    switch (s_state)
    {
        case SENSOR_STATE_IDLE:
            Sensor_SendStartCommand();
            s_start_tick = SysTick_GetTick();
            s_state = SENSOR_STATE_WAITING_DATA;
            break;

        case SENSOR_STATE_WAITING_DATA:
            if (Sensor_IsDataReady())
            {
                Sensor_ReadValue();
                s_state = SENSOR_STATE_IDLE;
            }
            else if (SysTick_GetTick() - s_start_tick > 500) /* 500ms 超时 */
            {
                Sensor_HandleTimeout();
                s_state = SENSOR_STATE_IDLE;
            }
            break;
    }
}
```
通过状态机，当前任务每次检查仅需几十纳秒，未完成则直接让出 CPU，绝不拖累其他任务的执行。
