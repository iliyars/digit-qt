#include "DllPhaseReconstructor.h"

#include "dqt_phase_reconstructor_abi.h"

namespace digitqt::core {

namespace {

int visibilityTrampoline(int32_t x, int32_t y, void *userData) {
  auto *fn = static_cast<std::function<bool(int, int)> *>(userData);
  return (*fn)(x, y) ? 1 : 0;
}

}  // namespace

std::unique_ptr<DllPhaseReconstructor> DllPhaseReconstructor::load(const std::string &libraryPath,
                                                                    std::string &outError) {
  std::unique_ptr<DllPhaseReconstructor> result(new DllPhaseReconstructor());

  if (!result->m_library.load(libraryPath)) {
    outError = result->m_library.lastError();
    return nullptr;
  }

  auto entry =
      reinterpret_cast<DqtPhasePluginEntryFn>(result->m_library.resolve("dqt_phase_plugin_entry"));
  if (!entry) {
    outError = "Plugin does not export dqt_phase_plugin_entry";
    return nullptr;
  }

  DqtPhasePluginInfo info{};
  if (!entry(DQT_PHASE_RECONSTRUCTOR_ABI_VERSION, &info, &result->m_vtable) || !result->m_vtable) {
    outError =
        "Plugin refused host ABI version " + std::to_string(DQT_PHASE_RECONSTRUCTOR_ABI_VERSION);
    return nullptr;
  }

  result->m_handle = result->m_vtable->create();
  if (!result->m_handle) {
    outError = "Plugin create() failed";
    return nullptr;
  }

  return result;
}

DllPhaseReconstructor::~DllPhaseReconstructor() {
  if (m_vtable && m_handle)
    m_vtable->destroy(m_handle);
}

PhaseMap DllPhaseReconstructor::reconstruct(int gridWidth, int gridHeight,
                                            const digitqt::core::Bitmap &image,
                                            std::function<bool(int, int)> isVisible,
                                            const std::vector<NumberedFringeLine> &lines) {
  DqtPhaseBitmapView view{image.width(), image.height(), image.data()};

  // Владеет точками ровно до конца этого вызова -- ABI-структуры ниже
  // только заимствуют указатели на них (тот же контракт "borrowed for
  // the duration of the call", что и у DqtBitmapView в трассировке).
  std::vector<std::vector<DqtPhasePoint>> pointStorage;
  pointStorage.reserve(lines.size());
  std::vector<DqtPhaseNumberedFringeLine> cLines;
  cLines.reserve(lines.size());
  for (const auto &line : lines) {
    std::vector<DqtPhasePoint> points;
    points.reserve(line.points.size());
    for (const auto &p : line.points)
      points.push_back(DqtPhasePoint{p.x, p.y});
    pointStorage.push_back(std::move(points));
    cLines.push_back(
        DqtPhaseNumberedFringeLine{pointStorage.back().data(), pointStorage.back().size(), line.order});
  }

  std::vector<double> outValues(static_cast<size_t>(gridWidth) * static_cast<size_t>(gridHeight));

  const bool ok = m_vtable->reconstruct(m_handle, gridWidth, gridHeight, &view, &visibilityTrampoline,
                                        &isVisible, cLines.empty() ? nullptr : cLines.data(),
                                        cLines.size(), outValues.data()) != 0;
  m_lastErrorCache = m_vtable->lastError(m_handle);

  if (!ok)
    return {};

  PhaseMap result(gridWidth, gridHeight);
  for (int y = 0; y < gridHeight; ++y)
    for (int x = 0; x < gridWidth; ++x)
      result.setValue(x, y, outValues[static_cast<size_t>(y) * gridWidth + x]);
  return result;
}

std::string DllPhaseReconstructor::name() const {
  return m_vtable->name(m_handle);
}

const std::string &DllPhaseReconstructor::lastError() const {
  return m_lastErrorCache;
}

}  // namespace digitqt::core
