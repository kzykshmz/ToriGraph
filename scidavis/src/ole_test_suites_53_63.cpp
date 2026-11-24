#include "ole_test_common.h"

// =============================================================================
// Suite 53: Project save safety (failed saves are reported, the existing file is never
//           truncated, compressed saves keep a backup), undo history survives saving,
//           and auto-save writes a separate recovery copy.
// =============================================================================
static QByteArray suite53ReadAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

#include <QAbstractButton>
#include <QDateTime>
#include <QGroupBox>
#include <QMessageBox>
#include <QSet>
#include <QTreeWidgetItemIterator>
#include "PythonAutomationServer.h"

static bool suite59WriteFileEarly(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    const bool ok = f.write(bytes) == bytes.size();
    f.close();
    return ok;
}

int runSuite53SaveSafetyUndoAndRecovery(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 53: Save Safety, Undo-after-Save & Auto-save Recovery Gate...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 53");

    QTemporaryDir tmpDir53;
    CHECK_COND(tmpDir53.isValid(), "Suite 53: Failed to create QTemporaryDir");

    // No message boxes while saving is made to fail on purpose
    qApp->setProperty("torigraph_noninteractive", true);

    Table *t = mw->newTable("Save53", 5, 2);
    CHECK_COND(t != nullptr, "Suite 53: Failed to create table");
    for (int r = 0; r < 5; ++r) {
        t->setCell(r, 0, r);
        t->setCell(r, 1, r * 10.0);
    }

    const QString prj = tmpDir53.filePath(QString::fromUtf8("save53_\xE4\xBF\x9D\xE5\xAD\x98 test.sciprj"));

    // ---- 53a: first save
    CHECK_COND(mw->saveFolder(mw->projectFolder(), prj), "53a: saveFolder reported failure");
    CHECK_COND(QFileInfo::exists(prj), "53a: project file missing");
    CHECK_COND(!QFileInfo::exists(prj + ".new"), "53a: .new left behind");
    CHECK_COND(!QFileInfo::exists(prj + "~"), "53a: unexpected backup on first save");
    const QByteArray v1 = suite53ReadAll(prj);
    CHECK_COND(v1.size() > 100, "53a: project file too small");

    // ---- 53b: second save keeps the previous version as backup
    t->setCell(0, 1, 111.0);
    CHECK_COND(mw->saveFolder(mw->projectFolder(), prj), "53b: second saveFolder failed");
    const QByteArray v2 = suite53ReadAll(prj);
    CHECK_COND(v2 != v1, "53b: project file did not change");
    CHECK_COND(suite53ReadAll(prj + "~") == v1, "53b: backup is not the previous version");
    CHECK_COND(!QFileInfo::exists(prj + ".new"), "53b: .new left behind");

    // ---- 53c: unwritable location is reported as failure
    CHECK_COND(!mw->saveFolder(mw->projectFolder(), tmpDir53.filePath("no_such_dir/x.sciprj")),
               "53c: saveFolder claimed success for a missing directory");

#ifdef Q_OS_WIN
    // ---- 53d: target locked by another handle: failure, old file and backup untouched
    {
        const std::wstring wPrj = QDir::toNativeSeparators(prj).toStdWString();
        HANDLE lock = CreateFileW(wPrj.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK_COND(lock != INVALID_HANDLE_VALUE, "53d: could not lock project file");
        t->setCell(0, 1, 222.0);
        const bool lockedSaveOk = mw->saveFolder(mw->projectFolder(), prj);
        CloseHandle(lock);
        CHECK_COND(!lockedSaveOk, "53d: saveFolder claimed success although the file was locked");
        CHECK_COND(suite53ReadAll(prj) == v2, "53d: locked project file was modified");
        CHECK_COND(suite53ReadAll(prj + "~") == v1, "53d: backup was lost by a failed save");
        const QByteArray pending = suite53ReadAll(prj + ".new");
        CHECK_COND(pending.size() > 100 && pending != v2,
                   "53d: new data was not kept in .new after the failed save");

        // once the lock is gone, saving works again
        CHECK_COND(mw->saveFolder(mw->projectFolder(), prj), "53d: save after unlock failed");
        CHECK_COND(suite53ReadAll(prj) == pending, "53d: saved content differs from pending data");
        CHECK_COND(suite53ReadAll(prj + "~") == v2, "53d: backup not rotated after unlock");
        CHECK_COND(!QFileInfo::exists(prj + ".new"), "53d: .new left behind after unlock");
    }
#endif

    // ---- 53e: compressed save does not touch a sibling .sciprj and keeps a .gz~ backup
    {
        const QString sibling = tmpDir53.filePath("gz53.sciprj");
        const QString gzPrj = sibling + ".gz";
        {
            QFile sf(sibling);
            CHECK_COND(sf.open(QIODevice::WriteOnly), "53e: cannot create sibling file");
            sf.write("SIBLING-MARKER");
        }
        CHECK_COND(mw->writeProjectFile(mw->projectFolder(), gzPrj), "53e: compressed save failed");
        const QByteArray gz1 = suite53ReadAll(gzPrj);
        CHECK_COND(gz1.size() > 20 && static_cast<unsigned char>(gz1[0]) == 0x1f
                           && static_cast<unsigned char>(gz1[1]) == 0x8b,
                   "53e: output is not gzip data");
        CHECK_COND(suite53ReadAll(sibling) == QByteArray("SIBLING-MARKER"),
                   "53e: sibling .sciprj was overwritten by the compressed save");
        CHECK_COND(!QFileInfo::exists(sibling + "~"), "53e: sibling backup was created");
        CHECK_COND(QDir(tmpDir53.path()).entryList(QStringList() << "*.tmp" << "*.new").isEmpty(),
                   "53e: scratch files left behind");

        t->setCell(1, 1, 333.0);
        CHECK_COND(mw->writeProjectFile(mw->projectFolder(), gzPrj), "53e: second compressed save failed");
        CHECK_COND(suite53ReadAll(gzPrj + "~") == gz1, "53e: .gz~ backup is not the previous version");

        // the compressed project can be opened again
        ApplicationWindow *reopened = mw->openProject(gzPrj);
        CHECK_COND(reopened != nullptr, "53e: compressed project could not be reopened");
        CHECK_COND(reopened->table("Save53") != nullptr, "53e: table missing after reopening");
        reopened->savedProject();
        reopened->setOleEmbeddedMode(true);
        delete reopened;
    }

    // ---- 53f: a failed saveProject() leaves the project marked as modified
    {
        mw->projectname = tmpDir53.filePath("no_such_dir/fail53.sciprj");
        mw->modifiedProject();
        CHECK_COND(!mw->saveProject(), "53f: saveProject claimed success");
        CHECK_COND(!mw->saved, "53f: project marked as saved although saving failed");
    }

    // ---- 53g: saving keeps the table undo history
    {
        mw->projectname = prj;
        QUndoStack *stack = mw->project()->undoStack();
        CHECK_COND(stack != nullptr, "53g: no project undo stack");
        t->column(1)->setValueAt(2, 444.0);
        const int countBefore = stack->count();
        CHECK_COND(countBefore > 0 && stack->canUndo(), "53g: edit was not recorded for undo");
        CHECK_COND(mw->saveProject(), "53g: saveProject failed");
        CHECK_COND(mw->saved, "53g: project not marked as saved");
        CHECK_COND(stack->count() == countBefore && stack->canUndo(),
                   "53g: undo history was cleared by saving");
        CHECK_COND(stack->isClean(), "53g: undo stack not clean after saving");
        stack->undo();
        CHECK_COND(std::fabs(t->cell(2, 1) - 444.0) > 1e-9, "53g: undo after save did not revert the edit");
    }

    // ---- 53h: auto-save writes a recovery copy and leaves the project file alone
    {
        QStandardPaths::setTestModeEnabled(true);
        qApp->setProperty("torigraph_noninteractive", QVariant());
        const QString recDir = ApplicationWindow::recoveryDirPath();
        QDir(recDir).removeRecursively();
        const QByteArray onDisk = suite53ReadAll(prj);

        mw->modifiedProject();
        mw->writeRecoveryFile(); // disabled: nothing may be written
        CHECK_COND(QDir(recDir).entryList(QDir::Files).isEmpty(),
                   "53h: recovery copy written although recovery is disabled");

        ApplicationWindow::setCrashRecoveryEnabled(true);
        const bool wasVisible = mw->isVisible();
        mw->writeRecoveryFile();
        const QStringList copies = QDir(recDir).entryList(QStringList() << "*.sciprj", QDir::Files);
        const QStringList metas = QDir(recDir).entryList(QStringList() << "*.json", QDir::Files);
        const bool skippedAsNonInteractive = ApplicationWindow::isNonInteractiveContext(mw);
        if (!skippedAsNonInteractive) {
            CHECK_COND(copies.size() == 1 && metas.size() == 1, "53h: recovery copy not written");
            CHECK_COND(suite53ReadAll(QDir(recDir).filePath(copies.first())).size() > 100,
                       "53h: recovery copy is empty");
            CHECK_COND(suite53ReadAll(QDir(recDir).filePath(metas.first()))
                               .contains(QFileInfo(prj).fileName().toUtf8()),
                       "53h: recovery metadata does not name the project");
            CHECK_COND(suite53ReadAll(prj) == onDisk, "53h: auto-save modified the project file");
            CHECK_COND(!mw->saved, "53h: auto-save marked the project as saved");

            mw->writeRecoveryFile(); // second tick overwrites the same copy
            CHECK_COND(QDir(recDir).entryList(QStringList() << "*.sciprj", QDir::Files).size() == 1,
                       "53h: second auto-save created another copy");

            CHECK_COND(mw->saveProject(), "53h: saveProject failed");
            CHECK_COND(QDir(recDir).entryList(QDir::Files).isEmpty(),
                       "53h: recovery copy not removed after saving");
        } else {
            std::cout << "[INFO] Suite 53h: window is non-interactive, recovery write skipped as designed\n";
            CHECK_COND(copies.isEmpty(), "53h: recovery copy written in non-interactive context");
        }
        Q_UNUSED(wasVisible)
        ApplicationWindow::setCrashRecoveryEnabled(false);
        QDir(recDir).removeRecursively();
        QStandardPaths::setTestModeEnabled(false);
    }

    // ---- 53i: app.save_project (Python RPC) uses the same safe save as File > Save
    {
        qApp->setProperty("torigraph_noninteractive", true);
        PythonAutomationServer *srv = mw->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "53i: no RPC server");
        int rpcId = 5300;
        auto rpcSave = [&](const QString &path) {
            QJsonObject req;
            req["jsonrpc"] = "2.0";
            req["id"] = ++rpcId;
            req["method"] = "app.save_project";
            req["params"] = QJsonObject{ { "path", path } };
            return srv->handleRpcRequest(req);
        };
        const QString rpcPrj = tmpDir53.filePath("rpc53.sciprj");
        QUndoStack *stack = mw->project()->undoStack();
        t->column(1)->setValueAt(3, 555.0);
        const int countBefore = stack->count();
        const QJsonObject r1 = rpcSave(rpcPrj);
        CHECK_COND(r1.value("result").toObject().value("ok").toBool(), "53i: RPC save failed");
        CHECK_COND(QFileInfo::exists(rpcPrj) && !QFileInfo::exists(rpcPrj + ".new")
                           && !QFileInfo::exists(rpcPrj + "~"),
                   "53i: first RPC save left .new / an unexpected backup");
        CHECK_COND(stack->count() == countBefore && stack->canUndo() && stack->isClean(),
                   "53i: RPC save cleared the GUI undo history");
        CHECK_COND(mw->saved, "53i: RPC save did not mark the project as saved");
        const QByteArray rv1 = suite53ReadAll(rpcPrj);
        t->column(1)->setValueAt(3, 556.0);
        CHECK_COND(rpcSave(rpcPrj).value("result").toObject().value("ok").toBool(),
                   "53i: second RPC save failed");
        CHECK_COND(suite53ReadAll(rpcPrj + "~") == rv1,
                   "53i: RPC save did not keep the previous version as '~'");
#ifdef Q_OS_WIN
        {
            const QByteArray rv2 = suite53ReadAll(rpcPrj);
            const std::wstring wPath = QDir::toNativeSeparators(rpcPrj).toStdWString();
            HANDLE lock = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            CHECK_COND(lock != INVALID_HANDLE_VALUE, "53i: could not lock the RPC project file");
            t->column(1)->setValueAt(3, 557.0);
            const QJsonObject rLocked = rpcSave(rpcPrj);
            CloseHandle(lock);
            CHECK_COND(rLocked.contains("error") && !rLocked.contains("result"),
                       "53i: RPC save into a locked file did not return an RPC error");
            CHECK_COND(suite53ReadAll(rpcPrj) == rv2,
                       "53i: failed RPC save changed the project file (no direct overwrite allowed)");
            CHECK_COND(!mw->saved, "53i: failed RPC save marked the project as saved");
            QFile::remove(rpcPrj + ".new");
        }
#endif
        const QJsonObject rGz = rpcSave(tmpDir53.filePath("rpc53gz.sciprj.gz"));
        const QByteArray gzBytes = suite53ReadAll(tmpDir53.filePath("rpc53gz.sciprj.gz"));
        CHECK_COND(rGz.value("result").toObject().value("compressed").toBool() && gzBytes.size() > 20
                           && static_cast<unsigned char>(gzBytes[0]) == 0x1f
                           && static_cast<unsigned char>(gzBytes[1]) == 0x8b,
                   "53i: compressed RPC save is not gzip data");
        mw->projectname = prj;
    }

    // ---- 53j: a restored recovery copy is claimed by this process; stale orphans are removed
    {
        QStandardPaths::setTestModeEnabled(true);
        qApp->setProperty("torigraph_noninteractive", QVariant());
        const QString recDir = ApplicationWindow::recoveryDirPath();
        QDir(recDir).removeRecursively();
        QDir().mkpath(recDir);
        ApplicationWindow::setCrashRecoveryEnabled(true);
        ApplicationWindow *w53 = new ApplicationWindow();
        w53->applyUserSettings();
        if (ApplicationWindow::isNonInteractiveContext(w53)) {
            std::cout << "[INFO] Suite 53j: window is non-interactive, recovery check skipped\n";
        } else {
            // A small project of its own (the shared test window holds every earlier suite's
            // windows, which would make the restore depend on the order the suites run in).
            const QString smallPrj = tmpDir53.filePath("recover53.sciprj");
            {
                Table *ts = w53->newTable("Rec53", 3, 2);
                CHECK_COND(ts != nullptr, "53j: table");
                ts->setCell(0, 1, 53.0);
                CHECK_COND(w53->saveFolder(w53->projectFolder(), smallPrj), "53j: save small project");
            }
            const QString id = QStringLiteral("suite53-crashed-copy");
            CHECK_COND(QFile::copy(smallPrj, QDir(recDir).filePath(id + ".sciprj")), "53j: copy data");
            QJsonObject meta;
            meta["project"] = smallPrj;
            meta["pid"] = 2147483000.0; // a process that does not exist
            meta["saved_at"] = QDateTime::currentDateTime().addSecs(-3600).toString(Qt::ISODate);
            CHECK_COND(suite59WriteFileEarly(QDir(recDir).filePath(id + ".json"),
                                             QJsonDocument(meta).toJson(QJsonDocument::Compact)),
                       "53j: write meta");
            const QString oldOrphan = QDir(recDir).filePath("orphan-old.sciprj");
            const QString newOrphan = QDir(recDir).filePath("orphan-new.sciprj");
            CHECK_COND(suite59WriteFileEarly(oldOrphan, "OLD") && suite59WriteFileEarly(newOrphan, "NEW"),
                       "53j: write orphans");
            {
                QFile of(oldOrphan);
                CHECK_COND(of.open(QIODevice::ReadWrite)
                                   && of.setFileTime(QDateTime::currentDateTime().addDays(-3),
                                                     QFileDevice::FileModificationTime),
                           "53j: could not age the orphan");
            }
            w53->projectname = QStringLiteral("suite53_start_window.sciprj"); // not closed
            // Answer "Restore" for our copy through the prompt hook instead of looking for the
            // message box by time (the box is built exactly as for the user, only exec() is
            // skipped). Any other copy would be left for later, which the checks below catch.
            QStringList asked;
            ApplicationWindow::setCrashRecoveryPromptOverride(
                    [&asked, id](const QString &metaPath) {
                        asked << QFileInfo(metaPath).completeBaseName();
                        return QFileInfo(metaPath).completeBaseName() == id
                                ? ApplicationWindow::RecoveryRestore
                                : ApplicationWindow::RecoveryDecideLater;
                    });
            w53->checkCrashRecovery();
            ApplicationWindow::setCrashRecoveryPromptOverride(nullptr);
            CHECK_COND(asked == QStringList() << id,
                       "53j: restore question not asked exactly once for the copy (asked: "
                               + asked.join(QStringLiteral(", ")).toStdString() + ")");
            CHECK_COND(!QFileInfo::exists(oldOrphan), "53j: stale .sciprj without .json not removed");
            CHECK_COND(QFileInfo::exists(newOrphan), "53j: recent .sciprj without .json removed");
            const QJsonObject newMeta =
                    QJsonDocument::fromJson(suite53ReadAll(QDir(recDir).filePath(id + ".json"))).object();
            const qint64 pidNow = static_cast<qint64>(newMeta.value("pid").toDouble());
            const QDateTime savedNow =
                    QDateTime::fromString(newMeta.value("saved_at").toString(), Qt::ISODate);
            std::cout << "[INFO] 53j: recovery meta pid " << pidNow << " (this process "
                      << QCoreApplication::applicationPid() << ")\n";
            CHECK_COND(pidNow == QCoreApplication::applicationPid(),
                       "53j: restored copy still names the crashed process");
            CHECK_COND(savedNow.isValid()
                               && std::llabs(savedNow.secsTo(QDateTime::currentDateTime())) < 120,
                       "53j: saved_at not refreshed when the copy was restored");
            // Find the restored window first and close it afterwards. The list of top-level
            // widgets also holds the restored window's own top-level children (its menus), so
            // destroying the window inside this loop left the loop with freed pointers; whether
            // that crashed depended on the (hash) order of the list.
            // The window is recognised by the project it holds (smallPrj lies in this run's own
            // temporary folder). It used to be also required to be missing from a list of the
            // top-level widgets taken before the restore; but windows of earlier suites that were
            // deleted (deleteLater) during the restore free their memory, and the restored window
            // could be created at such an address - it then counted as "already there" and was
            // not found, depending on timing.
            QList<QPointer<ApplicationWindow>> restoredWins;
            for (QWidget *tl : QApplication::topLevelWidgets()) {
                auto *cand = qobject_cast<ApplicationWindow *>(tl);
                if (cand && cand != mw && cand != w53 && cand->projectname == smallPrj)
                    restoredWins << cand;
            }
            if (restoredWins.size() != 1) {
                for (QWidget *tl : QApplication::topLevelWidgets())
                    if (auto *cand = qobject_cast<ApplicationWindow *>(tl))
                        std::cerr << "[INFO] 53j: window " << static_cast<void *>(cand) << " project '"
                                  << cand->projectname.toStdString() << "'\n";
            }
            CHECK_COND(restoredWins.size() == 1, "53j: restored window not found");
            // Close it the way a user does: File > Close / [x], answer "Don't Save" to the
            // question about the modified project. The window deletes itself (WA_DeleteOnClose)
            // and gives up the recovery copy it took over.
            QTimer dontSave;
            dontSave.setInterval(50);
            QObject::connect(&dontSave, &QTimer::timeout, [&dontSave]() {
                if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                    for (QAbstractButton *b : box->buttons()) {
                        if (b != box->defaultButton() && b != box->escapeButton()) {
                            dontSave.stop();
                            b->click();
                            return;
                        }
                    }
                }
            });
            for (const QPointer<ApplicationWindow> &rw : restoredWins) {
                if (!rw)
                    continue;
                CHECK_COND(rw->testAttribute(Qt::WA_DeleteOnClose),
                           "53j: restored window does not delete itself on close");
                dontSave.start();
                const bool closed = rw->close();
                dontSave.stop();
                CHECK_COND(closed, "53j: restored window refused to close");
            }
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QApplication::processEvents();
            for (const QPointer<ApplicationWindow> &rw : restoredWins)
                CHECK_COND(rw.isNull(), "53j: closed restored window was not deleted");
            CHECK_COND(!QFileInfo::exists(QDir(recDir).filePath(id + ".json"))
                               && !QFileInfo::exists(QDir(recDir).filePath(id + ".sciprj")),
                       "53j: closing the restored window kept the recovery copy");
        }
        w53->savedProject();
        w53->setOleEmbeddedMode(true);
        delete w53;
        ApplicationWindow::setCrashRecoveryEnabled(false);
        QDir(recDir).removeRecursively();
        QStandardPaths::setTestModeEnabled(false);
    }

    // ---- 53k: the real "Restore unsaved work?" box (no prompt hook): the button the user
    //      clicks decides what happens to the copy. "Decide Later" keeps it, "Discard Copy"
    //      removes it. ("Restore" is followed through in 53j.) The box is answered by a timer
    //      that keeps looking for it until it is up (bounded), inside this process only.
    {
        qApp->setProperty("torigraph_noninteractive", QVariant());
        const QString recDir = ApplicationWindow::recoveryDirPath();
        QDir(recDir).removeRecursively();
        QDir().mkpath(recDir);
        ApplicationWindow::setCrashRecoveryEnabled(true);
        ApplicationWindow *w53k = new ApplicationWindow();
        w53k->applyUserSettings();
        if (ApplicationWindow::isNonInteractiveContext(w53k)) {
            std::cout << "[INFO] Suite 53k: window is non-interactive, recovery box not shown\n";
        } else {
            w53k->projectname = QStringLiteral("suite53k_start_window.sciprj"); // not closed
            const QString id = QStringLiteral("suite53k-copy");
            const QString dataPath = QDir(recDir).filePath(id + ".sciprj");
            const QString metaPath = QDir(recDir).filePath(id + ".json");
            QJsonObject meta;
            meta["project"] = tmpDir53.filePath("suite53k.sciprj");
            meta["pid"] = 2147483000.0; // a process that does not exist
            meta["saved_at"] = QDateTime::currentDateTime().addSecs(-600).toString(Qt::ISODate);
            CHECK_COND(suite59WriteFileEarly(dataPath, "not opened in this test")
                               && suite59WriteFileEarly(metaPath,
                                                        QJsonDocument(meta).toJson(QJsonDocument::Compact)),
                       "53k: write recovery copy");

            auto answerRealBox = [&](QMessageBox::ButtonRole role, int *boxesSeen) -> bool {
                bool clicked = false;
                int tries = 0;
                QTimer probe;
                QObject::connect(&probe, &QTimer::timeout, [&]() {
                    if (++tries > 500) { // 10 s
                        probe.stop();
                        return;
                    }
                    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                    if (!box)
                        return;
                    for (QAbstractButton *b : box->buttons()) {
                        if (box->buttonRole(b) == role) {
                            probe.stop();
                            ++*boxesSeen;
                            clicked = true;
                            b->click();
                            return;
                        }
                    }
                });
                probe.start(20);
                w53k->checkCrashRecovery();
                probe.stop();
                return clicked;
            };

            int boxes = 0;
            CHECK_COND(answerRealBox(QMessageBox::RejectRole, &boxes) && boxes == 1,
                       "53k: the restore box was not shown / 'Decide Later' not found");
            CHECK_COND(QFileInfo::exists(dataPath) && QFileInfo::exists(metaPath),
                       "53k: 'Decide Later' removed the recovery copy");
            boxes = 0;
            CHECK_COND(answerRealBox(QMessageBox::DestructiveRole, &boxes) && boxes == 1,
                       "53k: the restore box was not shown again / 'Discard Copy' not found");
            CHECK_COND(!QFileInfo::exists(dataPath) && !QFileInfo::exists(metaPath),
                       "53k: 'Discard Copy' kept the recovery copy");
            boxes = 0;
            CHECK_COND(!answerRealBox(QMessageBox::RejectRole, &boxes) && boxes == 0,
                       "53k: the box was shown again although the copy was discarded");
        }
        w53k->savedProject();
        w53k->setOleEmbeddedMode(true);
        delete w53k;
        ApplicationWindow::setCrashRecoveryEnabled(false);
        QDir(recDir).removeRecursively();
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 53 Save Safety, Undo-after-Save & Auto-save Recovery Gate passed!\n";
    return 0;
}

