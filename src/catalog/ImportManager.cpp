#define NOMINMAX
#include "catalog/ImportManager.h"
#include "raw/RawDecoder.h"
#include "raw/ExifReader.h"
#include "core/Logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>

// For SHA-256 we use the Windows BCrypt API (available since Vista)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace vega {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Generate a UUID v4 string (lowercase hex with dashes).
static std::string generateUUID()
{
    static std::mt19937_64 rng(std::random_device{}());

    std::uniform_int_distribution<uint64_t> dist;
    uint64_t hi = dist(rng);
    uint64_t lo = dist(rng);

    // Set version (4) and variant (10xx) bits per RFC 4122
    hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;  // version 4
    lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;  // variant 10

    auto hex = [](uint64_t val, int start_byte, int count) {
        std::ostringstream ss;
        for (int i = 0; i < count; ++i) {
            int shift = (7 - (start_byte + i)) * 8;
            uint8_t byte = static_cast<uint8_t>((val >> shift) & 0xFF);
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
        }
        return ss.str();
    };

    // Format: 8-4-4-4-12
    return hex(hi, 0, 4) + "-" + hex(hi, 4, 2) + "-" + hex(hi, 6, 2) + "-" +
           hex(lo, 0, 2) + "-" + hex(lo, 2, 6);
}

// ---------------------------------------------------------------------------
// isRawExtension
// ---------------------------------------------------------------------------

bool ImportManager::isRawExtension(const std::string& ext)
{
    // Lowercase the extension for comparison
    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    static const std::vector<std::string> raw_extensions = {
        ".cr3", ".cr2", ".arw", ".nef", ".raf", ".dng",
        ".orf", ".rw2", ".pef", ".srw", ".x3f", ".3fr",
        ".rwl", ".mrw", ".dcr", ".kdc", ".erf", ".raw",
        ".iiq", ".mos", ".mef", ".nrw", ".rwz"
    };

    for (const auto& re : raw_extensions) {
        if (lower == re) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// scanDirectory
// ---------------------------------------------------------------------------

std::vector<std::filesystem::path> ImportManager::scanDirectory(
    const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> results;

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        VEGA_LOG_WARN("ImportManager::scanDirectory – not a valid directory: {}",
                      dir.string());
        return results;
    }

    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (ec) {
            VEGA_LOG_WARN("ImportManager::scanDirectory – iterator error: {}",
                          ec.message());
            ec.clear();
            continue;
        }

        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        if (isRawExtension(ext)) {
            results.push_back(entry.path());
        }
    }

    VEGA_LOG_INFO("ImportManager::scanDirectory – found {} RAW files in {}",
                  results.size(), dir.string());
    return results;
}

// ---------------------------------------------------------------------------
// computeFileHash (SHA-256)
// ---------------------------------------------------------------------------

std::string ImportManager::computeFileHash(const std::filesystem::path& path)
{
    // Open the file
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        VEGA_LOG_ERROR("ImportManager::computeFileHash – cannot open: {}",
                       path.string());
        return {};
    }

    // Open an algorithm handle
    BCRYPT_ALG_HANDLE alg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        VEGA_LOG_ERROR("ImportManager::computeFileHash – BCryptOpenAlgorithmProvider failed");
        return {};
    }

    // Create a hash object
    DWORD hash_obj_size = 0, data_size = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                      reinterpret_cast<PBYTE>(&hash_obj_size),
                      sizeof(hash_obj_size), &data_size, 0);

    std::vector<uint8_t> hash_obj(hash_obj_size);
    BCRYPT_HASH_HANDLE hash_handle = nullptr;

    status = BCryptCreateHash(alg, &hash_handle,
                              hash_obj.data(), hash_obj_size,
                              nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return {};
    }

    // Feed file data in chunks
    constexpr size_t CHUNK_SIZE = 64 * 1024;
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    while (file.good()) {
        file.read(reinterpret_cast<char*>(buffer.data()),
                  static_cast<std::streamsize>(CHUNK_SIZE));
        auto bytes_read = file.gcount();
        if (bytes_read > 0) {
            BCryptHashData(hash_handle, buffer.data(),
                           static_cast<ULONG>(bytes_read), 0);
        }
    }

    // Finalize
    DWORD hash_size = 0;
    BCryptGetProperty(alg, BCRYPT_HASH_LENGTH,
                      reinterpret_cast<PBYTE>(&hash_size),
                      sizeof(hash_size), &data_size, 0);

    std::vector<uint8_t> hash_value(hash_size);
    status = BCryptFinishHash(hash_handle, hash_value.data(), hash_size, 0);

    BCryptDestroyHash(hash_handle);
    BCryptCloseAlgorithmProvider(alg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        VEGA_LOG_ERROR("ImportManager::computeFileHash – BCryptFinishHash failed");
        return {};
    }

    // Convert to hex string
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (uint8_t byte : hash_value) {
        hex << std::setw(2) << static_cast<int>(byte);
    }

    return hex.str();
}

