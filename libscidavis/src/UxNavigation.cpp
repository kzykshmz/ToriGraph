/***************************************************************************
    File                 : UxNavigation.cpp
    Description          : Beginner navigation, menus and UI state:
                           - F1 quick guide (B1)
                           - plotting from a table with guidance instead of silence (B6, B7)
                           - empty-table paste hint and "plot this data" bar (B4)
                           - tool-mode indicator in the status bar (C3)
                           - dialogs that fit 1366x768 at 150 % (C5)
                           - shared graph context menu (C7)
                           - "Show other-field / advanced features" switch (G1)
                           - "Show Legacy UI" switch (C15)
 ***************************************************************************/
#include "UxNavigation.h"

#include "ApplicationWindow.h"
#include "AxesDialog.h"
#include "CanvasInlineTextEditor.h"
#include "CurveMathTools.h"
#include "Graph.h"
#include "GraphInspectorDock.h"
#include "I18nJapanese.h"
#include "Legend.h"
#include "ModernRibbonAndHud.h"
#include "MultiLayer.h"
#include "Plot.h"
#include "PlotToolInterface.h"
#include "Table.h"
#include "IconLoader.h"
#include "core/column/Column.h"
#include "future/table/TableModel.h"

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_scale_widget.h>
#include <qwt_text_label.h>

#include <cmath>

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QErrorMessage>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QWizard>

#include <mutex>

#define MANUAL_URI_JA "https://github.com/kzykshmz/ToriGraph/blob/main/README.ja.md"

namespace ToriUx {

void registerUxTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        // Only fills gaps: a word that already has a translation elsewhere (e.g. "Plot",
        // "Zoom In") keeps it, so these registrations never change existing UI text.
        auto reg = [](const char *en, const char *ja) {
            const QString src = QString::fromUtf8(en);
            if (!ToriGraphFullJapaneseTranslator::lookupJapanese(src).isEmpty())
                return;
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(src, QString::fromUtf8(ja));
        };
        // B1 help
        reg("The built-in guide (manual/index.html) was not found; opening the Japanese README on GitHub instead.",
            "同梱のガイド (manual/index.html) が見つからないため、GitHub の日本語 README を開きます。");
        reg("The guide could not be opened automatically. Open this address in a web browser:",
            "ガイドを自動で開けませんでした。次のアドレスをブラウザで開いてください:");
        // A11 close prompt
        reg("Do&n't Save", "保存しない(&N)");
        // B7 messages
        reg("Click a column header in the worksheet to select it, then press the plot button again.",
            "ワークシートの列見出しをクリックして列を選び、もう一度作図ボタンを押してください。");
        reg("Right-click the header of the column to use as X and choose \"Set Column(s) As\" > \"X\", then plot again.",
            "X にしたい列の見出しを右クリックし、「列の役割を設定」→「X」を選んでから、もう一度作図してください。");
        reg("Click the header of the Y column(s) to plot (Ctrl+click for several). A column becomes Y via right-click > \"Set Column(s) As\" > \"Y\".",
            "描きたい Y 列の見出しをクリックして選んでください(複数なら Ctrl+クリック)。列を Y にするには、見出しを右クリック →「列の役割を設定」→「Y」です。");
        reg("Paste X and Y data from Excel (Ctrl+V) or add a column (Home > More New > Add Column).",
            "Excel から X と Y のデータを貼り付ける(Ctrl+V)か、列を追加してください(ホーム →「その他の新規作成」→「列の追加」)。");
        reg("Nothing to plot yet", "まだ作図できません");
        reg("The worksheet \"%1\" is empty, so no graph was created.",
            "ワークシート「%1」が空のため、グラフは作りませんでした。");
        reg("Copy a range in Excel and paste it into the worksheet with Ctrl+V, or drop a CSV file onto the window. Then press the plot button again.",
            "Excel で範囲をコピーしてワークシートに Ctrl+V で貼り付けるか、Excel (.xlsx) / CSV ファイルをウィンドウにドロップしてから、もう一度作図ボタンを押してください。");
        reg("Copy a range in Excel and paste it into the worksheet with Ctrl+V, or drop an Excel (.xlsx) or CSV file onto the window. Then press the plot button again.",
            "Excel で範囲をコピーしてワークシートに Ctrl+V で貼り付けるか、Excel (.xlsx) / CSV ファイルをウィンドウにドロップしてから、もう一度作図ボタンを押してください。");
        reg("No numeric data in Y column of \"%1\".",
            "「%1」の Y 列に数値データがありません。");
        reg("Enter numeric values in column B(Y), or paste both X and Y columns from Excel (Ctrl+V).",
            "B(Y) 列に数値を入力するか、Excel から X と Y の 2 列を貼り付けてください (Ctrl+V)。");
        reg("Graph export works on a graph window. Click the graph you want to export first.",
            "画像のエクスポートはグラフのウィンドウが対象です。先にエクスポートしたいグラフをクリックしてください。");
        reg("Copying for PowerPoint works on a graph window. Click the graph you want to copy first.",
            "PowerPoint 用のコピーはグラフのウィンドウが対象です。先にコピーしたいグラフをクリックしてください。");
        // B6 ribbon plot buttons
        reg("Created a new graph from \"%1\", the worksheet of the active graph.",
            "選択中のグラフの元の表「%1」から新しいグラフを作りました。");
        reg("The plot buttons make a new graph from worksheet columns. This graph has no source worksheet (function or image data); click a worksheet first.",
            "作図ボタンはワークシートの列から新しいグラフを作ります。このグラフには元の表がありません(関数や画像のデータ)。先にワークシートをクリックしてください。");
        reg("The plot buttons make a graph from worksheet columns. Click a worksheet (or paste data from Excel) first.",
            "作図ボタンはワークシートの列からグラフを作ります。先にワークシートをクリックしてください(Excel から貼り付けても始められます)。");
        // B4
        reg("Paste from Excel here (Ctrl+V)\nor drop a CSV file onto this window",
            "ここへ Excel から貼り付け (Ctrl+V)\nまたは Excel (.xlsx) / CSV ファイルをこのウィンドウにドロップ");
        reg("Paste from Excel here (Ctrl+V)\nor drop an Excel (.xlsx) or CSV file onto this window",
            "ここへ Excel から貼り付け (Ctrl+V)\nまたは Excel (.xlsx) / CSV ファイルをこのウィンドウにドロップ");
        reg("Insert Simple X-Y Sample", "シンプルな X-Y サンプルを挿入");
        reg("Load a simple 2-series X-Y sample dataset and plot it right away",
            "シンプルな 2 系列 X-Y サンプルを読み込み、すぐにグラフを作成します");
        reg("Open CSV / Excel...", "CSV / Excel を開く...");
        reg("Open a CSV, TSV, or Excel (.xlsx) data file",
            "CSV・TSV・Excel (.xlsx) データファイルを開きます");
        reg("Plot this data", "このデータをグラフにする");
        reg("Line + symbol graph of all Y columns (use the Plot tab for other styles)",
            "すべての Y 列を折れ線+シンボルで描きます (ほかの形は「作図」タブから)");
        reg("Dismiss", "閉じる");
        // C3 tool mode
        reg("Mode: %1 (Esc to cancel)", "モード: %1(Esc で解除)");
        reg("Click (or press Esc) to return to the pointer", "クリック(または Esc)でポインタに戻ります");
        reg("Zoom In", "拡大");
        reg("Zoom Out", "縮小");
        reg("Draw Arrow", "矢印を描く");
        reg("Draw Line", "線を描く");
        reg("Add Text (click on the graph)", "テキストの追加(グラフ上をクリック)");
        reg("Data Reader", "データ読取");
        reg("Move Data Points", "データ点の移動");
        reg("Remove Data Points", "データ点の削除");
        reg("Mask Data Points", "データ点のマスク");
        reg("Unmask Data Points", "データ点のマスク解除");
        reg("Screen Reader", "画面座標の読取");
        reg("Line Profile", "ラインプロファイル");
        reg("Multi-Peak Fit (click the peaks)", "複数ピークのフィット(ピークをクリック)");
        reg("Select Data Range", "データ範囲の選択");
        reg("Translate Curve", "曲線の平行移動");
        reg("ROI Gadget", "ROI ガジェット");
        reg("Vertical Cursor", "垂直カーソル");
        reg("Delta Cursor", "デルタカーソル");
        reg("Image Profile", "画像プロファイル");
        reg("Click on the graph where the text should go, then type it (Esc to cancel).",
            "文字を入れたい位置でグラフをクリックし、そのまま入力してください(Esc で取り消し)。");
        // C7 context menu
        reg("Properties of the Selection...", "選択した要素のプロパティ...");
        reg("Delete Selection", "選択した要素を削除");
        reg("Edit Title", "タイトルを編集");
        reg("Title Properties...", "タイトルのプロパティ...");
        reg("Delete Title", "タイトルを削除");
        reg("Edit Reference Line...", "基準線を編集...");
        reg("Delete Reference Line", "基準線を削除");
        reg("Edit Axis Title", "軸タイトルを編集");
        reg("Edit Axis...", "軸を編集...");
        reg("Edit Text", "テキストを編集");
        reg("Rescale This Axis", "この軸を自動スケール");
        reg("Add Reference Line...", "基準線を追加...");
        reg("Lab Slide (18pt)", "研究室スライド (18pt)");
        reg("ISIJ 85mm", "ISIJ 85mm");
        reg("Journal 2-Column (175mm)", "論文 2 段幅 (175mm)");
        reg("&Axes...", "軸(&A)...");
        reg("Add &Text", "テキストの追加(&T)");
        reg("Anal&yze", "解析(&Y)");
        reg("St&yle", "体裁(&Y)");
        reg("Layer P&roperties...", "レイヤーのプロパティ(&R)...");
        reg("E&xport Graph...", "画像をエクスポート(&X)...");
        reg("Export Graph...", "画像をエクスポート...");
        reg("E&xport Image...", "画像をエクスポート(&X)...");
        reg("Export Image...", "画像をエクスポート...");
        reg("Export Image / PDF...", "画像 / PDF エクスポート...");
        reg("Export Active Layer...", "アクティブレイヤーをエクスポート...");
        reg("&Print...", "印刷(&P)...");
        reg("Print...", "印刷...");
        // C6 menus
        reg("Worksheet &Analysis", "表の解析(&A)");
        // C11 undo
        reg("Undo history of this graph window (each graph has its own).",
            "このグラフのウィンドウの操作履歴です(グラフごとに別の履歴です)。");
        reg("Undo history of the worksheets (shared by all tables and matrices).",
            "ワークシートの操作履歴です(すべての表と行列で共通です)。");
        // C12 command palette
        reg("Not available now: open or create a worksheet or graph first.",
            "今は使えません: 先にワークシートかグラフを開くか作ってください。");
        reg("Not available for a worksheet: click a graph window first.",
            "ワークシートでは使えません: 先にグラフのウィンドウをクリックしてください。");
        reg("Not available for this window: click a worksheet (or select a curve) first.",
            "このウィンドウでは使えません: 先にワークシートをクリックするか、曲線を選んでください。");
        reg("Not available in the current state.", "今の状態では使えません。");
        // G1
        reg("Show Other-Field / Advanced Features", "他分野・詳細機能を表示");
        reg("Show medical statistics, EIS, Tauc, wavelet/STFT/Hilbert, pie/Pareto and 3D ribbon in menus, the ribbon and command search",
            "医療統計、EIS、Tauc、ウェーブレット・STFT・Hilbert、円グラフ・パレート図、3D リボンをメニュー・リボン・コマンド検索に表示します");
        // C4 ribbon
        reg("Undo", "元に戻す");
        reg("Redo", "やり直し");
        reg("Pointer", "ポインタ");
        reg("Zoom", "ズーム");
        reg("Reader", "データ読取");
        reg("Text", "テキスト");
        reg("Legend", "凡例");
        reg("Error Bars", "エラーバー");
        reg("2 Stacked", "上下 2 段");
        reg("2x2 Panels", "4 分割");
        reg("Undo the last change in the active window (Ctrl+Z)", "選択中のウィンドウで直前の変更を元に戻す (Ctrl+Z)");
        reg("Redo the last undone change (Ctrl+Y)", "元に戻した変更をやり直す (Ctrl+Y)");
        reg("Pointer: leave zoom, reader and gadget modes (Esc)", "ポインタ: ズーム・データ読取・ガジェットのモードを解除 (Esc)");
        reg("Zoom into a dragged rectangle (Ctrl++)", "ドラッグした四角の範囲を拡大 (Ctrl++)");
        reg("Read data values by clicking points (Ctrl+D)", "点をクリックしてデータの値を読む (Ctrl+D)");
        reg("Add text: click on the graph where it should go (Ctrl+Shift+T)", "テキストの追加: 入れたい位置でグラフをクリック (Ctrl+Shift+T)");
        reg("Add or rebuild the legend", "凡例を追加・作り直す");
        reg("Add error bars from a column or a percentage", "列または割合からエラーバーを付ける");
        reg("Two graphs stacked vertically", "上下 2 段のグラフ");
        reg("Four graphs in a 2x2 grid", "2×2 の 4 分割グラフ");
        reg("Edit", "編集");
        reg("Tools", "ツール");
        reg("Annotate", "注釈");
        reg("Panels", "パネル");
        // C2 inspector
        reg("Details...", "詳細設定…");
        reg("Add text, legend, error bars or a 2-panel / 4-panel layout",
            "テキスト・凡例・エラーバーの追加、上下 2 段・4 分割の配置");
        reg("Paste from Excel into the table (Ctrl+V) or drop a CSV file",
            "Excel からコピーして表に貼り付け(Ctrl+V)、または CSV ファイルをドロップ");
        reg("Open the dialog with every setting for this section", "この節のすべての設定があるダイアログを開く");
        reg("Axes", "軸");
        reg("X Axis (Bottom)", "X 軸(下)");
        reg("Y Axis (Left)", "Y 軸(左)");
        reg("X Axis (Top)", "X 軸(上)");
        reg("Y Axis (Right)", "Y 軸(右)");
        // C14 ribbon tabs
        reg("Home", "ホーム");
        reg("Materials", "材料解析");
        reg("Analysis", "解析");
        reg("Format & Export", "書式・出力");
        reg("Weibull", "ワイブル");
        reg("Arrhenius", "アレニウス");
    });
}

