#define NOMINMAX
#include "pipeline/PreviewManager.h"
#include "core/Logger.h"
#include "core/Timer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace vega {

// ── Public ──────────────────────────────────────────────────────────────────

void PreviewManager::initialize(D3D11Context& ctx)
{
    ctx_ = &ctx;

    // Try to initialize the GPU pipeline
    gpu_available_ = gpu_pipeline_.initialize(ctx);
    if (gpu_available_) {
        VEGA_LOG_INFO("PreviewManager: GPU pipeline initialized");
    } else {
        VEGA_LOG_WARN("PreviewManager: GPU pipeline unavailable, using CPU fallback");
    }
}

void PreviewManager::setImage(const RawImage& raw)
{
    current_raw_ = &raw;
    has_image_ = !raw.bayer_data.empty() && raw.width > 0 && raw.height > 0;
    raw_uploaded_ = false;

    if (!has_image_) {
        VEGA_LOG_WARN("PreviewManager: setImage called with empty raw image");
        return;
    }

    // Upload to GPU if available
    if (gpu_available_) {
        gpu_pipeline_.uploadRawData(raw);
        raw_uploaded_ = true;
    }

    // Clear stale preview
    current_srv_ = nullptr;
    current_rgba_.clear();
    current_w_ = 0;
    current_h_ = 0;

    VEGA_LOG_INFO("PreviewManager: image set ({}x{})", raw.width, raw.height);
}

void PreviewManager::onRecipeChanged(const RawImage& raw, const EditRecipe& recipe)
{
    // If the raw pointer changed, update our reference
    if (current_raw_ != &raw) {
        setImage(raw);
    }

    pending_recipe_ = recipe;
    last_change_ = Clock::now();
    state_ = PreviewState::Fast;
}

ID3D11ShaderResourceView* PreviewManager::update(std::vector<uint8_t>& rgba_for_histogram,
                                                   uint32_t& hist_width,
                                                   uint32_t& hist_height)
{
    if (!has_image_ || !current_raw_) {
        rgba_for_histogram.clear();
        hist_width = hist_height = 0;
        return nullptr;
    }

    if (state_ == PreviewState::Idle) {
        // No pending changes -- return whatever we have cached
        rgba_for_histogram = current_rgba_;
        hist_width  = current_w_;
        hist_height = current_h_;
        return current_srv_;
    }

    // Determine how much time has passed since the last recipe change
    auto now = Clock::now();
    int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_change_).count();

    // Determine which tier to render
    PreviewState target_state = state_;
    if (elapsed_ms >= FULL_THRESHOLD_MS && state_ != PreviewState::Idle) {
        target_state = PreviewState::Full;
    } else if (elapsed_ms >= MEDIUM_THRESHOLD_MS && state_ == PreviewState::Fast) {
        target_state = PreviewState::Medium;
    }

    // Only render if we're stepping up in quality
    if (target_state <= state_ && target_state != PreviewState::Full) {
        // Already rendered this tier; wait for the next threshold
        rgba_for_histogram = current_rgba_;
        hist_width  = current_w_;
        hist_height = current_h_;
        return current_srv_;
    }

    // Select scale factor
    float scale = FAST_SCALE;
    switch (target_state) {
        case PreviewState::Fast:   scale = FAST_SCALE;   break;
        case PreviewState::Medium: scale = MEDIUM_SCALE; break;
        case PreviewState::Full:   scale = FULL_SCALE;   break;
        default: break;
    }

    Timer timer;

    // ── Render ──
    if (gpu_available_ && raw_uploaded_) {
        ID3D11ShaderResourceView* srv = renderGPU(*current_raw_, pending_recipe_, scale);
        if (srv) {
            current_srv_ = srv;

            // Read back a low-res version for histogram computation
            // Use the fast-scale version to keep readback cheap
            float hist_scale = std::min(scale, MEDIUM_SCALE);
            if (scale <= hist_scale) {
                // The current render is already small enough
                readbackForHistogram(srv, current_rgba_, current_w_, current_h_);
            } else {
                // Render a separate small version for histogram
                ID3D11ShaderResourceView* hist_srv =
                    renderGPU(*current_raw_, pending_recipe_, hist_scale);
                if (hist_srv) {
                    readbackForHistogram(hist_srv, current_rgba_, current_w_, current_h_);
                }
            }
        } else {
            // GPU render failed -- fall back to CPU
            VEGA_LOG_WARN("PreviewManager: GPU render failed, falling back to CPU");
            gpu_available_ = false;
            renderCPU(*current_raw_, pending_recipe_);
            uploadCPUResultToGPU();
            current_srv_ = cpu_display_srv_.Get();
        }
    } else {
        // CPU path
        renderCPU(*current_raw_, pending_recipe_);
        uploadCPUResultToGPU();
        current_srv_ = cpu_display_srv_.Get();
    }

    VEGA_LOG_DEBUG("PreviewManager: rendered {} preview ({:.0f}%): {:.1f}ms",
                   target_state == PreviewState::Fast   ? "fast" :
                   target_state == PreviewState::Medium  ? "medium" : "full",
                   scale * 100.0f, timer.elapsed_ms());

    // Advance state
    if (target_state == PreviewState::Full) {
        state_ = PreviewState::Idle;
    } else {
        state_ = target_state;
    }

    rgba_for_histogram = current_rgba_;
    hist_width  = current_w_;
    hist_height = current_h_;
    return current_srv_;
}

