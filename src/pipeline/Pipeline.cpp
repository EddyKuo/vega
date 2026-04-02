#include "pipeline/Pipeline.h"
#include "pipeline/cpu/WhiteBalanceNode.h"
#include "pipeline/cpu/ExposureNode.h"
#include "pipeline/cpu/ToneCurveNode.h"
#include "pipeline/cpu/HSLNode.h"
#include "pipeline/cpu/ColorSpaceNode.h"
#include "core/Logger.h"
#include "core/Timer.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace vega {

Pipeline::Pipeline()
{
    // Don't call buildNodeChain() here — this may be constructed as a static
    // global before Logger::init(). Nodes are built lazily on first process().
}

void Pipeline::buildNodeChain()
{
    nodes_.clear();
    nodes_.push_back(std::make_unique<WhiteBalanceNode>());
    nodes_.push_back(std::make_unique<ExposureNode>());
    nodes_.push_back(std::make_unique<ToneCurveNode>());
    nodes_.push_back(std::make_unique<HSLNode>());
    nodes_.push_back(std::make_unique<ColorSpaceNode>());

    VEGA_LOG_INFO("Pipeline: built node chain with {} nodes", nodes_.size());
    for (size_t i = 0; i < nodes_.size(); ++i) {
        VEGA_LOG_DEBUG("  [{}] {}", i, nodes_[i]->name());
    }
}

void Pipeline::demosaicAndTransform(const RawImage& raw, std::vector<float>& rgb_out)
{
    // We reuse SimplePipeline's static process approach but stop before gamma.
    // SimplePipeline::demosaic is private, so we replicate the Bayer->RGB logic here
    // by calling SimplePipeline::process and... no, that applies gamma.
    //
    // Instead, we do bilinear demosaic inline. The demosaic code is straightforward
    // and matches SimplePipeline's implementation.

    uint32_t width = raw.width;
    uint32_t height = raw.height;
    uint32_t pixel_count = width * height;

    rgb_out.resize(static_cast<size_t>(pixel_count) * 3);

    // Bayer channel lookup
    // Pattern encodes which color filter is at (row%2, col%2):
    //   RGGB = 0, BGGR = 1, GRBG = 2, GBRG = 3
    static constexpr int bayerTable[4][4] = {
        {0, 1, 1, 2},  // RGGB
        {2, 1, 1, 0},  // BGGR
        {1, 0, 2, 1},  // GRBG
        {1, 2, 0, 1},  // GBRG
    };

    auto bayerChannel = [&](uint32_t pattern, int row, int col) -> int {
        uint32_t p = pattern < 4 ? pattern : 0;
        return bayerTable[p][(row & 1) * 2 + (col & 1)];
    };

    auto clampCoord = [](int v, int maxVal) -> int {
        return v < 0 ? 0 : (v >= maxVal ? maxVal - 1 : v);
    };

    const float* bayer = raw.bayer_data.data();
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);
    uint32_t pattern = raw.metadata.bayer_pattern;

    auto sample = [&](int r, int c) -> float {
        r = clampCoord(r, h);
        c = clampCoord(c, w);
        return bayer[r * w + c];
    };

    // Bilinear demosaic
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            int ch = bayerChannel(pattern, r, c);
            float center = sample(r, c);
            float R, G, B;

            if (ch == 0) {
                R = center;
                G = (sample(r - 1, c) + sample(r + 1, c) +
                     sample(r, c - 1) + sample(r, c + 1)) * 0.25f;
                B = (sample(r - 1, c - 1) + sample(r - 1, c + 1) +
                     sample(r + 1, c - 1) + sample(r + 1, c + 1)) * 0.25f;
            } else if (ch == 2) {
                B = center;
                G = (sample(r - 1, c) + sample(r + 1, c) +
                     sample(r, c - 1) + sample(r, c + 1)) * 0.25f;
                R = (sample(r - 1, c - 1) + sample(r - 1, c + 1) +
                     sample(r + 1, c - 1) + sample(r + 1, c + 1)) * 0.25f;
            } else {
                G = center;
                int chUp   = bayerChannel(pattern, r - 1, c);
                int chLeft = bayerChannel(pattern, r, c - 1);

                if (chUp == 0) {
                    R = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                    B = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                } else if (chUp == 2) {
                    B = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                    R = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                } else if (chLeft == 0) {
                    R = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                    B = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                } else {
                    B = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                    R = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                }
            }

            size_t idx = (static_cast<size_t>(r) * width + c) * 3;
            rgb_out[idx + 0] = R;
            rgb_out[idx + 1] = G;
            rgb_out[idx + 2] = B;
        }
    }

    // Apply camera white balance (from RAW metadata, not the user's recipe WB)
    {
        float g_norm = (raw.wb_multipliers[1] + raw.wb_multipliers[3]) * 0.5f;
        if (g_norm <= 0.0f) g_norm = 1.0f;
        float r_scale = raw.wb_multipliers[0] / g_norm;
        float b_scale = raw.wb_multipliers[2] / g_norm;

        for (uint32_t i = 0; i < pixel_count; ++i) {
            rgb_out[i * 3 + 0] *= r_scale;
            // green stays at 1.0
            rgb_out[i * 3 + 2] *= b_scale;
        }
    }

    // Apply camera color matrix (camera RGB -> linear sRGB)
    // color_matrix stores LibRaw's rgb_cam[3][3] which converts camera
    // color space directly to sRGB. No intermediate XYZ step needed.
    {
        const float* M = raw.color_matrix;
        bool has_matrix = false;
        for (int i = 0; i < 9; ++i) {
            if (M[i] != 0.0f) { has_matrix = true; break; }
        }

        if (has_matrix) {
            VEGA_LOG_DEBUG("Color matrix (rgb_cam): [{:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f}]",
                M[0], M[1], M[2], M[3], M[4], M[5], M[6], M[7], M[8]);

            for (uint32_t i = 0; i < pixel_count; ++i) {
                float* p = rgb_out.data() + i * 3;
                float in_r = p[0], in_g = p[1], in_b = p[2];
                p[0] = M[0] * in_r + M[1] * in_g + M[2] * in_b;
                p[1] = M[3] * in_r + M[4] * in_g + M[5] * in_b;
                p[2] = M[6] * in_r + M[7] * in_g + M[8] * in_b;
            }
        }
        else {
            VEGA_LOG_WARN("No camera color matrix, skipping transform");
        }
    }
}

