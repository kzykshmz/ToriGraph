#include "EmfPaintDevice.h"

#ifdef Q_OS_WIN

#include "MultiLayer.h"
#include "Graph.h"
#include "Plot.h"

#include <QPainter>
#include <QPainterPath>
#include <QTextItem>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace {

class WinGdiEmfPaintEngine : public QPaintEngine
{
public:
    WinGdiEmfPaintEngine(HDC hdc, QSize logicalPx, QSize targetPx, int dpiX, int dpiY)
        : QPaintEngine(QPaintEngine::PrimitiveTransform
                       | QPaintEngine::PainterPaths
                       | QPaintEngine::Antialiasing
                       | QPaintEngine::AlphaBlend
                       | QPaintEngine::ConstantOpacity
                       | QPaintEngine::PaintOutsidePaintEvent),
          m_hdc(hdc),
          m_logicalPx(logicalPx),
          m_targetPx(targetPx),
          m_dpiX(dpiX),
          m_dpiY(dpiY)
    {
        m_scaleX = (logicalPx.width() > 0) ? double(targetPx.width()) / double(logicalPx.width()) : 1.0;
        m_scaleY = (logicalPx.height() > 0) ? double(targetPx.height()) / double(logicalPx.height()) : 1.0;
    }

    bool begin(QPaintDevice *) override
    {
        if (!m_hdc)
            return false;
        m_hasClip = false;
        m_opacity = 1.0;
        SetBkMode(m_hdc, TRANSPARENT);
        return true;
    }

    bool end() override
    {
        if (m_hdc)
            SelectClipRgn(m_hdc, nullptr);
        m_hasClip = false;
        return true;
    }

    Type type() const override
    {
        return QPaintEngine::User;
    }

    POINT mapPoint(const QPointF &pt) const
    {
        QPointF p = m_transform.map(pt);
        return POINT{
            static_cast<LONG>(std::lround(p.x() * m_scaleX)),
            static_cast<LONG>(std::lround(p.y() * m_scaleY))
        };
    }

    RECT computeMappedPathBounds(const QPainterPath &path) const
    {
        const QRectF br = path.boundingRect();
        if (br.isEmpty())
            return RECT{ 0, 0, 0, 0 };
        const POINT c0 = mapPoint(br.topLeft());
        const POINT c1 = mapPoint(br.topRight());
        const POINT c2 = mapPoint(br.bottomRight());
        const POINT c3 = mapPoint(br.bottomLeft());
        LONG minX = std::min({ c0.x, c1.x, c2.x, c3.x }) - 1;
        LONG minY = std::min({ c0.y, c1.y, c2.y, c3.y }) - 1;
        LONG maxX = std::max({ c0.x, c1.x, c2.x, c3.x }) + 2;
        LONG maxY = std::max({ c0.y, c1.y, c2.y, c3.y }) + 2;
        minX = std::clamp(minX, 0L, static_cast<LONG>(m_targetPx.width()));
        minY = std::clamp(minY, 0L, static_cast<LONG>(m_targetPx.height()));
        maxX = std::clamp(maxX, 0L, static_cast<LONG>(m_targetPx.width()));
        maxY = std::clamp(maxY, 0L, static_cast<LONG>(m_targetPx.height()));
        return RECT{ minX, minY, maxX, maxY };
    }

    static COLORREF compositeColorOverWhite(const QColor &c, double opacity = 1.0)
    {
        const int a = std::clamp(static_cast<int>(std::lround(c.alpha() * opacity)), 0, 255);
        if (a >= 255)
            return RGB(c.red(), c.green(), c.blue());
        const int r = (c.red() * a + 255 * (255 - a) + 127) / 255;
        const int g = (c.green() * a + 255 * (255 - a) + 127) / 255;
        const int b = (c.blue() * a + 255 * (255 - a) + 127) / 255;
        return RGB(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
    }

    bool alphaBlendClippedPath(const QColor &c, int effectiveAlpha, const RECT &dstBox) const
    {
        const int a = std::clamp(effectiveAlpha, 0, 255);
        if (a <= 0 || a >= 255 || !m_hdc)
            return false;
        if (dstBox.right <= dstBox.left || dstBox.bottom <= dstBox.top)
            return false;

        constexpr int kTileDim = 8;
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = kTileDim;
        bmi.bmiHeader.biHeight = kTileDim; // positive bottom-up DIB for reliable EMF playback
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void *bits = nullptr;
        HDC memDc = CreateCompatibleDC(nullptr);
        if (!memDc)
            return false;
        HBITMAP hBmp = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hBmp || !bits) {
            if (hBmp)
                DeleteObject(hBmp);
            DeleteDC(memDc);
            return false;
        }

        auto *px = static_cast<unsigned char *>(bits);
        for (int i = 0; i < kTileDim * kTileDim; ++i) {
            px[i * 4 + 0] = static_cast<unsigned char>(c.blue());
            px[i * 4 + 1] = static_cast<unsigned char>(c.green());
            px[i * 4 + 2] = static_cast<unsigned char>(c.red());
            px[i * 4 + 3] = 255;
        }

        HGDIOBJ oldBmp = SelectObject(memDc, hBmp);
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = static_cast<BYTE>(a);
        bf.AlphaFormat = 0;

        const BOOL ok = GdiAlphaBlend(m_hdc, dstBox.left, dstBox.top,
                                      dstBox.right - dstBox.left,
                                      dstBox.bottom - dstBox.top,
                                      memDc, 0, 0, kTileDim, kTileDim, bf);
        SelectObject(memDc, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDc);
        return ok != FALSE;
    }

