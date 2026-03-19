/**
 * @file    bubble.h
 * @brief   冒泡排序算法模块头文件
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-03-19
 *
 * @details
 * 提供多种数据类型的冒泡排序实现，支持：
 * - 基础冒泡排序（升序 / 降序）
 * - 带提前终止优化的冒泡排序
 * - 支持 int、uint16_t、uint32_t 类型
 *
 * @ingroup algo_sort
 */

#ifndef BUBBLE_H
#define BUBBLE_H

#include "main.h"
#include <stdbool.h>

/* ========================================================================== */
/*  返回值定义                                                                  */
/* ========================================================================== */

/**
 * @brief 排序函数返回状态码
 */
typedef enum {
    BUBBLE_OK       =  0,   /**< 排序成功 */
    BUBBLE_ERR_NULL = -1,   /**< 空指针错误 */
    BUBBLE_ERR_SIZE = -2    /**< 长度参数错误（<= 0 或 > 类型最大值） */
} bubble_status_t;

/* ========================================================================== */
/*  int 类型排序函数                                                           */
/* ========================================================================== */

/**
 * @brief 冒泡排序 — int 升序
 *
 * 对 int 数组执行基础冒泡排序（升序）。
 *
 * @param[out] arr  待排序数组指针（排序结果直接写入原数组）
 * @param[in]  n    数组元素个数，必须 > 0
 *
 * @retval BUBBLE_OK       排序成功
 * @retval BUBBLE_ERR_NULL arr 为 NULL
 * @retval BUBBLE_ERR_SIZE n <= 0
 *
 * @par 复杂度
 * | 情况   | 时间复杂度 | 空间复杂度 |
 * |--------|-----------|-----------|
 * | 最好   | O(n^2)    | O(1)      |
 * | 最差   | O(n^2)    | O(1)      |
 * | 平均   | O(n^2)    | O(1)      |
 *
 * @attention 会修改原始数组内容。
 *
 * @par 示例
 * @code
 * int data[] = {5, 2, 8, 1, 9};
 * bubble_status_t ret = bubble_sort_int_asc(data, 5);
 * // data: {1, 2, 5, 8, 9}
 * @endcode
 *
 * @see bubble_sort_int_optimized  带提前终止优化的版本
 */
bubble_status_t bubble_sort_int_asc(int arr[], int n);

/**
 * @brief 冒泡排序 — int 降序
 *
 * 对 int 数组执行基础冒泡排序（降序）。
 *
 * @param[out] arr  待排序数组指针
 * @param[in]  n    数组元素个数，必须 > 0
 *
 * @retval BUBBLE_OK       排序成功
 * @retval BUBBLE_ERR_NULL arr 为 NULL
 * @retval BUBBLE_ERR_SIZE n <= 0
 *
 * @see bubble_sort_int_asc
 */
bubble_status_t bubble_sort_int_desc(int arr[], int n);

/**
 * @brief 冒泡排序 — int 升序（带提前终止优化）
 *
 * 在基础版本上增加交换标志检测：若某一轮未发生任何交换，
 * 说明数组已有序，立即终止排序。
 *
 * @param[out] arr  待排序数组指针
 * @param[in]  n    数组元素个数，必须 > 0
 *
 * @retval BUBBLE_OK       排序成功
 * @retval BUBBLE_ERR_NULL arr 为 NULL
 * @retval BUBBLE_ERR_SIZE n <= 0
 *
 * @par 复杂度
 * | 情况   | 时间复杂度 | 空间复杂度 |
 * |--------|-----------|-----------|
 * | 最好   | O(n)      | O(1)      |
 * | 最差   | O(n^2)    | O(1)      |
 * | 平均   | O(n^2)    | O(1)      |
 *
 * @par 示例
 * @code
 * int data[] = {1, 2, 3, 4, 5};   // 已有序
 * bubble_sort_int_optimized(data, 5);
 * // 仅一轮扫描即退出，实际比较次数 = n-1
 * @endcode
 *
 * @see bubble_sort_int_asc
 */
bubble_status_t bubble_sort_int_optimized(int arr[], int n);

/* ========================================================================== */
/*  uint16_t 类型排序函数                                                      */
/* ========================================================================== */

/**
 * @brief 冒泡排序 — uint16_t 升序
 *
 * @param[out] arr  待排序数组指针
 * @param[in]  n    数组元素个数，必须 > 0 且 <= UINT16_MAX
 *
 * @retval BUBBLE_OK       排序成功
 * @retval BUBBLE_ERR_NULL arr 为 NULL
 * @retval BUBBLE_ERR_SIZE n 无效
 *
 * @see bubble_sort_uint16_optimized
 */
bubble_status_t bubble_sort_uint16_asc(uint16_t arr[], uint16_t n);

/**
 * @brief 冒泡排序 — uint16_t 升序（带提前终止优化）
 *
 * @param[out] arr  待排序数组指针
 * @param[in]  n    数组元素个数，必须 > 0 且 <= UINT16_MAX
 *
 * @retval BUBBLE_OK       排序成功
 * @retval BUBBLE_ERR_NULL arr 为 NULL
 * @retval BUBBLE_ERR_SIZE n 无效
 *
 * @see bubble_sort_uint16_asc
 */
bubble_status_t bubble_sort_uint16_optimized(uint16_t arr[], uint16_t n);

/* ========================================================================== */
/*  uint32_t 类型排序函数                                                      */
/* ========================================================================== */

/**
 * @brief 冒泡排序 — uint32_t 升序
 *
 * @param[out] arr  待排序数组指针
 * @param[in]  n    数组元素个数，必须 > 0
 *
 * @retval BUBBLE_OK       排序成功
 * @retval BUBBLE_ERR_NULL arr 为 NULL
 * @retval BUBBLE_ERR_SIZE n <= 0
 *
 * @see bubble_sort_uint32_optimized
 */
bubble_status_t bubble_sort_uint32_asc(uint32_t arr[], int n);

/**
 * @brief 冒泡排序 — uint32_t 升序（带提前终止优化）
 *
 * @param[out] arr  待排序数组指针
 * @param[in]  n    数组元素个数，必须 > 0
 *
 * @retval BUBBLE_OK       排序成功
 * @retval BUBBLE_ERR_NULL arr 为 NULL
 * @retval BUBBLE_ERR_SIZE n <= 0
 *
 * @see bubble_sort_uint32_asc
 */
bubble_status_t bubble_sort_uint32_optimized(uint32_t arr[], int n);

#endif /* BUBBLE_H */
