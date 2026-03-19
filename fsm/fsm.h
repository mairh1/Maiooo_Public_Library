/**
 * @file    fsm.h
 * @brief   轻量级有限状态机模块头文件
 * @author  Developer
 * @version 1.1.0
 * @date    2026-03-19
 *
 * @details
 * 提供基于查表法的有限状态机（FSM）驱动框架，适用于嵌入式场景。
 *
 * 核心设计：
 * - 状态处理函数以数组形式注册，通过状态值索引调用
 * - 用户可自定义状态枚举（uint8_t 范围），与状态表一一对应
 * - 支持传递 user_data 上下文指针，便于访问外部数据
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
 * // 3. 构建状态表
 * static const fsm_state_handler_t state_table[STATE_MAX] = {
 *     [STATE_IDLE] = on_idle,
 *     [STATE_RUN]  = on_run,
 * };
 *
 * // 4. 初始化并运行
 * fsm_t my_fsm;
 * fsm_init(&my_fsm, STATE_IDLE, state_table, STATE_MAX, NULL);
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
 * @brief 状态机实例结构体
 */
struct fsm_s {
    uint8_t current_state;                  /**< 当前状态值（索引状态表）        */
    uint8_t state_count;                    /**< 状态表有效条目数（用于边界检查） */
    const fsm_state_handler_t * state_table;/**< 状态处理函数表（数组）          */
    void * user_data;                       /**< 用户私有数据指针（可选）        */
};

/* ========================================================================== */
/*  函数声明                                                                   */
/* ========================================================================== */

/**
 * @brief 初始化状态机实例
 *
 * 将状态机绑定到指定的状态表，并设置初始状态。
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
 * @brief 强制设置当前状态
 *
 * 用于错误恢复、初始化跳转等特殊场景，不经过状态处理函数。
 *
 * @param[in,out] fsm        状态机实例指针
 * @param[in]     new_state  目标状态值（必须 < state_count）
 *
 * @retval FSM_OK                 状态已切换
 * @retval FSM_ERR_NULL_PTR       fsm 为 NULL
 * @retval FSM_ERR_INVALID_STATE  new_state >= state_count
 *
 * @warning 跳过状态处理函数直接切换，可能导致状态机上下文不一致。
 *          仅在确知目标状态安全时使用。
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

#endif /* FSM_H */
