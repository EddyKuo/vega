#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <functional>

namespace vega {

class FolderPanel {
public:
    struct FolderEntry {
        std::filesystem::path path;
        std::string display_name;
        int raw_count = 0;
        bool importing = false;
    };

    using FolderSelectedCallback = std::function<void(const std::filesystem::path&)>;

    void render();

    void addFolder(const std::filesystem::path& path, int raw_count = 0);
    void removeFolder(int index);
    void setFolders(const std::vector<std::filesystem::path>& paths);

    const std::vector<FolderEntry>& folders() const { return folders_; }
    int selectedIndex() const { return selected_idx_; }
    void updateRawCount(int index, int count);
    void setImporting(int index, bool importing);

    void setOnFolderSelected(FolderSelectedCallback cb) { on_folder_selected_ = std::move(cb); }
    void setOnAddFolder(std::function<void()> cb) { on_add_folder_ = std::move(cb); }
    void setOnRemoveFolder(std::function<void(int)> cb) { on_remove_folder_ = std::move(cb); }

    // Show all photos (no folder filter)
    void setOnShowAll(std::function<void()> cb) { on_show_all_ = std::move(cb); }

private:
    std::vector<FolderEntry> folders_;
    int selected_idx_ = -1;
    bool show_all_selected_ = true;

    FolderSelectedCallback on_folder_selected_;
    std::function<void()> on_add_folder_;
    std::function<void(int)> on_remove_folder_;
    std::function<void()> on_show_all_;
};

} // namespace vega
