/**
 * @file Point.cpp
 * @brief Implementation of Point class
 */

#include "geometry/Point.h"
#include <ostream>
#include <iomanip>

namespace aperture {

std::ostream& operator<<(std::ostream& os, const Point& p) {
    os << "Point(" << std::fixed << std::setprecision(6)
       << p.x << ", " << p.y << ")";
    return os;
}

} // namespace aperture
