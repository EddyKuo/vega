#define NOMINMAX
#include "pipeline/GPUPipeline.h"
#include "core/Logger.h"
#include "core/Timer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace vega {

// ── Shader thread group size (must match HLSL [numthreads(N,N,1)]) ──
static constexpr uint32_t THREAD_GROUP_SIZE = 16;

static uint32_t divRoundUp(uint32_t value, uint32_t divisor)
{
    return (value + divisor - 1) / divisor;
}

// ── Public ──────────────────────────────────────────────────────────────────

bool GPUPipeline::initialize(D3D11Context& ctx)
{
    if (initialized_)
        return true;

    ctx_ = &ctx;
    ID3D11Device* device = ctx.device();
    if (!device) {
        VEGA_LOG_ERROR("GPUPipeline: D3D11 device is null");
        return false;
    }

    // Create texture pool
    pool_ = std::make_unique<TexturePool>(device);

    // Create constant buffers
    if (!demosaic_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create Demosaic constant buffer");
        return false;
    }
    if (!wb_exp_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create WBExposure constant buffer");
        return false;
    }
    if (!hsl_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create HSL constant buffer");
        return false;
    }
    if (!dim_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create Dimensions constant buffer");
        return false;
    }
    if (!denoise_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create Denoise constant buffer");
        return false;
    }
    if (!sharpen_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create Sharpen constant buffer");
        return false;
    }
    if (!presence_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create Presence constant buffer");
        return false;
    }
    if (!color_grading_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create ColorGrading constant buffer");
        return false;
    }
    if (!crop_rotate_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create CropRotate constant buffer");
        return false;
    }
    if (!effects_cb_.create(device)) {
        VEGA_LOG_ERROR("GPUPipeline: failed to create Effects constant buffer");
        return false;
    }

    // Create linear sampler for LUT sampling
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        HRESULT hr = device->CreateSamplerState(&sd, &linear_sampler_);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("GPUPipeline: failed to create sampler state");
            return false;
        }
    }

    // Create curve LUT textures
    createCurveLUTs();

    // Load compute shaders
    loadShaders();

    // Shaders are optional at init time -- they may not exist yet if the
    // HLSL files haven't been written / compiled.  We still consider the
    // pipeline "initialized" so the rest of the infrastructure (texture pool,
    // constant buffers, curve LUTs) can be used.  process() will early-out
    // if the required shaders are missing.
    initialized_ = true;
    VEGA_LOG_INFO("GPUPipeline: initialized successfully");
    return true;
}

void GPUPipeline::uploadRawData(const RawImage& raw)
{
    if (!initialized_ || !ctx_) {
        VEGA_LOG_ERROR("GPUPipeline: not initialized, cannot upload raw data");
        return;
    }

    if (raw.bayer_data.empty() || raw.width == 0 || raw.height == 0) {
        VEGA_LOG_WARN("GPUPipeline: raw image is empty, nothing to upload");
        return;
    }

    Timer timer;
    ID3D11Device* device = ctx_->device();
    ID3D11DeviceContext* dc = ctx_->context();
    uint32_t width  = raw.width;
    uint32_t height = raw.height;

    // ── Step 1: Upload raw bayer data as single-channel R32_FLOAT texture ──
    {
        bayer_texture_.Reset();
        bayer_srv_.Reset();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width      = width;
        desc.Height     = height;
        desc.MipLevels  = 1;
        desc.ArraySize  = 1;
        desc.Format     = DXGI_FORMAT_R32_FLOAT;
        desc.SampleDesc = {1, 0};
        desc.Usage      = D3D11_USAGE_DEFAULT;
        desc.BindFlags  = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem     = raw.bayer_data.data();
        init.SysMemPitch = width * sizeof(float);

        HRESULT hr = device->CreateTexture2D(&desc, &init, &bayer_texture_);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("GPUPipeline: failed to create bayer texture: 0x{:08X}",
                           static_cast<uint32_t>(hr));
            return;
        }

        hr = device->CreateShaderResourceView(bayer_texture_.Get(), nullptr, &bayer_srv_);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("GPUPipeline: failed to create bayer SRV");
            bayer_texture_.Reset();
            return;
        }
    }

    VEGA_LOG_INFO("GPUPipeline: bayer upload: {:.1f}ms", timer.elapsed_ms());

    // ── Step 2: Create output RGBA texture for demosaiced result ──
    if (raw_width_ != width || raw_height_ != height) {
        raw_texture_.Reset();
        raw_srv_.Reset();
        raw_uav_.Reset();
    }

    if (!raw_texture_) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width      = width;
        desc.Height     = height;
        desc.MipLevels  = 1;
        desc.ArraySize  = 1;
        desc.Format     = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.SampleDesc = {1, 0};
        desc.Usage      = D3D11_USAGE_DEFAULT;
        desc.BindFlags  = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &raw_texture_);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("GPUPipeline: failed to create demosaic output texture");
            return;
        }

        hr = device->CreateShaderResourceView(raw_texture_.Get(), nullptr, &raw_srv_);
        if (FAILED(hr)) { raw_texture_.Reset(); return; }

        hr = device->CreateUnorderedAccessView(raw_texture_.Get(), nullptr, &raw_uav_);
        if (FAILED(hr)) { raw_texture_.Reset(); raw_srv_.Reset(); return; }
    }

    raw_width_  = width;
    raw_height_ = height;

    // ── Step 3: Run GPU demosaic shader ──
    if (demosaic_shader_.isValid()) {
        DemosaicCB cb{};
        cb.width = width;
        cb.height = height;
        cb.bayer_pattern = raw.metadata.bayer_pattern;

        float g_norm = (raw.wb_multipliers[1] + raw.wb_multipliers[3]) * 0.5f;
        if (g_norm <= 0.0f) g_norm = 1.0f;
        cb.wb_r = raw.wb_multipliers[0] / g_norm;
        cb.wb_g = 1.0f;
        cb.wb_b = raw.wb_multipliers[2] / g_norm;

        const float* M = raw.color_matrix;
        cb.color_row0[0] = M[0]; cb.color_row0[1] = M[1]; cb.color_row0[2] = M[2]; cb.color_row0[3] = 0;
        cb.color_row1[0] = M[3]; cb.color_row1[1] = M[4]; cb.color_row1[2] = M[5]; cb.color_row1[3] = 0;
        cb.color_row2[0] = M[6]; cb.color_row2[1] = M[7]; cb.color_row2[2] = M[8]; cb.color_row2[3] = 0;

        demosaic_cb_.update(dc, cb);
        demosaic_cb_.bindCS(dc, 0);

        ID3D11ShaderResourceView* srvs[] = { bayer_srv_.Get() };
        dc->CSSetShaderResources(0, 1, srvs);

        ID3D11UnorderedAccessView* uavs[] = { raw_uav_.Get() };
        dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        static constexpr uint32_t TILE = 16;
        uint32_t gx = (width + TILE - 1) / TILE;
        uint32_t gy = (height + TILE - 1) / TILE;
        demosaic_shader_.bind(dc);
        demosaic_shader_.dispatch(dc, gx, gy, 1);

        // Unbind
        ID3D11ShaderResourceView* null_srv[] = { nullptr };
        dc->CSSetShaderResources(0, 1, null_srv);
        ID3D11UnorderedAccessView* null_uav[] = { nullptr };
        dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);

        VEGA_LOG_INFO("GPUPipeline: GPU demosaic + upload total: {:.1f}ms ({}x{})",
                      timer.elapsed_ms(), width, height);
    } else {
        VEGA_LOG_ERROR("GPUPipeline: demosaic shader not available");
    }
}

