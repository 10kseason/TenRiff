#include "app/MenuApp.h"
#include "app/NativeFileDialog.h"
#include "util/Utf8Compat.h"

#ifdef _WIN32
#include <wrl/client.h>
#endif

namespace tenriff::app {

void MenuApp::choose_lr2_score_database() {
    if (lr2_import_future_.valid()) {
        lr2_import_cancel_->store(true, std::memory_order_relaxed);
        return;
    }
#ifdef _WIN32
    const auto picked = run_native_file_dialog_sta([]() -> NativeFileChoice {
        const NativeFileChoice failure{{}, "Could not open the LR2 database picker.", false};
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&dialog)))) return failure;
        DWORD options = 0;
        const COMDLG_FILTERSPEC filter{L"LR2 score database (*.db)", L"*.db"};
        if (FAILED(dialog->GetOptions(&options)) ||
            FAILED(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST)) ||
            FAILED(dialog->SetFileTypes(1, &filter))) return failure;
        const auto hr = show_native_file_dialog(dialog.Get());
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {{}, {}, true};
        if (FAILED(hr)) return failure;
        Microsoft::WRL::ComPtr<IShellItem> item;
        PWSTR raw = nullptr;
        if (FAILED(dialog->GetResult(&item)) ||
            FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw))) return failure;
        const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(raw, &CoTaskMemFree);
        return {std::filesystem::path(path.get()).u8string(), {}, false};
    });
    if (!picked.path.empty()) start_lr2_import(picked.path);
    else if (!picked.cancelled) lr2_import_status_ = ui_text(
        "Could not open the LR2 database picker.", "LR2 DB 선택 창을 열 수 없습니다.");
#else
    lr2_import_status_ = ui_text("Native file dialogs are available on Windows.",
                                "파일 선택 창은 Windows에서 사용할 수 있습니다.");
#endif
    publish_snapshot();
}

void MenuApp::start_lr2_import(const std::string& database) {
    if (lr2_import_future_.valid()) return;
    std::vector<std::filesystem::path> folders;
    for (const auto& folder : config_.ui.recent_song_sources)
        folders.push_back(std::filesystem::u8path(folder));
    if (!songs_path_.empty()) folders.push_back(std::filesystem::u8path(songs_path_));
    lr2_import_cancel_ = std::make_shared<std::atomic<bool>>(false);
    try {
        // Own all inputs. The worker never accesses MenuApp or writes the profile.
        lr2_import_future_ = std::async(std::launch::async,
            [database, folders = std::move(folders), cancel = lr2_import_cancel_] {
                return prepare_lr2_import(std::filesystem::u8path(database), folders, cancel.get());
            });
        lr2_import_status_ = ui_text("Importing LR2 lamps. Select again to cancel.",
                                     "LR2 램프를 가져오는 중입니다. 다시 선택하면 취소합니다.");
    } catch (...) {
        lr2_import_status_ = ui_text("Could not start LR2 import.", "LR2 가져오기를 시작할 수 없습니다.");
    }
}

void MenuApp::service_lr2_import() {
    if (!lr2_import_future_.valid() ||
        lr2_import_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;
    try {
        auto result = lr2_import_future_.get();
        // A click after the worker finished but before this poll still cancels.
        if (result.cancelled || lr2_import_cancel_->load(std::memory_order_relaxed)) {
            lr2_import_status_ = ui_text("LR2 import cancelled.", "LR2 가져오기를 취소했습니다.");
        } else if (!result.success()) {
            lr2_import_status_ = ui_text("LR2 import failed. Existing lamps were kept.",
                "LR2 가져오기에 실패했습니다. 기존 램프를 유지합니다.");
        } else {
            std::string error;
            if (save_lr2_import(std::filesystem::u8path(profile_dir_), result.lamps, error)) {
                lr2_imported_lamps_ = std::move(result.lamps);
                lr2_import_status_ = ui_text("LR2 lamps imported: ", "가져온 LR2 램프: ") +
                    std::to_string(lr2_imported_lamps_.size());
                if (result.skipped_folders) lr2_import_status_ += "\n" + ui_text(
                    "Some song folders could not be scanned for EX.", "일부 곡 폴더는 EX 확인을 위해 읽지 못했습니다.");
            } else lr2_import_status_ = ui_text("Could not save LR2 lamps. Existing lamps were kept.",
                "LR2 램프를 저장하지 못했습니다. 기존 램프를 유지합니다.");
        }
    } catch (...) {
        lr2_import_status_ = ui_text("LR2 import failed. Existing lamps were kept.",
            "LR2 가져오기에 실패했습니다. 기존 램프를 유지합니다.");
    }
    lr2_import_cancel_.reset();
    publish_snapshot();
}

}  // namespace tenriff::app
