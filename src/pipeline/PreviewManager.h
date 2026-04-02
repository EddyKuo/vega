#pragma once
#include "pipeline/GPUPipeline.h"
#include "pipeline/Pipeline.h"
#include "raw/RawImage.h"
#include "pipeline/EditRecipe.h"
#include <chrono>
#include <vector>
#include <cstdint>

namespace vega {

class PreviewManager {
public:
    void initialize(D3D11Context& ctx);

    // Called when the user modifies the recipe. Triggers progressive preview refresh.
    void onRecipeChanged(const RawImage& raw, const EditRecipe& recipe);

    // Called every frame. Returns the best available preview SRV for display.
    // Also fills rgba_for_histogram with a (possibly lower-res) RGBA8 buffer
    // suitable for computing a histogram on the CPU side.
    ID3D11ShaderResourceView* update(std::vector<uint8_t>& rgba_for_histogram,
                                      uint32_t& hist_width, uint32_t& hist_height);

    // Has an image been loaded?
    bool hasImage() const { return has_image_; }

    // Load a new raw image. Uploads to GPU if available.
    void setImage(const RawImage& raw);

    // Force a full-resolution re-render on next update()
    void invalidate();

    // Is GPU acceleration active?
    bool isGPUAvailable() const { return gpu_available_; }

private:
    GPUPipeline gpu_pipeline_;
    Pipeline    cpu_pipeline_;  // fallback when GPU is unavailable

    D3D11Context* ctx_ = nullptr;

    enum class PreviewState {
        Idle,    // No pending work
        Fast,    // Render at 1/8 resolution (immediate feedback)
        Medium,  // Render at 1/4 resolution
        Full     // Render at full resolution
    };
    PreviewState state_ = PreviewState::Idle;

    using Clock = std::chrono::steady_clock;
    Clock::time_point last_change_{};

    EditRecipe pending_recipe_{};
    const RawImage* current_raw_ = nullptr;

    bool has_image_    = false;
    bool gpu_available_ = false;
    bool raw_uploaded_  = false;

    // Current preview output
    ID3D11ShaderResourceView* current_srv_ = nullptr;

    // CPU-rendered RGBA8 buffer (used for histogram and as fallback display)
    std::vector<uint8_t> current_rgba_;
    uint32_t current_w_ = 0;
    uint32_t current_h_ = 0;

    // Fallback: CPU-rendered RGBA8 uploaded to a GPU texture for ImGui display
    ComPtr<ID3D11Texture2D>          cpu_display_tex_;
    ComPtr<ID3D11ShaderResourceView> cpu_display_srv_;
    uint32_t cpu_display_w_ = 0;
    uint32_t cpu_display_h_ = 0;

    // Timing thresholds (milliseconds since last recipe change)
    static constexpr int64_t FAST_THRESHOLD_MS   = 0;     // immediate
    static constexpr int64_t MEDIUM_THRESHOLD_MS = 200;   // after 200ms settle
    static constexpr int64_t FULL_THRESHOLD_MS   = 1000;  // after 1s settle

    // Scale factors for each preview tier
    static constexpr float FAST_SCALE   = 0.125f; // 1/8
    static constexpr float MEDIUM_SCALE = 0.25f;  // 1/4
    static constexpr float FULL_SCALE   = 1.0f;

    // Internal rendering helpers
    ID3D11ShaderResourceView* renderGPU(const RawImage& raw, const EditRecipe& recipe,
                                         float scale);
    void renderCPU(const RawImage& raw, const EditRecipe& recipe);
    void uploadCPUResultToGPU();

    // Read back a GPU texture to an RGBA8 CPU buffer for histogram
    void readbackForHistogram(ID3D11ShaderResourceView* srv,
                              std::vector<uint8_t>& out_rgba,
                              uint32_t& out_w, uint32_t& out_h);
};

} // namespace vega
