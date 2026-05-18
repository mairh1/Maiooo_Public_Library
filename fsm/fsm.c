/**
 * @file    fsm.c
 * @brief   轻量级有限状态机模块实现
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-05-07
 *
 * @details
 * 基于查表法的状态机实现，所有公开接口均进行参数校验。
 * 支持 entry/exit 动作、状态切换钩子和状态回退。
 *
 * @ingroup algo_fsm
 */

#include "fsm.h"

/* ========================================================================== */
/*  私有宏定义                                                                 */
/* ========================================================================== */

/**
 * @brief 空指针快速检查宏
 *
 * @param ptr  待检查的指针
 * @param ret  检查失败时的返回值
 */
#define FSM_ASSERT_PTR(ptr, ret)  do { if ((ptr) == NULL) return (ret); } while (0)

/* ========================================================================== */
/*  私有函数                                                                   */
/* ========================================================================== */

/**
 * @brief 校验状态值是否在有效范围内
 *
 * @param[in] fsm          状态机实例（已确认非 NULL）
 * @param[in] state        待校验的状态值
 *
 * @retval true   状态值有效
 * @retval false  状态值 >= state_count
 */
static bool fsm_is_state_valid(const fsm_t * fsm, uint8_t state)
{
    return (fsm->state_table != NULL) && (state < fsm->state_count);
}

/**
 * @brief 执行状态切换的完整流程（exit -> 切换 -> entry -> hook）
 *
 * 此函数是 fsm_set_state 和 fsm_init_ex 的内部实现，
 * 封装了状态切换时的所有动作调用逻辑。
 *
 * @param[in,out] fsm        状态机实例指针
 * @param[in]     new_state  目标状态值
 */
static void fsm_do_transition(fsm_t * fsm, uint8_t new_state)
{
    uint8_t old_state = fsm->current_state;

    if (fsm->exit_table != NULL)
    {
        if (fsm->exit_table[old_state] != NULL)
        {
            fsm->exit_table[old_state](fsm);
        }
    }

    fsm->prev_state    = old_state;
    fsm->current_state = new_state;

    if (fsm->entry_table != NULL)
    {
        if (fsm->entry_table[new_state] != NULL)
        {
            fsm->entry_table[new_state](fsm);
        }
    }

    if (fsm->transition_hook != NULL)
    {
        fsm->transition_hook(fsm, old_state, new_state);
    }
}

/**
 * @brief 初始化状态机内部通用逻辑
 *
 * @param[out] fsm              状态机实例指针
 * @param[in]  initial_state    初始状态值
 * @param[in]  state_table      状态处理函数数组
 * @param[in]  state_count      状态表条目数
 * @param[in]  entry_table      进入动作表（可为 NULL）
 * @param[in]  exit_table       退出动作表（可为 NULL）
 * @param[in]  transition_hook  状态切换钩子（可为 NULL）
 * @param[in]  user_data        用户上下文指针（可为 NULL）
 *
 * @retval FSM_OK                 初始化成功
 * @retval FSM_ERR_NULL_PTR       fsm 或 state_table 为 NULL
 * @retval FSM_ERR_INVALID_STATE  initial_state >= state_count 或 state_count == 0
 */
static fsm_status_t fsm_init_internal(fsm_t * fsm,
                                       uint8_t initial_state,
                                       const fsm_state_handler_t * state_table,
                                       uint8_t state_count,
                                       const fsm_state_action_t * entry_table,
                                       const fsm_state_action_t * exit_table,
                                       fsm_transition_hook_t transition_hook,
                                       void * user_data)
{
    FSM_ASSERT_PTR(fsm,         FSM_ERR_NULL_PTR);
    FSM_ASSERT_PTR(state_table, FSM_ERR_NULL_PTR);

    if (state_count == 0)
    {
        return FSM_ERR_INVALID_STATE;
    }

    if (initial_state >= state_count)
    {
        return FSM_ERR_INVALID_STATE;
    }

    fsm->current_state    = initial_state;
    fsm->prev_state       = initial_state;
    fsm->state_count      = state_count;
    fsm->state_table      = state_table;
    fsm->entry_table      = entry_table;
    fsm->exit_table       = exit_table;
    fsm->transition_hook  = transition_hook;
    fsm->user_data        = user_data;

    return FSM_OK;
}

/* ========================================================================== */
/*  公开接口函数                                                                */
/* ========================================================================== */

fsm_status_t fsm_init(fsm_t * fsm,
                       uint8_t initial_state,
                       const fsm_state_handler_t * state_table,
                       uint8_t state_count,
                       void * user_data)
{
    return fsm_init_internal(fsm, initial_state, state_table, state_count,
                             NULL, NULL, NULL, user_data);
}

fsm_status_t fsm_init_ex(fsm_t * fsm,
                          uint8_t initial_state,
                          const fsm_state_handler_t * state_table,
                          uint8_t state_count,
                          const fsm_state_action_t * entry_table,
                          const fsm_state_action_t * exit_table,
                          fsm_transition_hook_t transition_hook,
                          void * user_data)
{
    return fsm_init_internal(fsm, initial_state, state_table, state_count,
                             entry_table, exit_table, transition_hook, user_data);
}

fsm_status_t fsm_dispatch_event(fsm_t * fsm, uint8_t event)
{
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_state_valid(fsm, fsm->current_state))
    {
        return FSM_ERR_INVALID_STATE;
    }

    fsm_state_handler_t handler = fsm->state_table[fsm->current_state];

    if (handler == NULL)
    {
        return FSM_ERR_INVALID_STATE;
    }

    handler(fsm, event);

    return FSM_OK;
}

uint8_t fsm_get_state(const fsm_t * fsm)
{
    FSM_ASSERT_PTR(fsm, FSM_STATE_INVALID);
    return fsm->current_state;
}

uint8_t fsm_get_prev_state(const fsm_t * fsm)
{
    FSM_ASSERT_PTR(fsm, FSM_STATE_INVALID);
    return fsm->prev_state;
}

fsm_status_t fsm_set_state(fsm_t * fsm, uint8_t new_state)
{
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_state_valid(fsm, new_state))
    {
        return FSM_ERR_INVALID_STATE;
    }

    fsm_do_transition(fsm, new_state);

    return FSM_OK;
}

bool fsm_is_state(const fsm_t * fsm, uint8_t state)
{
    if (fsm == NULL)
    {
        return false;
    }

    return (fsm->current_state == state);
}

fsm_status_t fsm_reset(fsm_t * fsm, uint8_t initial_state)
{
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (fsm->state_table == NULL || initial_state >= fsm->state_count)
    {
        return FSM_ERR_INVALID_STATE;
    }

    fsm_do_transition(fsm, initial_state);

    return FSM_OK;
}
