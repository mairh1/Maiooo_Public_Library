/**
 * @file    fsm.h
 * @brief   轻量级有限状态机模块头文件
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-08-28
 *
 * @details
 * 提供基于查表法的有限状态机（FSM）框架，适用于嵌入式场景。
 *
 * 核心设计：
 * - 配置与实例分离：状态表 / 动作表 / 钩子 / 上下文等不变配置集中到
 *   @ref fsm_config_t，由用户定义为 const 对象（编译进 ROM）；运行实例
 *   @ref fsm_t 仅保存配置指针与状态值，32 位平台约占 8 字节 RAM
 * - 状态处理函数以数组形式注册，通过状态值索引调用
 * - 用户可自定义状态枚举（uint8_t 范围），与状态表一一对应
 * - entry / exit 动作、切换钩子、上一状态回退三个可选特性均可通过
 *   fsm_conf.h 编译期裁剪，不使用则零代码空间
 * - 纯 C99、零平台依赖：无 OS API、无动态内存、无静态全局状态，天然多实例
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
 * static void on_idle(fsm_t * fsm, uint8_t evt)
 * {
 *     if (evt == EVT_START) { fsm_set_state(fsm, STATE_RUN); }
 * }
 *
 * static void on_run(fsm_t * fsm, uint8_t evt)
 * {
 *     if (evt == EVT_STOP) { fsm_set_state(fsm, STATE_IDLE); }
 * }
 *
 * // 3. 编写进入/退出动作（可选，FSM_CFG_USE_ENTRY_EXIT = 1 时存在）
 * static void entry_run(fsm_t * fsm) { }   // 进入 RUN 时执行
 * static void exit_run(fsm_t * fsm)  { }   // 退出 RUN 时执行
 *
 * // 4. 构建状态表与静态配置（const，编译进 ROM）
 * static const fsm_state_handler_t s_table[STATE_MAX] = {
 *     [STATE_IDLE] = on_idle,
 *     [STATE_RUN]  = on_run,
 * };
 *
 * static const fsm_config_t s_cfg = {
 *     .state_table = s_table,
 *     .state_count = STATE_MAX,
 *     .entry_table = NULL,          // 不使用进入动作时传 NULL
 *     .exit_table  = NULL,          // 不使用退出动作时传 NULL
 * };
 *
 * // 5. 定义实例、初始化并运行
 * static fsm_t s_fsm;
 *
 * fsm_init(&s_fsm, &s_cfg, STATE_IDLE);
 * fsm_dispatch_event(&s_fsm, EVT_START);    // 触发 IDLE -> RUN 切换
 * @endcode
 *
 * @attention
 * - entry / exit 动作与切换钩子内不得再调用 @ref fsm_set_state；
 *   嵌套调用会返回 @ref FSM_ERR_REENTRANT。
 * - 模块本身不关中断、不加锁：同一实例若在 ISR 与主循环中并发访问，
 *   互斥由调用方保证。
 */

#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "fsm_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*  常量定义                                                                   */
/* ========================================================================== */

/**
 * @brief 无效状态标识符
 *
 * 用于 @ref fsm_get_state / @ref fsm_get_prev_state 在传入空指针时的
 * 返回值，表示状态机处于未定义状态。
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
    FSM_ERR_REENTRANT     = -3    /**< 转换回调中禁止嵌套切换 */
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

#if FSM_CFG_USE_ENTRY_EXIT

/**
 * @brief 状态动作函数类型（FSM_CFG_USE_ENTRY_EXIT = 1 时存在）
 *
 * 用于状态进入 / 退出动作，在状态切换时自动调用。
 *
 * @param[in,out] fsm 状态机实例指针
 */
typedef void (*fsm_state_action_t)(fsm_t * fsm);

#endif /* FSM_CFG_USE_ENTRY_EXIT */

#if FSM_CFG_USE_HOOK

/**
 * @brief 状态切换钩子函数类型（FSM_CFG_USE_HOOK = 1 时存在）
 *
 * 在状态切换发生时调用，可用于调试、日志记录等。
 *
 * @param[in,out] fsm       状态机实例指针
 * @param[in]     old_state 切换前的状态值
 * @param[in]     new_state 切换后的状态值
 */
typedef void (*fsm_transition_hook_t)(fsm_t * fsm, uint8_t old_state, uint8_t new_state);

#endif /* FSM_CFG_USE_HOOK */

/**
 * @brief 状态机静态配置（用户定义为 const 对象，编译进 ROM）
 *
 * 多实例可共享同一份配置；需要不同 user_data 上下文的实例
 * 请分别定义各自的配置对象。
 */
typedef struct {
    const fsm_state_handler_t * state_table;    /**< 状态处理函数表（长度为 state_count） */
    uint8_t state_count;                        /**< 状态表条目数（即状态总数）           */
#if FSM_CFG_USE_ENTRY_EXIT
    const fsm_state_action_t * entry_table;     /**< 进入动作表（长度为 state_count，可为 NULL） */
    const fsm_state_action_t * exit_table;      /**< 退出动作表（长度为 state_count，可为 NULL） */
#endif
#if FSM_CFG_USE_HOOK
    fsm_transition_hook_t transition_hook;      /**< 状态切换钩子（可为 NULL）           */
#endif
    void * user_data;                           /**< 用户私有数据指针（可为 NULL）       */
} fsm_config_t;

/**
 * @brief 状态机实例结构体
 *
 * 实例仅保存配置指针与运行状态，应用代码应通过 API 访问，
 * 不要直接读写成员。
 */
