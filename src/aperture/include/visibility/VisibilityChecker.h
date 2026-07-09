/**
 * @file VisibilityChecker.h
 * @brief Point visibility testing engine with 3-type APERTURE support
 *
 * ## Overview
 *
 * VisibilityChecker implements the correct 3-type visibility algorithm for
 * optical systems with apertures, obstructions, and openings. It evaluates
 * whether points in 2D space are visible based on a collection of shapes
 * with different visibility behaviors.
 *
 * ## Key Features
 *
 * ### 1. Three Visibility Types
 *
 * - **EXTERNAL**: Main aperture boundary (intersection logic)
 * - **INTERNAL**: Obstructions/blockages (absolute blockers)
 * - **APERTURE**: Local openings (union logic, can override EXTERNAL)
 *
 * ### 2. Optimized Checking Order
 *
 * Algorithm processes types in order optimized for early exit:
 *
 * ```
 * 1. INTERNAL (early exit) - fastest rejection
 * 2. EXTERNAL (intersection) - base visibility
 * 3. APERTURE (union) - can override EXTERNAL
 * ```
 *
 * ### 3. Performance Optimization
 *
 * - Conservative ROI culling (skip 80-90% of pixels)
 * - Early exit on INTERNAL hit
 * - Statistics tracking for optimization
 * - Batch checking support
 *
 * ### 4. Real-World Applications
 *
 * - Telescope pupil visibility
 * - Lens aperture with obstructions
 * - Window/opening systems
 * - Image sensor dead pixel masks
 *
 * ## Visibility Algorithm
 *
 * **Step 1: INTERNAL Check (Early Exit)**
 * ```cpp
 * for (auto& shape : internal_shapes) {
 *     if (shape.isInside(point)) {
 *         return false;  // BLOCKED - no override possible
 *     }
 * }
 * ```
 *
 * **Step 2: Initialize Visibility**
 * ```cpp
 * bool visible = hasExternalShapes;
 * ```
 *
 * **Step 3: EXTERNAL Check (Intersection)**
 * ```cpp
 * for (auto& shape : external_shapes) {
 *     if (!shape.isInside(point)) {
 *         visible = false;  // Outside boundary (but continue - APERTURE can
 * override) break;
 *     }
 * }
 * ```
 *
 * **Step 4: APERTURE Check (Union, Override)**
 * ```cpp
 * for (auto& shape : aperture_shapes) {
 *     if (shape.isInside(point)) {
 *         visible = true;  // OPENED - overrides EXTERNAL rejection
 *         break;
 *     }
 * }
 *
 * return visible;
 * ```
 *
 * ## Usage Examples
 *
 * ### Basic Visibility Checking
 *
 * @code{.cpp}
 * #include <aperturecore/visibility/VisibilityChecker.h>
 * #include <aperturecore/geometry/Ellipse.h>
 *
 * using namespace aperture;
 *
 * // Create shape collection
 * ShapeCollection shapes;
 * shapes.addExternal(std::make_unique<Ellipse>(100, 100, 0, 0));  // Main
 * aperture shapes.addInternal(std::make_unique<Ellipse>(20, 20, 0, 0));    //
 * Central obstruction
 *
 * // Create checker
 * VisibilityChecker checker(shapes);
 *
 * // Test points
 * Point p1{0, 0};     // Inside INTERNAL ? false (blocked)
 * Point p2{50, 50};   // Inside EXTERNAL, outside INTERNAL ? true
 * Point p3{110, 0};   // Outside EXTERNAL ? false
 *
 * assert(!checker.isVisible(p1));  // Blocked by obstruction
 * assert(checker.isVisible(p2));   // Visible
 * assert(!checker.isVisible(p3));  // Outside aperture
 * @endcode
 *
 * ### Annular Aperture with Openings
 *
 * @code{.cpp}
 * ShapeCollection shapes;
 *
 * // Outer boundary
 * shapes.addExternal(std::make_unique<Ellipse>(100, 100, 0, 0));
 *
 * // Central obstruction (blocks most of center)
 * shapes.addInternal(std::make_unique<Ellipse>(40, 40, 0, 0));
 *
 * // Small opening through obstruction (APERTURE type)
 * shapes.addAperture(std::make_unique<Rectangle>(5, 30, 0, 0));
 *
 * VisibilityChecker checker(shapes);
 *
 * Point blocked{10, 10};   // Inside INTERNAL, outside APERTURE ? false
 * Point opened{2, 10};     // Inside APERTURE ? true (overrides INTERNAL
 * location!) Point outside{110, 0};   // Outside EXTERNAL ? false
 *
 * assert(!checker.isVisible(blocked));  // Blocked by obstruction
 * assert(checker.isVisible(opened));    // Visible through opening!
 * assert(!checker.isVisible(outside));  // Outside main aperture
 * @endcode
 *
 * ### Optimized Image Processing
 *
 * @code{.cpp}
 * VisibilityChecker checker(shapes);
 *
 * // Get ROI for optimization
 * Bounds roi = checker.getVisibleRegion();
 *
 * // Process only pixels within ROI (skips 80-90% typically)
 * int visiblePixels = 0;
 * for (int y = roi.bottom(); y <= roi.top(); ++y) {
 *     for (int x = roi.left(); x <= roi.right(); ++x) {
 *         if (checker.isVisible({static_cast<double>(x),
 *                               static_cast<double>(y)})) {
 *             // Process visible pixel
 *             visiblePixels++;
 *         }
 *     }
 * }
 *
 * // Check performance
 * auto stats = checker.getStats();
 * std::cout << "Checked " << stats.totalChecks << " points\n";
 * std::cout << "Early exits: " << stats.earlyExits << "\n";
 * @endcode
 *
 * ### Batch Processing
 *
 * @code{.cpp}
 * std::vector<Point> pointsToCheck;
 * for (int i = 0; i < 1000; ++i) {
 *     pointsToCheck.push_back({
 *         static_cast<double>(rand() % 200 - 100),
 *         static_cast<double>(rand() % 200 - 100)
 *     });
 * }
 *
 * // Batch check (more efficient for large sets)
 * std::vector<bool> results = checker.checkPoints(pointsToCheck);
 *
 * // Count visible
 * int visible = std::count(results.begin(), results.end(), true);
 * std::cout << visible << "/" << pointsToCheck.size() << " visible\n";
 * @endcode
 *
 * ## Performance Characteristics
 *
 * - **isVisible**: O(I + E + A) where I/E/A = shape counts by type
 * - **ROI culling**: Reduces checks by 80-90% typically
 * - **Early exit**: INTERNAL hit exits immediately
 * - **Batch checking**: Same per-point cost, better cache locality
 *
 * ## Comparison to isPupil
 *
 * VisibilityChecker replaces legacy isPupil() with:
 * - **3-type support**: APERTURE type added
 * - **Correct ordering**: INTERNAL checked first
 * - **Better performance**: ROI optimization, statistics
 * - **Modern C++**: Uses unique_ptr, move semantics
 * - **Type safety**: Strong typing with enums
 *
 * ### Migration Example
 *
 * **Before (isPupil):**
 * ```cpp
 * CArrGen<std::unique_ptr<XYShape>> apertures;
 * apertures.Add(std::make_unique<XYEllipse>(...));
 *
 * CArrGen<std::unique_ptr<XYShape>> obstructions;
 * obstructions.Add(std::make_unique<XYEllipse>(...));
 *
 * XYPoint point{50, 50};
 * bool visible = isPupil(point, apertures, obstructions);
 * ```
 *
 * **After (VisibilityChecker):**
 * ```cpp
 * ShapeCollection shapes;
 * shapes.addExternal(std::make_unique<Ellipse>(...));
 * shapes.addInternal(std::make_unique<Ellipse>(...));
 *
 * VisibilityChecker checker(shapes);
 * Point point{50, 50};
 * bool visible = checker.isVisible(point);
 * ```
 *
 * ## Thread Safety
 *
 * **Not thread-safe!** VisibilityChecker is not synchronized.
 * - Stats are mutable (modified in const methods)
 * - Create separate checker per thread OR
 * - Synchronize access with mutex
 *
 * @note Replaces isPupil() from InterfSolver
 * @see ShapeCollection - Shape container
 * @see TypeLimits - Visibility type enumeration
 * @see Shape - Base shape interface
 */