QString quickGuideFilePath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/manual/index.html"),
        // development builds: <repo>/build/scidavis/ToriGraph.exe -> <repo>/manual
        appDir + QStringLiteral("/../../manual/index.html"),
        appDir + QStringLiteral("/../manual/index.html"),
        appDir + QStringLiteral("/../share/doc/torigraph/manual/index.html"),
    };
    for (const QString &c : candidates) {
        const QFileInfo fi(c);
        // The old placeholder was a 3-byte file; treat anything that small as "not installed".
        if (fi.isFile() && fi.size() > 256)
            return QDir::cleanPath(fi.absoluteFilePath());
    }
    return QString();
}

namespace {

constexpr int kFrameAllowanceW = 24; // left + right window border & margin
constexpr int kFrameAllowanceH = 48; // title bar + bottom border & taskbar clearance

bool isScreenFitCandidate(const QWidget *w)
{
    const QDialog *dlg = qobject_cast<const QDialog *>(w);
    if (!dlg || !dlg->isWindow())
        return false;
    // Qt's own dialogs size themselves (and the file dialog is usually native).
    if (qobject_cast<const QMessageBox *>(dlg) || qobject_cast<const QFileDialog *>(dlg)
        || qobject_cast<const QInputDialog *>(dlg) || qobject_cast<const QProgressDialog *>(dlg)
        || qobject_cast<const QColorDialog *>(dlg) || qobject_cast<const QFontDialog *>(dlg)
        || qobject_cast<const QErrorMessage *>(dlg) || qobject_cast<const QWizard *>(dlg))
        return false;
    if (dlg->property("toriNoScreenFit").toBool())
        return false;
    return true;
}

bool isButtonRow(QLayoutItem *item)
{
    if (!item)
        return false;
    if (QWidget *w = item->widget())
        return qobject_cast<QDialogButtonBox *>(w) != nullptr;
    QLayout *lay = item->layout();
    if (!lay || lay->count() == 0)
        return false;
    bool sawPush = false;
    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem *it = lay->itemAt(i);
        if (!it || it->spacerItem())
            continue;
        QWidget *w = it->widget();
        if (!w)
            return false;
        if (qobject_cast<QDialogButtonBox *>(w)) {
            sawPush = true;
            continue;
        }
        if (!qobject_cast<QAbstractButton *>(w))
            return false;
        if (qobject_cast<QPushButton *>(w))
            sawPush = true;
    }
    return sawPush;
}

void collectLayoutWidgets(QLayout *lay, QList<QPointer<QWidget>> &out)
{
    if (!lay)
        return;
    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem *it = lay->itemAt(i);
        if (!it)
            continue;
        if (QWidget *w = it->widget())
            out.append(w);
        else if (QLayout *sub = it->layout())
            collectLayoutWidgets(sub, out);
    }
}

