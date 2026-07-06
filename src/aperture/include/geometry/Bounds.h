/**
 * @file Bounds.h
 * @brief Axis-aligned bounding box for 2D shapes
 * 
 * ## Key Features
 * - Axis-aligned rectangle representation
 * - Factory methods for various construction patterns
 * - Containment and intersection queries
 * - Expansion and merging operations
 * - Point clamping and boundary calculations
 * - **Coordinate system awareness (SCREEN vs MATH)**
 * - **System-agnostic min/max accessors (recommended)**
 * 
 * ## Coordinate Convention & Naming
 * 
 * Bounds uses a **hybrid naming approach** to support both coordinate systems
 * while maintaining API compatibility:
 * 
 * ### Field Names (Traditional)
 * - `left`, `right` - Always min/max X (unambiguous)
 * - `top`, `bottom` - **Meaning depends on coordinate system**
 * 
 * ### System-Agnostic Accessors (Recommended)
 * - `minX()`, `maxX()` - Always return smaller/larger X
 * - `minY()`, `maxY()` - Always return smaller/larger Y (system-aware)
 * 
 * **Use the accessors for clearer, system-agnostic code!**
 * 
 * ## SCREEN vs MATH Coordinates
 * 
 * ### SCREEN Coordinates (Default)
 * - X-axis: left to right
 * - Y-axis: **top to bottom** (top < bottom)
 * - Origin: typically top-left corner
 * - Used by: bitmaps, images, UI
 * - Field mapping: `top = minY`, `bottom = maxY`
 * 
 * ### MATH Coordinates
 * - X-axis: left to right
 * - Y-axis: **bottom to top** (bottom < top)
 * - Origin: arbitrary
 * - Used by: geometry, wavefront analysis
 * - Field mapping: `top = maxY`, `bottom = minY` ⚠️
 * 
 * ## Usage Examples
 * 
 * ### Recommended: Use fromMinMax() and min/max accessors
 * @code{.cpp}
 * #include <aperturecore/geometry/Bounds.h>
 * 
 * using namespace aperture;
 * 
 * // SCREEN coordinates - clear and intuitive
 * Bounds screen = Bounds::fromMinMax(0, 0, 100, 50);
 * assert(screen.minY() == 0);
 * assert(screen.maxY() == 50);
 * assert(screen.isValid());  // Always works!
 * 
 * // MATH coordinates - same clarity!
 * Bounds math = Bounds::fromMinMax(0, 0, 100, 50, CoordinateSystem::math());
 * assert(math.minY() == 0);   // Always smaller value
 * assert(math.maxY() == 50);  // Always larger value
 * assert(math.isValid());     // Always works!
 * 
 * // System-agnostic validation
 * bool valid = (bounds.minX() <= bounds.maxX() && 
 *               bounds.minY() <= bounds.maxY());
 * @endcode
 * 
 * ### Traditional: Direct field access (when system is known)
 * @code{.cpp}
 * // SCREEN coordinates - traditional constructor
 * Bounds screenBox{0.0, 0.0, 100.0, 50.0};  // left, top, right, bottom
 * assert(screenBox.top == 0);     // top is min_y
 * assert(screenBox.bottom == 50); // bottom is max_y
 * 
 * // MATH coordinates - ⚠️ CONFUSING field order!
 * Bounds mathBox{0.0, 50.0, 100.0, 0.0, CoordinateSystem::math()};
 * // top=50 (max_y), bottom=0 (min_y) - opposite of SCREEN!
 * @endcode
 * 
 * ## Validation
 * 
 * Validation is now system-agnostic thanks to min/max accessors:
 * 
 * @code{.cpp}
 * // OLD (system-dependent, confusing):
 * // SCREEN: top <= bottom
 * // MATH: bottom <= top
 * 
 * // NEW (system-agnostic, clear):
 * bool valid = bounds.isValid();  // minX() <= maxX() && minY() <= maxY()
 * @endcode
 * 
 * @see Point, Shape, CoordinateSystem
 * @see minX(), maxX(), minY(), maxY() - System-agnostic accessors
 * @see fromMinMax() - Recommended factory method
 */
