#pragma once
#include <imgui.h>
#include <d3d11.h>
#include <cstdint>

namespace vega {

class BeforeAfter {
public:
    enum class Mode { SideBySide, SplitView, Toggle };

    void render(ID3D11ShaderResourceView* before_srv,
                ID3D11ShaderResourceView* after_srv,
                uint32_t img_width, uint32_t img_height);

    void setMode(Mode m) { mode_ = m; }
    Mode mode() const { return mode_; }
    void toggleMode();

    // For Toggle mode: show "before" while key held
    void setShowBefore(bool show) { show_before_ = show; }

private:
    Mode mode_ = Mode::SplitView;
    float split_pos_ = 0.5f;  // 0-1, position of divider
    bool show_before_ = false;
    bool dragging_split_ = false;
};

} // namespace vega
