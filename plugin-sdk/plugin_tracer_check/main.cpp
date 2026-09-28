// Грузит binary_thinning_poc_plugin через plugin_host::PluginFringeTracer
// (обёртка вокруг DqtFringeTracer C ABI -- см. src/plugin_host/) и
// сверяет результат со встроенным BinaryThinningTracker (тот же алгоритм,
// вызванный напрямую через C++), чтобы убедиться: сама обёртка, которую
// будет использовать остальное приложение, ведёт себя ТОЧНО так же, как
// прямые вызовы через сырой vtable (что уже было проверено раньше этим
// же инструментом).

#include "core/pipeline/stages/fringe_tracing/BinaryThinningTracker.h"
#include "io/ImageLoader.h"
#include "plugin_host/PluginFringeTracer.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/visibility/VisibilityChecker.h>

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <memory>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QTextStream out(stdout);
  QTextStream err(stderr);

  if (argc < 3) {
    err << "Usage: plugin_tracer_check <plugin.dll> <image.bmp>\n";
    return 1;
  }

  QString loadError;
  auto plugin = digitqt::plugin_host::PluginFringeTracer::load(
      QString::fromLocal8Bit(argv[1]), loadError);
  if (!plugin) {
    err << "Failed to load plugin: " << loadError << "\n";
    return 1;
  }
  out << "Loaded plugin: " << QString::fromStdString(plugin->name()) << "\n";

  auto loadResult = digitqt::io::loadImage(QString::fromLocal8Bit(argv[2]));
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

  // --- Через обёртку (использует ABI под капотом) ---
  if (!plugin->initialize(bitmap, isVisible)) {
    err << "Plugin initialize() failed: " << QString::fromStdString(plugin->lastError()) << "\n";
    return 1;
  }
  auto pluginLines = plugin->extract({});

  size_t pluginPointTotal = 0;
  for (const auto &line : pluginLines)
    pluginPointTotal += line.size();

  // --- Напрямую, встроенным трекером ---
  digitqt::core::tracing::BinaryThinningTracker builtin;
  builtin.initialize(bitmap, isVisible);
  auto builtinLines = builtin.extract({});
  size_t builtinPointTotal = 0;
  for (const auto &line : builtinLines)
    builtinPointTotal += line.size();

  out << "Plugin:  lines=" << pluginLines.size() << " points=" << pluginPointTotal << "\n";
  out << "Builtin: lines=" << builtinLines.size() << " points=" << builtinPointTotal << "\n";

  bool match = pluginLines.size() == builtinLines.size();
  for (size_t i = 0; match && i < pluginLines.size(); ++i) {
    if (pluginLines[i].size() != builtinLines[i].size()) {
      match = false;
      break;
    }
    for (size_t j = 0; j < pluginLines[i].size(); ++j) {
      const auto &pp = pluginLines[i][j];
      const auto &bp = builtinLines[i][j];
      if (pp.x != bp.x || pp.y != bp.y || pp.width != bp.width || pp.intensity != bp.intensity) {
        match = false;
        break;
      }
    }
  }

  out << (match ? "MATCH -- PluginFringeTracer wrapper is exact\n" : "MISMATCH -- see above\n");
  return match ? 0 : 1;
}
