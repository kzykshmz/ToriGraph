/***************************************************************************
    File                 : GraphDigitizer.cpp
    Project              : ToriGraph (SciDAVis OriginPro Parity)
    Description          : Interactive Graph Image Digitizer (4-point linear/log10
                           axis calibration, manual picking, RGB color auto-tracing,
                           and worksheet/graph export)
 ***************************************************************************/
#include "GraphDigitizer.h"

#include "ApplicationWindow.h"
#include "Graph.h"
#include "I18nJapanese.h"
#include "Legend.h"
#include "MultiLayer.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot.h>
#include <qwt_symbol.h>

#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {

void applyArialToDigitizedGraph(Graph *g)
{
    if (!g)
        return;
    g->initFonts(QFont(QStringLiteral("Arial"), 11, QFont::Bold),
                 QFont(QStringLiteral("Arial"), 10));
    g->setTitleFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));
    if (g->legend())
        g->legend()->setFont(QFont(QStringLiteral("Arial"), 9));
    g->setTextMarkerDefaults(g->textMarkerDefaultFrame(),
                             QFont(QStringLiteral("Arial"), 10),
                             g->textMarkerDefaultColor(),
                             g->textMarkerDefaultBackground());
}

enum class DigitizerToolMode {
    SetX1 = 0,
    SetX2 = 1,
    SetY1 = 2,
    SetY2 = 3,
    PickPoints = 4,
    PickColorAutoTrace = 5,
    SetTraceRoi = 6
};

