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

    // Upload raw bayer data to GPU and run GPU demosaic.
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
    ComputeShader demosaic_shader_;
    ComputeShader wb_exposure_shader_;
    ComputeShader tone_curve_shader_;
    ComputeShader hsl_shader_;
    ComputeShader denoise_shader_;
    ComputeShader sharpen_shader_;
    ComputeShader gamma_shader_;
    // Note: histogram_shader_ is defined but not dispatched in the display path;
    // it would be used separately for histogram computation.
    ComputeShader histogram_shader_;

    // ── Raw bayer data on GPU (single-channel float) ──
    ComPtr<ID3D11Texture2D>          bayer_texture_;
    ComPtr<ID3D11ShaderResourceView> bayer_srv_;

    // ── Demosaiced linear RGB on GPU ──
    ComPtr<ID3D11Texture2D>          raw_texture_;
    ComPtr<ID3D11ShaderResourceView> raw_srv_;
    ComPtr<ID3D11UnorderedAccessView> raw_uav_;
    uint32_t raw_width_  = 0;
    uint32_t raw_height_ = 0;

    // ── Demosaic CB ──
    struct DemosaicCB {
        uint32_t width, height, bayer_pattern;
        float pad0;
        float wb_r, wb_g, wb_b;
        float pad1;
        float color_row0[4];
        float color_row1[4];
        float color_row2[4];
    };
    ConstantBuffer<DemosaicCB> demosaic_cb_;

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

    // CB slot 0 (denoise pass): luminance/chroma denoise parameters
    struct DenoiseCB {
        float luma_strength;    // recipe.denoise_luminance / 100
        float chroma_strength;  // recipe.denoise_color / 100
        float detail_keep;      // recipe.denoise_detail / 100
        int   luma_radius;      // 1 + int(luma_strength * 2)

        int   chroma_radius;    // 1 + int(chroma_strength * 2)
        float pad0, pad1, pad2;
    };
    ConstantBuffer<DenoiseCB> denoise_cb_;

    // CB slot 0 (sharpen pass): unsharp-mask sharpening parameters
    struct SharpenCB {
        float amount;    // recipe.sharpen_amount / 50
        int   radius;    // max(1, int(recipe.sharpen_radius + 0.5f))
        float masking;   // recipe.sharpen_masking / 100
        float pad0;
    };
    ConstantBuffer<SharpenCB> sharpen_cb_;

    // ── Curve LUT textures (1D, 4096 entries each) ──
    static constexpr uint32_t CURVE_LUT_SIZE = 4096;
    ComPtr<ID3D11Texture1D>          curve_luts_[4]; // RGB master, R, G, B
    ComPtr<ID3D11ShaderResourceView> curve_srvs_[4];
    ComPtr<ID3D11SamplerState>       linear_sampler_;

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
