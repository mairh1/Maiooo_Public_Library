/**
 * @file    fsm.c
 * @brief   轻量级有限状态机模块实现
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-08-28
 *
 * @details
 * 基于查表法的状态机实现：配置与实例分离，不变配置位于 const
 * fsm_config_t（ROM），实例仅保存配置指针与状态值。
 * entry / exit 动作、切换钩子、上一状态记录与参数校验均可通过
 * fsm_conf.h 编译期裁剪，全部关闭后仅剩派发与切换核心路径。
 *
 * @ingroup algo_fsm
 */

#include "fsm.h"

/* ========================================================================== */
/*  私有宏定义                                                                 */
/* ========================================================================== */

/**
 * @brief 空指针快速检查宏（FSM_CFG_PARAM_CHECK = 0 时展开为空）
 *
 * @param ptr  待检查的指针
 * @param ret  检查失败时的返回值
 */
#if FSM_CFG_PARAM_CHECK
#define FSM_ASSERT_PTR(ptr, ret)  do { if ((ptr) == NULL) return (ret); } while (0)
#else
#define FSM_ASSERT_PTR(ptr, ret)
#endif

/* ========================================================================== */
/*  私有函数                                                                   */
/* ========================================================================== */

#if FSM_CFG_PARAM_CHECK

/**
 * @brief 校验实例与状态值是否有效
 *
 * @param[in] fsm   状态机实例（已确认非 NULL）
 * @param[in] state 待校验的状态值
 *
 * @retval true   实例已绑定有效配置，且状态值在状态表范围内
 * @retval false  实例未初始化或状态值越界
 */
static bool fsm_is_state_valid(const fsm_t * fsm, uint8_t state)
{
    return (fsm->config != NULL) &&
           (fsm->config->state_table != NULL) &&
           (state < fsm->config->state_count);
}

#endif /* FSM_CFG_PARAM_CHECK */

/**
 * @brief 执行状态切换的完整流程（exit -> 切换 -> entry -> hook）
 *
 * @param[in,out] fsm        状态机实例指针（已确认有效）
 * @param[in]     new_state  目标状态值（已确认越界检查，或由调用方保证）
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
#if FSM_CFG_PARAM_CHECK
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);
    FSM_ASSERT_PTR(config, FSM_ERR_NULL_PTR);
    FSM_ASSERT_PTR(config->state_table, FSM_ERR_NULL_PTR);

    if ((config->state_count == 0) || (initial_state >= config->state_count))
    {
        return FSM_ERR_INVALID_STATE;
    }
#endif

    fsm->config        = config;
    fsm->current_state = initial_state;

#if FSM_CFG_USE_PREV_STATE
    fsm->prev_state    = initial_state;
#endif

    return FSM_OK;
}

fsm_status_t fsm_dispatch_event(fsm_t * fsm, uint8_t event)
{
    fsm_state_handler_t handler;

#if FSM_CFG_PARAM_CHECK
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_state_valid(fsm, fsm->current_state))
    {
        return FSM_ERR_INVALID_STATE;
    }
#endif

    handler = fsm->config->state_table[fsm->current_state];

    if (handler == NULL)
    {
        return FSM_ERR_INVALID_STATE;
    }

    handler(fsm, event);

    return FSM_OK;
}

fsm_status_t fsm_set_state(fsm_t * fsm, uint8_t new_state)
{
#if FSM_CFG_PARAM_CHECK
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_state_valid(fsm, new_state))
    {
        return FSM_ERR_INVALID_STATE;
    }
#endif

    fsm_do_transition(fsm, new_state);

    return FSM_OK;
}