#pragma once

#include "Point.h"
#include "CoordinateSystem.h"
#include <algorithm>
#include <limits>
#include <array>

#ifdef min
#undef min  // Ensure std::min is used, not a macro
#endif
#ifdef max
#undef max  // Ensure std::max is used, not a macro
#endif

namespace aperture {

/**
 * @class Bounds
 * @brief Axis-aligned 2D bounding rectangle
 * 
 * Represents a rectangular region aligned with the coordinate axes. Used for
 * quick spatial queries, collision detection, and defining shape extents.
 * 
 * ## Hybrid Naming Approach
 * 
 * Bounds supports both **traditional field names** (left/top/right/bottom) for
 * compatibility and **system-agnostic accessors** (minX/maxX/minY/maxY) for clarity.
 * 
 * **Recommended:** Use `minX()`, `maxX()`, `minY()`, `maxY()` and `fromMinMax()`
 * for code that works correctly with both coordinate systems.
 * 
 * ### Field Semantics by Coordinate System
 * 
 * | Field | SCREEN Meaning | MATH Meaning |
 * |-------|----------------|--------------|
 * | `left` | min X ✓ | min X ✓ |
 * | `right` | max X ✓ | max X ✓ |
 * | `top` | min Y (top of screen) | **max Y** (top of graph) ⚠️ |
 * | `bottom` | max Y (bottom of screen) | **min Y** (bottom of graph) ⚠️ |
 * 
 * ### System-Agnostic Accessors (Always Clear)
 * 
 * | Accessor | Returns | Works with |
 * |----------|---------|------------|
 * | `minX()` | Smaller X | Both systems ✓ |
 * | `maxX()` | Larger X | Both systems ✓ |
 * | `minY()` | Smaller Y | Both systems ✓ |
 * | `maxY()` | Larger Y | Both systems ✓ |
 * 
 * ### Coordinate System Details
 * 
 * **SCREEN coordinates (default):**
 * ```
 * (left, top) = (minX, minY) -------- (right, top) = (maxX, minY)
 *      |                                   |
 *      |             center                |
 *      |                                   |
 * (left, bottom) = (minX, maxY) -- (right, bottom) = (maxX, maxY)
 * ```
 * - Y+ downward: top < bottom
 * - Validation: `left <= right && top <= bottom`
 * - Equivalent: `minX() <= maxX() && minY() <= maxY()`
 * 
 * **MATH coordinates:**
 * ```
 * (left, top) = (minX, maxY) -------- (right, top) = (maxX, maxY)
 *      |                                   |
 *      |             center                |
 *      |                                   |
 * (left, bottom) = (minX, minY) -- (right, bottom) = (maxX, minY)
 * ```
 * - Y+ upward: bottom < top
 * - Validation: `left <= right && bottom <= top`
 * - Equivalent: `minX() <= maxX() && minY() <= maxY()`
 * 
 * ### Memory Layout
 * ```
 * Bounds b{left, top, right, bottom};
 * sizeof(Bounds) == 40 bytes (4 × sizeof(double) + CoordinateSystem)
 * ```
 * 
 * ### Empty Bounds
 * An empty bounds is represented by all coordinates = 0.0
 * Use `isEmpty()` to check or `clear()` to reset.
 * 
 * ### Usage Recommendations
 * 
 * ✓ **DO:** Use `fromMinMax()` and `minY()/maxY()` for clarity
 * ```cpp
 * Bounds bounds = Bounds::fromMinMax(0, 0, 100, 50, system);
 * if (y >= bounds.minY() && y <= bounds.maxY()) { ... }
 * ```
 * 
 * ⚠️ **CAUTION:** Direct field access requires knowing the coordinate system
 * ```cpp
 * // SCREEN
 * if (y >= bounds.top && y <= bounds.bottom) { ... }  // OK
 * 
 * // MATH
 * if (y >= bounds.bottom && y <= bounds.top) { ... }  // Reversed!
 * ```
 * 
 * @note Replaces legacy XYBounds class
 * @see minX(), maxX(), minY(), maxY() for system-agnostic access
 * @see fromMinMax() for system-agnostic construction
 */
class Bounds {
public:
    double left{0.0};    ///< Minimum X coordinate (left edge)
    double top{0.0};     ///< Top edge (meaning depends on coordinate system)
    double right{0.0};   ///< Maximum X coordinate (right edge)
    double bottom{0.0};  ///< Bottom edge (meaning depends on coordinate system)
    
