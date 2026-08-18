/**
 * @file    fsm.h
 * @brief   轻量级有限状态机模块头文件
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-05-07
 *
 * @details
 * 提供基于查表法的有限状态机（FSM）驱动框架，适用于嵌入式场景。
 *
 * 核心设计：
 * - 状态处理函数以数组形式注册，通过状态值索引调用
 * - 用户可自定义状态枚举（uint8_t 范围），与状态表一一对应
 * - 支持传递 user_data 上下文指针，便于访问外部数据
 * - 支持 entry/exit 动作，状态切换时自动执行初始化/清理逻辑
 * - 支持状态切换钩子，便于调试和日志记录
 * - 记录上一个状态，支持"回到上一个状态"等模式
 *
 * @ingroup algo_fsm
 *
 * @par 使用示例
 * @code
 * // 1. 定义状态和事件枚举
 * typedef enum { STATE_IDLE, STATE_RUN, STATE_MAX } state_t;
 * typedef enum { EVT_START, EVT_STOP, EVT_MAX } event_t;
 *
 * // 2. 编写状态处理函数
 * void on_idle(fsm_t *fsm, uint8_t evt) {
 *     if (evt == EVT_START) fsm_set_state(fsm, STATE_RUN);
 * }
 *
 * void on_run(fsm_t *fsm, uint8_t evt) {
 *     if (evt == EVT_STOP) fsm_set_state(fsm, STATE_IDLE);
 * }
 *
 * // 3. 编写进入/退出动作（可选）
 * void entry_run(fsm_t *fsm) { printf("Entering RUN state\n"); }
 * void exit_run(fsm_t *fsm) { printf("Exiting RUN state\n"); }
 *
 * // 4. 构建状态表和动作表
 * static const fsm_state_handler_t state_table[STATE_MAX] = {
 *     [STATE_IDLE] = on_idle,
 *     [STATE_RUN]  = on_run,
 * };
 *
 * static const fsm_state_action_t entry_table[STATE_MAX] = {
 *     [STATE_RUN] = entry_run,
 * };
 *
 * static const fsm_state_action_t exit_table[STATE_MAX] = {
 *     [STATE_RUN] = exit_run,
 * };
 *
 * // 5. 初始化并运行
 * fsm_t my_fsm;
 * fsm_init_ex(&my_fsm, STATE_IDLE, state_table, STATE_MAX,
 *             entry_table, exit_table, NULL, NULL);
 * fsm_dispatch_event(&my_fsm, EVT_START);
 * @endcode
 */

#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ========================================================================== */
/*  常量定义                                                                   */
/* ========================================================================== */

/**
 * @brief 无效状态标识符
 *
 * 用于 @ref fsm_get_state 在传入空指针时的返回值，
 * 表示状态机处于未定义状态。
 *
 * @attention 用户状态枚举不应使用 0xFF 作为合法值。
 */
#define FSM_STATE_INVALID   ((uint8_t)0xFF)

/* ========================================================================== */
/*  返回值定义                                                                  */
/* ========================================================================== */

/**
 * @brief 状态机操作返回码
 */
typedef enum {
    FSM_OK                =  0,   /**< 操作成功 */
    FSM_ERR_NULL_PTR      = -1,   /**< 空指针错误 */
    FSM_ERR_INVALID_STATE = -2,   /**< 状态值超出状态表范围 */
    FSM_ERR_INVALID_EVENT = -3    /**< 事件值超出事件表范围 */
} fsm_status_t;

/* ========================================================================== */
/*  类型定义                                                                   */
/* ========================================================================== */

/** @brief 前向声明 */
typedef struct fsm_s fsm_t;

/**
 * @brief 状态处理函数类型
 *
 * 每个状态对应一个处理函数，事件到来时由调度器调用。
 *
 * @param[in,out] fsm   状态机实例指针（可在函数内调用 fsm_set_state 等）
 * @param[in]     event 事件值（通常来自用户定义的枚举）
 */
