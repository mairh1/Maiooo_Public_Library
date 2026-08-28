# FSM 轻量级有限状态机（v3.0.0）

基于查表法的同步派发状态机框架，纯 C99，**零平台依赖**：不含任何 OS API、中断操作、动态内存与静态全局状态，天然支持多实例，可直接复制到任意 MCU 工程。

## 文件组成

| 文件 | 说明 |
| --- | --- |
| `fsm.h` | 公共 API：类型定义、实例结构体、内联查询函数 |
| `fsm.c` | 核心实现：初始化、事件派发、状态切换 |
| `fsm_conf.h` | 全部配置宏：功能裁剪开关，可 `-D` 覆盖 |

## 设计要点

- **配置与实例分离**：状态表 / 动作表 / 钩子 / 上下文等不变配置集中到 `fsm_config_t`，由用户定义为 `const` 对象编译进 ROM；运行实例 `fsm_t` 仅保存配置指针与状态值，32 位平台 `sizeof(fsm_t) == 8` 字节（旧版 24 字节）。
- **状态处理函数查表**：每状态仅占 1 个函数指针（4 字节 ROM），事件合法性校验下沉到用户处理函数，比"状态 × 事件转移表"模型的表体积小一个数量级。
- **同步派发**：`fsm_dispatch_event()` 直接调用当前状态处理函数，无事件队列，适合裸机与 OS 任务内直调。
- **可裁剪特性**：entry / exit 动作、切换钩子、上一状态回退三个低频特性均可通过 `fsm_conf.h` 编译期整体剔除，不用则零代码空间。

## 快速上手

```c
/* 1. 定义状态与事件枚举 */
typedef enum { STATE_IDLE, STATE_RUN, STATE_MAX } state_t;
typedef enum { EVT_START, EVT_STOP, EVT_MAX } event_t;

/* 2. 编写状态处理函数（状态切换在函数内调用 fsm_set_state） */
static void on_idle(fsm_t * fsm, uint8_t evt)
{
    if (evt == EVT_START) { (void)fsm_set_state(fsm, STATE_RUN); }
}

static void on_run(fsm_t * fsm, uint8_t evt)
{
    if (evt == EVT_STOP) { (void)fsm_set_state(fsm, STATE_IDLE); }
}

/* 3. 构建状态表与 const 配置（ROM） */
static const fsm_state_handler_t s_table[STATE_MAX] = {
    [STATE_IDLE] = on_idle,
    [STATE_RUN]  = on_run,
};

static const fsm_config_t s_cfg = {
    .state_table = s_table,
    .state_count = STATE_MAX,
};

/* 4. 定义实例、初始化并运行 */
static fsm_t s_fsm;

fsm_init(&s_fsm, &s_cfg, STATE_IDLE);
fsm_dispatch_event(&s_fsm, EVT_START);      /* IDLE -> RUN */
```

entry / exit 动作表与切换钩子的用法见 `fsm.h` 头部 Doxygen 示例。

## 配置裁剪矩阵

| 宏 | 默认 | 置 0 后剔除的内容 |
| --- | --- | --- |
| `FSM_CFG_USE_ENTRY_EXIT` | 1 | `fsm_config_t` 的 `entry_table` / `exit_table` 字段、`fsm_state_action_t` 类型与切换时的动作调用 |
| `FSM_CFG_USE_HOOK` | 1 | `transition_hook` 字段、`fsm_transition_hook_t` 类型与切换后的钩子回调 |
| `FSM_CFG_USE_PREV_STATE` | 1 | `fsm_t` 的 `prev_state` 字段与 `fsm_get_prev_state()` API |
| `FSM_CFG_PARAM_CHECK` | 1 | 公开 API 的空指针 / 越界校验（调用方自行保证参数合法） |

所有宏均带 `#ifndef` 默认值，可在 `fsm_conf.h` 中修改或用编译器 `-D` 覆盖；非法取值在编译期 `#error` 报错。

## RAM / ROM 预算（实测）

工具链：GNU Arm Embedded 9.3.1（MounRiver），`-mcpu=cortex-m0 -mthumb -std=c99 -Os`，仅统计 `fsm.c` 目标文件：

| 配置 | `.text` | 相对 v2 |
| --- | --- | --- |
| v2.0.0 全功能（基线） | 322 B | — |
| v3.0.0 全功能 | 194 B | **-40%** |
| v3.0.0 全裁剪（4 个开关置 0） | 42 B | **-87%** |

`sizeof(fsm_t)`：v2 为 24 字节 → v3 为 **8 字节（-67%）**，其中 `prev_state` 裁剪不改变对齐后的 8 字节，但配置字段全部移入 ROM。

状态表 ROM 开销：每状态 1 个函数指针（4 字节）+ 可选 entry / exit 表各 4 字节，由用户 const 数组承担，不计入上表。

## v2 → v3 迁移对照

| v2.0.0 | v3.0.0 | 说明 |
| --- | --- | --- |
| `fsm_init(fsm, init, table, count, user_data)` | `fsm_init(fsm, &cfg, init)` | 合并单一初始化入口，表与上下文移入 `const fsm_config_t` |
| `fsm_init_ex(fsm, ..., entry, exit, hook, user_data)` | `fsm_init(fsm, &cfg, init)` | 同上，entry / exit / hook 写进配置对象 |
| `fsm_reset(fsm, init)` | `fsm_set_state(fsm, init)` | 二者行为本就相同，API 去重 |
| `fsm_get_state` / `fsm_is_state` / `fsm_get_prev_state` | 保留 | 改为头文件 `static inline`，零调用开销 |
| `FSM_ERR_INVALID_EVENT` | 已移除 | 从未有 API 返回该值的死代码 |
| 直接读写 `fsm_t` 成员 | 请走 API | 字段仍在结构体中可见，但语义归模块维护 |

## 集成方法

将 `fsm.h`、`fsm.c`、`fsm_conf.h` 三个文件加入工程（或仅添加 include 路径并把 `fsm.c` 加入编译），无其它依赖。需要裁剪特性时改 `fsm_conf.h` 或加 `-D`，不要改 `fsm.c`。

## 设计约束与注意事项

- **禁止嵌套转移**：entry / exit 动作与切换钩子内不得再调用 `fsm_set_state()`，嵌套会读到过期的 old / prev 状态值。
- **同状态切换**：`fsm_set_state()` 对相同状态仍执行 exit → entry 链，需要跳过时先用 `fsm_is_state()` 自检。
- **并发互斥**：模块不关中断、不加锁；同一实例若在 ISR 与主循环中并发访问，互斥由调用方保证。
- **派发期间**：处理函数执行期间实例处于"转移中"，禁止其他上下文并发调用本实例的任何 API。
- **多实例共享配置**：多个实例可指向同一份 `fsm_config_t`（此时 `user_data` 相同）；需要不同上下文的实例请各自定义配置对象。

## 许可证

WTFPL，与仓库整体一致。
