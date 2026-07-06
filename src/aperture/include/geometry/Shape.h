/**
 * @file Shape.h
 * @brief Abstract base class for all geometric shapes
 * 
 * ## Overview
 * 
 * Shape defines the common interface for all 2D geometric shapes in ApertureCore:
 * - Point containment testing (isInside)
 * - Bounding box calculation (getBounds)
 * - Contour generation (getContour)
 * - Geometric properties (area, perimeter)
 * - Deep copying (clone)
 * - Coordinate transformations (normalize, denormalize, inverseY)
 * 
 * ## Concrete Implementations
 * 
 * ApertureCore provides the following shape classes:
 * - **Ellipse** - Circles and ellipses with rotation
 * - **Rectangle** - Axis-aligned and rotated rectangles
 * - **Polygon** - Arbitrary closed polygons (convex or concave)
 * 
 * ## Key Features
 * 
 * ### 1. Polymorphic Shape Interface
 * 
 * All shapes share a common interface for basic operations:
 * 
 * @code{.cpp}
 * #include <aperturecore/geometry/Shape.h>
 * #include <aperturecore/geometry/Ellipse.h>
 * #include <aperturecore/geometry/Rectangle.h>
 * #include <memory>
 * #include <vector>
 * 
 * using namespace aperture;
 * 
 * // Work with any shape polymorphically
 * std::vector<std::unique_ptr<Shape>> shapes;
 * shapes.push_back(std::make_unique<Ellipse>(10.0, 10.0, 0.0, 0.0));
 * shapes.push_back(std::make_unique<Rectangle>(20.0, 15.0, 50.0, 50.0));
 * 
 * Point testPoint{5.0, 5.0};
 * for (const auto& shape : shapes) {
 *     if (shape->isInside(testPoint)) {
 *         std::cout << shape->typeName() << " contains point\n";
 *     }
 * }
 * @endcode
 * 
 * ### 2. Visibility Control (TypeLimits)
 * 
 * Each shape has a visibility type that controls how it affects point visibility:
 * - **EXTERNAL** - Aperture: only INSIDE is visible (blocks outside)
 * - **INTERNAL** - Obstruction: blocks inside, allows outside
 * - **APERTURE** - Opening: inside is visible, doesn't affect outside
 * 
 * @code{.cpp}
 * Ellipse mainAperture(50.0, 50.0, 0.0, 0.0);
 * mainAperture.setTypeLimits(TypeLimits::EXTERNAL);
 * 
 * Ellipse obstruction(10.0, 10.0, 0.0, 0.0);
 * obstruction.setTypeLimits(TypeLimits::INTERNAL);
 * @endcode
 * 
 * ### 3. Dual Coordinate System Support
 * 
 * Shapes track TWO independent coordinate properties:
 * 
 * **a) Spatial System (SCREEN vs MATH):**
 * - Affects Y-axis direction
 * - SCREEN: Y+ downward (for images, bitmaps)
 * - MATH: Y+ upward (for wavefront analysis)
 * 
 * **b) Normalization State (MEASURING vs NORMALIZED):**
 * - Affects coordinate scale
 * - MEASURING: Real-world physical units
 * - NORMALIZED: Unit coordinates centered at origin
 * 
 * @code{.cpp}
 * // Shape in screen coordinates, measuring units
 * Ellipse ellipse(50.0, 50.0, 100.0, 100.0);  
 * assert(ellipse.getSpatialSystem().isScreen());
 * assert(ellipse.isMeasuring());
 * 
 * // Normalize: transform to unit circle at origin
 * ellipse.normalize(100.0, 100.0, 50.0);
 * assert(ellipse.isNormalized());
 * // Center is now at (0, 0), semi-axes are (1.0, 1.0)
 * 
 * // Convert to math coordinates
 * ellipse.transformToSystem(CoordinateSystem::math(768.0));
 * assert(ellipse.getSpatialSystem().isMath());
 * @endcode
 * 
 * ### 4. Coordinate Transformations
 * 
 * Complete transformation pipeline for optical applications:
 * 
 * @code{.cpp}
 * // Start with shape in screen measuring coordinates
 * Rectangle rect(100.0, 50.0, 200.0, 150.0);  // Size 100x100
 * 
 * // 1. Normalize to unit system
 * rect.normalize(150.0, 100.0, 50.0);  // Center (150,100), radius 50
 * // Now in normalized coords: center ~ (0,0), size ~ (2,1)
 * 
 * // 2. Do calculations in normalized space
 * // ... (wavefront analysis, etc.)
 * 
 * // 3. Denormalize back to measuring coords
 * rect.denormalize(150.0, 100.0, 50.0);
 * // Back to original: (200, 150) with size 100x100
 * @endcode
 * 
 * ### 5. Clone Pattern
 * 
 * All shapes support deep copying via clone():
 * 
 * @code{.cpp}
 * Ellipse original(10.0, 5.0, 0.0, 0.0, 45.0);
 * original.setTypeLimits(TypeLimits::EXTERNAL);
 * 
 * std::unique_ptr<Shape> copy = original.clone();
 * // copy is a new Ellipse with same properties
 * assert(copy->getTypeLimits() == TypeLimits::EXTERNAL);
 * @endcode
 * 
 * ## Design Patterns
 * 
 * ### Virtual Interface Pattern
 * 
 * Shape defines pure virtual methods that all concrete shapes must implement:
 * - Geometry operations (isInside, getBounds, etc.)
 * - Coordinate transformations (normalize, denormalize, etc.)
 * - Cloning (clone)
 * 
 * ### Non-Virtual Interface (NVI) Pattern
 * 
 * Common functionality is non-virtual and calls virtual helpers:
 * - TypeLimits getters/setters
 * - Coordinate system management
 * - State tracking
 * 
 * ## Legacy Compatibility
 * 
 * Shape replaces the legacy XYShape base class, maintaining:
 * - Normalization/denormalization interface
 * - Y-axis inversion (inverseY)
 * - X/Y shifting methods
 * - TypeLimits enum (with new APERTURE type)
 * 
 * ## Thread Safety
 * 
 * **Not thread-safe!** Shape methods are not synchronized.
 * - Read-only operations (isInside, getBounds) are safe when shape is immutable
 * - Modifications require external synchronization
 * 
 * ## Performance Considerations
 * 
 * - isInside: O(1) for ellipses/rectangles, O(n) for polygons
 * - getBounds: O(1) for most shapes, O(n) for rotated shapes
 * - getContour: O(perimeter / stepSize) - generates many points
 * - clone: Deep copy - allocates new memory
 * 
 * @see Ellipse, Rectangle, Polygon
 * @see TypeLimits, CoordinateSystem, NormalizationState
 * @see Point, Bounds
 */
