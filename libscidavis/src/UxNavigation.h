/***************************************************************************
    File                 : UxNavigation.h
    Description          : Beginner navigation helpers: bundled F1 guide, dialogs that fit
                           small screens (1366x768 at 150 %), shared translations
 ***************************************************************************/
#ifndef TORIGRAPH_UX_NAVIGATION_H
#define TORIGRAPH_UX_NAVIGATION_H

#include <QRect>
#include <QString>

class QWidget;
class Table;

namespace ToriUx {

//! Register the Japanese strings used by the navigation / menu / status features (idempotent).
void registerUxTranslations();

//! Absolute path of the bundled quick guide (manual/index.en.html for EN with fallback to
//! manual/index.html), or empty when not installed.
QString quickGuideFilePath(const QString &language = QString());

//! Dismiss any floating "Plot this data" bar attached to \a table.
void dismissPlotThisDataBar(Table *table);

//! Keep a top-level window inside the available screen area.
/**
 * The window's minimum size and current size are capped to the available area (minus an
 * allowance for the title bar and frame). When the layout itself cannot shrink that far, the
 * content is moved into a QScrollArea, keeping a trailing OK/Cancel row outside so that the
 * buttons stay visible. Returns true when the content was wrapped.
 * \param available screen area to fit into; the window's screen when invalid (used by tests).
 */
bool fitWindowToAvailableScreen(QWidget *window, const QRect &available = QRect());

//! Install the application-wide filter that passes every dialog through
//! fitWindowToAvailableScreen() when it is polished and shown.
void installDialogScreenFit();

} // namespace ToriUx

#endif
