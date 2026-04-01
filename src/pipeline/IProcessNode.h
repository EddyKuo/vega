#pragma once
#include <string_view>
#include <cstdint>

namespace vega {

// Forward declare
struct EditRecipe;

enum class PipelineStage : uint32_t {
    Demosaic       = 1 << 0,
    WhiteBalance   = 1 << 1,
    Exposure       = 1 << 2,
    ToneCurve      = 1 << 3,
    HSL            = 1 << 4,
    Vibrance       = 1 << 5,
    Sharpen        = 1 << 6,
    Denoise        = 1 << 7,
    LensCorrection = 1 << 8,
    ColorSpace     = 1 << 9,
    Crop           = 1 << 10,
    All            = 0xFFFFFFFF
};

inline PipelineStage operator|(PipelineStage a, PipelineStage b) {
    return static_cast<PipelineStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(PipelineStage a, PipelineStage b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct Tile {
    float* data;
    uint32_t x, y, width, height;
    uint32_t overlap;
    uint32_t stride;   // in floats per row (width + 2*overlap) * channels
    uint32_t channels; // 3 for RGB
};

class IProcessNode {
public:
    virtual ~IProcessNode() = default;
    virtual std::string_view name() const = 0;
    virtual PipelineStage stage() const = 0;
    virtual void process(Tile& tile, const EditRecipe& recipe) = 0;
    virtual uint32_t requiredOverlap() const { return 0; }
};

} // namespace vega