// Move the content of a dialog's top-level box/grid/form layout into a QScrollArea. A trailing
// button row (QDialogButtonBox or a row of push buttons) stays outside so OK/Cancel remain visible.
bool wrapLayoutInScrollArea(QWidget *window)
{
    QLayout *old = window->layout();
    QBoxLayout *oldBox = qobject_cast<QBoxLayout *>(old);
    QGridLayout *oldGrid = qobject_cast<QGridLayout *>(old);
    QFormLayout *oldForm = qobject_cast<QFormLayout *>(old);
    if (!oldBox && !oldGrid && !oldForm)
        return false;

    // Remember which widgets were meant to be visible: reparenting hides them.
    QList<QPointer<QWidget>> widgets;
    collectLayoutWidgets(old, widgets);
    QList<QPointer<QWidget>> shownBefore;
    for (const QPointer<QWidget> &w : widgets) {
        if (w && !w->isHidden())
            shownBefore.append(w);
    }

    const QMargins margins = old->contentsMargins();
    const int spacing = old->spacing();
    const QBoxLayout::Direction dir = oldBox ? oldBox->direction() : QBoxLayout::TopToBottom;
    const int hSpacing = oldGrid ? oldGrid->horizontalSpacing()
                                 : (oldForm ? oldForm->horizontalSpacing() : spacing);
    const int vSpacing = oldGrid ? oldGrid->verticalSpacing()
                                 : (oldForm ? oldForm->verticalSpacing() : spacing);
    const QFormLayout::FieldGrowthPolicy formFieldGrowth =
            oldForm ? oldForm->fieldGrowthPolicy() : QFormLayout::ExpandingFieldsGrow;
    const QFormLayout::RowWrapPolicy formRowWrap =
            oldForm ? oldForm->rowWrapPolicy() : QFormLayout::DontWrapRows;
    const Qt::Alignment formLabelAlign = oldForm ? oldForm->labelAlignment() : Qt::Alignment();
    const Qt::Alignment formAlign = oldForm ? oldForm->formAlignment() : Qt::Alignment();

    struct Moved
    {
        QLayoutItem *item = nullptr;
        int stretch = 0;
        int row = 0, col = 0, rowSpan = 1, colSpan = 1;
        QFormLayout::ItemRole formRole = QFormLayout::SpanningRole;
    };
    QList<Moved> moved;
    int maxRow = -1;
    for (int i = 0; i < old->count(); ++i) {
        Moved m;
        if (oldBox) {
            m.stretch = oldBox->stretch(i);
        } else if (oldGrid) {
            oldGrid->getItemPosition(i, &m.row, &m.col, &m.rowSpan, &m.colSpan);
            if (m.row > maxRow)
                maxRow = m.row;
        } else if (oldForm) {
            oldForm->getItemPosition(i, &m.row, &m.formRole);
            if (m.row > maxRow)
                maxRow = m.row;
        }
        moved.append(m);
    }
    for (int i = 0; i < moved.size(); ++i)
        moved[i].item = old->takeAt(0);

    QLayoutItem *buttonRow = nullptr;
    int buttonStretch = 0;
    if (oldBox && dir == QBoxLayout::TopToBottom && !moved.isEmpty()
        && isButtonRow(moved.last().item)) {
        buttonRow = moved.last().item;
        buttonStretch = moved.last().stretch;
        moved.removeLast();
    } else if ((oldForm || oldGrid) && maxRow >= 0) {
        int countOnLastRow = 0;
        int lastRowIdx = -1;
        for (int i = 0; i < moved.size(); ++i) {
            if (moved[i].row == maxRow) {
                ++countOnLastRow;
                lastRowIdx = i;
            }
        }
        if (countOnLastRow == 1 && lastRowIdx >= 0 && isButtonRow(moved[lastRowIdx].item)
            && (!oldForm || moved[lastRowIdx].formRole == QFormLayout::SpanningRole
                || moved[lastRowIdx].formRole == QFormLayout::FieldRole)) {
            buttonRow = moved[lastRowIdx].item;
            moved.removeAt(lastRowIdx);
        }
    }

    // Detach sub-layouts from the old layout before deleting it (QObject children die with it).
    for (const Moved &m : moved) {
        if (m.item && m.item->layout())
            m.item->layout()->setParent(nullptr);
    }
    if (buttonRow && buttonRow->layout())
        buttonRow->layout()->setParent(nullptr);
    delete old;

    QVBoxLayout *outer = new QVBoxLayout(window);
    outer->setContentsMargins(margins);
    outer->setSpacing(spacing);

    QScrollArea *scroll = new QScrollArea(window);
    scroll->setObjectName(QStringLiteral("ToriScreenFitScrollArea"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
    QWidget *inner = new QWidget(scroll);
    inner->setObjectName(QStringLiteral("ToriScreenFitInner"));
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);

    auto addTo = [](QLayout *target, const Moved &m, QBoxLayout *asBox, QGridLayout *asGrid,
                    QFormLayout *asForm) {
        QLayoutItem *it = m.item;
        if (!it)
            return;
        if (QWidget *w = it->widget()) {
            const Qt::Alignment al = it->alignment();
            delete it;
            if (asBox)
                asBox->addWidget(w, m.stretch, al);
            else if (asGrid)
                asGrid->addWidget(w, m.row, m.col, m.rowSpan, m.colSpan, al);
            else if (asForm)
                asForm->setWidget(m.row, m.formRole, w);
        } else if (QLayout *sub = it->layout()) {
            if (asBox)
                asBox->addLayout(sub, m.stretch);
            else if (asGrid)
                asGrid->addLayout(sub, m.row, m.col, m.rowSpan, m.colSpan);
            else if (asForm)
                asForm->setLayout(m.row, m.formRole, sub);
        } else {
            if (asBox)
                asBox->addItem(it);
            else if (asGrid)
                asGrid->addItem(it, m.row, m.col, m.rowSpan, m.colSpan);
            else if (asForm)
                asForm->setItem(m.row, m.formRole, it);
        }
        Q_UNUSED(target);
    };

    if (oldBox) {
        QBoxLayout *nb = new QBoxLayout(dir, inner);
        nb->setContentsMargins(0, 0, 0, 0);
        nb->setSpacing(spacing);
        for (const Moved &m : moved)
            addTo(nb, m, nb, nullptr, nullptr);
    } else if (oldGrid) {
        QGridLayout *ng = new QGridLayout(inner);
        ng->setContentsMargins(0, 0, 0, 0);
        ng->setHorizontalSpacing(hSpacing);
        ng->setVerticalSpacing(vSpacing);
        for (const Moved &m : moved)
            addTo(ng, m, nullptr, ng, nullptr);
    } else if (oldForm) {
        QFormLayout *nf = new QFormLayout(inner);
        nf->setContentsMargins(0, 0, 0, 0);
        nf->setHorizontalSpacing(hSpacing);
        nf->setVerticalSpacing(vSpacing);
        nf->setFieldGrowthPolicy(formFieldGrowth);
        nf->setRowWrapPolicy(formRowWrap);
        if (formLabelAlign != Qt::Alignment())
            nf->setLabelAlignment(formLabelAlign);
        if (formAlign != Qt::Alignment())
            nf->setFormAlignment(formAlign);
        for (const Moved &m : moved)
            addTo(nf, m, nullptr, nullptr, nf);
    }

    if (buttonRow) {
        Moved b;
        b.item = buttonRow;
        b.stretch = buttonStretch;
        addTo(outer, b, outer, nullptr, nullptr);
    }

    for (const QPointer<QWidget> &w : shownBefore) {
        if (w && w->isHidden())
            w->show();
    }
    window->setProperty("toriScreenFitWrapped", true);
    return true;
}

class DialogScreenFitFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        const QEvent::Type t = event->type();
        if ((t == QEvent::Polish || t == QEvent::Show) && watched->isWidgetType()) {
            QWidget *w = static_cast<QWidget *>(watched);
            if (isScreenFitCandidate(w))
                fitWindowToAvailableScreen(w);
        }
        return QObject::eventFilter(watched, event);
    }
};

void refreshWidgetAndDescendantLayouts(QWidget *root)
{
    if (!root)
        return;
    root->ensurePolished();
    const QList<QWidget *> descendants = root->findChildren<QWidget *>();
    for (QWidget *w : descendants) {
        if (w)
            w->updateGeometry();
    }
    const QList<QLayout *> layouts = root->findChildren<QLayout *>();
    for (int i = layouts.size() - 1; i >= 0; --i) {
        if (layouts.at(i))
            layouts.at(i)->invalidate();
    }
    if (QLayout *rl = root->layout()) {
        rl->invalidate();
        rl->activate();
    }
    root->updateGeometry();
}

} // namespace

bool fitWindowToAvailableScreen(QWidget *window, const QRect &available)
{
    if (!window || !window->isWindow())
        return false;

    QRect avail = available;
    if (!avail.isValid()) {
        QScreen *scr = window->screen();
        if (!scr || window->geometry().isEmpty())
            scr = QGuiApplication::screenAt(QCursor::pos());
        if (!scr)
            scr = QGuiApplication::primaryScreen();
        if (!scr)
            return false;
        avail = scr->availableGeometry();
    }
    const QSize maxClient(qMax(200, avail.width() - kFrameAllowanceW),
                          qMax(150, avail.height() - kFrameAllowanceH));

    if (window->minimumWidth() > maxClient.width() || window->minimumHeight() > maxClient.height()) {
        window->setMinimumSize(qMin(window->minimumWidth(), maxClient.width()),
                               qMin(window->minimumHeight(), maxClient.height()));
    }

    bool wrapped = false;
    int preWrapWidth = window->width();
    int preWrapHeight = window->height();
    if (QLayout *lay = window->layout()) {
        const QSize minSz = lay->totalMinimumSize();
        const QSize hintSz = lay->totalSizeHint();
        if (!window->testAttribute(Qt::WA_Resized)) {
            preWrapWidth = qMax(window->minimumWidth(), qMax(minSz.width(), hintSz.width()));
            preWrapHeight = qMax(window->minimumHeight(), qMax(minSz.height(), hintSz.height()));
        } else {
            preWrapWidth = qMax(preWrapWidth, minSz.width());
            preWrapHeight = qMax(preWrapHeight, minSz.height());
        }
        const bool needsWrap = (minSz.width() > maxClient.width()
                                || minSz.height() > maxClient.height()
                                || hintSz.height() > maxClient.height());
        if (needsWrap && !window->property("toriScreenFitWrapped").toBool()
            && !window->property("toriHasInternalScroll").toBool()
            && qobject_cast<QDialog *>(window)) {
            wrapped = wrapLayoutInScrollArea(window);
        }
    }

    const QSize minNow = window->minimumSize();
    if (minNow.width() > maxClient.width() || minNow.height() > maxClient.height())
        window->setMinimumSize(minNow.boundedTo(maxClient));

    QScrollArea *fitScroll = window->findChild<QScrollArea *>(
            QStringLiteral("ToriScreenFitScrollArea"), Qt::FindDirectChildrenOnly);
    if (wrapped || fitScroll) {
        int vSbW = window->style()
                ? window->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, window)
                : 18;
        int outerMarginsW = 0;
        const QMargins winMargins = window->contentsMargins();
        outerMarginsW += winMargins.left() + winMargins.right();
        int outerLayoutW = 0;
        if (QLayout *ol = window->layout()) {
            const QMargins om = ol->contentsMargins();
            outerMarginsW += om.left() + om.right();
            outerLayoutW = qMax(ol->totalMinimumSize().width(), ol->totalSizeHint().width());
        }
        int innerNeedW = 0;
        if (fitScroll) {
            if (QScrollBar *vsb = fitScroll->verticalScrollBar())
                vSbW = qMax(vSbW, vsb->sizeHint().width());
            const QMargins sm = fitScroll->contentsMargins();
            outerMarginsW += sm.left() + sm.right() + 2 * fitScroll->frameWidth();
            if (QWidget *inner = fitScroll->widget()) {
                refreshWidgetAndDescendantLayouts(inner);
                innerNeedW = qMax(inner->sizeHint().width(), inner->minimumSizeHint().width());
                if (QLayout *il = inner->layout()) {
                    innerNeedW = qMax(innerNeedW,
                                      qMax(il->totalSizeHint().width(),
                                           il->totalMinimumSize().width()));
                }
            }
        }
        const int scrollNeededW = innerNeedW > 0 ? (innerNeedW + vSbW + outerMarginsW + 16) : 0;
        if (wrapped) {
            const int desiredW = qMax(window->minimumWidth(),
                                      qMax(outerLayoutW,
                                           qMax(preWrapWidth + vSbW + 16, scrollNeededW)));
            const int targetW = qMin(maxClient.width(), desiredW);
            const int targetH =
                    qMin(maxClient.height(), qMax(window->minimumHeight(), preWrapHeight));
            window->resize(targetW, targetH);
        } else {
            const int desiredW = qMin(
                    maxClient.width(),
                    qMax(window->width(),
                         qMax(window->minimumWidth(), qMax(outerLayoutW, scrollNeededW))));
            const int desiredH = qMin(maxClient.height(), window->height());
            if (desiredW != window->width() || desiredH != window->height())
                window->resize(desiredW, desiredH);
        }
    } else if (window->width() > maxClient.width() || window->height() > maxClient.height()) {
        window->resize(window->size().boundedTo(maxClient));
    }

    // Keep the title bar on screen (only meaningful once the window has a position).
    if (window->isVisible() && !available.isValid()) {
        const QRect g = window->geometry();
        int x = g.x(), y = g.y();
        if (g.right() > avail.right())
            x = avail.right() - g.width() - kFrameAllowanceW / 2;
        if (g.bottom() > avail.bottom())
            y = avail.bottom() - g.height() - kFrameAllowanceH / 2;
        x = qMax(x, avail.left());
        y = qMax(y, avail.top() + kFrameAllowanceH - 8);
        if (x != g.x() || y != g.y())
            window->move(x, y - (window->frameGeometry().height() - g.height() > 0
                                         ? window->frameGeometry().height() - g.height() - 8
                                         : 0));
    }
    return wrapped;
}

