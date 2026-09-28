// См. sequential_fringe_tracker_plugin/plugin.cpp -- тот же приём,
// StructureTensorTracker вместо SequentialFringeTracker.

#include "core/Bitmap.h"
#include "core/pipeline/stages/fringe_tracing/StructureTensorTracker.h"
#include "dqt_fringe_tracer_abi.h"

#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

using digitqt::core::tracing::StructureTensorTracker;
using digitqt::core::tracing::TracedLine;

struct PluginState {
  StructureTensorTracker tracker;
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

int initialize(DqtFringeTracerHandle self, const DqtBitmapView *image,
              DqtVisibilityFn isVisible, void *isVisibleUserData) {
  auto *state = reinterpret_cast<PluginState *>(self);

  digitqt::core::Bitmap bitmap(image->width, image->height);
  std::memcpy(bitmap.data(), image->pixels,
              static_cast<size_t>(image->width) * static_cast<size_t>(image->height));

  auto predicate = [isVisible, isVisibleUserData](int x, int y) {
    return isVisible(x, y, isVisibleUserData) != 0;
  };

  const bool ok = state->tracker.initialize(bitmap, predicate);
  state->lastErrorUtf8 = state->tracker.lastError();
  return ok ? 1 : 0;
}

int extract(DqtFringeTracerHandle self, const DqtSeedPoint *seeds, size_t seedCount,
           DqtTracedLine **outLines, size_t *outLineCount) {
  auto *state = reinterpret_cast<PluginState *>(self);
  *outLines = nullptr;
  *outLineCount = 0;

  std::vector<digitqt::core::tracing::SeedPoint> seedVec;
  seedVec.reserve(seedCount);
  for (size_t i = 0; i < seedCount; ++i)
    seedVec.push_back({seeds[i].x, seeds[i].y});

  std::vector<TracedLine> lines = state->tracker.extract(seedVec);
  state->lastErrorUtf8 = state->tracker.lastError();

  state->pointStorage.assign(lines.size(), {});
  state->lineStorage.clear();
  state->lineStorage.reserve(lines.size());

  for (size_t i = 0; i < lines.size(); ++i) {
    auto &dst = state->pointStorage[i];
    dst.reserve(lines[i].size());
    for (const auto &p : lines[i])
      dst.push_back(DqtTracedPoint{p.x, p.y, p.width, p.intensity});
    // StructureTensorTracker не вычисляет номер полосы сам -- hasOrder=0.
    state->lineStorage.push_back(DqtTracedLine{dst.data(), dst.size(), 0.0, 0});
  }

  if (!state->lineStorage.empty()) {
    *outLines = state->lineStorage.data();
    *outLineCount = state->lineStorage.size();
  }
  return 1;
}

void freeLines(DqtFringeTracerHandle /*self*/, DqtTracedLine * /*lines*/, size_t /*lineCount*/) {
}

int setParam(DqtFringeTracerHandle /*self*/, const char * /*key*/, const char * /*value*/) {
  // SetupStage сейчас не настраивает StructureTensorParams -- все ключи
  // неизвестны, как и у встроенного пути.
  return 0;
}

const char *name(DqtFringeTracerHandle /*self*/) {
  static const char *const kName = "Ride Tracking (Structure Tensor) [plugin]";
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
  outInfo->pluginName = "StructureTensorTracker";
  outInfo->pluginVersion = "1.0.0";
  *outVTable = &kVTable;
  return 1;
}
