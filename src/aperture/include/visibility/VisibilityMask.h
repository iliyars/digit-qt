#pragma once
#include <cstdint>
#include <vector>

namespace aperture {
namespace visibility {

/**
 * @brief Pure data container for pixel visibility
 *
 * Stores per-pixel visibility for a bitmap with version metadata
 * for cache validation. Uses flat storage for efficiency.
 *
 * Rules:
 * - No geometry logic
 * - No rebuild logic
 * - No threading
 * - No dependency on UI or documents
 */
class VisibilityMask {
public:
  int width;
  int height;

  // 0 = invisible, 1 = visible (flat storage, not vector<vector<bool>>)
  std::vector<uint8_t> data;

  // Version stamps for cache validation
  uint64_t shapeVersion;
  uint64_t imageVersion;

  /**
   * @brief Query pixel visibility
   * @param x Pixel x coordinate
   * @param y Pixel y coordinate
   * @return true if visible, false if invisible or out of bounds
   */
  bool IsVisible(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) {
      return false;
    }
    int index = y * width + x;
    return data[index] != 0;
  }

  /**
   * @brief Default constructor creates empty mask
   */
  VisibilityMask() : width(0), height(0), shapeVersion(0), imageVersion(0) {}

  /**
   * @brief Construct mask with specified dimensions
   * @param w Width in pixels
   * @param h Height in pixels
   */
  VisibilityMask(int w, int h)
      : width(w),
        height(h),
        data(w * h, 0)  // Initialize all pixels as invisible
        ,
        shapeVersion(0),
        imageVersion(0) {}
};

}  // namespace visibility
}  // namespace aperture
