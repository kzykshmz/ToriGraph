/***************************************************************************
    File                 : Fit.cpp
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2006 by Ion Vasilief, Tilman Benkert
    Email (use @ for *)  : ion_vasilief*yahoo.fr, thzs*gmx.net
    Description          : Fit base class

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
#include "Fit.h"
#include "ApplicationWindow.h"
#include "fit_gsl.h"
#include "Table.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "PlotCurve.h"
#include "QwtErrorPlotCurve.h"
#include "Legend.h"
#include "FunctionCurve.h"
#include "ColorButton.h"
#include "Script.h"
#include "NonLinearFit.h"
#include "PolynomialFit.h"
#include "MultiPeakFit.h"
#include "ExponentialFit.h"
#include "SigmoidalFit.h"
#include "future/core/column/Column.h"
#include "I18nJapanese.h"

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_deriv.h>
#include <gsl/gsl_version.h>

#include <QApplication>
#include <QMessageBox>
#include <QDateTime>
#include <QLocale>
#include <QRegExp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>

using namespace std;

namespace {

bool isNonInteractiveFit(const QObject *parentObj, bool silent = false)
{
    if (silent)
        return true;
    if (qApp && qApp->property("torigraph_noninteractive").toBool())
        return true;
    const ApplicationWindow *app = dynamic_cast<const ApplicationWindow *>(parentObj);
    return app && app->isHeadlessMode();
}

struct AffineExpr
{
    std::map<QString, double> coeffs;
    double constant = 0.0;

    bool isConstant() const
    {
        for (const auto &kv : coeffs) {
            if (std::abs(kv.second) > 1e-15)
                return false;
        }
        return true;
    }
};

class LinearSideParser
{
public:
    explicit LinearSideParser(const QString &src) : s(src), pos(0) {}

    bool parse(AffineExpr &out)
    {
        if (!parseAddSub(out))
            return false;
        skipSpaces();
        return pos == s.size();
    }

private:
    QString s;
    int pos;

    void skipSpaces()
    {
        while (pos < s.size() && s[pos].isSpace())
            ++pos;
    }

    bool parseAddSub(AffineExpr &out)
    {
        if (!parseMulDiv(out))
            return false;
        while (true) {
            skipSpaces();
            if (pos >= s.size() || (s[pos] != QLatin1Char('+') && s[pos] != QLatin1Char('-')))
                break;
            const double sign = (s[pos] == QLatin1Char('+')) ? 1.0 : -1.0;
            ++pos;
            AffineExpr rhs;
            if (!parseMulDiv(rhs))
                return false;
            out.constant += sign * rhs.constant;
            for (const auto &kv : rhs.coeffs)
                out.coeffs[kv.first] += sign * kv.second;
        }
        return true;
    }

    bool parseMulDiv(AffineExpr &out)
    {
        if (!parseUnary(out))
            return false;
        while (true) {
            skipSpaces();
            if (pos >= s.size() || (s[pos] != QLatin1Char('*') && s[pos] != QLatin1Char('/')))
                break;
            const QChar op = s[pos++];
            AffineExpr rhs;
            if (!parseUnary(rhs))
                return false;
            if (op == QLatin1Char('*')) {
                if (out.isConstant()) {
                    const double c = out.constant;
                    out.constant = c * rhs.constant;
                    out.coeffs.clear();
                    for (const auto &kv : rhs.coeffs)
                        out.coeffs[kv.first] = c * kv.second;
                } else if (rhs.isConstant()) {
                    const double c = rhs.constant;
                    out.constant *= c;
                    for (auto &kv : out.coeffs)
                        kv.second *= c;
                } else {
                    return false; // Nonlinear product of two parameters
                }
            } else {
                if (!rhs.isConstant() || std::abs(rhs.constant) <= 1e-30)
                    return false; // Nonlinear division or division by zero
                const double inv = 1.0 / rhs.constant;
                out.constant *= inv;
                for (auto &kv : out.coeffs)
                    kv.second *= inv;
            }
        }
        return true;
    }

    bool parseUnary(AffineExpr &out)
    {
        skipSpaces();
        if (pos < s.size() && (s[pos] == QLatin1Char('+') || s[pos] == QLatin1Char('-'))) {
            const double sign = (s[pos] == QLatin1Char('+')) ? 1.0 : -1.0;
            ++pos;
            if (!parseUnary(out))
                return false;
            out.constant *= sign;
            for (auto &kv : out.coeffs)
                kv.second *= sign;
            return true;
        }
        return parsePrimary(out);
    }

    bool parsePrimary(AffineExpr &out)
    {
        skipSpaces();
        if (pos >= s.size())
            return false;

        if (s[pos] == QLatin1Char('(')) {
            ++pos;
            if (!parseAddSub(out))
                return false;
            skipSpaces();
            if (pos >= s.size() || s[pos] != QLatin1Char(')'))
                return false;
            ++pos;
            return true;
        }

        // Number literal (supports scientific notation)
        if (s[pos].isDigit() || s[pos] == QLatin1Char('.')) {
            int start = pos;
            bool seenDot = false;
            while (pos < s.size() && (s[pos].isDigit() || (!seenDot && s[pos] == QLatin1Char('.')))) {
                if (s[pos] == QLatin1Char('.'))
                    seenDot = true;
                ++pos;
            }
            if (pos < s.size() && (s[pos] == QLatin1Char('e') || s[pos] == QLatin1Char('E'))) {
                int expPos = pos + 1;
                if (expPos < s.size() && (s[expPos] == QLatin1Char('+') || s[expPos] == QLatin1Char('-')))
                    ++expPos;
                if (expPos < s.size() && s[expPos].isDigit()) {
                    pos = expPos;
                    while (pos < s.size() && s[pos].isDigit())
                        ++pos;
                }
            }
            bool ok = false;
            double val = s.mid(start, pos - start).toDouble(&ok);
            if (!ok || !std::isfinite(val))
                return false;
            out = AffineExpr();
            out.constant = val;
            return true;
        }

        // Parameter identifier
        if (s[pos].isLetter() || s[pos] == QLatin1Char('_')) {
            int start = pos;
            while (pos < s.size() && (s[pos].isLetterOrNumber() || s[pos] == QLatin1Char('_')))
                ++pos;
            QString ident = s.mid(start, pos - start);
            out = AffineExpr();
            out.coeffs[ident] = 1.0;
            return true;
        }

        return false;
    }
};

struct ParsedLinearEquation
{
    std::map<QString, double> coeffs; // sum(coeff * var) = rhs
    double rhs = 0.0;
};

static bool parseLinearConstraintList(const QString &exprList,
                                      std::vector<ParsedLinearEquation> &equationsOut)
{
    equationsOut.clear();
    QString trimmedAll = exprList.trimmed();
    if (trimmedAll.isEmpty())
        return true;

    const QStringList rawLines = trimmedAll.split(QRegExp("[;\\r\\n]+"));
    QStringList rawEqs;
    for (const QString &line : rawLines) {
        QString chunk = line.trimmed();
        if (chunk.isEmpty())
            continue;
        // Support comma-separated equations like "w1=w2, A2=0.5*A1"
        if (chunk.count(QLatin1Char('=')) > 1 && chunk.contains(QLatin1Char(','))) {
            const QStringList sub = chunk.split(QLatin1Char(','));
            for (const QString &part : sub) {
                if (!part.trimmed().isEmpty())
                    rawEqs.append(part.trimmed());
            }
        } else {
            rawEqs.append(chunk);
        }
    }

    for (const QString &eqStr : rawEqs) {
        int eqIdx = eqStr.indexOf(QLatin1Char('='));
        if (eqIdx <= 0 || eqIdx != eqStr.lastIndexOf(QLatin1Char('=')))
            return false;
        QString lhsStr = eqStr.left(eqIdx).trimmed();
        QString rhsStr = eqStr.mid(eqIdx + 1).trimmed();
        if (lhsStr.isEmpty() || rhsStr.isEmpty())
            return false;

        AffineExpr lhs, rhs;
        LinearSideParser lp(lhsStr), rp(rhsStr);
        if (!lp.parse(lhs) || !rp.parse(rhs))
            return false;

        ParsedLinearEquation peq;
        peq.rhs = rhs.constant - lhs.constant;
        for (const auto &kv : lhs.coeffs)
            peq.coeffs[kv.first] += kv.second;
        for (const auto &kv : rhs.coeffs)
            peq.coeffs[kv.first] -= kv.second;

        bool hasNonZeroVar = false;
        for (auto it = peq.coeffs.begin(); it != peq.coeffs.end();) {
            if (std::abs(it->second) <= 1e-15) {
                it = peq.coeffs.erase(it);
            } else {
                hasNonZeroVar = true;
                ++it;
            }
        }
        if (!hasNonZeroVar) {
            if (std::abs(peq.rhs) > 1e-10)
                return false;
            continue;
        }
        equationsOut.push_back(peq);
    }
    return true;
}

static int resolveParamNameIndex(const QString &varName, const QStringList &paramNames,
                                 unsigned d_p, bool useZeroBasedP)
{
    for (int j = 0; j < paramNames.size() && unsigned(j) < d_p; ++j) {
        if (paramNames[j] == varName)
            return j;
    }
    for (int j = 0; j < paramNames.size() && unsigned(j) < d_p; ++j) {
        if (paramNames[j].compare(varName, Qt::CaseInsensitive) == 0)
            return j;
    }
    QRegExp pRx("^[pP](\\d+)$");
    if (pRx.exactMatch(varName)) {
        int idx = pRx.cap(1).toInt();
        int mapped = useZeroBasedP ? idx : (idx - 1);
        if (mapped >= 0 && unsigned(mapped) < d_p)
            return mapped;
    }
    return -1;
}

struct CompiledLinearConstraints
{
    bool valid = true;
    std::vector<int> dep_indices;
    std::vector<double> dep_const;
    std::vector<std::vector<double>> dep_full_coeffs; // [m][j_full]

    bool isDependent(int j) const
    {
        return std::find(dep_indices.begin(), dep_indices.end(), j) != dep_indices.end();
    }
};

static CompiledLinearConstraints compileLinearConstraintsForFit(
        const QString &exprList, unsigned d_p, const QStringList &paramNames,
        const std::vector<bool> &fixedFlags, const std::vector<bool> &boundedFlags,
        const std::vector<double> &baseInternalVals,
        const std::vector<double> &internalToUserSlope,
        const std::vector<double> &internalToUserOffset)
{
    CompiledLinearConstraints out;
    if (exprList.trimmed().isEmpty())
        return out;

    std::vector<ParsedLinearEquation> eqs;
    if (!parseLinearConstraintList(exprList, eqs)) {
        out.valid = false;
        return out;
    }
    if (eqs.empty() || d_p == 0)
        return out;

    bool usesPZero = false;
    QRegExp pZeroRx("^[pP]0$");
    for (const auto &eq : eqs) {
        for (const auto &kv : eq.coeffs) {
            bool inExplicitNames = false;
            for (const QString &nm : paramNames) {
                if (nm.compare(kv.first, Qt::CaseInsensitive) == 0) {
                    inExplicitNames = true;
                    break;
                }
            }
            if (!inExplicitNames && pZeroRx.exactMatch(kv.first))
                usesPZero = true;
        }
    }

    const size_t mRows = eqs.size();
    std::vector<std::vector<double>> A(mRows, std::vector<double>(d_p, 0.0));
    std::vector<double> b(mRows, 0.0);

    for (size_t r = 0; r < mRows; ++r) {
        double rhs = eqs[r].rhs;
        for (const auto &kv : eqs[r].coeffs) {
            int j = resolveParamNameIndex(kv.first, paramNames, d_p, usesPZero);
            if (j < 0 || unsigned(j) >= d_p) {
                out.valid = false;
                return out;
            }
            const double alpha = (size_t(j) < internalToUserSlope.size()) ? internalToUserSlope[size_t(j)] : 1.0;
            const double beta = (size_t(j) < internalToUserOffset.size()) ? internalToUserOffset[size_t(j)] : 0.0;
            A[r][size_t(j)] += kv.second * alpha;
            rhs -= kv.second * beta;
        }
        // Substitute fixed parameters into RHS
        for (unsigned j = 0; j < d_p; ++j) {
            if (j < fixedFlags.size() && fixedFlags[j]) {
                double fixedVal = (j < baseInternalVals.size()) ? baseInternalVals[j] : 0.0;
                rhs -= A[r][j] * fixedVal;
                A[r][j] = 0.0;
            }
        }
        b[r] = rhs;
    }

    // Prefer unbounded non-fixed parameters as dependent pivots so box constraints stay on free params
    std::vector<int> colOrder;
    colOrder.reserve(d_p);
    for (unsigned j = 0; j < d_p; ++j) {
        bool fixed = (j < fixedFlags.size() && fixedFlags[j]);
        bool bounded = (j < boundedFlags.size() && boundedFlags[j]);
        if (!fixed && !bounded)
            colOrder.push_back(int(j));
    }
    for (unsigned j = 0; j < d_p; ++j) {
        bool fixed = (j < fixedFlags.size() && fixedFlags[j]);
        bool bounded = (j < boundedFlags.size() && boundedFlags[j]);
        if (!fixed && bounded)
            colOrder.push_back(int(j));
    }

    size_t pivotRow = 0;
    std::vector<int> pivotCols;
    for (int col : colOrder) {
        if (pivotRow >= mRows)
            break;
        size_t bestR = pivotRow;
        double bestAbs = std::abs(A[pivotRow][size_t(col)]);
        for (size_t r = pivotRow + 1; r < mRows; ++r) {
            double v = std::abs(A[r][size_t(col)]);
            if (v > bestAbs) {
                bestAbs = v;
                bestR = r;
            }
        }
        if (bestAbs <= 1e-12)
            continue;

        if (bestR != pivotRow) {
            std::swap(A[pivotRow], A[bestR]);
            std::swap(b[pivotRow], b[bestR]);
        }

        const double piv = A[pivotRow][size_t(col)];
        for (unsigned j = 0; j < d_p; ++j)
            A[pivotRow][j] /= piv;
        b[pivotRow] /= piv;

        for (size_t r = 0; r < mRows; ++r) {
            if (r == pivotRow)
                continue;
            const double factor = A[r][size_t(col)];
            if (std::abs(factor) <= 1e-15)
                continue;
            for (unsigned j = 0; j < d_p; ++j)
                A[r][j] -= factor * A[pivotRow][j];
            A[r][size_t(col)] = 0.0;
            b[r] -= factor * b[pivotRow];
        }

        pivotCols.push_back(col);
        ++pivotRow;
    }

    // Check residual rows for inconsistency (0 = non-zero)
    for (size_t r = pivotRow; r < mRows; ++r) {
        double maxCoeff = 0.0;
        for (unsigned j = 0; j < d_p; ++j)
            maxCoeff = std::max(maxCoeff, std::abs(A[r][j]));
        if (maxCoeff <= 1e-12 && std::abs(b[r]) > 1e-8) {
            out.valid = false;
            return out;
        }
    }

    out.dep_indices = pivotCols;
    out.dep_const.resize(pivotCols.size(), 0.0);
    out.dep_full_coeffs.assign(pivotCols.size(), std::vector<double>(d_p, 0.0));
    for (size_t m = 0; m < pivotCols.size(); ++m) {
        int depCol = pivotCols[m];
        out.dep_const[m] = b[m];
        for (unsigned j = 0; j < d_p; ++j) {
            if (int(j) == depCol)
                continue;
            if (std::abs(A[m][j]) > 1e-15)
                out.dep_full_coeffs[m][j] = -A[m][j];
        }
    }
    return out;
}

enum ParamBoundMode {
    BoundNone = 0,
    BoundBoth = 1,
    BoundLower = 2,
    BoundUpper = 3
};

struct DependentBoundPenalty
{
    size_t dep_slot = 0; // index into dep_indices
    double lo = -std::numeric_limits<double>::infinity();
    double hi = std::numeric_limits<double>::infinity();
    double scale = 1e5;
    double delta = 1e-9;
};

struct ConstrainedFitContext
{
    FitData *orig_data = nullptr;
    Fit::fit_function orig_f = nullptr;
    Fit::fit_function_df orig_df = nullptr;
    Fit::fit_function_fdf orig_fdf = nullptr;
    Fit::fit_function_simplex orig_fsimplex = nullptr;
    size_t n_pts = 0;
    unsigned p_full = 0;
    std::vector<int> free_to_full;
    std::vector<double> base_p;
    std::vector<ParamBoundMode> modes;
    std::vector<double> lo;
    std::vector<double> hi;
    std::vector<int> dep_indices;
    std::vector<double> dep_const;
    std::vector<std::vector<double>> dep_weights; // [m][k_free]
    std::vector<DependentBoundPenalty> dep_penalties;
    gsl_vector *x_full_buf = nullptr;
    gsl_matrix *J_full_buf = nullptr;

    ~ConstrainedFitContext()
    {
        if (x_full_buf)
            gsl_vector_free(x_full_buf);
        if (J_full_buf)
            gsl_matrix_free(J_full_buf);
    }

    double mapUtoP(size_t k, double u) const
    {
        switch (modes[k]) {
        case BoundNone:
            return u;
        case BoundBoth: {
            double uc = std::max(-16.0, std::min(16.0, u));
            return lo[k] + 0.5 * (hi[k] - lo[k]) * (std::tanh(uc) + 1.0);
        }
        case BoundLower: {
            double uc = std::max(-1e8, std::min(1e8, u));
            return lo[k] + uc * uc;
        }
        case BoundUpper: {
            double uc = std::max(-1e8, std::min(1e8, u));
            return hi[k] - uc * uc;
        }
        }
        return u;
    }

    double dPdU(size_t k, double u) const
    {
        switch (modes[k]) {
        case BoundNone:
            return 1.0;
        case BoundBoth: {
            double uc = std::max(-16.0, std::min(16.0, u));
            double ch = std::cosh(uc);
            double sech2 = 1.0 / (ch * ch);
            return 0.5 * (hi[k] - lo[k]) * std::max(1e-13, sech2);
        }
        case BoundLower: {
            double deriv = 2.0 * u;
            return (std::abs(deriv) < 1e-10) ? (u >= 0.0 ? 1e-10 : -1e-10) : deriv;
        }
        case BoundUpper: {
            double deriv = -2.0 * u;
            return (std::abs(deriv) < 1e-10) ? (u >= 0.0 ? -1e-10 : 1e-10) : deriv;
        }
        }
        return 1.0;
    }

    double snapP(size_t k, double p) const
    {
        if (modes[k] == BoundBoth) {
            double span = std::max(1.0, hi[k] - lo[k]);
            if (p <= lo[k] + 1e-6 * span)
                return lo[k];
            if (p >= hi[k] - 1e-6 * span)
                return hi[k];
            return std::max(lo[k], std::min(hi[k], p));
        }
        if (modes[k] == BoundLower) {
            if (p <= lo[k] + 1e-7 * std::max(1.0, std::abs(lo[k])))
                return lo[k];
            return std::max(lo[k], p);
        }
        if (modes[k] == BoundUpper) {
            if (p >= hi[k] - 1e-7 * std::max(1.0, std::abs(hi[k])))
                return hi[k];
            return std::min(hi[k], p);
        }
        return p;
    }

    void applyDependentParams(std::vector<double> &fullVec) const
    {
        for (size_t m = 0; m < dep_indices.size(); ++m) {
            double val = dep_const[m];
            for (size_t k = 0; k < free_to_full.size(); ++k)
                val += dep_weights[m][k] * fullVec[size_t(free_to_full[k])];
            fullVec[size_t(dep_indices[m])] = val;
        }
    }

    void unpackToFull(const gsl_vector *u_vec, gsl_vector *x_full) const
    {
        for (unsigned j = 0; j < p_full; ++j)
            gsl_vector_set(x_full, j, base_p[j]);
        for (size_t k = 0; k < free_to_full.size(); ++k) {
            double u = gsl_vector_get(u_vec, k);
            gsl_vector_set(x_full, free_to_full[k], mapUtoP(k, u));
        }
        for (size_t m = 0; m < dep_indices.size(); ++m) {
            double val = dep_const[m];
            for (size_t k = 0; k < free_to_full.size(); ++k)
                val += dep_weights[m][k] * gsl_vector_get(x_full, free_to_full[k]);
            gsl_vector_set(x_full, dep_indices[m], val);
        }
    }

    void evalPenaltyResidualAndSlope(size_t q, double &rOut, double &drDpDepOut) const
    {
        const DependentBoundPenalty &pen = dep_penalties[q];
        const int depIdx = dep_indices[pen.dep_slot];
        const double pDep = gsl_vector_get(x_full_buf, size_t(depIdx));
        rOut = 0.0;
        drDpDepOut = 0.0;
        if (std::isfinite(pen.lo) && pDep < pen.lo) {
            const double v = pen.lo - pDep; // > 0, dv/dpDep = -1
            const double hyp = std::hypot(v, pen.delta);
            rOut = pen.scale * (hyp - pen.delta);
            drDpDepOut = -pen.scale * (v / std::max(hyp, 1e-30));
        } else if (std::isfinite(pen.hi) && pDep > pen.hi) {
            const double v = pDep - pen.hi; // > 0, dv/dpDep = +1
            const double hyp = std::hypot(v, pen.delta);
            rOut = pen.scale * (hyp - pen.delta);
            drDpDepOut = pen.scale * (v / std::max(hyp, 1e-30));
        }
    }

    void projectJacobianToFree(const gsl_vector *u, gsl_matrix *J_free) const
    {
        for (size_t k = 0; k < free_to_full.size(); ++k) {
            int j = free_to_full[k];
            double scale = dPdU(k, gsl_vector_get(u, k));
            for (size_t i = 0; i < n_pts; ++i) {
                double df_dpk = gsl_matrix_get(J_full_buf, i, j);
                for (size_t m = 0; m < dep_indices.size(); ++m) {
                    double w = dep_weights[m][k];
                    if (w != 0.0)
                        df_dpk += gsl_matrix_get(J_full_buf, i, dep_indices[m]) * w;
                }
                gsl_matrix_set(J_free, i, k, df_dpk * scale);
            }
            for (size_t q = 0; q < dep_penalties.size(); ++q) {
                double rPen = 0.0, drDpDep = 0.0;
                evalPenaltyResidualAndSlope(q, rPen, drDpDep);
                const size_t m = dep_penalties[q].dep_slot;
                const double w = dep_weights[m][k];
                gsl_matrix_set(J_free, n_pts + q, k, drDpDep * w * scale);
            }
        }
    }

    void populateFullCovariance(gsl_matrix *covarOut) const
    {
        const size_t p_free = free_to_full.size();
        if (p_free == 0 || !covarOut)
            return;

        // Build full-to-free sensitivity matrix T (p_full x p_free)
        std::vector<std::vector<double>> Tmat(p_full, std::vector<double>(p_free, 0.0));
        for (size_t k = 0; k < p_free; ++k)
            Tmat[size_t(free_to_full[k])][k] = 1.0;
        for (size_t m = 0; m < dep_indices.size(); ++m) {
            for (size_t k = 0; k < p_free; ++k)
                Tmat[size_t(dep_indices[m])][k] = dep_weights[m][k];
        }

        gsl_matrix *J_phys_free = gsl_matrix_alloc(n_pts, p_free);
        gsl_matrix *cov_free = gsl_matrix_alloc(p_free, p_free);
        for (size_t k = 0; k < p_free; ++k) {
            int j = free_to_full[k];
            for (size_t i = 0; i < n_pts; ++i) {
                double df_dpk = gsl_matrix_get(J_full_buf, i, j);
                for (size_t m = 0; m < dep_indices.size(); ++m) {
                    double w = dep_weights[m][k];
                    if (w != 0.0)
                        df_dpk += gsl_matrix_get(J_full_buf, i, dep_indices[m]) * w;
                }
                gsl_matrix_set(J_phys_free, i, k, df_dpk);
            }
        }
        gsl_multifit_covar(J_phys_free, 0.0, cov_free);

        gsl_matrix_set_zero(covarOut);
        for (unsigned a = 0; a < p_full; ++a) {
            for (unsigned b = 0; b < p_full; ++b) {
                double sum = 0.0;
                for (size_t k1 = 0; k1 < p_free; ++k1) {
                    if (Tmat[a][k1] == 0.0)
                        continue;
                    for (size_t k2 = 0; k2 < p_free; ++k2) {
                        if (Tmat[b][k2] != 0.0)
                            sum += Tmat[a][k1] * gsl_matrix_get(cov_free, k1, k2) * Tmat[b][k2];
                    }
                }
                gsl_matrix_set(covarOut, a, b, sum);
            }
        }

        gsl_matrix_free(J_phys_free);
        gsl_matrix_free(cov_free);
    }

    static int eval_f(const gsl_vector *u, void *params, gsl_vector *f)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        gsl_vector_view fData = gsl_vector_subvector(f, 0, ctx->n_pts);
        int st = ctx->orig_f(ctx->x_full_buf, ctx->orig_data, &fData.vector);
        if (st != GSL_SUCCESS)
            return st;
        for (size_t q = 0; q < ctx->dep_penalties.size(); ++q) {
            double rPen = 0.0, drDpDep = 0.0;
            ctx->evalPenaltyResidualAndSlope(q, rPen, drDpDep);
            gsl_vector_set(f, ctx->n_pts + q, rPen);
        }
        return GSL_SUCCESS;
    }

    static int eval_df(const gsl_vector *u, void *params, gsl_matrix *J_free)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        int st = ctx->orig_df(ctx->x_full_buf, ctx->orig_data, ctx->J_full_buf);
        if (st != GSL_SUCCESS)
            return st;
        ctx->projectJacobianToFree(u, J_free);
        return GSL_SUCCESS;
    }

    static int eval_fdf(const gsl_vector *u, void *params, gsl_vector *f, gsl_matrix *J_free)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        gsl_vector_view fData = gsl_vector_subvector(f, 0, ctx->n_pts);
        int st = GSL_SUCCESS;
        if (ctx->orig_fdf) {
            st = ctx->orig_fdf(ctx->x_full_buf, ctx->orig_data, &fData.vector, ctx->J_full_buf);
        } else {
            st = ctx->orig_f(ctx->x_full_buf, ctx->orig_data, &fData.vector);
            if (st == GSL_SUCCESS)
                st = ctx->orig_df(ctx->x_full_buf, ctx->orig_data, ctx->J_full_buf);
        }
        if (st != GSL_SUCCESS)
            return st;
        for (size_t q = 0; q < ctx->dep_penalties.size(); ++q) {
            double rPen = 0.0, drDpDep = 0.0;
            ctx->evalPenaltyResidualAndSlope(q, rPen, drDpDep);
            gsl_vector_set(f, ctx->n_pts + q, rPen);
        }
        ctx->projectJacobianToFree(u, J_free);
        return GSL_SUCCESS;
    }

    static double eval_simplex(const gsl_vector *u, void *params)
    {
        ConstrainedFitContext *ctx = static_cast<ConstrainedFitContext *>(params);
        ctx->unpackToFull(u, ctx->x_full_buf);
        double cost = ctx->orig_fsimplex(ctx->x_full_buf, ctx->orig_data);
        if (!std::isfinite(cost))
            return cost;
        for (size_t q = 0; q < ctx->dep_penalties.size(); ++q) {
            double rPen = 0.0, drDpDep = 0.0;
            ctx->evalPenaltyResidualAndSlope(q, rPen, drDpDep);
            cost += rPen * rPen;
        }
        return cost;
    }
};

static void enforceDependentParameterBounds(unsigned d_p, CompiledLinearConstraints &lc,
                                            std::vector<bool> &fixedMask,
                                            std::vector<bool> &boundedMask,
                                            std::vector<double> &loInternal,
                                            std::vector<double> &hiInternal,
                                            ConstrainedFitContext &ctx)
{
    if (!lc.valid || lc.dep_indices.empty())
        return;

    // 1. Tighten free parameter bounds whenever a bounded dependent parameter depends on <= 1 free param
    bool changed = true;
    for (int pass = 0; pass < int(lc.dep_indices.size()) + 1 && changed; ++pass) {
        changed = false;
        for (size_t m = 0; m < lc.dep_indices.size(); ++m) {
            const int dep = lc.dep_indices[m];
            if (dep < 0 || unsigned(dep) >= d_p || !boundedMask[size_t(dep)])
                continue;
            const double loD = loInternal[size_t(dep)];
            const double hiD = hiInternal[size_t(dep)];
            if (!std::isfinite(loD) && !std::isfinite(hiD))
                continue;

            int nnz = 0;
            int singleJ = -1;
            for (unsigned j = 0; j < d_p; ++j) {
                if (fixedMask[j] || lc.isDependent(int(j)))
                    continue;
                if (std::abs(lc.dep_full_coeffs[m][j]) > 1e-14) {
                    ++nnz;
                    singleJ = int(j);
                }
            }
            if (nnz == 0) {
                if (std::isfinite(loD) && lc.dep_const[m] < loD)
                    lc.dep_const[m] = loD;
                if (std::isfinite(hiD) && lc.dep_const[m] > hiD)
                    lc.dep_const[m] = hiD;
            } else if (nnz == 1 && singleJ >= 0) {
                const size_t jf = size_t(singleJ);
                const double w = lc.dep_full_coeffs[m][jf];
                const double c = lc.dep_const[m];
                double newLo = loInternal[jf];
                double newHi = hiInternal[jf];
                if (w > 0.0) {
                    if (std::isfinite(loD))
                        newLo = std::max(newLo, (loD - c) / w);
                    if (std::isfinite(hiD))
                        newHi = std::min(newHi, (hiD - c) / w);
                } else {
                    if (std::isfinite(loD))
                        newHi = std::min(newHi, (loD - c) / w);
                    if (std::isfinite(hiD))
                        newLo = std::max(newLo, (hiD - c) / w);
                }
                if (std::isfinite(newLo) && std::isfinite(newHi) && newLo > newHi) {
                    const double mid = 0.5 * (newLo + newHi);
                    newLo = mid;
                    newHi = mid;
                }
                loInternal[jf] = newLo;
                hiInternal[jf] = newHi;
                if (std::isfinite(newLo))
                    ctx.base_p[jf] = std::max(ctx.base_p[jf], newLo);
                if (std::isfinite(newHi))
                    ctx.base_p[jf] = std::min(ctx.base_p[jf], newHi);
                if (std::isfinite(newLo) && std::isfinite(newHi)
                    && std::abs(newHi - newLo) <= 1e-15) {
                    const double fixedVal = 0.5 * (newLo + newHi);
                    ctx.base_p[jf] = fixedVal;
                    fixedMask[jf] = true;
                    for (size_t m2 = 0; m2 < lc.dep_indices.size(); ++m2) {
                        lc.dep_const[m2] += lc.dep_full_coeffs[m2][jf] * fixedVal;
                        lc.dep_full_coeffs[m2][jf] = 0.0;
                    }
                    changed = true;
                } else if (std::isfinite(newLo) || std::isfinite(newHi)) {
                    boundedMask[jf] = true;
                }
            }
        }
    }

    // 2. Project initial free guesses toward feasible polytope for multi-parameter dependent bounds
    for (int iter = 0; iter < 30; ++iter) {
        bool anyViol = false;
        for (size_t m = 0; m < lc.dep_indices.size(); ++m) {
            const int dep = lc.dep_indices[m];
            if (dep < 0 || unsigned(dep) >= d_p || !boundedMask[size_t(dep)])
                continue;
            const double loD = loInternal[size_t(dep)];
            const double hiD = hiInternal[size_t(dep)];
            double val = lc.dep_const[m];
            double norm2 = 0.0;
            for (unsigned j = 0; j < d_p; ++j) {
                if (fixedMask[j] || lc.isDependent(int(j)))
                    continue;
                const double w = lc.dep_full_coeffs[m][j];
                val += w * ctx.base_p[j];
                norm2 += w * w;
            }
            if (norm2 <= 1e-28)
                continue;
            double targetDelta = 0.0;
            if (std::isfinite(loD) && val < loD)
                targetDelta = loD - val;
            else if (std::isfinite(hiD) && val > hiD)
                targetDelta = hiD - val;
            if (std::abs(targetDelta) > 1e-12) {
                anyViol = true;
                const double step = targetDelta / norm2;
                for (unsigned j = 0; j < d_p; ++j) {
                    if (fixedMask[j] || lc.isDependent(int(j)))
                        continue;
                    double pj = ctx.base_p[j] + step * lc.dep_full_coeffs[m][j];
                    if (std::isfinite(loInternal[j]))
                        pj = std::max(pj, loInternal[j]);
                    if (std::isfinite(hiInternal[j]))
                        pj = std::min(pj, hiInternal[j]);
                    ctx.base_p[j] = pj;
                }
            }
        }
        if (!anyViol)
            break;
    }
}

static void registerDependentPenaltySlots(unsigned d_p, const CompiledLinearConstraints &lc,
                                          const std::vector<bool> &boundedMask,
                                          const std::vector<double> &loInternal,
                                          const std::vector<double> &hiInternal,
                                          double initResidualNorm, ConstrainedFitContext &ctx)
{
    ctx.dep_penalties.clear();
    if (!lc.valid || lc.dep_indices.empty())
        return;

    const double baseNorm = (std::isfinite(initResidualNorm) && initResidualNorm > 1.0)
                                    ? initResidualNorm
                                    : 1.0;
    for (size_t m = 0; m < lc.dep_indices.size(); ++m) {
        const int dep = lc.dep_indices[m];
        if (dep < 0 || unsigned(dep) >= d_p || !boundedMask[size_t(dep)])
            continue;
        const double loD = loInternal[size_t(dep)];
        const double hiD = hiInternal[size_t(dep)];
        if (!std::isfinite(loD) && !std::isfinite(hiD))
            continue;
        double span = 1.0;
        if (std::isfinite(loD) && std::isfinite(hiD))
            span = std::max(1e-6, hiD - loD);
        else
            span = std::max(1.0, std::max(std::isfinite(loD) ? std::abs(loD) : 0.0,
                                          std::isfinite(hiD) ? std::abs(hiD) : 0.0));
        DependentBoundPenalty pen;
        pen.dep_slot = m;
        pen.lo = loD;
        pen.hi = hiD;
        pen.scale = 1e5 * baseNorm / span;
        pen.delta = 1e-9 * span;
        ctx.dep_penalties.push_back(pen);
    }
}

void registerFitJapaneseStrings()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Not converged (maximum iterations reached; check the initial values)",
            "未収束(最大反復到達/初期値を確認)");
        reg("Not converged (no further improvement; check the initial values)",
            "未収束(改善が止まった/初期値を確認)");
        reg("The parameters and the curve shown are the last iterate, not a converged result.",
            "表示中のパラメータと曲線は最後の反復値で、収束した結果ではありません。");
        reg("Convergence", "収束");
        reg("converged", "収束");
        reg("Not converged", "未収束");
        reg("Degrees of freedom", "自由度");
        reg("DOF", "自由度");
        reg("LowerCI95", "95%下限");
        reg("UpperCI95", "95%上限");
        reg("95% CI", "95%信頼区間");
        reg("Pearson's r (x, y)", "ピアソンの r (x, y)");
        reg("%1 of %2 points with non-positive values were excluded from the log-space fit.",
            "%2 点のうち、非正値の %1 点を対数空間のフィットから除外しました。");
        reg("Apparent linear fit: no axis is logarithmic, so an ordinary linear fit was performed.",
            "見かけ線形フィット: 対数軸が無いため、通常の線形フィットを行いました。");
        reg("Apparent linear fit needs at least %1 points with positive values on the logarithmic "
            "axis, but only %2 remain after excluding non-positive values.",
            "見かけ線形フィットには対数軸で正の値をもつ点が %1 点以上必要ですが、非正値を除くと %2 "
            "点しか残りません。");
        reg("Fit Warning", "フィットの警告");
        reg("Fit failed (%1)", "フィット失敗(%1)");
        reg("(area %1)", "(面積 %1)");
        reg("(height %1)", "(ピーク高さ %1)");
    });
}

} // namespace

void Fit::registerJapaneseStrings()
{
    registerFitJapaneseStrings();
}

void Fit::setStraightLinePearson(const std::vector<double> &x, const std::vector<double> &y)
{
    d_straight_line_fit = true;
    d_line_pearson_r = std::numeric_limits<double>::quiet_NaN();
    const size_t n = std::min(x.size(), y.size());
    if (n < 2)
        return;
    double mx = 0.0, my = 0.0;
    for (size_t i = 0; i < n; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= double(n);
    my /= double(n);
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double dx = x[i] - mx;
        const double dy = y[i] - my;
        sxy += dx * dy;
        sxx += dx * dx;
        syy += dy * dy;
    }
    if (!(sxx > 0.0) || !(syy > 0.0) || !std::isfinite(sxy))
        return;
    d_line_pearson_r = qBound(-1.0, sxy / std::sqrt(sxx * syy), 1.0);
}

bool Fit::lmGradientIsStationary(gsl_multifit_fdfsolver *s, double gtol)
{
    // MINPACK-style scale-invariant gradient test: the cosine of the angle between the residual
    // vector f and every column of the Jacobian must be small at a stationary point.
    if (!s || !s->f || !s->x)
        return false;
    const size_t n = s->f->size;
    const size_t p = s->x->size;
    const double fnorm = gsl_blas_dnrm2(s->f);
    if (!std::isfinite(fnorm))
        return false;
    if (fnorm == 0.0)
        return true;
    gsl_matrix *J = gsl_matrix_alloc(n, p);
#if GSL_MAJOR_VERSION < 2
    gsl_matrix_memcpy(J, s->J);
#else
    if (gsl_multifit_fdfsolver_jac(s, J) != GSL_SUCCESS) {
        gsl_matrix_free(J);
        return false;
    }
#endif
    bool stationary = true;
    for (size_t j = 0; j < p && stationary; ++j) {
        gsl_vector_view col = gsl_matrix_column(J, j);
        const double cnorm = gsl_blas_dnrm2(&col.vector);
        if (!(cnorm > 0.0))
            continue; // a parameter without influence cannot move the fit any further
        double dot = 0.0;
        gsl_blas_ddot(&col.vector, s->f, &dot);
        const double cosAngle = std::abs(dot) / (cnorm * fnorm);
        if (!std::isfinite(cosAngle) || cosAngle > gtol)
            stationary = false;
    }
    gsl_matrix_free(J);
    return stationary;
}

QString Fit::convergenceMessage() const
{
    switch (d_convergence) {
    case MaxIterationsReached:
        return tr("Not converged (maximum iterations reached; check the initial values)");
    case NoProgress:
        return tr("Not converged (no further improvement; check the initial values)");
    case Converged:
        break;
    }
    return QString();
}

QStringList Fit::fitNotes() const
{
    QStringList notes = d_fit_notes;
    if (!d_weighting_note.isEmpty() && !notes.contains(d_weighting_note))
        notes << d_weighting_note;
    return notes;
}

int Fit::degreesOfFreedom() const
{
    return effectivePointCount() - effectiveFreeParameters();
}

double Fit::parameterStdError(int i) const
{
    if (i < 0 || unsigned(i) >= d_p || !covar)
        return std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(chi_2) || chi_2 < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (isParameterFixed(i))
        return 0.0;
    const bool scaled = (d_y_error_source == UnknownErrors || d_scale_errors);
    if (scaled && degreesOfFreedom() <= 0)
        return std::numeric_limits<double>::quiet_NaN();
    const double v = effectiveCovarScale() * gsl_matrix_get(covar, size_t(i), size_t(i));
    if (!std::isfinite(v))
        return std::numeric_limits<double>::quiet_NaN();
    return std::sqrt(std::max(0.0, v));
}

double Fit::studentTQuantile(double p, int dof)
{
    if (dof <= 0 || !(p > 0.0) || !(p < 1.0))
        return std::numeric_limits<double>::quiet_NaN();
    return gsl_cdf_tdist_Pinv(p, double(dof));
}

bool Fit::parameterConfidenceInterval(int i, double level, double *lo, double *hi) const
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (lo)
        *lo = nan;
    if (hi)
        *hi = nan;
    if (i < 0 || size_t(i) >= d_results.size() || !std::isfinite(d_results[size_t(i)]))
        return false;
    const int dof = degreesOfFreedom();
    const double se = parameterStdError(i);
    if (dof <= 0 || !std::isfinite(se))
        return false;
    const double t = studentTQuantile(0.5 + 0.5 * std::clamp(level, 0.0, 0.999999), dof);
    if (!std::isfinite(t))
        return false;
    if (lo)
        *lo = d_results[size_t(i)] - t * se;
    if (hi)
        *hi = d_results[size_t(i)] + t * se;
    return true;
}

Fit::Fit(ApplicationWindow *parent, Graph *g, QString name)
    : Filter(parent, g, name), scripted(ScriptingLangManager::newEnv("muParser", parent))
{
    registerFitJapaneseStrings();
    d_p = 0;
    d_curveColor = ColorButton::color(1);
    d_solver = ScaledLevenbergMarquardt;
    // 1e-4 (the old SciDAVis default) stopped LM while parameters still moved in the 4th digit;
    // 1e-6 costs a few extra iterations near the minimum, where LM converges quadratically.
    d_tolerance = 1e-6;
    d_gen_function = true;
    d_points = 100;
    d_max_iterations = 1000;
    d_curve = 0;
    d_formula = QString();
    d_explanation = QString();
    d_y_error_source = UnknownErrors;
    d_y_error_dataset = QString();
    d_prec = parent->fit_output_precision;
    d_init_err = false;
    chi_2 = -1;
    d_scale_errors = false;
    d_sort_data = true;
    d_initial_guesses_set = false;
}

vector<double> Fit::fitGslMultifit(int &iterations, int &status)
{
    vector<double> result(d_p);
    d_convergence = Converged;
    d_last_iterations = 0;
    d_covar_scaled = false;
    if (covar)
        gsl_matrix_set_zero(covar);

    const std::vector<double> weightsVec = computeWeightsVector();
    applyWeightsToErrorVector(weightsVec);

    auto populateYFit = [this](const gsl_vector *f_vec, const std::vector<double> &solverPar) {
        bool anyExcluded = false;
        d_y_fit.resize(d_x.size());
        for (size_t i = 0; i < d_x.size(); ++i) {
            if (std::isfinite(d_y_errors[i]) && d_y_errors[i] > 0.0) {
                d_y_fit[i] = d_y[i] + gsl_vector_get(f_vec, i) * d_y_errors[i];
            } else {
                d_y_fit[i] = std::numeric_limits<double>::quiet_NaN();
                anyExcluded = true;
            }
        }
        if (anyExcluded && !solverPar.empty()) {
            const bool oldGen = d_gen_function;
            const int oldPts = d_points;
            d_gen_function = false;
            d_points = int(d_x.size());
            std::vector<double> X_tmp(d_x.size()), Y_tmp(d_x.size());
            if (evaluateModelCurve(solverPar, X_tmp, Y_tmp) && Y_tmp.size() == d_x.size()) {
                for (size_t i = 0; i < d_x.size(); ++i) {
                    if (!std::isfinite(d_y_fit[i]))
                        d_y_fit[i] = Y_tmp[i];
                }
            }
            d_gen_function = oldGen;
            d_points = oldPts;
        }
    };

    // declare input data
    struct FitData data = { d_x.size(), size_t(d_p), d_x.data(), d_y.data(), d_y_errors.data(),
                            this };

    const gsl_multifit_fdfsolver_type *T = nullptr;
    switch (d_solver) {
    case ScaledLevenbergMarquardt:
        T = gsl_multifit_fdfsolver_lmsder;
        break;
    case UnscaledLevenbergMarquardt:
        T = gsl_multifit_fdfsolver_lmder;
        break;
    default:
        T = gsl_multifit_fdfsolver_lmsder;
        break;
    }

    if (hasActiveConstraints()) {
        ConstrainedFitContext ctx;
        ctx.orig_data = &data;
        ctx.orig_f = d_f;
        ctx.orig_df = d_df;
        ctx.orig_fdf = d_fdf;
        ctx.orig_fsimplex = d_fsimplex;
        ctx.n_pts = d_x.size();
        ctx.p_full = d_p;
        ctx.base_p.resize(d_p, 0.0);
        ctx.x_full_buf = gsl_vector_alloc(d_p);
        ctx.J_full_buf = gsl_matrix_alloc(d_x.size(), d_p);

        std::vector<bool> fixedMask(d_p, false);
        std::vector<bool> boundedMask(d_p, false);
        std::vector<double> loInternal(d_p, -std::numeric_limits<double>::infinity());
        std::vector<double> hiInternal(d_p, std::numeric_limits<double>::infinity());
        std::vector<double> slopeToUser(d_p, 1.0);
        std::vector<double> offsetToUser(d_p, 0.0);

        for (unsigned j = 0; j < d_p; ++j) {
            double p0 = gsl_vector_get(d_param_init, j);
            double u0 = fromInternalParameter(int(j), 0.0);
            double u1 = fromInternalParameter(int(j), 1.0);
            slopeToUser[j] = (std::abs(u1 - u0) > 1e-15) ? (u1 - u0) : 1.0;
            offsetToUser[j] = u0;

            double lo_u = -std::numeric_limits<double>::infinity();
            double hi_u = std::numeric_limits<double>::infinity();
            bool bounded = hasParameterBounds(int(j), &lo_u, &hi_u);
            double lo = lo_u, hi = hi_u;
            if (bounded) {
                if (std::isfinite(lo_u))
                    lo = toInternalParameter(int(j), lo_u);
                if (std::isfinite(hi_u))
                    hi = toInternalParameter(int(j), hi_u);
                if (lo > hi)
                    std::swap(lo, hi);
            }
            loInternal[j] = lo;
            hiInternal[j] = hi;

            if (isParameterFixed(int(j))
                || (bounded && std::isfinite(lo) && std::isfinite(hi) && std::abs(hi - lo) <= 1e-15)) {
                if (!isParameterFixed(int(j)) && bounded)
                    p0 = 0.5 * (lo + hi);
                fixedMask[j] = true;
            } else if (bounded && (std::isfinite(lo) || std::isfinite(hi))) {
                boundedMask[j] = true;
            }
            ctx.base_p[j] = p0;
        }

        CompiledLinearConstraints lc = compileLinearConstraintsForFit(
                d_linear_constraint_expr, d_p, d_param_names, fixedMask, boundedMask, ctx.base_p,
                slopeToUser, offsetToUser);
        enforceDependentParameterBounds(d_p, lc, fixedMask, boundedMask, loInternal, hiInternal,
                                        ctx);

        std::vector<double> u0_vals;
        for (unsigned j = 0; j < d_p; ++j) {
            if (fixedMask[j] || (lc.valid && lc.isDependent(int(j))))
                continue;

            double p0 = ctx.base_p[j];
            double lo = loInternal[j];
            double hi = hiInternal[j];
            bool bounded = boundedMask[j];
            ctx.free_to_full.push_back(int(j));
            if (!bounded || (!std::isfinite(lo) && !std::isfinite(hi))) {
                ctx.modes.push_back(BoundNone);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(p0);
            } else if (std::isfinite(lo) && std::isfinite(hi)) {
                ctx.modes.push_back(BoundBoth);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double t = (p0 - lo) / (hi - lo);
                t = std::max(1e-4, std::min(1.0 - 1e-4, t));
                u0_vals.push_back(std::atanh(2.0 * t - 1.0));
            } else if (std::isfinite(lo)) {
                ctx.modes.push_back(BoundLower);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double d0 = std::max(1e-3, p0 - lo);
                u0_vals.push_back(std::sqrt(d0));
            } else {
                ctx.modes.push_back(BoundUpper);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double d0 = std::max(1e-3, hi - p0);
                u0_vals.push_back(std::sqrt(d0));
            }
        }

        if (lc.valid && !lc.dep_indices.empty()) {
            ctx.dep_indices = lc.dep_indices;
            ctx.dep_const = lc.dep_const;
            ctx.dep_weights.assign(lc.dep_indices.size(),
                                   std::vector<double>(ctx.free_to_full.size(), 0.0));
            for (size_t m = 0; m < lc.dep_indices.size(); ++m) {
                for (size_t k = 0; k < ctx.free_to_full.size(); ++k)
                    ctx.dep_weights[m][k] = lc.dep_full_coeffs[m][size_t(ctx.free_to_full[k])];
            }
            ctx.applyDependentParams(ctx.base_p);
        }

        const size_t p_free = ctx.free_to_full.size();
        if (p_free == 0) {
            result = ctx.base_p;
            ctx.applyDependentParams(result);
            for (unsigned j = 0; j < d_p; ++j)
                gsl_vector_set(ctx.x_full_buf, j, result[j]);
            gsl_vector *f_vec = gsl_vector_alloc(d_x.size());
            int evalSt = d_f(ctx.x_full_buf, &data, f_vec);
            if (evalSt != GSL_SUCCESS) {
                gsl_vector_free(f_vec);
                chi_2 = -1.0;
                iterations = 0;
                status = evalSt;
                result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
                return result;
            }
            gsl_blas_ddot(f_vec, f_vec, &chi_2);
            populateYFit(f_vec, result);
            gsl_vector_free(f_vec);
            gsl_matrix_set_zero(covar);
            iterations = 0;
            status = GSL_SUCCESS;
            return result;
        }

        double initResNorm = 1.0;
        {
            for (unsigned j = 0; j < d_p; ++j)
                gsl_vector_set(ctx.x_full_buf, j, ctx.base_p[j]);
            gsl_vector *f_init = gsl_vector_alloc(d_x.size());
            if (d_f(ctx.x_full_buf, &data, f_init) == GSL_SUCCESS) {
                double nrm = gsl_blas_dnrm2(f_init);
                if (std::isfinite(nrm) && nrm > 1.0)
                    initResNorm = nrm;
            }
            gsl_vector_free(f_init);
        }
        registerDependentPenaltySlots(d_p, lc, boundedMask, loInternal, hiInternal, initResNorm,
                                      ctx);

        gsl_vector *u_init = gsl_vector_alloc(p_free);
        for (size_t k = 0; k < p_free; ++k)
            gsl_vector_set(u_init, k, u0_vals[k]);

        const size_t n_solver = d_x.size() + ctx.dep_penalties.size();
        gsl_multifit_function_fdf f;
        f.f = &ConstrainedFitContext::eval_f;
        f.df = &ConstrainedFitContext::eval_df;
        f.fdf = &ConstrainedFitContext::eval_fdf;
        f.n = n_solver;
        f.p = p_free;
        f.params = &ctx;

        gsl_multifit_fdfsolver *s = gsl_multifit_fdfsolver_alloc(T, n_solver, p_free);
        status = gsl_multifit_fdfsolver_set(s, &f, u_init);
        if (status) {
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            chi_2 = -1.0;
            result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
            return result;
        }

        std::vector<double> prev_phys(p_free);
        for (size_t k = 0; k < p_free; ++k)
            prev_phys[k] = ctx.mapUtoP(k, u0_vals[k]);

        for (iterations = 0; iterations < d_max_iterations; iterations++) {
            status = gsl_multifit_fdfsolver_iterate(s);
            if (status) {
                // ETOL*: the requested tolerance is below machine precision, i.e. the solver
                // sits at the minimum. ENOPROG is only accepted when the gradient vanishes.
                if (status == GSL_ETOLF || status == GSL_ETOLX || status == GSL_ETOLG) {
                    status = GSL_SUCCESS;
                } else if (status == GSL_ENOPROG) {
                    if (!lmGradientIsStationary(s))
                        d_convergence = NoProgress;
                    status = GSL_SUCCESS;
                }
                break;
            }
            int delta_status = gsl_multifit_test_delta(s->dx, s->x, d_tolerance, d_tolerance);

            bool phys_converged = (iterations >= 2);
            double dx_norm = gsl_blas_dnrm2(s->dx);
            if (dx_norm == 0.0)
                phys_converged = false;
            for (size_t k = 0; k < p_free; ++k) {
                double pj_new = ctx.mapUtoP(k, gsl_vector_get(s->x, k));
                if (std::abs(pj_new - prev_phys[k]) > d_tolerance * (d_tolerance + std::abs(pj_new)))
                    phys_converged = false;
                prev_phys[k] = pj_new;
            }

            if (delta_status == GSL_SUCCESS || phys_converged) {
                status = GSL_SUCCESS;
                break;
            }
            status = delta_status;
            if (status != GSL_CONTINUE)
                break;
        }
        if (status == GSL_CONTINUE) {
            // Iteration budget exhausted: keep the last iterate but flag it as not converged.
            d_convergence = MaxIterationsReached;
            status = GSL_SUCCESS;
        }
        d_last_iterations = iterations;

        if (status != GSL_SUCCESS) {
            gsl_multifit_fdfsolver_free(s);
            gsl_vector_free(u_init);
            chi_2 = -1.0;
            d_y_fit.clear();
            result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
            return result;
        }

        result = ctx.base_p;
        for (size_t k = 0; k < p_free; ++k) {
            double p_val = ctx.snapP(k, ctx.mapUtoP(k, gsl_vector_get(s->x, k)));
            result[ctx.free_to_full[k]] = p_val;
        }
        ctx.applyDependentParams(result);
        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(ctx.x_full_buf, j, result[j]);

        gsl_vector *f_final = gsl_vector_alloc(d_x.size());
        if (d_f(ctx.x_full_buf, &data, f_final) == GSL_SUCCESS) {
            gsl_blas_ddot(f_final, f_final, &chi_2);
            populateYFit(f_final, result);
        } else {
            gsl_vector_view fDataView = gsl_vector_subvector(s->f, 0, d_x.size());
            gsl_blas_ddot(&fDataView.vector, &fDataView.vector, &chi_2);
            populateYFit(s->f, result);
        }
        gsl_vector_free(f_final);

        gsl_matrix_set_zero(covar);
        if (d_df(ctx.x_full_buf, &data, ctx.J_full_buf) == GSL_SUCCESS)
            ctx.populateFullCovariance(covar);

        gsl_multifit_fdfsolver_free(s);
        gsl_vector_free(u_init);
        return result;
    }

    gsl_multifit_function_fdf f;
    f.f = d_f;
    f.df = d_df;
    f.fdf = d_fdf;
    f.n = d_x.size();
    f.p = d_p;
    f.params = &data;

    gsl_multifit_fdfsolver *s = gsl_multifit_fdfsolver_alloc(T, d_x.size(), d_p);
    status = gsl_multifit_fdfsolver_set(s, &f, d_param_init);
    if (status) {
        gsl_multifit_fdfsolver_free(s);
        chi_2 = -1.0;
        result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
        return result;
    }

    // iterate solver algorithm
    for (iterations = 0; iterations < d_max_iterations; iterations++) {
        status = gsl_multifit_fdfsolver_iterate(s);
        if (status) {
            if (status == GSL_ETOLF || status == GSL_ETOLX || status == GSL_ETOLG) {
                status = GSL_SUCCESS;
                break;
            }
            if (status == GSL_ENOPROG) {
                if (!lmGradientIsStationary(s))
                    d_convergence = NoProgress;
                status = GSL_SUCCESS;
                break;
            }
            // If an aggressive unconstrained LM trial step crossed out of domain while s->x is
            // valid, try damped Marquardt backtracking inside the domain before aborting.
            bool recovered = false;
            if ((status == GSL_EINVAL || status == GSL_EDOM) && d_f && d_df && d_p > 0) {
                gsl_matrix *J_bt = gsl_matrix_alloc(d_x.size(), d_p);
                gsl_vector *f_cur = gsl_vector_alloc(d_x.size());
                if (d_f(s->x, &data, f_cur) == GSL_SUCCESS
                    && d_df(s->x, &data, J_bt) == GSL_SUCCESS) {
                    double curNorm2 = 0.0;
                    gsl_blas_ddot(f_cur, f_cur, &curNorm2);
                    std::vector<double> jtj(size_t(d_p) * size_t(d_p), 0.0);
                    std::vector<double> gvec(size_t(d_p), 0.0);
                    double maxGrad = 0.0;
                    for (unsigned r = 0; r < d_x.size(); ++r) {
                        const double fr = gsl_vector_get(f_cur, r);
                        for (unsigned c1 = 0; c1 < d_p; ++c1) {
                            const double j1 = gsl_matrix_get(J_bt, r, c1);
                            gvec[c1] += j1 * fr;
                            for (unsigned c2 = c1; c2 < d_p; ++c2)
                                jtj[size_t(c1) * size_t(d_p) + c2] += j1 * gsl_matrix_get(J_bt, r, c2);
                        }
                    }
                    for (unsigned c1 = 0; c1 < d_p; ++c1) {
                        maxGrad = std::max(maxGrad, std::abs(gvec[c1]));
                        for (unsigned c2 = 0; c2 < c1; ++c2)
                            jtj[size_t(c1) * size_t(d_p) + c2] = jtj[size_t(c2) * size_t(d_p) + c1];
                    }

                    gsl_vector *x_try = gsl_vector_alloc(d_p);
                    gsl_vector *f_try = gsl_vector_alloc(d_x.size());
                    gsl_vector *dx_try = gsl_vector_alloc(d_p);
                    for (int dampPow = -2; dampPow <= 10 && !recovered; ++dampPow) {
                        const double lam = std::pow(10.0, double(dampPow));
                        std::vector<double> A = jtj;
                        std::vector<double> rhs(size_t(d_p), 0.0);
                        for (unsigned c = 0; c < d_p; ++c) {
                            rhs[c] = -gvec[c];
                            const double diag = std::max(1e-12, A[size_t(c) * size_t(d_p) + c]);
                            A[size_t(c) * size_t(d_p) + c] += lam * (diag + 1.0);
                        }
                        // Gaussian elimination with partial pivoting
                        bool nonsingular = true;
                        for (unsigned k = 0; k < d_p; ++k) {
                            unsigned piv = k;
                            double maxA = std::abs(A[size_t(k) * size_t(d_p) + k]);
                            for (unsigned r = k + 1; r < d_p; ++r) {
                                const double v = std::abs(A[size_t(r) * size_t(d_p) + k]);
                                if (v > maxA) {
                                    maxA = v;
                                    piv = r;
                                }
                            }
                            if (!(maxA > 1e-30)) {
                                nonsingular = false;
                                break;
                            }
                            if (piv != k) {
                                for (unsigned c = k; c < d_p; ++c)
                                    std::swap(A[size_t(k) * size_t(d_p) + c],
                                              A[size_t(piv) * size_t(d_p) + c]);
                                std::swap(rhs[k], rhs[piv]);
                            }
                            const double akk = A[size_t(k) * size_t(d_p) + k];
                            for (unsigned r = k + 1; r < d_p; ++r) {
                                const double factor = A[size_t(r) * size_t(d_p) + k] / akk;
                                for (unsigned c = k + 1; c < d_p; ++c)
                                    A[size_t(r) * size_t(d_p) + c] -=
                                            factor * A[size_t(k) * size_t(d_p) + c];
                                rhs[r] -= factor * rhs[k];
                            }
                        }
                        if (!nonsingular)
                            continue;
                        std::vector<double> step(size_t(d_p), 0.0);
                        for (int k = int(d_p) - 1; k >= 0; --k) {
                            double sum = rhs[size_t(k)];
                            for (unsigned c = unsigned(k) + 1; c < d_p; ++c)
                                sum -= A[size_t(k) * size_t(d_p) + c] * step[c];
                            step[size_t(k)] = sum / A[size_t(k) * size_t(d_p) + size_t(k)];
                        }

                        for (int bt = 0; bt < 16; ++bt) {
                            const double alpha = std::ldexp(1.0, -bt);
                            for (unsigned c = 0; c < d_p; ++c) {
                                const double dxc = alpha * step[c];
                                gsl_vector_set(dx_try, c, dxc);
                                gsl_vector_set(x_try, c, gsl_vector_get(s->x, c) + dxc);
                            }
                            if (d_f(x_try, &data, f_try) == GSL_SUCCESS) {
                                double tryNorm2 = 0.0;
                                gsl_blas_ddot(f_try, f_try, &tryNorm2);
                                if (std::isfinite(tryNorm2) && tryNorm2 < curNorm2) {
                                    if (gsl_multifit_fdfsolver_set(s, &f, x_try) == GSL_SUCCESS) {
                                        recovered = true;
                                        if (gsl_multifit_test_delta(dx_try, x_try, d_tolerance,
                                                                    d_tolerance)
                                            == GSL_SUCCESS) {
                                            status = GSL_SUCCESS;
                                        } else {
                                            status = GSL_CONTINUE;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    gsl_vector_free(dx_try);
                    gsl_vector_free(f_try);
                    gsl_vector_free(x_try);

                    if (!recovered && std::isfinite(curNorm2)
                        && maxGrad <= 1e-7 * (1.0 + std::sqrt(curNorm2))) {
                        status = GSL_SUCCESS;
                    }
                }
                gsl_vector_free(f_cur);
                gsl_matrix_free(J_bt);
            }
            if (recovered && status == GSL_CONTINUE)
                continue;
            break;
        }
        int delta_status = gsl_multifit_test_delta(s->dx, s->x, d_tolerance, d_tolerance);
        if (delta_status == GSL_SUCCESS) {
            status = GSL_SUCCESS;
            break;
        }
        status = delta_status;
        if (status != GSL_CONTINUE)
            break;
    }
    if (status == GSL_CONTINUE) {
        d_convergence = MaxIterationsReached;
        status = GSL_SUCCESS;
    }
    d_last_iterations = iterations;

    if (status != GSL_SUCCESS) {
        gsl_multifit_fdfsolver_free(s);
        chi_2 = -1.0;
        d_y_fit.clear();
        result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
        return result;
    }

    // grab results
    bool allFinite = true;
    for (unsigned i = 0; i < d_p; i++) {
        result[i] = gsl_vector_get(s->x, i);
        if (!std::isfinite(result[i]))
            allFinite = false;
    }
    gsl_blas_ddot(s->f, s->f, &chi_2);
    if (!allFinite || !std::isfinite(chi_2) || chi_2 < 0.0) {
        gsl_multifit_fdfsolver_free(s);
        status = GSL_EDOM;
        chi_2 = -1.0;
        d_y_fit.clear();
        result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
        return result;
    }
    populateYFit(s->f, result);
#if GSL_MAJOR_VERSION < 2
    gsl_multifit_covar(s->J, 0.0, covar);
#else
    {
        gsl_matrix *J = gsl_matrix_alloc(d_x.size(), d_p);
        gsl_multifit_fdfsolver_jac(s, J);
        gsl_multifit_covar(J, 0.0, covar);
        gsl_matrix_free(J);
    }
#endif

    // free memory allocated for fitting
    gsl_multifit_fdfsolver_free(s);

    return result;
}

vector<double> Fit::fitGslMultimin(int &iterations, int &status)
{
    vector<double> result(d_p);
    d_convergence = Converged;
    d_last_iterations = 0;
    d_covar_scaled = false;
    if (covar)
        gsl_matrix_set_zero(covar);

    const std::vector<double> weightsVec = computeWeightsVector();
    applyWeightsToErrorVector(weightsVec);

    auto populateYFit = [this](const gsl_vector *f_vec, const std::vector<double> &solverPar) {
        bool anyExcluded = false;
        d_y_fit.resize(d_x.size());
        for (size_t i = 0; i < d_x.size(); ++i) {
            if (std::isfinite(d_y_errors[i]) && d_y_errors[i] > 0.0) {
                d_y_fit[i] = d_y[i] + gsl_vector_get(f_vec, i) * d_y_errors[i];
            } else {
                d_y_fit[i] = std::numeric_limits<double>::quiet_NaN();
                anyExcluded = true;
            }
        }
        if (anyExcluded && !solverPar.empty()) {
            const bool oldGen = d_gen_function;
            const int oldPts = d_points;
            d_gen_function = false;
            d_points = int(d_x.size());
            std::vector<double> X_tmp(d_x.size()), Y_tmp(d_x.size());
            if (evaluateModelCurve(solverPar, X_tmp, Y_tmp) && Y_tmp.size() == d_x.size()) {
                for (size_t i = 0; i < d_x.size(); ++i) {
                    if (!std::isfinite(d_y_fit[i]))
                        d_y_fit[i] = Y_tmp[i];
                }
            }
            d_gen_function = oldGen;
            d_points = oldPts;
        }
    };

    // declare input data
    struct FitData data = { d_x.size(), size_t(d_p), d_x.data(), d_y.data(), d_y_errors.data(),
                            this };

    if (hasActiveConstraints()) {
        ConstrainedFitContext ctx;
        ctx.orig_data = &data;
        ctx.orig_f = d_f;
        ctx.orig_df = d_df;
        ctx.orig_fdf = d_fdf;
        ctx.orig_fsimplex = d_fsimplex;
        ctx.n_pts = d_x.size();
        ctx.p_full = d_p;
        ctx.base_p.resize(d_p, 0.0);
        ctx.x_full_buf = gsl_vector_alloc(d_p);
        ctx.J_full_buf = gsl_matrix_alloc(d_x.size(), d_p);

        std::vector<bool> fixedMask(d_p, false);
        std::vector<bool> boundedMask(d_p, false);
        std::vector<double> loInternal(d_p, -std::numeric_limits<double>::infinity());
        std::vector<double> hiInternal(d_p, std::numeric_limits<double>::infinity());
        std::vector<double> slopeToUser(d_p, 1.0);
        std::vector<double> offsetToUser(d_p, 0.0);

        for (unsigned j = 0; j < d_p; ++j) {
            double p0 = gsl_vector_get(d_param_init, j);
            double u0 = fromInternalParameter(int(j), 0.0);
            double u1 = fromInternalParameter(int(j), 1.0);
            slopeToUser[j] = (std::abs(u1 - u0) > 1e-15) ? (u1 - u0) : 1.0;
            offsetToUser[j] = u0;

            double lo_u = -std::numeric_limits<double>::infinity();
            double hi_u = std::numeric_limits<double>::infinity();
            bool bounded = hasParameterBounds(int(j), &lo_u, &hi_u);
            double lo = lo_u, hi = hi_u;
            if (bounded) {
                if (std::isfinite(lo_u))
                    lo = toInternalParameter(int(j), lo_u);
                if (std::isfinite(hi_u))
                    hi = toInternalParameter(int(j), hi_u);
                if (lo > hi)
                    std::swap(lo, hi);
            }
            loInternal[j] = lo;
            hiInternal[j] = hi;

            if (isParameterFixed(int(j))
                || (bounded && std::isfinite(lo) && std::isfinite(hi) && std::abs(hi - lo) <= 1e-15)) {
                if (!isParameterFixed(int(j)) && bounded)
                    p0 = 0.5 * (lo + hi);
                fixedMask[j] = true;
            } else if (bounded && (std::isfinite(lo) || std::isfinite(hi))) {
                boundedMask[j] = true;
            }
            ctx.base_p[j] = p0;
        }

        CompiledLinearConstraints lc = compileLinearConstraintsForFit(
                d_linear_constraint_expr, d_p, d_param_names, fixedMask, boundedMask, ctx.base_p,
                slopeToUser, offsetToUser);
        enforceDependentParameterBounds(d_p, lc, fixedMask, boundedMask, loInternal, hiInternal,
                                        ctx);

        std::vector<double> u0_vals;
        for (unsigned j = 0; j < d_p; ++j) {
            if (fixedMask[j] || (lc.valid && lc.isDependent(int(j))))
                continue;

            double p0 = ctx.base_p[j];
            double lo = loInternal[j];
            double hi = hiInternal[j];
            bool bounded = boundedMask[j];
            ctx.free_to_full.push_back(int(j));
            if (!bounded || (!std::isfinite(lo) && !std::isfinite(hi))) {
                ctx.modes.push_back(BoundNone);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(p0);
            } else if (std::isfinite(lo) && std::isfinite(hi)) {
                ctx.modes.push_back(BoundBoth);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                double t = (p0 - lo) / (hi - lo);
                t = std::max(1e-4, std::min(1.0 - 1e-4, t));
                u0_vals.push_back(std::atanh(2.0 * t - 1.0));
            } else if (std::isfinite(lo)) {
                ctx.modes.push_back(BoundLower);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(std::sqrt(std::max(1e-3, p0 - lo)));
            } else {
                ctx.modes.push_back(BoundUpper);
                ctx.lo.push_back(lo);
                ctx.hi.push_back(hi);
                u0_vals.push_back(std::sqrt(std::max(1e-3, hi - p0)));
            }
        }

        if (lc.valid && !lc.dep_indices.empty()) {
            ctx.dep_indices = lc.dep_indices;
            ctx.dep_const = lc.dep_const;
            ctx.dep_weights.assign(lc.dep_indices.size(),
                                   std::vector<double>(ctx.free_to_full.size(), 0.0));
            for (size_t m = 0; m < lc.dep_indices.size(); ++m) {
                for (size_t k = 0; k < ctx.free_to_full.size(); ++k)
                    ctx.dep_weights[m][k] = lc.dep_full_coeffs[m][size_t(ctx.free_to_full[k])];
            }
            ctx.applyDependentParams(ctx.base_p);
        }

        const size_t p_free = ctx.free_to_full.size();
        if (p_free == 0) {
            result = ctx.base_p;
            ctx.applyDependentParams(result);
            for (unsigned j = 0; j < d_p; ++j)
                gsl_vector_set(ctx.x_full_buf, j, result[j]);
            chi_2 = d_fsimplex(ctx.x_full_buf, &data);
            if (!std::isfinite(chi_2) || chi_2 < 0.0) {
                chi_2 = -1.0;
                iterations = 0;
                status = GSL_EINVAL;
                result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
                return result;
            }
            if (d_f) {
                gsl_vector *f_vec = gsl_vector_alloc(d_x.size());
                if (d_f(ctx.x_full_buf, &data, f_vec) == GSL_SUCCESS)
                    populateYFit(f_vec, result);
                gsl_vector_free(f_vec);
            }
            gsl_matrix_set_zero(covar);
            iterations = 0;
            status = GSL_SUCCESS;
            return result;
        }

        double initResNorm = 1.0;
        {
            for (unsigned j = 0; j < d_p; ++j)
                gsl_vector_set(ctx.x_full_buf, j, ctx.base_p[j]);
            double cInit = d_fsimplex ? d_fsimplex(ctx.x_full_buf, &data) : 1.0;
            if (!std::isfinite(cInit)) {
                chi_2 = -1.0;
                iterations = 0;
                status = GSL_EINVAL;
                result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
                return result;
            }
            if (cInit > 1.0)
                initResNorm = std::sqrt(cInit);
        }
        registerDependentPenaltySlots(d_p, lc, boundedMask, loInternal, hiInternal, initResNorm,
                                      ctx);

        gsl_vector *u_init = gsl_vector_alloc(p_free);
        gsl_vector *ss = gsl_vector_alloc(p_free);
        for (size_t k = 0; k < p_free; ++k) {
            gsl_vector_set(u_init, k, u0_vals[k]);
            gsl_vector_set(ss, k, (ctx.modes[k] == BoundNone) ? 1.0 : 0.5);
        }

        gsl_multimin_function f;
        f.f = &ConstrainedFitContext::eval_simplex;
        f.n = p_free;
        f.params = &ctx;

        const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex;
        gsl_multimin_fminimizer *s_min = gsl_multimin_fminimizer_alloc(T, f.n);
        gsl_multimin_fminimizer_set(s_min, &f, u_init, ss);

        for (iterations = 0; iterations < d_max_iterations; iterations++) {
            status = gsl_multimin_fminimizer_iterate(s_min);
            if (status)
                break;
            double size = gsl_multimin_fminimizer_size(s_min);
            status = gsl_multimin_test_size(size, d_tolerance);
            if (status != GSL_CONTINUE)
                break;
        }
        if (status == GSL_CONTINUE) {
            d_convergence = MaxIterationsReached;
            status = GSL_SUCCESS;
        }
        d_last_iterations = iterations;

        result = ctx.base_p;
        for (size_t k = 0; k < p_free; ++k)
            result[ctx.free_to_full[k]] = ctx.snapP(k, ctx.mapUtoP(k, gsl_vector_get(s_min->x, k)));
        ctx.applyDependentParams(result);
        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(ctx.x_full_buf, j, result[j]);

        double pureCost = d_fsimplex ? d_fsimplex(ctx.x_full_buf, &data) : s_min->fval;
        chi_2 = std::isfinite(pureCost) ? pureCost : s_min->fval;
        if (!std::isfinite(chi_2) || chi_2 < 0.0) {
            chi_2 = -1.0;
            status = GSL_EINVAL;
            result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
        } else if (d_f) {
            gsl_vector *f_vec = gsl_vector_alloc(d_x.size());
            if (d_f(ctx.x_full_buf, &data, f_vec) == GSL_SUCCESS) {
                gsl_blas_ddot(f_vec, f_vec, &chi_2);
                populateYFit(f_vec, result);
            }
            gsl_vector_free(f_vec);
        }
        gsl_matrix_set_zero(covar);
        if (status == GSL_SUCCESS && d_df && d_df(ctx.x_full_buf, &data, ctx.J_full_buf) == GSL_SUCCESS)
            ctx.populateFullCovariance(covar);

        gsl_multimin_fminimizer_free(s_min);
        gsl_vector_free(ss);
        gsl_vector_free(u_init);
        return result;
    }

    if (d_fsimplex) {
        double cInit = d_fsimplex(d_param_init, &data);
        if (!std::isfinite(cInit)) {
            chi_2 = -1.0;
            iterations = 0;
            status = GSL_EINVAL;
            result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
            return result;
        }
    }

    gsl_multimin_function f;
    f.f = d_fsimplex;
    f.n = d_p;
    f.params = &data;

    // step size (size of the simplex)
    // can be increased for faster convergence
    gsl_vector *ss = gsl_vector_alloc(f.n);
    gsl_vector_set_all(ss, 10.0);

    // initialize minimizer
    const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex;
    gsl_multimin_fminimizer *s_min = gsl_multimin_fminimizer_alloc(T, f.n);
    gsl_multimin_fminimizer_set(s_min, &f, d_param_init, ss);

    // iterate minimization algorithm
    for (iterations = 0; iterations < d_max_iterations; iterations++) {
        status = gsl_multimin_fminimizer_iterate(s_min);
        if (status)
            break;

        double size = gsl_multimin_fminimizer_size(s_min);
        status = gsl_multimin_test_size(size, d_tolerance);
        if (status != GSL_CONTINUE)
            break;
    }
    if (status == GSL_CONTINUE) {
        d_convergence = MaxIterationsReached;
        status = GSL_SUCCESS;
    }
    d_last_iterations = iterations;

    // grab results
    for (unsigned i = 0; i < d_p; i++)
        result[i] = gsl_vector_get(s_min->x, i);
    chi_2 = s_min->fval;
    if (!std::isfinite(chi_2) || chi_2 < 0.0) {
        chi_2 = -1.0;
        status = GSL_EINVAL;
        result.assign(d_p, std::numeric_limits<double>::quiet_NaN());
    } else {
        if (d_f) {
            gsl_vector *f_vec = gsl_vector_alloc(d_x.size());
            if (d_f(s_min->x, (void *)f.params, f_vec) == GSL_SUCCESS)
                populateYFit(f_vec, result);
            gsl_vector_free(f_vec);
        }
        gsl_matrix *J = gsl_matrix_alloc(d_x.size(), d_p);
        if (d_df(s_min->x, (void *)f.params, J) == GSL_SUCCESS)
            gsl_multifit_covar(J, 0.0, covar);
        else
            gsl_matrix_set_zero(covar);
        gsl_matrix_free(J);
    }

    // free previously allocated memory
    gsl_multimin_fminimizer_free(s_min);
    gsl_vector_free(ss);

    return result;
}

void Fit::setDataCurve(int curve, double start, double end)
{
    Filter::setDataCurve(curve, start, end);

    if (d_explicit_weighting) {
        if (!setWeightingMethod(d_weighting_method, d_weighting_col)) {
            d_explicit_weighting = false;
            if (!setYErrorSource(AssociatedErrors, QString(), true))
                setYErrorSource(UnknownErrors);
        }
    } else {
        if (!setYErrorSource(AssociatedErrors, QString(), true))
            setYErrorSource(UnknownErrors);
    }
}

void Fit::setInitialGuesses(double *x_init)
{
    d_explicit_init_vals.clear();
    for (unsigned i = 0; i < d_p; i++)
        gsl_vector_set(d_param_init, i, x_init[i]);
    d_initial_guesses_set = true;
    d_all_guesses_set = true;
}

void Fit::generateFunction(bool yes, int points)
{
    d_gen_function = yes;
    if (d_gen_function)
        d_points = points;
}

QString Fit::logFitInfo(const vector<double> &par, int iterations, int status,
                        const QString &plotName)
{
    QDateTime dt = QDateTime::currentDateTime();
    const QString curveTitle =
            (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0) ? d_curve->title().text()
                                                                      : objectName();
    QString info = "[" + QLocale().toString(dt) + "\t" + tr("Plot") + ": ''" + plotName + "'']\n";
    info += d_explanation + " " + tr("fit of dataset") + ": " + curveTitle;
    if (!d_formula.isEmpty())
        info += ", " + tr("using function") + ": " + d_formula + "\n";
    else
        info += "\n";

    info += tr("Y standard errors") + ": ";
    switch (d_y_error_source) {
    case UnknownErrors:
        info += tr("Unknown");
        break;
    case AssociatedErrors:
        info += tr("Associated dataset (%1)").arg(d_y_error_dataset);
        break;
    case PoissonErrors:
        info += tr("Statistical (assuming Poisson distribution)");
        break;
    case CustomErrors:
        info += tr("Arbitrary Dataset") + ": " + d_y_error_dataset;
        break;
    }
    info += "\n";

    if (is_non_linear) {
        if (d_solver == NelderMeadSimplex)
            info += tr("Nelder-Mead Simplex");
        else if (d_solver == UnscaledLevenbergMarquardt)
            info += tr("Unscaled Levenberg-Marquardt");
        else
            info += tr("Scaled Levenberg-Marquardt");

        info += tr(" algorithm with tolerance = ") + QLocale().toString(d_tolerance) + "\n";
    }

    if (!d_x.empty()) {
        info += tr("From x") + " = " + QLocale().toString(d_x[0], 'g', 15) + " " + tr("to x")
                + " = " + QLocale().toString(d_x.back(), 'g', 15) + "\n";
    }
    if (!converged() && status == GSL_SUCCESS) {
        info += "*** " + convergenceMessage() + " ***\n";
        info += tr("The parameters and the curve shown are the last iterate, not a converged "
                   "result.")
                + "\n";
    }
    for (const QString &note : fitNotes())
        info += note + "\n";

    auto fmtNum = [this](double v) -> QString {
        return std::isfinite(v) ? QLocale().toString(v, 'g', d_prec) : QString::fromUtf8("—");
    };
    double chi_2_dof = reducedChiSquare();
    const int dof = degreesOfFreedom();
    for (unsigned i = 0; i < d_p; i++) {
        const double se = parameterStdError(int(i));
        info += d_param_names[i] + " " + d_param_explain[i] + " = "
                + QLocale().toString(par[i], 'g', d_prec) + " +/- " + fmtNum(se);
        double lo = 0.0, hi = 0.0;
        if (!isParameterFixed(int(i)) && parameterConfidenceInterval(int(i), 0.95, &lo, &hi))
            info += "   " + tr("95% CI") + " [" + fmtNum(lo) + ", " + fmtNum(hi) + "]";
        info += "\n";
    }
    info += "--------------------------------------------------------------------------------------"
            "\n";
    info += "Chi^2 = " + fmtNum(chi_2) + "\n";
    info += tr("Reduced Chi^2") + " (Chi^2/doF) = " + fmtNum(chi_2_dof) + "\n";
    info += tr("Degrees of freedom") + " = " + QString::number(std::max(0, dof)) + "\n";
    info += tr("RSS") + " = " + fmtNum(rss()) + "\n";
    info += tr("R^2") + " (COD) = " + fmtNum(rSquare()) + "\n";
    info += tr("Adjusted R^2") + " = " + fmtNum(adjustedRSquare()) + "\n";
    if (d_straight_line_fit)
        info += tr("Pearson's r (x, y)") + " = " + fmtNum(pearsonR()) + "\n";
    info += "--------------------------------------------------------------------------------------"
            "-\n";
    if (is_non_linear) {
        info += tr("Iterations") + " = " + QString::number(iterations) + "\n";
        if (status == GSL_SUCCESS && !converged())
            info += tr("Status") + " = " + convergenceMessage() + "\n";
        else
            info += tr("Status") + " = " + gsl_strerror(status) + "\n";
        info += "----------------------------------------------------------------------------------"
                "-----\n";
    }
    return info;
}

double Fit::effectiveCovarScale() const
{
    if (d_y_error_source == UnknownErrors || d_scale_errors) {
        const int dof = degreesOfFreedom();
        if (dof > 0 && std::isfinite(chi_2) && chi_2 >= 0.0)
            return chi_2 / double(dof);
        return std::numeric_limits<double>::quiet_NaN();
    }
    return 1.0;
}

double Fit::reducedChiSquare() const
{
    if (!std::isfinite(chi_2) || chi_2 < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const int dof = degreesOfFreedom();
    if (dof <= 0)
        return std::numeric_limits<double>::quiet_NaN();
    return chi_2 / double(dof);
}

namespace {

bool isCollinearLinearFit(const Fit *fit, const std::vector<double> &x)
{
    if (!dynamic_cast<const LinearFit *>(fit) || x.size() < 2)
        return false;
    const double meanX = std::accumulate(x.cbegin(), x.cend(), 0.0) / double(x.size());
    double sxx = 0.0;
    for (double xv : x) {
        if (!std::isfinite(xv))
            return true;
        const double dx = xv - meanX;
        sxx += dx * dx;
    }
    return !std::isfinite(sxx) || sxx <= 1e-30;
}

bool hasValidFitMetrics(const Fit *fit, const std::vector<double> &results, double chi2,
                        bool initErr, const std::vector<double> &x, const std::vector<double> &y)
{
    if (initErr || x.empty() || y.empty() || !std::isfinite(chi2) || chi2 < 0.0)
        return false;
    if (results.empty())
        return false;
    for (double v : results) {
        if (!std::isfinite(v))
            return false;
    }
    if (isCollinearLinearFit(fit, x))
        return false;
    return true;
}

} // namespace

double Fit::rss() const
{
    if (!hasValidFitMetrics(this, d_results, chi_2, d_init_err, d_x, d_y))
        return std::numeric_limits<double>::quiet_NaN();
    if (d_has_custom_rss) {
        return (std::isfinite(d_custom_rss) && d_custom_rss >= 0.0)
                ? d_custom_rss
                : std::numeric_limits<double>::quiet_NaN();
    }
    if (d_y_fit.size() == d_y.size() && !d_y.empty()) {
        const std::vector<double> w = computeWeightsVector();
        double sum = 0.0;
        size_t activeCount = 0;
        for (size_t i = 0; i < d_y.size(); ++i) {
            if (i < w.size() && !(w[i] > 0.0))
                continue;
            if (!std::isfinite(d_y[i]) || !std::isfinite(d_y_fit[i]))
                return std::numeric_limits<double>::quiet_NaN();
            double r = d_y[i] - d_y_fit[i];
            sum += r * r;
            ++activeCount;
        }
        if (activeCount == 0)
            return std::numeric_limits<double>::quiet_NaN();
        return std::isfinite(sum) ? sum : std::numeric_limits<double>::quiet_NaN();
    }
    return (std::isfinite(chi_2) && chi_2 >= 0.0)
            ? chi_2
            : std::numeric_limits<double>::quiet_NaN();
}

double Fit::rSquare() const
{
    if (d_has_custom_r2) {
        if (d_init_err || !std::isfinite(d_custom_r2))
            return std::numeric_limits<double>::quiet_NaN();
        return d_custom_r2;
    }

    // A failed or degenerate fit (e.g. collinear linear fit) has no valid R^2.
    if (!hasValidFitMetrics(this, d_results, chi_2, d_init_err, d_x, d_y))
        return std::numeric_limits<double>::quiet_NaN();

    double mean = 0.0, tss = 0.0, weights_sum = 0.0;

    if (d_y_error_source == UnknownErrors && d_weighting_method == WeightNone) {
        mean = std::accumulate(d_y.cbegin(), d_y.cend(), 0.0) / double(d_y.size());
        for (auto it = d_y.cbegin(); d_y.cend() != it; ++it)
            tss += (*it - mean) * (*it - mean);
    } else {
        const std::vector<double> w = computeWeightsVector();
        const size_t n = std::min(d_y.size(), w.size());
        for (size_t i = 0; i < n; ++i) {
            if (!(w[i] > 0.0) || !std::isfinite(w[i]))
                continue;
            mean += d_y[i] * w[i];
            weights_sum += w[i];
        }
        if (!(weights_sum > 0.0))
            return std::numeric_limits<double>::quiet_NaN();
        mean /= weights_sum;
        for (size_t i = 0; i < n; ++i) {
            if (!(w[i] > 0.0) || !std::isfinite(w[i]))
                continue;
            tss += (d_y[i] - mean) * (d_y[i] - mean) * w[i];
        }
    }
    if (tss <= 0.0)
        return 1.0;
    return 1.0 - chi_2 / tss;
}

double Fit::adjustedRSquare() const
{
    const double r2 = rSquare();
    if (!std::isfinite(r2))
        return std::numeric_limits<double>::quiet_NaN();
    const int nEff = effectivePointCount();
    const int dof = degreesOfFreedom();
    if (dof <= 0 || nEff <= 1)
        return std::numeric_limits<double>::quiet_NaN();
    return 1.0 - (1.0 - r2) * double(nEff - 1) / double(dof);
}

double Fit::pearsonR() const
{
    // Pearson's r is the signed x-y correlation of a straight-line regression. For any other
    // model "r" is ambiguous (the old corr(y, y_fit) was always >= 0 even for falling data),
    // so it is not reported.
    if (!d_straight_line_fit || !std::isfinite(chi_2) || chi_2 < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    return d_line_pearson_r;
}

QString Fit::legendInfo()
{
    const QString curveTitle =
            (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0) ? d_curve->title().text()
                                                                      : objectName();
    QString info;
    if (!converged())
        info += convergenceMessage() + "\n";
    info += tr("Dataset") + ": " + curveTitle + "\n";
    info += tr("Function") + ": " + d_formula + "\n\n";

    auto fmtNum = [this](double v) -> QString {
        return std::isfinite(v) ? QLocale().toString(v, 'g', d_prec) : QString::fromUtf8("—");
    };
    double chi_2_dof = reducedChiSquare();
    info += "Chi^2/doF = " + fmtNum(chi_2_dof) + "\n";
    info += tr("R^2") + " (COD) = " + fmtNum(rSquare()) + "\n";
    info += tr("Adj. R^2") + " = " + fmtNum(adjustedRSquare()) + "\n";

    for (unsigned i = 0; i < d_p; i++) {
        const double se = parameterStdError(int(i));
        info += d_param_names[i] + " = " + QLocale().toString(d_results[i], 'g', d_prec) + " +/- "
                + (std::isfinite(se) ? QLocale().toString(se, 'g', d_prec)
                                     : QString::fromUtf8("—"))
                + "\n";
    }
    return info;
}

void Fit::applyWeightsToErrorVector(const std::vector<double> &weightsVec)
{
    d_y_errors.resize(d_x.size(), 1.0);
    const double inf = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < d_x.size(); ++i) {
        const double wi =
                (i < weightsVec.size() && std::isfinite(weightsVec[i]) && weightsVec[i] > 0.0)
                        ? weightsVec[i]
                        : 0.0;
        if (wi > 0.0) {
            const double sig = 1.0 / std::sqrt(wi);
            d_y_errors[i] = std::isfinite(sig) ? sig : inf;
        } else {
            // Excluded point (weight == 0): infinite sigma yields zero residual and zero Jacobian row.
            d_y_errors[i] = inf;
        }
    }
}

bool Fit::readAlignedColumnValues(const QString &colName, std::vector<double> &valsOut,
                                  bool failSilently) const
{
    if (colName.isEmpty())
        return false;
    ApplicationWindow *app = dynamic_cast<ApplicationWindow *>(parent());
    Table *t = app ? app->table(colName) : nullptr;
    if (!t)
        return false;
    const int col = t->colIndex(colName);
    if (col < 0)
        return false;
    if (unsigned(t->numRows()) < d_x.size()) {
        if (!failSilently && !isNonInteractiveFit(app, d_silent))
            QMessageBox::critical(app, tr("Error"),
                                  tr("The column %1 has less points than the fitted data set. "
                                     "Please choose another column!")
                                          .arg(colName));
        return false;
    }

    Column *colPtr = t->column(col);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    valsOut.assign(d_x.size(), nan);
    DataCurve *dc = (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0)
            ? dynamic_cast<DataCurve *>(d_curve)
            : nullptr;
    for (size_t i = 0; i < d_x.size(); ++i) {
        int row = (i < d_indices.size()) ? int(d_indices[i]) : int(i);
        if (dc && dc->table() == t && i < d_indices.size()) {
            int trRow = dc->tableRow(int(d_indices[i]));
            if (trRow >= 0)
                row = trRow;
        }
        if (row < 0 || row >= t->numRows()) {
            if (!failSilently && !isNonInteractiveFit(app, d_silent))
                QMessageBox::critical(app, tr("Error"),
                                      tr("The column %1 has less points than the fitted data set. "
                                         "Please choose another column!")
                                              .arg(colName));
            return false;
        }
        if (!colPtr || colPtr->isInvalid(row))
            valsOut[i] = nan;
        else
            valsOut[i] = t->cell(row, col);
    }
    return true;
}

bool Fit::setYErrorSource(ErrorSource err, const QString &colName, bool fail_silently)
{
    try
    {
        d_y_errors.resize(d_x.size());
    }
    catch (const std::bad_alloc& e)
    {
        if (!fail_silently && !isNonInteractiveFit(parent(), d_silent))
            QMessageBox::critical((ApplicationWindow *)parent(), tr("ToriGraph") + " - " + tr("Error"),
                                  tr("Could not allocate memory, operation aborted!\n")
                                          + tr("Allocator returned: ") + e.what());
        d_init_err = true;
        return false;
    }

    auto updateExclusionNote = [this](const std::vector<double> &w, const QString &reason) {
        int excluded = 0;
        for (double wi : w) {
            if (!std::isfinite(wi) || !(wi > 0.0))
                ++excluded;
        }
        if (excluded > 0) {
            d_weighting_note = tr("Warning: %1 of %2 point(s) excluded with weight 0 (%3); "
                                  "degrees of freedom adjusted.")
                                       .arg(excluded)
                                       .arg(int(w.size()))
                                       .arg(reason);
        } else {
            d_weighting_note.clear();
        }
    };

    d_y_error_source = err;
    d_weighting_note.clear();
    switch (d_y_error_source) {
    case UnknownErrors: {
        d_y_error_dataset = QString();
        d_weighting_method = WeightNone;
        d_raw_weights.clear();
        d_raw_y_errors.clear();
        // using 1.0 here is important for correct error estimates,
        // cmp. Fit::fitGslMultifit and Fit::fitGslMultimin
        std::fill(d_y_errors.begin(), d_y_errors.end(), 1.0);
        break;
    }
    case AssociatedErrors: {
        bool error = true;
        QwtErrorPlotCurve *er = 0;
        const bool curveAlive = (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0);
        if (curveAlive) {
            if (DataCurve *dc = dynamic_cast<DataCurve *>(d_curve)) {
                QList<DataCurve *> lst = dc->errorBarsList();
                foreach (DataCurve *c, lst) {
                    er = (QwtErrorPlotCurve *)c;
                    if (!er->xErrors()) {
                        d_y_error_dataset = er->title().text();
                        error = false;
                        break;
                    }
                }
            }
        }
        if (error) {
            if (!fail_silently && !isNonInteractiveFit(parent(), d_silent))
                QMessageBox::critical((ApplicationWindow *)parent(), tr("Error"),
                                      tr("The curve %1 has no associated Y error bars.")
                                              .arg(curveAlive ? d_curve->title().text() : QString()));
            return false;
        }
        d_raw_weights.clear();
        d_raw_y_errors.assign(d_x.size(), std::numeric_limits<double>::quiet_NaN());
        if (er) {
            for (unsigned j = 0; j < d_raw_y_errors.size(); j++)
                d_raw_y_errors[j] = er->errorValue(int(d_indices[j]));
        }
        d_weighting_method = WeightInstrumental;
        const std::vector<double> w = computeWeightsVector();
        applyWeightsToErrorVector(w);
        updateExclusionNote(w, tr("non-positive or invalid associated Y error"));
        break;
    }
    case PoissonErrors: {
        const bool curveAlive = (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0);
        d_y_error_dataset = curveAlive ? d_curve->title().text() : QString();
        d_raw_weights.clear();
        d_raw_y_errors.clear();
        d_weighting_method = WeightStatistical;
        const std::vector<double> w = computeWeightsVector();
        applyWeightsToErrorVector(w);
        updateExclusionNote(w, tr("non-finite Y value"));
    } break;
    case CustomErrors: { // d_y_errors are equal to the values of the arbitrary dataset
        std::vector<double> rawErrs;
        if (!readAlignedColumnValues(colName, rawErrs, fail_silently))
            return false;

        d_y_error_dataset = colName;
        d_raw_weights.clear();
        d_raw_y_errors = std::move(rawErrs);
        d_weighting_method = WeightArbitraryDataset;
        const std::vector<double> w = computeWeightsVector();
        applyWeightsToErrorVector(w);
        updateExclusionNote(w, tr("non-positive or invalid custom Y error"));
    } break;
    }
    return true;
}

bool Fit::setWeightingMethod(WeightingMethod m, const QString &colName)
{
    auto updateExclusionNote = [this](const std::vector<double> &w, const QString &reason) {
        int excluded = 0;
        for (double wi : w) {
            if (!std::isfinite(wi) || !(wi > 0.0))
                ++excluded;
        }
        if (excluded > 0) {
            d_weighting_note = tr("Warning: %1 of %2 point(s) excluded with weight 0 (%3); "
                                  "degrees of freedom adjusted.")
                                       .arg(excluded)
                                       .arg(int(w.size()))
                                       .arg(reason);
        } else {
            d_weighting_note.clear();
        }
    };

    switch (m) {
    case WeightNone:
        if (!setYErrorSource(UnknownErrors, QString(), true))
            return false;
        d_weighting_method = WeightNone;
        d_explicit_weighting = false;
        d_weighting_col.clear();
        d_raw_weights.clear();
        d_raw_y_errors.clear();
        d_weighting_note.clear();
        return true;

    case WeightInstrumental: {
        bool ok = colName.isEmpty() ? setYErrorSource(AssociatedErrors, QString(), d_silent)
                                    : setYErrorSource(CustomErrors, colName, d_silent);
        if (ok) {
            d_weighting_method = WeightInstrumental;
            d_explicit_weighting = true;
            d_weighting_col = colName;
            d_raw_weights.clear();
        }
        return ok;
    }

    case WeightArbitraryDataset: {
        bool ok = setYErrorSource(CustomErrors, colName, d_silent);
        if (ok) {
            d_weighting_method = WeightArbitraryDataset;
            d_explicit_weighting = true;
            d_weighting_col = colName;
            d_raw_weights.clear();
        }
        return ok;
    }

    case WeightDirect: {
        std::vector<double> rawW;
        if (!readAlignedColumnValues(colName, rawW, d_silent))
            return false;
        d_raw_weights = std::move(rawW);
        d_raw_y_errors.clear();
        d_y_error_source = CustomErrors;
        d_y_error_dataset = colName;
        d_weighting_method = WeightDirect;
        d_explicit_weighting = true;
        d_weighting_col = colName;
        const std::vector<double> w = computeWeightsVector();
        applyWeightsToErrorVector(w);
        updateExclusionNote(w, tr("zero, negative, or invalid direct weight"));
        return true;
    }

    case WeightStatistical: {
        d_raw_weights.clear();
        d_raw_y_errors.clear();
        d_y_error_source = PoissonErrors;
        d_y_error_dataset = (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0)
                ? d_curve->title().text()
                : QString();
        d_weighting_method = WeightStatistical;
        d_explicit_weighting = true;
        d_weighting_col.clear();
        const std::vector<double> w = computeWeightsVector();
        applyWeightsToErrorVector(w);
        updateExclusionNote(w, tr("non-finite Y value"));
        return true;
    }

    case WeightProportional: {
        d_raw_weights.clear();
        d_raw_y_errors.clear();
        d_y_error_source = CustomErrors;
        d_y_error_dataset = (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0)
                ? d_curve->title().text()
                : QString();
        d_weighting_method = WeightProportional;
        d_explicit_weighting = true;
        d_weighting_col.clear();
        const std::vector<double> w = computeWeightsVector();
        applyWeightsToErrorVector(w);
        updateExclusionNote(w, tr("Y = 0 or non-finite Y in proportional 1/Y^2 weighting"));
        return true;
    }

    case WeightVariance: {
        std::vector<double> rawVar;
        if (!readAlignedColumnValues(colName, rawVar, d_silent))
            return false;
        d_raw_weights = std::move(rawVar);
        d_raw_y_errors.clear();
        d_y_error_source = CustomErrors;
        d_y_error_dataset = colName;
        d_weighting_method = WeightVariance;
        d_explicit_weighting = true;
        d_weighting_col = colName;
        const std::vector<double> w = computeWeightsVector();
        applyWeightsToErrorVector(w);
        updateExclusionNote(w, tr("zero, negative, or invalid variance"));
        return true;
    }
    }
    return false;
}

std::vector<double> Fit::computeWeightsVector() const
{
    const size_t n = d_x.size();
    std::vector<double> w(n, 1.0);
    if (n == 0)
        return w;

    // Never fabricate a minimum-positive fallback for <=0 or non-finite errors:
    // invalid points receive weight 0 (excluded from the objective and DOF).
    auto errorsToWeights = [&](const std::vector<double> &errs) {
        for (size_t i = 0; i < n; ++i) {
            if (i < errs.size() && std::isfinite(errs[i]) && errs[i] > 0.0 && errs[i] < 1e140) {
                const double s = errs[i];
                const double wi = 1.0 / (s * s);
                w[i] = (std::isfinite(wi) && wi > 0.0) ? wi : 0.0;
            } else {
                w[i] = 0.0;
            }
        }
    };

    if (d_in_bootstrap && d_y_errors.size() == n) {
        if (d_y_error_source == UnknownErrors && d_weighting_method == WeightNone)
            return w;
        errorsToWeights(d_y_errors);
        return w;
    }

    switch (d_weighting_method) {
    case WeightNone:
        if (d_y_error_source != UnknownErrors) {
            if (d_raw_y_errors.size() == n)
                errorsToWeights(d_raw_y_errors);
            else if (!d_y_errors.empty())
                errorsToWeights(d_y_errors);
        }
        return w;

    case WeightInstrumental:
    case WeightArbitraryDataset:
        errorsToWeights((d_raw_y_errors.size() == n) ? d_raw_y_errors : d_y_errors);
        return w;

    case WeightStatistical: {
        double maxAbsY = 0.0;
        const size_t lim = std::min(n, d_y.size());
        for (size_t i = 0; i < lim; ++i) {
            if (std::isfinite(d_y[i]))
                maxAbsY = std::max(maxAbsY, std::abs(d_y[i]));
        }
        const double floorVal = (maxAbsY > 0.0 && maxAbsY < 1.0)
                                        ? std::max(1e-12, 1e-3 * maxAbsY)
                                        : 1.0;
        for (size_t i = 0; i < n; ++i) {
            if (i < d_y.size() && std::isfinite(d_y[i]))
                w[i] = 1.0 / std::max(std::abs(d_y[i]), floorVal);
            else
                w[i] = 0.0;
        }
        return w;
    }

    case WeightProportional: {
        // w_i = 1 / y_i^2; points with y_i == 0 or non-finite y_i are excluded (w_i = 0)
        // rather than assigned an artificially tiny sigma that dominates the fit.
        for (size_t i = 0; i < n; ++i) {
            if (i < d_y.size() && std::isfinite(d_y[i]) && std::abs(d_y[i]) > 0.0) {
                const double ay = std::abs(d_y[i]);
                const double wi = 1.0 / (ay * ay);
                w[i] = (std::isfinite(wi) && wi > 0.0) ? wi : 0.0;
            } else {
                w[i] = 0.0;
            }
        }
        return w;
    }

    case WeightDirect: {
        if (d_raw_weights.size() == n) {
            for (size_t i = 0; i < n; ++i) {
                const double wi = d_raw_weights[i];
                w[i] = (std::isfinite(wi) && wi > 0.0) ? wi : 0.0;
            }
        } else {
            errorsToWeights(d_y_errors);
        }
        return w;
    }

    case WeightVariance: {
        if (d_raw_weights.size() == n) {
            for (size_t i = 0; i < n; ++i) {
                const double vi = d_raw_weights[i];
                if (std::isfinite(vi) && vi > 0.0) {
                    const double wi = 1.0 / vi;
                    w[i] = (std::isfinite(wi) && wi > 0.0) ? wi : 0.0;
                } else {
                    w[i] = 0.0;
                }
            }
        } else {
            errorsToWeights(d_y_errors);
        }
        return w;
    }
    }

    return w;
}

Table *Fit::parametersTable(const QString &tableName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    // Parameter | Value | Error | LowerCI95 | UpperCI95 | DOF. The interval uses Student's t with
    // the residual degrees of freedom (same as OriginPro's "LCL/UCL"). Undefined cells (e.g. a
    // standard error with zero degrees of freedom) are left empty rather than filled with 0.
    Table *t = app->newTable(tableName, d_p, 6);
    t->setHeader(QStringList() << QStringLiteral("Parameter") << QStringLiteral("Value")
                               << QStringLiteral("Error") << QStringLiteral("LowerCI95")
                               << QStringLiteral("UpperCI95") << QStringLiteral("DOF"));
    const QStringList paramLongNames = { tr("Parameter"), tr("Value"),     tr("Error"),
                                         tr("LowerCI95"), tr("UpperCI95"), tr("DOF") };
    t->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int c = 1; c < 6; ++c)
        t->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    for (int c = 0; c < 6; ++c)
        t->column(c)->setLongName(paramLongNames[c]);
    const int dof = degreesOfFreedom();
    for (unsigned i = 0; i < d_p; i++) {
        t->column(0)->setTextAt(i, d_param_names[i]);
        t->column(1)->setValueAt(i, d_results[i]);
        const double se = parameterStdError(int(i));
        if (std::isfinite(se))
            t->column(2)->setValueAt(i, se);
        else
            t->column(2)->setInvalid(int(i), true);
        double lo = 0.0, hi = 0.0;
        if (!isParameterFixed(int(i)) && parameterConfidenceInterval(int(i), 0.95, &lo, &hi)) {
            t->column(3)->setValueAt(i, lo);
            t->column(4)->setValueAt(i, hi);
        } else {
            t->column(3)->setInvalid(int(i), true);
            t->column(4)->setInvalid(int(i), true);
        }
        t->column(5)->setValueAt(i, std::max(0, dof));
    }

    t->column(2)->setPlotDesignation(SciDAVis::yErr);
// TODO: replace or remove this
#if 0
	for (int j=0; j<3; j++)
		t->table()->adjustColumn(j);
#endif
    t->showNormal();
    return t;
}

Matrix *Fit::covarianceMatrix(const QString &matrixName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    Matrix *m = app->newMatrix(matrixName, d_p, d_p);
    const double covScale = effectiveCovarScale();
    const bool undefined = !covar || !std::isfinite(covScale);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (unsigned i = 0; i < d_p; i++) {
        for (unsigned j = 0; j < d_p; j++) {
            if (undefined) {
                m->setCell(i, j, nan);
            } else {
                const double val = covScale * gsl_matrix_get(covar, i, j);
                if (std::isfinite(val))
                    m->setText(i, j, QLocale().toString(val, 'g', d_prec));
                else
                    m->setCell(i, j, nan);
            }
        }
    }
    m->showNormal();
    return m;
}

Table *Fit::createResidualsTable(const QString &tableName)
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (!app || d_init_err || d_x.empty() || d_y.size() != d_x.size())
        return nullptr;

    bool createdTempScript = false;
    if (!d_script && !d_formula.isEmpty() && scriptEnv) {
        d_script.reset(scriptEnv->newScript(d_formula, this, metaObject()->className()));
        createdTempScript = true;
    }

    if (d_y_fit.size() != d_x.size() && !d_results.empty()) {
        bool old_gen = d_gen_function;
        int old_pts = d_points;
        d_gen_function = false;
        d_points = int(d_x.size());
        std::vector<double> X_tmp(d_x.size()), Y_tmp(d_x.size());
        if (calculateFitCurveData(d_results, X_tmp, Y_tmp)) {
            d_y_fit = Y_tmp;
        }
        d_gen_function = old_gen;
        d_points = old_pts;
    }

    if (d_y_fit.size() != d_x.size()) {
        if (createdTempScript)
            d_script.reset();
        return nullptr;
    }

    const int rows = int(d_x.size());
    const std::vector<double> weightsVec = computeWeightsVector();
    std::vector<double> sqrtW(size_t(rows), 0.0);
    int activeRows = 0;
    for (int i = 0; i < rows; ++i) {
        const double wi = (size_t(i) < weightsVec.size() && std::isfinite(weightsVec[size_t(i)])
                           && weightsVec[size_t(i)] > 0.0)
                                  ? weightsVec[size_t(i)]
                                  : 0.0;
        if (wi > 0.0) {
            sqrtW[size_t(i)] = std::sqrt(wi);
            ++activeRows;
        }
    }

    // Compute hat-matrix diagonal Leverage h_ii, Standardized Residuals, and Cook's Distance
    std::vector<double> leverage(size_t(rows), 0.0);
    int p_eff = std::max(1, effectiveFreeParameters());

    if (d_p > 0 && d_results.size() >= d_p) {
        std::vector<bool> fixedMask(d_p, false);
        std::vector<bool> boundedMask(d_p, false);
        std::vector<double> onesSlope(d_p, 1.0);
        std::vector<double> zeroOffset(d_p, 0.0);
        for (unsigned j = 0; j < d_p; ++j) {
            fixedMask[j] = isParameterFixed(int(j));
            boundedMask[j] = hasParameterBounds(int(j));
        }
        CompiledLinearConstraints lcUser = compileLinearConstraintsForFit(
                d_linear_constraint_expr, d_p, d_param_names, fixedMask, boundedMask, d_results,
                onesSlope, zeroOffset);

        std::vector<int> freeCols;
        for (unsigned j = 0; j < d_p; ++j) {
            if (!fixedMask[j] && !(lcUser.valid && lcUser.isDependent(int(j))))
                freeCols.push_back(int(j));
        }

        const size_t pFree = freeCols.size();
        if (pFree > 0) {
            p_eff = int(pFree);
            bool old_gen = d_gen_function;
            int old_pts = d_points;
            d_gen_function = false;
            d_points = rows;

            gsl_matrix *Jw = gsl_matrix_calloc(static_cast<size_t>(rows), pFree);
            std::vector<double> X_tmp(static_cast<size_t>(rows)),
                    Y_plus(static_cast<size_t>(rows)),
                    Y_minus(static_cast<size_t>(rows));
            bool jacOk = true;
            for (size_t k = 0; k < pFree; ++k) {
                const int j = freeCols[k];
                const double pj = d_results[size_t(j)];
                const double h = std::max(1e-12, 1e-5 * std::abs(pj));
                std::vector<double> pPlus = d_results;
                std::vector<double> pMinus = d_results;
                pPlus[size_t(j)] = pj + h;
                pMinus[size_t(j)] = pj - h;
                if (lcUser.valid && !lcUser.dep_indices.empty()) {
                    for (size_t m = 0; m < lcUser.dep_indices.size(); ++m) {
                        double vp = lcUser.dep_const[m];
                        double vm = lcUser.dep_const[m];
                        for (int fj : freeCols) {
                            vp += lcUser.dep_full_coeffs[m][size_t(fj)] * pPlus[size_t(fj)];
                            vm += lcUser.dep_full_coeffs[m][size_t(fj)] * pMinus[size_t(fj)];
                        }
                        pPlus[size_t(lcUser.dep_indices[m])] = vp;
                        pMinus[size_t(lcUser.dep_indices[m])] = vm;
                    }
                }
                if (calculateFitCurveData(pPlus, X_tmp, Y_plus)
                    && calculateFitCurveData(pMinus, X_tmp, Y_minus)) {
                    for (int i = 0; i < rows; ++i) {
                        double dy_dp = (Y_plus[size_t(i)] - Y_minus[size_t(i)]) / (2.0 * h);
                        const double sw = sqrtW[size_t(i)];
                        gsl_matrix_set(Jw, size_t(i), k,
                                       (sw > 0.0 && std::isfinite(dy_dp)) ? (dy_dp * sw) : 0.0);
                    }
                } else {
                    jacOk = false;
                }
            }
            d_gen_function = old_gen;
            d_points = old_pts;

            if (jacOk) {
                gsl_matrix *invJTJ = gsl_matrix_calloc(pFree, pFree);
                gsl_multifit_covar(Jw, 0.0, invJTJ);
                for (int i = 0; i < rows; ++i) {
                    double hii = 0.0;
                    for (size_t a = 0; a < pFree; ++a) {
                        const double ja = gsl_matrix_get(Jw, size_t(i), a);
                        if (ja == 0.0)
                            continue;
                        for (size_t b = 0; b < pFree; ++b) {
                            hii += ja * gsl_matrix_get(invJTJ, a, b)
                                   * gsl_matrix_get(Jw, size_t(i), b);
                        }
                    }
                    if (std::isfinite(hii))
                        leverage[size_t(i)] = std::max(0.0, std::min(0.999999, hii));
                }
                gsl_matrix_free(invJTJ);
            }
            gsl_matrix_free(Jw);
        }
    }

    if (createdTempScript)
        d_script.reset();

    double weightedSse = 0.0;
    for (int i = 0; i < rows; ++i) {
        if (!(sqrtW[size_t(i)] > 0.0))
            continue;
        double rw = (d_y[size_t(i)] - d_y_fit[size_t(i)]) * sqrtW[size_t(i)];
        weightedSse += rw * rw;
    }
    const int dofInt = activeRows - p_eff;
    const bool validDof = (dofInt > 0);
    const double s2_w = validDof ? (weightedSse / double(dofInt))
                                 : std::numeric_limits<double>::quiet_NaN();

    QString name = tableName.isEmpty() ? app->generateUniqueName(tr("Residuals"))
                                       : app->generateUniqueName(tableName, false);
    Table *t = app->newTable(name, rows, 7);
    if (!t)
        return nullptr;

    t->setHeader(QStringList() << QStringLiteral("X") << QStringLiteral("Y_Obs")
                               << QStringLiteral("Y_Fit") << QStringLiteral("Residual")
                               << QStringLiteral("Standardized_Residual")
                               << QStringLiteral("Leverage_hii")
                               << QStringLiteral("Cooks_Distance"));
    const QStringList residLongNames = {
        tr("X"),
        tr("Y_Obs"),
        tr("Y_Fit"),
        tr("Residual"),
        tr("Standardized_Residual"),
        tr("Leverage_hii"),
        tr("Cooks_Distance")
    };
    for (int col = 0; col < 7; ++col) {
        t->column(col)->setColumnMode(SciDAVis::ColumnMode::Numeric);
        t->column(col)->setLongName(residLongNames[col]);
    }
    t->column(0)->setPlotDesignation(SciDAVis::X);
    for (int col = 1; col < 7; ++col)
        t->column(col)->setPlotDesignation(SciDAVis::Y);

    for (int i = 0; i < rows; ++i) {
        const double rawRes = d_y[size_t(i)] - d_y_fit[size_t(i)];
        const double sw = sqrtW[size_t(i)];
        const double hii = leverage[size_t(i)];
        t->column(0)->setValueAt(i, d_x[size_t(i)]);
        t->column(1)->setValueAt(i, d_y[size_t(i)]);
        t->column(2)->setValueAt(i, d_y_fit[size_t(i)]);
        t->column(3)->setValueAt(i, rawRes);
        t->column(5)->setValueAt(i, hii);
        if (!validDof || !(sw > 0.0)) {
            t->column(4)->setInvalid(i, true);
            t->column(6)->setInvalid(i, true);
        } else {
            const double wRes = rawRes * sw;
            const double oneMinusH = std::max(1e-6, 1.0 - hii);
            double stdRes = 0.0;
            double cooksD = 0.0;
            if (std::isfinite(s2_w) && s2_w > 1e-300) {
                stdRes = wRes / std::sqrt(s2_w * oneMinusH);
                cooksD = (stdRes * stdRes / double(std::max(1, p_eff))) * (hii / oneMinusH);
            }
            if (std::isfinite(stdRes))
                t->column(4)->setValueAt(i, stdRes);
            else
                t->column(4)->setInvalid(i, true);
            if (std::isfinite(cooksD))
                t->column(6)->setValueAt(i, cooksD);
            else
                t->column(6)->setInvalid(i, true);
        }
    }

    if (!d_silent)
        t->showNormal();
    return t;
}

const vector<double> &Fit::errors()
{
    if (d_result_errors.empty()) {
        d_result_errors.resize(d_p, 0.0);
        if (covar) {
            const double covScale = effectiveCovarScale();
            // Residual-scaled errors are undefined without residual degrees of freedom
            const bool undefined = !std::isfinite(covScale)
                    || ((d_y_error_source == UnknownErrors || d_scale_errors)
                        && degreesOfFreedom() <= 0);
            for (unsigned i = 0; i < d_p; i++) {
                if (isParameterFixed(int(i)))
                    d_result_errors[i] = 0.0;
                else if (undefined)
                    d_result_errors[i] = std::numeric_limits<double>::quiet_NaN();
                else {
                    const double v = covScale * gsl_matrix_get(covar, i, i);
                    d_result_errors[i] = std::isfinite(v)
                            ? std::sqrt(std::max(0.0, v))
                            : std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
    }
    return d_result_errors;
}

void Fit::fit()
{
    if (!d_graph || d_init_err)
        return;
    if (d_curve && d_graph->curveIndex(d_curve) < 0) {
        d_curve = nullptr;
        d_init_err = true;
        return;
    }

    if (d_x.empty()) {
        if (!isNonInteractiveFit(parent(), d_silent))
            QMessageBox::critical((ApplicationWindow *)parent(), tr("Fit Error"),
                                  tr("You didn't specify a valid data set for this fit operation. "
                                     "Operation aborted!"));
        return;
    }
    if (!d_p) {
        if (!isNonInteractiveFit(parent(), d_silent))
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("There are no parameters specified for this fit operation. Operation aborted!"));
        return;
    }
    d_has_custom_dof = false;
    d_has_custom_rss = false;
    const int minNeeded = std::max(1, effectiveFreeParameters());
    const int nActive = effectivePointCount();
    if (minNeeded > nActive) {
        if (!isNonInteractiveFit(parent(), d_silent))
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You need at least %1 valid data points (with positive weight) for this fit operation. "
                       "Operation aborted!")
                            .arg(minNeeded));
        return;
    }
    if (d_formula.isEmpty()) {
        if (!isNonInteractiveFit(parent(), d_silent))
            QMessageBox::critical(
                    (ApplicationWindow *)parent(), tr("Fit Error"),
                    tr("You must specify a valid fit function first. Operation aborted!"));
        return;
    }

    d_has_custom_r2 = false;
    d_straight_line_fit = false;
    d_convergence = Converged;
    d_last_iterations = 0;
    d_fit_notes.clear();
    chi_2 = -1.0;
    d_y_fit.clear();
    d_covar_scaled = false;
    if (covar)
        gsl_matrix_set_zero(covar);
    if (!d_all_guesses_set && unsigned(d_explicit_init_vals.size()) < d_p) {
        guessInitialValues();
        applyExplicitInitialGuesses();
    } else if (!d_initial_guesses_set) {
        guessInitialValues();
    } else {
        applyExplicitInitialGuesses();
    }

    d_result_errors.clear();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    int status = GSL_EINVAL, iterations = 0;
    vector<double> par;
    d_script.reset(scriptEnv->newScript(d_formula, this, metaObject()->className()));
    connect(d_script.get(), SIGNAL(error(const QString &, const QString &, int)), this,
            SLOT(scriptError(const QString &, const QString &, int)));

    if (d_solver == NelderMeadSimplex)
        par = fitGslMultimin(iterations, status);
    else
        par = fitGslMultifit(iterations, status);

    if (status != GSL_SUCCESS || !std::isfinite(chi_2) || chi_2 < 0.0) {
        chi_2 = -1.0;
        d_y_fit.clear();
        if (status == GSL_SUCCESS)
            status = GSL_EINVAL;
        par.assign(d_p, std::numeric_limits<double>::quiet_NaN());
    }

    storeCustomFitResults(par);
    if (status == GSL_SUCCESS && chi_2 >= 0.0 && !d_suppress_curve_output && d_graph) {
        generateFitCurve(par);
        if ((d_conf_band || d_pred_band) && d_graph)
            addConfidenceAndPredictionBands(par);
    }

    ApplicationWindow *app = qobject_cast<ApplicationWindow *>(parent());
    if (app && app->writeFitResultsToLog && !d_silent && d_graph)
        app->updateLog(logFitInfo(d_results, iterations, status, d_graph->parentPlotName()));
    disconnect(d_script.get(), SIGNAL(error(const QString &, const QString &, int)), this,
               SLOT(scriptError(const QString &, const QString &, int)));
    QApplication::restoreOverrideCursor();
}

void Fit::scriptError(const QString &message, const QString &script_name, int line_number)
{
    if (!isNonInteractiveFit(parent(), d_silent))
        QMessageBox::critical(qobject_cast<QWidget *>(parent()), tr("Input function error"),
                              QString("%1:%2\n").arg(script_name).arg(line_number) + message);
}

int Fit::evaluate_f(const gsl_vector *x, gsl_vector *f)
{
    for (unsigned i = 0; i < d_p; i++) {
        d_script->setDouble(gsl_vector_get(x, i), d_param_names[i].toUtf8());
    }
    for (unsigned j = 0; j < d_x.size(); j++) {
        if (j >= d_y_errors.size() || !std::isfinite(d_y_errors[j]) || !(d_y_errors[j] > 0.0)) {
            gsl_vector_set(f, j, 0.0);
            continue;
        }
        if (!d_script->setDouble(d_x[j], "x"))
            return GSL_EINVAL;
        bool success;
        auto y = d_script->eval();
        if (y.isNull())
            return GSL_EINVAL;
        double yVal = y.toDouble(&success);
        if (!success || !std::isfinite(yVal))
            return GSL_EINVAL;
        gsl_vector_set(f, j, (yVal - d_y[j]) / d_y_errors[j]);
    }
    return GSL_SUCCESS;
}

double Fit::evaluate_d(const gsl_vector *x)
{
    double result = 0.0;
    for (unsigned i = 0; i < d_p; i++)
        d_script->setDouble(gsl_vector_get(x, i), d_param_names[i].toUtf8());
    for (unsigned j = 0; j < d_x.size(); j++) {
        if (j >= d_y_errors.size() || !std::isfinite(d_y_errors[j]) || !(d_y_errors[j] > 0.0))
            continue;
        if (!d_script->setDouble(d_x[j], "x"))
            return std::numeric_limits<double>::quiet_NaN();
        bool success = false;
        auto y = d_script->eval();
        if (y.isNull())
            return std::numeric_limits<double>::quiet_NaN();
        double yVal = y.toDouble(&success);
        if (!success || !std::isfinite(yVal))
            return std::numeric_limits<double>::quiet_NaN();
        double r = (yVal - d_y[j]) / d_y_errors[j];
        result += r * r;
    }
    return result;
}

typedef struct
{
    Script *script;
    QString param;
    bool success;
} DiffData;

double Fit::evaluate_df_helper(double x, void *params)
{
    DiffData *data = static_cast<DiffData *>(params);
    if (!data->script->setDouble(x, (data->param).toUtf8())) {
        data->script->disconnect(SIGNAL(error(const QString &, const QString &, int)));
        data->success = false;
        return std::numeric_limits<double>::quiet_NaN();
    }
    bool success = false;
    QVariant v = data->script->eval();
    if (v.isNull()) {
        data->script->disconnect(SIGNAL(error(const QString &, const QString &, int)));
        data->success = false;
        return std::numeric_limits<double>::quiet_NaN();
    }
    double result = v.toDouble(&success);
    if (!success || !std::isfinite(result)) {
        data->script->disconnect(SIGNAL(error(const QString &, const QString &, int)));
        data->success = false;
        return std::numeric_limits<double>::quiet_NaN();
    }
    return result;
}

int Fit::evaluate_df(const gsl_vector *x, gsl_matrix *J)
{
    double result, abserr;
    gsl_function F;
    F.function = &evaluate_df_helper;
    DiffData data;
    F.params = &data;
    data.script = d_script.get();
    data.success = true;
    for (unsigned i = 0; i < d_p; i++)
        d_script->setDouble(gsl_vector_get(x, i), d_param_names[i].toUtf8());
    for (unsigned i = 0; i < d_x.size(); i++) {
        if (i >= d_y_errors.size() || !std::isfinite(d_y_errors[i]) || !(d_y_errors[i] > 0.0)) {
            for (unsigned j = 0; j < d_p; j++)
                gsl_matrix_set(J, i, j, 0.0);
            continue;
        }
        if (!d_script->setDouble(d_x[i], "x"))
            return GSL_EINVAL;
        double s = d_y_errors[i];
        for (unsigned j = 0; j < d_p; j++) {
            if (isParameterFixed(int(j))) {
                gsl_matrix_set(J, i, j, 0.0);
                continue;
            }
            double xj = gsl_vector_get(x, j);
            double h = std::max(1e-8, 1e-6 * std::abs(xj));
            data.param = d_param_names[j];
            data.success = true;
            gsl_deriv_central(&F, xj, h, &result, &abserr);
            if (!data.success || !std::isfinite(result)) {
                data.success = true;
                const double f0 = evaluate_df_helper(xj, &data);
                if (data.success && std::isfinite(f0)) {
                    data.success = true;
                    const double fPlus = evaluate_df_helper(xj + h, &data);
                    if (data.success && std::isfinite(fPlus)) {
                        result = (fPlus - f0) / h;
                    } else {
                        data.success = true;
                        const double fMinus = evaluate_df_helper(xj - h, &data);
                        if (data.success && std::isfinite(fMinus))
                            result = (f0 - fMinus) / h;
                    }
                }
            }
            d_script->setDouble(xj, d_param_names[j].toUtf8());
            if (!data.success || !std::isfinite(result))
                return GSL_EDOM;
            gsl_matrix_set(J, i, j, result / s);
        }
    }
    return GSL_SUCCESS;
}

void Fit::generateFitCurve(const vector<double> &par)
{
    if (d_suppress_curve_output)
        return;

    if (!hasValidFitMetrics(this, par, chi_2, d_init_err, d_x, d_y))
        return;

    if (!d_gen_function)
        d_points = d_x.size();

    std::vector<double> X(d_points);
    std::vector<double> Y(d_points);

    if (!calculateFitCurveData(par, X, Y)) {
        if (!isNonInteractiveFit(parent(), d_silent))
            QMessageBox::critical(0, tr("Fit failed"), tr("An error occurred during fit!"));
        return;
    }

    MultiLayer *ml = d_graph ? d_graph->multiLayerParent() : nullptr;
    const QString beforeSnap = ml ? ml->captureUndoSnapshot() : QString();

    if (d_gen_function) {
        QString baseName = objectName().trimmed();
        while (!baseName.isEmpty() && baseName.back().isDigit())
            baseName.chop(1);
        baseName = baseName.trimmed();
        if (baseName.endsWith(QStringLiteral("実行")))
            baseName.chop(2);
        baseName = baseName.trimmed();
        if (baseName.endsWith(QLatin1String("Fit")) && baseName.size() > 3
            && baseName.at(baseName.size() - 4).isLower()) {
            baseName.insert(baseName.size() - 3, QLatin1Char(' '));
        }
        if (baseName.isEmpty()) {
            baseName = tr("Fit").trimmed();
            if (baseName.endsWith(QStringLiteral("実行")))
                baseName.chop(2);
            baseName = baseName.trimmed();
            if (baseName.isEmpty())
                baseName = QStringLiteral("Fit");
        } else if (!baseName.contains(QLatin1String("Fit"), Qt::CaseInsensitive)
                   && !baseName.contains(QStringLiteral("フィット"))
                   && !baseName.contains(QStringLiteral("フィッティング"))) {
            QString fitSuffix = tr("Fit").trimmed();
            if (fitSuffix.endsWith(QStringLiteral("実行")))
                fitSuffix.chop(2);
            fitSuffix = fitSuffix.trimmed();
            if (fitSuffix.isEmpty())
                fitSuffix = QStringLiteral("Fit");
            const bool cjkJoin = (!baseName.isEmpty() && baseName.back().unicode() > 0x007F)
                    || (!fitSuffix.isEmpty() && fitSuffix.at(0).unicode() > 0x007F);
            baseName += cjkJoin ? fitSuffix : (QLatin1Char(' ') + fitSuffix);
        }
        insertFitFunctionCurve(baseName, X, Y);
        d_graph->replot();
    } else {
        QwtPlotCurve *rc = addResultCurve(X, Y);
        const int axesLw =
                (d_graph && d_graph->plotWidget()) ? d_graph->plotWidget()->axesLinewidth() : 1;
        if (rc && rc->pen().width() <= 1 && axesLw > 1) {
            QPen p = rc->pen();
            p.setWidth(qMax(1, axesLw));
            rc->setPen(p);
        }
        d_graph->addFitCurve(rc);
        if (d_graph)
            d_graph->replot();
    }
    if (ml && !beforeSnap.isEmpty())
        ml->pushUndoSnapshot(tr("Fit Curve"), beforeSnap);
}

void Fit::insertFitFunctionCurve(const QString &name, std::vector<double> &x,
                                 std::vector<double> &y, int penWidth)
{
    QString cleanBase = name.trimmed();
    cleanBase.replace(QStringLiteral("フィット実行"), QStringLiteral("フィット"));
    QString title = cleanBase;
    if (d_graph && (title.isEmpty() || d_graph->curvesList().contains(title))) {
        title = d_graph->generateFunctionName(
                cleanBase.isEmpty() ? QStringLiteral("Fit") : (cleanBase + QLatin1Char(' ')));
    }
    const int axesLw =
            (d_graph && d_graph->plotWidget()) ? d_graph->plotWidget()->axesLinewidth() : 1;
    const int effWidth = (penWidth <= 1) ? qMax(penWidth, axesLw) : penWidth;
    FunctionCurve *c =
            new FunctionCurve((ApplicationWindow *)parent(), FunctionCurve::Normal, title);
    c->setPen(QPen(d_curveColor, effWidth));
    // "Set data by copying x- and y-values from specified memory blocks."
    c->setData(x.data(), y.data(), d_points);
    c->setRange(d_x.front(), d_x.back());

    QString formula;
    for (unsigned j = 0; j < d_p; j++)
        formula += QString("%1=%2\n").arg(d_param_names[j]).arg(d_results[j], 0, 'g', d_prec);
    formula += "\n";
    formula += d_formula;
    c->setFormula(formula);
    d_graph->insertPlotItem(c, Graph::Line);
    d_graph->addFitCurve(c);
}

Fit::~Fit()
{
    if (!d_p)
        return;

    if (is_non_linear)
        gsl_vector_free(d_param_init);

    gsl_matrix_free(covar);
}

void Fit::generateX(std::vector<double> &X) const
{
    if (d_gen_function) {
        double X0 = d_x.front();
        double step = (d_points > 1) ? ((d_x.back() - X0) / (d_points - 1)) : 0.0;
        for (int i = 0; i < d_points; i++)
            X[i] = X0 + i * step;
    } else
        for (int i = 0; i < d_points; i++)
            X[i] = d_x[i];
}

void Fit::ensureConstraintCapacity(int index)
{
    if (index < 0)
        return;
    size_t req = std::max<size_t>(size_t(d_p), size_t(index + 1));
    if (d_param_fixed.size() < req)
        d_param_fixed.resize(req, false);
    if (d_param_bounded.size() < req)
        d_param_bounded.resize(req, false);
    if (d_param_min.size() < req)
        d_param_min.resize(req, -std::numeric_limits<double>::infinity());
    if (d_param_max.size() < req)
        d_param_max.resize(req, std::numeric_limits<double>::infinity());
}

void Fit::applyExplicitInitialGuesses()
{
    if (!d_param_init)
        return;
    for (auto it = d_explicit_init_vals.constBegin(); it != d_explicit_init_vals.constEnd(); ++it) {
        int idx = it.key();
        if (idx >= 0 && unsigned(idx) < d_p)
            gsl_vector_set(d_param_init, idx, toInternalParameter(idx, it.value()));
    }
}

void Fit::setParameterFixed(int index, bool fixed)
{
    if (index < 0)
        return;
    ensureConstraintCapacity(index);
    d_param_fixed[size_t(index)] = fixed;
}

bool Fit::isParameterFixed(int index) const
{
    if (index < 0 || size_t(index) >= d_param_fixed.size())
        return false;
    return d_param_fixed[size_t(index)];
}

void Fit::setParameterBounds(int index, double minVal, double maxVal)
{
    if (index < 0)
        return;
    ensureConstraintCapacity(index);
    if (minVal > maxVal)
        std::swap(minVal, maxVal);
    d_param_bounded[size_t(index)] = true;
    d_param_min[size_t(index)] = minVal;
    d_param_max[size_t(index)] = maxVal;
}

void Fit::clearParameterBounds(int index)
{
    if (index < 0 || size_t(index) >= d_param_bounded.size())
        return;
    d_param_bounded[size_t(index)] = false;
    d_param_min[size_t(index)] = -std::numeric_limits<double>::infinity();
    d_param_max[size_t(index)] = std::numeric_limits<double>::infinity();
}

bool Fit::hasParameterBounds(int index, double *minOut, double *maxOut) const
{
    if (index < 0 || size_t(index) >= d_param_bounded.size() || !d_param_bounded[size_t(index)])
        return false;
    if (minOut)
        *minOut = d_param_min[size_t(index)];
    if (maxOut)
        *maxOut = d_param_max[size_t(index)];
    return true;
}

bool Fit::setLinearConstraintExpr(const QString &exprList)
{
    QString trimmed = exprList.trimmed();
    if (trimmed.isEmpty()) {
        d_linear_constraint_expr.clear();
        return true;
    }

    std::vector<ParsedLinearEquation> parsed;
    if (!parseLinearConstraintList(trimmed, parsed))
        return false;

    if (d_p > 0 && !d_param_names.isEmpty()) {
        std::vector<bool> fixedMask(d_p, false);
        std::vector<bool> boundedMask(d_p, false);
        std::vector<double> baseVals(d_p, 0.0);
        std::vector<double> slopeToUser(d_p, 1.0);
        std::vector<double> offsetToUser(d_p, 0.0);
        for (unsigned j = 0; j < d_p; ++j) {
            fixedMask[j] = isParameterFixed(int(j));
            boundedMask[j] = hasParameterBounds(int(j));
            if (d_param_init)
                baseVals[j] = gsl_vector_get(d_param_init, j);
            double u0 = fromInternalParameter(int(j), 0.0);
            double u1 = fromInternalParameter(int(j), 1.0);
            slopeToUser[j] = (std::abs(u1 - u0) > 1e-15) ? (u1 - u0) : 1.0;
            offsetToUser[j] = u0;
        }
        CompiledLinearConstraints lc = compileLinearConstraintsForFit(
                trimmed, d_p, d_param_names, fixedMask, boundedMask, baseVals, slopeToUser,
                offsetToUser);
        if (!lc.valid)
            return false;
    }

    d_linear_constraint_expr = trimmed;
    return true;
}

int Fit::effectivePointCount() const
{
    if (d_has_custom_dof)
        return std::max(0, d_custom_n_points);
    if (d_x.empty())
        return 0;
    if (d_y_error_source == UnknownErrors && d_weighting_method == WeightNone)
        return int(d_x.size());
    const std::vector<double> w = computeWeightsVector();
    int active = 0;
    for (double wi : w) {
        if (std::isfinite(wi) && wi > 0.0)
            ++active;
    }
    return active;
}

int Fit::effectiveFreeParameters() const
{
    if (d_has_custom_dof)
        return std::max(0, d_custom_p_free);
    int count = 0;
    std::vector<bool> fixedMask(d_p, false);
    std::vector<bool> boundedMask(d_p, false);
    std::vector<double> baseVals(d_p, 0.0);
    std::vector<double> slopeToUser(d_p, 1.0);
    std::vector<double> offsetToUser(d_p, 0.0);

    for (unsigned j = 0; j < d_p; ++j) {
        bool fixed = isParameterFixed(int(j));
        fixedMask[j] = fixed;
        boundedMask[j] = hasParameterBounds(int(j));
        if (d_param_init)
            baseVals[j] = gsl_vector_get(d_param_init, j);
        double u0 = fromInternalParameter(int(j), 0.0);
        double u1 = fromInternalParameter(int(j), 1.0);
        slopeToUser[j] = (std::abs(u1 - u0) > 1e-15) ? (u1 - u0) : 1.0;
        offsetToUser[j] = u0;
        if (!fixed)
            ++count;
    }

    if (!d_linear_constraint_expr.trimmed().isEmpty() && d_p > 0) {
        CompiledLinearConstraints lc = compileLinearConstraintsForFit(
                d_linear_constraint_expr, d_p, d_param_names, fixedMask, boundedMask, baseVals,
                slopeToUser, offsetToUser);
        if (lc.valid)
            count = std::max(0, count - int(lc.dep_indices.size()));
    }
    return count;
}

bool Fit::hasActiveConstraints() const
{
    if (!d_linear_constraint_expr.trimmed().isEmpty())
        return true;
    for (unsigned j = 0; j < d_p; ++j) {
        if (isParameterFixed(int(j)) || hasParameterBounds(int(j)))
            return true;
    }
    return false;
}

bool Fit::evaluatePreviewCurve(std::vector<double> &X, std::vector<double> &Y, int nPoints)
{
    if (d_init_err || d_x.size() < 2 || d_p == 0)
        return false;

    const int pts = std::max(2, nPoints);
    bool createdScript = false;
    if (!d_script && !d_formula.isEmpty() && scriptEnv) {
        d_script.reset(scriptEnv->newScript(d_formula, this, metaObject()->className()));
        createdScript = true;
    }

    std::vector<double> userGuess(d_p, 0.0);
    for (unsigned j = 0; j < d_p; ++j)
        userGuess[j] = initialGuess(int(j));

    if (!d_linear_constraint_expr.trimmed().isEmpty()) {
        std::vector<bool> fixedMask(d_p, false);
        std::vector<bool> boundedMask(d_p, false);
        std::vector<double> onesSlope(d_p, 1.0);
        std::vector<double> zeroOffset(d_p, 0.0);
        for (unsigned j = 0; j < d_p; ++j) {
            fixedMask[j] = isParameterFixed(int(j));
            boundedMask[j] = hasParameterBounds(int(j));
        }
        CompiledLinearConstraints lc = compileLinearConstraintsForFit(
                d_linear_constraint_expr, d_p, d_param_names, fixedMask, boundedMask, userGuess,
                onesSlope, zeroOffset);
        if (lc.valid && !lc.dep_indices.empty()) {
            for (size_t m = 0; m < lc.dep_indices.size(); ++m) {
                double val = lc.dep_const[m];
                for (unsigned j = 0; j < d_p; ++j) {
                    if (!fixedMask[j] && !lc.isDependent(int(j)))
                        val += lc.dep_full_coeffs[m][j] * userGuess[j];
                }
                userGuess[size_t(lc.dep_indices[m])] = val;
            }
        }
    }

    const bool oldGen = d_gen_function;
    const int oldPoints = d_points;
    d_gen_function = true;
    d_points = pts;
    X.resize(size_t(pts));
    Y.resize(size_t(pts));
    const bool ok = calculateFitCurveData(userGuess, X, Y);
    d_gen_function = oldGen;
    d_points = oldPoints;

    if (createdScript)
        d_script.reset();

    if (!ok)
        return false;
    for (double yv : Y) {
        if (!std::isfinite(yv))
            return false;
    }
    return true;
}

Fit::BootstrapResult Fit::runBootstrapCI(int nSamples, double confLevel, int mode)
{
    BootstrapResult out;
    if (d_init_err || d_x.empty() || d_y.size() != d_x.size() || d_p == 0 || nSamples < 2)
        return out;

    const double cl = std::max(0.50, std::min(0.9999, confLevel));
    const bool origSilent = d_silent;
    const bool origSuppress = d_suppress_curve_output;

    // Ensure baseline fit and d_y_fit exist
    if (d_results.size() < d_p) {
        d_silent = true;
        d_suppress_curve_output = true;
        fit();
        d_silent = origSilent;
        d_suppress_curve_output = origSuppress;
    }
    if (d_results.size() < d_p)
        return out;

    if (d_y_fit.size() != d_x.size()) {
        bool createdTmp = false;
        if (!d_script && !d_formula.isEmpty() && scriptEnv) {
            d_script.reset(scriptEnv->newScript(d_formula, this, metaObject()->className()));
            createdTmp = true;
        }
        bool oldGen = d_gen_function;
        int oldPts = d_points;
        d_gen_function = false;
        d_points = int(d_x.size());
        std::vector<double> X_tmp(d_x.size()), Y_tmp(d_x.size());
        if (calculateFitCurveData(d_results, X_tmp, Y_tmp))
            d_y_fit = Y_tmp;
        d_gen_function = oldGen;
        d_points = oldPts;
        if (createdTmp)
            d_script.reset();
    }
    if (d_y_fit.size() != d_x.size()) {
        d_silent = true;
        d_suppress_curve_output = true;
        fit();
        d_silent = origSilent;
        d_suppress_curve_output = origSuppress;
        if (d_y_fit.size() != d_x.size())
            return out;
    }

    // Snapshot full fitter state so caller sees baseline fit untouched
    const std::vector<double> savedY = d_y;
    const std::vector<double> savedYFit = d_y_fit;
    const std::vector<double> savedYErrors = d_y_errors;
    const std::vector<double> savedRawYErrors = d_raw_y_errors;
    const QString savedWeightingNote = d_weighting_note;
    const std::vector<double> savedResults = d_results;
    const std::vector<double> savedResultErrors = d_result_errors;
    const double savedChi2 = chi_2;
    const int savedMaxIter = d_max_iterations;
    const ConvergenceState savedConvergence = d_convergence;
    const int savedIterations = d_last_iterations;
    const QStringList savedNotes = d_fit_notes;
    const bool savedStraightLine = d_straight_line_fit;
    const double savedLineR = d_line_pearson_r;
    const bool savedCovarScaled = d_covar_scaled;
    const bool savedHasCustomDof = d_has_custom_dof;
    const int savedCustomN = d_custom_n_points;
    const int savedCustomP = d_custom_p_free;
    const bool savedHasCustomRss = d_has_custom_rss;
    const double savedCustomRss = d_custom_rss;

    std::vector<double> savedParamInit(d_p, 0.0);
    if (d_param_init) {
        for (unsigned j = 0; j < d_p; ++j)
            savedParamInit[j] = gsl_vector_get(d_param_init, j);
    }
    std::vector<double> savedCovar(size_t(d_p) * size_t(d_p), 0.0);
    if (covar) {
        for (unsigned r = 0; r < d_p; ++r)
            for (unsigned c = 0; c < d_p; ++c)
                savedCovar[size_t(r) * size_t(d_p) + size_t(c)] = gsl_matrix_get(covar, r, c);
    }

    const size_t n = d_x.size();
    const std::vector<double> baseWeights = computeWeightsVector();
    std::vector<size_t> activeIdx;
    activeIdx.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (i < baseWeights.size() && std::isfinite(baseWeights[i]) && baseWeights[i] > 0.0)
            activeIdx.push_back(i);
    }
    if (activeIdx.empty())
        return out;

    const int pEff = std::max(0, effectiveFreeParameters());
    const double dofScale =
            std::sqrt(double(activeIdx.size()) / std::max(1.0, double(int(activeIdx.size()) - pEff)));

    std::vector<double> sigmaVec(n, std::numeric_limits<double>::infinity());
    std::vector<double> activeNormRes;
    activeNormRes.reserve(activeIdx.size());
    double meanNormRes = 0.0;
    for (size_t idx : activeIdx) {
        sigmaVec[idx] = 1.0 / std::sqrt(baseWeights[idx]);
        const double nr = (savedY[idx] - savedYFit[idx]) / sigmaVec[idx];
        activeNormRes.push_back(nr);
        meanNormRes += nr;
    }
    meanNormRes /= double(activeIdx.size());
    std::vector<double> centeredScaledNormRes(activeIdx.size(), 0.0);
    for (size_t k = 0; k < activeIdx.size(); ++k)
        centeredScaledNormRes[k] = dofScale * (activeNormRes[k] - meanNormRes);

    bool createdBootScript = false;
    if (is_non_linear && !d_script && !d_formula.isEmpty() && scriptEnv) {
        d_script.reset(scriptEnv->newScript(d_formula, this, metaObject()->className()));
        createdBootScript = true;
    }

    std::vector<double> warmInternal(d_p, 0.0);
    for (unsigned j = 0; j < d_p; ++j)
        warmInternal[j] = toInternalParameter(int(j), savedResults[j]);

    std::vector<std::vector<double>> replicates(d_p);
    for (unsigned j = 0; j < d_p; ++j)
        replicates[j].reserve(size_t(nSamples));

    std::mt19937_64 rng(0x5EED2026ULL);
    std::uniform_int_distribution<size_t> idxDist(0, activeIdx.size() - 1);
    std::bernoulli_distribution rademacher(0.5);

    d_silent = true;
    d_suppress_curve_output = true;
    d_in_bootstrap = true;
    d_max_iterations = std::min(savedMaxIter, 250);

    for (int b = 0; b < nSamples; ++b) {
        d_y = savedY;
        for (size_t idx : activeIdx) {
            if (mode == 1) {
                // Wild Bootstrap (Rademacher sign flip for heteroscedastic errors)
                const double sign = rademacher(rng) ? 1.0 : -1.0;
                d_y[idx] = savedYFit[idx] + sign * dofScale * (savedY[idx] - savedYFit[idx]);
            } else {
                // Nonparametric Residual Bootstrap
                const size_t u = idxDist(rng);
                d_y[idx] = savedYFit[idx] + sigmaVec[idx] * centeredScaledNormRes[u];
            }
        }
        d_y_errors = savedYErrors;

        bool repOk = false;
        if (is_non_linear && d_param_init && (d_f || d_fsimplex)) {
            for (unsigned j = 0; j < d_p; ++j)
                gsl_vector_set(d_param_init, j, warmInternal[j]);
            int iter = 0;
            int st = GSL_FAILURE;
            std::vector<double> par = (d_solver == NelderMeadSimplex) ? fitGslMultimin(iter, st)
                                                                      : fitGslMultifit(iter, st);
            // Exclude non-converged replicates rather than polluting the bootstrap distribution.
            if (st == GSL_SUCCESS && converged() && std::isfinite(chi_2) && chi_2 >= 0.0
                && par.size() == d_p) {
                storeCustomFitResults(par);
                repOk = (d_results.size() >= d_p);
            }
        } else {
            fit();
            repOk = (converged() && std::isfinite(chi_2) && chi_2 >= 0.0 && d_results.size() >= d_p);
        }

        if (repOk) {
            bool allFinite = true;
            for (unsigned j = 0; j < d_p; ++j) {
                if (!std::isfinite(d_results[j])) {
                    allFinite = false;
                    break;
                }
            }
            if (allFinite) {
                for (unsigned j = 0; j < d_p; ++j)
                    replicates[j].push_back(d_results[j]);
                ++out.nSuccess;
            }
        }
    }

    if (createdBootScript)
        d_script.reset();

    // Restore full baseline fitter state
    d_y = savedY;
    d_y_fit = savedYFit;
    d_y_errors = savedYErrors;
    d_raw_y_errors = savedRawYErrors;
    d_weighting_note = savedWeightingNote;
    d_results = savedResults;
    d_result_errors = savedResultErrors;
    chi_2 = savedChi2;
    d_max_iterations = savedMaxIter;
    d_convergence = savedConvergence;
    d_last_iterations = savedIterations;
    d_fit_notes = savedNotes;
    d_straight_line_fit = savedStraightLine;
    d_line_pearson_r = savedLineR;
    d_has_custom_dof = savedHasCustomDof;
    d_custom_n_points = savedCustomN;
    d_custom_p_free = savedCustomP;
    d_has_custom_rss = savedHasCustomRss;
    d_custom_rss = savedCustomRss;
    d_in_bootstrap = false;
    d_silent = origSilent;
    d_suppress_curve_output = origSuppress;
    if (d_param_init) {
        for (unsigned j = 0; j < d_p; ++j)
            gsl_vector_set(d_param_init, j, savedParamInit[j]);
    }
    if (covar) {
        for (unsigned r = 0; r < d_p; ++r)
            for (unsigned c = 0; c < d_p; ++c)
                gsl_matrix_set(covar, r, c, savedCovar[size_t(r) * size_t(d_p) + size_t(c)]);
    }
    d_covar_scaled = savedCovarScaled;

    if (out.nSuccess < 2)
        return out;

    out.valid = true;
    out.meanParams.resize(int(d_p));
    out.sdParams.resize(int(d_p));
    out.ciLow.resize(int(d_p));
    out.ciHigh.resize(int(d_p));

    const double qLow = 0.5 * (1.0 - cl);
    const double qHigh = 1.0 - qLow;
    auto quantileInterp = [](const std::vector<double> &sortedVals, double q) -> double {
        if (sortedVals.empty())
            return 0.0;
        if (sortedVals.size() == 1)
            return sortedVals.front();
        const double pos = std::max(0.0, std::min(1.0, q)) * double(sortedVals.size() - 1);
        const size_t loIdx = size_t(std::floor(pos));
        const double frac = pos - double(loIdx);
        if (loIdx + 1 < sortedVals.size())
            return sortedVals[loIdx] * (1.0 - frac) + sortedVals[loIdx + 1] * frac;
        return sortedVals.back();
    };

    for (unsigned j = 0; j < d_p; ++j) {
        std::vector<double> &vec = replicates[j];
        const double m = std::accumulate(vec.begin(), vec.end(), 0.0) / double(vec.size());
        double ss = 0.0;
        for (double v : vec) {
            double d = v - m;
            ss += d * d;
        }
        const double sd = (vec.size() > 1) ? std::sqrt(ss / double(vec.size() - 1)) : 0.0;
        std::sort(vec.begin(), vec.end());
        out.meanParams[int(j)] = m;
        out.sdParams[int(j)] = sd;
        out.ciLow[int(j)] = quantileInterp(vec, qLow);
        out.ciHigh[int(j)] = quantileInterp(vec, qHigh);
    }

    ApplicationWindow *app = dynamic_cast<ApplicationWindow *>(parent());
    if (app) {
        QString tblName = app->generateUniqueName(tr("BootstrapCI"));
        Table *tbl = app->newTable(tblName, int(d_p), 6);
        if (tbl) {
            tbl->setHeader(QStringList() << QStringLiteral("Parameter") << QStringLiteral("Estimate")
                                         << QStringLiteral("Boot_Mean") << QStringLiteral("Boot_SE")
                                         << QStringLiteral("CI_Low") << QStringLiteral("CI_High"));
            const QStringList bootLongNames = { tr("Parameter"), tr("Estimate"),
                                                tr("Boot_Mean"), tr("Boot_SE"),
                                                tr("CI_Low"),    tr("CI_High") };
            tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
            for (int c = 1; c < 6; ++c)
                tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            for (int c = 0; c < 6; ++c)
                tbl->column(c)->setLongName(bootLongNames[c]);

            for (unsigned j = 0; j < d_p; ++j) {
                QString pName = (int(j) < d_param_names.size()) ? d_param_names[int(j)]
                                                                : QString("p%1").arg(j + 1);
                tbl->column(0)->setTextAt(int(j), pName);
                tbl->column(1)->setValueAt(int(j), savedResults[j]);
                tbl->column(2)->setValueAt(int(j), out.meanParams[int(j)]);
                tbl->column(3)->setValueAt(int(j), out.sdParams[int(j)]);
                tbl->column(4)->setValueAt(int(j), out.ciLow[int(j)]);
                tbl->column(5)->setValueAt(int(j), out.ciHigh[int(j)]);
            }
            if (!origSilent)
                tbl->showNormal();
            out.reportTable = tbl;
        }
    }

    return out;
}

void Fit::enableConfidenceBand(bool on, double confLevel)
{
    d_conf_band = on;
    if (confLevel > 0.0 && confLevel < 1.0)
        d_conf_level = confLevel;
}

void Fit::enablePredictionBand(bool on, double confLevel)
{
    d_pred_band = on;
    if (confLevel > 0.0 && confLevel < 1.0)
        d_pred_level = confLevel;
}

void Fit::addConfidenceAndPredictionBands(const std::vector<double> &solverPar)
{
    if ((!d_conf_band && !d_pred_band) || !d_graph || d_suppress_curve_output || d_x.empty()
        || !covar || solverPar.size() < d_p)
        return;

    const int dofInt = degreesOfFreedom();
    const double covScale = effectiveCovarScale();
    if (dofInt <= 0 || !std::isfinite(covScale)) {
        d_fit_notes << tr("Confidence/prediction bands skipped: residual degrees of freedom <= 0.");
        return;
    }

    int savedPoints = d_points;
    int nPts = d_gen_function ? std::max(2, d_points) : int(d_x.size());
    d_points = nPts;

    std::vector<double> X_band(nPts), Y_hat(nPts);
    if (!evaluateModelCurve(solverPar, X_band, Y_hat)) {
        d_points = savedPoints;
        return;
    }

    std::vector<std::vector<double>> G(nPts, std::vector<double>(d_p, 0.0));
    std::vector<double> X_tmp(nPts), Y_plus(nPts), Y_minus(nPts);
    for (unsigned j = 0; j < d_p; ++j) {
        if (isParameterFixed(int(j)))
            continue;
        double pj = solverPar[j];
        double h = std::max(1e-12, 1e-5 * std::abs(pj));
        std::vector<double> p_plus = solverPar;
        std::vector<double> p_minus = solverPar;
        p_plus[j] = pj + h;
        p_minus[j] = pj - h;
        if (evaluateModelCurve(p_plus, X_tmp, Y_plus)
            && evaluateModelCurve(p_minus, X_tmp, Y_minus)) {
            const double scale = covarSolverToStoredScale(int(j), pj);
            const double denom = 2.0 * h * ((std::abs(scale) > 1e-30) ? scale : 1.0);
            for (int k = 0; k < nPts; ++k)
                G[k][j] = (Y_plus[k] - Y_minus[k]) / denom;
        }
    }
    d_points = savedPoints;

    const double dof = double(dofInt);
    const double rawRss = rss();
    const double s2Unweighted = (std::isfinite(rawRss) && rawRss >= 0.0) ? (rawRss / dof) : 0.0;
    const bool isWeighted = (d_y_error_source != UnknownErrors || d_weighting_method != WeightNone);

    // Build local observation variance sigma^2(x) for weighted prediction bands
    std::vector<std::pair<double, double>> activeXVar;
    double medianVar = s2Unweighted;
    double statFloor = 1.0;
    if (isWeighted) {
        const std::vector<double> w = computeWeightsVector();
        std::vector<double> varList;
        double maxAbsY = 0.0;
        for (size_t i = 0; i < d_x.size(); ++i) {
            if (i < d_y.size() && std::isfinite(d_y[i]))
                maxAbsY = std::max(maxAbsY, std::abs(d_y[i]));
            if (i < w.size() && std::isfinite(w[i]) && w[i] > 0.0) {
                const double v = 1.0 / w[i];
                if (std::isfinite(v)) {
                    activeXVar.emplace_back(d_x[i], v);
                    varList.push_back(v);
                }
            }
        }
        statFloor = (maxAbsY > 0.0 && maxAbsY < 1.0) ? std::max(1e-12, 1e-3 * maxAbsY) : 1.0;
        std::sort(activeXVar.begin(), activeXVar.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });
        if (!varList.empty()) {
            std::sort(varList.begin(), varList.end());
            medianVar = varList[varList.size() / 2];
        }
    }

    auto localObsVariance = [&](double xq, double yHat) -> double {
        if (!isWeighted)
            return s2Unweighted;
        double baseVar = medianVar;
        if (d_weighting_method == WeightStatistical) {
            baseVar = std::max(std::abs(yHat), statFloor);
        } else if (d_weighting_method == WeightProportional) {
            baseVar = (std::isfinite(yHat) && std::abs(yHat) > 0.0) ? (yHat * yHat) : medianVar;
        } else if (!activeXVar.empty()) {
            if (xq <= activeXVar.front().first) {
                baseVar = activeXVar.front().second;
            } else if (xq >= activeXVar.back().first) {
                baseVar = activeXVar.back().second;
            } else {
                auto it = std::lower_bound(
                        activeXVar.begin(), activeXVar.end(), xq,
                        [](const std::pair<double, double> &p, double val) { return p.first < val; });
                if (it == activeXVar.begin()) {
                    baseVar = it->second;
                } else {
                    const auto &p1 = *it;
                    const auto &p0 = *(it - 1);
                    const double dx = p1.first - p0.first;
                    const double t = (dx > 1e-30) ? ((xq - p0.first) / dx) : 0.0;
                    baseVar = p0.second + t * (p1.second - p0.second);
                }
            }
        }
        return std::max(0.0, (d_scale_errors ? covScale : 1.0) * baseVar);
    };

    double confAlpha = std::max(0.50, std::min(0.9999, d_conf_level));
    double predAlpha = std::max(0.50, std::min(0.9999, d_pred_level));
    double t_conf = gsl_cdf_tdist_Pinv(0.5 * (1.0 + confAlpha), dof);
    double t_pred = gsl_cdf_tdist_Pinv(0.5 * (1.0 + predAlpha), dof);

    std::vector<double> confUpper(nPts), confLower(nPts), predUpper(nPts), predLower(nPts);
    for (int k = 0; k < nPts; ++k) {
        double var_mean = 0.0;
        for (unsigned a = 0; a < d_p; ++a) {
            if (G[k][a] == 0.0)
                continue;
            for (unsigned b = 0; b < d_p; ++b)
                var_mean += G[k][a] * (covScale * gsl_matrix_get(covar, a, b)) * G[k][b];
        }
        var_mean = std::max(0.0, var_mean);
        const double obsVar = localObsVariance(X_band[k], Y_hat[k]);
        double var_pred = std::max(0.0, obsVar + var_mean);

        confUpper[k] = Y_hat[k] + t_conf * std::sqrt(var_mean);
        confLower[k] = Y_hat[k] - t_conf * std::sqrt(var_mean);
        predUpper[k] = Y_hat[k] + t_pred * std::sqrt(var_pred);
        predLower[k] = Y_hat[k] - t_pred * std::sqrt(var_pred);
    }

    // Preserve the graph legend text so band boundaries do not pollute the legend with 4 extra rows.
    Legend *existingLegend = d_graph->legend();
    const QString savedLegendText = existingLegend ? existingLegend->text() : QString();

    auto insertBandCurve = [this](const QString &exactTitle, const std::vector<double> &Xb,
                                  const std::vector<double> &Yb, const QColor &col,
                                  Qt::PenStyle penStyle) {
        if (d_graph->curveIndex(exactTitle) >= 0)
            d_graph->removeCurve(exactTitle);
        FunctionCurve *c = new FunctionCurve((ApplicationWindow *)parent(), FunctionCurve::Normal,
                                             exactTitle);
        c->setPen(QPen(col, 1, penStyle));
        c->setItemAttribute(QwtPlotItem::Legend, false);
        c->setData(Xb.data(), Yb.data(), int(Xb.size()));
        if (!Xb.empty())
            c->setRange(Xb.front(), Xb.back());
        d_graph->insertPlotItem(c, Graph::Line);
        d_graph->addFitCurve(c);
    };

    QString baseName =
            (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0) ? d_curve->title().text()
                                                                      : objectName();
    QVector<double> qX(nPts);
    for (int k = 0; k < nPts; ++k)
        qX[k] = X_band[k];

    if (d_pred_band) {
        QVector<double> qLo(nPts), qHi(nPts);
        for (int k = 0; k < nPts; ++k) {
            qLo[k] = predLower[k];
            qHi[k] = predUpper[k];
        }
        const QString predBandTitle = baseName + QStringLiteral("_PredBand");
        d_graph->removeShadedBandByTitle(predBandTitle);
        if (QwtPlotItem *band = d_graph->addShadedBand(qX, qLo, qHi, QColor(255, 140, 0, 30),
                                                       predBandTitle)) {
            band->setZ(0.8);
            band->setItemAttribute(QwtPlotItem::Legend, false);
        }
        insertBandCurve(baseName + "_PredUpper", X_band, predUpper, QColor(230, 125, 0, 130),
                        Qt::DotLine);
        insertBandCurve(baseName + "_PredLower", X_band, predLower, QColor(230, 125, 0, 130),
                        Qt::DotLine);
    }
    if (d_conf_band) {
        QVector<double> qLo(nPts), qHi(nPts);
        for (int k = 0; k < nPts; ++k) {
            qLo[k] = confLower[k];
            qHi[k] = confUpper[k];
        }
        const QString confBandTitle = baseName + QStringLiteral("_ConfBand");
        d_graph->removeShadedBandByTitle(confBandTitle);
        if (QwtPlotItem *band = d_graph->addShadedBand(qX, qLo, qHi, QColor(205, 35, 45, 42),
                                                       confBandTitle)) {
            band->setZ(1.0);
            band->setItemAttribute(QwtPlotItem::Legend, false);
        }
        insertBandCurve(baseName + "_ConfUpper", X_band, confUpper, QColor(205, 35, 45, 135),
                        Qt::DashLine);
        insertBandCurve(baseName + "_ConfLower", X_band, confLower, QColor(205, 35, 45, 135),
                        Qt::DashLine);
    }

    if (existingLegend && d_graph->legend() == existingLegend)
        existingLegend->setText(savedLegendText);
    d_graph->replot();
}

double Fit::aic() const
{
    int n = effectivePointCount();
    if (n <= 0 || !hasValidFitMetrics(this, d_results, chi_2, d_init_err, d_x, d_y))
        return std::numeric_limits<double>::quiet_NaN();
    const bool isWeighted = (d_y_error_source != UnknownErrors || d_weighting_method != WeightNone);
    const double rawObj = isWeighted ? chi_2 : rss();
    if (!std::isfinite(rawObj) || rawObj < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    int k = std::max(1, effectiveFreeParameters()) + 1;
    double objVal = std::max(1e-300, rawObj);
    return double(n) * std::log(objVal / double(n)) + 2.0 * double(k);
}

double Fit::aicc() const
{
    int n = effectivePointCount();
    if (n <= 0 || !hasValidFitMetrics(this, d_results, chi_2, d_init_err, d_x, d_y))
        return std::numeric_limits<double>::quiet_NaN();
    int k = std::max(1, effectiveFreeParameters()) + 1;
    if (n <= k + 1)
        return std::numeric_limits<double>::quiet_NaN();
    double baseAic = aic();
    if (!std::isfinite(baseAic))
        return std::numeric_limits<double>::quiet_NaN();
    return baseAic + (2.0 * double(k) * double(k + 1)) / double(n - k - 1);
}

double Fit::bic() const
{
    int n = effectivePointCount();
    if (n <= 0 || !hasValidFitMetrics(this, d_results, chi_2, d_init_err, d_x, d_y))
        return std::numeric_limits<double>::quiet_NaN();
    const bool isWeighted = (d_y_error_source != UnknownErrors || d_weighting_method != WeightNone);
    const double rawObj = isWeighted ? chi_2 : rss();
    if (!std::isfinite(rawObj) || rawObj < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    int k = std::max(1, effectiveFreeParameters()) + 1;
    double objVal = std::max(1e-300, rawObj);
    return double(n) * std::log(objVal / double(n)) + double(k) * std::log(std::max(1.0, double(n)));
}

double Fit::anovaFValue() const
{
    const int n = effectivePointCount();
    const int p_free = std::max(1, effectiveFreeParameters());
    const int df_err = n - p_free;
    if (n <= 1 || df_err <= 0 || d_y.empty()
        || !hasValidFitMetrics(this, d_results, chi_2, d_init_err, d_x, d_y))
        return std::numeric_limits<double>::quiet_NaN();
    const double rawRss = rss();
    if (!std::isfinite(rawRss) || rawRss < 0.0)
        return std::numeric_limits<double>::quiet_NaN();

    const std::vector<double> w = computeWeightsVector();
    double mean = 0.0;
    int activeCnt = 0;
    for (size_t i = 0; i < d_y.size(); ++i) {
        if (i < w.size() && !(w[i] > 0.0))
            continue;
        mean += d_y[i];
        ++activeCnt;
    }
    if (activeCnt <= 1)
        return std::numeric_limits<double>::quiet_NaN();
    mean /= double(activeCnt);
    double tss = 0.0;
    for (size_t i = 0; i < d_y.size(); ++i) {
        if (i < w.size() && !(w[i] > 0.0))
            continue;
        double d = d_y[i] - mean;
        tss += d * d;
    }
    double rssVal = std::max(0.0, rawRss);
    double mss = std::max(0.0, tss - rssVal);

    int df_model = (p_free > 1) ? (p_free - 1) : 1;

    double ms_model = mss / double(df_model);
    double ms_err = rssVal / double(df_err);
    if (ms_err <= 1e-300)
        return (ms_model > 0.0) ? 1e15 : 0.0;
    return ms_model / ms_err;
}

double Fit::anovaPValue() const
{
    const int n = effectivePointCount();
    const int p_free = std::max(1, effectiveFreeParameters());
    const int df_err = n - p_free;
    if (n <= 1 || df_err <= 0 || d_y.empty())
        return std::numeric_limits<double>::quiet_NaN();

    int df_model = (p_free > 1) ? (p_free - 1) : 1;

    double F = anovaFValue();
    if (!std::isfinite(F))
        return std::numeric_limits<double>::quiet_NaN();
    if (F <= 0.0)
        return 1.0;
    if (F >= 1e14)
        return 0.0;
    double p = gsl_cdf_fdist_Q(F, double(df_model), double(df_err));
    return std::max(0.0, std::min(1.0, p));
}

void Fit::insertFitSummaryBoxOnGraph()
{
    if (!d_graph)
        return;

    QString curveLabel;
    if (d_curve) {
        const int cIdx = d_graph->curveIndex(d_curve);
        if (cIdx >= 0) {
            curveLabel = d_graph->resolvedAutoCurveLabel(cIdx).trimmed();
            if (curveLabel.isEmpty()) {
                curveLabel = d_curve->title().text().trimmed();
                if (DataCurve *dc = dynamic_cast<DataCurve *>(d_curve)) {
                    if (Table *tbl = dc->table()) {
                        const QString pfx = tbl->objectName() + QLatin1Char('_');
                        if (curveLabel.startsWith(pfx))
                            curveLabel = curveLabel.mid(pfx.length());
                    }
                } else {
                    const int us = curveLabel.indexOf(QLatin1Char('_'));
                    if (us > 0
                        && curveLabel.left(us).startsWith(QStringLiteral("Table"),
                                                          Qt::CaseInsensitive))
                        curveLabel = curveLabel.mid(us + 1);
                }
            }
        }
    }

    QStringList lines;
    if (!curveLabel.isEmpty())
        lines << QStringLiteral("%1 (%2)").arg(objectName(), curveLabel);
    else
        lines << objectName();
    if (!converged())
        lines << convergenceMessage();

    const double r2Val = rSquare();
    lines << QStringLiteral("%1 = %2")
                     .arg(tr("R\u00B2"),
                          std::isfinite(r2Val) ? QLocale().toString(r2Val, 'f', 4)
                                               : QString::fromUtf8("—"));

    const int valPrec = qBound(3, d_prec, 5);
    const int errPrec = qBound(2, d_prec - 1, 3);
    for (unsigned i = 0; i < d_p && i < d_results.size(); ++i) {
        const QString pName = (int(i) < d_param_names.size())
                ? d_param_names[int(i)]
                : QStringLiteral("p%1").arg(i);
        QString pLine = pName + QStringLiteral(" = ")
                + QLocale().toString(d_results[i], 'g', valPrec);
        if (isParameterFixed(int(i))) {
            pLine += QStringLiteral(" (") + tr("fixed") + QStringLiteral(")");
        } else if (covar) {
            const double err = parameterStdError(int(i));
            if (std::isfinite(err))
                pLine += QStringLiteral(" \u00B1 ") + QLocale().toString(err, 'g', errPrec);
            else
                pLine += QStringLiteral(" \u00B1 \u2014");
        }
        lines << pLine;
    }

    const QString text = lines.join(QLatin1Char('\n'));

    Legend *mrk = d_graph->newLegend(text);
    if (mrk) {
        const int fontPt = qMax(9, d_graph->legendFont().pointSize());
        QFont calloutFont(QStringLiteral("Arial"), fontPt, QFont::Normal);
        calloutFont.setHintingPreference(QFont::PreferNoHinting);
        mrk->setFont(calloutFont);
        mrk->setFrameStyle(Legend::Line);
        mrk->setBackgroundColor(QColor(255, 255, 255, 235));
        if (d_graph->hasLegend()) {
            Legend *leg = d_graph->legend();
            if (leg && leg != mrk) {
                const QPoint p = leg->rect().bottomLeft();
                mrk->setOrigin(QPoint(p.x(), p.y() + 14));
            }
        }
    }
    d_graph->replot();
}

MultiLayer *Fit::createResidualPlotWindow()
{
    ApplicationWindow *app = (ApplicationWindow *)parent();
    if (!app)
        return nullptr;

    Table *resTable = createResidualsTable();
    if (!resTable || resTable->numRows() <= 0)
        return nullptr;

    QStringList plotCols;
    plotCols << resTable->colName(3);
    MultiLayer *ml = app->multilayerPlot(resTable, plotCols, Graph::Scatter, 0,
                                         resTable->numRows() - 1);
    if (!ml)
        return nullptr;

    ml->setFonts(QFont("Arial", 11, QFont::Normal), QFont("Arial", 10, QFont::Normal),
                 QFont("Arial", 9, QFont::Normal), QFont("Arial", 9, QFont::Normal));
    Graph *rg = ml->activeGraph();
    if (rg) {
        QString srcTitle = (d_curve && d_graph && d_graph->curveIndex(d_curve) >= 0)
                ? d_curve->title().text()
                : objectName();
        rg->setTitle(tr("Residuals of %1 (%2)").arg(srcTitle, objectName()));
        rg->setTitleFont(QFont("Arial", 11, QFont::Normal));
        rg->setXAxisTitle(tr("X"));
        rg->setYAxisTitle(tr("Residual (Y - Y_Fit)"));
        rg->initFonts(QFont("Arial", 10, QFont::Normal), QFont("Arial", 9, QFont::Normal));
        if (rg->legend())
            rg->legend()->setFont(QFont("Arial", 9, QFont::Normal));
        if (!d_x.empty()) {
            double xMin = *std::min_element(d_x.begin(), d_x.end());
            double xMax = *std::max_element(d_x.begin(), d_x.end());
            if (xMax > xMin) {
                FunctionCurve *zeroLine =
                        new FunctionCurve(app, FunctionCurve::Normal, tr("ZeroResidualLine"));
                zeroLine->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
                double zx[2] = { xMin, xMax };
                double zy[2] = { 0.0, 0.0 };
                zeroLine->setData(zx, zy, 2);
                zeroLine->setRange(xMin, xMax);
                zeroLine->setFormula("0");
                rg->insertPlotItem(zeroLine, Graph::Line);
            }
        }
        rg->replot();
    }
    return ml;
}

Table *Fit::compareModels(ApplicationWindow *app, Graph *g, const QString &curveTitle,
                          const QStringList &modelNames, int maxIterations)
{
    if (!app || !g || curveTitle.isEmpty() || modelNames.isEmpty())
        return nullptr;
    if (g->curveIndex(curveTitle) < 0)
        return nullptr;

    struct ModelEvalRow
    {
        QString name;
        int numParams = 0;
        double rssVal = 0.0;
        double r2Val = 0.0;
        double adjR2Val = 0.0;
        double aicVal = 0.0;
        double aiccVal = 0.0;
        double bicVal = 0.0;
        double deltaAic = 0.0;
        double akaikeWeight = 0.0;
        bool converged = true;
        QString convergenceNote;
    };

    std::vector<ModelEvalRow> evalRows;
    evalRows.reserve(size_t(modelNames.size()));

    for (const QString &rawName : modelNames) {
        QString mName = rawName.trimmed();
        if (mName.isEmpty())
            continue;

        std::unique_ptr<Fit> fitter;
        QRegExp polyRx("^(?:poly(?:nomial)?)[_\\s]*(\\d+)$", Qt::CaseInsensitive);
        if (mName.compare("Linear", Qt::CaseInsensitive) == 0
            || mName.compare("Line", Qt::CaseInsensitive) == 0
            || mName.compare("Poly1", Qt::CaseInsensitive) == 0) {
            fitter.reset(new LinearFit(app, g));
        } else if (mName.compare("Quadratic", Qt::CaseInsensitive) == 0
                   || mName.compare("Parabola", Qt::CaseInsensitive) == 0) {
            fitter.reset(new PolynomialFit(app, g, 2, false));
        } else if (mName.compare("Cubic", Qt::CaseInsensitive) == 0) {
            fitter.reset(new PolynomialFit(app, g, 3, false));
        } else if (mName.compare("Quartic", Qt::CaseInsensitive) == 0) {
            fitter.reset(new PolynomialFit(app, g, 4, false));
        } else if (polyRx.exactMatch(mName)) {
            int ord = std::max(1, polyRx.cap(1).toInt());
            if (ord == 1)
                fitter.reset(new LinearFit(app, g));
            else
                fitter.reset(new PolynomialFit(app, g, ord, false));
        } else {
            std::unique_ptr<NonLinearFit> nlf(new NonLinearFit(app, g));
            if (nlf->setBuiltInModel(mName)) {
                fitter = std::move(nlf);
            } else if (mName.compare("Gauss", Qt::CaseInsensitive) == 0) {
                fitter.reset(new GaussFit(app, g));
            } else if (mName.compare("Lorentz", Qt::CaseInsensitive) == 0) {
                fitter.reset(new LorentzFit(app, g));
            } else if (mName.compare("GaussAmp", Qt::CaseInsensitive) == 0) {
                fitter.reset(new GaussAmpFit(app, g));
            } else if (mName.compare("PseudoVoigt", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::PseudoVoigt, 1));
            } else if (mName.compare("PearsonVII", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::PearsonVII, 1));
            } else if (mName.compare("Voigt", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::Voigt, 1));
            } else if (mName.compare("BiGaussian", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::BiGaussian, 1));
            } else if (mName.compare("EMG", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::EMG, 1));
            } else if (mName.compare("LogNormal", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::LogNormal, 1));
            } else if (mName.compare("Asym2Sig", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::Asym2Sig, 1));
            } else if (mName.compare("DoniachSunjic", Qt::CaseInsensitive) == 0) {
                fitter.reset(new MultiPeakFit(app, g, MultiPeakFit::DoniachSunjic, 1));
            } else if (mName.compare("Boltzmann", Qt::CaseInsensitive) == 0
                       || mName.compare("Sigmoidal", Qt::CaseInsensitive) == 0) {
                fitter.reset(new SigmoidalFit(app, g));
            } else if (mName.compare("ExpDecay", Qt::CaseInsensitive) == 0
                       || mName.compare("ExpDecay1", Qt::CaseInsensitive) == 0) {
                fitter.reset(new ExponentialFit(app, g, false));
            } else if (mName.compare("ExpDecay2", Qt::CaseInsensitive) == 0) {
                fitter.reset(new TwoExpFit(app, g));
            } else if (mName.compare("ExpDecay3", Qt::CaseInsensitive) == 0) {
                fitter.reset(new ThreeExpFit(app, g));
            } else if (mName.compare("ExpGrowth", Qt::CaseInsensitive) == 0) {
                fitter.reset(new ExponentialFit(app, g, true));
            } else {
                continue;
            }
        }

        fitter->d_silent = true;
        fitter->d_suppress_curve_output = true;
        if (!fitter->setDataFromCurve(curveTitle, g) || fitter->error())
            continue;
        if (maxIterations > 0)
            fitter->setMaximumIterations(maxIterations);

        fitter->fit();
        if (fitter->results().empty() || !std::isfinite(fitter->rss()))
            continue;

        ModelEvalRow row;
        row.name = rawName;
        row.numParams = fitter->effectiveFreeParameters();
        row.rssVal = fitter->rss();
        row.r2Val = fitter->rSquare();
        row.adjR2Val = fitter->adjustedRSquare();
        row.aicVal = fitter->aic();
        row.aiccVal = fitter->aicc();
        row.bicVal = fitter->bic();
        row.converged = fitter->converged();
        row.convergenceNote = fitter->convergenceMessage();
        evalRows.push_back(row);
    }

    if (evalRows.empty())
        return nullptr;

    // A non-converged fit is the last iterate, not a minimum: its AIC says nothing about the
    // model. Rank only the converged fits; list the others after them with no ΔAIC, weight or
    // rank and a note in the Convergence column.
    std::stable_sort(evalRows.begin(), evalRows.end(),
                     [](const ModelEvalRow &a, const ModelEvalRow &b) {
                         if (a.converged != b.converged)
                             return a.converged;
                         return a.aicVal < b.aicVal;
                     });

    double minAic = std::numeric_limits<double>::infinity();
    for (const auto &r : evalRows) {
        if (r.converged)
            minAic = std::min(minAic, r.aicVal);
    }
    double weightSum = 0.0;
    for (auto &r : evalRows) {
        if (!r.converged)
            continue;
        r.deltaAic = r.aicVal - minAic;
        r.akaikeWeight = std::exp(-0.5 * std::min(700.0, r.deltaAic));
        weightSum += r.akaikeWeight;
    }
    if (weightSum > 0.0) {
        for (auto &r : evalRows) {
            if (r.converged)
                r.akaikeWeight /= weightSum;
        }
    }

    registerFitJapaneseStrings();
    QString tblName = app->generateUniqueName(tr("ModelComparison"));
    int nRows = int(evalRows.size());
    Table *tbl = app->newTable(tblName, nRows, 12);
    if (!tbl)
        return nullptr;

    tbl->setHeader(QStringList() << QStringLiteral("Model") << QStringLiteral("NumParams")
                                 << QStringLiteral("RSS") << QStringLiteral("R2")
                                 << QStringLiteral("AdjR2") << QStringLiteral("AIC")
                                 << QStringLiteral("AICc") << QStringLiteral("BIC")
                                 << QStringLiteral("DeltaAIC") << QStringLiteral("AkaikeWeight")
                                 << QStringLiteral("Rank") << QStringLiteral("Convergence"));
    const QStringList cmpLongNames = {
        tr("Model"),        tr("NumParams"), tr("RSS"),     tr("R2"),
        tr("AdjR2"),        tr("AIC"),       tr("AICc"),    tr("BIC"),
        tr("DeltaAIC"),     tr("AkaikeWeight"), tr("Rank"), tr("Convergence")
    };
    tbl->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int c = 1; c < 11; ++c)
        tbl->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
    tbl->column(11)->setColumnMode(SciDAVis::ColumnMode::Text);
    for (int c = 0; c < 12; ++c)
        tbl->column(c)->setLongName(cmpLongNames[c]);

    for (int i = 0; i < nRows; ++i) {
        const auto &r = evalRows[size_t(i)];
        tbl->column(0)->setTextAt(i, r.name);
        tbl->column(1)->setValueAt(i, r.numParams);
        tbl->column(2)->setValueAt(i, r.rssVal);
        tbl->column(3)->setValueAt(i, r.r2Val);
        if (std::isfinite(r.adjR2Val))
            tbl->column(4)->setValueAt(i, r.adjR2Val);
        else
            tbl->column(4)->setInvalid(i);
        tbl->column(5)->setValueAt(i, r.aicVal);
        if (std::isfinite(r.aiccVal))
            tbl->column(6)->setValueAt(i, r.aiccVal);
        else
            tbl->column(6)->setInvalid(i);
        tbl->column(7)->setValueAt(i, r.bicVal);
        if (r.converged) {
            tbl->column(8)->setValueAt(i, r.deltaAic);
            tbl->column(9)->setValueAt(i, r.akaikeWeight);
            tbl->column(10)->setValueAt(i, i + 1); // converged rows come first
            tbl->column(11)->setTextAt(i, tr("converged"));
        } else {
            tbl->column(8)->setInvalid(i);
            tbl->column(9)->setInvalid(i);
            tbl->column(10)->setInvalid(i);
            tbl->column(11)->setTextAt(i, r.convergenceNote.isEmpty() ? tr("Not converged")
                                                                     : r.convergenceNote);
        }
    }

    tbl->showNormal();
    return tbl;
}
