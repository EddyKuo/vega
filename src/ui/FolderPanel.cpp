#define NOMINMAX
#include "ui/FolderPanel.h"
#include "core/i18n.h"

#include <imgui.h>
#include <algorithm>

namespace vega {

void FolderPanel::render()
{
    // Add Folder button
    if (ImGui::Button(tr("folder.add"), ImVec2(-1, 0))) {
        if (on_add_folder_) on_add_folder_();
    }

    ImGui::Separator();

    // "All Photos" entry
    {
        bool is_selected = show_all_selected_;
        if (ImGui::Selectable(tr("folder.all_photos"), is_selected)) {
            show_all_selected_ = true;
            selected_idx_ = -1;
            if (on_show_all_) on_show_all_();
        }
    }

    ImGui::Separator();

    // Folder list
    for (int i = 0; i < static_cast<int>(folders_.size()); ++i) {
        auto& entry = folders_[i];
        ImGui::PushID(i);

        bool is_selected = (!show_all_selected_ && selected_idx_ == i);

        // Format: "FolderName (123)"
        char label[512];
        if (entry.importing) {
            snprintf(label, sizeof(label), "%s (...)", entry.display_name.c_str());
        } else {
            snprintf(label, sizeof(label), "%s (%d)", entry.display_name.c_str(), entry.raw_count);
        }

        if (ImGui::Selectable(label, is_selected)) {
            selected_idx_ = i;
            show_all_selected_ = false;
            if (on_folder_selected_) on_folder_selected_(entry.path);
        }

        // Tooltip with full path
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", entry.path.string().c_str());
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem("##folder_ctx")) {
            if (ImGui::MenuItem(tr("folder.remove"))) {
                if (on_remove_folder_) on_remove_folder_(i);
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

void FolderPanel::addFolder(const std::filesystem::path& path, int raw_count)
{
    // Check for duplicate
    for (const auto& f : folders_) {
        if (f.path == path) return;
    }

    FolderEntry entry;
    entry.path = path;
    entry.display_name = path.filename().string();
    if (entry.display_name.empty())
        entry.display_name = path.string();
    entry.raw_count = raw_count;
    folders_.push_back(std::move(entry));
}

void FolderPanel::removeFolder(int index)
{
    if (index < 0 || index >= static_cast<int>(folders_.size())) return;
    folders_.erase(folders_.begin() + index);

    if (selected_idx_ == index) {
        selected_idx_ = -1;
        show_all_selected_ = true;
    } else if (selected_idx_ > index) {
        selected_idx_--;
    }
}

void FolderPanel::setFolders(const std::vector<std::filesystem::path>& paths)
{
    folders_.clear();
    for (const auto& p : paths) {
        addFolder(p);
    }
}

void FolderPanel::updateRawCount(int index, int count)
{
    if (index >= 0 && index < static_cast<int>(folders_.size())) {
        folders_[index].raw_count = count;
    }
}

void FolderPanel::setImporting(int index, bool importing)
{
    if (index >= 0 && index < static_cast<int>(folders_.size())) {
        folders_[index].importing = importing;
    }
}

bool FolderPanel::selectByPath(const std::filesystem::path& path)
{
    for (int i = 0; i < static_cast<int>(folders_.size()); ++i) {
        if (folders_[i].path == path) {
            selected_idx_ = i;
            show_all_selected_ = false;
            if (on_folder_selected_) on_folder_selected_(path);
            return true;
        }
    }
    return false;
}

} // namespace vega
