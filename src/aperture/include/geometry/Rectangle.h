/**
 * @file Rectangle.h
 * @brief Rectangular shape with rotation support
 * 
 * ## Overview
 * 
 * Rectangle provides axis-aligned and rotated rectangular shapes with:
 * - Point containment testing for rotated rectangles
 * - Bounding box calculation (tight-fitting for rotated shapes)
 * - Contour generation along perimeter
 * - Geometric properties (area, perimeter)
 * - Corner point extraction
 * - Full coordinate transformation support
 * 
 * ## Key Features
 * 
 * ### 1. Rotation Support
 * 
 * Rectangles can be rotated by any angle, with efficient point-in-rectangle
 * testing using coordinate transformation:
 * 
 * @code{.cpp}
 * #include <aperturecore/geometry/Rectangle.h>
 * 
 * using namespace aperture;
 * 
 * // Axis-aligned rectangle
 * Rectangle axisAligned(100.0, 50.0, 0.0, 0.0);  // 100x50 at origin
 * 
 * // Rotated rectangle (45 degrees)
 * Rectangle rotated(100.0, 50.0, 0.0, 0.0, 45.0);
 * 
 * Point testPoint{25.0, 25.0};
 * bool inside = rotated.isInside(testPoint);  // Handles rotation automatically
 * @endcode
 * 
 * ### 2. Efficient Point Containment
 * 
 * Point-in-rectangle test uses coordinate transformation to local frame:
 * 1. Translate point relative to rectangle center
 * 2. Rotate by -rotation angle (align with axes)
 * 3. Check if |x| <= width/2 and |y| <= height/2
 * 
 * This is O(1) and works for any rotation angle.
 * 
 * ### 3. Corner Points
 * 
 * Extract corner points in consistent order (Top-Left, Top-Right, Bottom-Right, Bottom-Left):
 * 
 * @code{.cpp}
 * Rectangle rect(100.0, 50.0, 0.0, 0.0, 30.0);
 * auto corners = rect.corners();
 * 
 * // corners[0] = Top-Left
 * // corners[1] = Top-Right
 * // corners[2] = Bottom-Right
 * // corners[3] = Bottom-Left
 * 
 * // All corners in world coordinates (rotation applied)
 * for (const auto& corner : corners) {
 *     std::cout << "(" << corner.x << ", " << corner.y << ")\n";
 * }
 * @endcode
 * 
 * ### 4. Coordinate System Awareness
 * 
 * Rectangles track both spatial coordinate system (SCREEN vs MATH) and
 * normalization state (MEASURING vs NORMALIZED):
 * 
 * @code{.cpp}
 * // Screen coordinates (Y+ downward)
 * Rectangle screen(100, 50, 200, 150, 0.0, 
 *                 TypeLimits::EXTERNAL,
 *                 CoordinateSystem::screen());
 * 
 * // Math coordinates (Y+ upward)
 * Rectangle math(100, 50, 200, 150, 0.0,
 *               TypeLimits::EXTERNAL,
 *               CoordinateSystem::math());
 * 
 * // Convert between systems
 * screen.transformToSystem(CoordinateSystem::math(768.0));
 * @endcode
 * 
 * ### 5. Normalization Support
 * 
 * Scale and translate rectangles for wavefront analysis:
 * 
 * @code{.cpp}
 * Rectangle rect(100.0, 50.0, 200.0, 150.0);
 * 
 * // Normalize to unit coordinate system
 * rect.normalize(200.0, 150.0, 50.0);  // Center, radius
 * // Now: width=2.0, height=1.0, center=(0,0)
 * 
 * // Do calculations...
 * 
 * // Denormalize back
 * rect.denormalize(200.0, 150.0, 50.0);
 * // Restored: width=100, height=50, center=(200,150)
 * @endcode
 * 
 * ## Design Considerations
 * 
 * ### Width/Height vs Semi-axes
 * 
 * Unlike Ellipse (which uses semi-major/minor axes), Rectangle uses
 * full width and height for more intuitive construction:
 * 
 * ```
 * Ellipse:  radiusX=50, radiusY=25  (from center to edge)
 * Rectangle: width=100, height=50   (full dimensions)
 * ```
 * 
 * ### Rotation Direction
 * 
 * Rotation is counter-clockwise in MATH coordinates, clockwise in SCREEN
 * coordinates (due to inverted Y-axis).
 * 
 * ### Local vs World Coordinates
 * 
 * - **Local**: Rectangle-aligned frame (no rotation, center at origin)
 * - **World**: Global frame (with rotation and translation applied)
 * 
 * Internal operations use local coordinates for efficiency, then transform
 * to world coordinates for output.
 * 
 * ## Performance
 * 
 * - **isInside**: O(1) - simple coordinate transform + comparison
 * - **getBounds**: O(1) - calculate extrema analytically
 * - **getContour**: O(perimeter/stepSize) - generate edge points
 * - **corners**: O(1) - transform 4 points
 * 
 * ## Usage Examples
 * 
 * ### Creating Rectangles
 * 
 * @code{.cpp}
 * // Axis-aligned square
 * Rectangle square(50.0, 50.0, 0.0, 0.0);
 * assert(square.isSquare());
 * 
 * // Axis-aligned rectangle
 * Rectangle rect(100.0, 60.0, 50.0, 30.0);
 * assert(!rect.isSquare());
 * 
 * // Rotated rectangle
 * Rectangle rotated(80.0, 40.0, 100.0, 100.0, 45.0);
 * assert(rotated.rotationDegrees() == 45.0);
 * @endcode
 * 
 * ### Point Containment
 * 
 * @code{.cpp}
 * Rectangle rect(100.0, 50.0, 0.0, 0.0, 30.0);  // 100x50, rotated 30°
 * 
 * // Test various points
 * assert(rect.isInside({0.0, 0.0}));      // Center - always inside
 * assert(rect.isInside({25.0, 10.0}));    // Inside
 * assert(!rect.isInside({100.0, 100.0})); // Far outside
 * 
 * // Bounding box test (quick rejection)
 * Bounds bounds = rect.getBounds();
 * if (!bounds.contains(testPoint)) {
 *     // Definitely outside - skip expensive test
 * } else {
 *     // Might be inside - do full test
 *     bool inside = rect.isInside(testPoint);
 * }
 * @endcode
 * 
 * ### Working with Corners
 * 
 * @code{.cpp}
 * Rectangle rect(100.0, 50.0, 0.0, 0.0);
 * auto corners = rect.corners();
 * 
 * // Corners are in clockwise order: TL, TR, BR, BL
 * Point topLeft = corners[0];      // (-50, -25)
 * Point topRight = corners[1];     // ( 50, -25)
 * Point bottomRight = corners[2];  // ( 50,  25)
 * Point bottomLeft = corners[3];   // (-50,  25)
 * 
 * // For rotated rectangle, corners are transformed
 * Rectangle rotated(100.0, 50.0, 0.0, 0.0, 45.0);
 * auto rotatedCorners = rotated.corners();
 * // Each corner has rotation applied
 * @endcode
 * 
 * ### Visibility in Optical Systems
 * 
 * @code{.cpp}
 * // Aperture (baffle opening)
 * Rectangle aperture(20.0, 10.0, 0.0, 0.0);
 * aperture.setTypeLimits(TypeLimits::EXTERNAL);
 * 
 * // Obstruction (sensor dead area)
 * Rectangle obstruction(5.0, 5.0, 0.0, 0.0);
 * obstruction.setTypeLimits(TypeLimits::INTERNAL);
 * 
 * // Test ray visibility
 * Point rayPoint{3.0, 2.0};
 * bool visibleThroughAperture = aperture.isInside(rayPoint);
 * bool blockedByObstruction = obstruction.isInside(rayPoint);
 * @endcode
 * 
 * ## Mathematical Background
 * 
 * ### Point-in-Rectangle Test
 * 
 * For a point P in world coordinates, rectangle with center C, rotation θ:
 * 
 * 1. Translate: P' = P - C
 * 2. Rotate: P_local = Rotate(P', -θ)
 * 3. Test: |P_local.x| <= width/2 AND |P_local.y| <= height/2
 * 
 * Rotation matrix (counter-clockwise):
 * ```
 * [ cos(θ)  sin(θ) ]
 * [-sin(θ)  cos(θ) ]
 * ```
 * 
 * ### Bounding Box for Rotated Rectangle
 * 
 * For rotated rectangle, the axis-aligned bounding box is found by:
 * 1. Transform all 4 corners to world coordinates
 * 2. Find min/max X and Y values
 * 
 * Alternatively (faster):
 * - deltaX = |width/2 * cos(θ)| + |height/2 * sin(θ)|
 * - deltaY = |width/2 * sin(θ)| + |height/2 * cos(θ)|
 * - bounds = [cx-deltaX, cy-deltaY, cx+deltaX, cy+deltaY]
 * 
 * ## Thread Safety
 * 
 * **Not thread-safe!** Rectangle methods are not synchronized.
 * - Read-only operations safe when rectangle is immutable
 * - Modifications require external synchronization
 * 
 * @see Ellipse - Similar rotation support for ellipses
 * @see Polygon - Arbitrary polygonal shapes
 * @see Shape - Base class interface
 * @see TypeLimits - Visibility behavior
 * @see CoordinateSystem - SCREEN vs MATH coordinates
 */
