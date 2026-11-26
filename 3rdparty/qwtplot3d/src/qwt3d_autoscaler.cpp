#include <cmath>
#include "qwt3d_helper.h"
#include "qwt3d_autoscaler.h"

using namespace Qwt3D;

namespace {

double floorExt(int &exponent, double x, std::vector<double> &sortedmantissi)
{
    if (!std::isfinite(x) || x == 0.0) {
        exponent = 0;
        return 0.0;
    }

    double sign = (x > 0) ? 1.0 : -1.0;
    double lx = log10(fabs(x));
    if (!std::isfinite(lx)) {
        exponent = 0;
        return 0.0;
    }
    exponent = (int)floor(lx);

    double fr = pow(10.0, lx - exponent);
    if (fr >= 10.0) {
        fr = 1.0;
        ++exponent;
    } else {
        for (int i = (int)sortedmantissi.size() - 1; i >= 0; --i) {
            if (fr >= sortedmantissi[i]) {
                fr = sortedmantissi[i];
                break;
            }
        }
    }
    return sign * fr;
}

/*
  \brief Find the largest value out of {1,2,5}*10^n with an integer number n
  which is smaller than or equal to x
  \param exponent n
  \param x Input value
  \return Mantissa
*/
[[maybe_unused]] double floor125(int &exponent, double x)
{
    std::vector<double> m(3);
    m[0] = 1;
    m[1] = 2;
    m[2] = 5;
    return floorExt(exponent, x, m);
}

} // anon ns

//! Initializes with an {1,2,5} sequence of mantissas
LinearAutoScaler::LinearAutoScaler()
{
    init(0, 1, 1);
    mantissi_ = std::vector<double>(3);
    mantissi_[0] = 1;
    mantissi_[1] = 2;
    mantissi_[2] = 5;
}
//! Initialize with interval [0,1] and one requested interval
/*!
val mantisse A increasing ordered vector of values representing
mantisse values between 1 and 9.
*/
LinearAutoScaler::LinearAutoScaler(std::vector<double> &mantisse)
{
    init(0, 1, 1);
    if (mantisse.empty()) {
        mantissi_ = std::vector<double>(3);
        mantissi_[0] = 1;
        mantissi_[1] = 2;
        mantissi_[2] = 5;
        return;
    }
    mantissi_ = mantisse;
}

//! Initialize with interval [start,stop] and number of requested intervals
/**
        Switchs start and stop, if stop < start and sets intervals = 1 if ivals < 1
*/
void LinearAutoScaler::init(double start, double stop, int ivals)
{
    start_ = start;
    stop_ = stop;
    intervals_ = ivals;

    if (start_ > stop_) {
        double tmp = start_;
        start_ = stop_;
        stop_ = tmp;
    }
    if (intervals_ < 1)
        intervals_ = 1;
}

/*!
\return Anchor value

\verbatim
|_______|____________ _ _ _ _  _____|_____________|________________

0     m*10^n                      start         anchor := c*m*10^n

c 'minimal' (anchor-start < m*10^n)
\endverbatim
*/
double LinearAutoScaler::anchorvalue(double start, double m, int n)
{
    double stepval = m * pow(10.0, n);
    if (!std::isfinite(stepval) || stepval <= 0.0)
        return start;
    double q = start / stepval;
    if (!std::isfinite(q))
        return start;
    double anchor = stepval * ceil(q);
    if (!std::isfinite(anchor))
        return start;
    return anchor;
}

/*!
\return New number of intervals (:= l_intervals + r_intervals)
\param l_intervals  Number of intervals left from anchor
\param r_intervals  Number of intervals right from anchor

\verbatim
                          -l_intervals * i    -2 * i    -i                 +r_intervals * i
                                                                                   |
|______|_______ _ _ _ ____|____|___ _ _ _ _ _ _ _|_______|_______|_ _ _ _ _ _ _____|__|_____
       |                  |                                      |                    |
0   i := m*10^n         start                                  anchor	              stop

c 'minimal' (anchor-start < m*10^n)
\endverbatim
*/
int LinearAutoScaler::segments(int &l_intervals, int &r_intervals, double start, double stop,
                               double anchor, double m, int n)
{
    double val = m * pow(10.0, n);
    if (!std::isfinite(val) || val <= 0.0 || !std::isfinite(anchor)) {
        l_intervals = 0;
        r_intervals = 0;
        return 0;
    }
    double delta = (stop - anchor) / val;
    if (!std::isfinite(delta) || fabs(delta) > 1e8) {
        l_intervals = 0;
        r_intervals = 0;
        return 0;
    }

    r_intervals = (int)floor(delta); // right side intervals

    delta = (anchor - start) / val;
    if (!std::isfinite(delta) || fabs(delta) > 1e8) {
        l_intervals = 0;
        r_intervals = 0;
        return 0;
    }

    l_intervals = (int)floor(delta); // left side intervals

    return r_intervals + l_intervals;
}

