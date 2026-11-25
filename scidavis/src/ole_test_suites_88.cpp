#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "FFTDialog.h"

#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QLineEdit>
#include <QRegularExpression>
#include <QTemporaryDir>

// =============================================================================
// Suite 88: ASCII export and FFT sampling-interval regressions.
//   88a  exportASCII quotes a text cell that contains a non-blank character of the separator
//        (", " / ";\t" with "Al,Zn" / "Mg;Si"), not only the whole separator string
//   88b  "All tables" + "Selection only": a table without a selection is exported whole and
//        the export goes on to the remaining tables (non-interactive, temporary directory)
//   88c  signal.fft on a graph with non-uniform / descending X keeps FFT's own grid step
//        span/(n-1) instead of |x1 - x0|; an explicit 'sampling' still wins
//   88d  FFTDialog (table mode) takes the default interval from the X column of the real
//        column (colX(real)), not from the first X column of the table
// =============================================================================

namespace {

struct NonInteractiveGuard88
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard88()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard88()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard88
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard88(ApplicationWindow *w = nullptr) : win(w) {}
    ~AppWinGuard88() { reset(); }
    AppWinGuard88(const AppWinGuard88 &) = delete;
    AppWinGuard88 &operator=(const AppWinGuard88 &) = delete;
    void reset()
    {
        if (win) {
            win->savedProject();
            win->setOleEmbeddedMode(true);
            delete win;
            win = nullptr;
        }
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

// Non-empty lines of a UTF-8 text file (BOM removed).
QStringList readLines88(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QStringList();
    QString text = QString::fromUtf8(f.readAll());
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    return text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
}

} // namespace

int runSuite88ExportAndFftSampling(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 88: ASCII export quoting/all tables & FFT sampling...\n";

    NonInteractiveGuard88 envGuard88;
    AppWinGuard88 w88(new ApplicationWindow());
    w88->applyUserSettings();
    w88->confirmCloseTable = false;
    w88->confirmClosePlot2D = false;
    w88->confirmClosePlot3D = false;
    w88->confirmCloseFolder = false;
    w88->resize(1280, 800);
    w88->show();
    QCoreApplication::processEvents();

    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "88: temporary directory could not be created");

    // -------------------------------------------------------------------------
    // 88a: quoting with multi-character separators
    // -------------------------------------------------------------------------
    {
        Table *t = w88->newTable(2, 2, QStringLiteral("T88Quote"), QStringLiteral("Quote88"));
        CHECK_COND(t != nullptr, "88a: newTable failed");
        t->setCell(0, 0, 1.0);
        t->setCell(1, 0, 2.0);
        Column *txt = t->column(1);
        CHECK_COND(txt != nullptr, "88a: text column null");
        txt->setColumnMode(SciDAVis::ColumnMode::Text);
        txt->setTextAt(0, QStringLiteral("Al,Zn"));
        txt->setTextAt(1, QStringLiteral("Mg;Si"));

        struct Case
        {
            const char *sep;
            const char *tag;
            const char *line0;
            const char *line1;
        };
        const Case cases[] = {
            { ", ", "comma-space", "1, \"Al,Zn\"", "2, Mg;Si" },
            { "; ", "semicolon-space", "1; Al,Zn", "2; \"Mg;Si\"" },
            { ",\t", "comma-tab", "1,\t\"Al,Zn\"", "2,\tMg;Si" },
            { ";\t", "semicolon-tab", "1;\tAl,Zn", "2;\t\"Mg;Si\"" },
            { ",", "comma", "1,\"Al,Zn\"", "2,Mg;Si" },
            { "\t", "tab", "1\tAl,Zn", "2\tMg;Si" },
        };
        for (const Case &c : cases) {
            const QString path = tmp.filePath(QStringLiteral("quote88_%1.txt").arg(c.tag));
            CHECK_COND(t->exportASCII(path, QString::fromLatin1(c.sep), false, false),
                       "88a(" << c.tag << "): exportASCII failed: "
                              << t->lastExportError().toStdString());
            const QStringList lines = readLines88(path);
            CHECK_COND(lines.size() == 2, "88a(" << c.tag << "): expected 2 lines, got " << lines.size());
            CHECK_COND(lines[0] == QString::fromLatin1(c.line0),
                       "88a(" << c.tag << "): row 1 written as '" << lines[0].toStdString()
                              << "', expected '" << c.line0 << "'");
            CHECK_COND(lines[1] == QString::fromLatin1(c.line1),
                       "88a(" << c.tag << "): row 2 written as '" << lines[1].toStdString()
                              << "', expected '" << c.line1 << "'");
        }
        std::cout << "[PASS] 88a: separator characters inside text cells are quoted\n";
    }

