#include "EmfPaintDevice.h"

#ifdef Q_OS_WIN

#include "MultiLayer.h"
#include "Graph.h"
#include "Plot.h"

#include <QPainter>
#include <QPainterPath>
#include <QSizeF>
#include <QTextItem>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

using std::max;
using std::min;
#include <objidl.h>
#include <gdiplus.h>

namespace {

bool ensureGdiplusStarted()
{
    static std::once_flag s_once;
    static bool s_ok = false;
    static ULONG_PTR s_token = 0;
    std::call_once(s_once, []() {
        Gdiplus::GdiplusStartupInput input;
        s_ok = (Gdiplus::GdiplusStartup(&s_token, &input, nullptr) == Gdiplus::Ok);
    });
    return s_ok;
}

class WinGdiEmfPaintEngine : public QPaintEngine
{
public:
    WinGdiEmfPaintEngine(Gdiplus::Graphics *gfx, QSize logicalPx, QSizeF targetPxF, int dpiX,
                         int dpiY)
        : QPaintEngine(QPaintEngine::PrimitiveTransform | QPaintEngine::PainterPaths
                       | QPaintEngine::Antialiasing | QPaintEngine::AlphaBlend
                       | QPaintEngine::ConstantOpacity | QPaintEngine::PaintOutsidePaintEvent),
          m_gfx(gfx),
          m_logicalPx(logicalPx),
          m_targetPxF(targetPxF),
          m_dpiX(dpiX),
          m_dpiY(dpiY)
    {
        m_scaleX = (logicalPx.width() > 0) ? (targetPxF.width() / double(logicalPx.width())) : 1.0;
        m_scaleY =
                (logicalPx.height() > 0) ? (targetPxF.height() / double(logicalPx.height())) : 1.0;
    }

    bool begin(QPaintDevice *) override
    {
        if (!m_gfx)
            return false;
        m_hasClip = false;
        m_deviceClipPath = QPainterPath();
        m_opacity = 1.0;
        restoreGdiplusState();
        return true;
    }

    bool end() override
    {
        if (m_gfx)
            m_gfx->ResetClip();
        m_hasClip = false;
        m_deviceClipPath = QPainterPath();
        return true;
    }

    Type type() const override { return QPaintEngine::User; }

    Gdiplus::PointF mapPointF(const QPointF &pt) const
    {
        const QPointF p = m_transform.map(pt);
        return Gdiplus::PointF(static_cast<Gdiplus::REAL>(p.x() * m_scaleX),
                               static_cast<Gdiplus::REAL>(p.y() * m_scaleY));
    }

    QPainterPath mapToDevicePath(const QPainterPath &path, const QTransform &xf) const
    {
        QTransform devXf = xf * QTransform::fromScale(m_scaleX, m_scaleY);
        QPainterPath mapped = devXf.map(path);
        mapped.setFillRule(path.fillRule());
        return mapped;
    }

    static void buildGdiplusPath(const QPainterPath &devPath, Gdiplus::GraphicsPath &outPath,
                                 bool forceCloseSubpaths = false)
    {
        outPath.Reset();
        outPath.SetFillMode(devPath.fillRule() == Qt::WindingFill ? Gdiplus::FillModeWinding
                                                                  : Gdiplus::FillModeAlternate);
        const int count = devPath.elementCount();
        bool hasSubpath = false;
        int subpathSegs = 0;
        Gdiplus::PointF subpathStart(0.0f, 0.0f);
        Gdiplus::PointF curPt(0.0f, 0.0f);

        auto finishSubpath = [&]() {
            if (!hasSubpath)
                return;
            const float dx = curPt.X - subpathStart.X;
            const float dy = curPt.Y - subpathStart.Y;
            if (forceCloseSubpaths || (subpathSegs >= 2 && (dx * dx + dy * dy) <= 1e-4f))
                outPath.CloseFigure();
        };

        for (int i = 0; i < count; ++i) {
            const QPainterPath::Element &el = devPath.elementAt(i);
            if (el.type == QPainterPath::MoveToElement) {
                finishSubpath();
                if (hasSubpath)
                    outPath.StartFigure();
                curPt = Gdiplus::PointF(static_cast<Gdiplus::REAL>(el.x),
                                        static_cast<Gdiplus::REAL>(el.y));
                subpathStart = curPt;
                hasSubpath = true;
                subpathSegs = 0;
            } else if (el.type == QPainterPath::LineToElement) {
                if (!hasSubpath) {
                    subpathStart = curPt;
                    hasSubpath = true;
                }
                const Gdiplus::PointF nextPt(static_cast<Gdiplus::REAL>(el.x),
                                             static_cast<Gdiplus::REAL>(el.y));
                outPath.AddLine(curPt, nextPt);
                curPt = nextPt;
                ++subpathSegs;
            } else if (el.type == QPainterPath::CurveToElement && i + 2 < count) {
                if (!hasSubpath) {
                    subpathStart = curPt;
                    hasSubpath = true;
                }
                const QPainterPath::Element &d1 = devPath.elementAt(i + 1);
                const QPainterPath::Element &d2 = devPath.elementAt(i + 2);
                const Gdiplus::PointF c1(static_cast<Gdiplus::REAL>(el.x),
                                         static_cast<Gdiplus::REAL>(el.y));
                const Gdiplus::PointF c2(static_cast<Gdiplus::REAL>(d1.x),
                                         static_cast<Gdiplus::REAL>(d1.y));
                const Gdiplus::PointF endPt(static_cast<Gdiplus::REAL>(d2.x),
                                            static_cast<Gdiplus::REAL>(d2.y));
                outPath.AddBezier(curPt, c1, c2, endPt);
                curPt = endPt;
                ++subpathSegs;
                i += 2;
            }
        }
        finishSubpath();
    }

