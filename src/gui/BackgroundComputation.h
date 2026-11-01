#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

class QWidget;

namespace digitqt::gui {

/// Serializes work on window-owned data. Closing is deferred until the worker
/// finishes; completion callbacks are discarded when closing or being destroyed.
class BackgroundComputation : public QObject {
  Q_OBJECT
public:
  explicit BackgroundComputation(QWidget *window);
  ~BackgroundComputation() override;

  bool start(std::function<bool()> compute, std::function<void(bool, const QString &)> onDone);
  bool isRunning() const { return m_running; }
  bool isClosing() const { return m_closeRequested; }
  // Final shutdown: the window must call this before destroying any data captured
  // by the worker. The runner cannot be restarted after this call.
  void waitForFinished();

signals:
  void closeDeferred();

protected:
  bool eventFilter(QObject *object, QEvent *event) override;

private:
  struct Result {
    bool success = false;
    QString error;
  };
  QPointer<QWidget> m_window;
  QFutureWatcher<Result> m_watcher;
  std::function<void(bool, const QString &)> m_onDone;
  bool m_running = false;
  bool m_closeRequested = false;
};

}  // namespace digitqt::gui
