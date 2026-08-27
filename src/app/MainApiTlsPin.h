#pragma once

#include <array>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>

namespace tenriff::app::main_api_tls_pin {

inline constexpr std::wstring_view kHost = L"121.174.18.181";
inline constexpr INTERNET_PORT kPort = 27303;
inline constexpr std::array<BYTE, 32> kCertificateSha256{
    0x29, 0xD8, 0xF5, 0x73, 0xE1, 0x3C, 0xE8, 0x92,
    0xB8, 0xE8, 0xD4, 0x85, 0x33, 0x4C, 0x18, 0xD1,
    0x04, 0x92, 0x50, 0x57, 0xB0, 0x84, 0x04, 0x5D,
    0xE2, 0x40, 0xC6, 0xA2, 0x57, 0xBE, 0x99, 0xBC,
};

inline bool applies(INTERNET_SCHEME scheme,
                    INTERNET_PORT port,
                    std::wstring_view host) {
    return scheme == INTERNET_SCHEME_HTTPS && port == kPort && host == kHost;
}

inline bool prepare(HINTERNET request, bool required, std::string& error) {
    if (!required) return true;
    DWORD security_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS,
                          &security_flags, sizeof(security_flags))) {
        error = "Could not prepare TenRiff main certificate pinning (WinHTTP error " +
                std::to_string(GetLastError()) + ").";
        return false;
    }
    return true;
}

inline bool verify(HINTERNET request, bool required, std::string& error) {
    if (!required) return true;

    PCCERT_CONTEXT certificate = nullptr;
    DWORD certificate_size = sizeof(certificate);
    if (!WinHttpQueryOption(request, WINHTTP_OPTION_SERVER_CERT_CONTEXT,
                            &certificate, &certificate_size) || !certificate) {
        error = "Could not inspect the TenRiff main TLS certificate (WinHTTP error " +
                std::to_string(GetLastError()) + ").";
        return false;
    }

    std::array<BYTE, 32> actual{};
    DWORD actual_size = static_cast<DWORD>(actual.size());
    const BOOL hashed = CryptHashCertificate(
        0, CALG_SHA_256, 0, certificate->pbCertEncoded,
        certificate->cbCertEncoded, actual.data(), &actual_size);
    CertFreeCertificateContext(certificate);

    if (!hashed || actual_size != actual.size() || actual != kCertificateSha256) {
        error = "TenRiff main TLS certificate pin mismatch. Connection refused.";
        return false;
    }
    return true;
}

}  // namespace tenriff::app::main_api_tls_pin
#endif
