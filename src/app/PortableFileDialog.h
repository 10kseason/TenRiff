#pragma once

#include <filesystem>
#include <string>

#include "util/Utf8Compat.h"

#ifdef _WIN32
#include <shobjidl.h>
#include <wrl/client.h>
#endif

namespace tenriff::app {

struct PortableFileChoice {
    std::string path;
    std::string error;
    bool cancelled = false;
};

// Save-as always chooses a new file: accepting the shell's overwrite prompt
// previously conflicted with the archive writer's no-overwrite contract.
inline PortableFileChoice choose_portable_file(bool save, bool profile = false) {
    PortableFileChoice result;
#ifdef _WIN32
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    struct ComScope { bool owns; ~ComScope() { if (owns) CoUninitialize(); } } scope{SUCCEEDED(init)};
    Microsoft::WRL::ComPtr<IFileDialog> dialog;
    HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
        nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        hr = dialog->GetOptions(&options);
        if (SUCCEEDED(hr)) hr = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
            (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
        const wchar_t* extension = profile ? L"trprofile" : L"trskin";
        const COMDLG_FILTERSPEC filter = profile
            ? COMDLG_FILTERSPEC{L"TenRiff Settings (*.trprofile)", L"*.trprofile"}
            : COMDLG_FILTERSPEC{L"TenRiff Skin Preset (*.trskin)", L"*.trskin"};
        if (SUCCEEDED(hr)) hr = dialog->SetFileTypes(1, &filter);
        if (SUCCEEDED(hr)) hr = dialog->SetDefaultExtension(extension);
        if (save && SUCCEEDED(hr)) hr = dialog->SetFileName(profile ? L"TenRiff Settings.trprofile" : L"My Skin Preset.trskin");
        if (SUCCEEDED(hr)) hr = dialog->Show(nullptr);
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) { result.cancelled = true; return result; }
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (SUCCEEDED(hr)) hr = dialog->GetResult(&item);
        PWSTR raw = nullptr;
        if (SUCCEEDED(hr)) hr = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);
        if (SUCCEEDED(hr) && raw) {
            auto path = std::filesystem::path(raw);
            CoTaskMemFree(raw);
            if (save) {
                const std::wstring expected = profile ? L".trprofile" : L".trskin";
                if (path.extension().empty()) path += expected;
                const auto original = path;
                std::error_code error;
                for (int n = 2; std::filesystem::exists(path, error) && n < 10002; ++n) {
                    path = original.parent_path() / (original.stem().wstring() + L"-" + std::to_wstring(n) + original.extension().wstring());
                }
                if (error) { result.error = "Could not inspect the selected save location."; return result; }
            }
            result.path = path.u8string();
            return result;
        }
    }
    result.error = "File dialog failed (HRESULT " + std::to_string(static_cast<unsigned long>(hr)) + ").";
#else
    result.error = "Native file dialogs are available on Windows.";
#endif
    return result;
}

} // namespace tenriff::app
