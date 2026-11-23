#include "ole_test_common.h"

#include "Graph3D.h"
#include "MyParser.h"

int runSuite36Round3SurgicalZero(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 36: Round 3 Surgical Zero Gate (tr(var)/QMessageBox JA, 3D Arial, Diamond DFS, Overlay Dtor, AutoAxisTitles)...\n";

    auto hasJapaneseChars = [](const QString &s) {
        for (const QChar &ch : s) {
            if (ch.unicode() >= 0x3000)
                return true;
        }
        return false;
    };

    // 1. Runtime Japanese verification for all Round 3 newly wrapped tr(var), QMessageBox, StatusBar, NonLinearFit & MyParser strings
    mw->switchToLanguage("ja");
    {
        struct CtxStr {
            const char *ctx;
            const char *src;
        };
        static const CtxStr r3Checks[] = {
            // RoiGadgetTool fitModels[] (QT_TR_NOOP + tr(fi.label))
            { "RoiGadgetTool", "Linear" },
            { "RoiGadgetTool", "Polynomial (Order 2)" },
            { "RoiGadgetTool", "Gaussian" },
            { "RoiGadgetTool", "Lorentzian" },
            { "RoiGadgetTool", "Exponential Decay (ExpDec1)" },
            { "RoiGadgetTool", "Boltzmann Sigmoidal" },
            // ApplicationWindow calculatePolygonLoopArea & presetPalettes[]
            { "ApplicationWindow", "%1: |Area| = %2, Arc Length = %3" },
            { "ApplicationWindow", "Target Curve: %1\n\n|Area| = %2\nSigned Area = %3\nArc Length = %4" },
            { "ApplicationWindow", "Error" },
            { "ApplicationWindow", "ISIJ / Origin Default" },
            { "ApplicationWindow", "Origin Classic" },
            { "ApplicationWindow", "Viridis" },
            { "ApplicationWindow", "Plasma" },
            { "ApplicationWindow", "Inferno" },
            { "ApplicationWindow", "CoolWarm" },
            { "ApplicationWindow", "Nature (NPG)" },
            { "ApplicationWindow", "Science (AAAS)" },
            { "ApplicationWindow", "Okabe-Ito (Colorblind Safe)" },
            { "ApplicationWindow", "Monochrome / Grayscale" },
            // Legacy QMessageBox & StatusBar strings (#103 Section 1.3)
            { "QObject", "Input function error" },
            { "QObject", "Origin Project Import Error" },
            { "QObject", "Import start ..." },
            { "QObject", "... file parsed. Starting conversion to SciDAVis ..." },
            { "QObject", "Spreadsheet %1 / %2" },
            { "QObject", "Excel %1 / %2, sheet %3 / %4" },
            { "QObject", "Matrix %1 / %2, sheet %3 / %4" },
            { "QObject", "info" },
            { "QObject", "not yet implemented" },
            { "QObject", "Embedded Graph" },
            { "QObject", "ToriGraph - %1 [PowerPoint OLE]" },
            { "LineProfileTool", "Pixel selection warning" },
            { "LineProfileTool", "Please select an image marker first." },
            { "LineProfileTool", "Please select the end line point inside the image rectangle!" }
        };

        int misses = 0;
        const int nChecks = int(sizeof(r3Checks) / sizeof(r3Checks[0]));
        for (int i = 0; i < nChecks; ++i) {
            const QString src = QString::fromUtf8(r3Checks[i].src);
            const QString ja = QCoreApplication::translate(r3Checks[i].ctx, r3Checks[i].src);
            if (ja.isEmpty() || ja == src || !hasJapaneseChars(ja)) {
                ++misses;
                std::cerr << "[MISS-R3] translate('" << r3Checks[i].ctx << "', '" << r3Checks[i].src
                          << "') -> '" << ja.toUtf8().constData() << "'\n";
            }
        }

        // Check all 58 NonLinearFit builtInModelDescription(name) entries
        const QStringList nlModels = NonLinearFit::builtInModelNames();
        CHECK_COND(!nlModels.isEmpty(), "NonLinearFit::builtInModelNames() empty");
        for (const QString &mName : nlModels) {
            const QString descJa = NonLinearFit::builtInModelDescription(mName);
            if (descJa.isEmpty() || !hasJapaneseChars(descJa)) {
                ++misses;
                std::cerr << "[MISS-NLF] builtInModelDescription('" << mName.toUtf8().constData()
                          << "') -> '" << descJa.toUtf8().constData() << "'\n";
            }
        }

        // Check all MyParser::explainFunction(i) entries
        const QStringList parserFuncs = MyParser::functionsList();
        CHECK_COND(!parserFuncs.isEmpty(), "MyParser::functionsList() empty");
        for (int i = 0; i < parserFuncs.size(); ++i) {
            const QString expJa = MyParser::explainFunction(i);
            if (expJa.isEmpty() || !hasJapaneseChars(expJa)) {
                ++misses;
                std::cerr << "[MISS-PARSER] explainFunction(" << i << ") -> '"
                          << expJa.toUtf8().constData() << "'\n";
            }
        }

        std::cout << "[INFO] Suite 36 JA runtime verified: explicit=" << nChecks
                  << ", nlModels=" << nlModels.size()
                  << ", parserFuncs=" << parserFuncs.size()
                  << ", misses=" << misses << "\n";
        CHECK_COND(misses == 0, "Suite 36 runtime Japanese translation had misses!");
    }

    // 2. 3D Plot & 3D Colorbar "Arial" Default Font Enforcement (#103 Section 2) (tested right inside JA mode too!)
    {
        Qwt3D::ColorLegend standaloneLegend;
        CHECK_COND(standaloneLegend.numberFont().family() == "Arial",
                   "Qwt3D::ColorLegend default numberFont must be Arial");

        Graph3D *rawG3D = new Graph3D("");
        CHECK_COND(rawG3D->titleFont().family() == "Arial",
                   "Raw Graph3D default titleFont must be Arial");
        CHECK_COND(rawG3D->numbersFont().family() == "Arial",
                   "Raw Graph3D default numbersFont must be Arial");
        CHECK_COND(rawG3D->xAxisLabelFont().family() == "Arial"
                           && rawG3D->yAxisLabelFont().family() == "Arial"
                           && rawG3D->zAxisLabelFont().family() == "Arial",
                   "Raw Graph3D default axis label fonts must be Arial");
        CHECK_COND(rawG3D->sp != nullptr
                           && rawG3D->sp->legend()->numberFont().family() == "Arial",
                   "Raw Graph3D sp->legend()->numberFont() must be Arial");
        delete rawG3D;

        Graph3D *appG3D = mw->newPlot3D();
        CHECK_COND(appG3D != nullptr && appG3D->sp != nullptr, "mw->newPlot3D() returned null");
        CHECK_COND(appG3D->titleFont().family() == "Arial"
                           && appG3D->numbersFont().family() == "Arial"
                           && appG3D->xAxisLabelFont().family() == "Arial"
                           && appG3D->sp->legend()->numberFont().family() == "Arial",
                   "ApplicationWindow::newPlot3D() in JA mode violated 3D Arial invariant!");

        appG3D->setNumbersFont(QFont("Meiryo", 11));
        CHECK_COND(appG3D->numbersFont().family() == "Meiryo"
                           && appG3D->sp->legend()->numberFont().family() == "Meiryo"
                           && appG3D->sp->legend()->numberFont().pointSize() == 11,
                   "Graph3D::setNumbersFont failed to propagate font to 3D ColorLegend!");
        appG3D->setNumbersFont(QFont("Arial", 10));
        CHECK_COND(appG3D->sp->legend()->numberFont().family() == "Arial",
                   "Graph3D::setNumbersFont failed to restore Arial on 3D ColorLegend!");
    }
    mw->switchToLanguage("en");

    // 3. FitDialog::expandUserFunctions Diamond Topological Order & Cycle Guard (#102 P2.2 & #103 3.2)
    {
        const QString diamond2 = FitDialog::expandUserFunctions(
                "a*f1 + b*f2",
                QStringList() << "f1" << "f2",
                QStringList() << "2*f2 + 1" << "x^2");
        CHECK_COND(diamond2 == "f2=x^2\nf1=2*f2 + 1\na*f1 + b*f2",
                   "expandUserFunctions failed 2-node diamond topological order!");

        const QString diamond3 = FitDialog::expandUserFunctions(
                "a*f1 + b*f2 + c*f3",
                QStringList() << "f1" << "f2" << "f3",
                QStringList() << "f2 + f3" << "f3 * x" << "sin(x)");
        CHECK_COND(diamond3 == "f3=sin(x)\nf2=f3 * x\nf1=f2 + f3\na*f1 + b*f2 + c*f3",
                   "expandUserFunctions failed 3-node diamond topological order!");

        const QString cyclicRel = FitDialog::expandUserFunctions(
                "a*f1",
                QStringList() << "f1" << "f2",
                QStringList() << "f1 = (x <= 0) ? f2 : 1" << "f2 = f1 + x");
        CHECK_COND(cyclicRel.contains("(x <= 0) ? f2 : 1") && cyclicRel.endsWith("a*f1")
                           && cyclicRel.size() < 120,
                   "expandUserFunctions failed relational RHS + cycle guard!");
        std::cout << "[INFO] Suite 36 expandUserFunctions(diamond3) = '"
                  << QString(diamond3).replace('\n', "; ").toUtf8().constData() << "'\n";
    }

    // 4. ~ImageProfileOverlayItem() Destructor Nulling Guard (#102 P2.1 & #103 3.1)
    {
        Matrix *mProf = mw->newMatrix(8, 8);
        mProf->setName("Suite36ProfMat");
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c)
                mProf->setCell(r, c, double(r * c));
        MultiLayer *mlProf = mw->plotColorMap(mProf);
        Graph *gProf = mlProf->activeGraph();
        ImageProfileGadgetTool *tool = ImageProfileGadgetTool::attach(gProf, mw);
        CHECK_COND(tool != nullptr && gProf->activeTool() == tool,
                   "ImageProfileGadgetTool::attach failed");
        // Simulate Plot::clear() / detachItems deleting the overlay item before tool destruction
        gProf->plotWidget()->detachItems(QwtPlotItem::Rtti_PlotUserItem + 704, true);
        // Now destroying the tool via setActiveTool(nullptr) must not double-delete d_overlay_item
        gProf->setActiveTool(nullptr);
        CHECK_COND(gProf->activeTool() == nullptr, "gProf->setActiveTool(nullptr) failed");
    }

    // 5. Graph::restorePhase5Tag <ToriAutoAxisTitles> refreshes immediately when curves() > 0 (#103 3.3)
    {
        Table *tAuto = mw->newTable(6, 2, "Suite36AutoAxisTbl", "AutoAxis Restore Check");
        tAuto->column(0)->setLongName("Temperature");
        tAuto->column(0)->setUnits("K");
        tAuto->column(1)->setLongName("Conductivity");
        tAuto->column(1)->setUnits("S/m");
        for (int r = 0; r < 6; ++r) {
            tAuto->setCell(r, 0, 300.0 + r * 10.0);
            tAuto->setCell(r, 1, 10.0 + r * 2.0);
        }
        MultiLayer *mlAuto = mw->multilayerPlot(
                tAuto, QStringList() << (QString(tAuto->name()) + "_2"), Graph::LineSymbols);
        Graph *gAuto = mlAuto->activeGraph();
        CHECK_COND(gAuto != nullptr && gAuto->curves() > 0, "gAuto must have curves > 0");
        gAuto->setAxisTitle(QwtPlot::xBottom, "Overridden Manual X");
        gAuto->setAxisTitle(QwtPlot::yLeft, "Overridden Manual Y");
        CHECK_COND(gAuto->plotWidget()->axisTitle(QwtPlot::xBottom).text() == "Overridden Manual X",
                   "Manual X axis title override did not take effect");

        const bool restored = gAuto->restorePhase5Tag(
                "<ToriAutoAxisTitles unitFormat=\"ISIJSlash\" a0=\"1\" a1=\"1\" a2=\"1\" a3=\"1\">");
        CHECK_COND(restored, "restorePhase5Tag(<ToriAutoAxisTitles>) returned false");
        const QString xRestored = gAuto->plotWidget()->axisTitle(QwtPlot::xBottom).text();
        const QString yRestored = gAuto->plotWidget()->axisTitle(QwtPlot::yLeft).text();
        std::cout << "[INFO] Suite 36 restored AutoAxisTitles: X='"
                  << xRestored.toUtf8().constData() << "', Y='"
                  << yRestored.toUtf8().constData() << "'\n";
        CHECK_COND(xRestored == "Temperature / K" && yRestored == "Conductivity / (S/m)"
                           || (xRestored == "Temperature / K" && yRestored.startsWith("Conductivity / ")),
                   "restorePhase5Tag(<ToriAutoAxisTitles>) did not refresh axis titles when curves() > 0!");
    }

    return 0;
}


#include "IconLoader.h"
#include "ModernUiTheme.h"
#include "ModernRibbonAndHud.h"
#include "AutoUpdater.h"
#include "PythonAutomationServer.h"

#include "Grid.h"
#include <QComboBox>
#include <QCryptographicHash>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QCompleter>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QToolBar>
#include <QToolButton>

int runSuite37DuotoneIconsAndTheme(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 37: Duotone Vector SVG Icons & Modern Fluent Theme...\n";

    const QStringList ids = IconLoader::registeredIconIds();
    std::cout << "[INFO] Suite 37 registered Duotone SVG icon IDs = " << ids.size() << "\n";
    CHECK_COND(ids.size() >= 100, "IconLoader::registeredIconIds() must register >= 100 semantic icons");

    const char *sampleIds[] = {
        "file-open", "plot-line-symbol", "peak-analyzer",
        "gadget-roi", "stats-anova", "preset-isij", "python-console"
    };
    const int sizes[] = { 16, 24, 48 };

    for (const char *sid : sampleIds) {
        const QString semanticId = QString::fromLatin1(sid);
        CHECK_COND(ids.contains(semanticId), "Missing required semantic icon ID in registry");

        for (int px : sizes) {
            for (int surf = 0; surf < 2; ++surf) {
                const bool darkSurface = (surf == 1);
                QPixmap pm = IconLoader::duotonePixmap(semanticId, px, 1.0, darkSurface);
                CHECK_COND(!pm.isNull() && pm.width() == px && pm.height() == px,
                           "duotonePixmap returned null or wrong pixel dimensions");

                QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
                CHECK_COND(!img.isNull() && img.hasAlphaChannel(),
                           "duotonePixmap image missing alpha channel");

                int transparentPx = 0;
                int antiAliasedPx = 0;
                int visiblePx = 0;
                int neutralStructuralPx = 0;
                int chromaticAccentPx = 0;
                double structuralLumaSum = 0.0;

                for (int y = 0; y < img.height(); ++y) {
                    for (int x = 0; x < img.width(); ++x) {
                        const QRgb c = img.pixel(x, y);
                        const int a = qAlpha(c);
                        if (a == 0) {
                            ++transparentPx;
                            continue;
                        }
                        if (a < 255)
                            ++antiAliasedPx;
                        if (a >= 25) {
                            ++visiblePx;
                            const int r = qRed(c), g = qGreen(c), b = qBlue(c);
                            const int maxC = std::max({ r, g, b });
                            const int minC = std::min({ r, g, b });
                            const int chroma = maxC - minC;
                            if (chroma <= 38) {
                                ++neutralStructuralPx;
                                structuralLumaSum += 0.2126 * r + 0.7152 * g + 0.0722 * b;
                            } else if (chroma >= 42) {
                                ++chromaticAccentPx;
                            }
                        }
                    }
                }

                CHECK_COND(transparentPx > 0 && antiAliasedPx > 0 && visiblePx > 0,
                           "duotonePixmap failed crisp alpha-channel anti-aliased rendering");
                CHECK_COND(neutralStructuralPx > 0 && chromaticAccentPx > 0,
                           "duotonePixmap failed distinct 2-color (structural + accent) RGB rendering");

                const double meanStructLuma = structuralLumaSum / double(neutralStructuralPx);
                if (!darkSurface) {
                    CHECK_COND(meanStructLuma < 110.0,
                               "Light-surface Duotone structural stroke must be dark slate (#1E293B)");
                } else {
                    CHECK_COND(meanStructLuma > 195.0,
                               "Dark-surface Duotone structural stroke must be bright slate (#F8FAFC)");
                }
            }
        }

        // Also check HiDPI @2x rendering
        QPixmap hiDpiPm = IconLoader::duotonePixmap(semanticId, 24, 2.0, false);
        CHECK_COND(!hiDpiPm.isNull() && hiDpiPm.width() == 48 && hiDpiPm.height() == 48,
                   "duotonePixmap @2x HiDPI physical pixel dimensions mismatch");
    }

    // Verify ModernUiTheme leaves Graph / MultiLayer "Arial" fonts & white export canvas 100% untouched
    ModernUiTheme::applyModernTheme(qApp);
    ModernUiTheme::upgradeAllWindowIcons(mw);
    const QString qss = ModernUiTheme::modernStyleSheet();
    CHECK_COND(!qss.isEmpty() && !qss.contains("QwtPlotCanvas"),
               "ModernUiTheme stylesheet must be non-empty and must not override QwtPlotCanvas");

    CHECK_COND(mw->plotAxesFont.family() == "Arial"
                       && mw->plotNumbersFont.family() == "Arial"
                       && mw->plotLegendFont.family() == "Arial"
                       && mw->plotTitleFont.family() == "Arial"
                       && mw->plot3DTitleFont.family() == "Arial"
                       && mw->plot3DNumbersFont.family() == "Arial"
                       && mw->plot3DAxesFont.family() == "Arial",
               "ModernUiTheme clobbered ApplicationWindow default Arial plot fonts!");

    Table *t37 = mw->newTable(6, 2, "Suite37ThemeArialTbl", "Theme Arial Check");
    for (int r = 0; r < 6; ++r) {
        t37->setCell(r, 0, double(r));
        t37->setCell(r, 1, double(r * r));
    }
    MultiLayer *ml37 = mw->multilayerPlot(
            t37, QStringList() << (QString(t37->name()) + "_2"), Graph::LineSymbols);
    Graph *g37 = ml37 ? ml37->activeGraph() : nullptr;
    CHECK_COND(g37 != nullptr, "Failed to create Suite 37 verification graph");
    if (!g37->legend())
        g37->newLegend();
    CHECK_COND(g37->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                       && g37->axisTitleFont(QwtPlot::yLeft).family() == "Arial"
                       && g37->axisFont(QwtPlot::xBottom).family() == "Arial"
                       && g37->axisFont(QwtPlot::yLeft).family() == "Arial"
                       && g37->legend() != nullptr
                       && g37->legend()->font().family() == "Arial"
                       && g37->plotWidget()->canvasBackground() == QColor(Qt::white),
               "ModernUiTheme altered Graph/MultiLayer Arial fonts or white canvas background!");

    return 0;
}

