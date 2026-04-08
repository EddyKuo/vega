#include "catalog/Database.h"
#include "core/Logger.h"

#include <sqlite3.h>
#include <sstream>
#include <cassert>

namespace vega {

// ---------------------------------------------------------------------------
// open / close / isOpen
// ---------------------------------------------------------------------------

bool Database::open(const std::filesystem::path& catalog_path)
{
    if (db_) {
        VEGA_LOG_WARN("Database::open – already open, closing first");
        close();
    }

    // Ensure parent directory exists
    auto parent = catalog_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            VEGA_LOG_ERROR("Database::open – failed to create directory: {}", ec.message());
            return false;
        }
    }

    int rc = sqlite3_open(catalog_path.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::open – sqlite3_open failed: {}", sqlite3_errmsg(db_));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    // Enable WAL mode for better concurrent access
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");

    if (!createTables()) {
        VEGA_LOG_ERROR("Database::open – createTables failed");
        close();
        return false;
    }

    VEGA_LOG_INFO("Database::open – opened catalog: {}", catalog_path.string());
    return true;
}

void Database::close()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        VEGA_LOG_INFO("Database::close – closed");
    }
}

bool Database::isOpen() const
{
    return db_ != nullptr;
}

// ---------------------------------------------------------------------------
// Table creation
// ---------------------------------------------------------------------------

bool Database::createTables()
{
    // Main photos table
    const char* photos_sql = R"SQL(
        CREATE TABLE IF NOT EXISTS photos (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            uuid            TEXT NOT NULL UNIQUE,
            file_path       TEXT NOT NULL,
            file_name       TEXT NOT NULL,
            file_size       INTEGER DEFAULT 0,
            file_hash       TEXT,
            camera_make     TEXT DEFAULT '',
            camera_model    TEXT DEFAULT '',
            lens_model      TEXT DEFAULT '',
            iso             INTEGER DEFAULT 0,
            shutter_speed   REAL DEFAULT 0,
            aperture        REAL DEFAULT 0,
            focal_length    REAL DEFAULT 0,
            datetime_taken  TEXT DEFAULT '',
            gps_lat         REAL DEFAULT 0,
            gps_lon         REAL DEFAULT 0,
            width           INTEGER DEFAULT 0,
            height          INTEGER DEFAULT 0,
            orientation     INTEGER DEFAULT 1,
            rating          INTEGER DEFAULT 0,
            color_label     INTEGER DEFAULT 0,
            flag            INTEGER DEFAULT 0,
            caption         TEXT DEFAULT '',
            has_edits       INTEGER DEFAULT 0,
            edit_recipe_json TEXT DEFAULT '',
            created_at      TEXT DEFAULT (datetime('now')),
            updated_at      TEXT DEFAULT (datetime('now'))
        );
    )SQL";

    if (!exec(photos_sql)) return false;

    // Index on file_path for fast lookup
    if (!exec("CREATE INDEX IF NOT EXISTS idx_photos_file_path ON photos(file_path);"))
        return false;

    // Index on file_hash for dedup
    if (!exec("CREATE INDEX IF NOT EXISTS idx_photos_file_hash ON photos(file_hash);"))
        return false;

    // Index on rating for filtering
    if (!exec("CREATE INDEX IF NOT EXISTS idx_photos_rating ON photos(rating);"))
        return false;

    // Thumbnails table (JPEG BLOBs)
    const char* thumbnails_sql = R"SQL(
        CREATE TABLE IF NOT EXISTS thumbnails (
            uuid    TEXT NOT NULL,
            level   INTEGER NOT NULL,
            data    BLOB NOT NULL,
            PRIMARY KEY (uuid, level)
        );
    )SQL";

    if (!exec(thumbnails_sql)) return false;

    // Tags table
    const char* tags_sql = R"SQL(
        CREATE TABLE IF NOT EXISTS tags (
            id   INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        );
    )SQL";

    if (!exec(tags_sql)) return false;

    // Photo-tag junction table
    const char* photo_tags_sql = R"SQL(
        CREATE TABLE IF NOT EXISTS photo_tags (
            photo_id INTEGER NOT NULL,
            tag_id   INTEGER NOT NULL,
            PRIMARY KEY (photo_id, tag_id),
            FOREIGN KEY (photo_id) REFERENCES photos(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id)   REFERENCES tags(id)   ON DELETE CASCADE
        );
    )SQL";

    if (!exec(photo_tags_sql)) return false;

    // FTS5 virtual table for full-text search (optional — not all SQLite builds have it)
    const char* fts_sql = R"SQL(
        CREATE VIRTUAL TABLE IF NOT EXISTS photos_fts USING fts5(
            file_name,
            caption,
            camera_make,
            camera_model,
            lens_model,
            content='photos',
            content_rowid='id'
        );
    )SQL";

    if (exec(fts_sql)) {
        // Triggers to keep FTS in sync
        exec(R"SQL(
            CREATE TRIGGER IF NOT EXISTS photos_ai AFTER INSERT ON photos BEGIN
                INSERT INTO photos_fts(rowid, file_name, caption, camera_make, camera_model, lens_model)
                VALUES (new.id, new.file_name, new.caption, new.camera_make, new.camera_model, new.lens_model);
            END;
        )SQL");

        exec(R"SQL(
            CREATE TRIGGER IF NOT EXISTS photos_ad AFTER DELETE ON photos BEGIN
                INSERT INTO photos_fts(photos_fts, rowid, file_name, caption, camera_make, camera_model, lens_model)
                VALUES ('delete', old.id, old.file_name, old.caption, old.camera_make, old.camera_model, old.lens_model);
            END;
        )SQL");

        exec(R"SQL(
            CREATE TRIGGER IF NOT EXISTS photos_au AFTER UPDATE ON photos BEGIN
                INSERT INTO photos_fts(photos_fts, rowid, file_name, caption, camera_make, camera_model, lens_model)
                VALUES ('delete', old.id, old.file_name, old.caption, old.camera_make, old.camera_model, old.lens_model);
                INSERT INTO photos_fts(rowid, file_name, caption, camera_make, camera_model, lens_model)
                VALUES (new.id, new.file_name, new.caption, new.camera_make, new.camera_model, new.lens_model);
            END;
        )SQL");

        VEGA_LOG_INFO("Database: FTS5 full-text search enabled");
    } else {
        VEGA_LOG_WARN("Database: FTS5 not available, full-text search disabled");
    }

    return true;
}

