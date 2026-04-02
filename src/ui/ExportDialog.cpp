#define NOMINMAX
#include "ui/ExportDialog.h"
#include "core/Logger.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace vega {

void ExportDialog::open(const std::filesystem::path& source_path)
{
    open_ = true;
    source_path_ = source_path;
    exporting_ = false;
    export_progress_ = 0.0f;
    export_done_ = false;

    // Default output dir: same as source file
    auto dir = source_path.parent_path();
    if (!dir.empty())
    {
        settings_.output_dir = dir;
        std::string dir_str = dir.string();
        std::strncpy(output_dir_buf_, dir_str.c_str(), sizeof(output_dir_buf_) - 1);
        output_dir_buf_[sizeof(output_dir_buf_) - 1] = '\0';
    }

    // Default filename: source stem
    auto stem = source_path.stem().string();
    std::strncpy(filename_buf_, stem.c_str(), sizeof(filename_buf_) - 1);
    filename_buf_[sizeof(filename_buf_) - 1] = '\0';
}

std::filesystem::path ExportDialog::buildOutputPath() const
{
    std::filesystem::path dir = output_dir_buf_;
    std::string name = filename_buf_;

    // Append appropriate extension
    switch (settings_.format)
    {
    case ExportSettings::Format::JPEG:    name += ".jpg"; break;
    case ExportSettings::Format::PNG_8:
    case ExportSettings::Format::PNG_16:  name += ".png"; break;
    case ExportSettings::Format::TIFF_8:
    case ExportSettings::Format::TIFF_16: name += ".tif"; break;
    }

    return dir / name;
}

