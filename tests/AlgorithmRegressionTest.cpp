#include "core/Bitmap.h"
#include "core/plugin_loading/PluginDirectory.h"
#include "dqt_fringe_tracer_abi.h"
#include "dqt_phase_reconstructor_abi.h"

#include <QLibrary>
#include <QtTest/QtTest>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;

struct Scene {
  digitqt::core::Bitmap image;
  std::vector<uint8_t> mask;
  std::vector<double> truth;
  Scene(int w, int h, double kx, double ky, double bowl, const QString &shape)
      : image(w, h), mask(size_t(w) * h), truth(mask.size()) {
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const double u = (x - (w - 1) / 2.0) / w;
        const double v = (y - (h - 1) / 2.0) / h;
        const double r = std::hypot(u, v);
        const size_t i = size_t(y) * w + x;
        mask[i] = r < .43;
        if (shape == "ring")
          mask[i] &= r > .16;
        if (shape == "split")
          mask[i] = std::hypot(u - .24, v) < .18 || std::hypot(u + .24, v) < .18;
        truth[i] = kx * u + ky * v + 4 * bowl * (u * u + v * v);
        image.setPixel(x, y, uint8_t(std::lround(128 + 90 * std::cos(2 * pi * truth[i]))));
      }
  }
  static int visible(int32_t x, int32_t y, void *user) {
    auto &s = *static_cast<Scene *>(user);
    return x >= 0 && y >= 0 && x < s.image.width() && y < s.image.height() &&
           s.mask[size_t(y) * s.image.width() + x];
  }
};

// Load the actual distributed DLL, not a second implementation of its math.
struct PhasePlugin {
  QLibrary library;
  const DqtPhaseReconstructorVTable *api = nullptr;
  DqtPhaseReconstructorHandle handle = nullptr;
  bool load(const char *name) {
    library.setFileName(
        QString::fromStdString(digitqt::core::plugin_loading::pluginsDirectory("phase")) + "/" +
        name + ".dll");
    if (!library.load())
      return false;
    auto entry = reinterpret_cast<DqtPhasePluginEntryFn>(library.resolve("dqt_phase_plugin_entry"));
    DqtPhasePluginInfo info{};
    return entry && entry(DQT_PHASE_RECONSTRUCTOR_ABI_VERSION, &info, &api) && api &&
           (handle = api->create());
  }
  ~PhasePlugin() {
    if (handle)
      api->destroy(handle);
  }
  bool reconstruct(Scene &s, std::vector<double> &out,
                   const std::vector<DqtPhaseNumberedFringeLine> &lines = {}) {
    // Finite sentinel ensures failure cases actively clear the output.
    out.assign(s.truth.size(), 12345.0);
    DqtPhaseBitmapView view{s.image.width(), s.image.height(), s.image.data()};
    return api->reconstruct(handle, view.width, view.height, &view, &Scene::visible, &s,
                            lines.data(), lines.size(), out.data()) != 0;
  }
};
}  // namespace

class AlgorithmRegressionTest : public QObject {
  Q_OBJECT
private slots:
  void fourierAccuracy_data();
  void fourierAccuracy();
  void fourierRejectsInvalidInput_data();
  void fourierRejectsInvalidInput();
  void scanlineSplineScale_data();
  void scanlineSplineScale();
};

void AlgorithmRegressionTest::fourierAccuracy_data() {
  QTest::addColumn<int>("w");
  QTest::addColumn<int>("h");
  QTest::addColumn<double>("kx");
  QTest::addColumn<double>("ky");
  QTest::addColumn<QString>("shape");
  QTest::newRow("even-vertical") << 128 << 128 << 16.0 << 0.0 << QString("disk");
  QTest::newRow("odd-vertical") << 129 << 129 << 16.0 << 0.0 << QString("disk");
  QTest::newRow("odd-horizontal") << 129 << 129 << 0.0 << 16.0 << QString("disk");
  QTest::newRow("mixed-parity") << 129 << 128 << 12.0 << 9.0 << QString("disk");
  QTest::newRow("rectangular") << 160 << 129 << 12.0 << 9.0 << QString("disk");
  QTest::newRow("ring-even") << 128 << 128 << 16.0 << 0.0 << QString("ring");
  QTest::newRow("ring-odd") << 129 << 129 << 16.0 << 0.0 << QString("ring");
}

