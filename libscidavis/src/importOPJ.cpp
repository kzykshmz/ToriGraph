/***************************************************************************
    File                 : importOPJ.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2010 Miquel Garriga (gbmiquel*gmail.com)
    Copyright            : (C) 2010 Knut Franke (knut.franke*gmx.de)
    Copyright            : (C) 2006-2007 by Ion Vasilief (ion_vasilief*yahoo.fr)
    Copyright            : (C) 2006-2007 by Alex Kargovsky (kargovsky*yumr.phys.msu.su)
    Copyright            : (C) 2006-2007 by Tilman Benkert (thzs*gmx.net)
    Description          : Origin project import class

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
#include "importOPJ.h"
#include <QPointer>

#include "future/core/datatypes/Double2StringFilter.h"
#include "future/core/datatypes/DateTime2StringFilter.h"

#include <QRegExp>
#include <QApplication>
#include <QMessageBox>
#include <QDockWidget>
#include <QLocale>
#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTemporaryFile>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include "Matrix.h"
#include "ColorButton.h"
#include "MultiLayer.h"
#include "Note.h"
#include "Folder.h"
#include "QwtBarCurve.h"
#include "QwtHistogram.h"
#include "Grid.h"

#define OBJECTXOFFSET 200

namespace {
constexpr int kMaxMatrixDim = 100000;
constexpr qint64 kMaxMatrixCells = 25000000LL; // 25M cells (~200 MB max)
constexpr int kMaxSpreadsheetCols = 100000;
constexpr int kMaxSpreadsheetRows = 25000000;
constexpr qint64 kMaxSpreadsheetCells = 25000000LL;
} // namespace

bool ImportOPJ::setCodec(const QString &codecName)
{
    if (codecName.trimmed().isEmpty())
        return false;
    auto codec = QTextCodec::codecForName(codecName.trimmed().toUtf8());
    if (nullptr != codec) {
        d_codec = codec;
        return true;
    }
    return false;
}

QString ImportOPJ::decodeMbcs(char const *const input) const
{
    if (!input || !*input)
        return QString();
    if (nullptr != d_codec)
        return d_codec->toUnicode(input);

    const int len = static_cast<int>(std::strlen(input));
    bool pureAscii = true;
    for (int i = 0; i < len; ++i) {
        if (static_cast<unsigned char>(input[i]) >= 0x80) {
            pureAscii = false;
            break;
        }
    }
    if (pureAscii)
        return QString::fromLatin1(input, len);

    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        QTextCodec::ConverterState state;
        const QString s = utf8->toUnicode(input, len, &state);
        if (state.invalidChars == 0)
            return s;
    }
    if (QTextCodec *sjis = QTextCodec::codecForName("Shift-JIS")) {
        QTextCodec::ConverterState sjisState;
        const QString s = sjis->toUnicode(input, len, &sjisState);
        if (sjisState.invalidChars == 0)
            return s;
    }
    return QString::fromLocal8Bit(input, len);
}

QString strreverse(const QString &str) // QString reversing
{
    QString out = str;
    std::reverse(out.begin(), out.end());
    return out;
}

QString posixTimeToString(time_t pt)
{
    QDateTime qdt;
    qdt.setTime_t(pt);
    return qdt.toString("dd.MM.yyyy hh.mm.ss");
}

ImportOPJ::ImportOPJ(ApplicationWindow *app, const QString &filename, const QString &codec)
    : parse_error(-1), xoffset(0), mw(app), d_codec(nullptr)
{
    setCodec(codec);
    try {
        if (mw)
            mw->setStatusBarText(QObject::tr("Import start ..."));
        QByteArray localPath = filename.toLocal8Bit();
        std::unique_ptr<QTemporaryFile> unicodeStaging;
        if (QString::fromLocal8Bit(localPath) != filename) {
            QFile srcFile(filename);
            if (srcFile.open(QIODevice::ReadOnly)) {
                const QString suffix = QFileInfo(filename).suffix();
                unicodeStaging.reset(new QTemporaryFile(
                        QDir::tempPath() + QLatin1String("/torigraph_opj_XXXXXX.")
                        + (suffix.isEmpty() ? QStringLiteral("opj") : suffix)));
                if (unicodeStaging->open()) {
                    char copyBuf[16384];
                    qint64 nRead = 0;
                    while ((nRead = srcFile.read(copyBuf, sizeof(copyBuf))) > 0)
                        unicodeStaging->write(copyBuf, nRead);
                    unicodeStaging->flush();
                    unicodeStaging->close();
                    localPath = unicodeStaging->fileName().toLocal8Bit();
                } else {
                    unicodeStaging.reset();
                }
            }
        }
        OriginFile opj(localPath.constData());
        const bool parsedOk = opj.parse();
        if (!parsedOk) {
            parse_error = -1;
            if (mw)
                mw->setStatusBarText(QString());
            QApplication::restoreOverrideCursor();
            if (!ApplicationWindow::isNonInteractiveContext(mw)) {
                QMessageBox::critical(mw, QObject::tr("Origin Project Import Error"),
                                      QObject::tr("Failed to parse Origin project file."));
            }
            return;
        }
        parse_error = 0;
        if (mw) {
            mw->setStatusBarText(
                    QObject::tr("... file parsed. Starting conversion to SciDAVis ..."));
            importTables(opj);
            importGraphs(opj);
            importNotes(opj);
            mw->setStatusBarText(QString());
            if (filename.endsWith(".opj", Qt::CaseInsensitive))
                createProjectTree(opj);
            mw->showResults(opj.resultsLogString().c_str(), mw->logWindow.isVisible());
        }
    } catch (const std::logic_error &er) {
        parse_error = -2;
        if (mw)
            mw->setStatusBarText(QString());
        QApplication::restoreOverrideCursor();
        if (!ApplicationWindow::isNonInteractiveContext(mw)) {
            QMessageBox::critical(mw, QObject::tr("Origin Project Import Error"),
                                  QObject::tr(er.what()));
        }
    } catch (const std::exception &er) {
        parse_error = -2;
        if (mw)
            mw->setStatusBarText(QString());
        QApplication::restoreOverrideCursor();
        if (!ApplicationWindow::isNonInteractiveContext(mw)) {
            QMessageBox::critical(mw, QObject::tr("Origin Project Import Error"),
                                  QString::fromUtf8(er.what()));
        }
    } catch (...) {
        parse_error = -2;
        if (mw)
            mw->setStatusBarText(QString());
        QApplication::restoreOverrideCursor();
    }
}

inline uint qHash(const tree<Origin::ProjectNode>::iterator &key)
{
    return qHash(key->name.c_str());
}

bool ImportOPJ::createProjectTree(const OriginFile &opj)
{
    if (!mw)
        return false;
    const tree<Origin::ProjectNode> *projectTree = opj.project();
    if (!projectTree || projectTree->empty() || !projectTree->begin().node)
        return false;
    tree<Origin::ProjectNode>::iterator root = projectTree->begin(projectTree->begin());
    if (!root.node)
        return false;
    Folder *baseFolder = mw->currentFolder() ? mw->currentFolder() : mw->projectFolder();
    if (!baseFolder)
        return false;
    if (baseFolder == mw->projectFolder()) {
        if (auto *item = dynamic_cast<FolderListItem *>(mw->folders.topLevelItem(0))) {
            const QString rootName = decodeMbcs(root->name.c_str());
            item->setText(0, rootName);
            if (item->folder())
                item->folder()->setName(rootName);
        }
    }
    QHash<tree<Origin::ProjectNode>::iterator, Folder *> parent;
    parent[root] = baseFolder;
    {
        QSignalBlocker sbFolders(&mw->folders);
        for (tree<Origin::ProjectNode>::iterator sib = projectTree->begin(root);
             sib != projectTree->end(root); ++sib) {
            if (!sib.node)
                continue;
            if (sib->type == Origin::ProjectNode::Folder) {
                Folder *pFolder = parent.value(projectTree->parent(sib));
                if (!pFolder)
                    pFolder = baseFolder;
                Folder &f = pFolder->addChild<Folder>(decodeMbcs(sib->name.c_str()));
                parent[sib] = &f;
                f.setBirthDate(posixTimeToString(sib->creationDate));
                f.setModificationDate(posixTimeToString(sib->modificationDate));
                if (FolderListItem *parentItem = pFolder->folderListItem()) {
                    FolderListItem *fi = new FolderListItem(parentItem, &f);
                    fi->setFlags(fi->flags() | Qt::ItemIsEditable);
                    f.setFolderListItem(fi);
                }
            } else {
                QString name = decodeMbcs(sib->name.c_str());
                if (sib->type == Origin::ProjectNode::Note) {
                    QRegExp rx("^@\\((\\S+)\\)$");
                    if (rx.indexIn(name) == 0)
                        name = rx.cap(1);
                }
                const char *nodetype;
                switch (sib->type) {
                case Origin::ProjectNode::SpreadSheet:
                    nodetype = "Table";
                    break;
                case Origin::ProjectNode::Matrix:
                    nodetype = "Matrix";
                    break;
                case Origin::ProjectNode::Graph:
                    nodetype = "MultiLayer";
                    break;
                case Origin::ProjectNode::Graph3D:
                    // there is no Graph3D type yet
                    nodetype = "MultiLayer"; // "Graph3D";
                    break;
                case Origin::ProjectNode::Note:
                    nodetype = "Note";
                    break;
                case Origin::ProjectNode::Excel:
                    // there is no Excel type yet
                    nodetype = "Table";
                    break;
                default:
                    nodetype = "Unknown";
                    break;
                }
                MyWidget *w = baseFolder->window(name, nodetype);
                while (w) { // Origin window names are unique, but we need to loop on all sheets of
                            // Excel windows
                    Folder *f = parent.value(projectTree->parent(sib));
                    if (!f || f == baseFolder)
                        break; // skip windows that go to base folder or unmapped parent
                    // removeWindow uses QList.removeAll, so remove w before adding it to its folder
                    baseFolder->removeWindow(w);
                    f->addWindow(w);
                    f->setActiveWindow(w);
                    w = baseFolder->window(name, nodetype);
                }
            }
        }
    }
    if (!baseFolder->folders().isEmpty())
        mw->folders.setVisible(true);
    mw->changeFolder(baseFolder, true);
    return true;
}

int ImportOPJ::translateOrigin2ScidavisLineStyle(int linestyle)
{
    int scidavisstyle = 0;
    switch (linestyle) {
    case Origin::GraphCurve::Solid:
        scidavisstyle = 0;
        break;
    case Origin::GraphCurve::Dash:
    case Origin::GraphCurve::ShortDash:
        scidavisstyle = 1;
        break;
    case Origin::GraphCurve::Dot:
    case Origin::GraphCurve::ShortDot:
        scidavisstyle = 2;
        break;
    case Origin::GraphCurve::DashDot:
    case Origin::GraphCurve::ShortDashDot:
        scidavisstyle = 3;
        break;
    case Origin::GraphCurve::DashDotDot:
        scidavisstyle = 4;
        break;
    }
    return scidavisstyle;
}

// spreadsheets can be either in its own window or as a sheet in excels windows
bool ImportOPJ::importSpreadsheet(const OriginFile &opj, const Origin::SpreadSheet &spread)
{
    QLocale locale = mw->locale();
    int SciDAVis_scaling_factor = 10; // in Origin width is measured in characters while in SciDAVis
                                      // - pixels --- need to be accurate
    const qint64 columnCount64 = static_cast<qint64>(spread.columns.size());
    const qint64 maxrows64 = static_cast<qint64>(spread.maxRows);
    if (columnCount64 <= 0 || columnCount64 > kMaxSpreadsheetCols || maxrows64 < 0
        || maxrows64 > kMaxSpreadsheetRows
        || (maxrows64 > 0 && (columnCount64 * maxrows64) > kMaxSpreadsheetCells))
        return false; // remove tables without cols or with out-of-bounds dimensions
    int columnCount = static_cast<int>(columnCount64);
    int maxrows = static_cast<int>(maxrows64);

    Table *table = mw->newTable(decodeMbcs(spread.name.c_str()), maxrows, columnCount);
    if (!table)
        return false;
    // Every cell is written with its own command; with automatic formula recalculation on,
    // each one re-evaluated the dependent / cross-table formulas (test/Histo.opj took minutes).
    // Recalculate once when the sheet is filled instead.
    struct AutoRecalcPause
    {
        QPointer<Table> t;
        bool prev;
        explicit AutoRecalcPause(Table *tbl) : t(tbl), prev(tbl->autoRecalcFormulas())
        {
            if (prev)
                tbl->setAutoRecalcFormulas(false);
        }
        ~AutoRecalcPause()
        {
            if (prev && t)
                t->setAutoRecalcFormulas(true);
        }
    } autoRecalcPause(table);
    if (spread.hidden || spread.loose)
        mw->hideWindow(table);

    table->setWindowLabel(decodeMbcs(spread.label.c_str()));
    for (int j = 0; j < columnCount; ++j) {
        Origin::SpreadColumn column = spread.columns[j];
        Column *scidavis_column = table->column(j);

        QString name(decodeMbcs(column.name.c_str()));
        scidavis_column->setName(name.replace(QRegExp(".*_"), ""));
        if (column.command.size() > 0)
            scidavis_column->setFormula(Interval<int>(0, maxrows),
                                        QString(decodeMbcs(column.command.c_str())));
        scidavis_column->setComment(QString(decodeMbcs(column.comment.c_str())));
        table->setColumnWidth(j, (int)column.width * SciDAVis_scaling_factor);

        switch (column.type) {
        case Origin::SpreadColumn::X:
            table->setColPlotDesignation(j, SciDAVis::X);
            break;
        case Origin::SpreadColumn::Y:
            table->setColPlotDesignation(j, SciDAVis::Y);
            break;
        case Origin::SpreadColumn::Z:
            table->setColPlotDesignation(j, SciDAVis::Z);
            break;
        case Origin::SpreadColumn::XErr:
            table->setColPlotDesignation(j, SciDAVis::xErr);
            break;
        case Origin::SpreadColumn::YErr:
            table->setColPlotDesignation(j, SciDAVis::yErr);
            break;
        case Origin::SpreadColumn::Label:
        default:
            table->setColPlotDesignation(j, SciDAVis::noDesignation);
        }

        QString format;
        switch (column.valueType) {
        case Origin::Numeric:
        case Origin::TextNumeric:
            /*
              A TextNumeric column in Origin is a column whose filled cells contain either a double
              or a string. In SciDAVis there is no equivalent column type. Set the SciDAVis column
              type as 'Numeric' or 'Text' depending on the type of first element in column.
              TODO: Add a "per column" flag, settable at import dialog, to choose between both
              types.
            */
            {
                const int nRows = std::min((int)column.data.size(), maxrows);
                bool setAsText = false;
                for (int i = 0; i < nRows; ++i) {
                    if (column.data[i].type() != Origin::variant::V_DOUBLE) {
                        setAsText = true;
                        break;
                    }
                }
                table->column(j)->setColumnMode(setAsText ? SciDAVis::ColumnMode::Text
                                                          : SciDAVis::ColumnMode::Numeric);
                for (int i = 0; i < nRows; ++i) {
                    const Origin::variant &value = column.data[i];
                    if (value.type() == Origin::variant::V_DOUBLE) {
                        const double datavalue = value.as_double();
                        if (datavalue == _ONAN || !std::isfinite(datavalue))
                            continue; // mark for empty cell
                        if (!setAsText) {
                            scidavis_column->setValueAt(i, datavalue);
                        } else { // convert double to string for Text columns
                            scidavis_column->setTextAt(i, locale.toString(datavalue, 'g', 16));
                        }
                    } else if (value.type() == Origin::variant::V_STRING) {
                        scidavis_column->setTextAt(i, decodeMbcs(value.as_string()));
                    }
                }
                if (!setAsText && column.numericDisplayType != 0) {
                    char f = 'g';
                    switch (column.valueTypeSpecification) {
                    case 0: // Decimal 1000
                        f = 'f';
                        break;
                    case 1: // Scientific
                        f = 'e';
                        break;
                    case 2: // Engineering
                    case 3: // Decimal 1,000
                    default:
                        f = 'g';
                        break;
                    }
                    if (auto *filter = dynamic_cast<Double2StringFilter *>(
                                table->column(j)->outputFilter())) {
                        filter->setNumericFormat(f);
                        if (column.decimalPlaces >= 0)
                            filter->setNumDigits(column.decimalPlaces);
                    }
                }
                break;
            }
        case Origin::Text:
            table->column(j)->setColumnMode(SciDAVis::ColumnMode::Text);
            for (int i = 0; i < std::min((int)column.data.size(), maxrows); ++i) {
                const Origin::variant &value = column.data[i];
                if (value.type() == Origin::variant::V_STRING) {
                    scidavis_column->setTextAt(i, decodeMbcs(value.as_string()));
                } else if (value.type() == Origin::variant::V_DOUBLE) {
                    const double v = value.as_double();
                    if (v != _ONAN && std::isfinite(v))
                        scidavis_column->setTextAt(i, locale.toString(v, 'g', 16));
                }
            }
            break;
        case Origin::Date: {
            switch (column.valueTypeSpecification) {
            case -128:
                format = "dd/MM/yyyy";
                break;
            case -119:
                format = "dd/MM/yyyy HH:mm";
                break;
            case -118:
                format = "dd/MM/yyyy HH:mm:ss";
                break;
            case 0:
            case 9:
            case 10:
                format = "dd.MM.yyyy";
                break;
            case 2:
                format = "MMM d";
                break;
            case 3:
                format = "M/d";
                break;
            case 4:
                format = "d";
                break;
            case 5:
            case 6:
                format = "ddd";
                break;
            case 7:
                format = "yyyy";
                break;
            case 8:
                format = "yy";
                break;
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
                format = "yyMMdd";
                break;
            case 16:
            case 17:
                format = "MMM";
                break;
            case 19:
                format = "M-d-yyyy";
                break;
            default:
                format = "dd.MM.yyyy";
            }
            for (int i = 0; i < std::min((int)column.data.size(), maxrows); ++i) {
                if (column.data[i].type() == Origin::variant::V_DOUBLE) {
                    const double v = column.data[i].as_double();
                    if (v != _ONAN && std::isfinite(v))
                        scidavis_column->setValueAt(i, v);
                }
            }
            table->column(j)->setColumnMode(SciDAVis::ColumnMode::DateTime);
            DateTime2StringFilter *filter =
                    static_cast<DateTime2StringFilter *>(scidavis_column->outputFilter());
            filter->setFormat(format);
            break;
        }
        case Origin::Time: {
            switch (column.valueTypeSpecification + 128) {
            case 0:
                format = "hh:mm";
                break;
            case 1:
                format = "hh";
                break;
            case 2:
                format = "hh:mm:ss";
                break;
            case 3:
                format = "hh:mm:ss.zzz";
                break;
            case 4:
                format = "hh ap";
                break;
            case 5:
                format = "hh:mm ap";
                break;
            case 6:
                format = "mm:ss";
                break;
            case 7:
                format = "mm:ss.zzz";
                break;
            case 8:
                format = "hhmm";
                break;
            case 9:
                format = "hhmmss";
                break;
            case 10:
                format = "hh:mm:ss.zzz";
                break;
            }
            for (int i = 0; i < std::min((int)column.data.size(), maxrows); ++i) {
                if (column.data[i].type() == Origin::variant::V_DOUBLE) {
                    const double v = column.data[i].as_double();
                    if (v != _ONAN && std::isfinite(v))
                        scidavis_column->setValueAt(i, v);
                }
            }
            table->column(j)->setColumnMode(SciDAVis::ColumnMode::DateTime);
            DateTime2StringFilter *filter =
                    static_cast<DateTime2StringFilter *>(table->column(j)->outputFilter());
            filter->setFormat(format);
            break;
        }
        case Origin::Month: {
            switch (column.valueTypeSpecification) {
            case 0:
                format = "MMM";
                break;
            case 1:
                format = "MMMM";
                break;
            case 2:
                format = "M";
                break;
            }
            for (int i = 0; i < std::min((int)column.data.size(), maxrows); ++i) {
                if (column.data[i].type() == Origin::variant::V_DOUBLE) {
                    const double v = column.data[i].as_double();
                    if (v != _ONAN && std::isfinite(v))
                        scidavis_column->setValueAt(i, v);
                }
            }
            table->column(j)->setColumnMode(SciDAVis::ColumnMode::Month);
            DateTime2StringFilter *filter =
                    static_cast<DateTime2StringFilter *>(table->column(j)->outputFilter());
            filter->setFormat(format);
            break;
        }
        case Origin::Day: {
            switch (column.valueTypeSpecification) {
            case 0:
                format = "ddd";
                break;
            case 1:
                format = "dddd";
                break;
            case 2:
                format = "d";
                break;
            }
            for (int i = 0; i < std::min((int)column.data.size(), maxrows); ++i) {
                if (column.data[i].type() == Origin::variant::V_DOUBLE) {
                    const double v = column.data[i].as_double();
                    if (v != _ONAN && std::isfinite(v))
                        scidavis_column->setValueAt(i, v);
                }
            }
            table->column(j)->setColumnMode(SciDAVis::ColumnMode::Day);
            DateTime2StringFilter *filter =
                    static_cast<DateTime2StringFilter *>(table->column(j)->outputFilter());
            filter->setFormat(format);
            break;
        }
        default:
            break;
        }
    }

    if (!(spread.hidden || spread.loose) || opj.version() != 7.5) {
        table->showNormal();

        // cascade the tables
        if (opj.version() >= 6.0) {
            Origin::Rect windowRect;
            windowRect = spread.frameRect;
            table->move(QPoint(windowRect.left, windowRect.top));
        } else {
            int dx = table->verticalHeaderWidth();
            int dy = table->frameGeometry().height() - table->height();
            table->move(QPoint(d_table_visible_count * dx + xoffset * OBJECTXOFFSET,
                               d_table_visible_count * dy));
            d_table_visible_count++;
        }
    }
    return true;
}
bool ImportOPJ::importTables(const OriginFile &opj)
{
    int matrix_visible_count = 0;
    int SciDAVis_scaling_factor = 10; // in Origin width is measured in characters while in SciDAVis
                                      // - pixels --- need to be accurate
    for (unsigned int s = 0; s < opj.spreadCount(); ++s) {
        mw->setStatusBarText(QObject::tr("Spreadsheet %1 / %2").arg(s + 1).arg(opj.spreadCount()));
        Origin::SpreadSheet spread = opj.spread(s);
        int columnCount = spread.columns.size();
        if (!columnCount) // remove tables without cols
            continue;
        importSpreadsheet(opj, spread);
    }
    // Import excels
    for (unsigned int s = 0; s < opj.excelCount(); ++s) {
        Origin::Excel excelwb = opj.excel(s);
        for (unsigned int j = 0; j < excelwb.sheets.size(); ++j) {
            mw->setStatusBarText(QObject::tr("Excel %1 / %2, sheet %3 / %4")
                                         .arg(s + 1)
                                         .arg(opj.excelCount())
                                         .arg(j + 1)
                                         .arg(excelwb.sheets.size()));
            Origin::SpreadSheet spread = excelwb.sheets[j];
            int columnCount = spread.columns.size();
            if (!columnCount) // remove tables without cols
                continue;
            spread.name = excelwb.name;
            // scidavis does not have windows with multiple sheets
            if (j > 0) {
                spread.name.append("@").append(std::to_string(j + 1));
            }
            spread.maxRows = excelwb.maxRows;
            importSpreadsheet(opj, spread);
        }
    }

    // Import matrices
    for (unsigned int s = 0; s < opj.matrixCount(); ++s) {
        Origin::Matrix matrix = opj.matrix(s);
        unsigned int layers = matrix.sheets.size();
        for (unsigned int l = 0; l < layers; ++l) {
            Origin::MatrixSheet &layer = matrix.sheets[l];
            const qint64 columnCount64 = static_cast<qint64>(layer.columnCount);
            const qint64 rowCount64 = static_cast<qint64>(layer.rowCount);
            if (rowCount64 <= 0 || columnCount64 <= 0 || rowCount64 > kMaxMatrixDim
                || columnCount64 > kMaxMatrixDim
                || (rowCount64 * columnCount64) > kMaxMatrixCells) {
                continue;
            }
            const int columnCount = static_cast<int>(columnCount64);
            const int rowCount = static_cast<int>(rowCount64);
            const qint64 totalCells = rowCount64 * columnCount64;
            mw->setStatusBarText(QObject::tr("Matrix %1 / %2, sheet %3 / %4")
                                         .arg(s + 1)
                                         .arg(opj.matrixCount())
                                         .arg(l + 1)
                                         .arg(layers));
            QString matName = decodeMbcs(matrix.name.c_str());
            if (l > 0)
                matName += QStringLiteral("@%1").arg(l + 1);
            Matrix *Matrix = mw->newMatrix(matName, rowCount, columnCount);
            if (!Matrix)
                return false;
            Matrix->setWindowLabel(decodeMbcs(matrix.label.c_str()));
            Matrix->setFormula(decodeMbcs(layer.command.c_str()));
            Matrix->setColumnsWidth(layer.width * SciDAVis_scaling_factor);
// TODO
#if 0
			Matrix->table()->blockSignals(true);
#endif
            QVector<qreal> values(static_cast<int>(totalCells), 0.0);
            const size_t srcSize = layer.data.size();
            for (int r = 0; r < rowCount; ++r) {
                const size_t rowBase = static_cast<size_t>(r) * static_cast<size_t>(columnCount);
                for (int c = 0; c < columnCount; ++c) {
                    const size_t srcIdx = rowBase + static_cast<size_t>(c);
                    if (srcIdx < srcSize) {
                        const double v = layer.data[srcIdx];
                        const qint64 dstIdx = static_cast<qint64>(c) * rowCount64
                                + static_cast<qint64>(r);
                        values[static_cast<int>(dstIdx)] = (std::isfinite(v) && v != _ONAN) ? v : 0.0;
                    }
                }
            }
            Matrix->setCells(values);
            if (layer.coordinates.size() >= 4) {
                // liborigin stores MatrixSheet::coordinates as [Y2, X2, Y1, X1]
                Matrix->setCoordinates(layer.coordinates[3], layer.coordinates[1],
                                       layer.coordinates[2], layer.coordinates[0]);
            }

            Matrix->saveCellsToMemory();

            QChar format = 'g';
            int prec = 6;
            switch (layer.valueTypeSpecification) {
            case 0: // Decimal 1000
                format = 'f';
                prec = layer.decimalPlaces;
                break;
            case 1: // Scientific
                format = 'e';
                prec = layer.decimalPlaces;
                break;
            case 2: // Engineering
            case 3: // Decimal 1,000
                format = 'g';
                prec = layer.significantDigits;
                break;
            default:
                format = 'g';
                prec = 6;
                break;
            }
            Matrix->setNumericFormat(format, prec);
            Matrix->forgetSavedCells();
// TODO
#if 0
        Matrix->table()->blockSignals(false);
#endif
            Matrix->showNormal();

            // cascade the matrices
#if 0
			int dx=Matrix->verticalHeaderWidth();
			int dy=Matrix->frameGeometry().height() - matrix->height();
#endif
            // TODO
            int dx = 100;
            int dy = 100;
            Matrix->move(QPoint(matrix_visible_count * dx + xoffset * OBJECTXOFFSET,
                                matrix_visible_count * dy));
            matrix_visible_count++;
        }

        if (matrix_visible_count > 0)
            xoffset++;
    }
    return true;
}

