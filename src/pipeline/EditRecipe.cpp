#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <fstream>

namespace vega
{

// ── Helper: ColorSpace <-> string ──────────────────────────────────────────

static const char* colorSpaceToString(EditRecipe::ColorSpace cs)
{
    switch (cs) {
        case EditRecipe::ColorSpace::sRGB:        return "sRGB";
        case EditRecipe::ColorSpace::AdobeRGB:    return "AdobeRGB";
        case EditRecipe::ColorSpace::ProPhotoRGB: return "ProPhotoRGB";
        case EditRecipe::ColorSpace::DisplayP3:   return "DisplayP3";
    }
    return "sRGB";
}

static EditRecipe::ColorSpace stringToColorSpace(const std::string& s)
{
    if (s == "AdobeRGB")    return EditRecipe::ColorSpace::AdobeRGB;
    if (s == "ProPhotoRGB") return EditRecipe::ColorSpace::ProPhotoRGB;
    if (s == "DisplayP3")   return EditRecipe::ColorSpace::DisplayP3;
    return EditRecipe::ColorSpace::sRGB;
}

// ── Helper: CurvePoint vector <-> JSON array ───────────────────────────────

static nlohmann::json curveToJson(const std::vector<CurvePoint>& curve)
{
    auto arr = nlohmann::json::array();
    for (const auto& pt : curve)
        arr.push_back({pt.x, pt.y});
    return arr;
}

static std::vector<CurvePoint> curveFromJson(const nlohmann::json& j,
                                             const std::vector<CurvePoint>& fallback)
{
    if (!j.is_array())
        return fallback;

    std::vector<CurvePoint> result;
    result.reserve(j.size());
    for (const auto& elem : j) {
        if (elem.is_array() && elem.size() >= 2)
            result.push_back({elem[0].get<float>(), elem[1].get<float>()});
    }
    return result.empty() ? fallback : result;
}

// ── Helper: std::array<float,8> <-> JSON array ────────────────────────────

static nlohmann::json arrayToJson(const std::array<float, 8>& a)
{
    auto arr = nlohmann::json::array();
    for (float v : a)
        arr.push_back(v);
    return arr;
}

static std::array<float, 8> arrayFromJson(const nlohmann::json& j,
                                          const std::array<float, 8>& fallback)
{
    if (!j.is_array() || j.size() < 8)
        return fallback;

    std::array<float, 8> result{};
    for (std::size_t i = 0; i < 8; ++i)
        result[i] = j[i].get<float>();
    return result;
}

// ── Helper: safe value extraction with default ─────────────────────────────

template <typename T>
static T val(const nlohmann::json& j, const char* key, T fallback)
{
    if (auto it = j.find(key); it != j.end()) {
        try {
            return it->get<T>();
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

// ── toJson ─────────────────────────────────────────────────────────────────

nlohmann::json EditRecipe::toJson() const
{
    nlohmann::json j;

    // Basic adjustments
    j["exposure"]   = exposure;
    j["contrast"]   = contrast;
    j["highlights"] = highlights;
    j["shadows"]    = shadows;
    j["whites"]     = whites;
    j["blacks"]     = blacks;

    // White balance
    j["wb_temperature"] = wb_temperature;
    j["wb_tint"]        = wb_tint;

    // Tone curves
    j["tone_curve_rgb"] = curveToJson(tone_curve_rgb);
    j["tone_curve_r"]   = curveToJson(tone_curve_r);
    j["tone_curve_g"]   = curveToJson(tone_curve_g);
    j["tone_curve_b"]   = curveToJson(tone_curve_b);

    // HSL
    j["hsl_hue"]        = arrayToJson(hsl_hue);
    j["hsl_saturation"] = arrayToJson(hsl_saturation);
    j["hsl_luminance"]  = arrayToJson(hsl_luminance);

    // Vibrance & Saturation
    j["vibrance"]   = vibrance;
    j["saturation"] = saturation;

    // B&W Mix
    j["bw_mode"] = bw_mode;
    j["bw_mix"]  = arrayToJson(bw_mix);

    // Color Grading
    j["cg_shadows"]    = {{"hue", cg_shadows.hue},    {"sat", cg_shadows.saturation}};
    j["cg_midtones"]   = {{"hue", cg_midtones.hue},   {"sat", cg_midtones.saturation}};
    j["cg_highlights"] = {{"hue", cg_highlights.hue}, {"sat", cg_highlights.saturation}};
    j["cg_blending"]   = cg_blending;
    j["cg_balance"]    = cg_balance;

    // Sharpening
    j["sharpen_amount"]  = sharpen_amount;
    j["sharpen_radius"]  = sharpen_radius;
    j["sharpen_detail"]  = sharpen_detail;
    j["sharpen_masking"] = sharpen_masking;

    // Denoise
    j["denoise_luminance"] = denoise_luminance;
    j["denoise_color"]     = denoise_color;
    j["denoise_detail"]    = denoise_detail;

    // Presence
    j["clarity"] = clarity;
    j["texture"] = texture;
    j["dehaze"]  = dehaze;

    // Crop & Rotation
    j["crop_left"]   = crop_left;
    j["crop_top"]    = crop_top;
    j["crop_right"]  = crop_right;
    j["crop_bottom"] = crop_bottom;
    j["rotation"]    = rotation;

    // Output color space
    j["output_colorspace"] = colorSpaceToString(output_colorspace);

    return j;
}

// ── fromJson ───────────────────────────────────────────────────────────────

EditRecipe EditRecipe::fromJson(const nlohmann::json& j)
{
    EditRecipe r;

    try {
        // Use a default-constructed recipe so any missing key keeps the default
        EditRecipe defaults;

        // Basic adjustments
        r.exposure   = val(j, "exposure",   defaults.exposure);
        r.contrast   = val(j, "contrast",   defaults.contrast);
        r.highlights = val(j, "highlights", defaults.highlights);
        r.shadows    = val(j, "shadows",    defaults.shadows);
        r.whites     = val(j, "whites",     defaults.whites);
        r.blacks     = val(j, "blacks",     defaults.blacks);

        // White balance
        r.wb_temperature = val(j, "wb_temperature", defaults.wb_temperature);
        r.wb_tint        = val(j, "wb_tint",        defaults.wb_tint);

        // Tone curves
        if (j.contains("tone_curve_rgb")) r.tone_curve_rgb = curveFromJson(j["tone_curve_rgb"], defaults.tone_curve_rgb);
        if (j.contains("tone_curve_r"))   r.tone_curve_r   = curveFromJson(j["tone_curve_r"],   defaults.tone_curve_r);
        if (j.contains("tone_curve_g"))   r.tone_curve_g   = curveFromJson(j["tone_curve_g"],   defaults.tone_curve_g);
        if (j.contains("tone_curve_b"))   r.tone_curve_b   = curveFromJson(j["tone_curve_b"],   defaults.tone_curve_b);

        // HSL
        if (j.contains("hsl_hue"))        r.hsl_hue        = arrayFromJson(j["hsl_hue"],        defaults.hsl_hue);
        if (j.contains("hsl_saturation")) r.hsl_saturation = arrayFromJson(j["hsl_saturation"], defaults.hsl_saturation);
        if (j.contains("hsl_luminance"))  r.hsl_luminance  = arrayFromJson(j["hsl_luminance"],  defaults.hsl_luminance);

        // Vibrance & Saturation
        r.vibrance   = val(j, "vibrance",   defaults.vibrance);
        r.saturation = val(j, "saturation", defaults.saturation);

        // B&W Mix
        r.bw_mode = val(j, "bw_mode", defaults.bw_mode);
        if (j.contains("bw_mix")) r.bw_mix = arrayFromJson(j["bw_mix"], defaults.bw_mix);

        // Color Grading
        if (j.contains("cg_shadows") && j["cg_shadows"].is_object()) {
            r.cg_shadows.hue        = val(j["cg_shadows"], "hue", defaults.cg_shadows.hue);
            r.cg_shadows.saturation = val(j["cg_shadows"], "sat", defaults.cg_shadows.saturation);
        }
        if (j.contains("cg_midtones") && j["cg_midtones"].is_object()) {
            r.cg_midtones.hue        = val(j["cg_midtones"], "hue", defaults.cg_midtones.hue);
            r.cg_midtones.saturation = val(j["cg_midtones"], "sat", defaults.cg_midtones.saturation);
        }
        if (j.contains("cg_highlights") && j["cg_highlights"].is_object()) {
            r.cg_highlights.hue        = val(j["cg_highlights"], "hue", defaults.cg_highlights.hue);
            r.cg_highlights.saturation = val(j["cg_highlights"], "sat", defaults.cg_highlights.saturation);
        }
        r.cg_blending = val(j, "cg_blending", defaults.cg_blending);
        r.cg_balance  = val(j, "cg_balance",  defaults.cg_balance);

        // Sharpening
        r.sharpen_amount  = val(j, "sharpen_amount",  defaults.sharpen_amount);
        r.sharpen_radius  = val(j, "sharpen_radius",  defaults.sharpen_radius);
        r.sharpen_detail  = val(j, "sharpen_detail",  defaults.sharpen_detail);
        r.sharpen_masking = val(j, "sharpen_masking", defaults.sharpen_masking);

        // Denoise
        r.denoise_luminance = val(j, "denoise_luminance", defaults.denoise_luminance);
        r.denoise_color     = val(j, "denoise_color",     defaults.denoise_color);
        r.denoise_detail    = val(j, "denoise_detail",    defaults.denoise_detail);

        // Presence
        r.clarity = val(j, "clarity", defaults.clarity);
        r.texture = val(j, "texture", defaults.texture);
        r.dehaze  = val(j, "dehaze",  defaults.dehaze);

        // Crop & Rotation
        r.crop_left   = val(j, "crop_left",   defaults.crop_left);
        r.crop_top    = val(j, "crop_top",     defaults.crop_top);
        r.crop_right  = val(j, "crop_right",   defaults.crop_right);
        r.crop_bottom = val(j, "crop_bottom",  defaults.crop_bottom);
        r.rotation    = val(j, "rotation",     defaults.rotation);

        // Output color space
        r.output_colorspace = stringToColorSpace(
            val<std::string>(j, "output_colorspace", "sRGB"));

    } catch (const std::exception& e) {
        VEGA_LOG_WARN("EditRecipe::fromJson: partial parse error: {}", e.what());
    }

    return r;
}

// ── Sidecar path helper ────────────────────────────────────────────────────

static std::filesystem::path sidecarPath(const std::filesystem::path& raw_path)
{
    auto p = raw_path;
    p.replace_extension(".vgr");
    return p;
}

// ── saveRecipe ─────────────────────────────────────────────────────────────

bool saveRecipe(const std::filesystem::path& raw_path, const EditRecipe& recipe)
{
    try {
        nlohmann::json root;
        root["vega_version"]   = "0.1.0";
        root["recipe_version"] = 1;
        root["recipe"]         = recipe.toJson();

        auto path = sidecarPath(raw_path);
        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            VEGA_LOG_ERROR("saveRecipe: failed to open '{}' for writing",
                           path.string());
            return false;
        }

        ofs << root.dump(4) << '\n';
        if (ofs.fail()) {
            VEGA_LOG_ERROR("saveRecipe: write error to '{}'", path.string());
            return false;
        }

        VEGA_LOG_INFO("saveRecipe: saved '{}'", path.string());
        return true;

    } catch (const std::exception& e) {
        VEGA_LOG_ERROR("saveRecipe: exception: {}", e.what());
        return false;
    }
}

// ── loadRecipe ─────────────────────────────────────────────────────────────

std::optional<EditRecipe> loadRecipe(const std::filesystem::path& raw_path)
{
    try {
        auto path = sidecarPath(raw_path);

        if (!std::filesystem::exists(path))
            return std::nullopt;

        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            VEGA_LOG_ERROR("loadRecipe: failed to open '{}'", path.string());
            return std::nullopt;
        }

        nlohmann::json root = nlohmann::json::parse(ifs);

        if (!root.contains("recipe")) {
            VEGA_LOG_WARN("loadRecipe: '{}' has no 'recipe' key", path.string());
            return std::nullopt;
        }

        VEGA_LOG_INFO("loadRecipe: loaded '{}'", path.string());
        return EditRecipe::fromJson(root["recipe"]);

    } catch (const std::exception& e) {
        VEGA_LOG_ERROR("loadRecipe: exception: {}", e.what());
        return std::nullopt;
    }
}

} // namespace vega
