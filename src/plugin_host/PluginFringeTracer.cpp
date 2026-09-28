#include "PluginFringeTracer.h"

#include "dqt_fringe_tracer_abi.h"

namespace digitqt::plugin_host {

namespace {

int visibilityTrampoline(int32_t x, int32_t y, void *userData) {
  auto *fn = static_cast<std::function<bool(int, int)> *>(userData);
  return (*fn)(x, y) ? 1 : 0;
}

}  // namespace

PluginFringeTracer::PluginFringeTracer() = default;

std::unique_ptr<PluginFringeTracer> PluginFringeTracer::load(const QString &libraryPath,
                                                              QString &outError) {
  // Конструктор private -- new вместо std::make_unique.
  std::unique_ptr<PluginFringeTracer> result(new PluginFringeTracer());

  result->m_library.setFileName(libraryPath);
  if (!result->m_library.load()) {
    outError = result->m_library.errorString();
    return nullptr;
  }

  auto entry =
      reinterpret_cast<DqtPluginEntryFn>(result->m_library.resolve("dqt_plugin_entry"));
  if (!entry) {
    outError = QStringLiteral("Plugin does not export dqt_plugin_entry");
    return nullptr;
  }

  DqtPluginInfo info{};
  if (!entry(DQT_FRINGE_TRACER_ABI_VERSION, &info, &result->m_vtable) || !result->m_vtable) {
    outError =
        QStringLiteral("Plugin refused host ABI version %1").arg(DQT_FRINGE_TRACER_ABI_VERSION);
    return nullptr;
  }

  result->m_handle = result->m_vtable->create();
  if (!result->m_handle) {
    outError = QStringLiteral("Plugin create() failed");
    return nullptr;
  }

  return result;
}

PluginFringeTracer::~PluginFringeTracer() {
  if (m_vtable && m_handle)
    m_vtable->destroy(m_handle);
}

bool PluginFringeTracer::initialize(const digitqt::core::Bitmap &image,
                                    std::function<bool(int, int)> isVisible) {
  // Копия предиката живёт в *this, не на стеке -- плагин может вызвать
  // её снова позже, из extract() (см. заголовок).
  m_isVisibleCpp = std::move(isVisible);

  DqtBitmapView view{image.width(), image.height(), image.data()};
  const bool ok =
      m_vtable->initialize(m_handle, &view, &visibilityTrampoline, &m_isVisibleCpp) != 0;
  m_lastErrorCache = m_vtable->lastError(m_handle);
  return ok;
}

std::vector<digitqt::core::tracing::TracedLine> PluginFringeTracer::extract(
    const std::vector<digitqt::core::tracing::SeedPoint> &seeds) {
  std::vector<DqtSeedPoint> cSeeds;
  cSeeds.reserve(seeds.size());
  for (const auto &s : seeds)
    cSeeds.push_back(DqtSeedPoint{s.x, s.y});

  DqtTracedLine *lines = nullptr;
  size_t lineCount = 0;
  m_vtable->extract(m_handle, cSeeds.empty() ? nullptr : cSeeds.data(), cSeeds.size(), &lines,
                    &lineCount);
  m_lastErrorCache = m_vtable->lastError(m_handle);

  std::vector<digitqt::core::tracing::TracedLine> result;
  result.reserve(lineCount);
  m_lastFringeOrders.clear();
  std::vector<double> orders;
  orders.reserve(lineCount);
  bool allHaveOrder = lineCount > 0;
  for (size_t i = 0; i < lineCount; ++i) {
    digitqt::core::tracing::TracedLine line;
    line.reserve(lines[i].count);
    for (size_t j = 0; j < lines[i].count; ++j) {
      const auto &p = lines[i].points[j];
      line.push_back(digitqt::core::tracing::TracedPoint{p.x, p.y, p.width, p.intensity});
    }
    result.push_back(std::move(line));
    orders.push_back(lines[i].order);
    if (!lines[i].hasOrder)
      allHaveOrder = false;
  }
  // Только "всё или ничего" -- если хоть одна линия не имеет вычисленного
  // порядка, считаем, что порядок в принципе не посчитан (см.
  // IFringeTracer::lastFringeOrders()), а не смешиваем частично.
  if (allHaveOrder)
    m_lastFringeOrders = std::move(orders);

  m_vtable->freeLines(m_handle, lines, lineCount);
  return result;
}

std::string PluginFringeTracer::name() const {
  return m_vtable->name(m_handle);
}

const std::string &PluginFringeTracer::lastError() const {
  return m_lastErrorCache;
}

bool PluginFringeTracer::setParam(const std::string &key, const std::string &value) {
  return m_vtable->setParam(m_handle, key.c_str(), value.c_str()) != 0;
}

std::vector<double> PluginFringeTracer::lastFringeOrders() const {
  return m_lastFringeOrders;
}

}  // namespace digitqt::plugin_host
