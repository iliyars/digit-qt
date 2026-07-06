/**
 * @file VisibilityChecker.cpp
 * @brief Implementation of VisibilityChecker class
 */

#include "visibility/VisibilityChecker.h"

namespace aperture {

VisibilityChecker::VisibilityChecker(const ShapeCollection& shapes)
    : shapes_(shapes)
    , stats_{}
{
}

bool VisibilityChecker::isVisible(const Point& point) const {
    stats_.totalChecks++;
    
    // Step 1: Check INTERNAL shapes first (early exit optimization)
    // Point inside ANY INTERNAL ? always blocked (final veto)
    const auto& internals = shapes_.getInternal();
    for (const auto& shape : internals) {
        stats_.internalChecks++;
        if (shape->isInside(point)) {
            // Inside INTERNAL ? blocked (even APERTURE can't override)
            stats_.earlyExits++;
            return false;
        }
    }
    
    // Step 2: Initial visibility state
    // visible = true if any EXTERNAL exists, false otherwise
    bool visible = shapes_.hasAnyExternal();
    
    // Step 3: Check EXTERNAL shapes (apertures)
    // Point must be INSIDE ALL EXTERNAL shapes (intersection required)
    // If outside any EXTERNAL, set visible=false but continue (APERTURE can override)
    const auto& externals = shapes_.getExternal();
    for (const auto& shape : externals) {
        stats_.externalChecks++;
        if (!shape->isInside(point)) {
            // Outside any EXTERNAL ? blocked (but APERTURE can still open)
            visible = false;
            break;
            // Don't return here! Continue to check APERTURE shapes
        }
    }
    
    // Step 4: Check APERTURE shapes (openings)
    // Point inside ANY APERTURE ? force visible (union)
    // This can override the visible=false from EXTERNAL check
    if (!visible) {
        const auto& apertures = shapes_.getApertures();
        for (const auto& shape : apertures) {
            stats_.apertureChecks++;
            if (shape->isInside(point)) {
                // Inside APERTURE ? force visible (overrides EXTERNAL blocking)
                visible = true;
                stats_.earlyExits++;
                break;  // Early exit optimization - APERTURE found
            }
        }
    } else
        stats_.earlyExits++;

    
    return visible;
}

std::vector<bool> VisibilityChecker::checkPoints(const std::vector<Point>& points) const {
    std::vector<bool> results;
    results.reserve(points.size());
    
    for (const auto& point : points) {
        results.push_back(isVisible(point));
    }
    
    return results;
}

void VisibilityChecker::resetStats() {
    stats_ = Stats{};
}

Bounds VisibilityChecker::getVisibleRegion() const {
    return shapes_.getVisibleRegion();
}

} // namespace aperture
