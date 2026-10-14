#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite_3.h>
#include <wrl/client.h>

#include <algorithm>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <string>

namespace tenriff::render {

// A window-owned collection: no font installation, registry writes or global
// loaders. DirectWrite retains the local file references while formats use it.
struct PrivateUiFont {
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection;
    std::wstring family;

    bool load(IDWriteFactory* factory, const std::string& utf8_path) {
        collection.Reset();
        family.clear();
        if (!factory || utf8_path.empty()) return false;
        try {
            const auto path = std::filesystem::absolute(std::filesystem::u8path(utf8_path));
            auto extension = path.extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            std::error_code ec;
            if ((extension != L".ttf" && extension != L".otf") ||
                !std::filesystem::is_regular_file(path, ec)) return false;
            Microsoft::WRL::ComPtr<IDWriteFactory3> factory3;
            if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory3)))) return false;
            Microsoft::WRL::ComPtr<IDWriteFontFile> file;
            if (FAILED(factory->CreateFontFileReference(path.c_str(), nullptr, &file))) return false;
            BOOL supported = FALSE;
            DWRITE_FONT_FILE_TYPE file_type{};
            DWRITE_FONT_FACE_TYPE face_type{};
            UINT32 faces = 0;
            if (FAILED(file->Analyze(&supported, &file_type, &face_type, &faces)) || !supported || faces == 0)
                return false;
            Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> builder;
            if (FAILED(factory3->CreateFontSetBuilder(&builder))) return false;
            for (UINT32 face = 0; face < faces; ++face) {
                Microsoft::WRL::ComPtr<IDWriteFontFaceReference> reference;
                if (FAILED(factory3->CreateFontFaceReference(file.Get(), face,
                            DWRITE_FONT_SIMULATIONS_NONE, &reference)) ||
                    FAILED(builder->AddFontFaceReference(reference.Get()))) return false;
            }
            Microsoft::WRL::ComPtr<IDWriteFontSet> set;
            Microsoft::WRL::ComPtr<IDWriteFontCollection1> loaded;
            if (FAILED(builder->CreateFontSet(&set)) ||
                FAILED(factory3->CreateFontCollectionFromFontSet(set.Get(), &loaded)) ||
                loaded->GetFontFamilyCount() == 0) return false;
            Microsoft::WRL::ComPtr<IDWriteFontFamily> first_family;
            Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
            if (FAILED(loaded->GetFontFamily(0, &first_family)) ||
                FAILED(first_family->GetFamilyNames(&names)) || names->GetCount() == 0) return false;
            UINT32 index = 0, length = 0;
            BOOL exists = FALSE;
            if (FAILED(names->FindLocaleName(L"en-us", &index, &exists)) || !exists) index = 0;
            if (FAILED(names->GetStringLength(index, &length)) || length == 0) return false;
            std::wstring name(length + 1, L'\0');
            if (FAILED(names->GetString(index, name.data(), length + 1))) return false;
            name.resize(length);
            collection = std::move(loaded);
            family = std::move(name);
            return true;
        } catch (const std::exception&) {
            // Invalid UTF-8 from a hand-edited profile must also fall back.
            return false;
        }
    }
};

}  // namespace tenriff::render
#endif
