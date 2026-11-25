/***************************************************************************
        File                 : ApplicationWindow_SettingsAndStyles.cpp
        Project              : SciDAVis
        Description          : SciDAVis's main window (settings, preferences, fonts, i18n, style presets, graph format & batch workflows)
    --------------------------------------------------------------------
    Copyright            : (C) 2006-2009 Knut Franke (knut.franke*gmx.de)
    Copyright            : (C) 2006-2009 Tilman Benkert (thzs*gmx.net)
    Copyright            : (C) 2004-2007 by Ion Vasilief (ion_vasilief*yahoo.fr)
                           (replace * with @ in the email address)

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

#include "globals.h"
#include "ApplicationWindow.h"
#include "CurvesDialog.h"
#include "PlotDialog.h"
#include "AxesDialog.h"
#include "LineDialog.h"
#include "TextDialog.h"
#include "ExportDialog.h"
#include "ErrDialog.h"
#include "Legend.h"
#include "ArrowMarker.h"
#include "ImageMarker.h"
#include "Graph.h"
#include "Plot.h"
#include "Grid.h"
#include "PlotWizard.h"
#include "PolynomFitDialog.h"
#include "ExpDecayDialog.h"
#include "FunctionDialog.h"
#include "FitDialog.h"
#include "SurfaceDialog.h"
#include "Graph3D.h"
#include "Plot3DDialog.h"
#include "ImageDialog.h"
#include "MultiLayer.h"
#include "LayerDialog.h"
#include "DataSetDialog.h"
#include "IntDialog.h"
#include "ConfigDialog.h"
#ifdef ORIGIN_IMPORT
#include "importOPJ.h"
#include "ImportOPJU.h"
#endif
#include "AssociationsDialog.h"
#include "RenameWindowDialog.h"
#include "QwtErrorPlotCurve.h"
#include "InterpolationDialog.h"
#include "ImportASCIIDialog.h"
#include "XlsxImport.h"
#include "PptxExport.h"
#include "Workbook.h"
#include "ImageExportDialog.h"
#include "SmoothCurveDialog.h"
#include "FilterDialog.h"
#include "FFTDialog.h"
#include "Note.h"
#include "Folder.h"
#include "FindDialog.h"
#include "ScaleDraw.h"
#include "ScriptingLangDialog.h"
#include "TableStatistics.h"
#include "Fit.h"
#include "MultiPeakFit.h"
#include "PolynomialFit.h"
#include "NonLinearFit.h"
#include "SigmoidalFit.h"
#include "FunctionCurve.h"
#include "QwtPieCurve.h"
#include "Spectrogram.h"
#include "Differentiation.h"
#include "SmoothFilter.h"
#include "FFTFilter.h"
#include "Convolution.h"
#include "Correlation.h"
#include "CurveRangeDialog.h"
#include "ColorButton.h"
#include "QwtHistogram.h"
#include "BoxCurve.h"
#include "QwtBarCurve.h"
#include "OpenProjectDialog.h"
#include "IconLoader.h"
#include "core/Project.h"
#include "core/column/Column.h"
#include "core/datatypes/Double2StringFilter.h"
#include "lib/XmlStreamReader.h"
#include "table/future_Table.h"
#include "table/AsciiTableImportFilter.h"

// TODO: move tool-specific code to an extension manager
#include "ScreenPickerTool.h"
#include "DataPickerTool.h"
#include "TranslateCurveTool.h"
#include "MultiPeakFitTool.h"
#include "LineProfileTool.h"
#include "OriginGadgets.h"
#include "CurveMathTools.h"
#include "AdvancedSignalProc.h"
#include "GraphStatsAndSurfaceFit.h"
#include "ExtraPlotTypes.h"
#include "SettingsTransfer.h"
#include "RecalcEngine.h"
#include "I18nJapanese.h"
#include "GraphMiniToolbar.h"
#include "GraphInspectorDock.h"
#include "ModernUiTheme.h"
#include "ModernRibbonAndHud.h"
#include "UxNavigation.h"
#include "AutoUpdater.h"
#include "PythonAutomationServer.h"
#include "PiecewiseAndRobustFit.h"
#include "GraphDigitizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <algorithm>
#include <gsl/gsl_errno.h>

#include <QFileDialog>
#include <QScopedPointer>
#include <QInputDialog>
#include <QProgressDialog>
#include <QPrintDialog>
#include <QPixmapCache>
#include <QMenuBar>
#include <QClipboard>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTranslator>
#include <QSplitter>
#include <QApplication>
#include <QMessageBox>
#include <QPrinter>
#include <QActionGroup>
#include <QAction>
#include <QToolBar>
#include <QKeySequence>
#include <QImageReader>
#include <QImageWriter>
#include <QDateTime>
#include <QShortcut>
#include <QDockWidget>
#include <QTextStream>
#include <QVarLengthArray>
#include <QList>
#include <QUrl>
#include <QDesktopServices>
#include <QStatusBar>
#include <QToolButton>
#include <QSignalMapper>
#include <QUndoStack>
#include <QtDebug>
#include <QDialogButtonBox>
#include <QUndoView>
#include <QUndoStack>
#include <QTemporaryFile>
#include <QDebug>
#include <QTextCodec>
#include <QScrollBar>
#include <QMimeData>
#include <QBuffer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFontDatabase>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QHeaderView>
#include <QHash>
#include <QSlider>
#include <QMouseEvent>
#include <QKeyEvent>
#include <qwt_symbol.h>
#include <qwt_plot_canvas.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_errno.h>
#include "OleServer.h"
#include "PlotCurve.h"
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>

#include <zlib.h>

#include <iostream>
#include <memory>
using namespace std;

#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QLockFile>
#include <QFileSystemWatcher>
#include <QCryptographicHash>
#include <QScreen>
#include <QFrame>
#include <QScopedValueRollback>

#ifdef Q_OS_WIN
#include <io.h> // for _commit()
#include <qt_windows.h> // ReplaceFileW / MoveFileExW
#else
#include <unistd.h> // for fsync()
#include <signal.h> // for kill()
#endif

#include "ApplicationWindow_Internal.h"

using namespace std;
using namespace Qwt3D;
using namespace ApplicationWindowInternal;

namespace ApplicationWindowInternal {

QTranslator *sharedBuiltinJaTranslator()
{
    static QPointer<ToriGraphFullJapaneseTranslator> s_ja;
    if (s_ja.isNull() && qApp)
        s_ja = new ToriGraphFullJapaneseTranslator(qApp);
    return s_ja.data();
}

void registerApplicationWindowJapaneseStrings()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;

    auto add = [](const char *src, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(src),
                                                                  QString::fromUtf8(ja));
    };
    {
        // Saving / auto-save recovery
        add("Project not saved", "プロジェクトを保存できませんでした");
        add("The project was NOT saved.", "プロジェクトは保存されていません。");
        add("<html>The project could not be written to disk:<br>%1<br><br>The incomplete file is "
            "<em>%2</em>. If there already was a version of this project on disk, it has not been "
            "touched.</html>",
            "<html>プロジェクトをディスクに書き込めませんでした:<br>%1<br><br>書きかけのファイルは "
            "<em>%2</em> です。ディスク上に以前のプロジェクトファイルがある場合、そのファイルは"
            "変更されていません。</html>");
        add("<html>The project file could not be replaced:<br>%1<br><br>The file <em>%2</em> has "
            "not been changed. It may be in use by another program (for example a file "
            "synchronization service). Your current data was written to <em>%3</em>.<br><br>Please "
            "try saving again, or use \"Save Project As\" with a different name.</html>",
            "<html>プロジェクトファイルを置き換えられませんでした:<br>%1<br><br><em>%2</em> は"
            "変更されていません。ほかのプログラム(Dropbox などの同期ソフト)が使用中の可能性が"
            "あります。現在のデータは <em>%3</em> に書き出してあります。<br><br>もう一度保存するか、"
            "「名前を付けて保存」で別の名前を指定してください。</html>");
        add("<html>The compressed project file <em>%1</em> could not be written:<br>%2<br><br>If "
            "there already was a version of this project on disk, it has not been changed.</html>",
            "<html>圧縮プロジェクトファイル <em>%1</em> を書き込めませんでした:<br>%2<br><br>"
            "ディスク上に以前のプロジェクトファイルがある場合、そのファイルは変更されていません。"
            "</html>");
        add("Recovery copy saved at %1 (the project file itself was not changed).",
            "回復用のコピーを保存しました(%1)。プロジェクトファイル自体は変更していません。");
        add("Restore unsaved work?", "保存されていない作業を復元しますか?");
        add("ToriGraph was not closed properly last time. An automatically saved copy of your work "
            "was found:<br><br><b>%1</b><br>Saved: %2",
            "前回、ToriGraph が正常に終了しませんでした。自動保存された作業のコピーが"
            "見つかりました:<br><br><b>%1</b><br>保存日時: %2");
        add("a project that had not been saved yet", "まだ一度も保存していなかったプロジェクト");
        add("Restoring opens the copy as a modified project. The project file on disk is only "
            "changed when you save.",
            "復元すると、コピーを「変更あり」のプロジェクトとして開きます。ディスク上の"
            "プロジェクトファイルは、保存するまで変更されません。");
        add("Restoring opens the copy as a new, unsaved project.",
            "復元すると、コピーを未保存の新しいプロジェクトとして開きます。");
        add("Restore", "復元する");
        add("Discard Copy", "コピーを破棄");
        add("Decide Later", "あとで決める");
        add("The recovery copy could not be opened. It is kept in:<br>%1",
            "回復用のコピーを開けませんでした。次の場所に残してあります:<br>%1");
        add("[restored]", "[復元]");
    }
    {
        // Main menus
        add("&File", "ファイル(&F)");
        add("&Edit", "編集(&E)");
        add("&View", "表示(&V)");
        add("Scripting", "スクリプト(&S)");
        add("&Scripting", "スクリプト(&S)");
        add("&Graph", "グラフ(&G)");
        add("Gra&ph", "グラフ(&P)");
        add("3D &Plot", "3Dプロット(&P)");
        add("3&D Plot", "3Dプロット(&D)");
        add("&Matrix", "行列(&M)");
        add("&Table", "ワークシート(&T)");
        add("&Plot", "作図(&P)");
        add("&Analysis", "解析(&A)");
        add("Anal&yze", "解析(&Y)");
        add("&Tools", "ツール(&T)");
        add("Too&ls", "ツール(&L)");
        add("Wor&ksheet Analysis", "表の解析(&K)");
        add("Worksheet &Analysis", "表の解析(&A)");
        add("&Quick Fit", "クイックフィット(&Q)");
        add("For&mat", "書式(&O)");
        add("F&ormat", "書式(&O)");
        add("&Windows", "ウィンドウ(&W)");
        add("&Language", "言語 / Language(&L)");
        add("&Help", "ヘルプ(&H)");

        // Submenus & dock windows
        add("&New", "新規作成(&N)");
        add("&Recent Projects", "最近使ったプロジェクト(&R)");
        add("&Export Graph", "グラフのエクスポート(&E)");
        add("Toolbars", "ツールバー");
        add("Special Line/Symb&ol", "特殊折れ線・シンボル(&O)");
        add("Statistical &Graphs", "統計グラフ(&G)");
        add("Pa&nel", "パネル・マルチレイヤー(&N)");
        add("Mat&erials Analysis", "材料解析(&E)");
        add("Materials Analysis", "材料解析");
        add("Convert to Double-Y Plot", "2軸Yグラフへ変更");
        add("Double-Y plot needs at least two curves on the graph. Add another Y column to this "
            "graph first, or select two Y columns in a worksheet.",
            "2軸Yグラフにするにはグラフ上に2本以上の曲線が必要です。先に別のY列をこのグラフへ"
            "追加するか、ワークシートで2列以上のY列を選択してください。");
        add("Double-Y plot requires at least two Y columns or a graph with two or more curves.",
            "2軸Yグラフには2列以上のY列、または2本以上の曲線があるグラフが必要です。");
        add("Add to Graph", "グラフへ追加");
        add("+ Add Column", "+列追加");
        add("Append a new Y column to this worksheet", "このワークシートの末尾に新しいY列を追加します");
        add("&Translate", "曲線移動(&T)");
        add("&Smooth", "平滑化(&S)");
        add("&FFT Filter", "FFTフィルタ(&F)");
        add("Fit E&xponential Decay", "指数減衰フィット(&X)");
        add("Fit &Multi-Peak", "マルチピークフィット(&M)");
        add("Fit &Multi-peak", "マルチピークフィット(&M)");
        add("Apply Color &Palette", "カラーパレット適用(&P)");
        add("Graph &Aspect Ratio", "グラフ縦横比(&A)");
        add("Project Explorer", "プロジェクトエクスプローラ");
        add("Project &Explorer", "プロジェクトエクスプローラ(&E)");
        add("Results Log", "結果ログ");
        add("&Results Log", "結果ログ(&R)");
        add("Scripting Console", "スクリプトコンソール");
        add("ToriGraph - untitled", "ToriGraph - 無題");

        // Explorer headers & filters
        add("Name", "名前");
        add("Type", "種類");
        add("View", "表示状態");
        add("Created", "作成日時");
        add("Label", "ラベル");
        add("Folder", "フォルダ");
        add("Table", "ワークシート");
        add("Graph", "グラフ");
        add("3D Graph", "3Dグラフ");
        add("Matrix", "行列");
        add("Note", "ノート");
        add("Notes", "ノート");
        add("Search windows (name / label)...", "ウィンドウ検索 (名前・ラベル)...");
        add("Search windows / labels...", "ウィンドウ検索...");
        add("All Windows", "すべて");
        add("Tables (Workbooks)", "ワークシート");
        add("Graphs (2D/3D)", "グラフ");
        add("Matrices", "行列");
        add("Notes / Scripts", "ノート");
        add("Subfolders Only", "サブフォルダ");
        add("Subfolders", "サブフォルダ");
        add("Tabbed View", "タブ表示");
        add("Tabbed Workspace &View", "タブ表示モード (Origin風)(&V)");

        // File & Edit actions
        add("New &Project", "新規プロジェクト(&P)");
        add("New &Graph", "新規グラフ(&G)");
        add("New &Note / Script", "新規ノート / スクリプト(&N)");
        add("New &Table", "新規ワークシート(&T)");
        add("New &Matrix", "新規行列(&M)");
        add("New &Function Plot", "新規関数プロット(&F)");
        add("New 3D &Surface Plot", "新規3D曲面プロット(&S)");
        add("&Open", "開く(&O)");
        add("Open Image &File", "画像ファイルを開く(&F)");
        add("Import I&mage...", "画像をインポート(&M)...");
        add("&Save Project", "プロジェクトを保存(&S)");
        add("Save Project &As...", "名前を付けてプロジェクトを保存(&A)...");
        add("Open Temp&late...", "テンプレートを開く(&L)...");
        add("Open Te&mplate...", "テンプレートを開く(&M)...");
        add("Save As &Template...", "テンプレートとして保存(&T)...");
        add("&Import ASCII...", "ASCIIインポート(&I)...");
        add("E&xport ASCII", "ASCIIエクスポート(&X)");
        add("&Current", "現在のグラフ(&C)");
        add("&All", "すべて(&A)");
        add("&Export PDF", "PDFエクスポート(&E)");
        add("&Print", "印刷(&P)");
        add("Print All Plo&ts", "全プロットを印刷(&T)");
        add("&Quit", "終了(&Q)");
        add("&Undo", "元に戻す(&U)");
        add("&Redo", "やり直し(&R)");
        add("&Duplicate", "複製(&D)");
        add("Duplicat&e", "複製(&E)");
        add("Cu&t Selection", "切り取り(&T)");
        add("&Copy Selection", "コピー(&C)");
        add("P&aste Selection", "貼り付け(&A)");
        add("&Delete Selection", "選択範囲を削除(&D)");
        add("Delete &Fit Tables", "フィット結果ワークシートを削除(&F)");
        add("Clear &Log Information", "結果ログを消去(&L)");
        add("Prefere&nces...", "環境設定(&N)...");
        add("Loc&k Toolbars", "ツールバーを固定(&K)");
        add("Undo/Redo &History", "元に戻す/やり直し履歴(&H)");
        add("Plot &Wizard", "プロットウィザード(&W)");

        // Plot types (Standard + OriginPro)
        add("&Line", "折れ線(&L)");
        add("&Scatter", "散布図(&S)");
        add("Line + S&ymbol", "折れ線 + シンボル(&Y)");
        add("Vertical &Drop Lines", "垂直ドロップライン(&D)");
        add("&Spline", "スプライン(&S)");
        add("&Horizontal Steps", "水平ステップ(&H)");
        add("&Vertical Steps", "垂直ステップ(&V)");
        add("Double-&Y Plot", "2軸グラフ (Double-Y)(&Y)");
        add("Waterfall / &Y-Offset Stack", "Yオフセット積み上げ (Waterfall)(&Y)");
        add("&Ternary Plot", "三角図 (Ternary)(&T)");
        add("&Vertical Bars", "垂直棒グラフ(&V)");
        add("&Horizontal Bars", "水平棒グラフ(&H)");
        add("&Area", "面グラフ(&A)");
        add("&Pie", "円グラフ(&P)");
        add("Vectors XY&AM", "ベクトル図 XY&AM");
        add("Vectors &XYXY", "ベクトル図 &XYXY");
        add("&Box Plot", "箱ひげ図(&B)");
        add("&Violin Plot", "バイオリン図(&V)");
        add("Box Plot", "箱ひげ図");
        add("Violin Plot", "バイオリン図");
        add("Bar Chart", "棒グラフ");
        add("&Pareto Chart", "パレート図 (Pareto)(&P)");
        add("Normal &Q-Q Plot", "正規Q-Qプロット (Normal Q-Q)(&Q)");
        add("&Histogram", "ヒストグラム(&H)");
        add("&Stacked Histogram", "積み上げヒストグラム(&S)");
        add("&Vertical 2 Layers", "垂直2レイヤー(&V)");
        add("&Horizontal 2 Layers", "水平2レイヤー(&H)");
        add("&4 Layers", "4レイヤー(&4)");
        add("&Stacked Layers", "スタックレイヤー(&S)");

        // Analysis & Peak Analyzer
        add("Peak &Analyzer...", "ピークアナライザ(&A)...");
        add("Peak Analyzer", "ピークアナライザ");
        add("Peak Analyzer (Baseline, Peak Finding, Deconvolution & Integration)",
            "ピークアナライザ (ベースライン・ピーク検出・分離・積分)");
        add("&Peak / Multi-Peak Fit...", "ピーク / マルチピークフィット(&P)...");
        add("Auto Peak Fit (&Gaussian)", "自動ピークフィット (Gaussian)(&G)");
        add("Auto Peak Fit (&Lorentzian)", "自動ピークフィット (Lorentzian)(&L)");
        add("Auto Peak Fit (Pseudo-&Voigt)", "自動ピークフィット (Pseudo-Voigt)(&V)");
        add("Fit &Wizard...", "フィットウィザード(&W)...");
        add("Fit &Linear", "線形フィット(&L)");
        add("Fit &Polynomial ...", "多項式フィット(&P)...");
        add("Fit &Boltzmann (Sigmoidal)", "ボルツマン (シグモイド) フィット(&B)");
        add("Fit &Gaussian", "ガウスフィット(&G)");
        add("Fit Lorent&zian", "ローレンツフィット(&Z)");
        add("&Differentiate", "微分(&D)");
        add("&Integrate ...", "積分(&I)...");
        add("Inte&rpolate ...", "補間(&R)...");
        add("&FFT...", "高速フーリエ変換 (FFT)(&F)...");
        add("Statistics on &Columns", "列の統計(&C)");
        add("Statistics on &Rows", "行の統計(&R)");

        // Format, Color Palette, Aspect Ratio, OLE
        add("Color Palette &Editor...", "カラーパレットエディタ(&E)...");
        add("Copy Graph as &OLE Object", "グラフをOLEオブジェクトとしてコピー(&O)");
        add("&Rescale to Show All", "全範囲に自動スケール(&R)");
        add("&Plot ...", "プロット詳細(&P)...");
        add("&Scales...", "軸スケール(&S)...");
        add("&Axes...", "軸と目盛り(&A)...");
        add("&Grid ...", "グリッド(&G)...");
        add("&Title ...", "タイトル(&T)...");
        add("1:1 (Square - ISIJ)", "1:1 (正方形 - ISIJ標準)");
        add("4:3 (Standard)", "4:3 (標準)");
        add("16:9 (Widescreen)", "16:9 (ワイド)");
        add("3:2 (Classic)", "3:2 (クラシック)");
        add("Custom Aspect Ratio (W/H)...", "カスタム縦横比 (W/H)...");
        add("Keep Aspect Ratio on Resize", "リサイズ時に縦横比を維持");

        // Peak Analyzer Dialog strings
        add("Peak Analyzer - Multi-Peak & Baseline Suite",
            "ピークアナライザ - マルチピーク・ベースライン解析");
        add("1. Goal & Baseline", "1. 目的とベースライン");
        add("2. Find Peaks & Peak List", "2. ピーク検出・一覧編集");
        add("3. Fit & Output", "3. フィット関数・出力設定");
        add("Target Curve:", "対象曲線:");
        add("Peak Analyzer Goal:", "解析ゴール (Goal):");
        add("Baseline Mode:", "ベースラインモード:");
        add("Preview Baseline on Graph", "グラフ上にベースラインをプレビュー");
        add("Subtract Baseline from Data", "データからベースラインを差し引く");
        add("Auto-Find Peaks", "自動ピーク検出");
        add("Interactive Pick on Graph", "グラフ上で手動ピーク選択");
        add("+ Add Peak", "+ ピーク追加");
        add("- Remove Peak", "- ピーク削除");
        add("Peak Profile:", "ピーク形状関数:");
        add("Plot Individual Deconvoluted Peak Curves", "分離された各ピーク曲線をプロット");
        add("Plot Baseline Curve on Graph", "ベースライン曲線をグラフにプロット");
        add("Generate Peak Summary Table", "ピーク特性一覧ワークシートを生成");
        add("Generate Integrated Peak Area Table", "数値積分ピーク面積ワークシートを生成");
        add("Generate Residuals Table", "残差ワークシートを生成");

        // Phase 3: OriginPro Top-Level Gadgets Menu & Interactive Gadgets
        add("&Gadgets", "ガジェット(&G)");
        add("Ga&dgets", "ガジェット(&D)");
        add("Gadgets", "ガジェット");
        add("&Vertical Cursor (All Layers)", "垂直カーソル (全レイヤー連動)(&V)");
        add("Vertical Cursor (All Layers)", "垂直カーソル (全レイヤー連動)");
        add("&Region Statistics (ROI)...", "領域統計 (ROI)(&R)...");
        add("Region Statistics (ROI)...", "領域統計 (ROI)...");
        add("Quick &Fit (ROI)...", "クイックフィット (ROI)(&F)...");
        add("Quick Fit (ROI)...", "クイックフィット (ROI)...");
        add("Quick &Peaks (ROI)...", "クイックピーク解析 (ROI)(&P)...");
        add("Quick Peaks (ROI)...", "クイックピーク解析 (ROI)...");
        add("Quick &Sigmoidal Fit (ROI)...", "クイックシグモイドフィット (ROI)(&S)...");
        add("Quick Sigmoidal Fit (ROI)...", "クイックシグモイドフィット (ROI)...");
        add("&Integrate (ROI)...", "数値積分 (ROI)(&I)...");
        add("Integrate (ROI)...", "数値積分 (ROI)...");
        add("In&terpolate (ROI)...", "補間 (ROI)(&T)...");
        add("Interpolate (ROI)...", "補間 (ROI)...");
        add("FF&T Preview (ROI)...", "FFTプレビュー (ROI)(&T)...");
        add("FFT Preview (ROI)...", "FFTプレビュー (ROI)...");
        add("Rise &Time (Pro)...", "立ち上がり時間解析 (Pro)(&T)...");
        add("Rise Time (Pro)...", "立ち上がり時間解析 (Pro)...");
        add("&Cluster Selector (Pro)...", "クラスタ選択・統計 (Pro)(&C)...");
        add("Cluster Selector (Pro)...", "クラスタ選択・統計 (Pro)...");
        add("Clear Active &Gadget / ROI", "アクティブなガジェット/ROIを解除(&G)");
        add("Clear Active Gadget / ROI", "アクティブなガジェット/ROIを解除");
        add("Interactive ROI Gadgets", "インタラクティブROIガジェット");

        // Phase 3: Non-Destructive Data Masking
        add("Data &Masking", "データマスク(&M)");
        add("Data Masking", "データマスク");
        add("&Mask Data Points (Click)", "データ点をマスク (クリック)(&M)");
        add("Mask Data Points (Click)", "データ点をマスク (クリック)");
        add("&Unmask Data Points (Click)", "データ点のマスク解除 (クリック)(&U)");
        add("Unmask Data Points (Click)", "データ点のマスク解除 (クリック)");
        add("Mask &Region (ROI)", "矩形領域をマスク (ROI)(&R)");
        add("Mask Region (ROI)", "矩形領域をマスク (ROI)");
        add("Unmask Re&gion (ROI)", "矩形領域のマスク解除 (ROI)(&G)");
        add("Unmask Region (ROI)", "矩形領域のマスク解除 (ROI)");
        add("&Clear All Masks on Graph", "グラフ上の全マスクをクリア(&C)");
        add("Clear All Masks on Graph", "グラフ上の全マスクをクリア");
        add("Mask data points on graph without deleting table values",
            "ワークシート値を削除せず非破壊でグラフ上のデータ点をマスクします");
        add("Unmask previously masked data points on graph",
            "マスクされたデータ点のマスクを解除します");

        // Phase 3: Structured OriginPro Analysis Submenus
        add("&Mathematics", "数学・曲線演算(&M)");
        add("Mathematics", "数学・曲線演算");
        add("&Fitting", "フィット(&F)");
        add("Fitting", "フィット");
        add("&Peaks and Baseline", "ピークとベースライン(&P)");
        add("Peaks and Baseline", "ピークとベースライン");
        add("Signal &Processing", "信号処理(&P)");
        add("Signal Processing", "信号処理");
        add("S&tatistics", "グラフ統計解析(&T)");
        add("Statistics on Graph", "グラフ統計解析");
        add("&Recalculation && Themes", "再計算とテーマ(&R)");
        add("Recalculation & Themes", "再計算とテーマ");

        // Phase 3: Curve Mathematics
        add("&Simple Curve Math (A +-*/ B)...", "単純曲線演算 (A +-*/ B)(&S)...");
        add("Simple Curve Math (A +-*/ B)...", "単純曲線演算 (A +-*/ B)...");
        add("Su&btract Straight Line...", "直線の減算(&B)...");
        add("Subtract Straight Line...", "直線の減算...");
        add("&Average Multiple Curves (Mean \xC2\xB1 SD)...", "複数曲線の平均 (平均 \xC2\xB1 SD)(&A)...");
        add("Average Multiple Curves (Mean \xC2\xB1 SD)...", "複数曲線の平均 (平均 \xC2\xB1 SD)...");
        add("&Normalize Curves...", "曲線の正規化(&N)...");
        add("Normalize Curves...", "曲線の正規化...");
        add("Curve &Intersections && X/Y Lookup...", "曲線の交点・X/Y逆引き検索(&I)...");
        add("Curve Intersections & X/Y Lookup...", "曲線の交点・X/Y逆引き検索...");
        add("Calculate &Polygon / Loop Area", "ポリゴン面積・曲線長を計算(&P)");
        add("Calculate Polygon / Loop Area", "ポリゴン面積・曲線長を計算");
        add("Polygon / Hysteresis Loop Area & Arc Length", "ポリゴン閉ループ面積・曲線長");

        // Phase 3: Advanced Fitting & Surface / 2D Peaks
        add("&Apparent Linear Fit (Log Axes)", "見かけの線形フィット (対数軸)(&A)");
        add("Apparent Linear Fit (Log Axes)", "見かけの線形フィット (対数軸)");
        add("Linear Fit with X && Y Errors (&York)...", "X・Y誤差付き線形フィット (York法)(&Y)...");
        add("Linear Fit with X & Y Errors (York)...", "X・Y誤差付き線形フィット (York法)...");
        add("&Global Curve Fit (Shared Parameters)...", "グローバルフィット (共有パラメータ)(&G)...");
        add("Global Curve Fit (Shared Parameters)...", "グローバルフィット (共有パラメータ)...");
        add("&Implicit / ODR Curve Fit...", "陰関数・直交距離回帰 (ODR) フィット(&I)...");
        add("Implicit / ODR Curve Fit...", "陰関数・直交距離回帰 (ODR) フィット...");
        add("&Compare && Rank Fit Models (AIC/BIC)...", "フィットモデル比較・AIC/BIC順位付け(&C)...");
        add("Compare & Rank Fit Models (AIC/BIC)...", "フィットモデル比較・AIC/BIC順位付け...");
        add("3D &Surface Fit (XYZ / Matrix)...", "3D曲面フィット (XYZ / 行列)(&S)...");
        add("3D Surface Fit (XYZ / Matrix)...", "3D曲面フィット (XYZ / 行列)...");
        add("&Batch Peak Analysis (All Curves)...", "バッチピーク解析 (全曲線一括)(&B)...");
        add("Batch Peak Analysis (All Curves)...", "バッチピーク解析 (全曲線一括)...");
        add("&2D Peak Analysis (Matrix / Image)...", "2Dピーク解析 (行列・画像)(&2)...");
        add("2D Peak Analysis (Matrix / Image)...", "2Dピーク解析 (行列・画像)...");

        // Phase 3: Peak Analyzer Upgrades
        add("User Anchor Points (Spline)", "ユーザー指定アンカー点 (スプライン補間)");
        add("Anchor Points (x1,y1; x2,y2; ...):", "アンカー座標 (x1,y1; x2,y2; ...):");
        add("Auto-Seed Anchors from Local Minima", "極小値からアンカー点を自動抽出");
        add("Cubic Spline Interpolation", "3次スプライン補間 (オフ時は折れ線)");
        add("2nd Derivative (Hidden / Shoulder Peaks)", "2次微分法 (隠れピーク・ショルダー検出)");

        // Phase 3: Signal Processing & Smoothing Upgrades
        add("&LOWESS Smooth...", "LOWESS局所回帰平滑化(&L)...");
        add("LOWESS Smooth...", "LOWESS局所回帰平滑化...");
        add("LOWESS Smooth", "LOWESS局所回帰平滑化");
        add("&Whittaker Smooth...", "Whittakerペナルティ付き平滑化(&W)...");
        add("Whittaker Smooth...", "Whittakerペナルティ付き平滑化...");
        add("Whittaker Smooth", "Whittakerペナルティ付き平滑化");
        add("M&edian Filter Smooth...", "メディアンフィルタ平滑化(&E)...");
        add("Median Filter Smooth...", "メディアンフィルタ平滑化...");
        add("&IIR Digital Filter (Butterworth/Chebyshev/Bessel)...",
            "IIRデジタルフィルタ (Butterworth/Chebyshev/Bessel)(&I)...");
        add("IIR Digital Filter (Butterworth/Chebyshev/Bessel)...",
            "IIRデジタルフィルタ (Butterworth/Chebyshev/Bessel)...");
        add("&Short-Time Fourier Transform (STFT)...", "短時間フーリエ変換 (STFTスペクトログラム)(&S)...");
        add("Short-Time Fourier Transform (STFT)...", "短時間フーリエ変換 (STFTスペクトログラム)...");
        add("&Hilbert Transform (Envelope)...", "ヒルベルト変換 (包絡線・瞬時位相)(&H)...");
        add("Hilbert Transform (Envelope)...", "ヒルベルト変換 (包絡線・瞬時位相)...");
        add("&Wavelet Transform (CWT / DWT Denoise)...",
            "ウェーブレット変換 (CWT / DWTノイズ除去)(&W)...");
        add("Wavelet Transform (CWT / DWT Denoise)...",
            "ウェーブレット変換 (CWT / DWTノイズ除去)...");
        add("Welch &PSD && Cross-Coherence...", "Welchパワースペクトル密度・コヒーレンス(&P)...");
        add("Welch PSD & Cross-Coherence...", "Welchパワースペクトル密度・コヒーレンス...");
        add("2D &FFT (Matrix)", "2次元FFT (行列)(&F)");
        add("2D FFT (Matrix)", "2次元FFT (行列)");
        add("2D &IFFT (Inverse Matrix FFT)", "2次元逆FFT (行列IFFT)(&I)");
        add("2D IFFT (Inverse Matrix FFT)", "2次元逆FFT (行列IFFT)");
        add("2D FFT / IFFT (Matrix)", "2次元FFT / IFFT (行列)");

        // Phase 3: Statistics on Graph
        add("2D &Confidence / Prediction Ellipse...", "2D信頼楕円・予測楕円(&C)...");
        add("2D Confidence / Prediction Ellipse...", "2D信頼楕円・予測楕円...");
        add("&t-Test / One-Way ANOVA + Significance Brackets...",
            "t検定・一元配置分散分析 (有意差ブラケット描画)(&T)...");
        add("t-Test / One-Way ANOVA + Significance Brackets...",
            "t検定・一元配置分散分析 (有意差ブラケット描画)...");
        add("&ROC Curve (AUC)...", "ROC曲線・AUC解析(&R)...");
        add("ROC Curve (AUC)...", "ROC曲線・AUC解析...");
        add("&Kaplan-Meier Survival Analysis...", "Kaplan-Meier生存時間解析(&K)...");
        add("Kaplan-Meier Survival Analysis...", "Kaplan-Meier生存時間解析...");
        add("&Principal Component Analysis (PCA Biplot)...", "主成分分析 (PCAバイプロット)(&P)...");
        add("Principal Component Analysis (PCA Biplot)...", "主成分分析 (PCAバイプロット)...");

        // Phase 3: Dynamic Recalculation & Dialog Themes
        add("&Auto Recalculate (Green Lock)", "自動再計算 (グリーンロック)(&A)");
        add("Auto Recalculate (Green Lock)", "自動再計算 (グリーンロック)");
        add("&Manual Recalculate", "手動再計算(&M)");
        add("Manual Recalculate", "手動再計算");
        add("&Disable Recalculation", "再計算を無効化(&D)");
        add("Disable Recalculation", "再計算を無効化");
        add("&Recalculate Pending Links Now", "保留中の再計算リンクを今すぐ更新(&R)");
        add("Recalculate Pending Links Now", "保留中の再計算リンクを今すぐ更新");
        add("Repeat &Linear Fit for All Curves", "全曲線に対して線形フィットを一括実行(&L)");
        add("Repeat Linear Fit for All Curves", "全曲線に対して線形フィットを一括実行");
        add("Repeat &Normalization for All Curves", "全曲線に対して正規化を一括実行(&N)");
        add("Repeat Normalization for All Curves", "全曲線に対して正規化を一括実行");
        add("Manage Dialog &Themes...", "ダイアログテーマの管理(&T)...");
        add("Manage Dialog Themes...", "ダイアログテーマの管理...");
        add("X Error Column (\xCF\x83x):", "X誤差列 (\xCF\x83x):");
        add("Y Error Column (\xCF\x83y):", "Y誤差列 (\xCF\x83y):");
        add("Plot 95% Confidence Band", "95%信頼帯 (Confidence Band) をプロット");
        add("Plot 95% Prediction Band", "95%予測帯 (Prediction Band) をプロット");
        add("Insert Fit Summary Box on Graph", "グラフ上にフィット結果表を挿入");
        add("<None / Unit Weights>", "<なし / 等重み>");
        add("Select Matrix:", "対象行列の選択:");
        add("Please create or activate a Matrix window first.",
            "先に行列 (Matrix) ウィンドウを作成またはアクティブにしてください。");
        add("Cleared all masked points across %1 curve(s).",
            "%1 本の曲線からすべてのマスクをクリアしました。");
        add("Recalculated %1 linked analysis output(s).",
            "%1 件の解析リンクを再計算しました。");

        // Language menu & Toolbar window titles & Cold-start core strings
        add("&Language (言語)", "言語 / Language(&L)");
        add("日本語 (&Japanese)", "日本語 (&Japanese)");
        add("&English", "英語 (English)(&E)");
        add("English", "英語 (English)");
        add("UNTITLED", "無題");
        add("File", "ファイル");
        add("Edit", "編集");
        add("Plot", "作図");
        add("Matrix Plot", "行列プロット");
        add("3D Surface", "3D曲面");
        add("Undo/Redo History", "元に戻す/やり直し履歴");
        add("Search windows / labels...", "ウィンドウ検索...");
        add("&Console", "スクリプトコンソール(&C)");
        add("New Folder", "新規フォルダ");
        add("New Table", "新規ワークシート");
        add("New Table (Workbook)", "新規ワークシート (ワークブック)");
        add("New Graph", "新規グラフ");
        add("New 2D Graph", "新規2Dグラフ");
        add("New Aspect", "新規オブジェクト作成");

        // Color Palette Presets
        add("Matplotlib (tab10)", "Matplotlib 既定色 (tab10)");
        add("Origin Classic", "Origin クラシック");
        add("Viridis", "Viridis (知覚均等)");
        add("Plasma", "Plasma (知覚均等)");
        add("Inferno", "Inferno (知覚均等)");
        add("CoolWarm", "CoolWarm (発散型)");
        add("Nature (NPG)", "Nature (NPG誌パレット)");
        add("Science (AAAS)", "Science (AAAS誌パレット)");
        add("Okabe-Ito (Colorblind Safe)", "岡部・伊藤 (色覚バリアフリー)");
        add("Monochrome / Grayscale", "モノクロ / グレースケール");

        // Complete Classic Menu / Action / Toolbar / Context Coverage
        add("&Open...", "開く(&O)...");
        add("Open Image &File...", "画像ファイルを開く(&F)...");
        add("Arran&ge Layers", "レイヤー配置(&G)");
        add("Arran&ge Layers...", "レイヤー配置(&G)...");
        add("&Current...", "現在のグラフ(&C)...");
        add("&All...", "すべてのグラフ(&A)...");
        add("&Export PDF...", "PDFエクスポート(&E)...");
        add("&Print...", "印刷(&P)...");
        add("Print All Plo&ts...", "全プロットを印刷(&T)...");
        add("E&xport ASCII...", "ASCIIエクスポート(&X)...");
        add("Add La&yer", "レイヤー追加(&Y)");
        add("Automatic Layout", "自動レイアウト");
        add("&Remove Layer", "レイヤー削除(&R)");
        add("Add/Remove &Curve...", "曲線の追加/削除(&C)...");
        add("Add &Error Bars...", "エラーバー追加(&E)...");
        add("Add &Function...", "関数曲線の追加(&F)...");
        add("New &Legend", "新規凡例(&L)");
        add("Add Time Stamp", "タイムスタンプ追加");
        add("Add &Image", "画像の追加(&I)");
        add("Add &Text", "テキスト追加(&T)");
        add("Draw &Arrow", "矢印を描画(&A)");
        add("Draw &Line", "直線を描画(&L)");
        add("Manage layers", "レイヤー管理");
        add("Add curves / error bars", "曲線・エラーバーの追加");
        add("Enrichments", "注釈・図形要素");
        add("Lines and/or symbols", "折れ線・散布図");
        add("Disable &Tools", "ツール選択解除(&T)");
        add("Disable &tools", "ツール選択解除(&T)");
        add("&Zoom In", "拡大(&Z)");
        add("&Zoom Out", "縮小(&Z)");
        add("Zoom &Out", "縮小(&O)");
        add("S&creen Reader", "スクリーン座標読取(&C)");
        add("&Data Reader", "データ読取カーソル(&D)");
        add("&Select Data Range", "データ範囲選択(&S)");
        add("&Move Data Points...", "データ点の移動(&M)...");
        add("Remove &Bad Data Points...", "外れ値データ点の除外(&B)...");
        add("&Double-Y Plot", "2軸グラフ (Double-Y)(&D)");
        add("&Waterfall / Y-Offset Stack", "Yオフセット積み上げ (Waterfall)(&W)");
        add("Pa&reto Chart", "パレート図 (Pareto)(&R)");
        add("&Ternary Plot (A-B-C)", "三角図 (A-B-C)(&T)");
        add("Color &Palette Editor...", "カラーパレットエディタ(&P)...");
        add("&1:1 (Square - ISIJ)", "1:1 (正方形 - ISIJ標準)(&1)");
        add("&4:3 (Standard)", "4:3 (標準)(&4)");
        add("1&6:9 (Widescreen)", "16:9 (ワイド)(&6)");
        add("&3:2 (Classic)", "3:2 (クラシック)(&3)");
        add("&Custom Aspect Ratio...", "カスタム縦横比(&C)...");
        add("&Keep Aspect Ratio on Resize", "リサイズ時に縦横比を維持(&K)");
        add("Custom Graph Aspect Ratio", "カスタムグラフ縦横比");
        add("Aspect ratio (Width / Height):", "縦横比 (幅 / 高さ):");
        add("&Ribbon", "3Dリボン(&R)");
        add("&Bars", "3D棒グラフ(&B)");
        add("&Trajectory", "3D軌跡(&T)");
        add("Contour + &Color Fill", "等高線 + カラー塗りつぶし(&C)");
        add("Contour &Lines", "等高線のみ(&L)");
        add("&Gray Scale Map", "グレースケールマップ(&G)");
        add("3D &Wire Frame", "3Dワイヤフレーム(&W)");
        add("3D &Hidden Line", "3D隠線処理(&H)");
        add("3D &Polygons", "3Dポリゴン(&P)");
        add("3D Wire &Surface", "3Dワイヤ曲面(&S)");
        add("&Invert", "逆行列(&I)");
        add("&Determinant", "行列式(&D)");
        add("&Convert to Table", "ワークシートへ変換(&C)");
        add("Convert to &Matrix", "行列へ変換(&M)");
        add("&Low Pass...", "ローパスフィルタ(&L)...");
        add("&High Pass...", "ハイパスフィルタ(&H)...");
        add("&Band Pass...", "バンドパスフィルタ(&B)...");
        add("&Band Block...", "バンド阻止フィルタ(&B)...");
        add("&Savitzky-Golay...", "Savitzky-Golay平滑化(&S)...");
        add("&FFT Filter...", "FFTフィルタ平滑化(&F)...");
        add("Moving Window &Average...", "移動平均平滑化(&A)...");
        add("&First Order ...", "1次指数減衰(&F)...");
        add("&Second Order ...", "2次指数減衰(&S)...");
        add("&Third Order ...", "3次指数減衰(&T)...");
        add("Fit Exponential Gro&wth ...", "指数成長フィット(&W)...");
        add("Co&rrelate", "相互相関(&R)");
        add("&Autocorrelate", "自己相関(&A)");
        add("&Convolute", "畳み込み (Convolution)(&C)");
        add("&Deconvolute", "逆畳み込み (Deconvolution)(&D)");
        add("&Horizontal", "水平移動(&H)");
        add("&Vertical", "垂直移動(&V)");
        add("&Gaussian...", "ガウス関数 (Gaussian)(&G)...");
        add("&Lorentzian...", "ローレンツ関数 (Lorentzian)(&L)...");
        add("Gaussian (&Amplitude)...", "ガウス関数・振幅型(&A)...");
        add("&Pseudo-Voigt...", "擬フォークト関数 (Pseudo-Voigt)(&P)...");
        add("Pearson &VII...", "ピアソンVII関数 (Pearson VII)(&V)...");
        add("Scripting &Language", "スクリプト言語の選択(&L)");
        add("&Restart Scripting", "スクリプト環境を再起動(&R)");
        add("E&xecute", "選択行を実行(&X)");
        add("Execute &All", "すべて実行(&A)");
        add("&Evaluate Expression", "式を評価(&E)");
        add("&Save Note As...", "ノートに名前を付けて保存(&S)...");
        add("&Cascade", "重ねて表示(&C)");
        add("&Tile", "並べて表示(&T)");
        add("&Next", "次のウィンドウ(&N)");
        add("&Previous", "前のウィンドウ(&P)");
        add("&Rename Window", "ウィンドウ名の変更(&R)");
        add("Close &Window", "ウィンドウを閉じる(&W)");
        add("Window &Geometry...", "ウィンドウの位置とサイズ(&G)...");
        add("&Hide Window", "ウィンドウを隠す(&H)");
        add("More Windows...", "その他のウィンドウ...");
        add("More windows...", "その他のウィンドウ...");
        add("&Activate Window", "ウィンドウをアクティブ化(&A)");
        add("Mi&nimize Window", "ウィンドウを最小化(&N)");
        add("Ma&ximize Window", "ウィンドウを最大化(&X)");
        add("Re&size Window...", "ウィンドウサイズ変更(&S)...");
        add("&Print Window", "ウィンドウを印刷(&P)");
        add("&About ToriGraph", "ToriGraph について(&A)");
        add("&Choose Help Folder...", "ヘルプフォルダを選択(&C)...");
        add("Search for &Updates", "アップデートを確認(&U)");
        add("&ToriGraph Homepage", "ToriGraph ホームページ(&T)");
        add("ToriGraph &Forums", "ToriGraph フォーラム(&F)");
        add("Visit ToriGraph &Forums", "ToriGraph フォーラムを開く(&F)");
        add("Report a &Bug", "バグを報告(&B)");
        add("Download &Manual", "マニュアルをダウンロード(&M)");
        add("&Plot details...", "プロット詳細(&P)...");
        add("&Worksheet", "ワークシートを表示(&W)");
        add("&Reset to Full Range", "全データ範囲にリセット(&R)");
        add("Edit &Range...", "データ範囲を編集(&R)...");
        add("&Delete", "削除(&D)");
        add("&Cut", "切り取り(&T)");
        add("&Copy", "コピー(&C)");
        add("&Hide", "非表示(&H)");
        add("Hide &Other Curves", "他の曲線を非表示(&O)");
        add("&Show All Curves", "すべての曲線を表示(&S)");
        add("&Edit Function...", "関数式を編集(&E)...");
        add("&Copy status bar text", "ステータスバーのテキストをコピー(&C)");
        add("&View Pixel Line Profile", "ピクセルラインプロファイル(&V)");
        add("&View Pixel Line profile", "ピクセルラインプロファイル(&V)");
        add("&Intensity Table", "強度ワークシート作成(&I)");
        add("&Intensity Matrix", "強度行列作成(&I)");
        add("&Properties", "プロパティ(&P)");
        add("&Properties...", "プロパティ(&P)...");
        add("&Layer Geometry", "レイヤー配置・サイズ(&L)");
        add("&Surface...", "曲面設定(&S)...");
        add("&Data Set...", "データセット選択(&D)...");
        add("Box", "ボックス枠");
        add("Frame", "フレーム枠");
        add("&Frame", "フレーム枠(&F)");
        add("No Axes", "軸なし");
        add("Front grid", "前面グリッド");
        add("Back grid", "背面グリッド");
        add("Right grid", "右面グリッド");
        add("Left grid", "左面グリッド");
        add("Ceiling grid", "上面グリッド");
        add("Floor grid", "底面グリッド");
        add("Wireframe", "ワイヤフレーム");
        add("Hidden Line", "隠線処理");
        add("Polygon Only", "ポリゴンのみ");
        add("Mesh & Filled Polygons", "メッシュ＋塗りつぶしポリゴン");
        add("Dots", "ドット");
        add("Bars", "3Dバー");
        add("Cones", "コーン");
        add("Crosshairs", "クロスヘア");
        add("Floor Data Projection", "底面データ投影");
        add("Floor Isolines", "底面等高線");
        add("Empty Floor", "底面投影なし");
        add("Animation", "回転アニメーション");
        add("Enable perspective", "透視投影を有効化");
        add("Reset rotation", "回転をリセット");
        add("Fit frame to window", "枠をウィンドウに合わせる");

        // Phase 4: OriginPro Page / Layer / Frame / Mini-Toolbar UX
        add("Page &Setup...", "ページ設定(&S)...");
        add("Page Setup...", "ページ設定...");
        add("Fit &Page to Layers", "ページをレイヤーに合わせる(&P)");
        add("Fit Page to Layers", "ページをレイヤーに合わせる");
        add("Fit &Layers to Page", "レイヤーをページに合わせる(&L)");
        add("Fit Layers to Page", "レイヤーをページに合わせる");
        add("Toggle Page &View", "ページ印刷ビュー切替(&V)");
        add("Toggle Page View", "ページ印刷ビュー切替");
        add("Add Linked Top-&X / Right-Y Layer", "上X・右Y連動レイヤーを追加(&X)");
        add("Add Linked Top-X / Right-Y Layer", "上X・右Y連動レイヤーを追加");
        add("Add &Zoom Inset Layer", "拡大インセットレイヤーを追加(&Z)");
        add("Add Zoom Inset Layer", "拡大インセットレイヤーを追加");
        add("&Frame Presets", "軸フレームプリセット(&F)");
        add("Frame Presets", "軸フレームプリセット");
        add("&Box Frame (Inward Ticks)", "ボックス枠・内向き目盛り (ISIJ標準)(&B)");
        add("Box Frame (Inward Ticks)", "ボックス枠・内向き目盛り (ISIJ標準)");
        add("Box Frame (&Plain Top/Right)", "ボックス枠・上右目盛りなし(&P)");
        add("Box Frame (Plain Top/Right)", "ボックス枠・上右目盛りなし");
        add("&Open L-Frame", "オープンL字枠 (上右軸なし)(&O)");
        add("Open L-Frame", "オープンL字枠 (上右軸なし)");
        add("&Zero-Crossed Axes", "原点交差軸 (X=0, Y=0)(&Z)");
        add("Zero-Crossed Axes", "原点交差軸 (X=0, Y=0)");
        add("Toggle Graph Mini-Tool&bar", "グラフミニツールバー表示(&B)");
        add("Toggle Graph Mini-Toolbar", "グラフミニツールバー表示");

        // Phase 4: Additional Gadgets
        add("&Differentiate Gadget...", "微分ガジェット (ROI)(&D)...");
        add("Differentiate Gadget...", "微分ガジェット (ROI)...");
        add("&Intersect Gadget...", "交点検出ガジェット (ROI)(&I)...");
        add("Intersect Gadget...", "交点検出ガジェット (ROI)...");
        add("&2D Integration Gadget (Matrix/Contour)...", "2次元積分ガジェット (行列・等高線)(&2)...");
        add("2D Integration Gadget (Matrix/Contour)...", "2次元積分ガジェット (行列・等高線)...");

        // Phase 4: Piecewise, Robust & Quantile, Compare Datasets, MLR
        add("&Piecewise Linear Fit (Breakpoint)...", "折れ線・区分線形フィット (変化点検出)(&P)...");
        add("Piecewise Linear Fit (Breakpoint)...", "折れ線・区分線形フィット (変化点検出)...");
        add("&Robust && Quantile Linear Fit...", "ロバスト・分位点線形フィット(&R)...");
        add("Robust & Quantile Linear Fit...", "ロバスト・分位点線形フィット...");
        add("Compare &Datasets (F-Test / AIC)...", "データセット間モデル比較 (F検定 / AIC)(&D)...");
        add("Compare Datasets (F-Test / AIC)...", "データセット間モデル比較 (F検定 / AIC)...");
        add("&Multiple Linear Regression (MLR)...", "重回帰分析 (MLR)(&M)...");
        add("Multiple Linear Regression (MLR)...", "重回帰分析 (MLR)...");

        // Phase 4: Signal Processing Additions
        add("Savitzky-Golay Smooth &Derivative...", "Savitzky-Golay平滑化微分(&D)...");
        add("Savitzky-Golay Smooth Derivative...", "Savitzky-Golay平滑化微分...");
        add("Peak &Envelope (Upper / Lower)...", "ピーク包絡線抽出 (上限・下限)(&E)...");
        add("Peak Envelope (Upper / Lower)...", "ピーク包絡線抽出 (上限・下限)...");
        add("&Decimate / Anti-Alias Resample...", "間引き・アンチエイリアス再サンプリング(&D)...");
        add("Decimate / Anti-Alias Resample...", "間引き・アンチエイリアス再サンプリング...");

        // Phase 4: Normality Test, Bland-Altman, Two-Way ANOVA & Graph Digitizer
        add("&Normality Test (Shapiro-Wilk / KS / AD)...",
            "正規性検定 (Shapiro-Wilk / KS / AD)(&N)...");
        add("Normality Test (Shapiro-Wilk / KS / AD)...",
            "正規性検定 (Shapiro-Wilk / KS / AD)...");
        add("&Bland-Altman Method Comparison...", "ブランド・アルトマン比較図(&B)...");
        add("Bland-Altman Method Comparison...", "ブランド・アルトマン比較図...");
        add("T&wo-Way ANOVA && Interaction Plot...", "二元配置分散分析・交互作用プロット(&W)...");
        add("Two-Way ANOVA & Interaction Plot...", "二元配置分散分析・交互作用プロット...");
        add("&Digitizer (Extract Data from Image)...", "デジタイザ (画像からデータ抽出)(&D)...");
        add("Digitizer (Extract Data from Image)...", "デジタイザ (画像からデータ抽出)...");
        add("Edit Column &Description", "列の説明を編集(&D)");
        add("S&et Column(s) As", "列の役割を設定(&E)");
        add("Clear Matrix", "行列をクリア");

        // Phase 5: Copy/Paste Graph Format, Batch Plotting, Style Presets & Menus
        add("&Style Presets", "スタイルプリセット(&S)");
        add("Style Presets", "スタイルプリセット");
        add("1-Click Publication & Slide Style Presets", "論文・発表スライド用ワンクリック・スタイルプリセット");
        add("&ISIJ Journal (85 mm, 600 DPI)", "鉄と鋼 / ISIJ 論文標準 (85 mm, 600 DPI)(&I)");
        add("ISIJ Journal (85 mm, 600 DPI)", "鉄と鋼 / ISIJ 論文標準 (85 mm, 600 DPI)");
        add("Apply 1-column ISIJ / Metallurgy journal style (85 mm, 600 DPI, inward ticks, Arial)",
            "鉄と鋼 / ISIJ 1段組論文スタイルを適用 (85 mm, 600 DPI, 内向き目盛り, Arial)");
        add("&Lab Slide (18 pt @ 140%, 83.3 mm)", "研究室スライド (18 pt @ 140%, 83.3 mm)(&L)");
        add("Lab Slide (18 pt @ 140%, 83.3 mm)", "研究室スライド (18 pt @ 140%, 83.3 mm)");
        add("Apply lab presentation slide style (83.3 mm, 12.9 pt Arial -> 18 pt at 140% in PowerPoint)",
            "研究室発表スライド標準を適用 (83.3 mm, 12.9 pt Arial -> PowerPoint 140%貼付で18 pt)");
        add("Lab Slide (83.3 mm, 18 pt @ 140%)", "研究室スライド (83.3 mm, 18 pt @ 140%)");
        add("&Presentation Slide (28 pt Arial)", "発表スライド (28 pt Arial)(&P)");
        add("Presentation Slide (28 pt Arial)", "発表スライド (28 pt Arial)");
        add("Apply high-visibility presentation slide style (185\xC3\x97"
            "185 mm, 28 pt Arial)",
            "発表スライド用の高視認様式を適用 (185\xC3\x97"
            "185 mm, 28 pt Arial)");
        add("Stacked &Bars", "積み上げ棒グラフ(&B)");
        add("Stacked Bars", "積み上げ棒グラフ");
        add("Plot vertical stacked bar chart", "選択したY列から積み上げ棒グラフを作成");
        add("&Polar Plot...", "極座標プロット(&P)...");
        add("Polar Plot...", "極座標プロット...");
        add("Plot polar (theta, radius) data from active worksheet",
            "アクティブなワークシートから極座標プロット (角度, 半径) を作成");
        add("Apply Style Preset to &Multiple Graphs...",
            "スタイルプリセットを複数グラフへ一括適用(&M)...");
        add("Apply Style Preset to Multiple Graphs...",
            "スタイルプリセットを複数グラフへ一括適用...");
        add("Plot Table Using &Template...", "テンプレートからワークシートを作図(&T)...");
        add("Plot Table Using Template...", "テンプレートからワークシートを作図...");
        add("Re-import &Source Data", "元データを再インポート(&S)");
        add("Re-import Source Data", "元データを再インポート");
        add("Quick &Linear Fit (No Dialog)", "クイック線形フィット (ダイアログなし)(&L)");
        add("Quick Linear Fit (No Dialog)", "クイック線形フィット (ダイアログなし)");
        add("&Graph", "グラフ(&G)");
        add("&Templates (.qpt)", "テンプレート (.qpt)(&T)");
        add("&More Gadgets", "その他のガジェット(&M)");
        add("More / &Legacy Presets", "その他 / 旧プリセット(&L)");
        add("Open Image as &Graph...", "画像をグラフとして開く(&G)...");
        add("Import Image to &Matrix...", "画像を行列へインポート(&M)...");

        add("Copy For&mat", "書式をコピー(&M)");
        add("Copy Format", "書式コピー");
        add("Copy graph format (fonts, axes, ticks, page size, curve widths, reference lines)",
            "グラフ書式をコピー (フォント・軸・目盛り・ページ寸法・線幅・基準線)");
        add("Paste F&ormat", "書式を貼り付け(&O)");
        add("Paste Format", "書式貼付");
        add("Paste copied graph format onto active graph", "コピーしたグラフ書式をアクティブグラフへ適用");
        add("Copied graph format from %1.", "%1 からグラフ書式をコピーしました。");
        add("Applied copied graph format to %1.", "%1 にグラフ書式を適用しました。");

        add("Duplicate (Batc&h Plotting)...", "一括複製プロット (バッチプロット)(&H)...");
        add("Duplicate (Batch Plotting)...", "一括複製プロット (バッチプロット)...");
        add("Duplicate (Batch Plotting)", "一括複製プロット (バッチプロット)");
        add("Batch-clone graph across multiple worksheets or Y columns",
            "テンプレートグラフを複数ワークシートまたはY列へ一括複製");
        add("Please create a template 2D graph first before batch plotting.",
            "バッチプロットを実行する前にテンプレートとなる2Dグラフを作成してください。");
        add("No worksheets are available in this project.",
            "プロジェクト内に利用可能なワークシートがありません。");
        add("Template Graph:", "テンプレートグラフ:");
        add("Select target worksheets to clone the template graph onto:",
            "テンプレートグラフを複製する対象ワークシートを選択:");
        add("Batch Across Worksheets", "複数ワークシートへ一括複製");
        add("Batch Across Y Columns", "Y列ごとに一括複製");
        add("Source Worksheet:", "元ワークシート:");
        add("Create one styled graph per checked Y column:",
            "チェックした各Y列ごとに書式付きグラフを作成:");

        // Phase 5: Legend, Curve & Context Menu Additions
        add("&New / Reconstruct Legend", "凡例の新規作成・再構築(&N)");
        add("New / Reconstruct Legend", "凡例の新規作成・再構築");
        add("Add or reconstruct legend", "凡例の追加または再構築");
        add("&Reconstruct Legend (Ctrl+L)", "凡例を再構築(&R) (Ctrl+L)");
        add("Reconstruct Legend (Ctrl+L)", "凡例を再構築 (Ctrl+L)");
        add("Legend &Columns", "凡例の段数(&C)");
        add("Legend Columns", "凡例の段数");
        add("&1 Column (Vertical)", "1列 (縦並び)(&1)");
        add("1 Column (Vertical)", "1列 (縦並び)");
        add("&%1 Columns (Horizontal)", "%1列 (横並び)(&%1)");
        add("%1 Columns (Horizontal)", "%1列 (横並び)");
        add("Change &Plot Type", "プロット形式を変更(&P)");
        add("Change Plot Type", "プロット形式を変更");
        add("Line + &Symbol", "線 + シンボル(&S)");
        add("Line + Symbol", "線 + シンボル");
        add("Go to &Worksheet", "ワークシートへ移動(&W)");
        add("Go to Worksheet", "ワークシートへ移動");

        // Phase 5: Gadgets, Fitting, Materials Science, Waterfall & Nonparametric / Multivariate Stats
        add("&Delta Cursor (C1-C2)...", "デルタカーソル (2点間差分 C1-C2)(&D)...");
        add("Delta Cursor (C1-C2)...", "デルタカーソル (2点間差分 C1-C2)...");
        add("Dual-crosshair Delta Cursor (dx, dy, slope, ratio, area, RMS)",
            "2点クロスヘア・デルタカーソル (dx, dy, 傾き, 比, 面積, RMS)");
        add("Binary &Logistic Regression...", "二値ロジスティック回帰(&L)...");
        add("Binary Logistic Regression...", "二値ロジスティック回帰...");
        add("&Arrhenius Plot (Activation Energy)...", "アレニウスプロット (活性化エネルギー)(&A)...");
        add("Arrhenius Plot (Activation Energy)...", "アレニウスプロット (活性化エネルギー)...");
        add("&Tauc Plot (Optical Bandgap)...", "Taucプロット (光学バンドギャップ)(&T)...");
        add("Tauc Plot (Optical Bandgap)...", "Taucプロット (光学バンドギャップ)...");
        add("&Fill Area Between Curves...", "曲線間領域の塗りつぶし(&F)...");
        add("Fill Area Between Curves...", "曲線間領域の塗りつぶし...");
        add("&Waterfall / Stack Offset...", "ウォーターフォール / スタックオフセット(&W)...");
        add("Waterfall / Stack Offset...", "ウォーターフォール / スタックオフセット...");
        add("Waterfall / Stack Offset", "ウォーターフォール / スタックオフセット");
        add("Reset Stac&k Offsets", "スタックオフセットをリセット(&K)");
        add("Reset Stack Offsets", "スタックオフセットをリセット");
        add("Reset Offsets", "オフセットをリセット");
        add("Offset Mode:", "オフセット方式:");
        add("Uniform Step (% of Span)", "等間隔ステップ (スパンに対する%)");
        add("Auto Gap (% above Previous Curve Peak)", "自動ギャップ (前曲線ピークからの余白%)");
        add("Y Offset / Gap:", "Yオフセット / 余白:");
        add("X Waterfall Shift:", "Xウォーターフォールシフト:");
        add("Normalize Each Curve to [0, 1] Before Stacking",
            "スタック前に各曲線を [0, 1] に正規化");
        add("&Weibull Probability Plot...", "ワイブル確率プロット(&W)...");
        add("Weibull Probability Plot...", "ワイブル確率プロット...");
        add("&Outlier Test (Grubbs / ESD / Dixon)...", "外れ値検定 (Grubbs / ESD / Dixon)(&O)...");
        add("Outlier Test (Grubbs / ESD / Dixon)...", "外れ値検定 (Grubbs / ESD / Dixon)...");
        add("&Nonparametric Tests (Mann-Whitney / Kruskal-Wallis / Auto)...",
            "ノンパラメトリック検定 (Mann-Whitney / Kruskal-Wallis / 自動)(&N)...");
        add("Nonparametric Tests (Mann-Whitney / Kruskal-Wallis / Auto)...",
            "ノンパラメトリック検定 (Mann-Whitney / Kruskal-Wallis / 自動)...");
        add("&Paired Comparison Plot (Before-After)...", "対応のある前後比較プロット (Before-After)(&P)...");
        add("Paired Comparison Plot (Before-After)...", "対応のある前後比較プロット (Before-After)...");
        add("&Correlation Matrix Heatmap...", "相関行列ヒートマップ(&C)...");
        add("Correlation Matrix Heatmap...", "相関行列ヒートマップ...");
        add("&Clustering (K-Means / Dendrogram)...", "クラスタリング (K-Means / デンドログラム)(&C)...");
        add("Clustering (K-Means / Dendrogram)...", "クラスタリング (K-Means / デンドログラム)...");

        // ToriGraph Rebranded Strings & Additional UI Menus
        add("ToriGraph", "ToriGraph");
        add("ToriGraph 2D Graph Template", "ToriGraph 2Dグラフテンプレート");
        add("ToriGraph 3D Surface Template", "ToriGraph 3Dサーフェステンプレート");
        add("ToriGraph Matrix Template", "ToriGraph 行列テンプレート");
        add("ToriGraph Table Template", "ToriGraph ワークシートテンプレート");
        add("ToriGraph currently does not support Origin import.",
            "この形式の古い Origin プロジェクト (.opj) は読み込めません。Origin 2018 以降の .opju はそのまま読み込めます (古い .opj は Origin で .opju または CSV / Excel に保存し直してください)。");
        add("ToriGraph does not support QtiPlot project files from versions later than 0.9.0.",
            "ToriGraphはバージョン0.9.0以降のQtiPlotプロジェクトファイルには対応していません。");
        add("ToriGraph does not support QtiPlot template files from versions later than 0.9.0.",
            "ToriGraphはバージョン0.9.0以降のQtiPlotテンプレートファイルには対応していません。");
        add("The file: <b>%1</b> is not a ToriGraph or Origin project file!",
            "ファイル <b>%1</b> は有効なToriGraphまたはOriginプロジェクトファイルではありません！");
        add("The file: <b>%1</b> is not a ToriGraph template file!",
            "ファイル <b>%1</b> は有効なToriGraphテンプレートファイルではありません！");
        add("Choose the location of the ToriGraph help folder!",
            "ToriGraphヘルプフォルダの場所を選択してください！");
        add("Do you want ToriGraph to guess the best position for the new layer?\n Warning: this will rearrange existing layers!",
            "新規レイヤーの最適な配置を自動推定しますか？\n警告: 既存レイヤーが再配置されます！");
        add("ToriGraph will now try to determine whether a new version of ToriGraph is available. Please modify your firewall settings in order to allow ToriGraph to connect to the internet.",
            "ToriGraphの新しいバージョンが利用可能か確認します。インターネット接続を許可してください。");
        add("There is a newer version of ToriGraph (%1) available for download. Would you like to download it now?",
            "新しいバージョンのToriGraph (%1) が利用可能です。今すぐダウンロードしますか？");

        // Round 2: High-DPI Page Copy, Interactive Stack Offsets, On-Canvas Baseline Anchors, Gadgets & Domain Analysis
        add("Cop&y Page as Image (High-DPI PNG)", "ページを高解像度画像としてコピー (300 DPI PNG)(&Y)");
        add("Copy Page as Image (High-DPI PNG)", "ページを高解像度画像としてコピー (300 DPI PNG)");
        add("Ctrl+Alt+C", "Ctrl+Alt+C");
        add("Copy active graph page to clipboard as a 300 DPI PNG image",
            "アクティブなグラフページを300 DPI PNG画像としてクリップボードへコピー");
        add("Copied %1 as 300 DPI PNG (%2 x %3 px) to clipboard.",
            "%1 を 300 DPI PNG画像 (%2 x %3 px) としてクリップボードにコピーしました。");
        add("2D &Cross-Section Profile Gadget (Image / Heatmap)",
            "2D断面プロファイルガジェット (画像 / ヒートマップ)(&C)");
        add("2D Cross-Section Profile Gadget (Image / Heatmap)",
            "2D断面プロファイルガジェット (画像 / ヒートマップ)");
        add("Interactive horizontal/vertical 2D cross-section profile gadget for heatmaps and images",
            "画像・ヒートマップ上の水平・垂直2D断面プロファイルを対話表示するガジェット");
        add("2D &Spatial Filter && Plane Leveling...",
            "2D空間フィルタ・平面レベリング(&S)...");
        add("2D Spatial Filter & Plane Leveling...",
            "2D空間フィルタ・平面レベリング...");
        add("Apply 2D Gaussian, median, Sobel/Laplacian filter or polynomial plane leveling to a matrix",
            "行列データへ2Dガウシアン・メディアン・Sobel/Laplacianフィルタや多項式平面レベリングを適用");
        add("&Fourier Self-Deconvolution (Peak Sharpening)...",
            "フーリエ自己デコンボリューション (ピーク先鋭化)(&F)...");
        add("Fourier Self-Deconvolution (Peak Sharpening)...",
            "フーリエ自己デコンボリューション (ピーク先鋭化)...");
        add("Sharpen overlapping spectral peaks via Kauppinen Fourier self-deconvolution",
            "Kauppinenフーリエ自己デコンボリューションにより重畳スペクトルピークを先鋭化");
        add("&EIS Nyquist && Bode Analyzer...",
            "EIS ナイキスト・ボード線図アナライザ(&E)...");
        add("EIS Nyquist & Bode Analyzer...",
            "EIS ナイキスト・ボード線図アナライザ...");
        add("Electrochemical Impedance Spectroscopy (Nyquist, Bode & Randles circuit fit)",
            "電気化学インピーダンス分光 (ナイキスト・ボード線図およびランドルス等価回路フィット)");
        add("Passing-&Bablok && Deming Regression...",
            "パッシング・バブロック／デミング回帰 (測定法比較)(&B)...");
        add("Passing-Bablok & Deming Regression...",
            "パッシング・バブロック／デミング回帰 (測定法比較)...");
        add("Method-comparison regression with errors in both X and Y (Passing-Bablok & Deming)",
            "X・Y両軸に誤差を含む測定法比較回帰 (パッシング・バブロック／デミング回帰)");
        add("Interactive Waterfall / Spectra Stacker",
            "対話型ウォーターフォール / スペクトルスタック設定");
        add("Y Offset (%):", "Yオフセット (%):");
        add("X Offset (%):", "Xオフセット (%):");
        add("Normalize [0, 1] (Scale Each Curve to Unit Span)",
            "[0, 1] 正規化 (各曲線を単位スパンへスケール)");
        add("Auto-Rescale Axes (Individual Autoscale)",
            "軸の自動再スケール (個別オートスケール)");
        add("Reverse Curve Order (Top-to-Bottom)",
            "曲線の積上げ順序を反転 (上から下へ)");
        add("Live Preview on Canvas", "キャンバス上でライブプレビュー");
        add("Reset (0%, 0%)", "リセット (0%, 0%)");
        add("User Anchors (Linear)", "ユーザーアンカー点 (線形補間)");
        add("User Anchors (Cubic Spline)", "ユーザーアンカー点 (3次スプライン)");
        add("Pick / Drag Anchors on Graph", "グラフ上でアンカー点を選択・ドラッグ");
        add("Stop Picking Anchors (Active)", "アンカー選択・ドラッグを終了 (実行中)");
        add("Clear Anchors", "アンカーをクリア");
        add("Click on graph: add anchor (snaps to curve; Shift+Click for free Y) | Drag: move | Right-click / Delete: remove",
            "グラフをクリック: アンカー追加 (曲線へスナップ / Shift+クリックで自由Y配置) | ドラッグ: 移動 | 右クリック・Delete: 削除");

        // Slice 2: OriginPro GraphInspectorDock, Per-MultiLayer Undo/Redo & Column Drop
        add("Resize Page", "ページサイズ変更");
        add("Link Right-Y Axis", "右Y軸リンクを追加");
        add("Link Top-X Axis", "上X軸リンクを追加");
        add("1:1 Linear Link (x)", "1:1 線形リンク (x)");
        add("Arrhenius: K -> 1000/T", "アレニウス変換: K → 1000/T");
        add("Arrhenius: °C -> 1000/T", "アレニウス変換: °C → 1000/T");
        add("Inverse Arrhenius: 1000/T -> °C", "逆アレニウス変換: 1000/T → °C");
        add("Temperature (°C)", "温度 (°C)");
        add("Custom Formula...", "カスタム変換式...");
        add("Link Top-X Formula", "上X軸の変換式設定");
        add("Formula in terms of bottom X (variable 'x'):", "下X軸に対する変換式 (変数 'x'):");
        add("Toggle Axis Visibility", "軸の表示切替");
        add("Change Tick Direction", "目盛りの向きを変更");
        add("Toggle Axis Grid", "軸グリッドの表示切替");
        add("Axis Tick Font Size", "目盛り数字のフォントサイズ変更");
        add("Change Curve Plot Style", "曲線のプロット形式を変更");
        add("Select Series Color", "系列の色を選択");
        add("Change Series Color", "系列の色を変更");
        add("Change Line Pen Style", "線種を変更");
        add("Change Symbol Shape", "シンボル形状を変更");
        add("Assign Curve to Right-Y", "曲線を右Y軸に割当");
        add("Apply Y-Offset Stack", "Yオフセット積上げを適用");
        add("Place Legend", "凡例の配置を変更");
        add("Change Legend Frame", "凡例枠を変更");
        add("Edit Annotation Text", "注釈テキストを編集");
        add("Edit Legend Header", "凡例ヘッダーを編集");
        add("Change Legend Font Size", "凡例フォントサイズを変更");
        add("(No Active Graph)", "(グラフ未選択)");
        add("Page: %1", "ページ: %1");
        add("[Linked]", "[リンク]");
        add("Curves (%1)", "曲線 (%1)");
        add("Annotations (%1)", "注釈 (%1)");
        add("Text: %1", "テキスト: %1");
        add("Arrow %1", "矢印・線 %1");
        add("Image: %1", "画像: %1");
        add("Toggle Curve Visibility", "曲線の表示切替");
        add("Toggle Legend Visibility", "凡例の表示切替");
        add("Add Curve(s) to Layer", "レイヤーに曲線を追加");
        add("Plot Style:", "プロット形式:");
        add("Y Columns (multi-select):", "Y列 (複数選択可):");
        add("Assign to Right-Y Axis", "右Y軸に割り当て");
        add("Add Selected Curve(s)", "選択した曲線を追加");
        add("(Default Table X)", "(表の既定X列)");
        add("Add Curve(s)", "曲線を追加");
        add("Reorder Curve", "曲線の順序変更");
        add("Remove Legend", "凡例を削除");
        add("Remove Annotation", "注釈を削除");
        add("(No Curve Selected)", "(曲線未選択)");
        add("Text Annotation", "テキスト注釈");
        add("Graph Legend", "グラフ凡例");
        add("Text:", "テキスト:");
        add("Header:", "ヘッダー:");
        add("Modify Axis Scale", "軸スケールを変更");
        add("Set Axis Break", "軸の途中省略を設定");
        add("Pin Inspector Open (Prevent Auto-Collapse)",
            "インスペクタをピン留め固定 (自動折りたたみを抑止)");
        add("Collapse to Icon Rail (Ctrl+Shift+I)", "アイコンレールへ折りたたむ (Ctrl+Shift+I)");
        add("Expand Graph Inspector (Ctrl+Shift+I)", "グラフインスペクタを展開 (Ctrl+Shift+I)");
        add("Page & Layer Properties", "ページ・レイヤー設定");
        add("Axis Scale & Title Properties", "軸スケール・ラベル設定");
        add("Curve & Series Style Properties", "曲線・系列スタイル設定");
        add("Legend & Annotation Properties", "凡例・注釈設定");
        add("+ Add", "＋ 追加");
        add("Add Curve(s) from Worksheet", "ワークシートから曲線を追加");
        add("Move Selected Curve Up", "選択した曲線を上へ移動");
        add("Move Selected Curve Down", "選択した曲線を下へ移動");
        add("Remove Selected Object", "選択したオブジェクトを削除");
        add("Activate or create a 2D graph to inspect its layers, axes, curves, and legend.",
            "2Dグラフを作成または選択すると、レイヤー・軸・曲線・凡例をここで直接編集できます。");
        add("1-Click Publication & Slide Presets", "ワンクリック論文・スライドプリセット");
        add("ISIJ Journal (85mm)", "ISIJ論文 (85mm)");
        add("Presentation Slide (28pt)", "発表スライド (28pt)");
        add("Page Size & View", "ページ寸法・表示");
        add("DPI:", "解像度 (DPI):");
        add("Print Page View", "印刷ページ表示");
        add("Page Zoom:", "ページ倍率:");
        add("Frame, Linked Axes & Panels", "枠・リンク軸・パネル番号");
        add("+ Link Right-Y Axis", "+ 右Y軸リンク");
        add("+ Link Top-X Axis (Arrhenius)", "+ 上X軸リンク (アレニウス等)");
        add("Clipboard & Export", "クリップボード・出力");
        add("Show Selected Axis", "この軸を表示");
        add("Scale & Axis Break", "スケール範囲・途中省略");
        add("Min:", "最小:");
        add("Max:", "最大:");
        add("Step (0=Auto):", "刻み (0=自動):");
        add("Scale:", "スケール:");
        add("Axis Break (Omit Middle Range)", "軸の途中省略 (Break)");
        add("Break Start:", "省略開始:");
        add("Break End:", "省略終了:");
        add("Ticks & Grids", "目盛り・グリッド");
        add("Ticks:", "目盛りの向き:");
        add("Axis Label & Font (Arial)", "軸タイトル・フォント (Arial)");
        add("Enter axis label...", "軸タイトルを入力...");
        add("Greek", "ギリシャ");
        add("Insert Greek Symbol", "ギリシャ文字を挿入");
        add("Append Scientific Unit", "単位を追記");
        add("Label Size:", "ラベル pt:");
        add("Tick Numbers:", "目盛り数字 pt:");
        add("Line Appearance", "線の外観");
        add("Color:", "色:");
        add("Line Width:", "線幅:");
        add("Line Style:", "線種:");
        add("Dash", "破線");
        add("Symbol & Axis Binding", "シンボル・軸割当");
        add("Shape:", "形状:");
        add("Circle", "丸 (○/●)");
        add("Cross (+)", "十字 (+)");
        add("X-Cross (x)", "斜め十字 (×)");
        add("Star", "星 (★)");
        add("Symbol Size:", "シンボルサイズ:");
        add("Quick Error Bars", "クイックエラーバー");
        add("Adjacent Col", "隣接列");
        add("Std Dev (SD)", "標準偏差 SD");
        add("5%", "5%");
        add("Group Series Style Scheme", "全曲線の配色・シンボルスキーム");
        add("B/W (Filled / Hollow)", "白黒(塗り／白抜き)");
        add("Cool -> Warm Step", "寒色→暖色ステップ");
        add("Origin Default", "Origin既定");
        add("Non-Destructive Y-Offset Stack", "非破壊 Yオフセット積上げ");
        add("Vertical Gap:", "縦オフセット間隔:");
        add("Stack Curves", "積上げ適用");
        add("1-Click Legend Placement", "凡例のワンクリック配置");
        add("Legend Frame & Typography (Arial)", "凡例枠・フォント設定 (Arial)");
        add("Frame:", "枠スタイル:");
        add("Borderless", "枠なし");
        add("Black Box + White Fill", "黒枠＋白背景");
        add("Columns:", "列数:");
        add("Font Size:", "文字サイズ:");
        add("&Graph Inspector Dock", "グラフインスペクタ(&G)");
        add("Toggle Tabbed Workspace View", "タブ表示ワークスペースを切替");
        add("Apply Frame Preset", "枠プリセットを適用");
        add("Apply ISIJ Style Preset", "ISIJ論文プリセットを適用");
        add("Apply Presentation Slide Preset", "発表スライドプリセットを適用");
        add("Apply Compact Journal Preset", "コンパクト論文プリセットを適用");
        add("&Undo %1", "元に戻す: %1(&U)");
        add("&Redo %1", "やり直し: %1(&R)");
        add("Drop Error Bars", "エラーバーをドロップ追加");
        add("Replace Plotted Columns", "プロット列を置換");
        add("Drop Columns onto Graph", "列をグラフへドロップ追加");
        add("Toggle Graph Inspector Dock (Ctrl+Shift+I)",
            "グラフインスペクタの表示/折りたたみ切替 (Ctrl+Shift+I)");
        add("Inspector", "インスペクタ");
        add("Square 70%", "正方形枠");
        add("Apply 70% Square Inner Canvas Frame", "内側キャンバスを70%正方形枠に整える");
        add("Apply 70% Square Canvas Frame", "70%正方形枠を適用");
        add("Page Size, DPI, Font & Zoom — Click for Quick Presets",
            "ページ寸法・DPI・フォント・表示倍率 — クリックでクイックプリセット");
        add("Up/Down to navigate  ·  Enter to run  ·  Esc to close",
            "↑/↓ で選択  ·  Enter で実行  ·  Esc で閉じる");
        add(" [R-Y]", " [右Y]");
        add("[R-Y]", "[右Y]");
        add("Insert %1", "%1 を挿入");
        add("Insert Common Unit", "よく使う単位を挿入");
        add("\xCE\xB1 (alpha)", "\xCE\xB1 (アルファ)");
        add("\xCE\xB2 (beta)", "\xCE\xB2 (ベータ)");
        add("\xCE\xB3 (gamma)", "\xCE\xB3 (ガンマ)");
        add("\xCE\xB4 (delta)", "\xCE\xB4 (デルタ)");
        add("\xCE\x94 (Delta)", "\xCE\x94 (デルタ大)");
        add("\xCE\xB5 (epsilon)", "\xCE\xB5 (イプシロン)");
        add("\xCE\xB8 (theta)", "\xCE\xB8 (シータ)");
        add("\xCE\xBB (lambda)", "\xCE\xBB (ラムダ)");
        add("\xCE\xBC (mu)", "\xCE\xBC (ミュー)");
        add("\xCF\x81 (rho)", "\xCF\x81 (ロー)");
        add("\xCF\x83 (sigma)", "\xCF\x83 (シグマ)");
        add("\xCF\x84 (tau)", "\xCF\x84 (タウ)");
        add("\xCF\x86 (phi)", "\xCF\x86 (ファイ)");
        add("\xCE\xA9 (Omega)", "\xCE\xA9 (オメガ)");
        add("No unmasked points in ROI", "ROI内にマスク解除データ点がありません");
        add("0 masked points in ROI", "ROI内のマスク点数: 0");
        add("Region Stats", "領域統計");
        add("Need more points in ROI", "ROI内のデータ点が不足しています");
        add("Quick Fit", "クイックフィット");
        add("Quick Sigmoidal", "クイックシグモイド");
        add("Need >= 4 points in ROI", "ROI内に4点以上のデータが必要です");
        add("FFT Preview", "FFTプレビュー");
        add("Rise Time", "立ち上がり時間");
        add("Change Curve Range", "曲線範囲の変更");
        add("Keep Fitted Curve on Graph", "フィット曲線をグラフに保持");
        add("Toggle Error Bars", "エラーバーの切替");
        add("Header:", "ヘッダー:");
        add("Fill Between Curves", "曲線間の塗りつぶし");
        add("Auto Scale", "自動スケール");
        add("Add Timestamp", "タイムスタンプを追加");
        add("Paste Enrichment", "装飾の貼り付け");
        add("Curve Full Range", "曲線の全範囲表示");
        add("Modify Axis", "軸の変更");
        add("Modify Axis Break", "軸ブレークの変更");
        add("Add Shaded Band", "シェード帯を追加");
        add("Modify Grid", "グリッドの変更");
        add("Arrhenius: °C -> 1000/T", "アレニウス変換: °C -> 1000/T");
        add("Inverse Arrhenius: 1000/T -> °C", "逆アレニウス変換: 1000/T -> °C");
        add("Temperature (°C)", "温度 (°C)");
        add("Reconstruct Legend", "凡例を再構成");
        add("Quick Troughs", "クイック谷検出");
        add("Abs Area", "絶対値面積");
        add("Dom Freq", "主周波数");
        add("Peak Mag", "ピーク振幅");
        add("10-90% Rise", "10-90% 立ち上がり");
        add("90-10% Fall", "90-10% 立ち下がり");
        add("Stamp ΔX/ΔY Callout on Graph", "ΔX/ΔY 注釈をグラフに貼付");
        add("Stamp Callout on Graph", "注釈をグラフに貼付");
        add("Stamp Profile Readout on Graph", "プロファイル値をグラフに貼付");

        // Phase 8: Ribbon ToolBar, Worksheet Quick Bar, Smart Header, Scoped Format Paste & Merge Graphs
        add("Modern Ribbon", "モダンリボン");
        add("ISIJ 85mm", "ISIJ 85mm 様式");
        add("28pt Slide", "28pt スライド");
        add("Merge Graphs", "グラフ結合");
        add("Merge Multiple Open Graph Windows into Multi-Panel Figure",
            "複数のグラフウィンドウを1枚の複合パネル図に結合");
        add("Paste Format &Special", "形式を選択して書式貼り付け(&S)");
        add("Paste Format Special", "形式を選択して書式貼り付け");
        add("Paste copied graph style (fonts, colors, symbols, frame & ticks) onto active graph",
            "コピーしたグラフ書式(フォント・色・シンボル・枠・目盛り)を現在のグラフへ貼り付けます");
        add("Paste &Fonts Only", "フォントのみ貼り付け(&F)");
        add("Paste Fonts Only", "フォントのみ貼り付け");
        add("Paste only font family, sizes, weights, and alignments onto active graph",
            "フォント種類・サイズ・太さ・配置のみを現在のグラフへ貼り付けます");
        add("Paste &Colors && Symbols Only", "色・シンボルのみ貼り付け(&C)");
        add("Paste Colors & Symbols Only", "色・シンボルのみ貼り付け");
        add("Paste only curve pens, symbol styles/sizes, and colors onto active graph",
            "曲線の線種・色・シンボルスタイルとサイズのみを現在のグラフへ貼り付けます");
        add("Paste &Axis Scales Only", "軸スケールのみ貼り付け(&A)");
        add("Paste Axis Scales Only", "軸スケールのみ貼り付け");
        add("Paste axis min/max ranges, tick steps, log10 scale, and numeric formats",
            "軸の最小/最大範囲・刻み幅・対数スケール・数値書式のみを貼り付けます");
        add("Paste Format to All &Graphs in Folder", "フォルダ内の全グラフへ書式貼り付け(&G)");
        add("Paste Format to All Graphs in Folder", "フォルダ内の全グラフへ書式貼り付け");
        add("Apply copied graph format to every 2D graph window in the current folder",
            "現在のフォルダ内のすべての2Dグラフへコピーした書式を一括適用します");
        add("Paste Graph Format", "グラフ書式の貼り付け");
        add("Paste Graph Format to Folder", "フォルダ内のグラフへ書式貼り付け");
        add("Applied copied graph format to %1 graphs in folder.",
            "フォルダ内の %1 個のグラフに書式を適用しました。");
        add("&Merge Graph Windows...", "グラフウィンドウを結合(&M)...");
        add("Merge Graph Windows...", "グラフウィンドウを結合...");
        add("Merge Graph Windows", "グラフウィンドウを結合");
        add("Merge 2 or more open graph windows into a publication multi-panel figure (a)(b)(c)(d)",
            "複数の2Dグラフウィンドウを (a)(b)(c)(d) パネル付きの複合図に結合します");
        add("At least two 2D graph windows are required to merge into a multi-panel figure.",
            "複合パネル図に結合するには、少なくとも2つの2Dグラフウィンドウが必要です。");
        add("Check and order the graph windows to merge into a new multi-panel page:",
            "新しい複合パネル図に結合するグラフを選択し、順序を並べてください:");
        add("Move Up", "上へ移動");
        add("Move Down", "下へ移動");
        add("Select All", "すべて選択");
        add("Clear All", "選択解除");
        add("Multi-Panel Layout && Publication Style", "複合パネル配置・論文様式");
        add("Multi-Panel Layout & Publication Style", "複合パネル配置・論文様式");
        add("Rows:", "行数:");
        add("Grid Arrangement:", "グリッド配置:");
        add("Keep Source Graph Styles", "元のグラフ様式を維持");
        add("ISIJ 1-Column (85 mm width)", "ISIJ 1段組 (幅 85 mm)");
        add("ISIJ 2-Column (175 mm width)", "ISIJ 2段組 (幅 175 mm)");
        add("Style Preset:", "様式プリセット:");
        add("Add panel labels (a), (b), (c)...", "パネル記号 (a), (b), (c)... を付与");
        add("Outside Top-Left", "枠外 左上");
        add("Inside Top-Left", "枠内 左上");
        add("Panel Labels:", "パネル記号:");
        add("Hide inner shared axis titles && tick labels", "内側の共有軸タイトル・目盛り数字を非表示");
        add("Hide inner shared axis titles & tick labels", "内側の共有軸タイトル・目盛り数字を非表示");
        add("Please select at least two graph windows to merge.",
            "結合するグラフウィンドウを2つ以上選択してください。");
        add("Apply high-visibility presentation slide style (185\xC3\x97"
            "185 mm, 28 pt Arial)",
            "発表スライド用の高視認様式を適用 (185\xC3\x97"
            "185 mm, 28 pt Arial)");
        add("Apply ISIJ International 1-column style (85\xC3\x97"
            "85 mm, 600 DPI, Arial)",
            "ISIJ International 1段組様式を適用 (85\xC3\x97"
            "85 mm, 600 DPI, Arial)");
        add("Remove Curve", "曲線を削除");
        add("Clear Axis Title", "軸タイトルを消去");
        add("Clear Title", "タイトルを消去");
        add("Remove Reference Line", "基準線を削除");
        add("Edit Annotation", "注釈を編集");
        add("Legend &Properties...", "凡例のプロパティ(&P)...");
        add("Legend Properties...", "凡例のプロパティ...");
        add("Change Legend Columns", "凡例の列数を変更");
        add("Edit &Text / Properties...", "テキスト編集・プロパティ(&T)...");
        add("Edit Text / Properties...", "テキスト編集・プロパティ...");
        add("Change Curve Plot Type", "曲線のプロット形式を変更");
        add("Add Curves from Worksheet", "ワークシートから曲線を追加");
        add("Added %1 column(s) from %2 to %3.", "%2 から %1 列を %3 へ追加しました。");
        add("You need at least four columns for this operation!",
            "この操作には少なくとも4つの列が必要です。");

        // Worksheet Quick Bar & Multi-Line Column Header Metadata
        add("Set selected column(s) as X (Abscissa)", "選択列を X(横軸)に設定");
        add("Set selected column(s) as Y (Ordinate)", "選択列を Y(縦軸)に設定");
        add("Set selected column(s) as Y Error", "選択列を Y誤差(±y誤差)に設定");
        add("Set selected column(s) as X Error", "選択列を X誤差(±x誤差)に設定");
        add("Set selected column(s) as Z", "選択列を Z に設定");
        add("Label", "ラベル");
        add("Set selected column(s) as Text Label", "選択列をテキストラベルに設定");
        add("Disregard", "無視");
        add("Set selected column(s) as Disregard (None)", "選択列をプロット対象外(無視)に設定");
        add("Column formula (e.g. col(\"1\")*2) - Enter to apply",
            "列の数式(例: col(\"1\")*2)— Enter で適用");
        add("Enter formula for active column and press Enter to recalculate",
            "現在の列の数式を入力し、Enter キーで再計算します");
        add("Line+Sym", "線+シンボル");
        add("Plot Line + Symbol graph (uses selected Y or all Y columns)",
            "線＋シンボルグラフを作成(選択Y列、未選択時は全Y列)");
        add("Scatter", "散布図");
        add("Plot Scatter graph (uses selected Y or all Y columns)",
            "散布図を作成(選択Y列、未選択時は全Y列)");
        add("Line", "折れ線");
        add("Plot Line graph (uses selected Y or all Y columns)",
            "折れ線グラフを作成(選択Y列、未選択時は全Y列)");
        add("+ Add to Graph", "+ グラフへ追加");
        add("Add selected Y column(s) (or all Y columns) to most recent graph",
            "選択Y列(未選択時は全Y列)を直近のグラフへ追加");
        add("Column Props \xE2\x96\xB8", "列設定 \xE2\x96\xB8");
        add("Toggle Column Description & Type Side Panel",
            "列の説明・プロパティパネルの表示切替");
        add("%1: set column plot designation", "%1: 列のプロット役割を変更");
        add("Long Name:", "長い名前:");
        add("Units:", "単位:");
        add("Comments:", "コメント:");
        add("Double-click header to edit Long Name, Units & F(x)=",
            "ヘッダーをダブルクリックして長い名前・単位・F(x)= を編集");
        add("Search", "検索");
        add("Focus", "集中");
        add("Tabs", "タブ");
        add("Split", "分割");
        add("Free View", "自由配置");
        add("More", "他");
        add("More column plot roles (\xC2\xB1xErr, Z, Label, Disregard)",
            "その他の列役割(\xC2\xB1x誤差・Z・ラベル・無視)");
        add("+ Add", "+追加");
        add("Col Props", "列設定");
        add("Panel View", "パネル表示");
        add("28pt Slide", "28pt スライド");
        add("[\xC2\xB1xErr]", "[\xC2\xB1x誤差]");
        add("[\xC2\xB1yErr]", "[\xC2\xB1y誤差]");
        add("\xC2\xB1xErr", "\xC2\xB1x誤差");
        add("\xC2\xB1yErr", "\xC2\xB1y誤差");
        add("Paste Layer", "レイヤーを貼り付け");
        add("Add Layer", "レイヤーの追加");
        add("Solid Line", "実線");
        add("Square Symbol", "四角 (\xE2\x96\xA0)");
        add("Box Inward", "四方枠");
        add("Open-L Frame", "L字枠");
        add("Trim Page", "余白調整");
        add("Palette", "パレット");
        add("Batch Plot", "一括作図");
        add("Copy High-DPI Image", "画像コピー");
        add("Export PDF", "PDFエクスポート");
        add("ISIJ 85mm Fig", "ISIJ 85mm図");
        add("Size & DPI", "寸法・DPI");

        // Phase B: Pure Japanese Materials Science, Inspector, Worksheet & MDI System Menu
        add("Differentiate", "数値微分");
        add("Two-Way ANOVA", "二元配置分散分析");
        add("Group ANOVA", "分散分析");
        add("Bland-Altman Plot", "ブランド・アルトマン比較図");
        add("Bland-Altman Method Agreement Plot", "ブランド・アルトマン一致度比較図");
        add("Passing-Bablok Fit", "パッシング・バブロック回帰");
        add("Passing-Bablok & Deming Method Comparison Regression",
            "パッシング・バブロック／デミング測定法比較回帰");
        add("One-Way ANOVA & Tukey / Dunnett Post-Hoc Comparison",
            "一元配置分散分析・多重比較検定");
        add("Two-Way Factorial ANOVA & Interaction Plot",
            "二元配置分散分析・交互作用プロット");

        // QMdiSubWindow system menu strings
        add("&Move", "移動(&M)");
        add("Move", "移動");
        add("Stay on &Top", "常に手前に表示(&T)");
        add("Stay on Top", "常に手前に表示");
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("QMdiSubWindow"), QStringLiteral("&Restore"),
                QString::fromUtf8("元のサイズに戻す(&R)"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("QMdiSubWindow"), QStringLiteral("Restore"),
                QString::fromUtf8("元のサイズに戻す"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("QMdiSubWindow"), QStringLiteral("&Size"),
                QString::fromUtf8("サイズ変更(&S)"));
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("QMdiSubWindow"), QStringLiteral("Size"),
                QString::fromUtf8("サイズ変更"));
        add("Mi&nimize", "最小化(&N)");
        add("Minimize", "最小化");
        add("Ma&ximize", "最大化(&X)");
        add("Maximize", "最大化");
        add("&Close", "閉じる(&C)");

        // ImageProfile & Tensile / 0.2% Yield & Inspector & Worksheet
        add("ImageProfile", "断面プロファイル");
        add("ImageProfileGraph", "断面プロファイル図");
        add("Tensile / 0.2% Yield", "引張・0.2%耐力");
        add("Tensile / 0.2% Yield...", "引張・0.2%耐力...");
        add("&Tensile / 0.2% Yield Analysis...", "引張・0.2%耐力解析(&T)...");
        add("Tensile / 0.2% Yield Analysis...", "引張・0.2%耐力解析...");
        add("Tensile / 0.2% Proof Stress Analyzer", "引張試験・0.2%耐力解析");
        add("Tensile Stress-Strain: Young's Modulus E, 0.2% Proof Stress & UTS",
            "引張応力-ひずみ解析: ヤング率 E・0.2%耐力・引張強さ (UTS)");
        add("Tensile stress-strain analysis: Young's modulus E, 0.2% proof stress, UTS & True Stress-Strain",
            "引張応力-ひずみ解析: ヤング率 E・0.2%耐力・引張強さ・真応力-真ひずみ変換");
        add("Show Tick Numbers", "目盛り数値を表示");
        add("Toggle Tick Numbers", "目盛り数値の表示切替");
        add("Unit Format", "単位表記");
        add("Unit Format:", "単位表記:");
        add("Name (Unit)", "物理量名 (単位)");
        add("Name / Unit (ISIJ)", "物理量 / 単位 (ISIJ)");
        add("Name [Unit]", "物理量名 [単位]");
        add("Change Axis Unit Format", "軸の単位表記を変更");
        add("Symbol Skip", "シンボルの間引き");
        add("Symbol Skip:", "シンボルの間引き:");
        add("Change Symbol Skip", "シンボルの間引き間隔を変更");
        add("Colorbar Title", "カラーバータイトル");
        add("Colorbar Title:", "カラーバータイトル:");
        add("Change Colorbar Title", "カラーバータイトルを変更");
        add("Insert Subscript (<sub>...</sub>)", "下付き文字を挿入 (<sub>...</sub>)");
        add("Insert Superscript (<sup>...</sup>)", "上付き文字を挿入 (<sup>...</sup>)");
        add("Insert Italic (<i>...</i>)", "斜体を挿入 (<i>...</i>)");
        add("Insert Materials Science Symbol", "材料科学記号・単位を挿入");
        add("Insert Materials Science Symbol / Unit", "材料科学記号・単位を挿入");
        add("Unicode Subscript (select digits or insert \xE2\x82\x82)",
            "Unicode下付き変換(選択数字を変換、未選択時は \xE2\x82\x82 を挿入)");
        add("Unicode Superscript (select digits or insert \xC2\xB2)",
            "Unicode上付き変換(選択数字を変換、未選択時は \xC2\xB2 を挿入)");
        add("Long Name", "長い名前");
        add("Units", "単位");
        add("Comments", "コメント");
        add("Units (e.g. MPa, %)", "単位 (例: MPa, %)");
        add("Symbols:", "記号・単位:");
        add("%1 of the %2 layer(s) to be removed contain %3 plotted item(s).\nAre you sure you want to delete these layers and their curves?", "削除対象の %2 レイヤーのうち %1 レイヤーに %3 個のプロット要素が含まれています。\nこれらのレイヤーと曲線を削除してもよろしいですか？");
        add("%1: invert", "%1: 逆行列");
        add("Inversion failed, the matrix is singular or ill-conditioned!", "逆行列の計算に失敗しました(特異行列または悪条件行列です)。");
        add("Matrix size (%1x%1) exceeds dense LU limit (4096).", "行列サイズ (%1x%1) が密行列LU分解の上限 (4096) を超えています。");
        add("Samples", "サンプル");
        add("Sample Project &Gallery...", "サンプルプロジェクトギャラリー(&G)...");
        add("Sample Project Gallery...", "サンプルプロジェクトギャラリー...");
        add("Sample Project Gallery", "サンプルプロジェクトギャラリー");
        add("Open Built-In Scientific Sample Project Gallery", "組み込みの材料科学・統計サンプルプロジェクト集を開く");
        add("&Work Hardening && Considere Analysis...", "加工硬化・Considèreくびれ解析(&W)...");
        add("Work Hardening & Considere Analysis...", "加工硬化・Considèreくびれ解析...");
        add("Work Hardening / Considere Analysis...", "加工硬化・Considèreくびれ解析...");
        add("Work Hardening", "加工硬化");
        add("True Stress-Strain, Hollomon n, Crussard-Jaoul & Considere Necking", "真応力-真ひずみ変換・Hollomon加工硬化指数 n・Kocks-Mecking・Considèreくびれ解析");
        add("&Kissinger / Thermal Desorption (TDS)...", "Kissinger昇温脱離 (TDS/DSC) 解析(&K)...");
        add("Kissinger / Thermal Desorption (TDS)...", "Kissinger昇温脱離 (TDS/DSC) 解析...");
        add("Kissinger TDS", "昇温脱離(TDS)");
        add("Kissinger Thermal Desorption (TDS / DSC) Activation Energy Analysis", "Kissinger法による昇温脱離 (TDS / DSC) 活性化エネルギー解析");
        add("&XRD Peak && Williamson-Hall Analyzer...", "XRDピーク・Williamson-Hall解析(&X)...");
        add("XRD Peak & Williamson-Hall Analyzer...", "XRDピーク・Williamson-Hall解析...");
        add("XRD / W-H", "XRD・W-H");
        add("Powder XRD K-alpha2 Stripping, Scherrer & Williamson-Hall Analysis", "粉末X線回折 Kα2除去・Scherrer結晶子径・Williamson-Hall格子ひずみ解析");
        add("&VASP Electronic Band && DOS Plotter...", "VASPバンド構造・状態密度 (DOS) 作図(&V)...");
        add("VASP Electronic Band & DOS Plotter...", "VASPバンド構造・状態密度 (DOS) 作図...");
        add("VASP Bands/DOS", "VASP電子状態");
        add("VASP EIGENVAL / DOSCAR Electronic Band Structure & DOS Plotter", "VASP (EIGENVAL / DOSCAR) 電子バンド構造・状態密度プロッタ");
        add("Export Python", "Python出力");
        add("Export Python Script", "Pythonスクリプト出力");
        add("Export Active Graph to Python Script...", "現在のグラフをPythonスクリプトとして出力...");
        add("Export Reproducible &Python Script...", "再現用Pythonスクリプトを出力(&P)...");
        add("Export Reproducible Python Script...", "再現用Pythonスクリプトを出力...");
        add("Export Reproducible Python Script — %1", "再現用Pythonスクリプト出力 — %1");
        add("Generate Reproducible Python Script from Active Graph", "アクティブなグラフを再現するPythonスクリプトを生成");
        add("Please open or select a Graph, Worksheet, or Matrix window first.", "先にグラフ、ワークシート、または行列ウィンドウを開くか選択してください。");
        add("Could not generate a Python script for the active window.", "アクティブなウィンドウからPythonスクリプトを生成できませんでした。");
        add("Generated Python script (%1 lines) and copied to clipboard.", "Pythonスクリプト(%1 行)を生成し、クリップボードにコピーしました。");
        add("Standalone reproducible ToriGraph SDK Python script for '%1' (copied to clipboard):", "'%1' を再現するスタンドアロン ToriGraph SDK Pythonスクリプト(クリップボードへコピー済み):");
        add("Copy to Clipboard", "クリップボードにコピー");
        add("Copied Python script to clipboard.", "Pythonスクリプトをクリップボードにコピーしました。");
        add("Save .py File...", ".py ファイルを保存...");
        add("Save Reproducible Python Script", "再現用Pythonスクリプトを保存");
        add("Python Scripts (*.py);;All Files (*)", "Pythonスクリプト (*.py);;すべてのファイル (*)");
        add("Saved Python script to %1", "Pythonスクリプトを保存しました: %1");
        add("Failed to write file:\n%1", "ファイルの書き込みに失敗しました:\n%1");
        add("Import %1", "%1 をインポート");
        add("Origin Unicode project", "Origin Unicode プロジェクト");
        add("Origin OPJU Format", "Origin OPJU 形式");
        add("Origin OPJU Import Error", "Origin OPJU インポートエラー");
        add("Failed to import Origin Unicode project (.opju):\n%1",
            "Origin Unicode プロジェクト (.opju) のインポートに失敗しました:\n%1");
        add("Import Origin OPJ / OPJU Project", "Origin OPJ / OPJU プロジェクトをインポート");
        add("Origin Projects (*.opj *.OPJ *.opju *.OPJU);;All Files (*)",
            "Origin プロジェクト (*.opj *.OPJ *.opju *.OPJU);;すべてのファイル (*)");
        add("Ctrl+K Search Commands...", "Ctrl+K コマンド検索...");
        add("Ctrl+K Search Commands", "Ctrl+K コマンド検索");
        add("Ready — Select columns to plot or press Ctrl+K to search commands",
            "準備完了 — 列を選択してグラフ作成、または Ctrl+K でコマンド検索");
        add("Ready — Press Ctrl+K to search commands",
            "準備完了 — Ctrl+K でコマンド検索");
        add("Worksheet: %1  (%2 rows × %3 cols)  —  Select columns & click Plot or press Ctrl+K",
            "ワークシート: %1  (%2行 × %3列)  —  列を選択してグラフ作成、または Ctrl+K でコマンド検索");
        add("2D Graph: %1  (Layers: %2 · Curves: %3)  —  Click curve/axis to edit in Inspector",
            "2Dグラフ: %1  (レイヤー: %2 · 曲線: %3)  —  曲線・軸をクリックしてインスペクタで編集");
        add("Matrix: %1  (%2 × %3)", "行列: %1  (%2 × %3)");
        add("Active: %1", "アクティブ: %1");
        add("Categories:", "カテゴリ軸:");
        add("Categories", "カテゴリ軸");
        add("(Numeric Ticks)", "(数値目盛り)");
        add("Custom (%1 labels)", "カスタム (%1 ラベル)");
        add("Change Category Labels", "カテゴリ軸ラベルを変更");
        add("Set Category Labels", "カテゴリ軸ラベルを設定");
        add("Edit Curve Style...", "曲線スタイルを編集...");
        add("Layer / Plot Properties...", "レイヤー・プロット設定...");
        add("Show Object", "オブジェクトを表示");
        add("Hide Object", "オブジェクトを非表示");
        add("Remove Object", "オブジェクトを削除");
        add("Plot Function", "関数プロット");
        add("Reset Axis Color", "軸の色をリセット");
        add("Polar %1", "極座標 %1");
        add("Materials Science Sample Gallery", "材料科学サンプルギャラリー");
        add("Select a built-in materials science dataset to generate a ready-to-analyze worksheet and publication plot:", "組み込みの材料科学データセットを選択し、解析用ワークシートと論文品質グラフを生成します:");
        add("Generate & Run Recommended Analysis", "データ生成＋推奨解析を実行");
        add("Generate Worksheet & Plot", "ワークシートとグラフを生成");
        add("Metallurgy / Mechanical", "金属材料・力学特性");
        add("Hydrogen Embrittlement / TDS", "水素脆化・昇温脱離 (TDS)");
        add("X-Ray Diffraction (XRD)", "X線回折 (XRD)");
        add("Diffusion / Kinetics", "拡散・速度論");
        add("Synchrotron CT / Local Strain", "放射光CT・局所ひずみ");
        add("Ceramics / Fracture Statistics", "セラミックス・破壊統計");
        add("AA7075-T6 Tensile Stress-Strain Curve", "AA7075-T6 引張応力-ひずみ曲線");
        add("Hydrogen TDS Spectra at 3 Heating Rates (100, 200, 400 °C/h)", "水素昇温脱離 (TDS) スペクトル(昇温速度 100, 200, 400 °C/h)");
        add("Peak-Aged Al-Zn-Mg Alloy XRD Pattern (Cu Kα1)", "ピーク時効 Al-Zn-Mg 合金 XRD パターン (Cu Kα1)");
        add("Hydrogen Interstitial Diffusivity in α-Fe", "α-Fe 中の格子間水素拡散係数");
        add("3D-XRD / Synchrotron CT Crack-Tip Local Strain Map", "3D-XRD / 放射光CT き裂先端局所ひずみマップ");
        add("Sintered Si3N4 4-Point Flexural Strength (JIS R 1625)", "焼結 Si3N4 4点曲げ強度ワイブル統計 (JIS R 1625)");
        add("Kissinger TDS / DSC Activation Energy Analyzer", "Kissinger 昇温脱離 (TDS / DSC) 活性化エネルギー解析");
        add("From Worksheet (β, Tp Columns)", "ワークシート列から指定 (β, Tp 列)");
        add("Auto-Extract Peaks from TDS Curves", "複数TDS曲線からピーク温度を自動抽出");
        add("Manual Input (β, Tp Lists)", "手動リスト入力 (β, Tp)");
        add("Heating Rate Column (β):", "昇温速度列 (β):");
        add("Peak Temperature Column (Tp):", "ピーク温度列 (Tp):");
        add("Heating Rate Unit:", "昇温速度の単位:");
        add("K/min (°C/min)", "K/min (°C/min)");
        add("K/h (°C/h)", "K/h (°C/h)");
        add("Peak Temperatures in Celsius (°C)", "ピーク温度を摂氏 (°C) として入力");
        add("Top Temperature Axis in Celsius (°C)", "上X軸の参照温度を摂氏 (°C) で表示");
        add("TDS Flux Columns (Select 2+ Rates):", "TDS放出速度列(2水準以上を選択):");
        add("Heating Rates (comma-separated):", "昇温速度リスト(カンマ区切り):");
        add("Peak Temperatures (comma-separated):", "ピーク温度リスト(カンマ区切り):");
        add("Peak Search Window Min T:", "ピーク探索 最小温度 T:");
        add("Peak Search Window Max T:", "ピーク探索 最大温度 T:");
        add("Kissinger analysis requires at least 2 distinct peak temperatures.", "Kissinger解析には少なくとも2つの異なるピーク温度が必要です。");
        add("Kissinger analysis requires at least 2 valid (heating rate, peak temperature) pairs.", "Kissinger解析には少なくとも2組の有効な(昇温速度, ピーク温度)データ対が必要です。");
        add("Kocks-Mecking Work-Hardening & Considere Necking Analyzer", "Kocks-Mecking 加工硬化・Considère くびれ解析");
        add("Savitzky-Golay Smoothing Window (odd points):", "Savitzky-Golay 平滑化窓幅(奇数点数):");
        add("Create Considere (θ & σt vs εt) Plot", "Considère 線図 (θ・σt vs εt) を作成");
        add("XRD Peak, Scherrer & Williamson-Hall Analyzer", "XRD ピーク・Scherrer結晶子径・Williamson-Hall 解析");
        add("Diffractogram Curve:", "回折プロファイル曲線:");
        add("2θ Column (deg):", "2θ 列 (deg):");
        add("Intensity Column:", "回折強度列:");
        add("Radiation Source:", "X線管球・線源:");
        add("X-Ray Wavelength λ (Å):", "X線波長 λ (Å):");
        add("Instrumental FWHM (deg):", "装置関数半値幅 FWHM (deg):");
        add("Scherrer Shape Factor K:", "Scherrer 形状因子 K:");
        add("Crystal System for (hkl) Indexing:", "(hkl) 指数付け結晶系:");
        add("None (Peak Table & W-H Only)", "なし(ピーク表・W-H図のみ)");
        add("Strip Cu Kα2 Doublet (Rachinger)", "Rachinger法による Kα2 二重線除去");
        add("Add Linked Top Bragg d-Spacing Axis (Å)", "上X軸にブラッグ面間隔 d (Å) リンク軸を追加");
        add("Create Williamson-Hall Plot (β cos θ vs 4 sin θ)", "Williamson-Hall プロット (β cos θ vs 4 sin θ) を作成");
        add("Please open a graph or numeric worksheet with XRD data first.", "先にXRDデータを含むグラフまたは数値ワークシートを開いてください。");
        add("No prominent Bragg diffraction peaks were detected on the curve.", "指定された曲線上に明瞭なブラッグ回折ピークが検出されませんでした。");
        add("VASP Electronic Structure Plotter (DOS & Band Structure)", "VASP 電子状態プロッタ(状態密度 DOS・バンド構造)");
        add("Load VASP DOSCAR / EIGENVAL File...", "VASP DOSCAR / EIGENVAL ファイルを読み込む...");
        add("DOS / PDOS", "状態密度 (DOS / PDOS)");
        add("Band Structure", "バンド構造");
        add("Energy Column (eV):", "エネルギー列 (eV):");
        add("Spin-Up / Total DOS Columns:", "全DOS / Spin-Up DOS 列:");
        add("Spin-Down DOS Columns (Optional):", "Spin-Down DOS 列(任意):");
        add("Fermi Energy E_F (eV):", "フェルミ準位 E_F (eV):");
        add("Vertical Energy Axis (for side-by-side Band + DOS)", "縦軸をエネルギーにする(バンド図横並び用)");
        add("Wavevector Distance Column (k):", "波数空間距離列 (k):");
        add("Band Eigenvalue Columns:", "バンド固有値列:");
        add("Energy Window Min (E - E_F, eV):", "表示エネルギー最小値 (E - E_F, eV):");
        add("Energy Window Max (E - E_F, eV):", "表示エネルギー最大値 (E - E_F, eV):");
        add("High-Symmetry k-Tick Positions:", "高対称k点 目盛り位置:");
        add("High-Symmetry k-Tick Labels:", "高対称k点 ラベル (Γ, X, M...):");
        add("Source worksheet is null.", "入力元ワークシートが選択されていません。");
        add("Energy column could not be resolved.", "エネルギー列を特定できませんでした。");
        add("At least one numeric DOS column is required.", "数値のDOS列を少なくとも1列選択してください。");
        add("Insufficient numeric rows for VASP DOS plot.", "VASP状態密度プロットに必要な有効データ行数が不足しています。");
        add("Wavevector k-distance column could not be resolved.", "波数距離 k 列を特定できませんでした。");
        add("At least one band eigenvalue column is required.", "バンド固有値列を少なくとも1列選択してください。");
        add("Insufficient k-points for VASP band structure plot.", "VASPバンド構造プロットに必要な k 点数が不足しています。");
        add("Failure probability, <i>P</i><sub>f</sub> (%)", "破壊確率, <i>P</i><sub>f</sub> (%)");
        add("Failure probability, <i>P</i><sub>f</sub> / %", "破壊確率, <i>P</i><sub>f</sub> / %");
        add("Maximum Likelihood (MLE — JIS R 1625 / ISO 20501)", "最尤法 (MLE — JIS R 1625 / ISO 20501 準拠)");
        add("Median Rank Regression (MRR — Benard)", "メジアンランク回帰法 (MRR — Benard近似)");
        add("Parameter Estimator:", "母数推定法:");
        add("Plotted Columns", "プロット列");
        add("Plotted Columns ▾", "プロット列 ▾");
        add("Plotted Columns...", "プロット列...");
        add("Peak-aged Al-5.6Zn-2.5Mg-1.6Cu (T6) engineering stress-strain curve (E = 71.2 GPa, σ0.2 ≈ 508 MPa, UTS ≈ 578 MPa) for 0.2% offset yield & Kocks-Mecking Considère analysis.", "ピーク時効 Al-5.6Zn-2.5Mg-1.6Cu (T6) 公称応力-ひずみ曲線(E = 71.2 GPa, σ0.2 ≈ 508 MPa, UTS ≈ 578 MPa)— 0.2%耐力および Kocks-Mecking・Considère くびれ解析用。");
        add("First-order Polanyi-Wigner thermal desorption spectra of H-charged martensitic steel with dislocation (26.8 kJ/mol) and TiC precipitate (58.4 kJ/mol) traps.", "水素チャージしたマルテンサイト鋼の1次 Polanyi-Wigner 昇温脱離 (TDS) スペクトル(転位トラップ 26.8 kJ/mol・TiC析出物トラップ 58.4 kJ/mol)。");
        add("Cu Kα1 (λ = 1.540598 Å) diffractogram (2θ = 20–85°) of fcc-Al broadened by D = 42 nm crystallites and ε = 1.85×10^-3 microstrain for Bragg / Williamson-Hall analysis.", "結晶子径 D = 42 nm および不均一格子ひずみ ε = 1.85×10^-3 により広がった fcc-Al の Cu Kα1 (λ = 1.540598 Å) X線回折プロファイル (2θ = 20–85°) — Bragg / Williamson-Hall 解析用。");
        add("Temperature-dependent lattice H diffusivity D(T) in α-Fe (320–880 K) with standard deviations for Arrhenius activation energy regression.", "α-Fe 中の格子間水素拡散係数 D(T) の温度依存性データ(320–880 K、標準偏差付き)— アレニウス活性化エネルギー回帰解析用。");
        add("128×128 2D matrix of local axial strain ε_yy(x, y) (%) over a 200×200 μm field around a crack tip and micro-void ahead of the plastic zone.", "き裂先端および塑性域前方の微小ボイド周囲 200×200 μm 視野における局所軸ひずみ ε_yy(x, y) (%) の 128×128 2次元行列。");
        add("30-specimen ceramic 4-point bending dataset (28 fractures + 2 right-censored runouts at 820 MPa; m ≈ 12.4, σ0 ≈ 785 MPa) for MRR / MLE Weibull probability analysis.", "セラミックス4点曲げ強度データ(30試験片:破断28本＋820 MPa 右側打ち切り2本；m ≈ 12.4, σ0 ≈ 785 MPa)— MRR / 最尤法 (MLE) ワイブル確率解析用。");
        add("30-specimen ceramic 4-point bending dataset (25 fractures + 5 right-censored runouts at 820 MPa; m ≈ 12.4, σ0 ≈ 785 MPa) for MRR / MLE Weibull probability analysis.", "セラミックス4点曲げ強度データ(30試験片:破断25本＋820 MPa 右側打ち切り5本；m ≈ 12.4, σ0 ≈ 785 MPa)— MRR / 最尤法 (MLE) ワイブル確率解析用。");
        add("30-specimen ceramic 4-point bending dataset (28 fractures + 5 right-censored runouts at 820 MPa; m ≈ 12.4, σ0 ≈ 785 MPa) for MRR / MLE Weibull probability analysis.", "セラミックス4点曲げ強度データ(30試験片:破断28本＋820 MPa 右側打ち切り5本；m ≈ 12.4, σ0 ≈ 785 MPa)— MRR / 最尤法 (MLE) ワイブル確率解析用。");
        add("HCP (Hexagonal Close-Packed)", "HCP (六方最密充填)");
        add("Hexagonal Close-Packed (HCP)", "六方最密充填 (HCP)");
        add("HCP", "HCP (六方晶)");
        add("Axial Ratio c/a (HCP):", "軸比 c/a (HCP):");
        add("HCP Axial Ratio c/a:", "HCP 軸比 c/a:");
        add("c/a Ratio (HCP):", "c/a 軸比 (HCP):");
        add("c/a Ratio:", "軸比 c/a:");
        add("Plot Mode:", "作図モード:");
        add("Considere (θ & σt vs εt)", "Considère 線図 (θ・σt vs εt)");
        add("Considère (θ & σt vs εt)", "Considère 線図 (θ・σt vs εt)");
        add("Kocks-Mecking (θ vs σt)", "Kocks-Mecking プロット (θ vs σt)");
        add("Create Kocks-Mecking (θ vs σt) Plot", "Kocks-Mecking プロット (θ vs σt) を作成");
        add("Set Custom Tick Labels", "カスタム目盛りラベルを設定");
        add("Set Weibull Probability Axis", "ワイブル確率軸を設定");
        add("Axial Ratio c/a (for HCP):", "軸比 c/a (HCP用):");
        add("HCP / Hexagonal (100, 002, 101...)", "HCP / 六方晶 (100, 002, 101...)");
        add("Kocks-Mecking Stage-III (\xCE\xB8 vs \xCF\x83t)",
            "Kocks-Mecking ステージIII (\xCE\xB8 vs \xCF\x83t)");
        add("Lock", "固定");
        add("Free", "自由変形");
        add("Lock Aspect Ratio", "縦横比を固定 (Lock Aspect Ratio)");
        add("1:1 Square", "1:1 正方形");
        add("4:3 Landscape", "4:3 横長");
        add("16:9 Wide", "16:9 ワイド");
        add("3:4 Portrait", "3:4 縦長");
        add("Free Resize", "自由リサイズ");
        add("Page Size, Aspect Ratio, DPI, Font & Zoom — Click for Quick Presets",
            "ページサイズ・縦横比・DPI・フォント・ズーム — クリックでクイック設定");
        add("Please activate a 2D Graph window first before adding a linked layer.",
            "リンク軸レイヤーを追加するには、まず 2D グラフウィンドウをアクティブにしてください。");
        add("Please activate a 2D Graph window first before adding a zoom inset layer.",
            "拡大インセットレイヤーを追加するには、まず 2D グラフウィンドウをアクティブにしてください。");
    }
}

} // namespace ApplicationWindowInternal

