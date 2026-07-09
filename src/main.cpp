#include "gui/MainWindow.h"

#include <QApplication>
#include <QLocale>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QTranslator>

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

// Loads the UI translation, if any is needed/available. English is the
// language the source code itself is written in (tr("...") strings), so
// it needs no .qm file at all -- only non-English languages do.
//
// Language choice priority:
//   1. Explicit choice persisted via QSettings (see MainWindow's Language
//      menu).
//   2. The system locale, if we ship a translation for it.
//   3. English (i.e. do nothing -- the source strings are already English).
void loadTranslation(QApplication &app, QTranslator &translator) {
  QSettings settings;
  QString language = settings.value(QStringLiteral("language")).toString();

  if (language.isEmpty())
    language = QLocale::system().name().startsWith(QStringLiteral("ru"))
                   ? QStringLiteral("ru")
                   : QStringLiteral("en");

  if (language == QStringLiteral("en"))
    return;  // nothing to load -- source strings are already English

  const QString qmPath = app.applicationDirPath() +
                         QStringLiteral("/translations/DigitQt_") + language +
                         QStringLiteral(".qm");
  if (translator.load(qmPath))
    app.installTranslator(&translator);
  // If the .qm isn't found (e.g. a dev build before `translations` was
  // built), we silently fall back to the English strings already in the
  // source -- never a hard failure.
}

}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QApplication::setOrganizationName("DigitQt");
  QApplication::setApplicationName("DigitQt");

  QTranslator translator;
  loadTranslation(app, translator);

  applyApplicationStyle(app);

  digitqt::gui::MainWindow window;
  window.show();

  return QApplication::exec();
}