void AlgorithmRegressionTest::fourierAccuracy() {
  QFETCH(int, w);
  QFETCH(int, h);
  QFETCH(double, kx);
  QFETCH(double, ky);
  QFETCH(QString, shape);
  Scene s(w, h, kx, ky, 1.0, shape);
  PhasePlugin plugin;
  QVERIFY2(plugin.load("fourier"), qPrintable(plugin.library.errorString()));
  std::vector<double> out;
  QVERIFY2(plugin.reconstruct(s, out), plugin.api->lastError(plugin.handle));
  double sum = 0, sum2 = 0;
  int count = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const size_t i = size_t(y) * w + x;
      QCOMPARE(std::isfinite(out[i]), bool(s.mask[i]));
      const double u = (x - (w - 1) / 2.0) / w;
      const double v = (y - (h - 1) / 2.0) / h;
      // Accuracy ROI is explicit; coverage above checks the entire aperture.
      if (s.mask[i] && std::hypot(u, v) < .30) {
        const double error = out[i] - s.truth[i];
        sum += error;
        sum2 += error * error;
        ++count;
      }
    }
  QVERIFY(count > 100);
  const double rms = std::sqrt(std::max(0.0, sum2 / count - std::pow(sum / count, 2)));
  // Remove piston only: no fitted tilt, gain or best-sign selection.
  QVERIFY2(rms < .01, qPrintable(QString("RMS error = %1 periods").arg(rms, 0, 'g', 12)));
}

void AlgorithmRegressionTest::fourierRejectsInvalidInput_data() {
  QTest::addColumn<QString>("kind");
  for (const char *kind : {"flat", "masked-flat", "noise", "split", "empty", "tiny"})
    QTest::newRow(kind) << QString(kind);
}

void AlgorithmRegressionTest::fourierRejectsInvalidInput() {
  QFETCH(QString, kind);
  Scene s(129, 128, 16, 0, 1, kind);
  if (kind == "flat" || kind == "masked-flat") {
    s.image.fill(128);
    // Strong variation OUTSIDE the aperture must not count as contrast.
    if (kind == "masked-flat")
      for (size_t i = 0; i < s.mask.size(); ++i)
        if (!s.mask[i])
          s.image.data()[i] = i % 2 ? 0 : 255;
  }
  if (kind == "noise") {
    std::mt19937 random(20261101);
    for (size_t i = 0; i < s.mask.size(); ++i)
      s.image.data()[i] = uint8_t(random() & 255);
  }
  if (kind == "empty" || kind == "tiny") {
    std::fill(s.mask.begin(), s.mask.end(), 0);
    if (kind == "tiny")
      std::fill_n(s.mask.begin(), 50, 1);
  }
  PhasePlugin plugin;
  QVERIFY2(plugin.load("fourier"), qPrintable(plugin.library.errorString()));
  std::vector<double> out;
  QVERIFY(!plugin.reconstruct(s, out));
  QVERIFY(std::strlen(plugin.api->lastError(plugin.handle)) > 0);
  for (double v : out)
    QVERIFY(std::isnan(v));
  // A failed call must not poison the next calculation on the same handle.
  Scene good(128, 128, 16, 0, .25, "disk");
  QVERIFY2(plugin.reconstruct(good, out), plugin.api->lastError(plugin.handle));
  QCOMPARE(QString::fromUtf8(plugin.api->lastError(plugin.handle)), QString());
}

