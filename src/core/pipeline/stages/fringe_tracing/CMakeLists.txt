target_sources(DigitQt
    PRIVATE
        IFringeTracer.h
        SequentialFringeTracker.h
        SequentialFringeTracker.cpp
        ScanlineExtremumTracker.h
        ScanlineExtremumTracker.cpp
)

add_subdirectory(scanline_extremum)
