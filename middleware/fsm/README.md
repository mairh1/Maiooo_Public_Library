# FSM 轻量级有限状态机（v4.0.0）

基于查表法的同步派发状态机框架，纯 C99，**零平台依赖**：不含任何 OS API、中断操作、动态内存与静态全局状态，天然支持多实例，可直接复制到任意 MCU 工程。

## 文件组成

| 文件 | 说明 |
| --- | --- |
| `fsm.h` | 公共 API：类型定义、实例结构体、内联查询函数 |
| `fsm.c` | 核心实现：初始化、事件派发、状态切换 |
| `fsm_conf.h` | 全部配置宏：功能裁剪开关，可 `-D` 覆盖 |
| `test_fsm.c` | 主机端单元测试（仅依赖标准库，可选编译） |

## 设计要点

- **配置与实例分离**：状态表 / 动作表 / 钩子 / 上下文等不变配置集中到 `fsm_config_t`，由用户定义为 `const` 对象编译进 ROM；运行实例 `fsm_t` 仅保存配置指针与状态值，32 位平台 `sizeof(fsm_t) == 8` 字节（64 位平台为 16 字节）。
- **处理函数返回消费结果**：状态处理函数返回 `bool`，`true` 表示事件已处理；返回 `false` 时 `fsm_dispatch_event()` 返回 `FSM_ERR_UNHANDLED`，便于调用方感知未处理事件。
- **双重派发保护**：实例内 `transition_active` 与 `dispatching` 两个标志分别覆盖"转换回调中"与"处理函数中"；回调或处理函数重入派发 / 切换时返回 `FSM_ERR_REENTRANT`，避免递归失控。
- **状态处理函数查表**：每状态仅占 1 个函数指针（4 字节 ROM），事件合法性校验由用户处理函数负责并通过返回值告知框架，比"状态 × 事件转移表"模型的表体积小一个数量级。
- **同步派发**：`fsm_dispatch_event()` 直接调用当前状态处理函数，无事件队列，适合裸机与 OS 任务内直调。
- **可裁剪特性**：entry / exit 动作、切换钩子、上一状态回退三个低频特性均可通过 `fsm_conf.h` 编译期整体剔除，不用则零代码空间。

## 快速上手

```c
/* 1. 定义状态与事件枚举 */
typedef enum { STATE_IDLE, STATE_RUN, STATE_MAX } state_t;
typedef enum { EVT_START, EVT_STOP, EVT_MAX } event_t;

/* 2. 编写状态处理函数（返回 true 表示事件已处理） */
static bool on_idle(fsm_t * fsm, uint8_t evt)
{
    if (evt == EVT_START) { (void)fsm_set_state(fsm, STATE_RUN); return true; }
    return false;
}

static bool on_run(fsm_t * fsm, uint8_t evt)
{
    if (evt == EVT_STOP) { (void)fsm_set_state(fsm, STATE_IDLE); return true; }
    return false;
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

entry / exit 动作表、切换钩子与 `fsm_run_entry()` 的用法见 `fsm.h` 头部 Doxygen 示例。

## 配置裁剪矩阵

| 宏 | 默认 | 置 0 后剔除的内容 |
| --- | --- | --- |
| `FSM_CFG_USE_ENTRY_EXIT` | 1 | `fsm_config_t` 的 `entry_table` / `exit_table` 字段、`fsm_state_action_t` 类型、`fsm_run_entry()` 与切换时的动作调用 |
| `FSM_CFG_USE_HOOK` | 1 | `transition_hook` 字段、`fsm_transition_hook_t` 类型与切换后的钩子回调 |
| `FSM_CFG_USE_PREV_STATE` | 1 | `fsm_t` 的 `prev_state` 字段与 `fsm_get_prev_state()` API |

所有宏均带 `#ifndef` 默认值，可在 `fsm_conf.h` 中修改或用编译器 `-D` 覆盖；非法取值在编译期 `#error` 报错。

> **注意**：开关只接受数值 `0` 或 `1`。不要写成 `true` / `false`——在严格 `-std=c99` 下 `true` 会被预处理为 `0`，导致功能被静默关闭。

## RAM / ROM 预算

`sizeof(fsm_t)`：32 位平台 **8 字节**（含 `transition_active` 与 `dispatching` 两个标志，对齐后不变）；64 位平台 16 字节。配置字段全部位于 ROM。

状态表 ROM 开销：每状态 1 个函数指针（4 字节）+ 可选 entry / exit 表各 4 字节，由用户 const 数组承担，不计入下表。

### 历史实测（ARM Cortex-M0，GNU Arm Embedded 9.3.1，`-Os`）

