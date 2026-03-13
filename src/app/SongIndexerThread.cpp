#include "app/SongIndexerThread.h"

#include <filesystem>
#include <string>

#include "timing/HighResClock.h"

namespace tenriff::app {

SongIndexerThread::SongIndexerThread() = default;

SongIndexerThread::~SongIndexerThread() {
    stop();
}

bool SongIndexerThread::start(const std::string& songs_path,
                              const std::string& cache_path,
                              const SongIndexOptions& options) {
    if (is_running_.load(std::memory_order_acquire)) {
        return true;
    }

    if (thread_.joinable()) {
        should_stop_.store(true, std::memory_order_release);
        thread_.join();
        is_running_.store(false, std::memory_order_release);
    }

    should_stop_.store(false, std::memory_order_release);
    progress_total_.store(-1, std::memory_order_release);
    progress_processed_.store(0, std::memory_order_release);
    progress_stage_.store(static_cast<int>(SongIndexProgressStage::ScanningFiles), std::memory_order_release);
    progress_started_ns_.store(timing::HighResClock::now_ns(), std::memory_order_release);
    is_running_.store(true, std::memory_order_release);
    thread_ = std::thread(&SongIndexerThread::thread_main, this, songs_path, cache_path, options);
    return true;
}

void SongIndexerThread::stop() {
    should_stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    is_running_.store(false, std::memory_order_release);
}

SongIndexerThread::Progress SongIndexerThread::progress() const {
    Progress value;
    value.stage = static_cast<SongIndexProgressStage>(progress_stage_.load(std::memory_order_acquire));
    value.total = progress_total_.load(std::memory_order_acquire);
    value.processed = progress_processed_.load(std::memory_order_acquire);
    value.started_ns = progress_started_ns_.load(std::memory_order_acquire);
    return value;
}

bool SongIndexerThread::poll_result(SongIndex& out, std::vector<std::string>& warnings) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_result_) {
        return false;
    }
    out = std::move(result_);
    warnings = std::move(warnings_);
    result_ = {};
    warnings_.clear();
    has_result_ = false;
    return true;
}

void SongIndexerThread::thread_main(std::string songs_path, std::string cache_path, SongIndexOptions options) {
    std::vector<std::string> warnings;
    SongIndex index;

    try {
        SongIndexLoadResult cache_result = load_song_index(cache_path, options);
        for (const auto& warning : cache_result.warnings) {
            warnings.push_back(warning);
        }
        if (!cache_result.success() && !cache_result.error.empty()) {
            warnings.push_back(cache_result.error);
        }

        index = scan_songs(songs_path,
                           cache_result.success() ? &cache_result.index : nullptr,
                           warnings,
                           [this](const SongIndexProgress& update) {
                               progress_stage_.store(static_cast<int>(update.stage), std::memory_order_release);
                               progress_total_.store(update.total, std::memory_order_release);
                               progress_processed_.store(update.processed, std::memory_order_release);
                           },
                           options);

        std::error_code ec;
        const std::filesystem::path songs_root =
#ifdef _WIN32
            std::filesystem::u8path(songs_path);
#else
            std::filesystem::path(songs_path);
#endif
        const std::filesystem::path cache_file =
#ifdef _WIN32
            std::filesystem::u8path(cache_path);
#else
            std::filesystem::path(cache_path);
#endif
        if (std::filesystem::exists(songs_root, ec)) {
            std::filesystem::create_directories(cache_file.parent_path(), ec);
            if (ec) {
                warnings.push_back("Failed to create song index cache directory: " + ec.message());
            } else {
                std::string save_error;
                if (!save_song_index(cache_path,
                                     index,
                                     options,
                                     &save_error,
                                     [this](const SongIndexProgress& update) {
                                         progress_stage_.store(static_cast<int>(update.stage), std::memory_order_release);
                                         progress_total_.store(update.total, std::memory_order_release);
                                         progress_processed_.store(update.processed, std::memory_order_release);
                                     })) {
                    warnings.push_back(save_error);
                }
            }
        }
    } catch (const std::exception& e) {
        warnings.push_back(std::string("Song indexer exception: ") + e.what());
    } catch (...) {
        warnings.push_back("Song indexer exception: unknown.");
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = std::move(index);
        warnings_ = std::move(warnings);
        has_result_ = true;
    }

    is_running_.store(false, std::memory_order_release);
}

}  // namespace tenriff::app