int runSuite38RibbonSplitPaletteAndFocusHud(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 38: Hybrid Ribbon, Split View, Command Palette & F11 Focus HUD + Table Drawer...\n";

    ModernRibbonBar *ribbon = mw->modernRibbonBar();
    CHECK_COND(ribbon != nullptr, "ApplicationWindow::modernRibbonBar() returned null");
    CHECK_COND(ribbon->tabCount() == 7, "ModernRibbonBar must expose exactly 7 contextual tabs");

    for (int ti = 0; ti < 7; ++ti) {
        ribbon->setCurrentTabIndex(ti);
        CHECK_COND(ribbon->currentTabIndex() == ti && !ribbon->tabTitle(ti).trimmed().isEmpty(),
                   "ModernRibbonBar tab switching or tabTitle(i) failed");
    }
    ribbon->setCurrentTabIndex(0);

    // Real window geometry for Split View, Focus HUD, and Overlay layout verification
    mw->resize(1520, 860);
    mw->show();
    mw->explorerWindow.show();
    mw->logWindow.show();
    QApplication::processEvents();

    // 1. Context switching: Table -> MultiLayer -> Matrix
    Table *t38 = mw->newTable(8, 2, "Suite38SplitTbl", "Split View Table");
    for (int r = 0; r < 8; ++r) {
        t38->setCell(r, 0, double(r));
        t38->setCell(r, 1, double(r * 3 + 1));
    }
    MultiLayer *ml38 = mw->multilayerPlot(
            t38, QStringList() << (QString(t38->name()) + "_2"), Graph::LineSymbols);
    Matrix *m38 = mw->newMatrix(8, 8);
    m38->setName("Suite38CtxMat");

    ribbon->updateActiveContext(t38);
    const QString ctxTable = ribbon->activeContextText();
    ribbon->updateActiveContext(ml38);
    const QString ctxGraph = ribbon->activeContextText();
    ribbon->updateActiveContext(m38);
    const QString ctxMatrix = ribbon->activeContextText();
    std::cout << "[INFO] Suite 38 Context Pills: Table='" << ctxTable.toUtf8().constData()
              << "', Graph='" << ctxGraph.toUtf8().constData()
              << "', Matrix='" << ctxMatrix.toUtf8().constData() << "'\n";
    CHECK_COND(!ctxTable.isEmpty() && !ctxGraph.isEmpty() && !ctxMatrix.isEmpty()
                       && ctxTable != ctxGraph && ctxGraph != ctxMatrix && ctxTable != ctxMatrix,
               "ModernRibbonBar::updateActiveContext failed distinct Table/MultiLayer/Matrix badges");

    // 2. LayoutSplitTableGraph -> LayoutTabbed -> LayoutFreeMdi
    mw->activateSubWindow(t38);
    mw->activateSubWindow(ml38);
    QApplication::processEvents();
    ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
    ribbon->applySplitWorkspaceGeometry();
    QApplication::processEvents();

    CHECK_COND(ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph
                       && mw->d_workspace.viewMode() == QMdiArea::SubWindowView,
               "LayoutSplitTableGraph did not switch QMdiArea to SubWindowView");

    QMdiSubWindow *subTbl = nullptr;
    QMdiSubWindow *subMl = nullptr;
    for (QMdiSubWindow *sw : mw->d_workspace.subWindowList()) {
        if (!sw)
            continue;
        if (sw == static_cast<QMdiSubWindow *>(t38))
            subTbl = sw;
        else if (sw == static_cast<QMdiSubWindow *>(ml38))
            subMl = sw;
    }
    CHECK_COND(subTbl != nullptr && subMl != nullptr,
               "Failed to find QMdiSubWindows for Suite38 Table and MultiLayer");
    const QRect rTbl = subTbl->geometry();
    const QRect rMl = subMl->geometry();
    std::cout << "[INFO] Suite 38 Split Geometry: Table=(" << rTbl.x() << "," << rTbl.y() << " "
              << rTbl.width() << "x" << rTbl.height() << "), Graph=(" << rMl.x() << "," << rMl.y()
              << " " << rMl.width() << "x" << rMl.height() << ")\n";
    CHECK_COND(rTbl.width() > 150 && rMl.width() > rTbl.width()
                       && rTbl.left() < rMl.left()
                       && rTbl.right() <= rMl.left() + 8,
               "LayoutSplitTableGraph failed side-by-side 40% left Table / 60% right MultiLayer geometry");

    ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
    QApplication::processEvents();
    CHECK_COND(ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutTabbed
                       && mw->d_workspace.viewMode() == QMdiArea::TabbedView,
               "LayoutTabbed failed to set QMdiArea::TabbedView");

    ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
    QApplication::processEvents();
    CHECK_COND(ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutFreeMdi
                       && mw->d_workspace.viewMode() == QMdiArea::SubWindowView,
               "LayoutFreeMdi failed to set QMdiArea::SubWindowView");

    // 3. Command Palette (Ctrl+K) live fuzzy search
    CommandPalettePopup *cp = ribbon->commandPalette();
    CHECK_COND(cp != nullptr, "ModernRibbonBar::commandPalette() returned null");
    cp->refreshCommands();
    const int totalCmdCount = cp->visibleCommandCount();
    cp->setFilterText("ISIJ");
    const int filteredIsijCount = cp->visibleCommandCount();
    cp->setFilterText(QString());
    std::cout << "[INFO] Suite 38 CommandPalette: total=" << totalCmdCount
              << ", filter('ISIJ')=" << filteredIsijCount << "\n";
    CHECK_COND(totalCmdCount >= 50 && filteredIsijCount >= 1 && filteredIsijCount < totalCmdCount,
               "CommandPalettePopup failed command discovery or live fuzzy filtering");

    // 4. F11 Focus HUD toggle + FocusTableDrawer live edit + safe Table close + restoration
    mw->explorerWindow.show();
    mw->logWindow.show();
    QApplication::processEvents();
    CHECK_COND(ribbon->isVisible() && !ribbon->isFocusHudMode()
                       && mw->explorerWindow.isVisible() && mw->logWindow.isVisible(),
               "Pre-F11 Normal Mode preconditions not met");

    Table *tDrawer = mw->newTable(5, 2, "Suite38LiveDrawerTbl", "Live Drawer Table");
    for (int r = 0; r < 5; ++r) {
        tDrawer->setCell(r, 0, double(r + 1));
        tDrawer->setCell(r, 1, double((r + 1) * 10));
    }

    mw->toggleFocusHudMode(); // Enter F11 Focus Mode
    QApplication::processEvents();

    FocusCanvasHudOverlay *hud = ribbon->focusHudOverlay();
    FocusTableDrawer *drawer = ribbon->focusTableDrawer();
    CHECK_COND(ribbon->isFocusHudMode()
                       && !ribbon->isVisible()
                       && !mw->explorerWindow.isVisible()
                       && !mw->logWindow.isVisible()
                       && hud != nullptr && hud->isOverlayActive()
                       && hud->worksheetToggleButton() != nullptr
                       && hud->exitFocusButton() != nullptr
                       && drawer != nullptr,
               "Entering F11 Focus HUD Mode failed to collapse Ribbon/docks or activate FocusCanvasHudOverlay");

    hud->setTableDrawerVisible(true);
    QApplication::processEvents();
    CHECK_COND(hud->isTableDrawerVisible() && drawer->isVisible(),
               "FocusCanvasHudOverlay::setTableDrawerVisible(true) failed");

    drawer->setCurrentTable(tDrawer);
    QApplication::processEvents();
    CHECK_COND(drawer->currentTable() == tDrawer
                       && drawer->tableWidget() != nullptr
                       && drawer->tableWidget()->rowCount() == 5,
               "FocusTableDrawer failed to bind Suite38LiveDrawerTbl");

    CHECK_COND(drawer->setCellValueFromDrawer(2, 1, "987.5"),
               "FocusTableDrawer::setCellValueFromDrawer returned false");
    QApplication::processEvents();
    CHECK_COND(std::fabs(tDrawer->cell(2, 1) - 987.5) < 1e-9,
               "Live cell edit in FocusTableDrawer failed to update underlying Table");

    // Close the bound Table WHILE FocusTableDrawer is open — must never crash or leave dangling pointer!
    const bool prevConfirmTable = mw->confirmCloseTable;
    mw->confirmCloseTable = false;
    mw->closeWindow(tDrawer);
    mw->confirmCloseTable = prevConfirmTable;
    QApplication::processEvents();

    CHECK_COND(drawer->currentTable() != tDrawer,
               "Closing Table while FocusTableDrawer was open left a dangling currentTable() pointer!");
    drawer->refreshTableList();
    QApplication::processEvents();

    mw->toggleFocusHudMode(); // Exit F11 Focus Mode
    QApplication::processEvents();
    CHECK_COND(!ribbon->isFocusHudMode()
                       && !hud->isOverlayActive()
                       && ribbon->isVisible()
                       && mw->explorerWindow.isVisible()
                       && mw->logWindow.isVisible(),
               "Exiting F11 Focus HUD Mode failed to restore Ribbon and Explorer/Log docks!");

    // 5. MultiLayer Normal Mode CanvasStatusPill, non-redundant top chrome, & HUD-free PNG export
    CHECK_COND(ml38->layerSwitcherPill() != nullptr && ml38->canvasStatusPill() != nullptr,
               "MultiLayer missing layerSwitcherPill or canvasStatusPill");
    ml38->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
    ml38->updateCanvasStatusPill();
    const QString pillTxt = ml38->canvasStatusPill()->text();
    std::cout << "[INFO] Suite 38 CanvasStatusPill = '" << pillTxt.toUtf8().constData() << "'\n";
    CHECK_COND(pillTxt.contains("Arial") && pillTxt.contains("600"),
               "MultiLayer CanvasStatusPill missing live Arial font or 600 DPI readout");
    CHECK_COND(ml38->findChildren<QToolBar *>().isEmpty(),
               "MultiLayer Normal Mode must not embed duplicate top command QToolBars");

    MultiLayer *mlBlank = mw->newGraph("Suite38HudExcludedPng");
    mlBlank->resize(460, 340);
    mlBlank->show();
    Graph *gBlank = mlBlank->activeGraph();
    for (int ax = 0; ax < QwtPlot::axisCnt; ++ax)
        gBlank->enableAxis(ax, false);
    gBlank->removeLegend();
    gBlank->setFrame(0);
    gBlank->drawCanvasFrame(false, 0);
    gBlank->setBackgroundColor(Qt::white);
    gBlank->setCanvasBackground(Qt::white);
    if (gBlank->grid()) {
        gBlank->grid()->enableX(false);
        gBlank->grid()->enableY(false);
        gBlank->grid()->enableXMin(false);
        gBlank->grid()->enableYMin(false);
    }
    gBlank->setTitle(QString());
    gBlank->replot();
    mlBlank->updateCanvasStatusPill();
    QApplication::processEvents();
    CHECK_COND(mlBlank->canvasStatusPill()->isVisible()
                       && mlBlank->layerSwitcherPill()->isVisible(),
               "CanvasStatusPill and LayerSwitcherPill should be visible on screen in Normal Mode");

    const QImage screenGrab = mlBlank->widget()->grab().toImage();
    int hudScreenBluePx = 0;
    int hudScreenDarkOrBluePx = 0;
    for (int y = 0; y < screenGrab.height(); ++y) {
        for (int x = 0; x < screenGrab.width(); ++x) {
            const QRgb px = screenGrab.pixel(x, y);
            if (qBlue(px) - qRed(px) > 80)
                ++hudScreenBluePx;
            if (qRed(px) < 200 || qGreen(px) < 200 || qBlue(px) < 200)
                ++hudScreenDarkOrBluePx;
        }
    }

    const QString cleanExportPng = QDir::tempPath() + "/torigraph_suite38_hud_clean.png";
    mlBlank->exportImage(cleanExportPng, 100, false, 150);
    QImage exportedImg(cleanExportPng);
    QFile::remove(cleanExportPng);
    CHECK_COND(!exportedImg.isNull() && exportedImg.width() > 100 && exportedImg.height() > 100,
               "MultiLayer::exportImage failed for Suite 38 HUD-exclusion test");

    mlBlank->canvasStatusPill()->hide();
    QApplication::processEvents();
    const QString hiddenHudExportPng = QDir::tempPath() + "/torigraph_suite38_hud_hidden_ref.png";
    mlBlank->exportImage(hiddenHudExportPng, 100, false, 150);
    QImage exportedImgHidden(hiddenHudExportPng);
    QFile::remove(hiddenHudExportPng);
    mlBlank->canvasStatusPill()->show();
    QApplication::processEvents();

    int exportedBluePx = 0;
    for (int y = 0; y < exportedImg.height(); ++y) {
        for (int x = 0; x < exportedImg.width(); ++x) {
            const QRgb px = exportedImg.pixel(x, y);
            if (qBlue(px) - qRed(px) > 80)
                ++exportedBluePx;
        }
    }
    const bool hudIdenticalToHidden = (exportedImg == exportedImgHidden);
    std::cout << "[INFO] Suite 38 HUD-exclusion: screenGrabBluePx=" << hudScreenBluePx
              << ", screenGrabHudPx=" << hudScreenDarkOrBluePx
              << ", exportedBluePx=" << exportedBluePx
              << ", identicalWhenHudHidden=" << (hudIdenticalToHidden ? "true" : "false") << "\n";
    CHECK_COND(hudScreenBluePx >= 20 && hudScreenDarkOrBluePx >= 50
                       && exportedBluePx == 0 && hudIdenticalToHidden,
               "Exported PNG leaked MultiLayer HUD chrome (layerSwitcherPill or canvasStatusPill)!");

    return 0;
}

int runSuite39GitHubAutoUpdaterEngine(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 39: GitHub Auto-Updater Engine (SemVer, Feed Parse, Loopback HTTP, SHA-256 & Staging)...\n";

    // 1. SemVer edge cases
    CHECK_COND(AutoUpdater::compareSemVer("v2.4.0", "2.3.9") > 0,
               "compareSemVer failed v2.4.0 > 2.3.9");
    CHECK_COND(AutoUpdater::compareSemVer("2.10.0", "2.9.9") > 0,
               "compareSemVer failed integer minor comparison 2.10.0 > 2.9.9");
    CHECK_COND(AutoUpdater::compareSemVer("2.4.0", "2.4.0-rc.1") > 0,
               "compareSemVer failed release > pre-release (2.4.0 > 2.4.0-rc.1)");
    CHECK_COND(AutoUpdater::compareSemVer("2.4.0-rc.2", "2.4.0-beta.1") > 0,
               "compareSemVer failed pre-release ordering (rc.2 > beta.1)");
    CHECK_COND(AutoUpdater::compareSemVer("ToriGraph v2.4.0", "2.4.0") == 0,
               "compareSemVer failed prefix stripping equality");

    // 2. parseGitHubReleaseJson (single /releases/latest + /releases array pre-release & draft filtering + digest)
    const QString mockDigest = QString(64, QLatin1Char('d'));
    const QByteArray singleJson = QStringLiteral(R"({
        "tag_name": "v9.4.2",
        "name": "ToriGraph 9.4.2 Stable",
        "draft": false,
        "prerelease": false,
        "published_at": "2026-11-12T08:00:00Z",
        "html_url": "https://github.com/kzykshmz/ToriGraph/releases/tag/v9.4.2",
        "body": "### Highlights\n- Fast C++ engine",
        "assets": [
            {
                "name": "ToriGraph-9.4.2-macos.dmg",
                "size": 12000,
                "browser_download_url": "https://github.com/kzykshmz/ToriGraph/releases/download/v9.4.2/ToriGraph-macos.dmg"
            },
            {
                "name": "ToriGraph-9.4.2-win64.zip",
                "size": 4567890,
                "digest": "sha256:%1",
                "browser_download_url": "https://github.com/kzykshmz/ToriGraph/releases/download/v9.4.2/ToriGraph-9.4.2-win64.zip"
            }
        ]
    })").arg(mockDigest).toUtf8();

    ReleaseInfo parsedSingle;
    QString parseErr;
    CHECK_COND(AutoUpdater::parseGitHubReleaseJson(singleJson, &parsedSingle, &parseErr, false),
               "parseGitHubReleaseJson failed on valid /releases/latest JSON");
    CHECK_COND(parsedSingle.version == "9.4.2"
                       && parsedSingle.assetName == "ToriGraph-9.4.2-win64.zip"
                       && parsedSingle.expectedSha256 == mockDigest
                       && !parsedSingle.isPreRelease,
               "parseGitHubReleaseJson picked wrong Windows asset or digest");

    const QByteArray evilHostJson = QStringLiteral(R"({
        "tag_name": "v9.4.2",
        "draft": false,
        "prerelease": false,
        "assets": [{"name": "ToriGraph-9.4.2-win64.zip", "size": 100, "digest": "sha256:%1", "browser_download_url": "https://example.invalid/ToriGraph-9.4.2-win64.zip"}]
    })").arg(mockDigest).toUtf8();
    ReleaseInfo rejectedEvil;
    CHECK_COND(!AutoUpdater::parseGitHubReleaseJson(evilHostJson, &rejectedEvil, &parseErr, false),
               "parseGitHubReleaseJson accepted untrusted asset download host example.invalid");

    const QString bodySha = QString(64, QLatin1Char('e'));
    const QByteArray arrayJson = QStringLiteral(R"([
        {
            "tag_name": "v9.6.0",
            "name": "Draft Release",
            "draft": true,
            "prerelease": false,
            "assets": [{"name": "ToriGraph-9.6.0-win64.zip", "size": 100, "browser_download_url": "https://github.com/kzykshmz/ToriGraph/releases/download/v9.6.0/draft.zip"}]
        },
        {
            "tag_name": "v9.5.0-rc.1",
            "name": "Release Candidate 1",
            "draft": false,
            "prerelease": true,
            "assets": [{"name": "ToriGraph-9.5.0-rc.1-win64.zip", "size": 200, "browser_download_url": "https://github.com/kzykshmz/ToriGraph/releases/download/v9.5.0-rc.1/rc1.zip"}]
        },
        {
            "tag_name": "v9.4.0",
            "name": "Stable Release",
            "draft": false,
            "prerelease": false,
            "body": "SHA256 (ToriGraph-9.4.0-win64.zip) = %1",
            "assets": [{"name": "ToriGraph-9.4.0-win64.zip", "size": 300, "browser_download_url": "https://github.com/kzykshmz/ToriGraph/releases/download/v9.4.0/stable.zip"}]
        }
    ])").arg(bodySha).toUtf8();

    ReleaseInfo stableOnly, withPre;
    CHECK_COND(AutoUpdater::parseGitHubReleaseJson(arrayJson, &stableOnly, &parseErr, false)
                       && stableOnly.version == "9.4.0"
                       && stableOnly.expectedSha256 == bodySha,
               "parseGitHubReleaseJson failed draft/pre-release filtering or body SHA-256 extraction");
    CHECK_COND(AutoUpdater::parseGitHubReleaseJson(arrayJson, &withPre, &parseErr, true)
                       && withPre.version == "9.5.0-rc.1"
                       && withPre.isPreRelease,
               "parseGitHubReleaseJson(allowPreRelease=true) failed to pick latest non-draft pre-release");

    // 3. Local loopback QTcpServer HTTP mock feed + asset download + SHA-256 pass & tampered rejection
    QByteArray validZipPayload("PK\x03\x04ToriGraph-Mock-Win64-Archive-Payload-v99.1.0-VerifiedBytes");
    validZipPayload.append(QByteArray(4096, 'Z'));
    QByteArray tamperedZipPayload = validZipPayload;
    tamperedZipPayload[128] = char(tamperedZipPayload[128] ^ 0xFF);

    const QString trueShaHex = QString::fromLatin1(
            QCryptographicHash::hash(validZipPayload, QCryptographicHash::Sha256).toHex()).toLower();

    QTcpServer mockHttp;
    CHECK_COND(mockHttp.listen(QHostAddress::LocalHost, 0),
               "Failed to bind Suite 39 local loopback QTcpServer");
    const quint16 httpPort = mockHttp.serverPort();

    QObject::connect(&mockHttp, &QTcpServer::newConnection, &mockHttp, [&]() {
        while (QTcpSocket *sock = mockHttp.nextPendingConnection()) {
            QObject::connect(sock, &QTcpSocket::readyRead, sock, [sock, httpPort, &validZipPayload, &tamperedZipPayload, trueShaHex]() {
                if (!sock->canReadLine())
                    return;
                const QByteArray reqLine = sock->readLine();
                sock->readAll();
                QByteArray body;
                QByteArray contentType = "application/octet-stream";
                if (reqLine.contains("/releases/latest")) {
                    contentType = "application/json; charset=utf-8";
                    body = QStringLiteral(R"({
                        "tag_name": "v99.1.0",
                        "name": "ToriGraph 99.1.0 Loopback Test",
                        "draft": false,
                        "prerelease": false,
                        "published_at": "2026-11-12T09:00:00Z",
                        "html_url": "http://127.0.0.1:%1/release",
                        "body": "Loopback mock release notes",
                        "assets": [{
                            "name": "ToriGraph-99.1.0-win64.zip",
                            "size": %2,
                            "digest": "sha256:%3",
                            "browser_download_url": "http://127.0.0.1:%1/ToriGraph-99.1.0-win64.zip"
                        }]
                    })")
                                   .arg(httpPort)
                                   .arg(validZipPayload.size())
                                   .arg(trueShaHex)
                                   .toUtf8();
                } else if (reqLine.contains("/ToriGraph-99.1.0-win64.zip")) {
                    body = validZipPayload;
                } else if (reqLine.contains("/ToriGraph-tampered.zip")) {
                    body = tamperedZipPayload;
                } else {
                    const QByteArray notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    sock->write(notFound);
                    sock->flush();
                    sock->disconnectFromHost();
                    return;
                }
                QByteArray header = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType
                        + "\r\nContent-Length: " + QByteArray::number(body.size())
                        + "\r\nConnection: close\r\n\r\n";
                sock->write(header);
                sock->write(body);
                sock->flush();
                sock->disconnectFromHost();
            });
            QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
        }
    });

    AutoUpdater *updater = mw->autoUpdater();
    CHECK_COND(updater != nullptr, "ApplicationWindow::autoUpdater() returned null");

    const QUrl feedUrl(QStringLiteral("http://127.0.0.1:%1/releases/latest").arg(httpPort));
    const QJsonObject feedRes = updater->checkFeedBlocking(feedUrl, 5000);
    CHECK_COND(feedRes.value("ok").toBool()
                       && feedRes.value("update_available").toBool()
                       && feedRes.value("latest_version").toString() == "99.1.0"
                       && feedRes.value("sha256").toString() == trueShaHex,
               "AutoUpdater::checkFeedBlocking failed against local loopback HTTP server");

    const QString goodDest = QDir::tempPath() + "/torigraph_suite39_good.zip";
    const QString badDest = QDir::tempPath() + "/torigraph_suite39_tampered.zip";
    QFile::remove(goodDest);
    QFile::remove(goodDest + ".part");
    QFile::remove(badDest);
    QFile::remove(badDest + ".part");

    QString dlErr;
    const QUrl goodAssetUrl(QStringLiteral("http://127.0.0.1:%1/ToriGraph-99.1.0-win64.zip").arg(httpPort));
    CHECK_COND(AutoUpdater::downloadAndVerifyBlocking(goodAssetUrl, goodDest, trueShaHex, &dlErr, 5000),
               "downloadAndVerifyBlocking failed on valid SHA-256 asset");
    CHECK_COND(QFile::exists(goodDest)
                       && !QFile::exists(goodDest + ".part")
                       && AutoUpdater::verifyFileSha256(goodDest, trueShaHex),
               "Valid download missing destination file, left .part, or failed verifyFileSha256");

    dlErr.clear();
    const QUrl tamperedAssetUrl(QStringLiteral("http://127.0.0.1:%1/ToriGraph-tampered.zip").arg(httpPort));
    const bool tamperedAccepted =
            AutoUpdater::downloadAndVerifyBlocking(tamperedAssetUrl, badDest, trueShaHex, &dlErr, 5000);
    std::cout << "[INFO] Suite 39 Tampered SHA-256 rejection error = '"
              << dlErr.toUtf8().constData() << "'\n";
    CHECK_COND(!tamperedAccepted
                       && !dlErr.isEmpty()
                       && !QFile::exists(badDest)
                       && !QFile::exists(badDest + ".part"),
               "Tampered payload was NOT rejected or corrupt .part file was NOT deleted!");

    // 4. Windows PowerShell staging script generation
    QString psErr;
    const QString mockInstallDir = QDir::tempPath() + "/torigraph_suite39_install_dir";
    const QString scriptPath = AutoUpdater::writeWindowsStagingUpdaterScript(
            goodDest, mockInstallDir, "ToriGraph.exe", 424242, true, &psErr);
    CHECK_COND(!scriptPath.isEmpty() && QFile::exists(scriptPath),
               "writeWindowsStagingUpdaterScript failed to generate PowerShell script");
    {
        QFile sf(scriptPath);
        CHECK_COND(sf.open(QIODevice::ReadOnly), "Failed to read generated staging PowerShell script");
        const QString psBody = QString::fromUtf8(sf.readAll());
        sf.close();
        CHECK_COND(psBody.contains("424242")
                           && psBody.contains("Expand-Archive")
                           && psBody.contains("ToriGraph.exe"),
                   "Generated PowerShell staging script missing PID wait, Expand-Archive, or relaunch logic");
    }
    QFile::remove(scriptPath);
    QFile::remove(scriptPath.left(scriptPath.size() - 4) + ".cmd");
    QFile::remove(goodDest);

    // 5. OLE -Embedding silent-check suppression
    updater->setFeedUrl(feedUrl);
    const int countBeforeOle = updater->networkCheckCount();
    mw->setOleEmbeddedMode(true);
    updater->checkForUpdates(true /*silentIfUpToDate*/);
    QApplication::processEvents();
    const int countAfterOle = updater->networkCheckCount();
    const bool inProgDuringOle = updater->isCheckInProgress();
    mw->setOleEmbeddedMode(false);
    updater->setFeedUrl(QUrl());
    mockHttp.close();

    CHECK_COND(!inProgDuringOle && countAfterOle == countBeforeOle,
               "AutoUpdater::checkForUpdates(silent=true) failed to suppress network check in OLE -Embedding mode!");

    return 0;
}