void PreviewManager::invalidate()
{
    state_ = PreviewState::Fast;
    last_change_ = Clock::now();
}

// ── Private helpers ─────────────────────────────────────────────────────────

ID3D11ShaderResourceView* PreviewManager::renderGPU(const RawImage& raw,
                                                      const EditRecipe& recipe,
                                                      float scale)
{
    return gpu_pipeline_.process(raw, recipe, scale);
}

void PreviewManager::renderCPU(const RawImage& raw, const EditRecipe& recipe)
{
    Timer timer;

    current_rgba_ = cpu_pipeline_.process(raw, recipe);
    current_w_ = raw.width;
    current_h_ = raw.height;

    VEGA_LOG_DEBUG("PreviewManager: CPU render ({}x{}): {:.1f}ms",
                   current_w_, current_h_, timer.elapsed_ms());
}

void PreviewManager::uploadCPUResultToGPU()
{
    if (!ctx_ || current_rgba_.empty() || current_w_ == 0 || current_h_ == 0)
        return;

    ID3D11Device* device = ctx_->device();
    if (!device)
        return;

    // Recreate texture if dimensions changed
    if (cpu_display_w_ != current_w_ || cpu_display_h_ != current_h_) {
        cpu_display_tex_.Reset();
        cpu_display_srv_.Reset();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width      = current_w_;
        desc.Height     = current_h_;
        desc.MipLevels  = 1;
        desc.ArraySize  = 1;
        desc.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc = { 1, 0 };
        desc.Usage      = D3D11_USAGE_DEFAULT;
        desc.BindFlags  = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem     = current_rgba_.data();
        init.SysMemPitch = current_w_ * 4;

        HRESULT hr = device->CreateTexture2D(&desc, &init, &cpu_display_tex_);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("PreviewManager: failed to create CPU display texture: 0x{:08X}",
                           static_cast<uint32_t>(hr));
            return;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels       = 1;

        hr = device->CreateShaderResourceView(cpu_display_tex_.Get(), &srv_desc,
                                               &cpu_display_srv_);
        if (FAILED(hr)) {
            VEGA_LOG_ERROR("PreviewManager: failed to create CPU display SRV: 0x{:08X}",
                           static_cast<uint32_t>(hr));
            cpu_display_tex_.Reset();
            return;
        }

        cpu_display_w_ = current_w_;
        cpu_display_h_ = current_h_;
    } else {
        // Same dimensions -- just update the texture contents
        ctx_->context()->UpdateSubresource(
            cpu_display_tex_.Get(), 0, nullptr,
            current_rgba_.data(), current_w_ * 4, 0);
    }
}

