/**
 * @file ShapeCollection.cpp
 * @brief Implementation of ShapeCollection class
 */
#include "visibility/ShapeCollection.h"

#include <algorithm>

namespace aperture {

void ShapeCollection::addShape(std::unique_ptr<Shape> shape) {
  if (!shape)
    return;

  TypeLimits type = shape->getTypeLimits();

  switch (type) {
    case TypeLimits::EXTERNAL:
      external_.push_back(std::move(shape));
      break;
    case TypeLimits::INTERNAL:
      internal_.push_back(std::move(shape));
      break;
    case TypeLimits::APERTURE:
      apertures_.push_back(std::move(shape));
      break;
  }
  version_++;
}

void ShapeCollection::addExternal(std::unique_ptr<Shape> shape) {
  if (!shape)
    return;
  shape->setTypeLimits(TypeLimits::EXTERNAL);
  external_.push_back(std::move(shape));
  version_++;
}

void ShapeCollection::addInternal(std::unique_ptr<Shape> shape) {
  if (!shape)
    return;
  shape->setTypeLimits(TypeLimits::INTERNAL);
  internal_.push_back(std::move(shape));
  version_++;
}

void ShapeCollection::addAperture(std::unique_ptr<Shape> shape) {
  if (!shape)
    return;
  shape->setTypeLimits(TypeLimits::APERTURE);
  apertures_.push_back(std::move(shape));
  version_++;
}

Bounds ShapeCollection::getCombinedBounds() const {
  if (isEmpty()) {
    return Bounds{};
  }

  Bounds combined = Bounds::infinite();
  bool hasAny = false;

  // Process all EXTERNAL shapes
  for (const auto &shape : external_) {
    if (hasAny) {
      combined.merge(shape->getBounds());
    } else {
      combined = shape->getBounds();
      hasAny = true;
    }
  }

  // Process all INTERNAL shapes
  for (const auto &shape : internal_) {
    if (hasAny) {
      combined.merge(shape->getBounds());
    } else {
      combined = shape->getBounds();
      hasAny = true;
    }
  }

  // Process all APERTURE shapes
  for (const auto &shape : apertures_) {
    if (hasAny) {
      combined.merge(shape->getBounds());
    } else {
      combined = shape->getBounds();
      hasAny = true;
    }
  }

  return combined;
}

Bounds ShapeCollection::getVisibleRegion() const {
  // Priority 1: EXTERNAL shapes define the primary visible region
  // Visible region is the INTERSECTION of all EXTERNAL bounds
  if (!external_.empty()) {
    // Start with first EXTERNAL shape's bounds
    Bounds roi = external_[0]->getBounds();

    // Intersect with all other EXTERNAL shapes
    for (size_t i = 1; i < external_.size(); ++i) {
      roi = roi.intersection(external_[i]->getBounds());

      // Early exit if intersection becomes empty
      if (roi.isEmpty()) {
        return Bounds{};
      }
    }

    // NOTE: We return the EXTERNAL intersection as the ROI.
    // INTERNAL obstructions within this ROI require per-point checking.
    // This ROI is a conservative bound - all visible points are within it,
    // but not all points within it are necessarily visible (due to INTERNAL).

    return roi;
  }

  // Priority 2: If no EXTERNAL but have APERTURE shapes
  // Visible region is the UNION of all APERTURE bounds
  if (!apertures_.empty()) {
    Bounds roi = apertures_[0]->getBounds();

    // Merge with all other APERTURE shapes
    for (size_t i = 1; i < apertures_.size(); ++i) {
      roi.merge(apertures_[i]->getBounds());
    }

    return roi;
  }

  // No visibility-defining shapes (only INTERNAL obstructions)
  // No visible region exists
  return Bounds{};
}

void ShapeCollection::clear() {
  external_.clear();
  internal_.clear();
  apertures_.clear();
  version_++;
}

size_t ShapeCollection::countByType(TypeLimits type) const {
  switch (type) {
    case TypeLimits::EXTERNAL:
      return external_.size();
    case TypeLimits::INTERNAL:
      return internal_.size();
    case TypeLimits::APERTURE:
      return apertures_.size();
    default:
      return 0;
  }
}

}  // namespace aperture
