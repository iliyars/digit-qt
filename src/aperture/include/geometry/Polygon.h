/**
 * @file Polygon.h
 * @brief Arbitrary polygon shape with variable vertex count
 *
 * ## Overview
 *
 * Polygon provides arbitrary closed polygonal shapes with:
 * - Variable number of vertices (dynamic vertex list)
 * - Point-in-polygon testing using ray-casting algorithm
 * - Area calculation using shoelace formula
 * - Convexity testing
 * - Support for both convex and concave polygons
 * - Winding order awareness (clockwise vs counter-clockwise)
 *
 * ## Key Features
 *
 * ### 1. Arbitrary Vertex Count
 *
 * Polygons can have any number of vertices (minimum 3 for valid polygons):
 *
 * @code{.cpp}
 * #include <aperturecore/geometry/Polygon.h>
 *
 * using namespace aperture;
 *
 * // Triangle (3 vertices)
 * Polygon triangle({
 *     {0.0, 0.0},
 *     {10.0, 0.0},
 *     {5.0, 8.66}
 * });
 *
 * // Hexagon (6 vertices)
 * Polygon hexagon;
 * for (int i = 0; i < 6; i++) {
 *     double angle = i * 60.0 * M_PI / 180.0;
 *     hexagon.addVertex({10.0 * cos(angle), 10.0 * sin(angle)});
 * }
 *
 * // Complex shape (many vertices)
 * std::vector<Point> complexPoints = {...};  // 100+ points
 * Polygon complex(complexPoints);
 * @endcode
 *
 * ### 2. Ray-Casting Point-in-Polygon Test
 *
 * Efficient O(n) algorithm that works for both convex and concave polygons:
 *
 * **Algorithm:**
 * 1. Cast horizontal ray from test point to infinity (right)
 * 2. Count intersections with polygon edges
 * 3. Odd count = inside, even count = outside
 *
 * @code{.cpp}
 * Polygon polygon({
 *     {0, 0}, {10, 0}, {10, 10}, {0, 10}  // Square
 * });
 *
 * Point inside{5, 5};
 * Point outside{15, 15};
 * Point onEdge{10, 5};
 *
 * assert(polygon.isInside(inside));    // true - odd intersections
 * assert(!polygon.isInside(outside));  // false - even intersections
 * assert(polygon.isInside(onEdge));    // true - on boundary
 * @endcode
 *
 * ### 3. Shoelace Formula for Area
 *
 * Calculates signed area using Gauss's area formula (shoelace formula):
 *
 * **Formula:**
 * ```
 * A = 1/2 * |sum(x[i] * y[i+1] - x[i+1] * y[i])|
 * ```
 *
 * Sign indicates winding order:
 * - Positive: Counter-clockwise (CCW)
 * - Negative: Clockwise (CW)
 *
 * @code{.cpp}
 * // Counter-clockwise square (10x10)
 * Polygon ccw({{0,0}, {10,0}, {10,10}, {0,10}});
 * assert(ccw.area() == 100.0);
 *
 * // Clockwise square (same area, opposite winding)
 * Polygon cw({{0,0}, {0,10}, {10,10}, {10,0}});
 * assert(cw.area() == 100.0);  // Absolute value
 * @endcode
 *
 * ### 4. Convexity Testing
 *
 * Determines if polygon is convex by checking cross products:
 *
 * **Algorithm:**
 * - For each vertex triplet (p[i-1], p[i], p[i+1])
 * - Calculate cross product: (p[i]-p[i-1]) x (p[i+1]-p[i])
 * - All cross products must have same sign for convex polygon
 *
 * @code{.cpp}
 * // Convex polygon (triangle)
 * Polygon convex({{0,0}, {10,0}, {5,8.66}});
 * assert(convex.isConvex());
 *
 * // Concave polygon (star shape)
 * Polygon concave({
 *     {5,0}, {6,3}, {10,3}, {7,5},
 *     {8,10}, {5,7}, {2,10}, {3,5},
 *     {0,3}, {4,3}
 * });
 * assert(!concave.isConvex());
 * @endcode
 *
 * ### 5. Winding Order and Orientation
 *
 * Polygon winding order affects normal direction and visibility:
 *
 * - **Counter-Clockwise (CCW)**: Standard mathematical orientation
 * - **Clockwise (CW)**: Graphics/screen orientation
 *
 * @code{.cpp}
 * Polygon ccwSquare({{0,0}, {10,0}, {10,10}, {0,10}});
 * Polygon cwSquare({{0,0}, {0,10}, {10,10}, {10,0}});
 *
 * // Both have same area and contain same points
 * assert(ccwSquare.area() == cwSquare.area());
 * assert(ccwSquare.isInside({5,5}) == cwSquare.isInside({5,5}));
 * @endcode
 *
 * ## Design Considerations
 *
 * ### Dynamic vs Static Vertices
 *
 * Polygon uses `std::vector<Point>` for flexibility:
 * - Add/remove vertices dynamically
 * - Variable polygon complexity
 * - Trade-off: Slightly more memory overhead than fixed-size shapes
 *
 * ### Closure Handling
 *
 * Polygons are assumed to be closed (last vertex connects to first):
 * - No need to duplicate first vertex at end
 * - `ensureClosed()` adds closing vertex if needed
 * - `isClosed()` checks if first == last
 *
 * ### Degeneracy Detection
 *
 * A polygon is degenerate if:
 * - Fewer than 3 vertices
 * - Zero or near-zero area (all colinear points)
 *
 * @code{.cpp}
 * Polygon degenerate1;  // Empty
 * assert(degenerate1.isDegenerate());
 *
 * Polygon degenerate2({{0,0}, {10,0}});  // Line (2 points)
 * assert(degenerate2.isDegenerate());
 *
 * Polygon degenerate3({{0,0}, {5,0}, {10,0}});  // Collinear
 * assert(degenerate3.isDegenerate());
 * @endcode
 *
 * ## Performance
 *
 * - **isInside**: O(n) where n = vertex count (ray-casting)
 * - **area**: O(n) - shoelace formula
 * - **perimeter**: O(n) - sum of edge lengths
 * - **isConvex**: O(n) - check all cross products
 * - **getBounds**: O(n) - find min/max coordinates
 * - **getContour**: O(n) - already a contour!
 *
 * ## Coordinate Systems
 *
 * Polygon vertices can be in SCREEN or MATH coordinates:
 *
 * - **SCREEN**: Y+ downward, clockwise winding common
 * - **MATH**: Y+ upward, counter-clockwise winding common
 *
 * The winding order affects signed area and normal direction, but
 * not containment testing or absolute area.
 *
 * ## Usage Examples
 *
 * ### Creating Polygons
 *
 * @code{.cpp}
 * // From initializer list (simple shapes)
 * Polygon triangle({
 *     {0.0, 0.0},
 *     {10.0, 0.0},
 *     {5.0, 8.66}
 * });
 *
 * // From vector (complex shapes)
 * std::vector<Point> vertices;
 * for (int i = 0; i < 8; i++) {
 *     double angle = i * 45.0 * M_PI / 180.0;
 *     vertices.push_back({10 * cos(angle), 10 * sin(angle)});
 * }
 * Polygon octagon(vertices);
 *
 * // Build incrementally
 * Polygon custom;
 * custom.addVertex({0, 0});
 * custom.addVertex({10, 0});
 * custom.addVertex({10, 10});
 * custom.addVertex({0, 10});
 * @endcode
 *
 * ### Point Containment Testing
 *
 * @code{.cpp}
 * Polygon polygon({
 *     {0,0}, {10,0}, {15,5}, {10,10}, {0,10}  // Pentagon
 * });
 *
 * // Test various points
 * assert(polygon.isInside({5, 5}));     // Inside
 * assert(!polygon.isInside({20, 5}));   // Outside
 * assert(polygon.isInside({0, 5}));     // On edge
 *
 * // Quick rejection using bounding box
 * Bounds bounds = polygon.getBounds();
 * if (!bounds.contains(testPoint)) {
 *     // Definitely outside - skip expensive ray-casting
 * }
 * @endcode
 *
 * ### Working with Vertices
 *
 * @code{.cpp}
 * Polygon polygon;
 *
 * // Add vertices
 * polygon.addVertex({0, 0});
 * polygon.addVertex({10, 0});
 * polygon.addVertex({5, 8.66});
 *
 * // Query vertices
 * size_t count = polygon.vertexCount();  // 3
 * Point first = polygon.vertex(0);       // {0, 0}
 *
 * // Iterate vertices
 * for (const auto& vertex : polygon.vertices()) {
 *     std::cout << "(" << vertex.x << ", " << vertex.y << ")\n";
 * }
 *
 * // Clear
 * polygon.clear();
 * assert(polygon.vertexCount() == 0);
 * @endcode
 *
 * ### Geometric Properties
 *
 * @code{.cpp}
 * Polygon pentagon({
 *     {0,0}, {10,0}, {12,5}, {5,10}, {-2,5}
 * });
 *
 * double area = pentagon.area();
 * double perim = pentagon.perimeter();
 * Point center = pentagon.centroid();
 * bool convex = pentagon.isConvex();
 * bool degen = pentagon.isDegenerate();
 * @endcode
 *
 * ## Mathematical Background
 *
 * ### Ray-Casting Algorithm
 *
 * For point P, cast horizontal ray to right:
 * 1. For each edge (v[i], v[i+1]):
 *    - Check if ray crosses edge
 *    - Increment crossing count if yes
 * 2. Point is inside if crossing count is odd
 *
 * **Edge Crossing Test:**
 * - Edge must straddle horizontal line through P
 * - Intersection X-coordinate must be >= P.x
 *
 * ### Shoelace Formula (Gauss's Area Formula)
 *
 * For polygon with vertices (x₀,y₀), (x₁,y₁), ..., (xₙ₋₁,yₙ₋₁):
 *
 * ```
 * 2A = sum_{i=0}^{n-1} (x[i] * y[i+1] - x[i+1] * y[i])
 * ```
 *
 * Where indices wrap: x[n] = x[0], y[n] = y[0]
 *
 * Sign of A indicates winding:
 * - A > 0: Counter-clockwise
 * - A < 0: Clockwise
 *
 * ### Convexity Test (Cross Product Method)
 *
 * For consecutive vertices p[i-1], p[i], p[i+1]:
 *
 * ```
 * cross = (p[i].x - p[i-1].x) * (p[i+1].y - p[i].y) -
 *         (p[i].y - p[i-1].y) * (p[i+1].x - p[i].x)
 * ```
 *
 * Polygon is convex if all cross products have same sign.
 *
 * ## Thread Safety
 *
 * **Not thread-safe!** Polygon methods are not synchronized.
 * - Read-only operations safe when polygon is immutable
 * - addVertex(), clear() require external synchronization
 *
 * @see Ellipse - Parametric curved shapes
 * @see Rectangle - Rectangular shapes
 * @see Shape - Base class interface
 * @see Point - 2D point representation
 * @see Bounds - Axis-aligned bounding box
 */
