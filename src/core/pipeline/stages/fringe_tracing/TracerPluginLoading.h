#pragma once

#include "core/FringeTracingData.h"
#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <memory>

namespace digitqt::core::tracing {

/**
 * @brief Пытается загрузить DLL-плагин для данного алгоритма трассировки
 * из <каталог exe>/plugins/tracers/<имя>.dll (см. PluginDirectory.h).
 *
 * Возвращает nullptr, если плагина нет, файл повреждён либо он отказал
 * по версии ABI -- это НЕ ошибка сама по себе: SetupStage в этом случае
 * молча использует встроенную реализацию (см. SetupStage.cpp). Так
 * приложение продолжает работать даже без каталога plugins/ рядом с exe.
 */
std::unique_ptr<IFringeTracer> tryLoadTracerPlugin(digitqt::core::TracerAlgorithm algorithm);

}  // namespace digitqt::core::tracing