bool ImportOPJ::importNotes(const OriginFile &opj)
{
    int visible_count = 0;
    for (unsigned int n = 0; n < opj.noteCount(); ++n) {
        Origin::Note _note = opj.note(n);
        QString name = decodeMbcs(_note.name.c_str());
        QRegExp rx("^@(\\S+)$");
        if (rx.indexIn(name) == 0) {
            name = name.mid(2, name.length() - 3);
        }
        Note *note = mw->newNote(name);
        if (!note)
            return false;
        note->setWindowLabel(decodeMbcs(_note.label.c_str()));
        note->setText(QString(decodeMbcs(_note.text.c_str())));

        // cascade the notes
        int dx = 20;
        int dy = note->frameGeometry().height() - note->height();
        note->move(QPoint(visible_count * dx + xoffset * OBJECTXOFFSET, visible_count * dy));
        visible_count++;
    }
    if (visible_count > 0)
        xoffset++;
    return true;
}

namespace {

Table *resolveImportedOriginTable(ApplicationWindow *mw, const QString &rawName)
{
    if (!mw || rawName.trimmed().isEmpty())
        return nullptr;
    if (Table *t = mw->table(rawName))
        return t;
    const QString hyphenName = QString(rawName).replace(QLatin1Char('_'), QLatin1Char('-'));
    if (Table *t = mw->table(hyphenName))
        return t;
    for (MyWidget *w : mw->windowsList()) {
        if (w && w->inherits("Table")) {
            auto *t = static_cast<Table *>(w);
            if (t->name().compare(rawName, Qt::CaseInsensitive) == 0
                || t->name().compare(hyphenName, Qt::CaseInsensitive) == 0) {
                return t;
            }
        }
    }
    return nullptr;
}

} // namespace

