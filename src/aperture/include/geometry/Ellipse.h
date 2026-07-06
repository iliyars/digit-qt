/**
 * @file Ellipse.h
 * @brief Elliptical shape with rotation support and ellipse fitting
 * 
 * ## Overview
 * 
 * Ellipse provides parametric elliptical shapes with:
 * - Point containment testing for rotated ellipses
 * - Contour generation with adaptive sampling
 * - Ellipse fitting from 2-100+ points
 * - Geometric properties (eccentricity, focal distance)
 * - Full rotation and coordinate transformation support
 * 
 * ## Key Features
 * 
 * ### 1. Parametric Ellipse Representation
 * 
 * Ellipses are defined by semi-major (a) and semi-minor (b) axes:
 * 
 * @code{.cpp}
 * // Circle (a = b)
 * Ellipse circle(10.0, 10.0, 0.0, 0.0);
 * 
 * // Ellipse (a > b)
 * Ellipse ellipse(15.0, 8.0, 0.0, 0.0);
 * 
 * // Rotated ellipse
 * Ellipse rotated(12.0, 6.0, 0.0, 0.0, 45.0);
 * @endcode
 * 
 * ### 2. Ellipse Fitting from Points
 * 
 * Fit ellipses to measured data using various algorithms:
 * - 2-3 points: Circle fitting
 * - 4 points: Axis-aligned ellipse
 * - 5 points: Exact conic fit
 * - 6+ points: Least squares fit
 * 
 * See fitting constructor documentation for details.
 * 
 * ### 3. Rotation Support
 * 
 * Full rotation support with efficient coordinate transformation:
 * 
 * @code{.cpp}
 * Ellipse e(10, 5, 0, 0, 30);  // 30° rotation
 * 
 * // Point-in-ellipse works for any rotation
 * bool inside = e.isInside({3, 2});
 * @endcode
 * 
 * ### 4. Geometric Properties
 * 
 * Access standard ellipse properties:
 * - Eccentricity: e = sqrt(1 - b²/a²)
 * - Focal distance: c = sqrt(a² - b²)
 * - Area: π*a*b
 * - Perimeter: Ramanujan approximation
 * 
 * @see eccentricity(), focalDistance(), area(), perimeter()
 */
#pragma once

#include "Shape.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifdef EXTERNAL
#undef EXTERNAL
#endif
#ifdef MEASURING
#undef MEASURING
#endif

namespace aperture {

/**
 * @class Ellipse
 * @brief Parametric elliptical shape with rotation
 * 
 * Represents an ellipse defined by semi-major axis (a), semi-minor axis (b),
 * center position, and optional rotation angle. Supports point containment,
 * contour generation, and ellipse fitting from point sets.
 * 
 * ## Mathematical Definition
 * 
 * Standard ellipse equation in local coordinates:
 * ```
 * (x/a)² + (y/b)² = 1
 * ```
 * 
 * Where:
 * - a = semi-major axis (larger radius)
 * - b = semi-minor axis (smaller radius)
 * 
 * For rotated ellipse, apply rotation transformation.
 * 
 * ## Memory Layout
 * 
 * ```
 * sizeof(Ellipse) ≈ 64 bytes:
 *   - semiMajor, semiMinor: 16 bytes
 *   - center (Point): 16 bytes
 *   - rotationDeg, rotationRad: 16 bytes
 *   - cosRot, sinRot (cached): 16 bytes
 *   + Shape base: variable
 * ```
 * 
 * @note Replaces XYEllipse from InterfSolver
 */
class Ellipse : public Shape {
public:
    /**
     * @brief Construct ellipse from dimensions and position
     * @param semiMajorAxis Semi-major axis length (radius along major axis)
     * @param semiMinorAxis Semi-minor axis length (radius along minor axis)
     * @param centerX Center X coordinate in world frame
     * @param centerY Center Y coordinate in world frame
     * @param rotationDegrees Rotation angle in degrees (counter-clockwise)
     * @param typeLimits Visibility behavior (default: EXTERNAL)
     * @param spatialSystem Coordinate system (default: SCREEN)
     * @param normState Normalization state (default: MEASURING)
     * 
     * Creates an ellipse centered at (centerX, centerY) with specified radii.
     * Optional rotation rotates major axis from +X toward +Y.
     * 
     * ## Examples
     * 
     * @code{.cpp}
     * // Circle at origin (a = b = 10)
     * Ellipse circle(10.0, 10.0, 0.0, 0.0);
     * 
     * // Ellipse 15x8 at (50, 50)
     * Ellipse ellipse(15.0, 8.0, 50.0, 50.0);
     * 
     * // Rotated ellipse (45 degrees)
     * Ellipse rotated(12.0, 6.0, 0.0, 0.0, 45.0);
     * 
     * // With visibility type
     * Ellipse aperture(20.0, 10.0, 0.0, 0.0, 0.0, TypeLimits::EXTERNAL);
     * @endcode
     * 
     * @note Semi-axes are half the full width/height (radius from center to edge)
     * @note For circle, set semiMajor == semiMinor
     * @see Ellipse(std::vector<Point>) - Fit from points
     */
    Ellipse(double semiMajorAxis, double semiMinorAxis,
            double centerX, double centerY,
            double rotationDegrees = 0.0,
            TypeLimits typeLimits = TypeLimits::EXTERNAL,
            CoordinateSystem spatialSystem = CoordinateSystem::screen(),
            NormalizationState normState = NormalizationState::MEASURING);
    
