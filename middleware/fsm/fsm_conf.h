/**
 * @file    fsm_conf.h
 * @brief   轻量级有限状态机模块配置
 * @author  Maiooo
 * @version 3.0.0
 * @date    2026-08-28
 *
 * @details
 * 全部配置宏集中在本文件，均带 #ifndef 默认值，可通过编译器命令行
 * -D 覆盖，禁止修改模块源码来配置。
 *
 * 功能开关置 0 后，对应的公共 API、结构体字段与核心实现一起被 #if
 * 裁剪，被裁剪的 API 不再存在（误用时编译期报错），不占用任何
 * Flash / RAM 空间。
 */

#ifndef FSM_CONF_H
#define FSM_CONF_H

/* ══════════════════════════════════════════════════════════════════════════
 *  功能开关（1 = 启用，0 = 编译期裁剪）
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief entry / exit 动作开关
 *
 * 置 1：fsm_config_t 含 entry_table / exit_table 字段，状态切换时自动
 *       调用新旧状态的进入 / 退出动作。
 * 置 0：剔除动作表字段与全部调用代码。
 */
#ifndef FSM_CFG_USE_ENTRY_EXIT
#define FSM_CFG_USE_ENTRY_EXIT  1
#endif

#if (FSM_CFG_USE_ENTRY_EXIT != 0) && (FSM_CFG_USE_ENTRY_EXIT != 1)
#error "FSM_CFG_USE_ENTRY_EXIT 必须为 0 或 1"
#endif

/**
 * @brief 状态切换钩子开关
 *
 * 置 1：fsm_config_t 含 transition_hook 字段，每次切换后回调
 *       hook(fsm, old_state, new_state)，可用于调试与日志。
 * 置 0：剔除钩子字段与回调代码。
 */
#ifndef FSM_CFG_USE_HOOK
#define FSM_CFG_USE_HOOK        1
#endif

#if (FSM_CFG_USE_HOOK != 0) && (FSM_CFG_USE_HOOK != 1)
#error "FSM_CFG_USE_HOOK 必须为 0 或 1"
#endif

/**
 * @brief 上一状态记录开关
 *
 * 置 1：fsm_t 含 prev_state 字段，提供 fsm_get_prev_state() 查询，
 *       可实现"回到上一个状态"等模式。
 * 置 0：剔除该字段与查询 API（实例再省 1 字节 RAM）。
 */
#ifndef FSM_CFG_USE_PREV_STATE
#define FSM_CFG_USE_PREV_STATE  1
#endif

#if (FSM_CFG_USE_PREV_STATE != 0) && (FSM_CFG_USE_PREV_STATE != 1)
#error "FSM_CFG_USE_PREV_STATE 必须为 0 或 1"
#endif

/**
 * @brief API 诊断校验开关
 *
 * 当前版本的公共 API 始终保留空指针、配置有效性与状态边界检查；该宏保留
 * 用于兼容现有构建配置，后续可用于裁剪非必要的诊断检查。
 */
#ifndef FSM_CFG_PARAM_CHECK
#define FSM_CFG_PARAM_CHECK     1
#endif

#if (FSM_CFG_PARAM_CHECK != 0) && (FSM_CFG_PARAM_CHECK != 1)
#error "FSM_CFG_PARAM_CHECK 必须为 0 或 1"
#endif

#endif /* FSM_CONF_H */