void AlgorithmRegressionTest::scanlineSplineScale_data() {
  QTest::addColumn<QByteArray>("mode");
  QTest::addColumn<bool>("horizontal");
  for (const char *mode : {"max", "min", "minmax"}) {
    QTest::newRow(qPrintable(QString(mode) + "-vertical")) << QByteArray(mode) << false;
    QTest::newRow(qPrintable(QString(mode) + "-horizontal")) << QByteArray(mode) << true;
  }
}

void AlgorithmRegressionTest::scanlineSplineScale() {
  QFETCH(QByteArray, mode);
  QFETCH(bool, horizontal);
  Scene s(128, 128, horizontal ? 0 : 8, horizontal ? 8 : 0, 0, "disk");
  QLibrary tracer(
      QString::fromStdString(digitqt::core::plugin_loading::pluginsDirectory("tracers")) +
      "/scanline_extremum_tracker.dll");
  QVERIFY2(tracer.load(), qPrintable(tracer.errorString()));
  auto entry = reinterpret_cast<DqtPluginEntryFn>(tracer.resolve("dqt_plugin_entry"));
  QVERIFY(entry);
  const DqtFringeTracerVTable *api = nullptr;
  DqtPluginInfo info{};
  QVERIFY(entry(DQT_FRINGE_TRACER_ABI_VERSION, &info, &api));
  QVERIFY(api);
  auto handle = api->create();
  QVERIFY(handle);
  struct Guard {
    const DqtFringeTracerVTable *api;
    DqtFringeTracerHandle handle;
    ~Guard() { api->destroy(handle); }
  } guard{api, handle};
  QVERIFY(api->setParam(handle, "fringeCenterMode", mode.constData()));
  DqtBitmapView bitmap{s.image.width(), s.image.height(), s.image.data()};
  QVERIFY(api->initialize(handle, &bitmap, &Scene::visible, &s));
  DqtTracedLine *traced = nullptr;
  size_t count = 0;
  QVERIFY(api->extract(handle, nullptr, 0, &traced, &count));
  QVERIFY(count >= 4);
  std::vector<std::vector<DqtPhasePoint>> storage(count);
  std::vector<DqtPhaseNumberedFringeLine> lines;
  for (size_t i = 0; i < count; ++i) {
    QVERIFY(traced[i].hasOrder);
    for (size_t j = 0; j < traced[i].count; ++j)
      storage[i].push_back({traced[i].points[j].x, traced[i].points[j].y});
    lines.push_back({storage[i].data(), storage[i].size(), traced[i].order});
  }
  api->freeLines(handle, traced, count);
  PhasePlugin spline;
  QVERIFY2(spline.load("horizontal_spline"), qPrintable(spline.library.errorString()));
  std::vector<double> out;
  QVERIFY2(spline.reconstruct(s, out, lines), spline.api->lastError(spline.handle));
  double t = 0, z = 0, tt = 0, tz = 0, zz = 0;
  int n = 0;
  for (int y = 0; y < 128; ++y)
    for (int x = 0; x < 128; ++x) {
      const size_t i = size_t(y) * 128 + x;
      if (std::hypot(x - 63.5, y - 63.5) >= 38)
        continue;
      QVERIFY(std::isfinite(out[i]));
      t += s.truth[i];
      z += out[i];
      tt += s.truth[i] * s.truth[i];
      tz += s.truth[i] * out[i];
      zz += out[i] * out[i];
      ++n;
    }
  const double slope = (tz - t * z / n) / (tt - t * t / n);
  QVERIFY2(std::abs(slope - 1) < .005,
           qPrintable(QString("Reconstructed phase scale = %1").arg(slope, 0, 'g', 12)));
  // Constrain shape error too, removing only a constant offset.
  const double meanError = (z - t) / n;
  const double rms = std::sqrt(std::max(0.0, (zz + tt - 2 * tz) / n - meanError * meanError));
  QVERIFY2(rms < .01, qPrintable(QString("RMS error = %1 periods").arg(rms)));
}

QTEST_GUILESS_MAIN(AlgorithmRegressionTest)
#include "AlgorithmRegressionTest.moc"
