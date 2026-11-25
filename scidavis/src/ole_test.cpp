#include "ole_test_common.h"

struct ModularSuiteEntry {
    int id;
    const char *name;
    int (*run)(ApplicationWindow *);
};

static const ModularSuiteEntry kModularSuites[] = {
    { 36, "runSuite36Round3SurgicalZero", &runSuite36Round3SurgicalZero },
    { 37, "runSuite37DuotoneIconsAndTheme", &runSuite37DuotoneIconsAndTheme },
    { 38, "runSuite38RibbonSplitPaletteAndFocusHud", &runSuite38RibbonSplitPaletteAndFocusHud },
    { 39, "runSuite39GitHubAutoUpdaterEngine", &runSuite39GitHubAutoUpdaterEngine },
    { 40, "runSuite40PythonJsonRpcServerDispatch", &runSuite40PythonJsonRpcServerDispatch },
    { 41, "runSuite41Phase6JapaneseZeroMissAndArialGate", &runSuite41Phase6JapaneseZeroMissAndArialGate },
    { 42, "runSuite42Round2CriticRegressionGate", &runSuite42Round2CriticRegressionGate },
    { 43, "runSuite43Round3CriticRegressionGate", &runSuite43Round3CriticRegressionGate },
    { 44, "runSuite44Round4UnifiedRegressionGate", &runSuite44Round4UnifiedRegressionGate },
    { 45, "runSuite45Round5ZeroDefectGate", &runSuite45Round5ZeroDefectGate },
    { 46, "runSuite46OleAntialiasingAndSubpixelSmoothnessGate", &runSuite46OleAntialiasingAndSubpixelSmoothnessGate },
    { 47, "runSuite47Phase7GraphGuiUsabilityAndZeroLeakGate", &runSuite47Phase7GraphGuiUsabilityAndZeroLeakGate },
    { 48, "runSuite48Phase8RibbonDockWorksheetAndGraphErgonomicsGate", &runSuite48Phase8RibbonDockWorksheetAndGraphErgonomicsGate },
    { 49, "runSuite49PhaseBUxRenderMatSciAndApiGate", &runSuite49PhaseBUxRenderMatSciAndApiGate },
    { 50, "runSuite50MegaWave1PlotsMatSciAutonomyUndoPerf", &runSuite50MegaWave1PlotsMatSciAutonomyUndoPerf },
    { 51, "runSuite51Campaign6FinalEndToEndInvariants", &runSuite51Campaign6FinalEndToEndInvariants },
    { 52, "runSuite52Phase12OpjuApiGraphMathAndGuiGate", &runSuite52Phase12OpjuApiGraphMathAndGuiGate },
    { 53, "runSuite53SaveSafetyUndoAndRecovery", &runSuite53SaveSafetyUndoAndRecovery },
    { 55, "runSuite55TensileAndWorkHardeningCorrectness", &runSuite55TensileAndWorkHardeningCorrectness },
    { 56, "runSuite56FitConvergenceAndStatistics", &runSuite56FitConvergenceAndStatistics },
    { 57, "runSuite57NumericalRobustnessXrdWeibullKissinger", &runSuite57NumericalRobustnessXrdWeibullKissinger },
    { 58, "runSuite58FigureDefaultsPresetsAndExport", &runSuite58FigureDefaultsPresetsAndExport },
    { 59, "runSuite59DataImportAndWorksheet", &runSuite59DataImportAndWorksheet },
    { 60, "runSuite60PowerPointOleLink", &runSuite60PowerPointOleLink },
    { 61, "runSuite61DistributionVersionAndUpdaterSafety", &runSuite61DistributionVersionAndUpdaterSafety },
    { 62, "runSuite62BeginnerNavigationMenusAndScreenFit", &runSuite62BeginnerNavigationMenusAndScreenFit },
    { 63, "runSuite63JapaneseUiAccessibilityGate", &runSuite63JapaneseUiAccessibilityGate },
    { 64, "runSuite64XlsxImport", &runSuite64XlsxImport },
    { 65, "runSuite65E11ExtraGraphTypes", &runSuite65E11ExtraGraphTypes },
    { 66, "runSuite66Workbooks", &runSuite66Workbooks },
    { 67, "runSuite67Density2DAndPoleFigure", &runSuite67Density2DAndPoleFigure },
    { 68, "runSuite68RibbonFoldingRecentModelsAndMiniToolbarKeys", &runSuite68RibbonFoldingRecentModelsAndMiniToolbarKeys },
    { 69, "runSuite69SettingsTransferAndScriptCoverage", &runSuite69SettingsTransferAndScriptCoverage },
    { 70, "runSuite70IconCleanupAndLightTheme", &runSuite70IconCleanupAndLightTheme },
    { 71, "runSuite71LabSlideTextAndZoomFollowsText", &runSuite71LabSlideTextAndZoomFollowsText },
    { 72, "runSuite72ToriGraphStandardDefaultPalette", &runSuite72ToriGraphStandardDefaultPalette },
    { 73, "runSuite73SendToPowerPoint", &runSuite73SendToPowerPoint },
    { 74, "runSuite74LegacyUiHidden", &runSuite74LegacyUiHidden },
    { 75, "runSuite75CopyAndExportKeepGraphState", &runSuite75CopyAndExportKeepGraphState },
    { 76, "runSuite76ReviewPolishAndEdgeCases", &runSuite76ReviewPolishAndEdgeCases },
    { 77, "runSuite77Nov22NightReviewRegressions", &runSuite77Nov22NightReviewRegressions },
    { 78, "runSuite78ZeroBaseUxAndQualityGate", &runSuite78ZeroBaseUxAndQualityGate },
    { 79, "runSuite79ZeroBaseOverhaulV2Gate", &runSuite79ZeroBaseOverhaulV2Gate },
    { 80, "runSuite80Round4P0P1RegressionGate", &runSuite80Round4P0P1RegressionGate },
    { 81, "runSuite81NumericalTransformCorrectness", &runSuite81NumericalTransformCorrectness },
    { 82, "runSuite82JsonRpcParamValidation", &runSuite82JsonRpcParamValidation },
    { 83, "runSuite83ImportRobustness", &runSuite83ImportRobustness },
    { 84, "runSuite84ProjectSerializationFidelity", &runSuite84ProjectSerializationFidelity },
    { 85, "runSuite85TableMatrixCoreIntegrity", &runSuite85TableMatrixCoreIntegrity },
    { 86, "runSuite86AxisScaleCorrectness", &runSuite86AxisScaleCorrectness },
    { 87, "runSuite87Graph3DMatrixOrientation", &runSuite87Graph3DMatrixOrientation },
    { 88, "runSuite88ExportAndFftSampling", &runSuite88ExportAndFftSampling },
    { 89, "runSuite89CurveDataMapping", &runSuite89CurveDataMapping },
    { 90, "runSuite90FftWindowTeardown", &runSuite90FftWindowTeardown },
    { 91, "runSuite91GraphCopyStackTransforms", &runSuite91GraphCopyStackTransforms },
};

static const ModularSuiteEntry *findModularSuite(int id)
{
    for (const auto &entry : kModularSuites) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

static int runAllTests(int argc, char **argv, QTemporaryDir &runTempDir);

int main(int argc, char **argv)
{
    std::cout << std::unitbuf << "[INFO] Starting scidavis_ole_test...\n";
    std::cerr << std::unitbuf;
    // Every run gets a scratch folder of its own and uses it as %TEMP% / %TMP%: the suites (and
    // the product code they drive) write files with fixed names to QDir::tempPath(), which
    // collided when two runs - or another ToriGraph - were active at the same time (one run
    // deleting or overwriting the other's project file). The folder is removed at the end unless
    // the run failed or TORIGRAPH_TEST_KEEP_TEMP=1 is set.
    QTemporaryDir runTempDir(QDir::tempPath() + QStringLiteral("/torigraph_ole_test_XXXXXX"));
    if (runTempDir.isValid()) {
        const std::wstring wTemp = QDir::toNativeSeparators(runTempDir.path()).toStdWString();
        _wputenv_s(L"TMP", wTemp.c_str());
        _wputenv_s(L"TEMP", wTemp.c_str());
        SetEnvironmentVariableW(L"TMP", wTemp.c_str());
        SetEnvironmentVariableW(L"TEMP", wTemp.c_str());
    } else {
        std::cerr << "[WARN] could not create a scratch folder of this run ("
                  << runTempDir.errorString().toStdString() << "); the shared %TEMP% is used\n";
    }

    int rc = runAllTests(argc, argv, runTempDir);

    if (g_modalWatchdogDismissals > 0) {
        std::cerr << "[FAIL] the modal watchdog had to close " << g_modalWatchdogDismissals
                  << " dialog(s) that nobody answered (see the [WARN] modal watchdog lines)"
                  << std::endl;
        rc = rc != 0 ? rc : 1;
    }
    if (runTempDir.isValid()
        && (rc != 0 || qEnvironmentVariableIntValue("TORIGRAPH_TEST_KEEP_TEMP") != 0)) {
        runTempDir.setAutoRemove(false);
        std::cout << "[INFO] Scratch folder kept: "
                  << QDir::toNativeSeparators(runTempDir.path()).toStdString() << "\n";
    }
    return rc;
}

static int runAllTests(int argc, char **argv, QTemporaryDir &runTempDir)
{
    const QString testIniPath =
            QDir::tempPath()
            + QStringLiteral("/torigraph_ole_test_settings_%1.ini")
                      .arg(QCoreApplication::applicationPid() ? QCoreApplication::applicationPid()
                                                              : qint64(GetCurrentProcessId()));
    QFile::remove(testIniPath);
    qputenv("TORIGRAPH_SETTINGS_INI", testIniPath.toLocal8Bit());
    if (qEnvironmentVariableIsEmpty("QT_FONT_DPI"))
        qputenv("QT_FONT_DPI", "96");
    if (qEnvironmentVariable("QT_OPENGL").compare(QStringLiteral("software"), Qt::CaseInsensitive) == 0)
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, true);
    else
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
    QApplication app(argc, argv);
    QStringList args = app.arguments();
    std::cout << "[INFO] Scratch folder of this run: "
              << QDir::toNativeSeparators(QDir::tempPath()).toStdString() << "\n";
    // Recovery copies (Suite 53) live in a folder of this run, not in the per-user folder that
    // every ToriGraph and every other test run share.
    ApplicationWindow::setRecoveryDirPathOverride(
            (runTempDir.isValid() ? runTempDir.path() : QDir::tempPath())
            + QStringLiteral("/recovery_%1").arg(QCoreApplication::applicationPid()));
    ModalWatchdog modalWatchdog;

    QString errMsg;
    // Never redirect a working registration (installed ToriGraph) to this test build; the
    // class factory registered below serves the in-process tests. TORIGRAPH_NO_OLE_REGISTER=1
    // skips registration entirely.
    CHECK_COND(SciDAVisOle::ensureRegistrationAtStartup(&errMsg),
               "ensureRegistrationAtStartup failed");
    CHECK_COND(SciDAVisOle::initializeOleServer(false), "initializeOleServer failed");

    ApplicationWindow *mw = new ApplicationWindow();
    mw->applyUserSettings();
    // Suites 1-53 were written for the legacy pixel page (window resize stretches the canvas,
    // 520 px pages, free aspect after unlocking, ...). Since E1 new graphs get the ISIJ 85 mm
    // preset right after creation, which fixes the page in mm. Keep the legacy behaviour for
    // those suites; Suite 58 sets the real default (isij_85mm) and checks it explicitly.
    MultiLayer::setNewGraphStylePresetId(QString());
    mw->saveSettings();
    if (args.contains("--list-suites")) {
        for (const auto &entry : kModularSuites) {
            std::cout << "Suite " << entry.id << ": " << entry.name << "\n";
        }
        mw->savedProject();
        mw->setOleEmbeddedMode(true);
        delete mw;
        SciDAVisOle::shutdownOleServer();
        return 0;
    }

    QList<int> requestedSuites;
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args[i];
        if (a.startsWith("--suite=")) {
            const QStringList parts = a.mid(8).split(',', Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                bool ok = false;
                const int sid = part.trimmed().toInt(&ok);
                if (!ok || !findModularSuite(sid)) {
                    std::cerr << "[FAIL] Unknown modular suite id in " << a.toStdString()
                              << " (use --list-suites to inspect valid IDs 36..91)\n";
                    mw->savedProject();
                    mw->setOleEmbeddedMode(true);
                    delete mw;
                    SciDAVisOle::shutdownOleServer();
                    return 2;
                }
                requestedSuites.append(sid);
            }
        } else if (a == "--suite" && i + 1 < args.size()) {
            const QStringList parts = args[++i].split(',', Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                bool ok = false;
                const int sid = part.trimmed().toInt(&ok);
                if (!ok || !findModularSuite(sid)) {
                    std::cerr << "[FAIL] Unknown modular suite id: " << part.toStdString() << "\n";
                    mw->savedProject();
                    mw->setOleEmbeddedMode(true);
                    delete mw;
                    SciDAVisOle::shutdownOleServer();
                    return 2;
                }
                requestedSuites.append(sid);
            }
        } else {
            QRegularExpression rxLegacy("^--suite(\\d+)-only$");
            QRegularExpressionMatch m = rxLegacy.match(a);
            if (m.hasMatch()) {
                const int sid = m.captured(1).toInt();
                if (!findModularSuite(sid)) {
                    std::cerr << "[FAIL] Unknown modular suite flag: " << a.toStdString() << "\n";
                    mw->savedProject();
                    mw->setOleEmbeddedMode(true);
                    delete mw;
                    SciDAVisOle::shutdownOleServer();
                    return 2;
                }
                requestedSuites.append(sid);
            }
        }
    }

    const bool legacyOnly = args.contains("--legacy-only");
    if (legacyOnly && !requestedSuites.isEmpty()) {
        std::cerr << "[FAIL] --legacy-only cannot be combined with --suite\n";
        mw->savedProject();
        mw->setOleEmbeddedMode(true);
        delete mw;
        SciDAVisOle::shutdownOleServer();
        return 2;
    }

    if (!requestedSuites.isEmpty()) {
        int rcSelected = 0;
        for (int sid : requestedSuites) {
            const ModularSuiteEntry *entry = findModularSuite(sid);
            if (!entry)
                continue;
            rcSelected = entry->run(mw);
            mw->savedProject();
            if (rcSelected != 0)
                break;
        }
        mw->setOleEmbeddedMode(true);
        delete mw;
        SciDAVisOle::shutdownOleServer();
        return rcSelected;
    }

    // =========================================================================
    // 1. OriginPro 2021 Reference Scatter Plot & Visual Parity
    // =========================================================================
    struct ScatterPt { double x; double y; int series; };
    const ScatterPt pts[] = {
        // H18 (series 1)
        { 194.0, 110.0, 1 }, { 202.0, 250.0, 1 }, { 204.0, 260.0, 1 },
        { 203.0, 315.0, 1 }, { 202.0, 830.0, 1 }, { 201.0, 1900.0, 1 },
        { 199.0, 4800.0, 1 }, { 191.0, 6500.0, 1 }, { 178.0, 17500.0, 1 },
        // 240°C-3h (series 2)
        { 112.0, 102.0, 2 }, { 119.0, 85.0, 2 }, { 122.0, 410.0, 2 },
        { 126.0, 540.0, 2 }, { 128.0, 1120.0, 2 }, { 131.0, 1480.0, 2 },
        { 133.0, 2550.0, 2 }, { 134.0, 3200.0, 2 }, { 141.0, 4700.0, 2 },
        { 148.0, 6800.0, 2 }, { 153.0, 10300.0, 2 }, { 158.0, 15800.0, 2 },
        { 160.0, 20500.0, 2 },
        // 260°C-3h (series 3)
        { 98.0, 245.0, 3 }, { 102.0, 178.0, 3 }, { 107.0, 290.0, 3 },
        { 110.0, 485.0, 3 }, { 115.0, 910.0, 3 }, { 119.0, 1380.0, 3 },
        { 124.0, 2580.0, 3 }, { 127.0, 2750.0, 3 }, { 138.0, 5350.0, 3 },
        { 145.0, 8100.0, 3 }, { 147.0, 10800.0, 3 }, { 149.0, 14500.0, 3 }
    };
    const int nScatterRows = int(sizeof(pts) / sizeof(pts[0]));
    Table *t = mw->newTable("TestTable", nScatterRows, 4);
    CHECK_COND(t != nullptr, "Failed to create Table");
    t->setColName(0, "stress");
    t->setColName(1, "H18");
    t->setColName(2, QString::fromUtf8("240\xC2\xB0" "C-3h"));
    t->setColName(3, QString::fromUtf8("260\xC2\xB0" "C-3h"));
    for (int r = 0; r < nScatterRows; ++r) {
        t->setCell(r, 0, pts[r].x);
        for (int c = 1; c <= 3; ++c) {
            if (pts[r].series == c)
                t->setCell(r, c, pts[r].y);
            else
                t->clearCell(r, c);
        }
    }

    QStringList cols;
    cols << (t->name() + "_H18")
         << (t->name() + "_" + QString::fromUtf8("240\xC2\xB0" "C-3h"))
         << (t->name() + "_" + QString::fromUtf8("260\xC2\xB0" "C-3h"));
    MultiLayer *ml = mw->multilayerPlot(t, cols, Graph::Scatter);
    CHECK_COND(ml != nullptr, "Failed to create MultiLayer plot");
    Graph *ag = ml->activeGraph();
    CHECK_COND(ag != nullptr, "Active Graph is null");
    ag->setXAxisTitle("Local flow stress, <i>&sigma;</i><sub>f</sub> / MPa");
    ag->setYAxisTitle("<i>N</i><sub>v</sub> / mm<sup>-3</sup>");
    ag->setScale(QwtPlot::xBottom, 50.0, 250.0, 50.0, 5, 1, 0, false);
    ag->setScale(QwtPlot::yLeft, 10.0, 100000.0, 0.0, 8, 8, 1, false);
    if (ag->legend()) {
        ag->legend()->setOriginCoord(162.0, 110.0);
    }

    // Verify 1:1 square plot canvas after non-square window resizes
    ml->show();
    QApplication::processEvents();
    ml->resize(800, 480);
    ml->canvasWidget()->resize(780, 420);
    QApplication::processEvents();
    QRect cRectWide = ag->plotWidget()->plotLayout()->canvasRect();
    std::cout << "[INFO] After wide resize (800x480): canvasRect=" << cRectWide.width() << "x"
              << cRectWide.height() << "\n";
    CHECK_COND(qAbs(cRectWide.width() - cRectWide.height()) <= 2,
               "Plot canvas did not preserve 1:1 aspect ratio on wide resize");

    ml->resize(480, 800);
    ml->canvasWidget()->resize(460, 740);
    QApplication::processEvents();
    QRect cRectTall = ag->plotWidget()->plotLayout()->canvasRect();
    std::cout << "[INFO] After tall resize (480x800): canvasRect=" << cRectTall.width() << "x"
              << cRectTall.height() << "\n";
    CHECK_COND(qAbs(cRectTall.width() - cRectTall.height()) <= 2,
               "Plot canvas did not preserve 1:1 aspect ratio on tall resize");

    ml->resize(520, 520);
    ml->canvasWidget()->resize(500, 460);
    QApplication::processEvents();
    ml->arrangeLayers(false, false);
    if (args.contains("--save-preview"))
        ml->exportImage(QCoreApplication::applicationDirPath() + "/style_preview.png", 100);

    // =========================================================================
    // 2. Test Mouse Wheel Zoom & Pan on Linear and Log10 Axes
    // =========================================================================
    Plot *plotW = ag->plotWidget();
    QWidget *canvasW = plotW->canvas();
    CHECK_COND(canvasW != nullptr, "Plot canvas is null");
    QPoint centerPt(canvasW->width() / 2, canvasW->height() / 2);

    double xMinBefore = plotW->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
    double xMaxBefore = plotW->axisScaleDiv(QwtPlot::xBottom)->upperBound();
    double yMinBefore = plotW->axisScaleDiv(QwtPlot::yLeft)->lowerBound();
    double yMaxBefore = plotW->axisScaleDiv(QwtPlot::yLeft)->upperBound();

    // Send wheel zoom-in event (delta = +120)
    QWheelEvent wheelIn(centerPt, canvasW->mapToGlobal(centerPt), 120, Qt::NoButton,
                        Qt::NoModifier, Qt::Vertical);
    QCoreApplication::sendEvent(canvasW, &wheelIn);

    double xMinZoom = plotW->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
    double xMaxZoom = plotW->axisScaleDiv(QwtPlot::xBottom)->upperBound();
    double yMinZoom = plotW->axisScaleDiv(QwtPlot::yLeft)->lowerBound();
    double yMaxZoom = plotW->axisScaleDiv(QwtPlot::yLeft)->upperBound();
    std::cout << "[INFO] After wheel zoom-in: X=[" << xMinZoom << ", " << xMaxZoom
              << "], Y(Log10)=[" << yMinZoom << ", " << yMaxZoom << "]\n";
    CHECK_COND((xMaxZoom - xMinZoom) < (xMaxBefore - xMinBefore),
               "Linear X axis range did not narrow on wheel zoom-in");
    CHECK_COND(yMinZoom > yMinBefore && yMaxZoom < yMaxBefore && yMinZoom > 0.0,
               "Log10 Y axis range did not narrow positively on wheel zoom-in");

    // Test Middle-Button Drag Pan on canvas
    QPoint dragStart = centerPt;
    QPoint dragEnd(centerPt.x() + 40, centerPt.y() - 30);
    QMouseEvent panPress(QEvent::MouseButtonPress, dragStart, Qt::MiddleButton,
                         Qt::MiddleButton, Qt::NoModifier);
    QMouseEvent panMove(QEvent::MouseMove, dragEnd, Qt::NoButton,
                        Qt::MiddleButton, Qt::NoModifier);
    QMouseEvent panRelease(QEvent::MouseButtonRelease, dragEnd, Qt::MiddleButton,
                           Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvasW, &panPress);
    QCoreApplication::sendEvent(canvasW, &panMove);
    QCoreApplication::sendEvent(canvasW, &panRelease);

    double xMinPan = plotW->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
    double yMinPan = plotW->axisScaleDiv(QwtPlot::yLeft)->lowerBound();
    std::cout << "[INFO] After middle-drag pan: xMin=" << xMinPan << ", yMin=" << yMinPan << "\n";
    CHECK_COND(std::fabs(xMinPan - xMinZoom) > 1e-3 && yMinPan > 0.0,
               "Middle-drag pan did not shift axis ranges or broke Log10 positivity");

    // Restore reference scales
    ag->setScale(QwtPlot::xBottom, 50.0, 250.0, 50.0, 5, 1, 0, false);
    ag->setScale(QwtPlot::yLeft, 10.0, 100000.0, 0.0, 8, 8, 1, false);
    plotW->replot();

    // =========================================================================
    // 3. Test Single-Peak & Multi-Peak Auto-Detection & Fitting
    // =========================================================================
    const int nPts = 201;
    Table *peakTable = mw->newTable("PeakData", nPts, 4);
    CHECK_COND(peakTable != nullptr, "Failed to create PeakData table");
    peakTable->setColName(0, "X");
    peakTable->setColName(1, "Gauss2");
    peakTable->setColName(2, "Lorentz3");
    peakTable->setColName(3, "PseudoVoigt2");

    // Ground truth 2-peak Gauss (Area parameterization):
    // y = y0 + sum A_i * sqrt(2/pi) / w_i * exp(-2 * ((x - xc_i) / w_i)^2)
    const double g_y0 = 5.0;
    const double g_xc[2] = { 30.0, 68.0 };
    const double g_w[2] = { 6.0, 8.0 };
    const double g_A[2] = { 250.0, 420.0 };

    // Ground truth 3-peak Lorentz (Area parameterization):
    // y = y0 + sum (2 * A_i / pi) * w_i / (4 * (x - xc_i)^2 + w_i^2)
    const double l_y0 = 2.0;
    const double l_xc[3] = { 20.0, 50.0, 80.0 };
    const double l_w[3] = { 4.5, 6.0, 5.0 };
    const double l_A[3] = { 180.0, 320.0, 240.0 };

    // Ground truth 2-peak Pseudo-Voigt (mu = 0.5, w_i = FWHM, A_i = Area):
    const double pv_y0 = 3.5;
    const double pv_mu = 0.5;
    const double pv_xc[2] = { 36.0, 56.0 };
    const double pv_w[2] = { 13.0, 15.0 };
    const double pv_A[2] = { 420.0, 620.0 };

    const double cGaussNorm = std::sqrt(4.0 * std::log(2.0) / M_PI);
    const double cGaussExp = 4.0 * std::log(2.0);

    for (int i = 0; i < nPts; ++i) {
        double x = 0.5 * i; // 0.0 .. 100.0
        double yg = g_y0;
        for (int k = 0; k < 2; ++k) {
            double z = (x - g_xc[k]) / g_w[k];
            yg += g_A[k] * std::sqrt(2.0 / M_PI) / g_w[k] * std::exp(-2.0 * z * z);
        }

        double yl = l_y0;
        for (int k = 0; k < 3; ++k) {
            double dx = x - l_xc[k];
            yl += (2.0 * l_A[k] / M_PI) * l_w[k] / (4.0 * dx * dx + l_w[k] * l_w[k]);
        }

        double ypv = pv_y0;
        for (int k = 0; k < 2; ++k) {
            double dx = x - pv_xc[k];
            double w = pv_w[k];
            double L = (2.0 / M_PI) * w / (4.0 * dx * dx + w * w);
            double G = (cGaussNorm / w) * std::exp(-cGaussExp * dx * dx / (w * w));
            ypv += pv_A[k] * (pv_mu * L + (1.0 - pv_mu) * G);
        }

        peakTable->setCell(i, 0, x);
        peakTable->setCell(i, 1, yg);
        peakTable->setCell(i, 2, yl);
        peakTable->setCell(i, 3, ypv);
    }

    // 3a. 2-Peak Gaussian Auto-Detection & Fit
    MultiLayer *mlGauss = mw->multilayerPlot(
            peakTable, QStringList() << (peakTable->name() + "_Gauss2"), Graph::Scatter);
    CHECK_COND(mlGauss != nullptr, "Failed to create Gauss2 plot");
    Graph *gGauss = mlGauss->activeGraph();
    MultiPeakFit *gaussFit = new MultiPeakFit(mw, gGauss, MultiPeakFit::Gauss, 2);
    gaussFit->enablePeakCurves(true);
    CHECK_COND(gaussFit->setDataFromCurve(peakTable->name() + "_Gauss2"),
               "gaussFit->setDataFromCurve failed");
    gaussFit->fit(); // Automatically invokes guessInitialValues() -> autoDetectPeaks()

    const std::vector<double> &gRes = gaussFit->results();
    CHECK_COND(gRes.size() == 7, "Expected 7 parameters for 2-peak Gauss fit");
    std::cout << "[INFO] 2-Peak Gauss Fit: R^2=" << gaussFit->rSquare()
              << ", AdjR^2=" << gaussFit->adjustedRSquare()
              << ", chi^2=" << gaussFit->chiSquare()
              << ", Peak1(A=" << gRes[0] << ", xc=" << gRes[1] << ", w=" << gRes[2] << ")"
              << ", Peak2(A=" << gRes[3] << ", xc=" << gRes[4] << ", w=" << gRes[5] << ")"
              << ", y0=" << gRes[6] << "\n";
    CHECK_COND(gaussFit->rSquare() > 0.999, "2-Peak Gauss R^2 < 0.999");
    CHECK_COND(gaussFit->adjustedRSquare() > 0.999, "2-Peak Gauss Adjusted R^2 < 0.999");
    CHECK_COND(relErrWithin(gRes[0], g_A[0]) && relErrWithin(gRes[1], g_xc[0])
                       && relErrWithin(gRes[2], g_w[0]),
               "2-Peak Gauss Peak 1 parameters exceeded 2% tolerance");
    CHECK_COND(relErrWithin(gRes[3], g_A[1]) && relErrWithin(gRes[4], g_xc[1])
                       && relErrWithin(gRes[5], g_w[1]) && relErrWithin(gRes[6], g_y0),
               "2-Peak Gauss Peak 2 / baseline parameters exceeded 2% tolerance");
    // Data curve + total fit curve + 2 individual peak curves = 4 curves
    CHECK_COND(gGauss->curves() >= 4, "Expected individual deconvoluted peak curves on Gauss graph");

    Table *gaussSummary = gaussFit->createPeakSummaryTable("GaussPeakSummary");
    CHECK_COND(gaussSummary != nullptr && gaussSummary->numRows() == 2,
               "createPeakSummaryTable failed for 2-Peak Gauss");
    CHECK_COND(relErrWithin(gaussSummary->cell(0, 1), g_xc[0])
                       && relErrWithin(gaussSummary->cell(1, 1), g_xc[1]),
               "GaussPeakSummary centers mismatch");
    CHECK_COND(relErrWithin(gaussSummary->cell(0, 4) + gaussSummary->cell(1, 4),
                            g_A[0] + g_A[1]),
               "GaussPeakSummary total area mismatch");
    CHECK_COND(std::fabs((gaussSummary->cell(0, 5) + gaussSummary->cell(1, 5)) - 100.0) < 0.1,
               "GaussPeakSummary AreaPercent did not sum to 100%");

    Table *gaussResiduals = gaussFit->createResidualsTable("GaussResiduals");
    CHECK_COND(gaussResiduals != nullptr && gaussResiduals->numRows() == nPts,
               "createResidualsTable failed for 2-Peak Gauss");
    CHECK_COND(std::fabs(gaussResiduals->cell(50, 3)) < 1e-2,
               "Residual value in GaussResiduals table is too large");
    delete gaussFit;

    // 3b. 3-Peak Lorentzian Auto-Count Detection & Fit
    MultiLayer *mlLorentz = mw->multilayerPlot(
            peakTable, QStringList() << (peakTable->name() + "_Lorentz3"), Graph::Scatter);
    CHECK_COND(mlLorentz != nullptr, "Failed to create Lorentz3 plot");
    Graph *gLorentz = mlLorentz->activeGraph();
    MultiPeakFit *lorentzFit = new MultiPeakFit(mw, gLorentz, MultiPeakFit::Lorentz, 1);
    lorentzFit->enablePeakCurves(true);
    CHECK_COND(lorentzFit->setDataFromCurve(peakTable->name() + "_Lorentz3"),
               "lorentzFit->setDataFromCurve failed");
    int detectedLorentzPeaks = lorentzFit->autoDetectAndUpdateNumPeaks(10, 0.08);
    std::cout << "[INFO] Auto-detected Lorentz peak count = " << detectedLorentzPeaks << "\n";
    CHECK_COND(detectedLorentzPeaks == 3, "Expected autoDetectAndUpdateNumPeaks to find 3 peaks");
    lorentzFit->fit();

    const std::vector<double> &lRes = lorentzFit->results();
    CHECK_COND(lRes.size() == 10, "Expected 10 parameters for 3-peak Lorentz fit");
    std::cout << "[INFO] 3-Peak Lorentz Fit: R^2=" << lorentzFit->rSquare()
              << ", Peak1(A=" << lRes[0] << ", xc=" << lRes[1] << ", w=" << lRes[2] << ")"
              << ", Peak2(A=" << lRes[3] << ", xc=" << lRes[4] << ", w=" << lRes[5] << ")"
              << ", Peak3(A=" << lRes[6] << ", xc=" << lRes[7] << ", w=" << lRes[8] << ")"
              << ", y0=" << lRes[9] << "\n";
    CHECK_COND(lorentzFit->rSquare() > 0.999, "3-Peak Lorentz R^2 < 0.999");
    for (int k = 0; k < 3; ++k) {
        CHECK_COND(relErrWithin(lRes[3 * k + 0], l_A[k])
                           && relErrWithin(lRes[3 * k + 1], l_xc[k])
                           && relErrWithin(lRes[3 * k + 2], l_w[k]),
                   "3-Peak Lorentz parameter exceeded 2% tolerance");
    }
    CHECK_COND(gLorentz->curves() >= 5,
               "Expected 3 deconvoluted peak curves + total fit curve on Lorentz graph");
    delete lorentzFit;

    // 3c. 2-Peak Pseudo-Voigt Auto-Detection & Fit
    MultiLayer *mlPV = mw->multilayerPlot(
            peakTable, QStringList() << (peakTable->name() + "_PseudoVoigt2"), Graph::Scatter);
    CHECK_COND(mlPV != nullptr, "Failed to create PseudoVoigt2 plot");
    Graph *gPV = mlPV->activeGraph();
    gPV->setXAxisTitle("Diffraction angle, 2<i>&theta;</i> / deg");
    gPV->setYAxisTitle("Intensity / a.u.");
    MultiPeakFit *pvFit = new MultiPeakFit(mw, gPV, MultiPeakFit::PseudoVoigt, 2);
    pvFit->setLorentzFraction(pv_mu);
    pvFit->enablePeakCurves(true);
    CHECK_COND(pvFit->setDataFromCurve(peakTable->name() + "_PseudoVoigt2"),
               "pvFit->setDataFromCurve failed");
    pvFit->fit();

    const std::vector<double> &pvRes = pvFit->results();
    CHECK_COND(pvRes.size() == 7, "Expected 7 parameters for 2-peak Pseudo-Voigt fit");
    std::cout << "[INFO] 2-Peak Pseudo-Voigt Fit: R^2=" << pvFit->rSquare()
              << ", Peak1(A=" << pvRes[0] << ", xc=" << pvRes[1] << ", w=" << pvRes[2] << ")"
              << ", Peak2(A=" << pvRes[3] << ", xc=" << pvRes[4] << ", w=" << pvRes[5] << ")"
              << ", y0=" << pvRes[6] << "\n";
    CHECK_COND(pvFit->rSquare() > 0.999, "2-Peak Pseudo-Voigt R^2 < 0.999");
    for (int k = 0; k < 2; ++k) {
        CHECK_COND(relErrWithin(pvRes[3 * k + 0], pv_A[k])
                           && relErrWithin(pvRes[3 * k + 1], pv_xc[k])
                           && relErrWithin(pvRes[3 * k + 2], pv_w[k]),
                   "2-Peak Pseudo-Voigt parameter exceeded 2% tolerance");
    }
    if (args.contains("--save-preview"))
        mlPV->exportImage(QCoreApplication::applicationDirPath() + "/peak_fit_preview.png", 100);
    delete pvFit;

    // =========================================================================
    // 4. Test OriginPro 2021 Worksheet (Table) Auto-Expanding Paste & Keys
    // =========================================================================
    Table *pasteTbl = mw->newTable("PasteTest", 2, 2);
    CHECK_COND(pasteTbl != nullptr, "Failed to create PasteTest table");
    pasteTbl->setCellsSelected(0, 0, 0, 0, true);
    // pasteSelection() only reads the clipboard and hands the text to pasteText(); passing the
    // text directly keeps this check independent of other programs using the clipboard (the
    // clipboard path itself is covered by Suite 59a).
    CHECK_COND(pasteTbl->pasteText("1.5\t1.25e-3\t10.0\n"
                                   "2.5\t--\t20.0\n"
                                   "3.5\t3.5E+04\tNaN\n"
                                   "4.5\t7.0\t40.0\n"),
               "Table::pasteText failed");

    std::cout << "[INFO] After 4x3 paste into 2x2 table: rows=" << pasteTbl->numRows()
              << ", cols=" << pasteTbl->numCols() << "\n";
    CHECK_COND(pasteTbl->numRows() == 4 && pasteTbl->numCols() == 3,
               "Table::pasteText did not auto-expand rows and columns to 4x3");
    CHECK_COND(relErrWithin(pasteTbl->cell(0, 1), 1.25e-3, 1e-6),
               "Scientific notation 1.25e-3 was not parsed accurately");
    CHECK_COND(relErrWithin(pasteTbl->cell(2, 1), 3.5e4, 1e-6),
               "Scientific notation 3.5E+04 was not parsed accurately");
    CHECK_COND(pasteTbl->column(1)->isInvalid(1) && pasteTbl->column(2)->isInvalid(2),
               "Missing tokens '--' and 'NaN' were not marked invalid/empty");

    // Test Delete key clearing selected cell without shrinking rows
    pasteTbl->deselectAll();
    pasteTbl->setCellsSelected(0, 0, 0, 0, true);
    QKeyEvent delKey(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(pasteTbl, &delKey);
    CHECK_COND(pasteTbl->column(0)->isInvalid(0) && pasteTbl->numRows() == 4,
               "Delete key did not clear cell(0,0) or unexpectedly altered row count");

    // Test Return/Enter key at bottom row auto-appending a new row
    pasteTbl->deselectAll();
    pasteTbl->setCellsSelected(3, 0, 3, 0, true);
    QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(pasteTbl, &enterKey);
    CHECK_COND(pasteTbl->numRows() == 5,
               "Pressing Return on bottom row did not auto-append row 5");

    // =========================================================================
    // 5. Test All OriginPro Peak Functions (Voigt, BiGaussian, EMG, LogNormal, DoniachSunjic)
    //    & Built-in Nonlinear Functions (Arrhenius, Boltzmann, Hollomon + guessBuiltInParameters)
    // =========================================================================
    Table *extPeakTable = mw->newTable("ExtPeakData", nPts, 6);
    CHECK_COND(extPeakTable != nullptr, "Failed to create ExtPeakData table");
    extPeakTable->setColName(0, "X");
    extPeakTable->setColName(1, "Voigt1");
    extPeakTable->setColName(2, "BiGauss1");
    extPeakTable->setColName(3, "EMG1");
    extPeakTable->setColName(4, "LogNorm1");
    extPeakTable->setColName(5, "Doniach1");

    const double v_y0 = 4.0, v_A = 350.0, v_xc = 45.0, v_w = 10.0, v_mu = 0.4;
    const double bg_y0 = 3.0, bg_A = 80.0, bg_xc = 40.0, bg_w = 14.0, bg_alpha = 1.5;
    const double emg_y0 = 2.5, emg_A = 400.0, emg_xc = 42.0, emg_w = 8.0, emg_tau = 1.2;
    const double ln_y0 = 2.0, ln_A = 90.0, ln_xc = 50.0, ln_w = 12.0, ln_asym = 1.5;
    const double ds_y0 = 3.0, ds_A = 75.0, ds_xc = 52.0, ds_w = 10.0, ds_alpha = 0.15;

    for (int i = 0; i < nPts; ++i) {
        double x = 0.5 * i; // 0.0 .. 100.0
        extPeakTable->setCell(i, 0, x);
        extPeakTable->setCell(i, 1,
                              v_y0 + eval_origin_peak_profile(MultiPeakFit::Voigt, v_A, v_xc, v_w,
                                                              x, v_mu, 2.0, 1.5, 1.2));
        extPeakTable->setCell(i, 2,
                              bg_y0 + eval_origin_peak_profile(MultiPeakFit::BiGaussian, bg_A, bg_xc,
                                                               bg_w, x, 0.5, 2.0, bg_alpha, 1.2));
        extPeakTable->setCell(i, 3,
                              emg_y0 + eval_origin_peak_profile(MultiPeakFit::EMG, emg_A, emg_xc,
                                                                emg_w, x, 0.5, 2.0, 1.5, emg_tau));
        extPeakTable->setCell(i, 4,
                              ln_y0 + eval_origin_peak_profile(MultiPeakFit::LogNormal, ln_A, ln_xc,
                                                               ln_w, x, 0.5, 2.0, ln_asym, 1.2));
        extPeakTable->setCell(i, 5,
                              ds_y0 + eval_origin_peak_profile(MultiPeakFit::DoniachSunjic, ds_A,
                                                               ds_xc, ds_w, x, 0.5, 2.0, ds_alpha,
                                                               1.2));
    }

    QStringList extCols;
    extCols << (extPeakTable->name() + "_Voigt1")
            << (extPeakTable->name() + "_BiGauss1")
            << (extPeakTable->name() + "_EMG1")
            << (extPeakTable->name() + "_LogNorm1")
            << (extPeakTable->name() + "_Doniach1");
    MultiLayer *mlExtPeak = mw->multilayerPlot(extPeakTable, extCols, Graph::Scatter);
    CHECK_COND(mlExtPeak != nullptr, "Failed to create ExtPeakData plot");
    Graph *gExtPeak = mlExtPeak->activeGraph();

    // 5a. Voigt Peak Fit
    {
        MultiPeakFit *fitV = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::Voigt, 1);
        fitV->setLorentzFraction(v_mu);
        CHECK_COND(fitV->setDataFromCurve(extPeakTable->name() + "_Voigt1"), "Voigt setDataFromCurve failed");
        fitV->fit();
        const auto &res = fitV->results();
        std::cout << "[INFO] Voigt Fit: R^2=" << fitV->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitV->rSquare() > 0.999 && relErrWithin(res[0], v_A) && relErrWithin(res[1], v_xc)
                           && relErrWithin(res[2], v_w) && relErrWithin(res[3], v_y0),
                   "Voigt peak fit failed accuracy check");
        delete fitV;
    }

    // 5b. BiGaussian Peak Fit
    {
        MultiPeakFit *fitBG = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::BiGaussian, 1);
        fitBG->setAsymmetryFactor(bg_alpha);
        CHECK_COND(fitBG->setDataFromCurve(extPeakTable->name() + "_BiGauss1"), "BiGaussian setDataFromCurve failed");
        fitBG->fit();
        const auto &res = fitBG->results();
        std::cout << "[INFO] BiGaussian Fit: R^2=" << fitBG->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitBG->rSquare() > 0.999 && relErrWithin(res[0], bg_A) && relErrWithin(res[1], bg_xc)
                           && relErrWithin(res[2], bg_w) && relErrWithin(res[3], bg_y0),
                   "BiGaussian peak fit failed accuracy check");
        delete fitBG;
    }

    // 5c. EMG Peak Fit
    {
        MultiPeakFit *fitEMG = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::EMG, 1);
        fitEMG->setTailFactor(emg_tau);
        CHECK_COND(fitEMG->setDataFromCurve(extPeakTable->name() + "_EMG1"), "EMG setDataFromCurve failed");
        fitEMG->fit();
        const auto &res = fitEMG->results();
        std::cout << "[INFO] EMG Fit: R^2=" << fitEMG->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitEMG->rSquare() > 0.999 && relErrWithin(res[0], emg_A, 0.03)
                           && relErrWithin(res[1], emg_xc, 0.03) && relErrWithin(res[2], emg_w, 0.03),
                   "EMG peak fit failed accuracy check");
        delete fitEMG;
    }

    // 5d. LogNormal Peak Fit
    {
        MultiPeakFit *fitLN = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::LogNormal, 1);
        fitLN->setAsymmetryFactor(ln_asym);
        CHECK_COND(fitLN->setDataFromCurve(extPeakTable->name() + "_LogNorm1"), "LogNormal setDataFromCurve failed");
        fitLN->fit();
        const auto &res = fitLN->results();
        std::cout << "[INFO] LogNormal Fit: R^2=" << fitLN->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitLN->rSquare() > 0.999 && relErrWithin(res[0], ln_A, 0.03)
                           && relErrWithin(res[1], ln_xc, 0.03) && relErrWithin(res[2], ln_w, 0.03),
                   "LogNormal peak fit failed accuracy check");
        delete fitLN;
    }

    // 5e. Doniach-Sunjic Peak Fit
    {
        MultiPeakFit *fitDS = new MultiPeakFit(mw, gExtPeak, MultiPeakFit::DoniachSunjic, 1);
        fitDS->setAsymmetryFactor(ds_alpha);
        CHECK_COND(fitDS->setDataFromCurve(extPeakTable->name() + "_Doniach1"), "DoniachSunjic setDataFromCurve failed");
        fitDS->fit();
        const auto &res = fitDS->results();
        std::cout << "[INFO] DoniachSunjic Fit: R^2=" << fitDS->rSquare()
                  << ", A=" << res[0] << ", xc=" << res[1] << ", w=" << res[2] << ", y0=" << res[3] << "\n";
        CHECK_COND(fitDS->rSquare() > 0.999 && relErrWithin(res[0], ds_A, 0.03)
                           && relErrWithin(res[1], ds_xc, 0.03) && relErrWithin(res[2], ds_w, 0.03),
                   "DoniachSunjic peak fit failed accuracy check");
        delete fitDS;
    }

    // 5f. Built-in Nonlinear Functions (Arrhenius, Boltzmann, Hollomon with guessBuiltInParameters)
    QStringList builtInModels = NonLinearFit::builtInModelNames();
    std::cout << "[INFO] Built-in Nonlinear Models count = " << builtInModels.size() << "\n";
    CHECK_COND(builtInModels.size() >= 50, "Expected at least 50 built-in OriginPro nonlinear models");

    const int nNlfPts = 31;
    Table *nlfTable = mw->newTable("NlfData", nNlfPts, 6);
    CHECK_COND(nlfTable != nullptr, "Failed to create NlfData table");
    nlfTable->setColName(0, "T_K");
    nlfTable->setColPlotDesignation(0, SciDAVis::X);
    nlfTable->setColName(1, "ArrheniusY");
    nlfTable->setColPlotDesignation(1, SciDAVis::Y);
    nlfTable->setColName(2, "BoltzX");
    nlfTable->setColPlotDesignation(2, SciDAVis::X);
    nlfTable->setColName(3, "BoltzmannY");
    nlfTable->setColPlotDesignation(3, SciDAVis::Y);
    nlfTable->setColName(4, "Strain");
    nlfTable->setColPlotDesignation(4, SciDAVis::X);
    nlfTable->setColName(5, "HollomonY");
    nlfTable->setColPlotDesignation(5, SciDAVis::Y);

    const double arr_A = 1.5e6, arr_Ea = 42000.0;
    const double blz_A1 = 10.0, blz_A2 = 95.0, blz_x0 = 48.0, blz_dx = 6.5;
    const double hol_K = 680.0, hol_n = 0.24;

    for (int i = 0; i < nNlfPts; ++i) {
        double T = 350.0 + 15.0 * i; // 350 .. 800 K
        double yArr = arr_A * std::exp(-arr_Ea / (8.3144626 * T));
        double xb = (100.0 * i) / (nNlfPts - 1);
        double yBlz = (blz_A1 - blz_A2) / (1.0 + std::exp((xb - blz_x0) / blz_dx)) + blz_A2;
        double eps = 0.02 + 0.012 * i;
        double yHol = hol_K * std::pow(eps, hol_n);

        nlfTable->setCell(i, 0, T);
        nlfTable->setCell(i, 1, yArr);
        nlfTable->setCell(i, 2, xb);
        nlfTable->setCell(i, 3, yBlz);
        nlfTable->setCell(i, 4, eps);
        nlfTable->setCell(i, 5, yHol);
    }

    MultiLayer *mlNlf = mw->multilayerPlot(nlfTable, QStringList() << (nlfTable->name() + "_ArrheniusY"), Graph::Scatter);
    Graph *gNlf = mlNlf->activeGraph();
    gNlf->insertCurve(nlfTable, nlfTable->name() + "_BoltzX", nlfTable->name() + "_BoltzmannY", Graph::Scatter);
    gNlf->insertCurve(nlfTable, nlfTable->name() + "_Strain", nlfTable->name() + "_HollomonY", Graph::Scatter);

    {
        NonLinearFit *nlfArr = new NonLinearFit(mw, gNlf);
        CHECK_COND(nlfArr->setBuiltInModel("Arrhenius"), "setBuiltInModel(Arrhenius) failed");
        CHECK_COND(nlfArr->setDataFromCurve(nlfTable->name() + "_ArrheniusY"), "Arrhenius setDataFromCurve failed");
        nlfArr->guessBuiltInParameters();
        nlfArr->fit();
        const auto &res = nlfArr->results();
        std::cout << "[INFO] Arrhenius NLFit: R^2=" << nlfArr->rSquare()
                  << ", A=" << res[0] << ", Ea=" << res[1] << "\n";
        CHECK_COND(nlfArr->rSquare() > 0.999 && relErrWithin(res[0], arr_A, 0.02)
                           && relErrWithin(res[1], arr_Ea, 0.02),
                   "Arrhenius nonlinear fit failed accuracy check");
        delete nlfArr;
    }

    {
        NonLinearFit *nlfBlz = new NonLinearFit(mw, gNlf);
        CHECK_COND(nlfBlz->setBuiltInModel("Boltzmann"), "setBuiltInModel(Boltzmann) failed");
        CHECK_COND(nlfBlz->setDataFromCurve(nlfTable->name() + "_BoltzmannY"), "Boltzmann setDataFromCurve failed");
        nlfBlz->guessBuiltInParameters();
        nlfBlz->fit();
        const auto &res = nlfBlz->results();
        std::cout << "[INFO] Boltzmann NLFit: R^2=" << nlfBlz->rSquare()
                  << ", A1=" << res[0] << ", A2=" << res[1] << ", x0=" << res[2] << ", dx=" << res[3] << "\n";
        CHECK_COND(nlfBlz->rSquare() > 0.999 && relErrWithin(res[0], blz_A1, 0.02)
                           && relErrWithin(res[1], blz_A2, 0.02) && relErrWithin(res[2], blz_x0, 0.02)
                           && relErrWithin(res[3], blz_dx, 0.02),
                   "Boltzmann nonlinear fit failed accuracy check");
        delete nlfBlz;
    }

    {
        NonLinearFit *nlfHol = new NonLinearFit(mw, gNlf);
        CHECK_COND(nlfHol->setBuiltInModel("Hollomon"), "setBuiltInModel(Hollomon) failed");
        CHECK_COND(nlfHol->setDataFromCurve(nlfTable->name() + "_HollomonY"), "Hollomon setDataFromCurve failed");
        nlfHol->guessBuiltInParameters();
        nlfHol->fit();
        const auto &res = nlfHol->results();
        std::cout << "[INFO] Hollomon NLFit: R^2=" << nlfHol->rSquare()
                  << ", K=" << res[0] << ", n=" << res[1] << "\n";
        CHECK_COND(nlfHol->rSquare() > 0.999 && relErrWithin(res[0], hol_K, 0.02)
                           && relErrWithin(res[1], hol_n, 0.02),
                   "Hollomon nonlinear fit failed accuracy check");
        delete nlfHol;
    }

    // =========================================================================
    // 6. Test Peak Analyzer (BaselineLinear + BaselineALS + Subtraction + Integration)
    // =========================================================================
    Table *baseTable = mw->newTable("BaselineData", nPts, 3);
    CHECK_COND(baseTable != nullptr, "Failed to create BaselineData table");
    baseTable->setColName(0, "X");
    baseTable->setColName(1, "LinBaseSpec");
    baseTable->setColName(2, "ALSBaseSpec");

    const double bPeak_xc[2] = { 32.0, 68.0 };
    const double bPeak_w[2] = { 5.0, 6.0 };
    const double bPeak_A[2] = { 220.0, 340.0 };

    for (int i = 0; i < nPts; ++i) {
        double x = 0.5 * i; // 0..100
        double peaksVal = 0.0;
        for (int k = 0; k < 2; ++k) {
            double z = (x - bPeak_xc[k]) / bPeak_w[k];
            peaksVal += bPeak_A[k] * std::sqrt(2.0 / M_PI) / bPeak_w[k] * std::exp(-2.0 * z * z);
        }
        double linBase = 15.0 + 0.35 * x;
        double alsBase = 12.0 + 16.0 * std::exp(-x / 40.0) + 0.08 * x;
        baseTable->setCell(i, 0, x);
        baseTable->setCell(i, 1, linBase + peaksVal);
        baseTable->setCell(i, 2, alsBase + peaksVal);
    }

    MultiLayer *mlBase = mw->multilayerPlot(
            baseTable,
            QStringList() << (baseTable->name() + "_LinBaseSpec") << (baseTable->name() + "_ALSBaseSpec"),
            Graph::Line);
    Graph *gBase = mlBase->activeGraph();

    // 6a. BaselineLinear + insertBaselineCurve + subtractBaselineFromData + integratePeaksToTable
    {
        MultiPeakFit *mpfLin = new MultiPeakFit(mw, gBase, MultiPeakFit::Gauss, 2);
        mpfLin->setBaselineMode(MultiPeakFit::BaselineLinear);
        CHECK_COND(mpfLin->baselineMode() == MultiPeakFit::BaselineLinear, "baselineMode() mismatch");
        CHECK_COND(mpfLin->setDataFromCurve(baseTable->name() + "_LinBaseSpec"), "LinBaseSpec setDataFromCurve failed");

        std::vector<double> compLin = mpfLin->computeBaseline();
        CHECK_COND(compLin.size() == size_t(nPts), "computeBaseline size mismatch");
        CHECK_COND(std::fabs(compLin[100] - (15.0 + 0.35 * 50.0)) < 1.0,
                   "BaselineLinear midpoint deviated from ground-truth linear background");

        PlotCurve *baseCurve = mpfLin->insertBaselineCurve();
        CHECK_COND(baseCurve != nullptr, "insertBaselineCurve returned nullptr");

        CHECK_COND(mpfLin->subtractBaselineFromData(), "subtractBaselineFromData failed");
        mpfLin->fit();
        const auto &res = mpfLin->results();
        std::cout << "[INFO] BaselineLinear Subtracted 2-Peak Fit: R^2=" << mpfLin->rSquare()
                  << ", A1=" << res[0] << ", xc1=" << res[1] << ", A2=" << res[3] << ", xc2=" << res[4]
                  << ", y0=" << res[6] << "\n";
        CHECK_COND(mpfLin->rSquare() > 0.998 && relErrWithin(res[0], bPeak_A[0], 0.03)
                           && relErrWithin(res[3], bPeak_A[1], 0.03) && std::fabs(res[6]) < 1.0,
                   "BaselineLinear subtracted fit failed accuracy check");

        Table *intTbl = mpfLin->integratePeaksToTable("PeakIntLinear");
        CHECK_COND(intTbl != nullptr && intTbl->numRows() == 2 && intTbl->numCols() == 7,
                   "integratePeaksToTable failed");
        std::cout << "[INFO] Integrated Peak 1 Area=" << intTbl->cell(0, 4)
                  << ", Peak 2 Area=" << intTbl->cell(1, 4)
                  << ", Sum%=" << (intTbl->cell(0, 5) + intTbl->cell(1, 5)) << "\n";
        CHECK_COND(relErrWithin(intTbl->cell(0, 4), bPeak_A[0], 0.05)
                           && relErrWithin(intTbl->cell(1, 4), bPeak_A[1], 0.05)
                           && std::fabs((intTbl->cell(0, 5) + intTbl->cell(1, 5)) - 100.0) < 0.1,
                   "integratePeaksToTable areas or percentages out of tolerance");
        delete mpfLin;
    }

    // 6b. BaselineALS curved baseline subtraction & fit
    {
        MultiPeakFit *mpfALS = new MultiPeakFit(mw, gBase, MultiPeakFit::Gauss, 2);
        mpfALS->setBaselineMode(MultiPeakFit::BaselineALS);
        CHECK_COND(mpfALS->setDataFromCurve(baseTable->name() + "_ALSBaseSpec"), "ALSBaseSpec setDataFromCurve failed");
        std::vector<double> compALS = mpfALS->computeBaseline();
        std::cout << "[INFO] BaselineALS midpoint (x=50): computed=" << compALS[100]
                  << ", expected=" << (12.0 + 16.0 * std::exp(-50.0 / 40.0) + 4.0) << "\n";
        CHECK_COND(compALS.size() == size_t(nPts) && compALS[100] > 18.5 && compALS[100] < 23.5,
                   "BaselineALS did not track curved background envelope");
        CHECK_COND(mpfALS->subtractBaselineFromData(), "ALS subtractBaselineFromData failed");
        mpfALS->fit();
        std::cout << "[INFO] BaselineALS Subtracted 2-Peak Fit: R^2=" << mpfALS->rSquare() << "\n";
        CHECK_COND(mpfALS->rSquare() > 0.99, "BaselineALS subtracted fit R^2 < 0.99");
        delete mpfALS;
    }

    // =========================================================================
    // 7. Test OriginPro Plot Types (Y-Offset Stack, Violin+Jitter, Ternary, Pareto, Double-Y)
    // =========================================================================
    // 7a. Waterfall / Y-Offset Stack
    {
        QwtPlotCurve *c0 = gBase->curve(0);
        QwtPlotCurve *c1 = gBase->curve(1);
        CHECK_COND(c0 && c1, "Missing curves on gBase before applyYOffsetStack");
        double y1Before = c1->y(0);
        gBase->applyYOffsetStack(50.0, false);
        double y1After = c1->y(0);
        std::cout << "[INFO] Y-Offset Stack: curve1 y(0) before=" << y1Before << ", after=" << y1After << "\n";
        CHECK_COND(relErrWithin(y1After - y1Before, 50.0, 1e-4), "applyYOffsetStack did not offset curve 1 by 50.0");
    }

    // 7b. Violin + Jittered Data BoxCurve
    {
        MultiLayer *mlBox = mw->multilayerPlot(
                peakTable, QStringList() << (peakTable->name() + "_Gauss2"), Graph::Box);
        CHECK_COND(mlBox != nullptr && mlBox->activeGraph() != nullptr, "Failed to create Box plot");
        Graph *gBox = mlBox->activeGraph();
        BoxCurve *bc = dynamic_cast<BoxCurve *>(gBox->curve(0));
        CHECK_COND(bc != nullptr, "Curve 0 on Box plot is not a BoxCurve");
        bc->setViolinMode(true);
        bc->setShowDataPoints(true);
        CHECK_COND(bc->violinMode() && bc->showDataPoints(), "BoxCurve violinMode/showDataPoints getters failed");
        gBox->replot();
        QApplication::processEvents();
    }

    // 7c. Ternary Plot & ternaryToCartesian
    {
        QPointF pA = Graph::ternaryToCartesian(1.0, 0.0, 0.0);
        QPointF pB = Graph::ternaryToCartesian(0.0, 1.0, 0.0);
        QPointF pC = Graph::ternaryToCartesian(0.0, 0.0, 1.0);
        QPointF pCenter = Graph::ternaryToCartesian(1.0, 1.0, 1.0);
        CHECK_COND(std::fabs(pA.x() - 0.0) < 1e-6 && std::fabs(pA.y() - 0.0) < 1e-6, "ternaryToCartesian(A) mismatch");
        CHECK_COND(std::fabs(pB.x() - 1.0) < 1e-6 && std::fabs(pB.y() - 0.0) < 1e-6, "ternaryToCartesian(B) mismatch");
        CHECK_COND(std::fabs(pC.x() - 0.5) < 1e-6 && std::fabs(pC.y() - std::sqrt(3.0) * 0.5) < 1e-6,
                   "ternaryToCartesian(C) mismatch");
        CHECK_COND(std::fabs(pCenter.x() - 0.5) < 1e-6 && std::fabs(pCenter.y() - std::sqrt(3.0) / 6.0) < 1e-6,
                   "ternaryToCartesian(center) mismatch");

        Table *ternSrc = mw->newTable("TernaryInput", 4, 3);
        ternSrc->setColName(0, "Fe");
        ternSrc->setColName(1, "Cr");
        ternSrc->setColName(2, "Ni");
        const double ternRows[4][3] = {
            { 70.0, 18.0, 12.0 }, { 50.0, 25.0, 25.0 }, { 20.0, 50.0, 30.0 }, { 10.0, 20.0, 70.0 }
        };
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c)
                ternSrc->setCell(r, c, ternRows[r][c]);
        mw->activateSubWindow(ternSrc);
        mw->plotTernary();
        MultiLayer *mlTern = qobject_cast<MultiLayer *>(mw->windowsList().last());
        CHECK_COND(mlTern != nullptr && mlTern->activeGraph() != nullptr, "plotTernary did not create MultiLayer");
        CHECK_COND(!mlTern->activeGraph()->plotWidget()->axisEnabled(QwtPlot::xBottom),
                   "setupTernaryPlot did not hide rectangular xBottom axis");
    }

    // 7d. Pareto Chart (plotPareto)
    {
        Table *paretoSrc = mw->newTable("ParetoInput", 5, 2);
        paretoSrc->setColName(0, "DefectId");
        paretoSrc->setColName(1, "Count");
        const double counts[5] = { 15.0, 80.0, 35.0, 50.0, 20.0 };
        for (int r = 0; r < 5; ++r) {
            paretoSrc->setCell(r, 0, r + 1);
            paretoSrc->setCell(r, 1, counts[r]);
        }
        paretoSrc->deselectAll();
        paretoSrc->setCellsSelected(0, 1, 4, 1, true);
        mw->activateSubWindow(paretoSrc);
        mw->plotPareto();
        MultiLayer *mlPar = qobject_cast<MultiLayer *>(mw->windowsList().last());
        CHECK_COND(mlPar != nullptr && mlPar->activeGraph() != nullptr, "plotPareto did not create MultiLayer");
        Graph *gPar = mlPar->activeGraph();
        CHECK_COND(gPar->curves() >= 2, "Expected 2 curves (bars + cumulative %) on Pareto chart");
        CHECK_COND(gPar->curve(1)->yAxis() == QwtPlot::yRight
                           && gPar->plotWidget()->axisEnabled(QwtPlot::yRight),
                   "Pareto cumulative curve is not bound to enabled yRight axis");
        CHECK_COND(relErrWithin(gPar->curve(1)->y(4), 100.0, 1e-4),
                   "Pareto final cumulative percentage is not 100%");
    }

    // 7e. Double-Y Plot (plotDoubleY)
    {
        Table *dySrc = mw->newTable("DoubleYInput", 5, 3);
        dySrc->setColName(0, "Time");
        dySrc->setColPlotDesignation(0, SciDAVis::X);
        dySrc->setColName(1, "Temperature");
        dySrc->setColPlotDesignation(1, SciDAVis::Y);
        dySrc->setColName(2, "Pressure");
        dySrc->setColPlotDesignation(2, SciDAVis::Y);
        for (int r = 0; r < 5; ++r) {
            dySrc->setCell(r, 0, r * 10.0);
            dySrc->setCell(r, 1, 300.0 + r * 50.0);
            dySrc->setCell(r, 2, 1.0 + r * 2.5);
        }
        mw->activateSubWindow(dySrc);
        mw->plotDoubleY();
        MultiLayer *mlDY = qobject_cast<MultiLayer *>(mw->windowsList().last());
        CHECK_COND(mlDY != nullptr && mlDY->activeGraph() != nullptr, "plotDoubleY did not create MultiLayer");
        Graph *gDY = mlDY->activeGraph();
        CHECK_COND(gDY->curves() >= 2 && gDY->curve(0)->yAxis() == QwtPlot::yLeft
                           && gDY->curve(1)->yAxis() == QwtPlot::yRight,
                   "plotDoubleY did not bind curve 0 to yLeft and curve 1 to yRight");
    }

    // =========================================================================
    // 8. Test OriginPro Color Palettes (builtInPaletteNames, interpolatePalette, applyColorPaletteByName)
    // =========================================================================
    QStringList palNames = Graph::builtInPaletteNames();
    std::cout << "[INFO] Built-in Color Palettes count = " << palNames.size() << "\n";
    CHECK_COND(palNames.size() >= 10 && palNames.contains("Viridis")
                       && palNames.contains("Origin Classic"),
               "builtInPaletteNames missing required OriginPro presets");

    QList<QColor> anchors;
    anchors << QColor("#000000") << QColor("#ffffff");
    QList<QColor> interp = Graph::interpolatePalette(anchors, 5);
    CHECK_COND(interp.size() == 5 && interp.first() == QColor("#000000")
                       && interp.last() == QColor("#ffffff"),
               "Graph::interpolatePalette failed");

    ag->applyColorPaletteByName("Viridis", false);
    QList<QColor> viridisCols = Graph::paletteColors("Viridis");
    CHECK_COND(!viridisCols.isEmpty() && ag->curve(0)->pen().color() == viridisCols[0],
               "applyColorPaletteByName(Viridis) did not update curve 0 color");

    ag->applyColorPaletteByName("Origin Classic", false);
    QList<QColor> classicCols = Graph::paletteColors("Origin Classic");
    CHECK_COND(!classicCols.isEmpty() && ag->curve(0)->pen().color() == classicCols[0],
               "applyColorPaletteByName(Origin Classic) did not update curve 0 color");

    // =========================================================================
    // 9. Test Configurable Graph Aspect Ratio & X-Axis '1000' Label Clipping Fix
    // =========================================================================
    ml->resize(720, 540);
    ml->canvasWidget()->resize(700, 480);
    ml->applyAspectRatioPreset(4.0 / 3.0, true);
    QApplication::processEvents();
    QRect cRect43 = ag->plotWidget()->plotLayout()->canvasRect();
    double ratio43 = double(cRect43.width()) / double(qMax(1, cRect43.height()));
    std::cout << "[INFO] Aspect preset 4:3 -> canvasRect=" << cRect43.width() << "x"
              << cRect43.height() << " (ratio=" << ratio43 << ")\n";
    CHECK_COND(std::fabs(ratio43 - 4.0 / 3.0) < 0.05, "applyAspectRatioPreset(4:3) failed");

    ml->applyAspectRatioPreset(16.0 / 9.0, true);
    QApplication::processEvents();
    QRect cRect169 = ag->plotWidget()->plotLayout()->canvasRect();
    double ratio169 = double(cRect169.width()) / double(qMax(1, cRect169.height()));
    std::cout << "[INFO] Aspect preset 16:9 -> canvasRect=" << cRect169.width() << "x"
              << cRect169.height() << " (ratio=" << ratio169 << ")\n";
    CHECK_COND(std::fabs(ratio169 - 16.0 / 9.0) < 0.06, "applyAspectRatioPreset(16:9) failed");

    ml->setKeepAspectRatio(false);
    CHECK_COND(!ml->keepAspectRatio(), "setKeepAspectRatio(false) failed");
    ml->resize(820, 440);
    ml->canvasWidget()->resize(800, 380);
    ml->arrangeLayers(false, false);
    QApplication::processEvents();
    QRect cRectFree = ag->plotWidget()->plotLayout()->canvasRect();
    std::cout << "[INFO] Unlocked free resize (800x380) -> canvasRect=" << cRectFree.width() << "x"
              << cRectFree.height() << "\n";
    CHECK_COND(cRectFree.width() > cRectFree.height() + 100,
               "Unlocked keepAspectRatio(false) did not allow free wide canvas resize");

    // Restore 1:1 square aspect ratio and verify X-axis 0..1000 right margin >= 28 px
    ml->applyAspectRatioPreset(1.0, true);
    ml->resize(520, 520);
    ml->canvasWidget()->resize(500, 460);
    ag->setScale(QwtPlot::xBottom, 0.0, 1000.0, 200.0, 5, 1, 0, false);
    plotW->updateLayout();
    ml->arrangeLayers(false, false);
    QApplication::processEvents();
    int rightMargin1000 = plotW->width() - plotW->plotLayout()->canvasRect().right();
    std::cout << "[INFO] X-axis 0..1000 right margin (Plot::width - canvasRect.right) = "
              << rightMargin1000 << " px\n";
    CHECK_COND(rightMargin1000 >= 28,
               "Right margin on 0..1000 X-axis is < 28 px; rightmost label '1000' would clip!");
    ag->setScale(QwtPlot::xBottom, 50.0, 250.0, 50.0, 5, 1, 0, false);
    plotW->replot();

    // =========================================================================
    // 10. Test Workspace Management (Project Explorer Filtering & TabbedView Toggle)
    // =========================================================================
    mw->setTabbedWorkspaceView(true);
    CHECK_COND(mw->isTabbedWorkspaceView(), "setTabbedWorkspaceView(true) did not enable TabbedView");
    mw->setTabbedWorkspaceView(false);
    CHECK_COND(!mw->isTabbedWorkspaceView(), "setTabbedWorkspaceView(false) did not restore SubWindowView");

    mw->setExplorerSearchText("PeakData");
    mw->setExplorerTypeFilter(1); // Tables only
    mw->setExplorerTypeFilter(0); // All windows
    mw->setExplorerSearchText("");

    // =========================================================================
    // 11. Test Japanese / English GUI Language Switcher (日本語 / English)
    // =========================================================================
    mw->switchToLanguage("ja");
    QString jaFileMenu = QApplication::translate("ApplicationWindow", "&File");
    QString jaAnalysisMenu = QApplication::translate("ApplicationWindow", "&Analysis");
    std::cout << "[INFO] Switched to 'ja': appLanguage=" << mw->appLanguage.toStdString()
              << ", tr(&File)=" << jaFileMenu.toUtf8().constData()
              << ", tr(&Analysis)=" << jaAnalysisMenu.toUtf8().constData() << "\n";
    CHECK_COND(mw->appLanguage == "ja"
                       && jaFileMenu == QString::fromUtf8("ファイル(&F)")
                       && jaAnalysisMenu == QString::fromUtf8("解析(&A)"),
               "Japanese GUI translation was not activated by switchToLanguage(\"ja\")");

    mw->switchToLanguage("en");
    QString enFileMenu = QApplication::translate("ApplicationWindow", "&File");
    std::cout << "[INFO] Switched to 'en': appLanguage=" << mw->appLanguage.toStdString()
              << ", tr(&File)=" << enFileMenu.toStdString() << "\n";
    CHECK_COND(mw->appLanguage == "en" && enFileMenu == "&File",
               "English GUI strings were not restored by switchToLanguage(\"en\")");

    mw->savedProject();

    // =========================================================================
    // 12. Test EMF Vector Rendering & OLE Storage Round-Trip + Clipboard
    // =========================================================================
    OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, nullptr);
    CHECK_COND(!rg.emfBytes.isEmpty(), "EMF bytes are empty");
    CHECK_COND(!rg.dibBytes.isEmpty(), "DIB bytes are empty");
    CHECK_COND(rg.extentHm.cx > 1000 && rg.extentHm.cy > 1000, "Invalid HIMETRIC extent");

    HENHMETAFILE hemf = SciDAVisEmf::createHemfFromBytes(rg.emfBytes);
    CHECK_COND(hemf != nullptr, "createHemfFromBytes failed");
    ENHMETAHEADER emh{};
    emh.nSize = sizeof(emh);
    CHECK_COND(GetEnhMetaFileHeader(hemf, sizeof(emh), &emh) > 0, "GetEnhMetaFileHeader failed");

    EmfRecordStats stats{};
    EnumEnhMetaFile(nullptr, hemf, enumEmfProc, &stats, nullptr);
    DeleteEnhMetaFile(hemf);

    std::cout << "[INFO] EMF size=" << rg.emfBytes.size() << " bytes, totalRecords=" << stats.totalRecords
              << ", vectorRecords=" << stats.vectorPathOrLineRecords
              << ", textRecords=" << stats.textRecords
              << ", bitmapRecords=" << stats.bitmapRecords
              << ", extentHm=(" << rg.extentHm.cx << ", " << rg.extentHm.cy << ")\n";

    CHECK_COND(stats.totalRecords > 20, "Too few EMF records");
    CHECK_COND(stats.vectorPathOrLineRecords > 5, "Expected vector path/line records in EMF");
    CHECK_COND(stats.textRecords > 0, "Expected vector text records in EMF");

    SciDAVisOle::OleGraphPayload payload;
    payload.projectBytes = mw->saveProjectToByteArray();
    payload.targetWindow = ml->name();
    payload.targetLayer = 0;
    payload.emfBytes = rg.emfBytes;
    payload.dibBytes = rg.dibBytes;
    payload.extentHm = rg.extentHm;
    payload.pixelSize = rg.pixelSize;

    CHECK_COND(!payload.projectBytes.isEmpty(), "saveProjectToByteArray returned empty");

    ComPtr<IOleObject> oleObj;
    HRESULT hr = SciDAVisOle::createGraphObject(payload, IID_PPV_ARGS(&oleObj));
    CHECK_COND(SUCCEEDED(hr) && oleObj, "createGraphObject failed");

    ComPtr<IPersistStorage> persistStg;
    CHECK_COND(SUCCEEDED(oleObj.As(&persistStg)), "QueryInterface(IPersistStorage) failed");

    ComPtr<IViewObject2> viewObj;
    CHECK_COND(SUCCEEDED(oleObj.As(&viewObj)), "QueryInterface(IViewObject2) failed");

    SIZEL ext{};
    CHECK_COND(SUCCEEDED(viewObj->GetExtent(DVASPECT_CONTENT, -1, nullptr, &ext)), "IViewObject2::GetExtent failed");
    CHECK_COND(ext.cx == rg.extentHm.cx && ext.cy == rg.extentHm.cy, "Extent mismatch");

    // Save to compound file storage
    QString stgPath = QDir::tempPath() + "/scidavis_ole_test.stg";
    QFile::remove(stgPath);
    ComPtr<IStorage> stg;
    hr = StgCreateStorageEx( reinterpret_cast<const wchar_t *>(stgPath.utf16()),
                             STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                             STGFMT_STORAGE, 0, nullptr, nullptr, IID_PPV_ARGS(&stg) );
    CHECK_COND(SUCCEEDED(hr) && stg, "StgCreateStorageEx failed");

    CHECK_COND(SUCCEEDED(persistStg->Save(stg.Get(), FALSE)), "IPersistStorage::Save failed");
    CHECK_COND(SUCCEEDED(persistStg->SaveCompleted(stg.Get())), "IPersistStorage::SaveCompleted failed"); persistStg->HandsOffStorage();
    stg.Reset();

    // Reopen storage and load into a blank SciDAVisGraphObject
    hr = StgOpenStorageEx( reinterpret_cast<const wchar_t *>(stgPath.utf16()),
                           STGM_READ | STGM_SHARE_EXCLUSIVE,
                           STGFMT_STORAGE, 0, nullptr, nullptr, IID_PPV_ARGS(&stg) );
    CHECK_COND(SUCCEEDED(hr) && stg, "StgOpenStorageEx failed");

    SciDAVisOle::OleGraphPayload blankPayload;
    ComPtr<IPersistStorage> loadedPersist;
    CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(blankPayload, IID_PPV_ARGS(&loadedPersist))),
               "createGraphObject(blank) failed");
    CHECK_COND(SUCCEEDED(loadedPersist->Load(stg.Get())), "IPersistStorage::Load failed"); loadedPersist->HandsOffStorage();

    ComPtr<IDataObject> loadedData;
    CHECK_COND(SUCCEEDED(loadedPersist.As(&loadedData)), "QueryInterface(IDataObject) failed");

    FORMATETC fmtEmf{ CF_ENHMETAFILE, nullptr, DVASPECT_CONTENT, -1, TYMED_ENHMF };
    STGMEDIUM medEmf{};
    CHECK_COND(SUCCEEDED(loadedData->GetData(&fmtEmf, &medEmf)), "GetData(CF_ENHMETAFILE) after Load failed");
    CHECK_COND(medEmf.tymed == TYMED_ENHMF && medEmf.hEnhMetaFile != nullptr, "Invalid HENHMETAFILE from loaded object");
    ReleaseStgMedium(&medEmf);

    stg.Reset();
    QFile::remove(stgPath);

    // Test Clipboard Copy via MultiLayer::copyAllLayers(). Another program may replace the
    // clipboard between the copy and the checks; then the copy is simply repeated.
    {
        const CLIPFORMAT cfEmbed = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"Embed Source"));
        const CLIPFORMAT cfDesc = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"Object Descriptor"));
        FORMATETC fmtEmbed{ cfEmbed, nullptr, DVASPECT_CONTENT, -1, TYMED_ISTORAGE };
        FORMATETC fmtDesc{ cfDesc, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        FORMATETC fmtDib{ CF_DIB, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        const char *clipErr = retryClipboardStep(
                [&]() -> const char * {
                    ml->copyAllLayers();
                    markClipboardAfterCopy();
                    ComPtr<IDataObject> clipData;
                    const HRESULT hrClip = oleGetClipboardRetry(clipData);
                    if (FAILED(hrClip) || !clipData) {
                        std::cerr << "OleGetClipboard hr=0x" << std::hex << hrClip << std::dec << "\n";
                        return "OleGetClipboard failed";
                    }
                    if (clipData->QueryGetData(&fmtEmbed) != S_OK)
                        return "Clipboard missing Embed Source";
                    if (clipData->QueryGetData(&fmtDesc) != S_OK)
                        return "Clipboard missing Object Descriptor";
                    STGMEDIUM medDesc{};
                    HRESULT hrDesc = E_FAIL;
                    for (int k = 0; k < 40; ++k) {
                        QCoreApplication::processEvents();
                        hrDesc = clipData->GetData(&fmtDesc, &medDesc);
                        if (SUCCEEDED(hrDesc))
                            break;
                        Sleep(25);
                    }
                    if (FAILED(hrDesc)) {
                        std::cerr << "GetData(Object Descriptor) hr=0x" << std::hex << hrDesc
                                  << std::dec << "\n";
                        return "GetData(Object Descriptor) failed";
                    }
                    auto *objDesc = static_cast<OBJECTDESCRIPTOR *>(GlobalLock(medDesc.hGlobal));
                    const bool clsidOk = objDesc != nullptr && objDesc->clsid == SciDAVisOle::graph_clsid;
                    GlobalUnlock(medDesc.hGlobal);
                    ReleaseStgMedium(&medDesc);
                    if (!clsidOk)
                        return "Clipboard Object Descriptor CLSID mismatch!";
                    if (clipData->QueryGetData(&fmtEmf) != S_OK)
                        return "Clipboard missing CF_ENHMETAFILE";
                    if (clipData->QueryGetData(&fmtDib) != S_OK)
                        return "Clipboard missing CF_DIB";
                    return nullptr;
                },
                "Suite 1 copyAllLayers");
        CHECK_COND(clipErr == nullptr, (clipErr ? clipErr : ""));
    }

    // =========================================================================
    // 13. Interactive ROI Gadgets & Vertical Cursor (Domain 1)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 13: Interactive ROI Gadgets & Vertical Cursor...\n";
        const int nGad = 101;
        Table *gadTbl = mw->newTable("GadgetData", nGad, 3);
        CHECK_COND(gadTbl != nullptr, "Failed to create GadgetData table");
        gadTbl->setColName(0, "X");
        gadTbl->setColPlotDesignation(0, SciDAVis::X);
        gadTbl->setColName(1, "LinearRamp");
        gadTbl->setColPlotDesignation(1, SciDAVis::Y);
        gadTbl->setColName(2, "StepPulse");
        gadTbl->setColPlotDesignation(2, SciDAVis::Y);
        for (int r = 0; r < nGad; ++r) {
            double x = double(r);
            double yLin = 2.0 * x + 10.0; // exact line y = 2*x + 10
            double yStep = (x <= 50.0) ? 0.0 : ((x >= 70.0) ? 100.0 : 5.0 * (x - 50.0));
            gadTbl->setCell(r, 0, x);
            gadTbl->setCell(r, 1, yLin);
            gadTbl->setCell(r, 2, yStep);
        }
        QStringList gadCols;
        gadCols << (gadTbl->name() + "_LinearRamp") << (gadTbl->name() + "_StepPulse");
        MultiLayer *mlGad = mw->multilayerPlot(gadTbl, gadCols, Graph::LineSymbols);
        CHECK_COND(mlGad != nullptr && mlGad->activeGraph() != nullptr, "Failed to create Gadget plot");
        Graph *gGad = mlGad->activeGraph();
        gGad->setScale(QwtPlot::xBottom, 0.0, 100.0);
        gGad->setScale(QwtPlot::yLeft, -10.0, 220.0);
        mlGad->show();
        QApplication::processEvents();

        const QString cLinTitle = gadTbl->name() + "_LinearRamp";
        const QString cStepTitle = gadTbl->name() + "_StepPulse";

        // RegionStats on x in [10, 30] -> y = 30..70 (21 points, exact mean = 50.0)
        RoiGadgetTool *roi = RoiGadgetTool::attachGadget(mw, gGad, RoiGadgetTool::RegionStats);
        CHECK_COND(roi != nullptr && roi->rtti() == PlotToolInterface::RoiGadget,
                   "attachGadget(RegionStats) failed");
        roi->setTargetCurve(cLinTitle);
        roi->setRoiRect(QRectF(10.0, 0.0, 20.0, 120.0));
        std::cout << "[INFO] ROI RegionStats [10..30]: mean=" << roi->primaryMetric()
                  << ", sd=" << roi->secondaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 50.0, 1e-6),
                   "RoiGadgetTool(RegionStats) mean mismatch on [10, 30]");

        // Move ROI rectangle to [20, 40] -> y = 50..90 (exact mean = 70.0)
        roi->setRoiRect(QRectF(20.0, 0.0, 20.0, 120.0));
        std::cout << "[INFO] ROI RegionStats moved [20..40]: mean=" << roi->primaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 70.0, 1e-6),
                   "RoiGadgetTool(RegionStats) mean did not update after moving ROI");

        // QuickFit (FitLinear) on [10, 30] -> R^2 = 1.0, slope = 2.0
        roi->setRoiRect(QRectF(10.0, 0.0, 20.0, 120.0));
        roi->setGadgetMode(RoiGadgetTool::QuickFit);
        roi->setQuickFitModel(RoiGadgetTool::FitLinear);
        std::cout << "[INFO] ROI QuickFit(Linear): R^2=" << roi->primaryMetric()
                  << ", slope=" << roi->secondaryMetric() << "\n";
        CHECK_COND(roi->primaryMetric() > 0.999999 && relErrWithin(roi->secondaryMetric(), 2.0, 1e-5),
                   "RoiGadgetTool(QuickFit) R^2 or slope mismatch");

        // Integrate on [10, 30] -> integral_{10}^{30} (2x + 10) dx = 0.5*(30 + 70)*20 = 1000.0
        roi->setGadgetMode(RoiGadgetTool::Integrate);
        std::cout << "[INFO] ROI Integrate [10..30]: area=" << roi->primaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 1000.0, 1e-5),
                   "RoiGadgetTool(Integrate) trapezoidal area mismatch");

        // RiseTime on StepPulse (0 -> 100 ramp over x in [50, 70] -> 10%=52, 90%=68 -> riseTime = 16.0)
        roi->setTargetCurve(cStepTitle);
        roi->setRoiRect(QRectF(40.0, -5.0, 45.0, 115.0));
        roi->setGadgetMode(RoiGadgetTool::RiseTime);
        std::cout << "[INFO] ROI RiseTime [40..85]: 10-90% riseTime=" << roi->primaryMetric() << "\n";
        CHECK_COND(relErrWithin(roi->primaryMetric(), 16.0, 1e-4),
                   "RoiGadgetTool(RiseTime) 10-90% rise time mismatch");

        // Cluster on LinearRamp for x in [10, 20], y in [25, 55] -> 11 points (x=10..20, y=30..50), meanY = 40.0
        roi->setTargetCurve(cLinTitle);
        roi->setRoiRect(QRectF(10.0, 25.0, 10.0, 30.0));
        roi->setGadgetMode(RoiGadgetTool::Cluster);
        std::cout << "[INFO] ROI Cluster [10..20]: count=" << roi->primaryMetric()
                  << ", meanY=" << roi->secondaryMetric() << "\n";
        CHECK_COND(std::fabs(roi->primaryMetric() - 11.0) < 1e-6
                           && relErrWithin(roi->secondaryMetric(), 40.0, 1e-5),
                   "RoiGadgetTool(Cluster) count or meanY mismatch");

        Table *gadOut = roi->createOutputTable("RoiClusterSummary");
        CHECK_COND(gadOut != nullptr && gadOut->numRows() >= 1,
                   "RoiGadgetTool::createOutputTable failed");

        // VerticalCursorTool synchronized readout & exact linear interpolation
        VerticalCursorTool *vcur = VerticalCursorTool::attachCursor(mw, gGad, true);
        CHECK_COND(vcur != nullptr && vcur->rtti() == PlotToolInterface::VerticalCursor,
                   "VerticalCursorTool::attachCursor failed");
        vcur->setCursorX(60.5);
        QVector<QPair<QString, double>> ivals = vcur->interpolatedValues();
        std::cout << "[INFO] VerticalCursor at X=60.5: curves=" << ivals.size() << "\n";
        CHECK_COND(ivals.size() >= 2, "VerticalCursorTool::interpolatedValues missing curves");
        bool foundLin = false, foundStep = false;
        for (const auto &pr : ivals) {
            if (pr.first.contains("LinearRamp")) {
                foundLin = relErrWithin(pr.second, 2.0 * 60.5 + 10.0, 1e-6); // 131.0
            } else if (pr.first.contains("StepPulse")) {
                foundStep = relErrWithin(pr.second, 5.0 * (60.5 - 50.0), 1e-6); // 52.5
            }
        }
        CHECK_COND(foundLin && foundStep,
                   "VerticalCursorTool::interpolatedValues exact Y readout mismatch");
        RoiGadgetTool::clearGadget(gGad);
        CHECK_COND(gGad->activeTool() == nullptr, "RoiGadgetTool::clearGadget did not detach tool");
    }

    // =========================================================================
    // 14. Non-Destructive Data Point Masking (Domain 1 / Filter Integration)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 14: Non-Destructive Data Point Masking...\n";
        const int nMask = 21;
        Table *maskTbl = mw->newTable("MaskData", nMask, 2);
        maskTbl->setColName(0, "X");
        maskTbl->setColPlotDesignation(0, SciDAVis::X);
        maskTbl->setColName(1, "Y");
        maskTbl->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < nMask; ++r) {
            double x = double(r);
            maskTbl->setCell(r, 0, x);
            maskTbl->setCell(r, 1, 3.5 * x - 4.0);
        }
        // Inject extreme outlier y = +9999 at row 10 (x = 10)
        maskTbl->setCell(10, 1, 9999.0);

        MultiLayer *mlMask = mw->multilayerPlot(
                maskTbl, QStringList() << (maskTbl->name() + "_Y"), Graph::Scatter);
        CHECK_COND(mlMask != nullptr && mlMask->activeGraph() != nullptr,
                   "Failed to create MaskData graph");
        Graph *gMask = mlMask->activeGraph();
        DataCurve *dcMask = dynamic_cast<DataCurve *>(gMask->curve(0));
        CHECK_COND(dcMask != nullptr, "MaskData curve 0 is not a DataCurve");

        // Unmasked LinearFit should be severely distorted by +9999 outlier
        {
            LinearFit lfRaw(mw, gMask, dcMask->title().text());
            lfRaw.fit();
            std::cout << "[INFO] Unmasked outlier LinearFit: R^2=" << lfRaw.rSquare()
                      << ", slope=" << lfRaw.results()[1] << "\n";
            CHECK_COND(lfRaw.rSquare() < 0.25, "Outlier +9999 did not distort unmasked LinearFit");
        }

        // Mask the outlier non-destructively via DataCurve::maskPoint
        dcMask->maskPoint(10, true);
        CHECK_COND(dcMask->isPointMasked(10) && dcMask->maskedPointCount() == 1,
                   "DataCurve::maskPoint(10, true) did not mark point as masked");
        CHECK_COND(std::fabs(maskTbl->cell(10, 1) - 9999.0) < 1e-9,
                   "Masking destructively altered Table cell value instead of preserving 9999!");

        LinearFit lfMasked(mw, gMask, dcMask->title().text());
        lfMasked.fit();
        std::cout << "[INFO] Masked outlier LinearFit: R^2=" << lfMasked.rSquare()
                  << ", slope=" << lfMasked.results()[1]
                  << ", intercept=" << lfMasked.results()[0]
                  << ", tableCell(10,1)=" << maskTbl->cell(10, 1) << "\n";
        CHECK_COND(lfMasked.rSquare() > 0.999999
                           && relErrWithin(lfMasked.results()[1], 3.5, 1e-6)
                           && relErrWithin(lfMasked.results()[0], -4.0, 1e-6)
                           && std::fabs(maskTbl->cell(10, 1) - 9999.0) < 1e-9,
                   "LinearFit on masked curve failed to recover exact slope/R^2 while keeping cell=9999");

        dcMask->clearMasks();
        CHECK_COND(!dcMask->isPointMasked(10) && dcMask->maskedPointCount() == 0
                           && std::fabs(maskTbl->cell(10, 1) - 9999.0) < 1e-9,
                   "DataCurve::clearMasks() failed or modified underlying Table cell");
    }

    // =========================================================================
    // 15. Advanced Curve Fitting (Domain 2)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 15: Advanced Curve Fitting...\n";

        // 15a. Parameter Fixing & Box Bounds on NonLinearFit & MultiPeakFit
        {
            Table *conTbl = mw->newTable("ConstrainedFitData", 41, 2);
            conTbl->setColName(0, "X");
            conTbl->setColPlotDesignation(0, SciDAVis::X);
            conTbl->setColName(1, "Y");
            conTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 41; ++r) {
                double x = r * 1.5;
                // True unconstrained: y0 = 5, A = 100, t1 = 12
                conTbl->setCell(r, 0, x);
                conTbl->setCell(r, 1, 5.0 + 100.0 * std::exp(-x / 12.0));
            }
            MultiLayer *mlCon = mw->multilayerPlot(
                    conTbl, QStringList() << (conTbl->name() + "_Y"), Graph::Scatter);
            Graph *gCon = mlCon->activeGraph();
            const QString cTitle = conTbl->name() + "_Y";

            NonLinearFit nlf(mw, gCon, cTitle);
            nlf.setParametersList(QStringList() << "y0" << "A" << "t1");
            nlf.setFormula("y0 + A * exp(-x / t1)");
            nlf.setInitialGuess(0, 8.0);
            nlf.setInitialGuess(1, 95.0);
            nlf.setInitialGuess(2, 18.0);
            nlf.setParameterFixed(0, true);              // Fix y0 = 8.0
            nlf.setParameterBounds(2, 15.0, 25.0);       // Box-bound t1 in [15, 25]
            nlf.fit();
            std::cout << "[INFO] Constrained NonLinearFit: y0(fixed=8)=" << nlf.results()[0]
                      << ", A=" << nlf.results()[1] << ", t1(bounds 15..25)=" << nlf.results()[2] << "\n";
            CHECK_COND(nlf.isParameterFixed(0) && std::fabs(nlf.results()[0] - 8.0) < 1e-9,
                       "NonLinearFit::setParameterFixed did not hold y0 == 8.0");
            CHECK_COND(nlf.results()[2] >= 14.999999 && nlf.results()[2] <= 25.000001,
                       "NonLinearFit::setParameterBounds [15, 25] violated");

            // MultiPeakFit fixing & bounds (Reuse PeakData curve 0: Gauss peaks at xc1=30, xc2=68, y0=5)
            MultiLayer *mlMpfCon = mw->multilayerPlot(
                    peakTable, QStringList() << (peakTable->name() + "_Gauss2"), Graph::Scatter);
            Graph *gMpfCon = mlMpfCon->activeGraph();
            MultiPeakFit mpfCon(mw, gMpfCon, MultiPeakFit::Gauss, 2);
            CHECK_COND(mpfCon.setDataFromCurve(peakTable->name() + "_Gauss2"),
                       "mpfCon.setDataFromCurve failed");
            mpfCon.autoDetectPeaks(2);
            mpfCon.setInitialGuess(6, 7.5);              // y0 index = 3*2 = 6
            mpfCon.setParameterFixed(6, true);           // Fix y0 = 7.5
            mpfCon.setInitialGuess(1, 32.5);             // xc1 index = 1
            mpfCon.setParameterBounds(1, 31.5, 35.0);    // Bound xc1 >= 31.5 (true is 30.0)
            mpfCon.fit();
            std::cout << "[INFO] Constrained MultiPeakFit: y0(fixed=7.5)=" << mpfCon.results()[6]
                      << ", xc1(bounds 31.5..35)=" << mpfCon.results()[1] << "\n";
            CHECK_COND(std::fabs(mpfCon.results()[6] - 7.5) < 1e-8
                               && mpfCon.results()[1] >= 31.499999
                               && mpfCon.results()[1] <= 35.000001,
                       "MultiPeakFit parameter fixing or box bounds violated");
        }

        // 15b. Confidence/Prediction Bands + AIC/BIC + Fit::compareModels
        {
            Table *bandTbl = mw->newTable("BandFitData", 31, 3);
            bandTbl->setColName(0, "X");
            bandTbl->setColPlotDesignation(0, SciDAVis::X);
            bandTbl->setColName(1, "QuadNoisy");
            bandTbl->setColPlotDesignation(1, SciDAVis::Y);
            bandTbl->setColName(2, "ExpClean");
            bandTbl->setColPlotDesignation(2, SciDAVis::Y);
            for (int r = 0; r < 31; ++r) {
                double x = r * 0.5; // 0..15
                double jitter = 0.35 * std::sin(1.7 * r + 0.3);
                bandTbl->setCell(r, 0, x);
                bandTbl->setCell(r, 1, 2.0 + 1.5 * x + 0.4 * x * x + jitter);
                bandTbl->setCell(r, 2, 5.0 + 80.0 * std::exp(-x / 4.0) + 0.05 * jitter);
            }
            MultiLayer *mlBand = mw->multilayerPlot(
                    bandTbl, QStringList() << (bandTbl->name() + "_QuadNoisy"), Graph::Scatter);
            Graph *gBand = mlBand->activeGraph();
            QString qCurve = bandTbl->name() + "_QuadNoisy";

            PolynomialFit pf(mw, gBand, qCurve, 2, false);
            pf.enableConfidenceBand(true, 0.95);
            pf.enablePredictionBand(true, 0.95);
            pf.fit();

            QwtPlotCurve *cConfUp = gBand->curve(qCurve + "_ConfUpper");
            QwtPlotCurve *cConfLo = gBand->curve(qCurve + "_ConfLower");
            QwtPlotCurve *cPredUp = gBand->curve(qCurve + "_PredUpper");
            QwtPlotCurve *cPredLo = gBand->curve(qCurve + "_PredLower");
            CHECK_COND(cConfUp && cConfLo && cPredUp && cPredLo,
                       "Confidence or Prediction band curves were not generated on Graph");
            int midIdx = cConfUp->dataSize() / 2;
            double confSpan = cConfUp->y(midIdx) - cConfLo->y(midIdx);
            double predSpan = cPredUp->y(midIdx) - cPredLo->y(midIdx);
            std::cout << "[INFO] Poly2 Bands & Diagnostics: confSpan=" << confSpan
                      << ", predSpan=" << predSpan << ", AIC=" << pf.aic()
                      << ", BIC=" << pf.bic() << ", F=" << pf.anovaFValue() << "\n";
            CHECK_COND(confSpan > 0.0 && predSpan > confSpan,
                       "Prediction band must be strictly wider than Confidence band");
            CHECK_COND(std::isfinite(pf.aic()) && std::isfinite(pf.bic())
                               && pf.anovaFValue() > 100.0 && pf.anovaPValue() < 1e-6,
                       "AIC/BIC/ANOVA diagnostics invalid");

            MultiLayer *mlCmp = mw->multilayerPlot(
                    bandTbl, QStringList() << (bandTbl->name() + "_ExpClean"), Graph::Scatter);
            Graph *gCmp = mlCmp->activeGraph();
            Table *cmpTbl = Fit::compareModels(
                    mw, gCmp, bandTbl->name() + "_ExpClean",
                    QStringList() << "ExpDec1" << "GaussAmp" << "Boltzmann");
            CHECK_COND(cmpTbl != nullptr && cmpTbl->numRows() >= 3,
                       "Fit::compareModels failed to produce ranked table");
            std::cout << "[INFO] compareModels Rank #1 = "
                      << cmpTbl->column(0)->textAt(0).toStdString()
                      << ", AIC=" << cmpTbl->cell(0, 5)
                      << ", Weight=" << cmpTbl->cell(0, 9) << "\n";
            CHECK_COND(cmpTbl->column(0)->textAt(0).contains("ExpDec1", Qt::CaseInsensitive),
                       "Fit::compareModels did not rank true ExpDec1 model #1 by AIC");
        }

        // 15c. York Linear Fit with X & Y Errors + Apparent Linear Fit on Log10 Axes
        {
            Table *yorkTbl = mw->newTable("YorkFitData", 12, 4);
            yorkTbl->setColName(0, "X");
            yorkTbl->setColPlotDesignation(0, SciDAVis::X);
            yorkTbl->setColName(1, "Y");
            yorkTbl->setColPlotDesignation(1, SciDAVis::Y);
            yorkTbl->setColName(2, "sX");
            yorkTbl->setColPlotDesignation(2, SciDAVis::xErr);
            yorkTbl->setColName(3, "sY");
            yorkTbl->setColPlotDesignation(3, SciDAVis::yErr);
            for (int r = 0; r < 12; ++r) {
                double x = 1.0 + r * 2.0;
                double y = 2.5 * x + 3.0; // exact slope = 2.5, intercept = 3.0
                double sx = 0.05, sy = 0.08;
                if (r == 11) {
                    // Huge uncertainty noisy point that pulls unweighted OLS off course
                    y += 35.0;
                    sx = 80.0;
                    sy = 80.0;
                }
                yorkTbl->setCell(r, 0, x);
                yorkTbl->setCell(r, 1, y);
                yorkTbl->setCell(r, 2, sx);
                yorkTbl->setCell(r, 3, sy);
            }
            MultiLayer *mlYork = mw->multilayerPlot(
                    yorkTbl, QStringList() << (yorkTbl->name() + "_Y"), Graph::Scatter);
            Graph *gYork = mlYork->activeGraph();
            LinearFit lfYork(mw, gYork, yorkTbl->name() + "_Y");
            CHECK_COND(lfYork.fitWithXYErrors("sX", "sY"), "LinearFit::fitWithXYErrors returned false");
            std::cout << "[INFO] York Regression: slope=" << lfYork.yorkSlope()
                      << " +/- " << lfYork.yorkSlopeError()
                      << ", intercept=" << lfYork.yorkIntercept() << "\n";
            CHECK_COND(relErrWithin(lfYork.yorkSlope(), 2.5, 0.01)
                               && relErrWithin(lfYork.yorkIntercept(), 3.0, 0.02)
                               && lfYork.yorkSlopeError() > 0.0,
                       "York regression failed to recover exact weighted slope & intercept");

            // Apparent Fit on Log10-Log10 axes: y = 10 * x^2 -> log10(y) = 1.0 + 2.0 * log10(x)
            const double logXs[7] = { 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0 };
            Table *appTbl = mw->newTable("ApparentLogData", 7, 2);
            appTbl->setColName(0, "X");
            appTbl->setColPlotDesignation(0, SciDAVis::X);
            appTbl->setColName(1, "Y");
            appTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 7; ++r) {
                appTbl->setCell(r, 0, logXs[r]);
                appTbl->setCell(r, 1, 10.0 * logXs[r] * logXs[r]);
            }
            MultiLayer *mlApp = mw->multilayerPlot(
                    appTbl, QStringList() << (appTbl->name() + "_Y"), Graph::Scatter);
            Graph *gApp = mlApp->activeGraph();
            gApp->setScale(QwtPlot::xBottom, 1.0, 100.0, 0.0, 5, 5, 1, false);
            gApp->setScale(QwtPlot::yLeft, 10.0, 100000.0, 0.0, 5, 5, 1, false);
            LinearFit lfApp(mw, gApp, appTbl->name() + "_Y");
            lfApp.setApparentFit(true);
            lfApp.fit();
            std::cout << "[INFO] Apparent Log10 LinearFit: logInt=" << lfApp.results()[0]
                      << ", logSlope=" << lfApp.results()[1] << ", R^2=" << lfApp.rSquare() << "\n";
            CHECK_COND(relErrWithin(lfApp.results()[0], 1.0, 1e-4)
                               && relErrWithin(lfApp.results()[1], 2.0, 1e-4)
                               && lfApp.rSquare() > 0.99999,
                       "Apparent LinearFit on Log10 axes failed to fit in log-transformed space");
        }

        // 15d. Global Curve Fit (shared decay constant) & Implicit ODR Fit (ellipse x^2/a^2 + y^2/b^2 - 1 = 0)
        {
            Table *globTbl = mw->newTable("GlobalFitData", 31, 3);
            globTbl->setColName(0, "X");
            globTbl->setColPlotDesignation(0, SciDAVis::X);
            globTbl->setColName(1, "Decay1");
            globTbl->setColPlotDesignation(1, SciDAVis::Y);
            globTbl->setColName(2, "Decay2");
            globTbl->setColPlotDesignation(2, SciDAVis::Y);
            for (int r = 0; r < 31; ++r) {
                double x = r * 2.0; // 0..60, shared tau = 18.0
                globTbl->setCell(r, 0, x);
                globTbl->setCell(r, 1, 2.0 + 100.0 * std::exp(-x / 18.0));
                globTbl->setCell(r, 2, 10.0 + 250.0 * std::exp(-x / 18.0));
            }
            QStringList globCurves;
            globCurves << (globTbl->name() + "_Decay1") << (globTbl->name() + "_Decay2");
            MultiLayer *mlGlob = mw->multilayerPlot(globTbl, globCurves, Graph::Scatter);
            Graph *gGlob = mlGlob->activeGraph();

            NonLinearFit nlGlob(mw, gGlob, globCurves[0]);
            nlGlob.setParametersList(QStringList() << "y0" << "A" << "t1");
            nlGlob.setFormula("y0 + A * exp(-x / t1)");
            nlGlob.setInitialGuess(0, 5.0);
            nlGlob.setInitialGuess(1, 150.0);
            nlGlob.setInitialGuess(2, 14.0);
            Table *globSummary = nullptr;
            QVector<bool> sharedFlags;
            sharedFlags << false << false << true; // share t1 across both curves
            CHECK_COND(nlGlob.fitGlobal(globCurves, sharedFlags, &globSummary) && globSummary != nullptr,
                       "NonLinearFit::fitGlobal failed");
            double tauCurve1 = globSummary->cell(2, 3);
            double tauCurve2 = globSummary->cell(5, 3);
            double ampCurve1 = globSummary->cell(1, 3);
            double ampCurve2 = globSummary->cell(4, 3);
            std::cout << "[INFO] Global Fit: A1=" << ampCurve1 << ", A2=" << ampCurve2
                      << ", shared t1=(" << tauCurve1 << ", " << tauCurve2 << "), R^2="
                      << nlGlob.rSquare() << "\n";
            CHECK_COND(std::fabs(tauCurve1 - tauCurve2) < 1e-12
                               && relErrWithin(tauCurve1, 18.0, 0.005)
                               && relErrWithin(ampCurve1, 100.0, 0.005)
                               && relErrWithin(ampCurve2, 250.0, 0.005),
                       "NonLinearFit::fitGlobal did not recover shared decay constant & per-curve amplitudes");

            // Implicit ODR Fit: ellipse x^2/a^2 + y^2/b^2 - 1 = 0 with true a = 6.0, b = 3.5
            const int nEll = 36;
            Table *odrTbl = mw->newTable("ImplicitEllipseData", nEll, 2);
            odrTbl->setColName(0, "X");
            odrTbl->setColPlotDesignation(0, SciDAVis::X);
            odrTbl->setColName(1, "Y");
            odrTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < nEll; ++r) {
                double th = 2.0 * M_PI * double(r) / double(nEll);
                odrTbl->setCell(r, 0, 6.0 * std::cos(th));
                odrTbl->setCell(r, 1, 3.5 * std::sin(th));
            }
            MultiLayer *mlOdr = mw->multilayerPlot(
                    odrTbl, QStringList() << (odrTbl->name() + "_Y"), Graph::Scatter);
            Graph *gOdr = mlOdr->activeGraph();
            NonLinearFit nlOdr(mw, gOdr, odrTbl->name() + "_Y");
            CHECK_COND(nlOdr.fitImplicitODR("x^2/(a^2) + y^2/(b^2) - 1",
                                            QStringList() << "a" << "b",
                                            QVector<double>() << 4.8 << 2.6),
                       "NonLinearFit::fitImplicitODR returned false");
            double aFit = std::fabs(nlOdr.results()[0]);
            double bFit = std::fabs(nlOdr.results()[1]);
            std::cout << "[INFO] Implicit ODR Ellipse Fit: a=" << aFit << " (expected 6.0), b="
                      << bFit << " (expected 3.5)\n";
            CHECK_COND(relErrWithin(aFit, 6.0, 0.01) && relErrWithin(bFit, 3.5, 0.01),
                       "fitImplicitODR failed to recover ellipse semi-axes a=6.0, b=3.5 within <1%");
        }
    }

    // =========================================================================
    // 16. Advanced Peak Analyzer (Domain 3)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 16: Advanced Peak Analyzer...\n";
        const int nSpec = 201;
        Table *paTbl = mw->newTable("AdvPeakData", nSpec, 5);
        paTbl->setColName(0, "X");
        paTbl->setColPlotDesignation(0, SciDAVis::X);
        paTbl->setColName(1, "SplineBaseSpec");
        paTbl->setColPlotDesignation(1, SciDAVis::Y);
        paTbl->setColName(2, "ShoulderSpec");
        paTbl->setColPlotDesignation(2, SciDAVis::Y);
        paTbl->setColName(3, "DoubletMixed");
        paTbl->setColPlotDesignation(3, SciDAVis::Y);
        paTbl->setColName(4, "BatchSpec2");
        paTbl->setColPlotDesignation(4, SciDAVis::Y);

        auto cubicBase = [](double x) {
            return 5.0 + 0.12 * x + 0.0015 * x * x - 0.000012 * x * x * x;
        };
        auto gaussArea = [](double x, double A, double xc, double w) {
            return std::sqrt(2.0 / M_PI) * (A / w) * std::exp(-2.0 * (x - xc) * (x - xc) / (w * w));
        };
        auto lorentzArea = [](double x, double A, double xc, double fwhm) {
            return (2.0 * A / M_PI) * fwhm / (4.0 * (x - xc) * (x - xc) + fwhm * fwhm);
        };

        // Mixed-profile spin-orbit doublet: Peak 0 = Gauss(A=300, xc=38, w=5 -> FWHM=5*sqrt(2*ln2)=5.88705),
        // Peak 1 = Lorentz(Area = 0.5 * 300 = 150, xc = 38 + 14 = 52, shared FWHM = 5.8870501)
        const double fwhmShared = 5.0 * std::sqrt(2.0 * std::log(2.0));
        for (int r = 0; r < nSpec; ++r) {
            double x = r * 0.5; // 0..100
            paTbl->setCell(r, 0, x);
            paTbl->setCell(r, 1, cubicBase(x) + gaussArea(x, 220.0, 30.0, 5.0)
                                 + gaussArea(x, 320.0, 70.0, 6.0));
            // Closely overlapping primary peak (xc=45, H=100) + hidden shoulder (xc=52.5, H=42)
            double yShoulder = 100.0 * std::exp(-0.5 * std::pow((x - 45.0) / 3.5, 2.0))
                               + 42.0 * std::exp(-0.5 * std::pow((x - 52.5) / 2.6, 2.0));
            paTbl->setCell(r, 2, yShoulder);
            paTbl->setCell(r, 3, 2.0 + gaussArea(x, 300.0, 38.0, 5.0)
                                 + lorentzArea(x, 150.0, 52.0, fwhmShared));
            paTbl->setCell(r, 4, 3.0 + 0.05 * x + gaussArea(x, 180.0, 28.0, 5.0)
                                 + gaussArea(x, 260.0, 66.0, 5.5));
        }

        // 16a. BaselineAnchors cubic spline baseline subtraction & fit
        MultiLayer *mlPA1 = mw->multilayerPlot(
                paTbl, QStringList() << (paTbl->name() + "_SplineBaseSpec"), Graph::Line);
        Graph *gPA1 = mlPA1->activeGraph();
        MultiPeakFit mpfSpline(mw, gPA1, MultiPeakFit::Gauss, 2);
        mpfSpline.setDataFromCurve(paTbl->name() + "_SplineBaseSpec");
        mpfSpline.setBaselineMode(MultiPeakFit::BaselineAnchors);
        QVector<QPointF> anchors;
        for (double ax : { 0.0, 10.0, 50.0, 90.0, 100.0 })
            anchors << QPointF(ax, cubicBase(ax));
        mpfSpline.setBaselineAnchors(anchors, true);
        std::vector<double> spBase = mpfSpline.computeBaseline();
        CHECK_COND(spBase.size() == size_t(nSpec)
                           && relErrWithin(spBase[60], cubicBase(30.0), 0.005)
                           && relErrWithin(spBase[140], cubicBase(70.0), 0.005),
                   "BaselineAnchors cubic spline baseline deviated from exact cubic polynomial");
        CHECK_COND(mpfSpline.subtractBaselineFromData(), "BaselineAnchors subtractBaselineFromData failed");
        mpfSpline.fit();
        std::cout << "[INFO] BaselineAnchors Subtracted Fit: R^2=" << mpfSpline.rSquare()
                  << ", A1=" << mpfSpline.results()[0] << ", A2=" << mpfSpline.results()[3] << "\n";
        CHECK_COND(mpfSpline.rSquare() > 0.9999 && relErrWithin(mpfSpline.results()[0], 220.0, 0.02)
                           && relErrWithin(mpfSpline.results()[3], 320.0, 0.02),
                   "BaselineAnchors subtracted peak fit failed");

        // 16b. 2nd-Derivative Hidden Shoulder Peak Detection
        MultiLayer *mlPA2 = mw->multilayerPlot(
                paTbl, QStringList() << (paTbl->name() + "_ShoulderSpec"), Graph::Line);
        Graph *gPA2 = mlPA2->activeGraph();
        MultiPeakFit mpfD2(mw, gPA2, MultiPeakFit::Gauss, 2);
        mpfD2.setDataFromCurve(paTbl->name() + "_ShoulderSpec");
        QVector<double> centersD2 = mpfD2.detectPeaksSecondDerivative(11, 0.10);
        std::cout << "[INFO] 2nd-Derivative Detected Centers count=" << centersD2.size();
        for (double c : centersD2)
            std::cout << " " << c;
        std::cout << "\n";
        CHECK_COND(centersD2.size() == 2 && std::fabs(centersD2[0] - 45.0) < 1.2
                           && std::fabs(centersD2[1] - 52.5) < 1.5,
                   "detectPeaksSecondDerivative failed to resolve overlapping shoulder peak");

        // 16c. Mixed Peak Profiles (Gauss + Lorentz) & Doublet Ratio/Offset Constraint
        MultiLayer *mlPA3 = mw->multilayerPlot(
                paTbl, QStringList() << (paTbl->name() + "_DoubletMixed"), Graph::Line);
        Graph *gPA3 = mlPA3->activeGraph();
        MultiPeakFit mpfDbl(mw, gPA3, MultiPeakFit::Gauss, 2);
        mpfDbl.setDataFromCurve(paTbl->name() + "_DoubletMixed");
        QVector<MultiPeakFit::PeakProfile> mixedProfs;
        mixedProfs << MultiPeakFit::Gauss << MultiPeakFit::Lorentz;
        mpfDbl.setPerPeakProfiles(mixedProfs);
        mpfDbl.setInitialGuess(0, 270.0);
        mpfDbl.setInitialGuess(1, 37.5);
        mpfDbl.setInitialGuess(2, 4.6);
        mpfDbl.setInitialGuess(6, 1.5);
        mpfDbl.addPeakDoubletConstraint(1, 0, 0.5, 14.0, true);
        mpfDbl.fit();
        auto dblRows = mpfDbl.computePeakSummary(mpfDbl.results());
        std::cout << "[INFO] Mixed-Profile Doublet Fit: R^2=" << mpfDbl.rSquare()
                  << ", Peak1(xc=" << dblRows[0].center << ", Area=" << dblRows[0].area << ")"
                  << ", Peak2(xc=" << dblRows[1].center << ", Area=" << dblRows[1].area << ")\n";
        CHECK_COND(mpfDbl.rSquare() > 0.9999
                           && relErrWithin(dblRows[0].area, 300.0, 0.01)
                           && relErrWithin(dblRows[1].area, 150.0, 0.01)
                           && std::fabs((dblRows[1].center - dblRows[0].center) - 14.0) < 1e-6,
                   "Mixed-profile doublet constraint failed");

        // 16d. Batch Peak Analysis across multiple curves
        QStringList batchCurves;
        batchCurves << (paTbl->name() + "_SplineBaseSpec") << (paTbl->name() + "_BatchSpec2");
        MultiLayer *mlBatch = mw->multilayerPlot(paTbl, batchCurves, Graph::Line);
        Table *batchTbl = MultiPeakFit::batchAnalyzeCurves(
                mw, mlBatch->activeGraph(), batchCurves, MultiPeakFit::Gauss,
                MultiPeakFit::BaselineALS);
        CHECK_COND(batchTbl != nullptr && batchTbl->numRows() >= 4,
                   "MultiPeakFit::batchAnalyzeCurves failed to analyze all peaks across curves");
        std::cout << "[INFO] Batch Peak Analysis rows=" << batchTbl->numRows()
                  << ", row0 R^2=" << batchTbl->cell(0, 8) << "\n";
        CHECK_COND(batchTbl->cell(0, 8) > 0.99 && batchTbl->cell(batchTbl->numRows() - 1, 8) > 0.99,
                   "Batch Peak Analysis produced poor R^2");
    }

    // =========================================================================
    // 17. Curve Math & Advanced Signal Processing (Domains 4 & 5)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 17: Curve Math & Advanced Signal Processing...\n";

        // 17a. CurveMathTools (simpleCurveMath on mismatched grids, averageMultipleCurves,
        //      normalizeCurve, findCurveIntersections, polygonArea)
        {
            Table *cmTblA = mw->newTable("CurveMathA", 11, 2);
            cmTblA->setColName(0, "XA");
            cmTblA->setColPlotDesignation(0, SciDAVis::X);
            cmTblA->setColName(1, "YA");
            cmTblA->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 11; ++r) {
                double x = r * 2.0; // coarse grid 0, 2, ..., 20
                cmTblA->setCell(r, 0, x);
                cmTblA->setCell(r, 1, 3.0 * x + 1.0);
            }
            Table *cmTblB = mw->newTable("CurveMathB", 21, 4);
            cmTblB->setColName(0, "XB");
            cmTblB->setColPlotDesignation(0, SciDAVis::X);
            cmTblB->setColName(1, "YB");
            cmTblB->setColPlotDesignation(1, SciDAVis::Y);
            cmTblB->setColName(2, "Rep2");
            cmTblB->setColPlotDesignation(2, SciDAVis::Y);
            cmTblB->setColName(3, "Rep3");
            cmTblB->setColPlotDesignation(3, SciDAVis::Y);
            for (int r = 0; r < 21; ++r) {
                double x = double(r); // fine grid 0, 1, ..., 20
                cmTblB->setCell(r, 0, x);
                cmTblB->setCell(r, 1, x + 5.0);
                cmTblB->setCell(r, 2, x + 7.0);
                cmTblB->setCell(r, 3, x + 9.0);
            }
            MultiLayer *mlCM = mw->multilayerPlot(
                    cmTblA, QStringList() << (cmTblA->name() + "_YA"), Graph::Line);
            Graph *gCM = mlCM->activeGraph();
            gCM->insertCurve(cmTblB, cmTblB->colName(0), cmTblB->colName(1), Graph::Line);

            QwtPlotCurve *sumCurve = CurveMathTools::simpleCurveMath(
                    mw, gCM, cmTblA->name() + "_YA", cmTblB->name() + "_YB",
                    CurveMathTools::Add, true);
            CHECK_COND(sumCurve != nullptr && sumCurve->dataSize() >= 21,
                       "CurveMathTools::simpleCurveMath(Add) failed on mismatched X grids");
            double ySumAt10 = CurveMathTools::findYFromX(gCM, sumCurve->title().text(), 10.0);
            std::cout << "[INFO] simpleCurveMath(Add) at x=10: " << ySumAt10 << " (expected 46)\n";
            CHECK_COND(relErrWithin(ySumAt10, 46.0, 1e-5), "simpleCurveMath(Add) value mismatch");

            // Average Multiple Curves (YB=x+5, Rep2=x+7, Rep3=x+9 -> Mean=x+7, SD=2.0)
            MultiLayer *mlAvg = mw->multilayerPlot(
                    cmTblB,
                    QStringList() << (cmTblB->name() + "_YB") << (cmTblB->name() + "_Rep2")
                                  << (cmTblB->name() + "_Rep3"),
                    Graph::Line);
            Graph *gAvg = mlAvg->activeGraph();
            int curvesBeforeAvg = gAvg->curves();
            Table *avgTbl = nullptr;
            QwtPlotCurve *meanCurve = CurveMathTools::averageMultipleCurves(
                    mw, gAvg,
                    QStringList() << (cmTblB->name() + "_YB") << (cmTblB->name() + "_Rep2")
                                  << (cmTblB->name() + "_Rep3"),
                    41, true, &avgTbl);
            CHECK_COND(meanCurve != nullptr && avgTbl != nullptr
                               && gAvg->curves() >= curvesBeforeAvg + 3,
                       "averageMultipleCurves did not add Mean + SD band curves");
            double meanAt10 = CurveMathTools::findYFromX(gAvg, meanCurve->title().text(), 10.0);
            CHECK_COND(relErrWithin(meanAt10, 17.0, 1e-4)
                               && relErrWithin(avgTbl->cell(20, 2), 2.0, 1e-4),
                       "averageMultipleCurves Mean or SD mismatch");

            // Normalize Curve (Range0To1 and UnitArea)
            QwtPlotCurve *norm01 = CurveMathTools::normalizeCurve(
                    mw, gAvg, cmTblB->name() + "_YB", CurveMathTools::Range0To1);
            CHECK_COND(norm01 != nullptr && std::fabs(norm01->minYValue() - 0.0) < 1e-6
                               && std::fabs(norm01->maxYValue() - 1.0) < 1e-6,
                       "normalizeCurve(Range0To1) failed");
            QwtPlotCurve *normArea = CurveMathTools::normalizeCurve(
                    mw, gAvg, cmTblB->name() + "_YB", CurveMathTools::UnitArea);
            CHECK_COND(normArea != nullptr, "normalizeCurve(UnitArea) returned null");
            double trapArea = 0.0;
            for (int i = 0; i + 1 < normArea->dataSize(); ++i)
                trapArea += 0.5 * (normArea->y(i) + normArea->y(i + 1))
                            * (normArea->x(i + 1) - normArea->x(i));
            CHECK_COND(relErrWithin(trapArea, 1.0, 1e-4),
                       "normalizeCurve(UnitArea) integral is not 1.0");

            // Curve Intersections & Closed Polygon Loop Area
            Table *ixTbl = mw->newTable("IntersectPolyData", 5, 4);
            ixTbl->setColName(0, "X");
            ixTbl->setColPlotDesignation(0, SciDAVis::X);
            ixTbl->setColName(1, "LineUp");
            ixTbl->setColPlotDesignation(1, SciDAVis::Y);
            ixTbl->setColName(2, "LineDown");
            ixTbl->setColPlotDesignation(2, SciDAVis::Y);
            ixTbl->setColName(3, "LoopY");
            ixTbl->setColPlotDesignation(3, SciDAVis::Y);
            for (int r = 0; r < 5; ++r) {
                double x = r * 2.5; // 0..10
                ixTbl->setCell(r, 0, x);
                ixTbl->setCell(r, 1, x);
                ixTbl->setCell(r, 2, 10.0 - x);
                ixTbl->setCell(r, 3, 0.0);
            }
            MultiLayer *mlIx = mw->multilayerPlot(
                    ixTbl, QStringList() << (ixTbl->name() + "_LineUp")
                                         << (ixTbl->name() + "_LineDown"),
                    Graph::Line);
            Graph *gIx = mlIx->activeGraph();
            QVector<QPointF> xpts = CurveMathTools::findCurveIntersections(
                    gIx, ixTbl->name() + "_LineUp", ixTbl->name() + "_LineDown", true, mw);
            CHECK_COND(xpts.size() == 1 && relErrWithin(xpts[0].x(), 5.0, 1e-6)
                               && relErrWithin(xpts[0].y(), 5.0, 1e-6),
                       "findCurveIntersections failed to locate exact (5, 5) crossing");

            Table *loopTbl = mw->newTable("ClosedLoop", 5, 2);
            loopTbl->setColName(0, "LX");
            loopTbl->setColPlotDesignation(0, SciDAVis::X);
            loopTbl->setColName(1, "LY");
            loopTbl->setColPlotDesignation(1, SciDAVis::Y);
            const double loopPts[5][2] = {
                { 0.0, 0.0 }, { 4.0, 0.0 }, { 4.0, 3.0 }, { 0.0, 3.0 }, { 0.0, 0.0 }
            };
            for (int r = 0; r < 5; ++r) {
                loopTbl->setCell(r, 0, loopPts[r][0]);
                loopTbl->setCell(r, 1, loopPts[r][1]);
            }
            MultiLayer *mlLoop = mw->multilayerPlot(
                    loopTbl, QStringList() << (loopTbl->name() + "_LY"), Graph::Line);
            double polyA = CurveMathTools::polygonArea(
                    mlLoop->activeGraph(), loopTbl->name() + "_LY", false);
            std::cout << "[INFO] polygonArea of 4x3 rectangle = " << polyA << "\n";
            CHECK_COND(relErrWithin(polyA, 12.0, 1e-6), "polygonArea exact Shoelace loop area mismatch");
        }

        // 17b. SmoothFilter (LOWESS & Whittaker) + AdvancedSignalProc (IIR, STFT, Hilbert, CWT, DWT, 2D FFT)
        {
            const int nSig = 512;
            const double fs = 500.0;
            const double dt = 1.0 / fs;
            Table *sigTbl = mw->newTable("SignalProcData", nSig, 4);
            sigTbl->setColName(0, "Time");
            sigTbl->setColPlotDesignation(0, SciDAVis::X);
            sigTbl->setColName(1, "TwoTone");
            sigTbl->setColPlotDesignation(1, SciDAVis::Y);
            sigTbl->setColName(2, "AMSignal");
            sigTbl->setColPlotDesignation(2, SciDAVis::Y);
            sigTbl->setColName(3, "NoisyWave");
            sigTbl->setColPlotDesignation(3, SciDAVis::Y);

            double noisyMse = 0.0;
            for (int i = 0; i < nSig; ++i) {
                double tVal = i * dt;
                double cleanLow = std::sin(2.0 * M_PI * 5.0 * tVal);
                double twoTone = cleanLow + 0.85 * std::sin(2.0 * M_PI * 120.0 * tVal);
                double amEnv = 1.0 + 0.5 * std::cos(2.0 * M_PI * 3.0 * tVal);
                double amSig = amEnv * std::cos(2.0 * M_PI * 60.0 * tVal);
                // Deterministic high-frequency pseudo-Gaussian sum of incommensurate sines
                double noise = 0.22 * (std::sin(37.3 * i + 0.1) + std::cos(71.9 * i + 1.2)
                                       + std::sin(113.7 * i + 2.4)) / std::sqrt(1.5);
                double cleanSmooth = std::sin(2.0 * M_PI * 2.0 * tVal)
                                     + 0.5 * std::cos(2.0 * M_PI * 4.0 * tVal);
                noisyMse += noise * noise;

                sigTbl->setCell(i, 0, tVal);
                sigTbl->setCell(i, 1, twoTone);
                sigTbl->setCell(i, 2, amSig);
                sigTbl->setCell(i, 3, cleanSmooth + noise);
            }
            noisyMse /= double(nSig);

            MultiLayer *mlSig = mw->multilayerPlot(
                    sigTbl,
                    QStringList() << (sigTbl->name() + "_TwoTone")
                                  << (sigTbl->name() + "_AMSignal")
                                  << (sigTbl->name() + "_NoisyWave"),
                    Graph::Line);
            Graph *gSig = mlSig->activeGraph();

            // SmoothFilter::LOWESS & SmoothFilter::Whittaker
            int beforeSmooth = gSig->curves();
            SmoothFilter sfLowess(mw, gSig, sigTbl->name() + "_NoisyWave", SmoothFilter::LOWESS);
            sfLowess.setLowessSpan(0.08);
            sfLowess.run();
            CHECK_COND(gSig->curves() == beforeSmooth + 1, "SmoothFilter::LOWESS did not add curve");
            QwtPlotCurve *cLowess = gSig->curve(gSig->curves() - 1);

            SmoothFilter sfWhit(mw, gSig, sigTbl->name() + "_NoisyWave", SmoothFilter::Whittaker);
            sfWhit.setWhittakerLambda(200.0);
            sfWhit.run();
            CHECK_COND(gSig->curves() == beforeSmooth + 2, "SmoothFilter::Whittaker did not add curve");
            QwtPlotCurve *cWhit = gSig->curve(gSig->curves() - 1);

            double mseLowess = 0.0, mseWhit = 0.0;
            for (int i = 30; i < nSig - 30; ++i) {
                double tVal = i * dt;
                double clean = std::sin(2.0 * M_PI * 2.0 * tVal)
                               + 0.5 * std::cos(2.0 * M_PI * 4.0 * tVal);
                double dL = cLowess->y(i) - clean;
                double dW = cWhit->y(i) - clean;
                mseLowess += dL * dL;
                mseWhit += dW * dW;
            }
            mseLowess /= double(nSig - 60);
            mseWhit /= double(nSig - 60);
            std::cout << "[INFO] Smoothing MSE: raw=" << noisyMse << ", LOWESS=" << mseLowess
                      << ", Whittaker=" << mseWhit << "\n";
            CHECK_COND(mseLowess < 0.35 * noisyMse && mseWhit < 0.35 * noisyMse,
                       "LOWESS or Whittaker smoothing did not significantly reduce noise MSE");

            // Zero-phase Butterworth LowPass IIR Filter (cutoff 25 Hz removes 120 Hz, preserves 5 Hz!)
            QwtPlotCurve *cLp = AdvancedSignalProc::iirFilter(
                    mw, gSig, sigTbl->name() + "_TwoTone", AdvancedSignalProc::Butterworth,
                    AdvancedSignalProc::LowPass, 4, 25.0, 0.0, true);
            CHECK_COND(cLp != nullptr && cLp->dataSize() == nSig, "iirFilter returned null/wrong size");
            double rmsLpErr = 0.0;
            for (int i = 75; i < nSig - 75; ++i) {
                double clean5Hz = std::sin(2.0 * M_PI * 5.0 * (i * dt));
                double e = cLp->y(i) - clean5Hz;
                rmsLpErr += e * e;
            }
            rmsLpErr = std::sqrt(rmsLpErr / double(nSig - 150));
            std::cout << "[INFO] Zero-phase Butterworth LP RMS error vs 5Hz sine = " << rmsLpErr << "\n";
            CHECK_COND(rmsLpErr < 0.03,
                       "Zero-phase Butterworth LP failed to remove 120Hz tone while preserving 5Hz sine");

            // STFT Spectrogram Matrix
            Matrix *stftMat = AdvancedSignalProc::computeSTFT(
                    mw, gSig, sigTbl->name() + "_TwoTone", 64, 16, false);
            CHECK_COND(stftMat != nullptr && stftMat->numRows() >= 16 && stftMat->numCols() >= 16,
                       "AdvancedSignalProc::computeSTFT failed");

            // Hilbert Envelope on AM Signal: envelope = 1.0 + 0.5*cos(2*pi*3*t)
            QwtPlotCurve *cEnv = AdvancedSignalProc::hilbertEnvelope(
                    mw, gSig, sigTbl->name() + "_AMSignal", false);
            CHECK_COND(cEnv != nullptr && cEnv->dataSize() == nSig,
                       "AdvancedSignalProc::hilbertEnvelope failed");
            double rmsEnvErr = 0.0;
            for (int i = 60; i < nSig - 60; ++i) {
                double trueEnv = 1.0 + 0.5 * std::cos(2.0 * M_PI * 3.0 * (i * dt));
                double e = cEnv->y(i) - trueEnv;
                rmsEnvErr += e * e;
            }
            rmsEnvErr = std::sqrt(rmsEnvErr / double(nSig - 120));
            std::cout << "[INFO] Hilbert AM Envelope RMS error = " << rmsEnvErr << "\n";
            CHECK_COND(rmsEnvErr < 0.035, "Hilbert envelope failed to recover exact AM modulation");

            // Continuous Wavelet Transform (Morlet CWT)
            Matrix *cwtMat = AdvancedSignalProc::waveletCWT(
                    mw, gSig, sigTbl->name() + "_TwoTone", 24, AdvancedSignalProc::Morlet, false);
            CHECK_COND(cwtMat != nullptr && cwtMat->numRows() == 24 && cwtMat->numCols() == nSig,
                       "AdvancedSignalProc::waveletCWT failed");

            // Discrete Wavelet Denoising (Daubechies4 DWT SNR improvement check)
            QwtPlotCurve *cDwt = AdvancedSignalProc::waveletDenoiseDWT(
                    mw, gSig, sigTbl->name() + "_NoisyWave", AdvancedSignalProc::Daubechies4, 4,
                    true, 1.0);
            CHECK_COND(cDwt != nullptr && cDwt->dataSize() == nSig,
                       "AdvancedSignalProc::waveletDenoiseDWT failed");
            double dwtMse = 0.0;
            for (int i = 20; i < nSig - 20; ++i) {
                double tVal = i * dt;
                double clean = std::sin(2.0 * M_PI * 2.0 * tVal)
                               + 0.5 * std::cos(2.0 * M_PI * 4.0 * tVal);
                double d = cDwt->y(i) - clean;
                dwtMse += d * d;
            }
            dwtMse /= double(nSig - 40);
            std::cout << "[INFO] DWT(db4) Denoise MSE: before=" << noisyMse
                      << ", after=" << dwtMse << "\n";
            CHECK_COND(dwtMse < 0.45 * noisyMse,
                       "waveletDenoiseDWT did not improve SNR over noisy signal");

            // 2D Matrix FFT
            Matrix *srcMat2D = mw->newMatrix("Cosine2DMat", 32, 32);
            for (int r = 0; r < 32; ++r)
                for (int c = 0; c < 32; ++c)
                    srcMat2D->setCell(r, c, std::cos(2.0 * M_PI * (3.0 * c / 32.0 + 5.0 * r / 32.0)));
            Matrix *fftMat2D = AdvancedSignalProc::fft2D(mw, srcMat2D, false, true);
            CHECK_COND(fftMat2D != nullptr && fftMat2D->numRows() == 32 && fftMat2D->numCols() == 32,
                       "AdvancedSignalProc::fft2D failed");
            double maxSpec2D = 0.0;
            for (int r = 0; r < 32; ++r)
                for (int c = 0; c < 32; ++c)
                    maxSpec2D = std::max(maxSpec2D, fftMat2D->cell(r, c));
            CHECK_COND(maxSpec2D > 100.0 && fftMat2D->cell(16, 16) < 0.05 * maxSpec2D,
                       "fft2D centered magnitude spectrum missing sharp off-DC spatial frequency peak");
        }
    }

    // =========================================================================
    // 18. 3D Surface Fitting, 2D Peak Analysis, & Graph Statistics (Domain 6)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 18: 3D Surface Fit, 2D Peaks, & Graph Statistics...\n";

        // 18a. fitSurfaceXYZ & fitSurfaceMatrix (Gauss2D) + analyze2DPeaks
        {
            const int side = 25;
            Table *xyzTbl = mw->newTable("SurfXYZ", side * side, 3);
            xyzTbl->setColName(0, "X");
            xyzTbl->setColPlotDesignation(0, SciDAVis::X);
            xyzTbl->setColName(1, "Y");
            xyzTbl->setColPlotDesignation(1, SciDAVis::Y);
            xyzTbl->setColName(2, "Z");
            xyzTbl->setColPlotDesignation(2, SciDAVis::Z);

            Matrix *surfMat = mw->newMatrix("SurfGaussMat", side, side);
            surfMat->setCoordinates(-5.0, 5.0, -5.0, 5.0);

            int rowIdx = 0;
            for (int r = 0; r < side; ++r) {
                double y = -5.0 + 10.0 * double(r) / double(side - 1);
                for (int c = 0; c < side; ++c) {
                    double x = -5.0 + 10.0 * double(c) / double(side - 1);
                    double z = 2.0
                               + 50.0 * std::exp(-0.5 * (std::pow((x - 1.2) / 1.1, 2.0)
                                                         + std::pow((y + 0.8) / 1.6, 2.0)));
                    xyzTbl->setCell(rowIdx, 0, x);
                    xyzTbl->setCell(rowIdx, 1, y);
                    xyzTbl->setCell(rowIdx, 2, z);
                    surfMat->setCell(r, c, z);
                    ++rowIdx;
                }
            }

            auto resXYZ = GraphStatsAndSurfaceFit::fitSurfaceXYZ(
                    mw, xyzTbl, "X", "Y", "Z", GraphStatsAndSurfaceFit::Gauss2D, QString(), 25);
            std::cout << "[INFO] fitSurfaceXYZ(Gauss2D): converged=" << resXYZ.converged
                      << ", R^2=" << resXYZ.rSquare << ", xc=" << resXYZ.params.value(2)
                      << ", yc=" << resXYZ.params.value(3) << ", A=" << resXYZ.params.value(1) << "\n";
            CHECK_COND(resXYZ.converged && resXYZ.rSquare > 0.9999
                               && resXYZ.fittedMatrix != nullptr && resXYZ.paramTable != nullptr
                               && relErrWithin(resXYZ.params[1], 50.0, 0.005)
                               && std::fabs(resXYZ.params[2] - 1.2) < 0.02
                               && std::fabs(resXYZ.params[3] - (-0.8)) < 0.02,
                       "fitSurfaceXYZ(Gauss2D) failed to recover 2D Gaussian parameters");

            auto resMat = GraphStatsAndSurfaceFit::fitSurfaceMatrix(
                    mw, surfMat, GraphStatsAndSurfaceFit::Gauss2D);
            double minSig = std::min(resMat.params.value(4), resMat.params.value(5));
            double maxSig = std::max(resMat.params.value(4), resMat.params.value(5));
            std::cout << "[INFO] fitSurfaceMatrix(Gauss2D): R^2=" << resMat.rSquare
                      << ", sigmas=(" << minSig << ", " << maxSig << ")\n";
            CHECK_COND(resMat.converged && resMat.rSquare > 0.9999
                               && relErrWithin(minSig, 1.1, 0.01)
                               && relErrWithin(maxSig, 1.6, 0.01),
                       "fitSurfaceMatrix(Gauss2D) failed to recover widths");

            // 2D Peak Analysis on 41x41 Matrix with 2 separated 2D Gaussian peaks
            Matrix *pkMat2D = mw->newMatrix("TwoPeaks2DMat", 41, 41);
            pkMat2D->setCoordinates(-10.0, 10.0, -10.0, 10.0);
            for (int r = 0; r < 41; ++r) {
                double y = -10.0 + 20.0 * double(r) / 40.0;
                for (int c = 0; c < 41; ++c) {
                    double x = -10.0 + 20.0 * double(c) / 40.0;
                    double z = 1.5 + 0.05 * x
                               + 80.0 * std::exp(-0.5 * ((x + 4.0) * (x + 4.0) / 1.44
                                                         + (y + 3.0) * (y + 3.0) / 1.44))
                               + 55.0 * std::exp(-0.5 * ((x - 4.5) * (x - 4.5) / 1.69
                                                         + (y - 3.5) * (y - 3.5) / 1.69));
                    pkMat2D->setCell(r, c, z);
                }
            }
            Table *pk2DTbl = GraphStatsAndSurfaceFit::analyze2DPeaks(mw, pkMat2D, 5, 0.15);
            CHECK_COND(pk2DTbl != nullptr && pk2DTbl->numRows() == 2,
                       "analyze2DPeaks did not detect exactly 2 peaks");
            std::cout << "[INFO] analyze2DPeaks: Peak1=(" << pk2DTbl->cell(0, 1) << ", "
                      << pk2DTbl->cell(0, 2) << "), Peak2=(" << pk2DTbl->cell(1, 1) << ", "
                      << pk2DTbl->cell(1, 2) << ")\n";
            CHECK_COND(std::fabs(pk2DTbl->cell(0, 1) - (-4.0)) < 0.25
                               && std::fabs(pk2DTbl->cell(0, 2) - (-3.0)) < 0.25
                               && std::fabs(pk2DTbl->cell(1, 1) - 4.5) < 0.25
                               && std::fabs(pk2DTbl->cell(1, 2) - 3.5) < 0.25,
                       "analyze2DPeaks 2D peak center coordinates inaccurate");
        }

        // 18b. Confidence Ellipse, ANOVA + Significance Brackets, ROC Curve, Kaplan-Meier, PCA Biplot
        {
            // Confidence Ellipse
            Table *ellTbl = mw->newTable("BivarData", 25, 2);
            ellTbl->setColName(0, "X");
            ellTbl->setColPlotDesignation(0, SciDAVis::X);
            ellTbl->setColName(1, "Y");
            ellTbl->setColPlotDesignation(1, SciDAVis::Y);
            for (int r = 0; r < 25; ++r) {
                double u = -2.0 + 4.0 * double(r) / 24.0;
                double v = 0.6 * std::sin(2.1 * r);
                ellTbl->setCell(r, 0, 10.0 + 2.0 * u);
                ellTbl->setCell(r, 1, 20.0 + 1.4 * u + v);
            }
            MultiLayer *mlEll = mw->multilayerPlot(
                    ellTbl, QStringList() << (ellTbl->name() + "_Y"), Graph::Scatter);
            Graph *gEll = mlEll->activeGraph();
            QwtPlotCurve *cEll = GraphStatsAndSurfaceFit::addConfidenceEllipse(
                    mw, gEll, ellTbl->name() + "_Y", 0.95, false);
            CHECK_COND(cEll != nullptr && cEll->dataSize() == 121
                               && std::fabs(cEll->x(0) - cEll->x(120)) < 1e-6,
                       "addConfidenceEllipse failed to plot closed 121-point 95% ellipse");

            // One-Way ANOVA + Pairwise Significance Brackets on Graph
            Table *grpTbl = mw->newTable("GroupAnovaData", 12, 3);
            grpTbl->setColName(0, "Control");
            grpTbl->setColName(1, "LowDose");
            grpTbl->setColName(2, "HighDose");
            for (int r = 0; r < 12; ++r) {
                double spread = 0.5 * std::sin(1.9 * r);
                grpTbl->setCell(r, 0, 10.0 + spread);
                grpTbl->setCell(r, 1, 14.5 + spread);
                grpTbl->setCell(r, 2, 26.0 + spread);
            }
            MultiLayer *mlGrp = mw->newGraph("AnovaBracketGraph");
            Graph *gGrp = mlGrp->activeGraph();
            int arrowsBefore = gGrp->lineMarkerKeys().size();
            Table *anovaTbl = GraphStatsAndSurfaceFit::runGroupComparisonAndAnnotate(
                    mw, grpTbl, QStringList() << "Control" << "LowDose" << "HighDose", gGrp);
            CHECK_COND(anovaTbl != nullptr && anovaTbl->numRows() == 4,
                       "runGroupComparisonAndAnnotate failed");
            double pAnova = anovaTbl->cell(0, 4);
            QString sigStars = anovaTbl->column(7)->textAt(0);
            std::cout << "[INFO] One-Way ANOVA: F=" << anovaTbl->cell(0, 1) << ", p=" << pAnova
                      << " (" << sigStars.toStdString() << "), arrows added="
                      << (gGrp->lineMarkerKeys().size() - arrowsBefore) << "\n";
            CHECK_COND(pAnova < 1e-8 && sigStars == "***"
                               && gGrp->lineMarkerKeys().size() >= arrowsBefore + 3,
                       "ANOVA p-value or graph significance brackets missing");

            // ROC Curve & AUC
            Table *rocSrc = mw->newTable("RocInput", 30, 2);
            rocSrc->setColName(0, "State");
            rocSrc->setColName(1, "Score");
            for (int r = 0; r < 30; ++r) {
                int state = (r >= 15) ? 1 : 0;
                double score = (state == 1 ? 2.5 : 0.5) + 0.6 * std::sin(1.3 * r);
                rocSrc->setCell(r, 0, double(state));
                rocSrc->setCell(r, 1, score);
            }
            double aucVal = 0.0;
            MultiLayer *mlRoc = GraphStatsAndSurfaceFit::plotROCCurve(
                    mw, rocSrc, "State", "Score", &aucVal);
            std::cout << "[INFO] ROC Curve AUC = " << aucVal << "\n";
            CHECK_COND(mlRoc != nullptr && mlRoc->activeGraph() != nullptr
                               && aucVal > 0.95 && aucVal <= 1.0,
                       "plotROCCurve failed or returned unexpected AUC");

            // Kaplan-Meier Survival & Mantel-Cox Log-Rank p-value
            Table *kmSrc = mw->newTable("KmInput", 24, 3);
            kmSrc->setColName(0, "Time");
            kmSrc->setColName(1, "Event");
            kmSrc->setColName(2, "Group");
            kmSrc->setColumnType(2, SciDAVis::ColumnMode::Text);
            for (int r = 0; r < 12; ++r) {
                kmSrc->setCell(r, 0, 2.0 + r * 0.8);      // Placebo: early events t = 2..10.8
                kmSrc->setCell(r, 1, 1.0);
                kmSrc->setText(r, 2, "Placebo");

                kmSrc->setCell(12 + r, 0, 20.0 + r * 3.0); // Drug: much longer survival t = 20..53
                kmSrc->setCell(12 + r, 1, (r % 3 == 0) ? 0.0 : 1.0);
                kmSrc->setText(12 + r, 2, "Drug");
            }
            double logRankP = 1.0;
            MultiLayer *mlKm = GraphStatsAndSurfaceFit::plotKaplanMeier(
                    mw, kmSrc, "Time", "Event", "Group", &logRankP);
            std::cout << "[INFO] Kaplan-Meier Log-Rank p = " << logRankP << "\n";
            CHECK_COND(mlKm != nullptr && mlKm->activeGraph() != nullptr
                               && mlKm->activeGraph()->curves() >= 2 && logRankP < 0.001,
                       "plotKaplanMeier failed or Log-Rank p-value not significant");

            // Principal Component Analysis (PCA) & Biplot
            Table *pcaSrc = mw->newTable("PcaInput", 24, 4);
            pcaSrc->setColName(0, "Var1");
            pcaSrc->setColName(1, "Var2");
            pcaSrc->setColName(2, "Var3");
            pcaSrc->setColName(3, "Var4");
            for (int r = 0; r < 24; ++r) {
                double f1 = std::cos(0.45 * r);
                double f2 = std::sin(0.73 * r);
                pcaSrc->setCell(r, 0, 2.0 * f1 + 0.1 * f2);
                pcaSrc->setCell(r, 1, 1.9 * f1 - 0.15 * f2);
                pcaSrc->setCell(r, 2, 0.1 * f1 + 1.8 * f2);
                pcaSrc->setCell(r, 3, -0.1 * f1 + 1.75 * f2);
            }
            Table *pcaScores = nullptr;
            MultiLayer *mlPca = GraphStatsAndSurfaceFit::runPCAAndBiplot(
                    mw, pcaSrc, QStringList() << "Var1" << "Var2" << "Var3" << "Var4", &pcaScores);
            CHECK_COND(mlPca != nullptr && pcaScores != nullptr && mlPca->activeGraph() != nullptr,
                       "runPCAAndBiplot failed");
            double cumVarPC2 = pcaScores->cell(1, 5);
            std::cout << "[INFO] PCA Biplot: PC1+PC2 cumulative variance = " << cumVarPC2
                      << "%, loading arrows = " << mlPca->activeGraph()->lineMarkerKeys().size() << "\n";
            CHECK_COND(cumVarPC2 > 98.0 && mlPca->activeGraph()->lineMarkerKeys().size() >= 4,
                       "PCA Biplot cumulative variance or loading vectors invalid");
        }
    }

    // =========================================================================
    // 19. Dynamic Recalculation ("Green Lock"), Dialog Themes, & Bilingual Menus (Domain 7)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 19: Dynamic Recalculation, Themes, & Bilingual Menus...\n";

        Table *recTbl = mw->newTable("LiveRecalcData", 11, 2);
        recTbl->setColName(0, "X");
        recTbl->setColPlotDesignation(0, SciDAVis::X);
        recTbl->setColName(1, "Y");
        recTbl->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < 11; ++r) {
            double x = double(r);
            recTbl->setCell(r, 0, x);
            recTbl->setCell(r, 1, 2.5 * x + 1.0); // initial slope = 2.5
        }
        MultiLayer *mlRec = mw->multilayerPlot(
                recTbl, QStringList() << (recTbl->name() + "_Y"), Graph::Scatter);
        Graph *gRec = mlRec->activeGraph();

        RecalcManager *rm = RecalcManager::instance(mw);
        CHECK_COND(rm != nullptr, "RecalcManager::instance returned null");
        rm->setRecalcMode(RecalcManager::Auto);

        int linkId = rm->registerLink(mw, gRec, recTbl, recTbl->name() + "_Y",
                                      "LiveAutoLinearFit", RecalcManager::OpLinearFit);
        CHECK_COND(linkId > 0 && rm->activeLinkCount(gRec) >= 1,
                   "RecalcManager::registerLink failed");

        QwtPlotCurve *fitBefore = gRec->curve("LiveAutoLinearFit");
        CHECK_COND(fitBefore != nullptr && fitBefore->dataSize() >= 2,
                   "Auto RecalcManager did not materialize initial LiveAutoLinearFit curve");
        int lastPt = fitBefore->dataSize() - 1;
        double slopeBefore = (fitBefore->y(lastPt) - fitBefore->y(0))
                             / (fitBefore->x(lastPt) - fitBefore->x(0));
        std::cout << "[INFO] Live Recalc initial slope = " << slopeBefore << "\n";
        CHECK_COND(relErrWithin(slopeBefore, 2.5, 1e-5), "Initial live linear fit slope mismatch");

        // Double all Y values in source Table WITHOUT manually invoking LinearFit or recalculateAll!
        for (int r = 0; r < 11; ++r)
            recTbl->setCell(r, 1, 2.0 * recTbl->cell(r, 1)); // new line: y = 5.0 * x + 2.0
        QApplication::processEvents();

        QwtPlotCurve *fitAfter = gRec->curve("LiveAutoLinearFit");
        CHECK_COND(fitAfter != nullptr && fitAfter->dataSize() >= 2,
                   "LiveAutoLinearFit curve missing after source Table update");
        int lastPtAfter = fitAfter->dataSize() - 1;
        double slopeAfter = (fitAfter->y(lastPtAfter) - fitAfter->y(0))
                            / (fitAfter->x(lastPtAfter) - fitAfter->x(0));
        std::cout << "[INFO] Live Recalc slope after doubling Y = " << slopeAfter
                  << " (expected 5.0)\n";
        CHECK_COND(relErrWithin(slopeAfter, 5.0, 1e-5) && !rm->hasDirtyLinks(gRec),
                   "Auto RecalcManager did not automatically double fitted slope on Graph!");
        rm->removeLinksForGraph(gRec);

        // DialogThemeManager save / load / themeNames / delete round-trip
        QVariantMap themeIn;
        themeIn["model"] = QString("Boltzmann");
        themeIn["tolerance"] = 1e-9;
        themeIn["maxIterations"] = 600;
        themeIn["confidenceBand"] = true;
        DialogThemeManager::saveTheme("NonLinearFit", "OriginParityTheme", themeIn);
        CHECK_COND(DialogThemeManager::themeNames("NonLinearFit").contains("OriginParityTheme"),
                   "DialogThemeManager::themeNames missing saved theme");
        QVariantMap themeOut = DialogThemeManager::loadTheme("NonLinearFit", "OriginParityTheme");
        CHECK_COND(themeOut.value("model").toString() == "Boltzmann"
                           && std::fabs(themeOut.value("tolerance").toDouble() - 1e-9) < 1e-15
                           && themeOut.value("maxIterations").toInt() == 600
                           && themeOut.value("confidenceBand").toBool() == true,
                   "DialogThemeManager::loadTheme round-trip mismatch");
        DialogThemeManager::deleteTheme("NonLinearFit", "OriginParityTheme");
        CHECK_COND(!DialogThemeManager::themeNames("NonLinearFit").contains("OriginParityTheme"),
                   "DialogThemeManager::deleteTheme did not remove theme");

        // Bilingual Japanese menu verification (&Gadgets -> ガジェット(&G))
        mw->switchToLanguage("ja");
        QString jaGadgets = QApplication::translate("ApplicationWindow", "&Gadgets");
        std::cout << "[INFO] Bilingual check (ja): tr(&Gadgets) = "
                  << jaGadgets.toUtf8().constData() << "\n";
        CHECK_COND(jaGadgets == QString::fromUtf8("ガジェット(&G)"),
                   "switchToLanguage(\"ja\") did not translate &Gadgets to ガジェット(&G)");
        if (mw->gadgetsMenu) {
            CHECK_COND(mw->gadgetsMenu->title() == QString::fromUtf8("ガジェット(&D)"),
                       "ApplicationWindow::gadgetsMenu title was not updated to ガジェット(&D)");
        }
        mw->switchToLanguage("en");
        CHECK_COND(QApplication::translate("ApplicationWindow", "&Gadgets") == "&Gadgets",
                   "switchToLanguage(\"en\") did not restore &Gadgets");
    }

    // =========================================================================
    // 20. Suite 20: 100% Japanese GUI Localization Audit
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 20: 100% Japanese GUI Localization Audit...\n";

        Table *tJa = mw->newTable(20, 2, "JaAuditTable", "JaAudit");
        for (int r = 0; r < 20; ++r) {
            double x = double(r) * 1.5;
            tJa->setCell(r, 0, x);
            tJa->setCell(r, 1, 2.0 + 15.0 * std::exp(-0.5 * std::pow((x - 12.0) / 3.0, 2.0)));
        }
        MultiLayer *mlJa = mw->multilayerPlot(tJa, QStringList() << "JaAuditTable_2", Graph::LineSymbols);
        CHECK_COND(mlJa != nullptr && mlJa->activeGraph() != nullptr, "Failed to create mlJa for Suite 20");
        Graph *gJa = mlJa->activeGraph();
        mw->activateSubWindow(mlJa);
        mw->customMenu(mlJa);

        mw->switchToLanguage("ja");
        CHECK_COND(mw->appLanguage == "ja", "appLanguage was not set to 'ja'");

        auto stripMnemonics = [](QString s) -> QString {
            s.replace("&&", "\x01");
            int idx = 0;
            while ((idx = s.indexOf("(&", idx)) >= 0) {
                if (idx + 3 < s.size() && s.at(idx + 3) == QChar(')'))
                    s.remove(idx, 4);
                else
                    idx += 2;
            }
            s.remove('&');
            s.replace('\x01', '&');
            return s.trimmed();
        };

        auto hasJapaneseChars = [](const QString &s) -> bool {
            for (int i = 0; i < s.size(); ++i) {
                ushort u = s.at(i).unicode();
                if ((u >= 0x3040 && u <= 0x30FF)       // Hiragana & Katakana
                    || (u >= 0x4E00 && u <= 0x9FFF)    // CJK Unified Ideographs
                    || (u >= 0x3400 && u <= 0x4DBF)) { // CJK Extension A
                    return true;
                }
            }
            return false;
        };

        const QSet<QString> allowedUniversalTokens = {
            "English", "English (&E)", "OK", "FFT", "IFFT", "2D", "3D",
            "AIC", "BIC", "AIC/BIC", "PCA", "ROC", "CSV", "SVG", "PDF", "EPS",
            "ASCII", "TeX", "LaTeX", "LOWESS", "LOESS", "STFT", "IIR", "FIR",
            "CWT", "DWT", "PSD", "MLR", "ANOVA", "Savitzky-Golay",
            "X", "Y", "Z", "A", "B", "C", "R^2", "R²", "p", "t", "F"
        };

        auto isLocalizedJapaneseOrUniversal = [&](const QString &raw) -> bool {
            QString clean = stripMnemonics(raw);
            if (clean.isEmpty())
                return true;
            if (hasJapaneseChars(clean))
                return true;
            if (allowedUniversalTokens.contains(clean))
                return true;
            bool anyAsciiAlpha = false;
            for (int i = 0; i < clean.size(); ++i) {
                ushort u = clean.at(i).unicode();
                if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z')) {
                    anyAsciiAlpha = true;
                    break;
                }
            }
            if (!anyAsciiAlpha)
                return true;
            return false;
        };

        int verifiedMenuActions = 0;
        QStringList untranslatedItems;
        QSet<const QMenu *> visitedMenus;

        std::function<void(QMenu *, const QString &)> auditMenuRecursive =
                [&](QMenu *menu, const QString &pathPrefix) {
            if (!menu || visitedMenus.contains(menu))
                return;
            if (pathPrefix.contains(QString::fromUtf8("最近のプロジェクト"))
                || pathPrefix.contains("Recent Projects")) {
                return;
            }
            visitedMenus.insert(menu);

            QString menuTitle = menu->title();
            if (!menuTitle.isEmpty()) {
                ++verifiedMenuActions;
                if (!isLocalizedJapaneseOrUniversal(menuTitle)) {
                    untranslatedItems << QString("MenuTitle[%1]: '%2'").arg(pathPrefix, menuTitle);
                }
            }

            for (QAction *act : menu->actions()) {
                if (pathPrefix.contains(QString::fromUtf8("\xE6\x9C\x80\xE8\xBF\x91\xE4\xBD\xBF\xE3\x81\xA3\xE3\x81\x9F\xE3\x83\x97\xE3\x83\xAD\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88")))
                    break;
                if (!act || act->isSeparator())
                    continue;
                if (act->menu()) {
                    auditMenuRecursive(act->menu(), pathPrefix + "/" + stripMnemonics(act->text()));
                    continue;
                }
                QString txt = act->text();
                if (txt.trimmed().isEmpty())
                    continue;
                // Skip dynamic window list entries in Windows menu or recent file paths
                QString stripped = stripMnemonics(txt);
                // Dynamic window entries look like "&1 Table1" or window names in workspace
                int spIdx = stripped.indexOf(' ');
                QString afterNum = (spIdx > 0) ? stripped.mid(spIdx + 1).trimmed() : stripped;
                if (mw->alreadyUsedName(stripped) || mw->alreadyUsedName(afterNum)
                    || QFile::exists(stripped)) {
                    continue;
                }
                ++verifiedMenuActions;
                if (!isLocalizedJapaneseOrUniversal(txt)) {
                    untranslatedItems << QString("MenuAction[%1]: '%2'").arg(pathPrefix, txt);
                }
            }
        };

        auto auditCurrentMenuBar = [&]() {
            for (QAction *topAct : mw->menuBar()->actions()) {
                if (!topAct || topAct->isSeparator())
                    continue;
                if (topAct->menu()) {
                    auditMenuRecursive(topAct->menu(), stripMnemonics(topAct->text()));
                } else if (!topAct->text().trimmed().isEmpty()) {
                    ++verifiedMenuActions;
                    if (!isLocalizedJapaneseOrUniversal(topAct->text()))
                        untranslatedItems << QString("TopBarAction: '%1'").arg(topAct->text());
                }
            }
        };

        // Audit menuBar with Table active, Matrix active, and 2D Graph active so all context menus are covered
        Matrix *mJa = mw->newMatrix(4, 4);
        mw->customMenu(tJa);
        auditCurrentMenuBar();
        mw->customMenu(mJa);
        auditCurrentMenuBar();
        mw->activateSubWindow(mlJa);
        mw->customMenu(mlJa);
        auditCurrentMenuBar();
        if (mw->gadgetsMenu)
            auditMenuRecursive(mw->gadgetsMenu, "GadgetsMenu");

        // Audit Project Explorer header labels
        int verifiedExplorerHeaders = 0;
        CHECK_COND(mw->lv.headerItem() != nullptr, "Project Explorer lv.headerItem() is null");
        for (int col = 0; col < 5; ++col) {
            QString hTxt = mw->lv.headerItem()->text(col);
            ++verifiedExplorerHeaders;
            if (!hasJapaneseChars(hTxt))
                untranslatedItems << QString("ExplorerHeader[col=%1]: '%2'").arg(col).arg(hTxt);
        }
        if (mw->folders.headerItem()) {
            QString fTxt = mw->folders.headerItem()->text(0);
            ++verifiedExplorerHeaders;
            if (!hasJapaneseChars(fTxt))
                untranslatedItems << QString("ExplorerFolderHeader: '%1'").arg(fTxt);
        }

        // Audit key dialogs: QTabWidget tab labels + QDialogButtonBox buttons + top-level action buttons
        int verifiedDialogLabels = 0;
        auto auditDialogWidget = [&](QDialog *dlg, const QString &dlgName) {
            if (!dlg)
                return;
            QString wTitle = dlg->windowTitle();
            if (!wTitle.trimmed().isEmpty()) {
                ++verifiedDialogLabels;
                if (!isLocalizedJapaneseOrUniversal(wTitle))
                    untranslatedItems << QString("%1.windowTitle: '%2'").arg(dlgName, wTitle);
            }
            for (QTabWidget *tw : dlg->findChildren<QTabWidget *>()) {
                for (int ti = 0; ti < tw->count(); ++ti) {
                    QString tabTxt = tw->tabText(ti);
                    if (tabTxt.trimmed().isEmpty())
                        continue;
                    ++verifiedDialogLabels;
                    if (!isLocalizedJapaneseOrUniversal(tabTxt))
                        untranslatedItems << QString("%1.Tab[%2]: '%3'").arg(dlgName).arg(ti).arg(tabTxt);
                }
            }
            for (QDialogButtonBox *bb : dlg->findChildren<QDialogButtonBox *>()) {
                for (QAbstractButton *btn : bb->buttons()) {
                    if (!btn || btn->text().trimmed().isEmpty())
                        continue;
                    ++verifiedDialogLabels;
                    if (!isLocalizedJapaneseOrUniversal(btn->text()))
                        untranslatedItems << QString("%1.ButtonBox: '%2'").arg(dlgName, btn->text());
                }
            }
            for (QPushButton *pb : dlg->findChildren<QPushButton *>()) {
                if (!pb)
                    continue;
                // Skip symbol/math-operator keypad or format mini-buttons
                if (pb->parentWidget() && pb->parentWidget()->inherits("TextFormatButtons"))
                    continue;
                QString bTxt = pb->text();
                if (bTxt.trimmed().isEmpty())
                    continue;
                QString strippedBtn = stripMnemonics(bTxt);
                if (strippedBtn == "exp" || strippedBtn == "ln" || strippedBtn == "log"
                    || strippedBtn == "sin" || strippedBtn == "cos" || strippedBtn == "tan"
                    || strippedBtn == "sqrt" || strippedBtn == "abs") {
                    continue;
                }
                ++verifiedDialogLabels;
                if (!isLocalizedJapaneseOrUniversal(bTxt))
                    untranslatedItems << QString("%1.PushButton: '%2'").arg(dlgName, bTxt);
            }
        };

        {
            ConfigDialog cfgDlg(mw);
            auditDialogWidget(&cfgDlg, "ConfigDialog");
        }
        {
            AxesDialog axesDlg;
            axesDlg.setGraph(gJa);
            auditDialogWidget(&axesDlg, "AxesDialog");
        }
        {
            PlotDialog plotDlg(true, mw);
            plotDlg.initFonts(mw->plotTitleFont, mw->plotAxesFont, mw->plotNumbersFont, mw->plotLegendFont);
            plotDlg.setMultiLayer(mlJa);
            auditDialogWidget(&plotDlg, "PlotDialog");
        }
        {
            LayerDialog layerDlg(mw);
            layerDlg.setMultiLayer(mlJa);
            auditDialogWidget(&layerDlg, "LayerDialog");
        }
        {
            FitDialog fitDlg(mw);
            fitDlg.setGraph(gJa);
            auditDialogWidget(&fitDlg, "FitDialog");
        }
        {
            bool auditedPeakDlg = false;
            mw->activateSubWindow(mlJa);
            QApplication::processEvents();
            mw->showPeakAnalyzerDialog(0);
            for (QDialog *d : mw->findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
                if (d && d->isVisible()) {
                    auditDialogWidget(d, "PeakAnalyzerDialog");
                    auditedPeakDlg = true;
                    d->close();
                }
            }
            QApplication::processEvents();
            CHECK_COND(auditedPeakDlg, "PeakAnalyzerDialog was not opened/audited");
        }
        {
            bool auditedPaletteDlg = false;
            // Poll until the dialog is up (a single zero-delay shot could fire before the dialog
            // became the active modal widget and then leave it waiting for input forever).
            QTimer paletteProbe;
            int paletteTries = 0;
            QObject::connect(&paletteProbe, &QTimer::timeout, [&]() {
                if (++paletteTries > 500) {
                    paletteProbe.stop();
                    return;
                }
                if (QWidget *w = QApplication::activeModalWidget()) {
                    if (QDialog *d = qobject_cast<QDialog *>(w)) {
                        paletteProbe.stop();
                        auditDialogWidget(d, "ColorPaletteDialog");
                        auditedPaletteDlg = true;
                        d->reject();
                    }
                }
            });
            paletteProbe.start(20);
            Graph::showColorPaletteEditor(mw, gJa);
            paletteProbe.stop();
            CHECK_COND(auditedPaletteDlg, "ColorPaletteDialog was not opened/audited");
        }

        if (!untranslatedItems.isEmpty()) {
            std::cerr << "[WARN] Untranslated items detected in Suite 20 ("
                      << untranslatedItems.size() << "):\n";
            for (const QString &u : untranslatedItems)
                std::cerr << "  - " << u.toUtf8().constData() << "\n";
        }
        std::cout << "[INFO] Suite 20 Japanese GUI Audit: verifiedMenuActions=" << verifiedMenuActions
                  << ", verifiedExplorerHeaders=" << verifiedExplorerHeaders
                  << ", verifiedDialogLabels=" << verifiedDialogLabels
                  << ", untranslated=" << untranslatedItems.size() << "\n";
        CHECK_COND(verifiedMenuActions >= 80, "Too few menu actions audited in Suite 20");
        CHECK_COND(verifiedExplorerHeaders >= 5, "Too few Project Explorer headers audited in Suite 20");
        CHECK_COND(verifiedDialogLabels >= 40, "Too few dialog tabs/buttons audited in Suite 20");
        CHECK_COND(untranslatedItems.isEmpty(),
                   "Suite 20 detected untranslated English GUI menu/tab/button labels!");
    }

    // =========================================================================
    // 21. Suite 21: Strict Default Arial Font Verification (While UI is Japanese)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 21: Strict Default Arial Font Verification...\n";
        CHECK_COND(mw->appLanguage == "ja", "Suite 21 must run while appLanguage == 'ja'");

        CHECK_COND(mw->plotAxesFont.family() == "Arial", "mw->plotAxesFont.family() != Arial");
        CHECK_COND(mw->plotNumbersFont.family() == "Arial", "mw->plotNumbersFont.family() != Arial");
        CHECK_COND(mw->plotLegendFont.family() == "Arial", "mw->plotLegendFont.family() != Arial");
        CHECK_COND(mw->plotTitleFont.family() == "Arial", "mw->plotTitleFont.family() != Arial");
        CHECK_COND(mw->plot3DAxesFont.family() == "Arial", "mw->plot3DAxesFont.family() != Arial");
        CHECK_COND(mw->plot3DNumbersFont.family() == "Arial", "mw->plot3DNumbersFont.family() != Arial");
        CHECK_COND(mw->plot3DTitleFont.family() == "Arial", "mw->plot3DTitleFont.family() != Arial");

        Table *tAr = mw->newTable(15, 2, "ArialCheckTable", "Arial Plot Data");
        for (int r = 0; r < 15; ++r) {
            double x = double(r) * 2.0;
            tAr->setCell(r, 0, x);
            tAr->setCell(r, 1, 3.0 * x + 7.5);
        }
        MultiLayer *mlAr = mw->multilayerPlot(tAr, QStringList() << "ArialCheckTable_2", Graph::LineSymbols);
        CHECK_COND(mlAr != nullptr && mlAr->activeGraph() != nullptr, "Failed to create 2D plot in Suite 21");
        Graph *gAr = mlAr->activeGraph();
        gAr->setTitle("Thermal Conductivity vs Temperature");
        if (!gAr->legend())
            gAr->newLegend();

        // Add Fit Summary Box on plot
        bool savedPasteFit = mw->pasteFitResultsToPlot;
        mw->pasteFitResultsToPlot = true;
        LinearFit *lfAr = new LinearFit(mw, gAr);
        lfAr->setDataFromCurve("ArialCheckTable_2");
        lfAr->fit();
        lfAr->insertFitSummaryBoxOnGraph();
        delete lfAr;
        mw->pasteFitResultsToPlot = savedPasteFit;

        // Add Reference Line on plot
        Graph::ReferenceLineSpec refAr;
        refAr.orientation = 0;
        refAr.value = 35.0;
        refAr.label = "Threshold";
        gAr->addReferenceLine(refAr);
        gAr->replot();

        // Verify plot title, all 4 axis title fonts, all 4 axis tick number fonts, and all Legend/Fit boxes
        CHECK_COND(gAr->plotWidget()->title().font().family() == "Arial",
                   "2D Graph plot title font family is not Arial");
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            CHECK_COND(gAr->axisTitleFont(a).family() == "Arial",
                       "gAr->axisTitleFont(a) family is not Arial");
            CHECK_COND(gAr->axisFont(a).family() == "Arial",
                       "gAr->axisFont(a) tick number font family is not Arial");
            CHECK_COND(gAr->plotWidget()->axisTitle(a).font().family() == "Arial",
                       "plotWidget()->axisTitle(a) font family is not Arial");
            CHECK_COND(gAr->plotWidget()->axisFont(a).family() == "Arial",
                       "plotWidget()->axisFont(a) font family is not Arial");
        }
        CHECK_COND(gAr->legend() != nullptr && gAr->legend()->font().family() == "Arial",
                   "Graph legend font family is not Arial");

        int checkedTextBoxes = 0;
        for (QwtPlotItem *it : gAr->plotWidget()->itemList()) {
            Legend *txtBox = dynamic_cast<Legend *>(it);
            if (txtBox) {
                ++checkedTextBoxes;
                CHECK_COND(txtBox->font().family() == "Arial",
                           "Graph text/fit-summary Legend box font family is not Arial");
            }
        }
        CHECK_COND(checkedTextBoxes >= 2,
                   "Expected both Legend and Fit Summary Legend boxes on gAr");
        QImage renderImg(600, 450, QImage::Format_ARGB32);
        renderImg.fill(Qt::white);
        QPainter rp(&renderImg);
        gAr->exportPainter(rp, false, QRect(0, 0, 600, 450));
        rp.end();

        std::cout << "[INFO] Suite 21 Arial verified (appLanguage=" << mw->appLanguage.toStdString()
                  << ", text/legend boxes=" << (checkedTextBoxes + 1) << ", axes=4)\n";

        mw->switchToLanguage("en");
        CHECK_COND(mw->appLanguage == "en", "Failed to restore appLanguage to 'en' after Suite 21");
    }

    // =========================================================================
    // 22. Suite 22: OriginPro Page, Layer Linking, Inset Graph, Frame Presets & Reference Lines
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 22: Page View, Layer Linking, Inset Graph, Frame Presets & Reference Lines...\n";

        Table *t22 = mw->newTable(21, 2, "PageLayerTable", "Arrhenius & Inset");
        double sumY22 = 0.0;
        for (int r = 0; r < 21; ++r) {
            double x = 5.0 + double(r) * 36.1; // spans 5.0 .. 727.0
            double y = 10.0 + 2.0 * double(r); // 10, 12, ..., 50 -> exact mean = 30.0
            t22->setCell(r, 0, x);
            t22->setCell(r, 1, y);
            sumY22 += y;
        }
        const double expectedMeanY22 = sumY22 / 21.0;

        MultiLayer *ml22 = mw->multilayerPlot(t22, QStringList() << "PageLayerTable_2", Graph::LineSymbols);
        CHECK_COND(ml22 != nullptr && ml22->activeGraph() != nullptr, "Failed to create ml22");
        Graph *g1 = ml22->activeGraph();

        // 1. Page Physical Size, Page Zoom & Fit Page to Layers
        ml22->setPageSizePhysical(85.0, 85.0, MultiLayer::UnitMm, 300);
        CHECK_COND(ml22->pageUnit() == MultiLayer::UnitMm && ml22->pageDpi() == 300,
                   "setPageSizePhysical did not store UnitMm / 300 DPI");
        CHECK_COND(std::fabs(ml22->pageWidthPhysical() - 85.0) < 1e-6
                           && std::fabs(ml22->pageHeightPhysical() - 85.0) < 1e-6,
                   "setPageSizePhysical 85x85 mm mismatch");
        ml22->setPageZoom(1.25);
        CHECK_COND(std::fabs(ml22->pageZoom() - 1.25) < 1e-6, "setPageZoom(1.25) mismatch");
        ml22->fitPageToLayers();
        CHECK_COND(ml22->width() >= 100 && ml22->height() >= 100, "fitPageToLayers produced invalid size");

        // 2. Frame Presets & Reference Lines (Constant + Statistical "Mean") & MiniToolbar on g1
        g1->applyFramePreset(Graph::FrameBoxInwardTicks);
        CHECK_COND(g1->isAxisBackboneVisible(QwtPlot::xBottom)
                           && g1->isAxisBackboneVisible(QwtPlot::yLeft)
                           && g1->isAxisBackboneVisible(QwtPlot::xTop)
                           && g1->isAxisBackboneVisible(QwtPlot::yRight),
                   "FrameBoxInwardTicks did not enable all 4 axis backbones");

        g1->applyFramePreset(Graph::FrameOpenL);
        CHECK_COND(g1->isAxisBackboneVisible(QwtPlot::xBottom)
                           && g1->isAxisBackboneVisible(QwtPlot::yLeft)
                           && !g1->isAxisBackboneVisible(QwtPlot::xTop)
                           && !g1->isAxisBackboneVisible(QwtPlot::yRight),
                   "FrameOpenL did not hide Top/Right axis backbones");

        g1->clearReferenceLines();
        Graph::ReferenceLineSpec constRef;
        constRef.orientation = 0;
        constRef.value = 22.5;
        constRef.statMode = "Constant";
        constRef.label = "Spec Limit";
        g1->addReferenceLine(constRef);

        Graph::ReferenceLineSpec meanRef;
        meanRef.orientation = 0;
        meanRef.statMode = "Mean";
        g1->addReferenceLine(meanRef);

        QVector<Graph::ReferenceLineSpec> refs = g1->referenceLines();
        CHECK_COND(refs.size() == 2, "Expected 2 reference lines on g1");
        CHECK_COND(std::fabs(refs[0].value - 22.5) < 1e-9, "Constant reference line value != 22.5");
        CHECK_COND(std::fabs(refs[1].value - expectedMeanY22) < 1e-6,
                   "Statistical 'Mean' reference line did not compute exact curve mean 30.0");

        GraphMiniToolbar::setMiniToolbarEnabled(true);
        CHECK_COND(GraphMiniToolbar::isMiniToolbarEnabled(), "GraphMiniToolbar failed to enable");
        GraphMiniToolbar *mtb = GraphMiniToolbar::instance();
        CHECK_COND(mtb != nullptr, "GraphMiniToolbar::instance() returned null");
        mtb->popupForCanvas(g1, QPoint(240, 240));
        CHECK_COND(mtb->currentGraph() == g1 && mtb->currentTarget() == GraphMiniToolbar::TargetCanvas,
                   "GraphMiniToolbar::popupForCanvas state mismatch");
        mtb->hide();

        // 3. Mathematical Linked Layer: 1000/(x+273.15)
        Graph *g2 = ml22->addLinkedLayer(g1, true, false, "1000/(x+273.15)", "y");
        CHECK_COND(g2 != nullptr && g2->linkedParentLayer() == g1, "addLinkedLayer failed");
        g1->setScale(QwtPlot::xBottom, 27.0, 727.0);
        const QwtScaleDiv *topDiv = g2->plotWidget()->axisScaleDiv(QwtPlot::xTop);
        CHECK_COND(topDiv != nullptr, "Linked g2 Top X scaleDiv is null");
        const double expLo = 1000.0 / (727.0 + 273.15); // ~0.99985002 (1.0)
        const double expHi = 1000.0 / (27.0 + 273.15);  // 1000.0 / 300.15 = 3.3316675
        const double gotLo = std::min(topDiv->lowerBound(), topDiv->upperBound());
        const double gotHi = std::max(topDiv->lowerBound(), topDiv->upperBound());
        std::cout << "[INFO] Linked Top X [27..727 C] -> [" << gotLo << ", " << gotHi
                  << "] (expected [" << expLo << ", " << expHi << "])\n";
        CHECK_COND(std::fabs(gotLo - expLo) < 1e-4 && std::fabs(gotLo - 1.0) < 1e-3
                           && std::fabs(gotHi - expHi) < 1e-4,
                   "Linked layer Top X mathematical endpoints mismatch for 1000/(x+273.15)");

        // 4. Zoomed Inset Layer inside g1
        Graph *gInset = ml22->addInsetLayer(g1, QRectF(10.0, 0.0, 20.0, 50.0));
        CHECK_COND(gInset != nullptr && ml22->layersList().size() == 3,
                   "addInsetLayer did not create 3rd layer inside ml22");
        CHECK_COND(gInset->curves() >= 1, "addInsetLayer did not copy curves from g1");
        const QwtScaleDiv *inX = gInset->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        const QwtScaleDiv *inY = gInset->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
        CHECK_COND(inX != nullptr && inY != nullptr
                           && std::fabs(inX->lowerBound() - 10.0) < 1e-5
                           && std::fabs(inX->upperBound() - 30.0) < 1e-5
                           && std::fabs(inY->lowerBound() - 0.0) < 1e-5
                           && std::fabs(inY->upperBound() - 50.0) < 1e-5,
                   "Inset layer zoomed scale bounds mismatch for QRectF(10, 0, 20, 50)");
    }

    // =========================================================================
    // 23. Suite 23: Phase 4 Gadgets & Advanced Regression
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 23: Phase 4 Gadgets & Advanced Regression...\n";

        // 1. RoiGadgetTool::Differentiate on y = 3*x^2 over [0..10], ROI [2..8] -> maxSlope at x=8 is 6*8 = 48
        Table *tDiff = mw->newTable(101, 2, "DiffGadgetTbl", "y=3x^2");
        for (int i = 0; i <= 100; ++i) {
            double x = double(i) * 0.1;
            tDiff->setCell(i, 0, x);
            tDiff->setCell(i, 1, 3.0 * x * x);
        }
        MultiLayer *mlDiff = mw->multilayerPlot(tDiff, QStringList() << "DiffGadgetTbl_2", Graph::Line);
        Graph *gDiff = mlDiff->activeGraph();
        RoiGadgetTool *diffGadget = RoiGadgetTool::attachGadget(mw, gDiff, RoiGadgetTool::Differentiate);
        CHECK_COND(diffGadget != nullptr, "Failed to attach Differentiate gadget");
        diffGadget->setRoiRect(QRectF(2.0, -10.0, 6.0, 400.0)); // x in [2, 8], xmax = 8 -> 6*8 = 48
        double maxSlope = diffGadget->primaryMetric();
        double xAtMax = diffGadget->secondaryMetric();
        std::cout << "[INFO] Differentiate y=3x^2 on [2..8]: maxSlope=" << maxSlope
                  << " at x=" << xAtMax << " (expected 48.0 at x=8.0)\n";
        CHECK_COND(relErrWithin(maxSlope, 48.0, 0.02) && std::fabs(xAtMax - 8.0) < 0.15,
                   "RoiGadgetTool::Differentiate maxSlope mismatch vs analytic 6*xmax");
        RoiGadgetTool::clearGadget(gDiff);

        // 2. RoiGadgetTool::Intersect on y = 2x vs y = 10 - 3x -> exact intersection at (2.0, 4.0)
        Table *tInt = mw->newTable(21, 3, "IntersectGadgetTbl", "Two Lines");
        tInt->setColPlotDesignation(2, SciDAVis::Y);
        for (int i = 0; i <= 20; ++i) {
            double x = double(i) * 0.25; // 0 .. 5
            tInt->setCell(i, 0, x);
            tInt->setCell(i, 1, 2.0 * x);
            tInt->setCell(i, 2, 10.0 - 3.0 * x);
        }
        MultiLayer *mlInt = mw->multilayerPlot(
                tInt, QStringList() << "IntersectGadgetTbl_2" << "IntersectGadgetTbl_3", Graph::Line);
        Graph *gInt = mlInt->activeGraph();
        RoiGadgetTool *intGadget = RoiGadgetTool::attachGadget(mw, gInt, RoiGadgetTool::Intersect);
        CHECK_COND(intGadget != nullptr, "Failed to attach Intersect gadget");
        intGadget->setRoiRect(QRectF(0.5, -5.0, 4.0, 20.0));
        Table *intOutTbl = intGadget->createOutputTable("IntersectHitsTbl");
        CHECK_COND(intGadget->primaryMetric() == 1.0 && intOutTbl != nullptr,
                   "RoiGadgetTool::Intersect did not find exactly 1 intersection");
        double hitX = intOutTbl->cell(0, 2);
        double hitY = intOutTbl->cell(0, 3);
        std::cout << "[INFO] Intersect y=2x vs y=10-3x: count=" << intGadget->primaryMetric()
                  << ", hit=(" << hitX << ", " << hitY << ") (expected 2.0, 4.0)\n";
        CHECK_COND(std::fabs(hitX - 2.0) < 1e-6 && std::fabs(hitY - 4.0) < 1e-6,
                   "RoiGadgetTool::Intersect coordinates mismatch vs (2, 4)");
        RoiGadgetTool::clearGadget(gInt);

        // 3. RoiGadgetTool::Integrate2D on a Matrix / Spectrogram surface
        // z(x,y) = 5.0 + 12.0 on [0..4]x[0..3]: let's use z(x,y) = 10.0 constant + Gaussian bump, or exact planar z = 10.0 + 2*x + 4*y!
        // On ROI [1..3] x [1..3] (width=2, height=2, area=4): mean of (10 + 2x + 4y) is 10 + 4 + 8 = 22 -> Total Volume = 22 * 4 = 88.0!
        // And with a Gaussian peak 20*exp(-((x-2)^2+(y-2)^2)/0.15) on baseline 5.0, Net Volume > 0 and Total Volume = Net + 5*4!
        Matrix *mat2D = mw->newMatrix(41, 41);
        mat2D->setCoordinates(0.0, 4.0, 0.0, 4.0);
        for (int r = 0; r < 41; ++r) {
            double y = 4.0 * double(r) / 40.0;
            for (int c = 0; c < 41; ++c) {
                double x = 4.0 * double(c) / 40.0;
                double z = 5.0 + 25.0 * std::exp(-((x - 2.0) * (x - 2.0) + (y - 2.0) * (y - 2.0)) / 0.16);
                mat2D->setCell(r, c, z);
            }
        }
        MultiLayer *mlSpec = mw->plotSpectrogram(mat2D, Graph::ColorMap);
        CHECK_COND(mlSpec != nullptr && mlSpec->activeGraph() != nullptr, "plotSpectrogram failed in Suite 23");
        Graph *gSpec = mlSpec->activeGraph();
        RoiGadgetTool *int2DGadget = RoiGadgetTool::attachGadget(mw, gSpec, RoiGadgetTool::Integrate2D);
        CHECK_COND(int2DGadget != nullptr, "Failed to attach Integrate2D gadget");
        int2DGadget->setRoiRect(QRectF(0.0, 0.0, 4.0, 4.0));
        double totalVol2D = int2DGadget->primaryMetric();
        double netVol2D = int2DGadget->secondaryMetric();
        // Analytic Gaussian integral on R^2: 25 * pi * 0.16 = 4 * pi = 12.56637; base = 5 * 16 = 80 -> total ~ 92.566
        std::cout << "[INFO] Integrate2D Spectrogram: totalVol=" << totalVol2D
                  << ", netVol=" << netVol2D << " (expected ~92.57, ~12.57)\n";
        CHECK_COND(relErrWithin(totalVol2D, 80.0 + 4.0 * M_PI, 0.02)
                           && relErrWithin(netVol2D, 4.0 * M_PI, 0.03),
                   "RoiGadgetTool::Integrate2D volume mismatch on Matrix Spectrogram");
        RoiGadgetTool::clearGadget(gSpec);

        // 4. PiecewiseAndRobustFit::fitPiecewiseLinear (2-segment bilinear, breakpoint at x=40)
        Table *tPw = mw->newTable(81, 2, "PiecewiseTbl", "Bilinear");
        for (int i = 0; i <= 80; ++i) {
            double x = double(i); // 0 .. 80
            double y = (x <= 40.0) ? (2.0 * x) : (80.0 + 0.5 * (x - 40.0));
            tPw->setCell(i, 0, x);
            tPw->setCell(i, 1, y);
        }
        MultiLayer *mlPw = mw->multilayerPlot(tPw, QStringList() << "PiecewiseTbl_2", Graph::Scatter);
        Graph *gPw = mlPw->activeGraph();
        auto pwRes = PiecewiseAndRobustFit::fitPiecewiseLinear(mw, gPw, "PiecewiseTbl_2", 2, true);
        CHECK_COND(pwRes.converged && pwRes.breakpointsX.size() == 1 && pwRes.slopes.size() == 2,
                   "fitPiecewiseLinear did not converge with 1 breakpoint and 2 slopes");
        std::cout << "[INFO] Piecewise Linear: bpX=" << pwRes.breakpointsX[0]
                  << ", slopes=(" << pwRes.slopes[0] << ", " << pwRes.slopes[1]
                  << "), R^2=" << pwRes.rSquare << "\n";
        CHECK_COND(std::fabs(pwRes.breakpointsX[0] - 40.0) <= 0.2
                           && std::fabs(pwRes.slopes[0] - 2.0) < 0.02
                           && std::fabs(pwRes.slopes[1] - 0.5) < 0.02
                           && pwRes.rSquare > 0.999,
                   "fitPiecewiseLinear parameter accuracy check failed");

        // 5. PiecewiseAndRobustFit::fitRobustOrQuantileLinear (TukeyBisquare with two +9999 outliers, NO masking!)
        Table *tRob = mw->newTable(30, 2, "RobustTbl", "y=3x+5 with +9999 outliers");
        for (int i = 0; i < 30; ++i) {
            double x = double(i);
            double y = 3.0 * x + 5.0 + 0.01 * std::sin(double(i));
            if (i == 7 || i == 21)
                y = 9999.0;
            tRob->setCell(i, 0, x);
            tRob->setCell(i, 1, y);
        }
        MultiLayer *mlRob = mw->multilayerPlot(tRob, QStringList() << "RobustTbl_2", Graph::Scatter);
        Graph *gRob = mlRob->activeGraph();
        auto robRes = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                mw, gRob, "RobustTbl_2", PiecewiseAndRobustFit::TukeyBisquare, 0.5, true);
        std::cout << "[INFO] TukeyBisquare Robust Fit (with two +9999 outliers): slope="
                  << robRes.slope << ", intercept=" << robRes.intercept << "\n";
        CHECK_COND(robRes.converged && std::fabs(robRes.slope - 3.0) < 0.05
                           && std::fabs(robRes.intercept - 5.0) < 0.1,
                   "fitRobustOrQuantileLinear(TukeyBisquare) failed to reject +9999 outliers");

        // 6. PiecewiseAndRobustFit::compareDatasets (identical vs distinct decay curves)
        Table *tCmp = mw->newTable(40, 4, "CompareDecayTbl", "Decay F-test");
        tCmp->setColPlotDesignation(2, SciDAVis::Y);
        tCmp->setColPlotDesignation(3, SciDAVis::Y);
        for (int i = 0; i < 40; ++i) {
            double x = double(i) * 1.5;
            double base = 5.0 + 100.0 * std::exp(-x / 18.0);
            double n1 = 0.35 * std::sin(1.7 * double(i) + 0.3);
            double n2 = 0.35 * std::cos(1.9 * double(i) - 0.2);
            double distinct = 5.0 + 100.0 * std::exp(-x / 6.0) + 0.35 * std::sin(1.3 * double(i));
            tCmp->setCell(i, 0, x);
            tCmp->setCell(i, 1, base + n1);
            tCmp->setCell(i, 2, base + n2);
            tCmp->setCell(i, 3, distinct);
        }
        MultiLayer *mlCmp = mw->multilayerPlot(
                tCmp,
                QStringList() << "CompareDecayTbl_2" << "CompareDecayTbl_3" << "CompareDecayTbl_4",
                Graph::Scatter);
        Graph *gCmp = mlCmp->activeGraph();
        auto cmpSame = PiecewiseAndRobustFit::compareDatasets(
                mw, gCmp, QStringList() << "CompareDecayTbl_2" << "CompareDecayTbl_3", "ExpDec1");
        auto cmpDiff = PiecewiseAndRobustFit::compareDatasets(
                mw, gCmp, QStringList() << "CompareDecayTbl_2" << "CompareDecayTbl_4", "ExpDec1");
        std::cout << "[INFO] compareDatasets(same tau=18): F=" << cmpSame.fStatistic
                  << ", p=" << cmpSame.pValue << ", preferSeparate=" << cmpSame.preferSeparate << "\n";
        std::cout << "[INFO] compareDatasets(tau=18 vs 6): F=" << cmpDiff.fStatistic
                  << ", p=" << cmpDiff.pValue << ", preferSeparate=" << cmpDiff.preferSeparate << "\n";
        CHECK_COND(cmpSame.valid && cmpSame.pValue > 0.1 && !cmpSame.preferSeparate,
                   "compareDatasets falsely rejected shared parameters on identical decay curves");
        CHECK_COND(cmpDiff.valid && cmpDiff.pValue < 0.01 && cmpDiff.preferSeparate,
                   "compareDatasets failed to detect distinct decay time constants");

        // 7. PiecewiseAndRobustFit::fitMultipleLinearRegression on Y = 4 + 2*X1 - 3*X2
        Table *tMlr = mw->newTable(30, 3, "MlrTbl", "Y = 4 + 2*X1 - 3*X2");
        tMlr->setColName(0, "X1");
        tMlr->setColName(1, "X2");
        tMlr->setColName(2, "Y");
        for (int i = 0; i < 30; ++i) {
            double x1 = double(i) * 0.5;
            double x2 = std::sin(double(i) * 0.7) * 4.0 + double(i % 5);
            double y = 4.0 + 2.0 * x1 - 3.0 * x2 + 1e-4 * std::cos(double(i));
            tMlr->setCell(i, 0, x1);
            tMlr->setCell(i, 1, x2);
            tMlr->setCell(i, 2, y);
        }
        auto mlrRes = PiecewiseAndRobustFit::fitMultipleLinearRegression(
                mw, tMlr, "Y", QStringList() << "X1" << "X2", true);
        CHECK_COND(mlrRes.valid && mlrRes.betas.size() == 3 && mlrRes.vif.size() == 2,
                   "fitMultipleLinearRegression returned invalid result sizes");
        std::cout << "[INFO] MLR betas=(" << mlrRes.betas[0] << ", " << mlrRes.betas[1]
                  << ", " << mlrRes.betas[2] << "), R^2=" << mlrRes.rSquare
                  << ", VIF=(" << mlrRes.vif[0] << ", " << mlrRes.vif[1] << ")\n";
        CHECK_COND(std::fabs(mlrRes.betas[0] - 4.0) < 1e-3
                           && std::fabs(mlrRes.betas[1] - 2.0) < 1e-3
                           && std::fabs(mlrRes.betas[2] - (-3.0)) < 1e-3
                           && mlrRes.rSquare > 0.9999
                           && mlrRes.vif[0] >= 1.0 && mlrRes.vif[1] >= 1.0 && mlrRes.vif[0] < 5.0,
                   "fitMultipleLinearRegression accuracy or VIF verification failed");
    }

    // =========================================================================
    // 24. Suite 24: Savitzky-Golay Derivative, Peak Envelope, Decimation,
    //               Normality, Bland-Altman, Two-Way ANOVA & Graph Digitizer
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 24: Signal Derivatives/Envelope/Decimation, Normality/Bland-Altman/2-Way ANOVA & Graph Digitizer...\n";

        // 1. Savitzky-Golay 1st & 2nd Derivative on cubic y = 2*x^3 - 3*x^2 + 4*x + 1
        // Analytic: y'(x) = 6*x^2 - 6*x + 4, y''(x) = 12*x - 6 (exact for polyOrder=3!)
        Table *tSg = mw->newTable(101, 2, "SavGolCubicTbl", "Cubic");
        for (int i = 0; i <= 100; ++i) {
            double x = -2.0 + 0.05 * double(i); // -2.0 .. 3.0
            double y = 2.0 * x * x * x - 3.0 * x * x + 4.0 * x + 1.0;
            tSg->setCell(i, 0, x);
            tSg->setCell(i, 1, y);
        }
        MultiLayer *mlSg = mw->multilayerPlot(tSg, QStringList() << "SavGolCubicTbl_2", Graph::Line);
        Graph *gSg = mlSg->activeGraph();
        QwtPlotCurve *cD1 = AdvancedSignalProc::savitzkyGolayDerivative(
                mw, gSg, "SavGolCubicTbl_2", 1, 9, 3);
        QwtPlotCurve *cD2 = AdvancedSignalProc::savitzkyGolayDerivative(
                mw, gSg, "SavGolCubicTbl_2", 2, 9, 3);
        CHECK_COND(cD1 != nullptr && cD2 != nullptr && cD1->dataSize() == 101 && cD2->dataSize() == 101,
                   "savitzkyGolayDerivative failed on cubic");
        // Check interior index i=50 (x = 0.5): y'(0.5) = 6*(0.25) - 3 + 4 = 2.5; y''(0.5) = 6 - 6 = 0;
        // and index i=70 (x = 1.5): y'(1.5) = 6*(2.25) - 9 + 4 = 8.5; y''(1.5) = 18 - 6 = 12.0
        std::cout << "[INFO] SavGol at x=1.5: d1=" << cD1->y(70) << " (exp 8.5), d2="
                  << cD2->y(70) << " (exp 12.0)\n";
        CHECK_COND(std::fabs(cD1->y(50) - 2.5) < 1e-4 && std::fabs(cD1->y(70) - 8.5) < 1e-4
                           && std::fabs(cD2->y(70) - 12.0) < 1e-3,
                   "savitzkyGolayDerivative 1st/2nd derivative mismatch vs analytic cubic");

        // 2. Peak Envelope (EnvelopeBoth) on AM modulated carrier A(x)*sin(omega*x) + Decimation (factor=4)
        const int nAm = 400;
        Table *tAm = mw->newTable(nAm, 2, "AmEnvTbl", "AM Carrier");
        for (int i = 0; i < nAm; ++i) {
            double x = double(i) * 0.01; // 0 .. 3.99
            double env = 2.0 + 0.8 * std::cos(2.0 * M_PI * 0.5 * x);
            double y = env * std::sin(2.0 * M_PI * 12.5 * x);
            tAm->setCell(i, 0, x);
            tAm->setCell(i, 1, y);
        }
        MultiLayer *mlAm = mw->multilayerPlot(tAm, QStringList() << "AmEnvTbl_2", Graph::Line);
        Graph *gAm = mlAm->activeGraph();
        QwtPlotCurve *lowerEnvCurve = nullptr;
        QwtPlotCurve *upperEnvCurve = AdvancedSignalProc::computePeakEnvelope(
                mw, gAm, "AmEnvTbl_2", AdvancedSignalProc::EnvelopeBoth, 3, &lowerEnvCurve);
        CHECK_COND(upperEnvCurve != nullptr && lowerEnvCurve != nullptr
                           && upperEnvCurve->dataSize() == nAm && lowerEnvCurve->dataSize() == nAm,
                   "computePeakEnvelope(EnvelopeBoth) did not return upper & lower curves");
        double envErrSq = 0.0;
        int envCnt = 0;
        for (int i = 40; i < nAm - 40; ++i) {
            double x = upperEnvCurve->x(i);
            double expEnv = 2.0 + 0.8 * std::cos(2.0 * M_PI * 0.5 * x);
            double du = upperEnvCurve->y(i) - expEnv;
            double dl = lowerEnvCurve->y(i) - (-expEnv);
            envErrSq += 0.5 * (du * du + dl * dl);
            ++envCnt;
        }
        double envRms = std::sqrt(envErrSq / double(envCnt));
        std::cout << "[INFO] PeakEnvelope(EnvelopeBoth) interior RMS error = " << envRms << "\n";
        CHECK_COND(envRms < 0.08, "computePeakEnvelope RMS error too large on AM carrier");

        QwtPlotCurve *decCurve = AdvancedSignalProc::decimateCurve(mw, gAm, "AmEnvTbl_2", 4, true);
        CHECK_COND(decCurve != nullptr && decCurve->dataSize() == nAm / 4,
                   "decimateCurve(factor=4) output length mismatch");

        // 3. Normality Test (Gaussian passes p > 0.05 vs strongly bimodal/exponential rejected p < 0.05)
        // Use inverse normal CDF (Acklam / Box-Muller deterministic quantiles) for clean Gaussian sample
        const int nNorm = 60;
        Table *tNorm = mw->newTable(nNorm, 2, "NormalityTbl", "Gauss vs Bimodal");
        tNorm->setColName(0, "GaussianSample");
        tNorm->setColName(1, "ExponentialBimodal");
        for (int i = 0; i < nNorm; ++i) {
            // Deterministic Box-Muller pair from Halton sequence (bases 2 and 3)
            auto halton = [](int index, int base) {
                double f = 1.0, r = 0.0;
                int k = index;
                while (k > 0) {
                    f /= double(base);
                    r += f * double(k % base);
                    k /= base;
                }
                return r;
            };
            double u1 = std::max(1e-6, std::min(1.0 - 1e-6, halton(i + 1, 2)));
            double u2 = halton(i + 1, 3);
            double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
            tNorm->setCell(i, 0, 50.0 + 10.0 * z);
            tNorm->setCell(i, 1, (i < 42) ? (1.0 + 0.1 * double(i % 5)) : (100.0 + double(i)));
        }
        auto normPass = GraphStatsAndSurfaceFit::runNormalityTest(mw, tNorm, "GaussianSample", nullptr);
        auto normFail = GraphStatsAndSurfaceFit::runNormalityTest(mw, tNorm, "ExponentialBimodal", nullptr);
        std::cout << "[INFO] Normality(Gaussian): W=" << normPass.shapiroWilkW
                  << ", p=" << normPass.shapiroWilkP << ", normal=" << normPass.isNormalAt005 << "\n";
        std::cout << "[INFO] Normality(Bimodal):  W=" << normFail.shapiroWilkW
                  << ", p=" << normFail.shapiroWilkP << ", normal=" << normFail.isNormalAt005 << "\n";
        CHECK_COND(normPass.shapiroWilkP > 0.05 && normPass.isNormalAt005,
                   "runNormalityTest rejected valid Gaussian sample");
        CHECK_COND(normFail.shapiroWilkP < 0.05 && !normFail.isNormalAt005,
                   "runNormalityTest failed to reject strongly non-normal sample");

        // 4. Bland-Altman Plot (exact Mean Bias & +/- 1.96 SD Limits of Agreement)
        const int nBa = 25;
        Table *tBa = mw->newTable(nBa, 2, "BlandAltmanTbl", "Method A vs B");
        tBa->setColName(0, "MethodA");
        tBa->setColName(1, "MethodB");
        QVector<double> diffs(nBa);
        double diffSum = 0.0;
        for (int i = 0; i < nBa; ++i) {
            double a = 10.0 + 2.0 * double(i);
            double d = 1.5 + 0.4 * std::sin(double(i) * 1.1); // A - B or B - A
            double b = a - d;
            tBa->setCell(i, 0, a);
            tBa->setCell(i, 1, b);
            diffs[i] = d;
            diffSum += d;
        }
        const double meanD = diffSum / double(nBa);
        double varD = 0.0;
        for (int i = 0; i < nBa; ++i)
            varD += (diffs[i] - meanD) * (diffs[i] - meanD);
        const double sdD = std::sqrt(varD / double(nBa - 1));

        double biasOut = 0.0, loaLo = 0.0, loaHi = 0.0;
        MultiLayer *mlBa = GraphStatsAndSurfaceFit::plotBlandAltman(
                mw, tBa, "MethodA", "MethodB", &biasOut, &loaLo, &loaHi);
        CHECK_COND(mlBa != nullptr, "plotBlandAltman returned null MultiLayer");
        // Accept either (A - B) or (B - A) convention as long as |biasOut| == |meanD| and span == 2 * 1.96 * sdD
        std::cout << "[INFO] Bland-Altman: bias=" << biasOut << ", LoA=[" << loaLo << ", " << loaHi
                  << "] (expected |bias|=" << std::fabs(meanD) << ", 1.96*SD=" << 1.96 * sdD << ")\n";
        CHECK_COND(std::fabs(std::fabs(biasOut) - std::fabs(meanD)) < 1e-6
                           && std::fabs((biasOut - loaLo) - 1.96 * sdD) < 1e-4
                           && std::fabs((loaHi - biasOut) - 1.96 * sdD) < 1e-4,
                   "plotBlandAltman Mean Bias or +/-1.96 SD LoA mismatch");

        // 5. Two-Way ANOVA (2x2 factorial with strong Factor A, Factor B, and A x B interaction)
        Table *tAnova2 = mw->newTable(24, 3, "TwoWayAnovaTbl", "2x2 Factorial");
        tAnova2->setColName(0, "TempLevel");
        tAnova2->setColumnType(0, SciDAVis::ColumnMode::Text);
        tAnova2->setColName(1, "Catalyst");
        tAnova2->setColumnType(1, SciDAVis::ColumnMode::Text);
        tAnova2->setColName(2, "Yield");
        int rowIdx = 0;
        for (int a = 0; a < 2; ++a) {
            for (int b = 0; b < 2; ++b) {
                double cellMean = 20.0 + 15.0 * double(a) + 12.0 * double(b) + 25.0 * double(a * b);
                for (int rep = 0; rep < 6; ++rep) {
                    tAnova2->setText(rowIdx, 0, a == 0 ? "LowTemp" : "HighTemp");
                    tAnova2->setText(rowIdx, 1, b == 0 ? "CatA" : "CatB");
                    tAnova2->setCell(rowIdx, 2, cellMean + 0.3 * double(rep - 2.5));
                    ++rowIdx;
                }
            }
        }
        auto a2Res = GraphStatsAndSurfaceFit::runTwoWayANOVA(
                mw, tAnova2, "TempLevel", "Catalyst", "Yield", true);
        std::cout << "[INFO] Two-Way ANOVA: pA=" << a2Res.pFactorA << ", pB=" << a2Res.pFactorB
                  << ", pAxB=" << a2Res.pInteraction << "\n";
        CHECK_COND(a2Res.valid && a2Res.pFactorA < 0.001 && a2Res.pFactorB < 0.001
                           && a2Res.pInteraction < 0.001 && a2Res.interactionGraph != nullptr,
                   "runTwoWayANOVA failed to detect significant A, B, and A x B effects");

        // 6. GraphDigitizer: Linear & Log10 round-trip + synthetic QImage(500, 400) auto-trace y = 0.5*x + 10
        GraphDigitizer::AxisCalibration calLin;
        calLin.pixelX1 = QPointF(60.0, 350.0);
        calLin.valueX1 = 0.0;
        calLin.pixelX2 = QPointF(460.0, 350.0); // 400 px for X in [0..100] -> 4 px/unit
        calLin.valueX2 = 100.0;
        calLin.logX = false;
        calLin.pixelY1 = QPointF(60.0, 350.0);
        calLin.valueY1 = 0.0;
        calLin.pixelY2 = QPointF(60.0, 50.0);   // 300 px upward for Y in [0..100] -> 3 px/unit
        calLin.valueY2 = 100.0;
        calLin.logY = false;

        QPointF testDataLin(42.5, 31.25);
        QPointF pxLin = GraphDigitizer::mapDataToPixel(testDataLin, calLin);
        QPointF backLin = GraphDigitizer::mapPixelToData(pxLin, calLin);
        CHECK_COND(std::fabs(backLin.x() - testDataLin.x()) < 1e-9
                           && std::fabs(backLin.y() - testDataLin.y()) < 1e-9,
                   "GraphDigitizer Linear mapDataToPixel/mapPixelToData round-trip failed");

        GraphDigitizer::AxisCalibration calLog = calLin;
        calLog.valueX1 = 1.0;
        calLog.valueX2 = 1000.0;
        calLog.logX = true;
        calLog.valueY1 = 0.1;
        calLog.valueY2 = 1000.0;
        calLog.logY = true;
        QPointF testDataLog(31.6227766, 25.0);
        QPointF pxLog = GraphDigitizer::mapDataToPixel(testDataLog, calLog);
        QPointF backLog = GraphDigitizer::mapPixelToData(pxLog, calLog);
        CHECK_COND(relErrWithin(backLog.x(), testDataLog.x(), 1e-9)
                           && relErrWithin(backLog.y(), testDataLog.y(), 1e-9),
                   "GraphDigitizer Log10 mapDataToPixel/mapPixelToData round-trip failed");

        // Render synthetic 500x400 white image with pure red curve y = 0.5*x + 10
        QImage digImg(500, 400, QImage::Format_RGB32);
        digImg.fill(Qt::white);
        {
            QPainter p(&digImg);
            p.setRenderHint(QPainter::Antialiasing, true);
            // Black axes frame (will be ignored by red color tracer)
            p.setPen(QPen(Qt::black, 1));
            p.drawLine(QPointF(60.0, 50.0), QPointF(60.0, 350.0));
            p.drawLine(QPointF(60.0, 350.0), QPointF(460.0, 350.0));
            // Pure red line y = 0.5*x + 10 for x in [5..95]
            p.setPen(QPen(QColor(255, 0, 0), 3, Qt::SolidLine, Qt::RoundCap));
            QPointF pStart = GraphDigitizer::mapDataToPixel(QPointF(5.0, 0.5 * 5.0 + 10.0), calLin);
            QPointF pEnd = GraphDigitizer::mapDataToPixel(QPointF(95.0, 0.5 * 95.0 + 10.0), calLin);
            p.drawLine(pStart, pEnd);
        }

        QVector<QPointF> tracedPts = GraphDigitizer::autoTraceCurveByColor(
                digImg, calLin, QColor(255, 0, 0), 40, 80);
        CHECK_COND(tracedPts.size() >= 50, "autoTraceCurveByColor extracted too few points");

        MultiLayer *digPlot = nullptr;
        Table *digTbl = GraphDigitizer::exportPointsToTable(mw, tracedPts, "DigitizedRedLine", &digPlot);
        CHECK_COND(digTbl != nullptr && digPlot != nullptr && digTbl->numRows() == tracedPts.size(),
                   "exportPointsToTable failed to create Table & MultiLayer");

        double maxPixErrY = 0.0;
        for (int i = 0; i < digTbl->numRows(); ++i) {
            double x = digTbl->cell(i, 0);
            double y = digTbl->cell(i, 1);
            // Skip extreme cap endpoints (x near 5.0 or 95.0) if within 1px of cap
            if (x < 7.0 || x > 93.0)
                continue;
            double expectedY = 0.5 * x + 10.0;
            QPointF actualPix = GraphDigitizer::mapDataToPixel(QPointF(x, y), calLin);
            QPointF expectedPix = GraphDigitizer::mapDataToPixel(QPointF(x, expectedY), calLin);
            double pixErr = std::fabs(actualPix.y() - expectedPix.y());
            if (pixErr > maxPixErrY)
                maxPixErrY = pixErr;
        }
        std::cout << "[INFO] GraphDigitizer autoTrace points=" << tracedPts.size()
                  << ", max pixel Y error=" << maxPixErrY << " px (< 1.0 px)\n";
        CHECK_COND(maxPixErrY < 1.0,
                   "GraphDigitizer extracted (X,Y) exceeded 1.0 pixel tolerance vs y = 0.5*x + 10");
    }

    // =========================================================================
    // 25. Suite 25: Exhaustive Codebase-Wide Japanese tr(...) Audit + Phase 4/5 Dialog Localization
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 25: Exhaustive Japanese tr(...) Dictionary & Phase 4/5 Menu/Dialog Audit...\n";

        Table *t25 = mw->newTable(12, 2, "JaPhase5Tbl", "JaPhase5");
        for (int r = 0; r < 12; ++r) {
            t25->setCell(r, 0, double(r));
            t25->setCell(r, 1, 2.0 * double(r) + 1.0);
        }
        MultiLayer *ml25 = mw->multilayerPlot(t25, QStringList() << "JaPhase5Tbl_2", Graph::LineSymbols);
        CHECK_COND(ml25 != nullptr && ml25->activeGraph() != nullptr, "Failed to create ml25");
        Graph *g25 = ml25->activeGraph();
        if (!g25->legend())
            g25->newLegend();

        mw->switchToLanguage("ja");
        CHECK_COND(mw->appLanguage == "ja", "Suite 25 failed to switch appLanguage to 'ja'");
        std::cout << "[INFO] Japanese dictionary total curated entries = "
                  << ToriGraphFullJapaneseTranslator::dictionarySize() << "\n";
        CHECK_COND(ToriGraphFullJapaneseTranslator::dictionarySize() >= 3500,
                   "Expected >= 3500 curated entries in ToriGraphFullJapaneseTranslator");

        auto hasJaChars25 = [](const QString &s) -> bool {
            for (int i = 0; i < s.size(); ++i) {
                ushort u = s.at(i).unicode();
                if ((u >= 0x3040 && u <= 0x30FF) || (u >= 0x4E00 && u <= 0x9FFF)
                    || (u >= 0x3400 && u <= 0x4DBF)) {
                    return true;
                }
            }
            return false;
        };

        auto stripMnem25 = [](QString s) -> QString {
            s.replace("&&", "\x01");
            int idx = 0;
            while ((idx = s.indexOf("(&", idx)) >= 0) {
                if (idx + 3 < s.size() && s.at(idx + 3) == QChar(')'))
                    s.remove(idx, 4);
                else
                    idx += 2;
            }
            s.remove('&');
            s.replace('\x01', '&');
            return s.trimmed();
        };

        const QSet<QString> universalTokens25 = {
            "English", "English (&E)", "OK", "FFT", "IFFT", "2D", "3D",
            "AIC", "BIC", "AIC/BIC", "PCA", "ROC", "CSV", "SVG", "PDF", "EPS", "EMF",
            "ASCII", "TeX", "LaTeX", "LOWESS", "LOESS", "STFT", "IIR", "FIR",
            "CWT", "DWT", "PSD", "MLR", "ANOVA", "Savitzky-Golay",
            "X", "Y", "Z", "A", "B", "C", "R^2", "R²", "p", "t", "F",
            "X=0", "Y=0"
        };

        auto isJaOrUniversal25 = [&](const QString &raw) -> bool {
            QString clean = stripMnem25(raw);
            if (clean.isEmpty() || hasJaChars25(clean) || universalTokens25.contains(clean))
                return true;
            for (int i = 0; i < clean.size(); ++i) {
                ushort u = clean.at(i).unicode();
                if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))
                    return false;
            }
            return true;
        };

        // 1. Representative strings across ALL Phase 4 & Phase 5 modules
        const QStringList requiredJaLookupStrings = {
            "Piecewise Linear Breakpoint Fit",
            "Graph Image Digitizer",
            "Normality Test (Shapiro-Wilk / KS / Anderson-Darling)",
            "Delta Cursor",
            "Delta Cursor (C1-C2)...",
            "Arrhenius Plot (Activation Energy)",
            "Arrhenius Plot (Activation Energy)...",
            "Tauc Plot (Optical Bandgap)...",
            "Weibull Probability Plot",
            "Weibull Probability Plot...",
            "Presentation Slide (28 pt Arial)",
            "ISIJ Journal (85 mm, 600 DPI)",
            "Duplicate (Batch Plotting)...",
            "Copy Format",
            "Paste Format",
            "Long Name",
            "Units",
            "Axis Break",
            "Enable Axis Break",
            "Divide by Factor:",
            "Reconstruct Legend",
            "Linear Constraints:",
            "Run Bootstrap CI",
            "Binary Logistic Regression",
            "Fill Area Between Curves...",
            "Outlier Test (Grubbs / ESD / Dixon)...",
            "Mann-Whitney U Test",
            "Kruskal-Wallis ANOVA",
            "Auto (Shapiro-Wilk Smart Switch)",
            "Paired Comparison Plot (Before-After)...",
            "Correlation Matrix Heatmap...",
            "K-Means Clustering...",
            "Hierarchical Cluster Dendrogram...",
            "Waterfall / Stack Offset...",
            "Reset Stack Offsets",
            "Export DPI:",
            "Transparency"
        };

        QStringList untranslated25;
        for (const QString &srcEn : requiredJaLookupStrings) {
            QString ja = ToriGraphFullJapaneseTranslator::lookupJapanese(srcEn);
            if (ja.isEmpty() || !hasJaChars25(ja)) {
                untranslated25 << QString("lookupJapanese('%1') -> '%2'").arg(srcEn, ja);
            }
        }

        // 2. Audit MenuBar + Curve Context Menu + Marker Context Menu + Phase 5 Dialogs
        int auditedCount25 = 0;
        QSet<const QMenu *> seenMenus25;
        std::function<void(QMenu *, const QString &)> auditMenu25 =
                [&](QMenu *m, const QString &prefix) {
            if (!m || seenMenus25.contains(m))
                return;
            if (prefix.contains(QString::fromUtf8("最近のプロジェクト"))
                || prefix.contains("Recent Projects"))
                return;
            seenMenus25.insert(m);
            if (!m->title().isEmpty()) {
                ++auditedCount25;
                if (!isJaOrUniversal25(m->title()))
                    untranslated25 << QString("MenuTitle[%1]: '%2'").arg(prefix, m->title());
            }
            for (QAction *a : m->actions()) {
                if (!a || a->isSeparator())
                    continue;
                if (a->menu()) {
                    auditMenu25(a->menu(), prefix + "/" + stripMnem25(a->text()));
                    continue;
                }
                QString tStr = a->text().trimmed();
                if (tStr.isEmpty())
                    continue;
                QString stripped = stripMnem25(tStr);
                int sp = stripped.indexOf(' ');
                QString afterSp = (sp > 0) ? stripped.mid(sp + 1).trimmed() : stripped;
                if (mw->alreadyUsedName(stripped) || mw->alreadyUsedName(afterSp)
                    || g25->analysableCurvesList().contains(stripped)
                    || QFile::exists(stripped))
                    continue;
                ++auditedCount25;
                if (!isJaOrUniversal25(tStr))
                    untranslated25 << QString("MenuAction[%1]: '%2'").arg(prefix, tStr);
            }
        };

        Matrix *m25 = mw->newMatrix(4, 4);
        for (MyWidget *wCtx : QList<MyWidget *>{ t25, m25, ml25 }) {
            mw->activateSubWindow(wCtx);
            mw->customMenu(wCtx);
            for (QAction *topA : mw->menuBar()->actions()) {
                if (topA && topA->menu())
                    auditMenu25(topA->menu(), stripMnem25(topA->text()));
            }
        }
        if (mw->gadgetsMenu)
            auditMenu25(mw->gadgetsMenu, "GadgetsMenu");

        int firstCurveKey = g25->curveKey(0);
        if (QMenu *curveMenu = mw->showCurveContextMenuImpl(firstCurveKey)) {
            auditMenu25(curveMenu, "CurveContextMenu");
            delete curveMenu;
        }

        // Audit Phase 5 upgraded dialogs (AxesDialog, PlotDialog, FitDialog, ImageExportDialog, AssociationsDialog)
        auto auditDlg25 = [&](QDialog *dlg, const QString &name) {
            if (!dlg)
                return;
            if (!dlg->windowTitle().trimmed().isEmpty()) {
                ++auditedCount25;
                if (!isJaOrUniversal25(dlg->windowTitle()))
                    untranslated25 << QString("%1.windowTitle: '%2'").arg(name, dlg->windowTitle());
            }
            for (QTabWidget *tw : dlg->findChildren<QTabWidget *>()) {
                for (int i = 0; i < tw->count(); ++i) {
                    ++auditedCount25;
                    if (!isJaOrUniversal25(tw->tabText(i)))
                        untranslated25 << QString("%1.Tab[%2]: '%3'").arg(name).arg(i).arg(tw->tabText(i));
                }
            }
            for (QCheckBox *cb : dlg->findChildren<QCheckBox *>()) {
                if (!cb->text().trimmed().isEmpty()) {
                    ++auditedCount25;
                    if (!isJaOrUniversal25(cb->text()))
                        untranslated25 << QString("%1.CheckBox: '%2'").arg(name, cb->text());
                }
            }
        };

        {
            AxesDialog axDlg;
            axDlg.setGraph(g25);
            auditDlg25(&axDlg, "AxesDialog");
        }
        {
            PlotDialog plDlg(true, mw);
            plDlg.initFonts(mw->plotTitleFont, mw->plotAxesFont, mw->plotNumbersFont, mw->plotLegendFont);
            plDlg.setMultiLayer(ml25);
            auditDlg25(&plDlg, "PlotDialog");
        }
        {
            FitDialog ftDlg(mw);
            ftDlg.setGraph(g25);
            auditDlg25(&ftDlg, "FitDialog");
        }
        {
            ImageExportDialog expDlg(mw, true, true);
            auditDlg25(&expDlg, "ImageExportDialog");
        }
        {
            AssociationsDialog assocDlg(mw);
            assocDlg.setGraph(g25);
            auditDlg25(&assocDlg, "AssociationsDialog");
        }

        if (!untranslated25.isEmpty()) {
            std::cerr << "[WARN] Untranslated items in Suite 25 (" << untranslated25.size() << "):\n";
            for (const QString &u : untranslated25)
                std::cerr << "  - " << u.toUtf8().constData() << "\n";
        }
        std::cout << "[INFO] Suite 25 audited " << requiredJaLookupStrings.size()
                  << " Phase 4/5 lookup strings + " << auditedCount25
                  << " menu/context/dialog items, untranslated=" << untranslated25.size() << "\n";
        CHECK_COND(untranslated25.isEmpty(),
                   "Suite 25 found untranslated English strings in Phase 4/5 localization audit!");

        mw->switchToLanguage("en");
        CHECK_COND(mw->appLanguage == "en", "Failed to restore 'en' at end of Suite 25");
    }

    // =========================================================================
    // 26. Suite 26: Worksheet Long Name & Units -> Auto Axis Title, Live F(x)= Auto-Recalc,
    //               StylePresentationSlide28pt, Copy/Paste Format & Batch Plotting
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 26: Worksheet Long Name/Units, Live F(x)=, 28pt Slide Preset, Copy/Paste Format & Batch Plot...\n";

        // 1. Worksheet Long Name & Units -> Automatic Graph Axis Titles in Arial
        Table *tTensile = mw->newTable(21, 2, "TensileSpecA", "Tensile A");
        tTensile->setColName(0, "Strain");
        tTensile->setColPlotDesignation(0, SciDAVis::X);
        tTensile->column(0)->setLongName("Nominal Strain");
        tTensile->column(0)->setUnits("%");

        tTensile->setColName(1, "Stress");
        tTensile->setColPlotDesignation(1, SciDAVis::Y);
        tTensile->column(1)->setLongName("Nominal Stress");
        tTensile->column(1)->setUnits("MPa");

        CHECK_COND(tTensile->column(0)->formattedAxisTitle(Column::UnitFormat::Paren) == "Nominal Strain (%)",
                   "Column::formattedAxisTitle(Paren) mismatch for Strain (%)");
        CHECK_COND(tTensile->column(1)->formattedAxisTitle(Column::UnitFormat::Paren) == "Nominal Stress (MPa)",
                   "Column::formattedAxisTitle(Paren) mismatch for Stress (MPa)");
        CHECK_COND(tTensile->column(1)->formattedAxisTitle(Column::UnitFormat::ISIJSlash) == "Nominal Stress / MPa",
                   "Column::formattedAxisTitle(ISIJSlash) mismatch for Stress / MPa");

        for (int r = 0; r < 21; ++r) {
            double eps = double(r) * 0.5;
            tTensile->setCell(r, 0, eps);
            tTensile->setCell(r, 1, 520.0 * (1.0 - std::exp(-0.35 * eps)));
        }

        MultiLayer *mlTensile = mw->multilayerPlot(
                tTensile, QStringList() << (tTensile->name() + "_Stress"), Graph::LineSymbols);
        CHECK_COND(mlTensile != nullptr && mlTensile->activeGraph() != nullptr,
                   "Failed to plot TensileSpecA");
        Graph *gTensile = mlTensile->activeGraph();
        QString xTitleAuto = gTensile->plotWidget()->axisTitle(QwtPlot::xBottom).text();
        QString yTitleAuto = gTensile->plotWidget()->axisTitle(QwtPlot::yLeft).text();
        std::cout << "[INFO] Auto Axis Titles: X='" << xTitleAuto.toStdString()
                  << "', Y='" << yTitleAuto.toStdString() << "'\n";
        CHECK_COND(xTitleAuto == "Nominal Strain / %" && yTitleAuto == "Nominal Stress / MPa",
                   "multilayerPlot did not auto-populate ISIJ Long Name / Units on X/Y axes");
        CHECK_COND(gTensile->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                           && gTensile->axisTitleFont(QwtPlot::yLeft).family() == "Arial",
                   "Auto-populated axis titles did not preserve Arial font");

        // 2. Live F(x)= Auto-Recalc without manual row selection
        Table *tFx = mw->newTable(8, 2, "LiveFxTable", "Live F(x)=");
        tFx->setColName(0, "A");
        tFx->setColName(1, "B");
        for (int r = 0; r < 8; ++r)
            tFx->setCell(r, 0, double(r + 1)); // 1..8
        tFx->deselectAll(); // 0 selected rows -> tests P0 recalculate() fix!
        tFx->setCommand(1, "col(\"A\")*3.0");
        CHECK_COND(tFx->recalculate(1, false), "Table::recalculate(1, false) failed with no selection");
        CHECK_COND(std::fabs(tFx->cell(0, 1) - 3.0) < 1e-9
                           && std::fabs(tFx->cell(5, 1) - 18.0) < 1e-9,
                   "Initial col(\"A\")*3.0 values incorrect");

        // Modify cell in col A WITHOUT calling recalculate() -> live auto-recalc!
        tFx->setCell(0, 0, 100.0);
        tFx->setCell(5, 0, 25.0);
        QApplication::processEvents();
        std::cout << "[INFO] Live F(x)= auto-recalc: row0=" << tFx->cell(0, 1)
                  << " (exp 300), row5=" << tFx->cell(5, 1) << " (exp 75)\n";
        CHECK_COND(std::fabs(tFx->cell(0, 1) - 300.0) < 1e-9
                           && std::fabs(tFx->cell(5, 1) - 75.0) < 1e-9,
                   "Formula column col(\"A\")*3.0 did not auto-recalculate when col A changed!");

        // 3. Presentation Slide Preset (28 pt Arial Regular on BOTH axis titles & tick numbers, 1:1 frame)
        mlTensile->show();
        QApplication::processEvents();
        mlTensile->applyStylePreset(MultiLayer::StylePresentationSlide28pt);
        QApplication::processEvents();

        QFont slideXTitleFnt = gTensile->axisTitleFont(QwtPlot::xBottom);
        QFont slideYTitleFnt = gTensile->axisTitleFont(QwtPlot::yLeft);
        QFont slideXTickFnt = gTensile->axisFont(QwtPlot::xBottom);
        QFont slideYTickFnt = gTensile->axisFont(QwtPlot::yLeft);
        std::cout << "[INFO] StylePresentationSlide28pt: titleFont=" << slideXTitleFnt.family().toStdString()
                  << " " << slideXTitleFnt.pointSize() << "pt bold=" << slideXTitleFnt.bold()
                  << ", tickFont=" << slideXTickFnt.family().toStdString()
                  << " " << slideXTickFnt.pointSize() << "pt bold=" << slideXTickFnt.bold() << "\n";
        CHECK_COND(slideXTitleFnt.family() == "Arial" && slideXTitleFnt.pointSize() == 28 && !slideXTitleFnt.bold()
                           && slideYTitleFnt.family() == "Arial" && slideYTitleFnt.pointSize() == 28 && !slideYTitleFnt.bold(),
                   "StylePresentationSlide28pt axis title font is not Arial 28 pt Regular");
        CHECK_COND(slideXTickFnt.family() == "Arial" && slideXTickFnt.pointSize() == 28 && !slideXTickFnt.bold()
                           && slideYTickFnt.family() == "Arial" && slideYTickFnt.pointSize() == 28 && !slideYTickFnt.bold(),
                   "StylePresentationSlide28pt tick number font is not Arial 28 pt Regular");
        CHECK_COND(mlTensile->keepAspectRatio()
                           && std::fabs(mlTensile->targetAspectRatio() - 1.0) < 1e-6,
                   "StylePresentationSlide28pt did not lock 1:1 square aspect ratio");

        // Add a ReferenceLine before Copy Format / Batch Clone
        Graph::ReferenceLineSpec yieldRef;
        yieldRef.orientation = 0;
        yieldRef.value = 350.0;
        yieldRef.label = "Yield";
        gTensile->addReferenceLine(yieldRef);

        // 4. Copy Graph Format -> Paste Graph Format onto a 2nd graph
        MultiLayer *mlFmtTarget = mw->multilayerPlot(
                tFx, QStringList() << (tFx->name() + "_B"), Graph::Line);
        CHECK_COND(mlFmtTarget != nullptr && mlFmtTarget->activeGraph() != nullptr,
                   "Failed to create mlFmtTarget");
        mw->activateSubWindow(mlTensile);
        mw->copyGraphFormat();
        mw->activateSubWindow(mlFmtTarget);
        mw->pasteGraphFormat();

        Graph *gFmtTarget = mlFmtTarget->activeGraph();
        CHECK_COND(gFmtTarget->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                           && gFmtTarget->axisTitleFont(QwtPlot::xBottom).pointSize() == 28
                           && gFmtTarget->axisFont(QwtPlot::xBottom).pointSize() == 28
                           && gFmtTarget->referenceLines().isEmpty(),
                   "copyGraphFormat -> safe pasteGraphFormat did not transfer 28pt Arial fonts cleanly");
        mw->pasteGraphFormatWithScope(ApplicationWindow::FormatAll);
        CHECK_COND(gFmtTarget->referenceLines().size() == 1,
                   "pasteGraphFormatWithScope(FormatAll) did not transfer reference lines");

        // 5. Batch Plotting (batchCloneGraphToTable) onto a 2nd Table
        Table *tTensileB = mw->newTable(21, 2, "TensileSpecB", "Tensile B");
        tTensileB->setColName(0, "Strain");
        tTensileB->setColPlotDesignation(0, SciDAVis::X);
        tTensileB->column(0)->setLongName("True Strain");
        tTensileB->column(0)->setUnits("%");
        tTensileB->setColName(1, "Stress");
        tTensileB->setColPlotDesignation(1, SciDAVis::Y);
        tTensileB->column(1)->setLongName("True Stress");
        tTensileB->column(1)->setUnits("MPa");
        for (int r = 0; r < 21; ++r) {
            double eps = double(r) * 0.5;
            tTensileB->setCell(r, 0, eps);
            tTensileB->setCell(r, 1, 780.0 * (1.0 - std::exp(-0.4 * eps)));
        }

        MultiLayer *mlCloned = mw->batchCloneGraphToTable(mlTensile, tTensileB);
        CHECK_COND(mlCloned != nullptr && mlCloned->activeGraph() != nullptr,
                   "batchCloneGraphToTable returned null");
        Graph *gCloned = mlCloned->activeGraph();
        DataCurve *dcCloned = dynamic_cast<DataCurve *>(gCloned->curve(0));
        CHECK_COND(dcCloned != nullptr && dcCloned->table() == tTensileB,
                   "batchCloneGraphToTable did not rebind curve to tTensileB");
        CHECK_COND(gCloned->axisFont(QwtPlot::xBottom).pointSize() == 28
                           && gCloned->referenceLines().size() == 1
                           && gCloned->plotWidget()->axisTitle(QwtPlot::xBottom).text() == "True Strain / %"
                           && gCloned->plotWidget()->axisTitle(QwtPlot::yLeft).text() == "True Stress / MPa",
                   "batchCloneGraphToTable did not preserve styling or update target ISIJ Long Name / Units titles");
    }

    // =========================================================================
    // 27. Suite 27: Lossless .sciprj Project Save/Reload, High-DPI Export Metadata & Translucent EMF
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 27: Lossless .sciprj Save/Reload, 600 DPI Export Metadata & Translucent EMF...\n";

        ApplicationWindow *projWriter = new ApplicationWindow();
        projWriter->applyUserSettings();

        Table *tSave = projWriter->newTable(20, 2, "PersistTable", "Persist");
        tSave->setColName(0, "Time");
        tSave->setColPlotDesignation(0, SciDAVis::X);
        tSave->column(0)->setLongName("Aging Time");
        tSave->column(0)->setUnits("ks");
        tSave->setColName(1, "Hardness");
        tSave->setColPlotDesignation(1, SciDAVis::Y);
        tSave->column(1)->setLongName("Vickers Hardness");
        tSave->column(1)->setUnits("HV");
        for (int r = 0; r < 20; ++r) {
            double x = double(r) * 5.0; // 0 .. 95
            double y = 1200.0 + 800.0 * std::sin(0.1 * double(r)) + 40.0 * double(r);
            tSave->setCell(r, 0, x);
            tSave->setCell(r, 1, y);
        }

        MultiLayer *mlSave = projWriter->multilayerPlot(
                tSave, QStringList() << "PersistTable_Hardness", Graph::LineSymbols);
        CHECK_COND(mlSave != nullptr && mlSave->activeGraph() != nullptr, "Failed to create mlSave");
        mlSave->setName("PersistGraph");
        mlSave->setPageSizePhysical(85.0, 85.0, MultiLayer::UnitMm, 600);
        mlSave->applyAspectRatioPreset(1.0, true);

        Graph *gMainSave = mlSave->activeGraph();
        gMainSave->setScale(QwtPlot::xBottom, 0.0, 100.0);
        gMainSave->setScale(QwtPlot::yLeft, 0.0, 3000.0);

        Graph::ReferenceLineSpec refSave;
        refSave.orientation = 0;
        refSave.value = 1800.0;
        refSave.label = "Target HV";
        refSave.fillToAxis = true;
        refSave.fillColor = QColor(30, 102, 184, 70);
        gMainSave->addReferenceLine(refSave);

        Graph::AxisBreakSpec brkSave;
        brkSave.enabled = true;
        brkSave.breakFrom = 15.0;
        brkSave.breakTo = 80.0;
        brkSave.positionPct = 0.50;
        gMainSave->setAxisBreak(QwtPlot::xBottom, brkSave);

        gMainSave->setTickLabelTransform(QwtPlot::yLeft, 1000.0, "", "k");

        Graph *gLinkedSave = mlSave->addLinkedLayer(gMainSave, true, false, "x/3.6", "y");
        Graph *gInsetSave = mlSave->addInsetLayer(gMainSave, QRectF(0.0, 1000.0, 15.0, 1000.0));
        CHECK_COND(gLinkedSave != nullptr && gInsetSave != nullptr && mlSave->layersList().size() == 3,
                   "Failed to add linked & inset layers to mlSave");

        // Violin plot window in project
        MultiLayer *mlVioSave = projWriter->multilayerPlot(
                tSave, QStringList() << "PersistTable_Hardness", Graph::Box);
        mlVioSave->setName("PersistViolin");
        BoxCurve *bcSave = dynamic_cast<BoxCurve *>(mlVioSave->activeGraph()->curve(0));
        CHECK_COND(bcSave != nullptr, "PersistViolin missing BoxCurve");
        bcSave->setViolinMode(true);
        bcSave->setShowDataPoints(true);

        // 1. High-DPI (600 DPI) PNG & TIFF Export Metadata Verification
        QString png600Path = QDir::tempPath() + "/torigraph_suite27_600dpi.png";
        QString tif600Path = QDir::tempPath() + "/torigraph_suite27_600dpi.tif";
        QFile::remove(png600Path);
        QFile::remove(tif600Path);
        mlSave->exportImage(png600Path, 100, false, 600);
        mlSave->exportImage(tif600Path, 100, false, 600);

        const int expectedDpm600 = qRound(600.0 / 0.0254); // 23622 dots/meter
        QImage imgPng600(png600Path);
        QImage imgTif600(tif600Path);
        QFile::remove(png600Path);
        QFile::remove(tif600Path);
        std::cout << "[INFO] 600 DPI PNG: size=" << imgPng600.width() << "x" << imgPng600.height()
                  << ", dpmX=" << imgPng600.dotsPerMeterX() << " (expected " << expectedDpm600 << ")\n";
        std::cout << "[INFO] 600 DPI TIFF: size=" << imgTif600.width() << "x" << imgTif600.height()
                  << ", dpmX=" << imgTif600.dotsPerMeterX() << "\n";
        CHECK_COND(!imgPng600.isNull()
                           && qAbs(imgPng600.dotsPerMeterX() - expectedDpm600) <= 2
                           && qAbs(imgPng600.dotsPerMeterY() - expectedDpm600) <= 2
                           && imgPng600.width() > 1500 && imgPng600.height() > 1500,
                   "600 DPI PNG export metadata or scaled pixel dimensions mismatch");
        CHECK_COND(!imgTif600.isNull()
                           && qAbs(imgTif600.dotsPerMeterX() - expectedDpm600) <= 2
                           && imgTif600.width() == imgPng600.width(),
                   "600 DPI TIFF export metadata or dimensions mismatch");

        // 2. Translucent EMF Rendering & Zoom-Independent extentHm
        QVector<double> bx = { 0.0, 50.0, 100.0 };
        QVector<double> byLo = { 600.0, 700.0, 800.0 };
        QVector<double> byHi = { 1600.0, 1700.0, 1800.0 };
        gMainSave->addShadedBand(bx, byLo, byHi, QColor(220, 50, 47, 80), "ConfidenceBand");

        mlSave->setPageZoom(1.0);
        OleRenderedGraph emfZ1 = SciDAVisEmf::renderMultiLayer(mlSave);
        mlSave->setPageZoom(1.75);
        OleRenderedGraph emfZ2 = SciDAVisEmf::renderMultiLayer(mlSave);
        mlSave->setPageZoom(1.0);
        std::cout << "[INFO] EMF extentHm at zoom=1.0: (" << emfZ1.extentHm.cx << ", " << emfZ1.extentHm.cy
                  << "), zoom=1.75: (" << emfZ2.extentHm.cx << ", " << emfZ2.extentHm.cy << ")\n";
        CHECK_COND(!emfZ1.emfBytes.isEmpty() && !emfZ2.emfBytes.isEmpty()
                           && qAbs(emfZ1.extentHm.cx - emfZ2.extentHm.cx) <= 2
                           && qAbs(emfZ1.extentHm.cy - emfZ2.extentHm.cy) <= 2,
                   "SciDAVisEmf::renderMultiLayer extentHm was corrupted by pageZoom()!");

        // Play back EMF onto a white 300x300 DIB section to verify non-opaque-black translucent rendering
        HENHMETAFILE hEmf27 = SciDAVisEmf::createHemfFromBytes(emfZ1.emfBytes);
        CHECK_COND(hEmf27 != nullptr, "createHemfFromBytes failed in Suite 27");
        QImage emfPlayback(300, 300, QImage::Format_RGB32);
        emfPlayback.fill(Qt::white);
        {
            HDC screenDc = GetDC(nullptr);
            HDC memDc = CreateCompatibleDC(screenDc);
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = 300;
            bmi.bmiHeader.biHeight = -300; // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void *dibBits = nullptr;
            HBITMAP hBmp = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
            HGDIOBJ oldObj = SelectObject(memDc, hBmp);
            RECT playRect{ 0, 0, 300, 300 };
            HBRUSH whiteBr = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
            FillRect(memDc, &playRect, whiteBr);
            PlayEnhMetaFile(memDc, hEmf27, &playRect);
            GdiFlush();
            if (dibBits) {
                std::memcpy(emfPlayback.bits(), dibBits, 300 * 300 * 4);
            }
            SelectObject(memDc, oldObj);
            DeleteObject(hBmp);
            DeleteDC(memDc);
            ReleaseDC(nullptr, screenDc);
        }
        DeleteEnhMetaFile(hEmf27);

        int blackPixels = 0;
        int pastelTranslucentPixels = 0;
        for (int py = 0; py < 300; ++py) {
            for (int px = 0; px < 300; ++px) {
                QRgb c = emfPlayback.pixel(px, py);
                int r = qRed(c), g = qGreen(c), b = qBlue(c);
                if (r < 15 && g < 15 && b < 15)
                    ++blackPixels;
                // Alpha-blended blue/red band over white yields light pastel RGB (all channels > 140, not pure white)
                if (r > 140 && g > 140 && b > 140 && (r < 250 || g < 250 || b < 250)
                    && (qAbs(r - b) >= 8)) {
                    ++pastelTranslucentPixels;
                }
            }
        }
        std::cout << "[INFO] EMF playback (300x300): blackPx=" << blackPixels
                  << ", pastelTranslucentPx=" << pastelTranslucentPixels << "\n";
        CHECK_COND(blackPixels < 15000 && pastelTranslucentPixels > 200,
                   "EMF translucent ReferenceLine/ShadedBand rendered as opaque black or failed to alpha-blend!");

        // 3. Save .sciprj to disk and reload into a fresh ApplicationWindow
        QByteArray savedBytes = projWriter->saveProjectToByteArray();
        CHECK_COND(!savedBytes.isEmpty(), "projWriter->saveProjectToByteArray() returned empty");
        QString sciprjPath = QDir::tempPath() + "/torigraph_suite27_roundtrip.sciprj";
        {
            QFile f(sciprjPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "Failed to open temp .sciprj for writing");
            f.write(savedBytes);
            f.close();
        }
        projWriter->savedProject();
        projWriter->setOleEmbeddedMode(true);
        delete projWriter;

        ApplicationWindow *projReader = new ApplicationWindow();
        projReader->applyUserSettings();
        bool loadedOk = projReader->loadProject(sciprjPath);
        QFile::remove(sciprjPath);
        CHECK_COND(loadedOk, "projReader->loadProject() failed on Phase 5 .sciprj");

        Table *tReload = projReader->table("PersistTable");
        CHECK_COND(tReload != nullptr, "Reloaded project missing PersistTable");
        CHECK_COND(tReload->column(0)->longName() == "Aging Time"
                           && tReload->column(0)->units() == "ks"
                           && tReload->column(1)->longName() == "Vickers Hardness"
                           && tReload->column(1)->units() == "HV",
                   "Column Long Name / Units lost across .sciprj save/reload!");

        MultiLayer *mlReload = qobject_cast<MultiLayer *>(projReader->window("PersistGraph"));
        CHECK_COND(mlReload != nullptr, "Reloaded project missing PersistGraph");
        std::cout << "[INFO] Reloaded PersistGraph: page=" << mlReload->pageWidthPhysical() << "x"
                  << mlReload->pageHeightPhysical() << " unit=" << int(mlReload->pageUnit())
                  << " dpi=" << mlReload->pageDpi()
                  << " layers=" << mlReload->layersList().size() << "\n";
        CHECK_COND(std::fabs(mlReload->pageWidthPhysical() - 85.0) < 1e-4
                           && std::fabs(mlReload->pageHeightPhysical() - 85.0) < 1e-4
                           && mlReload->pageUnit() == MultiLayer::UnitMm
                           && mlReload->pageDpi() == 600
                           && mlReload->keepAspectRatio()
                           && mlReload->layersList().size() == 3,
                   "MultiLayer physical page setup, aspect ratio, or 3 layers lost across .sciprj reload!");

        Graph *gMainReload = mlReload->layersList().at(0);
        Graph *gLinkedReload = mlReload->layersList().at(1);
        CHECK_COND(gMainReload->referenceLines().size() == 1
                           && std::fabs(gMainReload->referenceLines()[0].value - 1800.0) < 1e-4
                           && gMainReload->referenceLines()[0].fillToAxis,
                   "ReferenceLineSpec lost across .sciprj reload!");
        CHECK_COND(gMainReload->axisBreak(QwtPlot::xBottom).enabled
                           && std::fabs(gMainReload->axisBreak(QwtPlot::xBottom).breakFrom - 15.0) < 1e-4
                           && std::fabs(gMainReload->axisBreak(QwtPlot::xBottom).breakTo - 80.0) < 1e-4,
                   "AxisBreakSpec lost across .sciprj reload!");
        CHECK_COND(std::fabs(gMainReload->tickDivideFactor(QwtPlot::yLeft) - 1000.0) < 1e-4
                           && gMainReload->tickSuffix(QwtPlot::yLeft) == "k",
                   "TickLabelTransform divideByFactor/suffix lost across .sciprj reload!");
        CHECK_COND(gLinkedReload->linkedParentLayer() == gMainReload
                           && gLinkedReload->xLinkExpr() == "x/3.6",
                   "LinkedLayer parent pointer or formula lost across .sciprj reload!");

        MultiLayer *mlVioReload = qobject_cast<MultiLayer *>(projReader->window("PersistViolin"));
        CHECK_COND(mlVioReload != nullptr && mlVioReload->activeGraph() != nullptr,
                   "Reloaded project missing PersistViolin");
        BoxCurve *bcReload = dynamic_cast<BoxCurve *>(mlVioReload->activeGraph()->curve(0));
        CHECK_COND(bcReload != nullptr && bcReload->violinMode() && bcReload->showDataPoints(),
                   "BoxCurve violinMode / showDataPoints lost across .sciprj reload!");

        projReader->savedProject();
        projReader->setOleEmbeddedMode(true);
        delete projReader;
    }

    // =========================================================================
    // 28. Suite 28: Axis Breaks, Segment Curve Hit-Test, Immediate Arrow Nudging,
    //               Multi-Column Legend & Non-Destructive Stack Offsets
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 28: Axis Breaks, Segment Hit-Test, Arrow Nudging, Multi-Col Legend & Stack Offsets...\n";

        // 1. Axis Break piecewise coordinate transformation: [0..10] break [10..90] resume [90..100]
        Table *tBrk = mw->newTable(2, 3, "BreakHitTable", "Sparse 2-pt");
        tBrk->setColPlotDesignation(2, SciDAVis::Y);
        tBrk->setCell(0, 0, 0.0);
        tBrk->setCell(0, 1, 0.0);
        tBrk->setCell(0, 2, 0.0);
        tBrk->setCell(1, 0, 100.0);
        tBrk->setCell(1, 1, 100.0);
        tBrk->setCell(1, 2, 100.0);

        MultiLayer *mlBrk = mw->multilayerPlot(
                tBrk, QStringList() << "BreakHitTable_2" << "BreakHitTable_3", Graph::Line);
        mlBrk->show();
        mlBrk->resize(520, 520);
        QApplication::processEvents();
        Graph *gBrk = mlBrk->activeGraph();
        gBrk->setScale(QwtPlot::xBottom, 0.0, 100.0);
        gBrk->setScale(QwtPlot::yLeft, 0.0, 100.0);

        Graph::AxisBreakSpec brk28;
        brk28.enabled = true;
        brk28.breakFrom = 10.0;
        brk28.breakTo = 90.0;
        brk28.positionPct = 0.50;
        gBrk->setAxisBreak(QwtPlot::xBottom, brk28);
        gBrk->replot();
        QApplication::processEvents();

        QwtScaleMap xMapBrk = gBrk->plotWidget()->canvasMap(QwtPlot::xBottom);
        double px0 = xMapBrk.xTransform(0.0);
        double px5 = xMapBrk.xTransform(5.0);
        double px10 = xMapBrk.xTransform(10.0);
        double px95 = xMapBrk.xTransform(95.0);
        double px100 = xMapBrk.xTransform(100.0);
        double pxMid = 0.5 * (px0 + px100);
        std::cout << "[INFO] AxisBreak [0..10 // 90..100]: px0=" << px0 << ", px5=" << px5
                  << ", px10=" << px10 << ", mid=" << pxMid << ", px95=" << px95
                  << ", px100=" << px100 << "\n";
        CHECK_COND(px5 < pxMid && px95 > pxMid
                           && (px10 - px0) > 0.35 * (px100 - px0)
                           && (px100 - px95) > 0.15 * (px100 - px0),
                   "Graph::setAxisBreak piecewise scale transformation did not map [0..10] & [90..100] around midpoint");

        // Clear axis break to test clean segment hit-testing on linear [0..100] x [0..100]
        gBrk->clearAxisBreak(QwtPlot::xBottom);
        gBrk->replot();
        QApplication::processEvents();

        // 2. Plot::closestCurve hitting midpoint (50, 50) of sparse 2-point line (0,0)->(100,100) while ignoring hidden curve
        QwtPlotCurve *cVisible = gBrk->curve(0);
        QwtPlotCurve *cHidden = gBrk->curve(1);
        CHECK_COND(cVisible != nullptr && cHidden != nullptr, "Missing curves on gBrk");
        cHidden->setVisible(false); // Hide curve 1 -> must be ignored even though it shares exact coordinates!
        gBrk->replot();

        int midX = gBrk->plotWidget()->canvasMap(QwtPlot::xBottom).transform(50.0);
        int midY = gBrk->plotWidget()->canvasMap(QwtPlot::yLeft).transform(50.0);
        int segDist = 9999;
        int nearestPt = -1;
        int hitCurveKey = gBrk->plotWidget()->closestCurve(midX, midY, segDist, nearestPt);
        std::cout << "[INFO] Segment hit-test at midpoint (50,50): hitKey=" << hitCurveKey
                  << ", dist=" << segDist << " px\n";
        CHECK_COND(hitCurveKey > 0
                           && gBrk->plotWidget()->curve(hitCurveKey) == cVisible
                           && segDist <= 3,
                   "Plot::closestCurve failed to hit sparse 2-point segment midpoint or hit hidden curve!");
        cHidden->setVisible(true);

        // 3. Immediate Arrow-Key Nudging via SelectionMoveResizer (commits without Enter!)
        if (!gBrk->legend())
            gBrk->newLegend();
        Legend *leg28 = gBrk->legend();
        leg28->setOrigin(QPoint(80, 60));
        gBrk->replot();
        QPoint posBeforeNudge = leg28->rect().topLeft();
        {
            SelectionMoveResizer nudger(leg28);
            QKeyEvent keyRight(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
            QCoreApplication::sendEvent(&nudger, &keyRight); // +2 px X
            QKeyEvent keyShiftDown(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
            QCoreApplication::sendEvent(&nudger, &keyShiftDown); // +10 px Y
        }
        QPoint posAfterNudge = leg28->rect().topLeft();
        std::cout << "[INFO] Arrow nudge without Enter: before=(" << posBeforeNudge.x() << ","
                  << posBeforeNudge.y() << "), after=(" << posAfterNudge.x() << ","
                  << posAfterNudge.y() << ")\n";
        CHECK_COND(posAfterNudge.x() == posBeforeNudge.x() + 2
                           && posAfterNudge.y() == posBeforeNudge.y() + 10,
                   "SelectionMoveResizer arrow-key nudging did not commit +2px/+10px immediately!");

        // 4. Multi-Column Legend & reconstructLegend() (no duplicate legend boxes!)
        leg28->setNumColumns(2);
        CHECK_COND(leg28->numColumns() == 2, "Legend::setNumColumns(2) getter mismatch");
        auto countLegends = [](Graph *g) {
            int cnt = 0;
            for (QwtPlotItem *it : g->plotWidget()->itemList()) {
                if (dynamic_cast<Legend *>(it))
                    ++cnt;
            }
            return cnt;
        };
        int legCountBefore = countLegends(gBrk);
        gBrk->reconstructLegend();
        gBrk->reconstructLegend();
        int legCountAfter = countLegends(gBrk);
        CHECK_COND(legCountBefore == 1 && legCountAfter == 1 && gBrk->legend()->numColumns() == 2,
                   "Graph::reconstructLegend() created duplicate Legend items or lost numColumns!");

        // 5. Non-destructive Waterfall / Stack Offsets & Reset
        Table *tStk = mw->newTable(10, 4, "StackOffsetTbl", "3 Curves");
        tStk->setColPlotDesignation(2, SciDAVis::Y);
        tStk->setColPlotDesignation(3, SciDAVis::Y);
        for (int r = 0; r < 10; ++r) {
            tStk->setCell(r, 0, double(r));
            tStk->setCell(r, 1, 10.0 + double(r)); // span = 9
            tStk->setCell(r, 2, 10.0 + double(r));
            tStk->setCell(r, 3, 10.0 + double(r));
        }
        MultiLayer *mlStk = mw->multilayerPlot(
                tStk,
                QStringList() << "StackOffsetTbl_2" << "StackOffsetTbl_3" << "StackOffsetTbl_4",
                Graph::Line);
        Graph *gStk = mlStk->activeGraph();
        PlotCurve *pc0 = dynamic_cast<PlotCurve *>(gStk->curve(0));
        PlotCurve *pc1 = dynamic_cast<PlotCurve *>(gStk->curve(1));
        PlotCurve *pc2 = dynamic_cast<PlotCurve *>(gStk->curve(2));
        CHECK_COND(pc0 && pc1 && pc2, "Missing PlotCurves on gStk");

        gStk->setStackOffsets(0 /*Uniform*/, 50.0, 0.0, false);
        std::cout << "[INFO] StackOffsets: dy0=" << pc0->stackOffsetY()
                  << ", dy1=" << pc1->stackOffsetY() << ", dy2=" << pc2->stackOffsetY()
                  << ", curve2.y(0)=" << pc2->y(0) << ", table.cell(0,3)=" << tStk->cell(0, 3) << "\n";
        CHECK_COND(pc1->stackOffsetY() > 0.0 && pc2->stackOffsetY() > pc1->stackOffsetY()
                           && pc2->y(0) > pc1->y(0)
                           && std::fabs(tStk->cell(0, 3) - 10.0) < 1e-9,
                   "Graph::setStackOffsets failed or mutated underlying Table data!");

        gStk->resetStackOffsets();
        CHECK_COND(std::fabs(pc1->stackOffsetY()) < 1e-9 && std::fabs(pc2->stackOffsetY()) < 1e-9
                           && std::fabs(pc1->y(0) - 10.0) < 1e-9 && std::fabs(pc2->y(0) - 10.0) < 1e-9,
                   "Graph::resetStackOffsets did not cleanly restore original curve Y coordinates!");
    }

    // =========================================================================
    // 29. Suite 29: Delta Cursor C1-C2, Weighting Modes, Linear Constraints,
    //               Bootstrap CI, Breakpoint SE & Binary Logistic Regression
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 29: Delta Cursor, Weighting/Constraints/Bootstrap CI, Breakpoint SE & Logistic Regression...\n";

        // 1. DeltaCursorTool on y = 5*x + 1 (at x1=2, x2=6 -> dx=4, dy=20, slope=5, area=84)
        //    and on y = 5*x - 3 (at x1=2, x2=6 -> dx=4, dy=20, slope=5, area=68)
        Table *tDc = mw->newTable(21, 3, "DeltaCursorTbl", "Linear");
        tDc->setColPlotDesignation(2, SciDAVis::Y);
        for (int i = 0; i <= 20; ++i) {
            double x = 0.5 * double(i); // 0 .. 10
            tDc->setCell(i, 0, x);
            tDc->setCell(i, 1, 5.0 * x + 1.0);
            tDc->setCell(i, 2, 5.0 * x - 3.0);
        }
        MultiLayer *mlDc = mw->multilayerPlot(
                tDc, QStringList() << "DeltaCursorTbl_2" << "DeltaCursorTbl_3", Graph::Line);
        Graph *gDc = mlDc->activeGraph();
        DeltaCursorTool *dcTool = DeltaCursorTool::attach(mw, gDc, DeltaCursorTool::SnapInterpolateCurve);
        CHECK_COND(dcTool != nullptr && dcTool->rtti() == PlotToolInterface::DeltaCursor,
                   "Failed to attach DeltaCursorTool");
        dcTool->setTargetCurve("DeltaCursorTbl_2");
        dcTool->setCursorPositions(2.0, 6.0);
        auto ro1 = dcTool->currentReadout();
        std::cout << "[INFO] DeltaCursor (y=5x+1, [2..6]): dx=" << ro1.dx << ", dy=" << ro1.dy
                  << ", slope=" << ro1.slope << ", area=" << ro1.area << " (expected 4, 20, 5, 84)\n";
        CHECK_COND(std::fabs(ro1.dx - 4.0) < 1e-9 && std::fabs(ro1.dy - 20.0) < 1e-9
                           && std::fabs(ro1.slope - 5.0) < 1e-9 && std::fabs(ro1.area - 84.0) < 1e-6,
                   "DeltaCursorTool readout mismatch on y = 5x + 1");

        dcTool->setTargetCurve("DeltaCursorTbl_3");
        dcTool->setCursorPositions(2.0, 6.0);
        auto ro2 = dcTool->currentReadout();
        CHECK_COND(std::fabs(ro2.dx - 4.0) < 1e-9 && std::fabs(ro2.dy - 20.0) < 1e-9
                           && std::fabs(ro2.slope - 5.0) < 1e-9 && std::fabs(ro2.area - 68.0) < 1e-6,
                   "DeltaCursorTool readout mismatch on y = 5x - 3 (area=68)");
        gDc->setActiveTool(nullptr);

        // 2. Fit Weighting Modes, Linear Equality Constraint ("A2=0.5*A1") & Bootstrap 95% CI
        Table *tCon = mw->newTable(41, 2, "ConstrainedBootTbl", "Bi-Exp");
        for (int i = 0; i <= 40; ++i) {
            double x = double(i) * 0.8; // 0 .. 32
            // True parameters: A1 = 120.0, A2 = 60.0 (= 0.5 * A1), y0 = 10.0 + tiny deterministic noise
            double noise = 0.25 * std::sin(1.7 * double(i)) + 0.18 * std::cos(2.9 * double(i));
            double y = 10.0 + 120.0 * std::exp(-x / 4.0) + 60.0 * std::exp(-x / 18.0) + noise;
            tCon->setCell(i, 0, x);
            tCon->setCell(i, 1, y);
        }
        MultiLayer *mlCon = mw->multilayerPlot(tCon, QStringList() << "ConstrainedBootTbl_2", Graph::Scatter);
        Graph *gCon = mlCon->activeGraph();

        {
            // Perturb data slightly so unconstrained A2/A1 != 0.5, then verify constraint enforces A2 == 0.5*A1 exactly!
            NonLinearFit *nlfCon = new NonLinearFit(mw, gCon);
            nlfCon->setParametersList(QStringList() << "A1" << "A2" << "y0");
            nlfCon->setFormula("y0 + A1*exp(-x/4.0) + A2*exp(-x/18.0)");
            CHECK_COND(nlfCon->setDataFromCurve("ConstrainedBootTbl_2"), "nlfCon setDataFromCurve failed");
            CHECK_COND(nlfCon->setWeightingMethod(Fit::WeightStatistical)
                               && nlfCon->weightingMethod() == Fit::WeightStatistical,
                       "Fit::setWeightingMethod(WeightStatistical) failed");
            nlfCon->setWeightingMethod(Fit::WeightNone);
            CHECK_COND(nlfCon->setLinearConstraintExpr("A2=0.5*A1"),
                       "Fit::setLinearConstraintExpr(\"A2=0.5*A1\") returned false");
            nlfCon->setInitialGuess(0, 90.0);
            nlfCon->setInitialGuess(1, 45.0);
            nlfCon->setInitialGuess(2, 8.0);
            nlfCon->fit();
            const auto &cRes = nlfCon->results();
            std::cout << "[INFO] Constrained Fit (A2=0.5*A1): A1=" << cRes[0] << ", A2=" << cRes[1]
                      << ", ratio=" << (cRes[1] / cRes[0]) << ", y0=" << cRes[2] << "\n";
            CHECK_COND(std::fabs(cRes[1] - 0.5 * cRes[0]) < 1e-6
                               && relErrWithin(cRes[0], 120.0, 0.02)
                               && relErrWithin(cRes[2], 10.0, 0.05),
                       "Fit::setLinearConstraintExpr(\"A2=0.5*A1\") was not satisfied or inaccurate");

            // Unconstrained fit + 200-sample Nonparametric Bootstrap 95% CI
            nlfCon->setLinearConstraintExpr("");
            nlfCon->fit();
            auto boot = nlfCon->runBootstrapCI(200, 0.95, 0);
            std::cout << "[INFO] Bootstrap 95% CI (nSuccess=" << boot.nSuccess
                      << "): A1 in [" << boot.ciLow.value(0) << ", " << boot.ciHigh.value(0)
                      << "], A2 in [" << boot.ciLow.value(1) << ", " << boot.ciHigh.value(1)
                      << "], y0 in [" << boot.ciLow.value(2) << ", " << boot.ciHigh.value(2) << "]\n";
            CHECK_COND(boot.valid && boot.nSuccess >= 180 && boot.ciLow.size() == 3
                               && boot.ciLow[0] < 120.0 && 120.0 < boot.ciHigh[0]
                               && (boot.ciHigh[0] - boot.ciLow[0]) < 8.0
                               && boot.ciLow[1] < 60.0 && 60.0 < boot.ciHigh[1]
                               && boot.ciLow[2] < 10.0 && 10.0 < boot.ciHigh[2],
                       "Fit::runBootstrapCI(200, 0.95) failed to return tight CIs containing true parameters");
            delete nlfCon;
        }

        // 3. Piecewise Breakpoint Standard Error (breakpointsXSE > 0)
        Table *tPwSe = mw->newTable(61, 2, "PiecewiseSETbl", "Noisy Bilinear");
        for (int i = 0; i <= 60; ++i) {
            double x = double(i); // 0 .. 60, breakpoint at x = 30
            double noise = 0.25 * std::sin(2.3 * double(i));
            double y = ((x <= 30.0) ? (3.0 * x) : (90.0 + 0.5 * (x - 30.0))) + noise;
            tPwSe->setCell(i, 0, x);
            tPwSe->setCell(i, 1, y);
        }
        MultiLayer *mlPwSe = mw->multilayerPlot(tPwSe, QStringList() << "PiecewiseSETbl_2", Graph::Scatter);
        auto pwSeRes = PiecewiseAndRobustFit::fitPiecewiseLinear(
                mw, mlPwSe->activeGraph(), "PiecewiseSETbl_2", 2, false);
        std::cout << "[INFO] Piecewise Breakpoint SE: bp=" << pwSeRes.breakpointsX.value(0)
                  << " +/- " << pwSeRes.breakpointsXSE.value(0) << "\n";
        CHECK_COND(pwSeRes.converged && pwSeRes.breakpointsXSE.size() == 1
                           && pwSeRes.breakpointsXSE[0] > 0.0 && pwSeRes.breakpointsXSE[0] < 2.0
                           && std::fabs(pwSeRes.breakpointsX[0] - 30.0) < 0.5,
                   "fitPiecewiseLinear breakpointsXSE missing or out of range");

        // 4. Binary Logistic Regression recovering true log-odds slope (beta_0 = -3.0, beta_1 = 1.5)
        const int nLogit = 160;
        Table *tLogit = mw->newTable(nLogit, 2, "LogisticTbl", "Binary Outcome");
        tLogit->setColName(0, "Dose");
        tLogit->setColName(1, "Response");
        auto halton29 = [](int index, int base) {
            double f = 1.0, r = 0.0;
            for (int k = index; k > 0; k /= base) {
                f /= double(base);
                r += f * double(k % base);
            }
            return r;
        };
        for (int i = 0; i < nLogit; ++i) {
            double x = -1.0 + 6.0 * double(i) / double(nLogit - 1); // -1 .. +5 (centered around x0 = 2.0)
            double pTrue = 1.0 / (1.0 + std::exp(-(-3.0 + 1.5 * x)));
            int yBin = (halton29(i + 1, 2) < pTrue) ? 1 : 0;
            tLogit->setCell(i, 0, x);
            tLogit->setCell(i, 1, double(yBin));
        }
        auto logRes = PiecewiseAndRobustFit::runLogisticRegression(
                mw, tLogit, "Response", QStringList() << "Dose", true);
        std::cout << "[INFO] Logistic Regression: converged=" << logRes.converged
                  << ", beta0=" << logRes.betas.value(0) << ", beta1=" << logRes.betas.value(1)
                  << ", OR=" << logRes.oddsRatios.value(0) << ", lrP=" << logRes.lrPValue << "\n";
        CHECK_COND(logRes.converged && logRes.betas.size() == 2 && logRes.oddsRatios.size() == 1
                           && std::fabs(logRes.betas[1] - 1.5) < 0.35
                           && std::fabs(logRes.oddsRatios[0] - std::exp(logRes.betas[1])) < 1e-6
                           && logRes.lrPValue < 1e-6 && logRes.probGraph != nullptr,
                   "runLogisticRegression failed to recover true log-odds slope or probability plot");
    }

    // =========================================================================
    // 30. Suite 30: Arrhenius Ea, Tauc Bandgap Eg, Weibull Modulus m,
    //               Grubbs Outlier, Smart-Switch Nonparametric & K-Means Clustering
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 30: Arrhenius Ea, Tauc Eg, Weibull m, Grubbs Outlier, Smart-Switch Nonparam & Clustering...\n";

        // 1. Arrhenius Plot: k(T) = A * exp(-Ea / (R*T)) with Ea = 50.0 kJ/mol (~0.51821 eV)
        const double R_gas = 8.314462618;
        const double trueEa_J = 50000.0; // 50.0 kJ/mol
        const double trueA = 2.5e7;
        Table *tArr = mw->newTable(10, 2, "ArrheniusInputTbl", "Rate vs T");
        tArr->setColName(0, "Temp_K");
        tArr->setColName(1, "Rate_k");
        for (int i = 0; i < 10; ++i) {
            double T = 300.0 + 40.0 * double(i); // 300 .. 660 K
            double k = trueA * std::exp(-trueEa_J / (R_gas * T));
            tArr->setCell(i, 0, T);
            tArr->setCell(i, 1, k);
        }
        auto arrRes = CurveMathTools::createArrheniusPlot(
                mw, tArr, "Temp_K", "Rate_k", false /*Kelvin*/, true /*1000/T*/, false);
        std::cout << "[INFO] Arrhenius Plot: Ea=" << arrRes.activationEnergyKJmol << " kJ/mol ("
                  << arrRes.activationEnergyEV << " eV), R^2=" << arrRes.rSquare << "\n";
        CHECK_COND(arrRes.valid && arrRes.plot != nullptr && arrRes.rSquare > 0.9999
                           && std::fabs(arrRes.activationEnergyKJmol - 50.0) < 0.1
                           && std::fabs(arrRes.activationEnergyEV - 0.5182) < 0.005,
                   "createArrheniusPlot failed to recover Ea = 50.0 kJ/mol (0.5182 eV)");

        // 2. Tauc Plot: Direct-gap semiconductor Eg = 3.20 eV (input wavelength nm)
        const int nTauc = 80;
        Table *tTauc = mw->newTable(nTauc, 2, "TaucInputTbl", "UV-Vis Absorption");
        tTauc->setColName(0, "Wavelength_nm");
        tTauc->setColName(1, "Alpha_cm1");
        for (int i = 0; i < nTauc; ++i) {
            // Span hv from 2.60 eV to 4.00 eV -> wavelength = 1239.84193 / hv
            double hv = 2.60 + 1.40 * double(i) / double(nTauc - 1);
            double lambdaNm = 1239.84193 / hv;
            double alpha = (hv <= 3.20)
                    ? (10.0 * std::exp((hv - 3.20) / 0.04)) // tiny Urbach sub-gap tail
                    : (5.0e4 * std::sqrt(hv - 3.20) / hv);   // exact (alpha*hv)^2 = 2.5e9 * (hv - 3.20)
            tTauc->setCell(i, 0, lambdaNm);
            tTauc->setCell(i, 1, alpha);
        }
        auto taucRes = CurveMathTools::createTaucPlot(
                mw, tTauc, "Wavelength_nm", "Alpha_cm1", true /*nm*/, true /*direct (a*hv)^2*/);
        std::cout << "[INFO] Tauc Plot (Direct): Eg=" << taucRes.bandgapEV << " +/- "
                  << taucRes.bandgapEVSE << " eV, R^2=" << taucRes.rSquare << "\n";
        CHECK_COND(taucRes.valid && taucRes.plot != nullptr
                           && std::fabs(taucRes.bandgapEV - 3.20) < 0.05,
                   "createTaucPlot failed to recover direct optical bandgap Eg = 3.20 eV");

        // 3. Weibull Probability Plot (m = 8.0, sigma_0 = 400.0 MPa)
        const int nWb = 30;
        Table *tWb = mw->newTable(nWb, 1, "WeibullStrengthTbl", "Ceramic Flexural Strength");
        tWb->setColName(0, "Strength_MPa");
        for (int i = 0; i < nWb; ++i) {
            double rankF = (double(i + 1) - 0.3) / (double(nWb) + 0.4); // Benard median rank
            double sigma = 400.0 * std::pow(-std::log(1.0 - rankF), 1.0 / 8.0);
            tWb->setCell(i, 0, sigma);
        }
        auto wbRes = GraphStatsAndSurfaceFit::plotWeibullProbability(mw, tWb, "Strength_MPa");
        std::cout << "[INFO] Weibull Plot: m=" << wbRes.modulusM << " (exp 8.0), sigma0="
                  << wbRes.scaleSigma0 << " MPa (exp 400.0), R^2=" << wbRes.rSquare << "\n";
        CHECK_COND(wbRes.valid && wbRes.weibullPlot != nullptr && wbRes.rSquare > 0.999
                           && relErrWithin(wbRes.modulusM, 8.0, 0.02)
                           && relErrWithin(wbRes.scaleSigma0, 400.0, 0.01),
                   "plotWeibullProbability failed to recover m=8.0 and sigma0=400 MPa");

        // 4. Grubbs Outlier Test detecting injected extreme outlier at row 12
        Table *tOut = mw->newTable(25, 1, "OutlierInputTbl", "Grubbs Test");
        tOut->setColName(0, "Measurement");
        for (int i = 0; i < 25; ++i) {
            double v = 100.0 + 1.2 * std::sin(1.7 * double(i));
            if (i == 12)
                v = 195.0; // clear outlier
            tOut->setCell(i, 0, v);
        }
        auto outRes = GraphStatsAndSurfaceFit::runOutlierTest(
                mw, tOut, "Measurement", 0 /*Grubbs*/, 0.05, 3, false);
        std::cout << "[INFO] Grubbs Outlier Test: G=" << outRes.primaryStatistic
                  << ", Gcrit=" << outRes.criticalValue << ", p=" << outRes.pValue
                  << ", detectedRows=" << outRes.outlierRows.size() << "\n";
        CHECK_COND(outRes.valid && outRes.outlierRows.contains(12) && outRes.pValue < 0.001,
                   "runOutlierTest(Grubbs) failed to detect injected outlier at row 12");

        // 5. Smart-Switch Nonparametric Comparison (TestAutoSmartSwitch):
        //    - Gaussian 3 groups -> delegates to One-Way ANOVA
        //    - Skewed/Bimodal 3 groups -> switches to Kruskal-Wallis H + Dunn
        //    - Skewed/Bimodal 2 groups -> switches to Mann-Whitney U
        const int nGrp = 30;
        Table *tSmart = mw->newTable(nGrp, 6, "SmartSwitchTbl", "Parametric vs Nonparam");
        tSmart->setColName(0, "Gauss1");
        tSmart->setColName(1, "Gauss2");
        tSmart->setColName(2, "Gauss3");
        tSmart->setColName(3, "Skew1");
        tSmart->setColName(4, "Skew2");
        tSmart->setColName(5, "Skew3");
        auto halton30 = [](int index, int base) {
            double f = 1.0, r = 0.0;
            for (int k = index; k > 0; k /= base) {
                f /= double(base);
                r += f * double(k % base);
            }
            return r;
        };
        for (int i = 0; i < nGrp; ++i) {
            double u1 = std::max(1e-6, std::min(1.0 - 1e-6, halton30(i + 1, 2)));
            double u2 = halton30(i + 1, 3);
            double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
            tSmart->setCell(i, 0, 10.0 + 1.5 * z);
            tSmart->setCell(i, 1, 15.0 + 1.5 * z);
            tSmart->setCell(i, 2, 22.0 + 1.5 * z);

            // Strongly skewed/heavy-tailed non-normal groups (Shapiro-Wilk p << 0.05)
            double skBase = (i < 24) ? (0.2 * double(i % 4)) : (30.0 + 5.0 * double(i));
            tSmart->setCell(i, 3, 1.0 + skBase);
            tSmart->setCell(i, 4, 25.0 + skBase);
            tSmart->setCell(i, 5, 80.0 + skBase);
        }

        auto npGauss = GraphStatsAndSurfaceFit::runNonparametricComparison(
                mw, tSmart, QStringList() << "Gauss1" << "Gauss2" << "Gauss3",
                GraphStatsAndSurfaceFit::TestAutoSmartSwitch);
        auto npSkew3 = GraphStatsAndSurfaceFit::runNonparametricComparison(
                mw, tSmart, QStringList() << "Skew1" << "Skew2" << "Skew3",
                GraphStatsAndSurfaceFit::TestAutoSmartSwitch);
        auto npSkew2 = GraphStatsAndSurfaceFit::runNonparametricComparison(
                mw, tSmart, QStringList() << "Skew1" << "Skew3",
                GraphStatsAndSurfaceFit::TestAutoSmartSwitch);

        std::cout << "[INFO] SmartSwitch(Gaussian 3-grp): test='" << npGauss.chosenTestName.toStdString()
                  << "', p=" << npGauss.pValue << "\n";
        std::cout << "[INFO] SmartSwitch(Skewed 3-grp):   test='" << npSkew3.chosenTestName.toStdString()
                  << "', p=" << npSkew3.pValue << "\n";
        std::cout << "[INFO] SmartSwitch(Skewed 2-grp):   test='" << npSkew2.chosenTestName.toStdString()
                  << "', p=" << npSkew2.pValue << "\n";
        CHECK_COND(npGauss.valid && npGauss.chosenTestName.contains("ANOVA") && npGauss.pValue < 0.001,
                   "TestAutoSmartSwitch did not select ANOVA on Gaussian groups");
        CHECK_COND(npSkew3.valid && npSkew3.chosenTestName.contains("Kruskal") && npSkew3.pValue < 0.001,
                   "TestAutoSmartSwitch did not select Kruskal-Wallis on 3 skewed groups");
        CHECK_COND(npSkew2.valid && npSkew2.chosenTestName.contains("Mann-Whitney") && npSkew2.pValue < 0.001,
                   "TestAutoSmartSwitch did not select Mann-Whitney U on 2 skewed groups");

        // 6. Correlation Matrix Heatmap & K-Means Clustering (silhouetteScore > 0.7)
        Matrix *corrMat = nullptr;
        MultiLayer *mlCorr = GraphStatsAndSurfaceFit::plotCorrelationMatrixHeatmap(
                mw, tSmart, QStringList() << "Gauss1" << "Gauss2" << "Gauss3", false, &corrMat);
        CHECK_COND(mlCorr != nullptr && corrMat != nullptr
                           && std::fabs(corrMat->cell(0, 0) - 1.0) < 1e-6,
                   "plotCorrelationMatrixHeatmap failed");

        Table *tClust = mw->newTable(45, 2, "KMeansClustersTbl", "3 Well-Separated Clusters");
        tClust->setColName(0, "FeatureX");
        tClust->setColName(1, "FeatureY");
        const double centers[3][2] = { { 0.0, 0.0 }, { 25.0, 0.0 }, { 12.5, 22.0 } };
        for (int c = 0; c < 3; ++c) {
            for (int j = 0; j < 15; ++j) {
                int r = c * 15 + j;
                tClust->setCell(r, 0, centers[c][0] + 0.6 * std::cos(double(j)));
                tClust->setCell(r, 1, centers[c][1] + 0.6 * std::sin(double(j)));
            }
        }
        auto kmRes = GraphStatsAndSurfaceFit::runKMeansClustering(
                mw, tClust, QStringList() << "FeatureX" << "FeatureY", 3, true);
        std::cout << "[INFO] K-Means (k=3): silhouetteScore=" << kmRes.silhouetteScore << "\n";
        CHECK_COND(kmRes.valid && kmRes.assignments.size() == 45
                           && kmRes.silhouetteScore > 0.7 && kmRes.clusterPlot != nullptr,
                   "runKMeansClustering failed or silhouetteScore <= 0.7");
    }

    // =========================================================================
    // 31. Suite 31: Round 2 P0/P1 Regression Gate
    //     (Cyclic expandUserFunctions, 6-Way Unweighted Fit SE Parity,
    //      Linear/Poly Weighting + yErr==0 + Eliminated Dependent Bounds,
    //      QuantileReg Sparsity SEs, DeltaCursor Stepping & Free Crosshair,
    //      Generalized ESD Masking Pair, KM/3-Param Weibull & .sciprj Reload)
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 31: Round 2 P0/P1 Regression Gate...\n";

        // 1. FitDialog::expandUserFunctions cyclic f1 -> f2 -> f1 finite termination
        QString cycExpanded = FitDialog::expandUserFunctions(
                "A*f1(x) + B",
                QStringList() << "f1" << "f2",
                QStringList() << "2*f2(x)+1" << "3*f1(x)-1");
        QString acycExpanded = FitDialog::expandUserFunctions(
                "A*f1(x)",
                QStringList() << "f1" << "f2",
                QStringList() << "2*f2(x)" << "(x^2+1)");
        std::cout << "[INFO] expandUserFunctions(cyclic) len=" << cycExpanded.length()
                  << ", acyclic='" << acycExpanded.toStdString() << "'\n";
        CHECK_COND(!cycExpanded.isEmpty() && cycExpanded.length() < 8192,
                   "FitDialog::expandUserFunctions failed finite termination on f1->f2->f1 cycle");
        CHECK_COND(acycExpanded.contains("f2=(x^2+1)") && acycExpanded.contains("f1=2*f2(x)")
                           && acycExpanded.indexOf("f2=") < acycExpanded.indexOf("f1="),
                   "FitDialog::expandUserFunctions failed valid nested acyclic topological expansion");

        // 2. Unweighted nonlinear Fit SE 6-way exact match against OLS LinearFit (no double chi2/dof scaling)
        Table *tSe = mw->newTable(10, 2, "Suite31SeParityTbl", "Unweighted SE Parity");
        tSe->setColName(0, "X");
        tSe->setColName(1, "Y");
        const double epsSe[10] = { 1.8, -2.1, 2.4, -1.9, 1.5, -2.3, 2.0, -1.7, 2.2, -1.9 };
        for (int r = 0; r < 10; ++r) {
            const double xv = double(r + 1);
            tSe->setCell(r, 0, xv);
            tSe->setCell(r, 1, 2.0 + 3.5 * xv + epsSe[r]); // chi2/dof ~ 4.7 => sqrt(chi2/dof) ~ 2.17
        }
        MultiLayer *mlSeLin = mw->multilayerPlot(
                tSe, QStringList() << (QString(tSe->name()) + "_Y"), Graph::Scatter);
        MultiLayer *mlSeNlf = mw->multilayerPlot(
                tSe, QStringList() << (QString(tSe->name()) + "_Y"), Graph::Scatter);
        Graph *gSeLin = mlSeLin ? mlSeLin->activeGraph() : nullptr;
        Graph *gSeNlf = mlSeNlf ? mlSeNlf->activeGraph() : nullptr;
        CHECK_COND(gSeLin != nullptr && gSeNlf != nullptr, "Failed to create SE parity graphs");

        LinearFit *lfRef = new LinearFit(mw, gSeLin);
        lfRef->setDataCurve(0, 1.0, 10.0);
        lfRef->scaleErrors(true);
        lfRef->enableConfidenceBand(true, 0.95);
        lfRef->fit();
        const double seB_ols = lfRef->errors().at(0);
        const double seA_ols = lfRef->errors().at(1);

        mw->writeFitResultsToLog = true;
        mw->clearLogInfo();
        NonLinearFit *nlfTest = new NonLinearFit(mw, gSeNlf);
        nlfTest->setDataCurve(0, 1.0, 10.0);
        nlfTest->setParametersList(QStringList() << "B" << "A");
        nlfTest->setFormula("B + A*x");
        nlfTest->setInitialGuess(0, 1.5);
        nlfTest->setInitialGuess(1, 3.0);
        nlfTest->scaleErrors(true);
        nlfTest->enableConfidenceBand(true, 0.95);
        nlfTest->fit();
        Table *parTblSe = nlfTest->parametersTable("Suite31NlfParTbl");
        const QString legSeText = nlfTest->legendInfo();
        const QString logSeText = mw->logInfo;
        nlfTest->insertFitSummaryBoxOnGraph();
        QString sumBoxSeText;
        if (!gSeNlf->textMarkerKeys().isEmpty()) {
            if (Legend *sb = gSeNlf->textMarker(gSeNlf->textMarkerKeys().last()))
                sumBoxSeText = sb->text();
        }

        const double seB_nlf = nlfTest->errors().at(0);
        const double seA_nlf = nlfTest->errors().at(1);
        const double seB_tbl = parTblSe ? parTblSe->column(2)->valueAt(0) : -1.0;
        const double seA_tbl = parTblSe ? parTblSe->column(2)->valueAt(1) : -1.0;

        auto extractPlusMinusAfter = [](const QString &txt, const QString &paramName) -> double {
            QRegularExpression re(QRegularExpression::escape(paramName)
                                  + QStringLiteral("\\s*=\\s*[-+0-9.eE]+\\s*(?:\\+/-|\xC2\xB1)\\s*([-+0-9.eE]+)"));
            QRegularExpressionMatch m = re.match(txt);
            return m.hasMatch() ? m.captured(1).toDouble() : -1.0;
        };
        const double seB_log = extractPlusMinusAfter(logSeText, "B");
        const double seA_log = extractPlusMinusAfter(logSeText, "A");
        const double seB_leg = extractPlusMinusAfter(legSeText, "B");
        const double seA_leg = extractPlusMinusAfter(legSeText, "A");
        const double seB_box = extractPlusMinusAfter(sumBoxSeText, "B");
        const double seA_box = extractPlusMinusAfter(sumBoxSeText, "A");

        // Compare 95% CI upper band curves at midpoint between OLS LinearFit and NonLinearFit
        auto maxCurveAtMid = [](Graph *g) -> double {
            double best = -1e300;
            for (int ci = 1; ci < g->curves(); ++ci) {
                QwtPlotCurve *c = g->curve(ci);
                if (c && c->dataSize() >= 5) {
                    const double yMid = c->y(c->dataSize() / 2);
                    if (yMid > best)
                        best = yMid;
                }
            }
            return best;
        };
        const double ciUpperMidLin = maxCurveAtMid(gSeLin);
        const double ciUpperMidNlf = maxCurveAtMid(gSeNlf);

        std::cout << "[INFO] SE(B): OLS=" << seB_ols << ", NLF=" << seB_nlf << ", tbl=" << seB_tbl
                  << ", log=" << seB_log << ", leg=" << seB_leg << ", box=" << seB_box << "\n";
        std::cout << "[INFO] SE(A): OLS=" << seA_ols << ", NLF=" << seA_nlf << ", tbl=" << seA_tbl
                  << ", log=" << seA_log << ", leg=" << seA_leg << ", box=" << seA_box
                  << ", CI_mid(Lin/Nlf)=" << ciUpperMidLin << "/" << ciUpperMidNlf << "\n";

        CHECK_COND(std::fabs(seB_nlf - seB_ols) / seB_ols < 1e-3
                           && std::fabs(seA_nlf - seA_ols) / seA_ols < 1e-3,
                   "Unweighted NonLinearFit errors() diverged from OLS LinearFit (double chi2/dof scaling?)");
        CHECK_COND(std::fabs(seB_tbl - seB_ols) / seB_ols < 1e-3
                           && std::fabs(seA_tbl - seA_ols) / seA_ols < 1e-3,
                   "parametersTable() SE diverged from OLS LinearFit");
        CHECK_COND(seB_log > 0.0 && std::fabs(seB_log - seB_ols) / seB_ols < 0.02
                           && seA_log > 0.0 && std::fabs(seA_log - seA_ols) / seA_ols < 0.02,
                   "logFitInfo() SE diverged from OLS LinearFit");
        CHECK_COND(seB_leg > 0.0 && std::fabs(seB_leg - seB_ols) / seB_ols < 0.02
                           && seA_leg > 0.0 && std::fabs(seA_leg - seA_ols) / seA_ols < 0.02,
                   "legendInfo() SE diverged from OLS LinearFit");
        CHECK_COND(seB_box > 0.0 && std::fabs(seB_box - seB_ols) / seB_ols < 0.02
                           && seA_box > 0.0 && std::fabs(seA_box - seA_ols) / seA_ols < 0.02,
                   "Summary box SE diverged from OLS LinearFit");
        CHECK_COND(std::isfinite(ciUpperMidLin) && std::isfinite(ciUpperMidNlf)
                           && std::fabs(ciUpperMidNlf - ciUpperMidLin) < 0.02,
                   "95% CI band on NonLinearFit diverged from OLS LinearFit");
        delete lfRef;
        delete nlfTest;

        // 3. LinearFit / PolynomialFit WeightStatistical / WeightDirect + yErr==0 safety
        //    + eliminated dependent-parameter box bounds
        Table *tWt = mw->newTable(6, 4, "Suite31WeightTbl", "Weighting & Zero yErr");
        tWt->setColName(0, "X");
        tWt->setColName(1, "Y");
        tWt->setColName(2, "DirectW");
        tWt->setColName(3, "ZeroErr");
        tWt->setColPlotDesignation(3, SciDAVis::yErr);
        for (int r = 0; r < 6; ++r) {
            const double xv = double(r + 1);
            // Points 0..4 lie on y = 10 + 2*x; point 5 (x=6) has large positive outlier y = 120
            const double yv = (r < 5) ? (10.0 + 2.0 * xv) : 120.0;
            tWt->setCell(r, 0, xv);
            tWt->setCell(r, 1, yv);
            tWt->setCell(r, 2, (r < 5) ? 100.0 : 0.01); // Direct weight strongly downweights r=5
            tWt->setCell(r, 3, (r == 2) ? 0.0 : 1.0);    // Contains exact yErr == 0.0!
        }
        MultiLayer *mlWt = mw->multilayerPlot(
                tWt, QStringList() << (QString(tWt->name()) + "_Y"), Graph::Scatter);
        Graph *gWt = mlWt ? mlWt->activeGraph() : nullptr;
        CHECK_COND(gWt != nullptr, "Failed to create weighting test graph");
        gWt->addErrorBars(QString(tWt->name()) + "_Y", tWt, QString(tWt->name()) + "_ZeroErr");

        LinearFit *lfUnw = new LinearFit(mw, gWt);
        lfUnw->setDataCurve(0, 1.0, 6.0);
        lfUnw->setWeightingMethod(Fit::WeightNone);
        lfUnw->fit();
        const double slopeUnw = lfUnw->results().at(1);

        LinearFit *lfStat = new LinearFit(mw, gWt);
        lfStat->setDataCurve(0, 1.0, 6.0);
        lfStat->setWeightingMethod(Fit::WeightStatistical);
        lfStat->fit();
        const double slopeStat = lfStat->results().at(1);

        LinearFit *lfDir = new LinearFit(mw, gWt);
        lfDir->setDataCurve(0, 1.0, 6.0);
        lfDir->setWeightingMethod(Fit::WeightDirect, QString(tWt->name()) + "_DirectW");
        lfDir->fit();
        const double slopeDir = lfDir->results().at(1);

        LinearFit *lfZeroErr = new LinearFit(mw, gWt);
        lfZeroErr->setDataCurve(0, 1.0, 6.0);
        lfZeroErr->setYErrorSource(Fit::AssociatedErrors, QString(tWt->name()) + "_ZeroErr", true);
        lfZeroErr->fit();

        PolynomialFit *pfDir = new PolynomialFit(mw, gWt, 2, false);
        pfDir->setDataCurve(0, 1.0, 6.0);
        pfDir->setWeightingMethod(Fit::WeightDirect, QString(tWt->name()) + "_DirectW");
        pfDir->fit();

        PolynomialFit *pfStat = new PolynomialFit(mw, gWt, 2, false);
        pfStat->setDataCurve(0, 1.0, 6.0);
        pfStat->setWeightingMethod(Fit::WeightStatistical);
        pfStat->fit();

        std::cout << "[INFO] LinearFit slopes: Unweighted=" << slopeUnw
                  << ", Statistical(1/|y|)=" << slopeStat
                  << ", DirectW=" << slopeDir << "\n";
        // Because point 5 has huge y=120, WeightStatistical (w=1/|y|) and WeightDirect (w=0.01) pull slope down toward true 2.0!
        CHECK_COND(slopeStat < slopeUnw - 1.5 && std::fabs(slopeDir - 2.0) < 0.15,
                   "LinearFit WeightStatistical / WeightDirect did not apply weights properly");
        CHECK_COND(std::isfinite(lfZeroErr->results().at(0)) && std::isfinite(lfZeroErr->results().at(1))
                           && std::isfinite(lfZeroErr->errors().at(0)) && std::isfinite(lfZeroErr->errors().at(1)),
                   "LinearFit with yErr==0 produced NaN/Inf");
        CHECK_COND(pfDir->results().size() == 3 && std::isfinite(pfDir->results().at(2))
                           && std::fabs(pfDir->results().at(2)) < 0.05
                           && pfStat->results().size() == 3 && std::isfinite(pfStat->results().at(2)),
                   "PolynomialFit WeightDirect / WeightStatistical failed");
        delete lfUnw;
        delete lfStat;
        delete lfDir;
        delete lfZeroErr;
        delete pfDir;
        delete pfStat;

        // Eliminated dependent-parameter box bounds: y = A1*x + A2, data generated from A1=10, A2=40
        // With constraint A2 = 4*A1 and box bound 0 <= A2 <= 20, A2 MUST be clamped to <= 20 (so A1 <= 5)!
        Table *tDep = mw->newTable(8, 2, "Suite31DepBoundTbl", "Dependent Bound");
        for (int r = 0; r < 8; ++r) {
            const double xv = double(r);
            tDep->setCell(r, 0, xv);
            tDep->setCell(r, 1, 10.0 * xv + 40.0);
        }
        MultiLayer *mlDep = mw->multilayerPlot(
                tDep, QStringList() << (QString(tDep->name()) + "_2"), Graph::Scatter);
        NonLinearFit *nlfDep = new NonLinearFit(mw, mlDep->activeGraph());
        nlfDep->setDataCurve(0, 0.0, 7.0);
        nlfDep->setParametersList(QStringList() << "A1" << "A2");
        nlfDep->setFormula("A1*x + A2");
        nlfDep->setInitialGuess(0, 3.0);
        nlfDep->setInitialGuess(1, 12.0);
        CHECK_COND(nlfDep->setLinearConstraintExpr("A2 = 4.0*A1"),
                   "setLinearConstraintExpr('A2 = 4.0*A1') failed");
        nlfDep->setParameterBounds(1, 0.0, 20.0); // Box bound on eliminated dependent parameter A2!
        nlfDep->fit();
        const double depA1 = nlfDep->results().at(0);
        const double depA2 = nlfDep->results().at(1);
        std::cout << "[INFO] Eliminated dependent bound fit: A1=" << depA1 << ", A2=" << depA2 << "\n";
        CHECK_COND(std::fabs(depA2 - 4.0 * depA1) < 1e-4 && depA2 <= 20.001 && depA2 > 18.0,
                   "Fit failed to enforce box bounds on eliminated dependent parameter A2");
        delete nlfDep;

        // 4. QuantileReg sparsity SEs in realistic range
        Table *tQr = mw->newTable(40, 2, "Suite31QrTbl", "Quantile Regression SE");
        for (int r = 0; r < 40; ++r) {
            const double xv = 0.25 * double(r);
            const double noise = 0.55 * std::sin(1.7 * double(r + 1)) + 0.25 * std::cos(2.9 * double(r));
            tQr->setCell(r, 0, xv);
            tQr->setCell(r, 1, 3.0 + 2.5 * xv + noise);
        }
        MultiLayer *mlQr = mw->multilayerPlot(
                tQr, QStringList() << (QString(tQr->name()) + "_2"), Graph::Scatter);
        Graph *gQr = mlQr->activeGraph();
        auto qrRes = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                mw, gQr, gQr->curve(0)->title().text(),
                PiecewiseAndRobustFit::QuantileReg, 0.5, false);
        std::cout << "[INFO] QuantileReg(tau=0.5): slope=" << qrRes.slope
                  << " +/- " << qrRes.slopeSE
                  << ", intercept=" << qrRes.intercept << " +/- " << qrRes.interceptSE << "\n";
        CHECK_COND(qrRes.converged
                           && std::fabs(qrRes.slope - 2.5) < 0.2
                           && qrRes.slopeSE > 0.005 && qrRes.slopeSE < 0.35
                           && qrRes.interceptSE > 0.02 && qrRes.interceptSE < 1.2,
                   "QuantileReg sparsity SEs out of realistic statistical range");

        // 5. DeltaCursorTool setActiveCursor single-cursor SnapNearestPoint stepping + FreeScreenCrosshair area==0
        Table *tDc = mw->newTable(11, 2, "Suite31DeltaTbl", "Discrete Curve");
        for (int r = 0; r <= 10; ++r) {
            tDc->setCell(r, 0, double(r));
            tDc->setCell(r, 1, 2.0 * double(r) + 1.0);
        }
        MultiLayer *mlDc = mw->multilayerPlot(
                tDc, QStringList() << (QString(tDc->name()) + "_2"), Graph::LineSymbols);
        Graph *gDc = mlDc->activeGraph();
        DeltaCursorTool *dcTool = DeltaCursorTool::attach(mw, gDc, DeltaCursorTool::SnapNearestPoint);
        CHECK_COND(dcTool != nullptr, "DeltaCursorTool::attach failed");
        dcTool->setCursorPositions(2.0, 7.0);
        dcTool->setActiveCursor(1);
        {
            QKeyEvent keRight(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
            QApplication::sendEvent(gDc->plotWidget()->canvas(), &keRight);
        }
        const auto roStep1 = dcTool->currentReadout();
        dcTool->setActiveCursor(2);
        {
            QKeyEvent keLeft(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
            QApplication::sendEvent(gDc->plotWidget()->canvas(), &keLeft);
        }
        const auto roStep2 = dcTool->currentReadout();
        dcTool->setSnapMode(DeltaCursorTool::FreeScreenCrosshair);
        dcTool->setCursorPoints2D(1.5, 4.0, 5.5, 12.0);
        const auto roFree = dcTool->currentReadout();
        std::cout << "[INFO] DeltaCursor SnapNearest step1=(" << roStep1.x1 << "," << roStep1.x2
                  << "), step2=(" << roStep2.x1 << "," << roStep2.x2
                  << "), FreeCrosshair area=" << roFree.area << ", rms=" << roFree.rms << "\n";
        CHECK_COND(std::fabs(roStep1.x1 - 3.0) < 1e-6 && std::fabs(roStep1.x2 - 7.0) < 1e-6,
                   "DeltaCursor activeCursor(1) Key_Right did not step C1 alone by 1 index");
        CHECK_COND(std::fabs(roStep2.x1 - 3.0) < 1e-6 && std::fabs(roStep2.x2 - 6.0) < 1e-6,
                   "DeltaCursor activeCursor(2) Key_Left did not step C2 alone by 1 index");
        CHECK_COND(std::fabs(roFree.dx - 4.0) < 1e-6 && std::fabs(roFree.dy - 8.0) < 1e-6
                           && roFree.area == 0.0 && roFree.rms == 0.0,
                   "FreeScreenCrosshair failed dx/dy or leaked non-zero curve area/rms");
        RoiGadgetTool::clearGadget(gDc);

        // 6. runOutlierTest Generalized ESD with 2 masking outliers (p < 0.05 on each step)
        Table *tEsd = mw->newTable(30, 1, "Suite31EsdTbl", "Masking Pair ESD");
        tEsd->setColName(0, "Val");
        for (int r = 0; r < 28; ++r)
            tEsd->setCell(r, 0, 10.0 + 0.35 * std::sin(double(r + 1) * 1.3));
        tEsd->setCell(28, 0, 52.0); // Masking outlier #1
        tEsd->setCell(29, 0, 50.5); // Masking outlier #2
        auto esdRes = GraphStatsAndSurfaceFit::runOutlierTest(
                mw, tEsd, "Val", 1 /*GeneralizedESD*/, 0.05, 5, false);
        std::cout << "[INFO] Generalized ESD: detected=" << esdRes.outlierRows.size()
                  << ", maxStat=" << esdRes.testStatistic << ", p=" << esdRes.pValue << "\n";
        CHECK_COND(esdRes.valid && esdRes.outlierRows.size() == 2
                           && esdRes.outlierRows.contains(28) && esdRes.outlierRows.contains(29)
                           && esdRes.pValue < 0.05 && esdRes.reportTable != nullptr,
                   "Generalized ESD failed to detect both masking outliers with p < 0.05");

        // 7. plotWeibullProbability KM censoring (1=Failure, 0=Censored) + 3-Parameter Weibull (gamma > 0)
        Table *tWb = mw->newTable(32, 3, "Suite31WeibullTbl", "KM & 3-Param Weibull");
        tWb->setColName(0, "Strength3P");
        tWb->setColName(1, "KMCensor");
        tWb->setColName(2, "StrengthKM");
        int expectedCens = 0;
        for (int r = 0; r < 32; ++r) {
            const double p = (double(r + 1) - 0.3) / (32.0 + 0.4);
            // Exact synthetic 3-parameter Weibull: gamma = 120.0, sigma0(scale) = 200.0, m = 3.5
            const double x3p = 120.0 + 200.0 * std::pow(-std::log(1.0 - p), 1.0 / 3.5);
            tWb->setCell(r, 0, x3p);
            const bool isCens = (r % 5 == 4); // 1=Failure, 0=Censored
            if (isCens)
                ++expectedCens;
            tWb->setCell(r, 1, isCens ? 0.0 : 1.0);
            tWb->setCell(r, 2, 400.0 * std::pow(-std::log(1.0 - p), 1.0 / 6.0));
        }
        auto wbKm = GraphStatsAndSurfaceFit::plotWeibullProbability(
                mw, tWb, "StrengthKM", "KMCensor", 0 /*1=Failure, 0=Censored*/, false);
        auto wb3p = GraphStatsAndSurfaceFit::plotWeibullProbability(
                mw, tWb, "Strength3P", QString(), 0, true /*fitThreeParam*/);
        std::cout << "[INFO] Weibull KM: censored=" << wbKm.censoredCount << "/" << expectedCens
                  << ", m=" << wbKm.modulusM << ", R2=" << wbKm.rSquare << "\n";
        std::cout << "[INFO] Weibull 3P: gamma=" << wb3p.thresholdGamma
                  << " (true 120), m=" << wb3p.modulusM
                  << " (true 3.5), scale=" << wb3p.scaleSigma0 << ", R2=" << wb3p.rSquare << "\n";
        CHECK_COND(wbKm.valid && wbKm.censoredCount == expectedCens && wbKm.rSquare > 0.92,
                   "Weibull KM censoring (1=Failure, 0=Censored) failed");
        CHECK_COND(wb3p.valid && wb3p.threeParameter
                           && wb3p.thresholdGamma > 85.0 && wb3p.thresholdGamma < 150.0
                           && wb3p.rSquare > 0.985,
                   "3-Parameter Weibull failed to recover positive synthetic threshold gamma ~ 120");

        // 8. .sciprj reload fidelity of fillBetweenCurves and plotHierarchicalDendrogram
        ApplicationWindow *wR2 = new ApplicationWindow();
        wR2->applyUserSettings();
        Table *tFb = wR2->newTable(15, 3, "FillTbl", "Two Curves");
        tFb->setColPlotDesignation(2, SciDAVis::Y);
        for (int r = 0; r < 15; ++r) {
            const double xv = double(r);
            tFb->setCell(r, 0, xv);
            tFb->setCell(r, 1, std::sin(0.3 * xv) + 2.0);
            tFb->setCell(r, 2, 0.5 * std::cos(0.3 * xv) + 1.0);
        }
        MultiLayer *mlFb = wR2->multilayerPlot(
                tFb, QStringList() << "FillTbl_2" << "FillTbl_3", Graph::Line);
        mlFb->setName("Suite31FillGraph");
        Graph *gFb = mlFb->activeGraph();
        QwtPlotCurve *fbDiff = CurveMathTools::fillBetweenCurves(
                wR2, gFb, gFb->curve(0)->title().text(), gFb->curve(1)->title().text());
        const int preSaveDiffPts = fbDiff ? fbDiff->dataSize() : 0;
        CHECK_COND(fbDiff != nullptr && gFb->curves() == 3 && preSaveDiffPts >= 100,
                   "fillBetweenCurves failed pre-save");

        Table *tHc = wR2->newTable(6, 2, "DendTbl", "6 Samples");
        tHc->setColName(0, "F1");
        tHc->setColName(1, "F2");
        const double hcPts[6][2] = { { 0.0, 0.0 }, { 0.4, 0.2 }, { 5.0, 5.0 },
                                     { 5.3, 4.8 }, { 12.0, 0.5 }, { 12.4, 0.9 } };
        for (int r = 0; r < 6; ++r) {
            tHc->setCell(r, 0, hcPts[r][0]);
            tHc->setCell(r, 1, hcPts[r][1]);
        }
        MultiLayer *mlDend = GraphStatsAndSurfaceFit::plotHierarchicalDendrogram(
                wR2, tHc, QStringList() << "F1" << "F2", 0 /*Ward*/);
        CHECK_COND(mlDend != nullptr && mlDend->activeGraph() != nullptr
                           && mlDend->activeGraph()->curves() == 5,
                   "plotHierarchicalDendrogram failed pre-save");
        const QString dendWinName = mlDend->name();

        QByteArray r2ProjBytes = wR2->saveProjectToByteArray();
        QString r2ProjPath = QDir::tempPath() + "/torigraph_suite31_r2_reload.sciprj";
        {
            QFile f(r2ProjPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "Failed to open Suite 31 temp .sciprj");
            f.write(r2ProjBytes);
            f.close();
        }
        wR2->savedProject();
        wR2->setOleEmbeddedMode(true);
        delete wR2;

        ApplicationWindow *rR2 = new ApplicationWindow();
        rR2->applyUserSettings();
        bool r2Loaded = rR2->loadProject(r2ProjPath);
        QFile::remove(r2ProjPath);
        CHECK_COND(r2Loaded, "Suite 31 loadProject failed");

        MultiLayer *mlFbRel = qobject_cast<MultiLayer *>(rR2->window("Suite31FillGraph"));
        MultiLayer *mlDendRel = qobject_cast<MultiLayer *>(rR2->window(dendWinName));
        CHECK_COND(mlFbRel != nullptr && mlFbRel->activeGraph() != nullptr,
                   "Reloaded project missing Suite31FillGraph");
        CHECK_COND(mlFbRel->activeGraph()->curves() == 3
                           && mlFbRel->activeGraph()->curve(2)->dataSize() == preSaveDiffPts
                           && mlFbRel->activeGraph()->saveToString().contains("kind=\"band\""),
                   "fillBetweenCurves hidden table or ToriRefLine kind='band' lost across .sciprj reload");
        CHECK_COND(mlDendRel != nullptr && mlDendRel->activeGraph() != nullptr
                           && mlDendRel->activeGraph()->curves() == 5
                           && mlDendRel->activeGraph()->curve(0)->dataSize() == 4,
                   "plotHierarchicalDendrogram bracket curves lost across .sciprj reload");

        rR2->savedProject();
        rR2->setOleEmbeddedMode(true);
        delete rR2;
    }

    // =========================================================================
    // 32. Suite 32: 2D ImageProfileGadgetTool & filterMatrix2D
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 32: 2D ImageProfileGadgetTool & filterMatrix2D...\n";

        // 1. ImageProfileGadgetTool on anisotropic 2D Gaussian Spectrogram
        //    sigma_x = 2.0 (FWHM_x = 4.7096), sigma_y = 3.0 (FWHM_y = 7.0645)
        Matrix *mGauss = mw->newMatrix(41, 41);
        mGauss->setName("Suite32GaussMat");
        mGauss->setCoordinates(-10.0, 10.0, -10.0, 10.0);
        for (int r = 0; r < 41; ++r) {
            const double yv = -10.0 + 0.5 * double(r);
            for (int c = 0; c < 41; ++c) {
                const double xv = -10.0 + 0.5 * double(c);
                const double zv = 100.0 * std::exp(-(xv * xv / (2.0 * 4.0) + yv * yv / (2.0 * 9.0)));
                mGauss->setCell(r, c, zv);
            }
        }
        MultiLayer *mlImg = mw->plotColorMap(mGauss);
        Graph *gImg = mlImg ? mlImg->activeGraph() : nullptr;
        CHECK_COND(gImg != nullptr, "plotColorMap failed for Suite 32");

        ImageProfileGadgetTool *ipg = ImageProfileGadgetTool::attach(gImg, mw);
        CHECK_COND(ipg != nullptr, "ImageProfileGadgetTool::attach failed");
        ipg->setCrosshairPosition(0.0, 0.0);
        std::cout << "[INFO] ImageProfileGadget(0,0): Z=" << ipg->currentZ()
                  << ", horizFWHM=" << ipg->horizontalFwhm()
                  << ", vertFWHM=" << ipg->verticalFwhm() << "\n";
        CHECK_COND(std::fabs(ipg->currentZ() - 100.0) < 0.5
                           && std::fabs(ipg->horizontalFwhm() - 4.7096) < 0.35
                           && std::fabs(ipg->verticalFwhm() - 7.0645) < 0.45
                           && ipg->horizontalProfile().size() >= 41
                           && ipg->verticalProfile().size() >= 41,
                   "ImageProfileGadgetTool failed currentZ or 2D Gaussian horizontal/vertical FWHM");

        const int winBeforeExtract = mw->windowsList().size();
        Table *profTbl = ipg->extractProfilesToTableAndGraph(true);
        CHECK_COND(profTbl != nullptr && profTbl->numRows() >= 41
                           && mw->windowsList().size() > winBeforeExtract,
                   "extractProfilesToTableAndGraph failed to create profile table and graph");
        MultiLayer *mlProf = nullptr;
        for (MyWidget *w : mw->windowsList()) {
            if (auto *cand = qobject_cast<MultiLayer *>(w))
                mlProf = cand;
        }
        CHECK_COND(mlProf != nullptr && mlProf->activeGraph() != nullptr,
                   "Profile MultiLayer missing after extractProfilesToTableAndGraph");
        Graph *gProf = mlProf->activeGraph();
        CHECK_COND(gProf->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                           && gProf->axisTitleFont(QwtPlot::yLeft).family() == "Arial"
                           && gProf->axisFont(QwtPlot::xBottom).family() == "Arial"
                           && gProf->axisFont(QwtPlot::yLeft).family() == "Arial",
                   "Extracted 2D profile graph violated Arial font invariant");
        RoiGadgetTool::clearGadget(gImg);

        // 2. AdvancedSignalProc::filterMatrix2D (Median2D despike, Gaussian2D, SubtractPlanarTilt2D)
        Matrix *mSpike = mw->newMatrix(21, 21);
        mSpike->setName("Suite32SpikeMat");
        mSpike->setCoordinates(0.0, 20.0, 0.0, 20.0);
        for (int r = 0; r < 21; ++r)
            for (int c = 0; c < 21; ++c)
                mSpike->setCell(r, c, 10.0);
        mSpike->setCell(10, 10, 1010.0); // Cosmic-ray spike (+1000 above background 10)

        Matrix *mMed = AdvancedSignalProc::filterMatrix2D(
                mw, mSpike, AdvancedSignalProc::FilterMedian2D, 1.0, 1, false, false);
        Matrix *mGau = AdvancedSignalProc::filterMatrix2D(
                mw, mSpike, AdvancedSignalProc::FilterGaussian2D, 1.5, 2, false, false);
        CHECK_COND(mMed != nullptr && std::fabs(mMed->cell(10, 10) - 10.0) < 1e-9,
                   "FilterMedian2D failed to remove isolated cosmic-ray spike");
        CHECK_COND(mGau != nullptr && mGau->cell(10, 10) > 40.0 && mGau->cell(10, 10) < 200.0
                           && mGau->cell(10, 11) > mGau->cell(10, 12),
                   "FilterGaussian2D failed radially symmetric smoothing");

        Matrix *mTilt = mw->newMatrix(21, 21);
        mTilt->setName("Suite32TiltMat");
        mTilt->setCoordinates(-5.0, 5.0, -5.0, 5.0);
        for (int r = 0; r < 21; ++r) {
            const double yv = -5.0 + 0.5 * double(r);
            for (int c = 0; c < 21; ++c) {
                const double xv = -5.0 + 0.5 * double(c);
                mTilt->setCell(r, c, 7.5 + 1.35 * xv - 0.85 * yv);
            }
        }
        Matrix *mFlat = AdvancedSignalProc::filterMatrix2D(
                mw, mTilt, AdvancedSignalProc::FilterSubtractPlanarTilt2D, 1.0, 1, false, false);
        double maxTiltResid = 0.0;
        if (mFlat) {
            for (int r = 0; r < 21; ++r)
                for (int c = 0; c < 21; ++c)
                    maxTiltResid = std::max(maxTiltResid, std::fabs(mFlat->cell(r, c)));
        }
        std::cout << "[INFO] FilterSubtractPlanarTilt2D maxAbsResid=" << maxTiltResid << "\n";
        CHECK_COND(mFlat != nullptr && maxTiltResid < 1e-9,
                   "FilterSubtractPlanarTilt2D failed exact plane removal (< 1e-9)");
    }

    // =========================================================================
    // 33. Suite 33: EIS Nyquist/Bode Randles-CPE, Fourier Self-Deconvolution
    //               & Passing-Bablok / Deming Regression
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 33: EIS Randles-CPE, Fourier Self-Deconvolution & Passing-Bablok/Deming...\n";

        // 1. CurveMathTools::analyzeEIS on synthetic Randles-CPE (Rs=25, Rct=180, Q=1.5e-5, n=0.92)
        const int nEis = 36;
        Table *tEis = mw->newTable(nEis, 3, "Suite33EisTbl", "Synthetic Randles-CPE");
        tEis->setColName(0, "Freq_Hz");
        tEis->setColName(1, "Z_re");
        tEis->setColName(2, "Z_im");
        const double trueRs = 25.0, trueRct = 180.0, trueQ = 1.5e-5, trueN = 0.92;
        for (int i = 0; i < nEis; ++i) {
            const double logF = -1.0 + 6.0 * double(i) / double(nEis - 1); // 0.1 Hz .. 1e5 Hz
            const double f = std::pow(10.0, logF);
            const double omega = 2.0 * M_PI * f;
            const double alpha = trueRct * trueQ * std::pow(omega, trueN);
            const double ang = 0.5 * M_PI * trueN;
            const double denomRe = 1.0 + alpha * std::cos(ang);
            const double denomIm = alpha * std::sin(ang);
            const double denomSq = denomRe * denomRe + denomIm * denomIm;
            const double zRe = trueRs + trueRct * denomRe / denomSq;
            const double zIm = -trueRct * denomIm / denomSq; // Negative capacitive Z''
            tEis->setCell(i, 0, f);
            tEis->setCell(i, 1, zRe);
            tEis->setCell(i, 2, zIm);
        }
        auto eis = CurveMathTools::analyzeEIS(
                mw, tEis, "Freq_Hz", "Z_re", "Z_im", false, true);
        std::cout << "[INFO] EIS Randles-CPE: Rs=" << eis.rs << " (true 25), Rct=" << eis.rct
                  << " (true 180), n=" << eis.nCpe << " (true 0.92), rmse=" << eis.rmse << "\n";
        CHECK_COND(eis.valid
                           && std::fabs(eis.rs - trueRs) / trueRs < 0.03
                           && std::fabs(eis.rct - trueRct) / trueRct < 0.03
                           && std::fabs(eis.nCpe - trueN) < 0.04
                           && eis.nyquistPlot != nullptr && eis.bodePlot != nullptr,
                   "CurveMathTools::analyzeEIS failed 3% accuracy on Rs / Rct");
        CHECK_COND(eis.nyquistPlot->activeGraph()->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                           && eis.bodePlot->activeGraph()->axisTitleFont(QwtPlot::xBottom).family() == "Arial",
                   "EIS Nyquist/Bode graphs violated Arial font invariant");

        // 2. AdvancedSignalProc::fourierSelfDeconvolve sharpening overlapping Lorentzian doublet
        const int nFsd = 256;
        Table *tFsd = mw->newTable(nFsd, 2, "Suite33FsdTbl", "Overlapping Lorentzian Doublet");
        const double gammaFsd = 5.0;
        const double hwhm = 0.5 * gammaFsd;
        double rawArea = 0.0;
        for (int i = 0; i < nFsd; ++i) {
            const double xv = 50.0 * double(i) / double(nFsd - 1);
            const double l1 = 10.0 * (hwhm * hwhm) / ((xv - 22.0) * (xv - 22.0) + hwhm * hwhm);
            const double l2 = 10.0 * (hwhm * hwhm) / ((xv - 28.0) * (xv - 28.0) + hwhm * hwhm);
            const double yv = l1 + l2;
            tFsd->setCell(i, 0, xv);
            tFsd->setCell(i, 1, yv);
            if (i > 0)
                rawArea += 0.5 * (tFsd->cell(i - 1, 1) + yv) * (xv - tFsd->cell(i - 1, 0));
        }
        MultiLayer *mlFsd = mw->multilayerPlot(
                tFsd, QStringList() << (QString(tFsd->name()) + "_2"), Graph::Line);
        Graph *gFsd = mlFsd->activeGraph();
        const QString rawCurveName = gFsd->curve(0)->title().text();
        const double rawPeakY = CurveMathTools::findYFromX(gFsd, rawCurveName, 22.0);
        const double rawValleyY = CurveMathTools::findYFromX(gFsd, rawCurveName, 25.0);

        QwtPlotCurve *fsdCurve = AdvancedSignalProc::fourierSelfDeconvolve(
                mw, gFsd, rawCurveName, gammaFsd, 1.8, 0 /*Bessel*/);
        CHECK_COND(fsdCurve != nullptr && fsdCurve->dataSize() == nFsd,
                   "fourierSelfDeconvolve returned null or wrong point count");
        double fsdArea = 0.0;
        for (int i = 1; i < fsdCurve->dataSize(); ++i)
            fsdArea += 0.5 * (fsdCurve->y(i - 1) + fsdCurve->y(i)) * (fsdCurve->x(i) - fsdCurve->x(i - 1));
        const double fsdPeakY = CurveMathTools::findYFromX(gFsd, fsdCurve->title().text(), 22.0);
        const double fsdValleyY = CurveMathTools::findYFromX(gFsd, fsdCurve->title().text(), 25.0);
        std::cout << "[INFO] FSD Doublet: rawValley/Peak=" << (rawValleyY / rawPeakY)
                  << " -> fsdValley/Peak=" << (fsdValleyY / fsdPeakY)
                  << ", areaRatio=" << (fsdArea / rawArea) << "\n";
        CHECK_COND(fsdPeakY > rawPeakY * 1.15
                           && (fsdValleyY / fsdPeakY) < (rawValleyY / rawPeakY) * 0.75
                           && std::fabs(fsdArea - rawArea) / rawArea < 0.06,
                   "fourierSelfDeconvolve failed to sharpen Lorentzian doublet while conserving area");

        // 3. GraphStatsAndSurfaceFit::runPassingBablokDeming with outlier resistance
        Table *tPbd = mw->newTable(28, 2, "Suite33PbdTbl", "Method Comparison");
        tPbd->setColName(0, "MethodA");
        tPbd->setColName(1, "MethodB");
        for (int r = 0; r < 26; ++r) {
            const double xv = 2.0 + 1.5 * double(r);
            const double jitter = 0.12 * std::sin(double(r + 1) * 1.9);
            tPbd->setCell(r, 0, xv);
            tPbd->setCell(r, 1, 4.0 + 1.25 * xv + jitter);
        }
        // Inject 2 gross outliers that heavily skew OLS/Deming but not Passing-Bablok median-of-slopes
        tPbd->setCell(26, 0, 14.0);
        tPbd->setCell(26, 1, 95.0);
        tPbd->setCell(27, 0, 32.0);
        tPbd->setCell(27, 1, -25.0);

        auto pbd = GraphStatsAndSurfaceFit::runPassingBablokDeming(
                mw, tPbd, "MethodA", "MethodB", 1.0, true);
        std::cout << "[INFO] Passing-Bablok: slope=" << pbd.pbSlope
                  << " [" << pbd.pbSlopeCILow << ", " << pbd.pbSlopeCIHigh << "]"
                  << ", intercept=" << pbd.pbIntercept
                  << " [" << pbd.pbInterceptCILow << ", " << pbd.pbInterceptCIHigh << "]"
                  << ", Deming slope=" << pbd.demingSlope << " +/- " << pbd.demingSlopeSE << "\n";
        CHECK_COND(pbd.valid
                           && std::fabs(pbd.pbSlope - 1.25) < 0.04
                           && std::fabs(pbd.pbIntercept - 4.0) < 0.6
                           && std::isfinite(pbd.pbSlopeCILow) && std::isfinite(pbd.pbSlopeCIHigh)
                           && pbd.pbSlopeCILow < 1.25 && pbd.pbSlopeCIHigh > 1.25
                           && std::isfinite(pbd.demingSlope) && pbd.demingSlopeSE > 0.0
                           && pbd.comparisonPlot != nullptr,
                   "runPassingBablokDeming failed outlier-resistant recovery or finite 95% CIs");
        CHECK_COND(pbd.comparisonPlot->activeGraph()->axisTitleFont(QwtPlot::xBottom).family() == "Arial",
                   "Passing-Bablok/Deming plot violated Arial font invariant");
    }

    // =========================================================================
    // 34. Suite 34: Colorbar Viridis & Arial, copyPageAsImage(300),
    //               RGBA EmfPaintDevice Alpha & FrameOpenL Backbone Preservation
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 34: Colorbar Viridis/Arial, copyPageAsImage(300), RGBA EMF & FrameOpenL...\n";

        // 1. Spectrogram PresetViridis + custom Z range + colorbar title .sciprj round-trip & Arial
        ApplicationWindow *cbWriter = new ApplicationWindow();
        cbWriter->applyUserSettings();
        Matrix *mCb = cbWriter->newMatrix(20, 20);
        mCb->setName("Suite34CbMat");
        mCb->setCoordinates(0.0, 10.0, 0.0, 10.0);
        for (int r = 0; r < 20; ++r)
            for (int c = 0; c < 20; ++c)
                mCb->setCell(r, c, double(r * 5 + c));
        MultiLayer *mlCb = cbWriter->plotColorMap(mCb);
        mlCb->setName("Suite34ColorbarGraph");
        Graph *gCb = mlCb->activeGraph();
        Spectrogram *spOrig = nullptr;
        for (QwtPlotItem *it : gCb->plotWidget()->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                spOrig = dynamic_cast<Spectrogram *>(it);
                if (spOrig)
                    break;
            }
        }
        CHECK_COND(spOrig != nullptr, "Spectrogram missing on plotColorMap graph");
        spOrig->showColorScale(QwtPlot::yRight, true);
        spOrig->setColorMapPreset(Spectrogram::PresetViridis, false);
        spOrig->setCustomValueRange(true, 15.0, 85.0);
        spOrig->setColorBarTitle("Intensity (arb. u.)");
        gCb->replot();

        QwtScaleWidget *rightSwOrig = gCb->plotWidget()->axisWidget(QwtPlot::yRight);
        CHECK_COND(rightSwOrig != nullptr
                           && rightSwOrig->font().family() == "Arial"
                           && rightSwOrig->title().font().family() == "Arial",
                   "Colorbar axis/title font on pre-save Spectrogram is not Arial");

        // 2. MultiLayer::copyPageAsImage(300) DPI (~11811 dpm) & non-blank pixels
        QImage pageClipImg = mlCb->copyPageAsImage(300);
        const int expectedDpm300 = qRound(300.0 / 0.0254);
        int nonWhiteClipPx = 0;
        for (int py = 0; py < pageClipImg.height(); py += 2) {
            for (int px = 0; px < pageClipImg.width(); px += 2) {
                QRgb rgb = pageClipImg.pixel(px, py);
                if (qRed(rgb) < 245 || qGreen(rgb) < 245 || qBlue(rgb) < 245)
                    ++nonWhiteClipPx;
            }
        }
        std::cout << "[INFO] copyPageAsImage(300): size=" << pageClipImg.width() << "x"
                  << pageClipImg.height() << ", dpmX=" << pageClipImg.dotsPerMeterX()
                  << " (expected " << expectedDpm300 << "), nonWhiteSampled=" << nonWhiteClipPx << "\n";
        CHECK_COND(!pageClipImg.isNull()
                           && std::abs(pageClipImg.dotsPerMeterX() - expectedDpm300) <= 2
                           && std::abs(pageClipImg.dotsPerMeterY() - expectedDpm300) <= 2
                           && nonWhiteClipPx > 200,
                   "MultiLayer::copyPageAsImage(300) failed DPI metadata or rendered blank");

        QByteArray cbBytes = cbWriter->saveProjectToByteArray();
        QString cbPath = QDir::tempPath() + "/torigraph_suite34_colorbar.sciprj";
        {
            QFile f(cbPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "Failed to open Suite 34 temp .sciprj");
            f.write(cbBytes);
            f.close();
        }
        cbWriter->savedProject();
        cbWriter->setOleEmbeddedMode(true);
        delete cbWriter;

        ApplicationWindow *cbReader = new ApplicationWindow();
        cbReader->applyUserSettings();
        bool cbLoaded = cbReader->loadProject(cbPath);
        QFile::remove(cbPath);
        CHECK_COND(cbLoaded, "Suite 34 loadProject failed");

        MultiLayer *mlCbRel = qobject_cast<MultiLayer *>(cbReader->window("Suite34ColorbarGraph"));
        CHECK_COND(mlCbRel != nullptr && mlCbRel->activeGraph() != nullptr,
                   "Reloaded project missing Suite34ColorbarGraph");
        Graph *gCbRel = mlCbRel->activeGraph();
        Spectrogram *spRel = nullptr;
        for (QwtPlotItem *it : gCbRel->plotWidget()->itemList()) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                spRel = dynamic_cast<Spectrogram *>(it);
                if (spRel)
                    break;
            }
        }
        QwtScaleWidget *rightSwRel = gCbRel->plotWidget()->axisWidget(QwtPlot::yRight);
        CHECK_COND(spRel != nullptr
                           && spRel->colorMapPreset() == Spectrogram::PresetViridis
                           && spRel->hasCustomValueRange()
                           && std::fabs(spRel->customZMin() - 15.0) < 1e-6
                           && std::fabs(spRel->customZMax() - 85.0) < 1e-6
                           && spRel->colorBarTitle() == "Intensity (arb. u.)"
                           && rightSwRel != nullptr
                           && rightSwRel->font().family() == "Arial"
                           && rightSwRel->title().font().family() == "Arial",
                   "Spectrogram Viridis preset, custom Z range, colorbar title, or Arial font lost across .sciprj reload");

        cbReader->savedProject();
        cbReader->setOleEmbeddedMode(true);
        delete cbReader;

        // 3. RGBA QImage alpha-blending over colored background in EmfPaintDevice
        QString rgbaPngPath = QDir::tempPath() + "/torigraph_suite34_rgba_overlay.png";
        {
            QImage rgbaOverlay(120, 120, QImage::Format_ARGB32);
            rgbaOverlay.fill(QColor(240, 0, 0, 128)); // 50% translucent pure red
            CHECK_COND(rgbaOverlay.save(rgbaPngPath, "PNG"), "Failed to save temp RGBA PNG");
        }
        MultiLayer *mlRgba = mw->newGraph("Suite34RgbaEmfGraph");
        mlRgba->resize(420, 360);
        Graph *gRgba = mlRgba->activeGraph();
        gRgba->setCanvasBackground(QColor(0, 60, 220)); // Royal blue background
        gRgba->setScale(QwtPlot::xBottom, 0.0, 10.0);
        gRgba->setScale(QwtPlot::yLeft, 0.0, 10.0);
        ImageMarker *imRgba = gRgba->addImage(rgbaPngPath);
        CHECK_COND(imRgba != nullptr, "Graph::addImage failed for RGBA PNG");
        imRgba->setBoundingRect(1.0, 9.0, 9.0, 1.0);
        gRgba->replot();

        OleRenderedGraph emfRgba = SciDAVisEmf::renderMultiLayer(mlRgba);
        HENHMETAFILE hEmf34 = SciDAVisEmf::createHemfFromBytes(emfRgba.emfBytes);
        QFile::remove(rgbaPngPath);
        CHECK_COND(hEmf34 != nullptr, "renderMultiLayer failed for RGBA ImageMarker graph");

        QImage emfRgbaPlay(300, 300, QImage::Format_RGB32);
        emfRgbaPlay.fill(Qt::white);
        {
            HDC screenDc = GetDC(nullptr);
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = 300;
            bmi.bmiHeader.biHeight = -300;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void *dibBits = nullptr;
            HBITMAP hDib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
            HDC memDc = CreateCompatibleDC(screenDc);
            HGDIOBJ oldBmp = SelectObject(memDc, hDib);
            RECT playRc = { 0, 0, 300, 300 };
            HBRUSH whiteBr = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDc, &playRc, whiteBr);
            DeleteObject(whiteBr);
            PlayEnhMetaFile(memDc, hEmf34, &playRc);
            GdiFlush();
            if (dibBits)
                std::memcpy(emfRgbaPlay.bits(), dibBits, 300 * 300 * 4);
            SelectObject(memDc, oldBmp);
            DeleteObject(hDib);
            DeleteDC(memDc);
            ReleaseDC(nullptr, screenDc);
        }
        DeleteEnhMetaFile(hEmf34);

        int purpleBlendedPx = 0;
        for (int py = 0; py < 300; ++py) {
            for (int px = 0; px < 300; ++px) {
                QRgb c = emfRgbaPlay.pixel(px, py);
                const int r = qRed(c), g = qGreen(c), b = qBlue(c);
                // 50% red (240,0,0) blended directly over royal blue (0,60,220) => ~(120, 30, 110) purple!
                // (If flattened onto white first, green would be ~128 instead of ~30!)
                if (r > 80 && r < 180 && b > 70 && b < 170 && g < 65)
                    ++purpleBlendedPx;
            }
        }
        std::cout << "[INFO] RGBA EmfPaintDevice playback: purpleBlendedPx=" << purpleBlendedPx << "\n";
        CHECK_COND(purpleBlendedPx > 500,
                   "EmfPaintDevice failed true per-pixel RGBA alpha-blending over colored background");

        // 4. Graph::showAxis preserving FrameOpenL hidden top/right backbones
        MultiLayer *mlOpenL = mw->newGraph("Suite34OpenLGraph");
        Graph *gOpenL = mlOpenL->activeGraph();
        gOpenL->applyFramePreset(Graph::FrameOpenL);
        CHECK_COND(!gOpenL->isAxisBackboneVisible(QwtPlot::xTop)
                           && !gOpenL->isAxisBackboneVisible(QwtPlot::yRight),
                   "FrameOpenL initial top/right backbones were not hidden");
        gOpenL->showAxis(QwtPlot::xTop, Graph::AxisType::Numeric, QString(), nullptr, true,
                         0, 0, false, Qt::black, 0, 6, 0, 0,
                         QString(), Qt::black);
        gOpenL->showAxis(QwtPlot::yRight, Graph::AxisType::Numeric, QString(), nullptr, true,
                         0, 0, false, Qt::black, 0, 6, 0, 0,
                         QString(), Qt::black);
        CHECK_COND(!gOpenL->isAxisBackboneVisible(QwtPlot::xTop)
                           && !gOpenL->isAxisBackboneVisible(QwtPlot::yRight)
                           && !gOpenL->plotWidget()->axisScaleDraw(QwtPlot::xTop)->hasComponent(
                                      QwtAbstractScaleDraw::Backbone)
                           && !gOpenL->plotWidget()->axisScaleDraw(QwtPlot::yRight)->hasComponent(
                                      QwtAbstractScaleDraw::Backbone),
                   "Graph::showAxis clobbered FrameOpenL hidden top/right backbones!");
    }

    // =========================================================================
    // 35. Suite 35: Runtime ToriGraphFullJapaneseTranslator Zero-Miss & Arial Gate
    // =========================================================================
    {
        std::cout << "[INFO] Running Suite 35: Runtime Japanese Translator Zero-Miss & Arial Gate...\n";

        mw->switchToLanguage("ja");

        struct RuntimeCtxCheck {
            const char *ctx;
            const char *src;
        };
        const RuntimeCtxCheck r2Checks[] = {
            // PlotDialog & Spectrogram Colorbar (Round 2)
            { "PlotDialog", "Colorbar Title" },
            { "PlotDialog", "Custom Z Range" },
            { "PlotDialog", "Z Min" },
            { "PlotDialog", "Z Max" },
            // 2D ImageProfileGadgetTool & DeltaCursorTool (Round 2)
            { "ImageProfileGadgetTool", "Close 2D Profile Gadget" },
            { "DeltaCursorTool", "Nearest Point" },
            // 2D Matrix Filter & Fourier Self-Deconvolution (Round 2)
            { "QObject", "2D Filter Method:" },
            { "QObject", "Kernel Radius (pixels):" },
            { "QObject", "Modify Source Matrix In-Place (Undoable)" },
            { "QObject", "Plot Filtered 2D Heatmap" },
            { "QObject", "Median 2D Filter (Despike)" },
            { "QObject", "Fourier Self-Deconvolution (Peak Sharpening)" },
            { "QObject", "Bessel / Kauppinen (1 - |t|/t_max)^2" },
            { "QObject", "Gaussian Apodization" },
            // EIS Nyquist/Bode & CurveMathTools (Round 2)
            { "QObject", "EIS Nyquist & Bode Analyzer (Randles-CPE)" },
            { "QObject", "Fit Randles + CPE Equivalent Circuit (Rs + Rct || CPE)" },
            { "QObject", "Imaginary column is already negated (-Z'' > 0)" },
            { "CurveMathTools", "Normalize Curves" },
            { "CurveMathTools", "Curve Intersections & X/Y Lookup" },
            // Passing-Bablok/Deming, 3-Param Weibull, Generalized ESD & Clustering (Round 2)
            { "QObject", "Weibull Probability Plot" },
            { "QObject", "3-Parameter Weibull (Estimate Threshold \316\263)" },
            { "QObject", "1 = Failure/Event, 0 = Censored [Kaplan-Meier Standard]" },
            { "QObject", "Method-comparison regression with errors in both X and Y (Passing-Bablok & Deming)" },
            { "QObject", "Cluster Analysis (K-Means & Hierarchical Dendrogram)" },
            { "QObject", "Hierarchical Dendrogram" },
            { "QObject", "Ward's Minimum Variance" },
            { "QObject", "Max Outliers k (Generalized ESD):" },
            // Phase 5 Iter 1 + Iter 2 ApplicationWindow / Fitting / Menus
            { "ApplicationWindow", "Presentation Slide (28 pt Arial)" },
            { "ApplicationWindow", "Duplicate (Batch Plotting)..." },
            { "FitDialog", "Linear Constraints:" }
        };

        auto hasJapaneseChars = [](const QString &s) {
            for (const QChar &ch : s) {
                if (ch.unicode() >= 0x3000)
                    return true;
            }
            return false;
        };

        int runtimeMisses = 0;
        const int nChecks = int(sizeof(r2Checks) / sizeof(r2Checks[0]));
        for (int i = 0; i < nChecks; ++i) {
            const QString srcStr = QString::fromUtf8(r2Checks[i].src);
            const QString jaStr = QCoreApplication::translate(r2Checks[i].ctx, r2Checks[i].src);
            if (jaStr.isEmpty() || jaStr == srcStr || !hasJapaneseChars(jaStr)) {
                ++runtimeMisses;
                std::cerr << "[MISS] translate('" << r2Checks[i].ctx << "', '" << r2Checks[i].src
                          << "') -> '" << jaStr.toUtf8().constData() << "'\n";
            }
        }
        std::cout << "[INFO] Runtime QCoreApplication::translate('ja') checked=" << nChecks
                  << ", misses=" << runtimeMisses << "\n";
        CHECK_COND(runtimeMisses == 0,
                   "Runtime ToriGraphFullJapaneseTranslator missed 1 or more Phase 5 Iter 1/2 strings!");

        // Verify newly created 2D curve graph + Spectrogram colorbar in "ja" mode strictly keep "Arial"!
        Table *tJa35 = mw->newTable(8, 2, "Suite35JaArialTbl", "JA Mode Arial Check");
        tJa35->column(0)->setLongName("Frequency");
        tJa35->column(0)->setUnits("Hz");
        tJa35->column(1)->setLongName("Impedance");
        tJa35->column(1)->setUnits("Ohm");
        for (int r = 0; r < 8; ++r) {
            tJa35->setCell(r, 0, double(r + 1));
            tJa35->setCell(r, 1, double((r + 1) * 3));
        }
        MultiLayer *mlJaCurve = mw->multilayerPlot(
                tJa35, QStringList() << (QString(tJa35->name()) + "_2"), Graph::LineSymbols);
        Graph *gJaCurve = mlJaCurve->activeGraph();
        if (!gJaCurve->legend())
            gJaCurve->newLegend();

        Matrix *mJaSpec = mw->newMatrix(12, 12);
        mJaSpec->setName("Suite35JaSpecMat");
        for (int r = 0; r < 12; ++r)
            for (int c = 0; c < 12; ++c)
                mJaSpec->setCell(r, c, double(r + c));
        MultiLayer *mlJaSpec = mw->plotColorMap(mJaSpec);
        Graph *gJaSpec = mlJaSpec->activeGraph();
        for (QwtPlotItem *it : gJaSpec->plotWidget()->itemList()) {
            if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
                sp->showColorScale(QwtPlot::yRight, true);
                sp->setColorMapPreset(Spectrogram::PresetViridis, false);
                sp->setColorBarTitle("Counts");
            }
        }
        gJaSpec->replot();
        QwtScaleWidget *jaCbWidget = gJaSpec->plotWidget()->axisWidget(QwtPlot::yRight);

        CHECK_COND(gJaCurve->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                           && gJaCurve->axisTitleFont(QwtPlot::yLeft).family() == "Arial"
                           && gJaCurve->axisFont(QwtPlot::xBottom).family() == "Arial"
                           && gJaCurve->axisFont(QwtPlot::yLeft).family() == "Arial"
                           && gJaCurve->legend() != nullptr
                           && gJaCurve->legend()->font().family() == "Arial",
                   "2D curve graph created in Japanese mode violated Arial font invariant!");
        CHECK_COND(gJaSpec->axisTitleFont(QwtPlot::xBottom).family() == "Arial"
                           && gJaSpec->axisFont(QwtPlot::xBottom).family() == "Arial"
                           && jaCbWidget != nullptr
                           && jaCbWidget->font().family() == "Arial"
                           && jaCbWidget->title().font().family() == "Arial",
                   "Spectrogram / Colorbar created in Japanese mode violated Arial font invariant!");

        mw->switchToLanguage("en");
    }

    if (legacyOnly) {
        mw->savedProject();
        std::cout << "[PASS] Legacy Suites 1..35 passed.\n";
        mw->setOleEmbeddedMode(true);
        delete mw;
        SciDAVisOle::shutdownOleServer();
        return 0;
    }

    if (runSuite36Round3SurgicalZero(mw) != 0)
        return 1;
    if (runSuite37DuotoneIconsAndTheme(mw) != 0)
        return 1;
    if (runSuite38RibbonSplitPaletteAndFocusHud(mw) != 0)
        return 1;
    if (runSuite39GitHubAutoUpdaterEngine(mw) != 0)
        return 1;
    if (runSuite40PythonJsonRpcServerDispatch(mw) != 0)
        return 1;
    if (runSuite41Phase6JapaneseZeroMissAndArialGate(mw) != 0)
        return 1;

    mw->savedProject();
    if (runSuite42Round2CriticRegressionGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite43Round3CriticRegressionGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite44Round4UnifiedRegressionGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite45Round5ZeroDefectGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite46OleAntialiasingAndSubpixelSmoothnessGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite47Phase7GraphGuiUsabilityAndZeroLeakGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite48Phase8RibbonDockWorksheetAndGraphErgonomicsGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite49PhaseBUxRenderMatSciAndApiGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite50MegaWave1PlotsMatSciAutonomyUndoPerf(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite51Campaign6FinalEndToEndInvariants(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite52Phase12OpjuApiGraphMathAndGuiGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite53SaveSafetyUndoAndRecovery(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite70IconCleanupAndLightTheme(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite71LabSlideTextAndZoomFollowsText(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite64XlsxImport(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite65E11ExtraGraphTypes(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite58FigureDefaultsPresetsAndExport(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite55TensileAndWorkHardeningCorrectness(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite57NumericalRobustnessXrdWeibullKissinger(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite56FitConvergenceAndStatistics(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite59DataImportAndWorksheet(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite63JapaneseUiAccessibilityGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite61DistributionVersionAndUpdaterSafety(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite60PowerPointOleLink(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite62BeginnerNavigationMenusAndScreenFit(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite67Density2DAndPoleFigure(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite68RibbonFoldingRecentModelsAndMiniToolbarKeys(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite66Workbooks(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite69SettingsTransferAndScriptCoverage(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite72ToriGraphStandardDefaultPalette(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite74LegacyUiHidden(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite73SendToPowerPoint(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite75CopyAndExportKeepGraphState(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite76ReviewPolishAndEdgeCases(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite77Nov22NightReviewRegressions(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite78ZeroBaseUxAndQualityGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite79ZeroBaseOverhaulV2Gate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite80Round4P0P1RegressionGate(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite81NumericalTransformCorrectness(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite82JsonRpcParamValidation(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite83ImportRobustness(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite84ProjectSerializationFidelity(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite85TableMatrixCoreIntegrity(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite86AxisScaleCorrectness(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite87Graph3DMatrixOrientation(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite88ExportAndFftSampling(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite89CurveDataMapping(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite90FftWindowTeardown(mw) != 0)
        return 1;
    mw->savedProject();
    if (runSuite91GraphCopyStackTransforms(mw) != 0)
        return 1;
    mw->savedProject();
    std::cout << "[PASS] All 53 ToriGraph OriginPro Parity, Phase 12 .opju Importer/Graph-Math/Modern-UI Gate, Campaign 6 Final End-to-End Invariants, Mega-Wave 1..6 Specialty Plots/MatSci/Autonomy/Undo/Perf, Phase 6/7/8/B Full-Width Ribbon & Dock Layout, Caption Visibility, Worksheet Quick Bar, Graph Inspector & Direct Canvas UX, 600-DPI Non-Cosmetic Render & Lossless PNG/PDF Export, Lab CSV/Shift-JIS/Rigaku .ras Importer, Tensile 0.2% Proof Stress & Non-Bold Arial Arrhenius, Nonlinear Linked Scales & Locale-Free Ticks, Dual EMF+ Translucent Pills, Python JSON-RPC 2.0 Automation, Japanese Zero-Miss Localization, and OLE EMF+ Anti-Aliased Vector suites passed!\n";

    // If --keep-clipboard-ms <ms> is passed, run the event loop for <ms> so external COM clients (e.g. PowerPoint) can paste
    int idx = args.indexOf("--keep-clipboard-ms");
    if (idx >= 0 && idx + 1 < args.size()) {
        int waitMs = args[idx + 1].toInt();
        if (waitMs > 0) {
            std::cout << "[INFO] Keeping OLE clipboard alive for " << waitMs << " ms...\n" << std::flush;
            QTimer::singleShot(waitMs, &app, &QCoreApplication::quit);
            app.exec();
        }
    }

    mw->setOleEmbeddedMode(true);
    delete mw;
    SciDAVisOle::shutdownOleServer();
    return 0;
}