namespace {

static void sanitizeUiTableFont(QFont &f)
{
    const QString fam = f.family().trimmed();
    const bool isLegacyBitmapUiFont = fam.isEmpty()
            || fam.compare(QLatin1String("MS UI Gothic"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS PGothic"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS Gothic"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS Shell Dlg"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS Shell Dlg 2"), Qt::CaseInsensitive) == 0;
    if (isLegacyBitmapUiFont) {
        const QStringList families = QFontDatabase().families();
        if (families.contains(QStringLiteral("Yu Gothic UI"), Qt::CaseInsensitive))
            f.setFamily(QStringLiteral("Yu Gothic UI"));
        else if (families.contains(QStringLiteral("Segoe UI"), Qt::CaseInsensitive))
            f.setFamily(QStringLiteral("Segoe UI"));
    }
    const double curPt = (f.pointSizeF() > 0.0) ? f.pointSizeF() : double(f.pointSize());
    if (curPt < 12.0)
        f.setPointSizeF(12.0);
    f.setStyleStrategy(QFont::PreferAntialias);
}

} // anonymous namespace

void ApplicationWindow::initFonts()
{
    sanitizeUiTableFont(appFont);
    tableTextFont = appFont;
    tableHeaderFont = appFont;
    sanitizeUiTableFont(tableTextFont);
    sanitizeUiTableFont(tableHeaderFont);
    plotAxesFont = QFont("Arial", 14, QFont::Normal);
    plotNumbersFont = QFont("Arial", 12);
    plotLegendFont = QFont("Arial", 10);
    plotTitleFont = QFont("Arial", 14, QFont::Normal);

    plot3DAxesFont = QFont("Arial", 12, QFont::Normal);
    plot3DNumbersFont = QFont("Arial", 10);
    plot3DTitleFont = QFont("Arial", 14, QFont::Normal);
}

