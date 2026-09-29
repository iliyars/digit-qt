#pragma once

#include <string>
#include <vector>

namespace digitqt::core::plugin_loading {

/// Имена файлов (не полные пути) с расширением .dll, лежащих
/// непосредственно в directory (без рекурсии в подпапки). Пустой
/// вектор, если каталога нет или там ничего подходящего -- не ошибка
/// (тот же принцип, что и у pluginsDirectory()/Win32Library: отсутствие
/// плагинов -- нормальное состояние, не повод падать).
std::vector<std::string> listDllFiles(const std::string &directory);

}  // namespace digitqt::core::plugin_loading
