/**
 * @file    test_fsm.c
 * @brief   FSM 重入保护回归测试
 * @details 在 RV32 模拟器或 C99 主机上验证派发、切换与重新初始化的调用边界。
 */

#include <stdio.h>

#include "fsm.h"

enum
{
    STATE_IDLE,
    STATE_RUN,
    STATE_COUNT
};

static int s_failures;
static int s_handler_calls;
static int s_exit_calls;
static int s_entry_calls;
static int s_hook_calls;
static fsm_status_t s_nested_dispatch_status;
static fsm_status_t s_handler_init_status;
static fsm_status_t s_handler_set_status;
static fsm_status_t s_exit_init_status;
static fsm_status_t s_exit_set_status;
static fsm_status_t s_exit_dispatch_status;
static fsm_status_t s_entry_init_status;
static fsm_status_t s_hook_init_status;

#define CHECK(condition)                                                 \
    do                                                                   \
    {                                                                    \
        if (!(condition))                                                \
        {                                                                \
            printf("FAIL line %d: %s\n", __LINE__, #condition);         \
            s_failures++;                                                \
        }                                                                \
    } while (0)

static void
on_idle(fsm_t * fsm, uint8_t event)
{
    s_handler_calls++;

    if (event == 1)
    {
        s_nested_dispatch_status = fsm_dispatch_event(fsm, event);
        s_handler_init_status = fsm_init(fsm, fsm->config, STATE_IDLE);
        s_handler_set_status = fsm_set_state(fsm, STATE_RUN);
    }
}

static void
on_run(fsm_t * fsm, uint8_t event)
{
    (void)fsm;
    (void)event;
}

static const fsm_state_handler_t s_small_table[] = {on_idle};
static const fsm_config_t s_small_config = {
    .state_table = s_small_table,
    .state_count = 1
};

static void
on_exit(fsm_t * fsm)
{
    s_exit_calls++;
    s_exit_init_status = fsm_init(fsm, &s_small_config, STATE_IDLE);
    s_exit_set_status = fsm_set_state(fsm, STATE_IDLE);
    s_exit_dispatch_status = fsm_dispatch_event(fsm, 0);
}

static void
on_entry(fsm_t * fsm)
{
    s_entry_calls++;
    s_entry_init_status = fsm_init(fsm, &s_small_config, STATE_IDLE);
}

static void
on_hook(fsm_t * fsm, uint8_t old_state, uint8_t new_state)
{
    (void)old_state;
    (void)new_state;
    s_hook_calls++;
    s_hook_init_status = fsm_init(fsm, &s_small_config, STATE_IDLE);
}

static const fsm_state_handler_t s_state_table[] = {on_idle, on_run};
static const fsm_state_action_t s_exit_table[] = {on_exit, NULL};
static const fsm_state_action_t s_entry_table[] = {NULL, on_entry};
static const fsm_config_t s_config = {
    .state_table = s_state_table,
    .state_count = STATE_COUNT,
    .entry_table = s_entry_table,
    .exit_table = s_exit_table,
    .transition_hook = on_hook
};

int
main(void)
{
    fsm_t fsm = {0};

    CHECK(fsm_init(&fsm, &s_config, STATE_IDLE) == FSM_OK);
    CHECK(fsm_dispatch_event(&fsm, 1) == FSM_OK);
    CHECK(s_handler_calls == 1);
    CHECK(s_nested_dispatch_status == FSM_ERR_REENTRANT);
    CHECK(s_handler_init_status == FSM_ERR_REENTRANT);
    CHECK(s_handler_set_status == FSM_OK);
    CHECK(s_exit_calls == 1);
    CHECK(s_exit_init_status == FSM_ERR_REENTRANT);
    CHECK(s_exit_set_status == FSM_ERR_REENTRANT);
    CHECK(s_exit_dispatch_status == FSM_ERR_REENTRANT);
    CHECK(s_entry_calls == 1);
    CHECK(s_entry_init_status == FSM_ERR_REENTRANT);
    CHECK(s_hook_calls == 1);
    CHECK(s_hook_init_status == FSM_ERR_REENTRANT);
    CHECK(fsm_get_state(&fsm) == STATE_RUN);
    CHECK(fsm.config == &s_config);

    CHECK(fsm_init(&fsm, &s_config, STATE_IDLE) == FSM_OK);
    CHECK(fsm_set_state(&fsm, STATE_RUN) == FSM_OK);
    CHECK(s_exit_calls == 2);
    CHECK(s_entry_calls == 2);
    CHECK(s_hook_calls == 2);
    CHECK(s_exit_init_status == FSM_ERR_REENTRANT);
    CHECK(s_entry_init_status == FSM_ERR_REENTRANT);
    CHECK(s_hook_init_status == FSM_ERR_REENTRANT);
    CHECK(s_exit_dispatch_status == FSM_ERR_REENTRANT);
    CHECK(fsm_get_state(&fsm) == STATE_RUN);

    CHECK(fsm_init(&fsm, &s_small_config, STATE_IDLE) == FSM_OK);
    CHECK(fsm.config == &s_small_config);
    CHECK(fsm_get_state(&fsm) == STATE_IDLE);

    if (s_failures == 0)
    {
        puts("FSM tests passed");
    }

    return s_failures;
}