bool ExportDialog::render(const std::vector<uint8_t>& rgba_data,
                          uint32_t width, uint32_t height)
{
    if (!open_)
        return false;

    bool exported = false;

    ImGui::SetNextWindowSize(ImVec2(460, 440), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Export Image", &open_))
    {
        // ── Format ──
        ImGui::SeparatorText("Format");
        {
            const char* format_names[] = { "JPEG", "TIFF 8-bit", "TIFF 16-bit", "PNG 8-bit", "PNG 16-bit" };
            int fmt = static_cast<int>(settings_.format);
            ImGui::SetNextItemWidth(200);
            if (ImGui::Combo("Format", &fmt, format_names, 5))
                settings_.format = static_cast<ExportSettings::Format>(fmt);
        }

        // JPEG quality slider (only when JPEG is selected)
        if (settings_.format == ExportSettings::Format::JPEG)
        {
            ImGui::SetNextItemWidth(200);
            ImGui::SliderInt("Quality", &settings_.jpeg_quality, 1, 100, "%d%%");
        }

        // ── Resize ──
        ImGui::SeparatorText("Resize");
        {
            const char* resize_names[] = { "Original Size", "Long Edge (px)", "Short Edge (px)", "Percentage" };
            int rm = static_cast<int>(settings_.resize_mode);
            ImGui::SetNextItemWidth(200);
            if (ImGui::Combo("Resize", &rm, resize_names, 4))
                settings_.resize_mode = static_cast<ExportSettings::ResizeMode>(rm);

            if (settings_.resize_mode != ExportSettings::ResizeMode::Original)
            {
                int val = static_cast<int>(settings_.resize_value);
                ImGui::SetNextItemWidth(200);

                if (settings_.resize_mode == ExportSettings::ResizeMode::Percentage)
                {
                    if (val == 0) val = 100;
                    ImGui::SliderInt("##resize_val", &val, 1, 200, "%d%%");
                }
                else
                {
                    if (val == 0) val = static_cast<int>(std::max(width, height));
                    ImGui::InputInt("Pixels", &val);
                    val = std::max(val, 1);
                }
                settings_.resize_value = static_cast<uint32_t>(val);

                // Preview output dimensions
                uint32_t out_w = width, out_h = height;
                if (settings_.resize_mode == ExportSettings::ResizeMode::LongEdge)
                {
                    uint32_t le = std::max(width, height);
                    if (settings_.resize_value < le)
                    {
                        float s = static_cast<float>(settings_.resize_value) / static_cast<float>(le);
                        out_w = static_cast<uint32_t>(width * s + 0.5f);
                        out_h = static_cast<uint32_t>(height * s + 0.5f);
                    }
                }
                else if (settings_.resize_mode == ExportSettings::ResizeMode::ShortEdge)
                {
                    uint32_t se = std::min(width, height);
                    if (settings_.resize_value < se)
                    {
                        float s = static_cast<float>(settings_.resize_value) / static_cast<float>(se);
                        out_w = static_cast<uint32_t>(width * s + 0.5f);
                        out_h = static_cast<uint32_t>(height * s + 0.5f);
                    }
                }
                else if (settings_.resize_mode == ExportSettings::ResizeMode::Percentage)
                {
                    float pct = static_cast<float>(settings_.resize_value) / 100.0f;
                    out_w = static_cast<uint32_t>(width * pct + 0.5f);
                    out_h = static_cast<uint32_t>(height * pct + 0.5f);
                }
                out_w = std::max(out_w, 1u);
                out_h = std::max(out_h, 1u);
                ImGui::TextDisabled("Output: %ux%u", out_w, out_h);
            }
            else
            {
                ImGui::TextDisabled("Output: %ux%u (original)", width, height);
            }
        }

        // ── Output path ──
        ImGui::SeparatorText("Output");
        {
            ImGui::SetNextItemWidth(320);
            ImGui::InputText("Directory", output_dir_buf_, sizeof(output_dir_buf_));
            ImGui::SameLine();

#ifdef _WIN32
            if (ImGui::Button("Browse..."))
            {
                // Use Windows folder picker
                wchar_t folder_path[MAX_PATH] = {};
                BROWSEINFOW bi = {};
                bi.lpszTitle = L"Select Export Folder";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl)
                {
                    if (SHGetPathFromIDListW(pidl, folder_path))
                    {
                        std::filesystem::path p(folder_path);
                        std::string ps = p.string();
                        std::strncpy(output_dir_buf_, ps.c_str(), sizeof(output_dir_buf_) - 1);
                        output_dir_buf_[sizeof(output_dir_buf_) - 1] = '\0';
                    }
                    CoTaskMemFree(pidl);
                }
            }
#endif

            ImGui::SetNextItemWidth(320);
            ImGui::InputText("Filename", filename_buf_, sizeof(filename_buf_));

            // Show full output path preview
            auto preview_path = buildOutputPath();
            ImGui::TextDisabled("-> %s", preview_path.string().c_str());
        }

        // ── Metadata options ──
        ImGui::SeparatorText("Metadata");
        ImGui::Checkbox("Embed ICC profile", &settings_.embed_icc_profile);
        ImGui::Checkbox("Preserve EXIF", &settings_.preserve_exif);
        if (settings_.preserve_exif)
        {
            ImGui::SameLine();
            ImGui::Checkbox("Strip GPS", &settings_.strip_gps);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Export button / Progress ──
        if (exporting_)
        {
            float prog = export_progress_.load();

            if (prog < 0.0f)
            {
                // Export failed
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Export failed!");
                if (ImGui::Button("Close"))
                {
                    exporting_ = false;
                    export_done_ = false;
                }
            }
            else if (prog >= 1.0f)
            {
                // Export complete
                ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "Export complete!");
                {
                    std::lock_guard lock(status_mutex_);
                    ImGui::TextWrapped("%s", export_status_.c_str());
                }
                if (ImGui::Button("Close"))
                {
                    open_ = false;
                    exporting_ = false;
                    export_done_ = false;
                    exported = true;
                }
            }
            else
            {
                // In progress
                ImGui::ProgressBar(prog, ImVec2(-1, 0));
                {
                    std::lock_guard lock(status_mutex_);
                    ImGui::Text("%s", export_status_.c_str());
                }
            }
        }
        else
        {
            bool can_export = (width > 0 && height > 0 && !rgba_data.empty() &&
                               std::strlen(output_dir_buf_) > 0 &&
                               std::strlen(filename_buf_) > 0);

            if (!can_export) ImGui::BeginDisabled();

            if (ImGui::Button("Export", ImVec2(120, 0)))
            {
                auto output_path = buildOutputPath();
                settings_.output_dir = output_dir_buf_;

                exporting_ = true;
                export_progress_ = 0.0f;
                export_done_ = false;

                exporter_.exportAsync(rgba_data, width, height, settings_, output_path,
                    [this](float progress, const std::string& status)
                    {
                        export_progress_ = progress;
                        {
                            std::lock_guard lock(status_mutex_);
                            export_status_ = status;
                        }
                    });
            }

            if (!can_export) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
                open_ = false;
        }
    }
    ImGui::End();

    return exported;
}

} // namespace vega
