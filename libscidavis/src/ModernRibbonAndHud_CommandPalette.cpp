/***************************************************************************
    File                 : ModernRibbonAndHud_CommandPalette.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : Ctrl+K Instant Fuzzy-Search Command Palette Popup
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

#include "ModernRibbonAndHud.h"
#include "ModernRibbonAndHud_Internal.h"
#include "UxNavigation.h"
#include "Workbook.h"
#include "XlsxImport.h"

#include <QToolTip>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QIdentityProxyModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QListWidgetItem>
#include <QLocale>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "ApplicationWindow.h"
#include "CurveMathTools.h"
#include "ExtraPlotTypes.h"
#include "SettingsTransfer.h"
#include "Graph.h"
#include "GraphInspectorDock.h"
#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "MyWidget.h"
#include "Note.h"
#include "PlotCurve.h"
#include "RecalcEngine.h"
#include "Table.h"
#include "Graph3D.h"
#include "OleServer.h"
#include "PptxExport.h"
#include "I18nJapanese.h"
#include "future/core/column/Column.h"
#include "future/table/TableModel.h"
#include "future/table/future_Table.h"

#include <QAbstractSpinBox>
#include <QDateTime>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTextEdit>
#include <mutex>

namespace {

enum PaletteItemRoles {
    RoleEntryIndex = Qt::UserRole + 1,
    RoleShortcutText = Qt::UserRole + 2,
    RoleCategoryText = Qt::UserRole + 3,
    RoleDisabledEntry = Qt::UserRole + 4,
    RoleDisabledReason = Qt::UserRole + 5
};

static QString cleanMnemonicText(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        if (raw.at(i) == QLatin1Char('&')) {
            if (i + 1 < raw.size() && raw.at(i + 1) == QLatin1Char('&')) {
                out.append(QLatin1Char('&'));
                ++i;
            }
            continue;
        }
        out.append(raw.at(i));
    }
    return out.trimmed();
}

class CommandPaletteItemDelegate : public QStyledItemDelegate
{
public:
    explicit CommandPaletteItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        const QString reason = index.data(RoleDisabledReason).toString();
        return QSize(480, reason.isEmpty() ? 38 : 54);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect rowRect = option.rect.adjusted(4, 2, -4, -2);
        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered = (option.state & QStyle::State_MouseOver);

        if (selected) {
            painter->setPen(QPen(QColor(37, 99, 235, 90), 1.0));
            painter->setBrush(QColor(239, 246, 255));
            painter->drawRoundedRect(rowRect, 7.0, 7.0);
        } else if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(248, 250, 252));
            painter->drawRoundedRect(rowRect, 7.0, 7.0);
        }

        const QString shortcut = index.data(RoleShortcutText).toString();
        const QString category = index.data(RoleCategoryText).toString();
        const QString title = index.data(Qt::DisplayRole).toString();
        const QString reason = index.data(RoleDisabledReason).toString();
        const bool hasReason = !reason.isEmpty();
        const bool dimmed = index.data(RoleDisabledEntry).toBool();

        if (dimmed)
            painter->setOpacity(0.68);

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const int iconPx = 20;
        const QRect iconRect(rowRect.left() + 10,
                             rowRect.top() + (rowRect.height() - iconPx) / 2, iconPx, iconPx);
        if (!icon.isNull())
            icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        const int topH = hasReason ? 26 : rowRect.height();
        int rightEdge = rowRect.right() - 10;
        if (!shortcut.isEmpty()) {
            QFont badgeFont(QStringLiteral("Segoe UI"), 10);
            badgeFont.setWeight(QFont::DemiBold);
            painter->setFont(badgeFont);
            const QFontMetrics bfm(badgeFont);
            const int bw = bfm.horizontalAdvance(shortcut) + 14;
            const int bh = 22;
            const QRect badgeRect(rightEdge - bw,
                                  rowRect.top() + (topH - bh) / 2, bw, bh);
            painter->setPen(QPen(QColor(203, 213, 225), 1.0));
            painter->setBrush(selected ? QColor(255, 255, 255) : QColor(241, 245, 249));
            painter->drawRoundedRect(badgeRect, 5.0, 5.0);
            painter->setPen(QColor(71, 85, 105));
            painter->drawText(badgeRect, Qt::AlignCenter, shortcut);
            rightEdge = badgeRect.left() - 8;
        }

        if (!category.isEmpty()) {
            QFont catFont(QStringLiteral("Segoe UI"), 10);
            painter->setFont(catFont);
            const QFontMetrics cfm(catFont);
            const int cw = qMin(148, cfm.horizontalAdvance(category) + 6);
            const QRect catRect(rightEdge - cw, rowRect.top(), cw, topH);
            painter->setPen(selected ? QColor(37, 99, 235) : QColor(100, 116, 139));
            painter->drawText(catRect, Qt::AlignRight | Qt::AlignVCenter,
                              cfm.elidedText(category, Qt::ElideRight, cw));
            rightEdge = catRect.left() - 8;
        }

        QFont titleFont(QStringLiteral("Segoe UI"), 11);
        titleFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(titleFont);
        painter->setPen(dimmed ? QColor(71, 85, 105)
                               : (selected ? QColor(29, 78, 216) : QColor(15, 23, 42)));
        const int textLeft = iconRect.right() + 10;
        const int availW = qMax(40, rightEdge - textLeft);
        const QFontMetrics tfm(titleFont);
        const QRect textRect(textLeft, rowRect.top(), availW, topH);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          tfm.elidedText(title, Qt::ElideRight, availW));

        if (hasReason) {
            painter->setOpacity(1.0);
            QFont reasonFont(QStringLiteral("Segoe UI"), 9);
            painter->setFont(reasonFont);
            painter->setPen(selected ? QColor(51, 65, 85) : QColor(71, 85, 105));
            const int reasonW = qMax(60, (rowRect.right() - 10) - textLeft);
            const QFontMetrics rfm(reasonFont);
            const QRect reasonRect(textLeft, rowRect.top() + 24, reasonW,
                                   qMax(16, rowRect.height() - 24));
            painter->drawText(reasonRect, Qt::AlignLeft | Qt::AlignVCenter,
                              rfm.elidedText(reason, Qt::ElideRight, reasonW));
        }

        painter->restore();
    }
};

} // namespace

namespace {

static QString foldKanaAndSearchText(const QString &raw)
{
    if (raw.isEmpty())
        return QString();
    const QString norm = raw.normalized(QString::NormalizationForm_KC).toLower();
    QString out;
    out.reserve(norm.size());
    for (int i = 0; i < norm.size(); ++i) {
        const QChar ch = norm.at(i);
        const ushort uc = ch.unicode();
        if (uc >= 0x30A1 && uc <= 0x30F6)
            out.append(QChar(uc - 0x60));
        else
            out.append(ch);
    }
    return out;
}

static QString romajiToHiragana(const QString &asciiLower)
{
    if (asciiLower.size() < 2)
        return QString();
    for (int i = 0; i < asciiLower.size(); ++i) {
        const ushort u = asciiLower.at(i).unicode();
        if (!((u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u == '-'))
            return QString();
    }

    struct MapEntry {
        const char *rom;
        const ushort hira[2];
    };
    static const MapEntry kRomaji[] = {
        { "kya", { 0x304D, 0x3083 } }, { "kyu", { 0x304D, 0x3085 } },
        { "kyo", { 0x304D, 0x3087 } }, { "sha", { 0x3057, 0x3083 } },
        { "shi", { 0x3057, 0 } },      { "shu", { 0x3057, 0x3085 } },
        { "sho", { 0x3057, 0x3087 } }, { "chi", { 0x3061, 0 } },
        { "cha", { 0x3061, 0x3083 } }, { "chu", { 0x3061, 0x3085 } },
        { "cho", { 0x3061, 0x3087 } }, { "tsu", { 0x3064, 0 } },
        { "nya", { 0x306B, 0x3083 } }, { "nyu", { 0x306B, 0x3085 } },
        { "nyo", { 0x306B, 0x3087 } }, { "hya", { 0x3072, 0x3083 } },
        { "hyu", { 0x3072, 0x3085 } }, { "hyo", { 0x3072, 0x3087 } },
        { "mya", { 0x307F, 0x3083 } }, { "myu", { 0x307F, 0x3085 } },
        { "myo", { 0x307F, 0x3087 } }, { "rya", { 0x308A, 0x3083 } },
        { "ryu", { 0x308A, 0x3085 } }, { "ryo", { 0x308A, 0x3087 } },
        { "gya", { 0x304E, 0x3083 } }, { "gyu", { 0x304E, 0x3085 } },
        { "gyo", { 0x304E, 0x3087 } }, { "bya", { 0x3073, 0x3083 } },
        { "byu", { 0x3073, 0x3085 } }, { "byo", { 0x3073, 0x3087 } },
        { "pya", { 0x3074, 0x3083 } }, { "pyu", { 0x3074, 0x3085 } },
        { "pyo", { 0x3074, 0x3087 } }, { "fya", { 0x3075, 0x3083 } },
        { "fyu", { 0x3075, 0x3085 } }, { "fyo", { 0x3075, 0x3087 } },
        { "ka", { 0x304B, 0 } },       { "ki", { 0x304D, 0 } },
        { "ku", { 0x304F, 0 } },       { "ke", { 0x3051, 0 } },
        { "ko", { 0x3053, 0 } },       { "sa", { 0x3055, 0 } },
        { "si", { 0x3057, 0 } },       { "su", { 0x3059, 0 } },
        { "se", { 0x305B, 0 } },       { "so", { 0x305D, 0 } },
        { "ta", { 0x305F, 0 } },       { "ti", { 0x3061, 0 } },
        { "tu", { 0x3064, 0 } },       { "te", { 0x3066, 0 } },
        { "to", { 0x3068, 0 } },       { "na", { 0x306A, 0 } },
        { "ni", { 0x306B, 0 } },       { "nu", { 0x306C, 0 } },
        { "ne", { 0x306D, 0 } },       { "no", { 0x306E, 0 } },
        { "ha", { 0x306F, 0 } },       { "hi", { 0x3072, 0 } },
        { "hu", { 0x3075, 0 } },       { "fu", { 0x3075, 0 } },
        { "he", { 0x3078, 0 } },       { "ho", { 0x307B, 0 } },
        { "fa", { 0x3075, 0x3041 } },  { "fi", { 0x3075, 0x3043 } },
        { "fe", { 0x3075, 0x3047 } },  { "fo", { 0x3075, 0x3049 } },
        { "ma", { 0x307E, 0 } },       { "mi", { 0x307F, 0 } },
        { "mu", { 0x3080, 0 } },       { "me", { 0x3081, 0 } },
        { "mo", { 0x3082, 0 } },       { "ya", { 0x3084, 0 } },
        { "yu", { 0x3086, 0 } },       { "yo", { 0x3088, 0 } },
        { "ra", { 0x3089, 0 } },       { "ri", { 0x308A, 0 } },
        { "ru", { 0x308B, 0 } },       { "re", { 0x308C, 0 } },
        { "ro", { 0x308D, 0 } },       { "wa", { 0x308F, 0 } },
        { "wo", { 0x3092, 0 } },       { "ga", { 0x304C, 0 } },
        { "gi", { 0x304E, 0 } },       { "gu", { 0x3050, 0 } },
        { "ge", { 0x3052, 0 } },       { "go", { 0x3054, 0 } },
        { "za", { 0x3056, 0 } },       { "zi", { 0x3058, 0 } },
        { "ji", { 0x3058, 0 } },       { "zu", { 0x305A, 0 } },
        { "ze", { 0x305C, 0 } },       { "zo", { 0x305E, 0 } },
        { "ja", { 0x3058, 0x3083 } },  { "ju", { 0x3058, 0x3085 } },
        { "jo", { 0x3058, 0x3087 } },  { "da", { 0x3060, 0 } },
        { "di", { 0x3062, 0 } },       { "du", { 0x3065, 0 } },
        { "de", { 0x3067, 0 } },       { "do", { 0x3069, 0 } },
        { "ba", { 0x3070, 0 } },       { "bi", { 0x3073, 0 } },
        { "bu", { 0x3076, 0 } },       { "be", { 0x3079, 0 } },
        { "bo", { 0x307C, 0 } },       { "pa", { 0x3071, 0 } },
        { "pi", { 0x3074, 0 } },       { "pu", { 0x3077, 0 } },
        { "pe", { 0x307A, 0 } },       { "po", { 0x307D, 0 } },
        { "nn", { 0x3093, 0 } },
        { "a", { 0x3042, 0 } },        { "i", { 0x3044, 0 } },
        { "u", { 0x3046, 0 } },        { "e", { 0x3048, 0 } },
        { "o", { 0x304A, 0 } },
    };

    QString out;
    out.reserve(asciiLower.size());
    int idx = 0;
    while (idx < asciiLower.size()) {
        const QChar c = asciiLower.at(idx);
        if (c == QLatin1Char('-')) {
            out.append(QChar(0x30FC));
            ++idx;
            continue;
        }
        if (idx + 1 < asciiLower.size() && c == asciiLower.at(idx + 1) && c >= QLatin1Char('a')
            && c <= QLatin1Char('z') && c != QLatin1Char('a') && c != QLatin1Char('i')
            && c != QLatin1Char('u') && c != QLatin1Char('e') && c != QLatin1Char('o')
            && c != QLatin1Char('n')) {
            out.append(QChar(0x3063)); // small tsu
            ++idx;
            continue;
        }
        bool matched = false;
        for (const MapEntry &m : kRomaji) {
            const int len = static_cast<int>(qstrlen(m.rom));
            if (idx + len <= asciiLower.size()
                && QStringView(asciiLower).mid(idx, len) == QLatin1String(m.rom)) {
                out.append(QChar(m.hira[0]));
                if (m.hira[1] != 0)
                    out.append(QChar(m.hira[1]));
                idx += len;
                matched = true;
                break;
            }
        }
        if (matched)
            continue;
        if (c == QLatin1Char('n')) {
            if (idx + 1 >= asciiLower.size()) {
                out.append(QChar(0x3093));
                ++idx;
                continue;
            }
            const QChar next = asciiLower.at(idx + 1);
            if (next != QLatin1Char('a') && next != QLatin1Char('i') && next != QLatin1Char('u')
                && next != QLatin1Char('e') && next != QLatin1Char('o')
                && next != QLatin1Char('y')) {
                out.append(QChar(0x3093));
                ++idx;
                continue;
            }
        }
        return QString();
    }
    return out;
}

static QString spacedObjectNameKeywords(const QString &objName)
{
    if (objName.isEmpty())
        return QString();
    QString raw = objName;
    if (raw.startsWith(QLatin1String("action")) && raw.size() > 6
        && raw.at(6).isUpper()) {
        raw = raw.mid(6);
    }
    QString spaced;
    spaced.reserve(raw.size() + 8);
    for (int i = 0; i < raw.size(); ++i) {
        const QChar c = raw.at(i);
        if (c == QLatin1Char('_') || c == QLatin1Char('-')) {
            spaced.append(QLatin1Char(' '));
            continue;
        }
        if (i > 0 && c.isUpper() && !raw.at(i - 1).isUpper())
            spaced.append(QLatin1Char(' '));
        spaced.append(c);
    }
    return spaced.toLower();
}

static QString actionParentMenuHint(const QAction *act)
{
    if (!act)
        return QString();
    for (QWidget *w : act->associatedWidgets()) {
        if (QMenu *m = qobject_cast<QMenu *>(w)) {
            const QString mt = cleanMnemonicText(m->title()).trimmed();
            if (!mt.isEmpty())
                return mt;
        }
    }
    return QString();
}

static QString computeActionDisabledReason(const QAction *act, ApplicationWindow *app,
                                           ModernRibbonBar *ribbon)
{
    if (!app)
        return CommandPalettePopup::tr("Currently unavailable in this view context");
    if (app->d_workspace.subWindowList().isEmpty())
        return CommandPalettePopup::tr("Requires an open project window");

    QWidget *aw = app->d_workspace.activeSubWindow();
    Table *activeTable = qobject_cast<Table *>(aw);
    Table *wsTarget = ribbon ? ribbon->resolvedWorksheetTarget() : activeTable;
    MultiLayer *activeMl = qobject_cast<MultiLayer *>(aw);
    if (!activeMl && ribbon && ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph)
        activeMl = qobject_cast<MultiLayer *>(ribbon->splitPlotTarget());
    Matrix *activeMatrix = qobject_cast<Matrix *>(aw);
    Graph3D *active3d = qobject_cast<Graph3D *>(aw);

    int activeCurves = 0;
    bool hasSelectedGraphObj = false;
    if (activeMl) {
        if (Graph *g = activeMl->activeGraph() ? activeMl->activeGraph() : activeMl->layer(1)) {
            activeCurves = g->plotItemsList().size();
            hasSelectedGraphObj =
                    (g->currentSelection().kind != Graph::GraphSelection::None);
        }
    }

    const QString obj = act ? act->objectName().toLower() : QString();
    const QString clean = act ? cleanMnemonicText(act->text()).toLower() : QString();
    const QString tip = act ? cleanMnemonicText(act->toolTip()).toLower() : QString();
    const QString comb = obj + QLatin1Char(' ') + clean + QLatin1Char(' ') + tip;

    if ((comb.contains(QLatin1String("matrix"))
         && !comb.contains(QLatin1String("to matrix"))
         && !comb.contains(QLatin1String("convert"))
         && !comb.contains(QLatin1String("correlation"))
         && !comb.contains(QString::fromUtf8("\xE8\xA1\x8C\xE5\x88\x97\xE3\x81\xB8\xE5\xA4\x89\xE6\x8F\x9B")))
        || comb.contains(QLatin1String("spatial filter"))
        || comb.contains(QLatin1String("2d spatial"))) {
        if (!activeMatrix)
            return CommandPalettePopup::tr("Requires a matrix window");
    }

    if (comb.contains(QLatin1String("3d"))
        && (comb.contains(QLatin1String("rotation")) || comb.contains(QLatin1String("surface3d"))
            || comb.contains(QLatin1String("perspective"))
            || comb.contains(QLatin1String("floor")))) {
        if (!active3d)
            return CommandPalettePopup::tr("Requires a 3D graph window");
    }

    const bool isPlotAction =
            (obj.startsWith(QLatin1String("actionplot"))
             || comb.contains(QLatin1String("error bar"))
             || comb.contains(QString::fromUtf8("\xE3\x82\xA8\xE3\x83\xA9\xE3\x83\xBC\xE3\x83\x90\xE3\x83\xBC")));
    const bool isTableAction =
            comb.contains(QLatin1String("stacked bar"))
            || comb.contains(QLatin1String("polar"))
            || comb.contains(QLatin1String("double-y"))
            || comb.contains(QLatin1String("y-offset"))
            || comb.contains(QLatin1String("violin"))
            || comb.contains(QLatin1String("pareto"))
            || comb.contains(QLatin1String("q-q"))
            || comb.contains(QLatin1String("ternary"))
            || comb.contains(QLatin1String("plot table"))
            || comb.contains(QLatin1String("column"))
            || comb.contains(QLatin1String("worksheet"))
            || comb.contains(QLatin1String("sort"))
            || comb.contains(QLatin1String("formula"))
            || comb.contains(QLatin1String("fill"))
            || comb.contains(QLatin1String("norm"))
            || comb.contains(QLatin1String("re-import"))
            || comb.contains(QLatin1String("reimport"))
            || comb.contains(QLatin1String("to matrix"))
            || comb.contains(QString::fromUtf8("\xE5\x88\x97"))
            || comb.contains(QString::fromUtf8("\xE3\x83\xAF\xE3\x83\xBC\xE3\x82\xAF\xE3\x82\xB7\xE3\x83\xBC\xE3\x83\x88"));

    if (isPlotAction || isTableAction) {
        if (!wsTarget && !activeTable) {
            return CommandPalettePopup::tr(
                    "Requires an open worksheet (click or create a worksheet)");
        }
        return CommandPalettePopup::tr("Requires a numeric or selected worksheet column");
    }

    const bool isCurveAction =
            comb.contains(QLatin1String("fit")) || comb.contains(QLatin1String("smooth"))
            || comb.contains(QLatin1String("diff")) || comb.contains(QLatin1String("integ"))
            || comb.contains(QLatin1String("fft")) || comb.contains(QLatin1String("peak"))
            || comb.contains(QLatin1String("interp")) || comb.contains(QLatin1String("filter"))
            || comb.contains(QLatin1String("wavelet")) || comb.contains(QLatin1String("psd"))
            || comb.contains(QLatin1String("baseline")) || comb.contains(QLatin1String("translate"))
            || comb.contains(QLatin1String("roc")) || comb.contains(QLatin1String("kaplan"))
            || comb.contains(QString::fromUtf8("\xE3\x83\x95\xE3\x82\xA3\xE3\x83\x83\xE3\x83\x88"))
            || comb.contains(QString::fromUtf8("\xE5\xB9\xB3\xE6\xBB\x91"))
            || comb.contains(QString::fromUtf8("\xE7\xA9\x8D\xE5\x88\x86"))
            || comb.contains(QString::fromUtf8("\xE5\xBE\xAE\xE5\x88\x86"))
            || comb.contains(QString::fromUtf8("\xE3\x83\x94\xE3\x83\xBC\xE3\x82\xAF"));

    if (isCurveAction) {
        if (!activeMl) {
            return CommandPalettePopup::tr(
                    "Requires a graph window (click a graph or plot data first)");
        }
        if (activeCurves == 0)
            return CommandPalettePopup::tr("Requires a plotted curve on the active graph");
    }

    if (comb.contains(QLatin1String("graph")) || comb.contains(QLatin1String("layer"))
        || comb.contains(QLatin1String("axis")) || comb.contains(QLatin1String("axes"))
        || comb.contains(QLatin1String("legend")) || comb.contains(QLatin1String("grid"))
        || comb.contains(QLatin1String("page")) || comb.contains(QLatin1String("ole"))
        || comb.contains(QLatin1String("powerpoint")) || comb.contains(QLatin1String("python"))
        || comb.contains(QLatin1String("style")) || comb.contains(QLatin1String("aspect"))
        || comb.contains(QLatin1String("frame"))
        || comb.contains(QString::fromUtf8("\xE3\x82\xB0\xE3\x83\xA9\xE3\x83\x95"))
        || comb.contains(QString::fromUtf8("\xE8\xBB\xB8"))
        || comb.contains(QString::fromUtf8("\xE5\x87\xA1\xE4\xBE\x8B"))
        || comb.contains(QString::fromUtf8("\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC"))) {
        if (!activeMl) {
            return CommandPalettePopup::tr(
                    "Requires a graph window (click a graph or plot data first)");
        }
        if (!hasSelectedGraphObj)
            return CommandPalettePopup::tr("Requires a selected graph object or layer");
    }

    if (activeMl && !hasSelectedGraphObj)
        return CommandPalettePopup::tr("Requires a selected graph object or layer");
    if (activeTable)
        return CommandPalettePopup::tr("Requires a numeric or selected worksheet column");

    return CommandPalettePopup::tr("Currently unavailable in this view context");
}

} // namespace

// ============================================================================
// CommandPalettePopup
// ============================================================================

CommandPalettePopup::CommandPalettePopup(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                         QWidget *parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
      d_app(app),
      d_ribbon(ribbon)
{
    setObjectName(QStringLiteral("ToriCommandPalettePopup"));
    setAttribute(Qt::WA_TranslucentBackground, false);
    setModal(false);
    setFixedSize(580, 410);

    setStyleSheet(QStringLiteral(
            "QDialog#ToriCommandPalettePopup {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "}"
            "QLineEdit#CommandPaletteSearchEdit {"
            "  background-color: #F8FAFC;"
            "  color: #0F172A;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 8px;"
            "  padding: 6px 10px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 12pt;"
            "}"
            "QLineEdit#CommandPaletteSearchEdit:focus {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #2563EB;"
            "}"
            "QListWidget#CommandPaletteList {"
            "  background-color: #FFFFFF;"
            "  border: none;"
            "  outline: none;"
            "}"
            "QLabel#CommandPaletteHintLabel {"
            "  color: #64748B;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 10pt;"
            "  padding: 2px 4px;"
            "}"));

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 10);
    rootLayout->setSpacing(8);

    QHBoxLayout *searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(8);

    QLabel *searchIconLbl = new QLabel(this);
    searchIconLbl->setPixmap(
            IconLoader::duotonePixmap(QStringLiteral("search-command"), 20, 0.0, false));
    searchIconLbl->setFixedSize(22, 22);
    searchIconLbl->setAlignment(Qt::AlignCenter);
    searchRow->addWidget(searchIconLbl);

    d_search_edit = new QLineEdit(this);
    d_search_edit->setObjectName(QStringLiteral("CommandPaletteSearchEdit"));
    d_search_edit->setClearButtonEnabled(true);
    d_search_edit->installEventFilter(this);
    searchRow->addWidget(d_search_edit, 1);

    rootLayout->addLayout(searchRow);

    d_list_widget = new QListWidget(this);
    d_list_widget->setObjectName(QStringLiteral("CommandPaletteList"));
    d_list_widget->setItemDelegate(new CommandPaletteItemDelegate(d_list_widget));
    d_list_widget->setUniformItemSizes(false);
    d_list_widget->setMouseTracking(true);
    d_list_widget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d_list_widget->installEventFilter(this);
    rootLayout->addWidget(d_list_widget, 1);

    d_hint_label = new QLabel(this);
    d_hint_label->setObjectName(QStringLiteral("CommandPaletteHintLabel"));
    rootLayout->addWidget(d_hint_label);

    connect(d_search_edit, &QLineEdit::textChanged, this,
            &CommandPalettePopup::rebuildFilteredList);
    connect(d_list_widget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item)
            executeItemAt(d_list_widget->row(item));
    });
    connect(d_list_widget, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item)
            executeItemAt(d_list_widget->row(item));
    });

    retranslateUi();
}

CommandPalettePopup::~CommandPalettePopup() = default;

void CommandPalettePopup::retranslateUi()
{
    setWindowTitle(tr("Command Palette"));
    if (d_search_edit)
        d_search_edit->setPlaceholderText(tr("Type a command, plot, fit, or preset..."));
    if (d_hint_label)
        d_hint_label->setText(tr("Up/Down to navigate  ·  Enter to run  ·  Esc to close"));
    refreshCommands();
}

void CommandPalettePopup::refreshCommands()
{
    d_entries.clear();

    auto addQuickEntry = [this](const QString &id, const QString &title, const QString &category,
                                const QString &shortcut, const QString &keywords,
                                const QString &iconId, std::function<void()> fn, int boost = 80) {
        CommandEntry e;
        e.id = id;
        e.title = title;
        e.category = category;
        e.shortcutText = shortcut;
        e.searchKey = foldKanaAndSearchText(title + QLatin1Char(' ') + category
                                            + QLatin1Char(' ') + shortcut + QLatin1Char(' ')
                                            + keywords);
        e.iconId = iconId;
        e.callback = std::move(fn);
        e.priorityBoost = boost;
        d_entries.append(std::move(e));
    };

    const QString catQuick = tr("Quick Preset");
    const QString catWorkspace = tr("Workspace");
    const QString catTools = tr("Tools");

    addQuickEntry(
            QStringLiteral("quick.isij85"), tr("ISIJ 85mm Journal Preset"), catQuick, QString(),
            QStringLiteral("isij 85mm journal style preset 600dpi"),
            QStringLiteral("preset-isij"),
            [this]() {
                if (!d_app)
                    return;
                if (MultiLayer *ml = ModernRibbonHudInternal::resolveTargetMultiLayer(d_app.data(), d_ribbon.data())) {
                    if (d_app->d_workspace.activeSubWindow() != ml)
                        d_app->d_workspace.setActiveSubWindow(ml);
                    d_app->applyStyleISIJJournal();
                } else {
                    d_app->showUxHint(
                            ModernRibbonBar::tr("Click or create a graph window first."));
                }
            },
            100);

    addQuickEntry(
            QStringLiteral("quick.lab_slide"), tr("Lab Slide (18pt) Preset"), catQuick,
            QString(),
            QString::fromUtf8("lab slide 18pt 83.3mm 12.9pt 140% powerpoint style preset "
                              "\xE7\xA0\x94\xE7\xA9\xB6\xE5\xAE\xA4\xE3\x82\xB9\xE3\x83\xA9\xE3\x82\xA4\xE3\x83\x89"),
            QStringLiteral("preset-slide"),
            [this]() {
                if (!d_app)
                    return;
                if (MultiLayer *ml = ModernRibbonHudInternal::resolveTargetMultiLayer(d_app.data(), d_ribbon.data())) {
                    ml->applyStylePresetById(QStringLiteral("lab_slide_83mm"));
                    if (ml->pageViewMode())
                        ml->fitPageZoomToViewport();
                    d_app->modifiedProject();
                    if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                        insp->refreshAll();
                } else {
                    d_app->showUxHint(
                            ModernRibbonBar::tr("Click or create a graph window first."));
                }
            },
            99);

    addQuickEntry(
            QStringLiteral("quick.apply_preset_multi"),
            tr("Apply Preset to Multiple Graphs..."), catQuick, QString(),
            QString::fromUtf8("apply style preset multiple batch graphs isij lab slide "
                              "\xE8\xA4\x87\xE6\x95\xB0\xE3\x82\xB0\xE3\x83\xA9\xE3\x83\x95 "
                              "\xE4\xB8\x80\xE6\x8B\xAC\xE9\x81\xA9\xE7\x94\xA8"),
            QStringLiteral("preset-isij"),
            [this]() {
                if (!d_app)
                    return;
                if (!QMetaObject::invokeMethod(
                            d_app.data(), "showApplyStylePresetToMultipleGraphsDialog",
                            Qt::DirectConnection)) {
                    d_app->showStylePresetManagerDialog();
                }
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.apply_template_table"),
            tr("Apply Template (.qpt) to Table..."), catQuick, QString(),
            QString::fromUtf8("apply template qpt table worksheet "
                              "\xE3\x83\x86\xE3\x83\xB3\xE3\x83\x97\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\x88"),
            QStringLiteral("page-setup"),
            [this]() {
                if (!d_app)
                    return;
                if (!QMetaObject::invokeMethod(d_app.data(), "applyTemplateToActiveTable",
                                               Qt::DirectConnection)) {
                    d_app->openTemplate();
                }
            },
            86);

    addQuickEntry(
            QStringLiteral("quick.sample_gallery"),
            tr("Sample Project Gallery..."), catQuick, QString(),
            QString::fromUtf8("sample gallery demo template example dataset project "
                              "\xE3\x82\xB5\xE3\x83\xB3\xE3\x83\x97\xE3\x83\xAB "
                              "\xE3\x82\xAE\xE3\x83\xA3\xE3\x83\xA9\xE3\x83\xAA\xE3\x83\xBC "
                              "\xE4\xBE\x8B\xE9\xA1\x8C"),
            QStringLiteral("sample-gallery"),
            [this]() {
                if (d_app)
                    CurveMathTools::showSampleGalleryDialog(d_app);
            },
            99);

    addQuickEntry(
            QStringLiteral("quick.tensile_yield"),
            tr("Tensile / 0.2% Yield Analysis..."), catQuick, QString(),
            QString::fromUtf8("tensile stress strain 0.2% offset yield proof youngs modulus uts "
                              "\xE5\xBC\x95\xE5\xBC\xB5 \xE8\x80\x90\xE5\x8A\x9B "
                              "\xE5\xBF\x9C\xE5\x8A\x9B \xE3\x81\xB2\xE3\x81\x9A\xE3\x81\xBF "
                              "\xE3\x83\xA4\xE3\x83\xB3\xE3\x82\xB0\xE7\x8E\x87"),
            QStringLiteral("curvemath"),
            [this]() {
                if (d_app)
                    CurveMathTools::showTensileYieldDialog(d_app);
            },
            97);

    addQuickEntry(
            QStringLiteral("quick.kissinger_tds"),
            tr("Kissinger / Thermal Desorption (TDS)..."), catQuick, QString(),
            QString::fromUtf8("kissinger tds thermal desorption dsc activation energy heating rate "
                              "\xE6\x98\x87\xE6\xB8\xA9\xE8\x84\xB1\xE9\x9B\xA2 "
                              "\xE6\xB4\xBB\xE6\x80\xA7\xE5\x8C\x96\xE3\x82\xA8\xE3\x83\x8D\xE3\x83\xAB\xE3\x82\xAE\xE3\x83\xBC"),
            QStringLiteral("kissinger-tds"),
            [this]() {
                if (d_app)
                    CurveMathTools::showKissingerDialog(d_app);
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.work_hardening"),
            tr("Work Hardening / Considere Analysis..."), catQuick, QString(),
            QString::fromUtf8("work hardening considere Hollomon Crussard Jaoul Kocks Mecking true stress strain "
                              "\xE5\x8A\xA0\xE5\xB7\xA5\xE7\xA1\xAC\xE5\x8C\x96 "
                              "\xE7\x9C\x9F\xE5\xBF\x9C\xE5\x8A\x9B \xE7\x9C\x9F\xE3\x81\xB2\xE3\x81\x9A\xE3\x81\xBF"),
            QStringLiteral("work-hardening"),
            [this]() {
                if (d_app)
                    CurveMathTools::showWorkHardeningDialog(d_app);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.xrd_analyzer"),
            tr("XRD Peak & Williamson-Hall Analyzer..."), catQuick, QString(),
            QString::fromUtf8("xrd diffraction scherrer williamson hall crystallite microstrain ka2 "
                              "X\xE7\xB7\x9A\xE5\x9B\x9E\xE6\x8A\x98 \xE7\xB5\x90\xE6\x99\xB6\xE5\xAD\x90"),
            QStringLiteral("xrd-peaks"),
            [this]() {
                if (d_app)
                    CurveMathTools::showXRDAnalyzerDialog(d_app);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.vasp_plotter"),
            tr("VASP Electronic Band & DOS Plotter..."), catQuick, QString(),
            QString::fromUtf8("vasp band structure doscar eigenval fermi electronic density of states "
                              "\xE3\x83\x90\xE3\x83\xB3\xE3\x83\x89\xE6\xA7\x8B\xE9\x80\xA0 "
                              "\xE7\x8A\xB6\xE6\x85\x8B\xE5\xAF\x86\xE5\xBA\xA6 \xE9\x9B\xBB\xE5\xAD\x90\xE7\x8A\xB6\xE6\x85\x8B"),
            QStringLiteral("vasp-bands"),
            [this]() {
                if (d_app)
                    CurveMathTools::showVaspPlotterDialog(d_app);
            },
            93);

    addQuickEntry(
            QStringLiteral("quick.export_python"),
            tr("Export Active Graph to Python Script..."), catTools, QString(),
            QString::fromUtf8("export python script code reproduce matplotlib torigraph generate py "
                              "Python\xE5\x87\xBA\xE5\x8A\x9B \xE3\x82\xB9\xE3\x82\xAF\xE3\x83\xAA\xE3\x83\x97\xE3\x83\x88\xE7\x94\x9F\xE6\x88\x90"),
            QStringLiteral("code-export"),
            [this]() {
                if (d_app)
                    d_app->exportActiveGraphToPython();
            },
            91);

    addQuickEntry(
            QStringLiteral("quick.merge_graphs"), tr("Merge Graph Windows..."), catWorkspace,
            QString(),
            QStringLiteral("merge graph windows multi panel figure a b c d combine layout"),
            QStringLiteral("merge-graphs"),
            [this]() {
                if (d_app)
                    d_app->mergeOpenGraphsDialog();
            },
            95);

    addQuickEntry(
            QStringLiteral("quick.split_view"), tr("Split View (Table + Graph)"), catWorkspace,
            QString(), QStringLiteral("split view side by side table graph workspace"),
            QStringLiteral("split-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
            },
            94);

    addQuickEntry(
            QStringLiteral("quick.tabbed_view"), tr("Tabbed Workspace View"), catWorkspace,
            QString(), QStringLiteral("tabbed tabs workspace view"), QStringLiteral("tab-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
            },
            90);

    addQuickEntry(
            QStringLiteral("quick.free_mdi"), tr("Free MDI Workspace View"), catWorkspace,
            QString(), QStringLiteral("free mdi cascade subwindow workspace view"),
            QStringLiteral("mdi-view"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
            },
            86);

    addQuickEntry(
            QStringLiteral("quick.focus_hud"), tr("Toggle Focus HUD Mode (F11)"), catWorkspace,
            QStringLiteral("F11"), QStringLiteral("focus hud canvas fullscreen f11"),
            QStringLiteral("focus-hud"),
            [this]() {
                if (d_ribbon)
                    d_ribbon->toggleFocusHudMode();
            },
            92);

    addQuickEntry(
            QStringLiteral("quick.graph_inspector"),
            tr("Toggle Graph Inspector Dock (Ctrl+Shift+I)"), catWorkspace,
            QStringLiteral("Ctrl+Shift+I"),
            QStringLiteral("graph inspector dock object manager property sidebar rail ctrl+shift+i"),
            QStringLiteral("table-drawer"),
            [this]() {
                if (d_app)
                    d_app->toggleGraphInspectorDock();
            },
            93);

    addQuickEntry(
            QStringLiteral("quick.python_console"), tr("Open Python Console"), catTools,
            QStringLiteral("F9"), QStringLiteral("python console repl script automation"),
            QStringLiteral("python-console"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->openPythonConsoleRequested();
            },
            88);

    addQuickEntry(
            QStringLiteral("quick.python_run"), tr("Run Python Script..."), catTools, QString(),
            QStringLiteral("run python script py file automation"), QStringLiteral("python-run"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->runPythonScriptRequested();
            },
            84);

    addQuickEntry(
            QStringLiteral("quick.check_updates"), tr("Check for Updates..."), catTools,
            QString(), QStringLiteral("check updates github release version upgrade"),
            QStringLiteral("update-check"),
            [this]() {
                if (d_ribbon)
                    emit d_ribbon->checkForUpdatesRequested();
            },
            82);

    addQuickEntry(
            QStringLiteral("quick.switch_language"), tr("Switch Language (Japanese / English)"),
            catTools, QString(), QStringLiteral("language japanese english i18n locale ja en"),
            QStringLiteral("language-ja-en"),
            [this]() {
                if (!d_app)
                    return;
                if (!QMetaObject::invokeMethod(d_app.data(), "toggleLanguage",
                                               Qt::DirectConnection)) {
                    if (d_app->appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive))
                        d_app->switchToEnglish();
                    else
                        d_app->switchToJapanese();
                }
            },
            85);

    // B4-10: Index canvas context actions so Ctrl+K can invoke them directly.
    if (d_app) {
        const QString catCanvas = tr("Graph Canvas");
        MultiLayer *activeMl = ModernRibbonHudInternal::resolveTargetMultiLayer(d_app.data(), d_ribbon.data());
        auto addCanvasEntry = [&](const QString &id, const QString &title, const QString &keywords,
                                  const QString &iconId, std::function<void(MultiLayer *, Graph *)> fn,
                                  int boost = 72) {
            CommandEntry e;
            e.id = id;
            e.title = title;
            e.category = catCanvas;
            e.searchKey = foldKanaAndSearchText(title + QLatin1Char(' ') + catCanvas
                                                + QLatin1Char(' ') + keywords);
            e.iconId = iconId;
            e.priorityBoost = activeMl ? boost : -35;
            if (!activeMl) {
                e.disabled = true;
                e.disabledReason = tr("Requires a graph window (click a graph or plot data first)");
            } else {
                QPointer<ApplicationWindow> appPtr(d_app);
                QPointer<ModernRibbonBar> ribbonPtr(d_ribbon);
                e.callback = [appPtr, ribbonPtr, fn = std::move(fn)]() {
                    if (!appPtr)
                        return;
                    if (MultiLayer *ml =
                                ModernRibbonHudInternal::resolveTargetMultiLayer(appPtr.data(), ribbonPtr.data())) {
                        Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->layer(1);
                        if (g)
                            fn(ml, g);
                    }
                };
            }
            d_entries.append(std::move(e));
        };

        addCanvasEntry(
                QStringLiteral("canvas.fit_zoom"), tr("Fit Page Zoom to Window"),
                QString::fromUtf8("zoom fit window page scale 100% "
                                  "\xE7\x94\xBB\xE9\x9D\xA2\xE3\x81\xAB\xE5\x90\x88\xE3\x82\x8F\xE3\x81\x9B\xE3\x82\x8B "
                                  "\xE6\x8B\xA1\xE5\xA4\xA7\xE7\x8E\x87"),
                QStringLiteral("zoom-auto"),
                [](MultiLayer *ml, Graph *) {
                    if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport",
                                                   Qt::DirectConnection)) {
                        ml->setPageZoom(1.0);
                    }
                },
                90);
        addCanvasEntry(
                QStringLiteral("canvas.rescale_all"), tr("Rescale All Axes"),
                QString::fromUtf8("autoscale rescale reset axes range "
                                  "\xE8\xBB\xB8 \xE8\x87\xAA\xE5\x8B\x95\xE3\x82\xB9\xE3\x82\xB1\xE3\x83\xBC\xE3\x83\xAB"),
                QStringLiteral("zoom-auto"),
                [this](MultiLayer *ml, Graph *g) {
                    const QString before = ml->captureUndoSnapshot();
                    g->setAutoScale();
                    ml->pushUndoSnapshot(tr("Rescale All Axes"), before);
                    if (d_app)
                        d_app->modifiedProject();
                },
                82);
        addCanvasEntry(
                QStringLiteral("canvas.autoscale_axis"), tr("Autoscale Active Axis"),
                QString::fromUtf8("autoscale active axis range "
                                  "\xE9\x81\xB8\xE6\x8A\x9E\xE8\xBB\xB8 \xE8\x87\xAA\xE5\x8B\x95"),
                QStringLiteral("zoom-auto"),
                [this](MultiLayer *ml, Graph *g) {
                    const auto sel = g->currentSelection();
                    const int ax = (sel.kind == Graph::GraphSelection::Axis
                                    || sel.kind == Graph::GraphSelection::AxisTitle)
                                    && sel.primaryId >= 0 && sel.primaryId < QwtPlot::axisCnt
                            ? sel.primaryId
                            : QwtPlot::xBottom;
                    const QString before = ml->captureUndoSnapshot();
                    g->setAxisUserLocked(ax, false);
                    g->enableAutoscaling(true);
                    g->setAutoScale();
                    ml->pushUndoSnapshot(tr("Autoscale Active Axis"), before);
                    if (d_app)
                        d_app->modifiedProject();
                },
                76);
        addCanvasEntry(
                QStringLiteral("canvas.ref_zero_y"), tr("y = 0 Reference Line"),
                QString::fromUtf8("reference line zero y=0 baseline "
                                  "\xE5\x9F\xBA\xE6\xBA\x96\xE7\xB7\x9A"),
                QStringLiteral("mean-line"),
                [this](MultiLayer *, Graph *g) {
                    GraphInspectorDock::toggleZeroReferenceLine(g, 0);
                    if (d_app) {
                        if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                            insp->refreshAll();
                    }
                },
                78);
        addCanvasEntry(
                QStringLiteral("canvas.ref_zero_x"), tr("x = 0 Reference Line"),
                QString::fromUtf8("reference line zero x=0 vertical "
                                  "\xE5\x9F\xBA\xE6\xBA\x96\xE7\xB7\x9A"),
                QStringLiteral("mean-line"),
                [this](MultiLayer *, Graph *g) {
                    GraphInspectorDock::toggleZeroReferenceLine(g, 1);
                    if (d_app) {
                        if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                            insp->refreshAll();
                    }
                },
                75);
        addCanvasEntry(
                QStringLiteral("canvas.ref_custom"), tr("Add Reference Line..."),
                QString::fromUtf8("reference line custom constant mean threshold "
                                  "\xE5\x9F\xBA\xE6\xBA\x96\xE7\xB7\x9A\xE3\x82\x92\xE8\xBF\xBD\xE5\x8A\xA0"),
                QStringLiteral("curve-add"),
                [](MultiLayer *, Graph *g) {
                    GraphInspectorDock::addReferenceLineInteractive(g, 0);
                },
                80);
        addCanvasEntry(
                QStringLiteral("canvas.auto_legend"), tr("Auto-Place Legend (Avoid Curves)"),
                QString::fromUtf8("legend auto place avoid curves position "
                                  "\xE5\x87\xA1\xE4\xBE\x8B \xE8\x87\xAA\xE5\x8B\x95\xE9\x85\x8D\xE7\xBD\xAE"),
                QStringLiteral("legend"),
                [this](MultiLayer *ml, Graph *g) {
                    const QString before = ml->captureUndoSnapshot();
                    g->autoPlaceLegend();
                    ml->pushUndoSnapshot(tr("Auto-Place Legend (Avoid Curves)"), before);
                    if (d_app)
                        d_app->modifiedProject();
                },
                76);
        addCanvasEntry(
                QStringLiteral("canvas.edit_title"), tr("Edit Plot Title..."),
                QString::fromUtf8("title edit plot graph heading "
                                  "\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB\xE7\xB7\xA8\xE9\x9B\x86"),
                QStringLiteral("text-label"),
                [this](MultiLayer *ml, Graph *) {
                    if (d_app && ml) {
                        if (d_app->d_workspace.activeSubWindow() != ml)
                            d_app->d_workspace.setActiveSubWindow(ml);
                        d_app->showTitleDialog();
                    }
                },
                74);
    }

    if (d_app) {
        QSet<QString> seenTitles;
        QSet<QString> coveredObjectNames = {
            QStringLiteral("actionTensileYieldDialog"),
            QStringLiteral("actionMergeGraphs"),
            QStringLiteral("actionWorkspaceLayoutSplit"),
            QStringLiteral("actionWorkspaceLayoutTabbed"),
            QStringLiteral("actionWorkspaceLayoutMdi"),
            QStringLiteral("actionToggleFocusHudMode"),
            QStringLiteral("actionToggleGraphInspector"),
            QStringLiteral("actionOpenPythonConsole"),
            QStringLiteral("actionRunPythonScript"),
            QStringLiteral("actionCheckUpdates"),
            QStringLiteral("actionCommandPalette")
        };
        const char *const kCoveredAppTexts[] = {
            "&ISIJ Journal (85 mm, 600 DPI)",
            "&Lab Slide (18 pt @ 140%, 83.3 mm)",
            "Apply Style Preset to &Multiple Graphs...",
            "Plot Table Using &Template...",
            "Sample Project &Gallery...",
            "&Kissinger / Thermal Desorption (TDS)...",
            "&Work Hardening && Considere Analysis...",
            "&XRD Peak && Williamson-Hall Analyzer...",
            "&VASP Electronic Band && DOS Plotter...",
            "Export Reproducible &Python Script...",
            "&Tabbed Workspace View"
        };
        QSet<const QAction *> seenActions;
        for (const CommandEntry &ce : d_entries)
            seenTitles.insert(ce.title.toLower());
        for (const char *raw : kCoveredAppTexts) {
            const QString rawClean = cleanMnemonicText(QString::fromUtf8(raw)).trimmed().toLower();
            if (!rawClean.isEmpty())
                seenTitles.insert(rawClean);
            const QString trClean = cleanMnemonicText(ApplicationWindow::tr(raw)).trimmed().toLower();
            if (!trClean.isEmpty())
                seenTitles.insert(trClean);
        }

        const QList<QAction *> actions = d_app->findChildren<QAction *>();
        const QString catCommand = tr("Command");
        for (QAction *act : actions) {
            if (!act || act->isSeparator() || act->menu() != nullptr)
                continue;
            if (seenActions.contains(act)
                || (!act->objectName().isEmpty()
                    && coveredObjectNames.contains(act->objectName()))) {
                continue;
            }
            // Hidden actions (e.g. other-field features switched off) stay out of the list;
            // disabled ones are listed dimmed with per-action reasons (C12).
            if (!act->isVisible())
                continue;

            const QString cleanTitle = cleanMnemonicText(act->text()).trimmed();
            if (cleanTitle.isEmpty())
                continue;
            if (cleanTitle.size() == 1 && !cleanTitle.at(0).isLetter())
                continue;

            QString displayTitle = cleanTitle;
            const QString lowerTitle = cleanTitle.toLower();
            const QString menuHint = actionParentMenuHint(act);
            if (seenTitles.contains(lowerTitle)) {
                if (menuHint.isEmpty() || seenTitles.contains((cleanTitle + QStringLiteral(" (") + menuHint + QLatin1Char(')')).toLower()))
                    continue;
                displayTitle = cleanTitle + QStringLiteral(" (") + menuHint + QLatin1Char(')');
            }
            seenTitles.insert(displayTitle.toLower());
            seenActions.insert(act);

            CommandEntry e;
            e.id = act->objectName();
            e.title = displayTitle;
            e.category = !menuHint.isEmpty() ? menuHint : catCommand;
            e.shortcutText = act->shortcut().toString(QKeySequence::NativeText);
            const QString tip = cleanMnemonicText(act->toolTip());
            e.searchKey = foldKanaAndSearchText(
                    displayTitle + QLatin1Char(' ') + tip + QLatin1Char(' ') + menuHint
                    + QLatin1Char(' ') + spacedObjectNameKeywords(act->objectName())
                    + QLatin1Char(' ') + act->objectName() + QLatin1Char(' ') + e.shortcutText);
            // Use the action's own duotone id (the one shown in menus and the ribbon); the old
            // keyword guess from the object name gave many entries an unrelated icon. Without
            // an id the palette falls back to the action's icon.
            e.iconId = act->property("_tg_duotone_id").toString();
            if (e.iconId.isEmpty())
                e.iconId = ModernUiTheme::semanticIconForAction(act);
            e.action = act;
            e.priorityBoost = 0;
            if (!act->isEnabled()) {
                e.disabled = true;
                e.disabledReason = computeActionDisabledReason(act, d_app.data(), d_ribbon.data());
                e.priorityBoost = -40;
            }
            d_entries.append(std::move(e));
        }
    }

    rebuildFilteredList(d_search_edit ? d_search_edit->text() : QString());
}

void CommandPalettePopup::setFilterText(const QString &query)
{
    if (!d_search_edit)
        return;
    if (d_search_edit->text() == query)
        rebuildFilteredList(query);
    else
        d_search_edit->setText(query);
}

QString CommandPalettePopup::filterText() const
{
    return d_search_edit ? d_search_edit->text() : QString();
}

int CommandPalettePopup::visibleCommandCount() const
{
    return d_list_widget ? d_list_widget->count() : 0;
}

int CommandPalettePopup::fuzzyMatchScore(const QString &needleLower, const CommandEntry &entry)
{
    if (needleLower.isEmpty())
        return 100 + entry.priorityBoost;

    const QString titleFolded = foldKanaAndSearchText(entry.title);
    QStringList variants;
    variants << needleLower;
    const QString romHira = romajiToHiragana(needleLower);
    if (!romHira.isEmpty() && romHira != needleLower)
        variants << romHira;

    int bestScore = -1;
    for (int v = 0; v < variants.size(); ++v) {
        const QString &q = variants.at(v);
        const int romPenalty = (v == 0) ? 0 : -40;
        if (titleFolded == q) {
            bestScore = qMax(bestScore, 1200 + romPenalty + entry.priorityBoost);
            continue;
        }
        if (titleFolded.startsWith(q)) {
            bestScore = qMax(bestScore, 900 + romPenalty + entry.priorityBoost);
            continue;
        }
        const int subIdx = titleFolded.indexOf(q);
        if (subIdx >= 0) {
            bestScore = qMax(bestScore, 700 - qMin(120, subIdx * 3) + romPenalty + entry.priorityBoost);
            continue;
        }

        const QStringList tokens = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() > 1) {
            bool allTokensFound = true;
            int tokenBonus = 0;
            for (const QString &tok : tokens) {
                if (!entry.searchKey.contains(tok)) {
                    allTokensFound = false;
                    break;
                }
                if (titleFolded.contains(tok))
                    tokenBonus += 40;
            }
            if (allTokensFound) {
                bestScore = qMax(bestScore, 520 + tokenBonus + romPenalty + entry.priorityBoost);
                continue;
            }
        }

        if (entry.searchKey.contains(q)) {
            bestScore = qMax(bestScore, 420 + romPenalty + entry.priorityBoost);
            continue;
        }

        auto subseqScore = [&](const QString &haystack, int baseScore) -> int {
            int pos = 0;
            int streak = 0;
            int score = baseScore;
            for (int i = 0; i < q.size(); ++i) {
                const QChar ch = q.at(i);
                if (ch.isSpace())
                    continue;
                const int found = haystack.indexOf(ch, pos);
                if (found < 0)
                    return -1;
                if (found == pos) {
                    streak += 1;
                    score += 18 + streak * 6;
                } else {
                    streak = 0;
                    score -= qMin(25, found - pos);
                }
                pos = found + 1;
            }
            return qMax(1, score + romPenalty + (entry.priorityBoost / 2));
        };

        int ss = subseqScore(titleFolded, 200);
        if (ss < 0)
            ss = subseqScore(entry.searchKey, 120);
        if (ss > 0)
            bestScore = qMax(bestScore, ss);
    }

    return bestScore;
}

void CommandPalettePopup::rebuildFilteredList(const QString &query)
{
    if (!d_list_widget)
        return;

    const QString needle = foldKanaAndSearchText(query.trimmed());
    struct ScoredIdx
    {
        int index;
        int score;
    };
    QVector<ScoredIdx> matches;
    matches.reserve(d_entries.size());

    for (int i = 0; i < d_entries.size(); ++i) {
        const int sc = fuzzyMatchScore(needle, d_entries.at(i));
        if (sc > 0)
            matches.append({ i, sc });
    }

    std::stable_sort(matches.begin(), matches.end(),
                     [](const ScoredIdx &a, const ScoredIdx &b) { return a.score > b.score; });

    d_list_widget->clear();
    d_visible_entry_indices.clear();

    const int maxItems = qMin(matches.size(), 80);
    d_visible_entry_indices.reserve(maxItems);
    for (int i = 0; i < maxItems; ++i) {
        const int entryIdx = matches.at(i).index;
        const CommandEntry &ce = d_entries.at(entryIdx);
        d_visible_entry_indices.append(entryIdx);

        QIcon itemIcon = (!ce.iconId.isEmpty())
                ? IconLoader::duotoneIcon(ce.iconId, 20, false)
                : (ce.action ? ce.action->icon() : QIcon());
        if (itemIcon.isNull())
            itemIcon = IconLoader::duotoneIcon(QStringLiteral("search-command"), 20, false);

        QListWidgetItem *item = new QListWidgetItem(itemIcon, ce.title, d_list_widget);
        item->setData(RoleEntryIndex, entryIdx);
        item->setData(RoleShortcutText, ce.shortcutText);
        item->setData(RoleCategoryText, ce.category);
        if (ce.disabled) {
            item->setData(RoleDisabledEntry, true);
            item->setData(RoleDisabledReason, ce.disabledReason);
            item->setToolTip(ce.disabledReason);
        }
    }

    if (d_list_widget->count() > 0)
        d_list_widget->setCurrentRow(0);
}

bool CommandPalettePopup::executeCurrentSelection()
{
    if (!d_list_widget || d_list_widget->count() <= 0)
        return false;
    const int row = qMax(0, d_list_widget->currentRow());
    return executeItemAt(row);
}

bool CommandPalettePopup::executeItemAt(int row)
{
    if (row < 0 || row >= d_visible_entry_indices.size())
        return false;

    const int entryIdx = d_visible_entry_indices.at(row);
    if (entryIdx < 0 || entryIdx >= d_entries.size())
        return false;

    const QPointer<QAction> act = d_entries.at(entryIdx).action;
    const std::function<void()> cb = d_entries.at(entryIdx).callback;

    if (d_entries.at(entryIdx).disabled && !(act && act->isEnabled())) {
        if (d_list_widget) {
            const QRect r = d_list_widget->visualItemRect(d_list_widget->item(row));
            QToolTip::showText(d_list_widget->viewport()->mapToGlobal(r.bottomLeft()),
                               d_entries.at(entryIdx).disabledReason, d_list_widget);
        }
        return false;
    }

    hide();

    if (cb) {
        cb();
        return true;
    }
    if (act && act->isEnabled()) {
        act->trigger();
        return true;
    }
    return false;
}

bool CommandPalettePopup::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (watched == d_search_edit || watched == d_list_widget) {
            if (ke->key() == Qt::Key_Down) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int cnt = d_list_widget->count();
                    const int cur = d_list_widget->currentRow();
                    const int next = (cur < 0) ? 0 : ((cur + 1) % cnt);
                    d_list_widget->setCurrentRow(next);
                }
                return true;
            }
            if (ke->key() == Qt::Key_Up) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int cnt = d_list_widget->count();
                    const int cur = d_list_widget->currentRow();
                    const int prev = (cur < 0) ? (cnt - 1) : ((cur - 1 + cnt) % cnt);
                    d_list_widget->setCurrentRow(prev);
                }
                return true;
            }
            if (ke->key() == Qt::Key_PageDown) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int next = qMin(d_list_widget->count() - 1,
                                          qMax(0, d_list_widget->currentRow()) + 8);
                    d_list_widget->setCurrentRow(next);
                }
                return true;
            }
            if (ke->key() == Qt::Key_PageUp) {
                if (d_list_widget && d_list_widget->count() > 0) {
                    const int prev = qMax(0, d_list_widget->currentRow() - 8);
                    d_list_widget->setCurrentRow(prev);
                }
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                executeCurrentSelection();
                return true;
            }
            if (ke->key() == Qt::Key_Escape
                || (ke->key() == Qt::Key_K && (ke->modifiers() & Qt::ControlModifier))) {
                hide();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPalettePopup::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void CommandPalettePopup::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (d_search_edit) {
        d_search_edit->setFocus(Qt::PopupFocusReason);
        d_search_edit->selectAll();
    }
}