#pragma once

#include "Shape.h"
#include <array>

namespace aperture {

/**
 * @class Rectangle
 * @brief Axis-aligned or rotated rectangular shape
 * 
 * Represents a rectangle defined by width, height, center position, and
 * optional rotation angle. Provides efficient point-in-rectangle testing,
 * corner extraction, and all standard Shape operations.
 * 
 * ## Key Properties
 * 
 * - **Width/Height**: Full dimensions (not half-widths like ellipse radii)
 * - **Rotation**: Counter-clockwise in degrees (from positive X-axis)
 * - **Center**: Position in world coordinates
 * - **Corners**: Always accessible in consistent order (TL, TR, BR, BL)
 * 
 * ## Coordinate Systems
 * 
 * Rectangle supports both local and world coordinate frames:
 * 
 * **Local Coordinates** (rectangle frame):
 * - Origin at rectangle center
 * - X-axis along width
 * - Y-axis along height
 * - No rotation
 * 
 * **World Coordinates** (global frame):
 * - Rectangle positioned at center_
 * - Rotated by rotationDeg_
 * - Used for all external operations
 * 
 * ## Memory Layout
 * 
 * ```
 * sizeof(Rectangle) = 64 bytes:
 *   - width, height: 16 bytes (2 doubles)
 *   - center: 16 bytes (Point = 2 doubles)
 *   - rotationDeg, rotationRad: 16 bytes (2 doubles)
 *   - cosRot, sinRot: 16 bytes (2 doubles, cached)
 *   + Shape base: variable
 * ```
 * 
 * ## Usage Patterns
 * 
 * ### Creating Rectangles
 * 
 * @code{.cpp}
 * // Simple square
 * Rectangle square(100.0, 100.0, 0.0, 0.0);
 * 
 * // Axis-aligned rectangle
 * Rectangle rect(200.0, 100.0, 50.0, 50.0);
 * 
 * // Rotated rectangle
 * Rectangle rotated(150.0, 75.0, 100.0, 100.0, 30.0);
 * 
 * // With visibility type
 * Rectangle aperture(50.0, 50.0, 0.0, 0.0, 0.0, TypeLimits::EXTERNAL);
 * @endcode
 * 
 * ### Common Operations
 * 
 * @code{.cpp}
 * Rectangle rect(100.0, 50.0, 0.0, 0.0, 45.0);
 * 
 * // Geometric properties
 * double a = rect.area();        // 5000.0
 * double p = rect.perimeter();   // 300.0
 * bool sq = rect.isSquare();     // false
 * 
 * // Get corners
 * auto corners = rect.corners(); // 4 points
 * 
 * // Test containment
 * bool inside = rect.isInside({10, 10});
 * 
 * // Get bounding box
 * Bounds bounds = rect.getBounds();
 * @endcode
 * 
 * ### Transformations
 * 
 * @code{.cpp}
 * Rectangle rect(100, 50, 200, 150);
 * 
 * // Move
 * rect.shiftX(50);  // Move right
 * rect.shiftY(25);  // Move down (SCREEN) or up (MATH)
 * 
 * // Normalize
 * rect.normalize(200, 150, 50);
 * // width=2, height=1, center=(0,0)
 * 
 * // Flip Y-axis
 * rect.inverseY(400);  // Mirror across y=400
 * @endcode
 * 
 * ## Comparison to XYRect
 * 
 * Rectangle replaces legacy XYRect with:
 * - Modern C++ (no MFC dependencies)
 * - Clearer API (width/height instead of left/right/top/bottom)
 * - Rotation support built-in
 * - Full coordinate system tracking
 * - Better documentation
 * 
 * ### Migration Example
 * 
 * **Before (XYRect):**
 * ```cpp
 * XYRect rect;
 * rect.XLeft = 0;
 * rect.XRight = 100;
 * rect.YTop = 0;
 * rect.YBottom = 50;
 * ```
 * 
 * **After (Rectangle):**
 * ```cpp
 * double width = 100;
 * double height = 50;
 * double centerX = (0 + 100) / 2;  // 50
 * double centerY = (0 + 50) / 2;   // 25
 * Rectangle rect(width, height, centerX, centerY);
 * ```
 * 
 * @note Replaces XYRect from InterfSolver
 * @see Ellipse, Polygon - Other Shape implementations
 * @see Bounds - Axis-aligned bounding box
 * @see Point - 2D point representation
 */
class Rectangle : public Shape {
public:
    /**
     * @brief Construct rectangle from dimensions and position
     * @param width Width of rectangle (full dimension, not half-width)
     * @param height Height of rectangle (full dimension, not half-height)
     * @param centerX Center X coordinate in world frame
     * @param centerY Center Y coordinate in world frame
     * @param rotationDegrees Rotation angle in degrees (counter-clockwise from +X axis)
     * @param typeLimits Visibility behavior (default: EXTERNAL)
     * @param spatialSystem Spatial coordinate system (default: SCREEN)
     * @param normState Normalization state (default: MEASURING)
     * 
     * Creates a rectangle with specified dimensions centered at (centerX, centerY).
     * Optional rotation is applied counter-clockwise (in MATH coordinates) or
     * clockwise (in SCREEN coordinates due to inverted Y-axis).
     * 
     * ## Examples
     * 
     * @code{.cpp}
     * // Axis-aligned square at origin
     * Rectangle square(100.0, 100.0, 0.0, 0.0);
     * 
     * // Axis-aligned rectangle offset
     * Rectangle rect(200.0, 100.0, 50.0, 30.0);
     * 
     * // Rotated rectangle (45 degrees)
     * Rectangle rotated(100.0, 50.0, 0.0, 0.0, 45.0);
     * 
     * // With full parameters
     * Rectangle aperture(80.0, 40.0, 100.0, 100.0, 30.0,
     *                   TypeLimits::EXTERNAL,
     *                   CoordinateSystem::screen());
     * @endcode
     * 
     * @note Width and height must be positive (not enforced, but required for valid geometry)
     * @note Rotation of 0° means width is along +X axis, height along +Y axis
     * @see Ellipse - Similar constructor but uses semi-axes (radius from center)
     */
    Rectangle(double width, double height,
              double centerX, double centerY,
              double rotationDegrees = 0.0,
              TypeLimits typeLimits = TypeLimits::EXTERNAL,
              CoordinateSystem spatialSystem = CoordinateSystem::screen(),
              NormalizationState normState = NormalizationState::MEASURING);
    
