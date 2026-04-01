#pragma once
#include "pipeline/IProcessNode.h"

namespace vega {

class ColorSpaceNode : public IProcessNode {
public:
    std::string_view name() const override { return "ColorSpace"; }
    PipelineStage stage() const override { return PipelineStage::ColorSpace; }
    void process(Tile& tile, const EditRecipe& recipe) override;

private:
    // sRGB gamma transfer function: linear -> sRGB
    static float linearToSRGB(float x);
};

} // namespace vega