// ---------------------------------------------------------------------------
// import
// ---------------------------------------------------------------------------

ImportManager::ImportProgress ImportManager::import(
    const std::vector<std::filesystem::path>& files,
    Database& db,
    ThumbnailCache& thumb_cache,
    ProgressCallback progress_cb)
{
    ImportProgress progress;
    progress.total_files = static_cast<int>(files.size());

    for (const auto& file_path : files) {
        progress.current_file = file_path.filename().string();
        progress.processed++;

        // 1. Compute SHA-256 for dedup
        std::string hash = computeFileHash(file_path);
        if (hash.empty()) {
            VEGA_LOG_WARN("ImportManager::import – skipping (hash failed): {}",
                          file_path.string());
            progress.failed++;
            if (progress_cb) progress_cb(progress);
            continue;
        }

        // 2. Check if already in database (by file path or hash)
        auto existing = db.getPhotoByPath(file_path.string());
        if (existing.has_value()) {
            VEGA_LOG_DEBUG("ImportManager::import – skipping duplicate (path): {}",
                           file_path.string());
            progress.skipped_duplicate++;
            if (progress_cb) progress_cb(progress);
            continue;
        }

        // 3. Read metadata from RAW file
        auto meta_result = RawDecoder::readMetadata(file_path);

        PhotoRecord record;
        record.uuid = generateUUID();
        record.file_path = file_path.string();
        record.file_name = file_path.filename().string();
        record.file_hash = hash;

        // Get file size
        std::error_code ec;
        record.file_size = static_cast<int64_t>(
            std::filesystem::file_size(file_path, ec));

        if (meta_result) {
            const auto& meta = meta_result.value();
            record.camera_make   = meta.camera_make;
            record.camera_model  = meta.camera_model;
            record.lens_model    = meta.lens_model;
            record.iso           = meta.iso_speed;
            record.shutter_speed = meta.shutter_speed;
            record.aperture      = meta.aperture;
            record.focal_length  = meta.focal_length_mm;
            record.datetime_taken = meta.datetime_original;
            record.orientation   = meta.orientation;
        }

        // Try to get GPS coordinates
        auto gps = ExifReader::readGps(file_path);
        if (gps) {
            record.gps_lat = gps->latitude;
            record.gps_lon = gps->longitude;
        }

        // 4. Insert into database
        int64_t new_id = db.insertPhoto(record);
        if (new_id < 0) {
            VEGA_LOG_ERROR("ImportManager::import – DB insert failed: {}",
                           file_path.string());
            progress.failed++;
            if (progress_cb) progress_cb(progress);
            continue;
        }

        // 5. Generate thumbnail
        thumb_cache.generateAsync(record.uuid, file_path);

        progress.imported++;
        VEGA_LOG_INFO("ImportManager::import – imported: {} (id={})",
                      file_path.filename().string(), new_id);

        if (progress_cb) progress_cb(progress);
    }

    progress.complete = true;
    if (progress_cb) progress_cb(progress);

    VEGA_LOG_INFO("ImportManager::import – done: {} imported, {} skipped, {} failed",
                  progress.imported, progress.skipped_duplicate, progress.failed);

    return progress;
}

} // namespace vega
