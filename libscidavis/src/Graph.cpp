#include "OleServer.h"
#include "MultiLayer.h"
/***************************************************************************
    File                 : Graph.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Graph widget

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/

#include <QVarLengthArray>

#include "Graph.h"
#include "ExtraPlotTypes.h"
#include "Grid.h"
#include "CanvasPicker.h"
#include "CanvasInlineTextEditor.h"
#include "QwtErrorPlotCurve.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ScalePicker.h"
#include "TitlePicker.h"
#include "QwtPieCurve.h"
#include "ImageMarker.h"
#include "QwtBarCurve.h"
#include "BoxCurve.h"
#include "QwtHistogram.h"
#include "VectorCurve.h"
#include "ScaleDraw.h"
#include "ColorButton.h"
#include "PatternBox.h"
#include "SymbolBox.h"
#include "FunctionCurve.h"
#include "Spectrogram.h"
#include "SelectionMoveResizer.h"
#include "DataPickerTool.h"
#include "RangeSelectorTool.h"
#include "PlotCurve.h"
#include "ApplicationWindow.h"
#include "I18nJapanese.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "core/column/Column.h"
#include "MultiLayer.h"
#include "EmfPaintDevice.h"
#include "MyParser.h"
#include <algorithm>
#include <limits>
#include <mutex>

#include <QApplication>
#include <QFontMetricsF>
#include <QBitmap>
#include <QClipboard>
#include <QCursor>
#include <QFile>
#include <QImage>
#include <QMessageBox>
#include <QPainterPath>
#include <QPixmap>
#include <QPainter>
#include <QMenu>
#include <QTextStream>
#include <QLocale>
#include <QPrintDialog>
#include <QImageWriter>
#include <QFileInfo>
#include <QRegExp>
#include <QRegularExpression>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QMainWindow>
#include <QStatusBar>
#include <QTextDocument>

#if QT_VERSION >= 0x040300
#include <QSvgGenerator>
#endif

#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include <qwt_color_map.h>

#include <cmath>
#include <stdexcept>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
using namespace std;

namespace {

struct ScopedExportFlag
{
    bool &target;
    bool prev;
    explicit ScopedExportFlag(bool &flag) : target(flag), prev(flag) { target = true; }
    ~ScopedExportFlag() { target = prev; }
};

class GraphSelectionOverlayWidget : public QWidget
{
public:
    explicit GraphSelectionOverlayWidget(Graph *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }

    void setDashed(bool dashed)
    {
        if (d_dashed != dashed) {
            d_dashed = dashed;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Graph *g = qobject_cast<const Graph *>(parentWidget());
        if (!g || g->isExporting() || g->property("isExporting").toBool())
            return;
        if (width() < 3 || height() < 3)
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 195), 3.0, Qt::SolidLine));
        p.drawRoundedRect(r, 2.0, 2.0);
        p.setPen(QPen(QColor(QStringLiteral("#2563EB")), 1.5,
                      d_dashed ? Qt::DashLine : Qt::SolidLine));
        p.drawRoundedRect(r, 2.0, 2.0);
    }

private:
    bool d_dashed = false;
};

} // namespace

Graph::Graph(QWidget *parent, QString name, Qt::WindowFlags f) : QWidget(parent, f)
{
    if (name.isEmpty())
        setObjectName("graph");
    else
        setObjectName(name);

    n_curves = 0;
    d_active_tool = NULL;
    widthLine = 2;
    selectedMarker = -1;
    drawTextOn = false;
    drawLineOn = false;
    drawArrowOn = false;
    ignoreResize = true;
    drawAxesBackbone = true;
    m_autoscale = true;
    d_is_ternary = false;
    autoScaleFonts = false;
    hidden_size = QSize();
    d_antialiasing = true;
    d_scale_on_print = true;
    d_print_cropmarks = false;

    defaultArrowLineWidth = 1;
    defaultArrowColor = QColor(Qt::black);
    defaultArrowLineStyle = Qt::SolidLine;
    defaultArrowHeadLength = 4;
    defaultArrowHeadAngle = 45;
    defaultArrowHeadFill = true;

    defaultMarkerFont = QFont("Arial", 14, QFont::Normal, false);
    defaultMarkerFont.setHintingPreference(QFont::PreferNoHinting);
    auxMrkFont = QFont("Arial", 12, QFont::Normal, false);
    auxMrkFont.setHintingPreference(QFont::PreferNoHinting);
    defaultMarkerFrame = 1;
    defaultTextMarkerColor = QColor(Qt::black);
    defaultTextMarkerBackground = QColor(255, 255, 255, 255);
    for (int i = 0; i < 4; ++i)
        d_axis_backbone[i] = true;
    d_axis_unit_format = Column::UnitFormat::ISIJSlash;

    d_user_step = QVector<double>(QwtPlot::axisCnt);
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        axisType << AxisType::Numeric;
        axesFormatInfo << QString();
        axesFormulas << QString();
        d_user_step[i] = 0.0;
    }

    d_plot = new Plot(this);
    {
        QFont initTitleFnt("Arial", 18, QFont::Normal, false);
        initTitleFnt.setHintingPreference(QFont::PreferNoHinting);
        QFont initNumFnt("Arial", 16, QFont::Normal, false);
        initNumFnt.setHintingPreference(QFont::PreferNoHinting);
        initFonts(initTitleFnt, initNumFnt);

        QwtText t = d_plot->title();
        QFont plotTitleFnt("Arial", 18, QFont::Normal, false);
        plotTitleFnt.setHintingPreference(QFont::PreferNoHinting);
        t.setFont(plotTitleFnt);
        d_plot->setTitle(t);
    }
    cp = new CanvasPicker(this);

    titlePicker = new TitlePicker(d_plot);
    scalePicker = new ScalePicker(d_plot);

    d_zoomer[0] = new QwtPlotZoomer(QwtPlot::xBottom, QwtPlot::yLeft,
                                    QwtPicker::DragSelection | QwtPicker::CornerToCorner,
                                    QwtPicker::AlwaysOff, d_plot->canvas());
    d_zoomer[0]->setRubberBandPen(QPen(Qt::black));
    d_zoomer[1] = new QwtPlotZoomer(QwtPlot::xTop, QwtPlot::yRight,
                                    QwtPicker::DragSelection | QwtPicker::CornerToCorner,
                                    QwtPicker::AlwaysOff, d_plot->canvas());
    zoom(false);

    setGeometry(0, 0, 500, 500);
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(d_plot);
    setMouseTracking(true);

    d_selection_overlay = new GraphSelectionOverlayWidget(this);

    legendMarkerID = -1; // no legend for an empty graph
    d_texts = QVector<int>();
    c_type = QVector<int>();
    c_keys = QVector<int>();

    connect(cp, SIGNAL(selectPlot()), this, SLOT(activateGraph()));
    connect(cp, SIGNAL(drawTextOff()), this, SIGNAL(drawTextOff()));
    connect(cp, SIGNAL(viewImageDialog()), this, SIGNAL(viewImageDialog()));
    connect(cp, SIGNAL(viewTextDialog()), this, SIGNAL(viewTextDialog()));
    connect(cp, SIGNAL(viewLineDialog()), this, SIGNAL(viewLineDialog()));
    connect(cp, SIGNAL(showPlotDialog(int)), this, SIGNAL(showPlotDialog(int)));
    connect(cp, SIGNAL(showMarkerPopupMenu()), this, SIGNAL(showMarkerPopupMenu()));
    connect(cp, SIGNAL(modified()), this, SIGNAL(modifiedGraph()));

    connect(titlePicker, SIGNAL(showTitleMenu()), this, SLOT(showTitleContextMenu()));
    connect(titlePicker, SIGNAL(doubleClicked()), this, SIGNAL(viewTitleDialog()));
    connect(titlePicker, SIGNAL(removeTitle()), this, SLOT(removeTitle()));
    connect(titlePicker, SIGNAL(clicked()), this, SLOT(selectTitle()));

    connect(scalePicker, SIGNAL(clicked()), this, SLOT(activateGraph()));
    connect(scalePicker, SIGNAL(clicked()), this, SLOT(deselectMarker()));
    connect(scalePicker, SIGNAL(axisDblClicked(int)), this, SIGNAL(axisDblClicked(int)));
    connect(scalePicker, SIGNAL(axisTitleRightClicked(int)), this, SLOT(showAxisTitleMenu(int)));
    connect(scalePicker, SIGNAL(axisRightClicked(int)), this, SLOT(showAxisContextMenu(int)));
    connect(scalePicker, SIGNAL(xAxisTitleDblClicked()), this, SIGNAL(xAxisTitleDblClicked()));
    connect(scalePicker, SIGNAL(yAxisTitleDblClicked()), this, SIGNAL(yAxisTitleDblClicked()));
    connect(scalePicker, SIGNAL(rightAxisTitleDblClicked()), this,
            SIGNAL(rightAxisTitleDblClicked()));
    connect(scalePicker, SIGNAL(topAxisTitleDblClicked()), this, SIGNAL(topAxisTitleDblClicked()));

    connect(d_zoomer[0], SIGNAL(zoomed(const QwtDoubleRect &)), this,
            SLOT(zoomed(const QwtDoubleRect &)));
}

void Graph::notifyChanges()
{
    updateStatReferenceLines();
    syncChildLinkedLayers();
    emit modifiedGraph();
}

void Graph::activateGraph()
{
    emit selectedGraph(this);
    setFocus();
}

namespace {

static void detachAndDeleteMarkerSelector(QPointer<SelectionMoveResizer> &sel)
{
    if (!sel)
        return;
    SelectionMoveResizer *old = sel.data();
    sel = nullptr;
    const bool hadFocus = old->hasFocus();
    QWidget *pw = old->parentWidget();
    old->blockSignals(true);
    old->disconnect();
    if (pw)
        pw->removeEventFilter(old);
    old->hide();
    old->setParent(nullptr);
    if (pw && hadFocus)
        pw->setFocus();
    old->deleteLater();
}

static QString unwrapNoWrapAxisTitle(const QString &s)
{
    QString out = s;
    static const QString kPrefix = QStringLiteral("<span style=\"white-space:nowrap;\">");
    static const QString kSuffix = QStringLiteral("</span>");
    while (out.startsWith(kPrefix, Qt::CaseInsensitive)
           && out.endsWith(kSuffix, Qt::CaseInsensitive)
           && out.length() >= kPrefix.length() + kSuffix.length()) {
        out = out.mid(kPrefix.length(), out.length() - kPrefix.length() - kSuffix.length());
    }
    return out;
}

static QString formatScientificMarkupTokens(const QString &input)
{
    const QString clean = unwrapNoWrapAxisTitle(input);
    if (clean.trimmed().isEmpty()
        || (!clean.contains(QLatin1Char('^')) && !clean.contains(QLatin1Char('_')))) {
        return clean;
    }

    static const QRegularExpression kHtmlTagSplitRx(QStringLiteral("(<[^>]+>)"));
    static const QRegularExpression kBracedSupRx(QStringLiteral("\\^\\{([^}]+)\\}"));
    static const QRegularExpression kBracedSubRx(QStringLiteral("_\\{([^}]+)\\}"));
    static const QRegularExpression kPlainSupRx(QStringLiteral(
            "([A-Za-z0-9\\)\\]\\x{0370}-\\x{03FF}\\x{00B0}\\x{00C5}])\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"));
    static const QRegularExpression kLog10SubRx(QStringLiteral("\\blog_10\\b"));
    static const QRegularExpression kSingleSymSubRx(QStringLiteral(
            "(?<![A-Za-z0-9\\x{0370}-\\x{03FF}])([A-Za-z\\x{0370}-\\x{03FF}])_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"));

    QString out;
    out.reserve(clean.size() + 24);
    int subDepth = 0;
    int supDepth = 0;
    int pos = 0;
    auto it = kHtmlTagSplitRx.globalMatch(clean);
    auto transformChunk = [&](QString chunk) -> QString {
        if (subDepth > 0 || supDepth > 0 || chunk.isEmpty())
            return chunk;
        chunk.replace(kBracedSupRx, QStringLiteral("<sup>\\1</sup>"));
        chunk.replace(kBracedSubRx, QStringLiteral("<sub>\\1</sub>"));
        chunk.replace(kPlainSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
        chunk.replace(kLog10SubRx, QStringLiteral("log<sub>10</sub>"));
        chunk.replace(kSingleSymSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
        return chunk;
    };

    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedStart() > pos)
            out += transformChunk(clean.mid(pos, m.capturedStart() - pos));
        const QString tag = m.captured(1);
        const QString tagLower = tag.toLower();
        if (tagLower.startsWith(QLatin1String("<sub")) && !tagLower.endsWith(QLatin1String("/>")))
            ++subDepth;
        else if (tagLower.startsWith(QLatin1String("</sub")) && subDepth > 0)
            --subDepth;
        else if (tagLower.startsWith(QLatin1String("<sup"))
                 && !tagLower.endsWith(QLatin1String("/>")))
            ++supDepth;
        else if (tagLower.startsWith(QLatin1String("</sup")) && supDepth > 0)
            --supDepth;
        out += tag;
        pos = m.capturedEnd();
    }
    if (pos < clean.size())
        out += transformChunk(clean.mid(pos));
    static const QRegularExpression kPostTagSubRx(
            QStringLiteral("(</\\s*i\\s*>)_([A-Za-z0-9+\\-]{1,4}(?:\\.\\d+)?)(?![A-Za-z0-9_])"),
            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kPostTagSupRx(
            QStringLiteral("(</\\s*(?:i|sub)\\s*>)\\^([+\\-\\x{2212}]?\\d+(?:[./]\\d+)?)"),
            QRegularExpression::CaseInsensitiveOption);
    out.replace(kPostTagSubRx, QStringLiteral("\\1<sub>\\2</sub>"));
    out.replace(kPostTagSupRx, QStringLiteral("\\1<sup>\\2</sup>"));
    return out;
}

static QString wrapRichAxisTitleNoWrap(const QString &s)
{
    const QString clean = unwrapNoWrapAxisTitle(s);
    if (clean.trimmed().isEmpty())
        return clean;
    if (clean.contains(QLatin1Char('\n'))
        || clean.contains(QLatin1String("<br"), Qt::CaseInsensitive))
        return clean;
    static const QRegularExpression kRichTagRx(
            QStringLiteral("<\\s*(?:i|sub|sup|b|u|span|font)\\b"),
            QRegularExpression::CaseInsensitiveOption);
    if (clean.contains(kRichTagRx)) {
        return QStringLiteral("<span style=\"white-space:nowrap;\">") + clean
                + QStringLiteral("</span>");
    }
    return clean;
}

class PiecewiseBreakScaleTransformation : public QwtScaleTransformation
{
public:
    explicit PiecewiseBreakScaleTransformation(const Graph::AxisBreakSpec &spec)
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_spec(spec)
    {
    }

    PiecewiseBreakScaleTransformation(const PiecewiseBreakScaleTransformation &other)
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_spec(other.d_spec)
    {
    }

    QwtScaleTransformation *copy() const override
    {
        return new PiecewiseBreakScaleTransformation(*this);
    }

    double xForm(double s, double s1, double s2, double p1, double p2) const override
    {
        if (!std::isfinite(s))
            return std::numeric_limits<double>::quiet_NaN();
        const double lo = qMin(s1, s2);
        const double hi = qMax(s1, s2);
        if (hi - lo <= 1e-300)
            return p1;

        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        double u = 0.0;
        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15)) {
            u = (s - lo) / (hi - lo);
        } else if (s <= bf) {
            if (d_spec.log10Before && lo > 0.0 && bf > lo) {
                const double sc = qMax(s, 1e-300);
                u = pos * (std::log10(sc) - std::log10(lo)) / (std::log10(bf) - std::log10(lo));
            } else {
                u = pos * (s - lo) / (bf - lo);
            }
        } else if (s >= bt) {
            if (d_spec.log10After && bt > 0.0 && hi > bt) {
                const double sc = qMax(s, 1e-300);
                u = pos
                        + (1.0 - pos) * (std::log10(sc) - std::log10(bt))
                                / (std::log10(hi) - std::log10(bt));
            } else {
                u = pos + (1.0 - pos) * (s - bt) / (hi - bt);
            }
        } else {
            u = pos;
        }

        const double t = (s1 <= s2) ? u : (1.0 - u);
        return p1 + t * (p2 - p1);
    }

    double invXForm(double p, double p1, double p2, double s1, double s2) const override
    {
        if (!std::isfinite(p))
            return std::numeric_limits<double>::quiet_NaN();
        if (qAbs(p2 - p1) <= 1e-15)
            return s1;

        const double lo = qMin(s1, s2);
        const double hi = qMax(s1, s2);
        const double t = (p - p1) / (p2 - p1);
        const double u = (s1 <= s2) ? t : (1.0 - t);

        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15))
            return lo + u * (hi - lo);

        if (u <= pos) {
            const double f = (pos > 1e-12) ? (u / pos) : 0.0;
            if (d_spec.log10Before && lo > 0.0 && bf > lo)
                return std::pow(10.0, std::log10(lo) + f * (std::log10(bf) - std::log10(lo)));
            return lo + f * (bf - lo);
        }

        const double f = (1.0 - pos > 1e-12) ? ((u - pos) / (1.0 - pos)) : 0.0;
        if (d_spec.log10After && bt > 0.0 && hi > bt)
            return std::pow(10.0, std::log10(bt) + f * (std::log10(hi) - std::log10(bt)));
        return bt + f * (hi - bt);
    }

private:
    Graph::AxisBreakSpec d_spec;
};

static void ensureLinearHalfStepMinorTicks(QwtScaleDiv &div)
{
    if (!div.ticks(QwtScaleDiv::MinorTick).isEmpty()
        || !div.ticks(QwtScaleDiv::MediumTick).isEmpty()) {
        return;
    }
    const QwtValueList maj = div.ticks(QwtScaleDiv::MajorTick);
    if (maj.size() < 2)
        return;
    const double step = std::fabs(maj[1] - maj[0]);
    if (!(step > 1e-300) || !std::isfinite(step))
        return;
    const double half = 0.5 * step;
    const double lo = qMin(div.lowerBound(), div.upperBound());
    const double hi = qMax(div.lowerBound(), div.upperBound());
    const double eps = 1e-9 * step;
    double firstMaj = maj.first();
    double lastMaj = maj.last();
    if (firstMaj > lastMaj)
        qSwap(firstMaj, lastMaj);

    QwtValueList mids;
    if (firstMaj - half >= lo - eps)
        mids += (firstMaj - half);
    for (int i = 0; i + 1 < maj.size(); ++i) {
        const double m = 0.5 * (maj[i] + maj[i + 1]);
        if (m >= lo - eps && m <= hi + eps)
            mids += m;
    }
    if (lastMaj + half <= hi + eps)
        mids += (lastMaj + half);
    if (!mids.isEmpty())
        div.setTicks(QwtScaleDiv::MediumTick, mids);
}

class PiecewiseBreakScaleEngine : public QwtLinearScaleEngine
{
public:
    explicit PiecewiseBreakScaleEngine(const Graph::AxisBreakSpec &spec) : d_spec(spec) {}

    QwtScaleTransformation *transformation() const override
    {
        return new PiecewiseBreakScaleTransformation(d_spec);
    }

    QwtScaleDiv divideScale(double x1, double x2, int maxMajSteps, int maxMinSteps,
                            double stepSize = 0.0) const override
    {
        const double lo = qMin(x1, x2);
        const double hi = qMax(x1, x2);
        const double bf = qMin(d_spec.breakFrom, d_spec.breakTo);
        const double bt = qMax(d_spec.breakFrom, d_spec.breakTo);
        const double pos = qBound(0.10, d_spec.positionPct, 0.90);

        if (!d_spec.enabled || !(bf > lo && bt < hi && bt > bf + 1e-15))
            return QwtLinearScaleEngine::divideScale(x1, x2, maxMajSteps, maxMinSteps, stepSize);

        const int maj1 = qMax(1, qRound(qMax(2, maxMajSteps) * pos));
        const int maj2 = qMax(1, qMax(2, maxMajSteps) - maj1);

        QwtScaleDiv div1;
        const bool logBefore = d_spec.log10Before && lo > 0.0 && bf > lo;
        if (logBefore) {
            const double logSpan1 = std::log10(bf / lo);
            const int majBefore = qMax(maj1, int(std::ceil(logSpan1)) + 2);
            const int minBefore = (maxMinSteps >= 1) ? qMax(9, maxMinSteps) : 0;
            QwtLog10ScaleEngine le1;
            div1 = le1.divideScale(lo, bf, majBefore, minBefore,
                                   d_spec.stepBefore > 0.0 ? d_spec.stepBefore : 0.0);
        } else {
            QwtLinearScaleEngine le1;
            div1 = le1.divideScale(lo, bf, maj1, maxMinSteps,
                                   d_spec.stepBefore > 0.0 ? d_spec.stepBefore : stepSize);
            if (maxMinSteps == 2)
                ensureLinearHalfStepMinorTicks(div1);
        }

        QwtScaleDiv div2;
        const bool logAfter = d_spec.log10After && bt > 0.0 && hi > bt;
        if (logAfter) {
            const double logSpan2 = std::log10(hi / bt);
            const int majAfter = qMax(maj2, int(std::ceil(logSpan2)) + 2);
            const int minAfter = (maxMinSteps >= 1) ? qMax(9, maxMinSteps) : 0;
            QwtLog10ScaleEngine le2;
            div2 = le2.divideScale(bt, hi, majAfter, minAfter,
                                   d_spec.stepAfter > 0.0 ? d_spec.stepAfter : 0.0);
        } else {
            QwtLinearScaleEngine le2;
            div2 = le2.divideScale(bt, hi, maj2, maxMinSteps,
                                   d_spec.stepAfter > 0.0 ? d_spec.stepAfter : stepSize);
            if (maxMinSteps == 2)
                ensureLinearHalfStepMinorTicks(div2);
        }

        const double maxAllowedBefore = logBefore
                ? (bf * std::pow(10.0, -0.03 * std::log10(bf / lo)))
                : (bf - 0.04 * (bf - lo));
        const double minAllowedAfter = logAfter
                ? (bt * std::pow(10.0, 0.03 * std::log10(hi / bt)))
                : (bt + 0.04 * (hi - bt));
        const double loEps = logBefore ? (lo * (1.0 - 1e-11)) : (lo - 1e-12 * (bf - lo));
        const double hiEps = logAfter ? (hi * (1.0 + 1e-11)) : (hi + 1e-12 * (hi - bt));

        QwtValueList mergedTicks[QwtScaleDiv::NTickTypes];
        for (int tt = 0; tt < QwtScaleDiv::NTickTypes; ++tt) {
            const QwtScaleDiv::TickType tickType = static_cast<QwtScaleDiv::TickType>(tt);
            for (double v : div1.ticks(tickType)) {
                if (v >= loEps && v <= maxAllowedBefore)
                    mergedTicks[tt].append(v);
            }
            for (double v : div2.ticks(tickType)) {
                if (v >= minAllowedAfter && v <= hiEps)
                    mergedTicks[tt].append(v);
            }
        }

        QwtScaleDiv out(lo, hi, mergedTicks);
        if (x1 > x2)
            out.invert();
        return out;
    }

private:
    Graph::AxisBreakSpec d_spec;
};

class FormulaLinkedScaleTransformation : public QwtScaleTransformation
{
public:
    FormulaLinkedScaleTransformation(const QVector<double> &xp, const QVector<double> &uc)
        : QwtScaleTransformation(QwtScaleTransformation::Other), d_xp(xp), d_uc(uc)
    {
    }

    FormulaLinkedScaleTransformation(const FormulaLinkedScaleTransformation &other)
        : QwtScaleTransformation(QwtScaleTransformation::Other),
          d_xp(other.d_xp),
          d_uc(other.d_uc)
    {
    }

    QwtScaleTransformation *copy() const override
    {
        return new FormulaLinkedScaleTransformation(*this);
    }

    double invertUToParentX(double u) const
    {
        const int n = qMin(d_xp.size(), d_uc.size());
        if (n < 2)
            return u;

        const bool inc = (d_uc.last() >= d_uc.first());
        if (inc) {
            if (u <= d_uc.first()) {
                const double du = d_uc[1] - d_uc[0];
                const double dx = d_xp[1] - d_xp[0];
                return (qAbs(du) > 1e-300) ? (d_xp[0] + (u - d_uc[0]) * (dx / du)) : d_xp[0];
            }
            if (u >= d_uc.last()) {
                const double du = d_uc[n - 1] - d_uc[n - 2];
                const double dx = d_xp[n - 1] - d_xp[n - 2];
                return (qAbs(du) > 1e-300)
                        ? (d_xp[n - 1] + (u - d_uc[n - 1]) * (dx / du))
                        : d_xp[n - 1];
            }
            int lo = 0;
            int hi = n - 1;
            while (hi - lo > 1) {
                const int mid = (lo + hi) / 2;
                if (d_uc[mid] <= u)
                    lo = mid;
                else
                    hi = mid;
            }
            const double du = d_uc[hi] - d_uc[lo];
            const double frac = (qAbs(du) > 1e-300) ? ((u - d_uc[lo]) / du) : 0.0;
            return d_xp[lo] + frac * (d_xp[hi] - d_xp[lo]);
        } else {
            if (u >= d_uc.first()) {
                const double du = d_uc[1] - d_uc[0];
                const double dx = d_xp[1] - d_xp[0];
                return (qAbs(du) > 1e-300) ? (d_xp[0] + (u - d_uc[0]) * (dx / du)) : d_xp[0];
            }
            if (u <= d_uc.last()) {
                const double du = d_uc[n - 1] - d_uc[n - 2];
                const double dx = d_xp[n - 1] - d_xp[n - 2];
                return (qAbs(du) > 1e-300)
                        ? (d_xp[n - 1] + (u - d_uc[n - 1]) * (dx / du))
                        : d_xp[n - 1];
            }
            int lo = 0;
            int hi = n - 1;
            while (hi - lo > 1) {
                const int mid = (lo + hi) / 2;
                if (d_uc[mid] >= u)
                    lo = mid;
                else
                    hi = mid;
            }
            const double du = d_uc[hi] - d_uc[lo];
            const double frac = (qAbs(du) > 1e-300) ? ((u - d_uc[lo]) / du) : 0.0;
            return d_xp[lo] + frac * (d_xp[hi] - d_xp[lo]);
        }
    }

    double evalFromParentX(double xp) const
    {
        const int n = qMin(d_xp.size(), d_uc.size());
        if (n < 2)
            return xp;
        if (xp <= d_xp.first()) {
            const double dx = d_xp[1] - d_xp[0];
            const double du = d_uc[1] - d_uc[0];
            return (qAbs(dx) > 1e-300) ? (d_uc[0] + (xp - d_xp[0]) * (du / dx)) : d_uc[0];
        }
        if (xp >= d_xp.last()) {
            const double dx = d_xp[n - 1] - d_xp[n - 2];
            const double du = d_uc[n - 1] - d_uc[n - 2];
            return (qAbs(dx) > 1e-300)
                    ? (d_uc[n - 1] + (xp - d_xp[n - 1]) * (du / dx))
                    : d_uc[n - 1];
        }
        int lo = 0;
        int hi = n - 1;
        while (hi - lo > 1) {
            const int mid = (lo + hi) / 2;
            if (d_xp[mid] <= xp)
                lo = mid;
            else
                hi = mid;
        }
        const double dx = d_xp[hi] - d_xp[lo];
        const double frac = (qAbs(dx) > 1e-300) ? ((xp - d_xp[lo]) / dx) : 0.0;
        return d_uc[lo] + frac * (d_uc[hi] - d_uc[lo]);
    }

    double xForm(double u, double s1, double s2, double p1Px, double p2Px) const override
    {
        if (!std::isfinite(u))
            return std::numeric_limits<double>::quiet_NaN();
        const double xpAtS1 = invertUToParentX(s1);
        const double xpAtS2 = invertUToParentX(s2);
        const double denom = xpAtS2 - xpAtS1;
        if (qAbs(denom) <= 1e-300)
            return p1Px;
        const double xp = invertUToParentX(u);
        const double t = (xp - xpAtS1) / denom;
        return p1Px + t * (p2Px - p1Px);
    }

    double invXForm(double px, double p1Px, double p2Px, double s1, double s2) const override
    {
        if (!std::isfinite(px))
            return std::numeric_limits<double>::quiet_NaN();
        if (qAbs(p2Px - p1Px) <= 1e-15)
            return s1;
        const double xpAtS1 = invertUToParentX(s1);
        const double xpAtS2 = invertUToParentX(s2);
        const double t = (px - p1Px) / (p2Px - p1Px);
        const double xp = xpAtS1 + t * (xpAtS2 - xpAtS1);
        return evalFromParentX(xp);
    }

private:
    QVector<double> d_xp;
    QVector<double> d_uc;
};

class FormulaLinkedScaleEngine : public QwtLinearScaleEngine
{
public:
    FormulaLinkedScaleEngine(const QVector<double> &xp, const QVector<double> &uc)
        : d_xp(xp), d_uc(uc)
    {
    }

    QwtScaleTransformation *transformation() const override
    {
        return new FormulaLinkedScaleTransformation(d_xp, d_uc);
    }

    QwtScaleDiv divideScale(double x1, double x2, int maxMajSteps, int maxMinSteps,
                            double stepSize = 0.0) const override
    {
        Q_UNUSED(maxMinSteps);
        const double lo = qMin(x1, x2);
        const double hi = qMax(x1, x2);
        const double span = hi - lo;
        const int maxMaj = qBound(2, maxMajSteps, 6);

        if (span > 1e-30 && stepSize <= 0.0 && d_xp.size() >= 2 && d_uc.size() >= 2) {
            const FormulaLinkedScaleTransformation tf(d_xp, d_uc);
            const double xp0 = tf.invertUToParentX(lo);
            const double xp1 = tf.invertUToParentX(hi);
            const double xpSpan = xp1 - xp0;
            if (std::isfinite(xpSpan) && std::fabs(xpSpan) > 1e-30) {
                double minDx = 1e300;
                double maxDx = 0.0;
                constexpr int kProbes = 16;
                for (int m = 0; m < kProbes; ++m) {
                    const double ua = lo + span * (double(m) / double(kProbes));
                    const double ub = lo + span * (double(m + 1) / double(kProbes));
                    const double dx = std::fabs(tf.invertUToParentX(ub) - tf.invertUToParentX(ua));
                    if (dx > 1e-30) {
                        minDx = qMin(minDx, dx);
                        maxDx = qMax(maxDx, dx);
                    }
                }

                if (minDx < 1e290 && maxDx > 1.5 * minDx) {
                    auto normT = [&](double u) -> double {
                        return (tf.invertUToParentX(u) - xp0) / xpSpan;
                    };

                    struct RoundCand {
                        double u;
                        double t;
                        int rank;
                    };
                    QVector<RoundCand> cands;
                    const double eps = 1e-11 * span;

                    auto addCand = [&](double val, int baseRank) {
                        if (!std::isfinite(val) || val < lo - eps || val > hi + eps)
                            return;
                        const double u = qBound(lo, val, hi);
                        const double t = normT(u);
                        if (!std::isfinite(t))
                            return;
                        const bool isInterior = (t >= 0.03 && t <= 0.97);
                        const bool isExactCorner = (baseRank <= 2)
                                && (t <= 1e-3 || t >= 1.0 - 1e-3) && (t >= -1e-4 && t <= 1.0 + 1e-4);
                        if (!isInterior && !isExactCorner)
                            return;
                        const int effRank = isInterior ? baseRank : (baseRank + 2);
                        for (int k = 0; k < cands.size(); ++k) {
                            if (std::fabs(cands[k].u - u) <= 1e-9 * qMax(1.0, std::fabs(u))) {
                                if (effRank < cands[k].rank)
                                    cands[k].rank = effRank;
                                return;
                            }
                        }
                        cands.append({ u, qBound(0.0, t, 1.0), effRank });
                    };

                    if (lo <= 0.0 && hi >= 0.0)
                        addCand(0.0, 0);

                    struct MantRank {
                        double m;
                        int r;
                    };
                    static const MantRank kMantissas[] = {
                        { 1.0, 0 },  { 5.0, 1 },  { 2.0, 2 },  { 3.0, 4 },  { 2.5, 5 },
                        { 4.0, 6 },  { 8.0, 7 },  { 6.0, 8 },  { 7.0, 9 },  { 1.5, 10 },
                        { 9.0, 11 }, { 1.2, 13 }, { 1.4, 14 }, { 1.6, 14 }, { 1.8, 14 },
                        { 3.5, 15 }, { 4.5, 15 }, { 5.5, 15 }, { 6.5, 15 }, { 7.5, 15 },
                        { 8.5, 15 }
                    };

                    auto populateSign = [&](double sgn, double absLo, double absHi) {
                        if (!(absHi > 0.0))
                            return;
                        const double safeLo = qMax(1e-24, absLo > 0.0 ? absLo : 1e-6 * absHi);
                        const int kMin = int(std::floor(std::log10(safeLo))) - 1;
                        const int kMax = int(std::ceil(std::log10(absHi))) + 1;
                        for (int k = qMax(-24, kMin); k <= qMin(24, kMax); ++k) {
                            const double p10 = std::pow(10.0, double(k));
                            for (const MantRank &mr : kMantissas)
                                addCand(sgn * mr.m * p10, mr.r);
                        }
                    };

                    if (hi > 0.0)
                        populateSign(1.0, qMax(0.0, lo), hi);
                    if (lo < 0.0)
                        populateSign(-1.0, qMax(0.0, -hi), -lo);

                    std::sort(cands.begin(), cands.end(),
                              [](const RoundCand &a, const RoundCand &b) {
                                  if (a.rank != b.rank)
                                      return a.rank < b.rank;
                                  return std::fabs(a.t - 0.5) < std::fabs(b.t - 0.5);
                              });

                    const int targetMaj = qBound(4, maxMajSteps, 6);
                    auto selectWithGap = [&](double minGap) -> QVector<RoundCand> {
                        QVector<RoundCand> picked;
                        for (const RoundCand &c : cands) {
                            if (picked.size() >= targetMaj)
                                break;
                            bool tooClose = false;
                            for (const RoundCand &p : picked) {
                                if (std::fabs(c.t - p.t) < minGap) {
                                    tooClose = true;
                                    break;
                                }
                            }
                            if (!tooClose)
                                picked.append(c);
                        }
                        return picked;
                    };

                    QVector<RoundCand> chosen = selectWithGap(0.14);
                    if (chosen.size() < 3)
                        chosen = selectWithGap(0.11);

                    if (chosen.size() >= 2) {
                        std::sort(chosen.begin(), chosen.end(),
                                  [](const RoundCand &a, const RoundCand &b) { return a.u < b.u; });

                        QwtValueList majList;
                        QVector<double> occupiedT;
                        for (const RoundCand &c : chosen) {
                            majList.append(c.u);
                            occupiedT.append(c.t);
                        }

                        QwtValueList minList;
                        auto tryAddMinor = [&](double mv) {
                            if (!std::isfinite(mv) || mv <= lo + eps || mv >= hi - eps)
                                return;
                            const double mt = normT(mv);
                            if (!std::isfinite(mt) || mt < 0.01 || mt > 0.99)
                                return;
                            for (double ot : occupiedT) {
                                if (std::fabs(mt - ot) < 0.032)
                                    return;
                            }
                            minList.append(mv);
                            occupiedT.append(mt);
                        };

                        QVector<double> knots;
                        knots.append(lo);
                        for (const RoundCand &c : chosen) {
                            if (c.u > knots.last() + eps)
                                knots.append(c.u);
                        }
                        if (hi > knots.last() + eps)
                            knots.append(hi);

                        for (int k = 0; k + 1 < knots.size(); ++k) {
                            const double a = knots[k];
                            const double b = knots[k + 1];
                            const double du = b - a;
                            if (!(du > 1e-30))
                                continue;
                            const double p10 =
                                    std::pow(10.0, std::floor(std::log10(du) + 1e-9));
                            const double ratio = du / p10;
                            int subDivs = 2;
                            if (std::fabs(ratio - 5.0) < 0.05 || std::fabs(ratio - 1.0) < 0.05)
                                subDivs = 5;
                            else if (std::fabs(ratio - 3.0) < 0.05)
                                subDivs = 3;
                            else if (std::fabs(ratio - 4.0) < 0.05)
                                subDivs = 4;
                            for (int s = 1; s < subDivs; ++s)
                                tryAddMinor(a + du * (double(s) / double(subDivs)));
                        }

                        QwtValueList ticks[QwtScaleDiv::NTickTypes];
                        ticks[QwtScaleDiv::MajorTick] = majList;
                        ticks[QwtScaleDiv::MinorTick] = minList;
                        QwtScaleDiv nlDiv(lo, hi, ticks);
                        if (x1 > x2 || testAttribute(QwtScaleEngine::Inverted)) {
                            if (nlDiv.lowerBound() < nlDiv.upperBound())
                                nlDiv.invert();
                        }
                        return nlDiv;
                    }
                }
            }
        }

        const int linMaxMaj = qMin(maxMaj, 5);
        QwtScaleDiv div = QwtLinearScaleEngine::divideScale(lo, hi, linMaxMaj, 2, stepSize);
        ensureLinearHalfStepMinorTicks(div);
        if (x1 > x2 || testAttribute(QwtScaleEngine::Inverted)) {
            if (div.lowerBound() < div.upperBound())
                div.invert();
        }
        return div;
    }

private:
    QVector<double> d_xp;
    QVector<double> d_uc;
};

} // namespace

void Graph::deselectMarker()
{
    selectedMarker = -1;
    detachAndDeleteMarkerSelector(d_markers_selector);
    if (cp)
        cp->disableEditing();
}

long Graph::selectedMarkerKey()
{
    return selectedMarker;
}

QwtPlotMarker *Graph::selectedMarkerPtr()
{
    return d_plot->marker(selectedMarker);
}

void Graph::setSelectedMarker(long mrk, bool add)
{
    selectedMarker = mrk;
    if (add) {
        if (d_markers_selector) {
            if (d_texts.contains(mrk))
                d_markers_selector->add((Legend *)d_plot->marker(mrk));
            else if (d_lines.contains(mrk))
                d_markers_selector->add((ArrowMarker *)d_plot->marker(mrk));
            else if (d_images.contains(mrk))
                d_markers_selector->add((ImageMarker *)d_plot->marker(mrk));
        } else {
            if (d_texts.contains(mrk))
                d_markers_selector = new SelectionMoveResizer((Legend *)d_plot->marker(mrk));
            else if (d_lines.contains(mrk))
                d_markers_selector = new SelectionMoveResizer((ArrowMarker *)d_plot->marker(mrk));
            else if (d_images.contains(mrk))
                d_markers_selector = new SelectionMoveResizer((ImageMarker *)d_plot->marker(mrk));
            else
                return;
            connect(d_markers_selector, SIGNAL(targetsChanged()), this, SIGNAL(modifiedGraph()));
            connect(d_markers_selector, &SelectionMoveResizer::targetsChanged, this, [this]() {
                if (d_plot && legend() && d_markers_selector
                    && d_markers_selector->contains(legend()))
                    d_plot->setProperty("_toriLegendAnchor", QVariant());
            });
            connect(d_markers_selector, SIGNAL(targetsContextMenuRequested()), this,
                    SIGNAL(showMarkerPopupMenu()));
        }
    } else {
        if (d_texts.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((Legend *)d_plot->marker(mrk)))
                    return;
                detachAndDeleteMarkerSelector(d_markers_selector);
            }
            d_markers_selector = new SelectionMoveResizer((Legend *)d_plot->marker(mrk));
        } else if (d_lines.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((ArrowMarker *)d_plot->marker(mrk)))
                    return;
                detachAndDeleteMarkerSelector(d_markers_selector);
            }
            d_markers_selector = new SelectionMoveResizer((ArrowMarker *)d_plot->marker(mrk));
        } else if (d_images.contains(mrk)) {
            if (d_markers_selector) {
                if (d_markers_selector->contains((ImageMarker *)d_plot->marker(mrk)))
                    return;
                detachAndDeleteMarkerSelector(d_markers_selector);
            }
            d_markers_selector = new SelectionMoveResizer((ImageMarker *)d_plot->marker(mrk));
        } else
            return;
        connect(d_markers_selector, SIGNAL(targetsChanged()), this, SIGNAL(modifiedGraph()));
        connect(d_markers_selector, &SelectionMoveResizer::targetsChanged, this, [this]() {
            if (d_plot && legend() && d_markers_selector && d_markers_selector->contains(legend()))
                d_plot->setProperty("_toriLegendAnchor", QVariant());
        });
        connect(d_markers_selector, SIGNAL(targetsContextMenuRequested()), this,
                SIGNAL(showMarkerPopupMenu()));
    }
}

void Graph::initFonts(const QFont &scaleTitleFnt, const QFont &numbersFnt)
{
    QFont tf = scaleTitleFnt;
    tf.setHintingPreference(QFont::PreferNoHinting);
    QFont nf = numbersFnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_plot->setAxisFont(i, nf);
        QwtText t = d_plot->axisTitle(i);
        t.setFont(tf);
        d_plot->setAxisTitle(i, t);
    }
}

void Graph::setAxisFont(int axis, const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    d_plot->setAxisFont(axis, nf);
    d_plot->replot();
    emit modifiedGraph();
}

QFont Graph::axisFont(int axis)
{
    return d_plot->axisFont(axis);
}

void Graph::enableAxis(int axis, bool on)
{
    d_plot->enableAxis(axis, on);
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale && !scale->isColorBarEnabled() && !hasColorScaleOnAxis(axis))
        scale->setMargin(0);

    scalePicker->refresh();
}

void Graph::enableAxes(const QStringList &list)
{
    int i;
    for (i = 0; i < QwtPlot::axisCnt; i++) {
        bool ok = list[i + 1].toInt();
        d_plot->enableAxis(i, ok);
    }

    for (i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale && !scale->isColorBarEnabled() && !hasColorScaleOnAxis(i))
            scale->setMargin(0);
    }
    scalePicker->refresh();
}

void Graph::enableAxes(QVector<bool> axesOn)
{
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        d_plot->enableAxis(i, axesOn[i]);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale && !scale->isColorBarEnabled() && !hasColorScaleOnAxis(i))
            scale->setMargin(0);
    }
    scalePicker->refresh();
}

QVector<bool> Graph::enabledAxes()
{
    QVector<bool> axesOn(4);
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        axesOn[i] = d_plot->axisEnabled(i);
    return axesOn;
}

namespace {
void applyAxisBaselineDist(QwtScaleWidget *scale, int dist)
{
    if (!scale)
        return;
    if (dist < 0) {
        scale->setProperty("_toriNegBaseline", dist);
        scale->setMargin(0);
    } else {
        scale->setProperty("_toriNegBaseline", 0);
        scale->setMargin(dist);
    }
}

int readAxisBaselineDist(const QwtScaleWidget *scale)
{
    if (!scale)
        return 0;
    const int neg = scale->property("_toriNegBaseline").toInt();
    return (neg < 0) ? neg : scale->margin();
}
} // namespace

QList<int> Graph::axesBaseline()
{
    QList<int> baselineDist;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        baselineDist << readAxisBaselineDist(scale);
    }
    return baselineDist;
}

void Graph::setAxesBaseline(const QList<int> &lst)
{
    for (int i = 0; i < QwtPlot::axisCnt && i < lst.size(); i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        applyAxisBaselineDist(scale, lst[i]);
    }
    d_plot->updateLayout();
}

void Graph::setAxesBaseline(QStringList &lst)
{
    lst.removeAll(lst.first());
    for (int i = 0; i < QwtPlot::axisCnt && i < lst.size(); i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        applyAxisBaselineDist(scale, (lst[i]).toInt());
    }
    d_plot->updateLayout();
}

QList<Graph::AxisType> Graph::axesType()
{
    return axisType;
}

void Graph::setLabelsNumericFormat(int axis, int format, int prec, const QString &formula)
{
    axisType[axis] = AxisType::Numeric;
    axesFormulas[axis] = formula;

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    if (format == Plot::Superscripts) {
        QwtSupersciptsScaleDraw *sd = new QwtSupersciptsScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                formula.toUtf8().constData());
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setLabelFormat('s', prec);
        sd->setScaleDiv(div);
        d_plot->setAxisScaleDraw(axis, sd);
    } else {
        ScaleDraw *sd = new ScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                      formula.toUtf8().constData());
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setScaleDiv(div);

        if (format == Plot::Automatic)
            sd->setLabelFormat('g', prec);
        else if (format == Plot::Scientific)
            sd->setLabelFormat('e', prec);
        else if (format == Plot::Decimal)
            sd->setLabelFormat('f', prec);

        d_plot->setAxisScaleDraw(axis, sd);
    }
}

void Graph::setLabelsNumericFormat(int axis, const QStringList &l)
{
    QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
    if (!sd->hasComponent(QwtAbstractScaleDraw::Labels) || axisType[axis] != AxisType::Numeric)
        return;

    int format = l[2 * axis].toInt();
    int prec = l[2 * axis + 1].toInt();
    setLabelsNumericFormat(axis, format, prec, axesFormulas[axis]);
}

void Graph::setLabelsNumericFormat(const QStringList &l)
{
    for (int axis = 0; axis < 4; axis++)
        setLabelsNumericFormat(axis, l);
}

QString Graph::saveAxesLabelsType()
{
    QString s = "AxisType\t";
    for (int i = 0; i < 4; i++) {
        auto type = axisType[i];
        s += QString::number(static_cast<int>(type));
        if (type == AxisType::Time || type == AxisType::Date || type == AxisType::DateTime
            || type == AxisType::Txt || type == AxisType::ColHeader || type == AxisType::Day
            || type == AxisType::Month)
            s += ";" + axesFormatInfo[i];
        s += "\t";
    };

    return s + "\n";
}

QString Graph::saveTicksType()
{
    QList<int> ticksTypeList = d_plot->getMajorTicksType();
    QString s = "MajorTicks\t";
    int i;
    for (i = 0; i < 4; i++)
        s += QString::number(ticksTypeList[i]) + "\t";
    s += "\n";

    ticksTypeList = d_plot->getMinorTicksType();
    s += "MinorTicks\t";
    for (i = 0; i < 4; i++)
        s += QString::number(ticksTypeList[i]) + "\t";

    return s + "\n";
}

QStringList Graph::enabledTickLabels()
{
    QStringList lst;
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        const QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
        lst << QString::number(sd->hasComponent(QwtAbstractScaleDraw::Labels));
    }
    return lst;
}

QString Graph::saveEnabledTickLabels()
{
    QString s = "EnabledTickLabels\t";
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        const QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
        s += QString::number(sd->hasComponent(QwtAbstractScaleDraw::Labels)) + "\t";
    }
    return s + "\n";
}

QString Graph::saveLabelsFormat()
{
    QString s = "LabelsFormat\t";
    for (int axis = 0; axis < QwtPlot::axisCnt; axis++) {
        s += QString::number(d_plot->axisLabelFormat(axis)) + "\t";
        s += QString::number(d_plot->axisLabelPrecision(axis)) + "\t";
    }
    return s + "\n";
}

QString Graph::saveAxesBaseline()
{
    QString s = "AxesBaseline\t";
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale)
            s += QString::number(readAxisBaselineDist(scale)) + "\t";
        else
            s += "0\t";
    }
    return s + "\n";
}

QString Graph::saveLabelsRotation()
{
    QString s = "LabelsRotation\t";
    s += QString::number(labelsRotation(QwtPlot::xBottom)) + "\t";
    s += QString::number(labelsRotation(QwtPlot::xTop)) + "\n";
    return s;
}

void Graph::setEnabledTickLabels(const QStringList &labelsOn)
{
    for (int axis = 0; axis < QwtPlot::axisCnt && axis < labelsOn.size(); axis++) {
        QwtScaleWidget *sc = d_plot->axisWidget(axis);
        if (sc) {
            QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
            if (sd)
                sd->enableComponent(QwtAbstractScaleDraw::Labels, labelsOn[axis] == "1");
        }
    }
}

void Graph::setTickLabelsOnAxis(int axis, bool on)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;
    if (QwtScaleDraw *sd = d_plot->axisScaleDraw(axis))
        sd->enableComponent(QwtAbstractScaleDraw::Labels, on);
    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->update();
    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::hasTickLabelsOnAxis(int axis) const
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return false;
    const QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
    return sd ? sd->hasComponent(QwtAbstractScaleDraw::Labels) : false;
}

void Graph::setMajorTicksType(const QList<int> &lst)
{
    if (d_plot->getMajorTicksType() == lst)
        return;

    for (int i = 0; i < (int)lst.count(); i++) {
        ScaleDraw *sd = (ScaleDraw *)d_plot->axisScaleDraw(i);
        if (lst[i] == ScaleDraw::None || lst[i] == ScaleDraw::In)
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
        else {
            sd->enableComponent(QwtAbstractScaleDraw::Ticks);
            sd->setTickLength(QwtScaleDiv::MinorTick, d_plot->minorTickLength());
            sd->setTickLength(QwtScaleDiv::MediumTick, d_plot->minorTickLength());
            sd->setTickLength(QwtScaleDiv::MajorTick, d_plot->majorTickLength());
        }
        sd->setMajorTicksStyle((ScaleDraw::TicksStyle)lst[i]);
    }
}

void Graph::setMajorTicksType(const QStringList &lst)
{
    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMajorTicksType(i, lst[i].toInt());
}

void Graph::setMinorTicksType(const QList<int> &lst)
{
    if (d_plot->getMinorTicksType() == lst)
        return;

    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMinorTicksType(i, lst[i]);
}

void Graph::setMinorTicksType(const QStringList &lst)
{
    for (int i = 0; i < (int)lst.count(); i++)
        d_plot->setMinorTicksType(i, lst[i].toInt());
}

int Graph::minorTickLength()
{
    return d_plot->minorTickLength();
}

int Graph::majorTickLength()
{
    return d_plot->majorTickLength();
}

void Graph::setAxisTicksLength(int axis, int majTicksType, int minTicksType, int minLength,
                               int majLength)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (!scale)
        return;

    d_plot->setTickLength(minLength, majLength);

    ScaleDraw *sd = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    sd->setMajorTicksStyle((ScaleDraw::TicksStyle)majTicksType);
    sd->setMinorTicksStyle((ScaleDraw::TicksStyle)minTicksType);

    if (majTicksType == ScaleDraw::None && minTicksType == ScaleDraw::None)
        sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
    else if (!sd->hasComponent(QwtAbstractScaleDraw::Labels)
             && (majTicksType == ScaleDraw::None || majTicksType == ScaleDraw::In)
             && (minTicksType == ScaleDraw::None || minTicksType == ScaleDraw::In))
        sd->enableComponent(QwtAbstractScaleDraw::Ticks, false);
    else
        sd->enableComponent(QwtAbstractScaleDraw::Ticks);

    if (majTicksType == ScaleDraw::None || majTicksType == ScaleDraw::In)
        majLength = minLength;
    if (minTicksType == ScaleDraw::None)
        minLength = 0;

    sd->setTickLength(QwtScaleDiv::MinorTick, minLength);
    sd->setTickLength(QwtScaleDiv::MediumTick, minLength);
    sd->setTickLength(QwtScaleDiv::MajorTick, majLength);
}

void Graph::setTicksLength(int minLength, int majLength)
{
    QList<int> majTicksType = d_plot->getMajorTicksType();
    QList<int> minTicksType = d_plot->getMinorTicksType();

    for (int i = 0; i < 4; i++)
        setAxisTicksLength(i, majTicksType[i], minTicksType[i], minLength, majLength);
}

void Graph::changeTicksLength(int minLength, int majLength)
{
    if (d_plot->minorTickLength() == minLength && d_plot->majorTickLength() == majLength)
        return;

    setTicksLength(minLength, majLength);

    d_plot->hide();
    for (int i = 0; i < 4; i++) {
        if (d_plot->axisEnabled(i)) {
            d_plot->enableAxis(i, false);
            d_plot->enableAxis(i, true);
        }
    }
    d_plot->replot();
    d_plot->show();

    emit modifiedGraph();
}

void Graph::showAxis(int axis, AxisType type, const QString &formatInfo, Table *table, bool axisOn,
                     int majTicksType, int minTicksType, bool labelsOn, const QColor &c, int format,
                     int prec, int rotation, int baselineDist, const QString &formula,
                     const QColor &labelsColor)
{
    d_plot->enableAxis(axis, axisOn);
    if (!axisOn)
        return;

    QList<int> majTicksTypeList = d_plot->getMajorTicksType();
    QList<int> minTicksTypeList = d_plot->getMinorTicksType();

    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);

    if (d_plot->axisEnabled(axis) == axisOn && majTicksTypeList[axis] == majTicksType
        && minTicksTypeList[axis] == minTicksType && axesColors()[axis] == COLORNAME(c)
        && axesNumColors()[axis] == COLORNAME(labelsColor)
        && prec == d_plot->axisLabelPrecision(axis) && format == d_plot->axisLabelFormat(axis)
        && labelsRotation(axis) == rotation && axisType[axis] == type
        && axesFormatInfo[axis] == formatInfo && axesFormulas[axis] == formula
        && readAxisBaselineDist(scale) == baselineDist
        && sclDraw->hasComponent(QwtAbstractScaleDraw::Labels) == labelsOn)
        return;

    applyAxisBaselineDist(scale, baselineDist);
    QPalette pal = scale->palette();
    if (pal.color(QPalette::Active, QPalette::WindowText) != c)
        pal.setColor(QPalette::WindowText, c);
    if (pal.color(QPalette::Active, QPalette::Text) != labelsColor)
        pal.setColor(QPalette::Text, labelsColor);
    scale->setPalette(pal);

    if (!labelsOn)
        sclDraw->enableComponent(QwtAbstractScaleDraw::Labels, false);
    else {
        switch (type) {
        case AxisType::Numeric:
            setLabelsNumericFormat(axis, format, prec, formula);
            break;
        case AxisType::Day:
            setLabelsDayFormat(axis, format);
            break;
        case AxisType::Month:
            setLabelsMonthFormat(axis, format);
            break;
        case AxisType::Time:
        case AxisType::Date:
        case AxisType::DateTime:
            setLabelsDateTimeFormat(axis, type, formatInfo);
            break;
        case AxisType::Txt:
            setLabelsTextFormat(axis, table, formatInfo);
            break;
        case AxisType::ColHeader:
            setLabelsColHeaderFormat(axis, table);
            break;
        }

        setAxisLabelRotation(axis, rotation);
    }

    sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const bool backboneOn = drawAxesBackbone && (axis >= 0 && axis < 4 ? d_axis_backbone[axis] : true);
    sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);

    setAxisTicksLength(axis, majTicksType, minTicksType, d_plot->minorTickLength(),
                       d_plot->majorTickLength());

    if (axisOn && (axis == QwtPlot::xTop || axis == QwtPlot::yRight))
        updateSecondaryAxis(axis); // synchronize scale divisions

    scalePicker->refresh();
    d_plot->updateLayout(); // This is necessary in order to enable/disable tick labels
    scale->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setLabelsDayFormat(int axis, int format)
{
    axisType[axis] = AxisType::Day;
    axesFormatInfo[axis] = QString::number(format);

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    WeekDayScaleDraw *sd =
            new WeekDayScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                 (WeekDayScaleDraw::NameFormat)format);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setScaleDiv(div);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsMonthFormat(int axis, int format)
{
    axisType[axis] = AxisType::Month;
    axesFormatInfo[axis] = QString::number(format);

    ScaleDraw *sd_old = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    const QwtScaleDiv div = sd_old->scaleDiv();

    MonthScaleDraw *sd =
            new MonthScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                               (MonthScaleDraw::NameFormat)format);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setScaleDiv(div);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsTextFormat(int axis, const Column *column, int startRow, int endRow)
{
    // TODO: The whole text labels functionality is very limited. Specifying
    // only one column for the labels will always mean that the labels
    // correspond to 1, 2, 3, 4, etc.. Other label mappings such as
    // a -> 1.5, b -> 4.5, c -> 16.5 (with step set to 0.5) or similar
    // require to have an additional column with numeric values.
    // This should be supported in our new plotting framework.
    if (!column)
        return;
    future::Table *table = qobject_cast<future::Table *>(column->parentAspect());
    if (!table)
        return;
    if (axis < 0 || axis > 3)
        return;

    axisType[axis] = AxisType::Txt;
    axesFormatInfo[axis] = table->name() + "_" + column->name();

    QMap<int, QString> list;
    for (int row = startRow; row <= endRow; row++)
        if (!column->isInvalid(row))
            list.insert(row + 1, column->textAt(row));
    QwtTextScaleDraw *sd = new QwtTextScaleDraw(
            *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)), list);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    sd->setMinorTicksStyle(ScaleDraw::None);
    d_plot->setAxisScaleDraw(axis, sd);
    d_plot->setAxisMaxMinor(axis, 0);
    d_plot->setAxisMaxMajor(axis, qMax(8, list.size() + 2));
    d_user_step[axis] = 1.0;
}

void Graph::setLabelsTextFormat(int axis, Table *table, const QString &columnName)
{
    Column *col = 0;
    if (!table || !(col = table->column(columnName))) {
        ApplicationWindow *aw = nullptr;
        for (QObject *p = parent(); p && !aw; p = p->parent())
            aw = qobject_cast<ApplicationWindow *>(p);
        if (!aw) {
            for (QWidget *tlw : QApplication::topLevelWidgets()) {
                if ((aw = qobject_cast<ApplicationWindow *>(tlw)) != nullptr)
                    break;
            }
        }
        if (aw && aw->isNonInteractive())
            return;

        QMessageBox::critical(
                this, tr("Internal Error"),
                tr("<html>Failed to set axis labels on Graph %1. Maybe you're trying to open a "
                   "corrupted"
                   " project file; or there's some problem within SciDAVis. Please report this"
                   " as a bug (together with detailed instructions how to reproduce this message or"
                   " the corrupted file).<p>"
                   "<a href=\"https://sourceforge.net/tracker/?group_id=199120&atid=968214>\">"
                   "bug tracker: "
                   "https://sourceforge.net/tracker/?group_id=199120&atid=968214</a></html>")
                        .arg(objectName()));
        return;
    }
    setLabelsTextFormat(axis, col, 0, col->rowCount() - 1);
}

void Graph::setLabelsColHeaderFormat(int axis, Table *table)
{
    if (!table)
        return;

    axisType[axis] = AxisType::ColHeader;
    axesFormatInfo[axis] = table->name();

    QMap<int, QString> list;
    for (int col = 0; col < table->columnCount(); col++)
        if (table->colPlotDesignation(col) == SciDAVis::Y)
            list.insert(col, table->colLabel(col));
    QwtTextScaleDraw *sd = new QwtTextScaleDraw(
            *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)), list);
    sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    d_plot->setAxisScaleDraw(axis, sd);
}

void Graph::setLabelsDateTimeFormat(int axis, AxisType type, const QString &formatInfo)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList list = formatInfo.split(";", Qt::KeepEmptyParts);
#else
    QStringList list = formatInfo.split(";", QString::KeepEmptyParts);
#endif
    if ((int)list.count() < 2 || list[1].isEmpty()) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Couldn't change the axis type to the requested format!"));
        }
        return;
    }

    const bool backboneOn = drawAxesBackbone && (axis >= 0 && axis < 4 ? d_axis_backbone[axis] : true);
    switch (type) {
    case AxisType::Time : {
        TimeScaleDraw *sd = new TimeScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                list[0].isEmpty() ? QTime(12, 0, 0, 0) : QTime::fromString(list[0].right(8)),
                list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    case AxisType::Date: {
        DateScaleDraw *sd =
                new DateScaleDraw(*static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                                  QDate::fromString(list[0].left(10), "yyyy-MM-dd"), list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    case AxisType::DateTime: {
        DateTimeScaleDraw *sd = new DateTimeScaleDraw(
                *static_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis)),
                QDateTime::fromString(list[0], "yyyy-MM-ddThh:mm:ss"), list[1]);
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->enableComponent(QwtAbstractScaleDraw::Backbone, backboneOn);
        d_plot->setAxisScaleDraw(axis, sd);
        break;
    }
    default:
        // safeguard - such an argument is invalid for this method
        return;
    }

    axisType[axis] = type;
    axesFormatInfo[axis] = formatInfo;
}

void Graph::setAxisLabelRotation(int axis, int rotation)
{
    if (axis == QwtPlot::xBottom) {
        if (rotation > 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignRight | Qt::AlignVCenter);
        else if (rotation < 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignLeft | Qt::AlignVCenter);
        else if (rotation == 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignHCenter | Qt::AlignBottom);
    } else if (axis == QwtPlot::xTop) {
        if (rotation > 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignLeft | Qt::AlignVCenter);
        else if (rotation < 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignRight | Qt::AlignVCenter);
        else if (rotation == 0)
            d_plot->setAxisLabelAlignment(axis, Qt::AlignHCenter | Qt::AlignTop);
    }
    d_plot->setAxisLabelRotation(axis, (double)rotation);
}

int Graph::labelsRotation(int axis)
{
    ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
    return (int)sclDraw->labelRotation();
}

void Graph::setYAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::yLeft);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::yLeft, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setXAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::xBottom);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::xBottom, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setRightAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::yRight);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::yRight, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTopAxisTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(QwtPlot::xTop);
    t.setFont(nf);
    d_plot->setAxisTitle(QwtPlot::xTop, t);
    d_plot->replot();
    emit modifiedGraph();
}

namespace {
void alignLinkedChildCanvasToParent(Graph *child)
{
    if (!child)
        return;
    Graph *parentLayer = child->linkedParentLayer();
    if (!parentLayer || parentLayer == child)
        return;
    MultiLayer *ml = child->multiLayerParent();
    if (ml && ml->isRestoringSnapshot())
        return;

    Plot *pPlot = parentLayer->plotWidget();
    Plot *cPlot = child->plotWidget();
    if (!pPlot || !cPlot)
        return;

    pPlot->updateLayout();
    QRect pCanvas = pPlot->plotLayout()->canvasRect();
    if (pCanvas.width() < 20 || pCanvas.height() < 20)
        return;

    cPlot->updateLayout();
    const QRect cCanvas0 = cPlot->plotLayout()->canvasRect();
    const int cL0 = qMax(0, cCanvas0.left());
    const int cT0 = qMax(0, cCanvas0.top());
    const int cR0 = qMax(0, cPlot->width() - (cCanvas0.left() + cCanvas0.width()));
    const int cB0 = qMax(0, cPlot->height() - (cCanvas0.top() + cCanvas0.height()));

    QRect pGeo = parentLayer->geometry();
    QRect targetInner = pCanvas.translated(pGeo.topLeft());

    int shiftTop = 0;
    if (targetInner.top() - cT0 < 4 && pGeo.height() - (4 - (targetInner.top() - cT0)) >= 80)
        shiftTop = 4 - (targetInner.top() - cT0);
    int shiftLeft = 0;
    if (targetInner.left() - cL0 < 4 && pGeo.width() - (4 - (targetInner.left() - cL0)) >= 80)
        shiftLeft = 4 - (targetInner.left() - cL0);

    int trimRight = 0;
    int trimBottom = 0;
    QWidget *hostCanvas = ml ? ml->canvasWidget() : parentLayer->parentWidget();
    if (hostCanvas && hostCanvas->width() >= 140) {
        const int overR =
                (targetInner.left() + targetInner.width() + shiftLeft + cR0) - (hostCanvas->width() - 4);
        if (overR > 0 && pGeo.width() - shiftLeft - overR >= 80)
            trimRight = overR;
    }
    if (hostCanvas && hostCanvas->height() >= 140) {
        const int overB =
                (targetInner.top() + targetInner.height() + shiftTop + cB0) - (hostCanvas->height() - 4);
        if (overB > 0 && pGeo.height() - shiftTop - overB >= 80)
            trimBottom = overB;
    }

    if (shiftTop > 0 || shiftLeft > 0 || trimRight > 0 || trimBottom > 0) {
        const bool prevPIgnore = parentLayer->ignoresResizeEvents();
        const bool prevPAuto = parentLayer->autoscaleFonts();
        parentLayer->setIgnoreResizeEvents(true);
        parentLayer->setAutoscaleFonts(false);
        pGeo = QRect(pGeo.x() + shiftLeft, pGeo.y() + shiftTop,
                     pGeo.width() - shiftLeft - trimRight,
                     pGeo.height() - shiftTop - trimBottom);
        parentLayer->setGeometry(pGeo);
        pPlot->resize(pGeo.size());
        pPlot->updateLayout();
        parentLayer->setAutoscaleFonts(prevPAuto);
        parentLayer->setIgnoreResizeEvents(prevPIgnore);
        pCanvas = pPlot->plotLayout()->canvasRect();
        targetInner = pCanvas.translated(parentLayer->pos());
    }

    if (targetInner.width() < 20 || targetInner.height() < 20)
        return;

    const bool prevCIgnore = child->ignoresResizeEvents();
    const bool prevCAuto = child->autoscaleFonts();
    child->setIgnoreResizeEvents(true);
    child->setAutoscaleFonts(false);
    for (int pass = 0; pass < 2; ++pass) {
        cPlot->updateLayout();
        const QRect cc = cPlot->plotLayout()->canvasRect();
        const int cl = qMax(0, cc.left());
        const int ct = qMax(0, cc.top());
        const int cr = qMax(0, cPlot->width() - (cc.left() + cc.width()));
        const int cb = qMax(0, cPlot->height() - (cc.top() + cc.height()));
        const QRect targetGeo(targetInner.left() - cl, targetInner.top() - ct,
                              qMax(40, targetInner.width() + cl + cr),
                              qMax(40, targetInner.height() + ct + cb));
        if (child->geometry() != targetGeo) {
            child->setGeometry(targetGeo);
            cPlot->resize(targetGeo.size());
        }
    }
    cPlot->updateLayout();
    child->setAutoscaleFonts(prevCAuto);
    child->setIgnoreResizeEvents(prevCIgnore);
}
} // namespace

void Graph::setAxisTitleFont(int axis, const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->axisTitle(axis);
    t.setFont(nf);
    d_plot->setAxisTitle(axis, t);
    if (d_linked_parent)
        alignLinkedChildCanvasToParent(this);
    else
        syncChildLinkedLayers();
    d_plot->replot();
    emit modifiedGraph();
}

QFont Graph::axisTitleFont(int axis)
{
    return d_plot->axisTitle(axis).font();
}

QColor Graph::axisTitleColor(int axis)
{
    QColor c;
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        c = scale->title().color();
    return c;
}

void Graph::setAxesNumColors(const QStringList &colors)
{
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QPalette pal = scale->palette();
            pal.setColor(QPalette::Text, QColor(COLORVALUE(colors[i])));
            scale->setPalette(pal);
        }
    }
}

void Graph::setAxesColors(const QStringList &colors)
{
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QPalette pal = scale->palette();
            pal.setColor(QPalette::WindowText, QColor(COLORVALUE(colors[i])));
            scale->setPalette(pal);
        }
    }
}

QString Graph::saveAxesColors()
{
    QString s = "AxesColors\t";
    QStringList colors, numColors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++) {
        colors << COLORNAME(QColor(Qt::black));
        numColors << COLORNAME(QColor(Qt::black));
    }

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::WindowText));
            numColors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::Text));
        }
    }
    s += colors.join("\t") + "\n";
    s += "AxesNumberColors\t" + numColors.join("\t") + "\n";
    return s;
}

QStringList Graph::axesColors()
{
    QStringList colors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++)
        colors << COLORNAME(QColor(Qt::black));

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::WindowText));
        }
    }
    return colors;
}

QColor Graph::axisColor(int axis)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        return scale->palette().color(QPalette::Active, QPalette::WindowText);
    else
        return QColor(Qt::black);
}

QColor Graph::axisNumbersColor(int axis)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale)
        return scale->palette().color(QPalette::Active, QPalette::Text);
    else
        return QColor(Qt::black);
}

QStringList Graph::axesNumColors()
{
    QStringList colors;
    QPalette pal;
    int i;
    for (i = 0; i < 4; i++)
        colors << COLORNAME(QColor(Qt::black));

    for (i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            pal = scale->palette();
            colors[i] = COLORNAME(pal.color(QPalette::Active, QPalette::Text));
        }
    }
    return colors;
}

void Graph::setTitleColor(const QColor &c)
{
    QwtText t = d_plot->title();
    t.setColor(c);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTitleAlignment(int align)
{
    QwtText t = d_plot->title();
    t.setRenderFlags(align);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTitleFont(const QFont &fnt)
{
    QFont nf = fnt;
    nf.setHintingPreference(QFont::PreferNoHinting);
    QwtText t = d_plot->title();
    t.setFont(nf);
    d_plot->setTitle(t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setYAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::yLeft, text);
}

void Graph::setXAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::xBottom, text);
}

void Graph::setRightAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::yRight, text);
}

void Graph::setTopAxisTitle(const QString &text)
{
    setAxisTitle(QwtPlot::xTop, text);
}

int Graph::axisTitleAlignment(int axis)
{
    return d_plot->axisTitle(axis).renderFlags();
}

void Graph::setAxesTitlesAlignment(const QStringList &align)
{
    for (int i = 0; i < 4; i++) {
        QwtText t = d_plot->axisTitle(i);
        t.setRenderFlags(align[i + 1].toInt());
        d_plot->setAxisTitle(i, t);
    }
}

void Graph::setXAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xBottom);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::xBottom, t);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setYAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yLeft);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::yLeft, t);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTopAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::xTop);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::xTop, t);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setRightAxisTitleAlignment(int align)
{
    QwtText t = d_plot->axisTitle(QwtPlot::yRight);
    t.setRenderFlags(align);
    d_plot->setAxisTitle(QwtPlot::yRight, t);

    d_plot->replot();
    emit modifiedGraph();
}

namespace {
QString firstNonEmptyCommentLine(const QString &comment)
{
    QString norm = comment;
    norm.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    norm.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = norm.split(QLatin1Char('\n'));
    for (const QString &ln : lines) {
        const QString t = ln.trimmed();
        if (!t.isEmpty())
            return t;
    }
    return QString();
}

// Cross-track contract (A3-8 / E1-4): Column::UnitFormat::Bracket == 2.
static constexpr Column::UnitFormat kUnitFormatBracket = static_cast<Column::UnitFormat>(2);

QString formatAxisBaseAndUnits(const QString &base, const QString &units, Column::UnitFormat fmt)
{
    auto stripOuterBrackets = [](QString s) -> QString {
        s = s.trimmed();
        while (s.size() >= 3) {
            const QChar f = s.front();
            const QChar l = s.back();
            const bool matched =
                    (f == QLatin1Char('(') && l == QLatin1Char(')'))
                    || (f == QLatin1Char('[') && l == QLatin1Char(']'))
                    || (f == QChar(0xFF08) && l == QChar(0xFF09))
                    || (f == QChar(0xFF3B) && l == QChar(0xFF3D));
            if (!matched)
                break;
            s = s.mid(1, s.size() - 2).trimmed();
        }
        return s;
    };

    const QString b = base.trimmed();
    QString u = stripOuterBrackets(units);
    if (b.isEmpty())
        return QString();
    if (u.isEmpty())
        return b;

    const QString uLower = u.toLower();
    const bool isDimensionless = (u == QLatin1String("-") || u == QStringLiteral("\u2013")
                                  || u == QStringLiteral("\u2014") || u == QStringLiteral("\u2212")
                                  || uLower == QLatin1String("none")
                                  || uLower == QLatin1String("dimensionless"));
    if (isDimensionless) {
        if (fmt == Column::UnitFormat::ISIJSlash)
            return b;
        if (fmt == kUnitFormatBracket)
            return QStringLiteral("%1 [-]").arg(b);
        return QStringLiteral("%1 (-)").arg(b);
    }

    const bool isArbUnits = (uLower == QLatin1String("a.u.") || uLower == QLatin1String("a.u")
                             || uLower == QLatin1String("au")
                             || uLower == QLatin1String("arb. units")
                             || uLower == QLatin1String("arb. unit")
                             || uLower == QLatin1String("arb.units")
                             || uLower == QLatin1String("arbitrary units"));
    if (isArbUnits)
        u = QStringLiteral("a.u.");

    // If Long Name already ends with "/ <Units>", do not duplicate.
    const int slashIdx = b.lastIndexOf(QLatin1Char('/'));
    if (slashIdx >= 0) {
        const QString tail = stripOuterBrackets(b.mid(slashIdx + 1));
        if (!tail.isEmpty() && tail.compare(u, Qt::CaseInsensitive) == 0)
            return b;
    }

    if (fmt == Column::UnitFormat::ISIJSlash)
        return QStringLiteral("%1 / %2").arg(b, u);
    if (fmt == kUnitFormatBracket)
        return QStringLiteral("%1 [%2]").arg(b, u);
    return QStringLiteral("%1 (%2)").arg(b, u);
}

QString extractSharedLongNamePrefix(const QStringList &longNames)
{
    if (longNames.size() < 2)
        return QString();

    auto extractStemBeforeDelim = [](const QString &s) -> QString {
        int cut = -1;
        for (int i = 0; i < s.size(); ++i) {
            const QChar ch = s.at(i);
            if (ch == QLatin1Char('(') || ch == QLatin1Char('_')) {
                cut = i;
                break;
            }
            if (ch == QLatin1Char('-') || ch == QChar(0x2013) || ch == QChar(0x2014)) {
                cut = i;
                break;
            }
        }
        return (cut > 0) ? s.left(cut).trimmed() : QString();
    };

    const QString firstStem = extractStemBeforeDelim(longNames.first());
    if (!firstStem.isEmpty()) {
        bool allSameStem = true;
        for (int i = 1; i < longNames.size(); ++i) {
            const QString st = extractStemBeforeDelim(longNames[i]);
            if (st.isEmpty() || st.compare(firstStem, Qt::CaseInsensitive) != 0) {
                allSameStem = false;
                break;
            }
        }
        if (allSameStem)
            return firstStem;
    }

    const QString &s0 = longNames.first();
    int commonLen = s0.size();
    for (int i = 1; i < longNames.size(); ++i) {
        const QString &si = longNames[i];
        int k = 0;
        const int lim = qMin(commonLen, si.size());
        while (k < lim && s0.at(k).toLower() == si.at(k).toLower())
            ++k;
        commonLen = k;
        if (commonLen == 0)
            return QString();
    }

    // Require that the divergence happens at or after a delimiter '(', '-', '_'
    QString rawPrefix = s0.left(commonLen);
    bool hasDelimBoundary = false;
    for (const QString &ln : longNames) {
        if (ln.size() > commonLen) {
            const QChar nextCh = ln.at(commonLen);
            if (nextCh == QLatin1Char('(') || nextCh == QLatin1Char('-')
                || nextCh == QLatin1Char('_') || nextCh == QChar(0x2013)
                || nextCh == QChar(0x2014)) {
                hasDelimBoundary = true;
            }
        }
    }
    while (!rawPrefix.isEmpty()) {
        const QChar tail = rawPrefix.at(rawPrefix.size() - 1);
        if (tail.isSpace() || tail == QLatin1Char('(') || tail == QLatin1Char('-')
            || tail == QLatin1Char('_') || tail == QChar(0x2013) || tail == QChar(0x2014)) {
            if (tail == QLatin1Char('(') || tail == QLatin1Char('-') || tail == QLatin1Char('_')
                || tail == QChar(0x2013) || tail == QChar(0x2014)) {
                hasDelimBoundary = true;
            }
            rawPrefix.chop(1);
        } else {
            break;
        }
    }
    return (hasDelimBoundary && rawPrefix.size() >= 2) ? rawPrefix.trimmed() : QString();
}

bool isAxisPlaceholderTitle(const QString &plain)
{
    return plain.isEmpty() || plain == QStringLiteral("X Axis Title")
            || plain == QStringLiteral("Y Axis Title") || plain == Graph::tr("X Axis Title")
            || plain == Graph::tr("Y Axis Title");
}

bool isSingleColumnHijackCandidate(const QList<Column *> &cols, const QString &plainText)
{
    const QString p = plainText.trimmed();
    if (p.isEmpty())
        return false;
    for (Column *col : cols) {
        if (!col)
            continue;
        const QString colBase =
                col->longName().trimmed().isEmpty() ? col->name().trimmed() : col->longName().trimmed();
        if (p.compare(col->formattedAxisTitle(Column::UnitFormat::Paren).trimmed(),
                      Qt::CaseInsensitive)
                    == 0
            || p.compare(col->formattedAxisTitle(Column::UnitFormat::ISIJSlash).trimmed(),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(col->formattedAxisTitle(kUnitFormatBracket).trimmed(),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(formatAxisBaseAndUnits(colBase, col->units(), Column::UnitFormat::Paren),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(formatAxisBaseAndUnits(colBase, col->units(), Column::UnitFormat::ISIJSlash),
                         Qt::CaseInsensitive)
                    == 0
            || p.compare(formatAxisBaseAndUnits(colBase, col->units(), kUnitFormatBracket),
                         Qt::CaseInsensitive)
                    == 0
            || (!col->longName().trimmed().isEmpty()
                && p.compare(col->longName().trimmed(), Qt::CaseInsensitive) == 0)
            || (!col->name().trimmed().isEmpty()
                && p.compare(col->name().trimmed(), Qt::CaseInsensitive) == 0)) {
            return true;
        }
    }
    return false;
}
} // namespace

bool Graph::isAuxiliaryOrFitCurve(const QwtPlotCurve *pc, bool skipHiddenTables) const
{
    if (!pc)
        return true;
    if (d_fit_curves.contains(const_cast<QwtPlotCurve *>(pc)))
        return true;
    const QString cTitle = pc->title().text().trimmed();
    if (cTitle.startsWith(QStringLiteral("RpcFitCurve")))
        return true;
    const DataCurve *dc = dynamic_cast<const DataCurve *>(pc);
    if (!dc || !dc->table())
        return false;
    if (dc->yColumnName().trimmed().startsWith(QStringLiteral("RpcFitCurve")))
        return true;
    const QString tblObj = dc->table()->objectName();
    const QString tblName = dc->table()->name();
    if (tblObj.startsWith(QStringLiteral("GadgetFit_"))
        || tblName.startsWith(QStringLiteral("GadgetFit_"))
        || tblName.startsWith(QStringLiteral("RpcFit"))) {
        return true;
    }
    if (skipHiddenTables && dc->table()->status() == MyWidget::Hidden)
        return true;
    return false;
}

QString Graph::resolvedAutoCurveLabel(int curveIndex) const
{
    if (curveIndex < 0 || curveIndex >= n_curves)
        return QString();
    const QwtPlotCurve *c = curve(curveIndex);
    if (!c)
        return QString();

    const DataCurve *dc = dynamic_cast<const DataCurve *>(c);
    if (!dc || !dc->table())
        return c->title().text().trimmed();

    Table *t = dc->table();
    const int yIdx = t->colIndex(dc->yColumnName());
    Column *yc = (yIdx >= 0) ? t->column(yIdx) : nullptr;

    QString cleanShort = yc ? yc->name().trimmed() : dc->yColumnName().trimmed();
    const QString prefix = QString(t->name()) + QLatin1Char('_');
    if (cleanShort.startsWith(prefix) && cleanShort.length() > prefix.length())
        cleanShort = cleanShort.mid(prefix.length());
    if (cleanShort.isEmpty()) {
        cleanShort = c->title().text().trimmed();
        if (cleanShort.startsWith(prefix) && cleanShort.length() > prefix.length())
            cleanShort = cleanShort.mid(prefix.length());
    }

    if (!yc)
        return cleanShort;

    const QString myLong = yc->longName().trimmed();
    const QString myComment = firstNonEmptyCommentLine(yc->comment());

    if (!myLong.isEmpty()) {
        int sameLongCount = 0;
        for (int j = 0; j < n_curves; ++j) {
            if (j < c_type.size() && c_type[j] == ErrorBars)
                continue;
            const QwtPlotCurve *opc = curve(j);
            if (!opc || (j != curveIndex && !opc->testItemAttribute(QwtPlotItem::Legend)))
                continue;
            if (j != curveIndex && isAuxiliaryOrFitCurve(opc, true))
                continue;
            const DataCurve *odc = dynamic_cast<const DataCurve *>(opc);
            if (!odc || !odc->table())
                continue;
            Column *oxDummy = nullptr;
            Column *oyc = nullptr;
            odc->resolveColumns(oxDummy, oyc);
            if (!oyc) {
                const int oyIdx = odc->table()->colIndex(odc->yColumnName());
                oyc = (oyIdx >= 0) ? odc->table()->column(oyIdx) : nullptr;
            }
            if (oyc && oyc->longName().trimmed().compare(myLong, Qt::CaseInsensitive) == 0)
                ++sameLongCount;
        }
        if (sameLongCount >= 2)
            return !myComment.isEmpty() ? myComment : cleanShort;

        if (!myComment.isEmpty()) {
            const int ya = (c->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight : QwtPlot::yLeft;
            QString curYPlain = unwrapNoWrapAxisTitle(axisTitle(ya)).trimmed();
            if (curYPlain.startsWith(QLatin1String("<b>"))
                && curYPlain.endsWith(QLatin1String("</b>")) && curYPlain.length() >= 7) {
                curYPlain = curYPlain.mid(3, curYPlain.length() - 7).trimmed();
            }
            const QString autoY = resolvedAutoAxisTitle(ya, d_axis_unit_format).trimmed();
            if (curYPlain.startsWith(myLong, Qt::CaseInsensitive)
                || autoY.startsWith(myLong, Qt::CaseInsensitive)) {
                return myComment;
            }
        }
    }

    if (!myLong.isEmpty())
        return myLong;
    if (!myComment.isEmpty())
        return myComment;
    return cleanShort;
}

QString Graph::resolvedAutoAxisTitle(int axis, Column::UnitFormat fmt) const
{
    if (axis < 0 || axis >= 4 || isPiePlot() || isTernaryPlot() || isPolarPlot()
        || isPoleFigurePlot())
        return QString();

    bool hasNonHiddenDataCurve = false;
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        if (isAuxiliaryOrFitCurve(pc, false))
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->table() && dc->table()->status() != MyWidget::Hidden) {
            hasNonHiddenDataCurve = true;
            break;
        }
    }

    QList<Column *> xBottomCols, xTopCols, yLeftCols, yRightCols;
    int detectedStyle = -1;

    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        const int cType = (i < c_type.size()) ? c_type[i] : -1;
        if (cType == ErrorBars)
            continue;
        if (detectedStyle < 0)
            detectedStyle = cType;

        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (!dc || !dc->table())
            continue;
        if (isAuxiliaryOrFitCurve(pc, hasNonHiddenDataCurve)
            || (!pc->testItemAttribute(QwtPlotItem::Legend) && n_curves > 1))
            continue;
        Table *t = dc->table();
        const int xIdx = t->colIndex(dc->xColumnName());
        const int yIdx = t->colIndex(dc->yColumnName());
        if (Column *xc = (xIdx >= 0 ? t->column(xIdx) : nullptr)) {
            QList<Column *> &dst = (pc->xAxis() == QwtPlot::xTop) ? xTopCols : xBottomCols;
            if (!dst.contains(xc))
                dst << xc;
        }
        if (Column *yc = (yIdx >= 0 ? t->column(yIdx) : nullptr)) {
            QList<Column *> &dst = (pc->yAxis() == QwtPlot::yRight) ? yRightCols : yLeftCols;
            if (!dst.contains(yc))
                dst << yc;
        }
    }

    auto cleanColAxisTitle = [fmt](Column *col) -> QString {
        if (!col)
            return QString();
        const QString ln = col->longName().trimmed();
        const QString base = ln.isEmpty() ? col->name().trimmed() : ln;
        return formatAxisBaseAndUnits(base, col->units(), fmt);
    };

    auto resolveMultiCol = [&](const QList<Column *> &cols) -> QString {
        if (cols.isEmpty())
            return QString();
        if (cols.size() == 1)
            return cleanColAxisTitle(cols.first());

        QString sharedUnits;
        bool unitsConflict = false;
        QStringList longNames;
        bool allHaveLongName = true;
        bool allEmptyLongName = true;
        for (Column *col : cols) {
            const QString u = col->units().trimmed();
            if (!u.isEmpty()) {
                if (sharedUnits.isEmpty())
                    sharedUnits = u;
                else if (sharedUnits.compare(u, Qt::CaseInsensitive) != 0)
                    unitsConflict = true;
            }
            const QString ln = col->longName().trimmed();
            longNames << ln;
            if (ln.isEmpty())
                allHaveLongName = false;
            else
                allEmptyLongName = false;
        }

        // Case 1: All curves on this axis share the same non-empty Long Name (case-insensitive)
        if (allHaveLongName) {
            const QString firstLong = longNames.first();
            bool allSameLong = true;
            for (int i = 1; i < longNames.size(); ++i) {
                if (longNames[i].compare(firstLong, Qt::CaseInsensitive) != 0) {
                    allSameLong = false;
                    break;
                }
            }
            if (allSameLong) {
                return unitsConflict ? firstLong
                                     : formatAxisBaseAndUnits(firstLong, sharedUnits, fmt);
            }

            // Case 2: All curves share a non-empty common prefix before '(' / '-' / '_'
            const QString sharedPrefix = extractSharedLongNamePrefix(longNames);
            if (!sharedPrefix.isEmpty()) {
                return unitsConflict ? sharedPrefix
                                     : formatAxisBaseAndUnits(sharedPrefix, sharedUnits, fmt);
            }
        } else if (allEmptyLongName) {
            const QString firstShortFull = cleanColAxisTitle(cols.first());
            bool allSameShortFull = !firstShortFull.isEmpty();
            for (int i = 1; i < cols.size(); ++i) {
                if (cleanColAxisTitle(cols[i]).compare(firstShortFull, Qt::CaseInsensitive) != 0) {
                    allSameShortFull = false;
                    break;
                }
            }
            if (allSameShortFull)
                return firstShortFull;
        }

        // Case 3: Long Names differ completely (e.g. "Uncharged" vs "H-charged").
        // Never hijack curve 1's specimen-specific Long Name! Keep existing non-placeholder
        // axis title if not a single-curve hijack, or format cleanly without curve 1's sample label.
        QString curPlain = unwrapNoWrapAxisTitle(axisTitle(axis)).trimmed();
        if (curPlain.startsWith("<b>") && curPlain.endsWith("</b>") && curPlain.length() >= 7)
            curPlain = curPlain.mid(3, curPlain.length() - 7).trimmed();

        const QString axisSym = (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
                ? QStringLiteral("X")
                : QStringLiteral("Y");
        const QString genericParen = (!unitsConflict && !sharedUnits.isEmpty())
                ? formatAxisBaseAndUnits(axisSym, sharedUnits, Column::UnitFormat::Paren)
                : QString();
        const QString genericSlash = (!unitsConflict && !sharedUnits.isEmpty())
                ? formatAxisBaseAndUnits(axisSym, sharedUnits, Column::UnitFormat::ISIJSlash)
                : QString();
        const QString genericBracket = (!unitsConflict && !sharedUnits.isEmpty())
                ? formatAxisBaseAndUnits(axisSym, sharedUnits, kUnitFormatBracket)
                : QString();

        if (!curPlain.isEmpty() && !isAxisPlaceholderTitle(curPlain)
            && !isSingleColumnHijackCandidate(cols, curPlain)
            && curPlain != genericParen && curPlain != genericSlash
            && curPlain != genericBracket) {
            return curPlain;
        }

        if (!unitsConflict && !sharedUnits.isEmpty())
            return formatAxisBaseAndUnits(axisSym, sharedUnits, fmt);
        return QString();
    };

    auto isSelfLabeledCategoryCols = [&](int catAxis, const QList<Column *> &cols) -> bool {
        if (catAxis < 0 || catAxis >= axisType.size() || axisType[catAxis] != AxisType::Txt)
            return false;
        for (Column *col : cols) {
            if (col && (!col->longName().trimmed().isEmpty() || !col->units().trimmed().isEmpty()))
                return false;
        }
        return true;
    };

    if (detectedStyle == Histogram) {
        if (axis == QwtPlot::xBottom)
            return resolveMultiCol(yLeftCols);
        if (axis == QwtPlot::yLeft)
            return tr("Counts");
        return QString();
    }
    if (detectedStyle == Box) {
        return (axis == QwtPlot::yLeft) ? resolveMultiCol(yLeftCols) : QString();
    }
    if (detectedStyle == HorizontalBars) {
        if (axis == QwtPlot::xBottom)
            return resolveMultiCol(yLeftCols);
        if (axis == QwtPlot::yLeft)
            return isSelfLabeledCategoryCols(QwtPlot::yLeft, xBottomCols)
                    ? QString()
                    : resolveMultiCol(xBottomCols);
        return QString();
    }
    if (detectedStyle == VerticalBars && axis == QwtPlot::xBottom
        && isSelfLabeledCategoryCols(QwtPlot::xBottom, xBottomCols)) {
        return QString();
    }

    switch (axis) {
    case QwtPlot::xBottom:
        return resolveMultiCol(xBottomCols);
    case QwtPlot::xTop:
        return resolveMultiCol(xTopCols);
    case QwtPlot::yLeft:
        return resolveMultiCol(yLeftCols);
    case QwtPlot::yRight:
        return resolveMultiCol(yRightCols);
    default:
        return QString();
    }
}

bool Graph::matchesAutoAxisTitleCandidate(int axis, const QString &text) const
{
    QString plain = unwrapNoWrapAxisTitle(text).trimmed();
    if (plain.startsWith("<b>") && plain.endsWith("</b>") && plain.length() >= 7)
        plain = plain.mid(3, plain.length() - 7).trimmed();

    if (isAxisPlaceholderTitle(plain))
        return true;

    const QString parenCandidate = resolvedAutoAxisTitle(axis, Column::UnitFormat::Paren);
    if (!parenCandidate.isEmpty() && plain == parenCandidate)
        return true;

    const QString slashCandidate = resolvedAutoAxisTitle(axis, Column::UnitFormat::ISIJSlash);
    if (!slashCandidate.isEmpty() && plain == slashCandidate)
        return true;

    const QString bracketCandidate = resolvedAutoAxisTitle(axis, kUnitFormatBracket);
    if (!bracketCandidate.isEmpty() && plain == bracketCandidate)
        return true;

    const QString axisSym = (axis == QwtPlot::xBottom || axis == QwtPlot::xTop)
            ? QStringLiteral("X")
            : QStringLiteral("Y");

    for (int i = 0; i < n_curves; ++i) {
        DataCurve *dc = dynamic_cast<DataCurve *>(curve(i));
        if (!dc || !dc->table())
            continue;
        Table *t = dc->table();
        const bool isHorizBar = (dc->type() == HorizontalBars);
        const bool isHorizAxis = (axis == QwtPlot::xBottom || axis == QwtPlot::xTop);
        const QString colName = (isHorizAxis != isHorizBar) ? dc->xColumnName() : dc->yColumnName();
        const int idx = t->colIndex(colName);
        if (Column *col = (idx >= 0 ? t->column(idx) : nullptr)) {
            const QString colBase = col->longName().trimmed().isEmpty() ? col->name().trimmed()
                                                                        : col->longName().trimmed();
            if (plain == col->name().trimmed() || plain == col->longName().trimmed()
                || plain == col->formattedAxisTitle(Column::UnitFormat::Paren).trimmed()
                || plain == col->formattedAxisTitle(Column::UnitFormat::ISIJSlash).trimmed()
                || plain == col->formattedAxisTitle(kUnitFormatBracket).trimmed()
                || plain == formatAxisBaseAndUnits(colBase, col->units(), Column::UnitFormat::Paren)
                || plain == formatAxisBaseAndUnits(colBase, col->units(),
                                                   Column::UnitFormat::ISIJSlash)
                || plain == formatAxisBaseAndUnits(colBase, col->units(), kUnitFormatBracket)) {
                return true;
            }
            const QString u = col->units().trimmed();
            if (!u.isEmpty()
                && (plain == formatAxisBaseAndUnits(axisSym, u, Column::UnitFormat::Paren)
                    || plain == formatAxisBaseAndUnits(axisSym, u, Column::UnitFormat::ISIJSlash)
                    || plain == formatAxisBaseAndUnits(axisSym, u, kUnitFormatBracket))) {
                return true;
            }
        }
    }
    return false;
}

void Graph::setAxisTitleAuto(int axis, bool autoSync)
{
    if (axis >= 0 && axis < 4)
        d_axis_title_auto[axis] = autoSync;
}

bool Graph::isAxisTitleAuto(int axis) const
{
    return (axis >= 0 && axis < 4) ? d_axis_title_auto[axis] : false;
}

void Graph::setAxisUnitFormat(Column::UnitFormat fmt)
{
    if (d_axis_unit_format == fmt)
        return;
    d_axis_unit_format = fmt;
    refreshAutoAxisTitlesFromColumns();
}

void Graph::refreshAutoAxisTitlesFromColumns()
{
    if (!d_plot || isPiePlot() || isTernaryPlot())
        return;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QFont af = axisTitleFont(a);
        if (af.family().trimmed().isEmpty()
            || af.hintingPreference() != QFont::PreferNoHinting) {
            if (af.family().trimmed().isEmpty())
                af.setFamily(QStringLiteral("Arial"));
            af.setHintingPreference(QFont::PreferNoHinting);
            setAxisTitleFont(a, af);
        }
    }

    QSet<int> colorBarAxes;
    const QwtPlotItemList &items = d_plot->itemList();
    for (QwtPlotItem *it : items) {
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(it);
            if (sp->hasColorScale())
                colorBarAxes.insert(sp->colorScaleAxis());
        }
    }

    bool hasNonHiddenDataCurve = false;
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        if (isAuxiliaryOrFitCurve(pc, false))
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->table() && dc->table()->status() != MyWidget::Hidden) {
            hasNonHiddenDataCurve = true;
            break;
        }
    }

    QList<Column *> axisCols[4];
    QList<Column *> allGraphCols;
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotCurve *pc = curve(i);
        if (!pc || pc->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (!dc || !dc->table())
            continue;
        if (isAuxiliaryOrFitCurve(pc, hasNonHiddenDataCurve)
            || (!pc->testItemAttribute(QwtPlotItem::Legend) && n_curves > 1))
            continue;
        Table *t = dc->table();
        const int xIdx = t->colIndex(dc->xColumnName());
        const int yIdx = t->colIndex(dc->yColumnName());
        const int xa = (pc->xAxis() == QwtPlot::xTop) ? QwtPlot::xTop : QwtPlot::xBottom;
        const int ya = (pc->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight : QwtPlot::yLeft;
        const bool isHorizBar = (dc->type() == HorizontalBars);
        if (Column *xc = (xIdx >= 0 ? t->column(xIdx) : nullptr)) {
            const int targetAx = isHorizBar ? ya : xa;
            if (!axisCols[targetAx].contains(xc))
                axisCols[targetAx] << xc;
            if (!allGraphCols.contains(xc))
                allGraphCols << xc;
        }
        if (Column *yc = (yIdx >= 0 ? t->column(yIdx) : nullptr)) {
            const int targetAy = isHorizBar ? xa : ya;
            if (!axisCols[targetAy].contains(yc))
                axisCols[targetAy] << yc;
            if (!allGraphCols.contains(yc))
                allGraphCols << yc;
        }
    }

    const bool prevRefreshing = d_refreshing_auto_titles;
    d_refreshing_auto_titles = true;
    bool anyChanged = false;

    for (int a = 0; a < 4; ++a) {
        if (!d_axis_title_auto[a] || colorBarAxes.contains(a))
            continue;
        if ((a == QwtPlot::xTop || a == QwtPlot::yRight) && !d_plot->axisEnabled(a))
            continue;

        QString nextTitle = resolvedAutoAxisTitle(a, d_axis_unit_format);
        if (nextTitle.isEmpty()) {
            QString curPlain = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
            if (curPlain.startsWith("<b>") && curPlain.endsWith("</b>") && curPlain.length() >= 7)
                curPlain = curPlain.mid(3, curPlain.length() - 7).trimmed();
            const bool shouldClearHijack = isAxisPlaceholderTitle(curPlain)
                    || (axisCols[a].size() >= 2
                        && isSingleColumnHijackCandidate(axisCols[a], curPlain))
                    || (a < axisType.size() && axisType[a] == AxisType::Txt
                        && isSingleColumnHijackCandidate(allGraphCols, curPlain))
                    || ((a == QwtPlot::yRight || a == QwtPlot::xTop) && axisCols[a].isEmpty()
                        && isSingleColumnHijackCandidate(allGraphCols, curPlain));
            if (shouldClearHijack && !curPlain.isEmpty()) {
                QwtText t = d_plot->axisTitle(a);
                t.setText(QString());
                d_plot->setAxisTitle(a, t);
                anyChanged = true;
            }
            continue;
        }

        nextTitle = formatScientificMarkupTokens(nextTitle);
        const QString wrappedNext = wrapRichAxisTitleNoWrap(nextTitle);
        const QString curRawTitle = d_plot->axisTitle(a).text();

        if (curRawTitle != wrappedNext) {
            QwtText t = d_plot->axisTitle(a);
            QFont f = t.font();
            if (f.family().trimmed().isEmpty())
                f = QFont(QStringLiteral("Arial"), 14, QFont::Normal, false);
            f.setHintingPreference(QFont::PreferNoHinting);
            t.setFont(f);
            t.setText(wrappedNext);
            d_plot->setAxisTitle(a, t);
            if (a == QwtPlot::yRight && !nextTitle.trimmed().isEmpty()) {
                if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight))
                    sw->setSpacing(6);
            }
            anyChanged = true;
        }
    }

    if (Legend *leg = legend()) {
        QStringList lines = leg->text().split('\n');
        bool legChanged = false;
        for (int li = 0; li < lines.size(); ++li) {
            const QString line = lines[li];
            if (!line.startsWith("\\c{"))
                continue;
            const int closeBrace = line.indexOf('}');
            if (closeBrace <= 3)
                continue;
            bool ok = false;
            const int oneBased = line.mid(3, closeBrace - 3).toInt(&ok);
            if (!ok || oneBased < 1 || oneBased > n_curves)
                continue;
            const int ci = oneBased - 1;
            DataCurve *dc = dynamic_cast<DataCurve *>(curve(ci));
            if (!dc || !dc->table())
                continue;
            Table *t = dc->table();
            const int yIdx = t->colIndex(dc->yColumnName());
            Column *yc = (yIdx >= 0) ? t->column(yIdx) : nullptr;
            if (!yc)
                continue;

            const QString currentLabel = line.mid(closeBrace + 1);
            const QString colShort = yc->name().trimmed();
            const QString colLong = yc->longName().trimmed();
            const QString colComment = firstNonEmptyCommentLine(yc->comment());
            const QString desiredLabel = resolvedAutoCurveLabel(ci);
            if (desiredLabel.isEmpty())
                continue;
            const QString prevAuto = d_last_auto_curve_labels.value(ci);

            const QString pfx = QString(t->name()) + "_";
            const QString yColFull = dc->yColumnName();
            const bool hasStrippedTitle =
                    yColFull.startsWith(pfx) && yColFull.length() > pfx.length();
            const QString strippedTitle = hasStrippedTitle ? yColFull.mid(pfx.length()) : QString();

            const bool hasRichMeta = !colLong.isEmpty() || !colComment.isEmpty();
            if (currentLabel == colShort || (hasStrippedTitle && currentLabel == strippedTitle)
                || (hasRichMeta && currentLabel == yColFull)
                || (!colLong.isEmpty() && currentLabel == colLong)
                || (!colComment.isEmpty() && currentLabel == colComment)
                || (!prevAuto.isEmpty() && currentLabel == prevAuto)) {
                int availCw = 320;
                if (d_plot && d_plot->plotLayout()
                    && d_plot->plotLayout()->canvasRect().width() > 20) {
                    availCw = d_plot->plotLayout()->canvasRect().width();
                } else if (d_plot && d_plot->canvas() && d_plot->canvas()->width() > 20) {
                    availCw = d_plot->canvas()->width();
                }
                QFont measureLegFnt = leg->font();
                if (availCw < 260 && measureLegFnt.pointSize() > 9)
                    measureLegFnt.setPointSize(9);
                const bool desiredOverflowsCanvas =
                        !currentLabel.trimmed().isEmpty()
                        && desiredLabel.size() > currentLabel.trimmed().size()
                        && (QFontMetrics(measureLegFnt).horizontalAdvance(desiredLabel) + 38
                            > availCw);
                if (!desiredOverflowsCanvas) {
                    d_last_auto_curve_labels[ci] = desiredLabel;
                    const QString updatedLine =
                            QStringLiteral("\\c{%1}%2").arg(oneBased).arg(desiredLabel);
                    if (lines[li] != updatedLine) {
                        lines[li] = updatedLine;
                        legChanged = true;
                    }
                }
            } else if (currentLabel == desiredLabel) {
                d_last_auto_curve_labels[ci] = desiredLabel;
            }
        }
        if (legChanged) {
            leg->setText(lines.join("\n"));
            anyChanged = true;
        }
    }

    d_refreshing_auto_titles = prevRefreshing;
    if (anyChanged) {
        d_plot->replot();
        emit modifiedGraph();
    }
}

void Graph::setAxisTitle(int axis, const QString &text)
{
    QString unesc = text;
    unesc.replace(QLatin1String("&#9;"), QLatin1String("\t"));
    unesc.replace(QLatin1String("&#60;"), QLatin1String("<"));
    const QString rawInput = unwrapNoWrapAxisTitle(unesc);
    QString finalText = rawInput;
    if (axis >= 0 && axis < 4 && !d_refreshing_auto_titles) {
        if (rawInput.trimmed().isEmpty()) {
            d_axis_title_auto[axis] = false;
        } else if (d_axis_title_auto[axis]) {
            d_axis_title_auto[axis] = matchesAutoAxisTitleCandidate(axis, rawInput);
            if (d_axis_title_auto[axis]) {
                QString stripped = rawInput.trimmed();
                if (stripped.startsWith("<b>") && stripped.endsWith("</b>")
                    && stripped.length() >= 7) {
                    stripped = stripped.mid(3, stripped.length() - 7).trimmed();
                }
                const QString autoTitle = resolvedAutoAxisTitle(axis, d_axis_unit_format);
                finalText = !autoTitle.isEmpty() ? autoTitle : stripped;
            }
        }
    }

    if (!finalText.trimmed().isEmpty())
        finalText = formatScientificMarkupTokens(finalText);

    QwtText t = d_plot->axisTitle(axis);
    QFont f = t.font();
    if (f.family().trimmed().isEmpty())
        f = QFont("Arial", 14, QFont::Normal, false);
    f.setHintingPreference(QFont::PreferNoHinting);
    t.setFont(f);
    t.setText(wrapRichAxisTitleNoWrap(finalText));
    d_plot->setAxisTitle(axis, t);
    if (axis == QwtPlot::yRight && !finalText.trimmed().isEmpty()) {
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight))
            sw->setSpacing(6);
    }
    if (d_linked_parent)
        alignLinkedChildCanvasToParent(this);
    else
        syncChildLinkedLayers();
    d_plot->replot();
    emit modifiedGraph();
}

QString Graph::axisTitle(int axis) const
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return unwrapNoWrapAxisTitle(d_plot->axisTitle(axis).text());
}

QStringList Graph::scalesTitles()
{
    QStringList scaleTitles;
    int axis;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        switch (i) {
        case 0:
            axis = 2;
            scaleTitles << axisTitle(axis);
            break;

        case 1:
            axis = 0;
            scaleTitles << axisTitle(axis);
            break;

        case 2:
            axis = 3;
            scaleTitles << axisTitle(axis);
            break;

        case 3:
            axis = 1;
            scaleTitles << axisTitle(axis);
            break;
        }
    }
    return scaleTitles;
}

void Graph::updateSecondaryAxis(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt || hasColorScaleOnAxis(axis))
        return;

    if (d_weibull_prob_axis[axis] || !d_custom_tick_positions[axis].isEmpty()
        || !d_custom_category_labels[axis].isEmpty()) {
        syncCustomAndWeibullAxes();
        return;
    }

    for (int i = 0; i < n_curves; i++) {
        QwtPlotItem *it = plotItem(i);
        if (!it)
            continue;

        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = (Spectrogram *)it;
            if (sp->colorScaleAxis() == axis)
                return;
        }

        if ((axis == QwtPlot::yRight && it->yAxis() == QwtPlot::yRight)
            || (axis == QwtPlot::xTop && it->xAxis() == QwtPlot::xTop))
            return;
    }

    int a = QwtPlot::xBottom;
    if (axis == QwtPlot::yRight)
        a = QwtPlot::yLeft;

    if (!d_plot->axisEnabled(a))
        return;

    QwtScaleEngine *se = d_plot->axisScaleEngine(a);
    const QwtScaleDiv *sd = d_plot->axisScaleDiv(a);
    if (sd && !sd->isValid()) {
        d_plot->updateAxes();
        se = d_plot->axisScaleEngine(a);
        sd = d_plot->axisScaleDiv(a);
    }
    if (!se || !sd)
        return;

    QwtScaleEngine *sc_engine = 0;
    if (d_axis_breaks[a].enabled && !d_axis_breaks[axis].enabled)
        sc_engine = new PiecewiseBreakScaleEngine(d_axis_breaks[a]);
    else if (Plot::isLog10ScaleEngine(se))
        sc_engine = new QwtLog10ScaleEngine();
    else
        sc_engine = new QwtLinearScaleEngine();

    if (se->testAttribute(QwtScaleEngine::Inverted))
        sc_engine->setAttribute(QwtScaleEngine::Inverted);

    d_plot->setAxisScaleEngine(axis, sc_engine);
    d_plot->setAxisScaleDiv(axis, *sd);

    if (dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(a)) != nullptr) {
        if (QwtScaleDraw *oppSd = d_plot->axisScaleDraw(axis))
            oppSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
        d_plot->setMinorTicksType(axis, ScaleDraw::None);
        d_plot->setMajorTicksType(axis, ScaleDraw::None);
    }

    if (ScaleDraw *mirrorSd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        const double lo = sd ? qMin(sd->lowerBound(), sd->upperBound()) : 0.0;
        const double hi = sd ? qMax(sd->lowerBound(), sd->upperBound()) : 0.0;
        const double bf = qMin(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
        const double bt = qMax(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
        if (d_axis_breaks[a].enabled && !d_axis_breaks[axis].enabled && bf > lo && bt < hi)
            mirrorSd->setAxisBreakIndicator(true, bf, bt, d_axis_breaks[a].log10Before,
                                            d_axis_breaks[a].log10After);
        else if (!d_axis_breaks[axis].enabled)
            mirrorSd->setAxisBreakIndicator(false, 0.0);
    }

    d_user_step[axis] = d_user_step[a];

    QwtScaleWidget *scale = d_plot->axisWidget(a);
    int start = scale->startBorderDist();
    int end = scale->endBorderDist();

    scale = d_plot->axisWidget(axis);
    scale->setMinBorderDist(start, end);
}

namespace {
QByteArray toriUserLockedAxisProp(int axis)
{
    return QByteArray("_toriUserLockedAxis_") + QByteArray::number(axis);
}

bool isToriAxisUserLocked(const QwtPlot *plot, int axis)
{
    return plot && axis >= 0 && axis < QwtPlot::axisCnt
            && plot->property(toriUserLockedAxisProp(axis).constData()).toBool();
}

void setToriAxisUserLocked(QwtPlot *plot, int axis, bool locked)
{
    if (plot && axis >= 0 && axis < QwtPlot::axisCnt)
        plot->setProperty(toriUserLockedAxisProp(axis).constData(), locked);
}
} // namespace

bool Graph::isAxisUserLocked(int axis) const
{
    return isToriAxisUserLocked(d_plot, axis);
}

void Graph::setAxisUserLocked(int axis, bool locked)
{
    setToriAxisUserLocked(d_plot, axis, locked);
}

void Graph::cancelInlineScaleEditor()
{
    if (scalePicker)
        scalePicker->cancelInlineScaleEditor();
}

void Graph::applyZeroSnapOnAutoAxes()
{
    if (!d_plot || isPiePlot() || isTernaryPlot())
        return;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        if (isToriAxisUserLocked(d_plot, a) || !d_plot->axisAutoScale(a)
            || d_axis_breaks[a].enabled || hasColorScaleOnAxis(a)) {
            continue;
        }
        QwtScaleEngine *se = d_plot->axisScaleEngine(a);
        if (!se || Plot::isLog10ScaleEngine(se) || dynamic_cast<FormulaLinkedScaleEngine *>(se))
            continue;

        const bool isX = (a == QwtPlot::xBottom || a == QwtPlot::xTop);
        bool hasData = false;
        double dataMin = 0.0;
        double dataMax = 0.0;
        auto considerVal = [&](double v) {
            if (!std::isfinite(v))
                return;
            if (!hasData) {
                dataMin = dataMax = v;
                hasData = true;
            } else {
                if (v < dataMin)
                    dataMin = v;
                if (v > dataMax)
                    dataMax = v;
            }
        };

        for (int i = 0; i < n_curves; ++i) {
            const int cType = (i < c_type.size()) ? c_type[i] : -1;
            if (cType == Pie || cType == Box || cType == VerticalBars || cType == HorizontalBars
                || cType == Histogram) {
                continue;
            }
            QwtPlotItem *it = plotItem(i);
            if (!it || !it->isVisible() || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
                continue;
            if ((isX && it->xAxis() != a) || (!isX && it->yAxis() != a))
                continue;
            QwtPlotCurve *cv = curve(i);
            if (!cv)
                continue;
            const DataCurve *dc = dynamic_cast<const DataCurve *>(cv);
            const int n = cv->dataSize();
            if (cType == ErrorBars) {
                auto *eb = dynamic_cast<QwtErrorPlotCurve *>(cv);
                if (!eb || !eb->masterCurve())
                    continue;
                const bool matchesDir = (isX && eb->direction() == QwtErrorPlotCurve::Horizontal)
                        || (!isX && eb->direction() == QwtErrorPlotCurve::Vertical);
                if (!matchesDir)
                    continue;
                for (int k = 0; k < n; ++k) {
                    const double baseV = isX ? eb->x(k) : eb->y(k);
                    const double errV = eb->errorValue(k);
                    if (std::isfinite(baseV) && std::isfinite(errV)) {
                        if (eb->minusSide())
                            considerVal(baseV - errV);
                        if (eb->plusSide())
                            considerVal(baseV + errV);
                    }
                }
                continue;
            }
            Column *dcXCol = nullptr;
            Column *dcYCol = nullptr;
            const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
            if (dcHasMasks)
                dc->resolveColumns(dcXCol, dcYCol);
            for (int k = 0; k < n; ++k) {
                if (dcHasMasks && dc->isPointMaskedFast(k, dcXCol, dcYCol))
                    continue;
                considerVal(isX ? cv->x(k) : cv->y(k));
            }
        }

        const bool snapZero = hasData && dataMin >= 0.0 && dataMax > 0.0
                && dataMin <= 0.06 * dataMax;
        se->setReference(0.0);
        se->setAttribute(QwtScaleEngine::IncludeReference, snapZero);
    }
}

void Graph::setAutoScale()
{
    const MarkerAxisState axesBefore = markerAxisState();
    if (!d_applying_zoom_history)
        recordZoomHistorySnapshot();
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        setToriAxisUserLocked(d_plot, i, false);
        d_plot->setAxisAutoScale(i);
    }
    applyZeroSnapOnAutoAxes();

    d_plot->replot();
    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();
    updateScale(axesBefore);
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();

    emit modifiedGraph();
}

void Graph::setScale(int axis, double start, double end, double step, int majorTicks,
                     int minorTicks, int type, bool inverted)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt || !std::isfinite(start) || !std::isfinite(end))
        return;

    constexpr double kMaxSafeScaleBound = 1.0e300;
    constexpr double kClampedOverflowBound = 1.0e150;
    if (std::fabs(start) > kMaxSafeScaleBound || std::fabs(end) > kMaxSafeScaleBound
        || !std::isfinite(end - start)) {
        start = qBound(-kClampedOverflowBound, start, kClampedOverflowBound);
        end = qBound(-kClampedOverflowBound, end, kClampedOverflowBound);
        if (std::fabs(end - start) < 1e-300) {
            start -= 1.0;
            end += 1.0;
        }
    }
    if (!std::isfinite(step) || step < 0.0
        || (step > 0.0 && std::fabs(end - start) / step > 2000.0)) {
        step = 0.0;
    }
    majorTicks = qBound(1, majorTicks, 100);
    minorTicks = qBound(0, minorTicks, 50);

    const MarkerAxisState axesBefore = markerAxisState();
    setToriAxisUserLocked(d_plot, axis, true);
    d_plot->setAxisScale(axis, inverted ? qMax(start, end) : qMin(start, end),
                         inverted ? qMin(start, end) : qMax(start, end), step);

    QwtScaleEngine *sc_engine = 0;
    if (type)
        sc_engine = new QwtLog10ScaleEngine();
    else
        sc_engine = new QwtLinearScaleEngine();

    int max_min_intervals = minorTicks;
    if (minorTicks >= 1)
        max_min_intervals = minorTicks + 1;

    QwtScaleDiv div = sc_engine->divideScale(qMin(start, end), qMax(start, end), majorTicks,
                                             max_min_intervals, step);
    if (!type && minorTicks == 1)
        ensureLinearHalfStepMinorTicks(div);
    d_plot->setAxisMaxMajor(axis, majorTicks);
    d_plot->setAxisMaxMinor(axis, minorTicks);

    if (inverted) {
        sc_engine->setAttribute(QwtScaleEngine::Inverted);
        div.invert();
    }

    d_plot->setAxisScaleEngine(axis, sc_engine);
    d_plot->setAxisScaleDiv(axis, div);

    if (!d_weibull_prob_axis[axis] && d_custom_tick_positions[axis].isEmpty()) {
        if (type && axisType[axis] == AxisType::Numeric
            && d_plot->axisLabelFormat(axis) == Plot::Automatic
            && d_plot->axisScaleDraw(axis)->hasComponent(QwtAbstractScaleDraw::Labels)) {
            setLabelsNumericFormat(axis, Plot::Superscripts, 0, axesFormulas[axis]);
        } else if (!type && axisType[axis] == AxisType::Numeric
                   && d_plot->axisLabelFormat(axis) == Plot::Superscripts
                   && d_plot->axisScaleDraw(axis)->hasComponent(QwtAbstractScaleDraw::Labels)) {
            setLabelsNumericFormat(axis, Plot::Automatic, 4, axesFormulas[axis]);
        }
    }

    // A break that the new range cannot show is kept (suspended) instead of being dropped,
    // and restored automatically once the axis range contains it again (C2-#7).
    if (!d_axis_breaks[axis].enabled && d_suspended_axis_breaks[axis].enabled) {
        d_axis_breaks[axis] = d_suspended_axis_breaks[axis];
        d_axis_breaks[axis].enabled = true;
        d_suspended_axis_breaks[axis] = AxisBreakSpec();
    }
    if (d_axis_breaks[axis].enabled) {
        const double lo = qMin(start, end);
        const double hi = qMax(start, end);
        const double bf = qMin(d_axis_breaks[axis].breakFrom, d_axis_breaks[axis].breakTo);
        const double bt = qMax(d_axis_breaks[axis].breakFrom, d_axis_breaks[axis].breakTo);
        const bool isLog = (type != 0);
        const bool logBefore = isLog || d_axis_breaks[axis].log10Before;
        const bool logAfter = isLog || d_axis_breaks[axis].log10After;
        const bool validBreak = (bf > lo && bt < hi && bt > bf + 1e-15)
                && (!logBefore || (lo > 0.0 && bf > 0.0))
                && (!logAfter || (bt > 0.0 && hi > 0.0));
        if (!validBreak) {
            d_suspended_axis_breaks[axis] = d_axis_breaks[axis];
            d_axis_breaks[axis].enabled = false;
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
                sd->setAxisBreakIndicator(false, 0.0);
            const int mirrorAxis = (axis == QwtPlot::xBottom)
                    ? QwtPlot::xTop
                    : ((axis == QwtPlot::yLeft) ? QwtPlot::yRight : -1);
            if (mirrorAxis >= 0 && !d_axis_breaks[mirrorAxis].enabled) {
                if (ScaleDraw *msd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(mirrorAxis)))
                    msd->setAxisBreakIndicator(false, 0.0);
            }
            if (QWidget *tw = window()) {
                if (QMainWindow *mw = qobject_cast<QMainWindow *>(tw)) {
                    if (mw->statusBar())
                        mw->statusBar()->showMessage(
                                tr("Axis break is outside the current axis range; it is kept "
                                   "and will reappear when the range contains it again."),
                                6000);
                }
            }
        } else {
            if (isLog) {
                d_axis_breaks[axis].log10Before = true;
                d_axis_breaks[axis].log10After = true;
            }
            int effMaj = majorTicks;
            if (axisFont(axis).pointSize() >= 20)
                effMaj = qMin(effMaj, 5);
            const int effMinIntervals =
                    (d_axis_breaks[axis].log10Before || d_axis_breaks[axis].log10After)
                    ? qMax(9, max_min_intervals)
                    : max_min_intervals;
            PiecewiseBreakScaleEngine *brkEng = new PiecewiseBreakScaleEngine(d_axis_breaks[axis]);
            QwtScaleDiv brkDiv = brkEng->divideScale(lo, hi, effMaj, effMinIntervals, step);
            if (inverted) {
                brkEng->setAttribute(QwtScaleEngine::Inverted);
                brkDiv.invert();
            }
            d_plot->setAxisScaleEngine(axis, brkEng);
            d_plot->setAxisScaleDiv(axis, brkDiv);
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
                sd->setAxisBreakIndicator(true, bf, bt, d_axis_breaks[axis].log10Before,
                                          d_axis_breaks[axis].log10After);
            }
        }
    }

    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();

    d_user_step[axis] = step;
    syncCustomAndWeibullAxes();

    if (axis == QwtPlot::xBottom || axis == QwtPlot::yLeft) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    d_plot->updateAxes();
    if (!ignoreResize && !d_linked_parent && !d_syncing_link) {
        if (MultiLayer *ml = multiLayerParent()) {
            if (ml->keepAspectRatio()) {
                static thread_local bool s_inSetScaleArrange = false;
                if (!s_inSetScaleArrange) {
                    struct ScopedBoolReset {
                        bool &flag;
                        explicit ScopedBoolReset(bool &f) : flag(f) { flag = true; }
                        ~ScopedBoolReset() { flag = false; }
                    } guard(s_inSetScaleArrange);
                    ml->arrangeLayers(false, false);
                }
            }
        }
    }
    // keep markers on canvas area (also performs the single d_plot->replot())
    updateMarkersBoundingRect(axesBefore);
    syncChildLinkedLayers();
}

QStringList Graph::analysableCurvesList()
{
    QStringList cList;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotCurve *c = d_plot->curve(c_keys[i]);
        if (c && c_type[i] != ErrorBars)
            cList << c->title().text();
    }
    return cList;
}

QStringList Graph::curvesList()
{
    QStringList cList;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotCurve *c = d_plot->curve(c_keys[i]);
        if (c)
            cList << c->title().text();
    }
    return cList;
}

QStringList Graph::plotItemsList() const
{
    QStringList cList;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (it)
            cList << it->title().text();
    }
    return cList;
}

void Graph::copyImage()
{
    ScopedUnzoomForExport unzoomGuard(multiLayerParent()); // copy as at 100 %
    MultiLayer *ml = multiLayerParent();
    const int pageDpi = (ml && ml->pageDpi() > 0) ? ml->pageDpi() : 96;
    const double scaleFactor = double(pageDpi) / 96.0;
    const QSize baseSize = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
    const QSize targetSize(qMax(1, qRound(baseSize.width() * scaleFactor)),
                           qMax(1, qRound(baseSize.height() * scaleFactor)));
    QImage image(targetSize, QImage::Format_ARGB32);
    const QColor bgCol = d_plot ? d_plot->paletteBackgroundColor() : QColor(Qt::white);
    const bool transparent = bgCol.isValid() && bgCol.alpha() == 0;
    image.fill(transparent ? Qt::transparent : Qt::white);
    {
        QPainter p(&image);
        exportPainter(p, false, QRect(QPoint(0, 0), baseSize), targetSize);
        p.end();
    }
    const int dpm = qRound(double(pageDpi) / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);
    QApplication::clipboard()->setImage(image);
}

bool Graph::exportToFileChecked(const QString &fileName, QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    const QString lowerName = fileName.toLower();
    if (lowerName.endsWith(".eps") || lowerName.endsWith(".pdf") || lowerName.endsWith(".ps")) {
        return exportVectorChecked(fileName, 0, true, true, QPageSize(QPageSize::Custom),
                                   QPageLayout::Portrait, errOut);
    } else if (lowerName.endsWith(".svg")) {
        return exportSVGChecked(fileName, errOut);
    } else if (lowerName.endsWith(".emf")) {
        return exportImageChecked(fileName, -1, false, 0, errOut);
    } else {
        QList<QByteArray> list = QImageWriter::supportedImageFormats();
        for (int i = 0; i < list.count(); i++) {
            if (lowerName.endsWith("." + QString::fromLatin1(list[i]).toLower())) {
                return exportImageChecked(fileName, -1, false, 0, errOut);
            }
        }
        if (errOut)
            *errOut = tr("File format not handled, operation aborted!");
        return false;
    }
}

void Graph::exportToFile(const QString &fileName)
{
    QString err;
    if (!exportToFileChecked(fileName, &err)) {
        if (!ApplicationWindow::isNonInteractiveContext(this))
            QMessageBox::critical(this, tr("Error"),
                                  err.isEmpty() ? tr("Export failed.") : err);
    }
}

bool Graph::exportImageChecked(const QString &fileName, int quality, bool transparent, int dpi,
                               QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
#ifdef Q_OS_WIN
        OleRenderedGraph emf = SciDAVisEmf::renderMultiLayer(multiLayerParent(), this, false);
        if (emf.emfBytes.isEmpty()) {
            if (errOut)
                *errOut = tr("Failed to render EMF graphics.");
            return false;
        }
        QFile f(fileName);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (errOut)
                *errOut = f.errorString().isEmpty()
                        ? tr("Could not open file for writing: %1").arg(fileName)
                        : f.errorString();
            return false;
        }
        const qint64 written = f.write(emf.emfBytes);
        const bool flushOk = f.flush();
        const QFile::FileError ferr = f.error();
        const QString ferrStr = f.errorString();
        f.close();
        if (written != emf.emfBytes.size() || !flushOk || ferr != QFile::NoError) {
            if (errOut)
                *errOut = ferrStr.isEmpty() ? tr("Failed to write complete EMF file.") : ferrStr;
            return false;
        }
        return true;
#else
        if (errOut)
            *errOut = tr("EMF export is only supported on Windows.");
        return false;
#endif
    }

    // The layer is exported as at 100 % whatever the Ctrl+wheel view zoom is.
    ScopedUnzoomForExport unzoomGuard(multiLayerParent());
    const int effectiveDpi = (dpi > 0) ? dpi : 96;
    const double scaleFactor = double(effectiveDpi) / 96.0;
    const QSize baseSize = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
    const QSize targetSize(qMax(1, qRound(baseSize.width() * scaleFactor)),
                           qMax(1, qRound(baseSize.height() * scaleFactor)));

    QImage image(targetSize, QImage::Format_ARGB32);
    image.fill(transparent ? Qt::transparent : Qt::white);

    QPalette origPlotPal;
    bool origAutoFill = false;
    QColor origCanvasBg;
    if (transparent && d_plot) {
        origPlotPal = d_plot->palette();
        origAutoFill = d_plot->autoFillBackground();
        origCanvasBg = d_plot->canvasBackground();
        QPalette transPal = origPlotPal;
        QColor winCol = transPal.color(QPalette::Window);
        winCol.setAlpha(0);
        transPal.setColor(QPalette::Window, winCol);
        d_plot->setPalette(transPal);
        d_plot->setAutoFillBackground(false);
        QColor cbg = origCanvasBg;
        cbg.setAlpha(0);
        d_plot->setCanvasBackground(cbg);
    }

    {
        QPainter p(&image);
        exportPainter(p, false, QRect(QPoint(0, 0), baseSize), targetSize);
        p.end();
    }

    if (transparent && d_plot) {
        d_plot->setPalette(origPlotPal);
        d_plot->setAutoFillBackground(origAutoFill);
        d_plot->setCanvasBackground(origCanvasBg);
    }

    const int dpm = qRound(double(effectiveDpi) / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);

    QImageWriter writer(fileName);
    if (fileName.endsWith(".tif", Qt::CaseInsensitive)
        || fileName.endsWith(".tiff", Qt::CaseInsensitive)) {
        writer.setCompression(1);
    }
    const bool isPng = fileName.endsWith(".png", Qt::CaseInsensitive);
    if (quality >= 0 && !(isPng && quality >= 95))
        writer.setQuality(quality);
    if (!writer.write(image)) {
        if (errOut) {
            *errOut = writer.errorString().isEmpty()
                    ? tr("Failed to write image file: %1").arg(fileName)
                    : writer.errorString();
        }
        return false;
    }
    return true;
}

void Graph::exportImage(const QString &fileName, int quality, bool transparent, int dpi)
{
    (void)exportImageChecked(fileName, quality, transparent, dpi, nullptr);
}

bool Graph::exportVectorChecked(const QString &fileName, int res, bool color, bool keepAspect,
                                QPageSize pageSize, QPageLayout::Orientation orientation,
                                QString *errOut)
{
    if (fileName.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }

    if (fileName.endsWith(".emf", Qt::CaseInsensitive)) {
        return exportImageChecked(fileName, -1, false, 0, errOut);
    }

    ScopedUnzoomForExport unzoomGuard(multiLayerParent()); // export as at 100 %
    QPrinter printer;
    printer.setCreator("ToriGraph");
    printer.setFullPage(true);

    QString outPath = fileName;
    printer.setOutputFileName(outPath);
    if (fileName.endsWith(".eps", Qt::CaseInsensitive)
        || fileName.endsWith(".ps", Qt::CaseInsensitive)) {
#if QT_VERSION >= 0x050000
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(this, tr("Warning"),
                                 tr("Output in postscript format is not available for Qt5, using PDF"));
        }
        printer.setOutputFormat(QPrinter::PdfFormat);
        outPath = fileName + ".pdf";
        printer.setOutputFileName(outPath);
#else
        printer.setOutputFormat(QPrinter::PostScriptFormat);
#endif
    }

    if (printer.outputFormat() == QPrinter::PdfFormat)
        printer.setResolution(96);
    else if (res > 0)
        printer.setResolution(res);

    if (color)
        printer.setColorMode(QPrinter::Color);
    else
        printer.setColorMode(QPrinter::GrayScale);

    if (pageSize == QPageSize(QPageSize::Custom)) {
        const QSize basePx = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
        const QSizeF ptSize(basePx.width() * 72.0 / 96.0, basePx.height() * 72.0 / 96.0);
        printer.setPageSize(QPageSize(ptSize, QPageSize::Point));
    } else {
        printer.setPageOrientation(orientation);
        printer.setPageSize(pageSize);
    }

    const QSize basePx = (d_plot && !d_plot->size().isEmpty()) ? d_plot->size() : size();
    QPainter p;
    if (!p.begin(&printer)) {
        if (errOut)
            *errOut = tr("Could not open vector output file for writing: %1").arg(outPath);
        return false;
    }
    exportPainter(p, keepAspect, QRect(QPoint(0, 0), basePx),
                  QSize(printer.width(), printer.height()));
    const bool endOk = p.end();
    QFileInfo fi(outPath);
    if (!endOk || !fi.exists() || fi.size() <= 0) {
        if (errOut)
            *errOut = tr("Failed to write vector output file: %1").arg(outPath);
        return false;
    }
    return true;
}

void Graph::exportVector(const QString &fileName, int res, bool color, bool keepAspect,
                         QPageSize pageSize, QPageLayout::Orientation orientation)
{
    QString err;
    if (!exportVectorChecked(fileName, res, color, keepAspect, pageSize, orientation, &err)) {
        if (!ApplicationWindow::isNonInteractiveContext(this) && fileName.isEmpty())
            QMessageBox::critical(this, tr("Error"), err);
    }
}

void Graph::print()
{
    QPrinter printer;
    printer.setColorMode(QPrinter::Color);
    printer.setFullPage(true);

    // printing should preserve plot aspect ratio, if possible
    double aspect = double(d_plot->width()) / double(d_plot->height());
    if (aspect < 1)
        printer.setPageOrientation(QPageLayout::Portrait);
    else
        printer.setPageOrientation(QPageLayout::Landscape);

    QPrintDialog printDialog(&printer);
    if (printDialog.exec() == QDialog::Accepted) {
        // Print as at 100 % (only once the dialog is accepted, so the view does not shrink
        // while the dialog is open).
        ScopedUnzoomForExport unzoomGuard(multiLayerParent());
        // The aspect of the layer as at 100 % (the zoomed rectangle is rounded differently).
        aspect = double(d_plot->width()) / double(qMax(1, d_plot->height()));
        QRect plotRect = d_plot->rect();
        QRect paperRect = printer.pageLayout().fullRectPixels(printer.resolution());
        if (d_scale_on_print) {
            int dpiy = printer.logicalDpiY();
            int margin = (int)((2 / 2.54) * dpiy); // 2 cm margins

            int width = qRound(aspect * printer.height()) - 2 * margin;
            int x = qRound(abs(printer.width() - width) * 0.5);

            plotRect = QRect(x, margin, width, printer.height() - 2 * margin);
            if (x < margin) {
                plotRect.setLeft(margin);
                plotRect.setWidth(printer.width() - 2 * margin);
            }
        } else {
            int x_margin = (paperRect.width() - plotRect.width()) / 2;
            int y_margin = (paperRect.height() - plotRect.height()) / 2;
            plotRect.moveTo(x_margin, y_margin);
        }

        QPainter paint(&printer);
        if (d_print_cropmarks) {
            QRect cr = plotRect; // cropmarks rectangle
            cr.adjust(-1, -1, 2, 2);
            paint.save();
            paint.setPen(QPen(QColor(Qt::black), 0.5, Qt::DashLine));
            paint.drawLine(paperRect.left(), cr.top(), paperRect.right(), cr.top());
            paint.drawLine(paperRect.left(), cr.bottom(), paperRect.right(), cr.bottom());
            paint.drawLine(cr.left(), paperRect.top(), cr.left(), paperRect.bottom());
            paint.drawLine(cr.right(), paperRect.top(), cr.right(), paperRect.bottom());
            paint.restore();
        }

        print(&paint, plotRect);
    }
}

bool Graph::exportSVGChecked(const QString &fname, QString *errOut)
{
    if (fname.isEmpty()) {
        if (errOut)
            *errOut = tr("Please provide a valid file name!");
        return false;
    }
#if QT_VERSION >= 0x040300
    ScopedUnzoomForExport unzoomGuard(multiLayerParent()); // export as at 100 %
    QFile outFile(fname);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut) {
            *errOut = outFile.errorString().isEmpty()
                    ? tr("Could not open SVG file for writing: %1").arg(fname)
                    : outFile.errorString();
        }
        return false;
    }
    QSvgGenerator svg;
    svg.setOutputDevice(&outFile);
#if QT_VERSION >= 0x040500
    svg.setSize(d_plot->size());
    svg.setViewBox(d_plot->rect());
    svg.setResolution(96); // FIXME hardcored
    svg.setTitle(this->objectName());
#endif
    QPainter p;
    if (!p.begin(&svg)) {
        outFile.close();
        if (errOut)
            *errOut = tr("Failed to initialize SVG painter.");
        return false;
    }
    exportPainter(p, false, QRect(), QSize(svg.width(), svg.height()));
    const bool endOk = p.end();
    const bool flushOk = outFile.flush();
    const QFile::FileError ferr = outFile.error();
    const QString ferrStr = outFile.errorString();
    const qint64 writtenBytes = outFile.size();
    outFile.close();
    if (!endOk || !flushOk || ferr != QFile::NoError || writtenBytes <= 0) {
        if (errOut)
            *errOut = ferrStr.isEmpty() ? tr("Failed to write SVG file: %1").arg(fname) : ferrStr;
        return false;
    }
    return true;
#else
    if (errOut)
        *errOut = tr("SVG export is not supported in this build.");
    return false;
#endif
}

void Graph::exportSVG(const QString &fname)
{
    (void)exportSVGChecked(fname, nullptr);
}

void Graph::exportPainter(QPaintDevice &paintDevice, bool keepAspect, QRect rect)
{
    QPainter p(&paintDevice);
    exportPainter(p, keepAspect, rect, QSize(paintDevice.width(), paintDevice.height()));
}

void Graph::exportPainter(QPainter &painter, bool keepAspect, QRect rect, QSize size)
{
    if (size == QSize())
        size = d_plot->size();
    if (rect == QRect())
        rect = QRect(QPoint(0, 0), size);
    if (keepAspect) {
        QSize scaled = rect.size();
        scaled.scale(size, Qt::KeepAspectRatio);
        size = scaled;
    }

    ScopedExportFlag exportGuard(d_is_exporting);
    painter.save();
    painter.scale((double)size.width() / (double)rect.width(),
                  (double)size.height() / (double)rect.height());

    print(&painter, rect);
    painter.restore();
}

int Graph::selectedCurveID()
{
    if (d_range_selector && d_range_selector->selectedCurve())
        return curveKey(curveIndex(d_range_selector->selectedCurve()));
    if ((d_selection.kind == GraphSelection::Curve
         || d_selection.kind == GraphSelection::ErrorBars)
        && d_selection.primaryId >= 0 && d_selection.primaryId < n_curves) {
        return curveKey(d_selection.primaryId);
    }
    return -1;
}

QString Graph::selectedCurveTitle()
{
    if (d_range_selector && d_range_selector->selectedCurve())
        return d_range_selector->selectedCurve()->title().text();
    if ((d_selection.kind == GraphSelection::Curve
         || d_selection.kind == GraphSelection::ErrorBars)
        && d_selection.primaryId >= 0 && d_selection.primaryId < n_curves) {
        if (const QwtPlotCurve *c = curve(d_selection.primaryId))
            return c->title().text();
    }
    return {};
}

bool Graph::markerSelected()
{
    return (selectedMarker >= 0);
}

void Graph::removeMarker()
{
    if (selectedMarker >= 0) {
        if (d_markers_selector) {
            if (d_texts.contains(selectedMarker))
                d_markers_selector->removeAll((Legend *)d_plot->marker(selectedMarker));
            else if (d_lines.contains(selectedMarker))
                d_markers_selector->removeAll((ArrowMarker *)d_plot->marker(selectedMarker));
            else if (d_images.contains(selectedMarker))
                d_markers_selector->removeAll((ImageMarker *)d_plot->marker(selectedMarker));
        }
        if (cp)
            cp->disableEditing();
        d_plot->removeMarker(selectedMarker);
        d_plot->replot();
        emit modifiedGraph();

        if (selectedMarker == legendMarkerID)
            legendMarkerID = -1;

        if (d_lines.contains(selectedMarker)) {
            int index = d_lines.indexOf(selectedMarker);
            int last_line_marker = (int)d_lines.size() - 1;
            for (int i = index; i < last_line_marker; i++)
                d_lines[i] = d_lines[i + 1];
            d_lines.resize(last_line_marker);
        } else if (d_texts.contains(selectedMarker)) {
            int index = d_texts.indexOf(selectedMarker);
            int last_text_marker = d_texts.size() - 1;
            for (int i = index; i < last_text_marker; i++)
                d_texts[i] = d_texts[i + 1];
            d_texts.resize(last_text_marker);
        } else if (d_images.contains(selectedMarker)) {
            int index = d_images.indexOf(selectedMarker);
            int last_image_marker = d_images.size() - 1;
            for (int i = index; i < last_image_marker; i++)
                d_images[i] = d_images[i + 1];
            d_images.resize(last_image_marker);
        }
        selectedMarker = -1;
    }
}

void Graph::cutMarker()
{
    copyMarker();
    removeMarker();
}

bool Graph::arrowMarkerSelected()
{
    return (d_lines.contains(selectedMarker));
}

bool Graph::imageMarkerSelected()
{
    return (d_images.contains(selectedMarker));
}

void Graph::copyMarker()
{
    if (selectedMarker < 0) {
        selectedMarkerType = None;
        return;
    }

    if (d_lines.contains(selectedMarker)) {
        ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(selectedMarker);
        auxMrkStart = mrkL->startPoint();
        auxMrkEnd = mrkL->endPoint();
        selectedMarkerType = Arrow;
    } else if (d_images.contains(selectedMarker)) {
        ImageMarker *mrkI = (ImageMarker *)d_plot->marker(selectedMarker);
        auxMrkStart = mrkI->origin();
        QRect rect = mrkI->rect();
        auxMrkEnd = rect.bottomRight();
        auxMrkFileName = mrkI->fileName();
        selectedMarkerType = Image;
    } else
        selectedMarkerType = Text;
}

void Graph::pasteMarker()
{
    if (selectedMarkerType == Arrow) {
        ArrowMarker *mrkL = new ArrowMarker();
        int linesOnPlot = (int)d_lines.size();
        d_lines.resize(++linesOnPlot);
        d_lines[linesOnPlot - 1] = d_plot->insertMarker(mrkL);

        mrkL->setColor(auxMrkColor);
        mrkL->setWidth(auxMrkWidth);
        mrkL->setStyle(auxMrkStyle);
        mrkL->setStartPoint(QPoint(auxMrkStart.x() + 10, auxMrkStart.y() + 10));
        mrkL->setEndPoint(QPoint(auxMrkEnd.x() + 10, auxMrkEnd.y() + 10));
        mrkL->drawStartArrow(startArrowOn);
        mrkL->drawEndArrow(endArrowOn);
        mrkL->setHeadLength(auxArrowHeadLength);
        mrkL->setHeadAngle(auxArrowHeadAngle);
        mrkL->fillArrowHead(auxFilledArrowHead);
    } else if (selectedMarkerType == Image) {
        ImageMarker *mrk = new ImageMarker(auxMrkFileName);
        int imagesOnPlot = d_images.size();
        d_images.resize(++imagesOnPlot);
        d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk);

        QPoint o = d_plot->canvas()->mapFromGlobal(QCursor::pos());
        if (!d_plot->canvas()->contentsRect().contains(o))
            o = QPoint(auxMrkStart.x() + 20, auxMrkStart.y() + 20);
        mrk->setOrigin(o);
        mrk->setSize(QRect(auxMrkStart, auxMrkEnd).size());
    } else {
        Legend *mrk = new Legend(d_plot);
        int texts = d_texts.size();
        d_texts.resize(++texts);
        d_texts[texts - 1] = d_plot->insertMarker(mrk);

        QPoint o = d_plot->canvas()->mapFromGlobal(QCursor::pos());
        if (!d_plot->canvas()->contentsRect().contains(o))
            o = QPoint(auxMrkStart.x() + 20, auxMrkStart.y() + 20);
        mrk->setOrigin(o);
        mrk->setAngle(auxMrkAngle);
        mrk->setFrameStyle(auxMrkBkg);
        mrk->setFont(auxMrkFont);
        mrk->setText(auxMrkText);
        mrk->setTextColor(auxMrkColor);
        mrk->setBackgroundColor(auxMrkBkgColor);
    }

    d_plot->replot();
    deselectMarker();
}

void Graph::setCopiedMarkerEnds(const QPoint &start, const QPoint &end)
{
    auxMrkStart = start;
    auxMrkEnd = end;
}

void Graph::setCopiedTextOptions(int bkg, const QString &text, const QFont &font,
                                 const QColor &color, const QColor &bkgColor)
{
    auxMrkBkg = bkg;
    auxMrkText = text;
    auxMrkFont = font;
    auxMrkColor = color;
    auxMrkBkgColor = bkgColor;
}

void Graph::setCopiedArrowOptions(int width, Qt::PenStyle style, const QColor &color, bool start,
                                  bool end, int headLength, int headAngle, bool filledHead)
{
    auxMrkWidth = width;
    auxMrkStyle = style;
    auxMrkColor = color;
    startArrowOn = start;
    endArrowOn = end;
    auxArrowHeadLength = headLength;
    auxArrowHeadAngle = headAngle;
    auxFilledArrowHead = filledHead;
}

bool Graph::titleSelected()
{
    // Stale keyboard focus on the title label alone no longer counts as "selected":
    // after Esc / blank click the title must not be deleted or nudged (B4-3).
    return d_selection.kind == GraphSelection::PlotTitle
            || (titlePicker && titlePicker->selected());
}

void Graph::selectTitle()
{
    if (!d_plot->hasFocus()) {
        emit selectedGraph(this);
        QwtTextLabel *title = d_plot->titleLabel();
        title->setFocus();
    }

    deselectMarker();
    setCurrentSelection({ GraphSelection::PlotTitle, -1, -1 });
}

void Graph::setTitle(const QString &t)
{
    QString cleanT = t;
    cleanT.replace(QLatin1String("&#9;"), QLatin1String("\t"));
    cleanT.replace(QLatin1String("&#60;"), QLatin1String("<"));
    QwtText cur = d_plot->title();
    QFont f = cur.font();
    if (f.family().trimmed().isEmpty())
        f = QFont("Arial", 16, QFont::Normal, false);
    cur.setFont(f);
    cur.setText(cleanT);
    d_plot->setTitle(cur);
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
}

void Graph::removeTitle()
{
    if (titleSelected() || d_selection.kind == GraphSelection::PlotTitle) {
        MultiLayer *ml = multiLayerParent();
        const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
        setTitle(QString());
        if (titlePicker)
            titlePicker->setSelected(false);
        if (d_selection.kind == GraphSelection::PlotTitle)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Remove Plot Title"), beforeSnap);
    }
}

void Graph::initTitle(bool on, const QFont &fnt)
{
    if (on) {
        QFont nf = fnt;
        nf.setHintingPreference(QFont::PreferNoHinting);
        QwtText t = d_plot->title();
        t.setFont(nf);
        t.setText(tr("Title"));
        d_plot->setTitle(t);
    }
}

void Graph::removeLegend()
{
    if (legendMarkerID >= 0) {
        if (selectedMarker == legendMarkerID)
            deselectMarker();
        int index = d_texts.indexOf(legendMarkerID);
        int texts = d_texts.size();
        for (int i = index; i < texts - 1; i++)
            d_texts[i] = d_texts[i + 1];
        d_texts.resize(--texts);

        d_plot->removeMarker(legendMarkerID);
        legendMarkerID = -1;
        if (d_plot->property("_outsideRightMargin").toInt() != 0)
            d_plot->setProperty("_outsideRightMargin", 0);
        d_plot->setProperty("_savedOutsideRightMargin", 0);
        if (d_selection.kind == GraphSelection::LegendBox)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
    }
}

void Graph::updateImageMarker(int x, int y, int w, int h)
{
    ImageMarker *mrk = (ImageMarker *)d_plot->marker(selectedMarker);
    mrk->setRect(x, y, w, h);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::updateTextMarker(const QString &text, int angle, int bkg, const QFont &fnt,
                             const QColor &textColor, const QColor &backgroundColor)
{
    Legend *mrkL = (Legend *)d_plot->marker(selectedMarker);
    mrkL->setText(text);
    mrkL->setAngle(angle);
    mrkL->setTextColor(textColor);
    mrkL->setBackgroundColor(backgroundColor);
    mrkL->setFont(fnt);
    mrkL->setFrameStyle(bkg);

    d_plot->replot();
    emit modifiedGraph();
}

Legend *Graph::legend()
{
    if (legendMarkerID >= 0)
        return (Legend *)d_plot->marker(legendMarkerID);
    else
        return 0;
}

QString Graph::legendText()
{
    QString text = "";
    for (int i = 0; i < n_curves; i++) {
        const QwtPlotCurve *c = curve(i);
        if (c && c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram && c_type[i] != ErrorBars
            && c->testItemAttribute(QwtPlotItem::Legend)) {
            text += "\\c{";
            text += QString::number(i + 1);
            text += "}";
            const FunctionCurve *fc = dynamic_cast<const FunctionCurve *>(c);
            if (fc && !d_fit_curves.contains(const_cast<QwtPlotCurve *>(c)))
                text += fc->legend();
            else if (fc)
                text += c->title().text();
            else {
                QString label = resolvedAutoCurveLabel(i);
                if (label.isEmpty())
                    label = c->title().text();
                DataCurve *dc = dynamic_cast<DataCurve *>(const_cast<QwtPlotCurve *>(c));
                if (dc && dc->table())
                    d_last_auto_curve_labels[i] = label;
                text += label;
            }
            text += "\n";
        }
    }
    return text;
}

QString Graph::pieLegendText()
{
    QString text = "";
    const QwtPlotCurve *pieCurve = c_keys.isEmpty() ? nullptr : curve(0);
    if (pieCurve) {
        for (int i = 0; i < int(pieCurve->dataSize()); i++) {
            text += "\\p{";
            text += QString::number(i + 1);
            text += "} ";
            text += QString::number(i + 1);
            text += "\n";
        }
    }
    return text;
}

void Graph::updateCurvesData(Table *w, const QString &colName)
{
    int updated_curves = 0;
    QList<PlotCurve *> to_remove; // curves which changed too much to be kept
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it)
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!pc || pc->type() == Function)
            continue;
        DataCurve *c = dynamic_cast<DataCurve *>(pc);
        if (!c)
            continue;
        if (c->table() == w && (c->xColumnName() == colName || c->yColumnName() == colName)) {
            Column *changedCol = w ? w->column(colName) : nullptr;
            if (!changedCol) {
                if (c->updateData(w, colName))
                    updated_curves++;
                continue;
            }
            auto colType = changedCol->columnMode();
            AxisType atype;
            if (c->xColumnName() == colName) {
                if (c->type() == HorizontalBars)
                    atype = (AxisType)axisType[QwtPlot::yLeft];
                else
                    atype = (AxisType)axisType[QwtPlot::xBottom];
            } else {
                if (c->type() == HorizontalBars)
                    atype = (AxisType)axisType[QwtPlot::xBottom];
                else
                    atype = (AxisType)axisType[QwtPlot::yLeft];
            }
            // compare setLabels*Format() calls in insertCurve()
            if ((colType == SciDAVis::ColumnMode::Text && atype != AxisType::Txt)
                || (colType == SciDAVis::ColumnMode::DateTime
                    && ((atype != AxisType::Time) && (atype != AxisType::Date)
                        && (atype != AxisType::DateTime))))
                to_remove << c;
            else if (c->updateData(w, colName))
                updated_curves++;
        } else if (c->updateData(w, colName))
            updated_curves++;
    }
    foreach (PlotCurve *c, to_remove) {
        removeCurve(curveIndex(c));
        updated_curves++;
    }
    if (updated_curves) {
        if (d_point_colorbar_active)
            syncPointColorBar();
        updateStatReferenceLines();
        if (isPiePlot())
            updatePlot();
        else {
            if (m_autoscale) {
                for (int i = 0; i < QwtPlot::axisCnt; i++) {
                    if (!isToriAxisUserLocked(d_plot, i))
                        d_plot->setAxisAutoScale(i);
                }
            }
            if (d_linked_parent)
                syncLinkedAxesFromParent();
            d_plot->replot();
            syncChildLinkedLayers();
        }
    }
}

QString Graph::saveEnabledAxes()
{
    QString list = "EnabledAxes\t";
    for (int i = 0; i < QwtPlot::axisCnt; i++)
        list += QString::number(d_plot->axisEnabled(i)) + "\t";

    list += "\n";
    return list;
}

bool Graph::framed()
{
    return d_plot->lineWidth() > 0;
}

QColor Graph::canvasFrameColor()
{
    QwtPlotCanvas *canvas = (QwtPlotCanvas *)d_plot->canvas();
    QPalette pal = canvas->palette();
    return pal.color(QPalette::Active, QPalette::WindowText);
}

int Graph::canvasFrameWidth()
{
    QwtPlotCanvas *canvas = (QwtPlotCanvas *)d_plot->canvas();
    return canvas->lineWidth();
}

void Graph::drawCanvasFrame(const QStringList &frame)
{
    d_plot->setLineWidth((frame[1]).toInt());
    if ((frame[1]).toInt())
        d_plot->setFrameStyle(QFrame::Box);

    QPalette pal = d_plot->palette();
    pal.setColor(QPalette::WindowText, QColor(COLORVALUE(frame[2])));
    d_plot->setPalette(pal);
}

void Graph::drawCanvasFrame(bool frameOn, int width, const QColor &color)
{
    QPalette pal = d_plot->palette();

    if (frameOn && d_plot->lineWidth() == width
        && pal.color(QPalette::Active, QPalette::WindowText) == color)
        return;

    if (frameOn) {
        d_plot->setLineWidth(width);
        pal.setColor(QPalette::WindowText, color);
        d_plot->setPalette(pal);
    } else {
        d_plot->setLineWidth(0);
        pal.setColor(QPalette::WindowText, QColor(Qt::black));
        d_plot->setPalette(pal);
    }
    emit modifiedGraph();
}

void Graph::drawCanvasFrame(bool frameOn, int width)
{
    if (frameOn) {
        d_plot->setLineWidth(width);
    }
}

void Graph::drawAxesBackbones(bool yes)
{
    drawAxesBackbone = yes;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_axis_backbone[i] = yes;
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(i);
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, yes);
            scale->repaint();
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::loadAxesOptions(const QString &s)
{
    if (s == "1")
        return;

    drawAxesBackbone = false;

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_axis_backbone[i] = false;
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(i);
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, false);
            scale->repaint();
        }
    }
}

void Graph::setAxesLinewidth(int width)
{
    if (d_plot->axesLinewidth() == width)
        return;

    d_plot->setAxesLinewidth(width);

    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            scale->setPenWidth(width);
            if (scale->isColorBarEnabled()) {
                if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(i)))
                    sd->setColorBarBorder(true, scale->colorBarWidth(), qMax(1, width));
            }
            scale->repaint();
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::loadAxesLinewidth(int width)
{
    d_plot->setAxesLinewidth(width);
}

QString Graph::saveCanvas()
{
    QString s = "";
    int w = d_plot->canvas()->lineWidth();
    if (w > 0) {
        s += "CanvasFrame\t" + QString::number(w) + "\t";
        s += COLORNAME(canvasFrameColor()) + "\n";
    }
    s += "CanvasBackground\t" + COLORNAME(d_plot->canvasBackground()) + "\t";
    s += QString::number(d_plot->canvasBackground().alpha()) + "\n";
    return s;
}

QString Graph::saveFonts()
{
    int i;
    QString s;
    QStringList list, axesList;
    QFont f;
    list << "TitleFont";
    f = d_plot->title().font();
    list << f.family();
    list << QString::number(f.pointSize());
    list << QString::number(f.weight());
    list << QString::number(f.italic());
    list << QString::number(f.underline());
    list << QString::number(f.strikeOut());
    // Exact size after the legacy integer field (12.9 pt presets); older readers ignore it.
    list << QString::number(f.pointSizeF() > 0.0 ? f.pointSizeF() : 0.0, 'g', 6);
    s = list.join("\t") + "\n";

    for (i = 0; i < d_plot->axisCnt; i++) {
        f = d_plot->axisTitle(i).font();
        list[0] = "ScaleFont" + QString::number(i);
        list[1] = f.family();
        list[2] = QString::number(f.pointSize());
        list[3] = QString::number(f.weight());
        list[4] = QString::number(f.italic());
        list[5] = QString::number(f.underline());
        list[6] = QString::number(f.strikeOut());
        list[7] = QString::number(f.pointSizeF() > 0.0 ? f.pointSizeF() : 0.0, 'g', 6);
        s += list.join("\t") + "\n";
    }

    for (i = 0; i < d_plot->axisCnt; i++) {
        f = d_plot->axisFont(i);
        list[0] = "AxisFont" + QString::number(i);
        list[1] = f.family();
        list[2] = QString::number(f.pointSize());
        list[3] = QString::number(f.weight());
        list[4] = QString::number(f.italic());
        list[5] = QString::number(f.underline());
        list[6] = QString::number(f.strikeOut());
        list[7] = QString::number(f.pointSizeF() > 0.0 ? f.pointSizeF() : 0.0, 'g', 6);
        s += list.join("\t") + "\n";
    }
    return s;
}

QString Graph::saveAxesFormulas()
{
    QString s;
    for (int i = 0; i < 4; i++)
        if (!axesFormulas[i].isEmpty()) {
            s += "<AxisFormula pos=\"" + QString::number(i) + "\">\n";
            s += axesFormulas[i];
            s += "\n</AxisFormula>\n";
        }
    return s;
}

QString Graph::saveScale()
{
    QString s;
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        s += "scale\t" + QString::number(i) + "\t";

        const QwtScaleDiv *scDiv = d_plot->axisScaleDiv(i);
        QwtValueList lst = scDiv->ticks(QwtScaleDiv::MajorTick);

#if QWT_VERSION >= 0x050200
        s += QString::number(qMin(scDiv->lowerBound(), scDiv->upperBound()), 'g', 15) + "\t";
        s += QString::number(qMax(scDiv->lowerBound(), scDiv->upperBound()), 'g', 15) + "\t";
#else
        s += QString::number(qMin(scDiv->lBound(), scDiv->hBound()), 'g', 15) + "\t";
        s += QString::number(qMax(scDiv->lBound(), scDiv->hBound()), 'g', 15) + "\t";
#endif
        s += QString::number(d_user_step[i], 'g', 15) + "\t";
        s += QString::number(d_plot->axisMaxMajor(i)) + "\t";
        s += QString::number(d_plot->axisMaxMinor(i)) + "\t";

        const QwtScaleEngine *sc_eng = d_plot->axisScaleEngine(i);
        const int savedScaleType = Plot::isLog10ScaleEngine(sc_eng) ? 1 : 0;
        s += QString::number(savedScaleType) + "\t";
        s += QString::number(sc_eng ? sc_eng->testAttribute(QwtScaleEngine::Inverted) : false)
                + "\n";
    }
    return s;
}

void Graph::setXAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::xBottom);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setYAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::yLeft);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setRightAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::yRight);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setTopAxisTitleColor(const QColor &c)
{
    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(QwtPlot::xTop);
    if (scale) {
        QwtText t = scale->title();
        t.setColor(c);
        scale->setTitle(t);
        emit modifiedGraph();
    }
}

void Graph::setAxesTitleColor(QStringList l)
{
    for (int i = 0; i < int(l.count() - 1); i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        if (scale) {
            QwtText title = scale->title();
            title.setColor(QColor(COLORVALUE(l[i + 1])));
            scale->setTitle(title);
        }
    }
}

QString Graph::saveAxesTitleColors()
{
    QString s = "AxesTitleColors\t";
    for (int i = 0; i < 4; i++) {
        QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(i);
        QColor c;
        if (scale)
            c = scale->title().color();
        else
            c = QColor(Qt::black);

        s += COLORNAME(c) + "\t";
    }
    return s + "\n";
}

namespace {
QString sanitizeProjectLineField(const QString &raw, bool newlineToBr)
{
    QString out = raw;
    out.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    out.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (newlineToBr)
        out.replace(QLatin1Char('\n'), QLatin1String("<br>"));
    out.replace(QLatin1Char('\t'), QLatin1String("&#9;"));
    static const char *const kProtectedTags[] = {
        "<Tori",       "</graph>",  "<graph>",    "</multiLayer>", "<multiLayer>",
        "</text>",     "<text>",    "</legend>",  "<legend>",      "</line>",
        "</image>",    "</curve>",  "</formula>"
    };
    for (const char *tag : kProtectedTags) {
        const QString qtag = QString::fromLatin1(tag);
        if (out.contains(qtag, Qt::CaseInsensitive)) {
            QRegularExpression rx(QRegularExpression::escape(qtag),
                                  QRegularExpression::CaseInsensitiveOption);
            out.replace(rx, QStringLiteral("&#60;") + qtag.mid(1));
        }
    }
    return out;
}
} // namespace

QString Graph::saveTitle()
{
    QString s = "PlotTitle\t";
    const QString escTitle = sanitizeProjectLineField(d_plot->title().text(), true);
    s += escTitle + "\t";
    s += COLORNAME(d_plot->title().color()) + "\t";
    s += QString::number(d_plot->title().renderFlags()) + "\n";
    return s;
}

QString Graph::saveScaleTitles()
{
    int a{};
    QString s = "";
    for (int i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            a = 2;
            break;
        case 1:
            a = 0;
            break;
        case 2:
            a = 3;
            break;
        case 3:
            a = 1;
            break;
        }
        const QString title = axisTitle(a);
        if (!title.isEmpty())
            s += sanitizeProjectLineField(title, true) + "\t";
        else
            s += "\t";
    }
    return s + "\n";
}

QString Graph::saveAxesTitleAlignement()
{
    QString s = "AxesTitleAlignment\t";
    QStringList axes;
    int i;
    for (i = 0; i < 4; i++)
        axes << QString::number(Qt::AlignHCenter);

    for (i = 0; i < 4; i++) {

        if (d_plot->axisEnabled(i))
            axes[i] = QString::number(d_plot->axisTitle(i).renderFlags());
    }

    s += axes.join("\t") + "\n";
    return s;
}

QString Graph::savePieCurveLayout()
{
    QString s = "PieCurve\t";

    QwtPieCurve *pieCurve = (QwtPieCurve *)curve(0);
    s += pieCurve->title().text() + "\t";
    QPen pen = pieCurve->pen();

    s += QString::number(pen.width()) + "\t";
    s += COLORNAME(pen.color()) + "\t";
    s += penStyleName(pen.style()) + "\t";

    Qt::BrushStyle pattern = pieCurve->pattern();
    int index;
    if (pattern == Qt::SolidPattern)
        index = 0;
    else if (pattern == Qt::HorPattern)
        index = 1;
    else if (pattern == Qt::VerPattern)
        index = 2;
    else if (pattern == Qt::CrossPattern)
        index = 3;
    else if (pattern == Qt::BDiagPattern)
        index = 4;
    else if (pattern == Qt::FDiagPattern)
        index = 5;
    else if (pattern == Qt::DiagCrossPattern)
        index = 6;
    else if (pattern == Qt::Dense1Pattern)
        index = 7;
    else if (pattern == Qt::Dense2Pattern)
        index = 8;
    else if (pattern == Qt::Dense3Pattern)
        index = 9;
    else if (pattern == Qt::Dense4Pattern)
        index = 10;
    else if (pattern == Qt::Dense5Pattern)
        index = 11;
    else if (pattern == Qt::Dense6Pattern)
        index = 12;
    else if (pattern == Qt::Dense7Pattern)
        index = 13;
    else
        throw std::runtime_error("pattern out of range");

    s += QString::number(index) + "\t";
    s += QString::number(pieCurve->ray()) + "\t";
    s += QString::number(pieCurve->firstColor()) + "\t";
    s += QString::number(pieCurve->startRow()) + "\t" + QString::number(pieCurve->endRow()) + "\t";
    s += QString::number(pieCurve->isVisible()) + "\n";
    return s;
}

QString Graph::saveCurveLayout(int index)
{
    QString s;
    int style = c_type[index];
    QwtPlotCurve *c = (QwtPlotCurve *)curve(index);
    if (c) {
        s += QString::number(style) + "\t";
        if (style == Spline)
            s += "5\t";
        else if (style == VerticalSteps)
            s += "6\t";
        else
            s += QString::number(c->style()) + "\t";
        s += COLORNAME(c->pen().color()) + "\t";
        s += QString::number(c->pen().style() - 1) + "\t";
        const int savedLw = (c->pen().widthF() > 0.0) ? qMax(1, qRound(c->pen().widthF()))
                                                      : c->pen().width();
        s += QString::number(savedLw) + "\t";

        const QwtSymbol symbol = c->symbol();
        s += QString::number(symbol.size().width()) + "\t";
        s += QString::number(SymbolBox::symbolIndex(symbol.style())) + "\t";
        s += COLORNAME(symbol.pen().color()) + "\t";
        if (symbol.brush().style() != Qt::NoBrush)
            s += COLORNAME(symbol.brush().color()) + "\t";
        else
            s += QString::number(-1) + "\t";

        bool filled = c->brush().style() == Qt::NoBrush ? false : true;
        s += QString::number(filled) + "\t";

        s += COLORNAME(c->brush().color()) + "\t";
        s += QString::number(PatternBox::patternIndex(c->brush().style())) + "\t";
        if (style <= LineSymbols || style == Box) {
            const int savedSw = (symbol.pen().widthF() > 0.0)
                    ? qMax(1, qRound(symbol.pen().widthF()))
                    : symbol.pen().width();
            s += QString::number(savedSw) + "\t";
        }
        // extension for custom dash pattern
        s += QString::number(static_cast<int>(c->pen().capStyle())) + "\t";
        s += QString::number(static_cast<int>(c->pen().joinStyle())) + "\t";
        QStringList customDash;
        for (auto v : c->pen().dashPattern())
            customDash << QString::number(v);
        s += customDash.join(" ") + "\t";
    }

    if (style == VerticalBars || style == HorizontalBars || style == Histogram) {
        QwtBarCurve *b = (QwtBarCurve *)c;
        s += QString::number(b->gap()) + "\t";
        s += QString::number(b->offset()) + "\t";
    }

    if (style == Histogram) {
        QwtHistogram *h = (QwtHistogram *)c;
        s += QString::number(h->autoBinning()) + "\t";
        s += QString::number(h->binSize()) + "\t";
        s += QString::number(h->begin()) + "\t";
        s += QString::number(h->end()) + "\t";
    } else if (style == VectXYXY || style == VectXYAM) {
        VectorCurve *v = (VectorCurve *)c;
        s += COLORNAME(v->color()) + "\t";
        s += QString::number(v->width()) + "\t";
        s += QString::number(v->headLength()) + "\t";
        s += QString::number(v->headAngle()) + "\t";
        s += QString::number(v->filledArrowHead()) + "\t";

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList colsList = v->plotAssociation().split(",", Qt::SkipEmptyParts);
#else
        QStringList colsList = v->plotAssociation().split(",", QString::SkipEmptyParts);
#endif
        s += colsList[2].remove("(X)").remove("(A)") + "\t";
        s += colsList[3].remove("(Y)").remove("(M)");
        if (style == VectXYAM)
            s += "\t" + QString::number(v->position());
        s += "\t";
    } else if (style == Box) {
        BoxCurve *b = (BoxCurve *)c;
        s += QString::number(SymbolBox::symbolIndex(b->maxStyle())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->p99Style())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->meanStyle())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->p1Style())) + "\t";
        s += QString::number(SymbolBox::symbolIndex(b->minStyle())) + "\t";
        s += QString::number(b->boxStyle()) + "\t";
        s += QString::number(b->boxWidth()) + "\t";
        s += QString::number(b->boxRangeType()) + "\t";
        s += QString::number(b->boxRange()) + "\t";
        s += QString::number(b->whiskersRangeType()) + "\t";
        s += QString::number(b->whiskersRange()) + "\t";
        s += QString::number(b->violinMode() ? 1 : 0) + "\t";
        s += QString::number(b->showDataPoints() ? 1 : 0) + "\t";
    }

    return s;
}

QString Graph::saveCurves()
{
    QString s;
    if (isPiePlot())
        s += savePieCurveLayout();
    else {
        for (int i = 0; i < n_curves; i++) {
            QwtPlotItem *it = plotItem(i);
            if (!it)
                continue;

            if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                s += ((Spectrogram *)it)->saveToString();
                continue;
            }

            PlotCurve *c = dynamic_cast<PlotCurve *>(it);
            if (!c)
                continue;
            if (c->type() != ErrorBars) {
                FunctionCurve *fc = (c->type() == Function) ? dynamic_cast<FunctionCurve *>(c) : nullptr;
                DataCurve *dc = dynamic_cast<DataCurve *>(c);
                if (fc) {
                    s += "FunctionCurve\t";
                    s += QString::number(fc->functionType()) + ",";
                    s += c->title().text() + ",";
                    s += fc->variable() + ",";
                    s += QString::number(fc->startRange(), 'g', 15) + ",";
                    s += QString::number(fc->endRange(), 'g', 15) + "\t";
                    s += QString::number(fc->dataSize()) + "\t\t\t";
                    // the 2 last tabs are legacy code, kept for compatibility with old project
                    // files
                } else if (c->type() == Box) {
                    s += "curve\t" + QString::number(c->x(0)) + "\t" + c->title().text() + "\t";
                } else if (dc) {
                    s += "curve\t" + dc->xColumnName() + "\t" + c->title().text() + "\t";
                } else {
                    continue;
                }

                s += saveCurveLayout(i);
                s += QString::number(c->xAxis()) + "\t" + QString::number(c->yAxis()) + "\t";
                const int startR = dc ? dc->startRow() : 0;
                const int endR = dc ? dc->endRow() : qMax(0, c->dataSize() - 1);
                s += QString::number(startR) + "\t" + QString::number(endR) + "\t";
                s += QString::number(c->isVisible()) + "\n";
                if (fc) {
                    s += "<formula>\n"
                            + fc->formulas().join(
                                      "\n</formula>\n<formula>\n")
                            + "\n</formula>\n";
                    if (fc->dataSize() > 0 && fc->dataSize() <= 20000) {
                        s += "<sampled_xy>\n";
                        for (int k = 0; k < fc->dataSize(); ++k) {
                            s += QString::number(fc->x(k), 'g', 15) + "\t"
                                    + QString::number(fc->y(k), 'g', 15) + "\n";
                        }
                        s += "</sampled_xy>\n";
                    }
                }
            } else if (c->type() == ErrorBars) {
                QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it);
                if (!er || !er->masterCurve())
                    continue;
                s += "ErrorBars\t";
                s += QString::number(er->direction()) + "\t";
                s += er->masterCurve()->xColumnName() + "\t";
                s += er->masterCurve()->title().text() + "\t";
                s += er->title().text() + "\t";
                s += QString::number(er->width()) + "\t";
                s += QString::number(er->capLength()) + "\t";
                s += COLORNAME(er->color()) + "\t";
                s += QString::number(er->throughSymbol()) + "\t";
                s += QString::number(er->plusSide()) + "\t";
                s += QString::number(er->minusSide()) + "\t";
                s += QString::number(er->isVisible() ? 1 : 0) + "\n";
            }
        }
    }
    return s;
}

Legend *Graph::newLegend()
{
    d_legend_visible = true;
    Legend *mrk = new Legend(d_plot);
    mrk->setOrigin(QPoint(16, 16));

    if (isPiePlot())
        mrk->setText(pieLegendText());
    else
        mrk->setText(legendText());

    mrk->setFrameStyle(defaultMarkerFrame);
    mrk->setFont(defaultMarkerFont);
    mrk->setTextColor(defaultTextMarkerColor);
    mrk->setBackgroundColor(defaultTextMarkerBackground);

    legendMarkerID = d_plot->insertMarker(mrk);
    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = legendMarkerID;

    if (!isPiePlot() && curves() > 0 && d_plot) {
        d_plot->updateLayout();
        if (d_plot->canvas() && d_plot->canvas()->width() >= 40
            && d_plot->canvas()->height() >= 40) {
            d_plot->replot();
            placeLegendAnchor(LegendAutoAvoidData, nullptr, 12);
            return mrk;
        }
    }

    emit modifiedGraph();
    d_plot->replot();
    return mrk;
}

void Graph::addTimeStamp()
{
    Legend *mrk = newLegend(QLocale().toString(QDateTime::currentDateTime()));
    mrk->setOrigin(QPoint(d_plot->canvas()->width() / 2, 10));
    emit modifiedGraph();
    d_plot->replot();
}

Legend *Graph::newLegend(const QString &text)
{
    Legend *mrk = new Legend(d_plot);
    selectedMarker = d_plot->insertMarker(mrk);
    detachAndDeleteMarkerSelector(d_markers_selector);

    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = selectedMarker;

    mrk->setOrigin(QPoint(5, 5));
    mrk->setText(text);
    mrk->setFrameStyle(defaultMarkerFrame);
    mrk->setFont(defaultMarkerFont);
    mrk->setTextColor(defaultTextMarkerColor);
    mrk->setBackgroundColor(defaultTextMarkerBackground);
    return mrk;
}

void Graph::insertLegend(const QStringList &lst, int fileVersion)
{
    legendMarkerID = insertTextMarker(lst, fileVersion);
    if (Legend *leg = legend())
        leg->setVisible(d_legend_visible);
}

long Graph::insertTextMarker(const QStringList &list, int fileVersion)
{
    QStringList fList = list;
    Legend *mrk = new Legend(d_plot);
    long key = d_plot->insertMarker(mrk);

    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = key;

    if (fileVersion < 86)
        mrk->setOrigin(QPoint(fList[1].toInt(), fList[2].toInt()));
    else
        mrk->setOriginCoord(fList[1].toDouble(), fList[2].toDouble());

    QFont fnt = QFont(fList[3], fList[4].toInt(), fList[5].toInt(), fList[6].toInt());
    fnt.setUnderline(fList[7].toInt());
    fnt.setStrikeOut(fList[8].toInt());
    mrk->setFont(fnt);

    mrk->setAngle(fList[11].toInt());

    QString text = QString();
    if (fileVersion < 71) {
        int bkg = fList[10].toInt();
        if (bkg <= 2)
            mrk->setFrameStyle(bkg);
        else if (bkg == 3) {
            mrk->setFrameStyle(0);
            mrk->setBackgroundColor(QColor(255, 255, 255));
        } else if (bkg == 4) {
            mrk->setFrameStyle(0);
            mrk->setBackgroundColor(QColor(Qt::black));
        }

        int n = (int)fList.count();
        for (int i = 0; i < n - 12; i++)
            text += fList[12 + i] + "\n";
    } else if (fileVersion < 90) {
        mrk->setTextColor(QColor(COLORVALUE(fList[9])));
        mrk->setFrameStyle(fList[10].toInt());
        mrk->setBackgroundColor(QColor(COLORVALUE(fList[12])));

        int n = (int)fList.count();
        for (int i = 0; i < n - 13; i++)
            text += fList[13 + i] + "\n";
    } else {
        mrk->setTextColor(QColor(COLORVALUE(fList[9])));
        mrk->setFrameStyle(fList[10].toInt());
        QColor c = QColor(COLORVALUE(fList[12]));
        c.setAlpha(fList[13].toInt());
        mrk->setBackgroundColor(c);

        int n = (int)fList.count();
        for (int i = 0; i < n - 14; i++)
            text += fList[14 + i] + "\n";
    }
    QString unescMarker = text.trimmed();
    unescMarker.replace(QLatin1String("&#9;"), QLatin1String("\t"));
    unescMarker.replace(QLatin1String("&#60;"), QLatin1String("<"));
    mrk->setText(unescMarker);
    return key;
}

void Graph::addArrow(QStringList list, int fileVersion)
{
    ArrowMarker *mrk = new ArrowMarker();
    long mrkID = d_plot->insertMarker(mrk);
    int linesOnPlot = (int)d_lines.size();
    d_lines.resize(++linesOnPlot);
    d_lines[linesOnPlot - 1] = mrkID;

    if (fileVersion < 86) {
        mrk->setStartPoint(QPoint(list[1].toInt(), list[2].toInt()));
        mrk->setEndPoint(QPoint(list[3].toInt(), list[4].toInt()));
    } else
        mrk->setBoundingRect(list[1].toDouble(), list[2].toDouble(), list[3].toDouble(),
                             list[4].toDouble());

    bool okW = false;
    const double lw = list[5].toDouble(&okW);
    mrk->setWidthF((okW && lw > 0.0) ? lw : double(qMax(1, list[5].toInt())));
    mrk->setColor(QColor(COLORVALUE(list[6])));
    mrk->setStyle(getPenStyle(list[7]));
    mrk->drawEndArrow(list[8] == "1");
    mrk->drawStartArrow(list[9] == "1");
    if (list.count() > 10) {
        mrk->setHeadLength(list[10].toInt());
        mrk->setHeadAngle(list[11].toInt());
        mrk->fillArrowHead(list[12] == "1");
    }
    if (list.count() > 13) {
        mrk->setCapStyle(static_cast<Qt::PenCapStyle>(list[13].toInt()));
        mrk->setJoinStyle(static_cast<Qt::PenJoinStyle>(list[14].toInt()));
        QVector<qreal> customDash;
        for (auto v : list[15].split(" "))
            customDash << v.toDouble();
        QPen pen = mrk->linePen();
        if (mrk->style() == Qt::CustomDashLine)
            pen.setDashPattern(customDash);
        mrk->setLinePen(pen);
    }
}

void Graph::addArrow(ArrowMarker *mrk)
{
    ArrowMarker *aux = new ArrowMarker();
    int linesOnPlot = (int)d_lines.size();
    d_lines.resize(++linesOnPlot);
    d_lines[linesOnPlot - 1] = d_plot->insertMarker(aux);

    aux->setBoundingRect(mrk->startPointCoord().x(), mrk->startPointCoord().y(),
                         mrk->endPointCoord().x(), mrk->endPointCoord().y());
    aux->setWidthF(mrk->widthF());
    aux->setColor(mrk->color());
    aux->setStyle(mrk->style());
    aux->setCapStyle(mrk->capStyle());
    aux->setJoinStyle(mrk->joinStyle());
    if (mrk->style() == Qt::CustomDashLine) {
        QPen pen = aux->linePen();
        pen.setDashPattern(mrk->linePen().dashPattern());
        aux->setLinePen(pen);
    }
    aux->drawEndArrow(mrk->hasEndArrow());
    aux->drawStartArrow(mrk->hasStartArrow());
    aux->setHeadLength(mrk->headLength());
    aux->setHeadAngle(mrk->headAngle());
    aux->fillArrowHead(mrk->filledArrowHead());
}

ArrowMarker *Graph::arrow(long id)
{
    return (ArrowMarker *)d_plot->marker(id);
}

ImageMarker *Graph::imageMarker(long id)
{
    return (ImageMarker *)d_plot->marker(id);
}

Legend *Graph::textMarker(long id)
{
    return (Legend *)d_plot->marker(id);
}

long Graph::insertTextMarker(Legend *mrk)
{
    Legend *aux = new Legend(d_plot);
    selectedMarker = d_plot->insertMarker(aux);
    detachAndDeleteMarkerSelector(d_markers_selector);

    int texts = d_texts.size();
    d_texts.resize(++texts);
    d_texts[texts - 1] = selectedMarker;

    aux->setTextColor(mrk->textColor());
    aux->setBackgroundColor(mrk->backgroundColor());
    aux->setOriginCoord(mrk->xValue(), mrk->yValue());
    aux->setFont(mrk->font());
    aux->setFrameStyle(mrk->frameStyle());
    aux->setAngle(mrk->angle());
    aux->setNumColumns(mrk->numColumns());
    aux->setText(mrk->text());
    return selectedMarker;
}

QString Graph::saveMarkers()
{
    QString s;
    int t = d_texts.size(), l = d_lines.size(), im = d_images.size();
    for (int i = 0; i < im; i++) {
        ImageMarker *mrkI = (ImageMarker *)d_plot->marker(d_images[i]);
        s += "<image>\t";
        s += mrkI->fileName() + "\t";
        s += QString::number(mrkI->xValue(), 'g', 15) + "\t";
        s += QString::number(mrkI->yValue(), 'g', 15) + "\t";
        s += QString::number(mrkI->right(), 'g', 15) + "\t";
        s += QString::number(mrkI->bottom(), 'g', 15) + "</image>\n";
    }

    for (int i = 0; i < l; i++) {
        ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]);
        s += "<line>\t";

        QwtDoublePoint sp = mrkL->startPointCoord();
        s += (QString::number(sp.x(), 'g', 15)) + "\t";
        s += (QString::number(sp.y(), 'g', 15)) + "\t";

        QwtDoublePoint ep = mrkL->endPointCoord();
        s += (QString::number(ep.x(), 'g', 15)) + "\t";
        s += (QString::number(ep.y(), 'g', 15)) + "\t";

        s += QString::number(mrkL->widthF(), 'g', 15) + "\t";
        s += COLORNAME(mrkL->color()) + "\t";
        s += penStyleName(mrkL->style()) + "\t";
        s += QString::number(mrkL->hasEndArrow()) + "\t";
        s += QString::number(mrkL->hasStartArrow()) + "\t";
        s += QString::number(mrkL->headLength()) + "\t";
        s += QString::number(mrkL->headAngle()) + "\t";
        s += QString::number(mrkL->filledArrowHead()) + "\t";
        s += QString::number(static_cast<int>(mrkL->capStyle())) + "\t";
        s += QString::number(static_cast<int>(mrkL->joinStyle())) + "\t";
        QStringList customDash;
        for (auto v : mrkL->linePen().dashPattern())
            customDash << QString::number(v);
        s += customDash.join(" ");
        s += "</line>\n";
    }

    for (int i = 0; i < t; i++) {
        Legend *mrk = (Legend *)d_plot->marker(d_texts[i]);
        if (d_texts[i] != legendMarkerID)
            s += "<text>\t";
        else
            s += "<legend>\t";

        s += QString::number(mrk->xValue(), 'g', 15) + "\t";
        s += QString::number(mrk->yValue(), 'g', 15) + "\t";

        QFont f = mrk->font();
        s += f.family() + "\t";
        s += QString::number(f.pointSize()) + "\t";
        s += QString::number(f.weight()) + "\t";
        s += QString::number(f.italic()) + "\t";
        s += QString::number(f.underline()) + "\t";
        s += QString::number(f.strikeOut()) + "\t";
        s += COLORNAME(mrk->textColor()) + "\t";
        s += QString::number(mrk->frameStyle()) + "\t";
        s += QString::number(mrk->angle()) + "\t";
        s += COLORNAME(mrk->backgroundColor()) + "\t";
        s += QString::number(mrk->backgroundColor().alpha()) + "\t";
        QString normMrk = mrk->text();
        normMrk.replace(QLatin1String("\r\n"), QLatin1String("\n"));
        normMrk.replace(QLatin1Char('\r'), QLatin1Char('\n'));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList textList = normMrk.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
#else
        QStringList textList = normMrk.split(QLatin1Char('\n'), QString::KeepEmptyParts);
#endif
        for (QString &ln : textList)
            ln = sanitizeProjectLineField(ln, false);
        s += textList.join("\t");
        if (d_texts[i] != legendMarkerID)
            s += "</text>\n";
        else
            s += "</legend>\n";
    }
    return s;
}

double Graph::selectedXStartValue()
{
    if (d_range_selector)
        return d_range_selector->minXValue();
    else
        return 0;
}

double Graph::selectedXEndValue()
{
    if (d_range_selector)
        return d_range_selector->maxXValue();
    else
        return 0;
}

QwtPlotItem *Graph::plotItem(int index)
{
    if (!n_curves || index >= n_curves || index < 0)
        return 0;

    return d_plot->plotItem(c_keys[index]);
}

int Graph::plotItemIndex(QwtPlotItem *it) const
{
    for (int i = 0; i < n_curves; i++) {
        if (d_plot->plotItem(c_keys[i]) == it)
            return i;
    }
    return -1;
}

QwtPlotCurve *Graph::curve(int index) const
{
    if (!n_curves || index >= n_curves || index < 0)
        return 0;

    return d_plot->curve(c_keys[index]);
}

int Graph::curveIndex(QwtPlotCurve *c) const
{
    return plotItemIndex(c);
}

int Graph::range(const int index, double& start, double& end) const
{
    if (d_range_selector && d_range_selector->selectedCurve() == curve(index)) {
        start = d_range_selector->minXValue();
        end = d_range_selector->maxXValue();
        return d_range_selector->dataSize();
    } else {
    QwtPlotCurve *c = curve(index);
        if (!c) return 0;
        start = c->minXValue();
        end = c->maxXValue();
        return c->dataSize();
    }
}

CurveLayout Graph::initCurveLayout()
{
    CurveLayout cl;
    cl.connectType = 1;
    cl.lStyle = 0;
    cl.lWidth = 2;
    cl.sSize = 9;
    cl.sType = defaultCurveSymbolIndex(0);
    cl.filledArea = 0;
    cl.aCol = 0;
    cl.aStyle = 0;
    const QList<QColor> defPalette = paletteColors(defaultCurvePaletteName());
    const QRgb firstCol = defPalette.isEmpty() ? QColor(Qt::black).rgba() : defPalette.first().rgba();
    cl.lCol = firstCol;
    cl.penWidth = 2;
    cl.symCol = firstCol;
    cl.fillCol = firstCol;
    cl.symbolFill = true;
    return cl;
}

CurveLayout Graph::initCurveLayout(int style, int curves)
{
    int i = n_curves - 1;

    CurveLayout cl = initCurveLayout();
    int color;
    guessUniqueCurveLayout(color, cl.sType);

    cl.lCol = color;
    cl.symCol = color;
    cl.fillCol = color;

    const QList<QColor> defPalette = paletteColors(defaultCurvePaletteName());
    const QColor okCol = defPalette.isEmpty() ? QColor(Qt::black)
                                              : defPalette.at(qMax(0, i) % defPalette.size());

    if (style == Graph::Line)
        cl.sType = 0;
    else if (style == Graph::VerticalDropLines)
        cl.connectType = 2;
    else if (style == Graph::HorizontalSteps || style == Graph::VerticalSteps) {
        cl.connectType = 3;
        cl.sType = 0;
    } else if (style == Graph::Spline)
        cl.connectType = 5;
    else if (style == Graph::VerticalBars || style == Graph::HorizontalBars) {
        cl.filledArea = 1;
        cl.lWidth = 1;
        cl.lCol = okCol.darker(140).rgba();
        cl.aCol = okCol.rgba();
        cl.fillCol = okCol.rgba();
        cl.aStyle = 0;
        cl.sType = 0;
        const int effBarCount = qMax(1, curves);
        QwtBarCurve *b = dynamic_cast<QwtBarCurve *>(curve(i));
        if (b) {
            if (b->isStacked() || effBarCount <= 1) {
                b->setGap(35);
                b->setOffset(0);
            } else {
                b->setGap(qBound(30, qRound(100.0 * (1.0 - 0.74 / double(effBarCount))), 92));
                b->setOffset(qRound((double(i) - 0.5 * double(effBarCount - 1)) * 106.0));
            }
        }
    } else if (style == Graph::Histogram) {
        cl.filledArea = 1;
        cl.lWidth = 1;
        cl.lCol = okCol.darker(140).rgba();
        QColor histFill = okCol;
        histFill.setAlpha(165);
        cl.aCol = histFill.rgba();
        cl.fillCol = histFill.rgba();
        cl.aStyle = 0;
        cl.sType = 0;
    } else if (style == Graph::Area) {
        cl.filledArea = 1;
        cl.aCol = color;
        cl.sType = 0;
    }
    return cl;
}

bool Graph::canConvertTo(QwtPlotCurve *c, CurveType type)
{
    if (!c)
        return false;
    // conversion between VectXYXY and VectXYAM is possible, but not implemented
    if (dynamic_cast<VectorCurve *>(c))
        return false;
    // conversion between Pie, Histogram and Box should be possible (all of them take one input
    // column), but lots of special-casing in ApplicationWindow and Graph makes this very difficult
    if (dynamic_cast<QwtPieCurve *>(c) || dynamic_cast<QwtHistogram *>(c)
        || dynamic_cast<BoxCurve *>(c))
        return false;
    // converting error bars doesn't make sense
    if (dynamic_cast<QwtErrorPlotCurve *>(c))
        return false;
    // line/symbol, area and bar curves can be converted to each other
    if (dynamic_cast<DataCurve *>(c))
        return type == Line || type == Scatter || type == LineSymbols || type == VerticalBars
                || type == HorizontalBars || type == HorizontalSteps || type == VerticalSteps
                || type == Area || type == VerticalDropLines || type == Spline;
    return false;
}

void Graph::setCurveType(int curve_index, CurveType type, bool update)
{
    if (curve_index < 0 || curve_index >= n_curves || curve_index >= c_type.size())
        return;
    CurveType old_type = static_cast<CurveType>(c_type[curve_index]);
    // nothing changed
    if (type == old_type)
        return;
    // not all types can be modified cleanly
    if (type != Line && type != Scatter && type != LineSymbols && type != VerticalBars
        && type != Area && type != VerticalDropLines && type != Spline && type != HorizontalSteps
        && type != HorizontalBars && type != VerticalSteps)
        return;
    if (old_type != Line && old_type != Scatter && old_type != LineSymbols
        && old_type != VerticalBars && old_type != Area && old_type != VerticalDropLines
        && old_type != Spline && old_type != HorizontalSteps && old_type != HorizontalBars
        && old_type != VerticalSteps)
        return;

    if ((type == VerticalBars || type == HorizontalBars)
        != (old_type == VerticalBars || old_type == HorizontalBars)) {
        // bar curves are represented by a different class than the other curve types, which makes
        // changing from/to horizontal or vertical bars more difficult
        PlotCurve *base_curve = dynamic_cast<PlotCurve *>(curve(curve_index));
        if (!base_curve || base_curve->type() == Function)
            return;
        DataCurve *old_curve = dynamic_cast<DataCurve *>(base_curve);
        if (!old_curve)
            return;
        DataCurve *new_curve = nullptr;
        switch (type) {
        case Histogram:
        case Box:
        case Pie:
            return;
        case VerticalBars:
            new_curve = new QwtBarCurve(QwtBarCurve::Vertical, old_curve->table(),
                                        old_curve->xColumnName(), old_curve->yColumnName(),
                                        old_curve->startRow(), old_curve->endRow());
            new_curve->setStyle(QwtPlotCurve::UserCurve);
            break;
        case HorizontalBars:
            new_curve = new QwtBarCurve(QwtBarCurve::Horizontal, old_curve->table(),
                                        old_curve->xColumnName(), old_curve->yColumnName(),
                                        old_curve->startRow(), old_curve->endRow());
            new_curve->setStyle(QwtPlotCurve::UserCurve);
            break;
        default:
            new_curve = new DataCurve(old_curve->table(), old_curve->xColumnName(),
                                      old_curve->yColumnName(), old_curve->startRow(),
                                      old_curve->endRow());
            break;
        }

        const bool wasVisible = old_curve->isVisible();
        old_curve->clearErrorBars();
        curve_index = plotItemIndex(old_curve);
        if (curve_index < 0 || curve_index >= n_curves || curve_index >= c_type.size()) {
            delete new_curve;
            return;
        }

        const int fitIdx = d_fit_curves.indexOf(old_curve);
        if (fitIdx >= 0)
            d_fit_curves.replace(fitIdx, new_curve);

        if (d_active_tool) {
            if (DataPickerTool *dpt = dynamic_cast<DataPickerTool *>(d_active_tool)) {
                if (dpt->selectedCurve() == old_curve)
                    setActiveTool(NULL);
            }
        }
        if (d_range_selector && old_curve == d_range_selector->selectedCurve())
            d_range_selector->setSelectedCurve(new_curve);

        const long oldKey = c_keys[curve_index];
        const bool wasHighlighted = (d_plot->highlightedCurveKey() == oldKey);
        d_plot->removeCurve(oldKey);
        old_curve = nullptr;
        c_keys[curve_index] = d_plot->insertCurve(new_curve);
        if (wasHighlighted)
            d_plot->setHighlightedCurveKey(c_keys[curve_index]);
        new_curve->setVisible(wasVisible);
        new_curve->loadData();
    }

    c_type[curve_index] = type;
    if (!update)
        return;

    CurveLayout cl = initCurveLayout(type, 1);
    updateCurveLayout(curve_index, &cl);
    updatePlot();
}

void Graph::updateCurveLayout(int index, const CurveLayout *cL)
{
    PlotCurve *c = dynamic_cast<PlotCurve *>(this->curve(index));
    if (!c)
        return;
    if (index < 0 || c_type.isEmpty() || (int)c_type.size() <= index)
        return;

    QPen pen = QPen(QBrush(ColorButton::color(cL->symCol)), cL->penWidth, Qt::SolidLine);
    if (cL->symbolFill)
        c->setSymbol(QwtSymbol(SymbolBox::style(cL->sType), QBrush(ColorButton::color(cL->fillCol)),
                               pen, QSize(cL->sSize, cL->sSize)));
    else
        c->setSymbol(
                QwtSymbol(SymbolBox::style(cL->sType), QBrush(), pen, QSize(cL->sSize, cL->sSize)));

    // custom dash pattern
    pen = QPen(QBrush(ColorButton::color(cL->lCol)), cL->lWidth, getPenStyle(cL->lStyle));
    pen.setCapStyle(static_cast<Qt::PenCapStyle>(cL->lCapStyle));
    pen.setJoinStyle(static_cast<Qt::PenJoinStyle>(cL->lJoinStyle));
    QVector<qreal> customDash;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    for (auto v : (cL->lCustomDash).split(" ", Qt::SkipEmptyParts))
#else
    for (auto v : (cL->lCustomDash).split(" ", QString::SkipEmptyParts))
#endif
        customDash << v.toDouble();
    if (pen.style() == Qt::CustomDashLine)
        pen.setDashPattern(customDash);
    c->setPen(pen);

    int style = c_type[index];
    if (style == Scatter || style == VectXYXY || style == VectXYAM)
        c->setStyle(QwtPlotCurve::NoCurve);
    else if (style == VerticalBars || style == HorizontalBars || style == Histogram || style == Box
             || style == Pie)
        c->setStyle(QwtPlotCurve::UserCurve);
    else if (style == Spline) {
        c->setStyle(QwtPlotCurve::Lines);
        c->setCurveAttribute(QwtPlotCurve::Fitted, true);
    } else if (style == VerticalSteps) {
        c->setStyle(QwtPlotCurve::Steps);
        c->setCurveAttribute(QwtPlotCurve::Inverted, true);
    } else
        c->setStyle((QwtPlotCurve::CurveStyle)cL->connectType);

    QBrush brush = QBrush(ColorButton::color(cL->aCol));
    if (cL->filledArea)
        brush.setStyle(getBrushStyle(cL->aStyle));
    else
        brush.setStyle(Qt::NoBrush);
    c->setBrush(brush);
}

void Graph::updateErrorBars(QwtErrorPlotCurve *er, bool xErr, int width, int cap, const QColor &c,
                            bool plus, bool minus, bool through)
{
    if (!er)
        return;

    if (er->width() == width && er->capLength() == cap && er->color() == c && er->plusSide() == plus
        && er->minusSide() == minus && er->throughSymbol() == through && er->xErrors() == xErr)
        return;

    er->setWidth(width);
    er->setCapLength(cap);
    er->setColor(c);
    er->setXErrors(xErr);
    er->drawThroughSymbol(through);
    er->drawPlusSide(plus);
    er->drawMinusSide(minus);

    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::addErrorBars(const QString &yColName, Table *errTable, const QString &errColName,
                         int type, int width, int cap, const QColor &color, bool through,
                         bool minus, bool plus)
{
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        DataCurve *c = dynamic_cast<DataCurve *>(d_plot->curve(c_keys[i]));
        if (c && c->title().text() == yColName && c_type[i] != ErrorBars) {
            return addErrorBars(c->xColumnName(), yColName, errTable, errColName, type, width, cap,
                                color, through, minus, plus);
        }
    }
    return false;
}

bool Graph::addErrorBars(const QString &xColName, const QString &yColName, Table *errTable,
                         const QString &errColName, int type, int width, int cap,
                         const QColor &color, bool through, bool minus, bool plus)
{
    DataCurve *master_curve = masterCurve(xColName, yColName);
    if (!master_curve)
        return false;

    QwtErrorPlotCurve *er = new QwtErrorPlotCurve(type, errTable, errColName);
    er->setMasterCurve(master_curve);
    er->setCapLength(cap);
    er->setColor(color);
    er->setWidth(width);
    er->drawPlusSide(plus);
    er->drawMinusSide(minus);
    er->drawThroughSymbol(through);
    er->setVisible(master_curve->isVisible());

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = ErrorBars;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(er);
    syncCurveZOrder();

    updatePlot();
    return true;
}

void Graph::plotPie(Table *w, const QString &name, const QPen &pen, int brush, int size,
                    int firstColor, int startRow, int endRow, bool visible)
{
    if (endRow < 0)
        endRow = w->numRows() - 1;

    QwtPieCurve *pieCurve = new QwtPieCurve(w, name, startRow, endRow);
    pieCurve->loadData();
    pieCurve->setPen(pen);
    pieCurve->setRay(size);
    pieCurve->setFirstColor(firstColor);
    pieCurve->setBrushStyle(getBrushStyle(brush));
    pieCurve->setVisible(visible);

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(pieCurve);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = Pie;

    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        enableAxis(a, false);
    drawAxesBackbones(false);
    drawCanvasFrame(false, 0);
    setFrame(0, Qt::black);
}

void Graph::plotPie(Table *w, const QString &name, int startRow, int endRow)
{
    const MarkerAxisState axesBefore = markerAxisState();
    int ycol = w->colIndex(name);
    int size = 0;
    double sum = 0.0;

    Column *y_col_ptr = w->column(ycol);
    auto yColType = w->columnType(ycol);

    if (endRow < 0)
        endRow = w->numRows() - 1;

    for (int i = 0; i < QwtPlot::axisCnt; i++)
        d_plot->enableAxis(i, false);
    scalePicker->refresh();

    d_plot->setTitle(QString());
    drawCanvasFrame(false, 0);
    setFrame(0, Qt::black);

    QVarLengthArray<double> Y(abs(endRow - startRow) + 1);
    QStringList sliceNames;
    int labelCol = -1;
    for (int c = 0; c < w->numCols(); ++c) {
        if (c == ycol || !w->column(c))
            continue;
        if (w->column(c)->columnMode() == SciDAVis::ColumnMode::Text
            || w->colPlotDesignation(c) == SciDAVis::X) {
            labelCol = c;
            break;
        }
    }
    Column *lblColPtr = (labelCol >= 0) ? w->column(labelCol) : nullptr;

    for (int row = startRow; row <= endRow && row < y_col_ptr->rowCount(); row++) {
        if (!y_col_ptr->isInvalid(row) && !y_col_ptr->isMasked(row)) {
            double val = 0.0;
            if (yColType == SciDAVis::ColumnMode::Text) {
                QString yval = y_col_ptr->textAt(row);
                bool valid_data = true;
                val = QLocale().toDouble(yval, &valid_data);
                if (!valid_data)
                    continue;
            } else {
                val = y_col_ptr->valueAt(row);
            }
            if (!std::isfinite(val) || val <= 0.0)
                continue;

            Y[size] = val;
            sum += val;
            QString sLbl;
            if (lblColPtr && row < lblColPtr->rowCount() && !lblColPtr->isInvalid(row)) {
                sLbl = (lblColPtr->columnMode() == SciDAVis::ColumnMode::Text)
                        ? lblColPtr->textAt(row).trimmed()
                        : QLocale::c().toString(lblColPtr->valueAt(row), 'g', 6);
            }
            if (sLbl.isEmpty())
                sLbl = QString::number(size + 1);
            sliceNames << sLbl;
            size++;
        }
    }
    if (!size || sum <= 0.0)
        return;
    Y.resize(size);

    QwtPieCurve *pieCurve = new QwtPieCurve(w, name, startRow, endRow);
    pieCurve->setData(Y.data(), Y.data(), size);

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(pieCurve);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = Pie;

    double maxPieVal = 1.0;
    for (int i = 0; i < size; ++i) {
        if (std::isfinite(Y[i]) && Y[i] > maxPieVal)
            maxPieVal = Y[i];
    }
    m_autoscale = false;
    setScale(QwtPlot::xBottom, -2.0 * maxPieVal, 3.0 * maxPieVal, 0.0, 5, 0, 0, false);
    setScale(QwtPlot::yLeft, -1.2 * maxPieVal, 3.8 * maxPieVal, 0.0, 5, 0, 0, false);
    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        enableAxis(a, false);
    drawAxesBackbones(false);
    drawCanvasFrame(false, 0);

    d_plot->updateLayout();
    QRect canvas_rect = d_plot->plotLayout()->canvasRect();
    if (canvas_rect.width() < 60 || canvas_rect.height() < 60)
        canvas_rect = QRect(0, 0, 420, 420);

    bool legendShowsPct = false;
    int estLegW = 0;
    int estLegH = 0;
    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString text;
            for (int i = 0; i < size; i++) {
                text += "\\p{" + QString::number(i + 1) + "} " + sliceNames.value(i) + "\n";
            }
            mrk->setText(text);
            legendShowsPct = text.contains(QLatin1Char('%'));
            QwtText qtLeg(text);
            qtLeg.setFont(mrk->font());
            const QSize lsz = qtLeg.textSize();
            estLegW = lsz.width() + 28;
            estLegH = lsz.height() + 10;
        }
    }

    const bool hasVisPieLeg = (legendMarkerID >= 0) && isLegendVisible();
    const bool topBandPieLeg =
            hasVisPieLeg && (estLegW > int(0.34 * double(canvas_rect.width())));
    double pieRadius = 0.285 * qMin(canvas_rect.width(), canvas_rect.height());
    double pieCx = double(canvas_rect.center().x());
    double pieCy = double(canvas_rect.center().y());
    if (topBandPieLeg) {
        const double legBottom =
                qBound(0.08 * double(canvas_rect.height()), double(4 + estLegH),
                       0.52 * double(canvas_rect.height()));
        const double pad = 0.028 * double(canvas_rect.height());
        const double availH =
                qMax(0.20 * double(canvas_rect.height()),
                     double(canvas_rect.height()) - legBottom - 2.0 * pad);
        pieRadius = qMax(14.0,
                         qMin((legendShowsPct ? 0.31 : 0.255) * double(canvas_rect.width()),
                              (legendShowsPct ? 0.44 : 0.34) * availH));
        pieCx = double(canvas_rect.center().x());
        pieCy = double(canvas_rect.top()) + legBottom + pad + 0.5 * availH;
    } else if (hasVisPieLeg) {
        pieRadius = (legendShowsPct ? 0.285 : 0.24)
                * qMin(canvas_rect.width(), canvas_rect.height());
        pieCx = double(canvas_rect.left()) + 0.36 * double(canvas_rect.width());
        pieCy = double(canvas_rect.top()) + 0.52 * double(canvas_rect.height());
    }
    const double radius = 1.22 * pieRadius;

    const double PI = 4.0 * atan(1.0);
    double angle = 90.0;
    QFont pctFont(QStringLiteral("Arial"), 10, QFont::Normal, false);
    pctFont.setBold(false);
    pctFont.setHintingPreference(QFont::PreferNoHinting);

    QVector<QRect> placedPctBoxes;
    for (int i = 0; i < size; i++) {
        const double pctVal = Y[i] / sum * 100.0;
        const double value = Y[i] / sum * 360.0;
        const double alabel = (angle - value * 0.5) * PI / 180.0;
        const QString pctStr =
                QLocale::c().toString(pctVal, 'f', 1) + QStringLiteral("%");

        if (!legendShowsPct) {
            Legend *aux = new Legend(d_plot);
            aux->setFrameStyle(0);
            aux->setBackgroundColor(QColor(255, 255, 255, 0));
            aux->setFont(pctFont);
            aux->setText(pctStr);

            int texts = d_texts.size();
            d_texts.resize(++texts);
            d_texts[texts - 1] = d_plot->insertMarker(aux);

            QwtText qtPct(pctStr);
            qtPct.setFont(pctFont);
            const QSize pctSz = qtPct.textSize();
            const int boxW = pctSz.width() + 8;
            const int boxH = pctSz.height() + 4;
            int ox = qBound(2, qRound(pieCx + radius * cos(alabel) - 0.5 * double(boxW)),
                            qMax(2, canvas_rect.width() - boxW - 2));
            int oy = qBound(2, qRound(pieCy - radius * sin(alabel) - 0.5 * double(boxH)),
                            qMax(2, canvas_rect.height() - boxH - 2));
            QRect candBox(ox, oy, boxW, boxH);
            bool collides = false;
            for (const QRect &prev : placedPctBoxes) {
                if (candBox.intersects(prev)) {
                    collides = true;
                    break;
                }
            }
            if (collides) {
                const int dirY = (sin(alabel) >= 0.0) ? -1 : 1;
                for (int step = 1; step <= 4 && collides; ++step) {
                    const int tryY = qBound(2, oy + dirY * step * (boxH - 2),
                                            qMax(2, canvas_rect.height() - boxH - 2));
                    const QRect tryBox(ox, tryY, boxW, boxH);
                    bool hitAny = false;
                    for (const QRect &prev : placedPctBoxes) {
                        if (tryBox.intersects(prev)) {
                            hitAny = true;
                            break;
                        }
                    }
                    if (!hitAny) {
                        oy = tryY;
                        candBox = tryBox;
                        collides = false;
                    }
                }
            }
            if (collides && pctVal < 6.0) {
                aux->setText(QString());
                aux->setVisible(false);
            } else {
                aux->setOrigin(QPoint(ox, oy));
                placedPctBoxes.append(candBox);
            }
        }

        angle -= value;
    }

    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk)
            placeLegendAnchor(topBandPieLeg ? LegendTopCenter : LegendTopRight);
    }

    d_plot->replot();
    updateScale(axesBefore);
}

bool Graph::plotHistogram(Table *w, QStringList names, int startRow, int endRow)
{
    if (!w)
        return false;
    if (endRow < 0 || endRow >= w->numRows())
        endRow = w->numRows() - 1;

    bool success = false;
    foreach (QString col, names) {
        Column *col_ptr = w->column(col);
        if (!col_ptr || col_ptr->columnMode() != SciDAVis::ColumnMode::Numeric)
            continue;

        QwtHistogram *c = new QwtHistogram(w, col, startRow, endRow);
        c->loadData();
        c->setStyle(QwtPlotCurve::UserCurve);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Histogram;
        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);
        syncCurveZOrder();

        CurveLayout cl = initCurveLayout(Histogram, names.size());
        updateCurveLayout(n_curves - 1, &cl);
        QBrush hb = c->brush();
        QColor hc = hb.color();
        hc.setAlpha(165);
        hb.setColor(hc);
        hb.setStyle(Qt::SolidPattern);
        c->setBrush(hb);

        addLegendItem(col);

        success = true;
    }
    if (success && !d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();

    return success;
}

void Graph::insertPlotItem(QwtPlotItem *i, int type)
{
    c_type.resize(++n_curves);
    c_type[n_curves - 1] = type;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(i);
    syncCurveZOrder();

    if (i->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
        addLegendItem(i->title().text());
}

bool Graph::insertCurvesList(Table *w, const QStringList &names, int style, int lWidth, int sSize,
                             int startRow, int endRow)
{
    if (style == Graph::Pie)
        plotPie(w, names[0], startRow, endRow);
    else if (style == Box)
        plotBoxDiagram(w, names, startRow, endRow);
    else if (style == Histogram)
        plotHistogram(w, names, startRow, endRow);
    else if (style == Graph::VectXYXY || style == Graph::VectXYAM) {
        const int prevCurves = n_curves;
        plotVectorCurve(w, names, style, startRow, endRow);
        if (n_curves <= prevCurves)
            return false;
    } else {
        QStringList errorCurves, otherCurves;
        foreach (QString col, names) {
            int colIndex = w->colIndex(col);
            if (colIndex < 0)
                continue;
            switch (w->colPlotDesignation(colIndex)) {
            case SciDAVis::xErr:
            case SciDAVis::yErr:
                errorCurves << col;
                break;
            default:
                otherCurves << col;
                break;
            }
        }
        QStringList lst = otherCurves + errorCurves;

        for (int i = 0; i < lst.size(); i++) {
            CurveType type_of_i;
            int j = w->colIndex(lst[i]);
            if (j < 0)
                continue;
            bool ok = false;
            if (i >= otherCurves.size()) {
                type_of_i = ErrorBars;
                int ycol = -1;
                for (int k = j - 1; k >= 0; k--) {
                    if (w->colPlotDesignation(k) == SciDAVis::Y) {
                        ycol = k;
                        break;
                    }
                }
                if (ycol < 0)
                    ycol = w->colY(w->colIndex(lst[i]));
                if (ycol < 0)
                    return false;

                if (w->colPlotDesignation(j) == SciDAVis::xErr)
                    ok = addErrorBars(w->colName(ycol), w, lst[i],
                                      (int)QwtErrorPlotCurve::Horizontal);
                else
                    ok = addErrorBars(w->colName(ycol), w, lst[i]);
            } else {
                type_of_i = (CurveType)style;
                ok = insertCurve(w, lst[i], style, startRow, endRow);
            }

            if (ok) {
                CurveLayout cl = initCurveLayout(type_of_i, otherCurves.size());
                cl.sSize = sSize;
                cl.lWidth = lWidth;
                updateCurveLayout(i, &cl);
            } else
                return false;
        }
    }
    updatePlot();
    return true;
}

bool Graph::insertCurve(Table *w, const QString &name, int style, int startRow, int endRow)
{ // provided for convenience
    int ycol = w->colIndex(name);
    int xcol = w->colX(ycol);

    bool succes = insertCurve(w, w->colName(xcol), w->colName(ycol), style, startRow, endRow);
    if (succes)
        emit modifiedGraph();
    return succes;
}

bool Graph::insertCurve(Table *w, int xcol, const QString &name, int style)
{
    return insertCurve(w, w->colName(xcol), w->colName(w->colIndex(name)), style);
}

bool Graph::insertCurve(Table *w, const QString &xColName, const QString &yColName, int style,
                        int startRow, int endRow)
{
    if (!w)
        return false;
    DataCurve *c = 0;

    switch (style) {
    case Histogram:
    case Box:
    case Pie:
        return false;
    case VerticalBars:
        c = new QwtBarCurve(QwtBarCurve::Vertical, w, xColName, yColName, startRow, endRow);
        c->setStyle(QwtPlotCurve::UserCurve);
        break;
    case HorizontalBars:
        c = new QwtBarCurve(QwtBarCurve::Horizontal, w, xColName, yColName, startRow, endRow);
        c->setStyle(QwtPlotCurve::UserCurve);
        break;
    default:
        c = new DataCurve(w, xColName, yColName, startRow, endRow);
        break;
    };

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = style;
    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);
    syncCurveZOrder();

    const QList<QColor> barPalette = paletteColors(defaultCurvePaletteName());
    if (style == VerticalBars || style == HorizontalBars) {
        QVector<QwtBarCurve *> allStyleBars;
        QVector<QwtBarCurve *> groupedBars;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (bi < c_type.size() && c_type[bi] == style) {
                if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(curve(bi))) {
                    allStyleBars.append(bc);
                    if (!bc->isStacked())
                        groupedBars.append(bc);
                    else
                        bc->setOffset(0);
                }
            }
        }
        const int barIdx = qMax(0, allStyleBars.size() - 1);
        const QColor fillCol = barPalette.isEmpty()
                ? QColor(Qt::black)
                : barPalette.at(barIdx % barPalette.size());
        QPen barPen(fillCol.darker(140), 1, Qt::SolidLine);
        barPen.setCosmetic(false);
        c->setPen(barPen);
        c->setBrush(QBrush(fillCol, Qt::SolidPattern));

        const int K = groupedBars.size();
        for (int ord = 0; ord < K; ++ord) {
            if (K <= 1) {
                groupedBars[ord]->setGap(35);
                groupedBars[ord]->setOffset(0);
            } else {
                groupedBars[ord]->setGap(
                        qBound(30, qRound(100.0 * (1.0 - 0.74 / double(K))), 92));
                groupedBars[ord]->setOffset(
                        qRound((double(ord) - 0.5 * double(K - 1)) * 106.0));
            }
        }
    } else {
        c->setPen(QPen(Qt::black, widthLine));
    }

    if (!c->loadData())
        return false;

    if (style == HorizontalBars) {
        bool anyHorizTextCurve = false;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (bi < c_type.size() && c_type[bi] != HorizontalBars && c_type[bi] != ErrorBars) {
                if (DataCurve *odc = dynamic_cast<DataCurve *>(curve(bi))) {
                    if (odc->table()) {
                        Column *oxc = odc->table()->column(odc->xColumnName());
                        if (oxc && oxc->columnMode() == SciDAVis::ColumnMode::Text)
                            anyHorizTextCurve = true;
                    }
                }
            }
        }
        if (!anyHorizTextCurve
            && (axisType[QwtPlot::xBottom] == AxisType::Txt
                || dynamic_cast<QwtTextScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::xBottom)))) {
            d_custom_category_labels[QwtPlot::xBottom].clear();
            axesFormatInfo[QwtPlot::xBottom].clear();
            setLabelsNumericFormat(QwtPlot::xBottom, Plot::Automatic, 4);
            setToriAxisUserLocked(d_plot, QwtPlot::xBottom, false);
            d_user_step[QwtPlot::xBottom] = 0.0;
            d_plot->setAxisAutoScale(QwtPlot::xBottom);
        }
    }

    addLegendItem(yColName);
    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();
    updatePlot();

    return true;
}

void Graph::setCurveAxes(int curveIndex, int xAxis, int yAxis)
{
    if (!d_plot || curveIndex < 0 || curveIndex >= n_curves)
        return;
    if (xAxis != QwtPlot::xBottom && xAxis != QwtPlot::xTop)
        xAxis = QwtPlot::xBottom;
    if (yAxis != QwtPlot::yLeft && yAxis != QwtPlot::yRight)
        yAxis = QwtPlot::yLeft;

    QwtPlotItem *it = plotItem(curveIndex);
    if (!it)
        return;
    it->setAxis(xAxis, yAxis);
    if (DataCurve *dc = dynamic_cast<DataCurve *>(it)) {
        for (DataCurve *eb : dc->errorBarsList()) {
            if (eb)
                eb->setAxis(xAxis, yAxis);
        }
    }

    auto ensureOppositeAxisVisible = [&](int secAxis, int primAxis) {
        enableAxis(secAxis, true);
        setAxisBackboneVisible(secAxis, true);
        if (QwtScaleDraw *sd = d_plot->axisScaleDraw(secAxis))
            sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        if (ScaleDraw *s = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(secAxis))) {
            const ScaleDraw *primSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(primAxis));
            const int targetMaj = (s->majorTicksStyle() == ScaleDraw::None)
                    ? (primSd && primSd->majorTicksStyle() != ScaleDraw::None
                               ? primSd->majorTicksStyle()
                               : int(ScaleDraw::In))
                    : s->majorTicksStyle();
            const int targetMin = (s->minorTicksStyle() == ScaleDraw::None && primSd)
                    ? primSd->minorTicksStyle()
                    : s->minorTicksStyle();
            d_plot->setMajorTicksType(secAxis, targetMaj);
            d_plot->setMinorTicksType(secAxis, targetMin);
        }
        if (!isAxisUserLocked(secAxis))
            d_plot->setAxisAutoScale(secAxis);
    };

    if (yAxis == QwtPlot::yRight)
        ensureOppositeAxisVisible(QwtPlot::yRight, QwtPlot::yLeft);
    if (xAxis == QwtPlot::xTop)
        ensureOppositeAxisVisible(QwtPlot::xTop, QwtPlot::xBottom);

    if (m_autoscale) {
        if (!isAxisUserLocked(QwtPlot::yLeft))
            d_plot->setAxisAutoScale(QwtPlot::yLeft);
        if (!isAxisUserLocked(QwtPlot::xBottom))
            d_plot->setAxisAutoScale(QwtPlot::xBottom);
    }

    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();
    if (scalePicker)
        scalePicker->refresh();
    updatePlot();
    emit modifiedGraph();
}

void Graph::plotVectorCurve(Table *w, const QStringList &colList, int style, int startRow,
                            int endRow)
{
    if (!w || !w->d_future_table || colList.count() != 4)
        return;

    auto resolveColIdx = [w](const QString &spec) -> int {
        if (spec.isEmpty())
            return -1;
        const int numCols = w->numCols();
        const QString tblPrefix = QString(w->name()) + QLatin1Char('_');
        if (spec.startsWith(tblPrefix)) {
            const QString shortName = spec.mid(tblPrefix.size());
            for (int i = 0; i < numCols; ++i) {
                if (const Column *col = w->column(i)) {
                    if (col->name() == shortName)
                        return i;
                }
            }
        }
        for (int i = 0; i < numCols; ++i) {
            if (const Column *col = w->column(i)) {
                if (col->name() == spec)
                    return i;
            }
        }
        const int fallback = w->colIndex(spec);
        return (fallback >= 0 && fallback < numCols && w->column(fallback) != nullptr) ? fallback
                                                                                       : -1;
    };

    QStringList canonicalCols;
    canonicalCols.reserve(4);
    for (int k = 0; k < 4; ++k) {
        const int idx = resolveColIdx(colList[k]);
        if (idx < 0)
            return;
        canonicalCols.append(w->colName(idx));
    }

    if (endRow < 0)
        endRow = w->numRows() - 1;

    VectorCurve *v = 0;
    if (style == VectXYAM)
        v = new VectorCurve(VectorCurve::XYAM, w, canonicalCols[0], canonicalCols[1],
                            canonicalCols[2], canonicalCols[3], startRow, endRow);
    else
        v = new VectorCurve(VectorCurve::XYXY, w, canonicalCols[0], canonicalCols[1],
                            canonicalCols[2], canonicalCols[3], startRow, endRow);

    if (!v)
        return;

    if (!v->loadData()) {
        delete v;
        return;
    }
    v->setStyle(QwtPlotCurve::NoCurve);

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = style;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(v);

    addLegendItem(canonicalCols[1]);
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QString plainT = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
        if (isAxisPlaceholderTitle(plainT)) {
            QwtText emptyT = d_plot->axisTitle(a);
            emptyT.setText(QString());
            d_plot->setAxisTitle(a, emptyT);
        }
    }
    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();
    updatePlot();
}

void Graph::updateVectorsLayout(int curve, const QColor &color, int width, int arrowLength,
                                int arrowAngle, bool filled, int position,
                                const QString &xEndColName, const QString &yEndColName)
{
    VectorCurve *vect = (VectorCurve *)this->curve(curve);
    if (!vect)
        return;

    vect->setColor(color);
    vect->setWidth(width);
    vect->setHeadLength(arrowLength);
    vect->setHeadAngle(arrowAngle);
    vect->fillArrowHead(filled);
    vect->setPosition(position);

    if (!xEndColName.isEmpty() && !yEndColName.isEmpty())
        vect->setVectorEnd(xEndColName, yEndColName);

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::updatePlot()
{
    const MarkerAxisState axesBefore = markerAxisState();
    if (d_is_ternary) {
        d_plot->setAxisScale(QwtPlot::xBottom, -0.24, 1.24, 0.2);
        d_plot->setAxisScale(QwtPlot::yLeft, -0.25, 1.23, 0.2);
        for (int i = 0; i < QwtPlot::axisCnt; i++)
            d_plot->enableAxis(i, false);
        drawAxesBackbones(false);
    } else if (d_is_polar) {
        const double rMax = qMax(1e-6, d_polar_rmax);
        const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
        const int extraRows = qMax(0, nPolarRows - 1);
        const double yLo = -1.44 * rMax;
        const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
        const double halfX = 0.5 * (yHi - yLo);
        d_plot->setAxisScale(QwtPlot::xBottom, -halfX, halfX);
        d_plot->setAxisScale(QwtPlot::yLeft, yLo, yHi);
        for (int i = 0; i < QwtPlot::axisCnt; i++)
            d_plot->enableAxis(i, false);
        drawAxesBackbones(false);
    } else if (isPoleFigurePlot()) {
        const double lim = ExtraPlotTypes::poleFigureScaleLimit(this);
        d_plot->setAxisScale(QwtPlot::xBottom, -lim, lim);
        d_plot->setAxisScale(QwtPlot::yLeft, -lim, lim);
        d_plot->enableAxis(QwtPlot::xBottom, false);
        d_plot->enableAxis(QwtPlot::yLeft, false);
        d_plot->enableAxis(QwtPlot::xTop, false);
        drawCanvasFrame(false, 0);
        drawAxesBackbones(false);
    } else if (m_autoscale && !zoomOn() && d_active_tool == NULL) {
        for (int i = 0; i < QwtPlot::axisCnt; i++) {
            if (!isToriAxisUserLocked(d_plot, i) && !d_weibull_prob_axis[i]
                && d_custom_tick_positions[i].isEmpty())
                d_plot->setAxisAutoScale(i);
        }
        applyZeroSnapOnAutoAxes();
    }

    // Lock discrete category axes (AxisType::Txt) to [minKey - 0.5, maxKey + 0.5, step = 1.0]
    // (preserving explicit category headroom when user-locked) with 0 minor ticks.
    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (const QwtTextScaleDraw *tsd =
                        dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(a))) {
                const QMap<int, QString> catMap = tsd->labelsMap();
                if (!catMap.isEmpty() && !zoomOn()) {
                    const double minKey = double(catMap.firstKey());
                    const double maxKey = double(catMap.lastKey());
                    double cLo = minKey - 0.5;
                    double cHi = maxKey + 0.5;
                    if (isToriAxisUserLocked(d_plot, a)) {
                        if (const QwtScaleDiv *curDiv = d_plot->axisScaleDiv(a)) {
                            const double curLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
                            const double curHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
                            if (std::isfinite(curLo) && std::isfinite(curHi) && curHi > curLo
                                && curLo >= minKey - 1.5 && curLo <= minKey
                                && curHi >= maxKey && curHi <= maxKey + 4.5) {
                                cLo = qMin(cLo, curLo);
                                cHi = qMax(cHi, curHi);
                            }
                        }
                    }
                    const bool isInvCat = d_plot->axisScaleEngine(a)
                            && d_plot->axisScaleEngine(a)->testAttribute(
                                    QwtScaleEngine::Inverted);
                    QwtValueList catMaj;
                    for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
                        catMaj += double(it.key());
                    QwtValueList emptyTicks;
                    QwtValueList catTicks[QwtScaleDiv::NTickTypes];
                    catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
                    catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
                    catTicks[QwtScaleDiv::MajorTick] = catMaj;
                    QwtScaleEngine *catEng = new QwtLinearScaleEngine();
                    QwtScaleDiv catDiv(cLo, cHi, catTicks);
                    if (isInvCat) {
                        catEng->setAttribute(QwtScaleEngine::Inverted);
                        catDiv.invert();
                    }
                    d_plot->setAxisMaxMinor(a, 0);
                    d_plot->setMinorTicksType(a, ScaleDraw::None);
                    d_plot->setAxisMaxMajor(a, qMax(8, catMap.size() + 2));
                    d_plot->setAxisScale(a, isInvCat ? cHi : cLo, isInvCat ? cLo : cHi, 1.0);
                    d_plot->setAxisScaleEngine(a, catEng);
                    d_plot->setAxisScaleDiv(a, catDiv);

                    const int opp = (a == QwtPlot::xBottom) ? QwtPlot::xTop
                            : (a == QwtPlot::yLeft)         ? QwtPlot::yRight
                                                            : -1;
                    if (opp >= 0 && !d_plot->hasVisibleItemOnAxis(opp)
                        && !hasColorScaleOnAxis(opp) && !d_weibull_prob_axis[opp]
                        && d_custom_tick_positions[opp].isEmpty()) {
                        if (QwtScaleDraw *oppSd = d_plot->axisScaleDraw(opp))
                            oppSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
                        d_plot->setMinorTicksType(opp, ScaleDraw::None);
                        d_plot->setMajorTicksType(opp, ScaleDraw::None);
                    }
                }
            }
        }
    }

    // Clamp extreme ±1e308 auto-scaled spans before QwtLinearScaleEngine::divideScale can spin
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QwtScaleDiv *sd = d_plot->axisScaleDiv(a);
        if (!sd)
            continue;
        const double lo = sd->lowerBound();
        const double hi = sd->upperBound();
        if (!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(hi - lo)
            || std::fabs(lo) > 1.0e300 || std::fabs(hi) > 1.0e300) {
            const double cLo = std::isfinite(lo) ? qBound(-1.0e150, lo, 1.0e150) : -1.0;
            const double cHi = std::isfinite(hi) ? qBound(-1.0e150, hi, 1.0e150) : 1.0;
            d_plot->setAxisScale(a, cLo, (std::fabs(cHi - cLo) > 1e-300) ? cHi : (cLo + 1.0));
        }
    }

    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
        syncCustomAndWeibullAxes();
    }

    d_plot->updateAxes();
    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot()) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    if (isPiePlot()) {
        drawCanvasFrame(false, 0);
        setFrame(0, Qt::black);
        drawAxesBackbones(false);
        if (QwtPieCurve *c = (QwtPieCurve *)curve(0))
            c->updateBoundingRect();
    }

    updateMarkersBoundingRect(axesBefore);
    d_zoomer[0]->setZoomBase(false);
    d_zoomer[1]->setZoomBase(false);
    updateStatReferenceLines();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();
}

void Graph::updateScale()
{
    updateScale(markerAxisState());
}

void Graph::updateScale(const MarkerAxisState &axesBefore)
{
    const QwtScaleDiv *scDiv = d_plot->axisScaleDiv(QwtPlot::xBottom);
    QwtValueList lst = scDiv->ticks(QwtScaleDiv::MajorTick);

    double step = (lst.count() >= 2) ? fabs(lst[1] - lst[0]) : 0.0;

    if (!m_autoscale)
#if QWT_VERSION >= 0x050200
        d_plot->setAxisScale(QwtPlot::xBottom, scDiv->lowerBound(), scDiv->upperBound(), step);
#else
        d_plot->setAxisScale(QwtPlot::xBottom, scDiv->lBound(), scDiv->hBound(), step);
#endif

    scDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
    lst = scDiv->ticks(QwtScaleDiv::MajorTick);

    step = (lst.count() >= 2) ? fabs(lst[1] - lst[0]) : 0.0;

    if (!m_autoscale)
#if QWT_VERSION >= 0x050200
        d_plot->setAxisScale(QwtPlot::yLeft, scDiv->lowerBound(), scDiv->upperBound(), step);
#else
        d_plot->setAxisScale(QwtPlot::yLeft, scDiv->lBound(), scDiv->hBound(), step);
#endif

    d_plot->updateAxes();
    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);
    updateMarkersBoundingRect(axesBefore);
    syncChildLinkedLayers();
}

void Graph::setBarsGap(int curve, int gapPercent, int offset)
{
    QwtBarCurve *bars = dynamic_cast<QwtBarCurve *>(this->curve(curve));
    if (!bars)
        return;

    if (bars->isStacked()) {
        offset = 0;
    } else if (std::abs(offset) > 10) {
        int groupedCount = 0;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(this->curve(bi))) {
                if (!bc->isStacked() && bc->orientation() == bars->orientation())
                    ++groupedCount;
            }
        }
        if (groupedCount > 1 && gapPercent < 100 - qRound(88.0 / double(groupedCount))) {
            gapPercent = qBound(
                    25,
                    100 - qRound((100.0 - double(qBound(0, gapPercent, 90))) / double(groupedCount)),
                    94);
        }
    }

    if (bars->gap() == gapPercent && bars->offset() == offset)
        return;

    bars->setGap(gapPercent);
    bars->setOffset(offset);
}

void Graph::removePie()
{
    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk)
            mrk->setText({});
    }

    d_plot->removeCurve(c_keys[0]);
    d_plot->replot();

    c_keys.resize(0);
    c_type.resize(0);
    n_curves = 0;
    emit modifiedGraph();
}

void Graph::removeCurves(const QString &s)
{
    for (int i = n_curves - 1; i >= 0; --i) {
        if (i >= n_curves)
            continue;
        QwtPlotItem *it = plotItem(i);
        if (!it)
            continue;

        if (it->title().text() == s) {
            removeCurve(i);
            continue;
        }

        if (it->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!pc || pc->type() == Function)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (!dc)
            continue;

        if (dc->plotAssociation().contains(QRegExp("\\b" + QRegExp::escape(s) + "\\b"))
            || dc->plotAssociation().contains(QRegExp("\b" + s + "\b")))
            removeCurve(i);
    }
    d_plot->replot();
}

void Graph::removeCurve(const QString &s)
{
    removeCurve(plotItemsList().indexOf(s));
}

void Graph::removeCurve(int index)
{
    if (index < 0 || index >= n_curves)
        return;

    QwtPlotItem *it = plotItem(index);
    if (!it)
        return;

    removeLegendItem(index);

    if (it->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (pc && pc->type() == ErrorBars) {
            if (QwtErrorPlotCurve *epc = dynamic_cast<QwtErrorPlotCurve *>(pc)) {
                if (epc->masterCurve())
                    epc->detachFromMasterCurve();
            }
        } else if (pc && pc->type() != Function) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(pc))
                dc->clearErrorBars();
        }

        if (d_fit_curves.contains((QwtPlotCurve *)it)) {
            int i = d_fit_curves.indexOf((QwtPlotCurve *)it);
            if (i >= 0 && i < d_fit_curves.size())
                d_fit_curves.removeAt(i);
        }
    }

    // Re-resolve index in case clearErrorBars() removed items before it
    index = plotItemIndex(it);
    if (index < 0 || index >= n_curves)
        return;

    QwtPlotCurve *removedCurve = curve(index);
    if (d_active_tool) {
        if (DataPickerTool *dpt = dynamic_cast<DataPickerTool *>(d_active_tool)) {
            if (!removedCurve || dpt->selectedCurve() == removedCurve)
                setActiveTool(NULL);
        } else if (d_active_tool->rtti() == PlotToolInterface::TranslateCurve
                   || d_active_tool->rtti() == PlotToolInterface::MultiPeak) {
            setActiveTool(NULL);
        }
    }

    if (d_range_selector && (!removedCurve || removedCurve == d_range_selector->selectedCurve())) {
        QwtPlotCurve *replacement = nullptr;
        for (int delta = 1; delta < n_curves && !replacement; ++delta) {
            const int candIndices[2] = { index - delta, index + delta };
            for (int ci : candIndices) {
                if (ci < 0 || ci >= n_curves || ci == index)
                    continue;
                if (c_type[ci] == ErrorBars)
                    continue;
                QwtPlotCurve *cand = curve(ci);
                if (cand && cand->rtti() == QwtPlotItem::Rtti_PlotCurve && cand->dataSize() > 0) {
                    replacement = cand;
                    break;
                }
            }
        }
        if (replacement)
            d_range_selector->setSelectedCurve(replacement);
        else
            delete d_range_selector;
    }

    if (d_plot->highlightedCurveKey() == c_keys[index])
        d_plot->setHighlightedCurveKey(-1);

    d_plot->removeCurve(c_keys[index]);
    n_curves--;

    for (int i = index; i < n_curves; i++) {
        c_type[i] = c_type[i + 1];
        c_keys[i] = c_keys[i + 1];
    }
    c_type.resize(n_curves);
    c_keys.resize(n_curves);
    if (d_point_colorbar_active) // E11: drop the colour bar with its scatter curve
        syncPointColorBar();

    {
        QMap<int, QString> shiftedLabels;
        for (auto lit = d_last_auto_curve_labels.constBegin();
             lit != d_last_auto_curve_labels.constEnd(); ++lit) {
            if (lit.key() < index)
                shiftedLabels.insert(lit.key(), lit.value());
            else if (lit.key() > index)
                shiftedLabels.insert(lit.key() - 1, lit.value());
        }
        d_last_auto_curve_labels = shiftedLabels;
    }

    if (d_selection.kind == GraphSelection::Curve
        || d_selection.kind == GraphSelection::ErrorBars
        || d_selection.kind == GraphSelection::Spectrogram) {
        if (d_selection.primaryId == index)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
        else if (d_selection.primaryId > index)
            d_selection.primaryId--;
    }

    syncCurveZOrder();
    emit modifiedGraph();
}

void Graph::removeLegendItem(int index)
{
    if (legendMarkerID < 0 || c_type[index] == ErrorBars)
        return;

    Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
    if (!mrk)
        return;

    if (isPiePlot()) {
        mrk->setText({});
        return;
    }

    QString text = mrk->text();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList items = text.split("\n", Qt::SkipEmptyParts);
#else
    QStringList items = text.split("\n", QString::SkipEmptyParts);
#endif

    if (index >= (int)items.count())
        return;

    QStringList l = items.filter("\\c{" + QString::number(index + 1) + "}");
    if (!l.isEmpty())
        items.removeAll(l[0]); // remove the corresponding legend string
    text = items.join("\n") + "\n";

    QRegExp itemCmd("\\\\c\\{(\\d+)\\}");
    int pos = 0;
    while ((pos = itemCmd.indexIn(text, pos)) != -1) {
        int nr = itemCmd.cap(1).toInt();
        if (nr > index) {
            QString subst = QString("\\c{") + QString::number(nr - 1) + "}";
            text.replace(pos, itemCmd.matchedLength(), subst);
            pos += subst.length();
        } else
            pos += itemCmd.matchedLength();
    }

    mrk->setText(text);
}

void Graph::addLegendItem(const QString &colName)
{
    if (legendMarkerID >= 0) {
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString label = colName;
            if (n_curves > 0) {
                DataCurve *dc = dynamic_cast<DataCurve *>(curve(n_curves - 1));
                if (dc && dc->table()) {
                    const QString prefix = QString(dc->table()->name()) + "_";
                    QString strippedArg = colName;
                    if (strippedArg.startsWith(prefix) && strippedArg.length() > prefix.length())
                        strippedArg = strippedArg.mid(prefix.length());
                    QString strippedYCol = dc->yColumnName();
                    if (strippedYCol.startsWith(prefix) && strippedYCol.length() > prefix.length())
                        strippedYCol = strippedYCol.mid(prefix.length());

                    if (colName == dc->yColumnName() || strippedArg == strippedYCol
                        || colName == dc->title().text()) {
                        const QString richLabel = resolvedAutoCurveLabel(n_curves - 1);
                        if (!richLabel.isEmpty())
                            label = richLabel;
                        else
                            label = strippedYCol;
                    } else if (label.startsWith(prefix) && label.length() > prefix.length()) {
                        label = label.mid(prefix.length());
                    }
                    d_last_auto_curve_labels[n_curves - 1] = label;
                }
            }
            QString text = mrk->text();
            if (text.isEmpty() || text.endsWith("\n", Qt::CaseSensitive))
                text.append("\\c{" + QString::number(curves()) + "}" + label + "\n");
            else
                text.append("\n\\c{" + QString::number(curves()) + "}" + label + "\n");

            if (n_curves >= 2) {
                const bool hadTrailingNewline = text.endsWith(QLatin1Char('\n'));
                QString core = hadTrailingNewline ? text.chopped(1) : text;
                QStringList lines = core.split(QLatin1Char('\n'));
                static const QRegularExpression lineRx(QStringLiteral("^\\\\c\\{(\\d+)\\}(.*)$"));
                bool anyUpdated = false;
                for (int li = 0; li < lines.size(); ++li) {
                    const QRegularExpressionMatch m = lineRx.match(lines[li]);
                    if (!m.hasMatch())
                        continue;
                    const int ci = m.captured(1).toInt() - 1;
                    if (ci < 0 || ci >= n_curves - 1)
                        continue;
                    const QString prevAuto = d_last_auto_curve_labels.value(ci);
                    const QString curBody = m.captured(2);
                    if (!prevAuto.isEmpty() && curBody == prevAuto) {
                        const QString updated = resolvedAutoCurveLabel(ci);
                        if (!updated.isEmpty() && updated != curBody) {
                            lines[li] = QStringLiteral("\\c{%1}%2").arg(ci + 1).arg(updated);
                            d_last_auto_curve_labels[ci] = updated;
                            anyUpdated = true;
                        }
                    }
                }
                if (anyUpdated) {
                    text = lines.join(QLatin1Char('\n'));
                    if (hadTrailingNewline)
                        text.append(QLatin1Char('\n'));
                }
            }

            mrk->setText(text);
        }
    }
}

void Graph::reconstructLegend()
{
    Legend *mrk = legend();
    if (!mrk) {
        for (int i = 0; i < d_texts.size(); ++i) {
            Legend *cand = dynamic_cast<Legend *>(d_plot->marker(d_texts[i]));
            if (cand
                && (cand->text().contains("\\c{") || cand->text().contains("\\p{")
                    || cand->text().contains("%("))) {
                legendMarkerID = d_texts[i];
                mrk = cand;
                break;
            }
        }
    }

    if (!mrk) {
        newLegend();
        return;
    }

    if (isPiePlot())
        mrk->setText(pieLegendText());
    else
        mrk->setText(legendText());

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::contextMenuEvent(QContextMenuEvent *e)
{
    if (selectedMarker < 0
        && (d_selection.kind == GraphSelection::LegendBox
            || d_selection.kind == GraphSelection::TextAnnotation
            || d_selection.kind == GraphSelection::ArrowAnnotation
            || d_selection.kind == GraphSelection::ImageAnnotation)
        && d_selection.primaryId >= 0 && d_plot && d_plot->marker(d_selection.primaryId)) {
        selectedMarker = d_selection.primaryId;
    }
    if (selectedMarker >= 0) {
        emit showMarkerPopupMenu();
        e->accept();
        return;
    }

    QPoint pos = d_plot->canvas()->mapFrom(d_plot, e->pos());
    int dist, point;
    long curve = d_plot->closestCurve(pos.x(), pos.y(), dist, point);
    QwtPlotCurve *c = d_plot->curve(curve);
    if (QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(c)) {
        if (DataCurve *mc = er->masterCurve()) {
            const int mcIdx = curveIndex(mc);
            if (mcIdx >= 0) {
                curve = curveKey(mcIdx);
                c = mc;
            }
        }
    }

    const double dpr = qMax(1.0, double(d_plot ? d_plot->devicePixelRatioF() : 1.0));
    const int hitTolerance = qMax(10, qRound(10.0 * dpr));
    if (c && dist < hitTolerance)
        emit showCurveContextMenu(curve);
    else
        emit showContextMenu();

    e->accept();
}

void Graph::closeEvent(QCloseEvent *e)
{
    emit closedGraph();
    e->accept();
}

bool Graph::zoomOn()
{
    return (d_zoomer[0]->isEnabled() || d_zoomer[1]->isEnabled());
}

void Graph::zoomed(const QwtDoubleRect &)
{
    syncChildLinkedLayers();
    emit modifiedGraph();
}

void Graph::zoom(bool on)
{
    d_zoomer[0]->setEnabled(on);
    d_zoomer[1]->setEnabled(on);
    for (int i = 0; i < n_curves; i++) {
        Spectrogram *sp = (Spectrogram *)this->curve(i);
        if (sp && sp->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            if (sp->colorScaleAxis() == QwtPlot::xBottom || sp->colorScaleAxis() == QwtPlot::yLeft)
                d_zoomer[0]->setEnabled(false);
            else
                d_zoomer[1]->setEnabled(false);
        }
    }

    QCursor cursor = QCursor(QPixmap(":/lens.xpm"), -1, -1);
    if (on)
        d_plot->canvas()->setCursor(cursor);
    else
        d_plot->canvas()->setCursor(Qt::ArrowCursor);
}

void Graph::zoomOut()
{
    if (stepZoomHistory(-1))
        return;

    d_zoomer[0]->zoom(-1);
    d_zoomer[1]->zoom(-1);

    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);
    syncChildLinkedLayers();
}

void Graph::drawText(bool on)
{
    QCursor c = QCursor(Qt::IBeamCursor);
    if (on)
        d_plot->canvas()->setCursor(c);
    else
        d_plot->canvas()->setCursor(Qt::ArrowCursor);

    drawTextOn = on;
}

ImageMarker *Graph::addImage(ImageMarker *mrk)
{
    if (!mrk)
        return 0;

    ImageMarker *mrk2 = new ImageMarker(mrk->fileName());

    int imagesOnPlot = d_images.size();
    d_images.resize(++imagesOnPlot);
    d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk2);

    mrk2->setBoundingRect(mrk->xValue(), mrk->yValue(), mrk->right(), mrk->bottom());
    return mrk;
}

ImageMarker *Graph::addImage(const QString &fileName)
{
    if (fileName.isEmpty() || !QFile::exists(fileName)) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(
                    0, tr("File open error"),
                    tr("Image file: <p><b> %1 </b><p>does not exist anymore!").arg(fileName));
        }
        return 0;
    }

    ImageMarker *mrk = new ImageMarker(fileName);
    int imagesOnPlot = d_images.size();
    d_images.resize(++imagesOnPlot);
    d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk);

    QSize picSize = mrk->pixmap().size();
    int w = d_plot->canvas()->width();
    if (picSize.width() > w)
        picSize.setWidth(w);

    int h = d_plot->canvas()->height();
    if (picSize.height() > h)
        picSize.setHeight(h);

    mrk->setSize(picSize);
    d_plot->replot();

    emit modifiedGraph();
    return mrk;
}

void Graph::insertImageMarker(const QStringList &lst, int fileVersion)
{
    QString fn = lst[1];
    if (!QFile::exists(fn)) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(0, tr("File open error"),
                                 tr("Image file: <p><b> %1 </b><p>does not exist anymore!").arg(fn));
        }
    } else {
        ImageMarker *mrk = new ImageMarker(fn);
        if (!mrk)
            return;

        int imagesOnPlot = d_images.size();
        d_images.resize(++imagesOnPlot);
        d_images[imagesOnPlot - 1] = d_plot->insertMarker(mrk);

        if (fileVersion < 86) {
            mrk->setOrigin(QPoint(lst[2].toInt(), lst[3].toInt()));
            mrk->setSize(QSize(lst[4].toInt(), lst[5].toInt()));
        } else if (fileVersion < 90) {
            double left = lst[2].toDouble();
            double right = left + lst[4].toDouble();
            double top = lst[3].toDouble();
            double bottom = top - lst[5].toDouble();
            mrk->setBoundingRect(left, top, right, bottom);
        } else
            mrk->setBoundingRect(lst[2].toDouble(), lst[3].toDouble(), lst[4].toDouble(),
                                 lst[5].toDouble());
    }
}

void Graph::drawLine(bool on, bool arrow)
{
    drawLineOn = on;
    drawArrowOn = arrow;
    if (!on)
        emit drawLineEnded(true);
}

bool Graph::modifyFunctionCurve(ApplicationWindow *parent, int curve, int type,
                                const QStringList &formulas, const QString &var,
                                QList<double> &ranges, int points)
{
    FunctionCurve *c = (FunctionCurve *)this->curve(curve);
    if (!c)
        return false;

    QStringList effFormulas = formulas;
    if (type == FunctionCurve::Polar && effFormulas.size() == 1)
        effFormulas << (var.trimmed().isEmpty() ? QStringLiteral("t") : var.trimmed());

    if (c->functionType() == type && c->variable() == var && c->formulas() == effFormulas
        && c->startRange() == ranges[0] && c->endRange() == ranges[1] && c->dataSize() == points)
        return true;

    FunctionCurve backup(parent);
    backup.copy(c);

    c->setFunctionType((FunctionCurve::FunctionType)type);
    c->setRange(ranges[0], ranges[1]);
    c->setFormulas(effFormulas);
    c->setVariable(var);
    if (!c->loadData(points)) {
        c->copy(&backup);
        c->loadData(points);
        return false;
    }

    if (type == FunctionCurve::Polar) {
        double maxR = 0.0;
        for (int k = 0; k < c->dataSize(); ++k) {
            const double rk = std::hypot(c->x(k), c->y(k));
            if (std::isfinite(rk) && rk > maxR)
                maxR = rk;
        }
        setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        if (maxR > 0.0) {
            d_polar_rmax = qMax(d_polar_rmax, maxR);
            const double rBound = 1.32 * d_polar_rmax;
            setScale(QwtPlot::xBottom, -rBound, rBound, 0.0, 5, 0, 0, false);
            setScale(QwtPlot::yLeft, -rBound, rBound, 0.0, 5, 0, 0, false);
        }
    }

    if (legendMarkerID >= 0) { // update the legend marker
        Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
        if (mrk) {
            QString text = (mrk->text()).replace(backup.legend(), c->legend());
            mrk->setText(text);
        }
    }
    updatePlot();
    emit modifiedGraph();
    return true;
}

QString Graph::generateFunctionName(const QString &name)
{
    int index = 1;
    QString newName = name + QString::number(index);

    QStringList lst;
    for (int i = 0; i < n_curves; i++) {
        PlotCurve *c = (PlotCurve *)this->curve(i);
        if (!c)
            continue;

        if (c->type() == Function)
            lst << c->title().text();
    }

    while (lst.contains(newName)) {
        newName = name + QString::number(++index);
    }
    return newName;
}

bool Graph::addFunctionCurve(ApplicationWindow *parent, int type, const QStringList &formulas,
                             const QString &var, QList<double> &ranges, int points,
                             const QString &title)
{
    QString name;
    if (!title.isEmpty())
        name = title;
    else
        name = generateFunctionName();

    QStringList effFormulas = formulas;
    const QString effVar = var.trimmed().isEmpty() ? QStringLiteral("t") : var.trimmed();
    if (type == FunctionCurve::Polar && effFormulas.size() == 1)
        effFormulas << effVar;

    FunctionCurve *c = new FunctionCurve(parent, (FunctionCurve::FunctionType)type, name);
    QPen fPen(QColor(0, 114, 178), qMax(2, widthLine));
    fPen.setCosmetic(false);
    c->setPen(fPen);
    c->setRange(ranges[0], ranges[1]);
    c->setFormulas(effFormulas);
    c->setVariable(effVar);
    if (!c->loadData(points)) {
        delete c;
        return false;
    }

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = Function;

    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);

    if (type == FunctionCurve::Polar) {
        double maxR = 0.0;
        for (int k = 0; k < c->dataSize(); ++k) {
            const double rk = std::hypot(c->x(k), c->y(k));
            if (std::isfinite(rk) && rk > maxR)
                maxR = rk;
        }
        setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        if (maxR > 0.0) {
            double rLo = 0.0;
            double rHi = maxR;
            double rStep = 0.0;
            QwtLinearScaleEngine rse;
            rse.autoScale(4, rLo, rHi, rStep);
            d_polar_rmax = qMax(d_polar_rmax, (rHi >= maxR && std::isfinite(rHi)) ? rHi : maxR);
            const int extraRows = qMax(0, qMax(int(d_polar_series.size()), n_curves) - 1);
            const double yLo = -1.44 * d_polar_rmax;
            const double yHi = (1.64 + 0.36 * double(extraRows)) * d_polar_rmax;
            const double halfX = 0.5 * (yHi - yLo);
            setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
            setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
        }
    }

    const QString legLabel = (type == FunctionCurve::Polar && !formulas.isEmpty())
            ? QStringLiteral("r(%1) = %2").arg(effVar, formulas[0].trimmed())
            : c->legend();
    addLegendItem(legLabel);
    if (type == FunctionCurve::Polar && hasLegend())
        placeLegendAnchor(LegendTopRight);
    if (parent) {
        auto ml = qobject_cast<MultiLayer *>(parent->d_workspace.activeSubWindow());
        if (!ml || ml->status() != MyWidget::Minimized) {
            updatePlot();
            emit modifiedGraph();
        }
    } else {
        updatePlot();
        emit modifiedGraph();
    }
    return true;
}

bool Graph::insertFunctionCurve(ApplicationWindow *parent, const QStringList &func_spec, int points,
                                int fileVersion)
{
    int type = FunctionCurve::Normal;
    QStringList formulas;
    QString var, name;
    QList<double> ranges;

    QStringList curve = func_spec[0].split(",");
    if (fileVersion >= 0x000105) {
        // SciDAVis 0.1.4 and 0.2.0 crash when trying to save a function curve;
        // thus, it's safe to assume every version from 0.1.5 on uses the new format,
        // even though 0.2.0 doesn't actually contain the revised code yet
        if (curve.size() < 5)
            return false;
        type = curve[0].toInt();
        name = curve.mid(1, curve.size() - 4).join(",");
        var = curve[curve.size() - 3];
        ranges += curve[curve.size() - 2].toDouble();
        ranges += curve[curve.size() - 1].toDouble();

        if (func_spec.size() > 1)
            formulas << func_spec[1];
        else
            formulas << QString();
        if (type != FunctionCurve::Normal)
            formulas << (func_spec.size() > 2 ? func_spec[2] : QString());

        FunctionCurve *c = new FunctionCurve(parent, (FunctionCurve::FunctionType)type, name);
        c->setSuppressScriptErrors(true);
        c->setPen(QPen(QColor(Qt::black), widthLine));
        c->setRange(ranges[0], ranges[1]);
        c->setFormulas(formulas);
        c->setVariable(var);
        if (!c->loadData(points)) {
            delete c;
            return false;
        }
        c->setSuppressScriptErrors(false);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Line;

        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);

        addLegendItem(c->legend());
        auto ml = parent ? qobject_cast<MultiLayer *>(parent->d_workspace.activeSubWindow()) : nullptr;
        if (ml && ml->status() != MyWidget::Minimized) {
            updatePlot();
            emit modifiedGraph();
        }
        return true;

    } else if (fileVersion < 87) {
        if (curve[0][0] == 'f') {
            type = FunctionCurve::Normal;
            formulas += curve[0].section('=', 1, 1);
            var = curve[1];
            ranges += curve[2].toDouble();
            ranges += curve[3].toDouble();
        } else if (curve[0][0] == 'X') {
            type = FunctionCurve::Parametric;
            formulas += curve[0].section('=', 1, 1);
            formulas += curve[1].section('=', 1, 1);
            var = curve[2];
            ranges += curve[3].toDouble();
            ranges += curve[4].toDouble();
        } else if (curve[0][0] == 'R') {
            type = FunctionCurve::Polar;
            formulas += curve[0].section('=', 1, 1);
            formulas += curve[1].section('=', 1, 1);
            var = curve[2];
            ranges += curve[3].toDouble();
            ranges += curve[4].toDouble();
        }
    } else {
        type = curve[0].toInt();
        name = curve[1];

        if (type == FunctionCurve::Normal) {
            formulas << curve[2];
            var = curve[3];
            ranges += curve[4].toDouble();
            ranges += curve[5].toDouble();
        } else if (type == FunctionCurve::Polar || type == FunctionCurve::Parametric) {
            formulas << curve[2];
            formulas << curve[3];
            var = curve[4];
            ranges += curve[5].toDouble();
            ranges += curve[6].toDouble();
        }
    }
    return addFunctionCurve(parent, type, formulas, var, ranges, points, name);
}

void Graph::createTable(const QString &curveName)
{
    if (curveName.isEmpty())
        return;

    const QwtPlotCurve *cv = curve(curveName);
    if (!cv)
        return;

    createTable(cv);
}

void Graph::createTable(const QwtPlotCurve *curve)
{
    if (!curve)
        return;

    int size = curve->dataSize();

    Column *xCol = new Column(tr("1", "curve data table x column name"), SciDAVis::ColumnMode::Numeric);
    Column *yCol = new Column(tr("2", "curve data table y column name"), SciDAVis::ColumnMode::Numeric);
    xCol->setPlotDesignation(SciDAVis::X);
    yCol->setPlotDesignation(SciDAVis::Y);
    for (int i = 0; i < size; i++) {
        xCol->setValueAt(i, curve->x(i));
        yCol->setValueAt(i, curve->y(i));
    }
    QString legend = tr("Data set generated from curve") + ": " + curve->title().text();
    emit createTable(tr("Curve data %1").arg(1), legend, QList<Column *>() << xCol << yCol);
}

QString Graph::saveToString(bool saveAsTemplate)
{
    QString s = "<graph>\n";
    s += "ggeometry\t";
    s += QString::number(this->pos().x()) + "\t";
    s += QString::number(this->pos().y()) + "\t";
    s += QString::number(this->frameGeometry().width()) + "\t";
    s += QString::number(this->frameGeometry().height()) + "\n";
    s += saveTitle();
    s += "<Antialiasing>" + QString::number(d_antialiasing) + "</Antialiasing>\n";
    s += "Background\t" + COLORNAME(d_plot->paletteBackgroundColor()) + "\t";
    s += QString::number(d_plot->paletteBackgroundColor().alpha()) + "\n";
    s += "Margin\t" + QString::number(d_plot->margin()) + "\n";
    s += "Border\t" + QString::number(d_plot->lineWidth()) + "\t" + COLORNAME(d_plot->frameColor())
            + "\n";
    s += grid()->saveToString();
    s += saveEnabledAxes();
    s += "AxesTitles\t" + saveScaleTitles();
    s += saveAxesTitleColors();
    s += saveAxesTitleAlignement();
    s += saveFonts();
    s += saveEnabledTickLabels();
    s += saveAxesColors();
    s += saveAxesBaseline();
    s += saveCanvas();

    if (!saveAsTemplate)
        s += saveCurves();

    s += saveScale();
    s += saveAxesFormulas();
    s += saveLabelsFormat();
    s += saveAxesLabelsType();
    s += saveTicksType();
    s += "TicksLength\t" + QString::number(minorTickLength()) + "\t"
            + QString::number(majorTickLength()) + "\n";
    s += "DrawAxesBackbone\t" + QString::number(drawAxesBackbone) + "\n";
    s += "AxesLineWidth\t" + QString::number(d_plot->axesLinewidth()) + "\n";
    s += saveLabelsRotation();
    s += saveMarkers();

    // Phase 5 lossless persistence tags
    {
        auto escAttr = [](const QString &raw) -> QString {
            QString out;
            out.reserve(raw.size() + 8);
            for (QChar ch : raw) {
                const ushort u = ch.unicode();
                if (ch == '&')
                    out += "&amp;";
                else if (ch == '"')
                    out += "&quot;";
                else if (ch == '<')
                    out += "&lt;";
                else if (ch == '>')
                    out += "&gt;";
                else if (ch == '\n')
                    out += "&#10;";
                else if (ch == '\r')
                    out += "&#13;";
                else if (ch == '\t')
                    out += "&#9;";
                else if (u < 32)
                    out += QString("&#%1;").arg(int(u));
                else
                    out += ch;
            }
            static const char *const kLegacyTokens[] = {
                "ggeometry",         "EnabledAxes",      "AxesBaseline",     "EnabledTicks",
                "MajorTicks",        "MinorTicks",       "TicksLength",      "EnabledTickLabels",
                "AxesColors",        "AxesNumberColors", "PieCurve",         "FunctionCurve",
                "ErrorBars",         "PlotTitle",        "TitleFont",        "AxesTitles",
                "AxesTitleColors",   "AxesTitleAlignment","ScaleFont",       "AxisFont",
                "AxesFormulas",      "LabelsFormat",     "LabelsRotation",   "DrawAxesBackbone",
                "AxesLineWidth",     "CanvasFrame",      "CanvasBackground", "Legend",
                "textMarker",        "lineMarker",       "ImageMarker",      "AxisType",
                "AxesTickLabelsCol"
            };
            for (const char *tok : kLegacyTokens) {
                const QString qtok = QString::fromLatin1(tok);
                if (out.contains(qtok)) {
                    const QString repl =
                            QString("&#%1;").arg(int(qtok.at(0).unicode())) + qtok.mid(1);
                    out.replace(qtok, repl);
                }
            }
            return out;
        };

        int legCols = 1;
        if (Legend *leg = legend())
            legCols = leg->numColumns();

        QStringList textColsParts;
        QStringList textPtParts; // exact pt of legend / text fonts that are not whole numbers
        for (int ti = 0; ti < d_texts.size(); ++ti) {
            Legend *tm = dynamic_cast<Legend *>(d_plot->marker(d_texts[ti]));
            if (tm && tm->numColumns() > 1)
                textColsParts << QString("%1:%2").arg(ti).arg(tm->numColumns());
            if (tm) {
                const double tpt = tm->font().pointSizeF();
                if (tpt > 0.0 && std::fabs(tpt - std::round(tpt)) > 1e-9)
                    textPtParts << QString("%1:%2").arg(ti).arg(QString::number(tpt, 'g', 6));
            }
        }

        QStringList alphaParts;
        QStringList widthParts;
        QStringList stackParts;
        for (int ci = 0; ci < n_curves; ++ci) {
            QwtPlotCurve *cv = curve(ci);
            if (!cv)
                continue;
            const double lw = cv->pen().widthF();
            const double sw = cv->symbol().pen().widthF();
            if (lw > 0.0 || sw > 0.0) {
                const QString titleB64 =
                        QString::fromLatin1(cv->title().text().toUtf8().toBase64());
                widthParts << QString("%1:%2,%3,%4")
                                      .arg(ci)
                                      .arg(QString::number(lw, 'g', 8))
                                      .arg(QString::number(sw, 'g', 8))
                                      .arg(titleB64);
            }
            const int la = cv->pen().color().alpha();
            const int ba = cv->brush().color().alpha();
            const int spa = cv->symbol().pen().color().alpha();
            const int sba = cv->symbol().brush().color().alpha();
            if (la < 255 || ba < 255 || spa < 255 || sba < 255) {
                alphaParts << QString("%1:%2,%3,%4,%5")
                                      .arg(ci)
                                      .arg(la)
                                      .arg(ba)
                                      .arg(spa)
                                      .arg(sba);
            }
            if (!saveAsTemplate) {
                PlotCurve *pc = dynamic_cast<PlotCurve *>(cv);
                if (pc
                    && (qAbs(pc->stackOffsetX()) > 1e-15 || qAbs(pc->stackOffsetY()) > 1e-15
                        || pc->stackNormalize01())) {
                    stackParts << QString("%1:%2,%3,%4")
                                          .arg(ci)
                                          .arg(QString::number(pc->stackOffsetX(), 'g', 15))
                                          .arg(QString::number(pc->stackOffsetY(), 'g', 15))
                                          .arg(pc->stackNormalize01() ? 1 : 0);
                }
            }
        }

        QString legAnchorAttr =
                (d_plot && d_plot->property("_toriLegendAnchor").isValid())
                ? QString(" legendAnchor=\"%1\"").arg(d_plot->property("_toriLegendAnchor").toInt())
                : QString();
        // <legend>/<text> lines keep the integer pt for older readers; the exact value rides here.
        if (!textPtParts.isEmpty())
            legAnchorAttr += QString(" textPts=\"%1\"").arg(textPtParts.join(";"));
        // <curve> lines keep integer line/symbol widths for older readers; exact fractional px ride here.
        if (!widthParts.isEmpty())
            legAnchorAttr += QString(" curveWidths=\"%1\"").arg(widthParts.join(";"));

        s += QString("<ToriBackbones draw=\"%1\" b0=\"%2\" b1=\"%3\" b2=\"%4\" b3=\"%5\" "
                     "legendCols=\"%6\" legendVisible=\"%7\" outsideRightMargin=\"%8\" "
                     "textCols=\"%9\" curveAlphas=\"%10\" stackOffsets=\"%11\"%12/>\n")
                     .arg(drawAxesBackbone ? 1 : 0)
                     .arg(d_axis_backbone[0] ? 1 : 0)
                     .arg(d_axis_backbone[1] ? 1 : 0)
                     .arg(d_axis_backbone[2] ? 1 : 0)
                     .arg(d_axis_backbone[3] ? 1 : 0)
                     .arg(legCols)
                     .arg(d_legend_visible ? 1 : 0)
                     .arg(d_plot ? d_plot->property("_outsideRightMargin").toInt() : 0)
                     .arg(textColsParts.join(";"))
                     .arg(alphaParts.join(";"))
                     .arg(stackParts.join(";"))
                     .arg(legAnchorAttr);

        if (d_plot) {
            QStringList shiftAttrs;
            const QPointF ptShift = d_plot->property("_toriPlotTitleShift").toPointF();
            if (std::fabs(ptShift.x()) > 1e-6 || std::fabs(ptShift.y()) > 1e-6) {
                shiftAttrs << QString("titleDx=\"%1\"").arg(QString::number(ptShift.x(), 'g', 17));
                shiftAttrs << QString("titleDy=\"%1\"").arg(QString::number(ptShift.y(), 'g', 17));
            }
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (const QwtScaleWidget *sw = d_plot->axisWidget(a)) {
                    const QPointF atShift = sw->property("_toriAxisTitleShift").toPointF();
                    if (std::fabs(atShift.x()) > 1e-6 || std::fabs(atShift.y()) > 1e-6) {
                        shiftAttrs << QString("ax%1TitleDx=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(atShift.x(), 'g', 17));
                        shiftAttrs << QString("ax%1TitleDy=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(atShift.y(), 'g', 17));
                    }
                    const double normShift = sw->property("_toriAxisNormalShift").toDouble();
                    if (std::fabs(normShift) > 1e-6) {
                        shiftAttrs << QString("ax%1Norm=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(normShift, 'g', 17));
                    }
                    const double parShift = sw->property("_toriAxisParallelShift").toDouble();
                    if (std::fabs(parShift) > 1e-6) {
                        shiftAttrs << QString("ax%1Par=\"%2\"")
                                              .arg(a)
                                              .arg(QString::number(parShift, 'g', 17));
                    }
                }
            }
            if (!shiftAttrs.isEmpty())
                s += QString("<ToriLabelShifts %1/>\n").arg(shiftAttrs.join(' '));
        }

        for (int ci = 0; ci < n_curves; ++ci) {
            if (const PlotCurve *pc = dynamic_cast<const PlotCurve *>(curve(ci))) {
                if (pc->symbolSkip() > 0) {
                    s += QString("<ToriCurveAlpha idx=\"%1\" symSkip=\"%2\"/>\n")
                                 .arg(ci)
                                 .arg(pc->symbolSkip());
                }
            }
        }

        int lockMask = 0;
        int autoMask = 0;
        for (int a = 0; a < 4; ++a) {
            if (isAxisUserLocked(a))
                lockMask |= (1 << a);
            if (d_plot && d_plot->axisAutoScale(a))
                autoMask |= (1 << a);
        }
        s += QString("<ToriAxisLocks mask=\"%1\" autoscale=\"%2\" autoMask=\"%3\"/>\n")
                     .arg(lockMask)
                     .arg(m_autoscale ? 1 : 0)
                     .arg(autoMask);

        s += QString("<ToriAutoscaleFonts val=\"%1\"/>\n").arg(autoScaleFonts ? 1 : 0);

        const QString unitFmtStr = (d_axis_unit_format == Column::UnitFormat::ISIJSlash)
                ? QStringLiteral("ISIJSlash")
                : ((d_axis_unit_format == kUnitFormatBracket) ? QStringLiteral("Bracket")
                                                              : QStringLiteral("Paren"));
        s += QString("<ToriAutoAxisTitles unitFormat=\"%1\" a0=\"%2\" a1=\"%3\" a2=\"%4\" a3=\"%5\"/>\n")
                     .arg(unitFmtStr)
                     .arg(d_axis_title_auto[0] ? 1 : 0)
                     .arg(d_axis_title_auto[1] ? 1 : 0)
                     .arg(d_axis_title_auto[2] ? 1 : 0)
                     .arg(d_axis_title_auto[3] ? 1 : 0);

        for (const ReferenceLineSpec &rl : d_ref_lines) {
            s += QString("<ToriRefLine orient=\"%1\" value=\"%2\" stat=\"%3\" color=\"%4\" "
                         "style=\"%5\" width=\"%6\" label=\"%7\" fill=\"%8\" fillColor=\"%9\"")
                         .arg(rl.orientation)
                         .arg(QString::number(rl.value, 'g', 15))
                         .arg(escAttr(rl.statMode))
                         .arg(rl.color.name(QColor::HexArgb))
                         .arg(int(rl.style))
                         .arg(QString::number(rl.width, 'g', 8))
                         .arg(escAttr(rl.label))
                         .arg(rl.fillToAxis ? 1 : 0)
                         .arg(rl.fillColor.name(QColor::HexArgb))
                 + QStringLiteral(" visible=\"%1\"/>\n").arg(rl.visible ? 1 : 0);
        }

        if (!saveAsTemplate) {
            for (const ShadedBandSpec &bs : d_shaded_bands) {
                QStringList xStr, ylStr, yhStr;
                const int m = qMin(bs.x.size(), qMin(bs.yLow.size(), bs.yHigh.size()));
                xStr.reserve(m);
                ylStr.reserve(m);
                yhStr.reserve(m);
                for (int k = 0; k < m; ++k) {
                    xStr << QString::number(bs.x[k], 'g', 12);
                    ylStr << QString::number(bs.yLow[k], 'g', 12);
                    yhStr << QString::number(bs.yHigh[k], 'g', 12);
                }
                s += QString("<ToriRefLine kind=\"band\" color=\"%1\" title=\"%2\" x=\"%3\" "
                             "yLow=\"%4\" yHigh=\"%5\"/>\n")
                             .arg(bs.fillColor.name(QColor::HexArgb))
                             .arg(escAttr(bs.title))
                             .arg(xStr.join(","))
                             .arg(ylStr.join(","))
                             .arg(yhStr.join(","));
            }

            // E11: error bands and colour-mapped scatter symbols (restored after the curves)
            // (attributes are concatenated, not chained through arg(), so a '%' in a column
            // name or title can never be re-substituted)
            auto attr = [&escAttr](const char *key, const QString &val) -> QString {
                return QStringLiteral(" ") + QLatin1String(key) + QStringLiteral("=\"")
                        + escAttr(val) + QStringLiteral("\"");
            };
            for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
                QwtPlotItem *pit = d_plot->plotItem(c_keys[i]);
                if (c_type[i] == ErrorBars) {
                    auto *er = dynamic_cast<QwtErrorPlotCurve *>(pit);
                    if (er && er->isBand() && er->masterCurve()) {
                        s += QStringLiteral("<ToriErrorBand")
                                + attr("x", er->masterCurve()->xColumnName())
                                + attr("y", er->masterCurve()->title().text())
                                + attr("err", er->title().text())
                                + attr("plus", QString::number(er->plusSide() ? 1 : 0))
                                + attr("minus", QString::number(er->minusSide() ? 1 : 0))
                                + attr("alpha", QString::number(er->bandAlpha()))
                                + QStringLiteral("/>\n");
                    }
                    continue;
                }
                auto *dc = dynamic_cast<DataCurve *>(pit);
                if (!dc || !dc->hasPointColorMap())
                    continue;
                const DataCurve::PointColorMap &pm = dc->pointColorMap();
                s += QStringLiteral("<ToriPointColorMap") + attr("x", dc->xColumnName())
                        + attr("y", dc->title().text()) + attr("z", pm.zColumn)
                        + attr("size", pm.sizeColumn) + attr("preset", QString::number(pm.preset))
                        + attr("reverse", QString::number(pm.reverse ? 1 : 0))
                        + attr("auto", QString::number(pm.autoRange ? 1 : 0))
                        + attr("zmin", QString::number(pm.zMin, 'g', 15))
                        + attr("zmax", QString::number(pm.zMax, 'g', 15))
                        + attr("smin", QString::number(pm.sizeMinPx, 'g', 8))
                        + attr("smax", QString::number(pm.sizeMaxPx, 'g', 8))
                        + attr("cb", QString::number(pm.showColorBar ? 1 : 0))
                        + attr("title", pm.colorBarTitle) + QStringLiteral("/>\n");
            }
        }

        if (d_linked_parent) {
            int parentIdx = -1;
            if (MultiLayer *ml = multiLayerParent())
                parentIdx = ml->layersList().indexOf(d_linked_parent.data());
            if (parentIdx >= 0) {
                s += QString("<ToriLinkedLayer parentIdx=\"%1\" xMode=\"%2\" xExpr=\"%3\" "
                             "yMode=\"%4\" yExpr=\"%5\"/>\n")
                             .arg(parentIdx)
                             .arg(d_x_link_mode)
                             .arg(escAttr(d_x_link_expr))
                             .arg(d_y_link_mode)
                             .arg(escAttr(d_y_link_expr));
            }
        }

        for (int a = 0; a < 4; ++a) {
            const bool brkSuspended =
                    !d_axis_breaks[a].enabled && d_suspended_axis_breaks[a].enabled;
            const AxisBreakSpec &ab =
                    brkSuspended ? d_suspended_axis_breaks[a] : d_axis_breaks[a];
            if (ab.enabled) {
                s += QString("<ToriAxisBreak axis=\"%1\" enabled=\"1\" from=\"%2\" to=\"%3\" "
                             "pos=\"%4\" stepBefore=\"%5\" stepAfter=\"%6\" logBefore=\"%7\" "
                             "logAfter=\"%8\"%9/>\n")
                             .arg(a)
                             .arg(QString::number(ab.breakFrom, 'g', 15))
                             .arg(QString::number(ab.breakTo, 'g', 15))
                             .arg(QString::number(ab.positionPct, 'g', 8))
                             .arg(QString::number(ab.stepBefore, 'g', 15))
                             .arg(QString::number(ab.stepAfter, 'g', 15))
                             .arg(ab.log10Before ? 1 : 0)
                             .arg(ab.log10After ? 1 : 0)
                             .arg(brkSuspended ? QStringLiteral(" suspended=\"1\"") : QString());
            }
            if (qAbs(d_tick_divide_factor[a] - 1.0) > 1e-15 || !d_tick_prefix[a].isEmpty()
                || !d_tick_suffix[a].isEmpty()) {
                s += QString("<ToriTickTransform axis=\"%1\" factor=\"%2\" prefix=\"%3\" "
                             "suffix=\"%4\"/>\n")
                             .arg(a)
                             .arg(QString::number(d_tick_divide_factor[a], 'g', 15))
                             .arg(escAttr(d_tick_prefix[a]))
                             .arg(escAttr(d_tick_suffix[a]));
            }
        }

        if (d_is_ternary) {
            s += QString("<ToriTernary enabled=\"1\" a=\"%1\" b=\"%2\" c=\"%3\"/>\n")
                         .arg(escAttr(d_ternary_labels[0]))
                         .arg(escAttr(d_ternary_labels[1]))
                         .arg(escAttr(d_ternary_labels[2]));
        }

        QStringList stackedBarIndices;
        int stackedGap = 25;
        int stackedOffset = 0;
        bool haveStackedRef = false;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(curve(bi))) {
                if (bc->isStacked()) {
                    stackedBarIndices << QString::number(bi);
                    if (!haveStackedRef) {
                        stackedGap = bc->gap();
                        stackedOffset = bc->offset();
                        haveStackedRef = true;
                    }
                }
            }
        }
        if (!stackedBarIndices.isEmpty()) {
            s += QString("<ToriStackedBars indices=\"%1\" gap=\"%2\" offset=\"%3\"/>\n")
                         .arg(stackedBarIndices.join(","))
                         .arg(stackedGap)
                         .arg(stackedOffset);
        }

        for (int a = 0; a < 4; ++a) {
            if (!d_custom_tick_positions[a].isEmpty()
                && d_custom_tick_positions[a].size() == d_custom_tick_labels[a].size()) {
                QStringList posStrs;
                posStrs.reserve(d_custom_tick_positions[a].size());
                for (double pv : d_custom_tick_positions[a])
                    posStrs << QString::number(pv, 'g', 14);
                s += QString("<ToriCategoryAxis axis=\"%1\" positions=\"%2\" labels=\"%3\" "
                             "rot=\"%4\"/>\n")
                             .arg(a)
                             .arg(posStrs.join(","))
                             .arg(escAttr(d_custom_tick_labels[a].join(QChar(0x1F))))
                             .arg(QString::number(d_custom_tick_rotation[a], 'g', 8));
            } else if (!d_custom_category_labels[a].isEmpty()) {
                s += QString("<ToriCategoryAxis axis=\"%1\" labels=\"%2\"/>\n")
                             .arg(a)
                             .arg(escAttr(d_custom_category_labels[a].join(QChar(0x1F))));
            }
            if (d_weibull_prob_axis[a]) {
                s += QString("<ToriWeibullAxis axis=\"%1\" enabled=\"1\"/>\n").arg(a);
            }
        }

        if (saveAsTemplate) {
            auto appendTemplateStyleTag = [&](int outIdx, const TemplateCurveStyle &st) {
                s += QString("<ToriTemplateCurveStyle idx=\"%1\" type=\"%2\" penStyle=\"%3\" "
                             "lineWidth=\"%4\" color=\"%5\" brushStyle=\"%6\" brushColor=\"%7\" "
                             "symbol=\"%8\" symSize=\"%9\" symWidth=\"%10\" symColor=\"%11\" "
                             "symFill=\"%12\" hollow=\"%13\" symSkip=\"%14\" barGap=\"%15\" "
                             "barOffset=\"%16\" xAxis=\"%17\" yAxis=\"%18\"/>\n")
                             .arg(outIdx)
                             .arg(st.curveType)
                             .arg(st.penStyle)
                             .arg(QString::number(st.lineWidth, 'g', 8))
                             .arg(st.lineColor.name(QColor::HexArgb))
                             .arg(st.brushStyle)
                             .arg(st.brushColor.name(QColor::HexArgb))
                             .arg(st.symbolStyle)
                             .arg(st.symbolSize)
                             .arg(QString::number(st.symbolPenWidth, 'g', 8))
                             .arg(st.symbolEdgeColor.name(QColor::HexArgb))
                             .arg(st.symbolFillColor.name(QColor::HexArgb))
                             .arg(st.symbolHollow ? 1 : 0)
                             .arg(st.symbolSkip)
                             .arg(st.barGap)
                             .arg(st.barOffset)
                             .arg(st.xAxis)
                             .arg(st.yAxis);
            };
            int emitted = 0;
            for (int ci = 0; ci < n_curves; ++ci) {
                const int ct = (ci >= 0 && ci < c_type.size()) ? c_type[ci] : int(LineSymbols);
                if (ct == ErrorBars)
                    continue;
                QwtPlotCurve *cv = curve(ci);
                if (!cv)
                    continue;
                TemplateCurveStyle st;
                st.curveType = ct;
                st.penStyle = int(cv->pen().style());
                st.lineWidth = cv->pen().widthF();
                st.lineColor = cv->pen().color();
                st.brushStyle = int(cv->brush().style());
                st.brushColor = cv->brush().color();
                const QwtSymbol sym = cv->symbol();
                st.symbolStyle = int(sym.style());
                st.symbolSize = sym.size().width();
                st.symbolPenWidth = sym.pen().widthF();
                st.symbolEdgeColor = sym.pen().color();
                st.symbolFillColor = sym.brush().color();
                st.symbolHollow = isSymbolWhiteHollow(sym);
                if (const PlotCurve *pc = dynamic_cast<const PlotCurve *>(cv))
                    st.symbolSkip = pc->symbolSkip();
                if (const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(cv)) {
                    st.barGap = bc->gap();
                    st.barOffset = bc->offset();
                }
                st.xAxis = cv->xAxis();
                st.yAxis = cv->yAxis();
                appendTemplateStyleTag(emitted++, st);
            }
            if (emitted == 0 && !d_template_curve_styles.isEmpty()) {
                for (int si = 0; si < d_template_curve_styles.size(); ++si)
                    appendTemplateStyleTag(si, d_template_curve_styles[si]);
            }
        }

        if (d_is_polar) {
            s += QString("<ToriPolarPlot enabled=\"1\" rmax=\"%1\"/>\n")
                         .arg(QString::number(d_polar_rmax, 'g', 15));
            if (!saveAsTemplate) {
                for (const PolarSeriesSpec &ps : d_polar_series) {
                    const int m = qMin(ps.x.size(), ps.y.size());
                    if (m <= 0)
                        continue;
                    QStringList xs, ys;
                    xs.reserve(m);
                    ys.reserve(m);
                    for (int k = 0; k < m; ++k) {
                        xs << QString::number(ps.x[k], 'g', 12);
                        ys << QString::number(ps.y[k], 'g', 12);
                    }
                    s += QString("<ToriPolarSeries label=\"%1\" color=\"%2\" width=\"%3\" "
                                 "x=\"%4\" y=\"%5\"/>\n")
                                 .arg(escAttr(ps.label))
                                 .arg(ps.color.name(QColor::HexArgb))
                                 .arg(QString::number(ps.width, 'g', 8))
                                 .arg(xs.join(","))
                                 .arg(ys.join(","));
                }
            }
        }
    }

    // E11b: 2D density / pole figure specifications (synced with live Spectrogram state)
    if (!saveAsTemplate) {
        const QStringList extraTags = ExtraPlotTypes::syncedGraphTagsForState(this);
        for (const QString &tag : extraTags)
            s += tag + "\n";
    }

    s += "</graph>\n";
    return s;
}

void Graph::showIntensityTable()
{
    ImageMarker *mrk = (ImageMarker *)d_plot->marker(selectedMarker);
    if (!mrk)
        return;

    emit createIntensityTable(mrk->fileName());
}

Graph::MarkerAxisState Graph::markerAxisState() const
{
    MarkerAxisState st;
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QwtScaleDiv *d = d_plot->axisScaleDiv(a);
        st.valid[a] = d && d->isValid();
        st.map[a] = d_plot->canvasMap(a);
        if (!st.valid[a]) {
            // Qwt's setAxisScale() leaves the scale division invalid until the next replot. The
            // scale widget still draws the previous scale: take range and transformation from
            // its scale draw (the pixel interval of canvasMap() is right either way).
            const QwtScaleWidget *sw = d_plot->axisWidget(a);
            const QwtScaleDraw *sd = sw ? sw->scaleDraw() : nullptr;
            if (sd && sd->map().transformation() && sd->scaleDiv().isValid()
                && sd->map().s1() != sd->map().s2()) {
                st.map[a].setTransformation(sd->map().transformation()->copy());
                st.map[a].setScaleInterval(sd->map().s1(), sd->map().s2());
                st.valid[a] = true;
            }
        }
    }
    return st;
}

namespace {
// True when both maps put every value at the same fraction of their paint interval: same range
// and the same transformation, including axis breaks and formula axes (sampled along the axis).
bool sameAxisMapping(const QwtScaleMap &a, const QwtScaleMap &b)
{
    if (a.s1() != b.s1() || a.s2() != b.s2())
        return false;
    const QwtScaleTransformation *ta = a.transformation();
    const QwtScaleTransformation *tb = b.transformation();
    if (!ta || !tb || ta->type() != tb->type())
        return false;
    const double da = a.p2() - a.p1();
    const double db = b.p2() - b.p1();
    if (da == 0.0 || db == 0.0)
        return da == db;
    constexpr int kSamples = 64;
    for (int k = 0; k <= kSamples; ++k) {
        const double v = a.s1() + (a.s2() - a.s1()) * double(k) / double(kSamples);
        const double fa = (a.xTransform(v) - a.p1()) / da;
        const double fb = (b.xTransform(v) - b.p1()) / db;
        if (std::isnan(fa) && std::isnan(fb))
            continue;
        if (!(std::fabs(fa - fb) <= 1e-9))
            return false;
    }
    return true;
}
} // namespace

void Graph::refreshMarkerPixelCaches()
{
    for (int i = 0; i < (int)d_lines.size(); i++) {
        if (ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]))
            mrkL->updateBoundingRect();
    }
    for (int i = 0; i < (int)d_texts.size(); i++) {
        if (Legend *mrkT = (Legend *)d_plot->marker(d_texts[i])) {
            if (mrkT == legend() && repositionAnchoredOutsideRightLegend())
                continue;
            mrkT->refreshPixelOrigin();
        }
    }
    for (int i = 0; i < (int)d_images.size(); i++) {
        if (ImageMarker *mrk = (ImageMarker *)d_plot->marker(d_images[i]))
            mrk->refreshPixelCache();
    }
}

void Graph::updateMarkersBoundingRect(const MarkerAxisState &before)
{
    if (d_is_ternary) {
        updateMarkersBoundingRect();
        return;
    }
    // Settle the new scales and the layout (tick labels of another width move the canvas edges)
    // before taking the new canvas maps; Plot::updateLayout() also settles a setAxisScale() that
    // was not replotted yet.
    d_plot->updateLayout();
    const MarkerAxisState now = markerAxisState();
    bool same[QwtPlot::axisCnt];
    for (int a = 0; a < QwtPlot::axisCnt; ++a)
        same[a] = before.valid[a] && now.valid[a] && sameAxisMapping(before.map[a], now.map[a]);
    auto unchanged = [&](int xAxis, int yAxis) {
        return xAxis >= 0 && xAxis < QwtPlot::axisCnt && yAxis >= 0 && yAxis < QwtPlot::axisCnt
                && same[xAxis] && same[yAxis];
    };
    // The previous scale of an axis (range and transformation) on the current canvas. Not
    // available while the previous scale division was invalid (Qwt's setAxisScale() leaves it
    // so until the next replot): the marker then keeps its cached pixel position.
    auto previousMap = [&](int axis, QwtScaleMap &m) -> bool {
        if (axis < 0 || axis >= QwtPlot::axisCnt || !before.valid[axis] || !now.valid[axis])
            return false;
        m = before.map[axis];
        m.setPaintXInterval(now.map[axis].p1(), now.map[axis].p2());
        return true;
    };

    // Arrows are drawn between data points: they follow the data.
    for (int i = 0; i < (int)d_lines.size(); i++) {
        if (ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]))
            mrkL->updateBoundingRect();
    }
    for (int i = 0; i < (int)d_texts.size(); i++) {
        Legend *mrkT = (Legend *)d_plot->marker(d_texts[i]);
        if (!mrkT)
            continue;
        if (mrkT == legend() && repositionAnchoredOutsideRightLegend())
            continue;
        if (!unchanged(mrkT->xAxis(), mrkT->yAxis())) {
            QwtScaleMap xm, ym;
            if (previousMap(mrkT->xAxis(), xm) && previousMap(mrkT->yAxis(), ym))
                mrkT->refreshPixelOrigin(&xm, &ym);
            mrkT->updateOrigin();
        }
        mrkT->refreshPixelOrigin();
    }
    for (int i = 0; i < (int)d_images.size(); i++) {
        ImageMarker *mrk = (ImageMarker *)d_plot->marker(d_images[i]);
        if (!mrk)
            continue;
        if (!unchanged(mrk->xAxis(), mrk->yAxis())) {
            QwtScaleMap xm, ym;
            if (previousMap(mrk->xAxis(), xm) && previousMap(mrk->yAxis(), ym)) {
                // As ImageMarker::rect(), with the previous scales.
                const int x0 = xm.transform(mrk->xValue());
                const int y0 = ym.transform(mrk->yValue());
                const int x1 = xm.transform(mrk->right());
                const int y1 = ym.transform(mrk->bottom());
                mrk->setRect(x0, y0, qAbs(x1 - x0), qAbs(y1 - y0));
            }
            mrk->updateBoundingRect();
        }
        mrk->refreshPixelCache();
    }
    d_plot->replot();
}

void Graph::updateMarkersBoundingRect()
{
    if (d_is_ternary) {
        d_plot->replot();
        return;
    }
    for (int i = 0; i < (int)d_lines.size(); i++) {
        ArrowMarker *mrkL = (ArrowMarker *)d_plot->marker(d_lines[i]);
        if (mrkL)
            mrkL->updateBoundingRect();
    }
    for (int i = 0; i < (int)d_texts.size(); i++) {
        Legend *mrkT = (Legend *)d_plot->marker(d_texts[i]);
        if (!mrkT)
            continue;
        if (mrkT == legend() && repositionAnchoredOutsideRightLegend())
            continue;
        mrkT->updateOrigin();
    }

    for (int i = 0; i < (int)d_images.size(); i++) {
        ImageMarker *mrk = (ImageMarker *)d_plot->marker(d_images[i]);
        if (mrk)
            mrk->updateBoundingRect();
    }
    d_plot->replot();
}
void Graph::hideEvent(QHideEvent *e)
{
    Q_UNUSED(e);
    hidden_size = size();
}

void Graph::resizeEvent(QResizeEvent *e)
{
    if (ignoreResize)
        return;
    QSize oldSize = e->oldSize();
    if (!this->isVisible() || !oldSize.isValid())
        oldSize = hidden_size;
    if (!oldSize.isValid())
        return;

    if (autoScaleFonts) {
        QSize size = e->size();

        double ratio = (double)size.height() / (double)oldSize.height();
        scaleFonts(ratio);
    }

    d_plot->resize(e->size());
    updateSelectionOverlayGeometry();
    // The canvas has its new size: the markers' pixel caches follow (no data change).
    d_plot->updateLayout();
    refreshMarkerPixelCaches();
}

void Graph::scaleFonts(double factor)
{
    if (!std::isfinite(factor) || factor <= 0.0 || std::fabs(factor - 1.0) < 1e-6)
        return;

    auto scaledFont = [factor](QFont font) -> QFont {
        double curPt = font.pointSizeF();
        if (curPt <= 0.0)
            curPt = (font.pointSize() > 0) ? double(font.pointSize()) : 10.0;
        const double nextPt = qBound(4.0, factor * curPt, 144.0);
        font.setPointSizeF(nextPt);
        font.setHintingPreference(QFont::PreferNoHinting);
        return font;
    };

    for (int i = 0; i < (int)d_texts.size(); i++) {
        Legend *mrk = (Legend *)d_plot->marker(d_texts[i]);
        if (mrk)
            mrk->setFont(scaledFont(mrk->font()));
    }
    for (int i = 0; i < QwtPlot::axisCnt; i++) {
        d_plot->setAxisFont(i, scaledFont(axisFont(i)));

        QwtText title = d_plot->axisTitle(i);
        title.setFont(scaledFont(title.font()));
        d_plot->setAxisTitle(i, title);
    }

    QwtText title = d_plot->title();
    title.setFont(scaledFont(title.font()));
    d_plot->setTitle(title);

    d_plot->replot();
}

void Graph::setMargin(int d)
{
    if (d_plot->margin() == d)
        return;

    d_plot->setMargin(d);
    emit modifiedGraph();
}

void Graph::setFrame(int width, const QColor &color)
{
    if (d_plot->frameColor() == color && width == d_plot->lineWidth())
        return;

    QPalette pal = d_plot->palette();
    pal.setColor(QPalette::WindowText, color);
    d_plot->setPalette(pal);

    d_plot->setLineWidth(width);
}

void Graph::setBackgroundColor(const QColor &color)
{
    QPalette p = d_plot->palette();
    p.setColor(QPalette::Window, color);
    d_plot->setPalette(p);

    const bool opaque = (color.alpha() > 0);
    setAutoFillBackground(opaque);
    d_plot->setAutoFillBackground(opaque);
    emit modifiedGraph();
}

void Graph::setCanvasBackground(const QColor &color)
{
    d_plot->setCanvasBackground(color);
    emit modifiedGraph();
}

Qt::BrushStyle Graph::getBrushStyle(int style)
{
    Qt::BrushStyle brushStyle;
    switch (style) {
    case 0:
        brushStyle = Qt::SolidPattern;
        break;
    case 1:
        brushStyle = Qt::HorPattern;
        break;
    case 2:
        brushStyle = Qt::VerPattern;
        break;
    case 3:
        brushStyle = Qt::CrossPattern;
        break;
    case 4:
        brushStyle = Qt::BDiagPattern;
        break;
    case 5:
        brushStyle = Qt::FDiagPattern;
        break;
    case 6:
        brushStyle = Qt::DiagCrossPattern;
        break;
    case 7:
        brushStyle = Qt::Dense1Pattern;
        break;
    case 8:
        brushStyle = Qt::Dense2Pattern;
        break;
    case 9:
        brushStyle = Qt::Dense3Pattern;
        break;
    case 10:
        brushStyle = Qt::Dense4Pattern;
        break;
    case 11:
        brushStyle = Qt::Dense5Pattern;
        break;
    case 12:
        brushStyle = Qt::Dense6Pattern;
        break;
    case 13:
        brushStyle = Qt::Dense7Pattern;
        break;
    default:
        throw runtime_error("invalid brush style");
    }
    return brushStyle;
}

QString Graph::penStyleName(Qt::PenStyle style)
{
    if (style == Qt::SolidLine)
        return "SolidLine";
    else if (style == Qt::DashLine)
        return "DashLine";
    else if (style == Qt::DotLine)
        return "DotLine";
    else if (style == Qt::DashDotLine)
        return "DashDotLine";
    else if (style == Qt::DashDotDotLine)
        return "DashDotDotLine";
    else if (style == Qt::CustomDashLine)
        return "CustomDashLine";
    else
        return "SolidLine";
}

Qt::PenStyle Graph::getPenStyle(int style)
{
    Qt::PenStyle linePen;
    switch (style) {
    case 0:
        linePen = Qt::SolidLine;
        break;
    case 1:
        linePen = Qt::DashLine;
        break;
    case 2:
        linePen = Qt::DotLine;
        break;
    case 3:
        linePen = Qt::DashDotLine;
        break;
    case 4:
        linePen = Qt::DashDotDotLine;
        break;
    case 5:
        linePen = Qt::CustomDashLine;
        break;
    default:
        throw runtime_error("invalid pen style");
    }
    return linePen;
}

Qt::PenStyle Graph::getPenStyle(const QString &s)
{
    Qt::PenStyle style = Qt::SolidLine;
    if (s == "SolidLine")
        style = Qt::SolidLine;
    else if (s == "DashLine")
        style = Qt::DashLine;
    else if (s == "DotLine")
        style = Qt::DotLine;
    else if (s == "DashDotLine")
        style = Qt::DashDotLine;
    else if (s == "DashDotDotLine")
        style = Qt::DashDotDotLine;
    else if (s == "CustomDashLine")
        style = Qt::CustomDashLine;
    return style;
}

int Graph::obsoleteSymbolStyle(int type)
{
    if (type <= 4)
        return type + 1;
    else
        return type + 2;
}

int Graph::curveType(int curveIndex)
{
    if (curveIndex < (int)c_type.size() && curveIndex >= 0)
        return c_type[curveIndex];
    else
        return -1;
}

void Graph::showPlotErrorMessage(QWidget *parent, const QStringList &emptyColumns)
{
    QApplication::restoreOverrideCursor();
    if (ApplicationWindow::isNonInteractiveContext(parent))
        return;

    int n = (int)emptyColumns.count();
    if (n > 1) {
        QString columns;
        for (int i = 0; i < n; i++)
            columns += "<p><b>" + emptyColumns[i] + "</b></p>";

        QMessageBox::warning(parent, tr("Warning"),
                             tr("The columns") + ": " + columns
                                     + tr("are empty and will not be added to the plot!"));
    } else if (n == 1)
        QMessageBox::warning(parent, tr("Warning"),
                             tr("The column") + " <b>" + emptyColumns[0] + "</b> "
                                     + tr("is empty and will not be added to the plot!"));
}

void Graph::showTitleContextMenu()
{
    QMenu titleMenu(this);
    QAction *actEditInline = titleMenu.addAction(
            IconLoader::duotoneIcon(QStringLiteral("text-label")), tr("&Edit Text Inline"), this,
            [this]() {
                if (!d_plot || !d_plot->titleLabel())
                    return;
                QwtTextLabel *tl = d_plot->titleLabel();
                const QRect gRect(tl->mapToGlobal(QPoint(0, 0)), tl->size());
                CanvasInlineTextEditor::openForPlotTitle(this, gRect);
            });
    actEditInline->setShortcut(QKeySequence(Qt::Key_F2));
    titleMenu.addSeparator();
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("cut")), tr("&Cut"), this,
                        SLOT(cutTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("copy")), tr("&Copy"), this,
                        SLOT(copyTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("delete")), tr("&Delete"), this,
                        SLOT(removeTitle()));
    titleMenu.addSeparator();
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")), tr("&Properties..."),
                        this, SIGNAL(viewTitleDialog()));
    ModernUiTheme::upgradeMenuIcons(&titleMenu);
    titleMenu.exec(QCursor::pos());
}

void Graph::cutTitle()
{
    QApplication::clipboard()->setText(d_plot->title().text(), QClipboard::Clipboard);
    removeTitle();
}

void Graph::copyTitle()
{
    QApplication::clipboard()->setText(d_plot->title().text(), QClipboard::Clipboard);
}

void Graph::removeAxisTitle()
{
    int axis = (selectedAxis + 2) % 4; // inconsistent notation in Qwt enumerations between
    // QwtScaleDraw::alignment and QwtPlot::Axis
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;
    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    d_axis_title_auto[axis] = false;
    QwtText t = d_plot->axisTitle(axis);
    t.setText(QString());
    d_plot->setAxisTitle(axis, t);
    if (d_selection.kind == GraphSelection::AxisTitle && d_selection.primaryId == axis)
        setCurrentSelection({ GraphSelection::None, -1, -1 });
    d_plot->replot();
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Remove Axis Title"), beforeSnap);
}

void Graph::cutAxisTitle()
{
    copyAxisTitle();
    removeAxisTitle();
}

void Graph::copyAxisTitle()
{
    int axis = (selectedAxis + 2) % 4; // unconsistent notation in Qwt enumerations between
    // QwtScaleDraw::alignment and QwtPlot::Axis
    QApplication::clipboard()->setText(axisTitle(axis), QClipboard::Clipboard);
}

void Graph::showAxisTitleMenu(int axis)
{
    selectedAxis = axis;
    const int qwtAxis = Plot::scaleAlignmentToAxis(axis);

    QMenu titleMenu(this);
    QAction *actEditInline = titleMenu.addAction(
            IconLoader::duotoneIcon(QStringLiteral("text-label")), tr("&Edit Text Inline"), this,
            [this, qwtAxis]() {
                if (!d_plot || qwtAxis < 0 || qwtAxis >= QwtPlot::axisCnt)
                    return;
                QwtScaleWidget *sw = d_plot->axisWidget(qwtAxis);
                if (!sw)
                    return;
                QRect trLocal = scalePicker ? scalePicker->titleRect(sw) : sw->rect();
                if (!trLocal.isValid() || trLocal.isEmpty())
                    trLocal = sw->rect();
                const QRect gRect(sw->mapToGlobal(trLocal.topLeft()), trLocal.size());
                CanvasInlineTextEditor::openForAxisTitle(this, qwtAxis, gRect);
            });
    actEditInline->setShortcut(QKeySequence(Qt::Key_F2));
    titleMenu.addSeparator();
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("cut")), tr("&Cut"), this,
                        SLOT(cutAxisTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("copy")), tr("&Copy"), this,
                        SLOT(copyAxisTitle()));
    titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("delete")), tr("&Delete"), this,
                        SLOT(removeAxisTitle()));
    titleMenu.addSeparator();
    switch (axis) {
    case QwtScaleDraw::BottomScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(xAxisTitleDblClicked()));
        break;

    case QwtScaleDraw::LeftScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(yAxisTitleDblClicked()));
        break;

    case QwtScaleDraw::TopScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(topAxisTitleDblClicked()));
        break;

    case QwtScaleDraw::RightScale:
        titleMenu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")),
                            tr("&Properties..."), this, SIGNAL(rightAxisTitleDblClicked()));
        break;
    }

    ModernUiTheme::upgradeMenuIcons(&titleMenu);
    titleMenu.exec(QCursor::pos());
}

void Graph::showAxisContextMenu(int axis)
{
    selectedAxis = axis;
    const int qwtAxis = Plot::scaleAlignmentToAxis(axis);
    const QPoint globalClick = QCursor::pos();
    const QPoint canvasPos = (d_plot && d_plot->canvas())
            ? d_plot->canvas()->mapFromGlobal(globalClick)
            : QPoint();

    QMenu menu(this);
    if (qwtAxis >= 0 && qwtAxis < QwtPlot::axisCnt) {
        QAction *rescaleThisAct = menu.addAction(
                IconLoader::duotoneIcon(QStringLiteral("zoom-auto")), tr("Rescale &This Axis"));
        connect(rescaleThisAct, &QAction::triggered, this, [this, qwtAxis]() {
            MultiLayer *ml = multiLayerParent();
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
            setAutoScaleAxis(qwtAxis);
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Rescale Axis"), beforeSnap);
        });
    }
    QAction *rescaleAllAct = menu.addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-auto")),
                                            tr("&Rescale to show all"));
    rescaleAllAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(rescaleAllAct, &QAction::triggered, this, [this]() {
        MultiLayer *ml = multiLayerParent();
        const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
        setAutoScale();
        if (ml && !beforeSnap.isEmpty())
            ml->pushUndoSnapshot(tr("Autoscale Axes"), beforeSnap);
    });

    if (qwtAxis >= 0 && qwtAxis < QwtPlot::axisCnt && d_plot && !isPiePlot() && !isTernaryPlot()) {
        QAction *addRefAct =
                menu.addAction(IconLoader::duotoneIcon(QStringLiteral("draw-line")),
                               tr("Add &Reference Line Here"));
        connect(addRefAct, &QAction::triggered, this, [this, qwtAxis, canvasPos]() {
            const bool isHoriz = (qwtAxis == QwtPlot::yLeft || qwtAxis == QwtPlot::yRight);
            const int mapAxis = isHoriz ? QwtPlot::yLeft : QwtPlot::xBottom;
            const QwtScaleMap map = d_plot->canvasMap(mapAxis);
            double val = map.invTransform(isHoriz ? canvasPos.y() : canvasPos.x());
            if (!std::isfinite(val)) {
                const QwtScaleDiv *div = d_plot->axisScaleDiv(mapAxis);
                val = div ? 0.5 * (div->lowerBound() + div->upperBound()) : 0.0;
            }
            MultiLayer *ml = multiLayerParent();
            const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
            ReferenceLineSpec spec;
            spec.orientation = isHoriz ? 0 : 1;
            spec.value = val;
            spec.style = Qt::DashLine;
            spec.width = 1.5;
            spec.color = QColor(30, 102, 184);
            spec.label = QStringLiteral("%1 = %2")
                                 .arg(isHoriz ? QStringLiteral("Y") : QStringLiteral("X"))
                                 .arg(QLocale::c().toString(val, 'g', 4));
            addReferenceLine(spec);
            setCurrentSelection(
                    { GraphSelection::ReferenceLine, int(d_ref_lines.size()) - 1, -1 });
            if (ml && !beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Add Reference Line"), beforeSnap);
        });

        int matchCurveIdx = -1;
        if (d_selection.kind == GraphSelection::Curve && d_selection.primaryId >= 0
            && d_selection.primaryId < n_curves) {
            if (QwtPlotItem *selIt = plotItem(d_selection.primaryId)) {
                if (selIt->xAxis() == qwtAxis || selIt->yAxis() == qwtAxis)
                    matchCurveIdx = d_selection.primaryId;
            }
        }
        if (matchCurveIdx < 0) {
            for (int ci = 0; ci < n_curves; ++ci) {
                if (ci < c_type.size() && c_type[ci] == ErrorBars)
                    continue;
                QwtPlotItem *it = plotItem(ci);
                if (it && it->isVisible() && (it->xAxis() == qwtAxis || it->yAxis() == qwtAxis)) {
                    matchCurveIdx = ci;
                    break;
                }
            }
        }
        if (matchCurveIdx >= 0) {
            QAction *colorMatchAct =
                    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("palette")),
                                   tr("&Color Axis to Match Curve"));
            connect(colorMatchAct, &QAction::triggered, this, [this, qwtAxis, matchCurveIdx]() {
                MultiLayer *ml = multiLayerParent();
                const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
                syncAxisColorToCurve(qwtAxis, matchCurveIdx);
                if (ml && !beforeSnap.isEmpty())
                    ml->pushUndoSnapshot(tr("Color Axis to Match Curve"), beforeSnap);
            });
        }
    }

    menu.addSeparator();
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("close")), tr("&Hide axis"), this,
                   SLOT(hideSelectedAxis()));

    QAction *gridsID = menu.addAction(IconLoader::duotoneIcon(QStringLiteral("axis-grid")),
                                      tr("&Show grids"), this, SLOT(showGrids()));
    gridsID->setCheckable(true);
    if (axis == QwtScaleDraw::LeftScale || axis == QwtScaleDraw::RightScale) {
        if (d_plot->grid()->yEnabled())
            gridsID->setChecked(true);
    } else {
        if (d_plot->grid()->xEnabled())
            gridsID->setChecked(true);
    }

    menu.addSeparator();
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("axis-scale")), tr("&Scale..."), this,
                   SLOT(showScaleDialog()));
    menu.addAction(IconLoader::duotoneIcon(QStringLiteral("settings")), tr("&Properties..."), this,
                   SLOT(showAxisDialog()));
    ModernUiTheme::upgradeMenuIcons(&menu);
    menu.exec(globalClick);
}

void Graph::showAxisDialog()
{
    emit showAxisDialog(selectedAxis);
}

void Graph::showScaleDialog()
{
    emit axisDblClicked(selectedAxis);
}

void Graph::hideSelectedAxis()
{
    int axis = -1;
    if (selectedAxis == QwtScaleDraw::LeftScale || selectedAxis == QwtScaleDraw::RightScale)
        axis = selectedAxis - 2;
    else
        axis = selectedAxis + 2;

    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    d_plot->enableAxis(axis, false);
    scalePicker->refresh();
    if (d_selection.kind == GraphSelection::Axis && d_selection.primaryId == axis)
        setCurrentSelection({ GraphSelection::None, -1, -1 });
    emit modifiedGraph();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Hide Axis"), beforeSnap);
}

void Graph::showGrids()
{
    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    showGrid(selectedAxis);
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Toggle Grids"), beforeSnap);
}

void Graph::showGrid()
{
    showGrid(QwtScaleDraw::LeftScale);
    showGrid(QwtScaleDraw::BottomScale);
}

void Graph::showGrid(int axis)
{
    Grid *grid = d_plot->grid();
    if (!grid)
        return;

    if (axis == QwtScaleDraw::LeftScale || axis == QwtScaleDraw::RightScale) {
        grid->enableY(!grid->yEnabled());
        grid->enableYMin(!grid->yMinEnabled());
    } else if (axis == QwtScaleDraw::BottomScale || axis == QwtScaleDraw::TopScale) {
        grid->enableX(!grid->xEnabled());
        grid->enableXMin(!grid->xMinEnabled());
    } else
        return;

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::copy(ApplicationWindow *parent, Graph *g)
{
    int i;
    Plot *plot = g->plotWidget();
    d_plot->setMargin(plot->margin());

    setAntialiasing(g->antialiasing());
    autoScaleFonts = g->autoscaleFonts();

    setBackgroundColor(plot->paletteBackgroundColor());
    setFrame(plot->lineWidth(), plot->frameColor());
    setCanvasBackground(plot->canvasBackground());

    enableAxes(g->enabledAxes());
    setAxesColors(g->axesColors());
    setAxesNumColors(g->axesNumColors());
    setAxesBaseline(g->axesBaseline());

    grid()->copy(g->grid());

    d_plot->setTitle(g->plotWidget()->title());

    drawCanvasFrame(g->framed(), g->canvasFrameWidth(), g->canvasFrameColor());

    for (i = 0; i < 4; i++) {
        setAxisTitle(i, g->axisTitle(i));
        setAxisFont(i, g->axisFont(i));
    }

    setXAxisTitleColor(g->axisTitleColor(2));
    setXAxisTitleFont(g->axisTitleFont(2));
    setXAxisTitleAlignment(g->axisTitleAlignment(2));

    setYAxisTitleColor(g->axisTitleColor(0));
    setYAxisTitleFont(g->axisTitleFont(0));
    setYAxisTitleAlignment(g->axisTitleAlignment(0));

    setTopAxisTitleColor(g->axisTitleColor(3));
    setTopAxisTitleFont(g->axisTitleFont(3));
    setTopAxisTitleAlignment(g->axisTitleAlignment(3));

    setRightAxisTitleColor(g->axisTitleColor(1));
    setRightAxisTitleFont(g->axisTitleFont(1));
    setRightAxisTitleAlignment(g->axisTitleAlignment(1));

    setAxesLinewidth(plot->axesLinewidth());
    removeLegend();

    QVector<int> srcToDstIdx(g->curves(), -1);
    for (int pass = 0; pass < 2; ++pass) {
        for (i = 0; i < g->curves(); i++) {
            QwtPlotItem *it = (QwtPlotItem *)g->plotItem(i);
            if (!it)
                continue;
            if (it->rtti() == QwtPlotItem::Rtti_PlotCurve) {
                PlotCurve *srcPc = dynamic_cast<PlotCurve *>(it);
                if (!srcPc)
                    continue;
                DataCurve *cv = dynamic_cast<DataCurve *>(it);
                int n = srcPc->dataSize();
                int style = srcPc->type();
                if ((pass == 0 && style == ErrorBars) || (pass == 1 && style != ErrorBars))
                    continue;

                QVector<double> x(n);
                QVector<double> y(n);
                for (int j = 0; j < n; j++) {
                    x[j] = srcPc->x(j);
                    y[j] = srcPc->y(j);
                }

                PlotCurve *c = nullptr;
                if (style == Pie && cv) {
                    c = new QwtPieCurve(cv->table(), cv->title().text(), cv->startRow(),
                                        cv->endRow());
                    ((QwtPieCurve *)c)->setRay(((QwtPieCurve *)cv)->ray());
                    ((QwtPieCurve *)c)->setFirstColor(((QwtPieCurve *)cv)->firstColor());
                } else if (style == Function || !cv) {
                    if (FunctionCurve *fcSrc = dynamic_cast<FunctionCurve *>(it)) {
                        c = new FunctionCurve(parent, srcPc->title().text());
                        static_cast<FunctionCurve *>(c)->copy(fcSrc);
                    } else {
                        c = new PlotCurve(srcPc->title().text());
                        c->setType(Function);
                    }
                } else if (style == VerticalBars || style == HorizontalBars) {
                    c = new QwtBarCurve(((QwtBarCurve *)cv)->orientation(), cv->table(),
                                        cv->xColumnName(), cv->title().text(), cv->startRow(),
                                        cv->endRow());
                    ((QwtBarCurve *)c)->copy((const QwtBarCurve *)cv);
                } else if (style == ErrorBars) {
                    QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(cv);
                    DataCurve *master_curve = er ? masterCurve(er) : nullptr;
                    if (master_curve) {
                        c = new QwtErrorPlotCurve(cv->table(), cv->title().text());
                        ((QwtErrorPlotCurve *)c)->copy(er);
                        ((QwtErrorPlotCurve *)c)->setMasterCurve(master_curve);
                    }
                } else if (style == Histogram) {
                    c = new QwtHistogram(cv->table(), cv->title().text(), cv->startRow(),
                                         cv->endRow());
                    ((QwtHistogram *)c)->copy((const QwtHistogram *)cv);
                } else if (style == VectXYXY || style == VectXYAM) {
                    VectorCurve::VectorStyle vs = VectorCurve::XYXY;
                    if (style == VectXYAM)
                        vs = VectorCurve::XYAM;
                    c = new VectorCurve(vs, cv->table(), cv->xColumnName(), cv->title().text(),
                                        ((VectorCurve *)cv)->vectorEndXAColName(),
                                        ((VectorCurve *)cv)->vectorEndYMColName(), cv->startRow(),
                                        cv->endRow());
                    ((VectorCurve *)c)->copy((const VectorCurve *)cv);
                } else if (style == Box) {
                    c = new BoxCurve(cv->table(), cv->title().text(), cv->startRow(), cv->endRow());
                    ((BoxCurve *)c)->copy((const BoxCurve *)cv);
                    if (n > 0) {
                        QwtSingleArrayData dat(x[0], y, n);
                        c->setData(dat);
                    }
                } else {
                    c = new DataCurve(cv->table(), cv->xColumnName(), cv->title().text(),
                                      cv->startRow(), cv->endRow());
                }

                if (!c)
                    continue;

                const int dstIdx = n_curves;
                c_keys.resize(++n_curves);
                c_keys[dstIdx] = d_plot->insertCurve(c);

                c_type.resize(n_curves);
                c_type[dstIdx] = (!cv && style != Function) ? int(Function) : g->curveType(i);
                srcToDstIdx[i] = dstIdx;

                if (c_type[dstIdx] != Box && c_type[dstIdx] != ErrorBars)
                    c->setData(x.data(), y.data(), n);

                c->setPen(srcPc->pen());
                c->setBrush(srcPc->brush());
                c->setStyle(srcPc->style());
                c->setSymbol(srcPc->symbol());

                if (srcPc->testCurveAttribute(QwtPlotCurve::Fitted))
                    c->setCurveAttribute(QwtPlotCurve::Fitted, true);
                else if (srcPc->testCurveAttribute(QwtPlotCurve::Inverted))
                    c->setCurveAttribute(QwtPlotCurve::Inverted, true);

                c->setSymbolSkip(srcPc->symbolSkip());
                if (c_type[dstIdx] != Box && c_type[dstIdx] != ErrorBars) {
                    c->setStackNormalize01(srcPc->stackNormalize01());
                    c->setStackOffset(srcPc->stackOffsetX(), srcPc->stackOffsetY());
                }

                c->setAxis(srcPc->xAxis(), srcPc->yAxis());
                c->setVisible(srcPc->isVisible());
                if (cv && cv->hasPointColorMap()) {
                    if (DataCurve *dcDst = dynamic_cast<DataCurve *>(c))
                        dcDst->setPointColorMap(cv->pointColorMap());
                }

                QList<QwtPlotCurve *> lst = g->fitCurvesList();
                if (lst.contains((QwtPlotCurve *)it))
                    d_fit_curves << c;
            } else if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                if (pass != 0)
                    continue;
                Spectrogram *sp = ((Spectrogram *)it)->copy();
                const int dstIdx = n_curves;
                c_keys.resize(++n_curves);
                c_keys[dstIdx] = d_plot->insertCurve(sp);

                sp->showColorScale(((Spectrogram *)it)->colorScaleAxis(),
                                   ((Spectrogram *)it)->hasColorScale());
                sp->setColorBarWidth(((Spectrogram *)it)->colorBarWidth());
                if (!((Spectrogram *)it)->colorBarTitle().isEmpty())
                    sp->setColorBarTitle(((Spectrogram *)it)->colorBarTitle());
                sp->setVisible(it->isVisible());

                c_type.resize(n_curves);
                c_type[dstIdx] = g->curveType(i);
                srcToDstIdx[i] = dstIdx;
            }
        }
    }

    axesFormulas = g->axesFormulas;
    axisType = g->axisType;
    axesFormatInfo = g->axesFormatInfo;
    axisType = g->axisType;

    for (i = 0; i < QwtPlot::axisCnt; i++) {
        QwtScaleWidget *sc = g->plotWidget()->axisWidget(i);
        if (!sc)
            continue;

        QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(i);
        if (sd->hasComponent(QwtAbstractScaleDraw::Labels)) {
            if (axisType[i] == AxisType::Numeric)
                setLabelsNumericFormat(i, plot->axisLabelFormat(i), plot->axisLabelPrecision(i),
                                       axesFormulas[i]);
            else if (axisType[i] == AxisType::Day)
                setLabelsDayFormat(i, axesFormatInfo[i].toInt());
            else if (axisType[i] == AxisType::Month)
                setLabelsMonthFormat(i, axesFormatInfo[i].toInt());
            else if (axisType[i] == AxisType::Time || axisType[i] == AxisType::Date
                     || axisType[i] == AxisType::DateTime)
                setLabelsDateTimeFormat(i, axisType[i], axesFormatInfo[i]);
            else {
                QwtTextScaleDraw *sd = (QwtTextScaleDraw *)plot->axisScaleDraw(i);
                d_plot->setAxisScaleDraw(i, new QwtTextScaleDraw(sd->labelsMap()));
            }
        } else {
            sd = d_plot->axisScaleDraw(i);
            sd->enableComponent(QwtAbstractScaleDraw::Labels, false);
        }
    }
    for (i = 0; i < QwtPlot::axisCnt; i++) { // set same scales
        const QwtScaleEngine *se = plot->axisScaleEngine(i);
        if (!se)
            continue;

        d_axis_breaks[i] = g->axisBreak(i);
        d_suspended_axis_breaks[i] = g->suspendedAxisBreak(i);
        const int primaryMirror = (i == QwtPlot::xTop)
                ? QwtPlot::xBottom
                : ((i == QwtPlot::yRight) ? QwtPlot::yLeft : -1);
        const bool useMirrorBreak = (!d_axis_breaks[i].enabled && primaryMirror >= 0
                                     && g->axisBreak(primaryMirror).enabled
                                     && !g->hasColorScaleOnAxis(i));
        const AxisBreakSpec &activeBreak =
                d_axis_breaks[i].enabled ? d_axis_breaks[i] : g->axisBreak(qMax(0, primaryMirror));

        QwtScaleEngine *sc_engine = 0;
        if (d_axis_breaks[i].enabled || useMirrorBreak)
            sc_engine = new PiecewiseBreakScaleEngine(activeBreak);
        else if (Plot::isLog10ScaleEngine(se))
            sc_engine = new QwtLog10ScaleEngine();
        else
            sc_engine = new QwtLinearScaleEngine();

        int majorTicks = plot->axisMaxMajor(i);
        int minorTicks = plot->axisMaxMinor(i);
        d_plot->setAxisMaxMajor(i, majorTicks);
        d_plot->setAxisMaxMinor(i, minorTicks);

        const QwtScaleDiv *sd = plot->axisScaleDiv(i);
        QwtValueList lst = sd->ticks(QwtScaleDiv::MajorTick);

        d_user_step[i] = g->axisStep(i);

        QwtScaleDiv div;
        if (d_axis_breaks[i].enabled || useMirrorBreak) {
            div = *sd;
            if (se->testAttribute(QwtScaleEngine::Inverted))
                sc_engine->setAttribute(QwtScaleEngine::Inverted);
        } else {
            int max_min_intervals = minorTicks;
            if (minorTicks == 1)
                max_min_intervals = 3;
            else if (minorTicks > 1)
                max_min_intervals = minorTicks + 1;
#if QWT_VERSION >= 0x050200
            div = sc_engine->divideScale(qMin(sd->lowerBound(), sd->upperBound()),
                                         qMax(sd->lowerBound(), sd->upperBound()),
                                         majorTicks, max_min_intervals, d_user_step[i]);
#else
            div = sc_engine->divideScale(qMin(sd->lBound(), sd->hBound()),
                                         qMax(sd->lBound(), sd->hBound()), majorTicks,
                                         max_min_intervals, d_user_step[i]);
#endif
            if (se->testAttribute(QwtScaleEngine::Inverted)) {
                sc_engine->setAttribute(QwtScaleEngine::Inverted);
                div.invert();
            }
        }

        d_plot->setAxisScaleEngine(i, sc_engine);
        d_plot->setAxisScaleDiv(i, div);

        if (ScaleDraw *dstSd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(i))) {
            if (d_axis_breaks[i].enabled || useMirrorBreak) {
                dstSd->setAxisBreakIndicator(true, qMin(activeBreak.breakFrom, activeBreak.breakTo),
                                             qMax(activeBreak.breakFrom, activeBreak.breakTo),
                                             activeBreak.log10Before, activeBreak.log10After);
            } else {
                dstSd->setAxisBreakIndicator(false, 0.0);
            }
        }
    }

    drawAxesBackbones(g->drawAxesBackbone);
    setMajorTicksType(g->plotWidget()->getMajorTicksType());
    setMinorTicksType(g->plotWidget()->getMinorTicksType());
    setTicksLength(g->minorTickLength(), g->majorTickLength());

    setAxisLabelRotation(QwtPlot::xBottom, g->labelsRotation(QwtPlot::xBottom));
    setAxisLabelRotation(QwtPlot::xTop, g->labelsRotation(QwtPlot::xTop));

    QVector<int> imag = g->imageMarkerKeys();
    for (i = 0; i < (int)imag.size(); i++)
        addImage((ImageMarker *)g->imageMarker(imag[i]));

    QVector<int> txtMrkKeys = g->textMarkerKeys();
    for (i = 0; i < (int)txtMrkKeys.size(); i++) {
        Legend *mrk = (Legend *)g->textMarker(txtMrkKeys[i]);
        if (!mrk)
            continue;

        if (txtMrkKeys[i] == g->legendMarkerID)
            legendMarkerID = insertTextMarker(mrk);
        else
            insertTextMarker(mrk);
    }

    QVector<int> l = g->lineMarkerKeys();
    for (i = 0; i < (int)l.size(); i++) {
        ArrowMarker *lmrk = (ArrowMarker *)g->arrow(l[i]);
        if (lmrk)
            addArrow(lmrk);
    }

    for (i = 0; i < QwtPlot::axisCnt; ++i) {
        if (g->drawAxesBackbone)
            setAxisBackboneVisible(i, g->isAxisBackboneVisible(i));
        setTickLabelTransform(i, g->tickDivideFactor(i), g->tickPrefix(i), g->tickSuffix(i));
        if (g->axisBreak(i).enabled)
            setAxisBreak(i, g->axisBreak(i));
    }

    if (!g->referenceLines().isEmpty())
        setReferenceLines(g->referenceLines());

    for (const ShadedBandSpec &bs : g->d_shaded_bands) {
        addShadedBand(bs.x, bs.yLow, bs.yHigh, bs.fillColor, bs.title);
    }
    if (g->d_point_colorbar_active)
        syncPointColorBar();

    d_is_ternary = g->d_is_ternary;
    for (int k = 0; k < 3; ++k)
        d_ternary_labels[k] = g->d_ternary_labels[k];
    if (d_is_ternary)
        setupTernaryPlot(d_ternary_labels[0], d_ternary_labels[1], d_ternary_labels[2]);

    for (int bi = 0; bi < srcToDstIdx.size(); ++bi) {
        const int dstIdx = srcToDstIdx[bi];
        if (dstIdx < 0 || dstIdx >= n_curves)
            continue;
        const QwtBarCurve *srcBc = dynamic_cast<const QwtBarCurve *>(g->curve(bi));
        QwtBarCurve *dstBc = dynamic_cast<QwtBarCurve *>(curve(dstIdx));
        if (srcBc && dstBc && srcBc->isStacked()) {
            dstBc->setStacked(true);
            dstBc->setGap(srcBc->gap());
            dstBc->setOffset(0);
        }
    }

    m_autoscale = g->m_autoscale;
    for (i = 0; i < QwtPlot::axisCnt; ++i)
        setAxisUserLocked(i, g->isAxisUserLocked(i));

    for (int a = 0; a < 4; ++a) {
        if (!g->d_custom_tick_positions[a].isEmpty()) {
            setCustomTickLabels(a, g->d_custom_tick_positions[a], g->d_custom_tick_labels[a],
                                g->d_custom_tick_rotation[a]);
        } else if (!g->d_custom_category_labels[a].isEmpty()) {
            setCategoryLabels(a, g->d_custom_category_labels[a]);
        }
        if (g->d_weibull_prob_axis[a])
            setWeibullProbabilityAxis(a, true);
    }

    if (g->d_is_polar) {
        d_polar_rmax = g->d_polar_rmax;
        setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        d_polar_series = g->d_polar_series;
        const double rMax = qMax(1e-6, d_polar_rmax);
        const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
        const int extraRows = qMax(0, nPolarRows - 1);
        const double yLo = -1.44 * rMax;
        const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
        const double halfX = 0.5 * (yHi - yLo);
        setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
        setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, false);
    }

    d_axis_unit_format = g->axisUnitFormat();
    for (int a = 0; a < 4; ++a)
        d_axis_title_auto[a] = g->isAxisTitleAuto(a);
    d_template_curve_styles = g->d_template_curve_styles;

    d_legend_visible = g->d_legend_visible;
    if (Legend *leg = legend())
        leg->setVisible(d_legend_visible);
    if (g->d_plot && g->d_plot->property("_outsideRightMargin").toInt() > 0)
        d_plot->setProperty("_outsideRightMargin", g->d_plot->property("_outsideRightMargin"));
    if (g->d_plot && g->d_plot->property("_toriLegendAnchor").isValid())
        d_plot->setProperty("_toriLegendAnchor", g->d_plot->property("_toriLegendAnchor"));
    if (g->d_plot && g->d_plot->property("_toriOutsideRightGapPx").isValid())
        d_plot->setProperty("_toriOutsideRightGapPx",
                            g->d_plot->property("_toriOutsideRightGapPx"));
    if (g->d_plot && g->d_plot->property("_toriOutsideRightTopPx").isValid())
        d_plot->setProperty("_toriOutsideRightTopPx",
                            g->d_plot->property("_toriOutsideRightTopPx"));
    if (g->d_plot) {
        if (g->d_plot->property("_toriPlotTitleShift").isValid())
            d_plot->setProperty("_toriPlotTitleShift", g->d_plot->property("_toriPlotTitleShift"));
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            const QwtScaleWidget *srcSw = g->d_plot->axisWidget(a);
            QwtScaleWidget *dstSw = d_plot->axisWidget(a);
            if (!srcSw || !dstSw)
                continue;
            if (srcSw->property("_toriAxisTitleShift").isValid())
                dstSw->setProperty("_toriAxisTitleShift", srcSw->property("_toriAxisTitleShift"));
            if (srcSw->property("_toriAxisNormalShift").isValid())
                dstSw->setProperty("_toriAxisNormalShift", srcSw->property("_toriAxisNormalShift"));
            if (srcSw->property("_toriAxisParallelShift").isValid())
                dstSw->setProperty("_toriAxisParallelShift",
                                   srcSw->property("_toriAxisParallelShift"));
        }
    }

    if (g->d_plot && g->d_plot->property("_toriExtraPlotTags").isValid()) {
        for (const QString &tag : ExtraPlotTypes::syncedGraphTagsForState(g))
            ExtraPlotTypes::restoreGraphTag(this, tag);
    }

    if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
    }

    syncCurveZOrder();
    d_plot->replot();

    if (isPiePlot()) {
        drawCanvasFrame(false, 0);
        setFrame(0, Qt::black);
        drawAxesBackbones(false);
        if (QwtPieCurve *c = (QwtPieCurve *)curve(0))
            c->updateBoundingRect();
    }
}

void Graph::plotBoxDiagram(Table *w, const QStringList &names, int startRow, int endRow)
{
    if (!w || names.isEmpty())
        return;
    if (endRow < 0 || endRow >= w->numRows())
        endRow = w->numRows() - 1;

    const QList<QColor> boxPalette = paletteColors(defaultCurvePaletteName());

    const QString tblPrefix = QString(w->name()) + QLatin1Char('_');
    QStringList catLabels;
    catLabels.reserve(names.count());

    int plottedOrd = 0;
    for (int j = 0; j < (int)names.count(); j++) {
        Column *colPtr = w->column(names[j]);
        if (!colPtr && names[j].startsWith(tblPrefix))
            colPtr = w->column(names[j].mid(tblPrefix.size()));

        BoxCurve *c = new BoxCurve(w, names[j], startRow, endRow);
        c->setData(QwtSingleArrayData(double(plottedOrd + 1), QwtArray<double>(), 0));
        c->loadData();
        if (c->dataSize() <= 0) {
            delete c;
            continue;
        }

        c_keys.resize(++n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(c);
        c_type.resize(n_curves);
        c_type[n_curves - 1] = Box;

        const QColor col = boxPalette.isEmpty() ? QColor(Qt::black)
                                                : boxPalette.at(plottedOrd % boxPalette.size());
        QPen boxPen(col.darker(120), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
        boxPen.setCosmetic(false);
        c->setPen(boxPen);
        QColor fillTint = col;
        fillTint.setAlpha(75);
        c->setBrush(QBrush(fillTint, Qt::SolidPattern));
        QPen symPen(col.darker(135), 1.4, Qt::SolidLine);
        symPen.setCosmetic(false);
        c->setSymbol(QwtSymbol(QwtSymbol::NoSymbol, QBrush(Qt::white, Qt::SolidPattern), symPen,
                               QSize(8, 8)));

        QString catName;
        if (colPtr) {
            catName = firstNonEmptyCommentLine(colPtr->comment());
            if (catName.isEmpty())
                catName = colPtr->name().trimmed();
        }
        if (catName.isEmpty()) {
            catName = names[j];
            if (catName.startsWith(tblPrefix))
                catName = catName.mid(tblPrefix.size());
        }
        catLabels << catName;
        ++plottedOrd;
    }

    if (catLabels.isEmpty())
        return;

    syncCurveZOrder();
    setCategoryLabels(QwtPlot::xBottom, catLabels);
    if (QwtScaleDraw *topSd = d_plot->axisScaleDraw(QwtPlot::xTop))
        topSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
    d_plot->setMajorTicksType(QwtPlot::xTop, ScaleDraw::None);
    d_plot->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
    axesFormatInfo[QwtPlot::xBottom] = w->name();
    axesFormatInfo[QwtPlot::xTop] = w->name();

    {
        const QString curBotPlain = unwrapNoWrapAxisTitle(axisTitle(QwtPlot::xBottom)).trimmed();
        if (isAxisPlaceholderTitle(curBotPlain)) {
            QwtText emptyT = d_plot->axisTitle(QwtPlot::xBottom);
            emptyT.setText(QString());
            d_plot->setAxisTitle(QwtPlot::xBottom, emptyT);
        }
    }
    if (!d_refreshing_auto_titles)
        refreshAutoAxisTitlesFromColumns();

    Legend *mrk = (Legend *)d_plot->marker(legendMarkerID);
    if (mrk)
        mrk->setText(legendText());
}

void Graph::setCurveStyle(int index, int s)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    if (s == 5) // ancient spline style in Qwt 4.2.0
    {
        s = QwtPlotCurve::Lines;
        c->setCurveAttribute(QwtPlotCurve::Fitted, true);
        c_type[index] = Spline;
    } else if (s == QwtPlotCurve::Lines)
        c->setCurveAttribute(QwtPlotCurve::Fitted, false);
    else if (s == 6) // Vertical Steps
    {
        s = QwtPlotCurve::Steps;
        c->setCurveAttribute(QwtPlotCurve::Inverted, true);
        c_type[index] = VerticalSteps;
    } else if (s == QwtPlotCurve::Steps) // Horizontal Steps
    {
        c->setCurveAttribute(QwtPlotCurve::Inverted, false);
        c_type[index] = HorizontalSteps;
    }
    c->setStyle((QwtPlotCurve::CurveStyle)s);
}

void Graph::setCurveSymbol(int index, const QwtSymbol &s)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setSymbol(s);
}

void Graph::setCurvePen(int index, const QPen &p)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setPen(p);
}

void Graph::setCurveBrush(int index, const QBrush &b)
{
    QwtPlotCurve *c = curve(index);
    if (!c)
        return;

    c->setBrush(b);
}

void Graph::openBoxDiagram(Table *w, const QStringList &l, int fileVersion)
{
    if (!w)
        return;

    int s_offset = 0;
    int startRow = 0;
    int endRow = w->numRows() - 1;
    if (fileVersion >= 90) {
        startRow = l[l.count() - 3].toInt();
        endRow = l[l.count() - 2].toInt();
    }

    BoxCurve *c = new BoxCurve(w, l[2], startRow, endRow);
    c->setData(QwtSingleArrayData(l[1].toDouble(), QwtArray<double>(), 0));
    c->setData(QwtSingleArrayData(l[1].toDouble(), QwtArray<double>(), 0));
    c->loadData();

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(c);
    c_type.resize(n_curves);
    c_type[n_curves - 1] = Box;

    if (fileVersion >= 0x011800) // 1.24.0
    {
        s_offset = 3;
    }
    c->setMaxStyle(SymbolBox::style(l[16 + s_offset].toInt()));
    c->setP99Style(SymbolBox::style(l[17 + s_offset].toInt()));
    c->setMeanStyle(SymbolBox::style(l[18 + s_offset].toInt()));
    c->setP1Style(SymbolBox::style(l[19 + s_offset].toInt()));
    c->setMinStyle(SymbolBox::style(l[20 + s_offset].toInt()));

    c->setBoxStyle(l[21 + s_offset].toInt());
    c->setBoxWidth(l[22 + s_offset].toInt());
    c->setBoxRange(l[23 + s_offset].toInt(), l[24 + s_offset].toDouble());
    c->setWhiskersRange(l[25 + s_offset].toInt(), l[26 + s_offset].toDouble());
    if (l.count() > 33 + s_offset) {
        c->setViolinMode(l[27 + s_offset].toInt() != 0);
        c->setShowDataPoints(l[28 + s_offset].toInt() != 0);
    }
}

void Graph::setActiveTool(PlotToolInterface *tool)
{
    if (d_active_tool)
        delete d_active_tool;
    d_active_tool = tool;
    if (d_range_selector) {
        if (tool)
            // we want to use tool now, so disable range selection
            d_range_selector->setEnabled(false);
        else
            // re-enable range selection
            d_range_selector->setEnabled(true);
    }
}

void Graph::disableTools()
{
    if (zoomOn())
        zoom(false);
    if (drawLineActive())
        drawLine(false);
    setActiveTool(NULL);
    if (d_range_selector)
        delete d_range_selector;
}

bool Graph::enableRangeSelectors(const QObject *status_target, const char *status_slot)
{
    // disable other tools, otherwise it's undefined what tool should handle mouse clicks
    disableTools();
    d_range_selector = new RangeSelectorTool(this, status_target, status_slot);
    if (!d_range_selector->selectedCurve()) {
        delete d_range_selector;
        return false;
    }
    connect(d_range_selector, SIGNAL(changed()), this, SIGNAL(dataRangeChanged()));
    return true;
}

void Graph::setTextMarkerDefaults(int f, const QFont &font, const QColor &textCol,
                                  const QColor &backgroundCol)
{
    QFont resolved = font;
    if (resolved.family().trimmed().isEmpty())
        resolved = QFont("Arial", 12, QFont::Normal, false);
    defaultMarkerFont = resolved;
    defaultMarkerFrame = f;
    defaultTextMarkerColor = textCol;
    defaultTextMarkerBackground = backgroundCol;
}

void Graph::setArrowDefaults(int lineWidth, const QColor &c, Qt::PenStyle style, int headLength,
                             int headAngle, bool fillHead)
{
    defaultArrowLineWidth = lineWidth;
    defaultArrowColor = c;
    defaultArrowLineStyle = style;
    defaultArrowHeadLength = headLength;
    defaultArrowHeadAngle = headAngle;
    defaultArrowHeadFill = fillHead;
}

QString Graph::parentPlotName()
{
    if (MultiLayer *ml = multiLayerParent())
        return QString(ml->objectName());
    QObject *p = parent();
    while (p) {
        if (QWidget *w = qobject_cast<QWidget *>(p)) {
            if (!w->objectName().isEmpty() && w->objectName() != QLatin1String("MultiLayerPageHost")
                && w->objectName() != QLatin1String("MultiLayerCanvasScroll")
                && w->objectName() != QLatin1String("qt_scrollarea_viewport"))
                return QString(w->objectName());
        }
        p = p->parent();
    }
    return QString();
}

void Graph::guessUniqueCurveLayout(int &colorIndex, int &symbolIndex)
{
    // New curves follow the configured default palette (ToriGraph Standard unless changed in
    // the preset manager) together with Origin's symbol-shape increment.
    const QList<QColor> palette = paletteColors(defaultCurvePaletteName());
    const int kPaletteCount = qMax(1, palette.size());

    int curve_index = n_curves - 1;
    if (curve_index >= 0 && curve_index < c_type.size()
        && c_type[curve_index] == ErrorBars) { // find out the pen color of the master curve
        QwtErrorPlotCurve *er =
                dynamic_cast<QwtErrorPlotCurve *>(d_plot->curve(c_keys[curve_index]));
        DataCurve *master_curve = er ? er->masterCurve() : nullptr;
        if (master_curve) {
            colorIndex = ColorButton::colorIndex(master_curve->pen().color());
            symbolIndex = 1;
            return;
        }
    }

    int dataCurves = 0;
    for (int i = 0; i < n_curves; i++) {
        const QwtPlotCurve *c = curve(i);
        if (c && c->rtti() == QwtPlotItem::Rtti_PlotCurve && c_type[i] != ErrorBars)
            dataCurves++;
    }
    const int seriesIdx = qMax(0, dataCurves - 1);
    const QColor col = palette.isEmpty() ? QColor(Qt::black) : palette.at(seriesIdx % kPaletteCount);
    colorIndex = int(col.rgba());
    symbolIndex = defaultCurveSymbolIndex(seriesIdx);
}

void Graph::addFitCurve(QwtPlotCurve *c)
{
    if (c)
        d_fit_curves << c;
}

void Graph::deleteFitCurves()
{
    const QList<QwtPlotCurve *> fitCopy = d_fit_curves;
    for (QwtPlotCurve *c : fitCopy)
        removeCurve(curveIndex(c));

    d_plot->replot();
}

void Graph::plotSpectrogram(Matrix *m, CurveType type)
{
    if (type != GrayMap && type != ColorMap && type != ContourMap)
        return;

    Spectrogram *d_spectrogram = new Spectrogram(m);
    if (type == GrayMap)
        d_spectrogram->setGrayScale();
    else if (type == ContourMap) {
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ImageMode, false);
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, true);
    } else if (type == ColorMap) {
        // New heatmaps default to perceptually uniform Viridis (the legacy rainbow map is still
        // selectable and is restored for projects that saved it).
        d_spectrogram->setColorMapPreset(Spectrogram::PresetViridis, false);
        d_spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, false);
    }

    c_keys.resize(++n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(d_spectrogram);

    c_type.resize(n_curves);
    c_type[n_curves - 1] = type;
    syncCurveZOrder();

    QwtScaleWidget *rightAxis = d_plot->axisWidget(QwtPlot::yRight);
    rightAxis->setColorBarEnabled(type != ContourMap);
    rightAxis->setColorMap(d_spectrogram->effectiveValueRange(), d_spectrogram->colorMap());

    d_plot->setAxisScale(QwtPlot::xBottom, m->xStart(), m->xEnd());
    d_plot->setAxisScale(QwtPlot::yLeft, m->yStart(), m->yEnd());

    d_plot->setAxisScale(QwtPlot::yRight, d_spectrogram->effectiveValueRange().minValue(),
                         d_spectrogram->effectiveValueRange().maxValue());
    d_plot->enableAxis(QwtPlot::yRight, type != ContourMap);

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QFont tf = d_plot->axisFont(a);
        tf.setFamily(QStringLiteral("Arial"));
        tf.setBold(false);
        tf.setHintingPreference(QFont::PreferNoHinting);
        d_plot->setAxisFont(a, tf);
        QwtText tt = d_plot->axisTitle(a);
        QFont ttf = tt.font();
        ttf.setFamily(QStringLiteral("Arial"));
        ttf.setBold(false);
        ttf.setHintingPreference(QFont::PreferNoHinting);
        tt.setFont(ttf);
        const QString plainT = unwrapNoWrapAxisTitle(tt.text()).trimmed();
        if (isAxisPlaceholderTitle(plainT))
            tt.setText(QString());
        d_plot->setAxisTitle(a, tt);
    }
    d_spectrogram->enforceColorBarArialFont();

    d_plot->replot();
}

void Graph::restoreSpectrogram(ApplicationWindow *app, const QStringList &lst)
{
    QStringList::const_iterator line = lst.begin();
    QString s = (*line).trimmed();
    QString matrixName = s.remove("<matrix>").remove("</matrix>");
    Matrix *m = app->matrix(matrixName);
    if (!m)
        return;

    Spectrogram *sp = new Spectrogram(m);

    c_type.resize(++n_curves);
    c_type[n_curves - 1] = Graph::ColorMap;
    c_keys.resize(n_curves);
    c_keys[n_curves - 1] = d_plot->insertCurve(sp);
    syncCurveZOrder();

    bool hasPresetTag = false;
    Spectrogram::ColorMapPreset loadedPreset = Spectrogram::PresetLegacyDefault;
    bool revPalette = false;
    bool hasCustomZ = false;
    double zMin = 0.0, zMax = 1.0;
    int pendingLevels = -1;
    bool hasColorBar = false;
    int colorBarAxis = QwtPlot::yRight;
    int colorBarW = 16;
    QString cbTitle;
    bool maskInvalid = false, logColor = false, cellBlocks = false; // E11b

    for (line++; line != lst.end(); line++) {
        QString s = *line;
        if (s.contains("<ToriMaskInvalid>")) {
            maskInvalid = s.remove("<ToriMaskInvalid>").remove("</ToriMaskInvalid>").trimmed().toInt() != 0;
        } else if (s.contains("<ToriLogColor>")) {
            logColor = s.remove("<ToriLogColor>").remove("</ToriLogColor>").trimmed().toInt() != 0;
        } else if (s.contains("<ToriCellBlocks>")) {
            cellBlocks = s.remove("<ToriCellBlocks>").remove("</ToriCellBlocks>").trimmed().toInt() != 0;
        } else if (s.contains("<ColorPolicy>")) {
            int color_policy = s.remove("<ColorPolicy>").remove("</ColorPolicy>").trimmed().toInt();
            if (color_policy == Spectrogram::GrayScale)
                sp->setGrayScale();
            else if (color_policy == Spectrogram::Default)
                sp->setDefaultColorMap();
        } else if (s.contains("<ColorMap>")) {
            s = *(++line);
            int mode = s.remove("<Mode>").remove("</Mode>").trimmed().toInt();
            s = *(++line);
            QColor color1 =
                    QColor(COLORVALUE(s.remove("<MinColor>").remove("</MinColor>").trimmed()));
            s = *(++line);
            QColor color2 =
                    QColor(COLORVALUE(s.remove("<MaxColor>").remove("</MaxColor>").trimmed()));

            QwtLinearColorMap colorMap = QwtLinearColorMap(color1, color2);
            colorMap.setMode((QwtLinearColorMap::Mode)mode);

            s = *(++line);
            int stops = s.remove("<ColorStops>").remove("</ColorStops>").trimmed().toInt();
            for (int i = 0; i < stops; i++) {
                s = (*(++line)).trimmed();
                QStringList l = s.remove("<Stop>").remove("</Stop>").split("\t");
                colorMap.addColorStop(l[0].toDouble(), QColor(COLORVALUE(l[1])));
            }
            sp->setCustomColorMap(colorMap);
            line++;
        } else if (s.contains("<Image>")) {
            int mode = s.remove("<Image>").remove("</Image>").trimmed().toInt();
            sp->setDisplayMode(QwtPlotSpectrogram::ImageMode, mode);
        } else if (s.contains("<ContourLines>")) {
            int contours = s.remove("<ContourLines>").remove("</ContourLines>").trimmed().toInt();
            sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, contours);
            if (contours) {
                s = (*(++line)).trimmed();
                int levels = s.remove("<Levels>").remove("</Levels>").toInt();
                pendingLevels = levels;
                sp->setLevelsNumber(levels);

                s = (*(++line)).trimmed();
                int defaultPen = s.remove("<DefaultPen>").remove("</DefaultPen>").toInt();
                if (!defaultPen)
                    sp->setDefaultContourPen(Qt::NoPen);
                else {
                    s = (*(++line)).trimmed();
                    QColor c = QColor(COLORVALUE(s.remove("<PenColor>").remove("</PenColor>")));
                    s = (*(++line)).trimmed();
                    int width = s.remove("<PenWidth>").remove("</PenWidth>").toInt();
                    s = (*(++line)).trimmed();
                    int style = s.remove("<PenStyle>").remove("</PenStyle>").toInt();
                    sp->setDefaultContourPen(QPen(c, width, Graph::getPenStyle(style)));
                }
            }
        } else if (s.contains("<ColorBar>")) {
            s = *(++line);
            colorBarAxis = s.remove("<axis>").remove("</axis>").trimmed().toInt();
            s = *(++line);
            colorBarW = s.remove("<width>").remove("</width>").trimmed().toInt();
            hasColorBar = true;
            line++;
        } else if (s.contains("<ColorPreset>")) {
            const int p = s.remove("<ColorPreset>").remove("</ColorPreset>").trimmed().toInt();
            if (p >= static_cast<int>(Spectrogram::PresetLegacyDefault)
                && p <= static_cast<int>(Spectrogram::PresetCustom)) {
                loadedPreset = static_cast<Spectrogram::ColorMapPreset>(p);
                hasPresetTag = true;
            }
        } else if (s.contains("<ReversePalette>")) {
            revPalette =
                    s.remove("<ReversePalette>").remove("</ReversePalette>").trimmed().toInt() != 0;
        } else if (s.contains("<CustomZRange>")) {
            const QStringList parts =
                    s.remove("<CustomZRange>").remove("</CustomZRange>").trimmed().split("\t");
            if (parts.size() >= 3 && parts[0].toInt() != 0) {
                hasCustomZ = true;
                zMin = parts[1].toDouble();
                zMax = parts[2].toDouble();
            }
        } else if (s.contains("<ColorBarTitle>")) {
            QString raw = s.trimmed();
            if (raw.startsWith("<ColorBarTitle>"))
                raw = raw.mid(15);
            if (raw.endsWith("</ColorBarTitle>"))
                raw.chop(16);
            raw.replace("&#10;", "\n")
                    .replace("&gt;", ">")
                    .replace("&lt;", "<")
                    .replace("&amp;", "&");
            cbTitle = raw;
        } else if (s.contains("<Visible>")) {
            int on = s.remove("<Visible>").remove("</Visible>").trimmed().toInt();
            sp->setVisible(on);
        }
    }

    if (maskInvalid)
        sp->setMaskInvalidCells(true);
    if (cellBlocks)
        sp->setCellBlocks(true);
    if (logColor)
        sp->setLogColorScale(true);
    if ((maskInvalid || cellBlocks || logColor) && !hasCustomZ && pendingLevels >= 2)
        sp->setLevelsNumber(pendingLevels);
    if (hasCustomZ) {
        sp->setCustomValueRange(true, zMin, zMax);
        if (pendingLevels >= 2)
            sp->setLevelsNumber(pendingLevels);
    }
    if (hasPresetTag)
        sp->setColorMapPreset(loadedPreset, revPalette);
    else if (revPalette)
        sp->setReversePalette(true);

    if (hasColorBar) {
        sp->showColorScale(colorBarAxis, true);
        sp->setColorBarWidth(colorBarW);
    } else {
        if (QwtScaleWidget *ca = d_plot->axisWidget(sp->colorScaleAxis()))
            ca->setColorBarEnabled(false);
    }
    if (!cbTitle.isEmpty())
        sp->setColorBarTitle(cbTitle);
    sp->syncColorScale();
    sp->enforceColorBarArialFont();
}

bool Graph::validCurvesDataSize() const
{
    if (!n_curves) {
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(const_cast<Graph *>(this), tr("Warning"),
                                 tr("There are no curves available on this plot!"));
        }
        return false;
    } else {
        for (int i = 0; i < n_curves; i++) {
            QwtPlotItem *item = curve(i);
            if (item && item->rtti() != QwtPlotItem::Rtti_PlotSpectrogram) {
                QwtPlotCurve *c = (QwtPlotCurve *)item;
                if (c->dataSize() >= 2)
                    return true;
            }
        }
        if (!ApplicationWindow::isNonInteractiveContext(this)) {
            QMessageBox::warning(const_cast<Graph *>(this), tr("Error"),
                                 tr("There are no curves with more than two points on this plot. "
                                    "Operation aborted!"));
        }
        return false;
    }
}

Graph::~Graph()
{
    CanvasInlineTextEditor::cancelActiveForGraph(this);
    cancelInlineScaleEditor();
    detachAndDeleteMarkerSelector(d_markers_selector);
    setActiveTool(NULL);
    if (d_range_selector)
        delete d_range_selector;
    delete titlePicker;
    delete scalePicker;
    delete cp;
    delete d_plot;
}

void Graph::setAntialiasing(bool on, bool update)
{
    if (d_antialiasing == on)
        return;

    d_antialiasing = on;

    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *c = d_plot->plotItem(c_keys[i]);
        if (c)
            c->setRenderHint(QwtPlotItem::RenderAntialiased, d_antialiasing);
    }
    QList<int> marker_keys = d_plot->markerKeys();
    for (int i = 0; i < (int)marker_keys.count(); i++) {
        QwtPlotMarker *m = d_plot->marker(marker_keys[i]);
        if (m)
            m->setRenderHint(QwtPlotItem::RenderAntialiased, d_antialiasing);
    }

    if (update)
        d_plot->replot();
}

bool Graph::focusNextPrevChild(bool next)
{
    QVector<GraphSelection> targets;
    targets.reserve(n_curves + d_texts.size() + d_lines.size() + d_images.size()
                    + d_ref_lines.size() + 10);

    // 0. Visible plot title (B4-8)
    if (d_plot && d_plot->titleLabel() && d_plot->titleLabel()->isVisible()
        && !d_plot->title().text().trimmed().isEmpty()) {
        targets.append({ GraphSelection::PlotTitle, -1, -1 });
    }

    // 1. Visible curves
    for (int i = 0; i < n_curves; ++i) {
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = plotItem(i);
        if (it && it->isVisible())
            targets.append({ GraphSelection::Curve, i, -1 });
    }

    // 2. Legend
    if (legendMarkerID >= 0 && isLegendVisible()) {
        if (Legend *leg = legend()) {
            if (leg->isVisible())
                targets.append({ GraphSelection::LegendBox, int(legendMarkerID), -1 });
        }
    }

    // 3. Text, Arrow/Line, and Image annotations
    for (int id : d_texts) {
        if (id == legendMarkerID)
            continue;
        if (Legend *tm = textMarker(id)) {
            if (tm->isVisible())
                targets.append({ GraphSelection::TextAnnotation, id, -1 });
        }
    }
    for (int id : d_lines) {
        if (ArrowMarker *am = arrow(id)) {
            if (am->isVisible())
                targets.append({ GraphSelection::ArrowAnnotation, id, -1 });
        }
    }
    for (int id : d_images) {
        if (ImageMarker *im = imageMarker(id)) {
            if (im->isVisible())
                targets.append({ GraphSelection::ImageAnnotation, id, -1 });
        }
    }

    // 4. Reference lines
    for (int r = 0; r < d_ref_lines.size(); ++r)
        targets.append({ GraphSelection::ReferenceLine, r, -1 });

    // 5. Enabled axes and their visible titles (B4-8)
    if (d_plot) {
        const int axisOrder[4] = { QwtPlot::xBottom, QwtPlot::yLeft, QwtPlot::xTop,
                                   QwtPlot::yRight };
        for (int axisId : axisOrder) {
            if (!d_plot->axisEnabled(axisId))
                continue;
            QwtScaleWidget *sw = d_plot->axisWidget(axisId);
            if (!sw || !sw->isVisible())
                continue;
            targets.append({ GraphSelection::Axis, axisId, -1 });
            if (!axisTitle(axisId).trimmed().isEmpty())
                targets.append({ GraphSelection::AxisTitle, axisId, -1 });
        }
    }

    auto focusExternalPane = [&]() -> bool {
        QWidget *boundary = multiLayerParent() ? static_cast<QWidget *>(multiLayerParent())
                                               : static_cast<QWidget *>(this);
        QWidget *win = boundary ? boundary->window() : nullptr;
        QWidget *cur = QApplication::focusWidget();
        if (!cur || !boundary || (cur != boundary && !boundary->isAncestorOf(cur)))
            cur = boundary;
        if (!cur || !win)
            return false;
        QWidget *cand = next ? cur->nextInFocusChain() : cur->previousInFocusChain();
        QWidget *firstCand = cand;
        int guard = 0;
        while (cand && guard++ < 4096) {
            if (cand != boundary && !boundary->isAncestorOf(cand) && cand->window() == win
                && cand->isVisible() && cand->isEnabled()
                && (cand->focusPolicy() & Qt::TabFocus)) {
                deselect();
                cand->setFocus(next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
                return true;
            }
            cand = next ? cand->nextInFocusChain() : cand->previousInFocusChain();
            if (cand == firstCand || cand == cur)
                break;
        }
        return false;
    };

    if (targets.isEmpty())
        return focusExternalPane();

    int curIdx = -1;
    for (int k = 0; k < targets.size(); ++k) {
        if (targets[k].kind == d_selection.kind && targets[k].primaryId == d_selection.primaryId) {
            curIdx = k;
            break;
        }
    }
    if (curIdx < 0 && selectedMarker >= 0) {
        for (int k = 0; k < targets.size(); ++k) {
            if ((targets[k].kind == GraphSelection::LegendBox
                 || targets[k].kind == GraphSelection::TextAnnotation
                 || targets[k].kind == GraphSelection::ArrowAnnotation
                 || targets[k].kind == GraphSelection::ImageAnnotation)
                && targets[k].primaryId == selectedMarker) {
                curIdx = k;
                break;
            }
        }
    }

    const int n = targets.size();
    // Allow Tab / Shift+Tab at the last / first element to leave the graph to adjacent docks/panes (B4-8).
    if (curIdx >= 0 && ((next && curIdx == n - 1) || (!next && curIdx == 0))) {
        if (focusExternalPane())
            return true;
    }

    const int nextIdx = (curIdx < 0) ? (next ? 0 : (n - 1))
                                     : (next ? ((curIdx + 1) % n) : ((curIdx - 1 + n) % n));

    if (cp)
        cp->disableEditing();

    const Qt::FocusReason reason = next ? Qt::TabFocusReason : Qt::BacktabFocusReason;
    const GraphSelection &chosen = targets[nextIdx];
    if (chosen.kind == GraphSelection::PlotTitle) {
        if (titlePicker)
            titlePicker->setSelected(true);
        if (d_plot && d_plot->titleLabel())
            d_plot->titleLabel()->setFocus(reason);
        setCurrentSelection(chosen);
    } else if (chosen.kind == GraphSelection::Axis) {
        if (scalePicker && d_plot) {
            if (QwtScaleWidget *sw = d_plot->axisWidget(chosen.primaryId)) {
                scalePicker->selectLabels(sw, true);
                sw->setFocus(reason);
            }
        }
        setCurrentSelection(chosen);
    } else if (chosen.kind == GraphSelection::AxisTitle) {
        if (scalePicker && d_plot) {
            if (QwtScaleWidget *sw = d_plot->axisWidget(chosen.primaryId)) {
                scalePicker->selectTitle(sw, true);
                sw->setFocus(reason);
            }
        }
        setCurrentSelection(chosen);
    } else if (chosen.kind == GraphSelection::LegendBox
               || chosen.kind == GraphSelection::TextAnnotation
               || chosen.kind == GraphSelection::ArrowAnnotation
               || chosen.kind == GraphSelection::ImageAnnotation) {
        setSelectedMarker(chosen.primaryId);
        setCurrentSelection(chosen);
        if (d_plot && d_plot->canvas() && !d_markers_selector)
            d_plot->canvas()->setFocus(reason);
    } else {
        setCurrentSelection(chosen);
        if (d_plot && d_plot->canvas())
            d_plot->canvas()->setFocus(reason);
    }
    return true;
}

QString Graph::axisFormatInfo(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    else
        return axesFormatInfo[axis];
}

void Graph::updateCurveNames(const QString &oldName, const QString &newName, bool updateTableName)
{
    if (oldName.isEmpty() || oldName == newName)
        return;

    struct ChangedCurveInfo
    {
        int curveIdx = -1;
        QString prevTitle;
        QString nextTitle;
        QString oldShort;
        QString newShort;
        QString colLong;
    };
    QHash<int, ChangedCurveInfo> changedByOneBased;
    QList<ChangedCurveInfo> changedList;

    // update plotted curves list
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it || it->rtti() != QwtPlotItem::Rtti_PlotCurve)
            continue;

        DataCurve *c = dynamic_cast<DataCurve *>(it);
        if (!c)
            continue;

        const QString prevTitle = c->title().text();
        c->updateColumnNames(oldName, newName, updateTableName);
        const QString nextTitle = c->title().text();
        if (prevTitle != nextTitle) {
            ChangedCurveInfo info;
            info.curveIdx = curveIndex(c);
            info.prevTitle = prevTitle;
            info.nextTitle = nextTitle;
            if (Table *tbl = c->table()) {
                if (!updateTableName) {
                    const QString tblPrefix = QString(tbl->name()) + QLatin1Char('_');
                    if (prevTitle.startsWith(tblPrefix))
                        info.oldShort = prevTitle.mid(tblPrefix.size());
                    if (nextTitle.startsWith(tblPrefix))
                        info.newShort = nextTitle.mid(tblPrefix.size());
                }
                const int yIdx = tbl->colIndex(nextTitle);
                if (Column *yc = (yIdx >= 0 ? tbl->column(yIdx) : nullptr))
                    info.colLong = yc->longName().trimmed();
            }
            changedList.append(info);
            if (info.curveIdx >= 0)
                changedByOneBased.insert(info.curveIdx + 1, info);
        }
    }

    const bool extraTagsChanged =
            ExtraPlotTypes::updateGraphTagsForRename(this, oldName, newName, updateTableName, false);

    if (Legend *mrk = legend()) {
        auto isIdentChar = [](QChar ch) -> bool {
            return ch.isLetterOrNumber() || ch == QLatin1Char('_');
        };
        auto replaceExactToken = [&](const QString &src, const QString &fromTok,
                                     const QString &toTok) -> QString {
            if (fromTok.isEmpty() || fromTok == toTok)
                return src;
            QString out;
            out.reserve(src.size());
            int pos = 0;
            while (pos < src.size()) {
                const int hit = src.indexOf(fromTok, pos);
                if (hit < 0) {
                    out.append(src.mid(pos));
                    break;
                }
                const bool leftBoundary = (hit == 0) || !isIdentChar(src.at(hit - 1));
                const int after = hit + fromTok.size();
                const bool rightBoundary = (after >= src.size()) || !isIdentChar(src.at(after));
                if (leftBoundary && rightBoundary) {
                    out.append(src.mid(pos, hit - pos));
                    out.append(toTok);
                    pos = after;
                } else {
                    out.append(src.mid(pos, (hit - pos) + 1));
                    pos = hit + 1;
                }
            }
            return out;
        };

        const QString origText = mrk->text();
        const bool trailingNewline = origText.endsWith(QLatin1Char('\n'));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList lst = origText.split("\n", Qt::SkipEmptyParts);
#else
        QStringList lst = origText.split("\n", QString::SkipEmptyParts);
#endif
        bool anyLegendChanged = false;
        for (int li = 0; li < lst.size(); ++li) {
            const QString line = lst[li];
            int prefixEnd = -1;
            int oneBased = -1;

            const int cPos = line.indexOf(QStringLiteral("\\c{"));
            if (cPos >= 0 && line.left(cPos).trimmed().isEmpty()) {
                const int closeBrace = line.indexOf(QLatin1Char('}'), cPos + 3);
                if (closeBrace > cPos + 3) {
                    bool ok = false;
                    const int parsed = line.mid(cPos + 3, closeBrace - (cPos + 3)).toInt(&ok);
                    if (ok && parsed >= 1) {
                        oneBased = parsed;
                        prefixEnd = closeBrace + 1;
                    }
                }
            } else {
                const int lPos = line.indexOf(QStringLiteral("\\l("));
                if (lPos >= 0 && line.left(lPos).trimmed().isEmpty()) {
                    const int closeParen = line.indexOf(QLatin1Char(')'), lPos + 3);
                    if (closeParen > lPos + 3) {
                        bool ok = false;
                        const int parsed = line.mid(lPos + 3, closeParen - (lPos + 3)).toInt(&ok);
                        if (ok && parsed >= 1) {
                            oneBased = parsed;
                            prefixEnd = closeParen + 1;
                        }
                    }
                }
            }

            if (oneBased >= 1 && prefixEnd > 0) {
                if (!changedByOneBased.contains(oneBased))
                    continue;

                const ChangedCurveInfo &info = changedByOneBased.value(oneBased);
                const QString cmdPrefix = line.left(prefixEnd);
                const QString rest = line.mid(prefixEnd);
                QString newRest = rest;

                if (rest == info.prevTitle) {
                    newRest = info.nextTitle;
                } else if (!updateTableName && !info.oldShort.isEmpty() && rest == info.oldShort) {
                    newRest = !info.colLong.isEmpty() ? info.colLong : info.newShort;
                    if (info.curveIdx >= 0)
                        d_last_auto_curve_labels[info.curveIdx] = newRest;
                } else if (!updateTableName && !info.oldShort.isEmpty()
                           && rest.trimmed() == info.oldShort) {
                    const int idx = rest.indexOf(info.oldShort);
                    const QString target = !info.colLong.isEmpty() ? info.colLong : info.newShort;
                    newRest = rest.left(idx) + target + rest.mid(idx + info.oldShort.size());
                } else {
                    newRest = replaceExactToken(rest, info.prevTitle, info.nextTitle);
                }

                if (newRest != rest) {
                    lst[li] = cmdPrefix + newRest;
                    anyLegendChanged = true;
                }
            } else if (!changedList.isEmpty()) {
                QString updated = line;
                for (const ChangedCurveInfo &info : changedList)
                    updated = replaceExactToken(updated, info.prevTitle, info.nextTitle);
                if (updated != line) {
                    lst[li] = updated;
                    anyLegendChanged = true;
                }
            }
        }

        if (anyLegendChanged) {
            QString joined = lst.join("\n");
            if (trailingNewline && !joined.isEmpty())
                joined += QLatin1Char('\n');
            mrk->setText(joined);
        }
        if (anyLegendChanged || !changedList.isEmpty() || extraTagsChanged)
            d_plot->replot();
    } else if (!changedList.isEmpty() || extraTagsChanged) {
        d_plot->replot();
    }
}

void Graph::setCurveFullRange(int curveIndex)
{
    DataCurve *c = dynamic_cast<DataCurve *>(curve(curveIndex));
    if (c) {
        c->setFullRange();
        updatePlot();
        emit modifiedGraph();
    }
}

DataCurve *Graph::masterCurve(QwtErrorPlotCurve *er)
{
    if (!er || !er->masterCurve())
        return nullptr;
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); i++) {
        if (c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!pc || pc->type() == Function)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->plotAssociation() == er->masterCurve()->plotAssociation())
            return dc;
    }
    return nullptr;
}

DataCurve *Graph::masterCurve(const QString &xColName, const QString &yColName)
{
    QString master_curve = xColName + "(X)," + yColName + "(Y)";
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); i++) {
        if (c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it || it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(it);
        if (!pc || pc->type() == Function)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        if (dc && dc->plotAssociation() == master_curve)
            return dc;
    }
    return nullptr;
}

bool Graph::isCurveVisible(int index) const
{
    if (index < 0 || index >= n_curves || index >= c_keys.size() || !d_plot)
        return false;
    const QwtPlotItem *it = d_plot->plotItem(c_keys[index]);
    return it ? it->isVisible() : false;
}

void Graph::showCurve(int index, bool visible)
{
    if (index < 0 || index >= n_curves)
        return;

    QwtPlotItem *it = plotItem(index);
    if (!it)
        return;

    it->setVisible(visible);

    if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
        Spectrogram *sp = static_cast<Spectrogram *>(it);
        if (QwtScaleWidget *sw = d_plot->axisWidget(sp->colorScaleAxis()))
            sw->setColorBarEnabled(visible && sp->hasColorScale());
    } else if (c_type[index] != ErrorBars && c_type[index] != Function) {
        if (DataCurve *dc = dynamic_cast<DataCurve *>(it)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setVisible(visible);
            }
        }
    }

    if (!visible && d_plot->highlightedCurveKey() == c_keys[index])
        d_plot->setHighlightedCurveKey(-1);

    updateStatReferenceLines();
    d_plot->replot();
    emit curveVisibilityChanged(index, visible);
    emit modifiedGraph();
}

int Graph::visibleCurves()
{
    int c = 0;
    for (int i = 0; i < n_curves && i < c_keys.size(); i++) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (it && it->isVisible())
            c++;
    }
    return c;
}

QPrinter::PageSize Graph::minPageSize(const QPrinter &printer, const QRect &r)
{
    double x_margin = 0.2 / 2.54 * printer.logicalDpiX(); // 2 mm margins
    double y_margin = 0.2 / 2.54 * printer.logicalDpiY();
    double w_mm = 2 * x_margin + (double)(r.width()) / (double)printer.logicalDpiX() * 25.4;
    double h_mm = 2 * y_margin + (double)(r.height()) / (double)printer.logicalDpiY() * 25.4;

    int w, h;
    if (w_mm / h_mm > 1) {
        w = (int)ceil(w_mm);
        h = (int)ceil(h_mm);
    } else {
        h = (int)ceil(w_mm);
        w = (int)ceil(h_mm);
    }

    QPrinter::PageSize size = QPrinter::A5;
    if (w < 45 && h < 32)
        size = QPrinter::B10;
    else if (w < 52 && h < 37)
        size = QPrinter::A9;
    else if (w < 64 && h < 45)
        size = QPrinter::B9;
    else if (w < 74 && h < 52)
        size = QPrinter::A8;
    else if (w < 91 && h < 64)
        size = QPrinter::B8;
    else if (w < 105 && h < 74)
        size = QPrinter::A7;
    else if (w < 128 && h < 91)
        size = QPrinter::B7;
    else if (w < 148 && h < 105)
        size = QPrinter::A6;
    else if (w < 182 && h < 128)
        size = QPrinter::B6;
    else if (w < 210 && h < 148)
        size = QPrinter::A5;
    else if (w < 220 && h < 110)
        size = QPrinter::DLE;
    else if (w < 229 && h < 163)
        size = QPrinter::C5E;
    else if (w < 241 && h < 105)
        size = QPrinter::Comm10E;
    else if (w < 257 && h < 182)
        size = QPrinter::B5;
    else if (w < 279 && h < 216)
        size = QPrinter::Letter;
    else if (w < 297 && h < 210)
        size = QPrinter::A4;
    else if (w < 330 && h < 210)
        size = QPrinter::Folio;
    else if (w < 356 && h < 216)
        size = QPrinter::Legal;
    else if (w < 364 && h < 257)
        size = QPrinter::B4;
    else if (w < 420 && h < 297)
        size = QPrinter::A3;
    else if (w < 515 && h < 364)
        size = QPrinter::B3;
    else if (w < 594 && h < 420)
        size = QPrinter::A2;
    else if (w < 728 && h < 515)
        size = QPrinter::B2;
    else if (w < 841 && h < 594)
        size = QPrinter::A1;
    else if (w < 1030 && h < 728)
        size = QPrinter::B1;
    else if (w < 1189 && h < 841)
        size = QPrinter::A0;
    else if (w < 1456 && h < 1030)
        size = QPrinter::B0;

    return size;
}

int Graph::mapToQwtAxis(int axis)
{
    int a = -1;
    switch (axis) {
    case 0:
        a = QwtPlot::xBottom;
        break;
    case 1:
        a = QwtPlot::yLeft;
        break;
    case 2:
        a = QwtPlot::xTop;
        break;
    case 3:
        a = QwtPlot::yRight;
        break;
    }
    return a;
}

void Graph::print(QPainter *painter, const QRect &plotRect, const QwtPlotPrintFilter &pfilter)
{
    ScopedExportFlag exportGuard(d_is_exporting);
    struct ScopedTransientItemsHider {
        QVector<QwtPlotItem *> hiddenItems;
        ScopedTransientItemsHider(Plot *plot, bool exporting)
        {
            if (!plot || !exporting)
                return;
            const QwtPlotItemList &items = plot->itemList();
            for (QwtPlotItem *it : items) {
                if (it && it->isVisible()
                    && it->title().text().startsWith(QLatin1String("__tg_transient_ui_"))) {
                    it->setVisible(false);
                    hiddenItems.append(it);
                }
            }
        }
        ~ScopedTransientItemsHider()
        {
            for (QwtPlotItem *it : hiddenItems) {
                if (it)
                    it->setVisible(true);
            }
        }
    } transientGuard(d_plot, d_is_exporting);
    if (d_plot) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            QFont af = d_plot->axisFont(a);
            if (af.hintingPreference() != QFont::PreferNoHinting) {
                af.setHintingPreference(QFont::PreferNoHinting);
                d_plot->setAxisFont(a, af);
            }
            QwtText at = d_plot->axisTitle(a);
            QFont atf = at.font();
            if (atf.hintingPreference() != QFont::PreferNoHinting) {
                atf.setHintingPreference(QFont::PreferNoHinting);
                at.setFont(atf);
                d_plot->setAxisTitle(a, at);
            }
        }
        QwtText pt = d_plot->title();
        QFont ptf = pt.font();
        if (ptf.hintingPreference() != QFont::PreferNoHinting) {
            ptf.setHintingPreference(QFont::PreferNoHinting);
            pt.setFont(ptf);
            d_plot->setTitle(pt);
        }
        for (int tid : d_texts) {
            if (Legend *mrk = dynamic_cast<Legend *>(d_plot->marker(tid))) {
                QFont mf = mrk->font();
                if (mf.hintingPreference() != QFont::PreferNoHinting)
                    mrk->setFont(mf);
            }
        }
    }
    d_plot->print(painter, plotRect, pfilter);
}

void Graph::deselect()
{
    deselectMarker();
    if (scalePicker)
        scalePicker->deselect();
    if (titlePicker)
        titlePicker->setSelected(false);
    if (d_plot)
        d_plot->setHighlightedCurveKey(-1);
    setCurrentSelection({ GraphSelection::None, -1, -1 });

    // A deselected title/axis must not keep keyboard focus: otherwise Delete / arrow keys
    // still reach the (invisibly) focused title or scale widget after Esc (B4-3).
    if (d_plot) {
        bool hadFocus = false;
        if (QwtTextLabel *title = d_plot->titleLabel()) {
            if (title->hasFocus()) {
                title->clearFocus();
                hadFocus = true;
            }
        }
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if (QwtScaleWidget *sw = d_plot->axisWidget(a)) {
                if (sw->hasFocus()) {
                    sw->clearFocus();
                    hadFocus = true;
                }
            }
        }
        if (hadFocus && d_plot->canvas())
            d_plot->canvas()->setFocus(Qt::OtherFocusReason);
    }
}

// ============================================================================
// OriginPro Color Palette System & Interactive Palette Editor
// ============================================================================

namespace {
static QMap<QString, QList<QColor>> &customPaletteMemoryStore()
{
    static QMap<QString, QList<QColor>> store;
    return store;
}

static QList<QColor> hexListToColors(const QStringList &hexList)
{
    QList<QColor> out;
    out.reserve(hexList.size());
    for (const QString &h : hexList) {
        QColor c(h.trimmed());
        if (c.isValid())
            out.append(c);
    }
    return out;
}
} // namespace

const QList<QColor> &Graph::okabeItoColors()
{
    // Okabe & Ito (2008), blue-first order used throughout ToriGraph. The eighth entry is black
    // (part of the original set); never a non-palette gray.
    static const QList<QColor> kColors = hexListToColors(
            { "#0072b2", "#e69f00", "#009e73", "#d55e00", "#cc79a7", "#56b4e9", "#f0e442",
              "#000000" });
    return kColors;
}

const QList<QColor> &Graph::tab10Colors()
{
    static const QList<QColor> kColors = hexListToColors(
            { "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2",
              "#7f7f7f", "#bcbd22", "#17becf" });
    return kColors;
}

const QList<QColor> &Graph::originClassicColors()
{
    // OriginPro "Classic" color increment list: Black, Red, Blue, Magenta, Olive, Navy,
    // Violet, Purple, Wine, Dark Cyan, Royal, Orange.
    static const QList<QColor> kColors = hexListToColors(
            { "#000000", "#ff0000", "#0000ff", "#ff00ff", "#808000", "#000080", "#8000ff",
              "#800080", "#800000", "#008080", "#0000a0", "#ff8000" });
    return kColors;
}

const QList<QColor> &Graph::toriGraphStandardColors()
{
    // Default for new curves. Black first (the order people know from single-colour journal
    // figures), then colour-vision-safe hues darkened where needed for crisp contrast on white
    // pages and slide projectors: vermilion, blue, bluish green, dark amber (#b07800),
    // deep sky blue (#2b8cc4), reddish purple, gray (#666666).
    // Okabe-Ito yellow (#f0e442) is left out because it is hard to see on a white page.
    static const QList<QColor> kColors = hexListToColors(
            { "#000000", "#d55e00", "#0072b2", "#009e73", "#b07800", "#2b8cc4", "#cc79a7",
              "#666666" });
    return kColors;
}

void Graph::registerPaletteTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("ToriGraph Standard", "ToriGraph 標準 (黒・朱・青…)");
        reg("ToriGraph Standard (black, vermilion, blue, ...)", "ToriGraph 標準 (黒・朱・青…)");
        reg("\"%1\" is the name of a built-in palette. Choose another name.",
            "「%1」は組み込みのパレットの名前です。別の名前にしてください。");
        reg("Enter a palette name and at least one colour.",
            "パレットの名前と、少なくとも 1 つの色を指定してください。");
    });
}

QString Graph::canonicalPaletteName(const QString &name)
{
    const QString key = name.trimmed();
    if (key.compare(QLatin1String("ISIJ / Origin Default"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Origin Default"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("ISIJ"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("tab10"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Matplotlib"), Qt::CaseInsensitive) == 0
        || key.compare(tab10PaletteName(), Qt::CaseInsensitive) == 0)
        return tab10PaletteName();
    if (key.compare(QLatin1String("Origin Classic"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Origin"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("origin_classic"), Qt::CaseInsensitive) == 0)
        return originClassicPaletteName();
    if (key.compare(toriGraphStandardPaletteName(), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("ToriGraph"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("torigraph_standard"), Qt::CaseInsensitive) == 0)
        return toriGraphStandardPaletteName();
    if (key.compare(QLatin1String("Okabe-Ito"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("okabe_ito"), Qt::CaseInsensitive) == 0
        || key.compare(okabeItoPaletteName(), Qt::CaseInsensitive) == 0)
        return okabeItoPaletteName();
    if (key.compare(QLatin1String("Cool-Warm"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("CoolWarm"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("cool_warm"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("CoolWarm");
    return key;
}

namespace {
QString &defaultCurvePaletteCache()
{
    static QString s_name;
    return s_name;
}
} // namespace

QString Graph::defaultCurvePaletteName()
{
    QString &cached = defaultCurvePaletteCache();
    if (cached.isEmpty()) {
        QSettings &settings = ApplicationWindow::getSettings();
        const QString stored =
                settings.value(QStringLiteral("/Curves/DefaultPalette"), QString()).toString();
        // Users who never picked a palette follow the built-in default (ToriGraph Standard
        // since 2026-11; it was Origin Classic before). An explicit choice is kept as is.
        cached = stored.trimmed().isEmpty() ? builtInDefaultCurvePaletteName()
                                            : canonicalPaletteName(stored);
    }
    return cached;
}

void Graph::setDefaultCurvePaletteName(const QString &name)
{
    QSettings &settings = ApplicationWindow::getSettings();
    if (name.trimmed().isEmpty()) {
        // Back to "not chosen": follow the built-in default, also after it changes.
        defaultCurvePaletteCache() = builtInDefaultCurvePaletteName();
        settings.remove(QStringLiteral("/Curves/DefaultPalette"));
        return;
    }
    const QString canon = canonicalPaletteName(name);
    defaultCurvePaletteCache() = canon;
    settings.setValue(QStringLiteral("/Curves/DefaultPalette"), canon);
}

void Graph::reloadDefaultCurvePaletteName()
{
    defaultCurvePaletteCache().clear();
}

int Graph::defaultCurveSymbolIndex(int seriesIndex)
{
    // SymbolBox indices following Origin's symbol increment: square, circle, up triangle,
    // down triangle, diamond, left triangle, right triangle, hexagon, star, cross.
    static const int kSymbolIndices[] = { 2, 1, 6, 5, 3, 7, 8, 15, 13, 9 };
    const int n = int(sizeof(kSymbolIndices) / sizeof(kSymbolIndices[0]));
    return kSymbolIndices[qMax(0, seriesIndex) % n];
}

QStringList Graph::builtInPaletteNames()
{
    registerPaletteTranslations();
    return QStringList()
            << "ToriGraph Standard"
            << "Origin Classic"
            << "Matplotlib (tab10)"
            << "Viridis"
            << "Plasma"
            << "Inferno"
            << "CoolWarm"
            << "Nature (NPG)"
            << "Science (AAAS)"
            << "Okabe-Ito (Colorblind Safe)"
            << "Monochrome / Grayscale";
}

QStringList Graph::customPaletteNames()
{
    QStringList names = customPaletteMemoryStore().keys();
    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    for (const QString &k : settings.childKeys()) {
        if (!names.contains(k))
            names.append(k);
    }
    settings.endGroup();
    names.sort();
    return names;
}

QStringList Graph::allPaletteNames()
{
    QStringList names = builtInPaletteNames();
    for (const QString &custom : customPaletteNames()) {
        if (!names.contains(custom))
            names.append(custom);
    }
    return names;
}

QList<QColor> Graph::paletteColors(const QString &paletteName)
{
    const QString key = canonicalPaletteName(paletteName);
    if (key == tab10PaletteName())
        return tab10Colors();
    if (key == originClassicPaletteName())
        return originClassicColors();
    if (key == toriGraphStandardPaletteName())
        return toriGraphStandardColors();
    if (key.compare("Viridis", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#440154", "#482878", "#3e4989", "#31688e", "#26828e",
                                 "#1f9e89", "#35b779", "#6ece58", "#b5de2b", "#fde725" });
    }
    if (key.compare("Plasma", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#0d0887", "#46039f", "#7201a8", "#9c179e", "#bd3786",
                                 "#d8576b", "#ed7953", "#fb9f3a", "#fdca26", "#f0f921" });
    }
    if (key.compare("Inferno", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#000004", "#1b0c41", "#4a0c6b", "#781c6d", "#a52c60",
                                 "#cf4446", "#ed6925", "#fb9b06", "#f7d13d", "#fcffa4" });
    }
    if (key.compare("CoolWarm", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#3b4cc0", "#5977e3", "#7b9ff9", "#9ebeff", "#c0d4f5",
                                 "#f2cbb7", "#f7a889", "#e87a5f", "#c53334", "#b40426" });
    }
    if (key.compare("Nature (NPG)", Qt::CaseInsensitive) == 0
        || key.compare("Nature", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#e64b35", "#4dbbd5", "#00a087", "#3c5488", "#f39b7f",
                                 "#8491b4", "#91d1c2", "#dc0000", "#7e6148", "#b09c85" });
    }
    if (key.compare("Science (AAAS)", Qt::CaseInsensitive) == 0
        || key.compare("Science", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#3b4992", "#ee0000", "#008b45", "#631879", "#008280",
                                 "#bb0021", "#5f559b", "#a20056", "#808180", "#1b1919" });
    }
    if (key == okabeItoPaletteName())
        return okabeItoColors();
    if (key.compare("Monochrome / Grayscale", Qt::CaseInsensitive) == 0
        || key.compare("Monochrome", Qt::CaseInsensitive) == 0
        || key.compare("Grayscale", Qt::CaseInsensitive) == 0) {
        return hexListToColors({ "#000000", "#252525", "#454545", "#636363",
                                 "#7f7f7f", "#969696", "#bdbdbd", "#d9d9d9" });
    }

    auto &memStore = customPaletteMemoryStore();
    if (memStore.contains(key))
        return memStore.value(key);

    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    if (settings.contains(key)) {
        const QStringList hexList = settings.value(key).toStringList();
        settings.endGroup();
        QList<QColor> cols = hexListToColors(hexList);
        if (!cols.isEmpty()) {
            memStore.insert(key, cols);
            return cols;
        }
    } else {
        settings.endGroup();
    }

    // Fallback: the palette used for new curves.
    const QString defName = defaultCurvePaletteName();
    if (defName == okabeItoPaletteName())
        return okabeItoColors();
    if (defName == tab10PaletteName())
        return tab10Colors();
    if (defName == originClassicPaletteName())
        return originClassicColors();
    return toriGraphStandardColors();
}

bool Graph::isBuiltInPaletteName(const QString &name)
{
    const QString key = name.trimmed();
    if (key.isEmpty())
        return false;
    // Aliases ("ToriGraph", "torigraph_standard", "Okabe-Ito", "ISIJ / Origin Default", ...)
    // resolve to a built-in palette before custom palettes are looked up.
    if (canonicalPaletteName(key) != key)
        return true;
    for (const QString &builtIn : builtInPaletteNames()) {
        if (key.compare(builtIn, Qt::CaseInsensitive) == 0)
            return true;
    }
    // Short names that paletteColors() also maps to built-in lists.
    static const char *const kShort[] = { "Nature", "Science", "Monochrome", "Grayscale" };
    for (const char *s : kShort) {
        if (key.compare(QLatin1String(s), Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool Graph::saveCustomPalette(const QString &name, const QList<QColor> &colors, QString *error)
{
    const QString key = name.trimmed();
    if (key.isEmpty() || colors.isEmpty()) {
        if (error)
            *error = tr("Enter a palette name and at least one colour.");
        return false;
    }
    if (isBuiltInPaletteName(key)) {
        // A custom palette under this name would never be used (the built-in one wins).
        registerPaletteTranslations();
        if (error)
            *error = tr("\"%1\" is the name of a built-in palette. Choose another name.").arg(key);
        return false;
    }

    customPaletteMemoryStore().insert(key, colors);

    QStringList hexList;
    hexList.reserve(colors.size());
    for (const QColor &c : colors) {
        if (c.isValid())
            hexList.append(c.name());
    }
    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    settings.setValue(key, hexList);
    settings.endGroup();
    return true;
}

void Graph::removeCustomPalette(const QString &name)
{
    const QString key = name.trimmed();
    if (key.isEmpty())
        return;

    customPaletteMemoryStore().remove(key);
    QSettings settings;
    settings.beginGroup("CustomColorPalettes");
    settings.remove(key);
    settings.endGroup();
}

QList<QColor> Graph::interpolatePalette(const QList<QColor> &anchors, int count)
{
    QList<QColor> result;
    if (count <= 0)
        return result;

    QList<QColor> validAnchors;
    for (const QColor &c : anchors) {
        if (c.isValid())
            validAnchors.append(c);
    }
    if (validAnchors.isEmpty())
        validAnchors = paletteColors("ISIJ / Origin Default");

    result.reserve(count);
    if (validAnchors.size() == 1 || count == 1) {
        for (int i = 0; i < count; ++i)
            result.append(validAnchors.first());
        return result;
    }

    const int nSeg = validAnchors.size() - 1;
    for (int i = 0; i < count; ++i) {
        const double t = double(i) / double(count - 1);
        const double pos = t * double(nSeg);
        const int idx = qMin(int(std::floor(pos)), nSeg - 1);
        const double frac = pos - double(idx);
        const QColor &c0 = validAnchors[idx];
        const QColor &c1 = validAnchors[idx + 1];

        const int r = qBound(0, qRound((1.0 - frac) * c0.red() + frac * c1.red()), 255);
        const int g = qBound(0, qRound((1.0 - frac) * c0.green() + frac * c1.green()), 255);
        const int b = qBound(0, qRound((1.0 - frac) * c0.blue() + frac * c1.blue()), 255);
        const int a = qBound(0, qRound((1.0 - frac) * c0.alpha() + frac * c1.alpha()), 255);
        result.append(QColor(r, g, b, a));
    }
    return result;
}

void Graph::applyColorPalette(const QList<QColor> &colors, bool gradientMode, bool applyToSymbols)
{
    QList<QColor> validColors;
    for (const QColor &c : colors) {
        if (c.isValid())
            validColors.append(c);
    }
    if (validColors.isEmpty() || n_curves <= 0)
        return;

    QList<int> targetIndices;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == ColorMap || c_type[i] == GrayMap
            || c_type[i] == ContourMap || c_type[i] == Pie)
            continue;
        QwtPlotCurve *c = curve(i);
        if (c)
            targetIndices.append(i);
    }
    if (targetIndices.isEmpty())
        return;

    const int m = targetIndices.size();
    const QList<QColor> assignedColors = gradientMode ? interpolatePalette(validColors, m) : validColors;

    for (int k = 0; k < m; ++k) {
        const int idx = targetIndices[k];
        QwtPlotCurve *c = curve(idx);
        if (!c)
            continue;

        const QColor col = assignedColors[k % assignedColors.size()];
        QPen pen = c->pen();
        pen.setColor(col);
        c->setPen(pen);

        if (c_type[idx] == VectXYXY || c_type[idx] == VectXYAM) {
            VectorCurve *vc = dynamic_cast<VectorCurve *>(c);
            if (vc)
                vc->setColor(col);
        }

        if (c->brush().style() != Qt::NoBrush) {
            QBrush br = c->brush();
            QColor fillCol = col;
            if (br.color().alpha() < 255)
                fillCol.setAlpha(br.color().alpha());
            br.setColor(fillCol);
            c->setBrush(br);
        }

        if (applyToSymbols) {
            QwtSymbol sym = c->symbol();
            const bool wasWhiteHollow = isSymbolWhiteHollow(sym);
            QPen symPen = sym.pen();
            symPen.setColor(col);
            sym.setPen(symPen);
            if (wasWhiteHollow) {
                sym.setBrush(QBrush(Qt::white, Qt::SolidPattern));
            } else if (sym.brush().style() != Qt::NoBrush) {
                QBrush symBrush = sym.brush();
                symBrush.setColor(col);
                sym.setBrush(symBrush);
            }
            c->setSymbol(sym);
        }

        DataCurve *dc = dynamic_cast<DataCurve *>(c);
        if (dc) {
            for (DataCurve *eb : dc->errorBarsList()) {
                QwtErrorPlotCurve *errCurve = dynamic_cast<QwtErrorPlotCurve *>(eb);
                if (errCurve)
                    errCurve->setColor(col);
            }
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::applyColorPaletteByName(const QString &paletteName, bool gradientMode)
{
    const QList<QColor> cols = paletteColors(paletteName);
    if (!cols.isEmpty())
        applyColorPalette(cols, gradientMode, true);
}

bool Graph::showColorPaletteEditor(QWidget *parent, Graph *targetGraph)
{
    QPointer<Graph> safeTarget(targetGraph);
    const QString beforeSnap = (safeTarget && safeTarget->multiLayerParent())
            ? safeTarget->multiLayerParent()->captureUndoSnapshot()
            : QString();

    QDialog dlg(parent);
    dlg.setWindowTitle(tr("Color Palette & Gradient Editor"));
    dlg.setMinimumSize(560, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    // 1. Preset / Custom Palette Selector Row
    QGroupBox *presetGroup = new QGroupBox(tr("Color Palette Preset / Custom Palettes"), &dlg);
    QHBoxLayout *presetLayout = new QHBoxLayout(presetGroup);
    QComboBox *comboPalettes = new QComboBox(presetGroup);
    comboPalettes->addItems(allPaletteNames());
    QPushButton *btnSaveCustom = new QPushButton(tr("Save As Custom..."), presetGroup);
    QPushButton *btnDeleteCustom = new QPushButton(tr("Delete Custom"), presetGroup);
    presetLayout->addWidget(new QLabel(tr("Palette:"), presetGroup));
    presetLayout->addWidget(comboPalettes, 1);
    presetLayout->addWidget(btnSaveCustom);
    presetLayout->addWidget(btnDeleteCustom);
    mainLayout->addWidget(presetGroup);

    // 2. Color List & Swatch Editor
    QGroupBox *colorsGroup = new QGroupBox(tr("Palette Colors (Swatches & Hex Codes)"), &dlg);
    QHBoxLayout *colorsLayout = new QHBoxLayout(colorsGroup);
    QListWidget *colorListWidget = new QListWidget(colorsGroup);
    colorListWidget->setIconSize(QSize(36, 18));

    QVBoxLayout *colorBtnLayout = new QVBoxLayout();
    QPushButton *btnAddColor = new QPushButton(tr("Add Color..."), colorsGroup);
    QPushButton *btnEditColor = new QPushButton(tr("Edit Color..."), colorsGroup);
    QPushButton *btnRemoveColor = new QPushButton(tr("Remove Color"), colorsGroup);
    QPushButton *btnMoveUp = new QPushButton(tr("Move Up"), colorsGroup);
    QPushButton *btnMoveDown = new QPushButton(tr("Move Down"), colorsGroup);
    colorBtnLayout->addWidget(btnAddColor);
    colorBtnLayout->addWidget(btnEditColor);
    colorBtnLayout->addWidget(btnRemoveColor);
    colorBtnLayout->addSpacing(6);
    colorBtnLayout->addWidget(btnMoveUp);
    colorBtnLayout->addWidget(btnMoveDown);
    colorBtnLayout->addStretch();

    colorsLayout->addWidget(colorListWidget, 1);
    colorsLayout->addLayout(colorBtnLayout);
    mainLayout->addWidget(colorsGroup, 1);

    // 3. Gradient Generator Group
    QGroupBox *gradGroup = new QGroupBox(tr("Gradient Generator"), &dlg);
    QGridLayout *gradLayout = new QGridLayout(gradGroup);
    ColorButton *btnStartColor = new ColorButton(gradGroup);
    btnStartColor->setColor(QColor("#1f77b4"));
    QCheckBox *chkMidColor = new QCheckBox(tr("Middle Color:"), gradGroup);
    chkMidColor->setChecked(true);
    ColorButton *btnMidColor = new ColorButton(gradGroup);
    btnMidColor->setColor(QColor("#2ca02c"));
    ColorButton *btnEndColor = new ColorButton(gradGroup);
    btnEndColor->setColor(QColor("#d62728"));
    QSpinBox *spinGradSteps = new QSpinBox(gradGroup);
    spinGradSteps->setRange(2, 32);
    spinGradSteps->setValue(10);
    QPushButton *btnGenerateGrad = new QPushButton(tr("Generate Gradient"), gradGroup);

    gradLayout->addWidget(new QLabel(tr("Start Color:"), gradGroup), 0, 0);
    gradLayout->addWidget(btnStartColor, 0, 1);
    gradLayout->addWidget(chkMidColor, 0, 2);
    gradLayout->addWidget(btnMidColor, 0, 3);
    gradLayout->addWidget(new QLabel(tr("End Color:"), gradGroup), 1, 0);
    gradLayout->addWidget(btnEndColor, 1, 1);
    gradLayout->addWidget(new QLabel(tr("Steps (2-32):"), gradGroup), 1, 2);
    gradLayout->addWidget(spinGradSteps, 1, 3);
    gradLayout->addWidget(btnGenerateGrad, 0, 4, 2, 1);
    mainLayout->addWidget(gradGroup);

    // 4. Application Mode Group
    QGroupBox *modeGroup = new QGroupBox(tr("Application Mode"), &dlg);
    QHBoxLayout *modeLayout = new QHBoxLayout(modeGroup);
    QRadioButton *radioCycle = new QRadioButton(tr("Cycle Palette Colors"), modeGroup);
    QRadioButton *radioGradient =
            new QRadioButton(tr("Interpolate Gradient Across Curves"), modeGroup);
    QCheckBox *chkSymbols = new QCheckBox(tr("Apply to Symbols"), modeGroup);
    radioCycle->setChecked(true);
    chkSymbols->setChecked(true);
    modeLayout->addWidget(radioCycle);
    modeLayout->addWidget(radioGradient);
    modeLayout->addStretch();
    modeLayout->addWidget(chkSymbols);
    mainLayout->addWidget(modeGroup);

    // 5. Dialog Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QPushButton *btnApplyGraph = new QPushButton(tr("Apply to Active Graph"), &dlg);
    QPushButton *btnOk = new QPushButton(tr("OK"), &dlg);
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), &dlg);
    btnOk->setDefault(true);
    bottomLayout->addWidget(btnApplyGraph);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnOk);
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);

    auto makeSwatchIcon = [](const QColor &c) -> QIcon {
        QPixmap pm(36, 18);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.fillRect(0, 0, 35, 17, c);
        p.setPen(QColor("#303030"));
        p.drawRect(0, 0, 35, 17);
        p.end();
        return QIcon(pm);
    };

    auto populateList = [&](const QList<QColor> &cols) {
        colorListWidget->clear();
        for (int i = 0; i < cols.size(); ++i) {
            const QColor &c = cols[i];
            QListWidgetItem *item = new QListWidgetItem(
                    makeSwatchIcon(c),
                    QString("#%1  %2").arg(i + 1, 2, 10, QChar('0')).arg(c.name().toUpper()));
            item->setData(Qt::UserRole, c);
            colorListWidget->addItem(item);
        }
        if (colorListWidget->count() > 0)
            colorListWidget->setCurrentRow(0);
    };

    auto currentColorsFromList = [&]() -> QList<QColor> {
        QList<QColor> cols;
        for (int i = 0; i < colorListWidget->count(); ++i) {
            QColor c = colorListWidget->item(i)->data(Qt::UserRole).value<QColor>();
            if (c.isValid())
                cols.append(c);
        }
        return cols;
    };

    auto renumberItems = [&]() {
        for (int i = 0; i < colorListWidget->count(); ++i) {
            QListWidgetItem *it = colorListWidget->item(i);
            QColor c = it->data(Qt::UserRole).value<QColor>();
            it->setIcon(makeSwatchIcon(c));
            it->setText(QString("#%1  %2").arg(i + 1, 2, 10, QChar('0')).arg(c.name().toUpper()));
        }
    };

    populateList(paletteColors(comboPalettes->currentText()));
    btnDeleteCustom->setEnabled(!builtInPaletteNames().contains(comboPalettes->currentText()));

    QObject::connect(comboPalettes, &QComboBox::currentTextChanged, &dlg, [&](const QString &name) {
        if (name.isEmpty())
            return;
        populateList(paletteColors(name));
        btnDeleteCustom->setEnabled(!builtInPaletteNames().contains(name));
    });

    QObject::connect(chkMidColor, &QCheckBox::toggled, btnMidColor, &QWidget::setEnabled);

    QObject::connect(btnAddColor, &QPushButton::clicked, &dlg, [&]() {
        QColor c = QColorDialog::getColor(QColor("#1f77b4"), &dlg, tr("Select Color"),
                                          QColorDialog::DontUseNativeDialog);
        if (!c.isValid())
            return;
        QListWidgetItem *item = new QListWidgetItem(makeSwatchIcon(c), c.name().toUpper());
        item->setData(Qt::UserRole, c);
        colorListWidget->addItem(item);
        renumberItems();
        colorListWidget->setCurrentItem(item);
    });

    auto editSelectedColor = [&]() {
        QListWidgetItem *item = colorListWidget->currentItem();
        if (!item)
            return;
        QColor initial = item->data(Qt::UserRole).value<QColor>();
        QColor c = QColorDialog::getColor(initial, &dlg, tr("Edit Color"),
                                          QColorDialog::DontUseNativeDialog);
        if (!c.isValid())
            return;
        item->setData(Qt::UserRole, c);
        renumberItems();
    };

    QObject::connect(btnEditColor, &QPushButton::clicked, &dlg, editSelectedColor);
    QObject::connect(colorListWidget, &QListWidget::itemDoubleClicked, &dlg,
                     [&](QListWidgetItem *) { editSelectedColor(); });

    QObject::connect(btnRemoveColor, &QPushButton::clicked, &dlg, [&]() {
        int row = colorListWidget->currentRow();
        if (row < 0 || colorListWidget->count() <= 1)
            return;
        delete colorListWidget->takeItem(row);
        renumberItems();
    });

    QObject::connect(btnMoveUp, &QPushButton::clicked, &dlg, [&]() {
        int row = colorListWidget->currentRow();
        if (row <= 0)
            return;
        QListWidgetItem *it = colorListWidget->takeItem(row);
        colorListWidget->insertItem(row - 1, it);
        colorListWidget->setCurrentRow(row - 1);
        renumberItems();
    });

    QObject::connect(btnMoveDown, &QPushButton::clicked, &dlg, [&]() {
        int row = colorListWidget->currentRow();
        if (row < 0 || row >= colorListWidget->count() - 1)
            return;
        QListWidgetItem *it = colorListWidget->takeItem(row);
        colorListWidget->insertItem(row + 1, it);
        colorListWidget->setCurrentRow(row + 1);
        renumberItems();
    });

    QObject::connect(btnGenerateGrad, &QPushButton::clicked, &dlg, [&]() {
        QList<QColor> anchors;
        anchors.append(btnStartColor->color());
        if (chkMidColor->isChecked())
            anchors.append(btnMidColor->color());
        anchors.append(btnEndColor->color());
        populateList(interpolatePalette(anchors, spinGradSteps->value()));
    });

    QObject::connect(btnSaveCustom, &QPushButton::clicked, &dlg, [&]() {
        bool ok = false;
        QString name = QInputDialog::getText(&dlg, tr("Save Custom Palette"),
                                             tr("Custom Palette Name:"), QLineEdit::Normal,
                                             tr("My Custom Palette"), &ok)
                               .trimmed();
        if (!ok || name.isEmpty())
            return;
        QString saveError;
        if (!saveCustomPalette(name, currentColorsFromList(), &saveError)) {
            QMessageBox::warning(&dlg, tr("Save Custom Palette"), saveError);
            return;
        }
        comboPalettes->blockSignals(true);
        comboPalettes->clear();
        comboPalettes->addItems(allPaletteNames());
        comboPalettes->setCurrentText(name);
        comboPalettes->blockSignals(false);
        btnDeleteCustom->setEnabled(!builtInPaletteNames().contains(name));
    });

    QObject::connect(btnDeleteCustom, &QPushButton::clicked, &dlg, [&]() {
        const QString name = comboPalettes->currentText();
        if (builtInPaletteNames().contains(name))
            return;
        removeCustomPalette(name);
        comboPalettes->clear();
        comboPalettes->addItems(allPaletteNames());
    });

    bool appliedAny = false;
    auto applyToTarget = [&]() {
        if (!safeTarget)
            return;
        QList<QColor> cols = currentColorsFromList();
        if (!cols.isEmpty()) {
            safeTarget->applyColorPalette(cols, radioGradient->isChecked(),
                                          chkSymbols->isChecked());
            appliedAny = true;
        }
    };

    QObject::connect(btnApplyGraph, &QPushButton::clicked, &dlg, applyToTarget);
    QObject::connect(btnOk, &QPushButton::clicked, &dlg, [&]() {
        applyToTarget();
        dlg.accept();
    });
    QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    const int res = dlg.exec();
    const bool changed = (res == QDialog::Accepted) || appliedAny;
    if (changed && appliedAny && safeTarget && safeTarget->multiLayerParent()
        && !beforeSnap.isEmpty()) {
        safeTarget->multiLayerParent()->pushUndoSnapshot(tr("Modify Color Bar"), beforeSnap);
    }
    return changed;
}

// ============================================================================
// OriginPro Plot Types Support (Waterfall Y-Offset Stack & Ternary Diagram)
// ============================================================================

void Graph::applyYOffsetStack(double offsetStep, bool autoOffset)
{
    QList<QwtPlotCurve *> visibleDataCurves;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
            || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
            continue;
        QwtPlotCurve *c = curve(i);
        if (c && c->isVisible() && c->dataSize() > 0)
            visibleDataCurves.append(c);
    }

    if (visibleDataCurves.size() < 2)
        return;

    double deltaY = offsetStep;
    if (autoOffset && qAbs(offsetStep) <= 1e-15) {
        double sumSpan = 0.0;
        int validSpans = 0;
        for (QwtPlotCurve *c : visibleDataCurves) {
            const int n = c->dataSize();
            if (n <= 0)
                continue;
            double minY = c->y(0);
            double maxY = c->y(0);
            for (int j = 1; j < n; ++j) {
                const double yv = c->y(j);
                if (yv < minY)
                    minY = yv;
                if (yv > maxY)
                    maxY = yv;
            }
            sumSpan += (maxY - minY);
            validSpans++;
        }
        const double avgSpan = (validSpans > 0) ? (sumSpan / double(validSpans)) : 0.0;
        deltaY = (avgSpan > 1e-12) ? (0.8 * avgSpan) : 1.0;
    }

    for (int k = 1; k < visibleDataCurves.size(); ++k) {
        QwtPlotCurve *c = visibleDataCurves[k];
        const int n = c->dataSize();
        const double yShift = double(k) * deltaY;
        QVector<double> xVals(n);
        QVector<double> yVals(n);
        for (int j = 0; j < n; ++j) {
            xVals[j] = c->x(j);
            yVals[j] = c->y(j) + yShift;
        }
        c->setData(xVals.constData(), yVals.constData(), n);

        DataCurve *dc = dynamic_cast<DataCurve *>(c);
        if (dc) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setData(xVals.constData(), yVals.constData(), n);
            }
        }
    }

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        setToriAxisUserLocked(d_plot, axis, false);
        d_plot->setAxisAutoScale(axis);
    }
    updatePlot();

    {
        double overallMinY = 1e300;
        double overallMaxY = -1e300;
        for (QwtPlotCurve *c : visibleDataCurves) {
            const int n = c->dataSize();
            for (int j = 0; j < n; ++j) {
                const double yv = c->y(j);
                if (std::isfinite(yv)) {
                    overallMinY = qMin(overallMinY, yv);
                    overallMaxY = qMax(overallMaxY, yv);
                }
            }
        }
        const QwtScaleDiv *yDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
        const QwtScaleEngine *yEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
        if (yDiv && yEng && !Plot::isLog10ScaleEngine(yEng)
            && !yEng->testAttribute(QwtScaleEngine::Inverted)
            && !d_axis_breaks[QwtPlot::yLeft].enabled && overallMaxY > 0.0
            && overallMinY >= -0.04 * overallMaxY) {
            const double autoLo = yDiv->lowerBound();
            const double autoHi = yDiv->upperBound();
            if (autoHi > 0.0 && autoLo < -0.035 * autoHi) {
                const double clampedLo = (overallMinY >= 0.0) ? 0.0 : (-0.03 * autoHi);
                const QwtValueList majTicks = yDiv->ticks(QwtScaleDiv::MajorTick);
                const double step = (majTicks.size() >= 2)
                        ? std::fabs(majTicks[1] - majTicks[0])
                        : d_user_step[QwtPlot::yLeft];
                const int maj = d_plot->axisMaxMajor(QwtPlot::yLeft);
                const int minT = d_plot->axisMaxMinor(QwtPlot::yLeft);
                const int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
                QwtLinearScaleEngine *newEng = new QwtLinearScaleEngine();
                QwtScaleDiv newDiv = newEng->divideScale(clampedLo, autoHi, maj, maxMinInt, step);
                if (minT == 1)
                    ensureLinearHalfStepMinorTicks(newDiv);
                d_plot->setAxisScale(QwtPlot::yLeft, clampedLo, autoHi, step);
                d_plot->setAxisScaleEngine(QwtPlot::yLeft, newEng);
                d_plot->setAxisScaleDiv(QwtPlot::yLeft, newDiv);
                updateSecondaryAxis(QwtPlot::yRight);
                d_zoomer[0]->setZoomBase();
                d_plot->replot();
                syncChildLinkedLayers();
            }
        }
    }

    emit modifiedGraph();
}

namespace {

class TernaryDiagramItem : public QwtPlotItem
{
public:
    explicit TernaryDiagramItem(const Graph *graph) : d_graph(graph)
    {
        setZ(12.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 44; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph)
            return;

        const double h = std::sqrt(3.0) * 0.5;
        auto toScreen = [&](const QPointF &w) -> QPointF {
            return QPointF(xMap.xTransform(w.x()), yMap.xTransform(w.y()));
        };

        const QPointF pA = toScreen(QPointF(0.0, 0.0));
        const QPointF pB = toScreen(QPointF(1.0, 0.0));
        const QPointF pC = toScreen(QPointF(0.5, h));
        if (!std::isfinite(pA.x()) || !std::isfinite(pA.y()) || !std::isfinite(pB.x())
            || !std::isfinite(pB.y()) || !std::isfinite(pC.x()) || !std::isfinite(pC.y()))
            return;

        const double sc = Plot::printDeviceScale();
        QFont tickFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisFont(QwtPlot::xBottom));
        tickFont.setFamily(QStringLiteral("Arial"));
        tickFont.setBold(false);
        tickFont.setWeight(QFont::Normal);
        tickFont.setHintingPreference(QFont::PreferNoHinting);

        QFont vertFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisTitleFont(QwtPlot::xBottom));
        vertFont.setFamily(QStringLiteral("Arial"));
        vertFont.setBold(false);
        vertFont.setWeight(QFont::Normal);
        vertFont.setHintingPreference(QFont::PreferNoHinting);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        // 1. Interior 20% dashed grid lines
        QPen gridPen(QColor(184, 192, 204), 1.0 * sc, Qt::DashLine, Qt::FlatCap, Qt::MiterJoin);
        gridPen.setCosmetic(false);
        painter->setPen(gridPen);
        for (int step = 1; step <= 4; ++step) {
            const double f = 0.2 * double(step);
            // Constant-A line: (f, 1-f, 0) -> (f, 0, 1-f)
            painter->drawLine(toScreen(Graph::ternaryToCartesian(f, 1.0 - f, 0.0)),
                              toScreen(Graph::ternaryToCartesian(f, 0.0, 1.0 - f)));
            // Constant-B line: (1-f, f, 0) -> (0, f, 1-f)
            painter->drawLine(toScreen(Graph::ternaryToCartesian(1.0 - f, f, 0.0)),
                              toScreen(Graph::ternaryToCartesian(0.0, f, 1.0 - f)));
            // Constant-C line: (1-f, 0, f) -> (0, 1-f, f)
            painter->drawLine(toScreen(Graph::ternaryToCartesian(1.0 - f, 0.0, f)),
                              toScreen(Graph::ternaryToCartesian(0.0, 1.0 - f, f)));
        }

        // 2. Exterior ticks & thermodynamically consistent tick labels:
        //    - Base AB (left -> right): B% = 20, 40, 60, 80
        //    - Right BC (bottom -> top): C% = 20, 40, 60, 80
        //    - Left CA (bottom -> top): A% = 80, 60, 40, 20 (increases top -> bottom toward A=100% at vA!)
        const double baseLw = qMax(
                1.3,
                double(d_graph->plotWidget() ? d_graph->plotWidget()->axesLinewidth() : 2));
        QPen tickPen(QColor(Qt::black), baseLw * sc, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        tickPen.setCosmetic(false);

        const double tickLen = 6.5 * sc;
        const double padPx = 3.5 * sc;
        painter->setFont(tickFont);
        const QFontMetricsF tfm(tickFont, painter->device());
        const double tH = tfm.height();

        for (int step = 1; step <= 4; ++step) {
            const double f = 0.2 * double(step);
            const QString bText = QString::number(step * 20);
            const QString cText = QString::number(step * 20);
            const QString aText = QString::number(100 - step * 20);

            // Base AB tick (pointing down-left along constant-B or straight down)
            const QPointF basePt = toScreen(QPointF(f, 0.0));
            const QPointF baseTip(basePt.x(), basePt.y() + tickLen);
            painter->setPen(tickPen);
            painter->drawLine(basePt, baseTip);
            const double bw = tfm.horizontalAdvance(bText) + 4.0 * sc;
            painter->setPen(QColor(30, 41, 59));
            painter->drawText(QRectF(baseTip.x() - 0.5 * bw, baseTip.y() + padPx, bw, tH),
                              Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip, bText);

            // Right BC tick (outward normal (+sqrt(3)/2, -0.5) in screen coords -> (+0.866, -0.5))
            const QPointF rightPt = toScreen(Graph::ternaryToCartesian(0.0, 1.0 - f, f));
            const QPointF rightTip(rightPt.x() + 0.8660254 * tickLen,
                                   rightPt.y() - 0.5 * tickLen);
            painter->setPen(tickPen);
            painter->drawLine(rightPt, rightTip);
            const double cw = tfm.horizontalAdvance(cText) + 4.0 * sc;
            painter->setPen(QColor(30, 41, 59));
            painter->drawText(QRectF(rightTip.x() + padPx, rightTip.y() - 0.55 * tH, cw, tH),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, cText);

            // Left CA tick (outward normal (-0.866, -0.5) in screen coords)
            const QPointF leftPt = toScreen(Graph::ternaryToCartesian(1.0 - f, 0.0, f));
            const QPointF leftTip(leftPt.x() - 0.8660254 * tickLen,
                                  leftPt.y() - 0.5 * tickLen);
            painter->setPen(tickPen);
            painter->drawLine(leftPt, leftTip);
            const double aw = tfm.horizontalAdvance(aText) + 4.0 * sc;
            painter->setPen(QColor(30, 41, 59));
            painter->drawText(QRectF(leftTip.x() - padPx - aw, leftTip.y() - 0.55 * tH, aw, tH),
                              Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip, aText);
        }

        // 3. Equilateral triangle outer frame
        QPolygonF tri(3);
        tri[0] = pA;
        tri[1] = pB;
        tri[2] = pC;
        QPen borderPen(QColor(Qt::black), qMax(1.6, baseLw) * sc, Qt::SolidLine, Qt::SquareCap,
                       Qt::MiterJoin);
        borderPen.setCosmetic(false);
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(tri);

        // 4. Non-bold Arial vertex component labels with font-metrics clearance
        painter->setFont(vertFont);
        painter->setPen(QColor(Qt::black));
        const QFontMetricsF vfm(vertFont, painter->device());
        const double vH = vfm.height();

        const QString lblA = d_graph->ternaryLabel(0);
        const QString lblB = d_graph->ternaryLabel(1);
        const QString lblC = d_graph->ternaryLabel(2);

        const double wA = vfm.horizontalAdvance(lblA) + 8.0 * sc;
        const double wB = vfm.horizontalAdvance(lblB) + 8.0 * sc;
        const double wC = vfm.horizontalAdvance(lblC) + 8.0 * sc;
        const double botVertY = qMax(pA.y(), pB.y()) + tickLen + padPx + 0.35 * tH;

        const QRectF safeCanvas =
                QRectF(canvasRect).adjusted(2.0 * sc, 2.0 * sc, -2.0 * sc, -2.0 * sc);
        QRectF rectA(pA.x() - wA + 4.0 * sc, botVertY, wA, vH);
        QRectF rectB(pB.x() - 4.0 * sc, botVertY, wB, vH);
        QRectF rectC(pC.x() - 0.5 * wC, pC.y() - vH - 6.0 * sc, wC, vH);
        if (safeCanvas.width() > wA + 4.0 * sc && safeCanvas.height() > vH + 4.0 * sc) {
            if (rectA.left() < safeCanvas.left())
                rectA.moveLeft(safeCanvas.left());
            if (rectA.bottom() > safeCanvas.bottom())
                rectA.moveBottom(safeCanvas.bottom());
        }
        if (safeCanvas.width() > wB + 4.0 * sc && safeCanvas.height() > vH + 4.0 * sc) {
            if (rectB.right() > safeCanvas.right())
                rectB.moveRight(safeCanvas.right());
            if (rectB.bottom() > safeCanvas.bottom())
                rectB.moveBottom(safeCanvas.bottom());
        }
        if (safeCanvas.width() > wC + 4.0 * sc && safeCanvas.height() > vH + 4.0 * sc) {
            if (rectC.top() < safeCanvas.top())
                rectC.moveTop(safeCanvas.top());
            rectC.moveLeft(qBound(safeCanvas.left(), rectC.left(), safeCanvas.right() - wC));
        }

        painter->drawText(rectA, Qt::AlignRight | Qt::AlignTop | Qt::TextDontClip, lblA);
        painter->drawText(rectB, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, lblB);
        painter->drawText(rectC, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, lblC);

        painter->restore();
    }

private:
    const Graph *d_graph;
};

class PolarAxesItem : public QwtPlotItem
{
public:
    explicit PolarAxesItem(const Graph *graph) : d_graph(graph)
    {
        setZ(12.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 45; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph)
            return;

        const double rMax = qMax(1e-6, d_graph->polarRadiusMax());
        const double cx = xMap.xTransform(0.0);
        const double cy = yMap.xTransform(0.0);
        const double rxOuter = std::fabs(xMap.xTransform(rMax) - cx);
        const double ryOuter = std::fabs(yMap.xTransform(rMax) - cy);
        if (!std::isfinite(cx) || !std::isfinite(cy) || rxOuter < 6.0 || ryOuter < 6.0)
            return;

        const double sc = Plot::printDeviceScale();
        const double baseLw = qMax(
                1.3,
                double(d_graph->plotWidget() ? d_graph->plotWidget()->axesLinewidth() : 2));

        QFont tickFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisFont(QwtPlot::xBottom));
        tickFont.setFamily(QStringLiteral("Arial"));
        tickFont.setBold(false);
        tickFont.setWeight(QFont::Normal);
        tickFont.setHintingPreference(QFont::PreferNoHinting);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        // 1. Radial concentric rings
        QwtLinearScaleEngine rse;
        const QwtScaleDiv rDiv = rse.divideScale(0.0, rMax, 4, 0);
        const QwtValueList rMaj = rDiv.ticks(QwtScaleDiv::MajorTick);
        QVector<double> rings;
        for (double rv : rMaj) {
            if (rv > 1e-9 * rMax && rv < rMax * (1.0 - 1e-4))
                rings.append(rv);
        }
        if (rings.isEmpty()) {
            rings << (0.25 * rMax) << (0.50 * rMax) << (0.75 * rMax);
        }

        QPen innerRingPen(QColor(203, 213, 225), 1.0 * sc, Qt::DashLine);
        innerRingPen.setCosmetic(false);
        painter->setPen(innerRingPen);
        painter->setBrush(Qt::NoBrush);
        for (double rv : rings) {
            const double rx = std::fabs(xMap.xTransform(rv) - cx);
            const double ry = std::fabs(yMap.xTransform(rv) - cy);
            painter->drawEllipse(QPointF(cx, cy), rx, ry);
        }

        // 2. Angular spokes every 30 degrees (0..330)
        constexpr double kPi = 3.14159265358979323846;
        QPen spokePen(QColor(203, 213, 225), 0.95 * sc, Qt::SolidLine);
        spokePen.setCosmetic(false);
        painter->setPen(spokePen);
        for (int deg = 0; deg < 360; deg += 30) {
            const double rad = double(deg) * kPi / 180.0;
            const double ex = xMap.xTransform(rMax * std::cos(rad));
            const double ey = yMap.xTransform(rMax * std::sin(rad));
            painter->drawLine(QPointF(cx, cy), QPointF(ex, ey));
        }

        // 3. Outer polar circle
        QPen outerPen(QColor(15, 23, 42), qMax(1.5, baseLw) * sc, Qt::SolidLine);
        outerPen.setCosmetic(false);
        painter->setPen(outerPen);
        painter->drawEllipse(QPointF(cx, cy), rxOuter, ryOuter);

        // 4. Exterior degree labels (0°, 30°, ..., 330°) in non-bold Arial
        painter->setFont(tickFont);
        const QFontMetricsF fm(tickFont, painter->device());
        const double lblH = fm.height();
        const QRectF safePolarCanvas =
                QRectF(canvasRect).adjusted(2.0 * sc, 2.0 * sc, -2.0 * sc, -2.0 * sc);
        const double degPad = 5.5 * sc;
        painter->setPen(QColor(15, 23, 42));
        for (int deg = 0; deg < 360; deg += 30) {
            const double rad = double(deg) * kPi / 180.0;
            const double ux = std::cos(rad);
            const double uy = -std::sin(rad); // screen Y points downward
            const QString degStr = QString::number(deg) + QString::fromUtf8("\xC2\xB0");
            const double tw = fm.horizontalAdvance(degStr) + 4.0 * sc;
            const double anchorX = cx + (rxOuter + degPad + 0.5 * tw) * ux;
            const double anchorY = cy + (ryOuter + degPad + 0.45 * lblH) * uy;
            QRectF degRect(anchorX - 0.5 * tw, anchorY - 0.5 * lblH, tw, lblH);
            if (safePolarCanvas.width() > tw + 4.0 * sc && safePolarCanvas.height() > lblH + 4.0 * sc) {
                degRect.moveLeft(
                        qBound(safePolarCanvas.left(), degRect.left(), safePolarCanvas.right() - tw));
                degRect.moveTop(
                        qBound(safePolarCanvas.top(), degRect.top(), safePolarCanvas.bottom() - lblH));
            }
            painter->drawText(degRect, Qt::AlignCenter | Qt::TextDontClip, degStr);
        }

        // Radial tick values & pills are rendered in PolarRadialLabelsItem (z = 165.0)
        // so plotted polar curves and outer boundary ring never slice over tick digits.
        painter->restore();
    }

private:
    const Graph *d_graph;
};

class PolarRadialLabelsItem : public QwtPlotItem
{
public:
    explicit PolarRadialLabelsItem(const Graph *graph) : d_graph(graph)
    {
        setZ(165.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 46; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph || !d_graph->isPolarPlot())
            return;

        const double rMax = qMax(1e-6, d_graph->polarRadiusMax());
        const double cx = xMap.xTransform(0.0);
        const double cy = yMap.xTransform(0.0);
        const double rxOuter = std::fabs(xMap.xTransform(rMax) - cx);
        const double ryOuter = std::fabs(yMap.xTransform(rMax) - cy);
        if (!std::isfinite(cx) || !std::isfinite(cy) || rxOuter < 6.0 || ryOuter < 6.0)
            return;

        const double sc = Plot::printDeviceScale();
        QFont tickFont = d_graph->plotWidget()->displayFont(
                const_cast<Graph *>(d_graph)->axisFont(QwtPlot::xBottom));
        tickFont.setFamily(QStringLiteral("Arial"));
        tickFont.setBold(false);
        tickFont.setWeight(QFont::Normal);
        tickFont.setHintingPreference(QFont::PreferNoHinting);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setFont(tickFont);

        QwtLinearScaleEngine rse;
        const QwtScaleDiv rDiv = rse.divideScale(0.0, rMax, 4, 0);
        const QwtValueList rMaj = rDiv.ticks(QwtScaleDiv::MajorTick);
        QVector<double> rings;
        for (double rv : rMaj) {
            if (rv > 1e-9 * rMax && rv < rMax * (1.0 - 1e-4))
                rings.append(rv);
        }
        if (rings.isEmpty())
            rings << (0.25 * rMax) << (0.50 * rMax) << (0.75 * rMax);

        constexpr double kPi = 3.14159265358979323846;
        const QFontMetricsF fm(tickFont, painter->device());
        const double lblH = fm.height();
        const double degPad = 5.5 * sc;
        const QRectF safePolarCanvas =
                QRectF(canvasRect).adjusted(2.0 * sc, 2.0 * sc, -2.0 * sc, -2.0 * sc);

        auto degLabelBox = [&](int deg) -> QRectF {
            const double rad = double(deg) * kPi / 180.0;
            const double ux = std::cos(rad);
            const double uy = -std::sin(rad);
            const QString degStr = QString::number(deg) + QString::fromUtf8("\xC2\xB0");
            const double tw = fm.horizontalAdvance(degStr) + 4.0 * sc;
            const double ax = cx + (rxOuter + degPad + 0.5 * tw) * ux;
            const double ay = cy + (ryOuter + degPad + 0.45 * lblH) * uy;
            return QRectF(ax - 0.5 * tw, ay - 0.5 * lblH, tw, lblH);
        };
        const QRectF deg0Box = degLabelBox(0).adjusted(-1.5 * sc, -1.0 * sc, 1.5 * sc, 1.0 * sc);
        const QRectF deg30Box = degLabelBox(30).adjusted(-1.5 * sc, -1.0 * sc, 1.5 * sc, 1.0 * sc);

        const double rayRad = 15.0 * kPi / 180.0;
        const double cosRay = std::cos(rayRad);
        const double sinRay = std::sin(rayRad);
        QVector<double> candRings = rings;
        candRings.append(rMax);

        struct RadialPillSpec
        {
            QRectF pill;
            QString text;
        };
        QVector<RadialPillSpec> acceptedPills;
        acceptedPills.reserve(candRings.size());
        for (int ri = candRings.size() - 1; ri >= 0; --ri) {
            const double rv = candRings[ri];
            const QString rTxt = QLocale::c().toString(rv, 'g', 4);
            const double rw = fm.horizontalAdvance(rTxt) + 3.6 * sc;
            const double rh = lblH * 0.90;
            double px = xMap.xTransform(rv * cosRay);
            double py = yMap.xTransform(rv * sinRay);
            QRectF pill(px - 0.5 * rw, py - 0.5 * rh, rw, rh);

            if (ri == candRings.size() - 1) {
                const double outPx = cx + (rxOuter + 0.52 * rw + 3.2 * sc) * cosRay;
                const double outPy = cy - (ryOuter + 0.52 * rh + 3.2 * sc) * sinRay;
                const QRectF outPill(outPx - 0.5 * rw, outPy - 0.5 * rh, rw, rh);
                if (safePolarCanvas.contains(outPill) && !outPill.intersects(deg0Box)
                    && !outPill.intersects(deg30Box)) {
                    pill = outPill;
                }
            }

            const QRectF padded = pill.adjusted(-1.8 * sc, -1.0 * sc, 1.8 * sc, 1.0 * sc);
            bool overlapsAccepted = false;
            for (const RadialPillSpec &acc : acceptedPills) {
                if (padded.intersects(acc.pill)) {
                    overlapsAccepted = true;
                    break;
                }
            }
            if (!overlapsAccepted)
                acceptedPills.prepend({ pill, rTxt });
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 248));
        for (const RadialPillSpec &rp : acceptedPills)
            painter->drawRoundedRect(rp.pill, 2.0 * sc, 2.0 * sc);

        painter->setBrush(Qt::NoBrush);
        painter->setPen(QColor(15, 23, 42));
        for (const RadialPillSpec &rp : acceptedPills)
            painter->drawText(rp.pill, Qt::AlignCenter | Qt::TextDontClip, rp.text);

        painter->restore();
    }

private:
    const Graph *d_graph;
};

class CustomPosTextScaleDraw : public ScaleDraw
{
public:
    CustomPosTextScaleDraw(const QVector<double> &positions, const QStringList &labels)
        : ScaleDraw(), d_positions(positions), d_labels(labels)
    {
    }
    CustomPosTextScaleDraw(const ScaleDraw &other, const QVector<double> &positions,
                           const QStringList &labels)
        : ScaleDraw(other), d_positions(positions), d_labels(labels)
    {
    }

    QwtText label(double value) const override
    {
        if (!std::isfinite(value) || d_positions.isEmpty())
            return QwtText();
        const QwtScaleDiv sc = scaleDiv();
        const double span = std::fabs(sc.upperBound() - sc.lowerBound());
        double tol = 1e-4 * qMax(1.0, span);
        const int n = qMin(d_positions.size(), d_labels.size());
        int bestIdx = -1;
        double bestDist = 1e300;
        for (int i = 0; i < n; ++i) {
            const double d = std::fabs(value - d_positions[i]);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
            if (i > 0) {
                const double gap = std::fabs(d_positions[i] - d_positions[i - 1]);
                if (gap > 1e-12)
                    tol = qMin(tol, 0.25 * gap);
            }
        }
        if (bestIdx >= 0 && bestDist <= qMax(1e-7, tol))
            return QwtText(d_labels[bestIdx]);
        return QwtText();
    }

private:
    QVector<double> d_positions;
    QStringList d_labels;
};

class WeibullProbScaleDraw : public ScaleDraw
{
public:
    WeibullProbScaleDraw() : ScaleDraw() { }
    explicit WeibullProbScaleDraw(const ScaleDraw &other) : ScaleDraw(other) { }

    QwtText label(double value) const override
    {
        if (!std::isfinite(value) || value < -15.0 || value > 4.0)
            return QwtText();

        struct CanonicalWeibullTick {
            double pct;
            const char *txt;
        };
        static const CanonicalWeibullTick kMaj[] = {
            { 0.1, "0.1" },   { 1.0, "1" },     { 5.0, "5" },
            { 10.0, "10" },   { 20.0, "20" },   { 63.21205588285577, "63.2" },
            { 90.0, "90" },   { 99.0, "99" },   { 99.9, "99.9" }
        };
        for (const auto & item : kMaj) {
            const double yRef = std::log(-std::log(1.0 - item.pct / 100.0));
            if (std::fabs(value - yRef) <= 2.5e-3)
                return QwtText(QString::fromLatin1(item.txt));
        }
        if (std::fabs(value - std::log(-std::log(1.0 - 0.632))) <= 2.5e-3)
            return QwtText(QStringLiteral("63.2"));

        const double p = 100.0 * (1.0 - std::exp(-std::exp(value)));
        if (!(p > 0.0 && p < 100.0) || !std::isfinite(p))
            return QwtText();
        return QwtText(QLocale::c().toString(p, 'g', 3));
    }
};

} // namespace

QPointF Graph::ternaryToCartesian(double a, double b, double c)
{
    const double sum = a + b + c;
    const double h = std::sqrt(3.0) * 0.5;
    if (qAbs(sum) <= 1e-15)
        return QPointF(0.5, h / 3.0);

    const double bn = b / sum;
    const double cn = c / sum;
    const double x = bn + 0.5 * cn;
    const double y = h * cn;
    return QPointF(x, y);
}

void Graph::setupTernaryPlot(const QString &labelA, const QString &labelB, const QString &labelC)
{
    d_is_ternary = true;
    d_is_polar = false;
    d_ternary_labels[0] = labelA.isEmpty() ? QString("A") : labelA;
    d_ternary_labels[1] = labelB.isEmpty() ? QString("B") : labelB;
    d_ternary_labels[2] = labelC.isEmpty() ? QString("C") : labelC;
    m_autoscale = false;

    if (d_polar_item) {
        d_polar_item->detach();
        delete d_polar_item;
        d_polar_item = nullptr;
    }
    if (d_plot) {
        const QwtPlotItemList items = d_plot->itemList();
        for (QwtPlotItem *it : items) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotUserItem + 46) {
                it->detach();
                delete it;
            }
        }
    }

    if (grid()) {
        grid()->enableX(false);
        grid()->enableXMin(false);
        grid()->enableY(false);
        grid()->enableYMin(false);
    }
    drawCanvasFrame(false, 0);
    drawAxesBackbones(false);

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QString plainT = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
        if (isAxisPlaceholderTitle(plainT)) {
            QwtText emptyT = d_plot->axisTitle(a);
            emptyT.setText(QString());
            d_plot->setAxisTitle(a, emptyT);
        }
    }

    // Equal 1.48 x 1.48 world span guarantees a strictly 60-degree equilateral triangle on 1:1 canvases
    // with generous outer margin for long component titles (e.g. "Al (at.%)", "Zn (at.%)").
    setScale(QwtPlot::xBottom, -0.24, 1.24, 0.2, 6, 0, 0, false);
    setScale(QwtPlot::yLeft, -0.25, 1.23, 0.2, 6, 0, 0, false);
    for (int i = 0; i < QwtPlot::axisCnt; ++i)
        enableAxis(i, false);

    if (!d_ternary_item) {
        d_ternary_item = new TernaryDiagramItem(this);
        d_ternary_item->attach(d_plot);
    }

    if (hasLegend()) {
        if (n_curves == 1) {
            if (Legend *leg = legend()) {
                const QString lt = leg->text().trimmed();
                if (lt.contains(QLatin1String("Ternary_Y"))
                    || lt.endsWith(QLatin1String("}Y"))) {
                    setLegendVisible(false);
                }
            }
        }
        if (isLegendVisible())
            placeLegendAnchor(LegendTopRight);
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setCategoryLabels(int axis, const QStringList &labels)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_custom_category_labels[axis] = labels;
    d_custom_tick_positions[axis].clear();
    d_custom_tick_labels[axis].clear();
    d_custom_tick_rotation[axis] = 0.0;
    d_weibull_prob_axis[axis] = false;
    if (labels.isEmpty())
        return;

    QMap<int, QString> map;
    for (int i = 0; i < labels.size(); ++i)
        map.insert(i + 1, labels[i]);

    axisType[axis] = AxisType::Txt;
    const ScaleDraw *prevSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(axis));
    QwtTextScaleDraw *tsd =
            prevSd ? new QwtTextScaleDraw(*prevSd, map) : new QwtTextScaleDraw(map);
    tsd->enableComponent(QwtAbstractScaleDraw::Labels, true);
    tsd->setMinorTicksStyle(ScaleDraw::None);
    d_plot->setAxisScaleDraw(axis, tsd);

    double cLo = 0.5;
    double cHi = double(labels.size()) + 0.5;
    if (isToriAxisUserLocked(d_plot, axis)) {
        if (const QwtScaleDiv *curDiv = d_plot->axisScaleDiv(axis)) {
            const double curLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
            const double curHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
            if (std::isfinite(curLo) && std::isfinite(curHi) && curHi > curLo
                && curLo >= -1.0 && curLo <= 1.0
                && curHi >= double(labels.size()) && curHi <= double(labels.size()) + 4.5) {
                cLo = qMin(cLo, curLo);
                cHi = qMax(cHi, curHi);
            }
        }
    }
    const bool isInv = d_plot->axisScaleEngine(axis)
            && d_plot->axisScaleEngine(axis)->testAttribute(QwtScaleEngine::Inverted);
    QwtValueList catMaj;
    for (int i = 1; i <= labels.size(); ++i)
        catMaj += double(i);
    QwtValueList emptyTicks;
    QwtValueList catTicks[QwtScaleDiv::NTickTypes];
    catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
    catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
    catTicks[QwtScaleDiv::MajorTick] = catMaj;
    QwtScaleEngine *catEng = new QwtLinearScaleEngine();
    QwtScaleDiv catDiv(cLo, cHi, catTicks);
    if (isInv) {
        catEng->setAttribute(QwtScaleEngine::Inverted);
        catDiv.invert();
    }
    d_user_step[axis] = 1.0;
    d_plot->setAxisMaxMinor(axis, 0);
    d_plot->setMinorTicksType(axis, ScaleDraw::None);
    d_plot->setAxisMaxMajor(axis, qMax(8, labels.size() + 2));
    d_plot->setAxisScale(axis, isInv ? cHi : cLo, isInv ? cLo : cHi, 1.0);
    d_plot->setAxisScaleEngine(axis, catEng);
    d_plot->setAxisScaleDiv(axis, catDiv);

    const int opp = (axis == QwtPlot::xBottom) ? QwtPlot::xTop
            : (axis == QwtPlot::yLeft)         ? QwtPlot::yRight
                                               : -1;
    if (opp >= 0 && !d_plot->hasVisibleItemOnAxis(opp) && !hasColorScaleOnAxis(opp)
        && !d_weibull_prob_axis[opp] && d_custom_tick_positions[opp].isEmpty()) {
        if (QwtScaleDraw *oppSd = d_plot->axisScaleDraw(opp))
            oppSd->enableComponent(QwtAbstractScaleDraw::Labels, false);
        d_plot->setMinorTicksType(opp, ScaleDraw::None);
        d_plot->setMajorTicksType(opp, ScaleDraw::None);
    }

    if (axis == QwtPlot::yLeft && n_curves > 0) {
        bool allHorizBars = true;
        for (int bi = 0; bi < n_curves; ++bi) {
            if (bi < c_type.size() && c_type[bi] != HorizontalBars && c_type[bi] != ErrorBars) {
                allHorizBars = false;
                break;
            }
        }
        if (allHorizBars
            && (axisType[QwtPlot::xBottom] == AxisType::Txt
                || dynamic_cast<QwtTextScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::xBottom)))) {
            d_custom_category_labels[QwtPlot::xBottom].clear();
            axesFormatInfo[QwtPlot::xBottom].clear();
            setLabelsNumericFormat(QwtPlot::xBottom, Plot::Automatic, 4);
            updateSecondaryAxis(QwtPlot::xTop);
        }
    }

    const QString curPlain = unwrapNoWrapAxisTitle(axisTitle(axis)).trimmed();
    if (isAxisPlaceholderTitle(curPlain)) {
        QwtText emptyT = d_plot->axisTitle(axis);
        emptyT.setText(QString());
        d_plot->setAxisTitle(axis, emptyT);
    }

    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

QStringList Graph::categoryLabels(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QStringList();
    if (!d_custom_tick_labels[axis].isEmpty())
        return d_custom_tick_labels[axis];
    if (!d_custom_category_labels[axis].isEmpty())
        return d_custom_category_labels[axis];
    if (d_plot) {
        if (const QwtTextScaleDraw *tsd =
                    dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(axis))) {
            return tsd->labelsList();
        }
    }
    return QStringList();
}

void Graph::setCustomTickLabels(int axis, const QVector<double> &positions,
                                const QStringList &labels, double rotationDeg)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    const int m = qMin(positions.size(), labels.size());
    d_custom_category_labels[axis].clear();
    d_weibull_prob_axis[axis] = false;
    d_custom_tick_positions[axis].clear();
    d_custom_tick_labels[axis].clear();
    d_custom_tick_rotation[axis] = std::isfinite(rotationDeg) ? rotationDeg : 0.0;

    for (int i = 0; i < m; ++i) {
        if (std::isfinite(positions[i])) {
            d_custom_tick_positions[axis].append(positions[i]);
            d_custom_tick_labels[axis].append(labels[i]);
        }
    }
    if (d_custom_tick_positions[axis].isEmpty())
        return;

    axisType[axis] = AxisType::Numeric;
    if (std::fabs(d_custom_tick_rotation[axis]) > 1e-6)
        setAxisLabelRotation(axis, qRound(d_custom_tick_rotation[axis]));
    else if (labelsRotation(axis) != 0)
        setAxisLabelRotation(axis, 0);

    syncCustomAndWeibullAxes();

    const QString curPlain = unwrapNoWrapAxisTitle(axisTitle(axis)).trimmed();
    if (isAxisPlaceholderTitle(curPlain)) {
        QwtText emptyT = d_plot->axisTitle(axis);
        emptyT.setText(QString());
        d_plot->setAxisTitle(axis, emptyT);
    }

    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setWeibullProbabilityAxis(int axis, bool enabled)
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_weibull_prob_axis[axis] = enabled;
    if (!enabled) {
        setLabelsNumericFormat(axis, Plot::Automatic, 4, axesFormulas[axis]);
        d_plot->updateLayout();
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    d_custom_category_labels[axis].clear();
    d_custom_tick_positions[axis].clear();
    d_custom_tick_labels[axis].clear();

    enableAxis(axis, true);
    setAxisBackboneVisible(axis, true);
    d_plot->setMajorTicksType(axis, ScaleDraw::In);
    d_plot->setMinorTicksType(axis, ScaleDraw::In);

    QFont nf = d_plot->axisFont(axis == QwtPlot::yRight ? QwtPlot::yLeft : axis);
    nf.setFamily(QStringLiteral("Arial"));
    nf.setBold(false);
    nf.setWeight(QFont::Normal);
    nf.setHintingPreference(QFont::PreferNoHinting);
    d_plot->setAxisFont(axis, nf);

    syncCustomAndWeibullAxes();

    if (axis == QwtPlot::yRight) {
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight))
            sw->setSpacing(6);
    }

    d_plot->updateLayout();
    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::hasWeibullProbabilityAxis(int axis) const
{
    return (axis >= 0 && axis < 4) ? d_weibull_prob_axis[axis] : false;
}

void Graph::syncCustomAndWeibullAxes()
{
    if (!d_plot || d_is_ternary || d_is_polar || isPoleFigurePlot() || isPiePlot())
        return;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        if (!d_custom_tick_positions[a].isEmpty()
            && d_custom_tick_positions[a].size() == d_custom_tick_labels[a].size()) {
            const QVector<double> &pos = d_custom_tick_positions[a];
            const QStringList &lbls = d_custom_tick_labels[a];
            double minPos = pos.first();
            double maxPos = pos.first();
            QwtValueList maj;
            for (double pv : pos) {
                minPos = qMin(minPos, pv);
                maxPos = qMax(maxPos, pv);
                maj += pv;
            }

            const QwtScaleDiv *curDiv = d_plot->axisScaleDiv(a);
            const bool isInv = d_plot->axisScaleEngine(a)
                    && d_plot->axisScaleEngine(a)->testAttribute(QwtScaleEngine::Inverted);
            double cLo = minPos;
            double cHi = maxPos;
            if (curDiv && isToriAxisUserLocked(d_plot, a)) {
                const double dLo = qMin(curDiv->lowerBound(), curDiv->upperBound());
                const double dHi = qMax(curDiv->lowerBound(), curDiv->upperBound());
                if (std::isfinite(dLo) && std::isfinite(dHi) && dHi > dLo
                    && dLo <= minPos + 1e-6 && dHi >= maxPos - 1e-6) {
                    cLo = dLo;
                    cHi = dHi;
                }
            }
            if (!(cHi > cLo)) {
                cLo = minPos - 0.5;
                cHi = maxPos + 0.5;
            } else if (!isToriAxisUserLocked(d_plot, a) && pos.size() >= 2
                       && std::fabs(minPos - 1.0) <= 1e-6
                       && std::fabs(maxPos - double(pos.size())) <= 1e-6) {
                cLo = 0.5;
                cHi = double(pos.size()) + 0.5;
            }

            if (!dynamic_cast<CustomPosTextScaleDraw *>(d_plot->axisScaleDraw(a))) {
                const ScaleDraw *prevSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(a));
                CustomPosTextScaleDraw *csd = prevSd
                        ? new CustomPosTextScaleDraw(*prevSd, pos, lbls)
                        : new CustomPosTextScaleDraw(pos, lbls);
                csd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                csd->setMinorTicksStyle(ScaleDraw::None);
                d_plot->setAxisScaleDraw(a, csd);
            } else if (QwtScaleDraw *sd = d_plot->axisScaleDraw(a)) {
                sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
            }
            if (std::fabs(d_custom_tick_rotation[a]) > 1e-6)
                setAxisLabelRotation(a, qRound(d_custom_tick_rotation[a]));

            QwtValueList emptyTicks;
            QwtValueList ticks[QwtScaleDiv::NTickTypes];
            ticks[QwtScaleDiv::MinorTick] = emptyTicks;
            ticks[QwtScaleDiv::MediumTick] = emptyTicks;
            ticks[QwtScaleDiv::MajorTick] = maj;
            QwtScaleEngine *eng = new QwtLinearScaleEngine();
            QwtScaleDiv div(cLo, cHi, ticks);
            if (isInv) {
                eng->setAttribute(QwtScaleEngine::Inverted);
                div.invert();
            }
            setToriAxisUserLocked(d_plot, a, true);
            d_plot->setAxisMaxMinor(a, 0);
            d_plot->setMinorTicksType(a, ScaleDraw::None);
            d_plot->setAxisMaxMajor(a, qMax(8, pos.size() + 2));
            d_plot->setAxisScaleEngine(a, eng);
            d_plot->setAxisScaleDiv(a, div);

            const int opp = (a == QwtPlot::yLeft) ? QwtPlot::yRight
                    : (a == QwtPlot::xBottom)     ? QwtPlot::xTop
                                                  : -1;
            if (opp >= 0 && d_plot->axisEnabled(opp) && !d_plot->hasVisibleItemOnAxis(opp)
                && !hasColorScaleOnAxis(opp) && !d_weibull_prob_axis[opp]
                && d_custom_tick_positions[opp].isEmpty()
                && d_custom_category_labels[opp].isEmpty()) {
                QwtScaleEngine *oppEng = new QwtLinearScaleEngine();
                if (isInv)
                    oppEng->setAttribute(QwtScaleEngine::Inverted);
                d_plot->setAxisMaxMinor(opp, 0);
                d_plot->setMinorTicksType(opp, ScaleDraw::None);
                d_plot->setAxisScaleEngine(opp, oppEng);
                d_plot->setAxisScaleDiv(opp, div);
            }
            continue;
        }

        if (d_weibull_prob_axis[a]) {
            const Graph *srcG = linkedParentLayer() ? linkedParentLayer() : this;
            const QwtPlot *srcPlot = (srcG && srcG->plotWidget()) ? srcG->plotWidget() : d_plot;
            const int refAxis = (a == QwtPlot::yRight) ? QwtPlot::yLeft
                    : (a == QwtPlot::xTop)             ? QwtPlot::xBottom
                                                       : a;
            const QwtScaleDiv *refDiv = srcPlot->axisScaleDiv(refAxis);
            double lo = refDiv ? qMin(refDiv->lowerBound(), refDiv->upperBound()) : -5.0;
            double hi = refDiv ? qMax(refDiv->lowerBound(), refDiv->upperBound()) : 2.0;
            if (!std::isfinite(lo) || !std::isfinite(hi) || !(hi > lo)
                || (srcG != this && lo == 0.0 && hi == 1000.0)) {
                lo = -5.0;
                hi = 2.0;
            }
            const bool isInv = srcPlot->axisScaleEngine(refAxis)
                    && srcPlot->axisScaleEngine(refAxis)->testAttribute(QwtScaleEngine::Inverted);

            if (!dynamic_cast<WeibullProbScaleDraw *>(d_plot->axisScaleDraw(a))) {
                const ScaleDraw *prevSd = dynamic_cast<const ScaleDraw *>(d_plot->axisScaleDraw(a));
                WeibullProbScaleDraw *wsd =
                        prevSd ? new WeibullProbScaleDraw(*prevSd) : new WeibullProbScaleDraw();
                wsd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                wsd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
                wsd->enableComponent(QwtAbstractScaleDraw::Backbone, true);
                wsd->setMajorTicksStyle(ScaleDraw::In);
                wsd->setMinorTicksStyle(ScaleDraw::In);
                d_plot->setAxisScaleDraw(a, wsd);
            } else if (ScaleDraw *wsd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(a))) {
                wsd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                wsd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
            }

            static const double kMajPcts[] = {
                0.1, 1.0, 5.0, 10.0, 20.0, 63.21205588285577, 90.0, 99.0
            };
            static const double kMinPcts[] = {
                0.2, 0.5, 2.0, 3.0, 15.0, 30.0, 40.0, 50.0, 70.0, 80.0, 95.0, 98.0
            };
            const double eps = 1e-6 * (hi - lo);
            QwtValueList majTicks, minTicks, emptyMed;
            for (double p : kMajPcts) {
                const double yv = std::log(-std::log(1.0 - p / 100.0));
                if (yv >= lo - eps && yv <= hi + eps)
                    majTicks += yv;
            }
            for (double p : kMinPcts) {
                const double yv = std::log(-std::log(1.0 - p / 100.0));
                if (yv >= lo - eps && yv <= hi + eps)
                    minTicks += yv;
            }

            QwtValueList wTicks[QwtScaleDiv::NTickTypes];
            wTicks[QwtScaleDiv::MinorTick] = minTicks;
            wTicks[QwtScaleDiv::MediumTick] = emptyMed;
            wTicks[QwtScaleDiv::MajorTick] = majTicks;
            QwtScaleEngine *wEng = new QwtLinearScaleEngine();
            QwtScaleDiv wDiv(lo, hi, wTicks);
            if (isInv) {
                wEng->setAttribute(QwtScaleEngine::Inverted);
                wDiv.invert();
            }
            enableAxis(a, true);
            d_plot->setMajorTicksType(a, ScaleDraw::In);
            d_plot->setMinorTicksType(a, ScaleDraw::In);
            d_plot->setAxisMaxMajor(a, qMax(8, majTicks.size() + 2));
            d_plot->setAxisMaxMinor(a, 2);
            d_plot->setAxisScaleEngine(a, wEng);
            d_plot->setAxisScaleDiv(a, wDiv);
        }
    }
}

void Graph::setupPolarPlot(const QVector<double> &thetaDeg, const QVector<double> &radius,
                           const QString &seriesLabel, bool thetaInRadians)
{
    const bool alreadyPolar = d_is_polar;
    d_is_polar = true;
    d_is_ternary = false;
    m_autoscale = false;

    if (d_ternary_item) {
        d_ternary_item->detach();
        delete d_ternary_item;
        d_ternary_item = nullptr;
    }

    if (grid()) {
        grid()->enableX(false);
        grid()->enableXMin(false);
        grid()->enableY(false);
        grid()->enableYMin(false);
    }
    drawCanvasFrame(false, 0);
    drawAxesBackbones(false);

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QString plainT = unwrapNoWrapAxisTitle(axisTitle(a)).trimmed();
        if (isAxisPlaceholderTitle(plainT)) {
            QwtText emptyT = d_plot->axisTitle(a);
            emptyT.setText(QString());
            d_plot->setAxisTitle(a, emptyT);
        }
    }

    const int n = qMin(thetaDeg.size(), radius.size());
    QVector<double> pxVec, pyVec;
    pxVec.reserve(n);
    pyVec.reserve(n);
    double maxR = alreadyPolar ? d_polar_rmax : 0.0;
    constexpr double kPi = 3.14159265358979323846;

    for (int i = 0; i < n; ++i) {
        const double th = thetaDeg[i];
        const double rv = radius[i];
        if (!std::isfinite(th) || !std::isfinite(rv))
            continue;
        const double rad = thetaInRadians ? th : (th * kPi / 180.0);
        pxVec.append(rv * std::cos(rad));
        pyVec.append(rv * std::sin(rad));
        maxR = qMax(maxR, std::fabs(rv));
    }

    if (maxR > 0.0) {
        double rLo = 0.0;
        double rHi = maxR;
        double rStep = 0.0;
        QwtLinearScaleEngine rse;
        rse.autoScale(4, rLo, rHi, rStep);
        d_polar_rmax = (rHi >= maxR && std::isfinite(rHi)) ? rHi : maxR;
    } else if (!(d_polar_rmax > 0.0) || !std::isfinite(d_polar_rmax)) {
        d_polar_rmax = 1.0;
    }

    if (!d_polar_item) {
        d_polar_item = new PolarAxesItem(this);
        d_polar_item->attach(d_plot);
    }
    if (d_plot) {
        bool hasFgLabels = false;
        for (QwtPlotItem *it : d_plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotUserItem + 46) {
                hasFgLabels = true;
                break;
            }
        }
        if (!hasFgLabels) {
            PolarRadialLabelsItem *fgLbls = new PolarRadialLabelsItem(this);
            fgLbls->attach(d_plot);
        }
    }

    if (!pxVec.isEmpty()) {
        const QList<QColor> polarPalette = paletteColors(defaultCurvePaletteName());
        const int sIdx = d_polar_series.size();
        const QColor col = polarPalette.isEmpty()
                ? QColor(Qt::black)
                : polarPalette.at(sIdx % polarPalette.size());
        const QString lbl = seriesLabel.trimmed().isEmpty()
                ? tr("Polar %1").arg(sIdx + 1)
                : seriesLabel.trimmed();

        PlotCurve *pc = new PlotCurve(lbl);
        pc->setType(Function);
        pc->setData(pxVec.constData(), pyVec.constData(), pxVec.size());
        QPen p(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setCosmetic(false);
        pc->setPen(p);

        c_type.resize(++n_curves);
        c_type[n_curves - 1] = Function;
        c_keys.resize(n_curves);
        c_keys[n_curves - 1] = d_plot->insertCurve(pc);
        syncCurveZOrder();

        PolarSeriesSpec ps;
        ps.x = pxVec;
        ps.y = pyVec;
        ps.label = lbl;
        ps.color = col;
        ps.width = 2.0;
        d_polar_series.append(ps);

        addLegendItem(lbl);
    }

    const double rMax = qMax(1e-6, d_polar_rmax);
    const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
    const int extraRows = qMax(0, nPolarRows - 1);
    const double yLo = -1.44 * rMax;
    const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
    const double halfX = 0.5 * (yHi - yLo);
    setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
    setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
    for (int i = 0; i < QwtPlot::axisCnt; ++i)
        enableAxis(i, false);

    if (hasLegend())
        placeLegendAnchor(LegendTopRight);

    d_plot->replot();
    emit modifiedGraph();
}

// ============================================================================
// OriginPro Frame Presets, Per-Side Axis Backbones, Reference Lines & Linked Axes
// ============================================================================

namespace {
QString localizedStatModeLabel(const QString &rawStatMode)
{
    const QString trimmed = rawStatMode.trimmed();
    QString norm = trimmed.toLower();
    norm.remove(' ');
    if (norm == "mean")
        return Graph::tr("Mean");
    if (norm == "median")
        return Graph::tr("Median");
    if (norm == "mean+sd" || norm == "+sd")
        return Graph::tr("+SD");
    if (norm == "mean-sd" || norm == "-sd")
        return Graph::tr("-SD");
    if (norm == "min")
        return Graph::tr("Min");
    if (norm == "max")
        return Graph::tr("Max");
    return trimmed;
}

class OriginReferenceFillItem : public QwtPlotItem
{
public:
    explicit OriginReferenceFillItem(const Graph *graph) : d_graph(graph)
    {
        setZ(14.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 41; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph)
            return;
        const QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
        if (lines.isEmpty())
            return;

        const bool logX = xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10;
        const bool logY = yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10;

        painter->save();
        for (const Graph::ReferenceLineSpec &spec : lines) {
            if (!spec.visible || !spec.fillToAxis || !std::isfinite(spec.value))
                continue;
            if (spec.orientation == 0) {
                if (logY && spec.value <= 0.0)
                    continue;
                const double yPx = yMap.xTransform(spec.value);
                const double yLo = qMin(yMap.s1(), yMap.s2());
                const double yHi = qMax(yMap.s1(), yMap.s2());
                const double basePx = (!logY && 0.0 >= yLo && 0.0 <= yHi)
                        ? yMap.xTransform(0.0)
                        : double(canvasRect.bottom());
                if (!std::isfinite(yPx) || !std::isfinite(basePx))
                    continue;
                QRectF fillRect(canvasRect.left(), qMin(yPx, basePx), canvasRect.width(),
                                qAbs(basePx - yPx));
                fillRect = fillRect.intersected(QRectF(canvasRect));
                if (!fillRect.isEmpty())
                    painter->fillRect(fillRect, spec.fillColor);
            } else {
                if (logX && spec.value <= 0.0)
                    continue;
                const double xPx = xMap.xTransform(spec.value);
                const double xLo = qMin(xMap.s1(), xMap.s2());
                const double xHi = qMax(xMap.s1(), xMap.s2());
                const double basePx = (!logX && 0.0 >= xLo && 0.0 <= xHi)
                        ? xMap.xTransform(0.0)
                        : double(canvasRect.left());
                if (!std::isfinite(xPx) || !std::isfinite(basePx))
                    continue;
                QRectF fillRect(qMin(xPx, basePx), canvasRect.top(), qAbs(basePx - xPx),
                                canvasRect.height());
                fillRect = fillRect.intersected(QRectF(canvasRect));
                if (!fillRect.isEmpty())
                    painter->fillRect(fillRect, spec.fillColor);
            }
        }
        painter->restore();
    }

private:
    const Graph *d_graph;
};

class OriginReferenceLineItem : public QwtPlotItem
{
public:
    explicit OriginReferenceLineItem(const Graph *graph) : d_graph(graph)
    {
        setZ(18.0);
        setItemAttribute(QwtPlotItem::AutoScale, false);
        setItemAttribute(QwtPlotItem::Legend, false);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 42; }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &canvasRect) const override
    {
        if (!painter || !d_graph)
            return;
        const QVector<Graph::ReferenceLineSpec> lines = d_graph->referenceLines();
        if (lines.isEmpty())
            return;

        const QwtMetricsMap savedMetricsMap = QwtPainter::metricsMap();
        struct ScopedResetMetricsMap {
            QwtMetricsMap prev;
            ~ScopedResetMetricsMap() { QwtPainter::setMetricsMap(prev); }
        } metricsGuard{ savedMetricsMap };
        QwtPainter::resetMetricsMap();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing,
                               const_cast<Graph *>(d_graph)->antialiasing());

        const QFont axNumFnt = d_graph->axisNumbersFont(QwtPlot::yLeft);
        const double baseAxPt = axNumFnt.pointSizeF() > 0.0
                ? axNumFnt.pointSizeF()
                : (axNumFnt.pointSize() > 0 ? double(axNumFnt.pointSize()) : 10.0);
        const double refPt = qMax(8.0, baseAxPt - 1.0);
        QFont labelFont(QStringLiteral("Arial"));
        labelFont.setPointSizeF(refPt);
        labelFont.setWeight(QFont::Normal);
        labelFont.setBold(false);
        labelFont.setHintingPreference(QFont::PreferNoHinting);
        const QFontMetricsF fm(labelFont, painter->device());

        const Plot *pl = d_graph->plotWidget();
        const int cw = (pl && pl->canvas() && pl->canvas()->width() > 0)
                ? pl->canvas()->width()
                : qMax(1, canvasRect.width());
        const int ch = (pl && pl->canvas() && pl->canvas()->height() > 0)
                ? pl->canvas()->height()
                : qMax(1, canvasRect.height());
        const double sx = double(qMax(1, canvasRect.width())) / double(qMax(1, cw));
        const double sy = double(qMax(1, canvasRect.height())) / double(qMax(1, ch));

        double richScaleX = 1.0;
        double richScaleY = 1.0;
        if (painter->device() && pl) {
            const int devDpiX = painter->device()->logicalDpiX();
            const int devDpiY = painter->device()->logicalDpiY();
            const int scrDpiX = pl->logicalDpiX();
            const int scrDpiY = pl->logicalDpiY();
            if (devDpiX > 0 && scrDpiX > 0)
                richScaleX = double(devDpiX) / double(scrDpiX);
            if (devDpiY > 0 && scrDpiY > 0)
                richScaleY = double(devDpiY) / double(scrDpiY);
        }

        const int majTickLen = pl ? qMax(0, pl->majorTickLength()) : 6;
        const double tickInsetX = (double(majTickLen) + 6.0) * sx;
        const double tickInsetY = (double(majTickLen) + 6.0) * sy;

        auto mapWidgetCanvasRect = [&](const QRect &wr) -> QRectF {
            if (wr.isEmpty())
                return QRectF();
            return QRectF(double(canvasRect.left()) + double(wr.left()) * sx,
                          double(canvasRect.top()) + double(wr.top()) * sy,
                          double(wr.width()) * sx, double(wr.height()) * sy);
        };

        QVector<QPointF> curvePts;
        if (pl) {
            for (int ci = 0; ci < d_graph->curves(); ++ci) {
                const QwtPlotCurve *cv = d_graph->curve(ci);
                if (!cv || !cv->isVisible() || cv->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
                    continue;
                const DataCurve *dc = dynamic_cast<const DataCurve *>(cv);
                const bool useDirectX = (cv->xAxis() == QwtPlot::xBottom);
                const bool useDirectY = (cv->yAxis() == QwtPlot::yLeft);
                const QwtScaleMap cxMap = useDirectX ? xMap : pl->canvasMap(cv->xAxis());
                const QwtScaleMap cyMap = useDirectY ? yMap : pl->canvasMap(cv->yAxis());
                const int n = cv->dataSize();
                const int stride = qMax(1, n / 300);
                Column *dcXCol = nullptr;
                Column *dcYCol = nullptr;
                const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
                if (dcHasMasks)
                    dc->resolveColumns(dcXCol, dcYCol);
                for (int k = 0; k < n; k += stride) {
                    if (dcHasMasks && dc->isPointMaskedFast(k, dcXCol, dcYCol))
                        continue;
                    const double rawPx = cxMap.xTransform(cv->x(k));
                    const double rawPy = cyMap.xTransform(cv->y(k));
                    if (!std::isfinite(rawPx) || !std::isfinite(rawPy))
                        continue;
                    const double px = useDirectX ? rawPx
                                                 : (double(canvasRect.left()) + rawPx * sx);
                    const double py = useDirectY ? rawPy
                                                 : (double(canvasRect.top()) + rawPy * sy);
                    if (std::isfinite(px) && std::isfinite(py))
                        curvePts.append(QPointF(px, py));
                }
            }
        }

        QVector<QRectF> avoidBoxes;
        QVector<QRectF> textMaskBoxes;
        ::Legend *mainLeg = const_cast<Graph *>(d_graph)->legend();
        if (d_graph->isLegendVisible() && mainLeg && !mainLeg->text().trimmed().isEmpty()) {
            const QRectF lb = mapWidgetCanvasRect(mainLeg->rect());
            if (!lb.isEmpty()) {
                avoidBoxes.append(lb);
                textMaskBoxes.append(lb.adjusted(-2.0 * sx, -1.5 * sy, 2.0 * sx, 1.5 * sy));
            }
        }
        const QVector<int> txtKeys = const_cast<Graph *>(d_graph)->textMarkerKeys();
        for (int tid : txtKeys) {
            ::Legend *tm = const_cast<Graph *>(d_graph)->textMarker(tid);
            if (!tm || !tm->isVisible() || tm == mainLeg || tm->text().trimmed().isEmpty())
                continue;
            const QRectF tb = mapWidgetCanvasRect(tm->rect());
            if (!tb.isEmpty()) {
                avoidBoxes.append(tb);
                textMaskBoxes.append(tb.adjusted(-2.0 * sx, -1.5 * sy, 2.0 * sx, 1.5 * sy));
            }
        }
        const QVector<int> lineKeys = const_cast<Graph *>(d_graph)->lineMarkerKeys();
        for (int lid : lineKeys) {
            ArrowMarker *am = const_cast<Graph *>(d_graph)->arrow(lid);
            if (!am || !am->isVisible())
                continue;
            const double ax0 = xMap.xTransform(am->startPointCoord().x());
            const double ay0 = yMap.xTransform(am->startPointCoord().y());
            const double ax1 = xMap.xTransform(am->endPointCoord().x());
            const double ay1 = yMap.xTransform(am->endPointCoord().y());
            if (std::isfinite(ax0) && std::isfinite(ay0) && std::isfinite(ax1)
                && std::isfinite(ay1)) {
                avoidBoxes.append(QRectF(QPointF(qMin(ax0, ax1) - 5.0 * sx, qMin(ay0, ay1) - 5.0 * sy),
                                         QPointF(qMax(ax0, ax1) + 5.0 * sx, qMax(ay0, ay1) + 5.0 * sy)));
            }
        }
        if (MultiLayer *ml = d_graph->multiLayerParent()) {
            const QPoint myCanvasOrigin =
                    d_graph->pos() + (pl ? pl->plotLayout()->canvasRect().topLeft() : QPoint(0, 0));
            const QList<Graph *> allGraphs = ml->layersList();
            for (Graph *ins : allGraphs) {
                if (!ins || ins == d_graph || ins->isHidden() || !ml->isInsetLayer(ins)
                    || ml->insetSourceLayer(ins) != d_graph)
                    continue;
                const QSize insSz = ins->plotWidget() ? ins->plotWidget()->size() : ins->size();
                const QRect insLocal(ins->pos() - myCanvasOrigin, insSz);
                const QRectF ib = mapWidgetCanvasRect(insLocal);
                if (!ib.isEmpty())
                    avoidBoxes.append(ib);
            }
        }

        auto scorePill = [&](const QRectF &pill) -> int {
            int hits = 0;
            const QRectF testRc = pill.adjusted(-5.0 * sx, -4.0 * sy, 5.0 * sx, 4.0 * sy);
            for (const QRectF &ab : avoidBoxes) {
                if (!ab.isEmpty() && ab.intersects(testRc))
                    hits += 1000;
            }
            for (const QPointF &pt : curvePts) {
                if (testRc.contains(pt))
                    ++hits;
            }
            return hits;
        };

        static const QRegularExpression kRichTagRx(
                QStringLiteral("<\\s*(?:i|sub|sup|b|u|span|font|br)\\b"),
                QRegularExpression::CaseInsensitiveOption);
        const bool logX = xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10;
        const bool logY = yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10;

        for (const Graph::ReferenceLineSpec &spec : lines) {
            if (!spec.visible || !std::isfinite(spec.value))
                continue;

            QString displayLabel = spec.label.trimmed();
            if (displayLabel.isEmpty() && !spec.statMode.trimmed().isEmpty()
                && spec.statMode.trimmed().compare("Constant", Qt::CaseInsensitive) != 0) {
                displayLabel = QString("%1 = %2")
                                       .arg(localizedStatModeLabel(spec.statMode))
                                       .arg(QLocale::c().toString(spec.value, 'g', 4));
            }

            auto makeLabelQwtText = [&](double &twOut, double &thOut) -> QwtText {
                const bool isRich = displayLabel.contains(kRichTagRx);
                QString txtStr = displayLabel;
                if (isRich && !txtStr.contains(QLatin1Char('\n'))
                    && !txtStr.contains(QLatin1String("<br"), Qt::CaseInsensitive)) {
                    txtStr = QStringLiteral("<span style=\"white-space:nowrap;\">") + txtStr
                            + QStringLiteral("</span>");
                }
                QwtText qt(txtStr, isRich ? QwtText::RichText : QwtText::PlainText);
                qt.setFont(labelFont);
                qt.setColor(spec.color.darker(115));
                qt.setRenderFlags(Qt::AlignLeft | Qt::AlignVCenter);
                if (isRich) {
                    const QSize sz = qt.textSize(labelFont);
                    twOut = qMax(8.0 * sx, double(sz.width()) * richScaleX);
                    thOut = qMax(double(fm.height()), double(sz.height()) * richScaleY);
                } else {
                    twOut = qMax(8.0 * sx, fm.horizontalAdvance(displayLabel) + 2.0 * sx);
                    thOut = qMax(10.0 * sy, fm.height());
                }
                return qt;
            };

            const double padX = 3.0 * sx;
            const double padY = 1.0 * sy;
            const double offY = 4.0 * sy;
            const double offX = 6.0 * sx;

            const double devSc = Plot::printDeviceScale();
            auto drawMaskedHLine = [&](double yPx) {
                QVector<QPair<double, double>> spans;
                spans.append(qMakePair(double(canvasRect.left()), double(canvasRect.right())));
                for (const QRectF &mb : textMaskBoxes) {
                    if (yPx < mb.top() || yPx > mb.bottom())
                        continue;
                    QVector<QPair<double, double>> next;
                    for (const auto &sp : spans) {
                        if (mb.right() <= sp.first || mb.left() >= sp.second) {
                            next.append(sp);
                        } else {
                            if (mb.left() - sp.first > 1.5 * sx)
                                next.append(qMakePair(sp.first, mb.left()));
                            if (sp.second - mb.right() > 1.5 * sx)
                                next.append(qMakePair(mb.right(), sp.second));
                        }
                    }
                    spans.swap(next);
                }
                for (const auto &sp : spans)
                    painter->drawLine(QPointF(sp.first, yPx), QPointF(sp.second, yPx));
            };
            auto drawMaskedVLine = [&](double xPx) {
                QVector<QPair<double, double>> spans;
                spans.append(qMakePair(double(canvasRect.top()), double(canvasRect.bottom())));
                for (const QRectF &mb : textMaskBoxes) {
                    if (xPx < mb.left() || xPx > mb.right())
                        continue;
                    QVector<QPair<double, double>> next;
                    for (const auto &sp : spans) {
                        if (mb.bottom() <= sp.first || mb.top() >= sp.second) {
                            next.append(sp);
                        } else {
                            if (mb.top() - sp.first > 1.5 * sy)
                                next.append(qMakePair(sp.first, mb.top()));
                            if (sp.second - mb.bottom() > 1.5 * sy)
                                next.append(qMakePair(mb.bottom(), sp.second));
                        }
                    }
                    spans.swap(next);
                }
                for (const auto &sp : spans)
                    painter->drawLine(QPointF(xPx, sp.first), QPointF(xPx, sp.second));
            };

            if (spec.orientation == 0) {
                if (logY && spec.value <= 0.0)
                    continue;
                const double yPx = yMap.xTransform(spec.value);
                if (std::isfinite(yPx) && yPx >= canvasRect.top() - 2
                    && yPx <= canvasRect.bottom() + 2) {
                    QPen pen(spec.color, qMax(0.8, spec.width) * devSc, spec.style);
                    pen.setCosmetic(false);
                    painter->setPen(pen);
                    drawMaskedHLine(yPx);

                    if (!displayLabel.isEmpty()) {
                        double tw = 0.0, th = 0.0;
                        QwtText qt = makeLabelQwtText(tw, th);
                        const double minTx = canvasRect.left() + tickInsetX;
                        const double maxTx = qMax(minTx, canvasRect.right() - tw - tickInsetX);
                        const double yAbove = yPx - th - offY;
                        const double yBelow = yPx + offY;
                        const bool canAbove = (yAbove >= canvasRect.top() + 3.0 * sy);
                        const bool canBelow = (yBelow + th <= canvasRect.bottom() - 3.0 * sy);

                        struct CandPos {
                            double x;
                            double y;
                        };
                        QVector<CandPos> cands;
                        if (canAbove) {
                            cands.append({ maxTx, yAbove });
                            cands.append({ minTx, yAbove });
                        }
                        if (canBelow) {
                            cands.append({ maxTx, yBelow });
                            cands.append({ minTx, yBelow });
                        }
                        if (cands.isEmpty())
                            cands.append({ maxTx, qBound(double(canvasRect.top()) + 2.0 * sy, yAbove,
                                                         double(canvasRect.bottom()) - th - 2.0 * sy) });

                        CandPos best = cands.first();
                        int bestScore = scorePill(
                                QRectF(best.x - padX, best.y - padY, tw + 2.0 * padX, th + 2.0 * padY));
                        for (int ci = 1; ci < cands.size(); ++ci) {
                            const int sc = scorePill(QRectF(cands[ci].x - padX, cands[ci].y - padY,
                                                            tw + 2.0 * padX, th + 2.0 * padY));
                            if (sc < bestScore) {
                                bestScore = sc;
                                best = cands[ci];
                            }
                        }

                        const QRectF pill(best.x - padX, best.y - padY, tw + 2.0 * padX, th + 2.0 * padY);
                        painter->fillRect(pill, QColor(255, 255, 255, 215));
                        avoidBoxes.append(pill);
                        qt.draw(painter, QRectF(best.x, best.y, tw + 4.0 * sx, th).toRect());
                    }
                }
            } else {
                if (logX && spec.value <= 0.0)
                    continue;
                const double xPx = xMap.xTransform(spec.value);
                if (std::isfinite(xPx) && xPx >= canvasRect.left() - 2
                    && xPx <= canvasRect.right() + 2) {
                    QPen pen(spec.color, qMax(0.8, spec.width) * devSc, spec.style);
                    pen.setCosmetic(false);
                    painter->setPen(pen);
                    drawMaskedVLine(xPx);

                    if (!displayLabel.isEmpty()) {
                        double tw = 0.0, th = 0.0;
                        QwtText qt = makeLabelQwtText(tw, th);
                        const double minTx = canvasRect.left() + tickInsetX;
                        const double maxTx = qMax(minTx, canvasRect.right() - tw - tickInsetX);
                        const double tyTop = canvasRect.top() + tickInsetY;
                        const double tyBot = qMax(tyTop, canvasRect.bottom() - th - tickInsetY);

                        QVector<QPointF> cands;
                        if (xPx + offX + tw <= canvasRect.right() - tickInsetX + 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx + offX, maxTx), tyTop));
                        if (xPx - offX - tw >= canvasRect.left() + tickInsetX - 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx - tw - offX, maxTx), tyTop));
                        if (xPx + offX + tw <= canvasRect.right() - tickInsetX + 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx + offX, maxTx), tyBot));
                        if (xPx - offX - tw >= canvasRect.left() + tickInsetX - 2.0 * sx)
                            cands.append(QPointF(qBound(minTx, xPx - tw - offX, maxTx), tyBot));
                        if (cands.isEmpty())
                            cands.append(QPointF(qBound(minTx, xPx + 5.0 * sx, maxTx), tyTop));

                        QPointF best = cands.first();
                        int bestScore = scorePill(QRectF(best.x() - padX, best.y() - padY,
                                                         tw + 2.0 * padX, th + 2.0 * padY));
                        for (int ci = 1; ci < cands.size(); ++ci) {
                            const int sc = scorePill(QRectF(cands[ci].x() - padX, cands[ci].y() - padY,
                                                            tw + 2.0 * padX, th + 2.0 * padY));
                            if (sc < bestScore) {
                                bestScore = sc;
                                best = cands[ci];
                            }
                        }

                        const QRectF pill(best.x() - padX, best.y() - padY, tw + 2.0 * padX, th + 2.0 * padY);
                        painter->fillRect(pill, QColor(255, 255, 255, 215));
                        avoidBoxes.append(pill);
                        qt.draw(painter, QRectF(best.x(), best.y(), tw + 4.0 * sx, th).toRect());
                    }
                }
            }
        }

        painter->restore();
    }

private:
    const Graph *d_graph;
};
} // namespace

MultiLayer *Graph::multiLayerParent() const
{
    QObject *p = parent();
    while (p) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(p))
            return ml;
        p = p->parent();
    }
    return nullptr;
}

void Graph::setAxisBackboneVisible(int axis, bool visible)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    d_axis_backbone[axis] = visible;
    bool anyVisible = false;
    for (int i = 0; i < QwtPlot::axisCnt; ++i) {
        if (d_axis_backbone[i])
            anyVisible = true;
    }
    drawAxesBackbone = anyVisible;

    QwtScaleWidget *scale = (QwtScaleWidget *)d_plot->axisWidget(axis);
    if (scale) {
        ScaleDraw *sclDraw = (ScaleDraw *)d_plot->axisScaleDraw(axis);
        if (sclDraw)
            sclDraw->enableComponent(QwtAbstractScaleDraw::Backbone, visible);
        scale->repaint();
    }

    d_plot->replot();
    emit modifiedGraph();
}

bool Graph::isAxisBackboneVisible(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return false;
    return drawAxesBackbone && d_axis_backbone[axis];
}

bool Graph::hasColorScaleOnAxis(int axis) const
{
    if (!d_plot || axis < 0 || axis >= QwtPlot::axisCnt)
        return false;
    const QwtScaleWidget *sw = d_plot->axisWidget(axis);
    if (!sw || !sw->isColorBarEnabled())
        return false;
    const QwtPlotItemList &items = d_plot->itemList();
    for (QwtPlotItem *it : items) {
        if (!it || !it->isVisible())
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(it);
            if (sp->colorScaleAxis() == axis)
                return true;
        }
    }
    // E11: the colour bar of a colour-mapped scatter always sits on the right axis.
    if (axis == QwtPlot::yRight && d_point_colorbar_active && pointColorMapCurve())
        return true;
    return false;
}

DataCurve *Graph::pointColorMapCurve() const
{
    if (!d_plot)
        return nullptr;
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
        if (c_type[i] == ErrorBars)
            continue;
        DataCurve *dc = dynamic_cast<DataCurve *>(d_plot->curve(c_keys[i]));
        if (dc && dc->isVisible() && dc->hasPointColorMap())
            return dc;
    }
    return nullptr;
}

int Graph::errorBandCount() const
{
    int n = 0;
    for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
        if (c_type[i] != ErrorBars)
            continue;
        if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(d_plot->curve(c_keys[i])))
            if (er->isBand())
                ++n;
    }
    return n;
}

void Graph::syncPointColorBar()
{
    if (!d_plot)
        return;
    const int axis = QwtPlot::yRight;
    // A heatmap that owns the right colour bar wins.
    for (QwtPlotItem *it : d_plot->itemList()) {
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            Spectrogram *sp = static_cast<Spectrogram *>(it);
            if (sp->hasColorScale() && sp->colorScaleAxis() == axis)
                return;
        }
    }
    QwtScaleWidget *sw = d_plot->axisWidget(axis);
    if (!sw)
        return;
    DataCurve *src = pointColorMapCurve();
    if (!src || !src->pointColorMap().showColorBar) {
        if (d_point_colorbar_active || sw->isColorBarEnabled()) {
            d_point_colorbar_active = false;
            sw->setColorBarEnabled(false);
            sw->setMargin(0);
            bool anyRightCurve = false;
            for (int k = 0; k < n_curves && k < c_keys.size(); ++k) {
                if (QwtPlotItem *it = d_plot->plotItem(c_keys[k])) {
                    if (it->yAxis() == axis) {
                        anyRightCurve = true;
                        break;
                    }
                }
            }
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
                sd->setColorBarBorder(false, 0, 1);
                if (!anyRightCurve)
                    sd->enableComponent(QwtAbstractScaleDraw::Labels, false);
            }
            if (!anyRightCurve) {
                if (d_plot->axisEnabled(QwtPlot::xTop) && isAxisBackboneVisible(axis))
                    updateSecondaryAxis(axis);
                else
                    enableAxis(axis, false);
            }
            d_plot->updateLayout();
            d_plot->replot();
        }
        return;
    }
    double lo = 0.0, hi = 1.0;
    src->pointColorRange(&lo, &hi);
    const DataCurve::PointColorMap &spec = src->pointColorMap();
    const QwtLinearColorMap map = Spectrogram::presetColorMap(
            static_cast<Spectrogram::ColorMapPreset>(spec.preset), spec.reverse);
    d_point_colorbar_active = true;
    if (!d_plot->axisEnabled(axis))
        d_plot->enableAxis(axis, true);
    sw->setColorBarEnabled(true);
    sw->setColorMap(QwtDoubleInterval(lo, hi), map);
    sw->setMargin(4);
    sw->setSpacing(0);
    sw->setMinBorderDist(0, 0);
    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
        sd->setColorBarBorder(true, sw->colorBarWidth(), qMax(1, sw->penWidth()));
    }
    d_plot->setAxisScale(axis, lo, hi);
    if (!spec.colorBarTitle.isEmpty())
        setAxisTitle(axis, spec.colorBarTitle);
    QFont tf = d_plot->axisFont(axis);
    tf.setFamily(QStringLiteral("Arial"));
    tf.setHintingPreference(QFont::PreferNoHinting);
    d_plot->setAxisFont(axis, tf);
    sw->show();
    d_plot->updateLayout();
    d_plot->replot();
}

bool Graph::isPoleFigurePlot() const
{
    if (!d_plot
        || ExtraPlotTypes::graphTag(const_cast<Graph *>(this), QStringLiteral("ToriPoleFigure"))
                   .isEmpty()) {
        return false;
    }
    for (QwtPlotItem *it : d_plot->itemList()) {
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
            return true;
    }
    return false;
}

void Graph::applyFramePreset(FramePreset preset)
{
    if (isPoleFigurePlot()) {
        enableAxis(QwtPlot::xBottom, false);
        enableAxis(QwtPlot::yLeft, false);
        enableAxis(QwtPlot::xTop, false);
        if (!hasColorScaleOnAxis(QwtPlot::yRight))
            enableAxis(QwtPlot::yRight, false);
        drawCanvasFrame(false, 0);
        drawAxesBackbones(false);
        for (QwtPlotItem *it : d_plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                static_cast<Spectrogram *>(it)->syncColorScale();
                break;
            }
        }
        if (scalePicker)
            scalePicker->refresh();
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    if (isTernaryPlot() || isPolarPlot() || isPiePlot()) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, false);
        drawAxesBackbones(false);
        if (scalePicker)
            scalePicker->refresh();
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    const bool rightIsColorBar = hasColorScaleOnAxis(QwtPlot::yRight);
    const bool topIsColorBar = hasColorScaleOnAxis(QwtPlot::xTop);

    bool hasRightCurve = d_weibull_prob_axis[QwtPlot::yRight]
            || !d_custom_tick_positions[QwtPlot::yRight].isEmpty()
            || !d_custom_category_labels[QwtPlot::yRight].isEmpty();
    bool hasTopCurve = d_weibull_prob_axis[QwtPlot::xTop]
            || !d_custom_tick_positions[QwtPlot::xTop].isEmpty()
            || !d_custom_category_labels[QwtPlot::xTop].isEmpty();
    for (int i = 0; i < n_curves; ++i) {
        QwtPlotItem *it = plotItem(i);
        if (!it)
            continue;
        if (it->yAxis() == QwtPlot::yRight)
            hasRightCurve = true;
        if (it->xAxis() == QwtPlot::xTop)
            hasTopCurve = true;
    }

    auto setTickLabelsOnAxis = [&](int axis, bool on) {
        QwtScaleDraw *sd = d_plot->axisScaleDraw(axis);
        if (sd)
            sd->enableComponent(QwtAbstractScaleDraw::Labels, on);
    };

    if (d_plot->grid()) {
        d_plot->grid()->enableZeroLineX(false);
        d_plot->grid()->enableZeroLineY(false);
    }

    switch (preset) {
    case FrameBoxInwardTicks: {
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, true);
        drawAxesBackbones(true);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        setTickLabelsOnAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            if ((a == QwtPlot::yRight && rightIsColorBar) || (a == QwtPlot::xTop && topIsColorBar))
                continue;
            d_plot->setMajorTicksType(a, ScaleDraw::In);
            d_plot->setMinorTicksType(a, ScaleDraw::In);
        }
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
        break;
    }
    case FrameBoxPlainOpposite: {
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            enableAxis(a, true);
        drawAxesBackbones(true);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        setTickLabelsOnAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::In);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::In);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::In);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::In);
        if (!rightIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::In : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::In : ScaleDraw::None);
        }
        if (!topIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::In : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::In : ScaleDraw::None);
        }
        updateSecondaryAxis(QwtPlot::xTop);
        updateSecondaryAxis(QwtPlot::yRight);
        break;
    }
    case FrameOpenL: {
        enableAxis(QwtPlot::yLeft, true);
        enableAxis(QwtPlot::xBottom, true);
        enableAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        enableAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        setAxisBackboneVisible(QwtPlot::yLeft, true);
        setAxisBackboneVisible(QwtPlot::xBottom, true);
        setAxisBackboneVisible(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setAxisBackboneVisible(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        setTickLabelsOnAxis(QwtPlot::yRight, rightIsColorBar || hasRightCurve);
        setTickLabelsOnAxis(QwtPlot::xTop, topIsColorBar || hasTopCurve);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        if (!rightIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::Out : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::yRight,
                                      hasRightCurve ? ScaleDraw::Out : ScaleDraw::None);
        }
        if (!topIsColorBar) {
            d_plot->setMajorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::Out : ScaleDraw::None);
            d_plot->setMinorTicksType(QwtPlot::xTop, hasTopCurve ? ScaleDraw::Out : ScaleDraw::None);
        }
        break;
    }
    case FrameZeroCrossed: {
        enableAxis(QwtPlot::yLeft, true);
        enableAxis(QwtPlot::xBottom, true);
        enableAxis(QwtPlot::yRight, rightIsColorBar);
        enableAxis(QwtPlot::xTop, topIsColorBar);
        for (int a = 0; a < QwtPlot::axisCnt; ++a)
            setAxisBackboneVisible(a, false);
        setTickLabelsOnAxis(QwtPlot::yLeft, true);
        setTickLabelsOnAxis(QwtPlot::xBottom, true);
        d_plot->setMajorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::Out);
        d_plot->setMajorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::xBottom, ScaleDraw::Out);
        if (d_plot->grid()) {
            d_plot->grid()->enableZeroLineX(true);
            d_plot->grid()->enableZeroLineY(true);
        }
        break;
    }
    }

    if (rightIsColorBar) {
        enableAxis(QwtPlot::yRight, true);
        setTickLabelsOnAxis(QwtPlot::yRight, true);
        d_plot->setMajorTicksType(QwtPlot::yRight, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::yRight, ScaleDraw::None);
        if (QwtScaleDraw *sd = d_plot->axisScaleDraw(QwtPlot::yRight))
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::yRight)) {
            sw->setMargin(6);
            sw->setSpacing(0);
            if (ScaleDraw *s = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::yRight)))
                s->setColorBarBorder(true, sw->colorBarWidth(), qMax(1, sw->penWidth()));
        }
    }
    if (topIsColorBar) {
        enableAxis(QwtPlot::xTop, true);
        setTickLabelsOnAxis(QwtPlot::xTop, true);
        d_plot->setMajorTicksType(QwtPlot::xTop, ScaleDraw::Out);
        d_plot->setMinorTicksType(QwtPlot::xTop, ScaleDraw::None);
        if (QwtScaleDraw *sd = d_plot->axisScaleDraw(QwtPlot::xTop))
            sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
        if (QwtScaleWidget *sw = d_plot->axisWidget(QwtPlot::xTop)) {
            sw->setMargin(6);
            sw->setSpacing(0);
            if (ScaleDraw *s = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::xTop)))
                s->setColorBarBorder(true, sw->colorBarWidth(), qMax(1, sw->penWidth()));
        }
    }

    scalePicker->refresh();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::addReferenceLine(const ReferenceLineSpec &spec)
{
    if (!d_ref_fill_item) {
        d_ref_fill_item = new OriginReferenceFillItem(this);
        d_ref_fill_item->attach(d_plot);
    }
    if (!d_ref_line_item) {
        d_ref_line_item = new OriginReferenceLineItem(this);
        d_ref_line_item->attach(d_plot);
    }
    d_ref_lines.append(spec);
    updateStatReferenceLines();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setReferenceLines(const QVector<ReferenceLineSpec> &specs)
{
    if (!specs.isEmpty()) {
        if (!d_ref_fill_item) {
            d_ref_fill_item = new OriginReferenceFillItem(this);
            d_ref_fill_item->attach(d_plot);
        }
        if (!d_ref_line_item) {
            d_ref_line_item = new OriginReferenceLineItem(this);
            d_ref_line_item->attach(d_plot);
        }
    }
    d_ref_lines = specs;
    updateStatReferenceLines();
    d_plot->replot();
    emit modifiedGraph();
}

QVector<Graph::ReferenceLineSpec> Graph::referenceLines() const
{
    return d_ref_lines;
}

void Graph::clearReferenceLines()
{
    d_ref_lines.clear();
    if (d_selection.kind == GraphSelection::ReferenceLine)
        setCurrentSelection({ GraphSelection::None, -1, -1 });
    d_plot->replot();
    emit modifiedGraph();
}

int Graph::referenceLineAtCanvasPos(const QPoint &pos, int tolPx) const
{
    if (!d_plot || !d_plot->canvas() || d_ref_lines.isEmpty())
        return -1;
    const int effTol = qMax(2, tolPx);
    const QRect cr = d_plot->canvas()->rect();
    if (!cr.adjusted(-effTol, -effTol, effTol, effTol).contains(pos))
        return -1;

    const QwtScaleMap xMap = d_plot->canvasMap(QwtPlot::xBottom);
    const QwtScaleMap yMap = d_plot->canvasMap(QwtPlot::yLeft);

    double bestDist = double(effTol) + 0.001;
    int bestIdx = -1;
    for (int i = d_ref_lines.size() - 1; i >= 0; --i) {
        const ReferenceLineSpec &spec = d_ref_lines[i];
        if (!spec.visible || !std::isfinite(spec.value))
            continue;
        if (spec.orientation == 0) {
            const double yPx = yMap.transform(spec.value);
            if (!std::isfinite(yPx) || yPx < cr.top() - effTol || yPx > cr.bottom() + effTol)
                continue;
            const double d = std::fabs(double(pos.y()) - yPx);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        } else {
            const double xPx = xMap.transform(spec.value);
            if (!std::isfinite(xPx) || xPx < cr.left() - effTol || xPx > cr.right() + effTol)
                continue;
            const double d = std::fabs(double(pos.x()) - xPx);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

void Graph::updateReferenceLine(int index, const ReferenceLineSpec &spec)
{
    if (index < 0 || index >= d_ref_lines.size())
        return;
    if (!d_ref_fill_item) {
        d_ref_fill_item = new OriginReferenceFillItem(this);
        d_ref_fill_item->attach(d_plot);
    }
    if (!d_ref_line_item) {
        d_ref_line_item = new OriginReferenceLineItem(this);
        d_ref_line_item->attach(d_plot);
    }
    d_ref_lines[index] = spec;
    updateStatReferenceLines();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::removeReferenceLine(int index)
{
    if (index < 0 || index >= d_ref_lines.size())
        return;
    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    d_ref_lines.remove(index);
    if (d_selection.kind == GraphSelection::ReferenceLine) {
        if (d_selection.primaryId == index)
            setCurrentSelection({ GraphSelection::None, -1, -1 });
        else if (d_selection.primaryId > index)
            setCurrentSelection(
                    { GraphSelection::ReferenceLine, d_selection.primaryId - 1, -1 });
    }
    d_plot->replot();
    emit modifiedGraph();
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Delete Reference Line"), beforeSnap);
}

bool Graph::editReferenceLineDialog(int index)
{
    if (index < 0 || index >= d_ref_lines.size())
        return false;

    ReferenceLineSpec spec = d_ref_lines[index];

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Reference Line"));
    dlg.setModal(true);
    dlg.setFont(QFont(QStringLiteral("Arial"), 10));

    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    QComboBox *orientBox = new QComboBox(&dlg);
    orientBox->addItem(tr("Horizontal (Y = const)"), 0);
    orientBox->addItem(tr("Vertical (X = const)"), 1);
    orientBox->setCurrentIndex(spec.orientation == 1 ? 1 : 0);

    QLineEdit *valEdit = new QLineEdit(QLocale::c().toString(spec.value, 'g', 10), &dlg);
    QLineEdit *labelEdit = new QLineEdit(spec.label, &dlg);
    labelEdit->setPlaceholderText(tr("Optional label (e.g. Y = 0)"));

    QComboBox *styleBox = new QComboBox(&dlg);
    styleBox->addItem(tr("Solid"), int(Qt::SolidLine));
    styleBox->addItem(tr("Dash"), int(Qt::DashLine));
    styleBox->addItem(tr("Dot"), int(Qt::DotLine));
    styleBox->addItem(tr("Dash-Dot"), int(Qt::DashDotLine));
    int styleIdx = styleBox->findData(int(spec.style));
    styleBox->setCurrentIndex(styleIdx >= 0 ? styleIdx : 1);

    QDoubleSpinBox *widthSpin = new QDoubleSpinBox(&dlg);
    widthSpin->setRange(0.5, 12.0);
    widthSpin->setSingleStep(0.5);
    widthSpin->setDecimals(1);
    widthSpin->setValue(qBound(0.5, spec.width, 12.0));

    QColor chosenColor = spec.color.isValid() ? spec.color : QColor(30, 102, 184);
    QPushButton *colorBtn = new QPushButton(tr("Choose Color..."), &dlg);
    auto updateColorBtnStyle = [colorBtn](const QColor &c) {
        colorBtn->setStyleSheet(
                QStringLiteral("QPushButton { background-color: %1; color: %2; padding: 4px 10px; "
                               "border: 1px solid #888; border-radius: 3px; }")
                        .arg(c.name(), c.lightness() < 140 ? QStringLiteral("#ffffff")
                                                           : QStringLiteral("#111111")));
    };
    updateColorBtnStyle(chosenColor);
    connect(colorBtn, &QPushButton::clicked, &dlg, [&dlg, &chosenColor, updateColorBtnStyle]() {
        const QColor c = QColorDialog::getColor(chosenColor, &dlg, tr("Reference Line Color"));
        if (c.isValid()) {
            chosenColor = c;
            updateColorBtnStyle(chosenColor);
        }
    });

    QCheckBox *fillCheck = new QCheckBox(tr("Shade to zero / axis baseline"), &dlg);
    fillCheck->setChecked(spec.fillToAxis);

    grid->addWidget(new QLabel(tr("Orientation:"), &dlg), 0, 0);
    grid->addWidget(orientBox, 0, 1);
    grid->addWidget(new QLabel(tr("Value:"), &dlg), 1, 0);
    grid->addWidget(valEdit, 1, 1);
    grid->addWidget(new QLabel(tr("Label:"), &dlg), 2, 0);
    grid->addWidget(labelEdit, 2, 1);
    grid->addWidget(new QLabel(tr("Line Style:"), &dlg), 3, 0);
    grid->addWidget(styleBox, 3, 1);
    grid->addWidget(new QLabel(tr("Line Width:"), &dlg), 4, 0);
    grid->addWidget(widthSpin, 4, 1);
    grid->addWidget(new QLabel(tr("Color:"), &dlg), 5, 0);
    grid->addWidget(colorBtn, 5, 1);
    grid->addWidget(fillCheck, 6, 0, 1, 2);
    mainLay->addLayout(grid);

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *deleteBtn = new QPushButton(tr("Delete"), &dlg);
    bool deleted = false;
    connect(deleteBtn, &QPushButton::clicked, &dlg, [&dlg, &deleted]() {
        deleted = true;
        dlg.accept();
    });
    btnRow->addWidget(deleteBtn);
    btnRow->addStretch(1);
    QDialogButtonBox *bbox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    btnRow->addWidget(bbox);
    mainLay->addLayout(btnRow);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    if (deleted) {
        removeReferenceLine(index);
        return true;
    }

    bool valOk = false;
    double newVal = QLocale::c().toDouble(valEdit->text().trimmed(), &valOk);
    if (!valOk)
        newVal = QLocale().toDouble(valEdit->text().trimmed(), &valOk);
    if (!valOk || !std::isfinite(newVal))
        return false;

    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    if (qAbs(newVal - spec.value) > 1e-15
        || orientBox->currentData().toInt() != spec.orientation) {
        spec.statMode.clear();
    }
    spec.orientation = orientBox->currentData().toInt();
    spec.value = newVal;
    spec.label = labelEdit->text().trimmed();
    spec.style = static_cast<Qt::PenStyle>(styleBox->currentData().toInt());
    spec.width = widthSpin->value();
    spec.color = chosenColor;
    spec.fillToAxis = fillCheck->isChecked();
    QColor fillCol = chosenColor;
    fillCol.setAlpha(32);
    spec.fillColor = fillCol;

    updateReferenceLine(index, spec);
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Edit Reference Line"), beforeSnap);
    return true;
}

QFont Graph::axisNumbersFont(int axis) const
{
    if (d_plot && axis >= 0 && axis < QwtPlot::axisCnt)
        return d_plot->axisFont(axis);
    return QFont(QStringLiteral("Arial"), 12, QFont::Normal);
}

QFont Graph::legendFont() const
{
    if (Legend *leg = const_cast<Graph *>(this)->legend())
        return leg->font();
    const QFont axF = axisNumbersFont(QwtPlot::yLeft);
    const double axPt = axF.pointSizeF() > 0.0
            ? axF.pointSizeF()
            : (axF.pointSize() > 0 ? double(axF.pointSize()) : 11.0);
    const double basePt = qMax(9.0, axPt - 2.0);
    QFont f(QStringLiteral("Arial"));
    f.setPointSizeF(basePt);
    f.setWeight(QFont::Normal);
    return f;
}

void Graph::syncAxisColorToCurve(int qwtAxis, int curveIndex)
{
    if (!d_plot || qwtAxis < 0 || qwtAxis >= QwtPlot::axisCnt || curveIndex < 0
        || curveIndex >= n_curves) {
        return;
    }
    const QwtPlotCurve *c = curve(curveIndex);
    if (!c)
        return;
    QColor col = c->pen().color();
    if (!col.isValid() || col.alpha() == 0)
        col = c->symbol().pen().color();
    if (!col.isValid() || col.alpha() == 0)
        col = Qt::black;

    if (QwtScaleWidget *sw = d_plot->axisWidget(qwtAxis)) {
        QPalette pal = sw->palette();
        pal.setColor(QPalette::WindowText, col);
        pal.setColor(QPalette::Text, col);
        sw->setPalette(pal);

        QwtText t = d_plot->axisTitle(qwtAxis);
        t.setColor(col);
        d_plot->setAxisTitle(qwtAxis, t);
        sw->repaint();
    }
    d_plot->replot();
    emit modifiedGraph();
}

namespace {
bool axisViewportStatesEqual(const Graph::AxisViewportState &a, const Graph::AxisViewportState &b)
{
    if (a.autoscale != b.autoscale)
        return false;
    for (int i = 0; i < 4; ++i) {
        if (a.axisEnabled[i] != b.axisEnabled[i] || a.isLog[i] != b.isLog[i]
            || a.inverted[i] != b.inverted[i]) {
            return false;
        }
        const double span = qMax(1e-15, qAbs(a.upper[i] - a.lower[i]));
        if (qAbs(a.lower[i] - b.lower[i]) > 1e-9 * span
            || qAbs(a.upper[i] - b.upper[i]) > 1e-9 * span) {
            return false;
        }
    }
    return true;
}
} // namespace

Graph::AxisViewportState Graph::captureAxisViewportState() const
{
    AxisViewportState st;
    st.autoscale = m_autoscale;
    if (!d_plot)
        return st;
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        st.axisEnabled[a] = d_plot->axisEnabled(a);
        const QwtScaleDiv *div = d_plot->axisScaleDiv(a);
        const QwtScaleEngine *se = d_plot->axisScaleEngine(a);
        const double b1 = div ? div->lowerBound() : 0.0;
        const double b2 = div ? div->upperBound() : 1.0;
        st.lower[a] = qMin(b1, b2);
        st.upper[a] = qMax(b1, b2);
        st.userStep[a] = (a < d_user_step.size()) ? d_user_step[a] : 0.0;
        st.maxMajor[a] = d_plot->axisMaxMajor(a);
        st.maxMinor[a] = d_plot->axisMaxMinor(a);
        st.isLog[a] = d_axis_breaks[a].enabled
                ? (d_axis_breaks[a].log10Before && d_axis_breaks[a].log10After)
                : Plot::isLog10ScaleEngine(se);
        st.inverted[a] =
                (se && se->testAttribute(QwtScaleEngine::Inverted)) || (div && b1 > b2);
        st.userLocked[a] = isToriAxisUserLocked(d_plot, a);
    }
    return st;
}

void Graph::applyAxisViewportState(const AxisViewportState &st)
{
    if (!d_plot)
        return;
    const MarkerAxisState axesBefore = markerAxisState();
    const bool prevApplying = d_applying_zoom_history;
    d_applying_zoom_history = true;
    m_autoscale = st.autoscale;

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        QwtScaleEngine *se = d_plot->axisScaleEngine(a);
        if (!se || st.upper[a] <= st.lower[a])
            continue;

        const bool hasBreak = d_axis_breaks[a].enabled;
        const bool curLog = hasBreak
                ? (d_axis_breaks[a].log10Before && d_axis_breaks[a].log10After)
                : Plot::isLog10ScaleEngine(se);

        if (!hasBreak && curLog != st.isLog[a]) {
            se = st.isLog[a] ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                             : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
            d_plot->setAxisScaleEngine(a, se);
        }

        double effStep = 0.0;
        if (!st.isLog[a] && st.userStep[a] > 0.0) {
            const double nMaj = (st.upper[a] - st.lower[a]) / st.userStep[a];
            if (nMaj >= 1.5 && nMaj <= 25.0)
                effStep = st.userStep[a];
        }
        int maxMaj = qMax(2, st.maxMajor[a]);
        if (hasBreak && axisFont(a).pointSize() >= 20)
            maxMaj = qMin(maxMaj, 5);
        const int minT = st.maxMinor[a];
        const int maxMinInt = (minT == 1) ? (hasBreak ? 2 : 3) : ((minT > 1) ? (minT + 1) : 0);

        QwtScaleDiv div = se->divideScale(st.lower[a], st.upper[a], maxMaj, maxMinInt, effStep);
        if (st.inverted[a]) {
            se->setAttribute(QwtScaleEngine::Inverted, true);
            if (div.lowerBound() < div.upperBound())
                div.invert();
        } else {
            se->setAttribute(QwtScaleEngine::Inverted, false);
        }
        d_plot->setAxisScaleDiv(a, div);
        setToriAxisUserLocked(d_plot, a, st.userLocked[a]);

        if (hasBreak) {
            const double bf = qMin(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
            const double bt = qMax(d_axis_breaks[a].breakFrom, d_axis_breaks[a].breakTo);
            const bool straddles = (bf > st.lower[a] && bt < st.upper[a] && bt > bf + 1e-15);
            if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(a)))
                sd->setAxisBreakIndicator(straddles, bf, bt, d_axis_breaks[a].log10Before,
                                          d_axis_breaks[a].log10After);
        }
    }

    d_plot->syncMirrorAxisIfNeeded(QwtPlot::xBottom);
    d_plot->syncMirrorAxisIfNeeded(QwtPlot::yLeft);
    refreshAxisBreakIndicator(QwtPlot::xTop);
    refreshAxisBreakIndicator(QwtPlot::yRight);
    updateMarkersBoundingRect(axesBefore);
    updateStatReferenceLines();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();
    d_plot->replot();
    updateSelectionOverlayGeometry();
    d_applying_zoom_history = prevApplying;
    emit modifiedGraph();
}

void Graph::recordZoomHistorySnapshot()
{
    if (d_applying_zoom_history || !d_plot || isPiePlot() || isTernaryPlot())
        return;

    const AxisViewportState cur = captureAxisViewportState();
    if (d_zoom_history_pos < d_zoom_history.size())
        d_zoom_history.resize(d_zoom_history_pos + 1);

    if (d_zoom_history.isEmpty() || !axisViewportStatesEqual(d_zoom_history.last(), cur)) {
        if (d_zoom_history.size() >= 32)
            d_zoom_history.removeFirst();
        d_zoom_history.append(cur);
    }
    d_zoom_history_pos = d_zoom_history.size();
}

bool Graph::stepZoomHistory(int delta)
{
    if (delta == 0 || d_zoom_history.isEmpty() || !d_plot || isPiePlot() || isTernaryPlot())
        return false;

    if (delta < 0) {
        if (d_zoom_history_pos >= d_zoom_history.size()) {
            const AxisViewportState cur = captureAxisViewportState();
            if (!axisViewportStatesEqual(d_zoom_history.last(), cur)) {
                if (d_zoom_history.size() >= 32)
                    d_zoom_history.removeFirst();
                d_zoom_history.append(cur);
            }
            d_zoom_history_pos = d_zoom_history.size() - 1;
        }
        const int target = d_zoom_history_pos + delta;
        if (target < 0 || target >= d_zoom_history.size())
            return false;
        d_zoom_history_pos = target;
        applyAxisViewportState(d_zoom_history[d_zoom_history_pos]);
        return true;
    }

    // Redo (delta > 0)
    if (d_zoom_history_pos >= d_zoom_history.size() - 1)
        return false;
    const int target = qMin(int(d_zoom_history.size()) - 1, d_zoom_history_pos + delta);
    if (target == d_zoom_history_pos)
        return false;
    d_zoom_history_pos = target;
    applyAxisViewportState(d_zoom_history[d_zoom_history_pos]);
    return true;
}

bool Graph::handleEscapeKey()
{
    if (scalePicker && scalePicker->hasActiveInlineScaleEditor()) {
        cancelInlineScaleEditor();
        return true;
    }
    if (cp && cp->cancelActiveInteraction())
        return true;

    if (zoomOn() || activeTool() || rangeSelectorsEnabled() || drawLineActive()
        || drawTextActive()) {
        const bool wasDrawText = drawTextActive();
        disableTools();
        if (wasDrawText) {
            drawText(false);
            emit drawTextOff();
        }
        if (MultiLayer *ml = multiLayerParent())
            emit ml->setPointerCursor();
        if (d_plot && d_plot->canvas())
            d_plot->canvas()->setCursor(Qt::ArrowCursor);
        return true;
    }

    if (d_selection.kind != GraphSelection::None || markerSelected() || titleSelected()) {
        deselect();
        return true;
    }
    return false;
}

bool Graph::nudgeSelectedElement(int dx, int dy)
{
    if ((dx == 0 && dy == 0) || !d_plot)
        return false;

    MultiLayer *ml = multiLayerParent();
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();
    const double zoom = qMax(0.05, d_plot->displayFontScale());
    bool changed = false;

    if (d_selection.kind == GraphSelection::Axis && d_selection.primaryId >= 0
        && d_selection.primaryId < QwtPlot::axisCnt) {
        const int axis = d_selection.primaryId;
        if (QwtScaleWidget *sw = d_plot->axisWidget(axis)) {
            int deltaNormal = 0;
            int deltaParallel = 0;
            if (axis == QwtPlot::yLeft) {
                deltaNormal = -dx;
                deltaParallel = dy;
            } else if (axis == QwtPlot::yRight) {
                deltaNormal = dx;
                deltaParallel = dy;
            } else if (axis == QwtPlot::xBottom) {
                deltaNormal = dy;
                deltaParallel = dx;
            } else if (axis == QwtPlot::xTop) {
                deltaNormal = -dy;
                deltaParallel = dx;
            }
            if (deltaNormal != 0) {
                const double curNorm = sw->property("_toriAxisNormalShift").toDouble();
                sw->setProperty("_toriAxisNormalShift",
                                qBound(-500.0, curNorm + double(deltaNormal) / zoom, 500.0));
                changed = true;
            }
            if (deltaParallel != 0) {
                const double curPar = sw->property("_toriAxisParallelShift").toDouble();
                sw->setProperty("_toriAxisParallelShift",
                                qBound(-500.0, curPar + double(deltaParallel) / zoom, 500.0));
                changed = true;
            }
            if (changed)
                d_plot->updateLayout();
        }
    } else if (d_selection.kind == GraphSelection::AxisTitle && d_selection.primaryId >= 0
               && d_selection.primaryId < QwtPlot::axisCnt) {
        const int axis = d_selection.primaryId;
        if (QwtScaleWidget *sw = d_plot->axisWidget(axis)) {
            const QPointF cur = sw->property("_toriAxisTitleShift").toPointF();
            const QPointF next(qBound(-500.0, cur.x() + double(dx) / zoom, 500.0),
                               qBound(-500.0, cur.y() + double(dy) / zoom, 500.0));
            sw->setProperty("_toriAxisTitleShift", next);
            d_plot->updateLayout();
            changed = true;
        }
    } else if (d_selection.kind == GraphSelection::PlotTitle || titleSelected()) {
        const QPointF cur = d_plot->property("_toriPlotTitleShift").toPointF();
        const QPointF next(qBound(-500.0, cur.x() + double(dx) / zoom, 500.0),
                           qBound(-500.0, cur.y() + double(dy) / zoom, 500.0));
        d_plot->setProperty("_toriPlotTitleShift", next);
        d_plot->updateLayout();
        changed = true;
    } else if (d_selection.kind == GraphSelection::LegendBox
               || d_selection.kind == GraphSelection::TextAnnotation) {
        Legend *mrk = nullptr;
        if (d_selection.primaryId >= 0)
            mrk = dynamic_cast<Legend *>(d_plot->marker(d_selection.primaryId));
        if (!mrk && d_selection.kind == GraphSelection::LegendBox)
            mrk = legend();
        if (!mrk && selectedMarker >= 0)
            mrk = dynamic_cast<Legend *>(d_plot->marker(selectedMarker));
        if (mrk) {
            mrk->setOrigin(mrk->rect().topLeft() + QPoint(dx, dy));
            if (mrk == legend())
                d_plot->setProperty("_toriLegendAnchor", QVariant());
            if (d_markers_selector)
                d_markers_selector->recalcBoundingRect();
            changed = true;
        }
    } else if (d_selection.kind == GraphSelection::ArrowAnnotation) {
        ArrowMarker *am = (d_selection.primaryId >= 0)
                ? arrow(d_selection.primaryId)
                : ((selectedMarker >= 0) ? arrow(selectedMarker) : nullptr);
        if (am) {
            am->setStartPoint(am->startPoint() + QPoint(dx, dy));
            am->setEndPoint(am->endPoint() + QPoint(dx, dy));
            if (d_markers_selector)
                d_markers_selector->recalcBoundingRect();
            changed = true;
        }
    } else if (d_selection.kind == GraphSelection::ImageAnnotation) {
        ImageMarker *im = (d_selection.primaryId >= 0)
                ? imageMarker(d_selection.primaryId)
                : ((selectedMarker >= 0) ? imageMarker(selectedMarker) : nullptr);
        if (im) {
            im->setOrigin(im->rect().topLeft() + QPoint(dx, dy));
            if (d_markers_selector)
                d_markers_selector->recalcBoundingRect();
            changed = true;
        }
    }

    if (changed) {
        d_plot->replot();
        d_plot->update();
        updateSelectionOverlayGeometry();
        if (ml) {
            if (!beforeSnap.isEmpty())
                ml->pushUndoSnapshot(tr("Move Label / Scale"), beforeSnap, 703);
            ml->modifiedPlot();
        }
    }
    return changed;
}

bool Graph::cycleSelectableCurve(int step)
{
    if (step == 0 || n_curves <= 0)
        return false;

    QVector<int> visCurves;
    visCurves.reserve(n_curves);
    for (int i = 0; i < n_curves; ++i) {
        if (i < c_type.size() && c_type[i] == ErrorBars)
            continue;
        QwtPlotItem *it = plotItem(i);
        if (it && it->isVisible())
            visCurves.append(i);
    }
    if (visCurves.isEmpty())
        return false;

    int curPos = -1;
    if (d_selection.kind == GraphSelection::Curve)
        curPos = visCurves.indexOf(d_selection.primaryId);

    const int m = visCurves.size();
    const int nextPos = (curPos < 0)
            ? (step > 0 ? 0 : (m - 1))
            : (((curPos + (step > 0 ? 1 : -1)) % m + m) % m);
    setCurrentSelection({ GraphSelection::Curve, visCurves[nextPos], -1 });
    return true;
}

void Graph::updateStatReferenceLines()
{
    if (d_ref_lines.isEmpty())
        return;

    bool hasAnyStat = false;
    for (const ReferenceLineSpec &s : d_ref_lines) {
        if (!s.statMode.trimmed().isEmpty()
            && s.statMode.trimmed().compare("Constant", Qt::CaseInsensitive) != 0) {
            hasAnyStat = true;
            break;
        }
    }
    if (!hasAnyStat)
        return;

    QwtPlotCurve *activeCurve = nullptr;
    if (d_range_selector && d_range_selector->selectedCurve()) {
        activeCurve = d_range_selector->selectedCurve();
    }
    if (!activeCurve) {
        for (int i = 0; i < n_curves; ++i) {
            if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
                || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
                continue;
            QwtPlotCurve *c = curve(i);
            if (c && c->isVisible() && c->dataSize() > 0) {
                activeCurve = c;
                break;
            }
        }
    }
    if (!activeCurve) {
        for (int i = 0; i < n_curves; ++i) {
            QwtPlotCurve *c = curve(i);
            if (c && c->dataSize() > 0) {
                activeCurve = c;
                break;
            }
        }
    }
    if (!activeCurve)
        return;

    DataCurve *dc = dynamic_cast<DataCurve *>(activeCurve);

    auto computeStats = [&](int orientation, double &outMean, double &outSD, double &outMedian,
                            double &outMin, double &outMax) -> bool {
        QVector<double> vals;
        const int n = activeCurve->dataSize();
        vals.reserve(n);
        Column *dcXCol = nullptr;
        Column *dcYCol = nullptr;
        const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
        if (dcHasMasks)
            dc->resolveColumns(dcXCol, dcYCol);
        for (int j = 0; j < n; ++j) {
            if (dcHasMasks && dc->isPointMaskedFast(j, dcXCol, dcYCol))
                continue;
            const double v = (orientation == 0) ? activeCurve->y(j) : activeCurve->x(j);
            if (std::isfinite(v))
                vals.append(v);
        }
        if (vals.isEmpty())
            return false;

        double sum = 0.0;
        for (double v : vals)
            sum += v;
        outMean = sum / double(vals.size());

        double sqSum = 0.0;
        for (double v : vals) {
            const double d = v - outMean;
            sqSum += d * d;
        }
        outSD = (vals.size() > 1) ? std::sqrt(sqSum / double(vals.size() - 1)) : 0.0;

        QVector<double> sorted = vals;
        std::sort(sorted.begin(), sorted.end());
        const int m = sorted.size();
        outMedian = (m % 2 == 1) ? sorted[m / 2] : 0.5 * (sorted[m / 2 - 1] + sorted[m / 2]);
        outMin = sorted.first();
        outMax = sorted.last();
        return true;
    };

    double yMean = 0.0, ySD = 0.0, yMed = 0.0, yMin = 0.0, yMax = 0.0;
    bool yReady = false, yValid = false;
    double xMean = 0.0, xSD = 0.0, xMed = 0.0, xMin = 0.0, xMax = 0.0;
    bool xReady = false, xValid = false;

    for (ReferenceLineSpec &spec : d_ref_lines) {
        QString mode = spec.statMode.trimmed().toLower();
        mode.remove(' ');
        if (mode.isEmpty() || mode == "constant")
            continue;

        double mean = 0.0, sd = 0.0, med = 0.0, minV = 0.0, maxV = 0.0;
        if (spec.orientation == 0) {
            if (!yReady) {
                yValid = computeStats(0, yMean, ySD, yMed, yMin, yMax);
                yReady = true;
            }
            if (!yValid)
                continue;
            mean = yMean;
            sd = ySD;
            med = yMed;
            minV = yMin;
            maxV = yMax;
        } else {
            if (!xReady) {
                xValid = computeStats(1, xMean, xSD, xMed, xMin, xMax);
                xReady = true;
            }
            if (!xValid)
                continue;
            mean = xMean;
            sd = xSD;
            med = xMed;
            minV = xMin;
            maxV = xMax;
        }

        if (mode == "mean")
            spec.value = mean;
        else if (mode == "median")
            spec.value = med;
        else if (mode == "mean+sd" || mode == "+sd")
            spec.value = mean + sd;
        else if (mode == "mean-sd" || mode == "-sd")
            spec.value = mean - sd;
        else if (mode == "sd")
            spec.value = sd;
        else if (mode == "min")
            spec.value = minV;
        else if (mode == "max")
            spec.value = maxV;
    }
}

void Graph::setLinkedParentLayer(Graph *parentLayer, int xLinkMode, const QString &xExpr,
                                 int yLinkMode, const QString &yExpr)
{
    if (parentLayer == this)
        parentLayer = nullptr;

    d_linked_parent = parentLayer;
    d_x_link_mode = xLinkMode;
    d_x_link_expr = xExpr.trimmed().isEmpty() ? QString("x") : xExpr.trimmed();
    d_y_link_mode = yLinkMode;
    d_y_link_expr = yExpr.trimmed().isEmpty() ? QString("y") : yExpr.trimmed();

    if (d_linked_parent)
        syncLinkedAxesFromParent();
}

Graph *Graph::linkedParentLayer() const
{
    return d_linked_parent.data();
}

void Graph::syncLinkedAxesFromParent()
{
    if (d_syncing_link || !d_linked_parent)
        return;

    d_syncing_link = true;
    Plot *parentPlot = d_linked_parent->plotWidget();
    if (!parentPlot) {
        d_syncing_link = false;
        return;
    }

    auto evalEndpoint = [](const QString &expr, double val, bool &ok) -> double {
        ok = false;
        if (expr.isEmpty()) {
            ok = true;
            return val;
        }
        try {
            MyParser parser;
            double v = val;
            parser.DefineVar(_T("x"), &v);
            parser.DefineVar(_T("x1"), &v);
            parser.DefineVar(_T("X"), &v);
            parser.DefineVar(_T("y"), &v);
            parser.DefineVar(_T("y1"), &v);
            parser.DefineVar(_T("Y"), &v);
            parser.SetExpr(expr);
            const double res = parser.Eval();
            if (std::isfinite(res)) {
                ok = true;
                return res;
            }
        } catch (...) {
        }
        return val;
    };

    auto syncOneLinkedPair = [&](int linkMode, const QString &expr, int srcAxis, int dstPrimary,
                                 int dstSecondary) {
        if (linkMode != 1 && linkMode != 2)
            return;
        const QwtScaleDiv *pDiv = parentPlot->axisScaleDiv(srcAxis);
        const QwtScaleEngine *pEng = parentPlot->axisScaleEngine(srcAxis);
        if (!pDiv || !pEng)
            return;

        const double pBound1 = pDiv->lowerBound();
        const double pBound2 = pDiv->upperBound();
        const double pLo = qMin(pBound1, pBound2);
        const double pHi = qMax(pBound1, pBound2);
        const bool parentInverted = pEng->testAttribute(QwtScaleEngine::Inverted);
        const int maj = parentPlot->axisMaxMajor(srcAxis);
        const int minT = parentPlot->axisMaxMinor(srcAxis);

        if (linkMode == 1) {
            if (qAbs(pHi - pLo) <= 1e-300)
                return;
            const int scaleType = (Plot::isLog10ScaleEngine(pEng) && pLo > 0.0) ? 1 : 0;
            auto applyDirect = [&](int targetAxis) {
                QwtScaleEngine *se = scaleType
                        ? static_cast<QwtScaleEngine *>(new QwtLog10ScaleEngine())
                        : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
                int maxMinInt = minT;
                if (minT == 1)
                    maxMinInt = 3;
                else if (minT > 1)
                    maxMinInt = minT + 1;
                QwtScaleDiv div = se->divideScale(pLo, pHi, maj, maxMinInt, 0.0);
                if (parentInverted) {
                    se->setAttribute(QwtScaleEngine::Inverted);
                    div.invert();
                }
                d_plot->setAxisMaxMajor(targetAxis, maj);
                d_plot->setAxisMaxMinor(targetAxis, minT);
                d_plot->setAxisScaleEngine(targetAxis, se);
                d_plot->setAxisScaleDiv(targetAxis, div);
            };
            applyDirect(dstPrimary);
            applyDirect(dstSecondary);
            return;
        }

        // Formula mode (linkMode == 2): precompute 129-point monotone lookup table
        double pLoSafe = pLo;
        const double pHiSafe = pHi;
        if (pLoSafe <= 0.0 && pHiSafe > 0.0 && expr.contains(QLatin1Char('/')))
            pLoSafe = qMax(1e-3, pHiSafe * 0.02);
        if (pHiSafe - pLoSafe <= 1e-300)
            return;

        constexpr int kTablePts = 129;
        QVector<double> xpTable(kTablePts), ucTable(kTablePts);
        bool allFinite = true;
        try {
            MyParser parser;
            double v = pLoSafe;
            parser.DefineVar(_T("x"), &v);
            parser.DefineVar(_T("x1"), &v);
            parser.DefineVar(_T("X"), &v);
            parser.DefineVar(_T("y"), &v);
            parser.DefineVar(_T("y1"), &v);
            parser.DefineVar(_T("Y"), &v);
            parser.SetExpr(expr);
            for (int k = 0; k < kTablePts; ++k) {
                const double xk = pLoSafe
                        + (double(k) / double(kTablePts - 1)) * (pHiSafe - pLoSafe);
                xpTable[k] = xk;
                v = xk;
                const double uk = parser.Eval();
                if (!std::isfinite(uk)) {
                    allFinite = false;
                    break;
                }
                ucTable[k] = uk;
            }
        } catch (...) {
            allFinite = false;
        }

        if (!allFinite) {
            bool ok1 = false, ok2 = false;
            const double c1 = evalEndpoint(expr, pLoSafe, ok1);
            const double c2 = evalEndpoint(expr, pHiSafe, ok2);
            if (!ok1 || !ok2 || qAbs(c2 - c1) <= 1e-300)
                return;
            const double lo = qMin(c1, c2);
            const double hi = qMax(c1, c2);
            const bool shouldInvert = ((c1 > c2) != parentInverted);
            auto applyFallback = [&](int targetAxis) {
                QwtLinearScaleEngine *se = new QwtLinearScaleEngine();
                QwtScaleDiv div = se->divideScale(lo, hi, qMin(qMax(2, maj), 5), 3, 0.0);
                if (shouldInvert) {
                    se->setAttribute(QwtScaleEngine::Inverted, true);
                    div.invert();
                }
                d_plot->setAxisMaxMajor(targetAxis, qMin(qMax(2, maj), 5));
                d_plot->setAxisMaxMinor(targetAxis, 1);
                d_plot->setAxisScaleEngine(targetAxis, se);
                d_plot->setAxisScaleDiv(targetAxis, div);
            };
            applyFallback(dstPrimary);
            applyFallback(dstSecondary);
            return;
        }

        const double c1 = ucTable.first();
        const double c2 = ucTable.last();
        if (qAbs(c2 - c1) <= 1e-300)
            return;

        bool monoInc = true;
        bool monoDec = true;
        for (int k = 0; k < kTablePts - 1; ++k) {
            if (!(ucTable[k + 1] >= ucTable[k]))
                monoInc = false;
            if (!(ucTable[k + 1] <= ucTable[k]))
                monoDec = false;
        }
        const bool isMonotone = (monoInc || monoDec);
        const bool childDecreasing = (c1 > c2);
        const bool shouldInvert = (childDecreasing != parentInverted);
        const double uLo = qMin(c1, c2);
        const double uHi = qMax(c1, c2);
        const int maxMaj = qMin(qMax(2, maj), 5);

        auto applyFormulaAxis = [&](int targetAxis) {
            QwtScaleEngine *se = isMonotone
                    ? static_cast<QwtScaleEngine *>(new FormulaLinkedScaleEngine(xpTable, ucTable))
                    : static_cast<QwtScaleEngine *>(new QwtLinearScaleEngine());
            if (shouldInvert)
                se->setAttribute(QwtScaleEngine::Inverted, true);
            QwtScaleDiv div = se->divideScale(uLo, uHi, maxMaj, 3, 0.0);
            if (shouldInvert && div.lowerBound() < div.upperBound())
                div.invert();
            d_plot->setAxisMaxMajor(targetAxis, maxMaj);
            d_plot->setAxisMaxMinor(targetAxis, 1);
            d_plot->setAxisScaleEngine(targetAxis, se);
            d_plot->setAxisScaleDiv(targetAxis, div);
        };

        applyFormulaAxis(dstPrimary);
        applyFormulaAxis(dstSecondary);
    };

    const int srcX = parentPlot->axisEnabled(QwtPlot::xBottom) ? QwtPlot::xBottom : QwtPlot::xTop;
    syncOneLinkedPair(d_x_link_mode, d_x_link_expr, srcX, QwtPlot::xTop, QwtPlot::xBottom);

    const int srcY = parentPlot->axisEnabled(QwtPlot::yLeft) ? QwtPlot::yLeft : QwtPlot::yRight;
    syncOneLinkedPair(d_y_link_mode, d_y_link_expr, srcY, QwtPlot::yRight, QwtPlot::yLeft);

    if ((d_x_link_mode == 1 || d_x_link_mode == 2) && d_y_link_mode == 0
        && !hasColorScaleOnAxis(QwtPlot::yRight)) {
        bool hasRightYItem = false;
        for (int i = 0; i < n_curves; ++i) {
            if (QwtPlotItem *it = plotItem(i)) {
                if (it->yAxis() == QwtPlot::yRight) {
                    hasRightYItem = true;
                    break;
                }
            }
        }
        const QwtScaleDraw *sdRight = d_plot->axisScaleDraw(QwtPlot::yRight);
        const bool rightHasLabels = d_plot->axisEnabled(QwtPlot::yRight) && sdRight
                && sdRight->hasComponent(QwtAbstractScaleDraw::Labels);
        if (!hasRightYItem && !rightHasLabels) {
            if (d_plot->axisEnabled(QwtPlot::yRight))
                d_plot->enableAxis(QwtPlot::yRight, false);
            setAxisBackboneVisible(QwtPlot::yRight, false);
        }
    }

    syncCustomAndWeibullAxes();
    d_zoomer[0]->setZoomBase();
    d_zoomer[1]->setZoomBase();
    alignLinkedChildCanvasToParent(this);
    d_plot->replot();
    d_syncing_link = false;
}

void Graph::syncChildLinkedLayers()
{
    if (d_syncing_link)
        return;

    MultiLayer *ml = multiLayerParent();
    if (!ml)
        return;

    struct ScopedSyncFlag {
        bool &flag;
        explicit ScopedSyncFlag(bool &f) : flag(f) { flag = true; }
        ~ScopedSyncFlag() { flag = false; }
    } syncGuard(d_syncing_link);

    const QList<Graph *> allGraphs = ml->layersList();
    for (Graph *g : allGraphs) {
        if (g && g != this && g->linkedParentLayer() == this) {
            g->syncLinkedAxesFromParent();
            g->syncCustomAndWeibullAxes();
        }
    }
}

// ============================================================================
// OriginPro Phase 5: Piecewise Axis Breaks, Tick Transforms, Shaded Bands,
// Non-Destructive Stack Offsets & Lossless Phase 5 Persistence
// ============================================================================

namespace {

class ShadedBandPlotItem : public QwtPlotItem
{
public:
    ShadedBandPlotItem(const QVector<double> &x, const QVector<double> &yLow,
                       const QVector<double> &yHigh, const QColor &fillColor,
                       const QString &bandTitle)
        : d_x(x), d_yLow(yLow), d_yHigh(yHigh), d_color(fillColor)
    {
        setZ(15.0);
        setTitle(bandTitle);
        setItemAttribute(QwtPlotItem::AutoScale, true);
        setItemAttribute(QwtPlotItem::Legend, !bandTitle.trimmed().isEmpty());
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 43; }

    QwtDoubleRect boundingRect() const override
    {
        const int n = qMin(d_x.size(), qMin(d_yLow.size(), d_yHigh.size()));
        if (n <= 0)
            return QwtDoubleRect(0.0, 0.0, -1.0, -1.0);
        double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
        bool first = true;
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(d_x[i]) || !std::isfinite(d_yLow[i]) || !std::isfinite(d_yHigh[i]))
                continue;
            const double lo = qMin(d_yLow[i], d_yHigh[i]);
            const double hi = qMax(d_yLow[i], d_yHigh[i]);
            if (first) {
                minX = maxX = d_x[i];
                minY = lo;
                maxY = hi;
                first = false;
            } else {
                minX = qMin(minX, d_x[i]);
                maxX = qMax(maxX, d_x[i]);
                minY = qMin(minY, lo);
                maxY = qMax(maxY, hi);
            }
        }
        if (first)
            return QwtDoubleRect(0.0, 0.0, -1.0, -1.0);
        return QwtDoubleRect(minX, minY, qMax(1e-15, maxX - minX), qMax(1e-15, maxY - minY));
    }

    void draw(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
              const QRect &) const override
    {
        if (!painter)
            return;
        const int n = qMin(d_x.size(), qMin(d_yLow.size(), d_yHigh.size()));
        if (n < 2)
            return;

        const bool logX = xMap.transformation()
                && xMap.transformation()->type() == QwtScaleTransformation::Log10;
        const bool logY = yMap.transformation()
                && yMap.transformation()->type() == QwtScaleTransformation::Log10;
        const double logYFloor = logY ? qMax(1e-300, 0.1 * qMin(qMax(1e-300, yMap.s1()),
                                                                qMax(1e-300, yMap.s2())))
                                      : 0.0;

        auto isValidBandPt = [&](int idx) -> bool {
            if (!std::isfinite(d_x[idx]) || !std::isfinite(d_yLow[idx])
                || !std::isfinite(d_yHigh[idx])) {
                return false;
            }
            if (logX && d_x[idx] <= 0.0)
                return false;
            if (logY && qMax(d_yLow[idx], d_yHigh[idx]) <= 0.0)
                return false;
            return true;
        };

        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(d_color));

        int i = 0;
        while (i < n) {
            while (i < n && !isValidBandPt(i))
                ++i;
            const int segStart = i;
            while (i < n && isValidBandPt(i))
                ++i;
            const int segEnd = i - 1;
            if (segEnd - segStart >= 1) {
                QPolygonF poly;
                poly.reserve(2 * (segEnd - segStart + 1));
                for (int k = segStart; k <= segEnd; ++k) {
                    const double yHiVal = logY ? qMax(d_yHigh[k], logYFloor) : d_yHigh[k];
                    const double px = xMap.xTransform(d_x[k]);
                    const double py = yMap.xTransform(yHiVal);
                    if (std::isfinite(px) && std::isfinite(py))
                        poly.append(QPointF(px, py));
                }
                for (int k = segEnd; k >= segStart; --k) {
                    const double yLoVal = logY ? qMax(d_yLow[k], logYFloor) : d_yLow[k];
                    const double px = xMap.xTransform(d_x[k]);
                    const double py = yMap.xTransform(yLoVal);
                    if (std::isfinite(px) && std::isfinite(py))
                        poly.append(QPointF(px, py));
                }
                if (poly.size() >= 3)
                    painter->drawPolygon(poly);
            }
        }
        painter->restore();
    }

private:
    QVector<double> d_x;
    QVector<double> d_yLow;
    QVector<double> d_yHigh;
    QColor d_color;
};

QString unescapePhase5Attr(const QString &enc)
{
    QString out;
    out.reserve(enc.size());
    int i = 0;
    const int n = enc.size();
    while (i < n) {
        if (enc[i] == '&') {
            const int semi = enc.indexOf(';', i + 1);
            if (semi > i + 1 && semi - i <= 10) {
                const QString ent = enc.mid(i + 1, semi - i - 1);
                if (ent == "amp") {
                    out += '&';
                    i = semi + 1;
                    continue;
                } else if (ent == "quot") {
                    out += '"';
                    i = semi + 1;
                    continue;
                } else if (ent == "apos") {
                    out += '\'';
                    i = semi + 1;
                    continue;
                } else if (ent == "lt") {
                    out += '<';
                    i = semi + 1;
                    continue;
                } else if (ent == "gt") {
                    out += '>';
                    i = semi + 1;
                    continue;
                } else if (ent.startsWith("#x") || ent.startsWith("#X")) {
                    bool ok = false;
                    const uint code = ent.mid(2).toUInt(&ok, 16);
                    if (ok && code <= 0xFFFF) {
                        out += QChar(ushort(code));
                        i = semi + 1;
                        continue;
                    }
                } else if (ent.startsWith('#')) {
                    bool ok = false;
                    const uint code = ent.mid(1).toUInt(&ok, 10);
                    if (ok && code <= 0xFFFF) {
                        out += QChar(ushort(code));
                        i = semi + 1;
                        continue;
                    }
                }
            }
        }
        out += enc[i];
        ++i;
    }
    return out;
}

QMap<QString, QString> parsePhase5XmlAttrs(const QString &line)
{
    QMap<QString, QString> attrs;
    QRegExp rx("([A-Za-z0-9_]+)\\s*=\\s*\"([^\"]*)\"");
    int pos = 0;
    while ((pos = rx.indexIn(line, pos)) != -1) {
        attrs.insert(rx.cap(1), unescapePhase5Attr(rx.cap(2)));
        pos += rx.matchedLength();
    }
    return attrs;
}

} // namespace

void Graph::setAxisBreak(int axis, const AxisBreakSpec &spec)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    AxisBreakSpec norm = spec;
    if (norm.breakFrom > norm.breakTo)
        std::swap(norm.breakFrom, norm.breakTo);
    norm.positionPct = qBound(0.10, norm.positionPct, 0.90);

    if (!norm.enabled) {
        // clearAxisBreak() needs the previous (enabled) spec to restore a Log10 axis.
        clearAxisBreak(axis);
        d_axis_breaks[axis] = norm;
        return;
    }

    d_axis_breaks[axis] = norm;
    d_suspended_axis_breaks[axis] = AxisBreakSpec();

    const QwtScaleDiv *oldDiv = d_plot->axisScaleDiv(axis);
    const QwtScaleEngine *oldEng = d_plot->axisScaleEngine(axis);
    const double lo = oldDiv ? qMin(oldDiv->lowerBound(), oldDiv->upperBound()) : 0.0;
    const double hi = oldDiv ? qMax(oldDiv->lowerBound(), oldDiv->upperBound()) : 100.0;
    const bool inv = oldEng ? oldEng->testAttribute(QwtScaleEngine::Inverted) : false;

    const bool validBreak = (norm.breakFrom > lo && norm.breakTo < hi
                             && norm.breakTo > norm.breakFrom + 1e-15)
            && (!norm.log10Before || (lo > 0.0 && norm.breakFrom > 0.0))
            && (!norm.log10After || (norm.breakTo > 0.0 && hi > 0.0));
    if (oldDiv && oldDiv->isValid() && !validBreak) {
        d_suspended_axis_breaks[axis] = norm;
        d_axis_breaks[axis].enabled = false;
        if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
            sd->setAxisBreakIndicator(false, 0.0);
        const int mirrorAxis = (axis == QwtPlot::xBottom)
                ? QwtPlot::xTop
                : ((axis == QwtPlot::yLeft) ? QwtPlot::yRight : -1);
        if (mirrorAxis >= 0 && !d_axis_breaks[mirrorAxis].enabled) {
            if (ScaleDraw *msd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(mirrorAxis)))
                msd->setAxisBreakIndicator(false, 0.0);
        }
        if (QWidget *tw = window()) {
            if (QMainWindow *mw = qobject_cast<QMainWindow *>(tw)) {
                if (mw->statusBar())
                    mw->statusBar()->showMessage(
                            tr("Axis break is outside the current axis range; it is kept "
                               "and will appear once the range contains it."),
                            6000);
            }
        }
        d_plot->replot();
        emit modifiedGraph();
        return;
    }

    int maj = d_plot->axisMaxMajor(axis);
    if (axisFont(axis).pointSize() >= 20)
        maj = qMin(maj, 5);
    const int minT = d_plot->axisMaxMinor(axis);
    int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
    if (norm.log10Before || norm.log10After) {
        maxMinInt = qMax(9, maxMinInt);
        if (minT < 8)
            d_plot->setAxisMaxMinor(axis, 8);
    }

    PiecewiseBreakScaleEngine *eng = new PiecewiseBreakScaleEngine(norm);
    QwtScaleDiv div = eng->divideScale(lo, hi, maj, maxMinInt, d_user_step[axis]);
    if (inv) {
        eng->setAttribute(QwtScaleEngine::Inverted);
        div.invert();
    }

    d_plot->setAxisScaleEngine(axis, eng);
    d_plot->setAxisScaleDiv(axis, div);

    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        sd->setAxisBreakIndicator(true, norm.breakFrom, norm.breakTo, norm.log10Before,
                                  norm.log10After);
    }

    if (axis == QwtPlot::xBottom)
        updateSecondaryAxis(QwtPlot::xTop);
    else if (axis == QwtPlot::yLeft)
        updateSecondaryAxis(QwtPlot::yRight);

    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

Graph::AxisBreakSpec Graph::axisBreak(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return AxisBreakSpec();
    return d_axis_breaks[axis];
}

Graph::AxisBreakSpec Graph::suspendedAxisBreak(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return AxisBreakSpec();
    return d_suspended_axis_breaks[axis];
}

bool Graph::axisBreakSuspended(int axis) const
{
    return axis >= 0 && axis < QwtPlot::axisCnt && !d_axis_breaks[axis].enabled
            && d_suspended_axis_breaks[axis].enabled;
}

void Graph::refreshAxisBreakIndicator(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt || !d_plot)
        return;
    ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis));
    if (!sd)
        return;
    const AxisBreakSpec *brk = nullptr;
    if (d_axis_breaks[axis].enabled) {
        brk = &d_axis_breaks[axis];
    } else {
        const int primary = (axis == QwtPlot::xTop)
                ? QwtPlot::xBottom
                : ((axis == QwtPlot::yRight) ? QwtPlot::yLeft : -1);
        if (primary >= 0 && d_axis_breaks[primary].enabled && !hasColorScaleOnAxis(axis))
            brk = &d_axis_breaks[primary];
    }
    if (!brk) {
        sd->setAxisBreakIndicator(false, 0.0);
        return;
    }
    const QwtScaleDiv *div = d_plot->axisScaleDiv(axis);
    const double lo = div ? qMin(div->lowerBound(), div->upperBound()) : 0.0;
    const double hi = div ? qMax(div->lowerBound(), div->upperBound()) : 0.0;
    const double bf = qMin(brk->breakFrom, brk->breakTo);
    const double bt = qMax(brk->breakFrom, brk->breakTo);
    const bool straddles = (bf > lo && bt < hi && bt > bf + 1e-15);
    sd->setAxisBreakIndicator(straddles, bf, bt, brk->log10Before, brk->log10After);
}

void Graph::clearAxisBreak(int axis)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    // Restore the logarithmic scale type when the broken axis was log on both sides
    // (previously clearing a break always forced a linear axis).
    const AxisBreakSpec prevBrk =
            d_axis_breaks[axis].enabled ? d_axis_breaks[axis] : d_suspended_axis_breaks[axis];
    const bool prevWasBreak = d_axis_breaks[axis].enabled || d_suspended_axis_breaks[axis].enabled;
    d_axis_breaks[axis].enabled = false;
    d_suspended_axis_breaks[axis] = AxisBreakSpec();
    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis)))
        sd->setAxisBreakIndicator(false, 0.0);

    const QwtScaleDiv *oldDiv = d_plot->axisScaleDiv(axis);
    const QwtScaleEngine *oldEng = d_plot->axisScaleEngine(axis);
    const double lo = oldDiv ? qMin(oldDiv->lowerBound(), oldDiv->upperBound()) : 0.0;
    const double hi = oldDiv ? qMax(oldDiv->lowerBound(), oldDiv->upperBound()) : 100.0;
    const bool inv = oldEng ? oldEng->testAttribute(QwtScaleEngine::Inverted) : false;
    bool wasLog = prevWasBreak ? (prevBrk.log10Before && prevBrk.log10After)
                               : (oldEng && Plot::isLog10ScaleEngine(oldEng));
    if (wasLog && !(lo > 0.0 && hi > 0.0))
        wasLog = false;

    setScale(axis, lo, hi, d_user_step[axis], d_plot->axisMaxMajor(axis),
             d_plot->axisMaxMinor(axis), wasLog ? 1 : 0, inv);

    const int mirrorAxis = (axis == QwtPlot::xBottom)
            ? QwtPlot::xTop
            : ((axis == QwtPlot::yLeft) ? QwtPlot::yRight : -1);
    if (mirrorAxis >= 0 && !d_axis_breaks[mirrorAxis].enabled) {
        if (ScaleDraw *msd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(mirrorAxis)))
            msd->setAxisBreakIndicator(false, 0.0);
    }

    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setTickLabelTransform(int axis, double divideByFactor, const QString &prefix,
                                  const QString &suffix)
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return;

    if (!std::isfinite(divideByFactor) || qAbs(divideByFactor) <= 1e-300)
        divideByFactor = 1.0;

    d_tick_divide_factor[axis] = divideByFactor;
    d_tick_prefix[axis] = prefix;
    d_tick_suffix[axis] = suffix;

    if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(axis))) {
        sd->setTickLabelTransform(divideByFactor, prefix, suffix);
    }
    d_plot->updateLayout();
    if (QwtScaleWidget *sw = d_plot->axisWidget(axis))
        sw->repaint();
    d_plot->replot();
    emit modifiedGraph();
}

double Graph::tickDivideFactor(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return 1.0;
    return d_tick_divide_factor[axis];
}

QString Graph::tickPrefix(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return d_tick_prefix[axis];
}

QString Graph::tickSuffix(int axis) const
{
    if (axis < 0 || axis >= QwtPlot::axisCnt)
        return QString();
    return d_tick_suffix[axis];
}

QwtPlotItem *Graph::addShadedBand(const QVector<double> &x, const QVector<double> &yLow,
                                  const QVector<double> &yHigh, const QColor &fillColor,
                                  const QString &title)
{
    if (!d_plot)
        return nullptr;
    const int n = qMin(x.size(), qMin(yLow.size(), yHigh.size()));
    if (n < 2)
        return nullptr;

    ShadedBandPlotItem *item = new ShadedBandPlotItem(x, yLow, yHigh, fillColor, title);
    item->attach(d_plot);

    ShadedBandSpec spec;
    spec.x = x;
    spec.yLow = yLow;
    spec.yHigh = yHigh;
    spec.fillColor = fillColor;
    spec.title = title;
    spec.item = item;
    d_shaded_bands.append(spec);

    d_plot->replot();
    emit modifiedGraph();
    return item;
}

void Graph::clearShadedBands()
{
    if (d_shaded_bands.isEmpty())
        return;
    for (ShadedBandSpec &spec : d_shaded_bands) {
        if (spec.item) {
            spec.item->detach();
            delete spec.item;
            spec.item = nullptr;
        }
    }
    d_shaded_bands.clear();
    if (d_plot)
        d_plot->replot();
    emit modifiedGraph();
}

void Graph::removeShadedBandByTitle(const QString &title)
{
    if (title.isEmpty() || d_shaded_bands.isEmpty())
        return;
    bool removed = false;
    for (int i = d_shaded_bands.size() - 1; i >= 0; --i) {
        if (d_shaded_bands[i].title == title) {
            if (d_shaded_bands[i].item) {
                d_shaded_bands[i].item->detach();
                delete d_shaded_bands[i].item;
            }
            d_shaded_bands.removeAt(i);
            removed = true;
        }
    }
    if (removed) {
        if (d_plot)
            d_plot->replot();
        emit modifiedGraph();
    }
}

void Graph::setStackOffsets(int mode, double yGapPct, double xGapPct, bool normalize01)
{
    QList<PlotCurve *> targetCurves;
    for (int i = 0; i < n_curves; ++i) {
        if (c_type[i] == ErrorBars || c_type[i] == Pie || c_type[i] == Box
            || c_type[i] == ColorMap || c_type[i] == GrayMap || c_type[i] == ContourMap)
            continue;
        PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(i));
        if (pc && pc->isVisible() && pc->dataSize() > 0)
            targetCurves.append(pc);
    }
    if (targetCurves.isEmpty())
        return;

    // First reset to clean raw unstacked values (with optional 0..1 normalization)
    for (PlotCurve *pc : targetCurves) {
        pc->setStackOffset(0.0, 0.0);
        pc->setStackNormalize01(normalize01);
    }

    if (targetCurves.size() < 2) {
        if (m_autoscale) {
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                d_plot->setAxisAutoScale(a);
        }
        updatePlot();
        emit modifiedGraph();
        return;
    }

    const int m = targetCurves.size();
    QVector<double> minY(m, 0.0), maxY(m, 0.0), spanY(m, 0.0);
    double sumSpanY = 0.0;
    int validSpansY = 0;
    double maxSpanX = 0.0;

    for (int k = 0; k < m; ++k) {
        PlotCurve *pc = targetCurves[k];
        DataCurve *dc = dynamic_cast<DataCurve *>(pc);
        const int n = pc->dataSize();
        Column *dcXCol = nullptr;
        Column *dcYCol = nullptr;
        const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
        if (dcHasMasks)
            dc->resolveColumns(dcXCol, dcYCol);
        bool first = true;
        double loX = 0.0, hiX = 0.0, loY = 0.0, hiY = 0.0;
        for (int j = 0; j < n; ++j) {
            if (dcHasMasks && dc->isPointMaskedFast(j, dcXCol, dcYCol))
                continue;
            const double xv = pc->x(j);
            const double yv = pc->y(j);
            if (!std::isfinite(xv) || !std::isfinite(yv))
                continue;
            if (first) {
                loX = hiX = xv;
                loY = hiY = yv;
                first = false;
            } else {
                loX = qMin(loX, xv);
                hiX = qMax(hiX, xv);
                loY = qMin(loY, yv);
                hiY = qMax(hiY, yv);
            }
        }
        minY[k] = loY;
        maxY[k] = hiY;
        spanY[k] = qMax(0.0, hiY - loY);
        if (!first) {
            sumSpanY += spanY[k];
            validSpansY++;
            maxSpanX = qMax(maxSpanX, hiX - loX);
        }
    }

    const double avgSpanY =
            (validSpansY > 0 && sumSpanY > 1e-12) ? (sumSpanY / double(validSpansY)) : 1.0;
    const double refSpanX = (maxSpanX > 1e-12) ? maxSpanX : 1.0;
    const double stepX =
            (qAbs(xGapPct) > 1e-12) ? (refSpanX * (xGapPct / 100.0)) : 0.0;

    double prevTop = maxY[0];
    for (int k = 1; k < m; ++k) {
        PlotCurve *pc = targetCurves[k];
        const double dx = double(k) * stepX;
        double dy = 0.0;
        if (mode == 1) {
            // AutoGap: each curve sits cleanly above the previous curve with yGapPct% margin
            dy = (prevTop - minY[k]) + (yGapPct / 100.0) * avgSpanY;
            prevTop = maxY[k] + dy;
        } else {
            // Uniform: constant vertical step across all curves
            if (qAbs(yGapPct) <= 1e-12 && qAbs(xGapPct) > 1e-12)
                dy = 0.0;
            else
                dy = double(k) * avgSpanY * (1.0 + yGapPct / 100.0);
        }
        pc->setStackOffset(dx, dy);

        if (DataCurve *dc = dynamic_cast<DataCurve *>(pc)) {
            const int n = dc->dataSize();
            QVector<double> xv(n), yv(n);
            for (int j = 0; j < n; ++j) {
                xv[j] = dc->x(j);
                yv[j] = dc->y(j);
            }
            for (DataCurve *eb : dc->errorBarsList()) {
                if (eb)
                    eb->setData(xv.constData(), yv.constData(), n);
            }
        }
    }

    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        setToriAxisUserLocked(d_plot, a, false);
        d_plot->setAxisAutoScale(a);
    }
    updatePlot();

    {
        double overallMinY = 1e300;
        double overallMaxY = -1e300;
        for (PlotCurve *pc : targetCurves) {
            DataCurve *dc = dynamic_cast<DataCurve *>(pc);
            const int n = pc->dataSize();
            Column *dcXCol = nullptr;
            Column *dcYCol = nullptr;
            const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
            if (dcHasMasks)
                dc->resolveColumns(dcXCol, dcYCol);
            for (int j = 0; j < n; ++j) {
                if (dcHasMasks && dc->isPointMaskedFast(j, dcXCol, dcYCol))
                    continue;
                const double yv = pc->y(j);
                if (std::isfinite(yv)) {
                    overallMinY = qMin(overallMinY, yv);
                    overallMaxY = qMax(overallMaxY, yv);
                }
            }
        }
        const QwtScaleDiv *yDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
        const QwtScaleEngine *yEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
        if (yDiv && yEng && !Plot::isLog10ScaleEngine(yEng)
            && !yEng->testAttribute(QwtScaleEngine::Inverted)
            && !d_axis_breaks[QwtPlot::yLeft].enabled && overallMaxY > 0.0
            && overallMinY >= -0.04 * overallMaxY) {
            const double autoLo = yDiv->lowerBound();
            const double autoHi = yDiv->upperBound();
            if (autoHi > 0.0 && autoLo < -0.035 * autoHi) {
                const double clampedLo = (overallMinY >= 0.0) ? 0.0 : (-0.03 * autoHi);
                const QwtValueList majTicks = yDiv->ticks(QwtScaleDiv::MajorTick);
                const double step = (majTicks.size() >= 2)
                        ? std::fabs(majTicks[1] - majTicks[0])
                        : d_user_step[QwtPlot::yLeft];
                const int maj = d_plot->axisMaxMajor(QwtPlot::yLeft);
                const int minT = d_plot->axisMaxMinor(QwtPlot::yLeft);
                const int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
                QwtLinearScaleEngine *newEng = new QwtLinearScaleEngine();
                QwtScaleDiv newDiv = newEng->divideScale(clampedLo, autoHi, maj, maxMinInt, step);
                if (minT == 1)
                    ensureLinearHalfStepMinorTicks(newDiv);
                d_plot->setAxisScale(QwtPlot::yLeft, clampedLo, autoHi, step);
                d_plot->setAxisScaleEngine(QwtPlot::yLeft, newEng);
                d_plot->setAxisScaleDiv(QwtPlot::yLeft, newDiv);
                updateSecondaryAxis(QwtPlot::yRight);
                d_zoomer[0]->setZoomBase();
                d_plot->replot();
                syncChildLinkedLayers();
            }
        }
    }

    emit modifiedGraph();
}

void Graph::resetStackOffsets()
{
    bool changed = false;
    for (int i = 0; i < n_curves; ++i) {
        PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(i));
        if (!pc)
            continue;
        if (qAbs(pc->stackOffsetX()) > 1e-15 || qAbs(pc->stackOffsetY()) > 1e-15
            || pc->stackNormalize01()) {
            pc->setStackNormalize01(false);
            pc->setStackOffset(0.0, 0.0);
            changed = true;
            if (DataCurve *dc = dynamic_cast<DataCurve *>(pc)) {
                const int n = dc->dataSize();
                QVector<double> xv(n), yv(n);
                for (int j = 0; j < n; ++j) {
                    xv[j] = dc->x(j);
                    yv[j] = dc->y(j);
                }
                for (DataCurve *eb : dc->errorBarsList()) {
                    if (eb)
                        eb->setData(xv.constData(), yv.constData(), n);
                }
            }
        }
    }
    if (changed) {
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            setToriAxisUserLocked(d_plot, a, false);
            d_plot->setAxisAutoScale(a);
        }
        updatePlot();
        emit modifiedGraph();
    }
}

bool Graph::restorePhase5Tag(const QString &line)
{
    const QString s = line.trimmed();
    if (!s.startsWith("<Tori"))
        return false;

    const QMap<QString, QString> attrs = parsePhase5XmlAttrs(s);

    if (s.startsWith("<ToriDensity2D") || s.startsWith("<ToriPoleFigure")) // E11b
        return ExtraPlotTypes::restoreGraphTag(this, s);

    if (s.startsWith("<ToriLabelShifts")) {
        if (d_plot) {
            const double titleDx = attrs.value("titleDx").toDouble();
            const double titleDy = attrs.value("titleDy").toDouble();
            if (std::fabs(titleDx) > 1e-6 || std::fabs(titleDy) > 1e-6)
                d_plot->setProperty("_toriPlotTitleShift",
                                    QPointF(qBound(-500.0, titleDx, 500.0),
                                            qBound(-500.0, titleDy, 500.0)));
            else
                d_plot->setProperty("_toriPlotTitleShift", QVariant());

            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                QwtScaleWidget *sw = d_plot->axisWidget(a);
                if (!sw)
                    continue;
                const double axTDx = attrs.value(QString("ax%1TitleDx").arg(a)).toDouble();
                const double axTDy = attrs.value(QString("ax%1TitleDy").arg(a)).toDouble();
                if (std::fabs(axTDx) > 1e-6 || std::fabs(axTDy) > 1e-6)
                    sw->setProperty("_toriAxisTitleShift",
                                    QPointF(qBound(-500.0, axTDx, 500.0),
                                            qBound(-500.0, axTDy, 500.0)));
                else
                    sw->setProperty("_toriAxisTitleShift", QVariant());

                const double axNorm = attrs.value(QString("ax%1Norm").arg(a)).toDouble();
                if (std::fabs(axNorm) > 1e-6)
                    sw->setProperty("_toriAxisNormalShift", qBound(-500.0, axNorm, 500.0));
                else
                    sw->setProperty("_toriAxisNormalShift", QVariant());

                const double axPar = attrs.value(QString("ax%1Par").arg(a)).toDouble();
                if (std::fabs(axPar) > 1e-6)
                    sw->setProperty("_toriAxisParallelShift", qBound(-500.0, axPar, 500.0));
                else
                    sw->setProperty("_toriAxisParallelShift", QVariant());
            }
            d_plot->updateLayout();
        }
        return true;
    }

    if (s.startsWith("<ToriBackbones")) {
        const bool anyDraw = attrs.value("draw", "1").toInt() != 0;
        drawAxesBackbone = anyDraw;
        for (int a = 0; a < 4; ++a) {
            const QString key = QString("b%1").arg(a);
            if (attrs.contains(key)) {
                const bool vis = anyDraw && (attrs.value(key).toInt() != 0);
                d_axis_backbone[a] = vis;
                if (ScaleDraw *sd = dynamic_cast<ScaleDraw *>(d_plot->axisScaleDraw(a)))
                    sd->enableComponent(QwtAbstractScaleDraw::Backbone, vis);
            }
        }
        if (attrs.contains("legendCols")) {
            const int cols = qMax(1, attrs.value("legendCols").toInt());
            if (Legend *leg = legend())
                leg->setNumColumns(cols);
        }
        if (attrs.contains("outsideRightMargin")) {
            const int orm = qMax(0, attrs.value("outsideRightMargin").toInt());
            d_plot->setProperty("_outsideRightMargin", orm);
        }
        if (attrs.contains("legendAnchor")) {
            d_plot->setProperty("_toriLegendAnchor", attrs.value("legendAnchor").toInt());
        }
        if (attrs.contains("legendVisible")) {
            d_legend_visible = (attrs.value("legendVisible").toInt() != 0);
            if (Legend *leg = legend())
                leg->setVisible(d_legend_visible);
            if (!d_legend_visible && d_plot->property("_outsideRightMargin").toInt() > 0) {
                d_plot->setProperty("_savedOutsideRightMargin",
                                    d_plot->property("_outsideRightMargin").toInt());
                d_plot->setProperty("_outsideRightMargin", 0);
            }
        }
        const QString textCols = attrs.value("textCols");
        if (!textCols.isEmpty()) {
            for (const QString &pair : textCols.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = pair.split(':');
                if (kv.size() == 2) {
                    const int ti = kv[0].toInt();
                    const int cols = qMax(1, kv[1].toInt());
                    if (ti >= 0 && ti < d_texts.size()) {
                        if (Legend *tm = dynamic_cast<Legend *>(d_plot->marker(d_texts[ti])))
                            tm->setNumColumns(cols);
                    }
                }
            }
        }
        const QString textPts = attrs.value("textPts");
        if (!textPts.isEmpty()) {
            for (const QString &pair : textPts.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = pair.split(':');
                if (kv.size() != 2)
                    continue;
                const int ti = kv[0].toInt();
                const double tpt = kv[1].toDouble();
                if (ti >= 0 && ti < d_texts.size() && tpt > 0.0 && tpt <= 1000.0) {
                    if (Legend *tm = dynamic_cast<Legend *>(d_plot->marker(d_texts[ti]))) {
                        QFont tf = tm->font();
                        tf.setPointSizeF(tpt);
                        tm->setFont(tf);
                    }
                }
            }
        }
        const QString curveAlphas = attrs.value("curveAlphas");
        if (!curveAlphas.isEmpty()) {
            for (const QString &entry : curveAlphas.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = entry.split(':');
                if (kv.size() == 2) {
                    const int ci = kv[0].toInt();
                    const QStringList rgba = kv[1].split(',');
                    QwtPlotCurve *cv = curve(ci);
                    if (cv && rgba.size() == 4) {
                        QPen p = cv->pen();
                        QColor lc = p.color();
                        lc.setAlpha(qBound(0, rgba[0].toInt(), 255));
                        p.setColor(lc);
                        cv->setPen(p);

                        QBrush br = cv->brush();
                        QColor bc = br.color();
                        bc.setAlpha(qBound(0, rgba[1].toInt(), 255));
                        br.setColor(bc);
                        cv->setBrush(br);

                        QwtSymbol sym = cv->symbol();
                        QPen sp = sym.pen();
                        QColor spc = sp.color();
                        spc.setAlpha(qBound(0, rgba[2].toInt(), 255));
                        sp.setColor(spc);
                        sym.setPen(sp);
                        QBrush sb = sym.brush();
                        if (sb.style() != Qt::NoBrush) {
                            QColor sbc = sb.color();
                            sbc.setAlpha(qBound(0, rgba[3].toInt(), 255));
                            sb.setColor(sbc);
                            sym.setBrush(sb);
                        }
                        cv->setSymbol(sym);
                    }
                }
            }
        }
        const QString curveWidths = attrs.value("curveWidths");
        if (!curveWidths.isEmpty()) {
            for (const QString &entry : curveWidths.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = entry.split(':');
                if (kv.size() == 2) {
                    const int ci = kv[0].toInt();
                    const QStringList wv = kv[1].split(',');
                    if (wv.size() < 2)
                        continue;
                    QwtPlotCurve *cv = nullptr;
                    int resolvedIdx = -1;
                    bool titleMatched = false;
                    const bool hasTitleKey = (wv.size() >= 3 && !wv[2].isEmpty());
                    if (hasTitleKey) {
                        const QString wantTitle =
                                QString::fromUtf8(QByteArray::fromBase64(wv[2].toLatin1()));
                        if (QwtPlotCurve *cand = curve(ci)) {
                            if (cand->title().text() == wantTitle) {
                                cv = cand;
                                resolvedIdx = ci;
                                titleMatched = true;
                            }
                        }
                        if (!cv) {
                            for (int k = 0; k < curves(); ++k) {
                                if (QwtPlotCurve *cand = curve(k)) {
                                    if (cand->title().text() == wantTitle) {
                                        cv = cand;
                                        resolvedIdx = k;
                                        titleMatched = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (!cv) {
                        // Fallback when no title key is stored or appendProject renamed the source table
                        cv = curve(ci);
                        resolvedIdx = ci;
                    }
                    if (cv) {
                        bool okL = false, okS = false;
                        const double lw = wv[0].toDouble(&okL);
                        const double sw = wv[1].toDouble(&okS);
                        const int ct = curveType(resolvedIdx);
                        const bool symWidthInCurveLine = (ct <= LineSymbols || ct == Box);
                        if (okL && lw > 0.0 && lw <= 200.0
                            && (titleMatched || std::abs(qRound(lw) - cv->pen().width()) <= 1)) {
                            QPen p = cv->pen();
                            p.setWidthF(lw);
                            cv->setPen(p);
                        }
                        if (okS && sw > 0.0 && sw <= 200.0
                            && (titleMatched || !symWidthInCurveLine
                                || std::abs(qRound(sw) - cv->symbol().pen().width()) <= 1)) {
                            QwtSymbol sym = cv->symbol();
                            QPen sp = sym.pen();
                            sp.setWidthF(sw);
                            sym.setPen(sp);
                            cv->setSymbol(sym);
                        }
                    }
                }
            }
        }
        const QString stackOffsets = attrs.value("stackOffsets");
        if (!stackOffsets.isEmpty()) {
            for (const QString &entry : stackOffsets.split(';', Qt::SkipEmptyParts)) {
                const QStringList kv = entry.split(':');
                if (kv.size() == 2) {
                    const int ci = kv[0].toInt();
                    const QStringList vals = kv[1].split(',');
                    PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(ci));
                    if (pc && vals.size() >= 3) {
                        pc->setStackNormalize01(vals[2].toInt() != 0);
                        pc->setStackOffset(vals[0].toDouble(), vals[1].toDouble());
                    }
                }
            }
        }
        for (QwtPlotItem *it : d_plot->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                Spectrogram *sp = static_cast<Spectrogram *>(it);
                sp->syncColorScale();
                sp->enforceColorBarArialFont();
            }
        }
        syncCurveZOrder();
        d_plot->replot();
        return true;
    }

    if (s.startsWith("<ToriCurveAlpha")) {
        const int ci = attrs.contains("idx") ? attrs.value("idx", "-1").toInt()
                                             : attrs.value("curve", "-1").toInt();
        if (ci >= 0 && ci < n_curves) {
            if (PlotCurve *pc = dynamic_cast<PlotCurve *>(curve(ci))) {
                if (attrs.contains("symSkip"))
                    pc->setSymbolSkip(qMax(0, attrs.value("symSkip").toInt()));
            }
        }
        return true;
    }

    if (s.startsWith("<ToriTemplateCurveStyle")) {
        TemplateCurveStyle st;
        st.curveType = attrs.value("type", QString::number(int(LineSymbols))).toInt();
        st.penStyle = attrs.value("penStyle", QString::number(int(Qt::SolidLine))).toInt();
        st.lineWidth = attrs.value("lineWidth", "1.5").toDouble();
        if (attrs.contains("color")) {
            QColor c(attrs.value("color"));
            if (c.isValid())
                st.lineColor = c;
        }
        st.brushStyle = attrs.value("brushStyle", QString::number(int(Qt::NoBrush))).toInt();
        if (attrs.contains("brushColor")) {
            QColor c(attrs.value("brushColor"));
            if (c.isValid())
                st.brushColor = c;
        }
        st.symbolStyle = attrs.value("symbol", QString::number(int(QwtSymbol::NoSymbol))).toInt();
        st.symbolSize = attrs.value("symSize", "7").toInt();
        st.symbolPenWidth = attrs.value("symWidth", "1.0").toDouble();
        if (attrs.contains("symColor")) {
            QColor c(attrs.value("symColor"));
            if (c.isValid())
                st.symbolEdgeColor = c;
        }
        if (attrs.contains("symFill")) {
            QColor c(attrs.value("symFill"));
            if (c.isValid())
                st.symbolFillColor = c;
        }
        st.symbolHollow = attrs.value("hollow", "0").toInt() != 0;
        st.symbolSkip = qMax(0, attrs.value("symSkip", "0").toInt());
        st.barGap = attrs.value("barGap", "0").toInt();
        st.barOffset = attrs.value("barOffset", "0").toInt();
        st.xAxis = attrs.value("xAxis", QString::number(int(QwtPlot::xBottom))).toInt();
        st.yAxis = attrs.value("yAxis", QString::number(int(QwtPlot::yLeft))).toInt();

        const int idx = attrs.value("idx", "-1").toInt();
        if (idx >= 0) {
            if (idx >= d_template_curve_styles.size())
                d_template_curve_styles.resize(idx + 1);
            d_template_curve_styles[idx] = st;
            if (idx < n_curves)
                applyTemplateCurveStyle(idx, idx);
        } else {
            d_template_curve_styles.append(st);
        }
        return true;
    }

    if (s.startsWith("<ToriAutoAxisTitles")) {
        const QString uf = attrs.value("unitFormat");
        if (uf == QStringLiteral("Paren"))
            d_axis_unit_format = Column::UnitFormat::Paren;
        else if (uf == QStringLiteral("Bracket"))
            d_axis_unit_format = kUnitFormatBracket;
        else
            d_axis_unit_format = Column::UnitFormat::ISIJSlash;
        for (int a = 0; a < 4; ++a) {
            const QString key = QString("a%1").arg(a);
            if (attrs.contains(key))
                d_axis_title_auto[a] = (attrs.value(key).toInt() != 0);
        }
        if (curves() > 0)
            refreshAutoAxisTitlesFromColumns();
        return true;
    }

    if (s.startsWith("<ToriErrorBand")) {
        const QString xName = attrs.value("x");
        const QString yName = attrs.value("y");
        const QString errName = attrs.value("err");
        for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
            if (c_type[i] != ErrorBars)
                continue;
            auto *er = dynamic_cast<QwtErrorPlotCurve *>(d_plot->plotItem(c_keys[i]));
            if (!er || er->isBand() || !er->masterCurve() || er->title().text() != errName
                || er->masterCurve()->title().text() != yName
                || er->masterCurve()->xColumnName() != xName)
                continue;
            er->setBand(true);
            er->setBandAlpha(attrs.value("alpha", "64").toInt());
            er->drawPlusSide(attrs.value("plus", "1").toInt() != 0);
            er->drawMinusSide(attrs.value("minus", "1").toInt() != 0);
            break;
        }
        syncCurveZOrder();
        return true;
    }

    if (s.startsWith("<ToriPointColorMap")) {
        const QString xName = attrs.value("x");
        const QString yName = attrs.value("y");
        for (int i = 0; i < n_curves && i < c_keys.size() && i < c_type.size(); ++i) {
            if (c_type[i] == ErrorBars)
                continue;
            auto *dc = dynamic_cast<DataCurve *>(d_plot->plotItem(c_keys[i]));
            if (!dc || dc->hasPointColorMap() || dc->title().text() != yName
                || dc->xColumnName() != xName)
                continue;
            DataCurve::PointColorMap pm;
            pm.enabled = true;
            pm.zColumn = attrs.value("z");
            pm.sizeColumn = attrs.value("size");
            pm.preset = attrs.value("preset", "2").toInt();
            pm.reverse = attrs.value("reverse", "0").toInt() != 0;
            pm.autoRange = attrs.value("auto", "1").toInt() != 0;
            pm.zMin = attrs.value("zmin", "0").toDouble();
            pm.zMax = attrs.value("zmax", "1").toDouble();
            pm.sizeMinPx = attrs.value("smin", "4").toDouble();
            pm.sizeMaxPx = attrs.value("smax", "16").toDouble();
            pm.showColorBar = (attrs.value("cb", "1") != "0");
            pm.colorBarTitle = attrs.value("title");
            dc->setPointColorMap(pm);
            break;
        }
        syncPointColorBar();
        return true;
    }

    if (s.startsWith("<ToriRefLine") || s.startsWith("<ToriShadedBand")) {
        if (s.startsWith("<ToriShadedBand") || attrs.value("kind") == "band") {
            const QStringList xs = attrs.value("x").split(',', Qt::SkipEmptyParts);
            const QStringList yls = attrs.value("yLow").split(',', Qt::SkipEmptyParts);
            const QStringList yhs = attrs.value("yHigh").split(',', Qt::SkipEmptyParts);
            const int m = qMin(xs.size(), qMin(yls.size(), yhs.size()));
            if (m >= 2) {
                QVector<double> xv(m), ylv(m), yhv(m);
                for (int k = 0; k < m; ++k) {
                    xv[k] = xs[k].toDouble();
                    ylv[k] = yls[k].toDouble();
                    yhv[k] = yhs[k].toDouble();
                }
                QColor col(attrs.value("color", "#371e66b8"));
                if (!col.isValid())
                    col = QColor(30, 102, 184, 55);
                addShadedBand(xv, ylv, yhv, col, attrs.value("title"));
            }
            return true;
        }

        ReferenceLineSpec rl;
        rl.orientation = attrs.value("orient", "0").toInt();
        rl.value = attrs.value("value", "0").toDouble();
        rl.statMode = attrs.value("stat");
        const QColor col(attrs.value("color", "#ff1e66b8"));
        if (col.isValid())
            rl.color = col;
        rl.style = static_cast<Qt::PenStyle>(attrs.value("style", "2").toInt());
        rl.width = attrs.value("width", "1.5").toDouble();
        rl.label = attrs.value("label");
        rl.fillToAxis = attrs.value("fill", "0").toInt() != 0;
        const QColor fcol(attrs.value("fillColor", "#201e66b8"));
        if (fcol.isValid())
            rl.fillColor = fcol;
        rl.visible = (attrs.value("visible", "1") != "0");
        addReferenceLine(rl);
        return true;
    }

    if (s.startsWith("<ToriLinkedLayer")) {
        d_pending_linked_parent_idx = attrs.value("parentIdx", "-1").toInt();
        d_x_link_mode = attrs.value("xMode", "0").toInt();
        d_x_link_expr = attrs.value("xExpr", "x");
        d_y_link_mode = attrs.value("yMode", "0").toInt();
        d_y_link_expr = attrs.value("yExpr", "y");
        return true;
    }

    if (s.startsWith("<ToriAxisBreak")) {
        const int axis = attrs.value("axis", "-1").toInt();
        if (axis >= 0 && axis < 4) {
            AxisBreakSpec ab;
            ab.enabled = attrs.value("enabled", "1").toInt() != 0;
            ab.breakFrom = attrs.value("from", "0").toDouble();
            ab.breakTo = attrs.value("to", "0").toDouble();
            ab.positionPct = attrs.value("pos", "0.5").toDouble();
            ab.stepBefore = attrs.value("stepBefore", "0").toDouble();
            ab.stepAfter = attrs.value("stepAfter", "0").toDouble();
            ab.log10Before = attrs.value("logBefore", "0").toInt() != 0;
            ab.log10After = attrs.value("logAfter", "0").toInt() != 0;
            if (ab.enabled && attrs.value("suspended", "0").toInt() != 0) {
                if (ab.breakFrom > ab.breakTo)
                    std::swap(ab.breakFrom, ab.breakTo);
                ab.positionPct = qBound(0.10, ab.positionPct, 0.90);
                d_axis_breaks[axis].enabled = false;
                d_suspended_axis_breaks[axis] = ab;
            } else {
                setAxisBreak(axis, ab);
            }
        }
        return true;
    }

    if (s.startsWith("<ToriTickTransform")) {
        const int axis = attrs.value("axis", "-1").toInt();
        if (axis >= 0 && axis < 4) {
            setTickLabelTransform(axis, attrs.value("factor", "1").toDouble(),
                                  attrs.value("prefix"), attrs.value("suffix"));
        }
        return true;
    }

    if (s.startsWith("<ToriTernary")) {
        d_is_ternary = attrs.value("enabled", "1").toInt() != 0;
        if (attrs.contains("a"))
            d_ternary_labels[0] = attrs.value("a");
        if (attrs.contains("b"))
            d_ternary_labels[1] = attrs.value("b");
        if (attrs.contains("c"))
            d_ternary_labels[2] = attrs.value("c");
        if (d_is_ternary) {
            m_autoscale = false;
            if (!d_ternary_item) {
                d_ternary_item = new TernaryDiagramItem(this);
                d_ternary_item->attach(d_plot);
            }
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                enableAxis(a, false);
            drawAxesBackbones(false);
        }
        return true;
    }

    if (s.startsWith("<ToriStackedBars")) {
        const QStringList idxs = attrs.value("indices").split(',', Qt::SkipEmptyParts);
        int savedGap = attrs.value("gap", "-1").toInt();
        const int savedOffset = attrs.value("offset", "0").toInt();
        if (savedGap < 0) {
            for (const QString &tok : idxs) {
                bool ok = false;
                const int bi = tok.trimmed().toInt(&ok);
                if (ok && bi >= 0 && bi < n_curves) {
                    if (const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(curve(bi))) {
                        savedGap = bc->gap();
                        break;
                    }
                }
            }
            if (savedGap < 0)
                savedGap = 30;
        }
        for (const QString &tok : idxs) {
            bool ok = false;
            const int bi = tok.trimmed().toInt(&ok);
            if (ok && bi >= 0 && bi < n_curves) {
                if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(curve(bi))) {
                    bc->setStacked(true);
                    setBarsGap(bi, savedGap, savedOffset);
                }
            }
        }
        if (d_plot)
            d_plot->replot();
        return true;
    }

    if (s.startsWith("<ToriWeibullAxis")) {
        const int axis = attrs.value("axis", "-1").toInt();
        const bool en = attrs.value("enabled", "1").toInt() != 0;
        if (axis >= 0 && axis < 4)
            setWeibullProbabilityAxis(axis, en);
        return true;
    }

    if (s.startsWith("<ToriCategoryAxis")) {
        const int axis = attrs.value("axis", "-1").toInt();
        const QString rawLabels = attrs.value("labels");
        const QString rawPositions = attrs.value("positions");
        if (axis >= 0 && axis < 4 && !rawLabels.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            const QStringList lbls = rawLabels.split(QChar(0x1F), Qt::KeepEmptyParts);
#else
            const QStringList lbls = rawLabels.split(QChar(0x1F), QString::KeepEmptyParts);
#endif
            if (!rawPositions.isEmpty()) {
                const QStringList posStrs = rawPositions.split(',', Qt::SkipEmptyParts);
                QVector<double> posVec;
                posVec.reserve(posStrs.size());
                for (const QString &ps : posStrs)
                    posVec.append(ps.toDouble());
                const double rot = attrs.value("rot", "0").toDouble();
                setCustomTickLabels(axis, posVec, lbls, rot);
            } else {
                setCategoryLabels(axis, lbls);
            }
            if (!d_is_ternary && !d_is_polar && !isPoleFigurePlot() && !isPiePlot()) {
                updateSecondaryAxis(QwtPlot::xTop);
                updateSecondaryAxis(QwtPlot::yRight);
            }
        }
        return true;
    }

    if (s.startsWith("<ToriPolarPlot")) {
        const bool en = attrs.value("enabled", "1").toInt() != 0;
        if (en) {
            d_polar_rmax = qMax(1e-6, attrs.value("rmax", "1").toDouble());
            setupPolarPlot(QVector<double>(), QVector<double>(), QString(), true);
        }
        return true;
    }

    if (s.startsWith("<ToriPolarSeries")) {
        const QStringList xs = attrs.value("x").split(',', Qt::SkipEmptyParts);
        const QStringList ys = attrs.value("y").split(',', Qt::SkipEmptyParts);
        const int m = qMin(xs.size(), ys.size());
        if (m >= 1) {
            PolarSeriesSpec ps;
            ps.x.resize(m);
            ps.y.resize(m);
            for (int k = 0; k < m; ++k) {
                ps.x[k] = xs[k].toDouble();
                ps.y[k] = ys[k].toDouble();
            }
            ps.label = attrs.value("label", tr("Polar"));
            ps.color = QColor(attrs.value("color", "#ff0072b2"));
            if (!ps.color.isValid())
                ps.color = QColor(0, 114, 178);
            ps.width = qMax(1.0, attrs.value("width", "2").toDouble());

            PlotCurve *pc = new PlotCurve(ps.label);
            pc->setType(Function);
            pc->setData(ps.x.constData(), ps.y.constData(), m);
            QPen p(ps.color, ps.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setCosmetic(false);
            pc->setPen(p);

            c_type.resize(++n_curves);
            c_type[n_curves - 1] = Function;
            c_keys.resize(n_curves);
            c_keys[n_curves - 1] = d_plot->insertCurve(pc);
            syncCurveZOrder();
            d_polar_series.append(ps);

            const double rMax = qMax(1e-6, d_polar_rmax);
            const int nPolarRows = qMax(1, qMax(int(d_polar_series.size()), n_curves));
            const int extraRows = qMax(0, nPolarRows - 1);
            const double yLo = -1.44 * rMax;
            const double yHi = (1.64 + 0.36 * double(extraRows)) * rMax;
            const double halfX = 0.5 * (yHi - yLo);
            setScale(QwtPlot::xBottom, -halfX, halfX, 0.0, 5, 0, 0, false);
            setScale(QwtPlot::yLeft, yLo, yHi, 0.0, 5, 0, 0, false);
            for (int a = 0; a < QwtPlot::axisCnt; ++a)
                enableAxis(a, false);
            if (hasLegend())
                placeLegendAnchor(LegendTopRight);
        }
        return true;
    }

    if (s.startsWith("<ToriAutoscaleFonts")) {
        autoScaleFonts = (attrs.value("val", "0").toInt() != 0);
        return true;
    }

    if (s.startsWith("<ToriAxisLocks")) {
        const int mask = attrs.value("mask", "0").toInt();
        for (int a = 0; a < 4; ++a)
            setAxisUserLocked(a, (mask & (1 << a)) != 0);
        if (attrs.contains("autoscale")) {
            m_autoscale = (attrs.value("autoscale").toInt() != 0);
            if (d_plot)
                d_plot->setProperty("_toriRestoredAutoscale", true);
        }
        if (attrs.contains("autoMask") && d_plot && !isTernaryPlot() && !isPolarPlot()
            && !isPiePlot() && !isPoleFigurePlot()) {
            const int autoMask = attrs.value("autoMask", "0").toInt();
            bool restoredAnyAuto = false;
            for (int a = 0; a < 4; ++a) {
                if ((autoMask & (1 << a)) != 0 && (mask & (1 << a)) == 0 && !axisBreak(a).enabled
                    && !hasColorScaleOnAxis(a) && !hasWeibullProbabilityAxis(a)
                    && !hasCustomTickLabels(a)) {
                    d_plot->setAxisAutoScale(a);
                    restoredAnyAuto = true;
                }
            }
            if (restoredAnyAuto) {
                d_plot->updateAxes();
                if (d_plot->axisEnabled(QwtPlot::xTop) && !isAxisUserLocked(QwtPlot::xTop))
                    updateSecondaryAxis(QwtPlot::xTop);
                if (d_plot->axisEnabled(QwtPlot::yRight) && !isAxisUserLocked(QwtPlot::yRight))
                    updateSecondaryAxis(QwtPlot::yRight);
            }
        }
        if (isPiePlot()) {
            drawCanvasFrame(false, 0);
            setFrame(0, Qt::black);
            drawAxesBackbones(false);
        }
        return true;
    }

    return false;
}

// ============================================================================
// Slice 1: Unified Selection, Curve Z-Order & Reordering, Hollow Symbols,
//          Series Style Schemes, Single-Axis Autoscale, & Smart Legend Anchor
// ============================================================================

void Graph::setCurrentSelection(const GraphSelection &sel, bool notify)
{
    const bool changed = (d_selection != sel);
    d_selection = sel;

    if (sel.kind != GraphSelection::None) {
        if (MultiLayer *ml = multiLayerParent())
            ml->clearLayerSelector();
    }

    if (sel.kind != GraphSelection::Axis && sel.kind != GraphSelection::AxisTitle) {
        if (scalePicker)
            scalePicker->deselect();
    }
    if (sel.kind != GraphSelection::PlotTitle) {
        if (titlePicker)
            titlePicker->setSelected(false);
    }

    if (sel.kind == GraphSelection::Curve || sel.kind == GraphSelection::ErrorBars) {
        deselectMarker();
        const int key = (sel.primaryId >= 0 && sel.primaryId < n_curves) ? c_keys[sel.primaryId]
                                                                         : -1;
        if (d_plot)
            d_plot->setHighlightedCurveKey(key);
    } else if (sel.kind == GraphSelection::ArrowAnnotation) {
        if (d_plot)
            d_plot->setHighlightedCurveKey(-1);
        selectedMarker = sel.primaryId;
        if (d_markers_selector) {
            ArrowMarker *am = arrow(sel.primaryId);
            if (!am || !d_markers_selector->contains(am))
                detachAndDeleteMarkerSelector(d_markers_selector);
        }
    } else if (sel.kind == GraphSelection::LegendBox
               || sel.kind == GraphSelection::TextAnnotation
               || sel.kind == GraphSelection::ImageAnnotation) {
        if (d_plot)
            d_plot->setHighlightedCurveKey(-1);
        if (sel.kind == GraphSelection::LegendBox) {
            Legend *targetLeg = (sel.primaryId >= 0 && d_plot)
                    ? dynamic_cast<Legend *>(d_plot->marker(sel.primaryId))
                    : legend();
            if (!targetLeg)
                targetLeg = legend();
            if (d_markers_selector && (!targetLeg || !d_markers_selector->contains(targetLeg)))
                detachAndDeleteMarkerSelector(d_markers_selector);
            if (sel.primaryId >= 0)
                selectedMarker = sel.primaryId;
            else if (legendMarkerID >= 0)
                selectedMarker = legendMarkerID;
        } else if (sel.kind == GraphSelection::TextAnnotation) {
            Legend *targetTxt = (sel.primaryId >= 0 && d_plot)
                    ? dynamic_cast<Legend *>(d_plot->marker(sel.primaryId))
                    : nullptr;
            if (d_markers_selector && (!targetTxt || !d_markers_selector->contains(targetTxt)))
                detachAndDeleteMarkerSelector(d_markers_selector);
            if (sel.primaryId >= 0)
                selectedMarker = sel.primaryId;
        } else {
            ImageMarker *targetImg = (sel.primaryId >= 0) ? imageMarker(sel.primaryId) : nullptr;
            if (d_markers_selector && (!targetImg || !d_markers_selector->contains(targetImg)))
                detachAndDeleteMarkerSelector(d_markers_selector);
            if (sel.primaryId >= 0)
                selectedMarker = sel.primaryId;
        }
    } else {
        if (d_plot)
            d_plot->setHighlightedCurveKey(-1);
        deselectMarker();
    }

    updateSelectionOverlayGeometry();
    if (changed && d_plot && d_plot->canvas())
        d_plot->canvas()->update();

    if (notify && changed) {
        emit selectionChanged(this, d_selection);
        if (MultiLayer *ml = multiLayerParent())
            emit ml->selectionChanged(ml, this, d_selection);
    }
}

void Graph::updateSelectionOverlayGeometry()
{
    auto *overlay = static_cast<GraphSelectionOverlayWidget *>(d_selection_overlay);
    if (!overlay)
        return;

    if (!d_plot || isExporting() || property("isExporting").toBool()) {
        overlay->hide();
        return;
    }

    auto showOverlayRect = [&](const QRect &graphRect, bool dashed) {
        const QRect clamped = graphRect.intersected(rect());
        if (clamped.width() < 4 || clamped.height() < 4) {
            overlay->hide();
            return;
        }
        overlay->setDashed(dashed);
        overlay->setGeometry(clamped);
        overlay->show();
        overlay->raise();
    };

    switch (d_selection.kind) {
    case GraphSelection::PlotTitle: {
        QwtTextLabel *tl = d_plot->titleLabel();
        if (tl && tl->isVisible() && !d_plot->title().text().trimmed().isEmpty()) {
            const QPoint topLeft = tl->mapTo(this, QPoint(0, 0));
            showOverlayRect(QRect(topLeft, tl->size()).adjusted(-2, -1, 2, 1), false);
            return;
        }
        break;
    }
    case GraphSelection::AxisTitle: {
        const int axis = d_selection.primaryId;
        if (axis >= 0 && axis < QwtPlot::axisCnt && d_plot->axisEnabled(axis) && scalePicker) {
            QwtScaleWidget *sw = d_plot->axisWidget(axis);
            if (sw && sw->isVisible() && !axisTitle(axis).trimmed().isEmpty()) {
                const QRect trLocal = scalePicker->titleRect(sw);
                if (trLocal.isValid() && !trLocal.isEmpty()) {
                    const QPoint topLeft = sw->mapTo(this, trLocal.topLeft());
                    showOverlayRect(QRect(topLeft, trLocal.size()).adjusted(-3, -2, 3, 2), false);
                    return;
                }
            }
        }
        break;
    }
    case GraphSelection::Axis: {
        const int axis = d_selection.primaryId;
        if (axis >= 0 && axis < QwtPlot::axisCnt && d_plot->axisEnabled(axis) && scalePicker) {
            QwtScaleWidget *sw = d_plot->axisWidget(axis);
            if (sw && sw->isVisible()) {
                const QRect srLocal = scalePicker->scaleRect(sw);
                if (srLocal.isValid() && !srLocal.isEmpty()) {
                    const QPoint topLeft = sw->mapTo(this, srLocal.topLeft());
                    showOverlayRect(QRect(topLeft, srLocal.size()).adjusted(-2, -2, 2, 2), false);
                    return;
                }
            }
        }
        break;
    }
    case GraphSelection::LegendBox: {
        Legend *leg = legend();
        QwtPlotCanvas *cv = d_plot->canvas();
        if (leg && leg->isVisible() && cv) {
            const QRect lr = leg->rect();
            const bool outsideCanvas = (lr.right() > cv->width() || lr.left() < 0
                                        || lr.bottom() > cv->height() || lr.top() < 0);
            if (outsideCanvas) {
                if (d_markers_selector && !d_markers_selector->isInKeyNudge())
                    detachAndDeleteMarkerSelector(d_markers_selector);
                const QPoint topLeft = cv->mapTo(this, lr.topLeft());
                showOverlayRect(QRect(topLeft, lr.size()).adjusted(-2, -2, 2, 2), true);
                return;
            }
            if (!d_markers_selector) {
                const QPoint topLeft = cv->mapTo(this, lr.topLeft());
                showOverlayRect(QRect(topLeft, lr.size()).adjusted(-2, -2, 2, 2), true);
                return;
            }
        }
        break;
    }
    default:
        break;
    }

    overlay->hide();
}

void Graph::syncCurveZOrder()
{
    if (!d_plot)
        return;

    for (int i = 0; i < n_curves && i < c_keys.size(); ++i) {
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it)
            continue;
        if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
            it->setZ(8.0 + i * 0.01);
        } else if (i < c_type.size() && c_type[i] != ErrorBars) {
            it->setZ(20.0 + i * 0.01);
        }
    }

    QMap<DataCurve *, int> errOrdinalPerMaster;
    for (int i = 0; i < n_curves && i < c_keys.size(); ++i) {
        if (i >= c_type.size() || c_type[i] != ErrorBars)
            continue;
        QwtPlotItem *it = d_plot->plotItem(c_keys[i]);
        if (!it)
            continue;
        QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it);
        if (er && er->masterCurve() && er->isBand()) {
            // E11: error bands sit under every curve so a band never hides another line.
            const int ord = errOrdinalPerMaster[er->masterCurve()]++;
            it->setZ(er->masterCurve()->z() - 5.0 + ord * 0.0005);
        } else if (er && er->masterCurve()) {
            const int ord = errOrdinalPerMaster[er->masterCurve()]++;
            it->setZ(er->masterCurve()->z() - 0.004 + ord * 0.0005);
        } else {
            it->setZ(20.0 + i * 0.01);
        }
    }
}

bool Graph::moveCurve(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= n_curves || toIndex < 0 || toIndex >= n_curves
        || fromIndex == toIndex || c_keys.size() < n_curves || c_type.size() < n_curves) {
        return false;
    }

    auto masterOldIndexFor = [&](int errIdx) -> int {
        if (errIdx < 0 || errIdx >= n_curves || c_type[errIdx] != ErrorBars)
            return -1;
        QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(plotItem(errIdx));
        if (!er || !er->masterCurve())
            return -1;
        return plotItemIndex(er->masterCurve());
    };

    QVector<int> order;
    order.reserve(n_curves);

    if (c_type[fromIndex] == ErrorBars) {
        const int mIdx = masterOldIndexFor(fromIndex);
        if (mIdx >= 0)
            toIndex = qMax(toIndex, mIdx + 1);
        if (toIndex == fromIndex || toIndex >= n_curves)
            return false;

        order.resize(n_curves);
        for (int i = 0; i < n_curves; ++i)
            order[i] = i;
        const int movedVal = order[fromIndex];
        order.remove(fromIndex);
        order.insert(toIndex, movedVal);

        // Enforce .sciprj load invariant: every QwtErrorPlotCurve must sit strictly after its master
        for (int oldIdx = 0; oldIdx < n_curves; ++oldIdx) {
            if (c_type[oldIdx] != ErrorBars)
                continue;
            const int mOld = masterOldIndexFor(oldIdx);
            if (mOld < 0)
                continue;
            const int posErr = order.indexOf(oldIdx);
            const int posMaster = order.indexOf(mOld);
            if (posErr >= 0 && posMaster >= 0 && posErr < posMaster) {
                order.remove(posErr);
                const int newMasterPos = order.indexOf(mOld);
                int insertPos = newMasterPos + 1;
                while (insertPos < order.size() && masterOldIndexFor(order[insertPos]) == mOld)
                    ++insertPos;
                order.insert(insertPos, oldIdx);
            }
        }
    } else {
        // Partition curves into logical [MasterCurve + attached ErrorBars] groups so moving a
        // master curve steps cleanly across neighbor groups without snapping back or splitting.
        QVector<QVector<int>> groups;
        QVector<int> idxToGroup(n_curves, -1);
        for (int i = 0; i < n_curves; ++i) {
            if (idxToGroup[i] >= 0)
                continue;
            if (c_type[i] == ErrorBars) {
                const int mOld = masterOldIndexFor(i);
                if (mOld >= 0 && idxToGroup[mOld] >= 0) {
                    const int g = idxToGroup[mOld];
                    groups[g].append(i);
                    idxToGroup[i] = g;
                    continue;
                }
                if (mOld >= 0 && mOld > i) {
                    const int g = groups.size();
                    QVector<int> grp;
                    grp.append(mOld);
                    idxToGroup[mOld] = g;
                    for (int k = 0; k < n_curves; ++k) {
                        if (k != mOld && masterOldIndexFor(k) == mOld) {
                            grp.append(k);
                            idxToGroup[k] = g;
                        }
                    }
                    groups.append(grp);
                    continue;
                }
                const int g = groups.size();
                groups.append(QVector<int>{ i });
                idxToGroup[i] = g;
            } else {
                const int g = groups.size();
                QVector<int> grp;
                grp.append(i);
                idxToGroup[i] = g;
                for (int k = 0; k < n_curves; ++k) {
                    if (k != i && masterOldIndexFor(k) == i) {
                        grp.append(k);
                        idxToGroup[k] = g;
                    }
                }
                groups.append(grp);
            }
        }

        const int srcGroup = idxToGroup[fromIndex];
        int dstGroup = idxToGroup[toIndex];
        if (srcGroup < 0 || dstGroup < 0)
            return false;
        if (dstGroup == srcGroup) {
            if (toIndex > fromIndex)
                dstGroup = srcGroup + 1;
            else if (toIndex < fromIndex)
                dstGroup = srcGroup - 1;
        }
        if (dstGroup < 0 || dstGroup >= groups.size() || dstGroup == srcGroup)
            return false;

        const QVector<int> movedGrp = groups.takeAt(srcGroup);
        groups.insert(dstGroup, movedGrp);

        for (const QVector<int> &grp : groups) {
            for (int oldIdx : grp)
                order.append(oldIdx);
        }
    }

    if (order.size() != n_curves)
        return false;

    bool anyOrderChange = false;
    for (int i = 0; i < n_curves; ++i) {
        if (order[i] != i) {
            anyOrderChange = true;
            break;
        }
    }
    if (!anyOrderChange)
        return false;

    QVector<int> oldToNew(n_curves, 0);
    QVector<int> newKeys(n_curves);
    QVector<int> newTypes(n_curves);
    for (int newIdx = 0; newIdx < n_curves; ++newIdx) {
        const int oldIdx = order[newIdx];
        oldToNew[oldIdx] = newIdx;
        newKeys[newIdx] = c_keys[oldIdx];
        newTypes[newIdx] = c_type[oldIdx];
    }
    if (newKeys == c_keys)
        return false;
    c_keys = newKeys;
    c_type = newTypes;

    {
        QMap<int, QString> remappedAutoLabels;
        for (auto it = d_last_auto_curve_labels.constBegin();
             it != d_last_auto_curve_labels.constEnd(); ++it) {
            if (it.key() >= 0 && it.key() < n_curves)
                remappedAutoLabels.insert(oldToNew[it.key()], it.value());
        }
        d_last_auto_curve_labels = remappedAutoLabels;
    }

    if ((d_selection.kind == GraphSelection::Curve
         || d_selection.kind == GraphSelection::ErrorBars
         || d_selection.kind == GraphSelection::Spectrogram)
        && d_selection.primaryId >= 0 && d_selection.primaryId < n_curves) {
        d_selection.primaryId = oldToNew[d_selection.primaryId];
    }

    // Single-pass regex remap of 1-based \c{k}, \l(k), %(k) tokens across all Legend/text markers
    static const QRegularExpression tokenRx(
            QStringLiteral(R"((\\c\{(\d+)\})|(\\l\((\d+)\))|(%\((\d+)\)))"));
    for (int tid : d_texts) {
        Legend *tm = dynamic_cast<Legend *>(d_plot->marker(tid));
        if (!tm)
            continue;
        const QString src = tm->text();
        QRegularExpressionMatchIterator mit = tokenRx.globalMatch(src);
        if (!mit.hasNext())
            continue;

        QString dst;
        dst.reserve(src.size() + 8);
        int lastPos = 0;
        bool textChanged = false;
        while (mit.hasNext()) {
            const QRegularExpressionMatch m = mit.next();
            dst.append(src.mid(lastPos, m.capturedStart() - lastPos));
            lastPos = m.capturedEnd();

            QString replacement = m.captured(0);
            if (!m.captured(2).isEmpty()) {
                bool ok = false;
                const int oldOneBased = m.captured(2).toInt(&ok);
                if (ok && oldOneBased >= 1 && oldOneBased <= n_curves) {
                    const int newOneBased = oldToNew[oldOneBased - 1] + 1;
                    replacement = QStringLiteral("\\c{%1}").arg(newOneBased);
                }
            } else if (!m.captured(4).isEmpty()) {
                bool ok = false;
                const int oldOneBased = m.captured(4).toInt(&ok);
                if (ok && oldOneBased >= 1 && oldOneBased <= n_curves) {
                    const int newOneBased = oldToNew[oldOneBased - 1] + 1;
                    replacement = QStringLiteral("\\l(%1)").arg(newOneBased);
                }
            } else if (!m.captured(6).isEmpty()) {
                bool ok = false;
                const int oldOneBased = m.captured(6).toInt(&ok);
                if (ok && oldOneBased >= 1 && oldOneBased <= n_curves) {
                    const int newOneBased = oldToNew[oldOneBased - 1] + 1;
                    replacement = QStringLiteral("%(%1)").arg(newOneBased);
                }
            }
            if (replacement != m.captured(0))
                textChanged = true;
            dst.append(replacement);
        }
        dst.append(src.mid(lastPos));
        if (textChanged)
            tm->setText(dst);
    }

    syncCurveZOrder();
    d_plot->replot();
    emit curveOrderChanged();
    emit modifiedGraph();
    return true;
}

bool Graph::isLegendVisible() const
{
    if (legendMarkerID < 0 || !d_plot)
        return false;
    const QwtPlotMarker *m = d_plot->marker(legendMarkerID);
    return m ? (d_legend_visible && m->isVisible()) : false;
}

void Graph::setLegendVisible(bool visible)
{
    if (visible && legendMarkerID < 0 && !isPiePlot() && n_curves > 0) {
        newLegend();
        return;
    }

    d_legend_visible = visible;
    Legend *leg = legend();
    if (leg) {
        leg->setVisible(visible);
        if (!visible) {
            if (selectedMarker == legendMarkerID)
                deselectMarker();
            if (d_selection.kind == GraphSelection::LegendBox)
                setCurrentSelection({ GraphSelection::None, -1, -1 });
            const int curOrm = d_plot->property("_outsideRightMargin").toInt();
            if (curOrm > 0) {
                d_plot->setProperty("_savedOutsideRightMargin", curOrm);
                d_plot->setProperty("_outsideRightMargin", 0);
            }
        } else {
            const int savedOrm = d_plot->property("_savedOutsideRightMargin").toInt();
            if (savedOrm > 0 && d_plot->canvas() && leg->rect().right() > d_plot->canvas()->width()) {
                d_plot->setProperty("_outsideRightMargin", savedOrm);
                d_plot->setProperty("_savedOutsideRightMargin", 0);
            }
        }
    }

    d_plot->replot();
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
}

bool Graph::isSymbolWhiteHollow(const QwtSymbol &sym)
{
    return sym.style() != QwtSymbol::NoSymbol
            && sym.brush().style() == Qt::SolidPattern
            && sym.brush().color() == QColor(Qt::white)
            && sym.pen().color() != QColor(Qt::white);
}

void Graph::setCurveSymbolHollow(int curveIndex, bool hollowWhite)
{
    QwtPlotCurve *c = curve(curveIndex);
    if (!c)
        return;

    QwtSymbol sym = c->symbol();
    QColor curveColor = c->pen().color();
    if (!curveColor.isValid() || (hollowWhite && curveColor == QColor(Qt::white))) {
        const QColor spCol = sym.pen().color();
        curveColor = (spCol.isValid() && spCol != QColor(Qt::white)) ? spCol : QColor(Qt::black);
    }

    if (hollowWhite) {
        if (sym.style() == QwtSymbol::NoSymbol) {
            sym.setStyle(QwtSymbol::Ellipse);
            sym.setSize(9);
        } else if (sym.size().width() <= 0) {
            sym.setSize(9);
        }
        QPen sp = sym.pen();
        sp.setColor(curveColor);
        if (sp.widthF() <= 0.0)
            sp.setWidthF(c->pen().widthF() > 0.0 ? c->pen().widthF() : 1.0);
        sym.setPen(sp);
        sym.setBrush(QBrush(Qt::white, Qt::SolidPattern));
        if (curveIndex >= 0 && curveIndex < c_type.size() && c_type[curveIndex] == Line
            && c->style() != QwtPlotCurve::NoCurve) {
            c_type[curveIndex] = LineSymbols;
        }
    } else {
        QPen sp = sym.pen();
        sp.setColor(curveColor);
        sym.setPen(sp);
        sym.setBrush(QBrush(curveColor, Qt::SolidPattern));
    }

    c->setSymbol(sym);
    d_plot->replot();
    emit modifiedGraph();
}

void Graph::applySeriesStyleScheme(SeriesStyleScheme scheme)
{
    QList<int> targetIndices;
    for (int i = 0; i < n_curves && i < c_type.size(); ++i) {
        if (!isCurveVisible(i))
            continue;
        const int ct = c_type[i];
        if (ct == ErrorBars || ct == ColorMap || ct == GrayMap || ct == ContourMap || ct == Pie)
            continue;
        QwtPlotCurve *c = curve(i);
        if (c && c->rtti() != QwtPlotItem::Rtti_PlotSpectrogram)
            targetIndices.append(i);
    }
    if (targetIndices.isEmpty())
        return;

    struct SymbolStepSpec
    {
        QColor color;
        QwtSymbol::Style shape;
        bool whiteHollow;
        bool forceSymbol;
    };

    const int m = targetIndices.size();
    QVector<SymbolStepSpec> specs;
    specs.reserve(m);

    switch (scheme) {
    case SchemeBW2to3Group: {
        static const QPair<QwtSymbol::Style, bool> kCycle[] = {
            { QwtSymbol::Ellipse, false },
            { QwtSymbol::Ellipse, true },
            { QwtSymbol::Rect, true },
            { QwtSymbol::UTriangle, true },
            { QwtSymbol::Diamond, false },
            { QwtSymbol::DTriangle, true }
        };
        for (int k = 0; k < m; ++k) {
            const auto &step = kCycle[k % 6];
            specs.push_back({ QColor(QStringLiteral("#000000")), step.first, step.second, true });
        }
        break;
    }
    case SchemeBlueFilledRedHollow: {
        struct Entry { const char *hex; QwtSymbol::Style st; bool hollow; };
        static const Entry kCycle[] = {
            { "#1F77B4", QwtSymbol::Ellipse, false },
            { "#D62728", QwtSymbol::Ellipse, true },
            { "#555555", QwtSymbol::Rect, true },
            { "#2CA02C", QwtSymbol::UTriangle, true },
            { "#9467BD", QwtSymbol::Diamond, false },
            { "#8C564B", QwtSymbol::DTriangle, true }
        };
        for (int k = 0; k < m; ++k) {
            const Entry &e = kCycle[k % 6];
            specs.push_back({ QColor(QString::fromLatin1(e.hex)), e.st, e.hollow, true });
        }
        break;
    }
    case SchemeMutedCategorical: {
        static const char *const kHex[] = {
            "#1F77B4", "#D62728", "#555555", "#2CA02C", "#9467BD", "#8C564B"
        };
        static const QwtSymbol::Style kShapes[] = {
            QwtSymbol::Ellipse, QwtSymbol::Rect, QwtSymbol::UTriangle,
            QwtSymbol::Diamond, QwtSymbol::DTriangle, QwtSymbol::Cross
        };
        for (int k = 0; k < m; ++k) {
            const int cIdx = targetIndices[k];
            const QwtPlotCurve *c = curve(cIdx);
            const int ct = (cIdx >= 0 && cIdx < c_type.size()) ? c_type[cIdx] : LineSymbols;
            const bool hasSym = c
                    && (c->symbol().style() != QwtSymbol::NoSymbol || ct == Scatter
                        || ct == LineSymbols);
            const bool keepHollow = c ? isSymbolWhiteHollow(c->symbol()) : false;
            specs.push_back({ QColor(QString::fromLatin1(kHex[k % 6])),
                              hasSym ? kShapes[k % 6] : QwtSymbol::NoSymbol, keepHollow,
                              hasSym });
        }
        break;
    }
    case SchemeCoolToWarmStep: {
        const QList<QColor> anchors = {
            QColor(QStringLiteral("#2166AC")),
            QColor(QStringLiteral("#4393C3")),
            QColor(QStringLiteral("#68788C")),
            QColor(QStringLiteral("#D6604D")),
            QColor(QStringLiteral("#B2182B"))
        };
        const QList<QColor> interp = interpolatePalette(anchors, m);
        for (int k = 0; k < m; ++k) {
            const QwtPlotCurve *c = curve(targetIndices[k]);
            const QwtSymbol sym = c ? c->symbol() : QwtSymbol();
            specs.push_back({ interp[k % interp.size()], sym.style(), isSymbolWhiteHollow(sym),
                              false });
        }
        break;
    }
    case SchemeViridisSequential: {
        // Viridis t in [0.0, 0.84]: #440154 -> #31688E -> #35B779 -> #6DCD59 (avoids pale yellow)
        const QList<QColor> anchors = {
            QColor(QStringLiteral("#440154")),
            QColor(QStringLiteral("#3B528B")),
            QColor(QStringLiteral("#31688E")),
            QColor(QStringLiteral("#21918C")),
            QColor(QStringLiteral("#35B779")),
            QColor(QStringLiteral("#6DCD59"))
        };
        const QList<QColor> interp = interpolatePalette(anchors, m);
        for (int k = 0; k < m; ++k) {
            const QwtPlotCurve *c = curve(targetIndices[k]);
            const QwtSymbol sym = c ? c->symbol() : QwtSymbol();
            specs.push_back({ interp[k % interp.size()], sym.style(), isSymbolWhiteHollow(sym),
                              false });
        }
        break;
    }
    case SchemeOriginClassic:
    default: {
        const QList<QColor> &originCols = originClassicColors();
        for (int k = 0; k < m; ++k) {
            const QwtPlotCurve *c = curve(targetIndices[k]);
            const QwtSymbol sym = c ? c->symbol() : QwtSymbol();
            specs.push_back({ originCols.at(k % originCols.size()), sym.style(),
                              isSymbolWhiteHollow(sym), false });
        }
        break;
    }
    }

    for (int k = 0; k < m; ++k) {
        const int idx = targetIndices[k];
        QwtPlotCurve *c = curve(idx);
        if (!c)
            continue;
        const SymbolStepSpec &sp = specs[k];

        QPen pen = c->pen();
        pen.setColor(sp.color);
        c->setPen(pen);

        if (c_type[idx] == VectXYXY || c_type[idx] == VectXYAM) {
            if (VectorCurve *vc = dynamic_cast<VectorCurve *>(c))
                vc->setColor(sp.color);
        }

        if (c->brush().style() != Qt::NoBrush) {
            QBrush br = c->brush();
            QColor fillCol = sp.color;
            if (br.color().alpha() < 255)
                fillCol.setAlpha(br.color().alpha());
            br.setColor(fillCol);
            c->setBrush(br);
        }

        QwtSymbol sym = c->symbol();
        if (sp.forceSymbol) {
            const int ct = c_type[idx];
            if (ct == Line || ct == Scatter || ct == LineSymbols || ct == Spline) {
                sym.setStyle(sp.shape);
                if (sym.size().width() <= 0)
                    sym.setSize(9);
                QPen symPen = sym.pen();
                symPen.setColor(sp.color);
                if (symPen.widthF() <= 0.0)
                    symPen.setWidthF(pen.widthF() > 0.0 ? pen.widthF() : 1.0);
                sym.setPen(symPen);
                sym.setBrush(sp.whiteHollow ? QBrush(Qt::white, Qt::SolidPattern)
                                            : QBrush(sp.color, Qt::SolidPattern));
                c->setSymbol(sym);
                if (ct == Line && c->style() != QwtPlotCurve::NoCurve)
                    c_type[idx] = LineSymbols;
            }
        } else if (sym.style() != QwtSymbol::NoSymbol) {
            QPen symPen = sym.pen();
            symPen.setColor(sp.color);
            sym.setPen(symPen);
            if (sp.whiteHollow) {
                sym.setBrush(QBrush(Qt::white, Qt::SolidPattern));
            } else if (sym.brush().style() != Qt::NoBrush) {
                QBrush sb = sym.brush();
                sb.setColor(sp.color);
                sym.setBrush(sb);
            }
            c->setSymbol(sym);
        }

        if (DataCurve *dc = dynamic_cast<DataCurve *>(c)) {
            for (DataCurve *eb : dc->errorBarsList()) {
                if (QwtErrorPlotCurve *errCurve = dynamic_cast<QwtErrorPlotCurve *>(eb))
                    errCurve->setColor(sp.color);
            }
        }
    }

    d_plot->replot();
    emit modifiedGraph();
}

void Graph::setAutoScaleAxis(int qwtAxisId)
{
    if (qwtAxisId < 0 || qwtAxisId >= QwtPlot::axisCnt || !d_plot || isPiePlot()
        || isTernaryPlot()) {
        return;
    }
    const MarkerAxisState axesBefore = markerAxisState();
    if (!d_applying_zoom_history)
        recordZoomHistorySnapshot();

    int targetAxis = qwtAxisId;
    if (qwtAxisId == QwtPlot::xTop || qwtAxisId == QwtPlot::yRight) {
        bool hasOwnItem = false;
        for (int i = 0; i < n_curves; ++i) {
            QwtPlotItem *it = plotItem(i);
            if (!it || !it->isVisible())
                continue;
            if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                Spectrogram *sp = static_cast<Spectrogram *>(it);
                if (sp->colorScaleAxis() == qwtAxisId) {
                    setToriAxisUserLocked(d_plot, qwtAxisId, false);
                    d_plot->setAxisScale(qwtAxisId, sp->colorAxisRange().minValue(),
                                         sp->colorAxisRange().maxValue());
                    d_user_step[qwtAxisId] = 0.0;
                    d_plot->replot();
                    emit modifiedGraph();
                    return;
                }
            }
            if ((qwtAxisId == QwtPlot::xTop && it->xAxis() == QwtPlot::xTop)
                || (qwtAxisId == QwtPlot::yRight && it->yAxis() == QwtPlot::yRight)) {
                hasOwnItem = true;
                break;
            }
        }
        if (!hasOwnItem)
            targetAxis = (qwtAxisId == QwtPlot::xTop) ? QwtPlot::xBottom : QwtPlot::yLeft;
    }

    setToriAxisUserLocked(d_plot, targetAxis, false);
    d_user_step[targetAxis] = 0.0;
    d_plot->setAxisAutoScale(targetAxis);
    applyZeroSnapOnAutoAxes();
    d_plot->updateAxes();

    updateSecondaryAxis(QwtPlot::xTop);
    updateSecondaryAxis(QwtPlot::yRight);
    updateMarkersBoundingRect(axesBefore);
    d_zoomer[0]->setZoomBase(false);
    d_zoomer[1]->setZoomBase(false);
    updateStatReferenceLines();
    if (d_linked_parent)
        syncLinkedAxesFromParent();
    syncChildLinkedLayers();
    emit modifiedGraph();
}

bool Graph::applyTemplateCurveStyle(int curveIndex, int styleIndex)
{
    if (d_template_curve_styles.isEmpty() || curveIndex < 0 || curveIndex >= n_curves)
        return false;
    if (curveIndex < c_type.size() && c_type[curveIndex] == ErrorBars)
        return false;
    QwtPlotCurve *cv = curve(curveIndex);
    if (!cv)
        return false;

    const int sIdx = (styleIndex >= 0) ? (styleIndex % d_template_curve_styles.size())
                                       : (curveIndex % d_template_curve_styles.size());
    const TemplateCurveStyle &st = d_template_curve_styles[sIdx];

    QPen pen = cv->pen();
    pen.setStyle(static_cast<Qt::PenStyle>(st.penStyle));
    if (st.lineWidth >= 0.0)
        pen.setWidthF(st.lineWidth);
    if (st.lineColor.isValid())
        pen.setColor(st.lineColor);
    cv->setPen(pen);

    QBrush br = cv->brush();
    br.setStyle(static_cast<Qt::BrushStyle>(st.brushStyle));
    if (st.brushColor.isValid())
        br.setColor(st.brushColor);
    cv->setBrush(br);

    QwtSymbol sym = cv->symbol();
    sym.setStyle(static_cast<QwtSymbol::Style>(st.symbolStyle));
    if (st.symbolSize > 0)
        sym.setSize(st.symbolSize);
    QPen symPen = sym.pen();
    if (st.symbolPenWidth >= 0.0)
        symPen.setWidthF(st.symbolPenWidth);
    if (st.symbolEdgeColor.isValid())
        symPen.setColor(st.symbolEdgeColor);
    sym.setPen(symPen);
    if (st.symbolHollow) {
        sym.setBrush(QBrush(Qt::white));
    } else if (st.symbolFillColor.isValid()) {
        QBrush symBr = sym.brush();
        if (symBr.style() == Qt::NoBrush && st.symbolStyle != int(QwtSymbol::NoSymbol))
            symBr.setStyle(Qt::SolidPattern);
        symBr.setColor(st.symbolFillColor);
        sym.setBrush(symBr);
    }
    cv->setSymbol(sym);

    if (PlotCurve *pc = dynamic_cast<PlotCurve *>(cv))
        pc->setSymbolSkip(st.symbolSkip);
    if (QwtBarCurve *bc = dynamic_cast<QwtBarCurve *>(cv)) {
        bc->setGap(st.barGap);
        bc->setOffset(st.barOffset);
    }
    if (curveIndex < c_type.size()) {
        const int curType = c_type[curveIndex];
        const bool curXY = (curType == Line || curType == Scatter || curType == LineSymbols
                            || curType == Spline || curType == Area || curType == VerticalSteps
                            || curType == HorizontalSteps || curType == VerticalDropLines);
        const bool tplXY = (st.curveType == Line || st.curveType == Scatter
                            || st.curveType == LineSymbols || st.curveType == Spline
                            || st.curveType == Area || st.curveType == VerticalSteps
                            || st.curveType == HorizontalSteps
                            || st.curveType == VerticalDropLines);
        if (curXY && tplXY) {
            c_type[curveIndex] = st.curveType;
            switch (st.curveType) {
            case Scatter:
                cv->setStyle(QwtPlotCurve::NoCurve);
                break;
            case VerticalDropLines:
                cv->setStyle(QwtPlotCurve::Sticks);
                break;
            case VerticalSteps:
                cv->setStyle(QwtPlotCurve::Steps);
                cv->setCurveAttribute(QwtPlotCurve::Inverted, false);
                break;
            case HorizontalSteps:
                cv->setStyle(QwtPlotCurve::Steps);
                cv->setCurveAttribute(QwtPlotCurve::Inverted, true);
                break;
            case Spline:
                cv->setStyle(QwtPlotCurve::Lines);
                cv->setCurveAttribute(QwtPlotCurve::Fitted, true);
                break;
            default:
                cv->setStyle(QwtPlotCurve::Lines);
                cv->setCurveAttribute(QwtPlotCurve::Fitted, false);
                break;
            }
        }
    }
    return true;
}

void Graph::applyTemplateCurveStylesToAll()
{
    if (d_template_curve_styles.isEmpty())
        return;
    int seriesIdx = 0;
    bool anyApplied = false;
    for (int ci = 0; ci < n_curves; ++ci) {
        if (ci < c_type.size() && c_type[ci] == ErrorBars)
            continue;
        if (applyTemplateCurveStyle(ci, seriesIdx++))
            anyApplied = true;
    }
    if (anyApplied && d_plot) {
        d_plot->replot();
        emit modifiedGraph();
    }
}

bool Graph::repositionAnchoredOutsideRightLegend()
{
    if (!d_plot || !d_plot->canvas())
        return false;
    Legend *leg = legend();
    if (!leg || !isLegendVisible() || !leg->isVisible())
        return false;
    const QVariant ancVar = d_plot->property("_toriLegendAnchor");
    if (!ancVar.isValid() || ancVar.toInt() != int(LegendOutsideRight))
        return false;

    int rightAxisSpan = 0;
    if (d_plot->axisEnabled(QwtPlot::yRight) && d_plot->axisWidget(QwtPlot::yRight)) {
        QwtScaleWidget *rw = d_plot->axisWidget(QwtPlot::yRight);
        const QwtScaleDraw *rsd = d_plot->axisScaleDraw(QwtPlot::yRight);
        const bool hasRightContent =
                (rsd && rsd->hasComponent(QwtAbstractScaleDraw::Labels))
                || !d_plot->axisTitle(QwtPlot::yRight).text().trimmed().isEmpty()
                || rw->isColorBarEnabled();
        if (hasRightContent)
            rightAxisSpan = rw->width();
    }

    const double z = qMax(0.25, d_plot->displayFontScale());
    const int baseGap = d_plot->property("_toriOutsideRightGapPx").isValid()
            ? qMax(2, d_plot->property("_toriOutsideRightGapPx").toInt())
            : 12;
    const int baseTop = d_plot->property("_toriOutsideRightTopPx").isValid()
            ? qMax(2, d_plot->property("_toriOutsideRightTopPx").toInt())
            : 12;
    const int gapPx = qMax(2, qRound(baseGap * z));
    const int topPx = qMax(2, qRound(baseTop * z));
    const int cw = d_plot->canvas()->width();
    leg->setOrigin(QPoint(cw + rightAxisSpan + gapPx, topPx));
    return true;
}

QString Graph::placeLegendAnchor(LegendAnchor anchor, double *outOverlapScore, int marginPx)
{
    if (outOverlapScore)
        *outOverlapScore = 0.0;
    if (!d_plot || !d_plot->canvas())
        return QString();

    Legend *leg = legend();
    if (!leg)
        return QString();

    ScopedUnzoomForExport unzoomGuard(multiLayerParent());

    if (!isLegendVisible()) {
        d_legend_visible = true;
        leg->setVisible(true);
    }

    const int m = qMax(2, marginPx);
    d_plot->setProperty("_toriLegendAnchor", int(anchor));

    if (anchor == LegendOutsideRight) {
        d_plot->setProperty("_outsideRightMargin", 0);
        d_plot->setProperty("_savedOutsideRightMargin", 0);
        d_plot->updateAxes();
        d_plot->updateLayout();

        const QRect initialRect = leg->rect();
        const int legW = qMax(16, initialRect.width());
        const int gapPx = qMax(8, m);
        const int neededMargin = legW + gapPx + 6;
        d_plot->setProperty("_outsideRightMargin", neededMargin);
        d_plot->updateLayout();

        d_plot->setProperty("_toriOutsideRightGapPx", gapPx);
        d_plot->setProperty("_toriOutsideRightTopPx", m);
        // Replot first so axes and canvasMap settle at the shrunk _outsideRightMargin width
        // before converting pixel origin -> data coordinates in setOrigin().
        d_plot->replot();
        repositionAnchoredOutsideRightLegend();
        d_plot->replot();
        unzoomGuard.restoreNow();
        updateSelectionOverlayGeometry();
        emit modifiedGraph();
        return QStringLiteral("OutsideRight");
    }

    if (d_plot->property("_outsideRightMargin").toInt() != 0) {
        d_plot->setProperty("_outsideRightMargin", 0);
        d_plot->setProperty("_savedOutsideRightMargin", 0);
    }
    d_plot->setProperty("_toriOutsideRightGapPx", QVariant());
    d_plot->setProperty("_toriOutsideRightTopPx", QVariant());
    d_plot->updateAxes();
    d_plot->updateLayout();

    int cw = d_plot->canvas()->width();
    int ch = d_plot->canvas()->height();

    const int majLen = d_plot->majorTickLength();
    bool framelessSingleCol =
            (leg->frameStyle() == Legend::None && leg->numColumns() <= 1);

    int legW = 16;
    int legH = 12;
    int effLegW = 16;
    QVector<QRectF> rowVisualRects;
    const double dpiRatio =
            qMax(1.0, 96.0 / double(qMax(36, d_plot->logicalDpiX())));

    auto recomputeLegendMetrics = [&]() {
        const QRect lr = leg->rect();
        legW = qMax(16, int(std::ceil(lr.width() * dpiRatio)));
        legH = qMax(12, int(std::ceil(lr.height() * dpiRatio)));
        effLegW = legW;
        rowVisualRects.clear();

        if (!framelessSingleCol)
            return;

        QString norm = leg->text();
        norm.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        norm.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        while (norm.endsWith(QLatin1Char('\n')) && norm.size() > 1)
            norm.chop(1);
        const QStringList rawLines = norm.split(QLatin1Char('\n'));

        const QFont storedLegFnt = leg->font();
        const QFont dispLegFnt = d_plot ? d_plot->displayFont(storedLegFnt) : storedLegFnt;
        const double zoomScale = d_plot ? qMax(0.25, d_plot->displayFontScale()) : 1.0;
        int maxSymW = 0;
        bool allScatter = true;
        bool anyCurveTok = false;
        QStringList visLines;
        for (const QString &ln : rawLines) {
            int cvIdx = -1;
            if (ln.contains(QLatin1String("\\c{"))) {
                const int p1 = ln.indexOf(QLatin1String("\\c{"));
                const int p2 = ln.indexOf(QLatin1Char('}'), p1 + 3);
                if (p1 >= 0 && p2 > p1 + 3)
                    cvIdx = ln.mid(p1 + 3, p2 - (p1 + 3)).toInt() - 1;
            } else if (ln.contains(QLatin1String("\\l("))) {
                const int p1 = ln.indexOf(QLatin1String("\\l("));
                const int p2 = ln.indexOf(QLatin1Char(')'), p1 + 3);
                if (p1 >= 0 && p2 > p1 + 3)
                    cvIdx = ln.mid(p1 + 3, p2 - (p1 + 3)).toInt() - 1;
            }
            if (cvIdx >= 0 && cvIdx < n_curves) {
                QwtPlotItem *it = plotItem(cvIdx);
                if (it && !it->isVisible())
                    continue;
                anyCurveTok = true;
                if (const QwtPlotCurve *cv = curve(cvIdx)) {
                    if (cv->style() != QwtPlotCurve::NoCurve || cv->brush().style() != Qt::NoBrush)
                        allScatter = false;
                    if (cv->symbol().style() != QwtSymbol::NoSymbol)
                        maxSymW = qMax(maxSymW, qBound(3, cv->symbol().size().width(), 15));
                }
            }
            visLines.append(ln);
        }

        const double legPt = storedLegFnt.pointSizeF();
        const bool compactLeg =
                (legPt > 0.0) ? (legPt <= 9.5) : (QFontMetricsF(storedLegFnt).height() <= 16.0);
        const int baseSymLineLen = (anyCurveTok && allScatter)
                ? qMax(maxSymW + (compactLeg ? 2 : 0), compactLeg ? 11 : 14)
                : (compactLeg
                           ? qBound(14,
                                    qMax(maxSymW + 6,
                                         qRound(QFontMetricsF(storedLegFnt).height() * 1.05)),
                                    34)
                           : qBound(16,
                                    qMax(maxSymW + 8,
                                         qRound(QFontMetricsF(storedLegFnt).height() * 1.45)),
                                    34));
        const int sampleOff =
                qMax(8, qRound((baseSymLineLen + (compactLeg ? 5 : 10)) * zoomScale));

        int curTop = qMax(2, qRound(4.0 * zoomScale)); // Legend::top_margin
        double maxRight = 16.0;
        for (const QString &ln : visLines) {
            bool hasTok = false;
            QString parsed = ln;
            if (parsed.contains(QLatin1String("\\c{")) || parsed.contains(QLatin1String("\\p{"))
                || parsed.contains(QLatin1String("\\l("))) {
                hasTok = true;
                int pEnd = parsed.indexOf(parsed.contains(QLatin1String("\\l("))
                                                  ? QLatin1Char(')')
                                                  : QLatin1Char('}'));
                if (pEnd >= 0)
                    parsed = parsed.mid(pEnd + 1);
            }
            int sFrom = 0;
            int pPct = -1;
            while ((pPct = parsed.indexOf(QLatin1String("%("), sFrom)) >= 0) {
                const int pPct2 = parsed.indexOf(QLatin1Char(')'), pPct + 2);
                if (pPct2 < 0)
                    break;
                bool ok = false;
                const int ci = parsed.mid(pPct + 2, pPct2 - pPct - 2).toInt(&ok) - 1;
                if (ok && ci >= 0 && curve(ci)) {
                    const QString repl = curve(ci)->title().text();
                    parsed.replace(pPct, pPct2 - pPct + 1, repl);
                    sFrom = pPct + repl.size();
                } else {
                    sFrom = pPct2 + 1;
                }
            }
            QwtText qt(parsed);
            qt.setFont(dispLegFnt);
            const QSize tsz = qt.textSize();
            const int rw = int(std::ceil((hasTok ? sampleOff : 0) + qMax(4, tsz.width()) * dpiRatio));
            const int rh = qMax(8, int(std::ceil(tsz.height() * dpiRatio)));
            const double leftOff = 6.0 * zoomScale * dpiRatio;
            const QRectF rr(leftOff, double(curTop), double(rw), double(rh));
            rowVisualRects.append(rr);
            maxRight = qMax(maxRight, rr.right());
            curTop += rh;
        }
        if (!rowVisualRects.isEmpty()) {
            effLegW = qBound(16, int(std::ceil(maxRight)), legW);
            legH = qMax(legH, curTop + int(std::ceil(4.0 * zoomScale * dpiRatio)));
        }
    };

    recomputeLegendMetrics();

    if (!isPiePlot() && leg->numColumns() > 1) {
        const int estML = qMax(2, majLen - 4);
        const int estMR = qMax(6, majLen + 5);
        if (legW > qMax(24, cw - estML - estMR)) {
            leg->setNumColumns(1);
            framelessSingleCol = (leg->frameStyle() == Legend::None && leg->numColumns() <= 1);
            d_plot->updateLayout();
            recomputeLegendMetrics();
        }
    }
    if (!isPiePlot() && !d_is_ternary && leg->numColumns() <= 1 && cw >= 60) {
        const int maxFitLegW = qMax(36, cw - 10);
        if (effLegW > maxFitLegW) {
            QFont lf = leg->font();
            double curPt = lf.pointSizeF();
            if (curPt <= 0.0 && lf.pointSize() > 0)
                curPt = double(lf.pointSize());
            const double floorPt = (curPt > 14.0) ? 16.0 : 7.0;
            while (effLegW > maxFitLegW && curPt > floorPt + 0.1) {
                curPt = qMax(floorPt, curPt - ((curPt > 14.0) ? 1.0 : 0.5));
                lf.setPointSizeF(curPt);
                lf.setHintingPreference(QFont::PreferNoHinting);
                leg->setFont(lf);
                recomputeLegendMetrics();
            }
        }
    }

    if (isPiePlot()) {
        if (leg->text().contains(QLatin1String("\\p{"))
            && leg->text().contains(QLatin1Char('%'))) {
            static const QRegularExpression kPiePctRx(
                    QStringLiteral("^\\s*\\d+(?:\\.\\d+)?%\\s*$"));
            for (int tid : d_texts) {
                if (tid == legendMarkerID)
                    continue;
                Legend *tm = dynamic_cast<Legend *>(d_plot->marker(tid));
                if (tm && kPiePctRx.match(tm->text()).hasMatch()) {
                    tm->setText(QString());
                    tm->setVisible(false);
                }
            }
        }
        if ((anchor == LegendTopRight || anchor == LegendAutoAvoidData)
            && effLegW > int(0.34 * double(cw))) {
            anchor = LegendTopCenter;
            d_plot->setProperty("_toriLegendAnchor", int(anchor));
        }
    }

    const bool framelessLeg = (leg->frameStyle() == Legend::None);
    const int mL = framelessSingleCol ? qMax(2, majLen - 4) : qMax(m, majLen + 4);
    const int mR = framelessSingleCol ? qMax(6, majLen + 5) : qMax(m, majLen + 4);
    const int mT = isPiePlot() ? 4 : (framelessLeg ? qMax(3, majLen - 2) : qMax(m, majLen + 4));
    const int mB = framelessLeg ? qMax(3, majLen - 1) : qMax(m, majLen + 4);

    auto originForInsideAnchor = [&](LegendAnchor a) -> QPoint {
        const int xLeft = mL;
        const int xRight = qMax(2, cw - effLegW - mR);
        const int xCenter = qMax(2, (cw - effLegW) / 2);
        const int yTop = mT;
        const int yBottom = qMax(2, ch - legH - mB);
        switch (a) {
        case LegendTopRight:
            return QPoint(xRight, yTop);
        case LegendTopLeft:
            return QPoint(xLeft, yTop);
        case LegendBottomRight:
            return QPoint(xRight, yBottom);
        case LegendBottomLeft:
            return QPoint(xLeft, yBottom);
        case LegendTopCenter:
            return QPoint(xCenter, yTop);
        case LegendBottomCenter:
            return QPoint(xCenter, yBottom);
        default:
            return QPoint(xRight, yTop);
        }
    };

    auto anchorName = [](LegendAnchor a) -> QString {
        switch (a) {
        case LegendTopRight:
            return QStringLiteral("TopRight");
        case LegendTopLeft:
            return QStringLiteral("TopLeft");
        case LegendBottomRight:
            return QStringLiteral("BottomRight");
        case LegendBottomLeft:
            return QStringLiteral("BottomLeft");
        case LegendTopCenter:
            return QStringLiteral("TopCenter");
        case LegendBottomCenter:
            return QStringLiteral("BottomCenter");
        case LegendOutsideRight:
            return QStringLiteral("OutsideRight");
        default:
            return QStringLiteral("TopRight");
        }
    };

    auto liangBarskyClippedLength = [](double x0, double y0, double x1, double y1,
                                       const QRectF &box, bool *outHit) -> double {
        if (outHit)
            *outHit = false;
        const double dx = x1 - x0;
        const double dy = y1 - y0;
        double t0 = 0.0;
        double t1 = 1.0;

        auto clipEdge = [&](double p, double q) -> bool {
            if (std::fabs(p) < 1e-12)
                return q >= 0.0;
            const double r = q / p;
            if (p < 0.0) {
                if (r > t1)
                    return false;
                if (r > t0)
                    t0 = r;
            } else {
                if (r < t0)
                    return false;
                if (r < t1)
                    t1 = r;
            }
            return true;
        };

        if (!clipEdge(-dx, x0 - box.left()))
            return 0.0;
        if (!clipEdge(dx, box.right() - x0))
            return 0.0;
        if (!clipEdge(-dy, y0 - box.top()))
            return 0.0;
        if (!clipEdge(dy, box.bottom() - y0))
            return 0.0;
        if (t0 > t1)
            return 0.0;

        if (outHit)
            *outHit = true;
        return std::hypot((t1 - t0) * dx, (t1 - t0) * dy);
    };

    QList<Graph *> layersToScore;
    layersToScore << this;
    if (MultiLayer *ml = multiLayerParent()) {
        const QList<Graph *> allGraphs = ml->layersList();
        for (Graph *childGr : allGraphs) {
            if (childGr && childGr != this && childGr->linkedParentLayer() == this)
                layersToScore << childGr;
        }
    }

    auto evaluateCollisionScore = [&](const QRectF &candRect) -> double {
        const QRectF box = candRect.adjusted(-4.0, -4.0, 4.0, 4.0);
        double cost = 0.0;

        for (Graph *lg : layersToScore) {
            Plot *lp = lg ? lg->plotWidget() : nullptr;
            if (!lp)
                continue;

            for (int i = 0; i < lg->n_curves && i < lg->c_keys.size(); ++i) {
                if (!lg->isCurveVisible(i))
                    continue;
                QwtPlotItem *it = lg->plotItem(i);
                if (!it || !it->isVisible())
                    continue;

                // 1. Spectrogram hotspots
                if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    Spectrogram *sp = static_cast<Spectrogram *>(it);
                    const QwtDoubleInterval zRange = sp->effectiveValueRange();
                    const double zSpan = zRange.width();
                    if (zSpan > 1e-30) {
                        constexpr int kGrid = 7;
                        for (int gy = 0; gy < kGrid; ++gy) {
                            const double py = candRect.top()
                                    + (gy + 0.5) / double(kGrid) * candRect.height();
                            const double vy = lp->invTransform(sp->yAxis(), qRound(py));
                            for (int gx = 0; gx < kGrid; ++gx) {
                                const double px = candRect.left()
                                        + (gx + 0.5) / double(kGrid) * candRect.width();
                                const double vx = lp->invTransform(sp->xAxis(), qRound(px));
                                const double zVal = sp->data().value(vx, vy);
                                if (std::isfinite(zVal)) {
                                    const double norm =
                                            qBound(0.0, (zVal - zRange.minValue()) / zSpan, 1.0);
                                    cost += norm * norm * 12.0;
                                }
                            }
                        }
                    }
                    continue;
                }

                const int ct = (i < lg->c_type.size()) ? lg->c_type[i] : Line;

                // 2. Error-bar segments
                if (ct == ErrorBars) {
                    QwtErrorPlotCurve *er = dynamic_cast<QwtErrorPlotCurve *>(it);
                    if (!er || !er->masterCurve() || !er->masterCurve()->isVisible())
                        continue;
                    const QwtScaleMap xMap = lp->canvasMap(er->xAxis());
                    const QwtScaleMap yMap = lp->canvasMap(er->yAxis());
                    const int nPts = er->dataSize();
                    const int stride = (nPts > 4000) ? qMax(1, nPts / 2000) : 1;
                    for (int p = 0; p < nPts; p += stride) {
                        const double vx = er->x(p);
                        const double vy = er->y(p);
                        const double ev = er->errorValue(p);
                        if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(ev))
                            continue;
                        bool hit = false;
                        double clipLen = 0.0;
                        if (er->direction() == QwtErrorPlotCurve::Vertical) {
                            const double yLo = er->minusSide() ? (vy - ev) : vy;
                            const double yHi = er->plusSide() ? (vy + ev) : vy;
                            const double px = xMap.xTransform(vx);
                            clipLen = liangBarskyClippedLength(px, yMap.xTransform(yLo), px,
                                                               yMap.xTransform(yHi), box, &hit);
                        } else {
                            const double xLo = er->minusSide() ? (vx - ev) : vx;
                            const double xHi = er->plusSide() ? (vx + ev) : vx;
                            const double py = yMap.xTransform(vy);
                            clipLen = liangBarskyClippedLength(xMap.xTransform(xLo), py,
                                                               xMap.xTransform(xHi), py, box, &hit);
                        }
                        if (hit)
                            cost += (4.0 + clipLen * 1.2) * stride;
                    }
                    continue;
                }

                QwtPlotCurve *c = dynamic_cast<QwtPlotCurve *>(it);
                if (!c)
                    continue;
                const int nPts = c->dataSize();
                if (nPts <= 0)
                    continue;

                const QwtScaleMap xMap = lp->canvasMap(c->xAxis());
                const QwtScaleMap yMap = lp->canvasMap(c->yAxis());
                const DataCurve *dc = dynamic_cast<const DataCurve *>(c);

                // 3. Bars / Histograms
                if (ct == VerticalBars || ct == HorizontalBars || ct == Histogram) {
                    const QwtBarCurve *bc = dynamic_cast<const QwtBarCurve *>(c);
                    double stepPx = 28.0;
                    if (nPts >= 2) {
                        const double p0 = (ct == HorizontalBars) ? yMap.xTransform(c->y(0))
                                                                 : xMap.xTransform(c->x(0));
                        const double p1 = (ct == HorizontalBars) ? yMap.xTransform(c->y(1))
                                                                 : xMap.xTransform(c->x(1));
                        stepPx = qMax(6.0, std::fabs(p1 - p0));
                    } else {
                        stepPx = qMax(12.0,
                                      0.2 * std::fabs((ct == HorizontalBars) ? yMap.pDist()
                                                                             : xMap.pDist()));
                    }
                    const double gapFrac = bc ? (1.0 - double(bc->gap()) * 0.01) : 0.70;
                    const double barW = qBound(3.0, stepPx * gapFrac, 120.0);
                    const double shiftLeft =
                            bc ? ((0.5 - double(bc->offset()) * 0.01) * barW) : (0.5 * barW);
                    const double basePx = (ct == HorizontalBars) ? xMap.xTransform(c->baseline())
                                                                 : yMap.xTransform(c->baseline());
                    Column *barXCol = nullptr;
                    Column *barYCol = nullptr;
                    const bool barHasMasks = dc && dc->hasAnyMaskedPoints();
                    if (barHasMasks)
                        dc->resolveColumns(barXCol, barYCol);
                    for (int p = 0; p < nPts; ++p) {
                        if (barHasMasks && dc->isPointMaskedFast(p, barXCol, barYCol))
                            continue;
                        const double vx = c->x(p);
                        const double vy = c->y(p);
                        if (!std::isfinite(vx) || !std::isfinite(vy))
                            continue;
                        QRectF barRect;
                        if (ct == HorizontalBars) {
                            const double py = yMap.xTransform(vy);
                            const double px = xMap.xTransform(vx);
                            barRect = QRectF(qMin(basePx, px), py - shiftLeft,
                                             std::fabs(px - basePx), barW);
                        } else {
                            const double px = xMap.xTransform(vx);
                            const double py = yMap.xTransform(vy);
                            barRect = QRectF(px - shiftLeft, qMin(basePx, py), barW,
                                             std::fabs(py - basePx));
                        }
                        const QRectF inter = box.intersected(barRect);
                        if (!inter.isEmpty())
                            cost += 160.0 + inter.width() * inter.height() * 1.1;
                    }
                    continue;
                }

                // 4. Box / Violin plots
                if (ct == Box) {
                    double minY = 1e300, maxY = -1e300;
                    for (int p = 0; p < nPts; ++p) {
                        const double vy = c->y(p);
                        if (std::isfinite(vy)) {
                            minY = qMin(minY, vy);
                            maxY = qMax(maxY, vy);
                        }
                    }
                    if (minY <= maxY) {
                        const double px = xMap.xTransform(c->x(0));
                        const double py0 = yMap.xTransform(minY);
                        const double py1 = yMap.xTransform(maxY);
                        const QRectF boxRect(px - 24.0, qMin(py0, py1), 48.0,
                                             std::fabs(py1 - py0));
                        const QRectF inter = box.intersected(boxRect);
                        if (!inter.isEmpty())
                            cost += 40.0 + inter.width() * inter.height() * 0.25;
                    }
                    continue;
                }

                // 4b. Vector curves (shaft + arrowhead tip)
                if (ct == VectXYXY || ct == VectXYAM) {
                    VectorCurve *vc = dynamic_cast<VectorCurve *>(const_cast<QwtPlotCurve *>(c));
                    Table *vt = vc ? vc->table() : nullptr;
                    auto resolveVecCol = [vt](const QString &spec) -> Column * {
                        if (!vt || spec.isEmpty())
                            return nullptr;
                        const QString pfx = QString(vt->name()) + QLatin1Char('_');
                        if (spec.startsWith(pfx)) {
                            const QString sh = spec.mid(pfx.size());
                            for (int ci = 0; ci < vt->numCols(); ++ci) {
                                if (Column *col = vt->column(ci)) {
                                    if (col->name() == sh)
                                        return col;
                                }
                            }
                        }
                        for (int ci = 0; ci < vt->numCols(); ++ci) {
                            if (Column *col = vt->column(ci)) {
                                if (col->name() == spec)
                                    return col;
                            }
                        }
                        const int fb = vt->colIndex(spec);
                        return (fb >= 0 && fb < vt->numCols()) ? vt->column(fb) : nullptr;
                    };
                    Column *ec1 = vc ? resolveVecCol(vc->vectorEndXAColName()) : nullptr;
                    Column *ec2 = vc ? resolveVecCol(vc->vectorEndYMColName()) : nullptr;
                    const int vPos = vc ? vc->position() : int(VectorCurve::Tail);
                    const double headPad =
                            qMax(6.0, vc ? double(qMax(6, vc->headLength())) : 8.0);
                    const QRectF shaftBox = box.adjusted(-3.0, -3.0, 3.0, 3.0);
                    for (int p = 0; p < nPts; ++p) {
                        const double x0 = c->x(p);
                        const double y0 = c->y(p);
                        if (!std::isfinite(x0) || !std::isfinite(y0))
                            continue;
                        double xs = xMap.xTransform(x0);
                        double ys = yMap.xTransform(y0);
                        double xe = xs;
                        double ye = ys;
                        const int r = vc ? vc->tableRow(p) : -1;
                        if (ec1 && ec2 && r >= 0 && r < ec1->rowCount() && r < ec2->rowCount()
                            && !ec1->isInvalid(r) && !ec2->isInvalid(r)) {
                            const double v1 = ec1->valueAt(r);
                            const double v2 = ec2->valueAt(r);
                            if (std::isfinite(v1) && std::isfinite(v2)) {
                                if (ct == VectXYAM) {
                                    const double dx = v2 * std::cos(v1);
                                    const double dy = v2 * std::sin(v1);
                                    if (vPos == int(VectorCurve::Middle)) {
                                        xs = xMap.xTransform(x0 - 0.5 * dx);
                                        ys = yMap.xTransform(y0 - 0.5 * dy);
                                        xe = xMap.xTransform(x0 + 0.5 * dx);
                                        ye = yMap.xTransform(y0 + 0.5 * dy);
                                    } else if (vPos == int(VectorCurve::Head)) {
                                        xs = xMap.xTransform(x0 - dx);
                                        ys = yMap.xTransform(y0 - dy);
                                        xe = xMap.xTransform(x0);
                                        ye = yMap.xTransform(y0);
                                    } else {
                                        xe = xMap.xTransform(x0 + dx);
                                        ye = yMap.xTransform(y0 + dy);
                                    }
                                } else {
                                    xe = xMap.xTransform(v1);
                                    ye = yMap.xTransform(v2);
                                }
                            }
                        }
                        bool hit = false;
                        const double clipLen =
                                liangBarskyClippedLength(xs, ys, xe, ye, shaftBox, &hit);
                        if (hit)
                            cost += 120.0 + clipLen * 3.5;
                        const QRectF tipBox(xe - headPad, ye - headPad, 2.0 * headPad,
                                            2.0 * headPad);
                        if (box.intersects(tipBox))
                            cost += 220.0;
                    }
                    continue;
                }

                // 5. Standard 2D curve symbols + Liang-Barsky clipped curve segments
                const bool hasSymbols =
                        (c->symbol().style() != QwtSymbol::NoSymbol || ct == Scatter);
                const bool hasLines = (c->style() != QwtPlotCurve::NoCurve && ct != Scatter);
                const double symR = hasSymbols
                        ? qMax(4.0,
                               0.5 * qMax(c->symbol().size().width(), c->symbol().size().height()))
                        : 0.0;
                const QRectF symTestBox = box.adjusted(-symR, -symR, symR, symR);
                const double penWeight = 1.4 + 0.35 * qMax(1, c->pen().width());
                const int stride = (nPts > 6000) ? qMax(1, nPts / 3000) : 1;

                Column *dcXCol = nullptr;
                Column *dcYCol = nullptr;
                const bool dcHasMasks = dc && dc->hasAnyMaskedPoints();
                if (dcHasMasks)
                    dc->resolveColumns(dcXCol, dcYCol);

                bool hasPrev = false;
                double prevPx = 0.0, prevPy = 0.0;
                for (int p = 0; p < nPts; p += stride) {
                    if (dcHasMasks && dc->isPointMaskedFast(p, dcXCol, dcYCol)) {
                        hasPrev = false;
                        continue;
                    }
                    const double vx = c->x(p);
                    const double vy = c->y(p);
                    if (!std::isfinite(vx) || !std::isfinite(vy)) {
                        hasPrev = false;
                        continue;
                    }
                    const double px = xMap.xTransform(vx);
                    const double py = yMap.xTransform(vy);
                    if (!std::isfinite(px) || !std::isfinite(py)) {
                        hasPrev = false;
                        continue;
                    }

                    if (hasSymbols && symTestBox.contains(QPointF(px, py)))
                        cost += (45.0 + symR * 4.0) * stride;

                    if (hasLines && hasPrev) {
                        bool hit = false;
                        const double clipLen =
                                liangBarskyClippedLength(prevPx, prevPy, px, py, box, &hit);
                        if (hit)
                            cost += (6.0 + clipLen * penWeight) * stride;
                    }
                    prevPx = px;
                    prevPy = py;
                    hasPrev = true;
                }
            }

            // 6. Other text markers, image markers, arrow markers, and reference lines
            for (int tid : lg->d_texts) {
                if (lg == this && tid == legendMarkerID)
                    continue;
                Legend *tm = dynamic_cast<Legend *>(lp->marker(tid));
                if (tm && tm->isVisible()) {
                    QRectF tmRect(tm->rect());
                    if (tmRect.width() <= 2.0 || tmRect.height() <= 2.0) {
                        const double tx = lp->canvasMap(tm->xAxis()).xTransform(tm->xValue());
                        const double ty = lp->canvasMap(tm->yAxis()).xTransform(tm->yValue());
                        QwtText qt(tm->text());
                        qt.setFont(tm->font());
                        const QSize tsz = qt.textSize();
                        if (std::isfinite(tx) && std::isfinite(ty))
                            tmRect = QRectF(tx, ty, qMax(8, tsz.width() + 8),
                                            qMax(8, tsz.height() + 6));
                    }
                    const QRectF inter = box.intersected(tmRect);
                    if (!inter.isEmpty())
                        cost += 380.0 + inter.width() * inter.height() * 1.2;
                }
            }
            for (int iid : lg->d_images) {
                ImageMarker *im = dynamic_cast<ImageMarker *>(lp->marker(iid));
                if (im && im->isVisible()) {
                    const QRectF inter = box.intersected(QRectF(im->rect()));
                    if (!inter.isEmpty())
                        cost += 200.0 + inter.width() * inter.height() * 0.5;
                }
            }
            for (int lid : lg->d_lines) {
                ArrowMarker *am = dynamic_cast<ArrowMarker *>(lp->marker(lid));
                if (am && am->isVisible()) {
                    const QwtDoublePoint sC = am->startPointCoord();
                    const QwtDoublePoint eC = am->endPointCoord();
                    double sx0 = lp->canvasMap(am->xAxis()).xTransform(sC.x());
                    double sy0 = lp->canvasMap(am->yAxis()).xTransform(sC.y());
                    double sx1 = lp->canvasMap(am->xAxis()).xTransform(eC.x());
                    double sy1 = lp->canvasMap(am->yAxis()).xTransform(eC.y());
                    if (!std::isfinite(sx0) || !std::isfinite(sy0) || !std::isfinite(sx1)
                        || !std::isfinite(sy1)) {
                        const QPoint sPt = am->startPoint();
                        const QPoint ePt = am->endPoint();
                        sx0 = sPt.x();
                        sy0 = sPt.y();
                        sx1 = ePt.x();
                        sy1 = ePt.y();
                    }
                    bool hit = false;
                    const double clipLen = liangBarskyClippedLength(sx0, sy0, sx1, sy1, box, &hit);
                    if (hit)
                        cost += 450.0 + clipLen * 6.0;
                }
            }
            for (const ReferenceLineSpec &rl : lg->d_ref_lines) {
                if (!rl.visible)
                    continue;
                bool hit = false;
                double clipLen = 0.0;
                if (rl.orientation == 0) {
                    const double py = lp->canvasMap(QwtPlot::yLeft).xTransform(rl.value);
                    clipLen = liangBarskyClippedLength(0.0, py, double(cw), py, box, &hit);
                } else {
                    const double px = lp->canvasMap(QwtPlot::xBottom).xTransform(rl.value);
                    clipLen = liangBarskyClippedLength(px, 0.0, px, double(ch), box, &hit);
                }
                if (hit)
                    cost += 35.0 + clipLen * 0.8;
            }

            if (lg->isTernaryPlot()) {
                const QwtScaleMap txMap = lp->canvasMap(QwtPlot::xBottom);
                const QwtScaleMap tyMap = lp->canvasMap(QwtPlot::yLeft);
                const double th = std::sqrt(3.0) * 0.5;
                const QPointF vA(txMap.xTransform(0.0), tyMap.xTransform(0.0));
                const QPointF vB(txMap.xTransform(1.0), tyMap.xTransform(0.0));
                const QPointF vC(txMap.xTransform(0.5), tyMap.xTransform(th));
                const QPointF edges[3][2] = { { vA, vB }, { vB, vC }, { vC, vA } };
                for (int ei = 0; ei < 3; ++ei) {
                    bool hit = false;
                    const double cl = liangBarskyClippedLength(edges[ei][0].x(), edges[ei][0].y(),
                                                               edges[ei][1].x(), edges[ei][1].y(),
                                                               box, &hit);
                    if (hit)
                        cost += 600.0 + cl * 5.0;
                }
                QPolygonF triPoly(3);
                triPoly[0] = vA;
                triPoly[1] = vB;
                triPoly[2] = vC;
                if (triPoly.containsPoint(box.center(), Qt::OddEvenFill))
                    cost += 900.0;
            } else if (lg->isPolarPlot()) {
                const QwtScaleMap pxMap = lp->canvasMap(QwtPlot::xBottom);
                const QwtScaleMap pyMap = lp->canvasMap(QwtPlot::yLeft);
                const double pcx = pxMap.xTransform(0.0);
                const double pcy = pyMap.xTransform(0.0);
                const double pr = 1.24
                        * std::fabs(pxMap.xTransform(lg->polarRadiusMax()) - pcx);
                const double nearestX = qBound(box.left(), pcx, box.right());
                const double nearestY = qBound(box.top(), pcy, box.bottom());
                const double dist = std::hypot(nearestX - pcx, nearestY - pcy);
                if (dist < pr)
                    cost += 800.0 + (pr - dist) * 6.0;
            } else if (lg->isPiePlot()) {
                double pcx = 0.36 * double(cw);
                double pcy = 0.52 * double(ch);
                double pr = 1.25 * 0.285 * double(qMin(cw, ch));
                if (effLegW > int(0.34 * double(cw))) {
                    const double legBottom =
                            qBound(0.08 * double(ch), double(4 + legH), 0.52 * double(ch));
                    const double pad = 0.028 * double(ch);
                    const double availH = qMax(0.20 * double(ch), double(ch) - legBottom - 2.0 * pad);
                    pcx = 0.50 * double(cw);
                    pcy = legBottom + pad + 0.5 * availH;
                    pr = 1.05 * qMin(0.31 * double(cw), 0.44 * availH);
                }
                const double nearestX = qBound(box.left(), pcx, box.right());
                const double nearestY = qBound(box.top(), pcy, box.bottom());
                const double dist = std::hypot(nearestX - pcx, nearestY - pcy);
                if (dist < pr)
                    cost += 800.0 + (pr - dist) * 6.0;
            }
        }

        // 7. Child inset frames, zoom ROI boxes (+ leader lines), and inside panel labels
        if (MultiLayer *ml = multiLayerParent()) {
            const QPoint myCanvasOrigin = pos() + d_plot->plotLayout()->canvasRect().topLeft();
            const QRectF localCanvasF(0.0, 0.0, double(cw), double(ch));
            const QwtScaleMap myXMap = d_plot->canvasMap(QwtPlot::xBottom);
            const QwtScaleMap myYMap = d_plot->canvasMap(QwtPlot::yLeft);
            QVector<QRectF> insetAndRoiBoxes;

            const QList<Graph *> allGraphs = ml->layersList();
            for (Graph *ins : allGraphs) {
                if (!ins || ins == this || ins->isHidden() || !ml->isInsetLayer(ins)
                    || ml->insetSourceLayer(ins) != this)
                    continue;
                Plot *inPlot = ins->plotWidget();
                const QSize insSz = inPlot ? inPlot->size() : ins->size();
                const QRectF insBox = QRectF(QPointF(ins->pos() - myCanvasOrigin), QSizeF(insSz))
                                              .adjusted(-4.0, -4.0, 4.0, 4.0);
                insetAndRoiBoxes.append(insBox);
                const QRectF insInter = box.intersected(insBox);
                if (!insInter.isEmpty())
                    cost += 2000.0 + insInter.width() * insInter.height() * 2.0;

                QRectF zoomData = ml->insetZoomDataRect(ins);
                if (inPlot) {
                    const QwtScaleDiv *ixSd = inPlot->axisScaleDiv(QwtPlot::xBottom);
                    const QwtScaleDiv *iySd = inPlot->axisScaleDiv(QwtPlot::yLeft);
                    if (ixSd && iySd) {
                        const double xLo = qMin(ixSd->lowerBound(), ixSd->upperBound());
                        const double xHi = qMax(ixSd->lowerBound(), ixSd->upperBound());
                        const double yLo = qMin(iySd->lowerBound(), iySd->upperBound());
                        const double yHi = qMax(iySd->lowerBound(), iySd->upperBound());
                        if (std::isfinite(xLo) && std::isfinite(xHi) && std::isfinite(yLo)
                            && std::isfinite(yHi) && (xHi > xLo + 1e-15 || yHi > yLo + 1e-15)) {
                            zoomData = QRectF(QPointF(xLo, yLo), QPointF(xHi, yHi));
                        }
                    }
                }
                if (!zoomData.isEmpty() || qAbs(zoomData.width()) > 1e-15
                    || qAbs(zoomData.height()) > 1e-15) {
                    const double rx0 = myXMap.xTransform(zoomData.left());
                    const double rx1 = myXMap.xTransform(zoomData.right());
                    const double ry0 = myYMap.xTransform(zoomData.top());
                    const double ry1 = myYMap.xTransform(zoomData.bottom());
                    if (std::isfinite(rx0) && std::isfinite(rx1) && std::isfinite(ry0)
                        && std::isfinite(ry1)) {
                        const QRectF rawRoi(QPointF(qMin(rx0, rx1), qMin(ry0, ry1)),
                                            QPointF(qMax(rx0, rx1), qMax(ry0, ry1)));
                        const QRectF roiBox = rawRoi.intersected(localCanvasF);
                        if (roiBox.width() >= 2.0 && roiBox.height() >= 2.0) {
                            const QRectF padRoi = roiBox.adjusted(-3.0, -3.0, 3.0, 3.0);
                            insetAndRoiBoxes.append(padRoi);
                            const QRectF roiInter = box.intersected(padRoi);
                            if (!roiInter.isEmpty())
                                cost += 800.0 + roiInter.width() * roiInter.height() * 1.5;

                            if (ml->insetShowConnectors(ins) && inPlot) {
                                const QRect inCanvasLocal = inPlot->plotLayout()->canvasRect();
                                if (inCanvasLocal.width() >= 12 && inCanvasLocal.height() >= 12) {
                                    const QRectF insetFrame(
                                            inCanvasLocal.translated(ins->pos() - myCanvasOrigin));
                                    const QPointF rC = roiBox.center();
                                    const QPointF iC = insetFrame.center();
                                    bool lHit = false;
                                    const double clipLen = liangBarskyClippedLength(
                                            rC.x(), rC.y(), iC.x(), iC.y(), box, &lHit);
                                    if (lHit)
                                        cost += 180.0 + clipLen * 2.5;
                                }
                            }
                        }
                    }
                }
            }

            if (ml->panelLabelsEnabled()
                && ml->panelLabelPlacement() == MultiLayer::PanelInsideTopLeft
                && !ml->isInsetLayer(this) && !linkedParentLayer()) {
                double pt = double(ml->panelLabelFontPt());
                if (pt <= 0.0) {
                    const QFont tf = axisTitleFont(QwtPlot::yLeft);
                    const double titlePt = tf.pointSizeF() > 0.0
                            ? tf.pointSizeF()
                            : (tf.pointSize() > 0 ? double(tf.pointSize()) : 0.0);
                    pt = (titlePt > 0.0)
                            ? qBound(9.0, titlePt >= 24.0 ? titlePt : titlePt + 1.0, 40.0)
                            : 12.0;
                }
                QFont pf(QStringLiteral("Arial"));
                pf.setPointSizeF(pt);
                pf.setWeight(QFont::Normal);
                pf.setBold(false);
                pf.setHintingPreference(QFont::PreferNoHinting);
                const QFontMetrics pfm(pf);
                const int lblW = qMax(18, pfm.horizontalAdvance(QStringLiteral("(a)")));
                const int lblH = pfm.height();
                const int minX = 8;
                const int maxX = qMax(minX, cw - lblW - 8);
                const int minY = 6;
                const int maxY = qMax(minY, ch - lblH - 8);

                auto hitsInsetOrRoi = [&](int lx, int ly) -> bool {
                    const QRectF lb(lx - 4, ly - 4, lblW + 8, lblH + 8);
                    for (const QRectF &ob : insetAndRoiBoxes) {
                        if (ob.intersects(lb))
                            return true;
                    }
                    return false;
                };

                int px = minX;
                int py = minY;
                if (hitsInsetOrRoi(px, py)) {
                    bool placed = false;
                    for (int tx = minX; tx <= maxX && !placed; tx += 12) {
                        if (!hitsInsetOrRoi(tx, py)) {
                            px = tx;
                            placed = true;
                        }
                    }
                    for (int ty = minY + 12; ty <= maxY && !placed; ty += 12) {
                        if (!hitsInsetOrRoi(minX, ty)) {
                            px = minX;
                            py = ty;
                            placed = true;
                        }
                    }
                }
                const QRectF panelLblBox(px - 4.0, py - 4.0, lblW + 8.0, lblH + 8.0);
                const QRectF plInter = box.intersected(panelLblBox);
                if (!plInter.isEmpty())
                    cost += 900.0 + plInter.width() * plInter.height() * 1.5;
            }
        }

        return cost;
    };

    auto evaluateOriginScore = [&](int ox, int oy) -> double {
        const double fullBoxScore = evaluateCollisionScore(QRectF(ox, oy, legW, legH));
        if (framelessSingleCol && !rowVisualRects.isEmpty()) {
            double sum = 0.0;
            for (const QRectF &rr : rowVisualRects)
                sum += evaluateCollisionScore(rr.translated(double(ox), double(oy)));
            return sum + 0.35 * fullBoxScore;
        }
        return fullBoxScore;
    };

    auto ensureHorizBarCategoryHeadroom = [&]() -> bool {
        if (d_is_ternary || d_is_polar || isPoleFigurePlot() || isPiePlot()
            || !d_plot->axisEnabled(QwtPlot::yLeft)
            || d_axis_breaks[QwtPlot::yLeft].enabled) {
            return false;
        }
        bool hasHorizBars = false;
        for (int i = 0; i < n_curves && i < c_type.size(); ++i) {
            if (c_type[i] == HorizontalBars) {
                hasHorizBars = true;
                break;
            }
        }
        const QwtTextScaleDraw *yCatSd =
                dynamic_cast<const QwtTextScaleDraw *>(d_plot->axisScaleDraw(QwtPlot::yLeft));
        const QwtScaleDiv *yCatDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
        const QwtScaleEngine *yCatEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
        if (!hasHorizBars || !yCatSd || !yCatDiv || !yCatEng
            || yCatEng->testAttribute(QwtScaleEngine::Inverted)) {
            return false;
        }
        const QMap<int, QString> catMap = yCatSd->labelsMap();
        if (catMap.isEmpty())
            return false;
        const double minKey = double(catMap.firstKey());
        const double maxKey = double(catMap.lastKey());
        const double curLo =
                qMin(minKey - 0.5, qMin(yCatDiv->lowerBound(), yCatDiv->upperBound()));
        const double curHi =
                qMax(maxKey + 0.5, qMax(yCatDiv->lowerBound(), yCatDiv->upperBound()));
        const double topBarY = maxKey + 0.46;
        const double frac =
                qBound(0.14, double(legH + mT + 8) / double(qMax(60, ch)), 0.52);
        const double neededCatHi =
                qMin(maxKey + 4.2, (topBarY - frac * curLo) / (1.0 - frac));
        if (neededCatHi <= curHi + 0.05)
            return false;
        QwtValueList catMaj;
        for (auto it = catMap.constBegin(); it != catMap.constEnd(); ++it)
            catMaj += double(it.key());
        QwtValueList emptyTicks;
        QwtValueList catTicks[QwtScaleDiv::NTickTypes];
        catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
        catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
        catTicks[QwtScaleDiv::MajorTick] = catMaj;
        QwtScaleEngine *newCatEng = new QwtLinearScaleEngine();
        QwtScaleDiv newCatDiv(curLo, neededCatHi, catTicks);
        setToriAxisUserLocked(d_plot, QwtPlot::yLeft, true);
        d_plot->setAxisMaxMinor(QwtPlot::yLeft, 0);
        d_plot->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::None);
        d_plot->setAxisMaxMajor(QwtPlot::yLeft, qMax(8, catMap.size() + 2));
        d_plot->setAxisScale(QwtPlot::yLeft, curLo, neededCatHi, 1.0);
        d_plot->setAxisScaleEngine(QwtPlot::yLeft, newCatEng);
        d_plot->setAxisScaleDiv(QwtPlot::yLeft, newCatDiv);
        updateSecondaryAxis(QwtPlot::yRight);
        d_zoomer[0]->setZoomBase();
        d_plot->replot();
        d_plot->updateLayout();
        cw = d_plot->canvas()->width();
        ch = d_plot->canvas()->height();
        syncChildLinkedLayers();
        return true;
    };

    LegendAnchor chosenAnchor = anchor;
    double chosenScore = 0.0;
    bool useGridOrigin = false;
    QPoint bestGridOrigin;

    if (anchor == LegendAutoAvoidData) {
        // Tie-break order required by spec: TR -> TL -> BR -> BL -> TC -> BC
        static const LegendAnchor kCandidates[] = {
            LegendTopRight,   LegendTopLeft,   LegendBottomRight,
            LegendBottomLeft, LegendTopCenter, LegendBottomCenter
        };
        const QRectF canvasBoundsF(0.0, 0.0, static_cast<double>(cw), static_cast<double>(ch));
        auto evaluateProximityScore = [&](int ox, int oy) -> double {
            const QRectF baseBox(static_cast<double>(ox), static_cast<double>(oy),
                                 static_cast<double>(effLegW), static_cast<double>(legH));
            const double nearPadX = qMax(14.0, 0.07 * static_cast<double>(cw));
            const double nearPadY = qMax(14.0, 0.08 * static_cast<double>(ch));
            const double farPadX = qMax(28.0, 0.14 * static_cast<double>(cw));
            const double farPadY = qMax(28.0, 0.16 * static_cast<double>(ch));
            const QRectF nearBox =
                    baseBox.adjusted(-nearPadX, -nearPadY, nearPadX, nearPadY).intersected(canvasBoundsF);
            const QRectF farBox =
                    baseBox.adjusted(-farPadX, -farPadY, farPadX, farPadY).intersected(canvasBoundsF);
            return evaluateCollisionScore(nearBox) + 0.15 * evaluateCollisionScore(farBox);
        };
        auto scoreAllCandidates = [&]() {
            chosenAnchor = kCandidates[0];
            chosenScore = 1e300;
            double chosenProx = 1e300;
            useGridOrigin = false;
            for (LegendAnchor cand : kCandidates) {
                const QPoint pt = originForInsideAnchor(cand);
                const double score = evaluateOriginScore(pt.x(), pt.y());
                const double prox = evaluateProximityScore(pt.x(), pt.y());
                const bool strictlyLowerHard = (score < chosenScore - 1e-6);
                const bool tiedHardBetterClearance =
                        (qAbs(score - chosenScore) <= 1e-6 && prox < chosenProx - 1e-6);
                if (strictlyLowerHard || tiedHardBetterClearance) {
                    chosenScore = score;
                    chosenProx = prox;
                    chosenAnchor = cand;
                }
            }

            // 25x25 continuous 2D grid search (including middle-left/right/top/bottom margins)
            // when all 6 standard anchors collide (> 1.0)
            const int xMinGrid = qMin(mL, qMax(2, cw - effLegW - 2));
            const int xMaxGrid = qMax(xMinGrid, cw - effLegW - qMin(mR, 4));
            const int yMinGrid = qMin(mT, qMax(2, ch - legH - 2));
            const int yMaxGrid = qMax(yMinGrid, ch - legH - qMin(mB, 4));
            if (chosenScore > 1.0 && (xMaxGrid > xMinGrid || yMaxGrid > yMinGrid)) {
                double bestGridScore = 1e300;
                QPoint bestPt;
                constexpr int kGridN = 25;
                for (int gy = 0; gy < kGridN; ++gy) {
                    const int py = (yMaxGrid > yMinGrid)
                            ? (yMinGrid
                               + qRound((double(gy) / double(kGridN - 1)) * (yMaxGrid - yMinGrid)))
                            : yMinGrid;
                    for (int gx = 0; gx < kGridN; ++gx) {
                        // Prefer right before left on ties by scanning gx from right to left
                        const int rGx = (kGridN - 1) - gx;
                        const int px = (xMaxGrid > xMinGrid)
                                ? (xMinGrid
                                   + qRound((double(rGx) / double(kGridN - 1))
                                            * (xMaxGrid - xMinGrid)))
                                : xMinGrid;
                        const bool isCorner =
                                (rGx == 0 || rGx == kGridN - 1) && (gy == 0 || gy == kGridN - 1);
                        const double penalty = isCorner ? 0.0 : 0.05;
                        const double sc = evaluateOriginScore(px, py) + penalty;
                        if (sc < bestGridScore - 1e-9) {
                            bestGridScore = sc;
                            bestPt = QPoint(px, py);
                        }
                    }
                }
                if (bestGridScore + 0.5 < chosenScore) {
                    chosenScore = bestGridScore;
                    bestGridOrigin = bestPt;
                    useGridOrigin = true;
                    // Map nearest standard anchor name for RPC return value
                    double nearestDist2 = 1e300;
                    for (LegendAnchor cand : kCandidates) {
                        const QPoint cp = originForInsideAnchor(cand);
                        const double d2 = double(cp.x() - bestPt.x()) * double(cp.x() - bestPt.x())
                                + double(cp.y() - bestPt.y()) * double(cp.y() - bestPt.y());
                        if (d2 < nearestDist2) {
                            nearestDist2 = d2;
                            chosenAnchor = cand;
                        }
                    }
                }
            }
        };

        scoreAllCandidates();

        // Compact 2-column attempt when >= 4 visible series make a tall single-column legend
        // collide on small canvases (e.g. 6-row spin-polarized DOS on 85 mm ISIJ figures).
        if (chosenScore > 12.0 && leg->numColumns() == 1 && !isPiePlot() && !d_is_ternary
            && !d_is_polar && !isPoleFigurePlot() && legH > int(0.32 * double(qMax(60, ch)))) {
            int visCurves = 0;
            for (int ci = 0; ci < n_curves; ++ci) {
                if (isCurveVisible(ci) && (ci >= c_type.size() || c_type[ci] != ErrorBars))
                    ++visCurves;
            }
            if (visCurves >= 4) {
                const double score1Col = chosenScore;
                const LegendAnchor anchor1Col = chosenAnchor;
                const bool grid1Col = useGridOrigin;
                const QPoint pt1Col = bestGridOrigin;
                leg->setNumColumns(2);
                framelessSingleCol = false;
                d_plot->updateLayout();
                recomputeLegendMetrics();
                if (legW <= cw - mL - mR && legW <= cw - 8) {
                    scoreAllCandidates();
                    if (chosenScore > score1Col - 2.0 && legH * 13 >= ch * 5) {
                        leg->setNumColumns(1);
                        framelessSingleCol = (leg->frameStyle() == Legend::None);
                        d_plot->updateLayout();
                        recomputeLegendMetrics();
                        chosenScore = score1Col;
                        chosenAnchor = anchor1Col;
                        useGridOrigin = grid1Col;
                        bestGridOrigin = pt1Col;
                    }
                } else {
                    leg->setNumColumns(1);
                    framelessSingleCol = (leg->frameStyle() == Legend::None);
                    d_plot->updateLayout();
                    recomputeLegendMetrics();
                    chosenScore = score1Col;
                    chosenAnchor = anchor1Col;
                    useGridOrigin = grid1Col;
                    bestGridOrigin = pt1Col;
                }
            }
        }

        if (chosenScore > 35.0 && ensureHorizBarCategoryHeadroom())
            scoreAllCandidates();

        // Smart Y-Headroom: when every inside candidate AND 25x25 grid pocket collides (> 12.0),
        // expand a linear non-inverted non-broken yLeft upper bound enough to clear the legend
        // height (at least +22%, snapped up to the current major step) so TopRight / TopLeft /
        // TopCenter gets completely curve-free headroom above the data.
        if (chosenScore > 12.0 && !d_is_ternary && !d_is_polar && !isPoleFigurePlot()
            && !isPiePlot() && d_plot->axisEnabled(QwtPlot::yLeft)
            && !d_axis_breaks[QwtPlot::yLeft].enabled
            && !isToriAxisUserLocked(d_plot, QwtPlot::yLeft)
            && d_user_step[QwtPlot::yLeft] <= 0.0) {
            bool hasSpectrogram = false;
            double maxCurveY = -std::numeric_limits<double>::infinity();
            for (int i = 0; i < n_curves && i < c_keys.size(); ++i) {
                QwtPlotItem *it = plotItem(i);
                if (!it || !it->isVisible())
                    continue;
                if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    hasSpectrogram = true;
                    break;
                }
                if (const QwtPlotCurve *cv = curve(i)) {
                    if (cv->yAxis() == QwtPlot::yLeft) {
                        const int np = cv->dataSize();
                        for (int k = 0; k < np; ++k) {
                            const double yv = cv->y(k);
                            if (std::isfinite(yv))
                                maxCurveY = std::max(maxCurveY, yv);
                        }
                    }
                }
            }
            const QwtScaleDiv *yDiv = d_plot->axisScaleDiv(QwtPlot::yLeft);
            const QwtScaleEngine *yEng = d_plot->axisScaleEngine(QwtPlot::yLeft);
            if (!hasSpectrogram && yDiv && yEng && !Plot::isLog10ScaleEngine(yEng)
                && !yEng->testAttribute(QwtScaleEngine::Inverted)) {
                const double yLo = yDiv->lowerBound();
                const double yHi = yDiv->upperBound();
                const double span = yHi - yLo;
                if (span > 1e-30 && std::isfinite(span)) {
                    const QwtValueList majTicks = yDiv->ticks(QwtScaleDiv::MajorTick);
                    const double yStep = (majTicks.size() >= 2)
                            ? std::fabs(majTicks[1] - majTicks[0])
                            : d_user_step[QwtPlot::yLeft];
                    const double dataTop = (std::isfinite(maxCurveY) && maxCurveY > yLo)
                            ? std::min(yHi, maxCurveY)
                            : yHi;
                    const double reqFrac =
                            qBound(0.18, double(legH + mT + 10) / double(qMax(60, ch)), 0.56);
                    const double neededYHi = (dataTop - reqFrac * yLo) / (1.0 - reqFrac);
                    double newYHi = std::max(yHi + 0.22 * span, neededYHi);
                    if (yStep > 1e-30 && std::isfinite(yStep))
                        newYHi = std::ceil((newYHi - 1e-9 * yStep) / yStep) * yStep;

                    const int maj = d_plot->axisMaxMajor(QwtPlot::yLeft);
                    const int minT = d_plot->axisMaxMinor(QwtPlot::yLeft);
                    const int maxMinInt = (minT >= 1) ? (minT + 1) : 0;
                    double useStep = d_user_step[QwtPlot::yLeft];
                    if (useStep <= 0.0 && yStep > 1e-30) {
                        const double nSteps = (newYHi - yLo) / yStep;
                        if (nSteps <= double(qMax(maj + 2, 7)) + 1e-6)
                            useStep = yStep;
                    }
                    QwtLinearScaleEngine *newEng = new QwtLinearScaleEngine();
                    QwtScaleDiv newDiv =
                            newEng->divideScale(yLo, newYHi, qMax(maj, 5), maxMinInt, useStep);
                    if (minT == 1)
                        ensureLinearHalfStepMinorTicks(newDiv);
                    d_plot->setAxisScale(QwtPlot::yLeft, yLo, newYHi, useStep);
                    d_plot->setAxisScaleEngine(QwtPlot::yLeft, newEng);
                    d_plot->setAxisScaleDiv(QwtPlot::yLeft, newDiv);
                    updateSecondaryAxis(QwtPlot::yRight);
                    d_zoomer[0]->setZoomBase();
                    d_plot->replot();
                    d_plot->updateLayout();
                    cw = d_plot->canvas()->width();
                    ch = d_plot->canvas()->height();
                    syncChildLinkedLayers();

                    scoreAllCandidates();
                }
            }
        }

        // Gentle Compact Font Step-Down: when all pockets still collide (> 8.0) and axes are
        // locked (e.g. cramped Dual-Y or long labels), step legend font down by up to 1.5 pt
        // (floored at 7.5 pt for journal / 18 pt for slides) only if it materially cuts overlap.
        if (chosenScore > 8.0) {
            const QFont origLegFnt = leg->font();
            double curPt = origLegFnt.pointSizeF();
            if (curPt <= 0.0 && origLegFnt.pointSize() > 0)
                curPt = double(origLegFnt.pointSize());
            const double minAllowedPt = (curPt > 14.0) ? qMax(18.0, curPt - 3.0) : 7.5;
            if (curPt > minAllowedPt + 0.2) {
                const double prevScore = chosenScore;
                const LegendAnchor prevAnchor = chosenAnchor;
                const bool prevUseGrid = useGridOrigin;
                const QPoint prevGridPt = bestGridOrigin;
                QFont bestFnt = origLegFnt;

                for (int step = 1; step <= 2; ++step) {
                    const double tryPt = qMax(minAllowedPt, curPt - 0.75 * double(step));
                    QFont tryFnt = origLegFnt;
                    tryFnt.setPointSizeF(tryPt);
                    tryFnt.setHintingPreference(QFont::PreferNoHinting);
                    leg->setFont(tryFnt);
                    recomputeLegendMetrics();
                    scoreAllCandidates();
                    bestFnt = tryFnt;
                    if (chosenScore <= 1.0 || tryPt <= minAllowedPt + 1e-6)
                        break;
                }

                if (chosenScore >= prevScore - 4.0) {
                    leg->setFont(origLegFnt);
                    recomputeLegendMetrics();
                    chosenScore = prevScore;
                    chosenAnchor = prevAnchor;
                    useGridOrigin = prevUseGrid;
                    bestGridOrigin = prevGridPt;
                } else {
                    leg->setFont(bestFnt);
                }
            }
        }
    } else {
        QPoint pt = originForInsideAnchor(chosenAnchor);
        chosenScore = evaluateOriginScore(pt.x(), pt.y());
        if (chosenScore > 35.0
            && (chosenAnchor == LegendTopRight || chosenAnchor == LegendTopLeft
                || chosenAnchor == LegendTopCenter)
            && ensureHorizBarCategoryHeadroom()) {
            pt = originForInsideAnchor(chosenAnchor);
            chosenScore = evaluateOriginScore(pt.x(), pt.y());
        }
    }

    if (outOverlapScore)
        *outOverlapScore = chosenScore;

    leg->setOrigin(useGridOrigin ? bestGridOrigin : originForInsideAnchor(chosenAnchor));
    d_plot->replot();
    unzoomGuard.restoreNow();
    updateSelectionOverlayGeometry();
    emit modifiedGraph();
    return anchorName(chosenAnchor);
}




