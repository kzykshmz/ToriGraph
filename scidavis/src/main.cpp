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
#include "CrashHandler.h"
#include "ModernUiTheme.h"
#include "PythonAutomationServer.h"
#include "globals.h"
#ifdef Q_OS_WIN
#include "OleServer.h"
#include <windows.h>
#include <shellapi.h>
#endif

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QSplashScreen>
#include <QTimer>

#include <cstdio>
#include <memory>
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // Fractional scales (125 % / 150 %) must not be rounded to 100 % / 200 %: plots and fonts
    // would otherwise be drawn at the wrong physical size. Must precede QApplication creation.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QCoreApplication::setOrganizationName("ToriGraph");
    QCoreApplication::setApplicationName("ToriGraph");

    // Extract arguments before creating QApplication so `--version` and invalid CLI flags do not
    // pay for full GUI/theme/font initialization.
    QStringList args;
#ifdef Q_OS_WIN
    {
        int wargc = 0;
        if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc)) {
            for (int i = 1; i < wargc; ++i)
                args.append(QString::fromWCharArray(wargv[i]));
            LocalFree(wargv);
        }
    }
    if (args.isEmpty() && argc > 1)
#endif
    {
        for (int i = 1; i < argc; ++i)
            args.append(QString::fromLocal8Bit(argv[i]));
    }

    bool oleEmbedded = false;
    bool headlessMode = false;
    bool wantVersion = false;
    bool hasRpcPort = false;
    quint16 rpcPort = 0;
    QString rpcPortFile;
    QString rpcPipeName;
    QString runPythonScript;
    QStringList runPythonExtraArgs;
    QStringList filteredArgs;
    bool afterRunPython = false;

    QString cmdLineError;

    // Parses a TCP port strictly (0 = ephemeral, 1..65535); the old toUInt()+quint16 cast
    // silently wrapped "70000" to 4464 and "abc" to 0.
    auto parseRpcPort = [&](const QString &text) {
        bool ok = false;
        const uint v = text.trimmed().toUInt(&ok);
        if (!ok || v > 65535u) {
            if (cmdLineError.isEmpty())
                cmdLineError = QStringLiteral("invalid --rpc-port value '%1' (expected 0-65535)")
                                       .arg(text);
            return;
        }
        hasRpcPort = true;
        rpcPort = static_cast<quint16>(v);
    };

    for (int i = 0; i < args.count(); ++i) {
        const QString &arg = args[i];
        if (arg == QStringLiteral("--")) {
            for (int j = i + 1; j < args.count(); ++j) {
                if (afterRunPython || !runPythonScript.isEmpty())
                    runPythonExtraArgs.append(args[j]);
                else
                    filteredArgs.append(args[j]);
            }
            break;
        }
#ifdef Q_OS_WIN
        if (arg.compare(QStringLiteral("-Embedding"), Qt::CaseInsensitive) == 0
            || arg.compare(QStringLiteral("/Embedding"), Qt::CaseInsensitive) == 0) {
            oleEmbedded = true;
            continue;
        }
#endif
        if (!afterRunPython
            && (arg == QStringLiteral("-v") || arg == QStringLiteral("--version"))) {
            wantVersion = true;
        } else if (arg == QStringLiteral("--headless")) {
            headlessMode = true;
        } else if (arg == QStringLiteral("--rpc-port")) {
            if (i + 1 < args.count()) {
                parseRpcPort(args[++i]);
            } else if (cmdLineError.isEmpty()) {
                cmdLineError = QStringLiteral("--rpc-port requires a value (0-65535)");
            }
        } else if (arg.startsWith(QStringLiteral("--rpc-port="))) {
            parseRpcPort(arg.mid(11));
        } else if (arg == QStringLiteral("--rpc-port-file")) {
            if (i + 1 < args.count() && !args[i + 1].trimmed().isEmpty()) {
                rpcPortFile = args[++i];
            } else if (cmdLineError.isEmpty()) {
                cmdLineError = QStringLiteral("--rpc-port-file requires a file path");
            }
        } else if (arg.startsWith(QStringLiteral("--rpc-port-file="))) {
            const QString val = arg.mid(16);
            if (!val.trimmed().isEmpty()) {
                rpcPortFile = val;
            } else if (cmdLineError.isEmpty()) {
                cmdLineError = QStringLiteral("--rpc-port-file requires a file path");
            }
        } else if (arg == QStringLiteral("--rpc-pipe")) {
            if (i + 1 < args.count() && !args[i + 1].trimmed().isEmpty()) {
                rpcPipeName = args[++i];
            } else if (cmdLineError.isEmpty()) {
                cmdLineError = QStringLiteral("--rpc-pipe requires a pipe name");
            }
        } else if (arg.startsWith(QStringLiteral("--rpc-pipe="))) {
            const QString val = arg.mid(11);
            if (!val.trimmed().isEmpty()) {
                rpcPipeName = val;
            } else if (cmdLineError.isEmpty()) {
                cmdLineError = QStringLiteral("--rpc-pipe requires a pipe name");
            }
        } else if (!afterRunPython && arg == QStringLiteral("--run-python")) {
            if (i + 1 < args.count() && !args[i + 1].trimmed().isEmpty()) {
                runPythonScript = args[++i];
                afterRunPython = true;
            } else if (cmdLineError.isEmpty()) {
                cmdLineError = QStringLiteral("--run-python requires a script path");
            }
        } else if (!afterRunPython && arg.startsWith(QStringLiteral("--run-python="))) {
            const QString val = arg.mid(13);
            if (!val.trimmed().isEmpty()) {
                runPythonScript = val;
                afterRunPython = true;
            } else if (cmdLineError.isEmpty()) {
                cmdLineError = QStringLiteral("--run-python requires a script path");
            }
        } else if (afterRunPython) {
            // Everything after the script path belongs to the script (including -h/--help,
            // -a, -l=..., -m, -x ...); only --headless/--rpc-* above are taken by ToriGraph.
            runPythonExtraArgs.append(arg);
        } else {
            filteredArgs.append(arg);
        }
    }

    if (!cmdLineError.isEmpty()) {
        QCoreApplication coreApp(argc, argv);
        const QByteArray msg = QByteArray("ToriGraph: ") + cmdLineError.toUtf8() + '\n';
        std::fwrite(msg.constData(), 1, static_cast<size_t>(msg.size()), stderr);
        std::fflush(stderr);
        return 2;
    }

    if (wantVersion) {
        QCoreApplication coreApp(argc, argv);
        const QString verText = SciDAVis::versionString() + SciDAVis::extraVersion()
                + QStringLiteral("\n") + QObject::tr("Released") + QStringLiteral(": ")
                + SciDAVis::releaseDateString() + QStringLiteral("\n")
                + SciDAVis::copyrightString() + QStringLiteral("\n");
        const QByteArray verUtf8 = verText.toUtf8();
        std::fwrite(verUtf8.constData(), 1, static_cast<size_t>(verUtf8.size()), stdout);
        std::fflush(stdout);
        return 0;
    }

    const bool wantHelpEarly = filteredArgs.contains(QStringLiteral("-h"))
            || filteredArgs.contains(QStringLiteral("--help"));
    if (wantHelpEarly && headlessMode) {
        QCoreApplication coreApp(argc, argv);
        QString s = QStringLiteral("\n") + QObject::tr("Usage") + QStringLiteral(": ToriGraph [")
                + QObject::tr("options") + QStringLiteral("] [") + QObject::tr("file")
                + QStringLiteral("_") + QObject::tr("name") + QStringLiteral("]\n\n");
        s += QObject::tr("Valid options are") + QStringLiteral(":\n");
        s += QStringLiteral("-a ") + QObject::tr("or") + QStringLiteral(" --about: ")
                + QObject::tr("show about dialog and exit") + QStringLiteral("\n");
        s += QStringLiteral("-h ") + QObject::tr("or") + QStringLiteral(" --help: ")
                + QObject::tr("show command line options") + QStringLiteral("\n");
        s += QStringLiteral("-l=XX ") + QObject::tr("or") + QStringLiteral(" --lang=XX: ")
                + QObject::tr("start ToriGraph in language")
                + QStringLiteral(" XX ('en', 'fr', 'de', ...)\n");
        s += QStringLiteral("-m ") + QObject::tr("or") + QStringLiteral(" --manual: ")
                + QObject::tr("show ToriGraph manual in a standalone window")
                + QStringLiteral("\n");
        s += QStringLiteral("-v ") + QObject::tr("or") + QStringLiteral(" --version: ")
                + QObject::tr("print ToriGraph version and release date")
                + QStringLiteral("\n");
        s += QStringLiteral("-x ") + QObject::tr("or") + QStringLiteral(" --execute: ")
                + QObject::tr("execute the script file given as argument")
                + QStringLiteral("\n");
        s += QStringLiteral("--headless: ")
                + QObject::tr("run without visible GUI window for Python automation")
                + QStringLiteral("\n");
        s += QStringLiteral("--rpc-port <port>: ")
                + QObject::tr("bind localhost JSON-RPC 2.0 server to <port> (0 = ephemeral)")
                + QStringLiteral("\n");
        s += QStringLiteral("--rpc-port-file <path>: ")
                + QObject::tr("write bound JSON-RPC endpoint URL atomically to <path>")
                + QStringLiteral("\n");
        s += QStringLiteral("--rpc-pipe <name>: ")
                + QObject::tr("start local named pipe JSON-RPC 2.0 server")
                + QStringLiteral("\n");
        s += QStringLiteral("--run-python <script.py> [args...]: ")
                + QObject::tr("execute Python automation script against this ToriGraph instance "
                              "(all following arguments are passed to the script)")
                + QStringLiteral("\n\n");
        s += QStringLiteral("'") + QObject::tr("file") + QStringLiteral("_")
                + QObject::tr("name") + QStringLiteral("' ")
                + QObject::tr("can be any .sciprj, .sciprj.gz, .qti, qti.gz, .opj, .ogm, "
                              ".ogw, .ogg, .org, .py or ASCII file")
                + QStringLiteral("\n");
        const QByteArray helpUtf8 = s.toUtf8();
        std::fwrite(helpUtf8.constData(), 1, static_cast<size_t>(helpUtf8.size()), stdout);
        std::fflush(stdout);
        return 0;
    }

    Application app(argc, argv);
    ToriGraphCrash::installCrashHandler();

    // -h/--help and -a/--about are answered before any OLE registration or window creation,
    // and leave through a normal return (no ::exit() that would skip OLE shutdown).
    {
        const bool wantHelp = filteredArgs.contains(QStringLiteral("-h"))
                || filteredArgs.contains(QStringLiteral("--help"));
        const bool wantAbout = !headlessMode && filteredArgs.count() == 1
                && (filteredArgs[0] == QStringLiteral("-a")
                    || filteredArgs[0] == QStringLiteral("--about"));
        if (wantHelp) {
            QString s = QStringLiteral("\n") + QObject::tr("Usage") + QStringLiteral(": ToriGraph [")
                    + QObject::tr("options") + QStringLiteral("] [") + QObject::tr("file")
                    + QStringLiteral("_") + QObject::tr("name") + QStringLiteral("]\n\n");
            s += QObject::tr("Valid options are") + QStringLiteral(":\n");
            s += QStringLiteral("-a ") + QObject::tr("or") + QStringLiteral(" --about: ")
                    + QObject::tr("show about dialog and exit") + QStringLiteral("\n");
            s += QStringLiteral("-h ") + QObject::tr("or") + QStringLiteral(" --help: ")
                    + QObject::tr("show command line options") + QStringLiteral("\n");
            s += QStringLiteral("-l=XX ") + QObject::tr("or") + QStringLiteral(" --lang=XX: ")
                    + QObject::tr("start ToriGraph in language")
                    + QStringLiteral(" XX ('en', 'fr', 'de', ...)\n");
            s += QStringLiteral("-m ") + QObject::tr("or") + QStringLiteral(" --manual: ")
                    + QObject::tr("show ToriGraph manual in a standalone window")
                    + QStringLiteral("\n");
            s += QStringLiteral("-v ") + QObject::tr("or") + QStringLiteral(" --version: ")
                    + QObject::tr("print ToriGraph version and release date")
                    + QStringLiteral("\n");
            s += QStringLiteral("-x ") + QObject::tr("or") + QStringLiteral(" --execute: ")
                    + QObject::tr("execute the script file given as argument")
                    + QStringLiteral("\n");
            s += QStringLiteral("--headless: ")
                    + QObject::tr("run without visible GUI window for Python automation")
                    + QStringLiteral("\n");
            s += QStringLiteral("--rpc-port <port>: ")
                    + QObject::tr("bind localhost JSON-RPC 2.0 server to <port> (0 = ephemeral)")
                    + QStringLiteral("\n");
            s += QStringLiteral("--rpc-port-file <path>: ")
                    + QObject::tr("write bound JSON-RPC endpoint URL atomically to <path>")
                    + QStringLiteral("\n");
            s += QStringLiteral("--rpc-pipe <name>: ")
                    + QObject::tr("start local named pipe JSON-RPC 2.0 server")
                    + QStringLiteral("\n");
            s += QStringLiteral("--run-python <script.py> [args...]: ")
                    + QObject::tr("execute Python automation script against this ToriGraph instance "
                                  "(all following arguments are passed to the script)")
                    + QStringLiteral("\n\n");
            s += QStringLiteral("'") + QObject::tr("file") + QStringLiteral("_")
                    + QObject::tr("name") + QStringLiteral("' ")
                    + QObject::tr("can be any .sciprj, .sciprj.gz, .qti, qti.gz, .opj, .ogm, "
                                  ".ogw, .ogg, .org, .py or ASCII file")
                    + QStringLiteral("\n");
#ifdef Q_OS_WIN
            if (!headlessMode) {
                // a GUI-subsystem executable has no console: show the text in a dialog
                QMessageBox::information(nullptr, QObject::tr("ToriGraph - Help"), s);
            } else
#endif
            {
                const QByteArray helpUtf8 = s.toUtf8();
                std::fwrite(helpUtf8.constData(), 1, static_cast<size_t>(helpUtf8.size()), stdout);
                std::fflush(stdout);
            }
            return 0;
        }
        if (wantAbout) {
            ModernUiTheme::applyModernTheme(&app);
            ApplicationWindow::about();
            return 0;
        }
    }

    ModernUiTheme::applyModernTheme(&app);

