#include "ole_test_common.h"
#include "core/column/Column.h"
#include "core/datatypes/NumericDateTimeBaseFilter.h"
#include "lib/Interval.h"
#include "lib/XmlStreamReader.h"
#include "table/future_Table.h"
#include "PythonAutomationServer.h"

#include <QBuffer>
#include <QJsonObject>
#include <QXmlStreamWriter>

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Suite 85: worksheet / matrix core integrity.
//   85a Month/Day columns carry a NumericDateTimeBaseFilter (save -> load, type tab)
//   85b IntervalAttribute<T>::removeRows keeps every formula interval after a merge
//   85c Column::replaceDateTimes flags invalid rows at first + i (not at i)
//   85d normalizeColumns / normalizeSelection: max of the valid values, empty cells stay empty
//   85e NumericDateTimeBaseFilter unit Month counts whole years (2020-01-01 -> 2021-03-01 = 14)
//   85f Matrix::recalculate: x steps along the columns, y along the rows
//   85g Matrix::determinant: no ||A||-relative pivot tolerance (diag(1e10, 1e-5) -> 1e5)
//   85h Matrix transpose swaps the x and y coordinate ranges
//   85i determinant singularity: GUI (Matrix::determinant) and JSON-RPC matrix.determinant use
//       the same scale-independent pivot rule (diag(1e10, 1e-7) -> 1e3, 1..9 3x3 -> 0); a pivot
//       is only zeroed when it is lost in the rounding of its own cancellation, so triangular
//       matrices keep their true determinant ([[1,1e16],[0,1]] -> 1,
//       [[1,1000],[0,1e-13]] -> 1e-13)
// Every check is evaluated (failures are counted) so a single run reports all regressions.
// =============================================================================
namespace {

int g85Failures = 0;

#define CHECK85(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g85Failures;                                                                      \
        }                                                                                       \
    } while (0)

// Exposes the protected conversions of the numeric <-> date/time filter.
class Suite85NumericDateTimeProbe : public NumericDateTimeBaseFilter
{
public:
    Suite85NumericDateTimeProbe(UnitInterval unit, const QDateTime &base)
        : NumericDateTimeBaseFilter(unit, base)
    {
    }
    double toDouble(const QDateTime &t) const { return offsetToDouble(t); }
    QDateTime toDateTime(double v) const { return makeDateTime(v); }
};

QByteArray suite85SaveColumn(const Column *col)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    col->save(&writer);
    writer.writeEndDocument();
    buf.close();
    return bytes;
}

// Loads `xml` into a fresh column that starts as Numeric (as the project reader builds it).
std::unique_ptr<Column> suite85LoadColumn(const QByteArray &xml, bool *ok)
{
    std::unique_ptr<Column> col(new Column("s85_loaded", SciDAVis::ColumnMode::Numeric));
    XmlStreamReader reader(xml);
    while (!reader.atEnd() && !(reader.isStartElement() && reader.name() == "column"))
        reader.readNext();
    *ok = reader.isStartElement() && col->load(&reader);
    return col;
}

std::string suite85Str(const std::vector<double> &v)
{
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < v.size(); ++i)
        os << (i ? ", " : "") << v[i];
    os << "]";
    return os.str();
}

