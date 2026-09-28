# DigitQtCore -- canonical data model + pipeline algorithms (Measurement,
# Bitmap, PhaseMap, fringe tracers, phase reconstruction, modal analysis).
#
# Deliberately independent of Qt (like aperture/, its own dependency) --
# enforced here for real by simply never linking Qt5 to this target,
# rather than only checked by the DIGITQT_PROBE_CORE_QT_FREE discovery
# build (see top-level CMakeLists.txt) that guided removing QImage/
# QString from this code in the first place.
#
# commands/ stays OUT of this library and continues to build straight
# into DigitQt: it wraps QUndoCommand for GUI undo/redo, which is an
# interactive-editing concern, not part of the canonical data/algorithm
# contract this library exists to keep Qt-free.
add_library(core STATIC
    ApertureSamples.h
    ApertureSamples.cpp
    Measurement.h
    Measurement.cpp
    ShapeCollectionAccess.h
    AutoSeedPlacement.h
    AutoSeedPlacement.cpp
    AutoApertureDetection.h
    AutoApertureDetection.cpp
    ImagePadding.h
    ImagePadding.cpp
    NumberedFringeLine.h
    Fiducial.h
    AffineTransform2D.h
    FiducialTransformFit.h
    FiducialTransformFit.cpp
    FringeOrdering.h
    FringeOrdering.cpp
    FringeEdgeExtrapolation.h
    FringeEdgeExtrapolation.cpp
    PhaseMap.h
    ModalAnalysisResult.h
    ModalAnalysisReportData.h
    ModalAnalysisReportData.cpp
    ModalFitMethod.h
    PolynomialBasis.h
    SequentialSereginResult.h
    Bitmap.h
)
add_library(DigitQt::Core ALIAS core)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
)

target_link_libraries(core PUBLIC Aperture::Core Eigen3::Eigen ${OpenCV_LIBS})
target_include_directories(core PRIVATE ${OpenCV_INCLUDE_DIRS})

target_compile_features(core PUBLIC cxx_std_17)

set_target_properties(core PROPERTIES
    AUTOMOC OFF
    AUTORCC OFF
    AUTOUIC OFF
)

add_subdirectory(commands)
add_subdirectory(pipeline)
