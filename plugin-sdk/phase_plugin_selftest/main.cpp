// Сквозная проверка ИМЕННО того пути, которым реально пользуется
// PhaseReconstructionStage.cpp: core::tryLoadPhaseReconstructorPlugin()
// -- для всех 3 методов сшивки фазы сразу, со сверкой против всё ещё
// существующих встроенных реализаций (в отличие от трассировки, где
// built-in уже нет -- см. tracer_plugin_selftest).
//
// Собирается и запускается рядом с DigitQt.exe (см. CMakeLists.txt,
// RUNTIME_OUTPUT_DIRECTORY), чтобы core::plugin_loading::pluginsDirectory()
// нашёл тот же build/.../plugins/phase/, куда POST_BUILD-шаги плагинов
// кладут .dll.

#include "core/Measurement.h"
#include "core/pipeline/stages/PhaseReconstructionStage.h"
#include "core/pipeline/stages/SetupStage.h"
#include "core/pipeline/stages/phase_reconstruction/FourierPhaseExtractor.h"
#include "core/pipeline/stages/phase_reconstruction/PhasePluginLoading.h"
#include "core/pipeline/stages/phase_reconstruction/PhaseReconstructor.h"
#include "core/pipeline/stages/phase_reconstruction/WaveletPhaseExtractor.h"
#include "core/plugin_loading/PluginDirectory.h"
#include "io/ImageLoader.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/visibility/VisibilityChecker.h>

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

using digitqt::core::IPhaseReconstructor;
using digitqt::core::PhaseReconstructionAlgorithm;

std::unique_ptr<IPhaseReconstructor> makeBuiltin(PhaseReconstructionAlgorithm algorithm) {
  switch (algorithm) {
    case PhaseReconstructionAlgorithm::HorizontalSpline:
      return std::make_unique<digitqt::core::pipeline::PhaseReconstructor>();
    case PhaseReconstructionAlgorithm::FourierTransform:
      return std::make_unique<digitqt::core::pipeline::FourierPhaseExtractor>();
    case PhaseReconstructionAlgorithm::WaveletTransform:
      return std::make_unique<digitqt::core::pipeline::WaveletPhaseExtractor>();
  }
  return nullptr;
}

const char *algorithmName(PhaseReconstructionAlgorithm algorithm) {
  switch (algorithm) {
    case PhaseReconstructionAlgorithm::HorizontalSpline: return "HorizontalSpline";
    case PhaseReconstructionAlgorithm::FourierTransform: return "FourierTransform";
    case PhaseReconstructionAlgorithm::WaveletTransform: return "WaveletTransform";
  }
  return "?";
}

