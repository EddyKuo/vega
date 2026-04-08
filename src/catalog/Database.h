#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct sqlite3;  // forward declare

namespace vega {

struct PhotoRecord {
    int64_t id = 0;
    std::string uuid;
    std::string file_path, file_name;
    int64_t file_size = 0;
    std::string file_hash;
    std::string camera_make, camera_model, lens_model;
    uint32_t iso = 0;
    float shutter_speed = 0, aperture = 0, focal_length = 0;
    std::string datetime_taken;
    double gps_lat = 0, gps_lon = 0;
    uint32_t width = 0, height = 0;
    int orientation = 1;
    int rating = 0, color_label = 0, flag = 0;
    std::string caption;
    bool has_edits = false;
    std::string edit_recipe_json;
};

class Database {
public:
    bool open(const std::filesystem::path& catalog_path);
    void close();
    bool isOpen() const;

    // Photos CRUD
    int64_t insertPhoto(const PhotoRecord& photo);
    bool updatePhoto(const PhotoRecord& photo);
    std::optional<PhotoRecord> getPhoto(int64_t id);
    std::optional<PhotoRecord> getPhotoByPath(const std::string& path);
    std::vector<PhotoRecord> getAllPhotos();
    bool deletePhoto(int64_t id);

    // Rating/Flag/Color
    bool setRating(int64_t photo_id, int rating);
    bool setFlag(int64_t photo_id, int flag);
    bool setColorLabel(int64_t photo_id, int label);

    // Tags
    int64_t createTag(const std::string& name);
    bool addTagToPhoto(int64_t photo_id, int64_t tag_id);
    bool removeTagFromPhoto(int64_t photo_id, int64_t tag_id);
    std::vector<std::string> getPhotoTags(int64_t photo_id);

    // Search & Filter
    struct FilterCriteria {
        int min_rating = 0;
        int color_label = -1;  // -1 = any
        int flag = -1;
        std::string camera_model;
        std::string lens_model;
        std::string date_from, date_to;
        std::string search_text;  // FTS5
        std::string folder_path;  // filter by folder prefix
    };
    std::vector<PhotoRecord> filter(const FilterCriteria& criteria);

    // Thumbnails (stored as JPEG BLOBs)
    bool saveThumbnail(const std::string& uuid, int level,
                       const void* jpeg_data, size_t jpeg_size);
    std::vector<uint8_t> loadThumbnail(const std::string& uuid, int level);

    // Stats
    int64_t photoCount();
    int64_t countByFolder(const std::string& folder_path);

    sqlite3* handle() const { return db_; }

private:
    sqlite3* db_ = nullptr;
    bool createTables();
    bool exec(const std::string& sql);
};

} // namespace vega