#ifdef __APPLE__
    // set python home to bundle specific directory
    setenv("PYTHONHOME",(QCoreApplication::applicationDirPath()+"/../Resources").toStdString().c_str(),true);
#endif

    ApplicationWindow::getSettings();

    app.headless = headlessMode;
    app.setProperty("torigraph_noninteractive", headlessMode);

#ifdef Q_OS_WIN
    if (headlessMode && !oleEmbedded) {
        SciDAVisOle::initializeOleClipboardOnly();
    } else {
        // Keep a working registration (e.g. of the installed copy) as it is; only register this
        // executable when there is none or it points to a missing program. Development builds can
        // opt out with TORIGRAPH_NO_OLE_REGISTER=1. Explicit repair: Help > Repair PowerPoint Link.
        SciDAVisOle::ensureRegistrationAtStartup();
        SciDAVisOle::initializeOleServer(oleEmbedded);
    }
#endif

    if (oleEmbedded) {
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

            const bool wantRpc = hasRpcPort || !rpcPipeName.isEmpty() || !rpcPortFile.isEmpty()
                    || headlessMode || !runPythonScript.isEmpty();

            // Auto-save recovery copies are for people working in the GUI only: automation
            // sessions must neither write them nor be blocked by the "restore?" question.
            if (!wantRpc) {
                ApplicationWindow::setCrashRecoveryEnabled(true);
                QPointer<ApplicationWindow> recoverMw = activeMw;
                QTimer::singleShot(1200, activeMw, [recoverMw]() {
                    if (recoverMw)
                        recoverMw->checkCrashRecovery();
                });
            }

            PythonAutomationServer *rpcServer = activeMw->pythonAutomationServer();
            if (rpcServer) {
                bool rpcStarted = true;
                QString rpcWhat;
                if (!wantRpc) {
                    // Normal interactive GUI launch: never expose an unsolicited RPC listener.
                    rpcServer->stop();
                } else if (hasRpcPort) {
                    rpcWhat = QStringLiteral("TCP port %1").arg(rpcPort);
                    rpcStarted = rpcServer->startTcp(rpcPort);
                    if (rpcStarted && !rpcPipeName.isEmpty()) {
                        rpcWhat = QStringLiteral("named pipe '%1'").arg(rpcPipeName);
                        rpcStarted = rpcServer->startLocalPipe(rpcPipeName);
                    }
                } else if (!rpcPipeName.isEmpty()) {
                    if (rpcServer->isTcpListening())
                        rpcServer->stop();
                    rpcWhat = QStringLiteral("named pipe '%1'").arg(rpcPipeName);
                    rpcStarted = rpcServer->startLocalPipe(rpcPipeName);
                } else if (!rpcPortFile.isEmpty()) {
                    rpcWhat = QStringLiteral("ephemeral TCP port");
                    rpcStarted = rpcServer->startTcp(0);
                } else {
                    // --headless / --run-python without --rpc-port and without --rpc-port-file:
                    // prefer per-user Named Pipe (QLocalServer::UserAccessOption).
                    if (rpcServer->isTcpListening())
                        rpcServer->stop();
                    rpcWhat = QStringLiteral("named pipe or ephemeral TCP port");
                    rpcStarted = rpcServer->startLocalPipe() || rpcServer->startTcp(0);
                }

                if (wantRpc && rpcStarted && !rpcPortFile.isEmpty()
                    && !rpcServer->writeEndpointPortFile(rpcPortFile)) {
                    rpcStarted = false;
                    rpcWhat = QStringLiteral("endpoint file '%1'").arg(rpcPortFile);
                }

                if (wantRpc && !rpcStarted) {
                    // Automation clients wait for the endpoint line: fail loudly, not silently.
                    const QByteArray errLine =
                            QByteArray("ToriGraph: failed to start the JSON-RPC server on ")
                            + rpcWhat.toUtf8() + '\n';
                    std::fwrite(errLine.constData(), 1, static_cast<size_t>(errLine.size()),
                                stderr);
                    std::fflush(stderr);
                    rpcServer->stop();
#ifdef Q_OS_WIN
                    SciDAVisOle::shutdownOleServer();
#endif
                    return 2;
                }

                if (wantRpc) {

                    const QByteArray epLine = QByteArray("TORIGRAPH_RPC_ENDPOINT=")
                            + rpcServer->endpointString().toUtf8() + '\n';
                    std::fwrite(epLine.constData(), 1, static_cast<size_t>(epLine.size()), stdout);
                    std::fflush(stdout);
                }

                if (!runPythonScript.isEmpty()) {
                    const QString absScript = QFileInfo(runPythonScript).absoluteFilePath();
                    auto capturedErr = std::make_shared<QByteArray>();
                    auto reportGuiPyError = [activeMw](const QString &msg) {
                        std::fprintf(stderr, "%s\n", msg.toUtf8().constData());
                        std::fflush(stderr);
                        if (activeMw && !QCoreApplication::closingDown()) {
                            QMessageBox::warning(activeMw,
                                                 QObject::tr("ToriGraph - Python Script Error"),
                                                 msg);
                        }
                    };
                    QProcess *pyProc = QFileInfo::exists(absScript)
                            ? rpcServer->startPythonScriptProcess(absScript, runPythonExtraArgs,
                                                                  activeMw)
                            : nullptr;
                    if (pyProc) {
                        auto flushOut = [pyProc]() {
                            const QByteArray outBytes = pyProc->readAllStandardOutput();
                            if (!outBytes.isEmpty()) {
                                std::fwrite(outBytes.constData(), 1,
                                            static_cast<size_t>(outBytes.size()), stdout);
                                std::fflush(stdout);
                            }
                        };
                        auto flushErr = [pyProc, capturedErr]() {
                            const QByteArray errBytes = pyProc->readAllStandardError();
                            if (!errBytes.isEmpty()) {
                                if (capturedErr->size() < 4096)
                                    capturedErr->append(errBytes.left(4096 - capturedErr->size()));
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
                        } else {
                            QPointer<ApplicationWindow> mwGuard(activeMw);
                            QObject::connect(
                                    pyProc, &QProcess::errorOccurred, &app,
                                    [mwGuard, absScript, flushErr](QProcess::ProcessError err) {
                                        if (err == QProcess::FailedToStart) {
                                            flushErr();
                                            const QString msg = QObject::tr(
                                                    "Failed to start Python interpreter for:\n%1")
                                                                        .arg(absScript);
                                            std::fprintf(stderr, "%s\n", msg.toUtf8().constData());
                                            std::fflush(stderr);
                                            if (mwGuard && !QCoreApplication::closingDown()) {
                                                QMessageBox::warning(
                                                        mwGuard,
                                                        QObject::tr("ToriGraph - Python Script Error"),
                                                        msg);
                                            }
                                        }
                                    });
                            QObject::connect(
                                    pyProc,
                                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                                    &app,
                                    [mwGuard, absScript, capturedErr, flushOut, flushErr](
                                            int exitCode, QProcess::ExitStatus status) {
                                        flushOut();
                                        flushErr();
                                        if ((status != QProcess::NormalExit || exitCode != 0)
                                            && mwGuard && !QCoreApplication::closingDown()) {
                                            const QString detail = QString::fromUtf8(*capturedErr).trimmed();
                                            QString msg = QObject::tr("Python script exited with code %1:\n%2")
                                                                  .arg(exitCode)
                                                                  .arg(absScript);
                                            if (!detail.isEmpty())
                                                msg += QStringLiteral("\n\n") + detail;
                                            QMessageBox::warning(
                                                    mwGuard,
                                                    QObject::tr("ToriGraph - Python Script Error"),
                                                    msg);
                                        }
                                    });
                        }
                    } else if (headlessMode) {
                        std::fprintf(stderr, "Failed to launch --run-python script: %s\n",
                                     absScript.toUtf8().constData());
                        std::fflush(stderr);
                        QTimer::singleShot(0, &app, [&app]() { app.exit(1); });
                    } else {
                        reportGuiPyError(QObject::tr("Python script not found or could not be started:\n%1")
                                                 .arg(absScript));
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
