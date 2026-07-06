/**
 * @file Rectangle.cpp
 * @brief Implementation of Rectangle class
 */

#include "geometry/Rectangle.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace aperture {

Rectangle::Rectangle(double width, double height,
                     double centerX, double centerY,
                     double rotationDegrees,
                     TypeLimits typeLimits,
                     CoordinateSystem spatialSystem,
                     NormalizationState normState)
    : width_(width)
    , height_(height)
    , center_(centerX, centerY)
    , rotationDeg_(rotationDegrees)
    , rotationRad_(rotationDegrees * M_PI / 180.0)
    , cosRot_(0.0)
    , sinRot_(0.0)
{
    typeLimits_ = typeLimits;
    spatialSystem_ = spatialSystem;
    normState_ = normState;
    updateRotationCache();
}

Rectangle::Rectangle(
    const Point& p0,
    const Point& p1,
    const Point& p2,
    TypeLimits typeLimits,
    CoordinateSystem spatialSystem,
    NormalizationState normState)
    : cosRot_(0.0)
    , sinRot_(0.0)
{
    // 1. Compute width vector (p0 → p1)
    double vWidthX = p1.x - p0.x;
    double vWidthY = p1.y - p0.y;

    // 2. Width magnitude
    width_ = std::sqrt(vWidthX * vWidthX + vWidthY * vWidthY);

    // 3. Normalize width vector
    double widthLen = width_;
    if (widthLen < 1e-10) {
        widthLen = 1.0;  // Degenerate case: use unit vector
    }
    double uWidthX = vWidthX / widthLen;
    double uWidthY = vWidthY / widthLen;

    // 4. Perpendicular vector (rotate width vector 90° counter-clockwise)
    double uPerpX = -uWidthY;
    double uPerpY = uWidthX;

    // 5. Project p2 onto perpendicular to get height
    double vP2X = p2.x - p0.x;
    double vP2Y = p2.y - p0.y;
    double heightProjection = vP2X * uPerpX + vP2Y * uPerpY;
    height_ = std::abs(heightProjection);

    // 6. Determine height direction (sign of projection)
    double heightSign = (heightProjection >= 0.0) ? 1.0 : -1.0;

    // 7. Compute center
    // center = p0 + 0.5 * width_vector + 0.5 * height * perp_direction
    center_.x = p0.x + 0.5 * vWidthX + 0.5 * height_ * heightSign * uPerpX;
    center_.y = p0.y + 0.5 * vWidthY + 0.5 * height_ * heightSign * uPerpY;

    // 8. Compute rotation angle from width vector
    rotationRad_ = std::atan2(uWidthY, uWidthX);
    rotationDeg_ = rotationRad_ * 180.0 / M_PI;

    // 9. Set type and coordinate system
    typeLimits_ = typeLimits;
    spatialSystem_ = spatialSystem;
    normState_ = normState;

    // 10. Update cached trig values
    updateRotationCache();
}

void Rectangle::updateRotationCache() {
    cosRot_ = std::cos(rotationRad_);
    sinRot_ = std::sin(rotationRad_);
}

Point Rectangle::toLocalCoordinates(const Point& point) const {
    // Translate to origin
    double dx = point.x - center_.x;
    double dy = point.y - center_.y;

    // Rotate by -rotation to align with axes
    return Point{
        dx * cosRot_ + dy * sinRot_,
        -dx * sinRot_ + dy * cosRot_
    };
}

bool Rectangle::isInside(const Point& point) const {
    // Transform to rectangle-local coordinates
    Point local = toLocalCoordinates(point);

    // Check if within rectangle bounds
    double halfWidth = width_ / 2.0;
    double halfHeight = height_ / 2.0;

    return (std::abs(local.x) <= halfWidth) &&
           (std::abs(local.y) <= halfHeight);
}

Bounds Rectangle::getBounds() const {
    auto cornerPts = corners();

    Bounds bounds = Bounds::infinite();
    bounds.clear();

    for (const auto& corner : cornerPts) {
        bounds.expand(corner);
    }

    return bounds;
}

std::array<Point, 4> Rectangle::corners() const {
    double halfW = width_ / 2.0;
    double halfH = height_ / 2.0;

    // Corners in local coordinates (before rotation)
    std::array<Point, 4> localCorners = {{
        {-halfW, -halfH},  // Top-left
        { halfW, -halfH},  // Top-right
        { halfW,  halfH},  // Bottom-right
        {-halfW,  halfH}   // Bottom-left
    }};

    // Transform to world coordinates
    std::array<Point, 4> worldCorners;
    for (size_t i = 0; i < 4; ++i) {
        double x = localCorners[i].x * cosRot_ - localCorners[i].y * sinRot_;
        double y = localCorners[i].x * sinRot_ + localCorners[i].y * cosRot_;
        worldCorners[i] = Point{x + center_.x, y + center_.y};
    }

    return worldCorners;
}