void ApplicationWindow::applyUserSettings(bool resetScriptingLang)
{
    updateAppFonts();
    if (resetScriptingLang)
        setScriptingLang(defaultScriptingLang);

    d_workspace.setBackground(workspaceColor);

    QPalette cg = qApp->palette();
    if (panelsColor.isValid())
        cg.setColor(QPalette::Base, QColor(panelsColor));
    qApp->setPalette(cg);

    QPalette panelPal = cg;
    if (panelsTextColor.isValid()) {
        panelPal.setColor(QPalette::Text, QColor(panelsTextColor));
        panelPal.setColor(QPalette::WindowText, QColor(panelsTextColor));
    }
    lv.setPalette(panelPal);
    results->setPalette(panelPal);
}

void ApplicationWindow::showColorPaletteEditorDialog()
{
    if (isNonInteractiveContext(this))
        return;
    Graph *targetGraph = nullptr;
    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("MultiLayer"))
        targetGraph = static_cast<MultiLayer *>(d_workspace.activeSubWindow())->activeGraph();

    if (Graph::showColorPaletteEditor(this, targetGraph))
        emit modified();
}

void ApplicationWindow::applyColorPaletteByAction()
{
    QAction *act = qobject_cast<QAction *>(sender());
    if (!act)
        return;
    QString palName = act->data().toString();
    if (palName.isEmpty())
        palName = act->text();

    if (d_workspace.activeSubWindow() && d_workspace.activeSubWindow()->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
        Graph *g = ml->activeGraph();
        if (g) {
            const QString before = ml->captureUndoSnapshot();
            g->applyColorPaletteByName(palName, false);
            ml->pushUndoSnapshot(tr("Apply Color Palette"), before);
            emit modified();
        }
    } else if (!isNonInteractiveContext(this)) {
        showUxHint(tr("Please select or activate a 2D graph window first."));
    }
}

