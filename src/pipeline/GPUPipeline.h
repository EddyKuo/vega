#pragma once
#include "gpu/D3D11Context.h"
#include "gpu/TexturePool.h"
#include "gpu/ComputeShader.h"
#include "gpu/ConstantBuffer.h"
#include "raw/RawImage.h"
#include "pipeline/EditRecipe.h"
#include <memory>
#include <filesystem>

namespace vega {

class GPUPipeline {
public:
    bool initialize(D3D11Context& ctx);

    // Process raw image on GPU, returns SRV for display.
    // preview_scale: 1.0 = full res, 0.5 = half, 0.125 = 1/8th
    ID3D11ShaderResourceView* process(const RawImage& raw, const EditRecipe& recipe,
                                       float preview_scale = 1.0f);

    // Upload demosaiced linear RGB data to GPU (call once when image changes).
    // Internally demosaics+WB+color-transforms on CPU, then uploads the float3 result.
    void uploadRawData(const RawImage& raw);

    bool isInitialized() const { return initialized_; }

    // Dimensions of the currently loaded raw image on GPU
    uint32_t rawWidth()  const { return raw_width_; }
    uint32_t rawHeight() const { return raw_height_; }

private:
    D3D11Context* ctx_ = nullptr;
    std::unique_ptr<TexturePool> pool_;
    bool initialized_ = false;

    // ── Shaders ──
    ComputeShader wb_exposure_shader_;
    ComputeShader tone_curve_shader_;
    ComputeShader hsl_shader_;
    // Note: histogram_shader_ is defined but not dispatched in the display path;
    // it would be used separately for histogram computation.
    ComputeShader histogram_shader_;

    // ── Raw data on GPU ──
    ComPtr<ID3D11Texture2D>          raw_texture_;
    ComPtr<ID3D11ShaderResourceView> raw_srv_;
    uint32_t raw_width_  = 0;
    uint32_t raw_height_ = 0;

    // ── Output (last rendered result, kept alive for display) ──
    TexturePool::TextureHandle output_;

    // ── Constant buffers ──
    // CB slot 0: white balance + exposure + contrast + highlights + shadows
    struct WBExposureCB {
        float wb_r, wb_g, wb_b, exposure;
        float contrast, highlights, shadows, whites;
        float blacks, pad1, pad2, pad3;
    };
    ConstantBuffer<WBExposureCB> wb_exp_cb_;

    // CB slot 1: HSL adjustments
    struct HSLCB {
        float hsl_hue[8];
        float hsl_sat[8];
        float hsl_lum[8];
        float vibrance, saturation, pad1, pad2;
    };
    ConstantBuffer<HSLCB> hsl_cb_;

    // CB slot 2: dimensions + scale info (shared across all shaders)
    struct DimensionsCB {
        uint32_t src_width, src_height;
        uint32_t dst_width, dst_height;
    };
    ConstantBuffer<DimensionsCB> dim_cb_;

    // ── Curve LUT textures (1D, 4096 entries each) ──
    static constexpr uint32_t CURVE_LUT_SIZE = 4096;
    ComPtr<ID3D11Texture1D>          curve_luts_[4]; // RGB master, R, G, B
    ComPtr<ID3D11ShaderResourceView> curve_srvs_[4];

    // ── Internal helpers ──
    void loadShaders();
    void createCurveLUTs();
    void updateCurveLUTs(const EditRecipe& recipe);

    // Build a piecewise-linear LUT from a set of curve control points
    static void evaluateCurve(const std::vector<CurvePoint>& points,
                              float* lut, uint32_t lut_size);

    // White-balance temperature/tint to RGB multipliers
    static void temperatureTintToRGB(float temperature, float tint,
                                     float& r, float& g, float& b);

    // Find shader directory
    std::filesystem::path shaderDir() const;
};

} // namespace vega