    HPEN createCurrentPen() const
    {
        if (m_pen.style() == Qt::NoPen || m_pen.color().alpha() == 0)
            return static_cast<HPEN>(GetStockObject(NULL_PEN));

        double w = m_pen.widthF();
        if (w <= 0.0)
            w = 1.0;
        if (!m_pen.isCosmetic()) {
            double tScale = std::hypot(m_transform.m11(), m_transform.m12());
            if (tScale > 0.0)
                w *= tScale;
        }
        DWORD gdiWidth = static_cast<DWORD>(std::max(1L, std::lround(w * (m_scaleX + m_scaleY) * 0.5)));

        DWORD penStyle = PS_SOLID;
        switch (m_pen.style()) {
        case Qt::DashLine:
            penStyle = PS_DASH;
            break;
        case Qt::DotLine:
            penStyle = PS_DOT;
            break;
        case Qt::DashDotLine:
            penStyle = PS_DASHDOT;
            break;
        case Qt::DashDotDotLine:
            penStyle = PS_DASHDOTDOT;
            break;
        default:
            penStyle = PS_SOLID;
            break;
        }

        DWORD capStyle = PS_ENDCAP_FLAT;
        if (m_pen.capStyle() == Qt::RoundCap)
            capStyle = PS_ENDCAP_ROUND;
        else if (m_pen.capStyle() == Qt::SquareCap)
            capStyle = PS_ENDCAP_SQUARE;

        DWORD joinStyle = PS_JOIN_MITER;
        if (m_pen.joinStyle() == Qt::RoundJoin)
            joinStyle = PS_JOIN_ROUND;
        else if (m_pen.joinStyle() == Qt::BevelJoin)
            joinStyle = PS_JOIN_BEVEL;

        QColor c = m_pen.color();
        LOGBRUSH lb{};
        lb.lbStyle = BS_SOLID;
        lb.lbColor = compositeColorOverWhite(c, m_opacity);
        lb.lbHatch = 0;

        HPEN hPen = ExtCreatePen(PS_GEOMETRIC | penStyle | capStyle | joinStyle, gdiWidth, &lb, 0, nullptr);
        if (!hPen)
            hPen = CreatePen(PS_SOLID, static_cast<int>(gdiWidth), lb.lbColor);
        return hPen;
    }

    HBRUSH createCurrentBrush() const
    {
        const int effAlpha = std::clamp(static_cast<int>(std::lround(m_brush.color().alpha() * m_opacity)), 0, 255);
        if (m_brush.style() == Qt::NoBrush || effAlpha <= 0)
            return static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        QColor c = m_brush.color();
        return CreateSolidBrush(compositeColorOverWhite(c, m_opacity));
    }

    static void deletePenIfCustom(HPEN pen)
    {
        if (pen && pen != static_cast<HPEN>(GetStockObject(NULL_PEN)))
            DeleteObject(pen);
    }

    static void deleteBrushIfCustom(HBRUSH brush)
    {
        if (brush && brush != static_cast<HBRUSH>(GetStockObject(NULL_BRUSH)))
            DeleteObject(brush);
    }

