#include "core/Bitmap.h"
#include "core/AutoSeedPlacement.h"
#include "core/FringeOrdering.h"
#include "core/pipeline/stages/fringe_tracing/TracerPluginLoading.h"
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
  void curvedFringesDoNotJumpBetweenOrders();
  void sequentialSeededTracing_data();
  void sequentialSeededTracing();
  void autoSeedsCrossTheFringes();
  void fringeOrderFollowsCommonSections();
  void sequentialSplinePreservesPhase();
  void splineRimReconstruction_data();
  void splineRimReconstruction();
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

void AlgorithmRegressionTest::curvedFringesDoNotJumpBetweenOrders() {
  for (const char *mode : {"max", "min", "minmax"}) {
    Scene s(512, 512, 6, 12, 2, "disk");
    QLibrary tracer(QString::fromStdString(
                        digitqt::core::plugin_loading::pluginsDirectory("tracers")) +
                    "/scanline_extremum_tracker.dll");
    QVERIFY2(tracer.load(), qPrintable(tracer.errorString()));
    auto entry = reinterpret_cast<DqtPluginEntryFn>(tracer.resolve("dqt_plugin_entry"));
    QVERIFY(entry);
    const DqtFringeTracerVTable *api = nullptr;
    DqtPluginInfo info{};
    QVERIFY(entry(DQT_FRINGE_TRACER_ABI_VERSION, &info, &api));
    auto handle = api->create();
    QVERIFY(handle);
    struct Guard {
      const DqtFringeTracerVTable *api;
      DqtFringeTracerHandle handle;
      ~Guard() { api->destroy(handle); }
    } guard{api, handle};
    QVERIFY(api->setParam(handle, "fringeCenterMode", mode));
    DqtBitmapView bitmap{s.image.width(), s.image.height(), s.image.data()};
    QVERIFY(api->initialize(handle, &bitmap, &Scene::visible, &s));
    DqtTracedLine *lines = nullptr;
    size_t count = 0;
    QVERIFY(api->extract(handle, nullptr, 0, &lines, &count));
    double worstRange = 0;
    size_t checked = 0;
    for (size_t i = 0; i < count; ++i) {
      double low = 1e9, high = -1e9;
      for (size_t j = 0; j < lines[i].count; ++j) {
        const auto &p = lines[i].points[j];
        const double u = (p.x - 255.5) / 512, v = (p.y - 255.5) / 512;
        if (std::hypot(u, v) > .36)
          continue; // Exclude extrapolated endpoints near the pupil rim.
        const double phase = 6 * u + 12 * v + 8 * (u * u + v * v);
        low = std::min(low, phase);
        high = std::max(high, phase);
      }
      if (high >= low) {
        ++checked;
        worstRange = std::max(worstRange, high - low);
      }
    }
    api->freeLines(handle, lines, count);
    QVERIFY(checked >= 8);
    QVERIFY2(worstRange < .12,
             qPrintable(QString("%1: phase variation along a fringe = %2 periods")
                            .arg(mode).arg(worstRange)));
  }
}

void AlgorithmRegressionTest::sequentialSeededTracing_data() {
  QTest::addColumn<double>("kx");
  QTest::addColumn<double>("ky");
  QTest::addColumn<double>("bowl");
  QTest::addColumn<int>("seedX");
  QTest::addColumn<int>("seedY");
  QTest::newRow("vertical") << 8.0 << 0.0 << 0.0 << 256 << 256;
  QTest::newRow("horizontal") << 0.0 << 8.0 << 0.0 << 256 << 256;
  QTest::newRow("diagonal") << 6.0 << 6.0 << 0.0 << 256 << 256;
  QTest::newRow("curved") << 6.0 << 12.0 << 2.0 << 256 << 256;
  QTest::newRow("low-frequency") << 2.0 << 0.0 << 0.0 << 256 << 256;
  QTest::newRow("seed-near-rim") << 8.0 << 0.0 << 0.0 << 256 << 465;
  QTest::newRow("seed-near-opposite-rim") << 8.0 << 0.0 << 0.0 << 256 << 46;
}

