/**
 * @file TypeLimits.cpp
 * @brief Implementation of TypeLimits utilities
 */

#include "visibility/TypeLimits.h"
#include <stdexcept>
#include <cstring>

namespace aperture {

TypeLimits fromString(const char* str) {
    if (std::strcmp(str, "EXTERNAL") == 0) {
        return TypeLimits::EXTERNAL;
    }
    if (std::strcmp(str, "INTERNAL") == 0) {
        return TypeLimits::INTERNAL;
    }
    if (std::strcmp(str, "APERTURE") == 0) {
        return TypeLimits::APERTURE;
    }
    
    throw std::invalid_argument(std::string("Unknown TypeLimits value: ") + str);
}

} // namespace aperture
