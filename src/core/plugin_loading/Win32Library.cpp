#include "Win32Library.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace digitqt::core::plugin_loading {

namespace {

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

std::string lastWin32ErrorMessage() {
  const DWORD code = GetLastError();
  LPSTR buffer = nullptr;
  const DWORD size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&buffer),
      0, nullptr);
  std::string message = (size > 0 && buffer) ? std::string(buffer, size) : "Unknown error";
  if (buffer)
    LocalFree(buffer);
  while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
    message.pop_back();
  return message;
}

}  // namespace

// Намеренно НЕ вызывает FreeLibrary -- см. .h. Деструктор просто обнуляет
// m_handle, ничего не выгружая.
Win32Library::~Win32Library() = default;

Win32Library::Win32Library(Win32Library &&other) noexcept
    : m_handle(other.m_handle), m_lastError(std::move(other.m_lastError)) {
  other.m_handle = nullptr;
}

Win32Library &Win32Library::operator=(Win32Library &&other) noexcept {
  if (this != &other) {
    // Тот же приём, что и в деструкторе -- см. .h: не выгружаем m_handle,
    // даже если он был загружен и сейчас затирается.
    m_handle = other.m_handle;
    m_lastError = std::move(other.m_lastError);
    other.m_handle = nullptr;
  }
  return *this;
}

bool Win32Library::load(const std::string &path) {
  const std::wstring widePath = utf8ToWide(path);
  m_handle = LoadLibraryW(widePath.c_str());
  if (!m_handle) {
    m_lastError = lastWin32ErrorMessage();
    return false;
  }
  m_lastError.clear();
  return true;
}

void *Win32Library::resolve(const char *symbolName) const {
  if (!m_handle)
    return nullptr;
  return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(m_handle), symbolName));
}

}  // namespace digitqt::core::plugin_loading