int runSuite40PythonJsonRpcServerDispatch(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 40: In-Process Python JSON-RPC 2.0 Server Dispatch...\n";

    PythonAutomationServer *srv = mw->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "ApplicationWindow::pythonAutomationServer() returned null");
    if (!srv->isTcpListening())
        CHECK_COND(srv->startTcp(0, QHostAddress::LocalHost), "PythonAutomationServer::startTcp(0) failed");
    CHECK_COND(srv->tcpPort() > 0 && srv->endpointString().startsWith("tcp://127.0.0.1:"),
               "PythonAutomationServer invalid TCP port or endpointString");

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, srv->tcpPort());
    {
        QElapsedTimer timer;
        timer.start();
        while (client.state() != QAbstractSocket::ConnectedState && timer.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
    }
    CHECK_COND(client.state() == QAbstractSocket::ConnectedState,
               "Suite 40 QTcpSocket failed to connect to PythonAutomationServer loopback port");

    auto sendRawLine = [&](const QByteArray &rawLine) -> QJsonObject {
        client.write(rawLine + "\n");
        client.flush();
        QElapsedTimer t;
        t.start();
        while (!client.canReadLine() && t.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        if (!client.canReadLine())
            return QJsonObject();
        const QByteArray respBytes = client.readLine().trimmed();
        return QJsonDocument::fromJson(respBytes).object();
    };

    // 0. P0-1: Unauthenticated TCP requests must be rejected with -32001 and closed;
    //          authenticated rpc.auth with srv->tcpAuthToken() must succeed.
    {
        QTcpSocket unauth;
        unauth.connectToHost(QHostAddress::LocalHost, srv->tcpPort());
        QElapsedTimer ut;
        ut.start();
        while (unauth.state() != QAbstractSocket::ConnectedState && ut.elapsed() < 3000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        CHECK_COND(unauth.state() == QAbstractSocket::ConnectedState,
                   "Suite 40 unauth probe socket failed to connect");
        unauth.write("{\"jsonrpc\":\"2.0\",\"id\":99,\"method\":\"app.ping\",\"params\":{}}\n");
        unauth.flush();
        ut.restart();
        while (!unauth.canReadLine() && ut.elapsed() < 3000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        CHECK_COND(unauth.canReadLine(), "Suite 40 unauth probe received no response");
        const QJsonObject unauthResp = QJsonDocument::fromJson(unauth.readLine().trimmed()).object();
        CHECK_COND(unauthResp.value("error").toObject().value("code").toInt() == -32001,
                   "Unauthenticated TCP RPC call did not return -32001 Unauthorized");
    }
    {
        CHECK_COND(!srv->tcpAuthToken().isEmpty() && srv->tcpAuthToken().size() >= 64,
                   "PythonAutomationServer::tcpAuthToken() missing 256-bit random token");
        QJsonObject authReq;
        authReq["jsonrpc"] = "2.0";
        authReq["id"] = 0;
        authReq["method"] = "rpc.auth";
        QJsonObject authParams;
        authParams["token"] = srv->tcpAuthToken();
        authReq["params"] = authParams;
        const QJsonObject authResp =
                sendRawLine(QJsonDocument(authReq).toJson(QJsonDocument::Compact));
        CHECK_COND(authResp.value("result").toObject().value("authenticated").toBool(),
                   "Suite 40 rpc.auth handshake failed with valid tcpAuthToken()");
    }

    // 1. Malformed JSON error handling (-32700 Parse error) & connection survival
    const QJsonObject badJsonResp = sendRawLine("{malformed_json_payload");
    CHECK_COND(badJsonResp.value("error").toObject().value("code").toInt() == -32700,
               "Malformed JSON did not return JSON-RPC 2.0 Parse Error (-32700)");

    // 2. Unknown method error code (-32601 Method not found)
    const QJsonObject unknownResp = sendRawLine(
            R"({"jsonrpc":"2.0","id":4002,"method":"nonexistent.module.func","params":{}})");
    CHECK_COND(unknownResp.value("id").toInt() == 4002
                       && unknownResp.value("error").toObject().value("code").toInt() == -32601,
               "Unknown RPC method did not return JSON-RPC 2.0 Method Not Found (-32601)");

    // 3. Core TCP loopback round-trips (ping, app.info, table -> plot -> fit_linear -> graph.get_info)
    const QJsonObject pingResp = sendRawLine(
            R"({"jsonrpc":"2.0","id":1,"method":"ping","params":{}})");
    CHECK_COND(pingResp.value("result").toObject().value("pong").toBool(),
               "JSON-RPC ping failed over loopback TCP");

    const QJsonObject infoResp = sendRawLine(
            R"({"jsonrpc":"2.0","id":2,"method":"app.info","params":{}})");
    const int methodCount = infoResp.value("result").toObject().value("method_count").toInt();
    std::cout << "[INFO] Suite 40 RPC endpoint=" << srv->endpointString().toUtf8().constData()
              << ", supportedMethods=" << methodCount << "\n";
    CHECK_COND(methodCount >= 100, "app.info reported fewer than 100 RPC methods");

    const QJsonObject tblCreate = sendRawLine(
            R"({"jsonrpc":"2.0","id":3,"method":"table.create","params":{"name":"Suite40RpcTbl","rows":5,"cols":2}})");
    const QString tblName = tblCreate.value("result").toObject().value("name").toString();
    CHECK_COND(!tblName.isEmpty(), "table.create over TCP RPC returned empty table name");

    sendRawLine(QJsonDocument(QJsonObject{
            { "jsonrpc", "2.0" },
            { "id", 4 },
            { "method", "table.set_column_data" },
            { "params", QJsonObject{
                    { "table", tblName },
                    { "col", 0 },
                    { "data", QJsonArray{ 1.0, 2.0, 3.0, 4.0, 5.0 } }
            } }
    }).toJson(QJsonDocument::Compact));

    sendRawLine(QJsonDocument(QJsonObject{
            { "jsonrpc", "2.0" },
            { "id", 5 },
            { "method", "table.set_column_data" },
            { "params", QJsonObject{
                    { "table", tblName },
                    { "col", 1 },
                    { "data", QJsonArray{ 4.5, 7.0, 9.5, 12.0, 14.5 } }
            } }
    }).toJson(QJsonDocument::Compact));

    const QJsonObject plotResp = sendRawLine(QJsonDocument(QJsonObject{
            { "jsonrpc", "2.0" },
            { "id", 6 },
            { "method", "plot.xy" },
            { "params", QJsonObject{
                    { "table", tblName },
                    { "x_col", 0 },
                    { "y_cols", QJsonArray{ 1 } },
                    { "style", "line_symbol" }
            } }
    }).toJson(QJsonDocument::Compact));
    const QString graphName = plotResp.value("result").toObject().value("name").toString();
    CHECK_COND(!graphName.isEmpty(), "plot.xy over TCP RPC failed");

    const QJsonObject fitResp = sendRawLine(QJsonDocument(QJsonObject{
            { "jsonrpc", "2.0" },
            { "id", 7 },
            { "method", "analysis.fit_linear" },
            { "params", QJsonObject{ { "graph", graphName } } }
    }).toJson(QJsonDocument::Compact)).value("result").toObject();
    CHECK_COND(fitResp.value("ok").toBool()
                       && std::fabs(fitResp.value("slope").toDouble() - 2.5) < 1e-9
                       && std::fabs(fitResp.value("intercept").toDouble() - 2.0) < 1e-9
                       && std::fabs(fitResp.value("r2").toDouble() - 1.0) < 1e-9,
               "analysis.fit_linear over TCP RPC failed exact slope=2.5, intercept=2.0 recovery");

    const QJsonObject gInfo = sendRawLine(QJsonDocument(QJsonObject{
            { "jsonrpc", "2.0" },
            { "id", 8 },
            { "method", "graph.get_info" },
            { "params", QJsonObject{ { "graph", graphName } } }
    }).toJson(QJsonDocument::Compact)).value("result").toObject();
    const QJsonObject layer0Axes = gInfo.value("layers").toArray().at(0).toObject().value("axes").toObject();
    CHECK_COND(layer0Axes.value("x").toObject().value("title_font_family").toString() == "Arial",
               "graph.get_info over TCP RPC reported non-Arial X axis font");

    client.disconnectFromHost();
    return 0;
}

int runSuite41Phase6JapaneseZeroMissAndArialGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 41: Phase 6 Zero-Miss Japanese i18n & 100% Arial Font Gate...\n";

    auto hasJaChars = [](const QString &s) {
        for (const QChar &ch : s) {
            if (ch.unicode() >= 0x3000)
                return true;
        }
        return false;
    };
    auto hasAsciiAlpha = [](const QString &s) {
        for (const QChar &ch : s) {
            const ushort u = ch.unicode();
            if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))
                return true;
        }
        return false;
    };

    mw->switchToLanguage("ja");
    QApplication::processEvents();

    ModernRibbonBar *ribbon = mw->modernRibbonBar();
    CHECK_COND(ribbon != nullptr, "ModernRibbonBar null in Suite 41");
    ribbon->retranslateUi();

    int untranslatedAsciiLabels = 0;
    auto checkUiString = [&](const char *surface, const QString &txt) {
        const QString t = txt.trimmed();
        if (t.isEmpty())
            return;
        if (hasAsciiAlpha(t) && !hasJaChars(t)) {
            ++untranslatedAsciiLabels;
            std::cerr << "[MISS-P6-JA] " << surface << ": '" << t.toUtf8().constData() << "'\n";
        }
    };

    // 1. Ribbon 7 tabs, context pill, group captions, and all Ribbon QToolButtons
    for (int i = 0; i < ribbon->tabCount(); ++i)
        checkUiString("RibbonTab", ribbon->tabTitle(i));

    Table *t41 = mw->newTable(6, 2, "Suite41JaTbl", "Suite 41 JA");
    ribbon->updateActiveContext(t41);
    checkUiString("RibbonContext(Table)", ribbon->activeContextText());
    MultiLayer *ml41 = mw->multilayerPlot(
            t41, QStringList() << (QString(t41->name()) + "_2"), Graph::LineSymbols);
    ribbon->updateActiveContext(ml41);
    checkUiString("RibbonContext(Graph)", ribbon->activeContextText());

    for (QLabel *lbl : ribbon->findChildren<QLabel *>()) {
        if (lbl)
            checkUiString("RibbonLabel", lbl->text());
    }
    for (QToolButton *btn : ribbon->findChildren<QToolButton *>()) {
        if (!btn)
            continue;
        checkUiString("RibbonButtonText", btn->text());
        checkUiString("RibbonButtonTip", btn->toolTip());
    }

    // 2. Focus HUD & FocusTableDrawer labels/buttons
    FocusCanvasHudOverlay *hud = ribbon->focusHudOverlay();
    FocusTableDrawer *drawer = ribbon->focusTableDrawer();
    CHECK_COND(hud != nullptr && drawer != nullptr, "Focus HUD / Drawer null in Suite 41");
    hud->retranslateUi();
    drawer->retranslateUi();
    if (hud->worksheetToggleButton()) {
        checkUiString("HudWorksheetBtn", hud->worksheetToggleButton()->text());
        checkUiString("HudWorksheetTip", hud->worksheetToggleButton()->toolTip());
    }
    if (hud->exitFocusButton()) {
        checkUiString("HudExitBtn", hud->exitFocusButton()->text());
        checkUiString("HudExitTip", hud->exitFocusButton()->toolTip());
    }
    for (QToolButton *btn : hud->findChildren<QToolButton *>()) {
        if (!btn)
            continue;
        checkUiString("HudPillBtnText", btn->text());
        checkUiString("HudPillBtnTip", btn->toolTip());
    }
    for (QLabel *lbl : drawer->findChildren<QLabel *>()) {
        if (lbl)
            checkUiString("FocusDrawerLabel", lbl->text());
    }
    for (QToolButton *btn : drawer->findChildren<QToolButton *>()) {
        if (btn)
            checkUiString("FocusDrawerBtnTip", btn->toolTip());
    }

    // 3. Command Palette strings
    CommandPalettePopup *cp = ribbon->commandPalette();
    CHECK_COND(cp != nullptr, "CommandPalettePopup null in Suite 41");
    cp->retranslateUi();
    cp->refreshCommands();
    checkUiString("CommandPaletteTitle", cp->windowTitle());
    if (cp->searchEdit())
        checkUiString("CommandPalettePlaceholder", cp->searchEdit()->placeholderText());
    for (QLabel *lbl : cp->findChildren<QLabel *>()) {
        if (lbl)
            checkUiString("CommandPaletteLabel", lbl->text());
    }

    // 4. AutoUpdater UpdateAvailableDialog & ConfigDialog strings
    ReleaseInfo relJa;
    relJa.tagName = "v9.9.0";
    relJa.version = "9.9.0";
    relJa.title = QString::fromUtf8("ToriGraph 9.9.0 \345\256\211\345\256\232\347\211\210");
    relJa.assetName = "ToriGraph-9.9.0-win64.zip";
    relJa.assetSize = 12345678;
    relJa.expectedSha256 = QString(64, QLatin1Char('a'));
    relJa.publishedAt = "2026-11-12T00:00:00Z";
    relJa.isPreRelease = true;

    {
        UpdateAvailableDialog updDlg(relJa, "2.3.0", mw);
        updDlg.retranslateUi();
        checkUiString("UpdateDialogTitle", updDlg.windowTitle());
        for (QPushButton *btn : updDlg.findChildren<QPushButton *>()) {
            if (btn)
                checkUiString("UpdateDialogBtn", btn->text());
        }
        for (QLabel *lbl : updDlg.findChildren<QLabel *>()) {
            if (lbl)
                checkUiString("UpdateDialogLabel", lbl->text());
        }
    }

    checkUiString("ConfigDialogUpdatesGroup", QCoreApplication::translate("ConfigDialog", "Software Updates"));
    checkUiString("ConfigDialogAutoCheck", QCoreApplication::translate("ConfigDialog", "Automatically check for updates on startup"));
    checkUiString("ConfigDialogPreRelease", QCoreApplication::translate("ConfigDialog", "Include pre-release / beta versions"));
    checkUiString("ConfigDialogCheckNow", QCoreApplication::translate("ConfigDialog", "Check for Updates Now..."));

    // 5. PythonConsoleDialog strings
    {
        PythonConsoleDialog pyDlg(mw, mw->pythonAutomationServer());
        pyDlg.retranslateUi();
        checkUiString("PythonConsoleTitle", pyDlg.windowTitle());
        for (QPushButton *btn : pyDlg.findChildren<QPushButton *>()) {
            if (btn)
                checkUiString("PythonConsoleBtn", btn->text());
        }
        for (QLabel *lbl : pyDlg.findChildren<QLabel *>()) {
            if (lbl)
                checkUiString("PythonConsoleLabel", lbl->text());
        }
        for (QComboBox *cmb : pyDlg.findChildren<QComboBox *>()) {
            if (!cmb)
                continue;
            for (int ci = 0; ci < cmb->count(); ++ci)
                checkUiString("PythonConsoleExampleItem", cmb->itemText(ci));
        }
        for (QPlainTextEdit *ed : pyDlg.findChildren<QPlainTextEdit *>()) {
            if (ed)
                checkUiString("PythonConsolePlaceholder", ed->placeholderText());
        }
    }

    std::cout << "[INFO] Suite 41 Phase 6 Japanese UI audit: untranslatedAsciiLabels="
              << untranslatedAsciiLabels << "\n";
    CHECK_COND(untranslatedAsciiLabels == 0,
               "Suite 41 found 1 or more untranslated pure-ASCII Phase 6 UI labels in Japanese mode!");

    // 6. Switch "ja" -> "en" -> "ja" and verify live retranslation + 100% Arial across 2D, Colorbar, and 3D!
    mw->switchToLanguage("en");
    QApplication::processEvents();
    CHECK_COND(ribbon->tabTitle(0) == "Home",
               "Switching ja -> en did not restore English Ribbon tab title 'Home'");

    mw->switchToLanguage("ja");
    QApplication::processEvents();
    CHECK_COND(hasJaChars(ribbon->tabTitle(0)),
               "Switching en -> ja did not restore Japanese Ribbon tab title");

    // Create fresh 2D curve graph, 2D Spectrogram + Colorbar, and 3D Surface plot while in "ja" mode
    for (int r = 0; r < 6; ++r) {
        t41->setCell(r, 0, double(r));
        t41->setCell(r, 1, double(r * 4 + 2));
    }
    MultiLayer *ml2dJa = mw->multilayerPlot(
            t41, QStringList() << (QString(t41->name()) + "_2"), Graph::LineSymbols);
    Graph *g2dJa = ml2dJa ? ml2dJa->activeGraph() : nullptr;
    CHECK_COND(g2dJa != nullptr, "Failed to create 2D plot in JA mode");
    if (!g2dJa->legend())
        g2dJa->newLegend();

    Matrix *mSpecJa = mw->newMatrix(10, 10);
    mSpecJa->setName("Suite41SpecMat");
    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 10; ++c)
            mSpecJa->setCell(r, c, double(r * c));
    MultiLayer *mlCbJa = mw->plotColorMap(mSpecJa);
    Graph *gCbJa = mlCbJa ? mlCbJa->activeGraph() : nullptr;
    CHECK_COND(gCbJa != nullptr, "Failed to create ColorMap plot in JA mode");
    for (QwtPlotItem *it : gCbJa->plotWidget()->itemList()) {
        if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
            sp->showColorScale(QwtPlot::yRight, true);
            sp->setColorMapPreset(Spectrogram::PresetViridis, false);
            sp->setColorBarTitle("Strain");
        }
    }
    gCbJa->replot();
    QwtScaleWidget *cbAxisJa = gCbJa->plotWidget()->axisWidget(QwtPlot::yRight);

    Graph3D *g3dJa = mw->newPlot3D();
    CHECK_COND(g3dJa != nullptr && g3dJa->sp != nullptr, "Failed to create 3D plot in JA mode");

    CHECK_COND(g2dJa->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                       && g2dJa->axisTitleFont(QwtPlot::yLeft).family() == "Arial"
                       && g2dJa->axisFont(QwtPlot::xBottom).family() == "Arial"
                       && g2dJa->axisFont(QwtPlot::yLeft).family() == "Arial"
                       && g2dJa->legend() != nullptr
                       && g2dJa->legend()->font().family() == "Arial",
               "2D Graph created after ja->en->ja switch violated Arial font invariant!");
    CHECK_COND(cbAxisJa != nullptr
                       && cbAxisJa->font().family() == "Arial"
                       && cbAxisJa->title().font().family() == "Arial",
               "Colorbar created after ja->en->ja switch violated Arial font invariant!");
    CHECK_COND(g3dJa->titleFont().family() == "Arial"
                       && g3dJa->numbersFont().family() == "Arial"
                       && g3dJa->xAxisLabelFont().family() == "Arial"
                       && g3dJa->yAxisLabelFont().family() == "Arial"
                       && g3dJa->zAxisLabelFont().family() == "Arial"
                       && g3dJa->sp->legend()->numberFont().family() == "Arial",
               "3D Graph / ColorLegend created after ja->en->ja switch violated Arial font invariant!");

    mw->switchToLanguage("en");
    mw->hide();
    QApplication::processEvents();
    return 0;
}


#include "CurvesDialog.h"
#include "Folder.h"
#include "Note.h"
#include <QProcess>
#include <QTemporaryDir>

static quint32 computeZipCrc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    const auto *bytes = reinterpret_cast<const unsigned char *>(data.constData());
    for (int i = 0; i < data.size(); ++i) {
        crc ^= quint32(bytes[i]);
        for (int b = 0; b < 8; ++b)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return ~crc;
}

static QByteArray buildStoredZipArchive(const QList<QPair<QString, QByteArray>> &entries)
{
    auto appendU16 = [](QByteArray &buf, quint16 v) {
        buf.append(char(v & 0xFFu));
        buf.append(char((v >> 8) & 0xFFu));
    };
    auto appendU32 = [](QByteArray &buf, quint32 v) {
        buf.append(char(v & 0xFFu));
        buf.append(char((v >> 8) & 0xFFu));
        buf.append(char((v >> 16) & 0xFFu));
        buf.append(char((v >> 24) & 0xFFu));
    };

    QByteArray out;
    QByteArray centralDir;
    const quint16 dosTime = 0x6000u; // 12:00:00
    const quint16 dosDate = 0x5CCCu; // valid modern DOS date

    for (const auto &entry : entries) {
        const QByteArray nameUtf8 = entry.first.toUtf8();
        const QByteArray &payload = entry.second;
        const quint32 crc = computeZipCrc32(payload);
        const quint32 sz = quint32(payload.size());
        const quint32 localOffset = quint32(out.size());
        const quint16 utf8Flag = 0x0800u;

        // Local file header
        appendU32(out, 0x04034B50u);
        appendU16(out, 20u);
        appendU16(out, utf8Flag);
        appendU16(out, 0u); // Stored
        appendU16(out, dosTime);
        appendU16(out, dosDate);
        appendU32(out, crc);
        appendU32(out, sz);
        appendU32(out, sz);
        appendU16(out, quint16(nameUtf8.size()));
        appendU16(out, 0u);
        out.append(nameUtf8);
        out.append(payload);

        // Central directory file header
        appendU32(centralDir, 0x02014B50u);
        appendU16(centralDir, 20u);
        appendU16(centralDir, 20u);
        appendU16(centralDir, utf8Flag);
        appendU16(centralDir, 0u);
        appendU16(centralDir, dosTime);
        appendU16(centralDir, dosDate);
        appendU32(centralDir, crc);
        appendU32(centralDir, sz);
        appendU32(centralDir, sz);
        appendU16(centralDir, quint16(nameUtf8.size()));
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU32(centralDir, 0u);
        appendU32(centralDir, localOffset);
        centralDir.append(nameUtf8);
    }

    const quint32 cdOffset = quint32(out.size());
    const quint32 cdSize = quint32(centralDir.size());
    out.append(centralDir);

    // End of central directory record
    appendU32(out, 0x06054B50u);
    appendU16(out, 0u);
    appendU16(out, 0u);
    appendU16(out, quint16(entries.size()));
    appendU16(out, quint16(entries.size()));
    appendU32(out, cdSize);
    appendU32(out, cdOffset);
    appendU16(out, 0u);
    return out;
}

