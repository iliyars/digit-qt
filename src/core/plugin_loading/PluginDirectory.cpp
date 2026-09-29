#include "PluginDirectory.h"

#include "Utf8Windows.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace digitqt::core::plugin_loading {

namespace {

std::string executableDirectory() {
  std::wstring buffer(MAX_PATH, L'\0');
  DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  // GetModuleFileNameW doesn't null-terminate on truncation and returns
  // the buffer size -- grow and retry rather than silently truncating a
  // long install path.
  while (length == buffer.size()) {
    buffer.resize(buffer.size() * 2);
    length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  }
  if (length == 0)
    return {};
  buffer.resize(length);

  const size_t lastSlash = buffer.find_last_of(L"\\/");
  if (lastSlash == std::wstring::npos)
    return {};
  return wideToUtf8(buffer.substr(0, lastSlash));
}

}  // namespace

std::string pluginsDirectory(const std::string &subfolder) {
  const std::string exeDir = executableDirectory();
  if (exeDir.empty())
    return {};
  return exeDir + "/plugins/" + subfolder;
}

}  // namespace digitqt::core::plugin_loading