// =============================================================================
// Suite 55: Tensile & work-hardening analysis correctness (review items F2, F3,
// F4, F8 and F10). Synthetic curves with known E and sigma_0.2:
//   - Ramberg-Osgood eps = sigma/E + 0.002 (sigma/sigma0)^n, E = 70 GPa,
//     sigma0 = 400 MPa (so sigma_0.2 = 400 MPa exactly), n = 10, UTS = 500 MPa
//     -> e_u = 100 (500/70000 + 0.002 * 1.25^10) = 2.57693 %, plastic 1.86265 %
//     -> after UTS: (e_u + 0.5 %, 480), (e_u + 1.0 %, 450), then a drop to 10 MPa,
//        so e_f = 3.57693 %, plastic e_f = 3.57693 - 100 * 450 / 70000 = 2.93407 %
//   - the same curve in mm/mm without labels (unit decided by E plausibility)
//   - an H-charged-like curve in % whose elongation is below 1.2 % (old heuristic failed)
//   - 5000 points in the elastic range (old 250-point window limit fell back to 3 points)
//   - a 0.15 % toe in front of the curve (x0 must enter elongations and true strain)
//   - Hollomon sigma_t = 700 eps_p^0.12 (fraction) for the Considere point, with
//     serrations, equal-strain duplicates and post-necking rows
// =============================================================================
int runSuite55TensileAndWorkHardeningCorrectness(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 55: Tensile & Work-Hardening Analysis Correctness (F2/F3/F4/F8/F10)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 55");
    qApp->setProperty("torigraph_noninteractive", true);
    const double qnan = std::numeric_limits<double>::quiet_NaN();

    auto makeGraph = [&](const QString &name, const QVector<double> &xs, const QVector<double> &ys,
                         const QString &xUnit, const QString &xLong, Table **outTbl = nullptr) -> Graph * {
        Table *t = mw->newTable(xs.size(), 2, name, name);
        if (!t)
            return nullptr;
        t->column(0)->setLongName(xLong);
        t->column(0)->setUnits(xUnit);
        t->column(1)->setLongName(QStringLiteral("Stress"));
        t->column(1)->setUnits(QStringLiteral("MPa"));
        for (int i = 0; i < xs.size(); ++i) {
            t->setCell(i, 0, xs[i]);
            t->setCell(i, 1, ys[i]);
        }
        if (outTbl)
            *outTbl = t;
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (name + QStringLiteral("_2")), Graph::Line);
        return (ml && ml->activeGraph()) ? ml->activeGraph() : nullptr;
    };

    // Ramberg-Osgood reference curve (strain in %)
    const double E = 70000.0, sig0 = 400.0;
    auto roStrainPct = [&](double s) { return 100.0 * (s / E + 0.002 * std::pow(s / sig0, 10.0)); };
    const double euPct = roStrainPct(500.0);                 // 2.57693 %
    const double euPlPct = euPct - 100.0 * 500.0 / E;        // 1.86265 %
    const double efPct = euPct + 1.0;                        // 3.57693 %
    const double efPlPct = efPct - 100.0 * 450.0 / E;        // 2.93407 %
    QVector<double> xA, yA;
    for (int i = 0; i <= 250; ++i) {
        const double s = 2.0 * i;
        xA << roStrainPct(s);
        yA << s;
    }
    xA << euPct + 0.5 << euPct + 1.0 << euPct + 1.2;
    yA << 480.0 << 450.0 << 10.0;
    std::cout << "[INFO] Suite 55 reference: e_u=" << euPct << "% (plastic " << euPlPct
              << "%), e_f=" << efPct << "% (plastic " << efPlPct << "%)\n";

    // ---- 55a: % data labelled "%": E, sigma_0.2, elongations, band, unit source
    {
        Graph *gA = makeGraph(QStringLiteral("Ten55A"), xA, yA, QStringLiteral("%"), QStringLiteral("Strain"));
        CHECK_COND(gA != nullptr, "55a: graph not created");
        const int bandsBefore = gA->shadedBandCount();
        const CurveMathTools::TensileResult r = CurveMathTools::analyzeTensile(
                mw, gA, QString(), 0.002, -1, qnan, qnan, true, true, false);
        std::cout << "[INFO] 55a: valid=" << r.valid << " E=" << r.youngsModulus << " Rp0.2=" << r.offsetYieldY
                  << " eu=" << r.uniformElongationPct << " euPl=" << r.uniformPlasticElongationPct
                  << " ef=" << r.fractureElongationPct << " efPl=" << r.fracturePlasticElongationPct
                  << " src=" << r.strainUnitSource.toStdString() << " elastic=[" << r.elasticXMin << ","
                  << r.elasticXMax << "]\n";
        CHECK_COND(r.valid && r.strainInPercent && r.strainUnitSource == QLatin1String("column")
                           && !r.strainUnitAmbiguous,
                   "55a: % column unit not used");
        CHECK_COND(std::fabs(r.youngsModulus - E) < 0.005 * E, "55a: E deviates from 70 GPa");
        CHECK_COND(std::fabs(r.offsetYieldY - 400.0) < 2.0, "55a: sigma_0.2 deviates from 400 MPa");
        CHECK_COND(std::fabs(r.uniformElongationPct - euPct) < 0.01
                           && std::fabs(r.uniformPlasticElongationPct - euPlPct) < 0.02,
                   "55a: uniform elongation / plastic part wrong");
        CHECK_COND(std::fabs(r.fractureElongationPct - efPct) < 0.01
                           && std::fabs(r.fracturePlasticElongationPct - efPlPct) < 0.02,
                   "55a: fracture elongation / plastic part wrong");
        // F3: the elastic window lies inside 10-60 % of UTS (50-300 MPa) and is shown as a band
        CHECK_COND(r.elasticXMin >= roStrainPct(50.0) - 1e-9 && r.elasticXMax <= roStrainPct(300.0) + 1e-9
                           && (r.elasticXMax - r.elasticXMin) >= roStrainPct(75.0) - roStrainPct(0.0) - 1e-9,
                   "55a: elastic window outside 10-60 % of UTS or narrower than 15 % of UTS");
        CHECK_COND(gA->shadedBandCount() == bandsBefore + 1, "55a: elastic range band not drawn");
        CHECK_COND(r.warnings.isEmpty(), "55a: unexpected warnings on a clean curve");
    }

    // ---- 55b: same curve in mm/mm without any unit label -> decided by E plausibility
    {
        QVector<double> xB;
        for (double v : xA)
            xB << v / 100.0;
        Graph *gB = makeGraph(QStringLiteral("Ten55B"), xB, yA, QString(), QStringLiteral("Strain"));
        CHECK_COND(gB != nullptr, "55b: graph not created");
        const CurveMathTools::TensileResult r = CurveMathTools::analyzeTensile(
                mw, gB, QString(), 0.002, -1, qnan, qnan, false, false, false);
        std::cout << "[INFO] 55b: valid=" << r.valid << " pct=" << r.strainInPercent << " E=" << r.youngsModulus
                  << " Rp0.2=" << r.offsetYieldY << " src=" << r.strainUnitSource.toStdString() << "\n";
        CHECK_COND(r.valid && !r.strainInPercent && r.strainUnitSource == QLatin1String("E range")
                           && !r.strainUnitAmbiguous,
                   "55b: mm/mm data not recognised from E plausibility");
        CHECK_COND(std::fabs(r.youngsModulus - E) < 0.005 * E && std::fabs(r.offsetYieldY - 400.0) < 2.0,
                   "55b: E or sigma_0.2 wrong for mm/mm data");
        CHECK_COND(std::fabs(r.uniformElongationPct - euPct) < 0.01
                           && std::fabs(r.fractureElongationPct - efPct) < 0.01,
                   "55b: elongations (in %) wrong for mm/mm data");
    }

    // ---- 55c: % data, elongation < 1.2 %, no unit label (old rule maxX > 1.2 said mm/mm)
    {
        QVector<double> xC, yC;
        for (int i = 0; i <= 210; ++i) {
            const double s = 2.0 * i; // up to 420 MPa (UTS)
            xC << roStrainPct(s);
            yC << s;
        }
        xC << roStrainPct(420.0) + 0.01;
        yC << 5.0;
        CHECK_COND(xC.back() < 1.2, "55c: test curve must stay below 1.2 %");
        Graph *gC = makeGraph(QStringLiteral("Ten55C"), xC, yC, QString(), QStringLiteral("Strain"));
        CHECK_COND(gC != nullptr, "55c: graph not created");
        const CurveMathTools::TensileResult r = CurveMathTools::analyzeTensile(
                mw, gC, QString(), 0.002, -1, qnan, qnan, false, false, false);
        std::cout << "[INFO] 55c: valid=" << r.valid << " pct=" << r.strainInPercent << " E=" << r.youngsModulus
                  << " Rp0.2=" << r.offsetYieldY << " eu=" << r.uniformElongationPct << "\n";
        CHECK_COND(r.valid && r.strainInPercent && r.strainUnitSource == QLatin1String("E range"),
                   "55c: small-elongation % data misread as mm/mm");
        CHECK_COND(std::fabs(r.youngsModulus - E) < 0.005 * E && std::fabs(r.offsetYieldY - 400.0) < 2.0,
                   "55c: E or sigma_0.2 wrong for small-elongation % data");
        CHECK_COND(std::fabs(r.uniformElongationPct - roStrainPct(420.0)) < 0.01,
                   "55c: uniform elongation wrong");

        // F4: work hardening follows the tensile settings that are passed in
        const CurveMathTools::WorkHardeningResult whPct = CurveMathTools::analyzeWorkHardening(
                mw, gC, QString(), 11, false, false, 1, 0.002);
        CHECK_COND(whPct.valid && whPct.strainInPercent && std::fabs(whPct.youngsModulus - E) < 0.005 * E,
                   "55c: work hardening did not take strain unit % from the settings");
        const CurveMathTools::WorkHardeningResult whFrac = CurveMathTools::analyzeWorkHardening(
                mw, gC, QString(), 11, false, false, 0, 0.002);
        std::cout << "[INFO] 55c: WH(mm/mm) valid=" << whFrac.valid << " pct=" << whFrac.strainInPercent
                  << " E=" << whFrac.youngsModulus << " err='" << whFrac.error.toStdString() << "'\n";
        CHECK_COND(!whFrac.valid || (!whFrac.strainInPercent && std::fabs(whFrac.youngsModulus - E / 100.0) < 0.01 * E / 100.0),
                   "55c: work hardening ignored the mm/mm setting");
    }

    // ---- 55d: ambiguous units are flagged (label contradicts E; neither reading plausible)
    {
        QVector<double> xB;
        for (double v : xA)
            xB << v / 100.0;
        Graph *gD = makeGraph(QStringLiteral("Ten55D"), xB, yA, QStringLiteral("%"), QStringLiteral("Strain"));
        CHECK_COND(gD != nullptr, "55d: graph not created");
        const CurveMathTools::TensileResult r = CurveMathTools::analyzeTensile(
                mw, gD, QString(), 0.002, -1, qnan, qnan, false, false, false);
        CHECK_COND(r.strainInPercent && r.strainUnitAmbiguous && !r.warnings.isEmpty()
                           && std::fabs(r.youngsModulusIfFractionGPa - 70.0) < 0.5,
                   "55d: '%' label on mm/mm data (E = 7000 GPa) not flagged");

        QVector<double> xP, yP; // polymer-like, E = 2 GPa if %, 0.02 GPa if mm/mm
        for (int i = 0; i <= 40; ++i) {
            xP << 0.05 * i;
            yP << 20.0 * 0.05 * i;
        }
        for (int i = 1; i <= 60; ++i) {
            xP << 2.0 + 0.1 * i;
            yP << 40.0 + (10.0 / 6.0) * (0.1 * i);
        }
        xP << 8.5;
        yP << 2.0;
        Graph *gP = makeGraph(QStringLiteral("Ten55P"), xP, yP, QString(), QStringLiteral("Strain"));
        CHECK_COND(gP != nullptr, "55d: polymer graph not created");
        const CurveMathTools::TensileResult rp = CurveMathTools::analyzeTensile(
                mw, gP, QString(), 0.002, -1, qnan, qnan, false, false, false);
        std::cout << "[INFO] 55d: polymer pct=" << rp.strainInPercent << " ambiguous=" << rp.strainUnitAmbiguous
                  << " src=" << rp.strainUnitSource.toStdString() << "\n";
        CHECK_COND(rp.strainUnitAmbiguous && rp.strainUnitSource == QLatin1String("max strain")
                           && !rp.warnings.isEmpty(),
                   "55d: unit with E outside 5-1000 GPa either way not flagged as ambiguous");
        const CurveMathTools::TensileResult rpUser = CurveMathTools::analyzeTensile(
                mw, gP, QString(), 0.002, 1, qnan, qnan, false, false, false);
        CHECK_COND(!rpUser.strainUnitAmbiguous && rpUser.strainUnitSource == QLatin1String("user"),
                   "55d: explicit unit must not be flagged");
    }

    // ---- 55e: 5000 points in the elastic range (E = 200 GPa, +/-0.5 MPa noise)
    {
        QVector<double> xE, yE;
        for (int i = 0; i < 5000; ++i) {
            const double s = 400.0 * i / 4999.0;
            xE << s / 2000.0; // % (E = 2000 MPa/%)
            yE << ((i == 0) ? 0.0 : s + 0.5 * std::sin(1.3 * i));
        }
        const double H = 100.0 / 9.8; // MPa/% linear hardening from (0.2 %, 400) to (10 %, 500)
        for (int k = 1; k <= 200; ++k) {
            const double e = 0.2 + 9.8 * k / 200.0;
            xE << e;
            yE << 400.0 + H * (e - 0.2);
        }
        // Offset line 2000 (e - 0.2) meets 400 + H (e - 0.2) at e - 0.2 = 400 / (2000 - H)
        const double rpExpected = 2000.0 * 400.0 / (2000.0 - H); // 402.05 MPa
        Graph *gE = makeGraph(QStringLiteral("Ten55E"), xE, yE, QStringLiteral("%"), QStringLiteral("Strain"));
        CHECK_COND(gE != nullptr, "55e: graph not created");
        const CurveMathTools::TensileResult r = CurveMathTools::analyzeTensile(
                mw, gE, QString(), 0.002, -1, qnan, qnan, false, false, false);
        std::cout << "[INFO] 55e: valid=" << r.valid << " E=" << r.youngsModulus << " R2=" << r.elasticR2
                  << " Rp0.2=" << r.offsetYieldY << " (expected " << rpExpected << ") elastic=["
                  << r.elasticXMin << "," << r.elasticXMax << "] relaxed=" << r.elasticAutoRelaxed << "\n";
        CHECK_COND(r.valid && !r.elasticAutoRelaxed && std::fabs(r.youngsModulus - 200000.0) < 0.005 * 200000.0,
                   "55e: E from 5000-point elastic range deviates from 200 GPa");
        CHECK_COND(std::fabs(r.offsetYieldY - rpExpected) < 1.0, "55e: sigma_0.2 deviates from 402.05 MPa");
        CHECK_COND(r.elasticXMax - r.elasticXMin >= 0.15 * 500.0 / 2000.0 - 1e-6,
                   "55e: elastic window narrower than 15 % of UTS");

        // F4: linear hardening -> theta never meets sigma_t: Considere falls back to UTS with a
        // reason, and sigma_s is NaN with a reason (no 1.08 x sigma substitute)
        const CurveMathTools::WorkHardeningResult wh = CurveMathTools::analyzeWorkHardening(
                mw, gE, QString(), 11, false, false, -1, 0.002);
        std::cout << "[INFO] 55e: WH valid=" << wh.valid << " fromUts=" << wh.considereFromUts
                  << " sigma_s=" << wh.voceSigmaS << " note='" << wh.voceNote.toStdString() << "'\n";
        CHECK_COND(wh.valid && wh.considereFromUts && !wh.considereNote.isEmpty(),
                   "55e: missing Considere crossing not reported");
        CHECK_COND(!std::isfinite(wh.voceSigmaS) && !wh.voceNote.isEmpty(),
                   "55e: sigma_s must be NaN with a reason when Stage III does not fit");
    }

    // ---- 55f: 0.15 % toe in front of the curve
    {
        const double x0 = 0.15;
        QVector<double> xF, yF;
        for (int i = 0; i < 30; ++i) { // sigma = E' x^2 / (4 x0) for x < 2 x0, E' = 700 MPa/%
            const double x = 0.01 * i;
            xF << x;
            yF << 700.0 * x * x / (4.0 * x0);
        }
        for (int i = 0; i <= 250; ++i) {
            const double s = 105.0 + 395.0 * i / 250.0;
            xF << x0 + roStrainPct(s);
            yF << s;
        }
        xF << x0 + euPct + 0.5 << x0 + euPct + 1.0 << x0 + euPct + 1.2;
        yF << 480.0 << 450.0 << 10.0;
        Graph *gF = makeGraph(QStringLiteral("Ten55F"), xF, yF, QStringLiteral("%"), QStringLiteral("Strain"));
        CHECK_COND(gF != nullptr, "55f: graph not created");
        const CurveMathTools::TensileResult r = CurveMathTools::analyzeTensile(
                mw, gF, QString(), 0.002, -1, qnan, qnan, false, false, true);
        std::cout << "[INFO] 55f: valid=" << r.valid << " x0=" << r.toeStrainX0 << " E=" << r.youngsModulus
                  << " Rp0.2=" << r.offsetYieldY << " eu=" << r.uniformElongationPct << "\n";
        CHECK_COND(r.valid && std::fabs(r.toeStrainX0 - x0) < 0.005, "55f: toe origin x0 not found");
        CHECK_COND(std::fabs(r.offsetYieldY - 400.0) < 2.0 && std::fabs(r.youngsModulus - E) < 0.005 * E,
                   "55f: E or sigma_0.2 wrong with toe");
        CHECK_COND(std::fabs(r.uniformElongationPct - euPct) < 0.01
                           && std::fabs(r.fractureElongationPct - efPct) < 0.01,
                   "55f: elongations not measured from the toe-corrected origin");
        CHECK_COND(r.trueStressTable != nullptr && r.trueStressTable->numRows() > 10,
                   "55f: true stress table missing");
        const int lastRow = r.trueStressTable->numRows() - 1;
        const double epsTLast = r.trueStressTable->column(0)->valueAt(lastRow);
        const double epsTFirst = r.trueStressTable->column(0)->valueAt(0);
        std::cout << "[INFO] 55f: true strain first=" << epsTFirst << " last=" << epsTLast
                  << " expected last=" << std::log1p(euPct / 100.0) << "\n";
        CHECK_COND(epsTFirst >= 0.0 && std::fabs(epsTLast - std::log1p(euPct / 100.0)) < 2e-4,
                   "55f: true strain does not include the toe correction");

        const CurveMathTools::WorkHardeningResult wh = CurveMathTools::analyzeWorkHardening(
                mw, gF, QString(), 11, false, false, -1, 0.002);
        CHECK_COND(wh.valid && std::fabs(wh.toeStrainX0 - x0) < 0.005,
                   "55f: work hardening did not use the toe-corrected origin");
    }

    // ---- 55g: F8 exclusions are counted and reported
    {
        Table *tG = nullptr;
        Graph *gG = makeGraph(QStringLiteral("Ten55G"), xA, yA, QStringLiteral("%"), QStringLiteral("Strain"), &tG);
        CHECK_COND(gG != nullptr && tG != nullptr, "55g: graph not created");
        tG->column(1)->setMasked(200, true);
        tG->clearCell(210, 1);
        gG->updatePlot();
        const CurveMathTools::TensileResult r = CurveMathTools::analyzeTensile(
                mw, gG, QString(), 0.002, -1, qnan, qnan, false, false, false);
        std::cout << "[INFO] 55g: masked=" << r.excludedMaskedPoints << " nonfinite=" << r.excludedNonFinitePoints
                  << " warnings=" << r.warnings.size() << "\n";
        CHECK_COND(r.valid && r.excludedMaskedPoints == 1 && r.excludedNonFinitePoints == 1 && !r.warnings.isEmpty(),
                   "55g: excluded points not counted/reported");
        CHECK_COND(std::fabs(r.offsetYieldY - 400.0) < 2.0, "55g: sigma_0.2 changed by exclusions");
    }

    // ---- 55h: Considere point on a Hollomon curve (mm/mm), clean and serrated
    {
        const double K = 700.0, n = 0.12;
        struct HPt { double e; double s; double epsT; };
        std::vector<HPt> hp;
        auto pushSig = [&](double st, double ep) {
            const double epsT = st / E + ep;
            const double e = std::expm1(epsT);
            hp.push_back({ e, st / (1.0 + e), epsT });
        };
        for (int j = 0; j < 60; ++j) {
            const double st = 232.0 * j / 60.0;
            pushSig(st, std::pow(st / K, 1.0 / n));
        }
        for (int j = 0; j < 4000; ++j) {
            const double ep = 1e-4 + (0.20 - 1e-4) * j / 3999.0;
            pushSig(K * std::pow(ep, n), ep);
        }
        size_t iMax = 0;
        for (size_t i = 0; i < hp.size(); ++i)
            if (hp[i].s > hp[iMax].s)
                iMax = i;
        const double epsTuExpected = hp[iMax].epsT;

        QVector<double> xH, yH;
        for (const auto &p : hp) {
            xH << p.e;
            yH << p.s;
        }
        Graph *gH = makeGraph(QStringLiteral("Ten55H"), xH, yH, QStringLiteral("-"), QStringLiteral("Strain"));
        CHECK_COND(gH != nullptr, "55h: graph not created");
        const CurveMathTools::WorkHardeningResult wh = CurveMathTools::analyzeWorkHardening(
                mw, gH, QString(), 11, false, false, -1, 0.002);
        std::cout << "[INFO] 55h: valid=" << wh.valid << " eps_u=" << wh.considereTrueStrain << " (max load at "
                  << epsTuExpected << ") fromUts=" << wh.considereFromUts << " masked=" << wh.postNeckRowsMasked
                  << " n=" << wh.hollomonN << "\n";
        CHECK_COND(wh.valid && !wh.strainInPercent && !wh.considereFromUts
                           && std::fabs(wh.considereTrueStrain - epsTuExpected) < 0.004,
                   "55h: Considere point does not match the maximum load on a Hollomon curve");
        CHECK_COND(std::fabs(wh.hollomonN - n) < 0.01, "55h: Hollomon n deviates from 0.12");

        // theta accuracy (scaled normal equations) at eps_t ~ 0.06 against the analytic value
        Table *tbl = wh.resultTable;
        CHECK_COND(tbl != nullptr && tbl->numRows() > 100, "55h: work-hardening table missing");
        int rowBest = 0;
        for (int rr = 0; rr < tbl->numRows(); ++rr) {
            if (std::fabs(tbl->column(0)->valueAt(rr) - 0.06) < std::fabs(tbl->column(0)->valueAt(rowBest) - 0.06))
                rowBest = rr;
        }
        const double sT = tbl->column(1)->valueAt(rowBest);
        const double epP = std::pow(sT / K, 1.0 / n);
        const double dSdEp = n * sT / epP;
        const double thetaExact = dSdEp / (1.0 + dSdEp / E);
        const double thetaTbl = tbl->column(2)->valueAt(rowBest);
        std::cout << "[INFO] 55h: theta(" << tbl->column(0)->valueAt(rowBest) << ")=" << thetaTbl
                  << " exact=" << thetaExact << "\n";
        CHECK_COND(std::fabs(thetaTbl - thetaExact) < 0.01 * thetaExact, "55h: theta deviates > 1 % from analytic");

        // post-necking rows are masked, earlier rows are not
        CHECK_COND(wh.postNeckRowsMasked > 0 && tbl->column(1)->isMasked(tbl->numRows() - 1)
                           && !tbl->column(1)->isMasked(rowBest),
                   "55h: post-necking rows not masked");

        // Serrations (one-point load drops of 15 MPa) between eps_p 0.03 and 0.07 and equal-strain
        // duplicate readings must not move the Considere point
        QVector<double> xS, yS;
        int cntSer = 0;
        for (size_t i = 0; i < hp.size(); ++i) {
            const double ep = hp[i].epsT - hp[i].s * (1.0 + hp[i].e) / E;
            double s = hp[i].s;
            if (ep > 0.03 && ep < 0.07 && (i % 40) == 0) {
                s -= 15.0;
                ++cntSer;
            }
            xS << hp[i].e;
            yS << s;
            if (i > 100 && (i % 100) == 0) { // duplicate reading at the same strain
                xS << hp[i].e;
                yS << s + 1.0;
            }
        }
        Graph *gS = makeGraph(QStringLiteral("Ten55S"), xS, yS, QStringLiteral("-"), QStringLiteral("Strain"));
        CHECK_COND(gS != nullptr, "55h: serrated graph not created");
        const CurveMathTools::WorkHardeningResult whS = CurveMathTools::analyzeWorkHardening(
                mw, gS, QString(), 11, false, false, -1, 0.002);
        std::cout << "[INFO] 55h: serrated (" << cntSer << " drops) eps_u=" << whS.considereTrueStrain
                  << " fromUts=" << whS.considereFromUts << " merged=" << whS.mergedDuplicatePoints
                  << " note='" << whS.considereNote.toStdString() << "'\n";
        CHECK_COND(whS.valid && !whS.considereFromUts && std::fabs(whS.considereTrueStrain - epsTuExpected) < 0.005,
                   "55h: serrations moved the Considere point (early crossing accepted)");
        CHECK_COND(whS.mergedDuplicatePoints > 0, "55h: equal-strain readings were not merged");
    }

    // ---- 55i: the work-hardening plot is still produced with a NaN sigma_s
    {
        Table *tSample = nullptr;
        MultiLayer *mlS = CurveMathTools::loadSampleDataset(mw, "tensile_alznmg", true, &tSample, nullptr);
        CHECK_COND(mlS != nullptr && mlS->activeGraph() != nullptr, "55i: sample not loaded");
        const CurveMathTools::WorkHardeningResult wh = CurveMathTools::analyzeWorkHardening(
                mw, mlS->activeGraph(), QString(), 11, true, true, -1, 0.002);
        std::cout << "[INFO] 55i: AA7075 eps_u=" << wh.considereTrueStrain << " sigma_u=" << wh.considereTrueStress
                  << " sigma_s=" << wh.voceSigmaS << " fromUts=" << wh.considereFromUts << "\n";
        CHECK_COND(wh.valid && wh.plot != nullptr && wh.considereTrueStrain > 0.05 && wh.considereTrueStrain < 0.20,
                   "55i: AA7075 sample work-hardening analysis failed");
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 55 Tensile & Work-Hardening Analysis Correctness passed!\n";
    return 0;
}

// =============================================================================
// Suite 57: Numerical robustness of smoothing / derivative / ROI integral /
// normalisation, XRD per-anode Ka2 stripping & Williamson-Hall physicality,
// Weibull (method, ln L, JIS R 1625 bias correction), Arrhenius / Kissinger
// exclusion reporting and Kissinger input from peak-fit centres.
// All checks use synthetic data with known solutions.
// =============================================================================
namespace {

Table *suite57Table(ApplicationWindow *mw, const QString &name, const QStringList &header,
                    const std::vector<std::vector<double>> &cols)
{
    const int nRows = cols.empty() ? 0 : int(cols.front().size());
    Table *t = mw->newTable(name, nRows, int(header.size()));
    if (!t)
        return nullptr;
    t->setHeader(header);
    for (int c = 0; c < int(cols.size()); ++c) {
        t->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        for (int r = 0; r < nRows; ++r)
            t->column(c)->setValueAt(r, cols[size_t(c)][size_t(r)]);
    }
    return t;
}

Graph *suite57Plot(ApplicationWindow *mw, Table *t, const QString &yCol, int type = Graph::Line)
{
    MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_" + yCol), type);
    return ml ? ml->activeGraph() : nullptr;
}

// Synthetic fcc diffractogram, Gaussian profiles, FWHM(2theta) from beta cos(theta) = f(4 sin(theta))
template <typename BetaCosFn>
std::vector<double> suite57XrdPattern(const std::vector<double> &tt, double a0,
                                      const std::vector<std::pair<double, double>> &lines,
                                      BetaCosFn betaCos)
{
    struct Refl {
        int h, k, l;
        double I;
    };
    const Refl refl[] = { { 1, 1, 1, 1000.0 }, { 2, 0, 0, 480.0 }, { 2, 2, 0, 300.0 },
                          { 3, 1, 1, 320.0 },  { 2, 2, 2, 110.0 }, { 4, 0, 0, 80.0 },
                          { 3, 3, 1, 120.0 },  { 4, 2, 0, 110.0 } };
    std::vector<double> y(tt.size(), 0.0);
    for (size_t i = 0; i < tt.size(); ++i)
        y[i] = 40.0 + 0.05 * (tt[i] - tt.front());
    for (const Refl &rf : refl) {
        const double d = a0 / std::sqrt(double(rf.h * rf.h + rf.k * rf.k + rf.l * rf.l));
        for (const auto &ln : lines) {
            const double s = ln.first / (2.0 * d);
            if (s >= 0.999)
                continue;
            const double th = std::asin(s);
            const double tt0 = 2.0 * th * 180.0 / M_PI;
            const double fwDeg = betaCos(4.0 * std::sin(th)) / std::cos(th) * 180.0 / M_PI;
            for (size_t i = 0; i < tt.size(); ++i) {
                const double u = (tt[i] - tt0) / fwDeg;
                if (std::fabs(u) < 8.0)
                    y[i] += rf.I * ln.second * std::exp(-4.0 * M_LN2 * u * u);
            }
        }
    }
    return y;
}

} // namespace