    void restoreGdiplusState() const
    {
        if (!m_gfx)
            return;
        m_gfx->SetPageUnit(Gdiplus::UnitPixel);
        m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        m_gfx->SetPixelOffsetMode(Gdiplus::PixelOffsetModeDefault);
        m_gfx->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        m_gfx->SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        m_gfx->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

        if (m_hasClip && !m_deviceClipPath.isEmpty()) {
            Gdiplus::GraphicsPath clipGp;
            buildGdiplusPath(m_deviceClipPath, clipGp, true);
            m_gfx->SetClip(&clipGp, Gdiplus::CombineModeReplace);
        } else {
            m_gfx->ResetClip();
        }
    }

    double computeDevicePenWidth() const
    {
        double w = m_pen.widthF();
        if (w <= 0.0)
            w = 1.0;
        if (!m_pen.isCosmetic()) {
            const double tScale = std::hypot(m_transform.m11(), m_transform.m12());
            if (tScale > 0.0)
                w *= tScale;
        }
        return std::max(0.5, w * 0.5 * (m_scaleX + m_scaleY));
    }

    void configureGdiplusPen(Gdiplus::Pen &pen, int alpha = 255) const
    {
        const QColor c = m_pen.color();
        const BYTE a = static_cast<BYTE>(std::clamp(alpha, 0, 255));
        pen.SetColor(Gdiplus::Color(a, c.red(), c.green(), c.blue()));
        pen.SetWidth(static_cast<Gdiplus::REAL>(computeDevicePenWidth()));

        Gdiplus::LineCap cap = Gdiplus::LineCapFlat;
        Gdiplus::DashCap dashCap = Gdiplus::DashCapFlat;
        if (m_pen.capStyle() == Qt::RoundCap) {
            cap = Gdiplus::LineCapRound;
            dashCap = Gdiplus::DashCapRound;
        } else if (m_pen.capStyle() == Qt::SquareCap) {
            cap = Gdiplus::LineCapSquare;
        }
        pen.SetLineCap(cap, cap, dashCap);

        if (m_pen.joinStyle() == Qt::RoundJoin) {
            pen.SetLineJoin(Gdiplus::LineJoinRound);
        } else if (m_pen.joinStyle() == Qt::BevelJoin) {
            pen.SetLineJoin(Gdiplus::LineJoinBevel);
        } else {
            pen.SetLineJoin(Gdiplus::LineJoinMiterClipped);
            pen.SetMiterLimit(static_cast<Gdiplus::REAL>(std::max(1.0, m_pen.miterLimit())));
        }

        switch (m_pen.style()) {
        case Qt::DashLine:
            pen.SetDashStyle(Gdiplus::DashStyleDash);
            break;
        case Qt::DotLine:
            pen.SetDashStyle(Gdiplus::DashStyleDot);
            break;
        case Qt::DashDotLine:
            pen.SetDashStyle(Gdiplus::DashStyleDashDot);
            break;
        case Qt::DashDotDotLine:
            pen.SetDashStyle(Gdiplus::DashStyleDashDotDot);
            break;
        case Qt::CustomDashLine: {
            const QVector<qreal> pat = m_pen.dashPattern();
            if (!pat.isEmpty()) {
                std::vector<Gdiplus::REAL> dashes;
                dashes.reserve(static_cast<size_t>(pat.size()) * 2);
                for (qreal seg : pat)
                    dashes.push_back(static_cast<Gdiplus::REAL>(std::max(0.1, double(seg))));
                if (dashes.size() % 2 == 1) {
                    const size_t n = dashes.size();
                    for (size_t k = 0; k < n; ++k)
                        dashes.push_back(dashes[k]);
                }
                pen.SetDashPattern(dashes.data(), static_cast<INT>(dashes.size()));
                pen.SetDashOffset(static_cast<Gdiplus::REAL>(m_pen.dashOffset()));
            } else {
                pen.SetDashStyle(Gdiplus::DashStyleSolid);
            }
            break;
        }
        default:
            pen.SetDashStyle(Gdiplus::DashStyleSolid);
            break;
        }
    }

