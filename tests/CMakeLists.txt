# Unit tests, built only when -DDIGITQT_BUILD_TESTS=ON. Each links
# DigitQt::Core (see src/core/CMakeLists.txt) instead of recompiling
# core/ sources directly -- until this project's Qt-decoupling work,
# there was no such library target, so every test binary duplicated the
# compilation of whatever core/ files it needed. That's fixed now.

find_package(Qt5 REQUIRED COMPONENTS Test)

add_executable(digitqt_tests
    ModalAnalysisStageTest.cpp
)

target_link_libraries(digitqt_tests PRIVATE Qt5::Test Qt5::Widgets DigitQt::Core)

set_target_properties(digitqt_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ModalAnalysisStageTest COMMAND digitqt_tests)

# Separate executable: each QTEST_MAIN(...) defines its own main(), so
# distinct test classes can't share one binary.
add_executable(digitqt_image_padding_tests
    ImagePaddingTest.cpp
)

target_link_libraries(digitqt_image_padding_tests PRIVATE Qt5::Test Qt5::Widgets DigitQt::Core)

set_target_properties(digitqt_image_padding_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME ImagePaddingTest COMMAND digitqt_image_padding_tests)

add_executable(digitqt_fringe_edge_extrapolation_tests
    FringeEdgeExtrapolationTest.cpp
)

target_link_libraries(digitqt_fringe_edge_extrapolation_tests PRIVATE Qt5::Test Qt5::Widgets
                                                                       DigitQt::Core)

set_target_properties(digitqt_fringe_edge_extrapolation_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME FringeEdgeExtrapolationTest COMMAND digitqt_fringe_edge_extrapolation_tests)

add_executable(digitqt_phase_reconstructor_tests
    PhaseReconstructorTest.cpp
)

target_link_libraries(digitqt_phase_reconstructor_tests PRIVATE Qt5::Test Qt5::Widgets
                                                                 DigitQt::Core)

set_target_properties(digitqt_phase_reconstructor_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    # Грузит реальный horizontal_spline.dll -- HorizontalSpline больше не
    # встроена в core (см. IPhaseReconstructor.h). Тот же приём, что у
    # digitqt_e2e_synthetic_tests ниже -- общий output dir с DigitQt.exe,
    # чтобы pluginsDirectory() нашёл тот же plugins/phase/.
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:DigitQt>"
)

add_test(NAME PhaseReconstructorTest COMMAND digitqt_phase_reconstructor_tests)

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
)

target_link_libraries(digitqt_e2e_synthetic_tests PRIVATE Qt5::Test Qt5::Widgets DigitQt::Core)

set_target_properties(digitqt_e2e_synthetic_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    # Эксплуатирует реальный ScanlineExtremum-трекер (S1), а он больше не
    # встроен в core -- только plugins/tracers/scanline_extremum_tracker.dll
    # (см. IFringeTracer.h). Ставим тот же output dir, что и у DigitQt.exe,
    # чтобы core::plugin_loading::pluginsDirectory() (GetModuleFileNameW
    # этого процесса) нашёл тот же plugins/tracers/, куда плагины
    # копируются POST_BUILD (см. plugin-sdk/*_tracker_plugin/CMakeLists.txt).
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:DigitQt>"
)

add_test(NAME EndToEndSyntheticInterferogramTest COMMAND digitqt_e2e_synthetic_tests)