void ApplicationWindow::applyAspectRatioByAction()
{
    QAction *act = qobject_cast<QAction *>(sender());
    if (!act)
        return;
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer")) {
        if (!isNonInteractiveContext(this))
            showUxHint(tr("Please select or activate a 2D graph window first."));
        return;
    }

    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    double ratio = act->data().toDouble();
    if (ratio <= 0.0) {
        if (isNonInteractiveContext(this))
            return;
        bool ok = false;
        double currentRatio = ml->targetAspectRatio() > 0.0 ? ml->targetAspectRatio() : 1.0;
        ratio = QInputDialog::getDouble(this, tr("Custom Graph Aspect Ratio"),
                                        tr("Aspect ratio (Width / Height):"), currentRatio, 0.2,
                                        5.0, 3, &ok);
        if (!ok || ratio <= 0.0)
            return;
    }
    const QString before = ml->captureUndoSnapshot();
    ml->applyAspectRatioPreset(ratio, true);
    ml->pushUndoSnapshot(tr("Graph Aspect Ratio"), before);
    if (actionKeepAspectRatio)
        actionKeepAspectRatio->setChecked(ml->keepAspectRatio());
    emit modified();
}

void ApplicationWindow::setGraphKeepAspectRatio(bool keep)
{
    if (!d_workspace.activeSubWindow() || !d_workspace.activeSubWindow()->inherits("MultiLayer")) {
        if (!isNonInteractiveContext(this))
            showUxHint(tr("Please select or activate a 2D graph window first."));
        return;
    }
    MultiLayer *ml = static_cast<MultiLayer *>(d_workspace.activeSubWindow());
    const QString before = ml->captureUndoSnapshot();
    ml->setKeepAspectRatio(keep);
    ml->pushUndoSnapshot(tr("Keep Aspect Ratio"), before);
    emit modified();
}

void ApplicationWindow::showPreferencesDialog()
{
    if (isNonInteractiveContext(this))
        return;
    ConfigDialog *cd = new ConfigDialog(this);
    cd->setAttribute(Qt::WA_DeleteOnClose);
    cd->setColumnSeparator(columnSeparator);
    cd->exec();

    const int effectiveLimit = qMax(1, undoLimit);
    if (d_project && d_project->undoStack())
        d_project->undoStack()->setUndoLimit(effectiveLimit);
    QSet<MultiLayer *> allMls;
    for (MyWidget *mw : windowsList()) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(mw))
            allMls.insert(ml);
    }
    for (MyWidget *mw : hiddenWindows) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(mw))
            allMls.insert(ml);
    }
    for (MyWidget *mw : outWindows) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(mw))
            allMls.insert(ml);
    }
    for (MultiLayer *ml : allMls) {
        if (ml && ml->undoStack())
            ml->undoStack()->setUndoLimit(effectiveLimit);
    }
}

void ApplicationWindow::setSaveSettings(bool autoSaving, int min)
{
    if (autoSave == autoSaving && autoSaveTime == min)
        return;

    autoSave = autoSaving;
    autoSaveTime = min;

    killTimer(savingTimerId);

    if (autoSave)
        savingTimerId = startTimer(autoSaveTime * 60000);
    else
        savingTimerId = 0;
}

void ApplicationWindow::changeAppStyle(const QString &s)
{
    // H6: ToriGraph always draws with its own theme (ModernUiTheme on top of Fusion); the old
    // "Style" setting is no longer offered in the preferences. The stored name is kept only so
    // that the ini file round-trips; whatever it says, the theme is (re)applied unchanged.
    if (s.trimmed().isEmpty())
        return;
    appStyle = s;
    if (QApplication *qa = qobject_cast<QApplication *>(qApp)) {
        if (!qa->property("_tg_modern_theme_applied").toBool() || qa->styleSheet().isEmpty()) {
            ModernUiTheme::applyModernTheme(qa);
            qa->setProperty("_tg_modern_theme_applied", true);
        }
        QPalette pal = qa->palette();
        if (panelsColor.isValid())
            pal.setColor(QPalette::Active, QPalette::Base, QColor(panelsColor));
        if (qa->palette() != pal)
            qa->setPalette(pal);
    }
}

void ApplicationWindow::changeAppFont(const QFont &f)
{
    if (appFont == f)
        return;

    appFont = f;
    updateAppFonts();
}

void ApplicationWindow::updateAppFonts()
{
    sanitizeUiTableFont(appFont);
    if (qApp->font() != appFont)
        qApp->setFont(appFont);
    this->setFont(appFont);
    if (menuBar())
        menuBar()->setFont(appFont);
    if (statusBar())
        statusBar()->setFont(appFont);
    explorerWindow.setFont(appFont);
    folders.setFont(appFont);
    lv.setFont(appFont);
    logWindow.setFont(appFont);
    if (results)
        results->setFont(appFont);
    scriptingMenu->setFont(appFont);
    if (materialsAnalysisMenu)
        materialsAnalysisMenu->setFont(appFont);
    windowsMenu->setFont(appFont);
    view->setFont(appFont);
    graph->setFont(appFont);
    file->setFont(appFont);
    format->setFont(appFont);
    calcul->setFont(appFont);
    edit->setFont(appFont);
    dataMenu->setFont(appFont);
    recent->setFont(appFont);
    help->setFont(appFont);
    type->setFont(appFont);
    plot2D->setFont(appFont);
    plot3D->setFont(appFont);
    plot3DMenu->setFont(appFont);
    matrixMenu->setFont(appFont);
    specialPlot->setFont(appFont);
    panels->setFont(appFont);
    stat->setFont(appFont);
    smooth->setFont(appFont);
    filter->setFont(appFont);
    decay->setFont(appFont);
    plotDataMenu->setFont(appFont);
    tableMenu->setFont(appFont);
    exportPlot->setFont(appFont);
    translateMenu->setFont(appFont);
    multiPeakMenu->setFont(appFont);
    if (colorPaletteMenu)
        colorPaletteMenu->setFont(appFont);
    if (aspectRatioMenu)
        aspectRatioMenu->setFont(appFont);
    if (languageMenu)
        languageMenu->setFont(appFont);
}

void ApplicationWindow::updateConfirmOptions(bool askTables, bool askMatrices, bool askPlots2D,
                                             bool askPlots3D, bool askNotes)
{
    QList<MyWidget *> windows = windowsList();
    if (confirmCloseTable != askTables) {
        confirmCloseTable = askTables;
        for (int i = 0; i < int(windows.count()); i++) {
            if (windows.at(i)->inherits("Table"))
                ((MyWidget *)windows.at(i))->askOnCloseEvent(confirmCloseTable);
        }
    }

    if (confirmCloseMatrix != askMatrices) {
        confirmCloseMatrix = askMatrices;
        for (int i = 0; i < int(windows.count()); i++) {
            if (windows.at(i)->inherits("Matrix"))
                ((MyWidget *)windows.at(i))->askOnCloseEvent(confirmCloseMatrix);
        }
    }

    if (confirmClosePlot2D != askPlots2D) {
        confirmClosePlot2D = askPlots2D;
        for (int i = 0; i < int(windows.count()); i++) {
            if (windows.at(i)->inherits("MultiLayer"))
                ((MyWidget *)windows.at(i))->askOnCloseEvent(confirmClosePlot2D);
        }
    }

    if (confirmClosePlot3D != askPlots3D) {
        confirmClosePlot3D = askPlots3D;
        for (int i = 0; i < int(windows.count()); i++) {
            if (windows.at(i)->inherits("Graph3D"))
                ((MyWidget *)windows.at(i))->askOnCloseEvent(confirmClosePlot3D);
        }
    }

    if (confirmCloseNotes != askNotes) {
        confirmCloseNotes = askNotes;
        for (int i = 0; i < int(windows.count()); i++) {
            if (windows.at(i)->inherits("Note"))
                ((MyWidget *)windows.at(i))->askOnCloseEvent(confirmCloseNotes);
        }
    }
}

void ApplicationWindow::setGraphDefaultSettings(bool autoscale, bool scaleFonts, bool resizeLayers,
                                                bool antialiasing)
{
    if (autoscale2DPlots == autoscale && autoScaleFonts == scaleFonts
        && autoResizeLayers != resizeLayers && antialiasing2DPlots == antialiasing)
        return;

    autoscale2DPlots = autoscale;
    autoScaleFonts = scaleFonts;
    autoResizeLayers = !resizeLayers;
    antialiasing2DPlots = antialiasing;

    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (w->inherits("MultiLayer")) {
            QWidgetList lst = ((MultiLayer *)w)->graphPtrs();
            Graph *g;
            foreach (QWidget *widget, lst) {
                g = (Graph *)widget;
                g->enableAutoscaling(autoscale2DPlots);
                g->updateScale();
                g->setIgnoreResizeEvents(!autoResizeLayers);
                g->setAutoscaleFonts(autoScaleFonts);
                g->setAntialiasing(antialiasing2DPlots);
            }
        }
    }
}

void ApplicationWindow::setLegendDefaultSettings(int frame, const QFont &font,
                                                 const QColor &textCol, const QColor &backgroundCol)
{
    if (legendFrameStyle == frame && legendTextColor == textCol && legendBackground == backgroundCol
        && plotLegendFont == font)
        return;

    legendFrameStyle = frame;
    legendTextColor = textCol;
    legendBackground = backgroundCol;
    plotLegendFont = font;

    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (w->inherits("MultiLayer")) {
            QWidgetList graphsList = ((MultiLayer *)w)->graphPtrs();
            foreach (QWidget *widget, graphsList)
                ((Graph *)widget)->setTextMarkerDefaults(frame, font, textCol, backgroundCol);
        }
    }
    saveSettings();
}

void ApplicationWindow::setArrowDefaultSettings(const QPen &pen, int headLength, int headAngle,
                                                bool fillHead)
{
    if (defaultArrowLineWidth == pen.width() && defaultArrowColor == pen.color()
        && defaultArrowLineStyle == pen.style() && defaultArrowHeadLength == headLength
        && defaultArrowHeadAngle == headAngle && defaultArrowHeadFill == fillHead)
        return;

    defaultArrowLineWidth = pen.width();
    defaultArrowColor = pen.color();
    defaultArrowLineStyle = pen.style();
    defaultArrowHeadLength = headLength;
    defaultArrowHeadAngle = headAngle;
    defaultArrowHeadFill = fillHead;

    QList<MyWidget *> windows = windowsList();
    foreach (MyWidget *w, windows) {
        if (w->inherits("MultiLayer")) {
            QWidgetList graphsList = ((MultiLayer *)w)->graphPtrs();
            foreach (QWidget *widget, graphsList)
                ((Graph *)widget)
                        ->setArrowDefaults(defaultArrowLineWidth, defaultArrowColor,

                                           defaultArrowLineStyle, defaultArrowHeadLength,
                                           defaultArrowHeadAngle, defaultArrowHeadFill);
        }
    }
    saveSettings();
}

