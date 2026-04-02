#pragma once
#include <functional>

namespace vega {

class Toolbar {
public:
    // Callbacks that the host (main.cpp) wires up
    struct Callbacks {
        std::function<void()> on_open;
        std::function<void()> on_save_recipe;
        std::function<void()> on_export;
        std::function<void()> on_undo;
        std::function<void()> on_redo;
        std::function<void()> on_zoom_fit;
        std::function<void()> on_zoom_100;
        std::function<void()> on_zoom_200;
        std::function<void()> on_toggle_before_after;
        std::function<void()> on_mode_grid;
        std::function<void()> on_mode_develop;
    };

    void setCallbacks(const Callbacks& cb) { callbacks_ = cb; }

    /// Render the toolbar. Call this each frame.
    /// has_image: whether an image is currently loaded.
    /// can_undo/can_redo: current history state.
    /// is_develop_mode: true = Develop, false = Grid.
    void render(bool has_image, bool can_undo, bool can_redo,
                bool is_develop_mode);

private:
    Callbacks callbacks_;

    // Helper to draw a text "icon button" with tooltip
    bool iconButton(const char* label, const char* tooltip, bool enabled = true);
};

} // namespace vega
