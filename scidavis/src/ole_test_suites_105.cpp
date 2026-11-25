#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <cstring>
#include <memory>

// =============================================================================
// Suite 105: column types of the JSON-RPC text import (app.import_csv) follow the GUI import
// (AsciiTableImportFilter::importDroppedTextData, File > Open / drag & drop). Every case
// imports the same file on both paths and checks that each column has the same name, the same
// type and the same values (numeric: the same invalid cells; text: the same strings).
//   105a missing / Excel error tokens ("N.D.", "ND", "#N/A", "#DIV/0!", "#VALUE!", quoted
//        "N.D.") are missing values of a numeric column
//   105b one non-numeric cell in a column of numbers ("OVER", a repeated "x y" header line
//        between two data blocks) is an invalid cell of a numeric column (90 % rule)
//   105c a column holding only "--" / "N/A" is an empty numeric column; only quoted
//        missing tokens ("\"N/A\"") make it a text column
//   105d label columns, columns with too many non-numeric cells, too few numbers and date /
//        time columns stay text
// =============================================================================

namespace {
bool rpcIsError105(const QJsonObject &resp)
{
    return resp.contains(QStringLiteral("error"));
}
QJsonObject rpcResult105(const QJsonObject &resp)
{
    return resp.value(QStringLiteral("result")).toObject();
}
} // namespace