void installDialogScreenFit()
{
    static QPointer<DialogScreenFitFilter> s_filter;
    if (s_filter || !qApp)
        return;
    s_filter = new DialogScreenFitFilter(qApp);
    qApp->installEventFilter(s_filter);
}

} // namespace ToriUx

// ===========================================================================
// Empty-table hint (B4)
// ===========================================================================
namespace {

bool columnHasFiniteNumeric(Column *col)
{
    if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric)
        return false;
    const int rows = col->rowCount();
    for (int r = 0; r < rows; ++r) {
        if (!col->isInvalid(r) && std::isfinite(col->valueAt(r)))
            return true;
    }
    return false;
}

bool tableHasAnyData(Table *t)
{
    if (!t)
        return false;
    for (int c = 0; c < t->columnCount(); ++c) {
        Column *col = t->column(c);
        if (!col)
            continue;
        const int rows = col->rowCount();
        for (int r = 0; r < rows; ++r) {
            if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                if (!col->isInvalid(r))
                    return true;
            } else if (!col->textAt(r).isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

// A1-4: Verify that at least one plottable Y column (or single-column target for
// Histogram/Pie/Box) contains at least one finite numeric value so a lone X cell never
// creates a blank graph or shows "Plot this data" prematurely.
bool tableHasPlottableYData(Table *t, int curveType = Graph::LineSymbols)
{
    if (!t)
        return false;
    auto anyNamedColumnHasNumeric = [t](const QStringList &colNames) -> bool {
        for (const QString &name : colNames) {
            const int idx = t->colIndex(name);
            if (idx >= 0 && columnHasFiniteNumeric(t->column(idx)))
                return true;
        }
        return false;
    };
    const bool singleColPlot = (curveType == Graph::Histogram || curveType == Graph::Pie
                                || curveType == Graph::Box);
    if (singleColPlot) {
        QStringList cols = t->selectedColumns();
        if (cols.isEmpty())
            cols = t->YColumns();
        if (!cols.isEmpty())
            return anyNamedColumnHasNumeric(cols);
        return t->numCols() > 0 && columnHasFiniteNumeric(t->column(t->numCols() - 1));
    }
    const QStringList selY = t->selectedYColumns();
    if (!selY.isEmpty())
        return anyNamedColumnHasNumeric(selY);
    for (int c = 0; c < t->columnCount(); ++c) {
        if (t->colPlotDesignation(c) == SciDAVis::Y && columnHasFiniteNumeric(t->column(c)))
            return true;
    }
    return false;
}

QTableView *mainTableView(Table *t)
{
    if (!t)
        return nullptr;
    for (QTableView *v : t->findChildren<QTableView *>()) {
        if (v && v->model() == static_cast<QAbstractItemModel *>(t->tableModel()))
            return v;
    }
    const QList<QTableView *> all = t->findChildren<QTableView *>();
    return all.isEmpty() ? nullptr : all.first();
}

class EmptyTableHint : public QObject
{
public:
    EmptyTableHint(ApplicationWindow *app, Table *table, QTableView *view)
        : QObject(view), d_app(app), d_table(table), d_view(view)
    {
        setObjectName(QStringLiteral("ToriEmptyTableHint"));
        QWidget *vp = view->viewport();
        d_label = new QLabel(vp);
        d_label->setObjectName(QStringLiteral("ToriEmptyTableHintLabel"));
        d_label->setAttribute(Qt::WA_TransparentForMouseEvents);
        d_label->setAlignment(Qt::AlignCenter);
        d_label->setWordWrap(true);
        d_label->setStyleSheet(QStringLiteral(
                "QLabel#ToriEmptyTableHintLabel { color: rgba(71, 85, 105, 165); font-size: 13pt;"
                " background: transparent; }"));

        // Separate non-transparent container for quick-action buttons below the watermark label.
        d_actions_box = new QWidget(vp);
        d_actions_box->setObjectName(QStringLiteral("ToriEmptyTableActions"));
        d_actions_box->setStyleSheet(QStringLiteral(
                "QWidget#ToriEmptyTableActions { background: transparent; }"
                "QPushButton {"
                "  background: #FFFFFF; color: #1E293B; border: 1px solid #CBD5E1;"
                "  border-radius: 6px; padding: 5px 12px; font-size: 10pt; font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "  background: #EFF6FF; color: #0F6CBD; border-color: #2563EB;"
                "}"));
        QHBoxLayout *actRow = new QHBoxLayout(d_actions_box);
        actRow->setContentsMargins(0, 0, 0, 0);
        actRow->setSpacing(8);

        d_sample_btn = new QPushButton(d_actions_box);
        d_sample_btn->setObjectName(QStringLiteral("ToriEmptyTableSampleBtn"));
        d_sample_btn->setIcon(IconLoader::duotoneIcon(QStringLiteral("plot-line-symbol"), 16));
        d_sample_btn->setCursor(Qt::PointingHandCursor);

        d_open_btn = new QPushButton(d_actions_box);
        d_open_btn->setObjectName(QStringLiteral("ToriEmptyTableOpenBtn"));
        d_open_btn->setIcon(IconLoader::duotoneIcon(QStringLiteral("file-open"), 16));
        d_open_btn->setCursor(Qt::PointingHandCursor);

        actRow->addWidget(d_sample_btn);
        actRow->addWidget(d_open_btn);

        QPointer<ApplicationWindow> safeApp(app);
        connect(d_sample_btn, &QPushButton::clicked, this, [safeApp]() {
            if (safeApp)
                CurveMathTools::loadSampleDataset(safeApp.data(),
                                                  QStringLiteral("intro_simple_xy"), true);
        });
        connect(d_open_btn, &QPushButton::clicked, this, [safeApp]() {
            if (safeApp)
                safeApp->open();
        });

        vp->installEventFilter(this);
        if (d_app)
            d_app->installEventFilter(this);
        if (QAbstractItemModel *m = view->model()) {
            connect(m, &QAbstractItemModel::dataChanged, this, &EmptyTableHint::recheck);
            connect(m, &QAbstractItemModel::modelReset, this, &EmptyTableHint::recheck);
            connect(m, &QAbstractItemModel::rowsInserted, this, &EmptyTableHint::recheck);
            connect(m, &QAbstractItemModel::columnsInserted, this, &EmptyTableHint::recheck);
        }
        retranslateUi();
        d_label->show();
        d_actions_box->show();
        reposition();
    }

    ~EmptyTableHint() override
    {
        if (d_view && d_view->viewport())
            d_view->viewport()->removeEventFilter(this);
        if (d_app)
            d_app->removeEventFilter(this);
        if (d_label) {
            d_label->hide();
            d_label->deleteLater();
        }
        if (d_actions_box) {
            d_actions_box->hide();
            d_actions_box->deleteLater();
        }
    }

    QLabel *label() const { return d_label; }

    void retranslateUi()
    {
        if (d_label) {
            d_label->setText(ApplicationWindow::tr(
                    "Paste from Excel here (Ctrl+V)\nor drop an Excel (.xlsx) or CSV file onto "
                    "this window"));
        }
        if (d_sample_btn) {
            d_sample_btn->setText(ApplicationWindow::tr("Insert Simple X-Y Sample"));
            d_sample_btn->setToolTip(ApplicationWindow::tr(
                    "Load a simple 2-series X-Y sample dataset and plot it right away"));
        }
        if (d_open_btn) {
            d_open_btn->setText(ApplicationWindow::tr("Open CSV / Excel..."));
            d_open_btn->setToolTip(
                    ApplicationWindow::tr("Open a CSV, TSV, or Excel (.xlsx) data file"));
        }
        if (d_actions_box)
            d_actions_box->adjustSize();
        reposition();
    }

    void recheck()
    {
        if (d_done)
            return;
        // Coalesce: a paste emits one dataChanged per cell.
        if (d_pending)
            return;
        d_pending = true;
        QTimer::singleShot(0, this, [this]() {
            d_pending = false;
            if (d_done || !d_table)
                return;
            if (!d_hint_hidden && tableHasAnyData(d_table)) {
                d_hint_hidden = true;
                if (d_label)
                    d_label->hide();
                if (d_actions_box)
                    d_actions_box->hide();
            }
            if (tableHasPlottableYData(d_table)) {
                d_done = true;
                if (d_label) {
                    d_label->hide();
                    d_label->deleteLater();
                }
                if (d_actions_box) {
                    d_actions_box->hide();
                    d_actions_box->deleteLater();
                }
                if (d_app && d_table)
                    d_app->offerPlotAfterImport(d_table);
                deleteLater();
            }
        });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!event)
            return QObject::eventFilter(watched, event);
        const QEvent::Type et = event->type();
        if (et == QEvent::LanguageChange) {
            retranslateUi();
        } else if (d_view && watched == d_view->viewport()) {
            if (et == QEvent::Show)
                retranslateUi();
            else if (et == QEvent::Resize)
                reposition();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void reposition()
    {
        if (!d_view || !d_label || d_hint_hidden)
            return;
        const QRect r = d_view->viewport()->rect();
        if (r.width() <= 0 || r.height() <= 0)
            return;
        const int w = qMax(120, qMin(r.width() - 24, 540));
        const int h = 76;
        const int labelTop = qMax(8, r.center().y() - h / 2 - 26);
        d_label->setGeometry(r.center().x() - w / 2, labelTop, w, h);
        if (d_actions_box) {
            const QSize actSz = d_actions_box->sizeHint();
            const int actW = qMin(r.width() - 24, actSz.width());
            const int actH = actSz.height();
            const int actTop = labelTop + h + 6;
            const bool canFit = (r.width() >= 220 && actTop + actH <= r.bottom() - 6);
            d_actions_box->setVisible(canFit);
            if (canFit) {
                d_actions_box->setGeometry(r.center().x() - actW / 2, actTop, actW, actH);
                d_actions_box->raise();
            }
        }
    }

    QPointer<ApplicationWindow> d_app;
    QPointer<Table> d_table;
    QPointer<QTableView> d_view;
    QPointer<QLabel> d_label;
    QPointer<QWidget> d_actions_box;
    QPointer<QPushButton> d_sample_btn;
    QPointer<QPushButton> d_open_btn;
    bool d_hint_hidden = false;
    bool d_done = false;
    bool d_pending = false;
};

class PlotThisDataBarController : public QObject
{
public:
    PlotThisDataBarController(ApplicationWindow *app, QWidget *viewport, QFrame *bar,
                              QPushButton *plotBtn, QToolButton *closeBtn)
        : QObject(bar),
          d_app(app),
          d_viewport(viewport),
          d_bar(bar),
          d_plot_btn(plotBtn),
          d_close_btn(closeBtn)
    {
        if (d_viewport)
            d_viewport->installEventFilter(this);
        if (d_bar)
            d_bar->installEventFilter(this);
        if (d_app)
            d_app->installEventFilter(this);
        retranslateUi();
    }

    ~PlotThisDataBarController() override
    {
        if (d_viewport)
            d_viewport->removeEventFilter(this);
        if (d_bar)
            d_bar->removeEventFilter(this);
        if (d_app)
            d_app->removeEventFilter(this);
    }

    void retranslateUi()
    {
        if (d_plot_btn) {
            d_plot_btn->setText(ApplicationWindow::tr("Plot this data"));
            d_plot_btn->setToolTip(ApplicationWindow::tr(
                    "Line + symbol graph of all Y columns (use the Plot tab for other styles)"));
        }
        if (d_close_btn)
            d_close_btn->setToolTip(ApplicationWindow::tr("Dismiss"));
        if (d_bar)
            d_bar->adjustSize();
        reposition();
    }

    void reposition()
    {
        if (!d_viewport || !d_bar)
            return;
        const QRect vr = d_viewport->rect();
        if (vr.width() <= 0 || vr.height() <= 0)
            return;
        const int margin = 10;
        const int availW = qMax(80, vr.width() - 2 * margin);
        const QSize hint = d_bar->sizeHint();
        const int w = qMin(hint.width(), availW);
        const int h = qMin(qMax(28, hint.height()), qMax(28, vr.height() - 2 * margin));
        const int x = qMax(4, vr.right() - w - margin + 1);
        const int y = qMax(4, vr.bottom() - h - margin + 1);
        d_bar->setGeometry(x, y, w, h);
        d_bar->raise();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!event)
            return QObject::eventFilter(watched, event);
        const QEvent::Type et = event->type();
        if (et == QEvent::LanguageChange) {
            retranslateUi();
        } else if (watched == d_viewport
                   && (et == QEvent::Resize || et == QEvent::Show || et == QEvent::LayoutRequest)) {
            if (et == QEvent::Show)
                retranslateUi();
            else
                reposition();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<ApplicationWindow> d_app;
    QPointer<QWidget> d_viewport;
    QPointer<QFrame> d_bar;
    QPointer<QPushButton> d_plot_btn;
    QPointer<QToolButton> d_close_btn;
};

} // namespace

// ===========================================================================
// ApplicationWindow members
// ===========================================================================

QUrl ApplicationWindow::quickGuideUrl() const
{
    const QString local = ToriUx::quickGuideFilePath();
    if (!local.isEmpty())
        return QUrl::fromLocalFile(local);
    if (!helpFilePath.isEmpty()) {
        const QFileInfo fi(helpFilePath);
        if (fi.isFile() && fi.size() > 256)
            return QUrl::fromLocalFile(fi.absoluteFilePath());
    }
    return QUrl(QStringLiteral(MANUAL_URI_JA));
}

QString ApplicationWindow::currentToolModeName() const
{
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (!g)
        return QString();
    if (g->zoomOn())
        return (btnZoomOut && btnZoomOut->isChecked()) ? tr("Zoom Out") : tr("Zoom In");
    if (g->drawLineActive())
        return (btnArrow && btnArrow->isChecked()) ? tr("Draw Arrow") : tr("Draw Line");
    if (g->drawTextActive())
        return tr("Add Text (click on the graph)");
    if (PlotToolInterface *tool = g->activeTool()) {
        switch (tool->rtti()) {
        case PlotToolInterface::DataPicker:
            if (btnMovePoints && btnMovePoints->isChecked())
                return tr("Move Data Points");
            if (btnRemovePoints && btnRemovePoints->isChecked())
                return tr("Remove Data Points");
            if (btnMaskPoints && btnMaskPoints->isChecked())
                return tr("Mask Data Points");
            if (btnUnmaskPoints && btnUnmaskPoints->isChecked())
                return tr("Unmask Data Points");
            return tr("Data Reader");
        case PlotToolInterface::ScreenPicker:
            return tr("Screen Reader");
        case PlotToolInterface::LineProfile:
            return tr("Line Profile");
        case PlotToolInterface::MultiPeak:
            return tr("Multi-Peak Fit (click the peaks)");
        case PlotToolInterface::RangeSelector:
            return tr("Select Data Range");
        case PlotToolInterface::TranslateCurve:
            return tr("Translate Curve");
        case PlotToolInterface::RoiGadget:
            return tr("ROI Gadget");
        case PlotToolInterface::VerticalCursor:
            return tr("Vertical Cursor");
        case PlotToolInterface::DeltaCursor:
            return tr("Delta Cursor");
        case PlotToolInterface::ImageProfileGadget:
            return tr("Image Profile");
        }
    }
    if (g->rangeSelectorsEnabled())
        return tr("Select Data Range");
    return QString();
}

void ApplicationWindow::refreshToolModeIndicator()
{
    const QString mode = currentToolModeName();

    // Keep the checkable tool actions (classic toolbar, Tools menu, ribbon buttons) truthful.
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml ? ml->activeGraph() : nullptr;
    QAction *want = nullptr;
    bool noActionForMode = false;
    if (!g || mode.isEmpty()) {
        want = btnPointer;
    } else if (g->zoomOn()) {
        want = (btnZoomOut && btnZoomOut->isChecked()) ? btnZoomOut : btnZoomIn;
    } else if (g->drawLineActive()) {
        want = (btnArrow && btnArrow->isChecked()) ? btnArrow : btnLine;
    } else if (PlotToolInterface *tool = g->activeTool()) {
        if (tool->rtti() == PlotToolInterface::DataPicker) {
            QAction *checked = dataTools ? dataTools->checkedAction() : nullptr;
            const bool pickerAction = checked
                    && (checked == btnCursor || checked == btnMovePoints
                        || checked == btnRemovePoints || checked == btnMaskPoints
                        || checked == btnUnmaskPoints);
            want = pickerAction ? checked : btnCursor;
        } else if (tool->rtti() == PlotToolInterface::ScreenPicker) {
            want = btnPicker;
        } else if (tool->rtti() == PlotToolInterface::RangeSelector) {
            want = btnSelect;
        } else {
            noActionForMode = true; // gadgets, multi-peak, profiles: no toolbar action
        }
    } else if (g->rangeSelectorsEnabled()) {
        want = btnSelect;
    } else {
        noActionForMode = true; // text placement
    }
    if (want && !want->isChecked())
        want->setChecked(true);
    if (noActionForMode && dataTools) {
        if (QAction *c = dataTools->checkedAction())
            c->setChecked(false);
    }
    if (actionAddText) {
        const bool textOn = g && g->drawTextActive();
        if (actionAddText->isChecked() != textOn)
            actionAddText->setChecked(textOn);
    }

    if (!d_tool_mode_indicator)
        return;
    const QString text = mode.isEmpty() ? QString() : tr("Mode: %1 (Esc to cancel)").arg(mode);
    if (text == d_last_tool_mode_text)
        return;
    d_last_tool_mode_text = text;
    d_tool_mode_indicator->setText(text);
    d_tool_mode_indicator->setToolTip(tr("Click (or press Esc) to return to the pointer"));
    d_tool_mode_indicator->setVisible(!text.isEmpty());
}

void ApplicationWindow::cancelCurrentToolMode()
{
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    Graph *g = ml ? ml->activeGraph() : nullptr;
    if (g) {
        // Same path as Esc on the canvas.
        for (int guard = 0; guard < 3 && !currentToolModeName().isEmpty(); ++guard)
            g->handleEscapeKey();
    }
    if (actionAddText && actionAddText->isChecked())
        actionAddText->setChecked(false);
    if (btnPointer && !btnPointer->isChecked())
        btnPointer->setChecked(true);
    refreshToolModeIndicator();
}

QString ApplicationWindow::plotBlockerForTable(Table *table, int curveType) const
{
    if (!table)
        return tr("The plot buttons make a graph from worksheet columns. Click a worksheet (or "
                  "paste data from Excel) first.");
    if (!tableHasAnyData(table)) {
        return tr("The worksheet \"%1\" is empty, so no graph was created.").arg(table->name())
                + QStringLiteral("<p>")
                + tr("Copy a range in Excel and paste it into the worksheet with Ctrl+V, or drop "
                     "an Excel (.xlsx) or CSV file onto the window. Then press the plot button "
                     "again.");
    }
    if (!tableHasPlottableYData(table, curveType)) {
        return tr("No numeric data in Y column of \"%1\".").arg(table->name())
                + QStringLiteral("<p>")
                + tr("Enter numeric values in column B(Y), or paste both X and Y columns from "
                     "Excel (Ctrl+V).");
    }
    return QString();
}

MultiLayer *ApplicationWindow::plotColumnsFromTable(Table *table, int type)
{
    if (!table)
        return nullptr;
    const QString blocker = plotBlockerForTable(table, type);
    if (!blocker.isEmpty()) {
        if (!isNonInteractiveContext(this))
            QMessageBox::information(this, tr("Nothing to plot yet"), blocker);
        else
            statusBar()->showMessage(QString(blocker).replace(QStringLiteral("<p>"),
                                                              QStringLiteral(" ")),
                                     8000);
        return nullptr;
    }
    if (!validFor2DPlot(table, type))
        return nullptr;

    const int startRow = (table->selectedRowCount(false) >= 2) ? table->firstSelectedRow(false) : 0;
    const int endRow = (table->selectedRowCount(false) >= 2) ? table->lastSelectedRow(false) : -1;

    QStringList cols;
    switch (type) {
    case Graph::Histogram:
    case Graph::Pie:
    case Graph::Box:
        cols = table->selectedColumns();
        if (cols.isEmpty())
            cols = table->YColumns();
        if (cols.isEmpty() && table->numCols() > 0)
            cols << table->colName(table->numCols() - 1);
        break;
    default:
        if (table->selectedColumnCount(SciDAVis::Y) >= 1) {
            cols = table->drawableColumnSelection();
        } else {
            for (int i = 0; i < table->numCols(); ++i) {
                const SciDAVis::PlotDesignation pd = table->colPlotDesignation(i);
                if (pd == SciDAVis::Y || pd == SciDAVis::yErr || pd == SciDAVis::xErr)
                    cols << table->colName(i);
            }
        }
        break;
    }
    return multilayerPlot(table, cols, (Graph::CurveType)type, startRow, endRow);
}

void ApplicationWindow::showUxHint(const QString &text, int timeoutMs)
{
    QString plain = text;
    plain.replace(QStringLiteral("<p>"), QStringLiteral(" "));
    statusBar()->showMessage(plain, timeoutMs);
    if (!isNonInteractiveContext(this))
        QToolTip::showText(QCursor::pos(), text, this, QRect(), timeoutMs);
}

QStringList ApplicationWindow::advancedDomainActionNames()
{
    // G1: features of other fields (medicine, electrochemistry, optics, generic signal
    // processing, business charts). Hidden by default, never deleted.
    return { QStringLiteral("actionROCCurve"),         QStringLiteral("actionKaplanMeier"),
             QStringLiteral("actionBlandAltman"),      QStringLiteral("actionPassingBablokDeming"),
             QStringLiteral("actionLogisticRegression"), QStringLiteral("actionEISAnalyzer"),
             QStringLiteral("actionTaucPlot"),         QStringLiteral("actionWaveletTransform"),
             QStringLiteral("actionStftSpectrogram"),  QStringLiteral("actionHilbertEnvelope"),
             QStringLiteral("actionPlotPie"),          QStringLiteral("actionPlotPareto"),
             QStringLiteral("actionPlot3DRibbon") };
}

void ApplicationWindow::setShowAdvancedDomainFeatures(bool show)
{
    d_show_advanced_domain_features = show;
    if (!d_headless_mode && !m_batch) {
        // getSettings() honours TORIGRAPH_SETTINGS_INI (tests never touch the user registry)
        getSettings().setValue(QStringLiteral("/UX/ShowAdvancedDomainFeatures"), show);
    }
    applyAdvancedFeatureVisibility();
}

void ApplicationWindow::applyAdvancedFeatureVisibility()
{
    const bool show = d_show_advanced_domain_features;
    for (const QString &name : advancedDomainActionNames()) {
        if (QAction *a = findChild<QAction *>(name)) {
            a->setProperty("_tg_advanced_domain_action", true);
            a->setVisible(show);
        }
    }
    if (QAction *toggle = findChild<QAction *>(QStringLiteral("actionShowAdvancedDomainFeatures"))) {
        if (toggle->isChecked() != show) {
            QSignalBlocker b(toggle);
            toggle->setChecked(show);
        }
    }
    if (d_modern_ribbon)
        d_modern_ribbon->setAdvancedDomainFeaturesVisible(show);
}

namespace {

// Shortcut of a legacy action while it is hidden (PortableText), restored when shown again.
const char kLegacyHiddenShortcutProp[] = "_tg_legacy_hidden_shortcut";

void registerLegacyUiTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            const QString src = QString::fromUtf8(en);
            if (!ToriGraphFullJapaneseTranslator::lookupJapanese(src).isEmpty())
                return;
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(src, QString::fromUtf8(ja));
        };
        reg("Show Legacy UI", "旧 UI を表示");
        reg("Show the old toolbar menus, Plot Wizard, Add Time Stamp and Scripting Language in "
            "menus, the ribbon and command search",
            "旧ツールバーのメニュー、プロットウィザード、タイムスタンプの追加、スクリプト言語の選択を"
            "メニュー・リボン・コマンド検索に表示します");
    });
}

// Hide the shortcut of a hidden legacy action so a stray key press cannot run an invisible
// command. Re-applied whenever the action changes (translateActionsStrings() sets it again).
void suppressLegacyShortcut(QAction *a)
{
    if (!a || a->shortcut().isEmpty())
        return;
    a->setProperty(kLegacyHiddenShortcutProp,
                   a->shortcut().toString(QKeySequence::PortableText));
    a->setShortcut(QKeySequence());
}

} // namespace

