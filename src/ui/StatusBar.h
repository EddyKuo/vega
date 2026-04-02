#pragma once
#include <string>

namespace vega {

class StatusBar {
public:
    // ── Data to display ──

    // Left section: file info
    std::string filename;
    std::string camera_info;     // "Canon EOS R5"
    std::string resolution;      // "8192x5464"

    // Center section: pipeline timing
    double pipeline_ms = 0.0;

    // Right section: runtime info
    float zoom_pct = 100.0f;
    int undo_current = 0;
    int undo_total = 0;
    bool use_gpu = true;

    /// Render the status bar anchored to the bottom of the main viewport.
    void render();
};

} // namespace vega
