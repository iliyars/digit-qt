/**
 * @file TypeLimits.h
 * @brief Shape visibility type enumeration
 */
#pragma once

#include <cstdint>

 // Protect against legacy macro pollution from MFC/Windows headers
#ifdef EXTERNAL
#undef EXTERNAL
#endif
#ifdef INTERNAL
#undef INTERNAL
#endif

namespace aperture {

/**
 * @brief Shape visibility behavior types
 * 
 * Defines how a shape affects visibility of points in space.
 * 
 * Three fundamental types:
 * - EXTERNAL: Aperture - defines base visible area (inside = visible)
 * - INTERNAL: Obstruction - blocks visibility (inside = blocked)
 * - APERTURE: Opening - creates visibility without blocking outside
 * 
 * Visibility algorithm applies in order: EXTERNAL ? APERTURE ? INTERNAL
 */
enum class TypeLimits : uint8_t {
    /**
     * @brief External aperture (classic behavior)
     * 
     * Points INSIDE are potentially visible.
     * Points OUTSIDE are blocked.
     * 
     * Multiple EXTERNAL shapes: visible = intersection of all
     * 
     * Use case: Main aperture/lens opening
     */
    EXTERNAL = 0,
    
    /**
     * @brief Internal obstruction (classic behavior)  
     * 
     * Points INSIDE are blocked.
     * Points OUTSIDE are unaffected.
     * 
     * Use case: Central obstruction, hole, spider vanes
     */
    INTERNAL = 1,
    
    /**
     * @brief Aperture opening (new behavior)
     * 
     * Points INSIDE become visible.
     * Points OUTSIDE are unaffected.
     * 
     * Differs from EXTERNAL: doesn't block outside points.
     * Creates "local openings" in otherwise invisible areas.
     * 
     * Use case: Secondary openings, viewing windows, sub-apertures
     */
    APERTURE = 2
};

/**
 * @brief Convert TypeLimits to string
 */
constexpr const char* toString(TypeLimits type) {
    switch (type) {
        case TypeLimits::EXTERNAL: return "EXTERNAL";
        case TypeLimits::INTERNAL: return "INTERNAL";
        case TypeLimits::APERTURE: return "APERTURE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Parse TypeLimits from string
 * @throws std::invalid_argument if string doesn't match any type
 */
TypeLimits fromString(const char* str);

/**
 * @brief Convert to legacy integer value for backwards compatibility
 */
constexpr int toLegacyInt(TypeLimits type) {
    return static_cast<int>(type);
}

/**
 * @brief Convert from legacy integer value
 * @param value Legacy type value (0=EXTERNAL, 1=INTERNAL)
 * @return TypeLimits, defaults to EXTERNAL for unknown values
 */
constexpr TypeLimits fromLegacyInt(int value) {
    switch (value) {
        case 0: return TypeLimits::EXTERNAL;
        case 1: return TypeLimits::INTERNAL;
        case 2: return TypeLimits::APERTURE;
        default: return TypeLimits::EXTERNAL;  // Safe default
    }
}

/**
 * @brief Check if type allows visibility inside
 */
constexpr bool allowsInside(TypeLimits type) {
    return type == TypeLimits::EXTERNAL || type == TypeLimits::APERTURE;
}

/**
 * @brief Check if type blocks visibility inside
 */
constexpr bool blocksInside(TypeLimits type) {
    return type == TypeLimits::INTERNAL;
}

/**
 * @brief Check if type affects outside visibility
 */
constexpr bool affectsOutside(TypeLimits type) {
    return type == TypeLimits::EXTERNAL;  // Only EXTERNAL blocks outside
}

} // namespace aperture
