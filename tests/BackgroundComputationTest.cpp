#include "gui/BackgroundComputation.h"

#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QWidget>
#include <QtTest/QtTest>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>

using digitqt::gui::BackgroundComputation;

class BackgroundComputationTest : public QObject {
  Q_OBJECT
private slots:
  void idleWindowClosesImmediately();
  void completionRunsOnGuiThread();
  void exceptionsAreReported_data();
  void exceptionsAreReported();
  void overlappingJobsAreRejected();
  void closeWaitsAndSkipsCompletion_data();
  void closeWaitsAndSkipsCompletion();
  void destructionWaitsForWorker();
};

void BackgroundComputationTest::idleWindowClosesImmediately() {
  QWidget window;
  BackgroundComputation job(&window);
  window.show();
  QVERIFY(window.close());
  QVERIFY(!window.isVisible());
  QVERIFY(!job.isRunning());
}

void BackgroundComputationTest::completionRunsOnGuiThread() {
  QWidget window;
  BackgroundComputation job(&window);
  QThread *workerThread = nullptr;
  bool completed = false;
  bool success = false;
  QString error;
  QVERIFY(job.start(
      [&] {
        workerThread = QThread::currentThread();
        return true;
      },
      [&](bool ok, const QString &message) {
        success = ok;
        error = message;
        QCOMPARE(QThread::currentThread(), qApp->thread());
        QVERIFY(!job.isRunning());
        QVERIFY(job.start([] { return false; },
                          [&](bool nextOk, const QString &nextError) {
                            QVERIFY(!nextOk);
                            QVERIFY(nextError.isEmpty());
                            completed = true;
                          }));
      }));
  QTRY_VERIFY(completed);
  QVERIFY(success);
  QVERIFY(error.isEmpty());
  QVERIFY(workerThread != qApp->thread());
}

void BackgroundComputationTest::exceptionsAreReported_data() {
  QTest::addColumn<int>("kind");
  QTest::newRow("standard") << 0;
  QTest::newRow("empty-message") << 1;
  QTest::newRow("unknown") << 2;
}

void BackgroundComputationTest::exceptionsAreReported() {
  QFETCH(int, kind);
  QWidget window;
  BackgroundComputation job(&window);
  bool completed = false;
  QString error;
  QVERIFY(job.start(
      [kind]() -> bool {
        if (kind == 0)
          throw std::runtime_error("solver failed");
        if (kind == 1)
          throw std::runtime_error("");
        throw 42;
      },
      [&](bool ok, const QString &message) {
        QVERIFY(!ok);
        error = message;
        completed = true;
      }));
  QTRY_VERIFY(completed);
  QVERIFY(!error.isEmpty());
  if (kind == 0)
    QCOMPARE(error, QStringLiteral("solver failed"));
  QVERIFY(!job.isRunning());
}

void BackgroundComputationTest::overlappingJobsAreRejected() {
  QWidget window;
  BackgroundComputation job(&window);
  QSemaphore entered, release;
  bool completed = false;
  QVERIFY(job.start(
      [&] {
        entered.release();
        return release.tryAcquire(1, 5000);
      },
      [&](bool, const QString &) { completed = true; }));
  const bool started = entered.tryAcquire(1, 5000);
  bool unexpectedWork = false;
  const bool secondAccepted = job.start(
      [&] {
        unexpectedWork = true;
        return true;
      },
      [](bool, const QString &) {});
  release.release();
  QTRY_VERIFY(completed);
  QVERIFY(started);
  QVERIFY(!secondAccepted);
  QVERIFY(!unexpectedWork);
}

void BackgroundComputationTest::closeWaitsAndSkipsCompletion_data() {
  QTest::addColumn<bool>("throws");
  QTest::newRow("success") << false;
  QTest::newRow("exception") << true;
}

void BackgroundComputationTest::closeWaitsAndSkipsCompletion() {
  QFETCH(bool, throws);
  QWidget window;
  BackgroundComputation job(&window);
  QSignalSpy deferred(&job, &BackgroundComputation::closeDeferred);
  QSemaphore entered, release;
  bool completionCalled = false;
  window.show();
  QVERIFY(job.start(
      [&] {
        entered.release();
        const bool released = release.tryAcquire(1, 5000);
        if (throws)
          throw std::runtime_error("closing");
        return released;
      },
      [&](bool, const QString &) { completionCalled = true; }));
  const bool started = entered.tryAcquire(1, 5000);
  const bool closedEarly = window.close();
  const bool stillVisible = window.isVisible();
  window.close();  // repeated close requests do not launch more work or callbacks
  release.release();
  QTRY_VERIFY(!job.isRunning());
  QTRY_VERIFY(!window.isVisible());
  QVERIFY(started);
  QVERIFY(!closedEarly);
  QVERIFY(stillVisible);
  QCOMPARE(deferred.count(), 1);
  QVERIFY(!completionCalled);
  QVERIFY(!job.start([] { return true; }, [](bool, const QString &) {}));
}

void BackgroundComputationTest::destructionWaitsForWorker() {
  QWidget window;
  QSemaphore entered, release;
  std::atomic<bool> finished{false};
  bool completionCalled = false;
  {
    auto job = std::make_unique<BackgroundComputation>(&window);
    QVERIFY(job->start(
        [&] {
          entered.release();
          const bool released = release.tryAcquire(1, 5000);
          finished = true;
          return released;
        },
        [&](bool, const QString &) { completionCalled = true; }));
    const bool started = entered.tryAcquire(1, 5000);
    // The destructor must join the worker even if the GUI event loop stops.
    std::thread unblock([&] { release.release(); });
    job.reset();
    unblock.join();
    QVERIFY(started);
    QVERIFY(finished.load());
  }
  QCoreApplication::processEvents();
  QVERIFY(!completionCalled);
}

QTEST_MAIN(BackgroundComputationTest)
#include "BackgroundComputationTest.moc"