void AlgorithmRegressionTest::sequentialSeededTracing() {
  QFETCH(double, kx); QFETCH(double, ky); QFETCH(double, bowl);
  QFETCH(int, seedX); QFETCH(int, seedY);
  Scene s(512, 512, kx, ky, bowl, "disk");
  QLibrary library(QString::fromStdString(
      digitqt::core::plugin_loading::pluginsDirectory("tracers")) +
      "/sequential_fringe_tracker.dll");
  QVERIFY2(library.load(), qPrintable(library.errorString()));
  auto entry = reinterpret_cast<DqtPluginEntryFn>(library.resolve("dqt_plugin_entry"));
  QVERIFY(entry);
  const DqtFringeTracerVTable *api=nullptr; DqtPluginInfo info{};
  QVERIFY(entry(DQT_FRINGE_TRACER_ABI_VERSION, &info, &api));
  auto handle=api->create(); QVERIFY(handle);
  struct Guard {const DqtFringeTracerVTable *a;DqtFringeTracerHandle h;~Guard(){a->destroy(h);}} guard{api,handle};
  DqtBitmapView bitmap{512,512,s.image.data()};
  QVERIFY(api->initialize(handle,&bitmap,&Scene::visible,&s));
  DqtSeedPoint seed{seedX,seedY}; DqtTracedLine *lines=nullptr; size_t count=0;
  QVERIFY(api->extract(handle,&seed,1,&lines,&count));
  QVERIFY2(count==1, api->lastError(handle));
  double low=1e9,high=-1e9,minX=1e9,maxX=-1e9,minY=1e9,maxY=-1e9;
  for(size_t j=0;j<lines[0].count;++j) {
    const auto &p=lines[0].points[j];
    const double u=(p.x-255.5)/512,v=(p.y-255.5)/512;
    QVERIFY(Scene::visible(int(std::lround(p.x)),int(std::lround(p.y)),&s));
    if(std::hypot(u,v)>.37)continue;
    const double phase=kx*u+ky*v+4*bowl*(u*u+v*v);
    low=std::min(low,phase);high=std::max(high,phase);
    minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
    minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
  }
  api->freeLines(handle,lines,count);
  QVERIFY2(high-low<.15,qPrintable(QString("Phase range: %1 cycles").arg(high-low)));
  QVERIFY2(std::hypot(maxX-minX,maxY-minY)>300,
           qPrintable(QString("Traced span: %1 px").arg(std::hypot(maxX-minX,maxY-minY))));
}

void AlgorithmRegressionTest::autoSeedsCrossTheFringes() {
  for (bool horizontal : {false,true}) {
    Scene s(256,256,horizontal?0:8,horizontal?8:0,0,"disk");
    const auto seeds=digitqt::core::findRowSeeds(s.image,[&](int x,int y){return Scene::visible(x,y,&s);});
    QVERIFY(seeds.size()>=6);
    for (const auto &seed:seeds) {
      QVERIFY(Scene::visible(seed.x,seed.y,&s));
      QVERIFY(s.image.pixel(seed.x,seed.y)>200);
    }
    if(horizontal)QVERIFY(seeds.front().y<seeds.back().y);
    else QVERIFY(seeds.front().x<seeds.back().x);
  }
}

void AlgorithmRegressionTest::fringeOrderFollowsCommonSections() {
  std::vector<digitqt::core::NumberedFringeLine> lines(3);
  // The pupil clips each curve differently. Mean X would number them 1,0,2.
  for (int i=0;i<3;++i)
    for (int x=(i==1?0:20);x<=(i==2?70:100);++x) {
      digitqt::core::tracing::TracedPoint p;
      p.x=x;p.y=20*i+.002*x*x;
      lines[i].points.push_back(p);
    }
  digitqt::core::autoAssignFringeOrder(lines);
  for(int i=0;i<3;++i)QCOMPARE(lines[i].order,double(i));
  lines[1].order=12.5;lines[1].orderIsManual=true;
  digitqt::core::autoAssignFringeOrder(lines);
  QCOMPARE(lines[1].order,12.5);
}

void AlgorithmRegressionTest::sequentialSplinePreservesPhase() {
  Scene s(512,512,6,12,2,"disk");
  auto tracer=digitqt::core::tracing::tryLoadTracerPlugin(digitqt::core::TracerAlgorithm::SequentialTracking);
  QVERIFY(tracer);
  auto visible=[&](int x,int y){return Scene::visible(x,y,&s);};
  const auto seeds=digitqt::core::findRowSeeds(s.image,visible);
  QVERIFY(tracer->initialize(s.image,visible));
  auto traced=tracer->extract(seeds);
  QVERIFY(traced.size()>=8);
  std::vector<digitqt::core::NumberedFringeLine> numbered;
  for(auto &line:traced) {digitqt::core::NumberedFringeLine n;n.points=std::move(line);numbered.push_back(std::move(n));}
  digitqt::core::autoAssignFringeOrder(numbered);
  std::vector<std::vector<DqtPhasePoint>> storage(numbered.size());
  std::vector<DqtPhaseNumberedFringeLine> lines;
  for(size_t i=0;i<numbered.size();++i) {
    for(const auto &p:numbered[i].points)storage[i].push_back({p.x,p.y});
    lines.push_back({storage[i].data(),storage[i].size(),numbered[i].order});
  }
  PhasePlugin spline;QVERIFY(spline.load("horizontal_spline"));
  std::vector<double> out;QVERIFY(spline.reconstruct(s,out,lines));
  double t=0,z=0,tt=0,tz=0,zz=0;int n=0;
  for(int y=0;y<512;++y)for(int x=0;x<512;++x) {
    if(std::hypot(x-255.5,y-255.5)>165)continue;
    const size_t i=size_t(y)*512+x;QVERIFY(std::isfinite(out[i]));
    t+=s.truth[i];z+=out[i];tt+=s.truth[i]*s.truth[i];tz+=s.truth[i]*out[i];zz+=out[i]*out[i];++n;
  }
  const double slope=(tz-t*z/n)/(tt-t*t/n);
  QVERIFY2(std::abs(std::abs(slope)-1)<.02,qPrintable(QString("Scale = %1").arg(slope)));
  const double sign=slope>=0?1:-1;
  const double mean=(sign*z-t)/n;
  const double rms=std::sqrt(std::max(0.0,(zz+tt-2*sign*tz)/n-mean*mean));
  QVERIFY2(rms<.05,qPrintable(QString("Shape RMS = %1 cycles").arg(rms)));
}

