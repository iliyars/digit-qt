#include "TracerPluginLoading.h"

#include "core/pipeline/stages/fringe_tracing/DllFringeTracer.h"
#include "core/plugin_loading/PluginDirectory.h"

namespace digitqt::core::tracing {

namespace {

// Стабильные имена файлов -- не переименовывать без обновления сборки
// соответствующих plugin-sdk/*_plugin проектов и dist/plugins/tracers/.
const char *pluginFileName(digitqt::core::TracerAlgorithm algorithm) {
  switch (algorithm) {
    case digitqt::core::TracerAlgorithm::SequentialTracking:
      return "sequential_fringe_tracker.dll";
    case digitqt::core::TracerAlgorithm::StructureTensor:
      return "structure_tensor_tracker.dll";
    case digitqt::core::TracerAlgorithm::ScanlineExtremum:
      return "scanline_extremum_tracker.dll";
    case digitqt::core::TracerAlgorithm::BinaryThinning:
      return "binary_thinning_tracker.dll";
  }
  return nullptr;
}

}  // namespace

std::unique_ptr<IFringeTracer> tryLoadTracerPlugin(digitqt::core::TracerAlgorithm algorithm) {
  const char *fileName = pluginFileName(algorithm);
  if (!fileName)
    return nullptr;

  const std::string dir = plugin_loading::pluginsDirectory("tracers");
  if (dir.empty())
    return nullptr;

  std::string error;
  auto tracer = DllFringeTracer::load(dir + "/" + fileName, error);
  // Отсутствие/повреждение плагина -- не ошибка на этом уровне, вызывающий
  // код (SetupStage) переключается на встроенную реализацию молча.
  return tracer;
}

}  // namespace digitqt::core::tracing