bool ImportOPJ::importGraphs(const OriginFile &opj)
{
    double pi = 3.141592653589793;
    int visible_count = 0;
    int tickTypeMap[] = { 0, 3, 1, 2 };
    for (unsigned int g = 0; g < opj.graphCount(); ++g) {
        Origin::Graph _graph = opj.graph(g);
        MultiLayer *ml = mw->multilayerPlot(decodeMbcs(_graph.name.c_str())); //, 0);
        if (!ml)
            return false;

        ml->hide(); //!hack used in order to avoid resize and repaint events
        ml->setWindowLabel(decodeMbcs(_graph.label.c_str()));
        unsigned int layers = _graph.layers.size();
        for (unsigned int l = 0; l < layers; ++l) {
            mw->setStatusBarText(QObject::tr("Graph %1 / %2, layer %3 / %4")
                                         .arg(g + 1)
                                         .arg(opj.graphCount())
                                         .arg(l + 1)
                                         .arg(layers));
            Origin::GraphLayer &layer = _graph.layers[l];
            Graph *graph = ml->addLayer();
            if (!graph)
                return false;
            int auto_color = 0;
            int style = 0;
            QVector<int> originToScidavisCurveIdx(static_cast<int>(layer.curves.size()), -1);
            for (unsigned int c = 0; c < layer.curves.size(); ++c) {
                Origin::GraphCurve &_curve = layer.curves[c];
                QString data(decodeMbcs(_curve.dataName.c_str()));
                if (data.size() < 2)
                    continue;
                int color = 0;
                switch (_curve.type) {
                case Origin::GraphCurve::Line:
                case Origin::GraphCurve::LineSeries:
                    style = Graph::Line;
                    break;
                case Origin::GraphCurve::Scatter:
                case Origin::GraphCurve::PlotDot:
                    style = Graph::Scatter;
                    break;
                case Origin::GraphCurve::LineSymbol:
                    style = Graph::LineSymbols;
                    break;
                case Origin::GraphCurve::Area:
                case Origin::GraphCurve::AreaStack:
                    style = Graph::Area;
                    break;
                case Origin::GraphCurve::ErrorBar:
                case Origin::GraphCurve::XErrorBar:
                case Origin::GraphCurve::YErrorBar:
                case Origin::GraphCurve::XYErrorBar:
                    style = Graph::ErrorBars;
                    break;
                case Origin::GraphCurve::Column:
                case Origin::GraphCurve::ColumnStack:
                case Origin::GraphCurve::ColumnFloat:
                    style = Graph::VerticalBars;
                    break;
                case Origin::GraphCurve::Bar:
                case Origin::GraphCurve::BarStack:
                    style = Graph::HorizontalBars;
                    break;
                case Origin::GraphCurve::Histogram:
                    style = Graph::Histogram;
                    break;
                case Origin::GraphCurve::Box:
                    style = Graph::Box;
                    break;
                case Origin::GraphCurve::Pie:
                    style = Graph::Pie;
                    break;
                default:
                    continue;
                }
                const int prevCurves = graph->curves();
                QString tableName;
                switch (data[0].toLatin1()) {
                case 'T':
                case 'E': {
                    tableName = data.mid(2);
                    Table *table = resolveImportedOriginTable(mw, tableName);
                    if (!table)
                        continue;
                    const QString actualTblName = table->name();
                    QString xShort = decodeMbcs(_curve.xColumnName.c_str());
                    xShort.replace(QRegExp(".*_"), "");
                    QString yShort = decodeMbcs(_curve.yColumnName.c_str());
                    yShort.replace(QRegExp(".*_"), "");
                    if (style == Graph::ErrorBars) {
                        int flags = _curve.symbolShape;
                        graph->addErrorBars(
                                QString("%1_%2").arg(actualTblName, xShort), table,
                                QString("%1_%2").arg(actualTblName, yShort),
                                ((flags & 0x10) == 0x10 ? 0 : 1), ceil(_curve.lineWidth),
                                ceil(_curve.symbolSize), QColor(Qt::black), (flags & 0x40) == 0x40,
                                (flags & 2) == 2, (flags & 1) == 1);
                    } else if (style == Graph::Histogram || style == Graph::Box
                               || style == Graph::Pie) {
                        graph->insertCurvesList(
                                table,
                                QStringList() << QString("%1_%2").arg(actualTblName, yShort),
                                style,
                                std::max(1, static_cast<int>(std::ceil(_curve.lineWidth))),
                                std::max(3, static_cast<int>(std::ceil(_curve.symbolSize))));
                        if (style == Graph::Histogram && graph->curves() > prevCurves
                            && std::isfinite(layer.histogramBin) && layer.histogramBin > 0.0
                            && std::isfinite(layer.histogramBegin)
                            && std::isfinite(layer.histogramEnd)
                            && layer.histogramEnd > layer.histogramBegin) {
                            if (auto *hist = dynamic_cast<QwtHistogram *>(
                                        graph->curve(graph->curves() - 1))) {
                                hist->setBinning(false, layer.histogramBin, layer.histogramBegin,
                                                 layer.histogramEnd);
                                hist->loadData();
                            }
                        }
                    } else {
                        graph->insertCurve(table, QString("%1_%2").arg(actualTblName, xShort),
                                           QString("%1_%2").arg(actualTblName, yShort), style);
                    }
                    break;
                }
                case 'F': {
                    Origin::Function function;
                    QStringList formulas;
                    QList<double> ranges;
                    int s = opj.functionIndex(data.mid(2).toStdString().c_str());
                    if (s < 0 || static_cast<unsigned int>(s) >= opj.functionCount())
                        continue;
                    function = opj.function(s);

                    int type;
                    if (function.type == Origin::Function::Polar) {
                        type = 2;
                        formulas << decodeMbcs(function.formula.c_str()) << "x";
                        ranges << pi / 180 * function.begin << pi / 180 * function.end;
                    } else {
                        type = 0;
                        formulas << decodeMbcs(function.formula.c_str());
                        ranges << function.begin << function.end;
                    }
                    graph->addFunctionCurve(mw, type, formulas, "x", ranges, function.totalPoints,
                                            decodeMbcs(function.name.c_str()));

                    mw->updateFunctionLists(type, formulas);
                    break;
                }
                default:
                    continue;
                }

                if (graph->curves() <= prevCurves)
                    continue;
                const int curveIdx = graph->curves() - 1;
                originToScidavisCurveIdx[static_cast<int>(c)] = curveIdx;
                if (style == Graph::ErrorBars)
                    continue;

                CurveLayout cl = graph->initCurveLayout(style, layer.curves.size());
                cl.sSize = ceil(_curve.symbolSize * 0.5);
                cl.penWidth = _curve.symbolThickness;
                color = _curve.symbolColor.regular;
                if ((style == Graph::Scatter || style == Graph::LineSymbols)
                    && color == 0xF7) // 0xF7 -Automatic color
                    color = auto_color++;
                cl.symCol = color;
                switch (_curve.symbolShape) {
                case 0: // NoSymbol
                    cl.sType = 0;
                    break;
                case 1: // Rect
                    cl.sType = 2;
                    break;
                case 2: // Ellipse
                case 20: // Sphere
                    cl.sType = 1;
                    break;
                case 3: // UTriangle
                    cl.sType = 6;
                    break;
                case 4: // DTriangle
                    cl.sType = 5;
                    break;
                case 5: // Diamond
                    cl.sType = 3;
                    break;
                case 6: // Cross +
                    cl.sType = 9;
                    break;
                case 7: // Cross x
                    cl.sType = 10;
                    break;
                case 8: // Snow
                    cl.sType = 13;
                    break;
                case 9: // Horizontal -
                    cl.sType = 11;
                    break;
                case 10: // Vertical |
                    cl.sType = 12;
                    break;
                case 15: // LTriangle
                    cl.sType = 7;
                    break;
                case 16: // RTriangle
                    cl.sType = 8;
                    break;
                case 17: // Hexagon
                case 19: // Pentagon
                    cl.sType = 15;
                    break;
                case 18: // Star
                    cl.sType = 14;
                    break;
                default:
                    cl.sType = 0;
                }

                switch (_curve.symbolInterior) {
                case 0:
                    cl.fillCol = color;
                    break;
                case 1:
                case 2:
                case 8:
                case 9:
                case 10:
                case 11:
                    color = _curve.symbolFillColor.regular;
                    if ((style == Graph::Scatter || style == Graph::LineSymbols)
                        && color == 0xF7) // 0xF7 -Automatic color
                        color = 17; // depend on Origin settings - not stored in file
                    cl.fillCol = color;
                    break;
                default:
                    cl.fillCol = -1;
                }

                cl.lWidth = _curve.lineWidth;
                color = _curve.lineColor.regular % ColorButton::colors_count;
                cl.lCol = (_curve.lineColor.type == Origin::Color::Automatic
                                   ? 0
                                   : color); // 0xF7 -Automatic color
                int linestyle = _curve.lineStyle;
                cl.filledArea = (_curve.fillArea || style == Graph::VerticalBars
                                 || style == Graph::HorizontalBars || style == Graph::Histogram
                                 || style == Graph::Pie || style == Graph::Box)
                        ? 1
                        : 0;
                if (cl.filledArea) {
                    switch (_curve.fillAreaPattern) {
                    case 0:
                        cl.aStyle = 0;
                        break;
                    case 1:
                    case 2:
                    case 3:
                        cl.aStyle = 4;
                        break;
                    case 4:
                    case 5:
                    case 6:
                        cl.aStyle = 5;
                        break;
                    case 7:
                    case 8:
                    case 9:
                        cl.aStyle = 6;
                        break;
                    case 10:
                    case 11:
                    case 12:
                        cl.aStyle = 1;
                        break;
                    case 13:
                    case 14:
                    case 15:
                        cl.aStyle = 2;
                        break;
                    case 16:
                    case 17:
                    case 18:
                        cl.aStyle = 3;
                        break;
                    }
                    Origin::Color color;
                    color = (cl.aStyle == 0 ? _curve.fillAreaColor : _curve.fillAreaPatternColor);
                    cl.aCol = (color.type == Origin::Color::Automatic
                                       ? 0
                                       : color.regular); // 0xF7 -Automatic color
                    if (style == Graph::VerticalBars || style == Graph::HorizontalBars
                        || style == Graph::Histogram) {
                        color = _curve.fillAreaPatternBorderColor;
                        cl.lCol = (color.type == Origin::Color::Automatic
                                           ? 0
                                           : color.regular); // 0xF7 -Automatic color
                        color = (cl.aStyle == 0 ? _curve.fillAreaColor
                                                : _curve.fillAreaPatternColor);
                        cl.aCol = (color.type == Origin::Color::Automatic
                                           ? cl.lCol
                                           : color.regular); // 0xF7 -Automatic color
                        cl.lWidth = _curve.fillAreaPatternBorderWidth;
                        linestyle = _curve.fillAreaPatternBorderStyle;
                    }
                }
                switch (linestyle) {
                case Origin::GraphCurve::Solid:
                    cl.lStyle = 0;
                    break;
                case Origin::GraphCurve::Dash:
                case Origin::GraphCurve::ShortDash:
                    cl.lStyle = 1;
                    break;
                case Origin::GraphCurve::Dot:
                case Origin::GraphCurve::ShortDot:
                    cl.lStyle = 2;
                    break;
                case Origin::GraphCurve::DashDot:
                case Origin::GraphCurve::ShortDashDot:
                    cl.lStyle = 3;
                    break;
                case Origin::GraphCurve::DashDotDot:
                    cl.lStyle = 4;
                    break;
                }

                graph->updateCurveLayout(curveIdx, &cl);
                if (style == Graph::VerticalBars || style == Graph::HorizontalBars) {
                    if (auto *b = dynamic_cast<QwtBarCurve *>(graph->curve(curveIdx)))
                        b->setGap(qRound(100 - _curve.symbolSize * 10));
                } else if (style == Graph::Histogram) {
                    if (auto *h = dynamic_cast<QwtHistogram *>(graph->curve(curveIdx))) {
                        h->setBinning(false, layer.histogramBin, layer.histogramBegin,
                                      layer.histogramEnd);
                        h->loadData();
                    }
                }
                switch (_curve.lineConnect) {
                case Origin::GraphCurve::NoLine:
                    graph->setCurveStyle(curveIdx, QwtPlotCurve::NoCurve);
                    break;
                case Origin::GraphCurve::Straight:
                    graph->setCurveStyle(curveIdx, QwtPlotCurve::Lines);
                    break;
                case Origin::GraphCurve::BSpline:
                case Origin::GraphCurve::Bezier:
                case Origin::GraphCurve::Spline:
                    graph->setCurveStyle(curveIdx, 5);
                    break;
                case Origin::GraphCurve::StepHorizontal:
                case Origin::GraphCurve::StepHCenter:
                    graph->setCurveStyle(curveIdx, QwtPlotCurve::Steps);
                    break;
                case Origin::GraphCurve::StepVertical:
                case Origin::GraphCurve::StepVCenter:
                    graph->setCurveStyle(curveIdx, 6);
                    break;
                }
            }
            if (!layer.legend.text.empty()) {
                const QString resolvedLeg = resolveOriginLegendPlaceholders(
                        parseOriginText(decodeMbcs(layer.legend.text.c_str())), layer,
                        originToScidavisCurveIdx);
                if (!resolvedLeg.trimmed().isEmpty())
                    graph->newLegend(resolvedLeg);
            }
            // add texts
            for (unsigned int i = 0; i < layer.texts.size(); ++i) {
                const QString resolvedTxt = resolveOriginLegendPlaceholders(
                        parseOriginText(decodeMbcs(layer.texts[i].text.c_str())), layer,
                        originToScidavisCurveIdx);
                if (!resolvedTxt.trimmed().isEmpty())
                    graph->newLegend(resolvedTxt);
            }
            if (style == Graph::HorizontalBars) {
                graph->setScale(0, layer.xAxis.min, layer.xAxis.max, layer.xAxis.step,
                                layer.xAxis.majorTicks, layer.xAxis.minorTicks, layer.xAxis.scale);
                graph->setScale(2, layer.yAxis.min, layer.yAxis.max, layer.yAxis.step,
                                layer.yAxis.majorTicks, layer.yAxis.minorTicks, layer.yAxis.scale);
            } else {
                graph->setScale(2, layer.xAxis.min, layer.xAxis.max, layer.xAxis.step,
                                layer.xAxis.majorTicks, layer.xAxis.minorTicks, layer.xAxis.scale);
                graph->setScale(0, layer.yAxis.min, layer.yAxis.max, layer.yAxis.step,
                                layer.yAxis.majorTicks, layer.yAxis.minorTicks, layer.yAxis.scale);
            }

            // grid
            Grid *grid = graph->grid();

            grid->enableX(!layer.xAxis.majorGrid.hidden);
            grid->enableXMin(!layer.xAxis.minorGrid.hidden);
            grid->enableY(!layer.yAxis.majorGrid.hidden);
            grid->enableYMin(!layer.yAxis.minorGrid.hidden);

            grid->setMajPenX(
                    QPen(ColorButton::color(layer.xAxis.majorGrid.color),
                         ceil(layer.xAxis.majorGrid.width),
                         Graph::getPenStyle(translateOrigin2ScidavisLineStyle(
                                 (Origin::GraphCurve::LineStyle)layer.xAxis.majorGrid.style))));
            grid->setMinPenX(
                    QPen(ColorButton::color(layer.xAxis.minorGrid.color),
                         ceil(layer.xAxis.minorGrid.width),
                         Graph::getPenStyle(translateOrigin2ScidavisLineStyle(
                                 (Origin::GraphCurve::LineStyle)layer.xAxis.minorGrid.style))));
            grid->setMajPenY(
                    QPen(ColorButton::color(layer.yAxis.majorGrid.color),
                         ceil(layer.yAxis.majorGrid.width),
                         Graph::getPenStyle(translateOrigin2ScidavisLineStyle(
                                 (Origin::GraphCurve::LineStyle)layer.yAxis.majorGrid.style))));
            grid->setMinPenY(
                    QPen(ColorButton::color(layer.yAxis.minorGrid.color),
                         ceil(layer.yAxis.minorGrid.width),
                         Graph::getPenStyle(translateOrigin2ScidavisLineStyle(
                                 (Origin::GraphCurve::LineStyle)layer.yAxis.minorGrid.style))));

            grid->setAxis(2, 0);
            grid->enableZeroLineX(0);
            grid->enableZeroLineY(0);

            std::vector<Origin::GraphAxisFormat> formats;
            formats.push_back(layer.yAxis.formatAxis[0]); // left
            formats.push_back(layer.yAxis.formatAxis[1]); // right
            formats.push_back(layer.xAxis.formatAxis[0]); // bottom
            formats.push_back(layer.xAxis.formatAxis[1]); // top
            graph->setXAxisTitle(resolveOriginLegendPlaceholders(
                    parseOriginText(decodeMbcs(formats[2].label.text.c_str())), layer,
                    originToScidavisCurveIdx));
            graph->setYAxisTitle(resolveOriginLegendPlaceholders(
                    parseOriginText(decodeMbcs(formats[0].label.text.c_str())), layer,
                    originToScidavisCurveIdx));

            std::vector<Origin::GraphAxisTick> ticks;
            ticks.push_back(layer.yAxis.tickAxis[0]); // left
            ticks.push_back(layer.yAxis.tickAxis[1]); // right
            ticks.push_back(layer.xAxis.tickAxis[0]); // bottom
            ticks.push_back(layer.xAxis.tickAxis[1]); // top
            for (int i = 0; i < 4; ++i) {
                const QString data(decodeMbcs(ticks[i].dataName.c_str()));
                const QString rawTblName = (data.size() > 2) ? data.mid(2) : QString();
                Table *axisTable = resolveImportedOriginTable(mw, rawTblName);
                QString colNameDecoded = decodeMbcs(ticks[i].columnName.c_str());
                colNameDecoded.replace(QRegExp(".*_"), "");
                const QString colSpec = axisTable
                        ? (axisTable->name() + QStringLiteral("_") + colNameDecoded)
                        : colNameDecoded;

                int format = 0;
                Graph::AxisType type;
                int prec = ticks[i].decimalPlaces;
                switch (ticks[i].valueType) {
                case Origin::Numeric:
                    type = Graph::AxisType::Numeric;
                    if (prec < 0) {
                        format = 0; // Automatic ('g') when decimalPlaces == -1
                        prec = 6;
                    } else {
                        switch (ticks[i].valueTypeSpecification) {
                        case 0: // Decimal 1000
                            format = 1;
                            break;
                        case 1: // Scientific
                            format = 2;
                            break;
                        case 2: // Engeneering
                        case 3: // Decimal 1,000
                            format = 0;
                            break;
                        }
                    }
                    break;
                case Origin::Text: // Text
                    type = Graph::AxisType::Txt;
                    break;
                case 2: // Date
                    type = Graph::AxisType::Date;
                    break;
                case 3: // Time
                    type = Graph::AxisType::Time;
                    break;
                case Origin::Month: // Month
                    type = Graph::AxisType::Month;
                    format = ticks[i].valueTypeSpecification;
                    break;
                case Origin::Day: // Day
                    type = Graph::AxisType::Day;
                    format = ticks[i].valueTypeSpecification;
                    break;
                case Origin::ColumnHeading:
                    type = Graph::AxisType::ColHeader;
                    if (prec < 0) {
                        format = 0;
                        prec = 6;
                    } else {
                        switch (ticks[i].valueTypeSpecification) {
                        case 0: // Decimal 1000
                            format = 1;
                            break;
                        case 1: // Scientific
                            format = 2;
                            break;
                        case 2: // Engeneering
                        case 3: // Decimal 1,000
                            format = 0;
                            break;
                        }
                    }
                    break;
                default:
                    type = Graph::AxisType::Numeric;
                    format = 0;
                    prec = 6;
                }

                const int majTickIdx = std::clamp(static_cast<int>(formats[i].majorTicksType), 0, 3);
                const int minTickIdx = std::clamp(static_cast<int>(formats[i].minorTicksType), 0, 3);
                graph->showAxis(i, type, colSpec, axisTable, !(formats[i].hidden),
                                tickTypeMap[majTickIdx], tickTypeMap[minTickIdx],
                                ticks[i].showMajorLabels,
                                ColorButton::color(formats[i].color), format, prec,
                                ticks[i].rotation, 0, "",
                                (ticks[i].color == 0xF7 ? ColorButton::color(formats[i].color)
                                                        : ColorButton::color(ticks[i].color)));

                if (type == Graph::AxisType::Txt && axisTable && !(formats[i].hidden)) {
                    Column *txtCol = axisTable->column(colNameDecoded);
                    if (txtCol) {
                        QStringList catLabels;
                        const int nRows = txtCol->rowCount();
                        for (int r = 0; r < nRows; ++r) {
                            if (txtCol->isInvalid(r))
                                catLabels << QString();
                            else if (txtCol->columnMode() == SciDAVis::ColumnMode::Text)
                                catLabels << txtCol->textAt(r).trimmed();
                            else if (txtCol->asStringColumn())
                                catLabels << txtCol->asStringColumn()->textAt(r).trimmed();
                            else
                                catLabels << QString::number(txtCol->valueAt(r), 'g', 6);
                        }
                        while (!catLabels.isEmpty() && catLabels.last().isEmpty())
                            catLabels.removeLast();
                        if (!catLabels.isEmpty()) {
                            const int qwtAxis = (i == 0) ? QwtPlot::yLeft
                                    : (i == 1)           ? QwtPlot::yRight
                                    : (i == 2)           ? QwtPlot::xBottom
                                                         : QwtPlot::xTop;
                            graph->setCategoryLabels(qwtAxis, catLabels);
                            if (ticks[i].rotation != 0)
                                graph->setAxisLabelRotation(qwtAxis, ticks[i].rotation);
                        }
                    }
                }
            }

            graph->setAutoscaleFonts(mw->autoScaleFonts); // restore user defined fonts behaviour
            graph->setIgnoreResizeEvents(!mw->autoResizeLayers);
        }
        // cascade the graphs
        if (!_graph.hidden) {
            int dx = 20;
            int dy = ml->frameGeometry().height() - ml->height();
            ml->move(QPoint(visible_count * dx + xoffset * OBJECTXOFFSET, visible_count * dy));
            visible_count++;
            ml->show();
            ml->arrangeLayers(true, true);
        } else {
            ml->show();
            ml->arrangeLayers(true, true);
            mw->hideWindow(ml);
        }
    }
    if (visible_count > 0)
        xoffset++;
    return true;
}

