target_sources(DigitQt
    PRIVATE
        Measurement.h
        Measurement.cpp
        ShapeCollectionAccess.h
        AutoSeedPlacement.h
        AutoSeedPlacement.cpp
        NumberedFringeLine.h
        FringeOrdering.h
        FringeOrdering.cpp
        PhaseMap.h
)

add_subdirectory(commands)
add_subdirectory(pipeline)