int runSuite105RpcImportColumnTypes(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 105: RPC import column types follow the GUI import...\n";

    struct NonInteractiveQuitGuard105
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard105()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard105()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard105;

    struct AppWinGuard105
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard105(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard105() { reset(); }
        AppWinGuard105(const AppWinGuard105 &) = delete;
        AppWinGuard105 &operator=(const AppWinGuard105 &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard105 w105(new ApplicationWindow());
    w105->applyUserSettings();
    w105->confirmCloseTable = false;
    w105->confirmCloseMatrix = false;
    w105->confirmClosePlot2D = false;
    w105->confirmClosePlot3D = false;
    w105->confirmCloseFolder = false;
    w105->resize(1280, 800);
    w105->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w105->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "105: pythonAutomationServer null");

    QTemporaryDir tmp105;
    CHECK_COND(tmp105.isValid(), "105: temporary directory");

    int rpcId = 10500;
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
    auto writeFile = [&](const QString &name, const QByteArray &bytes) -> QString {
        const QString path = tmp105.filePath(name);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(bytes);
            f.close();
        }
        return path;
    };
    auto readFile = [](const QString &path) -> QByteArray {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };

    // One imported column, the same shape for both paths: NaN = empty / invalid numeric cell.
    struct Col105
    {
        QString name;
        bool numeric = false;
        QVector<double> nums;
        QStringList texts;
    };
    const double NaN = std::numeric_limits<double>::quiet_NaN();

    auto rpcImport = [&](const QString &path) {
        QVector<Col105> out;
        const QJsonObject r =
                rpc(QStringLiteral("app.import_csv"), QJsonObject { { "path", path } });
        if (rpcIsError105(r))
            return out;
        const QString tbl = rpcResult105(r).value(QStringLiteral("name")).toString();
        const QJsonArray cols = rpcResult105(rpc(QStringLiteral("table.get_info"),
                                                 QJsonObject { { "table", tbl } }))
                                        .value(QStringLiteral("columns"))
                                        .toArray();
        for (const QJsonValue &cv : cols) {
            const QJsonObject c = cv.toObject();
            Col105 col;
            col.name = c.value(QStringLiteral("name")).toString();
            col.numeric = c.value(QStringLiteral("mode")).toString().compare(
                                  QStringLiteral("numeric"), Qt::CaseInsensitive)
                    == 0;
            const QJsonArray data = rpcResult105(rpc(QStringLiteral("table.get_column_data"),
                                                     QJsonObject { { "table", tbl },
                                                                   { "col", col.name } }))
                                            .value(QStringLiteral("data"))
                                            .toArray();
            for (const QJsonValue &v : data) {
                if (col.numeric)
                    col.nums << (v.isDouble() ? v.toDouble() : NaN);
                else
                    col.texts << (v.isNull() ? QString() : v.toString());
            }
            out << col;
        }
        return out;
    };
    auto guiImport = [&](const QString &path) {
        QVector<Col105> out;
        std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                readFile(path), QStringLiteral("Gui105"), path));
        if (!ft)
            return out;
        for (int c = 0; c < ft->columnCount(); ++c) {
            Column *src = ft->column(c);
            Col105 col;
            col.name = src->name();
            col.numeric = src->columnMode() == SciDAVis::ColumnMode::Numeric;
            for (int r = 0; r < src->rowCount(); ++r) {
                if (col.numeric)
                    col.nums << (src->isInvalid(r) ? NaN : src->valueAt(r));
                else
                    col.texts << src->textAt(r);
            }
            out << col;
        }
        return out;
    };
    auto dumpCols = [](const QVector<Col105> &cols) {
        QStringList s;
        for (const Col105 &c : cols) {
            QStringList v;
            if (c.numeric) {
                for (double d : c.nums)
                    v << (std::isnan(d) ? QStringLiteral("NaN") : QString::number(d));
            } else {
                v = c.texts;
            }
            s << QStringLiteral("%1[%2]{%3}")
                            .arg(c.name, c.numeric ? QStringLiteral("num") : QStringLiteral("txt"),
                                 v.join(QLatin1Char(',')));
        }
        return s.join(QStringLiteral(" ")).toStdString();
    };
    // trailing empty cells / NaN do not count (the RPC table may keep its default row count)
    auto sameColumn = [](const Col105 &a, const Col105 &b) {
        if (a.name != b.name || a.numeric != b.numeric)
            return false;
        if (a.numeric) {
            QVector<double> x = a.nums, y = b.nums;
            while (!x.isEmpty() && std::isnan(x.last()))
                x.removeLast();
            while (!y.isEmpty() && std::isnan(y.last()))
                y.removeLast();
            if (x.size() != y.size())
                return false;
            for (int i = 0; i < x.size(); ++i) {
                if (std::isnan(x[i]) != std::isnan(y[i]))
                    return false;
                if (!std::isnan(x[i]) && std::fabs(x[i] - y[i]) > 1e-12)
                    return false;
            }
            return true;
        }
        QStringList x = a.texts, y = b.texts;
        while (!x.isEmpty() && x.last().isEmpty())
            x.removeLast();
        while (!y.isEmpty() && y.last().isEmpty())
            y.removeLast();
        return x == y;
    };
    // GUI and RPC agree, and the column types are the expected ones ('n' numeric, 't' text)
    auto checkSame = [&](const char *tag, const QString &path, const char *types) -> bool {
        const QVector<Col105> gui = guiImport(path);
        const QVector<Col105> rpcCols = rpcImport(path);
        bool ok = !gui.isEmpty() && gui.size() == rpcCols.size()
                && gui.size() == int(std::strlen(types));
        for (int i = 0; ok && i < gui.size(); ++i)
            ok = sameColumn(gui[i], rpcCols[i]) && gui[i].numeric == (types[i] == 'n');
        if (!ok) {
            std::cerr << "[FAIL] 105" << tag << " expected types " << types
                      << "\n  GUI: " << dumpCols(gui) << "\n  RPC: " << dumpCols(rpcCols) << "\n";
        }
        return ok;
    };

    // every case runs, so that one report lists all the disagreements
    int failures105 = 0;
