/**
 * @file    bubble.h
 * @brief   冒泡排序算法模块头文件
 * @author  Maiooo
 * @version 2.0.0
 * @date    2026-05-07
 *
 * @details
 * 提供多种数据类型的冒泡排序实现，支持：
 * - 基础冒泡排序（升序 / 降序）
 * - 带提前终止优化的冒泡排序（升序 / 降序）
 * - 支持 int、uint16_t、uint32_t 类型
 *
 * 与 v1.1 的区别：
 * - 修复 n == 1 时错误返回 BUBBLE_ERR_SIZE 的 bug
 * - 补齐所有类型的降序 / 优化降序变体
 * - 保留原始函数签名，ABI 完全兼容
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

typedef enum {
    BUBBLE_OK       =  0,
    BUBBLE_ERR_NULL = -1,
    BUBBLE_ERR_SIZE = -2
} bubble_status_t;

/* ========================================================================== */
/*  int 类型排序函数                                                           */
/* ========================================================================== */

bubble_status_t bubble_sort_int_asc(int arr[], int n);
bubble_status_t bubble_sort_int_desc(int arr[], int n);
bubble_status_t bubble_sort_int_optimized(int arr[], int n);
bubble_status_t bubble_sort_int_optimized_desc(int arr[], int n);

/* ========================================================================== */
/*  uint16_t 类型排序函数（n 保持 uint16_t，与 v1.1 签名兼容）                 */
/* ========================================================================== */

bubble_status_t bubble_sort_uint16_asc(uint16_t arr[], uint16_t n);
bubble_status_t bubble_sort_uint16_desc(uint16_t arr[], uint16_t n);
bubble_status_t bubble_sort_uint16_optimized(uint16_t arr[], uint16_t n);
bubble_status_t bubble_sort_uint16_optimized_desc(uint16_t arr[], uint16_t n);

/* ========================================================================== */
/*  uint32_t 类型排序函数（n 保持 int，与 v1.1 签名兼容）                       */
/* ========================================================================== */

bubble_status_t bubble_sort_uint32_asc(uint32_t arr[], int n);
bubble_status_t bubble_sort_uint32_desc(uint32_t arr[], int n);
bubble_status_t bubble_sort_uint32_optimized(uint32_t arr[], int n);
bubble_status_t bubble_sort_uint32_optimized_desc(uint32_t arr[], int n);

#endif /* BUBBLE_H */
