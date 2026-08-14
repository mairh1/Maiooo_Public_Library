/* SPDX-License-Identifier: WTFPL */

#include "aw32257.h"

int aw32257_public_header_is_self_contained(void)
{
    aw32257_t device;

    device.lifecycle = AW32257_LIFECYCLE_UNBOUND;
    return (int)device.lifecycle;
}