    /**
     * @brief Construct ellipse by fitting to point set
     * @param points Points to fit ellipse through
     * @param typeLimits Visibility type (default: EXTERNAL)
     * @param spatialSystem Spatial coordinate system (default: SCREEN)
     * @param normState Normalization state (default: MEASURING)
     * 
     * Fits an ellipse to the given points using different algorithms based on
     * the number of points:
     * 
     * - **0 points**: Degenerate ellipse at origin with zero radii
     * - **1 point**: Degenerate ellipse (point) at that location
     * - **2 points**: Circle with diameter defined by the two points
     * - **3 points**: Circle through three points (geometric fit)
     * - **4 points**: Axis-aligned ellipse (center = average, axes = max extents)
     * - **5 points**: Exact ellipse (general conic through 5 points)
     * - **6+ points**: Least squares ellipse fit (algebraic distance minimization)
     * 
     * ## Algorithm Details
     * 
     * ### 3 Points - Circle Through Three Points
     * 
     * Solves for circle center (xc, yc) and radius R such that all three
     * points lie on the circle. Uses geometric circle fitting formula.
     * 
     * Falls back to centroid if points are collinear.
     * 
     * ### 5 Points - Exact Ellipse Fit
     * 
     * Fits general conic equation: Ax² + Bxy + Cy² + Dx + Ey + F = 0
     * 
     * Constraints: F = 1 (normalization), B²-4AC < 0 (ellipse condition)
     * 
     * Converts conic coefficients to ellipse parameters:
     * - Center (xc, yc)
     * - Semi-major and semi-minor axes (a, b)
     * - Rotation angle φ
     * 
     * Falls back to 4-point method if conic is not an ellipse.
     * 
     * ### 6+ Points - Least Squares Ellipse Fit
     * 
     * Minimizes algebraic distance: Σ(Ax²ᵉ + Bxᵡyᵢ + Cy²ᵢ + Dxᵡ + Eyᵡ + F)²
     * 
     * Subject to ellipse constraint: B²-4AC < 0
     * 
     * Uses linear system: (D'D)α = D'b where α = [A,B,C,D,E]'
     * 
     * Converts solution to geometric ellipse parameters.
     * 
     * Falls back to 4-point method if fit fails or is degenerate.
     * 
     * ## Usage Examples
     * 
     * @code{.cpp}
     * #include <aperturecore/geometry/Ellipse.h>
     * #include <vector>
     * 
     * using namespace aperture;
     * 
     * // Circle through 3 points
     * std::vector<Point> threePoints = {
     *     {0.0, 0.0},
     *     {10.0, 0.0},
     *     {5.0, 8.66}  // Forms equilateral triangle
     * };
     * Ellipse circle(threePoints);
     * // Result: circle centered at ~(5, 2.89) with radius ~5.77
     * 
     * // Exact ellipse through 5 points
     * std::vector<Point> fivePoints = {
     *     {10.0, 0.0},   // Right
     *     {0.0, 5.0},    // Top
     *     {-10.0, 0.0},  // Left
     *     {0.0, -5.0},   // Bottom
     *     {7.07, 3.54}   // Diagonal
     * };
     * Ellipse exact(fivePoints);
     * // Result: ellipse centered at ~(0,0), a~10, b~5, rotation~0°
     * 
     * // Least squares fit (noisy data)
     * std::vector<Point> noisyPoints;
     * for (int i = 0; i < 100; i++) {
     *     double angle = 2.0 * M_PI * i / 100.0;
     *     double x = 10.0 * cos(angle) + (rand() % 100 - 50) / 100.0;
     *     double y = 5.0 * sin(angle) + (rand() % 100 - 50) / 100.0;
     *     noisyPoints.push_back({x, y});
     * }
     * Ellipse fitted(noisyPoints);
     * // Result: best-fit ellipse to noisy data, a~10, b~5
     * @endcode
     * 
     * ## Performance
     * 
     * - **2-4 points**: O(1) - Simple geometric calculations
     * - **5 points**: O(1) - Solves 5x5 linear system
     * - **N > 5 points**: O(N) for matrix assembly, O(1) for 5x5 solve
     * 
     * ## Fallback Behavior
     * 
     * If ellipse fitting fails (e.g., points are collinear, conic is hyperbola):
     * - Falls back to simpler method (5→4, 6+→4)
     * - 4-point method creates axis-aligned ellipse from bounding extents
     * - Always produces a valid ellipse (possibly degenerate)
     * 
     * ## Mathematical References
     * 
     * - Fitzgibbon, Pilu, Fisher: "Direct Least Square Fitting of Ellipses" (1999)
     * - Halir, Flusser: "Numerically Stable Direct Least Squares Fitting" (1998)
     * 
     * @param points Vector of 2D points to fit
     * @param typeLimits Visibility behavior (EXTERNAL/INTERNAL/APERTURE)
     * @param spatialSystem Coordinate system (SCREEN/MATH)
     * @param normState Normalization state (MEASURING/NORMALIZED)
     * 
     * @note For best results with noisy data, use 10+ points
     * @note Collinear points produce degenerate (zero-area) ellipse
     * @warning Least squares fit can be sensitive to outliers
     * 
     * @see Ellipse(double, double, double, double, double) - Direct construction
     * @see fitEllipseDirect() - Alternative direct fit method (if needed)
     */
    explicit Ellipse(const std::vector<Point>& points,
                    TypeLimits typeLimits = TypeLimits::EXTERNAL,
                    CoordinateSystem spatialSystem = CoordinateSystem::screen(),
                    NormalizationState normState = NormalizationState::MEASURING);
    
