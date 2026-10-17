#pragma once
#include "input/ModalInputFence.h"

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
// Declare before shell interfaces so every early return releases them first.
class NativeFileDialogApartment {
public:
    NativeFileDialogApartment() : result_(CoInitializeEx(nullptr,
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~NativeFileDialogApartment() { if (SUCCEEDED(result_)) CoUninitialize(); }
    HRESULT result() const { return result_; }
    NativeFileDialogApartment(const NativeFileDialogApartment&) = delete;
    NativeFileDialogApartment& operator=(const NativeFileDialogApartment&) = delete;
private:
    HRESULT result_;
};

inline HWND native_file_dialog_owner() {
    HWND owner = nullptr;
    // A shell picker in the foreground is never the next picker's owner.
    EnumWindows([](HWND window, LPARAM target) -> BOOL {
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        wchar_t type[64]{};
        GetClassNameW(window, type, 64);
        if (process == GetCurrentProcessId() && IsWindowVisible(window) &&
            lstrcmpW(type, L"TenRiffMenuWindow") == 0) {
            *reinterpret_cast<HWND*>(target) = window;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&owner));
    return owner;
}

inline HRESULT show_native_file_dialog(IFileDialog* dialog) {
    if (!dialog) return E_INVALIDARG;
    static thread_local IFileDialog* active = nullptr;
    static thread_local unsigned depth = 0;
    // A newer explicit request cancels our previous picker. Permit one nested
    // replacement only: shell callbacks cannot recursively spawn windows.
    if (depth >= 2 || active == dialog) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    IFileDialog* previous = active;
    const HWND owner = native_file_dialog_owner();
    const bool restore_owner = depth == 0 && owner && IsWindowEnabled(owner);
    active = dialog;
    ++depth;
    struct Completion {
        IFileDialog*& active;
        IFileDialog* previous;
        unsigned& depth;
        HWND owner;
        bool restore_owner;
        ~Completion() {
            active = previous;
            --depth;
            // The render thread owns the HWND. No menu/render locks may be
            // held across Show or this synchronous owner restoration.
            if (restore_owner && IsWindow(owner)) {
                EnableWindow(owner, TRUE);
                SetForegroundWindow(owner);
            }
            input::native_modal_input_fence.close(GetTickCount64());
            NativeFileDialogInputScope::discard_modal_input();
        }
    } completion{active, previous, depth, owner, restore_owner};
    // Reserve the slot before Close as even cancellation can pump COM calls.
    if (previous && FAILED(previous->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED))))
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    return dialog->Show(owner);
}
#endif

} // namespace tenriff::app