// ---------------------------------------------------------------------------
// Two-phase upload: CPU demosaic (thread-safe) + GPU upload (main thread)
// ---------------------------------------------------------------------------

ID3D11ShaderResourceView* GPUPipeline::process(const RawImage& raw,
                                                const EditRecipe& recipe,
                                                float preview_scale)
{
    if (!initialized_ || !ctx_) {
        VEGA_LOG_ERROR("GPUPipeline: not initialized");
        return nullptr;
    }

    // Ensure raw data is uploaded
    if (!raw_srv_) {
        VEGA_LOG_WARN("GPUPipeline: no raw data uploaded, call uploadRawData() first");
        return nullptr;
    }

    // Check that at least the WB+exposure shader is available
    if (!wb_exposure_shader_.isValid()) {
        VEGA_LOG_WARN("GPUPipeline: shaders not loaded, cannot process on GPU");
        return nullptr;
    }

    Timer timer;
    ID3D11DeviceContext* dc = ctx_->context();

    // Calculate output dimensions based on preview scale
    preview_scale = std::clamp(preview_scale, 0.0625f, 1.0f);
    uint32_t dst_w = std::max(1u, static_cast<uint32_t>(raw_width_  * preview_scale));
    uint32_t dst_h = std::max(1u, static_cast<uint32_t>(raw_height_ * preview_scale));

    // Release previous output if dimensions changed
    if (output_.isValid() && (output_.width != dst_w || output_.height != dst_h)) {
        pool_->release(output_);
    }

    // ── Acquire intermediate and output textures ──
    auto tex_a = pool_->acquire(dst_w, dst_h, DXGI_FORMAT_R32G32B32A32_FLOAT);
    auto tex_b = pool_->acquire(dst_w, dst_h, DXGI_FORMAT_R32G32B32A32_FLOAT);

    if (!tex_a.isValid() || !tex_b.isValid()) {
        VEGA_LOG_ERROR("GPUPipeline: failed to acquire intermediate textures");
        if (tex_a.isValid()) pool_->release(tex_a);
        if (tex_b.isValid()) pool_->release(tex_b);
        return nullptr;
    }

    // Update dimensions constant buffer (shared by all shaders at slot 2)
    {
        DimensionsCB dim{};
        dim.src_width  = raw_width_;
        dim.src_height = raw_height_;
        dim.dst_width  = dst_w;
        dim.dst_height = dst_h;
        dim_cb_.update(dc, dim);
        dim_cb_.bindCS(dc, 2);
    }

    uint32_t groups_x = divRoundUp(dst_w, THREAD_GROUP_SIZE);
    uint32_t groups_y = divRoundUp(dst_h, THREAD_GROUP_SIZE);

    // ──────────────────────────────────────────────────────────────────────
    // Pass 1: White Balance + Exposure + Contrast + Highlights/Shadows
    //   Input:  raw_srv_ (t0)
    //   Output: tex_a (u0)
    // ──────────────────────────────────────────────────────────────────────
    {
        WBExposureCB cb{};
        // Relative WB correction: user / default, matching CPU WhiteBalanceNode
        static constexpr float DEFAULT_TEMP = 5500.0f;
        static constexpr float DEFAULT_TINT = 0.0f;
        float def_r, def_g, def_b;
        temperatureTintToRGB(DEFAULT_TEMP, DEFAULT_TINT, def_r, def_g, def_b);
        float usr_r, usr_g, usr_b;
        temperatureTintToRGB(recipe.wb_temperature, recipe.wb_tint, usr_r, usr_g, usr_b);
        cb.wb_r = usr_r / def_r;
        cb.wb_g = usr_g / def_g;
        cb.wb_b = usr_b / def_b;
        cb.exposure   = recipe.exposure;
        cb.contrast   = recipe.contrast;
        cb.highlights = recipe.highlights;
        cb.shadows    = recipe.shadows;
        cb.whites     = recipe.whites;
        cb.blacks     = recipe.blacks;
        wb_exp_cb_.update(dc, cb);
        wb_exp_cb_.bindCS(dc, 0);

        // Bind input SRV (t0) and output UAV (u0)
        ID3D11ShaderResourceView* srvs[] = { raw_srv_.Get() };
        dc->CSSetShaderResources(0, 1, srvs);

        ID3D11UnorderedAccessView* uavs[] = { tex_a.uav.Get() };
        dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        wb_exposure_shader_.bind(dc);
        wb_exposure_shader_.dispatch(dc, groups_x, groups_y, 1);

        // Unbind to allow next pass to read tex_a
        ID3D11ShaderResourceView* null_srv[] = { nullptr };
        dc->CSSetShaderResources(0, 1, null_srv);
        ID3D11UnorderedAccessView* null_uav[] = { nullptr };
        dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);
    }

    // ──────────────────────────────────────────────────────────────────────
    // Pass 1b: Presence (Clarity, Texture, Dehaze) — linear light space
    //   Input:  tex_a (t0)
    //   Output: tex_b (u0)
    //   Skip if all three parameters are negligible.
    // ──────────────────────────────────────────────────────────────────────
    {
        const bool run_presence = presence_shader_.isValid() &&
                                  (std::fabs(recipe.clarity) > 0.5f ||
                                   std::fabs(recipe.texture) > 0.5f ||
                                   std::fabs(recipe.dehaze)  > 0.5f);

        if (run_presence) {
            PresenceCB cb{};
            cb.clarity = recipe.clarity;
            cb.texture = recipe.texture;
            cb.dehaze  = recipe.dehaze;
            cb.pad0    = 0.0f;
            cb.width   = dst_w;
            cb.height  = dst_h;
            cb.pad1    = 0;
            cb.pad2    = 0;
            presence_cb_.update(dc, cb);
            presence_cb_.bindCS(dc, 0);

            ID3D11ShaderResourceView* srvs[] = { tex_a.srv.Get() };
            dc->CSSetShaderResources(0, 1, srvs);

            ID3D11UnorderedAccessView* uavs[] = { tex_b.uav.Get() };
            dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

            presence_shader_.bind(dc);
            presence_shader_.dispatch(dc, groups_x, groups_y, 1);

            // Unbind
            ID3D11ShaderResourceView* null_srv2[] = { nullptr };
            dc->CSSetShaderResources(0, 1, null_srv2);
            ID3D11UnorderedAccessView* null_uav2[] = { nullptr };
            dc->CSSetUnorderedAccessViews(0, 1, null_uav2, nullptr);

            // Swap: result is now in tex_b; swap so tone curve reads from tex_a
            std::swap(tex_a, tex_b);
        }
        // If skipped, tex_a still holds the WB+Exposure result — no copy needed.
    }

    // ──────────────────────────────────────────────────────────────────────
    // Pass 2: Tone Curve (using 1D LUT textures)
    //   Input:  tex_a (t0), curve LUTs (t1-t4)
    //   Output: tex_b (u0)
    // ──────────────────────────────────────────────────────────────────────
    if (tone_curve_shader_.isValid()) {
        updateCurveLUTs(recipe);

        // Bind linear sampler at s0 for LUT sampling
        ID3D11SamplerState* samplers[] = { linear_sampler_.Get() };
        dc->CSSetSamplers(0, 1, samplers);

        ID3D11ShaderResourceView* srvs[5] = {
            tex_a.srv.Get(),
            curve_srvs_[0].Get(),  // RGB master
            curve_srvs_[1].Get(),  // R
            curve_srvs_[2].Get(),  // G
            curve_srvs_[3].Get(),  // B
        };
        dc->CSSetShaderResources(0, 5, srvs);

        ID3D11UnorderedAccessView* uavs[] = { tex_b.uav.Get() };
        dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        tone_curve_shader_.bind(dc);
        tone_curve_shader_.dispatch(dc, groups_x, groups_y, 1);

        // Unbind
        ID3D11ShaderResourceView* null_srvs[5] = {};
        dc->CSSetShaderResources(0, 5, null_srvs);
        ID3D11UnorderedAccessView* null_uav[] = { nullptr };
        dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);
    } else {
        // No tone curve shader -- copy tex_a to tex_b so the chain continues
        dc->CopyResource(tex_b.texture.Get(), tex_a.texture.Get());
    }

    // ──────────────────────────────────────────────────────────────────────
    // Pass 3: HSL + Vibrance + Saturation
    //   Input:  tex_b (t0)
    //   Output: tex_a (u0) -- ping-pong back
    // ──────────────────────────────────────────────────────────────────────
    if (hsl_shader_.isValid()) {
        HSLCB cb{};
        for (int i = 0; i < 8; ++i) {
            cb.hsl_hue[i] = recipe.hsl_hue[i];
            cb.hsl_sat[i] = recipe.hsl_saturation[i];
            cb.hsl_lum[i] = recipe.hsl_luminance[i];
        }
        cb.vibrance   = recipe.vibrance;
        cb.saturation = recipe.saturation;
        cb.bw_mode    = recipe.bw_mode ? 1u : 0u;
        for (int i = 0; i < 8; ++i) cb.bw_mix[i] = recipe.bw_mix[i];
        hsl_cb_.update(dc, cb);
        hsl_cb_.bindCS(dc, 1);

        ID3D11ShaderResourceView* srvs[] = { tex_b.srv.Get() };
        dc->CSSetShaderResources(0, 1, srvs);

        ID3D11UnorderedAccessView* uavs[] = { tex_a.uav.Get() };
        dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        hsl_shader_.bind(dc);
        hsl_shader_.dispatch(dc, groups_x, groups_y, 1);

        // Unbind
        ID3D11ShaderResourceView* null_srv[] = { nullptr };
        dc->CSSetShaderResources(0, 1, null_srv);
        ID3D11UnorderedAccessView* null_uav[] = { nullptr };
        dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);
    } else {
        // No HSL shader -- copy tex_b to tex_a so chain continues
        dc->CopyResource(tex_a.texture.Get(), tex_b.texture.Get());
    }

    // After pass 3 the latest result is in tex_a (result_in_a == true).
    // Passes 4 and 5 ping-pong without copies when skipped.
    bool result_in_a = true;

    // ──────────────────────────────────────────────────────────────────────
    // Pass 3b: Color Grading (Shadows / Midtones / Highlights)
    //   Input:  tex_a (t0)   Output: tex_b (u0)
    //   Skip if all three wheels have saturation below threshold.
    // ──────────────────────────────────────────────────────────────────────
    {
        const bool run_cg = color_grading_shader_.isValid() &&
                            (recipe.cg_shadows.saturation    >= 0.1f ||
                             recipe.cg_midtones.saturation   >= 0.1f ||
                             recipe.cg_highlights.saturation >= 0.1f);

        if (run_cg) {
            ColorGradingCB cb{};
            cb.shadow_hue  = recipe.cg_shadows.hue;
            cb.shadow_sat  = recipe.cg_shadows.saturation;
            cb.mid_hue     = recipe.cg_midtones.hue;
            cb.mid_sat     = recipe.cg_midtones.saturation;
            cb.high_hue    = recipe.cg_highlights.hue;
            cb.high_sat    = recipe.cg_highlights.saturation;
            cb.blending    = recipe.cg_blending;
            cb.balance     = recipe.cg_balance;
            cb.width       = dst_w;
            cb.height      = dst_h;
            cb.pad0 = cb.pad1 = 0.0f;
            color_grading_cb_.update(dc, cb);
            color_grading_cb_.bindCS(dc, 0);

            // Input from tex_a (result_in_a == true at this point)
            ID3D11ShaderResourceView* srvs[] = { tex_a.srv.Get() };
            dc->CSSetShaderResources(0, 1, srvs);

            ID3D11UnorderedAccessView* uavs[] = { tex_b.uav.Get() };
            dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

            color_grading_shader_.bind(dc);
            color_grading_shader_.dispatch(dc, groups_x, groups_y, 1);

            // Unbind before next pass
            ID3D11ShaderResourceView* null_srv[] = { nullptr };
            dc->CSSetShaderResources(0, 1, null_srv);
            ID3D11UnorderedAccessView* null_uav[] = { nullptr };
            dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);

            result_in_a = false; // result now in tex_b
        }
        // Skip: result_in_a stays true (tex_a holds the data)
    }

    // ──────────────────────────────────────────────────────────────────────
    // Pass 4: Denoise
    //   Input:  tex_a (t0)   Output: tex_b (u0)
    //   Skip if both luminance and color noise reduction are negligible.
    // ──────────────────────────────────────────────────────────────────────
    {
        const bool run_denoise = denoise_shader_.isValid() &&
                                 (recipe.denoise_luminance >= 0.5f ||
                                  recipe.denoise_color     >= 0.5f);

        if (run_denoise) {
            DenoiseCB cb{};
            cb.luma_strength   = recipe.denoise_luminance / 100.0f;
            cb.chroma_strength = recipe.denoise_color     / 100.0f;
            cb.detail_keep     = recipe.denoise_detail    / 100.0f;
            cb.luma_radius     = 1 + static_cast<int>(cb.luma_strength   * 2.0f);
            cb.chroma_radius   = 1 + static_cast<int>(cb.chroma_strength * 2.0f);
            cb.pad0 = cb.pad1 = cb.pad2 = 0.0f;
            denoise_cb_.update(dc, cb);
            denoise_cb_.bindCS(dc, 0);

            // Input from tex_a, output to tex_b
            ID3D11ShaderResourceView* srvs[] = { tex_a.srv.Get() };
            dc->CSSetShaderResources(0, 1, srvs);

            ID3D11UnorderedAccessView* uavs[] = { tex_b.uav.Get() };
            dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

            denoise_shader_.bind(dc);
            denoise_shader_.dispatch(dc, groups_x, groups_y, 1);

            // Unbind before next pass
            ID3D11ShaderResourceView* null_srv[] = { nullptr };
            dc->CSSetShaderResources(0, 1, null_srv);
            ID3D11UnorderedAccessView* null_uav[] = { nullptr };
            dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);

            result_in_a = false; // result now in tex_b
        } else {
            // Skip: no dispatch, no copy — result_in_a stays true (tex_a holds the data)
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // Pass 5: Sharpen (USM)
    //   Input:  current result texture   Output: the other texture
    //   Skip if sharpen_amount < 0.5.
    // ──────────────────────────────────────────────────────────────────────
    {
        const bool run_sharpen = sharpen_shader_.isValid() &&
                                 (recipe.sharpen_amount >= 0.5f);

        if (run_sharpen) {
            SharpenCB cb{};
            cb.amount  = recipe.sharpen_amount / 50.0f;
            cb.radius  = std::max(1, static_cast<int>(recipe.sharpen_radius + 0.5f));
            cb.masking = recipe.sharpen_masking / 100.0f;
            cb.pad0    = 0.0f;
            sharpen_cb_.update(dc, cb);
            sharpen_cb_.bindCS(dc, 0);

            // Bind whichever texture holds the current result as input
            ID3D11ShaderResourceView* in_srv  = result_in_a ? tex_a.srv.Get() : tex_b.srv.Get();
            ID3D11UnorderedAccessView* out_uav = result_in_a ? tex_b.uav.Get() : tex_a.uav.Get();

            ID3D11ShaderResourceView* srvs[] = { in_srv };
            dc->CSSetShaderResources(0, 1, srvs);

            ID3D11UnorderedAccessView* uavs[] = { out_uav };
            dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

            sharpen_shader_.bind(dc);
            sharpen_shader_.dispatch(dc, groups_x, groups_y, 1);

            // Unbind before next pass
            ID3D11ShaderResourceView* null_srv[] = { nullptr };
            dc->CSSetShaderResources(0, 1, null_srv);
            ID3D11UnorderedAccessView* null_uav[] = { nullptr };
            dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);

            result_in_a = !result_in_a; // flip: output went to the other texture
        }
        // Skip: no flip, no copy — current result pointer stays as-is
    }

    // ──────────────────────────────────────────────────────────────────────
    // Pass 6: sRGB Gamma output (mandatory)
    //   Input:  current result texture   Output: the other texture
    //   Uses only the dimensions CB at b2 (already bound).
    // ──────────────────────────────────────────────────────────────────────
    if (gamma_shader_.isValid()) {
        ID3D11ShaderResourceView* in_srv  = result_in_a ? tex_a.srv.Get() : tex_b.srv.Get();
        ID3D11UnorderedAccessView* out_uav = result_in_a ? tex_b.uav.Get() : tex_a.uav.Get();

        ID3D11ShaderResourceView* srvs[] = { in_srv };
        dc->CSSetShaderResources(0, 1, srvs);

        ID3D11UnorderedAccessView* uavs[] = { out_uav };
        dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        gamma_shader_.bind(dc);
        gamma_shader_.dispatch(dc, groups_x, groups_y, 1);

        // Unbind
        ID3D11ShaderResourceView* null_srv[] = { nullptr };
        dc->CSSetShaderResources(0, 1, null_srv);
        ID3D11UnorderedAccessView* null_uav[] = { nullptr };
        dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);

        result_in_a = !result_in_a; // output went to the other texture
    }
    // If gamma shader is missing, the linear result is still usable for display.

    // ──────────────────────────────────────────────────────────────────────
    // Pass 6b: Effects — Vignette + Film Grain (sRGB space, after gamma)
    //   Input:  current result texture   Output: the other texture
    //   Skip when all parameters are at their neutral defaults.
    // ──────────────────────────────────────────────────────────────────────
    {
        const bool run_effects = effects_shader_.isValid() &&
                                 (std::fabs(recipe.vignette_amount) > 0.5f ||
                                  recipe.grain_amount               > 0.5f);

        if (run_effects) {
            static uint32_t frame_counter = 0;
            ++frame_counter;

            EffectsCB cb{};
            cb.vig_amount    = recipe.vignette_amount;
            cb.vig_midpoint  = recipe.vignette_midpoint;
            cb.vig_roundness = recipe.vignette_roundness;
            cb.vig_feather   = recipe.vignette_feather;
            cb.grain_amount  = recipe.grain_amount;
            cb.grain_size    = recipe.grain_size;
            cb.grain_roughness = recipe.grain_roughness;
            cb.pad0          = 0.0f;
            cb.width         = dst_w;
            cb.height        = dst_h;
            cb.frame_seed    = frame_counter;
            cb.pad1          = 0;
            effects_cb_.update(dc, cb);
            effects_cb_.bindCS(dc, 0);

            ID3D11ShaderResourceView* in_srv  = result_in_a ? tex_a.srv.Get() : tex_b.srv.Get();
            ID3D11UnorderedAccessView* out_uav = result_in_a ? tex_b.uav.Get() : tex_a.uav.Get();

            ID3D11ShaderResourceView* srvs[] = { in_srv };
            dc->CSSetShaderResources(0, 1, srvs);

            ID3D11UnorderedAccessView* uavs[] = { out_uav };
            dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

            effects_shader_.bind(dc);
            effects_shader_.dispatch(dc, groups_x, groups_y, 1);

            // Unbind
            ID3D11ShaderResourceView* null_srv[] = { nullptr };
            dc->CSSetShaderResources(0, 1, null_srv);
            ID3D11UnorderedAccessView* null_uav[] = { nullptr };
            dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);

            result_in_a = !result_in_a; // output went to the other texture
        }
        // Skip: result_in_a unchanged, no copy needed
    }

    // ──────────────────────────────────────────────────────────────────────
    // Pass 7: Crop & Rotate (optional — skipped when identity)
    //   Input:  current result at full dst_w x dst_h (sRGB)
    //   Output: new texture at cropped dimensions
    // ──────────────────────────────────────────────────────────────────────
    {
        const bool is_identity =
            (recipe.crop_left   < 1e-5f) &&
            (recipe.crop_top    < 1e-5f) &&
            (recipe.crop_right  > 1.0f - 1e-5f) &&
            (recipe.crop_bottom > 1.0f - 1e-5f) &&
            (std::fabs(recipe.rotation) < 0.001f);

        if (!is_identity && crop_rotate_shader_.isValid()) {
            // Clamp crop values defensively
            float cl = std::clamp(recipe.crop_left,   0.0f, 1.0f);
            float ct = std::clamp(recipe.crop_top,    0.0f, 1.0f);
            float cr = std::clamp(recipe.crop_right,  0.0f, 1.0f);
            float cb_bottom = std::clamp(recipe.crop_bottom, 0.0f, 1.0f);
            if (cl >= cr) cr = std::min(cl + 1e-3f, 1.0f);
            if (ct >= cb_bottom) cb_bottom = std::min(ct + 1e-3f, 1.0f);

            uint32_t crop_w = std::max(1u, static_cast<uint32_t>((cr - cl) * dst_w));
            uint32_t crop_h = std::max(1u, static_cast<uint32_t>((cb_bottom - ct) * dst_h));

            auto tex_crop = pool_->acquire(crop_w, crop_h, DXGI_FORMAT_R32G32B32A32_FLOAT);
            if (tex_crop.isValid()) {
                // Build constant buffer
                const float rot_rad = recipe.rotation * 3.14159265358979323846f / 180.0f;
                CropRotateCB cr_cb{};
                cr_cb.crop_left   = cl;
                cr_cb.crop_top    = ct;
                cr_cb.crop_right  = cr;
                cr_cb.crop_bottom = cb_bottom;
                cr_cb.rotation    = recipe.rotation;
                cr_cb.sin_r       = std::sin(rot_rad);
                cr_cb.cos_r       = std::cos(rot_rad);
                cr_cb.pad0        = 0.0f;
                cr_cb.src_width   = dst_w;
                cr_cb.src_height  = dst_h;
                cr_cb.dst_width   = crop_w;
                cr_cb.dst_height  = crop_h;
                crop_rotate_cb_.update(dc, cr_cb);
                crop_rotate_cb_.bindCS(dc, 0);

                // Input: current result
                ID3D11ShaderResourceView* in_srv =
                    result_in_a ? tex_a.srv.Get() : tex_b.srv.Get();

                ID3D11ShaderResourceView* srvs[] = { in_srv };
                dc->CSSetShaderResources(0, 1, srvs);

                ID3D11UnorderedAccessView* uavs[] = { tex_crop.uav.Get() };
                dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

                uint32_t gx = divRoundUp(crop_w, THREAD_GROUP_SIZE);
                uint32_t gy = divRoundUp(crop_h, THREAD_GROUP_SIZE);
                crop_rotate_shader_.bind(dc);
                crop_rotate_shader_.dispatch(dc, gx, gy, 1);

                // Unbind
                ID3D11ShaderResourceView* null_srv[] = { nullptr };
                dc->CSSetShaderResources(0, 1, null_srv);
                ID3D11UnorderedAccessView* null_uav[] = { nullptr };
                dc->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);

                // Release the ping-pong pair and keep the cropped texture
                pool_->release(tex_a);
                pool_->release(tex_b);

                // Release previous output if we had one
                if (output_.isValid())
                    pool_->release(output_);

                output_ = std::move(tex_crop);

                VEGA_LOG_DEBUG("GPUPipeline: process ({}x{} -> {}x{} crop, scale={:.3f}): {:.1f}ms",
                               dst_w, dst_h, crop_w, crop_h, preview_scale, timer.elapsed_ms());

                return output_.srv.Get();
            }
            // If texture acquisition failed, fall through to uncropped output
        }
    }

    // ── Determine which texture holds the final result ──
    // Release the unused texture back to the pool and retain the output one.
    TexturePool::TextureHandle final_tex  = result_in_a ? std::move(tex_a) : std::move(tex_b);
    TexturePool::TextureHandle unused_tex = result_in_a ? std::move(tex_b) : std::move(tex_a);

    pool_->release(unused_tex);

    // Release previous output if we had one
    if (output_.isValid())
        pool_->release(output_);

    output_ = std::move(final_tex);

    VEGA_LOG_DEBUG("GPUPipeline: process ({}x{}, scale={:.3f}): {:.1f}ms",
                   dst_w, dst_h, preview_scale, timer.elapsed_ms());

    return output_.srv.Get();
}

