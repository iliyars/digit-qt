# Unit tests, built only when -DDIGITQT_BUILD_TESTS=ON. Compiles the handful
# of core/ + core/pipeline sources needed by ModalAnalysisStage directly into
# the test binary (there is no separate "digitqt_core" library target to
# link against -- see src/CMakeLists.txt), so those translation units get
# built twice (once for DigitQt.exe, once here). Acceptable for a project
# this size; revisit if the test list grows enough to justify a real
# core library target.

find_package(Qt5 REQUIRED COMPONENTS Test)

add_executable(digitqt_tests
    ModalAnalysisStageTest.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Measurement.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStageId.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/ModalAnalysisStage.cpp
)

target_include_directories(digitqt_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(digitqt_tests PRIVATE Qt5::Test Qt5::Widgets Aperture::Core Eigen3::Eigen)

set_target_properties(digitqt_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ModalAnalysisStageTest COMMAND digitqt_tests)

# Separate executable: each QTEST_MAIN(...) defines its own main(), so
# distinct test classes can't share one binary.
add_executable(digitqt_image_padding_tests
    ImagePaddingTest.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ImagePadding.cpp
)

target_include_directories(digitqt_image_padding_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(digitqt_image_padding_tests PRIVATE Qt5::Test Qt5::Widgets)

set_target_properties(digitqt_image_padding_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ImagePaddingTest COMMAND digitqt_image_padding_tests)

add_executable(digitqt_fringe_edge_extrapolation_tests
    FringeEdgeExtrapolationTest.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FringeEdgeExtrapolation.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FringeOrdering.cpp
)

target_include_directories(digitqt_fringe_edge_extrapolation_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(digitqt_fringe_edge_extrapolation_tests PRIVATE Qt5::Test Qt5::Widgets)

set_target_properties(digitqt_fringe_edge_extrapolation_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME FringeEdgeExtrapolationTest COMMAND digitqt_fringe_edge_extrapolation_tests)

# Integration test: renders a synthetic interferogram image with known
# Seregin-basis aberration coefficients (same formula as
# ModalAnalysisStageTest.cpp, at realistic nm magnitudes), then runs the
# REAL S1(Setup)->S2->S4->S5 pipeline on it (both Fourier and Horizontal
# Spline) and checks the recovered coefficients -- unlike
# ModalAnalysisStageTest (which feeds a wavefront map directly into S5
# only), this exercises fringe tracing, phase reconstruction and
# wavefront conversion too, needing most of core/pipeline/.
add_executable(digitqt_e2e_synthetic_tests
    EndToEndSyntheticInterferogramTest.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Measurement.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FringeOrdering.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/Pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStageId.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/NotYetImplementedStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/SetupStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/PhaseReconstructionStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/WavefrontReconstructionStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/ModalAnalysisStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/fringe_tracing/SequentialFringeTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/fringe_tracing/StructureTensorTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/fringe_tracing/ScanlineExtremumTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/fringe_tracing/BinaryThinningTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/fringe_tracing/scanline_extremum/MiddleAlgorithm.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/fringe_tracing/scanline_extremum/RedCenterDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/fringe_tracing/scanline_extremum/FringeConstructor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/phase_reconstruction/PhaseReconstructor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/phase_reconstruction/FourierPhaseExtractor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/phase_reconstruction/WaveletPhaseExtractor.cpp
)

target_include_directories(digitqt_e2e_synthetic_tests PRIVATE ${CMAKE_SOURCE_DIR}/src
                                                                ${OpenCV_INCLUDE_DIRS})

target_link_libraries(digitqt_e2e_synthetic_tests PRIVATE Qt5::Test Qt5::Widgets Aperture::Core
                                                           Eigen3::Eigen ${OpenCV_LIBS})

set_target_properties(digitqt_e2e_synthetic_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME EndToEndSyntheticInterferogramTest COMMAND digitqt_e2e_synthetic_tests)
