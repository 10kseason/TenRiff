#include "app/MenuMusicController.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

#ifdef _WIN32
constexpr wchar_t kMenuMusicAlias[] = L"tenriff_menu_bgm";

std::wstring wide_from_utf8(std::string_view value) {
    const std::filesystem::path path = util::path_from_utf8_lossy(value);
    return path.native();
}

std::string utf8_from_wide(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(), size, nullptr,
                            nullptr) <= 0) {
        return {};
    }
    return utf8;
}

std::string mci_error_text(MCIERROR error) {
    wchar_t buffer[256] = {};
    if (!mciGetErrorStringW(error, buffer, static_cast<UINT>(sizeof(buffer) / sizeof(buffer[0])))) {
        return "Unknown MCI error";
    }
    return utf8_from_wide(buffer);
}

bool run_mci_command(std::wstring_view command, std::string* error = nullptr) {
    const MCIERROR result = mciSendStringW(std::wstring(command).c_str(), nullptr, 0, nullptr);
    if (result == 0) {
        return true;
    }
    if (error) {
        *error = mci_error_text(result);
    }
    return false;
}
#endif

}  // namespace

MenuMusicController::~MenuMusicController() {
    stop();
}

void MenuMusicController::play_looping_file(const std::string& path, double gain) {
#ifdef _WIN32
    const double clamped_gain = std::clamp(gain, 0.0, 1.0);
    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);
    if (path.empty()) {
        close_locked();
        requested_path_.clear();
        open_failed_ = false;
        retry_allowed_at_ = {};
        return;
    }

    const bool requested_path_matches = requested_path_ == path;
    const auto action = menu_music_detail::playback_action(open_, requested_path_matches, clamped_gain);
    if (action == menu_music_detail::PlaybackAction::Close) {
        // Do not create or retain a muted MCI session. Some Windows MCI drivers do not
        // reliably recover when a file was opened and started at volume zero.
        close_locked();
        requested_path_ = path;
        gain_ = 0.0;
        open_failed_ = false;
        retry_allowed_at_ = {};
        return;
    }

    if (action == menu_music_detail::PlaybackAction::UpdateGain) {
        gain_ = clamped_gain;
        apply_gain_locked();
        return;
    }
    if (requested_path_ == path && open_failed_) {
        if (now < retry_allowed_at_) {
            gain_ = clamped_gain;
            return;
        }
        gain_ = clamped_gain;
    }

    close_locked();
    requested_path_ = path;
    gain_ = clamped_gain;
    open_failed_ = false;
    retry_allowed_at_ = {};

    std::string error;
    const std::wstring wide_path = wide_from_utf8(path);
    if (wide_path.empty()) {
        std::cerr << "[warn] Failed to resolve menu music path: " << path << std::endl;
        open_failed_ = true;
        retry_allowed_at_ = now + std::chrono::seconds(1);
        return;
    }

    std::wstring open_command = L"open \"" + wide_path + L"\" type mpegvideo alias " + std::wstring(kMenuMusicAlias);
    if (!run_mci_command(open_command, &error)) {
        open_command = L"open \"" + wide_path + L"\" alias " + std::wstring(kMenuMusicAlias);
        if (!run_mci_command(open_command, &error)) {
            std::cerr << "[warn] Failed to open menu music: " << path << " (" << error << ")" << std::endl;
            open_failed_ = true;
            retry_allowed_at_ = now + std::chrono::seconds(1);
            return;
        }
    }

    open_ = true;
    open_failed_ = false;
    current_path_ = path;
    apply_gain_locked();
    if (!run_mci_command(L"play " + std::wstring(kMenuMusicAlias) + L" from 0 repeat", &error)) {
        std::cerr << "[warn] Failed to start menu music: " << path << " (" << error << ")" << std::endl;
        close_locked();
        open_failed_ = true;
        retry_allowed_at_ = now + std::chrono::seconds(1);
    }
#else
    static_cast<void>(path);
    static_cast<void>(gain);
#endif
}

void MenuMusicController::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    close_locked();
    requested_path_.clear();
    open_failed_ = false;
    retry_allowed_at_ = {};
}

void MenuMusicController::close_locked() {
#ifdef _WIN32
    if (open_) {
        run_mci_command(L"stop " + std::wstring(kMenuMusicAlias));
        run_mci_command(L"close " + std::wstring(kMenuMusicAlias));
    }
#endif
    open_ = false;
    current_path_.clear();
}

void MenuMusicController::apply_gain_locked() {
#ifdef _WIN32
    if (!open_) {
        return;
    }

    const int volume = static_cast<int>(std::lround(std::clamp(gain_, 0.0, 1.0) * 1000.0));
    run_mci_command(L"setaudio " + std::wstring(kMenuMusicAlias) + L" volume to " + std::to_wstring(volume));
#endif
}

}  // namespace tenriff::app