#pragma once

#include "Point.h"
#include "Bounds.h"
#include "CoordinateSystem.h"
#include "Handle.h"
#include "../visibility/TypeLimits.h"
#include <vector>
#include <memory>

#ifdef MEASURING
#undef MEASURING
#endif
#ifdef NORMALIZED
#undef NORMALIZED
#endif

#ifdef EXTERNAL
#undef EXTERNAL
#endif

namespace aperture {

/**
 * @enum NormalizationState
 * @brief Coordinate normalization state for shapes
 * 
 * Tracks whether shape coordinates are in original measuring units
 * or normalized to a unit coordinate system.
 * 
 * Used in optical applications where:
 * 1. Measurements are made in real units (mm, pixels, etc.)
 * 2. Calculations done in normalized coordinates (radius = 1.0)
 * 3. Results transformed back to measuring units
 * 
 * @code{.cpp}
 * Shape* shape = ...;
 * 
 * if (shape->isMeasuring()) {
 *     // Coordinates in mm, pixels, etc.
 *     double actualRadius = shape->getBounds().width() / 2.0;
 * } else {
 *     // Normalized coordinates
 *     double normalizedRadius = 1.0;  // By definition
 * }
 * @endcode
 * 
 * @note This is independent of spatial coordinate system (SCREEN vs MATH)
 * @see Shape::normalize(), Shape::denormalize()
 */
enum class NormalizationState {
    MEASURING = 0,   ///< Original measuring coordinates (mm, pixels, etc.)
    NORMALIZED = 1   ///< Normalized coordinates (centered, unit scale)
};

/**
 * @class Shape
 * @brief Abstract base class for all 2D geometric shapes
 * 
 * Defines the interface that all concrete shapes must implement while
 * providing common functionality for visibility control, coordinate
 * system management, and transformation tracking.
 * 
 * ## Inheritance Hierarchy
 * 
 * ```
 * Shape (abstract)
 *  +-- Ellipse (circle, ellipse)
 *  +-- Rectangle (square, rectangle, rotated)
 *  +-- Polygon (arbitrary closed polygon)
 * ```
 * 
 * ## Pure Virtual Methods (Must Implement)
 * 
 * Geometry operations:
 * - isInside(point) - Point containment test
 * - getBounds() - Bounding box calculation
 * - getContour(stepSize) - Boundary point generation
 * - perimeter() - Boundary length
 * - area() - Enclosed area (optional, default 0.0)
 * - clone() - Deep copy creation
 * 
 * Coordinate transformations:
 * - normalize(origin, radius) - Transform to unit coords
 * - denormalize(origin, radius) - Transform to measuring coords
 * - inverseY(centerY) - Flip Y-axis
 * - shiftX(deltaX) - Translate in X
 * - shiftY(deltaY) - Translate in Y
 * 
 * Type identification:
 * - typeName() - String identifier for debugging
 * 
 * ## Provided Functionality (Non-Virtual)
 * 
 * Visibility management:
 * - getTypeLimits() / setTypeLimits() - Visibility behavior
 * 
 * Spatial coordinate system:
 * - getSpatialSystem() / setSpatialSystem() - SCREEN vs MATH
 * - transformToSystem() - Convert between systems
 * 
 * Normalization state:
 * - getNormalizationState() / setNormalizationState()
 * - isNormalized() / isMeasuring() - State queries
 * 
 * ## Usage Pattern
 * 
 * ### Creating Shapes
 * 
 * @code{.cpp}
 * // Create concrete shapes
 * Ellipse circle(50.0, 50.0, 0.0, 0.0);  // Circle radius 50
 * Rectangle rect(100.0, 80.0, 0.0, 0.0); // Rectangle 100x80
 * 
 * // Use polymorphically
 * std::unique_ptr<Shape> shape = std::make_unique<Ellipse>(10.0, 10.0, 0.0, 0.0);
 * @endcode
 * 
 * ### Basic Geometry Operations
 * 
 * @code{.cpp}
 * Shape* shape = ...;
 * 
 * // Test point containment
 * Point p{5.0, 5.0};
 * if (shape->isInside(p)) {
 *     std::cout << "Point is inside!\n";
 * }
 * 
 * // Get bounding box
 * Bounds bounds = shape->getBounds();
 * std::cout << "Width: " << bounds.width() << "\n";
 * 
 * // Get contour points
 * auto contour = shape->getContour(1.0);  // Points ~1.0 apart
 * std::cout << "Contour has " << contour.size() << " points\n";
 * 
 * // Calculate properties
 * double area = shape->area();
 * double perim = shape->perimeter();
 * @endcode
 * 
 * ### Visibility Control
 * 
 * @code{.cpp}
 * // Create aperture (only inside is visible)
 * auto aperture = std::make_unique<Ellipse>(50.0, 50.0, 0.0, 0.0);
 * aperture->setTypeLimits(TypeLimits::EXTERNAL);
 * 
 * // Create obstruction (blocks inside)
 * auto obstruction = std::make_unique<Ellipse>(10.0, 10.0, 0.0, 0.0);
 * obstruction->setTypeLimits(TypeLimits::INTERNAL);
 * 
 * // Create opening (inside visible, doesn't block outside)
 * auto opening = std::make_unique<Rectangle>(20.0, 20.0, 30.0, 30.0);
 * opening->setTypeLimits(TypeLimits::APERTURE);
 * @endcode
 * 
 * ### Coordinate Transformations
 * 
 * @code{.cpp}
 * // Normalize for calculations
 * shape->normalize(centerX, centerY, radius);
 * assert(shape->isNormalized());
 * 
 * // Do work in normalized space...
 * 
 * // Denormalize back
 * shape->denormalize(centerX, centerY, radius);
 * assert(shape->isMeasuring());
 * @endcode
 * 
 * ### System Conversion
 * 
 * @code{.cpp}
 * // Shape in screen coordinates
 * shape->setSpatialSystem(CoordinateSystem::screen(768.0));
 * 
 * // Convert to math coordinates
 * shape->transformToSystem(CoordinateSystem::math(768.0));
 * // Y-coordinates inverted around height/2
 * @endcode
 * 
 * ### Cloning
 * 
 * @code{.cpp}
 * std::unique_ptr<Shape> original = std::make_unique<Ellipse>(...);
 * original->setTypeLimits(TypeLimits::EXTERNAL);
 * 
 * // Create independent copy
 * std::unique_ptr<Shape> copy = original->clone();
 * // Modify copy without affecting original
 * copy->setTypeLimits(TypeLimits::INTERNAL);
 * @endcode
 * 
 * ## State Management
 * 
 * Shape tracks three independent states:
 * 
 * | State | Type | Values | Meaning |
 * |-------|------|--------|---------|
 * | Visibility | TypeLimits | EXTERNAL, INTERNAL, APERTURE | How shape affects visibility |
 * | Spatial | CoordinateSystem | SCREEN, MATH | Y-axis direction |
 * | Normalization | NormalizationState | MEASURING, NORMALIZED | Coordinate scale |
 * 
 * These are **independent** - any combination is valid:
 * - EXTERNAL + SCREEN + MEASURING
 * - INTERNAL + MATH + NORMALIZED
 * - APERTURE + SCREEN + NORMALIZED
 * - etc.
 * 
 * ## Memory Management
 * 
 * Shapes are designed for use with smart pointers:
 * 
 * @code{.cpp}
 * // Recommended: use unique_ptr
 * std::unique_ptr<Shape> shape = std::make_unique<Ellipse>(...);
 * 
 * // Polymorphic collections
 * std::vector<std::unique_ptr<Shape>> shapes;
 * shapes.push_back(std::make_unique<Rectangle>(...));
 * shapes.push_back(std::make_unique<Polygon>(...));
 * 
 * // Clone creates new unique_ptr
 * std::unique_ptr<Shape> copy = shape->clone();
 * @endcode
 * 
 * ## Coordinate System Independence
 * 
 * Shape provides two levels of coordinate system support:
 * 
 * **1. Spatial System (Y-axis direction):**
 * - Shapes work in either SCREEN or MATH coordinates
 * - transformToSystem() converts between them
 * - Affects: rotation direction, Y comparisons, bounds
 * 
 * **2. Normalization State (scale/origin):**
 * - Shapes work in MEASURING or NORMALIZED coordinates
 * - normalize() / denormalize() convert between them
 * - Affects: coordinate interpretation, calculations
 * 
 * These are orthogonal - a shape can be:
 * - SCREEN + MEASURING (e.g., bitmap coordinates)
 * - SCREEN + NORMALIZED (e.g., normalized bitmap)
 * - MATH + MEASURING (e.g., wavefront data)
 * - MATH + NORMALIZED (e.g., unit pupil)
 * 
 * ## Implementation Notes
 * 
 * ### For Shape Implementers
 * 
 * When creating a new Shape subclass:
 * 
 * 1. **Implement all pure virtual methods**
 *    - Geometry ops: isInside, getBounds, getContour, perimeter
 *    - Transformations: normalize, denormalize, inverseY, shiftX, shiftY
 *    - Cloning: clone (remember to copy all state!)
 *    - Identification: typeName
 * 
 * 2. **Handle coordinate systems correctly**
 *    - Check spatialSystem_ when dealing with Y-coordinates
 *    - Update normState_ in normalize/denormalize
 *    - Copy all state in clone()
 * 
 * 3. **Support TypeLimits**
 *    - Don't override getTypeLimits/setTypeLimits
 *    - Base class handles typeLimits_ storage
 * 
 * @code{.cpp}
 * class MyShape : public Shape {
 * public:
 *     bool isInside(const Point& point) const override {
 *         // Implement containment test
 *     }
 *     
 *     Bounds getBounds() const override {
 *         // Calculate and return bounding box
 *         // Remember to set correct spatial system!
 *         return Bounds{...}.setSpatialSystem(spatialSystem_);
 *     }
 *     
 *     std::unique_ptr<Shape> clone() const override {
 *         auto copy = std::make_unique<MyShape>(*this);
 *         // Base class state copied by default copy constructor
 *         return copy;
 *     }
 *     
 *     const char* typeName() const override {
 *         return "MyShape";
 *     }
 *     
 *     // ... implement other virtual methods
 * };
 * @endcode
 * 
 * @note Replaces legacy XYShape base class
 * @see Ellipse - Circular and elliptical shapes
 * @see Rectangle - Rectangular shapes (with rotation)
 * @see Polygon - Arbitrary polygonal shapes
 * @see TypeLimits - Visibility behavior enumeration
 * @see CoordinateSystem - Spatial coordinate system (SCREEN vs MATH)
 * @see NormalizationState - Coordinate scale state
 */
class Shape {
public:
    virtual ~Shape() = default;
    