    void emitPathToGdi(const QPainterPath &path, bool closeFigures = false) const
    {
        BeginPath(m_hdc);
        const int count = path.elementCount();
        bool hasSubpath = false;
        for (int i = 0; i < count; ++i) {
            const QPainterPath::Element &el = path.elementAt(i);
            if (el.type == QPainterPath::MoveToElement) {
                if (closeFigures && hasSubpath)
                    CloseFigure(m_hdc);
                POINT p = mapPoint(QPointF(el.x, el.y));
                MoveToEx(m_hdc, p.x, p.y, nullptr);
                hasSubpath = true;
            } else if (el.type == QPainterPath::LineToElement) {
                POINT p = mapPoint(QPointF(el.x, el.y));
                LineTo(m_hdc, p.x, p.y);
            } else if (el.type == QPainterPath::CurveToElement && i + 2 < count) {
                const QPainterPath::Element &d1 = path.elementAt(i + 1);
                const QPainterPath::Element &d2 = path.elementAt(i + 2);
                POINT pts[3] = {
                    mapPoint(QPointF(el.x, el.y)),
                    mapPoint(QPointF(d1.x, d1.y)),
                    mapPoint(QPointF(d2.x, d2.y))
                };
                PolyBezierTo(m_hdc, pts, 3);
                i += 2;
            }
        }
        if (closeFigures && hasSubpath)
            CloseFigure(m_hdc);
        EndPath(m_hdc);
    }

    void updateState(const QPaintEngineState &state) override
    {
        DirtyFlags flags = state.state();
        if (flags & DirtyPen)
            m_pen = state.pen();
        if (flags & DirtyBrush)
            m_brush = state.brush();
        if (flags & DirtyFont)
            m_font = state.font();
        if (flags & DirtyTransform)
            m_transform = state.transform();
        if (flags & DirtyOpacity)
            m_opacity = state.opacity();

        if (flags & (DirtyClipEnabled | DirtyClipPath | DirtyClipRegion)) {
            if (!painter() || !painter()->hasClipping()) {
                SelectClipRgn(m_hdc, nullptr);
                m_hasClip = false;
            } else {
                QTransform savedT = m_transform;
                m_transform = painter()->transform();
                QPainterPath clip = painter()->clipPath();
                SelectClipRgn(m_hdc, nullptr);
                m_hasClip = false;
                if (!clip.isEmpty()) {
                    SetPolyFillMode(m_hdc, clip.fillRule() == Qt::WindingFill ? WINDING : ALTERNATE);
                    emitPathToGdi(clip, true);
                    if (SelectClipPath(m_hdc, RGN_COPY))
                        m_hasClip = true;
                }
                m_transform = savedT;
            }
        }
    }

    void drawPath(const QPainterPath &path) override
    {
        if (path.isEmpty() || m_opacity <= 0.0)
            return;
        const int brushAlpha = std::clamp(static_cast<int>(std::lround(m_brush.color().alpha() * m_opacity)), 0, 255);
        const int penAlpha = std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        const bool hasBrush = (m_brush.style() != Qt::NoBrush && brushAlpha > 0);
        const bool hasPen = (m_pen.style() != Qt::NoPen && penAlpha > 0);
        if (!hasBrush && !hasPen)
            return;

        SetPolyFillMode(m_hdc, path.fillRule() == Qt::WindingFill ? WINDING : ALTERNATE);
        const bool translucentBrush = hasBrush && brushAlpha < 255;

        if (translucentBrush) {
            bool blended = false;
            const RECT dstBox = computeMappedPathBounds(path);
            const int clipMode = m_hasClip ? RGN_AND : RGN_COPY;
            const int savedDc = SaveDC(m_hdc);
            if (savedDc != 0) {
                SetPolyFillMode(m_hdc, path.fillRule() == Qt::WindingFill ? WINDING : ALTERNATE);
                emitPathToGdi(path, true);
                if (SelectClipPath(m_hdc, clipMode))
                    blended = alphaBlendClippedPath(m_brush.color(), brushAlpha, dstBox);
                RestoreDC(m_hdc, savedDc);
            }
            if (!blended) {
                HBRUSH brush = createCurrentBrush();
                HGDIOBJ oldBrush = SelectObject(m_hdc, brush);
                emitPathToGdi(path, true);
                FillPath(m_hdc);
                SelectObject(m_hdc, oldBrush);
                deleteBrushIfCustom(brush);
            }
            if (hasPen) {
                HPEN pen = createCurrentPen();
                HGDIOBJ oldPen = SelectObject(m_hdc, pen);
                emitPathToGdi(path, false);
                StrokePath(m_hdc);
                SelectObject(m_hdc, oldPen);
                deletePenIfCustom(pen);
            }
            return;
        }

        HPEN pen = createCurrentPen();
        HBRUSH brush = createCurrentBrush();
        HGDIOBJ oldPen = SelectObject(m_hdc, pen);
        HGDIOBJ oldBrush = SelectObject(m_hdc, brush);

        if (hasBrush && hasPen) {
            emitPathToGdi(path);
            StrokeAndFillPath(m_hdc);
        } else if (hasBrush) {
            emitPathToGdi(path);
            FillPath(m_hdc);
        } else if (hasPen) {
            emitPathToGdi(path);
            StrokePath(m_hdc);
        }

        SelectObject(m_hdc, oldBrush);
        SelectObject(m_hdc, oldPen);
        deleteBrushIfCustom(brush);
        deletePenIfCustom(pen);
    }