#pragma once

#include "Shape.h"

#include <vector>

namespace aperture {

/**
 * @class Polygon
 * @brief Arbitrary polygon defined by variable number of vertices
 *
 * Represents a closed polygon with any number of vertices (minimum 3 for
 * non-degenerate polygons). Supports both convex and concave polygons using
 * ray-casting for point containment and shoelace formula for area calculation.
 *
 * ## Key Properties
 *
 * - **Vertices**: Dynamic list of 2D points (std::vector<Point>)
 * - **Closure**: Implicit (last vertex connects to first, no duplication
 * needed)
 * - **Winding**: Supports both clockwise and counter-clockwise
 * - **Topology**: Handles convex and concave (but not self-intersecting)
 *
 * ## Algorithms
 *
 * | Operation | Algorithm | Complexity | Notes |
 * |-----------|-----------|------------|-------|
 * | isInside  | Ray-casting | O(n) | Works for concave |
 * | area      | Shoelace formula | O(n) | Signed area |
 * | isConvex  | Cross products | O(n) | Checks all vertices |
 * | perimeter | Edge sum | O(n) | Simple |
 * | getBounds | Min/max scan | O(n) | Axis-aligned box |
 *
 * ## Memory Layout
 *
 * ```
 * sizeof(Polygon) = sizeof(Shape) + sizeof(std::vector<Point>)
 *                 ≈ base + 32 bytes (vector overhead)
 *                 + n * 16 bytes (n Points)
 *
 * Example for 100-vertex polygon:
 *   ≈ base + 32 + 1600 = ~1632 bytes + base
 * ```
 *
 * ## Usage Patterns
 *
 * ### Construction
 *
 * @code{.cpp}
 * // Empty (build incrementally)
 * Polygon empty;
 *
 * // From initializer list
 * Polygon triangle({
 *     {0,0}, {10,0}, {5,8.66}
 * });
 *
 * // From vector
 * std::vector<Point> pts = {...};
 * Polygon poly(pts);
 * @endcode
 *
 * ### Vertex Manipulation
 *
 * @code{.cpp}
 * Polygon poly;
 *
 * // Add vertices
 * poly.addVertex({0, 0});
 * poly.addVertex({10, 0});
 * poly.addVertex({5, 8.66});
 *
 * // Query
 * size_t n = poly.vertexCount();
 * Point v0 = poly.vertex(0);
 * auto& all = poly.vertices();
 *
 * // Clear
 * poly.clear();
 * @endcode
 *
 * ### Geometric Queries
 *
 * @code{.cpp}
 * bool inside = poly.isInside({5, 5});
 * double a = poly.area();
 * double p = poly.perimeter();
 * bool convex = poly.isConvex();
 * Point center = poly.centroid();
 * @endcode
 *
 * ## Comparison to XYPolygon
 *
 * Polygon replaces legacy XYPolygon with:
 * - Modern C++ (std::vector instead of custom arrays)
 * - Clearer vertex management (addVertex vs SetSize/operator[])
 * - Better documentation
 * - Full coordinate system support
 * - Type-safe TypeLimits enum
 *
 * ### Migration Example
 *
 * **Before (XYPolygon):**
 * ```cpp
 * XYPolygon poly;
 * poly.SetSize(3);
 * poly[0] = XYPoint(0, 0);
 * poly[1] = XYPoint(10, 0);
 * poly[2] = XYPoint(5, 8.66);
 * ```
 *
 * **After (Polygon):**
 * ```cpp
 * Polygon poly({
 *     {0, 0},
 *     {10, 0},
 *     {5, 8.66}
 * });
 * // Or:
 * Polygon poly;
 * poly.addVertex({0, 0});
 * poly.addVertex({10, 0});
 * poly.addVertex({5, 8.66});
 * ```
 *
 * @note Replaces XYPolygon from InterfSolver
 * @see Ellipse, Rectangle - Other Shape implementations
 * @see Point - Vertex representation
 * @see Bounds - Bounding box
 */
class Polygon : public Shape {
public:
  /**
   * @brief Construct empty polygon
   *
   * Creates polygon with no vertices. Vertices can be added using addVertex().
   *
   * @code{.cpp}
   * Polygon poly;
   * assert(poly.vertexCount() == 0);
   * assert(poly.isDegenerate());
   *
   * // Build polygon incrementally
   * poly.addVertex({0, 0});
   * poly.addVertex({10, 0});
   * poly.addVertex({5, 8.66});
   * assert(poly.vertexCount() == 3);
   * @endcode
   *
   * @note Empty polygon is considered degenerate
   * @see addVertex() - Add vertices
   * @see Polygon(std::vector<Point>) - Construct from vertex list
   */
  Polygon() = default;

