// Сквозная проверка ИМЕННО того пути, которым реально пользуется
// SetupStage.cpp: core::tracing::tryLoadTracerPlugin() -- для всех 4
// алгоритмов трассировки сразу. Ни у одного нет больше встроенной
// реализации в core (все 4 -- самодостаточные плагины, см. историю в
// памяти qt_decoupling_and_plugin_abi), так что здесь только проверяется
// "плагин загрузился и дал разумный результат", без diff против builtin.
//
// Собирается и запускается рядом с DigitQt.exe (см. CMakeLists.txt,
// RUNTIME_OUTPUT_DIRECTORY), чтобы core::plugin_loading::pluginsDirectory()
// нашёл тот же build/.../plugins/tracers/, куда POST_BUILD-шаги плагинов
// кладут .dll.

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
using digitqt::core::tracing::SeedPoint;

const char *algorithmName(TracerAlgorithm algorithm) {
  switch (algorithm) {
    case TracerAlgorithm::SequentialTracking: return "SequentialTracking";
    case TracerAlgorithm::StructureTensor: return "StructureTensor";
    case TracerAlgorithm::ScanlineExtremum: return "ScanlineExtremum";
    case TracerAlgorithm::BinaryThinning: return "BinaryThinning";
  }
  return "?";
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

    // ScanlineExtremum -- прогоняем ещё и setParam(), как это делает
    // SetupStage, чтобы проверить именно эту часть ABI v2.
    if (algorithm == TracerAlgorithm::ScanlineExtremum) {
      plugin->setParam("fringeCenterMode", "minmax");
      plugin->setParam("hasInternalObstruction", "0");
    }

    if (!plugin->initialize(bitmap, isVisible)) {
      err << algorithmName(algorithm) << ": FAIL -- initialize() failed: " << plugin->lastError().c_str()
          << "\n";
      allOk = false;
      continue;
    }

    auto lines = plugin->extract(seeds);
    bool ok = !lines.empty();

    bool ordersOk = true;
    if (algorithm == TracerAlgorithm::ScanlineExtremum) {
      // ScanlineExtremum -- единственный, что должен сам посчитать
      // номер полосы (см. IFringeTracer::lastFringeOrders()).
      const auto orders = plugin->lastFringeOrders();
      ordersOk = !orders.empty() && orders.size() == lines.size();
      ok = ok && ordersOk;
    }

    allOk = allOk && ok;
    out << algorithmName(algorithm) << ": " << (ok ? "OK" : "FAIL") << " (plugin="
        << plugin->name().c_str() << ", lines=" << lines.size()
        << (algorithm == TracerAlgorithm::ScanlineExtremum
                ? (ordersOk ? ", ordersOk=yes" : ", ordersOk=NO")
                : "")
        << ")\n";
    out.flush();
  }

  out << (allOk ? "ALL OK\n" : "SOME FAILED\n");
  return allOk ? 0 : 1;
}