    std::unique_ptr<Gdiplus::Brush> createGdiplusBrush(int alpha = 255) const
    {
        const QColor c = m_brush.color();
        const Gdiplus::Color fg(static_cast<BYTE>(std::clamp(alpha, 0, 255)), c.red(), c.green(),
                                c.blue());
        const Gdiplus::Color transparentBg(0, 255, 255, 255);

        switch (m_brush.style()) {
        case Qt::HorPattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyleHorizontal, fg,
                                                         transparentBg);
        case Qt::VerPattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyleVertical, fg,
                                                         transparentBg);
        case Qt::CrossPattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyleCross, fg,
                                                         transparentBg);
        case Qt::BDiagPattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyleBackwardDiagonal, fg,
                                                         transparentBg);
        case Qt::FDiagPattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyleForwardDiagonal, fg,
                                                         transparentBg);
        case Qt::DiagCrossPattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyleDiagonalCross, fg,
                                                         transparentBg);
        case Qt::Dense1Pattern:
        case Qt::Dense2Pattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyle75Percent, fg,
                                                         transparentBg);
        case Qt::Dense3Pattern:
        case Qt::Dense4Pattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyle50Percent, fg,
                                                         transparentBg);
        case Qt::Dense5Pattern:
        case Qt::Dense6Pattern:
        case Qt::Dense7Pattern:
            return std::make_unique<Gdiplus::HatchBrush>(Gdiplus::HatchStyle25Percent, fg,
                                                         transparentBg);
        default:
            return std::make_unique<Gdiplus::SolidBrush>(fg);
        }
    }

    bool alphaBlendPremulImageViaHdc(const QImage &premul, int dstX, int dstY, int dstW, int dstH,
                                     int sca = 255) const
    {
        if (!m_gfx || premul.isNull() || dstW <= 0 || dstH <= 0 || sca <= 0)
            return false;

        const int srcW = premul.width();
        const int srcH = premul.height();
        if (srcW <= 0 || srcH <= 0)
            return false;

        BITMAPINFO bmiAlpha{};
        bmiAlpha.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmiAlpha.bmiHeader.biWidth = srcW;
        bmiAlpha.bmiHeader.biHeight = srcH; // positive bottom-up DIB for reliable EMF playback
        bmiAlpha.bmiHeader.biPlanes = 1;
        bmiAlpha.bmiHeader.biBitCount = 32;
        bmiAlpha.bmiHeader.biCompression = BI_RGB;

        void *bits = nullptr;
        HDC memDc = CreateCompatibleDC(nullptr);
        if (!memDc)
            return false;

        HBITMAP hBmp = CreateDIBSection(memDc, &bmiAlpha, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hBmp || !bits) {
            if (hBmp)
                DeleteObject(hBmp);
            DeleteDC(memDc);
            return false;
        }

        auto *dstBytes = static_cast<unsigned char *>(bits);
        const size_t rowBytes = static_cast<size_t>(srcW) * 4u;
        for (int y = 0; y < srcH; ++y) {
            std::memcpy(dstBytes + static_cast<size_t>(srcH - 1 - y) * rowBytes,
                        premul.constScanLine(y), rowBytes);
        }

        HGDIOBJ oldBmp = SelectObject(memDc, hBmp);
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = static_cast<BYTE>(std::clamp(sca, 0, 255));
        bf.AlphaFormat = AC_SRC_ALPHA;

        HDC targetHdc = m_gfx->GetHDC();
        BOOL blended = FALSE;
        if (targetHdc) {
            int savedDc = SaveDC(targetHdc);
            if (m_hasClip && !m_deviceClipPath.isEmpty()) {
                SetPolyFillMode(targetHdc, m_deviceClipPath.fillRule() == Qt::WindingFill
                                                   ? WINDING
                                                   : ALTERNATE);
                BeginPath(targetHdc);
                const int cnt = m_deviceClipPath.elementCount();
                bool hasSub = false;
                for (int i = 0; i < cnt; ++i) {
                    const QPainterPath::Element &el = m_deviceClipPath.elementAt(i);
                    if (el.type == QPainterPath::MoveToElement) {
                        if (hasSub)
                            CloseFigure(targetHdc);
                        MoveToEx(targetHdc, static_cast<int>(std::lround(el.x)),
                                 static_cast<int>(std::lround(el.y)), nullptr);
                        hasSub = true;
                    } else if (el.type == QPainterPath::LineToElement) {
                        LineTo(targetHdc, static_cast<int>(std::lround(el.x)),
                               static_cast<int>(std::lround(el.y)));
                    } else if (el.type == QPainterPath::CurveToElement && i + 2 < cnt) {
                        const QPainterPath::Element &d1 = m_deviceClipPath.elementAt(i + 1);
                        const QPainterPath::Element &d2 = m_deviceClipPath.elementAt(i + 2);
                        POINT pts[3] = {
                            { static_cast<LONG>(std::lround(el.x)),
                              static_cast<LONG>(std::lround(el.y)) },
                            { static_cast<LONG>(std::lround(d1.x)),
                              static_cast<LONG>(std::lround(d1.y)) },
                            { static_cast<LONG>(std::lround(d2.x)),
                              static_cast<LONG>(std::lround(d2.y)) }
                        };
                        PolyBezierTo(targetHdc, pts, 3);
                        i += 2;
                    }
                }
                if (hasSub)
                    CloseFigure(targetHdc);
                EndPath(targetHdc);
                SelectClipPath(targetHdc, RGN_COPY);
            }
            blended = GdiAlphaBlend(targetHdc, dstX, dstY, dstW, dstH, memDc, 0, 0, srcW, srcH, bf);
            if (savedDc != 0)
                RestoreDC(targetHdc, savedDc);
            m_gfx->ReleaseHDC(targetHdc);
            restoreGdiplusState();
        }

        SelectObject(memDc, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDc);
        return blended != FALSE;
    }

    bool blendTranslucentPath(const QPainterPath &rawPath, const QPainterPath &devPath, bool doFill,
                              bool doStroke) const
    {
        if ((!doFill && !doStroke) || !m_gfx)
            return false;

        QRectF br = devPath.boundingRect();
        if (m_hasClip && !m_deviceClipPath.isEmpty())
            br = br.intersected(m_deviceClipPath.boundingRect().adjusted(-2.0, -2.0, 2.0, 2.0));
        if (br.isEmpty() && !doStroke)
            return false;

        const double pad = (doStroke ? computeDevicePenWidth() : 0.0) + 2.0;
        const int maxTargetW = std::max(1, static_cast<int>(std::ceil(m_targetPxF.width())));
        const int maxTargetH = std::max(1, static_cast<int>(std::ceil(m_targetPxF.height())));
        const int dstX = std::clamp(static_cast<int>(std::floor(br.left() - pad)), 0, maxTargetW);
        const int dstY = std::clamp(static_cast<int>(std::floor(br.top() - pad)), 0, maxTargetH);
        const int dstR = std::clamp(static_cast<int>(std::ceil(br.right() + pad)), 0, maxTargetW);
        const int dstB = std::clamp(static_cast<int>(std::ceil(br.bottom() + pad)), 0, maxTargetH);
        const int dstW = dstR - dstX;
        const int dstH = dstB - dstY;
        if (dstW <= 0 || dstH <= 0)
            return false;

        double ss = 3.0;
        const double maxDim = double(std::max(dstW, dstH));
        if (maxDim * ss > 2048.0)
            ss = std::max(1.0, 2048.0 / maxDim);

        const int imgW = std::max(1, static_cast<int>(std::lround(dstW * ss)));
        const int imgH = std::max(1, static_cast<int>(std::lround(dstH * ss)));
        const double effSx = double(imgW) / double(dstW);
        const double effSy = double(imgH) / double(dstH);

        QImage tile(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
        tile.fill(Qt::transparent);
        {
            QPainter tp(&tile);
            tp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
            tp.setOpacity(m_opacity);
            tp.translate(-double(dstX) * effSx, -double(dstY) * effSy);
            tp.scale(effSx, effSy);
            if (m_hasClip && !m_deviceClipPath.isEmpty())
                tp.setClipPath(m_deviceClipPath);
            tp.scale(m_scaleX, m_scaleY);
            tp.setTransform(m_transform, true);

            if (doFill) {
                tp.setPen(Qt::NoPen);
                tp.setBrush(m_brush);
                tp.drawPath(rawPath);
            }
            if (doStroke) {
                QPen effPen = m_pen;
                if (effPen.isCosmetic() || effPen.widthF() <= 0.0) {
                    const double baseW = (effPen.widthF() <= 0.0) ? 1.0 : effPen.widthF();
                    effPen.setCosmetic(true);
                    effPen.setWidthF(baseW * 0.5 * (effSx * m_scaleX + effSy * m_scaleY));
                }
                tp.setBrush(Qt::NoBrush);
                tp.setPen(effPen);
                tp.drawPath(rawPath);
            }
            tp.end();
        }

        return alphaBlendPremulImageViaHdc(tile, dstX, dstY, dstW, dstH, 255);
    }

    void updateState(const QPaintEngineState &state) override
    {
        const DirtyFlags flags = state.state();
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
            if (!m_gfx || !painter() || !painter()->hasClipping()) {
                if (m_gfx)
                    m_gfx->ResetClip();
                m_hasClip = false;
                m_deviceClipPath = QPainterPath();
            } else {
                const QPainterPath clip = painter()->clipPath();
                if (clip.isEmpty()) {
                    m_gfx->ResetClip();
                    m_hasClip = false;
                    m_deviceClipPath = QPainterPath();
                } else {
                    m_deviceClipPath = mapToDevicePath(clip, painter()->transform());
                    m_hasClip = true;
                    Gdiplus::GraphicsPath clipGp;
                    buildGdiplusPath(m_deviceClipPath, clipGp, true);
                    m_gfx->SetClip(&clipGp, Gdiplus::CombineModeReplace);
                }
            }
        }
    }

    void drawPath(const QPainterPath &path) override
    {
        if (!m_gfx || path.isEmpty() || m_opacity <= 0.0)
            return;

        const int brushAlpha =
                std::clamp(static_cast<int>(std::lround(m_brush.color().alpha() * m_opacity)), 0, 255);
        const int penAlpha =
                std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        const bool hasBrush = (m_brush.style() != Qt::NoBrush && brushAlpha > 0);
        const bool hasPen = (m_pen.style() != Qt::NoPen && penAlpha > 0);
        if (!hasBrush && !hasPen)
            return;

        const QPainterPath devPath = mapToDevicePath(path, m_transform);
        const bool translucentBrush = hasBrush && (brushAlpha < 255);
        const bool translucentPen = hasPen && (penAlpha < 255);

        if (translucentBrush || translucentPen) {
            if (translucentBrush && translucentPen) {
                if (blendTranslucentPath(path, devPath, true, true))
                    return;
            } else {
                if (hasBrush) {
                    if (translucentBrush) {
                        blendTranslucentPath(path, devPath, true, false);
                    } else {
                        Gdiplus::GraphicsPath gpFill;
                        buildGdiplusPath(devPath, gpFill, true);
                        m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                        std::unique_ptr<Gdiplus::Brush> gbrush = createGdiplusBrush(255);
                        m_gfx->FillPath(gbrush.get(), &gpFill);
                    }
                }
                if (hasPen) {
                    if (translucentPen) {
                        blendTranslucentPath(path, devPath, false, true);
                    } else {
                        Gdiplus::GraphicsPath gpStroke;
                        buildGdiplusPath(devPath, gpStroke, false);
                        m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                        Gdiplus::Pen gpen(Gdiplus::Color(255, 0, 0, 0), 1.0f);
                        configureGdiplusPen(gpen, 255);
                        m_gfx->DrawPath(&gpen, &gpStroke);
                    }
                }
                return;
            }
        }

        m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (hasBrush) {
            Gdiplus::GraphicsPath gpFill;
            buildGdiplusPath(devPath, gpFill, true);
            std::unique_ptr<Gdiplus::Brush> gbrush = createGdiplusBrush(brushAlpha);
            m_gfx->FillPath(gbrush.get(), &gpFill);
        }
        if (hasPen) {
            Gdiplus::GraphicsPath gpStroke;
            buildGdiplusPath(devPath, gpStroke, false);
            Gdiplus::Pen gpen(Gdiplus::Color(255, 0, 0, 0), 1.0f);
            configureGdiplusPen(gpen, penAlpha);
            m_gfx->DrawPath(&gpen, &gpStroke);
        }
    }

    void drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode) override
    {
        if (!m_gfx || !points || pointCount < 2 || m_opacity <= 0.0)
            return;

        const int brushAlpha =
                std::clamp(static_cast<int>(std::lround(m_brush.color().alpha() * m_opacity)), 0, 255);
        const int penAlpha =
                std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        const bool hasBrush =
                (mode != PolylineMode && m_brush.style() != Qt::NoBrush && brushAlpha > 0);
        const bool hasPen = (m_pen.style() != Qt::NoPen && penAlpha > 0);
        if (!hasBrush && !hasPen)
            return;

        if ((hasBrush && brushAlpha < 255) || (hasPen && penAlpha < 255)) {
            QPainterPath path;
            path.setFillRule(mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
            path.moveTo(points[0]);
            for (int i = 1; i < pointCount; ++i)
                path.lineTo(points[i]);
            if (mode != PolylineMode)
                path.closeSubpath();
            drawPath(path);
            return;
        }

        std::vector<Gdiplus::PointF> gpts(static_cast<size_t>(pointCount));
        for (int i = 0; i < pointCount; ++i)
            gpts[static_cast<size_t>(i)] = mapPointF(points[i]);

        m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (mode == PolylineMode) {
            if (hasPen) {
                Gdiplus::Pen gpen(Gdiplus::Color(255, 0, 0, 0), 1.0f);
                configureGdiplusPen(gpen, 255);
                m_gfx->DrawLines(&gpen, gpts.data(), pointCount);
            }
        } else {
            if (hasBrush) {
                std::unique_ptr<Gdiplus::Brush> gbrush = createGdiplusBrush(255);
                m_gfx->FillPolygon(gbrush.get(), gpts.data(), pointCount,
                                   mode == WindingMode ? Gdiplus::FillModeWinding
                                                       : Gdiplus::FillModeAlternate);
            }
            if (hasPen) {
                Gdiplus::Pen gpen(Gdiplus::Color(255, 0, 0, 0), 1.0f);
                configureGdiplusPen(gpen, 255);
                m_gfx->DrawPolygon(&gpen, gpts.data(), pointCount);
            }
        }
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
        if (!m_gfx || !lines || lineCount <= 0 || m_opacity <= 0.0)
            return;

        const int penAlpha =
                std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        if (m_pen.style() == Qt::NoPen || penAlpha <= 0)
            return;

        if (penAlpha < 255) {
            QPainterPath linePath;
            for (int i = 0; i < lineCount; ++i) {
                linePath.moveTo(lines[i].p1());
                linePath.lineTo(lines[i].p2());
            }
            drawPath(linePath);
            return;
        }

        m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen gpen(Gdiplus::Color(255, 0, 0, 0), 1.0f);
        configureGdiplusPen(gpen, 255);
        for (int i = 0; i < lineCount; ++i) {
            const Gdiplus::PointF p1 = mapPointF(lines[i].p1());
            const Gdiplus::PointF p2 = mapPointF(lines[i].p2());
            m_gfx->DrawLine(&gpen, p1, p2);
        }
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
        if (!m_gfx || !rects || rectCount <= 0 || m_opacity <= 0.0)
            return;

        const int brushAlpha =
                std::clamp(static_cast<int>(std::lround(m_brush.color().alpha() * m_opacity)), 0, 255);
        const int penAlpha =
                std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        const bool hasBrush = (m_brush.style() != Qt::NoBrush && brushAlpha > 0);
        const bool hasPen = (m_pen.style() != Qt::NoPen && penAlpha > 0);
        if (!hasBrush && !hasPen)
            return;

        const bool axisAligned = (std::fabs(m_transform.m12()) < 1e-9
                                  && std::fabs(m_transform.m21()) < 1e-9
                                  && m_transform.isAffine());

        if (axisAligned && hasBrush && brushAlpha == 255 && !hasPen
            && m_brush.style() == Qt::SolidPattern) {
            const QColor c = m_brush.color();
            Gdiplus::SolidBrush sbr(Gdiplus::Color(255, c.red(), c.green(), c.blue()));
            m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeNone);
            for (int i = 0; i < rectCount; ++i) {
                const Gdiplus::PointF pTL = mapPointF(rects[i].topLeft());
                const Gdiplus::PointF pBR = mapPointF(rects[i].bottomRight());
                Gdiplus::REAL x0 = std::min(pTL.X, pBR.X);
                Gdiplus::REAL y0 = std::min(pTL.Y, pBR.Y);
                Gdiplus::REAL rw = std::fabs(pBR.X - pTL.X);
                Gdiplus::REAL rh = std::fabs(pBR.Y - pTL.Y);
                if (x0 <= 0.5f && y0 <= 0.5f
                    && rw >= static_cast<Gdiplus::REAL>(m_targetPxF.width() - 1.0)
                    && rh >= static_cast<Gdiplus::REAL>(m_targetPxF.height() - 1.0)) {
                    x0 = -1.0f;
                    y0 = -1.0f;
                    rw = static_cast<Gdiplus::REAL>(m_targetPxF.width() + 2.0);
                    rh = static_cast<Gdiplus::REAL>(m_targetPxF.height() + 2.0);
                }
                m_gfx->FillRectangle(&sbr, x0, y0, rw, rh);
            }
            m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            return;
        }

        for (int i = 0; i < rectCount; ++i) {
            const QRectF &r = rects[i];
            const QPointF poly[4] = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };
            drawPolygon(poly, 4, OddEvenMode);
        }
    }

    void drawRects(const QRect *rects, int rectCount) override
    {
        if (!rects || rectCount <= 0)
            return;
        std::vector<QRectF> rf(static_cast<size_t>(rectCount));
        for (int i = 0; i < rectCount; ++i)
            rf[static_cast<size_t>(i)] =
                    QRectF(rects[i].left(), rects[i].top(), rects[i].width(), rects[i].height());
        drawRects(rf.data(), rectCount);
    }

    void drawEllipse(const QRectF &rect) override
    {
        if (!m_gfx || rect.isEmpty() || m_opacity <= 0.0)
            return;

        const int brushAlpha =
                std::clamp(static_cast<int>(std::lround(m_brush.color().alpha() * m_opacity)), 0, 255);
        const int penAlpha =
                std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        const bool hasBrush = (m_brush.style() != Qt::NoBrush && brushAlpha > 0);
        const bool hasPen = (m_pen.style() != Qt::NoPen && penAlpha > 0);
        if (!hasBrush && !hasPen)
            return;

        const bool axisAligned = (std::fabs(m_transform.m12()) < 1e-9
                                  && std::fabs(m_transform.m21()) < 1e-9
                                  && m_transform.isAffine());

        if (axisAligned && (!hasBrush || brushAlpha == 255) && (!hasPen || penAlpha == 255)) {
            const Gdiplus::PointF pTL = mapPointF(rect.topLeft());
            const Gdiplus::PointF pBR = mapPointF(rect.bottomRight());
            const Gdiplus::RectF er(std::min(pTL.X, pBR.X), std::min(pTL.Y, pBR.Y),
                                    std::fabs(pBR.X - pTL.X), std::fabs(pBR.Y - pTL.Y));
            m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            if (hasBrush) {
                std::unique_ptr<Gdiplus::Brush> gbrush = createGdiplusBrush(255);
                m_gfx->FillEllipse(gbrush.get(), er);
            }
            if (hasPen) {
                Gdiplus::Pen gpen(Gdiplus::Color(255, 0, 0, 0), 1.0f);
                configureGdiplusPen(gpen, 255);
                m_gfx->DrawEllipse(&gpen, er);
            }
            return;
        }

        QPainterPath p;
        p.addEllipse(rect);
        drawPath(p);
    }

    void drawEllipse(const QRect &rect) override { drawEllipse(QRectF(rect)); }

    void drawPoints(const QPointF *points, int pointCount) override
    {
        if (!m_gfx || !points || pointCount <= 0 || m_opacity <= 0.0)
            return;

        const int penAlpha =
                std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        if (m_pen.style() == Qt::NoPen || penAlpha <= 0)
            return;

        const double diam = std::max(1.6, computeDevicePenWidth());
        const double rad = 0.5 * diam;
        if (penAlpha < 255) {
            QPainterPath dotsPath;
            dotsPath.setFillRule(Qt::WindingFill);
            const double rx = rad / std::max(1e-6, m_scaleX);
            const double ry = rad / std::max(1e-6, m_scaleY);
            for (int i = 0; i < pointCount; ++i)
                dotsPath.addEllipse(points[i], rx, ry);
            QBrush savedBrush = m_brush;
            QPen savedPen = m_pen;
            m_brush = QBrush(m_pen.color(), Qt::SolidPattern);
            m_pen = QPen(Qt::NoPen);
            drawPath(dotsPath);
            m_brush = savedBrush;
            m_pen = savedPen;
            return;
        }

        const QColor c = m_pen.color();
        Gdiplus::SolidBrush dotBrush(Gdiplus::Color(255, c.red(), c.green(), c.blue()));
        m_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const Gdiplus::REAL dF = static_cast<Gdiplus::REAL>(diam);
        const Gdiplus::REAL rF = static_cast<Gdiplus::REAL>(rad);
        for (int i = 0; i < pointCount; ++i) {
            const Gdiplus::PointF gp = mapPointF(points[i]);
            m_gfx->FillEllipse(&dotBrush, gp.X - rF, gp.Y - rF, dF, dF);
        }
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
        if (!m_gfx)
            return;
        const QString text = textItem.text();
        if (text.isEmpty() || m_opacity <= 0.0)
            return;

        const int textAlpha =
                std::clamp(static_cast<int>(std::lround(m_pen.color().alpha() * m_opacity)), 0, 255);
        if (textAlpha <= 0)
            return;

        const QPointF p0 = m_transform.map(p);
        const QPointF px = m_transform.map(p + QPointF(1.0, 0.0)) - p0;
        const QPointF py = m_transform.map(p + QPointF(0.0, 1.0)) - p0;

        const double dx = px.x() * m_scaleX;
        const double dy = px.y() * m_scaleY;
        const double angleDeg = std::atan2(dy, dx) * (180.0 / 3.14159265358979323846);

        double vScale = std::hypot(py.x() * m_scaleX, py.y() * m_scaleY);
        if (vScale <= 0.0)
            vScale = m_scaleY;

        const QFont f = textItem.font();
        double logicalPxHeight = 12.0;
        if (f.pixelSize() > 0) {
            logicalPxHeight = double(f.pixelSize());
        } else if (f.pointSizeF() > 0.0) {
            logicalPxHeight = f.pointSizeF() * double(m_dpiY) / 72.0;
        }
        const Gdiplus::REAL emSizePx =
                static_cast<Gdiplus::REAL>(std::max(1.0, logicalPxHeight * vScale));

        std::wstring familyName = f.family().trimmed().toStdWString();
        if (familyName.empty())
            familyName = L"Arial";

        std::unique_ptr<Gdiplus::FontFamily> fam =
                std::make_unique<Gdiplus::FontFamily>(familyName.c_str());
        if (!fam->IsAvailable())
            fam = std::make_unique<Gdiplus::FontFamily>(L"Arial");

        INT styleFlags = Gdiplus::FontStyleRegular;
        if (f.bold())
            styleFlags |= Gdiplus::FontStyleBold;
        if (f.italic())
            styleFlags |= Gdiplus::FontStyleItalic;
        if (f.underline())
            styleFlags |= Gdiplus::FontStyleUnderline;
        if (f.strikeOut())
            styleFlags |= Gdiplus::FontStyleStrikeout;

        INT metricStyle = styleFlags & (Gdiplus::FontStyleBold | Gdiplus::FontStyleItalic);
        if (fam->IsAvailable() && !fam->IsStyleAvailable(metricStyle)) {
            if ((metricStyle & Gdiplus::FontStyleBold)
                && fam->IsStyleAvailable(Gdiplus::FontStyleBold)) {
                metricStyle = Gdiplus::FontStyleBold;
            } else if ((metricStyle & Gdiplus::FontStyleItalic)
                       && fam->IsStyleAvailable(Gdiplus::FontStyleItalic)) {
                metricStyle = Gdiplus::FontStyleItalic;
            } else {
                metricStyle = Gdiplus::FontStyleRegular;
            }
            styleFlags = metricStyle
                    | (styleFlags & (Gdiplus::FontStyleUnderline | Gdiplus::FontStyleStrikeout));
        }

        const UINT16 emH = fam->IsAvailable() ? fam->GetEmHeight(metricStyle) : 2048;
        const UINT16 cellAsc = fam->IsAvailable() ? fam->GetCellAscent(metricStyle) : 1854;
        const Gdiplus::REAL ascentPx =
                (emH > 0) ? (emSizePx * static_cast<Gdiplus::REAL>(cellAsc)
                             / static_cast<Gdiplus::REAL>(emH))
                          : (emSizePx * 0.9052734f);

        Gdiplus::Font gdipFont(fam.get(), emSizePx, styleFlags, Gdiplus::UnitPixel);
        Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
        INT sflags = fmt.GetFormatFlags();
        sflags |= Gdiplus::StringFormatFlagsNoClip | Gdiplus::StringFormatFlagsNoWrap
                | Gdiplus::StringFormatFlagsMeasureTrailingSpaces;
        sflags &= ~Gdiplus::StringFormatFlagsLineLimit;
        fmt.SetFormatFlags(sflags);

        const QColor tc = m_pen.color();
        Gdiplus::SolidBrush textBrush(
                Gdiplus::Color(static_cast<BYTE>(textAlpha), tc.red(), tc.green(), tc.blue()));

        const Gdiplus::PointF gp = mapPointF(p);
        const std::wstring ws = text.toStdWString();

        const Gdiplus::GraphicsState saved = m_gfx->Save();
        m_gfx->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        m_gfx->TranslateTransform(gp.X, gp.Y);
        if (std::fabs(angleDeg) > 1e-3)
            m_gfx->RotateTransform(static_cast<Gdiplus::REAL>(angleDeg));
        m_gfx->DrawString(ws.c_str(), static_cast<INT>(ws.size()), &gdipFont,
                          Gdiplus::PointF(0.0f, -ascentPx), &fmt, &textBrush);
        m_gfx->Restore(saved);
    }

    void drawImage(const QRectF &r, const QImage &pm, const QRectF &sr,
                   Qt::ImageConversionFlags) override
    {
        if (!m_gfx || pm.isNull() || r.isEmpty() || m_opacity <= 0.0)
            return;

        const int sca = std::clamp(static_cast<int>(std::lround(m_opacity * 255.0)), 0, 255);
        if (sca <= 0)
            return;

        const QImage sub = sr.isValid() && !sr.isEmpty() ? pm.copy(sr.toRect()) : pm;
        if (sub.isNull())
            return;

        const Gdiplus::PointF pTL = mapPointF(r.topLeft());
        const Gdiplus::PointF pBR = mapPointF(r.bottomRight());
        const Gdiplus::REAL fx = std::min(pTL.X, pBR.X);
        const Gdiplus::REAL fy = std::min(pTL.Y, pBR.Y);
        const Gdiplus::REAL fw = std::max(1.0f, std::fabs(pBR.X - pTL.X));
        const Gdiplus::REAL fh = std::max(1.0f, std::fabs(pBR.Y - pTL.Y));

        if (sub.hasAlphaChannel() || m_opacity < 1.0) {
            const QImage premul = sub.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            const int dstX = static_cast<int>(std::lround(fx));
            const int dstY = static_cast<int>(std::lround(fy));
            const int dstW = std::max(1, static_cast<int>(std::lround(fw)));
            const int dstH = std::max(1, static_cast<int>(std::lround(fh)));
            if (alphaBlendPremulImageViaHdc(premul, dstX, dstY, dstW, dstH, sca))
                return;
        }

        QImage argbImg = sub.convertToFormat(QImage::Format_ARGB32);
        Gdiplus::Bitmap gdipBmp(argbImg.width(), argbImg.height(), argbImg.bytesPerLine(),
                                PixelFormat32bppRGB, const_cast<BYTE *>(argbImg.constBits()));
        m_gfx->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        m_gfx->DrawImage(&gdipBmp, Gdiplus::RectF(fx, fy, fw, fh));
    }

    void drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr) override
    {
        if (pm.isNull())
            return;
        drawImage(r, pm.toImage(), sr, Qt::AutoColor);
    }