  /**
   * @brief Construct polygon from vertex list
   * @param vertices Vector of vertices defining the polygon
   * @param typeLimits Visibility behavior (default: EXTERNAL)
   * @param spatialSystem Coordinate system (default: SCREEN)
   * @param normState Normalization state (default: MEASURING)
   *
   * Creates polygon with specified vertices. The polygon is automatically
   * closed (no need to duplicate first vertex at end).
   *
   * ## Examples
   *
   * @code{.cpp}
   * // Triangle
   * std::vector<Point> triangleVerts = {
   *     {0.0, 0.0},
   *     {10.0, 0.0},
   *     {5.0, 8.66}
   * };
   * Polygon triangle(triangleVerts);
   * assert(triangle.vertexCount() == 3);
   *
   * // Square
   * std::vector<Point> squareVerts = {
   *     {0,0}, {10,0}, {10,10}, {0,10}
   * };
   * Polygon square(squareVerts);
   *
   * // Complex polygon (many vertices)
   * std::vector<Point> complexVerts;
   * for (int i = 0; i < 100; i++) {
   *     double angle = i * 3.6 * M_PI / 180.0;
   *     complexVerts.push_back({50 * cos(angle), 50 * sin(angle)});
   * }
   * Polygon complex(complexVerts);
   * @endcode
   *
   * @param vertices Vertex list (minimum 3 for valid polygon)
   * @note Polygon is closed automatically (first and last connect)
   * @note Fewer than 3 vertices creates degenerate polygon
   * @see Polygon(std::initializer_list) - Convenience constructor
   */
  explicit Polygon(
      const std::vector<Point> &vertices,
      TypeLimits typeLimits = TypeLimits::EXTERNAL,
      CoordinateSystem spatialSystem = CoordinateSystem::screen(),
      NormalizationState normState = NormalizationState::MEASURING);