    /**
     * @brief Construct rectangle from 3 points on perimeter
     * @param p0 First corner point
     * @param p1 Second corner point (defines width axis)
     * @param p2 Third point (defines height and orientation)
     * @param typeLimits Visibility behavior (default: EXTERNAL)
     * @param spatialSystem Spatial coordinate system (default: SCREEN)
     * @param normState Normalization state (default: MEASURING)
     * 
     * Creates a rectangle from three perimeter points where:
     * - Vector p0→p1 defines the width direction and magnitude
     * - Point p2 is projected onto the perpendicular to define height
     * - Center is computed as the geometric center of the resulting rectangle
     * - Rotation is computed from the width vector angle
     * 
     * ## Geometry
     * 
     * Given points p0, p1, p2:
     * 1. Width vector: v_width = p1 - p0
     * 2. Width magnitude: width = ||v_width||
     * 3. Perpendicular vector: v_perp = rotate_90(v_width)
     * 4. Height projection: height = |dot(p2 - p0, normalize(v_perp))|
     * 5. Center: center = p0 + 0.5 * v_width + 0.5 * height * normalize(v_perp)
     * 6. Rotation: atan2(v_width.y, v_width.x)
     * 
     * ## Examples
     * 
     * @code{.cpp}
     * // Axis-aligned rectangle from corners
     * Point p0{0, 0};
     * Point p1{100, 0};   // Width = 100 along X
     * Point p2{100, 50};  // Height = 50 along Y
     * Rectangle rect(p0, p1, p2);
     * // Result: width=100, height=50, center={50, 25}, rotation=0°
     * 
     * // Rotated rectangle (45 degrees)
     * Point p0{0, 0};
     * Point p1{70.7, 70.7};   // Width ~100 at 45°
     * Point p2{0, 100};       // Height ~50
     * Rectangle rotated(p0, p1, p2);
     * // Result: rotated 45° counter-clockwise
     * 
     * // Usage in shape creation workflow
     * std::vector<Point> clicks;
     * clicks.push_back(firstClick);
     * clicks.push_back(secondClick);
     * clicks.push_back(thirdClick);
     * auto shape = std::make_unique<Rectangle>(
     *     clicks[0], clicks[1], clicks[2],
     *     TypeLimits::EXTERNAL
     * );
     * @endcode
     * 
     * @note Points should not be collinear (results in degenerate rectangle)
     * @note The resulting rectangle's corners may not exactly match p0, p1, p2
     *       (p2 is projected onto perpendicular)
     * @see Rectangle(width, height, ...) - Standard constructor
     */
    explicit Rectangle(
        const Point& p0,
        const Point& p1,
        const Point& p2,
        TypeLimits typeLimits = TypeLimits::EXTERNAL,
        CoordinateSystem spatialSystem = CoordinateSystem::screen(),
        NormalizationState normState = NormalizationState::MEASURING
    );
    