    // Core geometric interface (pure virtual)
    
    /**
     * @brief Test if a point is inside the shape
     * @param point Point to test in current coordinate system
     * @return true if point is inside or on the shape boundary
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Behavior:
     * - Returns true if point is strictly inside
     * - Boundary points should return true (inclusive)
     * - Implementation depends on shape type (ellipse, rectangle, polygon)
     * 
     * @code{.cpp}
     * Ellipse circle(50.0, 50.0, 100.0, 100.0);  // Radius 50 at (100,100)
     * 
     * Point inside{100.0, 100.0};   // Center
     * Point boundary{150.0, 100.0}; // On edge (radius away)
     * Point outside{200.0, 100.0};  // Far outside
     * 
     * assert(circle.isInside(inside));    // true - inside
     * assert(circle.isInside(boundary));  // true - on boundary
     * assert(!circle.isInside(outside));  // false - outside
     * @endcode
     * 
     * @note Coordinate system awareness: implementations should handle
     *       both SCREEN and MATH coordinates correctly
     * @see getBounds() - for quick rejection test
     * @see contains() - Bounds version for box test
     */
    virtual bool isInside(const Point& point) const = 0;
    
    /**
     * @brief Get axis-aligned bounding box
     * @return Smallest Bounds containing the entire shape
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * The returned bounds should:
     * - Be axis-aligned (even if shape is rotated)
     * - Contain all points on shape boundary
     * - Use same coordinate system as shape
     * - Be minimal (tight fit)
     * 
     * @code{.cpp}
     * // Rotated rectangle
     * Rectangle rect(100.0, 50.0, 0.0, 0.0, 45.0);  // 100x50 rotated 45deg
     * Bounds bounds = rect.getBounds();
     * 
     * // Bounding box is larger than original rectangle (rotated)
     * assert(bounds.width() > 100.0);  // Diagonal width
     * assert(bounds.height() > 50.0);  // Diagonal height
     * 
     * // All shape points are inside bounds
     * for (const auto& p : rect.getContour(1.0)) {
     *     assert(bounds.contains(p));
     * }
     * @endcode
     * 
     * @note Common use: quick rejection in isInside() implementation
     * @note Result has same spatial system as shape
     * @see isInside(), contains()
     */
    virtual Bounds getBounds() const = 0;
    