    CoordinateSystem spatialSystem{CoordinateSystem::screen()};  ///< Coordinate system
    
    // Constructors
    
    /**
     * @brief Default constructor - creates empty bounds at origin in SCREEN coordinates
     * 
     * Creates bounds with all coordinates set to 0.0, representing
     * an empty rectangle at the origin in screen coordinate system.
     * 
     * @code{.cpp}
     * Bounds empty;  // {0, 0, 0, 0} in SCREEN coordinates
     * assert(empty.isEmpty());
     * assert(empty.spatialSystem.isScreen());
     * @endcode
     */
    Bounds() = default;
    
    /**
     * @brief Construct from coordinates (defaults to SCREEN system)
     * @param l Left edge (minimum X)
     * @param t Top edge
     * @param r Right edge (maximum X)
     * @param b Bottom edge
     * 
     * @code{.cpp}
     * Bounds box{0.0, 0.0, 100.0, 50.0};  // SCREEN coordinates
     * // Creates bounds: left=0, top=0, right=100, bottom=50
     * @endcode
     * 
     * @note No validation performed - use isValid() to check
     * @warning Ensure coordinates match intended system:
     *          SCREEN: top <= bottom, MATH: bottom <= top
     */
    Bounds(double l, double t, double r, double b)
        : left(l), top(t), right(r), bottom(b), spatialSystem(CoordinateSystem::screen()) {}
    
    /**
     * @brief Construct from coordinates with explicit coordinate system
     * @param l Left edge (minimum X)
     * @param t Top edge
     * @param r Right edge (maximum X)
     * @param b Bottom edge
     * @param sys Coordinate system
     * 
     * @code{.cpp}
     * // Screen coordinates
     * Bounds screenBox{0, 0, 100, 50, CoordinateSystem::screen(768)};
     * 
     * // Math coordinates
     * Bounds mathBox{0, 0, 100, 50, CoordinateSystem::math()};
     * @endcode
     */
    Bounds(double l, double t, double r, double b, const CoordinateSystem sys)
        : left(l), top(t), right(r), bottom(b), spatialSystem(sys) {}
    
    /**
     * @brief Construct from two corner points
     * @param p1 First corner
     * @param p2 Opposite corner (any diagonal)
     * @param sys Coordinate system (defaults to SCREEN)
     * @return Bounds containing both points
     * 
     * Automatically determines min/max for each axis, so points
     * can be in any order.
     * 
     * @code{.cpp}
     * Point topLeft{0.0, 0.0};
     * Point bottomRight{100.0, 50.0};
     * Bounds box = Bounds::fromCorners(topLeft, bottomRight);
     * 
     * // Works with any diagonal
     * Bounds box2 = Bounds::fromCorners(bottomRight, topLeft);
     * assert(box == box2);
     * @endcode
     * 
     * @see fromCenterAndSize()
     */

    static Bounds fromCorners(const Point& p1, const Point& p2, 
                             CoordinateSystem sys = CoordinateSystem::screen()) {
        return {
            std::min(p1.x, p2.x),
            std::min(p1.y, p2.y),
            std::max(p1.x, p2.x),
            std::max(p1.y, p2.y),
            sys
        };
    }
    
