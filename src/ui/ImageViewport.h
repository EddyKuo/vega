#pragma once
#include <imgui.h>
#include <d3d11.h>
#include <cstdint>
#include <functional>

namespace vega {

class ImageViewport {
public:
    void render(ID3D11ShaderResourceView* image_srv,
                uint32_t img_width, uint32_t img_height);

    float zoom() const { return zoom_; }
    void fitToWindow(ImVec2 window_size, uint32_t img_w, uint32_t img_h);

    // WB eyedropper mode
    bool isEyedropperActive() const { return eyedropper_active_; }
    void activateEyedropper() { eyedropper_active_ = true; }

    // Callback when eyedropper picks a pixel (x, y in image coordinates)
    using EyedropperCallback = std::function<void(uint32_t x, uint32_t y)>;
    void setEyedropperCallback(EyedropperCallback cb) { eyedropper_cb_ = cb; }

private:
    float zoom_ = 1.0f;
    ImVec2 pan_ = {0, 0};
    bool dragging_ = false;
    ImVec2 drag_start_ = {0, 0};

    bool eyedropper_active_ = false;
    EyedropperCallback eyedropper_cb_;

    // Cached content rect from last render
    ImVec2 content_min_ = {0, 0};
    ImVec2 content_size_ = {0, 0};

    void handleInput(ImVec2 viewport_size, uint32_t img_w, uint32_t img_h);

    // Convert screen position to image pixel coordinates
    bool screenToImage(ImVec2 screen_pos, ImVec2 viewport_min, ImVec2 viewport_size,
                       uint32_t img_w, uint32_t img_h,
                       uint32_t& out_x, uint32_t& out_y) const;
};

} // namespace vega
