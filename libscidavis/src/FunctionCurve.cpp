/***************************************************************************
    File                 : FunctionCurve.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Function curve class

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
#include "FunctionCurve.h"
#include "ApplicationWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QScopedPointer>
#include <cmath>

// FIXME: While FunctionCurve itself supports arbitrary scripting
// interpreters now, fit curves assume muParser in many formulas (using
// "^" as power operator). Thus, simply having FunctionCurve honour the
// application-wide scripting environment would break fit curve plotting.
FunctionCurve::FunctionCurve(ApplicationWindow *parent, QString name)
    : PlotCurve(name),
      scripted(ScriptingLangManager::newEnv("muParser", parent)),
      d_function_type(Normal),
      d_from(0.0),
      d_to(0.0)
{
    d_variable = "x";
    setType(Graph::Function);
}

FunctionCurve::FunctionCurve(ApplicationWindow *parent, const FunctionType &t, QString name)
    : PlotCurve(name),
      scripted(ScriptingLangManager::newEnv("muParser", parent)),
      d_function_type(t),
      d_from(0.0),
      d_to(0.0)
{
    d_variable = "x";
    setType(Graph::Function);
}

void FunctionCurve::setRange(double from, double to)
{
    d_from = from;
    d_to = to;
}

void FunctionCurve::copy(FunctionCurve *f)
{
    d_function_type = f->functionType();
    d_variable = f->variable();
    d_formulas = f->formulas();
    d_from = f->startRange();
    d_to = f->endRange();
}

QString FunctionCurve::legend() const
{
    if (d_function_type == Normal) {
        const QString f0 = d_formulas.value(0).trimmed();
        return f0.isEmpty() ? title().text() : (title().text() + ": " + d_formulas.value(0));
    }
    QString label = title().text() + ": ";
    if (d_function_type == Parametric) {
        label += "X(" + d_variable + ")=" + d_formulas.value(0);
        label += ", Y(" + d_variable + ")=" + d_formulas.value(1);
    } else if (d_function_type == Polar) {
        label += "R(" + d_variable + ")=" + d_formulas.value(0);
        label += ", Theta(" + d_variable + ")=" + d_formulas.value(1);
    }
    return label;
}

bool FunctionCurve::loadData(int points)
{
    if (!points)
        points = dataSize();
    if (points <= 0)
        points = 100;

    if (!scriptEnv)
        return dataSize() > 0;

    if (d_function_type == Normal) {
        if (d_formulas.isEmpty() || d_formulas.at(0).trimmed().isEmpty())
            return dataSize() > 0;
    } else if (d_function_type == Parametric || d_function_type == Polar) {
        if (d_formulas.size() < 2 || d_formulas.at(0).trimmed().isEmpty()
            || d_formulas.at(1).trimmed().isEmpty())
            return dataSize() > 0;
    }

    double *X = new double[points];
    double *Y = new double[points];
    double step = (points > 1) ? ((d_to - d_from) / (double)(points - 1)) : 0.0;

    switch (d_function_type) {
    case Normal: {
        QScopedPointer<Script> script(scriptEnv->newScript(d_formulas[0], 0, title().text()));
        if (!script) {
            delete[] X;
            delete[] Y;
            return dataSize() > 0;
        }
        QObject::connect(script.data(), SIGNAL(error(const QString &, const QString &, int)), this,
                         SLOT(scriptError(const QString &, const QString &, int)));
        int i;
        double x;
        for (i = 0, x = d_from; i < points; i++, x += step) {
            X[i] = x;
            script->setDouble(x, d_variable.toUtf8().constData());
            QVariant result = script->eval();
            if (result.type() != QVariant::Double) {
                delete[] X;
                delete[] Y;
                return false;
            }
            Y[i] = result.toDouble();
        }
        break;
    }
    case Parametric:
    case Polar: {
        QScopedPointer<Script> script_x(scriptEnv->newScript(d_formulas[0], 0, title().text()));
        QScopedPointer<Script> script_y(scriptEnv->newScript(d_formulas[1], 0, title().text()));
        if (!script_x || !script_y) {
            delete[] X;
            delete[] Y;
            return dataSize() > 0;
        }
        int i;
        double par;
        for (i = 0, par = d_from; i < points; i++, par += step) {
            script_x->setDouble(par, d_variable.toUtf8().constData());
            script_y->setDouble(par, d_variable.toUtf8().constData());
            QVariant result_x = script_x->eval();
            QVariant result_y = script_y->eval();
            if (result_x.type() != QVariant::Double || result_y.type() != QVariant::Double) {
                delete[] X;
                delete[] Y;
                return false;
            }
            if (d_function_type == Polar) {
                X[i] = result_x.toDouble() * cos(result_y.toDouble());
                Y[i] = result_x.toDouble() * sin(result_y.toDouble());
            } else {
                X[i] = result_x.toDouble();
                Y[i] = result_y.toDouble();
            }
        }
        break;
    }
    }
    setData(X, Y, points);
    delete[] X;
    delete[] Y;
    return true;
}

void FunctionCurve::setSuppressScriptErrors(bool suppress)
{
    setProperty("_tg_suppress_script_err", suppress);
}

void FunctionCurve::scriptError(const QString &message, const QString &scriptName, int lineNumber)
{
    if (property("_tg_suppress_script_err").toBool())
        return;
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (ApplicationWindow *aw = qobject_cast<ApplicationWindow *>(tlw)) {
            if (aw->isNonInteractive())
                return;
        }
    }
    QMessageBox::critical(0, tr("Input function error"),
                          QString("%1:%2\n\n%3").arg(scriptName).arg(lineNumber).arg(message));
}