QStringList ApplicationWindow::legacyUiActionNames()
{
    // C15: roles taken over by the ribbon (classic toolbars), the plot buttons (Plot Wizard),
    // the text tool (time stamp) and Python (scripting language). Hidden, never deleted.
    return { QStringLiteral("actionShowPlotWizard"), QStringLiteral("actionTimeStamp"),
             QStringLiteral("actionScriptingLang"), QStringLiteral("locktoolbar") };
}

void ApplicationWindow::setShowLegacyUi(bool show)
{
    d_show_legacy_ui = show;
    if (!d_headless_mode && !m_batch) {
        // getSettings() honours TORIGRAPH_SETTINGS_INI (tests never touch the user registry)
        getSettings().setValue(QStringLiteral("/UX/ShowLegacyUi"), show);
    }
    applyLegacyUiVisibility();
}

void ApplicationWindow::applyLegacyUiVisibility()
{
    const bool show = d_show_legacy_ui;
    for (const QString &name : legacyUiActionNames()) {
        QAction *a = findChild<QAction *>(name);
        if (!a)
            continue;
        a->setVisible(show);
        if (!show) {
            suppressLegacyShortcut(a);
        } else {
            const QString saved = a->property(kLegacyHiddenShortcutProp).toString();
            a->setProperty(kLegacyHiddenShortcutProp, QVariant());
            if (!saved.isEmpty() && a->shortcut().isEmpty())
                a->setShortcut(QKeySequence(saved, QKeySequence::PortableText));
        }
    }
    // View > Toolbars and the show/hide switches of the classic toolbars (they also appear in
    // the main window's right-click menu and in command search)
    if (toolbarsMenu)
        toolbarsMenu->menuAction()->setVisible(show);
    QToolBar *const legacyBars[] = { file_tools, edit_tools,        graph_tools,   plot_tools,
                                     table_tools, matrix_plot_tools, graph_3D_tools };
    for (QToolBar *tb : legacyBars) {
        if (tb && tb->toggleViewAction())
            tb->toggleViewAction()->setVisible(show);
    }
    if (QAction *toggle = findChild<QAction *>(QStringLiteral("actionShowLegacyUi"))) {
        if (toggle->isChecked() != show) {
            QSignalBlocker b(toggle);
            toggle->setChecked(show);
        }
    }
    if (d_modern_ribbon)
        d_modern_ribbon->setLegacyUiVisible(show);
}

