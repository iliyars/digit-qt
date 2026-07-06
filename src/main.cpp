#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include "gui/MainWindow.h"

namespace {

// A clean, cross-platform look: Qt's native styles vary a lot between
// Windows/Linux/macOS and none of them look particularly "professional" by
// default. Fusion + a deliberate palette gives a consistent, neutral look
// everywhere the app runs.
void applyApplicationStyle(QApplication &app) {
  app.setStyle(QStyleFactory::create("Fusion"));

  QPalette palette;
  palette.setColor(QPalette::Window, QColor(240, 240, 243));
  palette.setColor(QPalette::WindowText, Qt::black);
  palette.setColor(QPalette::Base, Qt::white);
  palette.setColor(QPalette::AlternateBase, QColor(235, 235, 238));
  palette.setColor(QPalette::ToolTipBase, Qt::white);
  palette.setColor(QPalette::ToolTipText, Qt::black);
  palette.setColor(QPalette::Text, Qt::black);
  palette.setColor(QPalette::Button, QColor(240, 240, 243));
  palette.setColor(QPalette::ButtonText, Qt::black);
  palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor(150, 150, 150));
  palette.setColor(QPalette::Disabled, QPalette::WindowText,
                   QColor(150, 150, 150));
  app.setPalette(palette);
}

} // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QApplication::setOrganizationName("DigitQt");
  QApplication::setApplicationName("DigitQt");

  applyApplicationStyle(app);

  digitqt::gui::MainWindow window;
  window.show();

  return QApplication::exec();
}
