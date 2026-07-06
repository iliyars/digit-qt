target_sources(DigitQt
    PRIVATE
        PipelineStageId.h
        PipelineStageId.cpp
        PipelineStage.h
        PipelineStage.cpp
        Pipeline.h
        Pipeline.cpp
        NotYetImplementedStage.h
        NotYetImplementedStage.cpp
)

add_subdirectory(stages)
