#pragma once
#include "IDataProviders.h"
#include "ShapeCollection.h"
#include "VisibilityMask.h"
#include "VisibilityMaskBuilder.h"

#include <cstdint>

namespace aperture {
namespace visibility {

/**
 * @brief Single access point for valid visibility masks
 *
 * Owns cached mask, tracks staleness, rebuilds lazily on demand.
 * This is the ONLY authority on mask validity.
 *
 * Responsibilities:
 * - Own the cached VisibilityMask
 * - Track whether it is stale
 * - Rebuild lazily on demand
 * - Hide all complexity from callers
 *
 * Must NOT:
 * - Expose dirty flags
 * - Require callers to invalidate
 * - Depend on UI or editor tools
 *
 * Design invariant:
 * Every call to GetMask() returns a fresh, valid mask.
 */
class VisibilityMaskProvider {
public:
  /**
   * @brief Construct provider with dependencies
   * @param shapes Reference to shape collection (not owned)
   * @param imageWidth Current image width
   * @param imageHeight Current image height
   */
  VisibilityMaskProvider(const ShapeCollection &shapes,
                         const IImageData &imageProvider)
      : shapes_(shapes),
        m_imageProvider_(imageProvider),
        imageWidth_(imageProvider.GetWidth()),
        imageHeight_(imageProvider.GetHeight()),
        imageVersion_(
            imageProvider.GetImageVersion())  // Start at 1, 0 means invalid
        ,
        hasMask_(false) {}

  /**
   * @brief Get current valid mask (rebuilds if stale)
   * @return Reference to valid cached mask
   *
   * Internal logic:
   * 1. Query ShapeCollection::GetVersion()
   * 2. Compare with cached version stamps
   * 3. If mismatch OR no mask exists: rebuild using VisibilityMaskBuilder
   * 4. Return cached mask
   *
   * Guarantees: Every call returns a fresh, valid mask
   */
  const VisibilityMask &getMask() {
    uint64_t currentShapeVer = shapes_.getVersion();
    imageWidth_ = m_imageProvider_.GetWidth();
    imageHeight_ = m_imageProvider_.GetHeight();
    imageVersion_ = m_imageProvider_.GetImageVersion();

    // Check if rebuild needed
    bool needRebuild = !hasMask_ ||
                       (cachedMask_.shapeVersion != currentShapeVer) ||
                       (cachedMask_.imageVersion != imageVersion_) ||
                       (cachedMask_.width != imageWidth_) ||
                       (cachedMask_.height != imageHeight_);

    if (needRebuild) {
      // Rebuild mask
      cachedMask_ = VisibilityMaskBuilder::Build(shapes_, imageWidth_,
                                                 imageHeight_, imageVersion_);
      hasMask_ = true;
    }

    return cachedMask_;
  }

  /**
   * @brief Update image dimensions (triggers rebuild on next GetMask)
   * @param width New image width
   * @param height New image height
   */
  void UpdateImageSize(int width, int height) {
    if (imageWidth_ != width || imageHeight_ != height) {
      imageWidth_ = width;
      imageHeight_ = height;
      imageVersion_++;  // Increment version to trigger rebuild
    }
  }

  /**
   * @brief Explicitly increment image version (e.g., after image reload)
   *
   * Call this after:
   * - Image load
   * - Image resize
   * - Image reallocation
   */
  void InvalidateImage() { imageVersion_++; }
  void Invalidate() { hasMask_ = false; }

private:
  // Dependencies (not owned)
  const ShapeCollection &shapes_;

  // Image state
  const IImageData &m_imageProvider_;
  int imageWidth_;
  int imageHeight_;
  uint64_t imageVersion_;

  // Cached mask
  VisibilityMask cachedMask_;
  bool hasMask_ = false;  // isValid flag (true if cachedMask_ is valid)
};

}  // namespace visibility
}  // namespace aperture