#pragma once

#include "../geometry/Point.h"
#include "ShapeCollection.h"

#include <vector>

namespace aperture {

/**
 * @class VisibilityChecker
 * @brief Point visibility testing engine
 *
 * Implements optimized 3-type visibility algorithm for optical systems.
 * Processes INTERNAL (blockers), EXTERNAL (apertures), and APERTURE (openings)
 * in order optimized for performance.
 *
 * ## Algorithm Summary
 *
 * 1. **INTERNAL check** - Early exit if blocked
 * 2. **EXTERNAL check** - Base visibility (intersection)
 * 3. **APERTURE check** - Override visibility (union)
 *
 * Returns true if:
 * - NOT inside any INTERNAL AND
 * - (Inside ALL EXTERNAL OR inside ANY APERTURE)
 *
 * ## Key Properties
 *
 * - **INTERNAL is absolute**: Nothing can override INTERNAL blocking
 * - **APERTURE can override EXTERNAL**: Creates local openings
 * - **Early exit**: INTERNAL hit returns immediately
 * - **Statistics tracking**: Performance monitoring built-in
 *
 * @see ShapeCollection - Manages shapes
 * @see isVisible() - Main visibility test
 * @see getVisibleRegion() - ROI optimization
 */
class VisibilityChecker {
public:
  /**
   * @brief Construct visibility checker
   * @param shapes Collection of shapes to check against
   *
   * Creates checker that references the shape collection.
   * The collection must remain valid for the lifetime of the checker.
   *
   * @code{.cpp}
   * ShapeCollection shapes;
   * shapes.addExternal(std::make_unique<Ellipse>(100, 100, 0, 0));
   *
   * VisibilityChecker checker(shapes);
   * // shapes must remain valid while checker exists
   * @endcode
   *
   * @warning shapes reference must remain valid
   */
  explicit VisibilityChecker(const ShapeCollection &shapes);