// ---------------------------------------------------------------------------
// exec helper
// ---------------------------------------------------------------------------

bool Database::exec(const std::string& sql)
{
    assert(db_);
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::exec – SQL error: {}", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Helper: read a PhotoRecord from a prepared statement row
// ---------------------------------------------------------------------------

/// Safe helper: read a text column, returning "" if the column is NULL.
static const char* safeText(sqlite3_stmt* stmt, int col)
{
    const unsigned char* val = sqlite3_column_text(stmt, col);
    return val ? reinterpret_cast<const char*>(val) : "";
}

static PhotoRecord readRow(sqlite3_stmt* stmt)
{
    PhotoRecord p;
    int col = 0;
    p.id              = sqlite3_column_int64(stmt, col++);
    p.uuid            = safeText(stmt, col++);
    p.file_path       = safeText(stmt, col++);
    p.file_name       = safeText(stmt, col++);
    p.file_size       = sqlite3_column_int64(stmt, col++);
    p.file_hash       = safeText(stmt, col++);
    p.camera_make     = safeText(stmt, col++);
    p.camera_model    = safeText(stmt, col++);
    p.lens_model      = safeText(stmt, col++);
    p.iso             = static_cast<uint32_t>(sqlite3_column_int(stmt, col++));
    p.shutter_speed   = static_cast<float>(sqlite3_column_double(stmt, col++));
    p.aperture        = static_cast<float>(sqlite3_column_double(stmt, col++));
    p.focal_length    = static_cast<float>(sqlite3_column_double(stmt, col++));
    p.datetime_taken  = safeText(stmt, col++);
    p.gps_lat         = sqlite3_column_double(stmt, col++);
    p.gps_lon         = sqlite3_column_double(stmt, col++);
    p.width           = static_cast<uint32_t>(sqlite3_column_int(stmt, col++));
    p.height          = static_cast<uint32_t>(sqlite3_column_int(stmt, col++));
    p.orientation     = sqlite3_column_int(stmt, col++);
    p.rating          = sqlite3_column_int(stmt, col++);
    p.color_label     = sqlite3_column_int(stmt, col++);
    p.flag            = sqlite3_column_int(stmt, col++);
    p.caption         = safeText(stmt, col++);
    p.has_edits       = sqlite3_column_int(stmt, col++) != 0;
    p.edit_recipe_json = safeText(stmt, col++);
    return p;
}

static const char* SELECT_ALL_COLUMNS =
    "SELECT id, uuid, file_path, file_name, file_size, file_hash, "
    "camera_make, camera_model, lens_model, iso, shutter_speed, aperture, "
    "focal_length, datetime_taken, gps_lat, gps_lon, width, height, "
    "orientation, rating, color_label, flag, caption, has_edits, edit_recipe_json "
    "FROM photos";

// ---------------------------------------------------------------------------
// Photos CRUD
// ---------------------------------------------------------------------------

int64_t Database::insertPhoto(const PhotoRecord& photo)
{
    if (!db_) return -1;

    const char* sql =
        "INSERT INTO photos (uuid, file_path, file_name, file_size, file_hash, "
        "camera_make, camera_model, lens_model, iso, shutter_speed, aperture, "
        "focal_length, datetime_taken, gps_lat, gps_lon, width, height, "
        "orientation, rating, color_label, flag, caption, has_edits, edit_recipe_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::insertPhoto – prepare failed: {}", sqlite3_errmsg(db_));
        return -1;
    }

    int col = 1;
    sqlite3_bind_text(stmt, col++, photo.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, col++, photo.file_size);
    sqlite3_bind_text(stmt, col++, photo.file_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.camera_make.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.camera_model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.lens_model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, col++, static_cast<int>(photo.iso));
    sqlite3_bind_double(stmt, col++, static_cast<double>(photo.shutter_speed));
    sqlite3_bind_double(stmt, col++, static_cast<double>(photo.aperture));
    sqlite3_bind_double(stmt, col++, static_cast<double>(photo.focal_length));
    sqlite3_bind_text(stmt, col++, photo.datetime_taken.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, col++, photo.gps_lat);
    sqlite3_bind_double(stmt, col++, photo.gps_lon);
    sqlite3_bind_int(stmt, col++, static_cast<int>(photo.width));
    sqlite3_bind_int(stmt, col++, static_cast<int>(photo.height));
    sqlite3_bind_int(stmt, col++, photo.orientation);
    sqlite3_bind_int(stmt, col++, photo.rating);
    sqlite3_bind_int(stmt, col++, photo.color_label);
    sqlite3_bind_int(stmt, col++, photo.flag);
    sqlite3_bind_text(stmt, col++, photo.caption.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, col++, photo.has_edits ? 1 : 0);
    sqlite3_bind_text(stmt, col++, photo.edit_recipe_json.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    int64_t new_id = -1;
    if (rc == SQLITE_DONE) {
        new_id = sqlite3_last_insert_rowid(db_);
    } else {
        VEGA_LOG_ERROR("Database::insertPhoto – step failed: {}", sqlite3_errmsg(db_));
    }

    sqlite3_finalize(stmt);
    return new_id;
}