void Pipeline::toRGBA8(const float* rgb, uint8_t* rgba, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; ++i) {
        float r = std::clamp(rgb[i * 3 + 0], 0.0f, 1.0f);
        float g = std::clamp(rgb[i * 3 + 1], 0.0f, 1.0f);
        float b = std::clamp(rgb[i * 3 + 2], 0.0f, 1.0f);

        // At this point, data has already gone through ColorSpaceNode (sRGB gamma).
        // Just quantize to 8-bit.
        rgba[i * 4 + 0] = static_cast<uint8_t>(r * 255.0f + 0.5f);
        rgba[i * 4 + 1] = static_cast<uint8_t>(g * 255.0f + 0.5f);
        rgba[i * 4 + 2] = static_cast<uint8_t>(b * 255.0f + 0.5f);
        rgba[i * 4 + 3] = 255;
    }
}

size_t Pipeline::findCacheHit(const EditRecipe& recipe) const
{
    // Walk the node chain in reverse. For each node, check if we have a cached
    // buffer whose recipe matches on all fields that affect that stage and all
    // prior stages.
    // A cache hit at node index N means we can skip nodes 0..N and resume at N+1.

    for (size_t ci = 0; ci < cache_.size(); ++ci) {
        const auto& entry = cache_[ci];

        // Find which node index this cache corresponds to
        for (size_t ni = 0; ni < nodes_.size(); ++ni) {
            if (nodes_[ni]->stage() & entry.stage) {
                // Check if recipe fields for this stage and all prior stages match
                PipelineStage dirty = firstDirtyStage(entry.recipe_snapshot, recipe);

                // If the first dirty stage is after this cached stage, we can reuse
                bool canReuse = true;
                for (size_t pi = 0; pi <= ni; ++pi) {
                    if (dirty & nodes_[pi]->stage()) {
                        canReuse = false;
                        break;
                    }
                }

                if (canReuse) {
                    VEGA_LOG_DEBUG("Pipeline: cache hit at node [{}] '{}'",
                                  ni, nodes_[ni]->name());
                    return ni + 1; // resume from the next node
                }
            }
        }
    }

    return 0; // no cache hit, start from scratch
}

void Pipeline::storeCache(PipelineStage stage, const EditRecipe& recipe,
                          const std::vector<float>& data, uint32_t width, uint32_t height)
{
    // Check if we already have a cache for this stage
    for (auto& entry : cache_) {
        if (entry.stage == stage) {
            entry.recipe_snapshot = recipe;
            entry.data = data;
            entry.width = width;
            entry.height = height;
            return;
        }
    }

    // If cache is full, evict the oldest entry
    if (cache_.size() >= MAX_CACHE_ENTRIES) {
        cache_.erase(cache_.begin());
    }

    cache_.push_back({stage, recipe, data, width, height});
}