// Month/Day column: filter present, save -> load keeps mode and values.
void suite85CheckDateLikeColumn(SciDAVis::ColumnMode mode, const char *label)
{
    std::unique_ptr<Column> col(new Column(QString("s85_%1").arg(label),
                                           SciDAVis::ColumnMode::Numeric));
    for (int r = 0; r < 3; ++r)
        col->setValueAt(r, double(r + 1)); // 1..3 -> January..March / Monday..Wednesday
    col->setColumnMode(mode);
    CHECK85(col->columnMode() == mode, "85a: " << label << " column mode not set");
    CHECK85(col->dataType() == SciDAVis::TypeQDateTime,
            "85a: " << label << " column is not QDateTime");
    const bool hasFilter = col->numericDateTimeBaseFilter() != nullptr;
    CHECK85(hasFilter, "85a: numeric -> " << label
                                          << " column has no NumericDateTimeBaseFilter (null)");
    if (!hasFilter)
        return; // saving would dereference null and take the whole run down
    std::vector<QDateTime> before;
    for (int r = 0; r < col->rowCount(); ++r)
        before.push_back(col->dateTimeAt(r));

    const QByteArray xml = suite85SaveColumn(col.get());
    CHECK85(xml.contains("numericDateTimeFilter"),
            "85a: " << label << " save did not write numericDateTimeFilter");
    bool ok = false;
    std::unique_ptr<Column> loaded = suite85LoadColumn(xml, &ok);
    CHECK85(ok, "85a: " << label << " column did not load back");
    CHECK85(loaded->columnMode() == mode, "85a: " << label << " mode lost on load");
    CHECK85(loaded->numericDateTimeBaseFilter() != nullptr,
            "85a: loaded " << label << " column has no NumericDateTimeBaseFilter");
    CHECK85(loaded->rowCount() == int(before.size()),
            "85a: " << label << " row count " << loaded->rowCount() << " after load");
    for (int r = 0; r < int(before.size()) && r < loaded->rowCount(); ++r) {
        CHECK85(loaded->dateTimeAt(r) == before[size_t(r)],
                "85a: " << label << " row " << r << " changed on save -> load ("
                        << before[size_t(r)].toString(Qt::ISODate).toStdString() << " -> "
                        << loaded->dateTimeAt(r).toString(Qt::ISODate).toStdString() << ")");
    }
}

} // namespace

