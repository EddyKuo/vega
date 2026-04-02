#pragma once
#include "catalog/Database.h"
#include "catalog/ThumbnailCache.h"
#include <filesystem>
#include <functional>
#include <vector>
#include <string>
#include <atomic>

namespace vega {

class ImportManager {
public:
    struct ImportProgress {
        int total_files = 0;
        int processed = 0;
        int imported = 0;
        int skipped_duplicate = 0;
        int failed = 0;
        std::string current_file;
        bool complete = false;
    };

    using ProgressCallback = std::function<void(const ImportProgress&)>;

    // Scan a directory recursively for RAW files, returning the list of paths found.
    static std::vector<std::filesystem::path> scanDirectory(
        const std::filesystem::path& dir);

    // Import files into the database with dedup via SHA-256 hash.
    // Generates thumbnails for each imported file.
    // Calls progress_cb after each file (may be nullptr).
    ImportProgress import(const std::vector<std::filesystem::path>& files,
                          Database& db,
                          ThumbnailCache& thumb_cache,
                          ProgressCallback progress_cb = nullptr);

    // Compute SHA-256 hash of a file (hex string).
    static std::string computeFileHash(const std::filesystem::path& path);

private:
    // Check if extension is a supported RAW format.
    static bool isRawExtension(const std::string& ext);
};

} // namespace vega
