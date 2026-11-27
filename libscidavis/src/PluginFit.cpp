/***************************************************************************
    File                 : PluginFit.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Plugin Fit class

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
#include "PluginFit.h"

#include <QLibrary>
#include <QMessageBox>
using namespace std;

PluginFit::PluginFit(ApplicationWindow *parent, Graph *g) : Fit(parent, g)
{
    init();
}

PluginFit::PluginFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle);
}

PluginFit::PluginFit(ApplicationWindow *parent, Graph *g, const QString &curveTitle, double start,
                     double end)
    : Fit(parent, g)
{
    init();
    setDataFromCurve(curveTitle, start, end);
}

void PluginFit::init()
{
    f_eval = nullptr;
    d_explanation = tr("Plugin Fit");
}

bool PluginFit::load(const QString &pluginName)
{
    if (!QFile::exists(pluginName)) {
        d_init_err = true;
        if (!isSilentOrNonInteractive())
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("File not found"),
                    tr("Plugin file: <p><b> %1 </b> <p>not found. Operation aborted!").arg(pluginName));
        return false;
    }

    QLibrary lib(pluginName);

    auto new_fsimplex = (fit_function_simplex)lib.resolve("function_d");
    if (!new_fsimplex) {
        d_init_err = true;
        if (!isSilentOrNonInteractive())
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Plugin Error"),
                    tr("The plugin does not implement a %1 method necessary for simplex fitting.")
                            .arg("function_d"));
        return false;
    }

    auto new_f = (fit_function)lib.resolve("function_f");
    if (!new_f) {
        d_init_err = true;
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Plugin Error"),
                                  tr("The plugin does not implement a %1 method necessary for "
                                     "Levenberg-Marquardt fitting.")
                                          .arg("function_f"));
        return false;
    }

    auto new_df = (fit_function_df)lib.resolve("function_df");
    if (!new_df) {
        d_init_err = true;
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Plugin Error"),
                                  tr("The plugin does not implement a %1 method necessary for "
                                     "Levenberg-Marquardt fitting.")
                                          .arg("function_df"));
        return false;
    }

    auto new_fdf = (fit_function_fdf)lib.resolve("function_fdf");
    if (!new_fdf) {
        d_init_err = true;
        if (!isSilentOrNonInteractive())
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Plugin Error"),
                                  tr("The plugin does not implement a %1 method necessary for "
                                     "Levenberg-Marquardt fitting.")
                                          .arg("function_fdf"));
        return false;
    }

    auto new_feval = (fitFunctionEval)lib.resolve("function_eval");
    if (!new_feval) {
        d_init_err = true;
        return false;
    }

    typedef char *(*fitFunc)();
    fitFunc paramsFn = (fitFunc)lib.resolve("parameters");
    QStringList parsedNames;
    if (paramsFn) {
        const char *rawParams = paramsFn();
        if (rawParams) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            parsedNames = QString(rawParams).split(",", Qt::SkipEmptyParts);
#else
            parsedNames = QString(rawParams).split(",", QString::SkipEmptyParts);
#endif
        }
    }
    if (parsedNames.isEmpty()) {
        d_init_err = true;
        return false;
    }

    fitFunc formulaFn = (fitFunc)lib.resolve("function");
    const char *rawFormula = formulaFn ? formulaFn() : nullptr;
    if (!rawFormula) {
        d_init_err = true;
        return false;
    }

    // All required symbols validated; commit atomically.
    if (d_param_init) {
        gsl_vector_free(d_param_init);
        d_param_init = nullptr;
    }
    if (covar) {
        gsl_matrix_free(covar);
        covar = nullptr;
    }

    d_fsimplex = new_fsimplex;
    d_f = new_f;
    d_df = new_df;
    d_fdf = new_fdf;
    f_eval = new_feval;
    d_param_names = parsedNames;
    d_p = (int)d_param_names.count();
    d_min_points = d_p;
    d_param_init = gsl_vector_alloc(d_p);
    gsl_vector_set_all(d_param_init, 1.0);
    covar = gsl_matrix_alloc(d_p, d_p);
    gsl_matrix_set_zero(covar);
    d_results.assign(d_p, 0.0);
    d_formula = QString(rawFormula);

    d_param_explain.clear();
    fitFunc fitExplain = (fitFunc)lib.resolve("explanations");
    if (fitExplain && fitExplain())
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        d_param_explain = QString(fitExplain()).split(",", Qt::SkipEmptyParts);
#else
        d_param_explain = QString(fitExplain()).split(",", QString::SkipEmptyParts);
#endif
    while (static_cast<unsigned>(d_param_explain.size()) < d_p)
        d_param_explain << "";

    setObjectName(pluginName);
    fitFunc nameFn = (fitFunc)lib.resolve("name");
    if (nameFn && nameFn())
        setObjectName(QString(nameFn()));

    d_init_err = false;
    return true;
}

bool PluginFit::calculateFitCurveData(const vector<double> &par, std::vector<double> &X,
                                      std::vector<double> &Y)
{
    if (!f_eval || d_p == 0 || par.size() < d_p)
        return false;
    generateX(X);
    for (int i = 0; i < d_points; i++)
        // TODO second par should be const double*
        Y[i] = f_eval(X[i], const_cast<double *>(&par[0]));
    return true;
}