int runSuite85TableMatrixCoreIntegrity(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 85: worksheet / matrix core integrity...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 85");
    const QVariant prevNonInteractive = qApp->property("torigraph_noninteractive");
    qApp->setProperty("torigraph_noninteractive", true);
    g85Failures = 0;

    // ------------------------------------------------ 85a: Month / Day columns
    {
        suite85CheckDateLikeColumn(SciDAVis::ColumnMode::Month, "Month");
        suite85CheckDateLikeColumn(SciDAVis::ColumnMode::Day, "Day");

        // Type tab of the worksheet (TableView::setColumnForControlTabs) on a Month column.
        Table *t = mw->newTable("S85Month", 3, 2);
        CHECK_COND(t != nullptr, "85a: table");
        Column *c = t->column(1);
        for (int r = 0; r < 3; ++r)
            c->setValueAt(r, double(r + 1));
        c->setColumnMode(SciDAVis::ColumnMode::Month);
        if (c->numericDateTimeBaseFilter() != nullptr) {
            t->goToCell(0, 1);
            t->goToCell(1, 0);
            t->goToCell(2, 1);
            std::cout << "[INFO] 85a selecting cells of a Month column did not crash\n";
        } else {
            CHECK85(false, "85a: Month column in a worksheet has no NumericDateTimeBaseFilter");
        }
    }

    // ------------------------------------------------ 85b: formula intervals after removeRows
    {
        std::unique_ptr<Column> col(new Column("s85_formula", SciDAVis::ColumnMode::Numeric));
        for (int r = 0; r < 8; ++r)
            col->setValueAt(r, double(r));
        col->setFormula(Interval<int>(0, 1), "A");
        col->setFormula(Interval<int>(2, 3), "B");
        col->setFormula(Interval<int>(4, 5), "A");
        col->setFormula(Interval<int>(6, 7), "C");
        col->removeRows(2, 2); // -> A A A A C C : the two A runs merge, C must survive
        CHECK85(col->rowCount() == 6, "85b: row count " << col->rowCount() << " after removeRows");
        const QString expect[6] = { "A", "A", "A", "A", "C", "C" };
        QString got;
        for (int r = 0; r < 6; ++r) {
            got += (r ? "," : "") + col->formula(r);
            CHECK85(col->formula(r) == expect[r], "85b: formula of row " << r << " is '"
                                                                       << col->formula(r).toStdString()
                                                                       << "', expected '"
                                                                       << expect[r].toStdString() << "'");
        }
        std::cout << "[INFO] 85b formulas after removeRows(2,2): " << got.toStdString() << "\n";
        CHECK85(col->formulaIntervals().size() == 2,
                "85b: " << col->formulaIntervals().size() << " formula intervals, expected 2");
    }

    // ------------------------------------------------ 85c: replaceDateTimes validity
    {
        std::unique_ptr<Column> col(new Column("s85_dt", SciDAVis::ColumnMode::DateTime));
        const QDateTime d0(QDate(2024, 1, 1), QTime(0, 0));
        for (int r = 0; r < 4; ++r)
            col->setDateTimeAt(r, d0.addDays(r));
        QList<QDateTime> repl;
        repl << QDateTime() << d0.addDays(30);
        col->replaceDateTimes(2, repl);
        CHECK85(col->rowCount() == 4, "85c: row count " << col->rowCount());
        CHECK85(!col->isInvalid(0), "85c: row 0 became invalid (flag written at i, not first + i)");
        CHECK85(!col->isInvalid(1), "85c: row 1 became invalid");
        CHECK85(col->isInvalid(2), "85c: row 2 (invalid QDateTime) is not flagged invalid");
        CHECK85(!col->isInvalid(3), "85c: row 3 (valid QDateTime) is flagged invalid");
        CHECK85(col->dateTimeAt(3) == d0.addDays(30), "85c: row 3 value not replaced");
    }

    // ------------------------------------------------ 85d: normalize
    {
        Table *t = mw->newTable("S85Norm", 3, 2);
        CHECK_COND(t != nullptr && t->d_future_table != nullptr, "85d: table");
        Column *neg = t->column(0);
        Column *gap = t->column(1);
        neg->setValueAt(0, -5.0);
        neg->setValueAt(1, -3.0);
        neg->setValueAt(2, -1.0);
        gap->setValueAt(0, 1.0);
        gap->setValueAt(1, 0.0);
        gap->setValueAt(2, 4.0);
        gap->setInvalid(1, true);
        t->d_future_table->normalizeColumns(QList<Column *>() << neg << gap);
        std::vector<double> negGot, gapGot;
        for (int r = 0; r < 3; ++r) {
            negGot.push_back(neg->valueAt(r));
            gapGot.push_back(gap->valueAt(r));
        }
        std::cout << "[INFO] 85d normalizeColumns([-5,-3,-1]) = " << suite85Str(negGot)
                  << ", ([1,empty,4]) = " << suite85Str(gapGot) << "\n";
        const double negExp[3] = { 5.0, 3.0, 1.0 }; // divided by the max -1
        for (int r = 0; r < 3; ++r)
            CHECK85(std::fabs(neg->valueAt(r) - negExp[r]) < 1e-12 && !neg->isInvalid(r),
                    "85d: [-5,-3,-1] row " << r << " -> " << neg->valueAt(r) << ", expected "
                                           << negExp[r]);
        CHECK85(std::fabs(gap->valueAt(0) - 0.25) < 1e-12 && !gap->isInvalid(0),
                "85d: [1,empty,4] row 0 -> " << gap->valueAt(0) << ", expected 0.25");
        CHECK85(gap->isInvalid(1), "85d: [1,empty,4] the empty row became a valid value");
        CHECK85(std::fabs(gap->valueAt(2) - 1.0) < 1e-12 && !gap->isInvalid(2),
                "85d: [1,empty,4] row 2 -> " << gap->valueAt(2) << ", expected 1");

        // normalizeSelection: 5-row table, column 0 has values only in rows 0..1.
        Table *s = mw->newTable("S85NormSel", 5, 2);
        CHECK_COND(s != nullptr && s->d_future_table != nullptr, "85d: selection table");
        Column *a = s->column(0);
        Column *b = s->column(1);
        a->setValueAt(0, 2.0);
        a->setValueAt(1, 4.0);
        for (int r = 2; r < 5; ++r)
            a->setInvalid(r, true);
        b->setValueAt(0, -2.0);
        b->setValueAt(1, -8.0);
        for (int r = 2; r < 5; ++r)
            b->setInvalid(r, true);
        const int aRowsBefore = a->rowCount();
        s->deselectAll();
        s->setCellSelected(0, 0);
        s->setCellSelected(1, 0);
        s->d_future_table->normalizeSelection();
        CHECK85(std::fabs(a->valueAt(0) - 0.5) < 1e-12 && std::fabs(a->valueAt(1) - 1.0) < 1e-12,
                "85d: normalizeSelection([2,4]) -> [" << a->valueAt(0) << ", " << a->valueAt(1)
                                                      << "], expected [0.5, 1]");
        CHECK85(a->rowCount() == aRowsBefore,
                "85d: normalizeSelection changed the column length " << aRowsBefore << " -> "
                                                                     << a->rowCount());
        for (int r = 2; r < a->rowCount(); ++r)
            CHECK85(a->isInvalid(r), "85d: normalizeSelection turned empty row " << r
                                                                                << " into a value");
        // only negative values selected: max = -2 -> [1, 4] (was left unchanged with max = 0)
        s->deselectAll();
        s->setCellSelected(0, 1);
        s->setCellSelected(1, 1);
        s->d_future_table->normalizeSelection();
        CHECK85(std::fabs(b->valueAt(0) - 1.0) < 1e-12 && std::fabs(b->valueAt(1) - 4.0) < 1e-12,
                "85d: normalizeSelection([-2,-8]) -> [" << b->valueAt(0) << ", " << b->valueAt(1)
                                                        << "], expected [1, 4]");
        for (int r = 2; r < b->rowCount(); ++r)
            CHECK85(b->isInvalid(r), "85d: normalizeSelection (negative) turned empty row "
                                             << r << " into a value");
        s->deselectAll();
    }

    // ------------------------------------------------ 85e: unit Month across years
    {
        const QDateTime base(QDate(2020, 1, 1), QTime(0, 0));
        Suite85NumericDateTimeProbe f(NumericDateTimeBaseFilter::UnitInterval::Month, base);
        const double m14 = f.toDouble(QDateTime(QDate(2021, 3, 1), QTime(0, 0)));
        std::cout << "[INFO] 85e 2020-01-01 -> 2021-03-01 in months = " << m14 << "\n";
        CHECK85(std::fabs(m14 - 14.0) < 1e-9, "85e: 2020-01-01 -> 2021-03-01 gives " << m14
                                                                                   << " months, expected 14");
        const double m135 = f.toDouble(QDateTime(QDate(2021, 2, 15), QTime(0, 0)));
        CHECK85(std::fabs(m135 - 13.5) < 1e-9,
                "85e: 2020-01-01 -> 2021-02-15 gives " << m135 << " months, expected 13.5");
        const double mNeg = f.toDouble(QDateTime(QDate(2018, 11, 1), QTime(0, 0)));
        CHECK85(std::fabs(mNeg + 14.0) < 1e-9,
                "85e: 2020-01-01 -> 2018-11-01 gives " << mNeg << " months, expected -14");
        Suite85NumericDateTimeProbe g(NumericDateTimeBaseFilter::UnitInterval::Month,
                                      QDateTime(QDate(2020, 12, 1), QTime(0, 0)));
        const double m1 = g.toDouble(QDateTime(QDate(2021, 1, 1), QTime(0, 0)));
        CHECK85(std::fabs(m1 - 1.0) < 1e-9,
                "85e: 2020-12-01 -> 2021-01-01 gives " << m1 << " months, expected 1");
        // numeric -> date/time -> numeric round trip
        const double samples[] = { 0.0, 1.0, 13.5, 14.0, 25.25, -3.0, -14.0, 37.0 };
        for (double v : samples) {
            const QDateTime dt = f.toDateTime(v);
            const double back = f.toDouble(dt);
            CHECK85(std::fabs(back - v) < 1e-6,
                    "85e: round trip " << v << " -> " << dt.toString(Qt::ISODate).toStdString()
                                       << " -> " << back);
        }
        CHECK85(f.toDateTime(14.0) == QDateTime(QDate(2021, 3, 1), QTime(0, 0)),
                "85e: 14 months after 2020-01-01 is "
                        << f.toDateTime(14.0).toString(Qt::ISODate).toStdString());
    }

    // ------------------------------------------------ 85f: matrix formula x / y
    {
        Matrix *m = mw->newMatrix("S85MatXY", 10, 3);
        CHECK_COND(m != nullptr, "85f: matrix");
        m->setCoordinates(0.0, 2.0, 0.0, 9.0); // x over 3 columns, y over 10 rows: steps 1, 1
        m->setFormula("x");
        m->selectAll();
        CHECK85(m->recalculate(), "85f: recalculate(x) failed");
        double maxErrX = 0.0;
        for (int r = 0; r < 10; ++r)
            for (int c = 0; c < 3; ++c)
                maxErrX = std::max(maxErrX, std::fabs(m->cell(r, c) - double(c)));
        std::cout << "[INFO] 85f x in row 0 = [" << m->cell(0, 0) << ", " << m->cell(0, 1) << ", "
                  << m->cell(0, 2) << "]\n";
        CHECK85(maxErrX < 1e-9, "85f: formula x on 10x3 must equal the column index (x 0..2), max err "
                                        << maxErrX << ", cell(0,2) = " << m->cell(0, 2));
        m->setFormula("y");
        m->selectAll();
        CHECK85(m->recalculate(), "85f: recalculate(y) failed");
        double maxErrY = 0.0;
        for (int r = 0; r < 10; ++r)
            for (int c = 0; c < 3; ++c)
                maxErrY = std::max(maxErrY, std::fabs(m->cell(r, c) - double(r)));
        CHECK85(maxErrY < 1e-9, "85f: formula y on 10x3 must equal the row index (y 0..9), max err "
                                        << maxErrY << ", cell(9,0) = " << m->cell(9, 0));
        // reversed x range keeps its sign (as the column header does)
        m->setCoordinates(2.0, 0.0, 0.0, 9.0);
        m->setFormula("x");
        m->selectAll();
        CHECK85(m->recalculate(), "85f: recalculate(x, reversed) failed");
        CHECK85(std::fabs(m->cell(0, 0) - 2.0) < 1e-9 && std::fabs(m->cell(0, 2) - 0.0) < 1e-9,
                "85f: reversed x range: row 0 = [" << m->cell(0, 0) << ", " << m->cell(0, 1) << ", "
                                                   << m->cell(0, 2) << "], expected [2, 1, 0]");
    }

    // ------------------------------------------------ 85g: determinant without relative tolerance
    {
        Matrix *m = mw->newMatrix("S85Det", 2, 2);
        CHECK_COND(m != nullptr, "85g: matrix");
        m->setCell(0, 0, 1e10);
        m->setCell(0, 1, 0.0);
        m->setCell(1, 0, 0.0);
        m->setCell(1, 1, 1e-5);
        const double det = m->determinant();
        std::cout << "[INFO] 85g det(diag(1e10, 1e-5)) = " << det << "\n";
        CHECK85(std::fabs(det - 1e5) <= 1e5 * 1e-12,
                "85g: det(diag(1e10, 1e-5)) = " << det << ", expected 1e5");
        // an exactly singular matrix still gives 0
        m->setCell(0, 0, 1.0);
        m->setCell(0, 1, 2.0);
        m->setCell(1, 0, 2.0);
        m->setCell(1, 1, 4.0);
        const double det0 = m->determinant();
        CHECK85(det0 == 0.0, "85g: det([[1,2],[2,4]]) = " << det0 << ", expected 0");
    }

    // ------------------------------------------------ 85i: GUI and SDK determinant agree
    {
        PythonAutomationServer *srv = mw->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "85i: pythonAutomationServer null");
        struct DetCase
        {
            const char *label;
            int n;
            std::vector<double> rowMajor;
            double expected;
        };
        const std::vector<DetCase> cases = {
            { "diag(1e10, 1e-5)", 2, { 1e10, 0.0, 0.0, 1e-5 }, 1e5 },
            { "diag(1e10, 1e-7)", 2, { 1e10, 0.0, 0.0, 1e-7 }, 1e3 },
            { "[[1,2,3],[4,5,6],[7,8,9]]", 3, { 1, 2, 3, 4, 5, 6, 7, 8, 9 }, 0.0 },
            { "[[1,2],[2,4]]", 2, { 1, 2, 2, 4 }, 0.0 },
            // no cancellation happens in a triangular matrix: nothing may be zeroed
            { "[[1,1e16],[0,1]]", 2, { 1.0, 1e16, 0.0, 1.0 }, 1.0 },
            { "[[1e20,1e20],[0,1]]", 2, { 1e20, 1e20, 0.0, 1.0 }, 1e20 },
            { "[[1,1000],[0,1e-13]]", 2, { 1.0, 1000.0, 0.0, 1e-13 }, 1e-13 },
            // nearly singular but exact in binary: 1 + 2^-30 - 1 = 2^-30 is not noise
            { "[[1,1],[1,1+2^-30]]", 2, { 1.0, 1.0, 1.0, 1.0 + std::ldexp(1.0, -30) },
              std::ldexp(1.0, -30) },
        };
        int rpcId = 8500;
        for (const DetCase &c : cases) {
            Matrix *m = mw->newMatrix(QStringLiteral("S85DetRule%1").arg(rpcId - 8499), c.n, c.n);
            CHECK_COND(m != nullptr, "85i: matrix");
            for (int r = 0; r < c.n; ++r)
                for (int col = 0; col < c.n; ++col)
                    m->setCell(r, col, c.rowMajor[size_t(r) * size_t(c.n) + size_t(col)]);
            const double gui = m->determinant();

            QJsonObject params;
            params[QStringLiteral("matrix")] = m->name();
            QJsonObject req;
            req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            req[QStringLiteral("id")] = ++rpcId;
            req[QStringLiteral("method")] = QStringLiteral("matrix.determinant");
            req[QStringLiteral("params")] = params;
            const QJsonObject resp = srv->handleRpcRequest(req);
            QCoreApplication::processEvents();
            const QJsonValue dv =
                    resp.value(QStringLiteral("result")).toObject().value(QStringLiteral("determinant"));
            const double sdk = dv.isDouble() ? dv.toDouble() : std::numeric_limits<double>::quiet_NaN();

            std::cout << "[INFO] 85i det(" << c.label << "): GUI = " << gui << ", SDK = " << sdk
                      << ", expected " << c.expected << "\n";
            auto matchesExpected = [&c](double v) {
                return c.expected == 0.0 ? v == 0.0
                                         : std::fabs(v - c.expected) <= std::fabs(c.expected) * 1e-12;
            };
            CHECK85(matchesExpected(gui), "85i: GUI det(" << c.label << ") = " << gui << ", expected "
                                               << c.expected);
            CHECK85(matchesExpected(sdk), "85i: SDK det(" << c.label << ") = " << sdk << ", expected "
                                               << c.expected);
            CHECK85(gui == sdk, "85i: GUI and SDK disagree on det(" << c.label << "): " << gui
                                                                  << " vs " << sdk);
        }
    }

    // ------------------------------------------------ 85h: transpose swaps coordinates
    {
        Matrix *m = mw->newMatrix("S85Transpose", 3, 5);
        CHECK_COND(m != nullptr, "85h: matrix");
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 5; ++c)
                m->setCell(r, c, 10.0 * r + c);
        m->setCoordinates(0.0, 4.0, 10.0, 12.0); // x over the 5 columns, y over the 3 rows
        m->transpose();
        CHECK85(m->numRows() == 5 && m->numCols() == 3,
                "85h: transposed size " << m->numRows() << "x" << m->numCols() << ", expected 5x3");
        std::cout << "[INFO] 85h after transpose x = [" << m->xStart() << ", " << m->xEnd()
                  << "], y = [" << m->yStart() << ", " << m->yEnd() << "]\n";
        CHECK85(m->xStart() == 10.0 && m->xEnd() == 12.0,
                "85h: x range after transpose [" << m->xStart() << ", " << m->xEnd()
                                                 << "], expected [10, 12]");
        CHECK85(m->yStart() == 0.0 && m->yEnd() == 4.0,
                "85h: y range after transpose [" << m->yStart() << ", " << m->yEnd()
                                                 << "], expected [0, 4]");
        bool dataOk = true;
        for (int r = 0; r < 5 && r < m->numRows(); ++r)
            for (int c = 0; c < 3 && c < m->numCols(); ++c)
                dataOk = dataOk && m->cell(r, c) == 10.0 * c + r;
        CHECK85(dataOk, "85h: transposed cells are wrong");
        m->transpose(); // back (the same command body as undo)
        CHECK85(m->numRows() == 3 && m->numCols() == 5, "85h: second transpose size");
        CHECK85(m->xStart() == 0.0 && m->xEnd() == 4.0 && m->yStart() == 10.0 && m->yEnd() == 12.0,
                "85h: second transpose did not restore the coordinates: x = [" << m->xStart()
                        << ", " << m->xEnd() << "], y = [" << m->yStart() << ", " << m->yEnd() << "]");
    }

    qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
    if (g85Failures > 0) {
        std::cerr << "[FAIL] Suite 85: " << g85Failures << " check(s) failed.\n";
        return 1;
    }
    std::cout << "[PASS] Suite 85 passed.\n";
    return 0;
}
