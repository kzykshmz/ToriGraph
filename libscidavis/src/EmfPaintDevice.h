#ifndef EMF_PAINT_DEVICE_H
#define EMF_PAINT_DEVICE_H

#include <QByteArray>
#include <QImage>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QSize>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class MultiLayer;
class Graph;

struct OleRenderedGraph
{
    QByteArray emfBytes;
    QByteArray dibBytes;
    SIZEL extentHm{15000, 11250};
    QSize pixelSize{500, 375};
};

namespace SciDAVisEmf {

// Render a MultiLayer (or a single Graph layer if singleLayer != nullptr) to vector EMF (+ optional DIB for OLE clipboard)
OleRenderedGraph renderMultiLayer(MultiLayer *ml, Graph *singleLayer = nullptr,
                                  bool includeDibClipboard = true);

// Render from an arbitrary QImage fallback (e.g. 3D plot or empty state)
OleRenderedGraph renderFromImage(const QImage &img);

// Create a live HENHMETAFILE handle from serialized EMF bytes (caller owns handle via DeleteEnhMetaFile)
HENHMETAFILE createHemfFromBytes(const QByteArray &emfBytes);

// Serialize HENHMETAFILE to QByteArray
QByteArray hemfToBytes(HENHMETAFILE hemf);

// Convert QImage to CF_DIB byte array (BITMAPINFOHEADER + pixels)
QByteArray imageToDibBytes(const QImage &img);

// Ensure GDI+ is initialized for the current process
bool ensureGdiplusInitialized();

// Render an enhanced metafile (EMF / EMF+ Dual) onto a raster/display HDC with GDI+ anti-aliasing.
// Returns false if dc is a metafile DC or GDI+ rendering is unavailable (caller should fall back to PlayEnhMetaFile).
bool drawHemfWithGdiplus(HDC dc, HENHMETAFILE hemf, const RECT &rc);

} // namespace SciDAVisEmf

#endif // Q_OS_WIN
#endif // EMF_PAINT_DEVICE_H