QString ImportOPJ::parseOriginText(const QString &str)
{
    QStringList lines = str.split("\n");
    QString text = "";
    for (int i = 0; i < lines.size(); ++i) {
        if (i > 0)
            text.append("\n");
        text.append(parseOriginTags(lines[i]));
    }
    return text;
}

QString ImportOPJ::parseOriginTags(const QString &str)
{
    QString line = str;
    // replace \l(...) tags
    QRegExp rxline("\\\\\\s*l\\s*\\(\\s*\\d+\\s*\\)");
    int pos = rxline.indexIn(line);
    while (pos > -1) {
        QString value = rxline.cap(0);
        int len = value.length();
        value.replace(QRegExp(" "), "");
        value = "\\c{" + value.mid(3, value.length() - 4) + "}";
        line.replace(pos, len, value);
        pos = rxline.indexIn(line);
    }
    // Lookbehind conditions are not supported - so need to reverse string
    QRegExp rx("\\)[^\\)\\(]*\\((?!\\s*[buig\\+\\-]\\s*\\\\)");
    QRegExp rxfont("\\)[^\\)\\(]*\\((?![^\\:]*\\:f\\s*\\\\)");
    QString linerev = strreverse(line);
    QString lBracket = strreverse("&lbracket;");
    QString rBracket = strreverse("&rbracket;");
    QString ltagBracket = strreverse("&ltagbracket;");
    QString rtagBracket = strreverse("&rtagbracket;");
    int pos1 = rx.indexIn(linerev);
    int pos2 = rxfont.indexIn(linerev);

    while (pos1 > -1 || pos2 > -1) {
        if (pos1 == pos2) {
            QString value = rx.cap(0);
            int len = value.length();
            value = rBracket + value.mid(1, len - 2) + lBracket;
            linerev.replace(pos1, len, value);
        } else if ((pos1 > pos2 && pos2 != -1) || pos1 == -1) {
            QString value = rxfont.cap(0);
            int len = value.length();
            value = rtagBracket + value.mid(1, len - 2) + ltagBracket;
            linerev.replace(pos2, len, value);
        } else if ((pos2 > pos1 && pos1 != -1) || pos2 == -1) {
            QString value = rx.cap(0);
            int len = value.length();
            value = rtagBracket + value.mid(1, len - 2) + ltagBracket;
            linerev.replace(pos1, len, value);
        }

        pos1 = rx.indexIn(linerev);
        pos2 = rxfont.indexIn(linerev);
    }
    linerev.replace(ltagBracket, "(");
    linerev.replace(rtagBracket, ")");

    line = strreverse(linerev);

    // replace \b(...), \i(...), \u(...), \g(...), \+(...), \-(...), \f:font(...) tags
    QString rxstr[] = { "\\\\\\s*b\\s*\\(",      "\\\\\\s*i\\s*\\(",   "\\\\\\s*u\\s*\\(",
                        "\\\\\\s*g\\s*\\(",      "\\\\\\s*\\+\\s*\\(", "\\\\\\s*\\-\\s*\\(",
                        "\\\\\\s*f\\:[^\\(]*\\(" };
    int postag[] = { 0, 0, 0, 0, 0, 0, 0 };
    QString ltag[] = {
        "<b>", "<i>", "<u>", "<font face=Symbol>", "<sup>", "<sub>", "<font face=%1>"
    };
    QString rtag[] = { "</b>", "</i>", "</u>", "</font>", "</sup>", "</sub>", "</font>" };
    QRegExp rxtags[7];
    for (int i = 0; i < 7; ++i)
        rxtags[i].setPattern(rxstr[i] + "[^\\(\\)]*\\)");

    bool flag = true;
    while (flag) {
        for (int i = 0; i < 7; ++i) {
            postag[i] = rxtags[i].indexIn(line);
            while (postag[i] > -1) {
                QString value = rxtags[i].cap(0);
                int len = value.length();
                int pos2 = value.indexOf("(");
                if (i < 6)
                    value = ltag[i] + value.mid(pos2 + 1, len - pos2 - 2) + rtag[i];
                else {
                    int posfont = value.indexOf("f:");
                    value = ltag[i].arg(value.mid(posfont + 2, pos2 - posfont - 2))
                            + value.mid(pos2 + 1, len - pos2 - 2) + rtag[i];
                }
                line.replace(postag[i], len, value);
                postag[i] = rxtags[i].indexIn(line);
            }
        }
        flag = false;
        for (int i = 0; i < 7; ++i) {
            if (rxtags[i].indexIn(line) > -1) {
                flag = true;
                break;
            }
        }
    }

    // replace unclosed tags
    for (int i = 0; i < 6; ++i)
        line.replace(QRegExp(rxstr[i]), ltag[i]);
    rxfont.setPattern(rxstr[6]);
    pos = rxfont.indexIn(line);
    while (pos > -1) {
        QString value = rxfont.cap(0);
        int len = value.length();
        int posfont = value.indexOf("f:");
        value = ltag[6].arg(value.mid(posfont + 2, len - posfont - 3));
        line.replace(pos, len, value);
        pos = rxfont.indexIn(line);
    }

    line.replace("&lbracket;", "(");
    line.replace("&rbracket;", ")");

    return line;
}