int runSuite42Round2CriticRegressionGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 42: Round 2 Critic Regression Gate (PageView Export, Split Isolation, 213 Duotone SVGs, OLE Hardening, Drawer [fx], Edge Filters & [x64] PS1)...\n";

    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 42");
    ModernRibbonBar *ribbon = mw->modernRibbonBar();
    CHECK_COND(ribbon != nullptr, "ModernRibbonBar is null in Suite 42");

    mw->resize(1360, 860);
    mw->show();
    ribbon->setFocusHudMode(false);
    ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
    QApplication::processEvents();

    QTemporaryDir tmpDir;
    CHECK_COND(tmpDir.isValid(), "Failed to create QTemporaryDir for Suite 42");

    // =========================================================================
    // 1. #136 P1-1: MultiLayer Page View Mode (#EAEEF3 pasteboard) White Export
    // =========================================================================
    {
        Table *tPv = mw->newTable(8, 2, "Suite42PageViewTbl", "PageView White Export");
        for (int r = 0; r < 8; ++r) {
            tPv->setCell(r, 0, double(r));
            tPv->setCell(r, 1, double(r * r + 1));
        }
        MultiLayer *mlPv = mw->multilayerPlot(
                tPv, QStringList() << (QString(tPv->name()) + "_2"), Graph::LineSymbols);
        CHECK_COND(mlPv != nullptr && mlPv->activeGraph() != nullptr, "Failed to create mlPv");
        mlPv->resize(560, 440);
        mlPv->show();
        mlPv->setPageBackgroundColor(QColor(Qt::white));
        mlPv->setPageViewMode(true);
        QApplication::processEvents();
        CHECK_COND(mlPv->pageViewMode(), "mlPv->pageViewMode() must be true");

        auto isPasteboardTint = [](const QColor &c) -> bool {
            return std::abs(c.red() - 234) <= 2
                    && std::abs(c.green() - 238) <= 2
                    && std::abs(c.blue() - 243) <= 2;
        };

        // 1a. Raster PNG export
        const QString pngPath = tmpDir.filePath("suite42_pageview.png");
        mlPv->exportImage(pngPath, 100, /*transparent=*/false, 150);
        QImage pngImg(pngPath);
        CHECK_COND(!pngImg.isNull() && pngImg.width() > 100 && pngImg.height() > 100,
                   "PageView PNG export failed");
        int pngPasteboardPx = 0;
        for (int y = 0; y < pngImg.height(); ++y) {
            for (int x = 0; x < pngImg.width(); ++x) {
                if (isPasteboardTint(pngImg.pixelColor(x, y)))
                    ++pngPasteboardPx;
            }
        }
        const QColor tlCorner = pngImg.pixelColor(2, 2);
        CHECK_COND(tlCorner.red() >= 252 && tlCorner.green() >= 252 && tlCorner.blue() >= 252,
                   "PageView PNG top-left corner must be pure white (#FFFFFF)");
        CHECK_COND(pngPasteboardPx == 0,
                   "PageView PNG export leaked #EAEEF3 pasteboard tint!");

        // 1b. Vector SVG export
        const QString svgPath = tmpDir.filePath("suite42_pageview.svg");
        mlPv->exportSVG(svgPath);
        QFile svgFile(svgPath);
        CHECK_COND(svgFile.open(QIODevice::ReadOnly), "Failed to open exported PageView SVG");
        const QString svgText = QString::fromUtf8(svgFile.readAll());
        svgFile.close();
        CHECK_COND(!svgText.isEmpty() && svgText.contains("<svg"),
                   "Exported PageView SVG is empty or invalid");
        CHECK_COND(!svgText.contains("#eaeef3", Qt::CaseInsensitive)
                           && !svgText.contains("234,238,243")
                           && !svgText.contains("234, 238, 243"),
                   "PageView SVG export leaked #EAEEF3 pasteboard background!");
        CHECK_COND(svgText.contains("#ffffff", Qt::CaseInsensitive)
                           || svgText.contains("255,255,255"),
                   "PageView SVG export missing white (#FFFFFF) page background fill!");

        // 1c. Vector PDF export
        const QString pdfPath = tmpDir.filePath("suite42_pageview.pdf");
        mlPv->exportPDF(pdfPath);
        QFile pdfFile(pdfPath);
        CHECK_COND(pdfFile.open(QIODevice::ReadOnly), "Failed to open exported PageView PDF");
        const QByteArray pdfBytes = pdfFile.readAll();
        pdfFile.close();
        CHECK_COND(pdfBytes.startsWith("%PDF-") && pdfBytes.size() > 1200,
                   "PageView PDF export invalid or empty");
        CHECK_COND(!pdfBytes.contains("0.917647 0.933333 0.952941"),
                   "PageView PDF export leaked normalized #EAEEF3 pasteboard RGB!");

        // 1d. OLE Vector EMF export + Win32 PlayEnhMetaFile onto magenta sentinel surface
        OleRenderedGraph oleGraph = SciDAVisEmf::renderMultiLayer(mlPv);
        CHECK_COND(!oleGraph.emfBytes.isEmpty(), "SciDAVisEmf::renderMultiLayer returned empty EMF");
        HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(oleGraph.emfBytes);
        CHECK_COND(hemf != nullptr, "createHemfFromBytes failed in Suite 42");

        const int pbW = 400, pbH = 300;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = pbW;
        bmi.bmiHeader.biHeight = -pbH; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void *dibBits = nullptr;
        HDC memDc = CreateCompatibleDC(nullptr);
        HBITMAP hDib = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        CHECK_COND(memDc != nullptr && hDib != nullptr && dibBits != nullptr,
                   "Failed to create Win32 DIB section for EMF playback");
        HGDIOBJ oldObj = SelectObject(memDc, hDib);
        auto *px = static_cast< quint32 * >(dibBits);
        for (int i = 0; i < pbW * pbH; ++i)
            px[i] = 0x00FF00FFu; // bright magenta sentinel

        RECT playRc = { 0, 0, pbW, pbH };
        const BOOL played = PlayEnhMetaFile(memDc, hemf, &playRc);
        GdiFlush();

        int emfPasteboardPx = 0;
        auto pixelAt = [&](int x, int y) -> QColor {
            const quint32 bgra = px[y * pbW + x];
            return QColor(int((bgra >> 16) & 0xFFu),
                          int((bgra >> 8) & 0xFFu),
                          int(bgra & 0xFFu));
        };
        for (int y = 0; y < pbH; ++y) {
            for (int x = 0; x < pbW; ++x) {
                if (isPasteboardTint(pixelAt(x, y)))
                    ++emfPasteboardPx;
            }
        }
        const QColor emfCornerTL = pixelAt(4, 4);
        const QColor emfCornerBR = pixelAt(pbW - 5, pbH - 5);

        SelectObject(memDc, oldObj);
        DeleteObject(hDib);
        DeleteDC(memDc);
        DeleteEnhMetaFile(hemf);

        CHECK_COND(played != FALSE, "PlayEnhMetaFile failed on PageView OLE EMF");
        CHECK_COND(emfPasteboardPx == 0, "PageView OLE EMF leaked #EAEEF3 pasteboard tint!");
        CHECK_COND(emfCornerTL.red() >= 250 && emfCornerTL.green() >= 250 && emfCornerTL.blue() >= 250
                           && emfCornerBR.red() >= 250 && emfCornerBR.green() >= 250 && emfCornerBR.blue() >= 250,
                   "PageView OLE EMF corners were not filled with opaque white (#FFFFFF)!");
        mlPv->setPageViewMode(false);
    }

    // =========================================================================
    // 2. #136 P1-2: LayoutSplitTableGraph Folder/Hidden Isolation, Note/Matrix Z-Order & Sync
    // =========================================================================
    {
        Folder *rootFolder = mw->projectFolder();
        CHECK_COND(rootFolder != nullptr, "projectFolder() is null");
        mw->changeFolder(rootFolder, true);
        QApplication::processEvents();

        Table *hiddenTbl = mw->newTable(4, 2, "Suite42HiddenTbl", "Must Stay Hidden");
        mw->hideWindow(hiddenTbl);
        QApplication::processEvents();
        CHECK_COND(hiddenTbl->status() == MyWidget::Hidden && hiddenTbl->isHidden(),
                   "hiddenTbl was not hidden prior to Split layout test");

        // Create subfolder + cross-folder table, then return to rootFolder
        const int foldersBefore = rootFolder->folders().size();
        mw->addFolder();
        QApplication::processEvents();
        CHECK_COND(rootFolder->folders().size() == foldersBefore + 1, "addFolder() failed");
        Folder *subFolder = rootFolder->folders().last();
        mw->changeFolder(subFolder, true);
        QApplication::processEvents();
        Table *crossFolderTbl = mw->newTable(4, 2, "Suite42CrossFolderTbl", "Other Folder");
        CHECK_COND(crossFolderTbl != nullptr, "Failed to create crossFolderTbl");

        mw->changeFolder(rootFolder, true);
        QApplication::processEvents();
        CHECK_COND(crossFolderTbl->isHidden(), "crossFolderTbl should be hidden when rootFolder is active");

        Table *splitTbl = mw->newTable(6, 2, "Suite42SplitTbl", "Active Split Table");
        for (int r = 0; r < 6; ++r) {
            splitTbl->setCell(r, 0, double(r));
            splitTbl->setCell(r, 1, double(2 * r + 1));
        }
        MultiLayer *splitMl = mw->multilayerPlot(
                splitTbl, QStringList() << (QString(splitTbl->name()) + "_2"), Graph::LineSymbols);
        CHECK_COND(splitMl != nullptr, "Failed to create splitMl");

        ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        QApplication::processEvents();
        CHECK_COND(ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph,
                   "Failed to enter LayoutSplitTableGraph");
        CHECK_COND(hiddenTbl->isHidden() && hiddenTbl->status() == MyWidget::Hidden,
                   "LayoutSplitTableGraph un-hid a hidden window!");
        CHECK_COND(crossFolderTbl->isHidden(),
                   "LayoutSplitTableGraph leaked a window from an inactive Folder!");

        // Create/activate Note & Matrix while in Split mode: must stay raised above split pair
        Note *noteWin = mw->newNote("Suite42SplitNote");
        CHECK_COND(noteWin != nullptr, "newNote failed in Split mode");
        mw->activateSubWindow(noteWin);
        QApplication::processEvents();
        const QList<QMdiSubWindow *> stackAfterNote =
                mw->d_workspace.subWindowList(QMdiArea::StackingOrder);
        CHECK_COND(!stackAfterNote.isEmpty() && stackAfterNote.last() == noteWin,
                   "Active Note window was buried behind Table/Graph in LayoutSplitTableGraph!");

        Matrix *matWin = mw->newMatrix(6, 6);
        matWin->setName("Suite42SplitMat");
        mw->activateSubWindow(matWin);
        QApplication::processEvents();
        const QList<QMdiSubWindow *> stackAfterMat =
                mw->d_workspace.subWindowList(QMdiArea::StackingOrder);
        CHECK_COND(!stackAfterMat.isEmpty() && stackAfterMat.last() == matWin,
                   "Active Matrix window was buried behind Table/Graph in LayoutSplitTableGraph!");

        // Mode sync: setTabbedWorkspaceView(true/false) syncs ribbon buttons without clobbering Split
        mw->setTabbedWorkspaceView(true);
        QApplication::processEvents();
        CHECK_COND(ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutTabbed,
                   "setTabbedWorkspaceView(true) did not sync ModernRibbonBar to LayoutTabbed");

        mw->setTabbedWorkspaceView(false);
        QApplication::processEvents();
        CHECK_COND(ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutFreeMdi,
                   "setTabbedWorkspaceView(false) did not sync ModernRibbonBar to LayoutFreeMdi");

        ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        QApplication::processEvents();
        CHECK_COND(ribbon->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph,
                   "Switching Tabbed->FreeMdi->Split clobbered LayoutSplitTableGraph!");

        ribbon->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
        QApplication::processEvents();
    }

    // =========================================================================
    // 3. #136 P1-3 & P1-4: >= 200 Duotone SVGs, Distinct Layout/Delete/Folder Icons & Menus
    // =========================================================================
    {
        const QStringList allIds = IconLoader::registeredIconIds();
        std::cout << "[INFO] Suite 42 registered Duotone SVG IDs = " << allIds.size() << "\n";
        CHECK_COND(allIds.size() >= 200, "Expected >= 200 registered Duotone SVG icon IDs in Round 2");

        auto render24 = [](const QString &id) -> QImage {
            return IconLoader::duotonePixmap(id, 24, 1.0, false)
                    .toImage()
                    .convertToFormat(QImage::Format_ARGB32);
        };

        const QImage imgDelete = render24("delete");
        const QImage imgNewGraph = render24("new-graph");
        const QImage imgTabs = render24("layout-tabs");
        const QImage imgSplit = render24("layout-split");
        const QImage imgMdi = render24("layout-mdi");
        const QImage imgFolderOpen = render24("folder-open");
        const QImage imgFolderClosed = render24("folder-closed");
        const QImage imgSub = render24("text-subscript");
        const QImage imgSup = render24("text-superscript");

        CHECK_COND(!imgDelete.isNull() && !imgTabs.isNull() && !imgSplit.isNull()
                           && !imgMdi.isNull() && !imgFolderOpen.isNull()
                           && !imgFolderClosed.isNull() && !imgSub.isNull() && !imgSup.isNull(),
                   "One or more Round 2 first-class Duotone SVG icons rendered null");
        CHECK_COND(imgDelete != imgNewGraph, "'delete' icon must not fall back to 'new-graph'");
        CHECK_COND(imgTabs != imgSplit && imgSplit != imgMdi && imgTabs != imgMdi,
                   "Workspace layout icons (tabs/split/mdi) must be visually distinct");
        CHECK_COND(imgMdi != imgNewGraph, "'layout-mdi' icon must be distinct from 'new-graph'");
        CHECK_COND(imgFolderOpen != imgFolderClosed,
                   "'folder-open' and 'folder-closed' icons must be distinct");
        CHECK_COND(imgSub != imgSup, "'text-subscript' and 'text-superscript' icons must be distinct");

        QAction actMdiEn(QStringLiteral("Free &MDI Windows"), nullptr);
        QAction actMdiJa(QString::fromUtf8("\343\203\225\343\203\252\343\203\274MDI\343\202\246\343\202\243\343\203\263\343\203\211\343\202\246(&M)"), nullptr);
        const QString resolvedEn = ModernUiTheme::semanticIconForAction(&actMdiEn);
        const QString resolvedJa = ModernUiTheme::semanticIconForAction(&actMdiJa);
        CHECK_COND((resolvedEn == "mdi-view" || resolvedEn == "layout-mdi")
                           && (resolvedJa == "mdi-view" || resolvedJa == "layout-mdi"),
                   "Free MDI Windows (EN/JA) failed to resolve to mdi-view/layout-mdi!");

        // Verify CurvesDialog & PlotDialog buttons carry crisp multi-DPI Duotone icons
        CurvesDialog cDlg(mw);
        int curvesIconButtons = 0;
        for (QPushButton *pb : cDlg.findChildren<QPushButton *>()) {
            if (pb && !pb->icon().isNull()) {
                QPixmap p32 = pb->icon().pixmap(32, 32);
                CHECK_COND(!p32.isNull() && p32.width() >= 24,
                           "CurvesDialog button icon failed multi-DPI vector rendering");
                ++curvesIconButtons;
            }
        }
        CHECK_COND(curvesIconButtons >= 2, "CurvesDialog expected >= 2 upgraded Duotone icon buttons");
    }

    // =========================================================================
    // 4. #136 P1-5: setOleEmbeddedMode(true) Hardening on Separate ApplicationWindow
    // =========================================================================
    {
        ApplicationWindow *oleMw = new ApplicationWindow();
        PythonAutomationServer *olePy = oleMw->pythonAutomationServer();
        CHECK_COND(olePy != nullptr, "oleMw->pythonAutomationServer() is null");
        CHECK_COND(olePy->startTcp(0, QHostAddress::LocalHost) && olePy->isTcpListening(),
                   "Failed to start pre-OLE TCP listener on oleMw");

        oleMw->setOleEmbeddedMode(true);
        QApplication::processEvents();

        CHECK_COND(oleMw->isOleEmbeddedMode(), "oleMw->isOleEmbeddedMode() must be true");
        CHECK_COND(!oleMw->isNonInteractive() && !ApplicationWindow::isNonInteractiveContext(oleMw),
                   "Interactive OLE embedded window must NOT be treated as non-interactive (double-click dialogs must work)!");
        CHECK_COND(!olePy->isTcpListening() && !olePy->isLocalPipeListening(),
                   "setOleEmbeddedMode(true) failed to stop PythonAutomationServer!");

        ModernRibbonBar *oleRibbon = oleMw->modernRibbonBar();
        if (oleRibbon) {
            CHECK_COND(oleRibbon->commandPalette() != nullptr,
                       "Command Palette must remain available in interactive OLE embedded mode!");
            oleRibbon->setFocusHudMode(true);
            CHECK_COND(oleRibbon->isFocusHudMode(),
                       "Focus HUD must remain available in interactive OLE embedded mode!");
            oleRibbon->setFocusHudMode(false);
        }

        for (QAction *act : oleMw->findChildren<QAction *>()) {
            if (!act)
                continue;
            if (act->objectName() == "actionToggleFocusHudMode"
                || act->objectName() == "actionCommandPalette") {
                CHECK_COND(act->isEnabled(),
                           "F11 Focus HUD / Ctrl+K Command Palette QAction must remain enabled in interactive OLE mode!");
            }
        }

        oleMw->savedProject();
        delete oleMw;
        QApplication::processEvents();
    }

    // =========================================================================
    // 5. #136 P2-1: FocusTableDrawer Formula-Cell ([fx]) Read-Only Protection
    // =========================================================================
    {
        Table *fxTbl = mw->newTable(5, 2, "Suite42FxTbl", "Formula Protection");
        for (int r = 0; r < 5; ++r)
            fxTbl->setCell(r, 0, double(r + 1)); // 1, 2, 3, 4, 5
        fxTbl->setCommand(1, "2*col(\"1\")+10");
        CHECK_COND(fxTbl->recalculate(1, false), "fxTbl->recalculate(1, false) failed");
        CHECK_COND(std::fabs(fxTbl->cell(2, 1) - 16.0) < 1e-9,
                   "Formula column initial evaluation mismatch");

        ribbon->setFocusHudMode(true);
        QApplication::processEvents();
        FocusCanvasHudOverlay *hud = ribbon->focusHudOverlay();
        FocusTableDrawer *drawer = ribbon->focusTableDrawer();
        CHECK_COND(hud != nullptr && drawer != nullptr, "Focus HUD / drawer null in Suite 42");

        hud->setTableDrawerVisible(true);
        drawer->setCurrentTable(fxTbl);
        QApplication::processEvents();

        QTableWidget *grid = drawer->tableWidget();
        CHECK_COND(grid != nullptr && grid->columnCount() == 2 && grid->rowCount() == 5,
                   "FocusTableDrawer grid dimensions mismatch for fxTbl");

        QTableWidgetItem *hdrCol1 = grid->horizontalHeaderItem(1);
        CHECK_COND(hdrCol1 != nullptr && !hdrCol1->toolTip().isEmpty(),
                   "Formula column header missing formula tooltip in FocusTableDrawer");
        CHECK_COND(hdrCol1->text().contains("fx", Qt::CaseInsensitive),
                   "Formula column header missing [fx] badge in FocusTableDrawer");

        QTableWidgetItem *dataItem = grid->item(2, 0);
        QTableWidgetItem *fxItem = grid->item(2, 1);
        CHECK_COND(dataItem != nullptr && fxItem != nullptr, "Grid items null in FocusTableDrawer");
        CHECK_COND((dataItem->flags() & Qt::ItemIsEditable) != 0,
                   "Plain data column should remain editable in FocusTableDrawer");
        CHECK_COND((fxItem->flags() & Qt::ItemIsEditable) == 0,
                   "Formula column cell MUST NOT have Qt::ItemIsEditable in FocusTableDrawer!");

        CHECK_COND(!drawer->setCellValueFromDrawer(2, 1, "999.0"),
                   "setCellValueFromDrawer must reject edits on formula cells!");
        CHECK_COND(std::fabs(fxTbl->cell(2, 1) - 16.0) < 1e-9,
                   "Rejected formula cell edit mutated underlying Table value!");
        CHECK_COND(drawer->setCellValueFromDrawer(2, 0, "10.0")
                           && std::fabs(fxTbl->cell(2, 0) - 10.0) < 1e-9,
                   "Editable data cell edit via FocusTableDrawer failed!");

        hud->setTableDrawerVisible(false);
        ribbon->setFocusHudMode(false);
        QApplication::processEvents();
    }

    // =========================================================================
    // 6. #137 P0-1, P0-2, P0-5: Direct C++ Small-Sample SmoothFilter & 1-Row QwtBarCurve
    // =========================================================================
    {
        const bool prevHeadless = mw->isHeadlessMode();
        mw->setHeadlessMode(true);

        // 2-row table + SmoothFilter::Average with points=7 (used to unsigned-underflow & segfault!)
        Table *t2 = mw->newTable(2, 2, "Suite42TwoRowTbl", "2-Row Smooth Safety");
        t2->setCell(0, 0, 1.0);
        t2->setCell(0, 1, 10.0);
        t2->setCell(1, 0, 2.0);
        t2->setCell(1, 1, 20.0);
        MultiLayer *ml2 = mw->multilayerPlot(
                t2, QStringList() << (QString(t2->name()) + "_2"), Graph::LineSymbols);
        Graph *g2 = ml2 ? ml2->activeGraph() : nullptr;
        CHECK_COND(g2 != nullptr && g2->curves() == 1, "Failed to plot 2-row table");
        const QString c2Title = g2->curve(0)->title().text();

        SmoothFilter sfAvg2(mw, g2, c2Title, SmoothFilter::Average);
        sfAvg2.setSilent(true);
        sfAvg2.setSmoothPoints(7);
        sfAvg2.run(); // Must NOT crash or show QMessageBox!

        // 1-row table + SmoothFilter (SavGol & Average) + 1-row VerticalBars export
        Table *t1 = mw->newTable(1, 2, "Suite42OneRowTbl", "1-Row Safety");
        t1->setCell(0, 0, 5.0);
        t1->setCell(0, 1, 42.0);
        MultiLayer *ml1Line = mw->multilayerPlot(
                t1, QStringList() << (QString(t1->name()) + "_2"), Graph::LineSymbols);
        Graph *g1Line = ml1Line ? ml1Line->activeGraph() : nullptr;
        CHECK_COND(g1Line != nullptr && g1Line->curves() == 1, "Failed to plot 1-row line graph");
        const QString c1Title = g1Line->curve(0)->title().text();

        SmoothFilter sfSg1(mw, g1Line, c1Title, SmoothFilter::SavitzkyGolay);
        sfSg1.setSilent(true);
        sfSg1.setSmoothPoints(5, 5);
        sfSg1.run(); // Must return cleanly without modal QMessageBox!

        mw->setHeadlessMode(prevHeadless);

        MultiLayer *ml1Bar = mw->multilayerPlot(
                t1, QStringList() << (QString(t1->name()) + "_2"), Graph::VerticalBars);
        CHECK_COND(ml1Bar != nullptr && ml1Bar->activeGraph() != nullptr,
                   "Failed to create 1-row VerticalBars graph");
        const QString bar1Png = tmpDir.filePath("suite42_1row_bars.png");
        ml1Bar->exportImage(bar1Png, -1, false, 100);
        QFileInfo barFi(bar1Png);
        QImage barImg(bar1Png);
        // The bar fill follows the default curve palette (ToriGraph Standard since 2026-11,
        // first colour black; Origin Classic and Okabe-Ito blue before). Count pixels of the actual fill colour and require
        // a solid area (>= 5 % of the image) so axis lines and text cannot satisfy the check.
        const QColor barFill = ml1Bar->activeGraph()->curve(0)->brush().color();
        int redBarPx = 0;
        for (int y = 0; y < barImg.height(); ++y) {
            for (int x = 0; x < barImg.width(); ++x) {
                const QColor c = barImg.pixelColor(x, y);
                if (std::abs(c.red() - barFill.red()) < 40 && std::abs(c.green() - barFill.green()) < 40
                    && std::abs(c.blue() - barFill.blue()) < 40) {
                    ++redBarPx;
                }
            }
        }
        CHECK_COND(barFi.exists() && barFi.size() > 200 && barFi.size() < 250000
                           && !barImg.isNull() && barImg.width() > 100 && barImg.width() < 1800
                           && barImg.height() > 100 && barImg.height() < 1800
                           && redBarPx > (barImg.width() * barImg.height()) / 20,
                   "1-row QwtBarCurve PNG export failed, missed bar fill, or produced corrupted/inflated dimensions!");
    }

    // =========================================================================
    // 7. #137 P0-4 & P1-4: Bracketed [x64] Live PowerShell Execution & HTTP 403 JSON Error
    // =========================================================================
    {
        struct Suite42SigSettingsGuard
        {
            QString keyReqSig = QStringLiteral("AutoUpdate/RequireCodeSignature");
            QString keySigner = QStringLiteral("AutoUpdate/ExpectedSignerSubject");
            bool hadReqSig = false;
            QVariant prevReqSig;
            bool hadSigner = false;
            QVariant prevSigner;
            Suite42SigSettingsGuard()
            {
                QSettings &st = ApplicationWindow::getSettings();
                hadReqSig = st.contains(keyReqSig);
                if (hadReqSig)
                    prevReqSig = st.value(keyReqSig);
                hadSigner = st.contains(keySigner);
                if (hadSigner)
                    prevSigner = st.value(keySigner);
                st.remove(keyReqSig);
                st.remove(keySigner);
            }
            ~Suite42SigSettingsGuard()
            {
                QSettings &st = ApplicationWindow::getSettings();
                if (hadReqSig)
                    st.setValue(keyReqSig, prevReqSig);
                else
                    st.remove(keyReqSig);
                if (hadSigner)
                    st.setValue(keySigner, prevSigner);
                else
                    st.remove(keySigner);
            }
        } sigSettingsGuard42;

        const QByteArray newExeBytes("TORIGRAPH_UPDATED_BINARY_PAYLOAD_V2_5_0");
        const QByteArray pluginBytes("TORIGRAPH_PLUGIN_DLL_PAYLOAD");
        const QByteArray zipBytes = buildStoredZipArchive({
                qMakePair(QStringLiteral("ToriGraph.exe"), newExeBytes),
                qMakePair(QStringLiteral("plugins/helper.dll"), pluginBytes),
        });
        const QString zipPath = tmpDir.filePath("ToriGraph-2.5.0-win64 [x64].zip");
        {
            QFile zf(zipPath);
            CHECK_COND(zf.open(QIODevice::WriteOnly), "Failed to write test update zip");
            zf.write(zipBytes);
            zf.close();
        }

        const QString bracketedDir =
                tmpDir.filePath(QString::fromUtf8("ToriGraph [x64] \343\203\206\343\202\271\343\203\210/install"));
        CHECK_COND(QDir().mkpath(bracketedDir), "Failed to create bracketed target install dir");
        const QString targetExePath = QDir(bracketedDir).filePath("ToriGraph.exe");
        {
            QFile oldExe(targetExePath);
            CHECK_COND(oldExe.open(QIODevice::WriteOnly), "Failed to seed old ToriGraph.exe");
            oldExe.write("OLD_BINARY_V2_3_0");
            oldExe.close();
        }

        QString psErr;
        const QString scriptPath = AutoUpdater::writeWindowsStagingUpdaterScript(
                zipPath, bracketedDir, "ToriGraph.exe", 99999999, /*relaunch=*/false, &psErr);
        CHECK_COND(!scriptPath.isEmpty() && QFile::exists(scriptPath),
                   "writeWindowsStagingUpdaterScript failed for bracketed [x64] path");

        {
            QFile psFile(scriptPath);
            CHECK_COND(psFile.open(QIODevice::ReadOnly), "Failed to read staged .ps1");
            const QString psText = QString::fromUtf8(psFile.readAll());
            CHECK_COND(psText.contains("-LiteralPath"),
                       "Staged updater .ps1 must use -LiteralPath to prevent [x64] wildcard globbing!");
        }
        const QString cmdCompanion = QFileInfo(scriptPath).dir().filePath("apply_torigraph_update.cmd");
        if (QFile::exists(cmdCompanion)) {
            QFile cmdFile(cmdCompanion);
            if (cmdFile.open(QIODevice::ReadOnly)) {
                const QString cmdText = QString::fromUtf8(cmdFile.readAll());
                CHECK_COND(cmdText.contains("chcp 65001", Qt::CaseInsensitive),
                           "apply_torigraph_update.cmd must include @chcp 65001 for UTF-8 paths!");
            }
        }

        // Execute staged PowerShell updater script end-to-end against the [x64] + Japanese directory!
        QProcess psProc;
        psProc.start(QStringLiteral("powershell.exe"),
                     QStringList() << QStringLiteral("-NoProfile")
                                   << QStringLiteral("-NonInteractive")
                                   << QStringLiteral("-ExecutionPolicy")
                                   << QStringLiteral("Bypass")
                                   << QStringLiteral("-File")
                                   << QDir::toNativeSeparators(scriptPath));
        const bool psFinished = psProc.waitForFinished(30000);
        const QString psOut = QString::fromUtf8(psProc.readAllStandardOutput());
        const QString psErrOut = QString::fromUtf8(psProc.readAllStandardError());
        if (!psFinished || psProc.exitCode() != 0) {
            std::cerr << "[DIAG] PowerShell updater exit=" << psProc.exitCode()
                      << "\nstdout: " << psOut.toStdString()
                      << "\nstderr: " << psErrOut.toStdString() << "\n";
        }
        CHECK_COND(psFinished && psProc.exitCode() == 0,
                   "PowerShell execution of staged updater failed on bracketed [x64] directory!");

        QFile updatedExe(targetExePath);
        CHECK_COND(updatedExe.open(QIODevice::ReadOnly),
                   "Updated ToriGraph.exe missing in bracketed [x64] directory after staging script!");
        CHECK_COND(updatedExe.readAll() == newExeBytes,
                   "ToriGraph.exe in bracketed [x64] directory was NOT overwritten with new binary!");
        updatedExe.close();

        QFile updatedPlugin(QDir(bracketedDir).filePath("plugins/helper.dll"));
        CHECK_COND(updatedPlugin.open(QIODevice::ReadOnly) && updatedPlugin.readAll() == pluginBytes,
                   "Subdirectory file plugins/helper.dll missing in bracketed [x64] install dir!");
        updatedPlugin.close();

        // HTTP 403 GitHub API rate-limit JSON message extraction
        QTcpServer errSrv;
        CHECK_COND(errSrv.listen(QHostAddress::LocalHost, 0), "Failed to bind HTTP 403 mock server");
        const quint16 errPort = errSrv.serverPort();
        const QByteArray rateLimitBody =
                QByteArrayLiteral("{\"message\":\"API rate limit exceeded for 127.0.0.1.\"}");
        QObject::connect(&errSrv, &QTcpServer::newConnection, &errSrv, [&]() {
            while (errSrv.hasPendingConnections()) {
                QTcpSocket *sock = errSrv.nextPendingConnection();
                QObject::connect(sock, &QTcpSocket::readyRead, sock, [sock, rateLimitBody]() {
                    if (!sock->canReadLine())
                        return;
                    sock->readAll();
                    QByteArray hdr = "HTTP/1.1 403 Forbidden\r\n"
                                     "Content-Type: application/json; charset=utf-8\r\n"
                                     "Content-Length: "
                            + QByteArray::number(rateLimitBody.size())
                            + "\r\nConnection: close\r\n\r\n";
                    sock->write(hdr);
                    sock->write(rateLimitBody);
                    sock->flush();
                    sock->disconnectFromHost();
                });
            }
        });

        AutoUpdater *upd = mw->autoUpdater();
        CHECK_COND(upd != nullptr, "mw->autoUpdater() is null");
        const QJsonObject feed403 = upd->checkFeedBlocking(
                QUrl(QStringLiteral("http://127.0.0.1:%1/releases/latest").arg(errPort)), 5000);
        errSrv.close();
        const QString err403 = feed403.value("error").toString();
        std::cout << "[INFO] Suite 42 HTTP 403 updater error = '" << err403.toUtf8().constData() << "'\n";
        CHECK_COND(!feed403.value("ok").toBool()
                           && err403.contains("403")
                           && err403.contains("API rate limit exceeded"),
                   "AutoUpdater::checkFeedBlocking did not surface HTTP 403 GitHub JSON 'message'!");
    }

    // =========================================================================
    // 8. #137 P1-5: PythonConsoleDialog Invalid TORIGRAPH_PYTHON_EXECUTABLE & Close Cleanup
    // =========================================================================
    {
        const QByteArray prevPyEnv = qgetenv("TORIGRAPH_PYTHON_EXECUTABLE");
        const bool hadPrevPyEnv = !prevPyEnv.isEmpty();

        qputenv("TORIGRAPH_PYTHON_EXECUTABLE", "Z:/nonexistent_dir_suite42/missing_python.exe");
        bool explicitInvalid = false;
        const QString badResolved = PythonAutomationServer::resolvePythonExecutable(&explicitInvalid);
        CHECK_COND(explicitInvalid
                           && badResolved == QStringLiteral("Z:/nonexistent_dir_suite42/missing_python.exe"),
                   "Invalid TORIGRAPH_PYTHON_EXECUTABLE must set explicitInvalid=true without silent PATH fallback!");

        if (hadPrevPyEnv)
            qputenv("TORIGRAPH_PYTHON_EXECUTABLE", prevPyEnv);
        else
            qunsetenv("TORIGRAPH_PYTHON_EXECUTABLE");

        bool validInvalidFlag = false;
        const QString goodPy = PythonAutomationServer::resolvePythonExecutable(&validInvalidFlag);
        if (!goodPy.isEmpty() && !validInvalidFlag) {
            PythonConsoleDialog pyDlg(mw, mw->pythonAutomationServer());
            const QString sleepPy = tmpDir.filePath("suite42_sleep.py");
            {
                QFile sf(sleepPy);
                CHECK_COND(sf.open(QIODevice::WriteOnly), "Failed to create suite42_sleep.py");
                sf.write("import time\nprint('STARTED', flush=True)\ntime.sleep(30)\n");
                sf.close();
            }
            CHECK_COND(pyDlg.loadScriptFile(sleepPy), "pyDlg.loadScriptFile failed");
            pyDlg.show();
            pyDlg.runCurrentScript();

            QProcess *childProc = nullptr;
            QElapsedTimer waitStart;
            waitStart.start();
            while (waitStart.elapsed() < 3000) {
                QApplication::processEvents();
                for (QProcess *p : pyDlg.findChildren<QProcess *>()) {
                    if (p && p->state() != QProcess::NotRunning) {
                        childProc = p;
                        break;
                    }
                }
                if (childProc)
                    break;
            }
            CHECK_COND(childProc != nullptr && childProc->state() != QProcess::NotRunning,
                       "PythonConsoleDialog child QProcess did not start");

            QPointer<QProcess> guardedProc(childProc);
            pyDlg.close();
            QApplication::processEvents();
            CHECK_COND(guardedProc.isNull() || guardedProc->state() == QProcess::NotRunning,
                       "Closing PythonConsoleDialog left a running child Python QProcess alive!");
        }
    }

    // =========================================================================
    // 9. Round 2 Newly Added Japanese Translation Strings Audit
    // =========================================================================
    {
        auto hasJa = [](const QString &s) -> bool {
            for (const QChar &ch : s) {
                if (ch.unicode() >= 0x3000)
                    return true;
            }
            return false;
        };

        mw->switchToLanguage("ja");
        QApplication::processEvents();
        const QString jaFxBadge = QCoreApplication::translate("FocusTableDrawer", "[fx]");
        const QString jaFxTip =
                QCoreApplication::translate("FocusTableDrawer", "Formula (read-only): %1").arg("2*x");
        const QString jaGhErr =
                QCoreApplication::translate("AutoUpdater", "GitHub API error (HTTP %1): %2")
                        .arg(403)
                        .arg("rate limit");
        CHECK_COND(hasJa(jaFxBadge) && hasJa(jaFxTip) && hasJa(jaGhErr),
                   "Round 2 newly added FocusTableDrawer / AutoUpdater strings missing Japanese translation!");

        mw->switchToLanguage("en");
        QApplication::processEvents();
    }

    mw->hide();
    QApplication::processEvents();
    std::cout << "[INFO] Suite 42 Round 2 Critic Regression Gate passed!\n";
    return 0;
}