int runSuite57NumericalRobustnessXrdWeibullKissinger(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 57: Smoothing/Derivative/ROI/Normalize robustness, XRD Ka2 & W-H, "
                 "Weibull/Arrhenius/Kissinger reporting...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 57");
    qApp->setProperty("torigraph_noninteractive", true);

    // ---------------------------------------------------------------- 57a: anode table
    {
        CurveMathTools::XRayAnodeLines al;
        bool avg = true;
        CHECK_COND(CurveMathTools::matchXRayAnode(1.540598, &al, &avg) && al.anode == "Cu" && !avg
                           && std::fabs(al.kAlpha2 - 1.544426) < 1e-9,
                   "57a: Cu Ka1 not matched");
        CHECK_COND(CurveMathTools::matchXRayAnode(1.541838, &al, &avg) && al.anode == "Cu" && avg,
                   "57a: Cu Ka average not matched");
        CHECK_COND(CurveMathTools::matchXRayAnode(1.788965, &al, &avg) && al.anode == "Co"
                           && std::fabs(al.kAlpha2 - 1.792850) < 1e-9,
                   "57a: Co Ka1 not matched");
        CHECK_COND(CurveMathTools::matchXRayAnode(1.936040, &al, &avg) && al.anode == "Fe",
                   "57a: Fe Ka1 (dialog value) not matched");
        CHECK_COND(CurveMathTools::matchXRayAnode(2.2897, &al, &avg) && al.anode == "Cr",
                   "57a: Cr Ka1 not matched");
        CHECK_COND(CurveMathTools::matchXRayAnode(0.709319, &al, &avg) && al.anode == "Mo",
                   "57a: Mo Ka1 not matched");
        CHECK_COND(!CurveMathTools::matchXRayAnode(1.6, &al, &avg), "57a: 1.6 A wrongly matched");
    }

    // ---------------------------------------------------------------- 57b: Co tube, Ka2 stripping
    {
        const double lam1 = 1.788965, lam2 = 1.792850, a0 = 4.0495, Dnm = 40.0, eps = 1.5e-3;
        const double K = 0.9;
        std::vector<double> tt;
        for (double v = 35.0; v <= 130.0 + 1e-9; v += 0.01)
            tt.push_back(v);
        auto betaCos = [&](double x4s) { return K * lam1 / (10.0 * Dnm) + eps * x4s; };
        const std::vector<double> y = suite57XrdPattern(tt, a0, { { lam1, 1.0 }, { lam2, 0.5 } }, betaCos);
        Table *t = suite57Table(mw, "S57CoXrd", QStringList() << "TwoTheta" << "I", { tt, y });
        CHECK_COND(t != nullptr, "57b: table");
        Graph *g = suite57Plot(mw, t, "I");
        CHECK_COND(g != nullptr, "57b: plot");

        const CurveMathTools::XRDAnalysisResult xs = CurveMathTools::analyzeXRD(
                mw, g, QString(), lam1, 0.0, K, QStringLiteral("fcc"), true, false, false);
        const CurveMathTools::XRDAnalysisResult xn = CurveMathTools::analyzeXRD(
                mw, g, QString(), lam1, 0.0, K, QStringLiteral("fcc"), false, false, false);
        std::cout << "[INFO] 57b Co strip: peaks=" << xs.peaks.size() << " a=" << xs.latticeParamAngstrom
                  << " D_WH=" << xs.whSizeNm << " eps=" << xs.whMicrostrain << " (no strip: D_WH="
                  << xn.whSizeNm << " eps=" << xn.whMicrostrain << ")\n";
        CHECK_COND(xs.valid && xs.anodeName == "Co" && xs.kAlpha2Stripped
                           && std::fabs(xs.kAlpha2Angstrom - lam2) < 1e-9,
                   "57b: Co Ka2 stripping not applied with the Co wavelength");
        CHECK_COND(xs.peaks.size() >= 5 && std::fabs(xs.latticeParamAngstrom - a0) < 0.003,
                   "57b: lattice parameter after Co Ka2 stripping is off");
        CHECK_COND(xs.whPhysical && std::fabs(xs.whSizeNm - Dnm) / Dnm < 0.12
                           && std::fabs(xs.whMicrostrain - eps) / eps < 0.15,
                   "57b: W-H size / strain not recovered after Co Ka2 stripping");
        // stripping must bring eps closer to the truth than leaving the doublet in
        CHECK_COND(std::isfinite(xn.whMicrostrain) == false
                           || std::fabs(xs.whMicrostrain - eps) < std::fabs(xn.whMicrostrain - eps),
                   "57b: Ka2 stripping did not improve the W-H strain");

        const CurveMathTools::XRDAnalysisResult xu = CurveMathTools::analyzeXRD(
                mw, g, QString(), 1.6, 0.0, K, QStringLiteral("none"), true, false, false);
        CHECK_COND(xu.valid && !xu.kAlpha2Stripped && !xu.notes.isEmpty(),
                   "57b: unknown wavelength must skip Ka2 stripping with a note");
    }

    // ---------------------------------------------------------------- 57c: W-H without physical solution
    {
        const double lam = 1.540598, a0 = 4.0495, K = 0.9;
        std::vector<double> tt;
        for (double v = 30.0; v <= 120.0 + 1e-9; v += 0.01)
            tt.push_back(v);
        // negative slope: beta cos = 0.0045 - 0.0006 * 4 sin(theta)
        const std::vector<double> yA = suite57XrdPattern(tt, a0, { { lam, 1.0 } },
                                                         [](double x) { return 0.0045 - 0.0006 * x; });
        Table *tA = suite57Table(mw, "S57WhNegSlope", QStringList() << "TwoTheta" << "I", { tt, yA });
        Graph *gA = suite57Plot(mw, tA, "I");
        CHECK_COND(gA != nullptr, "57c: plot A");
        const CurveMathTools::XRDAnalysisResult rA = CurveMathTools::analyzeXRD(
                mw, gA, QString(), lam, 0.0, K, QStringLiteral("fcc"), false, false, true);
        std::cout << "[INFO] 57c negative slope: slope=" << rA.whSlopeRaw << " eps=" << rA.whMicrostrain
                  << " D=" << rA.whSizeNm << " physical=" << rA.whPhysical << "\n";
        CHECK_COND(rA.valid && !rA.whPhysical && std::isnan(rA.whMicrostrain) && rA.whSlopeRaw < -3e-4
                           && std::fabs(rA.whSizeNm - K * lam / (10.0 * 0.0045)) / 30.8 < 0.10
                           && !rA.notes.isEmpty(),
                   "57c: negative W-H slope must give eps = N/A with a note");

        // non-positive intercept: beta cos = -0.0008 + 0.0016 * 4 sin(theta)
        const std::vector<double> yB = suite57XrdPattern(tt, a0, { { lam, 1.0 } },
                                                         [](double x) { return -0.0008 + 0.0016 * x; });
        Table *tB = suite57Table(mw, "S57WhNegIcpt", QStringList() << "TwoTheta" << "I", { tt, yB });
        Graph *gB = suite57Plot(mw, tB, "I");
        CHECK_COND(gB != nullptr, "57c: plot B");
        const CurveMathTools::XRDAnalysisResult rB = CurveMathTools::analyzeXRD(
                mw, gB, QString(), lam, 0.0, K, QStringLiteral("fcc"), false, false, true);
        std::cout << "[INFO] 57c negative intercept: icpt=" << rB.whInterceptRaw << " D=" << rB.whSizeNm
                  << " Scherrer=" << rB.meanScherrerSizeNm << " eps=" << rB.whMicrostrain << "\n";
        CHECK_COND(rB.valid && !rB.whPhysical && std::isnan(rB.whSizeNm) && rB.whInterceptRaw <= 0.0
                           && std::isfinite(rB.meanScherrerSizeNm)
                           && std::fabs(rB.whMicrostrain - 0.0016) / 0.0016 < 0.15,
                   "57c: non-positive W-H intercept must give D = N/A (not the Scherrer mean)");
    }

    // ---------------------------------------------------------------- 57d: SG edges & X spacing
    {
        std::vector<double> x, y, xn, yn;
        for (int i = 0; i <= 20; ++i) {
            x.push_back(i);
            y.push_back(100.0 + 2.0 * i + 0.5 * i * i);
            const double xv = i + 0.35 * std::sin(1.3 * i);
            xn.push_back(xv);
            yn.push_back(100.0 + 2.0 * xv + 0.5 * xv * xv);
        }
        Table *t = suite57Table(mw, "S57SgUni", QStringList() << "X" << "Y", { x, y });
        Graph *g = suite57Plot(mw, t, "Y");
        CHECK_COND(g != nullptr, "57d: plot");
        SmoothFilter sf(mw, g, t->name() + "_Y", SmoothFilter::SavitzkyGolay);
        sf.setSmoothPoints(2, 2);
        sf.setPolynomOrder(2);
        CHECK_COND(sf.run(), "57d: SG run failed");
        QwtPlotCurve *c = g->curve(g->curves() - 1);
        CHECK_COND(c && c->dataSize() == 21, "57d: SG output size");
        double maxErr = 0.0;
        for (int i = 0; i < 21; ++i)
            maxErr = std::max(maxErr, std::fabs(c->y(i) - y[size_t(i)]));
        std::cout << "[INFO] 57d SG quadratic max error (incl. edges) = " << maxErr
                  << " (edge y0 = " << c->y(0) << ", expected 100)\n";
        CHECK_COND(maxErr < 1e-8, "57d: SG must reproduce a quadratic exactly, including both edges");
        CHECK_COND(sf.xHandlingNotice().isEmpty(), "57d: unexpected notice for uniform X");

        Table *tn = suite57Table(mw, "S57SgNonUni", QStringList() << "X" << "Y", { xn, yn });
        Graph *gn = suite57Plot(mw, tn, "Y");
        SmoothFilter sfn(mw, gn, tn->name() + "_Y", SmoothFilter::SavitzkyGolay);
        sfn.setSmoothPoints(2, 2);
        sfn.setPolynomOrder(2);
        CHECK_COND(sfn.run(), "57d: non-uniform SG run failed");
        QwtPlotCurve *cn = gn->curve(gn->curves() - 1);
        double maxErrN = 0.0;
        for (int i = 0; i < 21; ++i)
            maxErrN = std::max(maxErrN, std::fabs(cn->y(i) - yn[size_t(i)]));
        std::cout << "[INFO] 57d non-uniform SG max error = " << maxErrN << "\n";
        CHECK_COND(maxErrN < 1e-7 && !sfn.xHandlingNotice().isEmpty(),
                   "57d: non-uniform SG must use real X (exact for a quadratic) and say so");

        SmoothFilter sfa(mw, gn, tn->name() + "_Y", SmoothFilter::Average);
        sfa.setSmoothPoints(5);
        CHECK_COND(sfa.run() && !sfa.xHandlingNotice().isEmpty(),
                   "57d: moving average on non-uniform X must emit a notice");
        const int logLen = mw->logInfo.size();
        SmoothFilter sfm(mw, gn, tn->name() + "_Y", SmoothFilter::Median);
        sfm.setSmoothPoints(5);
        CHECK_COND(sfm.run() && mw->logInfo.size() > logLen,
                   "57d: median smoothing notice was not written to the log");
    }

    // ---------------------------------------------------------------- 57e: derivative
    {
        std::vector<double> x, y;
        for (int i = 0; i <= 10; ++i) {
            x.push_back(i);
            y.push_back(double(i) * i);
        }
        Table *t = suite57Table(mw, "S57Deriv", QStringList() << "X" << "Y", { x, y });
        Graph *g = suite57Plot(mw, t, "Y");
        CHECK_COND(g != nullptr, "57e: plot");
        const int nWin = mw->windowsList().size();
        Differentiation d(mw, g, t->name() + "_Y");
        CHECK_COND(d.run(), "57e: differentiation failed");
        MultiLayer *mlD = nullptr;
        const QList<MyWidget *> wins = mw->windowsList();
        for (int i = nWin; i < wins.size(); ++i)
            if (wins[i] && wins[i]->inherits("MultiLayer"))
                mlD = static_cast<MultiLayer *>(wins[i]);
        CHECK_COND(mlD && mlD->activeGraph() && mlD->activeGraph()->curves() >= 1, "57e: derivative graph");
        QwtPlotCurve *dc = mlD->activeGraph()->curve(0);
        std::cout << "[INFO] 57e derivative n=" << dc->dataSize() << " y(0)=" << dc->y(0)
                  << " y(5)=" << dc->y(5) << " y(10)=" << dc->y(10) << "\n";
        CHECK_COND(dc->dataSize() == 11 && std::fabs(dc->y(0) - 1.0) < 1e-12
                           && std::fabs(dc->y(5) - 10.0) < 1e-12 && std::fabs(dc->y(10) - 19.0) < 1e-12,
                   "57e: derivative must keep n points with one-sided end differences");

        Table *t2 = suite57Table(mw, "S57DerivDup", QStringList() << "X" << "Y",
                                 { { 0, 1, 1, 2, 3, 4 }, { 0, 1, 2, 4, 9, 16 } });
        Graph *g2 = suite57Plot(mw, t2, "Y");
        const int nWin2 = mw->windowsList().size();
        Differentiation d2(mw, g2, t2->name() + "_Y");
        const bool ok2 = d2.run();
        CHECK_COND(!ok2 && d2.error() && mw->windowsList().size() == nWin2,
                   "57e: duplicate adjacent X must be rejected before differentiating");
    }

    // ---------------------------------------------------------------- 57f: signal-processing notice
    {
        std::vector<double> x, y;
        const double tx[] = { 0.0, 0.3, 0.1, 0.7, 0.5, 0.9, 1.4, 1.1, 1.6, 2.0, 1.8, 2.5,
                              2.2, 2.9, 3.1, 3.6, 3.3, 4.0, 3.8, 4.4 };
        for (double tv : tx) {
            x.push_back(tv);
            y.push_back(std::sin(2.0 * M_PI * 1.0 * tv));
        }
        Table *t = suite57Table(mw, "S57SigNotice", QStringList() << "T" << "S", { x, y });
        Graph *g = suite57Plot(mw, t, "S");
        CHECK_COND(g != nullptr, "57f: plot");
        const int logLen = mw->logInfo.size();
        QwtPlotCurve *env = AdvancedSignalProc::hilbertEnvelope(mw, g, t->name() + "_S");
        const QString added = mw->logInfo.mid(logLen);
        std::cout << "[INFO] 57f log: " << added.trimmed().toStdString() << "\n";
        CHECK_COND(env != nullptr && added.contains("Hilbert") && added.count('\n') >= 1,
                   "57f: Hilbert must log the re-ordering / resampling of the input in one line");
    }

    // ---------------------------------------------------------------- 57g: ROI integral end points
    {
        std::vector<double> x, y;
        for (int i = 0; i <= 10; ++i) {
            x.push_back(i);
            y.push_back(10.0 + i);
        }
        Table *t = suite57Table(mw, "S57Roi", QStringList() << "X" << "Y", { x, y });
        Graph *g = suite57Plot(mw, t, "Y", Graph::LineSymbols);
        CHECK_COND(g != nullptr, "57g: plot");
        RoiGadgetTool *roi = RoiGadgetTool::attachGadget(mw, g, RoiGadgetTool::Integrate);
        CHECK_COND(roi != nullptr, "57g: attach gadget");
        roi->setIntegrateBaseline(RoiGadgetTool::BaseYZero);
        roi->setTargetCurve(t->name() + "_Y");
        // box X = [2.5, 7.5], Y = [0, 14] (cuts the curve in Y); exact integral = 75
        roi->setRoiRect(QRectF(2.5, 0.0, 5.0, 14.0));
        std::cout << "[INFO] 57g ROI integral [2.5, 7.5] = " << roi->primaryMetric() << " (exact 75)\n";
        CHECK_COND(std::fabs(roi->primaryMetric() - 75.0) < 1e-9,
                   "57g: ROI integral must interpolate the ends and ignore the box Y range");
    }

    // ---------------------------------------------------------------- 57h: normalisation (unit area)
    {
        Table *t = suite57Table(mw, "S57Norm", QStringList() << "X" << "Y",
                                { { 3, 0, 4, 1, 2 }, { 4, 1, 5, 2, 3 } });
        Graph *g = suite57Plot(mw, t, "Y");
        CHECK_COND(g != nullptr, "57h: plot");
        const int before = g->curves();
        QVariantMap params;
        params.insert(QStringLiteral("mode"), 3);
        RecalcManager::repeatForAllCurves(mw, g, RecalcManager::OpNormalize, params);
        CHECK_COND(g->curves() == before + 1, "57h: recalc normalize did not add a curve");
        QwtPlotCurve *cr = g->curve(g->curves() - 1);
        QwtPlotCurve *cm = CurveMathTools::normalizeCurve(mw, g, t->name() + "_Y", CurveMathTools::UnitArea);
        CHECK_COND(cr && cm && cr->dataSize() == 5 && cm->dataSize() == 5, "57h: normalized curves");
        // The output keeps the original point order (2026-11-22); the area is taken X-sorted.
        double area = 0.0, maxDiff = 0.0;
        const double origX[5] = { 3, 0, 4, 1, 2 };
        std::vector<std::pair<double, double>> sorted;
        bool orderKept = true;
        for (int i = 0; i < 5; ++i) {
            maxDiff = std::max(maxDiff, std::fabs(cr->x(i) - cm->x(i)) + std::fabs(cr->y(i) - cm->y(i)));
            orderKept = orderKept && cr->x(i) == origX[i];
            sorted.emplace_back(cr->x(i), cr->y(i));
        }
        std::sort(sorted.begin(), sorted.end());
        for (size_t i = 1; i < sorted.size(); ++i)
            area += 0.5 * (sorted[i].second + sorted[i - 1].second) * (sorted[i].first - sorted[i - 1].first);
        std::cout << "[INFO] 57h recalc unit-area: area=" << area << " diff vs dialog=" << maxDiff << "\n";
        CHECK_COND(std::fabs(area - 1.0) < 1e-12 && maxDiff < 1e-12 && orderKept
                           && std::fabs(cr->y(0) - 4.0 / 12.0) < 1e-12,
                   "57h: recalc unit-area normalisation must keep the point order, use the X-sorted area and match the dialog");
    }

    // ---------------------------------------------------------------- 57i: Weibull
    {
        CHECK_COND(std::fabs(GraphStatsAndSurfaceFit::jisR1625UnbiasingFactor(30) - 0.9538) < 5e-5,
                   "57i: JIS R 1625 B_30 must be 0.9538 (worked example of the standard)");
        CHECK_COND(GraphStatsAndSurfaceFit::jisR1625UnbiasingFactor(121) == 1.0, "57i: B_n for n > 120");

        const double mTrue = 10.0, s0True = 500.0;
        const int n = 30;
        std::vector<double> s;
        for (int i = 1; i <= n; ++i) {
            const double F = (i - 0.5) / n;
            s.push_back(s0True * std::pow(-std::log(1.0 - F), 1.0 / mTrue));
        }
        std::vector<double> sx = s;
        sx.push_back(-5.0);
        sx.push_back(0.0);
        Table *t = suite57Table(mw, "S57Weib", QStringList() << "S", { sx });
        const auto wb = GraphStatsAndSurfaceFit::plotWeibullProbability(
                mw, t, "S", QString(), 0, false, true, false, true);
        double lnL = 0.0;
        for (double v : s) {
            const double z = v / wb.mleScaleSigma0;
            lnL += std::log(wb.mleModulusM / wb.mleScaleSigma0) + (wb.mleModulusM - 1.0) * std::log(z)
                    - std::pow(z, wb.mleModulusM);
        }
        std::cout << "[INFO] 57i Weibull MLE m=" << wb.mleModulusM << " -> unbiased " << wb.modulusM
                  << " (B=" << wb.unbiasingFactor << "), s0=" << wb.scaleSigma0 << ", lnL=" << wb.logLikelihood
                  << " (check " << lnL << "), excluded=" << wb.excludedCount << "\n";
        CHECK_COND(wb.valid && wb.usedMle && wb.excludedCount == 2 && wb.unbiased
                           && std::fabs(wb.unbiasingFactor - 0.9538) < 5e-5
                           && std::fabs(wb.modulusM - wb.mleModulusM * wb.unbiasingFactor) < 1e-9
                           && std::fabs(wb.mleModulusM - mTrue) / mTrue < 0.06
                           && std::fabs(wb.scaleSigma0 - s0True) / s0True < 0.01
                           && std::fabs(wb.logLikelihood - lnL) < 1e-6 && !wb.methodDescription.isEmpty(),
                   "57i: 2-parameter MLE with JIS R 1625 correction / ln L / exclusion count");

        std::vector<double> s3;
        for (double v : s)
            s3.push_back(v + 200.0);
        Table *t3 = suite57Table(mw, "S57Weib3", QStringList() << "S", { s3 });
        const auto wb3 = GraphStatsAndSurfaceFit::plotWeibullProbability(
                mw, t3, "S", QString(), 0, true, true, false, true);
        std::cout << "[INFO] 57i 3-param MLE: gamma=" << wb3.thresholdGamma << " m=" << wb3.modulusM
                  << " method=" << wb3.methodDescription.toStdString() << "\n";
        CHECK_COND(wb3.valid && wb3.threeParameter && wb3.usedMle && !wb3.unbiased
                           && !wb3.methodDescription.isEmpty() && !wb3.notes.isEmpty()
                           && wb3.methodDescription != wb.methodDescription,
                   "57i: 3-parameter MLE must be labelled as conditional MLE and skip the JIS correction");

        Table *tf = suite57Table(mw, "S57WeibFail", QStringList() << "S", { { 100, 100, 100, 100 } });
        const auto wf = GraphStatsAndSurfaceFit::plotWeibullProbability(mw, tf, "S");
        CHECK_COND(!wf.valid && !wf.error.isEmpty(), "57i: failed Weibull must give a reason");
    }

    // ---------------------------------------------------------------- 57j: Arrhenius
    {
        const double R = 8.314462618, Ea = 50.0e3, A = 1.0e6;
        std::vector<double> T, k;
        for (double tv = 300.0; tv <= 500.0 + 1e-9; tv += 50.0) {
            T.push_back(tv);
            k.push_back(A * std::exp(-Ea / (R * tv)));
        }
        T.push_back(0.0);
        k.push_back(1.0);
        T.push_back(400.0);
        k.push_back(0.0);
        Table *t = suite57Table(mw, "S57Arr", QStringList() << "T" << "k", { T, k });
        const auto ar = CurveMathTools::createArrheniusPlot(mw, t, "T", "k", false, true, false);
        std::cout << "[INFO] 57j Arrhenius Ea=" << ar.activationEnergyKJmol << " kJ/mol, excluded="
                  << ar.excludedCount << "\n";
        CHECK_COND(ar.valid && ar.excludedCount == 2 && !ar.notes.isEmpty()
                           && std::fabs(ar.activationEnergyKJmol - 50.0) < 1e-6,
                   "57j: Arrhenius exclusion count / Ea");
        Table *tf = suite57Table(mw, "S57ArrFail", QStringList() << "T" << "k", { { 300, 400 }, { 1.0, -1.0 } });
        const auto af = CurveMathTools::createArrheniusPlot(mw, tf, "T", "k", false, true, false);
        CHECK_COND(!af.valid && !af.error.isEmpty() && af.excludedCount == 1,
                   "57j: failed Arrhenius must give a reason");
    }

    // ---------------------------------------------------------------- 57k: Kissinger from peak-fit centres
    {
        const double R = 8.314462618, Ed = 100.0e3, A = 1.0e10;
        const double rates[] = { 5.0, 10.0, 20.0, 40.0 };
        std::vector<double> tp;
        for (double bmin : rates) {
            const double bs = bmin / 60.0;
            auto f = [&](double Tk) { return std::log(bs / (Tk * Tk)) - std::log(A * R / Ed) + Ed / (R * Tk); };
            double lo = 250.0, hi = 1500.0; // f(lo) > 0 > f(hi)
            for (int it = 0; it < 200; ++it) {
                const double mid = 0.5 * (lo + hi);
                (f(mid) > 0.0 ? lo : hi) = mid;
            }
            tp.push_back(0.5 * (lo + hi));
        }
        QList<Table *> fitTables;
        for (int i = 0; i < 4; ++i) {
            // two peaks, higher one listed first: peak 1 counted from the lowest centre = tp
            Table *pt = suite57Table(mw, QString("S57PeakSummary_%1Kmin").arg(rates[i]),
                                     QStringList() << "Peak" << "Center" << "FWHM",
                                     { { 1, 2 }, { tp[size_t(i)] + 60.0, tp[size_t(i)] }, { 20, 15 } });
            CHECK_COND(pt && CurveMathTools::isPeakFitResultTable(pt), "57k: peak table detection");
            fitTables << pt;
        }
        QStringList missing;
        const auto centers = CurveMathTools::collectPeakFitCenters(fitTables, 1, &missing);
        CHECK_COND(centers.size() == 4 && missing.isEmpty(), "57k: centre collection");
        QVector<double> vr, vt;
        for (int i = 0; i < 4; ++i) {
            vr << rates[i];
            vt << centers[i].center;
        }
        const auto kr = CurveMathTools::analyzeKissinger(nullptr, vr, vt, false, false, false);
        std::cout << "[INFO] 57k Kissinger from peak-fit centres: Ed=" << kr.activationEnergyKJmol
                  << " kJ/mol (true 100)\n";
        CHECK_COND(kr.valid && std::fabs(kr.activationEnergyKJmol - 100.0) < 1e-6,
                   "57k: Kissinger Ed from peak-fit centres");
        const auto missing3 = CurveMathTools::collectPeakFitCenters(fitTables, 3, &missing);
        CHECK_COND(missing3.isEmpty() && missing.size() == 4, "57k: missing peak N must be reported");

        // batch table (Curve, Peak, Center): 2 curves
        Table *bt = mw->newTable("S57BatchPeaks", 4, 3);
        bt->setHeader(QStringList() << "Curve" << "Peak" << "Center");
        bt->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
        const char *cn[] = { "TDS_100", "TDS_100", "TDS_200", "TDS_200" };
        const double pk[] = { 1, 2, 1, 2 }, ce[] = { 410.0, 380.0, 425.0, 395.0 };
        for (int r = 0; r < 4; ++r) {
            bt->column(0)->setTextAt(r, cn[r]);
            bt->column(1)->setValueAt(r, pk[r]);
            bt->column(2)->setValueAt(r, ce[r]);
        }
        const auto bc = CurveMathTools::collectPeakFitCenters(QList<Table *>() << bt, 2);
        CHECK_COND(bc.size() == 2 && bc[0].source == "TDS_100" && bc[0].center == 410.0
                           && bc[1].center == 425.0,
                   "57k: batch table must give one centre per curve");

        const auto ke = CurveMathTools::analyzeKissinger(
                nullptr, QVector<double>() << 5 << 10 << 20 << 0 << std::numeric_limits<double>::quiet_NaN(),
                QVector<double>() << tp[0] << tp[1] << tp[2] << 500 << 500, false, false, false);
        CHECK_COND(ke.valid && ke.excludedCount == 2 && !ke.notes.isEmpty(),
                   "57k: Kissinger must count and report excluded pairs");
        CHECK_COND(!CurveMathTools::isPeakFitResultTable(mw->newTable("S57Plain", 3, 2)),
                   "57k: plain table wrongly detected as a peak-fit table");
    }

    // ---------------------------------------------------------------- 57l: review fixes (2026-11-22)
    {
        // (1) Kissinger from peak-fit results: rates are paired with their own source. The
        // 15 K/min table has no peak 1 (empty table) and the 30 K/min table has only peak 1
        // when peak 2 is asked for; neither may shift the other rates.
        const double R = 8.314462618, Ed = 100.0e3, A = 1.0e10;
        auto tpOf = [&](double bmin) {
            const double bs = bmin / 60.0;
            auto f = [&](double Tk) { return std::log(bs / (Tk * Tk)) - std::log(A * R / Ed) + Ed / (R * Tk); };
            double lo = 250.0, hi = 1500.0;
            for (int it = 0; it < 200; ++it) {
                const double mid = 0.5 * (lo + hi);
                (f(mid) > 0.0 ? lo : hi) = mid;
            }
            return 0.5 * (lo + hi);
        };
        const double rl[] = { 5.0, 10.0, 15.0, 20.0, 40.0 };
        QList<Table *> tabs;
        for (int i = 0; i < 5; ++i) {
            Table *pt = nullptr;
            if (rl[i] == 15.0) {
                pt = mw->newTable(QString("S57lPeaks_%1Kmin").arg(rl[i]), 2, 3);
                pt->setHeader(QStringList() << "Peak" << "Center" << "FWHM"); // no peak rows
            } else {
                const double tp = tpOf(rl[i]);
                pt = suite57Table(mw, QString("S57lPeaks_%1Kmin").arg(rl[i]),
                                  QStringList() << "Peak" << "Center" << "FWHM",
                                  { { 1, 2 }, { tp + 60.0, tp }, { 20, 15 } });
            }
            CHECK_COND(pt && CurveMathTools::isPeakFitResultTable(pt), "57l: peak table detection");
            tabs << pt;
        }
        const auto src = CurveMathTools::collectPeakFitSources(tabs, 1);
        CHECK_COND(src.size() == 5 && !src[2].found && src[3].found
                           && src[3].source == tabs[3]->name(),
                   "57l: every table (also one without peaks) must be one source, in order");
        QVector<double> pr, pt;
        QString perr;
        QStringList skipped;
        const QVector<double> rates5 = { 5, 10, 15, 20, 40 };
        CHECK_COND(CurveMathTools::pairKissingerFitRates(rates5, src, &pr, &pt, &perr, &skipped)
                           && pr == (QVector<double>() << 5 << 10 << 20 << 40) && skipped.size() == 1
                           && skipped[0] == tabs[2]->name(),
                   "57l: a source without peak N must drop its own rate only");
        const auto kr = CurveMathTools::analyzeKissinger(nullptr, pr, pt, false, false, false);
        std::cout << "[INFO] 57l Kissinger with a peak-less table: Ed=" << kr.activationEnergyKJmol
                  << " kJ/mol (true 100)\n";
        CHECK_COND(kr.valid && std::fabs(kr.activationEnergyKJmol - 100.0) < 1e-6,
                   "57l: Ed must stay 100 kJ/mol when one table has no peak");
        const QVector<double> rates4 = { 5, 10, 20, 40 };
        CHECK_COND(!CurveMathTools::pairKissingerFitRates(rates4, src, &pr, &pt, &perr)
                           && pr.isEmpty() && perr.contains(tabs[2]->name()),
                   "57l: a rate count that differs from the source count must stop with the source list");
        CHECK_COND(CurveMathTools::collectPeakFitCenters(tabs, 1).size() == 4,
                   "57l: collectPeakFitCenters must still return only the found centres");

        // (3) K-alpha2: a rounded wavelength within 1e-3 identifies the tube.
        CurveMathTools::XRayAnodeLines al;
        bool avg = true, approx = false;
        CHECK_COND(CurveMathTools::matchXRayAnode(1.54, &al, &avg, &approx) && al.anode == "Cu"
                           && !avg && approx,
                   "57l: 1.54 A must match Cu K-alpha1 approximately");
        CHECK_COND(CurveMathTools::matchXRayAnode(1.788, &al, &avg, &approx) && al.anode == "Co"
                           && !avg && approx,
                   "57l: 1.788 A must match Co K-alpha1 approximately");
        CHECK_COND(CurveMathTools::matchXRayAnode(1.540598, &al, &avg, &approx) && !approx,
                   "57l: the exact Cu K-alpha1 must not be flagged approximate");
        CHECK_COND(!CurveMathTools::matchXRayAnode(1.55, &al, &avg, &approx),
                   "57l: 1.55 A (0.6 % off Cu) must not match");
        {
            const double lam1 = 1.788965, lam2 = 1.792850, a0 = 4.0495;
            std::vector<double> tt;
            for (double v = 35.0; v <= 130.0 + 1e-9; v += 0.01)
                tt.push_back(v);
            auto betaCos = [&](double x4s) { return 0.9 * lam1 / 400.0 + 1.5e-3 * x4s; };
            const std::vector<double> y =
                    suite57XrdPattern(tt, a0, { { lam1, 1.0 }, { lam2, 0.5 } }, betaCos);
            Table *t = suite57Table(mw, "S57lCoXrd", QStringList() << "TwoTheta" << "I", { tt, y });
            Graph *g = suite57Plot(mw, t, "I");
            CHECK_COND(g != nullptr, "57l: XRD plot");
            const auto xa = CurveMathTools::analyzeXRD(mw, g, QString(), 1.788, 0.0, 0.9,
                                                       QStringLiteral("fcc"), true, false, false);
            CHECK_COND(xa.valid && xa.kAlpha2Stripped && xa.anodeApproximate && xa.anodeName == "Co"
                               && std::fabs(xa.kAlpha2Angstrom - lam2) < 1e-9
                               && !xa.kAlpha2Notice.isEmpty(),
                       "57l: K-alpha2 must be stripped as Co for 1.788 A, with a notice");
            const auto xe = CurveMathTools::analyzeXRD(mw, g, QString(), lam1, 0.0, 0.9,
                                                       QStringLiteral("fcc"), true, false, false);
            CHECK_COND(xe.valid && xe.kAlpha2Stripped && !xe.anodeApproximate
                               && xe.kAlpha2Notice.isEmpty(),
                       "57l: the exact wavelength needs no notice");
            const auto xu = CurveMathTools::analyzeXRD(mw, g, QString(), 1.6, 0.0, 0.9,
                                                       QStringLiteral("none"), true, false, false);
            CHECK_COND(xu.valid && !xu.kAlpha2Stripped && !xu.kAlpha2Notice.isEmpty(),
                       "57l: skipped K-alpha2 stripping must carry a notice for the dialog");
        }

        // (6) W-H: the reason for "no W-H line" depends on the number of peaks.
        const QString wh1 = CurveMathTools::williamsonHallUnavailableNote(1);
        const QString wh3 = CurveMathTools::williamsonHallUnavailableNote(3);
        CHECK_COND(!wh1.isEmpty() && !wh3.isEmpty() && wh1 != wh3,
                   "57l: W-H reasons for <2 peaks and for coinciding 4 sin(theta) must differ");

        // (2) normalisation keeps the point order of round-trip data (hysteresis loop).
        {
            const std::vector<double> lx = { 0, 1, 2, 3, 2, 1, 0 };
            const std::vector<double> ly = { 0, 2, 4, 6, 5, 3, 1 };
            Table *t = suite57Table(mw, "S57lLoop", QStringList() << "X" << "Y", { lx, ly });
            Graph *g = suite57Plot(mw, t, "Y");
            CHECK_COND(g != nullptr, "57l: loop plot");
            const int before = g->curves();
            QVariantMap params;
            params.insert(QStringLiteral("mode"), 0);
            RecalcManager::repeatForAllCurves(mw, g, RecalcManager::OpNormalize, params);
            CHECK_COND(g->curves() == before + 1, "57l: recalc normalize did not add a curve");
            QwtPlotCurve *cr = g->curve(g->curves() - 1);
            QwtPlotCurve *cm = CurveMathTools::normalizeCurve(mw, g, t->name() + "_Y",
                                                              CurveMathTools::Range0To1);
            bool sameOrder = cr && cm && cr->dataSize() == 7 && cm->dataSize() == 7;
            for (int i = 0; sameOrder && i < 7; ++i) {
                sameOrder = cr->x(i) == lx[size_t(i)] && cm->x(i) == lx[size_t(i)]
                        && std::fabs(cr->y(i) - ly[size_t(i)] / 6.0) < 1e-12
                        && std::fabs(cm->y(i) - ly[size_t(i)] / 6.0) < 1e-12;
            }
            CHECK_COND(sameOrder, "57l: normalisation must keep the original point order of a loop");
        }

        // (9) JIS R 1625 6.2 c): B_n formula up to n = 120, B_n = 1 above (as the standard states).
        CHECK_COND(std::fabs(GraphStatsAndSurfaceFit::jisR1625UnbiasingFactor(120)
                             - 1.0 / (1.0 + 2.04 * std::pow(120.0, -1.1)))
                           < 1e-15,
                   "57l: B_120 must follow the formula");

        // (8) duplicate-X differentiation: no wait cursor while the error box is shown.
        {
            Table *t = suite57Table(mw, "S57lDupX", QStringList() << "X" << "Y",
                                    { { 0, 1, 1, 2, 3, 4 }, { 0, 1, 2, 3, 4, 5 } });
            Graph *g = suite57Plot(mw, t, "Y");
            CHECK_COND(g != nullptr, "57l: dup-X plot");
            qApp->setProperty("torigraph_noninteractive", QVariant());
            bool boxSeen = false, cursorDuringBox = true;
            QTimer poll;
            int tries = 0;
            QObject::connect(&poll, &QTimer::timeout, [&]() {
                ++tries;
                if (QMessageBox *mb = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                    boxSeen = true;
                    cursorDuringBox = (QApplication::overrideCursor() != nullptr);
                    mb->reject();
                    poll.stop();
                } else if (tries > 200) {
                    poll.stop();
                }
            });
            poll.start(20);
            Differentiation diff(mw, g, t->name() + "_Y");
            const bool ok = diff.run();
            poll.stop();
            qApp->setProperty("torigraph_noninteractive", true);
            std::cout << "[INFO] 57l dup-X derivative: ok=" << ok << " box=" << boxSeen
                      << " cursorDuringBox=" << cursorDuringBox
                      << " cursorAfter=" << (QApplication::overrideCursor() != nullptr) << "\n";
            CHECK_COND(!ok && boxSeen && !cursorDuringBox && QApplication::overrideCursor() == nullptr,
                       "57l: the duplicate-X error box must be shown with the normal cursor, and the cursor stack must stay balanced");
        }
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 57 numerical robustness / XRD / Weibull / Kissinger gate passed!\n";
    return 0;
}

