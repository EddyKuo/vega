#pragma once
#include <imgui.h>
#include <array>
#include <cstdint>
#include <vector>

namespace vega {

class HistogramView {
public:
    // Compute histogram from RGBA8 buffer
    void compute(const uint8_t* rgba_data, uint32_t width, uint32_t height);

    // Compute histogram from float RGB buffer (linear, pre-gamma)
    void computeFromFloat(const float* rgb_data, uint32_t width, uint32_t height);

    // Render the histogram in the current ImGui window
    void render();

    // Toggle which channels to show
    bool show_r = true, show_g = true, show_b = true, show_luma = true;
    bool show_clipping = true;

private:
    static constexpr int NUM_BINS = 256;
    std::array<uint32_t, NUM_BINS> hist_r_ = {};
    std::array<uint32_t, NUM_BINS> hist_g_ = {};
    std::array<uint32_t, NUM_BINS> hist_b_ = {};
    std::array<uint32_t, NUM_BINS> hist_luma_ = {};
    uint32_t max_count_ = 1;  // for normalization
    uint32_t clip_shadow_ = 0;   // pixels at 0
    uint32_t clip_highlight_ = 0; // pixels at 255
    uint32_t total_pixels_ = 0;
};

} // namespace vega