// ── Private helpers ─────────────────────────────────────────────────────────

std::filesystem::path GPUPipeline::shaderDir() const
{
    // Look for shaders/ directory relative to the executable, then relative to
    // the project root (for development builds).
    namespace fs = std::filesystem;

    // Try exe-relative first
    wchar_t exe_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    fs::path exe_dir = fs::path(exe_path).parent_path();

    fs::path candidates[] = {
        exe_dir / "shaders",
        exe_dir / ".." / "shaders",
        exe_dir / ".." / ".." / "shaders",
        exe_dir / ".." / ".." / ".." / "shaders",
        exe_dir / ".." / ".." / ".." / ".." / "shaders",
        fs::path("shaders"),  // CWD-relative fallback
    };

    for (auto& p : candidates) {
        if (fs::exists(p) && fs::is_directory(p))
            return fs::canonical(p);
    }

    // Default -- caller should handle missing files gracefully
    return exe_dir / "shaders";
}

void GPUPipeline::loadShaders()
{
    auto dir = shaderDir();
    ID3D11Device* device = ctx_->device();

    auto tryLoad = [&](ComputeShader& shader, const char* filename) {
        // Derive CSO name from HLSL filename (e.g. "tone_curve.hlsl" -> "tone_curve.cso")
        std::string base = std::filesystem::path(filename).stem().string();
        auto cso = dir / (base + ".cso");
        auto hlsl = dir / filename;

        // Prefer precompiled CSO (faster load, no d3dcompiler dependency)
        if (std::filesystem::exists(cso)) {
            if (!shader.loadFromCSO(device, cso)) {
                VEGA_LOG_WARN("GPUPipeline: failed to load CSO {}", cso.string());
            } else {
                VEGA_LOG_INFO("GPUPipeline: loaded shader CSO '{}'", cso.string());
                return;
            }
        }

#ifdef _DEBUG
        // Debug fallback: compile from HLSL source for live editing
        if (std::filesystem::exists(hlsl)) {
            if (!shader.compileFromFile(device, hlsl)) {
                VEGA_LOG_WARN("GPUPipeline: failed to compile {}: {}",
                              filename, shader.lastError());
            } else {
                VEGA_LOG_INFO("GPUPipeline: compiled shader '{}'", filename);
            }
            return;
        }
#endif

        VEGA_LOG_WARN("GPUPipeline: shader not found: {} (looked for {})",
                      cso.string(), hlsl.string());
    };

    tryLoad(demosaic_shader_,    "demosaic.hlsl");
    tryLoad(wb_exposure_shader_, "white_balance_exposure.hlsl");
    tryLoad(presence_shader_,    "presence.hlsl");
    tryLoad(tone_curve_shader_,  "tone_curve.hlsl");
    tryLoad(hsl_shader_,            "hsl_adjust.hlsl");
    tryLoad(color_grading_shader_,  "color_grading.hlsl");
    tryLoad(denoise_shader_,        "denoise.hlsl");
    tryLoad(sharpen_shader_,     "sharpen_usm.hlsl");
    tryLoad(gamma_shader_,       "gamma_output.hlsl");
    tryLoad(effects_shader_,     "effects.hlsl");
    tryLoad(crop_rotate_shader_, "crop_rotate.hlsl");
    tryLoad(histogram_shader_,   "histogram_compute.hlsl");
}

