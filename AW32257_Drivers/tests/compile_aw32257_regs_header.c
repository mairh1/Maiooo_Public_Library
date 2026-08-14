/* SPDX-License-Identifier: WTFPL */

#include "aw32257_regs.h"

uint8_t aw32257_regs_header_is_self_contained(void)
{
    return AW32257_I2C_ADDRESS_7BIT;
}