// =============================================================================
// Suite 56: Nonlinear / peak fit honesty (F1 convergence, F5 GaussAmp width & shape constants,
// F9 t-based confidence intervals / signed Pearson r / NaN R^2, F8 apparent-fit exclusions,
// F11 model search). Reference values were computed by hand (closed-form OLS) and with the
// textbook Student-t quantiles t(0.975, 3) = 3.182446305284263, t(0.975, 8) = 2.306004135033371.
// =============================================================================
int runSuite56FitConvergenceAndStatistics(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 56: fit convergence, peak width convention & statistics...\n";
    qApp->setProperty("torigraph_noninteractive", true);
    auto approxEq = [](double a, double b, double tol) { return std::fabs(a - b) <= tol; };

    // ---- 56a. Student-t quantiles --------------------------------------------------------
    CHECK_COND(approxEq(Fit::studentTQuantile(0.975, 3), 3.182446305284263, 1e-9),
               "56a: t(0.975, 3) wrong");
    CHECK_COND(approxEq(Fit::studentTQuantile(0.975, 8), 2.306004135033371, 1e-9),
               "56a: t(0.975, 8) wrong");
    CHECK_COND(std::isnan(Fit::studentTQuantile(0.975, 0)), "56a: t with dof 0 must be NaN");

    // ---- 56b. Negative-slope straight line: CI, DOF, signed Pearson r ---------------------
    {
        const double xs[5] = { 1, 2, 3, 4, 5 };
        const double ys[5] = { 9.9, 8.1, 5.8, 4.2, 1.9 };
        Table *t = mw->newTable(5, 2, "S56Line", "Suite 56 line");
        t->setColName(0, "X");
        t->setColName(1, "Y");
        for (int r = 0; r < 5; ++r) {
            t->setCell(r, 0, xs[r]);
            t->setCell(r, 1, ys[r]);
        }
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Scatter);
        CHECK_COND(ml && ml->activeGraph(), "56b: graph not created");
        Graph *g = ml->activeGraph();
        LinearFit lf(mw, g);
        lf.setDataCurve(0, 1.0, 5.0);
        lf.fit();
        const auto &res = lf.results();
        std::cout << "[INFO] 56b: B=" << res[0] << " A=" << res[1] << " r=" << lf.pearsonR()
                  << " SE(A)=" << lf.parameterStdError(1) << "\n";
        CHECK_COND(approxEq(res[1], -1.99, 1e-10) && approxEq(res[0], 11.95, 1e-10), "56b: OLS mismatch");
        CHECK_COND(lf.degreesOfFreedom() == 3, "56b: dof must be 3");
        CHECK_COND(approxEq(lf.parameterStdError(1), 0.05972157622389641, 1e-12)
                           && approxEq(lf.parameterStdError(0), 0.19807406022327445, 1e-12),
                   "56b: standard errors mismatch");
        CHECK_COND(approxEq(lf.pearsonR(), -0.9986517555689657, 1e-12),
                   "56b: Pearson r must be the signed x-y correlation");
        double lo = 0, hi = 0;
        CHECK_COND(lf.parameterConfidenceInterval(1, 0.95, &lo, &hi)
                           && approxEq(lo, -2.1800607095994913, 1e-10)
                           && approxEq(hi, -1.7999392904005083, 1e-10),
                   "56b: 95% CI of slope mismatch");
        Table *pt = lf.parametersTable("S56LinePar");
        CHECK_COND(pt && pt->numCols() == 6, "56b: parameter table must have 6 columns");
        CHECK_COND(approxEq(pt->column(3)->valueAt(0), 11.319639938869788, 1e-9)
                           && approxEq(pt->column(4)->valueAt(0), 12.58036006113021, 1e-9)
                           && approxEq(pt->column(3)->valueAt(1), -2.1800607095994913, 1e-9)
                           && approxEq(pt->column(4)->valueAt(1), -1.7999392904005083, 1e-9)
                           && approxEq(pt->column(5)->valueAt(0), 3.0, 0.0),
                   "56b: parameter table CI / DOF columns wrong");

        // Pearson r is not reported for a nonlinear model
        NonLinearFit nlf(mw, g);
        nlf.setDataCurve(0, 1.0, 5.0);
        nlf.setParametersList(QStringList() << "a" << "b");
        nlf.setFormula("a*exp(b*x)");
        nlf.setInitialGuess(0, 12.0);
        nlf.setInitialGuess(1, -0.2);
        nlf.fit();
        CHECK_COND(std::isnan(nlf.pearsonR()), "56b: nonlinear fit must not report Pearson r");
    }

    // ---- 56c. DOF = 0 -> standard error undefined ("—"), table cell empty ------------------
    {
        Table *t = mw->newTable(2, 2, "S56Two", "Suite 56 two points");
        t->setColName(0, "X");
        t->setColName(1, "Y");
        t->setCell(0, 0, 1.0);
        t->setCell(0, 1, 3.0);
        t->setCell(1, 0, 2.0);
        t->setCell(1, 1, 5.0);
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Scatter);
        CHECK_COND(ml && ml->activeGraph(), "56c: graph not created");
        LinearFit lf(mw, ml->activeGraph());
        lf.setDataCurve(0, 1.0, 2.0);
        lf.fit();
        CHECK_COND(lf.degreesOfFreedom() == 0 && std::isnan(lf.parameterStdError(1)),
                   "56c: SE with zero dof must be NaN");
        Table *pt = lf.parametersTable("S56TwoPar");
        CHECK_COND(pt && pt->column(2)->isInvalid(1) && pt->column(3)->isInvalid(1),
                   "56c: SE / CI cells must stay empty when dof = 0");
        CHECK_COND(lf.legendInfo().contains(QString::fromUtf8("—")),
                   "56c: legend must show a dash for the undefined SE");
    }

    // ---- 56d. Two Gaussians with known sigma: GaussAmp width = sigma everywhere ------------
    const double A1 = 100.0, xc1 = 30.0, s1 = 1.5, A2 = 60.0, xc2 = 40.0, s2 = 2.0, base = 5.0;
    {
        const int n = 301;
        Table *t = mw->newTable(n, 2, "S56Peaks", "Suite 56 two gaussians");
        t->setColName(0, "TwoTheta");
        t->setColName(1, "Intensity");
        t->column(0)->setUnits(QStringLiteral("deg"));
        t->column(1)->setUnits(QStringLiteral("cps"));
        for (int r = 0; r < n; ++r) {
            const double x = 20.0 + 0.1 * r;
            const double y = base + A1 * std::exp(-0.5 * std::pow((x - xc1) / s1, 2))
                    + A2 * std::exp(-0.5 * std::pow((x - xc2) / s2, 2));
            t->setCell(r, 0, x);
            t->setCell(r, 1, y);
        }
        MultiLayer *ml =
                mw->multilayerPlot(t, QStringList() << (t->name() + "_Intensity"), Graph::Line);
        CHECK_COND(ml && ml->activeGraph(), "56d: graph not created");
        Graph *g = ml->activeGraph();

        MultiPeakFit mpf(mw, g, MultiPeakFit::GaussAmp, 2);
        mpf.setDataCurve(0, 20.0, 50.0);
        mpf.fit();
        const auto &r = mpf.results();
        CHECK_COND(r.size() == 7, "56d: unexpected parameter count");
        const int i1 = (std::fabs(r[1] - xc1) < std::fabs(r[4] - xc1)) ? 0 : 1;
        const int i2 = 1 - i1;
        std::cout << "[INFO] 56d: w1=" << r[3 * i1 + 2] << " w2=" << r[3 * i2 + 2]
                  << " converged=" << mpf.converged() << " R2=" << mpf.rSquare() << "\n";
        CHECK_COND(mpf.converged(), "56d: clean two-peak fit must converge");
        CHECK_COND(approxEq(std::fabs(r[3 * i1 + 2]), s1, 1e-5) && approxEq(std::fabs(r[3 * i2 + 2]), s2, 1e-5)
                           && approxEq(r[3 * i1], A1, 1e-4) && approxEq(r[3 * i2], A2, 1e-4)
                           && approxEq(r[6], base, 1e-4),
                   "56d: multi-peak GaussAmp w must be sigma (OriginPro GaussAmp convention)");
        CHECK_COND(mpf.formula().contains("/(2*w1^2)") && !mpf.formula().contains("exp(-2*"),
                   "56d: GaussAmp formula string must use exp(-(x-xc)^2/(2 w^2))");

        auto rows = mpf.computePeakSummary(r);
        CHECK_COND(rows.size() == 2, "56d: peak summary size");
        const auto &p1 = (std::fabs(rows[0].center - xc1) < 1.0) ? rows[0] : rows[1];
        const double fwhm1 = 2.0 * std::sqrt(2.0 * std::log(2.0)) * s1; // 3.5322...
        const double area1 = A1 * s1 * std::sqrt(2.0 * M_PI);            // 375.99...
        CHECK_COND(approxEq(p1.fwhm, fwhm1, 1e-4) && approxEq(p1.height, A1, 1e-4)
                           && approxEq(p1.area, area1, 1e-3),
                   "56d: FWHM / height / area of GaussAmp peak wrong");

        Table *sum = mpf.createPeakSummaryTable("S56PeakSum");
        CHECK_COND(sum && sum->column(1)->units() == "deg" && sum->column(2)->units() == "deg"
                           && sum->column(3)->units() == "cps"
                           && sum->column(4)->units() == QString::fromUtf8("cps·deg"),
                   "56d: peak summary must carry the data units");

        // Single-peak GaussAmp (GaussAmpFit) and the NonLinearFit built-in agree on w = sigma
        GaussAmpFit gaf(mw, g);
        gaf.setDataCurve(0, 24.0, 35.0);
        gaf.fit();
        NonLinearFit nl(mw, g);
        nl.setDataCurve(0, 24.0, 35.0);
        CHECK_COND(nl.setBuiltInModel("GaussAmp"), "56d: GaussAmp built-in missing");
        nl.fit();
        MultiPeakFit one(mw, g, MultiPeakFit::GaussAmp, 1);
        one.setDataCurve(0, 24.0, 35.0);
        one.fit();
        std::cout << "[INFO] 56d single-peak w: GaussAmpFit=" << gaf.results()[3]
                  << " NonLinearFit=" << nl.results()[3] << " MultiPeak=" << one.results()[2]
                  << "\n";
        CHECK_COND(approxEq(std::fabs(gaf.results()[3]), std::fabs(one.results()[2]), 5e-3)
                           && approxEq(std::fabs(nl.results()[3]), std::fabs(one.results()[2]), 5e-3),
                   "56d: GaussAmp width differs between GaussAmpFit, NonLinearFit and MultiPeakFit");

        // Area-parameterised profiles are described as area, height-parameterised as height
        CHECK_COND(MultiPeakFit::generateExplanationList(2, MultiPeakFit::Gauss).value(0)
                                   == MultiPeakFit::tr("(area %1)").arg(1)
                           && MultiPeakFit::generateExplanationList(1, MultiPeakFit::GaussAmp).value(0)
                                   == MultiPeakFit::tr("(height)"),
                   "56d: parameter descriptions must say area / height");

        // Pseudo-Voigt: the Lorentz fraction actually used is written into the formula
        MultiPeakFit pv(mw, g, MultiPeakFit::PseudoVoigt, 1);
        pv.setLorentzFraction(0.3);
        CHECK_COND(pv.formula().contains("0.3*2/PI") && pv.formula().contains("0.7*sqrt(4*ln(2)/PI)"),
                   "56d: Pseudo-Voigt formula must embed mu = 0.3");
        MultiPeakFit p7(mw, g, MultiPeakFit::PearsonVII, 1);
        p7.setPearsonShape(1.5);
        CHECK_COND(p7.formula().contains("^(-1.5)"), "56d: Pearson VII formula must embed m");
    }

    // ---- 56e. Non-convergence is reported, last iterate kept ------------------------------
    {
        const int n = 201;
        Table *t = mw->newTable(n, 2, "S56Single", "Suite 56 single peak");
        t->setColName(0, "X");
        t->setColName(1, "Y");
        for (int r = 0; r < n; ++r) {
            const double x = 20.0 + 0.1 * r;
            t->setCell(r, 0, x);
            t->setCell(r, 1, base + A1 * std::exp(-0.5 * std::pow((x - xc1) / s1, 2)));
        }
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Line);
        CHECK_COND(ml && ml->activeGraph(), "56e: graph not created");
        Graph *g = ml->activeGraph();

        auto makeFit = [&](NonLinearFit &f) {
            f.setDataCurve(0, 20.0, 40.0);
            f.setParametersList(QStringList() << "y0" << "A" << "xc" << "w");
            f.setFormula("y0+A*exp(-(x-xc)^2/(2*w^2))");
            f.setInitialGuess(0, 0.0);
            f.setInitialGuess(1, 40.0);
            f.setInitialGuess(2, 33.0);
            f.setInitialGuess(3, 4.0);
        };

        mw->writeFitResultsToLog = true;
        mw->clearLogInfo();
        NonLinearFit capped(mw, g);
        makeFit(capped);
        capped.setMaximumIterations(2);
        capped.fit();
        const auto &rc = capped.results();
        std::cout << "[INFO] 56e capped: state=" << int(capped.convergenceState())
                  << " xc=" << rc[2] << "\n";
        CHECK_COND(!capped.converged()
                           && capped.convergenceState() == Fit::MaxIterationsReached,
                   "56e: hitting the iteration limit must not be reported as converged");
        CHECK_COND(std::isfinite(rc[2]) && std::isfinite(capped.chiSquare())
                           && capped.chiSquare() >= 0.0,
                   "56e: last iterate and chi^2 must be kept for a non-converged fit");
        CHECK_COND(mw->logInfo.contains(capped.convergenceMessage()),
                   "56e: log must state the non-convergence");
        CHECK_COND(capped.legendInfo().startsWith(capped.convergenceMessage()),
                   "56e: legend must state the non-convergence");
        CHECK_COND(g->curves() >= 2, "56e: provisional fit curve must still be drawn");

        NonLinearFit full(mw, g);
        makeFit(full);
        full.fit();
        CHECK_COND(full.converged() && approxEq(full.results()[2], xc1, 1e-6)
                           && approxEq(std::fabs(full.results()[3]), s1, 1e-6),
                   "56e: the same model must converge with enough iterations");

        NonLinearFit simplex(mw, g);
        makeFit(simplex);
        simplex.setAlgorithm(Fit::NelderMeadSimplex);
        simplex.setMaximumIterations(5);
        simplex.fit();
        CHECK_COND(!simplex.converged() && std::isfinite(simplex.results()[2]),
                   "56e: Simplex hitting the iteration limit must be flagged");
        mw->writeFitResultsToLog = false;
    }

    // ---- 56f. Apparent (log) linear fit: excluded points counted, shortage explained ------
    {
        Table *t = mw->newTable(6, 3, "S56App", "Suite 56 apparent");
        t->setColName(0, "X");
        t->setColName(1, "Y");
        t->setColName(2, "Z");
        const double ys[6] = { 10.0, -1.0, 100.0, 0.0, 1000.0, -5.0 };
        const double zs[6] = { 10.0, -1.0, -2.0, 0.0, -3.0, -5.0 };
        for (int r = 0; r < 6; ++r) {
            t->setCell(r, 0, double(r) + 1.0);
            t->setCell(r, 1, ys[r]);
            t->setCell(r, 2, zs[r]);
        }
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Scatter);
        CHECK_COND(ml && ml->activeGraph(), "56f: graph not created");
        Graph *g = ml->activeGraph();
        g->setScale(QwtPlot::yLeft, 0.1, 10000.0, 0.0, 8, 8, 1, false);
        LinearFit app(mw, g);
        app.setApparentFit(true);
        app.setDataCurve(0, 1.0, 6.0);
        app.fit();
        std::cout << "[INFO] 56f: notes=" << app.fitNotes().join(" | ").toStdString() << "\n";
        CHECK_COND(approxEq(app.results()[1], 0.5, 1e-10) && approxEq(app.results()[0], 0.5, 1e-10),
                   "56f: log-space line through the positive points expected");
        CHECK_COND(app.fitNotes().size() == 1 && app.fitNotes().first().contains("3")
                           && app.fitNotes().first().contains("6"),
                   "56f: the 3 excluded points must be reported");

        MultiLayer *ml2 = mw->multilayerPlot(t, QStringList() << (t->name() + "_Z"), Graph::Scatter);
        Graph *g2 = ml2 ? ml2->activeGraph() : nullptr;
        CHECK_COND(g2, "56f: second graph not created");
        g2->setScale(QwtPlot::yLeft, 0.1, 10000.0, 0.0, 8, 8, 1, false);
        LinearFit shortFit(mw, g2);
        shortFit.setApparentFit(true);
        shortFit.setDataCurve(0, 1.0, 6.0);
        shortFit.fit();
        CHECK_COND(std::isnan(shortFit.results()[1]) && std::isnan(shortFit.rSquare()),
                   "56f: too few positive points must give NaN results and NaN R^2 (not 0)");
    }

    // ---- 56g. Fit dialog model search -----------------------------------------------------
    {
        FitDialog *fd = new FitDialog(mw);
        QLineEdit *search = fd->findChild<QLineEdit *>(QStringLiteral("fitModelSearchBox"));
        CHECK_COND(search && search->completer(), "56g: model search box missing");
        search->completer()->setCompletionPrefix(QStringLiteral("arrhen"));
        CHECK_COND(search->completer()->completionCount() >= 1,
                   "56g: search must find the Arrhenius model by part of its name");
        CHECK_COND(fd->jumpToModel(QStringLiteral("pseudovoigt1")),
                   "56g: jumping to a model by (case-insensitive) name failed");
        CHECK_COND(!fd->jumpToModel(QStringLiteral("NoSuchModel")),
                   "56g: unknown model name must not be accepted");
        delete fd;
    }

    // ---- 56h. Surface fit: converged flag no longer forced by R^2 > 0.99 ------------------
    {
        const int side = 15;
        Table *xyz = mw->newTable("S56Surf", side * side, 3);
        xyz->setColName(0, "X");
        xyz->setColPlotDesignation(0, SciDAVis::X);
        xyz->setColName(1, "Y");
        xyz->setColPlotDesignation(1, SciDAVis::Y);
        xyz->setColName(2, "Z");
        xyz->setColPlotDesignation(2, SciDAVis::Z);
        int row = 0;
        for (int r = 0; r < side; ++r) {
            for (int c = 0; c < side; ++c) {
                const double x = -3.0 + 6.0 * c / (side - 1.0);
                const double y = -3.0 + 6.0 * r / (side - 1.0);
                xyz->setCell(row, 0, x);
                xyz->setCell(row, 1, y);
                xyz->setCell(row, 2, 1.0 + 2.0 * x - 0.5 * y);
                ++row;
            }
        }
        auto res = GraphStatsAndSurfaceFit::fitSurfaceXYZ(
                mw, xyz, "X", "Y", "Z", GraphStatsAndSurfaceFit::Plane2D, QString(), 10);
        CHECK_COND(res.converged && res.convergenceNote.isEmpty(),
                   "56h: exact plane fit must converge without a note");
    }

    // ---- 56i. Model comparison ranks converged fits only (review fix 2026-11-22) -----------
    {
        const int n = 121;
        Table *t = mw->newTable(n, 2, "S56Cmp", "Suite 56 model comparison");
        t->setColName(0, "X");
        t->setColName(1, "Y");
        for (int r = 0; r < n; ++r) {
            const double x = 20.0 + 0.1 * r;
            t->setCell(r, 0, x);
            t->setCell(r, 1, 2.0 + 40.0 * std::exp(-0.5 * std::pow((x - 26.0) / 1.5, 2)));
        }
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Scatter);
        CHECK_COND(ml && ml->activeGraph(), "56i: graph not created");
        Graph *g = ml->activeGraph();
        // One iteration: the non-linear models cannot converge, the linear ones always do.
        Table *cmp = Fit::compareModels(mw, g, t->name() + "_Y",
                                        QStringList() << "GaussAmp" << "Linear" << "Lorentz"
                                                      << "Quadratic",
                                        1);
        CHECK_COND(cmp != nullptr && cmp->numRows() == 4 && cmp->numCols() == 12,
                   "56i: comparison table must list all 4 models with a Convergence column");
        int nConv = 0;
        bool orderOk = true, unconvBlank = true, rankOk = true;
        for (int r = 0; r < cmp->numRows(); ++r) {
            const bool rowConv = !cmp->column(10)->isInvalid(r);
            std::cout << "[INFO] 56i row " << r << ": " << cmp->column(0)->textAt(r).toStdString()
                      << " AIC=" << cmp->cell(r, 5) << " conv="
                      << cmp->column(11)->textAt(r).toStdString() << "\n";
            if (rowConv) {
                orderOk = orderOk && (nConv == r); // converged rows first
                rankOk = rankOk && cmp->cell(r, 10) == double(r + 1);
                ++nConv;
            } else {
                unconvBlank = unconvBlank && cmp->column(8)->isInvalid(r)
                        && cmp->column(9)->isInvalid(r) && !cmp->column(11)->textAt(r).isEmpty();
            }
        }
        double wSum = 0.0;
        for (int r = 0; r < nConv; ++r)
            wSum += cmp->cell(r, 9);
        CHECK_COND(nConv == 2 && orderOk && rankOk && unconvBlank,
                   "56i: only the converged fits may be ranked; the others need blank ΔAIC/weight/rank and a note");
        CHECK_COND(std::fabs(cmp->cell(0, 8)) < 1e-12 && std::fabs(wSum - 1.0) < 1e-12,
                   "56i: ΔAIC and Akaike weights must be computed among the converged fits");
    }

    // ---- 56j. ROI quick fit: a stalled LM is "no further improvement", not "max iterations"
    {
        const int n = 41;
        Table *t = mw->newTable(n, 2, "S56RoiStall", "Suite 56 ROI stall");
        t->setColName(0, "X");
        t->setColName(1, "Y");
        for (int r = 0; r < n; ++r) {
            t->setCell(r, 0, double(r));
            t->setCell(r, 1, 2.0 * r + 10.0); // a straight line: a Gaussian has no finite optimum
        }
        MultiLayer *ml = mw->multilayerPlot(t, QStringList() << (t->name() + "_Y"), Graph::Scatter);
        CHECK_COND(ml && ml->activeGraph(), "56j: graph not created");
        Graph *g = ml->activeGraph();
        RoiGadgetTool *roi = RoiGadgetTool::attachGadget(mw, g, RoiGadgetTool::QuickFit);
        CHECK_COND(roi != nullptr, "56j: attach gadget");
        roi->setTargetCurve(t->name() + "_Y");
        Fit::registerJapaneseStrings();
        const QString maxIterMsg =
                QObject::tr("Not converged (maximum iterations reached; check the initial values)");
        const QString stalledMsg =
                QObject::tr("Not converged (no further improvement; check the initial values)");
        const RoiGadgetTool::QuickFitModel models[] = { RoiGadgetTool::FitGauss,
                                                        RoiGadgetTool::FitLorentz,
                                                        RoiGadgetTool::FitBoltzmann };
        bool anyMaxIter = false, gaussStalled = false;
        for (RoiGadgetTool::QuickFitModel m : models) {
            roi->setQuickFitModel(m);
            roi->setRoiRect(QRectF(0.0, 0.0, 40.0, 100.0));
            const QString s = roi->summaryText();
            std::cout << "[INFO] 56j model " << int(m) << ": "
                      << s.simplified().toStdString() << "\n";
            anyMaxIter = anyMaxIter || s.contains(maxIterMsg);
            if (m == RoiGadgetTool::FitGauss)
                gaussStalled = s.contains(stalledMsg);
        }
        RoiGadgetTool::clearGadget(g);
        // Before the fix the loop ignored GSL_ENOPROG, ran on to the 250-iteration limit and the
        // Gaussian (centre ~2e6, no finite optimum) was shown as converged.
        CHECK_COND(!anyMaxIter && gaussStalled,
                   "56j: a stalled LM fit (GSL_ENOPROG) must stop and be reported as 'no further improvement'");
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 56 Fit Convergence, Peak Width Convention & Statistics passed!\n";
    return 0;
}

// =============================================================================
// =========================================================================
// Suite 59: Data import & worksheet (D2 paste, B8 drop parser, B3, D5, D6, D10, D3)
// =========================================================================
#include "table/AsciiTableImportFilter.h"
#include "table/future_Table.h"
#include "ImportASCIIDialog.h"
#include <QDropEvent>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