| 配置 | `.text` | 说明 |
| --- | --- | --- |
| v2.0.0 全功能（基线） | 322 B | — |
| v3.0.0 全功能 | 194 B | -40% |
| v3.0.0 全裁剪 | 42 B | -87% |

### v4.0.0 参考实测（riscv64-unknown-elf-gcc 15.3.0，rv32imac/ilp32，`-Os`）

| 配置 | `.text` | 说明 |
| --- | --- | --- |
| 全功能 | 430 B | 含双重派发保护与未处理事件反馈 |
| 全裁剪 | 170 B | 仅剩派发与切换核心路径 |

> RISC-V 编码密度低于 Cortex-M0 Thumb，两组数据不可直接比较；v4 在 v3 基础上增加了派发保护、错误码细分与查询接口，ARM 平台的具体数值需用相应工具链复测。

## v3 → v4 迁移对照

| v3.0.0 | v4.0.0 | 说明 |
| --- | --- | --- |
| `void (*)(fsm_t *, uint8_t)` | `bool (*)(fsm_t *, uint8_t)` | 处理函数返回 `true` 表示事件已处理，`false` 时派发返回 `FSM_ERR_UNHANDLED` |
| `FSM_CFG_PARAM_CHECK` | 已移除 | 该宏从未生效，检查始终保留，移除以避免"死开关"误解 |
| `FSM_ERR_INVALID_STATE`（实例未初始化） | `FSM_ERR_NOT_INITIALIZED` | 错误码细分 |
| `FSM_ERR_INVALID_STATE`（handler 为 NULL） | `FSM_ERR_NO_HANDLER` | 错误码细分 |
| — | `FSM_ERR_UNHANDLED` | 新增：处理函数返回 `false` |
| — | `fsm_run_entry()` | 新增：补执行当前状态的 entry 动作 |
| — | `fsm_get_user_data()` | 新增：读取配置中的用户私有数据 |
| — | `fsm_is_transitioning()` | 新增：查询是否处于转换回调中 |
| `fsm_get_state` / `fsm_is_state` 对未初始化实例返回 0 / false | 返回 `FSM_STATE_INVALID` / false | 行为修正，避免误判为合法状态 |
| 回调内可重入派发 | 返回 `FSM_ERR_REENTRANT` | 处理函数与回调内均禁止重入派发 |
| 派发 / 回调期间可重新初始化 | 返回 `FSM_ERR_REENTRANT` | 新增：重入保护扩展到 `fsm_init()` |

## v2 → v3 迁移对照（历史）

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

## 测试

`test_fsm.c` 是主机端单元测试，覆盖正常转移、未处理事件、同状态切换、entry 补执行、回调 / 处理函数重入拦截、非法参数与未初始化实例等场景。

```sh
# 默认配置
gcc -std=c99 -Wall -Wextra -Wpedantic -I. test_fsm.c fsm.c -o test_fsm && ./test_fsm

# 全裁剪配置
gcc -std=c99 -Wall -Wextra -Wpedantic -DFSM_CFG_USE_ENTRY_EXIT=0 \
    -DFSM_CFG_USE_HOOK=0 -DFSM_CFG_USE_PREV_STATE=0 \
    test_fsm.c fsm.c -o test_fsm_min && ./test_fsm_min
```

## 设计约束与注意事项

- **禁止嵌套转移**：entry / exit 动作与切换钩子内调用 `fsm_set_state()` 会返回 `FSM_ERR_REENTRANT`，不会执行嵌套转换。
- **禁止重入派发**：状态处理函数与上述回调内调用 `fsm_dispatch_event()` 会返回 `FSM_ERR_REENTRANT`。
- **实例初始化**：首次调用 `fsm_init()` 前须将实例清零（静态实例天然清零；自动变量使用 `fsm_t fsm = {0}`）。空闲时可重新初始化；派发或转换回调期间调用 `fsm_init()` 会返回 `FSM_ERR_REENTRANT`。
- **同状态切换**：`fsm_set_state()` 对相同状态仍执行 exit → entry 链，需要跳过时先用 `fsm_is_state()` 自检。
- **并发互斥**：模块不关中断、不加锁；同一实例若在 ISR 与主循环中并发访问，互斥由调用方保证。
- **派发期间**：处理函数执行期间实例处于"派发中"，回调执行期间处于"转换中"，禁止重入调用本实例的任何 API。
- **多实例共享配置**：多个实例可指向同一份 `fsm_config_t`（此时 `user_data` 相同）；需要不同上下文的实例请各自定义配置对象。

## 许可证

WTFPL，与仓库整体一致。
