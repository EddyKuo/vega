#pragma once
#include "pipeline/EditRecipe.h"
#include "pipeline/EditHistory.h"
#include <functional>

namespace vega {

class DevelopPanel {
public:
    // Render the develop panel. Returns true if recipe was changed.
    bool render(EditRecipe& recipe, EditHistory& history);

    // Called after a RAW file is opened to store the camera's As Shot WB.
    void setAsShotWB(float temp, float tint) { as_shot_temperature_ = temp; as_shot_tint_ = tint; }

    // Image dimensions for crop aspect ratio calculations
    void setImageDimensions(uint32_t w, uint32_t h) { img_width_ = w; img_height_ = h; }

    // Set to true by the Auto button in the Tone section; consumed by main.cpp each frame.
    bool auto_tone_requested = false;

private:
    float as_shot_temperature_ = 5500.0f;
    float as_shot_tint_        = 0.0f;
    uint32_t img_width_ = 1, img_height_ = 1;
    // Track drag state for progressive preview
    bool is_dragging_ = false;

    // Snapshot of recipe before a drag begins, used for undo grouping
    EditRecipe drag_start_recipe_{};

    // Crop aspect ratio preset index: 0=Free, 1=1:1, 2=4:3, 3=3:2, 4=16:9
    int crop_ratio_idx_ = 0;

    // HSL tab selection: 0=Hue, 1=Saturation, 2=Luminance
    int hsl_tab_ = 0;

    // Tone curve channel: 0=RGB, 1=R, 2=G, 3=B
    int curve_channel_ = 0;

    // Tone curve interaction state
    int dragged_point_index_ = -1;

    // Custom slider that supports: double-click reset, right-click precision input
    // Returns true if value changed
    bool vegaSlider(const char* label, float* value, float min_val, float max_val,
                    float default_val = 0.0f, const char* format = "%.1f");

    // Section rendering — each returns true if a value changed
    bool renderCrop(EditRecipe& recipe);
    bool renderWhiteBalance(EditRecipe& recipe);
    bool renderTone(EditRecipe& recipe);
    bool renderPresence(EditRecipe& recipe);
    bool renderToneCurve(EditRecipe& recipe);
    bool renderHSL(EditRecipe& recipe);
    bool renderColorGrading(EditRecipe& recipe);
    bool renderDetail(EditRecipe& recipe);
    bool renderEffects(EditRecipe& recipe);

    // Helper: get the active curve points vector for the current channel
    std::vector<CurvePoint>& getActiveCurve(EditRecipe& recipe);
};

} // namespace vega
