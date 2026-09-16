# Диагностический CLI-инструмент, не часть приложения -- собирается
# только при -DDIGITQT_BUILD_TOOLS=ON. Компилирует нужные core/io
# исходники прямо в этот исполняемый файл (тот же приём, что и в
# tests/CMakeLists.txt -- нет отдельной библиотеки "digitqt_core"),
# поэтому эти единицы трансляции собираются повторно; это осознанный
# компромисс, как и в тестах.

add_executable(mtr_diff
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Measurement.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ApertureSamples.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ModalAnalysisReportData.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStageId.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/ModalAnalysisStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/stages/WavefrontReconstructionStage.cpp
    ${CMAKE_SOURCE_DIR}/src/io/MtrImporter.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ModalReportExporter.cpp
)

target_include_directories(mtr_diff PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(mtr_diff PRIVATE Qt5::Core Qt5::Gui Aperture::Core Eigen3::Eigen)

set_target_properties(mtr_diff PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
