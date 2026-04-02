#pragma once
#include "pipeline/IProcessNode.h"

namespace vega {

class SharpenNode : public IProcessNode {
public:
    std::string_view name() const override { return "Sharpen"; }
    PipelineStage stage() const override { return PipelineStage::Sharpen; }
    void process(Tile& tile, const EditRecipe& recipe) override;
};

} // namespace vega
