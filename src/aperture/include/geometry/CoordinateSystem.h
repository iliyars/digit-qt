/**
 * @file CoordinateSystem.h
 * @brief Coordinate system definitions and conversion utilities
 *
 * Handles the distinction between screen/image coordinates (Y+ downward)
 * and mathematical coordinates (Y+ upward). Critical for correct geometric
 * operations when working with both bitmap images and mathematical shapes.
 */
#pragma once

#include <cmath>
#include <stdexcept>

#ifdef min
#undef min  // Ensure std::min is used, not a macro
#endif
#ifdef max
#undef max  // Ensure std::max is used, not a macro
#endif

namespace aperture {

/**
 * @brief Coordinate system type
 */
enum class CoordinateSystemType {
  /**
   * @brief Screen/Image coordinates (left-handed)
   *
   * Convention:
   * - Origin: Top-left corner
   * - X-axis: Left to right (positive →)
   * - Y-axis: Top to bottom (positive ↓)
   * - Rotation: Clockwise is positive
   * - Bounds: top < bottom
   *
   * Used by:
   * - Bitmap images
   * - UI elements
   * - Image processing
   * - Legacy XYShape classes
   *
   * Visual:
   * ```
   * (0,0) -------- X+ →
   *   |
   *   | Y+ ↓
   *   |
   *   V
   * ```
   */
  SCREEN,

  /**
   * @brief Mathematical coordinates (right-handed)
   *
   * Convention:
   * - Origin: Arbitrary (often center or bottom-left)
   * - X-axis: Left to right (positive →)
   * - Y-axis: Bottom to top (positive ↑)
   * - Rotation: Counter-clockwise is positive
   * - Bounds: bottom < top
   *
   * Used by:
   * - Geometry calculations
   * - Wavefront analysis
   * - Mathematical models
   * - Some file formats
   *
   * Visual:
   * ```
   *   Y+ ↑
   *   |
   *   |
   *   |
   * (0,0) -------- X+ →
   * ```
   */
  MATH
};

/**
 * @brief Coordinate system context with conversion support
 *
 * Encapsulates both the coordinate system type and the reference height
 * needed for Y-axis conversions between screen and math coordinates.
 *
 * ## Usage
 *
 * ### Creating Context
 * ```cpp
 * // Screen coordinates for 1024×768 image
 * CoordinateSystem screenSys = CoordinateSystem::screen(768.0);
 *
 * // Mathematical coordinates (no height needed for pure math)
 * CoordinateSystem mathSys = CoordinateSystem::math();
 *
 * // With reference height for conversions
 * CoordinateSystem mathSys = CoordinateSystem::math(768.0);
 * ```
 *
 * ### Converting Points
 * ```cpp
 * Point screenPt{100.0, 50.0};  // Near top in screen coords
 *
 * // Convert to math coords (assuming 768px height)
 * Point mathPt = screenSys.convertPoint(screenPt, mathSys);
 * // mathPt = {100.0, 718.0}  // Near top in math coords
 * ```
 *
 * ### Converting Angles
 * ```cpp
 * double screenAngle = 45.0;  // 45° clockwise in screen
 * double mathAngle = screenSys.convertAngle(screenAngle, mathSys);
 * // mathAngle = -45.0  // 45° counter-clockwise in math
 * ```
 */
class CoordinateSystem {
public:
  /**
   * @brief Create screen coordinate system
   * @param referenceHeight Image height in pixels (for conversions)
   * @return Screen coordinate system context
   */
  static CoordinateSystem screen(double referenceHeight = 0.0) {
    return CoordinateSystem(CoordinateSystemType::SCREEN, referenceHeight);
  }

  /**
   * @brief Create mathematical coordinate system
   * @param referenceHeight Reference height (for conversions to screen)
   * @return Math coordinate system context
   */
  static CoordinateSystem math(double referenceHeight = 0.0) {
    return CoordinateSystem(CoordinateSystemType::MATH, referenceHeight);
  }

  /**
   * @brief Get coordinate system type
   */
  CoordinateSystemType type() const { return type_; }

  /**
   * @brief Get reference height
   */
  double referenceHeight() const { return referenceHeight_; }

  /**
   * @brief Set reference height
   * @param height New reference height
   */
  void setReferenceHeight(double height) { referenceHeight_ = height; }

  /**
   * @brief Check if coordinate system is screen type
   */
  bool isScreen() const { return type_ == CoordinateSystemType::SCREEN; }

  /**
   * @brief Check if coordinate system is math type
   */
  bool isMath() const { return type_ == CoordinateSystemType::MATH; }

  /**
   * @brief Check if two coordinate systems are compatible (same type)
   */
  bool isCompatible(const CoordinateSystem &other) const {
    return type_ == other.type_;
  }

