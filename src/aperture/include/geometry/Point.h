/**
 * @file Point.h
 * @brief 2D point class with geometric operations
 *
 * The Point struct provides fundamental 2D coordinate operations used
 * throughout the ApertureCore geometry module. It replaces the legacy XYPoint
 * class with modern C++17 features while maintaining compatibility.
 *
 * ## Key Features
 * - Lightweight aggregate type (POD-like)
 * - Constexpr constructors for compile-time evaluation
 * - Full set of arithmetic operators
 * - Distance and geometric calculations
 * - Rotation and transformation operations
 *
 * ## Usage Example
 * @code{.cpp}
 * #include <aperturecore/geometry/Point.h>
 *
 * using namespace aperture;
 *
 * // Create points
 * Point origin{0.0, 0.0};
 * Point p1{3.0, 4.0};
 *
 * // Calculate distance
 * double dist = origin.distanceTo(p1);  // Returns 5.0
 *
 * // Vector operations
 * Point midpoint = (origin + p1) * 0.5;  // {1.5, 2.0}
 * Point direction = p1.normalized();      // {0.6, 0.8}
 *
 * // Rotation
 * Point rotated = p1.rotated(M_PI / 2);  // Rotate 90° CCW
 *
 * // Comparison with tolerance
 * if (p1.isNear(Point{3.0, 4.0})) {
 *     // Points are equal within tolerance
 * }
 * @endcode
 *
 * @see Bounds, Shape
 */
#pragma once

#include <cmath>
#include <iosfwd>

namespace aperture {

/**
 * @struct Point
 * @brief 2D point in Cartesian coordinates
 *
 * A lightweight struct representing a point in 2D space. Uses double precision
 * for compatibility with scientific computing and legacy code.
 *
 * The Point struct is designed as an aggregate type, allowing brace
 * initialization and guaranteed copy elision. All methods are constexpr where
 * possible for compile-time evaluation.
 *
 * ### Memory Layout
 * ```
 * Point p{x, y};
 * sizeof(Point) == 16 bytes (2 × sizeof(double))
 * ```
 *
 * ### Coordinate System
 * - X-axis: Positive right
 * - Y-axis: Positive up
 * - Rotation: Counter-clockwise (mathematical convention)
 *
 * @note This replaces the legacy XYPoint class
 */
struct Point {
  double x{0.0};  ///< X coordinate (horizontal axis)
  double y{0.0};  ///< Y coordinate (vertical axis)

  // Constructors

  /**
   * @brief Default constructor - initializes to origin (0, 0)
   *
   * @note Constexpr allows compile-time construction
   */
  constexpr Point() = default;

  /**
   * @brief Construct point from coordinates
   * @param x_ X coordinate
   * @param y_ Y coordinate
   *
   * @code{.cpp}
   * Point p{10.0, 20.0};
   * constexpr Point origin{0.0, 0.0};  // Compile-time construction
   * @endcode
   */
  constexpr Point(double x_, double y_) : x(x_), y(y_) {}

  // Distance calculations

