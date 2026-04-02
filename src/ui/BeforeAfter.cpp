#define NOMINMAX
#include "ui/BeforeAfter.h"
#include "core/Logger.h"
#include <algorithm>

namespace vega {

void BeforeAfter::toggleMode()
{
    switch (mode_) {
    case Mode::SideBySide: mode_ = Mode::SplitView; VEGA_LOG_DEBUG("BeforeAfter: SplitView"); break;
    case Mode::SplitView:  mode_ = Mode::Toggle;    VEGA_LOG_DEBUG("BeforeAfter: Toggle"); break;
    case Mode::Toggle:     mode_ = Mode::SideBySide; VEGA_LOG_DEBUG("BeforeAfter: SideBySide"); break;
    }
}

void BeforeAfter::render(ID3D11ShaderResourceView* before_srv,
                         ID3D11ShaderResourceView* after_srv,
                         uint32_t img_width, uint32_t img_height)
{
    if (!before_srv || !after_srv || img_width == 0 || img_height == 0)
    {
        ImGui::TextDisabled("Before/After requires two images");
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f || avail.y < 1.0f)
        return;

    // Mode selector at top
    {
        const char* mode_names[] = { "Side by Side", "Split View", "Toggle" };
        int current = static_cast<int>(mode_);
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("##ba_mode", &current, mode_names, 3))
            mode_ = static_cast<Mode>(current);
        ImGui::SameLine();
        ImGui::TextDisabled("(M to cycle)");
    }

    // Remaining space after the mode selector
    avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f || avail.y < 1.0f)
        return;

    float img_aspect = static_cast<float>(img_width) / static_cast<float>(img_height);

    ImTextureRef before_tex(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(before_srv)));
    ImTextureRef after_tex(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(after_srv)));

    switch (mode_)
    {
    case Mode::SideBySide:
    {
        // Two images side by side, each taking half the width with a small gap
        float gap = 4.0f;
        float half_w = (avail.x - gap) * 0.5f;
        float half_h = half_w / img_aspect;
        if (half_h > avail.y)
        {
            half_h = avail.y;
            half_w = half_h * img_aspect;
        }

        // Center vertically
        float y_offset = (avail.y - half_h) * 0.5f;
        float x_offset = (avail.x - half_w * 2.0f - gap) * 0.5f;

        ImVec2 cursor_base = ImGui::GetCursorScreenPos();

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // "Before" label + image
        ImVec2 p_min_before(cursor_base.x + x_offset, cursor_base.y + y_offset);
        ImVec2 p_max_before(p_min_before.x + half_w, p_min_before.y + half_h);
        dl->AddImage(before_tex, p_min_before, p_max_before);
        dl->AddText(ImVec2(p_min_before.x + 4, p_min_before.y + 4),
                    IM_COL32(255, 255, 255, 200), "Before");

        // "After" label + image
        ImVec2 p_min_after(p_max_before.x + gap, cursor_base.y + y_offset);
        ImVec2 p_max_after(p_min_after.x + half_w, p_min_after.y + half_h);
        dl->AddImage(after_tex, p_min_after, p_max_after);
        dl->AddText(ImVec2(p_min_after.x + 4, p_min_after.y + 4),
                    IM_COL32(255, 255, 255, 200), "After");

        // Advance cursor so ImGui knows we used the space
        ImGui::Dummy(avail);
        break;
    }

    case Mode::SplitView:
    {
        // Single image fitting the viewport, with a draggable vertical split
        float disp_w = avail.x;
        float disp_h = disp_w / img_aspect;
        if (disp_h > avail.y)
        {
            disp_h = avail.y;
            disp_w = disp_h * img_aspect;
        }

        float x_offset = (avail.x - disp_w) * 0.5f;
        float y_offset = (avail.y - disp_h) * 0.5f;

        ImVec2 cursor_base = ImGui::GetCursorScreenPos();
        ImVec2 p_min(cursor_base.x + x_offset, cursor_base.y + y_offset);
        ImVec2 p_max(p_min.x + disp_w, p_min.y + disp_h);

        // Invisible button covering the entire area for input handling
        ImGui::InvisibleButton("##split_input", avail);

        // Handle dragging the split divider
        float split_x = p_min.x + disp_w * split_pos_;
        float grab_width = 8.0f;

        ImVec2 mouse = ImGui::GetMousePos();
        bool hovered = ImGui::IsItemHovered();

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (mouse.x >= split_x - grab_width && mouse.x <= split_x + grab_width &&
                mouse.y >= p_min.y && mouse.y <= p_max.y)
            {
                dragging_split_ = true;
            }
        }

        if (dragging_split_)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                float new_pos = (mouse.x - p_min.x) / disp_w;
                split_pos_ = std::clamp(new_pos, 0.02f, 0.98f);
            }
            else
            {
                dragging_split_ = false;
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Clip and draw "before" on the left side of the split
        float split_pixel = p_min.x + disp_w * split_pos_;

        // Draw "before" image (left portion)
        dl->PushClipRect(p_min, ImVec2(split_pixel, p_max.y), true);
        dl->AddImage(before_tex, p_min, p_max);
        dl->PopClipRect();

        // Draw "after" image (right portion)
        dl->PushClipRect(ImVec2(split_pixel, p_min.y), p_max, true);
        dl->AddImage(after_tex, p_min, p_max);
        dl->PopClipRect();

        // Draw the split divider line
        dl->AddLine(ImVec2(split_pixel, p_min.y), ImVec2(split_pixel, p_max.y),
                    IM_COL32(255, 255, 255, 220), 2.0f);

        // Draw small handle in the middle of the divider
        float handle_y = (p_min.y + p_max.y) * 0.5f;
        float handle_h = 30.0f;
        dl->AddRectFilled(ImVec2(split_pixel - 3, handle_y - handle_h * 0.5f),
                          ImVec2(split_pixel + 3, handle_y + handle_h * 0.5f),
                          IM_COL32(255, 255, 255, 200), 2.0f);

        // Labels
        dl->AddText(ImVec2(p_min.x + 4, p_min.y + 4),
                    IM_COL32(255, 255, 255, 180), "Before");
        dl->AddText(ImVec2(p_max.x - 40, p_min.y + 4),
                    IM_COL32(255, 255, 255, 180), "After");

        break;
    }

    case Mode::Toggle:
    {
        // Show one image at a time, toggled by backslash key
        float disp_w = avail.x;
        float disp_h = disp_w / img_aspect;
        if (disp_h > avail.y)
        {
            disp_h = avail.y;
            disp_w = disp_h * img_aspect;
        }

        float x_offset = (avail.x - disp_w) * 0.5f;
        float y_offset = (avail.y - disp_h) * 0.5f;

        ImVec2 cursor_base = ImGui::GetCursorScreenPos();
        ImVec2 p_min(cursor_base.x + x_offset, cursor_base.y + y_offset);
        ImVec2 p_max(p_min.x + disp_w, p_min.y + disp_h);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (show_before_)
        {
            dl->AddImage(before_tex, p_min, p_max);
            dl->AddText(ImVec2(p_min.x + 4, p_min.y + 4),
                        IM_COL32(255, 200, 100, 220), "Before (hold \\ to compare)");
        }
        else
        {
            dl->AddImage(after_tex, p_min, p_max);
            dl->AddText(ImVec2(p_min.x + 4, p_min.y + 4),
                        IM_COL32(200, 200, 200, 180), "After");
        }

        ImGui::Dummy(avail);
        break;
    }
    }
}

} // namespace vega
