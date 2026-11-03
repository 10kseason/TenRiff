#pragma once

#include "app/NativeFileDialog.h"

#include <string>
#include <thread>
#include <utility>

namespace tenriff::app {

struct MenuAudioFileChoice {
    std::string path;
    std::string error;
    bool cancelled = false;
};

#ifdef _WIN32
// WASAPI initializes the menu caller as MTA while scene music is open. Shell
// file dialogs require STA, so keep every shell interface on a request-scoped
// worker instead of changing or tearing down the caller's live audio apartment.
template<class Choose>
MenuAudioFileChoice run_menu_audio_picker_sta(Choose&& choose) {
    MenuAudioFileChoice result;
    try {
        std::thread picker([&] {
            try {
                const NativeFileDialogApartment apartment;
                if (FAILED(apartment.result())) {
                    result.error = "Audio picker COM initialization failed.";
                    return;
                }
                result = choose();
            } catch (...) {
                result.error = "Audio file selection failed.";
            }
        });
        picker.join();
    } catch (...) {
        result.error = "Audio picker thread could not start.";
    }
    // Show runs on the STA worker, while this thread owns the menu input scope.
    // Drain here as well: the worker's thread-local scope cannot reset the menu.
    input::native_modal_input_fence.close(GetTickCount64());
    NativeFileDialogInputScope::discard_modal_input();
    return result;
}
#endif

}  // namespace tenriff::app