    /**
     * @brief Fit circle to perimeter points using constrained LSM
     * @param points Perimeter sample points (N ≥ 3 required)
     * @param typeLimits Visibility behavior (default: EXTERNAL)
     * @param spatialSystem Coordinate system (default: SCREEN)
     * @param normState Normalization state (default: MEASURING)
     * @return Ellipse with radiusX == radiusY (perfect circle)
     * 
     * Uses least-squares circle fitting with equal-radii constraint:
     * - Computes optimal center to minimize sum of squared radial distances
     * - All points contribute equally to fit
     * - Returns Ellipse where major and minor axes are equal (circle)
     * 
     * ## Algorithm
     * 
     * 1. Compute centroid of points: c = mean(points)
     * 2. For each point p_i: compute distance r_i = ||p_i - c||
     * 3. Compute mean radius: r_mean = mean(r_i)
     * 4. Optimize center to minimize Σ(r_i - r_mean)²
     * 5. Return Ellipse(r_final, r_final, center)
     * 
     * ## When to Use
     * 
     * Use FitCircle when:
     * - User explicitly selected "Add Circle" tool
     * - You want to enforce circular constraint
     * - Data is known to be from a circle (with noise)
     * 
     * Use FitEllipse when:
     * - Data may represent an ellipse
     * - You want best-fit general conic
     * 
     * ## Example
     * 
     * @code{.cpp}
     * // User clicked 4 points roughly on circle perimeter
     * std::vector<Point> clicks{
     *     {10, 0}, {0, 10}, {-10, 0}, {0, -10}
     * };
     * 
     * // Fit circle (enforces radiusX == radiusY)
     * auto circle = Ellipse::FitCircle(clicks, TypeLimits::EXTERNAL);
     * assert(std::abs(circle->radiusX() - circle->radiusY()) < 1e-6);
     * 
     * // For comparison: general ellipse fit
     * auto ellipse = Ellipse::FitEllipse(clicks, TypeLimits::EXTERNAL);
     * // May have radiusX != radiusY if points are noisy
     * @endcode
     * 
     * @note Requires at least 3 points (mathematically determined)
     * @note Collinear points produce degenerate circle (zero radius)
     * @note Result is always a valid circle (possibly degenerate)
     * @see FitEllipse - Fit general ellipse (no circular constraint)
     */
    static std::unique_ptr<Ellipse> FitCircle(
        const std::vector<Point>& points,
        TypeLimits typeLimits = TypeLimits::EXTERNAL,
        CoordinateSystem spatialSystem = CoordinateSystem::screen(),
        NormalizationState normState = NormalizationState::MEASURING
    );
    
