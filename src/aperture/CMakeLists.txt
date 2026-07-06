# ApertureCore — geometry & visibility core.
#
# Owns the shape model (Ellipse/Rectangle/Polygon), TypeLimits
# (EXTERNAL/INTERNAL/APERTURE) and ShapeCollection used to describe the
# external/internal boundaries of an interferogram.
#
# Deliberately independent of any UI framework (no Qt, no MFC) so it can be
# unit-tested and reused on its own — this is why it stays a separate
# library target instead of being folded into the DigitQt executable.

add_library(aperture STATIC
    geometry/Point.cpp
    geometry/Bounds.cpp
    geometry/Shape.cpp
    geometry/Ellipse.cpp
    geometry/Rectangle.cpp
    geometry/Polygon.cpp
    visibility/TypeLimits.cpp
    visibility/ShapeCollection.cpp
    visibility/VisibilityChecker.cpp
)
add_library(Aperture::Core ALIAS aperture)

target_include_directories(aperture
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(aperture PUBLIC cxx_std_17)

set_target_properties(aperture PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
