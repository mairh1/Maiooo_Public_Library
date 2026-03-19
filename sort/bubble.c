/**
 * @file    bubble.c
 * @brief   冒泡排序算法模块实现
 * @author  Maiooo
 * @version 1.1.0
 * @date    2026-03-19
 *
 * @details
 * 实现多种数据类型的冒泡排序，包括基础版本和带提前终止优化的版本。
 * 所有公开函数均进行参数校验，排序结果直接写入原数组。
 *
 * @ingroup algo_sort
 */

#include "bubble.h"

/* ========================================================================== */
/*  私有宏定义                                                                 */
/* ========================================================================== */

/**
 * @brief 交换两个相同类型元素的值
 *
 * @param a  元素 A
 * @param b  元素 B
 * @param t  与元素同类型的临时变量
 */
#define BUBBLE_SWAP(a, b, t)  do { (t) = (a); (a) = (b); (b) = (t); } while (0)

/* ========================================================================== */
/*  私有函数 — 基础排序核心                                                    */
/* ========================================================================== */

/**
 * @brief 基础冒泡排序核心逻辑（升序，int 类型）
 *
 * 执行标准的双层循环冒泡排序，不做提前终止判断。
 *
 * @param[out] arr  待排序数组
 * @param[in]  n    元素个数
 */
static void bubble_core_int_asc(int arr[], int n)
{
    int i, j, tmp;

    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);
            }
        }
    }
}

/**
 * @brief 基础冒泡排序核心逻辑（降序，int 类型）
 *
 * @param[out] arr  待排序数组
 * @param[in]  n    元素个数
 */
static void bubble_core_int_desc(int arr[], int n)
{
    int i, j, tmp;

    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] < arr[j + 1])
            {
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);
            }
        }
    }
}

/**
 * @brief 带提前终止优化的冒泡排序核心逻辑（升序，int 类型）
 *
 * @param[out] arr  待排序数组
 * @param[in]  n    元素个数
 */
static void bubble_core_int_optimized(int arr[], int n)
{
    int i, j, tmp;
    bool swapped;

    for (i = 0; i < n - 1; i++)
    {
        swapped = false;

        for (j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);
                swapped = true;
            }
        }

        if (!swapped)
        {
            break;  /* 本轮无交换，数组已有序 */
        }
    }
}

/**
 * @brief 基础冒泡排序核心逻辑（升序，uint16_t 类型）
 *
 * @param[out] arr  待排序数组
 * @param[in]  n    元素个数
 */
static void bubble_core_uint16_asc(uint16_t arr[], uint16_t n)
{
    uint16_t i, j, tmp;

    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);
            }
        }
    }
}

/**
 * @brief 带提前终止优化的冒泡排序核心逻辑（升序，uint16_t 类型）
 *
 * @param[out] arr  待排序数组
 * @param[in]  n    元素个数
 */
static void bubble_core_uint16_optimized(uint16_t arr[], uint16_t n)
{
    uint16_t i, j, tmp;
    bool swapped;

    for (i = 0; i < n - 1; i++)
    {
        swapped = false;

        for (j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);
                swapped = true;
            }
        }

        if (!swapped)
        {
            break;
        }
    }
}

/**
 * @brief 基础冒泡排序核心逻辑（升序，uint32_t 类型）
 *
 * @param[out] arr  待排序数组
 * @param[in]  n    元素个数
 */
static void bubble_core_uint32_asc(uint32_t arr[], int n)
{
    int i, j;
    uint32_t tmp;

    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);
            }
        }
    }
}

/**
 * @brief 带提前终止优化的冒泡排序核心逻辑（升序，uint32_t 类型）
 *
 * @param[out] arr  待排序数组
 * @param[in]  n    元素个数
 */
static void bubble_core_uint32_optimized(uint32_t arr[], int n)
{
    int i, j;
    uint32_t tmp;
    bool swapped;

    for (i = 0; i < n - 1; i++)
    {
        swapped = false;

        for (j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                BUBBLE_SWAP(arr[j], arr[j + 1], tmp);
                swapped = true;
            }
        }

        if (!swapped)
        {
            break;
        }
    }
}

/* ========================================================================== */
/*  公开接口函数                                                                */
/* ========================================================================== */

/* ---- int 类型 ----------------------------------------------------------- */

bubble_status_t bubble_sort_int_asc(int arr[], int n)
{
    if (arr == NULL) return BUBBLE_ERR_NULL;
    if (n <= 1)      return BUBBLE_ERR_SIZE;

    bubble_core_int_asc(arr, n);
    return BUBBLE_OK;
}

bubble_status_t bubble_sort_int_desc(int arr[], int n)
{
    if (arr == NULL) return BUBBLE_ERR_NULL;
    if (n <= 1)      return BUBBLE_ERR_SIZE;

    bubble_core_int_desc(arr, n);
    return BUBBLE_OK;
}

bubble_status_t bubble_sort_int_optimized(int arr[], int n)
{
    if (arr == NULL) return BUBBLE_ERR_NULL;
    if (n <= 1)      return BUBBLE_ERR_SIZE;

    bubble_core_int_optimized(arr, n);
    return BUBBLE_OK;
}

/* ---- uint16_t 类型 ------------------------------------------------------- */

bubble_status_t bubble_sort_uint16_asc(uint16_t arr[], uint16_t n)
{
    if (arr == NULL) return BUBBLE_ERR_NULL;
    if (n <= 1)      return BUBBLE_ERR_SIZE;

    bubble_core_uint16_asc(arr, n);
    return BUBBLE_OK;
}

bubble_status_t bubble_sort_uint16_optimized(uint16_t arr[], uint16_t n)
{
    if (arr == NULL) return BUBBLE_ERR_NULL;
    if (n <= 1)      return BUBBLE_ERR_SIZE;

    bubble_core_uint16_optimized(arr, n);
    return BUBBLE_OK;
}

/* ---- uint32_t 类型 ------------------------------------------------------- */

bubble_status_t bubble_sort_uint32_asc(uint32_t arr[], int n)
{
    if (arr == NULL) return BUBBLE_ERR_NULL;
    if (n <= 1)      return BUBBLE_ERR_SIZE;

    bubble_core_uint32_asc(arr, n);
    return BUBBLE_OK;
}

bubble_status_t bubble_sort_uint32_optimized(uint32_t arr[], int n)
{
    if (arr == NULL) return BUBBLE_ERR_NULL;
    if (n <= 1)      return BUBBLE_ERR_SIZE;

    bubble_core_uint32_optimized(arr, n);
    return BUBBLE_OK;
}
