target_sources(DigitQt
    PRIVATE
        SetupStage.h
        SetupStage.cpp
        PhaseReconstructionStage.h
        PhaseReconstructionStage.cpp
        WavefrontReconstructionStage.h
        WavefrontReconstructionStage.cpp
        ModalAnalysisStage.h
        ModalAnalysisStage.cpp
        ModalAnalysisStage.h
        ModalAnalysisStage.cpp
)

add_subdirectory(fringe_tracing)
add_subdirectory(phase_reconstruction)
