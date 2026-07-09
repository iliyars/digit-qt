target_sources(DigitQt
    PRIVATE
        Measurement.h
        Measurement.cpp
        ShapeCollectionAccess.h
        AutoSeedPlacement.h
        AutoSeedPlacement.cpp
)

add_subdirectory(commands)
add_subdirectory(pipeline)
