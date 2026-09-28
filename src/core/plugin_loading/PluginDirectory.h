#pragma once

#include <string>

namespace digitqt::core::plugin_loading {

/**
 * @brief Путь к каталогу плагинов данной категории, рядом с исполняемым
 * файлом: <каталог exe>/plugins/<subfolder>/.
 *
 * Вычисляется через GetModuleFileNameW (Qt-свободно, чтобы SetupStage
 * мог сам решать, грузить плагин или нет, не завися от GUI-слоя -- см.
 * Win32Library.h). Каталог не обязан существовать; вызывающий код
 * (tryLoadTracerPlugin и т.п.) сам обрабатывает отсутствующие файлы как
 * "плагина нет, используем встроенную реализацию".
 */
std::string pluginsDirectory(const std::string &subfolder);

}  // namespace digitqt::core::plugin_loading
