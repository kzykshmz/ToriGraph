#ifndef SCIDAVIS_OLE_SERVER_H
#define SCIDAVIS_OLE_SERVER_H

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleidl.h>

class ApplicationWindow;
class MultiLayer;
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

// Ensure HKCU\Software\Classes\CLSID and ProgID point to current executable
bool ensureRegistryRegistered(QString *errMsg = nullptr);

// Initialize OLE and register local server class factory
bool initializeOleServer(bool embeddedMode);

// Flush OLE clipboard if owned and revoke class factory before exit
void shutdownOleServer();

// Copy a MultiLayer (or single Graph layer if singleLayer != nullptr) as an OLE embeddable object to clipboard
bool copyGraphAsOle(ApplicationWindow *app, MultiLayer *ml, Graph *singleLayer = nullptr);

// Copy a raster image (e.g. Graph3D) with project state as an OLE embeddable object to clipboard
bool copyImageAsOle(ApplicationWindow *app, const QImage &img, const QString &windowName);

// Create an OLE graph object instance directly (used by ClassFactory and internal tests)
HRESULT createGraphObject(const OleGraphPayload &payload, REFIID riid, void **ppvObj);

// Helper to save/load payload to/from an IStorage
HRESULT savePayloadToStorage(IStorage *stg, const OleGraphPayload &payload);
HRESULT loadPayloadFromStorage(IStorage *stg, OleGraphPayload *outPayload);

} // namespace SciDAVisOle

#endif // Q_OS_WIN
#endif // SCIDAVIS_OLE_SERVER_H