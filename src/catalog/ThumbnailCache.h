#pragma once
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

namespace vega {

class ThumbnailCache {
public:
    enum class Level { Micro = 160, Small = 320, Medium = 1024, Large = 2048 };

    void initialize(const std::filesystem::path& cache_dir, ID3D11Device* device);

    // Get or generate a thumbnail. Returns SRV for display.
    ID3D11ShaderResourceView* getThumbnail(const std::string& uuid,
                                            const std::filesystem::path& raw_path,
                                            Level level);

    // Pre-generate thumbnails in background
    void generateAsync(const std::string& uuid, const std::filesystem::path& raw_path);

    void clear();

private:
    std::filesystem::path cache_dir_;
    ID3D11Device* device_ = nullptr;

    struct CacheEntry {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        uint64_t last_access = 0;
    };
    std::unordered_map<std::string, CacheEntry> memory_cache_;
    static constexpr size_t MAX_MEMORY_ENTRIES = 500;

    std::filesystem::path diskPath(const std::string& uuid, Level level);
    bool loadFromDisk(const std::string& key, const std::filesystem::path& path);
    bool saveToDisk(const std::vector<uint8_t>& jpeg_data, const std::filesystem::path& path);
    void evictOldest();
};

} // namespace vega
