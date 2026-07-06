target_sources(DigitQt
    PRIVATE
        Measurement.h
        Measurement.cpp
        ShapeCollectionAccess.h
)

add_subdirectory(commands)
add_subdirectory(pipeline)