    // Shape interface implementation
    
    /**
     * @brief Test if point is inside rectangle
     * @param point Point to test in world coordinates
     * @return true if point is inside or on rectangle boundary
     * 
     * Implements Shape::isInside() using coordinate transformation:
     * 1. Transform point to rectangle-local coordinates
     * 2. Check if |x_local| <= width/2 AND |y_local| <= height/2
     * 
     * This works efficiently for any rotation angle (O(1) complexity).
     * 
     * @code{.cpp}
     * Rectangle rect(100.0, 50.0, 0.0, 0.0, 30.0);  // Rotated 30°
     * 
     * assert(rect.isInside({0.0, 0.0}));      // Center - always inside
     * assert(rect.isInside({25.0, 10.0}));    // Inside (if within bounds)
     * assert(!rect.isInside({100.0, 100.0})); // Outside
     * 
     * // Boundary points are inclusive
     * Point corner = rect.corners()[0];
     * assert(rect.isInside(corner));  // Corner is inside
     * @endcode
     * 
     * @note Boundary points return true (inclusive containment)
     * @note Handles rotation automatically via coordinate transform
     * @see toLocalCoordinates() - Internal coordinate transformation
     */
    bool isInside(const Point& point) const override;
    
    /**
     * @brief Get axis-aligned bounding box
     * @return Smallest axis-aligned Bounds containing entire rectangle
     * 
     * For rotated rectangles, computes tight-fitting axis-aligned bounding box
     * by calculating extrema of all 4 corners.
     * 
     * For axis-aligned rectangles (rotation = 0), returns exact rectangle bounds.
     * 
     * @code{.cpp}
     * // Axis-aligned rectangle
     * Rectangle aligned(100.0, 50.0, 0.0, 0.0);
     * Bounds b1 = aligned.getBounds();
     * // b1 = [-50, -25, 50, 25] (exact fit)
     * 
     * // Rotated rectangle (45 degrees)
     * Rectangle rotated(100.0, 50.0, 0.0, 0.0, 45.0);
     * Bounds b2 = rotated.getBounds();
     * // b2 is larger (contains rotated rectangle)
     * assert(b2.width() > 100.0);  // Diagonal extent
     * @endcode
     * 
     * @note Returned bounds are axis-aligned (no rotation)
     * @note For rotated rectangles, bounding box is larger than rectangle
     * @see corners() - Get actual rectangle corners (not bounding box)
     */
    Bounds getBounds() const override;
    