int runSuite59DataImportAndWorksheet(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 59: Data Import & Worksheet (paste, drop parser, "
                 "formulas, CSV export)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 59");
    qApp->setProperty("torigraph_noninteractive", true);
    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "59: QTemporaryDir failed");

    // ---- 59a (D2): menu paste == Ctrl+V: tab first, no split at spaces, header/unit rows
    {
        // This check is about the clipboard path, so it does use the system clipboard. Another
        // program may replace the clipboard text at any time: the text is read back before the
        // paste, and only if the clipboard changed meanwhile and now belongs to another process is
        // the paste repeated on a fresh table.
        const QString text59a = QStringLiteral("Nominal strain\tNominal stress\n"
                                               "%\tMPa\n"
                                               "0.0\t0.0\n"
                                               "2.0\t480.0\n"
                                               "4.0\t518.0\n");
        Table *t = nullptr;
        bool clipOk = false;
        for (int attempt = 0; attempt < 8 && !clipOk; ++attempt) {
            t = mw->newTable(attempt == 0 ? QStringLiteral("Paste59a")
                                          : QStringLiteral("Paste59a_r%1").arg(attempt),
                             4, 2);
            CHECK_COND(t != nullptr, "59a: table");
            t->setCellsSelected(0, 0, 0, 0, true);
            if (!setClipboardTextVerified(text59a)) {
                std::cout << "[INFO] 59a: could not place the test text on the clipboard, trying again\n";
                continue;
            }
            markClipboardAfterCopy();
            t->d_future_table->pasteIntoSelection(); // the Edit menu / context menu path
            DWORD ownerPid = 0;
            if (!clipboardTakenByOtherProcess(&ownerPid)) {
                CHECK_COND(clipboardStillHolds(text59a), "59a: the paste changed the clipboard text");
                clipOk = true;
                break;
            }
            std::cout << "[INFO] 59a: the clipboard changed during the paste and is now owned by process "
                      << ownerPid << ", pasting again into a fresh table\n";
        }
        CHECK_COND(clipOk, "59a: could not keep the test text on the clipboard");
        CHECK_COND(t->numCols() == 2, "59a: menu paste split 'Nominal stress' into two columns");
        CHECK_COND(t->column(1)->longName() == "Nominal stress" && t->column(1)->units() == "MPa",
                   "59a: header/unit rows not promoted on the menu paste path");
        CHECK_COND(std::fabs(t->cell(1, 1) - 480.0) < 1e-12 && std::fabs(t->cell(2, 0) - 4.0) < 1e-12,
                   "59a: pasted values wrong");
    }
    // leading empty cells are kept in place (row starts with a tab)
    {
        Table *t = mw->newTable("Paste59b", 3, 3);
        t->setCellsSelected(1, 0, 1, 0, true);
        CHECK_COND(t->pasteText("\t2\t3\n1\t\t6\n"), "59a2: pasteText failed");
        CHECK_COND(t->column(0)->isInvalid(1) && std::fabs(t->cell(1, 1) - 2.0) < 1e-12
                           && std::fabs(t->cell(1, 2) - 3.0) < 1e-12,
                   "59a2: leading empty cell was dropped (values shifted left)");
        CHECK_COND(std::fabs(t->cell(2, 0) - 1.0) < 1e-12 && t->column(1)->isInvalid(2)
                           && std::fabs(t->cell(2, 2) - 6.0) < 1e-12,
                   "59a2: inner empty cell not kept");
    }
    // a large paste is one Undo step made of a few column-wise commands
    {
        Table *t = mw->newTable("Paste59c", 2, 2);
        QString big;
        for (int r = 0; r < 5000; ++r)
            big += QString::number(r) + "\t" + QString::number(r * 0.5, 'g', 17)
                    + ((r % 1000 == 7) ? QString("\tNaN") : QString("\t1")) + "\n";
        QUndoStack *us = t->d_future_table->undoStack();
        CHECK_COND(us != nullptr, "59a3: no undo stack");
        const int idx0 = us->index();
        t->setCellsSelected(0, 0, 0, 0, true);
        CHECK_COND(t->pasteText(big), "59a3: big paste failed");
        CHECK_COND(t->numRows() == 5000 && t->numCols() == 3, "59a3: table not expanded to 5000x3");
        CHECK_COND(std::fabs(t->cell(4999, 1) - 2499.5) < 1e-12 && t->column(2)->isInvalid(7),
                   "59a3: values / NaN cell wrong");
        CHECK_COND(us->index() == idx0 + 1, "59a3: paste was not exactly one Undo step");
        const QUndoCommand *cmd = us->command(us->index() - 1);
        std::cout << "[INFO] 59a3: 5000x3 paste = " << cmd->childCount() << " undo child commands\n";
        CHECK_COND(cmd->childCount() < 60, "59a3: paste still pushes one command per cell");
        us->undo();
        CHECK_COND(t->numRows() == 2 && t->numCols() == 2, "59a3: undo did not restore the table");
        us->redo();
        CHECK_COND(t->numRows() == 5000 && std::fabs(t->cell(4999, 1) - 2499.5) < 1e-12,
                   "59a3: redo failed");
    }
    // transposed paste
    {
        Table *t = mw->newTable("Paste59d", 2, 2);
        t->setCellsSelected(0, 0, 0, 0, true);
        CHECK_COND(t->pasteText("1\t2\t3\n4\t5\t6\n", true), "59a4: transposed paste failed");
        CHECK_COND(t->numRows() == 3 && std::fabs(t->cell(2, 0) - 3.0) < 1e-12
                           && std::fabs(t->cell(0, 1) - 4.0) < 1e-12,
                   "59a4: transposed paste wrong");
    }

    // ---- 59b (B8): import filter
    {
        // unit row outside the unit dictionary
        const QByteArray csv = "Load,Stress,Speed\nkgf,N/mm2,mm/min\n1,2.5,3\n4,5.5,6\n";
        AsciiTableImportFilter::ImportReport rep;
        std::unique_ptr<future::Table> ft(
                AsciiTableImportFilter::importDroppedTextData(csv, "u", "u.csv", &rep));
        CHECK_COND(ft && ft->columnCount() == 3 && ft->rowCount() == 2, "59b1: shape");
        CHECK_COND(ft->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && ft->column(1)->units() == "N/mm2" && rep.unitsRowDetected,
                   "59b1: unknown unit row turned the columns into text");
        CHECK_COND(std::fabs(ft->column(1)->valueAt(1) - 5.5) < 1e-12, "59b1: value");
    }
    {
        // metadata preamble above name/unit rows (instrument export)
        const QByteArray txt = "Instrument: Potentiostat X\nDate: 2026/11/22\nOperator: test\n"
                               "Time\tVoltage\ns\tV\n0\t1.0\n1\t1.5\n2\t\n3\t2.5\n";
        AsciiTableImportFilter::ImportReport rep;
        std::unique_ptr<future::Table> ft(
                AsciiTableImportFilter::importDroppedTextData(txt, "p", "p.mpt", &rep));
        CHECK_COND(ft && ft->columnCount() == 2 && ft->rowCount() == 4, "59b2: shape");
        CHECK_COND(ft->column(0)->name() == "Time" && ft->column(1)->units() == "V",
                   "59b2: header/unit rows below the preamble not used");
        CHECK_COND(rep.skippedLines == 3 && rep.missingCells == 1, "59b2: report counts");
        CHECK_COND(ft->column(1)->isInvalid(2) && std::fabs(ft->column(1)->valueAt(3) - 2.5) < 1e-12,
                   "59b2: missing cell not kept as an empty cell");
        std::cout << "[INFO] 59b2 report: "
                  << AsciiTableImportFilter::describeImportReport(rep).toStdString() << "\n";
    }
    {
        // Rigaku .ras: two scans, a row with a missing value
        const QByteArray ras = "*RAS_DATA_START\n*RAS_HEADER_START\n*MEAS_SCAN_UNIT_X \"deg.\"\n"
                               "*RAS_HEADER_END\n*RAS_INT_START\n10.0 100 1\n10.1 \n10.2 120 1\n"
                               "*RAS_INT_END\n*RAS_HEADER_START\n*RAS_HEADER_END\n*RAS_INT_START\n"
                               "20.0 7 1\n20.1 8 1\n*RAS_INT_END\n*RAS_DATA_END\n";
        AsciiTableImportFilter::ImportReport rep;
        std::unique_ptr<future::Table> ft(
                AsciiTableImportFilter::importDroppedTextData(ras, "r", "r.ras", &rep));
        CHECK_COND(ft && rep.rasBlocks == 2, "59b3: second RAS scan was cut off");
        CHECK_COND(ft->columnCount() == 6 && ft->column(3)->name() == "TwoTheta_2"
                           && std::fabs(ft->column(4)->valueAt(1) - 8.0) < 1e-12,
                   "59b3: second scan columns");
        CHECK_COND(ft->column(0)->rowCount() >= 3 && ft->column(1)->isInvalid(1)
                           && std::fabs(ft->column(0)->valueAt(1) - 10.1) < 1e-12
                           && std::fabs(ft->column(1)->valueAt(2) - 120.0) < 1e-12,
                   "59b3: row with a missing value was dropped or filled with 0");
    }

    // ---- 59c (B8/B3): drag & drop uses the filter, never the last dialog settings
    {
        const int oldIgnored = mw->ignoredLines;
        mw->ignoredLines = 2; // a leftover from the import dialog must not be applied
        const QString csvPath = tmp.filePath("drop59.csv");
        CHECK_COND(suite59WriteFile(csvPath, "Strain,Stress\n%,MPa\n0,0\n1,100\n2,180\n"),
                   "59c: write csv");
        const QList<MyWidget *> beforeWins = mw->windowsList();
        suite59Drop(mw, QStringList() << csvPath);
        mw->ignoredLines = oldIgnored;
        Table *t = nullptr;
        for (MyWidget *w : mw->windowsList()) {
            if (!beforeWins.contains(w))
                t = qobject_cast<Table *>(w);
        }
        CHECK_COND(t != nullptr, "59c: csv drop created no table");
        CHECK_COND(t && t->numRows() == 3 && t->column(1)->units() == "MPa"
                           && std::fabs(t->cell(2, 1) - 180.0) < 1e-12,
                   "59c: dropped csv imported wrongly (dialog settings leaked?)");
        CHECK_COND(mw->property("torigraph_last_drop_import_summary").toString().contains("drop59.csv"),
                   "59c: no import summary for the drop");

        const QString xlsxPath = tmp.filePath("book59.xlsx");
        CHECK_COND(suite59WriteFile(xlsxPath, QByteArray("PK\x03\x04", 4) + QByteArray(64, 'x')),
                   "59c: write xlsx");
        mw->setProperty("torigraph_last_drop_unsupported_message", QVariant());
        const int before2 = mw->windowsList().size();
        suite59Drop(mw, QStringList() << xlsxPath);
        const QString msg = mw->property("torigraph_last_drop_unsupported_message").toString();
        CHECK_COND(mw->windowsList().size() == before2, "59c: xlsx drop created a window");
        CHECK_COND(msg.contains("CSV") && msg.contains("book59.xlsx"),
                   "59c: no copy-paste / CSV hint for an .xlsx drop");
    }

    // ---- 59d (D5): dependency order, circular references, error marks
    {
        Table *t = mw->newTable("Rc59", 3, 9);
        CHECK_COND(t != nullptr && t->numCols() == 9, "59d: table");
        const char *names[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I" };
        for (int c = 0; c < 9; ++c)
            t->setColName(c, names[c]);
        t->setAutoRecalcFormulas(true);
        for (int r = 0; r < 3; ++r)
            t->setCell(r, 0, r + 1.0);
        // reverse chain B <- C <- D <- E <- F <- G <- A: six steps, listed backwards
        const char *chain[] = { "col(\"C\")+1", "col(\"D\")+1", "col(\"E\")+1",
                                "col(\"F\")+1", "col(\"G\")+1", "col(\"A\")+1" };
        for (int k = 0; k < 6; ++k)
            t->column(1 + k)->setFormula(Interval<int>(0, 2), chain[k]);
        t->column(7)->setFormula(Interval<int>(0, 2), "col(\"I\")+1"); // H <-> I circular
        t->column(8)->setFormula(Interval<int>(0, 2), "col(\"H\")+1");
        t->setCell(0, 0, 10.0); // triggers the automatic recalculation
        QApplication::processEvents();
        std::cout << "[INFO] 59d: B(0) = " << t->cell(0, 1) << " (expected 16)\n";
        CHECK_COND(std::fabs(t->cell(0, 1) - 16.0) < 1e-12 && std::fabs(t->cell(2, 1) - 9.0) < 1e-12,
                   "59d: reverse dependency chain not brought up to date");
        const QList<int> errs = t->formulaErrorColumns();
        CHECK_COND(errs.contains(7) && errs.contains(8) && !errs.contains(1),
                   "59d: circular reference not detected / wrong column marked");
        const QString tip =
                t->d_future_table->headerData(7, Qt::Horizontal, Qt::ToolTipRole).toString();
        CHECK_COND(!t->column(7)->formulaError().isEmpty() && tip.contains(t->column(7)->formulaError()),
                   "59d: error not shown on the column header");
        // fixing the cycle clears the mark
        t->column(8)->setFormula(Interval<int>(0, 2), "col(\"A\")*2");
        t->setCell(1, 0, 3.0);
        QApplication::processEvents();
        CHECK_COND(t->formulaErrorColumns().isEmpty(), "59d: error mark not cleared after fix");
        CHECK_COND(std::fabs(t->cell(0, 7) - 21.0) < 1e-12, "59d: H after fixing the cycle");
    }

    // ---- 59e (D6): CSV export
    {
        Table *t = mw->newTable("Exp59", 3, 2);
        t->setColName(0, "Time");
        t->setColName(1, "Load");
        t->column(0)->setLongName(QString::fromUtf8("\xE6\x99\x82\xE9\x96\x93")); // 時間
        t->column(0)->setUnits("s");
        t->column(1)->setUnits("N");
        t->setCell(0, 0, 0.1);
        t->setCell(1, 0, 1.0 / 3.0);
        t->setCell(2, 0, 123456789.123456789);
        t->setCell(0, 1, 1e-300);
        t->setCell(1, 1, -2.5);
        t->setCell(2, 1, 3.0);
        Table::AsciiExportOptions opt;
        opt.utf8Bom = true;
        opt.longNameAndUnits = true;
        const QString out = tmp.filePath("exp59.csv");
        CHECK_COND(t->exportASCII(out, ",", true, false, opt), "59e: export failed");
        const QByteArray bytes = suite59ReadAll(out);
        CHECK_COND(bytes.startsWith("\xEF\xBB\xBF"), "59e: BOM missing");
        const QStringList lines = QString::fromUtf8(bytes.mid(3)).split('\n', Qt::SkipEmptyParts);
        CHECK_COND(lines.size() == 5, "59e: expected long-name row, unit row and 3 data rows");
        CHECK_COND(lines[0] == QString::fromUtf8("\xE6\x99\x82\xE9\x96\x93,Load") && lines[1] == "s,N",
                   "59e: header rows wrong");
        CHECK_COND(lines[2] == "0.1,1e-300", "59e: shortest round-trip text not used");
        const double third = lines[3].section(',', 0, 0).toDouble();
        const double big = lines[4].section(',', 0, 0).toDouble();
        CHECK_COND(third == 1.0 / 3.0 && big == 123456789.123456789,
                   "59e: exported numbers do not read back exactly (precision lost)");
        // default (old API): no BOM, short names
        CHECK_COND(t->exportASCII(out, "\t", true, false), "59e: plain export failed");
        CHECK_COND(suite59ReadAll(out).startsWith("Time\tLoad\n"), "59e: plain export header");
        // write errors are reported
        CHECK_COND(!t->exportASCII(tmp.filePath("no_dir/x.csv"), ",", true, false)
                           && !t->lastExportError().isEmpty(),
                   "59e: export into a missing folder reported success");
    }

    // ---- 59f (D10): overwrite import copies Long Name/units, type mismatch is refused
    {
        Table *t = mw->newTable("Imp59", 2, 2);
        t->setCell(0, 0, 9.0);
        t->setCell(0, 1, 9.0);
        mw->d_workspace.setActiveSubWindow(t);
        const QString f1 = tmp.filePath("imp59.csv");
        CHECK_COND(suite59WriteFile(f1, "Time (s),Load (N)\n1,2\n3,4\n5,6\n"), "59f: write");
        mw->importASCII(QStringList() << f1, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                        false, true, QLocale::c());
        CHECK_COND(t->numRows() == 3 && std::fabs(t->cell(2, 1) - 6.0) < 1e-12,
                   "59f: overwrite import values");
        CHECK_COND(t->column(1)->units() == "N" && t->column(0)->longName() == "Time",
                   "59f: Long Name / units not copied by the overwrite import");
        const QString f2 = tmp.filePath("imp59_text.csv");
        CHECK_COND(suite59WriteFile(f2, "x,y\nabc,1\ndef,2\n"), "59f: write 2");
        mw->setProperty("torigraph_last_import_error", QVariant());
        mw->importASCII(QStringList() << f2, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                        false, true, QLocale::c());
        CHECK_COND(!mw->property("torigraph_last_import_error").toString().isEmpty(),
                   "59f: text column imported into a numeric column without an error");
        CHECK_COND(std::fabs(t->cell(0, 0) - 1.0) < 1e-12, "59f: refused import changed the table");
    }

    // ---- 59g (D3): fill down and replace
    {
        Table *t = mw->newTable("Keys59", 4, 2);
        t->setCell(0, 0, 7.5);
        t->setCellsSelected(0, 0, 3, 0, true);
        t->fillSelectionDown();
        CHECK_COND(std::fabs(t->cell(3, 0) - 7.5) < 1e-12, "59g: fill down");
        t->deselectAll();
        t->setCell(2, 1, 480.0);
        const int n = t->replaceAllCells("480", "481", false, true, false);
        CHECK_COND(n == 1 && std::fabs(t->cell(2, 1) - 481.0) < 1e-12, "59g: replace");
        t->goToCell(0, 0);
        CHECK_COND(t->findNextCell("481", false, false), "59g: find");
    }

    // ---- 59h (review fix): a text-only CSV is still imported as a table, without a graph
    {
        const QString txtCsv = tmp.filePath("samples59.csv");
        CHECK_COND(suite59WriteFile(txtCsv, "Sample,Condition\nA1,T6\nA2,RRA\nA3,O\n"),
                   "59h: write");
        const QList<MyWidget *> beforeWins = mw->windowsList();
        suite59Drop(mw, QStringList() << txtCsv);
        Table *t = nullptr;
        int newGraphs = 0;
        for (MyWidget *w : mw->windowsList()) {
            if (beforeWins.contains(w))
                continue;
            if (qobject_cast<Table *>(w))
                t = qobject_cast<Table *>(w);
            else if (w->inherits("MultiLayer"))
                ++newGraphs;
        }
        CHECK_COND(t != nullptr, "59h: text-only CSV drop was refused");
        CHECK_COND(t && t->numRows() == 3 && t->numCols() == 2
                           && t->column(1)->columnMode() == SciDAVis::ColumnMode::Text
                           && t->text(2, 1) == "O",
                   "59h: text-only CSV imported wrongly");
        CHECK_COND(newGraphs == 0, "59h: a graph was made from a table without numbers");
        // other files still need numbers to count as data
        const QString txtLog = tmp.filePath("notes59.log");
        CHECK_COND(suite59WriteFile(txtLog, "just,some\nwords,here\n"), "59h: write log");
        const int nBefore = mw->windowsList().size();
        suite59Drop(mw, QStringList() << txtLog);
        CHECK_COND(mw->windowsList().size() == nBefore, "59h: text-only .log was imported");
    }

    // ---- 59i (review fix): a multi-file append stopped by a type mismatch still marks the
    // project as modified (what was imported before the mismatch stays and must be saved)
    {
        Table *t = mw->newTable("Part59", 2, 2);
        t->setCell(0, 0, 1.0);
        t->setCell(0, 1, 2.0);
        mw->d_workspace.setActiveSubWindow(t);
        const QString f1 = tmp.filePath("part59_a.csv");
        const QString f2 = tmp.filePath("part59_b.csv");
        CHECK_COND(suite59WriteFile(f1, "x,y\n10,20\n30,40\n"), "59i: write 1");
        CHECK_COND(suite59WriteFile(f2, "x,y\nabc,1\ndef,2\n"), "59i: write 2");
        mw->markProjectSaved();
        CHECK_COND(mw->saved, "59i: precondition");
        mw->setProperty("torigraph_last_import_error", QVariant());
        mw->importASCII(QStringList() << f1 << f2, ImportASCIIDialog::NewRows, ",", 0, true, true,
                        false, true, QLocale::c());
        CHECK_COND(!mw->property("torigraph_last_import_error").toString().isEmpty(),
                   "59i: the mismatching second file was not reported");
        CHECK_COND(t->numRows() == 4 && std::fabs(t->cell(3, 1) - 40.0) < 1e-12,
                   "59i: rows of the first file missing");
        CHECK_COND(!mw->saved, "59i: partial import did not mark the project as modified");
    }

    // ---- 59j (review fix): preview and detection result are visible with the options folded
    {
        ImportASCIIDialog dlg(true, mw, false);
        QPlainTextEdit *preview = dlg.findChild<QPlainTextEdit *>("ImportPreview");
        QLabel *summary = dlg.findChild<QLabel *>("ImportDetectionSummary");
        CHECK_COND(preview && summary, "59j: preview widgets missing");
        CHECK_COND(preview->isVisibleTo(&dlg) && summary->isVisibleTo(&dlg),
                   "59j: preview is hidden while the advanced options are folded");
        bool insideGroup = false;
        for (QWidget *p = preview->parentWidget(); p && p != &dlg; p = p->parentWidget())
            if (qobject_cast<QGroupBox *>(p))
                insideGroup = true;
        CHECK_COND(!insideGroup, "59j: preview is still inside the advanced options box");
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 59 Data Import & Worksheet passed!\n";
    return 0;
}

// =============================================================================
// Suite 63: Japanese dictionary (TSV resource, context-aware lookup, glossary), UI
// readability/keyboard access, inspector minor ticks / ranges / tooltips / multi-line
// annotation text, per-line legend sub/superscript (review items H1-H7, C1, C16).
// =============================================================================
#include "PlotStyleLimits.h"
#include "LayerDialog.h"
#include <QAbstractButton>
#include <QDoubleSpinBox>
#include <QSet>
#include <QSpinBox>

int runSuite63JapaneseUiAccessibilityGate(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 63: Japanese dictionary, UI readability & keyboard gate...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 63");
    const QString u8 = QString(); // keeps the macros below readable
    Q_UNUSED(u8)
    auto ja = [](const char *s) { return QString::fromUtf8(s); };

    // ---- 63a: the dictionary is loaded from the UTF-8 TSV resource (H2)
    CHECK_COND(ToriGraphFullJapaneseTranslator::builtinDictionarySize() > 5800,
               "63a: built-in dictionary resource not loaded (too few entries)");
    CHECK_COND(ToriGraphFullJapaneseTranslator::contextDictionarySize() >= 20,
               "63a: context-specific dictionary rows missing");
    CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese("&File").startsWith(ja("ファイル")),
               "63a: basic lookup from TSV failed");
    // escapes in the TSV: leading space (\s) and newline (\n)
    CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese(" Area") == ja(" 面グラフ"),
               "63a: leading-space key from TSV not decoded");
    CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese("\nReleased") == ja("\nリリース日"),
               "63a: newline escape from TSV not decoded");

    // ---- 63b: two-stage lookup, context first (H1). Contexts are the real class names.
    {
        ToriGraphFullJapaneseTranslator t;
        CHECK_COND(QString::fromLatin1(LayerDialog::staticMetaObject.className()) == "LayerDialog",
                   "63b: unexpected LayerDialog context name");
        CHECK_COND(QString::fromLatin1(MultiPeakFit::staticMetaObject.className()) == "MultiPeakFit",
                   "63b: unexpected MultiPeakFit context name");
        struct Case {
            const char *ctx;
            const char *en;
            const char *expected;
        };
        const Case cases[] = {
            { "LayerDialog", "Center", "中央揃え" },
            { "TextDialog", "Left", "左揃え" },
            { "TextDialog", "&Left", "左揃え(&L)" }, // mnemonic re-attached
            { "MultiPeakFit", "Center", "中心" },
            { "SomeOtherClass", "Center", "中央" }, // context-free default
            { "CurvesDialog", "Area", "面グラフ" },
            { "GraphInspectorDock", "Area", "面グラフ" },
            { "Integration", "Area", "面積" },
            { "PatternBox", "Solid", "塗りつぶし" },
            { "Graph", "Solid", "実線" },
            { "PenWidget", "Square", "角端" },
            { "AxesDialog", "Bottom", "下軸" },
            { "LayerDialog", "Bottom", "下揃え" },
            { "RangeSelectorTool", "Right", "右" },
            { "PlotDialog", "Current", "現在の設定" },
            { "ApplicationWindow", "&All", "すべて(&A)" },
            { "ApplicationWindow", "&Cut", "切り取り(&T)" },  // no clash with コピー(&C)
            { "ApplicationWindow", "&Copy", "コピー(&C)" },
            { "GraphStatsAndSurfaceFit", "Average", "群平均" },
            { "SomeOtherClass", "Average", "平均" },
        };
        for (const Case &c : cases) {
            const QString got = t.translate(c.ctx, c.en);
            if (got != ja(c.expected)) {
                std::cerr << "[FAIL] 63b: translate(" << c.ctx << ", " << c.en << ") = '"
                          << got.toUtf8().constData() << "', expected '" << c.expected << "'\n";
                return 1;
            }
        }
        // Mini toolbar shape names are registered for its own context only
        GraphMiniToolbar::instance();
        CHECK_COND(t.translate("GraphMiniToolbar", "Square") == ja("四角 (■)"),
                   "63b: mini-toolbar 'Square' not context-specific");
        CHECK_COND(t.translate("GraphMiniToolbar", "Cross") == ja("バツ (×)"),
                   "63b: mini-toolbar 'Cross' not context-specific");
        CHECK_COND(t.translate("PatternBox", "Cross") == ja("格子"),
                   "63b: fill pattern 'Cross' was overridden by the mini toolbar");
        CHECK_COND(t.translate("PenWidget", "Square") == ja("角端"),
                   "63b: pen cap 'Square' was overridden by the mini toolbar");
    }

    // ---- 63c: glossary normalisation (H3)
    {
        using T = ToriGraphFullJapaneseTranslator;
        CHECK_COND(T::normalizeGlossary("Active Layer", ja("現在のレイヤ")) == ja("現在のレイヤー"),
                   "63c: レイヤ -> レイヤー");
        CHECK_COND(T::normalizeGlossary("x", ja("目盛と目盛り")) == ja("目盛りと目盛り"),
                   "63c: 目盛 -> 目盛り");
        CHECK_COND(T::normalizeGlossary("x", ja("線幅（太さ）：")) == ja("線幅(太さ):"),
                   "63c: full-width brackets/colon -> half-width");
        CHECK_COND(T::normalizeGlossary("OK", ja("OK (決定)")) == "OK", "63c: OK (決定) -> OK");
        CHECK_COND(T::normalizeGlossary("Legend && Text", ja("凡例 && テキスト")) == ja("凡例・テキスト"),
                   "63c: && must not remain in a translation");
        CHECK_COND(T::normalizeGlossary("Set Category Labels", ja("カテゴリ軸ラベルを設定"))
                           == ja("カテゴリ軸ラベルを設定"),
                   "63c: category (tick) labels must stay 軸ラベル");
        CHECK_COND(T::normalizeGlossary("Edit Axis Title", ja("軸ラベルを編集")) == ja("軸タイトルを編集"),
                   "63c: axis title wording");
        CHECK_COND(T::normalizeGlossary("TeX table export dialog", ja("TeX テーブル")) == ja("TeX テーブル"),
                   "63c: TeX table must stay テーブル");
        CHECK_COND(T::normalizeGlossary("New Table", ja("新規テーブル")) == ja("新規ワークシート"),
                   "63c: Table -> ワークシート");
        CHECK_COND(T::normalizeGlossary("Symbol Size:", ja("記号サイズ:")) == ja("シンボルサイズ:"),
                   "63c: plot symbol -> シンボル");
        CHECK_COND(T::normalizeGlossary("Choose Symbol", ja("特殊記号の挿入")) == ja("特殊記号の挿入"),
                   "63c: special characters must stay 記号");

        T::registerExtraTranslation("Suite63 Probe Layer", ja("試験レイヤ（目盛）"));
        CHECK_COND(T::lookupJapanese("Suite63 Probe Layer") == ja("試験レイヤー(目盛り)"),
                   "63c: registerExtraTranslation does not apply the glossary");
        // a trailing ':' added by the lookup is half-width
        const QString colon = T::lookupJapanese("Suite63 Probe Layer:");
        CHECK_COND(colon == ja("試験レイヤー(目盛り):"), "63c: suffix colon must be half-width");
        // spot checks on the built-in dictionary
        CHECK_COND(T::lookupJapanese("OK") == "OK", "63c: 'OK' still has (決定)");
        CHECK_COND(!T::lookupJapanese("Legend && Text").contains("&&"), "63c: && in dictionary");
        CHECK_COND(T::lookupJapanese("Presentation Slide (28 pt Arial)") == ja("発表スライド (28 pt Arial)"),
                   "63c: duplicate key resolved inconsistently");
        CHECK_COND(T::lookupJapanese("Left") == ja("左軸"), "63c: English echo not removed");
        CHECK_COND(T::lookupJapanese("Manage Style Presets...").startsWith(ja("グラフプリセット")),
                   "63c: 'Manage Style Presets...' not translated before first use");
    }

    // ---- 63d: per-line legend sub/superscript (C16)
    {
        const QString sup0 = QStringLiteral("<sup>");
        const QString sup1 = QStringLiteral("</sup>");
        const QString two = QStringLiteral("\\l(1)sigma\n\\l(2)epsilon");
        const QString on = GraphMiniToolbar::toggleMarkupPerLine(two, sup0, sup1);
        CHECK_COND(on == QStringLiteral("\\l(1)<sup>sigma</sup>\n\\l(2)<sup>epsilon</sup>"),
                   "63d: tags must go after the \\l(n) token on every line");
        CHECK_COND(GraphMiniToolbar::toggleMarkupPerLine(on, sup0, sup1) == two,
                   "63d: second press must restore the original text");
        const QString cTok = QStringLiteral("\\c{1} A\n\\c{2} B ");
        const QString cOn = GraphMiniToolbar::toggleMarkupPerLine(
                cTok, QStringLiteral("<sub>"), QStringLiteral("</sub>"));
        CHECK_COND(cOn == QStringLiteral("\\c{1} <sub>A</sub>\n\\c{2} <sub>B</sub> "),
                   "63d: \\c{n} prefix or trailing space not preserved");
        const QString mixed = QStringLiteral("\\l(1)<sup>a</sup>\n\\l(2)b");
        CHECK_COND(GraphMiniToolbar::toggleMarkupPerLine(mixed, sup0, sup1)
                           == QStringLiteral("\\l(1)<sup>a</sup>\n\\l(2)<sup>b</sup>"),
                   "63d: mixed lines must be completed, not double-wrapped");
        CHECK_COND(GraphMiniToolbar::toggleMarkupPerLine(QStringLiteral("\\l(1)"), sup0, sup1)
                           == QStringLiteral("\\l(1)"),
                   "63d: a line with only a symbol token must stay unchanged");
    }

    // ---- 63e: inspector (C1 minor ticks / shared ranges, H7 tooltips, C16 multi-line text)
    const QString prevLang63 = mw->appLanguage;
    mw->switchToLanguage("ja");
    QApplication::processEvents();
    GraphInspectorDock *dock = mw->graphInspectorDock();
    CHECK_COND(dock != nullptr, "63e: inspector dock missing");

    Table *t63 = mw->newTable("Ui63", 6, 2);
    CHECK_COND(t63 != nullptr, "63e: table");
    for (int r = 0; r < 6; ++r) {
        t63->setCell(r, 0, std::pow(10.0, r * 0.5));
        t63->setCell(r, 1, 2.0 * r + 1.0);
    }
    MultiLayer *ml63 = mw->multilayerPlot(t63, QStringList() << "Ui63_2", Graph::LineSymbols);
    CHECK_COND(ml63 && ml63->activeGraph(), "63e: graph");
    mw->activateSubWindow(ml63);
    ml63->show();
    QApplication::processEvents();
    Graph *g63 = ml63->activeGraph();
    dock->setTargetMultiLayer(ml63, true);

    // minor ticks follow the axis type and accept a typed number
    g63->setScale(QwtPlot::xBottom, 1.0, 1000.0, 0.0, 5, 3, 1);
    dock->refreshAll();
    QApplication::processEvents();
    QComboBox *minor = dock->findChild<QComboBox *>(QStringLiteral("InspectorMinorTicksCombo"));
    CHECK_COND(minor != nullptr, "63e: minor tick combo missing");
    CHECK_COND(minor->isEditable(), "63e: minor tick combo must be editable");
    QList<int> presets;
    for (int i = 0; i < 4 && i < minor->count(); ++i)
        presets << minor->itemData(i).toInt();
    CHECK_COND(presets == (QList<int>() << 0 << 2 << 4 << 8),
               "63e: log axis must offer 0/2/4/8 minor ticks");
    CHECK_COND(minor->itemData(minor->currentIndex()).toInt() == 3,
               "63e: current non-preset value (3) not shown");
    minor->setEditText(QStringLiteral("6"));
    emit minor->lineEdit()->editingFinished();
    QApplication::processEvents();
    CHECK_COND(g63->plotWidget()->axisMaxMinor(QwtPlot::xBottom) == 6,
               "63e: typed minor tick count not applied");
    g63->setScale(QwtPlot::xBottom, 0.0, 40.0, 0.0, 5, 4, 0);
    dock->refreshAll();
    QApplication::processEvents();
    CHECK_COND(minor->itemData(1).toInt() == 1 && minor->itemData(3).toInt() == 9,
               "63e: linear axis must offer 0/1/4/9 minor ticks");

    // ranges shared with PlotDialog: a size set elsewhere is not clamped by the inspector
    bool symSizeOk = false, edgeOk = false;
    for (QSpinBox *sb : dock->findChildren<QSpinBox *>())
        if (sb->maximum() == PlotStyleLimits::kSymbolSizeMax && sb->minimum() == PlotStyleLimits::kSymbolSizeMin)
            symSizeOk = true;
    for (QDoubleSpinBox *sb : dock->findChildren<QDoubleSpinBox *>())
        if (qFuzzyCompare(sb->maximum(), PlotStyleLimits::kSymbolEdgeWidthMax))
            edgeOk = true;
    CHECK_COND(symSizeOk && edgeOk, "63e: inspector ranges not taken from PlotStyleLimits");

    // tooltips on tabs, quick error bars, legend placement and colour swatches (H7)
    for (int p = 0; p < 4; ++p) {
        dock->switchToInspectorPage(static_cast<GraphInspectorDock::InspectorPageIndex>(p));
        QApplication::processEvents();
    }
    {
        // Phase 2 UX overhaul renamed "X/Y" -> "Axis" and replaced table-mutating "Std Dev (SD)" / "5%"
        // error-bar buttons with safe "Adjacent Col" + "Add Error Bars...".
        auto trD = [](const char *s) { return QCoreApplication::translate("GraphInspectorDock", s); };
        const QStringList needTip = QStringList() << trD("Overall") << trD("Axis") << trD("Curve")
                                                  << trD("Legend") << trD("Add Error Bars...")
                                                  << trD("Adjacent Col") << trD("Top-Left")
                                                  << trD("Outside Right");
        QSet<QString> seen;
        for (QAbstractButton *b : dock->findChildren<QAbstractButton *>()) {
            if (!needTip.contains(b->text()))
                continue;
            seen.insert(b->text());
            if (b->toolTip().trimmed().isEmpty()) {
                std::cerr << "[FAIL] 63e: button without tooltip: '" << b->text().toUtf8().constData()
                          << "'\n";
                return 1;
            }
            CHECK_COND(isUntranslatedAsciiUiLabel(b->toolTip()) == false,
                       "63e: tooltip not translated in ja mode");
        }
        CHECK_COND(seen.size() == needTip.size(), "63e: some H7 buttons were not found");
        int swatches = 0;
        for (QPushButton *b : dock->findChildren<QPushButton *>(QStringLiteral("InspectorColorSwatchBtn"))) {
            ++swatches;
            CHECK_COND(!b->toolTip().isEmpty(), "63e: colour swatch without tooltip");
        }
        CHECK_COND(swatches > 0, "63e: no colour swatches found");
    }

    // multi-line text annotation (C16)
    {
        Legend *ann = g63->newLegend(QStringLiteral("Line 1\nLine 2"));
        CHECK_COND(ann != nullptr, "63e: text annotation");
        const int annId = g63->textMarkerKeys().last();
        Graph::GraphSelection sel;
        sel.kind = Graph::GraphSelection::TextAnnotation;
        sel.primaryId = annId;
        g63->setCurrentSelection(sel);
        dock->switchToInspectorPage(GraphInspectorDock::PageTabLegend);
        dock->refreshAll();
        QApplication::processEvents();
        QPlainTextEdit *ed = dock->findChild<QPlainTextEdit *>(QStringLiteral("InspectorAnnotationTextEdit"));
        CHECK_COND(ed != nullptr, "63e: multi-line annotation editor missing");
        CHECK_COND(!ed->isHidden(), "63e: multi-line editor hidden for a text annotation");
        CHECK_COND(ed->toPlainText() == QStringLiteral("Line 1\nLine 2"),
                   "63e: editor does not show all lines");
        ed->setPlainText(QStringLiteral("Line 1\nLine 2 edited\nLine 3"));
        QElapsedTimer et;
        et.start();
        while (et.elapsed() < 3000 && ann->text() != ed->toPlainText())
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        CHECK_COND(ann->text() == QStringLiteral("Line 1\nLine 2 edited\nLine 3"),
                   "63e: edited lines were not written back to the annotation");

        // switching to the primary legend shows the one-line header field again
        g63->newLegend();
        Graph::GraphSelection legSel;
        legSel.kind = Graph::GraphSelection::LegendBox;
        g63->setCurrentSelection(legSel);
        dock->refreshAll();
        QApplication::processEvents();
        CHECK_COND(ed->isHidden(), "63e: multi-line editor shown for the main legend");
    }

    // ---- 63f: keyboard access and readability (H4, H5, H6)
    {
        int ribbonButtons = 0, ribbonNoFocus = 0;
        for (QToolButton *b : mw->findChildren<QToolButton *>()) {
            const QString cls = b->property("class").toString();
            if (cls != QLatin1String("RibbonActionBtn") && cls != QLatin1String("RibbonPresetBtn"))
                continue;
            ++ribbonButtons;
            if (b->focusPolicy() == Qt::NoFocus)
                ++ribbonNoFocus;
        }
        std::cout << "[INFO] Suite 63f: ribbon buttons " << ribbonButtons << ", NoFocus "
                  << ribbonNoFocus << "\n";
        CHECK_COND(ribbonNoFocus == 0, "63f: ribbon buttons cannot be reached with Tab");

        // keyboard focus ring follows Tab focus, not mouse focus
        QWidget host;
        QPushButton *b1 = new QPushButton(QStringLiteral("A"), &host);
        QToolButton *iconOnly = new QToolButton(&host);
        iconOnly->setToolTip(QStringLiteral("<b>Zoom</b> in"));
        host.show();
        QApplication::processEvents();
        iconOnly->ensurePolished();
        CHECK_COND(iconOnly->accessibleName() == QStringLiteral("Zoom in"),
                   "63f: icon-only button got no accessible name from its tooltip");
        QFocusEvent tabIn(QEvent::FocusIn, Qt::TabFocusReason);
        QApplication::sendEvent(b1, &tabIn);
        CHECK_COND(ModernUiTheme::keyboardFocusRingTargets(b1), "63f: no focus ring after Tab focus");
        QFocusEvent mouseIn(QEvent::FocusIn, Qt::MouseFocusReason);
        QApplication::sendEvent(iconOnly, &mouseIn);
        CHECK_COND(!ModernUiTheme::keyboardFocusRingTargets(b1)
                           && !ModernUiTheme::keyboardFocusRingTargets(iconOnly),
                   "63f: focus ring shown for mouse focus");

        // disabled text / placeholder contrast >= 4.5:1 on white
        auto lum = [](const QColor &c) {
            auto ch = [](double v) {
                return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
        };
        auto contrast = [&](const QColor &a, const QColor &b) {
            const double la = lum(a), lb = lum(b);
            return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
        };
        const QColor dis = qApp->palette().color(QPalette::Disabled, QPalette::ButtonText);
        CHECK_COND(contrast(dis, QColor(0xF1, 0xF5, 0xF9)) >= 4.5,
                   "63f: disabled text contrast below 4.5:1");
        CHECK_COND(contrast(ModernUiTheme::placeholderTextColor(), Qt::white) >= 4.5,
                   "63f: placeholder contrast below 4.5:1");
        // style sheet: pt only, Japanese UI font first
        const QString qss = ModernUiTheme::modernStyleSheet();
        CHECK_COND(!QRegularExpression(QStringLiteral("font-size:\\s*[0-9.]+px")).match(qss).hasMatch(),
                   "63f: px font size left in the UI style sheet");
        CHECK_COND(!qss.contains(QStringLiteral("'Segoe UI', 'Yu Gothic UI'")),
                   "63f: Segoe UI listed before Yu Gothic UI");

        // the ineffective "Style" choice is gone from the preferences (H6)
        ConfigDialog *cd = new ConfigDialog(mw);
        bool hasStyleCombo = false;
        for (QComboBox *cb : cd->findChildren<QComboBox *>())
            if (cb->findText(QStringLiteral("Fusion"), Qt::MatchFixedString) >= 0
                || cb->findText(QStringLiteral("Windows"), Qt::MatchFixedString) >= 0)
                hasStyleCombo = true;
        delete cd;
        CHECK_COND(!hasStyleCombo, "63f: Qt style combo still present in ConfigDialog");
    }

    // ---- 63x (review fix): inspector built in English, then switched to Japanese (Suite 47
    // found 3 English leftovers), palette menu name, explorer icons for worksheets
    {
        ApplicationWindow *wx = new ApplicationWindow();
        wx->applyUserSettings();
        wx->switchToLanguage("en");
        wx->resize(1380, 880);
        wx->show();
        QApplication::processEvents();
        Table *tx = wx->newTable("Ja63x", 4, 2);
        for (int r = 0; r < 4; ++r) {
            tx->setCell(r, 0, r);
            tx->setCell(r, 1, r * r);
        }
        MultiLayer *mlx = wx->multilayerPlot(tx, QStringList() << "Ja63x_2", Graph::LineSymbols);
        CHECK_COND(mlx != nullptr, "63x: graph");
        wx->activateSubWindow(mlx);
        QApplication::processEvents();
        wx->switchToLanguage("ja");
        QApplication::processEvents();
        GraphInspectorDock *dock = wx->graphInspectorDock();
        CHECK_COND(dock != nullptr, "63x: dock");
        for (int pg = 0; pg < 4; ++pg) {
            dock->switchToInspectorPage(static_cast<GraphInspectorDock::InspectorPageIndex>(pg));
            QApplication::processEvents();
        }
        QPushButton *details = dock->findChild<QPushButton *>("InspectorDetailsButton");
        CHECK_COND(details != nullptr, "63x: no Details button");
        std::cout << "[INFO] 63x Details button: '" << details->text().toUtf8().constData()
                  << "' / '" << details->toolTip().toUtf8().constData() << "'\n";
        CHECK_COND(details->text() == QString::fromUtf8("詳細設定…")
                           && details->toolTip()
                                   == QString::fromUtf8("この節のすべての設定があるダイアログを開く"),
                   "63x: Details button not translated after switching to Japanese");
        int swatches = 0;
        for (QPushButton *sw : dock->findChildren<QPushButton *>("InspectorColorSwatchBtn")) {
            ++swatches;
            if (sw->toolTip().contains("Click to choose")) {
                std::cerr << "[63x] English swatch tooltip: " << sw->toolTip().toUtf8().constData()
                          << "\n";
                CHECK_COND(false, "63x: colour swatch tooltip still English after switching");
            }
        }
        std::cout << "[INFO] 63x colour swatches checked: " << swatches << "\n";
        // palette menu shows the real name of the tab10 list
        bool hasTab10 = false, hasLegacy = false;
        for (QAction *a : wx->findChildren<QAction *>()) {
            if (a->data().toString() == "Matplotlib (tab10)") {
                hasTab10 = true;
                CHECK_COND(a->text().contains("tab10") && !a->text().contains("ISIJ"),
                           "63x: palette menu text for tab10");
            }
            if (a->data().toString() == "ISIJ / Origin Default")
                hasLegacy = true;
        }
        CHECK_COND(hasTab10 && !hasLegacy, "63x: palette menu still uses 'ISIJ / Origin Default'");
        // project explorer: the worksheet row gets the worksheet icon, the graph row the graph one
        ModernUiTheme::upgradeAllWindowIcons(wx);
        QTreeWidget *lv = nullptr;
        for (QTreeWidget *tw : wx->findChildren<QTreeWidget *>())
            if (tw->objectName() == "lv")
                lv = tw;
        if (lv) {
            QString tableIcon, graphIcon;
            for (QTreeWidgetItemIterator it(lv); *it; ++it) {
                if ((*it)->text(0) == "Ja63x")
                    tableIcon = (*it)->icon(0).name();
                else if (mlx && (*it)->text(0) == mlx->name())
                    graphIcon = (*it)->icon(0).name();
            }
            std::cout << "[INFO] 63x explorer icons: table='" << tableIcon.toStdString()
                      << "' graph='" << graphIcon.toStdString() << "'\n";
            if (!tableIcon.isEmpty())
                CHECK_COND(tableIcon == "new-table", "63x: worksheet row shows a non-worksheet icon");
        } else {
            std::cout << "[INFO] 63x: project explorer list not found, icon check skipped\n";
        }
        mlx->askOnCloseEvent(false);
        tx->askOnCloseEvent(false);
        wx->savedProject();
        wx->setOleEmbeddedMode(true);
        delete wx;
        QApplication::processEvents();
        mw->switchToLanguage("ja");
        QApplication::processEvents();
    }

    ml63->setAttribute(Qt::WA_DeleteOnClose);
    ml63->askOnCloseEvent(false);
    ml63->close();
    t63->askOnCloseEvent(false);
    t63->close();
    QApplication::processEvents();
    mw->switchToLanguage(prevLang63.isEmpty() ? QStringLiteral("en") : prevLang63);
    QApplication::processEvents();
    std::cout << "[INFO] Suite 63 Japanese dictionary, UI readability & keyboard gate passed!\n";
    return 0;
}

