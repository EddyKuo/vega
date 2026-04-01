#define NOMINMAX
#include "ui/ImageViewport.h"
#include <algorithm>
#include <cmath>

namespace vega {

void ImageViewport::fitToWindow(ImVec2 window_size, uint32_t img_w, uint32_t img_h)
{
    if (img_w == 0 || img_h == 0)
        return;

    float scale_x = window_size.x / static_cast<float>(img_w);
    float scale_y = window_size.y / static_cast<float>(img_h);
    zoom_ = std::min(scale_x, scale_y);
    zoom_ = std::clamp(zoom_, 0.1f, 16.0f);

    // Center the image
    pan_.x = 0.0f;
    pan_.y = 0.0f;
}

void ImageViewport::handleInput(ImVec2 viewport_size, uint32_t img_w, uint32_t img_h)
{
    ImGuiIO& io = ImGui::GetIO();

    bool hovered = ImGui::IsItemHovered();

    // ── Keyboard shortcuts (only when viewport window is focused) ──
    if (ImGui::IsWindowFocused())
    {
        // F: fit to window
        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            fitToWindow(viewport_size, img_w, img_h);
        }

        // 1: zoom to 100%
        if (ImGui::IsKeyPressed(ImGuiKey_1, false))
        {
            zoom_ = 1.0f;
            pan_.x = 0.0f;
            pan_.y = 0.0f;
        }

        // 2: zoom to 200%
        if (ImGui::IsKeyPressed(ImGuiKey_2, false))
        {
            zoom_ = 2.0f;
            pan_.x = 0.0f;
            pan_.y = 0.0f;
        }
    }

    if (!hovered)
        return;

    // ── Double-click: fit to window ──
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        fitToWindow(viewport_size, img_w, img_h);
        return;
    }

    // ── Mouse wheel zoom (centered on cursor) ──
    float wheel = io.MouseWheel;
    if (wheel != 0.0f)
    {
        // Cursor position relative to viewport content origin
        ImVec2 cursor_pos = ImGui::GetMousePos();
        ImVec2 content_origin = ImGui::GetCursorScreenPos();

        // Offset from viewport center
        float vp_cx = content_origin.x + viewport_size.x * 0.5f;
        float vp_cy = content_origin.y + viewport_size.y * 0.5f;
        float mouse_offset_x = cursor_pos.x - vp_cx;
        float mouse_offset_y = cursor_pos.y - vp_cy;

        float old_zoom = zoom_;
        float zoom_factor = 1.15f; // ~15% per tick
        if (wheel > 0.0f)
            zoom_ *= zoom_factor;
        else
            zoom_ /= zoom_factor;

        zoom_ = std::clamp(zoom_, 0.1f, 16.0f);

        // Adjust pan so the point under cursor stays fixed
        float ratio = 1.0f - zoom_ / old_zoom;
        pan_.x += mouse_offset_x * ratio;
        pan_.y += mouse_offset_y * ratio;
    }

    // ── Pan: middle mouse button OR Space + left mouse button ──
    bool pan_trigger = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                       (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDown(ImGuiMouseButton_Left));

    if (pan_trigger)
    {
        if (!dragging_)
        {
            dragging_ = true;
            drag_start_ = ImGui::GetMousePos();
        }
        else
        {
            ImVec2 current = ImGui::GetMousePos();
            pan_.x += current.x - drag_start_.x;
            pan_.y += current.y - drag_start_.y;
            drag_start_ = current;
        }
    }
    else
    {
        dragging_ = false;
    }
}

void ImageViewport::render(ID3D11ShaderResourceView* image_srv,
                           uint32_t img_width, uint32_t img_height)
{
    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (!image_srv || img_width == 0 || img_height == 0)
    {
        ImGui::Text("No image loaded");
        return;
    }

    // Draw an invisible button covering the viewport area to capture input
    ImGui::InvisibleButton("##viewport_input", avail,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonMiddle);
    handleInput(avail, img_width, img_height);

    // Calculate the displayed image size
    float disp_w = img_width * zoom_;
    float disp_h = img_height * zoom_;

    // Image is centered in the viewport, then offset by pan
    ImVec2 content_min = ImGui::GetItemRectMin();
    float offset_x = content_min.x + (avail.x - disp_w) * 0.5f + pan_.x;
    float offset_y = content_min.y + (avail.y - disp_h) * 0.5f + pan_.y;

    ImVec2 p_min(offset_x, offset_y);
    ImVec2 p_max(offset_x + disp_w, offset_y + disp_h);

    // Clip to viewport bounds
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(content_min,
                            ImVec2(content_min.x + avail.x, content_min.y + avail.y),
                            true);

    // Draw checkerboard or dark background could go here in the future.

    // Render the image using the draw list for precise positioning
    ImTextureRef tex_ref(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(image_srv)));
    draw_list->AddImage(tex_ref, p_min, p_max);

    draw_list->PopClipRect();

    // Overlay: zoom percentage in bottom-left corner
    {
        char zoom_text[32];
        snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", zoom_ * 100.0f);
        ImVec2 text_pos(content_min.x + 8.0f, content_min.y + avail.y - ImGui::GetTextLineHeight() - 8.0f);
        draw_list->AddText(text_pos, IM_COL32(200, 200, 200, 180), zoom_text);
    }
}

} // namespace vega