bool Database::updatePhoto(const PhotoRecord& photo)
{
    if (!db_ || photo.id <= 0) return false;

    const char* sql =
        "UPDATE photos SET uuid=?, file_path=?, file_name=?, file_size=?, file_hash=?, "
        "camera_make=?, camera_model=?, lens_model=?, iso=?, shutter_speed=?, aperture=?, "
        "focal_length=?, datetime_taken=?, gps_lat=?, gps_lon=?, width=?, height=?, "
        "orientation=?, rating=?, color_label=?, flag=?, caption=?, has_edits=?, "
        "edit_recipe_json=?, updated_at=datetime('now') WHERE id=?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::updatePhoto – prepare failed: {}", sqlite3_errmsg(db_));
        return false;
    }

    int col = 1;
    sqlite3_bind_text(stmt, col++, photo.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, col++, photo.file_size);
    sqlite3_bind_text(stmt, col++, photo.file_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.camera_make.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.camera_model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, col++, photo.lens_model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, col++, static_cast<int>(photo.iso));
    sqlite3_bind_double(stmt, col++, static_cast<double>(photo.shutter_speed));
    sqlite3_bind_double(stmt, col++, static_cast<double>(photo.aperture));
    sqlite3_bind_double(stmt, col++, static_cast<double>(photo.focal_length));
    sqlite3_bind_text(stmt, col++, photo.datetime_taken.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, col++, photo.gps_lat);
    sqlite3_bind_double(stmt, col++, photo.gps_lon);
    sqlite3_bind_int(stmt, col++, static_cast<int>(photo.width));
    sqlite3_bind_int(stmt, col++, static_cast<int>(photo.height));
    sqlite3_bind_int(stmt, col++, photo.orientation);
    sqlite3_bind_int(stmt, col++, photo.rating);
    sqlite3_bind_int(stmt, col++, photo.color_label);
    sqlite3_bind_int(stmt, col++, photo.flag);
    sqlite3_bind_text(stmt, col++, photo.caption.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, col++, photo.has_edits ? 1 : 0);
    sqlite3_bind_text(stmt, col++, photo.edit_recipe_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, col++, photo.id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
        VEGA_LOG_ERROR("Database::updatePhoto – step failed: {}", sqlite3_errmsg(db_));

    sqlite3_finalize(stmt);
    return ok;
}