    /**
     * @brief Get contour points of the shape boundary
     * @param stepSize Maximum distance between consecutive points (in current units)
     * @return Vector of points forming the shape boundary
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Generates points along the shape boundary, approximately stepSize apart.
     * Points should form a closed contour (first and last points close or identical).
     * 
     * Typical uses:
     * - Drawing/rendering shape outline
     * - Calculating accurate perimeter
     * - Exporting to polygon representation
     * - Collision detection
     * 
     * @code{.cpp}
     * Ellipse circle(50.0, 50.0, 0.0, 0.0);
     * 
     * // Coarse contour (~50 points)
     * auto coarse = circle.getContour(10.0);
     * 
     * // Fine contour (~300 points)
     * auto fine = circle.getContour(1.0);
     * 
     * // Verify closed contour
     * assert(coarse.front().isNear(coarse.back(), stepSize * 2));
     * @endcode
     * 
     * @param stepSize Target spacing between points. Actual spacing may vary.
     *                 Smaller values = more points = higher accuracy.
     * @return Vector of boundary points in current coordinate system.
     *         First and last points should form a closed loop.
     * 
     * @note Point count depends on perimeter and stepSize
     * @note Returned points are in shape's current coordinate system
     * @warning Large shapes with small stepSize generate many points!
     * @see perimeter() - total boundary length
     */
    virtual std::vector<Point> getContour(double stepSize) const = 0;
    
    /**
     * @brief Test if a point is on the shape's contour (boundary)
     * @param point Point to test in current coordinate system
     * @param tolerance Distance tolerance (in current units)
     * @return true if point is within tolerance distance of the shape boundary
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Tests whether a point lies on or near the shape's boundary within
     * the specified tolerance. This is useful for:
     * - Interactive selection (clicking near shape edge)
     * - Hit testing for editing handles
     * - Proximity detection for snapping
     * - Boundary validation
     * 
     * Implementation strategies:
     * - **Ellipse/Rectangle**: Test if point is inside enlarged shape but
     *   not inside diminished shape (tolerance applied to axes/sides)
     * - **Polygon**: Check minimum distance to any edge segment
     * - Other efficient geometric algorithms as appropriate
     * 
     * @code{.cpp}
     * Ellipse circle(50.0, 50.0, 100.0, 100.0);  // Radius 50 at (100,100)
     * 
     * Point onBoundary{150.0, 100.0};    // Exactly on edge
     * Point nearBoundary{151.0, 100.0};  // 1 unit outside
     * Point farAway{200.0, 100.0};       // 50 units outside
     * 
     * assert(circle.isOnContour(onBoundary, 0.1));     // true - on edge
     * assert(circle.isOnContour(nearBoundary, 2.0));   // true - within tolerance
     * assert(!circle.isOnContour(farAway, 2.0));       // false - too far
     * 
     * // Works for rotated shapes
     * Rectangle rect(100.0, 50.0, 0.0, 0.0, 45.0);
     * Point nearEdge = rect.corners()[0];  // Get corner
     * assert(rect.isOnContour(nearEdge, 0.1));  // true - on corner
     * @endcode
     * 
     * @param point Point to test (in same coordinate system as shape)
     * @param tolerance Maximum distance from boundary to consider "on contour".
     *                  Must be positive. Larger values = more lenient.
     * @return true if distance from point to nearest boundary point <= tolerance
     * 
     * @note Tolerance is in the same units as shape coordinates
     * @note For performance, tolerance should be reasonable (not too large)
     * @note Points exactly on boundary should return true with any positive tolerance
     * @note Interior points far from boundary should return false
     * 
     * @see isInside() - for interior point testing
     * @see getContour() - for boundary point generation
     * @see getBounds() - for quick rejection test
     */
    virtual bool isOnContour(const Point& point, double tolerance) const = 0;
    
    /**
     * @brief Calculate perimeter/circumference of the shape
     * @return Total length of shape boundary in current units
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * For closed shapes, returns the total boundary length.
     * For analytical shapes (ellipse, rectangle), uses exact formulas.
     * For polygons, sums edge lengths.
     * 
     * @code{.cpp}
     * // Circle radius 10
     * Ellipse circle(10.0, 10.0, 0.0, 0.0);
     * double perim = circle.perimeter();
     * assert(std::abs(perim - 2.0 * M_PI * 10.0) < 0.01);  // 2*pi*r
     * 
     * // Rectangle 20 x 10
     * Rectangle rect(20.0, 10.0, 0.0, 0.0);
     * assert(rect.perimeter() == 60.0);  // 2*(20+10)
     * @endcode
     * 
     * @note Units match current coordinate system (measuring or normalized)
     * @note For rotated shapes, perimeter is independent of rotation
     * @see area(), getContour()
     */
    virtual double perimeter() const = 0;
    