void ApplicationWindow::readSettings(bool restoreWindowLayout)
{
    auto &settings = getSettings();
    // Lab-wide defaults next to the exe fill only keys the user has not set (I10).
    SettingsTransfer::applyLabDefaults(settings, SettingsTransfer::labDefaultsPath());
    // The default curve palette may have been read (and cached) before this point, e.g. by
    // the graph inspector; re-read it so a lab-default palette applies from the first run.
    Graph::reloadDefaultCurvePaletteName();
    if (d_graph_inspector_dock)
        d_graph_inspector_dock->syncSeriesSchemeToDefaultPalette();
    /* ---------------- group General --------------- */
    settings.beginGroup("/General");
#ifdef SEARCH_FOR_UPDATES
    autoSearchUpdates = settings.value("/AutoSearchUpdates", false).toBool();
#endif
    appLanguage =
            settings.value("/Language", QLocale::system().name().section('_', 0, 0)).toString();
    show_windows_policy =
            (ShowWindowsPolicy)settings.value("/ShowWindowsPolicy", ApplicationWindow::ActiveFolder)
                    .toInt();
    setTabbedWorkspaceView(settings.value("/TabbedWorkspaceView", false).toBool());
    d_saved_workspace_layout_mode =
            settings.value("/WorkspaceLayoutMode",
                           isTabbedWorkspaceView() ? QStringLiteral("tabbed")
                                                   : QStringLiteral("mdi"))
                    .toString()
                    .trimmed()
                    .toLower();
    d_saved_split_left_ratio =
            qBound(0.15, settings.value("/SplitWorkspaceLeftRatio", 0.40).toDouble(), 0.85);

    recentProjects = settings.value("/RecentProjects").toStringList();
    // Follows an ugly hack added by Ion in order to fix Qt4 porting issues
    //(only needed on Windows due to a Qt bug?)
#ifdef Q_OS_WIN
    if (!recentProjects.isEmpty() && recentProjects[0].contains("^e"))
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        recentProjects = recentProjects[0].split("^e", Qt::SkipEmptyParts);
#else
        recentProjects = recentProjects[0].split("^e", QString::SkipEmptyParts);
#endif
    else if (recentProjects.count() == 1) {
        QString s = recentProjects[0];
        if (s.remove(QRegExp("\\s")).isEmpty())
            recentProjects = QStringList();
    }
#endif

    updateRecentProjectsList();

    changeAppStyle(settings.value("/Style", appStyle).toString());
    undoLimit = settings.value("/UndoLimit", 50).toInt();
    if (!settings.value("/UndoLimitMigrated50", false).toBool()) {
        if (undoLimit <= 10)
            undoLimit = 50;
        settings.setValue("/UndoLimit", undoLimit);
        settings.setValue("/UndoLimitMigrated50", true);
    }
    if (undoLimit < 1)
        undoLimit = 50;
    d_project->undoStack()->setUndoLimit(undoLimit);
    for (MultiLayer *ml : findChildren<MultiLayer *>()) {
        if (ml && ml->undoStack())
            ml->undoStack()->setUndoLimit(undoLimit);
    }
    autoSave = settings.value("/AutoSave", true).toBool();
    autoSaveTime = settings.value("/AutoSaveTime", 15).toInt();
    defaultScriptingLang = settings.value("/ScriptingLang", "muParser").toString();

    QLocale temp_locale = QLocale(settings.value("/Locale", QLocale::system().name()).toString());
    bool usegl = settings.value("/LocaleUseGroupSeparator", false).toBool();
    if (usegl)
        temp_locale.setNumberOptions(temp_locale.numberOptions() & ~QLocale::OmitGroupSeparator);
    else
        temp_locale.setNumberOptions(temp_locale.numberOptions() | QLocale::OmitGroupSeparator);
    QLocale::setDefault(temp_locale);

    d_decimal_digits = settings.value("/DecimalDigits", 6).toInt();
    d_default_numeric_format = settings.value("/DefaultNumericFormat", 'g').toChar().toLatin1();
    if (d_default_numeric_format == 'f' && d_decimal_digits == 14) {
        d_default_numeric_format = 'g';
        d_decimal_digits = 6;
    }
    settings.setValue("/DecimalDigits", d_decimal_digits);
    settings.setValue("/DefaultNumericFormat", QChar(d_default_numeric_format));

    if (restoreWindowLayout) {
        // restore geometry of main window
        const QByteArray savedGeom = settings.value("/ProjectWindow/Geometry").toByteArray();
        if (!savedGeom.isEmpty()) {
            restoreGeometry(savedGeom);
        } else if (!d_headless_mode && !d_ole_embedded && !m_batch) {
            if (QScreen *scr = QGuiApplication::primaryScreen()) {
                const QRect avail = scr->availableGeometry();
                if (avail.width() <= 1400 || avail.height() <= 820) {
                    resize(qMax(1280, avail.width()), qMax(800, avail.height()));
                    setWindowState(windowState() | Qt::WindowMaximized);
                } else {
                    resize(qMin(avail.width(), 1440), qMin(avail.height(), 900));
                }
            } else {
                resize(1280, 800);
            }
        }

        // restore dock windows and tool bars
        restoreState(settings.value("/DockWindows").toByteArray());
        for (QDockWidget *dw : findChildren<QDockWidget *>()) {
            if (!dw || dw->isFloating())
                continue;
            if (dockWidgetArea(dw) == Qt::TopDockWidgetArea) {
                if (dw == &explorerWindow)
                    addDockWidget(Qt::LeftDockWidgetArea, dw);
                else if (dw == d_graph_inspector_dock)
                    addDockWidget(Qt::RightDockWidgetArea, dw);
                else
                    addDockWidget(Qt::BottomDockWidgetArea, dw);
            }
        }
        setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
        setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
        setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
        setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
        if (d_ribbon_toolbar) {
            if (file_tools) {
                insertToolBar(file_tools, d_ribbon_toolbar);
                insertToolBarBreak(file_tools);
            } else {
                addToolBar(Qt::TopToolBarArea, d_ribbon_toolbar);
                addToolBarBreak(Qt::TopToolBarArea);
            }
            d_ribbon_toolbar->setVisible(true);
        }
        explorerSplitter->restoreState(settings.value("/ExplorerSplitter").toByteArray());
    }

    auto parseSavedFont = [](const QString &family, const QString &ptStr, const QString &weightStr,
                             const QString &italicStr) -> QFont {
        QFont f(family, -1, weightStr.toInt(), italicStr.toInt() != 0);
        bool okPt = false;
        const double ptF = QLocale::c().toDouble(ptStr.trimmed(), &okPt);
        if (okPt && ptF > 0.0)
            f.setPointSizeF(ptF);
        else if (ptStr.toInt() > 0)
            f.setPointSize(ptStr.toInt());
        return f;
    };

    QStringList applicationFont = settings.value("/Font").toStringList();
    if (applicationFont.size() == 4)
        appFont = parseSavedFont(applicationFont[0], applicationFont[1], applicationFont[2],
                                 applicationFont[3]);
    sanitizeUiTableFont(appFont);

    settings.beginGroup("/Dialogs");
    d_extended_open_dialog = settings.value("/ExtendedOpenDialog", true).toBool();
    d_extended_export_dialog = settings.value("/ExtendedExportDialog", true).toBool();
    d_extended_import_ASCII_dialog = settings.value("/ExtendedImportAsciiDialog", false).toBool();
    d_extended_plot_dialog =
            settings.value("/ExtendedPlotDialog", true).toBool(); // used by PlotDialog

    settings.beginGroup("/AddRemoveCurves");
    d_add_curves_dialog_size =
            QSize(settings.value("/Width", 700).toInt(), settings.value("/Height", 400).toInt());
    d_show_current_folder = settings.value("/ShowCurrentFolder", false).toBool();
    settings.endGroup(); // AddRemoveCurves Dialog
    settings.endGroup(); // Dialogs

    settings.beginGroup("/Colors");
    const QString rawWsColor = settings.value("/Workspace", "#E8EDF4").toString();
    if (rawWsColor.compare(QStringLiteral("darkGray"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#808080"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#ff808080"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#a0a0a4"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#ffa0a0a4"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#a9a9a9"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#ffa9a9a9"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#f1f5f9"), Qt::CaseInsensitive) == 0
        || rawWsColor.compare(QStringLiteral("#fff1f5f9"), Qt::CaseInsensitive) == 0) {
        workspaceColor = QColor(0xE8, 0xED, 0xF4);
    } else {
        workspaceColor = QColor(COLORVALUE(rawWsColor));
    }
    // see http://doc.trolltech.com/4.2/qvariant.html for instructions on qcolor <-> qvariant
    // conversion
    panelsColor = QColor(COLORVALUE(settings.value("/Panels", "#ffffffff").toString()));
    panelsTextColor = QColor(COLORVALUE(settings.value("/PanelsText", "#ff000000").toString()));
    settings.endGroup(); // Colors

    settings.beginGroup("/Paths");
    const QString docsLoc = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString defaultUserDir =
            (!docsLoc.isEmpty() && QDir(docsLoc).exists()) ? docsLoc : QDir::homePath();
    workingDir = settings.value("/WorkingDir", defaultUserDir).toString();
    helpFilePath = settings.value("/HelpFile", "").toString();
#ifdef PLUGIN_PATH
    QString defaultFitPluginsPath = PLUGIN_PATH;
#else // defined PLUGIN_PATH
#ifdef Q_OS_WIN
    QString defaultFitPluginsPath = "fitPlugins";
#else
    QString defaultFitPluginsPath = "/usr/lib/scidavis/plugins";
#endif
#endif // defined PLUGIN_PATH
#ifdef DYNAMIC_PLUGIN_PATH
    fitPluginsPath = settings.value("/FitPlugins", defaultFitPluginsPath).toString();
#else // defined PLUGIN_PATH
    fitPluginsPath = defaultFitPluginsPath;
#endif

#ifdef Q_OS_WIN
    const QString appDataLoc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString writableTemplatesDefault = !appDataLoc.isEmpty()
            ? (appDataLoc + QStringLiteral("/templates"))
            : defaultUserDir;
    templatesDir = settings.value("/TemplatesDir", writableTemplatesDefault).toString();
    if (templatesDir.isEmpty()
        || QDir::cleanPath(templatesDir).compare(QDir::cleanPath(qApp->applicationDirPath()),
                                                 Qt::CaseInsensitive)
                == 0) {
        templatesDir = writableTemplatesDefault;
    }
    asciiDirPath = settings.value("/ASCII", defaultUserDir).toString();
    imagesDirPath = settings.value("/Images", defaultUserDir).toString();
#else
    templatesDir = settings.value("/TemplatesDir", defaultUserDir).toString();
    asciiDirPath = settings.value("/ASCII", defaultUserDir).toString();
    imagesDirPath = settings.value("/Images", defaultUserDir).toString();
#endif
    locktoolbar->setChecked(settings.value("LockToolbars", false).toBool());
    settings.endGroup(); // Paths
    settings.endGroup();
    /* ------------- end group General ------------------- */

    settings.beginGroup("/UserFunctions");
    fitFunctions = settings.value("/FitFunctions").toStringList();
    surfaceFunc = settings.value("/SurfaceFunctions").toStringList();
    xFunctions = settings.value("/xFunctions").toStringList();
    yFunctions = settings.value("/yFunctions").toStringList();
    rFunctions = settings.value("/rFunctions").toStringList();
    thetaFunctions = settings.value("/thetaFunctions").toStringList();
    settings.endGroup(); // UserFunctions

    settings.beginGroup("/Confirmations");
    confirmCloseFolder = settings.value("/Folder", true).toBool();
    confirmCloseTable = settings.value("/Table", true).toBool();
    confirmCloseMatrix = settings.value("/Matrix", true).toBool();
    confirmClosePlot2D = settings.value("/Plot2D", true).toBool();
    confirmClosePlot3D = settings.value("/Plot3D", true).toBool();
    confirmCloseNotes = settings.value("/Note", true).toBool();
    settings.endGroup(); // Confirmations

    /* ---------------- group Tables --------------- */
    settings.beginGroup("/Tables");
    d_show_table_comments = settings.value("/DisplayComments", true).toBool();
    QStringList tableFonts = settings.value("/Fonts").toStringList();
    if (tableFonts.size() == 8) {
        tableTextFont =
                parseSavedFont(tableFonts[0], tableFonts[1], tableFonts[2], tableFonts[3]);
        tableHeaderFont =
                parseSavedFont(tableFonts[4], tableFonts[5], tableFonts[6], tableFonts[7]);
    }
    sanitizeUiTableFont(tableTextFont);
    sanitizeUiTableFont(tableHeaderFont);

    settings.beginGroup("/Colors");
    tableBkgdColor = QColor(COLORVALUE(settings.value("/Background", "#ffffffff").toString()));
    tableTextColor = QColor(COLORVALUE(settings.value("/Text", "#ff000000").toString()));
    tableHeaderColor = QColor(COLORVALUE(settings.value("/Header", "#ff000000").toString()));
    settings.endGroup(); // Colors

    settings.beginGroup("/LastTableWindow");
    d_has_last_table_geometry = settings.value("/HasGeometry", false).toBool();
    if (d_has_last_table_geometry) {
        const int tx = settings.value("/X", 0).toInt();
        const int ty = settings.value("/Y", 0).toInt();
        const int tw = settings.value("/Width", 0).toInt();
        const int th = settings.value("/Height", 0).toInt();
        if (tw >= 260 && th >= 200)
            d_last_table_geometry = QRect(qMax(0, tx), qMax(0, ty), tw, th);
        else
            d_has_last_table_geometry = false;
    }
    settings.endGroup(); // LastTableWindow
    settings.endGroup();
    /* --------------- end group Tables ------------------------ */

    /* --------------- group 2D Plots ----------------------------- */
    settings.beginGroup("/2DPlots");
    const int toriStyleVersion = settings.value("/ToriGraphStyleVersion", 0).toInt();
    const bool migrateToriGraphStyle = toriStyleVersion < 2;
    const bool migrateArialGraphFonts = toriStyleVersion < 3;
    const bool migratePhase8DockAndWorksheet = toriStyleVersion < 4;
    const bool migratePhase8CrispExplorer = toriStyleVersion < 5;
    const bool migrateNonBoldGraphFonts = toriStyleVersion < 6;
    if (migratePhase8DockAndWorksheet) {
        d_show_table_comments = true;
        if (restoreWindowLayout) {
            setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
            setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
            setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
            setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
            if (!explorerWindow.isFloating()
                && dockWidgetArea(&explorerWindow) == Qt::BottomDockWidgetArea) {
                addDockWidget(Qt::LeftDockWidgetArea, &explorerWindow);
                explorerSplitter->setOrientation(Qt::Vertical);
                explorerSplitter->setSizes(QList<int>() << 140 << 260);
            }
            if (!logWindow.isFloating() && dockWidgetArea(&logWindow) == Qt::TopDockWidgetArea)
                addDockWidget(Qt::BottomDockWidgetArea, &logWindow);
#ifdef SCRIPTING_CONSOLE
            if (!consoleWindow.isFloating()
                && dockWidgetArea(&consoleWindow) == Qt::TopDockWidgetArea) {
                addDockWidget(Qt::BottomDockWidgetArea, &consoleWindow);
            }
#endif
        }
    }
    if (migratePhase8CrispExplorer && restoreWindowLayout) {
        if (!explorerWindow.isFloating()
            && (dockWidgetArea(&explorerWindow) == Qt::LeftDockWidgetArea
                || dockWidgetArea(&explorerWindow) == Qt::RightDockWidgetArea)) {
            resizeDocks({ &explorerWindow }, { 255 }, Qt::Horizontal);
        }
    }
    ::future::Table::setDefaultCommentVisibility(d_show_table_comments);
    if (restoreWindowLayout)
        updateExplorerAdaptiveLayout();
    auto sanitizePlotFontToArial = [migrateArialGraphFonts](QFont &f) {
        const QString fam = f.family().trimmed();
        const bool isWinUiFont = fam.isEmpty()
                || fam.compare("Yu Gothic UI", Qt::CaseInsensitive) == 0
                || fam.compare("Yu Gothic", Qt::CaseInsensitive) == 0
                || fam.compare("MS UI Gothic", Qt::CaseInsensitive) == 0
                || fam.compare("MS PGothic", Qt::CaseInsensitive) == 0
                || fam.compare("MS Shell Dlg 2", Qt::CaseInsensitive) == 0
                || fam.compare("Meiryo UI", Qt::CaseInsensitive) == 0
                || fam.compare("Meiryo", Qt::CaseInsensitive) == 0;
        if (migrateArialGraphFonts || isWinUiFont)
            f.setFamily("Arial");
    };
    settings.beginGroup("/General");
    titleOn = migrateToriGraphStyle ? false : settings.value("/Title", false).toBool();
    allAxesOn = migrateToriGraphStyle ? true : settings.value("/AllAxes", true).toBool();
    canvasFrameOn = settings.value("/CanvasFrame", false).toBool();
    canvasFrameWidth = settings.value("/CanvasFrameWidth", 0).toInt();
    defaultPlotMargin = migrateToriGraphStyle ? 14 : settings.value("/Margin", 14).toInt();
    drawBackbones = migrateToriGraphStyle ? true : settings.value("/AxesBackbones", true).toBool();
    axesLineWidth = migrateToriGraphStyle ? 2 : settings.value("/AxesLineWidth", 2).toInt();
    autoscale2DPlots = settings.value("/Autoscale", true).toBool();
    autoScaleFonts = migrateToriGraphStyle ? false : settings.value("/AutoScaleFonts", false).toBool();
    autoResizeLayers = migrateToriGraphStyle ? true : settings.value("/AutoResizeLayers", true).toBool();
    antialiasing2DPlots = migrateToriGraphStyle ? true : settings.value("/Antialiasing", true).toBool();
    d_scale_plots_on_print = settings.value("/ScaleLayersOnPrint", false).toBool();
    d_print_cropmarks = settings.value("/PrintCropmarks", false).toBool();

    QStringList graphFonts = settings.value("/Fonts").toStringList();
    if (!migrateToriGraphStyle && graphFonts.size() == 16) {
        plotAxesFont = parseSavedFont(graphFonts[0], graphFonts[1], graphFonts[2], graphFonts[3]);
        plotNumbersFont =
                parseSavedFont(graphFonts[4], graphFonts[5], graphFonts[6], graphFonts[7]);
        plotLegendFont =
                parseSavedFont(graphFonts[8], graphFonts[9], graphFonts[10], graphFonts[11]);
        plotTitleFont =
                parseSavedFont(graphFonts[12], graphFonts[13], graphFonts[14], graphFonts[15]);
    }
    sanitizePlotFontToArial(plotAxesFont);
    sanitizePlotFontToArial(plotNumbersFont);
    sanitizePlotFontToArial(plotLegendFont);
    sanitizePlotFontToArial(plotTitleFont);
    if (migrateNonBoldGraphFonts) {
        plotAxesFont.setBold(false);
        plotNumbersFont.setBold(false);
        plotLegendFont.setBold(false);
        plotTitleFont.setBold(false);
    }
    settings.endGroup(); // General

    settings.beginGroup("/Curves");
    defaultCurveStyle = migrateToriGraphStyle ? int(Graph::LineSymbols)
                                              : settings.value("/Style", Graph::LineSymbols).toInt();
    defaultCurveLineWidth = migrateToriGraphStyle ? 2 : settings.value("/LineWidth", 2).toInt();
    defaultSymbolSize = migrateToriGraphStyle ? 9 : settings.value("/SymbolSize", 9).toInt();
    settings.endGroup(); // Curves

    settings.beginGroup("/Ticks");
    majTicksStyle = migrateToriGraphStyle ? int(ScaleDraw::In)
                                          : settings.value("/MajTicksStyle", ScaleDraw::In).toInt();
    minTicksStyle = migrateToriGraphStyle ? int(ScaleDraw::In)
                                          : settings.value("/MinTicksStyle", ScaleDraw::In).toInt();
    minTicksLength = migrateToriGraphStyle ? 5 : settings.value("/MinTicksLength", 5).toInt();
    majTicksLength = migrateToriGraphStyle ? 9 : settings.value("/MajTicksLength", 9).toInt();
    settings.endGroup(); // Ticks

    settings.beginGroup("/Legend");
    // E1: the default legend has no frame (matches the ISIJ preset). Settings saved by older
    // builds always stored the old boxed default; migrate it once.
    legendFrameStyle = migrateToriGraphStyle ? int(Legend::None)
                                             : settings.value("/FrameStyle", Legend::None).toInt();
    if (!settings.value("/FrameStyleMigratedNone", false).toBool()) {
        if (legendFrameStyle == int(Legend::Line))
            legendFrameStyle = int(Legend::None);
        settings.setValue("/FrameStyleMigratedNone", true);
    }
    legendTextColor = QColor(COLORVALUE(
            settings.value("/TextColor", "#ff000000").toString())); // default color Qt::black
    legendBackground = QColor(COLORVALUE(
            settings.value("/BackgroundColor", "#ffffffff").toString())); // default color Qt::white
    legendBackground.setAlpha(migrateToriGraphStyle
                                      ? 255
                                      : settings.value("/Transparency", 255).toInt()); // opaque white by default
    settings.endGroup(); // Legend

    settings.beginGroup("/Arrows");
    defaultArrowLineWidth = settings.value("/Width", 1).toInt();
    defaultArrowColor = QColor(COLORVALUE(
            settings.value("/Color", "#ff000000").toString())); // default color Qt::black
    defaultArrowHeadLength = settings.value("/HeadLength", 4).toInt();
    defaultArrowHeadAngle = settings.value("/HeadAngle", 45).toInt();
    defaultArrowHeadFill = settings.value("/HeadFill", true).toBool();
    defaultArrowLineStyle =
            Graph::getPenStyle(settings.value("/LineStyle", "SolidLine").toString());
    settings.endGroup(); // Arrows

    settings.beginGroup("/LastGraphWindow");
    d_has_last_graph_geometry = settings.value("/HasGeometry", false).toBool();
    d_last_graph_maximized = settings.value("/Maximized", false).toBool();
    if (d_has_last_graph_geometry) {
        const int gx = settings.value("/X", 0).toInt();
        const int gy = settings.value("/Y", 0).toInt();
        const int gw = settings.value("/Width", 0).toInt();
        const int gh = settings.value("/Height", 0).toInt();
        if (gw >= 260 && gh >= 220)
            d_last_graph_geometry = QRect(qMax(0, gx), qMax(0, gy), gw, gh);
        else
            d_has_last_graph_geometry = false;
    }
    settings.endGroup(); // LastGraphWindow

    settings.beginGroup("/LastGraphSettings");
    d_restored_graph_settings = SavedGraphSettings();
    d_restored_graph_settings.valid = settings.value("/HasSettings", false).toBool();
    if (d_restored_graph_settings.valid) {
        SavedGraphSettings &s = d_restored_graph_settings;
        s.pageViewMode = settings.value("/PageViewMode", false).toBool();
        s.pageUnit = qBound(0, settings.value("/PageUnit", 0).toInt(), 3);
        s.pageWidth = settings.value("/PageWidth", 520.0).toDouble();
        s.pageHeight = settings.value("/PageHeight", 520.0).toDouble();
        s.pageDpi = qBound(72, settings.value("/PageDpi", 300).toInt(), 2400);
        s.pageZoom = qBound(0.25, settings.value("/PageZoom", 1.0).toDouble(), 4.0);
        const QColor parsedBg(settings.value("/PageBgColor", QStringLiteral("#ffffffff")).toString());
        s.pageBg = parsedBg.isValid() ? parsedBg : QColor(Qt::white);
        s.keepAspectRatio = settings.value("/KeepAspectRatio", true).toBool();
        const double savedRatio = settings.value("/TargetAspectRatio", 1.0).toDouble();
        s.targetAspectRatio = (savedRatio > 0.05 && savedRatio <= 20.0) ? savedRatio : 1.0;
        s.panelLabelsEnabled = settings.value("/PanelLabelsEnabled", false).toBool();
        s.panelLabelPlacement = qBound(0, settings.value("/PanelLabelPlacement", 0).toInt(), 1);
        s.panelLabelPattern =
                settings.value("/PanelLabelPattern", QStringLiteral("(%a)")).toString();
        s.panelLabelFontPt = qBound(0, settings.value("/PanelLabelFontPt", 0).toInt(), 72);
        s.unitFormat = qBound(0, settings.value("/UnitFormat", 0).toInt(), 2);
        s.gridMajorX = settings.value("/GridMajorX", false).toBool();
        s.gridMajorY = settings.value("/GridMajorY", false).toBool();
        s.gridMinorX = settings.value("/GridMinorX", false).toBool();
        s.gridMinorY = settings.value("/GridMinorY", false).toBool();
    }
    d_exit_graph_settings = d_restored_graph_settings;
    settings.endGroup(); // LastGraphSettings
    settings.endGroup();
    /* ----------------- end group 2D Plots --------------------------- */

    /* ----------------- group 3D Plots --------------------------- */
    settings.beginGroup("/3DPlots");
    showPlot3DLegend = settings.value("/Legend", true).toBool();
    showPlot3DProjection = settings.value("/Projection", false).toBool();
    smooth3DMesh = settings.value("/Antialiasing", true).toBool();
    plot3DResolution = settings.value("/Resolution", 1).toInt();
    orthogonal3DPlots = settings.value("/Orthogonal", false).toBool();
    autoscale3DPlots = settings.value("/Autoscale", true).toBool();

    QStringList plot3DFonts = settings.value("/Fonts").toStringList();
    if (plot3DFonts.size() == 12) {
        plot3DTitleFont =
                parseSavedFont(plot3DFonts[0], plot3DFonts[1], plot3DFonts[2], plot3DFonts[3]);
        plot3DNumbersFont =
                parseSavedFont(plot3DFonts[4], plot3DFonts[5], plot3DFonts[6], plot3DFonts[7]);
        plot3DAxesFont =
                parseSavedFont(plot3DFonts[8], plot3DFonts[9], plot3DFonts[10], plot3DFonts[11]);
    }
    sanitizePlotFontToArial(plot3DTitleFont);
    sanitizePlotFontToArial(plot3DNumbersFont);
    sanitizePlotFontToArial(plot3DAxesFont);
    if (migrateNonBoldGraphFonts) {
        plot3DTitleFont.setBold(false);
        plot3DNumbersFont.setBold(false);
        plot3DAxesFont.setBold(false);
    }

    plot3DColors.clear();
    settings.beginGroup("/Colors");
    plot3DColors << settings.value("/MaxData", "blue").toString();
    plot3DColors << settings.value("/Labels", "#000000").toString();
    plot3DColors << settings.value("/Mesh", "#000000").toString();
    plot3DColors << settings.value("/Grid", "#000000").toString();
    plot3DColors << settings.value("/MinData", "red").toString();
    plot3DColors << settings.value("/Numbers", "#000000").toString();
    plot3DColors << settings.value("/Axes", "#000000").toString();
    plot3DColors << settings.value("/Background", "#ffffff").toString();
    settings.endGroup(); // Colors
    settings.endGroup();
    /* ----------------- end group 3D Plots --------------------------- */

    settings.beginGroup("/Fitting");
    fit_output_precision = settings.value("/OutputPrecision", 15).toInt();
    pasteFitResultsToPlot = settings.value("/PasteResultsToPlot", false).toBool();
    writeFitResultsToLog = settings.value("/WriteResultsToLog", true).toBool();
    generateUniformFitPoints = settings.value("/GenerateFunction", true).toBool();
    fitPoints = settings.value("/Points", 100).toInt();
    generatePeakCurves = settings.value("/GeneratePeakCurves", true).toBool();
    peakCurvesColor = QColor(
            COLORVALUE(settings.value("/PeaksColor", "#ff00ff00").toString())); // green color
    fit_scale_errors = settings.value("/ScaleErrors", false).toBool();
    d_2_linear_fit_points = settings.value("/TwoPointsLinearFit", true).toBool();
    settings.endGroup(); // Fitting

    settings.beginGroup("/ImportASCII");
    columnSeparator = settings.value("/ColumnSeparator", "\\t").toString();
    columnSeparator.replace("\\t", "\t").replace("\\s", " ");
    ignoredLines = settings.value("/IgnoreLines", 0).toInt();
    renameColumns = settings.value("/RenameColumns", true).toBool();
    strip_spaces = settings.value("/StripSpaces", false).toBool();
    simplify_spaces = settings.value("/SimplifySpaces", false).toBool();
    d_ASCII_file_filter = settings.value("/AsciiFileTypeFilter", "*").toString();
    d_ASCII_import_locale = settings.value("/AsciiImportLocale", "C").toString();
    d_convert_to_numeric = settings.value("/ConvertToNumeric", true).toBool();
    settings.endGroup(); // Import ASCII

    settings.beginGroup("/ExportImage");
    d_image_export_filter = settings.value("/ImageFileTypeFilter", ".png").toString();
    d_export_transparency = settings.value("/ExportTransparency", false).toBool();
    d_export_quality = settings.value("/ImageQuality", 100).toInt();
    // B2: publication default is 600 dpi. Settings written by older builds stored the old
    // 72 dpi default; migrate that value once (a deliberate 72 chosen later is kept).
    d_export_resolution = settings.value("/Resolution", 600).toInt();
    if (!settings.value("/ResolutionMigrated600", false).toBool()) {
        if (d_export_resolution == 72)
            d_export_resolution = 600;
        settings.setValue("/ResolutionMigrated600", true);
    }
    d_export_color = settings.value("/ExportColor", true).toBool();
    d_export_vector_size = settings.value("/ExportPageSize", QPrinter::Custom).toInt();
    d_keep_plot_aspect = settings.value("/KeepAspect", true).toBool();
    d_export_orientation = settings.value("/Orientation", QPrinter::Landscape).toInt();
    settings.endGroup(); // ExportImage
}

void ApplicationWindow::saveSettings()
{
    // After Settings > Import/Reset the stored settings are newer than this window's state.
    if (SettingsTransfer::saveSuppressed())
        return;
    // Headless/batch sessions (SDK scripts) must not change what the next GUI start uses.
    if (!persistsUserSettings())
        return;
    // Capture the latest active/most-recent Graph & Table window geometry and settings
    if (!d_ole_embedded) {
        MultiLayer *bestMl = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
        Table *bestTbl = qobject_cast<Table *>(d_workspace.activeSubWindow());
        const QList<QMdiSubWindow *> hist =
                d_workspace.subWindowList(QMdiArea::ActivationHistoryOrder);
        for (int i = hist.size() - 1; i >= 0 && (!bestMl || !bestTbl); --i) {
            if (!bestMl) {
                if (MultiLayer *cand = qobject_cast<MultiLayer *>(hist.at(i)))
                    bestMl = cand;
            }
            if (!bestTbl) {
                if (Table *cand = qobject_cast<Table *>(hist.at(i)))
                    bestTbl = cand;
            }
        }
        if (bestMl) {
            captureLastGraphGeometry(bestMl, true);
            captureLastGraphSettingsForExit(bestMl);
        }
        if (bestTbl)
            captureLastTableGeometry(bestTbl, true);
    }

    auto &settings = getSettings();
    /* ---------------- group General --------------- */
    settings.beginGroup("/General");
#ifdef SEARCH_FOR_UPDATES
    settings.setValue("/AutoSearchUpdates", autoSearchUpdates);
#endif
    settings.setValue("/Language", appLanguage);
    settings.setValue("/ShowWindowsPolicy", show_windows_policy);
    settings.setValue("/TabbedWorkspaceView", isTabbedWorkspaceView());
    if (d_modern_ribbon) {
        settings.setValue("/WorkspaceLayoutMode", d_modern_ribbon->workspaceLayoutModeName());
        settings.setValue("/SplitWorkspaceLeftRatio", d_modern_ribbon->splitLeftRatio());
    } else {
        settings.setValue("/WorkspaceLayoutMode",
                          isTabbedWorkspaceView() ? QStringLiteral("tabbed")
                                                  : QStringLiteral("mdi"));
        settings.setValue("/SplitWorkspaceLeftRatio", d_saved_split_left_ratio);
    }
    settings.setValue("/RecentProjects", recentProjects);
    settings.setValue("/Style", appStyle);
    settings.setValue("/AutoSave", autoSave);
    settings.setValue("/AutoSaveTime", autoSaveTime);
    settings.setValue("/UndoLimit", undoLimit);
    settings.setValue("/ScriptingLang", defaultScriptingLang);
    settings.setValue("/Locale", QLocale().name());
    settings.setValue("/LocaleUseGroupSeparator",
                      bool(!(QLocale().numberOptions() & QLocale::OmitGroupSeparator)));
    settings.setValue("/DecimalDigits", d_decimal_digits);
    settings.setValue("/DefaultNumericFormat", QChar(d_default_numeric_format));

    if (!d_ole_embedded && !(d_modern_ribbon && d_modern_ribbon->isFocusHudMode())) {
        settings.setValue("/ProjectWindow/Geometry", saveGeometry());
        settings.setValue("/DockWindows", saveState());
        settings.setValue("/ExplorerSplitter", explorerSplitter->saveState());
    }

    auto formatFontPt = [](const QFont &f) -> QString {
        const double ptF = (f.pointSizeF() > 0.0) ? f.pointSizeF() : double(f.pointSize());
        return (ptF > 0.0) ? QString::number(ptF, 'g', 6) : QString::number(f.pointSize());
    };

    QStringList applicationFont;
    applicationFont << appFont.family();
    applicationFont << formatFontPt(appFont);
    applicationFont << QString::number(appFont.weight());
    applicationFont << QString::number(appFont.italic());
    settings.setValue("/Font", applicationFont);

    settings.beginGroup("/Dialogs");
    settings.setValue("/ExtendedOpenDialog", d_extended_open_dialog);
    settings.setValue("/ExtendedExportDialog", d_extended_export_dialog);
    settings.setValue("/ExtendedImportAsciiDialog", d_extended_import_ASCII_dialog);
    settings.setValue("/ExtendedPlotDialog", d_extended_plot_dialog);
    settings.beginGroup("/AddRemoveCurves");
    settings.setValue("/Width", d_add_curves_dialog_size.width());
    settings.setValue("/Height", d_add_curves_dialog_size.height());
    settings.setValue("/ShowCurrentFolder", d_show_current_folder);
    settings.endGroup(); // AddRemoveCurves Dialog
    settings.endGroup(); // Dialogs

    settings.beginGroup("/Colors");
    settings.setValue("/Workspace", COLORNAME(workspaceColor));
    settings.setValue("/Panels", COLORNAME(panelsColor));
    settings.setValue("/PanelsText", COLORNAME(panelsTextColor));
    settings.endGroup(); // Colors

    settings.beginGroup("/Paths");
    settings.setValue("/WorkingDir", workingDir);
    settings.setValue("/TemplatesDir", templatesDir);
    settings.setValue("/HelpFile", helpFilePath);
    settings.setValue("/FitPlugins", fitPluginsPath);
    settings.setValue("/ASCII", asciiDirPath);
    settings.setValue("/Images", imagesDirPath);

    settings.setValue("LockToolbars", locktoolbar->isChecked());

    settings.endGroup(); // Paths
    settings.endGroup();
    /* ---------------- end group General --------------- */

    settings.beginGroup("/UserFunctions");
    settings.setValue("/FitFunctions", fitFunctions);
    settings.setValue("/SurfaceFunctions", surfaceFunc);
    settings.setValue("/xFunctions", xFunctions);
    settings.setValue("/yFunctions", yFunctions);
    settings.setValue("/rFunctions", rFunctions);
    settings.setValue("/thetaFunctions", thetaFunctions);
    settings.endGroup(); // UserFunctions

    settings.beginGroup("/Confirmations");
    settings.setValue("/Folder", confirmCloseFolder);
    settings.setValue("/Table", confirmCloseTable);
    settings.setValue("/Matrix", confirmCloseMatrix);
    settings.setValue("/Plot2D", confirmClosePlot2D);
    settings.setValue("/Plot3D", confirmClosePlot3D);
    settings.setValue("/Note", confirmCloseNotes);
    settings.endGroup(); // Confirmations

    /* ----------------- group Tables -------------- */
    settings.beginGroup("/Tables");
    settings.setValue("/DisplayComments", d_show_table_comments);
    QStringList tableFonts;
    tableFonts << tableTextFont.family();
    tableFonts << formatFontPt(tableTextFont);
    tableFonts << QString::number(tableTextFont.weight());
    tableFonts << QString::number(tableTextFont.italic());
    tableFonts << tableHeaderFont.family();
    tableFonts << formatFontPt(tableHeaderFont);
    tableFonts << QString::number(tableHeaderFont.weight());
    tableFonts << QString::number(tableHeaderFont.italic());
    settings.setValue("/Fonts", tableFonts);

    settings.beginGroup("/Colors");
    settings.setValue("/Background", COLORNAME(tableBkgdColor));
    settings.setValue("/Text", COLORNAME(tableTextColor));
    settings.setValue("/Header", COLORNAME(tableHeaderColor));
    settings.endGroup(); // Colors

    settings.beginGroup("/LastTableWindow");
    settings.setValue("/HasGeometry", d_has_last_table_geometry);
    if (d_has_last_table_geometry) {
        settings.setValue("/X", d_last_table_geometry.x());
        settings.setValue("/Y", d_last_table_geometry.y());
        settings.setValue("/Width", d_last_table_geometry.width());
        settings.setValue("/Height", d_last_table_geometry.height());
    }
    settings.endGroup(); // LastTableWindow
    settings.endGroup();
    /* ----------------- end group Tables ---------- */

    /* ----------------- group 2D Plots ------------ */
    const bool useExitVisual = d_exit_graph_settings.valid && d_exit_graph_settings.hasVisualStyle;
    const QFont &saveAxesFont = useExitVisual ? d_exit_graph_settings.axesFont : plotAxesFont;
    const QFont &saveNumbersFont =
            useExitVisual ? d_exit_graph_settings.numbersFont : plotNumbersFont;
    const QFont &saveLegendFont = useExitVisual ? d_exit_graph_settings.legendFont : plotLegendFont;
    const int saveAxesLw = useExitVisual ? d_exit_graph_settings.axesLineWidth : axesLineWidth;
    const int saveMajTicksStyle =
            useExitVisual ? d_exit_graph_settings.majTicksStyle : majTicksStyle;
    const int saveMinTicksStyle =
            useExitVisual ? d_exit_graph_settings.minTicksStyle : minTicksStyle;
    const int saveMajTicksLen =
            useExitVisual ? d_exit_graph_settings.majTicksLength : majTicksLength;
    const int saveMinTicksLen =
            useExitVisual ? d_exit_graph_settings.minTicksLength : minTicksLength;
    const int saveLegFrame =
            useExitVisual ? d_exit_graph_settings.legendFrameStyle : legendFrameStyle;
    const QColor &saveLegTextCol =
            useExitVisual ? d_exit_graph_settings.legendTextColor : legendTextColor;
    const QColor &saveLegBg =
            useExitVisual ? d_exit_graph_settings.legendBackground : legendBackground;

    settings.beginGroup("/2DPlots");
    settings.beginGroup("/General");
    settings.setValue("/Title", titleOn);
    settings.setValue("/AllAxes", allAxesOn);
    settings.setValue("/CanvasFrame", canvasFrameOn);
    settings.setValue("/CanvasFrameWidth", canvasFrameWidth);
    settings.setValue("/Margin", defaultPlotMargin);
    settings.setValue("/AxesBackbones", drawBackbones);
    settings.setValue("/AxesLineWidth", saveAxesLw);
    settings.setValue("/Autoscale", autoscale2DPlots);
    settings.setValue("/AutoScaleFonts", autoScaleFonts);
    settings.setValue("/AutoResizeLayers", autoResizeLayers);
    settings.setValue("/Antialiasing", antialiasing2DPlots);
    settings.setValue("/ScaleLayersOnPrint", d_scale_plots_on_print);
    settings.setValue("/PrintCropmarks", d_print_cropmarks);

    QStringList graphFonts;
    graphFonts << saveAxesFont.family();
    graphFonts << formatFontPt(saveAxesFont);
    graphFonts << QString::number(saveAxesFont.weight());
    graphFonts << QString::number(saveAxesFont.italic());
    graphFonts << saveNumbersFont.family();
    graphFonts << formatFontPt(saveNumbersFont);
    graphFonts << QString::number(saveNumbersFont.weight());
    graphFonts << QString::number(saveNumbersFont.italic());
    graphFonts << saveLegendFont.family();
    graphFonts << formatFontPt(saveLegendFont);
    graphFonts << QString::number(saveLegendFont.weight());
    graphFonts << QString::number(saveLegendFont.italic());
    graphFonts << plotTitleFont.family();
    graphFonts << formatFontPt(plotTitleFont);
    graphFonts << QString::number(plotTitleFont.weight());
    graphFonts << QString::number(plotTitleFont.italic());
    settings.setValue("/Fonts", graphFonts);
    settings.endGroup(); // General

    settings.beginGroup("/Curves");
    settings.setValue("/Style", defaultCurveStyle);
    settings.setValue("/LineWidth", defaultCurveLineWidth);
    settings.setValue("/SymbolSize", defaultSymbolSize);
    settings.endGroup(); // Curves

    settings.beginGroup("/Ticks");
    settings.setValue("/MajTicksStyle", saveMajTicksStyle);
    settings.setValue("/MinTicksStyle", saveMinTicksStyle);
    settings.setValue("/MinTicksLength", saveMinTicksLen);
    settings.setValue("/MajTicksLength", saveMajTicksLen);
    settings.endGroup(); // Ticks

    settings.beginGroup("/Legend");
    settings.setValue("/FrameStyle", saveLegFrame);
    settings.setValue("/TextColor", COLORNAME(saveLegTextCol));
    settings.setValue("/BackgroundColor", COLORNAME(saveLegBg));
    settings.setValue("/Transparency", saveLegBg.alpha());
    settings.endGroup(); // Legend

    settings.beginGroup("/Arrows");
    settings.setValue("/Width", defaultArrowLineWidth);
    settings.setValue("/Color", COLORNAME(defaultArrowColor));
    settings.setValue("/HeadLength", defaultArrowHeadLength);
    settings.setValue("/HeadAngle", defaultArrowHeadAngle);
    settings.setValue("/HeadFill", defaultArrowHeadFill);
    settings.setValue("/LineStyle", Graph::penStyleName(defaultArrowLineStyle));
    settings.endGroup(); // Arrows

    settings.beginGroup("/LastGraphWindow");
    settings.setValue("/HasGeometry", d_has_last_graph_geometry);
    settings.setValue("/Maximized", d_last_graph_maximized);
    if (d_has_last_graph_geometry) {
        settings.setValue("/X", d_last_graph_geometry.x());
        settings.setValue("/Y", d_last_graph_geometry.y());
        settings.setValue("/Width", d_last_graph_geometry.width());
        settings.setValue("/Height", d_last_graph_geometry.height());
    }
    settings.endGroup(); // LastGraphWindow

    settings.beginGroup("/LastGraphSettings");
    settings.setValue("/HasSettings", d_exit_graph_settings.valid);
    if (d_exit_graph_settings.valid) {
        const SavedGraphSettings &s = d_exit_graph_settings;
        settings.setValue("/PageViewMode", s.pageViewMode);
        settings.setValue("/PageUnit", s.pageUnit);
        settings.setValue("/PageWidth", s.pageWidth);
        settings.setValue("/PageHeight", s.pageHeight);
        settings.setValue("/PageDpi", s.pageDpi);
        settings.setValue("/PageZoom", s.pageZoom);
        settings.setValue("/PageBgColor", s.pageBg.name(QColor::HexArgb));
        settings.setValue("/KeepAspectRatio", s.keepAspectRatio);
        settings.setValue("/TargetAspectRatio", s.targetAspectRatio);
        settings.setValue("/PanelLabelsEnabled", s.panelLabelsEnabled);
        settings.setValue("/PanelLabelPlacement", s.panelLabelPlacement);
        settings.setValue("/PanelLabelPattern", s.panelLabelPattern);
        settings.setValue("/PanelLabelFontPt", s.panelLabelFontPt);
        settings.setValue("/UnitFormat", s.unitFormat);
        settings.setValue("/GridMajorX", s.gridMajorX);
        settings.setValue("/GridMajorY", s.gridMajorY);
        settings.setValue("/GridMinorX", s.gridMinorX);
        settings.setValue("/GridMinorY", s.gridMinorY);
    }
    settings.endGroup(); // LastGraphSettings

    settings.setValue("/ToriGraphStyleVersion", 6);
    settings.endGroup();
    /* ----------------- end group 2D Plots -------- */

    /* ----------------- group 3D Plots ------------ */
    settings.beginGroup("/3DPlots");
    settings.setValue("/Legend", showPlot3DLegend);
    settings.setValue("/Projection", showPlot3DProjection);
    settings.setValue("/Antialiasing", smooth3DMesh);
    settings.setValue("/Resolution", plot3DResolution);
    settings.setValue("/Orthogonal", orthogonal3DPlots);
    settings.setValue("/Autoscale", autoscale3DPlots);

    QStringList plot3DFonts;
    plot3DFonts << plot3DTitleFont.family();
    plot3DFonts << formatFontPt(plot3DTitleFont);
    plot3DFonts << QString::number(plot3DTitleFont.weight());
    plot3DFonts << QString::number(plot3DTitleFont.italic());
    plot3DFonts << plot3DNumbersFont.family();
    plot3DFonts << formatFontPt(plot3DNumbersFont);
    plot3DFonts << QString::number(plot3DNumbersFont.weight());
    plot3DFonts << QString::number(plot3DNumbersFont.italic());
    plot3DFonts << plot3DAxesFont.family();
    plot3DFonts << formatFontPt(plot3DAxesFont);
    plot3DFonts << QString::number(plot3DAxesFont.weight());
    plot3DFonts << QString::number(plot3DAxesFont.italic());
    settings.setValue("/Fonts", plot3DFonts);

    settings.beginGroup("/Colors");
    settings.setValue("/MaxData", plot3DColors[0]);
    settings.setValue("/Labels", plot3DColors[1]);
    settings.setValue("/Mesh", plot3DColors[2]);
    settings.setValue("/Grid", plot3DColors[3]);
    settings.setValue("/MinData", plot3DColors[4]);
    settings.setValue("/Numbers", plot3DColors[5]);
    settings.setValue("/Axes", plot3DColors[6]);
    settings.setValue("/Background", plot3DColors[7]);
    settings.endGroup(); // Colors
    settings.endGroup();
    /* ----------------- end group 2D Plots -------- */

    settings.beginGroup("/Fitting");
    settings.setValue("/OutputPrecision", fit_output_precision);
    settings.setValue("/PasteResultsToPlot", pasteFitResultsToPlot);
    settings.setValue("/WriteResultsToLog", writeFitResultsToLog);
    settings.setValue("/GenerateFunction", generateUniformFitPoints);
    settings.setValue("/Points", fitPoints);
    settings.setValue("/GeneratePeakCurves", generatePeakCurves);
    settings.setValue("/PeaksColor", COLORNAME(peakCurvesColor));
    settings.setValue("/ScaleErrors", fit_scale_errors);
    settings.setValue("/TwoPointsLinearFit", d_2_linear_fit_points);
    settings.endGroup(); // Fitting

    settings.beginGroup("/ImportASCII");
    QString sep = columnSeparator;
    settings.setValue("/ColumnSeparator", sep.replace("\t", "\\t").replace(" ", "\\s"));
    settings.setValue("/IgnoreLines", ignoredLines);
    settings.setValue("/RenameColumns", renameColumns);
    settings.setValue("/StripSpaces", strip_spaces);
    settings.setValue("/SimplifySpaces", simplify_spaces);
    settings.setValue("/AsciiFileTypeFilter", d_ASCII_file_filter);
    settings.setValue("/AsciiImportLocale", d_ASCII_import_locale.name());
    settings.setValue("/ConvertToNumeric", d_convert_to_numeric);
    settings.endGroup(); // ImportASCII

    settings.beginGroup("/ExportImage");
    settings.setValue("/ImageFileTypeFilter", d_image_export_filter);
    settings.setValue("/ExportTransparency", d_export_transparency);
    settings.setValue("/ImageQuality", d_export_quality);
    settings.setValue("/Resolution", d_export_resolution);
    settings.setValue("/ExportColor", d_export_color);
    settings.setValue("/ExportPageSize", d_export_vector_size);
    settings.setValue("/KeepAspect", d_keep_plot_aspect);
    settings.setValue("/Orientation", d_export_orientation);
    settings.endGroup(); // ExportImage
    settings.sync();
}

void ApplicationWindow::clearParamFunctionsList()
{
    xFunctions.clear();
    yFunctions.clear();
}

void ApplicationWindow::clearPolarFunctionsList()
{
    rFunctions.clear();
    thetaFunctions.clear();
}

void ApplicationWindow::clearFitFunctionsList()
{
    fitFunctions.clear();
}

void ApplicationWindow::saveFitFunctionsList(const QStringList &l)
{
    fitFunctions = l;
}

void ApplicationWindow::clearSurfaceFunctionsList()
{
    surfaceFunc.clear();
}

void ApplicationWindow::setAppColors(const QColor &wc, const QColor &pc, const QColor &tpc)
{
    if (workspaceColor != wc) {
        workspaceColor = wc;
        d_workspace.setBackground(wc);
    }

    if (panelsColor == pc && panelsTextColor == tpc)
        return;

    panelsColor = pc;
    panelsTextColor = tpc;

    QPalette cg = qApp->palette();
    if (panelsColor.isValid())
        cg.setColor(QPalette::Base, QColor(panelsColor));
    qApp->setPalette(cg);

    QPalette panelPal = cg;
    if (panelsTextColor.isValid()) {
        panelPal.setColor(QPalette::Text, QColor(panelsTextColor));
        panelPal.setColor(QPalette::WindowText, QColor(panelsTextColor));
    }
    lv.setPalette(panelPal);
    results->setPalette(panelPal);
}

void ApplicationWindow::setPlot3DOptions()
{
    QList<MyWidget *> windows = windowsList();
    for (int i = 0; i < int(windows.count()); i++) {
        if (windows.at(i)->inherits("Graph3D")) {
            Graph3D *g = (Graph3D *)windows.at(i);
            g->setSmoothMesh(smooth3DMesh);
            g->setOrtho(orthogonal3DPlots);
            g->setAutoscale(autoscale3DPlots);
        }
    }
}

void ApplicationWindow::createLanguagesList()
{
    registerApplicationWindowJapaneseStrings();
    appTranslator = new QTranslator(this);
    qtTranslator = new QTranslator(this);
    if (!d_builtin_ja_translator)
        d_builtin_ja_translator = sharedBuiltinJaTranslator();
    if (d_builtin_ja_translator)
        qApp->removeTranslator(d_builtin_ja_translator);
    qApp->installTranslator(appTranslator);
    qApp->installTranslator(qtTranslator);

    qmPath = TS_PATH;

    QString lng; // lang, as en_GB
    QString slng; // short lang, as en
    lng = QLocale().name();
    {
        if (lng == "C")
            lng = "en";
        int i = lng.indexOf(QString("."));
        if (i >= 0)
            lng = lng.left(i);
        i = lng.indexOf(QString("_"));
        if (i >= 0)
            slng = lng.left(i);
        else
            slng = lng;
    }
    if (slng.size() > 2)
        slng = slng.left(2);

    QDir dir(qmPath);
    QStringList fileNames = dir.entryList(QStringList("scidavis_*.qm"));
    if (fileNames.size() == 0) {
        // fall back to looking in the executable's directory
        qmPath = QFileInfo(QCoreApplication::applicationFilePath()).path() + "/translations";
        dir.setPath(qmPath);
        fileNames = dir.entryList(QStringList("scidavis_*.qm"));
    }
    for (int i = 0; i < (int)fileNames.size(); i++) {
        QString locale = fileNames[i];
        locale = locale.mid(locale.indexOf('_') + 1);
        locale.truncate(locale.indexOf('.'));
        if (!locales.contains(locale))
            locales.push_back(locale);
    }
    if (!locales.contains("en"))
        locales.push_back("en");
    if (!locales.contains("ja"))
        locales.push_back("ja");
    locales.sort();

    if (appLanguage != "en") {
        if (!appTranslator->load("scidavis_" + appLanguage, qmPath))
            if (!appTranslator->load("scidavis_" + appLanguage))
                if (!appTranslator->load("scidavis_" + lng, qmPath))
                    if (!appTranslator->load("scidavis_" + lng))
                        if (!appTranslator->load("scidavis_" + slng, qmPath))
                            appTranslator->load("scidavis_" + slng);
        if (!qtTranslator->load("qt_" + appLanguage, qmPath + "/qt"))
            if (!qtTranslator->load("qt_" + appLanguage))
                if (!qtTranslator->load("qt_" + lng, qmPath + "/qt"))
                    if (!qtTranslator->load("qt_" + lng))
                        if (!qtTranslator->load("qt_" + slng, qmPath + "/qt"))
                            qtTranslator->load("qt_" + slng);
    }

    // Install built-in full Japanese translator LAST so it has top priority in Qt's LIFO stack
    if (appLanguage.startsWith("ja")) {
        qApp->installTranslator(d_builtin_ja_translator);
    }
}

void ApplicationWindow::switchToLanguage(int param)
{
    if (param >= 0 && param < (int)locales.size())
        switchToLanguage(locales[param]);
}

void ApplicationWindow::switchToLanguage(const QString &locale)
{
    registerApplicationWindowJapaneseStrings();
    QString targetLocale = locale;
    if (targetLocale.startsWith("ja"))
        targetLocale = "ja";
    else if (targetLocale.startsWith("en"))
        targetLocale = "en";

    if (!locales.contains(targetLocale) && targetLocale != "en" && targetLocale != "ja")
        return;

    appLanguage = targetLocale;
    if (!d_builtin_ja_translator)
        d_builtin_ja_translator = sharedBuiltinJaTranslator();

    if (d_builtin_ja_translator)
        qApp->removeTranslator(d_builtin_ja_translator);
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (ApplicationWindow *other = qobject_cast<ApplicationWindow *>(tlw)) {
            if (other == this)
                continue;
            if (other->appTranslator)
                qApp->removeTranslator(other->appTranslator);
            if (other->qtTranslator)
                qApp->removeTranslator(other->qtTranslator);
        }
    }
    qApp->removeTranslator(appTranslator);
    qApp->removeTranslator(qtTranslator);
    delete appTranslator;
    delete qtTranslator;
    appTranslator = new QTranslator(this);
    qtTranslator = new QTranslator(this);

    if (appLanguage != "en") {
        if (!appTranslator->load("scidavis_" + appLanguage, qmPath))
            appTranslator->load("scidavis_" + appLanguage);
        if (!qtTranslator->load("qt_" + appLanguage, qmPath + "/qt"))
            qtTranslator->load("qt_" + appLanguage);
    }
    qApp->installTranslator(appTranslator);
    qApp->installTranslator(qtTranslator);
    if (appLanguage == "ja") {
        qApp->installTranslator(d_builtin_ja_translator);
    }

    if (actionLangJapanese)
        actionLangJapanese->setChecked(appLanguage == "ja");
    if (actionLangEnglish)
        actionLangEnglish->setChecked(appLanguage == "en");

    // A headless/batch session uses the language only while it runs.
    if (persistsUserSettings()) {
        auto &settings = getSettings();
        settings.setValue("/General/Language", appLanguage);
    }

    insertTranslatedStrings();
}

void ApplicationWindow::switchToJapanese()
{
    switchToLanguage("ja");
}

void ApplicationWindow::switchToEnglish()
{
    switchToLanguage("en");
}

unsigned int ApplicationWindow::convertOldToNewColorIndex(unsigned int cindex)
{
    if ((cindex == 13) || (cindex == 14)) // white and light gray
        return cindex + 4;

    if (cindex == 15) // dark gray
        return cindex + 8;

    return cindex;
}

void ApplicationWindow::captureLastGraphGeometry(MultiLayer *ml, bool allowHiddenGeometry)
{
    if (!ml || d_ole_embedded || d_restoring_window_geometry)
        return;

    const bool isFreeMdi = (d_workspace.viewMode() == QMdiArea::SubWindowView)
            && !(d_modern_ribbon
                 && (d_modern_ribbon->workspaceLayoutMode()
                             == ModernRibbonBar::LayoutSplitTableGraph
                     || d_modern_ribbon->isApplyingSplitGeometry()));

    if (isFreeMdi && !ml->isMinimized() && (allowHiddenGeometry || ml->isVisible())) {
        if (ml->isMaximized()) {
            d_last_graph_maximized = true;
            const QRect norm = ml->normalGeometry();
            if (norm.width() >= 260 && norm.height() >= 220) {
                d_last_graph_geometry = QRect(qMax(0, norm.x()), qMax(0, norm.y()),
                                              norm.width(), norm.height());
                d_has_last_graph_geometry = true;
            }
        } else {
            const QRect geo = ml->geometry();
            if (geo.width() >= 260 && geo.height() >= 220) {
                d_last_graph_geometry = QRect(qMax(0, geo.x()), qMax(0, geo.y()),
                                              geo.width(), geo.height());
                d_last_graph_maximized = false;
                d_has_last_graph_geometry = true;
            }
        }
    }
}

void ApplicationWindow::captureLastGraphSettingsForExit(MultiLayer *ml)
{
    if (!ml || d_ole_embedded || d_restoring_window_geometry)
        return;

    Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->layer(1);
    if (!g || !g->plotWidget() || g->isPiePlot() || g->isTernaryPlot() || g->isPolarPlot())
        return;

    SavedGraphSettings s;
    s.valid = true;
    s.pageViewMode = ml->pageViewMode();
    s.pageUnit = int(ml->pageUnit());
    s.pageWidth = ml->pageWidthPhysical();
    s.pageHeight = ml->pageHeightPhysical();
    s.pageDpi = qBound(72, ml->pageDpi(), 2400);
    s.pageZoom = qBound(0.25, ml->pageZoom(), 4.0);
    s.pageBg = ml->pageBackgroundColor().isValid() ? ml->pageBackgroundColor() : QColor(Qt::white);
    s.keepAspectRatio = ml->keepAspectRatio();
    s.targetAspectRatio = (ml->targetAspectRatio() > 0.05 && ml->targetAspectRatio() <= 20.0)
            ? ml->targetAspectRatio()
            : 1.0;
    s.panelLabelsEnabled = ml->hasPanelLabels();
    s.panelLabelPlacement = int(ml->panelLabelPlacement());
    s.panelLabelPattern = ml->panelLabelPattern();
    s.panelLabelFontPt = ml->panelLabelFontPt();

    s.hasVisualStyle = true;
    s.axesFont = plotAxesFont;
    s.numbersFont = plotNumbersFont;
    s.legendFont = plotLegendFont;
    s.legendFrameStyle = legendFrameStyle;
    s.legendTextColor = legendTextColor;
    s.legendBackground = legendBackground;
    s.axesLineWidth = axesLineWidth;
    s.majTicksStyle = majTicksStyle;
    s.minTicksStyle = minTicksStyle;
    s.majTicksLength = majTicksLength;
    s.minTicksLength = minTicksLength;

    auto adoptFontIfValid = [](QFont &dst, const QFont &src) {
        if ((src.pointSizeF() > 0.0 || src.pointSize() > 0) && !src.family().trimmed().isEmpty())
            dst = src;
    };
    adoptFontIfValid(s.axesFont, g->axisTitleFont(QwtPlot::xBottom));
    adoptFontIfValid(s.numbersFont, g->axisFont(QwtPlot::xBottom));
    if (Legend *leg = g->legend()) {
        adoptFontIfValid(s.legendFont, leg->font());
        s.legendFrameStyle = leg->frameStyle();
        s.legendTextColor = leg->textColor();
        s.legendBackground = leg->backgroundColor();
    }
    if (Plot *pl = g->plotWidget()) {
        s.axesLineWidth = qMax(1, pl->axesLinewidth());
        if (const ScaleDraw *sd = dynamic_cast<const ScaleDraw *>(pl->axisScaleDraw(QwtPlot::xBottom))) {
            s.majTicksStyle = sd->majorTicksStyle();
            s.minTicksStyle = sd->minorTicksStyle();
        }
        s.majTicksLength = qMax(1, g->majorTickLength());
        s.minTicksLength = qMax(1, g->minorTickLength());
    }
    if (g->property("_toriBracketUnitFmt").toBool())
        s.unitFormat = 2;
    else if (g->axisUnitFormat() == Column::UnitFormat::ISIJSlash)
        s.unitFormat = 1;
    else
        s.unitFormat = 0;

    if (Grid *gr = g->grid()) {
        s.gridMajorX = gr->xEnabled();
        s.gridMajorY = gr->yEnabled();
        s.gridMinorX = gr->xMinEnabled();
        s.gridMinorY = gr->yMinEnabled();
    }

    d_exit_graph_settings = s;
}

void ApplicationWindow::captureLastTableGeometry(Table *t, bool allowHiddenGeometry)
{
    if (!t || d_ole_embedded || d_restoring_window_geometry)
        return;

    const bool isFreeMdi = (d_workspace.viewMode() == QMdiArea::SubWindowView)
            && !(d_modern_ribbon
                 && (d_modern_ribbon->workspaceLayoutMode()
                             == ModernRibbonBar::LayoutSplitTableGraph
                     || d_modern_ribbon->isApplyingSplitGeometry()));
    if (!isFreeMdi || t->isMinimized() || t->isMaximized()
        || (!allowHiddenGeometry && !t->isVisible()))
        return;

    const QRect geo = t->geometry();
    if (geo.width() >= 260 && geo.height() >= 200) {
        d_last_table_geometry = QRect(qMax(0, geo.x()), qMax(0, geo.y()),
                                      geo.width(), geo.height());
        d_has_last_table_geometry = true;
    }
}

void ApplicationWindow::applyLastGraphWindowGeometry(MultiLayer *g)
{
    if (!g)
        return;
    const bool prevRestoring = d_restoring_window_geometry;
    d_restoring_window_geometry = true;
    const QSize vp = d_workspace.viewport() ? d_workspace.viewport()->size() : QSize();
    Table *srcTbl =
            qobject_cast<Table *>(g->property("_tg_initial_source_table").value<QObject *>());
    const bool hasValidSavedGeo = d_has_last_graph_geometry && d_last_graph_geometry.width() >= 260
            && d_last_graph_geometry.height() >= 220;
    const bool canPlaceBesideTable = !d_last_graph_maximized && srcTbl && !srcTbl->isHidden()
            && !srcTbl->isMinimized() && !srcTbl->isMaximized() && vp.width() >= 680
            && vp.height() >= 240;

    if (hasValidSavedGeo || canPlaceBesideTable) {
        QSize fallbackSz = g->size();
        if (fallbackSz.width() < 360 || fallbackSz.height() < 300)
            fallbackSz = QSize(560, 480);
        QRect target = hasValidSavedGeo ? d_last_graph_geometry : QRect(g->pos(), fallbackSz);
        if (vp.width() >= 320 && vp.height() >= 240) {
            target.setWidth(qBound(260, target.width(), qMax(260, vp.width() - 8)));
            target.setHeight(qBound(220, target.height(), qMax(220, vp.height() - 8)));
            target.moveLeft(qBound(0, target.x(), qMax(0, vp.width() - target.width())));
            target.moveTop(qBound(0, target.y(), qMax(0, vp.height() - target.height())));

            if (!hasValidSavedGeo && canPlaceBesideTable) {
                const QRect srcGeo = srcTbl->geometry();
                const int rightX = srcGeo.right() + 8;
                const int remRight = vp.width() - rightX - 4;
                const int alignedTop =
                        qBound(0, srcGeo.top(), qMax(0, vp.height() - target.height()));
                if (remRight >= target.width()) {
                    target.moveTo(rightX, alignedTop);
                } else if (remRight >= 360
                           || (vp.width() - target.width() >= 180 && srcGeo.left() < 120)) {
                    target.moveTo(qMax(0, vp.width() - target.width() - 4), alignedTop);
                }
            }

            // Gently cascade if another visible MultiLayer occupies the exact same top-left
            for (int step = 0; step < 8; ++step) {
                bool overlapsExact = false;
                for (QMdiSubWindow *sw : d_workspace.subWindowList()) {
                    MultiLayer *other = qobject_cast<MultiLayer *>(sw);
                    if (!other || other == g || other->isHidden() || other->isMinimized())
                        continue;
                    if (qAbs(other->x() - target.x()) <= 8 && qAbs(other->y() - target.y()) <= 8) {
                        overlapsExact = true;
                        break;
                    }
                }
                if (!overlapsExact)
                    break;
                int nx = target.x() + 26;
                int ny = target.y() + 26;
                if (nx + target.width() > vp.width() || ny + target.height() > vp.height()) {
                    nx = 12;
                    ny = 12;
                }
                target.moveTo(nx, ny);
            }
        }
        g->resize(target.size());
        g->move(target.topLeft());
        if (hasValidSavedGeo)
            g->setProperty("_tg_has_saved_window_geo", true);
        if (d_last_graph_maximized)
            g->showMaximized();
        else
            g->show();
    } else {
        g->show();
    }
    d_restoring_window_geometry = prevRestoring;
}

void ApplicationWindow::applyLastTableWindowGeometry(Table *w)
{
    if (!w)
        return;
    const bool prevRestoring = d_restoring_window_geometry;
    d_restoring_window_geometry = true;
    QRect target = d_last_table_geometry;
    const QSize vp = d_workspace.viewport() ? d_workspace.viewport()->size() : QSize();
    if (vp.width() >= 320 && vp.height() >= 240) {
        target.setWidth(qBound(260, target.width(), qMax(260, vp.width() - 8)));
        target.setHeight(qBound(200, target.height(), qMax(200, vp.height() - 8)));
        target.moveLeft(qBound(0, target.x(), qMax(0, vp.width() - target.width())));
        target.moveTop(qBound(0, target.y(), qMax(0, vp.height() - target.height())));
    }
    w->resize(target.size());
    w->move(target.topLeft());
    w->showNormal();
    d_restoring_window_geometry = prevRestoring;
}

void ApplicationWindow::applyLastGraphSettings(MultiLayer *g)
{
    if (!g || !d_restored_graph_settings.valid)
        return;
    const SavedGraphSettings &s = d_restored_graph_settings;
    const bool prevRestoring = d_restoring_window_geometry;
    d_restoring_window_geometry = true;
    g->setKeepAspectRatio(s.keepAspectRatio);
    if (s.targetAspectRatio > 0.05 && s.targetAspectRatio <= 20.0)
        g->setTargetAspectRatio(s.targetAspectRatio);
    if (s.pageBg.isValid())
        g->setPageBackgroundColor(s.pageBg);
    if (s.pageWidth > 1.0 && s.pageHeight > 1.0) {
        g->setPageSizePhysical(s.pageWidth, s.pageHeight,
                               static_cast<MultiLayer::PageUnit>(s.pageUnit), s.pageDpi);
    }
    if (s.panelLabelsEnabled) {
        g->setPanelLabels(
                true, static_cast<MultiLayer::PanelLabelPlacement>(s.panelLabelPlacement),
                s.panelLabelPattern.isEmpty() ? QStringLiteral("(%a)") : s.panelLabelPattern,
                s.panelLabelFontPt);
    }
    d_restoring_window_geometry = prevRestoring;
}

void ApplicationWindow::finalizeLastGraphSettings(MultiLayer *g)
{
    if (!g)
        return;
    const bool isFreeMdi = (d_workspace.viewMode() == QMdiArea::SubWindowView)
            && !(d_modern_ribbon
                 && d_modern_ribbon->workspaceLayoutMode()
                         == ModernRibbonBar::LayoutSplitTableGraph);
    if (!d_restored_graph_settings.valid) {
        if (isFreeMdi && g->property("_tg_initial_source_table").isValid())
            applyLastGraphWindowGeometry(g);
        g->setProperty("_tg_initial_source_table", QVariant());
        return;
    }
    const SavedGraphSettings &s = d_restored_graph_settings;
    const bool prevRestoring = d_restoring_window_geometry;
    d_restoring_window_geometry = true;
    const bool hasPhysicalPage = s.pageViewMode
            || (s.pageUnit != int(MultiLayer::UnitPx))
            || (qAbs(s.pageZoom - 1.0) > 1e-4);
    if (s.pageViewMode)
        g->setPageViewMode(true);
    if (hasPhysicalPage) {
        g->setPageZoom(qBound(0.25, s.pageZoom, 4.0));
        if (s.pageWidth > 1.0 && s.pageHeight > 1.0) {
            g->setPageSizePhysical(s.pageWidth, s.pageHeight,
                                   static_cast<MultiLayer::PageUnit>(s.pageUnit), s.pageDpi);
        }
        g->setKeepAspectRatio(s.keepAspectRatio);
        if (s.targetAspectRatio > 0.05 && s.targetAspectRatio <= 20.0)
            g->setTargetAspectRatio(s.targetAspectRatio);
        if (s.keepAspectRatio && qFuzzyCompare(s.targetAspectRatio, 1.0))
            g->applySquareCanvasFrame70Percent();
    } else {
        g->setKeepAspectRatio(s.keepAspectRatio);
        if (s.targetAspectRatio > 0.05 && s.targetAspectRatio <= 20.0)
            g->setTargetAspectRatio(s.targetAspectRatio);
    }
    if (Graph *ag = g->activeGraph()) {
        if (ag->legend() && ag->isLegendVisible())
            ag->placeLegendAnchor(Graph::LegendAutoAvoidData);
    }
    if (isFreeMdi && (d_has_last_graph_geometry || g->property("_tg_initial_source_table").isValid()))
        applyLastGraphWindowGeometry(g);
    g->setProperty("_tg_initial_source_table", QVariant());
    d_restoring_window_geometry = prevRestoring;
}

QSettings &ApplicationWindow::getSettings()
{
    const QByteArray overrideIni = qgetenv("TORIGRAPH_SETTINGS_INI");
    if (!overrideIni.isEmpty()) {
        static QString s_iniPath;
        static QScopedPointer<QSettings> s_overrideSettings;
        const QString wantPath = QString::fromLocal8Bit(overrideIni);
        if (!s_overrideSettings || s_iniPath != wantPath) {
            s_iniPath = wantPath;
            s_overrideSettings.reset(new QSettings(s_iniPath, QSettings::IniFormat));
        }
        return *s_overrideSettings;
    }
#ifdef Q_OS_MAC // Mac
    static QSettings d_settings(QSettings::IniFormat, QSettings::UserScope, "ToriGraph", "ToriGraph");
#else
    static QSettings d_settings(QSettings::NativeFormat, QSettings::UserScope, "ToriGraph",
                                "ToriGraph");
#endif
    return d_settings;
}

void ApplicationWindow::localeChanged()
{
    foreach (QObject *obj, findChildren<QWidget *>())
        QApplication::postEvent(obj, new QEvent(QEvent::LocaleChange));
}

// ============================================================================
// Phase 4: OriginPro Page/Layer/Frame UX, Gadgets, Fitting, Signal, Stats & Digitizer
// ============================================================================
void ApplicationWindow::showPageSetupDialog()
{
    if (isNonInteractiveContext(this))
        return;
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;
    QPointer<ApplicationWindow> self(this);
    QPointer<MultiLayer> safeMl(static_cast<MultiLayer *>(w));
    const QString before = safeMl->captureUndoSnapshot();
    const int prevCount = safeMl->undoStack() ? safeMl->undoStack()->count() : 0;
    safeMl->showPageSetupDialog();
    if (!self)
        return;
    if (safeMl && (!safeMl->undoStack() || safeMl->undoStack()->count() == prevCount))
        safeMl->pushUndoSnapshot(tr("Page Setup"), before);
    syncGraphPageAndToolbarMenuState();
    if (safeMl)
        emit modified();
}

void ApplicationWindow::fitPageToLayers()
{
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;
    MultiLayer *ml = static_cast<MultiLayer *>(w);
    const QString before = ml->captureUndoSnapshot();
    ml->fitPageToLayers();
    ml->pushUndoSnapshot(tr("Fit Page to Layers"), before);
    emit modified();
}

void ApplicationWindow::fitLayersToPage()
{
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;
    MultiLayer *ml = static_cast<MultiLayer *>(w);
    const QString before = ml->captureUndoSnapshot();
    ml->fitLayersToPage();
    ml->pushUndoSnapshot(tr("Fit Layers to Page"), before);
    emit modified();
}

void ApplicationWindow::togglePageViewMode()
{
    QWidget *w = d_workspace.activeSubWindow();
    if (!w || !w->inherits("MultiLayer"))
        return;
    MultiLayer *ml = static_cast<MultiLayer *>(w);
    const QString before = ml->captureUndoSnapshot();
    ml->setPageViewMode(!ml->pageViewMode());
    ml->pushUndoSnapshot(tr("Toggle Page View"), before);
    syncGraphPageAndToolbarMenuState();
}

void ApplicationWindow::addLinkedTopRightLayer()
{
    QWidget *w = d_workspace.activeSubWindow();
    MultiLayer *ml = (w && w->inherits("MultiLayer")) ? static_cast<MultiLayer *>(w) : nullptr;
    if (!ml) {
        const QString msg =
                tr("Please activate a 2D Graph window first before adding a linked layer.");
        if (statusBar())
            statusBar()->showMessage(msg, 4500);
        if (!isNonInteractiveContext(this))
            QMessageBox::information(this, tr("Add Linked Layer"), msg);
        return;
    }
    if (ml->isEmpty())
        ml->addLayer();
    const QString before = ml->captureUndoSnapshot();
    if (ml->addLinkedLayer()) {
        ml->pushUndoSnapshot(tr("Add Linked Layer"), before);
        if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
            d_graph_inspector_dock->setTargetMultiLayer(ml, true);
        if (statusBar())
            statusBar()->showMessage(tr("Added linked top/right axis layer to %1.").arg(ml->name()),
                                     3500);
        emit modified();
    }
}

void ApplicationWindow::addZoomInsetLayer()
{
    QWidget *w = d_workspace.activeSubWindow();
    MultiLayer *ml = (w && w->inherits("MultiLayer")) ? static_cast<MultiLayer *>(w) : nullptr;
    if (!ml) {
        const QString msg =
                tr("Please activate a 2D Graph window first before adding a zoom inset layer.");
        if (statusBar())
            statusBar()->showMessage(msg, 4500);
        if (!isNonInteractiveContext(this))
            QMessageBox::information(this, tr("Add Inset Layer"), msg);
        return;
    }
    if (ml->isEmpty())
        ml->addLayer();
    const QString before = ml->captureUndoSnapshot();
    if (ml->addInsetLayer()) {
        ml->pushUndoSnapshot(tr("Add Inset Layer"), before);
        if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
            d_graph_inspector_dock->setTargetMultiLayer(ml, true);
        if (statusBar())
            statusBar()->showMessage(tr("Added zoom inset layer to %1.").arg(ml->name()), 3500);
        emit modified();
    }
}

void ApplicationWindow::applyFrameBoxInward()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;
    MultiLayer *ml = g->multiLayerParent();
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    g->applyFramePreset(Graph::FrameBoxInwardTicks);
    if (ml)
        ml->pushUndoSnapshot(tr("Apply Frame Preset"), before);
    emit modified();
}