    /**
     * @brief Get contour points along rectangle perimeter
     * @param stepSize Maximum distance between consecutive points
     * @return Vector of points forming rectangle boundary
     * 
     * Generates points along all 4 edges of the rectangle, spacing them
     * approximately stepSize apart. Points are in world coordinates (rotation applied).
     * 
     * The contour forms a closed loop (first and last points are close/identical).
     * 
     * @code{.cpp}
     * Rectangle rect(100.0, 50.0, 0.0, 0.0);
     * 
     * // Generate contour with ~10 unit spacing
     * auto contour = rect.getContour(10.0);
     * // ~30 points (perimeter=300, spacing=10)
     * 
     * // Fine contour
     * auto fine = rect.getContour(1.0);
     * // ~300 points
     * 
     * // Verify closure
     * assert(contour.front().isNear(contour.back(), stepSize * 2));
     * @endcode
     * 
     * @param stepSize Target spacing between points (actual spacing may vary)
     * @return Points along perimeter in clockwise order, starting from top-left
     * 
     * @note Point count = perimeter / stepSize (approximately)
     * @note Returned points are in world coordinates (rotation applied)
     * @note Smaller stepSize = more points = higher accuracy
     * @see corners() - Get just the 4 corner points
     * @see perimeter() - Get total perimeter length
     */
    std::vector<Point> getContour(double stepSize) const override;
    