    /**
     * @brief Create a deep copy of the shape
     * @return Unique pointer to cloned shape with same properties
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Creates an independent copy of the shape including:
     * - Geometric parameters (size, position, rotation)
     * - TypeLimits setting
     * - Spatial coordinate system
     * - Normalization state
     * 
     * The clone is a complete deep copy - modifying it does not affect
     * the original.
     * 
     * @code{.cpp}
     * Ellipse original(50.0, 30.0, 100.0, 100.0, 45.0);
     * original.setTypeLimits(TypeLimits::EXTERNAL);
     * 
     * std::unique_ptr<Shape> clone = original.clone();
     * 
     * // Clone is independent
     * clone->setTypeLimits(TypeLimits::INTERNAL);
     * assert(original.getTypeLimits() == TypeLimits::EXTERNAL);  // Unchanged
     * 
     * // But has same geometry
     * assert(clone->getBounds() == original.getBounds());
     * @endcode
     * 
     * @return Unique pointer to new shape instance. Caller owns the memory.
     * 
     * @note Implementation must copy ALL state (geometry + base class members)
     * @see Shape copy constructor pattern
     */
    virtual std::unique_ptr<Shape> clone() const = 0;
    
    // Coordinate transformation interface (pure virtual)
    
    /**
     * @brief Normalize coordinates to unit system
     * @param originX Origin X coordinate in measuring system
     * @param originY Origin Y coordinate in measuring system
     * @param radius Normalization radius/scale factor
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Transforms shape from measuring coordinates to normalized coordinates:
     * - x_norm = (x_meas - originX) / radius
     * - y_norm = (y_meas - originY) / radius
     * 
     * After normalization:
     * - Origin is at (0, 0)
     * - Scale is relative to radius (radius = 1.0 unit)
     * - Normalization state becomes NORMALIZED
     * 
     * Common use in optics: normalize pupil to unit circle for aberration analysis.
     * 
     * @code{.cpp}
     * // Circle radius 50 centered at (100, 100)
     * Ellipse circle(50.0, 50.0, 100.0, 100.0);
     * assert(circle.isMeasuring());
     * 
     * // Normalize to unit circle at origin
     * circle.normalize(100.0, 100.0, 50.0);
     * 
     * assert(circle.isNormalized());
     * Bounds bounds = circle.getBounds();
     * // Now centered at ~(0,0) with radius ~1.0
     * assert(std::abs(bounds.center().x) < 0.01);
     * assert(std::abs(bounds.center().y) < 0.01);
     * @endcode
     * 
     * @param originX X-coordinate of normalization origin (measuring units)
     * @param originY Y-coordinate of normalization origin (measuring units)
     * @param radius Scale factor (measuring units). Points at distance=radius
     *               from origin become distance=1.0 in normalized coords.
     * 
     * @note Calling normalize() on already-normalized shape may give unexpected results
     * @note Shape remembers normalization state via setNormalizationState()
     * @see denormalize() - inverse operation
     * @see isNormalized(), isMeasuring()
     */
    virtual void normalize(double originX, double originY, double radius) = 0;
    
    /**
     * @brief Denormalize coordinates back to measuring system
     * @param originX Origin X coordinate in measuring system
     * @param originY Origin Y coordinate in measuring system
     * @param radius Normalization radius/scale factor
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Inverse of normalize(). Transforms from normalized to measuring coordinates:
     * - x_meas = x_norm * radius + originX
     * - y_meas = y_norm * radius + originY
     * 
     * After denormalization:
     * - Shape returns to measuring coordinate system
     * - Normalization state becomes MEASURING
     * 
     * Typical workflow:
     * 1. normalize() - transform to unit coords
     * 2. Do calculations in normalized space
     * 3. denormalize() - transform back to measuring coords
     * 
     * @code{.cpp}
     * Ellipse ellipse(...);
     * 
     * // Store original bounds
     * Bounds original = ellipse.getBounds();
     * 
     * // Normalize
     * ellipse.normalize(100.0, 100.0, 50.0);
     * // ... do work in normalized coordinates ...
     * 
     * // Denormalize with SAME parameters
     * ellipse.denormalize(100.0, 100.0, 50.0);
     * 
     * // Should restore original geometry
     * Bounds restored = ellipse.getBounds();
     * assert(original == restored);  // Back to original
     * assert(ellipse.isMeasuring());
     * @endcode
     * 
     * @param originX X-coordinate of normalization origin (measuring units)
     * @param originY Y-coordinate of normalization origin (measuring units)
     * @param radius Scale factor (measuring units)
     * 
     * @warning Must use SAME parameters as original normalize() call!
     * @note Calling denormalize() on measuring shape may give unexpected results
     * @see normalize() - forward operation
     * @see isNormalized(), isMeasuring()
     */
    virtual void denormalize(double originX, double originY, double radius) = 0;
    
    /**
     * @brief Invert Y coordinate (mirror across horizontal line)
     * @param centerY Y coordinate of inversion axis
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Mirrors the shape across a horizontal line at y = centerY.
     * New Y-coordinates: y_new = 2*centerY - y_old
     * 
     * Primary use: converting between SCREEN and MATH coordinate systems:
     * - SCREEN: Y+ downward (top=0)
     * - MATH: Y+ upward (bottom=0)
     * - Inversion at y=height/2 swaps the two
     * 
     * @code{.cpp}
     * // Rectangle in screen coordinates
     * Rectangle rect(100.0, 50.0, 100.0, 100.0);  // Top-left at (100,100)
     * rect.setSpatialSystem(CoordinateSystem::screen(768.0));
     * 
     * // Invert Y to convert to math coordinates
     * rect.inverseY(768.0 / 2.0);  // Mirror at screen center
     * // Now rectangle is at (100, 668) in math coordinates
     * 
     * // Invert again to go back
     * rect.inverseY(768.0 / 2.0);
     * // Back at (100, 100) in screen coordinates
     * @endcode
     * 
     * @param centerY Y-coordinate of horizontal mirror axis
     * 
     * @note This is a geometric transformation, not just a tag update
     * @note X-coordinates unchanged, only Y inverted
     * @note Calling twice with same centerY restores original
     * @see transformToSystem() - higher-level system conversion
     * @see shiftY() - simple Y translation
     */
    virtual void inverseY(double centerY) = 0;
    
    /**
     * @brief Shift shape in X direction
     * @param deltaX Amount to shift (positive = right, negative = left)
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Translates the shape horizontally without changing size, rotation,
     * or any other properties.
     * 
     * @code{.cpp}
     * Ellipse ellipse(50.0, 50.0, 100.0, 100.0);  // Center at (100, 100)
     * 
     * ellipse.shiftX(50.0);  // Move right
     * // Now center at (150, 100)
     * 
     * ellipse.shiftX(-100.0);  // Move left
     * // Now center at (50, 100)
     * @endcode
     * 
     * @param deltaX Distance to shift in X direction (in current units)
     * 
     * @note Does not change shape size or rotation
     * @see shiftY(), inverseY()
     */
    virtual void shiftX(double deltaX) = 0;
    
