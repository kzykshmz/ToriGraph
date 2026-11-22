/***************************************************************************
    File                 : PythonAutomationServer.h
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
 ***************************************************************************/
#ifndef PYTHON_AUTOMATION_SERVER_H
#define PYTHON_AUTOMATION_SERVER_H

#include <QByteArray>
#include <QDialog>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class ApplicationWindow;
class Graph;
class MultiLayer;
class MyWidget;
class QCloseEvent;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QLocalServer;
class QLocalSocket;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QTcpServer;
class QTcpSocket;
class QTextDecoder;

class PythonAutomationServer : public QObject
{
    Q_OBJECT

public:
    using CustomCommandHook =
            std::function<QJsonObject(const QString &method, const QJsonObject &params, bool *handled)>;

    explicit PythonAutomationServer(ApplicationWindow *app, QObject *parent = nullptr);
    ~PythonAutomationServer() override;

    // Loopback TCP JSON-RPC 2.0 transport (newline-delimited UTF-8 JSON)
    bool startTcp(quint16 port = 0, const QHostAddress &bindAddress = QHostAddress::LocalHost);
    quint16 tcpPort() const;
    bool isTcpListening() const;
    void setTcpAuthToken(const QString &token);
    QString tcpAuthToken() const;
    bool callerWasNonInteractive() const { return d_caller_noninteractive; }

    // Local Named Pipe / Domain Socket JSON-RPC 2.0 transport (newline-delimited UTF-8 JSON)
    bool startLocalPipe(const QString &pipeName = QString());
    QString localPipeName() const;
    bool isLocalPipeListening() const;

    void stop();
    QString endpointString() const;
    bool writeEndpointPortFile(const QString &filePath) const;

    // Synchronous JSON-RPC 2.0 dispatch (for socket framing & direct C++ test harness)
    QJsonObject handleRpcRequest(const QJsonObject &req);
    QByteArray handleRpcLine(const QByteArray &line);

    // Optional hook for ApplicationWindow UI state (focus HUD, workspace mode, auto-updater, etc.)
    void setCustomCommandHook(CustomCommandHook hook);

    static QStringList supportedMethods();
    //! Resolves a palette name given to graph.apply_palette (exact name, alias, prefix, word
    //! prefix, then substring) among  palettes; empty if none matches.  normalizedKey gets
    //! the normalised request (used for the extra built-in fallbacks).
    static QString matchPaletteName(const QString &raw, const QStringList &palettes,
                                    QString *normalizedKey = nullptr);
    static QString generatePythonScriptForWindow(ApplicationWindow *app, MyWidget *window,
                                                 bool standalone = true,
                                                 const QString &embedData = QStringLiteral("inline"));

    // Helpers for running external Python scripts connected to this server
    static QString resolvePythonExecutable();
    static QString resolvePythonExecutable(bool *explicitInvalidOut);
    static QString resolvePythonSdkDir();
    QProcess *startPythonScriptProcess(const QString &scriptPath,
                                       const QStringList &extraArgs = QStringList(),
                                       QObject *parent = nullptr);

signals:
    void rpcRequestHandled(const QString &method, bool ok);

private slots:
    void onNewTcpConnection();
    void onTcpReadyRead();
    void onTcpDisconnected();

    void onNewLocalConnection();
    void onLocalReadyRead();
    void onLocalDisconnected();

private:
    QJsonObject dispatchMethod(const QString &method, const QJsonObject &params);
    void writeTcpTokenFile(quint16 port);
    void removeTcpTokenFile();

    ApplicationWindow *d_app = nullptr;
    QTcpServer *d_tcp_server = nullptr;
    QHostAddress d_bind_address = QHostAddress::LocalHost;
    quint16 d_tcp_port = 0;
    QString d_tcp_auth_token;
    QString d_tcp_token_file_path;
    QHash<QTcpSocket *, QByteArray> d_tcp_buffers;
    QSet<QTcpSocket *> d_authed_tcp_sockets;
    bool d_caller_noninteractive = true;

    QLocalServer *d_local_server = nullptr;
    QString d_local_pipe_name;
    QHash<QLocalSocket *, QByteArray> d_local_buffers;
    bool d_prefer_local_pipe = false;

    CustomCommandHook d_custom_hook;
    bool d_focus_hud_fallback = false;
    QString d_workspace_mode_fallback = QStringLiteral("mdi");
    QString d_last_undo_domain;
    QString d_last_undo_graph;
};

class PythonConsoleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PythonConsoleDialog(ApplicationWindow *app, PythonAutomationServer *server,
                                 QWidget *parent = nullptr);
    ~PythonConsoleDialog() override;

    static PythonConsoleDialog *showDialog(ApplicationWindow *app, PythonAutomationServer *server);

    void loadAndRunScriptFile(const QString &pyFilePath);
    bool loadScriptFile(const QString &pyFilePath);
    bool saveScriptFile(const QString &pyFilePath);
    void retranslateUi();

public slots:
    void runCurrentScript();
    void stopRunningScript();
    void openScriptDialog();
    void saveScriptDialog();
    void clearOutput();
    void reject() override;

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onExampleSelected(int index);
    void onProcessReadyReadStdout();
    void onProcessReadyReadStderr();
    void onProcessFinished(int exitCode, int exitStatus);
    void onProcessErrorOccurred(int processError);

private:
    void buildUi();
    void refreshEndpointDisplay();
    void appendOutput(const QString &text, bool isError = false);
    QString exampleScriptCode(int index) const;
    void cleanupTempScript();

    QPointer<ApplicationWindow> d_app;
    QPointer<PythonAutomationServer> d_server;

    QLabel *d_endpoint_label = nullptr;
    QLineEdit *d_endpoint_edit = nullptr;
    QLabel *d_example_label = nullptr;
    QComboBox *d_example_combo = nullptr;

    QPushButton *d_run_btn = nullptr;
    QPushButton *d_stop_btn = nullptr;
    QPushButton *d_open_btn = nullptr;
    QPushButton *d_save_btn = nullptr;
    QPushButton *d_clear_btn = nullptr;
    QPushButton *d_close_btn = nullptr;

    QLabel *d_editor_label = nullptr;
    QPlainTextEdit *d_editor = nullptr;
    QLabel *d_output_label = nullptr;
    QPlainTextEdit *d_output = nullptr;

    QProcess *d_process = nullptr;
    QTextDecoder *d_stdout_decoder = nullptr;
    QTextDecoder *d_stderr_decoder = nullptr;
    QString d_temp_script_path;
    QString d_current_file_path;

    void resetStreamDecoders();

    static QPointer<PythonConsoleDialog> s_instance;
};

#endif // PYTHON_AUTOMATION_SERVER_H
