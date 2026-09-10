/**
 * @file    compile_aw32257_regs_header.c
 * @brief   编译期自检：aw32257_regs.h 必须自包含
 * @details 单独包含寄存器头并引用其中常量，验证 aw32257_regs.h 不
 *          依赖调用方预先包含其它头文件。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-08-13
 *
 * SPDX-License-Identifier: WTFPL
 */

#include "aw32257_regs.h"

uint8_t aw32257_regs_header_is_self_contained(void)
{
    return AW32257_I2C_ADDRESS_7BIT;
}