// Сравнивает две PhaseMap поточечно, допуская небольшую погрешность --
// плагин и built-in делают идентичную арифметику, но проходят через
// разные пути хранения промежуточных double (ABI-буфер туда-обратно),
// так что бит-в-бит не гарантирован в отличие от трассировки (там
// сравнивались только исходные double от алгоритма, тут ещё копирование
// через плоский буфер).
bool phaseMapsMatch(const digitqt::core::PhaseMap &a, const digitqt::core::PhaseMap &b,
                    double tolerance) {
  if (a.width() != b.width() || a.height() != b.height())
    return false;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      const bool ha = a.hasValue(x, y);
      const bool hb = b.hasValue(x, y);
      if (ha != hb)
        return false;
      if (ha && std::abs(a.value(x, y) - b.value(x, y)) > tolerance)
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
    err << "Usage: phase_plugin_selftest <image.bmp>\n";
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

  // HorizontalSpline: пара несложных прямых линий поперёк апертуры --
  // этому методу нужны реальные пересечения линий с каждой строкой, не
  // просто "хоть какие-то" данные.
  std::vector<digitqt::core::NumberedFringeLine> lines;
  for (int i = 0; i < 5; ++i) {
    digitqt::core::NumberedFringeLine line;
    line.order = i;
    const double x = cx - radius + i * (2.0 * radius / 4.0);
    for (int y = 0; y < bitmap.height(); y += 4) {
      digitqt::core::tracing::TracedPoint p;
      p.x = x;
      p.y = y;
      line.points.push_back(p);
    }
    lines.push_back(std::move(line));
  }

  const PhaseReconstructionAlgorithm algorithms[] = {
      PhaseReconstructionAlgorithm::HorizontalSpline,
      PhaseReconstructionAlgorithm::FourierTransform,
      PhaseReconstructionAlgorithm::WaveletTransform,
  };

  bool allOk = true;
  for (auto algorithm : algorithms) {
    auto plugin = digitqt::core::tryLoadPhaseReconstructorPlugin(algorithm);
    if (!plugin) {
      err << algorithmName(algorithm) << ": FAIL -- plugin not found/failed to load\n";
      allOk = false;
      continue;
    }

    auto builtin = makeBuiltin(algorithm);

    const int gridWidth = bitmap.width();
    const int gridHeight = bitmap.height();
    static const std::vector<digitqt::core::NumberedFringeLine> kNoLines;
    const auto &lineArg =
        (algorithm == PhaseReconstructionAlgorithm::HorizontalSpline) ? lines : kNoLines;

    auto pluginMap = plugin->reconstruct(gridWidth, gridHeight, bitmap, isVisible, lineArg);
    auto builtinMap = builtin->reconstruct(gridWidth, gridHeight, bitmap, isVisible, lineArg);

    if (pluginMap.isEmpty() || builtinMap.isEmpty()) {
      err << algorithmName(algorithm) << ": FAIL -- plugin.isEmpty()=" << pluginMap.isEmpty()
          << " builtin.isEmpty()=" << builtinMap.isEmpty()
          << " pluginError=" << plugin->lastError().c_str()
          << " builtinError=" << builtin->lastError().c_str() << "\n";
      allOk = false;
      continue;
    }

    const bool match = phaseMapsMatch(pluginMap, builtinMap, 1e-6);
    allOk = allOk && match;
    out << algorithmName(algorithm) << ": " << (match ? "MATCH" : "MISMATCH")
        << " (plugin=" << plugin->name().c_str() << ")\n";
  }

  // Проверка обнаружения: discoverPhaseReconstructorPlugins() должна
  // найти все 3 .dll в plugins/phase/, каждый со своим правильным
  // needsFringeLines -- не по enum, а по тому, что плагин сам заявил о
  // себе через DqtPhasePluginInfo (ABI v2).
  out << "--- discoverPhaseReconstructorPlugins() ---\n";
  auto discovered = digitqt::core::discoverPhaseReconstructorPlugins();
  if (discovered.size() != 3) {
    err << "FAIL -- expected 3 discovered plugins, found " << discovered.size() << "\n";
    allOk = false;
  }
  bool sawHorizontalSpline = false, sawFourier = false, sawWavelet = false;
  for (const auto &d : discovered) {
    out << "  " << d.pluginName.c_str() << " v" << d.pluginVersion.c_str()
        << " needsFringeLines=" << (d.needsFringeLines ? "yes" : "no") << " (" << d.filePath.c_str()
        << ")\n";
    if (d.pluginName == "HorizontalSplinePhaseReconstructor") {
      sawHorizontalSpline = true;
      if (!d.needsFringeLines) {
        err << "FAIL -- HorizontalSpline should have needsFringeLines=true\n";
        allOk = false;
      }
    } else if (d.pluginName == "FourierPhaseExtractor") {
      sawFourier = true;
      if (d.needsFringeLines) {
        err << "FAIL -- FourierPhaseExtractor should have needsFringeLines=false\n";
        allOk = false;
      }
    } else if (d.pluginName == "WaveletPhaseExtractor") {
      sawWavelet = true;
      if (d.needsFringeLines) {
        err << "FAIL -- WaveletPhaseExtractor should have needsFringeLines=false\n";
        allOk = false;
      }
    }
  }
  if (!sawHorizontalSpline || !sawFourier || !sawWavelet) {
    err << "FAIL -- discovery didn't find all 3 expected plugins by name\n";
    allOk = false;
  }

  // Сквозная проверка того, что реально соберёт ParametersDock/
  // PhaseReconstructionStage.cpp: Measurement::customPhaseReconstructorPluginPath()
  // выбирает плагин по ПУТИ, а не по enum -- SetupStage должен сам решить,
  // нужна ли трассировка (S1), спросив у плагина needsFringeLines(), а
  // PhaseReconstructionStage -- реально посчитать через него карту фазы.
  out << "--- custom plugin path (SetupStage + PhaseReconstructionStage) ---\n";
  const std::string phaseDir = digitqt::core::plugin_loading::pluginsDirectory("phase");

  auto runCustomPluginCase = [&](const char *label, const std::string &dllName,
                                 bool expectTracing) {
    digitqt::core::Measurement measurement;
    measurement.setImage(bitmap, "selftest");
    measurement.boundaries().addExternal(std::make_unique<aperture::Ellipse>(radius, radius, cx, cy));
    measurement.fringeTracing().setAlgorithm(digitqt::core::TracerAlgorithm::ScanlineExtremum);
    measurement.setCustomPhaseReconstructorPluginPath(phaseDir + "/" + dllName);

    digitqt::core::pipeline::SetupStage setup;
    if (!setup.compute(measurement)) {
      err << label << ": FAIL -- SetupStage: " << setup.errorMessage().c_str() << "\n";
      allOk = false;
      return;
    }
    const bool tracedSomething = !measurement.fringeTracing().tracedLines().empty();
    if (tracedSomething != expectTracing) {
      err << label << ": FAIL -- expected tracing=" << expectTracing
          << " but tracedLines().empty()=" << !tracedSomething << "\n";
      allOk = false;
      return;
    }

    digitqt::core::pipeline::PhaseReconstructionStage phase;
    if (!phase.compute(measurement)) {
      err << label << ": FAIL -- PhaseReconstructionStage: " << phase.errorMessage().c_str() << "\n";
      allOk = false;
      return;
    }
    const bool ok = !measurement.phaseMap().isEmpty();
    allOk = allOk && ok;
    out << label << ": " << (ok ? "OK" : "FAIL -- empty phaseMap")
        << " (tracedLines=" << measurement.fringeTracing().tracedLines().size() << ")\n";
  };

  runCustomPluginCase("custom=fourier.dll (needsFringeLines=false)", "fourier.dll",
                     /*expectTracing=*/false);
  runCustomPluginCase("custom=horizontal_spline.dll (needsFringeLines=true)", "horizontal_spline.dll",
                     /*expectTracing=*/true);

  out << (allOk ? "ALL OK\n" : "SOME FAILED\n");
  return allOk ? 0 : 1;
}