#include "AxesDialog.h"
#include <QListWidget>
#include <QListWidgetItem>

namespace {

struct Suite43AxesDialogProbe : public AxesDialog
{
    QList<QListWidget *> pageAxisLists() const
    {
        return QList<QListWidget *>()
                << axesList << axesGridList << axesTitlesList << axisBreakAxesList;
    }
};

static bool hasDuotoneAntiAliasedPixels(const QIcon &icon, int pxSize)
{
    if (icon.isNull())
        return false;
    const QPixmap pm = icon.pixmap(pxSize, pxSize);
    if (pm.isNull() || pm.width() != pxSize || pm.height() != pxSize)
        return false;

    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    if (img.isNull() || !img.hasAlphaChannel())
        return false;

    int transparentPx = 0;
    int antiAliasedPx = 0;
    int neutralStructuralPx = 0;
    int chromaticAccentPx = 0;

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = img.pixel(x, y);
            const int a = qAlpha(c);
            if (a == 0) {
                ++transparentPx;
                continue;
            }
            if (a < 255)
                ++antiAliasedPx;
            if (a >= 25) {
                const int r = qRed(c), g = qGreen(c), b = qBlue(c);
                const int chroma = std::max({ r, g, b }) - std::min({ r, g, b });
                if (chroma <= 38)
                    ++neutralStructuralPx;
                else if (chroma >= 42)
                    ++chromaticAccentPx;
            }
        }
    }
    return transparentPx > 0 && antiAliasedPx > 0
            && neutralStructuralPx > 0 && chromaticAccentPx > 0;
}

} // namespace

int runSuite43Round3CriticRegressionGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 43: Round 3 Critic Regression Gate (Split changeFolder, .sciprj <hidden> activeWindow, AxesDialog/Graph Duotone Icons, Non-Interactive Guards & UTF-8 CSV)...\n";

    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 43");
    QTemporaryDir tmpDir;
    CHECK_COND(tmpDir.isValid(), "Failed to create QTemporaryDir for Suite 43");

    // =========================================================================
    // 1. #152 P2-R2-1: changeFolder() in LayoutSplitTableGraph & Empty/Hidden-Active Folders
    // =========================================================================
    {
        ApplicationWindow *w43 = new ApplicationWindow();
        w43->applyUserSettings();
        w43->resize(1360, 860);
        w43->show();
        ModernRibbonBar *r43 = w43->modernRibbonBar();
        CHECK_COND(r43 != nullptr, "ModernRibbonBar is null on w43");
        r43->setFocusHudMode(false);
        r43->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
        QApplication::processEvents();

        Folder *root = w43->projectFolder();
        CHECK_COND(root != nullptr, "w43->projectFolder() is null");

        // FolderA: created and populated in Tabbed mode so its active window is Maximized
        w43->changeFolder(root, true);
        w43->addFolder();
        QApplication::processEvents();
        Folder *folderA = root->folders().last();
        folderA->setName("Suite43FolderA");
        w43->changeFolder(folderA, true);
        QApplication::processEvents();

        Table *tblA = w43->newTable(6, 2, "S43FolderATbl", "Folder A Split Table");
        for (int r = 0; r < 6; ++r) {
            tblA->setCell(r, 0, double(r));
            tblA->setCell(r, 1, double(3 * r + 2));
        }
        MultiLayer *mlA = w43->multilayerPlot(
                tblA, QStringList() << (QString(tblA->name()) + "_2"), Graph::LineSymbols);
        CHECK_COND(mlA != nullptr, "Failed to create mlA in Suite 43");
        mlA->setName("S43FolderAGraph");
        w43->activateSubWindow(mlA);
        mlA->setMaximized();
        folderA->setActiveWindow(mlA);
        QApplication::processEvents();
        CHECK_COND(folderA->activeWindow() == mlA && mlA->status() == MyWidget::Maximized,
                   "Precondition failed: mlA must be Maximized activeWindow in folderA");

        // FolderEmpty: subfolder with 0 windows
        w43->changeFolder(root, true);
        w43->addFolder();
        QApplication::processEvents();
        Folder *folderEmpty = root->folders().last();
        folderEmpty->setName("Suite43FolderEmpty");

        // FolderHidAct: visible table+graph PLUS a hidden window forced as saved active_window
        w43->changeFolder(root, true);
        w43->addFolder();
        QApplication::processEvents();
        Folder *folderHidAct = root->folders().last();
        folderHidAct->setName("Suite43FolderHidAct");
        w43->changeFolder(folderHidAct, true);
        QApplication::processEvents();

        Table *tblVisB = w43->newTable(5, 2, "S43VisBTbl", "Visible B Table");
        for (int r = 0; r < 5; ++r) {
            tblVisB->setCell(r, 0, double(r));
            tblVisB->setCell(r, 1, double(r * r));
        }
        MultiLayer *mlVisB = w43->multilayerPlot(
                tblVisB, QStringList() << (QString(tblVisB->name()) + "_2"), Graph::LineSymbols);
        mlVisB->setName("S43VisBGraph");
        Table *tblHidB = w43->newTable(4, 2, "S43HidBTbl", "Hidden Active B");
        w43->hideWindow(tblHidB);
        folderHidAct->setActiveWindow(tblHidB); // simulate saved active_window pointing to hidden window
        QApplication::processEvents();

        // FolderAllHid: subfolder whose ONLY window is hidden
        w43->changeFolder(root, true);
        w43->addFolder();
        QApplication::processEvents();
        Folder *folderAllHid = root->folders().last();
        folderAllHid->setName("Suite43FolderAllHid");
        w43->changeFolder(folderAllHid, true);
        QApplication::processEvents();
        Table *tblOnlyHid = w43->newTable(4, 2, "S43OnlyHidTbl", "Only Hidden");
        w43->hideWindow(tblOnlyHid);
        folderAllHid->setActiveWindow(tblOnlyHid);
        QApplication::processEvents();

        // Return to root, create root windows, and activate Split View
        w43->changeFolder(root, true);
        QApplication::processEvents();
        Table *rootTbl = w43->newTable(4, 2, "S43RootTbl", "Root Table");
        for (int r = 0; r < 4; ++r) {
            rootTbl->setCell(r, 0, double(r));
            rootTbl->setCell(r, 1, double(r + 10));
        }
        MultiLayer *rootMl = w43->multilayerPlot(
                rootTbl, QStringList() << (QString(rootTbl->name()) + "_2"), Graph::LineSymbols);
        CHECK_COND(rootMl != nullptr, "Failed to create rootMl");

        r43->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        QApplication::processEvents();
        CHECK_COND(r43->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph,
                   "Failed to enter LayoutSplitTableGraph on w43");

        // 1a. Switch to FolderA (whose saved active_window_state was Maximized):
        // must NOT maximize mlA over 100% of workspace — must keep side-by-side Split View!
        w43->changeFolder(folderA, true);
        QApplication::processEvents();
        CHECK_COND(r43->workspaceLayoutMode() == ModernRibbonBar::LayoutSplitTableGraph,
                   "changeFolder(folderA) altered workspaceLayoutMode out of LayoutSplitTableGraph!");
        CHECK_COND(!tblA->isHidden() && !mlA->isHidden(),
                   "changeFolder(folderA) failed to show tblA and mlA in Split View!");
        CHECK_COND(!tblA->isMaximized() && !mlA->isMaximized(),
                   "changeFolder(folderA) overwrote Split View with showMaximized()!");
        CHECK_COND(tblA->geometry().left() < mlA->geometry().left()
                           && tblA->width() > 220 && tblA->width() < (w43->width() * 75) / 100
                           && mlA->width() > 220 && mlA->width() < (w43->width() * 75) / 100,
                   "changeFolder(folderA) broke side-by-side Table-left / Graph-right Split geometry!");
        const auto ctxTr = [](const char *s) {
            return QCoreApplication::translate("ModernRibbonBar", s);
        };

        CHECK_COND(r43->activeContextText() == ctxTr("2D Graph")
                           || r43->activeContextText() == ctxTr("Worksheet"),
                   "Ribbon context pill did not update to folderA's active window type!");

        // 1b. Switch from FolderA -> FolderEmpty: context pill must clear FolderA's stale badge to "Project"!
        w43->changeFolder(folderEmpty, true);
        QApplication::processEvents();
        CHECK_COND(folderEmpty->activeWindow() == nullptr,
                   "Empty folder must have activeWindow() == nullptr");
        CHECK_COND(r43->activeContextText() == ctxTr("Project"),
                   "changeFolder(folderEmpty) left Ribbon context pill stale instead of 'Project'!");

        // 1c. Switch -> FolderHidAct: hidden active_window must stay hidden, visible windows split-tiled!
        w43->changeFolder(folderHidAct, true);
        QApplication::processEvents();
        CHECK_COND(tblHidB->isHidden() && tblHidB->status() == MyWidget::Hidden,
                   "changeFolder(folderHidAct) un-hid hidden window tblHidB!");
        CHECK_COND(!tblVisB->isHidden() && !mlVisB->isHidden()
                           && !tblVisB->isMaximized() && !mlVisB->isMaximized()
                           && tblVisB->geometry().left() < mlVisB->geometry().left(),
                   "changeFolder(folderHidAct) failed to split-tile visible fallback windows!");
        CHECK_COND(r43->activeContextText() == ctxTr("Worksheet")
                           || r43->activeContextText() == ctxTr("2D Graph"),
                   "changeFolder(folderHidAct) did not update Ribbon context to visible fallback window!");

        // 1d. Switch -> FolderAllHid: only window is hidden -> activeWindow() == nullptr & context == "Project"
        w43->changeFolder(folderAllHid, true);
        QApplication::processEvents();
        CHECK_COND(tblOnlyHid->isHidden() && folderAllHid->activeWindow() == nullptr,
                   "changeFolder(folderAllHid) un-hid tblOnlyHid or kept hidden activeWindow!");
        CHECK_COND(r43->activeContextText() == ctxTr("Project"),
                   "changeFolder(folderAllHid) left Ribbon context pill stale instead of 'Project'!");

        w43->savedProject();
        w43->setOleEmbeddedMode(true);
        delete w43;
    }

    // =========================================================================
    // 2. #152 P2-R2-2: .sciprj <hidden> Window Restore Preserving Folder::activeWindow()
    // =========================================================================
    {
        const QString sciprjPath = tmpDir.filePath("suite43_hidden_after_active.sciprj");
        {
            ApplicationWindow *wProj = new ApplicationWindow();
            wProj->applyUserSettings();
            wProj->resize(1200, 800);
            wProj->show();
            QApplication::processEvents();

            Table *tBase = wProj->newTable(5, 2, "S43BaseTbl", "Visible Base Table");
            for (int r = 0; r < 5; ++r) {
                tBase->setCell(r, 0, double(r));
                tBase->setCell(r, 1, double(10 + 5 * r));
            }
            MultiLayer *mlActive = wProj->multilayerPlot(
                    tBase, QStringList() << (QString(tBase->name()) + "_2"), Graph::LineSymbols);
            CHECK_COND(mlActive != nullptr, "Failed to create mlActive");
            mlActive->setName("S43SavedActiveGraph");

            // Create trailing Table & Matrix AFTER mlActive, then hide both and re-activate mlActive
            Table *tHidAfter = wProj->newTable(4, 2, "S43TrailingHiddenTbl", "Trailing Hidden Table");
            wProj->hideWindow(tHidAfter);
            Matrix *mHidAfter = wProj->newMatrix(4, 4);
            mHidAfter->setName("S43TrailingHiddenMat");
            wProj->hideWindow(mHidAfter);

            wProj->activateSubWindow(mlActive);
            wProj->projectFolder()->setActiveWindow(mlActive);
            QApplication::processEvents();

            CHECK_COND(wProj->projectFolder()->activeWindow() == mlActive,
                       "Pre-save activeWindow must be S43SavedActiveGraph");
            const QByteArray projBytes = wProj->saveProjectToByteArray();
            CHECK_COND(!projBytes.isEmpty(), "saveProjectToByteArray returned empty in Suite 43");
            QFile outF(sciprjPath);
            CHECK_COND(outF.open(QIODevice::WriteOnly), "Failed to open suite43 .sciprj for write");
            outF.write(projBytes);
            outF.close();

            wProj->savedProject();
            wProj->setOleEmbeddedMode(true);
            delete wProj;
        }

        ApplicationWindow *rProj = new ApplicationWindow();
        rProj->applyUserSettings();
        rProj->resize(1200, 800);
        rProj->show();
        const bool okLoad = rProj->loadProject(sciprjPath);
        QApplication::processEvents();
        CHECK_COND(okLoad, "rProj->loadProject failed on suite43_hidden_after_active.sciprj");

        MultiLayer *mlRel = qobject_cast<MultiLayer *>(rProj->window("S43SavedActiveGraph"));
        Table *tHidRel = qobject_cast<Table *>(rProj->window("S43TrailingHiddenTbl"));
        Matrix *mHidRel = qobject_cast<Matrix *>(rProj->window("S43TrailingHiddenMat"));
        CHECK_COND(mlRel != nullptr && tHidRel != nullptr && mHidRel != nullptr,
                   "Reloaded project missing S43SavedActiveGraph or trailing hidden windows");
        CHECK_COND(tHidRel->isHidden() && tHidRel->status() == MyWidget::Hidden
                           && mHidRel->isHidden() && mHidRel->status() == MyWidget::Hidden,
                   "Trailing <hidden> Table/Matrix did not restore as Hidden!");
        CHECK_COND(rProj->projectFolder()->activeWindow() == mlRel,
                   "Trailing <hidden> window restore clobbered Folder::activeWindow()!");
        CHECK_COND(rProj->modernRibbonBar() != nullptr
                           && rProj->modernRibbonBar()->activeContextText()
                                   == QCoreApplication::translate("ModernRibbonBar", "2D Graph"),
                   "Reloaded project Ribbon context did not reflect S43SavedActiveGraph ('2D Graph')!");

        rProj->savedProject();
        rProj->setOleEmbeddedMode(true);
        delete rProj;
    }

    // =========================================================================
    // 3. #152 P2-R2-3 & #153 P0-R2-2/P0-R2-3/P1-R2-1:
    //    AxesDialog & Graph Context Menu Duotone SVG Icons + C++ Non-Interactive Guards & UTF-8 CSV
    // =========================================================================
    {
        Table *t43 = mw->newTable(4, 2, "Suite43GuardTbl", "Guard & Icon Check");
        t43->setColName(0, "TempK");
        const QString jpKrCol = QString::fromUtf8("電流_μA_실험");
        t43->setColName(1, jpKrCol);
        const double xVals[4] = { 300.0, 400.0, 500.0, 600.0 };
        const double yVals[4] = { 12500.5, 98765.25, 1234567.891234, -45678.125 };
        for (int r = 0; r < 4; ++r) {
            t43->setCell(r, 0, xVals[r]);
            t43->setCell(r, 1, yVals[r]);
        }
        MultiLayer *ml43 = mw->multilayerPlot(
                t43, QStringList() << (QString(t43->name()) + "_" + jpKrCol), Graph::LineSymbols);
        Graph *g43 = ml43 ? ml43->activeGraph() : nullptr;
        CHECK_COND(g43 != nullptr, "Failed to create g43 in Suite 43");

        // 3a. AxesDialog 4 page QListWidgets use crisp 2-color Duotone SVG icons (zero legacy .xpm)
        Suite43AxesDialogProbe axesProbe;
        axesProbe.setGraph(g43);
        const QList<QListWidget *> pageLists = axesProbe.pageAxisLists();
        const int expectedCounts[4] = { 4, 2, 4, 4 }; // Scale(4), Grid(2: H/V), Axes(4), AxisBreak(4)
        CHECK_COND(pageLists.size() == 4, "Expected 4 page axis QListWidgets in AxesDialog");
        for (int li = 0; li < pageLists.size(); ++li) {
            QListWidget *lw = pageLists[li];
            CHECK_COND(lw != nullptr && lw->count() == expectedCounts[li]
                               && lw->iconSize() == QSize(24, 24),
                       "AxesDialog axis QListWidget missing expected items or 24x24 iconSize!");
            for (int i = 0; i < lw->count(); ++i) {
                QListWidgetItem *it = lw->item(i);
                CHECK_COND(it != nullptr && hasDuotoneAntiAliasedPixels(it->icon(), 48),
                           "AxesDialog QListWidget item is not backed by a multi-DPI 2-color Duotone SVG icon!");
            }
        }

        // Verify Graph title/axis context menu & AxesDialog semantic icon IDs render 2-color Duotone SVGs
        const char *ctxMenuIconIds[] = {
            "cut", "copy", "delete", "settings", "axis-scale", "axis-grid", "axis-ticks-in"
        };
        for (const char *cid : ctxMenuIconIds) {
            const QIcon ic = IconLoader::duotoneIcon(QString::fromLatin1(cid));
            CHECK_COND(hasDuotoneAntiAliasedPixels(ic, 48),
                       "Graph context menu icon ID failed 48px 2-color Duotone SVG check!");
        }

        // 3b. Direct C++ non-interactive guards on ApplicationWindow::renameWindow & Table::exportASCII
        Table *t43Dup = mw->newTable(2, 2, "Suite43DupTargetTbl", "Collision Target");
        const bool prevHeadless = mw->isHeadlessMode();
        mw->setHeadlessMode(true);

        // Invalid chars, existing window collision, and column-name collision must return false immediately
        CHECK_COND(!mw->renameWindow(t43, "Invalid Spaces &Parens!"),
                   "renameWindow accepted invalid characters in headless mode!");
        CHECK_COND(!mw->renameWindow(t43, t43Dup->name()),
                   "renameWindow accepted duplicate window name in headless mode!");
        CHECK_COND(!mw->renameWindow(t43, "TempK"),
                   "renameWindow accepted Table's own column label in headless mode!");

        // Exporting to an existing directory must return false cleanly with zero modal QMessageBox
        CHECK_COND(!t43->exportASCII(tmpDir.path(), ",", true, false),
                   "Table::exportASCII to an existing directory must return false in headless mode!");

        // Exporting Table::exportASCII with ',', JP/KR header, and >=10,000 numbers must write UTF-8 without locale thousands commas
        const QString directCsvPath = tmpDir.filePath("suite43_table_export_ascii.csv");
        CHECK_COND(t43->exportASCII(directCsvPath, ",", true, false),
                   "Table::exportASCII valid CSV export returned false!");
        mw->setHeadlessMode(prevHeadless);

        QFile csvF(directCsvPath);
        CHECK_COND(csvF.open(QIODevice::ReadOnly), "Failed to read suite43_table_export_ascii.csv");
        const QByteArray rawCsv = csvF.readAll();
        csvF.close();
        const QString csvUtf8 = QString::fromUtf8(rawCsv);
        CHECK_COND(csvUtf8.contains(jpKrCol) && !csvUtf8.contains("??"),
                   "Table::exportASCII failed UTF-8 JP/KR column header encoding!");
        CHECK_COND(csvUtf8.contains("12500.5") && csvUtf8.contains("98765.25")
                           && csvUtf8.contains("1234567.891234")
                           && !csvUtf8.contains("12,500") && !csvUtf8.contains("98,765"),
                   "Table::exportASCII corrupted numeric CSV with locale thousands-separator commas or 6-digit truncation!");
    }

    mw->hide();
    QApplication::processEvents();
    std::cout << "[INFO] Suite 43 Round 3 Critic Regression Gate passed!\n";
    return 0;
}


