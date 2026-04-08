#pragma once
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <d3d11.h>
#include <wrl/client.h>

namespace vega {

class Database;

class ThumbnailCache {
public:
    enum class Level { Micro = 160, Small = 320, Medium = 1024, Large = 2048 };

    void initialize(Database* db, ID3D11Device* device);

    // Get or generate a thumbnail. Returns SRV for display.
    // orientation: EXIF orientation / LibRaw flip value (0,3,5,6 etc.)
    ID3D11ShaderResourceView* getThumbnail(const std::string& uuid,
                                            const std::filesystem::path& raw_path,
                                            Level level,
                                            int orientation = 1);

    void clear();

private:
    Database* db_ = nullptr;
    ID3D11Device* device_ = nullptr;

    struct CacheEntry {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        uint64_t last_access = 0;
    };
    std::unordered_map<std::string, CacheEntry> memory_cache_;
    static constexpr size_t MAX_MEMORY_ENTRIES = 500;

    void evictOldest();
};

} // namespace vega
