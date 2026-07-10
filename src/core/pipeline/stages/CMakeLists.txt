target_sources(DigitQt
    PRIVATE
        SetupStage.h
        SetupStage.cpp
        PhaseReconstructionStage.h
        PhaseReconstructionStage.cpp
)

add_subdirectory(fringe_tracing)
add_subdirectory(phase_reconstruction)
