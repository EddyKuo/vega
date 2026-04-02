#pragma once
#include "pipeline/IProcessNode.h"

namespace vega {

class DenoiseNode : public IProcessNode {
public:
    std::string_view name() const override { return "Denoise"; }
    PipelineStage stage() const override { return PipelineStage::Denoise; }
    void process(Tile& tile, const EditRecipe& recipe) override;
};

} // namespace vega