  /**
   * @brief Construct polygon from initializer list
   * @param vertices Initializer list of vertices
   * @param typeLimits Visibility behavior (default: EXTERNAL)
   * @param spatialSystem Coordinate system (default: SCREEN)
   * @param normState Normalization state (default: MEASURING)
   *
   * Convenient constructor for literal polygon definitions.
   *
   * @code{.cpp}
   * // Triangle (most concise syntax)
   * Polygon triangle({
   *     {0, 0},
   *     {10, 0},
   *     {5, 8.66}
   * });
   *
   * // Square
   * Polygon square({
   *     {0,0}, {10,0}, {10,10}, {0,10}
   * });
   *
   * // Pentagon with options
   * Polygon aperture(
   *     {{0,0}, {10,0}, {12,5}, {5,10}, {-2,5}},
   *     TypeLimits::EXTERNAL,
   *     CoordinateSystem::screen()
   * );
   * @endcode
   *
   * @note Preferred for literal polygon construction
   * @see Polygon(std::vector<Point>) - From vector
   */
  Polygon(std::initializer_list<Point> vertices,
          TypeLimits typeLimits = TypeLimits::EXTERNAL,
          CoordinateSystem spatialSystem = CoordinateSystem::screen(),
          NormalizationState normState = NormalizationState::MEASURING);

  // Shape interface implementation

