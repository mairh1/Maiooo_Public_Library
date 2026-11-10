/**
 * @file    test_fsm.c
 * @brief   fsm 模块主机端单元测试（仅依赖标准库 assert / stdio）
 * @author  Maiooo
 * @version 4.0.0
 *
 * @details
 * 编译运行（在 fsm 目录下）：
 *   gcc -std=c99 -Wall -Wextra -Wpedantic -I. test_fsm.c fsm.c -o test_fsm && ./test_fsm
 *
 * 建议同时验证裁剪组合：
 *   gcc ... -DFSM_CFG_USE_ENTRY_EXIT=0 -DFSM_CFG_USE_HOOK=0 \
 *       -DFSM_CFG_USE_PREV_STATE=0 ...
 */

#include <stdio.h>
#include <string.h>
#include "fsm.h"

typedef enum { ST_IDLE, ST_RUN, ST_ERR, ST_MAX } test_state_t;
typedef enum { EVT_START, EVT_STOP, EVT_FAULT, EVT_OTHER } test_event_t;

#if FSM_CFG_USE_ENTRY_EXIT
static int s_entry_idle = 0;
static int s_entry_run  = 0;
static int s_exit_run   = 0;
#endif

#if FSM_CFG_USE_HOOK
static int s_hook_count = 0;
static uint8_t s_hook_old = 0xFF;
static uint8_t s_hook_new = 0xFF;
static int s_hook_nested_ret   = -99;
static int s_hook_dispatch_ret = -99;
static bool s_hook_transitioning = false;
static fsm_t * s_hook_fsm = NULL;
#endif

static int s_recurse_count = 0;
static int s_recurse_dispatch_ret = -99;
static fsm_t * s_recurse_fsm = NULL;

static fsm_t * s_reinit_fsm = NULL;
static const fsm_config_t * s_reinit_cfg = NULL;
static int s_handler_init_ret = -99;
#if FSM_CFG_USE_HOOK
static int s_hook_init_ret = -99;
#endif

static bool on_idle(fsm_t * fsm, uint8_t evt)
{
    switch (evt)
    {
    case EVT_START: (void)fsm_set_state(fsm, ST_RUN); return true;
    case EVT_FAULT: (void)fsm_set_state(fsm, ST_ERR); return true;
    default:        return false;
    }
}

static bool on_run(fsm_t * fsm, uint8_t evt)
{
    switch (evt)
    {
    case EVT_STOP:  (void)fsm_set_state(fsm, ST_IDLE); return true;
    case EVT_FAULT: (void)fsm_set_state(fsm, ST_ERR);  return true;
    default:        return false;
    }
}

static bool on_err(fsm_t * fsm, uint8_t evt)
{
    if (evt == EVT_STOP) { (void)fsm_set_state(fsm, ST_IDLE); return true; }
    return false;
}

static bool on_recurse(fsm_t * fsm, uint8_t evt)
{
    (void)fsm; (void)evt;
    s_recurse_count++;
    if (s_recurse_fsm != NULL)
    {
        /* 重入派发应被拒绝 */
        s_recurse_dispatch_ret = fsm_dispatch_event(s_recurse_fsm, EVT_START);
    }
    return true;
}

static bool on_reinit(fsm_t * fsm, uint8_t evt)
{
    (void)fsm; (void)evt;
    if (s_reinit_fsm != NULL)
    {
        /* 处理函数内重新初始化应被拒绝 */
        s_handler_init_ret = fsm_init(s_reinit_fsm, s_reinit_cfg, ST_IDLE);
    }
    return true;
}

#if FSM_CFG_USE_ENTRY_EXIT
static void entry_idle(fsm_t * fsm) { (void)fsm; s_entry_idle++; }
static void entry_run(fsm_t * fsm)  { (void)fsm; s_entry_run++; }
static void exit_run(fsm_t * fsm)   { (void)fsm; s_exit_run++; }
#endif

#if FSM_CFG_USE_HOOK
static void hook(fsm_t * fsm, uint8_t old_s, uint8_t new_s)
{
    (void)fsm;
    s_hook_count++;
    s_hook_old = old_s;
    s_hook_new = new_s;
    s_hook_transitioning = fsm_is_transitioning(fsm);
    if (s_hook_fsm != NULL)
    {
        /* 回调内嵌套切换应被拒绝 */
        s_hook_nested_ret = fsm_set_state(s_hook_fsm, ST_IDLE);
    }
}