struct fsm_s {
    const fsm_config_t * config;                /**< 静态配置指针（通常位于 ROM）    */
    uint8_t current_state;                      /**< 当前状态值（索引状态表）        */
#if FSM_CFG_USE_PREV_STATE
    uint8_t prev_state;                         /**< 上一个状态值（用于状态回退）    */
#endif
    bool transition_active;                     /**< 状态转换回调是否正在执行      */
};

/* ========================================================================== */
/*  函数声明                                                                   */
/* ========================================================================== */

/**
 * @brief 初始化状态机实例
 *
 * 将状态机绑定到静态配置，并设置初始状态。
 * 不会自动执行初始状态的 entry 动作，如需执行请随后手动调用。
 *
 * @param[out] fsm            状态机实例指针
 * @param[in]  config         静态配置指针（生命周期必须覆盖实例使用期）
 * @param[in]  initial_state  初始状态值（索引，必须 < config->state_count）
 *
 * @retval FSM_OK                 初始化成功
 * @retval FSM_ERR_NULL_PTR       fsm、config 或 config->state_table 为 NULL
 * @retval FSM_ERR_INVALID_STATE  initial_state >= state_count 或 state_count == 0
 *
 * @attention state_count 必须与状态表实际大小一致，否则边界检查将失效。
 */
fsm_status_t fsm_init(fsm_t * fsm,
                      const fsm_config_t * config,
                      uint8_t initial_state);

/**
 * @brief 分发事件到当前状态的处理函数
 *
 * 查找当前状态对应的处理函数并调用。若处理函数为 NULL（该状态
 * 不处理任何事件），返回 @ref FSM_ERR_INVALID_STATE。
 *
 * @param[in,out] fsm    状态机实例指针
 * @param[in]     event  事件值
 *
 * @retval FSM_OK                 事件已处理
 * @retval FSM_ERR_NULL_PTR       fsm 为 NULL
 * @retval FSM_ERR_INVALID_STATE  实例未初始化、状态越界或当前状态无处理函数
 *
 * @note 状态切换在处理函数内部通过 @ref fsm_set_state 完成。
 *       事件值的合法性校验由用户处理函数负责。
 * @attention 处理函数调用期间实例处于"转移中"状态，禁止在其他上下文
 *            并发调用本实例的任何 API。
 */
fsm_status_t fsm_dispatch_event(fsm_t * fsm, uint8_t event);

/**
 * @brief 强制切换到指定状态
 *
 * 用于错误恢复、初始化跳转等特殊场景。如果配置了 entry / exit
 * 动作表与切换钩子，将自动执行：
 * 1. 调用旧状态的 exit 动作（如果存在）
 * 2. 更新 prev_state 与 current_state
 * 3. 调用新状态的 entry 动作（如果存在）
 * 4. 调用切换钩子（如果存在）
 *
 * @param[in,out] fsm        状态机实例指针
 * @param[in]     new_state  目标状态值（必须 < state_count）
 *
 * @retval FSM_OK                 状态已切换
 * @retval FSM_ERR_NULL_PTR       fsm 为 NULL
 * @retval FSM_ERR_INVALID_STATE  实例未初始化或 new_state >= state_count
 * @retval FSM_ERR_REENTRANT      在 entry / exit / hook 回调中嵌套切换
 *
 * @note 目标状态与当前状态相同时仍会执行 exit / entry 动作链，
 *       如需跳过相同状态的切换，请在调用前用 @ref fsm_is_state 自行检查。
 */
fsm_status_t fsm_set_state(fsm_t * fsm, uint8_t new_state);

/* ========================================================================== */
/*  内联查询函数（零调用开销）                                                   */
/* ========================================================================== */

/**
 * @brief 获取当前状态值
 *
 * @param[in] fsm  状态机实例指针
 *
 * @return 当前状态值；若 fsm 为 NULL，返回 @ref FSM_STATE_INVALID。
 */
static inline uint8_t fsm_get_state(const fsm_t * fsm)
{
    if (fsm == NULL)
    {
        return FSM_STATE_INVALID;
    }

    return fsm->current_state;
}

/**
 * @brief 检查状态机是否处于指定状态
 *
 * @param[in] fsm    状态机实例指针
 * @param[in] state  待检查的状态值
 *
 * @retval true   当前状态等于 state
 * @retval false  当前状态不等于 state，或 fsm 为 NULL
 */
static inline bool fsm_is_state(const fsm_t * fsm, uint8_t state)
{
    if (fsm == NULL)
    {
        return false;
    }

    return (fsm->current_state == state);
}

#if FSM_CFG_USE_PREV_STATE

/**
 * @brief 获取上一个状态值（FSM_CFG_USE_PREV_STATE = 1 时存在）
 *
 * @param[in] fsm  状态机实例指针
 *
 * @return 上一个状态值；若 fsm 为 NULL，返回 @ref FSM_STATE_INVALID。
 *         初始状态下 prev_state 等于 initial_state。
 *
 * @note 可用于实现"回到上一个状态"的逻辑：fsm_set_state(fsm, fsm_get_prev_state(fsm))。
 */
static inline uint8_t fsm_get_prev_state(const fsm_t * fsm)
{
    if (fsm == NULL)
    {
        return FSM_STATE_INVALID;
    }

    return fsm->prev_state;
}

#endif /* FSM_CFG_USE_PREV_STATE */

#ifdef __cplusplus
}
#endif

#endif /* FSM_H */