  /**
   * @brief Test if point is inside polygon using ray-casting
   * @param point Point to test in world coordinates
   * @return true if point is inside or on polygon boundary
   *
   * Implements ray-casting algorithm (O(n) complexity):
   * 1. Cast horizontal ray from point to right (+X direction)
   * 2. Count intersections with polygon edges
   * 3. Odd count = inside, even count = outside
   *
   * Works correctly for both convex and concave polygons.
   *
   * @code{.cpp}
   * Polygon square({{0,0}, {10,0}, {10,10}, {0,10}});
   *
   * assert(square.isInside({5, 5}));      // Center - inside
   * assert(square.isInside({0, 5}));      // On edge - inside
   * assert(square.isInside({0, 0}));      // On vertex - inside
   * assert(!square.isInside({15, 5}));    // Outside
   *
   * // Concave polygon (star shape)
   * Polygon star({
   *     {5,0}, {6,3}, {10,3}, {7,5},
   *     {8,10}, {5,7}, {2,10}, {3,5},
   *     {0,3}, {4,3}
   * });
   * // Ray-casting handles concave correctly
   * bool inside = star.isInside({5, 5});
   * @endcode
   *
   * @note Boundary points return true (inclusive)
   * @note O(n) complexity where n = vertex count
   * @note Works for concave polygons
   * @see getBounds() - Quick rejection test
   */
  bool isInside(const Point &point) const override;

  /**
   * @brief Get axis-aligned bounding box
   * @return Smallest axis-aligned Bounds containing polygon
   *
   * Scans all vertices to find min/max X and Y coordinates.
   *
   * @code{.cpp}
   * Polygon triangle({{0,0}, {10,0}, {5,8.66}});
   * Bounds bounds = triangle.getBounds();
   *
   * assert(bounds.left() == 0.0);
   * assert(bounds.right() == 10.0);
   * assert(bounds.bottom() == 0.0);
   * assert(bounds.top() == 8.66);
   *
   * // Use for quick rejection
   * if (!bounds.contains(testPoint)) {
   *     // Definitely outside - skip expensive ray-casting
   * }
   * @endcode
   *
   * @note O(n) complexity
   * @note Returns tight axis-aligned box
   * @see isInside() - Precise containment test
   */
  Bounds getBounds() const override;

  /**
   * @brief Get contour points (returns vertices unchanged)
   * @param stepSize Ignored for Polygon (vertices already define contour)
   * @return Copy of vertices with closure
   *
   * For polygons, the vertices already form a contour, so this
   * simply returns a copy with an added closing point (first vertex
   * duplicated at end).
   *
   * @code{.cpp}
   * Polygon triangle({{0,0}, {10,0}, {5,8.66}});
   * auto contour = triangle.getContour(1.0);  // stepSize ignored
   *
   * // contour[0] = {0, 0}
   * // contour[1] = {10, 0}
   * // contour[2] = {5, 8.66}
   * // contour[3] = {0, 0}  // Closure
   *
   * assert(contour.size() == 4);  // n + 1
   * assert(contour.front() == contour.back());
   * @endcode
   *
   * @param stepSize Ignored (polygon vertices already define contour)
   * @return Vertices with closing point added
   * @note For other shapes (Ellipse, Rectangle), stepSize controls sampling
   * @see vertices() - Get vertices without closure
   */
  std::vector<Point> getContour(double stepSize) const override;

  /**
   * @brief Test if point is on the polygon contour (boundary)
   * @param point Point to test in world coordinates
   * @param tolerance Distance tolerance in current units
   * @return true if point is within tolerance of any polygon edge
   *
   * Tests if a point lies on or near the polygon boundary by computing
   * the minimum distance from the point to any edge segment.
   *
   * @param point Point to test (in same coordinate system as polygon)
   * @param tolerance Maximum distance from boundary (must be positive)
   * @return true if distance to nearest edge <= tolerance
   *
   * @note O(n) complexity where n = number of vertices
   * @see isInside() - Interior containment test
   */
  bool isOnContour(const Point &point, double tolerance) const override;

  /**
   * @brief Calculate perimeter (sum of edge lengths)
   * @return Total length of polygon boundary
   *
   * Sums distances between consecutive vertices.
   *
   * @code{.cpp}
   * // Square 10x10
   * Polygon square({{0,0}, {10,0}, {10,10}, {0,10}});
   * assert(square.perimeter() == 40.0);  // 4 * 10
   *
   * // Triangle (3-4-5 right triangle)
   * Polygon triangle({{0,0}, {3,0}, {3,4}});
   * double perim = triangle.perimeter();
   * assert(std::abs(perim - 12.0) < 0.01);  // 3 + 4 + 5
   * @endcode
   *
   * @note O(n) complexity
   * @see area() - Calculate enclosed area
   */
  double perimeter() const override;