    /**
     * @brief Test if point is on the rectangle contour (boundary)
     * @param point Point to test in world coordinates
     * @param tolerance Distance tolerance in current units
     * @return true if point is within tolerance of the rectangle boundary
     * 
     * Tests if a point lies on or near the rectangle boundary by checking
     * if the point is inside the enlarged rectangle (sides + tolerance) but
     * not inside the diminished rectangle (sides - tolerance).
     * 
     * @code{.cpp}
     * Rectangle rect(100.0, 50.0, 0.0, 0.0, 45.0);  // Rotated
     * 
     * // Point exactly on edge
     * Point onEdge = rect.corners()[0];  // Get corner
     * assert(rect.isOnContour(onEdge, 0.1));
     * 
     * // Point near edge
     * Point nearEdge{51.0, 0.0};  // 1 unit outside (if not rotated)
     * assert(rect.isOnContour(nearEdge, 2.0));
     * 
     * // Point far from edge
     * Point farAway{200.0, 200.0};
     * assert(!rect.isOnContour(farAway, 2.0));
     * 
     * // Works with rotation
     * assert(!rect.isOnContour(rect.center(), tolerance));  // Center not on edge
     * @endcode
     * 
     * @param point Point to test (in same coordinate system as rectangle)
     * @param tolerance Maximum distance from boundary (must be positive)
     * @return true if distance to boundary <= tolerance
     * 
     * @note Efficient O(1) implementation using coordinate transformation
     * @note Handles rotated rectangles correctly
     * @see isInside() - Interior containment test
     */
    bool isOnContour(const Point& point, double tolerance) const override;
    
    /**
     * @brief Calculate perimeter (boundary length)
     * @return Total length of rectangle boundary
     * 
     * Perimeter = 2 * (width + height)
     * 
     * This is independent of rotation (perimeter doesn't change when rotated).
     * 
     * @code{.cpp}
     * Rectangle rect(100.0, 50.0, 0.0, 0.0);
     * assert(rect.perimeter() == 300.0);  // 2*(100+50)
     * 
     * // Rotation doesn't change perimeter
     * Rectangle rotated(100.0, 50.0, 0.0, 0.0, 45.0);
     * assert(rotated.perimeter() == 300.0);  // Same!
     * @endcode
     * 
     * @note Rotation-invariant (same for any rotation angle)
     * @see area() - Calculate enclosed area
     */
    double perimeter() const override;
    
    /**
     * @brief Calculate area (enclosed region)
     * @return Area in square units
     * 
     * Area = width * height
     * 
     * This is independent of rotation (area doesn't change when rotated).
     * 
     * @code{.cpp}
     * Rectangle rect(100.0, 50.0, 0.0, 0.0);
     * assert(rect.area() == 5000.0);  // 100*50
     * 
     * // Rotation doesn't change area
     * Rectangle rotated(100.0, 50.0, 0.0, 0.0, 45.0);
     * assert(rotated.area() == 5000.0);  // Same!
     * @endcode
     * 
     * @note Rotation-invariant (same for any rotation angle)
     * @see perimeter() - Calculate boundary length
     */
    double area() const override;
    
    /**
     * @brief Create deep copy of rectangle
     * @return Unique pointer to cloned rectangle
     * 
     * Creates an independent copy including all geometry and state:
     * - Dimensions (width, height)
     * - Position (center)
     * - Rotation angle
     * - TypeLimits
     * - Coordinate system
     * - Normalization state
     * 
     * @code{.cpp}
     * Rectangle original(100, 50, 0, 0, 30);
     * original.setTypeLimits(TypeLimits::EXTERNAL);
     * 
     * std::unique_ptr<Shape> clone = original.clone();
     * 
     * // Clone is independent
     * clone->shiftX(100);  // Move clone
     * // original unchanged
     * @endcode
     * 
     * @return Unique pointer to new Rectangle instance
     * @see Shape::clone() - Base class interface
     */
    std::unique_ptr<Shape> clone() const override;
    
    /**
     * @brief Get type name for debugging/serialization
     * @return "Rectangle"
     * 
     * Returns constant string identifier for this shape type.
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 0, 0);
     * assert(std::string(rect.typeName()) == "Rectangle");
     * @endcode
     */
    const char* typeName() const override { return "Rectangle"; }
    
    // Rectangle-specific properties
    
    /**
     * @brief Get center point in world coordinates
     * @return Center point
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 75, 25);
     * Point c = rect.center();  // {75, 25}
     * @endcode
     */
    Point center() const { return center_; }
    
