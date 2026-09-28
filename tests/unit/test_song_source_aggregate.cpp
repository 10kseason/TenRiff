#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "app/MenuSongUtils.h"
#include "app/SongIndexerThread.h"
#include "app/SongSourceAggregate.h"
#include "util/Utf8Compat.h"

namespace {
namespace fs = std::filesystem;
using namespace tenriff::app;

struct AggregateFixture {
    fs::path base;
    AggregateFixture() {
        const auto parent = fs::temp_directory_path() / "tenriff_source_aggregate_tests";
        fs::create_directories(parent);
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = parent / (std::to_string(stamp) + "-" + std::to_string(attempt));
            std::error_code ec;
            if (fs::create_directory(candidate, ec)) { base = std::move(candidate); break; }
        }
        REQUIRE_FALSE(base.empty());
    }
    ~AggregateFixture() { std::error_code ignored; fs::remove_all(base, ignored); }
    fs::path source(const std::string& name) {
        auto value = base / fs::u8path(name);
        fs::create_directories(value);
        return value;
    }
    std::string profile() const { return (base / "profile").u8string(); }
    std::string cache(const fs::path& source) const {
        return song_index_cache_path_for_source(profile(), source.u8string());
    }
};

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << text;
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_chart(const fs::path& path, const std::string& title) {
    write_text(path, "#TITLE " + title + "\n#ARTIST Fixture\n#BPM 150\n#PLAYLEVEL 5\n#00111:01\n");
}

SongEntry cached_entry(std::string path, std::string title) {
    SongEntry entry;
    entry.path = std::move(path);
    entry.title = std::move(title);
    entry.format = "bms";
    entry.key_count = 10;
    entry.level = 5;
    entry.native_level = 5;
    return entry;
}

std::string source_key(const fs::path& source) {
    return menu_songs::normalize_path_key(fs::weakly_canonical(source));
}

bool wait_until_finished(SongIndexerThread& indexer) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (indexer.is_running() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return !indexer.is_running();
}
}  // namespace

TEST_CASE("ALL SONG aggregates cached roots and deduplicates overlapping canonical chart paths") {
    AggregateFixture f;
    const auto library = f.source(u8"음악_ライブラリー");
    const auto nested = library / "nested";
    write_chart(nested / "same.bms", "Actual file");
    write_chart(library / "second.bms", "Second actual");
    write_text(nested / "cover.png", "fixture");
    write_text(nested / "preview.wav", "fixture");
    auto first = cached_entry("nested/same.bms", "First cached winner");
    first.background_preview_path = "nested/cover.png";
    first.audio_preview_path = (nested / "preview.wav").u8string();
    SongIndex parent{{first, cached_entry("second.bms", "Second cached")}};
    REQUIRE(save_song_index(f.cache(library), parent));
    REQUIRE(save_song_index(f.cache(nested), SongIndex{{cached_entry("same.bms", "Nested duplicate")}}));
    const auto before_parent = read_text(fs::u8path(f.cache(library)));
    const auto before_nested = read_text(fs::u8path(f.cache(nested)));

    const auto combined = aggregate_song_sources(
        {library.u8string(), (library / ".").u8string(), nested.u8string()}, f.profile());
    CHECK_FALSE(combined.cancelled);
    CHECK(combined.cached_sources == 2);
    CHECK(combined.scanned_sources == 0);
    CHECK(combined.warnings.empty());
    REQUIRE(combined.index.entries.size() == 2);
    CHECK(combined.source_counts.at(source_key(library)) == 2);
    CHECK(combined.source_counts.at(source_key(nested)) == 1);
    const auto& entry = combined.index.entries.front();
    CHECK(entry.title == "First cached winner");
    CHECK(fs::u8path(entry.path) == fs::weakly_canonical(nested / "same.bms"));
    CHECK(fs::u8path(entry.background_preview_path) == fs::weakly_canonical(nested / "cover.png"));
    CHECK(fs::u8path(entry.audio_preview_path) == fs::weakly_canonical(nested / "preview.wav"));
    CHECK(read_text(fs::u8path(f.cache(library))) == before_parent);
    CHECK(read_text(fs::u8path(f.cache(nested))) == before_nested);
    CHECK(load_song_index(f.cache(library)).index.entries.front().path == "nested/same.bms");
}

TEST_CASE("ALL SONG preserves distinct copies and identical filenames in separate roots") {
    AggregateFixture f;
    const auto first = f.source("first"), second = f.source("second");
    write_chart(first / "same.bms", "Same bytes");
    write_chart(second / "same.bms", "Same bytes");
    const auto combined = aggregate_song_sources({first.u8string(), second.u8string()}, f.profile());
    REQUIRE(combined.index.entries.size() == 2);
    CHECK(combined.scanned_sources == 2);
    CHECK(combined.index.entries[0].path != combined.index.entries[1].path);
    CHECK(combined.index.entries[0].md5 == combined.index.entries[1].md5);
    for (const auto& source : {first, second}) {
        const auto cached = load_song_index(f.cache(source));
        REQUIRE(cached.loaded_from_file);
        REQUIRE(cached.index.entries.size() == 1);
        CHECK(cached.index.entries.front().path == "same.bms");
    }
}

