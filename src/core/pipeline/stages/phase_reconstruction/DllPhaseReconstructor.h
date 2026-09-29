#pragma once

#include "core/pipeline/stages/phase_reconstruction/IPhaseReconstructor.h"
#include "core/plugin_loading/Win32Library.h"

#include <memory>

// dqt_phase_reconstructor_abi.h объявляет чистый C API; этот заголовок
// его не подключает, чтобы не течь наружу -- тот же приём, что у
// core::tracing::DllFringeTracer.
typedef struct DqtPhaseReconstructorVTable DqtPhaseReconstructorVTable;
typedef struct DqtPhaseReconstructorImpl *DqtPhaseReconstructorHandle;

namespace digitqt::core {

/**
 * @brief Qt-свободный загрузчик DqtPhaseReconstructor C ABI-плагина
 * (.dll) через Win32Library -- аналог core::tracing::DllFringeTracer,
 * только для контракта сшивки фазы. Делает загруженный плагин
 * неотличимым от встроенной реализации для PhaseReconstructionStage --
 * см. tryLoadPhaseReconstructorPlugin().
 */
class DllPhaseReconstructor : public IPhaseReconstructor {
 public:
  /// Возвращает nullptr и заполняет outError при неудаче (файла нет, не
  /// экспортирует dqt_phase_plugin_entry, отказ по версии ABI, create()
  /// вернул 0).
  static std::unique_ptr<DllPhaseReconstructor> load(const std::string &libraryPath,
                                                      std::string &outError);

  ~DllPhaseReconstructor() override;

  PhaseMap reconstruct(int gridWidth, int gridHeight, const digitqt::core::Bitmap &image,
                       std::function<bool(int, int)> isVisible,
                       const std::vector<NumberedFringeLine> &lines) override;
  std::string name() const override;
  const std::string &lastError() const override;

 private:
  DllPhaseReconstructor() = default;

  plugin_loading::Win32Library m_library;
  const DqtPhaseReconstructorVTable *m_vtable = nullptr;
  DqtPhaseReconstructorHandle m_handle = nullptr;

  mutable std::string m_lastErrorCache;
};

}  // namespace digitqt::core