int runSuite44Round4UnifiedRegressionGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 44: Round 4 Unified Critic & QA Regression Gate (3D Underscore Tables & Headless UTF-8/Arial Vector _wfopen, Single-Hop appendProject, raw_V vs V Formula Lookup, Non-Interactive GUI Guard & ImportOPJ JA)...\n";

    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 44");
    QTemporaryDir tmpDir;
    CHECK_COND(tmpDir.isValid(), "Failed to create QTemporaryDir for Suite 44");

    // =========================================================================
    // 1. #166 P0-R3-1 + #167 P1-1: 3D Underscore-Named Table (CT_Strain_Raw),
    //    updateScales, Cell Mutation, First-Call Headless UTF-8/Arial .svg/.pdf
    //    on Non-ANSI Unicode Path (_wfopen), and .sciprj Round-Trip!
    // =========================================================================
    {
        ApplicationWindow *w44 = new ApplicationWindow();
        w44->applyUserSettings();
        w44->setHeadlessMode(true);
        w44->confirmCloseTable = false;
        w44->confirmClosePlot3D = false;
        w44->confirmClosePlot2D = false;
        w44->confirmCloseFolder = false;

        Table *t3d = w44->newTable(6, 3, "CT_Strain_Raw", "3D Underscore XYZ");
        CHECK_COND(t3d != nullptr && QString(t3d->name()) == "CT_Strain_Raw",
                   "Failed to create underscore-named Table CT_Strain_Raw");
        t3d->setColName(0, "X_pos");
        t3d->setColName(1, "Y_pos");
        t3d->setColName(2, "Z_val");
        t3d->setColPlotDesignation(0, SciDAVis::X);
        t3d->setColPlotDesignation(1, SciDAVis::Y);
        t3d->setColPlotDesignation(2, SciDAVis::Z);
        for (int r = 0; r < 6; ++r) {
            t3d->setCell(r, 0, double(r) * 1.5);
            t3d->setCell(r, 1, double(r) * 2.0 + 0.5);
            t3d->setCell(r, 2, double(r * r) + 5.0);
        }

        Graph3D *g3d = w44->dataPlotXYZ(t3d, t3d->colName(2), Graph3D::Scatter);
        CHECK_COND(g3d != nullptr && g3d->sp != nullptr && g3d->hasData(),
                   "dataPlotXYZ returned null or empty Graph3D for underscore-named CT_Strain_Raw!");
        CHECK_COND(g3d->formula().contains("CT_Strain_Raw_X_pos(X),CT_Strain_Raw_Y_pos(Y),CT_Strain_Raw_Z_val(Z)"),
                   "Graph3D::formula() corrupted underscore-named table/column XYZ binding!");

        // Set UTF-8 Japanese 3D axis labels and exercise scale update + live cell edit
        const QString xLblJa = QString::fromUtf8("X\xE8\xBB\xB8"); // "X軸"
        const QString yLblJa = QString::fromUtf8("Y\xE8\xBB\xB8"); // "Y軸"
        const QString zLblJa = QString::fromUtf8("Z\xE8\xBB\xB8"); // "Z軸"
        g3d->setAxesLabels(QStringList() << xLblJa << yLblJa << zLblJa);
        g3d->updateScales(-1.0, 12.0, -1.0, 16.0, 0.0, 60.0);
        CHECK_COND(g3d->hasData() && std::fabs(g3d->zStop() - 60.0) < 1e-6,
                   "Graph3D::updateScales failed to apply Z scale [0, 60] on CT_Strain_Raw!");
        t3d->setCell(5, 2, 48.5);
        QApplication::processEvents();
        CHECK_COND(g3d->hasData() && std::fabs(g3d->zStop() - 48.5) < 1e-6,
                   "Table::setCell failed to live-update Graph3D Z data (expected zStop=48.5) on CT_Strain_Raw!");
        CHECK_COND(g3d->xAxisLabelFont().family() == "Arial"
                           && g3d->yAxisLabelFont().family() == "Arial"
                           && g3d->zAxisLabelFont().family() == "Arial"
                           && g3d->numbersFont().family() == "Arial",
                   "Graph3D fonts violated Arial invariant!");

        // First-call headless vector export to non-ANSI multi-script Unicode paths (_wfopen)
        const QString uniBase = QString::fromUtf8("suite44_3d_\xC4\x8D_\xE2\x82\xAC_\xE5\xAE\x9F\xE9\xA8\x93");
        const QString svg3dPath = tmpDir.filePath(uniBase + ".svg");
        const QString pdf3dPath = tmpDir.filePath(uniBase + ".pdf");
        try {
            g3d->exportVector(svg3dPath, "svg");
        } catch (const std::exception &ex) {
            std::cerr << "[FAIL] Suite 44 exportVector threw: " << ex.what() << std::endl;
            return 1;
        }
        try {
            g3d->exportPDF(pdf3dPath);
        } catch (const std::exception &ex) {
            std::cerr << "[FAIL] Suite 44 exportPDF threw: " << ex.what() << std::endl;
            return 1;
        }

        QFile fSvg(svg3dPath);
        CHECK_COND(fSvg.open(QIODevice::ReadOnly),
                   "Graph3D::exportVector failed to write Unicode-path .svg via _wfopen!");
        const QByteArray svgBytes = fSvg.readAll();
        fSvg.close();
        CHECK_COND(svgBytes.size() > 500 && !svgBytes.contains('\xFF'),
                   "3D SVG export is empty or contains raw Latin-1 0xFF byte corruption!");
        const QString svgUtf8 = QString::fromUtf8(svgBytes);
        CHECK_COND(svgUtf8.contains("<svg") && svgUtf8.contains("<text"),
                   "First-call headless 3D SVG export missing <svg> or <text> elements!");
        CHECK_COND(svgUtf8.contains("Arial") && !svgUtf8.contains("Courier"),
                   "3D SVG export used Courier instead of Arial!");
        CHECK_COND(svgUtf8.contains(xLblJa) && !svgUtf8.contains(QChar::ReplacementCharacter),
                   "3D SVG export corrupted UTF-8 Japanese axis label!");

        QFile fPdf(pdf3dPath);
        CHECK_COND(fPdf.open(QIODevice::ReadOnly),
                   "Graph3D::exportPDF failed to write Unicode-path .pdf via _wfopen!");
        const QByteArray pdfBytes = fPdf.readAll();
        fPdf.close();
        CHECK_COND(pdfBytes.startsWith("%PDF-") && pdfBytes.size() > 800,
                   "3D PDF export on Unicode path is invalid or truncated!");

        // Save & reload .sciprj and verify CT_Strain_Raw + Graph3D survive intact
        const QString sciprj3dPath = tmpDir.filePath("suite44_3d_underscore.sciprj");
        w44->saveFolder(w44->projectFolder(), sciprj3dPath);
        w44->savedProject();
        CHECK_COND(QFileInfo::exists(sciprj3dPath) && QFileInfo(sciprj3dPath).size() > 200,
                   "Failed to save suite44_3d_underscore.sciprj");

        ApplicationWindow *w44Reload = new ApplicationWindow();
        w44Reload->applyUserSettings();
        w44Reload->setHeadlessMode(true);
        w44Reload->confirmCloseTable = false;
        w44Reload->confirmClosePlot3D = false;
        CHECK_COND(w44Reload->loadProject(sciprj3dPath),
                   "loadProject failed on suite44_3d_underscore.sciprj!");

        Table *reloadedTbl = w44Reload->table("CT_Strain_Raw");
        Graph3D *reloaded3d = nullptr;
        for (MyWidget *rw : w44Reload->windowsList()) {
            if (rw && rw->inherits("Graph3D")) {
                reloaded3d = static_cast<Graph3D *>(rw);
                break;
            }
        }
        CHECK_COND(reloadedTbl != nullptr && reloaded3d != nullptr && reloaded3d->hasData(),
                   "Reloaded .sciprj lost CT_Strain_Raw or Graph3D XYZ data!");

        w44Reload->savedProject();
        delete w44Reload;
        w44->savedProject();
        delete w44;
    }

    // =========================================================================
    // 2. #166 P1-R3-1 + P2-R3-1 + #169 P1-5: Chained Table1+Table2 Single-Hop
    //    appendProject, Multi-Folder newProject Cleanup, Multi-Dot Folder Name,
    //    and Corrupt .sciprj Rollback
    // =========================================================================
    {
        // Build source project with Table1 (y=111) + Table2 (y=222) and two curves
        const QString multiDotProjPath = tmpDir.filePath("my.study.part1.sciprj");
        {
            ApplicationWindow *wSrc = new ApplicationWindow();
            wSrc->applyUserSettings();
            wSrc->setHeadlessMode(true);
            wSrc->confirmCloseTable = false;
            wSrc->confirmClosePlot2D = false;

            Table *sT1 = wSrc->newTable(4, 2, "Table1", "Source T1");
            Table *sT2 = wSrc->newTable(4, 2, "Table2", "Source T2");
            for (int r = 0; r < 4; ++r) {
                sT1->setCell(r, 0, double(r));
                sT1->setCell(r, 1, (r == 3) ? 111.0 : 10.0);
                sT2->setCell(r, 0, double(r));
                sT2->setCell(r, 1, (r == 3) ? 222.0 : 20.0);
            }
            MultiLayer *mlT1 = wSrc->multilayerPlot(
                    sT1, QStringList() << "Table1_2", Graph::LineSymbols);
            mlT1->setName("ChainGraphT1");
            MultiLayer *mlT2 = wSrc->multilayerPlot(
                    sT2, QStringList() << "Table2_2", Graph::LineSymbols);
            mlT2->setName("ChainGraphT2");

            wSrc->saveFolder(wSrc->projectFolder(), multiDotProjPath);
            wSrc->savedProject();
            delete wSrc;
        }

        ApplicationWindow *wHost = new ApplicationWindow();
        wHost->applyUserSettings();
        wHost->setHeadlessMode(true);
        wHost->confirmCloseTable = false;
        wHost->confirmClosePlot2D = false;
        wHost->confirmCloseFolder = false;

        // Verify RPC app.new_project (resetProjectInPlaceHeadless) purges child folders cleanly
        wHost->addFolder();
        CHECK_COND(!wHost->projectFolder()->folders().isEmpty(),
                   "Precondition failed: addFolder did not create child folder");
        wHost->savedProject();
        CHECK_COND(wHost->pythonAutomationServer() != nullptr,
                   "wHost->pythonAutomationServer() is null");
        QJsonObject npReq;
        npReq[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        npReq[QStringLiteral("id")] = 1;
        npReq[QStringLiteral("method")] = QStringLiteral("app.new_project");
        npReq[QStringLiteral("params")] = QJsonObject();
        const QJsonObject npResp = wHost->pythonAutomationServer()->handleRpcRequest(npReq);
        const QJsonObject npRes = npResp.value("result").toObject();
        CHECK_COND(npRes.value("ok").toBool() && wHost->projectFolder()->folders().isEmpty(),
                   "app.new_project left stale child folders under projectFolder()!");

        // Create host Table1 (y_max = 9.0) so appending my.study.part1.sciprj chains
        // Table1 -> Table2 AND Table2 -> Table3 simultaneously!
        Table *hostT1 = wHost->newTable(4, 2, "Table1", "Host T1");
        for (int r = 0; r < 4; ++r) {
            hostT1->setCell(r, 0, double(r));
            hostT1->setCell(r, 1, 9.0);
        }

        Folder *rootBefore = wHost->projectFolder();
        wHost->changeFolder(rootBefore, true);
        wHost->appendProject(multiDotProjPath);
        QApplication::processEvents();

        CHECK_COND(wHost->currentFolder() == rootBefore,
                   "appendProject mutated caller's currentFolder()!");
        CHECK_COND(rootBefore->folders().size() == 1,
                   "appendProject did not create exactly 1 appended child folder!");
        Folder *appendedFld = rootBefore->folders().last();
        CHECK_COND(appendedFld->name() == "my.study.part1",
                   "appendProject did not strip .sciprj from multi-dot filename my.study.part1.sciprj!");

        MultiLayer *appG1 = nullptr;
        MultiLayer *appG2 = nullptr;
        for (MyWidget *fw : appendedFld->windowsList()) {
            if (fw && fw->inherits("MultiLayer")) {
                if (QString(fw->name()) == "ChainGraphT1")
                    appG1 = static_cast<MultiLayer *>(fw);
                else if (QString(fw->name()) == "ChainGraphT2")
                    appG2 = static_cast<MultiLayer *>(fw);
            }
        }
        CHECK_COND(appG1 != nullptr && appG2 != nullptr
                           && appG1->activeGraph() != nullptr && appG2->activeGraph() != nullptr,
                   "Appended folder missing ChainGraphT1 or ChainGraphT2!");

        auto *dc1 = dynamic_cast<DataCurve *>(appG1->activeGraph()->curve(0));
        auto *dc2 = dynamic_cast<DataCurve *>(appG2->activeGraph()->curve(0));
        CHECK_COND(dc1 != nullptr && dc2 != nullptr && dc1->dataSize() == 4 && dc2->dataSize() == 4,
                   "Appended ChainGraphT1/ChainGraphT2 missing 4-point DataCurve!");
        std::cout << "[INFO] Suite 44 Chained appendProject: dc1->title='"
                  << dc1->title().text().toStdString() << "' y[3]=" << dc1->y(3)
                  << ", dc2->title='" << dc2->title().text().toStdString()
                  << "' y[3]=" << dc2->y(3) << "\n";
        CHECK_COND(std::fabs(dc1->y(3) - 111.0) < 1e-9 && std::fabs(dc2->y(3) - 222.0) < 1e-9,
                   "Chained Table1+Table2 appendProject double-hopped ChainGraphT1 onto Table3!");

        // Corrupt .sciprj append must rollback cleanly without leaving an orphan folder
        const QString corruptPath = tmpDir.filePath("corrupt_payload.sciprj");
        {
            QFile cf(corruptPath);
            CHECK_COND(cf.open(QIODevice::WriteOnly), "Failed to create corrupt_payload.sciprj");
            cf.write("NOT_A_VALID_TORIGRAPH_PROJECT_HEADER\n<<<GARBAGE>>>\n");
            cf.close();
        }
        const int foldersBeforeCorrupt = rootBefore->folders().size();
        wHost->appendProject(corruptPath);
        QApplication::processEvents();
        CHECK_COND(rootBefore->folders().size() == foldersBeforeCorrupt,
                   "Corrupt appendProject left an orphan subfolder behind!");

        wHost->savedProject();
        delete wHost;
    }

    // =========================================================================
    // 3. #169 P1-6 + #170 P1-1 + #166 P0-R3-2 + #180: "raw_V" vs "V" Exact
    //    Column Lookup, tablecol("T", "C") Space Trimming, torigraph_noninteractive
    //    GUI Guard, and ImportOPJ Japanese Localization
    // =========================================================================
    {
        mw->setScriptingLang("muParser");
        Table *tCol = mw->newTable(4, 3, "S44FormulaTbl", "raw_V vs V");
        CHECK_COND(tCol != nullptr, "Failed to create S44FormulaTbl");
        tCol->setColName(0, "V");
        tCol->setColName(1, "raw_V");
        tCol->setColName(2, "calc");
        for (int r = 0; r < 4; ++r) {
            tCol->setCell(r, 0, double(r + 1));          // V = 1, 2, 3, 4
            tCol->setCell(r, 1, double((r + 1) * 100));  // raw_V = 100, 200, 300, 400
        }
        tCol->setCommand(2, "col(\"raw_V\") + tablecol(\"S44FormulaTbl\", \"V\")");
        CHECK_COND(tCol->recalculate(2, false),
                   "Table::recalculate failed on col(\"raw_V\") + tablecol(\"S44FormulaTbl\", \"V\")!");
        CHECK_COND(std::fabs(tCol->cell(0, 2) - 101.0) < 1e-9
                           && std::fabs(tCol->cell(3, 2) - 404.0) < 1e-9,
                   "col(\"raw_V\") resolved to \"V\" or tablecol(\"Tbl\", \"Col\") failed space trimming!");

        // Verify torigraph_noninteractive QCoreApplication property guards even a visible GUI window!
        const bool prevHeadless = mw->isHeadlessMode();
        mw->setHeadlessMode(false);
        CHECK_COND(!ApplicationWindow::isNonInteractiveContext(mw),
                   "Visible non-headless mw unexpectedly reported isNonInteractiveContext == true");
        qApp->setProperty("torigraph_noninteractive", true);
        CHECK_COND(ApplicationWindow::isNonInteractiveContext(mw),
                   "torigraph_noninteractive property failed to activate isNonInteractiveContext on GUI mw!");
        CHECK_COND(!mw->renameWindow(tCol, "Invalid Name With Spaces!"),
                   "renameWindow accepted invalid characters under torigraph_noninteractive guard!");
        qApp->setProperty("torigraph_noninteractive", QVariant());
        mw->setHeadlessMode(prevHeadless);

        // Verify Round 4 ImportOPJ Japanese translation entry
        mw->switchToLanguage("ja");
        QApplication::processEvents();
        const QString jaOpjErr =
                QCoreApplication::translate("ImportOPJ", "Failed to parse Origin project file.");
        bool hasJaChar = false;
        for (const QChar &ch : jaOpjErr) {
            if (ch.unicode() >= 0x3000) {
                hasJaChar = true;
                break;
            }
        }
        CHECK_COND(hasJaChar,
                   "ImportOPJ 'Failed to parse Origin project file.' missing Japanese translation!");
        mw->switchToLanguage("en");
        QApplication::processEvents();

        // #183 [P0-R4-1] + [P2-R4-2]: Verify subfolder closeWindow + savedProject() UAF guard
        // and idempotent app.trigger_action(checked=false/true) on toggle QActions!
        for (int cyc = 0; cyc < 5; ++cyc) {
            QJsonObject reqT;
            reqT["jsonrpc"] = "2.0";
            reqT["id"] = 4410 + cyc * 4;
            reqT["method"] = "app.trigger_action";
            QJsonObject pF;
            pF["name"] = "actionToggleFocusHudMode";
            pF["checked"] = false;
            reqT["params"] = pF;
            QJsonObject rF = mw->pythonAutomationServer()->handleRpcRequest(reqT);
            CHECK_COND(rF.value("result").toObject().value("checked").toBool(true) == false,
                       "app.trigger_action(actionToggleFocusHudMode, checked=false) inverted state!");

            Table *uafTbl = mw->newTable(4, 2, QString("S44UafTbl%1").arg(cyc), "UAF Check");
            Matrix *uafMtx = mw->newMatrix(QString("S44UafMtx%1").arg(cyc), 3, 3);
            CHECK_COND(uafTbl != nullptr && uafMtx != nullptr, "Failed to allocate S44 UAF Table/Matrix");
            uafTbl->showNormal();
            uafMtx->showNormal();
            QApplication::processEvents();
            QPointer<Table> pTbl(uafTbl);
            QPointer<Matrix> pMtx(uafMtx);
            uafTbl->askOnCloseEvent(false);
            uafMtx->askOnCloseEvent(false);
            uafTbl->close();
            uafMtx->close();
            mw->savedProject(); // clears QUndoStack -> destroys future::Table & future::Matrix + detached views
            QApplication::processEvents();
            CHECK_COND(pTbl.isNull() && pMtx.isNull(),
                       "Detached Table/Matrix view survived future::Table/Matrix destruction after undoStack()->clear()!");
        }
    }

    mw->hide();
    QApplication::processEvents();
    std::cout << "[INFO] Suite 44 Round 4 Unified Regression Gate passed!\n";
    return 0;
}

