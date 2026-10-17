#include "doctest/doctest.h"
#include "app/NativeFileDialog.h"
#include <functional>

#ifdef _WIN32
namespace {
// Exercise the actual Show coordinator without shell timing or user input.
// Unused COM methods deliberately fail; this is not a replacement dialog.
class ProbeDialog final : public IFileDialog {
public:
    std::function<HRESULT()> show;
    std::function<HRESULT()> close;
    int shows = 0, closes = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE Show(HWND) override { ++shows;return show ? show() : S_OK; }
    HRESULT STDMETHODCALLTYPE Close(HRESULT) override { ++closes;return close ? close() : S_OK; }
    HRESULT STDMETHODCALLTYPE SetFileTypes(UINT,const COMDLG_FILTERSPEC*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetFileTypeIndex(UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetFileTypeIndex(UINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Advise(IFileDialogEvents*,DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Unadvise(DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetOptions(FILEOPENDIALOGOPTIONS) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetOptions(FILEOPENDIALOGOPTIONS*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetDefaultFolder(IShellItem*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetFolder(IShellItem*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetFolder(IShellItem**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCurrentSelection(IShellItem**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetFileName(LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetFileName(LPWSTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetTitle(LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetOkButtonLabel(LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetFileNameLabel(LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetResult(IShellItem**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE AddPlace(IShellItem*,FDAP) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetDefaultExtension(LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetClientGuid(REFGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ClearClientData() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetFilter(IShellItemFilter*) override { return E_NOTIMPL; }
};
}

TEST_CASE("native picker replacement closes previous once and bounds COM reentry") {
    using tenriff::app::show_native_file_dialog;
    ProbeDialog first,second,third;
    first.close=[&] { CHECK(FAILED(show_native_file_dialog(&third)));return S_OK; };
    first.show=[&] {
        CHECK(FAILED(show_native_file_dialog(&first)));
        CHECK(show_native_file_dialog(&second)==S_OK);
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    };
    CHECK(FAILED(show_native_file_dialog(&first)));
    CHECK(first.shows==1);CHECK(first.closes==1);
    CHECK(second.shows==1);CHECK(third.shows==0);
    first.show=nullptr;
    CHECK(show_native_file_dialog(&first)==S_OK);
    CHECK(first.shows==2);
}

TEST_CASE("native picker cancellation failure and Show failure restore coordinator") {
    using tenriff::app::show_native_file_dialog;
    ProbeDialog first,second;
    first.close=[] { return E_FAIL; };
    first.show=[&] { CHECK(FAILED(show_native_file_dialog(&second)));return E_FAIL; };
    CHECK(FAILED(show_native_file_dialog(&first)));
    CHECK(second.shows==0);CHECK(first.closes==1);
    CHECK(show_native_file_dialog(&second)==S_OK);
    CHECK(show_native_file_dialog(nullptr)==E_INVALIDARG);
    CHECK(second.shows==1);
}
#endif
