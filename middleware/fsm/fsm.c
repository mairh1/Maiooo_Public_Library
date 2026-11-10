/**
 * @file    fsm.c
 * @brief   轻量级有限状态机模块实现
 * @author  Maiooo
 * @version 4.0.0
 * @date    2026-08-28
 *
 * @details
 * 基于查表法的状态机实现：配置与实例分离，不变配置位于 const
 * fsm_config_t（ROM），实例仅保存配置指针与状态值。
 * entry / exit 动作、切换钩子、上一状态记录均可通过 fsm_conf.h
 * 编译期裁剪，全部关闭后仅剩派发与切换核心路径。
 *
 * 派发保护：实例内 transition_active 与 dispatching 两个标志分别
 * 覆盖"转换回调中"与"处理函数中"，任何重入派发 / 切换都会返回
 * FSM_ERR_REENTRANT，避免递归失控。
 *
 * @ingroup algo_fsm
 */

#include "fsm.h"

/* ========================================================================== */
/*  私有宏定义                                                                 */
/* ========================================================================== */

/**
 * @brief 空指针检查宏（所有配置模式均保留）
 *
 * @param ptr  待检查的指针
 * @param ret  检查失败时的返回值
 */
#define FSM_ASSERT_PTR(ptr, ret)  do { if ((ptr) == NULL) return (ret); } while (0)

/* ========================================================================== */
/*  私有函数                                                                   */
/* ========================================================================== */

/**
 * @brief 校验实例是否绑定有效配置
 *
 * @param[in] fsm  状态机实例（已确认非 NULL）
 *
 * @retval true   实例已绑定有效配置
 * @retval false  实例未初始化
 */
static bool fsm_is_config_valid(const fsm_t * fsm)
{
    return (fsm->config != NULL) &&
           (fsm->config->state_table != NULL) &&
           (fsm->config->state_count != 0);
}

/**
 * @brief 校验状态值是否在状态表范围内
 *
 * @param[in] fsm   状态机实例（已确认绑定有效配置）
 * @param[in] state 待校验的状态值
 *
 * @retval true   状态值在范围内
 * @retval false  状态值越界
 */
static bool fsm_is_state_in_range(const fsm_t * fsm, uint8_t state)
{
    return (state < fsm->config->state_count);
}

/**
 * @brief 执行状态切换的完整流程（exit -> 切换 -> entry -> hook）
 *
 * @param[in,out] fsm        状态机实例指针（已确认有效）
 * @param[in]     new_state  目标状态值（已确认在状态表范围内）
 */
static void fsm_do_transition(fsm_t * fsm, uint8_t new_state)
{
#if FSM_CFG_USE_ENTRY_EXIT || FSM_CFG_USE_PREV_STATE || FSM_CFG_USE_HOOK
    uint8_t old_state = fsm->current_state;
#endif

#if FSM_CFG_USE_ENTRY_EXIT
    if (fsm->config->exit_table != NULL)
    {
        fsm_state_action_t exit_action = fsm->config->exit_table[old_state];

        if (exit_action != NULL)
        {
            exit_action(fsm);
        }
    }
#endif

    fsm->current_state = new_state;

#if FSM_CFG_USE_PREV_STATE
    fsm->prev_state = old_state;
#endif

#if FSM_CFG_USE_ENTRY_EXIT
    if (fsm->config->entry_table != NULL)
    {
        fsm_state_action_t entry_action = fsm->config->entry_table[new_state];

        if (entry_action != NULL)
        {
            entry_action(fsm);
        }
    }
#endif

#if FSM_CFG_USE_HOOK
    if (fsm->config->transition_hook != NULL)
    {
        fsm->config->transition_hook(fsm, old_state, new_state);
    }
#endif
}

/* ========================================================================== */
/*  公开接口函数                                                                */
/* ========================================================================== */

fsm_status_t fsm_init(fsm_t * fsm,
                      const fsm_config_t * config,
                      uint8_t initial_state)
{
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);
    FSM_ASSERT_PTR(config, FSM_ERR_NULL_PTR);
    FSM_ASSERT_PTR(config->state_table, FSM_ERR_NULL_PTR);

    if ((config->state_count == 0) || (initial_state >= config->state_count))
    {
        return FSM_ERR_INVALID_STATE;
    }

    fsm->config        = config;
    fsm->current_state = initial_state;

#if FSM_CFG_USE_PREV_STATE
    fsm->prev_state    = initial_state;
#endif
#if FSM_CFG_USE_ENTRY_EXIT || FSM_CFG_USE_HOOK
    fsm->transition_active = false;
#endif
    fsm->dispatching = false;

    return FSM_OK;
}

#if FSM_CFG_USE_ENTRY_EXIT

fsm_status_t fsm_run_entry(fsm_t * fsm)
{
    fsm_state_action_t entry_action;

    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_config_valid(fsm))
    {
        return FSM_ERR_NOT_INITIALIZED;
    }

    if (!fsm_is_state_in_range(fsm, fsm->current_state))
    {
        return FSM_ERR_INVALID_STATE;
    }

    if (fsm->config->entry_table == NULL)
    {
        return FSM_OK;
    }

    /* entry 动作属于转换回调，禁止重入 */
    if (fsm->transition_active)
    {
        return FSM_ERR_REENTRANT;
    }

    entry_action = fsm->config->entry_table[fsm->current_state];

    if (entry_action == NULL)
    {
        return FSM_OK;
    }

    fsm->transition_active = true;
    entry_action(fsm);
    fsm->transition_active = false;

    return FSM_OK;
}

#endif /* FSM_CFG_USE_ENTRY_EXIT */

fsm_status_t fsm_dispatch_event(fsm_t * fsm, uint8_t event)
{
    fsm_state_handler_t handler;
    bool handled;

    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_config_valid(fsm))
    {
        return FSM_ERR_NOT_INITIALIZED;
    }

    if (!fsm_is_state_in_range(fsm, fsm->current_state))
    {
        return FSM_ERR_INVALID_STATE;
    }

    /* 重入保护：处理函数或转换回调中不得再次派发 */
    if (fsm->dispatching)
    {
        return FSM_ERR_REENTRANT;
    }

#if FSM_CFG_USE_ENTRY_EXIT || FSM_CFG_USE_HOOK
    if (fsm->transition_active)
    {
        return FSM_ERR_REENTRANT;
    }
#endif

    handler = fsm->config->state_table[fsm->current_state];

    if (handler == NULL)
    {
        return FSM_ERR_NO_HANDLER;
    }

    fsm->dispatching = true;
    handled = handler(fsm, event);
    fsm->dispatching = false;

    return handled ? FSM_OK : FSM_ERR_UNHANDLED;
}

fsm_status_t fsm_set_state(fsm_t * fsm, uint8_t new_state)
{
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_config_valid(fsm))
    {
        return FSM_ERR_NOT_INITIALIZED;
    }

    if (!fsm_is_state_in_range(fsm, new_state))
    {
        return FSM_ERR_INVALID_STATE;
    }

#if FSM_CFG_USE_ENTRY_EXIT || FSM_CFG_USE_HOOK
    if (fsm->transition_active)
    {
        return FSM_ERR_REENTRANT;
    }
#endif

#if FSM_CFG_USE_ENTRY_EXIT || FSM_CFG_USE_HOOK
    fsm->transition_active = true;
#endif
    fsm_do_transition(fsm, new_state);
#if FSM_CFG_USE_ENTRY_EXIT || FSM_CFG_USE_HOOK
    fsm->transition_active = false;
#endif

    return FSM_OK;
}
