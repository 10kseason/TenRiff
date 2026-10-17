#include "doctest/doctest.h"

#include "app/MenuMusicController.h"

using tenriff::app::menu_music_detail::PlaybackAction;
using tenriff::app::menu_music_detail::playback_action;

TEST_CASE("menu music does not retain a muted playback session") {
    CHECK(playback_action(false, false, 0.0) == PlaybackAction::Close);
    CHECK(playback_action(true, true, 0.0) == PlaybackAction::Close);
    CHECK(playback_action(true, true, -0.1) == PlaybackAction::Close);
}

TEST_CASE("menu music opens after a muted startup and only updates an active matching track") {
    CHECK(playback_action(false, true, 1.0) == PlaybackAction::Open);
    CHECK(playback_action(true, false, 1.0) == PlaybackAction::Open);
    CHECK(playback_action(true, true, 1.0) == PlaybackAction::UpdateGain);
}

TEST_CASE("scene music retains output identity and hardware rate while sharing WASAPI") {
    using namespace tenriff::app::menu_music_detail;
    tenriff::audio::AudioConfig config;
    config.device_id = "selected-endpoint";
    config.sample_rate = 96000;
    const auto shared = output_config(config);
    CHECK_FALSE(shared.exclusive_mode);
    CHECK(shared.device_id == config.device_id);
    CHECK(shared.sample_rate == 96000);
    CHECK(same_output(shared, output_config(config)));
    config.device_id = "other-endpoint";
    CHECK_FALSE(same_output(shared, output_config(config)));
    config.backend = tenriff::audio::AudioBackend::ASIO;
    config.asio_driver = "selected-driver";
    CHECK(output_config(config).asio_driver == config.asio_driver);
    CHECK(output_config(config).sample_rate == 96000);
    CHECK_FALSE(same_output(shared, output_config(config)));
}

#ifdef _WIN32
#include "../support/MockAsioDriver.h"
#include <thread>

namespace tenriff::app {
struct MenuMusicControllerTestAccess {
    static void ready(MenuMusicController& music, const std::string& path, uint32_t rate,
                      const std::shared_ptr<tenriff::tests::MockState>& state) {
        music.audio_.asio_test_factory_ = tenriff::tests::mock_factory(state);
        std::promise<MenuMusicController::DecodedTrack> ready;
        ready.set_value({path, rate, std::make_shared<const std::vector<float>>(1000, 0.5f), {}});
        music.decoding_ = ready.get_future();
    }
    static bool open(const MenuMusicController& music) { return music.open_; }
    static bool failed(const MenuMusicController& music) { return music.open_failed_; }
    static float gain(const MenuMusicController& music) { return music.output_gain_.load(); }
    static std::string driver(const MenuMusicController& music) { return music.requested_config_.asio_driver; }
    static void pending(MenuMusicController& music, std::promise<MenuMusicController::DecodedTrack>& result) {
        music.decoding_ = result.get_future();
    }
    static void check_pending_stop() {
        MenuMusicController music;
        audio::AudioConfig config; config.backend = audio::AudioBackend::ASIO;
        auto state = std::make_shared<tenriff::tests::MockState>();
        music.audio_.asio_test_factory_ = tenriff::tests::mock_factory(state);
        std::promise<MenuMusicController::DecodedTrack> promise;
        pending(music, promise);
        music.play_looping_file("old-scene", 0.5, config);
        music.stop();
        promise.set_value({"old-scene", config.sample_rate,
            std::make_shared<const std::vector<float>>(1000, 0.5f), {}});
        CHECK_FALSE(state->initialized);
        CHECK_FALSE(open(music));
        // A later zero-volume scene cannot start the completed old request.
        music.play_looping_file("result-scene", 0.0, config);
        CHECK_FALSE(state->initialized);
    }
};
}

TEST_CASE("scene music uses selected ASIO and preserves its cursor through focus mute") {
    using namespace tenriff;
    using Access = app::MenuMusicControllerTestAccess;
    app::MenuMusicController music;
    audio::AudioConfig config; config.backend = audio::AudioBackend::ASIO;
    config.asio_driver = "scene-driver";
    auto state = std::make_shared<tests::MockState>();
    Access::ready(music, "title", config.sample_rate, state);
    music.play_looping_file("title", 0.5, config);
    REQUIRE(Access::open(music)); CHECK(Access::driver(music) == "scene-driver");
    state->tick(); CHECK(state->output[0][0][1] == 0x20);
    music.set_output_muted(true); state->tick();
    CHECK(Access::gain(music) == 0.0f); CHECK(state->output[0][0][1] == 0);
    music.set_output_muted(false); state->tick();
    CHECK(state->output[0][0][1] == 0x20); CHECK(state->starts == 1);
    music.play_looping_file("title", 0.25, config); CHECK(state->starts == 1);
    music.stop(); CHECK(state->releases == 1); CHECK(state->correct_owner);
    // A preview can claim ASIO only after the scene stream relinquishes it.
    audio::AsioBackend preview(tests::mock_factory(std::make_shared<tests::MockState>()));
    CHECK(preview.initialize(config, [](float*, uint32_t, int64_t, int64_t){}) == audio::AudioResult::Success);
}

TEST_CASE("scene music reopens same track on output change and recovers from zero volume or failure") {
    using namespace tenriff;
    using Access = app::MenuMusicControllerTestAccess;
    app::MenuMusicController music;
    audio::AudioConfig config; config.backend = audio::AudioBackend::ASIO;
    config.asio_driver = "first";
    auto first = std::make_shared<tests::MockState>();
    Access::ready(music, "result", config.sample_rate, first);
    music.play_looping_file("result", 0.0, config); CHECK_FALSE(first->initialized);
    music.play_looping_file("result", 0.5, config); REQUIRE(Access::open(music));
    auto second = std::make_shared<tests::MockState>(); second->fail_start = true;
    config.asio_driver = "missing-or-failed";
    Access::ready(music, "result", config.sample_rate, second);
    music.play_looping_file("result", 0.5, config);
    CHECK(first->releases == 1); CHECK(second->releases == 1);
    CHECK_FALSE(Access::open(music)); CHECK(Access::failed(music));
    auto third = std::make_shared<tests::MockState>();
    config.asio_driver = "reconnected";
    Access::ready(music, "result", config.sample_rate, third);
    music.play_looping_file("result", 0.5, config);
    CHECK(Access::open(music)); CHECK_FALSE(Access::failed(music));
    CHECK(Access::driver(music) == "reconnected");
}

TEST_CASE("scene music stop prevents pending decode from opening a device") {
    tenriff::app::MenuMusicControllerTestAccess::check_pending_stop();
}
#endif
