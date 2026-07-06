#pragma once
#include "VisibilityMask.h"
#include "ShapeCollection.h"
#include "VisibilityChecker.h"

namespace aperture {
namespace visibility {

/**
 * @brief Stateless builder for VisibilityMask
 * 
 * Heavy CPU operation that iterates all pixels and queries
 * VisibilityChecker to build a fresh mask.
 * 
 * Constraints:
 * - Stateless (all static)
 * - No caching
 * - No ownership of shapes
 * - No global state
 * - This is the ONLY place with per-pixel loops
 */
class VisibilityMaskBuilder {
public:
    /**
     * @brief Build a fresh visibility mask from shapes
     * @param shapes Shape collection to query
     * @param width Bitmap width in pixels
     * @param height Bitmap height in pixels
     * @return New VisibilityMask populated with visibility data
     * 
     * For each pixel (x,y):
     *   - call VisibilityChecker::IsVisible(x,y)
     *   - store result in mask
     */
    static VisibilityMask Build(
        const ShapeCollection& shapes,
        int width,
        int height)
    {
        VisibilityMask mask(width, height);
        
        // Store version stamps
        mask.shapeVersion = shapes.getVersion();
        mask.imageVersion = 0;  // Image version provided by caller later

        VisibilityChecker checker(shapes);  // Create checker (references shapes)

        // Iterate all pixels and query visibility
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = y * width + x;
                mask.data[index] = checker.isVisible({ static_cast<double>(x), static_cast<double>(y) }) ? 1 : 0;
            }
        }
        
        return mask;
    }
    
    /**
     * @brief Build with explicit image version stamp
     * @param shapes Shape collection
     * @param width Bitmap width
     * @param height Bitmap height
     * @param imgVersion Image version for cache validation
     * @return New VisibilityMask with all version stamps
     */
    static VisibilityMask Build(
        const ShapeCollection& shapes,
        int width,
        int height,
        uint64_t imgVersion)
    {
        VisibilityMask mask = Build(shapes, width, height);
        mask.imageVersion = imgVersion;
        return mask;
    }
};

} // namespace visibility
} // namespace aperture
