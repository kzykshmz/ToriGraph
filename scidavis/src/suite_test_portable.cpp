// Portable (non-Windows) runner for the modular regression suites that do not touch
// OLE / EMF / the Win32 clipboard: Suites 64-72, 79 and 81-120.
//
// The Windows runner (ole_test.cpp -> scidavis_ole_test.exe) stays the reference; this
// runner lets the same suites run on Linux/macOS, typically under GCC/Clang
// AddressSanitizer + UndefinedBehaviorSanitizer (scripts/dev.sh asan).
//
//   scidavis_suite_test --list-suites
//   scidavis_suite_test --suite=81,82          (default: every suite listed below)

#include "ole_test_common.h"

#include <QTemporaryDir>

#include <cstdlib>

namespace {

struct PortableSuiteEntry
{
    int id;
    const char *name;
    int (*run)(ApplicationWindow *);
};

const PortableSuiteEntry kPortableSuites[] = {
    { 64, "runSuite64XlsxImport", &runSuite64XlsxImport },
    { 65, "runSuite65E11ExtraGraphTypes", &runSuite65E11ExtraGraphTypes },
    { 66, "runSuite66Workbooks", &runSuite66Workbooks },
    { 67, "runSuite67Density2DAndPoleFigure", &runSuite67Density2DAndPoleFigure },
    { 68, "runSuite68RibbonFoldingRecentModelsAndMiniToolbarKeys", &runSuite68RibbonFoldingRecentModelsAndMiniToolbarKeys },
    { 69, "runSuite69SettingsTransferAndScriptCoverage", &runSuite69SettingsTransferAndScriptCoverage },
    { 70, "runSuite70IconCleanupAndLightTheme", &runSuite70IconCleanupAndLightTheme },
    { 71, "runSuite71LabSlideTextAndZoomFollowsText", &runSuite71LabSlideTextAndZoomFollowsText },
    { 72, "runSuite72ToriGraphStandardDefaultPalette", &runSuite72ToriGraphStandardDefaultPalette },
    { 79, "runSuite79ZeroBaseOverhaulV2Gate", &runSuite79ZeroBaseOverhaulV2Gate },
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
    { 92, "runSuite92RenamePropagation", &runSuite92RenamePropagation },
    { 93, "runSuite93WorksheetSortAndBlankRows", &runSuite93WorksheetSortAndBlankRows },
    { 94, "runSuite94RpcWorksheetDataFidelity", &runSuite94RpcWorksheetDataFidelity },
    { 95, "runSuite95FitDataDriven", &runSuite95FitDataDriven },
    { 96, "runSuite96XrdPeaksAndUnsortedDiff", &runSuite96XrdPeaksAndUnsortedDiff },
    { 97, "runSuite97PlotDataFidelity", &runSuite97PlotDataFidelity },
    { 98, "runSuite98RpcImportRobustness", &runSuite98RpcImportRobustness },
    { 99, "runSuite99DateTimeParsingAndWideTables", &runSuite99DateTimeParsingAndWideTables },
    { 100, "runSuite100NumericsRound2", &runSuite100NumericsRound2 },
    { 101, "runSuite101WorksheetRound2", &runSuite101WorksheetRound2 },
    { 102, "runSuite102PlotRound2", &runSuite102PlotRound2 },
    { 103, "runSuite103ImportRound2", &runSuite103ImportRound2 },
    { 104, "runSuite104ImportRound3", &runSuite104ImportRound3 },
    { 105, "runSuite105RpcImportColumnTypes", &runSuite105RpcImportColumnTypes },
    { 106, "runSuite106NumericsRound3", &runSuite106NumericsRound3 },
    { 107, "runSuite107PlotFixesRound3", &runSuite107PlotFixesRound3 },
    { 108, "runSuite108PlotGeometryCloneRoundTrip", &runSuite108PlotGeometryCloneRoundTrip },
    { 109, "runSuite109WorksheetFormulaRefsAndMasks", &runSuite109WorksheetFormulaRefsAndMasks },
    { 110, "runSuite110CurveMathKeepsGraphAndColumns", &runSuite110CurveMathKeepsGraphAndColumns },
    { 111, "runSuite111DualYHeadroomAndRemovedColumnUndo", &runSuite111DualYHeadroomAndRemovedColumnUndo },
    { 112, "runSuite112HeatmapHolesAndHistogramBins", &runSuite112HeatmapHolesAndHistogramBins },
    { 113, "runSuite113SwarmAuditRegressions", &runSuite113SwarmAuditRegressions },
    { 114, "runSuite114DeepSwarmAuditRegressions", &runSuite114DeepSwarmAuditRegressions },
    { 115, "runSuite115Campaign2SwarmAuditRegressions", &runSuite115Campaign2SwarmAuditRegressions },
    { 116, "runSuite116ImportAndProjectLoadLimits", &runSuite116ImportAndProjectLoadLimits },
    { 117, "runSuite117ScaleInvariantStatistics", &runSuite117ScaleInvariantStatistics },
    { 118, "runSuite118RpcParamAndSdkHardening", &runSuite118RpcParamAndSdkHardening },
    { 119, "runSuite119CurveColumnBookkeeping", &runSuite119CurveColumnBookkeeping },
    { 120, "runSuite120FuzzCampaignRegressions", &runSuite120FuzzCampaignRegressions },
};

const PortableSuiteEntry *findSuite(int id)
{
    for (const auto &e : kPortableSuites)
        if (e.id == id)
            return &e;
    return nullptr;
}

} // namespace

