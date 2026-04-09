#pragma once
#include <array>
#include <vector>
#include <string>
#include <filesystem>
#include <optional>
#include <nlohmann/json.hpp>

namespace vega {

struct CurvePoint {
    float x, y;
    bool operator==(const CurvePoint&) const = default;
};

struct EditRecipe {
    // Basic adjustments
    float exposure = 0, contrast = 0;
    float highlights = 0, shadows = 0, whites = 0, blacks = 0;

    // White balance
    float wb_temperature = 5500.0f, wb_tint = 0.0f;

    // Tone curves (RGB master + per-channel)
    std::vector<CurvePoint> tone_curve_rgb = {{0,0},{1,1}};
    std::vector<CurvePoint> tone_curve_r   = {{0,0},{1,1}};
    std::vector<CurvePoint> tone_curve_g   = {{0,0},{1,1}};
    std::vector<CurvePoint> tone_curve_b   = {{0,0},{1,1}};

    // HSL (8 channels: R, O, Y, G, Aqua, B, Purple, Magenta)
    std::array<float, 8> hsl_hue = {};
    std::array<float, 8> hsl_saturation = {};
    std::array<float, 8> hsl_luminance = {};

    // Vibrance & Saturation
    float vibrance = 0, saturation = 0;

    // B&W Mix
    bool bw_mode = false;
    std::array<float, 8> bw_mix = {};

    // Color Grading
    struct ColorWheel {
        float hue = 0;
        float saturation = 0;
        bool operator==(const ColorWheel&) const = default;
    };
    ColorWheel cg_shadows, cg_midtones, cg_highlights;
    float cg_blending = 50;  // 0-100
    float cg_balance  = 0;   // -100 to +100

    // Sharpening
    float sharpen_amount = 0, sharpen_radius = 1.0f;
    float sharpen_detail = 25, sharpen_masking = 0;

    // Denoise
    float denoise_luminance = 0, denoise_color = 0, denoise_detail = 50;

    // Presence
    float clarity = 0;   // -100 to 100
    float texture = 0;   // -100 to 100
    float dehaze  = 0;   // -100 to 100

    // Crop & Rotation
    float crop_left = 0, crop_top = 0, crop_right = 1, crop_bottom = 1;
    float rotation = 0;

    // Effects (applied in sRGB space, after gamma)
    float vignette_amount   = 0;    // -100 to 100
    float vignette_midpoint = 50;   // 0 to 100
    float vignette_roundness = 0;   // -100 to 100
    float vignette_feather  = 50;   // 0 to 100
    float grain_amount      = 0;    // 0 to 100
    float grain_size        = 25;   // 1 to 100
    float grain_roughness   = 50;   // 0 to 100

    // Output color space
    enum class ColorSpace { sRGB, AdobeRGB, ProPhotoRGB, DisplayP3 };
    ColorSpace output_colorspace = ColorSpace::sRGB;

    // Serialization
    nlohmann::json toJson() const;
    static EditRecipe fromJson(const nlohmann::json& j);

    // Default comparison
    bool operator==(const EditRecipe& o) const = default;
};

// .vgr sidecar file read/write
bool saveRecipe(const std::filesystem::path& raw_path, const EditRecipe& recipe);
std::optional<EditRecipe> loadRecipe(const std::filesystem::path& raw_path);

} // namespace vega