    /**
     * @brief Shift shape in Y direction
     * @param deltaY Amount to shift (direction depends on coordinate system)
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Translates the shape vertically without changing size, rotation,
     * or any other properties.
     * 
     * Direction depends on spatial coordinate system:
     * - SCREEN: positive = down
     * - MATH: positive = up
     * 
     * @code{.cpp}
     * Rectangle rect(100.0, 50.0, 100.0, 100.0);  // Center at (100, 100)
     * 
     * rect.shiftY(50.0);
     * // SCREEN: center now at (100, 150) - moved down
     * // MATH: center now at (100, 150) - moved up
     * @endcode
     * 
     * @param deltaY Distance to shift in Y direction (in current units)
     * 
     * @note Direction is coordinate-system dependent!
     * @note Does not change shape size or rotation
     * @see shiftX(), inverseY()
     */
    virtual void shiftY(double deltaY) = 0;
    
    // ========================================================================
    // Handle Enumeration (Interactive Editing - UX spec shapes_handles.md)
    // ========================================================================
    
    /**
     * @brief Enumerate interactive handles for this shape
     * @param out Output vector to receive handle descriptors
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * ## Architecture (§11, §12 - shapes_handles.md):
     * 
     * **Shapes expose handles, BoundsHandler interprets interaction, Commands commit geometry.**
     * 
     * This method describes WHERE handles should appear and WHAT they mean,
     * but does NOT create actual runtime handle objects. BoundsHandler converts
     * HandleDesc → RuntimeHandle (screen space, hit-testing, cursor, etc.)
     * 
     * ## What This Method Does:
     * - ✅ Generate HandleDesc for each interactive control point
     * - ✅ Provide handle type (Move, Rotate, Resize, Vertex, etc.)
     * - ✅ Provide local position in shape-local coordinates
     * - ✅ Provide index for vertex/corner/edge identification
     * - ✅ Optionally provide normal for resize cursor orientation
     * 
     * ## What This Method Does NOT Do:
     * - ❌ Create persistent Handle objects
     * - ❌ Store handles as members
     * - ❌ Perform hit-testing
     * - ❌ Handle mouse interaction
     * - ❌ Set cursor
     * - ❌ Screen coordinate conversion
     * 
     * ## Example Implementation (Rectangle - §3):
     * 
     * @code{.cpp}
     * void Rectangle::EnumerateHandles(std::vector<HandleDesc>& out) const {
     *     // Move handle at center (§2.1, §3.2)
     *     out.push_back({HandleType::Move, -1, Center()});
     *     
     *     // Rotation handle offset from shape (§2.2, §3.3)
     *     out.push_back({HandleType::Rotate, -1, RotationHandlePos()});
     *     
     *     // 4 Corner resize handles (§3.1)
     *     for (int i = 0; i < 4; ++i) {
     *         Point corner = Corner(i);
     *         Point normal = (corner - Center()).normalized();
     *         out.push_back({HandleType::CornerResize, i, corner, normal});
     *     }
     *     
     *     // 4 Edge midpoint resize handles (§3.1)
     *     for (int i = 0; i < 4; ++i) {
     *         Point edge = EdgeMidpoint(i);
     *         Point normal = EdgeNormal(i);
     *         out.push_back({HandleType::EdgeResize, i, edge, normal});
     *     }
     * }
     * @endcode
     * 
     * @param out Vector to receive handle descriptors. Existing contents are preserved.
     *            Append new handles, don't clear vector.
     * 
     * @note Called every frame during editing - must be fast
     * @note HandleDesc uses shape-local coordinates (NOT world, NOT screen)
     * @note BoundsHandler transforms: local → world → screen
     * 
     * @see HandleDesc, HandleType, DragContext
     * @see ApplyHandleDrag() - responds to handle drag
     * @see shapes_handles.md - complete UX specification
     */
    virtual void EnumerateHandles(std::vector<HandleDesc>& out) const = 0;
    
    /**
     * @brief Apply handle drag to update shape geometry
     * @param handle Handle being dragged (frozen at drag start)
     * @param drag Drag context with start/current positions and modifiers
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * ## Contract (§11):
     * 
     * This method mutates shape geometry in response to handle drag.
     * It performs PURE GEOMETRIC TRANSFORMATION only - no side effects.
     * 
     * ## What This Method Does:
     * - ✅ Update shape geometry based on drag delta
     * - ✅ Respect modifier keys (Shift = constrain, Alt = symmetric)
     * - ✅ Maintain shape invariants (e.g. positive size)
     * - ✅ Work in world coordinates
     * 
     * ## What This Method Does NOT Do:
     * - ❌ Create undo commands
     * - ❌ Apply snapping (BoundsHandler responsibility)
     * - ❌ Set cursor
     * - ❌ Draw preview
     * - ❌ Modify document/selection
     * 
     * ## Preview vs Commit (critical!):
     * 
     * **BoundsHandler must call this on a temporary clone during drag.**
     * 
     * - MouseDown → create shape clone + freeze HandleDesc
     * - MouseMove → call ApplyHandleDrag() on clone (live preview)
     * - MouseUp → commit cloned shape via command
     * - Esc → discard clone
     * 
     * This ensures drag preview doesn't mutate original shape.
     * 
     * ## Example Implementation (Rectangle Move - §2.1):
     * 
     * @code{.cpp}
     * void Rectangle::ApplyHandleDrag(const HandleDesc& handle, const DragContext& drag) {
     *     switch (handle.type) {
     *         case HandleType::Move:
     *             // Simple translation
     *             m_centerX += drag.deltaWorld.x;
     *             m_centerY += drag.deltaWorld.y;
     *             break;
     *             
     *         case HandleType::CornerResize:
     *             // Resize from opposite corner
     *             if (drag.altKey) {
     *                 // Symmetric resize from center
     *                 ResizeSymmetric(drag.deltaWorld, drag.shiftKey);
     *             } else {
     *                 // Resize from opposite corner
     *                 ResizeFromCorner(handle.index, drag.deltaWorld, drag.shiftKey);
     *             }
     *             break;
     *         
     *         // ... other handle types ...
     *     }
     * }
     * @endcode
     * 
     * @param handle Frozen handle descriptor from drag start (IMMUTABLE during drag)
     * @param drag Drag context with world-space positions and modifier keys
     * 
     * @warning Do NOT query current mouse position - use drag.dragCurrentWorld
     * @warning Do NOT create commands - that's BoundsHandler's job
     * @warning Do NOT snap to grid - BoundsHandler applies snapping before calling this
     * 
     * @note All coordinates in drag are world-space
     * @note BoundsHandler handles screen→world transformation
     * @note Modifier keys are pre-queried by BoundsHandler
     * 
     * @see EnumerateHandles() - provides handle descriptors
     * @see HandleDesc, DragContext
     * @see shapes_handles.md §8 - mouse interaction lifecycle
     */
    virtual void ApplyHandleDrag(const HandleDesc& handle, const DragContext& drag) = 0;
    