QString ImportOPJ::resolveOriginLegendPlaceholders(const QString &str,
                                                   const Origin::GraphLayer &layer,
                                                   const QVector<int> &curveIdxMap) const
{
    if (str.isEmpty())
        return str;

    QString out = str;

    // Remap \c{k} (1-based Origin curve index) if earlier curves in layer.curves were skipped
    static const QRegularExpression kCurveSymRe(QStringLiteral("\\\\c\\{(\\d+)\\}"));
    QRegularExpressionMatchIterator symIt = kCurveSymRe.globalMatch(out);
    QVector<QPair<QPair<int, int>, QString>> symEdits;
    while (symIt.hasNext()) {
        const QRegularExpressionMatch m = symIt.next();
        bool ok = false;
        const int origIdx = m.captured(1).toInt(&ok) - 1;
        if (ok && origIdx >= 0 && origIdx < curveIdxMap.size() && curveIdxMap[origIdx] >= 0) {
            const int mappedOneBased = curveIdxMap[origIdx] + 1;
            if (mappedOneBased != origIdx + 1) {
                symEdits.append({ qMakePair(m.capturedStart(0), m.capturedLength(0)),
                                  QStringLiteral("\\c{%1}").arg(mappedOneBased) });
            }
        }
    }
    for (int i = symEdits.size() - 1; i >= 0; --i)
        out.replace(symEdits[i].first.first, symEdits[i].first.second, symEdits[i].second);

    // Resolve %(k) and %(k, @...) Origin curve metadata placeholders
    static const QRegularExpression kPctCurveRe(
            QStringLiteral("%\\(\\s*(\\d+)(?:\\s*,\\s*([^\\)]*))?\\)"));
    QRegularExpressionMatchIterator it = kPctCurveRe.globalMatch(out);
    QVector<QPair<QPair<int, int>, QString>> edits;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int idx = m.captured(1).toInt(&ok) - 1;
        if (!ok || idx < 0 || static_cast<size_t>(idx) >= layer.curves.size()
            || (idx < curveIdxMap.size() && curveIdxMap[idx] < 0)) {
            edits.append({ qMakePair(m.capturedStart(0), m.capturedLength(0)), QString() });
            continue;
        }
        const QString modifier = m.captured(2).trimmed().toUpper();
        const Origin::GraphCurve &cv = layer.curves[static_cast<size_t>(idx)];
        const bool isErrCurve = (cv.type == Origin::GraphCurve::ErrorBar
                                 || cv.type == Origin::GraphCurve::XErrorBar
                                 || cv.type == Origin::GraphCurve::YErrorBar
                                 || cv.type == Origin::GraphCurve::XYErrorBar);
        const QString dataStr = decodeMbcs(cv.dataName.c_str());
        const QString rawTbl = (dataStr.size() > 2) ? dataStr.mid(2) : QString();
        QString yColName = decodeMbcs(cv.yColumnName.c_str());
        yColName.replace(QRegExp(".*_"), "");
        QString xColName = decodeMbcs(cv.xColumnName.c_str());
        xColName.replace(QRegExp(".*_"), "");

        Table *tbl = resolveImportedOriginTable(mw, rawTbl);
        Column *yCol = (tbl && !yColName.isEmpty()) ? tbl->column(yColName) : nullptr;
        Column *xCol = (tbl && !xColName.isEmpty()) ? tbl->column(xColName) : nullptr;

        QString replacement;
        if (isErrCurve) {
            if (yCol && !yCol->comment().trimmed().isEmpty())
                replacement = yCol->comment().trimmed();
            else if (yCol && !yCol->longName().trimmed().isEmpty())
                replacement = yCol->longName().trimmed();
        } else if (modifier.startsWith(QLatin1String("@X"))) {
            if (xCol && !xCol->comment().trimmed().isEmpty())
                replacement = xCol->comment().trimmed();
            else if (xCol && !xCol->longName().trimmed().isEmpty())
                replacement = xCol->longName().trimmed();
            else
                replacement = xColName;
        } else if (modifier == QLatin1String("@WS") || modifier == QLatin1String("@WL")) {
            replacement = (tbl && !tbl->windowLabel().trimmed().isEmpty()
                           && modifier == QLatin1String("@WL"))
                    ? tbl->windowLabel().trimmed()
                    : rawTbl;
        } else if (modifier == QLatin1String("@U") && yCol) {
            replacement = yCol->units().trimmed();
        } else if (modifier == QLatin1String("@C") && yCol) {
            replacement = yCol->comment().trimmed();
        } else if (modifier == QLatin1String("@SN")) {
            replacement = yColName;
        } else {
            if (yCol && !yCol->comment().trimmed().isEmpty())
                replacement = yCol->comment().trimmed();
            else if (yCol && !yCol->longName().trimmed().isEmpty())
                replacement = yCol->longName().trimmed();
            else if (!yColName.isEmpty())
                replacement = yColName;
            else
                replacement = rawTbl;
        }

        edits.append({ qMakePair(m.capturedStart(0), m.capturedLength(0)), replacement });
    }
    for (int i = edits.size() - 1; i >= 0; --i)
        out.replace(edits[i].first.first, edits[i].first.second, edits[i].second);

    // Strip lines left with a bare \c{k} curve symbol and empty label text (e.g. unnamed ErrorBars or unplotted curves)
    if (out.contains(QLatin1String("\\c{")) || out.contains(QLatin1Char('\n'))) {
        static const QRegularExpression kBareCurveSymOnlyRe(
                QStringLiteral("^\\s*(?:\\\\c\\{\\d+\\}\\s*)+$"));
        const QStringList lines = out.split(QLatin1Char('\n'));
        QStringList kept;
        kept.reserve(lines.size());
        for (const QString &ln : lines) {
            if (kBareCurveSymOnlyRe.match(ln).hasMatch())
                continue;
            if (lines.size() > 1 && ln.trimmed().isEmpty())
                continue;
            kept.append(ln);
        }
        out = kept.join(QLatin1Char('\n'));
    }

    return out;
}

// TODO: bug in grid dialog
//		scale/minor ticks checkbox
//		histogram: autobin export
//		if prec not setted - automac+4digits