void ApplicationWindow::applyFrameBoxPlain()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;
    MultiLayer *ml = g->multiLayerParent();
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    g->applyFramePreset(Graph::FrameBoxPlainOpposite);
    if (ml)
        ml->pushUndoSnapshot(tr("Apply Frame Preset"), before);
    emit modified();
}

void ApplicationWindow::applyFrameOpenL()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;
    MultiLayer *ml = g->multiLayerParent();
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    g->applyFramePreset(Graph::FrameOpenL);
    if (ml)
        ml->pushUndoSnapshot(tr("Apply Frame Preset"), before);
    emit modified();
}

void ApplicationWindow::applyFrameZeroCrossed()
{
    Graph *g = activeGraphForAnalysis(this, d_workspace, btnPointer, false);
    if (!g)
        return;
    MultiLayer *ml = g->multiLayerParent();
    const QString before = ml ? ml->captureUndoSnapshot() : QString();
    g->applyFramePreset(Graph::FrameZeroCrossed);
    if (ml)
        ml->pushUndoSnapshot(tr("Apply Frame Preset"), before);
    emit modified();
}

void ApplicationWindow::toggleGraphMiniToolbar(bool enabled)
{
    GraphMiniToolbar::setMiniToolbarEnabled(enabled);
    syncGraphPageAndToolbarMenuState();
}

void ApplicationWindow::syncGraphPageAndToolbarMenuState()
{
    updateStatusBarForWindow(d_status_info, d_workspace.activeSubWindow());
    if (actionToggleMiniToolbar) {
        actionToggleMiniToolbar->blockSignals(true);
        actionToggleMiniToolbar->setChecked(GraphMiniToolbar::isMiniToolbarEnabled());
        actionToggleMiniToolbar->blockSignals(false);
    }
    if (actionTogglePageView) {
        bool pageViewOn = false;
        QWidget *w = d_workspace.activeSubWindow();
        if (w && w->inherits("MultiLayer"))
            pageViewOn = static_cast<MultiLayer *>(w)->pageViewMode();
        actionTogglePageView->blockSignals(true);
        actionTogglePageView->setChecked(pageViewOn);
        actionTogglePageView->blockSignals(false);
    }
    if (actionToggleGraphInspector) {
        const bool inspectorExpanded = d_graph_inspector_dock
                && !d_graph_inspector_dock->isHidden()
                && !d_graph_inspector_dock->isCollapsedRail();
        actionToggleGraphInspector->blockSignals(true);
        actionToggleGraphInspector->setChecked(inspectorExpanded);
        actionToggleGraphInspector->blockSignals(false);
    }
    if (d_modern_ribbon)
        d_modern_ribbon->syncCheckableStates();
}

// ============================================================================
// Phase 5: Copy/Paste Graph Format, Batch Plotting, Style Presets & Analysis
// ============================================================================
namespace {

struct CopiedCurveFormatSpec
{
    QPen pen;
    QBrush brush;
    QwtPlotCurve::CurveStyle curveStyle = QwtPlotCurve::Lines;
    QwtSymbol symbol;
    bool symbolHollow = false;
};

struct CopiedLayerFormatSpec
{
    bool valid = false;
    QFont titleFont;
    QColor titleColor = QColor(Qt::black);
    int titleAlign = Qt::AlignHCenter;

    QVector<bool> enabledAxes;
    bool globalBackbones = true;
    bool axisBackboneVisible[4] = { true, true, true, true };
    QFont axisTitleFonts[4];
    QFont axisTickFonts[4];
    QColor axisTitleColors[4];
    int axisTitleAlignments[4] = { Qt::AlignHCenter, Qt::AlignHCenter, Qt::AlignHCenter,
                                   Qt::AlignHCenter };
    int labelRotations[4] = { 0, 0, 0, 0 };
    QStringList axesColors;
    QStringList axesNumColors;
    QStringList enabledTickLabels;
    QList<int> axesBaselines;

    int axesLinewidth = 1;
    QList<int> majorTicksType;
    QList<int> minorTicksType;
    int minorTickLength = 5;
    int majorTickLength = 9;

    double tickDivideFactors[4] = { 1.0, 1.0, 1.0, 1.0 };
    QString tickPrefixes[4];
    QString tickSuffixes[4];
    Graph::AxisBreakSpec axisBreaks[4];

    bool hasAxisScale[4] = { false, false, false, false };
    double scaleStart[4] = { 0.0, 0.0, 0.0, 0.0 };
    double scaleEnd[4] = { 1.0, 1.0, 1.0, 1.0 };
    double scaleStep[4] = { 0.0, 0.0, 0.0, 0.0 };
    int scaleMaxMajor[4] = { 5, 5, 5, 5 };
    int scaleMaxMinor[4] = { 5, 5, 5, 5 };
    int scaleType[4] = { 0, 0, 0, 0 };
    bool scaleInverted[4] = { false, false, false, false };
    bool isNumericAxis[4] = { true, true, true, true };
    int labelNumFormat[4] = { 0, 0, 0, 0 };
    int labelNumPrecision[4] = { 6, 6, 6, 6 };
    QStringList axisFormulas;

    bool framed = false;
    int canvasFrameWidth = 1;
    QColor canvasFrameColor = QColor(Qt::black);
    QColor canvasBackground = QColor(Qt::white);
    QColor backgroundColor = QColor(Qt::white);
    bool antialiasing = true;

    bool hasGrid = false;
    bool gridXEnabled = false;
    bool gridYEnabled = false;
    bool gridXMinEnabled = false;
    bool gridYMinEnabled = false;
    bool gridXZero = false;
    bool gridYZero = false;
    QPen gridMajPenX;
    QPen gridMinPenX;
    QPen gridMajPenY;
    QPen gridMinPenY;

    bool hasLegend = false;
    QFont legendFont;
    QColor legendTextColor = QColor(Qt::black);
    QColor legendBgColor = QColor(Qt::white);
    int legendFrameStyle = 0;
    int legendNumColumns = 1;
    bool hasLegendNormPos = false;
    double legendNormX = 0.0;
    double legendNormY = 0.0;

    QVector<CopiedCurveFormatSpec> curves;
    QVector<Graph::ReferenceLineSpec> referenceLines;
};

struct CopiedGraphFormatClipboard
{
    bool valid = false;
    bool pageViewMode = false;
    MultiLayer::PageUnit pageUnit = MultiLayer::UnitPx;
    double pageWidthPhysical = 520.0;
    double pageHeightPhysical = 520.0;
    int pageDpi = 300;
    QColor pageBackgroundColor = QColor(Qt::white);

    bool keepAspectRatio = false;
    double targetAspectRatio = 0.0;
    QSize windowSize;
    QSize layerCanvasSize;
    int leftMargin = 5;
    int rightMargin = 5;
    int topMargin = 5;
    int bottomMargin = 5;
    int rowsSpacing = 5;
    int colsSpacing = 5;
    int horAlign = 0;
    int vertAlign = 0;

