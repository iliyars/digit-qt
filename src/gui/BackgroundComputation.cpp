#include "BackgroundComputation.h"

#include <QCloseEvent>
#include <QWidget>
#include <QtConcurrent>
#include <exception>
#include <utility>

namespace digitqt::gui {

BackgroundComputation::BackgroundComputation(QWidget *window) : QObject(window), m_window(window) {
  window->installEventFilter(this);
  connect(&m_watcher, &QFutureWatcher<Result>::finished, this, [this] {
    const Result result = m_watcher.result();
    m_running = false;
    auto onDone = std::move(m_onDone);
    if (m_closeRequested) {
      if (m_window)
        m_window->close();
      return;
    }
    if (onDone)
      onDone(result.success, result.error);
  });
}

BackgroundComputation::~BackgroundComputation() {
  waitForFinished();
}

void BackgroundComputation::waitForFinished() {
  m_closeRequested = true;
  disconnect(&m_watcher, nullptr, this, nullptr);
  m_onDone = {};
  // QtConcurrent::run() cannot interrupt these algorithms. Wait without running
  // GUI callbacks, keeping the owner's model and controllers alive until return.
  m_watcher.waitForFinished();
  m_running = false;
}

bool BackgroundComputation::start(std::function<bool()> compute,
                                  std::function<void(bool, const QString &)> onDone) {
  if (m_running || m_closeRequested)
    return false;
  m_onDone = std::move(onDone);
  m_running = true;
  m_watcher.setFuture(QtConcurrent::run([compute = std::move(compute)] {
    try {
      return Result{compute(), {}};
    } catch (const std::exception &error) {
      const QString message = QString::fromUtf8(error.what());
      return Result{false, message.isEmpty() ? tr("Background calculation failed.") : message};
    } catch (...) {
      return Result{false, tr("Unknown error in background calculation.")};
    }
  }));
  return true;
}

bool BackgroundComputation::eventFilter(QObject *object, QEvent *event) {
  if (object == m_window && event->type() == QEvent::Close && m_running) {
    static_cast<QCloseEvent *>(event)->ignore();
    if (!m_closeRequested) {
      m_closeRequested = true;
      emit closeDeferred();
    }
    return true;
  }
  return QObject::eventFilter(object, event);
}

}  // namespace digitqt::gui