    // -------------------------------------------------------------------------
    // 88b: all tables + selection only
    // -------------------------------------------------------------------------
    {
        AppWinGuard88 wa(new ApplicationWindow());
        wa->applyUserSettings();
        wa->confirmCloseTable = false;
        Table *tA = wa->newTable(3, 2, QStringLiteral("T88A"), QStringLiteral("A88"));
        Table *tB = wa->newTable(3, 2, QStringLiteral("T88B"), QStringLiteral("B88"));
        Table *tC = wa->newTable(3, 2, QStringLiteral("T88C"), QStringLiteral("C88"));
        CHECK_COND(tA && tB && tC, "88b: newTable failed");
        for (Table *t : { tA, tB, tC })
            for (int r = 0; r < 3; ++r) {
                t->setCell(r, 0, double(r + 1));
                t->setCell(r, 1, double(10 * (r + 1)));
            }
        // Only T88B has a selection: rows 1..2 of column 2.
        tB->setCellsSelected(1, 1, 2, 1, true);
        QCoreApplication::processEvents();
        CHECK_COND(tB->firstSelectedRow() == 1 && tB->lastSelectedRow() == 2
                           && tA->firstSelectedRow() < 0 && tC->firstSelectedRow() < 0,
                   "88b: selection fixture not as expected");

        QDir dir(tmp.filePath(QStringLiteral("all88")));
        CHECK_COND(dir.mkpath(QStringLiteral(".")), "88b: mkpath failed");
        QString failedName, error;
        const bool ok = wa->exportTablesToDirectory(dir.absolutePath(), QStringLiteral(","), false,
                                                    true, &failedName, &error);
        CHECK_COND(ok, "88b: export of all tables stopped at '" << failedName.toStdString()
                                                                << "': " << error.toStdString());
        const QStringList linesA = readLines88(dir.filePath(QStringLiteral("T88A.csv")));
        const QStringList linesB = readLines88(dir.filePath(QStringLiteral("T88B.csv")));
        const QStringList linesC = readLines88(dir.filePath(QStringLiteral("T88C.csv")));
        const QStringList wholeTable = { QStringLiteral("1,10"), QStringLiteral("2,20"),
                                         QStringLiteral("3,30") };
        CHECK_COND(linesA == wholeTable,
                   "88b: table without selection not exported whole: '"
                           << linesA.join(QStringLiteral("|")).toStdString() << "'");
        CHECK_COND(linesB == QStringList({ QStringLiteral("20"), QStringLiteral("30") }),
                   "88b: table with selection did not export just its selection: '"
                           << linesB.join(QStringLiteral("|")).toStdString() << "'");
        CHECK_COND(linesC == wholeTable,
                   "88b: table after the selected one not exported (or not whole): '"
                           << linesC.join(QStringLiteral("|")).toStdString() << "'");
        std::cout << "[PASS] 88b: all tables + selection exports every table\n";
    }