    CopiedLayerFormatSpec activeLayer;
    QVector<CopiedLayerFormatSpec> allLayers;
};

static CopiedGraphFormatClipboard s_copiedGraphFormat;

static CopiedLayerFormatSpec captureLayerFormat(Graph *g)
{
    CopiedLayerFormatSpec spec;
    if (!g || !g->plotWidget())
        return spec;
    spec.valid = true;

    QwtText titleTxt = g->plotWidget()->title();
    spec.titleFont = titleTxt.font();
    if (spec.titleFont.family().isEmpty())
        spec.titleFont.setFamily("Arial");
    spec.titleColor = titleTxt.color();
    spec.titleAlign = titleTxt.renderFlags();

    spec.enabledAxes = g->enabledAxes();
    spec.globalBackbones = g->axesBackbones();
    const QList<Graph::AxisType> aTypes = g->axesType();
    spec.axisFormulas = g->getAxesFormulas();
    for (int a = 0; a < 4; ++a) {
        spec.axisBackboneVisible[a] = g->isAxisBackboneVisible(a);
        spec.axisTitleFonts[a] = g->axisTitleFont(a);
        if (spec.axisTitleFonts[a].family().isEmpty())
            spec.axisTitleFonts[a].setFamily("Arial");
        spec.axisTickFonts[a] = g->axisFont(a);
        if (spec.axisTickFonts[a].family().isEmpty())
            spec.axisTickFonts[a].setFamily("Arial");
        spec.axisTitleColors[a] = g->axisTitleColor(a);
        spec.axisTitleAlignments[a] = g->axisTitleAlignment(a);
        spec.labelRotations[a] = g->labelsRotation(a);
        spec.tickDivideFactors[a] = g->tickDivideFactor(a);
        spec.tickPrefixes[a] = g->tickPrefix(a);
        spec.tickSuffixes[a] = g->tickSuffix(a);
        spec.axisBreaks[a] = g->axisBreak(a);
        if (const QwtScaleDiv *scDiv = g->plotWidget()->axisScaleDiv(a)) {
            spec.hasAxisScale[a] = true;
            spec.scaleStart[a] = qMin(scDiv->lowerBound(), scDiv->upperBound());
            spec.scaleEnd[a] = qMax(scDiv->lowerBound(), scDiv->upperBound());
            spec.scaleStep[a] = g->axisStep(a);
            spec.scaleMaxMajor[a] = g->plotWidget()->axisMaxMajor(a);
            spec.scaleMaxMinor[a] = g->plotWidget()->axisMaxMinor(a);
            const QwtScaleEngine *scEng = g->plotWidget()->axisScaleEngine(a);
            spec.scaleType[a] = Plot::isLog10ScaleEngine(scEng) ? 1 : 0;
            spec.scaleInverted[a] = scEng ? scEng->testAttribute(QwtScaleEngine::Inverted) : false;
        }
        spec.isNumericAxis[a] =
                (a < aTypes.size()) ? (aTypes[a] == Graph::AxisType::Numeric) : true;
        spec.labelNumFormat[a] = g->plotWidget()->axisLabelFormat(a);
        spec.labelNumPrecision[a] = g->plotWidget()->axisLabelPrecision(a);
    }
    spec.axesColors = g->axesColors();
    spec.axesNumColors = g->axesNumColors();
    spec.enabledTickLabels = g->enabledTickLabels();
    spec.axesBaselines = g->axesBaseline();

    spec.axesLinewidth = g->plotWidget()->axesLinewidth();
    spec.majorTicksType = g->plotWidget()->getMajorTicksType();
    spec.minorTicksType = g->plotWidget()->getMinorTicksType();
    spec.minorTickLength = g->minorTickLength();
    spec.majorTickLength = g->majorTickLength();

    spec.framed = g->framed();
    spec.canvasFrameWidth = g->canvasFrameWidth();
    spec.canvasFrameColor = g->canvasFrameColor();
    spec.canvasBackground = g->plotWidget()->canvasBackground();
    spec.backgroundColor = g->plotWidget()->paletteBackgroundColor();
    spec.antialiasing = g->antialiasing();

    if (Grid *gr = g->grid()) {
        spec.hasGrid = true;
        spec.gridXEnabled = gr->xEnabled();
        spec.gridYEnabled = gr->yEnabled();
        spec.gridXMinEnabled = gr->xMinEnabled();
        spec.gridYMinEnabled = gr->yMinEnabled();
        spec.gridXZero = gr->xZeroLineEnabled();
        spec.gridYZero = gr->yZeroLineEnabled();
        spec.gridMajPenX = gr->majPenX();
        spec.gridMinPenX = gr->minPenX();
        spec.gridMajPenY = gr->majPenY();
        spec.gridMinPenY = gr->minPenY();
    }

    if (Legend *leg = g->legend()) {
        spec.hasLegend = true;
        spec.legendFont = leg->font();
        if (spec.legendFont.family().isEmpty())
            spec.legendFont.setFamily("Arial");
        spec.legendTextColor = leg->textColor();
        spec.legendBgColor = leg->backgroundColor();
        spec.legendFrameStyle = leg->frameStyle();
        spec.legendNumColumns = leg->numColumns();
        if (g->plotWidget() && g->plotWidget()->canvas()) {
            const QRect cr = g->plotWidget()->canvas()->contentsRect();
            if (cr.width() > 4 && cr.height() > 4) {
                const QPoint tl = leg->rect().topLeft();
                spec.hasLegendNormPos = true;
                spec.legendNormX = double(tl.x() - cr.left()) / double(cr.width());
                spec.legendNormY = double(tl.y() - cr.top()) / double(cr.height());
            }
        }
    }

    for (int i = 0; i < g->curves(); ++i) {
        QwtPlotCurve *c = g->curve(i);
        if (!c || c->rtti() == QwtPlotItem::Rtti_PlotSpectrogram
            || g->curveType(i) == Graph::ErrorBars)
            continue;
        CopiedCurveFormatSpec cf;
        cf.pen = c->pen();
        cf.brush = c->brush();
        cf.curveStyle = c->style();
        cf.symbol = c->symbol();
        cf.symbolHollow = Graph::isSymbolWhiteHollow(cf.symbol);
        spec.curves.append(cf);
    }

    spec.referenceLines = g->referenceLines();
    return spec;
}

static void applyLegendNormalizedPosition(Graph *dstG, const CopiedLayerFormatSpec &spec)
{
    if (!dstG || !spec.valid || !spec.hasLegend || !spec.hasLegendNormPos)
        return;
    Legend *leg = dstG->legend();
    if (!leg || !dstG->plotWidget() || !dstG->plotWidget()->canvas())
        return;
    dstG->plotWidget()->updateLayout();
    const QRect cr = dstG->plotWidget()->canvas()->contentsRect();
    if (cr.width() > 4 && cr.height() > 4) {
        const int nx = cr.left() + qRound(spec.legendNormX * double(cr.width()));
        const int ny = cr.top() + qRound(spec.legendNormY * double(cr.height()));
        leg->setOrigin(QPoint(nx, ny));
    }
}

static void applyLayerFormatNonDestructive(
        Graph *dstG, const CopiedLayerFormatSpec &spec,
        int scopeFlags = ApplicationWindow::FormatDefaultSafe,
        bool isChildOverlay = false)
{
    if (!dstG || !dstG->plotWidget() || !spec.valid)
        return;

    const bool doFonts = (scopeFlags & ApplicationWindow::FormatFonts) != 0;
    const bool doColors = (scopeFlags & ApplicationWindow::FormatColorsAndSymbols) != 0;
    const bool doFrame = (scopeFlags & ApplicationWindow::FormatFrameAndTicks) != 0;
    const bool doScales = (scopeFlags & ApplicationWindow::FormatAxisScales) != 0;
    const bool doBreaksRef = (scopeFlags & ApplicationWindow::FormatBreaksAndRefLines) != 0;

    const bool isPoleFig = dstG->isPoleFigurePlot();
    const bool linkedOverlay = isChildOverlay || (dstG->linkedParentLayer() != nullptr);

    if (doFonts) {
        dstG->setTitleFont(spec.titleFont);
        dstG->setTitleAlignment(spec.titleAlign);
    }
    if (doColors)
        dstG->setTitleColor(spec.titleColor);

    if (doFrame && !isPoleFig) {
        if (!linkedOverlay) {
            if (spec.enabledAxes.size() == 4)
                dstG->enableAxes(spec.enabledAxes);
            if (spec.enabledTickLabels.size() == 4)
                dstG->setEnabledTickLabels(spec.enabledTickLabels);
            dstG->drawAxesBackbones(spec.globalBackbones);
            if (spec.axesBaselines.size() == 4)
                dstG->setAxesBaseline(spec.axesBaselines);
        }
        dstG->setAxesLinewidth(spec.axesLinewidth);
        if (spec.majorTicksType.size() == 4)
            dstG->setMajorTicksType(spec.majorTicksType);
        if (spec.minorTicksType.size() == 4)
            dstG->setMinorTicksType(spec.minorTicksType);
        dstG->setTicksLength(spec.minorTickLength, spec.majorTickLength);
    }

    if (doColors && !isPoleFig) {
        if (spec.axesColors.size() == 4)
            dstG->setAxesColors(spec.axesColors);
        if (spec.axesNumColors.size() == 4)
            dstG->setAxesNumColors(spec.axesNumColors);
        dstG->setYAxisTitleColor(spec.axisTitleColors[QwtPlot::yLeft]);
        dstG->setRightAxisTitleColor(spec.axisTitleColors[QwtPlot::yRight]);
        dstG->setXAxisTitleColor(spec.axisTitleColors[QwtPlot::xBottom]);
        dstG->setTopAxisTitleColor(spec.axisTitleColors[QwtPlot::xTop]);
    }

    if (doFonts && !isPoleFig) {
        dstG->setYAxisTitleAlignment(spec.axisTitleAlignments[QwtPlot::yLeft]);
        dstG->setRightAxisTitleAlignment(spec.axisTitleAlignments[QwtPlot::yRight]);
        dstG->setXAxisTitleAlignment(spec.axisTitleAlignments[QwtPlot::xBottom]);
        dstG->setTopAxisTitleAlignment(spec.axisTitleAlignments[QwtPlot::xTop]);
    }

    if (!isPoleFig) {
        for (int a = 0; a < 4; ++a) {
            if (doFonts) {
                dstG->setAxisTitleFont(a, spec.axisTitleFonts[a]);
                dstG->setAxisFont(a, spec.axisTickFonts[a]);
                if (a == QwtPlot::xBottom || a == QwtPlot::xTop)
                    dstG->setAxisLabelRotation(a, spec.labelRotations[a]);
            }
            if (doFrame && !linkedOverlay)
                dstG->setAxisBackboneVisible(a, spec.axisBackboneVisible[a]);
            if (doScales) {
                dstG->setTickLabelTransform(a, spec.tickDivideFactors[a], spec.tickPrefixes[a],
                                            spec.tickSuffixes[a]);
                if (spec.isNumericAxis[a]
                    && dstG->axesType().value(a, Graph::AxisType::Numeric)
                            == Graph::AxisType::Numeric) {
                    dstG->setLabelsNumericFormat(a, spec.labelNumFormat[a],
                                                 spec.labelNumPrecision[a],
                                                 spec.axisFormulas.value(a));
                }
                if (spec.hasAxisScale[a] && !dstG->hasColorScaleOnAxis(a)) {
                    dstG->setScale(a, spec.scaleStart[a], spec.scaleEnd[a], spec.scaleStep[a],
                                   spec.scaleMaxMajor[a], spec.scaleMaxMinor[a], spec.scaleType[a],
                                   spec.scaleInverted[a]);
                }
            }
            if (doBreaksRef) {
                if (spec.axisBreaks[a].enabled)
                    dstG->setAxisBreak(a, spec.axisBreaks[a]);
                else
                    dstG->clearAxisBreak(a);
            }
        }
    }

    if (doFrame) {
        if (!isPoleFig)
            dstG->drawCanvasFrame(spec.framed, spec.canvasFrameWidth, spec.canvasFrameColor);
        if (!linkedOverlay || dstG->plotWidget()->canvasBackground().alpha() > 0)
            dstG->setCanvasBackground(spec.canvasBackground);
        if (!linkedOverlay || dstG->plotWidget()->paletteBackgroundColor().alpha() > 0)
            dstG->setBackgroundColor(spec.backgroundColor);
        dstG->setAntialiasing(spec.antialiasing, false);

        if (!isPoleFig && !linkedOverlay && spec.hasGrid && dstG->grid()) {
            Grid *gr = dstG->grid();
            gr->enableX(spec.gridXEnabled);
            gr->enableY(spec.gridYEnabled);
            gr->enableXMin(spec.gridXMinEnabled);
            gr->enableYMin(spec.gridYMinEnabled);
            gr->enableZeroLineX(spec.gridXZero);
            gr->enableZeroLineY(spec.gridYZero);
            gr->setMajPenX(spec.gridMajPenX);
            gr->setMinPenX(spec.gridMinPenX);
            gr->setMajPenY(spec.gridMajPenY);
            gr->setMinPenY(spec.gridMinPenY);
        }
    }

    if (spec.hasLegend && dstG->legend()) {
        Legend *leg = dstG->legend();
        if (doFonts)
            leg->setFont(spec.legendFont);
        if (doColors) {
            leg->setTextColor(spec.legendTextColor);
            leg->setBackgroundColor(spec.legendBgColor);
        }
        if (doFrame) {
            leg->setFrameStyle(spec.legendFrameStyle);
            leg->setNumColumns(spec.legendNumColumns);
            applyLegendNormalizedPosition(dstG, spec);
        }
    }

    if (doColors && !spec.curves.isEmpty()) {
        int visibleIdx = 0;
        for (int i = 0; i < dstG->curves(); ++i) {
            QwtPlotCurve *c = dstG->curve(i);
            if (!c || c->rtti() == QwtPlotItem::Rtti_PlotSpectrogram
                || dstG->curveType(i) == Graph::ErrorBars)
                continue;
            const bool directMatch = (visibleIdx < spec.curves.size());
            const CopiedCurveFormatSpec &cf =
                    spec.curves[directMatch ? visibleIdx : (spec.curves.size() - 1)];
            if (directMatch) {
                dstG->setCurveStyle(i, cf.curveStyle);
                dstG->setCurvePen(i, cf.pen);
                dstG->setCurveBrush(i, cf.brush);
                dstG->setCurveSymbol(i, cf.symbol);
                if (cf.symbolHollow)
                    dstG->setCurveSymbolHollow(i, true);
            } else {
                QPen p = c->pen();
                p.setWidthF(cf.pen.widthF());
                p.setStyle(cf.pen.style());
                p.setCapStyle(cf.pen.capStyle());
                p.setJoinStyle(cf.pen.joinStyle());
                dstG->setCurvePen(i, p);
                QwtSymbol s = c->symbol();
                if (cf.symbol.style() != QwtSymbol::NoSymbol && s.style() == QwtSymbol::NoSymbol)
                    s.setStyle(cf.symbol.style());
                s.setSize(cf.symbol.size());
                QPen sp = s.pen();
                sp.setWidthF(cf.symbol.pen().widthF());
                s.setPen(sp);
                dstG->setCurveSymbol(i, s);
                if (cf.symbolHollow || Graph::isSymbolWhiteHollow(s))
                    dstG->setCurveSymbolHollow(i, cf.symbolHollow);
            }
            ++visibleIdx;
        }
    }

    if (doBreaksRef) {
        dstG->setReferenceLines(spec.referenceLines);
        dstG->updateStatReferenceLines();
    }
    dstG->replot();
}

} // anonymous namespace

void ApplicationWindow::copyGraphFormat()
{
    QWidget *w = d_workspace.activeSubWindow();
    MultiLayer *srcMl = (w && w->inherits("MultiLayer")) ? static_cast<MultiLayer *>(w) : nullptr;
    if (!srcMl || srcMl->isEmpty())
        return;

    s_copiedGraphFormat = CopiedGraphFormatClipboard();
    s_copiedGraphFormat.valid = true;
    s_copiedGraphFormat.pageViewMode = srcMl->pageViewMode();
    s_copiedGraphFormat.pageUnit = srcMl->pageUnit();
    s_copiedGraphFormat.pageWidthPhysical = srcMl->pageWidthPhysical();
    s_copiedGraphFormat.pageHeightPhysical = srcMl->pageHeightPhysical();
    s_copiedGraphFormat.pageDpi = srcMl->pageDpi();
    s_copiedGraphFormat.pageBackgroundColor = srcMl->pageBackgroundColor();
    s_copiedGraphFormat.keepAspectRatio = srcMl->keepAspectRatio();
    s_copiedGraphFormat.targetAspectRatio = srcMl->targetAspectRatio();
    s_copiedGraphFormat.windowSize = srcMl->size();
    s_copiedGraphFormat.layerCanvasSize = srcMl->layerCanvasSize();
    s_copiedGraphFormat.leftMargin = srcMl->leftMargin();
    s_copiedGraphFormat.rightMargin = srcMl->rightMargin();
    s_copiedGraphFormat.topMargin = srcMl->topMargin();
    s_copiedGraphFormat.bottomMargin = srcMl->bottomMargin();
    s_copiedGraphFormat.rowsSpacing = srcMl->rowsSpacing();
    s_copiedGraphFormat.colsSpacing = srcMl->colsSpacing();
    s_copiedGraphFormat.horAlign = srcMl->horizontalAlignement();
    s_copiedGraphFormat.vertAlign = srcMl->verticalAlignement();

    QList<Graph *> layers = srcMl->layersList();
    Graph *actG = srcMl->activeGraph();
    if (!actG && !layers.isEmpty())
        actG = layers.first();
    if (actG)
        s_copiedGraphFormat.activeLayer = captureLayerFormat(actG);
    for (Graph *lg : layers)
        s_copiedGraphFormat.allLayers.append(captureLayerFormat(lg));

    if (statusBar())
        statusBar()->showMessage(tr("Copied graph format from %1.").arg(srcMl->name()), 3000);
}

void ApplicationWindow::pasteGraphFormat()
{
    pasteGraphFormatWithScope(FormatDefaultSafe);
}

void ApplicationWindow::pasteGraphFormatWithScope(int scopeFlags)
{
    if (!s_copiedGraphFormat.valid)
        return;
    QWidget *w = d_workspace.activeSubWindow();
    MultiLayer *dstMl = (w && w->inherits("MultiLayer")) ? static_cast<MultiLayer *>(w) : nullptr;
    if (!dstMl || dstMl->isEmpty())
        return;

    const QList<Graph *> dstLayers = dstMl->layersList();
    int primaryDstCount = 0;
    for (Graph *dg : dstLayers) {
        if (dg && !dg->linkedParentLayer() && !dstMl->isInsetLayer(dg))
            ++primaryDstCount;
    }
    const bool singleToSingle =
            (s_copiedGraphFormat.allLayers.size() <= 1 && primaryDstCount <= 1);
    const bool explicitPageOnly = (scopeFlags == FormatPageDimensions);
    const bool applyPageDims =
            (scopeFlags & FormatPageDimensions) != 0 && (singleToSingle || explicitPageOnly);

    const bool suppressInnerUndo = dstMl->property("_suppressInnerUndoPush").toBool();
    const QString before = suppressInnerUndo ? QString() : dstMl->captureUndoSnapshot();

    if (applyPageDims) {
        dstMl->setMargins(s_copiedGraphFormat.leftMargin, s_copiedGraphFormat.rightMargin,
                          s_copiedGraphFormat.topMargin, s_copiedGraphFormat.bottomMargin);
        dstMl->setSpacing(s_copiedGraphFormat.rowsSpacing, s_copiedGraphFormat.colsSpacing);
        dstMl->setAlignement(s_copiedGraphFormat.horAlign, s_copiedGraphFormat.vertAlign);
        dstMl->setPageBackgroundColor(s_copiedGraphFormat.pageBackgroundColor);
        if (s_copiedGraphFormat.pageWidthPhysical > 0.0
            && s_copiedGraphFormat.pageHeightPhysical > 0.0) {
            dstMl->setPageSizePhysical(s_copiedGraphFormat.pageWidthPhysical,
                                       s_copiedGraphFormat.pageHeightPhysical,
                                       s_copiedGraphFormat.pageUnit, s_copiedGraphFormat.pageDpi);
        }
        dstMl->setPageViewMode(s_copiedGraphFormat.pageViewMode);
        dstMl->setTargetAspectRatio(s_copiedGraphFormat.targetAspectRatio);
        dstMl->setKeepAspectRatio(s_copiedGraphFormat.keepAspectRatio);
        if (s_copiedGraphFormat.windowSize.isValid() && !dstMl->isMaximized())
            dstMl->resize(s_copiedGraphFormat.windowSize);
    } else if ((scopeFlags & FormatFrameAndTicks) != 0) {
        dstMl->setPageBackgroundColor(s_copiedGraphFormat.pageBackgroundColor);
    }

    for (int i = 0; i < dstLayers.size(); ++i) {
        const CopiedLayerFormatSpec &lspec = (i < s_copiedGraphFormat.allLayers.size())
                ? s_copiedGraphFormat.allLayers[i]
                : s_copiedGraphFormat.activeLayer;
        const bool isOverlay = dstLayers[i]
                && (dstLayers[i]->linkedParentLayer() != nullptr
                    || dstMl->isInsetLayer(dstLayers[i]));
        applyLayerFormatNonDestructive(dstLayers[i], lspec, scopeFlags, isOverlay);
    }

    if ((scopeFlags & (FormatFrameAndTicks | FormatFonts)) != 0 || applyPageDims) {
        dstMl->arrangeLayers(false, false);
        if ((scopeFlags & FormatFrameAndTicks) != 0) {
            for (int i = 0; i < dstLayers.size(); ++i) {
                const CopiedLayerFormatSpec &lspec = (i < s_copiedGraphFormat.allLayers.size())
                        ? s_copiedGraphFormat.allLayers[i]
                        : s_copiedGraphFormat.activeLayer;
                if (lspec.hasLegend && lspec.hasLegendNormPos && dstLayers[i]->legend()) {
                    applyLegendNormalizedPosition(dstLayers[i], lspec);
                    dstLayers[i]->replot();
                }
            }
        }
    }

    if (!suppressInnerUndo)
        dstMl->pushUndoSnapshot(tr("Paste Graph Format"), before);
    emit dstMl->modifiedPlot();
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == dstMl)
        d_graph_inspector_dock->setTargetMultiLayer(dstMl, true);
    emit modified();
    if (statusBar())
        statusBar()->showMessage(tr("Applied copied graph format to %1.").arg(dstMl->name()), 3000);
}

void ApplicationWindow::pasteGraphFormatFontsOnly()
{
    pasteGraphFormatWithScope(FormatFonts);
}

void ApplicationWindow::pasteGraphFormatColorsOnly()
{
    pasteGraphFormatWithScope(FormatColorsAndSymbols);
}

void ApplicationWindow::pasteGraphFormatAxisScalesOnly()
{
    pasteGraphFormatWithScope(FormatAxisScales);
}

void ApplicationWindow::pasteGraphFormatToFolder()
{
    if (!s_copiedGraphFormat.valid || !current_folder)
        return;

    QWidget *prevActive = d_workspace.activeSubWindow();
    const QList<MyWidget *> folderWins = current_folder->windowsList();
    int appliedCount = 0;
    for (MyWidget *mw : folderWins) {
        if (!mw || !mw->inherits("MultiLayer"))
            continue;
        MultiLayer *dstMl = static_cast<MultiLayer *>(mw);
        if (dstMl->isEmpty())
            continue;

        const QList<Graph *> dstLayers = dstMl->layersList();
        const bool singleToSingle =
                (s_copiedGraphFormat.allLayers.size() <= 1 && dstLayers.size() <= 1);
        const QString before = dstMl->captureUndoSnapshot();
        if (singleToSingle) {
            dstMl->setMargins(s_copiedGraphFormat.leftMargin, s_copiedGraphFormat.rightMargin,
                              s_copiedGraphFormat.topMargin, s_copiedGraphFormat.bottomMargin);
            dstMl->setSpacing(s_copiedGraphFormat.rowsSpacing, s_copiedGraphFormat.colsSpacing);
            dstMl->setAlignement(s_copiedGraphFormat.horAlign, s_copiedGraphFormat.vertAlign);
            dstMl->setPageBackgroundColor(s_copiedGraphFormat.pageBackgroundColor);
            if (s_copiedGraphFormat.pageWidthPhysical > 0.0
                && s_copiedGraphFormat.pageHeightPhysical > 0.0) {
                dstMl->setPageSizePhysical(
                        s_copiedGraphFormat.pageWidthPhysical,
                        s_copiedGraphFormat.pageHeightPhysical, s_copiedGraphFormat.pageUnit,
                        s_copiedGraphFormat.pageDpi);
            }
            dstMl->setPageViewMode(s_copiedGraphFormat.pageViewMode);
            dstMl->setTargetAspectRatio(s_copiedGraphFormat.targetAspectRatio);
            dstMl->setKeepAspectRatio(s_copiedGraphFormat.keepAspectRatio);
            if (s_copiedGraphFormat.windowSize.isValid() && !dstMl->isMaximized())
                dstMl->resize(s_copiedGraphFormat.windowSize);
        } else {
            dstMl->setPageBackgroundColor(s_copiedGraphFormat.pageBackgroundColor);
        }

        for (int i = 0; i < dstLayers.size(); ++i) {
            const CopiedLayerFormatSpec &lspec = (i < s_copiedGraphFormat.allLayers.size())
                    ? s_copiedGraphFormat.allLayers[i]
                    : s_copiedGraphFormat.activeLayer;
            applyLayerFormatNonDestructive(dstLayers[i], lspec, FormatDefaultSafe);
        }
        dstMl->arrangeLayers(false, false);
        for (int i = 0; i < dstLayers.size(); ++i) {
            const CopiedLayerFormatSpec &lspec = (i < s_copiedGraphFormat.allLayers.size())
                    ? s_copiedGraphFormat.allLayers[i]
                    : s_copiedGraphFormat.activeLayer;
            if (lspec.hasLegend && lspec.hasLegendNormPos && dstLayers[i]->legend()) {
                applyLegendNormalizedPosition(dstLayers[i], lspec);
                dstLayers[i]->replot();
            }
        }
        dstMl->pushUndoSnapshot(tr("Paste Graph Format to Folder"), before);
        if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == dstMl)
            d_graph_inspector_dock->setTargetMultiLayer(dstMl, true);
        ++appliedCount;
    }

    if (prevActive) {
        if (MyWidget *mw = qobject_cast<MyWidget *>(prevActive))
            activateSubWindow(mw);
    }
    if (appliedCount > 0) {
        emit modified();
        if (statusBar()) {
            statusBar()->showMessage(
                    tr("Applied copied graph format to %1 graphs in folder.").arg(appliedCount),
                    3500);
        }
    }
}