void PreviewManager::readbackForHistogram(ID3D11ShaderResourceView* srv,
                                           std::vector<uint8_t>& out_rgba,
                                           uint32_t& out_w, uint32_t& out_h)
{
    if (!ctx_ || !srv) {
        out_rgba.clear();
        out_w = out_h = 0;
        return;
    }

    ID3D11DeviceContext* dc = ctx_->context();

    // Get the texture resource from the SRV
    ComPtr<ID3D11Resource> resource;
    srv->GetResource(&resource);

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = resource.As(&texture);
    if (FAILED(hr)) {
        out_rgba.clear();
        out_w = out_h = 0;
        return;
    }

    D3D11_TEXTURE2D_DESC tex_desc{};
    texture->GetDesc(&tex_desc);

    out_w = tex_desc.Width;
    out_h = tex_desc.Height;

    // Create a staging texture for CPU readback
    D3D11_TEXTURE2D_DESC staging_desc = tex_desc;
    staging_desc.Usage          = D3D11_USAGE_STAGING;
    staging_desc.BindFlags      = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags      = 0;

    ComPtr<ID3D11Texture2D> staging;
    hr = ctx_->device()->CreateTexture2D(&staging_desc, nullptr, &staging);
    if (FAILED(hr)) {
        VEGA_LOG_WARN("PreviewManager: failed to create staging texture for readback");
        out_rgba.clear();
        out_w = out_h = 0;
        return;
    }

    dc->CopyResource(staging.Get(), texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = dc->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        VEGA_LOG_WARN("PreviewManager: failed to map staging texture for readback");
        out_rgba.clear();
        out_w = out_h = 0;
        return;
    }

    uint32_t pixel_count = out_w * out_h;

    // The source texture is R32G32B32A32_FLOAT -- convert to RGBA8
    if (tex_desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT) {
        out_rgba.resize(static_cast<size_t>(pixel_count) * 4);
        const uint8_t* src_row = static_cast<const uint8_t*>(mapped.pData);

        for (uint32_t y = 0; y < out_h; ++y) {
            const float* src = reinterpret_cast<const float*>(src_row + y * mapped.RowPitch);
            uint8_t* dst = out_rgba.data() + y * out_w * 4;

            for (uint32_t x = 0; x < out_w; ++x) {
                float r = std::clamp(src[x * 4 + 0], 0.0f, 1.0f);
                float g = std::clamp(src[x * 4 + 1], 0.0f, 1.0f);
                float b = std::clamp(src[x * 4 + 2], 0.0f, 1.0f);

                dst[x * 4 + 0] = static_cast<uint8_t>(r * 255.0f + 0.5f);
                dst[x * 4 + 1] = static_cast<uint8_t>(g * 255.0f + 0.5f);
                dst[x * 4 + 2] = static_cast<uint8_t>(b * 255.0f + 0.5f);
                dst[x * 4 + 3] = 255;
            }
        }
    } else if (tex_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        // Already RGBA8 -- direct copy
        out_rgba.resize(static_cast<size_t>(pixel_count) * 4);
        const uint8_t* src_row = static_cast<const uint8_t*>(mapped.pData);

        for (uint32_t y = 0; y < out_h; ++y) {
            std::memcpy(out_rgba.data() + y * out_w * 4,
                        src_row + y * mapped.RowPitch,
                        out_w * 4);
        }
    } else {
        VEGA_LOG_WARN("PreviewManager: unsupported texture format for readback: {}",
                       static_cast<uint32_t>(tex_desc.Format));
        out_rgba.clear();
        out_w = out_h = 0;
    }

    dc->Unmap(staging.Get(), 0);
}

} // namespace vega
