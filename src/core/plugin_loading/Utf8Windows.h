#pragma once

#include <string>

namespace digitqt::core::plugin_loading {

/// UTF-8 <-> UTF-16 конвертация для Win32 *W-функций (LoadLibraryW,
/// FindFirstFileW, ...). Общее место вместо трёх независимых копий в
/// Win32Library.cpp/PluginDirectory.cpp/DllDirectoryScan.cpp.
std::wstring utf8ToWide(const std::string &utf8);
std::string wideToUtf8(const std::wstring &wide);

}  // namespace digitqt::core::plugin_loading
