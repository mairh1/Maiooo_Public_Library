#include "wm8978.h"

int wm8978_header_probe(void)
{
    wm8978_t codec;

    codec.lifecycle = WM8978_LIFECYCLE_UNBOUND;
    return (int)codec.lifecycle;
}