    /**
     * @brief Fit general ellipse to perimeter points using LSM
     * @param points Perimeter sample points (N ≥ 5 recommended)
     * @param typeLimits Visibility behavior (default: EXTERNAL)
     * @param spatialSystem Coordinate system (default: SCREEN)
     * @param normState Normalization state (default: MEASURING)
     * @return Best-fit ellipse (may have radiusX != radiusY)
     * 
     * Uses least-squares ellipse fitting (general conic section):
     * - Fits full 5-parameter ellipse (center, 2 radii, rotation)
     * - No constraint on axis ratios
     * - Optimizes for minimum squared geometric distance
     * 
     * ## Distinction from FitCircle
     * 
     * This is an **explicit static factory** for clarity:
     * - FitCircle: Enforces radiusX == radiusY constraint
     * - FitEllipse: Allows radiusX != radiusY (general)
     * 
     * Internally delegates to existing Ellipse(vector<Point>) constructor.
     * 
     * ## Example
     * 
     * @code{.cpp}
     * // User clicked points on elliptical boundary
     * std::vector<Point> clicks;
     * // ... collect clicks ...
     * 
     * auto ellipse = Ellipse::FitEllipse(clicks, TypeLimits::APERTURE);
     * 
     * // Check if result is approximately circular
     * double ratio = ellipse->radiusX() / ellipse->radiusY();
     * if (std::abs(ratio - 1.0) < 0.1) {
     *     // Nearly circular
     * }
     * @endcode
     * 
     * @note Requires at least 5 points for full ellipse (5 parameters)
     * @note Works with 3-4 points but may produce degenerate fit
     * @see FitCircle - Constrained fit for circles only
     */
    static std::unique_ptr<Ellipse> FitEllipse(
        const std::vector<Point>& points,
        TypeLimits typeLimits = TypeLimits::EXTERNAL,
        CoordinateSystem spatialSystem = CoordinateSystem::screen(),
        NormalizationState normState = NormalizationState::MEASURING
    );
    
    // Shape interface implementation
    
    /**
     * @brief Test if point is inside ellipse
     * @param point Point to test in world coordinates
     * @return true if point is inside or on ellipse boundary
     * 
     * Tests point containment using coordinate transformation:
     * 1. Transform point to ellipse-local coordinates
     * 2. Check (x/a)² + (y/b)² <= 1
     * 
     * Works correctly for rotated ellipses (O(1) complexity).
     * 
     * @code{.cpp}
     * Ellipse ellipse(10.0, 5.0, 0.0, 0.0, 30.0);  // Rotated 30°
     * 
     * assert(ellipse.isInside({0, 0}));      // Center - inside
     * assert(ellipse.isInside({10, 0}));     // On boundary (major axis)
     * assert(!ellipse.isInside({15, 0}));    // Outside
     * 
     * // Rotation handled automatically
     * Point rotatedPoint{5, 3};
     * bool inside = ellipse.isInside(rotatedPoint);
     * @endcode
     * 
     * @note Boundary points return true (inclusive)
     * @note O(1) complexity regardless of rotation
     * @see getBounds() - Quick rejection test
     */
    bool isInside(const Point& point) const override;
    
