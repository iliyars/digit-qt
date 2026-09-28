#pragma once

#include <string>

namespace digitqt::core::plugin_loading {

/**
 * @brief Минимальная, Qt-свободная обёртка над LoadLibraryW/FreeLibrary/
 * GetProcAddress.
 *
 * core/ не должен линковать Qt (см. src/core/CMakeLists.txt) -- поэтому
 * не может переиспользовать QLibrary, которым пользуется
 * plugin_host::PluginFringeTracer. Это позволяет SetupStage (сам живущий
 * в core) грузить DqtFringeTracer-плагины самостоятельно, без обращения
 * к GUI-слою. Только Windows -- проект и так собирается только под
 * MSYS2/MinGW (см. заметки по сборке), кроссплатформенность не нужна.
 */
class Win32Library {
 public:
  Win32Library() = default;
  ~Win32Library();

  Win32Library(const Win32Library &) = delete;
  Win32Library &operator=(const Win32Library &) = delete;
  Win32Library(Win32Library &&other) noexcept;
  Win32Library &operator=(Win32Library &&other) noexcept;

  /// path -- UTF-8. Возвращает false и заполняет lastError() при неудаче.
  bool load(const std::string &path);

  /// nullptr, если библиотека не загружена или символ не найден.
  void *resolve(const char *symbolName) const;

  bool isLoaded() const { return m_handle != nullptr; }
  const std::string &lastError() const { return m_lastError; }

 private:
  void *m_handle = nullptr;  // HMODULE; void* чтобы не тащить <windows.h> в заголовок
  std::string m_lastError;
};

}  // namespace digitqt::core::plugin_loading
