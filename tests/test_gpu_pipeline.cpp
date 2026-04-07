#define NOMINMAX
#include <catch2/catch_test_macros.hpp>
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>

#include "core/Logger.h"
#include "core/Timer.h"
#include "gpu/D3D11Context.h"
#include "pipeline/GPUPipeline.h"
#include "pipeline/Pipeline.h"
#include "pipeline/EditRecipe.h"
#include "raw/RawDecoder.h"

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

static const fs::path SAMPLE_CR2 = "samples/_R4C7773.CR2";
static bool hasSample() { return fs::exists(SAMPLE_CR2); }

// Create a minimal hidden window for D3D11
static HWND createHiddenWindow()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"VegaTestGPU";
    RegisterClassExW(&wc);
    return CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 64, 64,
                           nullptr, nullptr, wc.hInstance, nullptr);
}

TEST_CASE("GPUPipeline initializes and compiles all shaders", "[gpu][init]")
{
    vega::Logger::init();
    HWND hwnd = createHiddenWindow();
    REQUIRE(hwnd != nullptr);

    vega::D3D11Context ctx;
    bool ok = ctx.initialize(hwnd, 64, 64);
    if (!ok) {
        DestroyWindow(hwnd);
        SKIP("D3D11 not available");
    }

    vega::GPUPipeline gpu;
    bool init_ok = gpu.initialize(ctx);
    CHECK(init_ok);
    CHECK(gpu.isInitialized());

    ctx.cleanup();
    DestroyWindow(hwnd);
}

TEST_CASE("GPUPipeline processes image and returns valid SRV", "[gpu][process]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }
    vega::Logger::init();

    HWND hwnd = createHiddenWindow();
    REQUIRE(hwnd != nullptr);

    vega::D3D11Context ctx;
    if (!ctx.initialize(hwnd, 64, 64)) {
        DestroyWindow(hwnd);
        SKIP("D3D11 not available");
    }

    vega::GPUPipeline gpu;
    REQUIRE(gpu.initialize(ctx));

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    // Upload raw data
    gpu.uploadRawData(result.value());
    CHECK(gpu.rawWidth() == result.value().width);
    CHECK(gpu.rawHeight() == result.value().height);

    // Process with default recipe
    vega::EditRecipe recipe;
    ID3D11ShaderResourceView* srv = gpu.process(result.value(), recipe);
    CHECK(srv != nullptr);

    INFO("GPU process returned valid SRV for " << result.value().width << "x" << result.value().height);

    ctx.cleanup();
    DestroyWindow(hwnd);
}

TEST_CASE("GPUPipeline with all adjustments active", "[gpu][process][full]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }
    vega::Logger::init();

    HWND hwnd = createHiddenWindow();
    REQUIRE(hwnd != nullptr);

    vega::D3D11Context ctx;
    if (!ctx.initialize(hwnd, 64, 64)) {
        DestroyWindow(hwnd);
        SKIP("D3D11 not available");
    }

    vega::GPUPipeline gpu;
    REQUIRE(gpu.initialize(ctx));

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());
    gpu.uploadRawData(result.value());

    // Recipe with everything turned on
    vega::EditRecipe recipe;
    recipe.exposure = 1.0f;
    recipe.contrast = 20.0f;
    recipe.highlights = -30.0f;
    recipe.shadows = 20.0f;
    recipe.whites = 10.0f;
    recipe.blacks = -10.0f;
    recipe.vibrance = 15.0f;
    recipe.saturation = 10.0f;
    recipe.denoise_luminance = 30.0f;
    recipe.denoise_color = 25.0f;
    recipe.sharpen_amount = 40.0f;

    vega::Timer timer;
    ID3D11ShaderResourceView* srv = gpu.process(result.value(), recipe);
    double ms = timer.elapsed_ms();

    CHECK(srv != nullptr);
    INFO("GPU full pipeline (all nodes): " << ms << "ms");
    // Should be much faster than CPU
    CHECK(ms < 500.0);  // generous limit, expect <50ms on RTX 3090

    ctx.cleanup();
    DestroyWindow(hwnd);
}

TEST_CASE("GPUPipeline performance benchmark", "[gpu][perf]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }
    vega::Logger::init();

    HWND hwnd = createHiddenWindow();
    REQUIRE(hwnd != nullptr);

    vega::D3D11Context ctx;
    if (!ctx.initialize(hwnd, 64, 64)) {
        DestroyWindow(hwnd);
        SKIP("D3D11 not available");
    }

    vega::GPUPipeline gpu;
    REQUIRE(gpu.initialize(ctx));

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());
    gpu.uploadRawData(result.value());

    vega::EditRecipe recipe;
    recipe.exposure = 0.5f;
    recipe.contrast = 10.0f;

    // Warm up
    gpu.process(result.value(), recipe);

    // Benchmark 5 runs
    double total_ms = 0;
    for (int i = 0; i < 5; ++i) {
        recipe.exposure = 0.5f + i * 0.1f;  // vary slightly to prevent caching
        vega::Timer timer;
        ID3D11ShaderResourceView* srv = gpu.process(result.value(), recipe);
        total_ms += timer.elapsed_ms();
        CHECK(srv != nullptr);
    }
    double avg_ms = total_ms / 5.0;

    INFO("GPU avg pipeline time: " << avg_ms << "ms (5 runs)");
    // GPU on RTX 3090 should be well under 100ms per frame
    CHECK(avg_ms < 200.0);

    ctx.cleanup();
    DestroyWindow(hwnd);
}

TEST_CASE("GPU vs CPU output both valid", "[gpu][compare]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }
    vega::Logger::init();

    HWND hwnd = createHiddenWindow();
    REQUIRE(hwnd != nullptr);

    vega::D3D11Context ctx;
    if (!ctx.initialize(hwnd, 64, 64)) {
        DestroyWindow(hwnd);
        SKIP("D3D11 not available");
    }

    vega::GPUPipeline gpu;
    REQUIRE(gpu.initialize(ctx));

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());
    gpu.uploadRawData(result.value());

    vega::EditRecipe recipe;
    recipe.exposure = 1.0f;

    // GPU
    vega::Timer gpu_timer;
    ID3D11ShaderResourceView* gpu_srv = gpu.process(result.value(), recipe);
    double gpu_ms = gpu_timer.elapsed_ms();

    // CPU
    vega::Pipeline cpu_pipeline;
    vega::Timer cpu_timer;
    const auto& cpu_rgba = cpu_pipeline.process(result.value(), recipe);
    double cpu_ms = cpu_timer.elapsed_ms();

    CHECK(gpu_srv != nullptr);
    CHECK(cpu_rgba.size() > 0);

    INFO("GPU: " << gpu_ms << "ms, CPU: " << cpu_ms << "ms, Speedup: " << cpu_ms / gpu_ms << "x");

    ctx.cleanup();
    DestroyWindow(hwnd);
}