/*!
        \brief Does the actual scaling
        \return Number of intervals after rescaling. This will in the most cases differ
        from the requested interval number!  Always >0.
        \param a Start value after scaling (always >= start)
        \param b Stop value after scaling  (always <= stop)
  \param start Start value
  \param stop Stop value
  \param ivals Requested intervals
  \return Number of intervals after autoscaling

        If the given interval has zero length the function returns the current
        interval number and a and b remain unchanged.
*/
int LinearAutoScaler::execute(double &a, double &b, double start, double stop, int ivals)
{
    init(start, stop, ivals);

    if (!std::isfinite(start_) || !std::isfinite(stop_) || !std::isfinite(stop_ - start_)
        || stop_ <= start_) {
        a = 0.0;
        b = 0.0;
        return 0;
    }

    double delta = stop_ - start_;

    if (isPracticallyZero(delta)) {
        a = start_;
        b = stop_;
        return intervals_;
    }

    if (mantissi_.empty()) {
        a = start_;
        b = stop_;
        return intervals_;
    }

    double c;
    int n;

    c = floorExt(n, delta, mantissi_);
    double stepval = c * pow(10.0, n);
    if (!std::isfinite(c) || c <= 0.0 || !std::isfinite(stepval) || stepval <= 0.0) {
        a = start_;
        b = stop_;
        return intervals_;
    }

    int l_ival = 0, r_ival = 0;

    double anchor = anchorvalue(start_, c, n);
    if (!std::isfinite(anchor)) {
        a = start_;
        b = stop_;
        return intervals_;
    }
    int ival = segments(l_ival, r_ival, start_, stop_, anchor, c, n);

    if (ival >= intervals_) {
        const double candA = anchor - l_ival * stepval;
        const double candB = anchor + r_ival * stepval;
        if (ival > 0 && std::isfinite(candA) && std::isfinite(candB) && candA < candB) {
            a = candA;
            b = candB;
            intervals_ = ival;
            return intervals_;
        }
        a = start_;
        b = stop_;
        return intervals_;
    }

    int prev_ival, prev_l_ival, prev_r_ival;
    double prev_anchor;
    double prev_c;
    int prev_n;

    constexpr int kMaxIter = 256;
    for (int iter = 0; iter < kMaxIter; ++iter) {
        prev_c = c;
        prev_n = n;
        prev_anchor = anchor;
        prev_ival = ival;
        prev_l_ival = l_ival;
        prev_r_ival = r_ival;

        if (int(c) <= 1) {
            c = mantissi_.back();
            --n;
        } else {
            bool matched = false;
            for (size_t i = mantissi_.size() - 1; i > 0; --i) {
                if (int(c) == int(mantissi_[i])) {
                    c = mantissi_[i - 1];
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                c = mantissi_.back();
                --n;
            }
        }

        if (n < -308) {
            a = start_;
            b = stop_;
            return intervals_;
        }

        stepval = c * pow(10.0, n);
        if (!std::isfinite(stepval) || stepval <= 0.0) {
            a = start_;
            b = stop_;
            return intervals_;
        }

        anchor = anchorvalue(start_, c, n);
        if (!std::isfinite(anchor)) {
            a = start_;
            b = stop_;
            return intervals_;
        }
        ival = segments(l_ival, r_ival, start_, stop_, anchor, c, n);

        int prev_diff = intervals_ - prev_ival;
        int actual_diff = ival - intervals_;

        if (prev_diff >= 0 && actual_diff >= 0) {
            if (prev_ival > 0 && prev_diff < actual_diff) {
                c = prev_c;
                n = prev_n;
                anchor = prev_anchor;
                ival = prev_ival;
                l_ival = prev_l_ival;
                r_ival = prev_r_ival;
            }
            const double chosenStep = c * pow(10.0, n);
            if (!std::isfinite(chosenStep) || chosenStep <= 0.0) {
                a = start_;
                b = stop_;
                return intervals_;
            }
            const double candA = anchor - l_ival * chosenStep;
            const double candB = anchor + r_ival * chosenStep;
            if (ival > 0 && std::isfinite(candA) && std::isfinite(candB) && candA < candB) {
                a = candA;
                b = candB;
                intervals_ = ival;
                return intervals_;
            }
            a = start_;
            b = stop_;
            return intervals_;
        }
    }

    a = start_;
    b = stop_;
    return intervals_;
}