    /**
     * @brief Get axis-aligned bounding box
     * @return Smallest axis-aligned Bounds containing ellipse
     * 
     * For rotated ellipses, computes tight-fitting bounding box analytically.
     * 
     * @code{.cpp}
     * // Axis-aligned ellipse
     * Ellipse aligned(10, 5, 0, 0);
     * Bounds b1 = aligned.getBounds();
     * // b1 = [-10, -5, 10, 5] (exact fit)
     * 
     * // Rotated ellipse (45 degrees)
     * Ellipse rotated(10, 5, 0, 0, 45);
     * Bounds b2 = rotated.getBounds();
     * // b2 is larger (contains rotated ellipse)
     * @endcode
     * 
     * @note O(1) complexity (analytical formula)
     * @note Bounds are axis-aligned (no rotation)
     * @see isInside() - Precise containment test
     */
    Bounds getBounds() const override;
    
    /**
     * @brief Get contour points along ellipse perimeter
     * @param stepSize Target distance between consecutive points
     * @return Vector of points forming ellipse boundary
     * 
     * Generates points parametrically along ellipse, spacing them
     * approximately stepSize apart. Number of points is based on
     * perimeter / stepSize.
     * 
     * @code{.cpp}
     * Ellipse ellipse(10, 5, 0, 0);
     * 
     * // Generate contour with ~1 unit spacing
     * auto contour = ellipse.getContour(1.0);
     * // ~47 points (perimeter ≈ 47.1)
     * 
     * // Fine contour
     * auto fine = ellipse.getContour(0.1);
     * // ~471 points
     * 
     * // Verify closure
     * assert(contour.front().isNear(contour.back(), stepSize * 2));
     * @endcode
     * 
     * @param stepSize Target spacing between points
     * @return Points in world coordinates (rotation applied)
     * @note Point count = perimeter / stepSize (approximately)
     * @note Contour forms closed loop
     * @see perimeter() - Get total perimeter
     */
    std::vector<Point> getContour(double stepSize) const override;
    
    /**
     * @brief Test if point is on the ellipse contour (boundary)
     * @param point Point to test in world coordinates
     * @param tolerance Distance tolerance in current units
     * @return true if point is within tolerance of the ellipse boundary
     * 
     * Tests if a point lies on or near the ellipse boundary by checking
     * if the point is inside the enlarged ellipse (axes + tolerance) but
     * not inside the diminished ellipse (axes - tolerance).
     * 
     * @code{.cpp}
     * Ellipse ellipse(50.0, 30.0, 100.0, 100.0, 45.0);
     * 
     * // Point exactly on boundary
     * Point onEdge{150.0, 100.0};  // On major axis
     * assert(ellipse.isOnContour(onEdge, 0.1));
     * 
     * // Point near boundary
     * Point nearEdge{151.0, 100.0};  // 1 unit outside
     * assert(ellipse.isOnContour(nearEdge, 2.0));
     * 
     * // Point far from boundary
     * Point farAway{200.0, 100.0};
     * assert(!ellipse.isOnContour(farAway, 2.0));
     * 
     * // Works with rotation
     * assert(ellipse.isOnContour(ellipse.center(), tolerance) == false);  // Center not on edge
     * @endcode
     * 
     * @param point Point to test (in same coordinate system as ellipse)
     * @param tolerance Maximum distance from boundary (must be positive)
     * @return true if distance to boundary <= tolerance
     * 
     * @note Efficient O(1) implementation using ellipse equation
     * @note Handles rotated ellipses correctly
     * @see isInside() - Interior containment test
     */
    bool isOnContour(const Point& point, double tolerance) const override;
    
    /**
     * @brief Calculate perimeter using Ramanujan's approximation
     * @return Approximate perimeter length
     * 
     * Uses Ramanujan's second approximation formula:
     * ```
     * h = ((a-b)/(a+b))²
     * P ≈ π(a+b)(1 + 3h/(10 + sqrt(4-3h)))
     * ```
     * 
     * Accurate to within 0.01% for most ellipses.
     * Exact for circles (a = b).
     * 
     * @code{.cpp}
     * // Circle radius 10
     * Ellipse circle(10, 10, 0, 0);
     * double p1 = circle.perimeter();
     * assert(std::abs(p1 - 2*M_PI*10) < 0.001);  // Exact
     * 
     * // Ellipse 15x8
     * Ellipse ellipse(15, 8, 0, 0);
     * double p2 = ellipse.perimeter();  // ~72.4
     * 
     * // Rotation doesn't change perimeter
     * Ellipse rotated(15, 8, 0, 0, 45);
     * assert(rotated.perimeter() == p2);
     * @endcode
     * 
     * @note Rotation-invariant
     * @note Very accurate approximation
     * @see area() - Calculate enclosed area
     */
    double perimeter() const override;
    