TEST_CASE("ALL SONG uses a valid empty cache and scans only roots missing a compatible cache") {
    AggregateFixture f;
    const auto cached = f.source("cached"), cold = f.source("cold");
    const auto missing = f.base / "removed";
    write_chart(cached / "new-unscanned.bms", "Not until F5");
    write_chart(cold / "fresh.bms", "Fresh scanned chart");
    REQUIRE(save_song_index(f.cache(cached), SongIndex{}));
    REQUIRE(save_song_index(f.cache(missing), SongIndex{{cached_entry("stale.bms", "Offline cache")}}));
    std::vector<int> sources;
    const auto combined = aggregate_song_sources(
        {cached.u8string(), cold.u8string(), missing.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            sources.push_back(update.source_index);
            CHECK(update.source_total == 3);
        });
    CHECK(combined.cached_sources == 1);
    CHECK(combined.scanned_sources == 1);
    CHECK(combined.missing_sources == 1);
    REQUIRE(combined.index.entries.size() == 1);
    CHECK(combined.index.entries.front().title == "Fresh scanned chart");
    CHECK(combined.source_counts.at(source_key(cached)) == 0);
    CHECK(combined.source_counts.at(source_key(cold)) == 1);
    CHECK(combined.source_counts.at(source_key(missing)) == 0);
    REQUIRE(combined.warnings.size() == 1);
    CHECK(combined.warnings.front().find("Songs path not found") != std::string::npos);
    REQUIRE_FALSE(sources.empty());
    for (std::size_t i = 1; i < sources.size(); ++i) CHECK(sources[i] >= sources[i - 1]);
}

TEST_CASE("ALL SONG falls back to legacy caches and rescans invalid or profile-incompatible caches") {
    AggregateFixture f;
    const auto legacy = f.source("legacy"), invalid = f.source("invalid"), mismatch = f.source("mismatch");
    write_chart(legacy / "legacy.bms", "Actual legacy");
    write_chart(invalid / "invalid.bms", "Scanned invalid");
    write_chart(mismatch / "mismatch.bms", "Scanned mismatch");
    REQUIRE(save_song_index(legacy_song_index_cache_path_for_source(legacy.u8string()),
                            SongIndex{{cached_entry("legacy.bms", "Legacy cached title")}}));
    write_text(fs::u8path(f.cache(invalid)), "{broken json");
    SongIndexOptions fast;
    fast.profile = SongIndexProfile::Fast;
    REQUIRE(save_song_index(f.cache(mismatch), SongIndex{{cached_entry("mismatch.bms", "Wrong profile")}}, fast));

    const auto combined = aggregate_song_sources({legacy.u8string(), invalid.u8string(), mismatch.u8string()}, f.profile());
    CHECK(combined.cached_sources == 1);
    CHECK(combined.scanned_sources == 2);
    REQUIRE(combined.index.entries.size() == 3);
    CHECK(combined.index.entries[0].title == "Legacy cached title");
    CHECK(combined.index.entries[1].title == "Scanned invalid");
    CHECK(combined.index.entries[2].title == "Scanned mismatch");
    CHECK_FALSE(combined.warnings.empty());
    CHECK(load_song_index(f.cache(invalid)).loaded_from_file);
    CHECK(load_song_index(f.cache(mismatch)).loaded_from_file);
}

TEST_CASE("ALL SONG force rescan bypasses valid metadata caches") {
    AggregateFixture f;
    const auto root = f.source("root");
    write_chart(root / "song.bms", "Original");
    const auto first = aggregate_song_sources({root.u8string()}, f.profile());
    REQUIRE(first.scanned_sources == 1);
    auto cache = load_song_index(f.cache(root));
    REQUIRE(cache.loaded_from_file);
    cache.index.entries.front().title = "Stale cached metadata";
    REQUIRE(save_song_index(f.cache(root), cache.index));
    const auto reused = aggregate_song_sources({root.u8string()}, f.profile());
    REQUIRE(reused.index.entries.size() == 1);
    CHECK(reused.index.entries.front().title == "Stale cached metadata");
    const auto rescanned = aggregate_song_sources({root.u8string()}, f.profile(), {}, {}, {}, true);
    REQUIRE(rescanned.index.entries.size() == 1);
    CHECK(rescanned.scanned_sources == 1);
    CHECK(rescanned.index.entries.front().title == "Original");
}

