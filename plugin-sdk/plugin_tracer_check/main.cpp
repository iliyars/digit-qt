// Грузит один и тот же DqtFringeTracer-плагин ДВУМЯ независимыми
// загрузчиками -- plugin_host::PluginFringeTracer (QLibrary, используется
// только этим инструментом сейчас) и core::tracing::DllFringeTracer
// (Win32Library, реально используется SetupStage через
// tryLoadTracerPlugin()) -- и сверяет результат, чтобы убедиться: оба
// независимых по коду загрузчика ABI ведут себя идентично на одном и том
// же плагине.
//
// До выноса всех 4 трекеров в самодостаточные плагины (см. историю в
// памяти qt_decoupling_and_plugin_abi) здесь была сверка с ВСТРОЕННЫМ
// core::tracing::BinaryThinningTracker -- этого класса больше не
// существует, поэтому сравнение теперь между двумя загрузчиками, а не
// загрузчиком и built-in.

#include "core/pipeline/stages/fringe_tracing/DllFringeTracer.h"
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

  QString qtLoadError;
  auto qtLoadedPlugin =
      digitqt::plugin_host::PluginFringeTracer::load(QString::fromLocal8Bit(argv[1]), qtLoadError);
  if (!qtLoadedPlugin) {
    err << "Failed to load plugin via PluginFringeTracer (QLibrary): " << qtLoadError << "\n";
    return 1;
  }
  out << "Loaded via PluginFringeTracer (QLibrary): "
      << QString::fromStdString(qtLoadedPlugin->name()) << "\n";

  std::string win32LoadError;
  auto win32LoadedPlugin = digitqt::core::tracing::DllFringeTracer::load(
      std::string(argv[1]), win32LoadError);
  if (!win32LoadedPlugin) {
    err << "Failed to load plugin via DllFringeTracer (Win32Library): "
        << QString::fromStdString(win32LoadError) << "\n";
    return 1;
  }
  out << "Loaded via DllFringeTracer (Win32Library): "
      << QString::fromStdString(win32LoadedPlugin->name()) << "\n";

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

  if (!qtLoadedPlugin->initialize(bitmap, isVisible)) {
    err << "PluginFringeTracer initialize() failed: "
        << QString::fromStdString(qtLoadedPlugin->lastError()) << "\n";
    return 1;
  }
  if (!win32LoadedPlugin->initialize(bitmap, isVisible)) {
    err << "DllFringeTracer initialize() failed: "
        << QString::fromStdString(win32LoadedPlugin->lastError()) << "\n";
    return 1;
  }

  auto qtLines = qtLoadedPlugin->extract({});
  auto win32Lines = win32LoadedPlugin->extract({});

  size_t qtPointTotal = 0;
  for (const auto &line : qtLines)
    qtPointTotal += line.size();
  size_t win32PointTotal = 0;
  for (const auto &line : win32Lines)
    win32PointTotal += line.size();

  out << "QLibrary loader:    lines=" << qtLines.size() << " points=" << qtPointTotal << "\n";
  out << "Win32Library loader: lines=" << win32Lines.size() << " points=" << win32PointTotal
      << "\n";

  bool match = qtLines.size() == win32Lines.size();
  for (size_t i = 0; match && i < qtLines.size(); ++i) {
    if (qtLines[i].size() != win32Lines[i].size()) {
      match = false;
      break;
    }
    for (size_t j = 0; j < qtLines[i].size(); ++j) {
      const auto &qp = qtLines[i][j];
      const auto &wp = win32Lines[i][j];
      if (qp.x != wp.x || qp.y != wp.y || qp.width != wp.width || qp.intensity != wp.intensity) {
        match = false;
        break;
      }
    }
  }

  out << (match ? "MATCH -- both ABI loaders agree\n" : "MISMATCH -- see above\n");
  return match ? 0 : 1;
}