void ApplicationWindow::installLegacyUiSwitch()
{
    registerLegacyUiTranslations();
    d_show_legacy_ui = getSettings().value(QStringLiteral("/UX/ShowLegacyUi"), false).toBool();
    if (toolbarsMenu && toolbarsMenu->objectName().isEmpty())
        toolbarsMenu->setObjectName(QStringLiteral("menuToolbars"));
    for (const QString &name : legacyUiActionNames()) {
        if (QAction *a = findChild<QAction *>(name)) {
            connect(a, &QAction::changed, this, [this, a]() {
                if (!d_show_legacy_ui)
                    suppressLegacyShortcut(a);
            });
        }
    }

    QAction *legacyToggle = new QAction(this);
    legacyToggle->setObjectName(QStringLiteral("actionShowLegacyUi"));
    legacyToggle->setCheckable(true);
    legacyToggle->setChecked(d_show_legacy_ui);
    connect(legacyToggle, &QAction::toggled, this, [this](bool on) { setShowLegacyUi(on); });
    if (view)
        view->addAction(legacyToggle); // next to the G1 switch at the end of the View menu
    retranslateUxSwitches();
    applyLegacyUiVisibility();
}

void ApplicationWindow::retranslateUxSwitches()
{
    if (QAction *adv = findChild<QAction *>(QStringLiteral("actionShowAdvancedDomainFeatures"))) {
        adv->setText(tr("Show Other-Field / Advanced Features"));
        adv->setToolTip(tr("Show medical statistics, EIS, Tauc, wavelet/STFT/Hilbert, pie/Pareto "
                           "and 3D ribbon in menus, the ribbon and command search"));
    }
    if (QAction *legacy = findChild<QAction *>(QStringLiteral("actionShowLegacyUi"))) {
        legacy->setText(tr("Show Legacy UI"));
        legacy->setToolTip(tr("Show the old toolbar menus, Plot Wizard, Add Time Stamp and "
                              "Scripting Language in menus, the ribbon and command search"));
    }
}