    void drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode) override
    {
        if (!points || pointCount < 2 || m_opacity <= 0.0)
            return;
        const int brushAlpha = std::clamp(static_cast<int>(std::lround(m_brush.color().alpha() * m_opacity)), 0, 255);
        if (mode != PolylineMode && m_brush.style() != Qt::NoBrush
            && brushAlpha > 0 && brushAlpha < 255) {
            QPainterPath path;
            path.setFillRule(mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
            path.moveTo(points[0]);
            for (int i = 1; i < pointCount; ++i)
                path.lineTo(points[i]);
            path.closeSubpath();
            drawPath(path);
            return;
        }

        std::vector<POINT> pts(static_cast<size_t>(pointCount));
        for (int i = 0; i < pointCount; ++i)
            pts[static_cast<size_t>(i)] = mapPoint(points[i]);

        HPEN pen = createCurrentPen();
        HGDIOBJ oldPen = SelectObject(m_hdc, pen);

        if (mode == PolylineMode) {
            Polyline(m_hdc, pts.data(), pointCount);
        } else {
            SetPolyFillMode(m_hdc, mode == WindingMode ? WINDING : ALTERNATE);
            HBRUSH brush = createCurrentBrush();
            HGDIOBJ oldBrush = SelectObject(m_hdc, brush);
            Polygon(m_hdc, pts.data(), pointCount);
            SelectObject(m_hdc, oldBrush);
            deleteBrushIfCustom(brush);
        }

        SelectObject(m_hdc, oldPen);
        deletePenIfCustom(pen);
    }

    void drawPolygon(const QPoint *points, int pointCount, PolygonDrawMode mode) override
    {
        if (!points || pointCount < 2)
            return;
        std::vector<QPointF> pts(static_cast<size_t>(pointCount));
        for (int i = 0; i < pointCount; ++i)
            pts[static_cast<size_t>(i)] = QPointF(points[i]);
        drawPolygon(pts.data(), pointCount, mode);
    }

    void drawLines(const QLineF *lines, int lineCount) override
    {
        if (!lines || lineCount <= 0)
            return;
        if (m_pen.style() == Qt::NoPen || m_pen.color().alpha() == 0)
            return;

        HPEN pen = createCurrentPen();
        HGDIOBJ oldPen = SelectObject(m_hdc, pen);
        for (int i = 0; i < lineCount; ++i) {
            POINT pts[2] = { mapPoint(lines[i].p1()), mapPoint(lines[i].p2()) };
            Polyline(m_hdc, pts, 2);
        }
        SelectObject(m_hdc, oldPen);
        deletePenIfCustom(pen);
    }

    void drawLines(const QLine *lines, int lineCount) override
    {
        if (!lines || lineCount <= 0)
            return;
        std::vector<QLineF> lf(static_cast<size_t>(lineCount));
        for (int i = 0; i < lineCount; ++i)
            lf[static_cast<size_t>(i)] = QLineF(lines[i]);
        drawLines(lf.data(), lineCount);
    }

    void drawRects(const QRectF *rects, int rectCount) override
    {
        if (!rects || rectCount <= 0)
            return;
        for (int i = 0; i < rectCount; ++i) {
            const QRectF &r = rects[i];
            QPointF poly[4] = {
                r.topLeft(),
                r.topRight(),
                r.bottomRight(),
                r.bottomLeft()
            };
            drawPolygon(poly, 4, OddEvenMode);
        }
    }

    void drawRects(const QRect *rects, int rectCount) override
    {
        if (!rects || rectCount <= 0)
            return;
        for (int i = 0; i < rectCount; ++i) {
            const QRect &r = rects[i];
            // In Qt, QRect right() is left() + width() - 1; for fill/stroke consistency use QRectF(r)
            QRectF rf(r.left(), r.top(), r.width(), r.height());
            drawRects(&rf, 1);
        }
    }

    void drawEllipse(const QRectF &rect) override
    {
        QPainterPath p;
        p.addEllipse(rect);
        drawPath(p);
    }

    void drawEllipse(const QRect &rect) override
    {
        drawEllipse(QRectF(rect));
    }

    void drawPoints(const QPointF *points, int pointCount) override
    {
        if (!points || pointCount <= 0)
            return;
        if (m_pen.style() == Qt::NoPen || m_pen.color().alpha() == 0)
            return;
        QColor c = m_pen.color();
        COLORREF cr = compositeColorOverWhite(c);
        HBRUSH br = CreateSolidBrush(cr);
        for (int i = 0; i < pointCount; ++i) {
            POINT p = mapPoint(points[i]);
            RECT rc{ p.x - 1, p.y - 1, p.x + 2, p.y + 2 };
            FillRect(m_hdc, &rc, br);
        }
        DeleteObject(br);
    }

    void drawPoints(const QPoint *points, int pointCount) override
    {
        if (!points || pointCount <= 0)
            return;
        std::vector<QPointF> pf(static_cast<size_t>(pointCount));
        for (int i = 0; i < pointCount; ++i)
            pf[static_cast<size_t>(i)] = QPointF(points[i]);
        drawPoints(pf.data(), pointCount);
    }

    void drawTextItem(const QPointF &p, const QTextItem &textItem) override
    {
        QString text = textItem.text();
        if (text.isEmpty())
            return;

        QPointF p0 = m_transform.map(p);
        QPointF px = m_transform.map(p + QPointF(1.0, 0.0)) - p0;
        QPointF py = m_transform.map(p + QPointF(0.0, 1.0)) - p0;

        double dx = px.x() * m_scaleX;
        double dy = px.y() * m_scaleY;
        double angleDeg = std::atan2(-dy, dx) * (180.0 / 3.14159265358979323846);
        LONG escapement = static_cast<LONG>(std::lround(angleDeg * 10.0));

        double vScale = std::hypot(py.x() * m_scaleX, py.y() * m_scaleY);
        if (vScale <= 0.0)
            vScale = m_scaleY;

        QFont f = textItem.font();
        double logicalPxHeight = 12.0;
        if (f.pixelSize() > 0) {
            logicalPxHeight = double(f.pixelSize());
        } else if (f.pointSizeF() > 0.0) {
            logicalPxHeight = f.pointSizeF() * double(m_dpiY) / 72.0;
        }

        LOGFONTW lf{};
        lf.lfHeight = -static_cast<LONG>(std::max(1L, std::lround(logicalPxHeight * vScale)));
        lf.lfEscapement = escapement;
        lf.lfOrientation = escapement;
        lf.lfWeight = f.bold() ? FW_BOLD : FW_NORMAL;
        lf.lfItalic = f.italic() ? TRUE : FALSE;
        lf.lfUnderline = f.underline() ? TRUE : FALSE;
        lf.lfStrikeOut = f.strikeOut() ? TRUE : FALSE;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_TT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = ANTIALIASED_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

        std::wstring family = f.family().toStdWString();
        if (family.empty())
            family = L"Arial";
        wcsncpy_s(lf.lfFaceName, family.c_str(), _TRUNCATE);

        HFONT hFont = CreateFontIndirectW(&lf);
        HGDIOBJ oldFont = SelectObject(m_hdc, hFont);

        QColor tc = m_pen.color();
        SetTextColor(m_hdc, compositeColorOverWhite(tc));
        SetBkMode(m_hdc, TRANSPARENT);
        SetTextAlign(m_hdc, TA_LEFT | TA_BASELINE);

        POINT gp = mapPoint(p);
        std::wstring ws = text.toStdWString();
        ExtTextOutW(m_hdc, gp.x, gp.y, 0, nullptr, ws.c_str(), static_cast<UINT>(ws.size()), nullptr);

        SelectObject(m_hdc, oldFont);
        if (hFont)
            DeleteObject(hFont);
    }

    void drawImage(const QRectF &r, const QImage &pm, const QRectF &sr, Qt::ImageConversionFlags) override
    {
        if (pm.isNull() || r.isEmpty())
            return;
        QImage sub = sr.isValid() && !sr.isEmpty() ? pm.copy(sr.toRect()) : pm;
        if (sub.isNull())
            return;

        // Composite onto white if image has alpha
        QImage rgbImg(sub.size(), QImage::Format_RGB32);
        rgbImg.fill(Qt::white);
        {
            QPainter ip(&rgbImg);
            ip.drawImage(0, 0, sub);
        }

        POINT pTL = mapPoint(r.topLeft());
        POINT pBR = mapPoint(r.bottomRight());
        int dstX = std::min(pTL.x, pBR.x);
        int dstY = std::min(pTL.y, pBR.y);
        int dstW = std::max(1L, std::abs(pBR.x - pTL.x));
        int dstH = std::max(1L, std::abs(pBR.y - pTL.y));

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = rgbImg.width();
        bmi.bmiHeader.biHeight = -rgbImg.height(); // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        SetStretchBltMode(m_hdc, HALFTONE);
        StretchDIBits(m_hdc,
                      dstX, dstY, dstW, dstH,
                      0, 0, rgbImg.width(), rgbImg.height(),
                      rgbImg.constBits(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    }

    void drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr) override
    {
        if (pm.isNull())
            return;
        drawImage(r, pm.toImage(), sr, Qt::AutoColor);
    }

private:
    HDC m_hdc = nullptr;
    QSize m_logicalPx;
    QSize m_targetPx;
    int m_dpiX = 96;
    int m_dpiY = 96;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    QPen m_pen;
    QBrush m_brush;
    QFont m_font;
    QTransform m_transform;
    bool m_hasClip = false;
    double m_opacity = 1.0;
};

class WinGdiEmfPaintDevice : public QPaintDevice
{
public:
    WinGdiEmfPaintDevice(HDC hdc, QSize logicalPx, QSize targetPx, int dpiX, int dpiY)
        : m_logicalPx(logicalPx),
          m_dpiX(dpiX > 0 ? dpiX : 96),
          m_dpiY(dpiY > 0 ? dpiY : 96),
          m_engine(new WinGdiEmfPaintEngine(hdc, logicalPx, targetPx, m_dpiX, m_dpiY))
    {
    }

    ~WinGdiEmfPaintDevice() override
    {
        delete m_engine;
    }

    QPaintEngine *paintEngine() const override
    {
        return m_engine;
    }

protected:
    int metric(PaintDeviceMetric m) const override
    {
        switch (m) {
        case PdmWidth:
            return m_logicalPx.width();
        case PdmHeight:
            return m_logicalPx.height();
        case PdmWidthMM:
            return std::max(1, static_cast<int>(std::lround(m_logicalPx.width() * 25.4 / m_dpiX)));
        case PdmHeightMM:
            return std::max(1, static_cast<int>(std::lround(m_logicalPx.height() * 25.4 / m_dpiY)));
        case PdmDpiX:
        case PdmPhysicalDpiX:
            return m_dpiX;
        case PdmDpiY:
        case PdmPhysicalDpiY:
            return m_dpiY;
        case PdmDepth:
            return 32;
        case PdmNumColors:
            return 16777216;
        case PdmDevicePixelRatio:
            return 1;
        case PdmDevicePixelRatioScaled:
            return static_cast<int>(1 * devicePixelRatioFScale());
        default:
            return QPaintDevice::metric(m);
        }
    }

private:
    QSize m_logicalPx;
    int m_dpiX;
    int m_dpiY;
    WinGdiEmfPaintEngine *m_engine;
};

} // namespace

