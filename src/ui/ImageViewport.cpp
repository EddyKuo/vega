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
    zoom_ = std::clamp(zoom_, 0.01f, 32.0f);
    pan_ = {0, 0};
}

bool ImageViewport::screenToImage(ImVec2 screen_pos, ImVec2 vp_min, ImVec2 vp_size,
                                   uint32_t img_w, uint32_t img_h,
                                   uint32_t& out_x, uint32_t& out_y) const
{
    float disp_w = img_w * zoom_;
    float disp_h = img_h * zoom_;
    float img_x0 = vp_min.x + (vp_size.x - disp_w) * 0.5f + pan_.x;
    float img_y0 = vp_min.y + (vp_size.y - disp_h) * 0.5f + pan_.y;

    float rel_x = (screen_pos.x - img_x0) / zoom_;
    float rel_y = (screen_pos.y - img_y0) / zoom_;

    if (rel_x < 0 || rel_y < 0 || rel_x >= img_w || rel_y >= img_h)
        return false;

    out_x = static_cast<uint32_t>(rel_x);
    out_y = static_cast<uint32_t>(rel_y);
    return true;
}

void ImageViewport::handleInput(ImVec2 viewport_size, uint32_t img_w, uint32_t img_h)
{
    ImGuiIO& io = ImGui::GetIO();
    bool hovered = ImGui::IsItemHovered();

    // Keyboard shortcuts when window is focused
    if (ImGui::IsWindowFocused())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
            fitToWindow(viewport_size, img_w, img_h);
        if (ImGui::IsKeyPressed(ImGuiKey_1, false))
        { zoom_ = 1.0f; pan_ = {0, 0}; }
        if (ImGui::IsKeyPressed(ImGuiKey_2, false))
        { zoom_ = 2.0f; pan_ = {0, 0}; }
        // Escape cancels eyedropper
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            eyedropper_active_ = false;
    }

    if (!hovered)
    {
        dragging_ = false;
        return;
    }

    // Change cursor for eyedropper mode
    if (eyedropper_active_)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // Double-click: fit to window
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !eyedropper_active_)
    {
        fitToWindow(viewport_size, img_w, img_h);
        return;
    }

    // Eyedropper click
    if (eyedropper_active_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        uint32_t px, py;
        if (screenToImage(ImGui::GetMousePos(), content_min_, content_size_,
                          img_w, img_h, px, py))
        {
            if (eyedropper_cb_)
                eyedropper_cb_(px, py);
        }
        eyedropper_active_ = false;
        return;
    }

    // Mouse wheel zoom centered on cursor
    float wheel = io.MouseWheel;
    if (wheel != 0.0f)
    {
        ImVec2 mouse = ImGui::GetMousePos();

        // Mouse offset from viewport center
        float vp_cx = content_min_.x + content_size_.x * 0.5f;
        float vp_cy = content_min_.y + content_size_.y * 0.5f;
        float mx = mouse.x - vp_cx;
        float my = mouse.y - vp_cy;

        float old_zoom = zoom_;
        float factor = (wheel > 0) ? 1.15f : (1.0f / 1.15f);
        zoom_ = std::clamp(zoom_ * factor, 0.01f, 32.0f);

        // Keep the point under cursor fixed
        float ratio = 1.0f - zoom_ / old_zoom;
        pan_.x += mx * ratio;
        pan_.y += my * ratio;
    }

    // Pan: left mouse drag (like Lightroom), middle mouse, or Space+Left
    bool left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mid_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    bool space_held = ImGui::IsKeyDown(ImGuiKey_Space);
    bool pan_trigger = mid_down || (left_down && !eyedropper_active_);

    if (space_held && left_down)
        pan_trigger = true;

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

    // Invisible button to capture all mouse input
    ImGui::InvisibleButton("##viewport_input", avail,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonMiddle);

    // Cache content rect for coordinate conversion
    content_min_ = ImGui::GetItemRectMin();
    content_size_ = avail;

    handleInput(avail, img_width, img_height);

    // Image display
    float disp_w = img_width * zoom_;
    float disp_h = img_height * zoom_;

    float offset_x = content_min_.x + (avail.x - disp_w) * 0.5f + pan_.x;
    float offset_y = content_min_.y + (avail.y - disp_h) * 0.5f + pan_.y;

    ImVec2 p_min(offset_x, offset_y);
    ImVec2 p_max(offset_x + disp_w, offset_y + disp_h);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 clip_max(content_min_.x + avail.x, content_min_.y + avail.y);
    dl->PushClipRect(content_min_, clip_max, true);

    // Checkerboard background for transparent areas (dark grey)
    dl->AddRectFilled(content_min_, clip_max, IM_COL32(30, 30, 32, 255));

    // Draw the image
    ImTextureRef tex_ref(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(image_srv)));
    dl->AddImage(tex_ref, p_min, p_max);

    dl->PopClipRect();

    // Overlay info
    {
        char info[64];
        snprintf(info, sizeof(info), "%.0f%%", zoom_ * 100.0f);
        if (eyedropper_active_)
            snprintf(info, sizeof(info), "%.0f%%  [WB Eyedropper]", zoom_ * 100.0f);

        ImVec2 text_pos(content_min_.x + 8.0f,
                        content_min_.y + avail.y - ImGui::GetTextLineHeight() - 8.0f);
        dl->AddText(text_pos, IM_COL32(200, 200, 200, 180), info);
    }

    // Show pixel info under cursor when hovered
    if (ImGui::IsItemHovered())
    {
        uint32_t px, py;
        if (screenToImage(ImGui::GetMousePos(), content_min_, avail,
                          img_width, img_height, px, py))
        {
            char coord[48];
            snprintf(coord, sizeof(coord), "(%u, %u)", px, py);
            ImVec2 text_pos(content_min_.x + avail.x - ImGui::CalcTextSize(coord).x - 8.0f,
                            content_min_.y + avail.y - ImGui::GetTextLineHeight() - 8.0f);
            dl->AddText(text_pos, IM_COL32(200, 200, 200, 180), coord);
        }
    }
}

} // namespace vega
