#pragma once
#include "raw/RawImage.h"
#include "pipeline/EditRecipe.h"
#include "pipeline/IProcessNode.h"
#include <vector>
#include <memory>

namespace vega {

class Pipeline {
public:
    Pipeline();

    // Process a RawImage with the given recipe, output RGBA8 buffer
    std::vector<uint8_t> process(const RawImage& raw, const EditRecipe& recipe);

    // Determine which stage was first affected by recipe change
    PipelineStage firstDirtyStage(const EditRecipe& old_recipe, const EditRecipe& new_recipe) const;

private:
    std::vector<std::unique_ptr<IProcessNode>> nodes_;

    // Stage cache: stores intermediate RGB float buffer after each stage
    struct StageCache {
        PipelineStage stage;
        EditRecipe recipe_snapshot;
        std::vector<float> data; // RGB float, width*height*3
        uint32_t width;
        uint32_t height;
    };
    std::vector<StageCache> cache_;
    static constexpr size_t MAX_CACHE_ENTRIES = 3;

    void buildNodeChain();

    // Demosaic the raw Bayer data to linear RGB float buffer.
    static void demosaicAndTransform(const RawImage& raw, std::vector<float>& rgb_out);

    // Cached demosaic result (only recomputed when image changes)
    std::vector<float> demosaic_cache_;
    std::vector<float> work_buffer_;  // reusable work buffer (avoids realloc)
    uint32_t demosaic_w_ = 0, demosaic_h_ = 0;
    const void* demosaic_src_ = nullptr;  // pointer to detect image change

    // Convert float RGB [0,1] buffer to RGBA8
    static void toRGBA8(const float* rgb, uint8_t* rgba, uint32_t pixel_count);

    // Find a cached buffer valid for the given recipe, at the latest possible stage
    // Returns the index of the node to resume from (0 = start from scratch)
    size_t findCacheHit(const EditRecipe& recipe) const;

    // Store a cache entry after a given stage
    void storeCache(PipelineStage stage, const EditRecipe& recipe,
                    const std::vector<float>& data, uint32_t width, uint32_t height);
};

} // namespace vega