void GPUPipeline::createCurveLUTs()
{
    ID3D11Device* device = ctx_->device();

    // Identity LUT data (linear 0..1)
    std::vector<float> identity(CURVE_LUT_SIZE);
    for (uint32_t i = 0; i < CURVE_LUT_SIZE; ++i) {
        identity[i] = static_cast<float>(i) / static_cast<float>(CURVE_LUT_SIZE - 1);
    }

    for (int c = 0; c < 4; ++c) {
        D3D11_TEXTURE1D_DESC desc{};
        desc.Width     = CURVE_LUT_SIZE;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format    = DXGI_FORMAT_R32_FLOAT;
        desc.Usage     = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem     = identity.data();
        init.SysMemPitch = CURVE_LUT_SIZE * sizeof(float);

        HRESULT hr = device->CreateTexture1D(&desc, &init, &curve_luts_[c]);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("GPUPipeline: failed to create curve LUT[{}]: 0x{:08X}",
                           c, static_cast<uint32_t>(hr));
            continue;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format              = DXGI_FORMAT_R32_FLOAT;
        srv_desc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE1D;
        srv_desc.Texture1D.MipLevels = 1;

        hr = device->CreateShaderResourceView(curve_luts_[c].Get(), &srv_desc,
                                               &curve_srvs_[c]);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("GPUPipeline: failed to create curve SRV[{}]: 0x{:08X}",
                           c, static_cast<uint32_t>(hr));
        }
    }
}

