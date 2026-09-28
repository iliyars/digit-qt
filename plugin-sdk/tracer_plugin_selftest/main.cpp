// Сквозная проверка ИМЕННО того пути, которым реально пользуется
// SetupStage.cpp: core::tracing::tryLoadTracerPlugin() (grep the real
// production entry point, not a manually указанный .dll путь, как в
// plugin_tracer_check) -- для всех 4 алгоритмов трассировки сразу.
//
// Собирается и запускается рядом с DigitQt.exe (см. CMakeLists.txt,
// RUNTIME_OUTPUT_DIRECTORY), чтобы core::plugin_loading::pluginsDirectory()
// нашёл тот же build/.../plugins/tracers/, куда POST_BUILD-шаги плагинов
// кладут .dll.

#include "core/pipeline/stages/fringe_tracing/BinaryThinningTracker.h"
#include "core/pipeline/stages/fringe_tracing/ScanlineExtremumTracker.h"
#include "core/pipeline/stages/fringe_tracing/StructureTensorTracker.h"
#include "core/pipeline/stages/fringe_tracing/TracerPluginLoading.h"
#include "io/ImageLoader.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/visibility/VisibilityChecker.h>

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <memory>

namespace {

using digitqt::core::TracerAlgorithm;
using digitqt::core::tracing::IFringeTracer;
using digitqt::core::tracing::SeedPoint;
using digitqt::core::tracing::TracedLine;

// SequentialTracking has no built-in anymore -- its implementation lives
// only in sequential_fringe_tracker_plugin (see SetupStage.cpp). nullptr
// here means "nothing to diff against", not a failure.
std::unique_ptr<IFringeTracer> makeBuiltin(TracerAlgorithm algorithm) {
  switch (algorithm) {
    case TracerAlgorithm::SequentialTracking:
      return nullptr;
    case TracerAlgorithm::StructureTensor:
      return std::make_unique<digitqt::core::tracing::StructureTensorTracker>();
    case TracerAlgorithm::ScanlineExtremum:
      return std::make_unique<digitqt::core::tracing::ScanlineExtremumTracker>();
    case TracerAlgorithm::BinaryThinning:
      return std::make_unique<digitqt::core::tracing::BinaryThinningTracker>();
  }
  return nullptr;
}

const char *algorithmName(TracerAlgorithm algorithm) {
  switch (algorithm) {
    case TracerAlgorithm::SequentialTracking: return "SequentialTracking";
    case TracerAlgorithm::StructureTensor: return "StructureTensor";
    case TracerAlgorithm::ScanlineExtremum: return "ScanlineExtremum";
    case TracerAlgorithm::BinaryThinning: return "BinaryThinning";
  }
  return "?";
}

bool linesEqual(const std::vector<TracedLine> &a, const std::vector<TracedLine> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].size() != b[i].size())
      return false;
    for (size_t j = 0; j < a[i].size(); ++j) {
      const auto &pa = a[i][j];
      const auto &pb = b[i][j];
      if (pa.x != pb.x || pa.y != pb.y || pa.width != pb.width || pa.intensity != pb.intensity)
        return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QTextStream out(stdout);
  QTextStream err(stderr);

  if (argc < 2) {
    err << "Usage: tracer_plugin_selftest <image.bmp>\n";
    return 1;
  }

  auto loadResult = digitqt::io::loadImage(QString::fromLocal8Bit(argv[1]));
  if (!loadResult.ok()) {
    err << "Failed to load image: " << loadResult.errorMessage << "\n";
    return 1;
  }
  const auto &bitmap = loadResult.image;

  const double cx = bitmap.width() / 2.0;
  const double cy = bitmap.height() / 2.0;
  const double radius = std::min(bitmap.width(), bitmap.height()) / 2.0 - 1.0;
  aperture::ShapeCollection boundaries;
  boundaries.addExternal(std::make_unique<aperture::Ellipse>(radius, radius, cx, cy));
  aperture::VisibilityChecker checker(boundaries);
  auto isVisible = [&checker](int x, int y) {
    return checker.isVisible(aperture::Point{static_cast<double>(x), static_cast<double>(y)});
  };

  const TracerAlgorithm algorithms[] = {
      TracerAlgorithm::SequentialTracking,
      TracerAlgorithm::StructureTensor,
      TracerAlgorithm::ScanlineExtremum,
      TracerAlgorithm::BinaryThinning,
  };
  const std::vector<SeedPoint> seeds = {{static_cast<int>(cx), static_cast<int>(cy - radius / 2)},
                                        {static_cast<int>(cx - radius / 2), static_cast<int>(cy)}};

  bool allOk = true;
  for (auto algorithm : algorithms) {
    auto plugin = digitqt::core::tracing::tryLoadTracerPlugin(algorithm);
    if (!plugin) {
      err << algorithmName(algorithm) << ": FAIL -- plugin not found/failed to load\n";
      allOk = false;
      continue;
    }

    auto builtin = makeBuiltin(algorithm);

    // ScanlineExtremum -- прогоняем ещё и setParam(), как это делает
    // SetupStage, чтобы проверить именно эту часть ABI v2.
    if (algorithm == TracerAlgorithm::ScanlineExtremum) {
      plugin->setParam("fringeCenterMode", "minmax");
      plugin->setParam("hasInternalObstruction", "0");
      builtin->setParam("fringeCenterMode", "minmax");
      builtin->setParam("hasInternalObstruction", "0");
    }

    if (!plugin->initialize(bitmap, isVisible) || (builtin && !builtin->initialize(bitmap, isVisible))) {
      err << algorithmName(algorithm) << ": FAIL -- initialize() failed\n";
      allOk = false;
      continue;
    }

    auto pluginLines = plugin->extract(seeds);

    if (!builtin) {
      // Нет встроенной реализации для сравнения (алгоритм полностью
      // вынесен в плагин) -- проверяем только, что плагин вообще
      // отработал и дал непустой результат.
      const bool ok = !pluginLines.empty();
      allOk = allOk && ok;
      out << algorithmName(algorithm) << ": " << (ok ? "LOADED (no builtin to diff)" : "FAIL -- empty result")
          << " (plugin=" << plugin->name().c_str() << ", lines=" << pluginLines.size() << ")\n";
      continue;
    }

    auto builtinLines = builtin->extract(seeds);
    const bool linesMatch = linesEqual(pluginLines, builtinLines);

    bool ordersMatch = true;
    if (algorithm == TracerAlgorithm::ScanlineExtremum) {
      ordersMatch = plugin->lastFringeOrders() == builtin->lastFringeOrders() &&
                   !plugin->lastFringeOrders().empty();
    }

    const bool ok = linesMatch && ordersMatch;
    allOk = allOk && ok;
    out << algorithmName(algorithm) << ": " << (ok ? "MATCH" : "MISMATCH")
        << " (plugin=" << plugin->name().c_str() << ", lines=" << pluginLines.size()
        << ", ordersMatch=" << (ordersMatch ? "yes" : "no") << ")\n";
  }

  out << (allOk ? "ALL OK\n" : "SOME FAILED\n");
  return allOk ? 0 : 1;
}
