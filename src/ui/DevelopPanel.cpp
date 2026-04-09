#define NOMINMAX
#include "ui/DevelopPanel.h"
#include "core/Logger.h"
#include "core/i18n.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace vega {

// ─────────────────────────────────────────────────────────────────────
// vegaSlider — custom slider with double-click reset & right-click input
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::vegaSlider(const char* label, float* value, float min_val, float max_val,
                              float default_val, const char* format)
{
    float old_value = *value;

    // PushID with raw English label ensures stable widget ID across languages
    ImGui::PushID(label);

    const char* display_text = tr(label);
    bool changed = ImGui::SliderFloat(display_text, value, min_val, max_val, format);

    // Double-click: reset to default
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        *value = default_val;
        changed = (*value != old_value);
    }

    // Right-click: precision input popup
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("##precision_input");
    }

    if (ImGui::BeginPopup("##precision_input"))
    {
        ImGui::Text("%s", display_text);
        ImGui::Separator();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputFloat("##val", value, 0.1f, 1.0f, format))
        {
            *value = std::clamp(*value, min_val, max_val);
            changed = true;
        }
        if (ImGui::Button("Reset"))
        {
            *value = default_val;
            changed = (*value != old_value);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// Helper: get the active curve for the current channel
// ─────────────────────────────────────────────────────────────────────

std::vector<CurvePoint>& DevelopPanel::getActiveCurve(EditRecipe& recipe)
{
    switch (curve_channel_)
    {
    case 1: return recipe.tone_curve_r;
    case 2: return recipe.tone_curve_g;
    case 3: return recipe.tone_curve_b;
    default: return recipe.tone_curve_rgb;
    }
}

// ─────────────────────────────────────────────────────────────────────
// render() — main entry point
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::render(EditRecipe& recipe, EditHistory& history)
{
    bool any_changed = false;
    EditRecipe before = recipe;

    // Track drag grouping: when user starts dragging a slider, capture the
    // recipe state.  When they release, push a single undo entry.
    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    if (!is_dragging_ && mouse_down && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
    {
        // Will be confirmed as a drag only if a value actually changes below
    }

    // Section renderers
    bool wb_changed       = renderWhiteBalance(recipe);
    ImGui::Separator();
    bool tone_changed     = renderTone(recipe);
    ImGui::Separator();
    bool presence_changed = renderPresence(recipe);
    ImGui::Separator();
    bool curve_changed    = renderToneCurve(recipe);
    ImGui::Separator();
    bool hsl_changed      = renderHSL(recipe);
    ImGui::Separator();
    bool cg_changed       = renderColorGrading(recipe);
    ImGui::Separator();
    bool detail_changed   = renderDetail(recipe);
    ImGui::Separator();
    bool fx_changed       = renderEffects(recipe);

    any_changed = wb_changed || tone_changed || presence_changed || curve_changed ||
                  hsl_changed || cg_changed || detail_changed || fx_changed;

    if (any_changed)
    {
        VEGA_LOG_TRACE("DevelopPanel: changed [WB:{} Tone:{} Presence:{} Curve:{} HSL:{} CG:{} Detail:{} FX:{}]",
            wb_changed, tone_changed, presence_changed, curve_changed, hsl_changed, cg_changed, detail_changed, fx_changed);
    }

    // Drag-based undo grouping
    if (any_changed && !is_dragging_)
    {
        is_dragging_ = true;
        drag_start_recipe_ = before;
    }

    if (is_dragging_ && !mouse_down)
    {
        // Drag ended — push a single history entry
        is_dragging_ = false;
        if (!(drag_start_recipe_ == recipe))
        {
            // Determine a description based on what changed
            std::string desc = "Adjustment";
            if (wb_changed)            desc = "White Balance";
            else if (tone_changed)     desc = "Tone";
            else if (presence_changed) desc = "Presence";
            else if (curve_changed)    desc = "Tone Curve";
            else if (hsl_changed)      desc = "HSL";
            else if (cg_changed)       desc = "Color Grading";
            else if (detail_changed)   desc = "Detail";
            else if (fx_changed)       desc = "Effects";

            EditCommand cmd;
            cmd.description = desc;
            cmd.before = drag_start_recipe_;
            cmd.after = recipe;
            cmd.affected_stage = PipelineStage::All;
            history.push(std::move(cmd));
        }
    }

    return any_changed;
}

// ─────────────────────────────────────────────────────────────────────
// White Balance
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::renderWhiteBalance(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr(S::WB_HEADER), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;

    // WB Preset dropdown
    const char* preset_labels[] = {
        tr("wb.preset.as_shot"),
        tr("wb.preset.daylight"),
        tr("wb.preset.cloudy"),
        tr("wb.preset.shade"),
        tr("wb.preset.tungsten"),
        tr("wb.preset.fluorescent"),
        tr("wb.preset.flash")
    };
    static int wb_preset_idx = 0;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##WBPreset", &wb_preset_idx, preset_labels, IM_ARRAYSIZE(preset_labels)))
    {
        const float temps[] = { as_shot_temperature_, 5500.0f, 6500.0f, 7500.0f, 2850.0f, 3800.0f, 5500.0f };
        const float tints[] = { as_shot_tint_,        0.0f,    0.0f,    0.0f,    0.0f,    10.0f,   0.0f    };
        recipe.wb_temperature = temps[wb_preset_idx];
        recipe.wb_tint        = tints[wb_preset_idx];
        changed = true;
    }

    changed |= vegaSlider("Temperature", &recipe.wb_temperature, 2000.0f, 12000.0f, 5500.0f, "%.0f K");
    changed |= vegaSlider("Tint",        &recipe.wb_tint,        -150.0f, 150.0f,   0.0f,    "%.0f");
    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// Tone
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::renderTone(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr(S::TONE_HEADER), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    ImGui::SameLine();
    if (ImGui::SmallButton(tr("tone.auto"))) {
        auto_tone_requested = true;
    }

    bool changed = false;
    changed |= vegaSlider("Exposure",   &recipe.exposure,   -5.0f,   5.0f,   0.0f, "%+.2f");
    changed |= vegaSlider("Contrast",   &recipe.contrast,   -100.0f, 100.0f, 0.0f, "%.0f");
    changed |= vegaSlider("Highlights", &recipe.highlights,  -100.0f, 100.0f, 0.0f, "%.0f");
    changed |= vegaSlider("Shadows",    &recipe.shadows,     -100.0f, 100.0f, 0.0f, "%.0f");
    changed |= vegaSlider("Whites",     &recipe.whites,      -100.0f, 100.0f, 0.0f, "%.0f");
    changed |= vegaSlider("Blacks",     &recipe.blacks,      -100.0f, 100.0f, 0.0f, "%.0f");
    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// Presence (Clarity, Texture, Dehaze)
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::renderPresence(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr("presence.header"), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;
    changed |= vegaSlider("presence.clarity", &recipe.clarity, -100.0f, 100.0f, 0.0f, "%.0f");
    changed |= vegaSlider("presence.texture", &recipe.texture, -100.0f, 100.0f, 0.0f, "%.0f");
    changed |= vegaSlider("presence.dehaze",  &recipe.dehaze,  -100.0f, 100.0f, 0.0f, "%.0f");
    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// Tone Curve — interactive Bezier-style curve editor
// ─────────────────────────────────────────────────────────────────────

// Evaluate a monotonic cubic spline through the given sorted control points
// at parameter t in [0,1].  Uses Catmull-Rom with clamped tangents.
static float evaluateCurveSpline(const std::vector<CurvePoint>& pts, float t)
{
    if (pts.empty()) return t;
    if (pts.size() == 1) return pts[0].y;
    if (t <= pts.front().x) return pts.front().y;
    if (t >= pts.back().x) return pts.back().y;

    // Find the segment
    size_t seg = 0;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        if (t >= pts[i].x && t <= pts[i + 1].x)
        {
            seg = i;
            break;
        }
    }

    const CurvePoint& p0 = pts[seg];
    const CurvePoint& p1 = pts[seg + 1];

    float dx = p1.x - p0.x;
    if (dx < 1e-6f) return p0.y;

    float local_t = (t - p0.x) / dx;

    // Catmull-Rom tangents
    float m0, m1;
    if (seg == 0)
        m0 = (p1.y - p0.y) / dx;
    else
        m0 = 0.5f * ((p1.y - p0.y) / dx + (p0.y - pts[seg - 1].y) / (p0.x - pts[seg - 1].x));

    if (seg + 2 >= pts.size())
        m1 = (p1.y - p0.y) / dx;
    else
        m1 = 0.5f * ((pts[seg + 2].y - p1.y) / (pts[seg + 2].x - p1.x) + (p1.y - p0.y) / dx);

    // Scale tangents by segment width
    m0 *= dx;
    m1 *= dx;

    // Hermite interpolation
    float t2 = local_t * local_t;
    float t3 = t2 * local_t;
    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + local_t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;

    float result = h00 * p0.y + h10 * m0 + h01 * p1.y + h11 * m1;
    return std::clamp(result, 0.0f, 1.0f);
}

bool DevelopPanel::renderToneCurve(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr(S::CURVE_HEADER), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;

    // Channel selector tabs
    {
        const char* channel_names[] = { "RGB", "R", "G", "B" };
        ImU32 channel_colors[] = {
            IM_COL32(200, 200, 200, 255),
            IM_COL32(220,  80,  80, 255),
            IM_COL32( 80, 200,  80, 255),
            IM_COL32( 80, 100, 220, 255)
        };

        for (int i = 0; i < 4; ++i)
        {
            if (i > 0) ImGui::SameLine();
            bool selected = (curve_channel_ == i);

            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            ImVec4 col = ImGui::ColorConvertU32ToFloat4(channel_colors[i]);
            ImGui::PushStyleColor(ImGuiCol_Text, col);

            if (ImGui::SmallButton(channel_names[i]))
            {
                curve_channel_ = i;
            }

            ImGui::PopStyleColor(); // Text
            if (selected) ImGui::PopStyleColor(2);
        }
    }

    // Curve canvas
    auto& curve = getActiveCurve(recipe);
    const float canvas_size = ImGui::GetContentRegionAvail().x;
    const float canvas_h = canvas_size; // Square
    const ImVec2 canvas_sz(canvas_size, canvas_h);

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_p1(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(25, 25, 28, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(60, 60, 65, 255));

    // Grid lines at 25% intervals
    for (int i = 1; i < 4; ++i)
    {
        float frac = i * 0.25f;
        // Vertical
        float gx = canvas_p0.x + frac * canvas_sz.x;
        draw_list->AddLine(ImVec2(gx, canvas_p0.y), ImVec2(gx, canvas_p1.y),
                           IM_COL32(50, 50, 55, 255));
        // Horizontal
        float gy = canvas_p0.y + frac * canvas_sz.y;
        draw_list->AddLine(ImVec2(canvas_p0.x, gy), ImVec2(canvas_p1.x, gy),
                           IM_COL32(50, 50, 55, 255));
    }

    // Diagonal reference line (linear = no adjustment)
    draw_list->AddLine(
        ImVec2(canvas_p0.x, canvas_p1.y),
        ImVec2(canvas_p1.x, canvas_p0.y),
        IM_COL32(60, 60, 65, 200));

    // Determine curve color based on channel
    ImU32 curve_col;
    switch (curve_channel_)
    {
    case 1: curve_col = IM_COL32(220, 80, 80, 255); break;
    case 2: curve_col = IM_COL32(80, 200, 80, 255); break;
    case 3: curve_col = IM_COL32(80, 100, 220, 255); break;
    default: curve_col = IM_COL32(200, 200, 200, 255); break;
    }

    // Draw the spline curve
    {
        const int num_segments = static_cast<int>(canvas_sz.x);
        ImVec2 prev;
        for (int i = 0; i <= num_segments; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(num_segments);
            float val = evaluateCurveSpline(curve, t);
            ImVec2 pt(
                canvas_p0.x + t * canvas_sz.x,
                canvas_p1.y - val * canvas_sz.y  // Y is inverted: 0 at bottom
            );
            if (i > 0)
                draw_list->AddLine(prev, pt, curve_col, 2.0f);
            prev = pt;
        }
    }

    // Draw control points
    const float point_radius = 5.0f;
    const float point_grab_radius = 10.0f;
    for (size_t i = 0; i < curve.size(); ++i)
    {
        ImVec2 screen_pos(
            canvas_p0.x + curve[i].x * canvas_sz.x,
            canvas_p1.y - curve[i].y * canvas_sz.y
        );

        // Filled circle for points
        draw_list->AddCircleFilled(screen_pos, point_radius, curve_col);
        draw_list->AddCircle(screen_pos, point_radius, IM_COL32(255, 255, 255, 200), 0, 1.5f);

        // Highlight hovered point
        ImVec2 mouse = ImGui::GetMousePos();
        float dx = mouse.x - screen_pos.x;
        float dy = mouse.y - screen_pos.y;
        if (dx * dx + dy * dy < point_grab_radius * point_grab_radius)
        {
            draw_list->AddCircle(screen_pos, point_radius + 3.0f,
                                 IM_COL32(255, 255, 255, 120), 0, 1.5f);
        }
    }

    // Invisible button for interaction
    ImGui::InvisibleButton("##curve_canvas", canvas_sz);
    bool canvas_hovered = ImGui::IsItemHovered();

    ImVec2 mouse = ImGui::GetMousePos();

    // Mouse to curve-space conversion
    auto screenToCurve = [&](ImVec2 pos) -> CurvePoint {
        return {
            std::clamp((pos.x - canvas_p0.x) / canvas_sz.x, 0.0f, 1.0f),
            std::clamp(1.0f - (pos.y - canvas_p0.y) / canvas_sz.y, 0.0f, 1.0f)
        };
    };

    // Find nearest point to mouse
    auto findNearestPoint = [&](float max_dist_sq) -> int {
        int nearest = -1;
        float best_dist = max_dist_sq;
        for (size_t i = 0; i < curve.size(); ++i)
        {
            ImVec2 sp(
                canvas_p0.x + curve[i].x * canvas_sz.x,
                canvas_p1.y - curve[i].y * canvas_sz.y
            );
            float dx = mouse.x - sp.x;
            float dy = mouse.y - sp.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_dist)
            {
                best_dist = d2;
                nearest = static_cast<int>(i);
            }
        }
        return nearest;
    };

    // Double-click on existing point (non-endpoint): remove it
    if (canvas_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        int idx = findNearestPoint(point_grab_radius * point_grab_radius);
        if (idx > 0 && idx < static_cast<int>(curve.size()) - 1)
        {
            curve.erase(curve.begin() + idx);
            dragged_point_index_ = -1;
            changed = true;
        }
    }
    // Start dragging a point or add a new point on click
    else if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        int idx = findNearestPoint(point_grab_radius * point_grab_radius);
        if (idx >= 0)
        {
            dragged_point_index_ = idx;
        }
        else
        {
            // Add a new point on the curve
            CurvePoint new_pt = screenToCurve(mouse);
            // Insert in sorted order by x
            auto it = std::lower_bound(curve.begin(), curve.end(), new_pt,
                [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
            auto inserted = curve.insert(it, new_pt);
            dragged_point_index_ = static_cast<int>(inserted - curve.begin());
            changed = true;
        }
    }

    // Drag the point
    if (dragged_point_index_ >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        CurvePoint new_pos = screenToCurve(mouse);
        int idx = dragged_point_index_;
        bool is_first = (idx == 0);
        bool is_last  = (idx == static_cast<int>(curve.size()) - 1);

        // Endpoints: lock x to 0.0 or 1.0
        if (is_first)
        {
            curve[idx].x = 0.0f;
            curve[idx].y = std::clamp(new_pos.y, 0.0f, 1.0f);
        }
        else if (is_last)
        {
            curve[idx].x = 1.0f;
            curve[idx].y = std::clamp(new_pos.y, 0.0f, 1.0f);
        }
        else
        {
            // Constrain x between neighbors (with small margin)
            float x_min = curve[idx - 1].x + 0.005f;
            float x_max = curve[idx + 1].x - 0.005f;
            curve[idx].x = std::clamp(new_pos.x, x_min, x_max);
            curve[idx].y = std::clamp(new_pos.y, 0.0f, 1.0f);
        }
        changed = true;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        dragged_point_index_ = -1;
    }

    // Tooltip showing coordinates when hovering
    if (canvas_hovered)
    {
        CurvePoint hover_pt = screenToCurve(mouse);
        float curve_val = evaluateCurveSpline(curve, hover_pt.x);
        ImGui::SetTooltip("In: %.0f  Out: %.0f", hover_pt.x * 255.0f, curve_val * 255.0f);
    }

    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// HSL
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::renderHSL(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr(S::HSL_HEADER), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;

    static const char* color_names[] = {
        "Red", "Orange", "Yellow", "Green", "Aqua", "Blue", "Purple", "Magenta"
    };

    // Color / B&W mode toggle
    if (ImGui::RadioButton(tr("hsl.color_mode"), !recipe.bw_mode))
    {
        recipe.bw_mode = false;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("hsl.bw_mode"), recipe.bw_mode))
    {
        recipe.bw_mode = true;
        changed = true;
    }

    ImGui::Spacing();

    if (recipe.bw_mode)
    {
        // B&W Mix: one slider per channel adjusting the grayscale luminance
        ImGui::TextDisabled("B&W Mix");
        ImGui::Spacing();
        for (int i = 0; i < 8; ++i)
        {
            changed |= vegaSlider(color_names[i], &recipe.bw_mix[i],
                                  -100.0f, 100.0f, 0.0f, "%.0f");
        }
    }
    else
    {
        // Existing HSL tab bar
        const char* tab_names[] = { tr("Hue"), tr("Saturation"), tr("Luminance") };

        if (ImGui::BeginTabBar("##hsl_tabs"))
        {
            for (int tab = 0; tab < 3; ++tab)
            {
                if (ImGui::BeginTabItem(tab_names[tab]))
                {
                    hsl_tab_ = tab;

                    std::array<float, 8>* arr = nullptr;
                    float range_min = 0.0f, range_max = 0.0f;
                    const char* fmt = "%.0f";

                    switch (tab)
                    {
                    case 0:
                        arr = &recipe.hsl_hue;
                        range_min = -180.0f; range_max = 180.0f;
                        break;
                    case 1:
                        arr = &recipe.hsl_saturation;
                        range_min = -100.0f; range_max = 100.0f;
                        break;
                    case 2:
                        arr = &recipe.hsl_luminance;
                        range_min = -100.0f; range_max = 100.0f;
                        break;
                    }

                    if (arr)
                    {
                        for (int i = 0; i < 8; ++i)
                        {
                            changed |= vegaSlider(color_names[i], &(*arr)[i],
                                                  range_min, range_max, 0.0f, fmt);
                        }
                    }

                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// Color Grading
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::renderColorGrading(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr("cg.header")))
        return false;

    bool changed = false;

    ImGui::TextDisabled("%s", tr("cg.shadows"));
    changed |= vegaSlider("cg.shadow_hue", &recipe.cg_shadows.hue,        0.0f, 360.0f, 0.0f,  "%.0f");
    changed |= vegaSlider("cg.shadow_sat", &recipe.cg_shadows.saturation, 0.0f, 100.0f, 0.0f,  "%.0f");

    ImGui::Separator();
    ImGui::TextDisabled("%s", tr("cg.midtones"));
    changed |= vegaSlider("cg.mid_hue", &recipe.cg_midtones.hue,        0.0f, 360.0f, 0.0f,  "%.0f");
    changed |= vegaSlider("cg.mid_sat", &recipe.cg_midtones.saturation, 0.0f, 100.0f, 0.0f,  "%.0f");

    ImGui::Separator();
    ImGui::TextDisabled("%s", tr("cg.highlights"));
    changed |= vegaSlider("cg.high_hue", &recipe.cg_highlights.hue,        0.0f, 360.0f, 0.0f,  "%.0f");
    changed |= vegaSlider("cg.high_sat", &recipe.cg_highlights.saturation, 0.0f, 100.0f, 0.0f,  "%.0f");

    ImGui::Separator();
    changed |= vegaSlider("cg.blending", &recipe.cg_blending, 0.0f, 100.0f, 50.0f, "%.0f");
    changed |= vegaSlider("cg.balance",  &recipe.cg_balance,  -100.0f, 100.0f, 0.0f, "%.0f");

    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// Detail (Sharpening + Noise Reduction)
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::renderDetail(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr(S::DETAIL_HEADER), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;

    // Sharpening sub-section
    ImGui::TextDisabled("%s", tr("Sharpening"));
    ImGui::Spacing();
    changed |= vegaSlider("Amount##sharp",  &recipe.sharpen_amount,  0.0f, 150.0f, 0.0f,  "%.0f");
    changed |= vegaSlider("Radius##sharp",  &recipe.sharpen_radius,  0.5f, 3.0f,   1.0f,  "%.1f");
    changed |= vegaSlider("Detail##sharp",  &recipe.sharpen_detail,  0.0f, 100.0f, 25.0f, "%.0f");
    changed |= vegaSlider("Masking##sharp", &recipe.sharpen_masking,  0.0f, 100.0f, 0.0f,  "%.0f");

    ImGui::Spacing();
    ImGui::Spacing();

    // Noise Reduction sub-section
    ImGui::TextDisabled("%s", tr("Noise Reduction"));
    ImGui::Spacing();
    changed |= vegaSlider("Luminance##nr",  &recipe.denoise_luminance, 0.0f, 100.0f, 0.0f,  "%.0f");
    changed |= vegaSlider("Color##nr",      &recipe.denoise_color,     0.0f, 100.0f, 0.0f,  "%.0f");
    changed |= vegaSlider("Detail##nr",     &recipe.denoise_detail,    0.0f, 100.0f, 50.0f, "%.0f");

    return changed;
}

// ─────────────────────────────────────────────────────────────────────
// Effects (Vibrance + Saturation)
// ─────────────────────────────────────────────────────────────────────

bool DevelopPanel::renderEffects(EditRecipe& recipe)
{
    if (!ImGui::CollapsingHeader(tr(S::FX_HEADER), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;
    changed |= vegaSlider("Vibrance",           &recipe.vibrance,   -100.0f, 100.0f, 0.0f, "%.0f");
    changed |= vegaSlider("Saturation##effect", &recipe.saturation, -100.0f, 100.0f, 0.0f, "%.0f");
    return changed;
}

} // namespace vega