void ApplicationWindow::attachEmptyTableHint(Table *table)
{
    if (!table || d_headless_mode || m_batch || tableHasAnyData(table))
        return;
    QTableView *view = mainTableView(table);
    if (!view || view->findChild<QObject *>(QStringLiteral("ToriEmptyTableHint")))
        return;
    new EmptyTableHint(this, table, view);
}

void ApplicationWindow::offerPlotAfterImport(Table *table)
{
    if (!table || d_headless_mode || m_batch || !tableHasPlottableYData(table))
        return;
    if (table->findChild<QFrame *>(QStringLiteral("ToriPlotThisDataBar")))
        return;
    QTableView *view = mainTableView(table);
    if (!view)
        return;

    QFrame *bar = new QFrame(view->viewport());
    bar->setObjectName(QStringLiteral("ToriPlotThisDataBar"));
    bar->setStyleSheet(QStringLiteral(
            "QFrame#ToriPlotThisDataBar { background: #EFF6FF; border: 1px solid #93C5FD;"
            " border-radius: 6px; }"
            "QPushButton { padding: 3px 12px; }"));
    QHBoxLayout *row = new QHBoxLayout(bar);
    row->setContentsMargins(8, 4, 4, 4);
    row->setSpacing(6);
    QPushButton *plotBtn = new QPushButton(tr("Plot this data"), bar);
    plotBtn->setObjectName(QStringLiteral("ToriPlotThisDataButton"));
    plotBtn->setToolTip(tr("Line + symbol graph of all Y columns (use the Plot tab for other styles)"));
    QToolButton *close = new QToolButton(bar);
    close->setText(QStringLiteral("×"));
    close->setToolTip(tr("Dismiss"));
    close->setAutoRaise(true);
    row->addWidget(plotBtn);
    row->addWidget(close);
    bar->adjustSize();
    auto *posCtrl = new PlotThisDataBarController(this, view->viewport(), bar, plotBtn, close);
    bar->show();
    posCtrl->reposition();

    QPointer<Table> safeTable(table);
    QPointer<QFrame> safeBar(bar);
    connect(plotBtn, &QPushButton::clicked, this, [this, safeTable, safeBar]() {
        if (safeBar)
            safeBar->deleteLater();
        if (!safeTable)
            return;
        plotColumnsFromTable(safeTable, Graph::LineSymbols);
    });
    connect(close, &QToolButton::clicked, bar, &QObject::deleteLater);
}

