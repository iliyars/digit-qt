#include "PhasePluginLoading.h"

#include "core/pipeline/stages/phase_reconstruction/DllPhaseReconstructor.h"
#include "core/plugin_loading/DllDirectoryScan.h"
#include "core/plugin_loading/PluginDirectory.h"
#include "core/plugin_loading/Win32Library.h"
#include "dqt_phase_reconstructor_abi.h"

namespace digitqt::core {

namespace {

// Стабильные имена файлов -- не переименовывать без обновления сборки
// соответствующих plugin-sdk/*_phase_plugin проектов.
const char *pluginFileName(digitqt::core::PhaseReconstructionAlgorithm algorithm) {
  switch (algorithm) {
    case digitqt::core::PhaseReconstructionAlgorithm::HorizontalSpline:
      return "horizontal_spline.dll";
    case digitqt::core::PhaseReconstructionAlgorithm::FourierTransform:
      return "fourier.dll";
    case digitqt::core::PhaseReconstructionAlgorithm::WaveletTransform:
      return "wavelet.dll";
  }
  return nullptr;
}

}  // namespace

std::unique_ptr<IPhaseReconstructor> tryLoadPhaseReconstructorPlugin(
    digitqt::core::PhaseReconstructionAlgorithm algorithm) {
  const char *fileName = pluginFileName(algorithm);
  if (!fileName)
    return nullptr;

  const std::string dir = plugin_loading::pluginsDirectory("phase");
  if (dir.empty())
    return nullptr;

  std::string error;
  auto reconstructor = DllPhaseReconstructor::load(dir + "/" + fileName, error);
  // nullptr при отсутствии/повреждении плагина -- не ошибка на уровне
  // ЭТОЙ функции, вызывающая сторона (PhaseReconstructionStage.cpp)
  // сама решает, что с этим делать. Встроенной реализации в core больше
  // нет ни у одного из 3 методов (см. CMakeLists.txt в этой папке), так
  // что на практике nullptr здесь означает настоящую ошибку расчёта.
  return reconstructor;
}

std::optional<DiscoveredPhasePlugin> probePhaseReconstructorPlugin(const std::string &filePath) {
  // Лёгкий пробный load+query напрямую через Win32Library, а не через
  // DllPhaseReconstructor -- тот заточен под "загрузить и использовать
  // один конкретный, уже выбранный плагин", здесь же нужен только опрос
  // DqtPhasePluginInfo.
  plugin_loading::Win32Library library;
  if (!library.load(filePath))
    return std::nullopt;  // не грузится

  auto entry = reinterpret_cast<DqtPhasePluginEntryFn>(library.resolve("dqt_phase_plugin_entry"));
  if (!entry)
    return std::nullopt;  // не экспортирует нужный символ -- не наш плагин

  DqtPhasePluginInfo info{};
  const DqtPhaseReconstructorVTable *vtable = nullptr;
  if (!entry(DQT_PHASE_RECONSTRUCTOR_ABI_VERSION, &info, &vtable) || !vtable)
    return std::nullopt;  // отказал по версии ABI

  DiscoveredPhasePlugin discovered;
  discovered.filePath = filePath;
  discovered.pluginName = info.pluginName ? info.pluginName : "";
  discovered.pluginVersion = info.pluginVersion ? info.pluginVersion : "";
  discovered.needsFringeLines = info.needsFringeLines != 0;
  return discovered;
}

std::vector<DiscoveredPhasePlugin> discoverPhaseReconstructorPlugins() {
  std::vector<DiscoveredPhasePlugin> result;

  const std::string dir = plugin_loading::pluginsDirectory("phase");
  if (dir.empty())
    return result;

  for (const auto &fileName : plugin_loading::listDllFiles(dir)) {
    // Один повреждённый .dll не должен ронять весь список остальных --
    // тихо пропускаем (см. probePhaseReconstructorPlugin()).
    if (auto plugin = probePhaseReconstructorPlugin(dir + "/" + fileName))
      result.push_back(std::move(*plugin));
  }

  return result;
}

}  // namespace digitqt::core