    // TypeLimits management
    
    /**
     * @brief Get visibility type of this shape
     * @return Current TypeLimits setting
     * 
     * Returns the visibility behavior of this shape:
     * - **EXTERNAL**: Aperture - only inside is visible
     * - **INTERNAL**: Obstruction - blocks inside, allows outside
     * - **APERTURE**: Opening - inside visible, doesn't affect outside
     * 
     * @code{.cpp}
     * Ellipse ellipse(...);
     * ellipse.setTypeLimits(TypeLimits::EXTERNAL);
     * 
     * assert(ellipse.getTypeLimits() == TypeLimits::EXTERNAL);
     * @endcode
     * 
     * @note Default is TypeLimits::EXTERNAL (aperture)
     * @see setTypeLimits(), TypeLimits
     */
    TypeLimits getTypeLimits() const { 
        return typeLimits_; 
    }
    
    /**
     * @brief Set visibility type of this shape
     * @param type New TypeLimits value
     * 
     * Controls how this shape affects visibility calculations:
     * - **EXTERNAL**: Classic aperture - bounds visible region
     * - **INTERNAL**: Obstruction - blocks area inside shape
     * - **APERTURE**: Opening - creates visible region without blocking
     * 
     * @code{.cpp}
     * // Main telescope aperture
     * Ellipse mainAperture(50.0, 50.0, 0.0, 0.0);
     * mainAperture.setTypeLimits(TypeLimits::EXTERNAL);
     * 
     * // Secondary mirror obstruction
     * Ellipse secondary(10.0, 10.0, 0.0, 0.0);
     * secondary.setTypeLimits(TypeLimits::INTERNAL);
     * 
     * // Baffle opening
     * Rectangle opening(5.0, 5.0, 20.0, 20.0);
     * opening.setTypeLimits(TypeLimits::APERTURE);
     * @endcode
     * 
     * @see getTypeLimits(), TypeLimits, VisibilityChecker
     */
    void setTypeLimits(TypeLimits type) { 
        typeLimits_ = type; 
    }
    
    // Spatial coordinate system management (SCREEN vs MATH)
    
    /**
     * @brief Get spatial coordinate system
     * @return Current coordinate system (SCREEN or MATH)
     * 
     * Returns which Y-axis convention this shape uses:
     * - SCREEN: Y+ downward
     * - MATH: Y+ upward
     * 
     * @code{.cpp}
     * Shape* shape = ...;
     * 
     * if (shape->getSpatialSystem().isScreen()) {
     *     // Y increases downward
     * } else {
     *     // Y increases upward
     * }
     * @endcode
     * 
     * @see setSpatialSystem(), transformToSystem()
     * @see CoordinateSystem
     */
    CoordinateSystem getSpatialSystem() const {
        return spatialSystem_;
    }
    
    /**
     * @brief Set spatial coordinate system
     * @param system New coordinate system
     * 
     * Updates the coordinate system tag for this shape.
     * 
     * @warning This does NOT transform coordinates, only updates the tag!
     *          To actually convert coordinates, use transformToSystem().
     * 
     * Use this when:
     * - You've manually inverted Y-coordinates
     * - You're constructing a shape in a specific system
     * 
     * DON'T use this when:
     * - You want to convert between systems (use transformToSystem())
     * 
     * @code{.cpp}
     * // Manually create shape in math coordinates
     * Ellipse ellipse(50.0, 50.0, 100.0, 668.0);  // Math Y-coordinate
     * ellipse.setSpatialSystem(CoordinateSystem::math(768.0));
     * // Now shape knows it's in math coordinates
     * @endcode
     * 
     * @see getSpatialSystem(), transformToSystem()
     */
    void setSpatialSystem(const CoordinateSystem& system) {
        spatialSystem_ = system;
    }
    
    /**
     * @brief Transform shape to different spatial coordinate system
     * @param targetSystem Target coordinate system
     * 
     * Converts shape coordinates from current spatial system to target system.
     * For SCREEN ? MATH conversion, this inverts the Y-axis around the
     * reference height stored in the coordinate system.
     * 
     * This is the **correct way** to convert between coordinate systems -
     * it both transforms the coordinates AND updates the system tag.
     * 
     * @code{.cpp}
     * // Shape in screen coordinates
     * Ellipse ellipse(50.0, 50.0, 100.0, 50.0);  // Center at screen (100, 50)
     * ellipse.setSpatialSystem(CoordinateSystem::screen(768.0));
     * 
     * // Convert to math coordinates
     * ellipse.transformToSystem(CoordinateSystem::math(768.0));
     * // Now center is at math (100, 718)  -- 768 - 50 = 718
     * // And system tag is updated to MATH
     * 
     * assert(ellipse.getSpatialSystem().isMath());
     * @endcode
     * 
     * @param targetSystem Target coordinate system with reference height
     * 
     * @throws std::invalid_argument if reference height not set for conversion
     * @note If already in target system, does nothing
     * @note For SCREEN ? MATH, inverts Y around height/2
     * @see inverseY() - low-level Y inversion
     * @see setSpatialSystem() - tag-only update (no transformation)
     */
    virtual void transformToSystem(const CoordinateSystem& targetSystem) {
        if (spatialSystem_.type() == targetSystem.type()) {
            return;  // Already in target system
        }
        
        // Get reference height for Y-axis inversion
        double height = targetSystem.referenceHeight();
        if (height <= 0.0) {
            height = spatialSystem_.referenceHeight();
        }
        
        if (height <= 0.0) {
            throw std::invalid_argument(
                "Shape::transformToSystem: reference height must be set for coordinate conversion"
            );
        }
        
        // Invert Y-axis around midpoint
        inverseY(height / 2.0);
        
        // Update system tag
        spatialSystem_ = targetSystem;
    }
    