  /**
   * @brief Convert Y coordinate to another system
   * @param y Y coordinate in this system
   * @param targetSystem Target coordinate system
   * @return Y coordinate in target system
   *
   * @throws std::invalid_argument if referenceHeight is not set
   *
   * @code{.cpp}
   * CoordinateSystem screen = CoordinateSystem::screen(768.0);
   * CoordinateSystem math = CoordinateSystem::math(768.0);
   *
   * double screenY = 50.0;  // Near top in screen coords
   * double mathY = screen.convertY(screenY, math);
   * // mathY = 718.0  // Near top in math coords (768 - 50)
   * @endcode
   */
  double convertY(double y, const CoordinateSystem &targetSystem) const {
    if (type_ == targetSystem.type_) {
      return y;  // Same system, no conversion
    }

    // Different systems: flip Y-axis around reference height
    double height = getReferenceHeightForConversion(targetSystem);
    return height - y;
  }

  /**
   * @brief Convert angle to another system
   * @param angleRadians Angle in radians in this system
   * @param targetSystem Target coordinate system
   * @return Angle in radians in target system
   *
   * Screen to Math (or vice versa): negates angle
   * - Screen: CW is positive → Math: CCW is positive
   * - Math: CCW is positive → Screen: CW is positive
   *
   * @code{.cpp}
   * CoordinateSystem screen = CoordinateSystem::screen();
   * CoordinateSystem math = CoordinateSystem::math();
   *
   * double screenAngle = M_PI / 4;  // 45° CW in screen
   * double mathAngle = screen.convertAngle(screenAngle, math);
   * // mathAngle = -?/4  // 45° CCW in math
   * @endcode
   */
  double convertAngle(double angleRadians,
                      const CoordinateSystem &targetSystem) const {
    if (type_ == targetSystem.type_) {
      return angleRadians;  // Same system, no conversion
    }

    // Different systems: negate angle (flip rotation direction)
    return -angleRadians;
  }

  /**
   * @brief Check if bounds are valid for this coordinate system
   * @param left Left edge
   * @param top Top edge
   * @param right Right edge
   * @param bottom Bottom edge
   * @return true if bounds are valid
   *
   * Validation rules:
   * - SCREEN: left ? right AND top ? bottom (top is smaller Y)
   * - MATH: left ? right AND bottom ? top (bottom is smaller Y)
   */
  bool areBoundsValid(double left, double top, double right,
                      double bottom) const {
    if (type_ == CoordinateSystemType::SCREEN) {
      return left <= right && top <= bottom;
    } else {
      return left <= right && bottom <= top;
    }
  }

  /**
   * @brief Get the "minimum" Y value for this coordinate system
   * @param y1 First Y value
   * @param y2 Second Y value
   * @return Minimum Y in this system's convention
   *
   * - SCREEN: min is top (smaller number)
   * - MATH: min is bottom (smaller number)
   */
  double minY(double y1, double y2) const {
    return std::min(y1, y2);  // Same for both systems (smaller value)
  }

  /**
   * @brief Get the "maximum" Y value for this coordinate system
   * @param y1 First Y value
   * @param y2 Second Y value
   * @return Maximum Y in this system's convention
   *
   * - SCREEN: max is bottom (larger number)
   * - MATH: max is top (larger number)
   */
  double maxY(double y1, double y2) const {
    return std::max(y1, y2);  // Same for both systems (larger value)
  }

  /**
   * @brief Equality comparison
   */
  bool operator==(const CoordinateSystem &other) const {
    return type_ == other.type_ && referenceHeight_ == other.referenceHeight_;
  }

  /**
   * @brief Inequality comparison
   */
  bool operator!=(const CoordinateSystem &other) const {
    return !(*this == other);
  }

private:
  /**
   * @brief Private constructor
   */
  CoordinateSystem(CoordinateSystemType type, double referenceHeight)
      : type_(type), referenceHeight_(referenceHeight) {}

  /**
   * @brief Get reference height for conversion
   * @throws std::invalid_argument if height is not set (0.0)
   */
  double getReferenceHeightForConversion(
      const CoordinateSystem &targetSystem) const {
    // Use target system's height if available, otherwise use ours
    double height = (targetSystem.referenceHeight_ > 0.0)
                        ? targetSystem.referenceHeight_
                        : referenceHeight_;

    if (height <= 0.0) {
      throw std::invalid_argument(
          "CoordinateSystem: referenceHeight must be set for Y-axis "
          "conversion. "
          "Use setReferenceHeight() or provide height in constructor.");
    }

    return height;
  }

  CoordinateSystemType type_;
  double referenceHeight_;  ///< Reference height for Y-axis conversions (image
                            ///< height in pixels)
};

}  // namespace aperture
