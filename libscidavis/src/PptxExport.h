/***************************************************************************
    File                 : PptxExport.h
    Project              : ToriGraph / SciDAVis
    Description          : "Send to PowerPoint" (E8): writes graphs into a .pptx as
                           editable ToriGraph.Graph.1 OLE objects with an EMF
                           preview, like the Python SDK's export_graphs_to_pptx,
                           without PowerPoint, COM automation or Qt private API.
 ***************************************************************************/

#ifndef TORIGRAPH_PPTXEXPORT_H
#define TORIGRAPH_PPTXEXPORT_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

class ApplicationWindow;
class MyWidget;
class QDialog;
class QWidget;

namespace ToriGraphPptx {

// EMU (English Metric Units) used by DrawingML.
constexpr qint64 kEmuPerCm = 360000;
constexpr qint64 kEmuPerHimetric = 360; // 1/100 mm
constexpr qint64 kSlideWidth16x9 = 12192000; // 33.867 cm
constexpr qint64 kSlideWidth4x3 = 9144000; // 25.4 cm
constexpr qint64 kSlideHeight = 6858000; // 19.05 cm

//! Registers the Japanese strings of this feature (idempotent).
void registerTranslations();
QString sendToPowerPointMenuText(); //!< "Send to &PowerPoint..."
QString sendToPowerPointLabel(); //!< short ribbon label
QString sendToPowerPointToolTip();

//! Built-in preset whose size and paste factor the export follows ("lab_slide_83mm").
QString labSlidePresetId();
//! Paste factor of a style preset in percent (100 when the preset does not define one).
double presetPastePercent(const QString &presetId);

//! One graph ready to be placed on a slide.
struct SlideItem
{
    QString graphName;
    QByteArray oleBytes; //!< OLE2 compound file (ToriGraph.Graph.1); empty = picture only
    QByteArray previewBytes; //!< EMF (or PNG) shown by PowerPoint
    QString previewExtension = QStringLiteral("emf"); //!< "emf" or "png"
    qint64 widthHm = 0; //!< natural (page) size, 1/100 mm
    qint64 heightHm = 0;
    double scalePercent = -1.0; //!< < 0 = inherit DeckOptions::scalePercent; 0 = fit cell
};

struct DeckOptions
{
    int perSlide = 1; //!< 1, 2 (1x2), 3 (1x3), 4 (2x2) or 6 (2x3)
    bool widescreen = true; //!< 16:9 (33.9 x 19.1 cm) or 4:3 (25.4 x 19.1 cm); new decks only
    //! Size on the slide in percent of the graph's page size. <= 0 = fit the grid cell.
    double scalePercent = 140.0;
    //! Shrink a graph that does not fit its cell at scalePercent (reported per placement).
    bool shrinkToFit = true;
    //! Add slides at the end of an existing .pptx instead of replacing it.
    bool appendToExisting = false;
    //! When true, resolve preset paste scale per MultiLayer instead of sharing one global scalePercent.
    bool perGraphPresetScale = false;
};

struct Placement
{
    QString graphName;
    int slide = 0; //!< 1-based slide number in the written deck
    qint64 left = 0, top = 0, width = 0, height = 0; //!< EMU
    double effectivePercent = 0.0; //!< size on the slide / page size x 100
    double requestedPercent = -1.0; //!< requested paste percent before shrink-to-fit
    bool shrunk = false; //!< made smaller than scalePercent to fit the cell
};

struct DeckResult
{
    bool ok = false;
    QString path;
    QString error; //!< translated
    int slideCount = 0; //!< slides in the written deck
    int newSlides = 0;
    bool appended = false;
    qint64 slideWidth = 0, slideHeight = 0; //!< EMU
    QVector<Placement> placements;
    QStringList warnings; //!< translated
};

//! Grid cell of the per-slide layout, the same margins as the Python SDK.
void gridCell(qint64 slideW, qint64 slideH, int perSlide, int cellIndex, qint64 *left,
              qint64 *top, qint64 *width, qint64 *height);

//! Builds the .pptx (new, or appended to `path` when opt.appendToExisting and the file
//! exists) and saves it atomically.
DeckResult writeDeck(const QString &path, const QVector<SlideItem> &items, const DeckOptions &opt);

//! Renders one graph window (MultiLayer or Graph3D) into an OLE object + EMF preview.
bool buildSlideItem(ApplicationWindow *app, MyWidget *window, SlideItem *out, QString *outErr);

//! Renders and writes `windows` (graph windows) to `path`.
DeckResult exportGraphs(ApplicationWindow *app, const QList<MyWidget *> &windows,
                        const DeckOptions &opt, const QString &path);

//! Graph windows (MultiLayer and Graph3D) of the project, in project order.
QList<MyWidget *> graphWindows(ApplicationWindow *app);

//! The options dialog (not shown). Its widgets have object names starting with "pptx".
QDialog *createSendDialog(ApplicationWindow *app, QWidget *parent = nullptr);
//! Reads the dialog. Returns false with a translated reason when the choice is incomplete.
bool readSendDialog(QDialog *dialog, ApplicationWindow *app, QList<MyWidget *> *windows,
                    DeckOptions *options, QString *path, bool *applyLabPreset, QString *outErr);

//! The menu command: dialog, export, and a completion message with an "Open" button.
void sendGraphsToPowerPoint(ApplicationWindow *app);

} // namespace ToriGraphPptx

#endif // TORIGRAPH_PPTXEXPORT_H