void registerDigitizerTranslationsOnce()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("4x Magnifier Loupe"), QStringLiteral("4倍拡大ルーペ"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Set Trace ROI"), QStringLiteral("トレースROI範囲設定"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Clear ROI"), QStringLiteral("ROI解除"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Drag or click two corners on the image to define the Auto-Trace ROI box."),
            QStringLiteral("画像上でドラッグまたは対角2点をクリックして自動トレースROI範囲を指定してください。"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Sub-pixel: (%1, %2)\nRGB: (%3, %4, %5)"),
            QStringLiteral("サブピクセル: (%1, %2)\nRGB: (%3, %4, %5)"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Hover over image\nfor 4x zoom"),
            QStringLiteral("画像上にカーソルを置くと\n4倍拡大表示します"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("ROI: Full Calibrated Area"),
            QStringLiteral("ROI: キャリブレーション全域"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("ROI Active: (%1, %2) %3x%4 px"),
            QStringLiteral("ROI有効: (%1, %2) %3x%4 px"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Graph Image Digitizer"),
            QStringLiteral("グラフ画像デジタイザ"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Reset View"),
            QStringLiteral("表示リセット"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Sort by X"),
            QStringLiteral("X で並べ替える"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Append to existing points"),
            QStringLiteral("既存の点に追加"));
    ToriGraphFullJapaneseTranslator::registerExtraTranslation(
            QStringLiteral("Click the second opposite corner on the image to finish the ROI box."),
            QStringLiteral("対角となる2点目をクリックしてROI範囲を確定してください。"));
}

class DigitizerMagnifierLoupe : public QWidget
{
public:
    explicit DigitizerMagnifierLoupe(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(124);
        setMinimumWidth(260);
    }

    void setImage(const QImage &img)
    {
        d_image = img;
        update();
    }

    void setFocusPixel(const QPointF &pix)
    {
        d_focusPix = pix;
        d_hasFocus = true;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(246, 248, 252));

        constexpr int kBox = 112;
        constexpr double kZoom = 4.0;
        const QRectF loupeRect(6.0, 0.5 * (height() - kBox), kBox, kBox);

        p.fillRect(loupeRect, QColor(30, 34, 42));
        if (!d_image.isNull() && d_hasFocus && d_image.width() > 0 && d_image.height() > 0) {
            p.save();
            p.setClipRect(loupeRect);
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);

            const double halfSpan = (0.5 * kBox) / kZoom;
            const int ix0 = std::max(0, static_cast<int>(std::floor(d_focusPix.x() - halfSpan)) - 1);
            const int iy0 = std::max(0, static_cast<int>(std::floor(d_focusPix.y() - halfSpan)) - 1);
            const int ix1 = std::min(d_image.width() - 1,
                                     static_cast<int>(std::ceil(d_focusPix.x() + halfSpan)) + 1);
            const int iy1 = std::min(d_image.height() - 1,
                                     static_cast<int>(std::ceil(d_focusPix.y() + halfSpan)) + 1);

            if (ix1 >= ix0 && iy1 >= iy0) {
                const QRect srcPatch(ix0, iy0, ix1 - ix0 + 1, iy1 - iy0 + 1);
                const double dstLeft = loupeRect.center().x()
                        + (static_cast<double>(ix0) - d_focusPix.x() - 0.5) * kZoom;
                const double dstTop = loupeRect.center().y()
                        + (static_cast<double>(iy0) - d_focusPix.y() - 0.5) * kZoom;
                const QRectF dstPatch(dstLeft, dstTop, srcPatch.width() * kZoom,
                                      srcPatch.height() * kZoom);
                p.drawImage(dstPatch, d_image, QRectF(srcPatch));
            }

            // Crisp sub-pixel crosshair at exact center
            const QPointF c = loupeRect.center();
            p.setPen(QPen(QColor(255, 255, 255, 210), 2.5));
            p.drawLine(QPointF(loupeRect.left(), c.y()), QPointF(loupeRect.right(), c.y()));
            p.drawLine(QPointF(c.x(), loupeRect.top()), QPointF(c.x(), loupeRect.bottom()));
            p.setPen(QPen(QColor(225, 35, 45), 1.2));
            p.drawLine(QPointF(loupeRect.left(), c.y()), QPointF(loupeRect.right(), c.y()));
            p.drawLine(QPointF(c.x(), loupeRect.top()), QPointF(c.x(), loupeRect.bottom()));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(c.x() - 2.0, c.y() - 2.0, 4.0, 4.0));
            p.restore();

            p.setPen(QPen(QColor(95, 108, 128), 1.2));
            p.drawRect(loupeRect);

            const int sx = std::clamp(static_cast<int>(std::round(d_focusPix.x())), 0,
                                      d_image.width() - 1);
            const int sy = std::clamp(static_cast<int>(std::round(d_focusPix.y())), 0,
                                      d_image.height() - 1);
            const QColor pc = d_image.pixelColor(sx, sy);

            const double infoLeft = loupeRect.right() + 10.0;
            const QRectF swatchRect(infoLeft, loupeRect.top() + 4.0, 28.0, 18.0);
            p.fillRect(swatchRect, pc);
            p.setPen(QPen(QColor(80, 88, 100), 1));
            p.drawRect(swatchRect);

            p.setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));
            p.setPen(QColor(25, 55, 105));
            p.drawText(QPointF(swatchRect.right() + 6.0, swatchRect.bottom() - 4.0),
                       QStringLiteral("4x"));

            p.setFont(QFont(QStringLiteral("Arial"), 8));
            p.setPen(QColor(35, 40, 50));
            const QRectF txtRect(infoLeft, swatchRect.bottom() + 6.0,
                                 std::max(40.0, width() - infoLeft - 4.0),
                                 loupeRect.height() - 28.0);
            p.drawText(txtRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                       tr("Sub-pixel: (%1, %2)\nRGB: (%3, %4, %5)")
                               .arg(d_focusPix.x(), 0, 'f', 2)
                               .arg(d_focusPix.y(), 0, 'f', 2)
                               .arg(pc.red())
                               .arg(pc.green())
                               .arg(pc.blue()));
        } else {
            p.setPen(QPen(QColor(150, 160, 175), 1));
            p.drawRect(loupeRect);
            p.setFont(QFont(QStringLiteral("Arial"), 8));
            p.setPen(QColor(110, 120, 135));
            const QRectF txtRect(loupeRect.right() + 10.0, loupeRect.top(),
                                 std::max(40.0, width() - loupeRect.right() - 14.0),
                                 loupeRect.height());
            p.drawText(txtRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
                       tr("Hover over image\nfor 4x zoom"));
        }
    }

private:
    QImage d_image;
    QPointF d_focusPix;
    bool d_hasFocus = false;
};

class DigitizerImageCanvas : public QWidget
{
public:
    explicit DigitizerImageCanvas(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(480, 360);
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
    }

    void setImage(const QImage &img)
    {
        d_image = img;
        d_zoom = 1.0;
        d_pan = QPointF();
        update();
        if (onZoomChanged)
            onZoomChanged(d_zoom);
    }

    const QImage &image() const { return d_image; }

    //! C4-9: back to "whole image fits the canvas" (zoom 100 %, no pan).
    void resetView()
    {
        d_zoom = 1.0;
        d_pan = QPointF();
        d_panning = false;
        update();
        if (onZoomChanged)
            onZoomChanged(d_zoom);
    }

    double zoom() const { return d_zoom; }

    void setCalibration(const GraphDigitizer::AxisCalibration &cal,
                        bool hasX1, bool hasX2, bool hasY1, bool hasY2)
    {
        d_cal = cal;
        d_hasX1 = hasX1;
        d_hasX2 = hasX2;
        d_hasY1 = hasY1;
        d_hasY2 = hasY2;
        update();
    }

    void setPickedPixelPoints(const QVector<QPointF> &pts)
    {
        d_pixelPts = pts;
        update();
    }

    void setTraceRoi(const QRect &roi)
    {
        d_traceRoi = roi;
        update();
    }

    QRect traceRoi() const { return d_traceRoi; }

    std::function<void(const QPointF &, Qt::MouseButton)> onImageClicked;
    std::function<void(const QPointF &)> onImageHovered;
    std::function<void(const QRect &)> onRoiDragged;
    std::function<void(double)> onZoomChanged;
    bool roiDragMode = false;

protected:
    //! Rectangle in which the whole image fits the widget (zoom 100 %).
    QRectF computeFitImageRect() const
    {
        if (d_image.isNull() || d_image.width() <= 0 || d_image.height() <= 0)
            return QRectF();
        const double pad = 10.0;
        const double availW = std::max(1.0, static_cast<double>(width()) - 2.0 * pad);
        const double availH = std::max(1.0, static_cast<double>(height()) - 2.0 * pad);
        const double scale = std::min(availW / static_cast<double>(d_image.width()),
                                      availH / static_cast<double>(d_image.height()));
        const double drawW = static_cast<double>(d_image.width()) * scale;
        const double drawH = static_cast<double>(d_image.height()) * scale;
        const double left = 0.5 * (static_cast<double>(width()) - drawW);
        const double top = 0.5 * (static_cast<double>(height()) - drawH);
        return QRectF(left, top, drawW, drawH);
    }

    //! Rectangle the image is drawn into, including wheel zoom and pan (C4-9).
    QRectF computeTargetImageRect() const
    {
        const QRectF base = computeFitImageRect();
        if (base.isEmpty() || d_zoom <= 1.0 + 1e-9)
            return base;
        const double w = base.width() * d_zoom;
        const double h = base.height() * d_zoom;
        const double left = 0.5 * (static_cast<double>(width()) - w) + d_pan.x();
        const double top = 0.5 * (static_cast<double>(height()) - h) + d_pan.y();
        return QRectF(left, top, w, h);
    }

    //! Keep the zoomed image covering the canvas (never pan it out of sight).
    void clampPan()
    {
        const QRectF base = computeFitImageRect();
        if (base.isEmpty() || d_zoom <= 1.0 + 1e-9) {
            d_pan = QPointF();
            return;
        }
        const double pad = 10.0;
        const double w = base.width() * d_zoom;
        const double h = base.height() * d_zoom;
        const double maxX = (w > width()) ? 0.5 * (w - width()) + pad : 0.0;
        const double maxY = (h > height()) ? 0.5 * (h - height()) + pad : 0.0;
        d_pan.setX(std::clamp(d_pan.x(), -maxX, maxX));
        d_pan.setY(std::clamp(d_pan.y(), -maxY, maxY));
    }

    QPointF widgetToImagePixel(const QPointF &wPt, bool *insideOut = nullptr) const
    {
        const QRectF r = computeTargetImageRect();
        if (r.isEmpty() || d_image.isNull()) {
            if (insideOut)
                *insideOut = false;
            return QPointF(0.0, 0.0);
        }
        const bool inside = r.contains(wPt);
        if (insideOut)
            *insideOut = inside;
        const double ux = std::clamp((wPt.x() - r.left()) / r.width(), 0.0, 1.0);
        const double uy = std::clamp((wPt.y() - r.top()) / r.height(), 0.0, 1.0);
        return QPointF(ux * static_cast<double>(d_image.width() - 1),
                       uy * static_cast<double>(d_image.height() - 1));
    }

    QPointF imagePixelToWidget(const QPointF &imgPt) const
    {
        const QRectF r = computeTargetImageRect();
        if (r.isEmpty() || d_image.width() <= 1 || d_image.height() <= 1)
            return QPointF(0.0, 0.0);
        const double ux = imgPt.x() / static_cast<double>(d_image.width() - 1);
        const double uy = imgPt.y() / static_cast<double>(d_image.height() - 1);
        return QPointF(r.left() + ux * r.width(), r.top() + uy * r.height());
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.fillRect(rect(), QColor(242, 244, 248));

        if (d_image.isNull()) {
            p.setPen(QColor(100, 110, 125));
            p.setFont(QFont(QStringLiteral("Arial"), 11));
            p.drawText(rect(), Qt::AlignCenter,
                       tr("Open an image or paste a graph screenshot from the clipboard."));
            return;
        }

        const QRectF imgRect = computeTargetImageRect();
        // Draw only the visible part of the image (the zoomed rect can be much larger
        // than the canvas); keep pixels crisp at high zoom.
        const QRectF visRect = imgRect.intersected(QRectF(rect()));
        if (!visRect.isEmpty() && imgRect.width() > 0.0 && imgRect.height() > 0.0) {
            const double sx = static_cast<double>(d_image.width()) / imgRect.width();
            const double sy = static_cast<double>(d_image.height()) / imgRect.height();
            const QRectF srcRect((visRect.left() - imgRect.left()) * sx,
                                 (visRect.top() - imgRect.top()) * sy, visRect.width() * sx,
                                 visRect.height() * sy);
            p.setRenderHint(QPainter::SmoothPixmapTransform, d_zoom < 3.0);
            p.drawImage(visRect, d_image, srcRect);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        }
        p.setPen(QPen(QColor(160, 168, 180), 1));
        p.drawRect(imgRect);

        // Draw calibrated X & Y axis vectors
        if (d_hasX1 && d_hasX2) {
            p.setPen(QPen(QColor(25, 100, 210), 2, Qt::DashLine));
            p.drawLine(imagePixelToWidget(d_cal.pixelX1), imagePixelToWidget(d_cal.pixelX2));
        }
        if (d_hasY1 && d_hasY2) {
            p.setPen(QPen(QColor(20, 155, 85), 2, Qt::DashLine));
            p.drawLine(imagePixelToWidget(d_cal.pixelY1), imagePixelToWidget(d_cal.pixelY2));
        }

        auto drawAnchor = [&](const QPointF &pix, const QString &tag, const QColor &col) {
            const QPointF wp = imagePixelToWidget(pix);
            p.setPen(QPen(col, 2));
            p.setBrush(QColor(255, 255, 255, 210));
            p.drawEllipse(wp, 5.5, 5.5);
            p.drawLine(QPointF(wp.x() - 8.0, wp.y()), QPointF(wp.x() + 8.0, wp.y()));
            p.drawLine(QPointF(wp.x(), wp.y() - 8.0), QPointF(wp.x(), wp.y() + 8.0));
            p.setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));
            p.drawText(wp + QPointF(7.0, -6.0), tag);
        };

        if (d_hasX1)
            drawAnchor(d_cal.pixelX1, QStringLiteral("X1"), QColor(25, 100, 210));
        if (d_hasX2)
            drawAnchor(d_cal.pixelX2, QStringLiteral("X2"), QColor(25, 100, 210));
        if (d_hasY1)
            drawAnchor(d_cal.pixelY1, QStringLiteral("Y1"), QColor(20, 155, 85));
        if (d_hasY2)
            drawAnchor(d_cal.pixelY2, QStringLiteral("Y2"), QColor(20, 155, 85));

        // Draw optional Auto-Trace ROI box
        const QRect activeRoi = d_draggingRoi ? d_liveDragRoi : d_traceRoi;
        if (activeRoi.isValid() && !activeRoi.isEmpty()) {
            const QPointF tl = imagePixelToWidget(QPointF(activeRoi.left(), activeRoi.top()));
            const QPointF br = imagePixelToWidget(QPointF(activeRoi.right(), activeRoi.bottom()));
            const QRectF roiWidgetRect = QRectF(tl, br).normalized();
            p.setPen(QPen(QColor(150, 40, 195), 1.8, Qt::DashLine));
            p.setBrush(QColor(150, 40, 195, 26));
            p.drawRect(roiWidgetRect);
            p.setFont(QFont(QStringLiteral("Arial"), 8, QFont::Bold));
            p.drawText(roiWidgetRect.topLeft() + QPointF(4.0, 12.0), QStringLiteral("ROI"));
        }

        // Draw digitized points & polyline preview
        if (!d_pixelPts.isEmpty()) {
            p.setPen(QPen(QColor(220, 40, 45, 180), 1.5, Qt::SolidLine));
            for (int i = 1; i < d_pixelPts.size(); ++i) {
                p.drawLine(imagePixelToWidget(d_pixelPts[i - 1]),
                           imagePixelToWidget(d_pixelPts[i]));
            }
            p.setPen(QPen(QColor(185, 20, 25), 1.5));
            p.setBrush(QColor(235, 55, 55, 210));
            for (const QPointF &pt : d_pixelPts) {
                const QPointF wp = imagePixelToWidget(pt);
                p.drawEllipse(wp, 3.5, 3.5);
            }
        }
    }

    void mousePressEvent(QMouseEvent *ev) override
    {
        if (d_image.isNull() || !ev)
            return;
        // C4-9: middle-drag (or Ctrl+left-drag) pans the zoomed image.
        if (ev->button() == Qt::MiddleButton
            || (ev->button() == Qt::LeftButton && (ev->modifiers() & Qt::ControlModifier))) {
            d_panning = true;
            d_panButton = ev->button();
            d_panLast = ev->localPos();
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        bool inside = false;
        const QPointF pix = widgetToImagePixel(ev->localPos(), &inside);
        if (inside && roiDragMode && ev->button() == Qt::LeftButton) {
            d_draggingRoi = true;
            d_dragStartPix = pix;
            d_liveDragRoi = QRect(QPoint(static_cast<int>(std::round(pix.x())),
                                         static_cast<int>(std::round(pix.y()))),
                                  QSize(1, 1));
            update();
            return;
        }
        if (inside && onImageClicked)
            onImageClicked(pix, ev->button());
    }

    void mouseMoveEvent(QMouseEvent *ev) override
    {
        if (d_image.isNull() || !ev)
            return;
        if (d_panning) {
            d_pan += ev->localPos() - d_panLast;
            d_panLast = ev->localPos();
            clampPan();
            update();
        }
        bool inside = false;
        const QPointF pix = widgetToImagePixel(ev->localPos(), &inside);
        if (d_draggingRoi) {
            const QPoint p0(static_cast<int>(std::round(d_dragStartPix.x())),
                            static_cast<int>(std::round(d_dragStartPix.y())));
            const QPoint p1(static_cast<int>(std::round(pix.x())),
                            static_cast<int>(std::round(pix.y())));
            d_liveDragRoi = QRect(p0, p1).normalized();
            update();
        }
        if (inside && onImageHovered)
            onImageHovered(pix);
    }

    void mouseReleaseEvent(QMouseEvent *ev) override
    {
        if (!ev)
            return;
        if (d_panning && ev->button() == d_panButton) {
            d_panning = false;
            setCursor(Qt::CrossCursor);
            return;
        }
        if (!d_draggingRoi || ev->button() != Qt::LeftButton)
            return;
        d_draggingRoi = false;
        const QPointF pix = widgetToImagePixel(ev->localPos(), nullptr);
        const QPoint p0(static_cast<int>(std::round(d_dragStartPix.x())),
                        static_cast<int>(std::round(d_dragStartPix.y())));
        const QPoint p1(static_cast<int>(std::round(pix.x())),
                        static_cast<int>(std::round(pix.y())));
        const QRect finalRoi = QRect(p0, p1).normalized();
        if (finalRoi.width() >= 3 && finalRoi.height() >= 3) {
            d_traceRoi = finalRoi;
            if (onRoiDragged)
                onRoiDragged(d_traceRoi);
        } else if (onImageClicked) {
            // C4-#12: a press/release without a real drag is a plain click, so the
            // "click two opposite corners" route of the ROI mode actually works.
            onImageClicked(d_dragStartPix, Qt::LeftButton);
        }
        update();
    }

    void wheelEvent(QWheelEvent *ev) override
    {
        if (!ev || d_image.isNull()) {
            if (ev)
                ev->ignore();
            return;
        }
        const int dy = ev->angleDelta().y();
        if (dy == 0) {
            ev->ignore();
            return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QPointF pos = ev->position();
#else
        const QPointF pos = ev->posF();
#endif
        const QRectF r0 = computeTargetImageRect();
        if (r0.isEmpty()) {
            ev->ignore();
            return;
        }
        // Zoom about the cursor: the image point under the mouse stays put.
        const double ux = (pos.x() - r0.left()) / r0.width();
        const double uy = (pos.y() - r0.top()) / r0.height();
        const double factor = std::pow(1.25, static_cast<double>(dy) / 120.0);
        const double newZoom = std::clamp(d_zoom * factor, 1.0, 32.0);
        ev->accept();
        if (std::fabs(newZoom - d_zoom) < 1e-9)
            return;
        d_zoom = newZoom;
        const QRectF base = computeFitImageRect();
        const double w = base.width() * d_zoom;
        const double h = base.height() * d_zoom;
        d_pan.setX(pos.x() - ux * w - 0.5 * (static_cast<double>(width()) - w));
        d_pan.setY(pos.y() - uy * h - 0.5 * (static_cast<double>(height()) - h));
        clampPan();
        update();
        if (onZoomChanged)
            onZoomChanged(d_zoom);
        bool inside = false;
        const QPointF pix = widgetToImagePixel(pos, &inside);
        if (inside && onImageHovered)
            onImageHovered(pix);
    }

    void resizeEvent(QResizeEvent *ev) override
    {
        QWidget::resizeEvent(ev);
        clampPan();
    }

private:
    QImage d_image;
    GraphDigitizer::AxisCalibration d_cal;
    bool d_hasX1 = false;
    bool d_hasX2 = false;
    bool d_hasY1 = false;
    bool d_hasY2 = false;
    QVector<QPointF> d_pixelPts;
    QRect d_traceRoi;
    bool d_draggingRoi = false;
    QPointF d_dragStartPix;
    QRect d_liveDragRoi;
    double d_zoom = 1.0;
    QPointF d_pan;
    bool d_panning = false;
    Qt::MouseButton d_panButton = Qt::MiddleButton;
    QPointF d_panLast;
};

class DigitizerDialog : public QDialog
{
public:
    DigitizerDialog(ApplicationWindow *app, const QString &initialPath)
        : QDialog(app), d_app(app)
    {
        registerDigitizerTranslationsOnce();
        setWindowTitle(tr("Graph Image Digitizer"));
        resize(1020, 740);

        QVBoxLayout *rootLay = new QVBoxLayout(this);

        // Top toolbar: Image loading + Zoom + Mode indicator
        QHBoxLayout *topBar = new QHBoxLayout();
        QPushButton *btnOpen = new QPushButton(tr("Open Image..."), this);
        QPushButton *btnPaste = new QPushButton(tr("Paste from Clipboard"), this);
        QPushButton *btnResetZoom = new QPushButton(tr("Reset View"), this);
        btnResetZoom->setToolTip(tr("Reset canvas zoom and pan (mouse wheel zooms, Middle-drag or Ctrl+Left-drag pans)"));
        d_lblZoom = new QLabel(QStringLiteral("100%"), this);
        d_lblModeStatus = new QLabel(tr("Step 1: Load an image and calibrate X1, X2, Y1, Y2."), this);
        d_lblCursorReadout = new QLabel(tr("Cursor: (-, -)"), this);
        topBar->addWidget(btnOpen);
        topBar->addWidget(btnPaste);
        topBar->addWidget(btnResetZoom);
        topBar->addWidget(d_lblZoom);
        topBar->addSpacing(12);
        topBar->addWidget(d_lblModeStatus, 1);
        topBar->addWidget(d_lblCursorReadout);
        rootLay->addLayout(topBar);

        QSplitter *split = new QSplitter(Qt::Horizontal, this);
        d_canvas = new DigitizerImageCanvas(split);
        split->addWidget(d_canvas);

        // Right control panel
        QWidget *sidePanel = new QWidget(split);
        sidePanel->setMinimumWidth(340);
        QVBoxLayout *sideLay = new QVBoxLayout(sidePanel);
        sideLay->setContentsMargins(6, 0, 0, 0);

        // 0. 4x Magnifier Loupe for sub-pixel precision
        QGroupBox *grpLoupe = new QGroupBox(tr("4x Magnifier Loupe"), sidePanel);
        QVBoxLayout *loupeLay = new QVBoxLayout(grpLoupe);
        loupeLay->setContentsMargins(4, 4, 4, 4);
        d_loupe = new DigitizerMagnifierLoupe(grpLoupe);
        loupeLay->addWidget(d_loupe);
        sideLay->addWidget(grpLoupe);

        // 1. Axis Calibration Group
        QGroupBox *grpCal = new QGroupBox(tr("4-Point Axis Calibration"), sidePanel);
        QFormLayout *calForm = new QFormLayout(grpCal);

        auto makeValueSpin = [&](double initVal) {
            QDoubleSpinBox *sb = new QDoubleSpinBox(grpCal);
            sb->setRange(-1e15, 1e15);
            sb->setDecimals(6);
            sb->setValue(initVal);
            return sb;
        };

        QPushButton *btnSetX1 = new QPushButton(tr("Set X1"), grpCal);
        d_sbX1 = makeValueSpin(0.0);
        QHBoxLayout *rowX1 = new QHBoxLayout();
        rowX1->addWidget(btnSetX1);
        rowX1->addWidget(d_sbX1, 1);
        calForm->addRow(tr("X1 Anchor & Value:"), rowX1);

        QPushButton *btnSetX2 = new QPushButton(tr("Set X2"), grpCal);
        d_sbX2 = makeValueSpin(100.0);
        QHBoxLayout *rowX2 = new QHBoxLayout();
        rowX2->addWidget(btnSetX2);
        rowX2->addWidget(d_sbX2, 1);
        calForm->addRow(tr("X2 Anchor & Value:"), rowX2);

        d_cbScaleX = new QComboBox(grpCal);
        d_cbScaleX->addItem(tr("Linear"), false);
        d_cbScaleX->addItem(tr("Log10"), true);
        calForm->addRow(tr("X Axis Scale:"), d_cbScaleX);

        QPushButton *btnSetY1 = new QPushButton(tr("Set Y1"), grpCal);
        d_sbY1 = makeValueSpin(0.0);
        QHBoxLayout *rowY1 = new QHBoxLayout();
        rowY1->addWidget(btnSetY1);
        rowY1->addWidget(d_sbY1, 1);
        calForm->addRow(tr("Y1 Anchor & Value:"), rowY1);

        QPushButton *btnSetY2 = new QPushButton(tr("Set Y2"), grpCal);
        d_sbY2 = makeValueSpin(100.0);
        QHBoxLayout *rowY2 = new QHBoxLayout();
        rowY2->addWidget(btnSetY2);
        rowY2->addWidget(d_sbY2, 1);
        calForm->addRow(tr("Y2 Anchor & Value:"), rowY2);

        d_cbScaleY = new QComboBox(grpCal);
        d_cbScaleY->addItem(tr("Linear"), false);
        d_cbScaleY->addItem(tr("Log10"), true);
        calForm->addRow(tr("Y Axis Scale:"), d_cbScaleY);

        d_lblCalWarning = new QLabel(grpCal);
        d_lblCalWarning->setWordWrap(true);
        d_lblCalWarning->setStyleSheet(QStringLiteral("color: #B91C1C; font-weight: bold;"));
        calForm->addRow(d_lblCalWarning);

        sideLay->addWidget(grpCal);

        // 2. Digitizing & Auto-Trace Group
        QGroupBox *grpTrace = new QGroupBox(tr("Point Picking & Color Auto-Trace"), sidePanel);
        QVBoxLayout *traceLay = new QVBoxLayout(grpTrace);

        QHBoxLayout *pickRow = new QHBoxLayout();
        QPushButton *btnPickManual = new QPushButton(tr("Pick Data Points"), grpTrace);
        QPushButton *btnPickColor = new QPushButton(tr("Pick Curve Color & Auto-Trace"), grpTrace);
        pickRow->addWidget(btnPickManual);
        pickRow->addWidget(btnPickColor);
        traceLay->addLayout(pickRow);

        QFormLayout *traceForm = new QFormLayout();
        QHBoxLayout *colRow = new QHBoxLayout();
        d_lblColorSwatch = new QLabel(grpTrace);
        d_lblColorSwatch->setFixedSize(28, 20);
        QPushButton *btnChooseColor = new QPushButton(tr("Select Color..."), grpTrace);
        QPushButton *btnRunTrace = new QPushButton(tr("Auto-Trace"), grpTrace);
        colRow->addWidget(d_lblColorSwatch);
        colRow->addWidget(btnChooseColor);
        colRow->addWidget(btnRunTrace);
        traceForm->addRow(tr("Target Color:"), colRow);

        d_sbTol = new QSpinBox(grpTrace);
        d_sbTol->setRange(1, 255);
        d_sbTol->setValue(45);
        traceForm->addRow(tr("Color Tolerance:"), d_sbTol);

        d_sbSamples = new QSpinBox(grpTrace);
        d_sbSamples->setRange(5, 2000);
        d_sbSamples->setValue(100);
        traceForm->addRow(tr("Sample Points:"), d_sbSamples);
        traceLay->addLayout(traceForm);

        QHBoxLayout *traceOptsRow = new QHBoxLayout();
        d_chkTraceAppend = new QCheckBox(tr("Append to existing points"), grpTrace);
        d_chkTraceAppend->setChecked(false);
        d_chkSortX = new QCheckBox(tr("Sort by X"), grpTrace);
        d_chkSortX->setChecked(false);
        traceOptsRow->addWidget(d_chkTraceAppend);
        traceOptsRow->addWidget(d_chkSortX);
        traceLay->addLayout(traceOptsRow);

        QHBoxLayout *roiRow = new QHBoxLayout();
        QPushButton *btnSetRoi = new QPushButton(tr("Set Trace ROI"), grpTrace);
        QPushButton *btnClearRoi = new QPushButton(tr("Clear ROI"), grpTrace);
        roiRow->addWidget(btnSetRoi);
        roiRow->addWidget(btnClearRoi);
        traceLay->addLayout(roiRow);

        d_lblRoiStatus = new QLabel(tr("ROI: Full Calibrated Area"), grpTrace);
        traceLay->addWidget(d_lblRoiStatus);

        QHBoxLayout *editPtsRow = new QHBoxLayout();
        QPushButton *btnRemoveSel = new QPushButton(tr("Delete Selected Point"), grpTrace);
        QPushButton *btnClearPts = new QPushButton(tr("Clear Points"), grpTrace);
        editPtsRow->addWidget(btnRemoveSel);
        editPtsRow->addWidget(btnClearPts);
        traceLay->addLayout(editPtsRow);

        sideLay->addWidget(grpTrace);

        // 3. Live Point Preview Table
        d_tablePreview = new QTableWidget(0, 2, sidePanel);
        d_tablePreview->setHorizontalHeaderLabels(QStringList() << tr("X") << tr("Y"));
        d_tablePreview->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        sideLay->addWidget(d_tablePreview, 1);

        // 4. Export Controls
        QFormLayout *expForm = new QFormLayout();
        d_edTableName = new QLineEdit(QStringLiteral("DigitizedData"), sidePanel);
        expForm->addRow(tr("Output Worksheet Name:"), d_edTableName);
        sideLay->addLayout(expForm);

        QHBoxLayout *expBtns = new QHBoxLayout();
        QPushButton *btnExportTbl = new QPushButton(tr("Export to Worksheet"), sidePanel);
        QPushButton *btnExportBoth = new QPushButton(tr("Export to Worksheet & Graph"), sidePanel);
        QPushButton *btnClose = new QPushButton(tr("Close"), sidePanel);
        expBtns->addWidget(btnExportTbl);
        expBtns->addWidget(btnExportBoth);
        expBtns->addWidget(btnClose);
        sideLay->addLayout(expBtns);

        split->addWidget(sidePanel);
        split->setStretchFactor(0, 3);
        split->setStretchFactor(1, 2);
        rootLay->addWidget(split, 1);

        updateColorSwatch();

        // Wire signals
        connect(btnOpen, &QPushButton::clicked, this, [this]() {
            const QString fn = QFileDialog::getOpenFileName(
                    this, tr("Open Graph Image"), QString(),
                    tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)"));
            if (!fn.isEmpty())
                loadImageFile(fn);
        });
        connect(btnPaste, &QPushButton::clicked, this, [this]() {
            const QClipboard *cb = QGuiApplication::clipboard();
            if (!cb)
                return;
            QImage clipImg = cb->image();
            if (clipImg.isNull()) {
                QMessageBox::information(this, tr("Graph Image Digitizer"),
                                         tr("No image found in the system clipboard."));
                return;
            }
            setWorkingImage(clipImg);
        });

        connect(btnResetZoom, &QPushButton::clicked, this, [this]() {
            if (d_canvas)
                d_canvas->resetView();
        });
        connect(d_chkSortX, &QCheckBox::toggled, this,
                [this](bool) { refreshCalibrationAndPreview(); });

        connect(btnSetX1, &QPushButton::clicked, this, [this]() { setMode(DigitizerToolMode::SetX1); });
        connect(btnSetX2, &QPushButton::clicked, this, [this]() { setMode(DigitizerToolMode::SetX2); });
        connect(btnSetY1, &QPushButton::clicked, this, [this]() { setMode(DigitizerToolMode::SetY1); });
        connect(btnSetY2, &QPushButton::clicked, this, [this]() { setMode(DigitizerToolMode::SetY2); });
        connect(btnPickManual, &QPushButton::clicked, this,
                [this]() { setMode(DigitizerToolMode::PickPoints); });
        connect(btnPickColor, &QPushButton::clicked, this,
                [this]() { setMode(DigitizerToolMode::PickColorAutoTrace); });
        connect(btnSetRoi, &QPushButton::clicked, this, [this]() {
            d_hasRoiFirstCorner = false;
            setMode(DigitizerToolMode::SetTraceRoi);
        });
        connect(btnClearRoi, &QPushButton::clicked, this, [this]() {
            d_hasRoiFirstCorner = false;
            d_canvas->setTraceRoi(QRect());
            if (d_lblRoiStatus)
                d_lblRoiStatus->setText(tr("ROI: Full Calibrated Area"));
            if (d_mode == DigitizerToolMode::SetTraceRoi)
                setMode(DigitizerToolMode::PickColorAutoTrace);
        });

        auto onCalChanged = [this]() { refreshCalibrationAndPreview(); };
        connect(d_sbX1, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [onCalChanged](double) { onCalChanged(); });
        connect(d_sbX2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [onCalChanged](double) { onCalChanged(); });
        connect(d_sbY1, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [onCalChanged](double) { onCalChanged(); });
        connect(d_sbY2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [onCalChanged](double) { onCalChanged(); });
        connect(d_cbScaleX, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [onCalChanged](int) { onCalChanged(); });
        connect(d_cbScaleY, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [onCalChanged](int) { onCalChanged(); });

        connect(btnChooseColor, &QPushButton::clicked, this, [this]() {
            const QColor chosen = QColorDialog::getColor(d_targetColor, this, tr("Select Curve Color"));
            if (chosen.isValid()) {
                d_targetColor = chosen;
                updateColorSwatch();
            }
        });
        connect(btnRunTrace, &QPushButton::clicked, this, [this]() { runAutoTraceNow(); });

        connect(btnRemoveSel, &QPushButton::clicked, this, [this]() {
            const int row = d_tablePreview->currentRow();
            if (row < 0)
                return;
            QTableWidgetItem *item = d_tablePreview->item(row, 0);
            if (!item)
                return;
            bool okIdx = false;
            const int origIdx = item->data(Qt::UserRole).toInt(&okIdx);
            if (okIdx && origIdx >= 0 && origIdx < d_pixelPts.size()) {
                d_pixelPts.remove(origIdx);
                if (origIdx < d_pointIsManual.size())
                    d_pointIsManual.remove(origIdx);
                refreshCalibrationAndPreview();
            }
        });
        connect(btnClearPts, &QPushButton::clicked, this, [this]() {
            d_pixelPts.clear();
            d_pointIsManual.clear();
            refreshCalibrationAndPreview();
        });

        connect(btnExportTbl, &QPushButton::clicked, this, [this]() { exportData(false); });
        connect(btnExportBoth, &QPushButton::clicked, this, [this]() { exportData(true); });
        connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

        d_canvas->onImageClicked = [this](const QPointF &pix, Qt::MouseButton btn) {
            handleCanvasClick(pix, btn);
        };
        d_canvas->onImageHovered = [this](const QPointF &pix) {
            if (d_loupe)
                d_loupe->setFocusPixel(pix);
            const QPointF dp = GraphDigitizer::mapPixelToData(pix, currentCalibration());
            d_lblCursorReadout->setText(tr("Pixel (%1, %2) -> Data (%3, %4)")
                                                .arg(std::round(pix.x()))
                                                .arg(std::round(pix.y()))
                                                .arg(dp.x(), 0, 'g', 5)
                                                .arg(dp.y(), 0, 'g', 5));
        };
        d_canvas->onRoiDragged = [this](const QRect &roi) {
            d_hasRoiFirstCorner = false;
            if (d_lblRoiStatus) {
                d_lblRoiStatus->setText(tr("ROI Active: (%1, %2) %3x%4 px")
                                                .arg(roi.x())
                                                .arg(roi.y())
                                                .arg(roi.width())
                                                .arg(roi.height()));
            }
            setMode(DigitizerToolMode::PickColorAutoTrace);
        };

        d_canvas->onZoomChanged = [this](double z) {
            if (d_lblZoom)
                d_lblZoom->setText(QStringLiteral("%1%").arg(qRound(z * 100.0)));
        };

        if (!initialPath.trimmed().isEmpty())
            loadImageFile(initialPath);
        refreshCalibrationAndPreview();
    }

private:
    GraphDigitizer::AxisCalibration currentCalibration() const
    {
        GraphDigitizer::AxisCalibration cal = d_cal;
        cal.valueX1 = d_sbX1->value();
        cal.valueX2 = d_sbX2->value();
        cal.logX = d_cbScaleX->currentData().toBool();
        cal.valueY1 = d_sbY1->value();
        cal.valueY2 = d_sbY2->value();
        cal.logY = d_cbScaleY->currentData().toBool();
        return cal;
    }

    void updateColorSwatch()
    {
        d_lblColorSwatch->setStyleSheet(
                QStringLiteral("background-color: %1; border: 1px solid #555;")
                        .arg(d_targetColor.name()));
    }

    void setMode(DigitizerToolMode m)
    {
        d_mode = m;
        if (d_canvas)
            d_canvas->roiDragMode = (m == DigitizerToolMode::SetTraceRoi);
        switch (m) {
        case DigitizerToolMode::SetX1:
            d_lblModeStatus->setText(tr("Click on the image to set X1 anchor."));
            break;
        case DigitizerToolMode::SetX2:
            d_lblModeStatus->setText(tr("Click on the image to set X2 anchor."));
            break;
        case DigitizerToolMode::SetY1:
            d_lblModeStatus->setText(tr("Click on the image to set Y1 anchor."));
            break;
        case DigitizerToolMode::SetY2:
            d_lblModeStatus->setText(tr("Click on the image to set Y2 anchor."));
            break;
        case DigitizerToolMode::PickPoints:
            d_lblModeStatus->setText(tr("Left-click on curve to add points; right-click to undo last point."));
            break;
        case DigitizerToolMode::PickColorAutoTrace:
            d_lblModeStatus->setText(tr("Click directly on a colored curve in the image to sample color & auto-trace."));
            break;
        case DigitizerToolMode::SetTraceRoi:
            d_lblModeStatus->setText(
                    tr("Drag or click two corners on the image to define the Auto-Trace ROI box."));
            break;
        }
    }

    void loadImageFile(const QString &path)
    {
        QImage img(path);
        if (img.isNull()) {
            QMessageBox::warning(this, tr("Graph Image Digitizer"),
                                 tr("Failed to load image file: %1").arg(path));
            return;
        }
        setWorkingImage(img);
    }

    void setWorkingImage(const QImage &img)
    {
        const int w = img.width();
        const int h = img.height();
        d_cal.pixelX1 = QPointF(0.10 * w, 0.88 * h);
        d_cal.pixelX2 = QPointF(0.90 * w, 0.88 * h);
        d_cal.pixelY1 = QPointF(0.10 * w, 0.88 * h);
        d_cal.pixelY2 = QPointF(0.10 * w, 0.12 * h);
        // C4-#8: anchors are not calibrated until the user actually clicks them.
        d_hasX1 = false;
        d_hasX2 = false;
        d_hasY1 = false;
        d_hasY2 = false;
        d_pixelPts.clear();
        d_pointIsManual.clear();
        if (d_chkSortX)
            d_chkSortX->setChecked(false);
        d_hasRoiFirstCorner = false;
        d_canvas->setTraceRoi(QRect());
        if (d_lblRoiStatus)
            d_lblRoiStatus->setText(tr("ROI: Full Calibrated Area"));
        d_canvas->setImage(img);
        if (d_loupe)
            d_loupe->setImage(img);
        setMode(DigitizerToolMode::SetX1);
        refreshCalibrationAndPreview();
    }

    void handleCanvasClick(const QPointF &pix, Qt::MouseButton btn)
    {
        if (btn == Qt::RightButton) {
            if (!d_pixelPts.isEmpty()) {
                d_pixelPts.removeLast();
                if (!d_pointIsManual.isEmpty())
                    d_pointIsManual.removeLast();
                refreshCalibrationAndPreview();
            }
            return;
        }
        if (btn != Qt::LeftButton)
            return;

        switch (d_mode) {
        case DigitizerToolMode::SetX1:
            d_cal.pixelX1 = pix;
            d_hasX1 = true;
            setMode(DigitizerToolMode::SetX2);
            break;
        case DigitizerToolMode::SetX2:
            d_cal.pixelX2 = pix;
            d_hasX2 = true;
            setMode(DigitizerToolMode::SetY1);
            break;
        case DigitizerToolMode::SetY1:
            d_cal.pixelY1 = pix;
            d_hasY1 = true;
            setMode(DigitizerToolMode::SetY2);
            break;
        case DigitizerToolMode::SetY2:
            d_cal.pixelY2 = pix;
            d_hasY2 = true;
            setMode(DigitizerToolMode::PickPoints);
            break;
        case DigitizerToolMode::PickPoints:
            d_pixelPts.append(pix);
            d_pointIsManual.append(true);
            break;
        case DigitizerToolMode::PickColorAutoTrace: {
            const QImage &im = d_canvas->image();
            const int ix = std::clamp(static_cast<int>(std::round(pix.x())), 0, im.width() - 1);
            const int iy = std::clamp(static_cast<int>(std::round(pix.y())), 0, im.height() - 1);
            d_targetColor = im.pixelColor(ix, iy);
            updateColorSwatch();
            runAutoTraceNow();
            setMode(DigitizerToolMode::PickPoints);
            return;
        }
        case DigitizerToolMode::SetTraceRoi: {
            const QPoint pt(static_cast<int>(std::round(pix.x())),
                            static_cast<int>(std::round(pix.y())));
            if (!d_hasRoiFirstCorner) {
                d_roiFirstCorner = pt;
                d_hasRoiFirstCorner = true;
                if (d_lblModeStatus) {
                    d_lblModeStatus->setText(
                            tr("Click the second opposite corner on the image to finish the ROI box."));
                }
            } else {
                const QRect roi = QRect(d_roiFirstCorner, pt).normalized();
                d_hasRoiFirstCorner = false;
                if (roi.width() >= 3 && roi.height() >= 3) {
                    d_canvas->setTraceRoi(roi);
                    if (d_lblRoiStatus) {
                        d_lblRoiStatus->setText(tr("ROI Active: (%1, %2) %3x%4 px")
                                                        .arg(roi.x())
                                                        .arg(roi.y())
                                                        .arg(roi.width())
                                                        .arg(roi.height()));
                    }
                    setMode(DigitizerToolMode::PickColorAutoTrace);
                }
            }
            return;
        }
        }
        refreshCalibrationAndPreview();
    }

    void runAutoTraceNow()
    {
        const QImage &im = d_canvas->image();
        if (im.isNull())
            return;
        const GraphDigitizer::AxisCalibration cal = currentCalibration();
        const QVector<QPointF> tracedData = GraphDigitizer::autoTraceCurveByColor(
                im, cal, d_targetColor, d_sbTol->value(), d_sbSamples->value(),
                d_canvas->traceRoi());
        const bool append = d_chkTraceAppend && d_chkTraceAppend->isChecked();
        if (!append) {
            const bool hasManualPts = std::any_of(
                    d_pointIsManual.cbegin(), d_pointIsManual.cend(),
                    [](bool isManual) { return isManual; });
            const QObject *ctxObj = d_app ? static_cast<const QObject *>(d_app)
                                          : static_cast<const QObject *>(this);
            if (hasManualPts && !ApplicationWindow::isNonInteractiveContext(ctxObj)) {
                const auto ans = QMessageBox::question(
                        this, tr("Graph Image Digitizer"),
                        tr("Auto-trace with \"Append\" unchecked will replace your existing manually picked points.\nContinue and replace existing points?"),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (ans != QMessageBox::Yes)
                    return;
            }
            d_pixelPts.clear();
            d_pointIsManual.clear();
            if (d_chkSortX)
                d_chkSortX->setChecked(true);
        }
        d_pixelPts.reserve(d_pixelPts.size() + tracedData.size());
        d_pointIsManual.reserve(d_pointIsManual.size() + tracedData.size());
        for (const QPointF &dp : tracedData) {
            d_pixelPts.append(GraphDigitizer::mapDataToPixel(dp, cal));
            d_pointIsManual.append(false);
        }
        refreshCalibrationAndPreview();
    }

    struct OrderedPoint
    {
        int origIndex;
        QPointF dataPt;
    };

    QVector<OrderedPoint> currentOrderedDataPoints() const
    {
        const GraphDigitizer::AxisCalibration cal = currentCalibration();
        QVector<OrderedPoint> out;
        out.reserve(d_pixelPts.size());
        for (int i = 0; i < d_pixelPts.size(); ++i)
            out.append({ i, GraphDigitizer::mapPixelToData(d_pixelPts[i], cal) });
        if (d_chkSortX && d_chkSortX->isChecked()) {
            std::stable_sort(out.begin(), out.end(),
                             [](const OrderedPoint &a, const OrderedPoint &b) {
                                 return a.dataPt.x() < b.dataPt.x();
                             });
        }
        return out;
    }

    QString calibrationWarningMessage(const GraphDigitizer::AxisCalibration &cal,
                                      bool *hasLogErrorOut = nullptr,
                                      bool *hasMissingAnchorsOut = nullptr,
                                      bool *hasDegenerateOut = nullptr) const
    {
        QStringList missing;
        if (!d_hasX1)
            missing << QStringLiteral("X1");
        if (!d_hasX2)
            missing << QStringLiteral("X2");
        if (!d_hasY1)
            missing << QStringLiteral("Y1");
        if (!d_hasY2)
            missing << QStringLiteral("Y2");
        const bool missingAnchors = !missing.isEmpty();

        QStringList logBad;
        if (cal.logX && (cal.valueX1 <= 0.0 || cal.valueX2 <= 0.0))
            logBad << tr("X axis Log10 requires X1 > 0 and X2 > 0 (currently linear fallback).");
        if (cal.logY && (cal.valueY1 <= 0.0 || cal.valueY2 <= 0.0))
            logBad << tr("Y axis Log10 requires Y1 > 0 and Y2 > 0 (currently linear fallback).");
        const bool logError = !logBad.isEmpty();

        QStringList degenBad;
        const bool xValsEqual = !std::isfinite(cal.valueX1) || !std::isfinite(cal.valueX2)
                || std::fabs(cal.valueX2 - cal.valueX1)
                        <= 1e-12 * std::max(std::fabs(cal.valueX1), std::fabs(cal.valueX2));
        const bool yValsEqual = !std::isfinite(cal.valueY1) || !std::isfinite(cal.valueY2)
                || std::fabs(cal.valueY2 - cal.valueY1)
                        <= 1e-12 * std::max(std::fabs(cal.valueY1), std::fabs(cal.valueY2));
        if (xValsEqual)
            degenBad << tr("X axis calibration requires X1 != X2.");
        if (yValsEqual)
            degenBad << tr("Y axis calibration requires Y1 != Y2.");

        const double pxDistX = std::hypot(cal.pixelX2.x() - cal.pixelX1.x(),
                                          cal.pixelX2.y() - cal.pixelX1.y());
        const double pxDistY = std::hypot(cal.pixelY2.x() - cal.pixelY1.x(),
                                          cal.pixelY2.y() - cal.pixelY1.y());
        if (d_hasX1 && d_hasX2 && pxDistX < 1.0)
            degenBad << tr("X1 and X2 pixel anchors must be distinct (distance >= 1 px).");
        if (d_hasY1 && d_hasY2 && pxDistY < 1.0)
            degenBad << tr("Y1 and Y2 pixel anchors must be distinct (distance >= 1 px).");
        const bool degenerateError = !degenBad.isEmpty();

        if (hasMissingAnchorsOut)
            *hasMissingAnchorsOut = missingAnchors;
        if (hasLogErrorOut)
            *hasLogErrorOut = logError;
        if (hasDegenerateOut)
            *hasDegenerateOut = degenerateError;

        QStringList lines;
        if (missingAnchors)
            lines << tr("Uncalibrated anchors: %1 (using default bounds).").arg(missing.join(QStringLiteral(", ")));
        lines.append(logBad);
        lines.append(degenBad);
        return lines.join(QStringLiteral("\n"));
    }

    void refreshCalibrationAndPreview()
    {
        const GraphDigitizer::AxisCalibration cal = currentCalibration();
        d_canvas->setCalibration(cal, d_hasX1, d_hasX2, d_hasY1, d_hasY2);
        d_canvas->setPickedPixelPoints(d_pixelPts);

        if (d_lblCalWarning) {
            const QString warn = calibrationWarningMessage(cal);
            d_lblCalWarning->setText(warn);
            d_lblCalWarning->setVisible(!warn.isEmpty());
        }

        const QVector<OrderedPoint> ordered = currentOrderedDataPoints();
        d_tablePreview->setRowCount(ordered.size());
        for (int i = 0; i < ordered.size(); ++i) {
            QTableWidgetItem *xItem = new QTableWidgetItem(
                    QString::number(ordered[i].dataPt.x(), 'g', 7));
            // C4-2 / C4-#7: remember original index in d_pixelPts even when sorted by X.
            xItem->setData(Qt::UserRole, ordered[i].origIndex);
            QTableWidgetItem *yItem = new QTableWidgetItem(
                    QString::number(ordered[i].dataPt.y(), 'g', 7));
            yItem->setData(Qt::UserRole, ordered[i].origIndex);
            d_tablePreview->setItem(i, 0, xItem);
            d_tablePreview->setItem(i, 1, yItem);
        }
    }

    void exportData(bool withPlot)
    {
        const GraphDigitizer::AxisCalibration cal = currentCalibration();
        bool logError = false;
        bool missingAnchors = false;
        bool degenerateError = false;
        const QString warn =
                calibrationWarningMessage(cal, &logError, &missingAnchors, &degenerateError);
        if (logError || degenerateError) {
            QMessageBox::warning(this, tr("Graph Image Digitizer"), warn);
            return;
        }
        if (missingAnchors) {
            const auto ans = QMessageBox::warning(
                    this, tr("Graph Image Digitizer"),
                    warn + QStringLiteral("\n\n") + tr("Export with uncalibrated defaults anyway?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ans != QMessageBox::Yes)
                return;
        }

        const QVector<OrderedPoint> ordered = currentOrderedDataPoints();
        QVector<QPointF> dataPts;
        dataPts.reserve(ordered.size());
        for (const OrderedPoint &op : ordered)
            dataPts.append(op.dataPt);

        if (dataPts.isEmpty()) {
            QMessageBox::information(this, tr("Graph Image Digitizer"),
                                     tr("Please pick or auto-trace at least one curve point first."));
            return;
        }
        MultiLayer *ml = nullptr;
        GraphDigitizer::exportPointsToTable(d_app, dataPts, d_edTableName->text().trimmed(),
                                            withPlot ? &ml : nullptr);
    }

    ApplicationWindow *d_app = nullptr;
    DigitizerImageCanvas *d_canvas = nullptr;
    DigitizerMagnifierLoupe *d_loupe = nullptr;
    GraphDigitizer::AxisCalibration d_cal;
    bool d_hasX1 = false;
    bool d_hasX2 = false;
    bool d_hasY1 = false;
    bool d_hasY2 = false;
    DigitizerToolMode d_mode = DigitizerToolMode::SetX1;
    QColor d_targetColor = QColor(220, 30, 30);
    QVector<QPointF> d_pixelPts;
    QVector<bool> d_pointIsManual;
    bool d_hasRoiFirstCorner = false;
    QPoint d_roiFirstCorner;

    QLabel *d_lblModeStatus = nullptr;
    QLabel *d_lblZoom = nullptr;
    QLabel *d_lblCursorReadout = nullptr;
    QLabel *d_lblCalWarning = nullptr;
    QLabel *d_lblColorSwatch = nullptr;
    QLabel *d_lblRoiStatus = nullptr;
    QCheckBox *d_chkTraceAppend = nullptr;
    QCheckBox *d_chkSortX = nullptr;
    QDoubleSpinBox *d_sbX1 = nullptr;
    QDoubleSpinBox *d_sbX2 = nullptr;
    QDoubleSpinBox *d_sbY1 = nullptr;
    QDoubleSpinBox *d_sbY2 = nullptr;
    QComboBox *d_cbScaleX = nullptr;
    QComboBox *d_cbScaleY = nullptr;
    QSpinBox *d_sbTol = nullptr;
    QSpinBox *d_sbSamples = nullptr;
    QTableWidget *d_tablePreview = nullptr;
    QLineEdit *d_edTableName = nullptr;
};

} // namespace

QPointF GraphDigitizer::mapPixelToData(const QPointF &pixelPt, const AxisCalibration &cal)
{
    const QPointF uX = cal.pixelX2 - cal.pixelX1;
    const double lenSqX = uX.x() * uX.x() + uX.y() * uX.y();
    const double tX = (lenSqX > 1e-24)
            ? (((pixelPt.x() - cal.pixelX1.x()) * uX.x()
                + (pixelPt.y() - cal.pixelX1.y()) * uX.y())
               / lenSqX)
            : 0.0;

    const QPointF uY = cal.pixelY2 - cal.pixelY1;
    const double lenSqY = uY.x() * uY.x() + uY.y() * uY.y();
    const double tY = (lenSqY > 1e-24)
            ? (((pixelPt.x() - cal.pixelY1.x()) * uY.x()
                + (pixelPt.y() - cal.pixelY1.y()) * uY.y())
               / lenSqY)
            : 0.0;

    double dataX = 0.0;
    if (cal.logX && cal.valueX1 > 0.0 && cal.valueX2 > 0.0) {
        const double l1 = std::log10(cal.valueX1);
        const double l2 = std::log10(cal.valueX2);
        dataX = std::pow(10.0, l1 + tX * (l2 - l1));
    } else {
        dataX = cal.valueX1 + tX * (cal.valueX2 - cal.valueX1);
    }

    double dataY = 0.0;
    if (cal.logY && cal.valueY1 > 0.0 && cal.valueY2 > 0.0) {
        const double l1 = std::log10(cal.valueY1);
        const double l2 = std::log10(cal.valueY2);
        dataY = std::pow(10.0, l1 + tY * (l2 - l1));
    } else {
        dataY = cal.valueY1 + tY * (cal.valueY2 - cal.valueY1);
    }

    return QPointF(dataX, dataY);
}

QPointF GraphDigitizer::mapDataToPixel(const QPointF &dataPt, const AxisCalibration &cal)
{
    double tX = 0.0;
    if (cal.logX && cal.valueX1 > 0.0 && cal.valueX2 > 0.0 && dataPt.x() > 0.0) {
        const double l1 = std::log10(cal.valueX1);
        const double l2 = std::log10(cal.valueX2);
        const double dl = l2 - l1;
        tX = (std::fabs(dl) > 1e-300) ? ((std::log10(dataPt.x()) - l1) / dl) : 0.0;
    } else {
        const double dv = cal.valueX2 - cal.valueX1;
        tX = (std::fabs(dv) > 1e-300) ? ((dataPt.x() - cal.valueX1) / dv) : 0.0;
    }

    double tY = 0.0;
    if (cal.logY && cal.valueY1 > 0.0 && cal.valueY2 > 0.0 && dataPt.y() > 0.0) {
        const double l1 = std::log10(cal.valueY1);
        const double l2 = std::log10(cal.valueY2);
        const double dl = l2 - l1;
        tY = (std::fabs(dl) > 1e-300) ? ((std::log10(dataPt.y()) - l1) / dl) : 0.0;
    } else {
        const double dv = cal.valueY2 - cal.valueY1;
        tY = (std::fabs(dv) > 1e-300) ? ((dataPt.y() - cal.valueY1) / dv) : 0.0;
    }

    const QPointF uX = cal.pixelX2 - cal.pixelX1;
    const QPointF uY = cal.pixelY2 - cal.pixelY1;
    const double lenSqX = uX.x() * uX.x() + uX.y() * uX.y();
    const double lenSqY = uY.x() * uY.x() + uY.y() * uY.y();

    // Exact 2x2 inverse of orthogonal/oblique axis-vector projection
    const double det = uX.x() * uY.y() - uX.y() * uY.x();
    const double normScale = std::sqrt(std::max(1e-30, lenSqX * lenSqY));
    if (lenSqX > 1e-24 && lenSqY > 1e-24 && std::fabs(det) > 1e-12 * normScale) {
        const double r1 = (cal.pixelX1.x() * uX.x() + cal.pixelX1.y() * uX.y()) + tX * lenSqX;
        const double r2 = (cal.pixelY1.x() * uY.x() + cal.pixelY1.y() * uY.y()) + tY * lenSqY;
        const double px = (r1 * uY.y() - r2 * uX.y()) / det;
        const double py = (uX.x() * r2 - uY.x() * r1) / det;
        return QPointF(px, py);
    }

    return QPointF(cal.pixelX1.x() + tX * uX.x(), cal.pixelY1.y() + tY * uY.y());
}

QVector<QPointF> GraphDigitizer::autoTraceCurveByColor(const QImage &img,
                                                       const AxisCalibration &cal,
                                                       const QColor &targetColor,
                                                       int colorTolerance,
                                                       int numSamplesX,
                                                       const QRect &pixelRoi)
{
    QVector<QPointF> out;
    if (img.isNull() || img.width() <= 0 || img.height() <= 0 || !targetColor.isValid())
        return out;

    const QImage argb = img.convertToFormat(QImage::Format_ARGB32);
    const int W = argb.width();
    const int H = argb.height();

    double xMinPix = std::min(cal.pixelX1.x(), cal.pixelX2.x());
    double xMaxPix = std::max(cal.pixelX1.x(), cal.pixelX2.x());
    if (xMaxPix - xMinPix < 1.0) {
        xMinPix = 0.0;
        xMaxPix = static_cast<double>(W - 1);
    }
    xMinPix = std::clamp(xMinPix, 0.0, static_cast<double>(W - 1));
    xMaxPix = std::clamp(xMaxPix, 0.0, static_cast<double>(W - 1));

    const double yMinCal = std::min(cal.pixelY1.y(), cal.pixelY2.y());
    const double yMaxCal = std::max(cal.pixelY1.y(), cal.pixelY2.y());
    int yLo = 0;
    int yHi = H - 1;
    if (yMaxCal - yMinCal >= 1.0) {
        yLo = std::clamp(static_cast<int>(std::floor(yMinCal)) - 2, 0, H - 1);
        yHi = std::clamp(static_cast<int>(std::ceil(yMaxCal)) + 2, 0, H - 1);
    }

    if (pixelRoi.isValid() && !pixelRoi.isEmpty()) {
        const QRect rNorm = pixelRoi.normalized();
        xMinPix = std::max(xMinPix, static_cast<double>(std::clamp(rNorm.left(), 0, W - 1)));
        xMaxPix = std::min(xMaxPix, static_cast<double>(std::clamp(rNorm.right(), 0, W - 1)));
        yLo = std::max(yLo, std::clamp(rNorm.top(), 0, H - 1));
        yHi = std::min(yHi, std::clamp(rNorm.bottom(), 0, H - 1));
        if (xMaxPix < xMinPix || yHi < yLo)
            return out;
    }

    const int nSamples = std::max(2, numSamplesX);
    const double tol = static_cast<double>(std::max(0, colorTolerance));
    const double tolSq = tol * tol;
    const int trR = targetColor.red();
    const int trG = targetColor.green();
    const int trB = targetColor.blue();

    int prevPx = -1;
    out.reserve(nSamples);

    for (int s = 0; s < nSamples; ++s) {
        const double pxF = xMinPix
                + (xMaxPix - xMinPix) * static_cast<double>(s) / static_cast<double>(nSamples - 1);
        const int px = std::clamp(static_cast<int>(std::round(pxF)), 0, W - 1);
        if (px == prevPx)
            continue;
        prevPx = px;

        // Find contiguous vertical runs of color-matching pixels and pick strongest cluster
        double bestClusterWeight = 0.0;
        double bestClusterWy = 0.0;

        double curWeight = 0.0;
        double curWy = 0.0;
        int lastMatchY = -10;

        for (int py = yLo; py <= yHi; ++py) {
            const QRgb rgb = reinterpret_cast<const QRgb *>(argb.constScanLine(py))[px];
            if (qAlpha(rgb) < 16)
                continue;
            const double dr = static_cast<double>(qRed(rgb) - trR);
            const double dg = static_cast<double>(qGreen(rgb) - trG);
            const double db = static_cast<double>(qBlue(rgb) - trB);
            const double distSq = dr * dr + dg * dg + db * db;
            if (distSq <= tolSq) {
                const double dist = std::sqrt(distSq);
                const double w = (1.0 - dist / (tol + 1.0)) + 0.05;
                if (py - lastMatchY <= 2) {
                    curWeight += w;
                    curWy += w * static_cast<double>(py);
                } else {
                    if (curWeight > bestClusterWeight) {
                        bestClusterWeight = curWeight;
                        bestClusterWy = curWy;
                    }
                    curWeight = w;
                    curWy = w * static_cast<double>(py);
                }
                lastMatchY = py;
            }
        }
        if (curWeight > bestClusterWeight) {
            bestClusterWeight = curWeight;
            bestClusterWy = curWy;
        }

        if (bestClusterWeight <= 0.0) {
            // Fallback for thin 1px anti-aliased strokes blended over white background
            const double wDx = static_cast<double>(trR - 255);
            const double wDy = static_cast<double>(trG - 255);
            const double wDz = static_cast<double>(trB - 255);
            const double rayLenSq = wDx * wDx + wDy * wDy + wDz * wDz;
            if (rayLenSq > std::max(400.0, tolSq)) {
                double aaWeight = 0.0;
                double aaWy = 0.0;
                int lastAaY = -10;
                for (int py = yLo; py <= yHi; ++py) {
                    const QRgb rgb = reinterpret_cast<const QRgb *>(argb.constScanLine(py))[px];
                    if (qAlpha(rgb) < 16)
                        continue;
                    const double pxR = static_cast<double>(qRed(rgb) - 255);
                    const double pxG = static_cast<double>(qGreen(rgb) - 255);
                    const double pxB = static_cast<double>(qBlue(rgb) - 255);
                    const double alpha = (pxR * wDx + pxG * wDy + pxB * wDz) / rayLenSq;
                    if (alpha >= 0.35 && alpha <= 1.15) {
                        const double rx = pxR - alpha * wDx;
                        const double ry = pxG - alpha * wDy;
                        const double rz = pxB - alpha * wDz;
                        const double perpSq = rx * rx + ry * ry + rz * rz;
                        const double maxPerp = tol * std::min(1.0, alpha);
                        if (perpSq <= maxPerp * maxPerp) {
                            const double w = alpha;
                            if (py - lastAaY <= 2) {
                                aaWeight += w;
                                aaWy += w * static_cast<double>(py);
                            } else {
                                if (aaWeight > bestClusterWeight) {
                                    bestClusterWeight = aaWeight;
                                    bestClusterWy = aaWy;
                                }
                                aaWeight = w;
                                aaWy = w * static_cast<double>(py);
                            }
                            lastAaY = py;
                        }
                    }
                }
                if (aaWeight > bestClusterWeight) {
                    bestClusterWeight = aaWeight;
                    bestClusterWy = aaWy;
                }
            }
        }

        if (bestClusterWeight > 0.0) {
            const double centroidY = bestClusterWy / bestClusterWeight;
            out.append(mapPixelToData(QPointF(static_cast<double>(px), centroidY), cal));
        }
    }

    std::sort(out.begin(), out.end(),
              [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
    return out;
}

Table *GraphDigitizer::exportPointsToTable(ApplicationWindow *app,
                                           const QVector<QPointF> &dataPts,
                                           const QString &tableName,
                                           MultiLayer **outPlot)
{
    if (outPlot)
        *outPlot = nullptr;
    if (!app || dataPts.isEmpty())
        return nullptr;

    QString baseName = tableName.trimmed();
    if (baseName.isEmpty())
        baseName = QStringLiteral("DigitizedData");
    baseName.replace(' ', '_');

    QString uniqueName = baseName;
    if (app->alreadyUsedName(uniqueName))
        uniqueName = app->generateUniqueName(baseName, false);

    const int nRows = dataPts.size();
    Table *tbl = app->newTable(nRows, 2, uniqueName, QObject::tr("Digitized Curve Data"));
    if (!tbl)
        return nullptr;

    tbl->setHeader(QStringList() << QStringLiteral("X") << QStringLiteral("Y"));
    tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(0)->setPlotDesignation(SciDAVis::X);
    tbl->column(1)->setPlotDesignation(SciDAVis::Y);

    for (int i = 0; i < nRows; ++i) {
        tbl->column(0)->setValueAt(i, dataPts[i].x());
        tbl->column(1)->setValueAt(i, dataPts[i].y());
    }

    if (outPlot) {
        MultiLayer *ml = app->newGraph(
                app->generateUniqueName(QStringLiteral("DigitizedPlot_") + uniqueName, false));
        if (ml) {
            if (Graph *g = ml->activeGraph()) {
                if (g->insertCurve(tbl, tbl->colName(0), tbl->colName(1), Graph::LineSymbols, 0,
                                   nRows - 1)) {
                    const int cIdx = g->curves() - 1;
                    g->setCurvePen(cIdx, QPen(QColor(210, 35, 40), 2, Qt::SolidLine));
                    QwtSymbol sym(QwtSymbol::Ellipse, QBrush(QColor(210, 35, 40)),
                                  QPen(QColor(140, 20, 25), 1), QSize(6, 6));
                    g->setCurveSymbol(cIdx, sym);
                }
                g->setXAxisTitle(QObject::tr("X"));
                g->setYAxisTitle(QObject::tr("Y"));
                g->setTitle(QObject::tr("Digitized Curve (%1)").arg(uniqueName));
                if (g->legend())
                    g->legend()->setText(g->legendText());
                applyArialToDigitizedGraph(g);
                g->replot();
            }
            *outPlot = ml;
        }
    }

    return tbl;
}

void GraphDigitizer::showDigitizerDialog(ApplicationWindow *app, const QString &imagePath)
{
    if (!app)
        return;
    DigitizerDialog dlg(app, imagePath);
    dlg.exec();
}
