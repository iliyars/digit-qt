#include "DllFringeTracer.h"

#include "dqt_fringe_tracer_abi.h"

namespace digitqt::core::tracing {

namespace {

int visibilityTrampoline(int32_t x, int32_t y, void *userData) {
  auto *fn = static_cast<std::function<bool(int, int)> *>(userData);
  return (*fn)(x, y) ? 1 : 0;
}

}  // namespace

std::unique_ptr<DllFringeTracer> DllFringeTracer::load(const std::string &libraryPath,
                                                        std::string &outError) {
  std::unique_ptr<DllFringeTracer> result(new DllFringeTracer());

  if (!result->m_library.load(libraryPath)) {
    outError = result->m_library.lastError();
    return nullptr;
  }

  auto entry = reinterpret_cast<DqtPluginEntryFn>(result->m_library.resolve("dqt_plugin_entry"));
  if (!entry) {
    outError = "Plugin does not export dqt_plugin_entry";
    return nullptr;
  }

  DqtPluginInfo info{};
  if (!entry(DQT_FRINGE_TRACER_ABI_VERSION, &info, &result->m_vtable) || !result->m_vtable) {
    outError = "Plugin refused host ABI version " +
               std::to_string(DQT_FRINGE_TRACER_ABI_VERSION);
    return nullptr;
  }

  result->m_handle = result->m_vtable->create();
  if (!result->m_handle) {
    outError = "Plugin create() failed";
    return nullptr;
  }

  return result;
}

DllFringeTracer::~DllFringeTracer() {
  if (m_vtable && m_handle)
    m_vtable->destroy(m_handle);
}

bool DllFringeTracer::initialize(const digitqt::core::Bitmap &image,
                                 std::function<bool(int, int)> isVisible) {
  m_isVisibleCpp = std::move(isVisible);

  DqtBitmapView view{image.width(), image.height(), image.data()};
  const bool ok =
      m_vtable->initialize(m_handle, &view, &visibilityTrampoline, &m_isVisibleCpp) != 0;
  m_lastErrorCache = m_vtable->lastError(m_handle);
  return ok;
}

std::vector<TracedLine> DllFringeTracer::extract(const std::vector<SeedPoint> &seeds) {
  std::vector<DqtSeedPoint> cSeeds;
  cSeeds.reserve(seeds.size());
  for (const auto &s : seeds)
    cSeeds.push_back(DqtSeedPoint{s.x, s.y});

  DqtTracedLine *lines = nullptr;
  size_t lineCount = 0;
  m_vtable->extract(m_handle, cSeeds.empty() ? nullptr : cSeeds.data(), cSeeds.size(), &lines,
                    &lineCount);
  m_lastErrorCache = m_vtable->lastError(m_handle);

  std::vector<TracedLine> result;
  result.reserve(lineCount);
  m_lastFringeOrders.clear();
  std::vector<double> orders;
  orders.reserve(lineCount);
  bool allHaveOrder = lineCount > 0;
  for (size_t i = 0; i < lineCount; ++i) {
    TracedLine line;
    line.reserve(lines[i].count);
    for (size_t j = 0; j < lines[i].count; ++j) {
      const auto &p = lines[i].points[j];
      line.push_back(TracedPoint{p.x, p.y, p.width, p.intensity});
    }
    result.push_back(std::move(line));
    orders.push_back(lines[i].order);
    if (!lines[i].hasOrder)
      allHaveOrder = false;
  }
  if (allHaveOrder)
    m_lastFringeOrders = std::move(orders);

  m_vtable->freeLines(m_handle, lines, lineCount);
  return result;
}

std::string DllFringeTracer::name() const {
  return m_vtable->name(m_handle);
}

const std::string &DllFringeTracer::lastError() const {
  return m_lastErrorCache;
}

bool DllFringeTracer::setParam(const std::string &key, const std::string &value) {
  return m_vtable->setParam(m_handle, key.c_str(), value.c_str()) != 0;
}

std::vector<double> DllFringeTracer::lastFringeOrders() const {
  return m_lastFringeOrders;
}

}  // namespace digitqt::core::tracing