void AlgorithmRegressionTest::splineRimReconstruction_data() {
  QTest::addColumn<QString>("shape");
  for(const char *name:{"plane","two-lines","quadratic","quartic","nonpolynomial"})
    QTest::newRow(name)<<QString(name);
}

void AlgorithmRegressionTest::splineRimReconstruction() {
  QFETCH(QString,shape);
  Scene s(256,256,0,0,0,"disk");
  auto phase = [&](double x, double y) {
    const double u = (x - 127.5) / 256, v = (y - 127.5) / 256;
    double z = 8 * u + 4 * v;
    if (shape == "quadratic" || shape == "quartic")
      z += 2 * u * u - 3 * u * v + v * v;
    if (shape == "quartic")
      z += 5 * u * v * v + 4 * std::pow(u * u + v * v, 2);
    if (shape == "nonpolynomial")
      z += .4 * std::sin(18 * v) +
           .2 * std::exp(-200 * ((u - .08) * (u - .08) + (v + .1) * (v + .1)));
    return z;
  };
  std::vector<std::vector<DqtPhasePoint>> storage;
  std::vector<double> orders;
  for (int order = -7; order <= 7; ++order) {
    if (shape == "two-lines" && order != -1 && order != 1)
      continue;
    std::vector<DqtPhasePoint> line;
    for (int y = 0; y < 256; ++y) {
      double low = 0, high = 255;
      if (phase(low, y) > order || phase(high, y) < order)
        continue;
      for (int j = 0; j < 50; ++j) {
        const double mid = (low + high) / 2;
        if (phase(mid, y) < order)
          low = mid;
        else
          high = mid;
      }
      const double x = (low + high) / 2;
      // Deliberately stop before the pupil rim to exercise continuation.
      if (std::hypot(x - 127.5, y - 127.5) < 256 * (shape == "nonpolynomial" ? .40 : .35))
        line.push_back({x, double(y)});
    }
    if (line.size() > 2) {
      storage.push_back(std::move(line));
      orders.push_back(order);
    }
  }
  std::vector<DqtPhaseNumberedFringeLine> lines;
  for (size_t i = 0; i < storage.size(); ++i)
    lines.push_back({storage[i].data(), storage[i].size(), orders[i]});
  PhasePlugin plugin;
  QVERIFY(plugin.load("horizontal_spline"));
  std::vector<double> out;
  QVERIFY(plugin.reconstruct(s, out, lines));
  double sum2 = 0, maximum = 0;
  int count = 0;
  for (int y = 0; y < 256; ++y)
    for (int x = 0; x < 256; ++x) {
      const auto i = size_t(y) * 256 + x;
      if (!s.mask[i]) {
        QVERIFY(!std::isfinite(out[i]));
        continue;
      }
      if (shape == "nonpolynomial" && std::hypot(x - 127.5, y - 127.5) > .32 * 256)
        continue;
      QVERIFY(std::isfinite(out[i]));
      const double error = out[i] - phase(x, y);
      sum2 += error * error;
      maximum = std::max(maximum, std::abs(error));
      ++count;
    }
  QVERIFY(count > 10000);
  const double rms = std::sqrt(sum2 / count);
  QVERIFY2(rms < (shape == "nonpolynomial" ? .035 : .002),
           qPrintable(QString("%1 RMS = %2, max = %3 cycles").arg(shape).arg(rms).arg(maximum)));
  if (shape == "nonpolynomial") {
    // The localized bump must survive, not just the global surface shape.
    const int px = 148, py = 102;
    QVERIFY(std::abs(out[size_t(py) * 256 + px] - phase(px, py)) < .04);
  }
}

QTEST_GUILESS_MAIN(AlgorithmRegressionTest)
#include "AlgorithmRegressionTest.moc"
