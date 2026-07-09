target_sources(DigitQt
    PRIVATE
        IFringeTracer.h
        SequentialFringeTracker.h
        SequentialFringeTracker.cpp
        StructureTensorTracker.h
        StructureTensorTracker.cpp
        ScanlineExtremumTracker.h
        ScanlineExtremumTracker.cpp
        BinaryThinningTracker.h
        BinaryThinningTracker.cpp
)

add_subdirectory(scanline_extremum)