typedef void (*fsm_state_handler_t)(fsm_t * fsm, uint8_t event);

/**
 * @brief 状态动作函数类型
 *
 * 用于状态进入/退出动作，在状态切换时自动调用。
 *
 * @param[in,out] fsm 状态机实例指针
 */
typedef void (*fsm_state_action_t)(fsm_t * fsm);

/**
 * @brief 状态切换钩子函数类型
 *
 * 在状态切换发生时调用，可用于调试、日志记录等。
 *
 * @param[in,out] fsm       状态机实例指针
 * @param[in]     old_state 切换前的状态值
 * @param[in]     new_state 切换后的状态值
 */
typedef void (*fsm_transition_hook_t)(fsm_t * fsm, uint8_t old_state, uint8_t new_state);

/**
 * @brief 状态机实例结构体
 */
struct fsm_s {
    uint8_t current_state;                      /**< 当前状态值（索引状态表）        */
    uint8_t prev_state;                         /**< 上一个状态值（用于状态回退）     */
    uint8_t state_count;                        /**< 状态表有效条目数（用于边界检查） */
    const fsm_state_handler_t * state_table;    /**< 状态处理函数表（数组）          */
    const fsm_state_action_t * entry_table;     /**< 进入动作表（数组，可选）        */
    const fsm_state_action_t * exit_table;      /**< 退出动作表（数组，可选）        */
    fsm_transition_hook_t transition_hook;      /**< 状态切换钩子（可选）            */
    void * user_data;                           /**< 用户私有数据指针（可选）        */
};

/* ========================================================================== */
/*  函数声明                                                                   */
/* ========================================================================== */

/**
 * @brief 初始化状态机实例（基础版本，兼容 v1.x API）
 *
 * 将状态机绑定到指定的状态表，并设置初始状态。
 * entry/exit 动作和切换钩子均设置为 NULL。
 *
 * @param[out] fsm            状态机实例指针
 * @param[in]  initial_state  初始状态值（索引，必须 < state_count）
 * @param[in]  state_table    状态处理函数数组（长度为 state_count）
 * @param[in]  state_count    状态表条目数（即状态总数）
 * @param[in]  user_data      用户上下文指针（不需要时传 NULL）
 *
 * @retval FSM_OK                 初始化成功
 * @retval FSM_ERR_NULL_PTR       fsm 或 state_table 为 NULL
 * @retval FSM_ERR_INVALID_STATE  initial_state >= state_count 或 state_count == 0
 *
 * @attention state_count 必须与状态表实际大小一致，否则边界检查将失效。
 */
fsm_status_t fsm_init(fsm_t * fsm,
                       uint8_t initial_state,
                       const fsm_state_handler_t * state_table,
                       uint8_t state_count,
                       void * user_data);

/**
 * @brief 初始化状态机实例（扩展版本，支持 entry/exit 动作和切换钩子）
 *
 * @param[out] fsm              状态机实例指针
 * @param[in]  initial_state    初始状态值（索引，必须 < state_count）
 * @param[in]  state_table      状态处理函数数组（长度为 state_count）
 * @param[in]  state_count      状态表条目数（即状态总数）
 * @param[in]  entry_table      进入动作表（可为 NULL，表示不使用进入动作）
 * @param[in]  exit_table       退出动作表（可为 NULL，表示不使用退出动作）
 * @param[in]  transition_hook  状态切换钩子（可为 NULL）
 * @param[in]  user_data        用户上下文指针（不需要时传 NULL）
 *
 * @retval FSM_OK                 初始化成功
 * @retval FSM_ERR_NULL_PTR       fsm 或 state_table 为 NULL
 * @retval FSM_ERR_INVALID_STATE  initial_state >= state_count 或 state_count == 0
 */
fsm_status_t fsm_init_ex(fsm_t * fsm,
                          uint8_t initial_state,
                          const fsm_state_handler_t * state_table,
                          uint8_t state_count,
                          const fsm_state_action_t * entry_table,
                          const fsm_state_action_t * exit_table,
                          fsm_transition_hook_t transition_hook,
                          void * user_data);

