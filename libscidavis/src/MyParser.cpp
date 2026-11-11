/***************************************************************************
    File                 : MyParser.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Parser class based on muParser

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
#include "MyParser.h"
#include <QObject>

MyParser::MyParser() : Parser()
{
    DefineConst(_T("pi"), M_PI);
    DefineConst(_T("Pi"), M_PI);
    DefineConst(_T("PI"), M_PI);

    DefineFun(_T("bessel_j0"), bessel_J0);
    DefineFun(_T("bessel_j1"), bessel_J1);
    DefineFun(_T("bessel_jn"), bessel_Jn);
    DefineFun(_T("bessel_y0"), bessel_Y0);
    DefineFun(_T("bessel_y1"), bessel_Y1);
    DefineFun(_T("bessel_yn"), bessel_Yn);
    DefineFun(_T("beta"), beta);
    DefineFun(_T("erf"), erf);
    DefineFun(_T("erfc"), erfc);
    DefineFun(_T("erfz"), erfz);
    DefineFun(_T("erfq"), erfq);
    DefineFun(_T("gamma"), gamma);
    DefineFun(_T("gammaln"), gammaln);
    DefineFun(_T("hazard"), hazard);
}

QStringList MyParser::functionsList()
{
    QStringList l;
    l << "abs()"
      << "acos()"
      << "acosh()"
      << "asin()"
      << "asinh()"
      << "atan()";
    l << "atanh()"
      << "avg(,)"
      << "bessel_j0()"
      << "bessel_j1()"
      << "bessel_jn(,)";
    l << "bessel_y0()"
      << "bessel_y1()"
      << "bessel_yn(,)"
      << "beta(,)";
    l << "cos()"
      << "cosh()"
      << "erf()"
      << "erfc()"
      << "erfz()"
      << "erfq()";
    l << "exp()"
      << "gamma()"
      << "gammaln()"
      << "hazard()";
    l << "() ? () : ()"
      << "ln()"
      << "log()"
      << "log2()"
      << "min()"
      << "max()";
    l << "rint()"
      << "sign()"
      << "sin()"
      << "sinh()"
      << "sqrt()"
      << "tan()"
      << "tanh()";
    return l;
}

QString MyParser::explainFunction(int index)
{
    static const char *const kExplanations[] = {
        QT_TR_NOOP("abs(x):\n Absolute value of x."),
        QT_TR_NOOP("acos(x):\n Inverse cos function."),
        QT_TR_NOOP("acosh(x):\n Hyperbolic inverse cos function."),
        QT_TR_NOOP("asin(x):\n Inverse sin function."),
        QT_TR_NOOP("asinh(x):\n Hyperbolic inverse sin function."),
        QT_TR_NOOP("atan(x):\n Inverse tan function."),
        QT_TR_NOOP("atanh(x):\n  Hyperbolic inverse tan function."),
        QT_TR_NOOP("avg(x,y,...):\n  Mean value of all arguments."),
        QT_TR_NOOP("bessel_j0(x):\n  Regular cylindrical Bessel function of zeroth order, J_0(x)."),
        QT_TR_NOOP("bessel_j1(x):\n  Regular cylindrical Bessel function of first order, J_1(x)."),
        QT_TR_NOOP("bessel_jn(double x, int n):\n Regular cylindrical Bessel function of order n, "
                   "J_n(x)."),
        QT_TR_NOOP("bessel_y0(x):\n Irregular cylindrical Bessel function of zeroth order, Y_0(x), "
                   "for x>0."),
        QT_TR_NOOP("bessel_y1(x):\n Irregular cylindrical Bessel function of first order, Y_1(x), "
                   "for x>0."),
        QT_TR_NOOP("bessel_yn(double x, int n):\n Irregular cylindrical Bessel function of order n, "
                   "Y_n(x), for x>0."),
        QT_TR_NOOP("beta(a,b):\n Computes the Beta Function, B(a,b) = Gamma(a)*Gamma(b)/Gamma(a+b) "
                   "for a > 0, b > 0."),
        QT_TR_NOOP("cos(x):\n Calculate cosine."),
        QT_TR_NOOP("cosh(x):\n Hyperbolic cos function."),
        QT_TR_NOOP("erf(x):\n  The error function."),
        QT_TR_NOOP("erfc(x):\n Complementary error function erfc(x) = 1 - erf(x)."),
        QT_TR_NOOP("erfz(x):\n The Gaussian probability density function Z(x)."),
        QT_TR_NOOP("erfq(x):\n The upper tail of the Gaussian probability function Q(x)."),
        QT_TR_NOOP("exp(x):\n Exponential function: e raised to the power of x."),
        QT_TR_NOOP("gamma(x):\n Computes the Gamma function, subject to x not being a negative "
                   "integer."),
        QT_TR_NOOP("gammaln(x):\n Computes the logarithm of the Gamma function, subject to x not a "
                   "being negative integer. For x<0, log(|Gamma(x)|) is returned."),
        QT_TR_NOOP("hazard(x):\n Computes the hazard function for the normal distribution h(x) = "
                   "erfz(x)/erfq(x)."),
        QT_TR_NOOP("(e1)?(e2):(e3):\n if e1 then e2 else e3."),
        QT_TR_NOOP("ln(x):\n Calculate natural logarithm log_e."),
        QT_TR_NOOP("log(x):\n Calculate decimal logarithm log_10."),
        QT_TR_NOOP("log2(x):\n Calculate binary logarithm log_2."),
        QT_TR_NOOP("min(x,y,...):\n Calculate minimum of all arguments."),
        QT_TR_NOOP("max(x,y,...):\n Calculate maximum of all arguments."),
        QT_TR_NOOP("rint(x):\n Round to nearest integer."),
        QT_TR_NOOP("sign(x):\n Sign function: -1 if x<0; 1 if x>0."),
        QT_TR_NOOP("sin(x):\n Calculate sine."),
        QT_TR_NOOP("sinh(x):\n Hyperbolic sin function."),
        QT_TR_NOOP("sqrt(x):\n Square root function."),
        QT_TR_NOOP("tan(x):\n Calculate tangent function."),
        QT_TR_NOOP("tanh(x):\n Hyperbolic tan function.")
    };
    const int n = static_cast<int>(sizeof(kExplanations) / sizeof(kExplanations[0]));
    if (index < 0 || index >= n)
        return QString();
    return QObject::tr(kExplanations[index]);
}
