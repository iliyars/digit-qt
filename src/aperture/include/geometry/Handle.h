/**
 * @file Handle.h
 * @brief Handle descriptor types for interactive shape editing
 *
 * Implements UX specification from Docs\shapes_handles.md
 *
 * ## Architecture (CRITICAL - NON-NEGOTIABLE)
 *
 * ❌ **WRONG**: Shapes store Handle objects as members
 * ✅ **CORRECT**: Shapes enumerate HandleDesc (descriptors) on demand
 *
 * ### Separation of Concerns:
 *
 * **Shapes (this file)**:
 * - Pure geometric + semantic objects
 * - Serializable (file load/save)
 * - UI-agnostic, testable without screen
 * - Enumerate HandleDesc via EnumerateHandles()
 * - Apply geometry changes via ApplyHandleDrag()
 *
 * **BoundsHandler (caller)**:
 * - Converts HandleDesc → RuntimeHandle (screen space)
 * - Hit-testing with pixel-radius tolerance
 * - Mouse interaction lifecycle
 * - Cursor logic
 * - Command creation and undo
 *
 * ### Mental Model (§12):
 * > **Shapes expose handles.**
 * > **BoundsHandler interprets interaction.**
 * > **Commands commit geometry.**
 *
 * @see shapes_handles.md for complete UX specification
 */
#pragma once

#include "Point.h"

#include <vector>

namespace aperture {

/**
 * @brief Handle type classification (UX spec §2-5)
 *
 * Defines semantic meaning of each handle type.
 * Each type implies specific interaction behavior and cursor.
 */
enum class HandleType {
  Move,          ///< Translate entire shape (§2.1)
  Rotate,        ///< Rotate around center (§2.2)
  CornerResize,  ///< Rectangle corner - resize in 2 axes (§3.1)
  EdgeResize,    ///< Rectangle edge midpoint - resize in 1 axis (§3.1)
  AxisResize,    ///< Ellipse axis endpoint - scale radius (§4.1)
  Vertex         ///< Polygon vertex - edit shape (§5.1)
};

/**
 * @brief Handle descriptor - pure data, NO UI logic
 *
 * Returned by Shape::EnumerateHandles() to describe each interactive control
 * point.
 *
 * **This is NOT a runtime handle object** - it's a lightweight descriptor.
 * BoundsHandler converts these to RuntimeHandle with screen positions, hover
 * state, etc.
 *
 * ### Coordinate Space:
 * - `localPos` is in **shape-local coordinates** (NOT world, NOT screen)
 * - BoundsHandler transforms: local → world → screen
 * - This keeps shape math stable across view transforms
 *
 * ### Immutability:
 * - Once drag begins, HandleDesc is frozen
 * - Even if geometry changes during preview
 * - This prevents index drift
 *
 * @warning Do NOT add methods like isMove(), isResize() - this is pure data
 */
struct HandleDesc {
  HandleType type = HandleType::Move;  ///< Semantic handle type
  int index = -1;  ///< Shape-specific index (-1 for Move/Rotate,
                   ///< vertex/corner/edge index otherwise)
  Point localPos;  ///< Position in shape-local coordinates
  Point normal;    ///< Resize direction (optional, zero if not applicable)

  HandleDesc() = default;

  HandleDesc(HandleType t, int idx, const Point &pos)
      : type(t), index(idx), localPos(pos), normal{0.0, 0.0} {}

  HandleDesc(HandleType t, int idx, const Point &pos, const Point &n)
      : type(t), index(idx), localPos(pos), normal(n) {}
};

/**
 * @brief Drag context for ApplyHandleDrag()
 *
 * Contains only information needed to compute new geometry.
 * NO UI state (cursor, mouse buttons, hover, etc.)
 *
 * ### Preview vs Commit:
 * - BoundsHandler calls ApplyHandleDrag() on a **temporary clone** during drag
 * - OR provides rollback mechanism on cancel
 * - ApplyHandleDrag() mutates geometry directly (pure transformation)
 * - No side effects, no commands, no snapping policy
 *
 * ### Coordinate Space:
 * - All positions are in **world coordinates**
 * - BoundsHandler handles screen→world transformation
 * - deltaWorld is explicitly world-space delta
 */
struct DragContext {
  HandleDesc handle;  ///< Which handle is being dragged (immutable during drag)
  Point dragStartWorld;    ///< World position where drag started
  Point dragCurrentWorld;  ///< Current world position during drag
  Point deltaWorld;  ///< World-space delta: dragCurrentWorld - dragStartWorld
  bool shiftKey =
      false;  ///< Shift modifier: constrain/snap (e.g. preserve aspect ratio)
  bool altKey = false;  ///< Alt modifier: symmetric resize (resize from center)
};

}  // namespace aperture