void GPUPipeline::updateCurveLUTs(const EditRecipe& recipe)
{
    if (!ctx_)
        return;

    ID3D11DeviceContext* dc = ctx_->context();

    const std::vector<CurvePoint>* curves[4] = {
        &recipe.tone_curve_rgb,
        &recipe.tone_curve_r,
        &recipe.tone_curve_g,
        &recipe.tone_curve_b,
    };

    std::vector<float> lut(CURVE_LUT_SIZE);

    for (int c = 0; c < 4; ++c) {
        if (!curve_luts_[c])
            continue;

        evaluateCurve(*curves[c], lut.data(), CURVE_LUT_SIZE);

        // Upload via UpdateSubresource (the texture is DEFAULT usage)
        dc->UpdateSubresource(curve_luts_[c].Get(), 0, nullptr,
                              lut.data(), CURVE_LUT_SIZE * sizeof(float), 0);
    }
}

void GPUPipeline::evaluateCurve(const std::vector<CurvePoint>& points,
                                 float* lut, uint32_t lut_size)
{
    // Handle degenerate cases
    if (points.empty() || lut_size == 0) {
        for (uint32_t i = 0; i < lut_size; ++i)
            lut[i] = static_cast<float>(i) / static_cast<float>(lut_size - 1);
        return;
    }

    if (points.size() == 1) {
        for (uint32_t i = 0; i < lut_size; ++i)
            lut[i] = std::clamp(points[0].y, 0.0f, 1.0f);
        return;
    }

    // Sort control points by x (they should already be sorted, but be safe)
    std::vector<CurvePoint> sorted = points;
    std::sort(sorted.begin(), sorted.end(),
              [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });

    // Monotone cubic Hermite interpolation for smooth, non-oscillating curves.
    // Compute tangents using Fritsch-Carlson method.
    size_t n = sorted.size();
    std::vector<float> m(n, 0.0f); // tangents

    if (n >= 2) {
        // Compute secants
        std::vector<float> delta(n - 1);
        for (size_t i = 0; i < n - 1; ++i) {
            float dx = sorted[i + 1].x - sorted[i].x;
            if (dx > 0.0f)
                delta[i] = (sorted[i + 1].y - sorted[i].y) / dx;
            else
                delta[i] = 0.0f;
        }

        // Interior tangents: average of adjacent secants (clamped for monotonicity)
        if (n == 2) {
            m[0] = delta[0];
            m[1] = delta[0];
        } else {
            m[0] = delta[0];
            m[n - 1] = delta[n - 2];
            for (size_t i = 1; i < n - 1; ++i) {
                if (delta[i - 1] * delta[i] <= 0.0f) {
                    m[i] = 0.0f;
                } else {
                    m[i] = (delta[i - 1] + delta[i]) * 0.5f;
                }
            }

            // Fritsch-Carlson monotonicity fix
            for (size_t i = 0; i < n - 1; ++i) {
                if (std::abs(delta[i]) < 1e-8f) {
                    m[i]     = 0.0f;
                    m[i + 1] = 0.0f;
                } else {
                    float alpha = m[i] / delta[i];
                    float beta  = m[i + 1] / delta[i];
                    float r2 = alpha * alpha + beta * beta;
                    if (r2 > 9.0f) {
                        float tau = 3.0f / std::sqrt(r2);
                        m[i]     = tau * alpha * delta[i];
                        m[i + 1] = tau * beta  * delta[i];
                    }
                }
            }
        }
    }

    // Evaluate LUT via cubic Hermite
    for (uint32_t i = 0; i < lut_size; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(lut_size - 1);

        // Clamp to the control point range
        if (x <= sorted.front().x) {
            lut[i] = std::clamp(sorted.front().y, 0.0f, 1.0f);
            continue;
        }
        if (x >= sorted.back().x) {
            lut[i] = std::clamp(sorted.back().y, 0.0f, 1.0f);
            continue;
        }

        // Binary search for the segment
        size_t lo = 0, hi = n - 1;
        while (lo + 1 < hi) {
            size_t mid = (lo + hi) / 2;
            if (sorted[mid].x <= x)
                lo = mid;
            else
                hi = mid;
        }

        float x0 = sorted[lo].x, x1 = sorted[hi].x;
        float y0 = sorted[lo].y, y1 = sorted[hi].y;
        float h  = x1 - x0;

        if (h < 1e-8f) {
            lut[i] = std::clamp(y0, 0.0f, 1.0f);
            continue;
        }

        float t = (x - x0) / h;
        float t2 = t * t;
        float t3 = t2 * t;

        // Hermite basis functions
        float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        float h10 = t3 - 2.0f * t2 + t;
        float h01 = -2.0f * t3 + 3.0f * t2;
        float h11 = t3 - t2;

        float val = h00 * y0 + h10 * h * m[lo] + h01 * y1 + h11 * h * m[hi];
        lut[i] = std::clamp(val, 0.0f, 1.0f);
    }
}