    /**
     * @brief Construct from center and size
     * @param center Center point of bounds
     * @param width Width of bounds
     * @param height Height of bounds
     * @param sys Coordinate system (defaults to SCREEN)
     * @return Bounds centered at specified point
     * 
     * @code{.cpp}
     * Point center{50.0, 25.0};
     * Bounds box = Bounds::fromCenterAndSize(center, 100.0, 50.0);
     * // Creates bounds: {0, 0, 100, 50} in SCREEN coords
     * 
     * assert(box.center() == center);
     * assert(box.width() == 100.0);
     * assert(box.height() == 50.0);
     * @endcode
     * 
     * @see fromCorners()
     */
    static Bounds fromCenterAndSize(const Point& center, double width, double height,
                                    CoordinateSystem sys = CoordinateSystem::screen()) {
        double halfW = width / 2.0;
        double halfH = height / 2.0;
        return {
            center.x - halfW,
            center.y - halfH,
            center.x + halfW,
            center.y + halfH,
            sys
        };
    }
    
    /**
     * @brief Create infinite bounds (for initial expansion)
     * @param sys Coordinate system (defaults to SCREEN)
     * @return Bounds with inverted infinity values
     * 
     * Creates bounds with left/top = +∞ and right/bottom = -∞.
     * Used as starting point for expanding to fit multiple points/shapes.
     * 
     * @code{.cpp}
     * Bounds bounds = Bounds::infinite();
     * 
     * for (const auto& point : points) {
     *     bounds.expand(point);  // First expand sets actual bounds
     * }
     * // bounds now contains all points
     * @endcode
     * 
     * @note After first expand(), bounds become finite
     * @see expand()
     */
    static Bounds infinite(CoordinateSystem sys = CoordinateSystem::screen()) {
        constexpr double inf = std::numeric_limits<double>::infinity();
        return {inf, inf, -inf, -inf, sys};
    }
    
    /**
     * @brief Construct from min/max coordinates (system-agnostic)
     * @param min_x Minimum X coordinate
     * @param min_y Minimum Y coordinate
     * @param max_x Maximum X coordinate
     * @param max_y Maximum Y coordinate
     * @param sys Coordinate system (defaults to SCREEN)
     * @return Bounds with correct field mapping for coordinate system
     * 
     * System-agnostic factory method that accepts min/max values and
     * automatically maps them to the correct left/top/right/bottom fields
     * based on the coordinate system.
     * 
     * This is the **recommended way** to create bounds when working with
     * both coordinate systems, as it eliminates confusion about field ordering.
     * 
     * @code{.cpp}
     * // SCREEN coordinates - intuitive ordering
     * Bounds screen = Bounds::fromMinMax(0, 0, 100, 50);
     * assert(screen.left == 0);
     * assert(screen.top == 0);      // min_y in SCREEN
     * assert(screen.right == 100);
     * assert(screen.bottom == 50);  // max_y in SCREEN
     * assert(screen.minY() == 0);
     * assert(screen.maxY() == 50);
     * 
     * // MATH coordinates - same intuitive ordering!
     * Bounds math = Bounds::fromMinMax(0, 0, 100, 50, CoordinateSystem::math());
     * assert(math.left == 0);
     * assert(math.top == 50);       // max_y in MATH
     * assert(math.right == 100);
     * assert(math.bottom == 0);     // min_y in MATH
     * assert(math.minY() == 0);     // Always smaller value
     * assert(math.maxY() == 50);    // Always larger value
     * 
     * // Both systems validate correctly
     * assert(screen.isValid());
     * assert(math.isValid());
     * @endcode
     * 
     * @note Guarantees min_x <= max_x and min_y <= max_y
     * @see fromCorners(), fromCenterAndSize()
     */
    static Bounds fromMinMax(double min_x, double min_y, double max_x, double max_y,
                            CoordinateSystem sys = CoordinateSystem::screen()) {
        if (sys.isScreen()) {
            // SCREEN: top = min_y, bottom = max_y
            return {min_x, min_y, max_x, max_y, sys};
        } else {
            // MATH: top = max_y, bottom = min_y
            return {min_x, max_y, max_x, min_y, sys};
        }
    }
    
    // Properties
    
