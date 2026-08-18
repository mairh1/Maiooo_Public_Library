/**
 * @file    bubble.c
 * @brief   冒泡排序算法模块实现
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-05-07
 *
 * @details
 * 实现多种数据类型的冒泡排序，包括基础版本和带提前终止优化的版本。
 * 使用宏消除重复代码，所有公开函数均进行参数校验，排序结果直接写入原数组。
 *
 * 与 v1.1 的区别：
 * - 修复 n == 1 时错误返回 BUBBLE_ERR_SIZE 的 bug（单元素数组是合法输入）
 * - 补齐所有类型的 4 种变体（升序、降序、优化升序、优化降序）
 * - 保留原始函数签名，ABI 完全兼容
 *
 * @ingroup algo_sort
 */

#include "bubble.h"

/* ========================================================================== */
/*  私有宏定义                                                                 */
/* ========================================================================== */

#define BUBBLE_SWAP(a, b, t)  do { (t) = (a); (a) = (b); (b) = (t); } while (0)

#define BUBBLE_PARAM_CHECK(arr, n) \
    do { if ((arr) == NULL) return BUBBLE_ERR_NULL; \
         if ((n) < 1)       return BUBBLE_ERR_SIZE; } while (0)

/* ========================================================================== */
/*  核心排序逻辑宏（生成 static 内部函数）                                      */
/*  核心函数统一使用 int n，公开 API 层负责类型转换                              */
/* ========================================================================== */

#define BUBBLE_CORE_ASC(prefix, type)                                          \
static void bubble_core_##prefix##_asc(type arr[], int n)                      \
{                                                                              \
    int i, j;                                                                  \
    type tmp;                                                                  \
    for (i = 0; i < n - 1; i++)                                                \
        for (j = 0; j < n - 1 - i; j++)                                       \
            if (arr[j] > arr[j + 1])                                           \
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);                          \
}

#define BUBBLE_CORE_DESC(prefix, type)                                         \
static void bubble_core_##prefix##_desc(type arr[], int n)                     \
{                                                                              \
    int i, j;                                                                  \
    type tmp;                                                                  \
    for (i = 0; i < n - 1; i++)                                                \
        for (j = 0; j < n - 1 - i; j++)                                       \
            if (arr[j] < arr[j + 1])                                           \
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);                          \
}

#define BUBBLE_CORE_OPT_ASC(prefix, type)                                      \
static void bubble_core_##prefix##_opt_asc(type arr[], int n)                  \
{                                                                              \
    int i, j;                                                                  \
    type tmp;                                                                  \
    bool swapped;                                                              \
    for (i = 0; i < n - 1; i++) {                                              \
        swapped = false;                                                       \
        for (j = 0; j < n - 1 - i; j++)                                       \
            if (arr[j] > arr[j + 1]) {                                         \
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);                          \
                swapped = true;                                                \
            }                                                                  \
        if (!swapped) break;                                                   \
    }                                                                          \
}

#define BUBBLE_CORE_OPT_DESC(prefix, type)                                     \
static void bubble_core_##prefix##_opt_desc(type arr[], int n)                 \
{                                                                              \
    int i, j;                                                                  \
    type tmp;                                                                  \
    bool swapped;                                                              \
    for (i = 0; i < n - 1; i++) {                                              \
        swapped = false;                                                       \
        for (j = 0; j < n - 1 - i; j++)                                       \
            if (arr[j] < arr[j + 1]) {                                         \
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);                          \
                swapped = true;                                                \
            }                                                                  \
        if (!swapped) break;                                                   \
    }                                                                          \
}

/* ========================================================================== */
/*  公开接口宏（参数校验 + 类型转换 + 调用核心函数）                              */
/*  n_type 保留原始签名，核心函数统一 int                                        */
/* ========================================================================== */

#define BUBBLE_API_ASC(prefix, type, n_type)                                   \
bubble_status_t bubble_sort_##prefix##_asc(type arr[], n_type n)               \
{                                                                              \
    BUBBLE_PARAM_CHECK(arr, n);                                                \
    bubble_core_##prefix##_asc(arr, (int)n);                                   \
    return BUBBLE_OK;                                                          \
}

#define BUBBLE_API_DESC(prefix, type, n_type)                                  \
bubble_status_t bubble_sort_##prefix##_desc(type arr[], n_type n)              \
{                                                                              \
    BUBBLE_PARAM_CHECK(arr, n);                                                \
    bubble_core_##prefix##_desc(arr, (int)n);                                  \
    return BUBBLE_OK;                                                          \
}

#define BUBBLE_API_OPT_ASC(prefix, type, n_type)                               \
bubble_status_t bubble_sort_##prefix##_optimized(type arr[], n_type n)         \
{                                                                              \
    BUBBLE_PARAM_CHECK(arr, n);                                                \
    bubble_core_##prefix##_opt_asc(arr, (int)n);                               \
    return BUBBLE_OK;                                                          \
}

#define BUBBLE_API_OPT_DESC(prefix, type, n_type)                              \
bubble_status_t bubble_sort_##prefix##_optimized_desc(type arr[], n_type n)    \
{                                                                              \
    BUBBLE_PARAM_CHECK(arr, n);                                                \
    bubble_core_##prefix##_opt_desc(arr, (int)n);                              \
    return BUBBLE_OK;                                                          \
}

/* ========================================================================== */
/*  实例化：int 类型（n_type = int）                                            */
/* ========================================================================== */

BUBBLE_CORE_ASC(int, int)
BUBBLE_CORE_DESC(int, int)
BUBBLE_CORE_OPT_ASC(int, int)
BUBBLE_CORE_OPT_DESC(int, int)

BUBBLE_API_ASC(int, int, int)
BUBBLE_API_DESC(int, int, int)
BUBBLE_API_OPT_ASC(int, int, int)
BUBBLE_API_OPT_DESC(int, int, int)

/* ========================================================================== */
/*  实例化：uint16_t 类型（n_type = uint16_t，保留原始签名）                     */
/* ========================================================================== */

BUBBLE_CORE_ASC(uint16, uint16_t)
BUBBLE_CORE_DESC(uint16, uint16_t)
BUBBLE_CORE_OPT_ASC(uint16, uint16_t)
BUBBLE_CORE_OPT_DESC(uint16, uint16_t)

BUBBLE_API_ASC(uint16, uint16_t, uint16_t)
BUBBLE_API_DESC(uint16, uint16_t, uint16_t)
BUBBLE_API_OPT_ASC(uint16, uint16_t, uint16_t)
BUBBLE_API_OPT_DESC(uint16, uint16_t, uint16_t)

/* ========================================================================== */
/*  实例化：uint32_t 类型（n_type = int，保留原始签名）                          */
/* ========================================================================== */

BUBBLE_CORE_ASC(uint32, uint32_t)
BUBBLE_CORE_DESC(uint32, uint32_t)
BUBBLE_CORE_OPT_ASC(uint32, uint32_t)
BUBBLE_CORE_OPT_DESC(uint32, uint32_t)

BUBBLE_API_ASC(uint32, uint32_t, int)
BUBBLE_API_DESC(uint32, uint32_t, int)
BUBBLE_API_OPT_ASC(uint32, uint32_t, int)
BUBBLE_API_OPT_DESC(uint32, uint32_t, int)
