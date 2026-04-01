#pragma once
#include <imgui.h>
#include <d3d11.h>
#include <cstdint>

namespace vega {

class ImageViewport {
public:
    void render(ID3D11ShaderResourceView* image_srv,
                uint32_t img_width, uint32_t img_height);

    float zoom() const { return zoom_; }
    void fitToWindow(ImVec2 window_size, uint32_t img_w, uint32_t img_h);

private:
    float zoom_ = 1.0f;
    ImVec2 pan_ = {0, 0};
    bool dragging_ = false;
    ImVec2 drag_start_ = {0, 0};

    void handleInput(ImVec2 viewport_size, uint32_t img_w, uint32_t img_h);
};

} // namespace vega
