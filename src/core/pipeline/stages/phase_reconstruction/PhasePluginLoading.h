#pragma once

#include "core/PhaseReconstructionAlgorithm.h"
#include "core/pipeline/stages/phase_reconstruction/IPhaseReconstructor.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace digitqt::core {

/**
 * @brief Пытается загрузить DLL-плагин для данного метода сшивки фазы
 * из <каталог exe>/plugins/phase/<имя>.dll (см. PluginDirectory.h).
 *
 * Возвращает nullptr, если плагина нет, файл повреждён либо он отказал
 * по версии ABI -- см. TracerPluginLoading.h, тот же контракт.
 */
std::unique_ptr<IPhaseReconstructor> tryLoadPhaseReconstructorPlugin(
    digitqt::core::PhaseReconstructionAlgorithm algorithm);

/// Один плагин, найденный сканированием plugins/phase/ -- см.
/// discoverPhaseReconstructorPlugins(). needsFringeLines -- то, что сам
/// плагин заявил о себе через DqtPhasePluginInfo (ABI v2), не предположение
/// хоста.
struct DiscoveredPhasePlugin {
  std::string filePath;
  std::string pluginName;
  std::string pluginVersion;
  bool needsFringeLines = false;
};

/**
 * @brief Сканирует <каталог exe>/plugins/phase/*.dll, пробуя загрузить и
 * опросить (dqt_phase_plugin_entry) КАЖДЫЙ файл -- в отличие от
 * tryLoadPhaseReconstructorPlugin(), не привязано к 3 известным
 * алгоритмам (PhaseReconstructionAlgorithm) и находит вообще всё, что
 * там лежит и отвечает на текущую версию ABI.
 *
 * Файл, который не грузится, не экспортирует dqt_phase_plugin_entry
 * либо отказал по версии ABI, просто пропускается -- один повреждённый
 * .dll не должен ронять весь список остальных.
 */
std::vector<DiscoveredPhasePlugin> discoverPhaseReconstructorPlugins();

/**
 * @brief Пробует опросить ОДИН конкретный .dll (произвольный путь, не
 * обязательно внутри plugins/phase/) -- та же проверка, что делает
 * discoverPhaseReconstructorPlugins() для каждого найденного файла, но
 * для пути, который явно назвал пользователь (см. "Browse..." в
 * ParametersDock). std::nullopt при неудаче (файл не грузится, не
 * экспортирует dqt_phase_plugin_entry, отказал по версии ABI) --
 * вызывающая сторона сама решает, как об этом сообщить пользователю.
 */
std::optional<DiscoveredPhasePlugin> probePhaseReconstructorPlugin(const std::string &filePath);

}  // namespace digitqt::core