void ApplicationWindow::mergeOpenGraphsDialog()
{
    if (isNonInteractiveContext(this))
        return;

    QList<MultiLayer *> folderGraphs;
    QList<MultiLayer *> allGraphs;
    const QList<MyWidget *> folderWins =
            current_folder ? current_folder->windowsList() : QList<MyWidget *>();
    for (MyWidget *mw : folderWins) {
        if (mw && mw->inherits("MultiLayer") && !static_cast<MultiLayer *>(mw)->isEmpty()) {
            MultiLayer *ml = static_cast<MultiLayer *>(mw);
            folderGraphs.append(ml);
            allGraphs.append(ml);
        }
    }
    for (MyWidget *mw : windowsList()) {
        if (mw && mw->inherits("MultiLayer") && !static_cast<MultiLayer *>(mw)->isEmpty()) {
            MultiLayer *ml = static_cast<MultiLayer *>(mw);
            if (!allGraphs.contains(ml))
                allGraphs.append(ml);
        }
    }

    if (allGraphs.size() < 2) {
        QMessageBox::information(
                this, tr("Merge Graph Windows"),
                tr("At least two 2D graph windows are required to merge into a multi-panel figure."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Merge Graph Windows"));
    dlg.setMinimumWidth(480);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);

    vbox->addWidget(new QLabel(
            tr("Check and order the graph windows to merge into a new multi-panel page:"), &dlg));

    QCheckBox *chkCurFolder = new QCheckBox(
            tr("Current folder only (%1)")
                    .arg(current_folder ? current_folder->name() : tr("Project")),
            &dlg);
    const bool hasMultipleFolders = (allGraphs.size() > folderGraphs.size());
    chkCurFolder->setChecked(hasMultipleFolders && folderGraphs.size() >= 2);
    chkCurFolder->setVisible(hasMultipleFolders);
    vbox->addWidget(chkCurFolder);

    QListWidget *graphList = new QListWidget(&dlg);
    graphList->setDragDropMode(QAbstractItemView::InternalMove);
    graphList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto populateMergeGraphList = [&](bool currentOnly) {
        graphList->clear();
        const QList<MultiLayer *> &src = currentOnly ? folderGraphs : allGraphs;
        for (MultiLayer *ml : src) {
            const int origIdx = allGraphs.indexOf(ml);
            QString lbl = ml->windowLabel().isEmpty()
                    ? QString(ml->name())
                    : QString("%1 (%2)").arg(ml->name(), ml->windowLabel());
            if (!currentOnly && ml->folder() && ml->folder() != current_folder)
                lbl = QString("[%1] %2").arg(ml->folder()->name(), lbl);
            QListWidgetItem *it = new QListWidgetItem(lbl, graphList);
            it->setData(Qt::UserRole, origIdx);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
            it->setCheckState((!hasMultipleFolders || ml->folder() == current_folder
                               || folderGraphs.size() < 2)
                                      ? Qt::Checked
                                      : Qt::Unchecked);
        }
    };
    populateMergeGraphList(chkCurFolder->isChecked());
    vbox->addWidget(graphList, 1);

    QHBoxLayout *listBtns = new QHBoxLayout();
    QPushButton *btnUp = new QPushButton(tr("Move Up"), &dlg);
    QPushButton *btnDown = new QPushButton(tr("Move Down"), &dlg);
    QPushButton *btnSelAll = new QPushButton(tr("Select All"), &dlg);
    QPushButton *btnClrAll = new QPushButton(tr("Clear All"), &dlg);
    listBtns->addWidget(btnUp);
    listBtns->addWidget(btnDown);
    listBtns->addStretch(1);
    listBtns->addWidget(btnSelAll);
    listBtns->addWidget(btnClrAll);
    vbox->addLayout(listBtns);

    QGroupBox *layoutGrp = new QGroupBox(tr("Multi-Panel Layout && Publication Style"), &dlg);
    QFormLayout *form = new QFormLayout(layoutGrp);

    auto computeDefaultGrid = [](int count, int &outRows, int &outCols) {
        const int n = qMax(2, count);
        if (n == 2) {
            outRows = 1;
            outCols = 2;
        } else if (n == 3) {
            outRows = 1;
            outCols = 3;
        } else if (n == 4) {
            outRows = 2;
            outCols = 2;
        } else {
            outCols = qMax(2, int(std::ceil(std::sqrt(double(n)))));
            outRows = (n + outCols - 1) / outCols;
        }
    };

    int initRows = 1, initCols = 2;
    computeDefaultGrid(chkCurFolder->isChecked() ? folderGraphs.size() : allGraphs.size(),
                       initRows, initCols);

    QWidget *gridSpinRow = new QWidget(layoutGrp);
    QHBoxLayout *gridSpinLay = new QHBoxLayout(gridSpinRow);
    gridSpinLay->setContentsMargins(0, 0, 0, 0);
    QSpinBox *spinRows = new QSpinBox(gridSpinRow);
    spinRows->setRange(1, 12);
    spinRows->setValue(initRows);
    QSpinBox *spinCols = new QSpinBox(gridSpinRow);
    spinCols->setRange(1, 12);
    spinCols->setValue(initCols);
    gridSpinLay->addWidget(new QLabel(tr("Rows:"), gridSpinRow));
    gridSpinLay->addWidget(spinRows);
    gridSpinLay->addSpacing(8);
    gridSpinLay->addWidget(new QLabel(tr("Columns:"), gridSpinRow));
    gridSpinLay->addWidget(spinCols);
    gridSpinLay->addStretch(1);
    form->addRow(tr("Grid Arrangement:"), gridSpinRow);

    QComboBox *presetCombo = new QComboBox(layoutGrp);
    presetCombo->addItem(tr("Keep Source Graph Styles"));
    presetCombo->addItem(tr("ISIJ 1-Column (85 mm width)"));
    presetCombo->addItem(tr("Journal 2-Column (175 mm width)"));
    presetCombo->addItem(tr("Lab Slide (83.3 mm, 18 pt @ 140%)"));
    presetCombo->setCurrentIndex(0);
    form->addRow(tr("Style Preset:"), presetCombo);

    QCheckBox *chkPanelLabels = new QCheckBox(tr("Add panel labels (a), (b), (c)..."), layoutGrp);
    chkPanelLabels->setChecked(true);
    QComboBox *panelPosCombo = new QComboBox(layoutGrp);
    panelPosCombo->addItem(tr("Outside Top-Left"));
    panelPosCombo->addItem(tr("Inside Top-Left"));
    QWidget *panelRow = new QWidget(layoutGrp);
    QHBoxLayout *panelLay = new QHBoxLayout(panelRow);
    panelLay->setContentsMargins(0, 0, 0, 0);
    panelLay->addWidget(chkPanelLabels);
    panelLay->addWidget(panelPosCombo);
    panelLay->addStretch(1);
    form->addRow(tr("Panel Labels:"), panelRow);

    QCheckBox *chkHideInner =
            new QCheckBox(tr("Hide inner shared axis titles && tick labels"), layoutGrp);
    chkHideInner->setChecked(false);
    form->addRow(QString(), chkHideInner);

    vbox->addWidget(layoutGrp);

    bool userEditedGrid = false;
    QObject::connect(spinRows, QOverload<int>::of(&QSpinBox::valueChanged), &dlg,
                     [&](int) { userEditedGrid = true; });
    QObject::connect(spinCols, QOverload<int>::of(&QSpinBox::valueChanged), &dlg,
                     [&](int) { userEditedGrid = true; });

    auto syncGridIfAuto = [&]() {
        if (userEditedGrid)
            return;
        int checkedCount = 0;
        for (int i = 0; i < graphList->count(); ++i) {
            if (graphList->item(i)->checkState() == Qt::Checked)
                ++checkedCount;
        }
        if (checkedCount >= 2) {
            int r = 1, c = 2;
            computeDefaultGrid(checkedCount, r, c);
            QSignalBlocker b1(spinRows);
            QSignalBlocker b2(spinCols);
            spinRows->setValue(r);
            spinCols->setValue(c);
        }
    };

    QObject::connect(chkCurFolder, &QCheckBox::toggled, &dlg, [&](bool on) {
        populateMergeGraphList(on);
        syncGridIfAuto();
    });
    QObject::connect(graphList, &QListWidget::itemChanged, &dlg,
                     [&](QListWidgetItem *) { syncGridIfAuto(); });
    QObject::connect(btnUp, &QPushButton::clicked, &dlg, [&]() {
        const int row = graphList->currentRow();
        if (row > 0) {
            QListWidgetItem *it = graphList->takeItem(row);
            graphList->insertItem(row - 1, it);
            graphList->setCurrentRow(row - 1);
        }
    });
    QObject::connect(btnDown, &QPushButton::clicked, &dlg, [&]() {
        const int row = graphList->currentRow();
        if (row >= 0 && row < graphList->count() - 1) {
            QListWidgetItem *it = graphList->takeItem(row);
            graphList->insertItem(row + 1, it);
            graphList->setCurrentRow(row + 1);
        }
    });
    QObject::connect(btnSelAll, &QPushButton::clicked, &dlg, [&]() {
        for (int i = 0; i < graphList->count(); ++i)
            graphList->item(i)->setCheckState(Qt::Checked);
    });
    QObject::connect(btnClrAll, &QPushButton::clicked, &dlg, [&]() {
        for (int i = 0; i < graphList->count(); ++i)
            graphList->item(i)->setCheckState(Qt::Unchecked);
    });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QList<MultiLayer *> chosenGraphs;
    for (int i = 0; i < graphList->count(); ++i) {
        QListWidgetItem *it = graphList->item(i);
        if (it && it->checkState() == Qt::Checked) {
            const int idx = it->data(Qt::UserRole).toInt();
            if (idx >= 0 && idx < allGraphs.size() && allGraphs[idx])
                chosenGraphs.append(allGraphs[idx]);
        }
    }
    if (chosenGraphs.size() < 2) {
        QMessageBox::warning(this, tr("Merge Graph Windows"),
                             tr("Please select at least two graph windows to merge."));
        return;
    }

    int cols = qMax(1, spinCols->value());
    int rows = qMax(1, spinRows->value());

    const int savedFileVer = d_file_version;
    d_file_version = SciDAVis::version();

    MultiLayer *merged = multilayerPlot(generateUniqueName(tr("MergedGraph")));
    if (!merged) {
        d_file_version = savedFileVer;
        return;
    }

    merged->blockSignals(true);
    int primaryPanelCount = 0;
    for (MultiLayer *srcMl : chosenGraphs) {
        if (!srcMl || srcMl->isEmpty())
            continue;
        const int baseLayerIdx = merged->layers();
        const QString mlSerialized = srcMl->saveToString("");
        QTextStream ts(const_cast<QString *>(&mlSerialized), QIODevice::ReadOnly);
        QString s;
        while (!ts.atEnd()) {
            s = ts.readLine();
            if (s == "</multiLayer>")
                break;
            if (s.startsWith("<graph>")) {
                QStringList gLines;
                bool isChildLinked = false;
                while (!ts.atEnd()) {
                    s = ts.readLine();
                    const QString trimmedTag = s.trimmed();
                    if (trimmedTag.startsWith(QLatin1String("<ToriLinkedLayer "))
                        || trimmedTag.startsWith(QLatin1String("<ToriLinkedLayer\t"))) {
                        isChildLinked = true;
                        static const QRegularExpression rxParentIdx(
                                QStringLiteral("parentIdx=\"(-?\\d+)\""));
                        const QRegularExpressionMatch m = rxParentIdx.match(trimmedTag);
                        if (m.hasMatch()) {
                            const int oldIdx = m.captured(1).toInt();
                            const int newIdx = (oldIdx >= 0) ? (oldIdx + baseLayerIdx) : oldIdx;
                            QString updated = trimmedTag;
                            updated.replace(m.capturedStart(1), m.capturedLength(1),
                                            QString::number(newIdx));
                            s = updated;
                        }
                    } else if (trimmedTag.startsWith(QLatin1String("<ToriLinkedParent\t"))) {
                        isChildLinked = true;
                        QStringList parts = trimmedTag.split('\t');
                        if (parts.size() >= 2) {
                            parts[1] = QString::number(parts[1].toInt() + baseLayerIdx);
                            s = parts.join('\t');
                        }
                    }
                    gLines << s;
                    if (s == "</graph>")
                        break;
                }
                openGraph(this, merged, gLines);
                if (!isChildLinked)
                    ++primaryPanelCount;
            } else if (s.trimmed().startsWith("<ToriInset\t")) {
                QStringList parts = s.trimmed().split('\t');
                if (parts.size() >= 3) {
                    parts[1] = QString::number(parts[1].toInt() + baseLayerIdx);
                    parts[2] = QString::number(parts[2].toInt() + baseLayerIdx);
                    merged->restorePhase5Tag(parts.join('\t'));
                    if (primaryPanelCount > 0)
                        --primaryPanelCount;
                }
            }
        }
    }
    merged->finalizePhase5Restore();
    merged->blockSignals(false);
    d_file_version = savedFileVer;

    if (rows * cols < primaryPanelCount)
        rows = (primaryPanelCount + cols - 1) / cols;
    merged->setCols(cols);
    merged->setRows(rows);
    merged->setMargins(10, 10, 10, 10);
    merged->setSpacing(12, 14);

    const int presetIdx = presetCombo->currentIndex();
    if (presetIdx == 1) {
        merged->applyStylePresetById(QStringLiteral("isij_85mm"));
        const double hMm = qBound(50.0, 85.0 * double(rows) / double(cols), 260.0);
        merged->setPageSizePhysical(85.0, hMm, MultiLayer::UnitMm, 600);
        merged->setPageViewMode(true);
    } else if (presetIdx == 2) {
        if (!merged->applyStylePresetById(QStringLiteral("journal_2col_175mm")))
            merged->applyStylePresetById(QStringLiteral("isij_85mm"));
        const double hMm = qBound(60.0, 175.0 * double(rows) / double(cols), 260.0);
        merged->setPageSizePhysical(175.0, hMm, MultiLayer::UnitMm, 600);
        merged->setPageViewMode(true);
    } else if (presetIdx == 3) {
        merged->applyStylePresetById(QStringLiteral("lab_slide_83mm"));
        const double wMm = (cols >= 2) ? qMin(254.0, 83.3 * double(cols)) : 83.3;
        const double hMm = qBound(55.0, wMm * double(rows) / double(cols), 185.0);
        merged->setPageSizePhysical(wMm, hMm, MultiLayer::UnitMm, 300);
        merged->setPageViewMode(true);
    } else {
        const int winW = qBound(560, 420 * cols, 1280);
        const int winH = qBound(420, 360 * rows, 960);
        merged->resize(winW, winH);
    }

    if (chkHideInner->isChecked()) {
        QList<Graph *> primaryPanels;
        for (Graph *lg : merged->layersList()) {
            if (lg && !lg->linkedParentLayer())
                primaryPanels.append(lg);
        }
        for (int idx = 0; idx < primaryPanels.size(); ++idx) {
            Graph *lg = primaryPanels[idx];
            const int c = idx % cols;
            if (c > 0 && !lg->hasColorScaleOnAxis(QwtPlot::yLeft)) {
                QStringList tl = lg->enabledTickLabels();
                if (tl.size() == 4) {
                    tl[QwtPlot::yLeft] = QStringLiteral("0");
                    lg->setEnabledTickLabels(tl);
                }
                lg->setAxisTitleAuto(QwtPlot::yLeft, false);
                lg->setAxisTitle(QwtPlot::yLeft, QString());
            }
            if ((idx + cols) < primaryPanels.size()) {
                QStringList tl = lg->enabledTickLabels();
                if (tl.size() == 4) {
                    tl[QwtPlot::xBottom] = QStringLiteral("0");
                    lg->setEnabledTickLabels(tl);
                }
                lg->setAxisTitleAuto(QwtPlot::xBottom, false);
                lg->setAxisTitle(QwtPlot::xBottom, QString());
            }
        }
    }

    merged->setPanelLabels(chkPanelLabels->isChecked(),
                           panelPosCombo->currentIndex() == 1 ? MultiLayer::PanelInsideTopLeft
                                                              : MultiLayer::PanelOutsideTopLeft,
                           QStringLiteral("(%a)"), 0, false); // re-arranged below

    for (Graph *lg : merged->layersList()) {
        if (!lg)
            continue;
        lg->updateStatReferenceLines();
        lg->syncChildLinkedLayers();
        lg->replot();
    }
    // 結合後の最終配置は利用者が求めた配置なので、収まらない格子にはページを広げてよい
    // （広げたときはステータスバーで知らせる）。
    merged->takeLayoutPageGrowth();
    {
        LayoutPageGrowthScope growthScope(merged);
        if (merged->pageViewMode())
            merged->fitLayersToPage();
        else
            merged->arrangeLayers(false, false);
    }
    if (merged->pageViewMode())
        merged->fitPageZoomToViewport();
    const QSizeF mergeGrownFrom = merged->takeLayoutPageGrowth();

    customMenu(merged);
    activateSubWindow(merged);
    emit merged->modifiedPlot();
    emit modified();
    if (mergeGrownFrom.isValid())
        statusBar()->showMessage(merged->layoutPageGrowthMessage(mergeGrownFrom), 10000);
}

MultiLayer *ApplicationWindow::batchCloneGraphToTable(MultiLayer *srcMl, Table *targetTable)
{
    if (!srcMl || !targetTable || srcMl->isEmpty())
        return nullptr;

    QList<Table *> allTables;
    for (MyWidget *mw : windowsList()) {
        if (mw && mw->inherits("Table"))
            allTables.append(static_cast<Table *>(mw));
    }

    auto findOwnerTable = [&](const QString &colRef, QString &outShortName) -> Table * {
        Table *best = nullptr;
        int bestLen = -1;
        for (Table *cand : allTables) {
            const QString pfx = QString(cand->name()) + "_";
            if (colRef.startsWith(pfx) && pfx.length() > bestLen) {
                best = cand;
                bestLen = pfx.length();
            }
        }
        if (best)
            outShortName = colRef.mid(bestLen);
        return best;
    };

    auto mapColumnToTarget = [&](const QString &colRef, Table **outOrigTbl = nullptr) -> QString {
        QString shortName;
        Table *origTbl = findOwnerTable(colRef, shortName);
        if (outOrigTbl)
            *outOrigTbl = origTbl;
        if (!origTbl || targetTable->numCols() <= 0)
            return colRef;

        const int origIdx = origTbl->colIndex(colRef);
        int dstIdx = targetTable->colIndex(QString(targetTable->name()) + "_" + shortName);
        if (dstIdx < 0)
            dstIdx = targetTable->colIndex(shortName);

        if (dstIdx < 0 && origIdx >= 0) {
            if (Column *origCol = origTbl->column(origIdx)) {
                const QString origLn = origCol->longName().trimmed();
                const SciDAVis::PlotDesignation origPd = origTbl->colPlotDesignation(origIdx);
                if (!origLn.isEmpty()) {
                    for (int c = 0; c < targetTable->numCols(); ++c) {
                        Column *candCol = targetTable->column(c);
                        if (candCol
                            && candCol->longName().trimmed().compare(origLn, Qt::CaseInsensitive)
                                    == 0
                            && targetTable->colPlotDesignation(c) == origPd) {
                            dstIdx = c;
                            break;
                        }
                    }
                    if (dstIdx < 0) {
                        for (int c = 0; c < targetTable->numCols(); ++c) {
                            Column *candCol = targetTable->column(c);
                            if (candCol
                                && candCol->longName().trimmed().compare(origLn,
                                                                         Qt::CaseInsensitive)
                                        == 0) {
                                dstIdx = c;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (dstIdx < 0 && origIdx >= 0 && origIdx < targetTable->numCols())
            dstIdx = origIdx;

        if (dstIdx < 0 && origIdx >= 0) {
            if (origTbl->colPlotDesignation(origIdx) == SciDAVis::X)
                dstIdx = targetTable->firstXCol();
            if (dstIdx < 0)
                dstIdx = qMin(origIdx, targetTable->numCols() - 1);
        }

        if (dstIdx >= 0 && dstIdx < targetTable->numCols())
            return targetTable->colName(dstIdx);
        return colRef;
    };

    auto remapGraphSpecLines = [&](const QStringList &rawLines) -> QStringList {
        QStringList out;
        out.reserve(rawLines.size());
        const int targetLastRow = qMax(0, targetTable->numRows() - 1);
        for (const QString &line : rawLines) {
            if (line.startsWith("curve\t")) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                QStringList parts = line.split('\t', Qt::SkipEmptyParts);
#else
                QStringList parts = line.split('\t', QString::SkipEmptyParts);
#endif
                if (parts.size() >= 6) {
                    const int plotType = parts[3].toInt();
                    Table *origTbl = nullptr;
                    if (plotType != Graph::Box)
                        parts[1] = mapColumnToTarget(parts[1], &origTbl);
                    Table *origYTbl = nullptr;
                    parts[2] = mapColumnToTarget(parts[2], &origYTbl);
                    if (!origTbl)
                        origTbl = origYTbl;
                    if (plotType == Graph::VectXYXY || plotType == Graph::VectXYAM) {
                        for (int vi = 20; vi < qMin(parts.size() - 5, 26); ++vi) {
                            QString sn;
                            if (findOwnerTable(parts[vi], sn))
                                parts[vi] = mapColumnToTarget(parts[vi]);
                        }
                    }
                    if (plotType != Graph::Box && parts.size() >= 8) {
                        const int endRowIdx = parts.size() - 2;
                        const int origEndRow = parts[endRowIdx].toInt();
                        if (origEndRow < 0
                            || (origTbl && origEndRow >= origTbl->numRows() - 1)
                            || origEndRow > targetLastRow) {
                            parts[endRowIdx] = QString::number(targetLastRow);
                        }
                    }
                    out.append(parts.join("\t"));
                    continue;
                }
            } else if (line.startsWith("ErrorBars\t")) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                QStringList parts = line.split('\t', Qt::SkipEmptyParts);
#else
                QStringList parts = line.split('\t', QString::SkipEmptyParts);
#endif
                if (parts.size() >= 5) {
                    parts[2] = mapColumnToTarget(parts[2]);
                    parts[3] = mapColumnToTarget(parts[3]);
                    parts[4] = mapColumnToTarget(parts[4]);
                    out.append(parts.join("\t"));
                    continue;
                }
            } else if (line.startsWith("PieCurve\t")) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                QStringList parts = line.split('\t', Qt::SkipEmptyParts);
#else
                QStringList parts = line.split('\t', QString::SkipEmptyParts);
#endif
                if (parts.size() >= 8) {
                    Table *origTbl = nullptr;
                    parts[1] = mapColumnToTarget(parts[1], &origTbl);
                    const int endRowIdx = parts.size() - 2;
                    const int origEndRow = parts[endRowIdx].toInt();
                    if (origEndRow < 0 || (origTbl && origEndRow >= origTbl->numRows() - 1)
                        || origEndRow > targetLastRow) {
                        parts[endRowIdx] = QString::number(targetLastRow);
                    }
                    out.append(parts.join("\t"));
                    continue;
                }
            } else if (line.startsWith("AxisType\t")) {
                QStringList parts = line.split('\t');
                for (int ai = 1; ai < parts.size(); ++ai) {
                    const int semi = parts[ai].indexOf(';');
                    if (semi > 0) {
                        const int fmtCode = parts[ai].left(semi).toInt();
                        const QString payload = parts[ai].mid(semi + 1);
                        if (fmtCode == static_cast<int>(Graph::AxisType::Txt)) {
                            QString sn;
                            if (findOwnerTable(payload, sn))
                                parts[ai] = QString::number(fmtCode) + ";"
                                        + mapColumnToTarget(payload);
                        } else if (fmtCode == static_cast<int>(Graph::AxisType::ColHeader)) {
                            parts[ai] = QString::number(fmtCode) + ";" + targetTable->name();
                        }
                    }
                }
                out.append(parts.join("\t"));
                continue;
            } else if (line.trimmed().startsWith("<ToriErrorBand")
                       || line.trimmed().startsWith("<ToriPointColorMap")) {
                QString remappedLine = line;
                for (const char *attrKey : { "x", "y", "err", "z", "size" }) {
                    const QRegularExpression rxAttr(
                            QStringLiteral("\\b%1=\"([^\"]*)\"").arg(QLatin1String(attrKey)));
                    const QRegularExpressionMatch m = rxAttr.match(remappedLine);
                    if (m.hasMatch() && !m.captured(1).isEmpty()) {
                        const QString mapped = mapColumnToTarget(m.captured(1));
                        if (mapped != m.captured(1)) {
                            remappedLine.replace(m.capturedStart(1), m.capturedLength(1), mapped);
                        }
                    }
                }
                out.append(remappedLine);
                continue;
            } else if (line.trimmed().startsWith("<ToriCurveLabel")) {
                // 独自ラベルは元の表の曲線に付けた名前なので、別の表へ差し替える複製には
                // 持ち越さない（凡例は複製先の表の列名から作り直す）。Y 列が同じ表に
                // 残るとき（複製先が元と同じ表）だけ残す。
                const QRegularExpressionMatch m =
                        QRegularExpression(QStringLiteral("\\by=\"([^\"]*)\"")).match(line);
                const QString yRef = m.hasMatch() ? m.captured(1) : QString();
                if (!yRef.isEmpty() && mapColumnToTarget(yRef) == yRef)
                    out.append(line);
                continue;
            }
            out.append(line);
        }
        return out;
    };

    const int savedFileVer = d_file_version;
    d_file_version = SciDAVis::version();

    const QString mlSerialized = srcMl->saveToString("");
    QTextStream ts(const_cast<QString *>(&mlSerialized), QIODevice::ReadOnly);

    const QString baseCap = generateUniqueName(QString("%1_%2").arg(srcMl->name(), targetTable->name()));
    MultiLayer *cloned = multilayerPlot(baseCap);
    if (!cloned) {
        d_file_version = savedFileVer;
        return nullptr;
    }

    cloned->setCols(srcMl->getCols());
    cloned->setRows(srcMl->getRows());
    cloned->setMargins(srcMl->leftMargin(), srcMl->rightMargin(), srcMl->topMargin(),
                       srcMl->bottomMargin());
    cloned->setSpacing(srcMl->rowsSpacing(), srcMl->colsSpacing());
    const QSize lcs = srcMl->layerCanvasSize();
    cloned->setLayerCanvasSize(lcs.width(), lcs.height());
    cloned->setAlignement(srcMl->horizontalAlignement(), srcMl->verticalAlignement());
    cloned->setWindowLabel(targetTable->windowLabel().isEmpty() ? targetTable->name()
                                                                : targetTable->windowLabel());
    setListViewLabel(cloned->name(), cloned->windowLabel());
    cloned->setCaptionPolicy(srcMl->captionPolicy());
    if (!srcMl->isMaximized())
        cloned->resize(srcMl->size());

    cloned->blockSignals(true);
    QString s;
    while (!ts.atEnd()) {
        s = ts.readLine();
        if (s == "</multiLayer>")
            break;
        if (s.startsWith("<graph>")) {
            QStringList gLines;
            while (!ts.atEnd()) {
                s = ts.readLine();
                gLines << s;
                if (s == "</graph>")
                    break;
            }
            openGraph(this, cloned, remapGraphSpecLines(gLines));
        } else if (s.trimmed().startsWith("<Tori")) {
            const QString trimmedTag = s.trimmed();
            if (!trimmedTag.startsWith(QLatin1String("<ToriRecalcLinks")))
                cloned->restorePhase5Tag(trimmedTag);
        }
    }
    cloned->finalizePhase5Restore();
    cloned->blockSignals(false);
    d_file_version = savedFileVer;

    const bool preserveScales = srcMl->property("_tg_batch_preserve_scales").toBool();
    QList<Graph *> clonedLayers = cloned->layersList();
    for (Graph *lg : clonedLayers) {
        if (!lg)
            continue;
        applyWorksheetMetadataToGraph(lg, targetTable);
        if (!preserveScales)
            lg->setAutoScale();
        lg->updateStatReferenceLines();
        lg->syncChildLinkedLayers();
        lg->replot();
    }
    cloned->finalizePhase5Restore();
    if (cloned->pageViewMode()) {
        cloned->fitLayersToPage();
        cloned->fitPageZoomToViewport();
    }
    customMenu(cloned);
    activateSubWindow(cloned);
    emit modified();
    return cloned;
}

void ApplicationWindow::showBatchPlotDialog()
{
    if (isNonInteractiveContext(this))
        return;

    QList<MultiLayer *> allGraphs;
    QList<Table *> allTables;
    for (MyWidget *mw : windowsList()) {
        if (!mw)
            continue;
        if (mw->inherits("MultiLayer") && !static_cast<MultiLayer *>(mw)->isEmpty())
            allGraphs.append(static_cast<MultiLayer *>(mw));
        else if (mw->inherits("Table"))
            allTables.append(static_cast<Table *>(mw));
    }

    if (allTables.isEmpty()) {
        QMessageBox::information(this, tr("Duplicate (Batch Plotting)"),
                                 tr("No worksheets are available in this project."));
        return;
    }

    QWidget *activeW = d_workspace.activeSubWindow();
    MultiLayer *initialGraph = (activeW && activeW->inherits("MultiLayer"))
            ? static_cast<MultiLayer *>(activeW)
            : (allGraphs.isEmpty() ? nullptr : allGraphs.last());
    Table *initialTable = (activeW && activeW->inherits("Table"))
            ? static_cast<Table *>(activeW)
            : nullptr;
    if (!initialTable && initialGraph) {
        for (Graph *lg : initialGraph->layersList()) {
            for (int i = 0; i < lg->curves(); ++i) {
                if (DataCurve *dc = dynamic_cast<DataCurve *>(lg->curve(i))) {
                    if (dc->table()) {
                        initialTable = dc->table();
                        break;
                    }
                }
            }
            if (initialTable)
                break;
        }
    }
    if (!initialTable)
        initialTable = allTables.first();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Duplicate (Batch Plotting)"));
    dlg.setMinimumWidth(480);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);

    QFormLayout *topForm = new QFormLayout();
    QHBoxLayout *tplSrcRow = new QHBoxLayout();
    tplSrcRow->setContentsMargins(0, 0, 0, 0);
    QComboBox *srcGraphCombo = new QComboBox(&dlg);
    for (int i = 0; i < allGraphs.size(); ++i) {
        srcGraphCombo->addItem(allGraphs[i]->name(), i);
        if (allGraphs[i] == initialGraph)
            srcGraphCombo->setCurrentIndex(i);
    }
    QString selectedQptPath;
    QPushButton *btnBrowseQpt = new QPushButton(tr("Browse .qpt..."), &dlg);
    tplSrcRow->addWidget(srcGraphCombo, 1);
    tplSrcRow->addWidget(btnBrowseQpt);
    topForm->addRow(tr("Template Graph:"), tplSrcRow);
    QLabel *qptPathLbl = new QLabel(&dlg);
    qptPathLbl->setVisible(false);
    qptPathLbl->setWordWrap(true);
    topForm->addRow(QString(), qptPathLbl);
    vbox->addLayout(topForm);

    QCheckBox *chkAutoScaleAxes = new QCheckBox(
            tr("Auto-scale axes to each target dataset"), &dlg);
    chkAutoScaleAxes->setChecked(true);
    vbox->addWidget(chkAutoScaleAxes);

    QTabWidget *modeTabs = new QTabWidget(&dlg);
    QObject::connect(btnBrowseQpt, &QPushButton::clicked, &dlg, [&]() {
        const QString fn = QFileDialog::getOpenFileName(
                &dlg, tr("Choose 2D Graph Template (.qpt)"), templatesDir,
                tr("ToriGraph 2D Graph Template (*.qpt)"));
        if (fn.isEmpty())
            return;
        templatesDir = QFileInfo(fn).absolutePath();
        selectedQptPath = fn;
        const int existingQptIdx = srcGraphCombo->findData(-1);
        const QString disp = tr("[Template] %1").arg(QFileInfo(fn).fileName());
        if (existingQptIdx >= 0) {
            srcGraphCombo->setItemText(existingQptIdx, disp);
            srcGraphCombo->setCurrentIndex(existingQptIdx);
        } else {
            srcGraphCombo->addItem(disp, -1);
            srcGraphCombo->setCurrentIndex(srcGraphCombo->count() - 1);
        }
        qptPathLbl->setText(QDir::toNativeSeparators(fn));
        qptPathLbl->setVisible(true);
    });

    // Tab 0: Clone across selected worksheets
    QWidget *wsPage = new QWidget(&dlg);
    QVBoxLayout *wsLayout = new QVBoxLayout(wsPage);
    wsLayout->addWidget(new QLabel(tr("Select target worksheets to clone the template graph onto:"),
                                   wsPage));
    QListWidget *wsList = new QListWidget(wsPage);
    for (Table *t : allTables) {
        QListWidgetItem *item = new QListWidgetItem(
                t->windowLabel().isEmpty() ? QString(t->name())
                                           : QString("%1 (%2)").arg(t->name(), t->windowLabel()),
                wsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState((t != initialTable || allTables.size() == 1) ? Qt::Checked
                                                                         : Qt::Unchecked);
    }
    wsLayout->addWidget(wsList);
    QHBoxLayout *wsBtnRow = new QHBoxLayout();
    QPushButton *btnSelAllWs = new QPushButton(tr("Select All"), wsPage);
    QPushButton *btnClrAllWs = new QPushButton(tr("Clear All"), wsPage);
    wsBtnRow->addWidget(btnSelAllWs);
    wsBtnRow->addWidget(btnClrAllWs);
    wsBtnRow->addStretch();
    wsLayout->addLayout(wsBtnRow);
    QObject::connect(btnSelAllWs, &QPushButton::clicked, &dlg, [=]() {
        for (int i = 0; i < wsList->count(); ++i)
            wsList->item(i)->setCheckState(Qt::Checked);
    });
    QObject::connect(btnClrAllWs, &QPushButton::clicked, &dlg, [=]() {
        for (int i = 0; i < wsList->count(); ++i)
            wsList->item(i)->setCheckState(Qt::Unchecked);
    });
    modeTabs->addTab(wsPage, tr("Batch Across Worksheets"));

    // Tab 1: Clone across Y columns in a single worksheet
    QWidget *colPage = new QWidget(&dlg);
    QVBoxLayout *colLayout = new QVBoxLayout(colPage);
    QFormLayout *colForm = new QFormLayout();
    QComboBox *targetTblCombo = new QComboBox(colPage);
    for (int i = 0; i < allTables.size(); ++i) {
        targetTblCombo->addItem(allTables[i]->name(), i);
        if (allTables[i] == initialTable)
            targetTblCombo->setCurrentIndex(i);
    }
    colForm->addRow(tr("Source Worksheet:"), targetTblCombo);
    colLayout->addLayout(colForm);
    colLayout->addWidget(new QLabel(tr("Create one styled graph per checked Y column:"), colPage));
    QListWidget *yColList = new QListWidget(colPage);
    colLayout->addWidget(yColList);

    auto repopulateYCols = [=]() {
        yColList->clear();
        const int ti = targetTblCombo->currentIndex();
        if (ti < 0 || ti >= allTables.size())
            return;
        Table *tbl = allTables[ti];
        for (int c = 0; c < tbl->numCols(); ++c) {
            if (tbl->colPlotDesignation(c) == SciDAVis::X)
                continue;
            Column *colObj = tbl->column(c);
            const QString desc = colObj ? colObj->formattedAxisTitle() : tbl->colLabel(c);
            QListWidgetItem *it = new QListWidgetItem(
                    QString("%1 — %2").arg(tbl->colLabel(c), desc), yColList);
            it->setData(Qt::UserRole, c);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(tbl->colPlotDesignation(c) == SciDAVis::Y ? Qt::Checked
                                                                        : Qt::Unchecked);
        }
    };
    QObject::connect(targetTblCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
                     [=](int) { repopulateYCols(); });
    repopulateYCols();
    modeTabs->addTab(colPage, tr("Batch Across Y Columns"));
    vbox->addWidget(modeTabs);

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const int graphDataIdx = srcGraphCombo->currentData().toInt();
    MultiLayer *srcMl = nullptr;
    QPointer<MultiLayer> tempQptMl;
    if (graphDataIdx == -1 && !selectedQptPath.isEmpty()) {
        QString qptErr;
        tempQptMl = openMultiLayerFromTemplateFile(selectedQptPath, &qptErr);
        if (!tempQptMl) {
            QMessageBox::warning(this, tr("Duplicate (Batch Plotting)"),
                                 qptErr.isEmpty() ? tr("Failed to open template file.") : qptErr);
            return;
        }
        srcMl = tempQptMl;
    } else if (graphDataIdx >= 0 && graphDataIdx < allGraphs.size()) {
        srcMl = allGraphs[graphDataIdx];
    }
    if (!srcMl)
        return;

    const bool preserveScales = !chkAutoScaleAxes->isChecked();
    srcMl->setProperty("_tg_batch_preserve_scales", preserveScales);

    if (modeTabs->currentIndex() == 0) {
        for (int i = 0; i < wsList->count() && i < allTables.size(); ++i) {
            if (wsList->item(i)->checkState() == Qt::Checked)
                batchCloneGraphToTable(srcMl, allTables[i]);
        }
    } else {
        const int ti = targetTblCombo->currentIndex();
        if (ti < 0 || ti >= allTables.size()) {
            srcMl->setProperty("_tg_batch_preserve_scales", QVariant());
            if (tempQptMl) {
                tempQptMl->askOnCloseEvent(false);
                closeWindow(tempQptMl);
            }
            return;
        }
        Table *tbl = allTables[ti];
        Graph *srcG = srcMl->activeGraph() ? srcMl->activeGraph()
                                           : (srcMl->layersList().isEmpty()
                                                      ? nullptr
                                                      : srcMl->layersList().first());
        const int style = (srcG && srcG->curves() > 0) ? srcG->curveType(0) : Graph::LineSymbols;
        const CopiedLayerFormatSpec fmtSpec = captureLayerFormat(srcG);

        for (int i = 0; i < yColList->count(); ++i) {
            if (yColList->item(i)->checkState() != Qt::Checked)
                continue;
            const int yColIdx = yColList->item(i)->data(Qt::UserRole).toInt();
            if (yColIdx < 0 || yColIdx >= tbl->numCols())
                continue;
            MultiLayer *newMl =
                    multilayerPlot(tbl, QStringList() << tbl->colName(yColIdx), style, 0,
                                   qMax(0, tbl->numRows() - 1));
            if (!newMl)
                continue;
            newMl->setPageBackgroundColor(srcMl->pageBackgroundColor());
            if (srcMl->pageWidthPhysical() > 0.0 && srcMl->pageHeightPhysical() > 0.0) {
                newMl->setPageSizePhysical(srcMl->pageWidthPhysical(), srcMl->pageHeightPhysical(),
                                           srcMl->pageUnit(), srcMl->pageDpi());
            }
            newMl->setPageViewMode(srcMl->pageViewMode());
            newMl->setTargetAspectRatio(srcMl->targetAspectRatio());
            newMl->setKeepAspectRatio(srcMl->keepAspectRatio());
            if (!srcMl->isMaximized())
                newMl->resize(srcMl->size());
            if (Graph *ng = newMl->activeGraph()) {
                applyLayerFormatNonDestructive(ng, fmtSpec);
                applyWorksheetMetadataToGraph(ng, tbl, QStringList() << tbl->colName(yColIdx),
                                              style);
                if (!preserveScales)
                    ng->setAutoScale();
                ng->updateStatReferenceLines();
                ng->replot();
            }
            newMl->arrangeLayers(false, false);
            if (newMl->pageViewMode()) {
                newMl->fitLayersToPage();
                newMl->fitPageZoomToViewport();
            }
        }
        emit modified();
    }
    srcMl->setProperty("_tg_batch_preserve_scales", QVariant());
    if (tempQptMl) {
        tempQptMl->askOnCloseEvent(false);
        closeWindow(tempQptMl);
    }
}

void ApplicationWindow::applyStyleISIJJournal()
{
    applyStylePresetById(QStringLiteral("isij_85mm"));
}

void ApplicationWindow::applyStylePresentationSlide()
{
    applyStylePresetById(QStringLiteral("lab_slide_83mm"));
}

void ApplicationWindow::applyStylePresetById(const QString &presetId)
{
    MultiLayer *ml = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
    if (!ml) {
        if (!isNonInteractiveContext(this))
            showUxHint(tr("Please select or activate a 2D graph window first to apply a style preset."));
        return;
    }
    if (presetId.trimmed().isEmpty())
        return;
    if (!ml->applyStylePresetById(presetId))
        return;
    if (ml->pageViewMode())
        ml->fitPageZoomToViewport();
    emit ml->modifiedPlot();
    if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
        d_graph_inspector_dock->setTargetMultiLayer(ml, true);
    updateUndoRedoActionsForActiveWindow();
    emit modified();
}

void ApplicationWindow::showStylePresetManagerDialog()
{
    QWidget *w = d_workspace.activeSubWindow();
    MultiLayer *ml = (w && w->inherits("MultiLayer")) ? static_cast<MultiLayer *>(w) : nullptr;
    MultiLayer::showStylePresetManagerDialog(this, ml);
}

void ApplicationWindow::toggleLanguage()
{
    const QString nextLang =
            appLanguage.startsWith(QLatin1String("ja"), Qt::CaseInsensitive) ? QStringLiteral("en")
                                                                             : QStringLiteral("ja");
    switchToLanguage(nextLang);
    if (statusBar()) {
        statusBar()->showMessage(
                nextLang == QLatin1String("ja") ? tr("Switched language to Japanese")
                                                : tr("Switched language to English"),
                2500);
    }
}

void ApplicationWindow::showApplyStylePresetToMultipleGraphsDialog()
{
    if (isNonInteractiveContext(this))
        return;

    QList<MultiLayer *> allGraphs;
    for (MyWidget *w : (current_folder ? current_folder->windowsList() : windowsList())) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(w)) {
            if (!ml->isEmpty())
                allGraphs << ml;
        }
    }
    for (MyWidget *w : windowsList()) {
        if (MultiLayer *ml = qobject_cast<MultiLayer *>(w)) {
            if (!ml->isEmpty() && !allGraphs.contains(ml))
                allGraphs << ml;
        }
    }
    if (allGraphs.isEmpty()) {
        QMessageBox::information(this, tr("Apply Style Preset to Multiple Graphs"),
                                 tr("There are no 2D graphs in this project."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Apply Style Preset to Multiple Graphs"));
    dlg.setMinimumWidth(460);
    QVBoxLayout *vbox = new QVBoxLayout(&dlg);

    QFormLayout *topForm = new QFormLayout();
    QComboBox *presetCombo = new QComboBox(&dlg);
    const QVector<MultiLayer::GraphStylePresetSpec> presets = MultiLayer::listStylePresets();
    QVector<MultiLayer::GraphStylePresetSpec> orderedPresets;
    QVector<MultiLayer::GraphStylePresetSpec> legacyPresets;
    for (const auto &sp : presets) {
        if (sp.id == QLatin1String("slide_28pt"))
            legacyPresets << sp;
        else
            orderedPresets << sp;
    }
    orderedPresets += legacyPresets;
    for (const auto &sp : orderedPresets) {
        QString label = sp.name;
        if (sp.id == QLatin1String("isij_85mm"))
            label = tr("ISIJ Journal (85 mm, 600 DPI)");
        else if (sp.id == QLatin1String("lab_slide_83mm"))
            label = tr("Lab Slide (18 pt @ 140%, 83.3 mm)");
        else if (sp.id == QLatin1String("journal_2col_175mm"))
            label = tr("Journal 2-Column (175 mm)");
        else if (sp.id == QLatin1String("slide_28pt"))
            label = tr("Legacy Slide (28 pt, 185 mm)");
        presetCombo->addItem(label, sp.id);
    }
    topForm->addRow(tr("Style Preset:"), presetCombo);
    vbox->addLayout(topForm);

    vbox->addWidget(new QLabel(tr("Select graph windows to style:"), &dlg));
    QListWidget *graphList = new QListWidget(&dlg);
    for (int i = 0; i < allGraphs.size(); ++i) {
        MultiLayer *ml = allGraphs[i];
        QString lbl = ml->windowLabel().isEmpty()
                ? QString(ml->name())
                : QString("%1 (%2)").arg(ml->name(), ml->windowLabel());
        if (ml->folder() && ml->folder() != current_folder)
            lbl = QString("[%1] %2").arg(ml->folder()->name(), lbl);
        QListWidgetItem *it = new QListWidgetItem(lbl, graphList);
        it->setData(Qt::UserRole, i);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState((!current_folder || ml->folder() == current_folder) ? Qt::Checked
                                                                              : Qt::Unchecked);
    }
    vbox->addWidget(graphList, 1);

    QHBoxLayout *selRow = new QHBoxLayout();
    QPushButton *btnAll = new QPushButton(tr("Select All"), &dlg);
    QPushButton *btnNone = new QPushButton(tr("Clear All"), &dlg);
    selRow->addWidget(btnAll);
    selRow->addWidget(btnNone);
    selRow->addStretch(1);
    vbox->addLayout(selRow);
    QObject::connect(btnAll, &QPushButton::clicked, &dlg, [&]() {
        for (int i = 0; i < graphList->count(); ++i)
            graphList->item(i)->setCheckState(Qt::Checked);
    });
    QObject::connect(btnNone, &QPushButton::clicked, &dlg, [&]() {
        for (int i = 0; i < graphList->count(); ++i)
            graphList->item(i)->setCheckState(Qt::Unchecked);
    });

    QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString presetId = presetCombo->currentData().toString();
    int styledCount = 0;
    for (int i = 0; i < graphList->count(); ++i) {
        QListWidgetItem *it = graphList->item(i);
        if (!it || it->checkState() != Qt::Checked)
            continue;
        const int idx = it->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= allGraphs.size())
            continue;
        MultiLayer *ml = allGraphs[idx];
        if (!ml || !ml->applyStylePresetById(presetId))
            continue;
        if (ml->pageViewMode())
            ml->fitPageZoomToViewport();
        emit ml->modifiedPlot();
        if (d_graph_inspector_dock && d_graph_inspector_dock->targetMultiLayer() == ml)
            d_graph_inspector_dock->setTargetMultiLayer(ml, true);
        ++styledCount;
    }

    if (styledCount > 0) {
        updateUndoRedoActionsForActiveWindow();
        emit modified();
        if (statusBar()) {
            statusBar()->showMessage(
                    tr("Applied preset '%1' to %2 graph(s).")
                            .arg(presetCombo->currentText())
                            .arg(styledCount),
                    3500);
        }
    }
}