    /**
     * @brief Get width (full dimension along local X-axis)
     * @return Width in current units
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 0, 0);
     * assert(rect.width() == 100.0);
     * @endcode
     * 
     * @note This is the FULL width, not half-width
     * @see height() - Get height dimension
     */
    double width() const { return width_; }
    
    /**
     * @brief Get height (full dimension along local Y-axis)
     * @return Height in current units
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 0, 0);
     * assert(rect.height() == 50.0);
     * @endcode
     * 
     * @note This is the FULL height, not half-height
     * @see width() - Get width dimension
     */
    double height() const { return height_; }
    
    /**
     * @brief Get rotation angle in degrees
     * @return Rotation angle (counter-clockwise from +X axis)
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 0, 0, 30.0);
     * assert(rect.rotationDegrees() == 30.0);
     * @endcode
     * 
     * @see rotationRadians() - Get rotation in radians
     */
    double rotationDegrees() const { return rotationDeg_; }
    
    /**
     * @brief Get rotation angle in radians
     * @return Rotation angle (counter-clockwise from +X axis)
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 0, 0, 45.0);
     * double rad = rect.rotationRadians();
     * assert(std::abs(rad - M_PI/4) < 1e-6);
     * @endcode
     * 
     * @see rotationDegrees() - Get rotation in degrees
     */
    double rotationRadians() const { return rotationRad_; }
    
    /**
     * @brief Check if rectangle is actually a square
     * @param tolerance Tolerance for width==height comparison
     * @return true if width and height are within tolerance
     * 
     * @code{.cpp}
     * Rectangle square(100, 100, 0, 0);
     * assert(square.isSquare());
     * 
     * Rectangle rect(100, 99.999, 0, 0);
     * assert(rect.isSquare(0.01));  // Within tolerance
     * 
     * Rectangle notSquare(100, 50, 0, 0);
     * assert(!notSquare.isSquare());
     * @endcode
     * 
     * @param tolerance Maximum difference between width and height
     */
    bool isSquare(double tolerance = 1e-6) const {
        return std::abs(width_ - height_) < tolerance;
    }
    
    /**
     * @brief Get corner points in consistent order
     * @return Array of 4 corner points: [TL, TR, BR, BL]
     * 
     * Returns corner points in clockwise order (when viewed in SCREEN coordinates):
     * - [0] Top-Left
     * - [1] Top-Right
     * - [2] Bottom-Right
     * - [3] Bottom-Left
     * 
     * Points are in world coordinates (rotation and translation applied).
     * 
     * @code{.cpp}
     * // Axis-aligned rectangle
     * Rectangle rect(100, 50, 0, 0);
     * auto corners = rect.corners();
     * 
     * // corners[0] = {-50, -25}  (top-left)
     * // corners[1] = { 50, -25}  (top-right)
     * // corners[2] = { 50,  25}  (bottom-right)
     * // corners[3] = {-50,  25}  (bottom-left)
     * 
     * // Rotated rectangle
     * Rectangle rotated(100, 50, 0, 0, 45);
     * auto rotatedCorners = rotated.corners();
     * // Each corner transformed by rotation
     * @endcode
     * 
     * @return std::array of 4 Points in clockwise order
     * @note Corners are in world coordinates (rotation applied)
     * @see getBounds() - Get axis-aligned bounding box instead
     */
    std::array<Point, 4> corners() const;
    
    // ========================================================================
    // Handle Enumeration (Interactive Editing - UX spec §3)
    // ========================================================================
    
    /**
     * @brief Enumerate interactive handles for rectangle editing
     * @param out Output vector to receive handle descriptors
     * 
     * Rectangle provides handles for:
     * - Move (§2.1, §3.2): centroid
     * - Rotate (§2.2, §3.3): offset from shape along local +Y axis
     * - CornerResize (§3.1): 4 corners - resize in 2 axes
     * - EdgeResize (§3.1): 4 edge midpoints - resize in 1 axis
     * 
     * Total: 10 handles (1 move + 1 rotate + 4 corners + 4 edges)
     * 
     * @param out Vector to append handle descriptors to
     * 
     * @see Shape::EnumerateHandles()
     * @see shapes_handles.md §3 - Rectangle handles specification
     */
    void EnumerateHandles(std::vector<HandleDesc>& out) const override;
    