    // -------------------------------------------------------------------------
    // 88c: signal.fft on a graph with non-uniform X
    // -------------------------------------------------------------------------
    {
        // Own window: its FFT result plots are torn down at the end of this block.
        AppWinGuard88 wc(new ApplicationWindow());
        wc->applyUserSettings();
        wc->confirmCloseTable = false;
        wc->confirmClosePlot2D = false;
        wc->confirmClosePlot3D = false;
        wc->confirmCloseFolder = false;
        wc->resize(1280, 800);
        wc->show();
        QCoreApplication::processEvents();
        PythonAutomationServer *srv = wc->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "88c: pythonAutomationServer null");
        int rpcId = 8800;
        auto rpc = [&](const QString &method, const QJsonObject &params) -> QJsonObject {
            QJsonObject req;
            req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            req[QStringLiteral("id")] = ++rpcId;
            req[QStringLiteral("method")] = method;
            req[QStringLiteral("params")] = params;
            const QJsonObject resp = srv->handleRpcRequest(req);
            QCoreApplication::processEvents();
            return resp;
        };
        // Frequency step of the FFT result table returned by `resp`.
        auto freqStep = [&](const QJsonObject &resp, const char *tag, double &dfOut) -> bool {
            if (resp.contains(QStringLiteral("error"))) {
                std::cerr << "[FAIL] 88c(" << tag << "): signal.fft error: "
                          << QJsonDocument(resp.value(QStringLiteral("error")).toObject())
                                     .toJson(QJsonDocument::Compact)
                                     .toStdString()
                          << std::endl;
                return false;
            }
            const QString name =
                    resp.value(QStringLiteral("result")).toObject().value(QStringLiteral("table")).toString();
            Table *ft = wc->table(name);
            if (!ft || ft->numRows() < 2) {
                std::cerr << "[FAIL] 88c(" << tag << "): FFT result table '" << name.toStdString()
                          << "' missing" << std::endl;
                return false;
            }
            dfOut = ft->cell(1, 0) - ft->cell(0, 0);
            return true;
        };

        // 8 points, X span 1.4 -> FFT grid step 1.4/7 = 0.2, df = 1/(8*0.2) = 0.625.
        // The old |x1 - x0| = 0.1 gave df = 1.25.
        const double xsAsc[8] = { 0.0, 0.1, 0.3, 0.35, 0.6, 0.8, 1.0, 1.4 };
        Table *tF = wc->newTable(8, 2, QStringLiteral("T88Fft"), QStringLiteral("Fft88"));
        CHECK_COND(tF != nullptr, "88c: newTable failed");
        for (int r = 0; r < 8; ++r) {
            tF->setCell(r, 0, xsAsc[r]);
            tF->setCell(r, 1, std::sin(4.0 * xsAsc[r]));
        }
        MultiLayer *ml = wc->multilayerPlot(tF, QStringList() << tF->colName(1), Graph::Line);
        CHECK_COND(ml != nullptr && ml->activeGraph() && ml->activeGraph()->curves() == 1,
                   "88c: plot fixture failed");
        QCoreApplication::processEvents();

        double df = 0.0;
        if (!freqStep(rpc(QStringLiteral("signal.fft"), QJsonObject{ { "graph", ml->name() } }),
                      "non-uniform", df))
            return 1;
        CHECK_COND(std::fabs(df - 0.625) < 1e-9,
                   "88c: frequency step " << df << " on non-uniform X, expected 0.625 (FFT grid "
                                          << "span/(n-1)); 1.25 means |x1-x0| overrode it");

        double dfExplicit = 0.0;
        if (!freqStep(rpc(QStringLiteral("signal.fft"),
                          QJsonObject{ { "graph", ml->name() }, { "sampling", 0.5 } }),
                      "explicit", dfExplicit))
            return 1;
        CHECK_COND(std::fabs(dfExplicit - 0.25) < 1e-9,
                   "88c: explicit sampling 0.5 gave frequency step " << dfExplicit
                                                                       << ", expected 0.25");

