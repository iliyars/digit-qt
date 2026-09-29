#include "DllDirectoryScan.h"

#include "Utf8Windows.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace digitqt::core::plugin_loading {

std::vector<std::string> listDllFiles(const std::string &directory) {
  std::vector<std::string> result;

  const std::wstring pattern = utf8ToWide(directory + "/*.dll");
  WIN32_FIND_DATAW findData{};
  HANDLE handle = FindFirstFileW(pattern.c_str(), &findData);
  if (handle == INVALID_HANDLE_VALUE)
    return result;  // каталога нет или в нём нет .dll -- не ошибка

  do {
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      continue;
    result.push_back(wideToUtf8(findData.cFileName));
  } while (FindNextFileW(handle, &findData));

  FindClose(handle);
  return result;
}

}  // namespace digitqt::core::plugin_loading