    /**
     * @brief Get minimum X coordinate (always left edge)
     * @return Left edge coordinate
     * 
     * System-agnostic accessor. Always returns the smaller X value.
     * Equivalent to `left` but more semantically clear.
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 100, 50};
     * assert(box.minX() == 0.0);
     * assert(box.minX() == box.left);
     * @endcode
     * 
     * @see maxX(), minY(), maxY()
     */
    double minX() const {
        return left;
    }
    
    /**
     * @brief Get maximum X coordinate (always right edge)
     * @return Right edge coordinate
     * 
     * System-agnostic accessor. Always returns the larger X value.
     * Equivalent to `right` but more semantically clear.
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 100, 50};
     * assert(box.maxX() == 100.0);
     * assert(box.maxX() == box.right);
     * @endcode
     * 
     * @see minX(), minY(), maxY()
     */
    double maxX() const {
        return right;
    }
    
    /**
     * @brief Get minimum Y coordinate (system-dependent)
     * @return Smaller Y value
     * 
     * System-agnostic accessor. Always returns the smaller Y value:
     * - SCREEN: returns `top` (top < bottom)
     * - MATH: returns `bottom` (bottom < top)
     * 
     * @code{.cpp}
     * // SCREEN coordinates
     * Bounds screen{0, 0, 100, 50, CoordinateSystem::screen()};
     * assert(screen.minY() == 0.0);    // top
     * assert(screen.minY() < screen.maxY());
     * 
     * // MATH coordinates
     * Bounds math{0, 50, 100, 0, CoordinateSystem::math()};
     * assert(math.minY() == 0.0);      // bottom
     * assert(math.minY() < math.maxY());
     * @endcode
     * 
     * @note Always guarantees minY() <= maxY() regardless of coordinate system
     * @see maxY(), minX(), maxX()
     */
    double minY() const {
        return spatialSystem.isScreen() ? top : bottom;
    }
    
    /**
     * @brief Get maximum Y coordinate (system-dependent)
     * @return Larger Y value
     * 
     * System-agnostic accessor. Always returns the larger Y value:
     * - SCREEN: returns `bottom` (bottom > top)
     * - MATH: returns `top` (top > bottom)
     * 
     * @code{.cpp}
     * // SCREEN coordinates
     * Bounds screen{0, 0, 100, 50, CoordinateSystem::screen()};
     * assert(screen.maxY() == 50.0);   // bottom
     * assert(screen.maxY() > screen.minY());
     * 
     * // MATH coordinates
     * Bounds math{0, 50, 100, 0, CoordinateSystem::math()};
     * assert(math.maxY() == 50.0);     // top
     * assert(math.maxY() > math.minY());
     * @endcode
     * 
     * @note Always guarantees maxY() >= minY() regardless of coordinate system
     * @see minY(), minX(), maxX()
     */
    double maxY() const {
        return spatialSystem.isScreen() ? bottom : top;
    }
    
    /**
     * @brief Check if bounds are empty (zero or negative area)
     * @return true if all coordinates are 0.0
     * 
     * @code{.cpp}
     * Bounds empty;
     * assert(empty.isEmpty());
     * 
     * Bounds box{0, 0, 10, 10};
     * assert(!box.isEmpty());
     * @endcode
     * 
     * @see clear(), isValid()
     */
    bool isEmpty() const {
        return left == 0.0 && top == 0.0 && right == 0.0 && bottom == 0.0;
    }
    