    /**
     * @brief Calculate area
     * @return Area in square units
     * 
     * Area = π × a × b
     * 
     * Exact formula. Rotation-invariant.
     * 
     * @code{.cpp}
     * // Circle radius 10
     * Ellipse circle(10, 10, 0, 0);
     * assert(circle.area() == M_PI * 100);  // π×10²
     * 
     * // Ellipse 15x8
     * Ellipse ellipse(15, 8, 0, 0);
     * assert(ellipse.area() == M_PI * 15 * 8);  // π×15×8
     * 
     * // Rotation doesn't change area
     * Ellipse rotated(15, 8, 0, 0, 45);
     * assert(rotated.area() == ellipse.area());
     * @endcode
     * 
     * @note Exact calculation (not approximation)
     * @note Rotation-invariant
     * @see perimeter() - Calculate boundary length
     */
    double area() const override;
    
    /**
     * @brief Create deep copy of ellipse
     * @return Unique pointer to cloned ellipse
     * 
     * Creates independent copy including all geometry and state.
     * 
     * @code{.cpp}
     * Ellipse original(10, 5, 0, 0, 30);
     * original.setTypeLimits(TypeLimits::EXTERNAL);
     * 
     * std::unique_ptr<Shape> clone = original.clone();
     * 
     * // Clone is independent
     * clone->shiftX(100);  // Move clone
     * // original unchanged
     * @endcode
     * 
     * @return Unique pointer to new Ellipse instance
     * @see Shape::clone() - Base class interface
     */
    std::unique_ptr<Shape> clone() const override;
    
    /**
     * @brief Get type name for debugging/serialization
     * @return "Ellipse"
     * 
     * @code{.cpp}
     * Ellipse e(10, 5, 0, 0);
     * assert(std::string(e.typeName()) == "Ellipse");
     * @endcode
     */
    const char* typeName() const override { return "Ellipse"; }
    
    // Ellipse-specific properties

    /**
     * @brief Get center point in world coordinates
     * @return Center point
     * 
     * @code{.cpp}
     * Ellipse e(10, 5, 50, 25);
     * Point c = e.center();  // {50, 25}
     * @endcode
     */
    Point center() const { return center_; }
    
    /**
     * @brief Get semi-major axis length
     * @return Semi-major axis (larger radius)
     * 
     * @code{.cpp}
     * Ellipse e(15, 8, 0, 0);
     * assert(e.semiMajor() == 15.0);
     * @endcode
     * 
     * @note This is radius from center to edge (not full width)
     * @see semiMinor() - Get minor axis
     */
    double semiMajor() const { return semiMajor_; }
    
    /**
     * @brief Get semi-minor axis length
     * @return Semi-minor axis (smaller radius)
     * 
     * @code{.cpp}
     * Ellipse e(15, 8, 0, 0);
     * assert(e.semiMinor() == 8.0);
     * @endcode
     * 
     * @note This is radius from center to edge (not full height)
     * @see semiMajor() - Get major axis
     */
    double semiMinor() const { return semiMinor_; }
    
    /**
     * @brief Get rotation angle in degrees
     * @return Rotation angle (counter-clockwise from +X)
     * 
     * @code{.cpp}
     * Ellipse e(10, 5, 0, 0, 30);
     * assert(e.rotationDegrees() == 30.0);
     * @endcode
     * 
     * @see rotationRadians() - Get in radians
     */
    double rotationDegrees() const { return rotationDeg_; }
    
    /**
     * @brief Get rotation angle in radians
     * @return Rotation angle (counter-clockwise from +X)
     * 
     * @code{.cpp}
     * Ellipse e(10, 5, 0, 0, 45);
     * double rad = e.rotationRadians();
     * assert(std::abs(rad - M_PI/4) < 1e-6);
     * @endcode
     * 
     * @see rotationDegrees() - Get in degrees
     */
    double rotationRadians() const { return rotationRad_; }
    
