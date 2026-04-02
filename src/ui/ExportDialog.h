#pragma once
#include "export/ExportManager.h"
#include <imgui.h>
#include <string>
#include <atomic>
#include <mutex>
#include <filesystem>

namespace vega {

class ExportDialog {
public:
    // Open the dialog
    void open(const std::filesystem::path& source_path);

    // Render the dialog. Returns true if an export was triggered.
    // Call every frame when visible.
    bool render(const std::vector<uint8_t>& rgba_data,
                uint32_t width, uint32_t height);

    bool isOpen() const { return open_; }

private:
    bool open_ = false;
    ExportSettings settings_;
    ExportManager exporter_;
    std::filesystem::path source_path_;

    // Export progress state
    std::atomic<bool> exporting_{false};
    std::atomic<float> export_progress_{0.0f};
    std::mutex status_mutex_;
    std::string export_status_;
    bool export_success_ = false;
    bool export_done_ = false;

    // Output directory buffer for ImGui input
    char output_dir_buf_[512] = {};
    char filename_buf_[256] = {};

    std::filesystem::path buildOutputPath() const;
};

} // namespace vega
