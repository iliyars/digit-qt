target_sources(DigitQt
    PRIVATE
        MainWindow.h
        MainWindow.cpp
        IconFactory.h
        IconFactory.cpp
        PipelineTreeDock.h
        PipelineTreeDock.cpp
        ParametersDock.h
        ParametersDock.cpp
        NotImplementedPage.h
        NotImplementedPage.cpp
        FiducialReferenceDialog.h
        FiducialReferenceDialog.cpp
)

add_subdirectory(canvas)
