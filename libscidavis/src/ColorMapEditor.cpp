/***************************************************************************
        File                 : ColorMapEditor.cpp
        Project              : SciDAVis
--------------------------------------------------------------------
        Copyright            : (C) 2006 by Ion Vasilief
        Email (use @ for *)  : ion_vasilief*yahoo.fr
        Description          : A QwtLinearColorMap Editor Widget
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
#include "ColorMapEditor.h"
#include "ColorButton.h"

#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QColorDialog>
#include <QLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMessageBox>

ColorMapEditor::ColorMapEditor(QWidget *parent)
    : QWidget(parent), color_map(QwtLinearColorMap()), min_val(0), max_val(1)
{
    table = new QTableWidget();
    table->setColumnCount(2);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
#if QT_VERSION >= 0x050000
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionsClickable(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
    table->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setClickable(false);
    table->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif
    table->viewport()->setMouseTracking(true);
    table->viewport()->installEventFilter(this);
    table->setHorizontalHeaderLabels(QStringList() << tr("Level") << tr("Color"));
    table->setMinimumHeight(6 * table->horizontalHeader()->height() + 2);
    table->installEventFilter(this);

    connect(table, SIGNAL(cellClicked(int, int)), this, SLOT(showColorDialog(int, int)));
    connect(table, SIGNAL(currentCellChanged(int, int, int, int)), this,
            SLOT(enableButtons(int, int, int, int)));
    connect(table, SIGNAL(cellChanged(int, int)), this, SLOT(validateLevel(int, int)));

    insertBtn = new QPushButton(tr("&Insert"));
    insertBtn->setEnabled(false);
    connect(insertBtn, SIGNAL(clicked()), this, SLOT(insertLevel()));

    deleteBtn = new QPushButton(tr("&Delete"));
    deleteBtn->setEnabled(false);
    connect(deleteBtn, SIGNAL(clicked()), this, SLOT(deleteLevel()));

    QHBoxLayout *hb = new QHBoxLayout();
    hb->addWidget(insertBtn);
    hb->addWidget(deleteBtn);

    scaleColorsBox = new QCheckBox(tr("&Scale Colors"));
    scaleColorsBox->setChecked(true);
    connect(scaleColorsBox, SIGNAL(toggled(bool)), this, SLOT(setScaledColors(bool)));

    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->setSpacing(0);
    vl->addWidget(table);
    vl->addLayout(hb);
    vl->addWidget(scaleColorsBox);

    setFocusProxy(table);
    setMaximumWidth(200);
}

void ColorMapEditor::updateColorMap()
{
    const int rows = table->rowCount();
    if (rows < 2 || !table->item(0, 1) || !table->item(rows - 1, 1))
        return;
    QColor c_min = QColor(COLORVALUE(table->item(0, 1)->text()));
    QColor c_max = QColor(COLORVALUE(table->item(rows - 1, 1)->text()));
    QwtLinearColorMap map(c_min, c_max);
    const QwtDoubleInterval range(min_val, max_val);
    const double span = range.width();
    if (span > 0.0 && std::isfinite(span)) {
        for (int i = 1; i < rows - 1; i++) {
            QTableWidgetItem *lvlIt = table->item(i, 0);
            QTableWidgetItem *colIt = table->item(i, 1);
            if (!lvlIt || !colIt)
                continue;
            const double val = qBound(0.0, (lvlIt->text().toDouble() - min_val) / span, 1.0);
            map.addColorStop(val, QColor(COLORVALUE(colIt->text())));
        }
    }

    color_map = map;
    setScaledColors(scaleColorsBox->isChecked());
}

void ColorMapEditor::setColorMap(const QwtLinearColorMap &map)
{
    scaleColorsBox->setChecked(map.mode() == QwtLinearColorMap::ScaledColors);

    QwtArray<double> colors = map.colorStops();
    int rows = (int)colors.size();
    table->setRowCount(rows);
    table->blockSignals(true);

    for (int i = 0; i < rows; i++) {
        QwtDoubleInterval range = QwtDoubleInterval(min_val, max_val);
        double val = min_val + colors[i] * range.width();

        QTableWidgetItem *it = new QTableWidgetItem(QString::number(val));
        table->setItem(i, 0, it);

        QColor c = QColor(map.rgb(QwtDoubleInterval(0, 1), colors[i]));
        it = new QTableWidgetItem(c.name());
        it->setFlags(it->flags() & (~Qt::ItemIsEditable));
        it->setBackground(QBrush(c));
        it->setForeground(QBrush(c));
        table->setItem(i, 1, it);
    }
    table->blockSignals(false);

    color_map = map;
}

void ColorMapEditor::setRange(double min, double max)
{
    min_val = min;
    max_val = max;
}

void ColorMapEditor::insertLevel()
{
    const int row = table->currentRow();
    if (row <= 0 || row >= table->rowCount())
        return;
    QTableWidgetItem *curIt = table->item(row, 0);
    QTableWidgetItem *prevIt = table->item(row - 1, 0);
    if (!curIt || !prevIt)
        return;

    const QwtDoubleInterval range(min_val, max_val);
    const double span = range.width();
    const double val = 0.5 * (curIt->text().toDouble() + prevIt->text().toDouble());
    const double mapped_val = (span > 0.0 && std::isfinite(span))
            ? qBound(0.0, (val - min_val) / span, 1.0)
            : 0.0;
    QColor c = QColor(color_map.rgb(QwtDoubleInterval(0, 1), mapped_val));

    table->blockSignals(true);
    table->insertRow(row);

    QTableWidgetItem *it = new QTableWidgetItem(QString::number(val));
    table->setItem(row, 0, it);

    it = new QTableWidgetItem(c.name());
    it->setFlags(it->flags() & (~Qt::ItemIsEditable));
    it->setBackground(QBrush(c));
    it->setForeground(QBrush(c));
    table->setItem(row, 1, it);
    table->blockSignals(false);

    enableButtons(table->currentRow(), 0);
    updateColorMap();
}

void ColorMapEditor::deleteLevel()
{
    const int row = table->currentRow();
    if (row <= 0 || row >= table->rowCount() - 1)
        return;
    table->removeRow(row);
    enableButtons(table->currentRow(), 0);
    updateColorMap();
}

void ColorMapEditor::showColorDialog(int row, int col)
{
    if (col != 1 || row < 0 || row >= table->rowCount() || !table->item(row, 1))
        return;

    QColor c = QColor(COLORVALUE(table->item(row, 1)->text()));
    QColor color = QColorDialog::getColor(c, this);
    if (!color.isValid() || color == c)
        return;

    table->item(row, 1)->setText(color.name());
    table->item(row, 1)->setForeground(QBrush(color));
    table->item(row, 1)->setBackground(QBrush(color));

    updateColorMap();
}

bool ColorMapEditor::eventFilter(QObject *object, QEvent *e)
{
    if (e->type() == QEvent::MouseMove && object == table->viewport()) {
        const QMouseEvent *me = (const QMouseEvent *)e;
        QPoint pos = table->viewport()->mapToParent(me->pos());
        int row = table->rowAt(pos.y() - table->horizontalHeader()->height());
        if (table->columnAt(pos.x()) == 1 && row >= 0 && row < table->rowCount())
            setCursor(QCursor(Qt::PointingHandCursor));
        else
            setCursor(QCursor(Qt::ArrowCursor));
        return true;
    } else if (e->type() == QEvent::Leave && object == table->viewport()) {
        setCursor(QCursor(Qt::ArrowCursor));
        return true;
    } else if (e->type() == QEvent::KeyPress && object == table) {
        QKeyEvent *ke = (QKeyEvent *)e;
        if (ke->key() == Qt::Key_Return && table->currentColumn() == 1) {
            showColorDialog(table->currentRow(), 1);
            return true;
        }
        return false;
    }
    return QObject::eventFilter(object, e);
}

void ColorMapEditor::validateLevel(int row, int col)
{
    if (col || row < 0 || row >= table->rowCount() || !table->item(row, 0))
        return;

    if (row == 0 || row == table->rowCount() - 1) {
        QMessageBox::critical(this, tr("Input Error"), tr("Sorry, you cannot edit this value!"));
        table->blockSignals(true);
        if (!row)
            table->item(0, 0)->setText(QString::number(min_val));
        else
            table->item(row, 0)->setText(QString::number(max_val));
        table->blockSignals(false);
        return;
    }

    const QString raw = table->item(row, 0)->text().trimmed();
    bool ok = false;
    double val = QLocale().toDouble(raw, &ok);
    if (!ok)
        val = QLocale::c().toDouble(QString(raw).replace(QLatin1Char(','), QLatin1Char('.')), &ok);
    const bool user_input_error = (!ok || !std::isfinite(val));
    if (user_input_error) {
        QMessageBox::critical(this, tr("Input Error"),
                              tr("Please enter a valid color level value!"));
    }

    const QwtDoubleInterval range(min_val, max_val);
    if (user_input_error || !range.contains(val)) {
        const QwtArray<double> colors = color_map.colorStops();
        const double stop = (row < int(colors.size())) ? colors[row] : 0.5;
        val = min_val + stop * range.width();
        table->blockSignals(true);
        table->item(row, 0)->setText(QString::number(val));
        table->blockSignals(false);
        return;
    }

    table->blockSignals(true);
    table->item(row, 0)->setText(QString::number(val));
    table->blockSignals(false);
    updateColorMap();
}

void ColorMapEditor::enableButtons(int row, int, int, int)
{
    const int rows = table->rowCount();
    const bool validRow = (row >= 0 && row < rows);
    deleteBtn->setEnabled(validRow && row > 0 && row < rows - 1);
    insertBtn->setEnabled(validRow && row > 0);
}

void ColorMapEditor::setScaledColors(bool scale)
{
    if (scale)
        color_map.setMode(QwtLinearColorMap::ScaledColors);
    else
        color_map.setMode(QwtLinearColorMap::FixedColors);
}
