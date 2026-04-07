#define NOMINMAX
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>

#include "core/Logger.h"
#include "core/Timer.h"
#include "raw/RawDecoder.h"
#include "raw/RawImage.h"
#include "raw/ExifReader.h"
#include "pipeline/SimplePipeline.h"
#include "pipeline/Pipeline.h"
#include "pipeline/EditRecipe.h"
#include "pipeline/EditHistory.h"

namespace fs = std::filesystem;

static const fs::path SAMPLE_CR2 = "samples/_R4C7773.CR2";

static bool hasSample()
{
    return fs::exists(SAMPLE_CR2);
}

// ---------- RawDecoder ----------

TEST_CASE("RawDecoder decodes CR2 to valid RawImage", "[raw][decode]")
{
    if (!hasSample()) { SKIP("Sample RAW not found at " + SAMPLE_CR2.string()); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    auto& img = result.value();
    REQUIRE(img.width > 0);
    REQUIRE(img.height > 0);
    REQUIRE(img.bayer_data.size() == static_cast<size_t>(img.width) * img.height);

    // Bayer data should be normalized to [0, 1]
    float min_val = 1.0f, max_val = 0.0f;
    for (size_t i = 0; i < std::min<size_t>(img.bayer_data.size(), 100000); ++i)
    {
        float v = img.bayer_data[i];
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }
    CHECK(min_val >= 0.0f);
    CHECK(max_val <= 1.0f);
    CHECK(max_val > 0.001f);  // not all black (some RAWs have low normalized values)

    // WB multipliers should be positive
    for (int i = 0; i < 4; ++i)
        CHECK(img.wb_multipliers[i] > 0.0f);

    INFO("Decoded: " << img.width << "x" << img.height
         << " bayer_pattern=" << img.metadata.bayer_pattern);
}

TEST_CASE("RawDecoder readMetadata without full decode", "[raw][metadata]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::readMetadata(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    auto& meta = result.value();
    CHECK(!meta.camera_make.empty());
    CHECK(!meta.camera_model.empty());
    CHECK(meta.iso_speed > 0);

    INFO("Camera: " << meta.camera_make << " " << meta.camera_model
         << " ISO:" << meta.iso_speed);
}

TEST_CASE("RawDecoder extractThumbnail returns JPEG data", "[raw][thumbnail]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::extractThumbnail(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    auto& thumb = result.value();
    REQUIRE(thumb.size() > 100);

    // JPEG starts with FF D8 FF
    CHECK(thumb[0] == 0xFF);
    CHECK(thumb[1] == 0xD8);
}

// ---------- ExifReader ----------

TEST_CASE("ExifReader enriches metadata", "[raw][exif]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    vega::RawImageMetadata meta;
    vega::ExifReader::enrichMetadata(SAMPLE_CR2, meta);

    CHECK(!meta.camera_make.empty());
    CHECK(!meta.camera_model.empty());
    // At least some EXIF fields should be populated
    bool has_any = meta.iso_speed > 0 || meta.aperture > 0 || !meta.datetime_original.empty();
    CHECK(has_any);

    INFO("EXIF: " << meta.camera_make << " " << meta.camera_model
         << " ISO:" << meta.iso_speed << " f/" << meta.aperture);
}

// ---------- SimplePipeline ----------

TEST_CASE("SimplePipeline produces valid RGBA8 output", "[pipeline][simple]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    vega::Timer timer;
    auto rgba = vega::SimplePipeline::process(result.value());
    double ms = timer.elapsed_ms();

    uint32_t w = result.value().width;
    uint32_t h = result.value().height;

    REQUIRE(rgba.size() == static_cast<size_t>(w) * h * 4);

    // Check not all black or all white
    size_t black_pixels = 0, white_pixels = 0;
    for (size_t i = 0; i < rgba.size(); i += 4)
    {
        if (rgba[i] == 0 && rgba[i+1] == 0 && rgba[i+2] == 0)
            black_pixels++;
        if (rgba[i] == 255 && rgba[i+1] == 255 && rgba[i+2] == 255)
            white_pixels++;
    }
    size_t total = static_cast<size_t>(w) * h;
    CHECK(black_pixels < total / 2);  // not mostly black
    CHECK(white_pixels < total / 2);  // not mostly white

    // Alpha should all be 255
    for (size_t i = 3; i < std::min<size_t>(rgba.size(), 4000); i += 4)
        CHECK(rgba[i] == 255);

    INFO("SimplePipeline: " << w << "x" << h << " in " << ms << "ms");
}

// ---------- Pipeline (node-based) ----------

TEST_CASE("Pipeline produces valid output with default recipe", "[pipeline][nodes]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    vega::Pipeline pipeline;
    vega::EditRecipe recipe;

    vega::Timer timer;
    auto rgba = pipeline.process(result.value(), recipe);
    double ms = timer.elapsed_ms();

    uint32_t w = result.value().width;
    uint32_t h = result.value().height;

    REQUIRE(rgba.size() == static_cast<size_t>(w) * h * 4);

    // Sanity: not all black
    uint64_t sum = 0;
    for (size_t i = 0; i < std::min<size_t>(rgba.size(), 40000); i += 4)
        sum += rgba[i] + rgba[i+1] + rgba[i+2];
    CHECK(sum > 0);

    INFO("Pipeline (default recipe): " << w << "x" << h << " in " << ms << "ms");
}

TEST_CASE("Pipeline respects exposure adjustment", "[pipeline][exposure]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    vega::Pipeline pipeline;

    // Default exposure
    vega::EditRecipe recipe_default;
    auto rgba_default = pipeline.process(result.value(), recipe_default);

    // +2 EV exposure
    vega::EditRecipe recipe_bright;
    recipe_bright.exposure = 2.0f;
    auto rgba_bright = pipeline.process(result.value(), recipe_bright);

    REQUIRE(rgba_default.size() == rgba_bright.size());

    // Brighter image should have higher average pixel value
    uint64_t sum_default = 0, sum_bright = 0;
    size_t sample_count = std::min<size_t>(rgba_default.size() / 4, 50000);
    for (size_t i = 0; i < sample_count; ++i)
    {
        size_t idx = i * 4;
        sum_default += rgba_default[idx] + rgba_default[idx+1] + rgba_default[idx+2];
        sum_bright  += rgba_bright[idx]  + rgba_bright[idx+1]  + rgba_bright[idx+2];
    }

    CHECK(sum_bright > sum_default);

    INFO("Default avg: " << (sum_default / sample_count / 3)
         << " Bright avg: " << (sum_bright / sample_count / 3));
}

// ---------- EditRecipe serialization ----------

TEST_CASE("EditRecipe JSON round-trip", "[recipe][json]")
{
    vega::EditRecipe original;
    original.exposure = 1.5f;
    original.contrast = -20.0f;
    original.wb_temperature = 6500.0f;
    original.wb_tint = 10.0f;
    original.highlights = -50.0f;
    original.shadows = 30.0f;
    original.hsl_hue[0] = 15.0f;
    original.hsl_saturation[2] = -30.0f;
    original.vibrance = 25.0f;
    original.sharpen_amount = 40.0f;
    original.tone_curve_rgb = {{0, 0}, {0.25f, 0.20f}, {0.75f, 0.82f}, {1, 1}};

    auto json = original.toJson();
    auto restored = vega::EditRecipe::fromJson(json);

    CHECK(restored.exposure == original.exposure);
    CHECK(restored.contrast == original.contrast);
    CHECK(restored.wb_temperature == original.wb_temperature);
    CHECK(restored.wb_tint == original.wb_tint);
    CHECK(restored.highlights == original.highlights);
    CHECK(restored.shadows == original.shadows);
    CHECK(restored.hsl_hue[0] == original.hsl_hue[0]);
    CHECK(restored.hsl_saturation[2] == original.hsl_saturation[2]);
    CHECK(restored.vibrance == original.vibrance);
    CHECK(restored.sharpen_amount == original.sharpen_amount);
    CHECK(restored.tone_curve_rgb.size() == original.tone_curve_rgb.size());
    CHECK(restored == original);
}

TEST_CASE("EditRecipe .vgr sidecar save/load round-trip", "[recipe][sidecar]")
{
    auto test_dir = fs::temp_directory_path() / "vega_test";
    fs::create_directories(test_dir);
    auto fake_raw = test_dir / "test.cr2";

    // Create a dummy file so sidecar path resolves
    { std::ofstream f(fake_raw); f << "x"; }

    vega::EditRecipe recipe;
    recipe.exposure = 2.5f;
    recipe.wb_temperature = 7000.0f;
    recipe.saturation = -15.0f;

    bool saved = vega::saveRecipe(fake_raw, recipe);
    REQUIRE(saved);

    auto vgr_path = test_dir / "test.vgr";
    CHECK(fs::exists(vgr_path));

    auto loaded = vega::loadRecipe(fake_raw);
    REQUIRE(loaded.has_value());
    CHECK(loaded->exposure == recipe.exposure);
    CHECK(loaded->wb_temperature == recipe.wb_temperature);
    CHECK(loaded->saturation == recipe.saturation);

    // Cleanup
    fs::remove_all(test_dir);
}

// ---------- EditHistory ----------

TEST_CASE("EditHistory undo/redo", "[history]")
{
    vega::EditHistory history;

    CHECK_FALSE(history.canUndo());
    CHECK_FALSE(history.canRedo());

    vega::EditRecipe r0, r1, r2;
    r1.exposure = 1.0f;
    r2.exposure = 2.0f;

    vega::EditCommand cmd1;
    cmd1.description = "Exposure +1";
    cmd1.before = r0;
    cmd1.after = r1;
    cmd1.affected_stage = vega::PipelineStage::Exposure;
    history.push(cmd1);

    CHECK(history.canUndo());
    CHECK_FALSE(history.canRedo());

    vega::EditCommand cmd2;
    cmd2.description = "Exposure +2";
    cmd2.before = r1;
    cmd2.after = r2;
    cmd2.affected_stage = vega::PipelineStage::Exposure;
    history.push(cmd2);

    CHECK(history.totalEntries() == 2);

    // Undo once -> back to r1
    const auto& undone = history.undo();
    CHECK(undone.exposure == r1.exposure);
    CHECK(history.canUndo());
    CHECK(history.canRedo());

    // Undo again -> back to r0
    const auto& undone2 = history.undo();
    CHECK(undone2.exposure == r0.exposure);
    CHECK_FALSE(history.canUndo());
    CHECK(history.canRedo());

    // Redo -> r1
    const auto& redone = history.redo();
    CHECK(redone.exposure == r1.exposure);
}

// ---------- Decode error handling ----------

TEST_CASE("RawDecoder returns error for nonexistent file", "[raw][error]")
{
    vega::Logger::init();

    auto result = vega::RawDecoder::decode("nonexistent_file.cr2");
    REQUIRE(result.is_err());
}

TEST_CASE("RawDecoder returns error for invalid file", "[raw][error]")
{
    vega::Logger::init();

    auto test_file = fs::temp_directory_path() / "vega_test_invalid.cr2";
    { std::ofstream f(test_file); f << "not a real raw file"; }

    auto result = vega::RawDecoder::decode(test_file);
    CHECK(result.is_err());

    fs::remove(test_file);
}

// ---------- Preview pipeline ----------

TEST_CASE("processPreview produces smaller valid output", "[pipeline][preview]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    vega::Pipeline pipeline;
    vega::EditRecipe recipe;
    recipe.exposure = 0.5f;

    uint32_t pw = 0, ph = 0;
    const auto& rgba = pipeline.processPreview(result.value(), recipe, 4, pw, ph);

    uint32_t expected_w = result.value().width / 4;
    uint32_t expected_h = result.value().height / 4;

    CHECK(pw == expected_w);
    CHECK(ph == expected_h);
    REQUIRE(rgba.size() == static_cast<size_t>(pw) * ph * 4);

    // Not all black
    uint64_t sum = 0;
    for (size_t i = 0; i < std::min<size_t>(rgba.size(), 4000); i += 4)
        sum += rgba[i] + rgba[i+1] + rgba[i+2];
    CHECK(sum > 0);

    INFO("Preview: " << pw << "x" << ph);
}

TEST_CASE("processPreview is faster than full process", "[pipeline][preview][perf]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    vega::Pipeline pipeline;
    vega::EditRecipe recipe;
    recipe.exposure = 1.0f;
    recipe.contrast = 20.0f;

    // Warm up demosaic cache
    pipeline.process(result.value(), recipe);

    // Benchmark full
    vega::Timer t_full;
    pipeline.process(result.value(), recipe);
    double ms_full = t_full.elapsed_ms();

    // Benchmark preview
    uint32_t pw, ph;
    vega::Timer t_prev;
    pipeline.processPreview(result.value(), recipe, 4, pw, ph);
    double ms_prev = t_prev.elapsed_ms();

    CHECK(ms_prev < ms_full);

    INFO("Full: " << ms_full << "ms, Preview 1/4: " << ms_prev << "ms, Speedup: " << ms_full / ms_prev << "x");
}

TEST_CASE("Two Pipeline instances produce same output", "[pipeline][thread-safety]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    vega::EditRecipe recipe;
    recipe.exposure = 1.5f;
    recipe.highlights = -30.0f;

    // Simulate main thread + background thread using separate Pipeline instances
    vega::Pipeline pipeline_a;
    vega::Pipeline pipeline_b;

    const auto& rgba_a = pipeline_a.process(result.value(), recipe);
    const auto& rgba_b = pipeline_b.process(result.value(), recipe);

    REQUIRE(rgba_a.size() == rgba_b.size());
    REQUIRE(rgba_a.size() > 0);

    // Compare first 10000 pixels — should be identical
    size_t mismatches = 0;
    size_t check_count = std::min<size_t>(rgba_a.size(), 40000);
    for (size_t i = 0; i < check_count; ++i) {
        if (rgba_a[i] != rgba_b[i])
            mismatches++;
    }

    CHECK(mismatches == 0);
    INFO("Checked " << check_count << " bytes, mismatches: " << mismatches);
}

TEST_CASE("Background thread pipeline concurrent with preview", "[pipeline][thread-safety]")
{
    if (!hasSample()) { SKIP("Sample RAW not found"); }

    vega::Logger::init();

    auto result = vega::RawDecoder::decode(SAMPLE_CR2);
    REQUIRE(result.is_ok());

    vega::EditRecipe recipe;
    recipe.exposure = 1.0f;

    vega::Pipeline preview_pipeline;
    vega::Pipeline bg_pipeline;

    // Run both concurrently (simulates the actual usage in main.cpp)
    std::vector<uint8_t> bg_result;
    uint32_t bg_w = 0, bg_h = 0;
    bool bg_done = false;

    std::thread bg_thread([&]() {
        const auto& rgba = bg_pipeline.process(result.value(), recipe);
        bg_result.assign(rgba.begin(), rgba.end());
        bg_w = result.value().width;
        bg_h = result.value().height;
        bg_done = true;
    });

    // Meanwhile, run preview on "main thread"
    uint32_t pw, ph;
    const auto& preview = preview_pipeline.processPreview(result.value(), recipe, 4, pw, ph);

    CHECK(preview.size() > 0);
    CHECK(pw == result.value().width / 4);

    bg_thread.join();

    CHECK(bg_done);
    CHECK(bg_result.size() == static_cast<size_t>(bg_w) * bg_h * 4);

    // Both outputs should be valid (not all zeros)
    uint64_t sum_preview = 0, sum_full = 0;
    for (size_t i = 0; i < std::min<size_t>(preview.size(), 4000); i += 4)
        sum_preview += preview[i];
    for (size_t i = 0; i < std::min<size_t>(bg_result.size(), 4000); i += 4)
        sum_full += bg_result[i];

    CHECK(sum_preview > 0);
    CHECK(sum_full > 0);

    INFO("Preview " << pw << "x" << ph << " OK, Full " << bg_w << "x" << bg_h << " OK");
}