        // Same samples with X in descending row order.
        Table *tD = wc->newTable(8, 2, QStringLiteral("T88FftDesc"), QStringLiteral("FftDesc88"));
        CHECK_COND(tD != nullptr, "88c: newTable (descending) failed");
        for (int r = 0; r < 8; ++r) {
            tD->setCell(r, 0, xsAsc[7 - r]);
            tD->setCell(r, 1, std::sin(4.0 * xsAsc[7 - r]));
        }
        MultiLayer *mlD = wc->multilayerPlot(tD, QStringList() << tD->colName(1), Graph::Line);
        CHECK_COND(mlD != nullptr && mlD->activeGraph() && mlD->activeGraph()->curves() == 1,
                   "88c: descending plot fixture failed");
        QCoreApplication::processEvents();
        double dfDesc = 0.0;
        if (!freqStep(rpc(QStringLiteral("signal.fft"), QJsonObject{ { "graph", mlD->name() } }),
                      "descending", dfDesc))
            return 1;
        CHECK_COND(std::fabs(dfDesc - 0.625) < 1e-9,
                   "88c: frequency step " << dfDesc << " on descending non-uniform X, expected 0.625");
        std::cout << "[PASS] 88c: signal.fft keeps FFT's own grid step (df=" << df << ")\n";
    }

    // -------------------------------------------------------------------------
    // 88d: FFTDialog default interval from colX(real)
    // -------------------------------------------------------------------------
    {
        // X1 (step 1) | Y1 | X2 (step 0.25) | Y2
        Table *t = w88->newTable(8, 4, QStringLiteral("T88Dlg"), QStringLiteral("Dlg88"));
        CHECK_COND(t != nullptr, "88d: newTable failed");
        for (int r = 0; r < 8; ++r) {
            t->setCell(r, 0, double(r));
            t->setCell(r, 1, std::cos(double(r)));
            t->setCell(r, 2, 0.25 * r);
            t->setCell(r, 3, std::sin(double(r)));
        }
        t->setColPlotDesignation(0, SciDAVis::X);
        t->setColPlotDesignation(1, SciDAVis::Y);
        t->setColPlotDesignation(2, SciDAVis::X);
        t->setColPlotDesignation(3, SciDAVis::Y);
        CHECK_COND(t->firstXCol() == 0 && t->colX(3) == 2, "88d: X column fixture not as expected");
        t->setCellsSelected(0, 3, 7, 3, true); // real part = Y2
        QCoreApplication::processEvents();

        FFTDialog *dlg = new FFTDialog(FFTDialog::onTable, w88);
        dlg->setAttribute(Qt::WA_DeleteOnClose, false);
        dlg->setTable(t);
        const QString realText = dlg->boxReal->currentText();
        const QString sampling = dlg->boxSampling->text();
        const int samplingCol = dlg->boxName->currentIndex();
        // Choosing Y1 as the real part switches back to X1.
        dlg->boxReal->setCurrentIndex(1);
        dlg->activateRealColumn(1);
        const QString samplingY1 = dlg->boxSampling->text();
        const int samplingColY1 = dlg->boxName->currentIndex();
        delete dlg;

        CHECK_COND(t->colIndex(realText) == 3,
                   "88d: real column is '" << realText.toStdString() << "', expected Y2");
        CHECK_COND(samplingCol == 2 && std::fabs(sampling.toDouble() - 0.25) < 1e-12,
                   "88d: default sampling '" << sampling.toStdString() << "' from column "
                                             << samplingCol
                                             << ", expected 0.25 from X2 (index 2)");
        CHECK_COND(samplingColY1 == 0 && std::fabs(samplingY1.toDouble() - 1.0) < 1e-12,
                   "88d: after choosing Y1 sampling '" << samplingY1.toStdString()
                                                       << "' from column " << samplingColY1
                                                       << ", expected 1 from X1 (index 0)");
        std::cout << "[PASS] 88d: FFTDialog default interval follows colX(real)\n";
    }

    std::cout << "[PASS] Suite 88 passed.\n";
    return 0;
}