private:
    Gdiplus::Graphics *m_gfx = nullptr;
    QSize m_logicalPx;
    QSizeF m_targetPxF;
    int m_dpiX = 96;
    int m_dpiY = 96;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    QPen m_pen;
    QBrush m_brush;
    QFont m_font;
    QTransform m_transform;
    bool m_hasClip = false;
    QPainterPath m_deviceClipPath;
    double m_opacity = 1.0;
};

class WinGdiEmfPaintDevice : public QPaintDevice
{
public:
    WinGdiEmfPaintDevice(Gdiplus::Graphics *gfx, QSize logicalPx, QSizeF targetPxF, int dpiX,
                         int dpiY)
        : m_logicalPx(logicalPx),
          m_dpiX(dpiX > 0 ? dpiX : 96),
          m_dpiY(dpiY > 0 ? dpiY : 96),
          m_engine(new WinGdiEmfPaintEngine(gfx, logicalPx, targetPxF, m_dpiX, m_dpiY))
    {
    }

    ~WinGdiEmfPaintDevice() override { delete m_engine; }

    QPaintEngine *paintEngine() const override { return m_engine; }

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

bool ensureGdiplusInitialized()
{
    return ensureGdiplusStarted();
}

bool drawHemfWithGdiplus(HDC dc, HENHMETAFILE hemf, const RECT &rc)
{
    if (!dc || !hemf)
        return false;
    const DWORD objType = GetObjectType(dc);
    if (objType == OBJ_ENHMETADC || objType == OBJ_METADC)
        return false;
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w == 0 || h == 0)
        return false;
    if (!ensureGdiplusStarted())
        return false;