  /**
   * @brief Calculate area using shoelace formula
   * @return Absolute area in square units
   *
   * Uses Gauss's area formula (shoelace formula):
   * ```
   * A = 1/2 * |sum(x[i] * y[i+1] - x[i+1] * y[i])|
   * ```
   *
   * Works for both convex and concave polygons.
   * Returns absolute value (always positive).
   *
   * @code{.cpp}
   * // Square 10x10
   * Polygon square({{0,0}, {10,0}, {10,10}, {0,10}});
   * assert(square.area() == 100.0);
   *
   * // Triangle (base=10, height=8.66)
   * Polygon triangle({{0,0}, {10,0}, {5,8.66}});
   * double area = triangle.area();
   * assert(std::abs(area - 43.3) < 0.1);  // ~1/2 * 10 * 8.66
   *
   * // Winding doesn't affect area (absolute value)
   * Polygon ccw({{0,0}, {10,0}, {10,10}, {0,10}});
   * Polygon cw({{0,0}, {0,10}, {10,10}, {10,0}});
   * assert(ccw.area() == cw.area());
   * @endcode
   *
   * @note O(n) complexity
   * @note Returns absolute value (winding-independent)
   * @note Works for concave polygons
   * @see signedArea() - Get signed area (private)
   * @see perimeter() - Calculate boundary length
   */
  double area() const override;

  /**
   * @brief Create deep copy of polygon
   * @return Unique pointer to cloned polygon
   *
   * Creates independent copy including all vertices and state.
   *
   * @code{.cpp}
   * Polygon original({{0,0}, {10,0}, {5,8.66}});
   * original.setTypeLimits(TypeLimits::EXTERNAL);
   *
   * std::unique_ptr<Shape> clone = original.clone();
   *
   * // Clone is independent
   * clone->shiftX(100);  // Move clone
   * // original unchanged
   * @endcode
   *
   * @return Unique pointer to new Polygon instance
   * @see Shape::clone() - Base class interface
   */
  std::unique_ptr<Shape> clone() const override;

  /**
   * @brief Get type name for debugging/serialization
   * @return "Polygon"
   *
   * @code{.cpp}
   * Polygon poly({{0,0}, {10,0}, {5,8.66}});
   * assert(std::string(poly.typeName()) == "Polygon");
   * @endcode
   */
  const char *typeName() const override { return "Polygon"; }

  // Polygon-specific operations

  /**
   * @brief Add vertex to polygon
   * @param point Vertex to append to vertex list
   *
   * Adds vertex to end of vertex list. Polygon grows dynamically.
   *
   * @code{.cpp}
   * Polygon poly;
   * poly.addVertex({0, 0});
   * poly.addVertex({10, 0});
   * poly.addVertex({5, 8.66});
   *
   * assert(poly.vertexCount() == 3);
   * @endcode
   *
   * @note No automatic closure - first and last connect implicitly
   * @see clear() - Remove all vertices
   */
  void addVertex(const Point &point);

  /**
   * @brief Get number of vertices
   * @return Vertex count
   *
   * @code{.cpp}
   * Polygon triangle({{0,0}, {10,0}, {5,8.66}});
   * assert(triangle.vertexCount() == 3);
   * @endcode
   *
   * @see vertices() - Get all vertices
   */
  size_t vertexCount() const { return vertices_.size(); }

  /**
   * @brief Get vertex at index
   * @param index Vertex index (0 to vertexCount()-1)
   * @return Const reference to vertex
   *
   * @code{.cpp}
   * Polygon triangle({{0,0}, {10,0}, {5,8.66}});
   *
   * Point v0 = triangle.vertex(0);  // {0, 0}
   * Point v1 = triangle.vertex(1);  // {10, 0}
   * Point v2 = triangle.vertex(2);  // {5, 8.66}
   * @endcode
   *
   * @note No bounds checking in release builds (use with care)
   * @see vertices() - Get all vertices
   * @see vertexCount() - Get vertex count
   */
  const Point &vertex(size_t index) const { return vertices_[index]; }

  /**
   * @brief Get all vertices
   * @return Const reference to vertex vector
   *
   * @code{.cpp}
   * Polygon poly({{0,0}, {10,0}, {5,8.66}});
   *
   * const auto& verts = poly.vertices();
   * for (const auto& v : verts) {
   *     std::cout << "(" << v.x << ", " << v.y << ")\n";
   * }
   * @endcode
   *
   * @return Const reference to internal vertex vector
   * @note Direct access to vertices (no copy)
   * @see vertex(index) - Get single vertex
   * @see vertexCount() - Get count
   */
  const std::vector<Point> &vertices() const { return vertices_; }