    /**
     * @brief Check if ellipse is actually a circle
     * @param tolerance Tolerance for a==b comparison
     * @return true if semi-major and semi-minor are within tolerance
     * 
     * @code{.cpp}
     * Ellipse circle(10, 10, 0, 0);
     * assert(circle.isCircle());
     * 
     * Ellipse nearCircle(10, 9.999, 0, 0);
     * assert(nearCircle.isCircle(0.01));
     * 
     * Ellipse ellipse(10, 5, 0, 0);
     * assert(!ellipse.isCircle());
     * @endcode
     */
    bool isCircle(double tolerance = 1e-6) const {
        return std::abs(semiMajor_ - semiMinor_) < tolerance;
    }
    
    /**
     * @brief Calculate eccentricity
     * @return Eccentricity (0 for circle, <1 for ellipse)
     * 
     * Eccentricity measures how "stretched" the ellipse is:
     * ```
     * e = sqrt(1 - b²/a²)
     * ```
     * 
     * - e = 0: Perfect circle
     * - 0 < e < 1: Ellipse
     * - e → 1: Very elongated
     * 
     * @code{.cpp}
     * Ellipse circle(10, 10, 0, 0);
     * assert(circle.eccentricity() == 0.0);  // Circle
     * 
     * Ellipse ellipse(10, 5, 0, 0);
     * double e = ellipse.eccentricity();
     * // e ≈ 0.866 (fairly elongated)
     * @endcode
     * 
     * @note Always 0 <= e < 1 for valid ellipse
     * @see focalDistance() - Related focal property
     */
    double eccentricity() const;
    
    /**
     * @brief Calculate focal distance
     * @return Distance from center to focus
     * 
     * Focal distance (linear eccentricity):
     * ```
     * c = sqrt(a² - b²)
     * ```
     * 
     * The two foci are at distance c from center along major axis.
     * 
     * @code{.cpp}
     * Ellipse circle(10, 10, 0, 0);
     * assert(circle.focalDistance() == 0.0);  // Foci at center
     * 
     * Ellipse ellipse(10, 5, 0, 0);
     * double c = ellipse.focalDistance();
     * // c ≈ 8.66 (foci at ±8.66 along major axis)
     * @endcode

     * @note Returns 0 for circles
     * @see eccentricity() - Related measure of elongation
     */
    double focalDistance() const;
    
    // Coordinate transformation interface implementation
    
    /**
     * @brief Normalize coordinates to unit system
     * @param originX Origin X in measuring system
     * @param originY Origin Y in measuring system
     * @param radius Normalization scale
     * 
     * Transforms: (a_norm, b_norm, center_norm) = (a/radius, b/radius, (center-origin)/radius)
     * 
     * @code{.cpp}
     * Ellipse e(100, 50, 200, 150);
     * e.normalize(200, 150, 50);
     * 
     * // Now: a=2, b=1, center=(0,0)
     * assert(e.isNormalized());
     * @endcode
     * 
     * @see denormalize() - Inverse operation
     */
    void normalize(double originX, double originY, double radius) override;
    
    /**
     * @brief Denormalize coordinates to measuring system
     * @param originX Origin X in measuring system
     * @param originY Origin Y in measuring system
     * @param radius Normalization scale
     * 
     * Inverse of normalize().
     * 
     * @code{.cpp}
     * Ellipse e(2, 1, 0, 0);  // Normalized
     * e.denormalize(200, 150, 50);
     * 
     * // Now: a=100, b=50, center=(200,150)
     * assert(e.isMeasuring());
     * @endcode
     * 
     * @warning Must use SAME parameters as normalize() call
     * @see normalize() - Forward operation
     */
    void denormalize(double originX, double originY, double radius) override;
    
    /**
     * @brief Invert Y coordinate (flip across horizontal line)
     * @param centerY Y coordinate of inversion axis
     * 
     * Mirrors ellipse: y_new = centerY - y_old
     * Also inverts rotation angle.
     * 
     * @code{.cpp}
     * Ellipse e(10, 5, 100, 100, 30);
     * e.inverseY(400);
     * 
     * // Center Y: 100 -> 300
     * // Rotation: 30° -> -30°
     * assert(e.center().y == 300);
     * assert(e.rotationDegrees() == -30);
     * @endcode
     * 
     * @see transformToSystem() - System conversion
     */
    void inverseY(double centerY) override;
    