    /**
     * @brief Check if bounds are valid (non-negative dimensions)
     * @return true if coordinates are valid for the current coordinate system
     * 
     * Validation now uses system-agnostic min/max accessors, ensuring
     * that bounds are valid regardless of coordinate system:
     * - minX() ? maxX() (always true)
     * - minY() ? maxY() (always true)
     * 
     * This is much clearer than the old system-dependent validation:
     * - SCREEN: left ? right AND top ? bottom
     * - MATH: left ? right AND bottom ? top
     * 
     * @code{.cpp}
     * // SCREEN coordinates
     * Bounds screen = Bounds::fromMinMax(0, 0, 10, 10);
     * assert(screen.isValid());  // minY(0) <= maxY(10) ✓
     * 
     * // MATH coordinates
     * Bounds math = Bounds::fromMinMax(0, 0, 10, 10, CoordinateSystem::math());
     * assert(math.isValid());    // minY(0) <= maxY(10) ✓
     * 
     * // Invalid bounds
     * Bounds invalid{10, 0, 0, 10};  // left > right
     * assert(!invalid.isValid());
     * @endcode
     * 
     * @note Empty bounds (0,0,0,0) are considered valid
     * @see minX(), maxX(), minY(), maxY()
     */
    bool isValid() const {
        // Use system-agnostic accessors for clear, uniform validation
        return minX() <= maxX() && minY() <= maxY();
    }
    
    /**
     * @brief Get width
     * @return Horizontal extent (right - left)
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 100, 50};
     * assert(box.width() == 100.0);
     * @endcode
     * 
     * @see height(), area()
     */
    double width() const {
        return right - left;
    }
    
    /**
     * @brief Get height  
     * @return Vertical extent (bottom - top)
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 100, 50};
     * assert(box.height() == 50.0);
     * @endcode
     * 
     * @see width(), area()
     */
    double height() const {
        return bottom - top;
    }
    
    /**
     * @brief Get area
     * @return Area of bounds (width × height)
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 5};
     * assert(box.area() == 50.0);
     * @endcode
     */
    double area() const {
        return width() * height();
    }
    
    /**
     * @brief Get center point
     * @return Point at geometric center of bounds
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 100, 50};
     * Point c = box.center();  // {50.0, 25.0}
     * @endcode
     * 
     * @see fromCenterAndSize()
     */
    Point center() const {
        return {(left + right) / 2.0, (top + bottom) / 2.0};
    }
    
    /**
     * @brief Get perimeter
     * @return Total length of boundary (2 × (width + height))
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 5};
     * assert(box.perimeter() == 30.0);  // 2 * (10 + 5)
     * @endcode
     */
    double perimeter() const {
        return 2.0 * (width() + height());
    }
    
    /**
     * @brief Get corner points (TL, TR, BR, BL)
     * @return Array of 4 corner points in clockwise order
     * 
     * Order: Top-Left, Top-Right, Bottom-Right, Bottom-Left
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 10};
     * auto corners = box.corners();
     * // corners[0] = {0, 0}   (top-left)
     * // corners[1] = {10, 0}  (top-right)
     * // corners[2] = {10, 10} (bottom-right)
     * // corners[3] = {0, 10}  (bottom-left)
     * @endcode
     */
    std::array<Point, 4> corners() const {
        return {{
            {left, top},      // Top-left
            {right, top},     // Top-right
            {right, bottom},  // Bottom-right
            {left, bottom}    // Bottom-left
        }};
    }
    
    // Modifications
    
    /**
     * @brief Clear bounds to empty state
     * 
     * Sets all coordinates to 0.0.
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 100, 50};
     * box.clear();
     * assert(box.isEmpty());
     * @endcode
     * 
     * @see isEmpty()
     */
    void clear() {
        left = top = right = bottom = 0.0;
    }
    
    /**
     * @brief Shift bounds by offset
     * @param dx Horizontal offset
     * @param dy Vertical offset
     * 
     * Translates bounds without changing size.
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 10};
     * box.shift(5.0, 5.0);
     * // box is now {5, 5, 15, 15}
     * assert(box.width() == 10.0);  // Size unchanged
     * @endcode
     * 
     * @see shift(const Point&)
     */
    void shift(double dx, double dy) {
        left += dx;
        right += dx;
        top += dy;
        bottom += dy;
    }
    
    /**
     * @brief Shift bounds by point
     * @param offset Translation vector
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 10};
     * box.shift({5.0, 5.0});
     * // box is now {5, 5, 15, 15}
     * @endcode
     */
    void shift(const Point& offset) {
        shift(offset.x, offset.y);
    }
    