    /**
     * @brief Apply handle drag to update rectangle geometry
     * @param handle Handle being dragged
     * @param drag Drag context with world-space positions and modifiers
     * 
     * Supported handle types:
     * - Move: Translate rectangle by drag.deltaWorld
     * - Rotate: Update rotation angle based on drag position
     * - CornerResize: Resize from opposite corner (Shift=preserve aspect, Alt=from center)
     * - EdgeResize: Resize perpendicular to edge (Alt=from center)
     * 
     * @param handle Frozen handle descriptor from drag start
     * @param drag Drag context with world-space delta and modifier keys
     * 
     * @see Shape::ApplyHandleDrag()
     * @see shapes_handles.md §3.1 - Rectangle resize modifiers
     */
    void ApplyHandleDrag(const HandleDesc& handle, const DragContext& drag) override;
    
    // Coordinate transformation interface implementation

    /**
     * @brief Normalize coordinates to unit system
     * @param originX Origin X coordinate in measuring system
     * @param originY Origin Y coordinate in measuring system
     * @param radius Normalization scale factor
     * 
     * Transforms from measuring to normalized coordinates:
     * - width_norm = width_meas / radius
     * - height_norm = height_meas / radius
     * - center_norm = (center_meas - origin) / radius
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 200, 150);
     * rect.normalize(200, 150, 50);
     * 
     * // Now: width=2, height=1, center=(0,0)
     * assert(rect.isNormalized());
     * @endcode
     * 
     * @see denormalize() - Inverse operation
     */
    void normalize(double originX, double originY, double radius) override;
    
    /**
     * @brief Denormalize coordinates back to measuring system
     * @param originX Origin X coordinate in measuring system
     * @param originY Origin Y coordinate in measuring system
     * @param radius Normalization scale factor
     * 
     * Inverse of normalize(). Transforms from normalized to measuring:
     * - width_meas = width_norm * radius
     * - height_meas = height_norm * radius
     * - center_meas = center_norm * radius + origin
     * 
     * @code{.cpp}
     * Rectangle rect(2, 1, 0, 0);  // Normalized
     * rect.denormalize(200, 150, 50);
     * 
     * // Now: width=100, height=50, center=(200,150)
     * assert(rect.isMeasuring());
     * @endcode
     * 
     * @warning Must use SAME parameters as normalize() call!
     * @see normalize() - Forward operation
     */
    void denormalize(double originX, double originY, double radius) override;
    
    /**
     * @brief Invert Y coordinate (flip across horizontal line)
     * @param centerY Y coordinate of inversion axis
     * 
     * Mirrors rectangle across horizontal line y=centerY.
     * Also inverts rotation angle (positive becomes negative).
     * 
     * Used for SCREEN ↔ MATH coordinate system conversion.
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 100, 100, 30);
     * rect.inverseY(400);
     * 
     * // Center Y: 100 -> 400-100 = 300
     * // Rotation: 30° -> -30°
     * assert(rect.center().y == 300);
     * assert(rect.rotationDegrees() == -30);
     * @endcode
     * 
     * @see transformToSystem() - Higher-level system conversion
     */
    void inverseY(double centerY) override;
    
    /**
     * @brief Shift rectangle in X direction
     * @param deltaX Amount to shift (positive=right, negative=left)
     * 
     * Translates rectangle horizontally without changing size or rotation.
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 0, 0);
     * rect.shiftX(50);
     * 
     * assert(rect.center().x == 50);
     * @endcode
     * 
     * @see shiftY() - Vertical shift
     */
    void shiftX(double deltaX) override;
    
    /**
     * @brief Shift rectangle in Y direction
     * @param deltaY Amount to shift (direction depends on coordinate system)
     * 
     * Translates rectangle vertically without changing size or rotation.
     * 
     * Direction:
     * - SCREEN: positive = down
     * - MATH: positive = up
     * 
     * @code{.cpp}
     * Rectangle rect(100, 50, 0, 0);
     * rect.shiftY(25);
     * 
     * assert(rect.center().y == 25);
     * @endcode
     * 
     * @note Direction depends on coordinate system!
     * @see shiftX() - Horizontal shift
     */
    void shiftY(double deltaY) override;

private:
    double width_;          ///< Width (local X dimension)
    double height_;         ///< Height (local Y dimension)
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
     * @brief Transform point from world to rectangle-local coordinates
     * @param point Point in world coordinates
     * @return Point in rectangle-local coordinates (centered, aligned)
     * 
     * Performs inverse transformation:
     * 1. Translate to rectangle-relative: p' = p - center
     * 2. Rotate by -rotation: p_local = Rotate(p', -θ)
     * 
     * Result is in rectangle frame (no rotation, center at origin).
     */
    Point toLocalCoordinates(const Point& point) const;
};

} // namespace aperture
