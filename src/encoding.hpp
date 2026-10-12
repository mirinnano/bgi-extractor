#pragma once

#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <iconv.h>
#include <cerrno>
#endif

namespace bgi {

inline std::string cp932_to_utf8(std::string_view sjis) {
    if (sjis.empty()) return {};

#if defined(_WIN32) || defined(_WIN64)
    int wlen = MultiByteToWideChar(932, 0, sjis.data(), static_cast<int>(sjis.size()), nullptr, 0);
    if (wlen <= 0) return std::string(sjis);
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(932, 0, sjis.data(), static_cast<int>(sjis.size()), &wstr[0], wlen);

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) return std::string(sjis);
    std::string u8str(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wlen, &u8str[0], u8len, nullptr, nullptr);
    return u8str;
#else
    iconv_t cd = iconv_open("UTF-8", "CP932");
    if (cd == (iconv_t)-1) {
        cd = iconv_open("UTF-8", "SHIFT_JIS");
    }
    if (cd == (iconv_t)-1) {
        return std::string(sjis);
    }

    size_t in_bytes_left = sjis.size();
    char* in_buf = const_cast<char*>(sjis.data());

    size_t out_bytes_left = sjis.size() * 4 + 16;
    std::string out_str(out_bytes_left, '\0');
    char* out_buf = &out_str[0];
    char* out_start = out_buf;

    size_t res = iconv(cd, &in_buf, &in_bytes_left, &out_buf, &out_bytes_left);
    iconv_close(cd);

    if (res == (size_t)-1 && errno != E2BIG) {
        return std::string(sjis);
    }

    out_str.resize(out_buf - out_start);
    return out_str;
#endif
}

} // namespace bgi