static void hook_dispatch(fsm_t * fsm, uint8_t old_s, uint8_t new_s)
{
    (void)fsm; (void)old_s; (void)new_s;
    if (s_hook_fsm != NULL)
    {
        /* 回调内派发应被拒绝 */
        s_hook_dispatch_ret = fsm_dispatch_event(s_hook_fsm, EVT_STOP);
    }
}

static void hook_reinit(fsm_t * fsm, uint8_t old_s, uint8_t new_s)
{
    (void)fsm; (void)old_s; (void)new_s;
    if (s_reinit_fsm != NULL)
    {
        /* 回调内重新初始化应被拒绝 */
        s_hook_init_ret = fsm_init(s_reinit_fsm, s_reinit_cfg, ST_IDLE);
    }
}
#endif

static const fsm_state_handler_t s_table[ST_MAX] = {
    [ST_IDLE] = on_idle,
    [ST_RUN]  = on_run,
    [ST_ERR]  = on_err,
};

#if FSM_CFG_USE_ENTRY_EXIT
static const fsm_state_action_t s_entry[ST_MAX] = {
    [ST_IDLE] = entry_idle,
    [ST_RUN]  = entry_run,
};
static const fsm_state_action_t s_exit[ST_MAX] = {
    [ST_RUN] = exit_run,
};
#endif

static const fsm_config_t s_cfg = {
    .state_table = s_table,
    .state_count = ST_MAX,
#if FSM_CFG_USE_ENTRY_EXIT
    .entry_table = s_entry,
    .exit_table  = s_exit,
#endif
#if FSM_CFG_USE_HOOK
    .transition_hook = hook,
#endif
    .user_data = (void *)(uintptr_t)0x1234,
};

static int s_failures = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); } \
    else      { printf("  [FAIL] %s\n", msg); s_failures++; } \
} while (0)

