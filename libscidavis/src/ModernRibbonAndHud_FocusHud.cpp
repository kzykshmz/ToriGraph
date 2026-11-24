/***************************************************************************
    File                 : ModernRibbonAndHud_FocusHud.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : F11 Non-Redundant 2-Stage Focus Canvas HUD and Linked Worksheet Drawer
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

// ============================================================================
// FocusTableDrawer
// ============================================================================

namespace {

static QString stripDrawerInlineRichTags(const QString &raw)
{
    if (!raw.contains(QLatin1Char('<')))
        return raw;
    static const QRegularExpression kInlineTags(
            QStringLiteral("</?(?:i|b|sub|sup|u|em|strong)>"),
            QRegularExpression::CaseInsensitiveOption);
    QString out = raw;
    out.remove(kInlineTags);
    return out.trimmed();
}

static QString formatDrawerColumnHeader(Table *tbl, int c, QString *outFormula = nullptr)
{
    if (!tbl || c < 0 || c >= tbl->numCols())
        return QString();

    QString roleTag;
    switch (tbl->colPlotDesignation(c)) {
    case SciDAVis::X:
        roleTag = QStringLiteral("[X]");
        break;
    case SciDAVis::Y:
        roleTag = QStringLiteral("[Y]");
        break;
    case SciDAVis::Z:
        roleTag = QStringLiteral("[Z]");
        break;
    case SciDAVis::xErr:
        roleTag = QStringLiteral("[dX]");
        break;
    case SciDAVis::yErr:
        roleTag = QStringLiteral("[dY]");
        break;
    default:
        break;
    }

    Column *colObj = tbl->column(c);
    QString colFormula;
    if (colObj && !colObj->formulaIntervals().isEmpty()) {
        const Interval<int> iv = colObj->formulaIntervals().first();
        colFormula = colObj->formula(iv.start()).trimmed();
    }
    if (outFormula)
        *outFormula = colFormula;

    const QString longName = stripDrawerInlineRichTags(tbl->colLongName(c).trimmed());
    QString units = stripDrawerInlineRichTags(tbl->colUnits(c).trimmed());
    if (units.size() >= 2
        && ((units.startsWith(QLatin1Char('(')) && units.endsWith(QLatin1Char(')')))
            || (units.startsWith(QLatin1Char('[')) && units.endsWith(QLatin1Char(']'))))) {
        units = units.mid(1, units.size() - 2).trimmed();
    }

    const QString titleLine = longName.isEmpty() ? tbl->colLabel(c) : longName;
    QStringList metaParts;
    if (!roleTag.isEmpty())
        metaParts << roleTag;
    if (!colFormula.isEmpty())
        metaParts << FocusTableDrawer::tr("[fx]");
    if (!units.isEmpty())
        metaParts << QStringLiteral("(%1)").arg(units);
    return metaParts.isEmpty() ? titleLine
                               : (titleLine + QLatin1Char('\n') + metaParts.join(QLatin1Char(' ')));
}

static void adjustDrawerColumnWidths(QTableView *view, Table *tbl)
{
    if (!view || !tbl)
        return;
    QHeaderView *hh = view->horizontalHeader();
    if (!hh)
        return;
    hh->ensurePolished();
    QFont secFont = hh->font();
    if (secFont.pointSizeF() < 11.0 && secFont.pointSize() < 11)
        secFont.setPointSize(11);
    if (secFont.weight() < QFont::DemiBold)
        secFont.setWeight(QFont::DemiBold);
    const QFontMetrics fmBase(hh->font());
    const QFontMetrics fmSec(secFont);
    const int cols = tbl->numCols();
    for (int c = 0; c < cols; ++c) {
        const QString h = formatDrawerColumnHeader(tbl, c);
        int headerTextWidth = 0;
        const QStringList lines = h.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            headerTextWidth = qMax(
                    headerTextWidth,
                    qMax(fmBase.horizontalAdvance(line), fmSec.horizontalAdvance(line)));
        }
        hh->resizeSection(c, qBound(108, headerTextWidth + 28, 220));
    }
}

class FocusDrawerProxyModel : public QIdentityProxyModel
{
public:
    explicit FocusDrawerProxyModel(FocusTableDrawer *drawer)
        : QIdentityProxyModel(drawer), d_drawer(drawer)
    {
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid() || !d_drawer)
            return Qt::NoItemFlags;
        Table *tbl = d_drawer->currentTable();
        if (!tbl)
            return Qt::NoItemFlags;
        Column *colObj = tbl->column(index.column());
        const bool isFormula =
                (colObj && index.row() < tbl->numRows()
                 && !colObj->formula(index.row()).trimmed().isEmpty());
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (!isFormula)
            f |= Qt::ItemIsEditable;
        return f;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || !d_drawer)
            return QVariant();
        Table *tbl = d_drawer->currentTable();
        if (!tbl || index.row() < 0 || index.row() >= tbl->numRows() || index.column() < 0
            || index.column() >= tbl->numCols()) {
            return QVariant();
        }

        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return tbl->text(index.row(), index.column());

        if (role == Qt::BackgroundRole || role == Qt::ForegroundRole || role == Qt::ToolTipRole) {
            Column *colObj = tbl->column(index.column());
            const QString cellFormula =
                    colObj ? colObj->formula(index.row()).trimmed() : QString();
            if (!cellFormula.isEmpty()) {
                if (role == Qt::BackgroundRole)
                    return QColor(241, 245, 249);
                if (role == Qt::ForegroundRole)
                    return QColor(30, 64, 175);
                if (role == Qt::ToolTipRole)
                    return FocusTableDrawer::tr("Formula (read-only): %1").arg(cellFormula);
            }
            return QVariant();
        }

        return QIdentityProxyModel::data(index, role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (!d_drawer)
            return QVariant();
        Table *tbl = d_drawer->currentTable();
        if (!tbl)
            return QVariant();

        if (orientation == Qt::Horizontal) {
            if (section < 0 || section >= tbl->numCols())
                return QVariant();
            if (role == Qt::DecorationRole)
                return QVariant();
            if (role == Qt::DisplayRole)
                return formatDrawerColumnHeader(tbl, section);
            if (role == Qt::ToolTipRole) {
                QString colFormula;
                formatDrawerColumnHeader(tbl, section, &colFormula);
                if (!colFormula.isEmpty())
                    return FocusTableDrawer::tr("Formula (read-only): %1").arg(colFormula);
                return QVariant();
            }
            return QIdentityProxyModel::headerData(section, orientation, role);
        }

        if (orientation == Qt::Vertical && role == Qt::DisplayRole)
            return QString::number(section + 1);

        return QIdentityProxyModel::headerData(section, orientation, role);
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if (role != Qt::EditRole || !index.isValid() || !d_drawer)
            return false;
        if (!d_drawer->setCellValueFromDrawer(index.row(), index.column(), value.toString()))
            return false;
        emit dataChanged(index, index);
        return true;
    }

    void notifyAllHeadersChanged()
    {
        const int cols = columnCount();
        if (cols > 0)
            emit headerDataChanged(Qt::Horizontal, 0, cols - 1);
    }

private:
    FocusTableDrawer *d_drawer = nullptr;
};

} // namespace

FocusTableDrawer::FocusTableDrawer(ApplicationWindow *app, QWidget *parent)
    : QFrame(parent), d_app(app)
{
    setObjectName(QStringLiteral("FocusTableDrawerCard"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(255, 255, 255));
        pal.setColor(QPalette::Base, QColor(255, 255, 255));
        setPalette(pal);
    }
    setFixedWidth(396);
    setStyleSheet(QStringLiteral(
            "QFrame#FocusTableDrawerCard {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 12px;"
            "}"
            "QLabel#FocusDrawerTitle {"
            "  color: #0F172A;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 700;"
            "}"
            "QLabel#FocusDrawerEmpty {"
            "  color: #64748B;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  padding: 12px;"
            "}"
            "QToolButton.FocusDrawerBtn {"
            "  background-color: #F8FAFC;"
            "  color: #1E293B;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 6px;"
            "  padding: 3px 6px;"
            "  font-size: 11pt;"
            "}"
            "QToolButton.FocusDrawerBtn:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QTableView#FocusDrawerGrid, QTableWidget#FocusDrawerCompatGrid {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 8px;"
            "  gridline-color: #E2E8F0;"
            "  selection-background-color: #DBEAFE;"
            "  selection-color: #0F172A;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "}"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    QHBoxLayout *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);

    QLabel *iconLbl = new QLabel(this);
    iconLbl->setPixmap(IconLoader::duotonePixmap(QStringLiteral("table-drawer"), 18, 0.0, false));
    iconLbl->setFixedSize(20, 20);
    headerRow->addWidget(iconLbl);

    d_title_label = new QLabel(this);
    d_title_label->setObjectName(QStringLiteral("FocusDrawerTitle"));
    headerRow->addWidget(d_title_label, 1);

    d_btn_new_table = new QToolButton(this);
    d_btn_new_table->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_new_table->setIcon(IconLoader::duotoneIcon(QStringLiteral("new-table"), 16, false));
    d_btn_new_table->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_new_table);

    d_btn_add_col = new QToolButton(this);
    d_btn_add_col->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_add_col->setIcon(IconLoader::duotoneIcon(QStringLiteral("add-column"), 16, false));
    d_btn_add_col->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_add_col);

    d_btn_close = new QToolButton(this);
    d_btn_close->setProperty("class", QStringLiteral("FocusDrawerBtn"));
    d_btn_close->setIcon(IconLoader::duotoneIcon(QStringLiteral("clear"), 16, false));
    d_btn_close->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(d_btn_close);

    mainLayout->addLayout(headerRow);

    d_table_combo = new QComboBox(this);
    d_table_combo->setMinimumHeight(28);
    mainLayout->addWidget(d_table_combo);

    d_empty_label = new QLabel(this);
    d_empty_label->setObjectName(QStringLiteral("FocusDrawerEmpty"));
    d_empty_label->setAlignment(Qt::AlignCenter);
    d_empty_label->setWordWrap(true);
    mainLayout->addWidget(d_empty_label);

    d_proxy_model = new FocusDrawerProxyModel(this);
    d_table_view = new QTableView(this);
    d_table_view->setObjectName(QStringLiteral("FocusDrawerGrid"));
    d_table_view->setModel(d_proxy_model);
    d_table_view->setAlternatingRowColors(true);
    d_table_view->horizontalHeader()->setDefaultSectionSize(108);
    d_table_view->horizontalHeader()->setMinimumSectionSize(54);
    d_table_view->verticalHeader()->setDefaultSectionSize(24);
    d_table_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    mainLayout->addWidget(d_table_view, 1);

    connect(d_proxy_model, &QAbstractItemModel::columnsInserted, this,
            [this](const QModelIndex &, int, int) {
                if (!d_shutting_down && !isHidden() && d_current_table)
                    adjustDrawerColumnWidths(d_table_view, d_current_table.data());
            });

    connect(d_btn_new_table, &QToolButton::clicked, this, [this]() {
        if (d_shutting_down || !d_app)
            return;
        Table *t = d_app->newTable();
        refreshTableList(false);
        if (t)
            setCurrentTable(t);
    });

    connect(d_btn_add_col, &QToolButton::clicked, this, [this]() {
        if (d_shutting_down || !d_current_table)
            return;
        d_current_table->addCol(SciDAVis::Y);
        syncGridFromTable();
    });

    connect(d_btn_close, &QToolButton::clicked, this, &FocusTableDrawer::closeDrawerRequested);

    connect(d_table_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                if (d_shutting_down || d_syncing || !d_app || idx < 0)
                    return;
                const QString tblName = d_table_combo->itemData(idx).toString();
                Table *target = d_app->table(tblName);
                if (target && !d_dying_tables.contains(target)) {
                    if (WorkbookManager *wb = WorkbookManager::existing(d_app)) {
                        const QString book = wb->bookOf(target);
                        if (!book.isEmpty() && wb->currentSheet(book) != target)
                            wb->setCurrentSheet(target, false);
                    }
                    setCurrentTable(target);
                }
            });

    if (d_app) {
        connect(&d_app->folders, &QObject::destroyed, this,
                &FocusTableDrawer::prepareForAppShutdown);
    }

    installEventFilter(this);
    if (d_table_view)
        d_table_view->installEventFilter(this);
    if (d_table_combo)
        d_table_combo->installEventFilter(this);

    retranslateUi();
}

FocusTableDrawer::~FocusTableDrawer()
{
    prepareForAppShutdown();
}

void FocusTableDrawer::prepareForAppShutdown()
{
    d_shutting_down = true;
    d_syncing = true;

    removeEventFilter(this);
    if (d_table_view)
        d_table_view->removeEventFilter(this);
    if (d_table_combo)
        d_table_combo->removeEventFilter(this);

    if (d_proxy_model)
        d_proxy_model->setSourceModel(nullptr);

    if (d_current_table) {
        disconnect(d_current_table.data(), nullptr, this, nullptr);
        d_current_table.clear();
    }
    d_dying_tables.clear();

    if (d_compat_grid)
        disconnect(d_compat_grid, nullptr, this, nullptr);
    if (d_table_combo)
        disconnect(d_table_combo, nullptr, this, nullptr);
    if (d_btn_new_table)
        disconnect(d_btn_new_table, nullptr, this, nullptr);
    if (d_btn_add_col)
        disconnect(d_btn_add_col, nullptr, this, nullptr);
    if (d_btn_close)
        disconnect(d_btn_close, nullptr, this, nullptr);

    d_app.clear();
}

Table *FocusTableDrawer::currentTable() const
{
    return d_current_table.data();
}

QTableWidget *FocusTableDrawer::tableWidget() const
{
    syncCompatGrid();
    return d_compat_grid;
}

void FocusTableDrawer::retranslateUi()
{
    if (d_shutting_down)
        return;
    if (d_title_label)
        d_title_label->setText(tr("Linked Worksheet"));
    if (d_btn_new_table)
        d_btn_new_table->setToolTip(tr("New Table"));
    if (d_btn_add_col)
        d_btn_add_col->setToolTip(tr("Add Column"));
    if (d_btn_close)
        d_btn_close->setToolTip(tr("Close Worksheet Drawer"));
    if (d_empty_label)
        d_empty_label->setText(tr("No worksheet available in the current project."));
    d_compat_grid_dirty = true;
    syncGridFromTable();
}

Table *FocusTableDrawer::detectPreferredTable(const QSet<QObject *> &ignoredObjects) const
{
    if (d_shutting_down || !d_app)
        return nullptr;

    QSet<QObject *> effectiveIgnored = ignoredObjects;
    for (MyWidget *mw : d_app->hiddenWindows) {
        if (mw && !ModernRibbonHudInternal::isWorkbookManagedSheet(d_app, mw))
            effectiveIgnored.insert(mw);
    }

    auto isUsableTable = [this, &effectiveIgnored](Table *tbl) -> bool {
        if (!tbl || effectiveIgnored.contains(tbl))
            return false;
        if (!ModernRibbonHudInternal::isUsableRibbonWorksheet(d_app, tbl))
            return false;
        if (Folder *curFolder = d_app->currentFolder()) {
            if (!curFolder->windowsList().contains(tbl))
                return false;
        }
        return true;
    };

    QMdiSubWindow *activeSub = d_app->d_workspace.activeSubWindow();
    if (activeSub && !effectiveIgnored.contains(activeSub)) {
        if (Table *activeTbl = qobject_cast<Table *>(activeSub)) {
            if (isUsableTable(activeTbl))
                return activeTbl;
        }
        if (MultiLayer *activeMl = qobject_cast<MultiLayer *>(activeSub)) {
            if (Table *srcTbl = ModernRibbonHudInternal::sourceTableFromMultiLayer(activeMl, effectiveIgnored)) {
                if (isUsableTable(srcTbl))
                    return srcTbl;
            }
        }
    }

    const QList<QMdiSubWindow *> ordered =
            d_app->d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
    for (int i = ordered.size() - 1; i >= 0; --i) {
        QMdiSubWindow *sw = ordered.at(i);
        if (!sw || effectiveIgnored.contains(sw))
            continue;
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(sw)) {
            if (Folder *curFolder = d_app->currentFolder()) {
                if (!curFolder->windowsList().contains(ml))
                    continue;
            }
            if (Table *srcTbl = ModernRibbonHudInternal::sourceTableFromMultiLayer(ml, effectiveIgnored)) {
                if (isUsableTable(srcTbl))
                    return srcTbl;
            }
        }
    }
    for (int i = ordered.size() - 1; i >= 0; --i) {
        QMdiSubWindow *sw = ordered.at(i);
        if (!sw || effectiveIgnored.contains(sw))
            continue;
        if (Table *tbl = qobject_cast<Table *>(sw)) {
            if (isUsableTable(tbl))
                return tbl;
        }
    }
    return nullptr;
}

void FocusTableDrawer::refreshTableList(bool autoSelectActiveCurveTable)
{
    if (d_shutting_down)
        return;
    refreshTableListInternal(autoSelectActiveCurveTable, d_dying_tables);
}

void FocusTableDrawer::refreshTableListInternal(bool autoSelectActiveCurveTable,
                                                const QSet<QObject *> &ignoredObjects)
{
    if (d_shutting_down || !d_table_combo)
        return;

    QVector<Table *> liveTables;
    if (d_app) {
        auto appendIfUserTable = [this, &ignoredObjects, &liveTables](MyWidget *mw) {
            if (!mw || ignoredObjects.contains(mw))
                return;
            if (!ModernRibbonHudInternal::isUsableRibbonWorksheet(d_app, mw))
                return;
            if (Table *tbl = qobject_cast<Table *>(mw)) {
                if (!liveTables.contains(tbl))
                    liveTables.append(tbl);
            }
        };

        if (Folder *curFolder = d_app->currentFolder()) {
            const QList<MyWidget *> folderWins = curFolder->windowsList();
            for (MyWidget *mw : folderWins)
                appendIfUserTable(mw);
        }
        const QList<QMdiSubWindow *> subs = d_app->d_workspace.subWindowList();
        for (QMdiSubWindow *sw : subs)
            appendIfUserTable(qobject_cast<MyWidget *>(sw));
        for (MyWidget *mw : d_app->outWindows)
            appendIfUserTable(mw);
    }

    Table *desired = nullptr;
    if (autoSelectActiveCurveTable)
        desired = detectPreferredTable(ignoredObjects);
    if (!desired && d_current_table && !ignoredObjects.contains(d_current_table.data())
        && liveTables.contains(d_current_table.data())) {
        desired = d_current_table.data();
    }
    if (!desired && !liveTables.isEmpty())
        desired = liveTables.first();

    {
        QSignalBlocker blocker(d_table_combo);
        d_table_combo->clear();
        int selectIdx = -1;
        for (int i = 0; i < liveTables.size(); ++i) {
            Table *t = liveTables.at(i);
            const QString lbl = t->windowLabel().trimmed();
            const QString disp = lbl.isEmpty()
                    ? t->name()
                    : QStringLiteral("%1 — %2").arg(t->name(), lbl);
            d_table_combo->addItem(
                    IconLoader::duotoneIcon(QStringLiteral("new-table"), 16, false), disp,
                    t->name());
            if (t == desired)
                selectIdx = i;
        }
        if (selectIdx >= 0)
            d_table_combo->setCurrentIndex(selectIdx);
    }

    setCurrentTable(desired);
}

void FocusTableDrawer::bindTableSignals(Table *table)
{
    if (d_shutting_down) {
        if (d_current_table) {
            disconnect(d_current_table.data(), nullptr, this, nullptr);
            d_current_table.clear();
        }
        return;
    }

    if (d_current_table == table)
        return;

    if (d_current_table)
        disconnect(d_current_table.data(), nullptr, this, nullptr);

    d_current_table = table;

    if (d_current_table) {
        connect(d_current_table.data(), &QObject::destroyed, this,
                &FocusTableDrawer::onCurrentTableDestroyed, Qt::UniqueConnection);
        connect(d_current_table.data(), &Table::modifiedData, this,
                [this](Table *, const QString &) {
                    if (!d_shutting_down && !d_syncing)
                        syncGridFromTable();
                });
        connect(d_current_table.data(), &Table::changedColHeader, this,
                [this](const QString &, const QString &) {
                    if (!d_shutting_down && !d_syncing)
                        syncGridFromTable();
                });
        connect(d_current_table.data(), &Table::removedCol, this, [this](const QString &) {
            if (!d_shutting_down && !d_syncing)
                syncGridFromTable();
        });
    }
}

void FocusTableDrawer::setCurrentTable(Table *table)
{
    if (d_shutting_down)
        return;

    if (table && d_dying_tables.contains(table))
        table = nullptr;

    bindTableSignals(table);

    if (d_table_combo && d_current_table) {
        const int idx = d_table_combo->findData(d_current_table->name());
        if (idx >= 0 && d_table_combo->currentIndex() != idx) {
            QSignalBlocker blocker(d_table_combo);
            d_table_combo->setCurrentIndex(idx);
        }
    }

    if (d_btn_add_col)
        d_btn_add_col->setEnabled(!d_current_table.isNull());

    syncGridFromTable();
}

void FocusTableDrawer::onTableClosed(Table *table)
{
    if (d_shutting_down || !d_app || !table)
        return;
    const bool wasCurrent = (d_current_table == table);
    d_dying_tables.insert(table);
    if (wasCurrent)
        bindTableSignals(nullptr);
    refreshTableListInternal(wasCurrent, d_dying_tables);
    d_dying_tables.remove(table);
}

void FocusTableDrawer::onCurrentTableDestroyed(QObject *destroyedObj)
{
    if (d_shutting_down || !d_app) {
        d_current_table.clear();
        return;
    }
    if (destroyedObj)
        d_dying_tables.insert(destroyedObj);
    d_current_table.clear();
    refreshTableListInternal(true, d_dying_tables);
    if (destroyedObj)
        d_dying_tables.remove(destroyedObj);
}

void FocusTableDrawer::syncGridFromTable()
{
    if (d_shutting_down || !d_table_view || !d_proxy_model || !d_empty_label)
        return;

    d_compat_grid_dirty = true;

    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl)) {
        d_proxy_model->setSourceModel(nullptr);
        d_table_view->hide();
        d_empty_label->show();
        if (d_compat_grid) {
            QSignalBlocker blocker(d_compat_grid);
            d_compat_grid->clear();
            d_compat_grid->setRowCount(0);
            d_compat_grid->setColumnCount(0);
            d_compat_grid_dirty = false;
        }
        return;
    }

    d_empty_label->hide();
    d_table_view->show();

    if (isHidden()) {
        // Defer binding while drawer is closed so zero signals/layout work run when hidden
        d_dirty = true;
        if (d_proxy_model->sourceModel() != nullptr)
            d_proxy_model->setSourceModel(nullptr);
        return;
    }

    d_dirty = false;
    TableModel *srcModel = tbl->tableModel();
    if (d_proxy_model->sourceModel() != srcModel)
        d_proxy_model->setSourceModel(srcModel);
    if (auto *fp = static_cast<FocusDrawerProxyModel *>(d_proxy_model))
        fp->notifyAllHeadersChanged();
    adjustDrawerColumnWidths(d_table_view, tbl);
    if (d_table_view->viewport())
        d_table_view->viewport()->update();

    if (d_compat_grid && tbl->numRows() <= 200)
        syncCompatGrid();
}

void FocusTableDrawer::syncCompatGrid() const
{
    if (d_shutting_down)
        return;

    FocusTableDrawer *self = const_cast<FocusTableDrawer *>(this);
    if (!d_compat_grid) {
        d_compat_grid = new QTableWidget(self);
        d_compat_grid->setObjectName(QStringLiteral("FocusDrawerCompatGrid"));
        d_compat_grid->hide();
        connect(d_compat_grid, &QTableWidget::cellChanged, self,
                &FocusTableDrawer::onGridCellChanged);
        d_compat_grid_dirty = true;
    }

    if (!d_compat_grid_dirty)
        return;

    d_compat_grid_dirty = false;
    QSignalBlocker blocker(d_compat_grid);

    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl)) {
        d_compat_grid->clear();
        d_compat_grid->setRowCount(0);
        d_compat_grid->setColumnCount(0);
        return;
    }

    const int rows = qBound(0, tbl->numRows(), 200);
    const int cols = qMax(0, tbl->numCols());
    d_compat_grid->setRowCount(rows);
    d_compat_grid->setColumnCount(cols);

    for (int c = 0; c < cols; ++c) {
        QString colFormula;
        const QString h = formatDrawerColumnHeader(tbl, c, &colFormula);
        QTableWidgetItem *hdrItem = new QTableWidgetItem(h);
        if (!colFormula.isEmpty())
            hdrItem->setToolTip(tr("Formula (read-only): %1").arg(colFormula));
        d_compat_grid->setHorizontalHeaderItem(c, hdrItem);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Column *colObj = tbl->column(c);
            const QString cellFormula = colObj ? colObj->formula(r).trimmed() : QString();
            const bool isFormulaCell = !cellFormula.isEmpty();
            const QString val = tbl->text(r, c);
            QTableWidgetItem *it = d_compat_grid->item(r, c);
            if (!it) {
                it = new QTableWidgetItem(val);
                d_compat_grid->setItem(r, c, it);
            } else if (it->text() != val) {
                it->setText(val);
            }

            if (isFormulaCell) {
                it->setFlags((it->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable)
                             & ~Qt::ItemIsEditable);
                it->setBackground(QColor(241, 245, 249));
                it->setForeground(QColor(30, 64, 175));
                it->setToolTip(tr("Formula (read-only): %1").arg(cellFormula));
            } else {
                it->setFlags(it->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable
                             | Qt::ItemIsEditable);
                it->setData(Qt::BackgroundRole, QVariant());
                it->setData(Qt::ForegroundRole, QVariant());
                it->setToolTip(QString());
            }
        }
    }
}

void FocusTableDrawer::onGridCellChanged(int row, int col)
{
    if (d_shutting_down || d_syncing || !d_current_table || !d_compat_grid)
        return;
    QTableWidgetItem *it = d_compat_grid->item(row, col);
    if (!it || !(it->flags() & Qt::ItemIsEditable))
        return;
    setCellValueFromDrawer(row, col, it->text());
}

bool FocusTableDrawer::setCellValueFromDrawer(int row, int col, const QString &text)
{
    if (d_shutting_down)
        return false;
    Table *tbl = d_current_table.data();
    if (!tbl || d_dying_tables.contains(tbl))
        return false;
    if (row < 0 || col < 0 || col >= tbl->numCols())
        return false;
    if (Column *colObj = tbl->column(col)) {
        if (row < tbl->numRows() && !colObj->formula(row).trimmed().isEmpty())
            return false;
        if (row >= tbl->numRows() && !colObj->formulaIntervals().isEmpty()) {
            const Interval<int> lastIv = colObj->formulaIntervals().last();
            if (tbl->numRows() > 0 && lastIv.end() >= tbl->numRows() - 1
                && !colObj->formula(lastIv.start()).trimmed().isEmpty()) {
                return false;
            }
        }
    }
    if (row >= tbl->numRows())
        tbl->setNumRows(row + 1);

    d_syncing = true;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        tbl->clearCell(row, col);
    } else if (tbl->columnType(col) == SciDAVis::ColumnMode::Numeric) {
        bool ok = false;
        double numericVal = QLocale().toDouble(trimmed, &ok);
        if (!ok)
            numericVal = trimmed.toDouble(&ok);
        if (ok) {
            tbl->setCell(row, col, numericVal);
        } else {
            Column *colObj = tbl->column(col);
            const QString lower = trimmed.toLower();
            const bool isNullSentinel =
                    (trimmed == QLatin1String("-") || trimmed == QLatin1String("--")
                     || lower == QLatin1String("nan") || lower == QLatin1String("na")
                     || lower == QLatin1String("n/a") || lower == QLatin1String("null"));
            bool hasAnyFinite = false;
            if (colObj && !isNullSentinel && colObj->formulaIntervals().isEmpty()) {
                const int nRows = colObj->rowCount();
                for (int r = 0; r < nRows; ++r) {
                    if (!colObj->isInvalid(r) && std::isfinite(colObj->valueAt(r))) {
                        hasAnyFinite = true;
                        break;
                    }
                }
                if (!hasAnyFinite) {
                    colObj->setColumnMode(SciDAVis::ColumnMode::Text);
                    colObj->setTextAt(row, trimmed);
                } else {
                    tbl->setText(row, col, trimmed);
                }
            } else {
                tbl->setText(row, col, trimmed);
            }
        }
    } else {
        tbl->setText(row, col, text);
    }
    d_syncing = false;

    d_compat_grid_dirty = true;
    syncGridFromTable();
    return true;
}

void FocusTableDrawer::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void FocusTableDrawer::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    raise();
    if (!d_shutting_down && (d_dirty || !d_proxy_model || !d_proxy_model->sourceModel())) {
        syncGridFromTable();
    }
}

void FocusTableDrawer::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    if (d_proxy_model && d_proxy_model->sourceModel() != nullptr) {
        d_proxy_model->setSourceModel(nullptr);
        d_dirty = true;
    }
}

bool FocusTableDrawer::eventFilter(QObject *watched, QEvent *event)
{
    if (!d_shutting_down && event && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            emit closeDrawerRequested();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

// ============================================================================
// FocusCanvasHudOverlay
// ============================================================================

static void syncFocusHudOleButton(QToolButton *btnOle, ApplicationWindow *app,
                                  ModernRibbonBar *ribbon = nullptr)
{
    if (!btnOle)
        return;
    bool hasGraph = false;
    QString tip;
    if (app) {
        if (QAction *aCopyOle = app->findChild<QAction *>(QStringLiteral("actionCopyGraphOle"))) {
            hasGraph = aCopyOle->isEnabled();
            tip = aCopyOle->toolTip();
        } else {
            QMdiSubWindow *sw = app->d_workspace.activeSubWindow();
            hasGraph = (qobject_cast<MultiLayer *>(sw) != nullptr
                        || qobject_cast<Graph3D *>(sw) != nullptr);
        }
        if (!hasGraph && ribbon
            && ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
            MyWidget *splitPlot = ribbon->splitPlotTarget();
            hasGraph = (qobject_cast<MultiLayer *>(splitPlot) != nullptr
                        || qobject_cast<Graph3D *>(splitPlot) != nullptr);
        }
    }
    if (!hasGraph) {
        const QString disabledHint = FocusCanvasHudOverlay::tr(
                "Create or select a graph window first to copy for PowerPoint.");
        if (tip.isEmpty() || !tip.contains(disabledHint)) {
            tip = ToriGraphOleUi::copyForPowerPointToolTip() + QStringLiteral("\n")
                    + disabledHint;
        }
    } else if (tip.isEmpty()) {
        tip = ToriGraphOleUi::copyForPowerPointToolTip();
    }
    btnOle->setEnabled(hasGraph);
    btnOle->setToolTip(tip);
}

FocusCanvasHudOverlay::FocusCanvasHudOverlay(ApplicationWindow *app, ModernRibbonBar *ribbon,
                                             QWidget *viewportParent)
    : QFrame(viewportParent), d_app(app), d_ribbon(ribbon), d_viewport(viewportParent)
{
    setObjectName(QStringLiteral("FocusCanvasDarkPill"));
    setStyleSheet(QStringLiteral(
            "QFrame#FocusCanvasDarkPill {"
            "  background-color: rgba(15, 23, 42, 235);"
            "  border: 1px solid rgba(148, 163, 184, 95);"
            "  border-radius: 14px;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton {"
            "  background-color: transparent;"
            "  color: #F8FAFC;"
            "  border: 1px solid transparent;"
            "  border-radius: 9px;"
            "  padding: 4px 8px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton:hover:enabled {"
            "  background-color: rgba(59, 130, 246, 75);"
            "  border: 1px solid rgba(96, 165, 250, 140);"
            "  color: #FFFFFF;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton:disabled {"
            "  color: #CBD5E1;"
            "}"
            "QFrame#FocusCanvasDarkPill QToolButton::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "}"));

    QHBoxLayout *pillLayout = new QHBoxLayout(this);
    pillLayout->setContentsMargins(8, 4, 8, 4);
    pillLayout->setSpacing(3);

    auto makeDarkBtn = [this, pillLayout](const QString &iconId,
                                          std::function<void()> onClick) -> QToolButton * {
        QToolButton *btn = new QToolButton(this);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setIcon(IconLoader::duotoneIcon(iconId, 18, true));
        btn->setIconSize(QSize(18, 18));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::TabFocus);
        if (onClick)
            connect(btn, &QToolButton::clicked, this, std::move(onClick));
        pillLayout->addWidget(btn);
        return btn;
    };

    auto addPillSep = [this, pillLayout]() {
        QFrame *sep = new QFrame(this);
        sep->setObjectName(QStringLiteral("FocusHudDivider"));
        sep->setFixedSize(1, 18);
        sep->setStyleSheet(
                QStringLiteral("QFrame#FocusHudDivider { background-color: rgba(148, 163, 184, 75); }"));
        pillLayout->addWidget(sep);
    };

    d_btn_plot = makeDarkBtn(QStringLiteral("plot-line-symbol"), nullptr);
    d_btn_plot->setPopupMode(QToolButton::InstantPopup);
    d_plot_menu = new QMenu(d_btn_plot);
    connect(d_plot_menu, &QMenu::aboutToShow, this, &FocusCanvasHudOverlay::populateQuickPlotMenu);
    d_btn_plot->setMenu(d_plot_menu);

    d_btn_analyze = makeDarkBtn(QStringLiteral("peak-analyzer"), nullptr);
    d_btn_analyze->setPopupMode(QToolButton::InstantPopup);
    d_analyze_menu = new QMenu(d_btn_analyze);
    connect(d_analyze_menu, &QMenu::aboutToShow, this, &FocusCanvasHudOverlay::populateAnalyzeMenu);
    d_btn_analyze->setMenu(d_analyze_menu);

    addPillSep();

    d_btn_presets = makeDarkBtn(QStringLiteral("preset-slide"), nullptr);
    d_btn_presets->setPopupMode(QToolButton::InstantPopup);
    d_presets_menu = new QMenu(d_btn_presets);
    connect(d_presets_menu, &QMenu::aboutToShow, this, &FocusCanvasHudOverlay::populatePresetsMenu);
    d_btn_presets->setMenu(d_presets_menu);

    d_btn_ole = makeDarkBtn(QStringLiteral("copy-ole"), [this]() {
        if (d_shutting_down || !d_app)
            return;
        QMdiSubWindow *sw = d_app->d_workspace.activeSubWindow();
        if (qobject_cast<MultiLayer *>(sw) == nullptr && qobject_cast<Graph3D *>(sw) == nullptr
            && d_ribbon
            && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph) {
            if (MyWidget *splitPlot = d_ribbon->splitPlotTarget())
                d_app->activateSubWindow(splitPlot);
        }
        d_app->copyGraphAsOleObject();
    });

    addPillSep();

    d_btn_palette = makeDarkBtn(QStringLiteral("search-command"), [this]() {
        if (!d_shutting_down && d_ribbon)
            d_ribbon->openCommandPalette();
    });

    const QString edgePillStyle = QStringLiteral(
            "QToolButton {"
            "  background-color: rgba(15, 23, 42, 230);"
            "  color: #F8FAFC;"
            "  border: 1px solid rgba(148, 163, 184, 95);"
            "  border-radius: 12px;"
            "  padding: 5px 11px;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "}"
            "QToolButton:hover {"
            "  background-color: rgba(37, 99, 235, 225);"
            "  border: 1px solid #60A5FA;"
            "  color: #FFFFFF;"
            "}"
            "QToolButton:checked {"
            "  background-color: #2563EB;"
            "  border: 1px solid #93C5FD;"
            "  color: #FFFFFF;"
            "}"
            "QToolButton:disabled {"
            "  color: #CBD5E1;"
            "}");

    d_btn_worksheet = new QToolButton(viewportParent);
    d_btn_worksheet->setObjectName(QStringLiteral("FocusHudWorksheetButton"));
    d_btn_worksheet->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_worksheet->setIcon(IconLoader::duotoneIcon(QStringLiteral("table-drawer"), 18, true));
    d_btn_worksheet->setIconSize(QSize(18, 18));
    d_btn_worksheet->setCheckable(true);
    d_btn_worksheet->setCursor(Qt::PointingHandCursor);
    d_btn_worksheet->setFocusPolicy(Qt::TabFocus);
    d_btn_worksheet->setStyleSheet(edgePillStyle);

    d_btn_exit_focus = new QToolButton(viewportParent);
    d_btn_exit_focus->setObjectName(QStringLiteral("FocusHudExitButton"));
    d_btn_exit_focus->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d_btn_exit_focus->setIcon(IconLoader::duotoneIcon(QStringLiteral("exit-focus-hud"), 18, true));
    d_btn_exit_focus->setIconSize(QSize(18, 18));
    d_btn_exit_focus->setCursor(Qt::PointingHandCursor);
    d_btn_exit_focus->setFocusPolicy(Qt::TabFocus);
    d_btn_exit_focus->setStyleSheet(edgePillStyle);

    d_table_drawer = new FocusTableDrawer(app, viewportParent);

    connect(d_btn_worksheet.data(), &QToolButton::toggled, this,
            &FocusCanvasHudOverlay::setTableDrawerVisible);
    connect(d_table_drawer.data(), &FocusTableDrawer::closeDrawerRequested, this,
            [this]() { setTableDrawerVisible(false); });
    connect(d_btn_exit_focus.data(), &QToolButton::clicked, this, [this]() {
        if (!d_shutting_down && d_ribbon)
            d_ribbon->setFocusHudMode(false);
    });

    if (d_viewport)
        d_viewport->installEventFilter(this);

    hide();
    d_btn_worksheet->hide();
    d_btn_exit_focus->hide();
    d_table_drawer->hide();

    retranslateUi();
}

FocusCanvasHudOverlay::~FocusCanvasHudOverlay()
{
    prepareForAppShutdown();
    if (d_btn_worksheet)
        delete d_btn_worksheet.data();
    if (d_btn_exit_focus)
        delete d_btn_exit_focus.data();
    if (d_table_drawer)
        delete d_table_drawer.data();
}

void FocusCanvasHudOverlay::prepareForAppShutdown()
{
    d_shutting_down = true;
    d_active = false;

    if (d_viewport) {
        d_viewport->removeEventFilter(this);
        d_viewport.clear();
    }
    if (d_table_drawer) {
        disconnect(d_table_drawer.data(), nullptr, this, nullptr);
        d_table_drawer->prepareForAppShutdown();
    }
    if (d_btn_worksheet)
        disconnect(d_btn_worksheet.data(), nullptr, this, nullptr);
    if (d_btn_exit_focus)
        disconnect(d_btn_exit_focus.data(), nullptr, this, nullptr);
    if (d_plot_menu)
        disconnect(d_plot_menu, nullptr, this, nullptr);
    if (d_analyze_menu)
        disconnect(d_analyze_menu, nullptr, this, nullptr);
    if (d_presets_menu)
        disconnect(d_presets_menu, nullptr, this, nullptr);

    d_app.clear();
    d_ribbon.clear();
}

FocusTableDrawer *FocusCanvasHudOverlay::tableDrawer() const
{
    return d_table_drawer.data();
}

QToolButton *FocusCanvasHudOverlay::worksheetToggleButton() const
{
    return d_btn_worksheet.data();
}

QToolButton *FocusCanvasHudOverlay::exitFocusButton() const
{
    return d_btn_exit_focus.data();
}

void FocusCanvasHudOverlay::populateQuickPlotMenu()
{
    if (d_shutting_down || !d_plot_menu)
        return;
    d_plot_menu->clear();
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-line"), 18, false),
                           tr("Line"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Line);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-scatter"), 18, false),
                           tr("Scatter"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Scatter);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 18, false),
                           tr("Line + Symbol"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::LineSymbols);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-bars"), 18, false),
                           tr("Bar Chart"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::VerticalBars);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-bars"), 18, false),
                           tr("Stacked Bars"), this, [this]() {
                               if (!d_shutting_down && d_app) {
                                   if (!QMetaObject::invokeMethod(d_app.data(), "plotStackedBars",
                                                                  Qt::DirectConnection)
                                       && d_ribbon) {
                                       d_ribbon->triggerPlot2D(Graph::VerticalBars);
                                   }
                               }
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-box"), 18, false),
                           tr("Box Plot"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerPlot2D(Graph::Box);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-violin"), 18, false),
                           tr("Violin Plot"), this, [this]() {
                               if (!d_shutting_down && d_app)
                                   d_app->plotViolin();
                           });
    d_plot_menu->addSeparator();
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-heatmap"), 18, false),
                           tr("Heatmap"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerMatrixOrXyzPlot(0);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-contour"), 18, false),
                           tr("Contour"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerMatrixOrXyzPlot(1);
                           });
    d_plot_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("plot-3d-surface"), 18, false),
                           tr("3D Surface"), this, [this]() {
                               if (!d_shutting_down && d_ribbon)
                                   d_ribbon->triggerMatrixOrXyzPlot(2);
                           });
}

void FocusCanvasHudOverlay::populateAnalyzeMenu()
{
    if (d_shutting_down || !d_analyze_menu)
        return;
    d_analyze_menu->clear();
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("peak-analyzer"), 18, false),
                              tr("Peak Analyzer"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->showPeakAnalyzerDialog(0);
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-roi"), 18, false),
                              tr("ROI Stats"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachRegionStatsGadget();
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-integrate"), 18, false),
                              tr("Integrate"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachIntegrateGadget();
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-quick-fit"), 18, false),
                              tr("Quick Fit"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachQuickFitGadget();
                              });
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-profile-2d"), 18, false),
                              tr("2D Profile"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->attachImageProfileGadget();
                              });
    d_analyze_menu->addSeparator();
    d_analyze_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("gadget-clear"), 18, false),
                              tr("Clear Gadget"), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->clearActiveGadget();
                              });
}

void FocusCanvasHudOverlay::populatePresetsMenu()
{
    if (d_shutting_down || !d_presets_menu)
        return;
    d_presets_menu->clear();
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-isij"), 18, false),
                              tr("ISIJ 85mm Preset"), this, [this]() {
                                  if (!d_shutting_down && d_app) {
                                      if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
                                          if (d_app->d_workspace.activeSubWindow() != ml)
                                              d_app->d_workspace.setActiveSubWindow(ml);
                                          d_app->applyStyleISIJJournal();
                                      }
                                  }
                              });
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 18, false),
                              tr("Lab Slide (18pt)"), this, [this]() {
                                  if (!d_shutting_down && d_app) {
                                      if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
                                          ml->applyStylePresetById(QStringLiteral("lab_slide_83mm"));
                                          d_app->modifiedProject();
                                          if (GraphInspectorDock *insp = d_app->graphInspectorDock())
                                              insp->refreshAll();
                                      }
                                  }
                              });
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("export-pptx"), 18, false),
                              ToriGraphPptx::sendToPowerPointLabel(), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      ToriGraphPptx::sendGraphsToPowerPoint(d_app);
                              });
    d_presets_menu->addSeparator();
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 18, false),
                              tr("Fit to Window"), this, [this]() {
                                  if (!d_shutting_down && d_app) {
                                      if (MultiLayer *ml = ModernRibbonHudInternal::activeMultiLayerOrUxHint(d_app)) {
                                          if (!QMetaObject::invokeMethod(ml, "fitPageZoomToViewport",
                                                                         Qt::DirectConnection)) {
                                              ml->setPageZoom(1.0);
                                          }
                                      }
                                  }
                              });
    d_presets_menu->addAction(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 18, false),
                              tr("Manage Style Presets..."), this, [this]() {
                                  if (!d_shutting_down && d_app)
                                      d_app->showStylePresetManagerDialog();
                              });
}

void FocusCanvasHudOverlay::retranslateUi()
{
    if (d_shutting_down)
        return;
    const QString kCaret = QString::fromUtf8(" \xE2\x96\xBE");
    if (d_btn_plot) {
        d_btn_plot->setText(tr("Plot") + kCaret);
        d_btn_plot->setToolTip(tr("Quick Plot from Active Worksheet"));
    }
    if (d_btn_analyze) {
        d_btn_analyze->setText(tr("Analyze") + kCaret);
        d_btn_analyze->setToolTip(tr("Peaks, ROI Statistics, Integration, Quick Fit & 2D Profile"));
    }
    if (d_btn_presets) {
        d_btn_presets->setText(tr("Presets") + kCaret);
        d_btn_presets->setToolTip(tr("Apply ISIJ 85mm, Lab Slide (18pt), or Custom Style Presets"));
    }
    if (d_btn_ole) {
        d_btn_ole->setText(ToriGraphOleUi::copyForPowerPointLabel());
        syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());
    }
    if (d_btn_palette) {
        d_btn_palette->setText(tr("Search"));
        d_btn_palette->setToolTip(tr("Open Command Palette (Ctrl+K)"));
    }
    if (d_btn_worksheet) {
        d_btn_worksheet->setText(tr("Worksheet"));
        d_btn_worksheet->setToolTip(tr("Toggle Slide-Over Linked Worksheet Drawer"));
    }
    if (d_btn_exit_focus) {
        d_btn_exit_focus->setText(tr("Exit Focus (F11)"));
        d_btn_exit_focus->setToolTip(tr("Exit Focus HUD Mode and Restore Ribbon (F11)"));
    }
    if (d_table_drawer)
        d_table_drawer->retranslateUi();

    populateQuickPlotMenu();
    populateAnalyzeMenu();
    populatePresetsMenu();
    if (d_active)
        repositionOverlayWidgets();
}

void FocusCanvasHudOverlay::setHostViewport(QWidget *host)
{
    if (d_shutting_down || !host || d_viewport == host)
        return;

    if (d_viewport)
        d_viewport->removeEventFilter(this);

    d_viewport = host;
    d_viewport->installEventFilter(this);

    auto reparentKeepVis = [host](QWidget *w) {
        if (!w || w->parentWidget() == host)
            return;
        const bool wasVis = !w->isHidden();
        w->setParent(host);
        w->setVisible(wasVis);
    };

    reparentKeepVis(this);
    reparentKeepVis(d_btn_worksheet.data());
    reparentKeepVis(d_btn_exit_focus.data());
    reparentKeepVis(d_table_drawer.data());

    if (d_active)
        repositionOverlayWidgets();
}

void FocusCanvasHudOverlay::setOverlayActive(bool active)
{
    if (d_shutting_down) {
        d_active = false;
        return;
    }
    d_active = active;
    if (!d_active) {
        hide();
        if (d_btn_worksheet) {
            QSignalBlocker blocker(d_btn_worksheet.data());
            d_btn_worksheet->setChecked(false);
            d_btn_worksheet->hide();
        }
        if (d_btn_exit_focus)
            d_btn_exit_focus->hide();
        if (d_table_drawer)
            d_table_drawer->hide();
        if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
            && !d_ribbon->isFocusHudMode()) {
            d_ribbon->applySplitWorkspaceGeometry();
        }
        return;
    }

    if (d_app) {
        QWidget *preferredHost = d_app->centralWidget() ? d_app->centralWidget()
                                                        : d_app->d_workspace.viewport();
        if (preferredHost)
            setHostViewport(preferredHost);
    }

    if (d_table_drawer && isTableDrawerVisible())
        d_table_drawer->refreshTableList(true);

    syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());

    show();
    if (d_btn_worksheet)
        d_btn_worksheet->show();
    if (d_btn_exit_focus)
        d_btn_exit_focus->show();

    repositionOverlayWidgets();
    if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph)
        d_ribbon->applySplitWorkspaceGeometry();
}

void FocusCanvasHudOverlay::setTableDrawerVisible(bool visible)
{
    if (d_shutting_down)
        return;

    if (d_btn_worksheet && d_btn_worksheet->isChecked() != visible) {
        QSignalBlocker blocker(d_btn_worksheet.data());
        d_btn_worksheet->setChecked(visible);
    }
    if (!d_table_drawer)
        return;

    if (visible && d_active) {
        d_table_drawer->refreshTableList(true);
        d_table_drawer->show();
        d_table_drawer->raise();
        repositionOverlayWidgets();
        if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph)
            d_ribbon->applySplitWorkspaceGeometry();
        syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());
    } else {
        const bool hadDrawerFocus = d_table_drawer->isAncestorOf(QApplication::focusWidget())
                || QApplication::focusWidget() == d_table_drawer;
        d_table_drawer->hide();
        if (d_ribbon && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph)
            d_ribbon->applySplitWorkspaceGeometry();
        if (hadDrawerFocus && d_app && d_ribbon) {
            if (MyWidget *splitPlot = d_ribbon->splitPlotTarget())
                splitPlot->setFocus();
        }
        syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());
    }
}

bool FocusCanvasHudOverlay::isTableDrawerVisible() const
{
    return d_table_drawer && !d_table_drawer->isHidden();
}

void FocusCanvasHudOverlay::refreshActiveContext()
{
    if (d_shutting_down)
        return;
    syncFocusHudOleButton(d_btn_ole, d_app.data(), d_ribbon.data());
    if (d_active) {
        if (isTableDrawerVisible() && d_table_drawer)
            d_table_drawer->refreshTableList(true);
        repositionOverlayWidgets();
    }
}

void FocusCanvasHudOverlay::repositionOverlayWidgets()
{
    if (d_shutting_down || !d_active)
        return;

    const QSize vpSz = (d_viewport && d_viewport->width() >= 160 && d_viewport->height() >= 100)
            ? d_viewport->size()
            : (d_app ? d_app->size() : QSize(1024, 720));

    int topY = 10;
    if (d_app && d_ribbon && d_viewport == d_app->centralWidget()) {
        if (d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutTabbed) {
            int tabBarH = 0;
            if (QTabBar *mdiTb = d_app->d_workspace.findChild<QTabBar *>()) {
                if (!mdiTb->isHidden() && mdiTb->height() > 0 && mdiTb->height() < 64)
                    tabBarH = mdiTb->height();
            }
            topY = tabBarH + 8;
            if (MultiLayer *ml =
                        qobject_cast<MultiLayer *>(d_app->d_workspace.activeSubWindow())) {
                // Offset below MultiLayer's top header strip ([1][+][Plot Columns] & dimension badge)
                topY = qMax(topY, tabBarH + 40);
                if (QFrame *pill = ml->findChild<QFrame *>(
                            QStringLiteral("MultiLayerPillContainer"))) {
                    if (!pill->isHidden() && pill->height() > 0 && d_viewport) {
                        const int pillBot =
                                pill->mapTo(d_viewport.data(), QPoint(0, pill->height())).y();
                        if (pillBot > 0 && pillBot < 140)
                            topY = qMax(topY, pillBot + 6);
                    }
                }
            }
        } else if (d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
                   || d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutFreeMdi) {
            topY = 6;
        }
    }

    int wsW = 100;
    if (d_btn_worksheet) {
        d_btn_worksheet->adjustSize();
        const QSize wsSz = d_btn_worksheet->sizeHint();
        wsW = qMax(100, wsSz.width());
        d_btn_worksheet->setGeometry(12, topY, wsW, qMax(32, wsSz.height()));
        d_btn_worksheet->raise();
    }

    int ew = 110;
    if (d_btn_exit_focus) {
        d_btn_exit_focus->adjustSize();
        const QSize exSz = d_btn_exit_focus->sizeHint();
        ew = qMax(110, exSz.width());
        const int eh = qMax(32, exSz.height());
        d_btn_exit_focus->setGeometry(qMax(12, vpSz.width() - ew - 12), topY, ew, eh);
        d_btn_exit_focus->raise();
    }

    const int leftLimit = 12 + wsW + 10;
    const int rightLimit = qMax(leftLimit + 160, vpSz.width() - 12 - ew - 10);
    const int availW = qMax(160, rightLimit - leftLimit);

    QToolButton *pillBtns[] = { d_btn_plot, d_btn_analyze, d_btn_presets, d_btn_ole,
                                d_btn_palette };

    auto setPillButtonStyle = [&pillBtns, this](Qt::ToolButtonStyle st) {
        bool changed = false;
        for (QToolButton *pb : pillBtns) {
            if (pb && pb->toolButtonStyle() != st) {
                pb->setToolButtonStyle(st);
                changed = true;
            }
        }
        if (changed && layout())
            layout()->activate();
    };

    // A4-3: Keep readable text labels even on 911px/1080px screens thanks to the compact 5-button
    // pill; only degrade less-critical buttons (or all) to icon-only if space truly runs out.
    setPillButtonStyle(Qt::ToolButtonTextBesideIcon);
    adjustSize();
    if (sizeHint().width() > availW) {
        if (d_btn_palette)
            d_btn_palette->setToolButtonStyle(Qt::ToolButtonIconOnly);
        if (d_btn_ole)
            d_btn_ole->setToolButtonStyle(Qt::ToolButtonIconOnly);
        if (layout())
            layout()->activate();
        adjustSize();
        if (sizeHint().width() > availW)
            setPillButtonStyle(Qt::ToolButtonIconOnly);
    }

    adjustSize();
    const QSize pillSz = sizeHint();
    const int pw = qMin(pillSz.width(), availW);
    const int ph = qMax(36, pillSz.height());
    const int centeredPx = (vpSz.width() - pw) / 2;
    const int px = qBound(leftLimit, centeredPx, qMax(leftLimit, rightLimit - pw));
    setGeometry(px, topY, pw, ph);
    raise();

    if (d_table_drawer && !d_table_drawer->isHidden()) {
        const int drawerTop = topY + ph + 10;
        const int drawerH = qMax(200, vpSz.height() - drawerTop - 14);
        const int drawerW =
                qBound(240, qMin(396, qRound(vpSz.width() * 0.36)), qMax(240, vpSz.width() - 24));
        d_table_drawer->setFixedWidth(drawerW);
        d_table_drawer->setGeometry(12, drawerTop, drawerW, drawerH);
        d_table_drawer->raise();
    }
}

bool FocusCanvasHudOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (d_shutting_down)
        return QFrame::eventFilter(watched, event);
    if (d_active && watched == d_viewport && event) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show
            || event->type() == QEvent::ChildAdded) {
            repositionOverlayWidgets();
            if (event->type() == QEvent::Resize && d_ribbon
                && d_ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
                && !d_ribbon->isApplyingSplitGeometry() && isTableDrawerVisible()) {
                d_ribbon->applySplitWorkspaceGeometry();
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void FocusCanvasHudOverlay::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
}