// =============================================================================
// ============================================================================
// Suite 61: Distribution - single-source version, updater safety & proxy (I4, I5)
// ============================================================================
#include "globals.h"
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPointer>

int runSuite61DistributionVersionAndUpdaterSafety(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 61: Distribution Version & Updater Safety Gate...\n";

    // 61a: version.cpp is generated from CMake project(VERSION); all views must agree.
    {
        const QString vs = SciDAVis::versionString();
        QRegularExpression rx(QStringLiteral("^ToriGraph (\\d+)\\.(\\d+)\\.(\\d+)$"));
        const QRegularExpressionMatch m = rx.match(vs);
        CHECK_COND(m.hasMatch(), "61a: versionString is not 'ToriGraph x.y.z'");
        const int major = m.captured(1).toInt();
        const int minor = m.captured(2).toInt();
        const int patch = m.captured(3).toInt();
        CHECK_COND(SciDAVis::version() == ((major << 16) | (minor << 8) | patch),
                   "61a: numeric version (project file schema) differs from the version string");
        CHECK_COND(SciDAVis::schemaVersion()
                           == QStringLiteral("SciDAVis %1.%2.%3").arg(major).arg(minor).arg(patch),
                   "61a: schemaVersion differs from the version string");
        CHECK_COND(AutoUpdater::currentAppVersion()
                           == QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch),
                   "61a: AutoUpdater::currentAppVersion differs from the version string");
        CHECK_COND(QRegularExpression(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"))
                           .match(SciDAVis::releaseDateString())
                           .hasMatch(),
                   "61a: release date is not the ISO date from CMake");
    }

    // 61b: proxy policy - loopback direct, remote via the system proxy factory.
    {
        const QList<QNetworkProxy> loop = AutoUpdater::proxiesForUrl(QUrl("http://127.0.0.1:8080/x"));
        CHECK_COND(loop.size() == 1 && loop.first().type() == QNetworkProxy::NoProxy,
                   "61b: loopback URL must not use a proxy");
        const QList<QNetworkProxy> loop2 = AutoUpdater::proxiesForUrl(QUrl("http://localhost/x"));
        CHECK_COND(loop2.size() == 1 && loop2.first().type() == QNetworkProxy::NoProxy,
                   "61b: localhost must not use a proxy");
        const QList<QNetworkProxy> remote =
                AutoUpdater::proxiesForUrl(QUrl("https://api.github.com/repos/x/y/releases/latest"));
        CHECK_COND(!remote.isEmpty(), "61b: remote URL returned no proxy decision");
        const QList<QNetworkProxy> sys = QNetworkProxyFactory::systemProxyForQuery(
                QNetworkProxyQuery(QUrl("https://api.github.com/repos/x/y/releases/latest")));
        if (!sys.isEmpty())
            CHECK_COND(remote.first().type() == sys.first().type()
                               && remote.first().hostName() == sys.first().hostName(),
                       "61b: remote URL does not follow the system proxy configuration");
        QNetworkAccessManager nam;
        CHECK_COND(nam.proxyFactory() == nullptr, "61b: unexpected default proxy factory");
        AutoUpdater::applyNetworkProxyPolicy(nam, QUrl("https://api.github.com/"));
        CHECK_COND(nam.proxyFactory() != nullptr, "61b: updater NAM does not use a proxy factory");
    }

    // 61c: release page fallback.
    {
        CHECK_COND(AutoUpdater::releasesPageUrl().toString()
                           == QStringLiteral("https://github.com/kzykshmz/ToriGraph/releases"),
                   "61c: default release page URL");
        const QUrl tagged("https://github.com/kzykshmz/ToriGraph/releases/tag/v9.9.0");
        CHECK_COND(AutoUpdater::releasesPageUrl(tagged) == tagged,
                   "61c: release-specific page not preferred");
        CHECK_COND(AutoUpdater::releasesPageUrl(QUrl::fromLocalFile("C:/x.zip")).scheme()
                           == QStringLiteral("https"),
                   "61c: non-web URL must fall back to the releases page");
    }

    // 61d/e: without a published SHA-256 nothing is installed by default.
    {
        const QString key = QStringLiteral("AutoUpdate/AllowUnverifiedInstall");
        QSettings &st = ApplicationWindow::getSettings();
        const QVariant saved = st.value(key);
        st.remove(key);
        CHECK_COND(!AutoUpdater::allowUnverifiedInstall(),
                   "61d: unverified installs must be off by default");

        ReleaseInfo rel;
        rel.tagName = "v9.9.0";
        rel.version = "9.9.0";
        rel.assetName = "ToriGraph-9.9.0-win64-portable.zip";
        rel.assetUrl = QUrl("https://github.com/kzykshmz/ToriGraph/releases/download/v9.9.0/ToriGraph-9.9.0-win64-portable.zip");
        rel.htmlUrl = QUrl("https://github.com/kzykshmz/ToriGraph/releases/tag/v9.9.0");
        auto primaryButton = [](UpdateAvailableDialog &dlg) -> QPushButton * {
            for (QPushButton *b : dlg.findChildren<QPushButton *>())
                if (b && b->isDefault())
                    return b;
            return nullptr;
        };
        {
            UpdateAvailableDialog dlg(rel, "3.0.0", mw);
            QPushButton *b = primaryButton(dlg);
            CHECK_COND(b, "61e: no default button in update dialog");
            CHECK_COND(b->text() == UpdateAvailableDialog::tr("Open Release Page..."),
                       "61e: without SHA-256 the primary action must open the release page");
            CHECK_COND(!b->toolTip().isEmpty(), "61e: missing explanation for unverified package");
            for (QLabel *l : dlg.findChildren<QLabel *>())
                CHECK_COND(!l || !l->text().contains(QStringLiteral("Verified"))
                                   || l->text().contains(QStringLiteral("Failed")),
                           "61e: dialog claims 'verified' without a checksum");
        }
        st.setValue(key, true);
        {
            UpdateAvailableDialog dlg(rel, "3.0.0", mw);
            QPushButton *b = primaryButton(dlg);
            CHECK_COND(b && b->text() == UpdateAvailableDialog::tr("Download && Install Update"),
                       "61e: AllowUnverifiedInstall=true should re-enable installing");
        }
        st.remove(key);
        rel.expectedSha256 = QString(64, QLatin1Char('a'));
        {
            UpdateAvailableDialog dlg(rel, "3.0.0", mw);
            QPushButton *b = primaryButton(dlg);
            CHECK_COND(b && b->text() == UpdateAvailableDialog::tr("Download && Install Update"),
                       "61e: with SHA-256 the dialog should offer installing");
        }
        if (saved.isValid())
            st.setValue(key, saved);
    }

    // 61f/g: the staged script waits, and aborts while ToriGraph is still running.
    {
        QTemporaryDir tmp;
        CHECK_COND(tmp.isValid(), "61f: temp dir");
        const QString installDir = tmp.filePath("install dir [x64]");
        CHECK_COND(QDir().mkpath(installDir), "61f: mkpath");
        const QString exePath = QDir(installDir).filePath("ToriGraph.exe");
        {
            QFile f(exePath);
            CHECK_COND(f.open(QIODevice::WriteOnly), "61f: seed exe");
            f.write("OLD_BINARY");
        }
        const QString pkg = tmp.filePath("ToriGraph-9.9.0-win64-portable.zip");
        {
            QFile f(pkg);
            CHECK_COND(f.open(QIODevice::WriteOnly), "61f: seed package");
            f.write("not really a zip");
        }
        QString err;
        const qint64 alivePid = QCoreApplication::applicationPid();
        const QString script = AutoUpdater::writeWindowsStagingUpdaterScript(
                pkg, installDir, "ToriGraph.exe", alivePid, /*relaunch=*/false, &err,
                /*waitTimeoutSec=*/2);
        CHECK_COND(!script.isEmpty(), "61f: script not written");
        QFile sf(script);
        CHECK_COND(sf.open(QIODevice::ReadOnly), "61f: read script");
        const QString body = QString::fromUtf8(sf.readAll());
        sf.close();
        CHECK_COND(body.contains("$WaitTimeoutSec = 2") && body.contains("Get-BlockingProcess")
                           && body.contains("exit 3"),
                   "61f: script lacks the wait-and-abort guard");
        CHECK_COND(body.indexOf("exit 3") < body.indexOf("Copy-Item -LiteralPath $TargetExe"),
                   "61f: abort check must come before the installation is touched");

        QProcess ps;
        ps.start(QStringLiteral("powershell.exe"),
                 QStringList() << "-NoProfile" << "-NonInteractive" << "-ExecutionPolicy"
                               << "Bypass" << "-File" << QDir::toNativeSeparators(script));
        const bool finished = ps.waitForFinished(60000);
        const QString psErr = QString::fromLocal8Bit(ps.readAllStandardError());
        if (!finished || ps.exitCode() != 3)
            std::cerr << "[DIAG] 61g exit=" << ps.exitCode() << " stderr=" << psErr.toStdString() << "\n";
        CHECK_COND(finished, "61g: staged script did not finish");
        CHECK_COND(ps.exitCode() == 3, "61g: script must abort (exit 3) while ToriGraph is alive");
        QFile f(exePath);
        CHECK_COND(f.open(QIODevice::ReadOnly) && f.readAll() == "OLD_BINARY",
                   "61g: installation was modified although ToriGraph was running");
        f.close();
        CHECK_COND(!QFile::exists(exePath + ".rollback.bak"),
                   "61g: script touched the install dir before aborting");
        QFile::remove(script);
        QFile::remove(QFileInfo(script).dir().filePath("apply_torigraph_update.cmd"));
        QFile::remove(QFileInfo(script).dir().filePath("apply_torigraph_update.log"));
    }

    // 61h: new updater strings have Japanese translations.
    {
        const char *keys[] = { "Open Release Page", "Update Not Installed",
                               "Install Unverified Update?",
                               "Download complete (not verified: no SHA-256 checksum published)." };
        for (const char *k : keys)
            CHECK_COND(!ToriGraphFullJapaneseTranslator::lookupJapanese(QString::fromUtf8(k)).isEmpty(),
                       std::string("61h: missing Japanese translation: ") + k);
    }

    // 61i: installer / msiexec arguments are quoted (PowerShell 5.1 Start-Process does not
    //      quote -ArgumentList elements, so "C:\Program Files\..." used to split), and the
    //      quoting expression really evaluates to /DIR="<dir without trailing \>".
    {
        QTemporaryDir tmp;
        CHECK_COND(tmp.isValid(), "61i: temp dir");
        const QString pkg = tmp.filePath("ToriGraph-9.9.0-win64-setup.exe");
        QString err;
        const QString script = AutoUpdater::writeWindowsStagingUpdaterScript(
                pkg, tmp.filePath("Program Files/ToriGraph"), "ToriGraph.exe", 0, true, &err, 2);
        CHECK_COND(!script.isEmpty(), "61i: script not written");
        QFile sf(script);
        CHECK_COND(sf.open(QIODevice::ReadOnly), "61i: read script");
        const QString body = QString::fromUtf8(sf.readAll());
        sf.close();
        CHECK_COND(!body.contains(QStringLiteral("(\"/DIR=$TargetInstallDir\")")),
                   "61i: unquoted /DIR=$TargetInstallDir is still passed to Start-Process");
        CHECK_COND(body.contains(QStringLiteral("('\"' + $DownloadedFile + '\"')")),
                   "61i: msiexec /i path is not quoted");
        CHECK_COND(body.contains(QStringLiteral("'/NORESTART', $dirArg)")),
                   "61i: installer does not receive the quoted $dirArg");
        CHECK_COND(body.contains(QStringLiteral("explorer.exe")),
                   "61i: elevated relaunch must go through Explorer (normal rights)");
        QString dirLine;
        for (const QString &line : body.split(QStringLiteral("\r\n")))
            if (line.trimmed().startsWith(QStringLiteral("$dirArg =")))
                dirLine = line.trimmed();
        CHECK_COND(!dirLine.isEmpty(), "61i: $dirArg line not found");
        QProcess ps;
        ps.start(AutoUpdater::resolvePowerShellExecutable(),
                 QStringList() << "-NoProfile" << "-NonInteractive" << "-Command"
                               << (QStringLiteral("$TargetInstallDir = 'C:\\Program Files\\Tori Graph\\'; ")
                                   + dirLine + QStringLiteral("; Write-Output $dirArg")));
        CHECK_COND(ps.waitForFinished(60000), "61i: powershell did not finish");
        const QString out = QString::fromLocal8Bit(ps.readAllStandardOutput()).trimmed();
        if (out != QStringLiteral("/DIR=\"C:\\Program Files\\Tori Graph\""))
            std::cerr << "[DIAG] 61i dirArg=" << out.toStdString() << "\n";
        CHECK_COND(out == QStringLiteral("/DIR=\"C:\\Program Files\\Tori Graph\""),
                   "61i: $dirArg does not evaluate to a quoted /DIR without trailing backslash");
        QFile::remove(script);
        QFile::remove(QFileInfo(script).dir().filePath("apply_torigraph_update.cmd"));
    }

    // 61j: launch prerequisites are checked BEFORE the main window is closed.
    {
        const QString psExe = AutoUpdater::resolvePowerShellExecutable();
        CHECK_COND(!psExe.isEmpty() && QFileInfo(psExe).isFile()
                           && psExe.endsWith(QStringLiteral("powershell.exe"), Qt::CaseInsensitive),
                   "61j: powershell.exe not resolved to an existing file");
        QTemporaryDir tmp;
        CHECK_COND(AutoUpdater::isDirectoryWritable(tmp.path()), "61j: temp dir reported read-only");
        CHECK_COND(!AutoUpdater::isDirectoryWritable(tmp.filePath("does/not/exist")),
                   "61j: missing dir reported writable");
        CHECK_COND(QDir(tmp.path()).entryList(QDir::Files | QDir::Hidden).isEmpty(),
                   "61j: write probe left a file behind");

        // A missing script must keep ToriGraph open (no close(), no quit) and explain why.
        bool sawBox = false;
        QTimer closer;
        closer.setInterval(50);
        QObject::connect(&closer, &QTimer::timeout, [&]() {
            if (QMessageBox *b = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                sawBox = true;
                b->reject();
            }
        });
        closer.start();
        const bool wasVisible = mw->isVisible();
        QPointer<ApplicationWindow> guard(mw);
        const bool launched = AutoUpdater::closeAppAndLaunchUpdater(
                mw, tmp.filePath("missing_apply_torigraph_update.ps1"));
        closer.stop();
        QApplication::processEvents();
        CHECK_COND(!launched, "61j: launching a missing script reported success");
        CHECK_COND(sawBox, "61j: no explanation shown for a missing update script");
        CHECK_COND(guard && mw->isVisible() == wasVisible,
                   "61j: main window was closed although nothing could be launched");
    }

    // 61k: proxy decision is cached per host (WPAD lookups can block the GUI thread), and a
    //      proxy asking for credentials gets an actionable hint.
    {
        const QUrl u("https://objects.githubusercontent.com/github-production-release-asset/x");
        const QList<QNetworkProxy> first = AutoUpdater::proxiesForUrl(u);
        QElapsedTimer et;
        et.start();
        for (int i = 0; i < 20; ++i) {
            const QList<QNetworkProxy> again = AutoUpdater::proxiesForUrl(u);
            CHECK_COND(again.size() == first.size() && again.first().type() == first.first().type()
                               && again.first().hostName() == first.first().hostName(),
                       "61k: cached proxy decision differs");
        }
        CHECK_COND(et.elapsed() < 200, "61k: repeated proxy lookups are not cached");

        const QString hint = AutoUpdater::describeNetworkError(
                QNetworkReply::ProxyAuthenticationRequiredError, QStringLiteral("auth"));
        // UI may be Japanese or English: the hint follows the error on its own line.
        CHECK_COND(hint.startsWith(QStringLiteral("auth\n")) && hint.size() > 40,
                   "61k: proxy-auth error lacks the release-page hint");
        CHECK_COND(AutoUpdater::describeNetworkError(QNetworkReply::HostNotFoundError,
                                                     QStringLiteral("dns"))
                           == QStringLiteral("dns"),
                   "61k: unrelated errors must stay unchanged");
    }

    // 61l: Japanese for the new updater messages.
    {
        const char *keys[] = { "Administrator Rights Needed",
                               "The proxy server asks for a user name and password, which the "
                               "updater cannot enter. Open the release page in your web browser "
                               "and download the package from there, or ask your network "
                               "administrator." };
        for (const char *k : keys)
            CHECK_COND(!ToriGraphFullJapaneseTranslator::lookupJapanese(QString::fromUtf8(k)).isEmpty(),
                       std::string("61l: missing Japanese translation: ") + k);
    }

    std::cout << "[INFO] Suite 61 Distribution Version & Updater Safety Gate passed!\n";
    return 0;
}

// =============================================================================
// =========================================================================
// Suite 60: PowerPoint / Word OLE link (B5, E9, A8, A9, I2)
//   60a "Copy for PowerPoint" names, tooltip and Japanese strings
//   60b embedding only the graph and the tables it plots (2D and 3D), or the whole project
//   60c copy result: size, embedded windows, 300-600 dpi PNG on the OLE data object
//   60d PNG resolution rule checked against hand-computed values
//   60e OLE failure: EMF + PNG + DIB still placed on the clipboard, reason reported
//   60f no graph active: the user is told to select a graph
//   60g re-editing an unreadable embedding is refused and leaves the object unchanged
//   60h start-up registration never redirects a valid registration
//   60i "copy page as 300 dpi image" replaces an image already on the clipboard
// =========================================================================
static QByteArray suite60EmfOfSize(LONG wHm, LONG hHm)
{
    RECT frame{ 0, 0, wHm, hHm };
    HDC dc = CreateEnhMetaFileW(nullptr, nullptr, &frame, nullptr);
    if (!dc)
        return {};
    Rectangle(dc, 10, 10, 300, 200);
    HENHMETAFILE h = CloseEnhMetaFile(dc);
    const UINT n = GetEnhMetaFileBits(h, 0, nullptr);
    QByteArray out(static_cast<int>(n), '\0');
    GetEnhMetaFileBits(h, n, reinterpret_cast<BYTE *>(out.data()));
    DeleteEnhMetaFile(h);
    return out;
}

static QByteArray suite60ReadGlobal(HGLOBAL hg)
{
    QByteArray out;
    if (!hg)
        return out;
    const SIZE_T sz = GlobalSize(hg);
    if (const void *p = GlobalLock(hg)) {
        out = QByteArray(static_cast<const char *>(p), static_cast<int>(sz));
        GlobalUnlock(hg);
    }
    return out;
}

static QString suite60RegistryExe()
{
    wchar_t buf[2048] = {};
    DWORD cb = sizeof(buf);
    if (RegGetValueW(HKEY_CLASSES_ROOT,
                     L"CLSID\\{8E6B9541-3C74-4D28-A915-7D84E5A12001}\\LocalServer32", nullptr,
                     RRF_RT_REG_SZ, nullptr, buf, &cb)
        != ERROR_SUCCESS)
        return QString();
    return QString::fromWCharArray(buf);
}

