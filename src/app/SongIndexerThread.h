#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "app/SongIndex.h"

namespace tenriff::app {

class SongIndexerThread {
public:
    struct Progress {
        SongIndexProgressStage stage = SongIndexProgressStage::ScanningFiles;
        int total = -1;
        int processed = 0;
        int64_t started_ns = 0;
        int source_index = 0;
        int source_total = 0;

        [[nodiscard]] bool has_total() const { return total >= 0; }
    };

    SongIndexerThread();
    ~SongIndexerThread();

    SongIndexerThread(const SongIndexerThread&) = delete;
    SongIndexerThread& operator=(const SongIndexerThread&) = delete;

    [[nodiscard]] bool start(const std::string& songs_path,
                             const std::string& cache_path,
                             const SongIndexOptions& options = {});
    [[nodiscard]] bool start_sources(const std::vector<std::string>& source_roots,
                                     const std::string& profile_root,
                                     const SongIndexOptions& options = {},
                                     bool force_rescan = false);
    void stop();

    [[nodiscard]] bool is_running() const { return is_running_.load(std::memory_order_acquire); }
    [[nodiscard]] Progress progress() const;

    bool poll_result(SongIndex& out, std::vector<std::string>& warnings,
                     std::unordered_map<std::string, int>* source_counts = nullptr);

private:
    void thread_main(std::string songs_path, std::string cache_path, SongIndexOptions options);
    void sources_thread_main(std::vector<std::string> source_roots, std::string profile_root,
                             SongIndexOptions options, bool force_rescan);

    std::thread thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};

    std::atomic<int> progress_total_{-1};
    std::atomic<int> progress_processed_{0};
    std::atomic<int> progress_stage_{static_cast<int>(SongIndexProgressStage::ScanningFiles)};
    std::atomic<int64_t> progress_started_ns_{0};
    std::atomic<int> progress_source_index_{0};
    std::atomic<int> progress_source_total_{0};

    std::mutex mutex_;
    SongIndex result_;
    std::vector<std::string> warnings_;
    std::unordered_map<std::string, int> source_counts_;
    bool has_result_ = false;
};

}  // namespace tenriff::app
