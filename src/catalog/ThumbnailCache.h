#pragma once
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <d3d11.h>
#include <wrl/client.h>

namespace vega {

class Database;

class ThumbnailCache {
public:
    enum class Level { Micro = 160, Small = 320, Medium = 1024, Large = 2048 };

    ~ThumbnailCache();

    void initialize(Database* db, ID3D11Device* device);
    void shutdown();

    // Returns cached SRV immediately, or nullptr if not ready yet.
    // On cache miss, queues background decoding automatically.
    ID3D11ShaderResourceView* getThumbnail(const std::string& uuid,
                                            const std::filesystem::path& raw_path,
                                            Level level,
                                            int orientation = 1);

    // Flush completed background results to GPU (call once per frame).
    void flushPending();

    void clear();

private:
    Database* db_ = nullptr;
    ID3D11Device* device_ = nullptr;

    // -- Memory cache (GPU SRVs, main thread only) --
    struct CacheEntry {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        uint64_t last_access = 0;
    };
    std::unordered_map<std::string, CacheEntry> memory_cache_;
    static constexpr size_t MAX_MEMORY_ENTRIES = 500;
    void evictOldest();

    // -- Background decode thread pool --
    struct DecodeRequest {
        std::string key;
        std::string uuid;
        std::filesystem::path raw_path;
        Level level;
        int orientation;
    };

    struct DecodeResult {
        std::string key;
        std::vector<uint8_t> rgba;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    std::vector<std::thread> workers_;
    std::queue<DecodeRequest> work_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    bool stop_workers_ = false;

    // Results ready for GPU upload (protected by result_mutex_)
    std::vector<DecodeResult> pending_results_;
    std::mutex result_mutex_;

    // Track what's already queued to avoid duplicates
    std::unordered_set<std::string> queued_keys_;
    std::mutex queued_mutex_;

    void workerLoop();
};

} // namespace vega
