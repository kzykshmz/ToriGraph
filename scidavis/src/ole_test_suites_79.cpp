#include "ole_test_common.h"
#include "future/table/TableModel.h"

#include <QTableView>

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
        QCoreApplication::processEvents();

        PythonAutomationServer *srv = w79->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "79f: pythonAutomationServer null");

        const QString fakeCsv = tmp79.filePath(QStringLiteral("bad_open_79.csv"));
        {
            QFile f(fakeCsv);
            f.open(QIODevice::WriteOnly);
            f.write("a,b\n1,2\n");
        }
        for (const QString &badPath :
             QStringList{ tmp79.filePath(QStringLiteral("nonexistent_79.sciprj")), fakeCsv }) {
            QJsonObject req;
            req["jsonrpc"] = "2.0";
            req["id"] = 7901;
            req["method"] = "app.open_project";
            req["params"] = QJsonObject{ { "path", badPath } };
            const QJsonObject resp = srv->handleRpcRequest(req);
            CHECK_COND(resp.contains("error"),
                       "79f: app.open_project did not fail on invalid path: " << badPath.toStdString());
            CHECK_COND(w79->table("T79Keep") != nullptr,
                       "79f: invalid app.open_project wiped current project!");
        }

        // Export Python localization EN vs JA
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
        CHECK_COND(!pyEn.isEmpty() && !pyJa.isEmpty() && pyEn != pyJa,
                   "79f: Export Python output identical in EN and JA!");
        bool jaHasNonAscii = false;
        for (QChar ch : pyJa) {
            if (ch.unicode() > 0x3000) {
                jaHasNonAscii = true;
                break;
            }
        }
        CHECK_COND(jaHasNonAscii, "79f: Export Python in JA mode lacked Japanese comments!");

        // grab_gui_diagnostic returns real visible geometry + clip report
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
                           && diagRes.contains("clipped_ribbon_captions")
                           && diagRes.contains("inspector_clipped_bottom"),
                   "79f: grab_gui_diagnostic missing expected geometry/clip keys");
    }

    w79->switchToLanguage("en");
    QCoreApplication::processEvents();
    w79.reset();
    std::cout << "[PASS] Suite 79 passed.\n";
    return 0;
}