void ApplicationWindow::buildGraphContextMenu(QMenu &cm, MultiLayer *plot, bool fromLayerButton)
{
    if (!plot)
        return;
    Graph *ag = plot->activeGraph();
    QPointer<MultiLayer> safeMl(plot);

    if (fromLayerButton) {
        cm.addAction(actionAddLayer);
        cm.addAction(actionDeleteLayer);
        cm.addSeparator();
    }

    if (ag && ag->isPiePlot()) {
        cm.addAction(tr("Re&move Pie Curve"), ag, SLOT(removePie()));
    } else if (ag) {
        QPointer<Graph> safeG(ag);

        // 1. Selection-first top actions (PlotTitle, ReferenceLine, Axis, Curve, Legend/Annotation).
        const Graph::GraphSelection sel = ag->currentSelection();
        const bool plotTitleSel =
                (sel.kind == Graph::GraphSelection::PlotTitle) || ag->titleSelected();
        const bool refLineSel = (sel.kind == Graph::GraphSelection::ReferenceLine
                                 && sel.primaryId >= 0
                                 && sel.primaryId < ag->referenceLines().size());
        const bool axisSel = (sel.kind == Graph::GraphSelection::Axis
                              || sel.kind == Graph::GraphSelection::AxisTitle)
                && sel.primaryId >= 0 && sel.primaryId < QwtPlot::axisCnt;
        const bool curveSel = (sel.kind == Graph::GraphSelection::Curve
                               || sel.kind == Graph::GraphSelection::ErrorBars
                               || sel.kind == Graph::GraphSelection::Spectrogram)
                && sel.primaryId >= 0 && sel.primaryId < ag->curves();
        const bool markerSel = ag->markerSelected() && !plotTitleSel && !refLineSel;

        if (plotTitleSel) {
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("text-label"), 16), tr("Edit Title"),
                         this, [this, safeG]() {
                             if (!safeG)
                                 return;
                             Plot *p = safeG->plotWidget();
                             if (p && p->titleLabel()) {
                                 QwtTextLabel *tl = p->titleLabel();
                                 QRect r = tl->textRect();
                                 if (!r.isValid() || r.isEmpty())
                                     r = tl->rect();
                                 const QRect globalR(tl->mapToGlobal(r.topLeft()), r.size());
                                 CanvasInlineTextEditor::openForPlotTitle(safeG.data(), globalR);
                             } else {
                                 showTitleDialog();
                             }
                         });
            cm.addAction(tr("Title Properties..."), this, SLOT(showTitleDialog()));
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("delete"), 16), tr("Delete Title"),
                         this, [this, safeG, safeMl]() {
                             if (!safeG || !safeMl)
                                 return;
                             const QString before = safeMl->captureUndoSnapshot();
                             safeG->removeTitle();
                             safeG->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
                             safeMl->pushUndoSnapshot(tr("Delete Title"), before);
                             emit modified();
                             if (d_graph_inspector_dock)
                                 d_graph_inspector_dock->refreshAll();
                         });
            cm.addSeparator();
        } else if (refLineSel) {
            const int refIdx = sel.primaryId;
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("mean-line"), 16),
                         tr("Edit Reference Line..."), this, [this, safeG, refIdx]() {
                             if (!safeG)
                                 return;
                             if (safeG->editReferenceLineDialog(refIdx) && d_graph_inspector_dock)
                                 d_graph_inspector_dock->refreshAll();
                         });
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("delete"), 16),
                         tr("Delete Reference Line"), this, [this, safeG, safeMl, refIdx]() {
                             if (!safeG || !safeMl)
                                 return;
                             if (refIdx < 0 || refIdx >= safeG->referenceLines().size())
                                 return;
                             const QString before = safeMl->captureUndoSnapshot();
                             safeG->removeReferenceLine(refIdx);
                             safeG->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
                             safeG->replot();
                             safeG->notifyChanges();
                             safeMl->pushUndoSnapshot(tr("Delete Reference Line"), before);
                             emit modified();
                             if (d_graph_inspector_dock)
                                 d_graph_inspector_dock->refreshAll();
                         });
            cm.addSeparator();
        } else if (axisSel) {
            const int qwtAxis = sel.primaryId;
            if (sel.kind == Graph::GraphSelection::AxisTitle) {
                cm.addAction(IconLoader::duotoneIcon(QStringLiteral("text-label"), 16),
                             tr("Edit Axis Title"), this, [safeG, qwtAxis]() {
                                 if (!safeG || !safeG->plotWidget())
                                     return;
                                 QWidget *aw = safeG->plotWidget()->axisWidget(qwtAxis);
                                 QRect anchor;
                                 if (aw)
                                     anchor = QRect(aw->mapToGlobal(QPoint(0, 0)), aw->size());
                                 else
                                     anchor = QRect(QCursor::pos(), QSize(120, 28));
                                 CanvasInlineTextEditor::openForAxisTitle(safeG.data(), qwtAxis,
                                                                          anchor);
                             });
            }
            // P0 (C2-1): Always use AxesDialog::setCurrentQwtAxis() for QwtPlot::Axis ids.
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("axis-scale"), 16),
                         tr("Edit Axis..."), this, [this, qwtAxis]() {
                             openAxesDialogConfigured([qwtAxis](AxesDialog *gd) {
                                 gd->showAxesPage();
                                 gd->setCurrentQwtAxis(qwtAxis);
                             });
                         });
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("zoom-auto"), 16),
                         tr("Rescale This Axis"), this, [safeG, safeMl, qwtAxis]() {
                             if (!safeG || !safeMl)
                                 return;
                             const QString before = safeMl->captureUndoSnapshot();
                             safeG->setAxisUserLocked(qwtAxis, false);
                             safeG->setAutoScaleAxis(qwtAxis);
                             safeG->replot();
                             safeG->notifyChanges();
                             safeMl->pushUndoSnapshot(tr("Rescale This Axis"), before);
                         });
            cm.addSeparator();
        } else if (curveSel) {
            const int primary = sel.primaryId;
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("settings"), 16),
                         tr("Properties of the Selection..."), this, [this, safeG, primary]() {
                             if (!safeG || primary < 0 || primary >= safeG->curves())
                                 return;
                             showPlotDialog(safeG->curveKey(primary));
                         });
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("delete"), 16),
                         tr("Delete Selection"), this, [this, safeG, primary]() {
                             if (!safeG || primary < 0 || primary >= safeG->curves())
                                 return;
                             actionRemoveCurve->setData(int(safeG->curveKey(primary)));
                             removeCurve();
                         });
            cm.addSeparator();
        } else if (markerSel) {
            const long mKey = ag->selectedMarkerKey();
            if (Legend *leg = dynamic_cast<Legend *>(ag->selectedMarkerPtr())) {
                const bool isMainLeg = (ag->legend() == leg);
                cm.addAction(IconLoader::duotoneIcon(QStringLiteral("text-label"), 16),
                             tr("Edit Text"), this, [safeG, mKey, isMainLeg]() {
                                 if (!safeG || !safeG->plotWidget())
                                     return;
                                 Legend *curLeg =
                                         dynamic_cast<Legend *>(safeG->plotWidget()->marker(mKey));
                                 if (!curLeg || !safeG->plotWidget()->canvas())
                                     return;
                                 const QRect lr = curLeg->rect();
                                 const QRect globalR(
                                         safeG->plotWidget()->canvas()->mapToGlobal(lr.topLeft()),
                                         lr.size());
                                 CanvasInlineTextEditor::openForLegendOrText(
                                         safeG.data(), mKey, isMainLeg ? 0 : -1, globalR);
                             });
            }
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("settings"), 16),
                         tr("Properties of the Selection..."), this, [this, safeG]() {
                             if (!safeG)
                                 return;
                             if (safeG->arrow(safeG->selectedMarkerKey()))
                                 showLineDialog();
                             else if (safeG->imageMarker(safeG->selectedMarkerKey()))
                                 showImageDialog();
                             else
                                 showTextDialog();
                         });
            cm.addAction(IconLoader::duotoneIcon(QStringLiteral("delete"), 16),
                         tr("Delete Selection"), this, [this, safeG, safeMl]() {
                             if (!safeG || !safeMl)
                                 return;
                             const QString before = safeMl->captureUndoSnapshot();
                             safeG->removeMarker();
                             safeG->setCurrentSelection({ Graph::GraphSelection::None, -1, -1 });
                             safeMl->pushUndoSnapshot(tr("Delete Annotation"), before);
                             emit modified();
                         });
            cm.addSeparator();
        }

        // 2. Clean, focused canvas actions (B4-5: no bloated 9 nested analysis submenus).
        cm.addAction(actionUnzoom);
        cm.addAction(actionAddText);
        cm.addAction(actionNewLegend);
        cm.addAction(IconLoader::duotoneIcon(QStringLiteral("mean-line"), 16),
                     tr("Add Reference Line..."), this, [safeG]() {
                         if (safeG)
                             GraphInspectorDock::addReferenceLineInteractive(safeG.data(), 0);
                     });
        if (ag->visibleCurves() != ag->curves())
            cm.addAction(actionShowAllCurves);
        cm.addSeparator();

        // 3. Style Presets (Lab Slide 18pt via lab_slide_83mm, ISIJ 85mm, 2-col & user presets;
        //    never promote legacy 185 mm slide_28pt as a 1-click slide preset).
        QMenu *presetsMenu = cm.addMenu(IconLoader::duotoneIcon(QStringLiteral("preset-slide"), 16),
                                        tr("&Style Presets"));
        auto addPresetItem = [this, presetsMenu, safeMl](const QString &presetId,
                                                         const QString &label,
                                                         const QString &iconId) {
            presetsMenu->addAction(IconLoader::duotoneIcon(iconId, 16), label, this,
                                   [this, safeMl, presetId]() {
                                       if (!safeMl)
                                           return;
                                       safeMl->applyStylePresetById(presetId);
                                       modifiedProject();
                                       if (d_graph_inspector_dock)
                                           d_graph_inspector_dock->refreshAll();
                                   });
        };
        addPresetItem(QStringLiteral("lab_slide_83mm"), tr("Lab Slide (18pt)"),
                      QStringLiteral("preset-slide"));
        addPresetItem(QStringLiteral("isij_85mm"), tr("ISIJ 85mm"), QStringLiteral("preset-isij"));
        const QVector<MultiLayer::GraphStylePresetSpec> allPresets = MultiLayer::listStylePresets();
        for (const auto &sp : allPresets) {
            if (sp.id.compare(QLatin1String("journal_2col_175mm"), Qt::CaseInsensitive) == 0) {
                addPresetItem(sp.id, tr("Journal 2-Column (175mm)"), QStringLiteral("preset-isij"));
                break;
            }
        }
        bool addedUserSep = false;
        for (const auto &sp : allPresets) {
            if (sp.builtIn)
                continue;
            if (!addedUserSep) {
                presetsMenu->addSeparator();
                addedUserSep = true;
            }
            addPresetItem(sp.id, sp.name, QStringLiteral("page-setup"));
        }
        presetsMenu->addSeparator();
        presetsMenu->addAction(IconLoader::duotoneIcon(QStringLiteral("page-setup"), 16),
                               tr("Manage Style Presets..."), this,
                               SLOT(showStylePresetManagerDialog()));
        cm.addSeparator();

        // 4. PowerPoint copy & format copy/paste.
        cm.addAction(actionCopyGraphOle);
        cm.addAction(actionCopyGraphFormat);
        cm.addAction(actionPasteGraphFormat);
        cm.addSeparator();

        // 5. Axis properties & export.
        cm.addAction(actionShowAxisDialog);
    }

    if (copiedLayer) {
        cm.addSeparator();
        cm.addAction(IconLoader::load("paste"), tr("&Paste Layer"), this,
                     SLOT(pasteSelection()));
    } else if (copiedMarkerType >= 0) {
        cm.addSeparator();
        if (copiedMarkerType == Graph::Text)
            cm.addAction(IconLoader::load("paste"), tr("&Paste Text"), plot,
                         SIGNAL(pasteMarker()));
        else if (copiedMarkerType == Graph::Arrow)
            cm.addAction(IconLoader::load("paste"), tr("&Paste Line/Arrow"), plot,
                         SIGNAL(pasteMarker()));
        else if (copiedMarkerType == Graph::Image)
            cm.addAction(IconLoader::load("paste"), tr("&Paste Image"), plot,
                         SIGNAL(pasteMarker()));
    }

    cm.addAction(IconLoader::duotoneIcon(QStringLiteral("export-image"), 16),
                 tr("Export Image / PDF..."), this, SLOT(exportGraph()));
    if (plot->layers() > 1) {
        cm.addAction(IconLoader::duotoneIcon(QStringLiteral("export-image"), 16),
                     tr("Export Active Layer..."), this, SLOT(exportLayer()));
    }
    cm.addSeparator();
    cm.addAction(tr("Layer P&roperties..."), this, SLOT(showGeneralPlotDialog()));
    if (!fromLayerButton) {
        cm.addAction(IconLoader::duotoneIcon(QStringLiteral("delete"), 16), tr("&Delete Layer"),
                     plot, SLOT(confirmRemoveLayer()));
    }
}

void ApplicationWindow::installUxNavigation()
{
    ToriUx::installDialogScreenFit();

    // C3: permanent "Mode: ... (Esc to cancel)" indicator.
    d_tool_mode_indicator = new QToolButton(statusBar());
    d_tool_mode_indicator->setObjectName(QStringLiteral("StatusBarToolModeIndicator"));
    d_tool_mode_indicator->setAutoRaise(false);
    d_tool_mode_indicator->setCursor(Qt::PointingHandCursor);
    d_tool_mode_indicator->setFocusPolicy(Qt::NoFocus);
    d_tool_mode_indicator->setStyleSheet(QStringLiteral(
            "QToolButton#StatusBarToolModeIndicator {"
            "  background-color: #FEF3C7; color: #92400E; border: 1px solid #F59E0B;"
            "  border-radius: 9px; padding: 1px 10px; margin-right: 4px; font-weight: 600;"
            "}"
            "QToolButton#StatusBarToolModeIndicator:hover { background-color: #FDE68A; }"));
    d_tool_mode_indicator->hide();
    connect(d_tool_mode_indicator, &QToolButton::clicked, this,
            &ApplicationWindow::cancelCurrentToolMode);
    statusBar()->insertPermanentWidget(0, d_tool_mode_indicator);

    if (dataTools) {
        connect(dataTools, &QActionGroup::triggered, this, [this](QAction *) {
            QTimer::singleShot(0, this, [this]() { refreshToolModeIndicator(); });
        });
    }
    connect(&d_workspace, &QMdiArea::subWindowActivated, this,
            [this](QMdiSubWindow *) { refreshToolModeIndicator(); });
    // Tools and gadgets are created and destroyed in many places without a common signal;
    // a light poll keeps the indicator honest.
    d_tool_mode_poll = new QTimer(this);
    d_tool_mode_poll->setInterval(300);
    connect(d_tool_mode_poll, &QTimer::timeout, this, &ApplicationWindow::refreshToolModeIndicator);
    d_tool_mode_poll->start();

    // G1: other-field / advanced features (hidden by default).
    {
        d_show_advanced_domain_features =
                getSettings().value(QStringLiteral("/UX/ShowAdvancedDomainFeatures"), false).toBool();
    }
    QAction *advToggle = new QAction(tr("Show Other-Field / Advanced Features"), this);
    advToggle->setObjectName(QStringLiteral("actionShowAdvancedDomainFeatures"));
    advToggle->setCheckable(true);
    advToggle->setChecked(d_show_advanced_domain_features);
    advToggle->setToolTip(tr("Show medical statistics, EIS, Tauc, wavelet/STFT/Hilbert, pie/Pareto "
                             "and 3D ribbon in menus, the ribbon and command search"));
    connect(advToggle, &QAction::toggled, this,
            [this](bool on) { setShowAdvancedDomainFeatures(on); });
    if (view) {
        view->addSeparator();
        view->addAction(advToggle);
    }
    applyAdvancedFeatureVisibility();

    // C15: legacy UI (hidden by default)
    installLegacyUiSwitch();
}
