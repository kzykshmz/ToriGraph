/***************************************************************************
    File                 : main.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : SciDAVis main function

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
#include "ApplicationWindow.h"
#include "ModernUiTheme.h"
#include "PythonAutomationServer.h"
#ifdef Q_OS_WIN
#include "OleServer.h"
#endif

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QSplashScreen>
#include <QTimer>

#include <cstdio>
#include <typeinfo>

// The following stuff is for the doxygen title page
/*!  \mainpage SciDAVis - Scientific Data Analysis and Visualization - API documentation

  \section description Program description:
SciDAVis is a user-friendly data analysis and visualization program primarily aimed at high-quality
plotting of scientific data. It strives to combine an intuitive, easy-to-use graphical user
interface with powerful features such as Python scriptability.

 The SciDAVis web page can be found at<br>
 <a href="http://scidavis.sourceforge.net">http://scidavis.sourceforge.net</a><br>

All suggestions and contributions are most welcome!<br>
If you want to contribute code, please read the notes on \ref style "coding style" first.
There is also a section with some notes about \ref future "future plans".
<br>

  \section libs SciDAVis uses the following libraries:
  <a href="http://www.trolltech.com/products/qt/index.html">Qt</a>,
  <a href="http://qwt.sourceforge.net/index.html">Qwt</a>,
  <a href="http://qwtplot3d.sourceforge.net/">QwtPlot3D</a>,
  <a href="http://sources.redhat.com/gsl/">GSL</a>,
  <a href="http://muparser.sourceforge.net/">muParser</a>,
  <a href="http://www.zlib.net/">zlib</a>,
  and <a href="http://sourceforge.net/projects/liborigin/">liborigin</a>.
  <br>

  \page future Future Plans

  - Make SciDAVis easier to extend and maintain by modularizing it and add
    generic plug-in support, support for implementing extensions in Python.
        For example, ideally it should be possible to implement new plot or marker
    types without modifying existing classes.
  - A full-featured multi-level undo/redo framework (based on QUndoCommand).
         [ undo/redo assigned to thzs; plug-in/Python extension support assigned to knut_f ]
  - Revise some internal APIs that use undocumented protocols for the
    strings that get passed around. Wherever possible, consistency should be
    checked at compile-time. This would also probably eliminate the need to
    place restrictions on object/column names.
  - Switch file format to a completely XML-based one so we can write a
    more robust parser based on Qt's XML support. Legacy support for the old
    format could be a plug-in. Use the new XML reader/write in Qt 4.3 for
        this since it is very fast.
  - Document as much as possible using doxygen and write simple demo
    plug-ins. Also a collection of demo projects, tutorials etc. would
        be nice. The latter could be contributed by non-developers.
  - Modularize the build process so that Python support could use SIP's
    build system instead of the current hacks. Support for muParser,
    liborigin and zlib could also be split out into plug-ins on the basis
    that you shouldn't have to install libraries you don't use. Also reduces
    compile times during bugfixing.
         [ assigned to knut_f ]
  - Change some internal APIs that depend on the notion of a selected/current item.
    This will lead to a cleaner and simpler interface for plug-ins and Python scripts.
        This will have to wait until the model/view implementation is done since it
        will extend the handling of selections to non-contiguous selections.
  - Replace or fork Qwt by a system based on Qt4's features and optimize it for
    optimum export quality. Very important: Eliminate the restriction to integer
        coordinates and implement true plot scaling.
  - Add support for custom page sizes once Trolltech fixes this issue:
    http://trolltech.com/developer/task-tracker/index_html?method=entry&id=99441
  .

  %Note about modularization: this is mainly about internal reorganizations.
  Most of the current features should remain part of the main executable, but use interfaces similar
or identical to those used by plug-ins. This should ease maintanance and make adding new features to
the core application a no-brainer once they're available as plug-ins. Support for Python, liborigin
and zlib could be real, external plug-ins since many people probably don't need them, but this can
be decided shortly before distribution and ideally by modifying just a few lines in a build script.

  \section features Other Planned Features
  - Waterfall plots
  - Polar charts
  - Ternary plots
  - Improved data analysis: base-line detection tool, more built-in fit functions...
  - Improved import of Origin project files
  - More import/export filters including Open Document spreadsheets (plugin)

  \sa ApplicationWindow, MultiLayer, Graph, Graph3D, ImageMarker, Legend, MyWidget,
SelectionMoveResizer, MyParser, Note, ScriptEdit, Table

  \page style Coding Style
  The following rules are not used everywhere (yet), but are intended as guidelines for new code and
eventually old code should be adapted as well.

  - Files use Unix-style line endings ("\n").
  - Class names start with a capital letter, object/variable names with a lower case letter.
  - Methods and functions are named likeThis(), variables like_this and instance variables are
prefixed with "d_".
  - Property access methods use Qt style: property() and setProperty().
  - Every file should be named exactly like the class inside and there should be only one class per
file, with the exception of really short classes like LayerButton.
  - For indentations, tabs are preferred because they allow everyone to choose the indentation depth
for him/herself.
*/

