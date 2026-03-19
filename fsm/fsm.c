/**
 * @file    fsm.c
 * @brief   轻量级有限状态机模块实现
 * @author  Developer
 * @version 1.1.0
 * @date    2026-03-19
 *
 * @details
 * 基于查表法的状态机实现，所有公开接口均进行参数校验。
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

/* ========================================================================== */
/*  公开接口函数                                                                */
/* ========================================================================== */

fsm_status_t fsm_init(fsm_t * fsm,
                       uint8_t initial_state,
                       const fsm_state_handler_t * state_table,
                       uint8_t state_count,
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

    fsm->current_state = initial_state;
    fsm->state_count   = state_count;
    fsm->state_table   = state_table;
    fsm->user_data     = user_data;

    return FSM_OK;
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

fsm_status_t fsm_set_state(fsm_t * fsm, uint8_t new_state)
{
    FSM_ASSERT_PTR(fsm, FSM_ERR_NULL_PTR);

    if (!fsm_is_state_valid(fsm, new_state))
    {
        return FSM_ERR_INVALID_STATE;
    }

    fsm->current_state = new_state;

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
