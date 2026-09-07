#include "doctest/doctest.h"
#include "app/GameSession.h"

#include <algorithm>
#include <memory>

namespace tenriff::app {

// Exercise the production input-to-voice-to-mixer path without starting a device,
// input thread, decoder, profile or record writer.
struct GameSessionAudioTestAccess {
    static void check_hold_release(bool release_required, int64_t release_sample) {
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 144000;
        gameplay::NoteEvent hold{1, 48000, 96000};
        hold.release_required = release_required;
        hold.audio_asset_id = 0;
        session->chart_.notes.push_back(hold);
        gameplay::GameplayConfig config;
        config.practice_no_fail_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        session->lane_activity_.assign(1, 0);
        session->lane_pressed_.assign(1, 0);
        session->synthetic_tones_enabled_ = false;
        session->chart_audio_assets_.resize(1);
        session->chart_audio_assets_[0].clip.samples = std::make_shared<const std::vector<float>>(960, 0.25f);
        session->last_keysound_chart_samples_.assign(1, -1);

        session->dispatch_lane_input(1, input::InputState::Pressed, 48000, 48000);
        REQUIRE(session->chart_audio_voices_.size() == 1);
        std::vector<float> head(960, 0);
        session->mix_chart_audio(head.data(), 480, 48000);
        CHECK(std::any_of(head.begin(), head.end(), [](float x) { return x != 0; }));
        CHECK(session->chart_audio_voices_.empty());

        session->engine_->advance(release_sample - 1);
        session->dispatch_lane_input(1, input::InputState::Released, release_sample, release_sample);
        session->engine_->advance(100800);
        session->schedule_chart_audio(101280);
        std::vector<float> tail(960, 0);
        session->mix_chart_audio(tail.data(), 480, release_sample);
        CHECK(session->chart_audio_voices_.empty());
        CHECK(std::all_of(tail.begin(), tail.end(), [](float x) { return x == 0; }));
    }
};

} // namespace tenriff::app

TEST_CASE("long note release never starts another keysound through the production mixer") {
    for (bool release_required : {false, true}) {
        for (int64_t sample : {72000, 96000, 100800}) {
            tenriff::app::GameSessionAudioTestAccess::check_hold_release(release_required, sample);
        }
    }
}