  /**
   * @brief Clear all vertices
   *
   * Removes all vertices, creating empty (degenerate) polygon.
   *
   * @code{.cpp}
   * Polygon poly({{0,0}, {10,0}, {5,8.66}});
   * assert(poly.vertexCount() == 3);
   *
   * poly.clear();
   * assert(poly.vertexCount() == 0);
   * assert(poly.isDegenerate());
   * @endcode
   *
   * @see addVertex() - Add vertices
   */
  void clear() { vertices_.clear(); }

  /**
   * @brief Check if polygon is closed
   * @param tolerance Maximum distance for first == last
   * @return true if first and last vertices are within tolerance
   *
   * Checks if polygon has explicit closure (first vertex duplicated at end).
   *
   * @code{.cpp}
   * // Implicit closure (normal)
   * Polygon implicit({{0,0}, {10,0}, {5,8.66}});
   * assert(!implicit.isClosed());  // No duplicate
   *
   * // Explicit closure (redundant but allowed)
   * Polygon explicit({{0,0}, {10,0}, {5,8.66}, {0,0}});
   * assert(explicit.isClosed());  // First == last
   * @endcode
   *
   * @param tolerance Distance threshold for equality
   * @note Explicit closure is redundant (polygon closes implicitly)
   * @see ensureClosed() - Add closing vertex if needed
   */
  bool isClosed(double tolerance = 1e-6) const;

  /**
   * @brief Check if polygon is degenerate
   * @param tolerance Threshold for near-zero area
   * @return true if < 3 vertices or near-zero area
   *
   * A polygon is degenerate if it cannot enclose area:
   * - Fewer than 3 vertices
   * - Zero or near-zero area (collinear vertices)
   *
   * @code{.cpp}
   * // Empty polygon
   * Polygon empty;
   * assert(empty.isDegenerate());
   *
   * // Line (2 vertices)
   * Polygon line({{0,0}, {10,0}});
   * assert(line.isDegenerate());
   *
   * // Collinear (3+ vertices, zero area)
   * Polygon collinear({{0,0}, {5,0}, {10,0}});
   * assert(collinear.isDegenerate());
   *
   * // Valid triangle
   * Polygon triangle({{0,0}, {10,0}, {5,8.66}});
   * assert(!triangle.isDegenerate());
   * @endcode
   *
   * @param tolerance Area threshold for "near-zero"
   * @see area() - Calculate area
   */
  bool isDegenerate(double tolerance = 1e-6) const;

  /**
   * @brief Check if polygon is convex
   * @return true if all interior angles are less than 180°
   *
   * Uses cross product method:
   * - For each vertex triplet, compute cross product
   * - All cross products must have same sign
   *
   * @code{.cpp}
   * // Convex polygon (triangle)
   * Polygon convex({{0,0}, {10,0}, {5,8.66}});
   * assert(convex.isConvex());
   *
   * // Concave polygon (star)
   * Polygon concave({
   *     {5,0}, {6,3}, {10,3}, {7,5},
   *     {8,10}, {5,7}, {2,10}, {3,5},
   *     {0,3}, {4,3}
   * });
   * assert(!concave.isConvex());
   * @endcode
   *
   * @note O(n) complexity
   * @note Degenerate polygons return false
   * @see isDegenerate() - Check for valid polygon
   */
  bool isConvex() const;

  /**
   * @brief Calculate centroid (geometric center)
   * @return Center point of polygon
   *
   * Computes area-weighted centroid:
   * ```
   * cx = sum(x[i] + x[i+1]) * (x[i]*y[i+1] - x[i+1]*y[i]) / (6*A)
   * cy = sum(y[i] + y[i+1]) * (x[i]*y[i+1] - x[i+1]*y[i]) / (6*A)
   * ```
   *
   * @code{.cpp}
   * // Square 10x10 at origin
   * Polygon square({{0,0}, {10,0}, {10,10}, {0,10}});
   * Point center = square.centroid();
   * assert(center.isNear({5, 5}));
   *
   * // Triangle
   * Polygon triangle({{0,0}, {10,0}, {5,8.66}});
   * Point triCenter = triangle.centroid();
   * // Centroid is 1/3 from each side
   * @endcode
   *
   * @note Returns {0, 0} for degenerate polygons
   * @note O(n) complexity
   * @see area() - Used in calculation
   */
  Point centroid() const;

  /**
   * @brief Ensure polygon is explicitly closed
   * @param tolerance Distance threshold for closure check
   *
   * If first and last vertices are not equal (within tolerance),
   * adds a copy of first vertex to end.
   *
   * @code{.cpp}
   * Polygon poly({{0,0}, {10,0}, {5,8.66}});
   * assert(poly.vertexCount() == 3);
   *
   * poly.ensureClosed();
   * if (!poly.isClosed(0.001)) {
   *     // Closing vertex was added
   *     assert(poly.vertexCount() == 4);
   *     assert(poly.vertex(0) == poly.vertex(3));
   * }
   * @endcode
   *
   * @param tolerance Distance for first==last check
   * @note Usually not needed (implicit closure)
   * @see isClosed() - Check if already closed
   */
  void ensureClosed(double tolerance = 1e-6);

