#include "Utf8Windows.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace digitqt::core::plugin_loading {

std::wstring utf8ToWide(const std::string &utf8) {
  if (utf8.empty())
    return {};
  const int wideLen =
      MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
  if (wideLen <= 0)
    return {};
  std::wstring wide(static_cast<size_t>(wideLen), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(),
                      wideLen);
  return wide;
}

std::string wideToUtf8(const std::wstring &wide) {
  if (wide.empty())
    return {};
  const int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                      nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return {};
  std::string out(static_cast<size_t>(len), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), len,
                      nullptr, nullptr);
  return out;
}

}  // namespace digitqt::core::plugin_loading