  /**
   * @brief Calculate Euclidean distance to another point
   * @param other Target point
   * @return Distance between this point and other
   *
   * Uses the Pythagorean theorem: ?((x?-x?)? + (y?-y?)?)
   *
   * @code{.cpp}
   * Point p1{0.0, 0.0};
   * Point p2{3.0, 4.0};
   * double dist = p1.distanceTo(p2);  // Returns 5.0
   * @endcode
   *
   * @see distanceSquaredTo() for faster calculation without sqrt
   * @see distance() for free function alternative
   */
  double distanceTo(const Point &other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  /**
   * @brief Calculate squared distance (faster, no sqrt)
   * @param other Target point
   * @return Squared distance between points
   *
   * Useful for distance comparisons without expensive sqrt operation.
   *
   * @code{.cpp}
   * Point p1{0.0, 0.0};
   * Point p2{3.0, 4.0};
   * double distSq = p1.distanceSquaredTo(p2);  // Returns 25.0
   *
   * // Efficient distance comparison
   * if (p1.distanceSquaredTo(p2) < threshold * threshold) {
   *     // Points are close
   * }
   * @endcode
   *
   * @note Approximately 2x faster than distanceTo()
   */
  double distanceSquaredTo(const Point &other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return dx * dx + dy * dy;
  }

  /**
   * @brief Get distance from origin (vector magnitude)
   * @return Distance from (0, 0) to this point
   *
   * Equivalent to treating the point as a vector and computing its length.
   *
   * @code{.cpp}
   * Point p{3.0, 4.0};
   * double mag = p.magnitude();  // Returns 5.0
   * @endcode
   *
   * @see magnitudeSquared() for faster calculation
   */
  double magnitude() const { return std::sqrt(x * x + y * y); }

  /**
   * @brief Get squared magnitude (faster)
   * @return Squared distance from origin
   *
   * @code{.cpp}
   * Point p{3.0, 4.0};
   * double magSq = p.magnitudeSquared();  // Returns 25.0
   * @endcode
   */
  double magnitudeSquared() const { return x * x + y * y; }

  // Arithmetic operators

  /**
   * @brief Add another point (vector addition)
   * @param other Point to add
   * @return Reference to this point
   *
   * @code{.cpp}
   * Point p{1.0, 2.0};
   * p += Point{3.0, 4.0};  // p is now {4.0, 6.0}
   * @endcode
   */
  Point &operator+=(const Point &other) {
    x += other.x;
    y += other.y;
    return *this;
  }

  /**
   * @brief Subtract another point (vector subtraction)
   * @param other Point to subtract
   * @return Reference to this point
   */
  Point &operator-=(const Point &other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  /**
   * @brief Multiply by scalar (scale vector)
   * @param scalar Scaling factor
   * @return Reference to this point
   *
   * @code{.cpp}
   * Point p{2.0, 3.0};
   * p *= 2.0;  // p is now {4.0, 6.0}
   * @endcode
   */
  Point &operator*=(double scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
  }

  /**
   * @brief Divide by scalar
   * @param scalar Division factor
   * @return Reference to this point
   *
   * @warning No division-by-zero check for performance
   */
  Point &operator/=(double scalar) {
    x /= scalar;
    y /= scalar;
    return *this;
  }

  /**
   * @brief Add two points (vector addition)
   * @param other Point to add
   * @return New point representing sum
   */
  Point operator+(const Point &other) const {
    return {x + other.x, y + other.y};
  }

  /**
   * @brief Subtract two points (vector subtraction)
   * @param other Point to subtract
   * @return New point representing difference
   *
   * @code{.cpp}
   * Point p1{5.0, 7.0};
   * Point p2{2.0, 3.0};
   * Point diff = p1 - p2;  // {3.0, 4.0}
   * @endcode
   */
  Point operator-(const Point &other) const {
    return {x - other.x, y - other.y};
  }

  /**
   * @brief Multiply point by scalar
   * @param scalar Scaling factor
   * @return Scaled point
   */
  Point operator*(double scalar) const { return {x * scalar, y * scalar}; }

  /**
   * @brief Divide point by scalar
   * @param scalar Division factor
   * @return Scaled point
   */
  Point operator/(double scalar) const { return {x / scalar, y / scalar}; }

  /**
   * @brief Negate point (flip direction)
   * @return Point with negated coordinates
   *
   * @code{.cpp}
   * Point p{3.0, 4.0};
   * Point neg = -p;  // {-3.0, -4.0}
   * @endcode
   */
  Point operator-() const { return {-x, -y}; }

  // Comparison operators

  /**
   * @brief Exact equality comparison
   * @param other Point to compare
   * @return true if coordinates are exactly equal
   *
   * @warning Uses exact floating-point equality - consider isNear() for
   * tolerance
   * @see isNear() for tolerance-based comparison
   */
  bool operator==(const Point &other) const {
    return x == other.x && y == other.y;
  }

  /**
   * @brief Inequality comparison
   * @param other Point to compare
   * @return true if coordinates differ
   */
  bool operator!=(const Point &other) const { return !(*this == other); }

  /**
   * @brief Check if points are within tolerance distance
   * @param other Point to compare with
   * @param tolerance Maximum distance for equality (default: 1e-6)
   * @return true if distance between points ? tolerance
   *
   * Recommended for floating-point comparison to avoid precision issues.
   *
   * @code{.cpp}
   * Point p1{1.0000001, 2.0};
   * Point p2{1.0, 2.0};
   *
   * if (p1 == p2) {  // false - exact comparison
   *     // ...
   * }
   *
   * if (p1.isNear(p2)) {  // true - within default tolerance
   *     // Points are practically equal
   * }
   *
   * if (p1.isNear(p2, 1e-10)) {  // false - stricter tolerance
   *     // ...
   * }
   * @endcode
   *
   * @note Uses squared distance internally for efficiency
   */
  bool isNear(const Point &other, double tolerance = 1e-6) const {
    return distanceSquaredTo(other) <= tolerance * tolerance;
  }

  // Geometric operations

  /**
   * @brief Dot product with another point (treating as vectors)
   * @param other Second point/vector
   * @return Scalar dot product: x?·x? + y?·y?
   *
   * Useful for:
   * - Calculating angles between vectors
   * - Projection operations
   * - Testing perpendicularity (dot = 0)
   *
   * @code{.cpp}
   * Point v1{1.0, 0.0};
   * Point v2{0.0, 1.0};
   * double dot = v1.dot(v2);  // Returns 0.0 (perpendicular)
   *
   * // Calculate angle between vectors
   * double cosAngle = v1.normalized().dot(v2.normalized());
   * double angle = std::acos(cosAngle);
   * @endcode
   */
  double dot(const Point &other) const { return x * other.x + y * other.y; }

  /**
   * @brief Cross product (z-component of 3D cross product)
   * @param other Second point/vector
   * @return Z-component of cross product: x?·y? - y?·x?
   *
   * Useful for:
   * - Determining rotation direction (sign indicates CW/CCW)
   * - Calculating signed area of triangles
   * - Testing collinearity (cross = 0)
   *
   * @code{.cpp}
   * Point v1{1.0, 0.0};
   * Point v2{0.0, 1.0};
   * double cross = v1.cross(v2);  // Returns 1.0 (CCW rotation)
   *
   * // Check if p2 is left of line from p0 to p1
   * Point p0{0.0, 0.0}, p1{1.0, 0.0}, p2{0.5, 1.0};
   * double cross = (p1 - p0).cross(p2 - p0);
   * if (cross > 0) {
   *     // p2 is to the left (CCW)
   * }
   * @endcode
   *
   * @note Positive = counter-clockwise, Negative = clockwise
   */
  double cross(const Point &other) const { return x * other.y - y * other.x; }

  /**
   * @brief Normalize to unit length
   * @return Normalized point (magnitude = 1.0)
   *
   * Creates a unit vector pointing in the same direction.
   *
   * @code{.cpp}
   * Point p{3.0, 4.0};
   * Point unit = p.normalized();  // {0.6, 0.8}, magnitude = 1.0
   * @endcode
   *
   * @warning Returns origin {0, 0} if magnitude is zero
   * @note Does not modify the original point
   */
  Point normalized() const {
    double mag = magnitude();
    return mag > 0.0 ? (*this / mag) : Point{};
  }

  /**
   * @brief Rotate point around origin
   * @param angleRad Rotation angle in radians (counter-clockwise, positive)
   * @return Rotated point
   *
   * Uses rotation matrix:
   * ```
   * [cos(?)  -sin(?)] [x]
   * [sin(?)   cos(?)] [y]
   * ```
   *
   * @code{.cpp}
   * Point p{1.0, 0.0};
   * Point rotated = p.rotated(M_PI / 2);  // Rotate 90° CCW ? {0.0, 1.0}
   *
   * // Rotate 180°
   * Point reversed = p.rotated(M_PI);  // ? {-1.0, 0.0}
   * @endcode
   *
   * @note Angle in radians, not degrees
   * @see rotatedAround() to rotate around arbitrary center
   */
  Point rotated(double angleRad) const {
    double cosA = std::cos(angleRad);
    double sinA = std::sin(angleRad);
    return {x * cosA - y * sinA, x * sinA + y * cosA};
  }

  /**
   * @brief Rotate point around a center point
   * @param center Center of rotation
   * @param angleRad Rotation angle in radians (counter-clockwise)
   * @return Rotated point
   *
   * Equivalent to: translate to origin, rotate, translate back
   *
   * @code{.cpp}
   * Point p{10.0, 5.0};
   * Point center{5.0, 5.0};
   * Point rotated = p.rotatedAround(center, M_PI);  // Rotate 180° around
   * center
   * // Result: {0.0, 5.0}
   * @endcode
   *
   * @see rotated() for rotation around origin
   */
  Point rotatedAround(const Point &center, double angleRad) const {
    return (*this - center).rotated(angleRad) + center;
  }
};

// Free functions

/**
 * @brief Scalar-first multiplication
 * @param scalar Scaling factor
 * @param p Point to scale
 * @return Scaled point
 *
 * Allows natural syntax: `2.0 * point` in addition to `point * 2.0`
 *
 * @relates Point
 */
inline Point operator*(double scalar, const Point &p) {
  return p * scalar;
}

/**
 * @brief Calculate distance between two points
 * @param a First point
 * @param b Second point
 * @return Euclidean distance
 *
 * Free function alternative to member function syntax.
 *
 * @code{.cpp}
 * Point p1{0.0, 0.0};
 * Point p2{3.0, 4.0};
 * double d = distance(p1, p2);  // Returns 5.0
 * @endcode
 *
 * @relates Point
 * @see Point::distanceTo()
 */
inline double distance(const Point &a, const Point &b) {
  return a.distanceTo(b);
}

/**
 * @brief Linear interpolation between two points
 * @param a First point (t = 0)
 * @param b Second point (t = 1)
 * @param t Interpolation parameter [0, 1]
 * @return Interpolated point
 *
 * Calculates: a + t(b - a) = (1-t)a + tb
 *
 * @code{.cpp}
 * Point p1{0.0, 0.0};
 * Point p2{10.0, 10.0};
 *
 * Point mid = lerp(p1, p2, 0.5);    // Midpoint: {5.0, 5.0}
 * Point quarter = lerp(p1, p2, 0.25);  // {2.5, 2.5}
 * Point end = lerp(p1, p2, 1.0);    // Same as p2: {10.0, 10.0}
 * @endcode
 *
 * @note Works for t outside [0, 1] for extrapolation
 * @relates Point
 */
inline Point lerp(const Point &a, const Point &b, double t) {
  return a + (b - a) * t;
}

/**
 * @brief Stream output operator
 * @param os Output stream
 * @param p Point to output
 * @return Reference to output stream
 *
 * Formats point as: `(x, y)`
 *
 * @code{.cpp}
 * Point p{3.14, 2.71};
 * std::cout << p << std::endl;  // Outputs: (3.14, 2.71)
 * @endcode
 *
 * @relates Point
 */
std::ostream &operator<<(std::ostream &os, const Point &p);

}  // namespace aperture