struct Application : public QApplication
{
    bool headless = false;

    Application(int &argc, char **argv) : QApplication(argc, argv) { }

    // catch exception, and display their contents as modal dialogue (or stderr in headless mode)
    bool notify(QObject *receiver, QEvent *event)
    {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::exception &e) {
            const QString msg = tr("Error ") + e.what() + tr(" sending event ")
                    + typeid(*event).name() + tr(" to object ")
                    + qPrintable(receiver ? receiver->objectName() : QString()) + " \""
                    + (receiver ? typeid(*receiver).name() : "null") + "\"";
            if (headless) {
                std::fprintf(stderr, "%s\n", msg.toUtf8().constData());
                std::fflush(stderr);
            } else {
                QMessageBox::critical(0, tr("Error!"), msg);
            }
        } catch (...) // shouldn't happen...
        {
            const QString msg = tr("Error <unknown> sending event") + typeid(*event).name()
                    + tr(" to object ")
                    + qPrintable(receiver ? receiver->objectName() : QString()) + " \""
                    + (receiver ? typeid(*receiver).name() : "null") + "\"";
            if (headless) {
                std::fprintf(stderr, "%s\n", msg.toUtf8().constData());
                std::fflush(stderr);
            } else {
                QMessageBox::critical(0, tr("Error!"), msg);
            }
        }
        return false;
    }
};

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
    QCoreApplication::setOrganizationName("ToriGraph");
    QCoreApplication::setApplicationName("ToriGraph");

    Application app(argc, argv);
    ModernUiTheme::applyModernTheme(&app);

#ifdef __APPLE__
    // set python home to bundle specific directory
    setenv("PYTHONHOME",(QCoreApplication::applicationDirPath()+"/../Resources").toStdString().c_str(),true);
#endif

    ApplicationWindow::getSettings();

    QStringList args = app.arguments();
    args.removeFirst(); // remove application name

    bool oleEmbedded = false;
#ifdef Q_OS_WIN
    for (int i = args.count() - 1; i >= 0; --i) {
        if (args[i].compare("-Embedding", Qt::CaseInsensitive) == 0
            || args[i].compare("/Embedding", Qt::CaseInsensitive) == 0) {
            oleEmbedded = true;
            args.removeAt(i);
        }
    }
    // Keep a working registration (e.g. of the installed copy) as it is; only register this
    // executable when there is none or it points to a missing program. Development builds can
    // opt out with TORIGRAPH_NO_OLE_REGISTER=1. Explicit repair: Help > Repair PowerPoint Link.
    SciDAVisOle::ensureRegistrationAtStartup();
    SciDAVisOle::initializeOleServer(oleEmbedded);
