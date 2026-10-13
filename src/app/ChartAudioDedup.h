#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace tenriff::app {

// Loading-thread registry. Fingerprints only select candidates: PCM bytes must
// match before two files share an identity. Weak references preserve streaming
// eviction; an evicted representative can be decoded again outside the mixer.
// This private mutex must never be nested with the session streaming mutex:
// candidate reloads can be slow, while the HUD loop also needs streaming state.
class ChartAudioPcmRegistry {
public:
    using Samples = std::shared_ptr<const std::vector<float>>;

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    template <typename Reload>
    std::size_t intern(const Samples& samples, const std::string& path, Reload&& reload) {
        if (!samples || samples->empty()) return 0;
        const auto fingerprint = hash(*samples);
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : entries_) {
            if (entry.fingerprint != fingerprint || entry.sample_count != samples->size()) continue;
            auto representative = entry.samples.lock();
            if (!representative) representative = reload(entry.path);
            if (!representative || representative->size() != samples->size() ||
                std::memcmp(representative->data(), samples->data(), samples->size() * sizeof(float)) != 0) continue;
            entry.samples = samples;
            entry.path = path;
            return entry.id;
        }
        const auto id = entries_.size() + 1;
        entries_.push_back({fingerprint, samples->size(), samples, path, id});
        return id;
    }

private:
    struct Entry {
        std::uint64_t fingerprint;
        std::size_t sample_count;
        std::weak_ptr<const std::vector<float>> samples;
        std::string path;
        std::size_t id;
    };
    static std::uint64_t hash(const std::vector<float>& samples) {
        std::uint64_t value = UINT64_C(14695981039346656037);
        const auto* bytes = reinterpret_cast<const unsigned char*>(samples.data());
        for (std::size_t i = 0; i < samples.size() * sizeof(float); ++i) {
            value ^= bytes[i];
            value *= UINT64_C(1099511628211);
        }
        return value;
    }
    std::mutex mutex_;
    std::vector<Entry> entries_;
};

// Published atomically alongside decoded PCM. Checking the shared sample object
// prevents an eviction/reload race from pairing old PCM with a new identity.
struct ChartAudioPcmIdentity {
    ChartAudioPcmRegistry::Samples samples;
    std::size_t id = 0;
};

// All storage is prepared from authored chart occurrences before audio starts.
// claim() performs no allocation, PCM comparison, file I/O, or locking.
class ChartAudioDeduplicator {
public:
    struct Occurrence {
        int64_t chart_sample;
        std::size_t asset_id;
        bool keysound;
        float gain;
        bool played = false;
        std::size_t pcm_identity = 0;
        float effective_gain = 0.0f;
        std::uint64_t candidate_epoch = 0;
        int64_t candidate_start = 0;
        std::size_t candidate_voice = 0;
        std::size_t candidate_identity = 0;
        float candidate_gain = 0.0f;
    };

    void prepare(std::vector<Occurrence> occurrences) {
        occurrences.erase(std::remove_if(occurrences.begin(), occurrences.end(),
            [](const auto& value) { return !std::isfinite(value.gain); }), occurrences.end());
        const auto key = [](const Occurrence& value) {
            return std::tie(value.chart_sample, value.asset_id, value.keysound, value.gain);
        };
        std::sort(occurrences.begin(), occurrences.end(), [&](const auto& a, const auto& b) { return key(a) < key(b); });
        occurrences.erase(std::unique(occurrences.begin(), occurrences.end(),
            [&](const auto& a, const auto& b) { return key(a) == key(b); }), occurrences.end());
        occurrences_ = std::move(occurrences);
    }
    void clear() { occurrences_.clear(); }

    void begin_buffer() {
        if (++epoch_ == 0) {
            for (auto& occurrence : occurrences_) occurrence.candidate_epoch = 0;
            ++epoch_;
        }
    }

    void consider(int64_t chart_sample, std::size_t asset_id, bool keysound, float gain,
                  std::size_t pcm_identity, float effective_gain, int64_t start, std::size_t voice) {
        if (pcm_identity == 0 || !(effective_gain > 0.0f)) return;
        const auto begin = std::lower_bound(occurrences_.begin(), occurrences_.end(), chart_sample,
            [](const Occurrence& value, int64_t sample) { return value.chart_sample < sample; });
        for (auto it = begin; it != occurrences_.end() && it->chart_sample == chart_sample; ++it) {
            if (it->asset_id != asset_id || it->keysound != keysound || it->gain != gain) continue;
            if (it->candidate_epoch != epoch_ || std::tie(start, voice) < std::tie(it->candidate_start, it->candidate_voice)) {
                it->candidate_epoch = epoch_;
                it->candidate_start = start;
                it->candidate_voice = voice;
                it->candidate_identity = pcm_identity;
                it->candidate_gain = effective_gain;
            }
            break;
        }
    }

    bool claim(int64_t chart_sample, std::size_t asset_id, bool keysound, float gain,
               std::size_t pcm_identity, float effective_gain,
               int64_t start = 0, std::size_t voice = (std::numeric_limits<std::size_t>::max)()) {
        // Unknown/failed PCM and silent voices cannot suppress an audible sound.
        if (pcm_identity == 0 || !(effective_gain > 0.0f)) return true;
        const auto begin = std::lower_bound(occurrences_.begin(), occurrences_.end(), chart_sample,
            [](const Occurrence& value, int64_t sample) { return value.chart_sample < sample; });
        auto own = occurrences_.end();
        for (auto it = begin; it != occurrences_.end() && it->chart_sample == chart_sample; ++it) {
            if (it->played && it->pcm_identity == pcm_identity && it->effective_gain == effective_gain) return false;
            if (epoch_ != 0 && it->candidate_epoch == epoch_ && it->candidate_identity == pcm_identity &&
                it->candidate_gain == effective_gain &&
                std::tie(it->candidate_start, it->candidate_voice) < std::tie(start, voice)) return false;
            if (it->asset_id == asset_id && it->keysound == keysound && it->gain == gain) own = it;
        }
        if (own != occurrences_.end()) {
            own->played = true;
            own->pcm_identity = pcm_identity;
            own->effective_gain = effective_gain;
        }
        return true;
    }

private:
    std::vector<Occurrence> occurrences_;
    std::uint64_t epoch_ = 0;
};

}  // namespace tenriff::app
