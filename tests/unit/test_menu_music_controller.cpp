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