PipelineStage Pipeline::firstDirtyStage(const EditRecipe& old_r, const EditRecipe& new_r) const
{
    // Compare recipe fields and return the first stage that needs re-processing.

    // White balance
    if (old_r.wb_temperature != new_r.wb_temperature ||
        old_r.wb_tint != new_r.wb_tint) {
        return PipelineStage::WhiteBalance;
    }

    // Exposure
    if (old_r.exposure != new_r.exposure ||
        old_r.contrast != new_r.contrast ||
        old_r.highlights != new_r.highlights ||
        old_r.shadows != new_r.shadows ||
        old_r.whites != new_r.whites ||
        old_r.blacks != new_r.blacks) {
        return PipelineStage::Exposure;
    }

    // Tone curves
    if (old_r.tone_curve_rgb != new_r.tone_curve_rgb ||
        old_r.tone_curve_r != new_r.tone_curve_r ||
        old_r.tone_curve_g != new_r.tone_curve_g ||
        old_r.tone_curve_b != new_r.tone_curve_b) {
        return PipelineStage::ToneCurve;
    }

    // HSL
    if (old_r.hsl_hue != new_r.hsl_hue ||
        old_r.hsl_saturation != new_r.hsl_saturation ||
        old_r.hsl_luminance != new_r.hsl_luminance ||
        old_r.vibrance != new_r.vibrance ||
        old_r.saturation != new_r.saturation) {
        return PipelineStage::HSL;
    }

    // Sharpening
    if (old_r.sharpen_amount != new_r.sharpen_amount ||
        old_r.sharpen_radius != new_r.sharpen_radius ||
        old_r.sharpen_detail != new_r.sharpen_detail ||
        old_r.sharpen_masking != new_r.sharpen_masking) {
        return PipelineStage::Sharpen;
    }

    // Denoise
    if (old_r.denoise_luminance != new_r.denoise_luminance ||
        old_r.denoise_color != new_r.denoise_color ||
        old_r.denoise_detail != new_r.denoise_detail) {
        return PipelineStage::Denoise;
    }

    // Color space
    if (old_r.output_colorspace != new_r.output_colorspace) {
        return PipelineStage::ColorSpace;
    }

    // Crop
    if (old_r.crop_left != new_r.crop_left ||
        old_r.crop_top != new_r.crop_top ||
        old_r.crop_right != new_r.crop_right ||
        old_r.crop_bottom != new_r.crop_bottom ||
        old_r.rotation != new_r.rotation) {
        return PipelineStage::Crop;
    }

    // No difference
    return PipelineStage::All;
}

std::vector<uint8_t> Pipeline::process(const RawImage& raw, const EditRecipe& recipe)
{
    if (raw.bayer_data.empty() || raw.width == 0 || raw.height == 0)
        return {};

    // Lazy init: build node chain on first use (avoids static init order issues)
    if (nodes_.empty())
        buildNodeChain();

    Timer totalTimer;
    uint32_t width = raw.width;
    uint32_t height = raw.height;
    uint32_t pixel_count = width * height;

    // Try to find a cache hit to skip early stages
    size_t startNode = findCacheHit(recipe);

    std::vector<float> rgb;

    if (startNode > 0 && startNode <= nodes_.size()) {
        // Find the matching cache entry
        PipelineStage cachedStage = nodes_[startNode - 1]->stage();
        for (const auto& entry : cache_) {
            if (entry.stage == cachedStage && entry.width == width && entry.height == height) {
                rgb = entry.data;
                VEGA_LOG_INFO("Pipeline: resuming from cache after node [{}] '{}'",
                              startNode - 1, nodes_[startNode - 1]->name());
                break;
            }
        }
        if (rgb.empty()) {
            // Cache entry wasn't found (shouldn't happen), fall back
            startNode = 0;
        }
    }

    if (startNode == 0) {
        // Step 1: Demosaic and color transform (Bayer -> linear sRGB)
        // Cache the result — only recompute if the image pointer changes
        const void* src_ptr = raw.bayer_data.data();
        if (src_ptr != demosaic_src_ || demosaic_w_ != width || demosaic_h_ != height) {
            Timer demosaicTimer;
            demosaicAndTransform(raw, demosaic_cache_);
            demosaic_src_ = src_ptr;
            demosaic_w_ = width;
            demosaic_h_ = height;
            VEGA_LOG_INFO("Pipeline: demosaic + color transform: {:.1f}ms",
                          demosaicTimer.elapsed_ms());
        } else {
            VEGA_LOG_DEBUG("Pipeline: using cached demosaic result");
        }
        rgb = demosaic_cache_;  // copy from cache (nodes will modify in-place)
    }

    // Step 2: Run processing nodes
    // Create a single tile covering the full image (no tiling for now)
    Tile tile{};
    tile.data = rgb.data();
    tile.x = 0;
    tile.y = 0;
    tile.width = width;
    tile.height = height;
    tile.overlap = 0;
    tile.stride = width * 3; // 3 channels, tightly packed
    tile.channels = 3;

    for (size_t i = startNode; i < nodes_.size(); ++i) {
        Timer nodeTimer;
        nodes_[i]->process(tile, recipe);
        double nodeMs = nodeTimer.elapsed_ms();

        VEGA_LOG_INFO("Pipeline: node [{}] '{}': {:.1f}ms",
                      i, nodes_[i]->name(), nodeMs);

        // Cache the result after key stages (WhiteBalance, Exposure, HSL)
        // These are the stages most likely to be adjusted interactively
        PipelineStage nodeStage = nodes_[i]->stage();
        if (nodeStage & (PipelineStage::WhiteBalance | PipelineStage::Exposure | PipelineStage::HSL)) {
            storeCache(nodeStage, recipe, rgb, width, height);
        }
    }

    // Step 3: Convert float RGB to RGBA8
    std::vector<uint8_t> rgba(static_cast<size_t>(pixel_count) * 4);
    toRGBA8(rgb.data(), rgba.data(), pixel_count);

    VEGA_LOG_INFO("Pipeline: total processing time: {:.1f}ms", totalTimer.elapsed_ms());

    return rgba;
}

} // namespace vega
