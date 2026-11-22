#ifndef SCIDAVIS_OLE_SERVER_H
#define SCIDAVIS_OLE_SERVER_H

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

// User-visible names of the "copy for PowerPoint" command. These are shared by the menu, the
// ribbon, the focus HUD and the graph inspector, and are available on every platform.
namespace ToriGraphOleUi {
// Registers the Japanese strings used by the PowerPoint/OLE commands (idempotent).
void registerJapaneseStrings();
QString copyForPowerPointLabel();       // short button label
QString copyForPowerPointMenuText();    // menu entry (with mnemonic)
QString copyForPowerPointToolTip();     // tooltip explaining re-editing
QString embedWholeProjectMenuText();    // checkable "embed whole project" option
QString repairRegistrationMenuText();   // Help menu entry
QString selectGraphFirstMessage();      // shown when no graph is active
} // namespace ToriGraphOleUi

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleidl.h>

class ApplicationWindow;
class MultiLayer;
class MyWidget;
class Graph;

namespace SciDAVisOle {

// {8E6B9541-3C74-4D28-A915-7D84E5A12001}
inline constexpr CLSID graph_clsid = {
    0x8e6b9541, 0x3c74, 0x4d28, { 0xa9, 0x15, 0x7d, 0x84, 0xe5, 0xa1, 0x20, 0x01 }
};

struct OleGraphPayload
{
    QByteArray projectBytes;
    QString targetWindow;
    int targetLayer = 0; // 0 = all layers in MultiLayer, >=1 = 1-based layer index
    SIZEL extentHm{ 15000, 11250 };
    QSize pixelSize{ 500, 375 };
    QByteArray emfBytes;
    QByteArray dibBytes;
};

// What goes into the project stream of an embedded object.
enum class EmbedScope {
    GraphOnly,   // the copied graph and the tables/matrices it plots (default)
    WholeProject // every window of the project, plus the results log
};

EmbedScope embedScopeSetting();
void setEmbedScopeSetting(EmbedScope scope);

// Names of the tables and matrices whose data are drawn by window w (a MultiLayer or Graph3D).
QStringList dataDependenciesForWindow(ApplicationWindow *app, MyWidget *w);

// The project text to embed for target. For EmbedScope::GraphOnly this is a stand-alone
// project holding the dependencies of target followed by target itself (no results log).
// embeddedWindows receives the names of the windows written.
QByteArray buildEmbeddedProjectBytes(ApplicationWindow *app, MyWidget *target, EmbedScope scope,
                                     QStringList *embeddedWindows = nullptr);

// State of the HKCU/HKCR registration of the ToriGraph OLE server.
struct RegistrationInfo
{
    enum State {
        NotRegistered,     // no LocalServer32 entry for the CLSID
        Valid,             // the registered server executable exists
        MissingExecutable  // registered, but the executable is gone (e.g. a deleted build)
    };
    State state = NotRegistered;
    QString executable;     // registered server executable (may be empty)
    bool pointsToSelf = false;
};
RegistrationInfo queryRegistration();

// Writes the HKCU registration so that it points to this executable (explicit repair).
bool ensureRegistryRegistered(QString *errMsg = nullptr);

// Start-up policy: leave a valid registration alone, register only when it is missing or
// broken, and do nothing at all when TORIGRAPH_NO_OLE_REGISTER is set to a non-zero value
// (for development builds and test executables). Returns false only if writing failed.
bool ensureRegistrationAtStartup(QString *errMsg = nullptr);

// Initialize OLE and register local server class factory
bool initializeOleServer(bool embeddedMode);

// Flush OLE clipboard if owned and revoke class factory before exit
void shutdownOleServer();

// Outcome of a "copy for PowerPoint" request.
struct OleCopyResult
{
    bool ok = false;                       // editable OLE object is on the clipboard
    bool staticFallbackOnClipboard = false; // OLE failed; EMF + PNG were placed instead
    QString windowName;
    double widthMm = 0.0;
    double heightMm = 0.0;
    int pngDpi = 0;                        // resolution of the PNG offered with the object
    EmbedScope scope = EmbedScope::GraphOnly;
    QStringList embeddedWindows;
    QString errorReason;                   // translated, empty on success
    bool registrationProblem = false;      // re-editing from PowerPoint will not work
};

OleCopyResult copyGraphForPowerPoint(ApplicationWindow *app, MultiLayer *ml,
                                     Graph *singleLayer = nullptr);
OleCopyResult copyImageForPowerPoint(ApplicationWindow *app, const QImage &img,
                                     const QString &windowName);

// Shows the outcome in the status bar; on failure (or a registration problem) also shows a
// message box with the reason and a repair button when showDialog is true and the context is
// interactive.
void reportCopyResult(ApplicationWindow *app, const OleCopyResult &result, bool showDialog);

// Re-registers the OLE server for this executable and tells the user what happened.
bool repairRegistrationInteractive(ApplicationWindow *app);

// Copy a MultiLayer (or single Graph layer if singleLayer != nullptr) as an OLE embeddable object to clipboard
bool copyGraphAsOle(ApplicationWindow *app, MultiLayer *ml, Graph *singleLayer = nullptr);

// Copy a raster image (e.g. Graph3D) with project state as an OLE embeddable object to clipboard
bool copyImageAsOle(ApplicationWindow *app, const QImage &img, const QString &windowName);

// Create an OLE graph object instance directly (used by ClassFactory and internal tests)
HRESULT createGraphObject(const OleGraphPayload &payload, REFIID riid, void **ppvObj);

// Helper to save/load payload to/from an IStorage
HRESULT savePayloadToStorage(IStorage *stg, const OleGraphPayload &payload);
HRESULT loadPayloadFromStorage(IStorage *stg, OleGraphPayload *outPayload);

// Clipboard format id of "PNG" (registered clipboard format used by Office).
CLIPFORMAT pngClipboardFormat();

// Renders an EMF at 300-600 dpi of its physical size and returns PNG bytes (dpi stored in the
// file). outDpi receives the resolution used.
QByteArray renderEmfToPng(const QByteArray &emfBytes, SIZEL extentHm, int *outDpi = nullptr);

} // namespace SciDAVisOle

#endif // Q_OS_WIN
#endif // SCIDAVIS_OLE_SERVER_H