void GPUPipeline::temperatureTintToRGB(float temperature, float tint,
                                        float& r, float& g, float& b)
{
    // Convert color temperature + tint offset to RGB multipliers.
    // Based on a simplified Planckian locus approximation.
    // temperature: in Kelvin (typically 2000-12000, daylight ~5500-6500)
    // tint: green/magenta shift (-100 to +100)
    //
    // At the reference temperature of 5500K, multipliers are (1, 1, 1).
    // Lower temperatures push warmer (more red), higher push cooler (more blue).

    float t = std::clamp(temperature, 1000.0f, 15000.0f);
    float ref = 5500.0f;

    // Simple linear-in-reciprocal-temperature model
    float delta = (1.0f / ref) - (1.0f / t);
    float warm = delta * ref; // positive = warmer (more red)

    r = 1.0f + warm * 0.6f;
    b = 1.0f - warm * 0.6f;

    // Tint adjusts green/magenta axis
    float tint_norm = tint / 100.0f; // normalized to [-1, 1]
    g = 1.0f + tint_norm * 0.3f;

    // Normalize so the brightest channel is 1.0 (exposure compensation is separate)
    float max_ch = std::max({ r, g, b });
    if (max_ch > 0.0f) {
        r /= max_ch;
        g /= max_ch;
        b /= max_ch;
    }
}

} // namespace vega