  // Coordinate transformation interface implementation

  /**
   * @brief Normalize all vertices to unit system
   * @param originX Origin X in measuring system
   * @param originY Origin Y in measuring system
   * @param radius Normalization scale
   *
   * Transforms each vertex: v_norm = (v_meas - origin) / radius
   *
   * @code{.cpp}
   * Polygon poly({{100,100}, {200,100}, {150,186.6}});
   * poly.normalize(150, 143.3, 50);
   *
   * // Vertices now in normalized coordinates
   * @endcode
   *
   * @see denormalize() - Inverse operation
   */
  void normalize(double originX, double originY, double radius) override;

  /**
   * @brief Denormalize all vertices to measuring system
   * @param originX Origin X in measuring system
   * @param originY Origin Y in measuring system
   * @param radius Normalization scale
   *
   * Transforms each vertex: v_meas = v_norm * radius + origin
   *
   * @code{.cpp}
   * Polygon poly({{-1,-0.866}, {1,-0.866}, {0,0.866}});
   * poly.denormalize(150, 143.3, 50);
   *
   * // Vertices now in measuring coordinates
   * @endcode
   *
   * @see normalize() - Forward operation
   */
  void denormalize(double originX, double originY, double radius) override;

  /**
   * @brief Invert Y coordinate of all vertices
   * @param centerY Y coordinate of inversion axis
   *
   * Mirrors polygon across horizontal line: y_new = centerY - y_old
   *
   * Reverses winding order (CCW ↔ CW).
   *
   * @code{.cpp}
   * Polygon poly({{0,0}, {10,0}, {5,8.66}});
   * poly.inverseY(400);
   *
   * // All Y coordinates inverted
   * // Winding order reversed
   * @endcode
   *
   * @see transformToSystem() - System conversion
   */
  void inverseY(double centerY) override;

  /**
   * @brief Shift all vertices in X direction
   * @param deltaX Amount to shift (positive=right)
   *
   * @code{.cpp}
   * Polygon poly({{0,0}, {10,0}, {5,8.66}});
   * poly.shiftX(50);
   *
   * // All vertices moved right by 50
   * @endcode
   */
  void shiftX(double deltaX) override;

  /**
   * @brief Shift all vertices in Y direction
   * @param deltaY Amount to shift (direction depends on coordinate system)
   *
   * @code{.cpp}
   * Polygon poly({{0,0}, {10,0}, {5,8.66}});
   * poly.shiftY(25);
   *
   * // All vertices moved by deltaY
   * // Direction: down in SCREEN, up in MATH
   * @endcode
   */
  void shiftY(double deltaY) override;

  // Handle interface (descriptor enumeration)

  /**
   * @brief Enumerate interactive handles for polygon editing
   * @param handles Output vector for handle descriptors
   *
   * Provides handles for:
   * - Move (at centroid)
   * - Rotate (offset from centroid)
   * - Vertex (one per vertex)
   *
   * All positions in shape-local coordinates.
   *
   * @note EdgeMidpoint handles deferred to v2
   * @see ApplyHandleDrag() - Apply drag operations
   */
  void EnumerateHandles(std::vector<HandleDesc> &handles) const override;

  /**
   * @brief Apply handle drag to mutate polygon geometry
   * @param handle Handle descriptor from EnumerateHandles
   * @param drag Drag context with delta in world coordinates
   *
   * Supported operations:
   * - Move: translate all vertices
   * - Rotate: rotate vertices around centroid
   * - Vertex: move individual vertex
   *
   * Pure geometry mutation; no side effects.
   *
   * @note Preview-only; command commits changes
   * @see EnumerateHandles() - Handle enumeration
   */
  void ApplyHandleDrag(const HandleDesc &handle,
                       const DragContext &drag) override;

private:
  std::vector<Point>
      vertices_;  ///< Polygon vertices (ordered, closed implicitly)

  /**
   * @brief Calculate signed area using shoelace formula
   * @return Signed area (positive=CCW, negative=CW)
   *
   * Returns raw shoelace formula result (half of sum):
   * ```
   * 2A = sum(x[i]*y[i+1] - x[i+1]*y[i])
   * ```
   *
   * Sign indicates winding order:
   * - Positive: Counter-clockwise
   * - Negative: Clockwise
   *
   * Used internally by area() (which returns abs value)
   * and by winding order calculations.
   */
  double signedArea() const;
};

}  // namespace aperture