int main(void)
{
    fsm_t fsm;
    fsm_status_t st;

    printf("config: ENTRY_EXIT=%d HOOK=%d PREV=%d\n",
           FSM_CFG_USE_ENTRY_EXIT, FSM_CFG_USE_HOOK, FSM_CFG_USE_PREV_STATE);
    printf("sizeof(fsm_t)=%zu sizeof(fsm_config_t)=%zu\n",
           sizeof(fsm_t), sizeof(fsm_config_t));

    /* 1. 基本 init / dispatch / transition */
    st = fsm_init(&fsm, &s_cfg, ST_IDLE);
    CHECK(st == FSM_OK, "init OK");
    CHECK(fsm_get_state(&fsm) == ST_IDLE, "initial state IDLE");
    CHECK(fsm_get_user_data(&fsm) == (void *)(uintptr_t)0x1234, "user_data readable");
#if FSM_CFG_USE_PREV_STATE
    CHECK(fsm_get_prev_state(&fsm) == ST_IDLE, "prev == initial");
#endif

    st = fsm_dispatch_event(&fsm, EVT_START);
    CHECK(st == FSM_OK, "dispatch EVT_START OK");
    CHECK(fsm_get_state(&fsm) == ST_RUN, "state -> RUN");
#if FSM_CFG_USE_ENTRY_EXIT
    CHECK(s_entry_run == 1, "entry_run called once");
#endif
#if FSM_CFG_USE_HOOK
    CHECK(s_hook_count == 1, "hook called once");
    CHECK(s_hook_old == ST_IDLE && s_hook_new == ST_RUN, "hook args correct");
    CHECK(s_hook_transitioning == true, "is_transitioning true in hook");
#endif

    st = fsm_dispatch_event(&fsm, EVT_STOP);
    CHECK(st == FSM_OK && fsm_get_state(&fsm) == ST_IDLE, "RUN -> IDLE");
#if FSM_CFG_USE_ENTRY_EXIT
    CHECK(s_exit_run == 1, "exit_run called once");
#endif

    /* 2. 未处理事件反馈 */
    st = fsm_dispatch_event(&fsm, EVT_OTHER);
    CHECK(st == FSM_ERR_UNHANDLED, "unhandled event -> FSM_ERR_UNHANDLED");
    CHECK(fsm_get_state(&fsm) == ST_IDLE, "state unchanged after unhandled");

    /* 3. 同状态切换仍触发 exit / entry / hook */
#if FSM_CFG_USE_ENTRY_EXIT
    s_entry_idle = 0; s_exit_run = 0;
#endif
#if FSM_CFG_USE_HOOK
    s_hook_count = 0;
#endif
    st = fsm_set_state(&fsm, ST_IDLE);
    CHECK(st == FSM_OK, "same-state set_state OK");
#if FSM_CFG_USE_ENTRY_EXIT
    CHECK(s_entry_idle == 1 && s_exit_run == 0, "same-state runs entry only (IDLE exit is NULL)");
#endif
#if FSM_CFG_USE_HOOK
    CHECK(s_hook_count == 1, "same-state still runs hook");
#endif

    /* 4. fsm_run_entry 补执行初始 entry */
#if FSM_CFG_USE_ENTRY_EXIT
    s_entry_idle = 0;
    st = fsm_run_entry(&fsm);
    CHECK(st == FSM_OK, "fsm_run_entry OK");
    CHECK(s_entry_idle == 1, "fsm_run_entry runs entry action");
#endif

    /* 5. 回调内嵌套切换被拦截 */
#if FSM_CFG_USE_HOOK
    s_hook_fsm = &fsm;
    s_hook_nested_ret = -99;
#endif
    st = fsm_set_state(&fsm, ST_RUN);
    CHECK(st == FSM_OK, "outer set_state OK");
#if FSM_CFG_USE_HOOK
    CHECK(s_hook_nested_ret == FSM_ERR_REENTRANT, "nested set_state in hook blocked");
    s_hook_fsm = NULL;
#endif

    /* 6. 回调内派发被拦截 */
#if FSM_CFG_USE_HOOK
    {
        static const fsm_config_t cfg_dispatch = {
            .state_table = s_table,
            .state_count = ST_MAX,
            .transition_hook = hook_dispatch,
        };
        fsm_t f2;
        s_hook_fsm = &f2;
        s_hook_dispatch_ret = -99;
        (void)fsm_init(&f2, &cfg_dispatch, ST_IDLE);
        (void)fsm_set_state(&f2, ST_RUN);
        CHECK(s_hook_dispatch_ret == FSM_ERR_REENTRANT, "dispatch in hook blocked");
        s_hook_fsm = NULL;
    }
#endif

    /* 7. 处理函数内递归派发被拦截 */
    {
        static const fsm_state_handler_t recurse_table[ST_MAX] = {
            [ST_IDLE] = on_recurse,
            [ST_RUN]  = on_recurse,
            [ST_ERR]  = on_recurse,
        };
        static const fsm_config_t recurse_cfg = {
            .state_table = recurse_table,
            .state_count = ST_MAX,
        };
        fsm_t f3;
        s_recurse_fsm = &f3;
        s_recurse_count = 0;
        s_recurse_dispatch_ret = -99;
        (void)fsm_init(&f3, &recurse_cfg, ST_IDLE);
        st = fsm_dispatch_event(&f3, EVT_START);
        CHECK(st == FSM_OK, "recursive dispatch outer returns OK");
        CHECK(s_recurse_count == 1, "recursive dispatch did not recurse");
        CHECK(s_recurse_dispatch_ret == FSM_ERR_REENTRANT, "recursive dispatch blocked");
        s_recurse_fsm = NULL;
    }

    /* 8. 处理函数 / 回调内重新初始化被拦截 */
    {
        static const fsm_state_handler_t reinit_table[ST_MAX] = {
            [ST_IDLE] = on_reinit,
            [ST_RUN]  = on_reinit,
            [ST_ERR]  = on_reinit,
        };
        static const fsm_config_t reinit_cfg = {
            .state_table = reinit_table,
            .state_count = ST_MAX,
        };
        fsm_t f5;
        s_reinit_fsm = &f5;
        s_reinit_cfg = &reinit_cfg;
        s_handler_init_ret = -99;
        (void)fsm_init(&f5, &reinit_cfg, ST_IDLE);
        st = fsm_dispatch_event(&f5, EVT_START);
        CHECK(st == FSM_OK, "outer dispatch with reinit returns OK");
        CHECK(s_handler_init_ret == FSM_ERR_REENTRANT, "reinit in handler blocked");
        s_reinit_fsm = NULL;

        /* 空闲时重新初始化应成功 */
        st = fsm_init(&f5, &reinit_cfg, ST_IDLE);
        CHECK(st == FSM_OK, "reinit when idle OK");
    }
#if FSM_CFG_USE_HOOK
    {
        static const fsm_config_t hook_reinit_cfg = {
            .state_table = s_table,
            .state_count = ST_MAX,
            .transition_hook = hook_reinit,
        };
        fsm_t f6;
        s_reinit_fsm = &f6;
        s_reinit_cfg = &hook_reinit_cfg;
        s_hook_init_ret = -99;
        (void)fsm_init(&f6, &hook_reinit_cfg, ST_IDLE);
        (void)fsm_set_state(&f6, ST_RUN);
        CHECK(s_hook_init_ret == FSM_ERR_REENTRANT, "reinit in hook blocked");
        s_reinit_fsm = NULL;
    }
#endif

    /* 9. 非法参数与未初始化实例 */
    CHECK(fsm_init(NULL, &s_cfg, ST_IDLE) == FSM_ERR_NULL_PTR, "init NULL fsm");
    CHECK(fsm_init(&fsm, NULL, ST_IDLE) == FSM_ERR_NULL_PTR, "init NULL cfg");
    CHECK(fsm_init(&fsm, &s_cfg, ST_MAX) == FSM_ERR_INVALID_STATE, "init bad state");
    CHECK(fsm_set_state(&fsm, ST_MAX) == FSM_ERR_INVALID_STATE, "set_state out of range");
    CHECK(fsm_dispatch_event(NULL, EVT_START) == FSM_ERR_NULL_PTR, "dispatch NULL fsm");
    CHECK(fsm_get_state(NULL) == FSM_STATE_INVALID, "get_state NULL");
    CHECK(fsm_is_state(NULL, ST_IDLE) == false, "is_state NULL");
    CHECK(fsm_get_user_data(NULL) == NULL, "get_user_data NULL");
    CHECK(fsm_is_transitioning(NULL) == false, "is_transitioning NULL");

    {
        fsm_t bad;
        memset(&bad, 0, sizeof(bad));
        CHECK(fsm_get_state(&bad) == FSM_STATE_INVALID, "uninitialized get_state -> INVALID");
        CHECK(fsm_is_state(&bad, ST_IDLE) == false, "uninitialized is_state -> false");
        CHECK(fsm_get_user_data(&bad) == NULL, "uninitialized get_user_data -> NULL");
        CHECK(fsm_dispatch_event(&bad, EVT_START) == FSM_ERR_NOT_INITIALIZED,
              "uninitialized dispatch -> NOT_INITIALIZED");
        CHECK(fsm_set_state(&bad, ST_IDLE) == FSM_ERR_NOT_INITIALIZED,
              "uninitialized set_state -> NOT_INITIALIZED");
#if FSM_CFG_USE_ENTRY_EXIT
        CHECK(fsm_run_entry(&bad) == FSM_ERR_NOT_INITIALIZED,
              "uninitialized run_entry -> NOT_INITIALIZED");
#endif
    }

    /* 10. NULL handler */
    {
        static const fsm_state_handler_t partial[ST_MAX] = {
            [ST_IDLE] = on_idle,
        };
        static const fsm_config_t partial_cfg = {
            .state_table = partial,
            .state_count = ST_MAX,
        };
        fsm_t f4;
        (void)fsm_init(&f4, &partial_cfg, ST_RUN);
        CHECK(fsm_dispatch_event(&f4, EVT_START) == FSM_ERR_NO_HANDLER,
              "NULL handler -> FSM_ERR_NO_HANDLER");
    }

    /* 11. 派发返回后标志清理 */
    CHECK(fsm_is_transitioning(&fsm) == false, "not transitioning after dispatch");

    printf("%s (%d failure(s))\n", s_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
           s_failures);
    return s_failures == 0 ? 0 : 1;
}