std::optional<PhotoRecord> Database::getPhoto(int64_t id)
{
    if (!db_) return std::nullopt;

    std::string sql = std::string(SELECT_ALL_COLUMNS) + " WHERE id=?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::getPhoto – prepare failed: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, id);

    std::optional<PhotoRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRow(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<PhotoRecord> Database::getPhotoByPath(const std::string& path)
{
    if (!db_) return std::nullopt;

    std::string sql = std::string(SELECT_ALL_COLUMNS) + " WHERE file_path=?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::getPhotoByPath – prepare failed: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<PhotoRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRow(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<PhotoRecord> Database::getAllPhotos()
{
    std::vector<PhotoRecord> results;
    if (!db_) return results;

    std::string sql = std::string(SELECT_ALL_COLUMNS) + " ORDER BY datetime_taken DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::getAllPhotos – prepare failed: {}", sqlite3_errmsg(db_));
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(readRow(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool Database::deletePhoto(int64_t id)
{
    if (!db_) return false;

    const char* sql = "DELETE FROM photos WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::deletePhoto – prepare failed: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
        VEGA_LOG_ERROR("Database::deletePhoto – step failed: {}", sqlite3_errmsg(db_));

    sqlite3_finalize(stmt);
    return ok;
}

// ---------------------------------------------------------------------------
// Rating / Flag / Color
// ---------------------------------------------------------------------------

bool Database::setRating(int64_t photo_id, int rating)
{
    if (!db_) return false;

    const char* sql = "UPDATE photos SET rating=?, updated_at=datetime('now') WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, rating);
    sqlite3_bind_int64(stmt, 2, photo_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::setFlag(int64_t photo_id, int flag)
{
    if (!db_) return false;

    const char* sql = "UPDATE photos SET flag=?, updated_at=datetime('now') WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, flag);
    sqlite3_bind_int64(stmt, 2, photo_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::setColorLabel(int64_t photo_id, int label)
{
    if (!db_) return false;

    const char* sql = "UPDATE photos SET color_label=?, updated_at=datetime('now') WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, label);
    sqlite3_bind_int64(stmt, 2, photo_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ---------------------------------------------------------------------------
// Tags
// ---------------------------------------------------------------------------

int64_t Database::createTag(const std::string& name)
{
    if (!db_ || name.empty()) return -1;

    // Check if tag already exists
    {
        const char* sql = "SELECT id FROM tags WHERE name=?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int64_t existing_id = sqlite3_column_int64(stmt, 0);
                sqlite3_finalize(stmt);
                return existing_id;
            }
            sqlite3_finalize(stmt);
        }
    }

    // Insert new tag
    const char* sql = "INSERT INTO tags (name) VALUES (?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::createTag – prepare failed: {}", sqlite3_errmsg(db_));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    int64_t new_id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        new_id = sqlite3_last_insert_rowid(db_);
    } else {
        VEGA_LOG_ERROR("Database::createTag – step failed: {}", sqlite3_errmsg(db_));
    }

    sqlite3_finalize(stmt);
    return new_id;
}

bool Database::addTagToPhoto(int64_t photo_id, int64_t tag_id)
{
    if (!db_) return false;

    const char* sql = "INSERT OR IGNORE INTO photo_tags (photo_id, tag_id) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, photo_id);
    sqlite3_bind_int64(stmt, 2, tag_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::removeTagFromPhoto(int64_t photo_id, int64_t tag_id)
{
    if (!db_) return false;

    const char* sql = "DELETE FROM photo_tags WHERE photo_id=? AND tag_id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, photo_id);
    sqlite3_bind_int64(stmt, 2, tag_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<std::string> Database::getPhotoTags(int64_t photo_id)
{
    std::vector<std::string> tags;
    if (!db_) return tags;

    const char* sql =
        "SELECT t.name FROM tags t "
        "INNER JOIN photo_tags pt ON pt.tag_id = t.id "
        "WHERE pt.photo_id = ? ORDER BY t.name;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return tags;

    sqlite3_bind_int64(stmt, 1, photo_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) tags.emplace_back(name);
    }

    sqlite3_finalize(stmt);
    return tags;
}

// ---------------------------------------------------------------------------
// Search & Filter
// ---------------------------------------------------------------------------

std::vector<PhotoRecord> Database::filter(const FilterCriteria& criteria)
{
    std::vector<PhotoRecord> results;
    if (!db_) return results;

    // If there is a search_text, try FTS (may not be available)
    bool use_fts = !criteria.search_text.empty();
    // Check if FTS5 table exists
    if (use_fts) {
        sqlite3_stmt* check = nullptr;
        if (sqlite3_prepare_v2(db_, "SELECT 1 FROM sqlite_master WHERE type='table' AND name='photos_fts'",
                               -1, &check, nullptr) == SQLITE_OK) {
            use_fts = (sqlite3_step(check) == SQLITE_ROW);
            sqlite3_finalize(check);
        } else {
            use_fts = false;
        }
        if (!use_fts) {
            // Fallback: use LIKE on file_name
        }
    }

    std::ostringstream sql;
    sql << SELECT_ALL_COLUMNS;

    if (use_fts) {
        sql << " INNER JOIN photos_fts ON photos.id = photos_fts.rowid";
    }

    sql << " WHERE 1=1";

    // Build WHERE clauses
    std::vector<std::string> text_binds;  // for text binds in order

    if (criteria.min_rating > 0) {
        sql << " AND rating >= " << criteria.min_rating;
    }
    if (criteria.color_label >= 0) {
        sql << " AND color_label = " << criteria.color_label;
    }
    if (criteria.flag >= 0) {
        sql << " AND flag = " << criteria.flag;
    }
    if (!criteria.folder_path.empty()) {
        sql << " AND file_path LIKE ?";
        std::string prefix = criteria.folder_path;
        char last = prefix.back();
        if (last != '\\' && last != '/')
            prefix += '\\';
        text_binds.push_back(prefix + "%");
    }
    if (!criteria.camera_model.empty()) {
        sql << " AND camera_model = ?";
        text_binds.push_back(criteria.camera_model);
    }
    if (!criteria.lens_model.empty()) {
        sql << " AND lens_model = ?";
        text_binds.push_back(criteria.lens_model);
    }
    if (!criteria.date_from.empty()) {
        sql << " AND datetime_taken >= ?";
        text_binds.push_back(criteria.date_from);
    }
    if (!criteria.date_to.empty()) {
        sql << " AND datetime_taken <= ?";
        text_binds.push_back(criteria.date_to);
    }
    if (use_fts) {
        sql << " AND photos_fts MATCH ?";
        text_binds.push_back(criteria.search_text);
    }

    sql << " ORDER BY datetime_taken DESC;";

    std::string sql_str = sql.str();

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::filter – prepare failed: {}", sqlite3_errmsg(db_));
        return results;
    }

    // Bind text parameters
    for (size_t i = 0; i < text_binds.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1),
                          text_binds[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(readRow(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

int64_t Database::photoCount()
{
    if (!db_) return 0;

    const char* sql = "SELECT COUNT(*) FROM photos;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    int64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

int64_t Database::countByFolder(const std::string& folder_path)
{
    if (!db_ || folder_path.empty()) return 0;

    // Ensure folder path ends with separator for precise prefix matching
    std::string prefix = folder_path;
    char last = prefix.back();
    if (last != '\\' && last != '/')
        prefix += '\\';

    const char* sql = "SELECT COUNT(*) FROM photos WHERE file_path LIKE ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    std::string pattern = prefix + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);

    int64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

// ---------------------------------------------------------------------------
// Thumbnails
// ---------------------------------------------------------------------------

bool Database::saveThumbnail(const std::string& uuid, int level,
                              const void* jpeg_data, size_t jpeg_size)
{
    if (!db_ || !jpeg_data || jpeg_size == 0) return false;

    const char* sql =
        "INSERT OR REPLACE INTO thumbnails (uuid, level, data) VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("Database::saveThumbnail – prepare failed: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, level);
    sqlite3_bind_blob(stmt, 3, jpeg_data, static_cast<int>(jpeg_size), SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<uint8_t> Database::loadThumbnail(const std::string& uuid, int level)
{
    std::vector<uint8_t> result;
    if (!db_) return result;

    const char* sql = "SELECT data FROM thumbnails WHERE uuid = ? AND level = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, level);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);
        if (blob && blob_size > 0) {
            result.resize(static_cast<size_t>(blob_size));
            std::memcpy(result.data(), blob, result.size());
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

} // namespace vega