  /**
   * @brief Check if point is visible
   * @param point Point to test in world coordinates
   * @return true if point is visible according to visibility rules
   *
   * Applies 3-type visibility algorithm:
   * 1. Check INTERNAL (if inside any ? return false)
   * 2. Check EXTERNAL (must be inside all)
   * 3. Check APERTURE (if inside any ? force visible)
   *
   * @code{.cpp}
   * VisibilityChecker checker(shapes);
   *
   * bool v1 = checker.isVisible({0, 0});    // Test origin
   * bool v2 = checker.isVisible({50, 50});  // Test point
   * bool v3 = checker.isVisible({110, 0});  // Outside
   * @endcode
   *
   * @note O(I + E + A) complexity where I/E/A = shape counts
   * @note Early exit on INTERNAL hit
   * @see checkPoints() - Batch checking
   */
  bool isVisible(const Point &point) const;

  /**
   * @brief Batch check multiple points
   * @param points Vector of points to test
   * @return Vector of boolean results (same order as input)
   *
   * More efficient than individual checks for large point sets
   * due to better cache locality.
   *
   * @code{.cpp}
   * std::vector<Point> points = {
   *     {0, 0}, {50, 50}, {110, 0}
   * };
   *
   * std::vector<bool> results = checker.checkPoints(points);
   * // results[0] = visibility of {0, 0}
   * // results[1] = visibility of {50, 50}
   * // results[2] = visibility of {110, 0}
   * @endcode
   *
   * @note Same per-point complexity as isVisible()
   * @note Better cache performance for large batches
   */
  std::vector<bool> checkPoints(const std::vector<Point> &points) const;