std::vector<Point> Rectangle::getContour(double stepSize) const {
    // For rectangles, stepSize determines points along edges
    auto cornerPts = corners();

    std::vector<Point> contour;

    // Calculate points per edge based on edge length and stepSize
    for (size_t i = 0; i < 4; ++i) {
        const Point& start = cornerPts[i];
        const Point& end = cornerPts[(i + 1) % 4];

        double edgeLength = start.distanceTo(end);
        int numSegments = std::max(1, static_cast<int>(edgeLength / stepSize));

        // Add points along this edge
        for (int j = 0; j < numSegments; ++j) {
            double t = static_cast<double>(j) / numSegments;
            contour.push_back(lerp(start, end, t));
        }
    }

    // Close the contour
    contour.push_back(cornerPts[0]);

    return contour;
}

bool Rectangle::isOnContour(const Point& point, double tolerance) const {
    // Transform to rectangle-local coordinates
    Point local = toLocalCoordinates(point);

    // Check if point is inside enlarged rectangle (sides + tolerance)
    double enlargedHalfWidth = (width_ / 2.0) + tolerance;
    double enlargedHalfHeight = (height_ / 2.0) + tolerance;

    if (std::abs(local.x) > enlargedHalfWidth || std::abs(local.y) > enlargedHalfHeight) {
        return false;  // Outside enlarged rectangle - too far from contour
    }

    // Check if point is outside diminished rectangle (sides - tolerance)
    double diminishedHalfWidth = std::max(0.0, (width_ / 2.0) - tolerance);
    double diminishedHalfHeight = std::max(0.0, (height_ / 2.0) - tolerance);

    // If diminished rectangle is degenerate (tolerance >= half-width or half-height),
    // and we're inside enlarged, then we're on the contour
    if (diminishedHalfWidth < 1e-10 || diminishedHalfHeight < 1e-10) {
        return true;
    }

    // Point is on contour if it's inside enlarged but outside diminished
    return (std::abs(local.x) > diminishedHalfWidth || std::abs(local.y) > diminishedHalfHeight);
}

double Rectangle::perimeter() const {
    return 2.0 * (width_ + height_);
}

double Rectangle::area() const {
    return width_ * height_;
}

std::unique_ptr<Shape> Rectangle::clone() const {
    return std::make_unique<Rectangle>(*this);
}

void Rectangle::normalize(double originX, double originY, double radius) {
    width_ /= radius;
    height_ /= radius;
    center_.x = (center_.x - originX) / radius;
    center_.y = (center_.y - originY) / radius;
    normState_ = NormalizationState::NORMALIZED;
}

void Rectangle::denormalize(double originX, double originY, double radius) {
    width_ *= radius;
    height_ *= radius;
    center_.x = center_.x * radius + originX;
    center_.y = center_.y * radius + originY;
    normState_ = NormalizationState::MEASURING;
}

void Rectangle::inverseY(double centerY) {
    center_.y = centerY - center_.y;
    rotationDeg_ = -rotationDeg_;
    rotationRad_ = -rotationRad_;
    updateRotationCache();
}

void Rectangle::shiftX(double deltaX) {
    center_.x += deltaX;
}

void Rectangle::shiftY(double deltaY) {
    center_.y += deltaY;
}

// ========================================================================
// Handle Enumeration (Interactive Editing - UX spec §3)
// ========================================================================

void Rectangle::EnumerateHandles(std::vector<HandleDesc>& out) const {
    // §2.1, §3.2 - Move handle at centroid
    out.push_back(HandleDesc{HandleType::Move, -1, center_});

    // §2.2, §3.3 - Rotation handle offset along local +Y axis
    // Distance = max(boundsRadius * 0.2, minimum in world coords)
    double boundsRadius = std::sqrt(width_ * width_ + height_ * height_) / 2.0;
    double rotHandleOffset = std::max(boundsRadius * 0.2, 20.0); // 20.0 = min offset

    // Local +Y in shape space → rotate by current angle
    Point rotHandlePos{
        center_.x - rotHandleOffset * sinRot_,  // -sin for +Y rotation
        center_.y + rotHandleOffset * cosRot_   // +cos for +Y rotation
    };
    out.push_back(HandleDesc{HandleType::Rotate, -1, rotHandlePos});

    // §3.1 - 4 Corner resize handles
    std::array<Point, 4> cornerPts = corners();
    for (int i = 0; i < 4; ++i) {
        // Normal = direction from center to corner
        Point normal{
            cornerPts[i].x - center_.x,
            cornerPts[i].y - center_.y
        };
        // Normalize the normal vector
        double len = std::sqrt(normal.x * normal.x + normal.y * normal.y);
        if (len > 1e-6) {
            normal.x /= len;
            normal.y /= len;
        }
        out.push_back(HandleDesc{HandleType::CornerResize, i, cornerPts[i], normal});
    }

    // §3.1 - 4 Edge midpoint resize handles
    for (int i = 0; i < 4; ++i) {
        // Edge midpoint = average of two adjacent corners
        int nextIdx = (i + 1) % 4;
        Point edgeMid{
            (cornerPts[i].x + cornerPts[nextIdx].x) / 2.0,
            (cornerPts[i].y + cornerPts[nextIdx].y) / 2.0
        };

        // Normal = perpendicular to edge (outward)
        Point edge{
            cornerPts[nextIdx].x - cornerPts[i].x,
            cornerPts[nextIdx].y - cornerPts[i].y
        };
        // Perpendicular (rotate 90° counter-clockwise)
        Point normal{-edge.y, edge.x};
        double len = std::sqrt(normal.x * normal.x + normal.y * normal.y);
        if (len > 1e-6) {
            normal.x /= len;
            normal.y /= len;
        }

        out.push_back(HandleDesc{HandleType::EdgeResize, i, edgeMid, normal});
    }
}