int runSuite60PowerPointOleLink(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 60: PowerPoint / Word OLE link...\n";
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    const SciDAVisOle::EmbedScope prevScope = SciDAVisOle::embedScopeSetting();

    // ---- 60a. names and translations ----------------------------------------------------
    {
        const QString prevLang = mw->appLanguage;
        mw->switchToLanguage("ja");
        QApplication::processEvents();
        CHECK_COND(ToriGraphOleUi::copyForPowerPointLabel() == QString::fromUtf8("PowerPoint 用にコピー"),
                   "60a: Japanese button label is not \"PowerPoint 用にコピー\"");
        CHECK_COND(ToriGraphOleUi::copyForPowerPointToolTip().contains(
                           QString::fromUtf8("スライド上でダブルクリックすると再編集できます")),
                   "60a: tooltip does not explain double-click re-editing");
        QAction *aCopy = mw->findChild<QAction *>(QStringLiteral("actionCopyGraphOle"));
        QAction *aScope = mw->findChild<QAction *>(QStringLiteral("actionOleEmbedWholeProject"));
        QAction *aRepair = mw->findChild<QAction *>(QStringLiteral("actionRepairOleRegistration"));
        CHECK_COND(aCopy && aCopy->text().contains(QStringLiteral("PowerPoint")),
                   "60a: menu action is not named for PowerPoint");
        CHECK_COND(aCopy->toolTip().contains(QString::fromUtf8("ダブルクリック")),
                   "60a: menu action tooltip missing");
        CHECK_COND(aScope && aScope->isCheckable(),
                   "60a: embed-scope option missing");
        CHECK_COND(aRepair
                           && aRepair->text().contains(
                                   QString::fromUtf8("修復")),
                   "60a: repair action missing or untranslated");
        mw->switchToLanguage(prevLang.isEmpty() ? QStringLiteral("en") : prevLang);
        QApplication::processEvents();
    }

    ApplicationWindow *w60 = new ApplicationWindow();
    w60->applyUserSettings();
    w60->setHeadlessMode(true);
    w60->confirmCloseTable = false;
    w60->confirmClosePlot2D = false;
    w60->confirmClosePlot3D = false;
    w60->confirmCloseFolder = false;
    struct W60Guard {
        ApplicationWindow *w;
        ~W60Guard()
        {
            if (w) {
                w->savedProject();
                w->setOleEmbeddedMode(true);
                delete w;
            }
        }
    } w60Guard{ w60 };

    Table *tA = w60->newTable(5, 2, "Tensile60", "plotted");
    Table *tB = w60->newTable(3, 2, "Unrelated60", "not plotted");
    CHECK_COND(tA && tB, "60b: tables not created");
    for (int r = 0; r < 5; ++r) {
        tA->setCell(r, 0, double(r));
        tA->setCell(r, 1, 100.0 + 10.0 * r);
    }
    MultiLayer *ml = w60->multilayerPlot(tA, QStringList() << (tA->name() + "_2"), Graph::LineSymbols);
    CHECK_COND(ml && ml->activeGraph(), "60b: graph not created");
    w60->logInfo += QStringLiteral("SUITE60_LOG_MARKER\n");

    Table *t3 = w60->newTable(4, 3, "Ct_Strain60", "3D data");
    t3->setColName(0, "Xp");
    t3->setColName(1, "Yp");
    t3->setColName(2, "Zv");
    t3->setColPlotDesignation(0, SciDAVis::X);
    t3->setColPlotDesignation(1, SciDAVis::Y);
    t3->setColPlotDesignation(2, SciDAVis::Z);
    for (int r = 0; r < 4; ++r) {
        t3->setCell(r, 0, double(r));
        t3->setCell(r, 1, double(r) * 2.0);
        t3->setCell(r, 2, double(r * r));
    }
    Graph3D *g3 = w60->dataPlotXYZ(t3, t3->colName(2), Graph3D::Scatter);
    CHECK_COND(g3 != nullptr, "60b: 3D plot not created");

    // ---- 60b. embedding scope -----------------------------------------------------------
    {
        const QStringList deps = SciDAVisOle::dataDependenciesForWindow(w60, ml);
        CHECK_COND(deps == QStringList() << QStringLiteral("Tensile60"),
                   "60b: 2D dependencies wrong: " + deps.join(',').toStdString());
        const QStringList deps3 = SciDAVisOle::dataDependenciesForWindow(w60, g3);
        CHECK_COND(deps3 == QStringList() << QStringLiteral("Ct_Strain60"),
                   "60b: 3D dependencies wrong: " + deps3.join(',').toStdString());

        QStringList names;
        const QByteArray graphOnly = SciDAVisOle::buildEmbeddedProjectBytes(
                w60, ml, SciDAVisOle::EmbedScope::GraphOnly, &names);
        CHECK_COND(names == QStringList() << QStringLiteral("Tensile60") << ml->name(),
                   "60b: embedded windows wrong: " + names.join(',').toStdString());
        const QString txt = QString::fromUtf8(graphOnly);
        CHECK_COND(txt.contains(QStringLiteral("Tensile60")) && !txt.contains(QStringLiteral("Unrelated60"))
                           && !txt.contains(QStringLiteral("Ct_Strain60"))
                           && !txt.contains(QStringLiteral("SUITE60_LOG_MARKER")),
                   "60b: graph-only project contains other windows or the results log");
        CHECK_COND(txt.contains(QStringLiteral("<windows>\t2")), "60b: window count header wrong");

        const QByteArray whole = SciDAVisOle::buildEmbeddedProjectBytes(
                w60, ml, SciDAVisOle::EmbedScope::WholeProject, &names);
        const QString wtxt = QString::fromUtf8(whole);
        CHECK_COND(wtxt.contains(QStringLiteral("Unrelated60")) && wtxt.contains(QStringLiteral("SUITE60_LOG_MARKER")),
                   "60b: whole-project scope is missing windows or the log");
        CHECK_COND(graphOnly.size() < whole.size(), "60b: graph-only project is not smaller");

        // The graph-only project loads on its own with the curve intact.
        const QString prj = QDir::temp().filePath(QStringLiteral("suite60_graph_only.sciprj"));
        QFile f(prj);
        CHECK_COND(f.open(QIODevice::WriteOnly) && f.write(graphOnly) == graphOnly.size(),
                   "60b: could not write temp project");
        f.close();
        ApplicationWindow *rd = new ApplicationWindow();
        rd->setHeadlessMode(true);
        rd->applyUserSettings();
        const bool loaded = rd->loadProject(prj);
        QFile::remove(prj);
        MultiLayer *rml = nullptr;
        int nTables = 0;
        for (MyWidget *w : rd->windowsList()) {
            if (w->inherits("MultiLayer"))
                rml = static_cast<MultiLayer *>(w);
            if (w->inherits("Table"))
                ++nTables;
        }
        const int curves = (rml && rml->activeGraph()) ? rml->activeGraph()->curves() : -1;
        rd->savedProject();
        rd->setOleEmbeddedMode(true);
        delete rd;
        CHECK_COND(loaded && rml && nTables == 1 && curves == 1,
                   "60b: graph-only project did not reload with 1 table and 1 curve");

        QStringList names3;
        const QString t3txt = QString::fromUtf8(SciDAVisOle::buildEmbeddedProjectBytes(
                w60, g3, SciDAVisOle::EmbedScope::GraphOnly, &names3));
        CHECK_COND(names3 == QStringList() << QStringLiteral("Ct_Strain60") << g3->name()
                           && !t3txt.contains(QStringLiteral("Tensile60")),
                   "60b: 3D graph-only embedding wrong");
    }

    // ---- 60c. copy result and PNG on the OLE data object -------------------------------
    {
        SciDAVisOle::setEmbedScopeSetting(SciDAVisOle::EmbedScope::GraphOnly);
        const SciDAVisOle::OleCopyResult res = SciDAVisOle::copyGraphForPowerPoint(w60, ml, nullptr);
        markClipboardAfterCopy();
        CHECK_COND(res.ok, "60c: copy failed: " + res.errorReason.toStdString());
        CHECK_COND(res.widthMm > 10.0 && res.heightMm > 10.0, "60c: size not reported");
        CHECK_COND(res.embeddedWindows == QStringList() << QStringLiteral("Tensile60") << ml->name(),
                   "60c: copy did not embed graph + table only");
        SciDAVisOle::reportCopyResult(w60, res, true);
        const QString msg = qApp->property("torigraph_last_ole_copy_message").toString();
        CHECK_COND(msg.contains(ml->name()) && msg.contains(QStringLiteral("mm"))
                           && msg.contains(QString::number(res.widthMm, 'f', 1)),
                   "60c: status message lacks name or size: " + msg.toStdString());

        // If another program took over the clipboard after the copy, copy again.
        ComPtr<IDataObject> clip;
        QByteArray png;
        FORMATETC fPng{ SciDAVisOle::pngClipboardFormat(), nullptr, DVASPECT_CONTENT, -1,
                        TYMED_HGLOBAL };
        bool firstAttempt = true;
        const char *clipErr = retryClipboardStep(
                [&]() -> const char * {
                    if (!firstAttempt) {
                        if (!SciDAVisOle::copyGraphForPowerPoint(w60, ml, nullptr).ok)
                            return "60c: repeated copy failed";
                        markClipboardAfterCopy();
                    }
                    firstAttempt = false;
                    // Is the object of this copy still the clipboard content? (Something else may
                    // have been copied meanwhile; then its PNG would be checked against our size.)
                    if (!SciDAVisOle::lastCopyStillOnClipboard()) {
                        std::cout << "[INFO] 60c: the copied object is no longer on the clipboard"
                                  << " (sequence at copy " << SciDAVisOle::lastCopyClipboardSequence()
                                  << ", at mark " << g_clipboardSeqAfterCopy << ", now "
                                  << GetClipboardSequenceNumber() << "; owner at mark "
                                  << g_clipboardOwnerAtMark << ", now " << currentClipboardOwnerPid()
                                  << ", this process " << GetCurrentProcessId() << ")\n";
                        return "60c: the copied object was replaced on the clipboard";
                    }
                    clip.Reset();
                    if (FAILED(oleGetClipboardRetry(clip)) || !clip)
                        return "60c: OleGetClipboard failed";
                    if (clip->QueryGetData(&fPng) != S_OK)
                        return "60c: PNG format not offered";
                    STGMEDIUM med{};
                    const HRESULT hrPng = clip->GetData(&fPng, &med);
                    if (FAILED(hrPng) || med.tymed != TYMED_HGLOBAL) {
                        if (SUCCEEDED(hrPng))
                            ReleaseStgMedium(&med);
                        return "60c: GetData(PNG) failed";
                    }
                    png = suite60ReadGlobal(med.hGlobal);
                    ReleaseStgMedium(&med);
                    FORMATETC fEmf{ CF_ENHMETAFILE, nullptr, DVASPECT_CONTENT, -1, TYMED_ENHMF };
                    if (clip->QueryGetData(&fEmf) != S_OK)
                        return "60c: EMF format missing";
                    // Physical width the EMF on the clipboard claims (rclFrame, 0.01 mm)
                    long emfFrameW = -1;
                    {
                        STGMEDIUM em{};
                        if (SUCCEEDED(clip->GetData(&fEmf, &em))) {
                            ENHMETAHEADER eh{};
                            if (em.tymed == TYMED_ENHMF && em.hEnhMetaFile
                                && GetEnhMetaFileHeader(em.hEnhMetaFile, sizeof(eh), &eh) > 0)
                                emfFrameW = eh.rclFrame.right - eh.rclFrame.left;
                            ReleaseStgMedium(&em);
                        }
                    }
                    // The PNG is checked here too: another ToriGraph (a parallel test run) offers
                    // the same formats, so only its size can tell that the data is not ours.
                    QImage img;
                    if (!img.loadFromData(png, "PNG"))
                        return "60c: PNG does not decode";
                    const double dpi = img.dotsPerMeterX() * 0.0254;
                    const double expectW = res.widthMm / 25.4 * res.pngDpi;
                    std::cout << "[INFO] 60c: reported " << res.widthMm << " x " << res.heightMm
                              << " mm, EMF frame " << emfFrameW / 100.0 << " mm, PNG "
                              << img.width() << " x " << img.height() << " px at " << dpi
                              << " dpi (" << img.width() / std::max(1.0, dpi) * 25.4
                              << " mm); clipboard owner pid " << currentClipboardOwnerPid()
                              << " (this process " << GetCurrentProcessId() << ", owner at copy "
                              << g_clipboardOwnerAtMark << ")\n";
                    const bool sizeMismatch = dpi <= 299.0 || dpi >= 601.0
                            || std::abs(dpi - res.pngDpi) >= 1.0
                            || std::fabs(img.width() - expectW) > 2.0
                            || emfFrameW < 0 || std::fabs(emfFrameW - res.widthMm * 100.0) > 2.0;
                    if (sizeMismatch && !SciDAVisOle::lastCopyStillOnClipboard()) {
                        std::cout << "[INFO] 60c: the clipboard changed while it was read (now "
                                  << GetClipboardSequenceNumber() << ", owner "
                                  << currentClipboardOwnerPid() << ")\n";
                        return "60c: the copied object was replaced on the clipboard";
                    }
                    if (dpi <= 299.0 || dpi >= 601.0 || std::abs(dpi - res.pngDpi) >= 1.0
                        || std::fabs(img.width() - expectW) > 2.0) {
                        std::cerr << "[INFO] 60c: PNG " << img.width() << " px at " << dpi
                                  << " dpi; expected " << expectW << " px at " << res.pngDpi
                                  << " dpi (" << res.widthMm << " mm)\n";
                        if (dpi <= 299.0 || dpi >= 601.0)
                            return "60c: PNG resolution outside 300-600 dpi";
                        if (std::abs(dpi - res.pngDpi) >= 1.0)
                            return "60c: reported PNG dpi differs from the file";
                        return "60c: PNG width does not match physical size x dpi";
                    }
                    // The vector picture must claim the same physical size as the PNG and the
                    // reported size (PowerPoint sizes the pasted object from rclFrame).
                    if (emfFrameW < 0 || std::fabs(emfFrameW - res.widthMm * 100.0) > 2.0)
                        return "60c: EMF frame (rclFrame) differs from the reported size";
                    return nullptr;
                },
                "60c");
        CHECK_COND(clipErr == nullptr, (clipErr ? clipErr : ""));
        clip.Reset();
    }

    // ---- 60d. PNG resolution rule ---------------------------------------------------------
    {
        // 85 mm square: 600 dpi -> 2008 px (85 / 25.4 * 600 = 2007.9)
        int dpi = 0;
        QImage a;
        CHECK_COND(a.loadFromData(SciDAVisOle::renderEmfToPng(suite60EmfOfSize(8500, 8500),
                                                              SIZEL{ 8500, 8500 }, &dpi),
                                  "PNG"),
                   "60d: 85 mm PNG not rendered");
        CHECK_COND(dpi == 600 && a.width() == 2008 && a.height() == 2008,
                   "60d: 85 mm square should be 600 dpi / 2008 px");
        // 300 mm square: 600 dpi would be 50 Mpx; the budget of 24 Mpx allows
        // 24e6 / (300/25.4)^2 = 172,000 -> dpi <= 414.8, i.e. 400 dpi in 25-dpi steps -> 4724 px.
        QImage b;
        CHECK_COND(b.loadFromData(SciDAVisOle::renderEmfToPng(suite60EmfOfSize(30000, 30000),
                                                              SIZEL{ 30000, 30000 }, &dpi),
                                  "PNG"),
                   "60d: 300 mm PNG not rendered");
        CHECK_COND(dpi == 400 && b.width() == 4724, "60d: 300 mm square should be 400 dpi / 4724 px");
    }

    // ---- 60e. OLE failure -> static EMF + PNG + DIB fallback ---------------------------
    {
        SciDAVisOle::OleCopyResult res;
        // The formats are checked right after the copy; if another program replaced the
        // clipboard in between, the (forced-failure) copy is repeated.
        const char *clipErr = retryClipboardStep(
                [&]() -> const char * {
                    qApp->setProperty("torigraph_test_force_ole_copy_failure", true);
                    res = SciDAVisOle::copyGraphForPowerPoint(w60, ml, nullptr);
                    markClipboardAfterCopy();
                    qApp->setProperty("torigraph_test_force_ole_copy_failure", QVariant());
                    if (!(!res.ok && res.staticFallbackOnClipboard))
                        return nullptr; // reported by the checks below, not a clipboard race
                    QCoreApplication::processEvents();
                    if (!(IsClipboardFormatAvailable(CF_ENHMETAFILE)
                          && IsClipboardFormatAvailable(SciDAVisOle::pngClipboardFormat())
                          && IsClipboardFormatAvailable(CF_DIB)))
                        return "60e: EMF / PNG / DIB missing from the clipboard";
                    return nullptr;
                },
                "60e");
        CHECK_COND(!res.ok && res.staticFallbackOnClipboard,
                   "60e: static fallback not placed (ok=" + std::to_string(res.ok) + ", reason="
                           + res.errorReason.toStdString() + ")");
        CHECK_COND(!res.errorReason.isEmpty(), "60e: no failure reason");
        CHECK_COND(res.pngDpi >= 300 && res.pngDpi <= 600, "60e: fallback PNG dpi out of range");
        CHECK_COND(clipErr == nullptr, (clipErr ? clipErr : ""));
        SciDAVisOle::reportCopyResult(w60, res, true);
        const QString msg = qApp->property("torigraph_last_ole_copy_message").toString();
        CHECK_COND(msg.contains(QStringLiteral("forced failure")) && msg.contains(QStringLiteral("dpi")),
                   "60e: failure message lacks reason or fallback note: " + msg.toStdString());
    }

    // ---- 60f. table active -> "select a graph" ---------------------------------------
    {
        w60->activateSubWindow(tB);
        QApplication::processEvents();
        qApp->setProperty("torigraph_last_ole_copy_message", QVariant());
        if (w60->d_workspace.activeSubWindow() == tB) {
            w60->copyGraphAsOleObject();
            CHECK_COND(qApp->property("torigraph_last_ole_copy_message").toString()
                               == ToriGraphOleUi::selectGraphFirstMessage(),
                       "60f: no 'select a graph' message with a table active");
        } else {
            std::cout << "[INFO] Suite 60f: headless window has no active subwindow; checked directly\n";
            const SciDAVisOle::OleCopyResult r = SciDAVisOle::copyGraphForPowerPoint(w60, nullptr);
            CHECK_COND(!r.ok && r.errorReason == ToriGraphOleUi::selectGraphFirstMessage(),
                       "60f: null graph not reported");
        }
    }

    // ---- 60g. unreadable embedding is not replaced by a sample graph --------------------
    {
        OleRenderedGraph rg = SciDAVisEmf::renderMultiLayer(ml, nullptr, false);
        auto checkRefused = [&](const QByteArray &projBytes, const QString &target,
                                const char *tag) -> int {
            SciDAVisOle::OleGraphPayload pay;
            pay.projectBytes = projBytes;
            pay.targetWindow = target;
            pay.emfBytes = rg.emfBytes;
            pay.extentHm = rg.extentHm;
            pay.pixelSize = rg.pixelSize;
            ComPtr<IOleObject> obj;
            CHECK_COND(SUCCEEDED(SciDAVisOle::createGraphObject(pay, IID_PPV_ARGS(&obj))) && obj,
                       std::string("60g: createGraphObject failed ") + tag);
            int appsBefore = 0;
            for (QWidget *t : QApplication::topLevelWidgets())
                appsBefore += qobject_cast<ApplicationWindow *>(t) && t->isVisible() ? 1 : 0;
            qApp->setProperty("torigraph_last_ole_open_error", QVariant());
            const HRESULT hrVerb = obj->DoVerb(OLEIVERB_PRIMARY, nullptr, nullptr, 0, nullptr, nullptr);
            QApplication::processEvents();
            int appsAfter = 0;
            for (QWidget *t : QApplication::topLevelWidgets())
                appsAfter += qobject_cast<ApplicationWindow *>(t) && t->isVisible() ? 1 : 0;
            CHECK_COND(FAILED(hrVerb), std::string("60g: DoVerb succeeded on a bad embedding ") + tag);
            CHECK_COND(appsAfter == appsBefore, std::string("60g: an editor window was opened ") + tag);
            CHECK_COND(!qApp->property("torigraph_last_ole_open_error").toString().isEmpty(),
                       std::string("60g: no error reported ") + tag);

            ComPtr<IPersistStorage> ps;
            CHECK_COND(SUCCEEDED(obj.As(&ps)), "60g: IPersistStorage missing");
            ComPtr<ILockBytes> lb;
            ComPtr<IStorage> stg;
            CHECK_COND(SUCCEEDED(CreateILockBytesOnHGlobal(nullptr, TRUE, &lb))
                               && SUCCEEDED(StgCreateDocfileOnILockBytes(
                                       lb.Get(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                       0, &stg)),
                       "60g: storage not created");
            CHECK_COND(SUCCEEDED(ps->Save(stg.Get(), FALSE)) && SUCCEEDED(ps->SaveCompleted(nullptr)),
                       "60g: Save failed");
            SciDAVisOle::OleGraphPayload back;
            CHECK_COND(SUCCEEDED(SciDAVisOle::loadPayloadFromStorage(stg.Get(), &back)),
                       "60g: reload failed");
            CHECK_COND(back.projectBytes == projBytes && back.emfBytes == rg.emfBytes
                               && back.targetWindow == target,
                       std::string("60g: embedding changed after the refused edit ") + tag);
            return 0;
        };
        if (checkRefused(QByteArrayLiteral("this is not a ToriGraph project\n\x01\x02"),
                         ml->name(), "(corrupt)") != 0)
            return 1;
        const QByteArray realProj = SciDAVisOle::buildEmbeddedProjectBytes(
                w60, ml, SciDAVisOle::EmbedScope::GraphOnly);
        if (checkRefused(realProj, QStringLiteral("NoSuchGraph60"), "(missing target)") != 0)
            return 1;
    }

    // ---- 60h. start-up registration ---------------------------------------------------
    {
        const QString before = suite60RegistryExe();
        const SciDAVisOle::RegistrationInfo info = SciDAVisOle::queryRegistration();
        CHECK_COND(info.state == SciDAVisOle::RegistrationInfo::Valid,
                   "60h: registration not valid after start-up policy ran");
        QString err;
        CHECK_COND(SciDAVisOle::ensureRegistrationAtStartup(&err), "60h: start-up policy failed");
        CHECK_COND(suite60RegistryExe() == before, "60h: a valid registration was rewritten");
        const bool hadNoRegister = qEnvironmentVariableIsSet("TORIGRAPH_NO_OLE_REGISTER");
        const QByteArray prevNoRegister = qgetenv("TORIGRAPH_NO_OLE_REGISTER");
        qputenv("TORIGRAPH_NO_OLE_REGISTER", "1");
        CHECK_COND(SciDAVisOle::ensureRegistrationAtStartup(&err) && suite60RegistryExe() == before,
                   "60h: TORIGRAPH_NO_OLE_REGISTER did not suppress registration");
        // Restore what the run was started with (a run started with TORIGRAPH_NO_OLE_REGISTER=1
        // must not begin rewriting the registration in later suites).
        if (hadNoRegister)
            qputenv("TORIGRAPH_NO_OLE_REGISTER", prevNoRegister);
        else
            qunsetenv("TORIGRAPH_NO_OLE_REGISTER");
        std::cout << "[INFO] Suite 60h: registered server = "
                  << QDir::toNativeSeparators(info.executable).toStdString()
                  << (info.pointsToSelf ? " (this build)" : " (left untouched)") << "\n";
    }

    // ---- 60i. copy page as 300 dpi image always replaces the clipboard -----------------
    {
        QImage tinyImg(10, 10, QImage::Format_RGB32);
        tinyImg.fill(Qt::red);
        w60->activateSubWindow(ml);
        QApplication::processEvents();
        if (w60->d_workspace.activeSubWindow() == ml) {
            // Put a small image first, then copy the page; repeat if another program put its
            // own data on the clipboard in between (the check reads the clipboard back).
            const char *clipErr = retryClipboardStep(
                    [&]() -> const char * {
                        QApplication::clipboard()->setImage(tinyImg);
                        QApplication::processEvents();
                        w60->copyPageAsHighDpiImage();
                        markClipboardAfterCopy();
                        QApplication::processEvents();
                        const QImage got = QApplication::clipboard()->image();
                        return got.width() > 100
                                ? nullptr
                                : "60i: page image did not replace the clipboard image";
                    },
                    "60i");
            CHECK_COND(clipErr == nullptr, (clipErr ? clipErr : ""));
        } else {
            std::cout << "[INFO] Suite 60i: graph could not be activated in headless window, skipped\n";
        }
    }

    // ---- 60j (review fix): "graph only" embedding follows column formulas to other tables
    {
        Table *base = w60->newTable("Base60j", 3, 2);
        Table *mid = w60->newTable("Mid60j", 3, 2);
        Table *top = w60->newTable("Top60j", 3, 2);
        Table *other = w60->newTable("Other60j", 3, 2);
        CHECK_COND(base && mid && top && other, "60j: tables");
        for (int r = 0; r < 3; ++r) {
            base->setCell(r, 0, r);
            base->setCell(r, 1, r * 2.0);
            mid->setCell(r, 0, r);
            top->setCell(r, 0, r);
            top->setCell(r, 1, r + 1.0); // values for the curve (formula not run here)
        }
        mid->column(1)->setFormula(Interval<int>(0, 2), "col(\"Base60j/2\")*2");
        top->column(1)->setFormula(Interval<int>(0, 2), "tablecol(\"Mid60j\", \"2\")+1");
        MultiLayer *mlj = w60->multilayerPlot(top, QStringList() << "Top60j_2", Graph::LineSymbols);
        CHECK_COND(mlj != nullptr, "60j: graph");
        const QStringList deps = SciDAVisOle::dataDependenciesForWindow(w60, mlj);
        std::cout << "[INFO] 60j dependencies: " << deps.join(", ").toStdString() << "\n";
        CHECK_COND(deps.contains("Top60j") && deps.contains("Mid60j") && deps.contains("Base60j"),
                   "60j: tables referenced by column formulas are not embedded");
        CHECK_COND(!deps.contains("Other60j"), "60j: an unrelated table was embedded");
        mlj->askOnCloseEvent(false);
        mlj->close();
        QApplication::processEvents();
    }

    // ---- 60k (review fix): a plain bitmap fallback is reported as such, not as "nothing"
    {
        SciDAVisOle::OleCopyResult r;
        r.windowName = "Graph3D60k";
        r.errorReason = "test";
        r.staticFallbackOnClipboard = true;
        r.pngDpi = 0;
        SciDAVisOle::reportCopyResult(w60, r, false);
        const QString msg = qApp->property("torigraph_last_ole_copy_message").toString();
        CHECK_COND(!msg.contains("Nothing was placed") && !msg.contains("EMF")
                           && !msg.contains(QString::fromUtf8("何も載せていません")),
                   "60k: bitmap fallback reported as 'nothing placed' / as EMF");
    }

    SciDAVisOle::setEmbedScopeSetting(prevScope);
    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    std::cout << "[INFO] Suite 60 PowerPoint / Word OLE link passed!\n";
    return 0;
}

// =============================================================================
// Suite 62: Beginner navigation, menus, ribbon state and screen fit
//   B1 F1 guide, B4 empty-table hint, B6/B7 plotting from the ribbon, C2 inspector details,
//   C3 tool-mode indicator, C4 ribbon actions, C5 dialogs on 1366x768 @150 %, C6 menu
//   duplicates, C7 graph context menu, C8 shortcuts, C11 undo target, C12 command search,
//   A11 save-as result, G1 other-field features.
// =============================================================================
#include "UxNavigation.h"
#include <QScrollArea>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QListWidget>
#include "GraphInspectorDock.h"

static int suite62CountMenusWithAction(ApplicationWindow *mw, QAction *a)
{
    int n = 0;
    for (QMenu *m : mw->findChildren<QMenu *>()) {
        if (m && a && m->actions().contains(a))
            ++n;
    }
    return n;
}

static bool suite62IsDescendant(QObject *child, QObject *ancestor)
{
    for (QObject *p = child; p; p = p->parent()) {
        if (p == ancestor)
            return true;
    }
    return false;
}

int runSuite62BeginnerNavigationMenusAndScreenFit(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 62: Beginner Navigation, Menus, Ribbon State & Screen Fit...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 62");
    ModernRibbonBar *ribbon = mw->modernRibbonBar();
    CHECK_COND(ribbon != nullptr, "62: ribbon missing");

    // ---- 62a (B1, C8): F1 opens bundled JA/EN guide matching appLanguage; shortcuts are unique
    {
        const QString prevLang62a = mw->appLanguage;
        mw->switchToLanguage(QStringLiteral("ja"));
        QCoreApplication::processEvents();
        const QUrl guideJa = mw->quickGuideUrl();
        CHECK_COND(guideJa.isLocalFile() && guideJa.toLocalFile().endsWith("manual/index.html"),
                   "62a: F1 guide in JA mode does not resolve to the bundled manual/index.html (got '"
                           << guideJa.toLocalFile().toStdString() << "')");
        QFile fJa(guideJa.toLocalFile());
        CHECK_COND(fJa.open(QIODevice::ReadOnly), "62a: JA guide file cannot be read");
        const QString htmlJa = QString::fromUtf8(fJa.readAll());
        for (const char *needle : { "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x9F\xE3\x82\x93\xE3\x82\xAC\xE3\x82\xA4\xE3\x83\x89", // かんたんガイド
                                    "PowerPoint", "SmartScreen", "Origin", "register-ole.ps1" }) {
            CHECK_COND(htmlJa.contains(QString::fromUtf8(needle)),
                       "62a: JA guide misses a required section: " << needle);
        }

        mw->switchToLanguage(QStringLiteral("en"));
        QCoreApplication::processEvents();
        const QUrl guideEn = mw->quickGuideUrl();
        CHECK_COND(guideEn.isLocalFile() && guideEn.toLocalFile().endsWith("manual/index.en.html"),
                   "62a: F1 guide in EN mode does not resolve to the bundled manual/index.en.html (got '"
                           << guideEn.toLocalFile().toStdString() << "')");
        QFile fEn(guideEn.toLocalFile());
        CHECK_COND(fEn.open(QIODevice::ReadOnly), "62a: EN guide file cannot be read");
        const QString htmlEn = QString::fromUtf8(fEn.readAll());
        for (const char *needle : { "Quick Guide", "PowerPoint" }) {
            CHECK_COND(htmlEn.contains(QString::fromUtf8(needle)),
                       "62a: EN guide misses a required section: " << needle);
        }

        mw->switchToLanguage(prevLang62a.isEmpty() ? QStringLiteral("en") : prevLang62a);
        QCoreApplication::processEvents();
        auto actionsWithKey = [mw](const QKeySequence &ks) {
            QStringList names;
            for (QAction *a : mw->findChildren<QAction *>()) {
                if (a && a->shortcuts().contains(ks))
                    names << a->objectName();
            }
            return names;
        };
        CHECK_COND(actionsWithKey(QKeySequence(Qt::Key_F1)) == QStringList { "actionShowHelp" },
                   "62a: F1 must belong to Help only");
        CHECK_COND(actionsWithKey(QKeySequence("Ctrl+Shift+S")) == QStringList { "actionSaveProjectAs" },
                   "62a: Ctrl+Shift+S must be Save As");
        CHECK_COND(actionsWithKey(QKeySequence("Ctrl+Shift+F")) == QStringList { "actionNewFunctionPlot" },
                   "62a: Ctrl+Shift+F must be the new function plot");
        CHECK_COND(actionsWithKey(QKeySequence("Ctrl+F")).isEmpty(),
                   "62a: Ctrl+F must stay free for searching a worksheet");
    }

    qApp->setProperty("torigraph_noninteractive", true);

    // ---- 62b (B4): empty table shows the paste hint; data replaces it with "Plot this data"
    Table *t = mw->newTable();
    CHECK_COND(t != nullptr, "62b: newTable failed");
    QApplication::processEvents();
    const bool hintSupported = !mw->isNonInteractive();
    if (hintSupported) {
        QLabel *hint = t->findChild<QLabel *>("ToriEmptyTableHintLabel");
        CHECK_COND(hint != nullptr && !hint->isHidden(), "62b: empty-table hint missing");
    }

    // ---- 62c (B7): an empty worksheet does not produce an empty graph
    {
        const int graphsBefore = mw->findChildren<MultiLayer *>().size();
        CHECK_COND(!mw->plotBlockerForTable(t, Graph::LineSymbols).isEmpty(),
                   "62c: empty table not reported as blocker");
        CHECK_COND(mw->plotColumnsFromTable(t, Graph::LineSymbols) == nullptr,
                   "62c: plotColumnsFromTable created a graph from an empty table");
        CHECK_COND(mw->findChildren<MultiLayer *>().size() == graphsBefore,
                   "62c: a graph window appeared for an empty table");
    }

    for (int r = 0; r < 6; ++r) {
        t->setCell(r, 0, r + 1.0);
        t->setCell(r, 1, (r + 1.0) * (r + 1.0));
    }
    t->notifyChanges();
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
    if (hintSupported) {
        QLabel *hint = t->findChild<QLabel *>("ToriEmptyTableHintLabel");
        CHECK_COND(!hint || hint->isHidden(), "62b: hint still shown after data arrived");
        CHECK_COND(t->findChild<QFrame *>("ToriPlotThisDataBar") != nullptr,
                   "62b: 'Plot this data' bar not offered after data arrived");
    }
    CHECK_COND(mw->plotBlockerForTable(t, Graph::LineSymbols).isEmpty(),
               "62c: table with data reported as blocked");

    // ---- 62d (B6): a plot button with a graph active plots the graph's worksheet again
    MultiLayer *ml = mw->plotColumnsFromTable(t, Graph::LineSymbols);
    CHECK_COND(ml != nullptr && ml->activeGraph() && ml->activeGraph()->curves() == 1,
               "62d: plotColumnsFromTable did not plot the Y column");
    mw->activateSubWindow(ml);
    QApplication::processEvents();
    {
        const int before = mw->findChildren<MultiLayer *>().size();
        const int curveTypeBefore = ml->activeGraph()->curveType(0);
        ribbon->triggerPlot2D(Graph::Scatter);
        QApplication::processEvents();
        CHECK_COND(mw->findChildren<MultiLayer *>().size() == before + 1,
                   "62d: ribbon plot button did nothing while a graph was active");
        CHECK_COND(ml->activeGraph()->curveType(0) == curveTypeBefore,
                   "62d: ribbon plot button re-styled the active graph instead of plotting");

        // A graph without a source worksheet: no new window, but a message.
        MultiLayer *bare = mw->newGraph("Suite62Bare");
        mw->activateSubWindow(bare);
        QApplication::processEvents();
        mw->statusBar()->clearMessage();
        const int before2 = mw->findChildren<MultiLayer *>().size();
        ribbon->triggerPlot2D(Graph::Line);
        QApplication::processEvents();
        CHECK_COND(mw->findChildren<MultiLayer *>().size() == before2,
                   "62d: plotting from a graph without worksheet created a window");
        CHECK_COND(!mw->statusBar()->currentMessage().isEmpty(),
                   "62d: no reason shown when the plot button cannot work");
        bare->askOnCloseEvent(false);
        bare->close();
        QApplication::processEvents();
    }

    // ---- 62e (C3): mode indicator and tool buttons follow the active tool
    mw->activateSubWindow(ml);
    QApplication::processEvents();
    {
        Graph *g = ml->activeGraph();
        g->zoom(true);
        mw->refreshToolModeIndicator();
        const QString mode = mw->currentToolModeName();
        CHECK_COND(!mode.isEmpty(), "62e: zoom mode not reported");
        QToolButton *ind = mw->statusBar()->findChild<QToolButton *>("StatusBarToolModeIndicator");
        CHECK_COND(ind != nullptr && !ind->isHidden() && ind->text().contains(mode),
                   "62e: status bar mode indicator not shown");
        QToolButton *zoomBtn = ribbon->actionBoundButton("btnZoomIn");
        QToolButton *ptrBtn = ribbon->actionBoundButton("btnPointer");
        CHECK_COND(zoomBtn && ptrBtn, "62e: ribbon tool buttons missing");
        CHECK_COND(zoomBtn->isChecked() && !ptrBtn->isChecked(),
                   "62e: ribbon zoom button not checked while zooming");
        mw->cancelCurrentToolMode();
        CHECK_COND(!g->zoomOn() && mw->currentToolModeName().isEmpty(),
                   "62e: cancelCurrentToolMode left the zoom on");
        CHECK_COND(ind->isHidden(), "62e: indicator still visible in pointer mode");
        CHECK_COND(ptrBtn->isChecked() && !zoomBtn->isChecked(),
                   "62e: pointer button not checked after leaving the mode");
    }

    // ---- 62f (C4): ribbon buttons for actions follow the action state
    for (const char *name : { "actionUndo", "actionRedo", "btnPointer", "btnZoomIn", "btnCursor" }) {
        QToolButton *b = ribbon->actionBoundButton(name);
        QAction *a = mw->findChild<QAction *>(name);
        CHECK_COND(b != nullptr && a != nullptr, "62f: ribbon button/action missing: " << name);
        ribbon->syncActionBoundButtons();
        CHECK_COND(b->isEnabled() == a->isEnabled(), "62f: enabled state not synced: " << name);
    }
    {
        QMenu *addMenu = ribbon->findChild<QMenu *>("RibbonAddToGraphMenu");
        CHECK_COND(addMenu != nullptr, "62f: ribbon 'Add' menu missing");
        for (const char *name : { "actionAddText", "actionNewLegend", "actionAddErrorBars" }) {
            QAction *a = mw->findChild<QAction *>(name);
            CHECK_COND(a && addMenu->actions().contains(a), "62f: ribbon 'Add' menu lacks " << name);
        }
        const QList<QMenu *> ribbonMenus = ribbon->findChildren<QMenu *>();
        for (const char *name : { "actionPlot2VerticalLayers", "actionPlot4Layers" }) {
            QAction *a = mw->findChild<QAction *>(name);
            bool inRibbonLayers = false;
            for (QMenu *m : ribbonMenus) {
                if (m && a && m->actions().contains(a)) {
                    inRibbonLayers = true;
                    break;
                }
            }
            CHECK_COND(inRibbonLayers, "62f: ribbon layers menu lacks " << name);
        }
    }
    {
        QAction *undo = mw->findChild<QAction *>("actionUndo");
        undo->setEnabled(false);
        CHECK_COND(!ribbon->actionBoundButton("actionUndo")->isEnabled(),
                   "62f: Undo button did not follow QAction::changed");
        mw->updateUndoRedoActionsForActiveWindow();
    }

    // ---- 62g (C11): Undo names the window it acts on
    {
        const QString before = ml->captureUndoSnapshot();
        ml->activeGraph()->setAxisTitle(QwtPlot::xBottom, "Suite62 X");
        ml->pushUndoSnapshot("Suite62 Title", before);
        mw->updateUndoRedoActionsForActiveWindow();
        QAction *undo = mw->findChild<QAction *>("actionUndo");
        CHECK_COND(undo->isEnabled() && undo->text().contains(ml->name()),
                   "62g: Undo text does not name the graph window");
        CHECK_COND(ribbon->actionBoundButton("actionUndo")->isEnabled(),
                   "62g: ribbon Undo not enabled after an undoable change");
    }

    // ---- 62h (C7): graph context menu is short, selection-first, one analysis submenu
    {
        QMenu cm;
        mw->buildGraphContextMenu(cm, ml, false);
        int topItems = 0, analysisSubmenus = 0;
        QStringList names;
        for (QAction *a : cm.actions()) {
            if (a->isSeparator())
                continue;
            ++topItems;
            names << a->objectName();
        }
        for (QAction *a : cm.actions()) {
            QMenu *sub = a->menu();
            if (!sub)
                continue;
            for (QAction *sa : sub->actions()) {
                if (sa->menu() && sa->menu()->actions().contains(
                                          mw->findChild<QAction *>("actionPeakAnalyzerDialog")))
                    ++analysisSubmenus;
            }
        }
        std::cout << "[INFO] Suite 62h graph context menu top-level items: " << topItems << "\n";
        CHECK_COND(topItems <= 24, "62h: graph context menu still has too many top-level items");
        for (const char *must : { "actionShowAxisDialog", "actionNewLegend", "actionAddText",
                                  "actionCopyGraphOle" }) {
            CHECK_COND(names.contains(QString::fromLatin1(must)),
                       "62h: graph context menu lacks " << must);
        }
        CHECK_COND(!names.contains("actionPeakAnalyzerDialog"),
                   "62h: peak analyzer listed flat in the graph context menu");
        CHECK_COND(analysisSubmenus == 0,
                   "62h: blank canvas context menu should not nest the full 9-submenu analysis tree");
    }

    // ---- 62i (C6): duplicate menu registrations removed
    {
        QAction *merge = mw->findChild<QAction *>("actionMergeGraphs");
        QAction *peak = mw->findChild<QAction *>("actionPeakAnalyzerDialog");
        QAction *langJa = mw->findChild<QAction *>("actionLangJapanese");
        CHECK_COND(merge && peak && langJa, "62i: menu actions missing");
        for (QMenu *m : mw->findChildren<QMenu *>()) {
            CHECK_COND(m->actions().count(merge) <= 1, "62i: Merge Graphs registered twice in a menu");
        }
        CHECK_COND(suite62CountMenusWithAction(mw, langJa) == 1,
                   "62i: language menu is still listed in several menus");
        const int peakMenus = suite62CountMenusWithAction(mw, peak);
        std::cout << "[INFO] Suite 62i menus listing Peak Analyzer: " << peakMenus << "\n";
        CHECK_COND(peakMenus <= 3, "62i: Peak Analyzer still listed in too many menus");
    }

    // ---- 62j (C12, G1): command search lists disabled commands; other-field features hidden
    {
        CommandPalettePopup *cp = ribbon->commandPalette();
        CHECK_COND(cp != nullptr, "62j: command palette missing");
        CHECK_COND(!mw->showsAdvancedDomainFeatures(), "62j: other-field features shown by default");
        QAction *roc = mw->findChild<QAction *>("actionROCCurve");
        CHECK_COND(roc && !roc->isVisible(), "62j: ROC action visible by default");
        cp->refreshCommands();
        cp->setFilterText(cleanMnemonicForSuite62(roc->text()));
        bool rocListed = false;
        for (int i = 0; i < cp->resultsList()->count(); ++i) {
            if (cp->resultsList()->item(i)->text() == cleanMnemonicForSuite62(roc->text()))
                rocListed = true;
        }
        CHECK_COND(!rocListed, "62j: hidden ROC command still found by command search");

        mw->setShowAdvancedDomainFeatures(true);
        CHECK_COND(roc->isVisible(), "62j: ROC not shown after enabling other-field features");
        mw->setShowAdvancedDomainFeatures(false);
        CHECK_COND(!roc->isVisible(), "62j: ROC not hidden again");

        // A command that is disabled in the current context is listed dimmed with a reason.
        QAction *undo = mw->findChild<QAction *>("actionUndo");
        undo->setEnabled(false);
        cp->refreshCommands();
        cp->setFilterText(cleanMnemonicForSuite62(undo->text()));
        bool sawDimmed = false;
        for (int i = 0; i < cp->resultsList()->count(); ++i) {
            QListWidgetItem *it = cp->resultsList()->item(i);
            if (it->data(Qt::UserRole + 4).toBool() && !it->toolTip().isEmpty())
                sawDimmed = true;
        }
        cp->setFilterText(QString());
        mw->updateUndoRedoActionsForActiveWindow();
        CHECK_COND(sawDimmed, "62j: no disabled command listed with its reason");
    }

    // ---- 62k (C5): dialogs fit 1366x768 at 150 % (911x512 logical)
    {
        const QRect smallScreen(0, 0, 911, 512);
        QDialog tall;
        QVBoxLayout *lay = new QVBoxLayout(&tall);
        QWidget *big = new QWidget(&tall);
        big->setMinimumSize(700, 900);
        lay->addWidget(big);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                    &tall);
        lay->addWidget(bb);
        tall.setMinimumSize(780, 960);
        CHECK_COND(ToriUx::fitWindowToAvailableScreen(&tall, smallScreen),
                   "62k: tall dialog content was not wrapped in a scroll area");
        QScrollArea *sa = tall.findChild<QScrollArea *>("ToriScreenFitScrollArea");
        CHECK_COND(sa && suite62IsDescendant(big, sa), "62k: content not inside the scroll area");
        CHECK_COND(!suite62IsDescendant(bb, sa), "62k: OK/Cancel scrolled away with the content");
        CHECK_COND(tall.minimumWidth() <= 911 && tall.minimumHeight() <= 512,
                   "62k: minimum size still larger than the screen");
        CHECK_COND(tall.layout()->totalMinimumSize().height() <= 512,
                   "62k: wrapped layout still needs more height than the screen");

        PlotDialog pd(false, mw);
        ToriUx::fitWindowToAvailableScreen(&pd, smallScreen);
        CHECK_COND(pd.minimumWidth() <= 911 && pd.minimumHeight() <= 512
                           && pd.width() <= 911 && pd.height() <= 512,
                   "62k: PlotDialog does not fit 1366x768 at 150 %");
        CHECK_COND(!pd.layout() || pd.layout()->totalMinimumSize().height() <= 512,
                   "62k: PlotDialog layout needs more height than the screen");
    }

    // ---- 62l (C2): inspector sections lead to their full dialog
    {
        GraphInspectorDock *dock = mw->graphInspectorDock();
        CHECK_COND(dock && dock->findChild<QPushButton *>("InspectorDetailsButton"),
                   "62l: inspector has no 'Details...' button");
    }

    // ---- 62m (A11): an untitled project reports whether Save As really saved
    {
        bool (ApplicationWindow::*saveAs)() = &ApplicationWindow::saveProjectAs;
        CHECK_COND(saveAs != nullptr, "62m: saveProjectAs does not return its result");
        const QString prevName = mw->projectname;
        mw->projectname = "untitled";
        CHECK_COND(!mw->saveProject(), "62m: untitled save without a file name reported success");
        mw->projectname = prevName;
    }

    // ---- 62n (C14): ribbon tabs are short nouns; materials analyses come right after Plot
    {
        CHECK_COND(ribbon->tabCount() == 7, "62n: ribbon tab count changed");
        const QString t2 = ribbon->tabTitle(2);
        CHECK_COND(t2 == QLatin1String("Materials")
                           || t2.contains(QString::fromUtf8("\xE6\x9D\x90\xE6\x96\x99")),
                   "62n: materials tab is not next to Plot");
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 62 Beginner Navigation, Menus, Ribbon State & Screen Fit passed!\n";
    return 0;
}

// =============================================================================
#include <QSpinBox>
#include <QComboBox>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include "ScaleDraw.h"

// =========================================================================
// Suite 58: figure defaults, style presets and image/vector export
//           (B2, B9, E1-E7 of the 2026-11-21 review)
// =========================================================================
static QByteArray suite58ReadAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

static Table *suite58MakeTable(ApplicationWindow *mw, const QString &name, int nY)
{
    Table *t = mw->newTable(name, 8, nY + 1);
    if (!t)
        return nullptr;
    t->setColName(0, "x");
    for (int c = 1; c <= nY; ++c)
        t->setColName(c, QStringLiteral("y%1").arg(c));
    for (int r = 0; r < 8; ++r) {
        t->setCell(r, 0, r + 1.0);
        for (int c = 1; c <= nY; ++c)
            t->setCell(r, c, (r + 1.0) * c + 0.5 * c);
    }
    return t;
}

int runSuite58FigureDefaultsPresetsAndExport(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 58: Figure Defaults, Style Presets & Image/Vector Export Gate...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 58");
    qApp->setProperty("torigraph_noninteractive", true);
    // Keep the user's real preset file untouched.
    QStandardPaths::setTestModeEnabled(true);
    QFile::remove(MultiLayer::userStylePresetsFilePath());
    QTemporaryDir tmp58;
    CHECK_COND(tmp58.isValid(), "Suite 58: QTemporaryDir failed");

    const QString prevNewGraphPreset = MultiLayer::newGraphStylePresetId();
    // Restore "not chosen" as not chosen (an empty name removes the setting); reading
    // defaultCurvePaletteName() would turn it into an explicit choice.
    const bool hadStoredPalette =
            ApplicationWindow::getSettings().contains(QStringLiteral("/Curves/DefaultPalette"));
    const QString prevPalette = hadStoredPalette ? Graph::defaultCurvePaletteName() : QString();
    const MultiLayer::MultiPanelSizing prevSizing = MultiLayer::multiPanelSizing();

    // ---- 58a: palettes (E5) -------------------------------------------------------------
    {
        CHECK_COND(Graph::okabeItoColors().size() == 8, "58a: Okabe-Ito must have 8 colours");
        CHECK_COND(Graph::okabeItoColors().first() == QColor("#0072b2")
                           && Graph::okabeItoColors().last() == QColor("#000000"),
                   "58a: Okabe-Ito order/last colour");
        CHECK_COND(Graph::tab10Colors().size() == 10 && Graph::tab10Colors().first() == QColor("#1f77b4"),
                   "58a: tab10 definition");
        CHECK_COND(Graph::paletteColors(QStringLiteral("ISIJ / Origin Default")) == Graph::tab10Colors(),
                   "58a: legacy name 'ISIJ / Origin Default' must still resolve to tab10");
        CHECK_COND(Graph::paletteColors(QStringLiteral("Okabe-Ito")) == Graph::okabeItoColors(),
                   "58a: short name Okabe-Ito must resolve");
        CHECK_COND(Graph::canonicalPaletteName(QStringLiteral("Cool-Warm")) == QStringLiteral("CoolWarm"),
                   "58a: Cool-Warm alias");
        const QStringList builtIns = Graph::builtInPaletteNames();
        CHECK_COND(builtIns.contains(QStringLiteral("Matplotlib (tab10)"))
                           && !builtIns.contains(QStringLiteral("ISIJ / Origin Default")),
                   "58a: tab10 must be listed under its real name");
        const QList<QColor> oc = Graph::originClassicColors();
        CHECK_COND(oc.size() >= 5 && oc.at(0) == QColor(Qt::black) && oc.at(1) == QColor(Qt::red)
                           && oc.at(2) == QColor(Qt::blue),
                   "58a: Origin classic starts black, red, blue");

        Graph::setDefaultCurvePaletteName(Graph::originClassicPaletteName());
        MultiLayer::setNewGraphStylePresetId(QString()); // plain graph for this check
        Table *t = suite58MakeTable(mw, "Pal58", 3);
        CHECK_COND(t != nullptr, "58a: table");
        QStringList cols;
        cols << (t->name() + "_y1") << (t->name() + "_y2") << (t->name() + "_y3");
        MultiLayer *ml = mw->multilayerPlot(t, cols, Graph::LineSymbols);
        CHECK_COND(ml && ml->activeGraph() && ml->activeGraph()->curves() == 3, "58a: graph");
        Graph *g = ml->activeGraph();
        const QColor c0 = g->curve(0)->pen().color();
        const QColor c1 = g->curve(1)->pen().color();
        const QColor c2 = g->curve(2)->pen().color();
        std::cout << "[INFO] 58a curve colours: " << c0.name().toStdString() << " "
                  << c1.name().toStdString() << " " << c2.name().toStdString() << "\n";
        CHECK_COND(c0 == QColor(Qt::black) && c1 == QColor(Qt::red) && c2 == QColor(Qt::blue),
                   "58a: new curves must follow Origin default colours (black, red, blue)");
        CHECK_COND(g->curve(0)->symbol().style() == QwtSymbol::Rect
                           && g->curve(1)->symbol().style() == QwtSymbol::Ellipse
                           && g->curve(2)->symbol().style() == QwtSymbol::UTriangle,
                   "58a: symbol shapes must cycle square, circle, up-triangle");
        QApplication::processEvents();
        CHECK_COND(ml->pageUnit() == MultiLayer::UnitPx,
                   "58a: with the new-graph preset off the page must stay a pixel page");

        // Switching the default palette takes effect for new curves.
        Graph::setDefaultCurvePaletteName(QStringLiteral("Okabe-Ito"));
        MultiLayer *ml2 = mw->multilayerPlot(t, QStringList() << cols.at(0), Graph::LineSymbols);
        CHECK_COND(ml2 && ml2->activeGraph(), "58a: second graph");
        CHECK_COND(ml2->activeGraph()->curve(0)->pen().color() == QColor("#0072b2"),
                   "58a: Okabe-Ito default palette not applied to new curve");
        Graph::setDefaultCurvePaletteName(Graph::originClassicPaletteName());
        ml2->askOnCloseEvent(false);
        ml2->close();
        ml->askOnCloseEvent(false);
        ml->close();
        QApplication::processEvents();
    }

    // ---- 58b: preset for new graphs, bars keep inward ticks, frameless legend (E1) --------
    MultiLayer *isijMl = nullptr;
    {
        MultiLayer::setNewGraphStylePresetId(QStringLiteral("isij_85mm"));
        CHECK_COND(MultiLayer::newGraphStylePresetId() == QStringLiteral("isij_85mm"),
                   "58b: new-graph preset setting");
        Table *t = suite58MakeTable(mw, "New58", 2);
        CHECK_COND(t != nullptr, "58b: table");
        QStringList cols;
        cols << (t->name() + "_y1") << (t->name() + "_y2");
        // 58b0: scripts / Python RPC / headless runs (non-interactive) keep the plain page; the
        // preset is only for graphs made by hand (SDK users call apply_style_preset()).
        {
            MultiLayer *scriptMl = mw->multilayerPlot(t, cols, Graph::LineSymbols);
            CHECK_COND(scriptMl && scriptMl->activeGraph(), "58b0: graph");
            for (int k = 0; k < 5; ++k)
                QApplication::processEvents();
            CHECK_COND(scriptMl->pageUnit() == MultiLayer::UnitPx,
                       "58b0: new-graph preset was applied to a graph made non-interactively");
            scriptMl->askOnCloseEvent(false);
            scriptMl->close();
            QApplication::processEvents();
        }
        // The graphs below stand for graphs made by hand: interactive while they are created.
        qApp->setProperty("torigraph_noninteractive", QVariant());
        isijMl = mw->multilayerPlot(t, cols, Graph::LineSymbols);
        CHECK_COND(isijMl && isijMl->activeGraph(), "58b: graph");
        for (int k = 0; k < 5; ++k)
            QApplication::processEvents();
        CHECK_COND(isijMl->pageUnit() == MultiLayer::UnitMm
                           && std::fabs(isijMl->pageWidthPhysical() - 85.0) < 0.01
                           && std::fabs(isijMl->pageHeightPhysical() - 85.0) < 0.01
                           && isijMl->pageDpi() == 600,
                   "58b: new graph must get the ISIJ 85 mm / 600 dpi page");
        CHECK_COND(isijMl->undoStack() && isijMl->undoStack()->count() == 0,
                   "58b: applying the new-graph preset must not leave an undo step");
        Graph *g = isijMl->activeGraph();
        CHECK_COND(g->legend() && g->legend()->frameStyle() == Legend::None,
                   "58b: default legend must be frameless");

        Table *tb = suite58MakeTable(mw, "Bar58", 1);
        MultiLayer *barMl = mw->multilayerPlot(tb, QStringList() << (tb->name() + "_y1"),
                                               Graph::VerticalBars);
        CHECK_COND(barMl && barMl->activeGraph(), "58b: bar graph");
        for (int k = 0; k < 5; ++k)
            QApplication::processEvents();
        qApp->setProperty("torigraph_noninteractive", true);
        const QList<int> majT = barMl->activeGraph()->plotWidget()->getMajorTicksType();
        CHECK_COND(!majT.isEmpty() && majT.at(QwtPlot::xBottom) != ScaleDraw::Out
                           && majT.at(QwtPlot::yLeft) != ScaleDraw::Out,
                   "58b: bar charts must no longer be forced to outward ticks");
        barMl->askOnCloseEvent(false);
        barMl->close();
        QApplication::processEvents();
    }

    // ---- 58c: E2 set values instead of minimums; titles keep their pt --------------------
    {
        Graph *g = isijMl->activeGraph();
        QwtPlotCurve *c0 = g->curve(0);
        QwtPlotCurve *c1 = g->curve(1);
        CHECK_COND(c0 && c1, "58c: curves");
        // User customises curve 1
        QPen p1 = c1->pen();
        p1.setWidth(5);
        c1->setPen(p1);
        CHECK_COND(isijMl->applyStylePresetById(QStringLiteral("slide_28pt")), "58c: slide preset");
        CHECK_COND(qRound(c0->pen().widthF()) == 3, "58c: slide preset must set curve width 3");
        CHECK_COND(c0->symbol().size().width() == 12, "58c: slide preset symbol size 12");
        CHECK_COND(qRound(c1->pen().widthF()) == 5, "58c: user-customised width must be kept");
        CHECK_COND(isijMl->applyStylePresetById(QStringLiteral("isij_85mm")), "58c: ISIJ preset");
        std::cout << "[INFO] 58c width after ISIJ: " << c0->pen().widthF()
                  << " symbol " << c0->symbol().size().width() << "\n";
        CHECK_COND(qRound(c0->pen().widthF()) == 2, "58c: switching back must make lines thinner again");
        CHECK_COND(c0->symbol().size().width() == 7, "58c: switching back must restore symbol size 7");
        CHECK_COND(qRound(c1->pen().widthF()) == 5, "58c: user-customised width must survive");

        // Long axis title: no wrapping, no shrinking, but a warning.
        const QString longTitle = QStringLiteral(
                "Hydrogen desorption rate measured by thermal desorption spectroscopy after charging");
        g->setAxisTitle(QwtPlot::xBottom, longTitle);
        CHECK_COND(isijMl->applyStylePresetById(QStringLiteral("isij_85mm")), "58c: re-apply");
        const QString after = g->axisTitle(QwtPlot::xBottom);
        CHECK_COND(!after.contains(QLatin1String("<br")) && !after.contains(QLatin1Char('\n')),
                   "58c: preset must not insert line breaks into axis titles");
        const QFont tf = g->axisTitleFont(QwtPlot::xBottom);
        const double pt = tf.pointSizeF() > 0 ? tf.pointSizeF() : tf.pointSize();
        CHECK_COND(std::fabs(pt - 11.0) < 0.05, "58c: axis title must keep the preset 11 pt");
        CHECK_COND(!isijMl->lastStylePresetWarnings().isEmpty(),
                   "58c: an over-long axis title must produce a warning");
        g->setAxisTitle(QwtPlot::xBottom, QStringLiteral("Time, t / s"));
        CHECK_COND(isijMl->applyStylePresetById(QStringLiteral("isij_85mm")), "58c: re-apply 2");
        CHECK_COND(isijMl->lastStylePresetWarnings().isEmpty(), "58c: short title must not warn");
    }

    // ---- 58d: new built-in presets (E3) ----------------------------------------------------
    {
        MultiLayer::GraphStylePresetSpec lab, twoCol;
        CHECK_COND(MultiLayer::findStylePreset(QStringLiteral("lab_slide_83mm"), &lab),
                   "58d: lab slide preset missing");
        CHECK_COND(std::fabs(lab.pageWidthMm - 83.3) < 1e-6 && std::fabs(lab.pageHeightMm - 83.3) < 1e-6,
                   "58d: lab slide page 83.3 mm");
        // 83.3 mm x 140 % = 116.6 mm (11.7 cm); text sized to read 18 pt on the slide after the
        // 140 % enlargement: 18 / 1.40 = 12.86 -> 12.9 pt (was 11.2 pt = 15.7 pt on the slide).
        CHECK_COND(std::fabs(lab.pageWidthMm * 1.40 - 116.62) < 0.01, "58d: 140 % gives 11.7 cm");
        CHECK_COND(std::fabs(lab.axisTitlePt * 1.40 - 18.0) < 0.1
                           && std::fabs(lab.tickLabelPt * 1.40 - 18.0) < 0.1,
                   "58d: lab slide text reads 18 pt at 140 %");
        CHECK_COND(MultiLayer::findStylePreset(QStringLiteral("journal_2col_175mm"), &twoCol)
                           && std::fabs(twoCol.pageWidthMm - 175.0) < 1e-6,
                   "58d: 175 mm double-column preset missing");
    }

    // ---- 58e: multi-panel sizing (E7) --------------------------------------------------------
    {
        MultiLayer::setNewGraphStylePresetId(QString());
        MultiLayer *ml = mw->newGraph(QStringLiteral("Panels58"));
        CHECK_COND(ml != nullptr, "58e: graph");
        ml->addLayer();
        ml->setCols(2);
        ml->setRows(1);
        ml->arrangeLayers(false, false);
        MultiLayer::setMultiPanelSizing(MultiLayer::PanelSizingPerPanel);
        CHECK_COND(ml->applyStylePresetById(QStringLiteral("isij_85mm")), "58e: apply 1");
        std::cout << "[INFO] 58e per-panel page: " << ml->pageWidthPhysical() << " x "
                  << ml->pageHeightPhysical() << "\n";
        CHECK_COND(std::fabs(ml->pageWidthPhysical() - 170.0) < 0.01
                           && std::fabs(ml->pageHeightPhysical() - 85.0) < 0.01,
                   "58e: 2 panels x 85 mm must give 170 x 85 mm (not 153 x 85)");
        MultiLayer::setMultiPanelSizing(MultiLayer::PanelSizingTwoColumn175);
        CHECK_COND(ml->applyStylePresetById(QStringLiteral("isij_85mm")), "58e: apply 2");
        CHECK_COND(std::fabs(ml->pageWidthPhysical() - 175.0) < 0.01
                           && std::fabs(ml->pageHeightPhysical() - 87.5) < 0.01,
                   "58e: double-column mode must give 175 x 87.5 mm");
        MultiLayer::setMultiPanelSizing(prevSizing);
        ml->askOnCloseEvent(false);
        ml->close();
        QApplication::processEvents();
    }

    // ---- 58f: JSON version, shared folder, template page setup (E4) ------------------------
    {
        const QString newer = tmp58.filePath("newer.json");
        {
            QFile f(newer);
            CHECK_COND(f.open(QIODevice::WriteOnly), "58f: write json");
            f.write("{\"version\": 99, \"presets\": [{\"id\": \"custom_v99\", \"name\": \"V99\", "
                    "\"page_width_mm\": 90, \"page_height_mm\": 60, \"curve_width\": 4, "
                    "\"future_key\": true}]}");
        }
        QString err;
        const int n = MultiLayer::importStylePresetsFromJson(newer, &err);
        CHECK_COND(n == 1, "58f: newer preset file must still import its known fields");
        CHECK_COND(!err.isEmpty(), "58f: newer format version must produce a warning");
        MultiLayer::GraphStylePresetSpec v99;
        CHECK_COND(MultiLayer::findStylePreset(QStringLiteral("custom_v99"), &v99)
                           && v99.minCurveWidth == 4 && std::fabs(v99.pageWidthMm - 90.0) < 1e-6,
                   "58f: imported values");
        {
            QFile uf(MultiLayer::userStylePresetsFilePath());
            CHECK_COND(uf.open(QIODevice::ReadOnly), "58f: user preset file");
            const QJsonObject root = QJsonDocument::fromJson(uf.readAll()).object();
            CHECK_COND(root.value(QStringLiteral("version")).toInt()
                               == MultiLayer::stylePresetFormatVersion(),
                       "58f: user preset file must carry the current format version");
        }

        QDir().mkpath(tmp58.filePath("shared"));
        {
            QFile f(tmp58.filePath("shared/lab.json"));
            CHECK_COND(f.open(QIODevice::WriteOnly), "58f: write shared json");
            f.write("{\"version\": 2, \"presets\": [{\"id\": \"lab_shared_fig\", \"name\": "
                    "\"Lab shared\", \"page_width_mm\": 80, \"page_height_mm\": 80}, "
                    "{\"id\": \"isij_85mm\", \"name\": \"hijack\", \"page_width_mm\": 30}]}");
        }
        MultiLayer::setSharedStylePresetsFolder(tmp58.filePath("shared"));
        MultiLayer::GraphStylePresetSpec shared, isij;
        CHECK_COND(MultiLayer::findStylePreset(QStringLiteral("lab_shared_fig"), &shared)
                           && shared.readOnly && std::fabs(shared.pageWidthMm - 80.0) < 1e-6,
                   "58f: shared preset must be listed read-only");
        CHECK_COND(MultiLayer::findStylePreset(QStringLiteral("isij_85mm"), &isij)
                           && std::fabs(isij.pageWidthMm - 85.0) < 1e-6,
                   "58f: a shared file must not override a built-in preset");
        QString delErr;
        CHECK_COND(!MultiLayer::deleteStylePreset(QStringLiteral("lab_shared_fig"), &delErr)
                           && !delErr.isEmpty(),
                   "58f: shared presets cannot be deleted");
        MultiLayer::setSharedStylePresetsFolder(QString());
        CHECK_COND(!MultiLayer::findStylePreset(QStringLiteral("lab_shared_fig"), nullptr),
                   "58f: shared preset must disappear when the folder is cleared");

        isijMl->setPanelLabels(true, MultiLayer::PanelOutsideTopLeft, QStringLiteral("(%a)"), 12);
        const QString tpl = isijMl->saveAsTemplate(QString());
        CHECK_COND(tpl.contains(QLatin1String("<ToriPageSetup"))
                           && tpl.contains(QLatin1String("unit=\"1\""))
                           && tpl.contains(QLatin1String("dpi=\"600\""))
                           && tpl.contains(QLatin1String("w=\"85\"")),
                   "58f: template must contain the page setup");
        CHECK_COND(tpl.contains(QLatin1String("<ToriAspectRatio"))
                           && tpl.contains(QLatin1String("<ToriPanelLabels")),
                   "58f: template must contain aspect lock and panel labels");
        CHECK_COND(!tpl.contains(QLatin1String("<ToriWinSize")),
                   "58f: template must not carry the window size");
        isijMl->setPanelLabels(false);
    }

    // ---- 58g: vector / raster export sizes (E6) --------------------------------------------
    {
        const QString svg = tmp58.filePath("isij.svg");
        isijMl->exportSVG(svg);
        const QByteArray svgData = suite58ReadAll(svg);
        QRegularExpression wRx(QStringLiteral("<svg[^>]*\\swidth=\"([0-9.]+)mm\""));
        QRegularExpression hRx(QStringLiteral("<svg[^>]*\\sheight=\"([0-9.]+)mm\""));
        const QRegularExpressionMatch wm = wRx.match(QString::fromUtf8(svgData));
        const QRegularExpressionMatch hm = hRx.match(QString::fromUtf8(svgData));
        CHECK_COND(wm.hasMatch() && hm.hasMatch(), "58g: SVG root size in mm not found");
        std::cout << "[INFO] 58g SVG size: " << wm.captured(1).toStdString() << " x "
                  << hm.captured(1).toStdString() << " mm\n";
        CHECK_COND(std::fabs(wm.captured(1).toDouble() - 85.0) < 0.011
                           && std::fabs(hm.captured(1).toDouble() - 85.0) < 0.011,
                   "58g: SVG must be 85.00 mm (old 96 dpi rounding gave 84.93 mm)");
        // The drawing itself must stay in 96-dpi page pixels (85 mm = 321 px); a high
        // generator resolution would double-scale fonts and pens.
        QRegularExpression vbRx(QStringLiteral("<svg[^>]*\\sviewBox=\"0 0 ([0-9.]+) ([0-9.]+)\""));
        const QRegularExpressionMatch vb = vbRx.match(QString::fromUtf8(svgData));
        CHECK_COND(vb.hasMatch() && std::fabs(vb.captured(1).toDouble() - 321.0) <= 2.0,
                   "58g: SVG viewBox must stay in 96-dpi page pixels");

        const QString pdf = tmp58.filePath("isij.pdf");
        isijMl->exportVector(pdf);
        const QByteArray pdfData = suite58ReadAll(pdf);
        CHECK_COND(pdfData.startsWith("%PDF"), "58g: PDF not written");
        QRegularExpression mbRx(QStringLiteral("/MediaBox\\s*\\[\\s*0\\s+0\\s+([0-9.]+)\\s+([0-9.]+)"));
        const QRegularExpressionMatch mb = mbRx.match(QString::fromLatin1(pdfData.constData(), pdfData.size()));
        CHECK_COND(mb.hasMatch(), "58g: PDF MediaBox missing");
        // 85 mm = 240.94 pt (Qt rounds the PDF page to whole points: 241)
        CHECK_COND(std::fabs(mb.captured(1).toDouble() - 240.94) < 0.6
                           && std::fabs(mb.captured(2).toDouble() - 240.94) < 0.6,
                   "58g: PDF page must be 85 x 85 mm");


#ifdef Q_OS_WIN
        // EMF fallback raster: physical size follows the raster's own dpi (was 3.1x too large)
        QImage r300(1000, 500, QImage::Format_ARGB32);
        r300.fill(Qt::white);
        r300.setDotsPerMeterX(qRound(300 / 0.0254));
        r300.setDotsPerMeterY(qRound(300 / 0.0254));
        const OleRenderedGraph rg = SciDAVisEmf::renderFromImage(r300);
        std::cout << "[INFO] 58g fallback extent " << rg.extentHm.cx << " x " << rg.extentHm.cy
                  << " (0.01 mm)\n";
        // 1000 px / 300 dpi = 3.333 in = 84.67 mm
        CHECK_COND(std::abs(rg.extentHm.cx - 8467) <= 3 && std::abs(rg.extentHm.cy - 4233) <= 3,
                   "58g: fallback EMF extent must follow the 300 dpi raster");
#endif
    }

    // ---- 58h: export dialog defaults and summary (B2, B9) -----------------------------------
    {
        CHECK_COND(mw->d_export_resolution == 600, "58h: default export resolution must be 600 dpi");
        const QStringList filters = ImageExportDialog::supportedFilters();
        CHECK_COND(filters.contains("*.png") && filters.contains("*.tif") && filters.contains("*.pdf")
                           && filters.contains("*.svg") && !filters.contains("*.eps")
                           && !filters.contains("*.ps") && !filters.contains("*.jpg"),
                   "58h: export formats must be PNG/TIFF/PDF/SVG/EMF only");
        CHECK_COND(ImageExportDialog::normalizedFilter(".png") == "*.png"
                           && ImageExportDialog::normalizedFilter("*.eps") == "*.png"
                           && ImageExportDialog::normalizedFilter("*.tiff") == "*.tif",
                   "58h: stored filters must map onto the supported list");
        const QString sum = ImageExportDialog::sizeSummary(85.0, 85.0, 600, true);
        CHECK_COND(sum.contains("85.0") && sum.contains("2008") && sum.contains("600"),
                   "58h: size summary must show 85.0 mm, 600 dpi and 2008 px");

        mw->d_export_resolution = 300; // last used value; page DPI must win for a physical page
        mw->d_workspace.setActiveSubWindow(isijMl);
        ImageExportDialog dlg(mw, true, true);
        CHECK_COND(dlg.resolution() == 600, "58h: dialog must start from the page DPI (600)");
        CHECK_COND(dlg.nameFilters() == ImageExportDialog::displayNameFilters(),
                   "58h: dialog filters");
        dlg.selectFilter(QStringLiteral("*.png"));
        CHECK_COND(dlg.selectedNameFilter() == QLatin1String("*.png"),
                   "58h: normalized selectedNameFilter");
        CHECK_COND(dlg.infoText().contains("2008"), "58h: dialog must show the pixel size");
        mw->d_export_resolution = 600;
    }

    // ---- 58i: inspector page unit and layer dialog mm (E7) ----------------------------------
    {
        LayerDialog ld(mw);
        QComboBox *unitBox = ld.findChild<QComboBox *>(QStringLiteral("layerSpacingUnitCombo"));
        CHECK_COND(unitBox && unitBox->count() == 2, "58i: layer dialog unit combo missing");
        unitBox->setCurrentIndex(1);
        bool anyMm = false;
        for (QSpinBox *sb : ld.findChildren<QSpinBox *>())
            if (sb->suffix() == QLatin1String(" mm"))
                anyMm = true;
        CHECK_COND(anyMm, "58i: layer dialog spin boxes must switch to mm");
    }

    isijMl->askOnCloseEvent(false);
    isijMl->close();
    QApplication::processEvents();

    MultiLayer::setNewGraphStylePresetId(prevNewGraphPreset);
    Graph::setDefaultCurvePaletteName(prevPalette);
    QFile::remove(MultiLayer::userStylePresetsFilePath());
    QStandardPaths::setTestModeEnabled(false);
    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 58 Figure Defaults, Style Presets & Export Gate passed!\n";
    return 0;
}