int main(int argc, char **argv)
{
    std::cout << std::unitbuf << "[INFO] Starting scidavis_suite_test (portable runner)...\n";
    std::cerr << std::unitbuf;

    // Private scratch folder (TMPDIR) and settings file for this run, like the Windows runner.
    QTemporaryDir runTempDir(QDir::tempPath() + QStringLiteral("/torigraph_suite_test_XXXXXX"));
    if (runTempDir.isValid())
        qputenv("TMPDIR", runTempDir.path().toLocal8Bit());
    const QString iniPath = (runTempDir.isValid() ? runTempDir.path() : QDir::tempPath())
            + QStringLiteral("/torigraph_suite_test_settings.ini");
    qputenv("TORIGRAPH_SETTINGS_INI", iniPath.toLocal8Bit());
    qputenv("TORIGRAPH_NO_OLE_REGISTER", "1");
    if (qEnvironmentVariableIsEmpty("QT_FONT_DPI"))
        qputenv("QT_FONT_DPI", "96");
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, true);

    QApplication app(argc, argv);
    const QStringList args = app.arguments();

    if (args.contains(QStringLiteral("--list-suites"))) {
        for (const auto &e : kPortableSuites)
            std::cout << "Suite " << e.id << ": " << e.name << "\n";
        return 0;
    }

    QList<int> requested;
    for (int i = 1; i < args.size(); ++i) {
        QString spec;
        if (args[i].startsWith(QStringLiteral("--suite=")))
            spec = args[i].mid(8);
        else if (args[i] == QStringLiteral("--suite") && i + 1 < args.size())
            spec = args[++i];
        else
            continue;
        for (const QString &part : spec.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            bool ok = false;
            const int sid = part.trimmed().toInt(&ok);
            if (!ok || !findSuite(sid)) {
                std::cerr << "[FAIL] Unknown or non-portable suite id: " << part.toStdString()
                          << " (use --list-suites)\n";
                return 2;
            }
            requested.append(sid);
        }
    }
    if (requested.isEmpty())
        for (const auto &e : kPortableSuites)
            requested.append(e.id);

    ApplicationWindow::setRecoveryDirPathOverride(
            (runTempDir.isValid() ? runTempDir.path() : QDir::tempPath())
            + QStringLiteral("/recovery"));
    ModalWatchdog modalWatchdog;

    ApplicationWindow *mw = new ApplicationWindow();
    mw->applyUserSettings();
    MultiLayer::setNewGraphStylePresetId(QString());
    mw->saveSettings();

    int failed = 0;
    QStringList failedIds;
    for (int sid : requested) {
        const PortableSuiteEntry *e = findSuite(sid);
        std::cout << "[RUN ] Suite " << sid << " (" << e->name << ")\n";
        QElapsedTimer timer;
        timer.start();
        const int rc = e->run(mw);
        mw->savedProject();
        std::cout << (rc == 0 ? "[ OK ] Suite " : "[FAIL] Suite ") << sid << " in "
                  << timer.elapsed() << " ms\n";
        if (rc != 0) {
            ++failed;
            failedIds << QString::number(sid);
        }
    }

    mw->savedProject();
    delete mw;

    if (g_modalWatchdogDismissals > 0) {
        std::cerr << "[FAIL] the modal watchdog had to close " << g_modalWatchdogDismissals
                  << " dialog(s) that nobody answered\n";
        failed = failed ? failed : 1;
    }
    std::cout << "[INFO] " << (requested.size() - failedIds.size()) << "/" << requested.size()
              << " suites passed" << (failedIds.isEmpty() ? "" : "; failed: ")
              << failedIds.join(QLatin1Char(',')).toStdString() << "\n";
    return failed ? 1 : 0;
}
