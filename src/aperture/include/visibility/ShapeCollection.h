/**
 * @file ShapeCollection.h
 * @brief Container for shapes organized by TypeLimits
 */
#pragma once

#include "../geometry/Shape.h"
#include "../geometry/Bounds.h"
#include "TypeLimits.h"
#include <vector>
#include <memory>

namespace aperture {

/**
 * @brief Collection of shapes organized by visibility type
 * 
 * Stores shapes in separate containers based on TypeLimits for efficient
 * visibility checking. Provides queries for shape management and bounds.
 */
class ShapeCollection {
public:
    ShapeCollection() : version_(1) {}; // Start at 1, 0 means uninitialized
    ~ShapeCollection() = default;
    
    // Non-copyable (shapes are unique_ptr)
    ShapeCollection(const ShapeCollection&) = delete;
    ShapeCollection& operator=(const ShapeCollection&) = delete;
    
    // Movable
    ShapeCollection(ShapeCollection&&) = default;
    ShapeCollection& operator=(ShapeCollection&&) = default;
    
    // Add shapes
    
    /**
     * @brief Add shape to collection (auto-categorized by shape's TypeLimits)
     * @param shape Shape to add (ownership transferred)
     * 
     * The shape is categorized based on its getTypeLimits() value.
     * This is the preferred method when shapes are constructed with
     * their TypeLimits already set in the constructor.
     */
    void addShape(std::unique_ptr<Shape> shape);
    
    /**
     * @brief Add EXTERNAL shape (aperture)
     * @param shape Shape to add (TypeLimits set to EXTERNAL)
     * 
     * Convenience method that sets the shape's TypeLimits before adding.
     */
    void addExternal(std::unique_ptr<Shape> shape);
    
    /**
     * @brief Add INTERNAL shape (obstruction)
     * @param shape Shape to add (TypeLimits set to INTERNAL)
     * 
     * Convenience method that sets the shape's TypeLimits before adding.
     */
    void addInternal(std::unique_ptr<Shape> shape);
    
    /**
     * @brief Add APERTURE shape (opening)
     * @param shape Shape to add (TypeLimits set to APERTURE)
     * 
     * Convenience method that sets the shape's TypeLimits before adding.
     */
    void addAperture(std::unique_ptr<Shape> shape);

    /**
     * @brief Get current collection version
     * @return Version number (increments on any modification)
     */
    uint64_t getVersion() const {
        return version_;
    }

    /**
     * @brief Notify that shape geometry was modified
     *
     * Call this after modifying a shape's geometry
     * to increment version and invalidate caches.
     */
    void notifyShapeModified() {
        version_++;
    }

    // Query shapes by type
    
    /**
     * @brief Get all EXTERNAL shapes
     * @return Const reference to EXTERNAL shapes container
     */
    const std::vector<std::unique_ptr<Shape>>& getExternal() const {
        return external_;
    }
    
    /**
     * @brief Get all INTERNAL shapes
     * @return Const reference to INTERNAL shapes container
     */
    const std::vector<std::unique_ptr<Shape>>& getInternal() const {
        return internal_;
    }
    
    /**
     * @brief Get all APERTURE shapes
     * @return Const reference to APERTURE shapes container
     */
    const std::vector<std::unique_ptr<Shape>>& getApertures() const {
        return apertures_;
    }
    
    // Collection queries
    
    /**
     * @brief Check if collection has any EXTERNAL shapes
     * @return true if at least one EXTERNAL shape exists
     */
    bool hasAnyExternal() const {
        return !external_.empty();
    }
    
    /**
     * @brief Get total number of shapes
     * @return Sum of shapes in all categories
     */
    size_t totalCount() const {
        return external_.size() + internal_.size() + apertures_.size();
    }
    
    /**
     * @brief Check if collection is empty
     * @return true if no shapes in any category
     */
    bool isEmpty() const {
        return totalCount() == 0;
    }
    
    /**
     * @brief Get combined bounds of all shapes
     * @return Bounds containing all shapes, or empty bounds if no shapes
     */
    Bounds getCombinedBounds() const;
    
    /**
     * @brief Get visible region bounds (conservative ROI for optimization)
     * @return Conservative bounds where visible points MAY exist
     * 
     * Computes a conservative Region of Interest (ROI) for visibility checking:
     * - If EXTERNAL shapes exist: intersection of all EXTERNAL bounds
     * - If only APERTURE shapes: union of all APERTURE bounds
     * - If no visibility-defining shapes: empty bounds
     * 
     * **Important:** This returns a CONSERVATIVE bounding box. Due to INTERNAL
     * obstructions within the EXTERNAL region, not all points inside the returned
     * bounds are necessarily visible. You must still call isVisible() for each point.
     * 
     * The ROI guarantees:
     * - All points OUTSIDE the ROI are invisible (safe to skip)
     * - All points INSIDE the ROI *might* be visible (must check with isVisible())
     * 
     * This is still very useful for optimization:
     * - Skip pixels clearly outside EXTERNAL bounds (often 80-90% of pixels)
     * - Then check remaining pixels with isVisible() (handles INTERNAL)
     * 
     * Example:
     * - EXTERNAL: Ellipse(100, 100, 0, 0) ? bounds [-100, -100, 100, 100]
     * - INTERNAL: Ellipse(50, 100, 50, 0) ? blocks right half
     * - ROI: [-100, -100, 100, 100] (conservative - includes blocked area)
     * - You must still call isVisible() for points in ROI to check INTERNAL
     * 
     * This ROI is used for:
     * - First-pass culling (skip pixels outside ROI - major speedup!)
     * - Coordinate normalization (scale to EXTERNAL bounds)
     * - Memory allocation (allocate for worst-case visible area)
     * 
     * @code{.cpp}
     * ShapeCollection shapes;
     * shapes.addExternal(std::make_unique<Ellipse>(100, 100, 50, 50));
     * shapes.addInternal(std::make_unique<Ellipse>(30, 50, 70, 50)); // Obstruction
     * 
     * Bounds roi = shapes.getVisibleRegion();
     * // roi is EXTERNAL bounds (conservative)
     * 
     * // Two-stage checking for optimization
     * for (int y = roi.top; y <= roi.bottom; ++y) {
     *     for (int x = roi.left; x <= roi.right; ++x) {
     *         Point p{x, y};
     *         // Only check points inside ROI (skips most pixels)
     *         if (checker.isVisible(p)) {
     *             // Process visible pixel (handles INTERNAL obstructions)
     *         }
     *     }
     * }
     * @endcode
     * 
     * @note Returns conservative bounds - use isVisible() for exact visibility
     * @note INTERNAL shapes require per-point checking even within ROI
     * @see getCombinedBounds() for bounds of all shapes regardless of type
     */
    Bounds getVisibleRegion() const;
    
    /**
     * @brief Clear all shapes
     */
    void clear();
    
    /**
     * @brief Get count of shapes by type
     * @param type TypeLimits to count
     * @return Number of shapes with specified type
     */
    size_t countByType(TypeLimits type) const;

private:
    std::vector<std::unique_ptr<Shape>> external_;   ///< EXTERNAL shapes
    std::vector<std::unique_ptr<Shape>> internal_;   ///< INTERNAL shapes
    std::vector<std::unique_ptr<Shape>> apertures_;  ///< APERTURE shapes
    uint64_t version_{ 0 };  ///< Version counter for change tracking
};

} // namespace aperture
