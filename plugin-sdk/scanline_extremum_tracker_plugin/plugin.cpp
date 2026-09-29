/**
 * @file plugin.cpp
 * @brief Scanline Extremum Method (FTM) -- самодостаточный DqtFringeTracer
 * C ABI-плагин. Не подключает и не линкует DigitQt::Core -- вся реализация
 * (ScanlineExtremumTracker.{h,cpp} + scanline_extremum/-эквивалент:
 * ScanlineExtremumTypes/RedCenterDetector/MiddleAlgorithm/FringeConstructor)
 * живёт прямо в этом плагине.
 */

#include "ScanlineExtremumTracker.h"
#include "dqt_fringe_tracer_abi.h"

#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

using scanline_extremum_plugin::ScanlineExtremumTracker;
using scanline_extremum_plugin::TracedLine;

struct PluginState {
  ScanlineExtremumTracker tracker;
  std::vector<std::vector<DqtTracedPoint>> pointStorage;
  std::vector<DqtTracedLine> lineStorage;
  std::string lastErrorUtf8;
};

DqtFringeTracerHandle create() {
  return reinterpret_cast<DqtFringeTracerHandle>(new (std::nothrow) PluginState());
}

void destroy(DqtFringeTracerHandle self) {
  delete reinterpret_cast<PluginState *>(self);
}

int initialize(DqtFringeTracerHandle self, const DqtBitmapView *image, DqtVisibilityFn isVisible,
              void *isVisibleUserData) {
  auto *state = reinterpret_cast<PluginState *>(self);
  auto predicate = [isVisible, isVisibleUserData](int x, int y) {
    return isVisible(x, y, isVisibleUserData) != 0;
  };
  const bool ok = state->tracker.initialize(image->pixels, image->width, image->height, predicate);
  state->lastErrorUtf8 = state->tracker.lastError();
  return ok ? 1 : 0;
}

int extract(DqtFringeTracerHandle self, const DqtSeedPoint *seeds, size_t seedCount,
           DqtTracedLine **outLines, size_t *outLineCount) {
  auto *state = reinterpret_cast<PluginState *>(self);
  *outLines = nullptr;
  *outLineCount = 0;

  std::vector<scanline_extremum_plugin::SeedPoint> seedVec;
  seedVec.reserve(seedCount);
  for (size_t i = 0; i < seedCount; ++i)
    seedVec.push_back({seeds[i].x, seeds[i].y});

  std::vector<TracedLine> lines = state->tracker.extract(seedVec);
  state->lastErrorUtf8 = state->tracker.lastError();
  const auto &numbers = state->tracker.lastFringeNumbers();
  const bool haveNumbers = numbers.size() == lines.size();

  state->pointStorage.assign(lines.size(), {});
  state->lineStorage.clear();
  state->lineStorage.reserve(lines.size());

  for (size_t i = 0; i < lines.size(); ++i) {
    auto &dst = state->pointStorage[i];
    dst.reserve(lines[i].size());
    for (const auto &p : lines[i])
      dst.push_back(DqtTracedPoint{p.x, p.y, p.width, p.intensity});
    const double order = haveNumbers ? numbers[i] : 0.0;
    state->lineStorage.push_back(
        DqtTracedLine{dst.data(), dst.size(), order, haveNumbers ? 1 : 0});
  }

  if (!state->lineStorage.empty()) {
    *outLines = state->lineStorage.data();
    *outLineCount = state->lineStorage.size();
  }
  return 1;
}

void freeLines(DqtFringeTracerHandle /*self*/, DqtTracedLine * /*lines*/, size_t /*lineCount*/) {
}

int setParam(DqtFringeTracerHandle self, const char *key, const char *value) {
  auto *state = reinterpret_cast<PluginState *>(self);
  return state->tracker.setParam(key, value) ? 1 : 0;
}

const char *name(DqtFringeTracerHandle /*self*/) {
  static const char *const kName = "Scanline Extremum Method (FTM)";
  return kName;
}

const char *lastError(DqtFringeTracerHandle self) {
  return reinterpret_cast<PluginState *>(self)->lastErrorUtf8.c_str();
}

const DqtFringeTracerVTable kVTable = {
    &create, &destroy, &initialize, &extract, &freeLines, &setParam, &name, &lastError,
};

}  // namespace

extern "C" DQT_ABI_EXPORT int dqt_plugin_entry(uint32_t hostAbiVersion, DqtPluginInfo *outInfo,
                                               const DqtFringeTracerVTable **outVTable) {
  if (hostAbiVersion != DQT_FRINGE_TRACER_ABI_VERSION) {
    *outVTable = nullptr;
    return 0;
  }
  outInfo->pluginName = "ScanlineExtremumTracker";
  outInfo->pluginVersion = "2.0.0";  // 2.0.0: полностью самодостаточная реализация
  *outVTable = &kVTable;
  return 1;
}