    // Normalization state management (MEASURING vs NORMALIZED)
    
    /**
     * @brief Get normalization state
     * @return Current normalization state (MEASURING or NORMALIZED)
     * 
     * Indicates whether shape coordinates are in:
     * - MEASURING: Original units (mm, pixels, etc.)
     * - NORMALIZED: Unit scale (radius=1.0)
     * 
     * @code{.cpp}
     * Shape* shape = ...;
     * 
     * if (shape->getNormalizationState() == NormalizationState::MEASURING) {
     *     // Coordinates in real units
     * } else {
     *     // Coordinates normalized (unit scale)
     * }
     * @endcode
     * 
     * @see setNormalizationState(), isNormalized(), isMeasuring()
     * @see normalize(), denormalize()
     */
    NormalizationState getNormalizationState() const {
        return normState_;
    }
    
    /**
     * @brief Set normalization state
     * @param state New normalization state
     * 
     * Updates the normalization state tag for this shape.
     * 
     * @warning This does NOT transform coordinates, only updates the tag!
     *          To actually normalize/denormalize, use normalize() or denormalize().
     * 
     * Use this when:
     * - You've manually transformed coordinates
     * - You're constructing a shape in a specific state
     * 
     * DON'T use this when:
     * - You want to normalize/denormalize (use those methods directly)
     * 
     * @code{.cpp}
     * // After manual normalization
     * shape->setNormalizationState(NormalizationState::NORMALIZED);
     * @endcode
     * 
     * @see getNormalizationState(), normalize(), denormalize()
     */
    void setNormalizationState(NormalizationState state) {
        normState_ = state;
    }
    
    /**
     * @brief Check if coordinates are normalized
     * @return true if in normalized coordinate system
     * 
     * Convenience method - equivalent to:
     * getNormalizationState() == NormalizationState::NORMALIZED
     * 
     * @code{.cpp}
     * Shape* shape = ...;
     * shape->normalize(0, 0, 50.0);
     * 
     * assert(shape->isNormalized());
     * assert(!shape->isMeasuring());
     * @endcode
     * 
     * @see isMeasuring(), getNormalizationState()
     * @see normalize(), denormalize()
     */
    bool isNormalized() const {
        return normState_ == NormalizationState::NORMALIZED;
    }
    
    /**
     * @brief Check if coordinates are in measuring system
     * @return true if in measuring coordinate system
     * 
     * Convenience method - equivalent to:
     * getNormalizationState() == NormalizationState::MEASURING
     * 
     * @code{.cpp}
     * Shape* shape = ...;
     * 
     * assert(shape->isMeasuring());  // Default state
     * 
     * shape->normalize(0, 0, 50.0);
     * assert(!shape->isMeasuring());
     * assert(shape->isNormalized());
     * @endcode
     * 
     * @see isNormalized(), getNormalizationState()
     * @see normalize(), denormalize()
     */
    bool isMeasuring() const {
        return normState_ == NormalizationState::MEASURING;
    }
    
    // Optional: Area calculation (not all shapes implement this)
    
    /**
     * @brief Calculate area enclosed by the shape
     * @return Area in square units (current coordinate system)
     * 
     * Virtual method with default implementation returning 0.0.
     * 
     * Concrete shapes should override to provide actual area calculation:
     * - Ellipse: pi * semiMajor * semiMinor
     * - Rectangle: width * height
     * - Polygon: shoelace formula
     * 
     * @code{.cpp}
     * Ellipse circle(10.0, 10.0, 0.0, 0.0);  // Radius 10
     * double area = circle.area();
     * assert(std::abs(area - M_PI * 100.0) < 0.01);  // pi*r^2
     * 
     * Rectangle square(20.0, 20.0, 0.0, 0.0);
     * assert(square.area() == 400.0);  // 20*20
     * @endcode
     * 
     * @note Default implementation returns 0.0
     * @note Units are square of current coordinate units
     * @note For normalized shapes, area is in normalized units^2
     * @see perimeter()
     */
    virtual double area() const {
        return 0.0;  // Default: not implemented
    }
    
    // Shape type identification (for debugging/serialization)
    
    /**
     * @brief Get human-readable type name
     * @return String identifier (e.g., "Ellipse", "Rectangle", "Polygon")
     * 
     * Pure virtual method - must be implemented by all concrete shapes.
     * 
     * Returns a constant string identifying the shape type.
     * Used for debugging, logging, and serialization.
     * 
     * @code{.cpp}
     * std::vector<std::unique_ptr<Shape>> shapes;
     * shapes.push_back(std::make_unique<Ellipse>(...));
     * shapes.push_back(std::make_unique<Rectangle>(...));
     * shapes.push_back(std::make_unique<Polygon>(...));
     * 
     * for (const auto& shape : shapes) {
     *     std::cout << "Shape type: " << shape->typeName() << "\n";
     *     // Output:
     *     // Shape type: Ellipse
     *     // Shape type: Rectangle
     *     // Shape type: Polygon
     * }
     * @endcode
     * 
     * @return Pointer to constant string. String is static - don't free!
     * 
     * @note Implementation should return a static string literal
     * @see clone() - preserves type via virtual dispatch
     */
    virtual const char* typeName() const = 0;

protected:
    TypeLimits typeLimits_{TypeLimits::EXTERNAL};           ///< Visibility behavior (EXTERNAL/INTERNAL/APERTURE)
    CoordinateSystem spatialSystem_{CoordinateSystem::screen()};  ///< Spatial coordinate system (SCREEN/MATH)
    NormalizationState normState_{NormalizationState::MEASURING};  ///< Normalization state (MEASURING/NORMALIZED)
};

} // namespace aperture
