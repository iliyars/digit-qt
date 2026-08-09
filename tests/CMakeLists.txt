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