/**
 * @brief 分发事件到当前状态的处理函数
 *
 * 查找当前状态对应的处理函数并调用。若处理函数为 NULL，
 * 返回 @ref FSM_ERR_INVALID_STATE。
 *
 * @param[in,out] fsm    状态机实例指针
 * @param[in]     event  事件值
 *
 * @retval FSM_OK                 事件已处理
 * @retval FSM_ERR_NULL_PTR       fsm 为 NULL
 * @retval FSM_ERR_INVALID_STATE  当前状态无对应处理函数或状态值越界
 *
 * @note 状态切换在处理函数内部通过 @ref fsm_set_state 完成。
 *       事件值的合法性校验由用户处理函数负责。
 */
fsm_status_t fsm_dispatch_event(fsm_t * fsm, uint8_t event);

/**
 * @brief 获取当前状态值
 *
 * @param[in] fsm  状态机实例指针
 *
 * @return 当前状态值；若 fsm 为 NULL，返回 @ref FSM_STATE_INVALID。
 *
 * @attention 调用方必须先检查返回值是否为 @ref FSM_STATE_INVALID。
 */
uint8_t fsm_get_state(const fsm_t * fsm);

/**
 * @brief 获取上一个状态值
 *
 * @param[in] fsm  状态机实例指针
 *
 * @return 上一个状态值；若 fsm 为 NULL，返回 @ref FSM_STATE_INVALID。
 *         初始状态下 prev_state 等于 initial_state。
 *
 * @note 可用于实现"回到上一个状态"的逻辑。
 */
uint8_t fsm_get_prev_state(const fsm_t * fsm);

/**
 * @brief 强制设置当前状态
 *
 * 用于错误恢复、初始化跳转等特殊场景。
 * 如果配置了 entry/exit 动作表，将自动执行对应动作：
 * 1. 调用旧状态的 exit 动作（如果存在）
 * 2. 切换状态值
 * 3. 调用新状态的 entry 动作（如果存在）
 * 4. 调用切换钩子（如果存在）
 *
 * @param[in,out] fsm        状态机实例指针
 * @param[in]     new_state  目标状态值（必须 < state_count）
 *
 * @retval FSM_OK                 状态已切换
 * @retval FSM_ERR_NULL_PTR       fsm 为 NULL
 * @retval FSM_ERR_INVALID_STATE  new_state >= state_count
 *
 * @note 如果目标状态与当前状态相同，仍然会执行 entry/exit 动作。
 *       如需跳过相同状态的切换，请在调用前自行检查。
 */
fsm_status_t fsm_set_state(fsm_t * fsm, uint8_t new_state);

/**
 * @brief 检查状态机是否处于指定状态
 *
 * @param[in] fsm    状态机实例指针
 * @param[in] state  待检查的状态值
 *
 * @retval true   当前状态等于 state
 * @retval false  当前状态不等于 state，或 fsm 为 NULL
 */
bool fsm_is_state(const fsm_t * fsm, uint8_t state);

/**
 * @brief 重置状态机到初始状态
 *
 * 将状态机恢复到初始化时的状态，保留状态表和动作表配置。
 * 会调用当前状态的 exit 动作和初始状态的 entry 动作（如果配置）。
 *
 * @param[in,out] fsm            状态机实例指针
 * @param[in]     initial_state  重置后的目标状态值
 *
 * @retval FSM_OK                 重置成功
 * @retval FSM_ERR_NULL_PTR       fsm 为 NULL
 * @retval FSM_ERR_INVALID_STATE  initial_state >= state_count 或状态表未初始化
 *
 * @note fsm 必须已经通过 fsm_init 或 fsm_init_ex 初始化。
 */
fsm_status_t fsm_reset(fsm_t * fsm, uint8_t initial_state);

#endif /* FSM_H */