#endif

    bool headlessMode = false;
    bool hasRpcPort = false;
    quint16 rpcPort = 0;
    QString rpcPortFile;
    QString rpcPipeName;
    QString runPythonScript;
    QStringList runPythonExtraArgs;
    QStringList filteredArgs;

    for (int i = 0; i < args.count(); ++i) {
        const QString &arg = args[i];
        if (arg == QStringLiteral("--headless")) {
            headlessMode = true;
        } else if (arg == QStringLiteral("--rpc-port") && i + 1 < args.count()) {
            hasRpcPort = true;
            rpcPort = static_cast<quint16>(args[++i].toUInt());
        } else if (arg.startsWith(QStringLiteral("--rpc-port="))) {
            hasRpcPort = true;
            rpcPort = static_cast<quint16>(arg.mid(11).toUInt());
        } else if (arg == QStringLiteral("--rpc-port-file") && i + 1 < args.count()) {
            rpcPortFile = args[++i];
        } else if (arg.startsWith(QStringLiteral("--rpc-port-file="))) {
            rpcPortFile = arg.mid(16);
        } else if (arg == QStringLiteral("--rpc-pipe") && i + 1 < args.count()) {
            rpcPipeName = args[++i];
        } else if (arg.startsWith(QStringLiteral("--rpc-pipe="))) {
            rpcPipeName = arg.mid(11);
        } else if (arg == QStringLiteral("--run-python") && i + 1 < args.count()) {
            runPythonScript = args[++i];
        } else if (arg.startsWith(QStringLiteral("--run-python="))) {
            runPythonScript = arg.mid(13);
        } else {
            filteredArgs.append(arg);
        }
    }

    app.headless = headlessMode;
    app.setProperty("torigraph_noninteractive", headlessMode);

    if ((filteredArgs.count() == 1) && (filteredArgs[0] == "-a" || filteredArgs[0] == "--about")
        && !headlessMode) {
        ApplicationWindow::about();
#ifdef Q_OS_WIN
        SciDAVisOle::shutdownOleServer();
#endif
        exit(0);
    } else if (oleEmbedded) {
        app.setQuitOnLastWindowClosed(false);
    } else {
        if (headlessMode)
            app.setQuitOnLastWindowClosed(false);

        QPointer<ApplicationWindow> mw = new ApplicationWindow;
        mw->setHeadlessMode(headlessMode);
        mw->applyUserSettings();
        mw->newTable();
        mw->activateSubWindow();
        mw->savedProject();
        mw->parseCommandLineArguments(filteredArgs);

        ApplicationWindow *activeMw = mw.data();
        if (!activeMw) {
            for (QWidget *tlw : QApplication::topLevelWidgets()) {
                if (ApplicationWindow *cand = qobject_cast<ApplicationWindow *>(tlw)) {
                    activeMw = cand;
                    break;
                }
            }
        }

        if (activeMw) {
            activeMw->setHeadlessMode(headlessMode);
            if (headlessMode && activeMw->isVisible())
                activeMw->hide();

            // Auto-save recovery copies are for people working in the GUI only: automation
            // sessions must neither write them nor be blocked by the "restore?" question.
            if (!headlessMode && !hasRpcPort && rpcPortFile.isEmpty() && rpcPipeName.isEmpty()
                && runPythonScript.isEmpty()) {
                ApplicationWindow::setCrashRecoveryEnabled(true);
                QPointer<ApplicationWindow> recoverMw = activeMw;
                QTimer::singleShot(1200, activeMw, [recoverMw]() {
                    if (recoverMw)
                        recoverMw->checkCrashRecovery();
                });
            }

            PythonAutomationServer *rpcServer = activeMw->pythonAutomationServer();
            if (rpcServer) {
                if (!rpcPipeName.isEmpty() && !hasRpcPort) {
                    if (rpcServer->isTcpListening())
                        rpcServer->stop();
                    rpcServer->startLocalPipe(rpcPipeName);
                } else {
                    if (hasRpcPort || !rpcServer->isTcpListening())
                        rpcServer->startTcp(hasRpcPort ? rpcPort : 0);
                    if (!rpcPipeName.isEmpty())
                        rpcServer->startLocalPipe(rpcPipeName);
                }
                if (!rpcPortFile.isEmpty())
                    rpcServer->writeEndpointPortFile(rpcPortFile);
                if (hasRpcPort || !rpcPortFile.isEmpty() || !rpcPipeName.isEmpty()
                    || headlessMode || !runPythonScript.isEmpty()) {
                    const QByteArray epLine = QByteArray("TORIGRAPH_RPC_ENDPOINT=")
                            + rpcServer->endpointString().toUtf8() + '\n';
                    std::fwrite(epLine.constData(), 1, static_cast<size_t>(epLine.size()), stdout);
                    std::fflush(stdout);
                }

                if (!runPythonScript.isEmpty()) {
                    const QString absScript = QFileInfo(runPythonScript).absoluteFilePath();
                    QProcess *pyProc = rpcServer->startPythonScriptProcess(
                            absScript, runPythonExtraArgs, activeMw);
                    if (pyProc) {
                        auto flushOut = [pyProc]() {
                            const QByteArray outBytes = pyProc->readAllStandardOutput();
                            if (!outBytes.isEmpty()) {
                                std::fwrite(outBytes.constData(), 1,
                                            static_cast<size_t>(outBytes.size()), stdout);
                                std::fflush(stdout);
                            }
                        };
                        auto flushErr = [pyProc]() {
                            const QByteArray errBytes = pyProc->readAllStandardError();
                            if (!errBytes.isEmpty()) {
                                std::fwrite(errBytes.constData(), 1,
                                            static_cast<size_t>(errBytes.size()), stderr);
                                std::fflush(stderr);
                            }
                        };
                        QObject::connect(pyProc, &QProcess::readyReadStandardOutput, pyProc,
                                         flushOut);
                        QObject::connect(pyProc, &QProcess::readyReadStandardError, pyProc,
                                         flushErr);

                        if (headlessMode) {
                            QObject::connect(
                                    pyProc,
                                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                                    &app,
                                    [&app, flushOut, flushErr](int exitCode,
                                                               QProcess::ExitStatus status) {
                                        flushOut();
                                        flushErr();
                                        const int rc =
                                                (status == QProcess::NormalExit) ? exitCode : 1;
                                        QTimer::singleShot(0, &app, [&app, rc]() { app.exit(rc); });
                                    });
                            QObject::connect(pyProc, &QProcess::errorOccurred, &app,
                                             [&app, flushErr](QProcess::ProcessError err) {
                                                 if (err == QProcess::FailedToStart) {
                                                     flushErr();
                                                     std::fprintf(stderr,
                                                                  "Failed to start Python "
                                                                  "interpreter for --run-python\n");
                                                     std::fflush(stderr);
                                                     QTimer::singleShot(
                                                             0, &app, [&app]() { app.exit(1); });
                                                 }
                                             });
                            if (pyProc->state() == QProcess::NotRunning) {
                                flushOut();
                                flushErr();
                                const int rc = (pyProc->exitStatus() == QProcess::NormalExit
                                                && pyProc->error() != QProcess::FailedToStart)
                                        ? pyProc->exitCode()
                                        : 1;
                                QTimer::singleShot(0, &app, [&app, rc]() { app.exit(rc); });
                            }
                        }
                    } else if (headlessMode) {
                        QTimer::singleShot(0, &app, [&app]() { app.exit(1); });
                    }
                }
            }

#ifdef SEARCH_FOR_UPDATES
            if (!headlessMode && !activeMw->isOleEmbeddedMode() && !activeMw->batchMode()
                && runPythonScript.isEmpty() && activeMw->autoSearchUpdates) {
                activeMw->autoSearchUpdatesRequest = true;
                activeMw->searchForUpdates();
            }
#endif
        }

        if (!headlessMode) {
            app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
        }
    }
    int rc = app.exec();
#ifdef Q_OS_WIN
    SciDAVisOle::shutdownOleServer();
#endif
    return rc;
}
