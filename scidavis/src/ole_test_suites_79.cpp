#include "ole_test_common.h"
#include "future/table/TableModel.h"

#include <QStackedWidget>
#include <QTableView>
#include <QToolButton>
#include <QToolTip>

// =========================================================================================
// Suite 79: Zero-base UX & Quality Overhaul (Wave 2A/2B) Regression Gate
// =========================================================================================
int runSuite79ZeroBaseOverhaulV2Gate(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 79: Zero-Base Overhaul V2 Regression Gate...\n";

    struct EnvGuard79
    {
        QVariant prevNonInteractive;
        EnvGuard79() : prevNonInteractive(qApp->property("torigraph_noninteractive"))
        {
            qApp->setProperty("torigraph_noninteractive", true);
        }
        ~EnvGuard79() { qApp->setProperty("torigraph_noninteractive", prevNonInteractive); }
    } envGuard79;

    struct AppWinGuard79
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard79(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard79()
        {
            reset();
            QApplication::processEvents();
        }
        AppWinGuard79(const AppWinGuard79 &) = delete;
        AppWinGuard79 &operator=(const AppWinGuard79 &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
        }
        ApplicationWindow *get() const { return win; }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    QTemporaryDir tmp79;
    CHECK_COND(tmp79.isValid(), "79: temp dir creation failed");

    AppWinGuard79 w79(new ApplicationWindow());
    w79->applyUserSettings();
    w79->confirmCloseTable = false;
    w79->confirmClosePlot2D = false;
    w79->confirmClosePlot3D = false;
    w79->confirmCloseFolder = false;
    w79->resize(1440, 860);
    w79->show();
    w79->switchToLanguage("en");
    QCoreApplication::processEvents();

    // -------------------------------------------------------------------------
    // 79a: Masked sort + Column::copy mask preservation + '=' numeric cell + stats signal
    // -------------------------------------------------------------------------
    {
        Table *tSort = w79->newTable(5, 2, "T79SortMask", "SortMask");
        CHECK_COND(tSort != nullptr && tSort->d_future_table != nullptr, "79a: T79SortMask failed");
        const double xs[5] = { 3.0, 1.0, 2.0, 5.0, 4.0 };
        const double ys[5] = { 30.0, 10.0, 20.0, 50.0, 40.0 };
        for (int r = 0; r < 5; ++r) {
            tSort->setCell(r, 0, xs[r]);
            tSort->setCell(r, 1, ys[r]);
        }
        tSort->column(0)->setMasked(0, true); // x=3, y=30
        tSort->column(0)->setMasked(3, true); // x=5, y=50
        tSort->column(1)->setMasked(0, true);
        tSort->column(1)->setMasked(3, true);

        tSort->d_future_table->sortColumns(
                tSort->column(0), QList<Column *>() << tSort->column(0) << tSort->column(1), true);
        QCoreApplication::processEvents();

        for (int r = 0; r < 5; ++r) {
            CHECK_COND(std::fabs(tSort->cell(r, 0) - double(r + 1)) < 1e-9,
                       "79a: sorted X mismatch at row " << r);
            CHECK_COND(std::fabs(tSort->cell(r, 1) - 10.0 * double(r + 1)) < 1e-9,
                       "79a: sorted Y mismatch at row " << r);
        }
        // Masked rows moved from (0,3) -> (2 [x=3], 4 [x=5]); rows 0,1,3 must be unmasked.
        CHECK_COND(!tSort->column(0)->isMasked(0) && !tSort->column(0)->isMasked(1)
                           && tSort->column(0)->isMasked(2) && !tSort->column(0)->isMasked(3)
                           && tSort->column(0)->isMasked(4),
                   "79a: X mask intervals corrupted by sortColumns (bool-as-index bug)!");
        CHECK_COND(!tSort->column(1)->isMasked(0) && !tSort->column(1)->isMasked(1)
                           && tSort->column(1)->isMasked(2) && !tSort->column(1)->isMasked(3)
                           && tSort->column(1)->isMasked(4),
                   "79a: Y mask intervals corrupted by sortColumns!");

        Column copiedY(QStringLiteral("CopiedY"), SciDAVis::ColumnMode::Numeric);
        CHECK_COND(copiedY.copy(tSort->column(1)), "79a: Column::copy failed");
        CHECK_COND(!copiedY.isMasked(0) && copiedY.isMasked(2) && !copiedY.isMasked(3)
                           && copiedY.isMasked(4),
                   "79a: Column::copy failed to preserve mask intervals!");

        // Entering '=' formula in an empty Numeric cell keeps Numeric mode
        Table *tEq = w79->newTable(4, 2, "T79EqCell", "EqCell");
        CHECK_COND(tEq != nullptr && tEq->column(1) != nullptr, "79a: T79EqCell failed");
        tEq->setCell(0, 0, 7.0);
        tEq->setCell(1, 0, 11.0);
        QTableView *tvEq = tEq->findChild<QTableView *>();
        CHECK_COND(tvEq != nullptr && tvEq->model() != nullptr, "79a: QTableView/model missing");
        CHECK_COND(tvEq->model()->setData(tvEq->model()->index(0, 1), QStringLiteral("=col(\"1\")*2"),
                                          Qt::EditRole),
                   "79a: TableModel::setData('=col(\"1\")*2') returned false");
        QCoreApplication::processEvents();
        CHECK_COND(tEq->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric,
                   "79a: '=' entry in empty Numeric cell switched ColumnMode away from Numeric!");
        CHECK_COND(std::fabs(tEq->cell(0, 1) - 14.0) < 1e-6,
                   "79a: '=' formula in empty Numeric cell did not evaluate to 14.0 (got "
                           << tEq->cell(0, 1) << ")");

        // selectionStatsChanged signal fires on selection change
        QString lastStats;
        QObject::connect(tSort, &TableView::selectionStatsChanged,
                         [&lastStats](const QString &s) { lastStats = s; });
        tSort->setCellsSelected(0, 0, 2, 1, true);
        QCoreApplication::processEvents();
        CHECK_COND(!lastStats.isEmpty(),
                   "79a: TableView::selectionStatsChanged did not emit stats summary!");

        tSort->close();
        tEq->close();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // -------------------------------------------------------------------------
    // 79b: deleteFolder() UAF & Undo safety, CurvesDialog d_graphGuard & dict check
    // -------------------------------------------------------------------------
    {
        QUndoStack *projUndo = w79->project() ? w79->project()->undoStack() : nullptr;
        CHECK_COND(projUndo != nullptr, "79b: project undoStack null");

        w79->addFolder();
        QList<Folder *> subFolders = w79->projectFolder()->folders();
        CHECK_COND(!subFolders.isEmpty(), "79b: addFolder failed");
        Folder *fSub = subFolders.last();
        w79->changeFolder(fSub, true);

        Folder &childF = fSub->addChild<Folder>(QStringLiteral("Nested79"));
        w79->addFolderListViewItem(&childF);
        FolderListItem *childFi = new FolderListItem(fSub->folderListItem(), &childF);
        childF.setFolderListItem(childFi);

        MultiLayer *mlInFolder = w79->newGraph("G79InFolder");
        CHECK_COND(mlInFolder != nullptr && mlInFolder->activeGraph() != nullptr,
                   "79b: G79InFolder failed");
        QPointer<MultiLayer> pMlInFolder(mlInFolder);
        QPointer<Folder> pSub(fSub);
        QPointer<Folder> pChild(&childF);

        CHECK_COND(w79->deleteFolder(fSub), "79b: deleteFolder(fSub) failed");
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        CHECK_COND(!pSub && !pChild && !pMlInFolder,
                   "79b: deleted folder subtree or child window survived!");
        CHECK_COND(!projUndo->canUndo(),
                   "79b: stale project Undo entries survived deleteFolder()!");

        // Modeless CurvesDialog survives parent MultiLayer closure via d_graphGuard
        MultiLayer *mlDlg = w79->newGraph("G79CurvesGuard");
        Table *tMissX = w79->newTable(4, 2, "T79NoX", "NoX");
        tMissX->setColPlotDesignation(0, SciDAVis::Y);
        tMissX->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < 4; ++r) {
            tMissX->setCell(r, 0, double(r));
            tMissX->setCell(r, 1, double(r * r));
        }

        w79->switchToLanguage("ja");
        QCoreApplication::processEvents();
        const QString appLangBefore = QCoreApplication::translate("ApplicationWindow", "Line + Symbol");
        {
            CurvesDialog cd(w79);
            cd.setGraph(mlDlg->activeGraph());
            cd.show();
            QCoreApplication::processEvents();

            const QString appLangAfter =
                    QCoreApplication::translate("ApplicationWindow", "Line + Symbol");
            CHECK_COND(appLangAfter == appLangBefore,
                       "79b: constructing CurvesDialog mutated ApplicationWindow translation dict!");

            mlDlg->askOnCloseEvent(false);
            mlDlg->close();
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();

            // Must not crash when invoked after target graph is deleted
            QMetaObject::invokeMethod(&cd, "addCurves", Qt::DirectConnection);
            QCoreApplication::processEvents();
        }
        w79->switchToLanguage("en");
        QCoreApplication::processEvents();

        // Missing-X warning fires cleanly when interactive & Y-only table is selected
        MultiLayer *mlWarn = w79->newGraph("G79MissX");
        CurvesDialog cdWarn(w79);
        cdWarn.setGraph(mlWarn->activeGraph());
        QListWidget *availList = cdWarn.findChildren<QListWidget *>().value(0, nullptr);
        CHECK_COND(availList != nullptr && availList->count() > 0, "79b: CurvesDialog available list empty");
        for (int i = 0; i < availList->count(); ++i) {
            if (availList->item(i)->data(Qt::UserRole).toString().contains("T79NoX")) {
                availList->item(i)->setSelected(true);
                break;
            }
        }
        qApp->setProperty("torigraph_noninteractive", false);
        bool sawMissingXDialog = false;
        QString warnText;
        QTimer::singleShot(40, [&]() {
            if (QWidget *mwModal = QApplication::activeModalWidget()) {
                sawMissingXDialog = true;
                if (QMessageBox *mb = qobject_cast<QMessageBox *>(mwModal))
                    warnText = mb->text();
                mwModal->close();
            }
        });
        QMetaObject::invokeMethod(&cdWarn, "addCurves", Qt::DirectConnection);
        qApp->setProperty("torigraph_noninteractive", true);
        CHECK_COND(sawMissingXDialog && warnText.contains(QStringLiteral("X")),
                   "79b: missing-X warning dialog did not appear (saw=" << sawMissingXDialog
                                                                       << " text='" << warnText.toStdString()
                                                                       << "')");

        tMissX->close();
        mlWarn->close();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // -------------------------------------------------------------------------
    // 79c: Mid-file #A1, European ';' + decimal comma, >=90% tolerance, 3-row Origin,
    //      failed import reporting, reimportFromSource formula preservation
    // -------------------------------------------------------------------------
    {
        // Mid-file #A1 channel block marker
        {
            const QByteArray midA1("freq,mag\n1,10\n2,20\n#A1\n3,30\n");
            AsciiTableImportFilter::ImportReport rep;
            std::unique_ptr<future::Table> tbl(
                    AsciiTableImportFilter::importLabTextData(midA1, "MidA1_79", "mida1.csv"));
            rep = AsciiTableImportFilter::lastImportReport();
            CHECK_COND(tbl && tbl->columnCount() == 2 && tbl->rowCount() == 3,
                       "79c: mid-file #A1 failed (rows=" << (tbl ? tbl->rowCount() : -1) << ")");
            CHECK_COND(std::fabs(tbl->column(1)->valueAt(2) - 30.0) < 1e-9,
                       "79c: mid-file #A1 row 2 value wrong");
        }

        // European semicolon + decimal comma
        {
            const QByteArray euro("Zeit;Spannung\n0,5;12,34\n1,5;23,45\n2,5;34,56\n");
            std::unique_ptr<future::Table> tbl(
                    AsciiTableImportFilter::importLabTextData(euro, "Euro_79", "euro.csv"));
            const AsciiTableImportFilter::ImportReport rep = AsciiTableImportFilter::lastImportReport();
            CHECK_COND(tbl && tbl->columnCount() == 2 && tbl->rowCount() == 3,
                       "79c: European ';' CSV rowCount mismatch");
            CHECK_COND(rep.separator == QStringLiteral(";"),
                       "79c: expected ';' separator, got '" << rep.separator.toStdString() << "'");
            CHECK_COND(tbl->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && tbl->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric,
                       "79c: European CSV columns not Numeric");
            CHECK_COND(std::fabs(tbl->column(1)->valueAt(0) - 12.34) < 1e-6,
                       "79c: European decimal-comma value mismatch (got "
                               << tbl->column(1)->valueAt(0) << ")");
        }

        // >=90% numeric tolerance (11 numeric + 1 non-numeric text token)
        {
            QByteArray tol("X,Y\n");
            for (int i = 0; i < 11; ++i)
                tol += QByteArray::number(i) + "," + QByteArray::number(i * 2.0) + "\n";
            tol += "11,SensorErr\n";
            std::unique_ptr<future::Table> tbl(
                    AsciiTableImportFilter::importLabTextData(tol, "Tol90_79", "tol90.csv"));
            const AsciiTableImportFilter::ImportReport rep = AsciiTableImportFilter::lastImportReport();
            CHECK_COND(tbl && tbl->columnCount() == 2 && tbl->rowCount() == 12,
                       "79c: >=90% tolerance table size mismatch");
            CHECK_COND(tbl->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric,
                       "79c: >=90% numeric column downgraded to Text!");
            CHECK_COND(tbl->column(1)->isInvalid(11) && rep.nonNumericCells >= 1,
                       "79c: stray non-numeric token not marked invalid/counted");
        }

        // 3-row Origin header [Long Name / Units / Comments]
        {
            const QByteArray orig("Temperature\tStress\nK\tMPa\nRun 1\tPeak load\n300\t120\n400\t150\n");
            std::unique_ptr<future::Table> tbl(
                    AsciiTableImportFilter::importLabTextData(orig, "Orig3_79", "orig3.txt"));
            CHECK_COND(tbl && tbl->columnCount() == 2 && tbl->rowCount() == 2,
                       "79c: 3-row Origin header data rowCount mismatch");
            CHECK_COND(tbl->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && std::fabs(tbl->column(1)->valueAt(1) - 150.0) < 1e-9,
                       "79c: 3-row Origin header leaked metadata into numeric data");
            CHECK_COND(tbl->column(1)->comment().contains(QStringLiteral("Peak load"))
                               && tbl->column(1)->units().contains(QStringLiteral("MPa"))
                               && tbl->column(1)->longName().contains(QStringLiteral("Stress")),
                       "79c: 3-row Origin metadata/units lost");
        }

        // Failed import reporting & reimportFromSource preserving derived formula column
        {
            const QString missingCsv = tmp79.filePath(QStringLiteral("missing_79.csv"));
            CHECK_COND(w79->newTable(missingCsv, QStringLiteral(","), 0, true, true, false, true,
                                     QLocale::c())
                               == nullptr,
                       "79c: newTable on missing file did not return nullptr!");
            Table *tFail = w79->newTable(2, 2, "T79Fail", "Fail");
            tFail->importASCII(missingCsv, QStringLiteral(","), 0, true, true, false, false);
            CHECK_COND(!tFail->importSucceeded() && !tFail->lastImportError().isEmpty(),
                       "79c: Table::importASCII failed import not recorded on importSucceeded()/lastImportError()!");
            tFail->close();

            const QString csvPath = tmp79.filePath(QStringLiteral("reimp_derived_79.csv"));
            {
                QFile f(csvPath);
                CHECK_COND(f.open(QIODevice::WriteOnly), "79c: write initial csv");
                f.write("X,Y\n1,5\n2,6\n");
            }
            Table *tDeriv = w79->newTable(csvPath, QStringLiteral(","), 0, true, true, false, true,
                                          QLocale::c());
            CHECK_COND(tDeriv != nullptr && tDeriv->numCols() == 2, "79c: initial newTable failed");
            tDeriv->addCol();
            tDeriv->setColName(2, QStringLiteral("Derived"));
            tDeriv->setCommand(2, QStringLiteral("col(\"Y\")*10"));
            tDeriv->recalculate(2, false);
            CHECK_COND(std::fabs(tDeriv->cell(0, 2) - 50.0) < 1e-6, "79c: derived col init failed");

            {
                QFile f(csvPath);
                CHECK_COND(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "79c: overwrite csv");
                f.write("X,Y\n1,7\n2,8\n3,9\n");
            }
            CHECK_COND(tDeriv->reimportFromSource(), "79c: reimportFromSource failed");
            QCoreApplication::processEvents();
            CHECK_COND(tDeriv->numCols() >= 3 && tDeriv->numRows() == 3,
                       "79c: reimportFromSource dropped derived column (cols=" << tDeriv->numCols()
                                                                               << ")");
            CHECK_COND(std::fabs(tDeriv->cell(2, 2) - 90.0) < 1e-6,
                       "79c: derived formula column not recalculated after reimportFromSource (got "
                               << tDeriv->cell(2, 2) << ")");
            tDeriv->close();
        }
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // -------------------------------------------------------------------------
    // 79d: journal_2col_175mm non-square aspect ratio, mixed-preset PptxExport + early fail,
    //      2-pass Graph::copy with ErrorBars, CurveLayout init
    // -------------------------------------------------------------------------
    {
        const CurveLayout clDef = Graph::initCurveLayout();
        CHECK_COND(clDef.lWidth > 0 && clDef.sSize >= 0 && clDef.filledArea >= 0 && clDef.connectType >= 0,
                   "79d: Graph::initCurveLayout left uninitialized fields!");

        Table *tPlot = w79->newTable(5, 3, "T79Plot", "Plot");
        tPlot->setColPlotDesignation(0, SciDAVis::X);
        tPlot->setColPlotDesignation(1, SciDAVis::Y);
        tPlot->setColPlotDesignation(2, SciDAVis::yErr);
        for (int r = 0; r < 5; ++r) {
            tPlot->setCell(r, 0, double(r));
            tPlot->setCell(r, 1, double(r * 2 + 1));
            tPlot->setCell(r, 2, 0.25);
        }

        MultiLayer *ml2Col = w79->multilayerPlot(tPlot, QStringList() << tPlot->colName(1), Graph::Line);
        MultiLayer *mlLab = w79->multilayerPlot(tPlot, QStringList() << tPlot->colName(1), Graph::Line);
        CHECK_COND(ml2Col && mlLab, "79d: multilayerPlot failed");
        CHECK_COND(ml2Col->applyStylePresetById(QStringLiteral("journal_2col_175mm")),
                   "79d: apply journal_2col_175mm failed");
        CHECK_COND(mlLab->applyStylePresetById(QStringLiteral("lab_slide_83mm")),
                   "79d: apply lab_slide_83mm failed");
        QCoreApplication::processEvents();

        const double ratio2Col = ml2Col->width() > 0 && ml2Col->height() > 0
                ? double(ml2Col->activeGraph()->width()) / double(ml2Col->activeGraph()->height())
                : 0.0;
        CHECK_COND(ml2Col->activeGraph()->width() > 1.5 * ml2Col->activeGraph()->height(),
                   "79d: journal_2col_175mm canvas was squashed instead of keeping ~2:1 landscape aspect (ratio="
                           << ratio2Col << ")!");

        ToriGraphPptx::DeckOptions opt;
        opt.perSlide = 2;
        opt.perGraphPresetScale = true;
        const QString pptxOk = tmp79.filePath(QStringLiteral("mixed_79.pptx"));
        const ToriGraphPptx::DeckResult resOk =
                ToriGraphPptx::exportGraphs(w79, QList<MyWidget *>() << mlLab << ml2Col, opt, pptxOk);
        CHECK_COND(resOk.ok && QFileInfo::exists(pptxOk) && resOk.placements.size() == 2,
                   "79d: mixed-preset PptxExport failed: " << resOk.error.toStdString());
        CHECK_COND(resOk.placements.at(0).requestedPercent >= 135.0
                           && resOk.placements.at(1).requestedPercent <= 105.0,
                   "79d: mixed-preset perGraphPresetScale did not distinguish 140% lab_slide vs 100% 2col (got "
                           << resOk.placements.at(0).requestedPercent << ", "
                           << resOk.placements.at(1).requestedPercent << ")");
        CHECK_COND(resOk.placements.at(1).width > 1.5 * resOk.placements.at(1).height,
                   "79d: 2col placement shape width/height not landscape");

        const QString pptxBad = pptxOk + QStringLiteral("/fail.pptx");
        const ToriGraphPptx::DeckResult resBad =
                ToriGraphPptx::exportGraphs(w79, QList<MyWidget *>() << mlLab, opt, pptxBad);
        CHECK_COND(!resBad.ok && !resBad.error.isEmpty(),
                   "79d: PptxExport to unwritable path did not fail cleanly");

        // 2-pass Graph::copy with ErrorBars (ASan verifies UAF freedom)
        Graph *gErr = mlLab->activeGraph();
        CHECK_COND(gErr->addErrorBars(tPlot->colName(0), tPlot->colName(1), tPlot, tPlot->colName(2)),
                   "79d: addErrorBars failed");
        MultiLayer *mlCopy = w79->newGraph("G79CopyErr");
        mlCopy->activeGraph()->copy(w79, gErr);
        QCoreApplication::processEvents();
        CHECK_COND(mlCopy->activeGraph()->curves() == 2,
                   "79d: Graph::copy with ErrorBars did not copy both curves");

        ml2Col->close();
        mlLab->close();
        mlCopy->close();
        tPlot->close();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // -------------------------------------------------------------------------
    // 79e: XRD Cu Ka avg + Ka2 ON default, Explorer helper hiding, ToriPlotThisDataBar dismissal,
    //      table-only Split full width, Focus HUD hiding menuBar, CenterWelcomeHubCard
    // -------------------------------------------------------------------------
    {
        // XRD analyzer defaults
        Table *tXrd = w79->newTable(10, 2, "T79XRD", "XRD");
        tXrd->setColPlotDesignation(0, SciDAVis::X);
        tXrd->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < 10; ++r) {
            tXrd->setCell(r, 0, 20.0 + r * 5.0);
            tXrd->setCell(r, 1, 100.0 + r * 10.0);
        }
        qApp->setProperty("torigraph_noninteractive", false);
        bool sawXrdDlg = false;
        bool defaultKaAvgAndKa2On = false;
        bool switchedToKa1TurnedOffKa2 = false;
        QTimer::singleShot(40, [&]() {
            if (QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                sawXrdDlg = true;
                QCheckBox *chkKa2 = nullptr;
                for (QCheckBox *cb : dlg->findChildren<QCheckBox *>()) {
                    if (cb->text().contains(QStringLiteral("K\u03B12"))
                        || cb->text().contains(QStringLiteral("Ka2"), Qt::CaseInsensitive)) {
                        chkKa2 = cb;
                        break;
                    }
                }
                QComboBox *cbAnode = nullptr;
                for (QComboBox *c : dlg->findChildren<QComboBox *>()) {
                    if (c->currentText().contains(QStringLiteral("Cu K\u03B1 avg"))) {
                        cbAnode = c;
                        break;
                    }
                }
                defaultKaAvgAndKa2On = (cbAnode != nullptr && chkKa2 != nullptr && chkKa2->isChecked());
                if (cbAnode && chkKa2 && cbAnode->count() > 1) {
                    cbAnode->setCurrentIndex(1); // Cu Ka1
                    QCoreApplication::processEvents();
                    switchedToKa1TurnedOffKa2 = !chkKa2->isChecked();
                }
                dlg->reject();
            }
        });
        w79->showXRDAnalyzerDialog();
        qApp->setProperty("torigraph_noninteractive", true);
        CHECK_COND(sawXrdDlg && defaultKaAvgAndKa2On && switchedToKa1TurnedOffKa2,
                   "79e: XRD defaults check failed (saw=" << sawXrdDlg << " def=" << defaultKaAvgAndKa2On
                                                          << " toggle=" << switchedToKa1TurnedOffKa2
                                                          << ")");

        // Explorer hides _tg_internal_helper & GadgetFit_*
        Table *tHelp = w79->newTable(2, 2, "T79InternalHelp", "Help");
        tHelp->setProperty("_tg_internal_helper", true);
        Table *tGad = w79->newTable(2, 2, "GadgetFit_79", "Gad");
        QCoreApplication::processEvents();
        w79->changeFolder(w79->projectFolder(), true);
        QCoreApplication::processEvents();
        for (int i = 0; i < w79->lv.topLevelItemCount(); ++i) {
            const QString txt = w79->lv.topLevelItem(i)->text(0);
            CHECK_COND(txt != QStringLiteral("T79InternalHelp") && txt != QStringLiteral("GadgetFit_79"),
                       "79e: internal helper window '" << txt.toStdString()
                                                       << "' leaked into Explorer!");
        }
        tHelp->close();
        tGad->close();

        // ToriPlotThisDataBar dismiss on plot / dismissPlotThisDataBar
        w79->offerPlotAfterImport(tXrd);
        QCoreApplication::processEvents();
        CHECK_COND(tXrd->findChild<QFrame *>(QStringLiteral("ToriPlotThisDataBar")) != nullptr,
                   "79e: offerPlotAfterImport did not attach ToriPlotThisDataBar");
        ToriUx::dismissPlotThisDataBar(tXrd);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CHECK_COND(tXrd->findChild<QFrame *>(QStringLiteral("ToriPlotThisDataBar")) == nullptr,
                   "79e: dismissPlotThisDataBar did not remove ToriPlotThisDataBar");

        // Table-only Split View expands full viewport width
        ModernRibbonBar *rib = w79->modernRibbonBar();
        CHECK_COND(rib != nullptr, "79e: modernRibbonBar null");
        rib->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
        QCoreApplication::processEvents();
        const int vpW = w79->d_workspace.viewport() ? w79->d_workspace.viewport()->width() : 0;
        CHECK_COND(vpW > 400 && tXrd->width() >= int(vpW * 0.85),
                   "79e: table-only Split View did not expand full width (tbl=" << tXrd->width()
                                                                                << " vp=" << vpW << ")");

        // Focus HUD hides menuBar
        CHECK_COND(w79->menuBar() != nullptr && !w79->menuBar()->isHidden(),
                   "79e: menuBar unexpectedly hidden before Focus HUD");
        rib->setFocusHudMode(true);
        QCoreApplication::processEvents();
        CHECK_COND(w79->menuBar()->isHidden(), "79e: Focus HUD left menuBar visible!");
        rib->setFocusHudMode(false);
        QCoreApplication::processEvents();
        CHECK_COND(!w79->menuBar()->isHidden(), "79e: exiting Focus HUD did not restore menuBar!");

        // Closing all windows surfaces CenterWelcomeHubCard
        tXrd->close();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        QFrame *hubCard = w79->findChild<QFrame *>(QStringLiteral("CenterWelcomeHubCard"));
        CHECK_COND(hubCard != nullptr && !hubCard->isHidden(),
                   "79e: CenterWelcomeHubCard not visible when zero windows remain!");
    }

    // -------------------------------------------------------------------------
    // 79f: app.open_project() error preservation, Export Python EN/JA, grab_gui_diagnostic
    // -------------------------------------------------------------------------
    {
        Table *tKeep = w79->newTable(3, 2, "T79Keep", "Keep");
        for (int r = 0; r < 3; ++r) {
            tKeep->setCell(r, 0, double(r));
            tKeep->setCell(r, 1, double(r * 10));
        }
        MultiLayer *mlExp = w79->multilayerPlot(tKeep, QStringList() << tKeep->colName(1), Graph::Line);
        CHECK_COND(mlExp != nullptr, "79f: mlExp null");
        const QString mlExpName = mlExp->name();
        QCoreApplication::processEvents();

        const QString keepProjPath = tmp79.filePath(QStringLiteral("keep_79.sciprj"));
        w79->projectname = keepProjPath;
        w79->recentProjects = QStringList{ QStringLiteral("C:/torigraph_keep_sentinel_79.sciprj") };
        CHECK_COND(w79->saveProject(), "79f: saveProject() on keepProjPath failed");
        const QStringList recentsBefore = w79->recentProjects;

        PythonAutomationServer *srv = w79->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "79f: pythonAutomationServer null");

        const QString fakeCsv = tmp79.filePath(QStringLiteral("bad_open_79.csv"));
        {
            QFile f(fakeCsv);
            f.open(QIODevice::WriteOnly);
            f.write("a,b\n1,2\n");
        }
        const QString corruptBodyPath = tmp79.filePath(QStringLiteral("corrupt_body_79.sciprj"));
        {
            QFile f(corruptBodyPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "79f: failed to write corruptBodyPath");
            f.write("SciDAVis\ncorrupt_body_no_version\n");
        }
        const QString midTruncPath = tmp79.filePath(QStringLiteral("mid_trunc_79.sciprj"));
        {
            QFile f(midTruncPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "79f: failed to write midTruncPath");
            f.write("SciDAVis 0.1.3 project file\n"
                    "<scripting-lang>\tPython\n"
                    "<windows>\t1\n"
                    "<table>\n"
                    "T79MidTrunc\t2\t2\t19/08/2026 00:00:00\n"
                    "<header>\tA[X]\tB[Y]\n");
        }
        const bool prevSavedState79 = w79->saved;
        w79->saved = false;
        for (const QString &badPath :
             QStringList{ tmp79.filePath(QStringLiteral("nonexistent_79.sciprj")), fakeCsv,
                          corruptBodyPath, midTruncPath }) {
            QJsonObject req;
            req["jsonrpc"] = "2.0";
            req["id"] = 7901;
            req["method"] = "app.open_project";
            req["params"] = QJsonObject{ { "path", badPath }, { "force", true } };
            const QJsonObject resp = srv->handleRpcRequest(req);
            CHECK_COND(resp.contains("error"),
                       "79f: app.open_project did not fail on invalid path: " << badPath.toStdString());
            CHECK_COND(w79->table("T79Keep") != nullptr,
                       "79f: invalid app.open_project wiped current project!");
            CHECK_COND(!w79->saved,
                       "79f: invalid app.open_project cleared dirty saved==false flag!");
            CHECK_COND(w79->projectname == keepProjPath,
                       "79f: rollback altered projectname to '"
                               << w79->projectname.toStdString() << "'");
            for (const QString &rp : w79->recentProjects) {
                CHECK_COND(!rp.contains(QLatin1String("torigraph_open_rb_")),
                           "79f: rollback temp path leaked into recentProjects: " << rp.toStdString());
            }
            CHECK_COND(w79->recentProjects == recentsBefore,
                       "79f: rollback mutated recentProjects");
        }
        w79->saved = prevSavedState79;

        // Export Python localization EN vs JA + multiline injection sanitization
        mlExp = qobject_cast<MultiLayer *>(w79->window(mlExpName));
        CHECK_COND(mlExp != nullptr, "79f: invalid app.open_project lost graph window after rollback!");
        const QVariant prevPresetProp = mlExp->property("toriAppliedStylePreset");
        mlExp->setProperty("toriAppliedStylePreset",
                           QStringLiteral("BadPreset\nimport os; os.system('echo pwn')"));

        w79->switchToLanguage("en");
        QCoreApplication::processEvents();
        const QString pyEn = PythonAutomationServer::generatePythonScriptForWindow(
                w79, mlExp, true, QStringLiteral("inline"));
        w79->switchToLanguage("ja");
        QCoreApplication::processEvents();
        const QString pyJa = PythonAutomationServer::generatePythonScriptForWindow(
                w79, mlExp, true, QStringLiteral("inline"));
        w79->switchToLanguage("en");
        QCoreApplication::processEvents();
        mlExp->setProperty("toriAppliedStylePreset", prevPresetProp);

        CHECK_COND(!pyEn.isEmpty() && !pyJa.isEmpty() && pyEn != pyJa,
                   "79f: Export Python output identical in EN and JA!");
        for (QChar ch : pyEn) {
            CHECK_COND(ch.unicode() <= 0x3000,
                       "79f: Export Python in EN mode leaked Japanese character U+"
                               << std::hex << int(ch.unicode()) << std::dec);
        }
        bool jaHasNonAscii = false;
        for (QChar ch : pyJa) {
            if (ch.unicode() > 0x3000) {
                jaHasNonAscii = true;
                break;
            }
        }
        CHECK_COND(jaHasNonAscii, "79f: Export Python in JA mode lacked Japanese comments!");
        auto hasInjectedImportLine = [](const QString &script) {
            const QStringList lines = script.split(QLatin1Char('\n'));
            for (const QString &ln : lines) {
                if (ln.trimmed().startsWith(QLatin1String("import os")))
                    return true;
            }
            return false;
        };
        CHECK_COND(!hasInjectedImportLine(pyEn) && !hasInjectedImportLine(pyJa),
                   "79f: Export Python did not sanitize multiline injection in style preset!");

        // grab_gui_diagnostic returns real visible geometry + zero clipped ribbon captions
        const QString diagPng = tmp79.filePath(QStringLiteral("diag_79.png"));
        QJsonObject diagReq;
        diagReq["jsonrpc"] = "2.0";
        diagReq["id"] = 7902;
        diagReq["method"] = "app.grab_gui_diagnostic";
        diagReq["params"] = QJsonObject{ { "path", diagPng }, { "width", 1366 }, { "height", 768 } };
        const QJsonObject diagResp = srv->handleRpcRequest(diagReq);
        CHECK_COND(!diagResp.contains("error"), "79f: grab_gui_diagnostic errored");
        const QJsonObject diagRes = diagResp.value("result").toObject();
        CHECK_COND(QFileInfo::exists(diagPng) && QFileInfo(diagPng).size() > 1000,
                   "79f: grab_gui_diagnostic did not write PNG");
        CHECK_COND(diagRes.value("ok").toBool() && diagRes.value("ribbon_bottom_y").toInt() > 0
                           && diagRes.value("width").toInt() == 1366
                           && diagRes.value("height").toInt() == 768
                           && diagRes.contains("clipped_ribbon_captions")
                           && diagRes.contains("inspector_clipped_bottom"),
                   "79f: grab_gui_diagnostic missing expected geometry/clip keys (w="
                           << diagRes.value("width").toInt() << " h="
                           << diagRes.value("height").toInt() << ")");
        CHECK_COND(diagRes.value("clipped_ribbon_captions").toInt() == 0,
                   "79f: grab_gui_diagnostic reported clipped ribbon captions: "
                           << diagRes.value("clipped_ribbon_captions").toInt());

        const QString diagExpandedPng = tmp79.filePath(QStringLiteral("diag_79_expanded.png"));
        QJsonObject diagReqExp;
        diagReqExp["jsonrpc"] = "2.0";
        diagReqExp["id"] = 7903;
        diagReqExp["method"] = "app.grab_gui_diagnostic";
        diagReqExp["params"] = QJsonObject{ { "path", diagExpandedPng },
                                            { "width", 1366 },
                                            { "height", 768 },
                                            { "show_explorer", true },
                                            { "expand_inspector", true },
                                            { "scroll_inspector", true } };
        const QJsonObject diagExpResp = srv->handleRpcRequest(diagReqExp);
        CHECK_COND(!diagExpResp.contains("error"),
                   "79f: grab_gui_diagnostic (explorer + expanded/scrolled inspector) errored");
        const QJsonObject diagExpRes = diagExpResp.value("result").toObject();
        CHECK_COND(diagExpRes.value("ok").toBool(),
                   "79f: grab_gui_diagnostic (explorer + expanded/scrolled inspector) returned ok=false: "
                           << diagExpRes.value("diagnostic_error").toString().toStdString()
                           << " expTop=" << diagExpRes.value("explorer_top_y").toInt()
                           << " inspTop=" << diagExpRes.value("inspector_top_y").toInt()
                           << " scrollMax=" << diagExpRes.value("inspector_scroll_max").toInt());
    }

    // -------------------------------------------------------------------------
    // 79g: HUD shortcuts, Plot bar X guard, SelectionMoveResizer deferred teardown,
    //      CSV reimportFromSource(), OPJU numeric type="3" X column
    // -------------------------------------------------------------------------
    {
        // Global shortcuts remain bound on ApplicationWindow when Focus HUD hides menuBar()
        const QList<QAction *> winActs = w79->actions();
        auto hasShortcutBound = [&](const QKeySequence &seq) -> bool {
            for (const QAction *act : winActs) {
                if (!act)
                    continue;
                if (act->shortcut() == seq || act->shortcuts().contains(seq))
                    return true;
            }
            return false;
        };
        CHECK_COND(hasShortcutBound(QKeySequence(QStringLiteral("Ctrl+Z")))
                           && hasShortcutBound(QKeySequence(QStringLiteral("Ctrl+Y")))
                           && hasShortcutBound(QKeySequence(QStringLiteral("Ctrl+S")))
                           && hasShortcutBound(QKeySequence(Qt::Key_F1))
                           && hasShortcutBound(QKeySequence(QStringLiteral("Ctrl+R"))),
                   "79g: core shortcut actions (Undo/Redo/Save/Help/Unzoom) are not attached to ApplicationWindow!");

        ModernRibbonBar *rib = w79->modernRibbonBar();
        CHECK_COND(rib != nullptr, "79g: modernRibbonBar null");
        rib->setFocusHudMode(true);
        QCoreApplication::processEvents();
        CHECK_COND(w79->menuBar() && w79->menuBar()->isHidden(),
                   "79g: Focus HUD did not hide menuBar");
        CHECK_COND(hasShortcutBound(QKeySequence(QStringLiteral("Ctrl+Z")))
                           && hasShortcutBound(QKeySequence(QStringLiteral("Ctrl+S"))),
                   "79g: core Undo/Save shortcuts unavailable while menuBar is hidden in Focus HUD");
        rib->setFocusHudMode(false);
        QCoreApplication::processEvents();

        // Regression test: ribbon button ancestor stylesheets must never leak unscoped
        // background:#FFFFFF into QToolTip / QTipLabel (white-on-white popup fix)
        {
            if (!qApp->styleSheet().contains(QLatin1String("QToolTip")))
                ModernUiTheme::applyModernTheme(qApp);
            w79->resize(1366, 768);
            w79->show();
            rib->setCurrentTabIndex(1);
            QCoreApplication::processEvents();

            QStackedWidget *tabStack =
                    rib->findChild<QStackedWidget *>(QStringLiteral("ModernRibbonTabStack"));
            CHECK_COND(tabStack != nullptr, "79g: ModernRibbonTabStack not found");
            QToolButton *tipBtn = nullptr;
            for (QToolButton *tb : tabStack->findChildren<QToolButton *>()) {
                if (tb && tb->isVisibleTo(rib) && tb->width() > 8 && tb->height() > 8) {
                    tipBtn = tb;
                    break;
                }
            }
            CHECK_COND(tipBtn != nullptr, "79g: no visible ribbon QToolButton found on tab 1");

            auto hasUnscopedBackground = [](const QString &qss) -> bool {
                QString topLevel;
                topLevel.reserve(qss.size());
                int depth = 0;
                for (QChar ch : qss) {
                    if (ch == QLatin1Char('{'))
                        ++depth;
                    else if (ch == QLatin1Char('}') && depth > 0)
                        --depth;
                    else if (depth == 0)
                        topLevel.append(ch);
                }
                return topLevel.contains(QLatin1String("background"), Qt::CaseInsensitive);
            };
            for (QWidget *anc = tipBtn; anc != nullptr; anc = anc->parentWidget()) {
                CHECK_COND(!hasUnscopedBackground(anc->styleSheet()),
                           "79g: unscoped background stylesheet on ribbon ancestor '"
                                   << anc->objectName().toStdString() << "'");
                if (anc == w79.get())
                    break;
            }

            struct TipEffectGuard {
                bool prevFade = QApplication::isEffectEnabled(Qt::UI_FadeTooltip);
                bool prevAnim = QApplication::isEffectEnabled(Qt::UI_AnimateTooltip);
                QPointer<QWidget> win;
                QPointer<QWidget> btn;
                bool prevWinAlways = false;
                bool prevBtnAlways = false;
                TipEffectGuard(QWidget *w, QWidget *b) : win(w), btn(b)
                {
                    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
                    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
                    if (win) {
                        prevWinAlways = win->testAttribute(Qt::WA_AlwaysShowToolTips);
                        win->setAttribute(Qt::WA_AlwaysShowToolTips, true);
                    }
                    if (btn) {
                        prevBtnAlways = btn->testAttribute(Qt::WA_AlwaysShowToolTips);
                        btn->setAttribute(Qt::WA_AlwaysShowToolTips, true);
                    }
                }
                ~TipEffectGuard()
                {
                    QToolTip::hideText();
                    if (btn)
                        btn->setAttribute(Qt::WA_AlwaysShowToolTips, prevBtnAlways);
                    if (win)
                        win->setAttribute(Qt::WA_AlwaysShowToolTips, prevWinAlways);
                    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, prevFade);
                    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, prevAnim);
                }
            } tipEffectGuard(w79.get(), tipBtn);

            QCoreApplication::processEvents();
            QToolTip::showText(tipBtn->mapToGlobal(tipBtn->rect().bottomLeft()),
                               QStringLiteral("Tooltip Contrast Check 12345"), tipBtn);

            QWidget *tipLbl = nullptr;
            for (QWidget *tl : QApplication::topLevelWidgets()) {
                if (!tl)
                    continue;
                const QString cls = QString::fromLatin1(tl->metaObject()->className());
                if ((cls == QLatin1String("QTipLabel")
                     || (tl->inherits("QLabel")
                         && (tl->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip))
                    && !tl->isHidden()) {
                    tipLbl = tl;
                    break;
                }
            }
            CHECK_COND(tipLbl != nullptr && QToolTip::isVisible(),
                       "79g: QToolTip::showText did not create visible QTipLabel (qtIsVis="
                               << QToolTip::isVisible() << " topLevels="
                               << QApplication::topLevelWidgets().size() << " btnVis="
                               << tipBtn->isVisible() << ")");
            tipLbl->ensurePolished();
            const QImage tipImg = tipLbl->grab().toImage().convertToFormat(QImage::Format_ARGB32);
            CHECK_COND(!tipImg.isNull() && tipImg.width() >= 20 && tipImg.height() >= 12,
                       "79g: QTipLabel image invalid or too small");

            int darkBgPixels = 0;
            int brightFgPixels = 0;
            int innerTotal = 0;
            const int x0 = 4;
            const int y0 = 4;
            const int x1 = tipImg.width() - 4;
            const int y1 = tipImg.height() - 4;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    const QRgb px = tipImg.pixel(x, y);
                    const int luma = qRound(0.299 * qRed(px) + 0.587 * qGreen(px)
                                            + 0.114 * qBlue(px));
                    if (luma < 80)
                        ++darkBgPixels;
                    else if (luma > 170)
                        ++brightFgPixels;
                    ++innerTotal;
                }
            }
            QToolTip::hideText();
            QCoreApplication::processEvents();
            CHECK_COND(innerTotal > 0 && darkBgPixels > innerTotal / 2 && brightFgPixels >= 10,
                       "79g: QTipLabel pixel contrast check failed (dark="
                               << darkBgPixels << " bright=" << brightFgPixels
                               << " total=" << innerTotal << ")");
        }

        // Y-only table must not get ToriPlotThisDataBar
        Table *tNoX = w79->newTable(4, 2, "T79NoX", "NoX");
        tNoX->setColPlotDesignation(0, SciDAVis::Y);
        tNoX->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < 4; ++r) {
            tNoX->setCell(r, 0, double(r));
            tNoX->setCell(r, 1, double(r * 2));
        }
        w79->offerPlotAfterImport(tNoX);
        QCoreApplication::processEvents();
        CHECK_COND(tNoX->findChild<QFrame *>(QStringLiteral("ToriPlotThisDataBar")) == nullptr,
                   "79g: offerPlotAfterImport unexpectedly attached ToriPlotThisDataBar to Y-only table!");
        tNoX->askOnCloseEvent(false);
        tNoX->close();

        // SelectionMoveResizer Escape & removeLayer() deferred teardown (ASan UAF guard)
        MultiLayer *mlSmr = w79->newGraph("G79Smr");
        CHECK_COND(mlSmr != nullptr, "79g: G79Smr creation failed");
        Graph *layer1 = mlSmr->activeGraph();
        Graph *layer2 = mlSmr->addLayer();
        CHECK_COND(layer1 != nullptr && layer2 != nullptr, "79g: addLayer failed");
        mlSmr->resize(640, 480);
        mlSmr->show();
        QCoreApplication::processEvents();
        layer1->setGeometry(10, 10, 160, 140);
        layer2->setGeometry(180, 10, 160, 140);
        QCoreApplication::processEvents();

        auto selectLayerViaClick = [&](Graph *targetG) -> QPointer<SelectionMoveResizer> {
            QWidget *host = targetG ? targetG->parentWidget() : nullptr;
            const QPoint hostPt = targetG->frameGeometry().topLeft() + QPoint(2, 2);
            if (QScrollArea *sa = mlSmr->findChild<QScrollArea *>(
                        QStringLiteral("MultiLayerCanvasScroll"))) {
                sa->ensureVisible(hostPt.x(), hostPt.y());
            }
            const QPoint mlPt = host ? host->mapTo(mlSmr, hostPt) : QPoint(182, 12);
            QMouseEvent pressEv(QEvent::MouseButtonPress, mlPt, Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier);
            QApplication::sendEvent(mlSmr, &pressEv);
            QMouseEvent relEv(QEvent::MouseButtonRelease, mlPt, Qt::LeftButton, Qt::NoButton,
                              Qt::NoModifier);
            QApplication::sendEvent(mlSmr, &relEv);
            QCoreApplication::processEvents();
            for (SelectionMoveResizer *sr : mlSmr->findChildren<SelectionMoveResizer *>()) {
                if (sr && sr->contains(static_cast<QWidget *>(targetG)))
                    return QPointer<SelectionMoveResizer>(sr);
            }
            return QPointer<SelectionMoveResizer>();
        };

        QPointer<SelectionMoveResizer> smrEsc = selectLayerViaClick(layer2);
        CHECK_COND(!smrEsc.isNull(), "79g: click did not create SelectionMoveResizer on layer2");
        QKeyEvent escEvt(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(smrEsc.data(), &escEvt);
        for (int pass = 0; pass < 2; ++pass) {
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        CHECK_COND(smrEsc.isNull(),
                   "79g: SelectionMoveResizer was not destroyed cleanly on Qt::Key_Escape");

        QPointer<SelectionMoveResizer> smrRm = selectLayerViaClick(layer2);
        CHECK_COND(!smrRm.isNull(), "79g: second click did not create SelectionMoveResizer on layer2");
        QPointer<Graph> layer2Guard(layer2);
        mlSmr->setActiveGraph(layer2);
        mlSmr->removeLayer();
        for (int pass = 0; pass < 2; ++pass) {
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        CHECK_COND(smrRm.isNull(),
                   "79g: SelectionMoveResizer leaked or dangled after MultiLayer::removeLayer()");
        CHECK_COND(layer2Guard.isNull() && mlSmr->activeGraph() != layer2,
                   "79g: removed layer2 still alive or still active after MultiLayer::removeLayer()");

        // CSV open() followed by overwrite + reimportFromSource()
        const QString csvPath = tmp79.filePath(QStringLiteral("reimport79.csv"));
        {
            QFile f(csvPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "79g: failed to write initial reimport79.csv");
            f.write("Strain,Stress\n0,0\n1,100\n2,200\n");
        }
        const QList<MyWidget *> winsBeforeCsv = w79->windowsList();
        w79->open(csvPath);
        QCoreApplication::processEvents();
        Table *tCsv = w79->table(QStringLiteral("reimport79"));
        if (!tCsv) {
            for (MyWidget *w : w79->windowsList()) {
                if (Table *cand = qobject_cast<Table *>(w)) {
                    if (!winsBeforeCsv.contains(cand)) {
                        tCsv = cand;
                        break;
                    }
                }
            }
        }
        CHECK_COND(tCsv != nullptr && tCsv->numRows() == 3 && tCsv->numCols() == 2,
                   "79g: w79->open(reimport79.csv) did not produce expected 3x2 table (ptr="
                           << (tCsv != nullptr) << " r=" << (tCsv ? tCsv->numRows() : -1)
                           << " c=" << (tCsv ? tCsv->numCols() : -1) << ")");
        const QString origCol0Name = tCsv->column(0) ? tCsv->column(0)->name() : QString();
        const QString origCol0Long = tCsv->column(0) ? tCsv->column(0)->longName() : QString();
        const QString origCol1Name = tCsv->column(1) ? tCsv->column(1)->name() : QString();
        const QString origCol1Long = tCsv->column(1) ? tCsv->column(1)->longName() : QString();
        CHECK_COND((origCol0Name == QLatin1String("Strain") || origCol0Long == QLatin1String("Strain"))
                           && (origCol1Name == QLatin1String("Stress")
                               || origCol1Long == QLatin1String("Stress")),
                   "79g: initial CSV headers did not preserve Strain/Stress");

        {
            QFile f(csvPath);
            CHECK_COND(f.open(QIODevice::WriteOnly | QIODevice::Truncate),
                       "79g: failed to overwrite reimport_79.csv");
            f.write("Strain,Stress\n0,0\n1,110\n2,220\n3,330\n");
        }
        CHECK_COND(tCsv->reimportFromSource(), "79g: reimportFromSource() failed on CSV");
        QCoreApplication::processEvents();
        CHECK_COND(tCsv->numRows() == 4,
                   "79g: CSV reimportFromSource row count mismatch: " << tCsv->numRows());
        const QString reCol0Name = tCsv->column(0) ? tCsv->column(0)->name() : QString();
        const QString reCol0Long = tCsv->column(0) ? tCsv->column(0)->longName() : QString();
        const QString reCol1Name = tCsv->column(1) ? tCsv->column(1)->name() : QString();
        const QString reCol1Long = tCsv->column(1) ? tCsv->column(1)->longName() : QString();
        CHECK_COND((reCol0Name == QLatin1String("Strain") || reCol0Long == QLatin1String("Strain"))
                           && (reCol1Name == QLatin1String("Stress")
                               || reCol1Long == QLatin1String("Stress")),
                   "79g: CSV reimportFromSource lost Strain/Stress header names");
        CHECK_COND(std::fabs(tCsv->cell(0, 1) - 0.0) < 1e-9
                           && std::fabs(tCsv->cell(3, 1) - 330.0) < 1e-9,
                   "79g: CSV reimportFromSource values mismatch: c(0,1)=" << tCsv->cell(0, 1)
                                                                          << " c(3,1)=" << tCsv->cell(3, 1));

        // Minimal ZIP .opju with type="3" Numeric X column (must not misclassify as Time/DateTime)
        const QByteArray opjuXml =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<OriginProject version=\"2024\">\n"
                "  <Folder name=\"Type3Test\">\n"
                "    <Workbook name=\"Type3Book\">\n"
                "      <Worksheet name=\"Sheet1\">\n"
                "        <Column name=\"A\" type=\"3\" designation=\"X\" longName=\"TimeOrX\">\n"
                "          <Data><v>0.0</v><v>1.0</v><v>2.0</v></Data>\n"
                "        </Column>\n"
                "        <Column name=\"B\" designation=\"Y\" longName=\"Signal\">\n"
                "          <Data><v>10.0</v><v>20.0</v><v>30.0</v></Data>\n"
                "        </Column>\n"
                "      </Worksheet>\n"
                "    </Workbook>\n"
                "  </Folder>\n"
                "</OriginProject>\n";
        torizip::ZipWriter opjuZw;
        CHECK_COND(opjuZw.addFile(QStringLiteral("OriginProj.xml"), opjuXml,
                                  torizip::ZipWriter::Compression::Store),
                   "79g: ZipWriter::addFile failed");
        const QString synOpjuPath = tmp79.filePath(QStringLiteral("type3_79.opju"));
        {
            QFile f(synOpjuPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "79g: failed to write type3_79.opju");
            f.write(opjuZw.finish());
        }
        QString opjuErr;
        ImportOPJU::ImportStats opjuStats;
        CHECK_COND(ImportOPJU::importFile(w79, synOpjuPath, &opjuErr, &opjuStats),
                   "79g: ImportOPJU::importFile failed: " << opjuErr.toStdString());
        Table *tOpju = w79->table(QStringLiteral("Type3Book"));
        if (!tOpju) {
            std::function<void(Folder *)> walkOpju = [&](Folder *f) {
                if (!f || tOpju)
                    return;
                for (MyWidget *w : f->windowsList()) {
                    if (Table *cand = qobject_cast<Table *>(w)) {
                        if (cand->name().contains(QStringLiteral("Type3Book"))) {
                            tOpju = cand;
                            return;
                        }
                    }
                }
                for (Folder *sub : f->folders())
                    walkOpju(sub);
            };
            walkOpju(w79->projectFolder());
        }
        CHECK_COND(tOpju != nullptr && tOpju->column(0) != nullptr,
                   "79g: Type3Book table missing after ImportOPJU");
        CHECK_COND(tOpju->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric,
                   "79g: OPJU column with type=\"3\" was not imported as Numeric!");
        CHECK_COND(std::fabs(tOpju->cell(0, 0) - 0.0) < 1e-9
                           && std::fabs(tOpju->cell(1, 0) - 1.0) < 1e-9
                           && std::fabs(tOpju->cell(2, 0) - 2.0) < 1e-9,
                   "79g: OPJU type=\"3\" column numeric values mismatch");
    }

    // -------------------------------------------------------------------------
    // 79h: Graph::copy Legend token remap across interleaved ErrorBars,
    //      1x2 isij_85mm square aspect & explicit Y step preservation,
    //      RecalcManager Auto Undo/Redo replay, moveFolder children, aborted appendProject undo
    // -------------------------------------------------------------------------
    {
        // Interleaved ErrorBars curve index remapping in Graph::copy
        Table *tLeg = w79->newTable(4, 4, "T79LegCopy", "LegCopy");
        for (int r = 0; r < 4; ++r) {
            tLeg->setCell(r, 0, double(r));
            tLeg->setCell(r, 1, double(r + 1));
            tLeg->setCell(r, 2, 0.2);
            tLeg->setCell(r, 3, double(2 * r + 5));
        }
        MultiLayer *mlSrc = w79->newGraph("G79CopySrc");
        Graph *gSrc = mlSrc->activeGraph();
        CHECK_COND(gSrc != nullptr, "79h: G79CopySrc activeGraph null");
        gSrc->insertCurve(tLeg, tLeg->colName(1), Graph::Line);
        CHECK_COND(gSrc->addErrorBars(tLeg->colName(0), tLeg->colName(1), tLeg, tLeg->colName(2)),
                   "79h: addErrorBars returned false");
        gSrc->insertCurve(tLeg, tLeg->colName(3), Graph::Line);
        const int idxY1Src = gSrc->curveIndex(tLeg->colName(1));
        const int idxY2Src = gSrc->curveIndex(tLeg->colName(3));
        CHECK_COND(idxY1Src == 0 && idxY2Src == 2,
                   "79h: unexpected source curve indices y1=" << idxY1Src << " y2=" << idxY2Src);
        Legend *srcLeg = gSrc->legend();
        CHECK_COND(srcLeg != nullptr, "79h: source Legend null");
        const QString srcText = QStringLiteral("\\l(%1) %(%1)\n\\l(%2) %(%2)")
                                        .arg(idxY1Src + 1)
                                        .arg(idxY2Src + 1);
        srcLeg->setText(srcText);

        MultiLayer *mlDst = w79->newGraph("G79CopyDst");
        Graph *gDst = mlDst->activeGraph();
        CHECK_COND(gDst != nullptr, "79h: G79CopyDst activeGraph null");
        gDst->copy(w79, gSrc);
        const int idxY1Dst = gDst->curveIndex(tLeg->colName(1));
        const int idxY2Dst = gDst->curveIndex(tLeg->colName(3));
        Legend *dstLeg = gDst->legend();
        CHECK_COND(idxY1Dst >= 0 && idxY2Dst >= 0 && dstLeg != nullptr,
                   "79h: Graph::copy lost curves or legend");
        CHECK_COND(gDst->curveType(idxY1Dst) != Graph::ErrorBars
                           && gDst->curveType(idxY2Dst) != Graph::ErrorBars,
                   "79h: Graph::copy curve mapping landed on ErrorBars");
        const QString expectedDst = QStringLiteral("\\l(%1) %(%1)\n\\l(%2) %(%2)")
                                            .arg(idxY1Dst + 1)
                                            .arg(idxY2Dst + 1);
        CHECK_COND(expectedDst != srcText && dstLeg->text() == expectedDst,
                   "79h: Graph::copy did not remap Legend tokens (src='"
                           << srcText.toStdString() << "', got '"
                           << dstLeg->text().toStdString() << "', expected '"
                           << expectedDst.toStdString() << "')");

        // 1x2 isij_85mm preserves square panels & explicit Y-axis step
        Table *tTwo = w79->newTable(5, 3, "T79IsijTwo", "IsijTwo");
        for (int r = 0; r < 5; ++r) {
            tTwo->setCell(r, 0, double(r * 10));
            tTwo->setCell(r, 1, double(r * 50));
            tTwo->setCell(r, 2, double(200 - r * 40));
        }
        MultiLayer *mlTwo = w79->newGraph("G79IsijTwo");
        Graph *gp1 = mlTwo->activeGraph();
        Graph *gp2 = mlTwo->addLayer();
        CHECK_COND(gp1 != nullptr && gp2 != nullptr, "79h: G79IsijTwo layers null");
        gp1->insertCurve(tTwo, tTwo->colName(1), Graph::Line);
        gp2->insertCurve(tTwo, tTwo->colName(2), Graph::Line);
        mlTwo->setRows(1);
        mlTwo->setCols(2);
        gp1->setScale(QwtPlot::yLeft, 0.0, 200.0, 50.0);
        CHECK_COND(mlTwo->applyStylePresetById(QStringLiteral("isij_85mm")),
                   "79h: applyStylePresetById(isij_85mm) failed");
        QCoreApplication::processEvents();
        CHECK_COND(std::fabs(gp1->axisStep(QwtPlot::yLeft) - 50.0) < 1e-6,
                   "79h: 1x2 isij_85mm overwrote explicit Y-axis step 50.0 (got "
                           << gp1->axisStep(QwtPlot::yLeft) << ")");
        CHECK_COND(gp1->plotWidget() && gp1->plotWidget()->canvas() && gp2->plotWidget()
                           && gp2->plotWidget()->canvas(),
                   "79h: G79IsijTwo plot canvas null");
        const QSize sz1 = gp1->plotWidget()->canvas()->size();
        const QSize sz2 = gp2->plotWidget()->canvas()->size();
        const double ar1 = (sz1.height() > 0) ? double(sz1.width()) / double(sz1.height()) : 0.0;
        const double ar2 = (sz2.height() > 0) ? double(sz2.width()) / double(sz2.height()) : 0.0;
        CHECK_COND(ar1 > 0.85 && ar1 < 1.15 && ar2 > 0.85 && ar2 < 1.15,
                   "79h: 1x2 isij_85mm canvas aspect drifted from square: ar1="
                           << ar1 << " ar2=" << ar2);

        // RecalcManager Auto mode recalculates dirty links after Undo/Redo replay
        Table *tRec = w79->newTable(5, 2, "T79Rec", "Rec");
        for (int r = 0; r < 5; ++r) {
            tRec->setCell(r, 0, double(r));
            tRec->setCell(r, 1, double(2 * r));
        }
        MultiLayer *mlRec = w79->multilayerPlot(tRec, QStringList() << tRec->colName(1), Graph::Scatter);
        Graph *gRec = mlRec->activeGraph();
        CHECK_COND(gRec != nullptr, "79h: mlRec activeGraph null");
        RecalcManager *rm = RecalcManager::instance(w79);
        CHECK_COND(rm != nullptr, "79h: RecalcManager null");
        rm->setRecalcMode(RecalcManager::Auto);
        const int lid = rm->registerLink(w79, gRec, tRec, tRec->colName(1),
                                         QStringLiteral("Rec79AutoFit"), RecalcManager::OpLinearFit);
        CHECK_COND(lid > 0, "79h: RecalcManager::registerLink failed");
        QCoreApplication::processEvents();
        w79->d_workspace.setActiveSubWindow(tRec);
        tRec->column(1)->setValueAt(4, 16.0);
        QCoreApplication::processEvents();
        QwtPlotCurve *fitEdited = gRec->curve(QStringLiteral("Rec79AutoFit"));
        CHECK_COND(fitEdited != nullptr && fitEdited->dataSize() >= 2,
                   "79h: Rec79AutoFit curve missing after edit");
        const double yEndEdited = fitEdited->y(fitEdited->dataSize() - 1);
        w79->undo();
        QCoreApplication::processEvents();
        CHECK_COND(!rm->hasDirtyLinks(gRec),
                   "79h: RecalcManager still has dirty links after Undo replay!");
        fitEdited = gRec->curve(QStringLiteral("Rec79AutoFit"));
        CHECK_COND(fitEdited != nullptr && std::fabs(tRec->cell(4, 1) - 8.0) < 1e-9,
                   "79h: Undo did not revert T79Rec cell(4,1) to 8.0");
        const double yEndUndone = fitEdited->y(fitEdited->dataSize() - 1);
        CHECK_COND(yEndUndone < yEndEdited - 1.0 && std::fabs(yEndUndone - 8.0) < 1e-5,
                   "79h: RecalcManager Auto did not recalculate fit after Undo (yEndUndone="
                           << yEndUndone << ")");
        w79->redo();
        QCoreApplication::processEvents();
        CHECK_COND(!rm->hasDirtyLinks(gRec),
                   "79h: RecalcManager still has dirty links after Redo replay!");
        fitEdited = gRec->curve(QStringLiteral("Rec79AutoFit"));
        const double yEndRedone = fitEdited ? fitEdited->y(fitEdited->dataSize() - 1) : 0.0;
        CHECK_COND(std::fabs(yEndRedone - yEndEdited) < 1e-5,
                   "79h: RecalcManager Auto did not recalculate fit after Redo (got "
                           << yEndRedone << " expected " << yEndEdited << ")");
        rm->removeLinksForGraph(gRec);

        // moveFolder preserves multiple child subfolders cleanly
        Folder *rootF = w79->projectFolder();
        CHECK_COND(rootF != nullptr, "79h: rootF null");
        Folder &parentA = rootF->addChild<Folder>(QStringLiteral("MoveParentA79"));
        w79->addFolderListViewItem(&parentA);
        FolderListItem *parentAItem = new FolderListItem(rootF->folderListItem(), &parentA);
        parentA.setFolderListItem(parentAItem);

        Folder &child1 = parentA.addChild<Folder>(QStringLiteral("MoveChild1_79"));
        FolderListItem *child1Item = new FolderListItem(parentAItem, &child1);
        child1.setFolderListItem(child1Item);

        Folder &child2 = parentA.addChild<Folder>(QStringLiteral("MoveChild2_79"));
        FolderListItem *child2Item = new FolderListItem(parentAItem, &child2);
        child2.setFolderListItem(child2Item);

        Folder &destB = rootF->addChild<Folder>(QStringLiteral("MoveDestB79"));
        w79->addFolderListViewItem(&destB);
        FolderListItem *destBItem = new FolderListItem(rootF->folderListItem(), &destB);
        destB.setFolderListItem(destBItem);

        w79->moveFolder(parentAItem, destBItem);
        QCoreApplication::processEvents();
        Folder *movedParent = destB.findSubfolder(QStringLiteral("MoveParentA79"));
        CHECK_COND(movedParent != nullptr, "79h: moveFolder did not relocate MoveParentA79 under MoveDestB79");
        CHECK_COND(movedParent->findSubfolder(QStringLiteral("MoveChild1_79")) != nullptr
                           && movedParent->findSubfolder(QStringLiteral("MoveChild2_79")) != nullptr,
                   "79h: moveFolder dropped sibling child subfolders!");
        CHECK_COND(rootF->findSubfolder(QStringLiteral("MoveParentA79")) == nullptr,
                   "79h: original MoveParentA79 still present under root after moveFolder");

        // Aborted appendProject (both preflight-rejected and mid-load truncated) leaves no
        // orphan folder/table even across Undo/Redo
        w79->changeFolder(rootF, true);
        const QString truncProjPath = tmp79.filePath(QStringLiteral("trunc_append_79.sciprj"));
        {
            QFile f(truncProjPath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "79h: failed to write trunc_append_79.sciprj");
            f.write("SciDAVis 2.9.0 project file\n"
                    "<scripting-lang>\tPython\n"
                    "<windows>\t1\n"
                    "<table>\n"
                    "T79TruncZombie\t2\t2\t19/08/2026 00:00:00\n"
                    "<header>\tA[X]\tB[Y]\n");
        }
        const QString midAbortProjPath = tmp79.filePath(QStringLiteral("mid_abort_append_79.sciprj"));
        {
            QFile f(midAbortProjPath);
            CHECK_COND(f.open(QIODevice::WriteOnly),
                       "79h: failed to write mid_abort_append_79.sciprj");
            f.write("SciDAVis 0.1.3 project file\n"
                    "<scripting-lang>\tPython\n"
                    "<windows>\t2\n"
                    "<table>\n"
                    "T79ValidPreAbort\t1\t2\t19/08/2026 00:00:00\n"
                    "geometry\t0\t0\t200\t200\tactive\n"
                    "header\tX[X]\tY[Y]\n"
                    "<data>\n"
                    "0\t1\t2\n"
                    "</data>\n"
                    "</table>\n"
                    "<table>\n"
                    "BadHeaderOnlyOneField\n"
                    "</table>\n");
        }
        const int foldersBeforeAppend = rootF->folders().size();
        for (const QString &badAppendPath : QStringList{ truncProjPath, midAbortProjPath }) {
            w79->appendProject(badAppendPath);
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            CHECK_COND(rootF->folders().size() == foldersBeforeAppend,
                       "79h: aborted appendProject left an orphan subfolder for "
                               << badAppendPath.toStdString());
            CHECK_COND(w79->table(QStringLiteral("T79TruncZombie")) == nullptr
                               && w79->table(QStringLiteral("T79ValidPreAbort")) == nullptr,
                       "79h: aborted appendProject left zombie table behind for "
                               << badAppendPath.toStdString());
        }
        w79->d_workspace.setActiveSubWindow(tRec);
        for (int step = 0; step < 3; ++step) {
            w79->undo();
            QCoreApplication::processEvents();
            CHECK_COND(w79->table(QStringLiteral("T79TruncZombie")) == nullptr
                               && w79->table(QStringLiteral("T79ValidPreAbort")) == nullptr,
                       "79h: Undo after aborted appendProject resurrected zombie table!");
        }
        for (int step = 0; step < 3; ++step) {
            w79->redo();
            QCoreApplication::processEvents();
            CHECK_COND(w79->table(QStringLiteral("T79TruncZombie")) == nullptr
                               && w79->table(QStringLiteral("T79ValidPreAbort")) == nullptr,
                       "79h: Redo after aborted appendProject resurrected zombie table!");
        }
    }

    w79->switchToLanguage("en");
    QCoreApplication::processEvents();
    w79.reset();
    std::cout << "[PASS] Suite 79 passed.\n";
    return 0;
}