  /**
   * @brief Performance statistics structure
   *
   * Tracks visibility checking performance for optimization.
   *
   * @code{.cpp}
   * auto stats = checker.getStats();
   * std::cout << "Total checks: " << stats.totalChecks << "\n";
   * std::cout << "Early exits: " << stats.earlyExits << "\n";
   * double hitRate = static_cast<double>(stats.earlyExits) / stats.totalChecks;
   * std::cout << "Early exit rate: " << (hitRate * 100) << "%\n";
   * @endcode
   */
  struct Stats {
    size_t totalChecks{0};     ///< Total isVisible() calls
    size_t externalChecks{0};  ///< EXTERNAL shape containment tests
    size_t apertureChecks{0};  ///< APERTURE shape containment tests
    size_t internalChecks{0};  ///< INTERNAL shape containment tests
    size_t earlyExits{0};      ///< Early exits (INTERNAL hits)
  };

  /**
   * @brief Get performance statistics
   * @return Current statistics snapshot
   *
   * Statistics are cumulative since last reset.
   *
   * @code{.cpp}
   * checker.resetStats();
   *
   * // Do visibility checks...
   * for (int i = 0; i < 1000; ++i) {
   *     checker.isVisible({i, i});
   * }
   *
   * auto stats = checker.getStats();
   * std::cout << "Checked " << stats.totalChecks << " points\n";
   * @endcode
   *
   * @see resetStats() - Clear counters
   */
  Stats getStats() const { return stats_; }

  /**
   * @brief Reset statistics counters
   *
   * Zeros all performance counters.
   *
   * @code{.cpp}
   * checker.resetStats();  // Start fresh
   * // ... do checks ...
   * auto stats = checker.getStats();
   * @endcode
   */
  void resetStats();

  /**
   * @brief Get conservative visible region bounds (ROI)
   * @return Conservative bounds where visible points MAY exist
   *
   * Returns Region of Interest (ROI) for optimization:
   * - Points OUTSIDE ROI: guaranteed invisible (safe to skip)
   * - Points INSIDE ROI: might be visible (must check with isVisible())
   *
   * ## ROI Calculation
   *
   * - With EXTERNAL shapes: intersection of EXTERNAL bounds
   * - Without EXTERNAL: union of APERTURE bounds
   * - No shapes: empty bounds
   *
   * ## Why Conservative?
   *
   * INTERNAL obstructions within EXTERNAL region are not accounted for
   * in the bounds. The ROI includes potentially blocked areas.
   *
   * ## Optimization Strategy
   *
   * 1. Get ROI (O(S) where S = shape count)
   * 2. Skip pixels outside ROI (typically 80-90%)
   * 3. Check remaining pixels with isVisible() (handles INTERNAL)
   *
   * @code{.cpp}
   * VisibilityChecker checker(shapes);
   * Bounds roi = checker.getVisibleRegion();
   *
   * int checked = 0, visible = 0;
   * for (int y = roi.bottom(); y <= roi.top(); ++y) {
   *     for (int x = roi.left(); x <= roi.right(); ++x) {
   *         checked++;
   *         if (checker.isVisible({x, y})) {
   *             visible++;
   *         }
   *     }
   * }
   *
   * // ROI optimization effectiveness
   * int totalPixels = imageWidth * imageHeight;
   * double skipRate = 1.0 - (static_cast<double>(checked) / totalPixels);
   * std::cout << "Skipped " << (skipRate * 100) << "% of pixels\n";
   * @endcode
   *
   * @note Conservative - includes potentially blocked areas
   * @note Points outside are GUARANTEED invisible
   * @note Points inside REQUIRE isVisible() check
   * @see ShapeCollection::getVisibleRegion()
   */
  Bounds getVisibleRegion() const;

private:
  const ShapeCollection
      &shapes_;  ///< Shape collection reference (must outlive checker)
  mutable Stats
      stats_;  ///< Performance counters (mutable for const isVisible())
};

}  // namespace aperture