    /**
     * @brief Expand to include a point
     * @param point Point to include in bounds
     * 
     * Grows bounds (if necessary) to contain the point.
     * If bounds are empty, sets bounds to contain just this point.
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 10};
     * box.expand({15.0, 15.0});
     * // box is now {0, 0, 15, 15}
     * 
     * Bounds empty;
     * empty.expand({5.0, 5.0});
     * // empty is now {5, 5, 5, 5}
     * @endcode
     * 
     * @note Common pattern: start with infinite() and expand for all points
     * @see merge(), infinite()
     */
    void expand(const Point& point) {
        if (isEmpty()) {
            left = right = point.x;
            top = bottom = point.y;
        } else {
            left = std::min(left, point.x);
            right = std::max(right, point.x);
            top = std::min(top, point.y);
            bottom = std::max(bottom, point.y);
        }
    }
    
    /**
     * @brief Merge with another bounds (union)
     * @param other Bounds to merge with
     * 
     * Expands this bounds to contain both this and other.
     * 
     * @code{.cpp}
     * Bounds b1{0, 0, 10, 10};
     * Bounds b2{5, 5, 15, 15};
     * b1.merge(b2);
     * // b1 is now {0, 0, 15, 15}
     * @endcode
     * 
     * @see unionWith() for non-modifying version
     * @see expand()
     */
    void merge(const Bounds& other) {
        if (other.isEmpty()) return;
        if (isEmpty()) {
            *this = other;
        } else {
            left = std::min(left, other.left);
            top = std::min(top, other.top);
            right = std::max(right, other.right);
            bottom = std::max(bottom, other.bottom);
        }
    }
    
    /**
     * @brief Expand bounds by margin on all sides
     * @param margin Amount to expand (can be negative to shrink)
     * 
     * @code{.cpp}
     * Bounds box{10, 10, 20, 20};
     * box.inflate(5.0);
     * // box is now {5, 5, 25, 25}
     * 
     * box.inflate(-2.0);  // Shrink
     * // box is now {7, 7, 23, 23}
     * @endcode
     * 
     * @note Negative margin shrinks the bounds
     * @see inflate(double, double) for asymmetric expansion
     */
    void inflate(double margin) {
        left -= margin;
        top -= margin;
        right += margin;
        bottom += margin;
    }
    
    /**
     * @brief Expand bounds by different margins
     * @param dx Horizontal margin
     * @param dy Vertical margin
     * 
     * @code{.cpp}
     * Bounds box{10, 10, 20, 20};
     * box.inflate(5.0, 2.0);
     * // box is now {5, 8, 25, 22}
     * @endcode
     */
    void inflate(double dx, double dy) {
        left -= dx;
        right += dx;
        top -= dy;
        bottom += dy;
    }
    
    // Queries
    
    /**
     * @brief Check if point is inside bounds (inclusive)
     * @param point Point to test
     * @return true if point is within or on boundary
     * 
     * Boundary points are considered inside.
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 10};
     * 
     * assert(box.contains({5.0, 5.0}));   // Inside
     * assert(box.contains({0.0, 0.0}));   // On corner (inclusive)
     * assert(box.contains({10.0, 5.0}));  // On edge (inclusive)
     * assert(!box.contains({15.0, 5.0})); // Outside
     * @endcode
     * 
     * @see contains(const Bounds&)
     */
    bool contains(const Point& point) const {
        return point.x >= left && point.x <= right &&
               point.y >= top && point.y <= bottom;
    }
    
    /**
     * @brief Check if other bounds is completely inside
     * @param other Bounds to test
     * @return true if other is completely contained
     * 
     * @code{.cpp}
     * Bounds outer{0, 0, 20, 20};
     * Bounds inner{5, 5, 15, 15};
     * 
     * assert(outer.contains(inner));
     * assert(!inner.contains(outer));
     * @endcode
     * 
     * @see intersects()
     */
    bool contains(const Bounds& other) const {
        return other.left >= left && other.right <= right &&
               other.top >= top && other.bottom <= bottom;
    }
    