TEST_CASE("ALL SONG cancellation drops partial aggregates and preserves existing caches") {
    AggregateFixture f;
    const auto first = f.source("first"), second = f.source("second");
    write_chart(first / "one.bms", "One");
    write_chart(second / "two.bms", "Two");
    REQUIRE(save_song_index(f.cache(first), SongIndex{{cached_entry("one.bms", "Old first")}}));
    REQUIRE(save_song_index(f.cache(second), SongIndex{{cached_entry("two.bms", "Old second")}}));
    const auto old_second = read_text(fs::u8path(f.cache(second)));
    bool cancel = false;
    const auto combined = aggregate_song_sources(
        {first.u8string(), second.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            if (update.source_index == 2 && update.source.stage == SongIndexProgressStage::BuildingMetadata) cancel = true;
        }, [&] { return cancel; }, true);
    CHECK(combined.cancelled);
    CHECK(combined.index.entries.empty());
    CHECK(combined.source_counts.empty());
    CHECK(read_text(fs::u8path(f.cache(second))) == old_second);
    // A completed earlier root may update its own cache, even though the final
    // aggregate is not published. It must still be a complete usable cache.
    CHECK(load_song_index(f.cache(first)).loaded_from_file);
    CHECK(load_song_index(f.cache(second)).loaded_from_file);
}

TEST_CASE("atomic source cache cancellation retains byte-identical previous cache") {
    AggregateFixture f;
    const auto root = f.source("root");
    const auto path = f.cache(root);
    const SongIndex old{{cached_entry("old.bms", "Old cache")}};
    const SongIndex replacement{{cached_entry("new.bms", "New cache")}};
    REQUIRE(save_song_index(path, old));
    const auto before = read_text(fs::u8path(path));
    for (bool cancel_at_end : {false, true}) {
        bool cancel = false;
        std::string error;
        CHECK_FALSE(save_song_source_cache_atomically(path, replacement, {}, &error,
            [&](const SongIndexProgress& update) {
                if (update.stage == SongIndexProgressStage::SavingCache &&
                    (!cancel_at_end || update.processed == update.total)) cancel = true;
            }, [&] { return cancel; }));
        CHECK_FALSE(error.empty());
        CHECK(read_text(fs::u8path(path)) == before);
    }
    for (const auto& entry : fs::directory_iterator(fs::u8path(path).parent_path())) {
        CHECK(entry.path().extension() != ".tmp");
    }
    REQUIRE(save_song_source_cache_atomically(path, replacement));
    CHECK(load_song_index(path).index.entries.front().title == "New cache");
}

TEST_CASE("ALL SONG source disappearance during scan cannot replace its previous cache with an empty index") {
    AggregateFixture f;
    const auto root = f.source("removed-during-scan");
    write_chart(root / "song.bms", "Before removal");
    REQUIRE(save_song_index(f.cache(root), SongIndex{{cached_entry("song.bms", "Previous cache")}}));
    const auto before = read_text(fs::u8path(f.cache(root)));
    bool removed = false;
    const auto combined = aggregate_song_sources({root.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            if (!removed && update.source.stage == SongIndexProgressStage::BuildingMetadata) {
                // This directory was created by this fixture and contains no user data.
                fs::remove_all(root);
                removed = true;
            }
        }, {}, true);
    CHECK(removed);
    CHECK_FALSE(combined.cancelled);
    CHECK(combined.index.entries.empty());
    CHECK(combined.missing_sources == 1);
    CHECK_FALSE(combined.warnings.empty());
    CHECK(read_text(fs::u8path(f.cache(root))) == before);
}

TEST_CASE("aggregate indexer returns source counts and stop preserves caller's prior index") {
    AggregateFixture f;
    const auto root = f.source("root");
    write_chart(root / "song.bms", "Indexed");
    SongIndexerThread indexer;
    REQUIRE(indexer.start_sources({root.u8string()}, f.profile()));
    REQUIRE(wait_until_finished(indexer));
    SongIndex visible{{cached_entry("prior.bms", "Previous visible index")}};
    std::vector<std::string> warnings;
    std::unordered_map<std::string, int> counts;
    REQUIRE(indexer.poll_result(visible, warnings, &counts));
    REQUIRE(visible.entries.size() == 1);
    CHECK(visible.entries.front().title == "Indexed");
    CHECK(counts.at(source_key(root)) == 1);
    CHECK(indexer.progress().source_total == 1);
    CHECK(indexer.progress().source_index == 1);
    REQUIRE(indexer.start_sources({root.u8string()}, f.profile(), {}, true));
    indexer.stop();
    CHECK_FALSE(indexer.poll_result(visible, warnings, &counts));
    CHECK(visible.entries.front().title == "Indexed");
    CHECK(counts.at(source_key(root)) == 1);
    REQUIRE(indexer.start(root.u8string(), f.cache(root)));
    REQUIRE(wait_until_finished(indexer));
    REQUIRE(indexer.poll_result(visible, warnings, &counts));
    CHECK(counts.empty());
}

TEST_CASE("ALL SONG empty roots complete without scanning default or current directories") {
    AggregateFixture f;
    write_chart(f.base / "unregistered.bms", "Do not scan");
    const auto combined = aggregate_song_sources({"", ""}, f.profile());
    CHECK_FALSE(combined.cancelled);
    CHECK(combined.index.entries.empty());
    CHECK(combined.source_counts.empty());
    CHECK(combined.scanned_sources == 0);
    CHECK(combined.cached_sources == 0);
    CHECK(combined.warnings.empty());
}
