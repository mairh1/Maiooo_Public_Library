/**
 * @file    compile_aw32257_header.c
 * @brief   编译期自检：aw32257.h 必须自包含
 * @details 单独包含公共头并使用其中类型，验证 aw32257.h 不依赖
 *          调用方预先包含其它头文件。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "aw32257.h"

int aw32257_public_header_is_self_contained(void)
{
    aw32257_t device;

    device.lifecycle = AW32257_LIFECYCLE_UNBOUND;
    return (int)device.lifecycle;
}
