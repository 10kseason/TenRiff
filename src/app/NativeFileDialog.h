#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#endif

namespace tenriff::app {

// Menu dispatch owns this context. The modal shell loop must not replay input
// collected by the separate input/render threads after the picker closes.
class NativeFileDialogInputScope {
public:
    using Reset = void (*)(void*);
    NativeFileDialogInputScope(void* owner, Reset reset)
        : previous_(current_), owner_(owner), reset_(reset) { current_ = this; }
    ~NativeFileDialogInputScope() { current_ = previous_; }
    NativeFileDialogInputScope(const NativeFileDialogInputScope&) = delete;
    NativeFileDialogInputScope& operator=(const NativeFileDialogInputScope&) = delete;
    static void discard_modal_input() {
        if (current_) current_->reset_(current_->owner_);
    }
private:
    inline static thread_local NativeFileDialogInputScope* current_ = nullptr;
    NativeFileDialogInputScope* previous_;
    void* owner_;
    Reset reset_;
};

#ifdef _WIN32
inline HRESULT show_native_file_dialog(IFileDialog* dialog) {
    static thread_local bool active = false;
    if (active) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    active = true;
    struct Completion {
        bool& active;
        ~Completion() {
            NativeFileDialogInputScope::discard_modal_input();
            active = false;
        }
    } completion{active};
    HWND owner = GetForegroundWindow();
    DWORD process = 0;
    if (owner) GetWindowThreadProcessId(owner, &process);
    if (process != GetCurrentProcessId()) owner = nullptr;
    if (!owner) {
        // Focus can change between menu dispatch and opening the shell. Keep
        // modal ownership on our primary window even in that transition.
        EnumWindows([](HWND window, LPARAM target) -> BOOL {
            DWORD process_id = 0;
            GetWindowThreadProcessId(window, &process_id);
            wchar_t class_name[64]{};
            GetClassNameW(window, class_name, 64);
            if (process_id == GetCurrentProcessId() && IsWindowVisible(window) &&
                lstrcmpW(class_name, L"TenRiffMenuWindow") == 0) {
                *reinterpret_cast<HWND*>(target) = window;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&owner));
    }
    // Owning the dialog disables the game window while the shell is modal.
    // Never disable or attach to a foreground window from another process.
    return dialog->Show(owner);
}
#endif

} // namespace tenriff::app
