#include "wm8978.h"

int wm8978_multi_tu_b(void)
{
    uint8_t frame[2];

    return (int)wm8978_pack_control_frame(WM8978_REG_AUDIO_INTERFACE,
                                           WM8978_R04_RESET_VALUE,
                                           frame);
}