namespace SciDAVisEmf {

QByteArray hemfToBytes(HENHMETAFILE hemf)
{
    if (!hemf)
        return {};
    UINT sz = GetEnhMetaFileBits(hemf, 0, nullptr);
    if (sz == 0)
        return {};
    QByteArray bytes(static_cast<int>(sz), '\0');
    if (GetEnhMetaFileBits(hemf, sz, reinterpret_cast<LPBYTE>(bytes.data())) != sz)
        return {};
    return bytes;
}

HENHMETAFILE createHemfFromBytes(const QByteArray &emfBytes)
{
    if (emfBytes.isEmpty())
        return nullptr;
    return SetEnhMetaFileBits(static_cast<UINT>(emfBytes.size()),
                              reinterpret_cast<const BYTE *>(emfBytes.constData()));
}

QByteArray imageToDibBytes(const QImage &src)
{
    if (src.isNull())
        return {};
    QImage rgb(src.size(), QImage::Format_RGB32);
    rgb.fill(Qt::white);
    {
        QPainter p(&rgb);
        p.drawImage(0, 0, src);
    }

    const int w = rgb.width();
    const int h = rgb.height();
    const int stride = w * 4;
    const int imageSize = stride * h;

    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = w;
    bih.biHeight = h; // bottom-up for standard CF_DIB compatibility
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = static_cast<DWORD>(imageSize);
    bih.biXPelsPerMeter = 3780;
    bih.biYPelsPerMeter = 3780;

    QByteArray dib(static_cast<int>(sizeof(BITMAPINFOHEADER)) + imageSize, '\0');
    std::memcpy(dib.data(), &bih, sizeof(BITMAPINFOHEADER));
    char *dst = dib.data() + sizeof(BITMAPINFOHEADER);
    for (int y = 0; y < h; ++y) {
        const uchar *scan = rgb.constScanLine(h - 1 - y);
        std::memcpy(dst + y * stride, scan, static_cast<size_t>(stride));
    }
    return dib;
}

OleRenderedGraph renderFromImage(const QImage &img)
{
    OleRenderedGraph out;
    QImage valid = img;
    if (valid.isNull() || valid.width() <= 0 || valid.height() <= 0) {
        valid = QImage(500, 375, QImage::Format_RGB32);
        valid.fill(Qt::white);
    }
    out.pixelSize = valid.size();
    out.dibBytes = imageToDibBytes(valid);

    const LONG widthHm = std::max(1000L, static_cast<LONG>(std::lround(valid.width() * 2540.0 / 96.0)));
    const LONG heightHm = std::max(1000L, static_cast<LONG>(std::lround(valid.height() * 2540.0 / 96.0)));
    out.extentHm = SIZEL{ widthHm, heightHm };

    HDC ref = GetDC(nullptr);
    if (!ref)
        return out;
    const int devW = GetDeviceCaps(ref, HORZRES);
    const int devH = GetDeviceCaps(ref, VERTRES);
    const int mmW = GetDeviceCaps(ref, HORZSIZE);
    const int mmH = GetDeviceCaps(ref, VERTSIZE);

    LONG targetW = std::max(1L, static_cast<LONG>(std::lround(double(widthHm) * devW / (100.0 * mmW))));
    LONG targetH = std::max(1L, static_cast<LONG>(std::lround(double(heightHm) * devH / (100.0 * mmH))));

    RECT frame{ 0, 0, widthHm, heightHm };
    HDC dc = CreateEnhMetaFileW(ref, nullptr, &frame, L"ToriGraph\0Graph\0\0");
    ReleaseDC(nullptr, ref);
    if (!dc)
        return out;

    {
        WinGdiEmfPaintDevice emfDev(dc, valid.size(), QSize(targetW, targetH), 96, 96);
        QPainter painter(&emfDev);
        painter.drawImage(QRect(0, 0, valid.width(), valid.height()), valid);
        painter.end();
    }

    HENHMETAFILE hemf = CloseEnhMetaFile(dc);
    if (hemf) {
        out.emfBytes = hemfToBytes(hemf);
        DeleteEnhMetaFile(hemf);
    }
    return out;
}

OleRenderedGraph renderMultiLayer(MultiLayer *ml, Graph *singleLayer)
{
    if (!ml && !singleLayer)
        return renderFromImage(QImage());

    QRect mlLayersRect;
    if (!singleLayer && ml) {
        if (ml->pageUnit() != MultiLayer::UnitPx && ml->canvasWidget()
            && !ml->canvasWidget()->rect().isEmpty()) {
            mlLayersRect = ml->canvasWidget()->rect();
        } else {
            QWidgetList gl = ml->graphPtrs();
            for (int i = 0; i < gl.count(); ++i) {
                Graph *gr = qobject_cast<Graph *>(gl.at(i));
                if (gr && gr->plotWidget())
                    mlLayersRect = mlLayersRect.united(QRect(gr->pos(), gr->plotWidget()->size()));
            }
            if (!mlLayersRect.isEmpty()) {
                mlLayersRect.adjust(-ml->leftMargin(), -ml->topMargin(), ml->rightMargin(),
                                    ml->bottomMargin());
            }
        }
    }

    QSize pxSize = singleLayer && singleLayer->plotWidget()
            ? singleLayer->plotWidget()->size()
            : (!mlLayersRect.isEmpty()
                       ? mlLayersRect.size()
                       : (ml && ml->canvasWidget() ? ml->canvasWidget()->size() : QSize(500, 500)));
    if (pxSize.width() < 50 || pxSize.height() < 50)
        pxSize = QSize(500, 500);

    const QWidget *dpiWidget = ml ? static_cast<const QWidget *>(ml)
                                  : static_cast<const QWidget *>(singleLayer);
    const int dpiX = (dpiWidget && dpiWidget->logicalDpiX() > 0) ? dpiWidget->logicalDpiX() : 96;
    const int dpiY = (dpiWidget && dpiWidget->logicalDpiY() > 0) ? dpiWidget->logicalDpiY() : 96;
    const double zoom = ml ? std::max(0.1, ml->pageZoom()) : 1.0;

    // 1. Render raster QImage for CF_DIB compatibility
    QImage raster(pxSize, QImage::Format_ARGB32);
    raster.fill(Qt::white);
    {
        QPainter rp(&raster);
        if (singleLayer)
            singleLayer->exportPainter(rp, false, QRect(QPoint(0, 0), pxSize), pxSize);
        else if (ml)
            ml->exportPainter(rp, false, mlLayersRect, pxSize);
        rp.end();
    }

    OleRenderedGraph out;
    out.pixelSize = pxSize;
    out.dibBytes = imageToDibBytes(raster);

    LONG widthHm = 15000L;
    LONG heightHm = 11250L;
    if (!singleLayer && ml && ml->pageUnit() != MultiLayer::UnitPx) {
        auto toHm = [](double val, MultiLayer::PageUnit u) -> LONG {
            double hm = val * 100.0;
            switch (u) {
            case MultiLayer::UnitMm:
                hm = val * 100.0;
                break;
            case MultiLayer::UnitCm:
                hm = val * 1000.0;
                break;
            case MultiLayer::UnitInch:
                hm = val * 2540.0;
                break;
            case MultiLayer::UnitPx:
            default:
                hm = (val / 96.0) * 2540.0;
                break;
            }
            return std::max(1000L, static_cast<LONG>(std::lround(hm)));
        };
        widthHm = toHm(ml->pageWidthPhysical(), ml->pageUnit());
        heightHm = toHm(ml->pageHeightPhysical(), ml->pageUnit());
    } else {
        const double unzoomedW = double(pxSize.width()) / zoom;
        const double unzoomedH = double(pxSize.height()) / zoom;
        widthHm = std::max(1000L, static_cast<LONG>(std::lround(unzoomedW * 2540.0 / dpiX)));
        heightHm = std::max(1000L, static_cast<LONG>(std::lround(unzoomedH * 2540.0 / dpiY)));
    }
    out.extentHm = SIZEL{ widthHm, heightHm };

    HDC ref = GetDC(nullptr);
    if (!ref)
        return out;
    const int devW = GetDeviceCaps(ref, HORZRES);
    const int devH = GetDeviceCaps(ref, VERTRES);
    const int mmW = GetDeviceCaps(ref, HORZSIZE);
    const int mmH = GetDeviceCaps(ref, VERTSIZE);

    LONG targetW = std::max(1L, static_cast<LONG>(std::lround(double(widthHm) * devW / (100.0 * mmW))));
    LONG targetH = std::max(1L, static_cast<LONG>(std::lround(double(heightHm) * devH / (100.0 * mmH))));

    RECT frame{ 0, 0, widthHm, heightHm };
    HDC dc = CreateEnhMetaFileW(ref, nullptr, &frame, L"ToriGraph\0Graph\0\0");
    ReleaseDC(nullptr, ref);
    if (!dc)
        return out;

    {
        const int emfLogicalDpiX = std::max(36, static_cast<int>(std::lround(dpiX * zoom)));
        const int emfLogicalDpiY = std::max(36, static_cast<int>(std::lround(dpiY * zoom)));
        WinGdiEmfPaintDevice emfDev(dc, pxSize, QSize(targetW, targetH), emfLogicalDpiX, emfLogicalDpiY);
        QPainter vp(&emfDev);
        if (singleLayer) {
            vp.fillRect(QRect(QPoint(0, 0), pxSize), Qt::white);
            singleLayer->exportPainter(vp, false, QRect(QPoint(0, 0), pxSize), pxSize);
        } else if (ml) {
            ml->exportPainter(vp, false, mlLayersRect, pxSize);
        }
        vp.end();
    }

    HENHMETAFILE hemf = CloseEnhMetaFile(dc);
    if (hemf) {
        out.emfBytes = hemfToBytes(hemf);
        DeleteEnhMetaFile(hemf);
    }
    if (out.emfBytes.isEmpty()) {
        return renderFromImage(raster);
    }
    return out;
}

} // namespace SciDAVisEmf

#endif // Q_OS_WIN