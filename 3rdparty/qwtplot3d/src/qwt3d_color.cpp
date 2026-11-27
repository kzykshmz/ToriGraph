#include <cmath>
#include "qwt3d_color.h"
#include "qwt3d_plot.h"

using namespace Qwt3D;

StandardColor::StandardColor(Plot3D *data, unsigned size) : data_(data)
{
    Q_ASSERT(data_);

    reset(size);
}

void StandardColor::reset(unsigned size)
{
    colors_ = ColorVector(size);
    RGBA elem;

    double dsize = size;

    for (unsigned int i = 0; i != size; ++i) {
        elem.r = i / dsize;
        elem.g = i / dsize / 4;
        elem.b = 1 - i / dsize;
        elem.a = 1.0;
        colors_[i] = elem;
    }
}

/**
        Assigns a new ColorVector (Also overwrites the constructors size argument)
*/
void StandardColor::setColorVector(ColorVector const &cv)
{
    colors_ = cv;
}

void StandardColor::setAlpha(double a)
{
    if (a < 0 || a > 1)
        return;

    RGBA elem;

    for (unsigned int i = 0; i != colors_.size(); ++i) {
        elem = colors_[i];
        elem.a = a;
        colors_[i] = elem;
    }
}

RGBA StandardColor::operator()(double, double, double z) const
{
    Q_ASSERT(data_);
    if (colors_.empty())
        return RGBA(0.0, 0.0, 0.0, 1.0);
    const double minZ = data_->hull().minVertex.z;
    const double dz = data_->hull().maxVertex.z - minZ;
    if (!(dz > 0.0) || !std::isfinite(dz) || !std::isfinite(z))
        return colors_[0];
    const double t = (z - minZ) / dz;
    const int maxIdx = static_cast<int>(colors_.size() - 1);
    int index = (t <= 0.0) ? 0 : ((t >= 1.0) ? maxIdx : static_cast<int>(maxIdx * t));
    return colors_[index];
}