void Rectangle::ApplyHandleDrag(const HandleDesc& handle, const DragContext& drag) {
    switch (handle.type) {
        case HandleType::Move:
            // Simple translation
            center_.x += drag.deltaWorld.x;
            center_.y += drag.deltaWorld.y;
            break;

        case HandleType::Rotate: {
            // Calculate angle from center to current drag position
            double dx = drag.dragCurrentWorld.x - center_.x;
            double dy = drag.dragCurrentWorld.y - center_.y;
            double angleRad = std::atan2(dy, dx);  // atan2(x,y) for +Y = up in shape space

            double angleDeg = angleRad * 180.0 / M_PI;

            // Apply snap if Shift is pressed (15° increments per review)
            if (drag.shiftKey) {
                double snapStep = 15.0;
                angleDeg = std::round(angleDeg / snapStep) * snapStep;
            }

            rotationDeg_ = angleDeg;
            rotationRad_ = angleDeg * M_PI / 180.0;
            updateRotationCache();
            break;
        }

        case HandleType::CornerResize: {
            // Transform drag position to local (rectangle-aligned) coordinates
            Point localPos = toLocalCoordinates(drag.dragCurrentWorld);

            // Opposite corner stays fixed (corners: 0↔2, 1↔3)
            int oppositeIdx = (handle.index + 2) % 4;

            // Get opposite corner local position
            double halfW = width_ / 2.0;
            double halfH = height_ / 2.0;
            double oppX = (oppositeIdx == 1 || oppositeIdx == 2) ? halfW : -halfW;
            double oppY = (oppositeIdx == 2 || oppositeIdx == 3) ? halfH : -halfH;

            // New bounding box from dragged corner to opposite corner
            double minX = std::min(localPos.x, oppX);
            double maxX = std::max(localPos.x, oppX);
            double minY = std::min(localPos.y, oppY);
            double maxY = std::max(localPos.y, oppY);

            // New center in local coords
            double newCenterLocalX = (minX + maxX) / 2.0;
            double newCenterLocalY = (minY + maxY) / 2.0;

            // New dimensions
            double newHalfWidth = (maxX - minX) / 2.0;
            double newHalfHeight = (maxY - minY) / 2.0;

            // Convert center change from local to world space
            double deltaWorldX = newCenterLocalX * cosRot_ - newCenterLocalY * sinRot_;
            double deltaWorldY = newCenterLocalX * sinRot_ + newCenterLocalY * cosRot_;

            center_.x += deltaWorldX;
            center_.y += deltaWorldY;
            width_ = std::max(newHalfWidth * 2.0, 1.0);
            height_ = std::max(newHalfHeight * 2.0, 1.0);
            break;
        }

        case HandleType::EdgeResize: {
            // Transform drag position to local (rectangle-aligned) coordinates
            Point localPos = toLocalCoordinates(drag.dragCurrentWorld);

            double halfW = width_ / 2.0;
            double halfH = height_ / 2.0;

            // Determine edge orientation: 0,2 = horizontal (top/bottom), 1,3 = vertical (right/left)
            bool isHorizontalEdge = (handle.index == 0 || handle.index == 2);

            if (isHorizontalEdge) {
                // Dragging top (0) or bottom (2): opposite y stays fixed
                double oppY = (handle.index == 0) ? halfH : -halfH;

                double minY = std::min(localPos.y, oppY);
                double maxY = std::max(localPos.y, oppY);
                double newCenterLocalY = -(minY + maxY) / 2.0;
                double newHalfHeight = (maxY - minY) / 2.0;

                // Transform center shift from local to world
                double deltaWorldX = newCenterLocalY * sinRot_;
                double deltaWorldY = -newCenterLocalY * cosRot_;

                center_.x += deltaWorldX;
                center_.y += deltaWorldY;
                height_ = std::max(newHalfHeight * 2.0, 1.0);
            }
            else {
                // Dragging right (1) or left (3): opposite x stays fixed
                double oppX = (handle.index == 1) ? -halfW : halfW;

                double minX = std::min(localPos.x, oppX);
                double maxX = std::max(localPos.x, oppX);
                double newCenterLocalX = (minX + maxX) / 2.0;
                double newHalfWidth = (maxX - minX) / 2.0;

                // Transform center shift from local to world
                double deltaWorldX = newCenterLocalX * cosRot_;
                double deltaWorldY = newCenterLocalX * sinRot_;

                center_.x += deltaWorldX;
                center_.y += deltaWorldY;
                width_ = std::max(newHalfWidth * 2.0, 1.0);
            }
            break;
        }

        default:
            break;
    }
}

} // namespace aperture