    /**
     * @brief Check if bounds intersects with another
     * @param other Bounds to test intersection
     * @return true if bounds overlap (even if just touching)
     * 
     * @code{.cpp}
     * Bounds b1{0, 0, 10, 10};
     * Bounds b2{5, 5, 15, 15};
     * Bounds b3{20, 20, 30, 30};
     * 
     * assert(b1.intersects(b2));   // Overlap
     * assert(!b1.intersects(b3));  // Separate
     * @endcode
     * 
     * @see intersection()
     */
    bool intersects(const Bounds& other) const {
        return !(right < other.left || left > other.right ||
                 bottom < other.top || top > other.bottom);
    }
    
    /**
     * @brief Get intersection with another bounds
     * @param other Bounds to intersect with
     * @return Intersection bounds, or empty if no overlap
     * 
     * @code{.cpp}
     * Bounds b1{0, 0, 10, 10};
     * Bounds b2{5, 5, 15, 15};
     * Bounds overlap = b1.intersection(b2);
     * // overlap = {5, 5, 10, 10}
     * 
     * Bounds b3{20, 20, 30, 30};
     * Bounds noOverlap = b1.intersection(b3);
     * assert(noOverlap.isEmpty());  // No intersection
     * @endcode
     * 
     * @see intersects(), unionWith()
     */
    Bounds intersection(const Bounds& other) const {
        if (!intersects(other)) {
            return Bounds{};
        }
        return {
            std::max(left, other.left),
            std::max(top, other.top),
            std::min(right, other.right),
            std::min(bottom, other.bottom)
        };
    }
    
    /**
     * @brief Get union with another bounds
     * @param other Bounds to union with
     * @return Smallest bounds containing both
     * 
     * @code{.cpp}
     * Bounds b1{0, 0, 10, 10};
     * Bounds b2{5, 5, 15, 15};
     * Bounds combined = b1.unionWith(b2);
     * // combined = {0, 0, 15, 15}
     * @endcode
     * 
     * @see merge() for in-place version
     * @see intersection()
     */
    Bounds unionWith(const Bounds& other) const {
        Bounds result = *this;
        result.merge(other);
        return result;
    }
    
    /**
     * @brief Clamp point to bounds
     * @param point Point to clamp
     * @return Nearest point inside bounds
     * 
     * If point is inside, returns it unchanged.
     * If outside, returns nearest point on boundary.
     * 
     * @code{.cpp}
     * Bounds box{0, 0, 10, 10};
     * 
     * Point inside{5, 5};
     * assert(box.clamp(inside) == inside);  // Unchanged
     * 
     * Point outside{15, -5};
     * Point clamped = box.clamp(outside);  // {10, 0}
     * @endcode
     * 
     * @note Useful for constraining points to valid regions
     */
    Point clamp(const Point& point) const {
        return {
            std::clamp(point.x, left, right),
            std::clamp(point.y, top, bottom)
        };
    }
    
    // Comparison
    
    /**
     * @brief Exact equality comparison
     * @param other Bounds to compare
     * @return true if all coordinates are exactly equal
     * 
     * @warning Uses exact floating-point equality
     */
    bool operator==(const Bounds& other) const {
        return left == other.left && top == other.top &&
               right == other.right && bottom == other.bottom;
    }
    
    /**
     * @brief Inequality comparison
     * @param other Bounds to compare
     * @return true if any coordinate differs
     */
    bool operator!=(const Bounds& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Stream output operator
 * @param os Output stream
 * @param b Bounds to output
 * @return Reference to output stream
 * 
 * Formats bounds as: `[left, top, right, bottom]`
 * 
 * @code{.cpp}
 * Bounds box{0, 0, 100, 50};
 * std::cout << box << std::endl;
 * // Outputs: [0, 0, 100, 50]
 * @endcode
 * 
 * @relates Bounds
 */
std::ostream& operator<<(std::ostream& os, const Bounds& b);

} // namespace aperture
