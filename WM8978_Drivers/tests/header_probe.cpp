#include "wm8978.h"

int wm8978_cpp_header_probe()
{
    wm8978_audio_interface_config_t config = {};
    config.format = WM8978_AUDIO_FORMAT_I2S;
    return static_cast<int>(config.format);
}