#define EXPECT105(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl; \
            ++failures105; \
        } \
    } while (0)

    // -------------------------------------------------------------------------
    // 105a: missing / Excel error tokens
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("a1", writeFile(QStringLiteral("r_tok00_tab.txt"),
                                         "x\ty\n1\t2\n2\tN.D.\n3\t4\n4\t5\n"),
                         "nn"),
               "105a1: 'N.D.' is a missing value on both paths");
    EXPECT105(checkSame("a2", writeFile(QStringLiteral("r_tok02_3col.csv"),
                                         "x,y,z\n1,2,3\n2,#N/A,4\n3,4,5\n4,5,6\n"),
                         "nnn"),
               "105a2: '#N/A' is a missing value on both paths");
    EXPECT105(checkSame("a3", writeFile(QStringLiteral("tokens.csv"),
                                         "x,a,b,c,d\n1,ND,1.5,#DIV/0!,2\n2,2,#VALUE!,7,n.d.\n"
                                         "3,3,2.5,8,\"N.D.\"\n4,4,3.5,#NUM!,5\n"),
                         "nnnnn"),
               "105a3: 'ND', '#DIV/0!', '#VALUE!', '#NUM!', 'n.d.', quoted 'N.D.' are missing");

    // -------------------------------------------------------------------------
    // 105b: one stray non-numeric cell in a numeric column (90 % rule)
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("b1", writeFile(QStringLiteral("stray.csv"),
                                         "x,y\n1,10\n2,11\n3,OVER\n4,13\n5,14\n6,15\n7,16\n8,17\n"
                                         "9,18\n10,19\n"),
                         "nn"),
               "105b1: one 'OVER' among nine numbers is an invalid cell of a numeric column");
    EXPECT105(checkSame("b2", writeFile(QStringLiteral("s_blocks6.dat"),
                                         "x y\n0 10\n1 11\n2 12\n3 13\n4 14\n5 15\n\nx y\n0 20\n"
                                         "1 21\n2 22\n3 23\n4 24\n5 25\n"),
                         "nn"),
               "105b2: a repeated 'x y' header line between two blocks is an invalid row");

    // -------------------------------------------------------------------------
    // 105c: columns of missing tokens only
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("c1", writeFile(QStringLiteral("missing_all.csv"),
                                         "x,y\n1,--\n2,--\n3,--\n"),
                         "nn"),
               "105c1: a column of '--' only is an empty numeric column");
    EXPECT105(checkSame("c2", writeFile(QStringLiteral("na_all.csv"),
                                         "x,y\n1,N/A\n2,N.D.\n3,#N/A\n"),
                         "nn"),
               "105c2: a column of missing tokens only is an empty numeric column");
    EXPECT105(checkSame("c3", writeFile(QStringLiteral("quoted_na.csv"),
                                         "x,y\n1,\"N/A\"\n2,\"N/A\"\n3,\"N/A\"\n"),
                         "nt"),
               "105c3: a column of quoted 'N/A' only stays text");

    // -------------------------------------------------------------------------
    // 105d: columns that stay text
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("d1", writeFile(QStringLiteral("labels.csv"),
                                         "Sample,Value\nA1,1\nB2,2\nC3,3\nD4,4\nE5,5\nF6,6\n"),
                         "tn"),
               "105d1: a label column stays text");
    EXPECT105(checkSame("d2", writeFile(QStringLiteral("mixed.csv"),
                                         "x,y\n1,10\n2,abc\n3,12\n4,def\n5,14\n6,15\n7,16\n8,17\n"
                                         "9,18\n10,19\n"),
                         "nt"),
               "105d2: two text cells among eight numbers keep the column text");
    EXPECT105(checkSame("d3", writeFile(QStringLiteral("few.csv"),
                                         "x,y\n1,10\n2,OVER\n3,12\n4,13\n"),
                         "nt"),
               "105d3: one text cell with fewer than five numbers keeps the column text");
    EXPECT105(checkSame("d4", writeFile(QStringLiteral("dates.csv"),
                                         "Date,Time,Value\n2026-01-01,12:00:00,1\n"
                                         "2026-01-02,12:30:00,2\n2026-01-03,13:00:00,3\n"
                                         "2026-01-04,13:30:00,4\n2026-01-05,14:00:00,5\n"
                                         "2026-01-06,14:30:00,6\n"),
                         "ttn"),
               "105d4: date and time columns keep the type of the GUI import");
    EXPECT105(checkSame("d5", writeFile(QStringLiteral("label_one_num.csv"),
                                         "Name,Value\nalpha,1\nbeta,2\n42,3\ngamma,4\ndelta,5\n"
                                         "eps,6\n"),
                         "tn"),
               "105d5: a label column with one number stays text");

#undef EXPECT105
    if (failures105 > 0) {
        std::cerr << "[FAIL] Suite 105: " << failures105 << " case(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 105: RPC import column types follow the GUI import\n";
    return 0;
}