    Gdiplus::Graphics gfx(dc);
    if (gfx.GetLastStatus() != Gdiplus::Ok)
        return false;
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    gfx.SetPixelOffsetMode(Gdiplus::PixelOffsetModeDefault);
    gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    gfx.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    Gdiplus::Metafile meta(hemf, FALSE);
    if (meta.GetLastStatus() != Gdiplus::Ok)
        return false;
    return gfx.DrawImage(&meta, Gdiplus::Rect(rc.left, rc.top, w, h)) == Gdiplus::Ok;
}

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
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
        p.drawImage(0, 0, src);
    }

    const int w = rgb.width();
    const int h = rgb.height();
    const int stride = w * 4;
    const int imageSize = stride * h;

    const int dpmX = (src.dotsPerMeterX() > 0) ? src.dotsPerMeterX() : 3780;
    const int dpmY = (src.dotsPerMeterY() > 0) ? src.dotsPerMeterY() : 3780;

    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = w;
    bih.biHeight = h; // bottom-up for standard CF_DIB compatibility
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = static_cast<DWORD>(imageSize);
    bih.biXPelsPerMeter = dpmX;
    bih.biYPelsPerMeter = dpmY;

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

    const LONG widthHm =
            std::max(1000L, static_cast<LONG>(std::lround(valid.width() * 2540.0 / 96.0)));
    const LONG heightHm =
            std::max(1000L, static_cast<LONG>(std::lround(valid.height() * 2540.0 / 96.0)));
    out.extentHm = SIZEL{ widthHm, heightHm };

    if (!ensureGdiplusStarted())
        return out;

    HDC ref = GetDC(nullptr);
    if (!ref)
        return out;
    const int devW = std::max(1, GetDeviceCaps(ref, HORZRES));
    const int devH = std::max(1, GetDeviceCaps(ref, VERTRES));
    const int mmW = std::max(1, GetDeviceCaps(ref, HORZSIZE));
    const int mmH = std::max(1, GetDeviceCaps(ref, VERTSIZE));

    const double targetWf = std::max(1.0, double(widthHm) * double(devW) / (100.0 * double(mmW)));
    const double targetHf = std::max(1.0, double(heightHm) * double(devH) / (100.0 * double(mmH)));

    const Gdiplus::RectF frameRect(0.0f, 0.0f, static_cast<Gdiplus::REAL>(widthHm),
                                   static_cast<Gdiplus::REAL>(heightHm));
    Gdiplus::Metafile metafile(ref, frameRect, Gdiplus::MetafileFrameUnitGdi,
                               Gdiplus::EmfTypeEmfPlusDual, L"ToriGraph");
    ReleaseDC(nullptr, ref);
    if (metafile.GetLastStatus() != Gdiplus::Ok)
        return out;

    {
        Gdiplus::Graphics gfx(&metafile);
        if (gfx.GetLastStatus() == Gdiplus::Ok) {
            WinGdiEmfPaintDevice emfDev(&gfx, valid.size(), QSizeF(targetWf, targetHf), 96, 96);
            QPainter painter(&emfDev);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                                           | QPainter::SmoothPixmapTransform,
                                   true);
            painter.drawImage(QRect(0, 0, valid.width(), valid.height()), valid);
            painter.end();
        }
    }

    HENHMETAFILE hemf = metafile.GetHENHMETAFILE();
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

    // 1. Render high-resolution (300 DPI) anti-aliased raster QImage for CF_DIB fallback
    constexpr int kDibFallbackDpi = 300;
    double dibScale = double(kDibFallbackDpi) / std::max(36.0, dpiX * zoom);
    const double maxLogicalSide = double(std::max(pxSize.width(), pxSize.height()));
    if (maxLogicalSide * dibScale > 3600.0)
        dibScale = std::max(1.0, 3600.0 / maxLogicalSide);
    const QSize dibSize(std::max(50, static_cast<int>(std::lround(pxSize.width() * dibScale))),
                        std::max(50, static_cast<int>(std::lround(pxSize.height() * dibScale))));

    QImage raster(dibSize, QImage::Format_ARGB32);
    raster.fill(Qt::white);
    const int dpm = static_cast<int>(std::lround(double(kDibFallbackDpi) / 0.0254));
    raster.setDotsPerMeterX(dpm);
    raster.setDotsPerMeterY(dpm);
    {
        QPainter rp(&raster);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                                  | QPainter::SmoothPixmapTransform,
                          true);
        if (singleLayer) {
            rp.scale(double(dibSize.width()) / double(pxSize.width()),
                     double(dibSize.height()) / double(pxSize.height()));
            singleLayer->exportPainter(rp, false, QRect(QPoint(0, 0), pxSize), pxSize);
        } else if (ml) {
            ml->exportPainter(rp, false, mlLayersRect, dibSize);
        }
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

    if (!ensureGdiplusStarted())
        return renderFromImage(raster);

    HDC ref = GetDC(nullptr);
    if (!ref)
        return out;
    const int devW = std::max(1, GetDeviceCaps(ref, HORZRES));
    const int devH = std::max(1, GetDeviceCaps(ref, VERTRES));
    const int mmW = std::max(1, GetDeviceCaps(ref, HORZSIZE));
    const int mmH = std::max(1, GetDeviceCaps(ref, VERTSIZE));

    const double targetWf = std::max(1.0, double(widthHm) * double(devW) / (100.0 * double(mmW)));
    const double targetHf = std::max(1.0, double(heightHm) * double(devH) / (100.0 * double(mmH)));

    const Gdiplus::RectF frameRect(0.0f, 0.0f, static_cast<Gdiplus::REAL>(widthHm),
                                   static_cast<Gdiplus::REAL>(heightHm));
    Gdiplus::Metafile metafile(ref, frameRect, Gdiplus::MetafileFrameUnitGdi,
                               Gdiplus::EmfTypeEmfPlusDual, L"ToriGraph");
    ReleaseDC(nullptr, ref);
    if (metafile.GetLastStatus() != Gdiplus::Ok)
        return renderFromImage(raster);

    {
        Gdiplus::Graphics gfx(&metafile);
        if (gfx.GetLastStatus() == Gdiplus::Ok) {
            const int emfLogicalDpiX = std::max(36, static_cast<int>(std::lround(dpiX * zoom)));
            const int emfLogicalDpiY = std::max(36, static_cast<int>(std::lround(dpiY * zoom)));
            WinGdiEmfPaintDevice emfDev(&gfx, pxSize, QSizeF(targetWf, targetHf), emfLogicalDpiX,
                                        emfLogicalDpiY);
            QPainter vp(&emfDev);
            vp.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                                      | QPainter::SmoothPixmapTransform,
                              true);
            if (singleLayer) {
                vp.fillRect(QRect(QPoint(0, 0), pxSize), Qt::white);
                singleLayer->exportPainter(vp, false, QRect(QPoint(0, 0), pxSize), pxSize);
            } else if (ml) {
                ml->exportPainter(vp, false, mlLayersRect, pxSize);
            }
            vp.end();
        }
    }

    HENHMETAFILE hemf = metafile.GetHENHMETAFILE();
    if (hemf) {
        out.emfBytes = hemfToBytes(hemf);
        DeleteEnhMetaFile(hemf);
    }
    if (out.emfBytes.isEmpty())
        return renderFromImage(raster);
    return out;
}

} // namespace SciDAVisEmf

#endif // Q_OS_WIN