int runSuite45Round5ZeroDefectGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 45: Round 5 Zero-Defect Gate (Multi-Curve 'raw_V' vs 'V', gadget.set_roi Kwargs, Swift Auto-Guess & Domain Jacobian Guard, 2-Level Text Binary fit_logistic/roc)...\n";
    CHECK_COND(mw != nullptr && mw->pythonAutomationServer() != nullptr,
               "Suite 45 requires live ApplicationWindow + PythonAutomationServer");

    auto rpcCall = [&](int id, const QString &method, const QJsonObject &params) -> QJsonObject {
        QJsonObject req;
        req["jsonrpc"] = "2.0";
        req["id"] = id;
        req["method"] = method;
        req["params"] = params;
        return mw->pythonAutomationServer()->handleRpcRequest(req);
    };

    // Reset clean workspace
    rpcCall(4500, "app.new_project", QJsonObject());

    // =========================================================================
    // 1. #194 [P1-R4-1]: Short-Curve-Name Resolution ("raw_V" vs "V") Across
    //    CurveMathTools, AdvancedSignalProc, and PythonAutomationServer
    // =========================================================================
    {
        Table *tHj = mw->newTable(16, 3, "S45MultiCurveHJ", "raw_V vs V");
        CHECK_COND(tHj != nullptr, "Failed to create S45MultiCurveHJ");
        tHj->setColName(0, "x");
        tHj->setColName(1, "V");
        tHj->setColName(2, "raw_V");
        tHj->setColPlotDesignation(0, SciDAVis::X);
        tHj->setColPlotDesignation(1, SciDAVis::Y);
        tHj->setColPlotDesignation(2, SciDAVis::Y);
        for (int r = 0; r < 16; ++r) {
            const double xv = double(r);
            tHj->setCell(r, 0, xv);
            tHj->setCell(r, 1, 2.0 * xv);      // V: slope 2, area on [0..15] = 225
            tHj->setCell(r, 2, 100.0 * xv);    // raw_V: slope 100, area on [0..15] = 11250
        }

        // Plot raw_V FIRST (curve 0) and V SECOND (curve 1) so endsWith("_V") would trap on raw_V!
        QStringList yCols;
        yCols << (QString(tHj->name()) + "_raw_V") << (QString(tHj->name()) + "_V");
        MultiLayer *mlHj = mw->multilayerPlot(tHj, yCols, Graph::LineSymbols, 0, 15);
        CHECK_COND(mlHj != nullptr && mlHj->activeGraph() != nullptr, "Failed to plot S45MultiCurveHJ");
        Graph *gHj = mlHj->activeGraph();

        // C++ CurveMathTools::findYFromX with short name "V"
        const double yAt4 = CurveMathTools::findYFromX(gHj, "V", 4.0);
        CHECK_COND(std::fabs(yAt4 - 8.0) < 1e-6,
                   "CurveMathTools::resolveCurve('V') hijacked by 'raw_V'!");

        // C++ AdvancedSignalProc::iirFilter with short name "V"
        QwtPlotCurve *cIirV = AdvancedSignalProc::iirFilter(
                mw, gHj, "V", AdvancedSignalProc::Butterworth,
                AdvancedSignalProc::LowPass, 2, 0.4, 0.0, true);
        CHECK_COND(cIirV != nullptr && cIirV->dataSize() == 16,
                   "AdvancedSignalProc::resolveSignalCurve('V') failed on multi-curve graph!");
        CHECK_COND(cIirV->y(8) < 40.0,
                   "AdvancedSignalProc::resolveSignalCurve('V') hijacked by 'raw_V' (y ~ 800 instead of 16)!");

        // RPC curvemath.integrate + analysis.fit_linear on "V"
        QJsonObject pInt;
        pInt["graph"] = mlHj->name();
        pInt["curve"] = "V";
        QJsonObject rInt = rpcCall(4501, "curvemath.integrate", pInt).value("result").toObject();
        CHECK_COND(rInt.value("ok").toBool() && std::fabs(rInt.value("area").toDouble() - 225.0) < 1e-5,
                   "RPC curvemath.integrate(curve='V') hijacked by 'raw_V'!");

        QJsonObject pLin;
        pLin["graph"] = mlHj->name();
        pLin["curve"] = "V";
        QJsonObject rLin = rpcCall(4502, "analysis.fit_linear", pLin).value("result").toObject();
        CHECK_COND(rLin.value("ok").toBool() && std::fabs(rLin.value("slope").toDouble() - 2.0) < 1e-5,
                   "RPC analysis.fit_linear(curve='V') hijacked by 'raw_V'!");

        // #194 [P2-R4-1]: gadget.set_roi without x_min/x_max (vertical_cursor x=5.0, delta_cursor x1/x2)
        QJsonObject pAttVc;
        pAttVc["graph"] = mlHj->name();
        pAttVc["type"] = "vertical_cursor";
        pAttVc["x"] = 2.0;
        CHECK_COND(rpcCall(4503, "gadget.attach", pAttVc).value("result").toObject().value("ok").toBool(),
                   "gadget.attach(vertical_cursor) failed");
        QJsonObject pSetVc;
        pSetVc["graph"] = mlHj->name();
        pSetVc["x"] = 5.0;
        CHECK_COND(rpcCall(4504, "gadget.set_roi", pSetVc).value("result").toObject().value("ok").toBool(),
                   "gadget.set_roi(x=5.0) failed");
        QJsonObject pQVc;
        pQVc["graph"] = mlHj->name();
        pQVc["type"] = "vertical_cursor";
        QJsonObject rQVc = rpcCall(4505, "gadget.query", pQVc).value("result").toObject();
        CHECK_COND(std::fabs(rQVc.value("x").toDouble(-1.0) - 5.0) < 1e-6,
                   "gadget.query(vertical_cursor) did not reflect x=5.0!");
    }

    // =========================================================================
    // 2. #195 [P1-R4-1]: Swift K*(eps0+x)^n Auto-Guess & Out-of-Domain Jacobian
    // =========================================================================
    {
        QJsonObject pCreate;
        pCreate["name"] = "S45SwiftTbl";
        pCreate["rows"] = 70;
        pCreate["cols"] = 2;
        rpcCall(4510, "table.create", pCreate);
        Table *tSw = mw->table("S45SwiftTbl");
        CHECK_COND(tSw != nullptr, "Failed to create S45SwiftTbl");
        tSw->setColName(0, "x");
        tSw->setColName(1, "y");
        tSw->setColPlotDesignation(0, SciDAVis::X);
        tSw->setColPlotDesignation(1, SciDAVis::Y);
        for (int i = 0; i < 70; ++i) {
            const double xv = 0.002 + 0.398 * double(i) / 69.0;
            const double yv = 780.0 * std::pow(0.015 + xv, 0.24);
            tSw->setCell(i, 0, xv);
            tSw->setCell(i, 1, yv);
        }

        QJsonObject pFitSw;
        pFitSw["table"] = "S45SwiftTbl";
        pFitSw["x_col"] = "x";
        pFitSw["y_col"] = "y";
        pFitSw["model"] = "Swift";
        QJsonObject rSw = rpcCall(4511, "analysis.fit_nonlinear", pFitSw).value("result").toObject();
        const QJsonObject swParams = rSw.value("params").toObject();
        std::cout << "[INFO] Suite 45 Swift Fit: R^2=" << rSw.value("r2").toDouble()
                  << ", K=" << swParams.value("K").toDouble()
                  << ", eps0=" << swParams.value("eps0").toDouble()
                  << ", n=" << swParams.value("n").toDouble() << "\n";
        CHECK_COND(rSw.value("ok").toBool() && rSw.value("converged").toBool()
                           && rSw.value("r2").toDouble() > 0.9999
                           && std::fabs(swParams.value("K").toDouble() - 780.0) < 1.0
                           && std::fabs(swParams.value("eps0").toDouble() - 0.015) < 1e-3
                           && std::fabs(swParams.value("n").toDouble() - 0.24) < 1e-3,
                   "Swift built-in nonlinear auto-guess failed to recover exact (K, eps0, n)!");
    }

    // =========================================================================
    // 3. #195 [P1-R4-2] + #196 [P2-R4-1]: 2-Level Text Binary Encoding Across
    //    analysis.fit_logistic and stats.roc
    // =========================================================================
    {
        struct BinCase {
            const char *name;
            QString loLbl;
            QString hiLbl;
            double expRocAuc;
        };
        const BinCase cases[] = {
            { "S45BinCtrlTrt", QStringLiteral("control"), QStringLiteral("treated"), 1.0 },
            { "S45BinCaseHlth", QStringLiteral("case"), QStringLiteral("healthy"), 0.0 },
            { "S45BinJp", QString::fromUtf8("\xE9\x99\xB0\xE6\x80\xA7"), QString::fromUtf8("\xE9\x99\xBD\xE6\x80\xA7"), 1.0 },
            { "S45BinHlthDis", QStringLiteral("Healthy"), QStringLiteral("Disease"), 1.0 },
        };
        int reqId = 4520;
        for (const BinCase &bc : cases) {
            Table *tBin = mw->newTable(21, 2, QString::fromLatin1(bc.name), "2-Level Text Binary");
            CHECK_COND(tBin != nullptr, "Failed to create binary test table");
            tBin->setColName(0, "X");
            tBin->setColName(1, "Y");
            tBin->setColumnType(1, SciDAVis::ColumnMode::Text);
            for (int i = 0; i < 21; ++i) {
                const double xv = -5.0 + 0.5 * double(i);
                tBin->setCell(i, 0, xv);
                tBin->setText(i, 1, (xv > 0.0) ? bc.hiLbl : bc.loLbl);
            }

            QJsonObject pLog;
            pLog["table"] = tBin->name();
            pLog["y_col"] = "Y";
            QJsonArray xCols;
            xCols.append("X");
            pLog["x_cols"] = xCols;
            pLog["plot"] = false;
            QJsonObject rLog = rpcCall(reqId++, "analysis.fit_logistic", pLog).value("result").toObject();
            CHECK_COND(rLog.value("ok").toBool()
                               && std::fabs(rLog.value("auc").toDouble(-1.0) - 1.0) < 1e-6,
                       "analysis.fit_logistic failed 2-level text binary polarity!");

            QJsonObject pRoc;
            pRoc["table"] = tBin->name();
            pRoc["state_col"] = "Y";
            pRoc["score_col"] = "X";
            QJsonObject rRoc = rpcCall(reqId++, "stats.roc", pRoc).value("result").toObject();
            CHECK_COND(rRoc.value("ok").toBool()
                               && rRoc.value("n_pos").toInt(0) > 0
                               && rRoc.value("n_neg").toInt(0) > 0
                               && std::fabs(rRoc.value("auc").toDouble(-1.0) - bc.expRocAuc) < 1e-6
                               && !rRoc.value("graph").toString().isEmpty(),
                       "stats.roc failed 2-level text binary polarity or plotROCCurve returned nullptr!");
        }
    }

    // =========================================================================
    // 4. #208 [P0-R5-1] + [P1-R5-1]: VectorCurve Empty/NaN/Masked Guard,
    //    VectorCurve Underscore Table/Cols + Rename + .sciprj Reload,
    //    CT_Strain_Raw Rename + Live Edit + Short Lookup + .sciprj Reload,
    //    Table1->ExpA Coexistence with Table10 & Table1_Raw (2D + 3D + Legend),
    //    Matrix1->MatA Coexistence with Matrix10 (Spectrogram 2D + 3D), and
    //    Column Rename V->V_new Coexistence with V10
    // =========================================================================
    {
        QTemporaryDir tmpR5;
        CHECK_COND(tmpR5.isValid(), "Failed to create QTemporaryDir for Suite 45 #208 checks");

        // (4a) Empty / all-NaN / all-masked plot.vectors -> -32602 (0 crash, 0 leaked Graph)
        rpcCall(4540, "app.new_project", QJsonObject());
        Table *tEmptyVec = mw->newTable(4, 4, "S45EmptyVec", "Empty Vector");
        CHECK_COND(tEmptyVec != nullptr, "Failed to create S45EmptyVec");

        // Direct C++ Graph::plotVectorCurve on empty table must NOT insert a dangling VectorCurve or crash!
        MultiLayer *mlDirectVec = mw->newGraph("S45DirectEmptyVecGraph");
        Graph *gDirectVec = mlDirectVec ? (mlDirectVec->activeGraph() ? mlDirectVec->activeGraph() : mlDirectVec->addLayer()) : nullptr;
        CHECK_COND(gDirectVec != nullptr, "Failed to create S45DirectEmptyVecGraph");
        QStringList emptyVecCols;
        for (int c = 0; c < 4; ++c)
            emptyVecCols << tEmptyVec->colName(c);
        gDirectVec->plotVectorCurve(tEmptyVec, emptyVecCols, Graph::VectXYXY, 0, 3);
        gDirectVec->replot();
        CHECK_COND(gDirectVec->curves() == 0,
                   "Graph::plotVectorCurve inserted an empty/uninitialized VectorCurve!");
        mlDirectVec->askOnCloseEvent(false);
        mlDirectVec->close();
        QApplication::processEvents();

        // RPC plot.vectors on empty table -> -32602
        const int winsBeforeEmptyVec = mw->windowsList().size();
        QJsonObject pVecEmpty;
        pVecEmpty["table"] = "S45EmptyVec";
        QJsonObject respVecEmpty = rpcCall(4541, "plot.vectors", pVecEmpty);
        CHECK_COND(respVecEmpty.value("error").toObject().value("code").toInt(0) == -32602
                           && mw->windowsList().size() == winsBeforeEmptyVec,
                   "RPC plot.vectors on empty table did not return -32602 cleanly!");

        // Populate finite values, mask all rows -> -32602
        for (int r = 0; r < 4; ++r) {
            tEmptyVec->setCell(r, 0, double(r));
            tEmptyVec->setCell(r, 1, double(r));
            tEmptyVec->setCell(r, 2, 1.0);
            tEmptyVec->setCell(r, 3, 2.0);
        }
        QJsonObject pMaskAll;
        pMaskAll["table"] = "S45EmptyVec";
        QJsonArray allRowsArr;
        for (int r = 0; r < 4; ++r)
            allRowsArr.append(r);
        pMaskAll["rows"] = allRowsArr;
        CHECK_COND(rpcCall(4542, "table.mask_rows", pMaskAll).value("result").toObject().value("ok").toBool(),
                   "table.mask_rows failed on S45EmptyVec");
        QJsonObject respVecMasked = rpcCall(4543, "plot.vectors", pVecEmpty);
        CHECK_COND(respVecMasked.value("error").toObject().value("code").toInt(0) == -32602,
                   "RPC plot.vectors on all-masked table did not return -32602!");

        // (4b) VectorCurve with underscore table & underscore cols (raw_U, raw_V) + rename_window + .sciprj reload
        rpcCall(4544, "app.new_project", QJsonObject());
        Table *tvUnder = mw->newTable(4, 4, "Vec_Under_Tbl", "Underscore Vector Table");
        CHECK_COND(tvUnder != nullptr, "Failed to create Vec_Under_Tbl");
        tvUnder->setColName(0, "X");
        tvUnder->setColName(1, "Y");
        tvUnder->setColName(2, "raw_U");
        tvUnder->setColName(3, "raw_V");
        for (int r = 0; r < 4; ++r) {
            tvUnder->setCell(r, 0, double(r));
            tvUnder->setCell(r, 1, double(r));
            tvUnder->setCell(r, 2, 1.0);
            tvUnder->setCell(r, 3, 2.0);
        }
        QJsonObject pVecOk;
        pVecOk["table"] = "Vec_Under_Tbl";
        QJsonArray vecColsArr;
        vecColsArr.append("X");
        vecColsArr.append("Y");
        vecColsArr.append("raw_U");
        vecColsArr.append("raw_V");
        pVecOk["cols"] = vecColsArr;
        pVecOk["graph_name"] = "VecGraphR5";
        QJsonObject rVecOk = rpcCall(4545, "plot.vectors", pVecOk).value("result").toObject();
        CHECK_COND(rVecOk.value("ok").toBool()
                           && rVecOk.value("layers").toArray().at(0).toObject().value("curve_count").toInt(0) == 1,
                   "plot.vectors failed on Vec_Under_Tbl with underscore cols raw_U/raw_V!");

        QJsonObject pRenVec;
        pRenVec["name"] = "Vec_Under_Tbl";
        pRenVec["new_name"] = "Exp_Vec_Tbl";
        QJsonObject rRenVec = rpcCall(4546, "app.rename_window", pRenVec).value("result").toObject();
        CHECK_COND(rRenVec.value("ok").toBool()
                           && rRenVec.value("name").toString() == "Exp-Vec-Tbl"
                           && rRenVec.value("new_name").toString() == "Exp-Vec-Tbl",
                   "app.rename_window did not return canonical hyphenated name/new_name 'Exp-Vec-Tbl'!");

        const QString vecProjPath = tmpR5.filePath("vec_r5.sciprj");
        QJsonObject pSaveVec;
        pSaveVec["path"] = vecProjPath;
        CHECK_COND(rpcCall(4547, "app.save_project", pSaveVec).value("result").toObject().value("ok").toBool(),
                   "Failed to save vec_r5.sciprj");
        QJsonObject pOpenVec;
        pOpenVec["path"] = vecProjPath;
        CHECK_COND(rpcCall(4548, "app.open_project", pOpenVec).value("result").toObject().value("ok").toBool(),
                   "Failed to reload vec_r5.sciprj");
        QJsonObject pWinVec;
        pWinVec["name"] = "VecGraphR5";
        QJsonObject rWinVec = rpcCall(4549, "app.window_info", pWinVec).value("result").toObject();
        CHECK_COND(rWinVec.value("layers").toArray().at(0).toObject().value("curve_count").toInt(0) == 1,
                   "VecGraphR5 lost its VectorCurve or crashed on .sciprj reload after table rename!");

        // (4c) Underscore table CT_Strain_Raw rename + live cell edit + short-curve lookup + .sciprj reload
        rpcCall(4550, "app.new_project", QJsonObject());
        Table *tStrain = mw->newTable(5, 2, "CT_Strain_Raw", "Underscore Strain Table");
        CHECK_COND(tStrain != nullptr, "Failed to create CT_Strain_Raw");
        tStrain->setColName(0, "Time");
        tStrain->setColName(1, "V");
        tStrain->setColPlotDesignation(0, SciDAVis::X);
        tStrain->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < 5; ++r) {
            tStrain->setCell(r, 0, double(r));
            tStrain->setCell(r, 1, double(2 * r + 1));
        }
        QJsonObject pPlotStrain;
        pPlotStrain["table"] = "CT_Strain_Raw";
        pPlotStrain["x_col"] = "Time";
        pPlotStrain["y_cols"] = QJsonArray{ "V" };
        pPlotStrain["graph_name"] = "StrainGraphR5";
        CHECK_COND(rpcCall(4551, "plot.xy", pPlotStrain).value("result").toObject().value("ok").toBool(),
                   "plot.xy failed on CT_Strain_Raw");

        QJsonObject pRenStrain;
        pRenStrain["name"] = "CT_Strain_Raw";
        pRenStrain["new_name"] = "Renamed_Strain_Tbl";
        QJsonObject rRenStrain = rpcCall(4552, "app.rename_window", pRenStrain).value("result").toObject();
        CHECK_COND(rRenStrain.value("ok").toBool()
                           && rRenStrain.value("new_name").toString() == "Renamed-Strain-Tbl",
                   "app.rename_window failed on CT_Strain_Raw!");

        QJsonObject pCellStrain;
        pCellStrain["table"] = "Renamed-Strain-Tbl";
        pCellStrain["row"] = 4;
        pCellStrain["col"] = 1;
        pCellStrain["value"] = 999.0;
        CHECK_COND(rpcCall(4553, "table.set_cell", pCellStrain).value("result").toObject().value("ok").toBool(),
                   "table.set_cell failed on Renamed-Strain-Tbl!");

        QJsonObject pInfoStrain;
        pInfoStrain["name"] = "StrainGraphR5";
        QJsonObject rInfoStrain = rpcCall(4554, "app.window_info", pInfoStrain).value("result").toObject();
        const double yMaxLive = rInfoStrain.value("layers").toArray().at(0).toObject()
                                        .value("curves").toArray().at(0).toObject().value("y_max").toDouble();
        CHECK_COND(std::fabs(yMaxLive - 999.0) < 1e-6,
                   "DataCurve live cell edit failed to update y_max after renaming CT_Strain_Raw!");

        QJsonObject pLkpStrain;
        pLkpStrain["graph"] = "StrainGraphR5";
        pLkpStrain["curve"] = "V";
        pLkpStrain["x"] = 4.0;
        QJsonObject rLkpStrain = rpcCall(4555, "curvemath.lookup_xy", pLkpStrain).value("result").toObject();
        CHECK_COND(rLkpStrain.value("ok").toBool()
                           && std::fabs(rLkpStrain.value("y").toDouble() - 999.0) < 1e-6,
                   "Short-curve lookup curve='V' failed after renaming CT_Strain_Raw!");

        const QString strainProjPath = tmpR5.filePath("strain_r5.sciprj");
        CHECK_COND(rpcCall(4556, "app.save_project", QJsonObject{ { "path", strainProjPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to save strain_r5.sciprj");
        CHECK_COND(rpcCall(4557, "app.open_project", QJsonObject{ { "path", strainProjPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to reload strain_r5.sciprj");
        QJsonObject rInfoStrainReload = rpcCall(4558, "app.window_info", pInfoStrain).value("result").toObject();
        const QJsonObject l0Strain = rInfoStrainReload.value("layers").toArray().at(0).toObject();
        CHECK_COND(l0Strain.value("curve_count").toInt(0) == 1
                           && std::fabs(l0Strain.value("curves").toArray().at(0).toObject()
                                                .value("y_max").toDouble() - 999.0) < 1e-6,
                   "StrainGraphR5 lost its curve or y_max on .sciprj reload after renaming CT_Strain_Raw!");

        // (4d) Table1 -> ExpA coexistence with Table10 & Table1_Raw (2D + Graph3D + Legend)
        rpcCall(4560, "app.new_project", QJsonObject());
        Table *t1 = mw->newTable(4, 3, "Table1", "T1");
        Table *t10 = mw->newTable(4, 3, "Table10", "T10");
        Table *t1Raw = mw->newTable(4, 3, "Table1_Raw", "T1Raw");
        CHECK_COND(t1 && t10 && t1Raw, "Failed to create Table1/Table10/Table1_Raw");
        for (Table *tb : { t1, t10, t1Raw }) {
            tb->setColName(0, "X");
            tb->setColName(1, "Y");
            tb->setColName(2, "Z");
            tb->setColPlotDesignation(0, SciDAVis::X);
            tb->setColPlotDesignation(1, SciDAVis::Y);
            tb->setColPlotDesignation(2, SciDAVis::Z);
        }
        for (int r = 0; r < 4; ++r) {
            t1->setCell(r, 0, double(r));    t1->setCell(r, 1, 10.0 + r);    t1->setCell(r, 2, 10.0 + r);
            t10->setCell(r, 0, double(r));   t10->setCell(r, 1, 100.0 + r);  t10->setCell(r, 2, 100.0 + r);
            t1Raw->setCell(r, 0, double(r)); t1Raw->setCell(r, 1, 1000.0 + r); t1Raw->setCell(r, 2, 1000.0 + r);
        }
        MultiLayer *mlCoexT = mw->multilayerPlot(t1, QStringList{ "Table1_Y" }, Graph::LineSymbols, 0, 3);
        CHECK_COND(mlCoexT && mlCoexT->activeGraph(), "Failed to create mlCoexT");
        mw->renameWindow(mlCoexT, "CoexTblGraph");
        Graph *gCoexT = mlCoexT->activeGraph();
        gCoexT->insertCurve(t10, "Table10_X", "Table10_Y", Graph::LineSymbols, 0, 3);
        gCoexT->insertCurve(t1Raw, "Table1_Raw_X", "Table1_Raw_Y", Graph::LineSymbols, 0, 3);
        gCoexT->reconstructLegend();
        CHECK_COND(gCoexT->legend() != nullptr, "gCoexT missing legend");
        gCoexT->legend()->setText("\\c{1}Table1_Y\n\\c{2}Table10_Y\n\\c{3}Table1_Raw_Y\n");

        Graph3D *g3dT1 = mw->dataPlotXYZ(t1, "Table1_Z", Graph3D::Scatter);
        Graph3D *g3dT10 = mw->dataPlotXYZ(t10, "Table10_Z", Graph3D::Scatter);
        CHECK_COND(g3dT1 != nullptr && g3dT10 != nullptr, "Failed to create Graph3D for Table1/Table10");

        CHECK_COND(rpcCall(4561, "app.rename_window", QJsonObject{ { "name", "Table1" }, { "new_name", "ExpA" } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to rename Table1 -> ExpA");

        const QString legTxtT = gCoexT->legend()->text();
        CHECK_COND(gCoexT->curve(0)->title().text() == "ExpA_Y"
                           && gCoexT->curve(1)->title().text() == "Table10_Y"
                           && gCoexT->curve(2)->title().text() == "Table1_Raw_Y",
                   "Renaming Table1 -> ExpA corrupted Table10_Y or Table1_Raw_Y curve titles!");
        CHECK_COND(legTxtT.contains("ExpA_Y") && legTxtT.contains("Table10_Y")
                           && legTxtT.contains("Table1_Raw_Y") && !legTxtT.contains("ExpA0_Y")
                           && !legTxtT.contains("ExpA_Raw_Y"),
                   "Renaming Table1 -> ExpA corrupted Legend text for Table10_Y or Table1_Raw_Y!");

        const QString coexTPath = tmpR5.filePath("coex_tbl_r5.sciprj");
        CHECK_COND(rpcCall(4562, "app.save_project", QJsonObject{ { "path", coexTPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to save coex_tbl_r5.sciprj");
        CHECK_COND(rpcCall(4563, "app.open_project", QJsonObject{ { "path", coexTPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to reload coex_tbl_r5.sciprj");
        const QJsonArray winsAfterCoexT = rpcCall(4564, "app.list_windows", QJsonObject{ { "type", "Graph3D" } })
                                                  .value("result").toObject().value("windows").toArray();
        CHECK_COND(winsAfterCoexT.size() == 2,
                   "Table1 -> ExpA rename destroyed Table10's Graph3D on .sciprj reload!");

        // (4e) Matrix1 -> MatA coexistence with Matrix10 (Spectrogram-only 2D graph + Graph3D)
        rpcCall(4570, "app.new_project", QJsonObject());
        Matrix *m1 = mw->newMatrix("Matrix1", 4, 4);
        Matrix *m10 = mw->newMatrix("Matrix10", 4, 4);
        CHECK_COND(m1 && m10, "Failed to create Matrix1/Matrix10");
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                m1->setCell(r, c, double(r + c + 1));
                m10->setCell(r, c, double(10 * (r + c + 1)));
            }
        }
        QJsonObject rHm1 = rpcCall(4571, "plot.matrix", QJsonObject{ { "matrix", "Matrix1" }, { "style", "colormap" }, { "graph_name", "HMap1" } }).value("result").toObject();
        QJsonObject rHm10 = rpcCall(4572, "plot.matrix", QJsonObject{ { "matrix", "Matrix10" }, { "style", "colormap" }, { "graph_name", "HMap10" } }).value("result").toObject();
        QJsonObject rM3d1 = rpcCall(4573, "plot.3d_matrix", QJsonObject{ { "matrix", "Matrix1" }, { "style", "surface" } }).value("result").toObject();
        QJsonObject rM3d10 = rpcCall(4574, "plot.3d_matrix", QJsonObject{ { "matrix", "Matrix10" }, { "style", "surface" } }).value("result").toObject();
        CHECK_COND(rHm1.value("ok").toBool() && rHm10.value("ok").toBool()
                           && rM3d1.value("ok").toBool() && rM3d10.value("ok").toBool(),
                   "Failed to create 2D/3D plots for Matrix1 and Matrix10");

        CHECK_COND(rpcCall(4575, "app.rename_window", QJsonObject{ { "name", "Matrix1" }, { "new_name", "MatA" } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to rename Matrix1 -> MatA");

        const QString coexMPath = tmpR5.filePath("coex_mtx_r5.sciprj");
        CHECK_COND(rpcCall(4576, "app.save_project", QJsonObject{ { "path", coexMPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to save coex_mtx_r5.sciprj");
        CHECK_COND(rpcCall(4577, "app.open_project", QJsonObject{ { "path", coexMPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to reload coex_mtx_r5.sciprj");
        const QJsonArray winsAfterCoexM = rpcCall(4578, "app.list_windows", QJsonObject{ { "type", "Graph3D" } })
                                                  .value("result").toObject().value("windows").toArray();
        QJsonObject hm1Reload = rpcCall(4579, "app.window_info", QJsonObject{ { "name", "HMap1" } }).value("result").toObject();
        QJsonObject hm10Reload = rpcCall(4580, "app.window_info", QJsonObject{ { "name", "HMap10" } }).value("result").toObject();
        CHECK_COND(winsAfterCoexM.size() == 2
                           && hm1Reload.value("layers").toArray().at(0).toObject()
                                      .value("spectrograms").toArray().at(0).toObject().value("matrix").toString() == "MatA"
                           && hm10Reload.value("layers").toArray().at(0).toObject()
                                      .value("spectrograms").toArray().at(0).toObject().value("matrix").toString() == "Matrix10",
                   "Matrix1 -> MatA rename corrupted Spectrogram-only HMap1/HMap10 or Matrix10 Graph3D!");

        // (4f) Column rename V -> V_new coexistence with V10 (2D + Legend + Graph3D + .sciprj reload)
        rpcCall(4585, "app.new_project", QJsonObject());
        Table *tColRen = mw->newTable(4, 4, "ColRenTbl", "Column Rename Coexistence");
        CHECK_COND(tColRen != nullptr, "Failed to create ColRenTbl");
        tColRen->setColName(0, "X");
        tColRen->setColName(1, "V");
        tColRen->setColName(2, "V10");
        tColRen->setColName(3, "Z");
        tColRen->setColPlotDesignation(0, SciDAVis::X);
        tColRen->setColPlotDesignation(1, SciDAVis::Y);
        tColRen->setColPlotDesignation(2, SciDAVis::Y);
        tColRen->setColPlotDesignation(3, SciDAVis::Z);
        for (int r = 0; r < 4; ++r) {
            tColRen->setCell(r, 0, double(r));
            tColRen->setCell(r, 1, 2.0 * r + 1.0);
            tColRen->setCell(r, 2, 20.0 * r + 10.0);
            tColRen->setCell(r, 3, 5.0 * r + 3.0);
        }
        CHECK_COND(rpcCall(4586, "plot.xy", QJsonObject{
                           { "table", "ColRenTbl" },
                           { "x_col", "X" },
                           { "y_cols", QJsonArray{ "V", "V10" } },
                           { "graph_name", "ColRen2D" } }).value("result").toObject().value("ok").toBool(),
                   "Failed to create ColRen2D");
        CHECK_COND(rpcCall(4587, "plot.3d_xyz", QJsonObject{
                           { "table", "ColRenTbl" },
                           { "x_col", "X" },
                           { "y_col", "V10" },
                           { "z_col", "Z" } }).value("result").toObject().value("ok").toBool(),
                   "Failed to create 3D XYZ plot for V10");

        CHECK_COND(rpcCall(4588, "table.set_column_meta", QJsonObject{
                           { "table", "ColRenTbl" },
                           { "col", "V" },
                           { "name", "V_new" } }).value("result").toObject().value("ok").toBool(),
                   "Failed to rename column V -> V_new");

        QJsonObject colRenInfo = rpcCall(4589, "app.window_info", QJsonObject{ { "name", "ColRen2D" } }).value("result").toObject();
        const QJsonObject crLayer = colRenInfo.value("layers").toArray().at(0).toObject();
        const QJsonArray crCurves = crLayer.value("curves").toArray();
        CHECK_COND(crCurves.size() == 2
                           && crCurves.at(0).toObject().value("title").toString() == "ColRenTbl_V_new"
                           && crCurves.at(1).toObject().value("title").toString() == "ColRenTbl_V10"
                           && crLayer.value("legend_text").toString().contains("\\c{1}V_new")
                           && crLayer.value("legend_text").toString().contains("\\c{2}V10")
                           && !crLayer.value("legend_text").toString().contains("V_new10"),
                   "Renaming column V -> V_new corrupted V10 curve title or legend!");

        const QString colRenPath = tmpR5.filePath("col_ren_r5.sciprj");
        CHECK_COND(rpcCall(4590, "app.save_project", QJsonObject{ { "path", colRenPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to save col_ren_r5.sciprj");
        CHECK_COND(rpcCall(4591, "app.open_project", QJsonObject{ { "path", colRenPath } })
                           .value("result").toObject().value("ok").toBool(),
                   "Failed to reload col_ren_r5.sciprj");
        const QJsonArray winsAfterColRen = rpcCall(4592, "app.list_windows", QJsonObject{ { "type", "Graph3D" } })
                                                   .value("result").toObject().value("windows").toArray();
        CHECK_COND(winsAfterColRen.size() == 1,
                   "Column rename V -> V_new destroyed V10's Graph3D on .sciprj reload!");
    }

    rpcCall(4599, "app.new_project", QJsonObject());
    mw->hide();
    QApplication::processEvents();
    std::cout << "[INFO] Suite 45 Round 5 Zero-Defect Gate passed!\n";
    return 0;
}

#include <objidl.h>
#include <gdiplus.h>
#include <qwt_symbol.h>

int runSuite46OleAntialiasingAndSubpixelSmoothnessGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 46: OLE EMF+ Dual Anti-Aliasing & Sub-Pixel Curve Smoothness Gate...\n";
    CHECK_COND(SciDAVisEmf::ensureGdiplusInitialized(), "ensureGdiplusInitialized failed in Suite 46");

    const int nPts = 500;
    Table *tbl = mw->newTable(nPts, 3, "S46SmoothTbl", "OLE Anti-Aliased Sub-Pixel Smoothness");
    CHECK_COND(tbl != nullptr, "Failed to create S46SmoothTbl");
    tbl->setColName(0, "X");
    tbl->setColName(1, "YBlue");
    tbl->setColName(2, "YRed");
    tbl->setColPlotDesignation(0, SciDAVis::X);
    tbl->setColPlotDesignation(1, SciDAVis::Y);
    tbl->setColPlotDesignation(2, SciDAVis::Y);

    for (int i = 0; i < nPts; ++i) {
        const double x = 10.0 * double(i) / double(nPts - 1);
        tbl->setCell(i, 0, x);
        // Gentle monotonic slope in lower half of [0, 10] canvas (exposes 96-DPI qRound staircases at 4x zoom)
        tbl->setCell(i, 1, 2.0 + 0.18 * x);
        // Sinusoidal curve in upper half of [0, 10] canvas (never intersects YBlue)
        tbl->setCell(i, 2, 7.2 + 0.9 * std::sin(x));
    }

    MultiLayer *ml = mw->multilayerPlot(tbl, QStringList{ "S46SmoothTbl_YBlue" }, Graph::Line, 0, nPts - 1);
    CHECK_COND(ml != nullptr && ml->activeGraph() != nullptr, "Failed to create S46 MultiLayer");
    mw->renameWindow(ml, "S46SmoothGraph");
    ml->setPageSizePhysical(85.0, 85.0, MultiLayer::UnitMm, 300);

    Graph *g = ml->activeGraph();
    // Add sparse 25-point LineSymbols curve in upper half to verify symbol AA + floating-point symbol placement
    Table *tblSym = mw->newTable(25, 2, "S46SymTbl", "OLE Symbol AA");
    CHECK_COND(tblSym != nullptr, "Failed to create S46SymTbl");
    tblSym->setColName(0, "X");
    tblSym->setColName(1, "YSym");
    tblSym->setColPlotDesignation(0, SciDAVis::X);
    tblSym->setColPlotDesignation(1, SciDAVis::Y);
    for (int i = 0; i < 25; ++i) {
        const double x = 0.4 + 9.2 * double(i) / 24.0;
        tblSym->setCell(i, 0, x);
        tblSym->setCell(i, 1, 7.2 + 0.9 * std::sin(x));
    }
    g->insertCurve(tblSym, "S46SymTbl_X", "S46SymTbl_YSym", Graph::LineSymbols, 0, 24);
    CHECK_COND(g->curves() == 2, "Expected 2 curves in S46SmoothGraph");

    // Verify Graph::setAntialiasing propagates to pre-existing curves even when update == false
    g->setAntialiasing(false, false);
    CHECK_COND(!g->curve(0)->testRenderHint(QwtPlotItem::RenderAntialiased)
                       && !g->curve(1)->testRenderHint(QwtPlotItem::RenderAntialiased),
               "Graph::setAntialiasing(false, false) failed to propagate RenderAntialiased=false to existing curves!");
    g->setAntialiasing(true, false);
    CHECK_COND(g->curve(0)->testRenderHint(QwtPlotItem::RenderAntialiased)
                       && g->curve(1)->testRenderHint(QwtPlotItem::RenderAntialiased),
               "Graph::setAntialiasing(true, false) failed to propagate RenderAntialiased=true to existing curves!");

    // Style curve 0: pure saturated blue (0, 90, 230), width 2, no symbols
    QwtPlotCurve *cBlue = g->curve(0);
    QPen bluePen(QColor(0, 90, 230));
    bluePen.setWidthF(2.0);
    cBlue->setPen(bluePen);

    // Style curve 1: pure saturated red (220, 20, 20), width 1.5, Ellipse symbols
    QwtPlotCurve *cRed = g->curve(1);
    QPen redPen(QColor(220, 20, 20));
    redPen.setWidthF(1.5);
    cRed->setPen(redPen);
    cRed->setSymbol(QwtSymbol(QwtSymbol::Ellipse, QBrush(QColor(220, 20, 20)),
                              QPen(QColor(220, 20, 20), 1), QSize(9, 9)));

    g->removeLegend();
    g->setScale(QwtPlot::xBottom, 0.0, 10.0);
    g->setScale(QwtPlot::yLeft, 0.0, 10.0);
    g->replot();
    QApplication::processEvents();

    // 1. Render OLE payload (EMF+ Dual + 300-DPI CF_DIB)
    OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml);
    CHECK_COND(!rg.emfBytes.isEmpty() && !rg.dibBytes.isEmpty(), "Suite 46 renderMultiLayer returned empty bytes");
    CHECK_COND(rg.extentHm.cx == 8500 && rg.extentHm.cy == 8500,
               "Suite 46 extentHm expected 8500x8500 (85x85 mm)");

    HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(rg.emfBytes);
    CHECK_COND(hemf != nullptr, "Suite 46 createHemfFromBytes failed");

    ENHMETAHEADER emh{};
    emh.nSize = sizeof(emh);
    CHECK_COND(GetEnhMetaFileHeader(hemf, sizeof(emh), &emh) > 0, "Suite 46 GetEnhMetaFileHeader failed");
    CHECK_COND(emh.rclFrame.left == 0 && emh.rclFrame.top == 0
                       && std::abs(emh.rclFrame.right - 8500) <= 2
                       && std::abs(emh.rclFrame.bottom - 8500) <= 2,
               "Suite 46 EMF rclFrame does not match 85x85 mm (8500x8500 0.01mm)!");

    EmfRecordStats recStats{};
    EnumEnhMetaFile(nullptr, hemf, enumEmfProc, &recStats, nullptr);
    CHECK_COND(recStats.vectorPathOrLineRecords >= 10 && recStats.textRecords >= 2,
               "Suite 46 EMF+ Dual lost down-level vector or text records!");

    // Helper to allocate a 1200x1200 top-down 32-bpp DIB (~3.74x zoom over 85mm @ 96 DPI)
    const int zoomW = 1200;
    const int zoomH = 1200;
    auto inspectPlayback = [&](bool useOleViewObjectDraw, bool testMagentaBleed, const char *modeTag) -> int {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = zoomW;
        bmi.bmiHeader.biHeight = -zoomH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void *bits = nullptr;
        HDC screenDc = GetDC(nullptr);
        HDC memDc = CreateCompatibleDC(screenDc);
        HBITMAP hbm = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, screenDc);
        CHECK_COND(memDc != nullptr && hbm != nullptr && bits != nullptr, "Failed to allocate 1200x1200 DIB");
        HGDIOBJ oldBm = SelectObject(memDc, hbm);

        auto *px = static_cast<quint32 *>(bits);
        const quint32 initColor = testMagentaBleed ? 0xFFFF00FFu : 0xFFFFFFFFu;
        for (int i = 0; i < zoomW * zoomH; ++i)
            px[i] = initColor;

        if (useOleViewObjectDraw) {
            SciDAVisOle::OleGraphPayload payload;
            payload.targetWindow = ml->name();
            payload.targetLayer = 0;
            payload.extentHm = rg.extentHm;
            payload.pixelSize = rg.pixelSize;
            payload.emfBytes = rg.emfBytes;
            payload.dibBytes = rg.dibBytes;

            ComPtr<IViewObject2> viewObj;
            HRESULT hr = SciDAVisOle::createGraphObject(payload, IID_IViewObject2,
                                                        reinterpret_cast<void **>(viewObj.GetAddressOf()));
            if (FAILED(hr) || !viewObj) {
                SelectObject(memDc, oldBm);
                DeleteObject(hbm);
                DeleteDC(memDc);
                std::cerr << "[FAIL] createGraphObject(IViewObject2) failed in Suite 46\n";
                return 1;
            }
            RECTL rcBounds{ 0, 0, zoomW, zoomH };
            hr = viewObj->Draw(DVASPECT_CONTENT, -1, nullptr, nullptr, nullptr, memDc, &rcBounds, nullptr, nullptr, 0);
            if (FAILED(hr)) {
                SelectObject(memDc, oldBm);
                DeleteObject(hbm);
                DeleteDC(memDc);
                std::cerr << "[FAIL] IViewObject2::Draw failed in Suite 46\n";
                return 1;
            }
        } else {
            // Deliberately do NOT call SetSmoothingMode on host Graphics: proves the EMF+ Dual stream
            // itself embeds EmfPlusSetAntiAliasMode so PowerPoint/Word render smooth curves automatically!
            Gdiplus::Metafile mf(hemf, FALSE);
            Gdiplus::Graphics gfx(memDc);
            Gdiplus::Status st = gfx.DrawImage(&mf, 0, 0, zoomW, zoomH);
            if (st != Gdiplus::Ok) {
                SelectObject(memDc, oldBm);
                DeleteObject(hbm);
                DeleteDC(memDc);
                std::cerr << "[FAIL] Gdiplus::Graphics::DrawImage(Metafile) failed in Suite 46\n";
                return 1;
            }
        }
        GdiFlush();

        if (testMagentaBleed) {
            int magentaLeft = 0;
            for (int i = 0; i < zoomW * zoomH; ++i) {
                const quint32 c = px[i] & 0x00FFFFFFu;
                if (c == 0x00FF00FFu)
                    ++magentaLeft;
            }
            SelectObject(memDc, oldBm);
            DeleteObject(hbm);
            DeleteDC(memDc);
            if (magentaLeft != 0) {
                std::cerr << "[FAIL] Suite 46 (" << modeTag << ") left " << magentaLeft
                          << " uncovered magenta border pixels!\n";
                return 1;
            }
            return 0;
        }

        int blueAaPixels = 0;
        int redAaPixels = 0;
        int crispBlackFramePixels = 0;
        QVector<int> blueCols;
        QVector<double> blueCentroids(zoomW, 0.0);

        for (int x = 0; x < zoomW; ++x) {
            double wSum = 0.0;
            double wySum = 0.0;
            for (int y = 0; y < zoomH; ++y) {
                const quint32 argb = px[y * zoomW + x];
                const int r = (argb >> 16) & 0xFF;
                const int gr = (argb >> 8) & 0xFF;
                const int b = argb & 0xFF;

                if (r == 0 && gr == 0 && b == 0)
                    ++crispBlackFramePixels;

                // Pure blue (0, 90, 230) over white (255, 255, 255): B - R == 230 * coverage, 0 on any gray/black/white!
                const int blueDiff = b - r;
                if (blueDiff >= 20 && blueDiff <= 205)
                    ++blueAaPixels;
                if (y > zoomH / 2 && blueDiff >= 12) {
                    wSum += double(blueDiff);
                    wySum += double(y) * double(blueDiff);
                }

                // Pure red (220, 20, 20) over white (255, 255, 255): R - B == 200 * coverage
                const int redDiff = r - b;
                if (y < (zoomH * 3) / 5 && redDiff >= 20 && redDiff <= 185)
                    ++redAaPixels;
            }
            if (wSum >= 180.0) {
                blueCols.push_back(x);
                blueCentroids[x] = wySum / wSum;
            }
        }

        SelectObject(memDc, oldBm);
        DeleteObject(hbm);
        DeleteDC(memDc);

        if (blueAaPixels < 400 || redAaPixels < 300 || crispBlackFramePixels < 1000 || blueCols.size() < 400) {
            std::cerr << "[FAIL] Suite 46 (" << modeTag << ") AA/frame check failed: blueAA=" << blueAaPixels
                      << " redAA=" << redAaPixels << " blackFrame=" << crispBlackFramePixels
                      << " blueCols=" << blueCols.size() << "\n";
            return 1;
        }

        // Sub-pixel centroid progression across the middle 200 columns of the plot canvas at ~3.74x zoom:
        // Old 96-DPI integer quantization produced ~18-column flat plateaus (4-col advance == 0.0)
        // punctuated by ~3.74-px jumps. True floating-point PointF vectors advance smoothly every column!
        const int midStartIdx = blueCols.size() / 2 - 100;
        double maxAbsStep1 = 0.0;
        double minAdvance4 = 1e9;
        double maxAdvance4 = 0.0;
        for (int k = 0; k < 196; ++k) {
            const int x0 = blueCols[midStartIdx + k];
            const int x1 = blueCols[midStartIdx + k + 1];
            const int x4 = blueCols[midStartIdx + k + 4];
            if (x1 != x0 + 1 || x4 != x0 + 4) {
                std::cerr << "[FAIL] Suite 46 (" << modeTag << ") non-contiguous blue curve columns at x=" << x0 << "\n";
                return 1;
            }
            const double step1 = std::fabs(blueCentroids[x1] - blueCentroids[x0]);
            const double adv4 = blueCentroids[x0] - blueCentroids[x4]; // positive since screen Y decreases upward
            maxAbsStep1 = std::max(maxAbsStep1, step1);
            minAdvance4 = std::min(minAdvance4, adv4);
            maxAdvance4 = std::max(maxAdvance4, adv4);
        }

        std::cout << "[INFO] Suite 46 (" << modeTag << "): blueAA=" << blueAaPixels
                  << ", redSymAA=" << redAaPixels
                  << ", maxStep1=" << maxAbsStep1
                  << ", adv4=[" << minAdvance4 << ", " << maxAdvance4 << "]\n";

        if (maxAbsStep1 > 0.65 || minAdvance4 < 0.25 || maxAdvance4 > 1.50) {
            std::cerr << "[FAIL] Suite 46 (" << modeTag << ") sub-pixel curve smoothness failed (staircase detected): "
                      << "maxStep1=" << maxAbsStep1 << " minAdv4=" << minAdvance4 << " maxAdv4=" << maxAdvance4 << "\n";
            return 1;
        }
        return 0;
    };

    CHECK_COND(inspectPlayback(false, true, "GDI+ Metafile Magenta Sentinel") == 0,
               "GDI+ Metafile playback left uncovered border pixels");
    CHECK_COND(inspectPlayback(true, true, "IViewObject2::Draw Magenta Sentinel") == 0,
               "IViewObject2::Draw left uncovered border pixels");
    CHECK_COND(inspectPlayback(false, false, "GDI+ Default Metafile Playback @ 1200x1200") == 0,
               "Self-contained EMF+ Dual anti-aliasing / sub-pixel smoothness check failed");
    CHECK_COND(inspectPlayback(true, false, "OLE IViewObject2::Draw @ 1200x1200") == 0,
               "OLE IViewObject2::Draw anti-aliasing / sub-pixel smoothness check failed");

    // 2. Verify CF_DIB fallback is 300-DPI and anti-aliased
    CHECK_COND(size_t(rg.dibBytes.size()) > sizeof(BITMAPINFOHEADER), "Suite 46 dibBytes too small");
    BITMAPINFOHEADER dibHdr{};
    std::memcpy(&dibHdr, rg.dibBytes.constData(), sizeof(dibHdr));
    const int dibW = dibHdr.biWidth;
    const int dibH = std::abs(dibHdr.biHeight);
    CHECK_COND(dibW >= 900 && dibH >= 900 && dibHdr.biXPelsPerMeter >= 11000,
               "Suite 46 CF_DIB fallback is not 300-DPI (~1004x1004, 11811 ppm)!");
    const auto *dibPx = reinterpret_cast<const uchar *>(rg.dibBytes.constData() + sizeof(BITMAPINFOHEADER));
    const int dibStride = ((dibW * (dibHdr.biBitCount / 8)) + 3) & ~3;
    const int bpp = dibHdr.biBitCount / 8;
    int dibBlueAa = 0;
    if (bpp >= 3 && int(sizeof(BITMAPINFOHEADER)) + dibStride * dibH <= rg.dibBytes.size()) {
        for (int y = 0; y < dibH; ++y) {
            const uchar *row = dibPx + y * dibStride;
            for (int x = 0; x < dibW; ++x) {
                const int b = row[x * bpp + 0];
                const int r = row[x * bpp + 2];
                const int diff = b - r;
                if (diff >= 20 && diff <= 205)
                    ++dibBlueAa;
            }
        }
    }
    CHECK_COND(dibBlueAa >= 400, "Suite 46 300-DPI CF_DIB fallback lacks anti-aliased curve edges!");

    DeleteEnhMetaFile(hemf);

    ml->askOnCloseEvent(false);
    ml->close();
    tblSym->askOnCloseEvent(false);
    tblSym->close();
    tbl->askOnCloseEvent(false);
    tbl->close();
    mw->savedProject();
    QApplication::processEvents();

    std::cout << "[INFO] Suite 46 OLE EMF+ Dual Anti-Aliasing & Sub-Pixel Curve Smoothness Gate passed!\n";
    return 0;
}

