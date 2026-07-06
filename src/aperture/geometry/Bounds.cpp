/**
 * @file Bounds.cpp
 * @brief Implementation of Bounds class
 */

#include "geometry/Bounds.h"
#include <ostream>
#include <iomanip>

namespace aperture {

std::ostream& operator<<(std::ostream& os, const Bounds& b) {
    os << "Bounds(" << std::fixed << std::setprecision(2)
       << "left=" << b.left << ", top=" << b.top
       << ", right=" << b.right << ", bottom=" << b.bottom
       << ", width=" << b.width() << ", height=" << b.height() << ")";
    return os;
}

} // namespace aperture
