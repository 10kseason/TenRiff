// Explicit local benchmark: writes only a new output directory, never a user profile.
#include "app/MenuApp.h"
#include "app/DifficultyTable.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "app/SongSourceAggregate.h"
#include "config/SimpleJson.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace tenriff::app {
struct MenuAppSongSourceTestAccess {
    static void run(const std::filesystem::path& output) {
        namespace fs = std::filesystem;
        using Clock = std::chrono::steady_clock;
        std::ofstream metrics(output / "metrics.csv");
        metrics << "phase,repeat,milliseconds,entries\n";
        const auto measure = [&](const char* name, int repeat, auto&& action) {
            const auto start = Clock::now();
            const auto count = action();
            const auto ms = std::chrono::duration<double, std::milli>(Clock::now()-start).count();
            metrics << name << ',' << repeat << ',' << ms << ',' << count << '\n';
            metrics.flush();
            std::cout << name << ' ' << repeat << " ms=" << ms << " entries=" << count << std::endl;
        };
        const auto songs = output / "songs";
        for (int folder = 0; folder < 200; ++folder) {
            const auto dir = songs / ("song-" + std::to_string(folder));
            fs::create_directories(dir);
            for (int item = 0; item < 10; ++item) {
                std::ofstream chart(dir / ("chart-" + std::to_string(item) + ".bms"));
                chart << "#PLAYER 1\n#TITLE Benchmark " << folder << ' ' << item
                      << "\n#ARTIST Fixture\n#BPM 150\n#PLAYLEVEL 10\n#WAV01 sample.wav\n";
                for (int measure = 1; measure <= 32; ++measure) {
                    chart << '#' << (measure < 10 ? "00" : "0") << measure
                          << "11:01010101\n";
                }
                std::ofstream(dir / ("asset-" + std::to_string(item) + ".txt")) << "not a chart\n";
            }
        }
        SongIndexOptions options;
        options.calculate_difficulty = false;
        SongIndex index;
        std::vector<std::string> warnings;
        for (int i = 0; i < 3; ++i) {
            measure("folder_scan", i, [&] {
                warnings.clear();
                index = scan_songs(songs.u8string(), nullptr, warnings, {}, options);
                if (index.entries.size() != 2000) throw std::runtime_error("Incomplete scan");
                return index.entries.size();
            });
        }
        // A larger warm library exercises parsing and menu table selection without
        // making first-scan file creation dominate the repeatable benchmark.
        const auto original = index.entries;
        for (int copy = 1; copy < 5; ++copy) {
            for (const auto& entry : original) {
                auto duplicated = entry;
                duplicated.path = "copy-" + std::to_string(copy) + '/' + entry.path;
                index.entries.push_back(std::move(duplicated));
            }
        }
        const auto profile = output / "profile";
        fs::create_directories(profile);
        const auto cache = song_index_cache_path_for_source(profile.u8string(), songs.u8string());
        std::string error;
        if (!save_song_index(cache, index, options, &error)) throw std::runtime_error(error);
        for (int i = 0; i < 5; ++i) {
            measure("cache_load", i, [&] {
                const auto loaded = load_song_index(cache, options);
                if (!loaded.loaded_from_file || loaded.index.entries.size() != index.entries.size())
                    throw std::runtime_error("Invalid cache load");
                return loaded.index.entries.size();
            });
        }
        const auto table_data = [&] {
            config::JsonArray rows;
            for (const auto& entry : original) {
                config::JsonObject row;
                row["md5"] = config::JsonValue(entry.md5);
                row["level"] = config::JsonValue(std::string("1"));
                rows.emplace_back(std::move(row));
            }
            return config::json_stringify(config::JsonValue(std::move(rows)), 0);
        }();
        const auto fetch = [&](std::string_view url, std::size_t) {
            DifficultyTableHttpResponse response;
            response.status_code = 200;
            response.final_url = std::string(url);
            response.body = url.find("data.json") != std::string_view::npos ? table_data :
                R"({"name":"Fixture","symbol":"T","data_url":"data.json","level_order":["1","2"]})";
            return response;
        };
        auto instance = std::make_unique<MenuApp>();
        auto& app = *instance;
        app.profile_dir_ = profile.u8string();
        app.config_.mode.song_index_profile = "safe";
        app.config_.mode.calculate_song_index_difficulty = false;
        app.config_.ui.song_sources_initialized = true;
        app.config_.ui.recent_song_sources = {songs.u8string()};
        for (int i = 0; i < 3; ++i) {
            measure("folder_open", i, [&] {
                app.switch_song_source(songs.u8string(), false);
                return app.indexed_songs_.size();
            });
        }
        for (int i = 0; i < 5; ++i) {
            measure("table_select", i, [&] {
                if (!app.apply_difficulty_table_url("https://fixture.invalid/header.json", fetch))
                    throw std::runtime_error("Table selection failed");
                if (app.indexed_songs_.size() != index.entries.size() ||
                    app.indexed_songs_.front().difficulty_table_level != "1")
                    throw std::runtime_error("Table labels mismatch");
                return app.indexed_songs_.size();
            });
            measure("native_select", i, [&] {
                app.select_native_difficulty_levels();
                if (app.indexed_songs_.size() != index.entries.size() ||
                    !app.indexed_songs_.front().difficulty_table_level.empty())
                    throw std::runtime_error("Native labels mismatch");
                return app.indexed_songs_.size();
            });
        }
    }
};
}

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Supply a new isolated output directory");
        const auto output = std::filesystem::absolute(argv[1]).lexically_normal();
        if (std::filesystem::exists(output)) throw std::runtime_error("Output already exists");
        std::filesystem::create_directories(output);
        tenriff::app::MenuAppSongSourceTestAccess::run(output);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