    /**
     * @brief Shift ellipse in X direction
     * @param deltaX Amount to shift (positive=right)
     * 
     * @code{.cpp}
     * Ellipse e(10, 5, 0, 0);
     * e.shiftX(50);
     * 
     * assert(e.center().x == 50);
     * @endcode
     * 
     * @see shiftY() - Vertical shift
     */
    void shiftX(double deltaX) override;
    
    /**
     * @brief Shift ellipse in Y direction
     * @param deltaY Amount to shift (direction depends on coordinate system)
     * 
     * Direction:
     * - SCREEN: positive = down
     * - MATH: positive = up
     * 
     * @code{.cpp}
     * Ellipse e(10, 5, 0, 0);
     * e.shiftY(25);
     * 
     * assert(e.center().y == 25);
     * @endcode
     * 
     * @note Direction depends on coordinate system
     * @see shiftX() - Horizontal shift
     */
    void shiftY(double deltaY) override;
    
    // ========================================================================
    // Handle Enumeration (Interactive Editing - UX spec §4)
    // ========================================================================
    
    /**
     * @brief Enumerate interactive handles for ellipse editing
     * @param out Output vector to receive handle descriptors
     * 
     * Ellipse provides handles for:
     * - Move (§2.1, §4.2): center point
     * - Rotate (§2.2, §4.3): offset along major axis normal
     * - AxisResize (§4.1): 4 endpoints (2 major + 2 minor)
     * 
     * Total: 6 handles (1 move + 1 rotate + 4 axis endpoints)
     * 
     * @param out Vector to append handle descriptors to
     * 
     * @see Shape::EnumerateHandles()
     * @see shapes_handles.md §4 - Ellipse handles specification
     */
    void EnumerateHandles(std::vector<HandleDesc>& out) const override;
    
    /**
     * @brief Apply handle drag to update ellipse geometry
     * @param handle Handle being dragged
     * @param drag Drag context with world-space positions and modifiers
     * 
     * Supported handle types:
     * - Move: Translate ellipse by drag.deltaWorld
     * - Rotate: Update rotation angle based on drag position
     * - AxisResize: Scale major/minor radius (index 0-1=major, 2-3=minor)
     * 
     * @param handle Frozen handle descriptor from drag start
     * @param drag Drag context with world-space delta and modifier keys
     * 
     * @see Shape::ApplyHandleDrag()
     * @see shapes_handles.md §4.1 - Ellipse axis resize
     */
    void ApplyHandleDrag(const HandleDesc& handle, const DragContext& drag) override;

private:
    double semiMajor_;      ///< Semi-major axis (A) - larger radius
    double semiMinor_;      ///< Semi-minor axis (B) - smaller radius
    Point center_;          ///< Center point in world coordinates
    double rotationDeg_;    ///< Rotation in degrees (counter-clockwise)
    double rotationRad_;    ///< Rotation in radians (cached for efficiency)
    
    // Cached trigonometric values for fast coordinate transformation
    double cosRot_;         ///< cos(rotationRad_) - cached
    double sinRot_;         ///< sin(rotationRad_) - cached
    
    /**
     * @brief Update cached rotation trigonometric values
     * 
     * Recalculates cosRot_ and sinRot_ from rotationRad_.
     * Called whenever rotation angle changes.
     */
    void updateRotationCache();
    
    /**
     * @brief Transform point from world to ellipse-local coordinates
     * @param point Point in world coordinates
     * @return Point in ellipse-local coordinates (centered, aligned)
     * 
     * Performs:
     * 1. Translate to ellipse-relative: p' = p - center
     * 2. Rotate by -rotation: p_local = Rotate(p', -θ)
     * 
     * Result is in ellipse frame (no rotation, center at origin).
     */
    Point toLocalCoordinates(const Point& point) const;
    
    /**
     * @brief Transform point from ellipse-local to world coordinates
     * @param point Point in ellipse-local coordinates
     * @return Point in world coordinates
     * 
     * Inverse of toLocalCoordinates():
     * 1. Rotate by +rotation: p' = Rotate(p, +θ)
     * 2. Translate: p_world = p' + center
     */
    Point toWorldCoordinates(const Point& point) const;
};

} // namespace aperture
