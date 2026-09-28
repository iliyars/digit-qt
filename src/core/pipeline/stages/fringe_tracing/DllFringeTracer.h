#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"
#include "core/plugin_loading/Win32Library.h"

#include <memory>

// dqt_fringe_tracer_abi.h объявляет чистый C API; этот заголовок его не
// подключает, чтобы не течь наружу -- см. тот же приём в
// plugin_host::PluginFringeTracer.
typedef struct DqtFringeTracerVTable DqtFringeTracerVTable;
typedef struct DqtFringeTracerImpl *DqtFringeTracerHandle;

namespace digitqt::core::tracing {

/**
 * @brief Qt-свободный аналог plugin_host::PluginFringeTracer -- грузит
 * DqtFringeTracer C ABI-плагин (.dll) напрямую через Win32Library, а не
 * через QLibrary, чтобы SetupStage (живущий в Qt-свободной core) мог сам
 * предпочесть плагин встроенной реализации, не протаскивая Qt обратно в
 * эту библиотеку. Делает загруженный плагин неотличимым от встроенного
 * трекера для остального кода -- как и Qt-версия, см. tryLoadTracerPlugin().
 */
class DllFringeTracer : public IFringeTracer {
 public:
  /// Возвращает nullptr и заполняет outError при неудаче (файла нет,
  /// не экспортирует dqt_plugin_entry, отказ по версии ABI, create()
  /// вернул 0).
  static std::unique_ptr<DllFringeTracer> load(const std::string &libraryPath,
                                                std::string &outError);

  ~DllFringeTracer() override;

  bool initialize(const digitqt::core::Bitmap &image,
                  std::function<bool(int, int)> isVisible) override;
  std::vector<TracedLine> extract(const std::vector<SeedPoint> &seeds) override;
  std::string name() const override;
  const std::string &lastError() const override;
  bool setParam(const std::string &key, const std::string &value) override;
  std::vector<double> lastFringeOrders() const override;

 private:
  DllFringeTracer() = default;

  plugin_loading::Win32Library m_library;
  const DqtFringeTracerVTable *m_vtable = nullptr;
  DqtFringeTracerHandle m_handle = nullptr;

  // Копия предиката живёт здесь, не на стеке initialize() -- плагин
  // может вызвать её повторно позже, из extract() (см. то же в
  // plugin_host::PluginFringeTracer).
  std::function<bool(int, int)> m_isVisibleCpp;

  mutable std::string m_lastErrorCache;
  std::vector<double> m_lastFringeOrders;
};

}  // namespace digitqt::core::tracing
