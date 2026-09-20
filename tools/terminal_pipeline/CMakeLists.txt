# Диагностический CLI-инструмент (см. main.cpp) -- собирается только при
# -DDIGITQT_BUILD_TOOLS=ON. Компилирует нужные core/io исходники прямо в
# этот исполняемый файл, как и mtr_diff (см. tools/mtr_diff/CMakeLists.txt) --
# тот же осознанный компромисс с повторной компиляцией.

add_executable(terminal_pipeline
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Measurement.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FringeOrdering.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FringeEdgeExtrapolation.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/Pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/PipelineStageId.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline/NotYetImplementedStage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ApertureSamples.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ModalAnalysisReportData.cpp
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
    ${CMAKE_SOURCE_DIR}/src/io/ImageLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/io/MtrExporter.cpp
)

target_include_directories(terminal_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src ${OpenCV_INCLUDE_DIRS})

target_link_libraries(terminal_pipeline PRIVATE Qt5::Core Qt5::Gui Aperture::Core ${OpenCV_LIBS}
                                                Eigen3::Eigen)

set_target_properties(terminal_pipeline PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
