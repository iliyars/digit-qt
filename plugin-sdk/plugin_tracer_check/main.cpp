// Грузит binary_thinning_poc_plugin через QLibrary, прогоняет его через
// DqtFringeTracer C ABI на реальном изображении и сверяет результат со
// встроенным BinaryThinningTracker (тот же алгоритм, вызванный напрямую
// через C++) -- чтобы убедиться, что путь через ABI (Bitmap ->
// DqtBitmapView, предикат isVisible, владение TracedLine[]) не теряет и
// не искажает ничего по дороге.

#include "dqt_fringe_tracer_abi.h"

#include "core/pipeline/stages/fringe_tracing/BinaryThinningTracker.h"
#include "io/ImageLoader.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/visibility/VisibilityChecker.h>

#include <QCoreApplication>
#include <QLibrary>
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

  QLibrary lib(QString::fromLocal8Bit(argv[1]));
  if (!lib.load()) {
    err << "Failed to load plugin: " << lib.errorString() << "\n";
    return 1;
  }

  auto entry = reinterpret_cast<DqtPluginEntryFn>(lib.resolve("dqt_plugin_entry"));
  if (!entry) {
    err << "Plugin does not export dqt_plugin_entry\n";
    return 1;
  }

  DqtPluginInfo info{};
  const DqtFringeTracerVTable *vtable = nullptr;
  if (!entry(DQT_FRINGE_TRACER_ABI_VERSION, &info, &vtable) || !vtable) {
    err << "Plugin refused (ABI version mismatch?)\n";
    return 1;
  }
  out << "Loaded plugin: " << info.pluginName << " " << info.pluginVersion << "\n";

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

  auto isVisibleCpp = [&checker](int x, int y) {
    return checker.isVisible(aperture::Point{static_cast<double>(x), static_cast<double>(y)});
  };
  auto isVisibleC = [](int32_t x, int32_t y, void *userData) -> int {
    auto *fn = reinterpret_cast<decltype(&isVisibleCpp)>(userData);
    return (*fn)(x, y) ? 1 : 0;
  };

  // --- Через ABI/плагин ---
  DqtFringeTracerHandle handle = vtable->create();
  DqtBitmapView view{bitmap.width(), bitmap.height(), bitmap.data()};
  if (!vtable->initialize(handle, &view, isVisibleC, &isVisibleCpp)) {
    err << "Plugin initialize() failed: " << vtable->lastError(handle) << "\n";
    return 1;
  }
  DqtTracedLine *pluginLines = nullptr;
  size_t pluginLineCount = 0;
  vtable->extract(handle, nullptr, 0, &pluginLines, &pluginLineCount);

  size_t pluginPointTotal = 0;
  for (size_t i = 0; i < pluginLineCount; ++i)
    pluginPointTotal += pluginLines[i].count;

  // --- Напрямую, встроенным трекером ---
  digitqt::core::tracing::BinaryThinningTracker builtin;
  builtin.initialize(bitmap, isVisibleCpp);
  auto builtinLines = builtin.extract({});
  size_t builtinPointTotal = 0;
  for (const auto &line : builtinLines)
    builtinPointTotal += line.size();

  out << "Plugin:  lines=" << pluginLineCount << " points=" << pluginPointTotal << "\n";
  out << "Builtin: lines=" << builtinLines.size() << " points=" << builtinPointTotal << "\n";

  bool match = pluginLineCount == builtinLines.size();
  for (size_t i = 0; match && i < pluginLineCount; ++i) {
    if (pluginLines[i].count != builtinLines[i].size()) {
      match = false;
      break;
    }
    for (size_t j = 0; j < pluginLines[i].count; ++j) {
      const auto &pp = pluginLines[i].points[j];
      const auto &bp = builtinLines[i][j];
      if (pp.x != bp.x || pp.y != bp.y || pp.width != bp.width || pp.intensity != bp.intensity) {
        match = false;
        break;
      }
    }
  }

  vtable->freeLines(handle, pluginLines, pluginLineCount);
  vtable->destroy(handle);

  out << (match ? "MATCH -- ABI round-trip is exact\n" : "MISMATCH -- see above\n");
  return match ? 0 : 1;
}
