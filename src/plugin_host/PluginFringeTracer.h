#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QLibrary>
#include <QString>

#include <functional>
#include <memory>

// dqt_fringe_tracer_abi.h объявляет чистый C API (DqtFringeTracerVTable и
// т.п.); PluginFringeTracer.cpp -- единственное место в приложении, где
// это видно, всё остальное продолжает работать через core::tracing::
// IFringeTracer, как и со встроенными трекерами.
typedef struct DqtFringeTracerVTable DqtFringeTracerVTable;
typedef struct DqtFringeTracerImpl *DqtFringeTracerHandle;

namespace digitqt::plugin_host {

/**
 * @brief Грузит DqtFringeTracer C ABI-плагин (.dll) и делает его
 * неотличимым для остального приложения от встроенного core::tracing::
 * IFringeTracer -- SetupStage и подборщик алгоритма в UI не должны знать
 * и не будут знать, что эта конкретная реализация живёт за границей DLL.
 *
 * Проверено на practике в plugin-sdk/plugin_tracer_check против
 * binary_thinning_poc_plugin -- побитовое совпадение с прямым вызовом.
 */
class PluginFringeTracer : public digitqt::core::tracing::IFringeTracer {
public:
  /// Возвращает nullptr и заполняет outError, если библиотеку не удалось
  /// загрузить, она не экспортирует dqt_plugin_entry, отказалась от
  /// версии ABI хоста, либо create() вернул нулевой handle.
  static std::unique_ptr<PluginFringeTracer> load(const QString &libraryPath, QString &outError);

  ~PluginFringeTracer() override;

  bool initialize(const digitqt::core::Bitmap &image,
                  std::function<bool(int, int)> isVisible) override;
  std::vector<digitqt::core::tracing::TracedLine> extract(
      const std::vector<digitqt::core::tracing::SeedPoint> &seeds) override;
  std::string name() const override;
  const std::string &lastError() const override;

private:
  PluginFringeTracer();

  QLibrary m_library;
  const DqtFringeTracerVTable *m_vtable = nullptr;
  DqtFringeTracerHandle m_handle = nullptr;

  // Держим копию предиката здесь (не на стеке initialize()!) -- плагин
  // может вызвать его повторно позже, из extract(), через userData-
  // указатель, который мы ему передали. Он обязан указывать на живой
  // объект весь срок жизни *this, а не только на время одного вызова.
  std::function<bool(int, int)> m_isVisibleCpp;

  mutable std::string m_lastErrorCache;
};

}  // namespace digitqt::plugin_host
