#pragma once
#include <cstdint>

namespace vega {

struct AutoToneResult {
    float exposure = 0, contrast = 0;
    float highlights = 0, shadows = 0, whites = 0, blacks = 0;
};

AutoToneResult computeAutoTone(const uint8_t* rgba, uint32_t width, uint32_t height);

} // namespace vega
